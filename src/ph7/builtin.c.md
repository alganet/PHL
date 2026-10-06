# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 420/469 lines (89.55%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|       - |    7 | `#include <locale.h>` |
|       - |    8 | `/* filter_var(FILTER_VALIDATE_FLOAT) parses with libc strtod directly because it` |
|       - |    9 | ` * needs errno==ERANGE to reject out-of-range magnitudes; SyStrToReal (also` |
|       - |   10 | ` * strtod-backed nowadays) exposes no range-error signal. */` |
|       - |   11 | `#include <stdlib.h>  /* strtod */` |
|       - |   12 | `#include <math.h>    /* HUGE_VAL */` |
|       - |   13 | `#include <errno.h>   /* ERANGE (strtod range-error signal) */` |
|       - |   14 | `#include <stdio.h>   /* snprintf (printf-family float conversions — correctly` |
|       - |   15 | `                      * rounded digits like php's zend_dtoa; see PH7_InputFormat) */` |
|       - |   16 | ``/* Shared ZPP helper for `int` parameters — defined OUTSIDE the`` |
|       - |   17 | ` * PH7_DISABLE_BUILTIN_FUNC guard because hashmap.c (array_slice) and` |
|       - |   18 | ` * builtin_math.c (intdiv) call it and both compile in the tiny build. */` |
| 1417793 |   19 | `PH7_PRIVATE sxi32 PH7_IntArgResolve(` |
|       - |   20 | `	ph7_context *pCtx,` |
|       - |   21 | `	ph7_value *pArg,` |
|       - |   22 | `	const char *zFunc,` |
|       - |   23 | `	int iArgNum,` |
|       - |   24 | `	const char *zParamName,` |
|       - |   25 | `	const char *zTypeStr,` |
|       - |   26 | `	sxi64 *pOut` |
|       5 |   27 | `){` |
| 1417798 |   28 | `	if( ph7_value_is_null(pArg) ){` |
|       - |   29 | `		/* php only DEPRECATES passing null to a non-nullable internal param; PHL` |
|       - |   30 | `		 * targets php's non-deprecated surface and rejects it with the TypeError` |
|       - |   31 | `		 * php will eventually raise. */` |
|     ! 0 |   32 | `		return PH7_VmThrowException(pCtx,` |
|       - |   33 | `			"TypeError",` |
|       - |   34 | `			"%s(): Argument #%d (%s) must be of type %s, null given",` |
|     ! 0 |   35 | `			zFunc,iArgNum,zParamName,zTypeStr` |
|       - |   36 | `			);` |
|       - |   37 | `	}` |
| 1417798 |   38 | `	if( ph7_value_is_float(pArg) ){` |
|      27 |   39 | `		double dVal = ph7_value_to_double(pArg);` |
|       - |   40 | `		sxi64 iVal;` |
|       - |   41 | `		/* php: NAN/INF/out-of-int64-range floats fail ZPP outright */` |
|      27 |   42 | `		if( !PH7_RealFitsInt64(dVal) ){` |
|     ! 0 |   43 | `			return PH7_VmThrowException(pCtx,` |
|       - |   44 | `				"TypeError",` |
|       - |   45 | `				"%s(): Argument #%d (%s) must be of type %s, float given",` |
|     ! 0 |   46 | `				zFunc,iArgNum,zParamName,zTypeStr` |
|       - |   47 | `				);` |
|       - |   48 | `		}` |
|      27 |   49 | `		iVal = (sxi64)dVal;` |
|      27 |   50 | `		if( (double)iVal != dVal ){` |
|       - |   51 | `			/* php DEPRECATES a lossy float->int; PHL rejects it (the value is not` |
|       - |   52 | `			 * representable as int). */` |
|     ! 0 |   53 | `			return PH7_VmThrowException(pCtx,` |
|       - |   54 | `				"TypeError",` |
|       - |   55 | `				"%s(): Argument #%d (%s) must be of type %s, float given",` |
|     ! 0 |   56 | `				zFunc,iArgNum,zParamName,zTypeStr` |
|       - |   57 | `				);` |
|       - |   58 | `		}` |
|      27 |   59 | `		*pOut = iVal;` |
|      27 |   60 | `		return PH7_OK;` |
|       - |   61 | `	}` |
| 1417774 |   62 | `	if( ph7_value_is_string(pArg) ){` |
|       - |   63 | `		const char *zNum;` |
|       - |   64 | `		int nSlen;` |
|      58 |   65 | `		int i,bFloat = 0;` |
|      58 |   66 | `		if( !PH7_MemObjStringIsNumeric(pArg) ){` |
|      31 |   67 | `			return PH7_VmThrowException(pCtx,` |
|       - |   68 | `				"TypeError",` |
|       - |   69 | `				"%s(): Argument #%d (%s) must be of type %s, string given",` |
|      10 |   70 | `				zFunc,iArgNum,zParamName,zTypeStr` |
|       - |   71 | `				);` |
|       - |   72 | `		}` |
|      38 |   73 | `		zNum = ph7_value_to_string(pArg,&nSlen);` |
|      88 |   74 | `		for( i = 0 ; i < nSlen ; i++ ){` |
|      56 |   75 | `			if( zNum[i] == '.' \|\| zNum[i] == 'e' \|\| zNum[i] == 'E' ){` |
|       5 |   76 | `				bFloat = 1;` |
|       5 |   77 | `				break;` |
|       - |   78 | `			}` |
|      27 |   79 | `		}` |
|      38 |   80 | `		if( bFloat ){` |
|       5 |   81 | `			double dVal = 0;` |
|       - |   82 | `			sxi64 iVal;` |
|       5 |   83 | `			SyStrToReal(zNum,(sxu32)nSlen,(void *)&dVal,0);` |
|       5 |   84 | `			if( !PH7_RealFitsInt64(dVal) ){` |
|     ! 0 |   85 | `				return PH7_VmThrowException(pCtx,` |
|       - |   86 | `					"TypeError",` |
|       - |   87 | `					"%s(): Argument #%d (%s) must be of type %s, string given",` |
|     ! 0 |   88 | `					zFunc,iArgNum,zParamName,zTypeStr` |
|       - |   89 | `					);` |
|       - |   90 | `			}` |
|       5 |   91 | `			iVal = (sxi64)dVal;` |
|       5 |   92 | `			if( (double)iVal != dVal ){` |
|       - |   93 | `				/* php DEPRECATES a lossy float-string->int; PHL rejects it. */` |
|     ! 0 |   94 | `				return PH7_VmThrowException(pCtx,` |
|       - |   95 | `					"TypeError",` |
|       - |   96 | `					"%s(): Argument #%d (%s) must be of type %s, string given",` |
|     ! 0 |   97 | `					zFunc,iArgNum,zParamName,zTypeStr` |
|       - |   98 | `					);` |
|       - |   99 | `			}` |
|       5 |  100 | `			*pOut = iVal;` |
|       5 |  101 | `			return PH7_OK;` |
|       - |  102 | `		}` |
|      34 |  103 | `		*pOut = ph7_value_to_int64(pArg);` |
|      34 |  104 | `		return PH7_OK;` |
|       - |  105 | `	}` |
| 1417718 |  106 | `	if( !ph7_value_is_int(pArg) && !ph7_value_is_bool(pArg) ){` |
|       - |  107 | `		/* Arrays, resources and objects: php names the class for objects */` |
|     ! 0 |  108 | `		const char *zType = ph7_type_name(pArg);` |
|     ! 0 |  109 | `		if( ph7_value_is_object(pArg) ){` |
|     ! 0 |  110 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     ! 0 |  111 | `			if( pInst && pInst->pClass ){` |
|     ! 0 |  112 | `				zType = SyStringData(&pInst->pClass->sName);` |
|     ! 0 |  113 | `			}` |
|     ! 0 |  114 | `		}` |
|     ! 0 |  115 | `		return PH7_VmThrowException(pCtx,` |
|       - |  116 | `			"TypeError",` |
|       - |  117 | `			"%s(): Argument #%d (%s) must be of type %s, %s given",` |
|     ! 0 |  118 | `			zFunc,iArgNum,zParamName,zTypeStr,zType` |
|       - |  119 | `			);` |
|       - |  120 | `	}` |
| 1417718 |  121 | `	*pOut = ph7_value_to_int64(pArg);` |
| 1417718 |  122 | `	return PH7_OK;` |
|  709930 |  123 | `}` |
|       - |  124 |  |
|       - |  125 | `/* This file implement built-in 'foreign' functions for the PH7 engine */` |
|       - |  126 | `/*` |
|       - |  127 | ` * Section:` |
|       - |  128 | ` *    Variable handling Functions.` |
|       - |  129 | ` * Status:` |
|       - |  130 | ` *    Stable.` |
|       - |  131 | ` */` |
|       - |  132 | `/*` |
|       - |  133 | ` * bool is_bool($var)` |
|       - |  134 | ` *  Finds out whether a variable is a boolean.` |
|       - |  135 | ` * Parameters` |
|       - |  136 | ` *   $var: The variable being evaluated.` |
|       - |  137 | ` * Return` |
|       - |  138 | ` *  TRUE if var is a boolean. False otherwise.` |
|       - |  139 | ` */` |
|     433 |  140 | `static int PH7_builtin_is_bool(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  141 | `{` |
|     436 |  142 | `	int res = 0; /* Assume false by default */` |
|     436 |  143 | `	if( nArg > 0 ){` |
|     436 |  144 | `		res = ph7_value_is_bool(apArg[0]);` |
|     216 |  145 | `	}` |
|       - |  146 | `	/* Query result */` |
|     436 |  147 | `	ph7_result_bool(pCtx,res);` |
|     436 |  148 | `	return PH7_OK;` |
|       3 |  149 | `}` |
|       - |  150 | `/*` |
|       - |  151 | ` * bool is_float($var)` |
|       - |  152 | ` * bool is_double($var)` |
|       - |  153 | ` *  Finds out whether a variable is a float.` |
|       - |  154 | ` * Parameters` |
|       - |  155 | ` *   $var: The variable being evaluated.` |
|       - |  156 | ` * Return` |
|       - |  157 | ` *  TRUE if var is a float. False otherwise.` |
|       - |  158 | ` */` |
|    6322 |  159 | `static int PH7_builtin_is_float(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  160 | `{` |
|    6323 |  161 | `	int res = 0; /* Assume false by default */` |
|    6323 |  162 | `	if( nArg > 0 ){` |
|    6323 |  163 | `		res = ph7_value_is_float(apArg[0]);` |
|    3161 |  164 | `	}` |
|       - |  165 | `	/* Query result */` |
|    6323 |  166 | `	ph7_result_bool(pCtx,res);` |
|    6323 |  167 | `	return PH7_OK;` |
|       1 |  168 | `}` |
|       - |  169 | `/*` |
|       - |  170 | ` * bool is_int($var)` |
|       - |  171 | ` * bool is_integer($var)` |
|       - |  172 | ` * bool is_long($var)` |
|       - |  173 | ` *  Finds out whether a variable is an integer.` |
|       - |  174 | ` * Parameters` |
|       - |  175 | ` *   $var: The variable being evaluated.` |
|       - |  176 | ` * Return` |
|       - |  177 | ` *  TRUE if var is an integer. False otherwise.` |
|       - |  178 | ` */` |
|    1153 |  179 | `static int PH7_builtin_is_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  180 | `{` |
|    1157 |  181 | `	int res = 0; /* Assume false by default */` |
|    1157 |  182 | `	if( nArg > 0 ){` |
|       - |  183 | `		/* Strict PHP identity: a float is never an int, even when it holds an` |
|       - |  184 | `		 * integer value (1.0). An integer-valued real carries both MEMOBJ_INT` |
|       - |  185 | `		 * (cached) and MEMOBJ_REAL, so REAL must be excluded here. */` |
|    1157 |  186 | `		res = ph7_value_is_int(apArg[0]) && !ph7_value_is_float(apArg[0]);` |
|     557 |  187 | `	}` |
|       - |  188 | `	/* Query result */` |
|    1157 |  189 | `	ph7_result_bool(pCtx,res);` |
|    1157 |  190 | `	return PH7_OK;` |
|       4 |  191 | `}` |
|       - |  192 | `/*` |
|       - |  193 | ` * bool is_string($var)` |
|       - |  194 | ` *  Finds out whether a variable is a string.` |
|       - |  195 | ` * Parameters` |
|       - |  196 | ` *   $var: The variable being evaluated.` |
|       - |  197 | ` * Return` |
|       - |  198 | ` *  TRUE if var is string. False otherwise.` |
|       - |  199 | ` */` |
|    1796 |  200 | `static int PH7_builtin_is_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  201 | `{` |
|    1801 |  202 | `	int res = 0; /* Assume false by default */` |
|    1801 |  203 | `	if( nArg > 0 ){` |
|    1801 |  204 | `		res = ph7_value_is_string(apArg[0]);` |
|     897 |  205 | `	}` |
|       - |  206 | `	/* Query result */` |
|    1801 |  207 | `	ph7_result_bool(pCtx,res);` |
|    1801 |  208 | `	return PH7_OK;` |
|       5 |  209 | `}` |
|       - |  210 | `/*` |
|       - |  211 | ` * bool is_null($var)` |
|       - |  212 | ` *  Finds out whether a variable is NULL.` |
|       - |  213 | ` * Parameters` |
|       - |  214 | ` *   $var: The variable being evaluated.` |
|       - |  215 | ` * Return` |
|       - |  216 | ` *  TRUE if var is NULL. False otherwise.` |
|       - |  217 | ` */` |
|      82 |  218 | `static int PH7_builtin_is_null(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  219 | `{` |
|      86 |  220 | `	int res = 0; /* Assume false by default */` |
|      86 |  221 | `	if( nArg > 0 ){` |
|      86 |  222 | `		res = ph7_value_is_null(apArg[0]);` |
|      41 |  223 | `	}` |
|       - |  224 | `	/* Query result */` |
|      86 |  225 | `	ph7_result_bool(pCtx,res);` |
|      86 |  226 | `	return PH7_OK;` |
|       4 |  227 | `}` |
|       - |  228 | `/*` |
|       - |  229 | ` * bool is_numeric($var)` |
|       - |  230 | ` *  Find out whether a variable is NULL.` |
|       - |  231 | ` * Parameters` |
|       - |  232 | ` *  $var: The variable being evaluated.` |
|       - |  233 | ` * Return` |
|       - |  234 | ` *  True if var is numeric. False otherwise.` |
|       - |  235 | ` */` |
|      94 |  236 | `static int PH7_builtin_is_numeric(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  237 | `{` |
|      99 |  238 | `	int res = 0; /* Assume false by default */` |
|      99 |  239 | `	if( nArg > 0 ){` |
|       - |  240 | `		/* Strict PHP semantics: only int/float and numeric strings are numeric.` |
|       - |  241 | `		 * PHL's lenient helper also reports booleans as numeric (they coerce for` |
|       - |  242 | `		 * arithmetic), but php's is_numeric() rejects true/false, so exclude` |
|       - |  243 | `		 * MEMOBJ_BOOL here. */` |
|      99 |  244 | `		res = ph7_value_is_numeric(apArg[0]) && !ph7_value_is_bool(apArg[0]);` |
|      47 |  245 | `	}` |
|       - |  246 | `	/* Query result */` |
|      99 |  247 | `	ph7_result_bool(pCtx,res);` |
|      99 |  248 | `	return PH7_OK;` |
|       5 |  249 | `}` |
|       - |  250 | `/*` |
|       - |  251 | ` * bool is_scalar($var)` |
|       - |  252 | ` *  Find out whether a variable is a scalar.` |
|       - |  253 | ` * Parameters` |
|       - |  254 | ` *  $var: The variable being evaluated.` |
|       - |  255 | ` * Return` |
|       - |  256 | ` *  True if var is scalar. False otherwise.` |
|       - |  257 | ` */` |
|      30 |  258 | `static int PH7_builtin_is_scalar(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  259 | `{` |
|      31 |  260 | `	int res = 0; /* Assume false by default */` |
|      31 |  261 | `	if( nArg > 0 ){` |
|       - |  262 | `		/* Strict PHP semantics: scalars are int/float/string/bool. PHL's` |
|       - |  263 | `		 * MEMOBJ_SCALAR bucket also includes NULL, but php's is_scalar(null) is` |
|       - |  264 | `		 * false, so exclude the NULL case. */` |
|      31 |  265 | `		res = ph7_value_is_scalar(apArg[0]) && !ph7_value_is_null(apArg[0]);` |
|      15 |  266 | `	}` |
|       - |  267 | `	/* Query result */` |
|      31 |  268 | `	ph7_result_bool(pCtx,res);` |
|      31 |  269 | `	return PH7_OK;` |
|       1 |  270 | `}` |
|       - |  271 | `/*` |
|       - |  272 | ` * bool is_array($var)` |
|       - |  273 | ` *  Find out whether a variable is an array.` |
|       - |  274 | ` * Parameters` |
|       - |  275 | ` *  $var: The variable being evaluated.` |
|       - |  276 | ` * Return` |
|       - |  277 | ` *  True if var is an array. False otherwise.` |
|       - |  278 | ` */` |
|    8916 |  279 | `static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  280 | `{` |
|    8921 |  281 | `	int res = 0; /* Assume false by default */` |
|    8921 |  282 | `	if( nArg > 0 ){` |
|    8921 |  283 | `		res = ph7_value_is_array(apArg[0]);` |
|    4456 |  284 | `	}` |
|       - |  285 | `	/* Query result */` |
|    8921 |  286 | `	ph7_result_bool(pCtx,res);` |
|    8921 |  287 | `	return PH7_OK;` |
|       5 |  288 | `}` |
|       - |  289 | `/*` |
|       - |  290 | ` * bool is_object($var)` |
|       - |  291 | ` *  Find out whether a variable is an object.` |
|       - |  292 | ` * Parameters` |
|       - |  293 | ` *  $var: The variable being evaluated.` |
|       - |  294 | ` * Return` |
|       - |  295 | ` *  True if var is an object. False otherwise.` |
|       - |  296 | ` */` |
|    1566 |  297 | `static int PH7_builtin_is_object(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  298 | `{` |
|    1571 |  299 | `	int res = 0; /* Assume false by default */` |
|    1571 |  300 | `	if( nArg > 0 ){` |
|    1571 |  301 | `		res = ph7_value_is_object(apArg[0]);` |
|     783 |  302 | `	}` |
|       - |  303 | `	/* Query result */` |
|    1571 |  304 | `	ph7_result_bool(pCtx,res);` |
|    1571 |  305 | `	return PH7_OK;` |
|       5 |  306 | `}` |
|       - |  307 | `/*` |
|       - |  308 | ` * bool is_resource($var)` |
|       - |  309 | ` *  Find out whether a variable is a resource.` |
|       - |  310 | ` * Parameters` |
|       - |  311 | ` *  $var: The variable being evaluated.` |
|       - |  312 | ` * Return` |
|       - |  313 | ` *  True if a resource. False otherwise.` |
|       - |  314 | ` */` |
|    1170 |  315 | `static int PH7_builtin_is_resource(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  316 | `{` |
|    1175 |  317 | `	int res = 0; /* Assume false by default */` |
|    1175 |  318 | `	if( nArg > 0 && ph7_value_is_resource(apArg[0]) ){` |
|       - |  319 | `		/* A handle closed via fclose()/closedir()/pclose() is no longer a` |
|       - |  320 | `		 * live resource — php's is_resource() returns false for it. */` |
|     915 |  321 | `		res = !PH7_VfsResourceIsClosed(apArg[0]->x.pOther);` |
|     455 |  322 | `	}` |
|    1175 |  323 | `	ph7_result_bool(pCtx,res);` |
|    1175 |  324 | `	return PH7_OK;` |
|       5 |  325 | `}` |
|       - |  326 | `/*` |
|       - |  327 | ` * float floatval($var)` |
|       - |  328 | ` *  Get float value of a variable.` |
|       - |  329 | ` * Parameter` |
|       - |  330 | ` *  $var: The variable being processed.` |
|       - |  331 | ` * Return` |
|       - |  332 | ` *  the float value of a variable.` |
|       - |  333 | ` */` |
|      26 |  334 | `static int PH7_builtin_floatval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  335 | `{` |
|      29 |  336 | `	if( nArg < 1 ){` |
|       - |  337 | `		/* return 0.0 */` |
|     ! 0 |  338 | `		ph7_result_double(pCtx,0);` |
|     ! 0 |  339 | `	}else{` |
|       - |  340 | `		double dval;` |
|       - |  341 | `		/* Perform the cast */` |
|      29 |  342 | `		dval = ph7_value_to_double(apArg[0]);` |
|      29 |  343 | `		ph7_result_double(pCtx,dval);` |
|       - |  344 | `	}` |
|      29 |  345 | `	return PH7_OK;` |
|       3 |  346 | `}` |
|       - |  347 | `/*` |
|       - |  348 | ` * One C strtol() run, which is what php's intval() calls (ZEND_STRTOL in` |
|       - |  349 | ` * ext/standard/type.c). The rules are strtol's, not php's own numeric-string` |
|       - |  350 | ` * ones, and every one of them is observable:` |
|       - |  351 | ` *  - leading whitespace, then at most ONE sign;` |
|       - |  352 | ` *  - base 16 skips an optional "0x"/"0X"; base 0 PICKS the base from the same` |
|       - |  353 | ` *    prefix ("0x" -> 16, a leading "0" -> 8, otherwise 10);` |
|       - |  354 | ` *  - the scan stops at the first byte the base cannot spell, so "12ag" in base` |
|       - |  355 | ` *    16 is 0x12a and "0x0x1" is 0;` |
|       - |  356 | ` *  - a base outside 2..36 makes strtol answer 0 -- php raises nothing for it;` |
|       - |  357 | ` *  - the result SATURATES at PHP_INT_MAX/PHP_INT_MIN instead of wrapping.` |
|       - |  358 | `` * `iPreSign` is the sign php pastes in FRONT of the string it hands over (see`` |
|       - |  359 | ` * IntvalStrToInt64): a sign already consumed, so neither whitespace nor a` |
|       - |  360 | ` * second sign may follow it.` |
|       - |  361 | ` */` |
|     178 |  362 | `static sxi64 IntvalStrtol(int iPreSign,const char *zIn,int nLen,int iBase)` |
|       1 |  363 | `{` |
|     179 |  364 | `	sxu64 uLimit,uCutoff,uAcc = 0;` |
|     179 |  365 | `	int iCutlim,iSign = 1,bAny = 0,bOvf = 0;` |
|     179 |  366 | `	int i = 0;` |
|     179 |  367 | `	if( iPreSign ){` |
|      15 |  368 | `		iSign = (iPreSign == '-') ? -1 : 1;` |
|       8 |  369 | `	}else{` |
|     187 |  370 | `		while( i < nLen && SyisSpace((unsigned char)zIn[i]) ){` |
|      23 |  371 | `			i++;` |
|       1 |  372 | `		}` |
|     165 |  373 | `		if( i < nLen && (zIn[i] == '-' \|\| zIn[i] == '+') ){` |
|      33 |  374 | `			iSign = (zIn[i] == '-') ? -1 : 1;` |
|      33 |  375 | `			i++;` |
|      16 |  376 | `		}` |
|       - |  377 | `	}` |
|     178 |  378 | `	if( (iBase == 0 \|\| iBase == 16) && i + 1 < nLen` |
|     127 |  379 | `	 && zIn[i] == '0' && (zIn[i+1] == 'x' \|\| zIn[i+1] == 'X') ){` |
|      15 |  380 | `		i += 2;` |
|      15 |  381 | `		iBase = 16;` |
|     172 |  382 | `	}else if( (iBase == 0 \|\| iBase == 2) && i + 2 < nLen` |
|     114 |  383 | `	 && zIn[i] == '0' && (zIn[i+1] == 'b' \|\| zIn[i+1] == 'B')` |
|      52 |  384 | `	 && (zIn[i+2] == '0' \|\| zIn[i+2] == '1') ){` |
|       - |  385 | `		/* The conversion accepts a binary prefix of its own, on TOP of the one` |
|       - |  386 | `		 * IntvalStrToInt64 strips -- which is why intval("0b0b1",2) is 1 and a` |
|       - |  387 | `		 * THIRD prefix stops the scan: intval("0b0b0b1",2) is 0. It is also the` |
|       - |  388 | `		 * only prefix reader a base that NARROWED to 0 or 2 gets, since the` |
|       - |  389 | `		 * strip upstream reads the base at full width. */` |
|      39 |  390 | `		i += 2;` |
|      39 |  391 | `		iBase = 2;` |
|     140 |  392 | `	}else if( iBase == 0 ){` |
|      15 |  393 | `		iBase = (i < nLen && zIn[i] == '0') ? 8 : 10;` |
|       7 |  394 | `	}` |
|     173 |  395 | `	if( iBase < 2 \|\| iBase > 36 ){` |
|       9 |  396 | `		return 0;` |
|       - |  397 | `	}` |
|       - |  398 | `	/* strtol's own overflow test: the magnitude a negative result may reach is` |
|       - |  399 | `	 * one larger than a positive one, so the cutoff is computed per sign. */` |
|     171 |  400 | `	uLimit  = (iSign < 0) ? (sxu64)SXI64_HIGH + 1 : (sxu64)SXI64_HIGH;` |
|     171 |  401 | `	uCutoff = uLimit / (sxu64)iBase;` |
|     171 |  402 | `	iCutlim = (int)(uLimit % (sxu64)iBase);` |
|     583 |  403 | `	for( ; i < nLen ; ++i ){` |
|     469 |  404 | `		int c = (unsigned char)zIn[i];` |
|     469 |  405 | `		if( c >= '0' && c <= '9' ){` |
|     269 |  406 | `			c -= '0';` |
|     335 |  407 | `		}else if( c >= 'A' && c <= 'Z' ){` |
|       5 |  408 | `			c -= 'A' - 10;` |
|     199 |  409 | `		}else if( c >= 'a' && c <= 'z' ){` |
|     183 |  410 | `			c -= 'a' - 10;` |
|      92 |  411 | `		}else{` |
|       8 |  412 | `			break;` |
|       - |  413 | `		}` |
|     455 |  414 | `		if( c >= iBase ){` |
|      43 |  415 | `			break;` |
|       - |  416 | `		}` |
|     413 |  417 | `		bAny = 1;` |
|     413 |  418 | `		if( bOvf \|\| uAcc > uCutoff \|\| (uAcc == uCutoff && c > iCutlim) ){` |
|      11 |  419 | `			bOvf = 1;   /* keep consuming digits, the answer is pinned */` |
|      11 |  420 | `			continue;` |
|       - |  421 | `		}` |
|     403 |  422 | `		uAcc = uAcc * (sxu64)iBase + (sxu64)c;` |
|     202 |  423 | `	}` |
|     171 |  424 | `	if( bOvf ){` |
|       7 |  425 | `		return (iSign < 0) ? (-(sxi64)SXI64_HIGH - 1) : (sxi64)SXI64_HIGH;` |
|       - |  426 | `	}` |
|     165 |  427 | `	if( !bAny ){` |
|      23 |  428 | `		return 0;` |
|       - |  429 | `	}` |
|     143 |  430 | `	if( iSign < 0 ){` |
|       - |  431 | `		/* uAcc may be exactly 2^63 here, which no sxi64 holds: PHP_INT_MIN is` |
|       - |  432 | `		 * its negation and negating the SIGNED value would be undefined. */` |
|      21 |  433 | `		return (uAcc == (sxu64)SXI64_HIGH + 1) ? (-(sxi64)SXI64_HIGH - 1) : -(sxi64)uAcc;` |
|       - |  434 | `	}` |
|     123 |  435 | `	return (sxi64)uAcc;` |
|      90 |  436 | `}` |
|       - |  437 | `/*` |
|       - |  438 | ` * php's intval() string path. strtol() knows "0x" but not "0b", so php strips a` |
|       - |  439 | ` * binary prefix ITSELF -- for base 2 and for base 0 -- by building a fresh` |
|       - |  440 | ` * string out of the sign it found and the bytes past the "0b", and running` |
|       - |  441 | ` * strtol over THAT. The rebuild is observable, because strtol then runs its` |
|       - |  442 | `` * whole prelude again over the remainder: `intval("0b-1",2)` is -1 and`` |
|       - |  443 | `` * `intval("0b 1",0)` is 1, while a sign BEFORE the prefix is already spent, so`` |
|       - |  444 | `` * `intval("-0b-1",2)` is 0. php hands strtol() the C string, so an embedded NUL`` |
|       - |  445 | ` * truncates: intval("12\0 34",16) is 0x12. That truncation is copied too.` |
|       - |  446 | ` */` |
|     178 |  447 | `static sxi64 IntvalStrToInt64(const char *zIn,int nLen,sxi64 iBase64,int iBase)` |
|       1 |  448 | `{` |
|     179 |  449 | `	int i = 0;` |
|       - |  450 | `	/* An embedded NUL ends the string for strtol(). */` |
|    1031 |  451 | `	while( i < nLen && zIn[i] != 0 ){` |
|     853 |  452 | `		i++;` |
|       1 |  453 | `	}` |
|     179 |  454 | `	nLen = i;` |
|     179 |  455 | `	i = 0;` |
|     189 |  456 | `	while( i < nLen && SyisSpace((unsigned char)zIn[i]) ){` |
|      11 |  457 | `		i++;` |
|       1 |  458 | `	}` |
|       - |  459 | `	/* php's own strip reads the base at FULL width, one step before the narrowing` |
|       - |  460 | `	 * cast the conversion below gets -- so base 2^32+2 does not strip here even` |
|       - |  461 | `	 * though it converts in base 2. Only when something FOLLOWS the prefix, too:` |
|       - |  462 | `	 * "0b" alone stays a base-2 zero. */` |
|     179 |  463 | `	if( (iBase64 == 0 \|\| iBase64 == 2) && nLen - i > 2 ){` |
|      83 |  464 | `		int off = (zIn[i] == '-' \|\| zIn[i] == '+') ? 1 : 0;` |
|      83 |  465 | `		if( zIn[i+off] == '0' && (zIn[i+off+1] == 'b' \|\| zIn[i+off+1] == 'B') ){` |
|      73 |  466 | `			int iPreSign = off ? (unsigned char)zIn[i] : 0;` |
|      73 |  467 | `			i += off + 2;` |
|      73 |  468 | `			return IntvalStrtol(iPreSign,&zIn[i],nLen - i,2);` |
|       - |  469 | `		}` |
|       5 |  470 | `	}` |
|     107 |  471 | `	return IntvalStrtol(0,zIn,nLen,iBase);` |
|      90 |  472 | `}` |
|       - |  473 | `/*` |
|       - |  474 | ` * int intval(mixed $value, int $base = 10)` |
|       - |  475 | ` *  Get integer value of a variable.` |
|       - |  476 | ` * Parameters` |
|       - |  477 | ` *  $value: The variable being processed.` |
|       - |  478 | ` *  $base: The base $value is written in -- read ONLY when $value is a string` |
|       - |  479 | ` *   and the base is not 10. Every other value takes the ordinary int cast and` |
|       - |  480 | ` *   ignores $base entirely, an out-of-range one included.` |
|       - |  481 | ` * Return` |
|       - |  482 | ` *  the int value of a variable.` |
|       - |  483 | ` */` |
| 1170946 |  484 | `static int PH7_builtin_intval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  485 | `{` |
| 1170949 |  486 | `	if( nArg < 1 ){` |
|       - |  487 | `		/* return 0 */` |
|     ! 0 |  488 | `		ph7_result_int(pCtx,0);` |
|     ! 0 |  489 | `	}else{` |
|       - |  490 | `		sxi64 iVal;` |
| 1170949 |  491 | `		sxi64 iBase = 10;` |
| 1170949 |  492 | `		if( nArg > 1 ){` |
|     203 |  493 | `			iBase = ph7_value_to_int64(apArg[1]);` |
|     101 |  494 | `		}` |
| 1171038 |  495 | `		if( iBase != 10 && ph7_value_is_string(apArg[0]) ){` |
|       - |  496 | `			/* The only path php reads $base on -- and the "is it 10?" test above is` |
|       - |  497 | `			 * the LAST thing to see the argument at full width. Everything past it` |
|       - |  498 | ``			 * is strtol's `int base` parameter, which php reaches through a plain`` |
|       - |  499 | `			 * narrowing cast, so a base of 2^32+16 really does read as 16 and` |
|       - |  500 | `			 * PHP_INT_MIN really does read as 0 (auto-detect). Spelled through` |
|       - |  501 | `			 * unsigned arithmetic because the two's-complement wrap of an` |
|       - |  502 | `			 * out-of-range signed conversion is implementation-defined. */` |
|       - |  503 | `			int nLen;` |
|     179 |  504 | `			const char *zVal = ph7_value_to_string(apArg[0],&nLen);` |
|     179 |  505 | `			sxu32 uB = (sxu32)((sxu64)iBase & 0xFFFFFFFF);` |
|     179 |  506 | `			int iB = (uB <= (sxu32)SXI32_HIGH)` |
|      89 |  507 | `				? (int)uB : -(int)(SXU32_HIGH - uB) - 1;` |
|     179 |  508 | `			iVal = IntvalStrToInt64(zVal,nLen,iBase,iB);` |
|      90 |  509 | `		}else{` |
|       - |  510 | ``			/* Perform the cast -- the same one the `(int)` operator performs, so`` |
|       - |  511 | `			 * a float no int can hold warns here too. */` |
| 1170771 |  512 | `			PH7_MemObjWarnIntCast(apArg[0]);` |
| 1170771 |  513 | `			iVal = ph7_value_to_int64(apArg[0]);` |
|       - |  514 | `		}` |
| 1170949 |  515 | `		ph7_result_int64(pCtx,iVal);` |
|       - |  516 | `	}` |
| 1170949 |  517 | `	return PH7_OK;` |
|       3 |  518 | `}` |
|       - |  519 | `/*` |
|       - |  520 | ` * string strval($var)` |
|       - |  521 | ` *  Get the string representation of a variable.` |
|       - |  522 | ` * Parameter` |
|       - |  523 | ` *  $var: The variable being processed.` |
|       - |  524 | ` * Return` |
|       - |  525 | ` *  the string value of a variable.` |
|       - |  526 | ` */` |
|      72 |  527 | `static int PH7_builtin_strval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  528 | `{` |
|      76 |  529 | `	if( nArg < 1 ){` |
|       - |  530 | `		/* return NULL */` |
|     ! 0 |  531 | `		ph7_result_null(pCtx);` |
|     ! 0 |  532 | `	}else{` |
|       - |  533 | `		const char *zVal;` |
|      76 |  534 | `		int iLen = 0; /* cc -O6 warning */` |
|       - |  535 | `		/* Perform the cast. It is the USER-VISIBLE one: strval() is php's` |
|       - |  536 | `		 * (string) cast spelled as a function, so an object with no` |
|       - |  537 | `		 * __toString() throws there too (it used to answer "Object"). */` |
|      76 |  538 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&zVal,&iLen);` |
|      76 |  539 | `		if( rcSv != SXRET_OK ){` |
|       3 |  540 | `			return rcSv;` |
|       - |  541 | `		}` |
|      74 |  542 | `		ph7_result_string(pCtx,zVal,iLen);` |
|       - |  543 | `	}` |
|      74 |  544 | `	return PH7_OK;` |
|      18 |  545 | `}` |
|       - |  546 | `/*` |
|       - |  547 | ` * bool boolval($var)` |
|       - |  548 | ` *  Get the boolean value of a variable.` |
|       - |  549 | ` * Parameter` |
|       - |  550 | ` *  $var: The variable being processed.` |
|       - |  551 | ` * Return` |
|       - |  552 | ` *  the bool value of a variable.` |
|       - |  553 | ` */` |
|      24 |  554 | `static int PH7_builtin_boolval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  555 | `{` |
|       - |  556 | `	int bVal;` |
|      25 |  557 | `	if( nArg != 1 ){` |
|     ! 0 |  558 | `		return PH7_VmThrowException(pCtx,` |
|       - |  559 | `			"ArgumentCountError",` |
|       - |  560 | `			"boolval() expects exactly 1 argument, %d given",` |
|     ! 0 |  561 | `			nArg` |
|       - |  562 | `			);` |
|       - |  563 | `	}` |
|       - |  564 | `	/* Perform the cast */` |
|      25 |  565 | `	bVal = ph7_value_to_bool(apArg[0]);` |
|      25 |  566 | `	ph7_result_bool(pCtx,bVal);` |
|      25 |  567 | `	return PH7_OK;` |
|      13 |  568 | `}` |
|       - |  569 | `/*` |
|       - |  570 | ` * bool empty($var)` |
|       - |  571 | ` *  Determine whether a variable is empty.` |
|       - |  572 | ` * Parameters` |
|       - |  573 | ` *   $var: The variable being checked.` |
|       - |  574 | ` * Return` |
|       - |  575 | ` *  0 if var has a non-empty and non-zero value.1 otherwise.` |
|       - |  576 | ` */` |
|   70974 |  577 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  578 | `{` |
|   70979 |  579 | `	int res = 1; /* Assume empty by default */` |
|   70979 |  580 | `	if( nArg > 0 ){` |
|   70979 |  581 | `		res = ph7_value_is_empty(apArg[0]);` |
|   35478 |  582 | `	}` |
|   70979 |  583 | `	ph7_result_bool(pCtx,res);` |
|   70979 |  584 | `	return PH7_OK;` |
|       - |  585 |  |
|       5 |  586 | `}` |
|       - |  587 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|       - |  588 | `#define PH7_NEED_BUILTIN_REG 1` |
|       - |  589 | `#endif` |
|       - |  590 | `#ifndef PH7_DISABLE_DISK_IO` |
|       - |  591 | `#define PH7_NEED_FMT_AND_INI 1` |
|       - |  592 | `#endif` |
|       - |  593 |  |
|       - |  594 | `/* Math functions moved to builtin_math.c */` |
|       - |  595 |  |
|       - |  596 | `/* Table of the built-in functions */` |
|       - |  597 | `/*` |
|       - |  598 | `` * One candidate name applied. php reads `"0"` as a QUERY -- "the locale setting is`` |
|       - |  599 | ` * not affected, only the current setting is returned" -- which is how a script asks` |
|       - |  600 | `` * what is in force, and is also what `setlocale(LC_ALL, 0)` spells once the integer`` |
|       - |  601 | ` * has been cast to a string. An EMPTY name asks the environment, as C does.` |
|       - |  602 | ` */` |
|      26 |  603 | `static const char * SetLocaleApply(int iCat,const char *zName,int nName)` |
|       4 |  604 | `{` |
|      30 |  605 | `	if( nName == 1 && zName[0] == '0' ){` |
|       5 |  606 | `		return setlocale(iCat,0);` |
|       - |  607 | `	}` |
|      26 |  608 | `	return setlocale(iCat,nName > 0 ? zName : "");` |
|      15 |  609 | `}` |
|       - |  610 | `/* One candidate list walked for setlocale(): the first name the system accepts wins. */` |
|       - |  611 | `typedef struct SetLocaleTry SetLocaleTry;` |
|       - |  612 | `struct SetLocaleTry { int iCat; const char *zRes; };` |
|       4 |  613 | `static int SetLocaleWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|       1 |  614 | `{` |
|       5 |  615 | `	SetLocaleTry *p = (SetLocaleTry *)pUserData;` |
|       - |  616 | `	int nName;` |
|       - |  617 | `	const char *zName;` |
|       2 |  618 | `	SXUNUSED(pKey);` |
|       5 |  619 | `	if( p->zRes ){` |
|     ! 0 |  620 | `		return PH7_OK;   /* already settled */` |
|       - |  621 | `	}` |
|       5 |  622 | `	zName = ph7_value_to_string(pData,&nName);` |
|       5 |  623 | `	p->zRes = SetLocaleApply(p->iCat,zName,nName);` |
|       5 |  624 | `	return PH7_OK;` |
|       3 |  625 | `}` |
|       - |  626 | `/*` |
|       - |  627 | ` * string\|false setlocale(int $category, array\|string $locales, string ...$rest)` |
|       - |  628 | ` *` |
|       - |  629 | ` * php's own LC_* numbering maps to the platform's <locale.h> macros here, so a` |
|       - |  630 | ` * script keeps php's numbers whatever the C library uses. Each candidate locale is` |
|       - |  631 | `` * tried in order and the FIRST one the system accepts wins; `""` asks the`` |
|       - |  632 | `` * environment and `"0"` (or 0) only QUERIES, changing nothing. false when none of`` |
|       - |  633 | ` * them is available -- which is php's answer too, on a box without that locale.` |
|       - |  634 | ` *` |
|       - |  635 | `` * Composer's `bin/composer` opens with `setlocale(LC_ALL, 'C')`, so without this`` |
|       - |  636 | ` * the tool did not reach its second line.` |
|       - |  637 | ` *` |
|       - |  638 | ` * PHL's own number and date formatting is its own code and does not read the C` |
|       - |  639 | ` * locale, exactly as php 8's does not -- so this changes what the C library does` |
|       - |  640 | ` * for the embedder, and nothing about how PHL prints.` |
|       - |  641 | ` */` |
|      24 |  642 | `static int PH7_builtin_setlocale(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  643 | `{` |
|       - |  644 | `	/* php's category number -> this platform's macro, in php's order. */` |
|       - |  645 | `	static const int aCat[] = {` |
|       - |  646 | `		LC_CTYPE, LC_NUMERIC, LC_TIME, LC_COLLATE, LC_MONETARY,` |
|       - |  647 | `#ifdef LC_MESSAGES` |
|       - |  648 | `		LC_MESSAGES,` |
|       - |  649 | `#else` |
|       - |  650 | `		LC_ALL,   /* Windows has no LC_MESSAGES; php maps it to LC_ALL there */` |
|       - |  651 | `#endif` |
|       - |  652 | `		LC_ALL` |
|       - |  653 | `	};` |
|       - |  654 | `	int iCat,i;` |
|      28 |  655 | `	const char *zRes = 0;` |
|      28 |  656 | `	if( nArg < 1 ){` |
|     ! 0 |  657 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  658 | `		return PH7_OK;` |
|       - |  659 | `	}` |
|      28 |  660 | `	iCat = (int)ph7_value_to_int(apArg[0]);` |
|      28 |  661 | `	if( iCat < 0 \|\| iCat >= (int)SX_ARRAYSIZE(aCat) ){` |
|       3 |  662 | `		ph7_result_bool(pCtx,0);` |
|       3 |  663 | `		return PH7_OK;` |
|       - |  664 | `	}` |
|      26 |  665 | `	if( nArg < 2 ){` |
|       - |  666 | `		/* Query only. */` |
|     ! 0 |  667 | `		zRes = setlocale(aCat[iCat],0);` |
|     ! 0 |  668 | `		if( zRes ){` |
|     ! 0 |  669 | `			ph7_result_string(pCtx,zRes,-1);` |
|     ! 0 |  670 | `		}else{` |
|     ! 0 |  671 | `			ph7_result_bool(pCtx,0);` |
|       - |  672 | `		}` |
|     ! 0 |  673 | `		return PH7_OK;` |
|       - |  674 | `	}` |
|      50 |  675 | `	for( i = 1 ; i < nArg && zRes == 0 ; i++ ){` |
|      28 |  676 | `		if( ph7_value_is_array(apArg[i]) ){` |
|       - |  677 | `			/* php accepts ONE array of candidates in the second position. */` |
|       - |  678 | `			SetLocaleTry sTry;` |
|       3 |  679 | `			sTry.iCat = aCat[iCat];` |
|       3 |  680 | `			sTry.zRes = 0;` |
|       3 |  681 | `			ph7_array_walk(apArg[i],SetLocaleWalker,&sTry);` |
|       3 |  682 | `			zRes = sTry.zRes;` |
|       3 |  683 | `			continue;` |
|       - |  684 | `		}` |
|       - |  685 | `		{` |
|       - |  686 | `			int nName;` |
|      26 |  687 | `			const char *zName = ph7_value_to_string(apArg[i],&nName);` |
|      26 |  688 | `			zRes = SetLocaleApply(aCat[iCat],zName,nName);` |
|       - |  689 | `		}` |
|      13 |  690 | `	}` |
|      26 |  691 | `	if( zRes ){` |
|      24 |  692 | `		ph7_result_string(pCtx,zRes,-1);` |
|      12 |  693 | `	}else{` |
|       3 |  694 | `		ph7_result_bool(pCtx,0);` |
|       - |  695 | `	}` |
|      26 |  696 | `	return PH7_OK;` |
|      14 |  697 | `}` |
|       - |  698 | `/* php registers sys_getloadavg() only under its HAVE_GETLOADAVG configure test.` |
|       - |  699 | ` * The call is a BSD one that glibc and the BSDs (macOS included) carry and neither` |
|       - |  700 | ` * Windows nor an embedded libc does, so the same list decides it here. */` |
|       - |  701 | `#if !defined(__WINNT__) && (defined(__linux__) \|\| defined(__APPLE__) \` |
|       - |  702 | `	\|\| defined(__FreeBSD__) \|\| defined(__NetBSD__) \|\| defined(__OpenBSD__) \` |
|       - |  703 | `	\|\| defined(__DragonFly__))` |
|       - |  704 | `#define PH7_HAVE_GETLOADAVG 1` |
|       - |  705 | `#endif` |
|       - |  706 | `#ifdef PH7_HAVE_GETLOADAVG` |
|       - |  707 | `/*` |
|       - |  708 | ` * array\|false sys_getloadavg()` |
|       - |  709 | ` *  The system's 1/5/15-minute load averages. php builds this one only where the` |
|       - |  710 | ` *  C library has getloadavg(), which is not Windows -- so a program that guards` |
|       - |  711 | ` *  its use with function_exists() (monolog's LoadAverageProcessor does) reads` |
|       - |  712 | ` *  the same FALSE there that it reads under php.` |
|       - |  713 | ` */` |
|       2 |  714 | `static int PH7_builtin_sys_getloadavg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       - |  715 | `{` |
|       - |  716 | `	double aLoad[3];` |
|       - |  717 | `	ph7_value *pArray,*pVal;` |
|       - |  718 | `	int i;` |
|       1 |  719 | `	SXUNUSED(nArg);` |
|       1 |  720 | `	SXUNUSED(apArg);` |
|       2 |  721 | `	if( getloadavg(aLoad,3) == -1 ){` |
|       - |  722 | `		/* php's own sentence and its own severity; the value is FALSE. */` |
|     ! 0 |  723 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Failed to retrieve load average");` |
|     ! 0 |  724 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  725 | `		return PH7_OK;` |
|       - |  726 | `	}` |
|       2 |  727 | `	pArray = ph7_context_new_array(pCtx);` |
|       2 |  728 | `	pVal = ph7_context_new_scalar(pCtx);` |
|       2 |  729 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|     ! 0 |  730 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  731 | `		return PH7_OK;` |
|       - |  732 | `	}` |
|       8 |  733 | `	for( i = 0 ; i < 3 ; ++i ){` |
|       6 |  734 | `		ph7_value_double(pVal,aLoad[i]);` |
|       6 |  735 | `		ph7_array_add_elem(pArray,0,pVal);` |
|       3 |  736 | `	}` |
|       2 |  737 | `	ph7_context_release_value(pCtx,pVal);` |
|       2 |  738 | `	ph7_result_value(pCtx,pArray);` |
|       2 |  739 | `	return PH7_OK;` |
|       1 |  740 | `}` |
|       - |  741 | `#endif /* PH7_HAVE_GETLOADAVG */` |
|       - |  742 | `/*` |
|       - |  743 | ` * int memory_get_usage([bool $real_usage = false])` |
|       - |  744 | ` *  Amount of memory, in bytes, currently allocated to the script through PHL's` |
|       - |  745 | ` *  memory backend. PHL tracks the backend's real allocated bytes, so the` |
|       - |  746 | ` *  $real_usage flag has no effect here (php's non-real figure would be smaller,` |
|       - |  747 | ` *  reflecting Zend's emalloc bookkeeping — recorded divergence).` |
|       - |  748 | ` */` |
|      12 |  749 | `static int PH7_builtin_memory_get_usage(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  750 | `{` |
|       6 |  751 | `	SXUNUSED(nArg);` |
|       6 |  752 | `	SXUNUSED(apArg);` |
|      14 |  753 | `	ph7_result_int64(pCtx,(ph7_int64)pCtx->pVm->sAllocator.nMemUsed);` |
|      14 |  754 | `	return PH7_OK;` |
|       2 |  755 | `}` |
|       - |  756 | `/*` |
|       - |  757 | ` * int memory_get_peak_usage([bool $real_usage = false])` |
|       - |  758 | ` *  High-water mark of memory_get_usage() over the script's lifetime.` |
|       - |  759 | ` */` |
|       4 |  760 | `static int PH7_builtin_memory_get_peak_usage(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  761 | `{` |
|       2 |  762 | `	SXUNUSED(nArg);` |
|       2 |  763 | `	SXUNUSED(apArg);` |
|       5 |  764 | `	ph7_result_int64(pCtx,(ph7_int64)pCtx->pVm->sAllocator.nMemPeak);` |
|       5 |  765 | `	return PH7_OK;` |
|       1 |  766 | `}` |
|       - |  767 | `/*` |
|       - |  768 | ` * void memory_reset_peak_usage()` |
|       - |  769 | ` *  Reset the peak memory usage (memory_get_peak_usage) back to the current` |
|       - |  770 | ` *  live usage — php 8.2. Frameworks call it between tests to measure per-test` |
|       - |  771 | ` *  peaks.` |
|       - |  772 | ` */` |
|       4 |  773 | `static int PH7_builtin_memory_reset_peak_usage(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  774 | `{` |
|       2 |  775 | `	SXUNUSED(nArg);` |
|       2 |  776 | `	SXUNUSED(apArg);` |
|       5 |  777 | `	pCtx->pVm->sAllocator.nMemPeak = pCtx->pVm->sAllocator.nMemUsed;` |
|       5 |  778 | `	return PH7_OK;` |
|       1 |  779 | `}` |
|       - |  780 | `/*` |
|       - |  781 | ` * The gc_* family, over the real collector in vm_gc.c.` |
|       - |  782 | ` *` |
|       - |  783 | ` * PHL frees a value when the last reference to it goes, which is exact for` |
|       - |  784 | ` * everything except a CYCLE; vm_gc.c is php's trial-deletion pass over the` |
|       - |  785 | ` * containers whose refcount dropped without reaching zero. gc_enable()/` |
|       - |  786 | ` * gc_disable() turn root buffering on and off, gc_collect_cycles() runs a` |
|       - |  787 | ` * collection on demand and answers what it freed, and gc_status() reports the` |
|       - |  788 | ` * counters the collector actually keeps.` |
|       - |  789 | ` */` |
|       4 |  790 | `static int PH7_builtin_gc_enable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  791 | `{` |
|       2 |  792 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|       5 |  793 | `	pCtx->pVm->bGcEnabled = 1;` |
|       5 |  794 | `	return PH7_OK;` |
|       1 |  795 | `}` |
|       2 |  796 | `static int PH7_builtin_gc_disable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  797 | `{` |
|       1 |  798 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|       3 |  799 | `	pCtx->pVm->bGcEnabled = 0;` |
|       3 |  800 | `	return PH7_OK;` |
|       1 |  801 | `}` |
|       6 |  802 | `static int PH7_builtin_gc_enabled(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  803 | `{` |
|       3 |  804 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|       7 |  805 | `	ph7_result_bool(pCtx,pCtx->pVm->bGcEnabled);` |
|       7 |  806 | `	return PH7_OK;` |
|       1 |  807 | `}` |
|       6 |  808 | `static int PH7_builtin_gc_collect_cycles(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  809 | `{` |
|       - |  810 | `	/* php: the number of collected CYCLES. Run one now, whatever the buffer holds --` |
|       - |  811 | `	 * an explicit call is a demand, not a hint. */` |
|       3 |  812 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|       9 |  813 | `	ph7_result_int(pCtx,(sxi64)PH7_GcCollect(pCtx->pVm));` |
|       9 |  814 | `	return PH7_OK;` |
|       3 |  815 | `}` |
|     ! 0 |  816 | `static int PH7_builtin_gc_mem_caches(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 |  817 | `{` |
|     ! 0 |  818 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     ! 0 |  819 | `	ph7_result_int(pCtx,0);` |
|     ! 0 |  820 | `	return PH7_OK;` |
|     ! 0 |  821 | `}` |
|       - |  822 | `/*` |
|       - |  823 | ` * array gc_status(void)` |
|       - |  824 | ` *  php 8.3 shape. The counters are the collector's own; the four timing fields are` |
|       - |  825 | ` *  0.0 (nothing here measures them) and 'protected'/'full' are php's own internal` |
|       - |  826 | ` *  re-entrancy states, which this collector expresses as one flag it never exposes` |
|       - |  827 | ` *  mid-collection.` |
|       - |  828 | ` */` |
|       2 |  829 | `static int PH7_builtin_gc_status(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  830 | `{` |
|       - |  831 | `	ph7_value *pArray,*pVal;` |
|       1 |  832 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|       3 |  833 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 |  834 | `	pVal = ph7_context_new_scalar(pCtx);` |
|       3 |  835 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|     ! 0 |  836 | `		ph7_result_null(pCtx);` |
|     ! 0 |  837 | `		return PH7_OK;` |
|       - |  838 | `	}` |
|       - |  839 | `	/* Key order matches php 8.3's gc_status(). */` |
|       - |  840 | `	/* php's "running" is its gc_active -- whether a collection is IN PROGRESS,` |
|       - |  841 | `	 * not whether the collector is switched on. Outside one it is always false,` |
|       - |  842 | `	 * and the only PHP that can see it true is a __destruct() the collector is` |
|       - |  843 | `	 * itself running. Answering bGcEnabled here made every call say true, which` |
|       - |  844 | `	 * is the one thing php's never says. gc_enabled() is the enabled question. */` |
|       3 |  845 | `	ph7_value_bool(pVal,pCtx->pVm->bGcRunning); ph7_array_add_strkey_elem(pArray,"running",pVal);` |
|       3 |  846 | `	ph7_value_bool(pVal,0);      ph7_array_add_strkey_elem(pArray,"protected",pVal);` |
|       3 |  847 | `	ph7_value_bool(pVal,0);      ph7_array_add_strkey_elem(pArray,"full",pVal);` |
|       3 |  848 | `	ph7_value_int(pVal,(sxi64)pCtx->pVm->nGcRuns);      ph7_array_add_strkey_elem(pArray,"runs",pVal);` |
|       3 |  849 | `	ph7_value_int(pVal,(sxi64)pCtx->pVm->nGcCollected); ph7_array_add_strkey_elem(pArray,"collected",pVal);` |
|       3 |  850 | `	ph7_value_int(pVal,(sxi64)pCtx->pVm->nGcThreshold); ph7_array_add_strkey_elem(pArray,"threshold",pVal);` |
|       - |  851 | `	/* php's two are the buffer's CAPACITY and how much of it is used. Answering` |
|       - |  852 | `	 * the used count twice made a full buffer indistinguishable from an empty` |
|       - |  853 | `	 * one -- the pair only means anything as a ratio. */` |
|       3 |  854 | `	ph7_value_int(pVal,(sxi64)SySetSize(&pCtx->pVm->aGcRoot)); ph7_array_add_strkey_elem(pArray,"buffer_size",pVal);` |
|       3 |  855 | `	ph7_value_int(pVal,(sxi64)SySetUsed(&pCtx->pVm->aGcRoot)); ph7_array_add_strkey_elem(pArray,"roots",pVal);` |
|       3 |  856 | `	ph7_value_double(pVal,0.0);  ph7_array_add_strkey_elem(pArray,"application_time",pVal);` |
|       3 |  857 | `	ph7_value_double(pVal,0.0);  ph7_array_add_strkey_elem(pArray,"collector_time",pVal);` |
|       3 |  858 | `	ph7_value_double(pVal,0.0);  ph7_array_add_strkey_elem(pArray,"destructor_time",pVal);` |
|       3 |  859 | `	ph7_value_double(pVal,0.0);  ph7_array_add_strkey_elem(pArray,"free_time",pVal);` |
|       3 |  860 | `	ph7_context_release_value(pCtx,pVal);` |
|       3 |  861 | `	ph7_result_value(pCtx,pArray);` |
|       3 |  862 | `	return PH7_OK;` |
|       2 |  863 | `}` |
|       - |  864 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|       - |  865 | `	{ "setlocale"            , PH7_builtin_setlocale             },` |
|       - |  866 | `#ifdef PH7_HAVE_GETLOADAVG` |
|       - |  867 | `	{ "sys_getloadavg"       , PH7_builtin_sys_getloadavg        },` |
|       - |  868 | `#endif` |
|       - |  869 | `	{ "memory_get_usage"     , PH7_builtin_memory_get_usage      },` |
|       - |  870 | `	{ "memory_get_peak_usage", PH7_builtin_memory_get_peak_usage },` |
|       - |  871 | `	{ "memory_reset_peak_usage", PH7_builtin_memory_reset_peak_usage },` |
|       - |  872 | `	{ "gc_enable"            , PH7_builtin_gc_enable             },` |
|       - |  873 | `	{ "gc_disable"           , PH7_builtin_gc_disable            },` |
|       - |  874 | `	{ "gc_enabled"           , PH7_builtin_gc_enabled            },` |
|       - |  875 | `	{ "gc_collect_cycles"    , PH7_builtin_gc_collect_cycles     },` |
|       - |  876 | `	{ "gc_mem_caches"        , PH7_builtin_gc_mem_caches         },` |
|       - |  877 | `	{ "gc_status"            , PH7_builtin_gc_status             },` |
|       - |  878 | `	   /* Variable handling functions */` |
|       - |  879 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|       - |  880 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|       - |  881 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|       - |  882 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|       - |  883 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|       - |  884 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|       - |  885 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|       - |  886 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|       - |  887 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|       - |  888 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|       - |  889 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|       - |  890 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|       - |  891 | `	{ "is_resource", PH7_builtin_is_resource },` |
|       - |  892 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|       - |  893 | `	{ "doubleval"  , PH7_builtin_floatval    },` |
|       - |  894 | `	{ "intval"     , PH7_builtin_intval      },` |
|       - |  895 | `	{ "strval"     , PH7_builtin_strval      },` |
|       - |  896 | `	{ "boolval"    , PH7_builtin_boolval     },` |
|       - |  897 | `	{ "empty"      , PH7_builtin_empty       },` |
|       - |  898 | `#ifdef PH7_NEED_BUILTIN_REG` |
|       - |  899 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|       - |  900 | `	   /* Math functions */` |
|       - |  901 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|       - |  902 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|       - |  903 | `	{ "acosh" ,   PH7_builtin_acosh        },` |
|       - |  904 | `	{ "asinh" ,   PH7_builtin_asinh        },` |
|       - |  905 | `	{ "atanh" ,   PH7_builtin_atanh        },` |
|       - |  906 | `	{ "expm1" ,   PH7_builtin_expm1        },` |
|       - |  907 | `	{ "log1p" ,   PH7_builtin_log1p        },` |
|       - |  908 | `	{ "deg2rad" , PH7_builtin_deg2rad      },` |
|       - |  909 | `	{ "rad2deg" , PH7_builtin_rad2deg      },` |
|       - |  910 | `	{ "fpow" ,    PH7_builtin_fpow         },` |
|       - |  911 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|       - |  912 | `	{ "floor",    PH7_builtin_floor        },` |
|       - |  913 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|       - |  914 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|       - |  915 | `	{ "acos" ,    PH7_builtin_acos         },` |
|       - |  916 | `	{ "asin" ,    PH7_builtin_asin         },` |
|       - |  917 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|       - |  918 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|       - |  919 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|       - |  920 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|       - |  921 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|       - |  922 | `	{ "atan" ,    PH7_builtin_atan         },` |
|       - |  923 | `	{ "atan2",    PH7_builtin_atan2        },` |
|       - |  924 | `	{ "log"  ,    PH7_builtin_log          },` |
|       - |  925 | `	{ "log10" ,   PH7_builtin_log10        },` |
|       - |  926 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|       - |  927 | `	{ "pi",       PH7_builtin_pi           },` |
|       - |  928 | `	{ "fmod",     PH7_builtin_fmod         },` |
|       - |  929 | `	{ "hypot",    PH7_builtin_hypot        },` |
|       - |  930 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|       - |  931 | `	{ "round",    PH7_builtin_round        },` |
|       - |  932 | `	{ "intdiv",   PH7_builtin_intdiv       },` |
|       - |  933 | `	{ "number_format", PH7_builtin_number_format },` |
|       - |  934 | `	{ "dechex", PH7_builtin_dechex         },` |
|       - |  935 | `	{ "decoct", PH7_builtin_decoct         },` |
|       - |  936 | `	{ "decbin", PH7_builtin_decbin         },` |
|       - |  937 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|       - |  938 | `	{ "bindec", PH7_builtin_bindec         },` |
|       - |  939 | `	{ "octdec", PH7_builtin_octdec         },` |
|       - |  940 | `	{ "srand",  PH7_builtin_srand          },` |
|       - |  941 | `	{ "mt_srand",PH7_builtin_srand         },` |
|       - |  942 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|       - |  943 | `#ifdef PH7_NEED_FMT_AND_INI` |
|       - |  944 | `	{ "base_convert", PH7_builtin_base_convert },` |
|       - |  945 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|       - |  946 | `#ifdef PH7_NEED_BUILTIN_REG` |
|       - |  947 | `	   /* String handling functions */` |
|       - |  948 |  |
|       - |  949 | `	{ "substr",          PH7_builtin_substr     },` |
|       - |  950 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|       - |  951 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|       - |  952 | `	{ "substr_replace",  PH7_builtin_substr_replace },` |
|       - |  953 | `	{ "levenshtein",     PH7_builtin_levenshtein },` |
|       - |  954 | `	{ "similar_text",    PH7_builtin_similar_text },` |
|       - |  955 | `	{ "str_word_count",  PH7_builtin_str_word_count },` |
|       - |  956 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|       - |  957 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|       - |  958 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|       - |  959 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|       - |  960 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|       - |  961 | `	{ "stripcslashes",   PH7_builtin_stripcslashes },` |
|       - |  962 | `	{ "quoted_printable_encode", PH7_builtin_quoted_printable_encode },` |
|       - |  963 | `	{ "quoted_printable_decode", PH7_builtin_quoted_printable_decode },` |
|       - |  964 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|       - |  965 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|       - |  966 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|       - |  967 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|       - |  968 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|       - |  969 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|       - |  970 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|       - |  971 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|       - |  972 | `	{ "strnatcmp"  , PH7_builtin_strnatcmp  },` |
|       - |  973 | `	{ "strnatcasecmp", PH7_builtin_strnatcmp },` |
|       - |  974 | `	{ "version_compare", PH7_builtin_version_compare },` |
|       - |  975 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|       - |  976 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|       - |  977 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|       - |  978 | `	{ "implode"    , PH7_builtin_implode    },` |
|       - |  979 | `	{ "join"       , PH7_builtin_implode    },` |
|       - |  980 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|       - |  981 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|       - |  982 | `	{ "explode"     , PH7_builtin_explode    },` |
|       - |  983 | `	{ "trim"        , PH7_builtin_trim       },` |
|       - |  984 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|       - |  985 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|       - |  986 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|       - |  987 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|       - |  988 | `	{ "mb_strtolower",PH7_builtin_mb_case_f }, /* UTF-8 only (builtin_mb.c) */` |
|       - |  989 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|       - |  990 | `	{ "mb_strtoupper",PH7_builtin_mb_case_f }, /* UTF-8 only (builtin_mb.c) */` |
|       - |  991 | `	{ "mb_strlen",    PH7_builtin_mb_strlen_f },` |
|       - |  992 | `	{ "mb_substr",    PH7_builtin_mb_substr_f },` |
|       - |  993 | `	{ "mb_convert_case", PH7_builtin_mb_convert_case_f },` |
|       - |  994 | `	{ "mb_strpos",    PH7_builtin_mb_strpos_f },` |
|       - |  995 | `	{ "mb_stripos",   PH7_builtin_mb_strpos_f },` |
|       - |  996 | `	{ "mb_strrpos",   PH7_builtin_mb_strpos_f },` |
|       - |  997 | `	{ "mb_strripos",  PH7_builtin_mb_strpos_f },` |
|       - |  998 | `	{ "mb_strstr",    PH7_builtin_mb_strstr_f },` |
|       - |  999 | `	{ "mb_stristr",   PH7_builtin_mb_strstr_f },` |
|       - | 1000 | `	{ "mb_strrchr",   PH7_builtin_mb_strstr_f },` |
|       - | 1001 | `	{ "mb_strrichr",  PH7_builtin_mb_strstr_f },` |
|       - | 1002 | `	{ "mb_substr_count", PH7_builtin_mb_substr_count_f },` |
|       - | 1003 | `	{ "mb_str_pad",   PH7_builtin_mb_str_pad_f },` |
|       - | 1004 | `	{ "mb_strcut",    PH7_builtin_mb_strcut_f },` |
|       - | 1005 | `	{ "mb_strimwidth", PH7_builtin_mb_strimwidth_f },` |
|       - | 1006 | `	{ "mb_str_split", PH7_builtin_mb_str_split_f },` |
|       - | 1007 | `	{ "mb_trim",      PH7_builtin_mb_trim_f  },` |
|       - | 1008 | `	{ "mb_ltrim",     PH7_builtin_mb_trim_f  },` |
|       - | 1009 | `	{ "mb_rtrim",     PH7_builtin_mb_trim_f  },` |
|       - | 1010 | `	{ "mb_internal_encoding", PH7_builtin_mb_internal_encoding_f },` |
|       - | 1011 | `	{ "mb_substitute_character", PH7_builtin_mb_substitute_character_f },` |
|       - | 1012 | `	{ "mb_scrub",     PH7_builtin_mb_scrub_f },` |
|       - | 1013 | `	{ "mb_check_encoding",    PH7_builtin_mb_check_encoding_f },` |
|       - | 1014 | `	{ "mb_strwidth",  PH7_builtin_mb_strwidth_f },` |
|       - | 1015 | `	{ "mb_chr",       PH7_builtin_mb_chr_f   },` |
|       - | 1016 | `	{ "mb_ord",       PH7_builtin_mb_ord_f   },` |
|       - | 1017 | `	{ "mb_ucfirst",   PH7_builtin_mb_ucfirst_f },` |
|       - | 1018 | `	{ "mb_lcfirst",   PH7_builtin_mb_ucfirst_f },` |
|       - | 1019 | `	{ "mb_detect_encoding", PH7_builtin_mb_detect_encoding_f },` |
|       - | 1020 | `	{ "mb_detect_order", PH7_builtin_mb_detect_order_f },` |
|       - | 1021 | `	{ "mb_list_encodings", PH7_builtin_mb_list_encodings_f },` |
|       - | 1022 | `	{ "mb_encoding_aliases", PH7_builtin_mb_encoding_aliases_f },` |
|       - | 1023 | `	{ "mb_convert_encoding", PH7_builtin_mb_convert_encoding_f },` |
|       - | 1024 | `	{ "iconv",        PH7_builtin_iconv_f    }, /* builtin_iconv.c */` |
|       - | 1025 | `	{ "iconv_strlen", PH7_builtin_iconv_strlen_f },` |
|       - | 1026 | `	{ "iconv_substr", PH7_builtin_iconv_substr_f },` |
|       - | 1027 | `	{ "iconv_strpos", PH7_builtin_iconv_strpos_f },` |
|       - | 1028 | `	{ "iconv_strrpos",PH7_builtin_iconv_strrpos_f },` |
|       - | 1029 | `	{ "iconv_get_encoding", PH7_builtin_iconv_get_encoding_f },` |
|       - | 1030 | `	{ "iconv_mime_encode", PH7_builtin_iconv_mime_encode_f },` |
|       - | 1031 | `	{ "iconv_mime_decode", PH7_builtin_iconv_mime_decode_f },` |
|       - | 1032 | `	{ "iconv_mime_decode_headers", PH7_builtin_iconv_mime_decode_headers_f },` |
|       - | 1033 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|       - | 1034 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|       - | 1035 | `	{ "ord",          PH7_builtin_ord        },` |
|       - | 1036 | `	{ "chr",          PH7_builtin_chr        },` |
|       - | 1037 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|       - | 1038 | `	{ "strstr",       PH7_builtin_strstr     },` |
|       - | 1039 | `	{ "stristr",      PH7_builtin_stristr    },` |
|       - | 1040 | `	{ "strchr",       PH7_builtin_strstr     },` |
|       - | 1041 | `	{ "strpos",       PH7_builtin_strpos     },` |
|       - | 1042 | `	{ "stripos",      PH7_builtin_stripos    },` |
|       - | 1043 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|       - | 1044 | `	{ "strripos",     PH7_builtin_strripos   },` |
|       - | 1045 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|       - | 1046 | `	{ "strrev",       PH7_builtin_strrev     },` |
|       - | 1047 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|       - | 1048 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|       - | 1049 | `	{ "str_contains", PH7_builtin_str_contains },` |
|       - | 1050 | `	{ "str_starts_with", PH7_builtin_str_starts_with },` |
|       - | 1051 | `	{ "str_ends_with", PH7_builtin_str_ends_with },` |
|       - | 1052 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|       - | 1053 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|       - | 1054 | `#ifdef PH7_NEED_FMT_AND_INI` |
|       - | 1055 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|       - | 1056 | `	{ "printf",       PH7_builtin_printf     },` |
|       - | 1057 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|       - | 1058 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|       - | 1059 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|       - | 1060 | `#ifdef PH7_NEED_BUILTIN_REG` |
|       - | 1061 | `	{ "size_format",  PH7_builtin_size_format},` |
|       - | 1062 | `	     /* ext/standard's syslog trio, in php's own order. Not an extension --` |
|       - | 1063 | `	      * these are here on every platform php has them on, Windows included. */` |
|       - | 1064 | `	{ "openlog",      PH7_builtin_openlog   },` |
|       - | 1065 | `	{ "closelog",     PH7_builtin_closelog  },` |
|       - | 1066 | `	{ "syslog",       PH7_builtin_syslog    },` |
|       - | 1067 |  |
|       - | 1068 |  |
|       - | 1069 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|       - | 1070 | `	{ "md5",          PH7_builtin_md5       },` |
|       - | 1071 | `	{ "sha1",         PH7_builtin_sha1      },` |
|       - | 1072 | `	{ "crc32",        PH7_builtin_crc32     },` |
|       - | 1073 | `	{ "hash",         PH7_builtin_hash      },` |
|       - | 1074 | `	{ "hash_hmac",    PH7_builtin_hash_hmac },` |
|       - | 1075 | `	{ "hash_equals",  PH7_builtin_hash_equals },` |
|       - | 1076 | `	{ "hash_algos",   PH7_builtin_hash_algos },` |
|       - | 1077 | `	{ "hash_hmac_algos", PH7_builtin_hash_hmac_algos },` |
|       - | 1078 | `	{ "hash_init",    PH7_builtin_hash_init },` |
|       - | 1079 | `	{ "hash_update",  PH7_builtin_hash_update },` |
|       - | 1080 | `	{ "hash_final",   PH7_builtin_hash_final },` |
|       - | 1081 | `	{ "hash_copy",    PH7_builtin_hash_copy },` |
|       - | 1082 | `	{ "hash_pbkdf2",  PH7_builtin_hash_pbkdf2 },` |
|       - | 1083 | `	{ "hash_hkdf",    PH7_builtin_hash_hkdf },` |
|       - | 1084 | `	{ "crypt",        PH7_builtin_crypt     },` |
|       - | 1085 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|       - | 1086 | `	{ "password_hash",         PH7_builtin_password_hash },` |
|       - | 1087 | `	{ "password_verify",       PH7_builtin_password_verify },` |
|       - | 1088 | `	{ "password_get_info",     PH7_builtin_password_get_info },` |
|       - | 1089 | `	{ "password_needs_rehash", PH7_builtin_password_needs_rehash },` |
|       - | 1090 | `	{ "password_algos",        PH7_builtin_password_algos },` |
|       - | 1091 | `	{ "filter_var",            PH7_builtin_filter_var },` |
|       - | 1092 | `	{ "filter_input",          PH7_builtin_filter_input },` |
|       - | 1093 | `	{ "filter_list",           PH7_builtin_filter_list },` |
|       - | 1094 | `	{ "filter_id",             PH7_builtin_filter_id },` |
|       - | 1095 | `	{ "filter_has_var",        PH7_builtin_filter_has_var },` |
|       - | 1096 | `	{ "filter_var_array",      PH7_builtin_filter_var_array },` |
|       - | 1097 | `	{ "filter_input_array",    PH7_builtin_filter_input_array },` |
|       - | 1098 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|       - | 1099 | `#ifdef PH7_NEED_FMT_AND_INI` |
|       - | 1100 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|       - | 1101 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|       - | 1102 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|       - | 1103 | `#ifdef PH7_NEED_BUILTIN_REG` |
|       - | 1104 |  |
|       - | 1105 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|       - | 1106 | `	{ "str_split",    PH7_builtin_str_split  },` |
|       - | 1107 | `	{ "count_chars",  PH7_builtin_count_chars},` |
|       - | 1108 | `	{ "strspn",       PH7_builtin_strspn     },` |
|       - | 1109 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|       - | 1110 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|       - | 1111 | `	{ "soundex",      PH7_builtin_soundex    },` |
|       - | 1112 | `	{ "str_rot13",    PH7_builtin_str_rot13  },` |
|       - | 1113 | `	{ "metaphone",    PH7_builtin_metaphone  },` |
|       - | 1114 | `	{ "pack",         PH7_builtin_pack       },` |
|       - | 1115 | `	{ "unpack",       PH7_builtin_unpack     },` |
|       - | 1116 | `	{ "sscanf",       PH7_builtin_sscanf     },` |
|       - | 1117 | `	     /* ext/bcmath: arbitrary-precision decimal arithmetic over strings */` |
|       - | 1118 | `	{ "bcadd",        PH7_builtin_bcadd      },` |
|       - | 1119 | `	{ "bcsub",        PH7_builtin_bcsub      },` |
|       - | 1120 | `	{ "bcmul",        PH7_builtin_bcmul      },` |
|       - | 1121 | `	{ "bccomp",       PH7_builtin_bccomp     },` |
|       - | 1122 | `	{ "bcdiv",        PH7_builtin_bcdiv      },` |
|       - | 1123 | `	{ "bcmod",        PH7_builtin_bcmod      },` |
|       - | 1124 | `	{ "bcdivmod",     PH7_builtin_bcdivmod   },` |
|       - | 1125 | `	{ "bcpow",        PH7_builtin_bcpow      },` |
|       - | 1126 | `	{ "bcpowmod",     PH7_builtin_bcpowmod   },` |
|       - | 1127 | `	{ "bcsqrt",       PH7_builtin_bcsqrt     },` |
|       - | 1128 | `	{ "bcround",      PH7_builtin_bcround    },` |
|       - | 1129 | `	{ "bcfloor",      PH7_builtin_bcfloor    },` |
|       - | 1130 | `	{ "bcceil",       PH7_builtin_bcceil     },` |
|       - | 1131 | `	{ "bcscale",      PH7_builtin_bcscale    },` |
|       - | 1132 | `	     /* ext/calendar: the serial day number and its four calendars */` |
|       - | 1133 | `	{ "gregoriantojd",PH7_builtin_gregoriantojd },` |
|       - | 1134 | `	{ "jdtogregorian",PH7_builtin_jdtogregorian },` |
|       - | 1135 | `	{ "juliantojd",   PH7_builtin_juliantojd    },` |
|       - | 1136 | `	{ "jdtojulian",   PH7_builtin_jdtojulian    },` |
|       - | 1137 | `	{ "frenchtojd",   PH7_builtin_frenchtojd    },` |
|       - | 1138 | `	{ "jdtofrench",   PH7_builtin_jdtofrench    },` |
|       - | 1139 | `	{ "jewishtojd",   PH7_builtin_jewishtojd    },` |
|       - | 1140 | `	{ "jdtojewish",   PH7_builtin_jdtojewish    },` |
|       - | 1141 | `	{ "cal_info",     PH7_builtin_cal_info      },` |
|       - | 1142 | `	{ "cal_days_in_month", PH7_builtin_cal_days_in_month },` |
|       - | 1143 | `	{ "cal_to_jd",    PH7_builtin_cal_to_jd     },` |
|       - | 1144 | `	{ "cal_from_jd",  PH7_builtin_cal_from_jd   },` |
|       - | 1145 | `	{ "jddayofweek",  PH7_builtin_jddayofweek   },` |
|       - | 1146 | `	{ "jdmonthname",  PH7_builtin_jdmonthname   },` |
|       - | 1147 | `	{ "unixtojd",     PH7_builtin_unixtojd      },` |
|       - | 1148 | `	{ "jdtounix",     PH7_builtin_jdtounix      },` |
|       - | 1149 | `	{ "easter_days",  PH7_builtin_easter_days   },` |
|       - | 1150 | `	{ "easter_date",  PH7_builtin_easter_date   },` |
|       - | 1151 | `	     /* ext/gettext: php's own order for the extension */` |
|       - | 1152 | `	{ "textdomain",              PH7_builtin_textdomain              },` |
|       - | 1153 | `	{ "gettext",                 PH7_builtin_gettext                 },` |
|       - | 1154 | `	{ "_",                       PH7_builtin_gettext                 },` |
|       - | 1155 | `	{ "dgettext",                PH7_builtin_dgettext                },` |
|       - | 1156 | `	{ "dcgettext",               PH7_builtin_dcgettext               },` |
|       - | 1157 | `	{ "bindtextdomain",          PH7_builtin_bindtextdomain          },` |
|       - | 1158 | `	{ "ngettext",                PH7_builtin_ngettext                },` |
|       - | 1159 | `	{ "dngettext",               PH7_builtin_dngettext               },` |
|       - | 1160 | `	{ "dcngettext",              PH7_builtin_dcngettext              },` |
|       - | 1161 | `	{ "bind_textdomain_codeset", PH7_builtin_bind_textdomain_codeset },` |
|       - | 1162 | `	     /* ext/fileinfo: php's own order for the extension */` |
|       - | 1163 | `	{ "finfo_open",              PH7_builtin_finfo_open              },` |
|       - | 1164 | `	{ "finfo_close",             PH7_builtin_finfo_close             },` |
|       - | 1165 | `	{ "finfo_set_flags",         PH7_builtin_finfo_set_flags         },` |
|       - | 1166 | `	{ "finfo_file",              PH7_builtin_finfo_file              },` |
|       - | 1167 | `	{ "finfo_buffer",            PH7_builtin_finfo_buffer            },` |
|       - | 1168 | `	{ "mime_content_type",       PH7_builtin_mime_content_type       },` |
|       - | 1169 | `#ifndef __WINNT__` |
|       - | 1170 | `	     /* ext/posix, in php's own order. php builds none of this on Windows,` |
|       - | 1171 | ``	      * so `function_exists('posix_kill')` is FALSE there -- which is what a`` |
|       - | 1172 | `	      * program guarding its use of them looks for. */` |
|       - | 1173 | `	{ "posix_kill",              PH7_builtin_posix_kill              },` |
|       - | 1174 | `	{ "posix_getpid",            PH7_builtin_posix_getpid            },` |
|       - | 1175 | `	{ "posix_getppid",           PH7_builtin_posix_getppid           },` |
|       - | 1176 | `	{ "posix_getuid",            PH7_builtin_posix_getuid            },` |
|       - | 1177 | `	{ "posix_setuid",            PH7_builtin_posix_setuid            },` |
|       - | 1178 | `	{ "posix_geteuid",           PH7_builtin_posix_geteuid           },` |
|       - | 1179 | `	{ "posix_seteuid",           PH7_builtin_posix_seteuid           },` |
|       - | 1180 | `	{ "posix_getgid",            PH7_builtin_posix_getgid            },` |
|       - | 1181 | `	{ "posix_setgid",            PH7_builtin_posix_setgid            },` |
|       - | 1182 | `	{ "posix_getegid",           PH7_builtin_posix_getegid           },` |
|       - | 1183 | `	{ "posix_setegid",           PH7_builtin_posix_setegid           },` |
|       - | 1184 | `	{ "posix_getgroups",         PH7_builtin_posix_getgroups         },` |
|       - | 1185 | `	{ "posix_getlogin",          PH7_builtin_posix_getlogin          },` |
|       - | 1186 | `	{ "posix_getpgrp",           PH7_builtin_posix_getpgrp           },` |
|       - | 1187 | `	{ "posix_setsid",            PH7_builtin_posix_setsid            },` |
|       - | 1188 | `	{ "posix_setpgid",           PH7_builtin_posix_setpgid           },` |
|       - | 1189 | `	{ "posix_getpgid",           PH7_builtin_posix_getpgid           },` |
|       - | 1190 | `	{ "posix_getsid",            PH7_builtin_posix_getsid            },` |
|       - | 1191 | `	{ "posix_uname",             PH7_builtin_posix_uname             },` |
|       - | 1192 | `	{ "posix_times",             PH7_builtin_posix_times             },` |
|       - | 1193 | `	{ "posix_ctermid",           PH7_builtin_posix_ctermid           },` |
|       - | 1194 | `	{ "posix_ttyname",           PH7_builtin_posix_ttyname           },` |
|       - | 1195 | `	{ "posix_isatty",            PH7_builtin_posix_isatty            },` |
|       - | 1196 | `	{ "posix_getcwd",            PH7_builtin_posix_getcwd            },` |
|       - | 1197 | `	{ "posix_mkfifo",            PH7_builtin_posix_mkfifo            },` |
|       - | 1198 | `	{ "posix_mknod",             PH7_builtin_posix_mknod             },` |
|       - | 1199 | `	{ "posix_access",            PH7_builtin_posix_access            },` |
|       - | 1200 | `	{ "posix_eaccess",           PH7_builtin_posix_eaccess           },` |
|       - | 1201 | `	{ "posix_getgrnam",          PH7_builtin_posix_getgrnam          },` |
|       - | 1202 | `	{ "posix_getgrgid",          PH7_builtin_posix_getgrgid          },` |
|       - | 1203 | `	{ "posix_getpwnam",          PH7_builtin_posix_getpwnam          },` |
|       - | 1204 | `	{ "posix_getpwuid",          PH7_builtin_posix_getpwuid          },` |
|       - | 1205 | `	{ "posix_getrlimit",         PH7_builtin_posix_getrlimit         },` |
|       - | 1206 | `	{ "posix_setrlimit",         PH7_builtin_posix_setrlimit         },` |
|       - | 1207 | `	{ "posix_get_last_error",    PH7_builtin_posix_get_last_error    },` |
|       - | 1208 | `	{ "posix_errno",             PH7_builtin_posix_get_last_error    },` |
|       - | 1209 | `	{ "posix_strerror",          PH7_builtin_posix_strerror          },` |
|       - | 1210 | `	{ "posix_initgroups",        PH7_builtin_posix_initgroups        },` |
|       - | 1211 | `	{ "posix_sysconf",           PH7_builtin_posix_sysconf           },` |
|       - | 1212 | `	{ "posix_pathconf",          PH7_builtin_posix_pathconf          },` |
|       - | 1213 | `	{ "posix_fpathconf",         PH7_builtin_posix_fpathconf         },` |
|       - | 1214 | `	     /* ext/pcntl, in php's own order. php builds none of this on Windows` |
|       - | 1215 | `	      * either, which is exactly what monolog's SignalHandler and` |
|       - | 1216 | `	      * symfony/console's SignalRegistry check before calling anything. */` |
|       - | 1217 | `	{ "pcntl_fork",               PH7_builtin_pcntl_fork               },` |
|       - | 1218 | `	{ "pcntl_waitpid",            PH7_builtin_pcntl_waitpid            },` |
|       - | 1219 | `	{ "pcntl_waitid",             PH7_builtin_pcntl_waitid             },` |
|       - | 1220 | `	{ "pcntl_wait",               PH7_builtin_pcntl_wait               },` |
|       - | 1221 | `	{ "pcntl_signal",             PH7_builtin_pcntl_signal             },` |
|       - | 1222 | `	{ "pcntl_signal_get_handler", PH7_builtin_pcntl_signal_get_handler },` |
|       - | 1223 | `	{ "pcntl_signal_dispatch",    PH7_builtin_pcntl_signal_dispatch    },` |
|       - | 1224 | `	{ "pcntl_sigprocmask",        PH7_builtin_pcntl_sigprocmask        },` |
|       - | 1225 | `#ifndef __APPLE__` |
|       - | 1226 | `	/* php builds these two only where the system has them; macOS has neither. */` |
|       - | 1227 | `	{ "pcntl_sigwaitinfo",        PH7_builtin_pcntl_sigwaitinfo        },` |
|       - | 1228 | `	{ "pcntl_sigtimedwait",       PH7_builtin_pcntl_sigtimedwait       },` |
|       - | 1229 | `#endif` |
|       - | 1230 | `	{ "pcntl_wifexited",          PH7_builtin_pcntl_wifexited          },` |
|       - | 1231 | `	{ "pcntl_wifstopped",         PH7_builtin_pcntl_wifstopped         },` |
|       - | 1232 | `	{ "pcntl_wifcontinued",       PH7_builtin_pcntl_wifcontinued       },` |
|       - | 1233 | `	{ "pcntl_wifsignaled",        PH7_builtin_pcntl_wifsignaled        },` |
|       - | 1234 | `	{ "pcntl_wexitstatus",        PH7_builtin_pcntl_wexitstatus        },` |
|       - | 1235 | `	{ "pcntl_wtermsig",           PH7_builtin_pcntl_wtermsig           },` |
|       - | 1236 | `	{ "pcntl_wstopsig",           PH7_builtin_pcntl_wstopsig           },` |
|       - | 1237 | `	{ "pcntl_exec",               PH7_builtin_pcntl_exec               },` |
|       - | 1238 | `	{ "pcntl_alarm",              PH7_builtin_pcntl_alarm              },` |
|       - | 1239 | `	{ "pcntl_get_last_error",     PH7_builtin_pcntl_get_last_error     },` |
|       - | 1240 | `	{ "pcntl_errno",              PH7_builtin_pcntl_get_last_error     },` |
|       - | 1241 | `	{ "pcntl_getpriority",        PH7_builtin_pcntl_getpriority        },` |
|       - | 1242 | `	{ "pcntl_setpriority",        PH7_builtin_pcntl_setpriority        },` |
|       - | 1243 | `	{ "pcntl_strerror",           PH7_builtin_pcntl_strerror           },` |
|       - | 1244 | `	{ "pcntl_async_signals",      PH7_builtin_pcntl_async_signals      },` |
|       - | 1245 | `#ifdef __linux__` |
|       - | 1246 | `	/* php builds these four only where the kernel has them, so a php on a` |
|       - | 1247 | `	 * non-Linux unix answers false to function_exists('pcntl_unshare'). */` |
|       - | 1248 | `	{ "pcntl_unshare",            PH7_builtin_pcntl_unshare            },` |
|       - | 1249 | `	{ "pcntl_getcpuaffinity",     PH7_builtin_pcntl_getcpuaffinity     },` |
|       - | 1250 | `	{ "pcntl_setcpuaffinity",     PH7_builtin_pcntl_setcpuaffinity     },` |
|       - | 1251 | `	{ "pcntl_getcpu",             PH7_builtin_pcntl_getcpu             },` |
|       - | 1252 | `#endif /* __linux__ */` |
|       - | 1253 | `#endif /* __WINNT__ */` |
|       - | 1254 | `	     /* ext/standard: the image container surface */` |
|       - | 1255 | `	{ "image_type_to_mime_type", PH7_builtin_image_type_to_mime_type },` |
|       - | 1256 | `	{ "image_type_to_extension", PH7_builtin_image_type_to_extension },` |
|       - | 1257 | `	{ "getimagesize",            PH7_builtin_getimagesize            },` |
|       - | 1258 | `	{ "getimagesizefromstring",  PH7_builtin_getimagesizefromstring  },` |
|       - | 1259 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|       - | 1260 | `	{ "strtok",       PH7_builtin_strtok     },` |
|       - | 1261 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|       - | 1262 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|       - | 1263 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|       - | 1264 | `	{ "strtr",        PH7_builtin_strtr      },` |
|       - | 1265 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|       - | 1266 | `#ifdef PH7_NEED_FMT_AND_INI` |
|       - | 1267 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|       - | 1268 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|       - | 1269 | `#ifdef PH7_NEED_BUILTIN_REG` |
|       - | 1270 |  |
|       - | 1271 | `	         /* Ctype functions */` |
|       - | 1272 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|       - | 1273 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|       - | 1274 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|       - | 1275 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|       - | 1276 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|       - | 1277 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|       - | 1278 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|       - | 1279 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|       - | 1280 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|       - | 1281 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|       - | 1282 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|       - | 1283 | `	         /* Time functions */` |
|       - | 1284 | `	{ "time"    ,    PH7_builtin_time         },` |
|       - | 1285 | `	{ "microtime",   PH7_builtin_microtime    },` |
|       - | 1286 | `	{ "hrtime",      PH7_builtin_hrtime       },` |
|       - | 1287 | `	{ "getrusage",   PH7_builtin_getrusage    },` |
|       - | 1288 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|       - | 1289 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|       - | 1290 | `	{ "date",        PH7_builtin_date         },` |
|       - | 1291 | `	{ "idate",       PH7_builtin_idate        },` |
|       - | 1292 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|       - | 1293 | `	{ "localtime",   PH7_builtin_localtime    },` |
|       - | 1294 | `	{ "mktime",      PH7_builtin_mktime       },` |
|       - | 1295 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|       - | 1296 | `	{ "date_default_timezone_get", PH7_builtin_date_default_timezone_get },` |
|       - | 1297 | `	{ "date_default_timezone_set", PH7_builtin_date_default_timezone_set },` |
|       - | 1298 | `	{ "date_sun_info", PH7_builtin_date_sun_info },` |
|       - | 1299 | `	{ "date_sunrise",  PH7_builtin_date_sunrise  },` |
|       - | 1300 | `	{ "date_sunset",   PH7_builtin_date_sunset   },` |
|       - | 1301 | `	        /* URL functions */` |
|       - | 1302 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|       - | 1303 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|       - | 1304 | `	{ "convert_uuencode",PH7_builtin_convert_uuencode },` |
|       - | 1305 | `	{ "convert_uudecode",PH7_builtin_convert_uudecode },` |
|       - | 1306 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|       - | 1307 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|       - | 1308 | `	{ "rawurlencode", PH7_builtin_rawurlencode },` |
|       - | 1309 | `	{ "http_build_query", PH7_builtin_http_build_query },` |
|       - | 1310 | `	{ "parse_str",    PH7_builtin_parse_str  },` |
|       - | 1311 | `	{ "rawurldecode", PH7_builtin_rawurldecode },` |
|       - | 1312 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|       - | 1313 | `};` |
|       - | 1314 | `/*` |
|       - | 1315 | ` * Register the built-in functions defined above,the array functions` |
|       - | 1316 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|       - | 1317 | ` */` |
|    8445 | 1318 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|       5 | 1319 | `{` |
|       - | 1320 | `	sxu32 n;` |
| 3285143 | 1321 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 3276698 | 1322 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 1623550 | 1323 | `	}` |
|       - | 1324 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|    8450 | 1325 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|       - | 1326 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|    8450 | 1327 | `	PH7_RegisterIORoutine(&(*pVm));` |
|    8450 | 1328 | `}` |
|       - | 1329 |  |
|       - | 1330 | `/*` |
|       - | 1331 | ` * UTF-8 codepoint reader shared by the glob/fnmatch matcher in vfs.c.` |
|       - | 1332 | ` * Relocated here from the removed vm_xml.c when the legacy xml_* API was` |
|       - | 1333 | ` * dropped; the utf8_encode()/utf8_decode() builtins it once served were` |
|       - | 1334 | ` * removed in turn (superseded by mb_convert_encoding()),` |
|       - | 1335 | ` * leaving only this public-domain SQLite reader.` |
|       - | 1336 | ` */` |
|       - | 1337 | `/* SPDX-SnippetBegin */` |
|       - | 1338 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|       - | 1339 | `/* SPDX-License-Identifier: blessing */` |
|       - | 1340 | `/*` |
|       - | 1341 | ` * UTF-8 decoding routine extracted from the sqlite3 source tree.` |
|       - | 1342 | ` * Original author: D. Richard Hipp (http://www.sqlite.org)` |
|       - | 1343 | ` * Status: Public Domain` |
|       - | 1344 | ` */` |
|       - | 1345 | `/*` |
|       - | 1346 | `** This lookup table is used to help decode the first byte of` |
|       - | 1347 | `** a multi-byte UTF8 character.` |
|       - | 1348 | `*/` |
|       - | 1349 | `static const unsigned char UtfTrans1[] = {` |
|       - | 1350 | `  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,` |
|       - | 1351 | `  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,` |
|       - | 1352 | `  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,` |
|       - | 1353 | `  0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,` |
|       - | 1354 | `  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,` |
|       - | 1355 | `  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,` |
|       - | 1356 | `  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,` |
|       - | 1357 | `  0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x00, 0x00,` |
|       - | 1358 | `};` |
|       - | 1359 | `/*` |
|       - | 1360 | `** Translate a single UTF-8 character.  Return the unicode value.` |
|       - | 1361 | `**` |
|       - | 1362 | `** During translation, assume that the byte that zTerm points` |
|       - | 1363 | `** is a 0x00.` |
|       - | 1364 | `**` |
|       - | 1365 | `** Write a pointer to the next unread byte back into *pzNext.` |
|       - | 1366 | `**` |
|       - | 1367 | `** Notes On Invalid UTF-8:` |
|       - | 1368 | `**` |
|       - | 1369 | `**  *  This routine never allows a 7-bit character (0x00 through 0x7f) to` |
|       - | 1370 | `**     be encoded as a multi-byte character.  Any multi-byte character that` |
|       - | 1371 | `**     attempts to encode a value between 0x00 and 0x7f is rendered as 0xfffd.` |
|       - | 1372 | `**` |
|       - | 1373 | `**  *  This routine never allows a UTF16 surrogate value to be encoded.` |
|       - | 1374 | `**     If a multi-byte character attempts to encode a value between` |
|       - | 1375 | `**     0xd800 and 0xe000 then it is rendered as 0xfffd.` |
|       - | 1376 | `**` |
|       - | 1377 | `**  *  Bytes in the range of 0x80 through 0xbf which occur as the first` |
|       - | 1378 | `**     byte of a character are interpreted as single-byte characters` |
|       - | 1379 | `**     and rendered as themselves even though they are technically` |
|       - | 1380 | `**     invalid characters.` |
|       - | 1381 | `**` |
|       - | 1382 | `**  *  This routine accepts an infinite number of different UTF8 encodings` |
|       - | 1383 | `**     for unicode values 0x80 and greater.  It do not change over-length` |
|       - | 1384 | `**     encodings to 0xfffd as some systems recommend.` |
|       - | 1385 | `*/` |
|       - | 1386 | `#define READ_UTF8(zIn, zTerm, c)                           \` |
|       - | 1387 | `  c = *(zIn++);                                            \` |
|       - | 1388 | `  if( c>=0xc0 ){                                           \` |
|       - | 1389 | `    c = UtfTrans1[c-0xc0];                                 \` |
|       - | 1390 | `    while( zIn!=zTerm && (*zIn & 0xc0)==0x80 ){            \` |
|       - | 1391 | `      c = (c<<6) + (0x3f & *(zIn++));                      \` |
|       - | 1392 | `    }                                                      \` |
|       - | 1393 | `    if( c<0x80                                             \` |
|       - | 1394 | `        \|\| (c&0xFFFFF800)==0xD800                          \` |
|       - | 1395 | `        \|\| (c&0xFFFFFFFE)==0xFFFE ){  c = 0xFFFD; }        \` |
|       - | 1396 | `  }` |
|   10342 | 1397 | `PH7_PRIVATE int PH7_Utf8Read(` |
|       - | 1398 | `  const unsigned char *z,         /* First byte of UTF-8 character */` |
|       - | 1399 | `  const unsigned char *zTerm,     /* Pretend this byte is 0x00 */` |
|       - | 1400 | `  const unsigned char **pzNext    /* Write first byte past UTF-8 char here */` |
|       5 | 1401 | `){` |
|       - | 1402 | `  int c;` |
|   10347 | 1403 | `  READ_UTF8(z, zTerm, c);` |
|   10347 | 1404 | `  *pzNext = z;` |
|   10347 | 1405 | `  return c;` |
|       5 | 1406 | `}` |
|       - | 1407 | `/* SPDX-SnippetEnd */` |
|       - | 1408 | `/*` |
|       - | 1409 | ` * Read one STRICTLY well-formed UTF-8 sequence from z[0..n-1].` |
|       - | 1410 | ` *` |
|       - | 1411 | ` * Unlike PH7_Utf8Read above (the lenient SQLite reader, which renders anything` |
|       - | 1412 | ` * dubious as U+FFFD and happily accepts over-long forms), this one implements` |
|       - | 1413 | ` * the RFC 3629 / Unicode "Table 3-7 well-formed byte sequences" rule php uses` |
|       - | 1414 | ` * wherever it has to decide whether a php string really is UTF-8:` |
|       - | 1415 | ` *` |
|       - | 1416 | ` *   00..7F                          one byte` |
|       - | 1417 | ` *   C2..DF  80..BF                  (C0/C1 are over-long two-byte forms)` |
|       - | 1418 | ` *   E0      A0..BF  80..BF          (E0 80..9F is over-long)` |
|       - | 1419 | ` *   E1..EC  80..BF  80..BF` |
|       - | 1420 | ` *   ED      80..9F  80..BF          (ED A0..BF is a UTF-16 surrogate)` |
|       - | 1421 | ` *   EE..EF  80..BF  80..BF` |
|       - | 1422 | ` *   F0      90..BF  80..BF  80..BF  (F0 80..8F is over-long)` |
|       - | 1423 | ` *   F1..F3  80..BF  80..BF  80..BF` |
|       - | 1424 | ` *   F4      80..8F  80..BF  80..BF  (past U+10FFFF)` |
|       - | 1425 | ` *` |
|       - | 1426 | ` * Returns the code point and writes the sequence length to *pLen. On an` |
|       - | 1427 | ` * ill-formed sequence it returns -1 and writes 1, so a caller can apply its own` |
|       - | 1428 | ` * php policy to the single offending byte (json_encode: JSON_ERROR_UTF8 or the` |
|       - | 1429 | ` * JSON_INVALID_UTF8_* substitution; mb_strtolower: '?') and resume at the next` |
|       - | 1430 | ` * byte exactly like php does. n must be >= 1.` |
|       - | 1431 | ` */` |
|   96779 | 1432 | `PH7_PRIVATE sxi32 PH7_Utf8ReadStrict(const unsigned char *z,sxu32 n,sxu32 *pLen)` |
|       4 | 1433 | `{` |
|   96783 | 1434 | `	sxu32 c = z[0];` |
|   96783 | 1435 | `	*pLen = 1;` |
|   96783 | 1436 | `	if( c < 0x80 ){` |
|   92600 | 1437 | `		return (sxi32)c;` |
|       - | 1438 | `	}` |
|    4185 | 1439 | `	if( c >= 0xC2 && c <= 0xDF ){` |
|    1933 | 1440 | `		if( n < 2 \|\| (z[1] & 0xC0) != 0x80 ){` |
|     123 | 1441 | `			return -1;` |
|       - | 1442 | `		}` |
|    1811 | 1443 | `		*pLen = 2;` |
|    1811 | 1444 | `		return (sxi32)(((c & 0x1F) << 6) \| (z[1] & 0x3F));` |
|       - | 1445 | `	}` |
|    2254 | 1446 | `	if( c >= 0xE0 && c <= 0xEF ){` |
|    1644 | 1447 | `		sxu32 iLow = (c == 0xE0) ? 0xA0 : 0x80;` |
|    1644 | 1448 | `		sxu32 iHigh = (c == 0xED) ? 0x9F : 0xBF;` |
|    1644 | 1449 | `		if( n < 3 \|\| z[1] < iLow \|\| z[1] > iHigh \|\| (z[2] & 0xC0) != 0x80 ){` |
|     103 | 1450 | `			return -1;` |
|       - | 1451 | `		}` |
|    1542 | 1452 | `		*pLen = 3;` |
|    1542 | 1453 | `		return (sxi32)(((c & 0x0F) << 12) \| ((z[1] & 0x3F) << 6) \| (z[2] & 0x3F));` |
|       - | 1454 | `	}` |
|     612 | 1455 | `	if( c >= 0xF0 && c <= 0xF4 ){` |
|     112 | 1456 | `		sxu32 iLow = (c == 0xF0) ? 0x90 : 0x80;` |
|     112 | 1457 | `		sxu32 iHigh = (c == 0xF4) ? 0x8F : 0xBF;` |
|     110 | 1458 | `		if( n < 4 \|\| z[1] < iLow \|\| z[1] > iHigh` |
|      80 | 1459 | `		 \|\| (z[2] & 0xC0) != 0x80 \|\| (z[3] & 0xC0) != 0x80 ){` |
|      40 | 1460 | `			return -1;` |
|       - | 1461 | `		}` |
|      73 | 1462 | `		*pLen = 4;` |
|     109 | 1463 | `		return (sxi32)(((c & 0x07) << 18) \| ((z[1] & 0x3F) << 12)` |
|      72 | 1464 | `			\| ((z[2] & 0x3F) << 6) \| (z[3] & 0x3F));` |
|       - | 1465 | `	}` |
|     502 | 1466 | `	return -1; /* 80..C1 as a lead byte, or F5..FF */` |
|   57743 | 1467 | `}` |
|       - | 1468 |  |
