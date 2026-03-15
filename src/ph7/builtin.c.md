# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2915/3328 lines (87.59%)

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
|     22 |   98 | `static int PH7_builtin_is_null(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   99 |  |
|     23 |  100 | `	int res = 0; /* Assume false by default */` |
|     23 |  101 | `	if( nArg > 0 ){` |
|     21 |  102 | `		res = ph7_value_is_null(apArg[0]);` |
|     10 |  103 | `	}` |
|      - |  104 | `	/* Query result */` |
|     23 |  105 | `	ph7_result_bool(pCtx,res);` |
|     23 |  106 | `	return PH7_OK;` |
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
|    122 |  152 | `static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  153 |  |
|    124 |  154 | `	int res = 0; /* Assume false by default */` |
|    124 |  155 | `	if( nArg > 0 ){` |
|    122 |  156 | `		res = ph7_value_is_array(apArg[0]);` |
|     60 |  157 | `	}` |
|      - |  158 | `	/* Query result */` |
|    124 |  159 | `	ph7_result_bool(pCtx,res);` |
|    124 |  160 | `	return PH7_OK;` |
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
|  18770 |  269 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  270 |  |
|  18772 |  271 | `	int res = 1; /* Assume empty by default */` |
|  18772 |  272 | `	if( nArg > 0 ){` |
|  18770 |  273 | `		res = ph7_value_is_empty(apArg[0]);` |
|   9384 |  274 | `	}` |
|  18772 |  275 | `	ph7_result_bool(pCtx,res);` |
|  18772 |  276 | `	return PH7_OK;` |
|      - |  277 |  |
|      2 |  278 |  |
|      - |  279 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |  280 | `#define PH7_NEED_BUILTIN_REG 1` |
|      - |  281 | `#endif` |
|      - |  282 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - |  283 | `#define PH7_NEED_FMT_AND_INI 1` |
|      - |  284 | `#endif` |
|      - |  285 |  |
|      - |  286 | `/* Math functions moved to builtin_math.c */` |
|      - |  287 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - |  288 | `/*` |
|      - |  289 | ` * Section:` |
|      - |  290 | ` *    String handling Functions.` |
|      - |  291 | ` * Status:` |
|      - |  292 | ` *    Stable.` |
|      - |  293 | ` */` |
|      - |  294 | `/*` |
|      - |  295 | ` * string substr(string $string,int $start[, int $length ])` |
|      - |  296 | ` *  Return part of a string.` |
|      - |  297 | ` * Parameters` |
|      - |  298 | ` *  $string` |
|      - |  299 | ` *   The input string. Must be one character or longer.` |
|      - |  300 | ` * $start` |
|      - |  301 | ` *   If start is non-negative, the returned string will start at the start'th position` |
|      - |  302 | ` *   in string, counting from zero. For instance, in the string 'abcdef', the character` |
|      - |  303 | ` *   at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - |  304 | ` *   If start is negative, the returned string will start at the start'th character` |
|      - |  305 | ` *   from the end of string.` |
|      - |  306 | ` *   If string is less than or equal to start characters long, FALSE will be returned.` |
|      - |  307 | ` * $length` |
|      - |  308 | ` *   If length is given and is positive, the string returned will contain at most length` |
|      - |  309 | ` *   characters beginning from start (depending on the length of string).` |
|      - |  310 | ` *   If length is given and is negative, then that many characters will be omitted from` |
|      - |  311 | ` *   the end of string (after the start position has been calculated when a start is negative).` |
|      - |  312 | ` *   If start denotes the position of this truncation or beyond, false will be returned.` |
|      - |  313 | ` *   If length is given and is 0, FALSE or NULL an empty string will be returned.` |
|      - |  314 | ` *   If length is omitted, the substring starting from start until the end of the string` |
|      - |  315 | ` *   will be returned.` |
|      - |  316 | ` * Return` |
|      - |  317 | ` *  Returns the extracted part of string, or FALSE on failure or an empty string.` |
|      - |  318 | ` */` |
| 131536 |  319 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  320 |  |
|      - |  321 | `	const char *zSource,*zOfft;` |
|      - |  322 | `	int nOfft,nLen,nSrcLen;` |
| 131538 |  323 | `	if( nArg < 2 ){` |
|      - |  324 | `		/* return FALSE */` |
|      5 |  325 | `		ph7_result_bool(pCtx,0);` |
|      5 |  326 | `		return PH7_OK;` |
|      - |  327 | `	}` |
|      - |  328 | `	/* Extract the target string */` |
| 131534 |  329 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
| 131534 |  330 | `	if( nSrcLen < 1 ){` |
|      - |  331 | `		/* Empty string,return FALSE */` |
|   8056 |  332 | `		ph7_result_bool(pCtx,0);` |
|   8056 |  333 | `		return PH7_OK;` |
|      - |  334 | `	}` |
| 123480 |  335 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  336 | `	/* Extract the offset */` |
| 123480 |  337 | `	nOfft = ph7_value_to_int(apArg[1]);` |
| 123480 |  338 | `	if( nOfft < 0 ){` |
|  21178 |  339 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|  21178 |  340 | `		if( zOfft < zSource ){` |
|      - |  341 | `			/* Invalid offset */` |
|      5 |  342 | `			ph7_result_bool(pCtx,0);` |
|      5 |  343 | `			return PH7_OK;` |
|      - |  344 | `		}` |
|  21174 |  345 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|  21174 |  346 | `		nOfft = (int)(zOfft-zSource);` |
| 112890 |  347 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  348 | `		/* Invalid offset */` |
|      7 |  349 | `		ph7_result_bool(pCtx,0);` |
|      7 |  350 | `		return PH7_OK;` |
|    ! 0 |  351 | `	}else{` |
| 102298 |  352 | `		zOfft = &zSource[nOfft];` |
| 102298 |  353 | `		nLen = nSrcLen - nOfft;` |
|      - |  354 | `	}` |
| 123470 |  355 | `	if( nArg > 2 ){` |
|      - |  356 | `		/* Extract the length */` |
| 102296 |  357 | `		nLen = ph7_value_to_int(apArg[2]);` |
| 102296 |  358 | `		if( nLen == 0 ){` |
|      - |  359 | `			/* Invalid length,return an empty string */` |
|      5 |  360 | `			ph7_result_string(pCtx,"",0);` |
|      5 |  361 | `			return PH7_OK;` |
| 102292 |  362 | `		}else if( nLen < 0 ){` |
|  21176 |  363 | `			nLen = nSrcLen + nLen - nOfft;` |
|  21176 |  364 | `			if( nLen < 1 ){` |
|      - |  365 | `				/* Invalid  length */` |
|      3 |  366 | `				nLen = nSrcLen - nOfft;` |
|      1 |  367 | `			}` |
|  10587 |  368 | `		}` |
| 102292 |  369 | `		if( nLen + nOfft > nSrcLen ){` |
|      - |  370 | `			/* Invalid length */` |
|   2492 |  371 | `			nLen = nSrcLen - nOfft;` |
|   1245 |  372 | `		}` |
|  51145 |  373 | `	}` |
|      - |  374 | `	/* Return the substring */` |
| 123466 |  375 | `	ph7_result_string(pCtx,zOfft,nLen);` |
| 123466 |  376 | `	return PH7_OK;` |
|  65770 |  377 |  |
|      - |  378 | `/*` |
|      - |  379 | ` * int substr_compare(string $main_str,string $str ,int $offset[,int $length[,bool $case_insensitivity = false ]])` |
|      - |  380 | ` *  Binary safe comparison of two strings from an offset, up to length characters.` |
|      - |  381 | ` * Parameters` |
|      - |  382 | ` *  $main_str` |
|      - |  383 | ` *  The main string being compared.` |
|      - |  384 | ` *  $str` |
|      - |  385 | ` *   The secondary string being compared.` |
|      - |  386 | ` * $offset` |
|      - |  387 | ` *  The start position for the comparison. If negative, it starts counting from` |
|      - |  388 | ` *  the end of the string.` |
|      - |  389 | ` * $length` |
|      - |  390 | ` *  The length of the comparison. The default value is the largest of the length` |
|      - |  391 | ` *  of the str compared to the length of main_str less the offset.` |
|      - |  392 | ` * $case_insensitivity` |
|      - |  393 | ` *  If case_insensitivity is TRUE, comparison is case insensitive.` |
|      - |  394 | ` * Return` |
|      - |  395 | ` *  Returns < 0 if main_str from position offset is less than str, > 0 if it is greater than` |
|      - |  396 | ` *  str, and 0 if they are equal. If offset is equal to or greater than the length of main_str` |
|      - |  397 | ` *  or length is set and is less than 1, substr_compare() prints a warning and returns FALSE.` |
|      - |  398 | ` */` |
|     26 |  399 | `static int PH7_builtin_substr_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  400 |  |
|      - |  401 | `	const char *zSource,*zOfft,*zSub;` |
|      - |  402 | `	int nOfft,nLen,nSrcLen,nSublen;` |
|     27 |  403 | `	int iCase = 0;` |
|      - |  404 | `	int rc;` |
|     27 |  405 | `	if( nArg < 3 ){` |
|      - |  406 | `		/* Missing arguments,return FALSE */` |
|      5 |  407 | `		ph7_result_bool(pCtx,0);` |
|      5 |  408 | `		return PH7_OK;` |
|      - |  409 | `	}` |
|      - |  410 | `	/* Extract the target string */` |
|     23 |  411 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|     23 |  412 | `	if( nSrcLen < 1 ){` |
|      - |  413 | `		/* Empty string,return FALSE */` |
|      3 |  414 | `		ph7_result_bool(pCtx,0);` |
|      3 |  415 | `		return PH7_OK;` |
|      - |  416 | `	}` |
|     21 |  417 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  418 | `	/* Extract the substring */` |
|     21 |  419 | `	zSub = ph7_value_to_string(apArg[1],&nSublen);` |
|     21 |  420 | `	if( nSublen < 1 \|\| nSublen > nSrcLen){` |
|      - |  421 | `		/* Empty string,return FALSE */` |
|      3 |  422 | `		ph7_result_bool(pCtx,0);` |
|      3 |  423 | `		return PH7_OK;` |
|      - |  424 | `	}` |
|      - |  425 | `	/* Extract the offset */` |
|     19 |  426 | `	nOfft = ph7_value_to_int(apArg[2]);` |
|     19 |  427 | `	if( nOfft < 0 ){` |
|      5 |  428 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|      5 |  429 | `		if( zOfft < zSource ){` |
|      - |  430 | `			/* Invalid offset */` |
|      3 |  431 | `			ph7_result_bool(pCtx,0);` |
|      3 |  432 | `			return PH7_OK;` |
|      - |  433 | `		}` |
|      3 |  434 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|      3 |  435 | `		nOfft = (int)(zOfft-zSource);` |
|     16 |  436 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  437 | `		/* Invalid offset */` |
|      3 |  438 | `		ph7_result_bool(pCtx,0);` |
|      3 |  439 | `		return PH7_OK;` |
|    ! 0 |  440 | `	}else{` |
|     13 |  441 | `		zOfft = &zSource[nOfft];` |
|     13 |  442 | `		nLen = nSrcLen - nOfft;` |
|      - |  443 | `	}` |
|     15 |  444 | `	if( nArg > 3 ){` |
|      - |  445 | `		/* Extract the length */` |
|     13 |  446 | `		nLen = ph7_value_to_int(apArg[3]);` |
|     13 |  447 | `		if( nLen < 1 ){` |
|      - |  448 | `			/* Invalid  length */` |
|      5 |  449 | `			ph7_result_int(pCtx,1);` |
|      5 |  450 | `			return PH7_OK;` |
|      9 |  451 | `		}else if( nLen + nOfft > nSrcLen ){` |
|      - |  452 | `			/* Invalid length */` |
|      3 |  453 | `			nLen = nSrcLen - nOfft;` |
|      1 |  454 | `		}` |
|      9 |  455 | `		if( nArg > 4 ){` |
|      - |  456 | `			/* Case-sensitive or not */` |
|      5 |  457 | `			iCase = ph7_value_to_bool(apArg[4]);` |
|      2 |  458 | `		}` |
|      4 |  459 | `	}` |
|      - |  460 | `	/* Perform the comparison */` |
|     11 |  461 | `	if( iCase ){` |
|      3 |  462 | `		rc = SyStrnicmp(zOfft,zSub,(sxu32)nLen);` |
|      2 |  463 | `	}else{` |
|      9 |  464 | `		rc = SyStrncmp(zOfft,zSub,(sxu32)nLen);` |
|      - |  465 | `	}` |
|      - |  466 | `	/* Comparison result */` |
|     11 |  467 | `	ph7_result_int(pCtx,rc);` |
|     11 |  468 | `	return PH7_OK;` |
|     14 |  469 |  |
|      - |  470 | `/*` |
|      - |  471 | ` * int substr_count(string $haystack,string $needle[,int $offset = 0 [,int $length ]])` |
|      - |  472 | ` *  Count the number of substring occurrences.` |
|      - |  473 | ` * Parameters` |
|      - |  474 | ` * $haystack` |
|      - |  475 | ` *   The string to search in` |
|      - |  476 | ` * $needle` |
|      - |  477 | ` *   The substring to search for` |
|      - |  478 | ` * $offset` |
|      - |  479 | ` *  The offset where to start counting` |
|      - |  480 | ` * $length (NOT USED)` |
|      - |  481 | ` *  The maximum length after the specified offset to search for the substring.` |
|      - |  482 | ` *  It outputs a warning if the offset plus the length is greater than the haystack length.` |
|      - |  483 | ` * Return` |
|      - |  484 | ` *  Toral number of substring occurrences.` |
|      - |  485 | ` */` |
|     24 |  486 | `static int PH7_builtin_substr_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  487 |  |
|      - |  488 | `	const char *zText,*zPattern,*zEnd;` |
|      - |  489 | `	int nTextlen,nPatlen;` |
|     25 |  490 | `	int iCount = 0;` |
|      - |  491 | `	sxu32 nOfft;` |
|      - |  492 | `	sxi32 rc;` |
|     25 |  493 | `	if( nArg < 2 ){` |
|      - |  494 | `		/* Missing arguments */` |
|      5 |  495 | `		ph7_result_int(pCtx,0);` |
|      5 |  496 | `		return PH7_OK;` |
|      - |  497 | `	}` |
|      - |  498 | `	/* Point to the haystack */` |
|     21 |  499 | `	zText = ph7_value_to_string(apArg[0],&nTextlen);` |
|      - |  500 | `	/* Point to the neddle */` |
|     21 |  501 | `	zPattern = ph7_value_to_string(apArg[1],&nPatlen);` |
|     21 |  502 | `	if( nTextlen < 1 \|\| nPatlen < 1 \|\| nPatlen > nTextlen ){` |
|      - |  503 | `		/* NOOP,return zero */` |
|      3 |  504 | `		ph7_result_int(pCtx,0);` |
|      3 |  505 | `		return PH7_OK;` |
|      - |  506 | `	}` |
|     19 |  507 | `	if( nArg > 2 ){` |
|      - |  508 | `		int iOfft;` |
|      - |  509 | `		/* Extract the offset */` |
|     15 |  510 | `		iOfft = ph7_value_to_int(apArg[2]);` |
|     15 |  511 | `		if( iOfft < 0 \|\| iOfft > nTextlen ){` |
|      - |  512 | `			/* Invalid offset,return zero */` |
|      3 |  513 | `			ph7_result_int(pCtx,0);` |
|      3 |  514 | `			return PH7_OK;` |
|      - |  515 | `		}` |
|      - |  516 | `		/* Point to the desired offset */` |
|     13 |  517 | `		zText = &zText[iOfft];` |
|      - |  518 | `		/* Adjust length */` |
|     13 |  519 | `		nTextlen -= iOfft;` |
|      6 |  520 | `	}` |
|      - |  521 | `	/* Point to the end of the string */` |
|     17 |  522 | `	zEnd = &zText[nTextlen];` |
|     17 |  523 | `	if( nArg > 3 ){` |
|      - |  524 | `		int nLen;` |
|      - |  525 | `		/* Extract the length */` |
|     13 |  526 | `		nLen = ph7_value_to_int(apArg[3]);` |
|     13 |  527 | `		if( nLen < 0 \|\| nLen > nTextlen ){` |
|      - |  528 | `			/* Invalid length,return 0 */` |
|      7 |  529 | `			ph7_result_int(pCtx,0);` |
|      7 |  530 | `			return PH7_OK;` |
|      - |  531 | `		}` |
|      - |  532 | `		/* Adjust pointer */` |
|      7 |  533 | `		nTextlen = nLen;` |
|      7 |  534 | `		zEnd = &zText[nTextlen];` |
|      3 |  535 | `	}` |
|      - |  536 | `	/* Perform the search */` |
|     12 |  537 | `	for(;;){` |
|     25 |  538 | `		rc = SyBlobSearch((const void *)zText,(sxu32)(zEnd-zText),(const void *)zPattern,nPatlen,&nOfft);` |
|     25 |  539 | `		if( rc != SXRET_OK ){` |
|      - |  540 | `			/* Pattern not found,break immediately */` |
|      9 |  541 | `			break;` |
|      - |  542 | `		}` |
|      - |  543 | `		/* Increment counter and update the offset */` |
|     17 |  544 | `		iCount++;` |
|     17 |  545 | `		zText += nOfft + nPatlen;` |
|     17 |  546 | `		if( zText >= zEnd ){` |
|      3 |  547 | `			break;` |
|      - |  548 | `		}` |
|      1 |  549 | `	}` |
|      - |  550 | `	/* Pattern count */` |
|     11 |  551 | `	ph7_result_int(pCtx,iCount);` |
|     11 |  552 | `	return PH7_OK;` |
|     13 |  553 |  |
|      - |  554 | `/*` |
|      - |  555 | ` * string chunk_split(string $body[,int $chunklen = 76 [, string $end = "\r\n" ]])` |
|      - |  556 | ` *   Split a string into smaller chunks.` |
|      - |  557 | ` * Parameters` |
|      - |  558 | ` *  $body` |
|      - |  559 | ` *   The string to be chunked.` |
|      - |  560 | ` * $chunklen` |
|      - |  561 | ` *   The chunk length.` |
|      - |  562 | ` * $end` |
|      - |  563 | ` *   The line ending sequence.` |
|      - |  564 | ` * Return` |
|      - |  565 | ` *  The chunked string or NULL on failure.` |
|      - |  566 | ` */` |
|     16 |  567 | `static int PH7_builtin_chunk_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  568 |  |
|     17 |  569 | `	const char *zIn,*zEnd,*zSep = "\r\n";` |
|      - |  570 | `	int nSepLen,nChunkLen,nLen;` |
|     17 |  571 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  572 | `		/* Nothing to split,return null */` |
|      5 |  573 | `		ph7_result_null(pCtx);` |
|      5 |  574 | `		return PH7_OK;` |
|      - |  575 | `	}` |
|      - |  576 | `	/* initialize/Extract arguments */` |
|     13 |  577 | `	nSepLen = (int)sizeof("\r\n") - 1;` |
|     13 |  578 | `	nChunkLen = 76;` |
|     13 |  579 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 |  580 | `	zEnd = &zIn[nLen];` |
|     13 |  581 | `	if( nArg > 1 ){` |
|      - |  582 | `		/* Chunk length */` |
|     13 |  583 | `		nChunkLen = ph7_value_to_int(apArg[1]);` |
|     13 |  584 | `		if( nChunkLen < 1 ){` |
|      - |  585 | `			/* Switch back to the default length */` |
|      3 |  586 | `			nChunkLen = 76;` |
|      1 |  587 | `		}` |
|     13 |  588 | `		if( nArg > 2 ){` |
|      - |  589 | `			/* Separator */` |
|      9 |  590 | `			zSep = ph7_value_to_string(apArg[2],&nSepLen);` |
|      9 |  591 | `			if( nSepLen < 1 ){` |
|      - |  592 | `				/* Switch back to the default separator */` |
|      3 |  593 | `				zSep = "\r\n";` |
|      3 |  594 | `				nSepLen = (int)sizeof("\r\n") - 1;` |
|      1 |  595 | `			}` |
|      4 |  596 | `		}` |
|      6 |  597 | `	}` |
|      - |  598 | `	/* Perform the requested operation */` |
|     13 |  599 | `	if( nChunkLen > nLen ){` |
|      - |  600 | `		/* Nothing to split,return the string and the separator */` |
|      9 |  601 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nLen,zIn,nSepLen,zSep);` |
|      9 |  602 | `		return PH7_OK;` |
|      - |  603 | `	}` |
|     17 |  604 | `	while( zIn < zEnd ){` |
|     13 |  605 | `		if( nChunkLen > (int)(zEnd-zIn) ){` |
|      3 |  606 | `			nChunkLen = (int)(zEnd - zIn);` |
|      1 |  607 | `		}` |
|      - |  608 | `		/* Append the chunk and the separator */` |
|     13 |  609 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nChunkLen,zIn,nSepLen,zSep);` |
|      - |  610 | `		/* Point beyond the chunk */` |
|     13 |  611 | `		zIn += nChunkLen;` |
|      1 |  612 | `	}` |
|      5 |  613 | `	return PH7_OK;` |
|      9 |  614 |  |
|      - |  615 | `/*` |
|      - |  616 | ` * string addslashes(string $str)` |
|      - |  617 | ` *  Quote string with slashes.` |
|      - |  618 | ` *  Returns a string with backslashes before characters that need` |
|      - |  619 | ` *  to be quoted in database queries etc. These characters are single` |
|      - |  620 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - |  621 | ` * Parameter` |
|      - |  622 | ` *  str: The string to be escaped.` |
|      - |  623 | ` * Return` |
|      - |  624 | ` *  Returns the escaped string` |
|      - |  625 | ` */` |
|     24 |  626 | `static int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  627 |  |
|      - |  628 | `	const char *zCur,*zIn,*zEnd;` |
|      - |  629 | `	int nLen;` |
|      - |  630 | `	/* PHP enforces exactly one argument. */` |
|     26 |  631 | `	if( nArg != 1 ){` |
|      7 |  632 | `		return PH7_VmThrowException(pCtx,` |
|      - |  633 | `			"ArgumentCountError",` |
|      - |  634 | `			"addslashes() expects exactly 1 argument, %d given",` |
|      2 |  635 | `			nArg` |
|      - |  636 | `			);` |
|      - |  637 | `	}` |
|      - |  638 | `	/* NULL is deprecated and treated as an empty string; other invalid` |
|      - |  639 | `	 * types still produce a TypeError. */` |
|     22 |  640 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 |  641 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - |  642 | `			E_DEPRECATED,` |
|      - |  643 | `			"addslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - |  644 | `			);` |
|      - |  645 | `		/* fall through so conversion below yields empty string */` |
|      1 |  646 | `	}` |
|      - |  647 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     38 |  648 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     29 |  649 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     18 |  650 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 |  651 | `		return PH7_VmThrowException(pCtx,` |
|      - |  652 | `			"TypeError",` |
|      - |  653 | `			"addslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      1 |  654 | `			ph7_type_name(apArg[0])` |
|      - |  655 | `			);` |
|      - |  656 | `	}` |
|      - |  657 | `	/* Convert to string representation first and obtain length. */` |
|     19 |  658 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     19 |  659 | `	if( nLen < 1 ){` |
|      - |  660 | `		/* Return the empty string */` |
|      5 |  661 | `		ph7_result_string(pCtx,"",0);` |
|      5 |  662 | `		return PH7_OK;` |
|      - |  663 | `	}` |
|     15 |  664 | `	zEnd = &zIn[nLen];` |
|     15 |  665 | `	zCur = 0; /* cc warning */` |
|     20 |  666 | `	for(;;){` |
|     41 |  667 | `		if( zIn >= zEnd ){` |
|      - |  668 | `			/* No more input */` |
|     15 |  669 | `			break;` |
|      - |  670 | `		}` |
|     27 |  671 | `		zCur = zIn;` |
|      - |  672 | `		/* scan until a character that needs escaping (', ", \\, or NUL) */` |
|     89 |  673 | `		while( zIn < zEnd && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '\\' && zIn[0] != '\0' ){` |
|     63 |  674 | `			zIn++;` |
|      1 |  675 | `		}` |
|     27 |  676 | `		if( zIn > zCur ){` |
|      - |  677 | `			/* Append raw contents */` |
|     23 |  678 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 |  679 | `		}` |
|     27 |  680 | `		if( zIn < zEnd ){` |
|     17 |  681 | `			int c = zIn[0];` |
|     17 |  682 | `			if( c == '\0' ){` |
|      - |  683 | `				/* PHP escapes NUL as "\\0" (two characters) */` |
|      3 |  684 | `				ph7_result_string(pCtx,"\\0",2);` |
|      2 |  685 | `			}else{` |
|     15 |  686 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - |  687 | `			}` |
|      8 |  688 | `		}` |
|     27 |  689 | `		zIn++;` |
|      1 |  690 | `	}` |
|     15 |  691 | `	return PH7_OK;` |
|     14 |  692 |  |
|      - |  693 | `/*` |
|      - |  694 | ` * Check if the given character is present in the given mask.` |
|      - |  695 | ` * Return TRUE if present. FALSE otherwise.` |
|      - |  696 | ` */` |
|    124 |  697 | `static int cSlashCheckMask(int c,const char *zMask,int nLen)` |
|      1 |  698 |  |
|    125 |  699 | `	const char *zEnd = &zMask[nLen];` |
|    555 |  700 | `	while( zMask < zEnd ){` |
|      - |  701 | `		/* Support range syntax A..Z where A and Z are literal bytes.  The` |
|      - |  702 | `		 * original PH7 implementation ignored ranges; tests rely on them so` |
|      - |  703 | `		 * provide a simple on-the-fly check here. */` |
|    475 |  704 | `		if( zMask + 3 < zEnd && zMask[1] == '.' && zMask[2] == '.' ){` |
|      3 |  705 | `			int lo = (unsigned char)zMask[0];` |
|      3 |  706 | `			int hi = (unsigned char)zMask[3];` |
|      3 |  707 | `			if( lo > hi ){` |
|    ! 0 |  708 | `				int tmp = lo; lo = hi; hi = tmp;` |
|    ! 0 |  709 | `			}` |
|      3 |  710 | `			if( c >= lo && c <= hi ){` |
|      3 |  711 | `				return 1;` |
|      - |  712 | `			}` |
|      - |  713 | `			/* consume the range specifier */` |
|    ! 0 |  714 | `			zMask += 4;` |
|    ! 0 |  715 | `			continue;` |
|      - |  716 | `		}` |
|    473 |  717 | `		if( zMask[0] == c ){` |
|      - |  718 | `			/* Character present,return TRUE */` |
|     43 |  719 | `			return 1;` |
|      - |  720 | `		}` |
|      - |  721 | `		/* Advance the pointer */` |
|    431 |  722 | `		zMask++;` |
|      1 |  723 | `	}` |
|      - |  724 | `	/* Not present */` |
|     81 |  725 | `	return 0;` |
|     63 |  726 |  |
|      - |  727 | `/*` |
|      - |  728 | ` * string addcslashes(string $str,string $charlist)` |
|      - |  729 | ` *  Quote string with slashes in a C style.` |
|      - |  730 | ` * Parameter` |
|      - |  731 | ` *  $str:` |
|      - |  732 | ` *    The string to be escaped.` |
|      - |  733 | ` *  $charlist:` |
|      - |  734 | ` *    A list of characters to be escaped. If charlist contains characters \n, \r etc.` |
|      - |  735 | ` *    they are converted in C-like style, while other non-alphanumeric characters` |
|      - |  736 | ` *    with ASCII codes lower than 32 and higher than 126 converted to octal representation.` |
|      - |  737 | ` * Return` |
|      - |  738 | ` *  Returns the escaped string.` |
|      - |  739 | ` * Note:` |
|      - |  740 | ` *  Range characters [i.e: 'A..Z'] is not implemented in the current release.` |
|      - |  741 | ` */` |
|     34 |  742 | `static int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  743 |  |
|      - |  744 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|      - |  745 | `	int nLen,nMask;` |
|      - |  746 | `	/* PHP enforces exactly two arguments. */` |
|     36 |  747 | `	if( nArg != 2 ){` |
|      7 |  748 | `		return PH7_VmThrowException(pCtx,` |
|      - |  749 | `			"ArgumentCountError",` |
|      - |  750 | `			"addcslashes() expects exactly 2 arguments, %d given",` |
|      2 |  751 | `			nArg` |
|      - |  752 | `			);` |
|      - |  753 | `	}` |
|      - |  754 | `	/* First argument must be a string-ish value.  NULL is deprecated and` |
|      - |  755 | `	 * treated as the empty string (PHP 8.1). */` |
|     32 |  756 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      - |  757 | `		/* Emit deprecation only once, similar to PHP behaviour. */` |
|      3 |  758 | `		PH7_VmThrowError(pCtx->pVm,0,/* iErr will be patched to 8192 below */` |
|      - |  759 | `			E_DEPRECATED,` |
|      - |  760 | `			"addcslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - |  761 | `			);` |
|      - |  762 | `		/* treat as empty string; fall through to conversion logic */` |
|     56 |  763 | `	} else if( ph7_value_is_array(apArg[0]) \|\|` |
|     41 |  764 | `	          ph7_value_is_object(apArg[0]) \|\|` |
|     26 |  765 | `	          ph7_value_is_resource(apArg[0]) ){` |
|      4 |  766 | `		return PH7_VmThrowException(pCtx,` |
|      - |  767 | `			"TypeError",` |
|      - |  768 | `			"addcslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      1 |  769 | `			ph7_type_name(apArg[0])` |
|      - |  770 | `			);` |
|      - |  771 | `	}` |
|      - |  772 | `	/* Second argument must be a string.  NULL is deprecated and treated as` |
|      - |  773 | `	 * an empty mask per PHP semantics.  Arrays/objects/resources still` |
|      - |  774 | `	 * trigger a TypeError. */` |
|     30 |  775 | `	if( ph7_value_is_null(apArg[1]) ){` |
|      3 |  776 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - |  777 | `			E_DEPRECATED,` |
|      - |  778 | `			"addcslashes(): Passing null to parameter #2 ($characters) of type string is deprecated"` |
|      - |  779 | `			);` |
|      - |  780 | `		/* allow through so it becomes empty string below */` |
|     52 |  781 | `	} else if( ph7_value_is_array(apArg[1]) \|\|` |
|     38 |  782 | `	          ph7_value_is_object(apArg[1]) \|\|` |
|     24 |  783 | `	          ph7_value_is_resource(apArg[1]) ){` |
|      4 |  784 | `		return PH7_VmThrowException(pCtx,` |
|      - |  785 | `			"TypeError",` |
|      - |  786 | `			"addcslashes(): Argument #2 ($characters) must be of type string, %s given",` |
|      2 |  787 | `			ph7_type_name(apArg[1])` |
|      - |  788 | `			);` |
|      - |  789 | `	}` |
|      - |  790 | `	/* Extract the string to process */` |
|     27 |  791 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      - |  792 | `	/* NULL would never reach here due to the check above. */` |
|     27 |  793 | `	if( nLen < 1 ){` |
|      - |  794 | `		/* Empty string returns itself. */` |
|      5 |  795 | `		ph7_result_string(pCtx,zIn,nLen);` |
|      5 |  796 | `		return PH7_OK;` |
|      - |  797 | `	}` |
|      - |  798 | `	/* Extract the desired mask */` |
|     23 |  799 | `	zMask = ph7_value_to_string(apArg[1],&nMask);` |
|     23 |  800 | `	zEnd = &zIn[nLen];` |
|     23 |  801 | `	zCur = 0; /* cc warning */` |
|     29 |  802 | `	for(;;){` |
|     59 |  803 | `		if( zIn >= zEnd ){` |
|      - |  804 | `			/* No more input */` |
|     23 |  805 | `			break;` |
|      - |  806 | `		}` |
|     37 |  807 | `		zCur = zIn;` |
|     91 |  808 | `		while( zIn < zEnd && !cSlashCheckMask(zIn[0],zMask,nMask) ){` |
|     55 |  809 | `			zIn++;` |
|      1 |  810 | `		}` |
|     37 |  811 | `		if( zIn > zCur ){` |
|      - |  812 | `			/* Append raw contents */` |
|     33 |  813 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     16 |  814 | `		}` |
|     37 |  815 | `		if( zIn < zEnd ){` |
|      - |  816 | `			/* Make sure we treat the byte as unsigned to avoid negative values` |
|      - |  817 | `			 * on platforms where char is signed. */` |
|     19 |  818 | `			int c = (unsigned char)zIn[0];` |
|      - |  819 | `			/* Handle special C-like escapes for common control characters first.` |
|      - |  820 | `			 * PHP outputs "\n" "\r" "\t" "\v" "\f" when those chars are` |
|      - |  821 | `			 * in the mask. NUL is left to the octal conversion below. */` |
|     19 |  822 | `			if( c == '\n' ){` |
|      3 |  823 | `				ph7_result_string(pCtx,"\\n",2);` |
|     18 |  824 | `			}else if( c == '\r' ){` |
|      3 |  825 | `				ph7_result_string(pCtx,"\\r",2);` |
|     16 |  826 | `			}else if( c == '\t' ){` |
|      3 |  827 | `				ph7_result_string(pCtx,"\\t",2);` |
|     14 |  828 | `			}else if( c == '\v' ){` |
|      3 |  829 | `				ph7_result_string(pCtx,"\\v",2);` |
|     12 |  830 | `			}else if( c == '\f' ){` |
|      3 |  831 | `				ph7_result_string(pCtx,"\\f",2);` |
|     10 |  832 | `			}else if( c > 126 \|\| (c < 32 && (!SyisAlphaNum(c)/*EBCDIC*/ && !SyisSpace(c))) ){` |
|      - |  833 | `				/* Convert to octal.  PHP always emits three-digit zero-padded` |
|      - |  834 | `				 * octal escapes (\001 not \1). */` |
|      7 |  835 | `				ph7_result_string_format(pCtx,"\\%03o",c);` |
|      4 |  836 | `			}else{` |
|      3 |  837 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - |  838 | `			}` |
|      9 |  839 | `		}` |
|     37 |  840 | `		zIn++;` |
|      1 |  841 | `	}` |
|     23 |  842 | `	return PH7_OK;` |
|     19 |  843 |  |
|      - |  844 | `/*` |
|      - |  845 | ` * string quotemeta(string $str)` |
|      - |  846 | ` *  Quote meta characters.` |
|      - |  847 | ` * Parameter` |
|      - |  848 | ` *  $str:` |
|      - |  849 | ` *    The string to be escaped.` |
|      - |  850 | ` * Return` |
|      - |  851 | ` *  Returns the escaped string.` |
|      - |  852 | `*/` |
|     10 |  853 | `static int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  854 |  |
|      - |  855 | `	const char *zCur,*zIn,*zEnd;` |
|      - |  856 | `	int nLen;` |
|     11 |  857 | `	if( nArg < 1 ){` |
|      - |  858 | `		/* Nothing to process,retun NULL */` |
|      3 |  859 | `		ph7_result_null(pCtx);` |
|      3 |  860 | `		return PH7_OK;` |
|      - |  861 | `	}` |
|      - |  862 | `	/* Extract the string to process */` |
|      9 |  863 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      9 |  864 | `	if( nLen < 1 ){` |
|      - |  865 | `		/* Return the empty string */` |
|      3 |  866 | `		ph7_result_string(pCtx,"",0);` |
|      3 |  867 | `		return PH7_OK;` |
|      - |  868 | `	}` |
|      7 |  869 | `	zEnd = &zIn[nLen];` |
|      7 |  870 | `	zCur = 0; /* cc warning */` |
|     17 |  871 | `	for(;;){` |
|     35 |  872 | `		if( zIn >= zEnd ){` |
|      - |  873 | `			/* No more input */` |
|      7 |  874 | `			break;` |
|      - |  875 | `		}` |
|     29 |  876 | `		zCur = zIn;` |
|     55 |  877 | `		while( zIn < zEnd && !cSlashCheckMask(zIn[0],".\\+*?[^]($)",(int)sizeof(".\\+*?[^]($)")-1) ){` |
|     27 |  878 | `			zIn++;` |
|      1 |  879 | `		}` |
|     29 |  880 | `		if( zIn > zCur ){` |
|      - |  881 | `			/* Append raw contents */` |
|     11 |  882 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      5 |  883 | `		}` |
|     29 |  884 | `		if( zIn < zEnd ){` |
|     27 |  885 | `			int c = zIn[0];` |
|     27 |  886 | `			ph7_result_string_format(pCtx,"\\%c",c);` |
|     13 |  887 | `		}` |
|     29 |  888 | `		zIn++;` |
|      1 |  889 | `	}` |
|      7 |  890 | `	return PH7_OK;` |
|      6 |  891 |  |
|      - |  892 | `/*` |
|      - |  893 | ` * string stripslashes(string $str)` |
|      - |  894 | ` *  Un-quotes a quoted string.` |
|      - |  895 | ` *  Returns a string with backslashes before characters that need` |
|      - |  896 | ` *  to be quoted in database queries etc. These characters are single` |
|      - |  897 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - |  898 | ` * Parameter` |
|      - |  899 | ` *  $str` |
|      - |  900 | ` *   The input string.` |
|      - |  901 | ` * Return` |
|      - |  902 | ` *  Returns a string with backslashes stripped off.` |
|      - |  903 | ` */` |
|      8 |  904 | `static int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  905 |  |
|      - |  906 | `	const char *zCur,*zIn,*zEnd;` |
|      - |  907 | `	int nLen;` |
|      9 |  908 | `	if( nArg < 1 ){` |
|      - |  909 | `		/* Nothing to process,retun NULL */` |
|      3 |  910 | `		ph7_result_null(pCtx);` |
|      3 |  911 | `		return PH7_OK;` |
|      - |  912 | `	}` |
|      - |  913 | `	/* Extract the string to process */` |
|      7 |  914 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      7 |  915 | `	if( zIn == 0 ){` |
|    ! 0 |  916 | `		ph7_result_null(pCtx);` |
|    ! 0 |  917 | `		return PH7_OK;` |
|      - |  918 | `	}` |
|      7 |  919 | `	zEnd = &zIn[nLen];` |
|      7 |  920 | `	zCur = 0; /* cc warning */` |
|      - |  921 | `	/* Encode the string */` |
|      4 |  922 | `	for(;;){` |
|      9 |  923 | `		if( zIn >= zEnd ){` |
|      - |  924 | `			/* No more input */` |
|      5 |  925 | `			break;` |
|      - |  926 | `		}` |
|      5 |  927 | `		zCur = zIn;` |
|     17 |  928 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|     13 |  929 | `			zIn++;` |
|      1 |  930 | `		}` |
|      5 |  931 | `		if( zIn > zCur ){` |
|      - |  932 | `			/* Append raw contents */` |
|      5 |  933 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 |  934 | `		}` |
|      5 |  935 | `		if( &zIn[1] < zEnd ){` |
|      3 |  936 | `			int c = zIn[1];` |
|      3 |  937 | `			if( c == '\'' \|\| c == '"' \|\| c == '\\' ){` |
|      - |  938 | `				/* Ignore the backslash */` |
|      3 |  939 | `				zIn++;` |
|      1 |  940 | `			}` |
|      2 |  941 | `		}else{` |
|      3 |  942 | `			break;` |
|      - |  943 | `		}` |
|      1 |  944 | `	}` |
|      7 |  945 | `	return PH7_OK;` |
|      5 |  946 |  |
|      - |  947 | `/*` |
|      - |  948 | ` * string htmlspecialchars(string $string [, int $flags = ENT_COMPAT \| ENT_HTML401 [, string $charset]])` |
|      - |  949 | ` *  HTML escaping of special characters.` |
|      - |  950 | ` *  The translations performed are:` |
|      - |  951 | ` *   '&' (ampersand) ==> '&amp;'` |
|      - |  952 | ` *   '"' (double quote) ==> '&quot;' when ENT_NOQUOTES is not set.` |
|      - |  953 | ` *   "'" (single quote) ==> '&#039;' only when ENT_QUOTES is set.` |
|      - |  954 | ` *   '<' (less than) ==> '&lt;'` |
|      - |  955 | ` *   '>' (greater than) ==> '&gt;'` |
|      - |  956 | ` * Parameters` |
|      - |  957 | ` *  $string` |
|      - |  958 | ` *   The string being converted.` |
|      - |  959 | ` * $flags` |
|      - |  960 | ` *   A bitmask of one or more of the following flags, which specify how to handle quotes.` |
|      - |  961 | ` *   The default is ENT_COMPAT \| ENT_HTML401.` |
|      - |  962 | ` *   ENT_COMPAT 	Will convert double-quotes and leave single-quotes alone.` |
|      - |  963 | ` *   ENT_QUOTES 	Will convert both double and single quotes.` |
|      - |  964 | ` *   ENT_NOQUOTES 	Will leave both double and single quotes unconverted.` |
|      - |  965 | ` *   ENT_IGNORE 	Silently discard invalid code unit sequences instead of returning an empty string.` |
|      - |  966 | ` * $charset` |
|      - |  967 | ` *  Defines character set used in conversion. The default character set is ISO-8859-1. (Not used)` |
|      - |  968 | ` * Return` |
|      - |  969 | ` *  The escaped string or NULL on failure.` |
|      - |  970 | ` */` |
|     20 |  971 | `static int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  972 |  |
|      - |  973 | `	const char *zCur,*zIn,*zEnd;` |
|     21 |  974 | `	int iFlags = 0x01\|0x40; /* ENT_COMPAT \| ENT_HTML401 */` |
|      - |  975 | `	int nLen,c;` |
|     21 |  976 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  977 | `		/* Missing/Invalid arguments,return NULL */` |
|      9 |  978 | `		ph7_result_null(pCtx);` |
|      9 |  979 | `		return PH7_OK;` |
|      - |  980 | `	}` |
|      - |  981 | `	/* Extract the target string */` |
|     13 |  982 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - |  983 | `	/* Return early when the input is empty, mirroring PHP's behavior. */` |
|     13 |  984 | `	if( nLen == 0 ){` |
|      3 |  985 | `		ph7_result_string(pCtx,"",0);` |
|      3 |  986 | `		return PH7_OK;` |
|      - |  987 | `	}` |
|     11 |  988 | `	zEnd = &zIn[nLen];` |
|      - |  989 | `	/* Extract the flags if available */` |
|     11 |  990 | `	if( nArg > 1 ){` |
|      9 |  991 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      9 |  992 | `		if( iFlags < 0 ){` |
|      3 |  993 | `			iFlags = 0x01\|0x40;` |
|      1 |  994 | `		}` |
|      4 |  995 | `	}` |
|      - |  996 | `	/* Perform the requested operation */` |
|     23 |  997 | `	for(;;){` |
|     47 |  998 | `		if( zIn >= zEnd ){` |
|      9 |  999 | `			break;` |
|      - | 1000 | `		}` |
|     39 | 1001 | `		zCur = zIn;` |
|     83 | 1002 | `		while( zIn < zEnd && zIn[0] != '&' && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '<' && zIn[0] != '>' ){` |
|     45 | 1003 | `			zIn++;` |
|      1 | 1004 | `		}` |
|     39 | 1005 | `		if( zCur < zIn ){` |
|      - | 1006 | `			/* Append the raw string verbatim */` |
|     17 | 1007 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      8 | 1008 | `		}` |
|     39 | 1009 | `		if( zIn >= zEnd ){` |
|      3 | 1010 | `			break;` |
|      - | 1011 | `		}` |
|     37 | 1012 | `		c = zIn[0];` |
|     37 | 1013 | `		if( c == '&' ){` |
|      - | 1014 | `			/* Expand '&amp;' */` |
|      9 | 1015 | `			ph7_result_string(pCtx,"&amp;",(int)sizeof("&amp;")-1);` |
|     33 | 1016 | `		}else if( c == '<' ){` |
|      - | 1017 | `			/* Expand '&lt;' */` |
|      7 | 1018 | `			ph7_result_string(pCtx,"&lt;",(int)sizeof("&lt;")-1);` |
|     26 | 1019 | `		}else if( c == '>' ){` |
|      - | 1020 | `			/* Expand '&gt;' */` |
|      9 | 1021 | `			ph7_result_string(pCtx,"&gt;",(int)sizeof("&gt;")-1);` |
|     19 | 1022 | `		}else if( c == '\'' ){` |
|      5 | 1023 | `			if( iFlags & 0x02 /*ENT_QUOTES*/ ){` |
|      - | 1024 | `				/* Expand '&#039;' */` |
|      5 | 1025 | `				ph7_result_string(pCtx,"&#039;",(int)sizeof("&#039;")-1);` |
|      3 | 1026 | `			}else{` |
|      - | 1027 | `				/* Leave the single quote untouched */` |
|    ! 0 | 1028 | `				ph7_result_string(pCtx,"'",(int)sizeof(char));` |
|      1 | 1029 | `			}` |
|     13 | 1030 | `		}else if( c == '"' ){` |
|     11 | 1031 | `			if( (iFlags & 0x04) == 0 /*ENT_NOQUOTES*/ ){` |
|      - | 1032 | `				/* Expand '&quot;' */` |
|      7 | 1033 | `				ph7_result_string(pCtx,"&quot;",(int)sizeof("&quot;")-1);` |
|      4 | 1034 | `			}else{` |
|      - | 1035 | `				/* Leave the double quote untouched */` |
|      5 | 1036 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      - | 1037 | `			}` |
|      5 | 1038 | `		}` |
|      - | 1039 | `		/* Ignore the unsafe HTML character */` |
|     37 | 1040 | `		zIn++;` |
|      1 | 1041 | `	}` |
|     11 | 1042 | `	return PH7_OK;` |
|     11 | 1043 |  |
|      - | 1044 | `/*` |
|      - | 1045 | ` * string htmlspecialchars_decode(string $string[,int $quote_style = ENT_COMPAT ])` |
|      - | 1046 | ` *  Unescape HTML entities.` |
|      - | 1047 | ` * Parameters` |
|      - | 1048 | ` *  $string` |
|      - | 1049 | ` *   The string to decode` |
|      - | 1050 | ` *  $quote_style` |
|      - | 1051 | ` *    The quote style. One of the following constants:` |
|      - | 1052 | ` *   ENT_COMPAT 	Will convert double-quotes and leave single-quotes alone (default)` |
|      - | 1053 | ` *   ENT_QUOTES 	Will convert both double and single quotes` |
|      - | 1054 | ` *   ENT_NOQUOTES 	Will leave both double and single quotes unconverted` |
|      - | 1055 | ` * Return` |
|      - | 1056 | ` *  The unescaped string or NULL on failure.` |
|      - | 1057 | ` */` |
|     16 | 1058 | `static int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1059 |  |
|      - | 1060 | `	const char *zCur,*zIn,*zEnd;` |
|     17 | 1061 | `	int iFlags = 0x01; /* ENT_COMPAT */` |
|      - | 1062 | `	int nLen,nJump;` |
|     17 | 1063 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1064 | `		/* Missing/Invalid arguments,return NULL */` |
|      7 | 1065 | `		ph7_result_null(pCtx);` |
|      7 | 1066 | `		return PH7_OK;` |
|      - | 1067 | `	}` |
|      - | 1068 | `	/* Extract the target string */` |
|     11 | 1069 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 1070 | `	zEnd = &zIn[nLen];` |
|      - | 1071 | `	/* Extract the flags if available */` |
|     11 | 1072 | `	if( nArg > 1 ){` |
|      7 | 1073 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      7 | 1074 | `		if( iFlags < 0 ){` |
|      3 | 1075 | `			iFlags = 0x01;` |
|      1 | 1076 | `		}` |
|      3 | 1077 | `	}` |
|      - | 1078 | `	/* Perform the requested operation */` |
|     15 | 1079 | `	for(;;){` |
|     31 | 1080 | `		if( zIn >= zEnd ){` |
|     11 | 1081 | `			break;` |
|      - | 1082 | `		}` |
|     21 | 1083 | `		zCur = zIn;` |
|     51 | 1084 | `		while( zIn < zEnd && zIn[0] != '&' ){` |
|     31 | 1085 | `			zIn++;` |
|      1 | 1086 | `		}` |
|     21 | 1087 | `		if( zCur < zIn ){` |
|      - | 1088 | `			/* Append the raw string verbatim */` |
|      9 | 1089 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 1090 | `		}` |
|     21 | 1091 | `		nLen = (int)(zEnd-zIn);` |
|     21 | 1092 | `		nJump = (int)sizeof(char);` |
|     21 | 1093 | `		if( nLen >= (int)sizeof("&amp;")-1 && SyStrnicmp(zIn,"&amp;",sizeof("&amp;")-1) == 0 ){` |
|      - | 1094 | `			/* &amp; ==> '&' */` |
|      3 | 1095 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|      3 | 1096 | `			nJump = (int)sizeof("&amp;")-1;` |
|     20 | 1097 | `		}else if( nLen >= (int)sizeof("&lt;")-1 && SyStrnicmp(zIn,"&lt;",sizeof("&lt;")-1) == 0 ){` |
|      - | 1098 | `			/* &lt; ==> < */` |
|      3 | 1099 | `			ph7_result_string(pCtx,"<",(int)sizeof(char));` |
|      3 | 1100 | `			nJump = (int)sizeof("&lt;")-1;` |
|     18 | 1101 | `		}else if( nLen >= (int)sizeof("&gt;")-1 && SyStrnicmp(zIn,"&gt;",sizeof("&gt;")-1) == 0 ){` |
|      - | 1102 | `			/* &gt; ==> '>' */` |
|      3 | 1103 | `			ph7_result_string(pCtx,">",(int)sizeof(char));` |
|      3 | 1104 | `			nJump = (int)sizeof("&gt;")-1;` |
|     16 | 1105 | `		}else if( nLen >= (int)sizeof("&quot;")-1 && SyStrnicmp(zIn,"&quot;",sizeof("&quot;")-1) == 0 ){` |
|      - | 1106 | `			/* &quot; ==> '"' */` |
|     13 | 1107 | `			if( (iFlags & 0x04) == 0 /*ENT_NOQUOTES*/ ){` |
|      9 | 1108 | `				ph7_result_string(pCtx,"\"",(int)sizeof(char));` |
|      5 | 1109 | `			}else{` |
|      - | 1110 | `				/* Leave untouched */` |
|      5 | 1111 | `				ph7_result_string(pCtx,"&quot;",(int)sizeof("&quot;")-1);` |
|      - | 1112 | `			}` |
|     13 | 1113 | `			nJump = (int)sizeof("&quot;")-1;` |
|      9 | 1114 | `		}else if( nLen >= (int)sizeof("&#039;")-1 && SyStrnicmp(zIn,"&#039;",sizeof("&#039;")-1) == 0 ){` |
|      - | 1115 | `			/* &#039; ==> ''' */` |
|      3 | 1116 | `			if( iFlags & 0x02 /*ENT_QUOTES*/ ){` |
|      - | 1117 | `				/* Expand ''' */` |
|      3 | 1118 | `				ph7_result_string(pCtx,"'",(int)sizeof(char));` |
|      2 | 1119 | `			}else{` |
|      - | 1120 | `				/* Leave untouched */` |
|    ! 0 | 1121 | `				ph7_result_string(pCtx,"&#039;",(int)sizeof("&#039;")-1);` |
|      - | 1122 | `			}` |
|      3 | 1123 | `			nJump = (int)sizeof("&#039;")-1;` |
|      1 | 1124 | `		}else if( nLen >= (int)sizeof(char) ){` |
|      - | 1125 | `			/* expand '&' */` |
|    ! 0 | 1126 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|    ! 0 | 1127 | `		}else{` |
|      - | 1128 | `			/* No more input to process */` |
|    ! 0 | 1129 | `			break;` |
|      - | 1130 | `		}` |
|     21 | 1131 | `		zIn += nJump;` |
|      1 | 1132 | `	}` |
|     11 | 1133 | `	return PH7_OK;` |
|      9 | 1134 |  |
|      - | 1135 | `/* HTML encoding/Decoding table` |
|      - | 1136 | ` * Source: Symisc RunTime API.[chm@symisc.net]` |
|      - | 1137 | ` */` |
|      - | 1138 | `static const char *azHtmlEscape[] = {` |
|      - | 1139 | ` 	"&lt;","<","&gt;",">","&amp;","&","&quot;","\"","&#39;","'",` |
|      - | 1140 | `	"&#33;","!","&#36;","$","&#35;","#","&#37;","%","&#40;","(",` |
|      - | 1141 | `	"&#41;",")","&#123;","{","&#125;","}","&#61;","=","&#43;","+",` |
|      - | 1142 | `	"&#63;","?","&#91;","[","&#93;","]","&#64;","@","&#44;",","` |
|      - | 1143 | ` };` |
|      - | 1144 | `/*` |
|      - | 1145 | ` * array get_html_translation_table(void)` |
|      - | 1146 | ` *  Returns the translation table used by htmlspecialchars() and htmlentities().` |
|      - | 1147 | ` * Parameters` |
|      - | 1148 | ` *  None` |
|      - | 1149 | ` * Return` |
|      - | 1150 | ` *  The translation table as an array or NULL on failure.` |
|      - | 1151 | ` */` |
|      4 | 1152 | `static int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1153 |  |
|      - | 1154 | `	ph7_value *pArray,*pValue;` |
|      - | 1155 | `	sxu32 n;` |
|      - | 1156 | `	/* Element value */` |
|      5 | 1157 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      5 | 1158 | `	if( pValue == 0 ){` |
|    ! 0 | 1159 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 1160 | `		SXUNUSED(apArg);` |
|      - | 1161 | `		/* Return NULL */` |
|    ! 0 | 1162 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1163 | `		return PH7_OK;` |
|      - | 1164 | `	}` |
|      - | 1165 | `	/* Create a new array */` |
|      5 | 1166 | `	pArray = ph7_context_new_array(pCtx);` |
|      5 | 1167 | `	if( pArray == 0 ){` |
|      - | 1168 | `		/* Return NULL */` |
|    ! 0 | 1169 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1170 | `		return PH7_OK;` |
|      - | 1171 | `	}` |
|      - | 1172 | `	/* Make the table */` |
|     85 | 1173 | `	for( n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|      - | 1174 | `		/* Prepare the value */` |
|     81 | 1175 | `		ph7_value_string(pValue,azHtmlEscape[n],-1 /* Compute length automatically */);` |
|      - | 1176 | `		/* Insert the value */` |
|     81 | 1177 | `		ph7_array_add_strkey_elem(pArray,azHtmlEscape[n+1],pValue);` |
|      - | 1178 | `		/* Reset the string cursor */` |
|     81 | 1179 | `		ph7_value_reset_string_cursor(pValue);` |
|     41 | 1180 | `	}` |
|      - | 1181 | `	/*` |
|      - | 1182 | `	 * Return the array.` |
|      - | 1183 | `	 * Don't worry about freeing memory, everything will be automatically` |
|      - | 1184 | `	 * released upon we return from this function.` |
|      - | 1185 | `	 */` |
|      5 | 1186 | `	ph7_result_value(pCtx,pArray);` |
|      5 | 1187 | `	return PH7_OK;` |
|      3 | 1188 |  |
|      - | 1189 | `/*` |
|      - | 1190 | ` * string htmlentities( string $string [, int $flags = ENT_COMPAT \| ENT_HTML401]);` |
|      - | 1191 | ` *   Convert all applicable characters to HTML entities` |
|      - | 1192 | ` * Parameters` |
|      - | 1193 | ` * $string` |
|      - | 1194 | ` *   The input string.` |
|      - | 1195 | ` * $flags` |
|      - | 1196 | ` *  A bitmask of one or more of the flags (see block-comment on PH7_builtin_htmlspecialchars())` |
|      - | 1197 | ` * Return` |
|      - | 1198 | ` * The encoded string.` |
|      - | 1199 | ` */` |
|     10 | 1200 | `static int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1201 |  |
|     11 | 1202 | `	int iFlags = 0x01; /* ENT_COMPAT */` |
|      - | 1203 | `	const char *zIn,*zEnd;` |
|      - | 1204 | `	int nLen,c;` |
|      - | 1205 | `	sxu32 n;` |
|     11 | 1206 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1207 | `		/* Missing/Invalid arguments,return NULL */` |
|      5 | 1208 | `		ph7_result_null(pCtx);` |
|      5 | 1209 | `		return PH7_OK;` |
|      - | 1210 | `	}` |
|      - | 1211 | `	/* Extract the target string */` |
|      7 | 1212 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1213 | `	/* Handle empty string up front */` |
|      7 | 1214 | `	if( nLen == 0 ){` |
|      3 | 1215 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1216 | `		return PH7_OK;` |
|      - | 1217 | `	}` |
|      5 | 1218 | `	zEnd = &zIn[nLen];` |
|      - | 1219 | `	/* Extract the flags if available */` |
|      5 | 1220 | `	if( nArg > 1 ){` |
|      3 | 1221 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      3 | 1222 | `		if( iFlags < 0 ){` |
|      3 | 1223 | `			iFlags = 0x01;` |
|      1 | 1224 | `		}` |
|      1 | 1225 | `	}` |
|      - | 1226 | `	/* Perform the requested operation */` |
|     11 | 1227 | `	for(;;){` |
|     23 | 1228 | `		if( zIn >= zEnd ){` |
|      - | 1229 | `			/* No more input to process */` |
|      5 | 1230 | `			break;` |
|      - | 1231 | `		}` |
|     19 | 1232 | `		c = zIn[0];` |
|      - | 1233 | `		/* Perform a linear lookup on the decoding table */` |
|    233 | 1234 | `		for( n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|    223 | 1235 | `			if( azHtmlEscape[n+1][0] == c ){` |
|      - | 1236 | `				/* Got one */` |
|      9 | 1237 | `				break;` |
|      - | 1238 | `			}` |
|    108 | 1239 | `		}` |
|     19 | 1240 | `		if( n < SX_ARRAYSIZE(azHtmlEscape) ){` |
|      - | 1241 | `			/* Output the safe sequence [i.e: '<' ==> '&lt;"] */` |
|      9 | 1242 | `			if( c == '"' && (iFlags & 0x04) /*ENT_NOQUOTES*/ ){` |
|      - | 1243 | `				/* Expand the double quote verbatim */` |
|    ! 0 | 1244 | `				ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      9 | 1245 | `			}else if(c == '\'' && ((iFlags & 0x02 /*ENT_QUOTES*/) == 0 \|\| (iFlags & 0x04) /*ENT_NOQUOTES*/) ){` |
|      - | 1246 | `				/* expand single quote verbatim */` |
|    ! 0 | 1247 | `				ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|    ! 0 | 1248 | `			}else{` |
|      9 | 1249 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1/*Compute length automatically */);` |
|      - | 1250 | `			}` |
|      5 | 1251 | `		}else{` |
|      - | 1252 | `			/* Output character verbatim */` |
|     11 | 1253 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 1254 | `		}` |
|     19 | 1255 | `		zIn++;` |
|      1 | 1256 | `	}` |
|      5 | 1257 | `	return PH7_OK;` |
|      6 | 1258 |  |
|      - | 1259 | `/*` |
|      - | 1260 | ` * string html_entity_decode(string $string [, int $quote_style = ENT_COMPAT [, string $charset = 'UTF-8' ]])` |
|      - | 1261 | ` *   Perform the reverse operation of html_entity_decode().` |
|      - | 1262 | ` * Parameters` |
|      - | 1263 | ` * $string` |
|      - | 1264 | ` *   The input string.` |
|      - | 1265 | ` * $flags` |
|      - | 1266 | ` *  A bitmask of one or more of the flags (see comment on PH7_builtin_htmlspecialchars())` |
|      - | 1267 | ` * Return` |
|      - | 1268 | ` * The decoded string.` |
|      - | 1269 | ` */` |
|     28 | 1270 | `static int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1271 |  |
|      - | 1272 | `	const char *zCur,*zIn,*zEnd;` |
|     29 | 1273 | `	int iFlags = 0x01; /* ENT_COMPAT  */` |
|      - | 1274 | `	int nLen;` |
|      - | 1275 | `	sxu32 n;` |
|     29 | 1276 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1277 | `		/* Missing/Invalid arguments,return NULL */` |
|      5 | 1278 | `		ph7_result_null(pCtx);` |
|      5 | 1279 | `		return PH7_OK;` |
|      - | 1280 | `	}` |
|      - | 1281 | `	/* Extract the target string */` |
|     25 | 1282 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 1283 | `	zEnd = &zIn[nLen];` |
|      - | 1284 | `	/* Extract the flags if available */` |
|     25 | 1285 | `	if( nArg > 1 ){` |
|     15 | 1286 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     15 | 1287 | `		if( iFlags < 0 ){` |
|      3 | 1288 | `			iFlags = 0x01;` |
|      1 | 1289 | `		}` |
|      7 | 1290 | `	}` |
|      - | 1291 | `	/* Perform the requested operation */` |
|     27 | 1292 | `	for(;;){` |
|     55 | 1293 | `		if( zIn >= zEnd ){` |
|      - | 1294 | `			/* No more input to process */` |
|     13 | 1295 | `			break;` |
|      - | 1296 | `		}` |
|     43 | 1297 | `		zCur = zIn;` |
|    173 | 1298 | `		while( zIn < zEnd && zIn[0] != '&' ){` |
|    131 | 1299 | `			zIn++;` |
|      1 | 1300 | `		}` |
|     43 | 1301 | `		if( zCur < zIn ){` |
|      - | 1302 | `			/* Append raw string verbatim */` |
|     27 | 1303 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     13 | 1304 | `		}` |
|     43 | 1305 | `		if( zIn >= zEnd ){` |
|     13 | 1306 | `			break;` |
|      - | 1307 | `		}` |
|     31 | 1308 | `		nLen = (int)(zEnd-zIn);` |
|      - | 1309 | `		/* Find an encoded sequence */` |
|    113 | 1310 | `		for(n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){` |
|    113 | 1311 | `			int iLen = (int)SyStrlen(azHtmlEscape[n]);` |
|    113 | 1312 | `			if( nLen >= iLen && SyStrnicmp(zIn,azHtmlEscape[n],(sxu32)iLen) == 0 ){` |
|      - | 1313 | `				/* Got one */` |
|     31 | 1314 | `				zIn += iLen;` |
|     31 | 1315 | `				break;` |
|      - | 1316 | `			}` |
|     42 | 1317 | `		}` |
|     31 | 1318 | `		if( n < SX_ARRAYSIZE(azHtmlEscape) ){` |
|     31 | 1319 | `			int c = azHtmlEscape[n+1][0];` |
|      - | 1320 | `			/* Output the decoded character */` |
|     31 | 1321 | `			if( c == '\'' && ((iFlags & 0x02) == 0 /*ENT_QUOTES*/\|\| (iFlags & 0x04) /*ENT_NOQUOTES*/)  ){` |
|      - | 1322 | `				/* Do not process single quotes */` |
|      9 | 1323 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1);` |
|     27 | 1324 | `			}else if( c == '"' && (iFlags & 0x04) /*ENT_NOQUOTES*/ ){` |
|      - | 1325 | `				/* Do not process double quotes */` |
|      5 | 1326 | `				ph7_result_string(pCtx,azHtmlEscape[n],-1);` |
|      3 | 1327 | `			}else{` |
|     19 | 1328 | `				ph7_result_string(pCtx,azHtmlEscape[n+1],-1); /* Compute length automatically */` |
|      - | 1329 | `			}` |
|     16 | 1330 | `		}else{` |
|      - | 1331 | `			/* Append '&' */` |
|    ! 0 | 1332 | `			ph7_result_string(pCtx,"&",(int)sizeof(char));` |
|    ! 0 | 1333 | `			zIn++;` |
|      - | 1334 | `		}` |
|      1 | 1335 | `	}` |
|     25 | 1336 | `	return PH7_OK;` |
|     15 | 1337 |  |
|      - | 1338 | `/*` |
|      - | 1339 | ` * int strlen($string)` |
|      - | 1340 | ` *  return the length of the given string.` |
|      - | 1341 | ` * Parameter` |
|      - | 1342 | ` *  string: The string being measured for length.` |
|      - | 1343 | ` * Return` |
|      - | 1344 | ` *  length of the given string.` |
|      - | 1345 | ` */` |
|   2308 | 1346 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1347 |  |
|   2310 | 1348 | `	int iLen = 0;` |
|   2310 | 1349 | `	if( nArg > 0 ){` |
|   2308 | 1350 | `		ph7_value_to_string(apArg[0],&iLen);` |
|   1153 | 1351 | `	}` |
|      - | 1352 | `	/* String length */` |
|   2310 | 1353 | `	ph7_result_int(pCtx,iLen);` |
|   2310 | 1354 | `	return PH7_OK;` |
|      2 | 1355 |  |
|      - | 1356 | `/*` |
|      - | 1357 | ` * int strcmp(string $str1,string $str2)` |
|      - | 1358 | ` *  Perform a binary safe string comparison.` |
|      - | 1359 | ` * Parameter` |
|      - | 1360 | ` *  str1: The first string` |
|      - | 1361 | ` *  str2: The second string` |
|      - | 1362 | ` * Return` |
|      - | 1363 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1364 | ` *  than str2, and 0 if they are equal.` |
|      - | 1365 | ` */` |
|     66 | 1366 | `static int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1367 |  |
|      - | 1368 | `	const char *z1,*z2;` |
|      - | 1369 | `	int n1,n2;` |
|      - | 1370 | `	int res;` |
|     67 | 1371 | `	if( nArg < 2 ){` |
|      5 | 1372 | `		res = nArg == 0 ? 0 : 1;` |
|      5 | 1373 | `		ph7_result_int(pCtx,res);` |
|      5 | 1374 | `		return PH7_OK;` |
|      - | 1375 | `	}` |
|      - | 1376 | `	/* Perform the comparison */` |
|     63 | 1377 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     63 | 1378 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     63 | 1379 | `	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 1380 | `	/* Comparison result */` |
|     63 | 1381 | `	ph7_result_int(pCtx,res);` |
|     63 | 1382 | `	return PH7_OK;` |
|     34 | 1383 |  |
|      - | 1384 | `/*` |
|      - | 1385 | ` * int strncmp(string $str1,string $str2,int n)` |
|      - | 1386 | ` *  Perform a binary safe string comparison of the first n characters.` |
|      - | 1387 | ` * Parameter` |
|      - | 1388 | ` *  str1: The first string` |
|      - | 1389 | ` *  str2: The second string` |
|      - | 1390 | ` * Return` |
|      - | 1391 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1392 | ` *  than str2, and 0 if they are equal.` |
|      - | 1393 | ` */` |
|     20 | 1394 | `static int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1395 |  |
|      - | 1396 | `	const char *z1,*z2;` |
|      - | 1397 | `	int res;` |
|      - | 1398 | `	int n;` |
|     21 | 1399 | `	if( nArg < 3 ){` |
|      - | 1400 | `		/* Perform a standard comparison */` |
|      5 | 1401 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|      - | 1402 | `	}` |
|      - | 1403 | `	/* Desired comparison length */` |
|     17 | 1404 | `	n  = ph7_value_to_int(apArg[2]);` |
|     17 | 1405 | `	if( n < 0 ){` |
|      - | 1406 | `		/* Invalid length */` |
|      3 | 1407 | `		ph7_result_int(pCtx,-1);` |
|      3 | 1408 | `		return PH7_OK;` |
|      - | 1409 | `	}` |
|      - | 1410 | `	/* Perform the comparison */` |
|     15 | 1411 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     15 | 1412 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     15 | 1413 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|      - | 1414 | `	/* Comparison result */` |
|     15 | 1415 | `	ph7_result_int(pCtx,res);` |
|     15 | 1416 | `	return PH7_OK;` |
|     11 | 1417 |  |
|      - | 1418 | `/*` |
|      - | 1419 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|      - | 1420 | ` *  Perform a binary safe case-insensitive string comparison.` |
|      - | 1421 | ` * Parameter` |
|      - | 1422 | ` *  str1: The first string` |
|      - | 1423 | ` *  str2: The second string` |
|      - | 1424 | ` * Return` |
|      - | 1425 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1426 | ` *  than str2, and 0 if they are equal.` |
|      - | 1427 | ` */` |
|     22 | 1428 | `static int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1429 |  |
|      - | 1430 | `	const char *z1,*z2;` |
|      - | 1431 | `	int n1,n2;` |
|      - | 1432 | `	int res;` |
|     23 | 1433 | `	if( nArg < 2 ){` |
|      9 | 1434 | `		res = nArg == 0 ? 0 : 1;` |
|      9 | 1435 | `		ph7_result_int(pCtx,res);` |
|      9 | 1436 | `		return PH7_OK;` |
|      - | 1437 | `	}` |
|      - | 1438 | `	/* Perform the comparison */` |
|     15 | 1439 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     15 | 1440 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     15 | 1441 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 1442 | `	/* Comparison result */` |
|     15 | 1443 | `	ph7_result_int(pCtx,res);` |
|     15 | 1444 | `	return PH7_OK;` |
|     12 | 1445 |  |
|      - | 1446 | `/*` |
|      - | 1447 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|      - | 1448 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|      - | 1449 | ` * Parameter` |
|      - | 1450 | ` *  $str1: The first string` |
|      - | 1451 | ` *  $str2: The second string` |
|      - | 1452 | ` *  $len:  The length of strings to be used in the comparison.` |
|      - | 1453 | ` * Return` |
|      - | 1454 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1455 | ` *  than str2, and 0 if they are equal.` |
|      - | 1456 | ` */` |
|      8 | 1457 | `static int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1458 |  |
|      - | 1459 | `	const char *z1,*z2;` |
|      - | 1460 | `	int res;` |
|      - | 1461 | `	int n;` |
|      9 | 1462 | `	if( nArg < 3 ){` |
|      - | 1463 | `		/* Perform a standard comparison */` |
|      5 | 1464 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 1465 | `	}` |
|      - | 1466 | `	/* Desired comparison length */` |
|      5 | 1467 | `	n  = ph7_value_to_int(apArg[2]);` |
|      5 | 1468 | `	if( n < 0 ){` |
|      - | 1469 | `		/* Invalid length */` |
|    ! 0 | 1470 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 1471 | `		return PH7_OK;` |
|      - | 1472 | `	}` |
|      - | 1473 | `	/* Perform the comparison */` |
|      5 | 1474 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|      5 | 1475 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|      5 | 1476 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 1477 | `	/* Comparison result */` |
|      5 | 1478 | `	ph7_result_int(pCtx,res);` |
|      5 | 1479 | `	return PH7_OK;` |
|      5 | 1480 |  |
|      - | 1481 | `/*` |
|      - | 1482 | ` * Implode context [i.e: it's private data].` |
|      - | 1483 | ` * A pointer to the following structure is forwarded` |
|      - | 1484 | ` * verbatim to the array walker callback defined below.` |
|      - | 1485 | ` */` |
|      - | 1486 | `struct implode_data {` |
|      - | 1487 | `	ph7_context *pCtx;    /* Call context */` |
|      - | 1488 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|      - | 1489 | `	const char *zSep;     /* Arguments separator if any */` |
|      - | 1490 | `	int nSeplen;          /* Separator length */` |
|      - | 1491 | `	int bFirst;           /* TRUE if first call */` |
|      - | 1492 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|      - | 1493 | `};` |
|      - | 1494 | `/*` |
|      - | 1495 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 1496 | ` * The following routine is invoked for each array entry passed` |
|      - | 1497 | ` * to the implode() function.` |
|      - | 1498 | ` */` |
|  88584 | 1499 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      2 | 1500 |  |
|  44292 | 1501 | `	SXUNUSED(pKey);` |
|  88586 | 1502 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 1503 | `	const char *zData;` |
|      - | 1504 | `	int nLen;` |
|  88586 | 1505 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 1506 | `		if( pData->nSeplen > 0 ){` |
|      3 | 1507 | `			if( !pData->bFirst ){` |
|      - | 1508 | `				/* append the separator first */` |
|      3 | 1509 | `				ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen);` |
|      2 | 1510 | `			}else{` |
|    ! 0 | 1511 | `				pData->bFirst = 0;` |
|      - | 1512 | `			}` |
|      1 | 1513 | `		}` |
|      - | 1514 | `		/* Recurse */` |
|      3 | 1515 | `		pData->bFirst = 1;` |
|      3 | 1516 | `		pData->nRecCount++;` |
|      3 | 1517 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 1518 | `		pData->nRecCount--;` |
|      3 | 1519 | `		return PH7_OK;` |
|      - | 1520 | `	}` |
|      - | 1521 | `	/* Extract the string representation of the entry value */` |
|  88584 | 1522 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 1523 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
|  88584 | 1524 | `	if( pData->bFirst ){` |
|  21392 | 1525 | `		pData->bFirst = 0;` |
|  77889 | 1526 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 1527 | `		/* append the separator first */` |
|  67182 | 1528 | `		ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen);` |
|  33590 | 1529 | `	}` |
|      - | 1530 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
|  88584 | 1531 | `	if( nLen > 0 ){` |
|  80530 | 1532 | `		ph7_result_string(pData->pCtx,zData,nLen);` |
|  40264 | 1533 | `	}` |
|  88584 | 1534 | `	return PH7_OK;` |
|  44294 | 1535 |  |
|      - | 1536 | `/*` |
|      - | 1537 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 1538 | ` * string implode(array $pieces,...)` |
|      - | 1539 | ` *  Join array elements with a string.` |
|      - | 1540 | ` * $glue` |
|      - | 1541 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 1542 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 1543 | ` * $pieces` |
|      - | 1544 | ` *   The array of strings to implode.` |
|      - | 1545 | ` * Return` |
|      - | 1546 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 1547 | ` *  order, with the glue string between each element.` |
|      - | 1548 | ` */` |
|  21418 | 1549 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1550 |  |
|      - | 1551 | `	struct implode_data imp_data;` |
|  21420 | 1552 | `	int i = 1;` |
|  21420 | 1553 | `	if( nArg < 1 ){` |
|      - | 1554 | `		/* Missing argument,return NULL */` |
|    ! 0 | 1555 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1556 | `		return PH7_OK;` |
|      - | 1557 | `	}` |
|      - | 1558 | `	/* Prepare the implode context */` |
|  21420 | 1559 | `	imp_data.pCtx = pCtx;` |
|  21420 | 1560 | `	imp_data.bRecursive = 0;` |
|  21420 | 1561 | `	imp_data.bFirst = 1;` |
|  21420 | 1562 | `	imp_data.nRecCount = 0;` |
|  21420 | 1563 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  21418 | 1564 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  10710 | 1565 | `	}else{` |
|      3 | 1566 | `		imp_data.zSep = 0;` |
|      3 | 1567 | `		imp_data.nSeplen = 0;` |
|      3 | 1568 | `		i = 0;` |
|      - | 1569 | `	}` |
|  21420 | 1570 | `	ph7_result_string(pCtx,"",0); /* Set an empty stirng */` |
|      - | 1571 | `	/* Start the 'join' process */` |
|  42838 | 1572 | `	while( i < nArg ){` |
|  21420 | 1573 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1574 | `			/* Iterate throw array entries */` |
|  21420 | 1575 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|  10711 | 1576 | `		}else{` |
|      - | 1577 | `			const char *zData;` |
|      - | 1578 | `			int nLen;` |
|      - | 1579 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 1580 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 1581 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 1582 | `			if( imp_data.bFirst ){` |
|    ! 0 | 1583 | `				imp_data.bFirst = 0;` |
|    ! 0 | 1584 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 1585 | `				ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen);` |
|    ! 0 | 1586 | `			}` |
|      - | 1587 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 1588 | `			if( nLen > 0 ){` |
|    ! 0 | 1589 | `				ph7_result_string(pCtx,zData,nLen);` |
|    ! 0 | 1590 | `			}` |
|      - | 1591 | `		}` |
|  21420 | 1592 | `		i++;` |
|      2 | 1593 | `	}` |
|  21420 | 1594 | `	return PH7_OK;` |
|  10711 | 1595 |  |
|      - | 1596 | `/*` |
|      - | 1597 | ` * Symisc eXtension:` |
|      - | 1598 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 1599 | ` * Purpose` |
|      - | 1600 | ` *  Same as implode() but recurse on arrays.` |
|      - | 1601 | ` * Example:` |
|      - | 1602 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 1603 | ` *   echo implode_recursive("/",$a);` |
|      - | 1604 | ` *   Will output` |
|      - | 1605 | ` *     usr/home/dean.` |
|      - | 1606 | ` *   While the standard implode would produce.` |
|      - | 1607 | ` *    usr/Array.` |
|      - | 1608 | ` * Parameter` |
|      - | 1609 | ` *  Refer to implode().` |
|      - | 1610 | ` * Return` |
|      - | 1611 | ` *  Refer to implode().` |
|      - | 1612 | ` */` |
|     12 | 1613 | `static int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1614 |  |
|      - | 1615 | `	struct implode_data imp_data;` |
|     13 | 1616 | `	int i = 1;` |
|     13 | 1617 | `	if( nArg < 1 ){` |
|      - | 1618 | `		/* Missing argument,return NULL */` |
|      3 | 1619 | `		ph7_result_null(pCtx);` |
|      3 | 1620 | `		return PH7_OK;` |
|      - | 1621 | `	}` |
|      - | 1622 | `	/* Prepare the implode context */` |
|     11 | 1623 | `	imp_data.pCtx = pCtx;` |
|     11 | 1624 | `	imp_data.bRecursive = 1;` |
|     11 | 1625 | `	imp_data.bFirst = 1;` |
|     11 | 1626 | `	imp_data.nRecCount = 0;` |
|     11 | 1627 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 1628 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 1629 | `	}else{` |
|    ! 0 | 1630 | `		imp_data.zSep = 0;` |
|    ! 0 | 1631 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 1632 | `		i = 0;` |
|      - | 1633 | `	}` |
|     11 | 1634 | `	ph7_result_string(pCtx,"",0); /* Set an empty stirng */` |
|      - | 1635 | `	/* Start the 'join' process */` |
|     21 | 1636 | `	while( i < nArg ){` |
|     11 | 1637 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1638 | `			/* Iterate throw array entries */` |
|      3 | 1639 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      2 | 1640 | `		}else{` |
|      - | 1641 | `			const char *zData;` |
|      - | 1642 | `			int nLen;` |
|      - | 1643 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 1644 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 1645 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 1646 | `			if( imp_data.bFirst ){` |
|      9 | 1647 | `				imp_data.bFirst = 0;` |
|      4 | 1648 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 1649 | `				ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen);` |
|    ! 0 | 1650 | `			}` |
|      - | 1651 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 1652 | `			if( nLen > 0 ){` |
|      9 | 1653 | `				ph7_result_string(pCtx,zData,nLen);` |
|      4 | 1654 | `			}` |
|      - | 1655 | `		}` |
|     11 | 1656 | `		i++;` |
|      1 | 1657 | `	}` |
|     11 | 1658 | `	return PH7_OK;` |
|      7 | 1659 |  |
|      - | 1660 | `/*` |
|      - | 1661 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 1662 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 1663 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 1664 | ` * Parameters` |
|      - | 1665 | ` *  $delimiter` |
|      - | 1666 | ` *   The boundary string.` |
|      - | 1667 | ` * $string` |
|      - | 1668 | ` *   The input string.` |
|      - | 1669 | ` * $limit` |
|      - | 1670 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 1671 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 1672 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 1673 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 1674 | ` * Returns` |
|      - | 1675 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 1676 | ` *  on boundaries formed by the delimiter.` |
|      - | 1677 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 1678 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 1679 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 1680 | ` *  will be returned.` |
|      - | 1681 | ` * NOTE:` |
|      - | 1682 | ` *  Negative limit is not supported.` |
|      - | 1683 | ` */` |
|   3946 | 1684 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1685 |  |
|      - | 1686 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 1687 | `	int nDelim,nStrlen,iLimit;` |
|      - | 1688 | `	ph7_value *pArray;` |
|      - | 1689 | `	ph7_value *pValue;` |
|      - | 1690 | `	sxu32 nOfft;` |
|      - | 1691 | `	sxi32 rc;` |
|   3948 | 1692 | `	if( nArg < 2 ){` |
|      - | 1693 | `		/* Missing arguments,return FALSE */` |
|      9 | 1694 | `		ph7_result_bool(pCtx,0);` |
|      9 | 1695 | `		return PH7_OK;` |
|      - | 1696 | `	}` |
|      - | 1697 | `	/* Extract the delimiter */` |
|   3940 | 1698 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   3940 | 1699 | `	if( nDelim < 1 ){` |
|      - | 1700 | `		/* Empty delimiter,return FALSE */` |
|      3 | 1701 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1702 | `		return PH7_OK;` |
|      - | 1703 | `	}` |
|      - | 1704 | `	/* Extract the string */` |
|   3938 | 1705 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   3938 | 1706 | `	if( nStrlen < 1 ){` |
|      - | 1707 | `		/* Empty string: return an array with a single empty element (PHP behavior) */` |
|      3 | 1708 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|      3 | 1709 | `		ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|      3 | 1710 | `		if( pArrayTmp == 0 \|\| pValueTmp == 0 ){` |
|      - | 1711 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 1712 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 1713 | `			return PH7_OK;` |
|      - | 1714 | `		}` |
|      3 | 1715 | `		ph7_value_string(pValueTmp, "", 0);` |
|      3 | 1716 | `		ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp);` |
|      3 | 1717 | `		ph7_result_value(pCtx, pArrayTmp);` |
|      3 | 1718 | `		return PH7_OK;` |
|      - | 1719 | `	}` |
|      - | 1720 | `	/* Point to the end of the string */` |
|   3936 | 1721 | `	zEnd = &zString[nStrlen];` |
|      - | 1722 | `	/* Create the array */` |
|   3936 | 1723 | `	pArray =  ph7_context_new_array(pCtx);` |
|   3936 | 1724 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   3936 | 1725 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 1726 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 1727 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1728 | `		return PH7_OK;` |
|      - | 1729 | `	}` |
|      - | 1730 | `	/* Set a defualt limit */` |
|   3936 | 1731 | `	iLimit = SXI32_HIGH;` |
|   3936 | 1732 | `	if( nArg > 2 ){` |
|      9 | 1733 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|      9 | 1734 | `		 if( iLimit < 0 ){` |
|      3 | 1735 | `			iLimit = -iLimit;` |
|      1 | 1736 | `		}` |
|      9 | 1737 | `		if( iLimit == 0 ){` |
|      3 | 1738 | `			iLimit = 1;` |
|      1 | 1739 | `		}` |
|      9 | 1740 | `		iLimit--;` |
|      4 | 1741 | `	}` |
|      - | 1742 | `	/* Start exploding */` |
|  44606 | 1743 | `	for(;;){` |
|  89214 | 1744 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
|  89214 | 1745 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 1746 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   3936 | 1747 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   3936 | 1748 | `			ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|   3936 | 1749 | `			break;` |
|      - | 1750 | `		}` |
|      - | 1751 | `		/* Point to the desired offset */` |
|  85280 | 1752 | `		zCur = &zString[nOfft];` |
|      - | 1753 | `		/* Perform the store operation (may be empty) */` |
|  85280 | 1754 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|  85280 | 1755 | `		ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue);` |
|      - | 1756 | `		/* Point beyond the delimiter */` |
|  85280 | 1757 | `		zString = &zCur[nDelim];` |
|      - | 1758 | `		/* Reset the cursor */` |
|  85280 | 1759 | `		ph7_value_reset_string_cursor(pValue);` |
|      2 | 1760 | `	}` |
|      - | 1761 | `	/* Return the freshly created array */` |
|   3936 | 1762 | `	ph7_result_value(pCtx,pArray);` |
|      - | 1763 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 1764 | `	 * released as soon we return from this foregin function.` |
|      - | 1765 | `	 */` |
|   3936 | 1766 | `	return PH7_OK;` |
|   1975 | 1767 |  |
|      - | 1768 | `/*` |
|      - | 1769 | ` * string trim(string $str[,string $charlist ])` |
|      - | 1770 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 1771 | ` * Parameters` |
|      - | 1772 | ` *  $str` |
|      - | 1773 | ` *   The string that will be trimmed.` |
|      - | 1774 | ` * $charlist` |
|      - | 1775 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1776 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1777 | ` *   With .. you can specify a range of characters.` |
|      - | 1778 | ` * Returns.` |
|      - | 1779 | ` *  Thr processed string.` |
|      - | 1780 | ` * NOTE:` |
|      - | 1781 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 1782 | ` */` |
|   9438 | 1783 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1784 |  |
|      - | 1785 | `	const char *zString;` |
|      - | 1786 | `	int nLen;` |
|   9440 | 1787 | `	if( nArg < 1 ){` |
|      - | 1788 | `		/* Missing arguments,return null */` |
|      3 | 1789 | `		ph7_result_null(pCtx);` |
|      3 | 1790 | `		return PH7_OK;` |
|      - | 1791 | `	}` |
|      - | 1792 | `	/* Extract the target string */` |
|   9438 | 1793 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|   9438 | 1794 | `	if( nLen < 1 ){` |
|      - | 1795 | `		/* Empty string,return */` |
|   1598 | 1796 | `		ph7_result_string(pCtx,"",0);` |
|   1598 | 1797 | `		return PH7_OK;` |
|      - | 1798 | `	}` |
|      - | 1799 | `	/* Start the trim process */` |
|   7842 | 1800 | `	if( nArg < 2 ){` |
|      - | 1801 | `		SyString sStr;` |
|      - | 1802 | `		/* Remove white spaces and NUL bytes */` |
|   7838 | 1803 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  18894 | 1804 | `		SyStringFullTrimSafe(&sStr);` |
|   7838 | 1805 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   3920 | 1806 | `	}else{` |
|      - | 1807 | `		/* Char list */` |
|      - | 1808 | `		const char *zList;` |
|      - | 1809 | `		int nListlen;` |
|      5 | 1810 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      5 | 1811 | `		if( nListlen < 1 ){` |
|      - | 1812 | `			/* Return the string unchanged */` |
|      3 | 1813 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 1814 | `		}else{` |
|      3 | 1815 | `			const char *zEnd = &zString[nLen];` |
|      3 | 1816 | `			const char *zCur = zString;` |
|      - | 1817 | `			const char *zPtr;` |
|      - | 1818 | `			int i;` |
|      - | 1819 | `			/* Left trim */` |
|      4 | 1820 | `			for(;;){` |
|      9 | 1821 | `				if( zCur >= zEnd ){` |
|    ! 0 | 1822 | `					break;` |
|      - | 1823 | `				}` |
|      9 | 1824 | `				zPtr = zCur;` |
|     17 | 1825 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|      9 | 1826 | `					if( zCur < zEnd && zCur[0] == zList[i] ){` |
|      7 | 1827 | `						zCur++;` |
|      3 | 1828 | `					}` |
|      5 | 1829 | `				}` |
|      9 | 1830 | `				if( zCur == zPtr ){` |
|      - | 1831 | `					/* No match,break immediately */` |
|      3 | 1832 | `					break;` |
|      - | 1833 | `				}` |
|      1 | 1834 | `			}` |
|      - | 1835 | `			/* Right trim */` |
|      3 | 1836 | `			zEnd--;` |
|      4 | 1837 | `			for(;;){` |
|      9 | 1838 | `				if( zEnd <= zCur ){` |
|    ! 0 | 1839 | `					break;` |
|      - | 1840 | `				}` |
|      9 | 1841 | `				zPtr = zEnd;` |
|     17 | 1842 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|      9 | 1843 | `					if( zEnd > zCur && zEnd[0] == zList[i] ){` |
|      7 | 1844 | `						zEnd--;` |
|      3 | 1845 | `					}` |
|      5 | 1846 | `				}` |
|      9 | 1847 | `				if( zEnd == zPtr ){` |
|      3 | 1848 | `					break;` |
|      - | 1849 | `				}` |
|      1 | 1850 | `			}` |
|      3 | 1851 | `			if( zCur >= zEnd ){` |
|      - | 1852 | `				/* Return the empty string */` |
|    ! 0 | 1853 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1854 | `			}else{` |
|      3 | 1855 | `				zEnd++;` |
|      3 | 1856 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 1857 | `			}` |
|      - | 1858 | `		}` |
|      - | 1859 | `	}` |
|   7842 | 1860 | `	return PH7_OK;` |
|   4721 | 1861 |  |
|      - | 1862 | `/*` |
|      - | 1863 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 1864 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 1865 | ` * Parameters` |
|      - | 1866 | ` *  $str` |
|      - | 1867 | ` *   The string that will be trimmed.` |
|      - | 1868 | ` * $charlist` |
|      - | 1869 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1870 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1871 | ` *   With .. you can specify a range of characters.` |
|      - | 1872 | ` * Returns.` |
|      - | 1873 | ` *  Thr processed string.` |
|      - | 1874 | ` * NOTE:` |
|      - | 1875 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 1876 | ` */` |
|     26 | 1877 | `static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1878 |  |
|      - | 1879 | `	const char *zString;` |
|      - | 1880 | `	int nLen;` |
|     27 | 1881 | `	if( nArg < 1 ){` |
|      - | 1882 | `		/* Missing arguments,return null */` |
|      3 | 1883 | `		ph7_result_null(pCtx);` |
|      3 | 1884 | `		return PH7_OK;` |
|      - | 1885 | `	}` |
|      - | 1886 | `	/* Extract the target string */` |
|     25 | 1887 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 1888 | `	if( nLen < 1 ){` |
|      - | 1889 | `		/* Empty string,return */` |
|      5 | 1890 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 1891 | `		return PH7_OK;` |
|      - | 1892 | `	}` |
|      - | 1893 | `	/* Start the trim process */` |
|     21 | 1894 | `	if( nArg < 2 ){` |
|      - | 1895 | `		SyString sStr;` |
|      - | 1896 | `		/* Remove white spaces and NUL bytes*/` |
|     17 | 1897 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     41 | 1898 | `		SyStringRightTrimSafe(&sStr);` |
|     17 | 1899 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      9 | 1900 | `	}else{` |
|      - | 1901 | `		/* Char list */` |
|      - | 1902 | `		const char *zList;` |
|      - | 1903 | `		int nListlen;` |
|      5 | 1904 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      5 | 1905 | `		if( nListlen < 1 ){` |
|      - | 1906 | `			/* Return the string unchanged */` |
|    ! 0 | 1907 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 1908 | `		}else{` |
|      5 | 1909 | `			const char *zEnd = &zString[nLen - 1];` |
|      5 | 1910 | `			const char *zCur = zString;` |
|      - | 1911 | `			const char *zPtr;` |
|      - | 1912 | `			int i;` |
|      - | 1913 | `			/* Right trim */` |
|      6 | 1914 | `			for(;;){` |
|     13 | 1915 | `				if( zEnd <= zCur ){` |
|    ! 0 | 1916 | `					break;` |
|      - | 1917 | `				}` |
|     13 | 1918 | `				zPtr = zEnd;` |
|     25 | 1919 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|     13 | 1920 | `					if( zEnd > zCur && zEnd[0] == zList[i] ){` |
|      9 | 1921 | `						zEnd--;` |
|      4 | 1922 | `					}` |
|      7 | 1923 | `				}` |
|     13 | 1924 | `				if( zEnd == zPtr ){` |
|      5 | 1925 | `					break;` |
|      - | 1926 | `				}` |
|      1 | 1927 | `			}` |
|      5 | 1928 | `			if( zEnd <= zCur ){` |
|      - | 1929 | `				/* Return the empty string */` |
|    ! 0 | 1930 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1931 | `			}else{` |
|      5 | 1932 | `				zEnd++;` |
|      5 | 1933 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 1934 | `			}` |
|      - | 1935 | `		}` |
|      - | 1936 | `	}` |
|     21 | 1937 | `	return PH7_OK;` |
|     14 | 1938 |  |
|      - | 1939 | `/*` |
|      - | 1940 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 1941 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 1942 | ` * Parameters` |
|      - | 1943 | ` *  $str` |
|      - | 1944 | ` *   The string that will be trimmed.` |
|      - | 1945 | ` * $charlist` |
|      - | 1946 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1947 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1948 | ` *   With .. you can specify a range of characters.` |
|      - | 1949 | ` * Returns.` |
|      - | 1950 | ` *  Thr processed string.` |
|      - | 1951 | ` * NOTE:` |
|      - | 1952 | ` *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.` |
|      - | 1953 | ` */` |
|     12 | 1954 | `static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1955 |  |
|      - | 1956 | `	const char *zString;` |
|      - | 1957 | `	int nLen;` |
|     13 | 1958 | `	if( nArg < 1 ){` |
|      - | 1959 | `		/* Missing arguments,return null */` |
|      3 | 1960 | `		ph7_result_null(pCtx);` |
|      3 | 1961 | `		return PH7_OK;` |
|      - | 1962 | `	}` |
|      - | 1963 | `	/* Extract the target string */` |
|     11 | 1964 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 1965 | `	if( nLen < 1 ){` |
|      - | 1966 | `		/* Empty string,return */` |
|    ! 0 | 1967 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1968 | `		return PH7_OK;` |
|      - | 1969 | `	}` |
|      - | 1970 | `	/* Start the trim process */` |
|     11 | 1971 | `	if( nArg < 2 ){` |
|      - | 1972 | `		SyString sStr;` |
|      - | 1973 | `		/* Remove white spaces and NUL byte */` |
|      3 | 1974 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|      8 | 1975 | `		SyStringLeftTrimSafe(&sStr);` |
|      3 | 1976 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      2 | 1977 | `	}else{` |
|      - | 1978 | `		/* Char list */` |
|      - | 1979 | `		const char *zList;` |
|      - | 1980 | `		int nListlen;` |
|      9 | 1981 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      9 | 1982 | `		if( nListlen < 1 ){` |
|      - | 1983 | `			/* Return the string unchanged */` |
|      3 | 1984 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 1985 | `		}else{` |
|      7 | 1986 | `			const char *zEnd = &zString[nLen];` |
|      7 | 1987 | `			const char *zCur = zString;` |
|      - | 1988 | `			const char *zPtr;` |
|      - | 1989 | `			int i;` |
|      - | 1990 | `			/* Left trim */` |
|      7 | 1991 | `			for(;;){` |
|     15 | 1992 | `				if( zCur >= zEnd ){` |
|    ! 0 | 1993 | `					break;` |
|      - | 1994 | `				}` |
|     15 | 1995 | `				zPtr = zCur;` |
|     41 | 1996 | `				for( i = 0 ; i < nListlen ; i++ ){` |
|     27 | 1997 | `					if( zCur < zEnd && zCur[0] == zList[i] ){` |
|     13 | 1998 | `						zCur++;` |
|      6 | 1999 | `					}` |
|     14 | 2000 | `				}` |
|     15 | 2001 | `				if( zCur == zPtr ){` |
|      - | 2002 | `					/* No match,break immediately */` |
|      7 | 2003 | `					break;` |
|      - | 2004 | `				}` |
|      1 | 2005 | `			}` |
|      7 | 2006 | `			if( zCur >= zEnd ){` |
|      - | 2007 | `				/* Return the empty string */` |
|    ! 0 | 2008 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2009 | `			}else{` |
|      7 | 2010 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2011 | `			}` |
|      - | 2012 | `		}` |
|      - | 2013 | `	}` |
|     11 | 2014 | `	return PH7_OK;` |
|      7 | 2015 |  |
|      - | 2016 | `/*` |
|      - | 2017 | ` * string strtolower(string $str)` |
|      - | 2018 | ` *  Make a string lowercase.` |
|      - | 2019 | ` * Parameters` |
|      - | 2020 | ` *  $str` |
|      - | 2021 | ` *   The input string.` |
|      - | 2022 | ` * Returns.` |
|      - | 2023 | ` *  The lowercased string.` |
|      - | 2024 | ` */` |
|  21176 | 2025 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2026 |  |
|      - | 2027 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2028 | `	int nLen;` |
|  21178 | 2029 | `	if( nArg < 1 ){` |
|      - | 2030 | `		/* Missing arguments,return null */` |
|      3 | 2031 | `		ph7_result_null(pCtx);` |
|      3 | 2032 | `		return PH7_OK;` |
|      - | 2033 | `	}` |
|      - | 2034 | `	/* Extract the target string */` |
|  21176 | 2035 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  21176 | 2036 | `	if( nLen < 1 ){` |
|      - | 2037 | `		/* Empty string,return */` |
|      3 | 2038 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2039 | `		return PH7_OK;` |
|      - | 2040 | `	}` |
|      - | 2041 | `	/* Perform the requested operation */` |
|  21174 | 2042 | `	zEnd = &zString[nLen];` |
|  66860 | 2043 | `	for(;;){` |
| 133722 | 2044 | `		if( zString >= zEnd ){` |
|      - | 2045 | `			/* No more input,break immediately */` |
|  21174 | 2046 | `			break;` |
|      - | 2047 | `		}` |
| 112550 | 2048 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2049 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2050 | `			zCur = zString;` |
|    ! 0 | 2051 | `			zString++;` |
|    ! 0 | 2052 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2053 | `				zString++;` |
|    ! 0 | 2054 | `			}` |
|      - | 2055 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2056 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2057 | `		}else{` |
| 112550 | 2058 | `			int c = zString[0];` |
| 112550 | 2059 | `			if( SyisUpper(c) ){` |
| 112548 | 2060 | `				c = SyToLower(zString[0]);` |
|  56273 | 2061 | `			}` |
|      - | 2062 | `			/* Append character */` |
| 112550 | 2063 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2064 | `			/* Advance the cursor */` |
| 112550 | 2065 | `			zString++;` |
|      - | 2066 | `		}` |
|      2 | 2067 | `	}` |
|  21174 | 2068 | `	return PH7_OK;` |
|  10590 | 2069 |  |
|      - | 2070 | `/*` |
|      - | 2071 | ` * string strtolower(string $str)` |
|      - | 2072 | ` *  Make a string uppercase.` |
|      - | 2073 | ` * Parameters` |
|      - | 2074 | ` *  $str` |
|      - | 2075 | ` *   The input string.` |
|      - | 2076 | ` * Returns.` |
|      - | 2077 | ` *  The uppercased string.` |
|      - | 2078 | ` */` |
|     14 | 2079 | `static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2080 |  |
|      - | 2081 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2082 | `	int nLen;` |
|     15 | 2083 | `	if( nArg < 1 ){` |
|      - | 2084 | `		/* Missing arguments,return null */` |
|      3 | 2085 | `		ph7_result_null(pCtx);` |
|      3 | 2086 | `		return PH7_OK;` |
|      - | 2087 | `	}` |
|      - | 2088 | `	/* Extract the target string */` |
|     13 | 2089 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 2090 | `	if( nLen < 1 ){` |
|      - | 2091 | `		/* Empty string,return */` |
|      3 | 2092 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2093 | `		return PH7_OK;` |
|      - | 2094 | `	}` |
|      - | 2095 | `	/* Perform the requested operation */` |
|     11 | 2096 | `	zEnd = &zString[nLen];` |
|     31 | 2097 | `	for(;;){` |
|     63 | 2098 | `		if( zString >= zEnd ){` |
|      - | 2099 | `			/* No more input,break immediately */` |
|     11 | 2100 | `			break;` |
|      - | 2101 | `		}` |
|     53 | 2102 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2103 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2104 | `			zCur = zString;` |
|    ! 0 | 2105 | `			zString++;` |
|    ! 0 | 2106 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2107 | `				zString++;` |
|    ! 0 | 2108 | `			}` |
|      - | 2109 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2110 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2111 | `		}else{` |
|     53 | 2112 | `			int c = zString[0];` |
|     53 | 2113 | `			if( SyisLower(c) ){` |
|     47 | 2114 | `				c = SyToUpper(zString[0]);` |
|     23 | 2115 | `			}` |
|      - | 2116 | `			/* Append character */` |
|     53 | 2117 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2118 | `			/* Advance the cursor */` |
|     53 | 2119 | `			zString++;` |
|      - | 2120 | `		}` |
|      1 | 2121 | `	}` |
|     11 | 2122 | `	return PH7_OK;` |
|      8 | 2123 |  |
|      - | 2124 | `/*` |
|      - | 2125 | ` * string ucfirst(string $str)` |
|      - | 2126 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 2127 | ` *  character is alphabetic.` |
|      - | 2128 | ` * Parameters` |
|      - | 2129 | ` *  $str` |
|      - | 2130 | ` *   The input string.` |
|      - | 2131 | ` * Returns.` |
|      - | 2132 | ` *  The processed string.` |
|      - | 2133 | ` */` |
|      6 | 2134 | `static int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2135 |  |
|      - | 2136 | `	const char *zString,*zEnd;` |
|      - | 2137 | `	int nLen,c;` |
|      7 | 2138 | `	if( nArg < 1 ){` |
|      - | 2139 | `		/* Missing arguments,return null */` |
|      3 | 2140 | `		ph7_result_null(pCtx);` |
|      3 | 2141 | `		return PH7_OK;` |
|      - | 2142 | `	}` |
|      - | 2143 | `	/* Extract the target string */` |
|      5 | 2144 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2145 | `	if( nLen < 1 ){` |
|      - | 2146 | `		/* Empty string,return */` |
|      3 | 2147 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2148 | `		return PH7_OK;` |
|      - | 2149 | `	}` |
|      - | 2150 | `	/* Perform the requested operation */` |
|      3 | 2151 | `	zEnd = &zString[nLen];` |
|      3 | 2152 | `	c = zString[0];` |
|      3 | 2153 | `	if( SyisLower(c) ){` |
|      3 | 2154 | `		c = SyToUpper(c);` |
|      1 | 2155 | `	}` |
|      - | 2156 | `	/* Append the first character */` |
|      3 | 2157 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2158 | `	zString++;` |
|      3 | 2159 | `	if( zString < zEnd ){` |
|      - | 2160 | `		/* Append the rest of the input verbatim */` |
|      3 | 2161 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2162 | `	}` |
|      3 | 2163 | `	return PH7_OK;` |
|      4 | 2164 |  |
|      - | 2165 | `/*` |
|      - | 2166 | ` * string lcfirst(string $str)` |
|      - | 2167 | ` *  Make a string's first character lowercase.` |
|      - | 2168 | ` * Parameters` |
|      - | 2169 | ` *  $str` |
|      - | 2170 | ` *   The input string.` |
|      - | 2171 | ` * Returns.` |
|      - | 2172 | ` *  The processed string.` |
|      - | 2173 | ` */` |
|      6 | 2174 | `static int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2175 |  |
|      - | 2176 | `	const char *zString,*zEnd;` |
|      - | 2177 | `	int nLen,c;` |
|      7 | 2178 | `	if( nArg < 1 ){` |
|      - | 2179 | `		/* Missing arguments,return null */` |
|      3 | 2180 | `		ph7_result_null(pCtx);` |
|      3 | 2181 | `		return PH7_OK;` |
|      - | 2182 | `	}` |
|      - | 2183 | `	/* Extract the target string */` |
|      5 | 2184 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2185 | `	if( nLen < 1 ){` |
|      - | 2186 | `		/* Empty string,return */` |
|      3 | 2187 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2188 | `		return PH7_OK;` |
|      - | 2189 | `	}` |
|      - | 2190 | `	/* Perform the requested operation */` |
|      3 | 2191 | `	zEnd = &zString[nLen];` |
|      3 | 2192 | `	c = zString[0];` |
|      3 | 2193 | `	if( SyisUpper(c) ){` |
|      3 | 2194 | `		c = SyToLower(c);` |
|      1 | 2195 | `	}` |
|      - | 2196 | `	/* Append the first character */` |
|      3 | 2197 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2198 | `	zString++;` |
|      3 | 2199 | `	if( zString < zEnd ){` |
|      - | 2200 | `		/* Append the rest of the input verbatim */` |
|      3 | 2201 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2202 | `	}` |
|      3 | 2203 | `	return PH7_OK;` |
|      4 | 2204 |  |
|      - | 2205 | `/*` |
|      - | 2206 | ` * int ord(string $string)` |
|      - | 2207 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 2208 | ` *  Passing null, an empty string, or a multi-byte string emits` |
|      - | 2209 | ` *  E_DEPRECATED to match PHP 8.4+ behaviour.` |
|      - | 2210 | ` * Parameters` |
|      - | 2211 | ` *  $string` |
|      - | 2212 | ` *   The input string.` |
|      - | 2213 | ` * Returns` |
|      - | 2214 | ` *  The ASCII value as an integer.` |
|      - | 2215 | ` */` |
|     62 | 2216 | `static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2217 |  |
|      - | 2218 | `	const char *zString;` |
|      - | 2219 | `	int nLen,c;` |
|      - | 2220 | `	/* PHP requires exactly one argument. */` |
|     64 | 2221 | `	if( nArg != 1 ){` |
|      7 | 2222 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2223 | `			"ArgumentCountError",` |
|      - | 2224 | `			"ord() expects exactly 1 argument, %d given",` |
|      2 | 2225 | `			nArg` |
|      - | 2226 | `			);` |
|      - | 2227 | `	}` |
|      - | 2228 | `	/* Passing null is deprecated (E_DEPRECATED).  PHP emits this before` |
|      - | 2229 | `	 * the empty-string deprecation, so we check null first. */` |
|     59 | 2230 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 2231 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2232 | `			"ord(): Passing null to parameter #1 ($character) "` |
|      - | 2233 | `			"of type string is deprecated"` |
|      - | 2234 | `			);` |
|      1 | 2235 | `	}` |
|      - | 2236 | `	/* Extract the target string */` |
|     59 | 2237 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     59 | 2238 | `	if( nLen < 1 ){` |
|      - | 2239 | `		/* Empty string is deprecated (E_DEPRECATED). */` |
|      5 | 2240 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2241 | `			"ord(): Providing an empty string is deprecated"` |
|      - | 2242 | `			);` |
|      5 | 2243 | `		ph7_result_int(pCtx,0);` |
|      5 | 2244 | `		return PH7_OK;` |
|      - | 2245 | `	}` |
|      - | 2246 | `	/* A string longer than one byte is deprecated (E_DEPRECATED). */` |
|     55 | 2247 | `	if( nLen > 1 ){` |
|      7 | 2248 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2249 | `			"ord(): Providing a string that is not one byte long "` |
|      - | 2250 | `			"is deprecated. Use ord($str[0]) instead"` |
|      - | 2251 | `			);` |
|      3 | 2252 | `	}` |
|      - | 2253 | `	/* Extract the ASCII value of the first character */` |
|     55 | 2254 | `	c = (unsigned char)zString[0];` |
|      - | 2255 | `	/* Return that value */` |
|     55 | 2256 | `	ph7_result_int(pCtx,c);` |
|     55 | 2257 | `	return PH7_OK;` |
|     33 | 2258 |  |
|      - | 2259 | `/*` |
|      - | 2260 | ` * string chr(int $codepoint)` |
|      - | 2261 | ` *  Returns a one-character string containing the character specified` |
|      - | 2262 | ` *  by the given codepoint.  Any integer is accepted; values outside` |
|      - | 2263 | ` *  the [0, 255] range emit an E_DEPRECATED and are masked with & 0xFF.` |
|      - | 2264 | ` * Parameters` |
|      - | 2265 | ` *  $codepoint` |
|      - | 2266 | ` *   An integer codepoint.  Values outside 0-255 are deprecated and` |
|      - | 2267 | ` *   will be constrained to a single byte.` |
|      - | 2268 | ` * Returns` |
|      - | 2269 | ` *  A single-character string.` |
|      - | 2270 | ` */` |
|     44 | 2271 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2272 |  |
|      - | 2273 | `	int c;` |
|      - | 2274 | `	unsigned char ch;` |
|      - | 2275 | `	/* PHP requires exactly one argument. */` |
|     46 | 2276 | `	if( nArg != 1 ){` |
|      7 | 2277 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2278 | `			"ArgumentCountError",` |
|      - | 2279 | `			"chr() expects exactly 1 argument, %d given",` |
|      2 | 2280 | `			nArg` |
|      - | 2281 | `			);` |
|      - | 2282 | `	}` |
|      - | 2283 | `	/* Implicit float-to-int conversion loses precision (E_DEPRECATED).` |
|      - | 2284 | `	 * PHP does not prefix this message with "chr():", so we call` |
|      - | 2285 | `	 * PH7_VmThrowError() with a NULL function name to avoid the` |
|      - | 2286 | `	 * automatic prefix that ph7_context_throw_error*() would add. */` |
|     41 | 2287 | `	if( ph7_value_is_float(apArg[0]) ){` |
|      - | 2288 | `		char zBuf[120];` |
|      4 | 2289 | `		SyBufferFormat(zBuf,sizeof(zBuf),` |
|      - | 2290 | `			"Implicit conversion from float %g to int loses precision",` |
|      1 | 2291 | `			ph7_value_to_double(apArg[0])` |
|      - | 2292 | `			);` |
|      3 | 2293 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zBuf);` |
|      1 | 2294 | `	}` |
|      - | 2295 | `	/* Extract the codepoint. */` |
|     41 | 2296 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 2297 | `	/* Out-of-range codepoint (E_DEPRECATED), then mask to a single byte.` |
|      - | 2298 | `	 * PHP includes "chr(): " in the $errstr passed to set_error_handler,` |
|      - | 2299 | `	 * so we embed the prefix in the message and pass NULL as the function` |
|      - | 2300 | `	 * name to avoid the API double-prefixing it. */` |
|     41 | 2301 | `	if( c < 0 \|\| c > 255 ){` |
|      5 | 2302 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 2303 | `			E_DEPRECATED,` |
|      - | 2304 | `			"chr(): Providing a value not in-between 0 and 255 is deprecated, "` |
|      - | 2305 | `			"this is because a byte value must be in the [0, 255] interval. "` |
|      - | 2306 | `			"The value used will be constrained using % 256"` |
|      - | 2307 | `			);` |
|      2 | 2308 | `	}` |
|      - | 2309 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|      - | 2310 | `	 * when taking the address of a wider int. */` |
|     41 | 2311 | `	ch = (unsigned char)(c & 0xFF);` |
|      - | 2312 | `	/* Return the specified character */` |
|     41 | 2313 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|     41 | 2314 | `	return PH7_OK;` |
|     24 | 2315 |  |
|      - | 2316 | `/*` |
|      - | 2317 | ` * Binary to hex consumer callback.` |
|      - | 2318 | ` * This callback is the default consumer used by the hash functions` |
|      - | 2319 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 2320 | ` */` |
|    226 | 2321 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      1 | 2322 |  |
|      - | 2323 | `	/* Append hex chunk verbatim */` |
|    227 | 2324 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|    227 | 2325 | `	return SXRET_OK;` |
|      1 | 2326 |  |
|      - | 2327 |  |
|      - | 2328 | `/*` |
|      - | 2329 | ` * string bin2hex(string $str)` |
|      - | 2330 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 2331 | ` * Parameters` |
|      - | 2332 | ` *  $str` |
|      - | 2333 | ` *   The input string.` |
|      - | 2334 | ` * Returns.` |
|      - | 2335 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 2336 | ` */` |
|     12 | 2337 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2338 |  |
|      - | 2339 | `	const char *zString;` |
|      - | 2340 | `	int nLen;` |
|     13 | 2341 | `	if( nArg < 1 ){` |
|      - | 2342 | `		/* Missing arguments,return null */` |
|      3 | 2343 | `		ph7_result_null(pCtx);` |
|      3 | 2344 | `		return PH7_OK;` |
|      - | 2345 | `	}` |
|      - | 2346 | `	/* Extract the target string */` |
|     11 | 2347 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 2348 | `	if( nLen < 1 ){` |
|      - | 2349 | `		/* Empty string,return */` |
|      3 | 2350 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2351 | `		return PH7_OK;` |
|      - | 2352 | `	}` |
|      - | 2353 | `	/* Perform the requested operation */` |
|      9 | 2354 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|      9 | 2355 | `	return PH7_OK;` |
|      7 | 2356 |  |
|      - | 2357 |  |
|      - | 2358 | `/* Search callback signature */` |
|      - | 2359 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 2360 | `/*` |
|      - | 2361 | ` * Case-insensitive pattern match.` |
|      - | 2362 | ` * Brute force is the default search method used here.` |
|      - | 2363 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 2364 | ` * well for short/medium texts on modern hardware.` |
|      - | 2365 | ` */` |
|    118 | 2366 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      1 | 2367 |  |
|    119 | 2368 | `	const char *zpIn = (const char *)pPattern;` |
|    119 | 2369 | `	const char *zIn = (const char *)pText;` |
|    119 | 2370 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    119 | 2371 | `	const char *zEnd = &zIn[nLen];` |
|      - | 2372 | `	const char *zPtr,*zPtr2;` |
|      - | 2373 | `	int c,d;` |
|    119 | 2374 | `	if( iPatLen > nLen ){` |
|      - | 2375 | `		/* Don't bother processing */` |
|     33 | 2376 | `		return SXERR_NOTFOUND;` |
|      - | 2377 | `	}` |
|    244 | 2378 | `	for(;;){` |
|    489 | 2379 | `		if( zIn >= zEnd ){` |
|     47 | 2380 | `			break;` |
|      - | 2381 | `		}` |
|    443 | 2382 | `		c = SyToLower(zIn[0]);` |
|    443 | 2383 | `		d = SyToLower(zpIn[0]);` |
|    443 | 2384 | `		if( c == d ){` |
|     41 | 2385 | `			zPtr   = &zIn[1];` |
|     41 | 2386 | `			zPtr2  = &zpIn[1];` |
|     71 | 2387 | `			for(;;){` |
|    143 | 2388 | `				if( zPtr2 >= zpEnd ){` |
|      - | 2389 | `					/* Pattern found */` |
|     41 | 2390 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 2391 | `					return SXRET_OK;` |
|      - | 2392 | `				}` |
|    103 | 2393 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 2394 | `					break;` |
|      - | 2395 | `				}` |
|    103 | 2396 | `				c = SyToLower(zPtr[0]);` |
|    103 | 2397 | `				d = SyToLower(zPtr2[0]);` |
|    103 | 2398 | `				if( c != d ){` |
|    ! 0 | 2399 | `					break;` |
|      - | 2400 | `				}` |
|    103 | 2401 | `				zPtr++; zPtr2++;` |
|      1 | 2402 | `			}` |
|    ! 0 | 2403 | `		}` |
|    403 | 2404 | `		zIn++;` |
|      1 | 2405 | `	}` |
|      - | 2406 | `	/* Pattern not found */` |
|     47 | 2407 | `	return SXERR_NOTFOUND;` |
|     60 | 2408 |  |
|      - | 2409 | `/*` |
|      - | 2410 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2411 | ` *  Find the first occurrence of a string.` |
|      - | 2412 | ` * Parameters` |
|      - | 2413 | ` *  $haystack` |
|      - | 2414 | ` *   The input string.` |
|      - | 2415 | ` * $needle` |
|      - | 2416 | ` *   Search pattern (must be a string).` |
|      - | 2417 | ` * $before_needle` |
|      - | 2418 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2419 | ` *   of the needle (excluding the needle).` |
|      - | 2420 | ` * Return` |
|      - | 2421 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2422 | ` */` |
|     10 | 2423 | `static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2424 |  |
|     11 | 2425 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2426 | `	const char *zBlob,*zPattern;` |
|      - | 2427 | `	int nLen,nPatLen;` |
|      - | 2428 | `	sxu32 nOfft;` |
|      - | 2429 | `	sxi32 rc;` |
|     11 | 2430 | `	if( nArg < 2 ){` |
|      - | 2431 | `		/* Missing arguments,return FALSE */` |
|      5 | 2432 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2433 | `		return PH7_OK;` |
|      - | 2434 | `	}` |
|      - | 2435 | `	/* Extract the needle and the haystack */` |
|      7 | 2436 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 2437 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 2438 | `	nOfft = 0; /* cc warning */` |
|      9 | 2439 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 2440 | `		int before = 0;` |
|      - | 2441 | `		/* Perform the lookup */` |
|      5 | 2442 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 2443 | `		if( rc != SXRET_OK ){` |
|      - | 2444 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 2445 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2446 | `			return PH7_OK;` |
|      - | 2447 | `		}` |
|      - | 2448 | `		/* Return the portion of the string */` |
|      5 | 2449 | `		if( nArg > 2 ){` |
|      3 | 2450 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 2451 | `		}` |
|      5 | 2452 | `		if( before ){` |
|      3 | 2453 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 2454 | `		}else{` |
|      3 | 2455 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2456 | `		}` |
|      3 | 2457 | `	}else{` |
|      3 | 2458 | `		ph7_result_bool(pCtx,0);` |
|      - | 2459 | `	}` |
|      7 | 2460 | `	return PH7_OK;` |
|      6 | 2461 |  |
|      - | 2462 | `/*` |
|      - | 2463 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2464 | ` *  Case-insensitive strstr().` |
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
|      6 | 2476 | `static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2477 |  |
|      7 | 2478 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2479 | `	const char *zBlob,*zPattern;` |
|      - | 2480 | `	int nLen,nPatLen;` |
|      - | 2481 | `	sxu32 nOfft;` |
|      - | 2482 | `	sxi32 rc;` |
|      7 | 2483 | `	if( nArg < 2 ){` |
|      - | 2484 | `		/* Missing arguments,return FALSE */` |
|      3 | 2485 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2486 | `		return PH7_OK;` |
|      - | 2487 | `	}` |
|      - | 2488 | `	/* Extract the needle and the haystack */` |
|      5 | 2489 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2490 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      5 | 2491 | `	nOfft = 0; /* cc warning */` |
|      7 | 2492 | `	if( nLen > 0 && nPatLen > 0 ){` |
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
|    ! 0 | 2511 | `		ph7_result_bool(pCtx,0);` |
|      - | 2512 | `	}` |
|      5 | 2513 | `	return PH7_OK;` |
|      4 | 2514 |  |
|      - | 2515 | `/*` |
|      - | 2516 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2517 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 2518 | ` * Parameters` |
|      - | 2519 | ` *  $haystack` |
|      - | 2520 | ` *   The input string.` |
|      - | 2521 | ` * $needle` |
|      - | 2522 | ` *   Search pattern (must be a string).` |
|      - | 2523 | ` * $offset` |
|      - | 2524 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 2525 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 2526 | ` *   of haystack.` |
|      - | 2527 | ` * Return` |
|      - | 2528 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 2529 | ` */` |
|     80 | 2530 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2531 |  |
|     82 | 2532 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2533 | `	const char *zBlob,*zPattern;` |
|      - | 2534 | `	int nLen,nPatLen,nStart;` |
|      - | 2535 | `	sxu32 nOfft;` |
|      - | 2536 | `	sxi32 rc;` |
|     82 | 2537 | `	if( nArg < 2 ){` |
|      - | 2538 | `		/* Missing arguments,return FALSE */` |
|      7 | 2539 | `		ph7_result_bool(pCtx,0);` |
|      7 | 2540 | `		return PH7_OK;` |
|      - | 2541 | `	}` |
|      - | 2542 | `	/* Extract the needle and the haystack */` |
|     76 | 2543 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     76 | 2544 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     76 | 2545 | `	nOfft = 0; /* cc warning */` |
|     76 | 2546 | `	nStart = 0;` |
|      - | 2547 | `	/* Peek the starting offset if available */` |
|     76 | 2548 | `	if( nArg > 2 ){` |
|    ! 0 | 2549 | `		nStart = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 2550 | `		if( nStart < 0 ){` |
|    ! 0 | 2551 | `			nStart = -nStart;` |
|    ! 0 | 2552 | `		}` |
|    ! 0 | 2553 | `		if( nStart >= nLen ){` |
|      - | 2554 | `			/* Invalid offset */` |
|    ! 0 | 2555 | `			nStart = 0;` |
|    ! 0 | 2556 | `		}else{` |
|    ! 0 | 2557 | `			zBlob += nStart;` |
|    ! 0 | 2558 | `			nLen -= nStart;` |
|      - | 2559 | `		}` |
|    ! 0 | 2560 | `	}` |
|     76 | 2561 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2562 | `		/* Perform the lookup */` |
|     74 | 2563 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     74 | 2564 | `		if( rc != SXRET_OK ){` |
|      - | 2565 | `			/* Pattern not found,return FALSE */` |
|      5 | 2566 | `			ph7_result_bool(pCtx,0);` |
|      5 | 2567 | `			return PH7_OK;` |
|      - | 2568 | `		}` |
|      - | 2569 | `		/* Return the pattern position */` |
|     70 | 2570 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|     36 | 2571 | `	}else{` |
|      3 | 2572 | `		ph7_result_bool(pCtx,0);` |
|      - | 2573 | `	}` |
|     72 | 2574 | `	return PH7_OK;` |
|     42 | 2575 |  |
|      - | 2576 | `/*` |
|      - | 2577 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2578 | ` *  Case-insensitive strpos.` |
|      - | 2579 | ` * Parameters` |
|      - | 2580 | ` *  $haystack` |
|      - | 2581 | ` *   The input string.` |
|      - | 2582 | ` * $needle` |
|      - | 2583 | ` *   Search pattern (must be a string).` |
|      - | 2584 | ` * $offset` |
|      - | 2585 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 2586 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 2587 | ` *   of haystack.` |
|      - | 2588 | ` * Return` |
|      - | 2589 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 2590 | ` */` |
|     18 | 2591 | `static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2592 |  |
|     19 | 2593 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2594 | `	const char *zBlob,*zPattern;` |
|      - | 2595 | `	int nLen,nPatLen,nStart;` |
|      - | 2596 | `	sxu32 nOfft;` |
|      - | 2597 | `	sxi32 rc;` |
|     19 | 2598 | `	if( nArg < 2 ){` |
|      - | 2599 | `		/* Missing arguments,return FALSE */` |
|      3 | 2600 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2601 | `		return PH7_OK;` |
|      - | 2602 | `	}` |
|      - | 2603 | `	/* Extract the needle and the haystack */` |
|     17 | 2604 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 2605 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     17 | 2606 | `	nOfft = 0; /* cc warning */` |
|     17 | 2607 | `	nStart = 0;` |
|      - | 2608 | `	/* Peek the starting offset if available */` |
|     17 | 2609 | `	if( nArg > 2 ){` |
|      5 | 2610 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 2611 | `		if( nStart < 0 ){` |
|      3 | 2612 | `			nStart = -nStart;` |
|      1 | 2613 | `		}` |
|      5 | 2614 | `		if( nStart >= nLen ){` |
|      - | 2615 | `			/* Invalid offset */` |
|    ! 0 | 2616 | `			nStart = 0;` |
|    ! 0 | 2617 | `		}else{` |
|      5 | 2618 | `			zBlob += nStart;` |
|      5 | 2619 | `			nLen -= nStart;` |
|      - | 2620 | `		}` |
|      2 | 2621 | `	}` |
|     17 | 2622 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2623 | `		/* Perform the lookup */` |
|     17 | 2624 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     17 | 2625 | `		if( rc != SXRET_OK ){` |
|      - | 2626 | `			/* Pattern not found,return FALSE */` |
|      3 | 2627 | `			ph7_result_bool(pCtx,0);` |
|      3 | 2628 | `			return PH7_OK;` |
|      - | 2629 | `		}` |
|      - | 2630 | `		/* Return the pattern position */` |
|     15 | 2631 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 2632 | `	}else{` |
|    ! 0 | 2633 | `		ph7_result_bool(pCtx,0);` |
|      - | 2634 | `	}` |
|     15 | 2635 | `	return PH7_OK;` |
|     10 | 2636 |  |
|      - | 2637 | `/*` |
|      - | 2638 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2639 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 2640 | ` * Parameters` |
|      - | 2641 | ` *  $haystack` |
|      - | 2642 | ` *   The input string.` |
|      - | 2643 | ` * $needle` |
|      - | 2644 | ` *   Search pattern (must be a string).` |
|      - | 2645 | ` * $offset` |
|      - | 2646 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 2647 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 2648 | ` *   characters from the end of the string, searching backwards.` |
|      - | 2649 | ` * Return` |
|      - | 2650 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 2651 | ` */` |
|     32 | 2652 | `static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2653 |  |
|      - | 2654 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     33 | 2655 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2656 | `	int nLen,nPatLen;` |
|      - | 2657 | `	sxu32 nOfft;` |
|      - | 2658 | `	sxi32 rc;` |
|     33 | 2659 | `	if( nArg < 2 ){` |
|      - | 2660 | `		/* Missing arguments,return FALSE */` |
|      3 | 2661 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2662 | `		return PH7_OK;` |
|      - | 2663 | `	}` |
|      - | 2664 | `	/* Extract the needle and the haystack */` |
|     31 | 2665 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 2666 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 2667 | `	/* Point to the end of the pattern */` |
|     31 | 2668 | `	zPtr = &zBlob[nLen - 1];` |
|     31 | 2669 | `	zEnd = &zBlob[nLen];` |
|      - | 2670 | `	/* Save the starting posistion */` |
|     31 | 2671 | `	zStart = zBlob;` |
|     31 | 2672 | `	nOfft = 0; /* cc warning */` |
|      - | 2673 | `	/* Peek the starting offset if available */` |
|     31 | 2674 | `	if( nArg > 2 ){` |
|      - | 2675 | `		int nStart;` |
|     21 | 2676 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 2677 | `		if( nStart < 0 ){` |
|     11 | 2678 | `			nStart = -nStart;` |
|     11 | 2679 | `			if( nStart >= nLen ){` |
|      - | 2680 | `				/* Invalid offset */` |
|      3 | 2681 | `				ph7_result_bool(pCtx,0);` |
|      3 | 2682 | `				return PH7_OK;` |
|    ! 0 | 2683 | `			}else{` |
|      9 | 2684 | `				nLen -= nStart;` |
|      9 | 2685 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 2686 | `				zEnd = &zBlob[nLen];` |
|      - | 2687 | `			}` |
|      5 | 2688 | `		}else{` |
|     11 | 2689 | `			if( nStart >= nLen ){` |
|      - | 2690 | `				/* Invalid offset */` |
|      5 | 2691 | `				ph7_result_bool(pCtx,0);` |
|      5 | 2692 | `				return PH7_OK;` |
|    ! 0 | 2693 | `			}else{` |
|      7 | 2694 | `				zBlob += nStart;` |
|      7 | 2695 | `				nLen -= nStart;` |
|      - | 2696 | `			}` |
|      - | 2697 | `		}` |
|      7 | 2698 | `	}` |
|     25 | 2699 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2700 | `		/* Perform the lookup */` |
|     57 | 2701 | `		for(;;){` |
|    115 | 2702 | `			if( zBlob >= zPtr ){` |
|     11 | 2703 | `				break;` |
|      - | 2704 | `			}` |
|    105 | 2705 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    105 | 2706 | `			if( rc == SXRET_OK ){` |
|      - | 2707 | `				/* Pattern found,return it's position */` |
|     13 | 2708 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     13 | 2709 | `				return PH7_OK;` |
|      - | 2710 | `			}` |
|     93 | 2711 | `			zPtr--;` |
|      1 | 2712 | `		}` |
|      - | 2713 | `		/* Pattern not found,return FALSE */` |
|     11 | 2714 | `		ph7_result_bool(pCtx,0);` |
|      6 | 2715 | `	}else{` |
|      3 | 2716 | `		ph7_result_bool(pCtx,0);` |
|      - | 2717 | `	}` |
|     13 | 2718 | `	return PH7_OK;` |
|     17 | 2719 |  |
|      - | 2720 | `/*` |
|      - | 2721 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2722 | ` *  Case-insensitive strrpos.` |
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
|     28 | 2735 | `static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2736 |  |
|      - | 2737 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     29 | 2738 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2739 | `	int nLen,nPatLen;` |
|      - | 2740 | `	sxu32 nOfft;` |
|      - | 2741 | `	sxi32 rc;` |
|     29 | 2742 | `	if( nArg < 2 ){` |
|      - | 2743 | `		/* Missing arguments,return FALSE */` |
|      3 | 2744 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2745 | `		return PH7_OK;` |
|      - | 2746 | `	}` |
|      - | 2747 | `	/* Extract the needle and the haystack */` |
|     27 | 2748 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 2749 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 2750 | `	/* Point to the end of the pattern */` |
|     27 | 2751 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 2752 | `	zEnd = &zBlob[nLen];` |
|      - | 2753 | `	/* Save the starting posistion */` |
|     27 | 2754 | `	zStart = zBlob;` |
|     27 | 2755 | `	nOfft = 0; /* cc warning */` |
|      - | 2756 | `	/* Peek the starting offset if available */` |
|     27 | 2757 | `	if( nArg > 2 ){` |
|      - | 2758 | `		int nStart;` |
|     15 | 2759 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 2760 | `		if( nStart < 0 ){` |
|      7 | 2761 | `			nStart = -nStart;` |
|      7 | 2762 | `			if( nStart >= nLen ){` |
|      - | 2763 | `				/* Invalid offset */` |
|      3 | 2764 | `				ph7_result_bool(pCtx,0);` |
|      3 | 2765 | `				return PH7_OK;` |
|    ! 0 | 2766 | `			}else{` |
|      5 | 2767 | `				nLen -= nStart;` |
|      5 | 2768 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 2769 | `				zEnd = &zBlob[nLen];` |
|      - | 2770 | `			}` |
|      3 | 2771 | `		}else{` |
|      9 | 2772 | `			if( nStart >= nLen ){` |
|      - | 2773 | `				/* Invalid offset */` |
|      5 | 2774 | `				ph7_result_bool(pCtx,0);` |
|      5 | 2775 | `				return PH7_OK;` |
|    ! 0 | 2776 | `			}else{` |
|      5 | 2777 | `				zBlob += nStart;` |
|      5 | 2778 | `				nLen -= nStart;` |
|      - | 2779 | `			}` |
|      - | 2780 | `		}` |
|      4 | 2781 | `	}` |
|     21 | 2782 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2783 | `		/* Perform the lookup */` |
|     44 | 2784 | `		for(;;){` |
|     89 | 2785 | `			if( zBlob >= zPtr ){` |
|      9 | 2786 | `				break;` |
|      - | 2787 | `			}` |
|     81 | 2788 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 2789 | `			if( rc == SXRET_OK ){` |
|      - | 2790 | `				/* Pattern found,return it's position */` |
|     11 | 2791 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 2792 | `				return PH7_OK;` |
|      - | 2793 | `			}` |
|     71 | 2794 | `			zPtr--;` |
|      1 | 2795 | `		}` |
|      - | 2796 | `		/* Pattern not found,return FALSE */` |
|      9 | 2797 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2798 | `	}else{` |
|      3 | 2799 | `		ph7_result_bool(pCtx,0);` |
|      - | 2800 | `	}` |
|     11 | 2801 | `	return PH7_OK;` |
|     15 | 2802 |  |
|      - | 2803 | `/*` |
|      - | 2804 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 2805 | ` *  Find the last occurrence of a character in a string.` |
|      - | 2806 | ` * Parameters` |
|      - | 2807 | ` *  $haystack` |
|      - | 2808 | ` *   The input string.` |
|      - | 2809 | ` * $needle` |
|      - | 2810 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 2811 | ` *  This behavior is different from that of strstr().` |
|      - | 2812 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 2813 | ` *  as the ordinal value of a character.` |
|      - | 2814 | ` * Return` |
|      - | 2815 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 2816 | ` */` |
|     24 | 2817 | `static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2818 |  |
|      - | 2819 | `	const char *zBlob;` |
|      - | 2820 | `	int nLen,c;` |
|     25 | 2821 | `	if( nArg < 2 ){` |
|      - | 2822 | `		/* Missing arguments,return FALSE */` |
|      3 | 2823 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2824 | `		return PH7_OK;` |
|      - | 2825 | `	}` |
|      - | 2826 | `	/* Extract the haystack */` |
|     23 | 2827 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 2828 | `	c = 0; /* cc warning */` |
|     23 | 2829 | `	if( nLen > 0 ){` |
|      - | 2830 | `		sxu32 nOfft;` |
|      - | 2831 | `		sxi32 rc;` |
|     21 | 2832 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 2833 | `			const char *zPattern;` |
|     11 | 2834 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 2835 | `														 * for NULL pointer.` |
|      - | 2836 | `														 */` |
|     11 | 2837 | `			c = zPattern[0];` |
|      6 | 2838 | `		}else{` |
|      - | 2839 | `			/* Int cast */` |
|     11 | 2840 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 2841 | `		}` |
|      - | 2842 | `		/* Perform the lookup */` |
|     21 | 2843 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 2844 | `		if( rc != SXRET_OK ){` |
|      - | 2845 | `			/* No such entry,return FALSE */` |
|      7 | 2846 | `			ph7_result_bool(pCtx,0);` |
|      7 | 2847 | `			return PH7_OK;` |
|      - | 2848 | `		}` |
|      - | 2849 | `		/* Return the string portion */` |
|     15 | 2850 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 2851 | `	}else{` |
|      3 | 2852 | `		ph7_result_bool(pCtx,0);` |
|      - | 2853 | `	}` |
|     17 | 2854 | `	return PH7_OK;` |
|     13 | 2855 |  |
|      - | 2856 | `/*` |
|      - | 2857 | ` * string strrev(string $string)` |
|      - | 2858 | ` *  Reverse a string.` |
|      - | 2859 | ` * Parameters` |
|      - | 2860 | ` *  $string` |
|      - | 2861 | ` *   String to be reversed.` |
|      - | 2862 | ` * Return` |
|      - | 2863 | ` *  The reversed string.` |
|      - | 2864 | ` */` |
|      4 | 2865 | `static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2866 |  |
|      - | 2867 | `	const char *zIn,*zEnd;` |
|      - | 2868 | `	int nLen,c;` |
|      5 | 2869 | `	if( nArg < 1 ){` |
|      - | 2870 | `		/* Missing arguments,return NULL */` |
|      3 | 2871 | `		ph7_result_null(pCtx);` |
|      3 | 2872 | `		return PH7_OK;` |
|      - | 2873 | `	}` |
|      - | 2874 | `	/* Extract the target string */` |
|      3 | 2875 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 2876 | `	if( nLen < 1 ){` |
|      - | 2877 | `		/* Empty string Return null */` |
|    ! 0 | 2878 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2879 | `		return PH7_OK;` |
|      - | 2880 | `	}` |
|      - | 2881 | `	/* Perform the requested operation */` |
|      3 | 2882 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 2883 | `	for(;;){` |
|      9 | 2884 | `		if( zEnd < zIn ){` |
|      - | 2885 | `			/* No more input to process */` |
|      3 | 2886 | `			break;` |
|      - | 2887 | `		}` |
|      - | 2888 | `		/* Append current character */` |
|      7 | 2889 | `		c = zEnd[0];` |
|      7 | 2890 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 2891 | `		zEnd--;` |
|      1 | 2892 | `	}` |
|      3 | 2893 | `	return PH7_OK;` |
|      3 | 2894 |  |
|      - | 2895 | `/*` |
|      - | 2896 | ` * string ucwords(string $string)` |
|      - | 2897 | ` *  Uppercase the first character of each word in a string.` |
|      - | 2898 | ` *  The definition of a word is any string of characters that is immediately after` |
|      - | 2899 | ` *  a whitespace (These are: space, form-feed, newline, carriage return, horizontal tab, and vertical tab).` |
|      - | 2900 | ` * Parameters` |
|      - | 2901 | ` *  $string` |
|      - | 2902 | ` *   The input string.` |
|      - | 2903 | ` * Return` |
|      - | 2904 | ` *  The modified string..` |
|      - | 2905 | ` */` |
|     14 | 2906 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2907 |  |
|      - | 2908 | `	const char *zIn,*zCur,*zEnd;` |
|      - | 2909 | `	int nLen,c;` |
|     15 | 2910 | `	if( nArg < 1 ){` |
|      - | 2911 | `		/* Missing arguments,return NULL */` |
|      3 | 2912 | `		ph7_result_null(pCtx);` |
|      3 | 2913 | `		return PH7_OK;` |
|      - | 2914 | `	}` |
|      - | 2915 | `	/* Extract the target string */` |
|     13 | 2916 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 2917 | `	if( nLen < 1 ){` |
|      - | 2918 | `		/* Empty string – match PHP semantics */` |
|      3 | 2919 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2920 | `		return PH7_OK;` |
|      - | 2921 | `	}` |
|      - | 2922 | `	/* Perform the requested operation */` |
|     11 | 2923 | `	zEnd = &zIn[nLen];` |
|     21 | 2924 | `	for(;;){` |
|      - | 2925 | `		/* Jump leading white spaces */` |
|     43 | 2926 | `		zCur = zIn;` |
|     65 | 2927 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0x80 && SyisSpace(zIn[0]) ){` |
|     23 | 2928 | `			zIn++;` |
|      1 | 2929 | `		}` |
|     43 | 2930 | `		if( zCur < zIn ){` |
|      - | 2931 | `			/* Append white space stream */` |
|     23 | 2932 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 2933 | `		}` |
|     43 | 2934 | `		if( zIn >= zEnd ){` |
|      - | 2935 | `			/* No more input to process */` |
|     11 | 2936 | `			break;` |
|      - | 2937 | `		}` |
|     33 | 2938 | `		c = zIn[0];` |
|     33 | 2939 | `		if( c < 0x80 && SyisLower(c) ){` |
|     29 | 2940 | `			c = SyToUpper(c);` |
|     14 | 2941 | `		}` |
|      - | 2942 | `		/* Append the upper-cased character */` |
|     33 | 2943 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     33 | 2944 | `		zIn++;` |
|     33 | 2945 | `		zCur = zIn;` |
|      - | 2946 | `		/* Append the word varbatim */` |
|    149 | 2947 | `		while( zIn < zEnd ){` |
|    139 | 2948 | `			if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 2949 | `				/* UTF-8 stream */` |
|    ! 0 | 2950 | `				zIn++;` |
|    ! 0 | 2951 | `				SX_JMP_UTF8(zIn,zEnd);` |
|    138 | 2952 | `			}else if( !SyisSpace(zIn[0]) ){` |
|    117 | 2953 | `				zIn++;` |
|     59 | 2954 | `			}else{` |
|     23 | 2955 | `				break;` |
|      - | 2956 | `			}` |
|      1 | 2957 | `		}` |
|     33 | 2958 | `		if( zCur < zIn ){` |
|     33 | 2959 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     16 | 2960 | `		}` |
|      1 | 2961 | `	}` |
|     11 | 2962 | `	return PH7_OK;` |
|      8 | 2963 |  |
|      - | 2964 | `/*` |
|      - | 2965 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 2966 | ` *  Returns input repeated multiplier times.` |
|      - | 2967 | ` * Parameters` |
|      - | 2968 | ` *  $string` |
|      - | 2969 | ` *   String to be repeated.` |
|      - | 2970 | ` * $multiplier` |
|      - | 2971 | ` *  Number of time the input string should be repeated.` |
|      - | 2972 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 2973 | ` *  to 0, the function will return an empty string.` |
|      - | 2974 | ` * Return` |
|      - | 2975 | ` *  The repeated string.` |
|      - | 2976 | ` */` |
|  20212 | 2977 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2978 |  |
|      - | 2979 | `	const char *zIn;` |
|      - | 2980 | `	int nLen,nMul;` |
|      - | 2981 | `	int rc;` |
|  20213 | 2982 | `	if( nArg < 2 ){` |
|      - | 2983 | `		/* Missing arguments,return NULL */` |
|      3 | 2984 | `		ph7_result_null(pCtx);` |
|      3 | 2985 | `		return PH7_OK;` |
|      - | 2986 | `	}` |
|      - | 2987 | `	/* Extract the target string */` |
|  20211 | 2988 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|  20211 | 2989 | `	if( nLen < 1 ){` |
|      - | 2990 | `		/* Empty string.Return null */` |
|    ! 0 | 2991 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2992 | `		return PH7_OK;` |
|      - | 2993 | `	}` |
|      - | 2994 | `	/* Extract the multiplier */` |
|  20211 | 2995 | `	nMul = ph7_value_to_int(apArg[1]);` |
|  20211 | 2996 | `	if( nMul < 1 ){` |
|      - | 2997 | `		/* Return the empty string */` |
|      3 | 2998 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2999 | `		return PH7_OK;` |
|      - | 3000 | `	}` |
|      - | 3001 | `	/* Perform the requested operation */` |
| 120220 | 3002 | `	for(;;){` |
| 240441 | 3003 | `		if( !nMul ){` |
|  20209 | 3004 | `			break;` |
|      - | 3005 | `		}` |
|      - | 3006 | `		/* Append the copy */` |
| 220233 | 3007 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 220233 | 3008 | `		if( rc != PH7_OK ){` |
|      - | 3009 | `			/* Out of memory,break immediately */` |
|    ! 0 | 3010 | `			break;` |
|      - | 3011 | `		}` |
| 220233 | 3012 | `		nMul--;` |
|      1 | 3013 | `	}` |
|  20209 | 3014 | `	return PH7_OK;` |
|  10107 | 3015 |  |
|      - | 3016 | `/*` |
|      - | 3017 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 3018 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 3019 | ` * Parameters` |
|      - | 3020 | ` *  $string` |
|      - | 3021 | ` *   The input string.` |
|      - | 3022 | ` * $is_xhtml` |
|      - | 3023 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 3024 | ` * Return` |
|      - | 3025 | ` *  The processed string.` |
|      - | 3026 | ` */` |
|      6 | 3027 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3028 |  |
|      - | 3029 | `	const char *zIn,*zCur,*zEnd;` |
|      7 | 3030 | `	int is_xhtml = 1; /* Default to XHTML-style '<br/>' like PHP */` |
|      - | 3031 | `	int nLen;` |
|      7 | 3032 | `	if( nArg < 1 ){` |
|      - | 3033 | `		/* Missing arguments,return the empty string */` |
|      3 | 3034 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3035 | `		return PH7_OK;` |
|      - | 3036 | `	}` |
|      - | 3037 | `	/* Extract the target string */` |
|      5 | 3038 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3039 | `	if( nLen < 1 ){` |
|      - | 3040 | `		/* Empty string,return null */` |
|    ! 0 | 3041 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3042 | `		return PH7_OK;` |
|      - | 3043 | `	}` |
|      5 | 3044 | `	if( nArg > 1 ){` |
|      3 | 3045 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 3046 | `	}` |
|      5 | 3047 | `	zEnd = &zIn[nLen];` |
|      - | 3048 | `	/* Perform the requested operation */` |
|      4 | 3049 | `	for(;;){` |
|      9 | 3050 | `		zCur = zIn;` |
|      - | 3051 | `		/* Delimit the string */` |
|     21 | 3052 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 3053 | `			zIn++;` |
|      1 | 3054 | `		}` |
|      9 | 3055 | `		if( zCur < zIn ){` |
|      - | 3056 | `			/* Output chunk verbatim */` |
|      9 | 3057 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 3058 | `		}` |
|      9 | 3059 | `		if( zIn >= zEnd ){` |
|      - | 3060 | `			/* No more input to process */` |
|      5 | 3061 | `			break;` |
|      - | 3062 | `		}` |
|      - | 3063 | `		/* Output the HTML line break */` |
|      - | 3064 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br/>' (legacy without space), otherwise use '<br>' */` |
|      5 | 3065 | `		if( is_xhtml ){` |
|      3 | 3066 | `			ph7_result_string(pCtx,"<br/>",(int)sizeof("<br/>")-1);` |
|      2 | 3067 | `		}else{` |
|      3 | 3068 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 3069 | `		}` |
|      5 | 3070 | `		zCur = zIn;` |
|      - | 3071 | `		/* Append trailing line */` |
|     11 | 3072 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 3073 | `			zIn++;` |
|      1 | 3074 | `		}` |
|      5 | 3075 | `		if( zCur < zIn ){` |
|      - | 3076 | `			/* Output chunk verbatim */` |
|      5 | 3077 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 3078 | `		}` |
|      1 | 3079 | `	}` |
|      5 | 3080 | `	return PH7_OK;` |
|      4 | 3081 |  |
|      - | 3082 | `/*` |
|      - | 3083 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 3084 | ` *  According to the PHP reference manual.` |
|      - | 3085 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 3086 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 3087 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 3088 | ` * This applies to both sprintf() and printf().` |
|      - | 3089 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 3090 | ` * or more of these elements, in order:` |
|      - | 3091 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 3092 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 3093 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 3094 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 3095 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 3096 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 3097 | ` *   it with a single quote ('). See the examples below.` |
|      - | 3098 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 3099 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 3100 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 3101 | ` *   should result in.` |
|      - | 3102 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 3103 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 3104 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 3105 | ` *   limit to the string.` |
|      - | 3106 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 3107 | ` *       % - a literal percent character. No argument is required.` |
|      - | 3108 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 3109 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 3110 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 3111 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 3112 | ` * 	     for the number of digits after the decimal point.` |
|      - | 3113 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 3114 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 3115 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 3116 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 3117 | ` *       g - shorter of %e and %f.` |
|      - | 3118 | ` *       G - shorter of %E and %f.` |
|      - | 3119 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 3120 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 3121 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 3122 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 3123 | ` */` |
|      - | 3124 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 3125 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 3126 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 3127 | `/*` |
|      - | 3128 | `** Conversion types fall into various categories as defined by the` |
|      - | 3129 | `** following enumeration.` |
|      - | 3130 | `*/` |
|      - | 3131 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 3132 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 3133 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 3134 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 3135 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 3136 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 3137 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 3138 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 3139 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 3140 |  |
|      - | 3141 | `/*` |
|      - | 3142 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 3143 | `*/` |
|      - | 3144 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 3145 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 3146 | `/*` |
|      - | 3147 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 3148 | `** by an instance of the following structure` |
|      - | 3149 | `*/` |
|      - | 3150 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 3151 | `struct ph7_fmt_info` |
|      - | 3152 |  |
|      - | 3153 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 3154 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 3155 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 3156 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 3157 | `  char *charset; /* The character set for conversion */` |
|      - | 3158 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 3159 | `};` |
|      - | 3160 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 3161 | `/*` |
|      - | 3162 | `** "*val" is a double such that 0.1 <= *val < 10.0` |
|      - | 3163 | `** Return the ascii code for the leading digit of *val, then` |
|      - | 3164 | `** multiply "*val" by 10.0 to renormalize.` |
|      - | 3165 | `**` |
|      - | 3166 | `** Example:` |
|      - | 3167 | `**     input:     *val = 3.14159` |
|      - | 3168 | `**     output:    *val = 1.4159    function return = '3'` |
|      - | 3169 | `**` |
|      - | 3170 | `** The counter *cnt is incremented each time.  After counter exceeds` |
|      - | 3171 | `** 16 (the number of significant digits in a 64-bit float) '0' is` |
|      - | 3172 | `** always returned.` |
|      - | 3173 | `*/` |
|    404 | 3174 | `static int vxGetdigit(sxlongreal *val,int *cnt)` |
|      1 | 3175 |  |
|      - | 3176 | `  sxlongreal d;` |
|      - | 3177 | `  int digit;` |
|      - | 3178 |  |
|    405 | 3179 | `  if( (*cnt)++ >= 16 ){` |
|    ! 0 | 3180 | `	  return '0';` |
|      - | 3181 | `  }` |
|    405 | 3182 | `  digit = (int)*val;` |
|    405 | 3183 | `  d = digit;` |
|    405 | 3184 | `   *val = (*val - d)*10.0;` |
|    405 | 3185 | `  return digit + '0' ;` |
|    203 | 3186 |  |
|      - | 3187 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      - | 3188 | `/*` |
|      - | 3189 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 3190 | ` * used conversion types first.` |
|      - | 3191 | ` */` |
|      - | 3192 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 3193 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 3194 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 3195 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 3196 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 3197 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 3198 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 3199 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 3200 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 3201 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3202 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3203 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 3204 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 3205 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 3206 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 3207 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 3208 | `};` |
|      - | 3209 | `/*` |
|      - | 3210 | ` * Format a given string.` |
|      - | 3211 | ` * The root program.  All variations call this core.` |
|      - | 3212 | ` * INPUTS:` |
|      - | 3213 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 3214 | ` *            1. A pointer to the call context.` |
|      - | 3215 | ` *            2. A pointer to the list of characters to be output` |
|      - | 3216 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 3217 | ` *            3. An integer number of characters to be output.` |
|      - | 3218 | ` *               (Note: This number might be zero.)` |
|      - | 3219 | ` *            4. Upper layer private data.` |
|      - | 3220 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 3221 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 3222 | ` */` |
|    120 | 3223 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 3224 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 3225 | `	ph7_context *pCtx,  /* call context */` |
|      - | 3226 | `	const char *zIn,    /* Format string */` |
|      - | 3227 | `	int nByte,          /* Format string length */` |
|      - | 3228 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 3229 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 3230 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 3231 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 3232 | `	)` |
|      1 | 3233 |  |
|    121 | 3234 | `	char spaces[] = "                                                  ";` |
|      - | 3235 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    121 | 3236 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 3237 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 3238 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 3239 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 3240 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 3241 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 3242 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 3243 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 3244 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 3245 | `	ph7_int64 iVal;` |
|      - | 3246 | `	int precision;           /* Precision of the current field */` |
|      - | 3247 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 3248 | `	int c,rc,n;` |
|      - | 3249 | `	int length;              /* Length of the field */` |
|      - | 3250 | `	int prefix;` |
|      - | 3251 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 3252 | `	int width;               /* Width of the current field */` |
|      - | 3253 | `	int idx;` |
|    121 | 3254 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 3255 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 3256 | `	/* Start the format process */` |
|    123 | 3257 | `	for(;;){` |
|    247 | 3258 | `		zCur = zIn;` |
|    697 | 3259 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|    451 | 3260 | `			zIn++;` |
|      1 | 3261 | `		}` |
|    247 | 3262 | `		if( zCur < zIn ){` |
|      - | 3263 | `			/* Consume chunk verbatim */` |
|     95 | 3264 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|     95 | 3265 | `			if( rc == SXERR_ABORT ){` |
|      - | 3266 | `				/* Callback request an operation abort */` |
|    ! 0 | 3267 | `				break;` |
|      - | 3268 | `			}` |
|     47 | 3269 | `		}` |
|    247 | 3270 | `		if( zIn >= zEnd ){` |
|      - | 3271 | `			/* No more input to process,break immediately */` |
|    119 | 3272 | `			break;` |
|      - | 3273 | `		}` |
|      - | 3274 | `		/* Find out what flags are present */` |
|    129 | 3275 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    128 | 3276 | `			flag_alternateform = flag_zeropad = 0;` |
|    129 | 3277 | `		zIn++; /* Jump the precent sign */` |
|     64 | 3278 | `		do{` |
|    157 | 3279 | `			c = zIn[0];` |
|    157 | 3280 | `			switch( c ){` |
|      9 | 3281 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      3 | 3282 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 3283 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|      9 | 3284 | `			case '#':   flag_alternateform = 1;   c = 0;   break;` |
|      5 | 3285 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 3286 | `			case '\'':` |
|    ! 0 | 3287 | `				zIn++;` |
|    ! 0 | 3288 | `				if( zIn < zEnd ){` |
|      - | 3289 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    ! 0 | 3290 | `					c = zIn[0];` |
|    ! 0 | 3291 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3292 | `						spaces[idx] = (char)c;` |
|    ! 0 | 3293 | `					}` |
|    ! 0 | 3294 | `					c = 0;` |
|    ! 0 | 3295 | `				}` |
|    ! 0 | 3296 | `				break;` |
|    128 | 3297 | `			default:                                       break;` |
|      - | 3298 | `			}` |
|    157 | 3299 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 3300 | `		/* Get the field width */` |
|    129 | 3301 | `		width = 0;` |
|    223 | 3302 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     31 | 3303 | `			width = width*10 + (zIn[0] - '0');` |
|     31 | 3304 | `			zIn++;` |
|      1 | 3305 | `		}` |
|    129 | 3306 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 3307 | `			/* Position specifer */` |
|    ! 0 | 3308 | `			if( width > 0 ){` |
|    ! 0 | 3309 | `				n = width;` |
|    ! 0 | 3310 | `				if( vf && n > 0 ){` |
|    ! 0 | 3311 | `					n--;` |
|    ! 0 | 3312 | `				}` |
|    ! 0 | 3313 | `			}` |
|    ! 0 | 3314 | `			zIn++;` |
|    ! 0 | 3315 | `			width = 0;` |
|    ! 0 | 3316 | `			if( zIn < zEnd && zIn[0] == '0' ){` |
|    ! 0 | 3317 | `				flag_zeropad = 1;` |
|    ! 0 | 3318 | `				zIn++;` |
|    ! 0 | 3319 | `			}` |
|    ! 0 | 3320 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    ! 0 | 3321 | `				width = width*10 + (zIn[0] - '0');` |
|    ! 0 | 3322 | `				zIn++;` |
|    ! 0 | 3323 | `			}` |
|    ! 0 | 3324 | `		}` |
|    129 | 3325 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 3326 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 3327 | `		}` |
|      - | 3328 | `		/* Get the precision */` |
|    129 | 3329 | `		precision = -1;` |
|    129 | 3330 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     57 | 3331 | `			precision = 0;` |
|     57 | 3332 | `			zIn++;` |
|    145 | 3333 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     61 | 3334 | `				precision = precision*10 + (zIn[0] - '0');` |
|     61 | 3335 | `				zIn++;` |
|      1 | 3336 | `			}` |
|     28 | 3337 | `		}` |
|    129 | 3338 | `		if( zIn >= zEnd ){` |
|      - | 3339 | `			/* No more input */` |
|      3 | 3340 | `			break;` |
|      - | 3341 | `		}` |
|      - | 3342 | `		/* Fetch the info entry for the field */` |
|    127 | 3343 | `		pInfo = 0;` |
|    127 | 3344 | `		xtype = PH7_FMT_ERROR;` |
|    127 | 3345 | `		c = zIn[0];` |
|    127 | 3346 | `		zIn++; /* Jump the format specifer */` |
|    699 | 3347 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|    697 | 3348 | `			if( c==aFmt[idx].fmttype ){` |
|    125 | 3349 | `				pInfo = &aFmt[idx];` |
|    125 | 3350 | `				xtype = pInfo->type;` |
|    125 | 3351 | `				break;` |
|      - | 3352 | `			}` |
|    287 | 3353 | `		}` |
|    127 | 3354 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    127 | 3355 | `		length = 0;` |
|      - | 3356 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 3357 | `		 /*` |
|      - | 3358 | `		  ** At this point, variables are initialized as follows:` |
|      - | 3359 | `		  **` |
|      - | 3360 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 3361 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 3362 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 3363 | `		  **                               field width was negative.` |
|      - | 3364 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 3365 | `		  **                               the conversion character.` |
|      - | 3366 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 3367 | `		  **   width                       The specified field width.  This is` |
|      - | 3368 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 3369 | `		  **   precision                   The specified precision.  The default` |
|      - | 3370 | `		  **                               is -1.` |
|      - | 3371 | `		  */` |
|    127 | 3372 | `		switch(xtype){` |
|    ! 0 | 3373 | `		case PH7_FMT_PERCENT:` |
|      - | 3374 | `			/* A literal percent character */` |
|    ! 0 | 3375 | `			zWorker[0] = '%';` |
|    ! 0 | 3376 | `			length = (int)sizeof(char);` |
|    ! 0 | 3377 | `			break;` |
|      3 | 3378 | `		case PH7_FMT_CHARX:` |
|      - | 3379 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 3380 | `			 * with that ASCII value` |
|      - | 3381 | `			 */` |
|      7 | 3382 | `			pArg = NEXT_ARG;` |
|      7 | 3383 | `			if( pArg == 0 ){` |
|      3 | 3384 | `				c = 0;` |
|      2 | 3385 | `			}else{` |
|      5 | 3386 | `				c = ph7_value_to_int(pArg);` |
|      - | 3387 | `			}` |
|      - | 3388 | `			/* NUL byte is an acceptable value */` |
|      7 | 3389 | `			zWorker[0] = (char)c;` |
|      7 | 3390 | `			length = (int)sizeof(char);` |
|      7 | 3391 | `			break;` |
|     12 | 3392 | `		case PH7_FMT_STRING:` |
|      - | 3393 | `			/* the argument is treated as and presented as a string */` |
|     25 | 3394 | `			pArg = NEXT_ARG;` |
|     25 | 3395 | `			if( pArg == 0 ){` |
|    ! 0 | 3396 | `				length = 0;` |
|    ! 0 | 3397 | `			}else{` |
|     25 | 3398 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 3399 | `			}` |
|     25 | 3400 | `			if( length < 1 ){` |
|    ! 0 | 3401 | `				zBuf = " ";` |
|    ! 0 | 3402 | `				length = (int)sizeof(char);` |
|    ! 0 | 3403 | `			}` |
|     25 | 3404 | `			if( precision>=0 && precision<length ){` |
|      3 | 3405 | `				length = precision;` |
|      1 | 3406 | `			}` |
|     25 | 3407 | `			if( flag_zeropad ){` |
|      - | 3408 | `				/* zero-padding works on strings too */` |
|    ! 0 | 3409 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3410 | `					spaces[idx] = '0';` |
|    ! 0 | 3411 | `				}` |
|    ! 0 | 3412 | `			}` |
|     25 | 3413 | `			break;` |
|     20 | 3414 | `		case PH7_FMT_RADIX:` |
|     41 | 3415 | `			pArg = NEXT_ARG;` |
|     41 | 3416 | `			if( pArg == 0 ){` |
|    ! 0 | 3417 | `				iVal = 0;` |
|    ! 0 | 3418 | `			}else{` |
|     41 | 3419 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 3420 | `			}` |
|      - | 3421 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|     41 | 3422 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 3423 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 3424 | `			}` |
|      - | 3425 | `#if 1` |
|      - | 3426 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 3427 | `        ** I think this is stupid.*/` |
|     41 | 3428 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 3429 | `#else` |
|      - | 3430 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 3431 | `        ** but leave the prefix for hex.*/` |
|      - | 3432 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 3433 | `#endif` |
|     41 | 3434 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|     23 | 3435 | `          if( iVal<0 ){` |
|      3 | 3436 | `            iVal = -iVal;` |
|      - | 3437 | `			/* Ticket 1433-003 */` |
|      3 | 3438 | `			if( iVal < 0 ){` |
|      - | 3439 | `				/* Overflow */` |
|    ! 0 | 3440 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 3441 | `			}` |
|      3 | 3442 | `            prefix = '-';` |
|     22 | 3443 | `          }else if( flag_plussign )  prefix = '+';` |
|     19 | 3444 | `          else if( flag_blanksign )  prefix = ' ';` |
|     17 | 3445 | `          else                       prefix = 0;` |
|     12 | 3446 | `        }else{` |
|     19 | 3447 | `			if( iVal<0 ){` |
|    ! 0 | 3448 | `				iVal = -iVal;` |
|      - | 3449 | `				/* Ticket 1433-003 */` |
|    ! 0 | 3450 | `				if( iVal < 0 ){` |
|      - | 3451 | `					/* Overflow */` |
|    ! 0 | 3452 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 3453 | `				}` |
|    ! 0 | 3454 | `			}` |
|     19 | 3455 | `			prefix = 0;` |
|      - | 3456 | `		}` |
|     41 | 3457 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|      3 | 3458 | `          precision = width-(prefix!=0);` |
|      1 | 3459 | `        }` |
|     41 | 3460 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 3461 | `        {` |
|      - | 3462 | `          register char *cset;      /* Use registers for speed */` |
|      - | 3463 | `          register int base;` |
|     41 | 3464 | `          cset = pInfo->charset;` |
|     41 | 3465 | `          base = pInfo->base;` |
|     20 | 3466 | `          do{                                           /* Convert to ascii */` |
|     79 | 3467 | `            *(--zBuf) = cset[iVal%base];` |
|     79 | 3468 | `            iVal = iVal/base;` |
|     79 | 3469 | `          }while( iVal>0 );` |
|      - | 3470 | `        }` |
|     41 | 3471 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     55 | 3472 | `        for(idx=precision-length; idx>0; idx--){` |
|     15 | 3473 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|      8 | 3474 | `        }` |
|     41 | 3475 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|     41 | 3476 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 3477 | `          char *pre, x;` |
|      9 | 3478 | `          pre = pInfo->prefix;` |
|      9 | 3479 | `          if( *zBuf!=pre[0] ){` |
|     23 | 3480 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|      4 | 3481 | `          }` |
|      4 | 3482 | `        }` |
|     41 | 3483 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|     41 | 3484 | `		break;` |
|     27 | 3485 | `		case PH7_FMT_FLOAT:` |
|      - | 3486 | `		case PH7_FMT_EXP:` |
|      - | 3487 | `		case PH7_FMT_GENERIC:{` |
|      - | 3488 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 3489 | `		long double realvalue;` |
|      - | 3490 | `		int  exp;                /* exponent of real numbers */` |
|      - | 3491 | `		double rounder;          /* Used for rounding floating point values */` |
|      - | 3492 | `		int flag_dp;            /* True if decimal point should be shown */` |
|      - | 3493 | `		int flag_rtz;           /* True if trailing zeros should be removed */` |
|      - | 3494 | `		int flag_exp;           /* True to force display of the exponent */` |
|      - | 3495 | `		int nsd;                 /* Number of significant digits returned */` |
|     55 | 3496 | `		pArg = NEXT_ARG;` |
|     55 | 3497 | `		if( pArg == 0 ){` |
|    ! 0 | 3498 | `			realvalue = 0;` |
|    ! 0 | 3499 | `		}else{` |
|     55 | 3500 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 3501 | `		}` |
|      - | 3502 | `		/* Special-case NaN and infinities since the normal formatting logic` |
|      - | 3503 | `		 * below assumes a finite positive realvalue. */` |
|     55 | 3504 | `		if( PH7_IS_NAN(realvalue) ){` |
|    ! 0 | 3505 | `			zBuf = "NAN";` |
|    ! 0 | 3506 | `			length = 3;` |
|    ! 0 | 3507 | `			break;` |
|      - | 3508 | `		}` |
|     55 | 3509 | `		if( PH7_IS_INF(realvalue) ){` |
|      - | 3510 | `			/* Infinity prints as INF or -INF depending on sign. */` |
|    ! 0 | 3511 | `			if( realvalue < 0.0 ){` |
|    ! 0 | 3512 | `				zBuf = "-INF";` |
|    ! 0 | 3513 | `				length = 4;` |
|    ! 0 | 3514 | `			}else{` |
|    ! 0 | 3515 | `				zBuf = "INF";` |
|    ! 0 | 3516 | `				length = 3;` |
|      - | 3517 | `			}` |
|    ! 0 | 3518 | `			break;` |
|      - | 3519 | `		}` |
|     55 | 3520 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|     55 | 3521 | `		if( precision>PH7_FMT_BUFSIZ-40) precision = PH7_FMT_BUFSIZ-40;` |
|     55 | 3522 | `        if( realvalue<0.0 ){` |
|    ! 0 | 3523 | `          realvalue = -realvalue;` |
|    ! 0 | 3524 | `          prefix = '-';` |
|    ! 0 | 3525 | `        }else{` |
|     55 | 3526 | `          if( flag_plussign )          prefix = '+';` |
|     55 | 3527 | `          else if( flag_blanksign )    prefix = ' ';` |
|     55 | 3528 | `          else                         prefix = 0;` |
|      - | 3529 | `        }` |
|     55 | 3530 | `        if( pInfo->type==PH7_FMT_GENERIC && precision>0 ) precision--;` |
|     55 | 3531 | `        rounder = 0.0;` |
|      - | 3532 | `#if 0` |
|      - | 3533 | `        /* Rounding works like BSD when the constant 0.4999 is used.Wierd! */` |
|      - | 3534 | `        for(idx=precision, rounder=0.4999; idx>0; idx--, rounder*=0.1);` |
|      - | 3535 | `#else` |
|      - | 3536 | `        /* It makes more sense to use 0.5 */` |
|    387 | 3537 | `        for(idx=precision, rounder=0.5; idx>0; idx--, rounder*=0.1);` |
|      - | 3538 | `#endif` |
|     55 | 3539 | `        if( pInfo->type==PH7_FMT_FLOAT ) realvalue += rounder;` |
|      - | 3540 | `        /* Normalize realvalue to within 10.0 > realvalue >= 1.0 */` |
|     55 | 3541 | `        exp = 0;` |
|     55 | 3542 | `        if( realvalue>0.0 ){` |
|     59 | 3543 | `          while( realvalue>=1e8 && exp<=350 ){ realvalue *= 1e-8; exp+=8; }` |
|     87 | 3544 | `          while( realvalue>=10.0 && exp<=350 ){ realvalue *= 0.1; exp++; }` |
|     59 | 3545 | `          while( realvalue<1e-8 && exp>=-350 ){ realvalue *= 1e8; exp-=8; }` |
|     69 | 3546 | `          while( realvalue<1.0 && exp>=-350 ){ realvalue *= 10.0; exp--; }` |
|     55 | 3547 | `          if( exp>350 \|\| exp<-350 ){` |
|    ! 0 | 3548 | `            zBuf = "NaN";` |
|    ! 0 | 3549 | `            length = 3;` |
|    ! 0 | 3550 | `            break;` |
|      - | 3551 | `          }` |
|     27 | 3552 | `        }` |
|     55 | 3553 | `        zBuf = zWorker;` |
|      - | 3554 | `        /*` |
|      - | 3555 | `        ** If the field type is etGENERIC, then convert to either etEXP` |
|      - | 3556 | `        ** or etFLOAT, as appropriate.` |
|      - | 3557 | `        */` |
|     55 | 3558 | `        flag_exp = xtype==PH7_FMT_EXP;` |
|     55 | 3559 | `        if( xtype!=PH7_FMT_FLOAT ){` |
|    ! 0 | 3560 | `          realvalue += rounder;` |
|    ! 0 | 3561 | `          if( realvalue>=10.0 ){ realvalue *= 0.1; exp++; }` |
|    ! 0 | 3562 | `        }` |
|     55 | 3563 | `        if( xtype==PH7_FMT_GENERIC ){` |
|    ! 0 | 3564 | `          flag_rtz = !flag_alternateform;` |
|    ! 0 | 3565 | `          if( exp<-4 \|\| exp>precision ){` |
|    ! 0 | 3566 | `            xtype = PH7_FMT_EXP;` |
|    ! 0 | 3567 | `          }else{` |
|    ! 0 | 3568 | `            precision = precision - exp;` |
|    ! 0 | 3569 | `            xtype = PH7_FMT_FLOAT;` |
|      - | 3570 | `          }` |
|    ! 0 | 3571 | `        }else{` |
|     55 | 3572 | `          flag_rtz = 0;` |
|      - | 3573 | `        }` |
|      - | 3574 | `        /*` |
|      - | 3575 | `        ** The "exp+precision" test causes output to be of type etEXP if` |
|      - | 3576 | `        ** the precision is too large to fit in buf[].` |
|      - | 3577 | `        */` |
|     55 | 3578 | `        nsd = 0;` |
|     55 | 3579 | `        if( xtype==PH7_FMT_FLOAT && exp+precision<PH7_FMT_BUFSIZ-30 ){` |
|     55 | 3580 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|     55 | 3581 | `          if( prefix ) *(zBuf++) = (char)prefix;         /* Sign */` |
|     55 | 3582 | `          if( exp<0 )  *(zBuf++) = '0';            /* Digits before "." */` |
|    145 | 3583 | `          else for(; exp>=0; exp--) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     55 | 3584 | `          if( flag_dp ) *(zBuf++) = '.';           /* The decimal point */` |
|     87 | 3585 | `          for(exp++; exp<0 && precision>0; precision--, exp++){` |
|     33 | 3586 | `            *(zBuf++) = '0';` |
|     17 | 3587 | `          }` |
|    355 | 3588 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|     55 | 3589 | `          *(zBuf--) = 0;                           /* Null terminate */` |
|     55 | 3590 | `          if( flag_rtz && flag_dp ){     /* Remove trailing zeros and "." */` |
|    ! 0 | 3591 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 3592 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 3593 | `          }` |
|     55 | 3594 | `          zBuf++;                            /* point to next free slot */` |
|     28 | 3595 | `        }else{    /* etEXP or etGENERIC */` |
|    ! 0 | 3596 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|    ! 0 | 3597 | `          if( prefix ) *(zBuf++) = (char)prefix;   /* Sign */` |
|    ! 0 | 3598 | `          *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);  /* First digit */` |
|    ! 0 | 3599 | `          if( flag_dp ) *(zBuf++) = '.';     /* Decimal point */` |
|    ! 0 | 3600 | `          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);` |
|    ! 0 | 3601 | `          zBuf--;                            /* point to last digit */` |
|    ! 0 | 3602 | `          if( flag_rtz && flag_dp ){          /* Remove tail zeros */` |
|    ! 0 | 3603 | `            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;` |
|    ! 0 | 3604 | `            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;` |
|    ! 0 | 3605 | `          }` |
|    ! 0 | 3606 | `          zBuf++;                            /* point to next free slot */` |
|    ! 0 | 3607 | `          if( exp \|\| flag_exp ){` |
|    ! 0 | 3608 | `            *(zBuf++) = pInfo->charset[0];` |
|    ! 0 | 3609 | `            if( exp<0 ){ *(zBuf++) = '-'; exp = -exp; } /* sign of exp */` |
|    ! 0 | 3610 | `            else       { *(zBuf++) = '+'; }` |
|    ! 0 | 3611 | `            if( exp>=100 ){` |
|    ! 0 | 3612 | `              *(zBuf++) = (char)((exp/100)+'0');                /* 100's digit */` |
|    ! 0 | 3613 | `              exp %= 100;` |
|    ! 0 | 3614 | `            }` |
|    ! 0 | 3615 | `            *(zBuf++) = (char)(exp/10+'0');                     /* 10's digit */` |
|    ! 0 | 3616 | `            *(zBuf++) = (char)(exp%10+'0');                     /* 1's digit */` |
|    ! 0 | 3617 | `          }` |
|      - | 3618 | `        }` |
|      - | 3619 | `        /* The converted number is in buf[] and zero terminated.Output it.` |
|      - | 3620 | `        ** Note that the number is in the usual order, not reversed as with` |
|      - | 3621 | `        ** integer conversions.*/` |
|     55 | 3622 | `        length = (int)(zBuf-zWorker);` |
|     55 | 3623 | `        zBuf = zWorker;` |
|      - | 3624 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 3625 | `        ** set and we are not left justified */` |
|     55 | 3626 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 3627 | `          int i;` |
|      3 | 3628 | `          int nPad = width - length;` |
|     13 | 3629 | `          for(i=width; i>=nPad; i--){` |
|     11 | 3630 | `            zBuf[i] = zBuf[i-nPad];` |
|      6 | 3631 | `          }` |
|      3 | 3632 | `          i = prefix!=0;` |
|      5 | 3633 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      3 | 3634 | `          length = width;` |
|      1 | 3635 | `        }` |
|      - | 3636 | `#else` |
|      - | 3637 | `         zBuf = " ";` |
|      - | 3638 | `		 length = (int)sizeof(char);` |
|      - | 3639 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|     55 | 3640 | `		 break;` |
|      - | 3641 | `							 }` |
|      1 | 3642 | `		default:` |
|      - | 3643 | `			/* Invalid format specifer */` |
|      3 | 3644 | `			zWorker[0] = '?';` |
|      3 | 3645 | `			length = (int)sizeof(char);` |
|      2 | 3646 | `			break;` |
|      - | 3647 | `		}` |
|      - | 3648 | `		 /*` |
|      - | 3649 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 3650 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 3651 | `		 ** the output.` |
|      - | 3652 | `		 */` |
|    127 | 3653 | `    if( !flag_leftjustify ){` |
|      - | 3654 | `      register int nspace;` |
|    119 | 3655 | `      nspace = width-length;` |
|    119 | 3656 | `      if( nspace>0 ){` |
|      5 | 3657 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 3658 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 3659 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3660 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3661 | `			}` |
|    ! 0 | 3662 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 3663 | `        }` |
|      5 | 3664 | `        if( nspace>0 ){` |
|      5 | 3665 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      5 | 3666 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3667 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3668 | `			}` |
|      2 | 3669 | `		}` |
|      2 | 3670 | `      }` |
|     59 | 3671 | `    }` |
|    127 | 3672 | `    if( length>0 ){` |
|    127 | 3673 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    127 | 3674 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 3675 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3676 | `		}` |
|     63 | 3677 | `    }` |
|    127 | 3678 | `    if( flag_leftjustify ){` |
|      - | 3679 | `      register int nspace;` |
|      9 | 3680 | `      nspace = width-length;` |
|      9 | 3681 | `      if( nspace>0 ){` |
|      9 | 3682 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 3683 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 3684 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3685 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3686 | `			}` |
|    ! 0 | 3687 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 3688 | `        }` |
|      9 | 3689 | `        if( nspace>0 ){` |
|      9 | 3690 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      9 | 3691 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3692 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3693 | `			}` |
|      4 | 3694 | `		}` |
|      4 | 3695 | `      }` |
|      4 | 3696 | `    }` |
|      1 | 3697 | ` }/* for(;;) */` |
|    121 | 3698 | `	return SXRET_OK;` |
|     61 | 3699 |  |
|      - | 3700 | `/*` |
|      - | 3701 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 3702 | ` */` |
|     84 | 3703 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 3704 |  |
|      - | 3705 | `	/* Consume directly */` |
|     85 | 3706 | `	ph7_result_string(pCtx,zInput,nLen);` |
|     42 | 3707 | `	SXUNUSED(pUserData); /* cc warning */` |
|     85 | 3708 | `	return PH7_OK;` |
|      1 | 3709 |  |
|      - | 3710 | `/*` |
|      - | 3711 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 3712 | ` *  Return a formatted string.` |
|      - | 3713 | ` * Parameters` |
|      - | 3714 | ` *  $format` |
|      - | 3715 | ` *    The format string (see block comment above)` |
|      - | 3716 | ` * Return` |
|      - | 3717 | ` *  A string produced according to the formatting string format.` |
|      - | 3718 | ` */` |
|     56 | 3719 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3720 |  |
|      - | 3721 | `	const char *zFormat;` |
|      - | 3722 | `	int nLen;` |
|     57 | 3723 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3724 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 3725 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3726 | `		return PH7_OK;` |
|      - | 3727 | `	}` |
|      - | 3728 | `	/* Extract the string format */` |
|     55 | 3729 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     55 | 3730 | `	if( nLen < 1 ){` |
|      - | 3731 | `		/* Empty string */` |
|    ! 0 | 3732 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3733 | `		return PH7_OK;` |
|      - | 3734 | `	}` |
|      - | 3735 | `	/* Format the string */` |
|     55 | 3736 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,0,FALSE);` |
|     55 | 3737 | `	return PH7_OK;` |
|     29 | 3738 |  |
|      - | 3739 | `/*` |
|      - | 3740 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 3741 | ` */` |
|    110 | 3742 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 3743 |  |
|    111 | 3744 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 3745 | `	/* Call the VM output consumer directly */` |
|    111 | 3746 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 3747 | `	/* Increment counter */` |
|    111 | 3748 | `	*pCounter += nLen;` |
|    111 | 3749 | `	return PH7_OK;` |
|      1 | 3750 |  |
|      - | 3751 | `/*` |
|      - | 3752 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 3753 | ` *  Output a formatted string.` |
|      - | 3754 | ` * Parameters` |
|      - | 3755 | ` *  $format` |
|      - | 3756 | ` *   See sprintf() for a description of format.` |
|      - | 3757 | ` * Return` |
|      - | 3758 | ` *  The length of the outputted string.` |
|      - | 3759 | ` */` |
|     42 | 3760 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3761 |  |
|     43 | 3762 | `	ph7_int64 nCounter = 0;` |
|      - | 3763 | `	const char *zFormat;` |
|      - | 3764 | `	int nLen;` |
|     43 | 3765 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3766 | `		/* Missing/Invalid arguments,return 0 */` |
|      3 | 3767 | `		ph7_result_int(pCtx,0);` |
|      3 | 3768 | `		return PH7_OK;` |
|      - | 3769 | `	}` |
|      - | 3770 | `	/* Extract the string format */` |
|     41 | 3771 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     41 | 3772 | `	if( nLen < 1 ){` |
|      - | 3773 | `		/* Empty string */` |
|    ! 0 | 3774 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3775 | `		return PH7_OK;` |
|      - | 3776 | `	}` |
|      - | 3777 | `	/* Format the string */` |
|     41 | 3778 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 3779 | `	/* Return the length of the outputted string */` |
|     41 | 3780 | `	ph7_result_int64(pCtx,nCounter);` |
|     41 | 3781 | `	return PH7_OK;` |
|     22 | 3782 |  |
|      - | 3783 | `/*` |
|      - | 3784 | ` * int vprintf(string $format,array $args)` |
|      - | 3785 | ` *  Output a formatted string.` |
|      - | 3786 | ` * Parameters` |
|      - | 3787 | ` *  $format` |
|      - | 3788 | ` *   See sprintf() for a description of format.` |
|      - | 3789 | ` * Return` |
|      - | 3790 | ` *  The length of the outputted string.` |
|      - | 3791 | ` */` |
|      2 | 3792 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3793 |  |
|      3 | 3794 | `	ph7_int64 nCounter = 0;` |
|      - | 3795 | `	const char *zFormat;` |
|      - | 3796 | `	ph7_hashmap *pMap;` |
|      - | 3797 | `	SySet sArg;` |
|      - | 3798 | `	int nLen,n;` |
|      3 | 3799 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 3800 | `		/* Missing/Invalid arguments,return 0 */` |
|    ! 0 | 3801 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3802 | `		return PH7_OK;` |
|      - | 3803 | `	}` |
|      - | 3804 | `	/* Extract the string format */` |
|      3 | 3805 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3806 | `	if( nLen < 1 ){` |
|      - | 3807 | `		/* Empty string */` |
|    ! 0 | 3808 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3809 | `		return PH7_OK;` |
|      - | 3810 | `	}` |
|      - | 3811 | `	/* Point to the hashmap */` |
|      3 | 3812 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 3813 | `	/* Extract arguments from the hashmap */` |
|      3 | 3814 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 3815 | `	/* Format the string */` |
|      3 | 3816 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 3817 | `	/* Return the length of the outputted string */` |
|      3 | 3818 | `	ph7_result_int64(pCtx,nCounter);` |
|      - | 3819 | `	/* Release the container */` |
|      3 | 3820 | `	SySetRelease(&sArg);` |
|      3 | 3821 | `	return PH7_OK;` |
|      2 | 3822 |  |
|      - | 3823 | `/*` |
|      - | 3824 | ` * int vsprintf(string $format,array $args)` |
|      - | 3825 | ` *  Output a formatted string.` |
|      - | 3826 | ` * Parameters` |
|      - | 3827 | ` *  $format` |
|      - | 3828 | ` *   See sprintf() for a description of format.` |
|      - | 3829 | ` * Return` |
|      - | 3830 | ` *  A string produced according to the formatting string format.` |
|      - | 3831 | ` */` |
|     10 | 3832 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3833 |  |
|      - | 3834 | `	const char *zFormat;` |
|      - | 3835 | `	ph7_hashmap *pMap;` |
|      - | 3836 | `	SySet sArg;` |
|      - | 3837 | `	int nLen,n;` |
|     11 | 3838 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 3839 | `		/* Missing/Invalid arguments,return the empty string */` |
|      5 | 3840 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 3841 | `		return PH7_OK;` |
|      - | 3842 | `	}` |
|      - | 3843 | `	/* Extract the string format */` |
|      7 | 3844 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 3845 | `	if( nLen < 1 ){` |
|      - | 3846 | `		/* Empty string */` |
|    ! 0 | 3847 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3848 | `		return PH7_OK;` |
|      - | 3849 | `	}` |
|      - | 3850 | `	/* Point to hashmap */` |
|      7 | 3851 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 3852 | `	/* Extract arguments from the hashmap */` |
|      7 | 3853 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 3854 | `	/* Format the string */` |
|      7 | 3855 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),0,TRUE);` |
|      - | 3856 | `	/* Release the container */` |
|      7 | 3857 | `	SySetRelease(&sArg);` |
|      7 | 3858 | `	return PH7_OK;` |
|      6 | 3859 |  |
|      - | 3860 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 3861 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 3862 | `/*` |
|      - | 3863 | ` * Symisc eXtension.` |
|      - | 3864 | ` * string size_format(int64 $size)` |
|      - | 3865 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 3866 | ` *  Example:` |
|      - | 3867 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 3868 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 3869 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 3870 | ` * Parameter` |
|      - | 3871 | ` *  $size` |
|      - | 3872 | ` *    Entity size in bytes.` |
|      - | 3873 | ` * Return` |
|      - | 3874 | ` *   Formatted string representation of the given size.` |
|      - | 3875 | ` */` |
|     24 | 3876 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3877 |  |
|      - | 3878 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 3879 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 3880 | `	sxi32 nRest,i_32;` |
|      - | 3881 | `	ph7_int64 iSize;` |
|     25 | 3882 | `	int c = -1; /* index in zUnit[] */` |
|      - | 3883 |  |
|     25 | 3884 | `	if( nArg < 1 ){` |
|      - | 3885 | `		/* Missing argument,return the empty string */` |
|      3 | 3886 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3887 | `		return PH7_OK;` |
|      - | 3888 | `	}` |
|      - | 3889 | `	/* Extract the given size */` |
|     23 | 3890 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 3891 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 3892 | `		/* Don't bother formatting,return immediately */` |
|      5 | 3893 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 3894 | `		return PH7_OK;` |
|      - | 3895 | `	}` |
|     19 | 3896 | `	for(;;){` |
|     39 | 3897 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 3898 | `		iSize >>= 10;` |
|     39 | 3899 | `		c++;` |
|     39 | 3900 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 3901 | `			break;` |
|      - | 3902 | `		}` |
|      1 | 3903 | `	}` |
|     19 | 3904 | `	nRest /= 100;` |
|     19 | 3905 | `	if( nRest > 9 ){` |
|    ! 0 | 3906 | `		nRest = 9;` |
|    ! 0 | 3907 | `	}` |
|     19 | 3908 | `	if( iSize > 999 ){` |
|    ! 0 | 3909 | `		c++;` |
|    ! 0 | 3910 | `		nRest = 9;` |
|    ! 0 | 3911 | `		iSize = 0;` |
|    ! 0 | 3912 | `	}` |
|     19 | 3913 | `	i_32 = (sxi32)iSize;` |
|      - | 3914 | `	/* Format */` |
|     19 | 3915 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 3916 | `	return PH7_OK;` |
|     13 | 3917 |  |
|      - | 3918 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 3919 | `/*` |
|      - | 3920 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 3921 | ` *   Calculate the md5 hash of a string.` |
|      - | 3922 | ` * Parameter` |
|      - | 3923 | ` *  $str` |
|      - | 3924 | ` *   Input string` |
|      - | 3925 | ` * $raw_output` |
|      - | 3926 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 3927 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 3928 | ` * Return` |
|      - | 3929 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 3930 | ` */` |
|     10 | 3931 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3932 |  |
|      - | 3933 | `	unsigned char zDigest[16];` |
|     11 | 3934 | `	int raw_output = FALSE;` |
|      - | 3935 | `	const void *pIn;` |
|      - | 3936 | `	int nLen;` |
|     11 | 3937 | `	if( nArg < 1 ){` |
|      - | 3938 | `		/* Missing arguments,return the empty string */` |
|      3 | 3939 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3940 | `		return PH7_OK;` |
|      - | 3941 | `	}` |
|      - | 3942 | `	/* Extract the input string */` |
|      9 | 3943 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 3944 | `	if( nLen < 1 ){` |
|      - | 3945 | `		/* Empty string */` |
|    ! 0 | 3946 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3947 | `		return PH7_OK;` |
|      - | 3948 | `	}` |
|      9 | 3949 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      3 | 3950 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      1 | 3951 | `	}` |
|      - | 3952 | `	/* Compute the MD5 digest */` |
|      9 | 3953 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|      9 | 3954 | `	if( raw_output ){` |
|      - | 3955 | `		/* Output raw digest */` |
|      3 | 3956 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      2 | 3957 | `	}else{` |
|      - | 3958 | `		/* Perform a binary to hex conversion */` |
|      7 | 3959 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 3960 | `	}` |
|      9 | 3961 | `	return PH7_OK;` |
|      6 | 3962 |  |
|      - | 3963 | `/*` |
|      - | 3964 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 3965 | ` *   Calculate the sha1 hash of a string.` |
|      - | 3966 | ` * Parameter` |
|      - | 3967 | ` *  $str` |
|      - | 3968 | ` *   Input string` |
|      - | 3969 | ` * $raw_output` |
|      - | 3970 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 3971 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 3972 | ` * Return` |
|      - | 3973 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 3974 | ` */` |
|      8 | 3975 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3976 |  |
|      - | 3977 | `	unsigned char zDigest[20];` |
|      9 | 3978 | `	int raw_output = FALSE;` |
|      - | 3979 | `	const void *pIn;` |
|      - | 3980 | `	int nLen;` |
|      9 | 3981 | `	if( nArg < 1 ){` |
|      - | 3982 | `		/* Missing arguments,return the empty string */` |
|      3 | 3983 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3984 | `		return PH7_OK;` |
|      - | 3985 | `	}` |
|      - | 3986 | `	/* Extract the input string */` |
|      7 | 3987 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 3988 | `	if( nLen < 1 ){` |
|      - | 3989 | `		/* Empty string */` |
|    ! 0 | 3990 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3991 | `		return PH7_OK;` |
|      - | 3992 | `	}` |
|      7 | 3993 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      3 | 3994 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      1 | 3995 | `	}` |
|      - | 3996 | `	/* Compute the SHA1 digest */` |
|      7 | 3997 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|      7 | 3998 | `	if( raw_output ){` |
|      - | 3999 | `		/* Output raw digest */` |
|      3 | 4000 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      2 | 4001 | `	}else{` |
|      - | 4002 | `		/* Perform a binary to hex conversion */` |
|      5 | 4003 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4004 | `	}` |
|      7 | 4005 | `	return PH7_OK;` |
|      5 | 4006 |  |
|      - | 4007 | `/*` |
|      - | 4008 | ` * int64 crc32(string $str)` |
|      - | 4009 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 4010 | ` * Parameter` |
|      - | 4011 | ` *  $str` |
|      - | 4012 | ` *   Input string` |
|      - | 4013 | ` * Return` |
|      - | 4014 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 4015 | ` */` |
|      4 | 4016 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4017 |  |
|      - | 4018 | `	const void *pIn;` |
|      - | 4019 | `	sxu32 nCRC;` |
|      - | 4020 | `	int nLen;` |
|      5 | 4021 | `	if( nArg < 1 ){` |
|      - | 4022 | `		/* Missing arguments,return 0 */` |
|      3 | 4023 | `		ph7_result_int(pCtx,0);` |
|      3 | 4024 | `		return PH7_OK;` |
|      - | 4025 | `	}` |
|      - | 4026 | `	/* Extract the input string */` |
|      3 | 4027 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4028 | `	if( nLen < 1 ){` |
|      - | 4029 | `		/* Empty string */` |
|    ! 0 | 4030 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4031 | `		return PH7_OK;` |
|      - | 4032 | `	}` |
|      - | 4033 | `	/* Calculate the sum */` |
|      3 | 4034 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 4035 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 4036 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 4037 | `	return PH7_OK;` |
|      3 | 4038 |  |
|      - | 4039 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 4040 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 4041 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 4042 | `/*` |
|      - | 4043 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 4044 |  |
|      - | 4045 | ` */` |
|      4 | 4046 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 4047 | `	const char *zInput, /* Raw input */` |
|      - | 4048 | `	int nByte,  /* Input length */` |
|      - | 4049 | `	int delim,  /* Delimiter */` |
|      - | 4050 | `	int encl,   /* Enclosure */` |
|      - | 4051 | `	int escape,  /* Escape character */` |
|      - | 4052 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 4053 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 4054 | `	)` |
|      1 | 4055 |  |
|      5 | 4056 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 4057 | `	const char *zIn = zInput;` |
|      - | 4058 | `	const char *zPtr;` |
|      - | 4059 | `	int isEnc;` |
|      - | 4060 | `	/* Start processing */` |
|      8 | 4061 | `	for(;;){` |
|     17 | 4062 | `		if( zIn >= zEnd ){` |
|      - | 4063 | `			/* No more input to process */` |
|      5 | 4064 | `			break;` |
|      - | 4065 | `		}` |
|     13 | 4066 | `		isEnc = 0;` |
|     13 | 4067 | `		zPtr = zIn;` |
|      - | 4068 | `		/* Find the first delimiter */` |
|     27 | 4069 | `		while( zIn < zEnd ){` |
|     23 | 4070 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 4071 | `				/* Delimiter found,break imediately */` |
|      5 | 4072 | `				break;` |
|     15 | 4073 | `			}else if( zIn[0] == encl ){` |
|      - | 4074 | `				/* Inside enclosure? */` |
|    ! 0 | 4075 | `				isEnc = !isEnc;` |
|     15 | 4076 | `			}else if( zIn[0] == escape ){` |
|      - | 4077 | `				/* Escape sequence */` |
|    ! 0 | 4078 | `				zIn++;` |
|    ! 0 | 4079 | `			}` |
|      - | 4080 | `			/* Advance the cursor */` |
|     15 | 4081 | `			zIn++;` |
|      1 | 4082 | `		}` |
|     13 | 4083 | `		if( zIn > zPtr ){` |
|     13 | 4084 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 4085 | `			sxi32 rc;` |
|      - | 4086 | `			/* Invoke the supllied callback */` |
|     13 | 4087 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 4088 | `				zPtr++;` |
|    ! 0 | 4089 | `				nByteChunk-=2;` |
|    ! 0 | 4090 | `			}` |
|     13 | 4091 | `			if( nByteChunk > 0 ){` |
|     13 | 4092 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 4093 | `				if( rc == SXERR_ABORT ){` |
|      - | 4094 | `					/* User callback request an operation abort */` |
|    ! 0 | 4095 | `					break;` |
|      - | 4096 | `				}` |
|      6 | 4097 | `			}` |
|      6 | 4098 | `		}` |
|      - | 4099 | `		/* Ignore trailing delimiter */` |
|     21 | 4100 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 4101 | `			zIn++;` |
|      1 | 4102 | `		}` |
|      1 | 4103 | `	}` |
|      5 | 4104 | `	return SXRET_OK;` |
|      1 | 4105 |  |
|      - | 4106 | `/*` |
|      - | 4107 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 4108 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 4109 | ` * argument to this callback.` |
|      - | 4110 | ` */` |
|     12 | 4111 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 4112 |  |
|     13 | 4113 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 4114 | `	ph7_value sEntry;` |
|      - | 4115 | `	SyString sToken;` |
|      - | 4116 | `	/* Insert the token in the given array */` |
|     13 | 4117 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 4118 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 4119 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 4120 | `	if( sToken.nByte < 1){` |
|    ! 0 | 4121 | `		return SXRET_OK;` |
|      - | 4122 | `	}` |
|     13 | 4123 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 4124 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 4125 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 4126 | `	return SXRET_OK;` |
|      7 | 4127 |  |
|      - | 4128 | `/*` |
|      - | 4129 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 4130 | ` *  Parse a CSV string into an array.` |
|      - | 4131 | ` * Parameters` |
|      - | 4132 | ` *  $input` |
|      - | 4133 | ` *   The string to parse.` |
|      - | 4134 | ` *  $delimiter` |
|      - | 4135 | ` *   Set the field delimiter (one character only).` |
|      - | 4136 | ` *  $enclosure` |
|      - | 4137 | ` *   Set the field enclosure character (one character only).` |
|      - | 4138 | ` *  $escape` |
|      - | 4139 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 4140 | ` * Return` |
|      - | 4141 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 4142 | ` */` |
|      4 | 4143 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4144 |  |
|      - | 4145 | `	const char *zInput,*zPtr;` |
|      - | 4146 | `	ph7_value *pArray;` |
|      5 | 4147 | `	int delim  = ',';   /* Delimiter */` |
|      5 | 4148 | `	int encl   = '"' ;  /* Enclosure */` |
|      5 | 4149 | `	int escape = '\\';  /* Escape character */` |
|      - | 4150 | `	int nLen;` |
|      5 | 4151 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4152 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 4153 | `		ph7_result_null(pCtx);` |
|      3 | 4154 | `		return PH7_OK;` |
|      - | 4155 | `	}` |
|      - | 4156 | `	/* Extract the raw input */` |
|      3 | 4157 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4158 | `	if( nArg > 1 ){` |
|      - | 4159 | `		int i;` |
|      3 | 4160 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 4161 | `			/* Extract the delimiter */` |
|      3 | 4162 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 4163 | `			if( i > 0 ){` |
|      3 | 4164 | `				delim = zPtr[0];` |
|      1 | 4165 | `			}` |
|      1 | 4166 | `		}` |
|      3 | 4167 | `		if( nArg > 2 ){` |
|      3 | 4168 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 4169 | `				/* Extract the enclosure */` |
|      3 | 4170 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 4171 | `				if( i > 0 ){` |
|      3 | 4172 | `					encl = zPtr[0];` |
|      1 | 4173 | `				}` |
|      1 | 4174 | `			}` |
|      3 | 4175 | `			if( nArg > 3 ){` |
|      3 | 4176 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 4177 | `					/* Extract the escape character */` |
|      3 | 4178 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 4179 | `					if( i > 0 ){` |
|      3 | 4180 | `						escape = zPtr[0];` |
|      1 | 4181 | `					}` |
|      1 | 4182 | `				}` |
|      1 | 4183 | `			}` |
|      1 | 4184 | `		}` |
|      1 | 4185 | `	}` |
|      - | 4186 | `	/* Create our array */` |
|      3 | 4187 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4188 | `	if( pArray == 0 ){` |
|    ! 0 | 4189 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 4190 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4191 | `		return PH7_OK;` |
|      - | 4192 | `	}` |
|      - | 4193 | `	/* Parse the raw input */` |
|      3 | 4194 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 4195 | `	/* Return the freshly created array */` |
|      3 | 4196 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4197 | `	return PH7_OK;` |
|      3 | 4198 |  |
|      - | 4199 | `/*` |
|      - | 4200 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 4201 | ` * container.` |
|      - | 4202 | ` * Refer to [strip_tags()].` |
|      - | 4203 | ` */` |
|     10 | 4204 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 4205 |  |
|     11 | 4206 | `	const char *zEnd = &zTag[nByte];` |
|      - | 4207 | `	const char *zPtr;` |
|      - | 4208 | `	SyString sEntry;` |
|      - | 4209 | `	/* Strip tags */` |
|     10 | 4210 | `	for(;;){` |
|     45 | 4211 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 4212 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 4213 | `				zTag++;` |
|      1 | 4214 | `		}` |
|     21 | 4215 | `		if( zTag >= zEnd ){` |
|     11 | 4216 | `			break;` |
|      - | 4217 | `		}` |
|     11 | 4218 | `		zPtr = zTag;` |
|      - | 4219 | `		/* Delimit the tag */` |
|     25 | 4220 | `		while(zTag < zEnd ){` |
|     25 | 4221 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 4222 | `				/* UTF-8 stream */` |
|      3 | 4223 | `				zTag++;` |
|      5 | 4224 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 4225 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 4226 | `				break;` |
|    ! 0 | 4227 | `			}else{` |
|     13 | 4228 | `				zTag++;` |
|      - | 4229 | `			}` |
|      1 | 4230 | `		}` |
|     11 | 4231 | `		if( zTag > zPtr ){` |
|      - | 4232 | `			/* Perform the insertion */` |
|     11 | 4233 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 4234 | `			SyStringFullTrim(&sEntry);` |
|     11 | 4235 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 4236 | `		}` |
|      - | 4237 | `		/* Jump the trailing '>' */` |
|     11 | 4238 | `		zTag++;` |
|      1 | 4239 | `	}` |
|     11 | 4240 | `	return SXRET_OK;` |
|      1 | 4241 |  |
|      - | 4242 | `/*` |
|      - | 4243 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 4244 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 4245 | ` * Refer to [strip_tags()].` |
|      - | 4246 | ` */` |
|     36 | 4247 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 4248 |  |
|     37 | 4249 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 4250 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 4251 | `		SyString sTag;` |
|     85 | 4252 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 4253 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 4254 | `			zTag++;` |
|      1 | 4255 | `		}` |
|      - | 4256 | `		/* Delimit the tag */` |
|     25 | 4257 | `		zCur = zTag;` |
|     77 | 4258 | `		while(zTag < zEnd ){` |
|     77 | 4259 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 4260 | `				/* UTF-8 stream */` |
|      5 | 4261 | `				zTag++;` |
|      9 | 4262 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 4263 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 4264 | `				break;` |
|    ! 0 | 4265 | `			}else{` |
|     49 | 4266 | `				zTag++;` |
|      - | 4267 | `			}` |
|      1 | 4268 | `		}` |
|     25 | 4269 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 4270 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 4271 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 4272 | `		if( sTag.nByte > 0 ){` |
|      - | 4273 | `			SyString *aEntry,*pEntry;` |
|      - | 4274 | `			sxi32 rc;` |
|      - | 4275 | `			sxu32 n;` |
|      - | 4276 | `			/* Perform the lookup */` |
|     25 | 4277 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 4278 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 4279 | `				pEntry = &aEntry[n];` |
|      - | 4280 | `				/* Do the comparison */` |
|     25 | 4281 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 4282 | `				if( !rc ){` |
|     21 | 4283 | `					return SXRET_OK;` |
|      - | 4284 | `				}` |
|      3 | 4285 | `			}` |
|      2 | 4286 | `		}` |
|      2 | 4287 | `	}` |
|      - | 4288 | `	/* No such tag */` |
|     17 | 4289 | `	return SXERR_NOTFOUND;` |
|     19 | 4290 |  |
|      - | 4291 | `/*` |
|      - | 4292 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 4293 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 4294 | ` * Refer to [strip_tags()].` |
|      - | 4295 | ` */` |
|     16 | 4296 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 4297 |  |
|     17 | 4298 | `	const char *zEnd = &zIn[nByte];` |
|      - | 4299 | `	const char *zPtr,*zTag;` |
|      - | 4300 | `	SySet sSet;` |
|      - | 4301 | `	/* initialize the set of allowed tags */` |
|     17 | 4302 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 4303 | `	if( nTaglen > 0 ){` |
|      - | 4304 | `		/* Set of allowed tags */` |
|     11 | 4305 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 4306 | `	}` |
|      - | 4307 | `	/* Set the empty string */` |
|     17 | 4308 | `	ph7_result_string(pCtx,"",0);` |
|      - | 4309 | `	/* Start processing */` |
|     26 | 4310 | `	for(;;){` |
|     53 | 4311 | `		if(zIn >= zEnd){` |
|      - | 4312 | `			/* No more input to process */` |
|     15 | 4313 | `			break;` |
|      - | 4314 | `		}` |
|     39 | 4315 | `		zPtr = zIn;` |
|      - | 4316 | `		/* Find a tag */` |
|    133 | 4317 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 4318 | `			zIn++;` |
|      1 | 4319 | `		}` |
|     39 | 4320 | `		if( zIn > zPtr ){` |
|      - | 4321 | `			/* Consume raw input */` |
|     21 | 4322 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 4323 | `		}` |
|      - | 4324 | `		/* Ignore trailing null bytes */` |
|     39 | 4325 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 4326 | `			zIn++;` |
|    ! 0 | 4327 | `		}` |
|     39 | 4328 | `		if(zIn >= zEnd){` |
|      - | 4329 | `			/* No more input to process */` |
|      3 | 4330 | `			break;` |
|      - | 4331 | `		}` |
|     37 | 4332 | `		if( zIn[0] == '<' ){` |
|      - | 4333 | `			sxi32 rc;` |
|     37 | 4334 | `			zTag = zIn++;` |
|      - | 4335 | `			/* Delimit the tag */` |
|    127 | 4336 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 4337 | `				zIn++;` |
|      1 | 4338 | `			}` |
|     37 | 4339 | `			if( zIn < zEnd ){` |
|     37 | 4340 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 4341 | `			}` |
|      - | 4342 | `			/* Query the set */` |
|     37 | 4343 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 4344 | `			if( rc == SXRET_OK ){` |
|      - | 4345 | `				/* Keep the tag */` |
|     21 | 4346 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 4347 | `			}` |
|     18 | 4348 | `		}` |
|      1 | 4349 | `	}` |
|      - | 4350 | `	/* Cleanup */` |
|     17 | 4351 | `	SySetRelease(&sSet);` |
|     17 | 4352 | `	return SXRET_OK;` |
|      1 | 4353 |  |
|      - | 4354 | `/*` |
|      - | 4355 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 4356 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 4357 | ` * Parameters` |
|      - | 4358 | ` *  $str` |
|      - | 4359 | ` *  The input string.` |
|      - | 4360 | ` * $allowable_tags` |
|      - | 4361 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 4362 | ` * Return` |
|      - | 4363 | ` *  Returns the stripped string.` |
|      - | 4364 | ` */` |
|     16 | 4365 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4366 |  |
|     17 | 4367 | `	const char *zTaglist = 0;` |
|      - | 4368 | `	const char *zString;` |
|     17 | 4369 | `	int nTaglen = 0;` |
|      - | 4370 | `	int nLen;` |
|     17 | 4371 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 4372 | `		/* Missing/Invalid arguments,return the empty string */` |
|      3 | 4373 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4374 | `		return PH7_OK;` |
|      - | 4375 | `	}` |
|      - | 4376 | `	/* Point to the raw string */` |
|     15 | 4377 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 4378 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 4379 | `		/* Allowed tag */` |
|     11 | 4380 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 4381 | `	}` |
|      - | 4382 | `	/* Process input */` |
|     15 | 4383 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 4384 | `	return PH7_OK;` |
|      9 | 4385 |  |
|      - | 4386 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 4387 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 4388 | `/*` |
|      - | 4389 | ` * string str_shuffle(string $str)` |
|      - | 4390 |  |
|      - | 4391 | ` *  Randomly shuffles a string.` |
|      - | 4392 | ` * Parameters` |
|      - | 4393 | ` *  $str` |
|      - | 4394 | ` *   The input string.` |
|      - | 4395 | ` * Return` |
|      - | 4396 | ` *  Returns the shuffled string.` |
|      - | 4397 | ` */` |
|     12 | 4398 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4399 |  |
|      - | 4400 | `	const char *zString;` |
|      - | 4401 | `	int nLen,i,c;` |
|      - | 4402 | `	sxu32 iR;` |
|     13 | 4403 | `	if( nArg < 1 ){` |
|      - | 4404 | `		/* Missing arguments,return the empty string */` |
|      3 | 4405 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4406 | `		return PH7_OK;` |
|      - | 4407 | `	}` |
|      - | 4408 | `	/* Extract the target string */` |
|     11 | 4409 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 4410 | `	if( nLen < 1 ){` |
|      - | 4411 | `		/* Nothing to shuffle */` |
|      3 | 4412 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4413 | `		return PH7_OK;` |
|      - | 4414 | `	}` |
|      - | 4415 | `	/* Shuffle the string */` |
|     43 | 4416 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 4417 | `		/* Generate a random number first */` |
|     35 | 4418 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 4419 | `		/* Extract a random offset */` |
|     35 | 4420 | `		c = zString[iR % nLen];` |
|      - | 4421 | `		/* Append it */` |
|     35 | 4422 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 4423 | `	}` |
|      9 | 4424 | `	return PH7_OK;` |
|      7 | 4425 |  |
|      - | 4426 | `/*` |
|      - | 4427 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 4428 | ` *  Convert a string to an array.` |
|      - | 4429 | ` * Parameters` |
|      - | 4430 | ` * $string` |
|      - | 4431 | ` *  The input string.` |
|      - | 4432 | ` * $split_length` |
|      - | 4433 | ` *  Maximum length of the chunk.` |
|      - | 4434 | ` * Return` |
|      - | 4435 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 4436 | ` *  except possibly the last one which may be shorter.` |
|      - | 4437 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 4438 | ` *  as the first (and only) array element.` |
|      - | 4439 | ` *  An empty string returns an empty array.` |
|      - | 4440 | ` * Errors` |
|      - | 4441 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 4442 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 4443 | ` *  ValueError if $split_length is less than 1.` |
|      - | 4444 | ` */` |
|     28 | 4445 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4446 |  |
|      - | 4447 | `	const char *zString,*zEnd;` |
|      - | 4448 | `	ph7_value *pArray,*pValue;` |
|      - | 4449 | `	int split_len;` |
|      - | 4450 | `	int nLen;` |
|     30 | 4451 | `	if( nArg < 1 ){` |
|      4 | 4452 | `		return PH7_VmThrowException(pCtx,` |
|      - | 4453 | `			"ArgumentCountError",` |
|      - | 4454 | `			"str_split() expects at least 1 argument, %d given",` |
|      1 | 4455 | `			nArg` |
|      - | 4456 | `			);` |
|      - | 4457 | `	}` |
|      - | 4458 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     50 | 4459 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     38 | 4460 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     24 | 4461 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 | 4462 | `		return PH7_VmThrowException(pCtx,` |
|      - | 4463 | `			"TypeError",` |
|      - | 4464 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 4465 | `			ph7_type_name(apArg[0])` |
|      - | 4466 | `			);` |
|      - | 4467 | `	}` |
|      - | 4468 | `	/* Point to the target string */` |
|     26 | 4469 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     26 | 4470 | `	split_len = (int)sizeof(char);` |
|     26 | 4471 | `	if( nArg > 1 ){` |
|      - | 4472 | `		/* Split length */` |
|     16 | 4473 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     16 | 4474 | `		if( split_len < 1 ){` |
|      5 | 4475 | `			return PH7_VmThrowException(pCtx,` |
|      - | 4476 | `				"ValueError",` |
|      - | 4477 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 4478 | `				);` |
|      - | 4479 | `		}` |
|     11 | 4480 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 4481 | `			split_len = nLen;` |
|      1 | 4482 | `		}` |
|      5 | 4483 | `	}` |
|      - | 4484 | `	/* Create the array and the scalar value */` |
|     21 | 4485 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 4486 | `	/*Chunk value */` |
|     21 | 4487 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     21 | 4488 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 4489 | `		/* Return FALSE */` |
|    ! 0 | 4490 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4491 | `		return PH7_OK;` |
|      - | 4492 | `	}` |
|      - | 4493 | `	/* Point to the end of the string */` |
|     21 | 4494 | `	zEnd = &zString[nLen];` |
|      - | 4495 | `	/* Perform the requested operation */` |
|     48 | 4496 | `	for(;;){` |
|      - | 4497 | `		int nMax;` |
|     59 | 4498 | `		if( zString >= zEnd ){` |
|      - | 4499 | `			/* No more input to process */` |
|     21 | 4500 | `			break;` |
|      - | 4501 | `		}` |
|     39 | 4502 | `		nMax = (int)(zEnd-zString);` |
|     39 | 4503 | `		if( nMax < split_len ){` |
|      3 | 4504 | `			split_len = nMax;` |
|      1 | 4505 | `		}` |
|      - | 4506 | `		/* Copy the current chunk */` |
|     39 | 4507 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 4508 | `		/* Insert it */` |
|     39 | 4509 | `		ph7_array_add_elem(pArray,0,pValue); /* Will make it's own copy */` |
|      - | 4510 | `		/* reset the string cursor */` |
|     39 | 4511 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 4512 | `		/* Update position */` |
|     39 | 4513 | `		zString += split_len;` |
|      1 | 4514 | `	}` |
|      - | 4515 | `	/*` |
|      - | 4516 | `	 * Return the array.` |
|      - | 4517 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 4518 | `	 * upon we return from this function.` |
|      - | 4519 | `	 */` |
|     21 | 4520 | `	ph7_result_value(pCtx,pArray);` |
|     21 | 4521 | `	return PH7_OK;` |
|     16 | 4522 |  |
|      - | 4523 | `/*` |
|      - | 4524 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 4525 | ` * Refer to [strspn()].` |
|      - | 4526 | ` */` |
|     28 | 4527 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 4528 |  |
|     29 | 4529 | `	const char *zIn = *pzIn;` |
|      - | 4530 | `	const char *zPtr;` |
|      - | 4531 | `	/* Ignore leading white spaces */` |
|     29 | 4532 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 4533 | `		zIn++;` |
|    ! 0 | 4534 | `	}` |
|     29 | 4535 | `	if( zIn >= zEnd ){` |
|      - | 4536 | `		/* End of input */` |
|    ! 0 | 4537 | `		return SXERR_EOF;` |
|      - | 4538 | `	}` |
|     29 | 4539 | `	zPtr = zIn;` |
|      - | 4540 | `	/* Extract the token */` |
|    201 | 4541 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 4542 | `		zIn++;` |
|      1 | 4543 | `	}` |
|     29 | 4544 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 4545 | `	/* Synchronize pointers */` |
|     29 | 4546 | `	*pzIn = zIn;` |
|      - | 4547 | `	/* Return to the caller */` |
|     29 | 4548 | `	return SXRET_OK;` |
|     15 | 4549 |  |
|      - | 4550 | `/*` |
|      - | 4551 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 4552 | ` * return the longest match.` |
|      - | 4553 | ` * Refer to [strspn()].` |
|      - | 4554 | ` */` |
|     18 | 4555 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 4556 |  |
|     19 | 4557 | `	const char *zEnd = &zString[nLen];` |
|     19 | 4558 | `	const char *zIn = zString;` |
|      - | 4559 | `	int i,c;` |
|     45 | 4560 | `	for(;;){` |
|     91 | 4561 | `		if( zString >= zEnd ){` |
|      7 | 4562 | `			break;` |
|      - | 4563 | `		}` |
|      - | 4564 | `		/* Extract current character */` |
|     85 | 4565 | `		c = zString[0];` |
|      - | 4566 | `		/* Perform the lookup */` |
|    383 | 4567 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 4568 | `			if( c == zMask[i] ){` |
|      - | 4569 | `				/* Character found */` |
|     73 | 4570 | `				break;` |
|      - | 4571 | `			}` |
|    150 | 4572 | `		}` |
|     85 | 4573 | `		if( i >= nMaskLen ){` |
|      - | 4574 | `			/* Character not in the current mask,break immediately */` |
|     13 | 4575 | `			break;` |
|      - | 4576 | `		}` |
|      - | 4577 | `		/* Advance cursor */` |
|     73 | 4578 | `		zString++;` |
|      1 | 4579 | `	}` |
|      - | 4580 | `	/* Longest match */` |
|     19 | 4581 | `	return (int)(zString-zIn);` |
|      1 | 4582 |  |
|      - | 4583 | `/*` |
|      - | 4584 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 4585 | ` * Refer to [strcspn()].` |
|      - | 4586 | ` */` |
|     10 | 4587 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 4588 |  |
|     11 | 4589 | `	const char *zEnd = &zString[nLen];` |
|     11 | 4590 | `	const char *zIn = zString;` |
|      - | 4591 | `	int i,c;` |
|     12 | 4592 | `	for(;;){` |
|     25 | 4593 | `		if( zString >= zEnd ){` |
|      3 | 4594 | `			break;` |
|      - | 4595 | `		}` |
|      - | 4596 | `		/* Extract current character */` |
|     23 | 4597 | `		c = zString[0];` |
|      - | 4598 | `		/* Perform the lookup */` |
|     51 | 4599 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 4600 | `			if( c == zMask[i] ){` |
|      9 | 4601 | `				break;` |
|      - | 4602 | `			}` |
|     15 | 4603 | `		}` |
|     23 | 4604 | `		if( i < nMaskLen ){` |
|      - | 4605 | `			/* Character in the current mask,break immediately */` |
|      9 | 4606 | `			break;` |
|      - | 4607 | `		}` |
|      - | 4608 | `		/* Advance cursor */` |
|     15 | 4609 | `		zString++;` |
|      1 | 4610 | `	}` |
|      - | 4611 | `	/* Longest match */` |
|     11 | 4612 | `	return (int)(zString-zIn);` |
|      1 | 4613 |  |
|      - | 4614 | `/*` |
|      - | 4615 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 4616 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 4617 | ` *  of characters contained within a given mask.` |
|      - | 4618 | ` * Parameters` |
|      - | 4619 | ` * $str` |
|      - | 4620 | ` *  The input string.` |
|      - | 4621 | ` * $mask` |
|      - | 4622 | ` *  The list of allowable characters.` |
|      - | 4623 | ` * $start` |
|      - | 4624 | ` *  The position in subject to start searching.` |
|      - | 4625 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 4626 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 4627 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 4628 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 4629 | ` *  start'th position from the end of subject.` |
|      - | 4630 | ` * $length` |
|      - | 4631 | ` *  The length of the segment from subject to examine.` |
|      - | 4632 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 4633 | ` *  characters after the starting position.` |
|      - | 4634 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 4635 | ` *  position up to length characters from the end of subject.` |
|      - | 4636 | ` * Return` |
|      - | 4637 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 4638 | ` * in mask.` |
|      - | 4639 | ` */` |
|     26 | 4640 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4641 |  |
|      - | 4642 | `	const char *zString,*zMask,*zEnd;` |
|      - | 4643 | `	int iMasklen,iLen;` |
|      - | 4644 | `	SyString sToken;` |
|     27 | 4645 | `	int iCount = 0;` |
|      - | 4646 | `	int rc;` |
|     27 | 4647 | `	if( nArg < 2 ){` |
|      - | 4648 | `		/* Missing agruments,return zero */` |
|      3 | 4649 | `		ph7_result_int(pCtx,0);` |
|      3 | 4650 | `		return PH7_OK;` |
|      - | 4651 | `	}` |
|      - | 4652 | `	/* Extract the target string */` |
|     25 | 4653 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 4654 | `	/* Extract the mask */` |
|     25 | 4655 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 4656 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 4657 | `		/* Nothing to process,return zero */` |
|      7 | 4658 | `		ph7_result_int(pCtx,0);` |
|      7 | 4659 | `		return PH7_OK;` |
|      - | 4660 | `	}` |
|     19 | 4661 | `	if( nArg > 2 ){` |
|      - | 4662 | `		int nOfft;` |
|      - | 4663 | `		/* Extract the offset */` |
|      9 | 4664 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 4665 | `		if( nOfft < 0 ){` |
|    ! 0 | 4666 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 4667 | `			if( zBase > zString ){` |
|    ! 0 | 4668 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 4669 | `				zString = zBase;` |
|    ! 0 | 4670 | `			}else{` |
|      - | 4671 | `				/* Invalid offset */` |
|    ! 0 | 4672 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 4673 | `				return PH7_OK;` |
|      - | 4674 | `			}` |
|    ! 0 | 4675 | `		}else{` |
|      9 | 4676 | `			if( nOfft >= iLen ){` |
|      - | 4677 | `				/* Invalid offset */` |
|    ! 0 | 4678 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 4679 | `				return PH7_OK;` |
|    ! 0 | 4680 | `			}else{` |
|      - | 4681 | `				/* Update offset */` |
|      9 | 4682 | `				zString += nOfft;` |
|      9 | 4683 | `				iLen -= nOfft;` |
|      - | 4684 | `			}` |
|      - | 4685 | `		}` |
|      9 | 4686 | `		if( nArg > 3 ){` |
|      - | 4687 | `			int iUserlen;` |
|      - | 4688 | `			/* Extract the desired length */` |
|      9 | 4689 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 4690 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 4691 | `				iLen = iUserlen;` |
|      2 | 4692 | `			}` |
|      4 | 4693 | `		}` |
|      4 | 4694 | `	}` |
|      - | 4695 | `	/* Point to the end of the string */` |
|     19 | 4696 | `	zEnd = &zString[iLen];` |
|      - | 4697 | `	/* Extract the first non-space token */` |
|     19 | 4698 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 4699 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 4700 | `		/* Compare against the current mask */` |
|     19 | 4701 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 4702 | `	}` |
|      - | 4703 | `	/* Longest match */` |
|     19 | 4704 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 4705 | `	return PH7_OK;` |
|     14 | 4706 |  |
|      - | 4707 | `/*` |
|      - | 4708 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 4709 | ` *  Find length of initial segment not matching mask.` |
|      - | 4710 | ` * Parameters` |
|      - | 4711 | ` * $str` |
|      - | 4712 | ` *  The input string.` |
|      - | 4713 | ` * $mask` |
|      - | 4714 | ` *  The list of not allowed characters.` |
|      - | 4715 | ` * $start` |
|      - | 4716 | ` *  The position in subject to start searching.` |
|      - | 4717 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 4718 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 4719 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 4720 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 4721 | ` *  start'th position from the end of subject.` |
|      - | 4722 | ` * $length` |
|      - | 4723 | ` *  The length of the segment from subject to examine.` |
|      - | 4724 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 4725 | ` *  characters after the starting position.` |
|      - | 4726 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 4727 | ` *  position up to length characters from the end of subject.` |
|      - | 4728 | ` * Return` |
|      - | 4729 | ` *  Returns the length of the segment as an integer.` |
|      - | 4730 | ` */` |
|     16 | 4731 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4732 |  |
|      - | 4733 | `	const char *zString,*zMask,*zEnd;` |
|      - | 4734 | `	int iMasklen,iLen;` |
|      - | 4735 | `	SyString sToken;` |
|     17 | 4736 | `	int iCount = 0;` |
|      - | 4737 | `	int rc;` |
|     17 | 4738 | `	if( nArg < 2 ){` |
|      - | 4739 | `		/* Missing agruments,return zero */` |
|      3 | 4740 | `		ph7_result_int(pCtx,0);` |
|      3 | 4741 | `		return PH7_OK;` |
|      - | 4742 | `	}` |
|      - | 4743 | `	/* Extract the target string */` |
|     15 | 4744 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 4745 | `	/* Extract the mask */` |
|     15 | 4746 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 4747 | `	if( iLen < 1 ){` |
|      - | 4748 | `		/* Nothing to process,return zero */` |
|    ! 0 | 4749 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4750 | `		return PH7_OK;` |
|      - | 4751 | `	}` |
|     15 | 4752 | `	if( iMasklen < 1 ){` |
|      - | 4753 | `		/* No given mask,return the string length */` |
|      3 | 4754 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 4755 | `		return PH7_OK;` |
|      - | 4756 | `	}` |
|     13 | 4757 | `	if( nArg > 2 ){` |
|      - | 4758 | `		int nOfft;` |
|      - | 4759 | `		/* Extract the offset */` |
|     11 | 4760 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 4761 | `		if( nOfft < 0 ){` |
|    ! 0 | 4762 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 4763 | `			if( zBase > zString ){` |
|    ! 0 | 4764 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 4765 | `				zString = zBase;` |
|    ! 0 | 4766 | `			}else{` |
|      - | 4767 | `				/* Invalid offset */` |
|    ! 0 | 4768 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 4769 | `				return PH7_OK;` |
|      - | 4770 | `			}` |
|    ! 0 | 4771 | `		}else{` |
|     11 | 4772 | `			if( nOfft >= iLen ){` |
|      - | 4773 | `				/* Invalid offset */` |
|      3 | 4774 | `				ph7_result_int(pCtx,0);` |
|      3 | 4775 | `				return PH7_OK;` |
|    ! 0 | 4776 | `			}else{` |
|      - | 4777 | `				/* Update offset */` |
|      9 | 4778 | `				zString += nOfft;` |
|      9 | 4779 | `				iLen -= nOfft;` |
|      - | 4780 | `			}` |
|      - | 4781 | `		}` |
|      9 | 4782 | `		if( nArg > 3 ){` |
|      - | 4783 | `			int iUserlen;` |
|      - | 4784 | `			/* Extract the desired length */` |
|    ! 0 | 4785 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 4786 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 4787 | `				iLen = iUserlen;` |
|    ! 0 | 4788 | `			}` |
|    ! 0 | 4789 | `		}` |
|      4 | 4790 | `	}` |
|      - | 4791 | `	/* Point to the end of the string */` |
|     11 | 4792 | `	zEnd = &zString[iLen];` |
|      - | 4793 | `	/* Extract the first non-space token */` |
|     11 | 4794 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 4795 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 4796 | `		/* Compare against the current mask */` |
|     11 | 4797 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 4798 | `	}` |
|      - | 4799 | `	/* Longest match */` |
|     11 | 4800 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 4801 | `	return PH7_OK;` |
|      9 | 4802 |  |
|      - | 4803 | `/*` |
|      - | 4804 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 4805 | ` *  Search a string for any of a set of characters.` |
|      - | 4806 | ` * Parameters` |
|      - | 4807 | ` *  $haystack` |
|      - | 4808 | ` *   The string where char_list is looked for.` |
|      - | 4809 | ` *  $char_list` |
|      - | 4810 | ` *   This parameter is case sensitive.` |
|      - | 4811 | ` * Return` |
|      - | 4812 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 4813 | ` */` |
|      6 | 4814 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4815 |  |
|      - | 4816 | `	const char *zString,*zList,*zEnd;` |
|      - | 4817 | `	int iLen,iListLen,i,c;` |
|      - | 4818 | `	sxu32 nOfft,nMax;` |
|      - | 4819 | `	sxi32 rc;` |
|      7 | 4820 | `	if( nArg < 2 ){` |
|      - | 4821 | `		/* Missing arguments,return FALSE */` |
|      3 | 4822 | `		ph7_result_bool(pCtx,0);` |
|      3 | 4823 | `		return PH7_OK;` |
|      - | 4824 | `	}` |
|      - | 4825 | `	/* Extract the haystack and the char list */` |
|      5 | 4826 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 4827 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 4828 | `	if( iLen < 1 ){` |
|      - | 4829 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 4830 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4831 | `		return PH7_OK;` |
|      - | 4832 | `	}` |
|      - | 4833 | `	/* Point to the end of the string */` |
|      5 | 4834 | `	zEnd = &zString[iLen];` |
|      5 | 4835 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 4836 | `	/* perform the requested operation */` |
|     15 | 4837 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 4838 | `		c = zList[i];` |
|     11 | 4839 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 4840 | `		if( rc == SXRET_OK ){` |
|      5 | 4841 | `			if( nMax < nOfft ){` |
|      3 | 4842 | `				nOfft = nMax;` |
|      1 | 4843 | `			}` |
|      2 | 4844 | `		}` |
|      6 | 4845 | `	}` |
|      5 | 4846 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 4847 | `		/* No such substring,return FALSE */` |
|      3 | 4848 | `		ph7_result_bool(pCtx,0);` |
|      2 | 4849 | `	}else{` |
|      - | 4850 | `		/* Return the substring */` |
|      3 | 4851 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 4852 | `	}` |
|      5 | 4853 | `	return PH7_OK;` |
|      4 | 4854 |  |
|      - | 4855 | `/*` |
|      - | 4856 | ` * string soundex(string $str)` |
|      - | 4857 | ` *  Calculate the soundex key of a string.` |
|      - | 4858 | ` * Parameters` |
|      - | 4859 | ` *  $str` |
|      - | 4860 | ` *   The input string.` |
|      - | 4861 | ` * Return` |
|      - | 4862 | ` *  Returns the soundex key as a string.` |
|      - | 4863 | ` * Note:` |
|      - | 4864 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 4865 | ` * source tree.` |
|      - | 4866 | ` */` |
|     20 | 4867 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4868 |  |
|      - | 4869 | `	const unsigned char *zIn;` |
|      - | 4870 | `	char zResult[8];` |
|      - | 4871 | `	int i, j;` |
|      - | 4872 | `	static const unsigned char iCode[] = {` |
|      - | 4873 |  |
|      - | 4874 |  |
|      - | 4875 |  |
|      - | 4876 |  |
|      - | 4877 |  |
|      - | 4878 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 4879 |  |
|      - | 4880 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 4881 | `	};` |
|     21 | 4882 | `	if( nArg < 1 ){` |
|      - | 4883 | `		/* Missing arguments,return the empty string */` |
|      3 | 4884 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4885 | `		return PH7_OK;` |
|      - | 4886 | `	}` |
|     19 | 4887 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     19 | 4888 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     19 | 4889 | `	if( zIn[i] ){` |
|     17 | 4890 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 4891 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 4892 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 4893 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 4894 | `			if( code>0 ){` |
|     45 | 4895 | `				if( code!=prevcode ){` |
|     33 | 4896 | `					prevcode = (unsigned char)code;` |
|     33 | 4897 | `					zResult[j++] = (char)code + '0';` |
|     16 | 4898 | `				}` |
|     23 | 4899 | `			}else{` |
|     49 | 4900 | `				prevcode = 0;` |
|      - | 4901 | `			}` |
|     47 | 4902 | `		}` |
|     33 | 4903 | `		while( j<4 ){` |
|     17 | 4904 | `			zResult[j++] = '0';` |
|      1 | 4905 | `		}` |
|     17 | 4906 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 4907 | `	}else{` |
|      3 | 4908 | `	  ph7_result_string(pCtx,"?000",4);` |
|      - | 4909 | `	}` |
|     19 | 4910 | `	return PH7_OK;` |
|     11 | 4911 |  |
|      - | 4912 | `/*` |
|      - | 4913 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 4914 | ` *  Wraps a string to a given number of characters.` |
|      - | 4915 | ` * Parameters` |
|      - | 4916 | ` *  $str` |
|      - | 4917 | ` *   The input string.` |
|      - | 4918 | ` * $width` |
|      - | 4919 | ` *  The column width.` |
|      - | 4920 | ` * $break` |
|      - | 4921 | ` *  The line is broken using the optional break parameter.` |
|      - | 4922 | ` * Return` |
|      - | 4923 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 4924 | ` */` |
|     14 | 4925 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4926 |  |
|      - | 4927 | `	const char *zIn,*zEnd,*zBreak;` |
|      - | 4928 | `	int iLen,iBreaklen,iChunk;` |
|     15 | 4929 | `	if( nArg < 1 ){` |
|      - | 4930 | `		/* Missing arguments,return the empty string */` |
|      3 | 4931 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4932 | `		return PH7_OK;` |
|      - | 4933 | `	}` |
|      - | 4934 | `	/* Extract the input string */` |
|     13 | 4935 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|     13 | 4936 | `	if( iLen < 1 ){` |
|      - | 4937 | `		/* Nothing to process,return the empty string */` |
|      3 | 4938 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4939 | `		return PH7_OK;` |
|      - | 4940 | `	}` |
|      - | 4941 | `	/* Chunk length */` |
|     11 | 4942 | `	iChunk = 75;` |
|     11 | 4943 | `	iBreaklen = 0;` |
|     11 | 4944 | `	zBreak = ""; /* cc warning */` |
|     11 | 4945 | `	if( nArg > 1 ){` |
|     11 | 4946 | `		iChunk = ph7_value_to_int(apArg[1]);` |
|     11 | 4947 | `		if( iChunk < 1 ){` |
|    ! 0 | 4948 | `			iChunk = 75;` |
|    ! 0 | 4949 | `		}` |
|     11 | 4950 | `		if( nArg > 2 ){` |
|      3 | 4951 | `			zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      1 | 4952 | `		}` |
|      5 | 4953 | `	}` |
|     11 | 4954 | `	if( iBreaklen < 1 ){` |
|      - | 4955 | `		/* Set a default column break */` |
|      - | 4956 | `#ifdef __WINNT__` |
|      1 | 4957 | `		zBreak = "\r\n";` |
|      1 | 4958 | `		iBreaklen = (int)sizeof("\r\n")-1;` |
|      - | 4959 | `#else` |
|      8 | 4960 | `		zBreak = "\n";` |
|      8 | 4961 | `		iBreaklen = (int)sizeof(char);` |
|      - | 4962 | `#endif` |
|      4 | 4963 | `	}` |
|      - | 4964 | `	/* Perform the requested operation */` |
|     11 | 4965 | `	zEnd = &zIn[iLen];` |
|     41 | 4966 | `	for(;;){` |
|      - | 4967 | `		int nMax;` |
|     47 | 4968 | `		if( zIn >= zEnd ){` |
|      - | 4969 | `			/* No more input to process */` |
|     11 | 4970 | `			break;` |
|      - | 4971 | `		}` |
|     37 | 4972 | `		nMax = (int)(zEnd-zIn);` |
|     37 | 4973 | `		if( iChunk > nMax ){` |
|     11 | 4974 | `			iChunk = nMax;` |
|      5 | 4975 | `		}` |
|      - | 4976 | `		/* Append the column first */` |
|     37 | 4977 | `		ph7_result_string(pCtx,zIn,iChunk); /* Will make it's own copy */` |
|      - | 4978 | `		/* Advance the cursor */` |
|     37 | 4979 | `		zIn += iChunk;` |
|     37 | 4980 | `		if( zIn < zEnd ){` |
|      - | 4981 | `			/* Append the line break */` |
|     27 | 4982 | `			ph7_result_string(pCtx,zBreak,iBreaklen);` |
|     13 | 4983 | `		}` |
|      1 | 4984 | `	}` |
|     11 | 4985 | `	return PH7_OK;` |
|      8 | 4986 |  |
|      - | 4987 | `/*` |
|      - | 4988 | ` * Check if the given character is a member of the given mask.` |
|      - | 4989 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 4990 | ` * Refer to [strtok()].` |
|      - | 4991 | ` */` |
|     30 | 4992 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 4993 |  |
|      - | 4994 | `	int i;` |
|     57 | 4995 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 4996 | `		if( c == zMask[i] ){` |
|     13 | 4997 | `			if( pOfft ){` |
|      5 | 4998 | `				*pOfft = i;` |
|      2 | 4999 | `			}` |
|     13 | 5000 | `			return TRUE;` |
|      - | 5001 | `		}` |
|     14 | 5002 | `	}` |
|     19 | 5003 | `	return FALSE;` |
|     16 | 5004 |  |
|      - | 5005 | `/*` |
|      - | 5006 | ` * Extract a single token from the input stream.` |
|      - | 5007 | ` * Refer to [strtok()].` |
|      - | 5008 | ` */` |
|      6 | 5009 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 5010 |  |
|      7 | 5011 | `	const char *zIn = *pzIn;` |
|      - | 5012 | `	const char *zPtr;` |
|      - | 5013 | `	/* Ignore leading delimiter */` |
|     11 | 5014 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5015 | `		zIn++;` |
|      1 | 5016 | `	}` |
|      7 | 5017 | `	if( zIn >= zEnd ){` |
|      - | 5018 | `		/* End of input */` |
|    ! 0 | 5019 | `		return SXERR_EOF;` |
|      - | 5020 | `	}` |
|      7 | 5021 | `	zPtr = zIn;` |
|      - | 5022 | `	/* Extract the token */` |
|     13 | 5023 | `	while( zIn < zEnd ){` |
|     11 | 5024 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 5025 | `			/* UTF-8 stream */` |
|    ! 0 | 5026 | `			zIn++;` |
|    ! 0 | 5027 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 5028 | `		}else{` |
|     11 | 5029 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5030 | `				break;` |
|      - | 5031 | `			}` |
|      7 | 5032 | `			zIn++;` |
|      - | 5033 | `		}` |
|      1 | 5034 | `	}` |
|      7 | 5035 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 5036 | `	/* Update the cursor */` |
|      7 | 5037 | `	*pzIn = zIn;` |
|      - | 5038 | `	/* Return to the caller */` |
|      7 | 5039 | `	return SXRET_OK;` |
|      4 | 5040 |  |
|      - | 5041 | `/* strtok auxiliary private data */` |
|      - | 5042 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 5043 | `struct strtok_aux_data` |
|      - | 5044 |  |
|      - | 5045 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 5046 | `	const char *zIn;   /* Current input stream */` |
|      - | 5047 | `	const char *zEnd;  /* End of input */` |
|      - | 5048 | `};` |
|      - | 5049 | `/*` |
|      - | 5050 | ` * string strtok(string $str,string $token)` |
|      - | 5051 | ` * string strtok(string $token)` |
|      - | 5052 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 5053 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 5054 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 5055 | ` *  words by using the space character as the token.` |
|      - | 5056 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 5057 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 5058 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 5059 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 5060 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 5061 | ` *  the argument are found.` |
|      - | 5062 | ` * Parameters` |
|      - | 5063 | ` *  $str` |
|      - | 5064 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 5065 | ` * $token` |
|      - | 5066 | ` *  The delimiter used when splitting up str.` |
|      - | 5067 | ` * Return` |
|      - | 5068 | ` *   Current token or FALSE on EOF.` |
|      - | 5069 | ` */` |
|      8 | 5070 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5071 |  |
|      - | 5072 | `	strtok_aux_data *pAux;` |
|      - | 5073 | `	const char *zMask;` |
|      - | 5074 | `	SyString sToken;` |
|      - | 5075 | `	int nMasklen;` |
|      - | 5076 | `	sxi32 rc;` |
|      9 | 5077 | `	if( nArg < 2 ){` |
|      - | 5078 | `		/* Extract top aux data */` |
|      7 | 5079 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      7 | 5080 | `		if( pAux == 0 ){` |
|      - | 5081 | `			/* No aux data,return FALSE */` |
|    ! 0 | 5082 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5083 | `			return PH7_OK;` |
|      - | 5084 | `		}` |
|      7 | 5085 | `		nMasklen = 0;` |
|      7 | 5086 | `		zMask = ""; /* cc warning */` |
|      7 | 5087 | `		if( nArg > 0 ){` |
|      - | 5088 | `			/* Extract the mask */` |
|      5 | 5089 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 5090 | `		}` |
|      7 | 5091 | `		if( nMasklen < 1 ){` |
|      - | 5092 | `			/* Invalid mask,return FALSE */` |
|      3 | 5093 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|      3 | 5094 | `			ph7_context_free_chunk(pCtx,pAux);` |
|      3 | 5095 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|      3 | 5096 | `			ph7_result_bool(pCtx,0);` |
|      3 | 5097 | `			return PH7_OK;` |
|      - | 5098 | `		}` |
|      - | 5099 | `		/* Extract the token */` |
|      5 | 5100 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 5101 | `		if( rc != SXRET_OK ){` |
|      - | 5102 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 5103 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 5104 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 5105 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 5106 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5107 | `		}else{` |
|      - | 5108 | `			/* Return the extracted token */` |
|      5 | 5109 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 5110 | `		}` |
|      3 | 5111 | `	}else{` |
|      - | 5112 | `		const char *zInput,*zCur;` |
|      - | 5113 | `		char *zDup;` |
|      - | 5114 | `		int nLen;` |
|      - | 5115 | `		/* Extract the raw input */` |
|      3 | 5116 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5117 | `		if( nLen < 1 ){` |
|      - | 5118 | `			/* Empty input,return FALSE */` |
|    ! 0 | 5119 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5120 | `			return PH7_OK;` |
|      - | 5121 | `		}` |
|      - | 5122 | `		/* Extract the mask */` |
|      3 | 5123 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 5124 | `		if( nMasklen < 1 ){` |
|      - | 5125 | `			/* Set a default mask */` |
|      - | 5126 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 5127 | `			zMask = TOK_MASK;` |
|    ! 0 | 5128 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 5129 | `#undef TOK_MASK` |
|    ! 0 | 5130 | `		}` |
|      - | 5131 | `		/* Extract a single token */` |
|      3 | 5132 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 5133 | `		if( rc != SXRET_OK ){` |
|      - | 5134 | `			/* Empty input */` |
|    ! 0 | 5135 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5136 | `			return PH7_OK;` |
|    ! 0 | 5137 | `		}else{` |
|      - | 5138 | `			/* Return the extracted token */` |
|      3 | 5139 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 5140 | `		}` |
|      - | 5141 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 5142 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 5143 | `		if( pAux ){` |
|      3 | 5144 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 5145 | `			if( nLen < 1 ){` |
|    ! 0 | 5146 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 5147 | `				return PH7_OK;` |
|      - | 5148 | `			}` |
|      - | 5149 | `			/* Duplicate input */` |
|      3 | 5150 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 5151 | `			if( zDup  ){` |
|      3 | 5152 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 5153 | `				/* Register the aux data */` |
|      3 | 5154 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 5155 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 5156 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 5157 | `			}` |
|      1 | 5158 | `		}` |
|      - | 5159 | `	}` |
|      7 | 5160 | `	return PH7_OK;` |
|      5 | 5161 |  |
|      - | 5162 | `/*` |
|      - | 5163 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 5164 | ` *  Pad a string to a certain length with another string` |
|      - | 5165 | ` * Parameters` |
|      - | 5166 | ` *  $input` |
|      - | 5167 | ` *   The input string.` |
|      - | 5168 | ` * $pad_length` |
|      - | 5169 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 5170 | ` *   string, no padding takes place.` |
|      - | 5171 | ` * $pad_string` |
|      - | 5172 | ` *   Note:` |
|      - | 5173 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 5174 | ` *    divided by the pad_string's length.` |
|      - | 5175 | ` * $pad_type` |
|      - | 5176 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 5177 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 5178 | ` * Return` |
|      - | 5179 | ` *  The padded string.` |
|      - | 5180 | ` */` |
|     10 | 5181 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5182 |  |
|      - | 5183 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 5184 | `	const char *zIn,*zPad;` |
|     11 | 5185 | `	if( nArg < 2 ){` |
|      - | 5186 | `		/* Missing arguments,return the empty string */` |
|      5 | 5187 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 5188 | `		return PH7_OK;` |
|      - | 5189 | `	}` |
|      - | 5190 | `	/* Extract the target string */` |
|      7 | 5191 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5192 | `	/* Padding length */` |
|      7 | 5193 | `	iRealPad = iPadlen = ph7_value_to_int(apArg[1]);` |
|      7 | 5194 | `	if( iPadlen > 0 ){` |
|      5 | 5195 | `		iPadlen -= iLen;` |
|      2 | 5196 | `	}` |
|      7 | 5197 | `	if( iPadlen < 1  ){` |
|      - | 5198 | `		/* Return the string verbatim */` |
|      3 | 5199 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      3 | 5200 | `		return PH7_OK;` |
|      - | 5201 | `	}` |
|      5 | 5202 | `	zPad = " "; /* Whitespace padding */` |
|      5 | 5203 | `	iStrpad = (int)sizeof(char);` |
|      5 | 5204 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      5 | 5205 | `	if( nArg > 2 ){` |
|      - | 5206 | `		/* Padding string */` |
|      5 | 5207 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      5 | 5208 | `		if( iStrpad < 1 ){` |
|      - | 5209 | `			/* Empty string */` |
|    ! 0 | 5210 | `			zPad = " "; /* Whitespace padding */` |
|    ! 0 | 5211 | `			iStrpad = (int)sizeof(char);` |
|    ! 0 | 5212 | `		}` |
|      5 | 5213 | `		if( nArg > 3 ){` |
|      - | 5214 | `			/* Padd type */` |
|      5 | 5215 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 5216 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 5217 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 5218 | `			}` |
|      2 | 5219 | `		}` |
|      2 | 5220 | `	}` |
|      5 | 5221 | `	iDiv = 1;` |
|      5 | 5222 | `	if( iType == 2 ){` |
|    ! 0 | 5223 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 5224 | `	}` |
|      - | 5225 | `	/* Perform the requested operation */` |
|      5 | 5226 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 5227 | `		jPad = iStrpad;` |
|      5 | 5228 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 5229 | `			/* Padding */` |
|      5 | 5230 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 5231 | `				break;` |
|      - | 5232 | `			}` |
|      3 | 5233 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      2 | 5234 | `		}` |
|      3 | 5235 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 5236 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 5237 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 5238 | `				if( jPad > iStrpad ){` |
|    ! 0 | 5239 | `					jPad = iStrpad;` |
|    ! 0 | 5240 | `				}` |
|      3 | 5241 | `				if( jPad < 1){` |
|    ! 0 | 5242 | `					break;` |
|      - | 5243 | `				}` |
|      3 | 5244 | `				ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 5245 | `			}` |
|      1 | 5246 | `		}` |
|      1 | 5247 | `	}` |
|      5 | 5248 | `	if( iLen > 0 ){` |
|      - | 5249 | `		/* Append the input string */` |
|      5 | 5250 | `		ph7_result_string(pCtx,zIn,iLen);` |
|      2 | 5251 | `	}` |
|      5 | 5252 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 5253 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 5254 | `			/* Padding */` |
|      5 | 5255 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 5256 | `				break;` |
|      - | 5257 | `			}` |
|      3 | 5258 | `			ph7_result_string(pCtx,zPad,iStrpad);` |
|      2 | 5259 | `		}` |
|      5 | 5260 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 5261 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 5262 | `			if( jPad > iStrpad ){` |
|    ! 0 | 5263 | `				jPad = iStrpad;` |
|    ! 0 | 5264 | `			}` |
|      3 | 5265 | `			if( jPad < 1){` |
|    ! 0 | 5266 | `				break;` |
|      - | 5267 | `			}` |
|      3 | 5268 | `			ph7_result_string(pCtx,zPad,jPad);` |
|      1 | 5269 | `		}` |
|      1 | 5270 | `	}` |
|      5 | 5271 | `	return PH7_OK;` |
|      6 | 5272 |  |
|      - | 5273 | `/*` |
|      - | 5274 | ` * String replacement private data.` |
|      - | 5275 | ` */` |
|      - | 5276 | `typedef struct str_replace_data str_replace_data;` |
|      - | 5277 | `struct str_replace_data` |
|      - | 5278 |  |
|      - | 5279 | `	/* The following two fields are only used by the strtr function */` |
|      - | 5280 | `	SyBlob *pWorker;         /* Working buffer */` |
|      - | 5281 | `	ProcStringMatch xMatch;  /* Pattern match routine */` |
|      - | 5282 | `	/* The following two fields are only used by the str_replace function */` |
|      - | 5283 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 5284 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 5285 | `};` |
|      - | 5286 | `/*` |
|      - | 5287 | ` * Remove a substring.` |
|      - | 5288 | ` */` |
|      - | 5289 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 5290 | `	for(;;){\` |
|      - | 5291 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 5292 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 5293 | `		++OFFT;\` |
|      - | 5294 | `	}\` |
|      - | 5295 |  |
|      - | 5296 | `/*` |
|      - | 5297 | ` * Shift right and insert algorithm.` |
|      - | 5298 | ` */` |
|      - | 5299 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 5300 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 5301 | `		for(;;){\` |
|      - | 5302 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 5303 | `			if(INLEN < 1 ) { break; }\` |
|      - | 5304 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 5305 | `			--INLEN; \` |
|      - | 5306 | `		}\` |
|      - | 5307 | `		for(;;){\` |
|      - | 5308 | `				if(ELEN < 1) { break; }\` |
|      - | 5309 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 5310 | `				OFFT++;\` |
|      - | 5311 | `				ENTRY++;\` |
|      - | 5312 | `				--ELEN;\` |
|      - | 5313 | `		}\` |
|      - | 5314 |  |
|      - | 5315 | `/*` |
|      - | 5316 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 5317 | ` * replacement string [i.e: zReplace].` |
|      - | 5318 | ` */` |
|     38 | 5319 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      1 | 5320 |  |
|     39 | 5321 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 5322 | `	sxu32 n,m;` |
|     39 | 5323 | `	n = SyBlobLength(pWorker);` |
|     39 | 5324 | `	m = nOfft;` |
|      - | 5325 | `	/* Delete the old entry */` |
|    475 | 5326 | `	STRDEL(zInput,n,m,nLen);` |
|     39 | 5327 | `	SyBlobLength(pWorker) -= nLen;` |
|     39 | 5328 | `	if( nReplen > 0 ){` |
|     33 | 5329 | `		sxi32 iRep = nReplen;` |
|      - | 5330 | `		sxi32 rc;` |
|      - | 5331 | `		/*` |
|      - | 5332 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 5333 | `		 * string.` |
|      - | 5334 | `		 */` |
|     33 | 5335 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     33 | 5336 | `		if( rc != SXRET_OK ){` |
|      - | 5337 | `			/* Simply ignore any memory failure problem */` |
|    ! 0 | 5338 | `			return SXRET_OK;` |
|      - | 5339 | `		}` |
|      - | 5340 | `		/* Perform the insertion now */` |
|     33 | 5341 | `		zInput = (char *)SyBlobData(pWorker);` |
|     33 | 5342 | `		n = SyBlobLength(pWorker);` |
|    163 | 5343 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     33 | 5344 | `		SyBlobLength(pWorker) += nReplen;` |
|     16 | 5345 | `	}` |
|     39 | 5346 | `	return SXRET_OK;` |
|     20 | 5347 |  |
|      - | 5348 | `/*` |
|      - | 5349 | ` * String replacement walker callback.` |
|      - | 5350 | ` * The following callback is invoked for each array entry that hold` |
|      - | 5351 | ` * the replace string.` |
|      - | 5352 | ` * Refer to the strtr() implementation for more information.` |
|      - | 5353 | ` */` |
|      8 | 5354 | `static int StringReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 5355 |  |
|      9 | 5356 | `	str_replace_data *pRepData = (str_replace_data *)pUserData;` |
|      - | 5357 | `	const char *zTarget,*zReplace;` |
|      - | 5358 | `	SyBlob *pWorker;` |
|      - | 5359 | `	int tLen,nLen;` |
|      - | 5360 | `	sxu32 nOfft;` |
|      - | 5361 | `	sxi32 rc;` |
|      - | 5362 | `	/* Point to the working buffer */` |
|      9 | 5363 | `	pWorker = pRepData->pWorker;` |
|      9 | 5364 | `	if( !ph7_value_is_string(pKey) ){` |
|      - | 5365 | `		/* Target and replace must be a string */` |
|      3 | 5366 | `		return PH7_OK;` |
|      - | 5367 | `	}` |
|      - | 5368 | `	/* Extract the target and the replace */` |
|      7 | 5369 | `	zTarget = ph7_value_to_string(pKey,&tLen);` |
|      7 | 5370 | `	if( tLen < 1 ){` |
|      - | 5371 | `		/* Empty target,return immediately */` |
|    ! 0 | 5372 | `		return PH7_OK;` |
|      - | 5373 | `	}` |
|      - | 5374 | `	/* Perform a pattern search */` |
|      7 | 5375 | `	rc = pRepData->xMatch(SyBlobData(pWorker),SyBlobLength(pWorker),(const void *)zTarget,(sxu32)tLen,&nOfft);` |
|      7 | 5376 | `	if( rc != SXRET_OK ){` |
|      - | 5377 | `		/* Pattern not found */` |
|    ! 0 | 5378 | `		return PH7_OK;` |
|      - | 5379 | `	}` |
|      - | 5380 | `	/* Extract the replace string */` |
|      7 | 5381 | `	zReplace = ph7_value_to_string(pData,&nLen);` |
|      - | 5382 | `	/* Perform the replace process */` |
|      7 | 5383 | `	StringReplace(pWorker,nOfft,tLen,zReplace,nLen);` |
|      - | 5384 | `	/* All done */` |
|      7 | 5385 | `	return PH7_OK;` |
|      5 | 5386 |  |
|      - | 5387 | `/*` |
|      - | 5388 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 5389 | ` * to collect search/replace string.` |
|      - | 5390 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 5391 | ` */` |
|     26 | 5392 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 5393 |  |
|     27 | 5394 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 5395 | `	SyString sWorker;` |
|      - | 5396 | `	const char *zIn;` |
|      - | 5397 | `	int nByte;` |
|      - | 5398 | `	/* Extract a string representation of the given argument */` |
|     27 | 5399 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     27 | 5400 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     27 | 5401 | `	if( nByte > 0 ){` |
|      - | 5402 | `		char *zDup;` |
|      - | 5403 | `		/* Duplicate the chunk */` |
|     25 | 5404 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 5405 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 5406 | `			);` |
|     25 | 5407 | `		if( zDup == 0 ){` |
|      - | 5408 | `			/* Ignore any memory failure problem */` |
|    ! 0 | 5409 | `			ph7_context_throw_error(pRep->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 5410 | `			return PH7_OK;` |
|      - | 5411 | `		}` |
|     25 | 5412 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 5413 | `		/* Save the chunk */` |
|     25 | 5414 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     12 | 5415 | `	}` |
|      - | 5416 | `	/* Save for later processing */` |
|     27 | 5417 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 5418 | `	/* All done */` |
|     13 | 5419 | `	SXUNUSED(pKey); /* cc warning */` |
|     27 | 5420 | `	return PH7_OK;` |
|     14 | 5421 |  |
|      - | 5422 | `/*` |
|      - | 5423 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 5424 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 5425 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 5426 | ` * Parameters` |
|      - | 5427 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 5428 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 5429 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 5430 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 5431 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 5432 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 5433 | ` * $search` |
|      - | 5434 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 5435 | ` *  to designate multiple needles.` |
|      - | 5436 | ` * $replace` |
|      - | 5437 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 5438 | ` *  to designate multiple replacements.` |
|      - | 5439 | ` * $subject` |
|      - | 5440 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 5441 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 5442 | ` *  of subject, and the return value is an array as well.` |
|      - | 5443 | ` * $count (Not used)` |
|      - | 5444 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 5445 | ` * Return` |
|      - | 5446 | ` * This function returns a string or an array with the replaced values.` |
|      - | 5447 | ` */` |
|  15634 | 5448 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5449 |  |
|      - | 5450 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 5451 | `	ProcStringMatch xMatch;` |
|      - | 5452 | `	const char *zIn,*zFunc;` |
|      - | 5453 | `	str_replace_data sRep;` |
|      - | 5454 | `	SyBlob sWorker;` |
|      - | 5455 | `	SySet sReplace;` |
|      - | 5456 | `	SySet sSearch;` |
|      - | 5457 | `	int rep_str;` |
|      - | 5458 | `	int nByte;` |
|      - | 5459 | `	sxi32 rc;` |
|  15636 | 5460 | `	if( nArg < 3 ){` |
|      - | 5461 | `		/* Missing/Invalid arguments,return null */` |
|      7 | 5462 | `		ph7_result_null(pCtx);` |
|      7 | 5463 | `		return PH7_OK;` |
|      - | 5464 | `	}` |
|      - | 5465 | `	/* Initialize fields */` |
|  15630 | 5466 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  15630 | 5467 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  15630 | 5468 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  15630 | 5469 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  15630 | 5470 | `	sRep.pCtx = pCtx;` |
|  15630 | 5471 | `	sRep.pCollector = &sSearch;` |
|  15630 | 5472 | `	rep_str = 0;` |
|      - | 5473 | `	/* Extract the subject */` |
|  15630 | 5474 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  15630 | 5475 | `	if( nByte < 1 ){` |
|      - | 5476 | `		/* Nothing to replace,return the empty string */` |
|     38 | 5477 | `		ph7_result_string(pCtx,"",0);` |
|     38 | 5478 | `		return PH7_OK;` |
|      - | 5479 | `	}` |
|      - | 5480 | `	/* Copy the subject */` |
|  15594 | 5481 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 5482 | `	/* Search string */` |
|  15594 | 5483 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 5484 | `		/* Collect search string */` |
|      9 | 5485 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|      5 | 5486 | `	}else{` |
|      - | 5487 | `		/* Single pattern */` |
|  15586 | 5488 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  15586 | 5489 | `		if( nByte < 1 ){` |
|      - | 5490 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 5491 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 5492 | `			return PH7_OK;` |
|      - | 5493 | `		}` |
|  15582 | 5494 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 5495 | `		/* Save for later processing */` |
|  15582 | 5496 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 5497 | `	}` |
|      - | 5498 | `	/* Replace string */` |
|  15590 | 5499 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 5500 | `		/* Collect replace string */` |
|      7 | 5501 | `		sRep.pCollector = &sReplace;` |
|      7 | 5502 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 5503 | `	}else{` |
|      - | 5504 | `		/* Single needle */` |
|  15584 | 5505 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  15584 | 5506 | `		rep_str = 1;` |
|  15584 | 5507 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 5508 | `		/* Save for later processing */` |
|  15584 | 5509 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 5510 | `	}` |
|      - | 5511 | `	/* Reset loop cursors */` |
|  15590 | 5512 | `	SySetResetCursor(&sSearch);` |
|  15590 | 5513 | `	SySetResetCursor(&sReplace);` |
|  15590 | 5514 | `	pReplace = pSearch = 0; /* cc warning */` |
|  15590 | 5515 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 5516 | `	/* Extract function name */` |
|  15590 | 5517 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 5518 | `	/* Set the default pattern match routine */` |
|  15590 | 5519 | `	xMatch = SyBlobSearch;` |
|  15590 | 5520 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 5521 | `		/* Case insensitive pattern match */` |
|     11 | 5522 | `		xMatch = iPatternMatch;` |
|      5 | 5523 | `	}` |
|      - | 5524 | `	/* Start the replace process */` |
|  31186 | 5525 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 5526 | `		sxu32 nCount,nOfft;` |
|  15598 | 5527 | `		if( pSearch->nByte <  1 ){` |
|      - | 5528 | `			/* Empty string,ignore */` |
|      3 | 5529 | `			continue;` |
|      - | 5530 | `		}` |
|      - | 5531 | `		/* Extract the replace string */` |
|  15596 | 5532 | `		if( rep_str ){` |
|  15586 | 5533 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|   7794 | 5534 | `		}else{` |
|     11 | 5535 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 5536 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 5537 | `				 * An empty string is used for the rest of replacement values` |
|      - | 5538 | `				 */` |
|      3 | 5539 | `				pReplace = 0;` |
|      1 | 5540 | `			}` |
|      - | 5541 | `		}` |
|  15596 | 5542 | `		if( pReplace == 0 ){` |
|      - | 5543 | `			/* Use an empty string instead */` |
|      3 | 5544 | `			pReplace = &sTemp;` |
|      1 | 5545 | `		}` |
|  15596 | 5546 | `		nOfft = nCount = 0;` |
|   7813 | 5547 | `		for(;;){` |
|  15628 | 5548 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 5549 | `				break;` |
|      - | 5550 | `			}` |
|      - | 5551 | `			/* Perform a pattern lookup */` |
|  23423 | 5552 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  15614 | 5553 | `				pSearch->nByte,&nOfft);` |
|  15616 | 5554 | `			if( rc != SXRET_OK ){` |
|      - | 5555 | `				/* Pattern not found */` |
|  15584 | 5556 | `				break;` |
|      - | 5557 | `			}` |
|      - | 5558 | `			/* Perform the replace operation */` |
|     33 | 5559 | `			StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|      - | 5560 | `			/* Increment offset counter */` |
|     33 | 5561 | `			nCount += nOfft + pReplace->nByte;` |
|      1 | 5562 | `		}` |
|      2 | 5563 | `	}` |
|      - | 5564 | `	/* All done,clean-up the mess left behind */` |
|  15590 | 5565 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  15590 | 5566 | `	SySetRelease(&sSearch);` |
|  15590 | 5567 | `	SySetRelease(&sReplace);` |
|  15590 | 5568 | `	SyBlobRelease(&sWorker);` |
|  15590 | 5569 | `	return PH7_OK;` |
|   7819 | 5570 |  |
|      - | 5571 | `/*` |
|      - | 5572 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 5573 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 5574 | ` *  Translate characters or replace substrings.` |
|      - | 5575 | ` * Parameters` |
|      - | 5576 | ` *  $str` |
|      - | 5577 | ` *  The string being translated.` |
|      - | 5578 | ` * $from` |
|      - | 5579 | ` *  The string being translated to to.` |
|      - | 5580 | ` * $to` |
|      - | 5581 | ` *  The string replacing from.` |
|      - | 5582 | ` * $replace_pairs` |
|      - | 5583 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 5584 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 5585 | ` * Return` |
|      - | 5586 | ` *  The translated string.` |
|      - | 5587 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 5588 | ` */` |
|     12 | 5589 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5590 |  |
|      - | 5591 | `	const char *zIn;` |
|      - | 5592 | `	int nLen;` |
|     13 | 5593 | `	if( nArg < 1 ){` |
|      - | 5594 | `		/* Nothing to replace,return FALSE */` |
|      7 | 5595 | `		ph7_result_bool(pCtx,0);` |
|      7 | 5596 | `		return PH7_OK;` |
|      - | 5597 | `	}` |
|      7 | 5598 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 5599 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 5600 | `		/* Invalid arguments */` |
|    ! 0 | 5601 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 5602 | `		return PH7_OK;` |
|      - | 5603 | `	}` |
|      9 | 5604 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 5605 | `		str_replace_data sRepData;` |
|      - | 5606 | `		SyBlob sWorker;` |
|      - | 5607 | `		/* Initilaize the working buffer */` |
|      5 | 5608 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|      - | 5609 | `		/* Copy raw string */` |
|      5 | 5610 | `		SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nLen);` |
|      - | 5611 | `		/* Init our replace data instance */` |
|      5 | 5612 | `		sRepData.pWorker = &sWorker;` |
|      5 | 5613 | `		sRepData.xMatch = SyBlobSearch;` |
|      - | 5614 | `		/* Iterate throw array entries and perform the replace operation.*/` |
|      5 | 5615 | `		ph7_array_walk(apArg[1],StringReplaceWalker,&sRepData);` |
|      - | 5616 | `		/* All done, return the result string */` |
|      7 | 5617 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|      4 | 5618 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 5619 | `		/* Clean-up */` |
|      5 | 5620 | `		SyBlobRelease(&sWorker);` |
|      3 | 5621 | `	}else{` |
|      - | 5622 | `		int i,flen,tlen,c,iOfft;` |
|      - | 5623 | `		const char *zFrom,*zTo;` |
|      3 | 5624 | `		if( nArg < 3 ){` |
|      - | 5625 | `			/* Nothing to replace */` |
|    ! 0 | 5626 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 5627 | `			return PH7_OK;` |
|      - | 5628 | `		}` |
|      - | 5629 | `		/* Extract given arguments */` |
|      3 | 5630 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 5631 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 5632 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 5633 | `			/* Nothing to replace */` |
|    ! 0 | 5634 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 5635 | `			return PH7_OK;` |
|      - | 5636 | `		}` |
|      - | 5637 | `		/* Start the replace process */` |
|     13 | 5638 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 5639 | `			c = zIn[i];` |
|     11 | 5640 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 5641 | `				if ( iOfft < tlen ){` |
|      5 | 5642 | `					c = zTo[iOfft];` |
|      2 | 5643 | `				}` |
|      2 | 5644 | `			}` |
|     11 | 5645 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 5646 |  |
|      6 | 5647 | `		}` |
|      - | 5648 | `	}` |
|      7 | 5649 | `	return PH7_OK;` |
|      7 | 5650 |  |
|      - | 5651 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 5652 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 5653 | `/*` |
|      - | 5654 | ` * Parse an INI string.` |
|      - | 5655 |  |
|      - | 5656 | ` * According to wikipedia` |
|      - | 5657 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 5658 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 5659 | ` *  Format` |
|      - | 5660 | `*    Properties` |
|      - | 5661 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 5662 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 5663 | `*     Example:` |
|      - | 5664 | `*      name=value` |
|      - | 5665 | `*    Sections` |
|      - | 5666 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 5667 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 5668 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 5669 | `*     or the end of the file. Sections may not be nested.` |
|      - | 5670 | `*     Example:` |
|      - | 5671 | `*      [section]` |
|      - | 5672 | `*   Comments` |
|      - | 5673 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 5674 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 5675 | `*/` |
|     12 | 5676 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 5677 |  |
|      - | 5678 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 5679 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 5680 | `	SyHashEntry *pEntry;` |
|      - | 5681 | `	SyString sEntry;` |
|      - | 5682 | `	SyHash sHash;` |
|      - | 5683 | `	int c;` |
|      - | 5684 | `	/* Create an empty array and worker variables */` |
|     13 | 5685 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 5686 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 5687 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 5688 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 5689 | `		/* Out of memory */` |
|    ! 0 | 5690 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      - | 5691 | `		/* Return FALSE */` |
|    ! 0 | 5692 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5693 | `		return PH7_OK;` |
|      - | 5694 | `	}` |
|     13 | 5695 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 5696 | `	pCur = pArray;` |
|      - | 5697 | `	/* Start the parse process */` |
|     21 | 5698 | `	for(;;){` |
|      - | 5699 | `		/* Ignore leading white spaces */` |
|     69 | 5700 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 5701 | `			zIn++;` |
|      1 | 5702 | `		}` |
|     43 | 5703 | `		if( zIn >= zEnd ){` |
|      - | 5704 | `			/* No more input to process */` |
|     13 | 5705 | `			break;` |
|      - | 5706 | `		}` |
|     31 | 5707 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 5708 | `			/* Comment til the end of line */` |
|    ! 0 | 5709 | `			zIn++;` |
|    ! 0 | 5710 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 5711 | `				zIn++;` |
|    ! 0 | 5712 | `			}` |
|    ! 0 | 5713 | `			continue;` |
|      - | 5714 | `		}` |
|      - | 5715 | `		/* Reset the string cursor of the working variable */` |
|     31 | 5716 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 5717 | `		if( zIn[0] == '[' ){` |
|      - | 5718 | `			/* Section: Extract the section name */` |
|      9 | 5719 | `			zIn++;` |
|      9 | 5720 | `			zCur = zIn;` |
|     73 | 5721 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 5722 | `				zIn++;` |
|      1 | 5723 | `			}` |
|      9 | 5724 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 5725 | `				/* Save the section name */` |
|      5 | 5726 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 5727 | `				SyStringFullTrim(&sEntry);` |
|      5 | 5728 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 5729 | `				if( sEntry.nByte > 0 ){` |
|      - | 5730 | `					/* Associate an array with the section */` |
|      5 | 5731 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 5732 | `					if( pSection ){` |
|      5 | 5733 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 5734 | `						pCur = pSection;` |
|      2 | 5735 | `					}` |
|      2 | 5736 | `				}` |
|      2 | 5737 | `			}` |
|      9 | 5738 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 5739 | `		}else{` |
|      - | 5740 | `			ph7_value *pOldCur;` |
|      - | 5741 | `			int is_array;` |
|      - | 5742 | `			int iLen;` |
|      - | 5743 | `			/* Properties */` |
|     23 | 5744 | `			is_array = 0;` |
|     23 | 5745 | `			zCur = zIn;` |
|     23 | 5746 | `			iLen = 0; /* cc warning */` |
|     23 | 5747 | `			pOldCur = pCur;` |
|    155 | 5748 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 5749 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 5750 | `					/* Array */` |
|    ! 0 | 5751 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 5752 | `					is_array = 1;` |
|    ! 0 | 5753 | `					if( iLen > 0 ){` |
|    ! 0 | 5754 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 5755 | `						/* Query the hashtable */` |
|    ! 0 | 5756 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 5757 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 5758 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 5759 | `						if( pEntry ){` |
|    ! 0 | 5760 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 5761 | `						}else{` |
|      - | 5762 | `							/* Create an empty array */` |
|    ! 0 | 5763 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 5764 | `							if( pvArr ){` |
|      - | 5765 | `								/* Save the entry */` |
|    ! 0 | 5766 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 5767 | `								/* Insert the entry */` |
|    ! 0 | 5768 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 5769 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 5770 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 5771 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 5772 | `							}` |
|      - | 5773 | `						}` |
|    ! 0 | 5774 | `						if( pvArr ){` |
|    ! 0 | 5775 | `							pCur = pvArr;` |
|    ! 0 | 5776 | `						}` |
|    ! 0 | 5777 | `					}` |
|    ! 0 | 5778 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 5779 | `						zIn++;` |
|    ! 0 | 5780 | `					}` |
|    ! 0 | 5781 | `				}` |
|    133 | 5782 | `				zIn++;` |
|      1 | 5783 | `			}` |
|     23 | 5784 | `			if( !is_array ){` |
|     23 | 5785 | `				iLen = (int)(zIn-zCur);` |
|     11 | 5786 | `			}` |
|      - | 5787 | `			/* Trim the key */` |
|     23 | 5788 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 5789 | `			SyStringFullTrim(&sEntry);` |
|     23 | 5790 | `			if( sEntry.nByte > 0 ){` |
|     23 | 5791 | `				if( !is_array ){` |
|      - | 5792 | `					/* Save the key name */` |
|     23 | 5793 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 5794 | `				}` |
|      - | 5795 | `				/* extract key value */` |
|     23 | 5796 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 5797 | `				zIn++; /* '=' */` |
|     39 | 5798 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 5799 | `					zIn++;` |
|      1 | 5800 | `				}` |
|     23 | 5801 | `				if( zIn < zEnd ){` |
|     21 | 5802 | `					zCur = zIn;` |
|     21 | 5803 | `					c = zIn[0];` |
|     21 | 5804 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 5805 | `						zIn++;` |
|      - | 5806 | `						/* Delimit the value */` |
|    ! 0 | 5807 | `						while( zIn < zEnd ){` |
|    ! 0 | 5808 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 5809 | `								break;` |
|      - | 5810 | `							}` |
|    ! 0 | 5811 | `							zIn++;` |
|    ! 0 | 5812 | `						}` |
|    ! 0 | 5813 | `						if( zIn < zEnd ){` |
|    ! 0 | 5814 | `							zIn++;` |
|    ! 0 | 5815 | `						}` |
|    ! 0 | 5816 | `					}else{` |
|    125 | 5817 | `						while( zIn < zEnd ){` |
|    123 | 5818 | `							if( zIn[0] == '\n' ){` |
|     19 | 5819 | `								if( zIn[-1] != '\\' ){` |
|     19 | 5820 | `									break;` |
|    ! 0 | 5821 | `								}` |
|    105 | 5822 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 5823 | `								/* Inline comments */` |
|    ! 0 | 5824 | `								break;` |
|      - | 5825 | `							}` |
|    105 | 5826 | `							zIn++;` |
|      1 | 5827 | `						}` |
|      - | 5828 | `					}` |
|      - | 5829 | `					/* Trim the value */` |
|     21 | 5830 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 5831 | `					SyStringFullTrim(&sEntry);` |
|     21 | 5832 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 5833 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 5834 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 5835 | `					}` |
|     21 | 5836 | `					if( sEntry.nByte > 0 ){` |
|     21 | 5837 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 5838 | `					}` |
|      - | 5839 | `					/* Insert the key and it's value */` |
|     21 | 5840 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 5841 | `				}` |
|     12 | 5842 | `			}else{` |
|    ! 0 | 5843 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 5844 | `					zIn++;` |
|    ! 0 | 5845 | `				}` |
|      - | 5846 | `			}` |
|     23 | 5847 | `			pCur = pOldCur;` |
|      - | 5848 | `		}` |
|      1 | 5849 | `	}` |
|     13 | 5850 | `	SyHashRelease(&sHash);` |
|      - | 5851 | `	/* Return the parse of the INI string */` |
|     13 | 5852 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 5853 | `	return SXRET_OK;` |
|      7 | 5854 |  |
|      - | 5855 | `/*` |
|      - | 5856 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 5857 | ` *  Parse a configuration string.` |
|      - | 5858 | ` * Parameters` |
|      - | 5859 | ` *  $ini` |
|      - | 5860 | ` *   The contents of the ini file being parsed.` |
|      - | 5861 | ` *  $process_sections` |
|      - | 5862 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 5863 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 5864 | ` *  $scanner_mode (Not used)` |
|      - | 5865 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 5866 | ` *   then option values will not be parsed.` |
|      - | 5867 | ` * Return` |
|      - | 5868 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 5869 | ` */` |
|     10 | 5870 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5871 |  |
|      - | 5872 | `	const char *zIni;` |
|      - | 5873 | `	int nByte;` |
|     11 | 5874 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5875 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 5876 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5877 | `		return PH7_OK;` |
|      - | 5878 | `	}` |
|      - | 5879 | `	/* Extract the raw INI buffer */` |
|     11 | 5880 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 5881 | `	/* Process the INI buffer*/` |
|     11 | 5882 | `	PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|     11 | 5883 | `	return PH7_OK;` |
|      6 | 5884 |  |
|      - | 5885 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 5886 |  |
|      - | 5887 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 5888 |  |
|      - | 5889 | `/*` |
|      - | 5890 | ` * Ctype Functions.` |
|      - | 5891 | ` * Status:` |
|      - | 5892 | ` *    Stable.` |
|      - | 5893 | ` */` |
|      - | 5894 | `/*` |
|      - | 5895 | ` * bool ctype_alnum(string $text)` |
|      - | 5896 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 5897 | ` * Parameters` |
|      - | 5898 | ` *  $text` |
|      - | 5899 | ` *   The tested string.` |
|      - | 5900 | ` * Return` |
|      - | 5901 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 5902 | ` */` |
|     16 | 5903 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5904 |  |
|      - | 5905 | `	const unsigned char *zIn,*zEnd;` |
|      - | 5906 | `	int nLen;` |
|     17 | 5907 | `	if( nArg < 1 ){` |
|      - | 5908 | `		/* Missing arguments,return FALSE */` |
|      3 | 5909 | `		ph7_result_bool(pCtx,0);` |
|      3 | 5910 | `		return PH7_OK;` |
|      - | 5911 | `	}` |
|      - | 5912 | `	/* Extract the target string */` |
|     15 | 5913 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 5914 | `	zEnd = &zIn[nLen];` |
|     15 | 5915 | `	if( nLen < 1 ){` |
|      - | 5916 | `		/* Empty string,return FALSE */` |
|      3 | 5917 | `		ph7_result_bool(pCtx,0);` |
|      3 | 5918 | `		return PH7_OK;` |
|      - | 5919 | `	}` |
|      - | 5920 | `	/* Perform the requested operation */` |
|     32 | 5921 | `	for(;;){` |
|     65 | 5922 | `		if( zIn >= zEnd ){` |
|      - | 5923 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 5924 | `			ph7_result_bool(pCtx,1);` |
|      9 | 5925 | `			return PH7_OK;` |
|      - | 5926 | `		}` |
|     57 | 5927 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 5928 | `			break;` |
|      - | 5929 | `		}` |
|      - | 5930 | `		/* Point to the next character */` |
|     53 | 5931 | `		zIn++;` |
|      1 | 5932 | `	}` |
|      - | 5933 | `	/* The test failed,return FALSE */` |
|      5 | 5934 | `	ph7_result_bool(pCtx,0);` |
|      5 | 5935 | `	return PH7_OK;` |
|      9 | 5936 |  |
|      - | 5937 | `/*` |
|      - | 5938 | ` * bool ctype_alpha(string $text)` |
|      - | 5939 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 5940 | ` * Parameters` |
|      - | 5941 | ` *  $text` |
|      - | 5942 | ` *   The tested string.` |
|      - | 5943 | ` * Return` |
|      - | 5944 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 5945 | ` */` |
|     18 | 5946 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5947 |  |
|      - | 5948 | `	const unsigned char *zIn,*zEnd;` |
|      - | 5949 | `	int nLen;` |
|     19 | 5950 | `	if( nArg < 1 ){` |
|      - | 5951 | `		/* Missing arguments,return FALSE */` |
|      3 | 5952 | `		ph7_result_bool(pCtx,0);` |
|      3 | 5953 | `		return PH7_OK;` |
|      - | 5954 | `	}` |
|      - | 5955 | `	/* Extract the target string */` |
|     17 | 5956 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 5957 | `	zEnd = &zIn[nLen];` |
|     17 | 5958 | `	if( nLen < 1 ){` |
|      - | 5959 | `		/* Empty string,return FALSE */` |
|      3 | 5960 | `		ph7_result_bool(pCtx,0);` |
|      3 | 5961 | `		return PH7_OK;` |
|      - | 5962 | `	}` |
|      - | 5963 | `	/* Perform the requested operation */` |
|     42 | 5964 | `	for(;;){` |
|     85 | 5965 | `		if( zIn >= zEnd ){` |
|      - | 5966 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 5967 | `			ph7_result_bool(pCtx,1);` |
|      9 | 5968 | `			return PH7_OK;` |
|      - | 5969 | `		}` |
|     77 | 5970 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 5971 | `			break;` |
|      - | 5972 | `		}` |
|      - | 5973 | `		/* Point to the next character */` |
|     71 | 5974 | `		zIn++;` |
|      1 | 5975 | `	}` |
|      - | 5976 | `	/* The test failed,return FALSE */` |
|      7 | 5977 | `	ph7_result_bool(pCtx,0);` |
|      7 | 5978 | `	return PH7_OK;` |
|     10 | 5979 |  |
|      - | 5980 | `/*` |
|      - | 5981 | ` * bool ctype_cntrl(string $text)` |
|      - | 5982 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 5983 | ` * Parameters` |
|      - | 5984 | ` *  $text` |
|      - | 5985 | ` *   The tested string.` |
|      - | 5986 | ` * Return` |
|      - | 5987 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 5988 | ` */` |
|     18 | 5989 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5990 |  |
|      - | 5991 | `	const unsigned char *zIn,*zEnd;` |
|      - | 5992 | `	int nLen;` |
|     19 | 5993 | `	if( nArg < 1 ){` |
|      - | 5994 | `		/* Missing arguments,return FALSE */` |
|      3 | 5995 | `		ph7_result_bool(pCtx,0);` |
|      3 | 5996 | `		return PH7_OK;` |
|      - | 5997 | `	}` |
|      - | 5998 | `	/* Extract the target string */` |
|     17 | 5999 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6000 | `	zEnd = &zIn[nLen];` |
|     17 | 6001 | `	if( nLen < 1 ){` |
|      - | 6002 | `		/* Empty string,return FALSE */` |
|      3 | 6003 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6004 | `		return PH7_OK;` |
|      - | 6005 | `	}` |
|      - | 6006 | `	/* Perform the requested operation */` |
|     14 | 6007 | `	for(;;){` |
|     29 | 6008 | `		if( zIn >= zEnd ){` |
|      - | 6009 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6010 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6011 | `			return PH7_OK;` |
|      - | 6012 | `		}` |
|     21 | 6013 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6014 | `			/* UTF-8 stream  */` |
|    ! 0 | 6015 | `			break;` |
|      - | 6016 | `		}` |
|     21 | 6017 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 6018 | `			break;` |
|      - | 6019 | `		}` |
|      - | 6020 | `		/* Point to the next character */` |
|     15 | 6021 | `		zIn++;` |
|      1 | 6022 | `	}` |
|      - | 6023 | `	/* The test failed,return FALSE */` |
|      7 | 6024 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6025 | `	return PH7_OK;` |
|     10 | 6026 |  |
|      - | 6027 | `/*` |
|      - | 6028 | ` * bool ctype_digit(string $text)` |
|      - | 6029 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 6030 | ` * Parameters` |
|      - | 6031 | ` *  $text` |
|      - | 6032 | ` *   The tested string.` |
|      - | 6033 | ` * Return` |
|      - | 6034 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 6035 | ` */` |
|   1924 | 6036 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6037 |  |
|      - | 6038 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6039 | `	int nLen;` |
|   1926 | 6040 | `	if( nArg < 1 ){` |
|      - | 6041 | `		/* Missing arguments,return FALSE */` |
|      3 | 6042 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6043 | `		return PH7_OK;` |
|      - | 6044 | `	}` |
|      - | 6045 | `	/* Extract the target string */` |
|   1924 | 6046 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   1924 | 6047 | `	zEnd = &zIn[nLen];` |
|   1924 | 6048 | `	if( nLen < 1 ){` |
|      - | 6049 | `		/* Empty string,return FALSE */` |
|      3 | 6050 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6051 | `		return PH7_OK;` |
|      - | 6052 | `	}` |
|      - | 6053 | `	/* Perform the requested operation */` |
|   1768 | 6054 | `	for(;;){` |
|   3538 | 6055 | `		if( zIn >= zEnd ){` |
|      - | 6056 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1586 | 6057 | `			ph7_result_bool(pCtx,1);` |
|   1586 | 6058 | `			return PH7_OK;` |
|      - | 6059 | `		}` |
|   1954 | 6060 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6061 | `			/* UTF-8 stream  */` |
|    ! 0 | 6062 | `			break;` |
|      - | 6063 | `		}` |
|   1954 | 6064 | `		if( !SyisDigit(zIn[0]) ){` |
|    338 | 6065 | `			break;` |
|      - | 6066 | `		}` |
|      - | 6067 | `		/* Point to the next character */` |
|   1618 | 6068 | `		zIn++;` |
|      2 | 6069 | `	}` |
|      - | 6070 | `	/* The test failed,return FALSE */` |
|    338 | 6071 | `	ph7_result_bool(pCtx,0);` |
|    338 | 6072 | `	return PH7_OK;` |
|    964 | 6073 |  |
|      - | 6074 | `/*` |
|      - | 6075 | ` * bool ctype_xdigit(string $text)` |
|      - | 6076 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 6077 | ` * Parameters` |
|      - | 6078 | ` *  $text` |
|      - | 6079 | ` *   The tested string.` |
|      - | 6080 | ` * Return` |
|      - | 6081 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 6082 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 6083 | ` */` |
|     20 | 6084 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6085 |  |
|      - | 6086 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6087 | `	int nLen;` |
|     21 | 6088 | `	if( nArg < 1 ){` |
|      - | 6089 | `		/* Missing arguments,return FALSE */` |
|      3 | 6090 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6091 | `		return PH7_OK;` |
|      - | 6092 | `	}` |
|      - | 6093 | `	/* Extract the target string */` |
|     19 | 6094 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 6095 | `	zEnd = &zIn[nLen];` |
|     19 | 6096 | `	if( nLen < 1 ){` |
|      - | 6097 | `		/* Empty string,return FALSE */` |
|      3 | 6098 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6099 | `		return PH7_OK;` |
|      - | 6100 | `	}` |
|      - | 6101 | `	/* Perform the requested operation */` |
|     46 | 6102 | `	for(;;){` |
|     93 | 6103 | `		if( zIn >= zEnd ){` |
|      - | 6104 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 6105 | `			ph7_result_bool(pCtx,1);` |
|     11 | 6106 | `			return PH7_OK;` |
|      - | 6107 | `		}` |
|     83 | 6108 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6109 | `			/* UTF-8 stream  */` |
|    ! 0 | 6110 | `			break;` |
|      - | 6111 | `		}` |
|     83 | 6112 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 6113 | `			break;` |
|      - | 6114 | `		}` |
|      - | 6115 | `		/* Point to the next character */` |
|     77 | 6116 | `		zIn++;` |
|      1 | 6117 | `	}` |
|      - | 6118 | `	/* The test failed,return FALSE */` |
|      7 | 6119 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6120 | `	return PH7_OK;` |
|     11 | 6121 |  |
|      - | 6122 | `/*` |
|      - | 6123 | ` * bool ctype_graph(string $text)` |
|      - | 6124 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 6125 | ` * Parameters` |
|      - | 6126 | ` *  $text` |
|      - | 6127 | ` *   The tested string.` |
|      - | 6128 | ` * Return` |
|      - | 6129 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 6130 | ` * (no white space), FALSE otherwise.` |
|      - | 6131 | ` */` |
|     18 | 6132 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6133 |  |
|      - | 6134 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6135 | `	int nLen;` |
|     19 | 6136 | `	if( nArg < 1 ){` |
|      - | 6137 | `		/* Missing arguments,return FALSE */` |
|      3 | 6138 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6139 | `		return PH7_OK;` |
|      - | 6140 | `	}` |
|      - | 6141 | `	/* Extract the target string */` |
|     17 | 6142 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6143 | `	zEnd = &zIn[nLen];` |
|     17 | 6144 | `	if( nLen < 1 ){` |
|      - | 6145 | `		/* Empty string,return FALSE */` |
|      3 | 6146 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6147 | `		return PH7_OK;` |
|      - | 6148 | `	}` |
|      - | 6149 | `	/* Perform the requested operation */` |
|     57 | 6150 | `	for(;;){` |
|    115 | 6151 | `		if( zIn >= zEnd ){` |
|      - | 6152 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6153 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6154 | `			return PH7_OK;` |
|      - | 6155 | `		}` |
|    107 | 6156 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6157 | `			/* UTF-8 stream  */` |
|    ! 0 | 6158 | `			break;` |
|      - | 6159 | `		}` |
|    107 | 6160 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 6161 | `			break;` |
|      - | 6162 | `		}` |
|      - | 6163 | `		/* Point to the next character */` |
|    101 | 6164 | `		zIn++;` |
|      1 | 6165 | `	}` |
|      - | 6166 | `	/* The test failed,return FALSE */` |
|      7 | 6167 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6168 | `	return PH7_OK;` |
|     10 | 6169 |  |
|      - | 6170 | `/*` |
|      - | 6171 | ` * bool ctype_print(string $text)` |
|      - | 6172 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 6173 | ` * Parameters` |
|      - | 6174 | ` *  $text` |
|      - | 6175 | ` *   The tested string.` |
|      - | 6176 | ` * Return` |
|      - | 6177 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 6178 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 6179 | ` *  or control function at all.` |
|      - | 6180 | ` */` |
|     18 | 6181 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6182 |  |
|      - | 6183 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6184 | `	int nLen;` |
|     19 | 6185 | `	if( nArg < 1 ){` |
|      - | 6186 | `		/* Missing arguments,return FALSE */` |
|      3 | 6187 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6188 | `		return PH7_OK;` |
|      - | 6189 | `	}` |
|      - | 6190 | `	/* Extract the target string */` |
|     17 | 6191 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6192 | `	zEnd = &zIn[nLen];` |
|     17 | 6193 | `	if( nLen < 1 ){` |
|      - | 6194 | `		/* Empty string,return FALSE */` |
|      3 | 6195 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6196 | `		return PH7_OK;` |
|      - | 6197 | `	}` |
|      - | 6198 | `	/* Perform the requested operation */` |
|     63 | 6199 | `	for(;;){` |
|    127 | 6200 | `		if( zIn >= zEnd ){` |
|      - | 6201 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6202 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6203 | `			return PH7_OK;` |
|      - | 6204 | `		}` |
|    119 | 6205 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6206 | `			/* UTF-8 stream  */` |
|    ! 0 | 6207 | `			break;` |
|      - | 6208 | `		}` |
|    119 | 6209 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 6210 | `			break;` |
|      - | 6211 | `		}` |
|      - | 6212 | `		/* Point to the next character */` |
|    113 | 6213 | `		zIn++;` |
|      1 | 6214 | `	}` |
|      - | 6215 | `	/* The test failed,return FALSE */` |
|      7 | 6216 | `	ph7_result_bool(pCtx,0);` |
|      7 | 6217 | `	return PH7_OK;` |
|     10 | 6218 |  |
|      - | 6219 | `/*` |
|      - | 6220 | ` * bool ctype_punct(string $text)` |
|      - | 6221 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 6222 | ` * Parameters` |
|      - | 6223 | ` *  $text` |
|      - | 6224 | ` *   The tested string.` |
|      - | 6225 | ` * Return` |
|      - | 6226 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 6227 | ` *  digit or blank, FALSE otherwise.` |
|      - | 6228 | ` */` |
|     20 | 6229 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6230 |  |
|      - | 6231 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6232 | `	int nLen;` |
|     21 | 6233 | `	if( nArg < 1 ){` |
|      - | 6234 | `		/* Missing arguments,return FALSE */` |
|      3 | 6235 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6236 | `		return PH7_OK;` |
|      - | 6237 | `	}` |
|      - | 6238 | `	/* Extract the target string */` |
|     19 | 6239 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 6240 | `	zEnd = &zIn[nLen];` |
|     19 | 6241 | `	if( nLen < 1 ){` |
|      - | 6242 | `		/* Empty string,return FALSE */` |
|      3 | 6243 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6244 | `		return PH7_OK;` |
|      - | 6245 | `	}` |
|      - | 6246 | `	/* Perform the requested operation */` |
|     38 | 6247 | `	for(;;){` |
|     77 | 6248 | `		if( zIn >= zEnd ){` |
|      - | 6249 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 6250 | `			ph7_result_bool(pCtx,1);` |
|      9 | 6251 | `			return PH7_OK;` |
|      - | 6252 | `		}` |
|     69 | 6253 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6254 | `			/* UTF-8 stream  */` |
|    ! 0 | 6255 | `			break;` |
|      - | 6256 | `		}` |
|     69 | 6257 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 6258 | `			break;` |
|      - | 6259 | `		}` |
|      - | 6260 | `		/* Point to the next character */` |
|     61 | 6261 | `		zIn++;` |
|      1 | 6262 | `	}` |
|      - | 6263 | `	/* The test failed,return FALSE */` |
|      9 | 6264 | `	ph7_result_bool(pCtx,0);` |
|      9 | 6265 | `	return PH7_OK;` |
|     11 | 6266 |  |
|      - | 6267 | `/*` |
|      - | 6268 | ` * bool ctype_space(string $text)` |
|      - | 6269 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 6270 | ` * Parameters` |
|      - | 6271 | ` *  $text` |
|      - | 6272 | ` *   The tested string.` |
|      - | 6273 | ` * Return` |
|      - | 6274 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 6275 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 6276 | ` *  and form feed characters.` |
|      - | 6277 | ` */` |
|  70976 | 6278 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6279 |  |
|      - | 6280 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6281 | `	int nLen;` |
|  70978 | 6282 | `	if( nArg < 1 ){` |
|      - | 6283 | `		/* Missing arguments,return FALSE */` |
|      3 | 6284 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6285 | `		return PH7_OK;` |
|      - | 6286 | `	}` |
|      - | 6287 | `	/* Extract the target string */` |
|  70976 | 6288 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  70976 | 6289 | `	zEnd = &zIn[nLen];` |
|  70976 | 6290 | `	if( nLen < 1 ){` |
|      - | 6291 | `		/* Empty string,return FALSE */` |
|      3 | 6292 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6293 | `		return PH7_OK;` |
|      - | 6294 | `	}` |
|      - | 6295 | `	/* Perform the requested operation */` |
|  36186 | 6296 | `	for(;;){` |
|  72330 | 6297 | `		if( zIn >= zEnd ){` |
|      - | 6298 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1334 | 6299 | `			ph7_result_bool(pCtx,1);` |
|   1334 | 6300 | `			return PH7_OK;` |
|      - | 6301 | `		}` |
|  70998 | 6302 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 6303 | `			/* UTF-8 stream  */` |
|    ! 0 | 6304 | `			break;` |
|      - | 6305 | `		}` |
|  70998 | 6306 | `		if( !SyisSpace(zIn[0]) ){` |
|  69642 | 6307 | `			break;` |
|      - | 6308 | `		}` |
|      - | 6309 | `		/* Point to the next character */` |
|   1358 | 6310 | `		zIn++;` |
|      2 | 6311 | `	}` |
|      - | 6312 | `	/* The test failed,return FALSE */` |
|  69642 | 6313 | `	ph7_result_bool(pCtx,0);` |
|  69642 | 6314 | `	return PH7_OK;` |
|  35512 | 6315 |  |
|      - | 6316 | `/*` |
|      - | 6317 | ` * bool ctype_lower(string $text)` |
|      - | 6318 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 6319 | ` * Parameters` |
|      - | 6320 | ` *  $text` |
|      - | 6321 | ` *   The tested string.` |
|      - | 6322 | ` * Return` |
|      - | 6323 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 6324 | ` */` |
|     18 | 6325 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6326 |  |
|      - | 6327 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6328 | `	int nLen;` |
|     19 | 6329 | `	if( nArg < 1 ){` |
|      - | 6330 | `		/* Missing arguments,return FALSE */` |
|      3 | 6331 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6332 | `		return PH7_OK;` |
|      - | 6333 | `	}` |
|      - | 6334 | `	/* Extract the target string */` |
|     17 | 6335 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6336 | `	zEnd = &zIn[nLen];` |
|     17 | 6337 | `	if( nLen < 1 ){` |
|      - | 6338 | `		/* Empty string,return FALSE */` |
|      3 | 6339 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6340 | `		return PH7_OK;` |
|      - | 6341 | `	}` |
|      - | 6342 | `	/* Perform the requested operation */` |
|     27 | 6343 | `	for(;;){` |
|     55 | 6344 | `		if( zIn >= zEnd ){` |
|      - | 6345 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 6346 | `			ph7_result_bool(pCtx,1);` |
|      5 | 6347 | `			return PH7_OK;` |
|      - | 6348 | `		}` |
|     51 | 6349 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 6350 | `			break;` |
|      - | 6351 | `		}` |
|      - | 6352 | `		/* Point to the next character */` |
|     41 | 6353 | `		zIn++;` |
|      1 | 6354 | `	}` |
|      - | 6355 | `	/* The test failed,return FALSE */` |
|     11 | 6356 | `	ph7_result_bool(pCtx,0);` |
|     11 | 6357 | `	return PH7_OK;` |
|     10 | 6358 |  |
|      - | 6359 | `/*` |
|      - | 6360 | ` * bool ctype_upper(string $text)` |
|      - | 6361 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 6362 | ` * Parameters` |
|      - | 6363 | ` *  $text` |
|      - | 6364 | ` *   The tested string.` |
|      - | 6365 | ` * Return` |
|      - | 6366 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 6367 | ` */` |
|     18 | 6368 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6369 |  |
|      - | 6370 | `	const unsigned char *zIn,*zEnd;` |
|      - | 6371 | `	int nLen;` |
|     19 | 6372 | `	if( nArg < 1 ){` |
|      - | 6373 | `		/* Missing arguments,return FALSE */` |
|      3 | 6374 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6375 | `		return PH7_OK;` |
|      - | 6376 | `	}` |
|      - | 6377 | `	/* Extract the target string */` |
|     17 | 6378 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 6379 | `	zEnd = &zIn[nLen];` |
|     17 | 6380 | `	if( nLen < 1 ){` |
|      - | 6381 | `		/* Empty string,return FALSE */` |
|      3 | 6382 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6383 | `		return PH7_OK;` |
|      - | 6384 | `	}` |
|      - | 6385 | `	/* Perform the requested operation */` |
|     28 | 6386 | `	for(;;){` |
|     57 | 6387 | `		if( zIn >= zEnd ){` |
|      - | 6388 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 6389 | `			ph7_result_bool(pCtx,1);` |
|      5 | 6390 | `			return PH7_OK;` |
|      - | 6391 | `		}` |
|     53 | 6392 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 6393 | `			break;` |
|      - | 6394 | `		}` |
|      - | 6395 | `		/* Point to the next character */` |
|     43 | 6396 | `		zIn++;` |
|      1 | 6397 | `	}` |
|      - | 6398 | `	/* The test failed,return FALSE */` |
|     11 | 6399 | `	ph7_result_bool(pCtx,0);` |
|     11 | 6400 | `	return PH7_OK;` |
|     10 | 6401 |  |
|      - | 6402 | `/* Date/Time functions moved to builtin_date.c */` |
|      - | 6403 | `/*` |
|      - | 6404 | ` * Section:` |
|      - | 6405 | ` *    URL handling Functions.` |
|      - | 6406 | ` * Status:` |
|      - | 6407 | ` *    Stable.` |
|      - | 6408 | ` */` |
|      - | 6409 | `/*` |
|      - | 6410 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 6411 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 6412 | ` */` |
|   1026 | 6413 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 6414 |  |
|      - | 6415 | `	/* Store in the call context result buffer */` |
|   1028 | 6416 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 6417 | `	return SXRET_OK;` |
|      2 | 6418 |  |
|      - | 6419 | `/*` |
|      - | 6420 | ` * string base64_encode(string $data)` |
|      - | 6421 | ` * string convert_uuencode(string $data)` |
|      - | 6422 | ` *  Encodes data with MIME base64` |
|      - | 6423 | ` * Parameter` |
|      - | 6424 | ` *  $data` |
|      - | 6425 | ` *    Data to encode` |
|      - | 6426 | ` * Return` |
|      - | 6427 | ` *  Encoded data or FALSE on failure.` |
|      - | 6428 | ` */` |
|     10 | 6429 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6430 |  |
|      - | 6431 | `	const char *zIn;` |
|      - | 6432 | `	int nLen;` |
|     11 | 6433 | `	if( nArg < 1 ){` |
|      - | 6434 | `		/* Missing arguments,return FALSE */` |
|      5 | 6435 | `		ph7_result_bool(pCtx,0);` |
|      5 | 6436 | `		return PH7_OK;` |
|      - | 6437 | `	}` |
|      - | 6438 | `	/* Extract the input string */` |
|      7 | 6439 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 6440 | `	if( nLen < 1 ){` |
|      - | 6441 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 6442 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6443 | `		return PH7_OK;` |
|      - | 6444 | `	}` |
|      - | 6445 | `	/* Perform the BASE64 encoding */` |
|      7 | 6446 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 6447 | `	return PH7_OK;` |
|      6 | 6448 |  |
|      - | 6449 | `/*` |
|      - | 6450 | ` * string base64_decode(string $data)` |
|      - | 6451 | ` * string convert_uudecode(string $data)` |
|      - | 6452 | ` *  Decodes data encoded with MIME base64` |
|      - | 6453 | ` * Parameter` |
|      - | 6454 | ` *  $data` |
|      - | 6455 | ` *    Encoded data.` |
|      - | 6456 | ` * Return` |
|      - | 6457 | ` *  Returns the original data or FALSE on failure.` |
|      - | 6458 | ` */` |
|     36 | 6459 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6460 |  |
|      - | 6461 | `	const char *zIn;` |
|      - | 6462 | `	int nLen;` |
|     38 | 6463 | `	if( nArg < 1 ){` |
|      - | 6464 | `		/* Missing arguments,return FALSE */` |
|      3 | 6465 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6466 | `		return PH7_OK;` |
|      - | 6467 | `	}` |
|      - | 6468 | `	/* Extract the input string */` |
|     36 | 6469 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 6470 | `	if( nLen < 1 ){` |
|      - | 6471 | `		/* Nothing to process,return FALSE */` |
|      3 | 6472 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6473 | `		return PH7_OK;` |
|      - | 6474 | `	}` |
|      - | 6475 | `	/* Perform the BASE64 decoding */` |
|     34 | 6476 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 6477 | `	return PH7_OK;` |
|     20 | 6478 |  |
|      - | 6479 | `/*` |
|      - | 6480 | ` * string urlencode(string $str)` |
|      - | 6481 | ` *  URL encoding` |
|      - | 6482 | ` * Parameter` |
|      - | 6483 | ` *  $data` |
|      - | 6484 | ` *   Input string.` |
|      - | 6485 | ` * Return` |
|      - | 6486 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 6487 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 6488 | ` *  encoded as plus (+) signs.` |
|      - | 6489 | ` */` |
|      6 | 6490 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6491 |  |
|      - | 6492 | `	const char *zIn;` |
|      - | 6493 | `	int nLen;` |
|      7 | 6494 | `	if( nArg < 1 ){` |
|      - | 6495 | `		/* Missing arguments,return FALSE */` |
|      3 | 6496 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6497 | `		return PH7_OK;` |
|      - | 6498 | `	}` |
|      - | 6499 | `	/* Extract the input string */` |
|      5 | 6500 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 6501 | `	if( nLen < 1 ){` |
|      - | 6502 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 6503 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6504 | `		return PH7_OK;` |
|      - | 6505 | `	}` |
|      - | 6506 | `	/* Perform the URL encoding */` |
|      5 | 6507 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 6508 | `	return PH7_OK;` |
|      4 | 6509 |  |
|      - | 6510 | `/*` |
|      - | 6511 | ` * string urldecode(string $str)` |
|      - | 6512 | ` *  Decodes any %## encoding in the given string.` |
|      - | 6513 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 6514 | ` * Parameter` |
|      - | 6515 | ` *  $data` |
|      - | 6516 | ` *    Input string.` |
|      - | 6517 | ` * Return` |
|      - | 6518 | ` *  Decoded URL or FALSE on failure.` |
|      - | 6519 | ` */` |
|      8 | 6520 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6521 |  |
|      - | 6522 | `	const char *zIn;` |
|      - | 6523 | `	int nLen;` |
|      9 | 6524 | `	if( nArg < 1 ){` |
|      - | 6525 | `		/* Missing arguments,return FALSE */` |
|      3 | 6526 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6527 | `		return PH7_OK;` |
|      - | 6528 | `	}` |
|      - | 6529 | `	/* Extract the input string */` |
|      7 | 6530 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 6531 | `	if( nLen < 1 ){` |
|      - | 6532 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 6533 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6534 | `		return PH7_OK;` |
|      - | 6535 | `	}` |
|      - | 6536 | `	/* Perform the URL decoding */` |
|      7 | 6537 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 6538 | `	return PH7_OK;` |
|      5 | 6539 |  |
|      - | 6540 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6541 | `/* Table of the built-in functions */` |
|      - | 6542 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 6543 | `	   /* Variable handling functions */` |
|      - | 6544 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 6545 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 6546 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 6547 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 6548 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 6549 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 6550 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 6551 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 6552 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 6553 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 6554 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 6555 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 6556 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 6557 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 6558 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 6559 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 6560 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 6561 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 6562 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 6563 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6564 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 6565 | `	   /* Math functions */` |
|      - | 6566 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 6567 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 6568 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 6569 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 6570 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 6571 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 6572 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 6573 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 6574 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 6575 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 6576 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 6577 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 6578 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 6579 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 6580 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 6581 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 6582 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 6583 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 6584 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 6585 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 6586 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 6587 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 6588 | `	{ "round",    PH7_builtin_round        },` |
|      - | 6589 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 6590 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 6591 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 6592 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 6593 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 6594 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 6595 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 6596 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 6597 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6598 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6599 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 6600 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6601 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6602 | `	   /* String handling functions */` |
|      - | 6603 |  |
|      - | 6604 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 6605 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 6606 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 6607 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 6608 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 6609 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 6610 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 6611 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 6612 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 6613 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 6614 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 6615 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 6616 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 6617 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 6618 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 6619 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 6620 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 6621 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 6622 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 6623 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 6624 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 6625 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 6626 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 6627 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 6628 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 6629 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 6630 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 6631 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 6632 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 6633 | `	{ "mb_strtolower",PH7_builtin_strtolower }, /* Only UTF-8 encoding is supported */` |
|      - | 6634 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 6635 | `	{ "mb_strtoupper",PH7_builtin_strtoupper }, /* Only UTF-8 encoding is supported */` |
|      - | 6636 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 6637 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 6638 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 6639 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 6640 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 6641 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 6642 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 6643 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 6644 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 6645 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 6646 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 6647 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 6648 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 6649 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 6650 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 6651 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 6652 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 6653 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6654 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6655 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 6656 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 6657 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 6658 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 6659 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6660 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6661 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 6662 |  |
|      - | 6663 |  |
|      - | 6664 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 6665 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 6666 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 6667 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 6668 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 6669 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6670 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6671 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 6672 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 6673 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6674 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6675 |  |
|      - | 6676 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 6677 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 6678 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 6679 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 6680 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 6681 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 6682 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 6683 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 6684 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 6685 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 6686 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 6687 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 6688 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6689 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6690 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 6691 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6692 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6693 |  |
|      - | 6694 | `	         /* Ctype functions */` |
|      - | 6695 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 6696 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 6697 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 6698 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 6699 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 6700 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 6701 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 6702 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 6703 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 6704 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 6705 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 6706 | `	         /* Time functions */` |
|      - | 6707 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 6708 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 6709 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 6710 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 6711 | `	{ "date",        PH7_builtin_date         },` |
|      - | 6712 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 6713 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 6714 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 6715 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 6716 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 6717 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 6718 | `	        /* URL functions */` |
|      - | 6719 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 6720 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 6721 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 6722 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 6723 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 6724 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 6725 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 6726 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 6727 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6728 | `};` |
|      - | 6729 | `/*` |
|      - | 6730 | ` * Register the built-in functions defined above,the array functions` |
|      - | 6731 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 6732 | ` */` |
|   1672 | 6733 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      2 | 6734 |  |
|      - | 6735 | `	sxu32 n;` |
| 255818 | 6736 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 254146 | 6737 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 127074 | 6738 | `	}` |
|      - | 6739 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   1674 | 6740 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 6741 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   1674 | 6742 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   1674 | 6743 |  |
|      - | 6744 |  |
