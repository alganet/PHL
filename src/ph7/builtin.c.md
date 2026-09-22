# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 226/310 lines (72.90%)

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
|      - |   15 | ``/* Shared ZPP helper for `int` parameters — defined OUTSIDE the`` |
|      - |   16 | ` * PH7_DISABLE_BUILTIN_FUNC guard because hashmap.c (array_slice) and` |
|      - |   17 | ` * builtin_math.c (intdiv) call it and both compile in the tiny build. */` |
| 606186 |   18 | `PH7_PRIVATE sxi32 PH7_IntArgResolve(` |
|      - |   19 | `	ph7_context *pCtx,` |
|      - |   20 | `	ph7_value *pArg,` |
|      - |   21 | `	const char *zFunc,` |
|      - |   22 | `	int iArgNum,` |
|      - |   23 | `	const char *zParamName,` |
|      - |   24 | `	const char *zTypeStr,` |
|      - |   25 | `	sxi64 *pOut` |
|      5 |   26 | `){` |
| 606191 |   27 | `	if( ph7_value_is_null(pArg) ){` |
|      - |   28 | `		/* php only DEPRECATES passing null to a non-nullable internal param; PHL` |
|      - |   29 | `		 * targets php's non-deprecated surface and rejects it with the TypeError` |
|      - |   30 | `		 * php will eventually raise. */` |
|    ! 0 |   31 | `		return PH7_VmThrowException(pCtx,` |
|      - |   32 | `			"TypeError",` |
|      - |   33 | `			"%s(): Argument #%d (%s) must be of type %s, null given",` |
|    ! 0 |   34 | `			zFunc,iArgNum,zParamName,zTypeStr` |
|      - |   35 | `			);` |
|      - |   36 | `	}` |
| 606191 |   37 | `	if( ph7_value_is_float(pArg) ){` |
|     16 |   38 | `		double dVal = ph7_value_to_double(pArg);` |
|      - |   39 | `		sxi64 iVal;` |
|      - |   40 | `		/* php: NAN/INF/out-of-int64-range floats fail ZPP outright */` |
|     16 |   41 | `		if( dVal != dVal \|\| dVal >= 9223372036854775808.0 \|\| dVal < -9223372036854775808.0 ){` |
|      7 |   42 | `			return PH7_VmThrowException(pCtx,` |
|      - |   43 | `				"TypeError",` |
|      - |   44 | `				"%s(): Argument #%d (%s) must be of type %s, float given",` |
|      2 |   45 | `				zFunc,iArgNum,zParamName,zTypeStr` |
|      - |   46 | `				);` |
|      - |   47 | `		}` |
|     12 |   48 | `		iVal = (sxi64)dVal;` |
|     12 |   49 | `		if( (double)iVal != dVal ){` |
|      - |   50 | `			/* php DEPRECATES a lossy float->int; PHL rejects it (the value is not` |
|      - |   51 | `			 * representable as int). */` |
|      7 |   52 | `			return PH7_VmThrowException(pCtx,` |
|      - |   53 | `				"TypeError",` |
|      - |   54 | `				"%s(): Argument #%d (%s) must be of type %s, float given",` |
|      2 |   55 | `				zFunc,iArgNum,zParamName,zTypeStr` |
|      - |   56 | `				);` |
|      - |   57 | `		}` |
|      8 |   58 | `		*pOut = iVal;` |
|      8 |   59 | `		return PH7_OK;` |
|      - |   60 | `	}` |
| 606177 |   61 | `	if( ph7_value_is_string(pArg) ){` |
|      - |   62 | `		const char *zNum;` |
|      - |   63 | `		int nSlen;` |
|     26 |   64 | `		int i,bFloat = 0;` |
|     26 |   65 | `		if( !PH7_MemObjStringIsNumeric(pArg) ){` |
|     28 |   66 | `			return PH7_VmThrowException(pCtx,` |
|      - |   67 | `				"TypeError",` |
|      - |   68 | `				"%s(): Argument #%d (%s) must be of type %s, string given",` |
|      9 |   69 | `				zFunc,iArgNum,zParamName,zTypeStr` |
|      - |   70 | `				);` |
|      - |   71 | `		}` |
|      8 |   72 | `		zNum = ph7_value_to_string(pArg,&nSlen);` |
|     14 |   73 | `		for( i = 0 ; i < nSlen ; i++ ){` |
|     10 |   74 | `			if( zNum[i] == '.' \|\| zNum[i] == 'e' \|\| zNum[i] == 'E' ){` |
|      3 |   75 | `				bFloat = 1;` |
|      3 |   76 | `				break;` |
|      - |   77 | `			}` |
|      5 |   78 | `		}` |
|      8 |   79 | `		if( bFloat ){` |
|      3 |   80 | `			double dVal = 0;` |
|      - |   81 | `			sxi64 iVal;` |
|      3 |   82 | `			SyStrToReal(zNum,(sxu32)nSlen,(void *)&dVal,0);` |
|      3 |   83 | `			if( dVal != dVal \|\| dVal >= 9223372036854775808.0 \|\| dVal < -9223372036854775808.0 ){` |
|    ! 0 |   84 | `				return PH7_VmThrowException(pCtx,` |
|      - |   85 | `					"TypeError",` |
|      - |   86 | `					"%s(): Argument #%d (%s) must be of type %s, string given",` |
|    ! 0 |   87 | `					zFunc,iArgNum,zParamName,zTypeStr` |
|      - |   88 | `					);` |
|      - |   89 | `			}` |
|      3 |   90 | `			iVal = (sxi64)dVal;` |
|      3 |   91 | `			if( (double)iVal != dVal ){` |
|      - |   92 | `				/* php DEPRECATES a lossy float-string->int; PHL rejects it. */` |
|      4 |   93 | `				return PH7_VmThrowException(pCtx,` |
|      - |   94 | `					"TypeError",` |
|      - |   95 | `					"%s(): Argument #%d (%s) must be of type %s, string given",` |
|      1 |   96 | `					zFunc,iArgNum,zParamName,zTypeStr` |
|      - |   97 | `					);` |
|      - |   98 | `			}` |
|    ! 0 |   99 | `			*pOut = iVal;` |
|    ! 0 |  100 | `			return PH7_OK;` |
|      - |  101 | `		}` |
|      5 |  102 | `		*pOut = ph7_value_to_int64(pArg);` |
|      5 |  103 | `		return PH7_OK;` |
|      - |  104 | `	}` |
| 606153 |  105 | `	if( !ph7_value_is_int(pArg) && !ph7_value_is_bool(pArg) ){` |
|      - |  106 | `		/* Arrays, resources and objects: php names the class for objects */` |
|    ! 0 |  107 | `		const char *zType = ph7_type_name(pArg);` |
|    ! 0 |  108 | `		if( ph7_value_is_object(pArg) ){` |
|    ! 0 |  109 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|    ! 0 |  110 | `			if( pInst && pInst->pClass ){` |
|    ! 0 |  111 | `				zType = SyStringData(&pInst->pClass->sName);` |
|    ! 0 |  112 | `			}` |
|    ! 0 |  113 | `		}` |
|    ! 0 |  114 | `		return PH7_VmThrowException(pCtx,` |
|      - |  115 | `			"TypeError",` |
|      - |  116 | `			"%s(): Argument #%d (%s) must be of type %s, %s given",` |
|    ! 0 |  117 | `			zFunc,iArgNum,zParamName,zTypeStr,zType` |
|      - |  118 | `			);` |
|      - |  119 | `	}` |
| 606153 |  120 | `	*pOut = ph7_value_to_int64(pArg);` |
| 606153 |  121 | `	return PH7_OK;` |
| 303296 |  122 | `}` |
|      - |  123 |  |
|      - |  124 | `/* This file implement built-in 'foreign' functions for the PH7 engine */` |
|      - |  125 | `/*` |
|      - |  126 | ` * Section:` |
|      - |  127 | ` *    Variable handling Functions.` |
|      - |  128 | ` * Status:` |
|      - |  129 | ` *    Stable.` |
|      - |  130 | ` */` |
|      - |  131 | `/*` |
|      - |  132 | ` * bool is_bool($var)` |
|      - |  133 | ` *  Finds out whether a variable is a boolean.` |
|      - |  134 | ` * Parameters` |
|      - |  135 | ` *   $var: The variable being evaluated.` |
|      - |  136 | ` * Return` |
|      - |  137 | ` *  TRUE if var is a boolean. False otherwise.` |
|      - |  138 | ` */` |
|    116 |  139 | `static int PH7_builtin_is_bool(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  140 | `{` |
|    120 |  141 | `	int res = 0; /* Assume false by default */` |
|    120 |  142 | `	if( nArg > 0 ){` |
|    120 |  143 | `		res = ph7_value_is_bool(apArg[0]);` |
|     58 |  144 | `	}` |
|      - |  145 | `	/* Query result */` |
|    120 |  146 | `	ph7_result_bool(pCtx,res);` |
|    120 |  147 | `	return PH7_OK;` |
|      4 |  148 | `}` |
|      - |  149 | `/*` |
|      - |  150 | ` * bool is_float($var)` |
|      - |  151 | ` * bool is_double($var)` |
|      - |  152 | ` *  Finds out whether a variable is a float.` |
|      - |  153 | ` * Parameters` |
|      - |  154 | ` *   $var: The variable being evaluated.` |
|      - |  155 | ` * Return` |
|      - |  156 | ` *  TRUE if var is a float. False otherwise.` |
|      - |  157 | ` */` |
|    588 |  158 | `static int PH7_builtin_is_float(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  159 | `{` |
|    593 |  160 | `	int res = 0; /* Assume false by default */` |
|    593 |  161 | `	if( nArg > 0 ){` |
|    593 |  162 | `		res = ph7_value_is_float(apArg[0]);` |
|    294 |  163 | `	}` |
|      - |  164 | `	/* Query result */` |
|    593 |  165 | `	ph7_result_bool(pCtx,res);` |
|    593 |  166 | `	return PH7_OK;` |
|      5 |  167 | `}` |
|      - |  168 | `/*` |
|      - |  169 | ` * bool is_int($var)` |
|      - |  170 | ` * bool is_integer($var)` |
|      - |  171 | ` * bool is_long($var)` |
|      - |  172 | ` *  Finds out whether a variable is an integer.` |
|      - |  173 | ` * Parameters` |
|      - |  174 | ` *   $var: The variable being evaluated.` |
|      - |  175 | ` * Return` |
|      - |  176 | ` *  TRUE if var is an integer. False otherwise.` |
|      - |  177 | ` */` |
|   1180 |  178 | `static int PH7_builtin_is_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  179 | `{` |
|   1185 |  180 | `	int res = 0; /* Assume false by default */` |
|   1185 |  181 | `	if( nArg > 0 ){` |
|      - |  182 | `		/* Strict PHP identity: a float is never an int, even when it holds an` |
|      - |  183 | `		 * integer value (1.0). An integer-valued real carries both MEMOBJ_INT` |
|      - |  184 | `		 * (cached) and MEMOBJ_REAL, so REAL must be excluded here. */` |
|   1185 |  185 | `		res = ph7_value_is_int(apArg[0]) && !ph7_value_is_float(apArg[0]);` |
|    590 |  186 | `	}` |
|      - |  187 | `	/* Query result */` |
|   1185 |  188 | `	ph7_result_bool(pCtx,res);` |
|   1185 |  189 | `	return PH7_OK;` |
|      5 |  190 | `}` |
|      - |  191 | `/*` |
|      - |  192 | ` * bool is_string($var)` |
|      - |  193 | ` *  Finds out whether a variable is a string.` |
|      - |  194 | ` * Parameters` |
|      - |  195 | ` *   $var: The variable being evaluated.` |
|      - |  196 | ` * Return` |
|      - |  197 | ` *  TRUE if var is string. False otherwise.` |
|      - |  198 | ` */` |
|   1818 |  199 | `static int PH7_builtin_is_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  200 | `{` |
|   1823 |  201 | `	int res = 0; /* Assume false by default */` |
|   1823 |  202 | `	if( nArg > 0 ){` |
|   1823 |  203 | `		res = ph7_value_is_string(apArg[0]);` |
|    909 |  204 | `	}` |
|      - |  205 | `	/* Query result */` |
|   1823 |  206 | `	ph7_result_bool(pCtx,res);` |
|   1823 |  207 | `	return PH7_OK;` |
|      5 |  208 | `}` |
|      - |  209 | `/*` |
|      - |  210 | ` * bool is_null($var)` |
|      - |  211 | ` *  Finds out whether a variable is NULL.` |
|      - |  212 | ` * Parameters` |
|      - |  213 | ` *   $var: The variable being evaluated.` |
|      - |  214 | ` * Return` |
|      - |  215 | ` *  TRUE if var is NULL. False otherwise.` |
|      - |  216 | ` */` |
|    110 |  217 | `static int PH7_builtin_is_null(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  218 | `{` |
|    114 |  219 | `	int res = 0; /* Assume false by default */` |
|    114 |  220 | `	if( nArg > 0 ){` |
|    114 |  221 | `		res = ph7_value_is_null(apArg[0]);` |
|     55 |  222 | `	}` |
|      - |  223 | `	/* Query result */` |
|    114 |  224 | `	ph7_result_bool(pCtx,res);` |
|    114 |  225 | `	return PH7_OK;` |
|      4 |  226 | `}` |
|      - |  227 | `/*` |
|      - |  228 | ` * bool is_numeric($var)` |
|      - |  229 | ` *  Find out whether a variable is NULL.` |
|      - |  230 | ` * Parameters` |
|      - |  231 | ` *  $var: The variable being evaluated.` |
|      - |  232 | ` * Return` |
|      - |  233 | ` *  True if var is numeric. False otherwise.` |
|      - |  234 | ` */` |
|    116 |  235 | `static int PH7_builtin_is_numeric(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  236 | `{` |
|    121 |  237 | `	int res = 0; /* Assume false by default */` |
|    121 |  238 | `	if( nArg > 0 ){` |
|      - |  239 | `		/* Strict PHP semantics: only int/float and numeric strings are numeric.` |
|      - |  240 | `		 * PHL's lenient helper also reports booleans as numeric (they coerce for` |
|      - |  241 | `		 * arithmetic), but php's is_numeric() rejects true/false, so exclude` |
|      - |  242 | `		 * MEMOBJ_BOOL here. */` |
|    121 |  243 | `		res = ph7_value_is_numeric(apArg[0]) && !ph7_value_is_bool(apArg[0]);` |
|     58 |  244 | `	}` |
|      - |  245 | `	/* Query result */` |
|    121 |  246 | `	ph7_result_bool(pCtx,res);` |
|    121 |  247 | `	return PH7_OK;` |
|      5 |  248 | `}` |
|      - |  249 | `/*` |
|      - |  250 | ` * bool is_scalar($var)` |
|      - |  251 | ` *  Find out whether a variable is a scalar.` |
|      - |  252 | ` * Parameters` |
|      - |  253 | ` *  $var: The variable being evaluated.` |
|      - |  254 | ` * Return` |
|      - |  255 | ` *  True if var is scalar. False otherwise.` |
|      - |  256 | ` */` |
|     30 |  257 | `static int PH7_builtin_is_scalar(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  258 | `{` |
|     31 |  259 | `	int res = 0; /* Assume false by default */` |
|     31 |  260 | `	if( nArg > 0 ){` |
|      - |  261 | `		/* Strict PHP semantics: scalars are int/float/string/bool. PHL's` |
|      - |  262 | `		 * MEMOBJ_SCALAR bucket also includes NULL, but php's is_scalar(null) is` |
|      - |  263 | `		 * false, so exclude the NULL case. */` |
|     31 |  264 | `		res = ph7_value_is_scalar(apArg[0]) && !ph7_value_is_null(apArg[0]);` |
|     15 |  265 | `	}` |
|      - |  266 | `	/* Query result */` |
|     31 |  267 | `	ph7_result_bool(pCtx,res);` |
|     31 |  268 | `	return PH7_OK;` |
|      1 |  269 | `}` |
|      - |  270 | `/*` |
|      - |  271 | ` * bool is_array($var)` |
|      - |  272 | ` *  Find out whether a variable is an array.` |
|      - |  273 | ` * Parameters` |
|      - |  274 | ` *  $var: The variable being evaluated.` |
|      - |  275 | ` * Return` |
|      - |  276 | ` *  True if var is an array. False otherwise.` |
|      - |  277 | ` */` |
|   2064 |  278 | `static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  279 | `{` |
|   2069 |  280 | `	int res = 0; /* Assume false by default */` |
|   2069 |  281 | `	if( nArg > 0 ){` |
|   2069 |  282 | `		res = ph7_value_is_array(apArg[0]);` |
|   1032 |  283 | `	}` |
|      - |  284 | `	/* Query result */` |
|   2069 |  285 | `	ph7_result_bool(pCtx,res);` |
|   2069 |  286 | `	return PH7_OK;` |
|      5 |  287 | `}` |
|      - |  288 | `/*` |
|      - |  289 | ` * bool is_object($var)` |
|      - |  290 | ` *  Find out whether a variable is an object.` |
|      - |  291 | ` * Parameters` |
|      - |  292 | ` *  $var: The variable being evaluated.` |
|      - |  293 | ` * Return` |
|      - |  294 | ` *  True if var is an object. False otherwise.` |
|      - |  295 | ` */` |
|   2392 |  296 | `static int PH7_builtin_is_object(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  297 | `{` |
|   2397 |  298 | `	int res = 0; /* Assume false by default */` |
|   2397 |  299 | `	if( nArg > 0 ){` |
|   2397 |  300 | `		res = ph7_value_is_object(apArg[0]);` |
|   1196 |  301 | `	}` |
|      - |  302 | `	/* Query result */` |
|   2397 |  303 | `	ph7_result_bool(pCtx,res);` |
|   2397 |  304 | `	return PH7_OK;` |
|      5 |  305 | `}` |
|      - |  306 | `/*` |
|      - |  307 | ` * bool is_resource($var)` |
|      - |  308 | ` *  Find out whether a variable is a resource.` |
|      - |  309 | ` * Parameters` |
|      - |  310 | ` *  $var: The variable being evaluated.` |
|      - |  311 | ` * Return` |
|      - |  312 | ` *  True if a resource. False otherwise.` |
|      - |  313 | ` */` |
|    432 |  314 | `static int PH7_builtin_is_resource(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  315 | `{` |
|    437 |  316 | `	int res = 0; /* Assume false by default */` |
|    437 |  317 | `	if( nArg > 0 && ph7_value_is_resource(apArg[0]) ){` |
|      - |  318 | `		/* A handle closed via fclose()/closedir()/pclose() is no longer a` |
|      - |  319 | `		 * live resource — php's is_resource() returns false for it. */` |
|     48 |  320 | `		res = !PH7_VfsResourceIsClosed(apArg[0]->x.pOther);` |
|     22 |  321 | `	}` |
|    437 |  322 | `	ph7_result_bool(pCtx,res);` |
|    437 |  323 | `	return PH7_OK;` |
|      5 |  324 | `}` |
|      - |  325 | `/*` |
|      - |  326 | ` * float floatval($var)` |
|      - |  327 | ` *  Get float value of a variable.` |
|      - |  328 | ` * Parameter` |
|      - |  329 | ` *  $var: The variable being processed.` |
|      - |  330 | ` * Return` |
|      - |  331 | ` *  the float value of a variable.` |
|      - |  332 | ` */` |
|      4 |  333 | `static int PH7_builtin_floatval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  334 | `{` |
|      5 |  335 | `	if( nArg < 1 ){` |
|      - |  336 | `		/* return 0.0 */` |
|    ! 0 |  337 | `		ph7_result_double(pCtx,0);` |
|    ! 0 |  338 | `	}else{` |
|      - |  339 | `		double dval;` |
|      - |  340 | `		/* Perform the cast */` |
|      5 |  341 | `		dval = ph7_value_to_double(apArg[0]);` |
|      5 |  342 | `		ph7_result_double(pCtx,dval);` |
|      - |  343 | `	}` |
|      5 |  344 | `	return PH7_OK;` |
|      1 |  345 | `}` |
|      - |  346 | `/*` |
|      - |  347 | ` * int intval($var)` |
|      - |  348 | ` *  Get integer value of a variable.` |
|      - |  349 | ` * Parameter` |
|      - |  350 | ` *  $var: The variable being processed.` |
|      - |  351 | ` * Return` |
|      - |  352 | ` *  the int value of a variable.` |
|      - |  353 | ` */` |
|     76 |  354 | `static int PH7_builtin_intval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  355 | `{` |
|     78 |  356 | `	if( nArg < 1 ){` |
|      - |  357 | `		/* return 0 */` |
|    ! 0 |  358 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  359 | `	}else{` |
|      - |  360 | `		sxi64 iVal;` |
|      - |  361 | `		/* Perform the cast */` |
|     78 |  362 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|     78 |  363 | `		ph7_result_int64(pCtx,iVal);` |
|      - |  364 | `	}` |
|     78 |  365 | `	return PH7_OK;` |
|      2 |  366 | `}` |
|      - |  367 | `/*` |
|      - |  368 | ` * string strval($var)` |
|      - |  369 | ` *  Get the string representation of a variable.` |
|      - |  370 | ` * Parameter` |
|      - |  371 | ` *  $var: The variable being processed.` |
|      - |  372 | ` * Return` |
|      - |  373 | ` *  the string value of a variable.` |
|      - |  374 | ` */` |
|      6 |  375 | `static int PH7_builtin_strval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  376 | `{` |
|      8 |  377 | `	if( nArg < 1 ){` |
|      - |  378 | `		/* return NULL */` |
|    ! 0 |  379 | `		ph7_result_null(pCtx);` |
|    ! 0 |  380 | `	}else{` |
|      - |  381 | `		const char *zVal;` |
|      8 |  382 | `		int iLen = 0; /* cc -O6 warning */` |
|      - |  383 | `		/* Perform the cast. It is the USER-VISIBLE one: strval() is php's` |
|      - |  384 | `		 * (string) cast spelled as a function, so an object with no` |
|      - |  385 | `		 * __toString() throws there too (it used to answer "Object"). */` |
|      8 |  386 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&zVal,&iLen);` |
|      8 |  387 | `		if( rcSv != SXRET_OK ){` |
|      3 |  388 | `			return rcSv;` |
|      - |  389 | `		}` |
|      6 |  390 | `		ph7_result_string(pCtx,zVal,iLen);` |
|      - |  391 | `	}` |
|      6 |  392 | `	return PH7_OK;` |
|      5 |  393 | `}` |
|      - |  394 | `/*` |
|      - |  395 | ` * bool boolval($var)` |
|      - |  396 | ` *  Get the boolean value of a variable.` |
|      - |  397 | ` * Parameter` |
|      - |  398 | ` *  $var: The variable being processed.` |
|      - |  399 | ` * Return` |
|      - |  400 | ` *  the bool value of a variable.` |
|      - |  401 | ` */` |
|     22 |  402 | `static int PH7_builtin_boolval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  403 | `{` |
|      - |  404 | `	int bVal;` |
|     23 |  405 | `	if( nArg != 1 ){` |
|    ! 0 |  406 | `		return PH7_VmThrowException(pCtx,` |
|      - |  407 | `			"ArgumentCountError",` |
|      - |  408 | `			"boolval() expects exactly 1 argument, %d given",` |
|    ! 0 |  409 | `			nArg` |
|      - |  410 | `			);` |
|      - |  411 | `	}` |
|      - |  412 | `	/* Perform the cast */` |
|     23 |  413 | `	bVal = ph7_value_to_bool(apArg[0]);` |
|     23 |  414 | `	ph7_result_bool(pCtx,bVal);` |
|     23 |  415 | `	return PH7_OK;` |
|     12 |  416 | `}` |
|      - |  417 | `/*` |
|      - |  418 | ` * bool empty($var)` |
|      - |  419 | ` *  Determine whether a variable is empty.` |
|      - |  420 | ` * Parameters` |
|      - |  421 | ` *   $var: The variable being checked.` |
|      - |  422 | ` * Return` |
|      - |  423 | ` *  0 if var has a non-empty and non-zero value.1 otherwise.` |
|      - |  424 | ` */` |
|  42962 |  425 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  426 | `{` |
|  42967 |  427 | `	int res = 1; /* Assume empty by default */` |
|  42967 |  428 | `	if( nArg > 0 ){` |
|  42967 |  429 | `		res = ph7_value_is_empty(apArg[0]);` |
|  21481 |  430 | `	}` |
|  42967 |  431 | `	ph7_result_bool(pCtx,res);` |
|  42967 |  432 | `	return PH7_OK;` |
|      - |  433 |  |
|      5 |  434 | `}` |
|      - |  435 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |  436 | `#define PH7_NEED_BUILTIN_REG 1` |
|      - |  437 | `#endif` |
|      - |  438 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - |  439 | `#define PH7_NEED_FMT_AND_INI 1` |
|      - |  440 | `#endif` |
|      - |  441 |  |
|      - |  442 | `/* Math functions moved to builtin_math.c */` |
|      - |  443 |  |
|      - |  444 | `/* Table of the built-in functions */` |
|      - |  445 | `/*` |
|      - |  446 | ` * int memory_get_usage([bool $real_usage = false])` |
|      - |  447 | ` *  Amount of memory, in bytes, currently allocated to the script through PHL's` |
|      - |  448 | ` *  memory backend. PHL tracks the backend's real allocated bytes, so the` |
|      - |  449 | ` *  $real_usage flag has no effect here (php's non-real figure would be smaller,` |
|      - |  450 | ` *  reflecting Zend's emalloc bookkeeping — recorded divergence).` |
|      - |  451 | ` */` |
|    ! 0 |  452 | `static int PH7_builtin_memory_get_usage(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 |  453 | `{` |
|    ! 0 |  454 | `	SXUNUSED(nArg);` |
|    ! 0 |  455 | `	SXUNUSED(apArg);` |
|    ! 0 |  456 | `	ph7_result_int64(pCtx,(ph7_int64)pCtx->pVm->sAllocator.nMemUsed);` |
|    ! 0 |  457 | `	return PH7_OK;` |
|    ! 0 |  458 | `}` |
|      - |  459 | `/*` |
|      - |  460 | ` * int memory_get_peak_usage([bool $real_usage = false])` |
|      - |  461 | ` *  High-water mark of memory_get_usage() over the script's lifetime.` |
|      - |  462 | ` */` |
|      4 |  463 | `static int PH7_builtin_memory_get_peak_usage(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  464 | `{` |
|      2 |  465 | `	SXUNUSED(nArg);` |
|      2 |  466 | `	SXUNUSED(apArg);` |
|      5 |  467 | `	ph7_result_int64(pCtx,(ph7_int64)pCtx->pVm->sAllocator.nMemPeak);` |
|      5 |  468 | `	return PH7_OK;` |
|      1 |  469 | `}` |
|      - |  470 | `/*` |
|      - |  471 | ` * void memory_reset_peak_usage()` |
|      - |  472 | ` *  Reset the peak memory usage (memory_get_peak_usage) back to the current` |
|      - |  473 | ` *  live usage — php 8.2. Frameworks call it between tests to measure per-test` |
|      - |  474 | ` *  peaks.` |
|      - |  475 | ` */` |
|      4 |  476 | `static int PH7_builtin_memory_reset_peak_usage(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  477 | `{` |
|      2 |  478 | `	SXUNUSED(nArg);` |
|      2 |  479 | `	SXUNUSED(apArg);` |
|      5 |  480 | `	pCtx->pVm->sAllocator.nMemPeak = pCtx->pVm->sAllocator.nMemUsed;` |
|      5 |  481 | `	return PH7_OK;` |
|      1 |  482 | `}` |
|      - |  483 | `/*` |
|      - |  484 | ` * PHL frees values by reference count as they go out of scope, so there is no` |
|      - |  485 | ` * mark-and-sweep cycle collector to drive. The gc_* family is provided for` |
|      - |  486 | ` * source compatibility (real frameworks call it around test runs): the state is` |
|      - |  487 | ` * observational and collection is a no-op. Recorded divergence from php, whose` |
|      - |  488 | ` * collector actually reclaims reference cycles.` |
|      - |  489 | ` */` |
|    ! 0 |  490 | `static int PH7_builtin_gc_enable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 |  491 | `{` |
|    ! 0 |  492 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    ! 0 |  493 | `	pCtx->pVm->bGcEnabled = 1;` |
|    ! 0 |  494 | `	return PH7_OK;` |
|    ! 0 |  495 | `}` |
|    ! 0 |  496 | `static int PH7_builtin_gc_disable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 |  497 | `{` |
|    ! 0 |  498 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    ! 0 |  499 | `	pCtx->pVm->bGcEnabled = 0;` |
|    ! 0 |  500 | `	return PH7_OK;` |
|    ! 0 |  501 | `}` |
|    ! 0 |  502 | `static int PH7_builtin_gc_enabled(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 |  503 | `{` |
|    ! 0 |  504 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    ! 0 |  505 | `	ph7_result_bool(pCtx,pCtx->pVm->bGcEnabled);` |
|    ! 0 |  506 | `	return PH7_OK;` |
|    ! 0 |  507 | `}` |
|    ! 0 |  508 | `static int PH7_builtin_gc_collect_cycles(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 |  509 | `{` |
|      - |  510 | `	/* No cycle collector: nothing to reclaim. Returns the count collected (0). */` |
|    ! 0 |  511 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    ! 0 |  512 | `	ph7_result_int(pCtx,0);` |
|    ! 0 |  513 | `	return PH7_OK;` |
|    ! 0 |  514 | `}` |
|    ! 0 |  515 | `static int PH7_builtin_gc_mem_caches(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 |  516 | `{` |
|    ! 0 |  517 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    ! 0 |  518 | `	ph7_result_int(pCtx,0);` |
|    ! 0 |  519 | `	return PH7_OK;` |
|    ! 0 |  520 | `}` |
|      - |  521 | `/*` |
|      - |  522 | ` * array gc_status(void)` |
|      - |  523 | ` *  php 8.3 shape. PHL never runs a collection, so every counter is zero and the` |
|      - |  524 | ` *  timing fields are 0.0; 'running' reflects gc_enable()/gc_disable().` |
|      - |  525 | ` */` |
|    ! 0 |  526 | `static int PH7_builtin_gc_status(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 |  527 | `{` |
|      - |  528 | `	ph7_value *pArray,*pVal;` |
|    ! 0 |  529 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    ! 0 |  530 | `	pArray = ph7_context_new_array(pCtx);` |
|    ! 0 |  531 | `	pVal = ph7_context_new_scalar(pCtx);` |
|    ! 0 |  532 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|    ! 0 |  533 | `		ph7_result_null(pCtx);` |
|    ! 0 |  534 | `		return PH7_OK;` |
|      - |  535 | `	}` |
|      - |  536 | `	/* Key order matches php 8.3's gc_status(). */` |
|    ! 0 |  537 | `	ph7_value_bool(pVal,pCtx->pVm->bGcEnabled); ph7_array_add_strkey_elem(pArray,"running",pVal);` |
|    ! 0 |  538 | `	ph7_value_bool(pVal,0);      ph7_array_add_strkey_elem(pArray,"protected",pVal);` |
|    ! 0 |  539 | `	ph7_value_bool(pVal,0);      ph7_array_add_strkey_elem(pArray,"full",pVal);` |
|    ! 0 |  540 | `	ph7_value_int(pVal,0);       ph7_array_add_strkey_elem(pArray,"runs",pVal);` |
|    ! 0 |  541 | `	ph7_value_int(pVal,0);       ph7_array_add_strkey_elem(pArray,"collected",pVal);` |
|    ! 0 |  542 | `	ph7_value_int(pVal,1000);    ph7_array_add_strkey_elem(pArray,"threshold",pVal);` |
|    ! 0 |  543 | `	ph7_value_int(pVal,0);       ph7_array_add_strkey_elem(pArray,"buffer_size",pVal);` |
|    ! 0 |  544 | `	ph7_value_int(pVal,0);       ph7_array_add_strkey_elem(pArray,"roots",pVal);` |
|    ! 0 |  545 | `	ph7_value_double(pVal,0.0);  ph7_array_add_strkey_elem(pArray,"application_time",pVal);` |
|    ! 0 |  546 | `	ph7_value_double(pVal,0.0);  ph7_array_add_strkey_elem(pArray,"collector_time",pVal);` |
|    ! 0 |  547 | `	ph7_value_double(pVal,0.0);  ph7_array_add_strkey_elem(pArray,"destructor_time",pVal);` |
|    ! 0 |  548 | `	ph7_value_double(pVal,0.0);  ph7_array_add_strkey_elem(pArray,"free_time",pVal);` |
|    ! 0 |  549 | `	ph7_context_release_value(pCtx,pVal);` |
|    ! 0 |  550 | `	ph7_result_value(pCtx,pArray);` |
|    ! 0 |  551 | `	return PH7_OK;` |
|    ! 0 |  552 | `}` |
|      - |  553 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - |  554 | `	{ "memory_get_usage"     , PH7_builtin_memory_get_usage      },` |
|      - |  555 | `	{ "memory_get_peak_usage", PH7_builtin_memory_get_peak_usage },` |
|      - |  556 | `	{ "memory_reset_peak_usage", PH7_builtin_memory_reset_peak_usage },` |
|      - |  557 | `	{ "gc_enable"            , PH7_builtin_gc_enable             },` |
|      - |  558 | `	{ "gc_disable"           , PH7_builtin_gc_disable            },` |
|      - |  559 | `	{ "gc_enabled"           , PH7_builtin_gc_enabled            },` |
|      - |  560 | `	{ "gc_collect_cycles"    , PH7_builtin_gc_collect_cycles     },` |
|      - |  561 | `	{ "gc_mem_caches"        , PH7_builtin_gc_mem_caches         },` |
|      - |  562 | `	{ "gc_status"            , PH7_builtin_gc_status             },` |
|      - |  563 | `	   /* Variable handling functions */` |
|      - |  564 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - |  565 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - |  566 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - |  567 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - |  568 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - |  569 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - |  570 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - |  571 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - |  572 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - |  573 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - |  574 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - |  575 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - |  576 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - |  577 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - |  578 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - |  579 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - |  580 | `	{ "boolval"    , PH7_builtin_boolval     },` |
|      - |  581 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - |  582 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - |  583 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - |  584 | `	   /* Math functions */` |
|      - |  585 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - |  586 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - |  587 | `	{ "acosh" ,   PH7_builtin_acosh        },` |
|      - |  588 | `	{ "asinh" ,   PH7_builtin_asinh        },` |
|      - |  589 | `	{ "atanh" ,   PH7_builtin_atanh        },` |
|      - |  590 | `	{ "expm1" ,   PH7_builtin_expm1        },` |
|      - |  591 | `	{ "log1p" ,   PH7_builtin_log1p        },` |
|      - |  592 | `	{ "deg2rad" , PH7_builtin_deg2rad      },` |
|      - |  593 | `	{ "rad2deg" , PH7_builtin_rad2deg      },` |
|      - |  594 | `	{ "fpow" ,    PH7_builtin_fpow         },` |
|      - |  595 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - |  596 | `	{ "floor",    PH7_builtin_floor        },` |
|      - |  597 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - |  598 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - |  599 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - |  600 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - |  601 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - |  602 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - |  603 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - |  604 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - |  605 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - |  606 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - |  607 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - |  608 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - |  609 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - |  610 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - |  611 | `	{ "pi",       PH7_builtin_pi           },` |
|      - |  612 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - |  613 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - |  614 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - |  615 | `	{ "round",    PH7_builtin_round        },` |
|      - |  616 | `	{ "intdiv",   PH7_builtin_intdiv       },` |
|      - |  617 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - |  618 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - |  619 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - |  620 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - |  621 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - |  622 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - |  623 | `	{ "srand",  PH7_builtin_srand          },` |
|      - |  624 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - |  625 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - |  626 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - |  627 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - |  628 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - |  629 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - |  630 | `	   /* String handling functions */` |
|      - |  631 |  |
|      - |  632 | `	{ "substr",          PH7_builtin_substr     },` |
|      - |  633 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - |  634 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - |  635 | `	{ "substr_replace",  PH7_builtin_substr_replace },` |
|      - |  636 | `	{ "levenshtein",     PH7_builtin_levenshtein },` |
|      - |  637 | `	{ "similar_text",    PH7_builtin_similar_text },` |
|      - |  638 | `	{ "str_word_count",  PH7_builtin_str_word_count },` |
|      - |  639 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - |  640 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - |  641 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - |  642 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - |  643 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - |  644 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - |  645 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - |  646 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - |  647 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - |  648 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - |  649 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - |  650 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - |  651 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - |  652 | `	{ "strnatcmp"  , PH7_builtin_strnatcmp  },` |
|      - |  653 | `	{ "strnatcasecmp", PH7_builtin_strnatcmp },` |
|      - |  654 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - |  655 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - |  656 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - |  657 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - |  658 | `	{ "join"       , PH7_builtin_implode    },` |
|      - |  659 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - |  660 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - |  661 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - |  662 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - |  663 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - |  664 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - |  665 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - |  666 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - |  667 | `	{ "mb_strtolower",PH7_builtin_mb_case_f }, /* UTF-8 only (builtin_mb.c) */` |
|      - |  668 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - |  669 | `	{ "mb_strtoupper",PH7_builtin_mb_case_f }, /* UTF-8 only (builtin_mb.c) */` |
|      - |  670 | `	{ "mb_strlen",    PH7_builtin_mb_strlen_f },` |
|      - |  671 | `	{ "mb_substr",    PH7_builtin_mb_substr_f },` |
|      - |  672 | `	{ "mb_convert_case", PH7_builtin_mb_convert_case_f },` |
|      - |  673 | `	{ "mb_strpos",    PH7_builtin_mb_strpos_f },` |
|      - |  674 | `	{ "mb_stripos",   PH7_builtin_mb_strpos_f },` |
|      - |  675 | `	{ "mb_strrpos",   PH7_builtin_mb_strpos_f },` |
|      - |  676 | `	{ "mb_str_split", PH7_builtin_mb_str_split_f },` |
|      - |  677 | `	{ "mb_internal_encoding", PH7_builtin_mb_internal_encoding_f },` |
|      - |  678 | `	{ "mb_check_encoding",    PH7_builtin_mb_check_encoding_f },` |
|      - |  679 | `	{ "mb_strwidth",  PH7_builtin_mb_strwidth_f },` |
|      - |  680 | `	{ "mb_chr",       PH7_builtin_mb_chr_f   },` |
|      - |  681 | `	{ "mb_ord",       PH7_builtin_mb_ord_f   },` |
|      - |  682 | `	{ "mb_detect_encoding", PH7_builtin_mb_detect_encoding_f },` |
|      - |  683 | `	{ "mb_convert_encoding", PH7_builtin_mb_convert_encoding_f },` |
|      - |  684 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - |  685 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - |  686 | `	{ "ord",          PH7_builtin_ord        },` |
|      - |  687 | `	{ "chr",          PH7_builtin_chr        },` |
|      - |  688 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - |  689 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - |  690 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - |  691 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - |  692 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - |  693 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - |  694 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - |  695 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - |  696 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - |  697 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - |  698 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - |  699 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - |  700 | `	{ "str_contains", PH7_builtin_str_contains },` |
|      - |  701 | `	{ "str_starts_with", PH7_builtin_str_starts_with },` |
|      - |  702 | `	{ "str_ends_with", PH7_builtin_str_ends_with },` |
|      - |  703 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - |  704 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - |  705 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - |  706 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - |  707 | `	{ "printf",       PH7_builtin_printf     },` |
|      - |  708 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - |  709 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - |  710 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - |  711 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - |  712 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - |  713 |  |
|      - |  714 |  |
|      - |  715 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - |  716 | `	{ "md5",          PH7_builtin_md5       },` |
|      - |  717 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - |  718 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - |  719 | `	{ "hash",         PH7_builtin_hash      },` |
|      - |  720 | `	{ "hash_hmac",    PH7_builtin_hash_hmac },` |
|      - |  721 | `	{ "hash_equals",  PH7_builtin_hash_equals },` |
|      - |  722 | `	{ "hash_algos",   PH7_builtin_hash_algos },` |
|      - |  723 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - |  724 | `	{ "password_hash",         PH7_builtin_password_hash },` |
|      - |  725 | `	{ "password_verify",       PH7_builtin_password_verify },` |
|      - |  726 | `	{ "password_get_info",     PH7_builtin_password_get_info },` |
|      - |  727 | `	{ "password_needs_rehash", PH7_builtin_password_needs_rehash },` |
|      - |  728 | `	{ "filter_var",            PH7_builtin_filter_var },` |
|      - |  729 | `	{ "filter_input",          PH7_builtin_filter_input },` |
|      - |  730 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - |  731 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - |  732 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - |  733 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - |  734 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - |  735 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - |  736 |  |
|      - |  737 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - |  738 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - |  739 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - |  740 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - |  741 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - |  742 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - |  743 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - |  744 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - |  745 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - |  746 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - |  747 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - |  748 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - |  749 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - |  750 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - |  751 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - |  752 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - |  753 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - |  754 |  |
|      - |  755 | `	         /* Ctype functions */` |
|      - |  756 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - |  757 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - |  758 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - |  759 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - |  760 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - |  761 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - |  762 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - |  763 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - |  764 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - |  765 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - |  766 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - |  767 | `	         /* Time functions */` |
|      - |  768 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - |  769 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - |  770 | `	{ "hrtime",      PH7_builtin_hrtime       },` |
|      - |  771 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - |  772 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - |  773 | `	{ "date",        PH7_builtin_date         },` |
|      - |  774 | `	{ "idate",       PH7_builtin_idate        },` |
|      - |  775 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - |  776 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - |  777 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - |  778 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - |  779 | `	{ "date_default_timezone_get", PH7_builtin_date_default_timezone_get },` |
|      - |  780 | `	{ "date_default_timezone_set", PH7_builtin_date_default_timezone_set },` |
|      - |  781 | `	        /* URL functions */` |
|      - |  782 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - |  783 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - |  784 | `	{ "convert_uuencode",PH7_builtin_convert_uuencode },` |
|      - |  785 | `	{ "convert_uudecode",PH7_builtin_convert_uudecode },` |
|      - |  786 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - |  787 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - |  788 | `	{ "rawurlencode", PH7_builtin_rawurlencode },` |
|      - |  789 | `	{ "rawurldecode", PH7_builtin_rawurldecode },` |
|      - |  790 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - |  791 | `};` |
|      - |  792 | `/*` |
|      - |  793 | ` * Register the built-in functions defined above,the array functions` |
|      - |  794 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - |  795 | ` */` |
|   3956 |  796 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      5 |  797 | `{` |
|      - |  798 | `	sxu32 n;` |
| 810985 |  799 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 807029 |  800 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 403517 |  801 | `	}` |
|      - |  802 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   3961 |  803 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - |  804 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   3961 |  805 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   3961 |  806 | `}` |
|      - |  807 |  |
|      - |  808 | `/*` |
|      - |  809 | ` * UTF-8 codepoint reader shared by the glob/fnmatch matcher in vfs.c.` |
|      - |  810 | ` * Relocated here from the removed vm_xml.c when the legacy xml_* API was` |
|      - |  811 | ` * dropped; the utf8_encode()/utf8_decode() builtins it once served were` |
|      - |  812 | ` * removed in turn (superseded by mb_convert_encoding()),` |
|      - |  813 | ` * leaving only this public-domain SQLite reader.` |
|      - |  814 | ` */` |
|      - |  815 | `/* SPDX-SnippetBegin */` |
|      - |  816 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - |  817 | `/* SPDX-License-Identifier: blessing */` |
|      - |  818 | `/*` |
|      - |  819 | ` * UTF-8 decoding routine extracted from the sqlite3 source tree.` |
|      - |  820 | ` * Original author: D. Richard Hipp (http://www.sqlite.org)` |
|      - |  821 | ` * Status: Public Domain` |
|      - |  822 | ` */` |
|      - |  823 | `/*` |
|      - |  824 | `** This lookup table is used to help decode the first byte of` |
|      - |  825 | `** a multi-byte UTF8 character.` |
|      - |  826 | `*/` |
|      - |  827 | `static const unsigned char UtfTrans1[] = {` |
|      - |  828 | `  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,` |
|      - |  829 | `  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,` |
|      - |  830 | `  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,` |
|      - |  831 | `  0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,` |
|      - |  832 | `  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,` |
|      - |  833 | `  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,` |
|      - |  834 | `  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,` |
|      - |  835 | `  0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x00, 0x00,` |
|      - |  836 | `};` |
|      - |  837 | `/*` |
|      - |  838 | `** Translate a single UTF-8 character.  Return the unicode value.` |
|      - |  839 | `**` |
|      - |  840 | `** During translation, assume that the byte that zTerm points` |
|      - |  841 | `** is a 0x00.` |
|      - |  842 | `**` |
|      - |  843 | `** Write a pointer to the next unread byte back into *pzNext.` |
|      - |  844 | `**` |
|      - |  845 | `** Notes On Invalid UTF-8:` |
|      - |  846 | `**` |
|      - |  847 | `**  *  This routine never allows a 7-bit character (0x00 through 0x7f) to` |
|      - |  848 | `**     be encoded as a multi-byte character.  Any multi-byte character that` |
|      - |  849 | `**     attempts to encode a value between 0x00 and 0x7f is rendered as 0xfffd.` |
|      - |  850 | `**` |
|      - |  851 | `**  *  This routine never allows a UTF16 surrogate value to be encoded.` |
|      - |  852 | `**     If a multi-byte character attempts to encode a value between` |
|      - |  853 | `**     0xd800 and 0xe000 then it is rendered as 0xfffd.` |
|      - |  854 | `**` |
|      - |  855 | `**  *  Bytes in the range of 0x80 through 0xbf which occur as the first` |
|      - |  856 | `**     byte of a character are interpreted as single-byte characters` |
|      - |  857 | `**     and rendered as themselves even though they are technically` |
|      - |  858 | `**     invalid characters.` |
|      - |  859 | `**` |
|      - |  860 | `**  *  This routine accepts an infinite number of different UTF8 encodings` |
|      - |  861 | `**     for unicode values 0x80 and greater.  It do not change over-length` |
|      - |  862 | `**     encodings to 0xfffd as some systems recommend.` |
|      - |  863 | `*/` |
|      - |  864 | `#define READ_UTF8(zIn, zTerm, c)                           \` |
|      - |  865 | `  c = *(zIn++);                                            \` |
|      - |  866 | `  if( c>=0xc0 ){                                           \` |
|      - |  867 | `    c = UtfTrans1[c-0xc0];                                 \` |
|      - |  868 | `    while( zIn!=zTerm && (*zIn & 0xc0)==0x80 ){            \` |
|      - |  869 | `      c = (c<<6) + (0x3f & *(zIn++));                      \` |
|      - |  870 | `    }                                                      \` |
|      - |  871 | `    if( c<0x80                                             \` |
|      - |  872 | `        \|\| (c&0xFFFFF800)==0xD800                          \` |
|      - |  873 | `        \|\| (c&0xFFFFFFFE)==0xFFFE ){  c = 0xFFFD; }        \` |
|      - |  874 | `  }` |
|    268 |  875 | `PH7_PRIVATE int PH7_Utf8Read(` |
|      - |  876 | `  const unsigned char *z,         /* First byte of UTF-8 character */` |
|      - |  877 | `  const unsigned char *zTerm,     /* Pretend this byte is 0x00 */` |
|      - |  878 | `  const unsigned char **pzNext    /* Write first byte past UTF-8 char here */` |
|      1 |  879 | `){` |
|      - |  880 | `  int c;` |
|    269 |  881 | `  READ_UTF8(z, zTerm, c);` |
|    269 |  882 | `  *pzNext = z;` |
|    269 |  883 | `  return c;` |
|      1 |  884 | `}` |
|      - |  885 | `/* SPDX-SnippetEnd */` |
|      - |  886 | `/*` |
|      - |  887 | ` * Read one STRICTLY well-formed UTF-8 sequence from z[0..n-1].` |
|      - |  888 | ` *` |
|      - |  889 | ` * Unlike PH7_Utf8Read above (the lenient SQLite reader, which renders anything` |
|      - |  890 | ` * dubious as U+FFFD and happily accepts over-long forms), this one implements` |
|      - |  891 | ` * the RFC 3629 / Unicode "Table 3-7 well-formed byte sequences" rule php uses` |
|      - |  892 | ` * wherever it has to decide whether a php string really is UTF-8:` |
|      - |  893 | ` *` |
|      - |  894 | ` *   00..7F                          one byte` |
|      - |  895 | ` *   C2..DF  80..BF                  (C0/C1 are over-long two-byte forms)` |
|      - |  896 | ` *   E0      A0..BF  80..BF          (E0 80..9F is over-long)` |
|      - |  897 | ` *   E1..EC  80..BF  80..BF` |
|      - |  898 | ` *   ED      80..9F  80..BF          (ED A0..BF is a UTF-16 surrogate)` |
|      - |  899 | ` *   EE..EF  80..BF  80..BF` |
|      - |  900 | ` *   F0      90..BF  80..BF  80..BF  (F0 80..8F is over-long)` |
|      - |  901 | ` *   F1..F3  80..BF  80..BF  80..BF` |
|      - |  902 | ` *   F4      80..8F  80..BF  80..BF  (past U+10FFFF)` |
|      - |  903 | ` *` |
|      - |  904 | ` * Returns the code point and writes the sequence length to *pLen. On an` |
|      - |  905 | ` * ill-formed sequence it returns -1 and writes 1, so a caller can apply its own` |
|      - |  906 | ` * php policy to the single offending byte (json_encode: JSON_ERROR_UTF8 or the` |
|      - |  907 | ` * JSON_INVALID_UTF8_* substitution; mb_strtolower: '?') and resume at the next` |
|      - |  908 | ` * byte exactly like php does. n must be >= 1.` |
|      - |  909 | ` */` |
|   2260 |  910 | `PH7_PRIVATE sxi32 PH7_Utf8ReadStrict(const unsigned char *z,sxu32 n,sxu32 *pLen)` |
|      1 |  911 | `{` |
|   2261 |  912 | `	sxu32 c = z[0];` |
|   2261 |  913 | `	*pLen = 1;` |
|   2261 |  914 | `	if( c < 0x80 ){` |
|   1175 |  915 | `		return (sxi32)c;` |
|      - |  916 | `	}` |
|   1087 |  917 | `	if( c >= 0xC2 && c <= 0xDF ){` |
|    463 |  918 | `		if( n < 2 \|\| (z[1] & 0xC0) != 0x80 ){` |
|     45 |  919 | `			return -1;` |
|      - |  920 | `		}` |
|    419 |  921 | `		*pLen = 2;` |
|    419 |  922 | `		return (sxi32)(((c & 0x1F) << 6) \| (z[1] & 0x3F));` |
|      - |  923 | `	}` |
|    625 |  924 | `	if( c >= 0xE0 && c <= 0xEF ){` |
|    229 |  925 | `		sxu32 iLow = (c == 0xE0) ? 0xA0 : 0x80;` |
|    229 |  926 | `		sxu32 iHigh = (c == 0xED) ? 0x9F : 0xBF;` |
|    229 |  927 | `		if( n < 3 \|\| z[1] < iLow \|\| z[1] > iHigh \|\| (z[2] & 0xC0) != 0x80 ){` |
|     83 |  928 | `			return -1;` |
|      - |  929 | `		}` |
|    147 |  930 | `		*pLen = 3;` |
|    147 |  931 | `		return (sxi32)(((c & 0x0F) << 12) \| ((z[1] & 0x3F) << 6) \| (z[2] & 0x3F));` |
|      - |  932 | `	}` |
|    397 |  933 | `	if( c >= 0xF0 && c <= 0xF4 ){` |
|     61 |  934 | `		sxu32 iLow = (c == 0xF0) ? 0x90 : 0x80;` |
|     61 |  935 | `		sxu32 iHigh = (c == 0xF4) ? 0x8F : 0xBF;` |
|     60 |  936 | `		if( n < 4 \|\| z[1] < iLow \|\| z[1] > iHigh` |
|     37 |  937 | `		 \|\| (z[2] & 0xC0) != 0x80 \|\| (z[3] & 0xC0) != 0x80 ){` |
|     31 |  938 | `			return -1;` |
|      - |  939 | `		}` |
|     31 |  940 | `		*pLen = 4;` |
|     46 |  941 | `		return (sxi32)(((c & 0x07) << 18) \| ((z[1] & 0x3F) << 12)` |
|     30 |  942 | `			\| ((z[2] & 0x3F) << 6) \| (z[3] & 0x3F));` |
|      - |  943 | `	}` |
|    337 |  944 | `	return -1; /* 80..C1 as a lead byte, or F5..FF */` |
|   1131 |  945 | `}` |
|      - |  946 |  |
