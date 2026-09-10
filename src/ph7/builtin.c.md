# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4445/5194 lines (85.58%)

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
| 486888 |   18 | `PH7_PRIVATE sxi32 PH7_IntArgResolve(` |
|      - |   19 | `	ph7_context *pCtx,` |
|      - |   20 | `	ph7_value *pArg,` |
|      - |   21 | `	const char *zFunc,` |
|      - |   22 | `	int iArgNum,` |
|      - |   23 | `	const char *zParamName,` |
|      - |   24 | `	const char *zTypeStr,` |
|      - |   25 | `	sxi64 *pOut` |
|      5 |   26 | `){` |
| 486893 |   27 | `	if( ph7_value_is_null(pArg) ){` |
|    ! 0 |   28 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|      - |   29 | `			"%s(): Passing null to parameter #%d (%s) of type %s is deprecated",` |
|    ! 0 |   30 | `			zFunc,iArgNum,zParamName,zTypeStr` |
|      - |   31 | `			);` |
|    ! 0 |   32 | `		*pOut = 0;` |
|    ! 0 |   33 | `		return PH7_OK;` |
|      - |   34 | `	}` |
| 486893 |   35 | `	if( ph7_value_is_float(pArg) ){` |
|      7 |   36 | `		double dVal = ph7_value_to_double(pArg);` |
|      - |   37 | `		sxi64 iVal;` |
|      - |   38 | `		/* php: NAN/INF/out-of-int64-range floats fail ZPP outright */` |
|      7 |   39 | `		if( dVal != dVal \|\| dVal >= 9223372036854775808.0 \|\| dVal < -9223372036854775808.0 ){` |
|      7 |   40 | `			return PH7_VmThrowException(pCtx,` |
|      - |   41 | `				"TypeError",` |
|      - |   42 | `				"%s(): Argument #%d (%s) must be of type %s, float given",` |
|      2 |   43 | `				zFunc,iArgNum,zParamName,zTypeStr` |
|      - |   44 | `				);` |
|      - |   45 | `		}` |
|      3 |   46 | `		iVal = (sxi64)dVal;` |
|      3 |   47 | `		if( (double)iVal != dVal ){` |
|    ! 0 |   48 | `			PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|      - |   49 | `				"Implicit conversion from float %s to int loses precision",` |
|    ! 0 |   50 | `				ph7_value_to_string(pArg,0)` |
|      - |   51 | `				);` |
|    ! 0 |   52 | `		}` |
|      3 |   53 | `		*pOut = iVal;` |
|      3 |   54 | `		return PH7_OK;` |
|      - |   55 | `	}` |
| 486887 |   56 | `	if( ph7_value_is_string(pArg) ){` |
|      - |   57 | `		const char *zNum;` |
|      - |   58 | `		int nSlen;` |
|     15 |   59 | `		int i,bFloat = 0;` |
|     15 |   60 | `		if( !PH7_MemObjStringIsNumeric(pArg) ){` |
|     16 |   61 | `			return PH7_VmThrowException(pCtx,` |
|      - |   62 | `				"TypeError",` |
|      - |   63 | `				"%s(): Argument #%d (%s) must be of type %s, string given",` |
|      5 |   64 | `				zFunc,iArgNum,zParamName,zTypeStr` |
|      - |   65 | `				);` |
|      - |   66 | `		}` |
|      5 |   67 | `		zNum = ph7_value_to_string(pArg,&nSlen);` |
|      9 |   68 | `		for( i = 0 ; i < nSlen ; i++ ){` |
|      5 |   69 | `			if( zNum[i] == '.' \|\| zNum[i] == 'e' \|\| zNum[i] == 'E' ){` |
|    ! 0 |   70 | `				bFloat = 1;` |
|    ! 0 |   71 | `				break;` |
|      - |   72 | `			}` |
|      3 |   73 | `		}` |
|      5 |   74 | `		if( bFloat ){` |
|    ! 0 |   75 | `			double dVal = 0;` |
|      - |   76 | `			sxi64 iVal;` |
|    ! 0 |   77 | `			SyStrToReal(zNum,(sxu32)nSlen,(void *)&dVal,0);` |
|    ! 0 |   78 | `			if( dVal != dVal \|\| dVal >= 9223372036854775808.0 \|\| dVal < -9223372036854775808.0 ){` |
|    ! 0 |   79 | `				return PH7_VmThrowException(pCtx,` |
|      - |   80 | `					"TypeError",` |
|      - |   81 | `					"%s(): Argument #%d (%s) must be of type %s, string given",` |
|    ! 0 |   82 | `					zFunc,iArgNum,zParamName,zTypeStr` |
|      - |   83 | `					);` |
|      - |   84 | `			}` |
|    ! 0 |   85 | `			iVal = (sxi64)dVal;` |
|    ! 0 |   86 | `			if( (double)iVal != dVal ){` |
|    ! 0 |   87 | `				PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|      - |   88 | `					"Implicit conversion from float-string \"%s\" to int loses precision",` |
|    ! 0 |   89 | `					zNum` |
|      - |   90 | `					);` |
|    ! 0 |   91 | `			}` |
|    ! 0 |   92 | `			*pOut = iVal;` |
|    ! 0 |   93 | `			return PH7_OK;` |
|      - |   94 | `		}` |
|      5 |   95 | `		*pOut = ph7_value_to_int64(pArg);` |
|      5 |   96 | `		return PH7_OK;` |
|      - |   97 | `	}` |
| 486873 |   98 | `	if( !ph7_value_is_int(pArg) && !ph7_value_is_bool(pArg) ){` |
|      - |   99 | `		/* Arrays, resources and objects: php names the class for objects */` |
|    ! 0 |  100 | `		const char *zType = ph7_type_name(pArg);` |
|    ! 0 |  101 | `		if( ph7_value_is_object(pArg) ){` |
|    ! 0 |  102 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|    ! 0 |  103 | `			if( pInst && pInst->pClass ){` |
|    ! 0 |  104 | `				zType = SyStringData(&pInst->pClass->sName);` |
|    ! 0 |  105 | `			}` |
|    ! 0 |  106 | `		}` |
|    ! 0 |  107 | `		return PH7_VmThrowException(pCtx,` |
|      - |  108 | `			"TypeError",` |
|      - |  109 | `			"%s(): Argument #%d (%s) must be of type %s, %s given",` |
|    ! 0 |  110 | `			zFunc,iArgNum,zParamName,zTypeStr,zType` |
|      - |  111 | `			);` |
|      - |  112 | `	}` |
| 486873 |  113 | `	*pOut = ph7_value_to_int64(pArg);` |
| 486873 |  114 | `	return PH7_OK;` |
| 243449 |  115 | `}` |
|      - |  116 |  |
|      - |  117 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |  118 | `/* Forward decl: null-to-string ZPP deprecation notice (defined near the ZPP` |
|      - |  119 | ` * helpers; both live inside the same DISABLE_BUILTIN_FUNC region as every` |
|      - |  120 | ` * caller — the tiny build compiles none of them). */` |
|      - |  121 | `static void StrNullArgNotice(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNum,const char *zParamName);` |
|      - |  122 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - |  123 | `/* This file implement built-in 'foreign' functions for the PH7 engine */` |
|      - |  124 | `/*` |
|      - |  125 | ` * Section:` |
|      - |  126 | ` *    Variable handling Functions.` |
|      - |  127 | ` * Status:` |
|      - |  128 | ` *    Stable.` |
|      - |  129 | ` */` |
|      - |  130 | `/*` |
|      - |  131 | ` * bool is_bool($var)` |
|      - |  132 | ` *  Finds out whether a variable is a boolean.` |
|      - |  133 | ` * Parameters` |
|      - |  134 | ` *   $var: The variable being evaluated.` |
|      - |  135 | ` * Return` |
|      - |  136 | ` *  TRUE if var is a boolean. False otherwise.` |
|      - |  137 | ` */` |
|     72 |  138 | `static int PH7_builtin_is_bool(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  139 | `{` |
|     74 |  140 | `	int res = 0; /* Assume false by default */` |
|     74 |  141 | `	if( nArg > 0 ){` |
|     74 |  142 | `		res = ph7_value_is_bool(apArg[0]);` |
|     36 |  143 | `	}` |
|      - |  144 | `	/* Query result */` |
|     74 |  145 | `	ph7_result_bool(pCtx,res);` |
|     74 |  146 | `	return PH7_OK;` |
|      2 |  147 | `}` |
|      - |  148 | `/*` |
|      - |  149 | ` * bool is_float($var)` |
|      - |  150 | ` * bool is_real($var)` |
|      - |  151 | ` * bool is_double($var)` |
|      - |  152 | ` *  Finds out whether a variable is a float.` |
|      - |  153 | ` * Parameters` |
|      - |  154 | ` *   $var: The variable being evaluated.` |
|      - |  155 | ` * Return` |
|      - |  156 | ` *  TRUE if var is a float. False otherwise.` |
|      - |  157 | ` */` |
|    308 |  158 | `static int PH7_builtin_is_float(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  159 | `{` |
|    309 |  160 | `	int res = 0; /* Assume false by default */` |
|    309 |  161 | `	if( nArg > 0 ){` |
|    309 |  162 | `		res = ph7_value_is_float(apArg[0]);` |
|    154 |  163 | `	}` |
|      - |  164 | `	/* Query result */` |
|    309 |  165 | `	ph7_result_bool(pCtx,res);` |
|    309 |  166 | `	return PH7_OK;` |
|      1 |  167 | `}` |
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
|    922 |  178 | `static int PH7_builtin_is_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  179 | `{` |
|    926 |  180 | `	int res = 0; /* Assume false by default */` |
|    926 |  181 | `	if( nArg > 0 ){` |
|      - |  182 | `		/* Strict PHP identity: a float is never an int, even when it holds an` |
|      - |  183 | `		 * integer value (1.0). An integer-valued real carries both MEMOBJ_INT` |
|      - |  184 | `		 * (cached) and MEMOBJ_REAL, so REAL must be excluded here. */` |
|    926 |  185 | `		res = ph7_value_is_int(apArg[0]) && !ph7_value_is_float(apArg[0]);` |
|    461 |  186 | `	}` |
|      - |  187 | `	/* Query result */` |
|    926 |  188 | `	ph7_result_bool(pCtx,res);` |
|    926 |  189 | `	return PH7_OK;` |
|      4 |  190 | `}` |
|      - |  191 | `/*` |
|      - |  192 | ` * bool is_string($var)` |
|      - |  193 | ` *  Finds out whether a variable is a string.` |
|      - |  194 | ` * Parameters` |
|      - |  195 | ` *   $var: The variable being evaluated.` |
|      - |  196 | ` * Return` |
|      - |  197 | ` *  TRUE if var is string. False otherwise.` |
|      - |  198 | ` */` |
|   1100 |  199 | `static int PH7_builtin_is_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  200 | `{` |
|   1103 |  201 | `	int res = 0; /* Assume false by default */` |
|   1103 |  202 | `	if( nArg > 0 ){` |
|   1103 |  203 | `		res = ph7_value_is_string(apArg[0]);` |
|    550 |  204 | `	}` |
|      - |  205 | `	/* Query result */` |
|   1103 |  206 | `	ph7_result_bool(pCtx,res);` |
|   1103 |  207 | `	return PH7_OK;` |
|      3 |  208 | `}` |
|      - |  209 | `/*` |
|      - |  210 | ` * bool is_null($var)` |
|      - |  211 | ` *  Finds out whether a variable is NULL.` |
|      - |  212 | ` * Parameters` |
|      - |  213 | ` *   $var: The variable being evaluated.` |
|      - |  214 | ` * Return` |
|      - |  215 | ` *  TRUE if var is NULL. False otherwise.` |
|      - |  216 | ` */` |
|     86 |  217 | `static int PH7_builtin_is_null(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  218 | `{` |
|     90 |  219 | `	int res = 0; /* Assume false by default */` |
|     90 |  220 | `	if( nArg > 0 ){` |
|     90 |  221 | `		res = ph7_value_is_null(apArg[0]);` |
|     43 |  222 | `	}` |
|      - |  223 | `	/* Query result */` |
|     90 |  224 | `	ph7_result_bool(pCtx,res);` |
|     90 |  225 | `	return PH7_OK;` |
|      4 |  226 | `}` |
|      - |  227 | `/*` |
|      - |  228 | ` * bool is_numeric($var)` |
|      - |  229 | ` *  Find out whether a variable is NULL.` |
|      - |  230 | ` * Parameters` |
|      - |  231 | ` *  $var: The variable being evaluated.` |
|      - |  232 | ` * Return` |
|      - |  233 | ` *  True if var is numeric. False otherwise.` |
|      - |  234 | ` */` |
|     66 |  235 | `static int PH7_builtin_is_numeric(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  236 | `{` |
|     71 |  237 | `	int res = 0; /* Assume false by default */` |
|     71 |  238 | `	if( nArg > 0 ){` |
|     71 |  239 | `		res = ph7_value_is_numeric(apArg[0]);` |
|     33 |  240 | `	}` |
|      - |  241 | `	/* Query result */` |
|     71 |  242 | `	ph7_result_bool(pCtx,res);` |
|     71 |  243 | `	return PH7_OK;` |
|      5 |  244 | `}` |
|      - |  245 | `/*` |
|      - |  246 | ` * bool is_scalar($var)` |
|      - |  247 | ` *  Find out whether a variable is a scalar.` |
|      - |  248 | ` * Parameters` |
|      - |  249 | ` *  $var: The variable being evaluated.` |
|      - |  250 | ` * Return` |
|      - |  251 | ` *  True if var is scalar. False otherwise.` |
|      - |  252 | ` */` |
|     12 |  253 | `static int PH7_builtin_is_scalar(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  254 | `{` |
|     13 |  255 | `	int res = 0; /* Assume false by default */` |
|     13 |  256 | `	if( nArg > 0 ){` |
|     13 |  257 | `		res = ph7_value_is_scalar(apArg[0]);` |
|      6 |  258 | `	}` |
|      - |  259 | `	/* Query result */` |
|     13 |  260 | `	ph7_result_bool(pCtx,res);` |
|     13 |  261 | `	return PH7_OK;` |
|      1 |  262 | `}` |
|      - |  263 | `/*` |
|      - |  264 | ` * bool is_array($var)` |
|      - |  265 | ` *  Find out whether a variable is an array.` |
|      - |  266 | ` * Parameters` |
|      - |  267 | ` *  $var: The variable being evaluated.` |
|      - |  268 | ` * Return` |
|      - |  269 | ` *  True if var is an array. False otherwise.` |
|      - |  270 | ` */` |
|    760 |  271 | `static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  272 | `{` |
|    764 |  273 | `	int res = 0; /* Assume false by default */` |
|    764 |  274 | `	if( nArg > 0 ){` |
|    764 |  275 | `		res = ph7_value_is_array(apArg[0]);` |
|    380 |  276 | `	}` |
|      - |  277 | `	/* Query result */` |
|    764 |  278 | `	ph7_result_bool(pCtx,res);` |
|    764 |  279 | `	return PH7_OK;` |
|      4 |  280 | `}` |
|      - |  281 | `/*` |
|      - |  282 | ` * bool is_object($var)` |
|      - |  283 | ` *  Find out whether a variable is an object.` |
|      - |  284 | ` * Parameters` |
|      - |  285 | ` *  $var: The variable being evaluated.` |
|      - |  286 | ` * Return` |
|      - |  287 | ` *  True if var is an object. False otherwise.` |
|      - |  288 | ` */` |
|    516 |  289 | `static int PH7_builtin_is_object(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  290 | `{` |
|    519 |  291 | `	int res = 0; /* Assume false by default */` |
|    519 |  292 | `	if( nArg > 0 ){` |
|    519 |  293 | `		res = ph7_value_is_object(apArg[0]);` |
|    258 |  294 | `	}` |
|      - |  295 | `	/* Query result */` |
|    519 |  296 | `	ph7_result_bool(pCtx,res);` |
|    519 |  297 | `	return PH7_OK;` |
|      3 |  298 | `}` |
|      - |  299 | `/*` |
|      - |  300 | ` * bool is_resource($var)` |
|      - |  301 | ` *  Find out whether a variable is a resource.` |
|      - |  302 | ` * Parameters` |
|      - |  303 | ` *  $var: The variable being evaluated.` |
|      - |  304 | ` * Return` |
|      - |  305 | ` *  True if a resource. False otherwise.` |
|      - |  306 | ` */` |
|     62 |  307 | `static int PH7_builtin_is_resource(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  308 | `{` |
|     66 |  309 | `	int res = 0; /* Assume false by default */` |
|     66 |  310 | `	if( nArg > 0 ){` |
|     66 |  311 | `		res = ph7_value_is_resource(apArg[0]);` |
|     31 |  312 | `	}` |
|     66 |  313 | `	ph7_result_bool(pCtx,res);` |
|     66 |  314 | `	return PH7_OK;` |
|      4 |  315 | `}` |
|      - |  316 | `/*` |
|      - |  317 | ` * float floatval($var)` |
|      - |  318 | ` *  Get float value of a variable.` |
|      - |  319 | ` * Parameter` |
|      - |  320 | ` *  $var: The variable being processed.` |
|      - |  321 | ` * Return` |
|      - |  322 | ` *  the float value of a variable.` |
|      - |  323 | ` */` |
|      4 |  324 | `static int PH7_builtin_floatval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  325 | `{` |
|      5 |  326 | `	if( nArg < 1 ){` |
|      - |  327 | `		/* return 0.0 */` |
|    ! 0 |  328 | `		ph7_result_double(pCtx,0);` |
|    ! 0 |  329 | `	}else{` |
|      - |  330 | `		double dval;` |
|      - |  331 | `		/* Perform the cast */` |
|      5 |  332 | `		dval = ph7_value_to_double(apArg[0]);` |
|      5 |  333 | `		ph7_result_double(pCtx,dval);` |
|      - |  334 | `	}` |
|      5 |  335 | `	return PH7_OK;` |
|      1 |  336 | `}` |
|      - |  337 | `/*` |
|      - |  338 | ` * int intval($var)` |
|      - |  339 | ` *  Get integer value of a variable.` |
|      - |  340 | ` * Parameter` |
|      - |  341 | ` *  $var: The variable being processed.` |
|      - |  342 | ` * Return` |
|      - |  343 | ` *  the int value of a variable.` |
|      - |  344 | ` */` |
|     46 |  345 | `static int PH7_builtin_intval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  346 | `{` |
|     47 |  347 | `	if( nArg < 1 ){` |
|      - |  348 | `		/* return 0 */` |
|    ! 0 |  349 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  350 | `	}else{` |
|      - |  351 | `		sxi64 iVal;` |
|      - |  352 | `		/* Perform the cast */` |
|     47 |  353 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|     47 |  354 | `		ph7_result_int64(pCtx,iVal);` |
|      - |  355 | `	}` |
|     47 |  356 | `	return PH7_OK;` |
|      1 |  357 | `}` |
|      - |  358 | `/*` |
|      - |  359 | ` * string strval($var)` |
|      - |  360 | ` *  Get the string representation of a variable.` |
|      - |  361 | ` * Parameter` |
|      - |  362 | ` *  $var: The variable being processed.` |
|      - |  363 | ` * Return` |
|      - |  364 | ` *  the string value of a variable.` |
|      - |  365 | ` */` |
|      2 |  366 | `static int PH7_builtin_strval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  367 | `{` |
|      3 |  368 | `	if( nArg < 1 ){` |
|      - |  369 | `		/* return NULL */` |
|    ! 0 |  370 | `		ph7_result_null(pCtx);` |
|    ! 0 |  371 | `	}else{` |
|      - |  372 | `		const char *zVal;` |
|      3 |  373 | `		int iLen = 0; /* cc -O6 warning */` |
|      - |  374 | `		/* Perform the cast */` |
|      3 |  375 | `		zVal = ph7_value_to_string(apArg[0],&iLen);` |
|      3 |  376 | `		ph7_result_string(pCtx,zVal,iLen);` |
|      - |  377 | `	}` |
|      3 |  378 | `	return PH7_OK;` |
|      1 |  379 | `}` |
|      - |  380 | `/*` |
|      - |  381 | ` * bool boolval($var)` |
|      - |  382 | ` *  Get the boolean value of a variable.` |
|      - |  383 | ` * Parameter` |
|      - |  384 | ` *  $var: The variable being processed.` |
|      - |  385 | ` * Return` |
|      - |  386 | ` *  the bool value of a variable.` |
|      - |  387 | ` */` |
|     14 |  388 | `static int PH7_builtin_boolval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  389 | `{` |
|      - |  390 | `	int bVal;` |
|     15 |  391 | `	if( nArg != 1 ){` |
|    ! 0 |  392 | `		return PH7_VmThrowException(pCtx,` |
|      - |  393 | `			"ArgumentCountError",` |
|      - |  394 | `			"boolval() expects exactly 1 argument, %d given",` |
|    ! 0 |  395 | `			nArg` |
|      - |  396 | `			);` |
|      - |  397 | `	}` |
|      - |  398 | `	/* Perform the cast */` |
|     15 |  399 | `	bVal = ph7_value_to_bool(apArg[0]);` |
|     15 |  400 | `	ph7_result_bool(pCtx,bVal);` |
|     15 |  401 | `	return PH7_OK;` |
|      8 |  402 | `}` |
|      - |  403 | `/*` |
|      - |  404 | ` * bool empty($var)` |
|      - |  405 | ` *  Determine whether a variable is empty.` |
|      - |  406 | ` * Parameters` |
|      - |  407 | ` *   $var: The variable being checked.` |
|      - |  408 | ` * Return` |
|      - |  409 | ` *  0 if var has a non-empty and non-zero value.1 otherwise.` |
|      - |  410 | ` */` |
|  33788 |  411 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  412 | `{` |
|  33793 |  413 | `	int res = 1; /* Assume empty by default */` |
|  33793 |  414 | `	if( nArg > 0 ){` |
|  33791 |  415 | `		res = ph7_value_is_empty(apArg[0]);` |
|  16893 |  416 | `	}` |
|  33793 |  417 | `	ph7_result_bool(pCtx,res);` |
|  33793 |  418 | `	return PH7_OK;` |
|      - |  419 |  |
|      5 |  420 | `}` |
|      - |  421 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |  422 | `#define PH7_NEED_BUILTIN_REG 1` |
|      - |  423 | `#endif` |
|      - |  424 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - |  425 | `#define PH7_NEED_FMT_AND_INI 1` |
|      - |  426 | `#endif` |
|      - |  427 |  |
|      - |  428 | `/* Math functions moved to builtin_math.c */` |
|      - |  429 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - |  430 | `/*` |
|      - |  431 | ` * Section:` |
|      - |  432 | ` *    String handling Functions.` |
|      - |  433 | ` * Status:` |
|      - |  434 | ` *    Stable.` |
|      - |  435 | ` */` |
|      - |  436 | `/*` |
|      - |  437 | ` * string substr(string $string,int $start[, int $length ])` |
|      - |  438 | ` *  Return part of a string.` |
|      - |  439 | ` * Parameters` |
|      - |  440 | ` *  $string` |
|      - |  441 | ` *   The input string. Must be one character or longer.` |
|      - |  442 | ` * $start` |
|      - |  443 | ` *   If start is non-negative, the returned string will start at the start'th position` |
|      - |  444 | ` *   in string, counting from zero. For instance, in the string 'abcdef', the character` |
|      - |  445 | ` *   at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - |  446 | ` *   If start is negative, the returned string will start at the start'th character` |
|      - |  447 | ` *   from the end of string.` |
|      - |  448 | ` *   If string is less than or equal to start characters long, FALSE will be returned.` |
|      - |  449 | ` * $length` |
|      - |  450 | ` *   If length is given and is positive, the string returned will contain at most length` |
|      - |  451 | ` *   characters beginning from start (depending on the length of string).` |
|      - |  452 | ` *   If length is given and is negative, then that many characters will be omitted from` |
|      - |  453 | ` *   the end of string (after the start position has been calculated when a start is negative).` |
|      - |  454 | ` *   If start denotes the position of this truncation or beyond, false will be returned.` |
|      - |  455 | ` *   If length is given and is 0, FALSE or NULL an empty string will be returned.` |
|      - |  456 | ` *   If length is omitted, the substring starting from start until the end of the string` |
|      - |  457 | ` *   will be returned.` |
|      - |  458 | ` * Return` |
|      - |  459 | ` *  Returns the extracted part of string, or FALSE on failure or an empty string.` |
|      - |  460 | ` */` |
| 268532 |  461 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  462 | `{` |
|      - |  463 | `	const char *zSource;` |
|      - |  464 | `	int nSrcLen;` |
|      - |  465 | `	sxi64 iStart,iEnd;` |
| 268537 |  466 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"substr",1,"$string"); }` |
| 268537 |  467 | `	if( nArg < 2 ){` |
|      - |  468 | `		/* Arity is enforced at the call boundary; nothing sensible to return here. */` |
|    ! 0 |  469 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 |  470 | `		return PH7_OK;` |
|      - |  471 | `	}` |
|      - |  472 | `	/* Extract the target string */` |
| 268537 |  473 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|      - |  474 | `	/* Extract the offset */` |
|      - |  475 | `	{` |
| 268537 |  476 | `		sxi64 iTmp = 0;` |
| 268537 |  477 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"substr",2,"$offset","int",&iTmp);` |
| 268537 |  478 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |  479 | `			return rcArg;` |
|      - |  480 | `		}` |
| 268537 |  481 | `		iStart = iTmp;` |
|      - |  482 | `	}` |
|      - |  483 | `	/*` |
|      - |  484 | `	 * php 8 never answers substr() with FALSE — every out-of-range window simply` |
|      - |  485 | `	 * clamps to the empty string (substr("",0), substr("abc",5) and` |
|      - |  486 | `	 * substr("abc",1,-5) are all ""). PH7 returned FALSE for each of those, which` |
|      - |  487 | `	 * then flowed on as a bool into string context.` |
|      - |  488 | `	 *` |
|      - |  489 | `	 * A negative offset counts back from the end (clamped to 0); a negative length` |
|      - |  490 | `	 * leaves that many bytes off the end. Computed in sxi64 so an INT64 offset or` |
|      - |  491 | `	 * length cannot overflow the window arithmetic.` |
|      - |  492 | `	 */` |
| 268537 |  493 | `	if( iStart < 0 ){` |
|  33495 |  494 | `		iStart += nSrcLen;` |
|  33495 |  495 | `		if( iStart < 0 ){` |
|      5 |  496 | `			iStart = 0;` |
|      7 |  497 | `		}` |
| 251792 |  498 | `	}else if( iStart > nSrcLen ){` |
|      7 |  499 | `		iStart = nSrcLen;` |
|      3 |  500 | `	}` |
| 268537 |  501 | `	iEnd = nSrcLen;` |
| 268537 |  502 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
| 197325 |  503 | `		sxi64 iLen = 0;` |
| 197325 |  504 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[2],"substr",3,"$length","?int",&iLen);` |
| 197325 |  505 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |  506 | `			return rcArg;` |
|      - |  507 | `		}` |
| 197325 |  508 | `		if( iLen < 0 ){` |
|  33127 |  509 | `			iEnd = (sxi64)nSrcLen + iLen;` |
| 180764 |  510 | `		}else if( iLen > (sxi64)nSrcLen - iStart ){` |
|  18907 |  511 | `			iEnd = nSrcLen;` |
|   9456 |  512 | `		}else{` |
| 145301 |  513 | `			iEnd = iStart + iLen;` |
|      - |  514 | `		}` |
|  98660 |  515 | `	}` |
| 268537 |  516 | `	if( iEnd < iStart ){` |
|      3 |  517 | `		iEnd = iStart;` |
|      1 |  518 | `	}` |
| 268537 |  519 | `	ph7_result_string(pCtx,&zSource[iStart],(int)(iEnd - iStart));` |
| 268537 |  520 | `	return PH7_OK;` |
| 134271 |  521 | `}` |
|      - |  522 | `/*` |
|      - |  523 | ` * int substr_compare(string $main_str,string $str ,int $offset[,int $length[,bool $case_insensitivity = false ]])` |
|      - |  524 | ` *  Binary safe comparison of two strings from an offset, up to length characters.` |
|      - |  525 | ` * Parameters` |
|      - |  526 | ` *  $main_str` |
|      - |  527 | ` *  The main string being compared.` |
|      - |  528 | ` *  $str` |
|      - |  529 | ` *   The secondary string being compared.` |
|      - |  530 | ` * $offset` |
|      - |  531 | ` *  The start position for the comparison. If negative, it starts counting from` |
|      - |  532 | ` *  the end of the string.` |
|      - |  533 | ` * $length` |
|      - |  534 | ` *  The length of the comparison. The default value is the largest of the length` |
|      - |  535 | ` *  of the str compared to the length of main_str less the offset.` |
|      - |  536 | ` * $case_insensitivity` |
|      - |  537 | ` *  If case_insensitivity is TRUE, comparison is case insensitive.` |
|      - |  538 | ` * Return` |
|      - |  539 | ` *  Returns < 0 if main_str from position offset is less than str, > 0 if it is greater than` |
|      - |  540 | ` *  str, and 0 if they are equal. If offset is equal to or greater than the length of main_str` |
|      - |  541 | ` *  or length is set and is less than 1, substr_compare() prints a warning and returns FALSE.` |
|      - |  542 | ` */` |
|     20 |  543 | `static int PH7_builtin_substr_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  544 | `{` |
|      - |  545 | `	const char *zSource,*zSub;` |
|      - |  546 | `	int nSrcLen,nSubLen;` |
|      - |  547 | `	sxi64 iOfft,iLen,l1,l2,nCmp;` |
|     21 |  548 | `	int iCase = 0;` |
|      - |  549 | `	int rc;` |
|     21 |  550 | `	if( nArg < 3 ){` |
|    ! 0 |  551 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  552 | `		return PH7_OK;` |
|      - |  553 | `	}` |
|     21 |  554 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|     21 |  555 | `	zSub    = ph7_value_to_string(apArg[1],&nSubLen);` |
|      - |  556 | `	{` |
|     21 |  557 | `		sxi64 iTmp = 0;` |
|     21 |  558 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[2],"substr_compare",3,"$offset","int",&iTmp);` |
|     21 |  559 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |  560 | `			return rcArg;` |
|      - |  561 | `		}` |
|     21 |  562 | `		iOfft = iTmp;` |
|      - |  563 | `	}` |
|     21 |  564 | `	if( iOfft < 0 ){` |
|      5 |  565 | `		iOfft += nSrcLen;` |
|      5 |  566 | `		if( iOfft < 0 ){` |
|      3 |  567 | `			iOfft = 0;` |
|      1 |  568 | `		}` |
|      2 |  569 | `	}` |
|     21 |  570 | `	if( iOfft > nSrcLen ){` |
|      - |  571 | `		/* php rejects an offset past the end of the haystack outright */` |
|    ! 0 |  572 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  573 | `			"substr_compare(): Argument #3 ($offset) must be contained in argument #1 ($haystack)");` |
|      - |  574 | `	}` |
|      - |  575 | `	/* A NULL/absent length compares as far as the longer of the two operands reaches */` |
|     21 |  576 | `	iLen = (sxi64)nSrcLen - iOfft;` |
|     21 |  577 | `	if( iLen < nSubLen ){` |
|      5 |  578 | `		iLen = nSubLen;` |
|      2 |  579 | `	}` |
|     21 |  580 | `	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){` |
|     11 |  581 | `		sxi64 iTmp = 0;` |
|     11 |  582 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[3],"substr_compare",4,"$length","?int",&iTmp);` |
|     11 |  583 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |  584 | `			return rcArg;` |
|      - |  585 | `		}` |
|     11 |  586 | `		if( iTmp < 0 ){` |
|      3 |  587 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  588 | `				"substr_compare(): Argument #4 ($length) must be greater than or equal to 0");` |
|      - |  589 | `		}` |
|      9 |  590 | `		iLen = iTmp;` |
|      4 |  591 | `	}` |
|     19 |  592 | `	if( nArg > 4 ){` |
|      5 |  593 | `		iCase = ph7_value_to_bool(apArg[4]);` |
|      2 |  594 | `	}` |
|      - |  595 | `	/* Each side contributes at most what it actually has left */` |
|     19 |  596 | `	l1 = (sxi64)nSrcLen - iOfft;` |
|     19 |  597 | `	if( l1 > iLen ){ l1 = iLen; }` |
|     19 |  598 | `	l2 = nSubLen;` |
|     19 |  599 | `	if( l2 > iLen ){ l2 = iLen; }` |
|     19 |  600 | `	nCmp = (l1 < l2) ? l1 : l2;` |
|     19 |  601 | `	if( iCase ){` |
|      3 |  602 | `		rc = SyStrnicmp(&zSource[iOfft],zSub,(sxu32)nCmp);` |
|      2 |  603 | `	}else{` |
|     17 |  604 | `		rc = SyStrncmp(&zSource[iOfft],zSub,(sxu32)nCmp);` |
|      - |  605 | `	}` |
|     19 |  606 | `	if( rc == 0 ){` |
|      - |  607 | `		/* Prefixes equal: php falls back to a THREE-WAY compare of the lengths, so this` |
|      - |  608 | `		 * arm is normalized to -1/0/1 (substr_compare("abc","",0) is 1, not 3). */` |
|      9 |  609 | `		rc = (l1 == l2) ? 0 : (l1 < l2 ? -1 : 1);` |
|      4 |  610 | `	}` |
|      - |  611 | `	/* ...but when the prefixes differ php returns the RAW byte difference, not its sign:` |
|      - |  612 | `	 * substr_compare("abc","def",1,10) is -2 ('b' - 'd'), which is what SyMemcmp gives. */` |
|     19 |  613 | `	ph7_result_int(pCtx,rc);` |
|     19 |  614 | `	return PH7_OK;` |
|     11 |  615 | `}` |
|      - |  616 | `/*` |
|      - |  617 | ` * int substr_count(string $haystack,string $needle[,int $offset = 0 [,int $length ]])` |
|      - |  618 | ` *  Count the number of substring occurrences.` |
|      - |  619 | ` * Parameters` |
|      - |  620 | ` * $haystack` |
|      - |  621 | ` *   The string to search in` |
|      - |  622 | ` * $needle` |
|      - |  623 | ` *   The substring to search for` |
|      - |  624 | ` * $offset` |
|      - |  625 | ` *  The offset where to start counting` |
|      - |  626 | ` * $length (NOT USED)` |
|      - |  627 | ` *  The maximum length after the specified offset to search for the substring.` |
|      - |  628 | ` *  It outputs a warning if the offset plus the length is greater than the haystack length.` |
|      - |  629 | ` * Return` |
|      - |  630 | ` *  Toral number of substring occurrences.` |
|      - |  631 | ` */` |
|     26 |  632 | `static int PH7_builtin_substr_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  633 | `{` |
|      - |  634 | `	const char *zText,*zPattern,*zEnd;` |
|      - |  635 | `	int nTextlen,nPatlen;` |
|     27 |  636 | `	int iCount = 0;` |
|      - |  637 | `	sxu32 nOfft;` |
|      - |  638 | `	sxi32 rc;` |
|     27 |  639 | `	if( nArg < 2 ){` |
|      - |  640 | `		/* Missing arguments */` |
|    ! 0 |  641 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  642 | `		return PH7_OK;` |
|      - |  643 | `	}` |
|      - |  644 | `	/* Point to the haystack */` |
|     27 |  645 | `	zText = ph7_value_to_string(apArg[0],&nTextlen);` |
|      - |  646 | `	/* Point to the neddle */` |
|     27 |  647 | `	zPattern = ph7_value_to_string(apArg[1],&nPatlen);` |
|     27 |  648 | `	if( nPatlen < 1 ){` |
|      - |  649 | `		/* Empty needle: PHP 8 throws a catchable ValueError. */` |
|      3 |  650 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  651 | `			"substr_count(): Argument #2 ($needle) must not be empty");` |
|      - |  652 | `	}` |
|      - |  653 | `	/* Apply the optional $offset/$length window before searching. PHP 8 validates` |
|      - |  654 | `	 * both against the haystack (a negative value counts from the end) and throws a` |
|      - |  655 | `	 * catchable ValueError when the result falls outside it — this happens before the` |
|      - |  656 | `	 * needle-fits check, so it fires even when the needle is longer than the haystack. */` |
|     25 |  657 | `	if( nArg > 2 ){` |
|     19 |  658 | `		ph7_int64 iOfft = ph7_value_to_int64(apArg[2]);` |
|     19 |  659 | `		if( iOfft < 0 ){` |
|      5 |  660 | `			iOfft += nTextlen;` |
|      2 |  661 | `		}` |
|     19 |  662 | `		if( iOfft < 0 \|\| iOfft > nTextlen ){` |
|      3 |  663 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  664 | `				"substr_count(): Argument #3 ($offset) must be contained in argument #1 ($haystack)");` |
|      - |  665 | `		}` |
|      - |  666 | `		/* Point to the desired offset and shrink the remaining region */` |
|     17 |  667 | `		zText = &zText[iOfft];` |
|     17 |  668 | `		nTextlen -= (int)iOfft;` |
|      8 |  669 | `	}` |
|     23 |  670 | `	if( nArg > 3 ){` |
|     15 |  671 | `		ph7_int64 nLen = ph7_value_to_int64(apArg[3]);` |
|     15 |  672 | `		if( nLen < 0 ){` |
|      - |  673 | `			/* Negative length is relative to the end of the (offset) haystack */` |
|      5 |  674 | `			nLen += nTextlen;` |
|      2 |  675 | `		}` |
|     15 |  676 | `		if( nLen < 0 \|\| nLen > nTextlen ){` |
|      5 |  677 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  678 | `				"substr_count(): Argument #4 ($length) must be contained in argument #1 ($haystack)");` |
|      - |  679 | `		}` |
|     11 |  680 | `		nTextlen = (int)nLen;` |
|      5 |  681 | `	}` |
|     19 |  682 | `	if( nTextlen < 1 \|\| nPatlen > nTextlen ){` |
|      - |  683 | `		/* The windowed haystack can't contain the needle: zero matches */` |
|      3 |  684 | `		ph7_result_int(pCtx,0);` |
|      3 |  685 | `		return PH7_OK;` |
|      - |  686 | `	}` |
|      - |  687 | `	/* Point to the end of the windowed haystack */` |
|     17 |  688 | `	zEnd = &zText[nTextlen];` |
|      - |  689 | `	/* Perform the search */` |
|     17 |  690 | `	for(;;){` |
|     35 |  691 | `		rc = SyBlobSearch((const void *)zText,(sxu32)(zEnd-zText),(const void *)zPattern,nPatlen,&nOfft);` |
|     35 |  692 | `		if( rc != SXRET_OK ){` |
|      - |  693 | `			/* Pattern not found,break immediately */` |
|     13 |  694 | `			break;` |
|      - |  695 | `		}` |
|      - |  696 | `		/* Increment counter and update the offset */` |
|     23 |  697 | `		iCount++;` |
|     23 |  698 | `		zText += nOfft + nPatlen;` |
|     23 |  699 | `		if( zText >= zEnd ){` |
|      5 |  700 | `			break;` |
|      - |  701 | `		}` |
|      1 |  702 | `	}` |
|      - |  703 | `	/* Pattern count */` |
|     17 |  704 | `	ph7_result_int(pCtx,iCount);` |
|     17 |  705 | `	return PH7_OK;` |
|     14 |  706 | `}` |
|      - |  707 | `/* Forward declarations: defined with the trim/addcslashes and str_contains` |
|      - |  708 | ` * families below. */` |
|      - |  709 | `static void PH7_BuildCharMask(ph7_context *pCtx,const char *zList,int nLen,char aMask[256]);` |
|      - |  710 | `/*` |
|      - |  711 | ` * php 8.1 null-to-non-nullable ZPP deprecation, notice-only form for the` |
|      - |  712 | ` * legacy string builtins that still coerce null to "" themselves: emit` |
|      - |  713 | ``  * `f(): Passing null to parameter #N ($name) of type string is deprecated` `` |
|      - |  714 | ` * when the arg is an actual null, leaving the resolution unchanged.` |
|      - |  715 | ` */` |
| 395808 |  716 | `static void StrNullArgNotice(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNum,const char *zParamName)` |
|      5 |  717 | `{` |
| 395813 |  718 | `	if( ph7_value_is_null(pArg) ){` |
|     22 |  719 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|      - |  720 | `			"%s(): Passing null to parameter #%d (%s) of type string is deprecated",` |
|      7 |  721 | `			zFunc,iArgNum,zParamName);` |
|      7 |  722 | `	}` |
| 395813 |  723 | `}` |
|      - |  724 | `static sxi32 StrPredicateResolveArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,` |
|      - |  725 | `	int iArgNum,const char *zParamName,const char *zTypeStr,const char *zNullMsg,` |
|      - |  726 | `	ph7_value *pTmp,const char **pzOut,int *pnOut);` |
|      - |  727 | `/*` |
|      - |  728 | ` * Validate and resolve an int-typed builtin parameter with php-8 ZPP weak-mode` |
|      - |  729 | ` * semantics: ints and bools pass through; null emits the 8.1 deprecation and` |
|      - |  730 | ` * resolves to 0; floats and float-strings convert, with the implicit-conversion` |
|      - |  731 | ` * E_DEPRECATED when lossy and a TypeError when NAN/INF/out of int range;` |
|      - |  732 | ` * integral numeric strings convert exactly; everything else (arrays, resources,` |
|      - |  733 | ` * objects, non-numeric strings) is a TypeError naming zTypeStr (e.g. "int",` |
|      - |  734 | ` * "array\|int"). Returns PH7_OK with *pOut set, or the throw status.` |
|      - |  735 | ` */` |
|      - |  736 | `/*` |
|      - |  737 | ` * Normalize a substr_replace() offset/length pair against a string of nStrLen` |
|      - |  738 | ` * bytes, exactly like PHP: a negative offset counts from the end (clamped to 0),` |
|      - |  739 | ` * an offset past the end clamps to the end; a negative length leaves that many` |
|      - |  740 | ` * bytes off the end of the remaining region (clamped to 0), and the length is` |
|      - |  741 | ` * finally clamped to the remaining region. Written without f+l additions so an` |
|      - |  742 | ` * INT64_MAX length cannot overflow.` |
|      - |  743 | ` */` |
|     60 |  744 | `static void SubstrReplaceWindow(sxi64 *pF,sxi64 *pL,int nStrLen)` |
|      1 |  745 | `{` |
|     61 |  746 | `	sxi64 f = *pF,l = *pL;` |
|     61 |  747 | `	if( f < 0 ){` |
|      9 |  748 | `		f += nStrLen;` |
|      9 |  749 | `		if( f < 0 ){` |
|      5 |  750 | `			f = 0;` |
|      3 |  751 | `		}` |
|     57 |  752 | `	}else if( f > nStrLen ){` |
|      5 |  753 | `		f = nStrLen;` |
|      2 |  754 | `	}` |
|     61 |  755 | `	if( l < 0 ){` |
|      7 |  756 | `		l += nStrLen - f;` |
|      7 |  757 | `		if( l < 0 ){` |
|      5 |  758 | `			l = 0;` |
|      2 |  759 | `		}` |
|      3 |  760 | `	}` |
|     61 |  761 | `	if( l > nStrLen - f ){` |
|     25 |  762 | `		l = nStrLen - f;` |
|     12 |  763 | `	}` |
|     61 |  764 | `	*pF = f;` |
|     61 |  765 | `	*pL = l;` |
|     61 |  766 | `}` |
|      - |  767 | `/* A replacement string collected out of substr_replace()'s $replace array.` |
|      - |  768 | ` * The bytes live in a shared pool blob (walker values are transient), so the` |
|      - |  769 | ` * item stores pool offsets, mirroring the strtr_entry technique. */` |
|      - |  770 | `typedef struct substr_repl_item substr_repl_item;` |
|      - |  771 | `struct substr_repl_item` |
|      - |  772 | `{` |
|      - |  773 | `	sxu32 nOfft; /* Offset of the string inside the pool */` |
|      - |  774 | `	sxu32 nLen;  /* Length of the string */` |
|      - |  775 | `};` |
|      - |  776 | `typedef struct substr_replace_collect substr_replace_collect;` |
|      - |  777 | `struct substr_replace_collect` |
|      - |  778 | `{` |
|      - |  779 | `	SyBlob *pPool;  /* Byte pool for string items (string walker only) */` |
|      - |  780 | `	SySet *pSet;    /* substr_repl_item set (string) or sxi64 set (int) */` |
|      - |  781 | `	sxi32 rc;       /* SXRET_OK or SXERR_MEM on collector failure */` |
|      - |  782 | `};` |
|      - |  783 | `/* ph7_array_walk() callback: append one $replace element to the pool. */` |
|      6 |  784 | `static int SubstrReplaceStrWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 |  785 | `{` |
|      7 |  786 | `	substr_replace_collect *pCol = (substr_replace_collect *)pUserData;` |
|      - |  787 | `	substr_repl_item sItem;` |
|      - |  788 | `	const char *zStr;` |
|      - |  789 | `	int nLen;` |
|      3 |  790 | `	SXUNUSED(pKey);` |
|      7 |  791 | `	zStr = ph7_value_to_string(pData,&nLen);` |
|      7 |  792 | `	sItem.nOfft = SyBlobLength(pCol->pPool);` |
|      7 |  793 | `	sItem.nLen = (sxu32)nLen;` |
|      7 |  794 | `	if( nLen > 0 && SXRET_OK != SyBlobAppend(pCol->pPool,(const void *)zStr,(sxu32)nLen) ){` |
|    ! 0 |  795 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 |  796 | `		return SXERR_ABORT;` |
|      - |  797 | `	}` |
|      7 |  798 | `	if( SXRET_OK != SySetPut(pCol->pSet,(const void *)&sItem) ){` |
|    ! 0 |  799 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 |  800 | `		return SXERR_ABORT;` |
|      - |  801 | `	}` |
|      7 |  802 | `	return PH7_OK;` |
|      4 |  803 | `}` |
|      - |  804 | `/* ph7_array_walk() callback: collect one $offset/$length element as an int. */` |
|     12 |  805 | `static int SubstrReplaceIntWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 |  806 | `{` |
|     13 |  807 | `	substr_replace_collect *pCol = (substr_replace_collect *)pUserData;` |
|     13 |  808 | `	sxi64 iVal = ph7_value_to_int64(pData);` |
|      6 |  809 | `	SXUNUSED(pKey);` |
|     13 |  810 | `	if( SXRET_OK != SySetPut(pCol->pSet,(const void *)&iVal) ){` |
|    ! 0 |  811 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 |  812 | `		return SXERR_ABORT;` |
|      - |  813 | `	}` |
|     13 |  814 | `	return PH7_OK;` |
|      7 |  815 | `}` |
|      - |  816 | `/* Per-element state while walking substr_replace()'s array $string. */` |
|      - |  817 | `typedef struct substr_replace_ctx substr_replace_ctx;` |
|      - |  818 | `struct substr_replace_ctx` |
|      - |  819 | `{` |
|      - |  820 | `	ph7_value *pResult;   /* Result array (keys preserved) */` |
|      - |  821 | `	ph7_value *pScratch;  /* Reusable string value for each element */` |
|      - |  822 | `	SyBlob *pReplPool;    /* Pool behind aRepl items */` |
|      - |  823 | `	SySet *pRepl;         /* substr_repl_item set or NULL when $replace is scalar */` |
|      - |  824 | `	SySet *pFrom;         /* sxi64 set or NULL when $offset is scalar */` |
|      - |  825 | `	SySet *pLen;          /* sxi64 set or NULL when $length is scalar/absent */` |
|      - |  826 | `	sxu32 iReplCur;       /* Next-position cursors into the three sets */` |
|      - |  827 | `	sxu32 iFromCur;` |
|      - |  828 | `	sxu32 iLenCur;` |
|      - |  829 | `	const char *zRepl;    /* Scalar $replace */` |
|      - |  830 | `	int nRepl;` |
|      - |  831 | `	sxi64 iFrom;          /* Scalar $offset */` |
|      - |  832 | `	sxi64 iLen;           /* Scalar $length */` |
|      - |  833 | `	int bLenGiven;        /* FALSE: $length absent/null -> element length */` |
|      - |  834 | `	sxi32 rc;             /* SXRET_OK or SXERR_MEM */` |
|      - |  835 | `};` |
|      - |  836 | `/*` |
|      - |  837 | ` * ph7_array_walk() callback over the array $string: replace the window of one` |
|      - |  838 | ` * element and insert the result under the element's original key. Array-form` |
|      - |  839 | ` * $replace/$offset/$length are consumed positionally; when a set runs out PHP` |
|      - |  840 | ` * falls back to ""/0/element-length respectively.` |
|      - |  841 | ` */` |
|     24 |  842 | `static int SubstrReplaceElemWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 |  843 | `{` |
|     25 |  844 | `	substr_replace_ctx *pRep = (substr_replace_ctx *)pUserData;` |
|      - |  845 | `	const char *zStr,*zRepl;` |
|      - |  846 | `	sxi64 f,l;` |
|      - |  847 | `	int nLen,nRepl;` |
|     25 |  848 | `	zStr = ph7_value_to_string(pData,&nLen);` |
|      - |  849 | `	/* Positional $replace element ("" when exhausted) */` |
|     25 |  850 | `	if( pRep->pRepl ){` |
|     11 |  851 | `		if( pRep->iReplCur < SySetUsed(pRep->pRepl) ){` |
|      7 |  852 | `			substr_repl_item *pItem = (substr_repl_item *)SySetAt(pRep->pRepl,pRep->iReplCur++);` |
|      7 |  853 | `			zRepl = (const char *)SyBlobDataAt(pRep->pReplPool,pItem->nOfft);` |
|      7 |  854 | `			nRepl = (int)pItem->nLen;` |
|      4 |  855 | `		}else{` |
|      5 |  856 | `			zRepl = "";` |
|      5 |  857 | `			nRepl = 0;` |
|      - |  858 | `		}` |
|      6 |  859 | `	}else{` |
|     15 |  860 | `		zRepl = pRep->zRepl;` |
|     15 |  861 | `		nRepl = pRep->nRepl;` |
|      - |  862 | `	}` |
|      - |  863 | `	/* Positional $offset element (0 when exhausted) */` |
|     25 |  864 | `	if( pRep->pFrom ){` |
|     13 |  865 | `		sxi64 *pVal = 0;` |
|     13 |  866 | `		if( pRep->iFromCur < SySetUsed(pRep->pFrom) ){` |
|      9 |  867 | `			pVal = (sxi64 *)SySetAt(pRep->pFrom,pRep->iFromCur++);` |
|      4 |  868 | `		}` |
|     13 |  869 | `		f = pVal ? *pVal : 0;` |
|      7 |  870 | `	}else{` |
|     13 |  871 | `		f = pRep->iFrom;` |
|      - |  872 | `	}` |
|      - |  873 | `	/* Positional $length element (element length when exhausted) */` |
|     25 |  874 | `	if( pRep->pLen ){` |
|      7 |  875 | `		sxi64 *pVal = 0;` |
|      7 |  876 | `		if( pRep->iLenCur < SySetUsed(pRep->pLen) ){` |
|      5 |  877 | `			pVal = (sxi64 *)SySetAt(pRep->pLen,pRep->iLenCur++);` |
|      2 |  878 | `		}` |
|      7 |  879 | `		l = pVal ? *pVal : nLen;` |
|      4 |  880 | `	}else{` |
|     19 |  881 | `		l = pRep->bLenGiven ? pRep->iLen : nLen;` |
|      - |  882 | `	}` |
|     25 |  883 | `	SubstrReplaceWindow(&f,&l,nLen);` |
|      - |  884 | `	/* Assemble prefix + replacement + suffix in the scratch value */` |
|     25 |  885 | `	ph7_value_reset_string_cursor(pRep->pScratch);` |
|     24 |  886 | `	if( (f > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,zStr,(int)f))` |
|     24 |  887 | `	 \|\| (nRepl > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,zRepl,nRepl))` |
|     40 |  888 | `	 \|\| (nLen - (int)(f+l) > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,&zStr[f+l],nLen - (int)(f+l))) ){` |
|     30 |  889 | `		pRep->rc = SXERR_MEM;` |
|     30 |  890 | `		return SXERR_ABORT;` |
|      - |  891 | `	}` |
|     25 |  892 | `	if( SXRET_OK != ph7_array_add_elem(pRep->pResult,pKey,pRep->pScratch) ){` |
|    ! 0 |  893 | `		pRep->rc = SXERR_MEM;` |
|    ! 0 |  894 | `		return SXERR_ABORT;` |
|      - |  895 | `	}` |
|     25 |  896 | `	return PH7_OK;` |
|     43 |  897 | `}` |
|      - |  898 | `/*` |
|      - |  899 | ` * mixed substr_replace(array\|string $string,array\|string $replace,array\|int $offset[,array\|int\|null $length = null])` |
|      - |  900 | ` *  Replace text within a portion of a string.` |
|      - |  901 | ` * Parameters` |
|      - |  902 | ` *  $string` |
|      - |  903 | ` *   The input string or an array of strings (each element is processed with` |
|      - |  904 | ` *   its own positional replace/offset/length when those are arrays too).` |
|      - |  905 | ` *  $replace` |
|      - |  906 | ` *   The replacement string. When $string is scalar and $replace is an array,` |
|      - |  907 | ` *   only its first element is used (PHP quirk).` |
|      - |  908 | ` *  $offset` |
|      - |  909 | ` *   Window start; negative counts from the end of the string.` |
|      - |  910 | ` *  $length` |
|      - |  911 | ` *   Window length; negative leaves that many bytes at the end; null/absent` |
|      - |  912 | ` *   means "to the end of the string".` |
|      - |  913 | ` * Return` |
|      - |  914 | ` *  The processed string, or an array of processed strings (keys preserved).` |
|      - |  915 | ` * Errors` |
|      - |  916 | ` *  ArgumentCountError on fewer than 3 arguments; TypeError when an array` |
|      - |  917 | ` *  $offset/$length is combined with a scalar $string.` |
|      - |  918 | ` */` |
|     58 |  919 | `static int PH7_builtin_substr_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  920 | `{` |
|      - |  921 | `	ph7_value sStrTmp,sReplTmp;` |
|     59 |  922 | `	const char *zStr = 0,*zRepl = 0;` |
|     59 |  923 | `	int nLen = 0,nRepl = 0;` |
|      - |  924 | `	int bLenGiven;` |
|     59 |  925 | `	sxi64 f = 0,l = 0;` |
|      - |  926 | `	sxi32 rc;` |
|     59 |  927 | `	if( nArg < 3 ){` |
|    ! 0 |  928 | `		return PH7_VmThrowException(pCtx,` |
|      - |  929 | `			"ArgumentCountError",` |
|      - |  930 | `			"substr_replace() expects at least 3 arguments, %d given",` |
|    ! 0 |  931 | `			nArg` |
|      - |  932 | `			);` |
|      - |  933 | `	}` |
|      - |  934 | `	/* $length counts as given unless absent or null (php: ?null semantics) */` |
|     59 |  935 | `	bLenGiven = (nArg > 3 && !ph7_value_is_null(apArg[3]));` |
|      - |  936 | `	/* php ZPP validates all four args, in order, before the body runs: the` |
|      - |  937 | `	 * non-array forms resolve here (null deprecation, __toString objects,` |
|      - |  938 | `	 * numeric strings), arrays pass through to the per-mode handling. */` |
|     59 |  939 | `	PH7_MemObjInit(pCtx->pVm,&sStrTmp);` |
|     59 |  940 | `	PH7_MemObjInit(pCtx->pVm,&sReplTmp);` |
|     59 |  941 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     45 |  942 | `		rc = StrPredicateResolveArg(pCtx,apArg[0],"substr_replace",1,"$string","array\|string",` |
|      - |  943 | `			"substr_replace(): Passing null to parameter #1 ($string) "` |
|      - |  944 | `			"of type array\|string is deprecated",` |
|      - |  945 | `			&sStrTmp,&zStr,&nLen);` |
|     45 |  946 | `		if( rc != PH7_OK ) goto out;` |
|     22 |  947 | `	}` |
|     59 |  948 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|     51 |  949 | `		rc = StrPredicateResolveArg(pCtx,apArg[1],"substr_replace",2,"$replace","array\|string",` |
|      - |  950 | `			"substr_replace(): Passing null to parameter #2 ($replace) "` |
|      - |  951 | `			"of type array\|string is deprecated",` |
|      - |  952 | `			&sReplTmp,&zRepl,&nRepl);` |
|     51 |  953 | `		if( rc != PH7_OK ) goto out;` |
|     25 |  954 | `	}` |
|     59 |  955 | `	if( !ph7_value_is_array(apArg[2]) ){` |
|     51 |  956 | `		rc = PH7_IntArgResolve(pCtx,apArg[2],"substr_replace",3,"$offset","array\|int",&f);` |
|     51 |  957 | `		if( rc != PH7_OK ) goto out;` |
|     24 |  958 | `	}` |
|     57 |  959 | `	if( bLenGiven && !ph7_value_is_array(apArg[3]) ){` |
|     31 |  960 | `		rc = PH7_IntArgResolve(pCtx,apArg[3],"substr_replace",4,"$length","array\|int\|null",&l);` |
|     31 |  961 | `		if( rc != PH7_OK ) goto out;` |
|     14 |  962 | `	}` |
|     55 |  963 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - |  964 | `		/* Array form: process each element, preserving keys */` |
|      - |  965 | `		substr_replace_ctx sRep;` |
|      - |  966 | `		substr_replace_collect sCol;` |
|      - |  967 | `		SyBlob sReplPool;` |
|      - |  968 | `		SySet sRepl,sFrom,sLen;` |
|      - |  969 | `		ph7_value *pResult,*pScratch;` |
|     15 |  970 | `		sxi32 rcWalk = SXRET_OK;` |
|     15 |  971 | `		SyBlobInit(&sReplPool,&pCtx->pVm->sAllocator);` |
|     15 |  972 | `		SySetInit(&sRepl,&pCtx->pVm->sAllocator,sizeof(substr_repl_item));` |
|     15 |  973 | `		SySetInit(&sFrom,&pCtx->pVm->sAllocator,sizeof(sxi64));` |
|     15 |  974 | `		SySetInit(&sLen,&pCtx->pVm->sAllocator,sizeof(sxi64));` |
|     15 |  975 | `		SyZero(&sRep,sizeof(substr_replace_ctx));` |
|     15 |  976 | `		sRep.bLenGiven = bLenGiven;` |
|     15 |  977 | `		sCol.rc = SXRET_OK;` |
|      - |  978 | `		/* Collect array-form $replace/$offset/$length positionally; the` |
|      - |  979 | `		 * scalar forms were already resolved above. */` |
|     15 |  980 | `		if( ph7_value_is_array(apArg[1]) ){` |
|      5 |  981 | `			sCol.pPool = &sReplPool;` |
|      5 |  982 | `			sCol.pSet = &sRepl;` |
|      5 |  983 | `			ph7_array_walk(apArg[1],SubstrReplaceStrWalker,&sCol);` |
|      5 |  984 | `			sRep.pRepl = &sRepl;` |
|      5 |  985 | `			sRep.pReplPool = &sReplPool;` |
|      3 |  986 | `		}else{` |
|     11 |  987 | `			sRep.zRepl = zRepl;` |
|     11 |  988 | `			sRep.nRepl = nRepl;` |
|      - |  989 | `		}` |
|     15 |  990 | `		if( sCol.rc == SXRET_OK && ph7_value_is_array(apArg[2]) ){` |
|      7 |  991 | `			sCol.pSet = &sFrom;` |
|      7 |  992 | `			ph7_array_walk(apArg[2],SubstrReplaceIntWalker,&sCol);` |
|      7 |  993 | `			sRep.pFrom = &sFrom;` |
|      4 |  994 | `		}else{` |
|      9 |  995 | `			sRep.iFrom = f;` |
|      - |  996 | `		}` |
|     15 |  997 | `		if( sCol.rc == SXRET_OK && bLenGiven ){` |
|      9 |  998 | `			if( ph7_value_is_array(apArg[3]) ){` |
|      5 |  999 | `				sCol.pSet = &sLen;` |
|      5 | 1000 | `				ph7_array_walk(apArg[3],SubstrReplaceIntWalker,&sCol);` |
|      5 | 1001 | `				sRep.pLen = &sLen;` |
|      3 | 1002 | `			}else{` |
|      5 | 1003 | `				sRep.iLen = l;` |
|      - | 1004 | `			}` |
|      4 | 1005 | `		}` |
|     15 | 1006 | `		pResult = ph7_context_new_array(pCtx);` |
|     15 | 1007 | `		pScratch = ph7_context_new_scalar(pCtx);` |
|     15 | 1008 | `		if( sCol.rc != SXRET_OK \|\| pResult == 0 \|\| pScratch == 0 ){` |
|    ! 0 | 1009 | `			rcWalk = SXERR_MEM;` |
|    ! 0 | 1010 | `		}else{` |
|     15 | 1011 | `			sRep.pResult = pResult;` |
|     15 | 1012 | `			sRep.pScratch = pScratch;` |
|     15 | 1013 | `			ph7_value_string(pScratch,"",0); /* Force string representation */` |
|     15 | 1014 | `			ph7_array_walk(apArg[0],SubstrReplaceElemWalker,&sRep);` |
|     15 | 1015 | `			rcWalk = sRep.rc;` |
|      - | 1016 | `		}` |
|     15 | 1017 | `		SyBlobRelease(&sReplPool);` |
|     15 | 1018 | `		SySetRelease(&sRepl);` |
|     15 | 1019 | `		SySetRelease(&sFrom);` |
|     15 | 1020 | `		SySetRelease(&sLen);` |
|     15 | 1021 | `		if( rcWalk != SXRET_OK ){` |
|    ! 0 | 1022 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1023 | `			goto out;` |
|      - | 1024 | `		}` |
|     15 | 1025 | `		ph7_result_value(pCtx,pResult);` |
|     15 | 1026 | `		rc = PH7_OK;` |
|     15 | 1027 | `		goto out;` |
|      - | 1028 | `	}` |
|      - | 1029 | `	/* Scalar form: array $offset/$length are a TypeError, array $replace` |
|      - | 1030 | `	 * degrades to its first element (php quirk). */` |
|     41 | 1031 | `	if( ph7_value_is_array(apArg[2]) ){` |
|      3 | 1032 | `		rc = PH7_VmThrowException(pCtx,` |
|      - | 1033 | `			"TypeError",` |
|      - | 1034 | `			"substr_replace(): Argument #3 ($offset) cannot be an array when working on a single string"` |
|      - | 1035 | `			);` |
|      3 | 1036 | `		goto out;` |
|      - | 1037 | `	}` |
|     39 | 1038 | `	if( bLenGiven && ph7_value_is_array(apArg[3]) ){` |
|      3 | 1039 | `		rc = PH7_VmThrowException(pCtx,` |
|      - | 1040 | `			"TypeError",` |
|      - | 1041 | `			"substr_replace(): Argument #4 ($length) cannot be an array when working on a single string"` |
|      - | 1042 | `			);` |
|      3 | 1043 | `		goto out;` |
|      - | 1044 | `	}` |
|     37 | 1045 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 1046 | `		/* First element of the replace array, or "" when empty */` |
|      5 | 1047 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      5 | 1048 | `		zRepl = "";` |
|      5 | 1049 | `		nRepl = 0;` |
|      5 | 1050 | `		if( pMap->pFirst ){` |
|      3 | 1051 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pMap->pFirst->nValIdx);` |
|      3 | 1052 | `			if( pVal ){` |
|      3 | 1053 | `				zRepl = ph7_value_to_string(pVal,&nRepl);` |
|      1 | 1054 | `			}` |
|      1 | 1055 | `		}` |
|      2 | 1056 | `	}` |
|     37 | 1057 | `	if( !bLenGiven ){` |
|     15 | 1058 | `		l = nLen;` |
|      7 | 1059 | `	}` |
|     37 | 1060 | `	SubstrReplaceWindow(&f,&l,nLen);` |
|      - | 1061 | `	/* Assemble prefix + replacement + suffix straight into the call result` |
|      - | 1062 | `	 * (ph7_result_string appends), no scratch buffer needed. */` |
|     37 | 1063 | `	rc = SXRET_OK;` |
|     37 | 1064 | `	if( f > 0 ){` |
|     29 | 1065 | `		rc = ph7_result_string(pCtx,zStr,(int)f);` |
|     14 | 1066 | `	}` |
|     37 | 1067 | `	if( rc == SXRET_OK && nRepl > 0 ){` |
|     33 | 1068 | `		rc = ph7_result_string(pCtx,zRepl,nRepl);` |
|     16 | 1069 | `	}` |
|     37 | 1070 | `	if( rc == SXRET_OK && nLen - (int)(f+l) > 0 ){` |
|     17 | 1071 | `		rc = ph7_result_string(pCtx,&zStr[f+l],nLen - (int)(f+l));` |
|      8 | 1072 | `	}` |
|     37 | 1073 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 1074 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1075 | `		goto out;` |
|      - | 1076 | `	}` |
|      - | 1077 | `	/* Force a string result even when all three segments are empty */` |
|     37 | 1078 | `	rc = ph7_result_string(pCtx,"",0);` |
|     37 | 1079 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 1080 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1081 | `		goto out;` |
|      - | 1082 | `	}` |
|     37 | 1083 | `	rc = PH7_OK;` |
|     29 | 1084 | `out:` |
|     59 | 1085 | `	PH7_MemObjRelease(&sStrTmp);` |
|     59 | 1086 | `	PH7_MemObjRelease(&sReplTmp);` |
|     59 | 1087 | `	return rc;` |
|     30 | 1088 | `}` |
|      - | 1089 | `/*` |
|      - | 1090 | ` * int levenshtein(string $string1,string $string2[,int $insertion_cost = 1[,int $replacement_cost = 1[,int $deletion_cost = 1]]])` |
|      - | 1091 | ` *  Calculate the Levenshtein distance between two strings, byte per byte` |
|      - | 1092 | ` *  (case-sensitive), with optional per-operation costs. Mirrors PHP's` |
|      - | 1093 | ` *  reference_levdist(): two rolling rows over string2.` |
|      - | 1094 | ` * Return` |
|      - | 1095 | ` *  The minimal number of weighted edit operations turning $string1 into` |
|      - | 1096 | ` *  $string2.` |
|      - | 1097 | ` */` |
|     34 | 1098 | `static int PH7_builtin_levenshtein(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1099 | `{` |
|      - | 1100 | `	static const char *azParam[] = { "$insertion_cost","$replacement_cost","$deletion_cost" };` |
|      - | 1101 | `	const char *zStr1,*zStr2;` |
|     35 | 1102 | `	sxi64 iCostIns = 1,iCostRep = 1,iCostDel = 1;` |
|      - | 1103 | `	sxi64 *p1,*p2,*pTmp;` |
|      - | 1104 | `	sxi64 c0,c1,c2;` |
|      - | 1105 | `	ph7_value sTmp1,sTmp2;` |
|      - | 1106 | `	int nLen1,nLen2;` |
|      - | 1107 | `	int i1,i2;` |
|      - | 1108 | `	sxi32 rc;` |
|      - | 1109 | `	int i;` |
|     35 | 1110 | `	if( nArg < 2 ){` |
|    ! 0 | 1111 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1112 | `			"ArgumentCountError",` |
|      - | 1113 | `			"levenshtein() expects at least 2 arguments, %d given",` |
|    ! 0 | 1114 | `			nArg` |
|      - | 1115 | `			);` |
|      - | 1116 | `	}` |
|      - | 1117 | `	/* $string1/$string2: null deprecates to "", __toString objects resolve,` |
|      - | 1118 | `	 * everything non-stringish is a TypeError (php ZPP weak mode). */` |
|     35 | 1119 | `	PH7_MemObjInit(pCtx->pVm,&sTmp1);` |
|     35 | 1120 | `	PH7_MemObjInit(pCtx->pVm,&sTmp2);` |
|     35 | 1121 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"levenshtein",1,"$string1","string",` |
|      - | 1122 | `		"levenshtein(): Passing null to parameter #1 ($string1) "` |
|      - | 1123 | `		"of type string is deprecated",` |
|      - | 1124 | `		&sTmp1,&zStr1,&nLen1);` |
|     35 | 1125 | `	if( rc != PH7_OK ) goto out;` |
|     35 | 1126 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"levenshtein",2,"$string2","string",` |
|      - | 1127 | `		"levenshtein(): Passing null to parameter #2 ($string2) "` |
|      - | 1128 | `		"of type string is deprecated",` |
|      - | 1129 | `		&sTmp2,&zStr2,&nLen2);` |
|     35 | 1130 | `	if( rc != PH7_OK ) goto out;` |
|      - | 1131 | `	/* Optional integer costs */` |
|     57 | 1132 | `	for( i = 2 ; i < nArg && i < 5 ; i++ ){` |
|      - | 1133 | `		sxi64 iVal;` |
|     31 | 1134 | `		rc = PH7_IntArgResolve(pCtx,apArg[i],"levenshtein",i+1,azParam[i-2],"int",&iVal);` |
|     31 | 1135 | `		if( rc != PH7_OK ) goto out;` |
|     23 | 1136 | `		if( i == 2 ){` |
|     11 | 1137 | `			iCostIns = iVal;` |
|     18 | 1138 | `		}else if( i == 3 ){` |
|      7 | 1139 | `			iCostRep = iVal;` |
|      4 | 1140 | `		}else{` |
|      7 | 1141 | `			iCostDel = iVal;` |
|      - | 1142 | `		}` |
|     12 | 1143 | `	}` |
|     27 | 1144 | `	if( nLen1 == 0 ){` |
|      3 | 1145 | `		ph7_result_int64(pCtx,(sxi64)nLen2 * iCostIns);` |
|      3 | 1146 | `		rc = PH7_OK;` |
|      3 | 1147 | `		goto out;` |
|      - | 1148 | `	}` |
|     25 | 1149 | `	if( nLen2 == 0 ){` |
|      3 | 1150 | `		ph7_result_int64(pCtx,(sxi64)nLen1 * iCostDel);` |
|      3 | 1151 | `		rc = PH7_OK;` |
|      3 | 1152 | `		goto out;` |
|      - | 1153 | `	}` |
|      - | 1154 | `	/* Two rolling DP rows over string2 (auto-released on return). Reject a` |
|      - | 1155 | `	 * string2 long enough to overflow the 32-bit allocation size. */` |
|     23 | 1156 | `	if( (sxu32)nLen2 >= (SXU32_HIGH / sizeof(sxi64)) - 1 ){` |
|    ! 0 | 1157 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1158 | `		goto out;` |
|      - | 1159 | `	}` |
|     23 | 1160 | `	p1 = (sxi64 *)ph7_context_alloc_chunk(pCtx,(unsigned int)(sizeof(sxi64) * (sxu32)(nLen2 + 1)),FALSE,TRUE);` |
|     23 | 1161 | `	p2 = (sxi64 *)ph7_context_alloc_chunk(pCtx,(unsigned int)(sizeof(sxi64) * (sxu32)(nLen2 + 1)),FALSE,TRUE);` |
|     23 | 1162 | `	if( p1 == 0 \|\| p2 == 0 ){` |
|    ! 0 | 1163 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1164 | `		goto out;` |
|      - | 1165 | `	}` |
|    733 | 1166 | `	for( i2 = 0 ; i2 <= nLen2 ; i2++ ){` |
|    711 | 1167 | `		p1[i2] = (sxi64)i2 * iCostIns;` |
|    356 | 1168 | `	}` |
|    707 | 1169 | `	for( i1 = 0 ; i1 < nLen1 ; i1++ ){` |
|    685 | 1170 | `		p2[0] = p1[0] + iCostDel;` |
| 181111 | 1171 | `		for( i2 = 0 ; i2 < nLen2 ; i2++ ){` |
| 180427 | 1172 | `			c0 = p1[i2] + ((zStr1[i1] == zStr2[i2]) ? 0 : iCostRep);` |
| 180427 | 1173 | `			c1 = p1[i2 + 1] + iCostDel;` |
| 180427 | 1174 | `			if( c1 < c0 ){` |
|  45393 | 1175 | `				c0 = c1;` |
|  22696 | 1176 | `			}` |
| 180427 | 1177 | `			c2 = p2[i2] + iCostIns;` |
| 180427 | 1178 | `			if( c2 < c0 ){` |
|  44809 | 1179 | `				c0 = c2;` |
|  22404 | 1180 | `			}` |
| 180427 | 1181 | `			p2[i2 + 1] = c0;` |
|  90214 | 1182 | `		}` |
|    685 | 1183 | `		pTmp = p1;` |
|    685 | 1184 | `		p1 = p2;` |
|    685 | 1185 | `		p2 = pTmp;` |
|    343 | 1186 | `	}` |
|     23 | 1187 | `	ph7_result_int64(pCtx,p1[nLen2]);` |
|     23 | 1188 | `	rc = PH7_OK;` |
|     17 | 1189 | `out:` |
|     35 | 1190 | `	PH7_MemObjRelease(&sTmp1);` |
|     35 | 1191 | `	PH7_MemObjRelease(&sTmp2);` |
|     35 | 1192 | `	return rc;` |
|     18 | 1193 | `}` |
|      - | 1194 | `/*` |
|      - | 1195 | ` * Longest common substring scan behind similar_text() — a faithful port of` |
|      - | 1196 | ` * PHP's php_similar_str(): O(n*m) scan recording the first longest run.` |
|      - | 1197 | ` */` |
|     26 | 1198 | `static void SimilarStr(const char *zTxt1,int nLen1,const char *zTxt2,int nLen2,` |
|      - | 1199 | `	int *pPos1,int *pPos2,int *pMax,int *pCount)` |
|      1 | 1200 | `{` |
|      - | 1201 | `	const char *p,*q;` |
|     27 | 1202 | `	const char *zEnd1 = &zTxt1[nLen1];` |
|     27 | 1203 | `	const char *zEnd2 = &zTxt2[nLen2];` |
|      - | 1204 | `	int l;` |
|     27 | 1205 | `	*pMax = 0;` |
|     27 | 1206 | `	*pCount = 0;` |
|    143 | 1207 | `	for( p = zTxt1 ; p < zEnd1 ; p++ ){` |
|    843 | 1208 | `		for( q = zTxt2 ; q < zEnd2 ; q++ ){` |
|    999 | 1209 | `			for( l = 0 ; (p+l < zEnd1) && (q+l < zEnd2) && (p[l] == q[l]) ; l++ );` |
|    727 | 1210 | `			if( l > *pMax ){` |
|     25 | 1211 | `				*pMax = l;` |
|     25 | 1212 | `				*pCount += 1;` |
|     25 | 1213 | `				*pPos1 = (int)(p - zTxt1);` |
|     25 | 1214 | `				*pPos2 = (int)(q - zTxt2);` |
|     12 | 1215 | `			}` |
|    364 | 1216 | `		}` |
|     59 | 1217 | `	}` |
|     27 | 1218 | `}` |
|      - | 1219 | `/*` |
|      - | 1220 | ` * Recursive divide-and-conquer behind similar_text() — a faithful port of` |
|      - | 1221 | `` * PHP's php_similar_char(), including its quirky `count > 1` guard on the`` |
|      - | 1222 | ` * left-side recursion.` |
|      - | 1223 | ` */` |
|     26 | 1224 | `static int SimilarChar(const char *zTxt1,int nLen1,const char *zTxt2,int nLen2)` |
|      1 | 1225 | `{` |
|      - | 1226 | `	int nSum;` |
|     27 | 1227 | `	int nPos1 = 0,nPos2 = 0,nMax,nCount;` |
|     27 | 1228 | `	SimilarStr(zTxt1,nLen1,zTxt2,nLen2,&nPos1,&nPos2,&nMax,&nCount);` |
|     27 | 1229 | `	if( (nSum = nMax) != 0 ){` |
|     25 | 1230 | `		if( nPos1 && nPos2 && nCount > 1 ){` |
|    ! 0 | 1231 | `			nSum += SimilarChar(zTxt1,nPos1,zTxt2,nPos2);` |
|    ! 0 | 1232 | `		}` |
|     25 | 1233 | `		if( (nPos1 + nMax < nLen1) && (nPos2 + nMax < nLen2) ){` |
|     13 | 1234 | `			nSum += SimilarChar(&zTxt1[nPos1 + nMax],nLen1 - nPos1 - nMax,` |
|      8 | 1235 | `				&zTxt2[nPos2 + nMax],nLen2 - nPos2 - nMax);` |
|      4 | 1236 | `		}` |
|     12 | 1237 | `	}` |
|     27 | 1238 | `	return nSum;` |
|      1 | 1239 | `}` |
|      - | 1240 | `/*` |
|      - | 1241 | ` * int similar_text(string $string1,string $string2[,float &$percent])` |
|      - | 1242 | ` *  Calculate the similarity between two strings, as the number of matching` |
|      - | 1243 | ` *  characters found by PHP's greedy longest-common-substring recursion.` |
|      - | 1244 | ` *  When $percent is given it receives the similarity in percent:` |
|      - | 1245 | ` *  matching * 200 / (len1 + len2).` |
|      - | 1246 | ` * Return` |
|      - | 1247 | ` *  The number of matching characters in both strings.` |
|      - | 1248 | ` */` |
|     22 | 1249 | `static int PH7_builtin_similar_text(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1250 | `{` |
|      - | 1251 | `	const char *zStr1,*zStr2;` |
|      - | 1252 | `	ph7_value sTmp1,sTmp2;` |
|      - | 1253 | `	int nLen1,nLen2;` |
|      - | 1254 | `	int nSim;` |
|      - | 1255 | `	sxi32 rc;` |
|     23 | 1256 | `	if( nArg < 2 ){` |
|    ! 0 | 1257 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1258 | `			"ArgumentCountError",` |
|      - | 1259 | `			"similar_text() expects at least 2 arguments, %d given",` |
|    ! 0 | 1260 | `			nArg` |
|      - | 1261 | `			);` |
|      - | 1262 | `	}` |
|     23 | 1263 | `	PH7_MemObjInit(pCtx->pVm,&sTmp1);` |
|     23 | 1264 | `	PH7_MemObjInit(pCtx->pVm,&sTmp2);` |
|     23 | 1265 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"similar_text",1,"$string1","string",` |
|      - | 1266 | `		"similar_text(): Passing null to parameter #1 ($string1) "` |
|      - | 1267 | `		"of type string is deprecated",` |
|      - | 1268 | `		&sTmp1,&zStr1,&nLen1);` |
|     23 | 1269 | `	if( rc != PH7_OK ) goto out;` |
|     23 | 1270 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"similar_text",2,"$string2","string",` |
|      - | 1271 | `		"similar_text(): Passing null to parameter #2 ($string2) "` |
|      - | 1272 | `		"of type string is deprecated",` |
|      - | 1273 | `		&sTmp2,&zStr2,&nLen2);` |
|     23 | 1274 | `	if( rc != PH7_OK ) goto out;` |
|     23 | 1275 | `	if( nLen1 + nLen2 == 0 ){` |
|      5 | 1276 | `		nSim = 0;` |
|      3 | 1277 | `	}else{` |
|     19 | 1278 | `		nSim = SimilarChar(zStr1,nLen1,zStr2,nLen2);` |
|      - | 1279 | `	}` |
|     23 | 1280 | `	if( nArg > 2 ){` |
|      - | 1281 | `		/* Write the percentage through the by-ref out-param */` |
|      7 | 1282 | `		ph7_value *pPercent = ph7_context_new_scalar(pCtx);` |
|      7 | 1283 | `		if( pPercent == 0 ){` |
|    ! 0 | 1284 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1285 | `			goto out;` |
|    ! 0 | 1286 | `		}else{` |
|      7 | 1287 | `			double dPct = (nLen1 + nLen2 == 0) ? 0.0 : (double)nSim * 200.0 / (double)(nLen1 + nLen2);` |
|      7 | 1288 | `			ph7_value_double(pPercent,dPct);` |
|      7 | 1289 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[2],pPercent);` |
|      - | 1290 | `		}` |
|      3 | 1291 | `	}` |
|     23 | 1292 | `	ph7_result_int(pCtx,nSim);` |
|     23 | 1293 | `	rc = PH7_OK;` |
|     11 | 1294 | `out:` |
|     23 | 1295 | `	PH7_MemObjRelease(&sTmp1);` |
|     23 | 1296 | `	PH7_MemObjRelease(&sTmp2);` |
|     23 | 1297 | `	return rc;` |
|     12 | 1298 | `}` |
|      - | 1299 | `/*` |
|      - | 1300 | ` * array\|int str_word_count(string $string[,int $format = 0[,?string $characters = null]])` |
|      - | 1301 | ` *  Count (or return) the words inside a string. A word is a run of alphabetic` |
|      - | 1302 | ` *  characters, which may contain (but not start the string with) "'" and "-";` |
|      - | 1303 | ` *  $characters adds extra bytes to the word set ("a..z" ranges supported, as` |
|      - | 1304 | ` *  in PHP's php_charmask).` |
|      - | 1305 | ` *  $format: 0 -> word count, 1 -> array of words, 2 -> array of words keyed` |
|      - | 1306 | ` *  by their byte position in $string.` |
|      - | 1307 | ` * Errors` |
|      - | 1308 | ` *  ValueError when $format is not 0, 1 or 2.` |
|      - | 1309 | ` */` |
|     44 | 1310 | `static int PH7_builtin_str_word_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1311 | `{` |
|      - | 1312 | `	const char *zIn,*zEnd,*zPtr;` |
|     45 | 1313 | `	ph7_value *pArray = 0,*pValue = 0;` |
|      - | 1314 | `	ph7_value sTmp,sListTmp;` |
|      - | 1315 | `	char aMask[256];` |
|     45 | 1316 | `	int bMask = 0;` |
|     45 | 1317 | `	int iFormat = 0;` |
|     45 | 1318 | `	int nCount = 0;` |
|      - | 1319 | `	int nLen;` |
|      - | 1320 | `	sxi32 rc;` |
|     45 | 1321 | `	if( nArg < 1 ){` |
|    ! 0 | 1322 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1323 | `			"ArgumentCountError",` |
|      - | 1324 | `			"str_word_count() expects at least 1 argument, %d given",` |
|    ! 0 | 1325 | `			nArg` |
|      - | 1326 | `			);` |
|      - | 1327 | `	}` |
|     45 | 1328 | `	PH7_MemObjInit(pCtx->pVm,&sTmp);` |
|     45 | 1329 | `	PH7_MemObjInit(pCtx->pVm,&sListTmp);` |
|     45 | 1330 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_word_count",1,"$string","string",` |
|      - | 1331 | `		"str_word_count(): Passing null to parameter #1 ($string) "` |
|      - | 1332 | `		"of type string is deprecated",` |
|      - | 1333 | `		&sTmp,&zIn,&nLen);` |
|     45 | 1334 | `	if( rc != PH7_OK ) goto out;` |
|     45 | 1335 | `	if( nArg > 1 ){` |
|      - | 1336 | `		sxi64 iVal;` |
|     31 | 1337 | `		rc = PH7_IntArgResolve(pCtx,apArg[1],"str_word_count",2,"$format","int",&iVal);` |
|     33 | 1338 | `		if( rc != PH7_OK ) goto out;` |
|     29 | 1339 | `		if( iVal < 0 \|\| iVal > 2 ){` |
|      5 | 1340 | `			rc = PH7_VmThrowException(pCtx,` |
|      - | 1341 | `				"ValueError",` |
|      - | 1342 | `				"str_word_count(): Argument #2 ($format) must be a valid format value"` |
|      - | 1343 | `				);` |
|      5 | 1344 | `			goto out;` |
|      - | 1345 | `		}` |
|     25 | 1346 | `		iFormat = (int)iVal;` |
|     12 | 1347 | `	}` |
|     39 | 1348 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      - | 1349 | `		/* $characters is ?string: null (skipped above) simply keeps the` |
|      - | 1350 | `		 * default word set, no deprecation. */` |
|      - | 1351 | `		const char *zList;` |
|      - | 1352 | `		int nList;` |
|     13 | 1353 | `		rc = StrPredicateResolveArg(pCtx,apArg[2],"str_word_count",3,"$characters","?string",` |
|      - | 1354 | `			"" /* unreachable: null never gets here */,` |
|      - | 1355 | `			&sListTmp,&zList,&nList);` |
|     13 | 1356 | `		if( rc != PH7_OK ) goto out;` |
|     13 | 1357 | `		PH7_BuildCharMask(pCtx,zList,nList,aMask);` |
|     13 | 1358 | `		bMask = 1;` |
|      6 | 1359 | `	}` |
|     39 | 1360 | `	if( iFormat != 0 ){` |
|     25 | 1361 | `		pArray = ph7_context_new_array(pCtx);` |
|     25 | 1362 | `		pValue = ph7_context_new_scalar(pCtx);` |
|     25 | 1363 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1364 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1365 | `			goto out;` |
|      - | 1366 | `		}` |
|     12 | 1367 | `	}` |
|     39 | 1368 | `	zPtr = zIn;` |
|     39 | 1369 | `	zEnd = &zIn[nLen];` |
|     39 | 1370 | `	if( nLen > 0 ){` |
|      - | 1371 | `		/* php: the string's first byte cannot be ' or -, and its last byte` |
|      - | 1372 | `		 * cannot be -, unless the charlist explicitly allows them. */` |
|     33 | 1373 | `		if( (zPtr[0] == '\'' && (!bMask \|\| !aMask[(unsigned char)'\''])) \|\|` |
|     28 | 1374 | `			(zPtr[0] == '-'  && (!bMask \|\| !aMask[(unsigned char)'-'])) ){` |
|      9 | 1375 | `			zPtr++;` |
|      4 | 1376 | `		}` |
|     33 | 1377 | `		if( zEnd[-1] == '-' && (!bMask \|\| !aMask[(unsigned char)'-']) ){` |
|      9 | 1378 | `			zEnd--;` |
|      4 | 1379 | `		}` |
|     16 | 1380 | `	}` |
|    135 | 1381 | `	while( zPtr < zEnd ){` |
|     91 | 1382 | `		const char *zStart = zPtr;` |
|    477 | 1383 | `		while( zPtr < zEnd && ( SyisAlpha((unsigned char)zPtr[0])` |
|    253 | 1384 | `			\|\| (bMask && aMask[(unsigned char)zPtr[0]])` |
|     98 | 1385 | `			\|\| zPtr[0] == '\'' \|\| zPtr[0] == '-' ) ){` |
|    339 | 1386 | `			zPtr++;` |
|      1 | 1387 | `		}` |
|     97 | 1388 | `		if( zPtr > zStart ){` |
|     91 | 1389 | `			if( iFormat == 0 ){` |
|     19 | 1390 | `				nCount++;` |
|     10 | 1391 | `			}else{` |
|     73 | 1392 | `				ph7_value_reset_string_cursor(pValue);` |
|     73 | 1393 | `				if( SXRET_OK != ph7_value_string(pValue,zStart,(int)(zPtr-zStart)) ){` |
|    ! 0 | 1394 | `					rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1395 | `					goto out;` |
|      - | 1396 | `				}` |
|     73 | 1397 | `				if( iFormat == 1 ){` |
|     59 | 1398 | `					if( SXRET_OK != ph7_array_add_elem(pArray,0,pValue) ){` |
|    ! 0 | 1399 | `						rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1400 | `						goto out;` |
|      - | 1401 | `					}` |
|     30 | 1402 | `				}else{` |
|     15 | 1403 | `					if( SXRET_OK != ph7_array_add_intkey_elem(pArray,(int)(zStart-zIn),pValue) ){` |
|    ! 0 | 1404 | `						rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1405 | `						goto out;` |
|      - | 1406 | `					}` |
|      - | 1407 | `				}` |
|      - | 1408 | `			}` |
|     45 | 1409 | `		}` |
|     97 | 1410 | `		zPtr++;` |
|      1 | 1411 | `	}` |
|     37 | 1412 | `	if( iFormat == 0 ){` |
|     13 | 1413 | `		ph7_result_int(pCtx,nCount);` |
|      7 | 1414 | `	}else{` |
|     25 | 1415 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1416 | `	}` |
|     37 | 1417 | `	rc = PH7_OK;` |
|     21 | 1418 | `out:` |
|     43 | 1419 | `	PH7_MemObjRelease(&sTmp);` |
|     43 | 1420 | `	PH7_MemObjRelease(&sListTmp);` |
|     43 | 1421 | `	return rc;` |
|     22 | 1422 | `}` |
|      - | 1423 | `/*` |
|      - | 1424 | ` * string chunk_split(string $body[,int $chunklen = 76 [, string $end = "\r\n" ]])` |
|      - | 1425 | ` *   Split a string into smaller chunks.` |
|      - | 1426 | ` * Parameters` |
|      - | 1427 | ` *  $body` |
|      - | 1428 | ` *   The string to be chunked.` |
|      - | 1429 | ` * $chunklen` |
|      - | 1430 | ` *   The chunk length.` |
|      - | 1431 | ` * $end` |
|      - | 1432 | ` *   The line ending sequence.` |
|      - | 1433 | ` * Return` |
|      - | 1434 | ` *  The chunked string or NULL on failure.` |
|      - | 1435 | ` */` |
|     14 | 1436 | `static int PH7_builtin_chunk_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1437 | `{` |
|     15 | 1438 | `	const char *zIn,*zEnd,*zSep = "\r\n";` |
|      - | 1439 | `	int nSepLen,nChunkLen,nLen;` |
|      - | 1440 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1441 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     15 | 1442 | `	if( nArg < 1 ){` |
|      - | 1443 | `		/* Nothing to split,return null */` |
|    ! 0 | 1444 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1445 | `		return PH7_OK;` |
|      - | 1446 | `	}` |
|      - | 1447 | `	/* initialize/Extract arguments */` |
|     15 | 1448 | `	nSepLen = (int)sizeof("\r\n") - 1;` |
|     15 | 1449 | `	nChunkLen = 76;` |
|     15 | 1450 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 1451 | `	zEnd = &zIn[nLen];` |
|     15 | 1452 | `	if( nArg > 1 ){` |
|      - | 1453 | `		/* Chunk length */` |
|     13 | 1454 | `		nChunkLen = ph7_value_to_int(apArg[1]);` |
|     13 | 1455 | `		if( nChunkLen < 1 ){` |
|      - | 1456 | `			/* PHP 8 throws a catchable ValueError for a non-positive length. */` |
|      3 | 1457 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1458 | `				"chunk_split(): Argument #2 ($length) must be greater than 0");` |
|      - | 1459 | `		}` |
|     11 | 1460 | `		if( nArg > 2 ){` |
|      - | 1461 | `			/* Separator */` |
|      9 | 1462 | `			zSep = ph7_value_to_string(apArg[2],&nSepLen);` |
|      9 | 1463 | `			if( nSepLen < 1 ){` |
|      - | 1464 | `				/* Switch back to the default separator */` |
|      3 | 1465 | `				zSep = "\r\n";` |
|      3 | 1466 | `				nSepLen = (int)sizeof("\r\n") - 1;` |
|      1 | 1467 | `			}` |
|      4 | 1468 | `		}` |
|      5 | 1469 | `	}` |
|      - | 1470 | `	/* Perform the requested operation */` |
|     13 | 1471 | `	if( nChunkLen > nLen ){` |
|      - | 1472 | `		/* Nothing to split,return the string and the separator */` |
|      9 | 1473 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nLen,zIn,nSepLen,zSep);` |
|      9 | 1474 | `		return PH7_OK;` |
|      - | 1475 | `	}` |
|     17 | 1476 | `	while( zIn < zEnd ){` |
|     13 | 1477 | `		if( nChunkLen > (int)(zEnd-zIn) ){` |
|      3 | 1478 | `			nChunkLen = (int)(zEnd - zIn);` |
|      1 | 1479 | `		}` |
|      - | 1480 | `		/* Append the chunk and the separator */` |
|     13 | 1481 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nChunkLen,zIn,nSepLen,zSep);` |
|      - | 1482 | `		/* Point beyond the chunk */` |
|     13 | 1483 | `		zIn += nChunkLen;` |
|      1 | 1484 | `	}` |
|      5 | 1485 | `	return PH7_OK;` |
|      8 | 1486 | `}` |
|      - | 1487 | `/*` |
|      - | 1488 | ` * string addslashes(string $str)` |
|      - | 1489 | ` *  Quote string with slashes.` |
|      - | 1490 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1491 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1492 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1493 | ` * Parameter` |
|      - | 1494 | ` *  str: The string to be escaped.` |
|      - | 1495 | ` * Return` |
|      - | 1496 | ` *  Returns the escaped string` |
|      - | 1497 | ` */` |
|     20 | 1498 | `static int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1499 | `{` |
|      - | 1500 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1501 | `	int nLen;` |
|      - | 1502 | `	/* PHP enforces exactly one argument. */` |
|     22 | 1503 | `	if( nArg != 1 ){` |
|      4 | 1504 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1505 | `			"ArgumentCountError",` |
|      - | 1506 | `			"addslashes() expects exactly 1 argument, %d given",` |
|      1 | 1507 | `			nArg` |
|      - | 1508 | `			);` |
|      - | 1509 | `	}` |
|      - | 1510 | `	/* NULL is deprecated and treated as an empty string; other invalid` |
|      - | 1511 | `	 * types still produce a TypeError. */` |
|     19 | 1512 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 1513 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 1514 | `			E_DEPRECATED,` |
|      - | 1515 | `			"addslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - | 1516 | `			);` |
|      - | 1517 | `		/* fall through so conversion below yields empty string */` |
|      1 | 1518 | `	}` |
|      - | 1519 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     27 | 1520 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     28 | 1521 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     18 | 1522 | `	    ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 1523 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1524 | `			"TypeError",` |
|      - | 1525 | `			"addslashes(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 1526 | `			ph7_type_name(apArg[0])` |
|      - | 1527 | `			);` |
|      - | 1528 | `	}` |
|      - | 1529 | `	/* Convert to string representation first and obtain length. */` |
|     19 | 1530 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 1531 | `	if( nLen < 1 ){` |
|      - | 1532 | `		/* Return the empty string */` |
|      5 | 1533 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 1534 | `		return PH7_OK;` |
|      - | 1535 | `	}` |
|     15 | 1536 | `	zEnd = &zIn[nLen];` |
|     15 | 1537 | `	zCur = 0; /* cc warning */` |
|     20 | 1538 | `	for(;;){` |
|     41 | 1539 | `		if( zIn >= zEnd ){` |
|      - | 1540 | `			/* No more input */` |
|     15 | 1541 | `			break;` |
|      - | 1542 | `		}` |
|     27 | 1543 | `		zCur = zIn;` |
|      - | 1544 | `		/* scan until a character that needs escaping (', ", \\, or NUL) */` |
|     89 | 1545 | `		while( zIn < zEnd && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '\\' && zIn[0] != '\0' ){` |
|     63 | 1546 | `			zIn++;` |
|      1 | 1547 | `		}` |
|     27 | 1548 | `		if( zIn > zCur ){` |
|      - | 1549 | `			/* Append raw contents */` |
|     23 | 1550 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 1551 | `		}` |
|     27 | 1552 | `		if( zIn < zEnd ){` |
|     17 | 1553 | `			int c = zIn[0];` |
|     17 | 1554 | `			if( c == '\0' ){` |
|      - | 1555 | `				/* PHP escapes NUL as "\\0" (two characters) */` |
|      3 | 1556 | `				ph7_result_string(pCtx,"\\0",2);` |
|      2 | 1557 | `			}else{` |
|     15 | 1558 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - | 1559 | `			}` |
|      8 | 1560 | `		}` |
|     27 | 1561 | `		zIn++;` |
|      1 | 1562 | `	}` |
|     15 | 1563 | `	return PH7_OK;` |
|     12 | 1564 | `}` |
|      - | 1565 | `/*` |
|      - | 1566 | ``  * Build a 256-entry membership mask from a PHP charlist, expanding `a..z` `` |
|      - | 1567 | ` * byte ranges exactly like PHP's php_charmask(). On return aMask[c] != 0 iff` |
|      - | 1568 | ` * the byte c belongs to the set. Emits the PHP-exact warnings for the three` |
|      - | 1569 | ` * malformed-range shapes (ph7_context_throw_error_format prepends the active` |
|      - | 1570 | ` * function name, so the messages omit it); on a bad range the surrounding` |
|      - | 1571 | ` * bytes are still added and the scan never aborts. Reads only within` |
|      - | 1572 | ` * [zList, zList+nLen).` |
|      - | 1573 | ` *` |
|      - | 1574 | ` * Use ONLY for the builtins whose charlist expands ranges the way PHP's` |
|      - | 1575 | ` * php_charmask() does: trim/ltrim/rtrim/addcslashes (and quotemeta, whose set` |
|      - | 1576 | ` * is a fixed literal with no ".."). Do NOT route strspn/strcspn/strtok/strpbrk` |
|      - | 1577 | ` * through this — PHP treats their charlists literally, so expanding "a..z" here` |
|      - | 1578 | ` * would be a behavior regression plus spurious "Invalid '..'-range" warnings.` |
|      - | 1579 | ` */` |
|    234 | 1580 | `static void PH7_BuildCharMask(ph7_context *pCtx,const char *zList,int nLen,char aMask[256])` |
|      5 | 1581 | `{` |
|    239 | 1582 | `	const unsigned char *zIn  = (const unsigned char *)zList;` |
|    239 | 1583 | `	const unsigned char *zEnd = zIn + (nLen > 0 ? nLen : 0);` |
|    239 | 1584 | `	SyZero(aMask,256);` |
|    635 | 1585 | `	for( ; zIn < zEnd ; zIn++ ){` |
|    401 | 1586 | `		int c = zIn[0];` |
|    401 | 1587 | `		if( zIn + 3 < zEnd && zIn[1] == '.' && zIn[2] == '.' && zIn[3] >= c ){` |
|      - | 1588 | `			/* Valid incrementing range c..zIn[3] */` |
|     22 | 1589 | `			int hi = zIn[3],k;` |
|    386 | 1590 | `			for( k = c ; k <= hi ; k++ ){` |
|    366 | 1591 | `				aMask[k] = 1;` |
|    184 | 1592 | `			}` |
|     22 | 1593 | `			zIn += 3; /* the loop's ++ then steps past the range end */` |
|    400 | 1594 | `		}else if( zIn + 1 < zEnd && zIn[0] == '.' && zIn[1] == '.' ){` |
|      - | 1595 | `			/* Malformed range: mirror php_charmask's three diagnostics. */` |
|      - | 1596 | `			const char *zMsg;` |
|     20 | 1597 | `			if( (const unsigned char *)zList >= zIn ){` |
|      6 | 1598 | `				zMsg = "no character to the left of '..'";` |
|     18 | 1599 | `			}else if( zIn + 2 >= zEnd ){` |
|      6 | 1600 | `				zMsg = "no character to the right of '..'";` |
|     14 | 1601 | `			}else if( zIn[-1] > zIn[2] ){` |
|     12 | 1602 | `				zMsg = "'..'-range needs to be incrementing";` |
|      7 | 1603 | `			}else{` |
|    ! 0 | 1604 | `				zMsg = 0; /* catch-all (e.g. a..b..c) */` |
|      - | 1605 | `			}` |
|     20 | 1606 | `			if( zMsg ){` |
|     29 | 1607 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      9 | 1608 | `					"Invalid '..'-range, %s",zMsg);` |
|     11 | 1609 | `			}else{` |
|    ! 0 | 1610 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1611 | `					"Invalid '..'-range");` |
|      - | 1612 | `			}` |
|      - | 1613 | `			/* Do not consume the dots: the loop's ++ steps one byte so the` |
|      - | 1614 | `			 * dots are re-scanned as literals, exactly like php_charmask. */` |
|     11 | 1615 | `		}else{` |
|    363 | 1616 | `			aMask[c] = 1;` |
|      - | 1617 | `		}` |
|    203 | 1618 | `	}` |
|    239 | 1619 | `}` |
|      - | 1620 | `/*` |
|      - | 1621 | ` * string addcslashes(string $str,string $charlist)` |
|      - | 1622 | ` *  Quote string with slashes in a C style.` |
|      - | 1623 | ` * Parameter` |
|      - | 1624 | ` *  $str:` |
|      - | 1625 | ` *    The string to be escaped.` |
|      - | 1626 | ` *  $charlist:` |
|      - | 1627 | ` *    A list of characters to be escaped. If charlist contains characters \n, \r etc.` |
|      - | 1628 | ` *    they are converted in C-like style, while other non-alphanumeric characters` |
|      - | 1629 | ` *    with ASCII codes lower than 32 and higher than 126 converted to octal representation.` |
|      - | 1630 | ` * Return` |
|      - | 1631 | ` *  Returns the escaped string.` |
|      - | 1632 | ` * Note:` |
|      - | 1633 | ` *  Character ranges [i.e: 'A..Z'] are supported (see PH7_BuildCharMask).` |
|      - | 1634 | ` */` |
|     34 | 1635 | `static int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1636 | `{` |
|      - | 1637 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|      - | 1638 | `	char aMask[256];` |
|      - | 1639 | `	int nLen,nMask;` |
|      - | 1640 | `	/* PHP enforces exactly two arguments. */` |
|     37 | 1641 | `	if( nArg != 2 ){` |
|      4 | 1642 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1643 | `			"ArgumentCountError",` |
|      - | 1644 | `			"addcslashes() expects exactly 2 arguments, %d given",` |
|      1 | 1645 | `			nArg` |
|      - | 1646 | `			);` |
|      - | 1647 | `	}` |
|      - | 1648 | `	/* First argument must be a string-ish value.  NULL is deprecated and` |
|      - | 1649 | `	 * treated as the empty string (PHP 8.1). */` |
|     35 | 1650 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      - | 1651 | `		/* Emit deprecation only once, similar to PHP behaviour. */` |
|      3 | 1652 | `		PH7_VmThrowError(pCtx->pVm,0,/* iErr will be patched to 8192 below */` |
|      - | 1653 | `			E_DEPRECATED,` |
|      - | 1654 | `			"addcslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - | 1655 | `			);` |
|      - | 1656 | `		/* treat as empty string; fall through to conversion logic */` |
|     47 | 1657 | `	} else if( ph7_value_is_array(apArg[0]) \|\|` |
|     48 | 1658 | `	          ph7_value_is_object(apArg[0]) \|\|` |
|     30 | 1659 | `	          ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 1660 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1661 | `			"TypeError",` |
|      - | 1662 | `			"addcslashes(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 1663 | `			ph7_type_name(apArg[0])` |
|      - | 1664 | `			);` |
|      - | 1665 | `	}` |
|      - | 1666 | `	/* Second argument must be a string.  NULL is deprecated and treated as` |
|      - | 1667 | `	 * an empty mask per PHP semantics.  Arrays/objects/resources still` |
|      - | 1668 | `	 * trigger a TypeError. */` |
|     35 | 1669 | `	if( ph7_value_is_null(apArg[1]) ){` |
|      3 | 1670 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 1671 | `			E_DEPRECATED,` |
|      - | 1672 | `			"addcslashes(): Passing null to parameter #2 ($characters) of type string is deprecated"` |
|      - | 1673 | `			);` |
|      - | 1674 | `		/* allow through so it becomes empty string below */` |
|     47 | 1675 | `	} else if( ph7_value_is_array(apArg[1]) \|\|` |
|     48 | 1676 | `	          ph7_value_is_object(apArg[1]) \|\|` |
|     30 | 1677 | `	          ph7_value_is_resource(apArg[1]) ){` |
|    ! 0 | 1678 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1679 | `			"TypeError",` |
|      - | 1680 | `			"addcslashes(): Argument #2 ($characters) must be of type string, %s given",` |
|    ! 0 | 1681 | `			ph7_type_name(apArg[1])` |
|      - | 1682 | `			);` |
|      - | 1683 | `	}` |
|      - | 1684 | `	/* Extract the string to process */` |
|     35 | 1685 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1686 | `	/* NULL would never reach here due to the check above. */` |
|     35 | 1687 | `	if( nLen < 1 ){` |
|      - | 1688 | `		/* Empty string returns itself. */` |
|      5 | 1689 | `		ph7_result_string(pCtx,zIn,nLen);` |
|      5 | 1690 | `		return PH7_OK;` |
|      - | 1691 | `	}` |
|      - | 1692 | ``	/* Extract the desired mask and expand any `a..z` ranges into a lookup. */`` |
|     31 | 1693 | `	zMask = ph7_value_to_string(apArg[1],&nMask);` |
|     31 | 1694 | `	PH7_BuildCharMask(pCtx,zMask,nMask,aMask);` |
|     31 | 1695 | `	zEnd = &zIn[nLen];` |
|     31 | 1696 | `	zCur = 0; /* cc warning */` |
|     37 | 1697 | `	for(;;){` |
|     77 | 1698 | `		if( zIn >= zEnd ){` |
|      - | 1699 | `			/* No more input */` |
|     31 | 1700 | `			break;` |
|      - | 1701 | `		}` |
|     49 | 1702 | `		zCur = zIn;` |
|    125 | 1703 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|     79 | 1704 | `			zIn++;` |
|      3 | 1705 | `		}` |
|     49 | 1706 | `		if( zIn > zCur ){` |
|      - | 1707 | `			/* Append raw contents */` |
|     43 | 1708 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     20 | 1709 | `		}` |
|     49 | 1710 | `		if( zIn < zEnd ){` |
|      - | 1711 | `			/* Make sure we treat the byte as unsigned to avoid negative values` |
|      - | 1712 | `			 * on platforms where char is signed. */` |
|     29 | 1713 | `			int c = (unsigned char)zIn[0];` |
|      - | 1714 | `			/* Handle special C-like escapes for common control characters first.` |
|      - | 1715 | `			 * PHP outputs "\n" "\r" "\t" "\v" "\f" when those chars are` |
|      - | 1716 | `			 * in the mask. NUL is left to the octal conversion below. */` |
|     29 | 1717 | `			if( c == '\n' ){` |
|      3 | 1718 | `				ph7_result_string(pCtx,"\\n",2);` |
|     28 | 1719 | `			}else if( c == '\r' ){` |
|      3 | 1720 | `				ph7_result_string(pCtx,"\\r",2);` |
|     26 | 1721 | `			}else if( c == '\t' ){` |
|      3 | 1722 | `				ph7_result_string(pCtx,"\\t",2);` |
|     24 | 1723 | `			}else if( c == '\v' ){` |
|      3 | 1724 | `				ph7_result_string(pCtx,"\\v",2);` |
|     22 | 1725 | `			}else if( c == '\f' ){` |
|      3 | 1726 | `				ph7_result_string(pCtx,"\\f",2);` |
|     20 | 1727 | `			}else if( c > 126 \|\| (c < 32 && (!SyisAlphaNum(c)/*EBCDIC*/ && !SyisSpace(c))) ){` |
|      - | 1728 | `				/* Convert to octal.  PHP always emits three-digit zero-padded` |
|      - | 1729 | `				 * octal escapes (\001 not \1). */` |
|      7 | 1730 | `				ph7_result_string_format(pCtx,"\\%03o",c);` |
|      4 | 1731 | `			}else{` |
|     13 | 1732 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - | 1733 | `			}` |
|     13 | 1734 | `		}` |
|     49 | 1735 | `		zIn++;` |
|      3 | 1736 | `	}` |
|     31 | 1737 | `	return PH7_OK;` |
|     20 | 1738 | `}` |
|      - | 1739 | `/*` |
|      - | 1740 | ` * string quotemeta(string $str)` |
|      - | 1741 | ` *  Quote meta characters.` |
|      - | 1742 | ` * Parameter` |
|      - | 1743 | ` *  $str:` |
|      - | 1744 | ` *    The string to be escaped.` |
|      - | 1745 | ` * Return` |
|      - | 1746 | ` *  Returns the escaped string.` |
|      - | 1747 | `*/` |
|     10 | 1748 | `static int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1749 | `{` |
|      - | 1750 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1751 | `	char aMask[256];` |
|      - | 1752 | `	int nLen;` |
|     12 | 1753 | `	if( nArg < 1 ){` |
|      - | 1754 | `		/* Nothing to process,retun NULL */` |
|    ! 0 | 1755 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1756 | `		return PH7_OK;` |
|      - | 1757 | `	}` |
|      - | 1758 | `	/* Extract the string to process */` |
|     12 | 1759 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     12 | 1760 | `	if( nLen < 1 ){` |
|      - | 1761 | `		/* Return the empty string */` |
|      3 | 1762 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1763 | `		return PH7_OK;` |
|      - | 1764 | `	}` |
|      - | 1765 | `	/* Fixed meta-character set (no ranges); build the lookup once. */` |
|     10 | 1766 | `	PH7_BuildCharMask(pCtx,".\\+*?[^]($)",(int)sizeof(".\\+*?[^]($)")-1,aMask);` |
|     10 | 1767 | `	zEnd = &zIn[nLen];` |
|     10 | 1768 | `	zCur = 0; /* cc warning */` |
|     22 | 1769 | `	for(;;){` |
|     46 | 1770 | `		if( zIn >= zEnd ){` |
|      - | 1771 | `			/* No more input */` |
|     10 | 1772 | `			break;` |
|      - | 1773 | `		}` |
|     38 | 1774 | `		zCur = zIn;` |
|     76 | 1775 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|     40 | 1776 | `			zIn++;` |
|      2 | 1777 | `		}` |
|     38 | 1778 | `		if( zIn > zCur ){` |
|      - | 1779 | `			/* Append raw contents */` |
|     20 | 1780 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      9 | 1781 | `		}` |
|     38 | 1782 | `		if( zIn < zEnd ){` |
|     36 | 1783 | `			int c = zIn[0];` |
|     36 | 1784 | `			ph7_result_string_format(pCtx,"\\%c",c);` |
|     17 | 1785 | `		}` |
|     38 | 1786 | `		zIn++;` |
|      2 | 1787 | `	}` |
|     10 | 1788 | `	return PH7_OK;` |
|      7 | 1789 | `}` |
|      - | 1790 | `/*` |
|      - | 1791 | ` * string stripslashes(string $str)` |
|      - | 1792 | ` *  Un-quotes a quoted string.` |
|      - | 1793 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1794 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1795 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1796 | ` * Parameter` |
|      - | 1797 | ` *  $str` |
|      - | 1798 | ` *   The input string.` |
|      - | 1799 | ` * Return` |
|      - | 1800 | ` *  Returns a string with backslashes stripped off.` |
|      - | 1801 | ` */` |
|      6 | 1802 | `static int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1803 | `{` |
|      - | 1804 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1805 | `	int nLen;` |
|      7 | 1806 | `	if( nArg < 1 ){` |
|      - | 1807 | `		/* Nothing to process,retun NULL */` |
|    ! 0 | 1808 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1809 | `		return PH7_OK;` |
|      - | 1810 | `	}` |
|      - | 1811 | `	/* Extract the string to process */` |
|      7 | 1812 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 1813 | `	if( zIn == 0 ){` |
|    ! 0 | 1814 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1815 | `		return PH7_OK;` |
|      - | 1816 | `	}` |
|      7 | 1817 | `	zEnd = &zIn[nLen];` |
|      7 | 1818 | `	zCur = 0; /* cc warning */` |
|      - | 1819 | `	/* Encode the string */` |
|      4 | 1820 | `	for(;;){` |
|      9 | 1821 | `		if( zIn >= zEnd ){` |
|      - | 1822 | `			/* No more input */` |
|      5 | 1823 | `			break;` |
|      - | 1824 | `		}` |
|      5 | 1825 | `		zCur = zIn;` |
|     17 | 1826 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|     13 | 1827 | `			zIn++;` |
|      1 | 1828 | `		}` |
|      5 | 1829 | `		if( zIn > zCur ){` |
|      - | 1830 | `			/* Append raw contents */` |
|      5 | 1831 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 1832 | `		}` |
|      5 | 1833 | `		if( &zIn[1] < zEnd ){` |
|      3 | 1834 | `			int c = zIn[1];` |
|      3 | 1835 | `			if( c == '\'' \|\| c == '"' \|\| c == '\\' ){` |
|      - | 1836 | `				/* Ignore the backslash */` |
|      3 | 1837 | `				zIn++;` |
|      1 | 1838 | `			}` |
|      2 | 1839 | `		}else{` |
|      3 | 1840 | `			break;` |
|      - | 1841 | `		}` |
|      1 | 1842 | `	}` |
|      7 | 1843 | `	return PH7_OK;` |
|      4 | 1844 | `}` |
|      - | 1845 | `/*` |
|      - | 1846 | ` * UTF-8-aware HTML entity machinery, shared by htmlspecialchars/htmlentities/` |
|      - | 1847 | ` * htmlspecialchars_decode/html_entity_decode/get_html_translation_table.` |
|      - | 1848 | ` * The implementations live further down in this file, next to the filter_var` |
|      - | 1849 | ` * FULL_SPECIAL_CHARS machinery they reuse (aHtml401Ent[]/FvHtml401Lookup()/` |
|      - | 1850 | ` * FvUtf8Next()). Semantics are byte-exact vs php 8.5.7; PHL is UTF-8-only` |
|      - | 1851 | ` * so every charset argument other than a UTF-8 alias gets PHP's` |
|      - | 1852 | ` * unsupported-charset warning and is treated as UTF-8.` |
|      - | 1853 | ` *` |
|      - | 1854 | ` * Flag model (the PHP-exact ENT_* values, see constant.c): bit 1 = encode/` |
|      - | 1855 | ` * decode single quotes, bit 2 = double quotes (ENT_QUOTES=3, ENT_COMPAT=2,` |
|      - | 1856 | ` * ENT_NOQUOTES=0); bits 16\|32 select the doctype (0=HTML401, 16=XML1,` |
|      - | 1857 | ` * 32=XHTML, 48=HTML5); ENT_IGNORE=4 drops invalid UTF-8 bytes (wins over` |
|      - | 1858 | ` * ENT_SUBSTITUTE=8, which replaces each with U+FFFD; with neither set the` |
|      - | 1859 | ` * whole result collapses to ""); ENT_DISALLOWED=128 substitutes valid but` |
|      - | 1860 | ` * doctype-disallowed codepoints. The shared default is` |
|      - | 1861 | ` * ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 = 11.` |
|      - | 1862 | ` */` |
|      - | 1863 | `static void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,int iFlags,int bAll,int bDoubleEncode);` |
|      - | 1864 | `static void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,int iFlags,int bFull);` |
|      - | 1865 | `static void HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx);` |
|      - | 1866 | `static void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags);` |
|      - | 1867 | `/*` |
|      - | 1868 | ` * string htmlspecialchars(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1869 | ` *                         [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])` |
|      - | 1870 | ` *  Convert the special characters & < > " ' to HTML entities.` |
|      - | 1871 | ` * Return` |
|      - | 1872 | ` *  The escaped string or NULL on failure.` |
|      - | 1873 | ` */` |
|     42 | 1874 | `static int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1875 | `{` |
|     43 | 1876 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1877 | `	const char *zIn;` |
|     43 | 1878 | `	int nLen,bDouble = 1;` |
|      - | 1879 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1880 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     43 | 1881 | `	if( nArg < 1 ){` |
|      - | 1882 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1883 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1884 | `		return PH7_OK;` |
|      - | 1885 | `	}` |
|      - | 1886 | `	/* Extract the target string */` |
|     43 | 1887 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     43 | 1888 | `	if( nArg > 1 ){` |
|     35 | 1889 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     17 | 1890 | `	}` |
|     43 | 1891 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     43 | 1892 | `	if( nArg > 3 ){` |
|      7 | 1893 | `		bDouble = ph7_value_to_bool(apArg[3]);` |
|      3 | 1894 | `	}` |
|     43 | 1895 | `	HtmlEscape(pCtx,zIn,nLen,iFlags,0,bDouble);` |
|     43 | 1896 | `	return PH7_OK;` |
|     22 | 1897 | `}` |
|      - | 1898 | `/*` |
|      - | 1899 | ` * string htmlspecialchars_decode(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401])` |
|      - | 1900 | ` *  Convert the special HTML entities (&amp; &lt; &gt; &quot; and the` |
|      - | 1901 | ` *  numeric/doctype forms of the two quotes) back to characters.` |
|      - | 1902 | ` * Return` |
|      - | 1903 | ` *  The unescaped string or NULL on failure.` |
|      - | 1904 | ` */` |
|     22 | 1905 | `static int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1906 | `{` |
|     23 | 1907 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1908 | `	const char *zIn;` |
|      - | 1909 | `	int nLen;` |
|      - | 1910 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1911 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     23 | 1912 | `	if( nArg < 1 ){` |
|      - | 1913 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1914 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1915 | `		return PH7_OK;` |
|      - | 1916 | `	}` |
|      - | 1917 | `	/* Extract the target string */` |
|     23 | 1918 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 1919 | `	if( nArg > 1 ){` |
|      9 | 1920 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      4 | 1921 | `	}` |
|     23 | 1922 | `	HtmlUnescape(pCtx,zIn,nLen,iFlags,0);` |
|     23 | 1923 | `	return PH7_OK;` |
|     12 | 1924 | `}` |
|      - | 1925 | `/*` |
|      - | 1926 | ` * array get_html_translation_table(int $table = HTML_SPECIALCHARS` |
|      - | 1927 | ` *      [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 [, string $encoding = "UTF-8"]])` |
|      - | 1928 | ` *  Return the translation table used by htmlspecialchars() (HTML_SPECIALCHARS)` |
|      - | 1929 | ` *  or htmlentities() (HTML_ENTITIES) as character => entity pairs.` |
|      - | 1930 | ` * Return` |
|      - | 1931 | ` *  The translation table as an array or NULL on failure.` |
|      - | 1932 | ` */` |
|     12 | 1933 | `static int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1934 | `{` |
|     13 | 1935 | `	int iTable = 0; /* HTML_SPECIALCHARS */` |
|     13 | 1936 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|     13 | 1937 | `	if( nArg > 0 ){` |
|     11 | 1938 | `		iTable = ph7_value_to_int(apArg[0]);` |
|      5 | 1939 | `	}` |
|     13 | 1940 | `	if( nArg > 1 ){` |
|      9 | 1941 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      4 | 1942 | `	}` |
|     13 | 1943 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     13 | 1944 | `	HtmlTranslationTable(pCtx,iTable,iFlags);` |
|     13 | 1945 | `	return PH7_OK;` |
|      1 | 1946 | `}` |
|      - | 1947 | `/*` |
|      - | 1948 | ` * string htmlentities(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1949 | ` *                     [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])` |
|      - | 1950 | ` *  Convert all applicable characters to HTML entities: the specials plus` |
|      - | 1951 | ` *  every codepoint with an HTML 4.01 named entity (aHtml401Ent[]).` |
|      - | 1952 | ` * Return` |
|      - | 1953 | ` *  The encoded string or NULL on failure.` |
|      - | 1954 | ` */` |
|     30 | 1955 | `static int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1956 | `{` |
|     31 | 1957 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1958 | `	const char *zIn;` |
|     31 | 1959 | `	int nLen,bDouble = 1;` |
|      - | 1960 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1961 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     31 | 1962 | `	if( nArg < 1 ){` |
|      - | 1963 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1964 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1965 | `		return PH7_OK;` |
|      - | 1966 | `	}` |
|      - | 1967 | `	/* Extract the target string */` |
|     31 | 1968 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 1969 | `	if( nArg > 1 ){` |
|     19 | 1970 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      9 | 1971 | `	}` |
|     31 | 1972 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     31 | 1973 | `	if( nArg > 3 ){` |
|      3 | 1974 | `		bDouble = ph7_value_to_bool(apArg[3]);` |
|      1 | 1975 | `	}` |
|     31 | 1976 | `	HtmlEscape(pCtx,zIn,nLen,iFlags,1,bDouble);` |
|     31 | 1977 | `	return PH7_OK;` |
|     16 | 1978 | `}` |
|      - | 1979 | `/*` |
|      - | 1980 | ` * string html_entity_decode(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1981 | ` *                           [, string $encoding = "UTF-8"]])` |
|      - | 1982 | ` *  Convert HTML entities (named — case-sensitive — and numeric, decimal or` |
|      - | 1983 | ` *  hex) back to their UTF-8 characters. The reverse of htmlentities().` |
|      - | 1984 | ` * Return` |
|      - | 1985 | ` *  The decoded string or NULL on failure.` |
|      - | 1986 | ` */` |
|     58 | 1987 | `static int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1988 | `{` |
|     59 | 1989 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1990 | `	const char *zIn;` |
|      - | 1991 | `	int nLen;` |
|      - | 1992 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1993 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     59 | 1994 | `	if( nArg < 1 ){` |
|      - | 1995 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1996 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1997 | `		return PH7_OK;` |
|      - | 1998 | `	}` |
|      - | 1999 | `	/* Extract the target string */` |
|     59 | 2000 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     59 | 2001 | `	if( nArg > 1 ){` |
|     27 | 2002 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     13 | 2003 | `	}` |
|     59 | 2004 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     59 | 2005 | `	HtmlUnescape(pCtx,zIn,nLen,iFlags,1);` |
|     59 | 2006 | `	return PH7_OK;` |
|     30 | 2007 | `}` |
|      - | 2008 | `/*` |
|      - | 2009 | ` * int strlen($string)` |
|      - | 2010 | ` *  return the length of the given string.` |
|      - | 2011 | ` * Parameter` |
|      - | 2012 | ` *  string: The string being measured for length.` |
|      - | 2013 | ` * Return` |
|      - | 2014 | ` *  length of the given string.` |
|      - | 2015 | ` */` |
|  75794 | 2016 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2017 | `{` |
|  75799 | 2018 | `	int iLen = 0;` |
|  75799 | 2019 | `	if( nArg > 0 ){` |
|  75799 | 2020 | `		StrNullArgNotice(pCtx,apArg[0],"strlen",1,"$string");` |
|  75799 | 2021 | `		ph7_value_to_string(apArg[0],&iLen);` |
|  37897 | 2022 | `	}` |
|      - | 2023 | `	/* String length */` |
|  75799 | 2024 | `	ph7_result_int(pCtx,iLen);` |
|  75799 | 2025 | `	return PH7_OK;` |
|      5 | 2026 | `}` |
|      - | 2027 | `/*` |
|      - | 2028 | ` * int strcmp(string $str1,string $str2)` |
|      - | 2029 | ` *  Perform a binary safe string comparison.` |
|      - | 2030 | ` * Parameter` |
|      - | 2031 | ` *  str1: The first string` |
|      - | 2032 | ` *  str2: The second string` |
|      - | 2033 | ` * Return` |
|      - | 2034 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2035 | ` *  than str2, and 0 if they are equal.` |
|      - | 2036 | ` */` |
|     72 | 2037 | `static int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2038 | `{` |
|      - | 2039 | `	const char *z1,*z2;` |
|      - | 2040 | `	int n1,n2;` |
|      - | 2041 | `	int res;` |
|     73 | 2042 | `	if( nArg < 2 ){` |
|    ! 0 | 2043 | `		res = nArg == 0 ? 0 : 1;` |
|    ! 0 | 2044 | `		ph7_result_int(pCtx,res);` |
|    ! 0 | 2045 | `		return PH7_OK;` |
|      - | 2046 | `	}` |
|      - | 2047 | `	/* Perform the comparison */` |
|     73 | 2048 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     73 | 2049 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     73 | 2050 | `	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2051 | `	/* Comparison result */` |
|     73 | 2052 | `	ph7_result_int(pCtx,res);` |
|     73 | 2053 | `	return PH7_OK;` |
|     37 | 2054 | `}` |
|      - | 2055 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 2056 | `/*` |
|      - | 2057 | ` * The natural-order comparison core lives OUTSIDE the PH7_DISABLE_BUILTIN_FUNC` |
|      - | 2058 | ` * guard: hashmap.c's SORT_NATURAL path (always compiled) calls PH7_StrNatCmp, so` |
|      - | 2059 | ` * it must exist in the tiny build too. [[tiny-build-disk-io-guard-fragility]]` |
|      - | 2060 | ` */` |
|      - | 2061 | `/*` |
|      - | 2062 | ` * Natural-order comparison core (Martin Pool's natcompare as adapted by php's` |
|      - | 2063 | ` * ext/standard/strnatcmp.c): digit runs compare numerically — the longer run` |
|      - | 2064 | ` * wins, a leading zero flips to fractional first-difference-wins semantics —` |
|      - | 2065 | ` * everything else compares bytewise with whitespace skipped.` |
|      - | 2066 | ` */` |
|     42 | 2067 | `static int StrNatCompareRight(const char **pa,const char *aEnd,const char **pb,const char *bEnd)` |
|      1 | 2068 | `{` |
|     43 | 2069 | `	int bias = 0;` |
|     71 | 2070 | `	for(;;){` |
|     93 | 2071 | `		int da = (*pa < aEnd) && SyisDigit(**pa);` |
|     93 | 2072 | `		int db = (*pb < bEnd) && SyisDigit(**pb);` |
|     93 | 2073 | `		if( !da && !db ){ return bias; }` |
|     73 | 2074 | `		if( !da ){ return -1; }` |
|     59 | 2075 | `		if( !db ){ return 1; }` |
|     51 | 2076 | `		if( **pa < **pb ){ if( !bias ){ bias = -1; } }` |
|     39 | 2077 | `		else if( **pa > **pb ){ if( !bias ){ bias = 1; } }` |
|     51 | 2078 | `		(*pa)++;` |
|     51 | 2079 | `		(*pb)++;` |
|      1 | 2080 | `	}` |
|     22 | 2081 | `}` |
|      4 | 2082 | `static int StrNatCompareLeft(const char **pa,const char *aEnd,const char **pb,const char *bEnd)` |
|      1 | 2083 | `{` |
|      2 | 2084 | `	for(;;){` |
|      5 | 2085 | `		int da = (*pa < aEnd) && SyisDigit(**pa);` |
|      5 | 2086 | `		int db = (*pb < bEnd) && SyisDigit(**pb);` |
|      5 | 2087 | `		if( !da && !db ){ return 0; }` |
|      5 | 2088 | `		if( !da ){ return -1; }` |
|      5 | 2089 | `		if( !db ){ return 1; }` |
|      5 | 2090 | `		if( **pa < **pb ){ return -1; }` |
|    ! 0 | 2091 | `		if( **pa > **pb ){ return 1; }` |
|    ! 0 | 2092 | `		(*pa)++;` |
|    ! 0 | 2093 | `		(*pb)++;` |
|    ! 0 | 2094 | `	}` |
|      3 | 2095 | `}` |
|     48 | 2096 | `PH7_PRIVATE int PH7_StrNatCmp(const char *zA,int nA,const char *zB,int nB,int bFold)` |
|      1 | 2097 | `{` |
|     49 | 2098 | `	const char *a = zA,*aEnd = &zA[nA];` |
|     49 | 2099 | `	const char *b = zB,*bEnd = &zB[nB];` |
|    146 | 2100 | `	for(;;){` |
|      - | 2101 | `		int ca,cb;` |
|    175 | 2102 | `		while( a < aEnd && SyisSpace(a[0]) ){ a++; }` |
|    173 | 2103 | `		while( b < bEnd && SyisSpace(b[0]) ){ b++; }` |
|    173 | 2104 | `		ca = (a < aEnd) ? (unsigned char)a[0] : 0;` |
|    173 | 2105 | `		cb = (b < bEnd) ? (unsigned char)b[0] : 0;` |
|    173 | 2106 | `		if( SyisDigit(ca) && SyisDigit(cb) ){` |
|     45 | 2107 | `			int r = (ca == '0' \|\| cb == '0')` |
|      4 | 2108 | `				? StrNatCompareLeft(&a,aEnd,&b,bEnd)` |
|     65 | 2109 | `				: StrNatCompareRight(&a,aEnd,&b,bEnd);` |
|     47 | 2110 | `			if( r ){ return r; }` |
|      5 | 2111 | `			continue;` |
|      - | 2112 | `		}` |
|    127 | 2113 | `		if( ca == 0 && cb == 0 ){ return 0; }` |
|    121 | 2114 | `		if( bFold ){` |
|     67 | 2115 | `			ca = SyToLower(ca);` |
|     67 | 2116 | `			cb = SyToLower(cb);` |
|     33 | 2117 | `		}` |
|    121 | 2118 | `		if( ca < cb ){ return -1; }` |
|    121 | 2119 | `		if( ca > cb ){ return 1; }` |
|    121 | 2120 | `		a++;` |
|    121 | 2121 | `		b++;` |
|      1 | 2122 | `	}` |
|     25 | 2123 | `}` |
|      - | 2124 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 2125 | `/*` |
|      - | 2126 | ` * int strnatcmp(string $string1, string $string2)` |
|      - | 2127 | ` * int strnatcasecmp(string $string1, string $string2)` |
|      - | 2128 | ` *  Natural-order string comparison ("img2" < "img10"), case folded for the` |
|      - | 2129 | ` *  latter. php 8.2+ normalizes the result to -1/0/1.` |
|      - | 2130 | ` */` |
|     20 | 2131 | `static int PH7_builtin_strnatcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2132 | `{` |
|      - | 2133 | `	const char *z1,*z2,*zFunc;` |
|      - | 2134 | `	int n1,n2,bFold;` |
|     21 | 2135 | `	if( nArg < 2 ){` |
|    ! 0 | 2136 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2137 | `		return PH7_OK;` |
|      - | 2138 | `	}` |
|     21 | 2139 | `	zFunc = ph7_function_name(pCtx);` |
|     21 | 2140 | `	bFold = zFunc[sizeof("strnat")-1] == 'c'; /* strnatCasecmp */` |
|     21 | 2141 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     21 | 2142 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     21 | 2143 | `	ph7_result_int(pCtx,PH7_StrNatCmp(z1,n1,z2,n2,bFold));` |
|     21 | 2144 | `	return PH7_OK;` |
|     11 | 2145 | `}` |
|      - | 2146 | `/*` |
|      - | 2147 | ` * int strncmp(string $str1,string $str2,int n)` |
|      - | 2148 | ` *  Perform a binary safe string comparison of the first n characters.` |
|      - | 2149 | ` * Parameter` |
|      - | 2150 | ` *  str1: The first string` |
|      - | 2151 | ` *  str2: The second string` |
|      - | 2152 | ` * Return` |
|      - | 2153 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2154 | ` *  than str2, and 0 if they are equal.` |
|      - | 2155 | ` */` |
|    366 | 2156 | `static int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2157 | `{` |
|      - | 2158 | `	const char *z1,*z2;` |
|      - | 2159 | `	int res;` |
|      - | 2160 | `	int n;` |
|    368 | 2161 | `	if( nArg < 3 ){` |
|      - | 2162 | `		/* Perform a standard comparison */` |
|    ! 0 | 2163 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|      - | 2164 | `	}` |
|      - | 2165 | `	/* Desired comparison length */` |
|    368 | 2166 | `	n  = ph7_value_to_int(apArg[2]);` |
|    368 | 2167 | `	if( n < 0 ){` |
|      - | 2168 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|      4 | 2169 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2170 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|      1 | 2171 | `			ph7_function_name(pCtx));` |
|      - | 2172 | `	}` |
|      - | 2173 | `	/* Perform the comparison */` |
|    366 | 2174 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|    366 | 2175 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|    366 | 2176 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|      - | 2177 | `	/* Comparison result */` |
|    366 | 2178 | `	ph7_result_int(pCtx,res);` |
|    366 | 2179 | `	return PH7_OK;` |
|    185 | 2180 | `}` |
|      - | 2181 | `/*` |
|      - | 2182 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|      - | 2183 | ` *  Perform a binary safe case-insensitive string comparison.` |
|      - | 2184 | ` * Parameter` |
|      - | 2185 | ` *  str1: The first string` |
|      - | 2186 | ` *  str2: The second string` |
|      - | 2187 | ` * Return` |
|      - | 2188 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2189 | ` *  than str2, and 0 if they are equal.` |
|      - | 2190 | ` */` |
|    152 | 2191 | `static int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2192 | `{` |
|      - | 2193 | `	const char *z1,*z2;` |
|      - | 2194 | `	int n1,n2;` |
|      - | 2195 | `	int res;` |
|    153 | 2196 | `	if( nArg < 2 ){` |
|    ! 0 | 2197 | `		res = nArg == 0 ? 0 : 1;` |
|    ! 0 | 2198 | `		ph7_result_int(pCtx,res);` |
|    ! 0 | 2199 | `		return PH7_OK;` |
|      - | 2200 | `	}` |
|      - | 2201 | `	/* Perform the comparison */` |
|    153 | 2202 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|    153 | 2203 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|    153 | 2204 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2205 | `	/* Comparison result */` |
|    153 | 2206 | `	ph7_result_int(pCtx,res);` |
|    153 | 2207 | `	return PH7_OK;` |
|     77 | 2208 | `}` |
|      - | 2209 | `/*` |
|      - | 2210 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|      - | 2211 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|      - | 2212 | ` * Parameter` |
|      - | 2213 | ` *  $str1: The first string` |
|      - | 2214 | ` *  $str2: The second string` |
|      - | 2215 | ` *  $len:  The length of strings to be used in the comparison.` |
|      - | 2216 | ` * Return` |
|      - | 2217 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2218 | ` *  than str2, and 0 if they are equal.` |
|      - | 2219 | ` */` |
|     40 | 2220 | `static int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2221 | `{` |
|      - | 2222 | `	const char *z1,*z2;` |
|      - | 2223 | `	int res;` |
|      - | 2224 | `	int n;` |
|     45 | 2225 | `	if( nArg < 3 ){` |
|      - | 2226 | `		/* Perform a standard comparison */` |
|    ! 0 | 2227 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 2228 | `	}` |
|      - | 2229 | `	/* Desired comparison length */` |
|     45 | 2230 | `	n  = ph7_value_to_int(apArg[2]);` |
|     45 | 2231 | `	if( n < 0 ){` |
|      - | 2232 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|      4 | 2233 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2234 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|      1 | 2235 | `			ph7_function_name(pCtx));` |
|      - | 2236 | `	}` |
|      - | 2237 | `	/* Perform the comparison */` |
|     43 | 2238 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     43 | 2239 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     43 | 2240 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 2241 | `	/* Comparison result */` |
|     43 | 2242 | `	ph7_result_int(pCtx,res);` |
|     43 | 2243 | `	return PH7_OK;` |
|     25 | 2244 | `}` |
|      - | 2245 | `/*` |
|      - | 2246 | ` * Implode context [i.e: it's private data].` |
|      - | 2247 | ` * A pointer to the following structure is forwarded` |
|      - | 2248 | ` * verbatim to the array walker callback defined below.` |
|      - | 2249 | ` */` |
|      - | 2250 | `struct implode_data {` |
|      - | 2251 | `	ph7_context *pCtx;    /* Call context */` |
|      - | 2252 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|      - | 2253 | `	const char *zSep;     /* Arguments separator if any */` |
|      - | 2254 | `	int nSeplen;          /* Separator length */` |
|      - | 2255 | `	int bFirst;           /* TRUE if first call */` |
|      - | 2256 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|      - | 2257 | `	sxi32 rc;             /* Captured allocation rc; SXERR_MEM => the builtin raises an OOM fatal */` |
|      - | 2258 | `};` |
|      - | 2259 | `/*` |
|      - | 2260 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 2261 | ` * The following routine is invoked for each array entry passed` |
|      - | 2262 | ` * to the implode() function.` |
|      - | 2263 | ` */` |
| 154730 | 2264 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      5 | 2265 | `{` |
|  77365 | 2266 | `	SXUNUSED(pKey);` |
| 154735 | 2267 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 2268 | `	const char *zData;` |
|      - | 2269 | `	int nLen;` |
| 154735 | 2270 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 2271 | `		if( pData->nSeplen > 0 ){` |
|      3 | 2272 | `			if( !pData->bFirst ){` |
|      - | 2273 | `				/* append the separator first */` |
|      3 | 2274 | `				if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 2275 | `					pData->rc = SXERR_MEM;` |
|    ! 0 | 2276 | `					return PH7_ABORT;` |
|      - | 2277 | `				}` |
|      2 | 2278 | `			}else{` |
|    ! 0 | 2279 | `				pData->bFirst = 0;` |
|      - | 2280 | `			}` |
|      1 | 2281 | `		}` |
|      - | 2282 | `		/* Recurse */` |
|      3 | 2283 | `		pData->bFirst = 1;` |
|      3 | 2284 | `		pData->nRecCount++;` |
|      3 | 2285 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 2286 | `		pData->nRecCount--;` |
|      - | 2287 | `		/* Propagate an allocation failure surfaced deeper in the recursion. */` |
|      3 | 2288 | `		if( pData->rc != SXRET_OK ){` |
|    ! 0 | 2289 | `			return PH7_ABORT;` |
|      - | 2290 | `		}` |
|      3 | 2291 | `		return PH7_OK;` |
|      - | 2292 | `	}` |
|      - | 2293 | `	/* Extract the string representation of the entry value */` |
| 154733 | 2294 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 2295 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 154733 | 2296 | `	if( pData->bFirst ){` |
|  33695 | 2297 | `		pData->bFirst = 0;` |
| 137888 | 2298 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 2299 | `		/* append the separator first */` |
| 120963 | 2300 | `		if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 2301 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 2302 | `			return PH7_ABORT;` |
|      - | 2303 | `		}` |
|  60479 | 2304 | `	}` |
|      - | 2305 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 154733 | 2306 | `	if( nLen > 0 ){` |
| 142005 | 2307 | `		if( ph7_result_string(pData->pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2308 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 2309 | `			return PH7_ABORT;` |
|      - | 2310 | `		}` |
|  71000 | 2311 | `	}` |
| 154733 | 2312 | `	return PH7_OK;` |
|  77370 | 2313 | `}` |
|      - | 2314 | `/*` |
|      - | 2315 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 2316 | ` * string implode(array $pieces,...)` |
|      - | 2317 | ` *  Join array elements with a string.` |
|      - | 2318 | ` * $glue` |
|      - | 2319 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 2320 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 2321 | ` * $pieces` |
|      - | 2322 | ` *   The array of strings to implode.` |
|      - | 2323 | ` * Return` |
|      - | 2324 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 2325 | ` *  order, with the glue string between each element.` |
|      - | 2326 | ` */` |
|  33718 | 2327 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2328 | `{` |
|      - | 2329 | `	struct implode_data imp_data;` |
|  33723 | 2330 | `	int i = 1;` |
|  33723 | 2331 | `	if( nArg < 1 ){` |
|      - | 2332 | `		/* Missing argument,return NULL */` |
|    ! 0 | 2333 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2334 | `		return PH7_OK;` |
|      - | 2335 | `	}` |
|      - | 2336 | `	/* Prepare the implode context */` |
|  33723 | 2337 | `	imp_data.pCtx = pCtx;` |
|  33723 | 2338 | `	imp_data.bRecursive = 0;` |
|  33723 | 2339 | `	imp_data.bFirst = 1;` |
|  33723 | 2340 | `	imp_data.nRecCount = 0;` |
|  33723 | 2341 | `	imp_data.rc = SXRET_OK;` |
|  33723 | 2342 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  33721 | 2343 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  33721 | 2344 | `		if( nArg > 1 && !ph7_value_is_array(apArg[1]) && !ph7_value_is_null(apArg[1]) ){` |
|      - | 2345 | `			/* php: implode($glue, $pieces) requires an ARRAY. PH7 stringified whatever it` |
|      - | 2346 | `			 * was handed, so implode(",", 5) quietly returned "5". */` |
|      - | 2347 | `			char zBuf[64];` |
|      4 | 2348 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 2349 | `				"implode(): Argument #2 ($array) must be of type ?array, %s given",` |
|      2 | 2350 | `				VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 2351 | `		}` |
|  16862 | 2352 | `	}else{` |
|      3 | 2353 | `		imp_data.zSep = 0;` |
|      3 | 2354 | `		imp_data.nSeplen = 0;` |
|      3 | 2355 | `		i = 0;` |
|      - | 2356 | `	}` |
|  33721 | 2357 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 2358 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2359 | `	}` |
|      - | 2360 | `	/* Start the 'join' process */` |
|  67437 | 2361 | `	while( i < nArg ){` |
|  33721 | 2362 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2363 | `			/* Iterate throw array entries */` |
|  33721 | 2364 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 2365 | `			/* Surface a callback allocation failure as a fatal */` |
|  33721 | 2366 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 2367 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2368 | `			}` |
|  16863 | 2369 | `		}else{` |
|      - | 2370 | `			const char *zData;` |
|      - | 2371 | `			int nLen;` |
|      - | 2372 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 2373 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2374 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 2375 | `			if( imp_data.bFirst ){` |
|    ! 0 | 2376 | `				imp_data.bFirst = 0;` |
|    ! 0 | 2377 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2378 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 2379 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2380 | `				}` |
|    ! 0 | 2381 | `			}` |
|      - | 2382 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 2383 | `			if( nLen > 0 ){` |
|    ! 0 | 2384 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2385 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2386 | `				}` |
|    ! 0 | 2387 | `			}` |
|      - | 2388 | `		}` |
|  33721 | 2389 | `		i++;` |
|      5 | 2390 | `	}` |
|  33721 | 2391 | `	return PH7_OK;` |
|  16864 | 2392 | `}` |
|      - | 2393 | `/*` |
|      - | 2394 | ` * Symisc eXtension:` |
|      - | 2395 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 2396 | ` * Purpose` |
|      - | 2397 | ` *  Same as implode() but recurse on arrays.` |
|      - | 2398 | ` * Example:` |
|      - | 2399 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 2400 | ` *   echo implode_recursive("/",$a);` |
|      - | 2401 | ` *   Will output` |
|      - | 2402 | ` *     usr/home/dean.` |
|      - | 2403 | ` *   While the standard implode would produce.` |
|      - | 2404 | ` *    usr/Array.` |
|      - | 2405 | ` * Parameter` |
|      - | 2406 | ` *  Refer to implode().` |
|      - | 2407 | ` * Return` |
|      - | 2408 | ` *  Refer to implode().` |
|      - | 2409 | ` */` |
|     12 | 2410 | `static int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2411 | `{` |
|      - | 2412 | `	struct implode_data imp_data;` |
|     13 | 2413 | `	int i = 1;` |
|     13 | 2414 | `	if( nArg < 1 ){` |
|      - | 2415 | `		/* Missing argument,return NULL */` |
|      3 | 2416 | `		ph7_result_null(pCtx);` |
|      3 | 2417 | `		return PH7_OK;` |
|      - | 2418 | `	}` |
|      - | 2419 | `	/* Prepare the implode context */` |
|     11 | 2420 | `	imp_data.pCtx = pCtx;` |
|     11 | 2421 | `	imp_data.bRecursive = 1;` |
|     11 | 2422 | `	imp_data.bFirst = 1;` |
|     11 | 2423 | `	imp_data.nRecCount = 0;` |
|     11 | 2424 | `	imp_data.rc = SXRET_OK;` |
|     11 | 2425 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 2426 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 2427 | `	}else{` |
|    ! 0 | 2428 | `		imp_data.zSep = 0;` |
|    ! 0 | 2429 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 2430 | `		i = 0;` |
|      - | 2431 | `	}` |
|     11 | 2432 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 2433 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2434 | `	}` |
|      - | 2435 | `	/* Start the 'join' process */` |
|     21 | 2436 | `	while( i < nArg ){` |
|     11 | 2437 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2438 | `			/* Iterate throw array entries */` |
|      3 | 2439 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 2440 | `			/* Surface a callback allocation failure as a fatal */` |
|      3 | 2441 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 2442 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2443 | `			}` |
|      2 | 2444 | `		}else{` |
|      - | 2445 | `			const char *zData;` |
|      - | 2446 | `			int nLen;` |
|      - | 2447 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 2448 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2449 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 2450 | `			if( imp_data.bFirst ){` |
|      9 | 2451 | `				imp_data.bFirst = 0;` |
|      4 | 2452 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2453 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 2454 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2455 | `				}` |
|    ! 0 | 2456 | `			}` |
|      - | 2457 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 2458 | `			if( nLen > 0 ){` |
|      9 | 2459 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2460 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2461 | `				}` |
|      4 | 2462 | `			}` |
|      - | 2463 | `		}` |
|     11 | 2464 | `		i++;` |
|      1 | 2465 | `	}` |
|     11 | 2466 | `	return PH7_OK;` |
|      7 | 2467 | `}` |
|      - | 2468 | `/*` |
|      - | 2469 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 2470 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 2471 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 2472 | ` * Parameters` |
|      - | 2473 | ` *  $delimiter` |
|      - | 2474 | ` *   The boundary string.` |
|      - | 2475 | ` * $string` |
|      - | 2476 | ` *   The input string.` |
|      - | 2477 | ` * $limit` |
|      - | 2478 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 2479 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 2480 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 2481 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 2482 | ` * Returns` |
|      - | 2483 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 2484 | ` *  on boundaries formed by the delimiter.` |
|      - | 2485 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 2486 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 2487 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 2488 | ` *  will be returned.` |
|      - | 2489 | ` * NOTE:` |
|      - | 2490 | ` *  Negative limit is not supported.` |
|      - | 2491 | ` */` |
|   6872 | 2492 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2493 | `{` |
|      - | 2494 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 2495 | `	int nDelim,nStrlen,iLimit;` |
|      - | 2496 | `	ph7_value *pArray;` |
|      - | 2497 | `	ph7_value *pValue;` |
|      - | 2498 | `	sxu32 nOfft;` |
|      - | 2499 | `	sxi32 rc;` |
|   6877 | 2500 | `	if( nArg < 2 ){` |
|      - | 2501 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2502 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2503 | `		return PH7_OK;` |
|      - | 2504 | `	}` |
|      - | 2505 | `	/* Extract the delimiter */` |
|   6877 | 2506 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   6877 | 2507 | `	if( nDelim < 1 ){` |
|      - | 2508 | `		/* Empty delimiter: PHP 8 throws a catchable ValueError. */` |
|      5 | 2509 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2510 | `			"explode(): Argument #1 ($separator) must not be empty");` |
|      - | 2511 | `	}` |
|      - | 2512 | `	/* Extract the string */` |
|   6873 | 2513 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   6873 | 2514 | `	if( nStrlen < 1 ){` |
|      - | 2515 | `		/* Empty string: normally an array with a single empty element (PHP behavior).` |
|      - | 2516 | `		 * A negative limit drops the last -limit components, so the sole empty` |
|      - | 2517 | `		 * component is dropped and the result is an empty array. */` |
|     13 | 2518 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|     13 | 2519 | `		if( pArrayTmp == 0 ){` |
|      - | 2520 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 2521 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2522 | `			return PH7_OK;` |
|      - | 2523 | `		}` |
|     13 | 2524 | `		if( !(nArg > 2 && ph7_value_to_int(apArg[2]) < 0) ){` |
|     11 | 2525 | `			ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|     11 | 2526 | `			if( pValueTmp == 0 ){` |
|      - | 2527 | `				/* Out of memory,return FALSE */` |
|    ! 0 | 2528 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 2529 | `				return PH7_OK;` |
|      - | 2530 | `			}` |
|     11 | 2531 | `			ph7_value_string(pValueTmp, "", 0);` |
|     11 | 2532 | `			if( ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp) != SXRET_OK ){` |
|    ! 0 | 2533 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2534 | `			}` |
|      5 | 2535 | `		}` |
|     13 | 2536 | `		ph7_result_value(pCtx, pArrayTmp);` |
|     13 | 2537 | `		return PH7_OK;` |
|      - | 2538 | `	}` |
|      - | 2539 | `	/* Point to the end of the string */` |
|   6861 | 2540 | `	zEnd = &zString[nStrlen];` |
|      - | 2541 | `	/* Create the array */` |
|   6861 | 2542 | `	pArray =  ph7_context_new_array(pCtx);` |
|   6861 | 2543 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   6861 | 2544 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 2545 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 2546 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2547 | `		return PH7_OK;` |
|      - | 2548 | `	}` |
|      - | 2549 | `	/* Set a defualt limit */` |
|   6861 | 2550 | `	iLimit = SXI32_HIGH;` |
|   6861 | 2551 | `	if( nArg > 2 ){` |
|     37 | 2552 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|     37 | 2553 | `		if( iLimit < 0 ){` |
|      - | 2554 | `			/* Negative limit: keep all components except the last -iLimit (PHP).` |
|      - | 2555 | `			 * Pre-count the components (delimiters + 1), then emit only the first` |
|      - | 2556 | `			 * nKeep CLEAN components — no trailing-remainder merge (the difference` |
|      - | 2557 | `			 * from the positive path). nKeep <= 0 drops everything -> empty array. */` |
|     17 | 2558 | `			int nTotal = 1,nKeep;` |
|     17 | 2559 | `			const char *zScan = zString;` |
|      - | 2560 | `			sxu32 nScanOfft;` |
|     57 | 2561 | `			while( SyBlobSearch(zScan,(sxu32)(zEnd - zScan),zDelim,nDelim,&nScanOfft) == SXRET_OK ){` |
|     41 | 2562 | `				nTotal++;` |
|     41 | 2563 | `				zScan = &zScan[nScanOfft + nDelim];` |
|      1 | 2564 | `			}` |
|     17 | 2565 | `			nKeep = nTotal + iLimit; /* iLimit < 0, so this is nTotal - (-iLimit) */` |
|     49 | 2566 | `			while( nKeep > (int)ph7_array_count(pArray)` |
|     39 | 2567 | `				&& SyBlobSearch(zString,(sxu32)(zEnd - zString),zDelim,nDelim,&nOfft) == SXRET_OK ){` |
|      - | 2568 | `				/* Emit the next clean component */` |
|     23 | 2569 | `				zCur = &zString[nOfft];` |
|     23 | 2570 | `				ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|     23 | 2571 | `				if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2572 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2573 | `				}` |
|     23 | 2574 | `				zString = &zCur[nDelim];` |
|     23 | 2575 | `				ph7_value_reset_string_cursor(pValue);` |
|      1 | 2576 | `			}` |
|     17 | 2577 | `			ph7_result_value(pCtx,pArray);` |
|     17 | 2578 | `			return PH7_OK;` |
|      - | 2579 | `		}` |
|     21 | 2580 | `		if( iLimit == 0 ){` |
|      5 | 2581 | `			iLimit = 1;` |
|      2 | 2582 | `		}` |
|     21 | 2583 | `		iLimit--;` |
|      9 | 2584 | `	}` |
|      - | 2585 | `	/* Start exploding */` |
|  82285 | 2586 | `	for(;;){` |
| 164575 | 2587 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 164575 | 2588 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 2589 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   6845 | 2590 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   6845 | 2591 | `			if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2592 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2593 | `			}` |
|   6845 | 2594 | `			break;` |
|      - | 2595 | `		}` |
|      - | 2596 | `		/* Point to the desired offset */` |
| 157735 | 2597 | `		zCur = &zString[nOfft];` |
|      - | 2598 | `		/* Perform the store operation (may be empty) */` |
| 157735 | 2599 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 157735 | 2600 | `		if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2601 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 2602 | `		}` |
|      - | 2603 | `		/* Point beyond the delimiter */` |
| 157735 | 2604 | `		zString = &zCur[nDelim];` |
|      - | 2605 | `		/* Reset the cursor */` |
| 157735 | 2606 | `		ph7_value_reset_string_cursor(pValue);` |
|      5 | 2607 | `	}` |
|      - | 2608 | `	/* Return the freshly created array */` |
|   6845 | 2609 | `	ph7_result_value(pCtx,pArray);` |
|      - | 2610 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 2611 | `	 * released as soon we return from this foregin function.` |
|      - | 2612 | `	 */` |
|   6845 | 2613 | `	return PH7_OK;` |
|   3441 | 2614 | `}` |
|      - | 2615 | `/*` |
|      - | 2616 | ` * string trim(string $str[,string $charlist ])` |
|      - | 2617 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2618 | ` * Parameters` |
|      - | 2619 | ` *  $str` |
|      - | 2620 | ` *   The string that will be trimmed.` |
|      - | 2621 | ` * $charlist` |
|      - | 2622 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2623 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2624 | ` *   With .. you can specify a range of characters.` |
|      - | 2625 | ` * Returns.` |
|      - | 2626 | ` *  Thr processed string.` |
|      - | 2627 | ` * NOTE:` |
|      - | 2628 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2629 | ` */` |
|  14498 | 2630 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2631 | `{` |
|  14503 | 2632 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"trim",1,"$string"); }` |
|      - | 2633 | `	const char *zString;` |
|      - | 2634 | `	int nLen;` |
|  14503 | 2635 | `	if( nArg < 1 ){` |
|      - | 2636 | `		/* Missing arguments,return null */` |
|    ! 0 | 2637 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2638 | `		return PH7_OK;` |
|      - | 2639 | `	}` |
|      - | 2640 | `	/* Extract the target string */` |
|  14503 | 2641 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  14503 | 2642 | `	if( nLen < 1 ){` |
|      - | 2643 | `		/* Empty string,return */` |
|    753 | 2644 | `		ph7_result_string(pCtx,"",0);` |
|    753 | 2645 | `		return PH7_OK;` |
|      - | 2646 | `	}` |
|      - | 2647 | `	/* Start the trim process */` |
|  13755 | 2648 | `	if( nArg < 2 ){` |
|      - | 2649 | `		SyString sStr;` |
|      - | 2650 | `		/* Remove white spaces and NUL bytes */` |
|  13725 | 2651 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  34425 | 2652 | `		SyStringFullTrimSafe(&sStr);` |
|  13725 | 2653 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   6865 | 2654 | `	}else{` |
|      - | 2655 | `		/* Char list */` |
|      - | 2656 | `		const char *zList;` |
|      - | 2657 | `		int nListlen;` |
|     33 | 2658 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     33 | 2659 | `		if( nListlen < 1 ){` |
|      - | 2660 | `			/* Return the string unchanged */` |
|      6 | 2661 | `			ph7_result_string(pCtx,zString,nLen);` |
|      4 | 2662 | `		}else{` |
|      - | 2663 | `			char aMask[256];` |
|     29 | 2664 | `			const char *zEnd = &zString[nLen];` |
|     29 | 2665 | `			const char *zCur = zString;` |
|     29 | 2666 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2667 | `			/* Left trim */` |
|     79 | 2668 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     53 | 2669 | `				zCur++;` |
|      3 | 2670 | `			}` |
|      - | 2671 | `			/* Right trim */` |
|     79 | 2672 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     53 | 2673 | `				zEnd--;` |
|      3 | 2674 | `			}` |
|     29 | 2675 | `			if( zCur >= zEnd ){` |
|      - | 2676 | `				/* Return the empty string */` |
|    ! 0 | 2677 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2678 | `			}else{` |
|     29 | 2679 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2680 | `			}` |
|      - | 2681 | `		}` |
|      - | 2682 | `	}` |
|  13755 | 2683 | `	return PH7_OK;` |
|   7254 | 2684 | `}` |
|      - | 2685 | `/*` |
|      - | 2686 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 2687 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 2688 | ` * Parameters` |
|      - | 2689 | ` *  $str` |
|      - | 2690 | ` *   The string that will be trimmed.` |
|      - | 2691 | ` * $charlist` |
|      - | 2692 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2693 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2694 | ` *   With .. you can specify a range of characters.` |
|      - | 2695 | ` * Returns.` |
|      - | 2696 | ` *  Thr processed string.` |
|      - | 2697 | ` * NOTE:` |
|      - | 2698 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2699 | ` */` |
|    166 | 2700 | `static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2701 | `{` |
|    170 | 2702 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"rtrim",1,"$string"); }` |
|      - | 2703 | `	const char *zString;` |
|      - | 2704 | `	int nLen;` |
|    170 | 2705 | `	if( nArg < 1 ){` |
|      - | 2706 | `		/* Missing arguments,return null */` |
|    ! 0 | 2707 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2708 | `		return PH7_OK;` |
|      - | 2709 | `	}` |
|      - | 2710 | `	/* Extract the target string */` |
|    170 | 2711 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    170 | 2712 | `	if( nLen < 1 ){` |
|      - | 2713 | `		/* Empty string,return */` |
|      7 | 2714 | `		ph7_result_string(pCtx,"",0);` |
|      7 | 2715 | `		return PH7_OK;` |
|      - | 2716 | `	}` |
|      - | 2717 | `	/* Start the trim process */` |
|    164 | 2718 | `	if( nArg < 2 ){` |
|      - | 2719 | `		SyString sStr;` |
|      - | 2720 | `		/* Remove white spaces and NUL bytes*/` |
|     19 | 2721 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     48 | 2722 | `		SyStringRightTrimSafe(&sStr);` |
|     19 | 2723 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|     10 | 2724 | `	}else{` |
|      - | 2725 | `		/* Char list */` |
|      - | 2726 | `		const char *zList;` |
|      - | 2727 | `		int nListlen;` |
|    146 | 2728 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|    146 | 2729 | `		if( nListlen < 1 ){` |
|      - | 2730 | `			/* Return the string unchanged */` |
|    ! 0 | 2731 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 2732 | `		}else{` |
|      - | 2733 | `			char aMask[256];` |
|    146 | 2734 | `			const char *zEnd = &zString[nLen];` |
|    146 | 2735 | `			const char *zCur = zString;` |
|    146 | 2736 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2737 | `			/* Right trim */` |
|    164 | 2738 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     21 | 2739 | `				zEnd--;` |
|      3 | 2740 | `			}` |
|    146 | 2741 | `			if( zEnd <= zCur ){` |
|      - | 2742 | `				/* Return the empty string */` |
|    ! 0 | 2743 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2744 | `			}else{` |
|    146 | 2745 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2746 | `			}` |
|      - | 2747 | `		}` |
|      - | 2748 | `	}` |
|    164 | 2749 | `	return PH7_OK;` |
|     87 | 2750 | `}` |
|      - | 2751 | `/*` |
|      - | 2752 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 2753 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2754 | ` * Parameters` |
|      - | 2755 | ` *  $str` |
|      - | 2756 | ` *   The string that will be trimmed.` |
|      - | 2757 | ` * $charlist` |
|      - | 2758 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2759 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2760 | ` *   With .. you can specify a range of characters.` |
|      - | 2761 | ` * Returns.` |
|      - | 2762 | ` *  Thr processed string.` |
|      - | 2763 | ` * NOTE:` |
|      - | 2764 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2765 | ` */` |
|     42 | 2766 | `static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2767 | `{` |
|     47 | 2768 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"ltrim",1,"$string"); }` |
|      - | 2769 | `	const char *zString;` |
|      - | 2770 | `	int nLen;` |
|     47 | 2771 | `	if( nArg < 1 ){` |
|      - | 2772 | `		/* Missing arguments,return null */` |
|    ! 0 | 2773 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2774 | `		return PH7_OK;` |
|      - | 2775 | `	}` |
|      - | 2776 | `	/* Extract the target string */` |
|     47 | 2777 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     47 | 2778 | `	if( nLen < 1 ){` |
|      - | 2779 | `		/* Empty string,return */` |
|     23 | 2780 | `		ph7_result_string(pCtx,"",0);` |
|     23 | 2781 | `		return PH7_OK;` |
|      - | 2782 | `	}` |
|      - | 2783 | `	/* Start the trim process */` |
|     29 | 2784 | `	if( nArg < 2 ){` |
|      - | 2785 | `		SyString sStr;` |
|      - | 2786 | `		/* Remove white spaces and NUL byte */` |
|      5 | 2787 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     13 | 2788 | `		SyStringLeftTrimSafe(&sStr);` |
|      5 | 2789 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      3 | 2790 | `	}else{` |
|      - | 2791 | `		/* Char list */` |
|      - | 2792 | `		const char *zList;` |
|      - | 2793 | `		int nListlen;` |
|     25 | 2794 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     25 | 2795 | `		if( nListlen < 1 ){` |
|      - | 2796 | `			/* Return the string unchanged */` |
|      3 | 2797 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2798 | `		}else{` |
|      - | 2799 | `			char aMask[256];` |
|     23 | 2800 | `			const char *zEnd = &zString[nLen];` |
|     23 | 2801 | `			const char *zCur = zString;` |
|     23 | 2802 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2803 | `			/* Left trim */` |
|     57 | 2804 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     39 | 2805 | `				zCur++;` |
|      5 | 2806 | `			}` |
|     23 | 2807 | `			if( zCur >= zEnd ){` |
|      - | 2808 | `				/* Return the empty string */` |
|    ! 0 | 2809 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2810 | `			}else{` |
|     23 | 2811 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2812 | `			}` |
|      - | 2813 | `		}` |
|      - | 2814 | `	}` |
|     29 | 2815 | `	return PH7_OK;` |
|     26 | 2816 | `}` |
|      - | 2817 | `/*` |
|      - | 2818 | ` * string strtolower(string $str)` |
|      - | 2819 | ` *  Make a string lowercase.` |
|      - | 2820 | ` * Parameters` |
|      - | 2821 | ` *  $str` |
|      - | 2822 | ` *   The input string.` |
|      - | 2823 | ` * Returns.` |
|      - | 2824 | ` *  The lowercased string.` |
|      - | 2825 | ` */` |
|  33598 | 2826 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2827 | `{` |
|  33603 | 2828 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtolower",1,"$string"); }` |
|      - | 2829 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2830 | `	int nLen;` |
|  33603 | 2831 | `	if( nArg < 1 ){` |
|      - | 2832 | `		/* Missing arguments,return null */` |
|    ! 0 | 2833 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2834 | `		return PH7_OK;` |
|      - | 2835 | `	}` |
|      - | 2836 | `	/* Extract the target string */` |
|  33603 | 2837 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  33603 | 2838 | `	if( nLen < 1 ){` |
|      - | 2839 | `		/* Empty string,return */` |
|      5 | 2840 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 2841 | `		return PH7_OK;` |
|      - | 2842 | `	}` |
|      - | 2843 | `	/* Perform the requested operation */` |
|  33599 | 2844 | `	zEnd = &zString[nLen];` |
| 105929 | 2845 | `	for(;;){` |
| 211863 | 2846 | `		if( zString >= zEnd ){` |
|      - | 2847 | `			/* No more input,break immediately */` |
|  33599 | 2848 | `			break;` |
|      - | 2849 | `		}` |
| 178269 | 2850 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2851 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2852 | `			zCur = zString;` |
|    ! 0 | 2853 | `			zString++;` |
|    ! 0 | 2854 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2855 | `				zString++;` |
|    ! 0 | 2856 | `			}` |
|      - | 2857 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2858 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2859 | `		}else{` |
| 178269 | 2860 | `			int c = zString[0];` |
| 178269 | 2861 | `			if( SyisUpper(c) ){` |
| 175697 | 2862 | `				c = SyToLower(zString[0]);` |
|  87846 | 2863 | `			}` |
|      - | 2864 | `			/* Append character */` |
| 178269 | 2865 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2866 | `			/* Advance the cursor */` |
| 178269 | 2867 | `			zString++;` |
|      - | 2868 | `		}` |
|      5 | 2869 | `	}` |
|  33599 | 2870 | `	return PH7_OK;` |
|  16804 | 2871 | `}` |
|      - | 2872 | `/*` |
|      - | 2873 | ` * string strtolower(string $str)` |
|      - | 2874 | ` *  Make a string uppercase.` |
|      - | 2875 | ` * Parameters` |
|      - | 2876 | ` *  $str` |
|      - | 2877 | ` *   The input string.` |
|      - | 2878 | ` * Returns.` |
|      - | 2879 | ` *  The uppercased string.` |
|      - | 2880 | ` */` |
|     74 | 2881 | `static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2882 | `{` |
|     78 | 2883 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtoupper",1,"$string"); }` |
|      - | 2884 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2885 | `	int nLen;` |
|     78 | 2886 | `	if( nArg < 1 ){` |
|      - | 2887 | `		/* Missing arguments,return null */` |
|    ! 0 | 2888 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2889 | `		return PH7_OK;` |
|      - | 2890 | `	}` |
|      - | 2891 | `	/* Extract the target string */` |
|     78 | 2892 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     78 | 2893 | `	if( nLen < 1 ){` |
|      - | 2894 | `		/* Empty string,return */` |
|      5 | 2895 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 2896 | `		return PH7_OK;` |
|      - | 2897 | `	}` |
|      - | 2898 | `	/* Perform the requested operation */` |
|     74 | 2899 | `	zEnd = &zString[nLen];` |
|    145 | 2900 | `	for(;;){` |
|    294 | 2901 | `		if( zString >= zEnd ){` |
|      - | 2902 | `			/* No more input,break immediately */` |
|     74 | 2903 | `			break;` |
|      - | 2904 | `		}` |
|    224 | 2905 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2906 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2907 | `			zCur = zString;` |
|    ! 0 | 2908 | `			zString++;` |
|    ! 0 | 2909 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2910 | `				zString++;` |
|    ! 0 | 2911 | `			}` |
|      - | 2912 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2913 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2914 | `		}else{` |
|    224 | 2915 | `			int c = zString[0];` |
|    224 | 2916 | `			if( SyisLower(c) ){` |
|    208 | 2917 | `				c = SyToUpper(zString[0]);` |
|    102 | 2918 | `			}` |
|      - | 2919 | `			/* Append character */` |
|    224 | 2920 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2921 | `			/* Advance the cursor */` |
|    224 | 2922 | `			zString++;` |
|      - | 2923 | `		}` |
|      4 | 2924 | `	}` |
|     74 | 2925 | `	return PH7_OK;` |
|     41 | 2926 | `}` |
|      - | 2927 | `/*` |
|      - | 2928 | ` * string ucfirst(string $str)` |
|      - | 2929 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 2930 | ` *  character is alphabetic.` |
|      - | 2931 | ` * Parameters` |
|      - | 2932 | ` *  $str` |
|      - | 2933 | ` *   The input string.` |
|      - | 2934 | ` * Returns.` |
|      - | 2935 | ` *  The processed string.` |
|      - | 2936 | ` */` |
|      4 | 2937 | `static int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2938 | `{` |
|      - | 2939 | `	const char *zString,*zEnd;` |
|      - | 2940 | `	int nLen,c;` |
|      5 | 2941 | `	if( nArg < 1 ){` |
|      - | 2942 | `		/* Missing arguments,return null */` |
|    ! 0 | 2943 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2944 | `		return PH7_OK;` |
|      - | 2945 | `	}` |
|      - | 2946 | `	/* Extract the target string */` |
|      5 | 2947 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2948 | `	if( nLen < 1 ){` |
|      - | 2949 | `		/* Empty string,return */` |
|      3 | 2950 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2951 | `		return PH7_OK;` |
|      - | 2952 | `	}` |
|      - | 2953 | `	/* Perform the requested operation */` |
|      3 | 2954 | `	zEnd = &zString[nLen];` |
|      3 | 2955 | `	c = zString[0];` |
|      3 | 2956 | `	if( SyisLower(c) ){` |
|      3 | 2957 | `		c = SyToUpper(c);` |
|      1 | 2958 | `	}` |
|      - | 2959 | `	/* Append the first character */` |
|      3 | 2960 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2961 | `	zString++;` |
|      3 | 2962 | `	if( zString < zEnd ){` |
|      - | 2963 | `		/* Append the rest of the input verbatim */` |
|      3 | 2964 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2965 | `	}` |
|      3 | 2966 | `	return PH7_OK;` |
|      3 | 2967 | `}` |
|      - | 2968 | `/*` |
|      - | 2969 | ` * string lcfirst(string $str)` |
|      - | 2970 | ` *  Make a string's first character lowercase.` |
|      - | 2971 | ` * Parameters` |
|      - | 2972 | ` *  $str` |
|      - | 2973 | ` *   The input string.` |
|      - | 2974 | ` * Returns.` |
|      - | 2975 | ` *  The processed string.` |
|      - | 2976 | ` */` |
|      4 | 2977 | `static int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2978 | `{` |
|      - | 2979 | `	const char *zString,*zEnd;` |
|      - | 2980 | `	int nLen,c;` |
|      5 | 2981 | `	if( nArg < 1 ){` |
|      - | 2982 | `		/* Missing arguments,return null */` |
|    ! 0 | 2983 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2984 | `		return PH7_OK;` |
|      - | 2985 | `	}` |
|      - | 2986 | `	/* Extract the target string */` |
|      5 | 2987 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2988 | `	if( nLen < 1 ){` |
|      - | 2989 | `		/* Empty string,return */` |
|      3 | 2990 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2991 | `		return PH7_OK;` |
|      - | 2992 | `	}` |
|      - | 2993 | `	/* Perform the requested operation */` |
|      3 | 2994 | `	zEnd = &zString[nLen];` |
|      3 | 2995 | `	c = zString[0];` |
|      3 | 2996 | `	if( SyisUpper(c) ){` |
|      3 | 2997 | `		c = SyToLower(c);` |
|      1 | 2998 | `	}` |
|      - | 2999 | `	/* Append the first character */` |
|      3 | 3000 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 3001 | `	zString++;` |
|      3 | 3002 | `	if( zString < zEnd ){` |
|      - | 3003 | `		/* Append the rest of the input verbatim */` |
|      3 | 3004 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 3005 | `	}` |
|      3 | 3006 | `	return PH7_OK;` |
|      3 | 3007 | `}` |
|      - | 3008 | `/*` |
|      - | 3009 | ` * int ord(string $string)` |
|      - | 3010 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 3011 | ` *  Passing null, an empty string, or a multi-byte string emits` |
|      - | 3012 | ` *  E_DEPRECATED to match PHP 8.4+ behaviour.` |
|      - | 3013 | ` * Parameters` |
|      - | 3014 | ` *  $string` |
|      - | 3015 | ` *   The input string.` |
|      - | 3016 | ` * Returns` |
|      - | 3017 | ` *  The ASCII value as an integer.` |
|      - | 3018 | ` */` |
|    226 | 3019 | `static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3020 | `{` |
|      - | 3021 | `	const char *zString;` |
|      - | 3022 | `	int nLen,c;` |
|      - | 3023 | `	/* PHP requires exactly one argument. */` |
|    229 | 3024 | `	if( nArg != 1 ){` |
|      4 | 3025 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3026 | `			"ArgumentCountError",` |
|      - | 3027 | `			"ord() expects exactly 1 argument, %d given",` |
|      1 | 3028 | `			nArg` |
|      - | 3029 | `			);` |
|      - | 3030 | `	}` |
|      - | 3031 | `	/* Passing null is deprecated (E_DEPRECATED).  PHP emits this before` |
|      - | 3032 | `	 * the empty-string deprecation, so we check null first. */` |
|    226 | 3033 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 3034 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 3035 | `			"ord(): Passing null to parameter #1 ($character) "` |
|      - | 3036 | `			"of type string is deprecated"` |
|      - | 3037 | `			);` |
|      1 | 3038 | `	}` |
|      - | 3039 | `	/* Extract the target string */` |
|    226 | 3040 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    226 | 3041 | `	if( nLen < 1 ){` |
|      - | 3042 | `		/* Empty string is deprecated (E_DEPRECATED). */` |
|      5 | 3043 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 3044 | `			"ord(): Providing an empty string is deprecated"` |
|      - | 3045 | `			);` |
|      5 | 3046 | `		ph7_result_int(pCtx,0);` |
|      5 | 3047 | `		return PH7_OK;` |
|      - | 3048 | `	}` |
|      - | 3049 | `	/* A string longer than one byte is deprecated (E_DEPRECATED). */` |
|    222 | 3050 | `	if( nLen > 1 ){` |
|      7 | 3051 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 3052 | `			"ord(): Providing a string that is not one byte long "` |
|      - | 3053 | `			"is deprecated. Use ord($str[0]) instead"` |
|      - | 3054 | `			);` |
|      3 | 3055 | `	}` |
|      - | 3056 | `	/* Extract the ASCII value of the first character */` |
|    222 | 3057 | `	c = (unsigned char)zString[0];` |
|      - | 3058 | `	/* Return that value */` |
|    222 | 3059 | `	ph7_result_int(pCtx,c);` |
|    222 | 3060 | `	return PH7_OK;` |
|    116 | 3061 | `}` |
|      - | 3062 | `/*` |
|      - | 3063 | ` * string chr(int $codepoint)` |
|      - | 3064 | ` *  Returns a one-character string containing the character specified` |
|      - | 3065 | ` *  by the given codepoint.  Any integer is accepted; values outside` |
|      - | 3066 | ` *  the [0, 255] range emit an E_DEPRECATED and are masked with & 0xFF.` |
|      - | 3067 | ` * Parameters` |
|      - | 3068 | ` *  $codepoint` |
|      - | 3069 | ` *   An integer codepoint.  Values outside 0-255 are deprecated and` |
|      - | 3070 | ` *   will be constrained to a single byte.` |
|      - | 3071 | ` * Returns` |
|      - | 3072 | ` *  A single-character string.` |
|      - | 3073 | ` */` |
|   7170 | 3074 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3075 | `{` |
|      - | 3076 | `	int c;` |
|      - | 3077 | `	unsigned char ch;` |
|      - | 3078 | `	/* PHP requires exactly one argument. */` |
|   7173 | 3079 | `	if( nArg != 1 ){` |
|      4 | 3080 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3081 | `			"ArgumentCountError",` |
|      - | 3082 | `			"chr() expects exactly 1 argument, %d given",` |
|      1 | 3083 | `			nArg` |
|      - | 3084 | `			);` |
|      - | 3085 | `	}` |
|      - | 3086 | `	/* Implicit float-to-int conversion loses precision (E_DEPRECATED).` |
|      - | 3087 | `	 * PHP does not prefix this message with "chr():", so we call` |
|      - | 3088 | `	 * PH7_VmThrowError() with a NULL function name to avoid the` |
|      - | 3089 | `	 * automatic prefix that ph7_context_throw_error*() would add. */` |
|   7171 | 3090 | `	if( ph7_value_is_float(apArg[0]) ){` |
|      - | 3091 | `		char zBuf[120];` |
|      4 | 3092 | `		SyBufferFormat(zBuf,sizeof(zBuf),` |
|      - | 3093 | `			"Implicit conversion from float %g to int loses precision",` |
|      1 | 3094 | `			ph7_value_to_double(apArg[0])` |
|      - | 3095 | `			);` |
|      3 | 3096 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zBuf);` |
|      1 | 3097 | `	}` |
|      - | 3098 | `	/* Extract the codepoint. */` |
|   7171 | 3099 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 3100 | `	/* Out-of-range codepoint (E_DEPRECATED), then mask to a single byte.` |
|      - | 3101 | `	 * PHP includes "chr(): " in the $errstr passed to set_error_handler,` |
|      - | 3102 | `	 * so we embed the prefix in the message and pass NULL as the function` |
|      - | 3103 | `	 * name to avoid the API double-prefixing it. */` |
|   7171 | 3104 | `	if( c < 0 \|\| c > 255 ){` |
|      5 | 3105 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 3106 | `			E_DEPRECATED,` |
|      - | 3107 | `			"chr(): Providing a value not in-between 0 and 255 is deprecated, "` |
|      - | 3108 | `			"this is because a byte value must be in the [0, 255] interval. "` |
|      - | 3109 | `			"The value used will be constrained using % 256"` |
|      - | 3110 | `			);` |
|      2 | 3111 | `	}` |
|      - | 3112 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|      - | 3113 | `	 * when taking the address of a wider int. */` |
|   7171 | 3114 | `	ch = (unsigned char)(c & 0xFF);` |
|      - | 3115 | `	/* Return the specified character */` |
|   7171 | 3116 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|   7171 | 3117 | `	return PH7_OK;` |
|   3588 | 3118 | `}` |
|      - | 3119 | `/*` |
|      - | 3120 | ` * Binary to hex consumer callback.` |
|      - | 3121 | ` * This callback is the default consumer used by the hash functions` |
|      - | 3122 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 3123 | ` */` |
|   3158 | 3124 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      3 | 3125 | `{` |
|      - | 3126 | `	/* Append hex chunk verbatim */` |
|   3161 | 3127 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   3161 | 3128 | `	return SXRET_OK;` |
|      3 | 3129 | `}` |
|      - | 3130 |  |
|      - | 3131 | `/*` |
|      - | 3132 | ` * string bin2hex(string $str)` |
|      - | 3133 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 3134 | ` * Parameters` |
|      - | 3135 | ` *  $str` |
|      - | 3136 | ` *   The input string.` |
|      - | 3137 | ` * Returns.` |
|      - | 3138 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 3139 | ` */` |
|    144 | 3140 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3141 | `{` |
|      - | 3142 | `	const char *zString;` |
|      - | 3143 | `	int nLen;` |
|      - | 3144 | `	/* PHP 8 requires exactly one argument (ArgumentCountError). */` |
|    147 | 3145 | `	if( nArg != 1 ){` |
|      4 | 3146 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3147 | `			"ArgumentCountError",` |
|      - | 3148 | `			"bin2hex() expects exactly 1 argument, %d given",` |
|      1 | 3149 | `			nArg` |
|      - | 3150 | `			);` |
|      - | 3151 | `	}` |
|      - | 3152 | `	/* In PHP 8, bin2hex() is strict about its parameter type.` |
|      - | 3153 | `	 * Array/Resource values are not allowed and trigger a TypeError.` |
|      - | 3154 | `	 * Objects without __toString() must also raise a TypeError.` |
|      - | 3155 | `	 */` |
|    216 | 3156 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) \|\|` |
|     71 | 3157 | `		( ph7_value_is_object(apArg[0]) &&` |
|    ! 0 | 3158 | `		  ((ph7_class_instance *)apArg[0]->x.pOther) != 0 &&` |
|    ! 0 | 3159 | `		  PH7_ClassExtractMethod(((ph7_class_instance *)apArg[0]->x.pOther)->pClass,` |
|    ! 0 | 3160 | `			"__toString",sizeof("__toString")-1) == 0` |
|      - | 3161 | `		)` |
|      - | 3162 | `	){` |
|    ! 0 | 3163 | `		const char *zType = ph7_type_name(apArg[0]);` |
|    ! 0 | 3164 | `		if( ph7_value_is_object(apArg[0]) ){` |
|    ! 0 | 3165 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    ! 0 | 3166 | `			if( pInst && pInst->pClass ){` |
|    ! 0 | 3167 | `				zType = SyStringData(&pInst->pClass->sName);` |
|    ! 0 | 3168 | `			}` |
|    ! 0 | 3169 | `		}` |
|    ! 0 | 3170 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3171 | `			"TypeError",` |
|      - | 3172 | `			"bin2hex(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 3173 | `			zType` |
|      - | 3174 | `			);` |
|      - | 3175 | `	}` |
|      - | 3176 | `	/* Extract the target string */` |
|    145 | 3177 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    145 | 3178 | `	if( nLen < 1 ){` |
|      - | 3179 | `		/* Empty string,return */` |
|     13 | 3180 | `		ph7_result_string(pCtx,"",0);` |
|     13 | 3181 | `		return PH7_OK;` |
|      - | 3182 | `	}` |
|      - | 3183 | `	/* Perform the requested operation */` |
|    133 | 3184 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|    133 | 3185 | `	return PH7_OK;` |
|     75 | 3186 | `}` |
|      - | 3187 |  |
|      - | 3188 | `/* Search callback signature */` |
|      - | 3189 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 3190 | `/*` |
|      - | 3191 | ` * Case-insensitive pattern match.` |
|      - | 3192 | ` * Brute force is the default search method used here.` |
|      - | 3193 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 3194 | ` * well for short/medium texts on modern hardware.` |
|      - | 3195 | ` */` |
|    298 | 3196 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      2 | 3197 | `{` |
|    300 | 3198 | `	const char *zpIn = (const char *)pPattern;` |
|    300 | 3199 | `	const char *zIn = (const char *)pText;` |
|    300 | 3200 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    300 | 3201 | `	const char *zEnd = &zIn[nLen];` |
|      - | 3202 | `	const char *zPtr,*zPtr2;` |
|      - | 3203 | `	int c,d;` |
|    300 | 3204 | `	if( iPatLen > nLen ){` |
|      - | 3205 | `		/* Don't bother processing */` |
|     67 | 3206 | `		return SXERR_NOTFOUND;` |
|      - | 3207 | `	}` |
|    860 | 3208 | `	for(;;){` |
|   1722 | 3209 | `		if( zIn >= zEnd ){` |
|    194 | 3210 | `			break;` |
|      - | 3211 | `		}` |
|   1530 | 3212 | `		c = SyToLower(zIn[0]);` |
|   1530 | 3213 | `		d = SyToLower(zpIn[0]);` |
|   1530 | 3214 | `		if( c == d ){` |
|    182 | 3215 | `			zPtr   = &zIn[1];` |
|    182 | 3216 | `			zPtr2  = &zpIn[1];` |
|    141 | 3217 | `			for(;;){` |
|    284 | 3218 | `				if( zPtr2 >= zpEnd ){` |
|      - | 3219 | `					/* Pattern found */` |
|     41 | 3220 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 3221 | `					return SXRET_OK;` |
|      - | 3222 | `				}` |
|    244 | 3223 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 3224 | `					break;` |
|      - | 3225 | `				}` |
|    244 | 3226 | `				c = SyToLower(zPtr[0]);` |
|    244 | 3227 | `				d = SyToLower(zPtr2[0]);` |
|    244 | 3228 | `				if( c != d ){` |
|    142 | 3229 | `					break;` |
|      - | 3230 | `				}` |
|    103 | 3231 | `				zPtr++; zPtr2++;` |
|      1 | 3232 | `			}` |
|     70 | 3233 | `		}` |
|   1490 | 3234 | `		zIn++;` |
|      2 | 3235 | `	}` |
|      - | 3236 | `	/* Pattern not found */` |
|    194 | 3237 | `	return SXERR_NOTFOUND;` |
|    151 | 3238 | `}` |
|      - | 3239 | `/*` |
|      - | 3240 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3241 | ` *  Find the first occurrence of a string.` |
|      - | 3242 | ` * Parameters` |
|      - | 3243 | ` *  $haystack` |
|      - | 3244 | ` *   The input string.` |
|      - | 3245 | ` * $needle` |
|      - | 3246 | ` *   Search pattern (must be a string).` |
|      - | 3247 | ` * $before_needle` |
|      - | 3248 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3249 | ` *   of the needle (excluding the needle).` |
|      - | 3250 | ` * Return` |
|      - | 3251 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3252 | ` */` |
|      6 | 3253 | `static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3254 | `{` |
|      7 | 3255 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3256 | `	const char *zBlob,*zPattern;` |
|      - | 3257 | `	int nLen,nPatLen;` |
|      - | 3258 | `	sxu32 nOfft;` |
|      - | 3259 | `	sxi32 rc;` |
|      7 | 3260 | `	if( nArg < 2 ){` |
|      - | 3261 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3262 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3263 | `		return PH7_OK;` |
|      - | 3264 | `	}` |
|      - | 3265 | `	/* Extract the needle and the haystack */` |
|      7 | 3266 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 3267 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 3268 | `	nOfft = 0; /* cc warning */` |
|      9 | 3269 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3270 | `		int before = 0;` |
|      - | 3271 | `		/* Perform the lookup */` |
|      5 | 3272 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3273 | `		if( rc != SXRET_OK ){` |
|      - | 3274 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3275 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3276 | `			return PH7_OK;` |
|      - | 3277 | `		}` |
|      - | 3278 | `		/* Return the portion of the string */` |
|      5 | 3279 | `		if( nArg > 2 ){` |
|      3 | 3280 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3281 | `		}` |
|      5 | 3282 | `		if( before ){` |
|      3 | 3283 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3284 | `		}else{` |
|      3 | 3285 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3286 | `		}` |
|      3 | 3287 | `	}else{` |
|      3 | 3288 | `		ph7_result_bool(pCtx,0);` |
|      - | 3289 | `	}` |
|      7 | 3290 | `	return PH7_OK;` |
|      4 | 3291 | `}` |
|      - | 3292 | `/*` |
|      - | 3293 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3294 | ` *  Case-insensitive strstr().` |
|      - | 3295 | ` * Parameters` |
|      - | 3296 | ` *  $haystack` |
|      - | 3297 | ` *   The input string.` |
|      - | 3298 | ` * $needle` |
|      - | 3299 | ` *   Search pattern (must be a string).` |
|      - | 3300 | ` * $before_needle` |
|      - | 3301 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3302 | ` *   of the needle (excluding the needle).` |
|      - | 3303 | ` * Return` |
|      - | 3304 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3305 | ` */` |
|      4 | 3306 | `static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3307 | `{` |
|      5 | 3308 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3309 | `	const char *zBlob,*zPattern;` |
|      - | 3310 | `	int nLen,nPatLen;` |
|      - | 3311 | `	sxu32 nOfft;` |
|      - | 3312 | `	sxi32 rc;` |
|      5 | 3313 | `	if( nArg < 2 ){` |
|      - | 3314 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3315 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3316 | `		return PH7_OK;` |
|      - | 3317 | `	}` |
|      - | 3318 | `	/* Extract the needle and the haystack */` |
|      5 | 3319 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3320 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      5 | 3321 | `	nOfft = 0; /* cc warning */` |
|      7 | 3322 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3323 | `		int before = 0;` |
|      - | 3324 | `		/* Perform the lookup */` |
|      5 | 3325 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3326 | `		if( rc != SXRET_OK ){` |
|      - | 3327 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3328 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3329 | `			return PH7_OK;` |
|      - | 3330 | `		}` |
|      - | 3331 | `		/* Return the portion of the string */` |
|      5 | 3332 | `		if( nArg > 2 ){` |
|      3 | 3333 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3334 | `		}` |
|      5 | 3335 | `		if( before ){` |
|      3 | 3336 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3337 | `		}else{` |
|      3 | 3338 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3339 | `		}` |
|      3 | 3340 | `	}else{` |
|    ! 0 | 3341 | `		ph7_result_bool(pCtx,0);` |
|      - | 3342 | `	}` |
|      5 | 3343 | `	return PH7_OK;` |
|      3 | 3344 | `}` |
|      - | 3345 | `/*` |
|      - | 3346 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3347 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 3348 | ` * Parameters` |
|      - | 3349 | ` *  $haystack` |
|      - | 3350 | ` *   The input string.` |
|      - | 3351 | ` * $needle` |
|      - | 3352 | ` *   Search pattern (must be a string).` |
|      - | 3353 | ` * $offset` |
|      - | 3354 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3355 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3356 | ` *   of haystack.` |
|      - | 3357 | ` * Return` |
|      - | 3358 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3359 | ` */` |
|   1552 | 3360 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3361 | `{` |
|   1557 | 3362 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strpos",1,"$haystack"); }` |
|   1557 | 3363 | `	if( nArg > 1 ){ StrNullArgNotice(pCtx,apArg[1],"strpos",2,"$needle"); }` |
|   1557 | 3364 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3365 | `	const char *zBlob,*zPattern;` |
|      - | 3366 | `	int nLen,nPatLen,nStart;` |
|      - | 3367 | `	sxu32 nOfft;` |
|      - | 3368 | `	sxi32 rc;` |
|   1557 | 3369 | `	if( nArg < 2 ){` |
|      - | 3370 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3371 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3372 | `		return PH7_OK;` |
|      - | 3373 | `	}` |
|      - | 3374 | `	/* Extract the needle and the haystack */` |
|   1557 | 3375 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|   1557 | 3376 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|   1557 | 3377 | `	nOfft = 0; /* cc warning */` |
|   1557 | 3378 | `	nStart = 0;` |
|      - | 3379 | `	/* Peek the starting offset if available */` |
|   1557 | 3380 | `	if( nArg > 2 ){` |
|     15 | 3381 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3382 | `		if( nStart < 0 ){` |
|    ! 0 | 3383 | `			nStart = -nStart;` |
|    ! 0 | 3384 | `		}` |
|     15 | 3385 | `		if( nStart >= nLen ){` |
|      - | 3386 | `			/* Invalid offset */` |
|    ! 0 | 3387 | `			nStart = 0;` |
|    ! 0 | 3388 | `		}else{` |
|     15 | 3389 | `			zBlob += nStart;` |
|     15 | 3390 | `			nLen -= nStart;` |
|      - | 3391 | `		}` |
|      7 | 3392 | `	}` |
|   1557 | 3393 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3394 | `		/* Perform the lookup */` |
|   1553 | 3395 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|   1553 | 3396 | `		if( rc != SXRET_OK ){` |
|      - | 3397 | `			/* Pattern not found,return FALSE */` |
|    799 | 3398 | `			ph7_result_bool(pCtx,0);` |
|    799 | 3399 | `			return PH7_OK;` |
|      - | 3400 | `		}` |
|      - | 3401 | `		/* Return the pattern position */` |
|    758 | 3402 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|    381 | 3403 | `	}else{` |
|      5 | 3404 | `		ph7_result_bool(pCtx,0);` |
|      - | 3405 | `	}` |
|    762 | 3406 | `	return PH7_OK;` |
|    781 | 3407 | `}` |
|      - | 3408 | `/*` |
|      - | 3409 | ` * Validate and resolve a single string-typed parameter for str_contains/` |
|      - | 3410 | ` * str_starts_with/str_ends_with. Emits an E_DEPRECATED notice for null` |
|      - | 3411 | ` * (matching PHP 8.1+; falls through with an empty string), and throws` |
|      - | 3412 | ` * TypeError for arrays, resources, and objects without __toString.` |
|      - | 3413 | ` *` |
|      - | 3414 | ` * For objects with __toString, invokes the method directly into pTmp and` |
|      - | 3415 | ` * uses its raw byte buffer. This preserves empty results, which the` |
|      - | 3416 | ` * engine's MemObjStringValue otherwise replaces with the literal "Object".` |
|      - | 3417 | ` *` |
|      - | 3418 | ` * On success, pzOut/pnOut point at the resolved byte buffer; the buffer` |
|      - | 3419 | ` * is valid until pTmp is released or pArg is mutated.` |
|      - | 3420 | ` */` |
|    668 | 3421 | `static sxi32 StrPredicateResolveArg(` |
|      - | 3422 | `	ph7_context *pCtx,` |
|      - | 3423 | `	ph7_value *pArg,` |
|      - | 3424 | `	const char *zFunc,` |
|      - | 3425 | `	int iArgNum,` |
|      - | 3426 | `	const char *zParamName,` |
|      - | 3427 | `	const char *zTypeStr, /* Declared type in the TypeError, e.g. "string" / "?string" */` |
|      - | 3428 | `	const char *zNullMsg,` |
|      - | 3429 | `	ph7_value *pTmp,` |
|      - | 3430 | `	const char **pzOut,` |
|      - | 3431 | `	int *pnOut` |
|      2 | 3432 | `){` |
|    670 | 3433 | `	if( ph7_value_is_null(pArg) ){` |
|     13 | 3434 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zNullMsg);` |
|     13 | 3435 | `		*pzOut = "";` |
|     13 | 3436 | `		*pnOut = 0;` |
|     13 | 3437 | `		return PH7_OK;` |
|      - | 3438 | `	}` |
|   1010 | 3439 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_resource(pArg) \|\|` |
|    656 | 3440 | `	    ( ph7_value_is_object(pArg) &&` |
|     72 | 3441 | `	      ((ph7_class_instance *)pArg->x.pOther) != 0 &&` |
|     48 | 3442 | `	      PH7_ClassExtractMethod(((ph7_class_instance *)pArg->x.pOther)->pClass,` |
|     24 | 3443 | `	        "__toString",sizeof("__toString")-1) == 0` |
|      - | 3444 | `	    )` |
|      - | 3445 | `	){` |
|    ! 0 | 3446 | `		const char *zType = ph7_type_name(pArg);` |
|    ! 0 | 3447 | `		if( ph7_value_is_object(pArg) ){` |
|    ! 0 | 3448 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|    ! 0 | 3449 | `			if( pInst && pInst->pClass ){` |
|    ! 0 | 3450 | `				zType = SyStringData(&pInst->pClass->sName);` |
|    ! 0 | 3451 | `			}` |
|    ! 0 | 3452 | `		}` |
|    ! 0 | 3453 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3454 | `			"TypeError",` |
|      - | 3455 | `			"%s(): Argument #%d (%s) must be of type %s, %s given",` |
|    ! 0 | 3456 | `			zFunc, iArgNum, zParamName, zTypeStr, zType` |
|      - | 3457 | `			);` |
|      - | 3458 | `	}` |
|    658 | 3459 | `	if( ph7_value_is_object(pArg) ){` |
|     49 | 3460 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     49 | 3461 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pInst->pClass,` |
|      - | 3462 | `			"__toString",sizeof("__toString")-1);` |
|     49 | 3463 | `		PH7_VmCallClassMethod(pCtx->pVm,pInst,pMethod,pTmp,0,0);` |
|     49 | 3464 | `		*pzOut = (const char *)SyBlobData(&pTmp->sBlob);` |
|     49 | 3465 | `		*pnOut = (int)SyBlobLength(&pTmp->sBlob);` |
|     49 | 3466 | `		return PH7_OK;` |
|      - | 3467 | `	}` |
|    610 | 3468 | `	*pzOut = ph7_value_to_string(pArg,pnOut);` |
|    610 | 3469 | `	return PH7_OK;` |
|    336 | 3470 | `}` |
|      - | 3471 | `/*` |
|      - | 3472 | ` * bool str_contains(string $haystack, string $needle)` |
|      - | 3473 | ` *  Determine if a string contains a given substring (PHP 8.0).` |
|      - | 3474 | ` * Return` |
|      - | 3475 | ` *  TRUE if needle occurs in haystack. An empty needle always returns TRUE.` |
|      - | 3476 | ` */` |
|     92 | 3477 | `static int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3478 | `{` |
|      - | 3479 | `	const char *zHaystack,*zNeedle;` |
|      - | 3480 | `	int nHayLen,nNeedleLen;` |
|      - | 3481 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3482 | `	sxi32 rc;` |
|     95 | 3483 | `	if( nArg != 2 ){` |
|      8 | 3484 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3485 | `			"ArgumentCountError",` |
|      - | 3486 | `			"str_contains() expects exactly 2 arguments, %d given",` |
|      2 | 3487 | `			nArg` |
|      - | 3488 | `			);` |
|      - | 3489 | `	}` |
|     90 | 3490 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     90 | 3491 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     90 | 3492 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_contains",1,"$haystack","string",` |
|      - | 3493 | `		"str_contains(): Passing null to parameter #1 ($haystack) "` |
|      - | 3494 | `		"of type string is deprecated",` |
|      - | 3495 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     90 | 3496 | `	if( rc != PH7_OK ) goto out;` |
|     90 | 3497 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_contains",2,"$needle","string",` |
|      - | 3498 | `		"str_contains(): Passing null to parameter #2 ($needle) "` |
|      - | 3499 | `		"of type string is deprecated",` |
|      - | 3500 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     90 | 3501 | `	if( rc != PH7_OK ) goto out;` |
|     90 | 3502 | `	if( nNeedleLen < 1 ){` |
|     13 | 3503 | `		ph7_result_bool(pCtx,1);` |
|     84 | 3504 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 3505 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3506 | `	}else{` |
|    104 | 3507 | `		sxi32 srch = SyBlobSearch((const void *)zHaystack,(sxu32)nHayLen,` |
|     34 | 3508 | `		                          (const void *)zNeedle,(sxu32)nNeedleLen,0);` |
|     70 | 3509 | `		ph7_result_bool(pCtx,srch == SXRET_OK ? 1 : 0);` |
|      - | 3510 | `	}` |
|     90 | 3511 | `	rc = PH7_OK;` |
|     44 | 3512 | `out:` |
|     90 | 3513 | `	PH7_MemObjRelease(&sHayTmp);` |
|     90 | 3514 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     90 | 3515 | `	return rc;` |
|     49 | 3516 | `}` |
|      - | 3517 | `/*` |
|      - | 3518 | ` * bool str_starts_with(string $haystack, string $needle)` |
|      - | 3519 | ` *  Check if a string starts with a given substring (PHP 8.0).` |
|      - | 3520 | ` * Return` |
|      - | 3521 | ` *  TRUE if haystack begins with needle. An empty needle always returns TRUE.` |
|      - | 3522 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 3523 | ` */` |
|     62 | 3524 | `static int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3525 | `{` |
|      - | 3526 | `	const char *zHaystack,*zNeedle;` |
|      - | 3527 | `	int nHayLen,nNeedleLen;` |
|      - | 3528 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3529 | `	sxi32 rc;` |
|     64 | 3530 | `	if( nArg != 2 ){` |
|      8 | 3531 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3532 | `			"ArgumentCountError",` |
|      - | 3533 | `			"str_starts_with() expects exactly 2 arguments, %d given",` |
|      2 | 3534 | `			nArg` |
|      - | 3535 | `			);` |
|      - | 3536 | `	}` |
|     59 | 3537 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     59 | 3538 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     59 | 3539 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_starts_with",1,"$haystack","string",` |
|      - | 3540 | `		"str_starts_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 3541 | `		"of type string is deprecated",` |
|      - | 3542 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     59 | 3543 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3544 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_starts_with",2,"$needle","string",` |
|      - | 3545 | `		"str_starts_with(): Passing null to parameter #2 ($needle) "` |
|      - | 3546 | `		"of type string is deprecated",` |
|      - | 3547 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     59 | 3548 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3549 | `	if( nNeedleLen < 1 ){` |
|     13 | 3550 | `		ph7_result_bool(pCtx,1);` |
|     53 | 3551 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 3552 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3553 | `	}else{` |
|     58 | 3554 | `		ph7_result_bool(pCtx,` |
|     38 | 3555 | `			SyMemcmp(zHaystack,zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 3556 | `	}` |
|     59 | 3557 | `	rc = PH7_OK;` |
|     29 | 3558 | `out:` |
|     59 | 3559 | `	PH7_MemObjRelease(&sHayTmp);` |
|     59 | 3560 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     59 | 3561 | `	return rc;` |
|     33 | 3562 | `}` |
|      - | 3563 | `/*` |
|      - | 3564 | ` * bool str_ends_with(string $haystack, string $needle)` |
|      - | 3565 | ` *  Check if a string ends with a given substring (PHP 8.0).` |
|      - | 3566 | ` * Return` |
|      - | 3567 | ` *  TRUE if haystack ends with needle. An empty needle always returns TRUE.` |
|      - | 3568 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 3569 | ` */` |
|     62 | 3570 | `static int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3571 | `{` |
|      - | 3572 | `	const char *zHaystack,*zNeedle;` |
|      - | 3573 | `	int nHayLen,nNeedleLen;` |
|      - | 3574 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3575 | `	sxi32 rc;` |
|     64 | 3576 | `	if( nArg != 2 ){` |
|      8 | 3577 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3578 | `			"ArgumentCountError",` |
|      - | 3579 | `			"str_ends_with() expects exactly 2 arguments, %d given",` |
|      2 | 3580 | `			nArg` |
|      - | 3581 | `			);` |
|      - | 3582 | `	}` |
|     59 | 3583 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     59 | 3584 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     59 | 3585 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_ends_with",1,"$haystack","string",` |
|      - | 3586 | `		"str_ends_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 3587 | `		"of type string is deprecated",` |
|      - | 3588 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     59 | 3589 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3590 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_ends_with",2,"$needle","string",` |
|      - | 3591 | `		"str_ends_with(): Passing null to parameter #2 ($needle) "` |
|      - | 3592 | `		"of type string is deprecated",` |
|      - | 3593 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     59 | 3594 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3595 | `	if( nNeedleLen < 1 ){` |
|     13 | 3596 | `		ph7_result_bool(pCtx,1);` |
|     53 | 3597 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 3598 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3599 | `	}else{` |
|     58 | 3600 | `		ph7_result_bool(pCtx,` |
|     38 | 3601 | `			SyMemcmp(zHaystack + (nHayLen - nNeedleLen),zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 3602 | `	}` |
|     59 | 3603 | `	rc = PH7_OK;` |
|     29 | 3604 | `out:` |
|     59 | 3605 | `	PH7_MemObjRelease(&sHayTmp);` |
|     59 | 3606 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     59 | 3607 | `	return rc;` |
|     33 | 3608 | `}` |
|      - | 3609 | `/*` |
|      - | 3610 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3611 | ` *  Case-insensitive strpos.` |
|      - | 3612 | ` * Parameters` |
|      - | 3613 | ` *  $haystack` |
|      - | 3614 | ` *   The input string.` |
|      - | 3615 | ` * $needle` |
|      - | 3616 | ` *   Search pattern (must be a string).` |
|      - | 3617 | ` * $offset` |
|      - | 3618 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3619 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3620 | ` *   of haystack.` |
|      - | 3621 | ` * Return` |
|      - | 3622 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3623 | ` */` |
|    196 | 3624 | `static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3625 | `{` |
|    198 | 3626 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3627 | `	const char *zBlob,*zPattern;` |
|      - | 3628 | `	int nLen,nPatLen,nStart;` |
|      - | 3629 | `	sxu32 nOfft;` |
|      - | 3630 | `	sxi32 rc;` |
|    198 | 3631 | `	if( nArg < 2 ){` |
|      - | 3632 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3633 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3634 | `		return PH7_OK;` |
|      - | 3635 | `	}` |
|      - | 3636 | `	/* Extract the needle and the haystack */` |
|    198 | 3637 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|    198 | 3638 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|    198 | 3639 | `	nOfft = 0; /* cc warning */` |
|    198 | 3640 | `	nStart = 0;` |
|      - | 3641 | `	/* Peek the starting offset if available */` |
|    198 | 3642 | `	if( nArg > 2 ){` |
|      5 | 3643 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 3644 | `		if( nStart < 0 ){` |
|      3 | 3645 | `			nStart = -nStart;` |
|      1 | 3646 | `		}` |
|      5 | 3647 | `		if( nStart >= nLen ){` |
|      - | 3648 | `			/* Invalid offset */` |
|    ! 0 | 3649 | `			nStart = 0;` |
|    ! 0 | 3650 | `		}else{` |
|      5 | 3651 | `			zBlob += nStart;` |
|      5 | 3652 | `			nLen -= nStart;` |
|      - | 3653 | `		}` |
|      2 | 3654 | `	}` |
|    198 | 3655 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3656 | `		/* Perform the lookup */` |
|    198 | 3657 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|    198 | 3658 | `		if( rc != SXRET_OK ){` |
|      - | 3659 | `			/* Pattern not found,return FALSE */` |
|    184 | 3660 | `			ph7_result_bool(pCtx,0);` |
|    184 | 3661 | `			return PH7_OK;` |
|      - | 3662 | `		}` |
|      - | 3663 | `		/* Return the pattern position */` |
|     15 | 3664 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 3665 | `	}else{` |
|    ! 0 | 3666 | `		ph7_result_bool(pCtx,0);` |
|      - | 3667 | `	}` |
|     15 | 3668 | `	return PH7_OK;` |
|    100 | 3669 | `}` |
|      - | 3670 | `/*` |
|      - | 3671 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3672 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 3673 | ` * Parameters` |
|      - | 3674 | ` *  $haystack` |
|      - | 3675 | ` *   The input string.` |
|      - | 3676 | ` * $needle` |
|      - | 3677 | ` *   Search pattern (must be a string).` |
|      - | 3678 | ` * $offset` |
|      - | 3679 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3680 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3681 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3682 | ` * Return` |
|      - | 3683 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3684 | ` */` |
|     42 | 3685 | `static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3686 | `{` |
|      - | 3687 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     43 | 3688 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3689 | `	int nLen,nPatLen;` |
|      - | 3690 | `	sxu32 nOfft;` |
|      - | 3691 | `	sxi32 rc;` |
|     43 | 3692 | `	if( nArg < 2 ){` |
|      - | 3693 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3694 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3695 | `		return PH7_OK;` |
|      - | 3696 | `	}` |
|      - | 3697 | `	/* Extract the needle and the haystack */` |
|     43 | 3698 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     43 | 3699 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3700 | `	/* Point to the end of the pattern */` |
|     43 | 3701 | `	zPtr = &zBlob[nLen - 1];` |
|     43 | 3702 | `	zEnd = &zBlob[nLen];` |
|      - | 3703 | `	/* Save the starting posistion */` |
|     43 | 3704 | `	zStart = zBlob;` |
|     43 | 3705 | `	nOfft = 0; /* cc warning */` |
|      - | 3706 | `	/* Peek the starting offset if available */` |
|     43 | 3707 | `	if( nArg > 2 ){` |
|      - | 3708 | `		int nStart;` |
|     21 | 3709 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 3710 | `		if( nStart < 0 ){` |
|     11 | 3711 | `			nStart = -nStart;` |
|     11 | 3712 | `			if( nStart >= nLen ){` |
|      - | 3713 | `				/* Invalid offset */` |
|      3 | 3714 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3715 | `				return PH7_OK;` |
|    ! 0 | 3716 | `			}else{` |
|      9 | 3717 | `				nLen -= nStart;` |
|      9 | 3718 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 3719 | `				zEnd = &zBlob[nLen];` |
|      - | 3720 | `			}` |
|      5 | 3721 | `		}else{` |
|     11 | 3722 | `			if( nStart >= nLen ){` |
|      - | 3723 | `				/* Invalid offset */` |
|      5 | 3724 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3725 | `				return PH7_OK;` |
|    ! 0 | 3726 | `			}else{` |
|      7 | 3727 | `				zBlob += nStart;` |
|      7 | 3728 | `				nLen -= nStart;` |
|      - | 3729 | `			}` |
|      - | 3730 | `		}` |
|      7 | 3731 | `	}` |
|     37 | 3732 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3733 | `		/* Perform the lookup */` |
|    123 | 3734 | `		for(;;){` |
|    247 | 3735 | `			if( zBlob >= zPtr ){` |
|     21 | 3736 | `				break;` |
|      - | 3737 | `			}` |
|    227 | 3738 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    227 | 3739 | `			if( rc == SXRET_OK ){` |
|      - | 3740 | `				/* Pattern found,return it's position */` |
|     15 | 3741 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     15 | 3742 | `				return PH7_OK;` |
|      - | 3743 | `			}` |
|    213 | 3744 | `			zPtr--;` |
|      1 | 3745 | `		}` |
|      - | 3746 | `		/* Pattern not found,return FALSE */` |
|     21 | 3747 | `		ph7_result_bool(pCtx,0);` |
|     11 | 3748 | `	}else{` |
|      3 | 3749 | `		ph7_result_bool(pCtx,0);` |
|      - | 3750 | `	}` |
|     23 | 3751 | `	return PH7_OK;` |
|     22 | 3752 | `}` |
|      - | 3753 | `/*` |
|      - | 3754 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3755 | ` *  Case-insensitive strrpos.` |
|      - | 3756 | ` * Parameters` |
|      - | 3757 | ` *  $haystack` |
|      - | 3758 | ` *   The input string.` |
|      - | 3759 | ` * $needle` |
|      - | 3760 | ` *   Search pattern (must be a string).` |
|      - | 3761 | ` * $offset` |
|      - | 3762 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3763 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3764 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3765 | ` * Return` |
|      - | 3766 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3767 | ` */` |
|     26 | 3768 | `static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3769 | `{` |
|      - | 3770 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     27 | 3771 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3772 | `	int nLen,nPatLen;` |
|      - | 3773 | `	sxu32 nOfft;` |
|      - | 3774 | `	sxi32 rc;` |
|     27 | 3775 | `	if( nArg < 2 ){` |
|      - | 3776 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3777 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3778 | `		return PH7_OK;` |
|      - | 3779 | `	}` |
|      - | 3780 | `	/* Extract the needle and the haystack */` |
|     27 | 3781 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 3782 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3783 | `	/* Point to the end of the pattern */` |
|     27 | 3784 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 3785 | `	zEnd = &zBlob[nLen];` |
|      - | 3786 | `	/* Save the starting posistion */` |
|     27 | 3787 | `	zStart = zBlob;` |
|     27 | 3788 | `	nOfft = 0; /* cc warning */` |
|      - | 3789 | `	/* Peek the starting offset if available */` |
|     27 | 3790 | `	if( nArg > 2 ){` |
|      - | 3791 | `		int nStart;` |
|     15 | 3792 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3793 | `		if( nStart < 0 ){` |
|      7 | 3794 | `			nStart = -nStart;` |
|      7 | 3795 | `			if( nStart >= nLen ){` |
|      - | 3796 | `				/* Invalid offset */` |
|      3 | 3797 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3798 | `				return PH7_OK;` |
|    ! 0 | 3799 | `			}else{` |
|      5 | 3800 | `				nLen -= nStart;` |
|      5 | 3801 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 3802 | `				zEnd = &zBlob[nLen];` |
|      - | 3803 | `			}` |
|      3 | 3804 | `		}else{` |
|      9 | 3805 | `			if( nStart >= nLen ){` |
|      - | 3806 | `				/* Invalid offset */` |
|      5 | 3807 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3808 | `				return PH7_OK;` |
|    ! 0 | 3809 | `			}else{` |
|      5 | 3810 | `				zBlob += nStart;` |
|      5 | 3811 | `				nLen -= nStart;` |
|      - | 3812 | `			}` |
|      - | 3813 | `		}` |
|      4 | 3814 | `	}` |
|     21 | 3815 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3816 | `		/* Perform the lookup */` |
|     44 | 3817 | `		for(;;){` |
|     89 | 3818 | `			if( zBlob >= zPtr ){` |
|      9 | 3819 | `				break;` |
|      - | 3820 | `			}` |
|     81 | 3821 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 3822 | `			if( rc == SXRET_OK ){` |
|      - | 3823 | `				/* Pattern found,return it's position */` |
|     11 | 3824 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 3825 | `				return PH7_OK;` |
|      - | 3826 | `			}` |
|     71 | 3827 | `			zPtr--;` |
|      1 | 3828 | `		}` |
|      - | 3829 | `		/* Pattern not found,return FALSE */` |
|      9 | 3830 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3831 | `	}else{` |
|      3 | 3832 | `		ph7_result_bool(pCtx,0);` |
|      - | 3833 | `	}` |
|     11 | 3834 | `	return PH7_OK;` |
|     14 | 3835 | `}` |
|      - | 3836 | `/*` |
|      - | 3837 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 3838 | ` *  Find the last occurrence of a character in a string.` |
|      - | 3839 | ` * Parameters` |
|      - | 3840 | ` *  $haystack` |
|      - | 3841 | ` *   The input string.` |
|      - | 3842 | ` * $needle` |
|      - | 3843 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 3844 | ` *  This behavior is different from that of strstr().` |
|      - | 3845 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 3846 | ` *  as the ordinal value of a character.` |
|      - | 3847 | ` * Return` |
|      - | 3848 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 3849 | ` */` |
|     22 | 3850 | `static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3851 | `{` |
|      - | 3852 | `	const char *zBlob;` |
|      - | 3853 | `	int nLen,c;` |
|     23 | 3854 | `	if( nArg < 2 ){` |
|      - | 3855 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3856 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3857 | `		return PH7_OK;` |
|      - | 3858 | `	}` |
|      - | 3859 | `	/* Extract the haystack */` |
|     23 | 3860 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3861 | `	c = 0; /* cc warning */` |
|     23 | 3862 | `	if( nLen > 0 ){` |
|      - | 3863 | `		sxu32 nOfft;` |
|      - | 3864 | `		sxi32 rc;` |
|     21 | 3865 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 3866 | `			const char *zPattern;` |
|     11 | 3867 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 3868 | `														 * for NULL pointer.` |
|      - | 3869 | `														 */` |
|     11 | 3870 | `			c = zPattern[0];` |
|      6 | 3871 | `		}else{` |
|      - | 3872 | `			/* Int cast */` |
|     11 | 3873 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 3874 | `		}` |
|      - | 3875 | `		/* Perform the lookup */` |
|     21 | 3876 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 3877 | `		if( rc != SXRET_OK ){` |
|      - | 3878 | `			/* No such entry,return FALSE */` |
|      7 | 3879 | `			ph7_result_bool(pCtx,0);` |
|      7 | 3880 | `			return PH7_OK;` |
|      - | 3881 | `		}` |
|      - | 3882 | `		/* Return the string portion */` |
|     15 | 3883 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 3884 | `	}else{` |
|      3 | 3885 | `		ph7_result_bool(pCtx,0);` |
|      - | 3886 | `	}` |
|     17 | 3887 | `	return PH7_OK;` |
|     12 | 3888 | `}` |
|      - | 3889 | `/*` |
|      - | 3890 | ` * string strrev(string $string)` |
|      - | 3891 | ` *  Reverse a string.` |
|      - | 3892 | ` * Parameters` |
|      - | 3893 | ` *  $string` |
|      - | 3894 | ` *   String to be reversed.` |
|      - | 3895 | ` * Return` |
|      - | 3896 | ` *  The reversed string.` |
|      - | 3897 | ` */` |
|      2 | 3898 | `static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3899 | `{` |
|      - | 3900 | `	const char *zIn,*zEnd;` |
|      - | 3901 | `	int nLen,c;` |
|      3 | 3902 | `	if( nArg < 1 ){` |
|      - | 3903 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3904 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3905 | `		return PH7_OK;` |
|      - | 3906 | `	}` |
|      - | 3907 | `	/* Extract the target string */` |
|      3 | 3908 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3909 | `	if( nLen < 1 ){` |
|      - | 3910 | `		/* Empty string Return null */` |
|    ! 0 | 3911 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3912 | `		return PH7_OK;` |
|      - | 3913 | `	}` |
|      - | 3914 | `	/* Perform the requested operation */` |
|      3 | 3915 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 3916 | `	for(;;){` |
|      9 | 3917 | `		if( zEnd < zIn ){` |
|      - | 3918 | `			/* No more input to process */` |
|      3 | 3919 | `			break;` |
|      - | 3920 | `		}` |
|      - | 3921 | `		/* Append current character */` |
|      7 | 3922 | `		c = zEnd[0];` |
|      7 | 3923 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 3924 | `		zEnd--;` |
|      1 | 3925 | `	}` |
|      3 | 3926 | `	return PH7_OK;` |
|      2 | 3927 | `}` |
|      - | 3928 | `/*` |
|      - | 3929 | ` * string ucwords(string $string [, string $separators = " \t\r\n\f\v"])` |
|      - | 3930 | ` *  Uppercase the first character of each word in a string.` |
|      - | 3931 | ` *  A word begins at the start of the string and after any character present in` |
|      - | 3932 | ` *  $separators. The default separators are the whitespace characters (space,` |
|      - | 3933 | ` *  horizontal tab, carriage return, newline, form-feed and vertical tab); an` |
|      - | 3934 | ` *  explicit $separators argument REPLACES them (an empty string leaves only the` |
|      - | 3935 | ` *  very first character upper-cased). Like PHP, this is byte-based: only ASCII` |
|      - | 3936 | ` *  bytes are upper-cased and a byte is a separator only if it appears in the set.` |
|      - | 3937 | ` * Parameters` |
|      - | 3938 | ` *  $string` |
|      - | 3939 | ` *   The input string.` |
|      - | 3940 | ` *  $separators` |
|      - | 3941 | ` *   The optional word-boundary characters.` |
|      - | 3942 | ` * Return` |
|      - | 3943 | ` *  The modified string.` |
|      - | 3944 | ` */` |
|     22 | 3945 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3946 | `{` |
|      - | 3947 | `	const char *zIn;` |
|      - | 3948 | `	int nLen,i,iStart;` |
|      - | 3949 | `	char aDelim[256];` |
|     23 | 3950 | `	if( nArg < 1 ){` |
|      - | 3951 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3952 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3953 | `		return PH7_OK;` |
|      - | 3954 | `	}` |
|      - | 3955 | `	/* Build the separator membership table: an explicit $separators argument` |
|      - | 3956 | `	 * replaces the default whitespace set (an empty string clears it). */` |
|     23 | 3957 | `	SyZero(aDelim,(sxu32)sizeof(aDelim));` |
|     23 | 3958 | `	if( nArg > 1 ){` |
|      - | 3959 | `		int nDelim;` |
|      9 | 3960 | `		const char *zDelim = ph7_value_to_string(apArg[1],&nDelim);` |
|     17 | 3961 | `		for( i = 0 ; i < nDelim ; i++ ){` |
|      9 | 3962 | `			aDelim[(unsigned char)zDelim[i]] = 1;` |
|      5 | 3963 | `		}` |
|      5 | 3964 | `	}else{` |
|     15 | 3965 | `		aDelim[(unsigned char)' ']  = 1;` |
|     15 | 3966 | `		aDelim[(unsigned char)'\t'] = 1;` |
|     15 | 3967 | `		aDelim[(unsigned char)'\r'] = 1;` |
|     15 | 3968 | `		aDelim[(unsigned char)'\n'] = 1;` |
|     15 | 3969 | `		aDelim[(unsigned char)'\f'] = 1;` |
|     15 | 3970 | `		aDelim[(unsigned char)'\v'] = 1;` |
|      - | 3971 | `	}` |
|      - | 3972 | `	/* Extract the target string */` |
|     23 | 3973 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3974 | `	if( nLen < 1 ){` |
|      - | 3975 | `		/* Empty string – match PHP semantics */` |
|      3 | 3976 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3977 | `		return PH7_OK;` |
|      - | 3978 | `	}` |
|      - | 3979 | `	/* Upper-case the first byte of each word (the leading byte, or any byte that` |
|      - | 3980 | `	 * follows a separator), appending the untouched runs in between verbatim. */` |
|     21 | 3981 | `	iStart = 0;` |
|    309 | 3982 | `	for( i = 0 ; i < nLen ; i++ ){` |
|    289 | 3983 | `		int c = (unsigned char)zIn[i];` |
|    289 | 3984 | `		if( (i == 0 \|\| aDelim[(unsigned char)zIn[i-1]]) && c < 0x80 && SyisLower(c) ){` |
|     53 | 3985 | `			char up = (char)SyToUpper(c);` |
|     53 | 3986 | `			if( i > iStart ){` |
|     35 | 3987 | `				ph7_result_string(pCtx,&zIn[iStart],i - iStart);` |
|     17 | 3988 | `			}` |
|     53 | 3989 | `			ph7_result_string(pCtx,&up,1);` |
|     53 | 3990 | `			iStart = i + 1;` |
|     26 | 3991 | `		}` |
|    145 | 3992 | `	}` |
|     21 | 3993 | `	if( nLen > iStart ){` |
|     21 | 3994 | `		ph7_result_string(pCtx,&zIn[iStart],nLen - iStart);` |
|     10 | 3995 | `	}` |
|     21 | 3996 | `	return PH7_OK;` |
|     12 | 3997 | `}` |
|      - | 3998 | `/*` |
|      - | 3999 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 4000 | ` *  Returns input repeated multiplier times.` |
|      - | 4001 | ` * Parameters` |
|      - | 4002 | ` *  $string` |
|      - | 4003 | ` *   String to be repeated.` |
|      - | 4004 | ` * $multiplier` |
|      - | 4005 | ` *  Number of time the input string should be repeated.` |
|      - | 4006 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 4007 | ` *  to 0, the function will return an empty string.` |
|      - | 4008 | ` * Return` |
|      - | 4009 | ` *  The repeated string.` |
|      - | 4010 | ` */` |
|  20434 | 4011 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4012 | `{` |
|      - | 4013 | `	const char *zIn;` |
|      - | 4014 | `	int nLen;` |
|      - | 4015 | `	ph7_int64 nMul;` |
|      - | 4016 | `	int rc;` |
|  20436 | 4017 | `	if( nArg < 2 ){` |
|      - | 4018 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 4019 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4020 | `		return PH7_OK;` |
|      - | 4021 | `	}` |
|      - | 4022 | `	/* Extract the target string */` |
|  20436 | 4023 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 4024 | `	/* Resolve $times through the shared ZPP helper so a lossy float / float-string` |
|      - | 4025 | `	 * carries php's precision deprecation and NAN/INF/non-numeric fail with php's` |
|      - | 4026 | `	 * TypeError — a bare ph7_value_to_int64() coerced them silently. */` |
|      - | 4027 | `	{` |
|  20436 | 4028 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"str_repeat",2,"$times","int",&nMul);` |
|  20436 | 4029 | `		if( rcArg != PH7_OK ){` |
|    ! 0 | 4030 | `			return rcArg;` |
|      - | 4031 | `		}` |
|      - | 4032 | `	}` |
|  20436 | 4033 | `	if( nMul < 0 ){` |
|      3 | 4034 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4035 | `			"str_repeat(): Argument #2 ($times) must be greater than or equal to 0");` |
|      - | 4036 | `	}` |
|  20434 | 4037 | `	if( nLen < 1 \|\| nMul < 1 ){` |
|      - | 4038 | `		/* Empty input or a zero multiplier yields the empty string (PHP). */` |
|    ! 0 | 4039 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4040 | `		return PH7_OK;` |
|      - | 4041 | `	}` |
|      - | 4042 | `	/* Perform the requested operation */` |
| 221930 | 4043 | `	for(;;){` |
| 443862 | 4044 | `		if( !nMul ){` |
|  20434 | 4045 | `			break;` |
|      - | 4046 | `		}` |
|      - | 4047 | `		/* Append the copy */` |
| 423430 | 4048 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 423430 | 4049 | `		if( rc != PH7_OK ){` |
|      - | 4050 | `			/* Allocation failed: surface a fatal instead of returning a` |
|      - | 4051 | `			 * silently-truncated string with a success status. */` |
|    ! 0 | 4052 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 4053 | `		}` |
| 423430 | 4054 | `		nMul--;` |
|      2 | 4055 | `	}` |
|  20434 | 4056 | `	return PH7_OK;` |
|  10219 | 4057 | `}` |
|      - | 4058 | `/*` |
|      - | 4059 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 4060 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 4061 | ` * Parameters` |
|      - | 4062 | ` *  $string` |
|      - | 4063 | ` *   The input string.` |
|      - | 4064 | ` * $is_xhtml` |
|      - | 4065 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 4066 | ` * Return` |
|      - | 4067 | ` *  The processed string.` |
|      - | 4068 | ` */` |
|      4 | 4069 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4070 | `{` |
|      - | 4071 | `	const char *zIn,*zCur,*zEnd;` |
|      5 | 4072 | `	int is_xhtml = 1; /* Default to XHTML-style '<br />' like PHP */` |
|      - | 4073 | `	int nLen;` |
|      5 | 4074 | `	if( nArg < 1 ){` |
|      - | 4075 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4076 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4077 | `		return PH7_OK;` |
|      - | 4078 | `	}` |
|      - | 4079 | `	/* Extract the target string */` |
|      5 | 4080 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 4081 | `	if( nLen < 1 ){` |
|      - | 4082 | `		/* Empty string,return null */` |
|    ! 0 | 4083 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4084 | `		return PH7_OK;` |
|      - | 4085 | `	}` |
|      5 | 4086 | `	if( nArg > 1 ){` |
|      3 | 4087 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 4088 | `	}` |
|      5 | 4089 | `	zEnd = &zIn[nLen];` |
|      - | 4090 | `	/* Perform the requested operation */` |
|      4 | 4091 | `	for(;;){` |
|      9 | 4092 | `		zCur = zIn;` |
|      - | 4093 | `		/* Delimit the string */` |
|     21 | 4094 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 4095 | `			zIn++;` |
|      1 | 4096 | `		}` |
|      9 | 4097 | `		if( zCur < zIn ){` |
|      - | 4098 | `			/* Output chunk verbatim */` |
|      9 | 4099 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 4100 | `		}` |
|      9 | 4101 | `		if( zIn >= zEnd ){` |
|      - | 4102 | `			/* No more input to process */` |
|      5 | 4103 | `			break;` |
|      - | 4104 | `		}` |
|      - | 4105 | `		/* Output the HTML line break */` |
|      - | 4106 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br />' (space before the slash), otherwise use '<br>' */` |
|      5 | 4107 | `		if( is_xhtml ){` |
|      3 | 4108 | `			ph7_result_string(pCtx,"<br />",(int)sizeof("<br />")-1);` |
|      2 | 4109 | `		}else{` |
|      3 | 4110 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 4111 | `		}` |
|      5 | 4112 | `		zCur = zIn;` |
|      - | 4113 | `		/* Append trailing line */` |
|     11 | 4114 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 4115 | `			zIn++;` |
|      1 | 4116 | `		}` |
|      5 | 4117 | `		if( zCur < zIn ){` |
|      - | 4118 | `			/* Output chunk verbatim */` |
|      5 | 4119 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 4120 | `		}` |
|      1 | 4121 | `	}` |
|      5 | 4122 | `	return PH7_OK;` |
|      3 | 4123 | `}` |
|      - | 4124 | `/*` |
|      - | 4125 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 4126 | ` *  According to the PHP reference manual.` |
|      - | 4127 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 4128 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 4129 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 4130 | ` * This applies to both sprintf() and printf().` |
|      - | 4131 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 4132 | ` * or more of these elements, in order:` |
|      - | 4133 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 4134 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 4135 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 4136 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 4137 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 4138 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 4139 | ` *   it with a single quote ('). See the examples below.` |
|      - | 4140 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 4141 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 4142 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 4143 | ` *   should result in.` |
|      - | 4144 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 4145 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 4146 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 4147 | ` *   limit to the string.` |
|      - | 4148 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 4149 | ` *       % - a literal percent character. No argument is required.` |
|      - | 4150 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 4151 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 4152 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 4153 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 4154 | ` * 	     for the number of digits after the decimal point.` |
|      - | 4155 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 4156 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 4157 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 4158 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 4159 | ` *       g - shorter of %e and %f.` |
|      - | 4160 | ` *       G - shorter of %E and %f.` |
|      - | 4161 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 4162 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 4163 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 4164 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 4165 | ` */` |
|      - | 4166 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 4167 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 4168 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 4169 | `/*` |
|      - | 4170 | `** Conversion types fall into various categories as defined by the` |
|      - | 4171 | `** following enumeration.` |
|      - | 4172 | `*/` |
|      - | 4173 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 4174 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 4175 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 4176 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 4177 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 4178 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 4179 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 4180 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 4181 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 4182 |  |
|      - | 4183 | `/*` |
|      - | 4184 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 4185 | `*/` |
|      - | 4186 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 4187 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 4188 | `/*` |
|      - | 4189 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 4190 | `** by an instance of the following structure` |
|      - | 4191 | `*/` |
|      - | 4192 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 4193 | `struct ph7_fmt_info` |
|      - | 4194 | `{` |
|      - | 4195 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 4196 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 4197 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 4198 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 4199 | `  char *charset; /* The character set for conversion */` |
|      - | 4200 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 4201 | `};` |
|      - | 4202 | `/* PH7_PhpFloatShape (php's float-shape post-processing) lives in memobj.c —` |
|      - | 4203 | ` * the default float->string cast needs it even when this whole formatting` |
|      - | 4204 | ` * region is compiled out by PH7_DISABLE_DISK_IO. */` |
|      - | 4205 | `/*` |
|      - | 4206 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 4207 | ` * used conversion types first.` |
|      - | 4208 | ` */` |
|      - | 4209 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 4210 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 4211 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 4212 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 4213 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 4214 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 4215 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 4216 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 4217 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 4218 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 4219 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 4220 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 4221 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 4222 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 4223 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 4224 | `  /* php's 'h'/'H' are the locale-independent twins of 'g'/'G'; PHL always` |
|      - | 4225 | `   * formats in the C locale, so they behave identically. */` |
|      - | 4226 | `  {  'h',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 4227 | `  {  'H',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 4228 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 4229 | `};` |
|      - | 4230 | `/*` |
|      - | 4231 | ` * PHP 8 raises a catchable ValueError for an unknown conversion specifier` |
|      - | 4232 | ` * (e.g. "%y", or the C-ism "%#x" — '#' is not a php flag). Because printf()` |
|      - | 4233 | ` * and fprintf() stream their output incrementally while sprintf() buffers it,` |
|      - | 4234 | ` * every format builtin calls PH7_FormatValidate (below) to check the whole` |
|      - | 4235 | ` * format string BEFORE formatting so the throw happens with no partial output` |
|      - | 4236 | ` * escaping (php buffers the entire result and only emits it on success). This` |
|      - | 4237 | ` * scan mirrors the specifier-locating logic of the main format loop below.` |
|      - | 4238 | ` * On the first unknown specifier, stores it in *pBad and returns TRUE; returns` |
|      - | 4239 | ` * FALSE when every specifier is known. (A found-flag rather than a sentinel` |
|      - | 4240 | ` * char, so a NUL specifier byte — "%\0" — is still reported, not mistaken for` |
|      - | 4241 | ` * "all valid".)` |
|      - | 4242 | ` */` |
|    498 | 4243 | `static int FormatUnknownSpec(const char *zIn,int nByte,int *pBad)` |
|      3 | 4244 | `{` |
|    501 | 4245 | `	const char *zEnd = &zIn[nByte];` |
|      - | 4246 | `	int c,idx;` |
|   3865 | 4247 | `	while( zIn < zEnd ){` |
|   3387 | 4248 | `		if( zIn[0] != '%' ){` |
|   2429 | 4249 | `			zIn++;` |
|   2429 | 4250 | `			continue;` |
|      - | 4251 | `		}` |
|    959 | 4252 | `		zIn++; /* jump the percent sign */` |
|      - | 4253 | `		/* php-supported flags: '-', '+', ' ', '0' and the "'<pad>'" custom-pad` |
|      - | 4254 | `		 * form. '#' is intentionally NOT treated as a flag so it surfaces as an` |
|      - | 4255 | `		 * unknown specifier, matching php. */` |
|   1199 | 4256 | `		while( zIn < zEnd ){` |
|   1197 | 4257 | `			c = zIn[0];` |
|   1197 | 4258 | `			if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|    229 | 4259 | `				zIn++;` |
|    229 | 4260 | `				continue;` |
|      - | 4261 | `			}` |
|    969 | 4262 | `			if( c=='\'' ){` |
|     13 | 4263 | `				zIn++;` |
|     13 | 4264 | `				if( zIn < zEnd ){` |
|     13 | 4265 | `					zIn++; /* the custom pad character */` |
|      6 | 4266 | `				}` |
|     13 | 4267 | `				continue;` |
|      - | 4268 | `			}` |
|    957 | 4269 | `			break;` |
|    ! 0 | 4270 | `		}` |
|      - | 4271 | `		/* field width */` |
|   1273 | 4272 | `		while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|    315 | 4273 | `			zIn++;` |
|      1 | 4274 | `		}` |
|      - | 4275 | `		/* positional specifier ($) — php parses flags AFTER it (e.g. "%1$-10s"),` |
|      - | 4276 | `		 * so skip the full flag set and width again, mirroring the main loop. */` |
|    959 | 4277 | `		if( zIn < zEnd && zIn[0]=='$' ){` |
|     15 | 4278 | `			zIn++;` |
|     17 | 4279 | `			while( zIn < zEnd ){` |
|     17 | 4280 | `				c = zIn[0];` |
|     17 | 4281 | `				if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|    ! 0 | 4282 | `					zIn++;` |
|    ! 0 | 4283 | `					continue;` |
|      - | 4284 | `				}` |
|     17 | 4285 | `				if( c=='\'' ){` |
|      3 | 4286 | `					zIn++;` |
|      3 | 4287 | `					if( zIn < zEnd ){` |
|      3 | 4288 | `						zIn++;` |
|      1 | 4289 | `					}` |
|      3 | 4290 | `					continue;` |
|      - | 4291 | `				}` |
|     15 | 4292 | `				break;` |
|    ! 0 | 4293 | `			}` |
|     23 | 4294 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|      9 | 4295 | `				zIn++;` |
|      1 | 4296 | `			}` |
|      7 | 4297 | `		}` |
|      - | 4298 | `		/* precision */` |
|    959 | 4299 | `		if( zIn < zEnd && zIn[0]=='.' ){` |
|    113 | 4300 | `			zIn++;` |
|    243 | 4301 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|    133 | 4302 | `				zIn++;` |
|      3 | 4303 | `			}` |
|     55 | 4304 | `		}` |
|      - | 4305 | `		/* a single 'l' length modifier (ignored, php compat) */` |
|    959 | 4306 | `		if( zIn < zEnd && zIn[0]=='l' ){` |
|     11 | 4307 | `			zIn++;` |
|      5 | 4308 | `		}` |
|    959 | 4309 | `		if( zIn >= zEnd ){` |
|      - | 4310 | `			/* A dangling '%' with no specifier: PHL's legacy path silently` |
|      - | 4311 | `			 * truncates here (recorded residual); nothing to validate. */` |
|      3 | 4312 | `			break;` |
|      - | 4313 | `		}` |
|    957 | 4314 | `		c = zIn[0];` |
|    957 | 4315 | `		zIn++; /* jump the conversion specifier */` |
|   3801 | 4316 | `		for( idx = 0 ; idx < (int)SX_ARRAYSIZE(aFmt) ; idx++ ){` |
|   3783 | 4317 | `			if( c == aFmt[idx].fmttype ){` |
|    939 | 4318 | `				break;` |
|      - | 4319 | `			}` |
|   1425 | 4320 | `		}` |
|    957 | 4321 | `		if( idx >= (int)SX_ARRAYSIZE(aFmt) ){` |
|     19 | 4322 | `			*pBad = c; /* unknown specifier */` |
|     19 | 4323 | `			return TRUE;` |
|      - | 4324 | `		}` |
|      3 | 4325 | `	}` |
|    483 | 4326 | `	return FALSE;` |
|    252 | 4327 | `}` |
|      - | 4328 | `/*` |
|      - | 4329 | ` * Validate a printf-style format string. PHP 8 raises a catchable ValueError for` |
|      - | 4330 | ` * an unknown conversion specifier, thrown before any output is produced. Every` |
|      - | 4331 | ` * format builtin (sprintf/printf/vprintf/vsprintf/fprintf/vfprintf) calls this` |
|      - | 4332 | ` * up-front, then propagates the returned status verbatim (PH7_EXCEPTION when the` |
|      - | 4333 | ` * throw is caught in place, PH7_ABORT when it goes uncaught).` |
|      - | 4334 | ` * Returns PH7_OK when the format is valid.` |
|      - | 4335 | ` */` |
|    498 | 4336 | `PH7_PRIVATE sxi32 PH7_FormatValidate(ph7_context *pCtx,const char *zFormat,int nByte)` |
|      3 | 4337 | `{` |
|    501 | 4338 | `	int badSpec = 0;` |
|    501 | 4339 | `	if( FormatUnknownSpec(zFormat,nByte,&badSpec) ){` |
|     28 | 4340 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      9 | 4341 | `			"Unknown format specifier \"%c\"",badSpec);` |
|      - | 4342 | `	}` |
|    483 | 4343 | `	return PH7_OK;` |
|    252 | 4344 | `}` |
|      - | 4345 | `/*` |
|      - | 4346 | ` * Count the number of VALUE arguments a format string needs: the greater of the` |
|      - | 4347 | ` * sequential (non-positional) conversion count and the highest positional index` |
|      - | 4348 | `` * (`%N$`). `%%` consumes nothing. Mirrors FormatUnknownSpec's specifier walk.`` |
|      - | 4349 | ` */` |
|    480 | 4350 | `static int FormatRequiredArgs(const char *zIn,int nByte)` |
|      3 | 4351 | `{` |
|    483 | 4352 | `	const char *zEnd = &zIn[nByte];` |
|    483 | 4353 | `	int c,seq = 0,maxpos = 0;` |
|   3833 | 4354 | `	while( zIn < zEnd ){` |
|   3355 | 4355 | `		int numVal = 0,pos = 0;` |
|   3355 | 4356 | `		if( zIn[0] != '%' ){` |
|   2415 | 4357 | `			zIn++;` |
|   2415 | 4358 | `			continue;` |
|      - | 4359 | `		}` |
|    941 | 4360 | `		zIn++; /* jump the percent sign */` |
|      - | 4361 | `		/* leading flags (incl. the "'<pad>'" custom-pad form) */` |
|   1181 | 4362 | `		while( zIn < zEnd ){` |
|   1179 | 4363 | `			c = zIn[0];` |
|   1179 | 4364 | `			if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){ zIn++; continue; }` |
|    951 | 4365 | `			if( c=='\'' ){ zIn++; if( zIn < zEnd ){ zIn++; } continue; }` |
|    939 | 4366 | `			break;` |
|    ! 0 | 4367 | `		}` |
|      - | 4368 | `		/* leading number: a positional index when a '$' follows, else the width */` |
|   1255 | 4369 | `		while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|    315 | 4370 | `			numVal = numVal*10 + (zIn[0]-'0');` |
|    315 | 4371 | `			zIn++;` |
|      1 | 4372 | `		}` |
|    941 | 4373 | `		if( zIn < zEnd && zIn[0]=='$' ){` |
|     15 | 4374 | `			pos = numVal;` |
|     15 | 4375 | `			zIn++;` |
|      - | 4376 | `			/* flags then width may follow the positional marker */` |
|     17 | 4377 | `			while( zIn < zEnd ){` |
|     17 | 4378 | `				c = zIn[0];` |
|     17 | 4379 | `				if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){ zIn++; continue; }` |
|     17 | 4380 | `				if( c=='\'' ){ zIn++; if( zIn < zEnd ){ zIn++; } continue; }` |
|     15 | 4381 | `				break;` |
|    ! 0 | 4382 | `			}` |
|     23 | 4383 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){ zIn++; }` |
|      7 | 4384 | `		}` |
|      - | 4385 | `		/* precision */` |
|    941 | 4386 | `		if( zIn < zEnd && zIn[0]=='.' ){` |
|    113 | 4387 | `			zIn++;` |
|    243 | 4388 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){ zIn++; }` |
|     55 | 4389 | `		}` |
|      - | 4390 | `		/* a single 'l' length modifier (ignored, php compat) */` |
|    941 | 4391 | `		if( zIn < zEnd && zIn[0]=='l' ){ zIn++; }` |
|    941 | 4392 | `		if( zIn >= zEnd ){ break; }` |
|    939 | 4393 | `		c = zIn[0];` |
|    939 | 4394 | `		zIn++; /* jump the conversion specifier */` |
|    939 | 4395 | `		if( c == '%' ){ continue; } /* %% consumes no argument */` |
|    931 | 4396 | `		if( pos > 0 ){` |
|     15 | 4397 | `			if( pos > maxpos ){ maxpos = pos; }` |
|      8 | 4398 | `		}else{` |
|    917 | 4399 | `			seq++;` |
|      - | 4400 | `		}` |
|      3 | 4401 | `	}` |
|    483 | 4402 | `	return seq > maxpos ? seq : maxpos;` |
|      3 | 4403 | `}` |
|      - | 4404 | `/*` |
|      - | 4405 | ` * PHP 8: a printf-family call with fewer VALUE arguments than the format needs` |
|      - | 4406 | ` * throws BEFORE any output. The non-vararg family (sprintf/printf/fprintf) raises` |
|      - | 4407 | ` * ArgumentCountError counting the format itself ("N arguments are required, M` |
|      - | 4408 | ` * given"); the vararg family (vsprintf/vprintf/vfprintf) raises a ValueError over` |
|      - | 4409 | ` * the values array ("The arguments array must contain N items, M given"). nValues` |
|      - | 4410 | ` * is the count of value arguments actually supplied; nFixed is the number of` |
|      - | 4411 | ` * fixed leading parameters counted in the ArgumentCountError totals (1 for the` |
|      - | 4412 | ` * $format of sprintf/printf, 2 for fprintf's $stream + $format — the vararg` |
|      - | 4413 | ` * ValueError counts only the array, so nFixed is ignored there). Returns PH7_OK` |
|      - | 4414 | ` * when enough.` |
|      - | 4415 | ` */` |
|    480 | 4416 | `PH7_PRIVATE sxi32 PH7_FormatCheckArgCount(ph7_context *pCtx,const char *zFormat,int nByte,int nValues,int nFixed,int bVararg)` |
|      3 | 4417 | `{` |
|    483 | 4418 | `	int required = FormatRequiredArgs(zFormat,nByte);` |
|    483 | 4419 | `	if( nValues < required ){` |
|     21 | 4420 | `		if( bVararg ){` |
|     10 | 4421 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      3 | 4422 | `				"The arguments array must contain %d items, %d given",required,nValues);` |
|      - | 4423 | `		}` |
|     22 | 4424 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|      7 | 4425 | `			"%d arguments are required, %d given",required+nFixed,nValues+nFixed);` |
|      - | 4426 | `	}` |
|    463 | 4427 | `	return PH7_OK;` |
|    243 | 4428 | `}` |
|      - | 4429 | `/*` |
|      - | 4430 | `` * PHP 8: a printf-family `$format` argument is a `string` parameter — scalars`` |
|      - | 4431 | ` * (int/float/bool) and null coerce to a string, but an array/object/resource` |
|      - | 4432 | ` * raises a catchable TypeError. iArg is the 1-based argument position ($format` |
|      - | 4433 | ` * is #1 for sprintf/printf/vprintf/vsprintf, #2 for fprintf/vfprintf). Returns` |
|      - | 4434 | ` * PH7_OK when the value is string-coercible (the caller then uses` |
|      - | 4435 | ` * ph7_value_to_string, which renders scalars/null verbatim).` |
|      - | 4436 | ` */` |
|      - | 4437 | `/*` |
|      - | 4438 | ` * php 8: a stream parameter that is not a resource is a TypeError, not a warning` |
|      - | 4439 | ` * with a 0 return -- the caller never learned its write went nowhere.` |
|      - | 4440 | ` */` |
|     24 | 4441 | `PH7_PRIVATE sxi32 PH7_CheckStreamArg(ph7_context *pCtx,ph7_value *pArg,int iArg,const char *zName)` |
|      1 | 4442 | `{` |
|     25 | 4443 | `	if( !ph7_value_is_resource(pArg) ){` |
|      - | 4444 | `		char zBuf[64];` |
|      4 | 4445 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4446 | `			"%s(): Argument #%d ($%s) must be of type resource, %s given",` |
|      1 | 4447 | `			ph7_function_name(pCtx),iArg,zName,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|      - | 4448 | `	}` |
|     23 | 4449 | `	return PH7_OK;` |
|     13 | 4450 | `}` |
|    510 | 4451 | `PH7_PRIVATE sxi32 PH7_FormatCheckFormatArg(ph7_context *pCtx,ph7_value *pArg,int iArg)` |
|      3 | 4452 | `{` |
|    513 | 4453 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg) \|\| ph7_value_is_resource(pArg) ){` |
|      - | 4454 | `		char zBuf[64];` |
|    ! 0 | 4455 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4456 | `			"%s(): Argument #%d ($format) must be of type string, %s given",` |
|    ! 0 | 4457 | `			ph7_function_name(pCtx),iArg,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|      - | 4458 | `	}` |
|    513 | 4459 | `	return PH7_OK;` |
|    258 | 4460 | `}` |
|      - | 4461 | `/*` |
|      - | 4462 | ` * Format a given string.` |
|      - | 4463 | ` * The root program.  All variations call this core.` |
|      - | 4464 | ` * INPUTS:` |
|      - | 4465 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 4466 | ` *            1. A pointer to the call context.` |
|      - | 4467 | ` *            2. A pointer to the list of characters to be output` |
|      - | 4468 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 4469 | ` *            3. An integer number of characters to be output.` |
|      - | 4470 | ` *               (Note: This number might be zero.)` |
|      - | 4471 | ` *            4. Upper layer private data.` |
|      - | 4472 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 4473 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 4474 | ` */` |
|    460 | 4475 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 4476 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 4477 | `	ph7_context *pCtx,  /* call context */` |
|      - | 4478 | `	const char *zIn,    /* Format string */` |
|      - | 4479 | `	int nByte,          /* Format string length */` |
|      - | 4480 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 4481 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 4482 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 4483 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 4484 | `	)` |
|      3 | 4485 | `{` |
|    463 | 4486 | `	char spaces[] = "                                                  ";` |
|      - | 4487 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    463 | 4488 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 4489 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 4490 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 4491 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 4492 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 4493 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 4494 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 4495 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 4496 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 4497 | `	ph7_int64 iVal;` |
|      - | 4498 | `	int precision;           /* Precision of the current field */` |
|      - | 4499 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 4500 | `	int c,rc,n;` |
|      - | 4501 | `	int length;              /* Length of the field */` |
|      - | 4502 | `	int prefix;` |
|      - | 4503 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 4504 | `	int width;               /* Width of the current field */` |
|      - | 4505 | `	int idx;` |
|    463 | 4506 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 4507 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 4508 | `	/* An unknown conversion specifier is rejected up-front by PH7_FormatValidate()` |
|      - | 4509 | `	 * (called by every format builtin before this routine), so the specifier set` |
|      - | 4510 | `	 * seen here is always valid. */` |
|      - | 4511 | `	/* Start the format process */` |
|    682 | 4512 | `	for(;;){` |
|   1367 | 4513 | `		zCur = zIn;` |
|   3773 | 4514 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|   2407 | 4515 | `			zIn++;` |
|      1 | 4516 | `		}` |
|   1367 | 4517 | `		if( zCur < zIn ){` |
|      - | 4518 | `			/* Consume chunk verbatim */` |
|    793 | 4519 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|    793 | 4520 | `			if( rc != SXRET_OK ){` |
|      - | 4521 | `				/* Callback requested an abort (e.g. an allocation failure) */` |
|    ! 0 | 4522 | `				break;` |
|      - | 4523 | `			}` |
|    396 | 4524 | `		}` |
|   1367 | 4525 | `		if( zIn >= zEnd ){` |
|      - | 4526 | `			/* No more input to process,break immediately */` |
|    461 | 4527 | `			break;` |
|      - | 4528 | `		}` |
|      - | 4529 | `		/* Find out what flags are present */` |
|    909 | 4530 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    906 | 4531 | `			flag_alternateform = flag_zeropad = 0;` |
|      - | 4532 | `		/* Reset the pad buffer to spaces: a custom pad char ('X) — or the string` |
|      - | 4533 | `		 * zero-pad below — from a PREVIOUS specifier must not bleed into this one.` |
|      - | 4534 | `		 * php resets the pad character for every specifier. */` |
|  46209 | 4535 | `		for( idx = 0 ; idx < etSPACESIZE ; ++idx ){ spaces[idx] = ' '; }` |
|    909 | 4536 | `		zIn++; /* Jump the precent sign */` |
|    453 | 4537 | `		do{` |
|   1149 | 4538 | `			c = zIn[0];` |
|   1149 | 4539 | `			switch( c ){` |
|     19 | 4540 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      7 | 4541 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 4542 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|    199 | 4543 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|      6 | 4544 | `			case '\'':` |
|     13 | 4545 | `				zIn++;` |
|     13 | 4546 | `				if( zIn < zEnd ){` |
|      - | 4547 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|     13 | 4548 | `					c = zIn[0];` |
|    613 | 4549 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    601 | 4550 | `						spaces[idx] = (char)c;` |
|    301 | 4551 | `					}` |
|     13 | 4552 | `					c = 0;` |
|      6 | 4553 | `				}` |
|     12 | 4554 | `				break;` |
|    906 | 4555 | `			default:                                       break;` |
|      - | 4556 | `			}` |
|   1149 | 4557 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 4558 | `		/* Get the field width */` |
|    909 | 4559 | `		width = 0;` |
|   1670 | 4560 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    309 | 4561 | `			width = width*10 + (zIn[0] - '0');` |
|    309 | 4562 | `			zIn++;` |
|      1 | 4563 | `		}` |
|    909 | 4564 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 4565 | `			/* Position specifer */` |
|      9 | 4566 | `			if( width > 0 ){` |
|      9 | 4567 | `				n = width;` |
|      9 | 4568 | `				if( vf && n > 0 ){` |
|    ! 0 | 4569 | `					n--;` |
|    ! 0 | 4570 | `				}` |
|      4 | 4571 | `			}` |
|      9 | 4572 | `			zIn++;` |
|      9 | 4573 | `			width = 0;` |
|      - | 4574 | `			/* php's grammar is %argnum$<flags><width>: the flags come AFTER the` |
|      - | 4575 | `			 * positional, so re-parse the full flag set here (e.g. "%1$-10s"),` |
|      - | 4576 | `			 * not just zero-padding. */` |
|      4 | 4577 | `			do{` |
|     11 | 4578 | `				c = zIn[0];` |
|     11 | 4579 | `				switch( c ){` |
|    ! 0 | 4580 | `				case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|    ! 0 | 4581 | `				case '+':   flag_plussign = 1;        c = 0;   break;` |
|    ! 0 | 4582 | `				case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|    ! 0 | 4583 | `				case '0':   flag_zeropad = 1;         c = 0;   break;` |
|      1 | 4584 | `				case '\'':` |
|      3 | 4585 | `					zIn++;` |
|      3 | 4586 | `					if( zIn < zEnd ){` |
|      3 | 4587 | `						c = zIn[0];` |
|    103 | 4588 | `						for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    101 | 4589 | `							spaces[idx] = (char)c;` |
|     51 | 4590 | `						}` |
|      3 | 4591 | `						c = 0;` |
|      1 | 4592 | `					}` |
|      2 | 4593 | `					break;` |
|      8 | 4594 | `				default:                                       break;` |
|      - | 4595 | `				}` |
|     11 | 4596 | `			}while( c==0 && (zIn++ < zEnd) );` |
|     21 | 4597 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|      9 | 4598 | `				width = width*10 + (zIn[0] - '0');` |
|      9 | 4599 | `				zIn++;` |
|      1 | 4600 | `			}` |
|      4 | 4601 | `		}` |
|    909 | 4602 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 4603 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 4604 | `		}` |
|      - | 4605 | `		/* Get the precision */` |
|    909 | 4606 | `		precision = -1;` |
|    909 | 4607 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|    113 | 4608 | `			precision = 0;` |
|    113 | 4609 | `			zIn++;` |
|    298 | 4610 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    133 | 4611 | `				precision = precision*10 + (zIn[0] - '0');` |
|    133 | 4612 | `				zIn++;` |
|      3 | 4613 | `			}` |
|     55 | 4614 | `		}` |
|      - | 4615 | `		/* Consume a single 'l' length modifier (a C-ism php accepts and ignores,` |
|      - | 4616 | `		 * e.g. "%ld"); PH7_FormatValidate mirrors this. Exactly one is skipped:` |
|      - | 4617 | `		 * in "%lld" the second 'l' becomes the (unknown) specifier, just like php. */` |
|    909 | 4618 | `		if( zIn < zEnd && zIn[0] == 'l' ){` |
|      9 | 4619 | `			zIn++;` |
|      4 | 4620 | `		}` |
|    909 | 4621 | `		if( zIn >= zEnd ){` |
|      - | 4622 | `			/* No more input */` |
|      3 | 4623 | `			break;` |
|      - | 4624 | `		}` |
|      - | 4625 | `		/* Fetch the info entry for the field */` |
|    907 | 4626 | `		pInfo = 0;` |
|    907 | 4627 | `		xtype = PH7_FMT_ERROR;` |
|    907 | 4628 | `		c = zIn[0];` |
|    907 | 4629 | `		zIn++; /* Jump the format specifer */` |
|   3431 | 4630 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|   3431 | 4631 | `			if( c==aFmt[idx].fmttype ){` |
|    907 | 4632 | `				pInfo = &aFmt[idx];` |
|    907 | 4633 | `				xtype = pInfo->type;` |
|    907 | 4634 | `				break;` |
|      - | 4635 | `			}` |
|   1265 | 4636 | `		}` |
|    907 | 4637 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    907 | 4638 | `		length = 0;` |
|      - | 4639 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 4640 | `		 /*` |
|      - | 4641 | `		  ** At this point, variables are initialized as follows:` |
|      - | 4642 | `		  **` |
|      - | 4643 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 4644 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 4645 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 4646 | `		  **                               field width was negative.` |
|      - | 4647 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 4648 | `		  **                               the conversion character.` |
|      - | 4649 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 4650 | `		  **   width                       The specified field width.  This is` |
|      - | 4651 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 4652 | `		  **   precision                   The specified precision.  The default` |
|      - | 4653 | `		  **                               is -1.` |
|      - | 4654 | `		  */` |
|    907 | 4655 | `		switch(xtype){` |
|      4 | 4656 | `		case PH7_FMT_PERCENT:` |
|      - | 4657 | `			/* A literal percent character */` |
|      9 | 4658 | `			zWorker[0] = '%';` |
|      9 | 4659 | `			length = (int)sizeof(char);` |
|      9 | 4660 | `			break;` |
|      2 | 4661 | `		case PH7_FMT_CHARX:` |
|      - | 4662 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 4663 | `			 * with that ASCII value` |
|      - | 4664 | `			 */` |
|      5 | 4665 | `			pArg = NEXT_ARG;` |
|      5 | 4666 | `			if( pArg == 0 ){` |
|    ! 0 | 4667 | `				c = 0;` |
|    ! 0 | 4668 | `			}else{` |
|      5 | 4669 | `				c = ph7_value_to_int(pArg);` |
|      - | 4670 | `			}` |
|      - | 4671 | `			/* NUL byte is an acceptable value */` |
|      5 | 4672 | `			zWorker[0] = (char)c;` |
|      5 | 4673 | `			length = (int)sizeof(char);` |
|      5 | 4674 | `			break;` |
|    188 | 4675 | `		case PH7_FMT_STRING:` |
|      - | 4676 | `			/* the argument is treated as and presented as a string */` |
|    377 | 4677 | `			pArg = NEXT_ARG;` |
|    377 | 4678 | `			if( pArg == 0 ){` |
|    ! 0 | 4679 | `				length = 0;` |
|    ! 0 | 4680 | `			}else{` |
|    377 | 4681 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 4682 | `			}` |
|    377 | 4683 | `			if( length < 1 ){` |
|      - | 4684 | `				/* An empty %s substitutes NOTHING in php. PH7 substituted a single` |
|      - | 4685 | `				 * SPACE here, so printf("[%s]","") printed "[ ]" and any format with an` |
|      - | 4686 | `				 * absent optional part gained a stray space. */` |
|      9 | 4687 | `				zBuf = "";` |
|      9 | 4688 | `				length = 0;` |
|      4 | 4689 | `			}` |
|    377 | 4690 | `			if( precision>=0 && precision<length ){` |
|      3 | 4691 | `				length = precision;` |
|      1 | 4692 | `			}` |
|    377 | 4693 | `			if( flag_zeropad ){` |
|      - | 4694 | `				/* zero-padding works on strings too */` |
|    103 | 4695 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    101 | 4696 | `					spaces[idx] = '0';` |
|     51 | 4697 | `				}` |
|      1 | 4698 | `			}` |
|    377 | 4699 | `			break;` |
|    158 | 4700 | `		case PH7_FMT_RADIX:` |
|    317 | 4701 | `			pArg = NEXT_ARG;` |
|    317 | 4702 | `			if( pArg == 0 ){` |
|    ! 0 | 4703 | `				iVal = 0;` |
|    ! 0 | 4704 | `			}else{` |
|    317 | 4705 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 4706 | `			}` |
|      - | 4707 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|    317 | 4708 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 4709 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 4710 | `			}` |
|      - | 4711 | `#if 1` |
|      - | 4712 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 4713 | `        ** I think this is stupid.*/` |
|    317 | 4714 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 4715 | `#else` |
|      - | 4716 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 4717 | `        ** but leave the prefix for hex.*/` |
|      - | 4718 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 4719 | `#endif` |
|    317 | 4720 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|    293 | 4721 | `          if( iVal<0 ){` |
|     25 | 4722 | `            iVal = -iVal;` |
|      - | 4723 | `			/* Ticket 1433-003 */` |
|     25 | 4724 | `			if( iVal < 0 ){` |
|      - | 4725 | `				/* Overflow */` |
|    ! 0 | 4726 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4727 | `			}` |
|     25 | 4728 | `            prefix = '-';` |
|    281 | 4729 | `          }else if( flag_plussign )  prefix = '+';` |
|    267 | 4730 | `          else if( flag_blanksign )  prefix = ' ';` |
|    265 | 4731 | `          else                       prefix = 0;` |
|    147 | 4732 | `        }else{` |
|     25 | 4733 | `			if( iVal<0 ){` |
|    ! 0 | 4734 | `				iVal = -iVal;` |
|      - | 4735 | `				/* Ticket 1433-003 */` |
|    ! 0 | 4736 | `				if( iVal < 0 ){` |
|      - | 4737 | `					/* Overflow */` |
|    ! 0 | 4738 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4739 | `				}` |
|    ! 0 | 4740 | `			}` |
|     25 | 4741 | `			prefix = 0;` |
|      - | 4742 | `		}` |
|    317 | 4743 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|    185 | 4744 | `          precision = width-(prefix!=0);` |
|     92 | 4745 | `        }` |
|    317 | 4746 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 4747 | `        {` |
|      - | 4748 | `          register char *cset;      /* Use registers for speed */` |
|      - | 4749 | `          register int base;` |
|    317 | 4750 | `          cset = pInfo->charset;` |
|    317 | 4751 | `          base = pInfo->base;` |
|    158 | 4752 | `          do{                                           /* Convert to ascii */` |
|    393 | 4753 | `            *(--zBuf) = cset[iVal%base];` |
|    393 | 4754 | `            iVal = iVal/base;` |
|    393 | 4755 | `          }while( iVal>0 );` |
|      - | 4756 | `        }` |
|    317 | 4757 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    519 | 4758 | `        for(idx=precision-length; idx>0; idx--){` |
|    203 | 4759 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|    102 | 4760 | `        }` |
|    317 | 4761 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|    317 | 4762 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 4763 | `          char *pre, x;` |
|    ! 0 | 4764 | `          pre = pInfo->prefix;` |
|    ! 0 | 4765 | `          if( *zBuf!=pre[0] ){` |
|    ! 0 | 4766 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|    ! 0 | 4767 | `          }` |
|    ! 0 | 4768 | `        }` |
|    317 | 4769 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    317 | 4770 | `		break;` |
|    100 | 4771 | `		case PH7_FMT_FLOAT:` |
|      - | 4772 | `		case PH7_FMT_EXP:` |
|      - | 4773 | `		case PH7_FMT_GENERIC:{` |
|      - | 4774 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 4775 | `		double realvalue;` |
|      - | 4776 | `		char zFmt[8];` |
|      - | 4777 | `		int nOut, nFmt;` |
|    203 | 4778 | `		pArg = NEXT_ARG;` |
|    203 | 4779 | `		if( pArg == 0 ){` |
|    ! 0 | 4780 | `			realvalue = 0;` |
|    ! 0 | 4781 | `		}else{` |
|    203 | 4782 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 4783 | `		}` |
|      - | 4784 | `		/* php prints the IEEE specials bare — NaN / INF / -INF with no width` |
|      - | 4785 | `		 * padding, precision, or sign flags (php_sprintf_appenddouble). */` |
|    203 | 4786 | `		if( PH7_IS_NAN(realvalue) ){` |
|     21 | 4787 | `			zBuf = "NaN";` |
|     21 | 4788 | `			length = 3;` |
|     21 | 4789 | `			width = 0;` |
|     21 | 4790 | `			break;` |
|      - | 4791 | `		}` |
|    183 | 4792 | `		if( PH7_IS_INF(realvalue) ){` |
|     37 | 4793 | `			if( realvalue < 0.0 ){` |
|     15 | 4794 | `				zBuf = "-INF";` |
|     15 | 4795 | `				length = 4;` |
|      8 | 4796 | `			}else{` |
|     23 | 4797 | `				zBuf = "INF";` |
|     23 | 4798 | `				length = 3;` |
|      - | 4799 | `			}` |
|     37 | 4800 | `			width = 0;` |
|     37 | 4801 | `			break;` |
|      - | 4802 | `		}` |
|    147 | 4803 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|    147 | 4804 | `		if( precision > 53 ){` |
|      - | 4805 | `			/* php's FORMAT_CONV_MAX_PRECISION cap, with the same E_NOTICE` |
|      - | 4806 | `			 * (message prefixed with the active function's name, like` |
|      - | 4807 | `			 * php_error_docref). */` |
|      - | 4808 | `			char zMsg[160];` |
|      4 | 4809 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - | 4810 | `				"%z(): Requested precision of %d digits was truncated to PHP maximum of %d digits",` |
|      2 | 4811 | `				&pCtx->pFunc->sName,precision,53);` |
|      3 | 4812 | `			PH7_VmThrowError(pCtx->pVm,0,E_NOTICE,zMsg);` |
|      3 | 4813 | `			precision = 53;` |
|      1 | 4814 | `		}` |
|      - | 4815 | ``		/* php's %f/%e extract the sign via `num < 0`, so negative zero prints`` |
|      - | 4816 | `		 * unsigned there — while %g (php_gcvt on the raw value) keeps "-0". */` |
|    147 | 4817 | `		if( xtype!=PH7_FMT_GENERIC && realvalue == 0.0 ){` |
|      9 | 4818 | `			realvalue = 0.0;` |
|      4 | 4819 | `		}` |
|      - | 4820 | `		/* php's float conversions are correctly rounded (zend_dtoa); use libc` |
|      - | 4821 | `		 * snprintf as the digit engine (the byte-exact-floats rule — the old` |
|      - | 4822 | `		 * hand-rolled vxGetdigit loop stopped at 16 significant digits, so` |
|      - | 4823 | `		 * e.g. %f of 1e308 printed zeros where php prints the exact binary64` |
|      - | 4824 | `		 * expansion), then post-process into php's exact shapes below. */` |
|    147 | 4825 | `		nFmt = 0;` |
|    147 | 4826 | `		zFmt[nFmt++] = '%';` |
|    147 | 4827 | `		if( flag_alternateform ) zFmt[nFmt++] = '#';` |
|      - | 4828 | `		/* php's ' ' flag selects space PADDING (its default), not C's` |
|      - | 4829 | `		 * space-for-positive-sign — so flag_blanksign is NOT forwarded. */` |
|    147 | 4830 | `		if( flag_plussign ) zFmt[nFmt++] = '+';` |
|    147 | 4831 | `		zFmt[nFmt++] = '.';` |
|    147 | 4832 | `		zFmt[nFmt++] = '*';` |
|    195 | 4833 | `		zFmt[nFmt++] = (char)(xtype==PH7_FMT_FLOAT ? 'f' :` |
|     32 | 4834 | `			(xtype==PH7_FMT_EXP ? ((pInfo->charset[0]=='E') ? 'E' : 'e')` |
|     32 | 4835 | `			                    : ((pInfo->charset[0]=='E') ? 'G' : 'g')));` |
|    147 | 4836 | `		zFmt[nFmt] = 0;` |
|    147 | 4837 | `		nOut = snprintf(zWorker,sizeof(zWorker),zFmt,precision,realvalue);` |
|    147 | 4838 | `		if( nOut < 0 \|\| nOut >= (int)sizeof(zWorker) ){` |
|      - | 4839 | `			/* Cannot happen with precision capped at 53 (%f of DBL_MAX is` |
|      - | 4840 | `			 * ~365 bytes); keep the truncated output rather than overrun. */` |
|    ! 0 | 4841 | `			nOut = (int)SyStrlen(zWorker);` |
|    ! 0 | 4842 | `		}` |
|    147 | 4843 | `		nOut = (int)PH7_PhpFloatShape(zWorker,(sxi32)nOut,xtype==PH7_FMT_GENERIC);` |
|    147 | 4844 | `		zBuf = zWorker;` |
|    147 | 4845 | `		length = nOut;` |
|      - | 4846 | `		/* Let the zero-pad block below insert zeros between the sign (written` |
|      - | 4847 | `		 * by snprintf) and the first digit, as before. */` |
|    147 | 4848 | `		prefix = (zWorker[0]=='-' \|\| zWorker[0]=='+' \|\| zWorker[0]==' ') ? zWorker[0] : 0;` |
|      - | 4849 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 4850 | `        ** set and we are not left justified */` |
|    147 | 4851 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 4852 | `          int i;` |
|      9 | 4853 | `          int nPad = width - length;` |
|     63 | 4854 | `          for(i=width; i>=nPad; i--){` |
|     55 | 4855 | `            zBuf[i] = zBuf[i-nPad];` |
|     28 | 4856 | `          }` |
|      9 | 4857 | `          i = prefix!=0;` |
|     39 | 4858 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      9 | 4859 | `          length = width;` |
|      4 | 4860 | `        }` |
|      - | 4861 | `#else` |
|      - | 4862 | `         zBuf = " ";` |
|      - | 4863 | `		 length = (int)sizeof(char);` |
|      - | 4864 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|    147 | 4865 | `		 break;` |
|      - | 4866 | `							 }` |
|    ! 0 | 4867 | `		default:` |
|      - | 4868 | `			/* Unreachable: PH7_FormatValidate() rejects unknown specifiers with a` |
|      - | 4869 | `			 * catchable ValueError before formatting begins. Kept as a defensive` |
|      - | 4870 | `			 * no-op that emits nothing. */` |
|    ! 0 | 4871 | `			length = 0;` |
|    ! 0 | 4872 | `			break;` |
|      - | 4873 | `		}` |
|      - | 4874 | `		 /*` |
|      - | 4875 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 4876 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 4877 | `		 ** the output.` |
|      - | 4878 | `		 */` |
|    907 | 4879 | `    if( !flag_leftjustify ){` |
|      - | 4880 | `      register int nspace;` |
|    889 | 4881 | `      nspace = width-length;` |
|    889 | 4882 | `      if( nspace>0 ){` |
|     37 | 4883 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4884 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4885 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4886 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4887 | `			}` |
|    ! 0 | 4888 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4889 | `        }` |
|     37 | 4890 | `        if( nspace>0 ){` |
|     37 | 4891 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|     37 | 4892 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4893 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4894 | `			}` |
|     18 | 4895 | `		}` |
|     18 | 4896 | `      }` |
|    443 | 4897 | `    }` |
|    907 | 4898 | `    if( length>0 ){` |
|    899 | 4899 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    899 | 4900 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 4901 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4902 | `		}` |
|    448 | 4903 | `    }` |
|    907 | 4904 | `    if( flag_leftjustify ){` |
|      - | 4905 | `      register int nspace;` |
|     19 | 4906 | `      nspace = width-length;` |
|     19 | 4907 | `      if( nspace>0 ){` |
|     15 | 4908 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4909 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4910 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4911 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4912 | `			}` |
|    ! 0 | 4913 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4914 | `        }` |
|     15 | 4915 | `        if( nspace>0 ){` |
|     15 | 4916 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|     15 | 4917 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4918 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4919 | `			}` |
|      7 | 4920 | `		}` |
|      7 | 4921 | `      }` |
|      9 | 4922 | `    }` |
|      3 | 4923 | ` }/* for(;;) */` |
|    463 | 4924 | `	return SXRET_OK;` |
|    233 | 4925 | `}` |
|      - | 4926 | `/*` |
|      - | 4927 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 4928 | ` */` |
|    534 | 4929 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      3 | 4930 | `{` |
|      - | 4931 | `	/* pUserData points to the caller's allocation-rc slot so an OOM during the` |
|      - | 4932 | `	 * result append is surfaced (the builtin raises a fatal); returning the` |
|      - | 4933 | `	 * non-OK rc also stops the format loop. */` |
|    537 | 4934 | `	sxi32 *pRc = (sxi32 *)pUserData;` |
|    537 | 4935 | `	*pRc = ph7_result_string(pCtx,zInput,nLen);` |
|    537 | 4936 | `	return *pRc;` |
|      3 | 4937 | `}` |
|      - | 4938 | `/*` |
|      - | 4939 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 4940 | ` *  Return a formatted string.` |
|      - | 4941 | ` * Parameters` |
|      - | 4942 | ` *  $format` |
|      - | 4943 | ` *    The format string (see block comment above)` |
|      - | 4944 | ` * Return` |
|      - | 4945 | ` *  A string produced according to the formatting string format.` |
|      - | 4946 | ` */` |
|    254 | 4947 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4948 | `{` |
|      - | 4949 | `	const char *zFormat;` |
|    257 | 4950 | `	sxi32 rc = SXRET_OK;` |
|      - | 4951 | `	int nLen;` |
|    257 | 4952 | `	if( nArg < 1 ){` |
|      - | 4953 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4954 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4955 | `		return PH7_OK;` |
|      - | 4956 | `	}` |
|      - | 4957 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|    257 | 4958 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    257 | 4959 | `	if( rc != PH7_OK ){` |
|    ! 0 | 4960 | `		return rc;` |
|      - | 4961 | `	}` |
|      - | 4962 | `	/* Extract the string format (scalars/null coerce). */` |
|    257 | 4963 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    257 | 4964 | `	if( nLen < 1 ){` |
|      - | 4965 | `		/* Empty string */` |
|    ! 0 | 4966 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4967 | `		return PH7_OK;` |
|      - | 4968 | `	}` |
|      - | 4969 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4970 | `	 * output; propagate the throw status verbatim. */` |
|    257 | 4971 | `	rc = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    257 | 4972 | `	if( rc != PH7_OK ){` |
|     17 | 4973 | `		return rc;` |
|      - | 4974 | `	}` |
|      - | 4975 | `	/* PHP 8: too few value arguments is a catchable ArgumentCountError before output. */` |
|    241 | 4976 | `	rc = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg-1,1,FALSE);` |
|    241 | 4977 | `	if( rc != PH7_OK ){` |
|     11 | 4978 | `		return rc;` |
|      - | 4979 | `	}` |
|      - | 4980 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|    231 | 4981 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&rc,FALSE);` |
|    231 | 4982 | `	if( rc != SXRET_OK ){` |
|      - | 4983 | `		/* The result append ran out of memory: raise a fatal rather than` |
|      - | 4984 | `		 * returning a silently-truncated string. */` |
|    ! 0 | 4985 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4986 | `	}` |
|    231 | 4987 | `	return PH7_OK;` |
|    130 | 4988 | `}` |
|      - | 4989 | `/*` |
|      - | 4990 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 4991 | ` */` |
|   1174 | 4992 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4993 | `{` |
|   1175 | 4994 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 4995 | `	/* Call the VM output consumer directly */` |
|   1175 | 4996 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 4997 | `	/* Increment counter */` |
|   1175 | 4998 | `	*pCounter += nLen;` |
|   1175 | 4999 | `	return PH7_OK;` |
|      1 | 5000 | `}` |
|      - | 5001 | `/*` |
|      - | 5002 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 5003 | ` *  Output a formatted string.` |
|      - | 5004 | ` * Parameters` |
|      - | 5005 | ` *  $format` |
|      - | 5006 | ` *   See sprintf() for a description of format.` |
|      - | 5007 | ` * Return` |
|      - | 5008 | ` *  The length of the outputted string.` |
|      - | 5009 | ` */` |
|    206 | 5010 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5011 | `{` |
|    207 | 5012 | `	ph7_int64 nCounter = 0;` |
|      - | 5013 | `	const char *zFormat;` |
|      - | 5014 | `	int nLen;` |
|    207 | 5015 | `	if( nArg < 1 ){` |
|      - | 5016 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 5017 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5018 | `		return PH7_OK;` |
|      - | 5019 | `	}` |
|      - | 5020 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|      - | 5021 | `	{` |
|    207 | 5022 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    207 | 5023 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 5024 | `			return rcf;` |
|      - | 5025 | `		}` |
|      - | 5026 | `	}` |
|      - | 5027 | `	/* Extract the string format (scalars/null coerce). */` |
|    207 | 5028 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    207 | 5029 | `	if( nLen < 1 ){` |
|      - | 5030 | `		/* Empty string */` |
|    ! 0 | 5031 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5032 | `		return PH7_OK;` |
|      - | 5033 | `	}` |
|      - | 5034 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 5035 | `	 * output; propagate the throw status verbatim. */` |
|      - | 5036 | `	{` |
|    207 | 5037 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    207 | 5038 | `		if( rcv != PH7_OK ){` |
|    ! 0 | 5039 | `			return rcv;` |
|      - | 5040 | `		}` |
|      - | 5041 | `		/* PHP 8: too few value arguments is a catchable ArgumentCountError before output. */` |
|    207 | 5042 | `		rcv = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg-1,1,FALSE);` |
|    207 | 5043 | `		if( rcv != PH7_OK ){` |
|      3 | 5044 | `			return rcv;` |
|      - | 5045 | `		}` |
|      - | 5046 | `	}` |
|      - | 5047 | `	/* Format the string */` |
|    205 | 5048 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 5049 | `	/* Return the length of the outputted string */` |
|    205 | 5050 | `	ph7_result_int64(pCtx,nCounter);` |
|    205 | 5051 | `	return PH7_OK;` |
|    104 | 5052 | `}` |
|      - | 5053 | `/*` |
|      - | 5054 | ` * int vprintf(string $format,array $args)` |
|      - | 5055 | ` *  Output a formatted string.` |
|      - | 5056 | ` * Parameters` |
|      - | 5057 | ` *  $format` |
|      - | 5058 | ` *   See sprintf() for a description of format.` |
|      - | 5059 | ` * Return` |
|      - | 5060 | ` *  The length of the outputted string.` |
|      - | 5061 | ` */` |
|      4 | 5062 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5063 | `{` |
|      5 | 5064 | `	ph7_int64 nCounter = 0;` |
|      - | 5065 | `	const char *zFormat;` |
|      - | 5066 | `	ph7_hashmap *pMap;` |
|      - | 5067 | `	SySet sArg;` |
|      - | 5068 | `	int nLen,n;` |
|      - | 5069 | `	sxi32 rcFmt;` |
|      5 | 5070 | `	if( nArg < 2 ){` |
|      - | 5071 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 5072 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5073 | `		return PH7_OK;` |
|      - | 5074 | `	}` |
|      - | 5075 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|      5 | 5076 | `	rcFmt = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|      5 | 5077 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 5078 | `		return rcFmt;` |
|      - | 5079 | `	}` |
|      5 | 5080 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 5081 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 5082 | `		char zBuf[64];` |
|      4 | 5083 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5084 | `			"vprintf(): Argument #2 ($values) must be of type array, %s given",` |
|      2 | 5085 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 5086 | `	}` |
|      - | 5087 | `	/* Extract the string format (scalars/null coerce). */` |
|      3 | 5088 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5089 | `	if( nLen < 1 ){` |
|      - | 5090 | `		/* Empty string */` |
|    ! 0 | 5091 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5092 | `		return PH7_OK;` |
|      - | 5093 | `	}` |
|      - | 5094 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 5095 | `	 * output; propagate the throw status verbatim. */` |
|      3 | 5096 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      3 | 5097 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 5098 | `		return rcFmt;` |
|      - | 5099 | `	}` |
|      - | 5100 | `	/* Point to the hashmap */` |
|      3 | 5101 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 5102 | `	/* PHP 8: too few items in the $values array is a catchable ValueError before output.` |
|      - | 5103 | `	 * Checked on the entry count before materialising the value set. */` |
|      3 | 5104 | `	rcFmt = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);` |
|      3 | 5105 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 5106 | `		return rcFmt;` |
|      - | 5107 | `	}` |
|      - | 5108 | `	/* Extract arguments from the hashmap */` |
|      3 | 5109 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 5110 | `	/* Format the string */` |
|      3 | 5111 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 5112 | `	/* Release the container */` |
|      3 | 5113 | `	SySetRelease(&sArg);` |
|      - | 5114 | `	/* Return the length of the outputted string */` |
|      3 | 5115 | `	ph7_result_int64(pCtx,nCounter);` |
|      3 | 5116 | `	return PH7_OK;` |
|      3 | 5117 | `}` |
|      - | 5118 | `/*` |
|      - | 5119 | ` * int vsprintf(string $format,array $args)` |
|      - | 5120 | ` *  Output a formatted string.` |
|      - | 5121 | ` * Parameters` |
|      - | 5122 | ` *  $format` |
|      - | 5123 | ` *   See sprintf() for a description of format.` |
|      - | 5124 | ` * Return` |
|      - | 5125 | ` *  A string produced according to the formatting string format.` |
|      - | 5126 | ` */` |
|     24 | 5127 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5128 | `{` |
|      - | 5129 | `	const char *zFormat;` |
|      - | 5130 | `	ph7_hashmap *pMap;` |
|      - | 5131 | `	SySet sArg;` |
|     25 | 5132 | `	sxi32 rc = SXRET_OK;` |
|      - | 5133 | `	sxi32 rcFmt;` |
|      - | 5134 | `	int nLen,n;` |
|     25 | 5135 | `	if( nArg < 2 ){` |
|      - | 5136 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5137 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5138 | `		return PH7_OK;` |
|      - | 5139 | `	}` |
|      - | 5140 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|     25 | 5141 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|     25 | 5142 | `	if( rc != PH7_OK ){` |
|    ! 0 | 5143 | `		return rc;` |
|      - | 5144 | `	}` |
|     25 | 5145 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 5146 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 5147 | `		char zBuf[64];` |
|     16 | 5148 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5149 | `			"vsprintf(): Argument #2 ($values) must be of type array, %s given",` |
|     10 | 5150 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 5151 | `	}` |
|      - | 5152 | `	/* Extract the string format (scalars/null coerce). */` |
|     15 | 5153 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 5154 | `	if( nLen < 1 ){` |
|      - | 5155 | `		/* Empty string */` |
|    ! 0 | 5156 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5157 | `		return PH7_OK;` |
|      - | 5158 | `	}` |
|      - | 5159 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 5160 | `	 * output; propagate the throw status verbatim. */` |
|     15 | 5161 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|     15 | 5162 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 5163 | `		return rcFmt;` |
|      - | 5164 | `	}` |
|      - | 5165 | `	/* Point to hashmap */` |
|     15 | 5166 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 5167 | `	/* PHP 8: too few items in the $values array is a catchable ValueError before output. */` |
|     15 | 5168 | `	rcFmt = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);` |
|     15 | 5169 | `	if( rcFmt != PH7_OK ){` |
|      5 | 5170 | `		return rcFmt;` |
|      - | 5171 | `	}` |
|      - | 5172 | `	/* Extract arguments from the hashmap */` |
|     11 | 5173 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 5174 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|     11 | 5175 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&rc,TRUE);` |
|      - | 5176 | `	/* Release the container */` |
|     11 | 5177 | `	SySetRelease(&sArg);` |
|     11 | 5178 | `	if( rc != SXRET_OK ){` |
|      - | 5179 | `		/* The result append ran out of memory: raise a fatal. */` |
|    ! 0 | 5180 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5181 | `	}` |
|     11 | 5182 | `	return PH7_OK;` |
|     13 | 5183 | `}` |
|      - | 5184 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 5185 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 5186 | `/*` |
|      - | 5187 | ` * Symisc eXtension.` |
|      - | 5188 | ` * string size_format(int64 $size)` |
|      - | 5189 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 5190 | ` *  Example:` |
|      - | 5191 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 5192 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 5193 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 5194 | ` * Parameter` |
|      - | 5195 | ` *  $size` |
|      - | 5196 | ` *    Entity size in bytes.` |
|      - | 5197 | ` * Return` |
|      - | 5198 | ` *   Formatted string representation of the given size.` |
|      - | 5199 | ` */` |
|     24 | 5200 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5201 | `{` |
|      - | 5202 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 5203 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 5204 | `	sxi32 nRest,i_32;` |
|      - | 5205 | `	ph7_int64 iSize;` |
|     25 | 5206 | `	int c = -1; /* index in zUnit[] */` |
|      - | 5207 |  |
|     25 | 5208 | `	if( nArg < 1 ){` |
|      - | 5209 | `		/* Missing argument,return the empty string */` |
|      3 | 5210 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5211 | `		return PH7_OK;` |
|      - | 5212 | `	}` |
|      - | 5213 | `	/* Extract the given size */` |
|     23 | 5214 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 5215 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 5216 | `		/* Don't bother formatting,return immediately */` |
|      5 | 5217 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 5218 | `		return PH7_OK;` |
|      - | 5219 | `	}` |
|     19 | 5220 | `	for(;;){` |
|     39 | 5221 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 5222 | `		iSize >>= 10;` |
|     39 | 5223 | `		c++;` |
|     39 | 5224 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 5225 | `			break;` |
|      - | 5226 | `		}` |
|      1 | 5227 | `	}` |
|     19 | 5228 | `	nRest /= 100;` |
|     19 | 5229 | `	if( nRest > 9 ){` |
|    ! 0 | 5230 | `		nRest = 9;` |
|    ! 0 | 5231 | `	}` |
|     19 | 5232 | `	if( iSize > 999 ){` |
|    ! 0 | 5233 | `		c++;` |
|    ! 0 | 5234 | `		nRest = 9;` |
|    ! 0 | 5235 | `		iSize = 0;` |
|    ! 0 | 5236 | `	}` |
|     19 | 5237 | `	i_32 = (sxi32)iSize;` |
|      - | 5238 | `	/* Format */` |
|     19 | 5239 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 5240 | `	return PH7_OK;` |
|     13 | 5241 | `}` |
|      - | 5242 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 5243 | `/*` |
|      - | 5244 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 5245 | ` *   Calculate the md5 hash of a string.` |
|      - | 5246 | ` * Parameter` |
|      - | 5247 | ` *  $str` |
|      - | 5248 | ` *   Input string` |
|      - | 5249 | ` * $raw_output` |
|      - | 5250 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 5251 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 5252 | ` * Return` |
|      - | 5253 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 5254 | ` */` |
|     12 | 5255 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5256 | `{` |
|      - | 5257 | `	unsigned char zDigest[16];` |
|     13 | 5258 | `	int raw_output = FALSE;` |
|      - | 5259 | `	const void *pIn;` |
|      - | 5260 | `	int nLen;` |
|     13 | 5261 | `	if( nArg < 1 ){` |
|      - | 5262 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5263 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5264 | `		return PH7_OK;` |
|      - | 5265 | `	}` |
|      - | 5266 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 5267 | `	 * digest in PHP — d41d8cd9… — so it must NOT short-circuit). */` |
|     13 | 5268 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 5269 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 5270 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 5271 | `	}` |
|      - | 5272 | `	/* Compute the MD5 digest */` |
|     13 | 5273 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|     13 | 5274 | `	if( raw_output ){` |
|      - | 5275 | `		/* Output raw digest */` |
|      5 | 5276 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 5277 | `	}else{` |
|      - | 5278 | `		/* Perform a binary to hex conversion */` |
|      9 | 5279 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 5280 | `	}` |
|     13 | 5281 | `	return PH7_OK;` |
|      7 | 5282 | `}` |
|      - | 5283 | `/*` |
|      - | 5284 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 5285 | ` *   Calculate the sha1 hash of a string.` |
|      - | 5286 | ` * Parameter` |
|      - | 5287 | ` *  $str` |
|      - | 5288 | ` *   Input string` |
|      - | 5289 | ` * $raw_output` |
|      - | 5290 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 5291 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 5292 | ` * Return` |
|      - | 5293 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 5294 | ` */` |
|     10 | 5295 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5296 | `{` |
|      - | 5297 | `	unsigned char zDigest[20];` |
|     11 | 5298 | `	int raw_output = FALSE;` |
|      - | 5299 | `	const void *pIn;` |
|      - | 5300 | `	int nLen;` |
|     11 | 5301 | `	if( nArg < 1 ){` |
|      - | 5302 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5303 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5304 | `		return PH7_OK;` |
|      - | 5305 | `	}` |
|      - | 5306 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 5307 | `	 * digest in PHP — da39a3ee… — so it must NOT short-circuit). */` |
|     11 | 5308 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 5309 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 5310 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 5311 | `	}` |
|      - | 5312 | `	/* Compute the SHA1 digest */` |
|     11 | 5313 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|     11 | 5314 | `	if( raw_output ){` |
|      - | 5315 | `		/* Output raw digest */` |
|      5 | 5316 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 5317 | `	}else{` |
|      - | 5318 | `		/* Perform a binary to hex conversion */` |
|      7 | 5319 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 5320 | `	}` |
|     11 | 5321 | `	return PH7_OK;` |
|      6 | 5322 | `}` |
|      - | 5323 | `/*` |
|      - | 5324 | ` * int64 crc32(string $str)` |
|      - | 5325 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 5326 | ` * Parameter` |
|      - | 5327 | ` *  $str` |
|      - | 5328 | ` *   Input string` |
|      - | 5329 | ` * Return` |
|      - | 5330 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 5331 | ` */` |
|      2 | 5332 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5333 | `{` |
|      - | 5334 | `	const void *pIn;` |
|      - | 5335 | `	sxu32 nCRC;` |
|      - | 5336 | `	int nLen;` |
|      3 | 5337 | `	if( nArg < 1 ){` |
|      - | 5338 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 5339 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5340 | `		return PH7_OK;` |
|      - | 5341 | `	}` |
|      - | 5342 | `	/* Extract the input string */` |
|      3 | 5343 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5344 | `	if( nLen < 1 ){` |
|      - | 5345 | `		/* crc32("") is 0 in PHP, so this short-circuit is correct here — unlike` |
|      - | 5346 | `		 * md5()/sha1(), whose empty-string digests are non-zero. */` |
|    ! 0 | 5347 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5348 | `		return PH7_OK;` |
|      - | 5349 | `	}` |
|      - | 5350 | `	/* Calculate the sum */` |
|      3 | 5351 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 5352 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 5353 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 5354 | `	return PH7_OK;` |
|      2 | 5355 | `}` |
|      - | 5356 | `/*` |
|      - | 5357 | ` * The hash() family (hash/hash_hmac/hash_equals/hash_algos). Each algorithm is` |
|      - | 5358 | ` * described by a small record so one dispatch (and one generic HMAC) serves them` |
|      - | 5359 | ` * all. Thin adapters normalize the differing context types and the reversed` |
|      - | 5360 | ` * MD5Final argument order behind a uniform Init/Update/Final over a HashCtx union.` |
|      - | 5361 | ` */` |
|     11 | 5362 | `static void HashMd5Init(HashCtx *c){ MD5Init(&c->md5); }` |
|     15 | 5363 | `static void HashMd5Update(HashCtx *c,const unsigned char *d,unsigned int n){ MD5Update(&c->md5,d,n); }` |
|     11 | 5364 | `static void HashMd5Final(HashCtx *c,unsigned char *o){ MD5Final(o,&c->md5); }` |
|     11 | 5365 | `static void HashSha1Init(HashCtx *c){ SHA1Init(&c->sha1); }` |
|     15 | 5366 | `static void HashSha1Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA1Update(&c->sha1,d,n); }` |
|     11 | 5367 | `static void HashSha1Final(HashCtx *c,unsigned char *o){ SHA1Final(&c->sha1,o); }` |
|      9 | 5368 | `static void HashSha224Init(HashCtx *c){ SHA224Init(&c->sha256); }` |
|     33 | 5369 | `static void HashSha256Init(HashCtx *c){ SHA256Init(&c->sha256); }` |
|     57 | 5370 | `static void HashSha256Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA256Update(&c->sha256,d,n); }` |
|     41 | 5371 | `static void HashSha256Final(HashCtx *c,unsigned char *o){ SHA256Final(&c->sha256,o); }` |
|      9 | 5372 | `static void HashSha384Init(HashCtx *c){ SHA384Init(&c->sha512); }` |
|     15 | 5373 | `static void HashSha512Init(HashCtx *c){ SHA512Init(&c->sha512); }` |
|     27 | 5374 | `static void HashSha512Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA512Update(&c->sha512,d,n); }` |
|     23 | 5375 | `static void HashSha512Final(HashCtx *c,unsigned char *o){ SHA512Final(&c->sha512,o); }` |
|      - | 5376 | `typedef struct HashAlgo HashAlgo;` |
|      - | 5377 | `struct HashAlgo {` |
|      - | 5378 | `	const char *zName;   /* lowercase canonical name */` |
|      - | 5379 | `	int nDigestLen;      /* output bytes: 16/20/28/32/48/64 */` |
|      - | 5380 | `	int nBlockLen;       /* internal block bytes (for HMAC): 64 or 128 */` |
|      - | 5381 | `	void (*xInit)(HashCtx *);` |
|      - | 5382 | `	void (*xUpdate)(HashCtx *,const unsigned char *,unsigned int);` |
|      - | 5383 | `	void (*xFinal)(HashCtx *,unsigned char *);` |
|      - | 5384 | `};` |
|      - | 5385 | `static const HashAlgo aHashAlgo[] = {` |
|      - | 5386 | `	{ "md5",    16, 64,  HashMd5Init,    HashMd5Update,    HashMd5Final    },` |
|      - | 5387 | `	{ "sha1",   20, 64,  HashSha1Init,   HashSha1Update,   HashSha1Final   },` |
|      - | 5388 | `	{ "sha224", 28, 64,  HashSha224Init, HashSha256Update, HashSha256Final },` |
|      - | 5389 | `	{ "sha256", 32, 64,  HashSha256Init, HashSha256Update, HashSha256Final },` |
|      - | 5390 | `	{ "sha384", 48, 128, HashSha384Init, HashSha512Update, HashSha512Final },` |
|      - | 5391 | `	{ "sha512", 64, 128, HashSha512Init, HashSha512Update, HashSha512Final },` |
|      - | 5392 | `};` |
|      - | 5393 | `/* Case-insensitive algorithm lookup (PHP accepts 'SHA256' etc.). */` |
|     73 | 5394 | `static const HashAlgo * HashFindAlgo(const char *zName,int nLen){` |
|      - | 5395 | `	sxu32 i;` |
|    279 | 5396 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|    272 | 5397 | `		if( (int)SyStrlen(aHashAlgo[i].zName) == nLen` |
|    211 | 5398 | `			&& SyStrnicmp(aHashAlgo[i].zName,zName,(sxu32)nLen) == 0 ){` |
|     67 | 5399 | `			return &aHashAlgo[i];` |
|      - | 5400 | `		}` |
|    106 | 5401 | `	}` |
|      6 | 5402 | `	return 0;` |
|     38 | 5403 | `}` |
|      - | 5404 | `/*` |
|      - | 5405 | ` * string hash(string $algo,string $data[,bool $binary = false])` |
|      - | 5406 | ` *   Generate a hash value (message digest).` |
|      - | 5407 | ` */` |
|     54 | 5408 | `static int PH7_builtin_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5409 | `{` |
|      - | 5410 | `	const HashAlgo *pAlgo;` |
|      - | 5411 | `	const char *zAlgo,*zData;` |
|     56 | 5412 | `	int nAlgoLen,nDataLen,raw_output = FALSE;` |
|      - | 5413 | `	HashCtx sCtx;` |
|      - | 5414 | `	unsigned char zDigest[64];` |
|     56 | 5415 | `	if( nArg < 2 ){` |
|    ! 0 | 5416 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5417 | `			"hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5418 | `	}` |
|     56 | 5419 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     56 | 5420 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     56 | 5421 | `	if( pAlgo == 0 ){` |
|      3 | 5422 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5423 | `			"hash(): Argument #1 ($algo) must be a valid hashing algorithm");` |
|      - | 5424 | `	}` |
|     53 | 5425 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     53 | 5426 | `	if( nArg > 2 ){` |
|      9 | 5427 | `		raw_output = ph7_value_to_bool(apArg[2]);` |
|      4 | 5428 | `	}` |
|     53 | 5429 | `	pAlgo->xInit(&sCtx);` |
|     53 | 5430 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     53 | 5431 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     53 | 5432 | `	if( raw_output ){` |
|      9 | 5433 | `		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);` |
|      5 | 5434 | `	}else{` |
|     45 | 5435 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);` |
|      - | 5436 | `	}` |
|     53 | 5437 | `	return PH7_OK;` |
|     29 | 5438 | `}` |
|      - | 5439 | `/*` |
|      - | 5440 | ` * string hash_hmac(string $algo,string $data,string $key[,bool $binary = false])` |
|      - | 5441 | ` *   Generate a keyed hash value using the HMAC method (RFC 2104).` |
|      - | 5442 | ` */` |
|     16 | 5443 | `static int PH7_builtin_hash_hmac(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5444 | `{` |
|      - | 5445 | `	const HashAlgo *pAlgo;` |
|      - | 5446 | `	const char *zAlgo,*zData,*zKey;` |
|     18 | 5447 | `	int nAlgoLen,nDataLen,nKeyLen,raw_output = FALSE;` |
|      - | 5448 | `	HashCtx sCtx;` |
|      - | 5449 | `	unsigned char zKeyBlock[128],zIpad[128],zOpad[128],zInner[64],zDigest[64];` |
|      - | 5450 | `	int i,nBlock,nDigest;` |
|     18 | 5451 | `	if( nArg < 3 ){` |
|    ! 0 | 5452 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5453 | `			"hash_hmac() expects at least 3 arguments, %d given",nArg);` |
|      - | 5454 | `	}` |
|     18 | 5455 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     18 | 5456 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     18 | 5457 | `	if( pAlgo == 0 ){` |
|      3 | 5458 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5459 | `			"hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");` |
|      - | 5460 | `	}` |
|     15 | 5461 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     15 | 5462 | `	zKey = ph7_value_to_string(apArg[2],&nKeyLen);` |
|     15 | 5463 | `	if( nArg > 3 ){` |
|      3 | 5464 | `		raw_output = ph7_value_to_bool(apArg[3]);` |
|      1 | 5465 | `	}` |
|     15 | 5466 | `	nBlock = pAlgo->nBlockLen;` |
|     15 | 5467 | `	nDigest = pAlgo->nDigestLen;` |
|      - | 5468 | `	/* Reduce the key to a single block: hash it if longer than the block, then` |
|      - | 5469 | `	 * zero-pad (a short or empty key is just zero-padded). */` |
|     15 | 5470 | `	SyZero(zKeyBlock,sizeof(zKeyBlock));` |
|     15 | 5471 | `	if( nKeyLen > nBlock ){` |
|      3 | 5472 | `		pAlgo->xInit(&sCtx);` |
|      3 | 5473 | `		pAlgo->xUpdate(&sCtx,(const unsigned char *)zKey,(unsigned int)nKeyLen);` |
|      3 | 5474 | `		pAlgo->xFinal(&sCtx,zKeyBlock);` |
|     14 | 5475 | `	}else if( nKeyLen > 0 ){` |
|     11 | 5476 | `		SyMemcpy(zKey,zKeyBlock,(sxu32)nKeyLen);` |
|      5 | 5477 | `	}` |
|   1039 | 5478 | `	for( i = 0; i < nBlock; i++ ){` |
|   1025 | 5479 | `		zIpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x36);` |
|   1025 | 5480 | `		zOpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x5c);` |
|    513 | 5481 | `	}` |
|      - | 5482 | `	/* inner = H((key ^ ipad) \|\| data) */` |
|     15 | 5483 | `	pAlgo->xInit(&sCtx);` |
|     15 | 5484 | `	pAlgo->xUpdate(&sCtx,zIpad,(unsigned int)nBlock);` |
|     15 | 5485 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     15 | 5486 | `	pAlgo->xFinal(&sCtx,zInner);` |
|      - | 5487 | `	/* out = H((key ^ opad) \|\| inner) */` |
|     15 | 5488 | `	pAlgo->xInit(&sCtx);` |
|     15 | 5489 | `	pAlgo->xUpdate(&sCtx,zOpad,(unsigned int)nBlock);` |
|     15 | 5490 | `	pAlgo->xUpdate(&sCtx,zInner,(unsigned int)nDigest);` |
|     15 | 5491 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     15 | 5492 | `	if( raw_output ){` |
|      3 | 5493 | `		ph7_result_string(pCtx,(const char *)zDigest,nDigest);` |
|      2 | 5494 | `	}else{` |
|     13 | 5495 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)nDigest,HashConsumer,pCtx);` |
|      - | 5496 | `	}` |
|     15 | 5497 | `	return PH7_OK;` |
|     10 | 5498 | `}` |
|      - | 5499 | `/*` |
|      - | 5500 | ` * bool hash_equals(string $known_string,string $user_string)` |
|      - | 5501 | ` *   Timing-attack-safe string comparison.` |
|      - | 5502 | ` */` |
|     12 | 5503 | `static int PH7_builtin_hash_equals(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5504 | `{` |
|      - | 5505 | `	const char *zKnown,*zUser;` |
|      - | 5506 | `	int nKnown,nUser,i;` |
|     14 | 5507 | `	volatile unsigned char vDiff = 0;` |
|     14 | 5508 | `	if( nArg < 2 ){` |
|    ! 0 | 5509 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5510 | `			"hash_equals() expects exactly 2 arguments, %d given",nArg);` |
|      - | 5511 | `	}` |
|     14 | 5512 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      4 | 5513 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5514 | `			"hash_equals(): Argument #1 ($known_string) must be of type string, %s given",` |
|      1 | 5515 | `			ph7_type_name(apArg[0]));` |
|      - | 5516 | `	}` |
|     11 | 5517 | `	if( !ph7_value_is_string(apArg[1]) ){` |
|    ! 0 | 5518 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5519 | `			"hash_equals(): Argument #2 ($user_string) must be of type string, %s given",` |
|    ! 0 | 5520 | `			ph7_type_name(apArg[1]));` |
|      - | 5521 | `	}` |
|     11 | 5522 | `	zKnown = ph7_value_to_string(apArg[0],&nKnown);` |
|     11 | 5523 | `	zUser = ph7_value_to_string(apArg[1],&nUser);` |
|     11 | 5524 | `	if( nKnown != nUser ){` |
|      5 | 5525 | `		ph7_result_bool(pCtx,0);` |
|      5 | 5526 | `		return PH7_OK;` |
|      - | 5527 | `	}` |
|      - | 5528 | `	/* Constant-time: read every byte, never short-circuit. */` |
|     19 | 5529 | `	for( i = 0; i < nKnown; i++ ){` |
|     13 | 5530 | `		vDiff \|= (unsigned char)(zKnown[i] ^ zUser[i]);` |
|      7 | 5531 | `	}` |
|      7 | 5532 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|      7 | 5533 | `	return PH7_OK;` |
|      8 | 5534 | `}` |
|      - | 5535 | `/*` |
|      - | 5536 | ` * array hash_algos(void)` |
|      - | 5537 | ` *   Return a list of the registered hashing algorithms.` |
|      - | 5538 | ` */` |
|      2 | 5539 | `static int PH7_builtin_hash_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5540 | `{` |
|      - | 5541 | `	ph7_value *pArray,*pValue;` |
|      - | 5542 | `	sxu32 i;` |
|      1 | 5543 | `	SXUNUSED(nArg);` |
|      1 | 5544 | `	SXUNUSED(apArg);` |
|      3 | 5545 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 5546 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 5547 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 5548 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5549 | `		return PH7_OK;` |
|      - | 5550 | `	}` |
|     15 | 5551 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|     13 | 5552 | `		ph7_value_string(pValue,aHashAlgo[i].zName,-1);` |
|     13 | 5553 | `		ph7_array_add_elem(pArray,0 /* Automatic 0-based index */,pValue);` |
|     13 | 5554 | `		ph7_value_reset_string_cursor(pValue);` |
|      7 | 5555 | `	}` |
|      3 | 5556 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5557 | `	return PH7_OK;` |
|      2 | 5558 | `}` |
|      - | 5559 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 5560 | `/*` |
|      - | 5561 | ` * password_* (bcrypt). These live in ext/standard in real PHP — outside the` |
|      - | 5562 | ` * hash extension — so they are NOT guarded by PH7_DISABLE_HASH_FUNC.` |
|      - | 5563 | ` */` |
|      - | 5564 | `/*` |
|      - | 5565 | ` * Parse a bcrypt crypt string. Returns TRUE and fills *piCost when zHash is a` |
|      - | 5566 | ` * well-formed "$2?$NN$"+53-char bcrypt hash (60 bytes, valid minor, cost 4..31).` |
|      - | 5567 | ` */` |
|     40 | 5568 | `static int BcryptParseHash(const char *zHash,int nHash,int *piCost)` |
|      1 | 5569 | `{` |
|      - | 5570 | `	int iCost;` |
|     40 | 5571 | `	if( nHash != 60 \|\| zHash[0] != '$' \|\| zHash[1] != '2' \|\| zHash[3] != '$'` |
|     29 | 5572 | `		\|\| (zHash[2] != 'a' && zHash[2] != 'b' && zHash[2] != 'x' && zHash[2] != 'y') ){` |
|     13 | 5573 | `		return FALSE;` |
|      - | 5574 | `	}` |
|     29 | 5575 | `	if( zHash[4] < '0' \|\| zHash[4] > '9' \|\| zHash[5] < '0' \|\| zHash[5] > '9' \|\| zHash[6] != '$' ){` |
|    ! 0 | 5576 | `		return FALSE;` |
|      - | 5577 | `	}` |
|     29 | 5578 | `	iCost = (zHash[4]-'0')*10 + (zHash[5]-'0');` |
|     29 | 5579 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      3 | 5580 | `		return FALSE;` |
|      - | 5581 | `	}` |
|     27 | 5582 | `	if( piCost ){ *piCost = iCost; }` |
|     27 | 5583 | `	return TRUE;` |
|     21 | 5584 | `}` |
|      - | 5585 | `/*` |
|      - | 5586 | ` * TRUE if the $algo argument selects bcrypt: null (PASSWORD_DEFAULT) or the` |
|      - | 5587 | ` * "2y" id (PASSWORD_BCRYPT/PASSWORD_DEFAULT). bcrypt is the only supported algo.` |
|      - | 5588 | ` */` |
|     20 | 5589 | `static int BcryptIsBcryptAlgo(ph7_value *pAlgo)` |
|      3 | 5590 | `{` |
|     23 | 5591 | `	if( ph7_value_is_null(pAlgo) ){` |
|    ! 0 | 5592 | `		return TRUE;` |
|      - | 5593 | `	}` |
|     23 | 5594 | `	if( ph7_value_is_string(pAlgo) ){` |
|      - | 5595 | `		int nAlgo;` |
|     23 | 5596 | `		const char *zAlgo = ph7_value_to_string(pAlgo,&nAlgo);` |
|     23 | 5597 | `		return ( nAlgo == 2 && zAlgo[0] == '2' && zAlgo[1] == 'y' );` |
|      - | 5598 | `	}` |
|    ! 0 | 5599 | `	return FALSE;` |
|     13 | 5600 | `}` |
|      - | 5601 | `/*` |
|      - | 5602 | ` * bool\|string password_hash(string $password,string\|int\|null $algo[,array $options])` |
|      - | 5603 | ` *  Create a bcrypt hash of the password.` |
|      - | 5604 | ` */` |
|     16 | 5605 | `static int PH7_builtin_password_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 5606 | `{` |
|      - | 5607 | `	const char *zPwd;` |
|     19 | 5608 | `	int nPwd,iCost = 12;` |
|      - | 5609 | `	unsigned char aSalt[16];` |
|      - | 5610 | `	char zHash[60];` |
|     19 | 5611 | `	if( nArg < 2 ){` |
|    ! 0 | 5612 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5613 | `			"password_hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5614 | `	}` |
|     19 | 5615 | `	if( !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      3 | 5616 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5617 | `			"password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm");` |
|      - | 5618 | `	}` |
|      - | 5619 | `	/* cost from $options['cost'] (default 12). */` |
|     16 | 5620 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|     14 | 5621 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|     14 | 5622 | `		if( pCost ){ iCost = ph7_value_to_int(pCost); }` |
|      6 | 5623 | `	}` |
|     16 | 5624 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      4 | 5625 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 5626 | `			"Invalid bcrypt cost parameter specified: %d",iCost);` |
|      - | 5627 | `	}` |
|     13 | 5628 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     13 | 5629 | `	if( SyOSCSPRNG(aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 5630 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - | 5631 | `			"password_hash(): unable to gather sufficient entropy for the salt");` |
|      - | 5632 | `	}` |
|     13 | 5633 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zHash) != SXRET_OK ){` |
|    ! 0 | 5634 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5635 | `		return PH7_OK;` |
|      - | 5636 | `	}` |
|     13 | 5637 | `	ph7_result_string(pCtx,zHash,(int)sizeof(zHash));` |
|     13 | 5638 | `	return PH7_OK;` |
|     11 | 5639 | `}` |
|      - | 5640 | `/*` |
|      - | 5641 | ` * bool password_verify(string $password,string $hash)` |
|      - | 5642 | ` *  Verify a password against a bcrypt hash. Never throws on a malformed hash.` |
|      - | 5643 | ` */` |
|     28 | 5644 | `static int PH7_builtin_password_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5645 | `{` |
|      - | 5646 | `	const char *zPwd,*zHash;` |
|      - | 5647 | `	int nPwd,nHash,iCost,i;` |
|      - | 5648 | `	unsigned char aSalt[16];` |
|      - | 5649 | `	char zComputed[60];` |
|     29 | 5650 | `	volatile unsigned char vDiff = 0;` |
|     29 | 5651 | `	if( nArg < 2 ){` |
|    ! 0 | 5652 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5653 | `			"password_verify() expects exactly 2 arguments, %d given",nArg);` |
|      - | 5654 | `	}` |
|     29 | 5655 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     29 | 5656 | `	zHash = ph7_value_to_string(apArg[1],&nHash);` |
|     29 | 5657 | `	if( !BcryptParseHash(zHash,nHash,&iCost) ){` |
|     11 | 5658 | `		ph7_result_bool(pCtx,0);` |
|     11 | 5659 | `		return PH7_OK;` |
|      - | 5660 | `	}` |
|      - | 5661 | `	/* Recover the 16 salt bytes from the 22-char salt field [7..28]. */` |
|     19 | 5662 | `	if( SyBcryptB64Decode(&zHash[7],22,aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 5663 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5664 | `		return PH7_OK;` |
|      - | 5665 | `	}` |
|     19 | 5666 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zComputed) != SXRET_OK ){` |
|    ! 0 | 5667 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5668 | `		return PH7_OK;` |
|      - | 5669 | `	}` |
|      - | 5670 | `	/* Constant-time compare of the 31-char hash field [29..59] only — sidesteps` |
|      - | 5671 | `	 * salt re-canonicalisation and any "$2a"/"$2y" prefix difference. */` |
|    577 | 5672 | `	for( i = 29; i < 60; i++ ){` |
|    559 | 5673 | `		vDiff \|= (unsigned char)(zComputed[i] ^ zHash[i]);` |
|    280 | 5674 | `	}` |
|     19 | 5675 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|     19 | 5676 | `	return PH7_OK;` |
|     15 | 5677 | `}` |
|      - | 5678 | `/*` |
|      - | 5679 | ` * array password_get_info(string $hash)` |
|      - | 5680 | ` *  Return ["algo"=>id\|null, "algoName"=>name, "options"=>[...]].` |
|      - | 5681 | ` */` |
|      6 | 5682 | `static int PH7_builtin_password_get_info(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5683 | `{` |
|      7 | 5684 | `	const char *zHash = "";` |
|      7 | 5685 | `	int nHash,iCost = 0,bBcrypt = 0;` |
|      - | 5686 | `	ph7_value *pArray,*pOptions,*pVal;` |
|      7 | 5687 | `	if( nArg > 0 ){` |
|      7 | 5688 | `		zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 5689 | `		bBcrypt = BcryptParseHash(zHash,nHash,&iCost);` |
|      3 | 5690 | `	}` |
|      7 | 5691 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 5692 | `	pOptions = ph7_context_new_array(pCtx);` |
|      7 | 5693 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      7 | 5694 | `	if( pArray == 0 \|\| pOptions == 0 \|\| pVal == 0 ){` |
|    ! 0 | 5695 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5696 | `		return PH7_OK;` |
|      - | 5697 | `	}` |
|      7 | 5698 | `	if( bBcrypt ){` |
|      5 | 5699 | `		ph7_value_string(pVal,&zHash[1],2);            /* algo "2y"/"2a" */` |
|      5 | 5700 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      5 | 5701 | `		ph7_value_reset_string_cursor(pVal);` |
|      5 | 5702 | `		ph7_value_string(pVal,"bcrypt",(int)sizeof("bcrypt")-1);` |
|      5 | 5703 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      5 | 5704 | `		ph7_value_int(pVal,iCost);` |
|      5 | 5705 | `		ph7_array_add_strkey_elem(pOptions,"cost",pVal);` |
|      3 | 5706 | `	}else{` |
|      3 | 5707 | `		ph7_value_null(pVal);                          /* algo => null */` |
|      3 | 5708 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      3 | 5709 | `		ph7_value_string(pVal,"unknown",(int)sizeof("unknown")-1);` |
|      3 | 5710 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      - | 5711 | `	}` |
|      7 | 5712 | `	ph7_array_add_strkey_elem(pArray,"options",pOptions);` |
|      7 | 5713 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 5714 | `	return PH7_OK;` |
|      4 | 5715 | `}` |
|      - | 5716 | `/*` |
|      - | 5717 | ` * bool password_needs_rehash(string $hash,string\|int\|null $algo[,array $options])` |
|      - | 5718 | ` *  True if the hash was not made with the given algo/options.` |
|      - | 5719 | ` */` |
|      6 | 5720 | `static int PH7_builtin_password_needs_rehash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5721 | `{` |
|      - | 5722 | `	const char *zHash;` |
|      7 | 5723 | `	int nHash,iCost = 0,iWantCost = 12;` |
|      7 | 5724 | `	if( nArg < 2 ){` |
|    ! 0 | 5725 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5726 | `			"password_needs_rehash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5727 | `	}` |
|      7 | 5728 | `	zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 5729 | `	if( !BcryptParseHash(zHash,nHash,&iCost) \|\| !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      - | 5730 | `		/* A non-bcrypt hash, or a request for a different algo → needs rehash. */` |
|      3 | 5731 | `		ph7_result_bool(pCtx,1);` |
|      3 | 5732 | `		return PH7_OK;` |
|      - | 5733 | `	}` |
|      5 | 5734 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|      5 | 5735 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|      5 | 5736 | `		if( pCost ){ iWantCost = ph7_value_to_int(pCost); }` |
|      2 | 5737 | `	}` |
|      5 | 5738 | `	ph7_result_bool(pCtx,iCost != iWantCost);` |
|      5 | 5739 | `	return PH7_OK;` |
|      4 | 5740 | `}` |
|      - | 5741 | `/*` |
|      - | 5742 | ` * filter_var() — input validation and sanitization (the ext/filter API).` |
|      - | 5743 | ` *` |
|      - | 5744 | ` * Filter and flag identifiers (values match PHP 8.5; the constants themselves` |
|      - | 5745 | ` * are registered in constant.c). The validate filters are hand-rolled rather` |
|      - | 5746 | ` * than delegating to SyStrToInt64/SyStrToReal: the former silently skips leading` |
|      - | 5747 | ` * zeros and cannot signal overflow, and the latter treats ',' as a decimal point` |
|      - | 5748 | ` * unconditionally — neither matches PHP's filter semantics.` |
|      - | 5749 | ` */` |
|      - | 5750 | `#define FV_VALIDATE_INT     257` |
|      - | 5751 | `#define FV_VALIDATE_BOOLEAN 258` |
|      - | 5752 | `#define FV_VALIDATE_FLOAT   259` |
|      - | 5753 | `#define FV_VALIDATE_REGEXP  272` |
|      - | 5754 | `#define FV_VALIDATE_URL     273` |
|      - | 5755 | `#define FV_VALIDATE_EMAIL   274` |
|      - | 5756 | `#define FV_VALIDATE_IP      275` |
|      - | 5757 | `#define FV_VALIDATE_MAC     276` |
|      - | 5758 | `#define FV_VALIDATE_DOMAIN  277` |
|      - | 5759 | `#define FV_SANITIZE_SPECIAL_CHARS      515` |
|      - | 5760 | `#define FV_DEFAULT          516 /* == FILTER_UNSAFE_RAW: pass the value through */` |
|      - | 5761 | `#define FV_SANITIZE_EMAIL   517` |
|      - | 5762 | `#define FV_SANITIZE_URL     518` |
|      - | 5763 | `#define FV_SANITIZE_NUMBER_INT   519` |
|      - | 5764 | `#define FV_SANITIZE_NUMBER_FLOAT 520` |
|      - | 5765 | `#define FV_SANITIZE_FULL_SPECIAL_CHARS 522` |
|      - | 5766 | `#define FV_FLAG_ALLOW_OCTAL  1` |
|      - | 5767 | `#define FV_FLAG_ALLOW_HEX    2` |
|      - | 5768 | `#define FV_FLAG_STRIP_LOW    4` |
|      - | 5769 | `#define FV_FLAG_STRIP_HIGH   8` |
|      - | 5770 | `#define FV_FLAG_ENCODE_LOW   16` |
|      - | 5771 | `#define FV_FLAG_ENCODE_HIGH  32` |
|      - | 5772 | `#define FV_FLAG_ENCODE_AMP   64` |
|      - | 5773 | `#define FV_FLAG_NO_ENCODE_QUOTES 128` |
|      - | 5774 | `#define FV_FLAG_STRIP_BACKTICK   512` |
|      - | 5775 | `#define FV_FLAG_ALLOW_FRACTION   4096` |
|      - | 5776 | `#define FV_FLAG_ALLOW_THOUSAND   8192` |
|      - | 5777 | `#define FV_FLAG_ALLOW_SCIENTIFIC 16384` |
|      - | 5778 | `#define FV_FLAG_IPV4  1048576` |
|      - | 5779 | `#define FV_FLAG_IPV6  2097152` |
|      - | 5780 | `#define FV_NULL_ON_FAILURE 134217728` |
|      - | 5781 | `/* The subset of flags the UNSAFE_RAW/DEFAULT string filter (FvSanitizeString)` |
|      - | 5782 | ` * acts on: when none are set the filter is a verbatim pass-through, so FV_DEFAULT` |
|      - | 5783 | ` * can shortcut. Keep this in sync with FvSanitizeString's flag handling. */` |
|      - | 5784 | `#define FV_FLAG_STRING_MASK (FV_FLAG_STRIP_LOW\|FV_FLAG_STRIP_HIGH\|FV_FLAG_STRIP_BACKTICK \` |
|      - | 5785 | `                            \|FV_FLAG_ENCODE_LOW\|FV_FLAG_ENCODE_HIGH\|FV_FLAG_ENCODE_AMP)` |
|      - | 5786 |  |
|      - | 5787 | `/* Trim leading/trailing PHP whitespace, adjusting the (*pz,*pn) view in place.` |
|      - | 5788 | ` * SyisSpace (isspace) matches PHP's filter whitespace set " \t\n\r\v\f". */` |
|    153 | 5789 | `static void FvTrim(const char **pz,int *pn){` |
|    153 | 5790 | `	const char *z = *pz;` |
|    153 | 5791 | `	int n = *pn;` |
|    157 | 5792 | `	while( n>0 && SyisSpace((unsigned char)z[0]) ){ z++; n--; }` |
|    161 | 5793 | `	while( n>0 && SyisSpace((unsigned char)z[n-1]) ){ n--; }` |
|    153 | 5794 | `	*pz = z; *pn = n;` |
|    153 | 5795 | `}` |
|      - | 5796 | `/* FILTER_VALIDATE_INT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     57 | 5797 | `static int FvValidateInt(const char *z,int n,int flags,ph7_int64 *pOut){` |
|     57 | 5798 | `	int neg = 0, i;` |
|     57 | 5799 | `	sxu64 u = 0;` |
|     57 | 5800 | `	FvTrim(&z,&n);` |
|     57 | 5801 | `	if( n==0 ){ return 0; }` |
|     51 | 5802 | `	if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; n--; }` |
|     51 | 5803 | `	if( n==0 ){ return 0; }` |
|     49 | 5804 | `	if( (flags & FV_FLAG_ALLOW_HEX) && n>=2 && z[0]=='0' && (z[1]=='x'\|\|z[1]=='X') ){` |
|      3 | 5805 | `		z += 2; n -= 2;` |
|      3 | 5806 | `		if( n==0 ){ return 0; }` |
|      7 | 5807 | `		for( i=0; i<n; i++ ){` |
|      5 | 5808 | `			int h = SyHexToint((unsigned char)z[i]);` |
|      5 | 5809 | `			if( h<0 ){ return 0; }` |
|      5 | 5810 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)h)/16 ){ return 0; }` |
|      5 | 5811 | `			u = u*16 + (sxu64)h;` |
|      3 | 5812 | `		}` |
|     48 | 5813 | `	}else if( (flags & FV_FLAG_ALLOW_OCTAL) && z[0]=='0' ){` |
|      9 | 5814 | `		for( i=0; i<n; i++ ){` |
|      7 | 5815 | `			if( z[i]<'0' \|\| z[i]>'7' ){ return 0; }` |
|      7 | 5816 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/8 ){ return 0; }` |
|      7 | 5817 | `			u = u*8 + (sxu64)(z[i]-'0');` |
|      4 | 5818 | `		}` |
|      2 | 5819 | `	}else{` |
|     45 | 5820 | `		if( z[0]=='0' && n>1 ){ return 0; } /* a leading zero is rejected in base 10 */` |
|    201 | 5821 | `		for( i=0; i<n; i++ ){` |
|    173 | 5822 | `			if( !SyisDigit((unsigned char)z[i]) ){ return 0; }` |
|    161 | 5823 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/10 ){ return 0; }` |
|    161 | 5824 | `			u = u*10 + (sxu64)(z[i]-'0');` |
|     81 | 5825 | `		}` |
|      - | 5826 | `	}` |
|     33 | 5827 | `	if( neg ){` |
|      5 | 5828 | `		if( u > 0x8000000000000000ULL ){ return 0; }` |
|      5 | 5829 | `		*pOut = (ph7_int64)(0ULL - u); /* two's-complement negate in unsigned space */` |
|      3 | 5830 | `	}else{` |
|     29 | 5831 | `		if( u > 0x7FFFFFFFFFFFFFFFULL ){ return 0; }` |
|     27 | 5832 | `		*pOut = (ph7_int64)u;` |
|      - | 5833 | `	}` |
|     31 | 5834 | `	return 1;` |
|     29 | 5835 | `}` |
|      - | 5836 | `/* FILTER_VALIDATE_FLOAT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     69 | 5837 | `static int FvValidateFloat(const char *z,int n,int flags,double *pOut){` |
|      - | 5838 | `	char zBuf[512];` |
|     69 | 5839 | `	int i, m = 0, seenDigit = 0;` |
|     69 | 5840 | `	const char *zv; int nv; double d = 0;` |
|     69 | 5841 | `	FvTrim(&z,&n);` |
|      - | 5842 | `	/* Bound the input: zBuf[512] holds the thousand-separator-stripped copy, and` |
|      - | 5843 | `	 * the cap also rejects the pathological 500+ digit floats PHP refuses. */` |
|     69 | 5844 | `	if( n==0 \|\| n>500 ){ return 0; }` |
|     69 | 5845 | `	if( flags & FV_FLAG_ALLOW_THOUSAND ){` |
|      - | 5846 | `		/* Commas are optional, but when present they must group the integer part` |
|      - | 5847 | `		 * into a leading run of 1..3 digits followed by groups of exactly 3` |
|      - | 5848 | `		 * ("1,000" ok, "1,5"/"1234,567" rejected). Strip them into zBuf and reject` |
|      - | 5849 | `		 * a comma anywhere in the fractional/exponent tail. */` |
|     25 | 5850 | `		int s = 0, intEnd, segStart, segIdx, hasComma = 0;` |
|     25 | 5851 | `		if( s<n && (z[s]=='+'\|\|z[s]=='-') ){ zBuf[m++] = z[s]; s++; }` |
|     25 | 5852 | `		intEnd = s;` |
|    167 | 5853 | `		while( intEnd<n && z[intEnd]!='.' && z[intEnd]!='e' && z[intEnd]!='E' ){` |
|    143 | 5854 | `			if( z[intEnd]==',' ){ hasComma = 1; }` |
|    143 | 5855 | `			intEnd++;` |
|      1 | 5856 | `		}` |
|     25 | 5857 | `		if( hasComma ){` |
|     25 | 5858 | `			segStart = s; segIdx = 0;` |
|    165 | 5859 | `			for( i=s; i<=intEnd; i++ ){` |
|    151 | 5860 | `				if( i==intEnd \|\| z[i]==',' ){` |
|     49 | 5861 | `					int segLen = i - segStart, k;` |
|     49 | 5862 | `					if( segIdx==0 ){ if( segLen<1 \|\| segLen>3 ){ return 0; } }` |
|     25 | 5863 | `					else if( segLen!=3 ){ return 0; }` |
|    119 | 5864 | `					for( k=segStart; k<i; k++ ){` |
|     81 | 5865 | `						if( !SyisDigit((unsigned char)z[k]) ){ return 0; }` |
|     81 | 5866 | `						zBuf[m++] = z[k];` |
|     41 | 5867 | `					}` |
|     39 | 5868 | `					segStart = i+1; segIdx++;` |
|     19 | 5869 | `				}` |
|     71 | 5870 | `			}` |
|      8 | 5871 | `		}else{` |
|    ! 0 | 5872 | `			for( i=s; i<intEnd; i++ ){ zBuf[m++] = z[i]; }` |
|      - | 5873 | `		}` |
|     27 | 5874 | `		for( i=intEnd; i<n; i++ ){` |
|     13 | 5875 | `			if( z[i]==',' ){ return 0; }` |
|     13 | 5876 | `			zBuf[m++] = z[i];` |
|      7 | 5877 | `		}` |
|     15 | 5878 | `		zv = zBuf; nv = m;` |
|      8 | 5879 | `	}else{` |
|     45 | 5880 | `		zv = z; nv = n;` |
|      - | 5881 | `	}` |
|     59 | 5882 | `	i = 0;` |
|     59 | 5883 | `	if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|    167 | 5884 | `	while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     59 | 5885 | `	if( i<nv && zv[i]=='.' ){` |
|     21 | 5886 | `		i++;` |
|     39 | 5887 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     10 | 5888 | `	}` |
|     59 | 5889 | `	if( !seenDigit ){ return 0; }` |
|     57 | 5890 | `	if( i<nv && (zv[i]=='e'\|\|zv[i]=='E') ){` |
|     29 | 5891 | `		i++;` |
|     29 | 5892 | `		if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|     29 | 5893 | `		if( i>=nv \|\| !SyisDigit((unsigned char)zv[i]) ){ return 0; }` |
|    105 | 5894 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; }` |
|     14 | 5895 | `	}` |
|     57 | 5896 | `	if( i!=nv ){ return 0; } /* trailing junk */` |
|      - | 5897 | `	/* The grammar above guarantees zv[0..nv) is a clean ASCII decimal float (no hex /` |
|      - | 5898 | `	 * inf / nan / trailing junk), so it is safe to hand to libc strtod, which — unlike` |
|      - | 5899 | `	 * SyStrToReal (15 sig-digits + exponent clamped to 308, so it silently saturates` |
|      - | 5900 | `	 * overflowing magnitudes to a finite value) — is overflow/underflow-aware and` |
|      - | 5901 | `	 * correctly rounded. strtod needs a NUL-terminated string: the ALLOW_THOUSAND path` |
|      - | 5902 | `	 * already built the span in zBuf (zv==zBuf); the plain path must copy it there (z is` |
|      - | 5903 | `	 * const + not NUL-terminated). nv <= n <= 500 < sizeof(zBuf) by the cap above.` |
|      - | 5904 | `	 * Matches PHP 8.5 byte-for-byte: reject overflow (-> +/-INF) and total underflow` |
|      - | 5905 | `	 * (-> 0.0), keep subnormals (nonzero, errno==ERANGE) and a genuine "0" (errno==0). */` |
|     53 | 5906 | `	if( zv != zBuf ){ SyMemcpy(zv,zBuf,(sxu32)nv); }` |
|     53 | 5907 | `	zBuf[nv] = 0;` |
|     53 | 5908 | `	errno = 0;` |
|     53 | 5909 | `	d = strtod(zBuf,0);` |
|     53 | 5910 | `	if( errno == ERANGE && (d == HUGE_VAL \|\| d == -HUGE_VAL \|\| d == 0.0) ){` |
|     15 | 5911 | `		return 0;` |
|      - | 5912 | `	}` |
|     39 | 5913 | `	*pOut = d;` |
|     39 | 5914 | `	return 1;` |
|     35 | 5915 | `}` |
|      - | 5916 | `/* FILTER_VALIDATE_BOOLEAN. Returns 1 if the string is recognized (sets *pBool),` |
|      - | 5917 | ` * 0 if it is unrecognized (the failure path). "0"/"false"/"" are recognized as` |
|      - | 5918 | ` * false, NOT failures. */` |
|     33 | 5919 | `static int FvValidateBool(const char *z,int n,int *pBool){` |
|     33 | 5920 | `	FvTrim(&z,&n);` |
|     32 | 5921 | `	if( (n==1 && z[0]=='1') \|\| (n==4 && SyStrnicmp(z,"true",4)==0)` |
|     25 | 5922 | `	    \|\| (n==2 && SyStrnicmp(z,"on",2)==0) \|\| (n==3 && SyStrnicmp(z,"yes",3)==0) ){` |
|     11 | 5923 | `		*pBool = 1; return 1;` |
|      - | 5924 | `	}` |
|     22 | 5925 | `	if( n==0 \|\| (n==1 && z[0]=='0') \|\| (n==5 && SyStrnicmp(z,"false",5)==0)` |
|     11 | 5926 | `	    \|\| (n==3 && SyStrnicmp(z,"off",3)==0) \|\| (n==2 && SyStrnicmp(z,"no",2)==0) ){` |
|     11 | 5927 | `		*pBool = 0; return 1;` |
|      - | 5928 | `	}` |
|      9 | 5929 | `	return 0;` |
|     15 | 5930 | `}` |
|      - | 5931 | `/* IPv4 dotted-quad: exactly 4 octets 0..255, no leading zeros. */` |
|     33 | 5932 | `static int FvValidateIp4(const char *z,int n){` |
|     33 | 5933 | `	int i = 0, parts = 0;` |
|     77 | 5934 | `	while( i<n ){` |
|     65 | 5935 | `		int val = 0, digits = 0, start = i;` |
|    143 | 5936 | `		while( i<n && SyisDigit((unsigned char)z[i]) ){` |
|     85 | 5937 | `			val = val*10 + (z[i]-'0');` |
|     85 | 5938 | `			if( val>255 ){ return 0; }` |
|     79 | 5939 | `			digits++; i++;` |
|      1 | 5940 | `		}` |
|     59 | 5941 | `		if( digits==0 \|\| digits>3 ){ return 0; }` |
|     49 | 5942 | `		if( digits>1 && z[start]=='0' ){ return 0; } /* leading zero */` |
|     45 | 5943 | `		parts++;` |
|     45 | 5944 | `		if( parts>4 ){ return 0; }` |
|     45 | 5945 | `		if( i<n ){` |
|     33 | 5946 | `			if( z[i]!='.' ){ return 0; }` |
|     33 | 5947 | `			i++;` |
|     33 | 5948 | `			if( i>=n ){ return 0; } /* trailing dot */` |
|     16 | 5949 | `		}` |
|      1 | 5950 | `	}` |
|     13 | 5951 | `	return parts==4;` |
|     17 | 5952 | `}` |
|      - | 5953 | `/* A colon-separated run of IPv6 hextets with no "::" (n may be 0 -> 0 groups),` |
|      - | 5954 | ` * allowing a trailing embedded IPv4. Returns the 16-bit group count or -1. */` |
|     19 | 5955 | `static int FvIp6Hextets(const char *z,int n){` |
|     19 | 5956 | `	int i = 0, segStart = 0, groups = 0;` |
|     19 | 5957 | `	if( n==0 ){ return 0; }` |
|    145 | 5958 | `	while( i<=n ){` |
|    133 | 5959 | `		if( i==n \|\| z[i]==':' ){` |
|     23 | 5960 | `			int segLen = i - segStart, j, isV4 = 0;` |
|     23 | 5961 | `			if( segLen==0 ){ return -1; } /* an empty hextet (stray ':') */` |
|     77 | 5962 | `			for( j=segStart; j<i; j++ ){ if( z[j]=='.' ){ isV4 = 1; break; } }` |
|     23 | 5963 | `			if( isV4 ){` |
|     11 | 5964 | `				if( i!=n ){ return -1; } /* IPv4 only as the final token */` |
|     11 | 5965 | `				if( !FvValidateIp4(z+segStart,segLen) ){ return -1; }` |
|      5 | 5966 | `				groups += 2;` |
|      3 | 5967 | `			}else{` |
|     13 | 5968 | `				if( segLen>4 ){ return -1; }` |
|     47 | 5969 | `				for( j=segStart; j<i; j++ ){ if( SyHexToint((unsigned char)z[j])<0 ){ return -1; } }` |
|     13 | 5970 | `				groups++;` |
|      - | 5971 | `			}` |
|     17 | 5972 | `			segStart = i+1;` |
|      8 | 5973 | `		}` |
|    127 | 5974 | `		i++;` |
|      1 | 5975 | `	}` |
|     13 | 5976 | `	return groups;` |
|     10 | 5977 | `}` |
|      - | 5978 | `/* IPv6: at most one "::" zero-run; 8 groups exactly, or fewer when "::" present. */` |
|     19 | 5979 | `static int FvValidateIp6(const char *z,int n){` |
|     19 | 5980 | `	const char *zDbl = 0;` |
|      - | 5981 | `	int i, ga, gb;` |
|    139 | 5982 | `	for( i=0; i+1<n; i++ ){` |
|    123 | 5983 | `		if( z[i]==':' && z[i+1]==':' ){` |
|     13 | 5984 | `			if( zDbl ){ return 0; } /* a second "::" is invalid */` |
|     11 | 5985 | `			zDbl = z+i;` |
|      5 | 5986 | `		}` |
|     61 | 5987 | `	}` |
|     17 | 5988 | `	if( zDbl==0 ){` |
|      9 | 5989 | `		return FvIp6Hextets(z,n)==8;` |
|    ! 0 | 5990 | `	}else{` |
|      9 | 5991 | `		int lenA = (int)(zDbl - z);` |
|      9 | 5992 | `		int lenB = n - lenA - 2;` |
|      9 | 5993 | `		ga = (lenA==0) ? 0 : FvIp6Hextets(z,lenA);` |
|      9 | 5994 | `		gb = (lenB==0) ? 0 : FvIp6Hextets(zDbl+2,lenB);` |
|      9 | 5995 | `		if( ga<0 \|\| gb<0 ){ return 0; }` |
|      9 | 5996 | `		return (ga+gb)<=7; /* "::" stands for at least one zero group */` |
|      - | 5997 | `	}` |
|     10 | 5998 | `}` |
|     25 | 5999 | `static int FvValidateIp(const char *z,int n,int flags){` |
|     25 | 6000 | `	int v4 = (flags & FV_FLAG_IPV4), v6 = (flags & FV_FLAG_IPV6);` |
|     25 | 6001 | `	if( !v4 && !v6 ){ v4 = v6 = 1; } /* default accepts either family */` |
|     25 | 6002 | `	if( v4 && FvValidateIp4(z,n) ){ return 1; }` |
|     21 | 6003 | `	if( v6 && FvValidateIp6(z,n) ){ return 1; }` |
|     13 | 6004 | `	return 0;` |
|     13 | 6005 | `}` |
|      - | 6006 | `/* FILTER_VALIDATE_MAC: 17-char colon- or dash-separated hex (XX:XX:..:XX). */` |
|     11 | 6007 | `static int FvValidateMac(const char *z,int n){` |
|      - | 6008 | `	char sep;` |
|      - | 6009 | `	int i;` |
|     11 | 6010 | `	if( n!=17 ){ return 0; }` |
|      7 | 6011 | `	sep = z[2];` |
|      7 | 6012 | `	if( sep!=':' && sep!='-' ){ return 0; }` |
|    105 | 6013 | `	for( i=0; i<17; i++ ){` |
|    101 | 6014 | `		if( (i%3)==2 ){ if( z[i]!=sep ){ return 0; } }` |
|     71 | 6015 | `		else if( SyHexToint((unsigned char)z[i])<0 ){ return 0; }` |
|     50 | 6016 | `	}` |
|      5 | 6017 | `	return 1;` |
|      6 | 6018 | `}` |
|      - | 6019 | `/* FILTER_VALIDATE_EMAIL (best-effort: covers the common cases, not quoted local` |
|      - | 6020 | ` * parts or IP-literal domains). */` |
|     28 | 6021 | `static int FvValidateEmail(const char *z,int n){` |
|     28 | 6022 | `	int at = -1, i, localLen, domLen, labelStart, dotCount = 0;` |
|      - | 6023 | `	const char *zDom;` |
|     28 | 6024 | `	if( n==0 \|\| n>320 ){ return 0; }` |
|    201 | 6025 | `	for( i=0; i<n; i++ ){` |
|    181 | 6026 | `		if( z[i]=='@' ){ if( at>=0 ){ return 0; } at = i; }` |
|     91 | 6027 | `	}` |
|     21 | 6028 | `	if( at<=0 \|\| at==n-1 ){ return 0; } /* one '@', non-empty local and domain */` |
|     21 | 6029 | `	localLen = at;` |
|     21 | 6030 | `	zDom = z + at + 1;` |
|     21 | 6031 | `	domLen = n - at - 1;` |
|     21 | 6032 | `	if( z[0]=='.' \|\| z[at-1]=='.' ){ return 0; }` |
|     57 | 6033 | `	for( i=0; i<localLen; i++ ){` |
|     43 | 6034 | `		unsigned char c = (unsigned char)z[i];` |
|     43 | 6035 | `		if( c<=' ' ){ return 0; }` |
|     41 | 6036 | `		if( c=='.' && i+1<localLen && z[i+1]=='.' ){ return 0; }` |
|     20 | 6037 | `	}` |
|     15 | 6038 | `	if( zDom[0]=='.' \|\| zDom[domLen-1]=='.' ){ return 0; }` |
|     13 | 6039 | `	labelStart = 0;` |
|     85 | 6040 | `	for( i=0; i<=domLen; i++ ){` |
|     75 | 6041 | `		if( i==domLen \|\| zDom[i]=='.' ){` |
|     25 | 6042 | `			int ll = i - labelStart;` |
|     25 | 6043 | `			if( ll==0 ){ return 0; } /* consecutive dots */` |
|     23 | 6044 | `			if( zDom[labelStart]=='-' \|\| zDom[i-1]=='-' ){ return 0; }` |
|     23 | 6045 | `			if( i<domLen ){ dotCount++; }` |
|     23 | 6046 | `			labelStart = i+1;` |
|     12 | 6047 | `		}else{` |
|     51 | 6048 | `			unsigned char c = (unsigned char)zDom[i];` |
|     51 | 6049 | `			if( !((c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9')\|\|c=='-') ){ return 0; }` |
|      - | 6050 | `		}` |
|     37 | 6051 | `	}` |
|     11 | 6052 | `	if( dotCount<1 ){ return 0; } /* PHP requires a dot in the domain (any TLD length) */` |
|      9 | 6053 | `	return 1;` |
|     15 | 6054 | `}` |
|      - | 6055 | `/* FILTER_VALIDATE_DOMAIN (lenient, matching PHP without FILTER_FLAG_HOSTNAME). */` |
|     11 | 6056 | `static int FvValidateDomain(const char *z,int n){` |
|      - | 6057 | `	int i;` |
|     11 | 6058 | `	if( n<1 \|\| n>253 \|\| z[0]=='.' ){ return 0; }` |
|     81 | 6059 | `	for( i=0; i<n; i++ ){` |
|     75 | 6060 | `		unsigned char c = (unsigned char)z[i];` |
|     75 | 6061 | `		if( c<=' ' ){ return 0; }` |
|     75 | 6062 | `		if( c=='.' && i+1<n && z[i+1]=='.' ){ return 0; }` |
|     37 | 6063 | `	}` |
|      7 | 6064 | `	return 1;` |
|      6 | 6065 | `}` |
|      - | 6066 | `/* FILTER_VALIDATE_URL: require a scheme and a host (PHP's filter is itself` |
|      - | 6067 | ` * parse_url-based, so PH7_VmHttpSplitURI tracks it closely). */` |
|     15 | 6068 | `static int FvValidateUrl(const char *z,int n){` |
|      - | 6069 | `	SyhttpUri sUri;` |
|     15 | 6070 | `	if( n==0 ){ return 0; }` |
|     15 | 6071 | `	SyZero(&sUri,(sxu32)sizeof(sUri));` |
|     15 | 6072 | `	if( PH7_VmHttpSplitURI(&sUri,z,(sxu32)n)!=SXRET_OK ){ return 0; }` |
|     15 | 6073 | `	return sUri.sScheme.nByte!=0 && sUri.sHost.nByte!=0;` |
|      8 | 6074 | `}` |
|      - | 6075 | `/* The Fv sanitizers build their result by appending directly to the call` |
|      - | 6076 | ` * context (ph7_result_string accumulates, like htmlspecialchars), emitting each` |
|      - | 6077 | ` * kept run in one call and seeding "" so an all-stripped input yields "". */` |
|      - | 6078 | `/* SANITIZE_NUMBER_INT (isFloat=0) / SANITIZE_NUMBER_FLOAT (isFloat=1). */` |
|     37 | 6079 | `static void FvSanitizeNumber(ph7_context *pCtx,const char *z,int n,int isFloat,int flags){` |
|     37 | 6080 | `	int i, runStart = 0;` |
|     37 | 6081 | `	ph7_result_string(pCtx,"",0);` |
|     97 | 6082 | `	for( i=0; i<n; i++ ){` |
|     91 | 6083 | `		char c = z[i];` |
|     91 | 6084 | `		int keep = (c>='0'&&c<='9') \|\| c=='+' \|\| c=='-';` |
|     91 | 6085 | `		if( !keep && isFloat ){` |
|     38 | 6086 | `			keep = (c=='.' && (flags & FV_FLAG_ALLOW_FRACTION))` |
|     23 | 6087 | `			    \|\| (c==',' && (flags & FV_FLAG_ALLOW_THOUSAND))` |
|     36 | 6088 | `			    \|\| ((c=='e'\|\|c=='E') && (flags & FV_FLAG_ALLOW_SCIENTIFIC));` |
|     12 | 6089 | `		}` |
|     61 | 6090 | `		if( !keep ){` |
|     33 | 6091 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     33 | 6092 | `			runStart = i+1;` |
|     16 | 6093 | `		}` |
|     31 | 6094 | `	}` |
|      7 | 6095 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      7 | 6096 | `}` |
|      - | 6097 | `/* Return non-zero when byte c must be stripped under the STRIP_* flags. Shared` |
|      - | 6098 | ` * by the UNSAFE_RAW string filter and SANITIZE_SPECIAL_CHARS. STRIP_LOW drops` |
|      - | 6099 | `` * bytes <32, STRIP_HIGH drops bytes >=127 (incl. DEL), STRIP_BACKTICK drops '`'.`` |
|      - | 6100 | ` * Matches php_filter_strip(); verified byte-exact vs php 8.5.7. */` |
|    287 | 6101 | `static int FvStripByte(unsigned char c,int flags){` |
|    287 | 6102 | `	if( (flags & FV_FLAG_STRIP_LOW)      && c<32 )    { return 1; }` |
|    281 | 6103 | `	if( (flags & FV_FLAG_STRIP_HIGH)     && c>=127 )  { return 1; }` |
|    269 | 6104 | `	if( (flags & FV_FLAG_STRIP_BACKTICK) && c==0x60 ) { return 1; }` |
|    267 | 6105 | `	return 0;` |
|    144 | 6106 | `}` |
|      - | 6107 | `/* FILTER_UNSAFE_RAW / FILTER_DEFAULT with flags: no default transform, but the` |
|      - | 6108 | ` * STRIP/ENCODE flags apply. Precedence (per php_filter_unsafe_raw, verified` |
|      - | 6109 | ` * vs php 8.5.7): a byte is first tested for stripping; a surviving byte is then` |
|      - | 6110 | ` * encoded as a decimal numeric entity if ENCODE_LOW (<32) / ENCODE_HIGH (>=127)` |
|      - | 6111 | ` * is set, and '&' becomes "&#38;" under ENCODE_AMP. So STRIP_LOW\|ENCODE_LOW` |
|      - | 6112 | ` * strips (nothing left to encode). Bytes are treated individually — ENCODE_HIGH` |
|      - | 6113 | ` * numeric-encodes each byte of a multibyte sequence separately, not the codepoint. */` |
|     25 | 6114 | `static void FvSanitizeString(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 6115 | `	int i, runStart = 0;` |
|     25 | 6116 | `	ph7_result_string(pCtx,"",0);` |
|    193 | 6117 | `	for( i=0; i<n; i++ ){` |
|    179 | 6118 | `		unsigned char c = (unsigned char)z[i];` |
|    179 | 6119 | `		if( FvStripByte(c,flags) ){` |
|     13 | 6120 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     13 | 6121 | `			runStart = i+1;` |
|     13 | 6122 | `			continue;` |
|      - | 6123 | `		}` |
|    167 | 6124 | `		if( c=='&' && (flags & FV_FLAG_ENCODE_AMP) ){` |
|      3 | 6125 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      3 | 6126 | `			ph7_result_string(pCtx,"&#38;",-1);` |
|      3 | 6127 | `			runStart = i+1;` |
|    166 | 6128 | `		}else if( (c<32 && (flags & FV_FLAG_ENCODE_LOW))` |
|    164 | 6129 | `		       \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     37 | 6130 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 6131 | `			ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|      9 | 6132 | `			runStart = i+1;` |
|      4 | 6133 | `		}` |
|     79 | 6134 | `	}` |
|     15 | 6135 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     15 | 6136 | `}` |
|      - | 6137 | `/* FILTER_SANITIZE_SPECIAL_CHARS: encode <>&"' and every control byte <32 as a` |
|      - | 6138 | ` * decimal numeric entity (&#60; &#38; &#34; ...). The STRIP_* flags remove bytes` |
|      - | 6139 | ` * before encoding; ENCODE_HIGH numeric-encodes surviving bytes >=127. Bytes >=128` |
|      - | 6140 | ` * are otherwise passed through verbatim (this filter is NOT UTF-8-aware — only the` |
|      - | 6141 | ` * FULL variant is). Byte-exact vs php 8.5.7. */` |
|     13 | 6142 | `static void FvSanitizeSpecial(ph7_context *pCtx,const char *z,int n,int flags){` |
|     13 | 6143 | `	int i, runStart = 0;` |
|      - | 6144 | `	const char *zEnt;` |
|     13 | 6145 | `	ph7_result_string(pCtx,"",0);` |
|    131 | 6146 | `	for( i=0; i<n; i++ ){` |
|    119 | 6147 | `		unsigned char c = (unsigned char)z[i];` |
|    119 | 6148 | `		if( FvStripByte(c,flags) ){` |
|      9 | 6149 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 6150 | `			runStart = i+1;` |
|      9 | 6151 | `			continue;` |
|      - | 6152 | `		}` |
|    111 | 6153 | `		switch( c ){` |
|      3 | 6154 | `		case '<':  zEnt = "&#60;"; break;` |
|      3 | 6155 | `		case '>':  zEnt = "&#62;"; break;` |
|     11 | 6156 | `		case '&':  zEnt = "&#38;"; break;` |
|      3 | 6157 | `		case '"':  zEnt = "&#34;"; break;` |
|      3 | 6158 | `		case '\'': zEnt = "&#39;"; break;` |
|     46 | 6159 | `		default:` |
|      - | 6160 | `			/* Control bytes <32 are always numeric-encoded; bytes >=127 only when` |
|      - | 6161 | `			 * ENCODE_HIGH is set. Everything else stays in the current run. */` |
|     93 | 6162 | `			if( c<32 \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     17 | 6163 | `				if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     17 | 6164 | `				ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|     17 | 6165 | `				runStart = i+1;` |
|      8 | 6166 | `			}` |
|     93 | 6167 | `			continue; /* keep in the current run */` |
|      - | 6168 | `		}` |
|     19 | 6169 | `		if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     19 | 6170 | `		ph7_result_string(pCtx,zEnt,-1); /* -1: length from strlen */` |
|     19 | 6171 | `		runStart = i+1;` |
|     10 | 6172 | `	}` |
|     13 | 6173 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     13 | 6174 | `}` |
|      - | 6175 | `/* HTML 4.01 named-entity table (codepoint -> "&name;") used by the UTF-8-aware` |
|      - | 6176 | ` * FULL_SPECIAL_CHARS filter, sorted ascending by codepoint for binary search.` |
|      - | 6177 | ` * Generated from php 8.5.7 (the exact set php_escape_html_entities emits for the` |
|      - | 6178 | ` * default document type); the five inline specials <>&"' are handled separately,` |
|      - | 6179 | ` * so every entry here is a codepoint >=0xA0. 248 rows. */` |
|      - | 6180 | `static const struct { sxu32 cp; const char *zEnt; } aHtml401Ent[] = {` |
|      - | 6181 | `	{0x00A0,"&nbsp;"},{0x00A1,"&iexcl;"},{0x00A2,"&cent;"},{0x00A3,"&pound;"},` |
|      - | 6182 | `	{0x00A4,"&curren;"},{0x00A5,"&yen;"},{0x00A6,"&brvbar;"},{0x00A7,"&sect;"},` |
|      - | 6183 | `	{0x00A8,"&uml;"},{0x00A9,"&copy;"},{0x00AA,"&ordf;"},{0x00AB,"&laquo;"},` |
|      - | 6184 | `	{0x00AC,"&not;"},{0x00AD,"&shy;"},{0x00AE,"&reg;"},{0x00AF,"&macr;"},` |
|      - | 6185 | `	{0x00B0,"&deg;"},{0x00B1,"&plusmn;"},{0x00B2,"&sup2;"},{0x00B3,"&sup3;"},` |
|      - | 6186 | `	{0x00B4,"&acute;"},{0x00B5,"&micro;"},{0x00B6,"&para;"},{0x00B7,"&middot;"},` |
|      - | 6187 | `	{0x00B8,"&cedil;"},{0x00B9,"&sup1;"},{0x00BA,"&ordm;"},{0x00BB,"&raquo;"},` |
|      - | 6188 | `	{0x00BC,"&frac14;"},{0x00BD,"&frac12;"},{0x00BE,"&frac34;"},{0x00BF,"&iquest;"},` |
|      - | 6189 | `	{0x00C0,"&Agrave;"},{0x00C1,"&Aacute;"},{0x00C2,"&Acirc;"},{0x00C3,"&Atilde;"},` |
|      - | 6190 | `	{0x00C4,"&Auml;"},{0x00C5,"&Aring;"},{0x00C6,"&AElig;"},{0x00C7,"&Ccedil;"},` |
|      - | 6191 | `	{0x00C8,"&Egrave;"},{0x00C9,"&Eacute;"},{0x00CA,"&Ecirc;"},{0x00CB,"&Euml;"},` |
|      - | 6192 | `	{0x00CC,"&Igrave;"},{0x00CD,"&Iacute;"},{0x00CE,"&Icirc;"},{0x00CF,"&Iuml;"},` |
|      - | 6193 | `	{0x00D0,"&ETH;"},{0x00D1,"&Ntilde;"},{0x00D2,"&Ograve;"},{0x00D3,"&Oacute;"},` |
|      - | 6194 | `	{0x00D4,"&Ocirc;"},{0x00D5,"&Otilde;"},{0x00D6,"&Ouml;"},{0x00D7,"&times;"},` |
|      - | 6195 | `	{0x00D8,"&Oslash;"},{0x00D9,"&Ugrave;"},{0x00DA,"&Uacute;"},{0x00DB,"&Ucirc;"},` |
|      - | 6196 | `	{0x00DC,"&Uuml;"},{0x00DD,"&Yacute;"},{0x00DE,"&THORN;"},{0x00DF,"&szlig;"},` |
|      - | 6197 | `	{0x00E0,"&agrave;"},{0x00E1,"&aacute;"},{0x00E2,"&acirc;"},{0x00E3,"&atilde;"},` |
|      - | 6198 | `	{0x00E4,"&auml;"},{0x00E5,"&aring;"},{0x00E6,"&aelig;"},{0x00E7,"&ccedil;"},` |
|      - | 6199 | `	{0x00E8,"&egrave;"},{0x00E9,"&eacute;"},{0x00EA,"&ecirc;"},{0x00EB,"&euml;"},` |
|      - | 6200 | `	{0x00EC,"&igrave;"},{0x00ED,"&iacute;"},{0x00EE,"&icirc;"},{0x00EF,"&iuml;"},` |
|      - | 6201 | `	{0x00F0,"&eth;"},{0x00F1,"&ntilde;"},{0x00F2,"&ograve;"},{0x00F3,"&oacute;"},` |
|      - | 6202 | `	{0x00F4,"&ocirc;"},{0x00F5,"&otilde;"},{0x00F6,"&ouml;"},{0x00F7,"&divide;"},` |
|      - | 6203 | `	{0x00F8,"&oslash;"},{0x00F9,"&ugrave;"},{0x00FA,"&uacute;"},{0x00FB,"&ucirc;"},` |
|      - | 6204 | `	{0x00FC,"&uuml;"},{0x00FD,"&yacute;"},{0x00FE,"&thorn;"},{0x00FF,"&yuml;"},` |
|      - | 6205 | `	{0x0152,"&OElig;"},{0x0153,"&oelig;"},{0x0160,"&Scaron;"},{0x0161,"&scaron;"},` |
|      - | 6206 | `	{0x0178,"&Yuml;"},{0x0192,"&fnof;"},{0x02C6,"&circ;"},{0x02DC,"&tilde;"},` |
|      - | 6207 | `	{0x0391,"&Alpha;"},{0x0392,"&Beta;"},{0x0393,"&Gamma;"},{0x0394,"&Delta;"},` |
|      - | 6208 | `	{0x0395,"&Epsilon;"},{0x0396,"&Zeta;"},{0x0397,"&Eta;"},{0x0398,"&Theta;"},` |
|      - | 6209 | `	{0x0399,"&Iota;"},{0x039A,"&Kappa;"},{0x039B,"&Lambda;"},{0x039C,"&Mu;"},` |
|      - | 6210 | `	{0x039D,"&Nu;"},{0x039E,"&Xi;"},{0x039F,"&Omicron;"},{0x03A0,"&Pi;"},` |
|      - | 6211 | `	{0x03A1,"&Rho;"},{0x03A3,"&Sigma;"},{0x03A4,"&Tau;"},{0x03A5,"&Upsilon;"},` |
|      - | 6212 | `	{0x03A6,"&Phi;"},{0x03A7,"&Chi;"},{0x03A8,"&Psi;"},{0x03A9,"&Omega;"},` |
|      - | 6213 | `	{0x03B1,"&alpha;"},{0x03B2,"&beta;"},{0x03B3,"&gamma;"},{0x03B4,"&delta;"},` |
|      - | 6214 | `	{0x03B5,"&epsilon;"},{0x03B6,"&zeta;"},{0x03B7,"&eta;"},{0x03B8,"&theta;"},` |
|      - | 6215 | `	{0x03B9,"&iota;"},{0x03BA,"&kappa;"},{0x03BB,"&lambda;"},{0x03BC,"&mu;"},` |
|      - | 6216 | `	{0x03BD,"&nu;"},{0x03BE,"&xi;"},{0x03BF,"&omicron;"},{0x03C0,"&pi;"},` |
|      - | 6217 | `	{0x03C1,"&rho;"},{0x03C2,"&sigmaf;"},{0x03C3,"&sigma;"},{0x03C4,"&tau;"},` |
|      - | 6218 | `	{0x03C5,"&upsilon;"},{0x03C6,"&phi;"},{0x03C7,"&chi;"},{0x03C8,"&psi;"},` |
|      - | 6219 | `	{0x03C9,"&omega;"},{0x03D1,"&thetasym;"},{0x03D2,"&upsih;"},{0x03D6,"&piv;"},` |
|      - | 6220 | `	{0x2002,"&ensp;"},{0x2003,"&emsp;"},{0x2009,"&thinsp;"},{0x200C,"&zwnj;"},` |
|      - | 6221 | `	{0x200D,"&zwj;"},{0x200E,"&lrm;"},{0x200F,"&rlm;"},{0x2013,"&ndash;"},` |
|      - | 6222 | `	{0x2014,"&mdash;"},{0x2018,"&lsquo;"},{0x2019,"&rsquo;"},{0x201A,"&sbquo;"},` |
|      - | 6223 | `	{0x201C,"&ldquo;"},{0x201D,"&rdquo;"},{0x201E,"&bdquo;"},{0x2020,"&dagger;"},` |
|      - | 6224 | `	{0x2021,"&Dagger;"},{0x2022,"&bull;"},{0x2026,"&hellip;"},{0x2030,"&permil;"},` |
|      - | 6225 | `	{0x2032,"&prime;"},{0x2033,"&Prime;"},{0x2039,"&lsaquo;"},{0x203A,"&rsaquo;"},` |
|      - | 6226 | `	{0x203E,"&oline;"},{0x2044,"&frasl;"},{0x20AC,"&euro;"},{0x2111,"&image;"},` |
|      - | 6227 | `	{0x2118,"&weierp;"},{0x211C,"&real;"},{0x2122,"&trade;"},{0x2135,"&alefsym;"},` |
|      - | 6228 | `	{0x2190,"&larr;"},{0x2191,"&uarr;"},{0x2192,"&rarr;"},{0x2193,"&darr;"},` |
|      - | 6229 | `	{0x2194,"&harr;"},{0x21B5,"&crarr;"},{0x21D0,"&lArr;"},{0x21D1,"&uArr;"},` |
|      - | 6230 | `	{0x21D2,"&rArr;"},{0x21D3,"&dArr;"},{0x21D4,"&hArr;"},{0x2200,"&forall;"},` |
|      - | 6231 | `	{0x2202,"&part;"},{0x2203,"&exist;"},{0x2205,"&empty;"},{0x2207,"&nabla;"},` |
|      - | 6232 | `	{0x2208,"&isin;"},{0x2209,"&notin;"},{0x220B,"&ni;"},{0x220F,"&prod;"},` |
|      - | 6233 | `	{0x2211,"&sum;"},{0x2212,"&minus;"},{0x2217,"&lowast;"},{0x221A,"&radic;"},` |
|      - | 6234 | `	{0x221D,"&prop;"},{0x221E,"&infin;"},{0x2220,"&ang;"},{0x2227,"&and;"},` |
|      - | 6235 | `	{0x2228,"&or;"},{0x2229,"&cap;"},{0x222A,"&cup;"},{0x222B,"&int;"},` |
|      - | 6236 | `	{0x2234,"&there4;"},{0x223C,"&sim;"},{0x2245,"&cong;"},{0x2248,"&asymp;"},` |
|      - | 6237 | `	{0x2260,"&ne;"},{0x2261,"&equiv;"},{0x2264,"&le;"},{0x2265,"&ge;"},` |
|      - | 6238 | `	{0x2282,"&sub;"},{0x2283,"&sup;"},{0x2284,"&nsub;"},{0x2286,"&sube;"},` |
|      - | 6239 | `	{0x2287,"&supe;"},{0x2295,"&oplus;"},{0x2297,"&otimes;"},{0x22A5,"&perp;"},` |
|      - | 6240 | `	{0x22C5,"&sdot;"},{0x2308,"&lceil;"},{0x2309,"&rceil;"},{0x230A,"&lfloor;"},` |
|      - | 6241 | `	{0x230B,"&rfloor;"},{0x2329,"&lang;"},{0x232A,"&rang;"},{0x25CA,"&loz;"},` |
|      - | 6242 | `	{0x2660,"&spades;"},{0x2663,"&clubs;"},{0x2665,"&hearts;"},{0x2666,"&diams;"}` |
|      - | 6243 | `};` |
|      - | 6244 | `/* Binary-search aHtml401Ent[] for cp; return its "&name;" entity or 0. */` |
|     41 | 6245 | `static const char *FvHtml401Lookup(sxu32 cp){` |
|     41 | 6246 | `	int lo = 0, hi = (int)SX_ARRAYSIZE(aHtml401Ent) - 1;` |
|    323 | 6247 | `	while( lo <= hi ){` |
|    309 | 6248 | `		int mid = (lo + hi) / 2;` |
|    309 | 6249 | `		sxu32 c = aHtml401Ent[mid].cp;` |
|    309 | 6250 | `		if( c == cp ){ return aHtml401Ent[mid].zEnt; }` |
|    283 | 6251 | `		if( c < cp ){ lo = mid + 1; } else { hi = mid - 1; }` |
|      1 | 6252 | `	}` |
|     15 | 6253 | `	return 0;` |
|     21 | 6254 | `}` |
|      - | 6255 | `/* Decode one strict-UTF-8 sequence at p (< zEnd). On success returns its byte` |
|      - | 6256 | ` * length (1..4) and sets *pCp to the codepoint; on any malformed, overlong,` |
|      - | 6257 | ` * surrogate, truncated or out-of-range (>U+10FFFF) sequence returns 0. Matches` |
|      - | 6258 | ` * PHP's UTF-8 validation used by FULL_SPECIAL_CHARS (verified vs php 8.5.7). */` |
|    101 | 6259 | `static int FvUtf8Next(const unsigned char *p,const unsigned char *zEnd,sxu32 *pCp){` |
|    101 | 6260 | `	unsigned char c = p[0];` |
|    101 | 6261 | `	if( c < 0x80 ){ *pCp = c; return 1; }` |
|    101 | 6262 | `	if( c < 0xC2 ){ return 0; }              /* 0x80-0xBF stray cont / 0xC0-0xC1 overlong */` |
|     99 | 6263 | `	if( c < 0xE0 ){                          /* 2-byte: U+0080..U+07FF */` |
|     47 | 6264 | `		if( zEnd-p < 2 \|\| (p[1]&0xC0)!=0x80 ){ return 0; }` |
|     45 | 6265 | `		*pCp = ((sxu32)(c&0x1F)<<6) \| (p[1]&0x3F);` |
|     45 | 6266 | `		return 2;` |
|      - | 6267 | `	}` |
|     53 | 6268 | `	if( c < 0xF0 ){                          /* 3-byte: U+0800..U+FFFF minus surrogates */` |
|      - | 6269 | `		sxu32 cp;` |
|     47 | 6270 | `		if( zEnd-p < 3 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 ){ return 0; }` |
|     33 | 6271 | `		cp = ((sxu32)(c&0x0F)<<12) \| ((sxu32)(p[1]&0x3F)<<6) \| (p[2]&0x3F);` |
|     33 | 6272 | `		if( cp < 0x800 \|\| (cp>=0xD800 && cp<=0xDFFF) ){ return 0; }` |
|     29 | 6273 | `		*pCp = cp;` |
|     29 | 6274 | `		return 3;` |
|      - | 6275 | `	}` |
|      7 | 6276 | `	if( c < 0xF5 ){                          /* 4-byte: U+10000..U+10FFFF */` |
|      - | 6277 | `		sxu32 cp;` |
|      5 | 6278 | `		if( zEnd-p < 4 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 \|\| (p[3]&0xC0)!=0x80 ){ return 0; }` |
|      5 | 6279 | `		cp = ((sxu32)(c&0x07)<<18) \| ((sxu32)(p[1]&0x3F)<<12) \| ((sxu32)(p[2]&0x3F)<<6) \| (p[3]&0x3F);` |
|      5 | 6280 | `		if( cp < 0x10000 \|\| cp > 0x10FFFF ){ return 0; }` |
|      5 | 6281 | `		*pCp = cp;` |
|      5 | 6282 | `		return 4;` |
|      - | 6283 | `	}` |
|      3 | 6284 | `	return 0;                                /* 0xF5-0xFF */` |
|     51 | 6285 | `}` |
|      - | 6286 | `/* FILTER_SANITIZE_FULL_SPECIAL_CHARS: htmlentities-style, UTF-8-aware. Encodes` |
|      - | 6287 | ` * <>&"' as named entities ("'" -> &#039;; quotes suppressed under NO_ENCODE_QUOTES),` |
|      - | 6288 | ` * and every valid UTF-8 codepoint with an HTML 4.01 named entity as that entity;` |
|      - | 6289 | ` * valid codepoints without a named entity (and low control bytes) pass through` |
|      - | 6290 | ` * verbatim. If the input contains ANY invalid UTF-8 the whole result is "".` |
|      - | 6291 | ` * The STRIP/ENCODE flags do NOT apply to this filter (only NO_ENCODE_QUOTES).` |
|      - | 6292 | ` * php's filter does NOT re-encode valid pre-existing entities ("&amp;" stays,` |
|      - | 6293 | ` * "&bogus;" becomes "&amp;bogus;"), i.e. double_encode=false semantics —` |
|      - | 6294 | ` * exactly htmlentities(ENT_QUOTES\|ENT_HTML401, double_encode: false), so this` |
|      - | 6295 | ` * delegates to the shared encoder. Byte-exact vs php 8.5.7. */` |
|     25 | 6296 | `static void FvSanitizeFull(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 6297 | `	int iEntFlags = (flags & FV_FLAG_NO_ENCODE_QUOTES) ? 0 : PH7_ENT_QUOTES;` |
|     25 | 6298 | `	HtmlEscape(pCtx,z,n,iEntFlags,1/*bAll*/,0/*bDoubleEncode*/);` |
|     25 | 6299 | `}` |
|      - | 6300 | `/* ---------------------------------------------------------------------------` |
|      - | 6301 | ` * UTF-8-aware HTML entity core (htmlspecialchars/htmlentities family).` |
|      - | 6302 | ` * Prototyped next to the five builtins earlier in this file; lives here so it` |
|      - | 6303 | ` * can share aHtml401Ent[]/FvHtml401Lookup()/FvUtf8Next() with the filter_var` |
|      - | 6304 | ` * FULL_SPECIAL_CHARS filter above. Byte-exact vs php 8.5.7 (oracle-swept).` |
|      - | 6305 | ` * ------------------------------------------------------------------------ */` |
|      - | 6306 | `/* Encode cp as UTF-8 into zBuf (>= 4 bytes); return the byte length 1..4.` |
|      - | 6307 | ` * Thin wrapper over the engine-wide SX_WRITE_UTF8 (sxmacros.h). */` |
|    585 | 6308 | `static int HtmlCpUtf8(sxu32 cp,char *zBuf){` |
|    585 | 6309 | `	sxu8 *z = (sxu8 *)zBuf;` |
|    585 | 6310 | `	SX_WRITE_UTF8(z,cp);` |
|    585 | 6311 | `	return (int)(z - (sxu8 *)zBuf);` |
|      1 | 6312 | `}` |
|      - | 6313 | `/* Doctype-allowed codepoint test (php's unicode_cp_is_allowed) — gates what a` |
|      - | 6314 | ` * numeric reference may DECODE to. Oracle-pinned per doctype: HTML401` |
|      - | 6315 | ` * disallows C0 (except TAB/LF/CR) and DEL..U+009F; XML1 and XHTML share the` |
|      - | 6316 | ` * XML rules — DEL..U+009F allowed, U+FFFE/U+FFFF excluded; HTML5 swaps CR` |
|      - | 6317 | ` * for FF (0x0C) and excludes the noncharacters (U+FDD0..U+FDEF and every` |
|      - | 6318 | ` * U+xFFFE/U+xFFFF). Surrogates are disallowed everywhere. */` |
|     91 | 6319 | `static int HtmlCpAllowed(sxu32 cp,int iFlags){` |
|     91 | 6320 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|     91 | 6321 | `	if( cp==0x09 \|\| cp==0x0A ){ return 1; }` |
|     87 | 6322 | `	if( cp==0x0D ){ return iDoc != PH7_ENT_DOC_HTML5; }` |
|     85 | 6323 | `	if( cp==0x0C ){ return iDoc == PH7_ENT_DOC_HTML5; }` |
|     85 | 6324 | `	if( cp < 0x20 \|\| cp > 0x10FFFF ){ return 0; }` |
|     79 | 6325 | `	if( cp>=0xD800 && cp<=0xDFFF ){ return 0; }` |
|     77 | 6326 | `	if( cp>=0x7F && cp<=0x9F ){ return iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML; }` |
|     71 | 6327 | `	if( iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML ){` |
|    ! 0 | 6328 | `		return cp!=0xFFFE && cp!=0xFFFF;` |
|      - | 6329 | `	}` |
|     71 | 6330 | `	if( iDoc == PH7_ENT_DOC_HTML5 ){` |
|      9 | 6331 | `		if( cp>=0xFDD0 && cp<=0xFDEF ){ return 0; }` |
|      9 | 6332 | `		if( (cp & 0xFFFF) >= 0xFFFE ){ return 0; }` |
|      4 | 6333 | `	}` |
|     71 | 6334 | `	return 1;` |
|     46 | 6335 | `}` |
|      - | 6336 | `/* The ENT_DISALLOWED gate for RAW characters on the ENCODE side. Same as the` |
|      - | 6337 | ` * decode gate except CR under HTML5: php's encode-side unicode_cp_is_allowed` |
|      - | 6338 | ` * keeps a literal "\r" verbatim under ENT_HTML5\|ENT_DISALLOWED while the` |
|      - | 6339 | ` * decode side leaves "&#13;" un-decoded (oracle-pinned at flags 176). */` |
|      9 | 6340 | `static int HtmlCpAllowedEncode(sxu32 cp,int iFlags){` |
|      9 | 6341 | `	if( cp==0x0D && (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 ){ return 1; }` |
|      9 | 6342 | `	return HtmlCpAllowed(cp,iFlags);` |
|      5 | 6343 | `}` |
|      - | 6344 | `/* Numeric-reference validity for the double_encode=false "is this already a` |
|      - | 6345 | ` * valid entity" test — a MUCH looser predicate than the decode gate above:` |
|      - | 6346 | ` * any codepoint <= U+10FFFF is valid (controls and surrogates included, every` |
|      - | 6347 | ` * doctype). ENT_DISALLOWED re-tightens non-HTML401 doctypes to the decode` |
|      - | 6348 | ` * gate, except that HTML5 exempts surrogates. All oracle-pinned: &#0; and` |
|      - | 6349 | ` * &#xD800; stay verbatim at flags 11 and 139; flags -1 (HTML5+DISALLOWED)` |
|      - | 6350 | ` * re-encodes &#0; and &#x10FFFF; but still keeps &#xD800;; flags 144` |
|      - | 6351 | ` * (XML1+DISALLOWED) re-encodes &#xD800;. */` |
|      9 | 6352 | `static int HtmlNumericAllowed(sxu32 cp,int iFlags){` |
|      9 | 6353 | `	if( cp > 0x10FFFF ){ return 0; }` |
|      7 | 6354 | `	if( (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML401 ){ return 1; /* never tightened */ }` |
|    ! 0 | 6355 | `	if( (iFlags & PH7_ENT_DISALLOWED)` |
|    ! 0 | 6356 | `	 && !((iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 && cp>=0xD800 && cp<=0xDFFF)` |
|    ! 0 | 6357 | `	 && !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|    ! 0 | 6358 | `	return 1;` |
|      5 | 6359 | `}` |
|      - | 6360 | `/* How many bytes the malformed UTF-8 sequence at p consumes — php's` |
|      - | 6361 | ` * get_next_char failure step (one U+FFFD substitution / one ENT_IGNORE drop` |
|      - | 6362 | ` * per MAXIMAL invalid subpart, not per byte): a prefix-valid sequence eats` |
|      - | 6363 | ` * its continuation bytes ("\xE0\x80\xAF" is ONE unit) while a byte that could` |
|      - | 6364 | ` * start a new sequence is left for the next round. */` |
|      5 | 6365 | `static int HtmlUtf8Trail(unsigned char c){ return c>=0x80 && c<=0xBF; }` |
|     11 | 6366 | `static int HtmlUtf8Lead(unsigned char c){ return c<0x80 \|\| (c>=0xC2 && c<=0xF4); }` |
|     15 | 6367 | `static int HtmlUtf8FailAdvance(const unsigned char *p,const unsigned char *zEnd){` |
|     15 | 6368 | `	unsigned char c = p[0];` |
|     15 | 6369 | `	int nAvail = (int)(zEnd - p);` |
|     15 | 6370 | `	if( c < 0xC2 \|\| c > 0xF4 ){ return 1; } /* stray trail / C0-C1 / F5-FF */` |
|     13 | 6371 | `	if( c < 0xE0 ){` |
|      3 | 6372 | `		if( nAvail < 2 ){ return 1; }` |
|      3 | 6373 | `		return HtmlUtf8Lead(p[1]) ? 1 : 2;` |
|      - | 6374 | `	}` |
|     11 | 6375 | `	if( c < 0xF0 ){` |
|     11 | 6376 | `		if( nAvail >= 3 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) ){` |
|      3 | 6377 | `			return 3; /* complete but overlong/surrogate */` |
|      - | 6378 | `		}` |
|      9 | 6379 | `		if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 6380 | `		if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 6381 | `		return 3;` |
|      - | 6382 | `	}` |
|    ! 0 | 6383 | `	if( nAvail >= 4 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) && HtmlUtf8Trail(p[3]) ){` |
|    ! 0 | 6384 | `		return 4; /* complete but overlong / > U+10FFFF */` |
|      - | 6385 | `	}` |
|    ! 0 | 6386 | `	if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 6387 | `	if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 6388 | `	if( nAvail < 4 \|\| HtmlUtf8Lead(p[3]) ){ return 3; }` |
|    ! 0 | 6389 | `	return 4;` |
|      8 | 6390 | `}` |
|      - | 6391 | `/* The basic special entities, shared by named matching, the hsc_decode` |
|      - | 6392 | ` * numeric whitelist and the translation-table builder so the sets can never` |
|      - | 6393 | ` * drift apart. (&apos; is not an HTML 4.01 entity — doctype-gated below.) */` |
|      - | 6394 | `static const struct { const char *zEnt; int n; sxu32 cp; } aHtmlSpecEnt[] = {` |
|      - | 6395 | `	{"&amp;",5,38},{"&lt;",4,60},{"&gt;",4,62},{"&quot;",6,34},{"&apos;",6,39}` |
|      - | 6396 | `};` |
|      - | 6397 | `/* Does this doctype consult the named-entity table (aHtml401Ent)? XML 1.0 has` |
|      - | 6398 | ` * no named entities beyond the specials; XHTML/HTML5 are approximated by the` |
|      - | 6399 | ` * HTML 4.01 table (documented divergence). */` |
|     63 | 6400 | `static int HtmlDocHasNamedTable(int iDoc){` |
|     63 | 6401 | `	return iDoc != PH7_ENT_DOC_XML1;` |
|      1 | 6402 | `}` |
|      - | 6403 | `/* The single-quote entity per doctype. Oracle-pinned asymmetry: for every` |
|      - | 6404 | ` * non-HTML401 doctype htmlspecialchars emits &apos; while htmlentities` |
|      - | 6405 | ` * (bEntities) keeps &#039; under XHTML too. The translation table mirrors` |
|      - | 6406 | ` * whichever function the requested table belongs to. */` |
|     29 | 6407 | `static const char *HtmlAposEntity(int iDoc,int bEntities){` |
|     29 | 6408 | `	if( iDoc == PH7_ENT_DOC_HTML401 \|\| (bEntities && iDoc == PH7_ENT_DOC_XHTML) ){` |
|     21 | 6409 | `		return "&#039;";` |
|      - | 6410 | `	}` |
|      9 | 6411 | `	return "&apos;";` |
|     15 | 6412 | `}` |
|      - | 6413 | `/* Try to parse one HTML entity at z (z[0]=='&', z < zEnd). bFull selects the` |
|      - | 6414 | ` * html_entity_decode set (doctype named table + any allowed numeric ref) vs` |
|      - | 6415 | ` * the htmlspecialchars_decode set (the basic specials + quote numerics only).` |
|      - | 6416 | ` * Named matching is case-SENSITIVE and the ';' is required (both PHP-exact);` |
|      - | 6417 | ` * numeric refs accept dec/hex (x or X) with any number of leading zeros but` |
|      - | 6418 | ` * reject out-of-range, surrogate and doctype-disallowed codepoints (the` |
|      - | 6419 | ` * caller then leaves the source verbatim). Quote-flag gating is NOT applied` |
|      - | 6420 | ` * here — the same routine doubles as the "is this a valid entity" test for` |
|      - | 6421 | ` * double_encode=false, which ignores the quote bits (oracle-pinned).` |
|      - | 6422 | ` * bEncodeCheck selects the looser HtmlNumericAllowed predicate used by that` |
|      - | 6423 | ` * double_encode test; decode callers pass 0 for the HtmlCpAllowed gate.` |
|      - | 6424 | ` * On success sets *pCp / *pnConsumed and returns 1. */` |
|    172 | 6425 | `static int HtmlParseEntity(const unsigned char *z,const unsigned char *zEnd,` |
|      1 | 6426 | `                           int iFlags,int bFull,int bEncodeCheck,sxu32 *pCp,int *pnConsumed){` |
|    173 | 6427 | `	int nAvail = (int)(zEnd - z);` |
|    173 | 6428 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6429 | `	sxu32 n;` |
|    173 | 6430 | `	if( nAvail < 4 ){ return 0; } /* shortest entities: &lt; &#9; */` |
|    169 | 6431 | `	if( z[1] == '#' ){` |
|      - | 6432 | `		/* Numeric reference */` |
|     89 | 6433 | `		sxu32 cp = 0;` |
|     89 | 6434 | `		int i = 2, bHex = 0, nDig = 0;` |
|     89 | 6435 | `		if( z[i]=='x' \|\| z[i]=='X' ){ bHex = 1; i++; }` |
|    317 | 6436 | `		for( ; i < nAvail && z[i] != ';' ; i++ ){` |
|      - | 6437 | `			int v;` |
|    221 | 6438 | `			unsigned char c = z[i];` |
|    221 | 6439 | `			if( c>='0' && c<='9' ){ v = c - '0'; }` |
|     17 | 6440 | `			else if( bHex && c>='a' && c<='f' ){ v = c - 'a' + 10; }` |
|     17 | 6441 | `			else if( bHex && c>='A' && c<='F' ){ v = c - 'A' + 10; }` |
|    ! 0 | 6442 | `			else { return 0; }` |
|      - | 6443 | `			/* Stop accumulating once out of range (keeps validating the shape;` |
|      - | 6444 | `			 * max intermediate is 0x10FFFF*16+15, no sxu32 overflow). */` |
|    221 | 6445 | `			if( cp <= 0x10FFFF ){ cp = cp * (bHex ? 16 : 10) + (sxu32)v; }` |
|    221 | 6446 | `			nDig++;` |
|    111 | 6447 | `		}` |
|     97 | 6448 | `		if( nDig == 0 \|\| i >= nAvail ){ return 0; } /* no digits / no ';' */` |
|     97 | 6449 | `		if( bEncodeCheck ? !HtmlNumericAllowed(cp,iFlags) : !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|     83 | 6450 | `		if( !bFull ){` |
|      - | 6451 | `			/* hsc_decode: numeric refs to the five specials only. */` |
|     99 | 6452 | `			for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) && aHtmlSpecEnt[n].cp != cp ; n++ ){}` |
|     25 | 6453 | `			if( n >= SX_ARRAYSIZE(aHtmlSpecEnt) ){ return 0; }` |
|     11 | 6454 | `		}` |
|     75 | 6455 | `		*pCp = cp;` |
|     75 | 6456 | `		*pnConsumed = i + 1;` |
|     75 | 6457 | `		return 1;` |
|      - | 6458 | `	}` |
|      - | 6459 | `	/* Named reference — every entity name starts with a letter, so anything` |
|      - | 6460 | `	 * else can bail out before touching the tables. */` |
|     81 | 6461 | `	if( !((z[1]>='a' && z[1]<='z') \|\| (z[1]>='A' && z[1]<='Z')) ){ return 0; }` |
|    287 | 6462 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) ; n++ ){` |
|    265 | 6463 | `		if( aHtmlSpecEnt[n].cp == 39 && iDoc == PH7_ENT_DOC_HTML401 ){ continue; }` |
|    243 | 6464 | `		if( nAvail >= aHtmlSpecEnt[n].n && SyMemcmp(z,aHtmlSpecEnt[n].zEnt,(sxu32)aHtmlSpecEnt[n].n) == 0 ){` |
|     53 | 6465 | `			*pCp = aHtmlSpecEnt[n].cp;` |
|     53 | 6466 | `			*pnConsumed = aHtmlSpecEnt[n].n;` |
|     53 | 6467 | `			return 1;` |
|      - | 6468 | `		}` |
|     96 | 6469 | `	}` |
|     23 | 6470 | `	if( bFull && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 6471 | `		/* Linear scan of the 248-row table: runs only at '&'-then-letter` |
|      - | 6472 | `		 * positions and guarantees the decode set can never drift from the` |
|      - | 6473 | `		 * encode table. The first-letter guard skips the SyStrlen/SyMemcmp` |
|      - | 6474 | `		 * for ~96% of rows. */` |
|   3369 | 6475 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|      - | 6476 | `			sxu32 nEnt;` |
|   3357 | 6477 | `			if( z[1] != (unsigned char)aHtml401Ent[n].zEnt[1] ){ continue; }` |
|    121 | 6478 | `			nEnt = SyStrlen(aHtml401Ent[n].zEnt);` |
|    121 | 6479 | `			if( (sxu32)nAvail >= nEnt && SyMemcmp(z,aHtml401Ent[n].zEnt,nEnt) == 0 ){` |
|      7 | 6480 | `				*pCp = aHtml401Ent[n].cp;` |
|      7 | 6481 | `				*pnConsumed = (int)nEnt;` |
|      7 | 6482 | `				return 1;` |
|      - | 6483 | `			}` |
|     58 | 6484 | `		}` |
|      6 | 6485 | `	}` |
|     17 | 6486 | `	return 0;` |
|     88 | 6487 | `}` |
|      - | 6488 | `/* Shared encoder for htmlspecialchars (bAll=0) and htmlentities (bAll=1).` |
|      - | 6489 | ` * Invalid UTF-8 policy: ENT_IGNORE drops the byte (and wins over SUBSTITUTE),` |
|      - | 6490 | ` * ENT_SUBSTITUTE emits one U+FFFD per invalid byte, neither -> the whole` |
|      - | 6491 | ` * result is "" (pre-validated in a first pass: the accumulating result API` |
|      - | 6492 | ` * cannot roll back — same reason FvSanitizeFull is two-pass). */` |
|     96 | 6493 | `static void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 6494 | `                       int iFlags,int bAll,int bDoubleEncode){` |
|     97 | 6495 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     97 | 6496 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|      - | 6497 | `	const unsigned char *runStart;` |
|     97 | 6498 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6499 | `	sxu32 cp;` |
|     97 | 6500 | `	if( (iFlags & (PH7_ENT_IGNORE\|PH7_ENT_SUBSTITUTE)) == 0 ){` |
|      - | 6501 | `		/* Pass 1: any malformed sequence rejects the entire input. ASCII` |
|      - | 6502 | `		 * bytes cannot be malformed, so skip them without the decoder. */` |
|    381 | 6503 | `		while( p < zEnd ){` |
|      - | 6504 | `			int len;` |
|    323 | 6505 | `			if( *p < 0x80 ){ p++; continue; }` |
|     37 | 6506 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     37 | 6507 | `			if( len == 0 ){ ph7_result_string(pCtx,"",0); return; }` |
|     27 | 6508 | `			p += len;` |
|      1 | 6509 | `		}` |
|     59 | 6510 | `		p = (const unsigned char *)zIn;` |
|     29 | 6511 | `	}` |
|     87 | 6512 | `	runStart = p;` |
|     87 | 6513 | `	ph7_result_string(pCtx,"",0);` |
|    463 | 6514 | `	while( p < zEnd ){` |
|    377 | 6515 | `		const char *zEnt = 0;` |
|      - | 6516 | `		int len;` |
|    377 | 6517 | `		if( *p < 0x80 ){` |
|    313 | 6518 | `			len = 1;` |
|    313 | 6519 | `			switch( *p ){` |
|     25 | 6520 | `			case '<': zEnt = "&lt;"; break;` |
|     25 | 6521 | `			case '>': zEnt = "&gt;"; break;` |
|     18 | 6522 | `			case '&':` |
|     37 | 6523 | `				zEnt = "&amp;";` |
|     37 | 6524 | `				if( !bDoubleEncode ){` |
|      - | 6525 | `					sxu32 eCp; int nEat;` |
|     25 | 6526 | `					if( HtmlParseEntity(p,zEnd,iFlags,1,1,&eCp,&nEat) ){` |
|      - | 6527 | `						/* A valid existing entity: keep it verbatim. */` |
|     13 | 6528 | `						zEnt = 0;` |
|     13 | 6529 | `						len = nEat;` |
|      6 | 6530 | `					}` |
|     12 | 6531 | `				}` |
|     37 | 6532 | `				break;` |
|     10 | 6533 | `			case '"':` |
|     21 | 6534 | `				if( iFlags & PH7_ENT_QUOTE_DOUBLE ){ zEnt = "&quot;"; }` |
|     21 | 6535 | `				break;` |
|     12 | 6536 | `			case '\'':` |
|     25 | 6537 | `				if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|     23 | 6538 | `					zEnt = HtmlAposEntity(iDoc,bAll);` |
|     11 | 6539 | `				}` |
|     25 | 6540 | `				break;` |
|     92 | 6541 | `			default:` |
|    185 | 6542 | `				if( (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode((sxu32)*p,iFlags) ){` |
|    ! 0 | 6543 | `					zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 6544 | `				}` |
|    184 | 6545 | `				break;` |
|      - | 6546 | `			}` |
|    157 | 6547 | `		}else{` |
|     65 | 6548 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     65 | 6549 | `			if( len == 0 ){` |
|      - | 6550 | `				/* Malformed subpart (IGNORE or SUBSTITUTE is set, else pass 1` |
|      - | 6551 | `				 * would have rejected): drop it or emit ONE U+FFFD for the` |
|      - | 6552 | `				 * whole unit (php substitutes per maximal invalid subpart). */` |
|     15 | 6553 | `				if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|     15 | 6554 | `				if( (iFlags & PH7_ENT_IGNORE) == 0 ){ ph7_result_string(pCtx,"\xEF\xBF\xBD",3); }` |
|     15 | 6555 | `				p += HtmlUtf8FailAdvance(p,zEnd);` |
|     15 | 6556 | `				runStart = p;` |
|     15 | 6557 | `				continue;` |
|      - | 6558 | `			}` |
|     51 | 6559 | `			if( bAll && HtmlDocHasNamedTable(iDoc) ){` |
|     41 | 6560 | `				zEnt = FvHtml401Lookup(cp);` |
|     20 | 6561 | `			}` |
|     51 | 6562 | `			if( zEnt == 0 && (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode(cp,iFlags) ){` |
|    ! 0 | 6563 | `				zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 6564 | `			}` |
|      - | 6565 | `		}` |
|    363 | 6566 | `		if( zEnt ){` |
|    135 | 6567 | `			if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|    135 | 6568 | `			ph7_result_string(pCtx,zEnt,-1);` |
|    135 | 6569 | `			runStart = p + len;` |
|     67 | 6570 | `		}` |
|    363 | 6571 | `		p += len;` |
|      1 | 6572 | `	}` |
|     87 | 6573 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     49 | 6574 | `}` |
|      - | 6575 | `/* Shared decoder for html_entity_decode (bFull=1) and htmlspecialchars_decode` |
|      - | 6576 | ` * (bFull=0). Quote refs (cp 34/39, named or numeric) are gated by the quote` |
|      - | 6577 | ` * bits and left verbatim when suppressed; an invalid entity leaves its '&'` |
|      - | 6578 | ` * verbatim and rescans right after it, which also yields PHP's no-double-` |
|      - | 6579 | ` * decode behavior ("&amp;lt;" -> "&lt;"). */` |
|     84 | 6580 | `static void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 6581 | `                         int iFlags,int bFull){` |
|     85 | 6582 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     85 | 6583 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|     85 | 6584 | `	const unsigned char *runStart = p;` |
|     85 | 6585 | `	ph7_result_string(pCtx,"",0);` |
|    565 | 6586 | `	while( p < zEnd ){` |
|      - | 6587 | `		sxu32 cp;` |
|      - | 6588 | `		int nEat;` |
|    516 | 6589 | `		if( *p != '&' ){ p++; continue; }` |
|    155 | 6590 | `		if( !HtmlParseEntity(p,zEnd,iFlags,bFull,0,&cp,&nEat) ){ p++; continue; }` |
|    124 | 6591 | `		if( (cp == 34 && (iFlags & PH7_ENT_QUOTE_DOUBLE) == 0)` |
|    117 | 6592 | `		 \|\| (cp == 39 && (iFlags & PH7_ENT_QUOTE_SINGLE) == 0) ){` |
|      - | 6593 | `			/* Suppressed quote: leave the entity source verbatim. */` |
|     37 | 6594 | `			p += nEat;` |
|     37 | 6595 | `			continue;` |
|      - | 6596 | `		}` |
|     89 | 6597 | `		if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|      - | 6598 | `		{` |
|      - | 6599 | `			char zBuf[4];` |
|     89 | 6600 | `			int n = HtmlCpUtf8(cp,zBuf);` |
|     89 | 6601 | `			ph7_result_string(pCtx,zBuf,n);` |
|      - | 6602 | `		}` |
|     89 | 6603 | `		p += nEat;` |
|     89 | 6604 | `		runStart = p;` |
|      1 | 6605 | `	}` |
|     81 | 6606 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     81 | 6607 | `}` |
|      - | 6608 | `/* Validate the optional charset argument at apArg[idx]: UTF-8 aliases (and` |
|      - | 6609 | ` * ""/NULL meaning the default) are accepted; anything else — including` |
|      - | 6610 | ` * php-supported single-byte charsets like ISO-8859-1, PHL is UTF-8-only by` |
|      - | 6611 | ` * policy — raises PHP's unsupported-charset warning and is treated as` |
|      - | 6612 | ` * UTF-8 (ph7_context_throw_error_format prepends the function name). */` |
|    143 | 6613 | `static void HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx){` |
|      - | 6614 | `	const char *zCs;` |
|      - | 6615 | `	int nCs;` |
|    150 | 6616 | `	if( nArg <= idx \|\| ph7_value_is_null(apArg[idx]) ){ return; }` |
|     15 | 6617 | `	zCs = ph7_value_to_string(apArg[idx],&nCs);` |
|     15 | 6618 | `	if( nCs == 0 ){ return; } /* "" selects the default charset (UTF-8) */` |
|     13 | 6619 | `	if( nCs == 5 && SyStrnicmp(zCs,"UTF-8",5) == 0 ){` |
|     13 | 6620 | `		return; /* php accepts only "UTF-8" (any case) silently — "UTF8" warns */` |
|      - | 6621 | `	}` |
|    ! 0 | 6622 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 6623 | `		"Charset \"%.*s\" is not supported, assuming UTF-8",nCs,zCs);` |
|     72 | 6624 | `}` |
|      - | 6625 | `/* get_html_translation_table() worker: character (UTF-8 bytes) => entity.` |
|      - | 6626 | ` * The five specials come first in byte order, then — for HTML_ENTITIES with a` |
|      - | 6627 | ` * named-table doctype — the 248 aHtml401Ent rows ascending (oracle-pinned` |
|      - | 6628 | ` * ordering; 253 entries under the defaults). */` |
|    549 | 6629 | `static void HtmlTableAdd(ph7_value *pArray,ph7_value *pValue,const char *zKey,const char *zEnt){` |
|    549 | 6630 | `	ph7_value_string(pValue,zEnt,-1);` |
|    549 | 6631 | `	ph7_array_add_strkey_elem(pArray,zKey,pValue);` |
|    549 | 6632 | `	ph7_value_reset_string_cursor(pValue);` |
|    549 | 6633 | `}` |
|     13 | 6634 | `static void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags){` |
|      - | 6635 | `	ph7_value *pArray,*pValue;` |
|     13 | 6636 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6637 | `	sxu32 n;` |
|     13 | 6638 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 6639 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 6640 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|    ! 0 | 6641 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6642 | `		return;` |
|      - | 6643 | `	}` |
|     13 | 6644 | `	if( iFlags & PH7_ENT_QUOTE_DOUBLE ){` |
|     11 | 6645 | `		HtmlTableAdd(pArray,pValue,"\"","&quot;");` |
|      5 | 6646 | `	}` |
|     13 | 6647 | `	HtmlTableAdd(pArray,pValue,"&","&amp;");` |
|     13 | 6648 | `	if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|      - | 6649 | `		/* The apostrophe row mirrors the function each table belongs to:` |
|      - | 6650 | `		 * SPECIALCHARS follows htmlspecialchars, ENTITIES follows` |
|      - | 6651 | `		 * htmlentities (oracle-pinned at flags 35). */` |
|      7 | 6652 | `		HtmlTableAdd(pArray,pValue,"'",HtmlAposEntity(iDoc,iTable != 0));` |
|      3 | 6653 | `	}` |
|     13 | 6654 | `	HtmlTableAdd(pArray,pValue,"<","&lt;");` |
|     13 | 6655 | `	HtmlTableAdd(pArray,pValue,">","&gt;");` |
|     13 | 6656 | `	if( iTable != 0 /*php: any non-HTML_SPECIALCHARS table => entities*/ && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 6657 | `		char zKey[8];` |
|    499 | 6658 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|    497 | 6659 | `			int nK = HtmlCpUtf8(aHtml401Ent[n].cp,zKey);` |
|    497 | 6660 | `			zKey[nK] = 0;` |
|    497 | 6661 | `			HtmlTableAdd(pArray,pValue,zKey,aHtml401Ent[n].zEnt);` |
|    249 | 6662 | `		}` |
|      1 | 6663 | `	}` |
|     13 | 6664 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 6665 | `}` |
|     25 | 6666 | `static int FvEmailAllowed(unsigned char c){` |
|     25 | 6667 | `	if( (c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9') ){ return 1; }` |
|     16 | 6668 | `	return c=='!'\|\|c=='#'\|\|c=='$'\|\|c=='%'\|\|c=='&'\|\|c=='\''\|\|c=='*'\|\|c=='+'` |
|     10 | 6669 | ``	    \|\| c=='-'\|\|c=='='\|\|c=='?'\|\|c=='^'\|\|c=='_'\|\|c=='`'\|\|c=='{'\|\|c=='\|'`` |
|     15 | 6670 | `	    \|\| c=='}'\|\|c=='~'\|\|c=='@'\|\|c=='.'\|\|c=='['\|\|c==']';` |
|     13 | 6671 | `}` |
|     23 | 6672 | `static int FvUrlAllowed(unsigned char c){` |
|     23 | 6673 | `	return c>=33 && c<=126; /* PHP keeps every printable ASCII byte except space */` |
|      1 | 6674 | `}` |
|      - | 6675 | `/* SANITIZE_EMAIL (isUrl=0) / SANITIZE_URL (isUrl=1): strip disallowed bytes. */` |
|      5 | 6676 | `static void FvSanitizeChars(ph7_context *pCtx,const char *z,int n,int isUrl){` |
|      5 | 6677 | `	int i, runStart = 0;` |
|      5 | 6678 | `	ph7_result_string(pCtx,"",0);` |
|     51 | 6679 | `	for( i=0; i<n; i++ ){` |
|     47 | 6680 | `		unsigned char c = (unsigned char)z[i];` |
|     47 | 6681 | `		if( !(isUrl ? FvUrlAllowed(c) : FvEmailAllowed(c)) ){` |
|     11 | 6682 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     11 | 6683 | `			runStart = i+1;` |
|      5 | 6684 | `		}` |
|     24 | 6685 | `	}` |
|      5 | 6686 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      5 | 6687 | `}` |
|      - | 6688 | `/*` |
|      - | 6689 | ` * Apply the selected filter to one already-resolved input value and write the` |
|      - | 6690 | ` * result into pCtx. Shared by filter_var() and filter_input(): the caller has` |
|      - | 6691 | ` * already parsed $filter/$flags/$options. On validation failure the 'default'` |
|      - | 6692 | ` * option (if any) is returned, else null when FILTER_NULL_ON_FAILURE is set,` |
|      - | 6693 | ` * else false. A validating filter that passes returns the (string) input` |
|      - | 6694 | ` * unchanged; a sanitizer writes its transformed output directly.` |
|      - | 6695 | ` */` |
|    316 | 6696 | `static int FvApplyFilter(ph7_context *pCtx,ph7_value *pInput,` |
|      - | 6697 | `                         int iFilter,int iFlags,ph7_value *pOpts,` |
|      - | 6698 | `                         ph7_value *pDefault)` |
|      3 | 6699 | `{` |
|    319 | 6700 | `	int bNull = (iFlags & FV_NULL_ON_FAILURE) ? 1 : 0;` |
|      - | 6701 | `	const char *zVal; int nVal;` |
|      - | 6702 | `	/* An array/object input fails every scalar filter. */` |
|    319 | 6703 | `	if( ph7_value_is_array(pInput) ){ goto fail; }` |
|    317 | 6704 | `	zVal = ph7_value_to_string(pInput,&nVal);` |
|    317 | 6705 | `	switch( iFilter ){` |
|     28 | 6706 | `	case FV_VALIDATE_INT: {` |
|      - | 6707 | `		ph7_int64 v;` |
|     58 | 6708 | `		if( !FvValidateInt(zVal,nVal,iFlags,&v) ){ goto fail; }` |
|     31 | 6709 | `		if( pOpts ){` |
|      7 | 6710 | `			ph7_value *pMin = ph7_array_fetch(pOpts,"min_range",(int)sizeof("min_range")-1);` |
|      7 | 6711 | `			ph7_value *pMax = ph7_array_fetch(pOpts,"max_range",(int)sizeof("max_range")-1);` |
|      7 | 6712 | `			if( pMin && v<ph7_value_to_int64(pMin) ){ goto fail; }` |
|      7 | 6713 | `			if( pMax && v>ph7_value_to_int64(pMax) ){ goto fail; }` |
|      2 | 6714 | `		}` |
|     29 | 6715 | `		ph7_result_int64(pCtx,v);` |
|     29 | 6716 | `		return PH7_OK;` |
|      - | 6717 | `	}` |
|     34 | 6718 | `	case FV_VALIDATE_FLOAT: {` |
|      - | 6719 | `		double d;` |
|     69 | 6720 | `		if( !FvValidateFloat(zVal,nVal,iFlags,&d) ){ goto fail; }` |
|     39 | 6721 | `		ph7_result_double(pCtx,d);` |
|     39 | 6722 | `		return PH7_OK;` |
|      - | 6723 | `	}` |
|     14 | 6724 | `	case FV_VALIDATE_BOOLEAN: {` |
|      - | 6725 | `		int b;` |
|     29 | 6726 | `		if( !FvValidateBool(zVal,nVal,&b) ){ goto fail; }` |
|     21 | 6727 | `		ph7_result_bool(pCtx,b);` |
|     21 | 6728 | `		return PH7_OK;` |
|      - | 6729 | `	}` |
|     25 | 6730 | `	case FV_VALIDATE_IP:     if( !FvValidateIp(zVal,nVal,iFlags) ){ goto fail; } goto pass;` |
|     11 | 6731 | `	case FV_VALIDATE_MAC:    if( !FvValidateMac(zVal,nVal) ){ goto fail; }       goto pass;` |
|     28 | 6732 | `	case FV_VALIDATE_EMAIL:  if( !FvValidateEmail(zVal,nVal) ){ goto fail; }     goto pass;` |
|     11 | 6733 | `	case FV_VALIDATE_DOMAIN: if( !FvValidateDomain(zVal,nVal) ){ goto fail; }    goto pass;` |
|     15 | 6734 | `	case FV_VALIDATE_URL:    if( !FvValidateUrl(zVal,nVal) ){ goto fail; }       goto pass;` |
|      3 | 6735 | `	case FV_VALIDATE_REGEXP: {` |
|      - | 6736 | `#ifdef PH7_ENABLE_PCRE` |
|      8 | 6737 | `		ph7_value *pRe = pOpts ? ph7_array_fetch(pOpts,"regexp",(int)sizeof("regexp")-1) : 0;` |
|      8 | 6738 | `		const char *zRe; int nRe, matched = 0;` |
|      8 | 6739 | `		if( pRe==0 ){` |
|      3 | 6740 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6741 | `				"filter_var(): \"regexp\" option is missing");` |
|      - | 6742 | `		}` |
|      5 | 6743 | `		zRe = ph7_value_to_string(pRe,&nRe);` |
|      5 | 6744 | `		if( PH7_PcreMatchQuiet(pCtx,zRe,nRe,zVal,nVal,&matched)!=SXRET_OK \|\| !matched ){ goto fail; }` |
|      3 | 6745 | `		goto pass;` |
|      - | 6746 | `#else` |
|      - | 6747 | `		goto fail;` |
|      - | 6748 | `#endif` |
|      - | 6749 | `	}` |
|      3 | 6750 | `	case FV_SANITIZE_NUMBER_INT:   FvSanitizeNumber(pCtx,zVal,nVal,0,0);      return PH7_OK;` |
|      5 | 6751 | `	case FV_SANITIZE_NUMBER_FLOAT: FvSanitizeNumber(pCtx,zVal,nVal,1,iFlags); return PH7_OK;` |
|     13 | 6752 | `	case FV_SANITIZE_SPECIAL_CHARS:      FvSanitizeSpecial(pCtx,zVal,nVal,iFlags); return PH7_OK;` |
|     25 | 6753 | `	case FV_SANITIZE_FULL_SPECIAL_CHARS: FvSanitizeFull(pCtx,zVal,nVal,iFlags);    return PH7_OK;` |
|      3 | 6754 | `	case FV_SANITIZE_EMAIL: FvSanitizeChars(pCtx,zVal,nVal,0); return PH7_OK;` |
|      3 | 6755 | `	case FV_SANITIZE_URL:   FvSanitizeChars(pCtx,zVal,nVal,1); return PH7_OK;` |
|     13 | 6756 | `	case FV_DEFAULT:` |
|      - | 6757 | `		/* FILTER_UNSAFE_RAW / FILTER_DEFAULT: pass through unchanged unless a` |
|      - | 6758 | `		 * STRIP/ENCODE flag is set, in which case apply the string filter. */` |
|     28 | 6759 | `		if( iFlags & FV_FLAG_STRING_MASK ){` |
|     15 | 6760 | `			FvSanitizeString(pCtx,zVal,nVal,iFlags);` |
|     15 | 6761 | `			return PH7_OK;` |
|      - | 6762 | `		}` |
|     14 | 6763 | `		goto pass;` |
|    ! 0 | 6764 | `	default:` |
|    ! 0 | 6765 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 6766 | `			"Unknown filter with ID %d",iFilter);` |
|    ! 0 | 6767 | `		break; /* unknown filter id -> fail */` |
|    ! 0 | 6768 | `	}` |
|     58 | 6769 | `fail:` |
|    118 | 6770 | `	if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|    114 | 6771 | `	else if( bNull ){ ph7_result_null(pCtx); }` |
|    108 | 6772 | `	else { ph7_result_bool(pCtx,0); }` |
|    118 | 6773 | `	return PH7_OK;` |
|     26 | 6774 | `pass: /* validation passed: return the (string) input unchanged */` |
|     54 | 6775 | `	ph7_result_string(pCtx,zVal,nVal);` |
|     54 | 6776 | `	return PH7_OK;` |
|    161 | 6777 | `}` |
|      - | 6778 | `/*` |
|      - | 6779 | ` * Parse the ($filter, $options) pair shared by filter_var()/filter_input() out` |
|      - | 6780 | ` * of apArg[iBase] ($filter) and apArg[iBase+1] ($options): $options is either a` |
|      - | 6781 | ` * plain flags int, or an array with 'flags' and an 'options' sub-array (whose` |
|      - | 6782 | ` * 'default' entry is the fallback value). Fills the four output pointers;` |
|      - | 6783 | ` * unset outputs keep the caller-provided defaults.` |
|      - | 6784 | ` */` |
|    328 | 6785 | `static void FvParseFilterArgs(int nArg,ph7_value **apArg,int iBase,` |
|      - | 6786 | `                              int *piFilter,int *piFlags,` |
|      - | 6787 | `                              ph7_value **ppOpts,ph7_value **ppDefault)` |
|      3 | 6788 | `{` |
|    331 | 6789 | `	if( nArg>iBase ){ *piFilter = ph7_value_to_int(apArg[iBase]); }` |
|    331 | 6790 | `	if( nArg>iBase+1 ){` |
|     88 | 6791 | `		if( ph7_value_is_array(apArg[iBase+1]) ){` |
|     42 | 6792 | `			ph7_value *pF = ph7_array_fetch(apArg[iBase+1],"flags",(int)sizeof("flags")-1);` |
|     42 | 6793 | `			if( pF ){ *piFlags = ph7_value_to_int(pF); }` |
|     42 | 6794 | `			*ppOpts = ph7_array_fetch(apArg[iBase+1],"options",(int)sizeof("options")-1);` |
|     42 | 6795 | `			if( *ppOpts && !ph7_value_is_array(*ppOpts) ){ *ppOpts = 0; }` |
|     42 | 6796 | `			if( *ppOpts ){ *ppDefault = ph7_array_fetch(*ppOpts,"default",(int)sizeof("default")-1); }` |
|     22 | 6797 | `		}else{` |
|     48 | 6798 | `			*piFlags = ph7_value_to_int(apArg[iBase+1]);` |
|      - | 6799 | `		}` |
|     43 | 6800 | `	}` |
|    331 | 6801 | `}` |
|      - | 6802 | `/*` |
|      - | 6803 | ` * filter_var($value, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 6804 | ` *  Validate or sanitize a value; see FvApplyFilter for the failure semantics.` |
|      - | 6805 | ` */` |
|    306 | 6806 | `static int PH7_builtin_filter_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6807 | `{` |
|    308 | 6808 | `	int iFilter = FV_DEFAULT, iFlags = 0;` |
|    308 | 6809 | `	ph7_value *pOpts = 0, *pDefault = 0;` |
|    308 | 6810 | `	if( nArg<1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    308 | 6811 | `	FvParseFilterArgs(nArg,apArg,1,&iFilter,&iFlags,&pOpts,&pDefault);` |
|    308 | 6812 | `	return FvApplyFilter(pCtx,apArg[0],iFilter,iFlags,pOpts,pDefault);` |
|    155 | 6813 | `}` |
|      - | 6814 | `/*` |
|      - | 6815 | ` * filter_input($type, $var_name, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 6816 | ` *  Look up $var_name in the requested INPUT_* superglobal, then apply the` |
|      - | 6817 | ` *  filter. Semantics verified byte-for-byte against php 8.5:` |
|      - | 6818 | ` *   - variable NOT set: 'default' option wins, else false when` |
|      - | 6819 | ` *     FILTER_NULL_ON_FAILURE is set, else null. (Note the null/false roles are` |
|      - | 6820 | ` *     INVERTED relative to a present value that fails validation, which yields` |
|      - | 6821 | ` *     default > null-if-NULL_ON_FAILURE > false via FvApplyFilter.)` |
|      - | 6822 | ` *   - variable present: delegate to FvApplyFilter.` |
|      - | 6823 | ` *  Divergence: php reads a SAPI snapshot of the original request variables` |
|      - | 6824 | ` *  captured at startup; PHL reads the live superglobal. In CLI they match for` |
|      - | 6825 | ` *  the SAPI-registered keys (SCRIPT_NAME/PHP_SELF/DOCUMENT_ROOT); keys added` |
|      - | 6826 | ` *  only to the live $_SERVER (REQUEST_TIME/PWD/…) are visible here but not in` |
|      - | 6827 | ` *  php's snapshot.` |
|      - | 6828 | ` */` |
|     24 | 6829 | `static int PH7_builtin_filter_input(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6830 | `{` |
|     26 | 6831 | `	int iType, iFilter = FV_DEFAULT, iFlags = 0;` |
|     26 | 6832 | `	ph7_value *pOpts = 0, *pDefault = 0, *pSuper, *pElem;` |
|      - | 6833 | `	const char *zVar, *zSuper; int nVar; sxu32 nSuper;` |
|     26 | 6834 | `	if( nArg<2 ){` |
|    ! 0 | 6835 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 6836 | `			"filter_input() expects at least 2 arguments, %d given",nArg);` |
|      - | 6837 | `	}` |
|     26 | 6838 | `	iType = ph7_value_to_int(apArg[0]);` |
|     26 | 6839 | `	switch( iType ){` |
|      3 | 6840 | `	case 0: zSuper = "_POST";   nSuper = (sxu32)sizeof("_POST")-1;   break; /* INPUT_POST */` |
|      3 | 6841 | `	case 1: zSuper = "_GET";    nSuper = (sxu32)sizeof("_GET")-1;    break; /* INPUT_GET */` |
|    ! 0 | 6842 | `	case 2: zSuper = "_COOKIE"; nSuper = (sxu32)sizeof("_COOKIE")-1; break; /* INPUT_COOKIE */` |
|    ! 0 | 6843 | `	case 4: zSuper = "_ENV";    nSuper = (sxu32)sizeof("_ENV")-1;    break; /* INPUT_ENV */` |
|     19 | 6844 | `	case 5: zSuper = "_SERVER"; nSuper = (sxu32)sizeof("_SERVER")-1; break; /* INPUT_SERVER */` |
|      1 | 6845 | `	default:` |
|      3 | 6846 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6847 | `			"filter_input(): Argument #1 ($type) must be an INPUT_* constant");` |
|      - | 6848 | `	}` |
|     23 | 6849 | `	zVar = ph7_value_to_string(apArg[1],&nVar);` |
|     23 | 6850 | `	FvParseFilterArgs(nArg,apArg,2,&iFilter,&iFlags,&pOpts,&pDefault);` |
|      - | 6851 | `	/* Resolve the variable from the superglobal (missing/non-array -> not set). */` |
|     23 | 6852 | `	pSuper = PH7_VmExtractSuper(pCtx->pVm,zSuper,nSuper);` |
|     23 | 6853 | `	pElem = (pSuper && ph7_value_is_array(pSuper))` |
|     33 | 6854 | `		? ph7_array_fetch(pSuper,zVar,nVar) : 0;` |
|     23 | 6855 | `	if( pElem==0 ){` |
|      - | 6856 | `		/* Variable not set: default > false(if NULL_ON_FAILURE) > null. Note the` |
|      - | 6857 | `		 * false/null roles are inverted vs FvApplyFilter's present-but-fails path. */` |
|     13 | 6858 | `		if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|      9 | 6859 | `		else if( iFlags & FV_NULL_ON_FAILURE ){ ph7_result_bool(pCtx,0); }` |
|      7 | 6860 | `		else { ph7_result_null(pCtx); }` |
|     13 | 6861 | `		return PH7_OK;` |
|      - | 6862 | `	}` |
|     11 | 6863 | `	return FvApplyFilter(pCtx,pElem,iFilter,iFlags,pOpts,pDefault);` |
|     14 | 6864 | `}` |
|      - | 6865 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6866 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6867 | `/*` |
|      - | 6868 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 6869 |  |
|      - | 6870 | ` */` |
|      4 | 6871 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 6872 | `	const char *zInput, /* Raw input */` |
|      - | 6873 | `	int nByte,  /* Input length */` |
|      - | 6874 | `	int delim,  /* Delimiter */` |
|      - | 6875 | `	int encl,   /* Enclosure */` |
|      - | 6876 | `	int escape,  /* Escape character */` |
|      - | 6877 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 6878 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 6879 | `	)` |
|      1 | 6880 | `{` |
|      5 | 6881 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 6882 | `	const char *zIn = zInput;` |
|      - | 6883 | `	const char *zPtr;` |
|      - | 6884 | `	int isEnc;` |
|      - | 6885 | `	/* Start processing */` |
|      8 | 6886 | `	for(;;){` |
|     17 | 6887 | `		if( zIn >= zEnd ){` |
|      - | 6888 | `			/* No more input to process */` |
|      5 | 6889 | `			break;` |
|      - | 6890 | `		}` |
|     13 | 6891 | `		isEnc = 0;` |
|     13 | 6892 | `		zPtr = zIn;` |
|      - | 6893 | `		/* Find the first delimiter */` |
|     27 | 6894 | `		while( zIn < zEnd ){` |
|     23 | 6895 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 6896 | `				/* Delimiter found,break imediately */` |
|      5 | 6897 | `				break;` |
|     15 | 6898 | `			}else if( zIn[0] == encl ){` |
|      - | 6899 | `				/* Inside enclosure? */` |
|    ! 0 | 6900 | `				isEnc = !isEnc;` |
|     15 | 6901 | `			}else if( zIn[0] == escape ){` |
|      - | 6902 | `				/* Escape sequence */` |
|    ! 0 | 6903 | `				zIn++;` |
|    ! 0 | 6904 | `			}` |
|      - | 6905 | `			/* Advance the cursor */` |
|     15 | 6906 | `			zIn++;` |
|      1 | 6907 | `		}` |
|     13 | 6908 | `		if( zIn > zPtr ){` |
|     13 | 6909 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 6910 | `			sxi32 rc;` |
|      - | 6911 | `			/* Invoke the supllied callback */` |
|     13 | 6912 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 6913 | `				zPtr++;` |
|    ! 0 | 6914 | `				nByteChunk-=2;` |
|    ! 0 | 6915 | `			}` |
|     13 | 6916 | `			if( nByteChunk > 0 ){` |
|     13 | 6917 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 6918 | `				if( rc == SXERR_ABORT ){` |
|      - | 6919 | `					/* User callback request an operation abort */` |
|    ! 0 | 6920 | `					break;` |
|      - | 6921 | `				}` |
|      6 | 6922 | `			}` |
|      6 | 6923 | `		}` |
|      - | 6924 | `		/* Ignore trailing delimiter */` |
|     21 | 6925 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 6926 | `			zIn++;` |
|      1 | 6927 | `		}` |
|      1 | 6928 | `	}` |
|      5 | 6929 | `	return SXRET_OK;` |
|      1 | 6930 | `}` |
|      - | 6931 | `/*` |
|      - | 6932 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 6933 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 6934 | ` * argument to this callback.` |
|      - | 6935 | ` */` |
|     12 | 6936 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 6937 | `{` |
|     13 | 6938 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 6939 | `	ph7_value sEntry;` |
|      - | 6940 | `	SyString sToken;` |
|      - | 6941 | `	/* Insert the token in the given array */` |
|     13 | 6942 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 6943 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 6944 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 6945 | `	if( sToken.nByte < 1){` |
|    ! 0 | 6946 | `		return SXRET_OK;` |
|      - | 6947 | `	}` |
|     13 | 6948 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 6949 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 6950 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 6951 | `	return SXRET_OK;` |
|      7 | 6952 | `}` |
|      - | 6953 | `/*` |
|      - | 6954 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 6955 | ` *  Parse a CSV string into an array.` |
|      - | 6956 | ` * Parameters` |
|      - | 6957 | ` *  $input` |
|      - | 6958 | ` *   The string to parse.` |
|      - | 6959 | ` *  $delimiter` |
|      - | 6960 | ` *   Set the field delimiter (one character only).` |
|      - | 6961 | ` *  $enclosure` |
|      - | 6962 | ` *   Set the field enclosure character (one character only).` |
|      - | 6963 | ` *  $escape` |
|      - | 6964 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 6965 | ` * Return` |
|      - | 6966 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 6967 | ` */` |
|      2 | 6968 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6969 | `{` |
|      - | 6970 | `	const char *zInput,*zPtr;` |
|      - | 6971 | `	ph7_value *pArray;` |
|      3 | 6972 | `	int delim  = ',';   /* Delimiter */` |
|      3 | 6973 | `	int encl   = '"' ;  /* Enclosure */` |
|      3 | 6974 | `	int escape = '\\';  /* Escape character */` |
|      - | 6975 | `	int nLen;` |
|      3 | 6976 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6977 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 6978 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6979 | `		return PH7_OK;` |
|      - | 6980 | `	}` |
|      - | 6981 | `	/* Extract the raw input */` |
|      3 | 6982 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 6983 | `	if( nArg > 1 ){` |
|      - | 6984 | `		int i;` |
|      3 | 6985 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 6986 | `			/* Extract the delimiter */` |
|      3 | 6987 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 6988 | `			if( i > 0 ){` |
|      3 | 6989 | `				delim = zPtr[0];` |
|      1 | 6990 | `			}` |
|      1 | 6991 | `		}` |
|      3 | 6992 | `		if( nArg > 2 ){` |
|      3 | 6993 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 6994 | `				/* Extract the enclosure */` |
|      3 | 6995 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 6996 | `				if( i > 0 ){` |
|      3 | 6997 | `					encl = zPtr[0];` |
|      1 | 6998 | `				}` |
|      1 | 6999 | `			}` |
|      3 | 7000 | `			if( nArg > 3 ){` |
|      3 | 7001 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 7002 | `					/* Extract the escape character */` |
|      3 | 7003 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 7004 | `					if( i > 0 ){` |
|      3 | 7005 | `						escape = zPtr[0];` |
|      1 | 7006 | `					}` |
|      1 | 7007 | `				}` |
|      1 | 7008 | `			}` |
|      1 | 7009 | `		}` |
|      1 | 7010 | `	}` |
|      - | 7011 | `	/* Create our array */` |
|      3 | 7012 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 7013 | `	if( pArray == 0 ){` |
|      - | 7014 | `		/* Surface a fatal instead of silently returning null on OOM */` |
|    ! 0 | 7015 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7016 | `	}` |
|      - | 7017 | `	/* Parse the raw input */` |
|      3 | 7018 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 7019 | `	/* Return the freshly created array */` |
|      3 | 7020 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 7021 | `	return PH7_OK;` |
|      2 | 7022 | `}` |
|      - | 7023 | `/*` |
|      - | 7024 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 7025 | ` * container.` |
|      - | 7026 | ` * Refer to [strip_tags()].` |
|      - | 7027 | ` */` |
|     10 | 7028 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 7029 | `{` |
|     11 | 7030 | `	const char *zEnd = &zTag[nByte];` |
|      - | 7031 | `	const char *zPtr;` |
|      - | 7032 | `	SyString sEntry;` |
|      - | 7033 | `	/* Strip tags */` |
|     10 | 7034 | `	for(;;){` |
|     45 | 7035 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 7036 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 7037 | `				zTag++;` |
|      1 | 7038 | `		}` |
|     21 | 7039 | `		if( zTag >= zEnd ){` |
|     11 | 7040 | `			break;` |
|      - | 7041 | `		}` |
|     11 | 7042 | `		zPtr = zTag;` |
|      - | 7043 | `		/* Delimit the tag */` |
|     25 | 7044 | `		while(zTag < zEnd ){` |
|     25 | 7045 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 7046 | `				/* UTF-8 stream */` |
|      3 | 7047 | `				zTag++;` |
|      5 | 7048 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 7049 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 7050 | `				break;` |
|    ! 0 | 7051 | `			}else{` |
|     13 | 7052 | `				zTag++;` |
|      - | 7053 | `			}` |
|      1 | 7054 | `		}` |
|     11 | 7055 | `		if( zTag > zPtr ){` |
|      - | 7056 | `			/* Perform the insertion */` |
|     11 | 7057 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 7058 | `			SyStringFullTrim(&sEntry);` |
|     11 | 7059 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 7060 | `		}` |
|      - | 7061 | `		/* Jump the trailing '>' */` |
|     11 | 7062 | `		zTag++;` |
|      1 | 7063 | `	}` |
|     11 | 7064 | `	return SXRET_OK;` |
|      1 | 7065 | `}` |
|      - | 7066 | `/*` |
|      - | 7067 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 7068 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 7069 | ` * Refer to [strip_tags()].` |
|      - | 7070 | ` */` |
|     36 | 7071 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 7072 | `{` |
|     37 | 7073 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 7074 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 7075 | `		SyString sTag;` |
|     85 | 7076 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 7077 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 7078 | `			zTag++;` |
|      1 | 7079 | `		}` |
|      - | 7080 | `		/* Delimit the tag */` |
|     25 | 7081 | `		zCur = zTag;` |
|     77 | 7082 | `		while(zTag < zEnd ){` |
|     77 | 7083 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 7084 | `				/* UTF-8 stream */` |
|      5 | 7085 | `				zTag++;` |
|      9 | 7086 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 7087 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 7088 | `				break;` |
|    ! 0 | 7089 | `			}else{` |
|     49 | 7090 | `				zTag++;` |
|      - | 7091 | `			}` |
|      1 | 7092 | `		}` |
|     25 | 7093 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 7094 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 7095 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 7096 | `		if( sTag.nByte > 0 ){` |
|      - | 7097 | `			SyString *aEntry,*pEntry;` |
|      - | 7098 | `			sxi32 rc;` |
|      - | 7099 | `			sxu32 n;` |
|      - | 7100 | `			/* Perform the lookup */` |
|     25 | 7101 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 7102 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 7103 | `				pEntry = &aEntry[n];` |
|      - | 7104 | `				/* Do the comparison */` |
|     25 | 7105 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 7106 | `				if( !rc ){` |
|     21 | 7107 | `					return SXRET_OK;` |
|      - | 7108 | `				}` |
|      3 | 7109 | `			}` |
|      2 | 7110 | `		}` |
|      2 | 7111 | `	}` |
|      - | 7112 | `	/* No such tag */` |
|     17 | 7113 | `	return SXERR_NOTFOUND;` |
|     19 | 7114 | `}` |
|      - | 7115 | `/*` |
|      - | 7116 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 7117 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 7118 | ` * Refer to [strip_tags()].` |
|      - | 7119 | ` */` |
|     16 | 7120 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 7121 | `{` |
|     17 | 7122 | `	const char *zEnd = &zIn[nByte];` |
|      - | 7123 | `	const char *zPtr,*zTag;` |
|      - | 7124 | `	SySet sSet;` |
|      - | 7125 | `	/* initialize the set of allowed tags */` |
|     17 | 7126 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 7127 | `	if( nTaglen > 0 ){` |
|      - | 7128 | `		/* Set of allowed tags */` |
|     11 | 7129 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 7130 | `	}` |
|      - | 7131 | `	/* Set the empty string */` |
|     17 | 7132 | `	ph7_result_string(pCtx,"",0);` |
|      - | 7133 | `	/* Start processing */` |
|     26 | 7134 | `	for(;;){` |
|     53 | 7135 | `		if(zIn >= zEnd){` |
|      - | 7136 | `			/* No more input to process */` |
|     15 | 7137 | `			break;` |
|      - | 7138 | `		}` |
|     39 | 7139 | `		zPtr = zIn;` |
|      - | 7140 | `		/* Find a tag */` |
|    133 | 7141 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 7142 | `			zIn++;` |
|      1 | 7143 | `		}` |
|     39 | 7144 | `		if( zIn > zPtr ){` |
|      - | 7145 | `			/* Consume raw input */` |
|     21 | 7146 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 7147 | `		}` |
|      - | 7148 | `		/* Ignore trailing null bytes */` |
|     39 | 7149 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 7150 | `			zIn++;` |
|    ! 0 | 7151 | `		}` |
|     39 | 7152 | `		if(zIn >= zEnd){` |
|      - | 7153 | `			/* No more input to process */` |
|      3 | 7154 | `			break;` |
|      - | 7155 | `		}` |
|     37 | 7156 | `		if( zIn[0] == '<' ){` |
|      - | 7157 | `			sxi32 rc;` |
|     37 | 7158 | `			zTag = zIn++;` |
|      - | 7159 | `			/* Delimit the tag */` |
|    127 | 7160 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 7161 | `				zIn++;` |
|      1 | 7162 | `			}` |
|     37 | 7163 | `			if( zIn < zEnd ){` |
|     37 | 7164 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 7165 | `			}` |
|      - | 7166 | `			/* Query the set */` |
|     37 | 7167 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 7168 | `			if( rc == SXRET_OK ){` |
|      - | 7169 | `				/* Keep the tag */` |
|     21 | 7170 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 7171 | `			}` |
|     18 | 7172 | `		}` |
|      1 | 7173 | `	}` |
|      - | 7174 | `	/* Cleanup */` |
|     17 | 7175 | `	SySetRelease(&sSet);` |
|     17 | 7176 | `	return SXRET_OK;` |
|      1 | 7177 | `}` |
|      - | 7178 | `/*` |
|      - | 7179 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 7180 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 7181 | ` * Parameters` |
|      - | 7182 | ` *  $str` |
|      - | 7183 | ` *  The input string.` |
|      - | 7184 | ` * $allowable_tags` |
|      - | 7185 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 7186 | ` * Return` |
|      - | 7187 | ` *  Returns the stripped string.` |
|      - | 7188 | ` */` |
|     14 | 7189 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7190 | `{` |
|     15 | 7191 | `	const char *zTaglist = 0;` |
|      - | 7192 | `	const char *zString;` |
|     15 | 7193 | `	int nTaglen = 0;` |
|      - | 7194 | `	int nLen;` |
|     15 | 7195 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7196 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 7197 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7198 | `		return PH7_OK;` |
|      - | 7199 | `	}` |
|      - | 7200 | `	/* Point to the raw string */` |
|     15 | 7201 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 7202 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 7203 | `		/* Allowed tag */` |
|     11 | 7204 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 7205 | `	}` |
|      - | 7206 | `	/* Process input */` |
|     15 | 7207 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 7208 | `	return PH7_OK;` |
|      8 | 7209 | `}` |
|      - | 7210 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7211 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7212 | `/*` |
|      - | 7213 | ` * string str_shuffle(string $str)` |
|      - | 7214 |  |
|      - | 7215 | ` *  Randomly shuffles a string.` |
|      - | 7216 | ` * Parameters` |
|      - | 7217 | ` *  $str` |
|      - | 7218 | ` *   The input string.` |
|      - | 7219 | ` * Return` |
|      - | 7220 | ` *  Returns the shuffled string.` |
|      - | 7221 | ` */` |
|     10 | 7222 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7223 | `{` |
|      - | 7224 | `	const char *zString;` |
|      - | 7225 | `	int nLen,i,c;` |
|      - | 7226 | `	sxu32 iR;` |
|     11 | 7227 | `	if( nArg < 1 ){` |
|      - | 7228 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7229 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7230 | `		return PH7_OK;` |
|      - | 7231 | `	}` |
|      - | 7232 | `	/* Extract the target string */` |
|     11 | 7233 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 7234 | `	if( nLen < 1 ){` |
|      - | 7235 | `		/* Nothing to shuffle */` |
|      3 | 7236 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 7237 | `		return PH7_OK;` |
|      - | 7238 | `	}` |
|      - | 7239 | `	/* Shuffle the string */` |
|     43 | 7240 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 7241 | `		/* Generate a random number first */` |
|     35 | 7242 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 7243 | `		/* Extract a random offset */` |
|     35 | 7244 | `		c = zString[iR % nLen];` |
|      - | 7245 | `		/* Append it */` |
|     35 | 7246 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 7247 | `	}` |
|      9 | 7248 | `	return PH7_OK;` |
|      6 | 7249 | `}` |
|      - | 7250 | `/*` |
|      - | 7251 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 7252 | ` *  Convert a string to an array.` |
|      - | 7253 | ` * Parameters` |
|      - | 7254 | ` * $string` |
|      - | 7255 | ` *  The input string.` |
|      - | 7256 | ` * $split_length` |
|      - | 7257 | ` *  Maximum length of the chunk.` |
|      - | 7258 | ` * Return` |
|      - | 7259 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 7260 | ` *  except possibly the last one which may be shorter.` |
|      - | 7261 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 7262 | ` *  as the first (and only) array element.` |
|      - | 7263 | ` *  An empty string returns an empty array.` |
|      - | 7264 | ` * Errors` |
|      - | 7265 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 7266 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 7267 | ` *  ValueError if $split_length is less than 1.` |
|      - | 7268 | ` */` |
|     26 | 7269 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 7270 | `{` |
|      - | 7271 | `	const char *zString,*zEnd;` |
|      - | 7272 | `	ph7_value *pArray,*pValue;` |
|      - | 7273 | `	int split_len;` |
|      - | 7274 | `	int nLen;` |
|     29 | 7275 | `	if( nArg < 1 ){` |
|    ! 0 | 7276 | `		return PH7_VmThrowException(pCtx,` |
|      - | 7277 | `			"ArgumentCountError",` |
|      - | 7278 | `			"str_split() expects at least 1 argument, %d given",` |
|    ! 0 | 7279 | `			nArg` |
|      - | 7280 | `			);` |
|      - | 7281 | `	}` |
|      - | 7282 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     39 | 7283 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     42 | 7284 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     26 | 7285 | `	    ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 7286 | `		return PH7_VmThrowException(pCtx,` |
|      - | 7287 | `			"TypeError",` |
|      - | 7288 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 7289 | `			ph7_type_name(apArg[0])` |
|      - | 7290 | `			);` |
|      - | 7291 | `	}` |
|      - | 7292 | `	/* Point to the target string */` |
|     29 | 7293 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     29 | 7294 | `	split_len = (int)sizeof(char);` |
|     29 | 7295 | `	if( nArg > 1 ){` |
|      - | 7296 | `		/* Split length */` |
|     17 | 7297 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     17 | 7298 | `		if( split_len < 1 ){` |
|      6 | 7299 | `			return PH7_VmThrowException(pCtx,` |
|      - | 7300 | `				"ValueError",` |
|      - | 7301 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 7302 | `				);` |
|      - | 7303 | `		}` |
|     11 | 7304 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 7305 | `			split_len = nLen;` |
|      1 | 7306 | `		}` |
|      5 | 7307 | `	}` |
|      - | 7308 | `	/* Create the array and the scalar value */` |
|     23 | 7309 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 7310 | `	/*Chunk value */` |
|     23 | 7311 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     23 | 7312 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 7313 | `		/* Return FALSE */` |
|    ! 0 | 7314 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7315 | `		return PH7_OK;` |
|      - | 7316 | `	}` |
|      - | 7317 | `	/* Point to the end of the string */` |
|     23 | 7318 | `	zEnd = &zString[nLen];` |
|      - | 7319 | `	/* Perform the requested operation */` |
|    131 | 7320 | `	for(;;){` |
|      - | 7321 | `		int nMax;` |
|    143 | 7322 | `		if( zString >= zEnd ){` |
|      - | 7323 | `			/* No more input to process */` |
|     23 | 7324 | `			break;` |
|      - | 7325 | `		}` |
|    121 | 7326 | `		nMax = (int)(zEnd-zString);` |
|    121 | 7327 | `		if( nMax < split_len ){` |
|      3 | 7328 | `			split_len = nMax;` |
|      1 | 7329 | `		}` |
|      - | 7330 | `		/* Copy the current chunk */` |
|    121 | 7331 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 7332 | `		/* Insert it */` |
|    121 | 7333 | `		if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){ /* Will make it's own copy */` |
|    ! 0 | 7334 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 7335 | `		}` |
|      - | 7336 | `		/* reset the string cursor */` |
|    121 | 7337 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 7338 | `		/* Update position */` |
|    121 | 7339 | `		zString += split_len;` |
|      1 | 7340 | `	}` |
|      - | 7341 | `	/*` |
|      - | 7342 | `	 * Return the array.` |
|      - | 7343 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 7344 | `	 * upon we return from this function.` |
|      - | 7345 | `	 */` |
|     23 | 7346 | `	ph7_result_value(pCtx,pArray);` |
|     23 | 7347 | `	return PH7_OK;` |
|     16 | 7348 | `}` |
|      - | 7349 | `/*` |
|      - | 7350 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 7351 | ` * Refer to [strspn()].` |
|      - | 7352 | ` */` |
|     28 | 7353 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 7354 | `{` |
|     29 | 7355 | `	const char *zIn = *pzIn;` |
|      - | 7356 | `	const char *zPtr;` |
|      - | 7357 | `	/* Ignore leading white spaces */` |
|     29 | 7358 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 7359 | `		zIn++;` |
|    ! 0 | 7360 | `	}` |
|     29 | 7361 | `	if( zIn >= zEnd ){` |
|      - | 7362 | `		/* End of input */` |
|    ! 0 | 7363 | `		return SXERR_EOF;` |
|      - | 7364 | `	}` |
|     29 | 7365 | `	zPtr = zIn;` |
|      - | 7366 | `	/* Extract the token */` |
|    201 | 7367 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 7368 | `		zIn++;` |
|      1 | 7369 | `	}` |
|     29 | 7370 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 7371 | `	/* Synchronize pointers */` |
|     29 | 7372 | `	*pzIn = zIn;` |
|      - | 7373 | `	/* Return to the caller */` |
|     29 | 7374 | `	return SXRET_OK;` |
|     15 | 7375 | `}` |
|      - | 7376 | `/*` |
|      - | 7377 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 7378 | ` * return the longest match.` |
|      - | 7379 | ` * Refer to [strspn()].` |
|      - | 7380 | ` */` |
|     18 | 7381 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 7382 | `{` |
|     19 | 7383 | `	const char *zEnd = &zString[nLen];` |
|     19 | 7384 | `	const char *zIn = zString;` |
|      - | 7385 | `	int i,c;` |
|     45 | 7386 | `	for(;;){` |
|     91 | 7387 | `		if( zString >= zEnd ){` |
|      7 | 7388 | `			break;` |
|      - | 7389 | `		}` |
|      - | 7390 | `		/* Extract current character */` |
|     85 | 7391 | `		c = zString[0];` |
|      - | 7392 | `		/* Perform the lookup */` |
|    383 | 7393 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 7394 | `			if( c == zMask[i] ){` |
|      - | 7395 | `				/* Character found */` |
|     73 | 7396 | `				break;` |
|      - | 7397 | `			}` |
|    150 | 7398 | `		}` |
|     85 | 7399 | `		if( i >= nMaskLen ){` |
|      - | 7400 | `			/* Character not in the current mask,break immediately */` |
|     13 | 7401 | `			break;` |
|      - | 7402 | `		}` |
|      - | 7403 | `		/* Advance cursor */` |
|     73 | 7404 | `		zString++;` |
|      1 | 7405 | `	}` |
|      - | 7406 | `	/* Longest match */` |
|     19 | 7407 | `	return (int)(zString-zIn);` |
|      1 | 7408 | `}` |
|      - | 7409 | `/*` |
|      - | 7410 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 7411 | ` * Refer to [strcspn()].` |
|      - | 7412 | ` */` |
|     10 | 7413 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 7414 | `{` |
|     11 | 7415 | `	const char *zEnd = &zString[nLen];` |
|     11 | 7416 | `	const char *zIn = zString;` |
|      - | 7417 | `	int i,c;` |
|     12 | 7418 | `	for(;;){` |
|     25 | 7419 | `		if( zString >= zEnd ){` |
|      3 | 7420 | `			break;` |
|      - | 7421 | `		}` |
|      - | 7422 | `		/* Extract current character */` |
|     23 | 7423 | `		c = zString[0];` |
|      - | 7424 | `		/* Perform the lookup */` |
|     51 | 7425 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 7426 | `			if( c == zMask[i] ){` |
|      9 | 7427 | `				break;` |
|      - | 7428 | `			}` |
|     15 | 7429 | `		}` |
|     23 | 7430 | `		if( i < nMaskLen ){` |
|      - | 7431 | `			/* Character in the current mask,break immediately */` |
|      9 | 7432 | `			break;` |
|      - | 7433 | `		}` |
|      - | 7434 | `		/* Advance cursor */` |
|     15 | 7435 | `		zString++;` |
|      1 | 7436 | `	}` |
|      - | 7437 | `	/* Longest match */` |
|     11 | 7438 | `	return (int)(zString-zIn);` |
|      1 | 7439 | `}` |
|      - | 7440 | `/*` |
|      - | 7441 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 7442 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 7443 | ` *  of characters contained within a given mask.` |
|      - | 7444 | ` * Parameters` |
|      - | 7445 | ` * $str` |
|      - | 7446 | ` *  The input string.` |
|      - | 7447 | ` * $mask` |
|      - | 7448 | ` *  The list of allowable characters.` |
|      - | 7449 | ` * $start` |
|      - | 7450 | ` *  The position in subject to start searching.` |
|      - | 7451 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 7452 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 7453 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 7454 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 7455 | ` *  start'th position from the end of subject.` |
|      - | 7456 | ` * $length` |
|      - | 7457 | ` *  The length of the segment from subject to examine.` |
|      - | 7458 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 7459 | ` *  characters after the starting position.` |
|      - | 7460 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 7461 | ` *  position up to length characters from the end of subject.` |
|      - | 7462 | ` * Return` |
|      - | 7463 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 7464 | ` * in mask.` |
|      - | 7465 | ` */` |
|     24 | 7466 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7467 | `{` |
|      - | 7468 | `	const char *zString,*zMask,*zEnd;` |
|      - | 7469 | `	int iMasklen,iLen;` |
|      - | 7470 | `	SyString sToken;` |
|     25 | 7471 | `	int iCount = 0;` |
|      - | 7472 | `	int rc;` |
|     25 | 7473 | `	if( nArg < 2 ){` |
|      - | 7474 | `		/* Missing agruments,return zero */` |
|    ! 0 | 7475 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7476 | `		return PH7_OK;` |
|      - | 7477 | `	}` |
|      - | 7478 | `	/* Extract the target string */` |
|     25 | 7479 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7480 | `	/* Extract the mask */` |
|     25 | 7481 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 7482 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 7483 | `		/* Nothing to process,return zero */` |
|      7 | 7484 | `		ph7_result_int(pCtx,0);` |
|      7 | 7485 | `		return PH7_OK;` |
|      - | 7486 | `	}` |
|     19 | 7487 | `	if( nArg > 2 ){` |
|      - | 7488 | `		int nOfft;` |
|      - | 7489 | `		/* Extract the offset */` |
|      9 | 7490 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 7491 | `		if( nOfft < 0 ){` |
|    ! 0 | 7492 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 7493 | `			if( zBase > zString ){` |
|    ! 0 | 7494 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 7495 | `				zString = zBase;` |
|    ! 0 | 7496 | `			}else{` |
|      - | 7497 | `				/* Invalid offset */` |
|    ! 0 | 7498 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7499 | `				return PH7_OK;` |
|      - | 7500 | `			}` |
|    ! 0 | 7501 | `		}else{` |
|      9 | 7502 | `			if( nOfft >= iLen ){` |
|      - | 7503 | `				/* Invalid offset */` |
|    ! 0 | 7504 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7505 | `				return PH7_OK;` |
|    ! 0 | 7506 | `			}else{` |
|      - | 7507 | `				/* Update offset */` |
|      9 | 7508 | `				zString += nOfft;` |
|      9 | 7509 | `				iLen -= nOfft;` |
|      - | 7510 | `			}` |
|      - | 7511 | `		}` |
|      9 | 7512 | `		if( nArg > 3 ){` |
|      - | 7513 | `			int iUserlen;` |
|      - | 7514 | `			/* Extract the desired length */` |
|      9 | 7515 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 7516 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 7517 | `				iLen = iUserlen;` |
|      2 | 7518 | `			}` |
|      4 | 7519 | `		}` |
|      4 | 7520 | `	}` |
|      - | 7521 | `	/* Point to the end of the string */` |
|     19 | 7522 | `	zEnd = &zString[iLen];` |
|      - | 7523 | `	/* Extract the first non-space token */` |
|     19 | 7524 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 7525 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 7526 | `		/* Compare against the current mask */` |
|     19 | 7527 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 7528 | `	}` |
|      - | 7529 | `	/* Longest match */` |
|     19 | 7530 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 7531 | `	return PH7_OK;` |
|     13 | 7532 | `}` |
|      - | 7533 | `/*` |
|      - | 7534 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 7535 | ` *  Find length of initial segment not matching mask.` |
|      - | 7536 | ` * Parameters` |
|      - | 7537 | ` * $str` |
|      - | 7538 | ` *  The input string.` |
|      - | 7539 | ` * $mask` |
|      - | 7540 | ` *  The list of not allowed characters.` |
|      - | 7541 | ` * $start` |
|      - | 7542 | ` *  The position in subject to start searching.` |
|      - | 7543 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 7544 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 7545 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 7546 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 7547 | ` *  start'th position from the end of subject.` |
|      - | 7548 | ` * $length` |
|      - | 7549 | ` *  The length of the segment from subject to examine.` |
|      - | 7550 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 7551 | ` *  characters after the starting position.` |
|      - | 7552 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 7553 | ` *  position up to length characters from the end of subject.` |
|      - | 7554 | ` * Return` |
|      - | 7555 | ` *  Returns the length of the segment as an integer.` |
|      - | 7556 | ` */` |
|     14 | 7557 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7558 | `{` |
|      - | 7559 | `	const char *zString,*zMask,*zEnd;` |
|      - | 7560 | `	int iMasklen,iLen;` |
|      - | 7561 | `	SyString sToken;` |
|     15 | 7562 | `	int iCount = 0;` |
|      - | 7563 | `	int rc;` |
|     15 | 7564 | `	if( nArg < 2 ){` |
|      - | 7565 | `		/* Missing agruments,return zero */` |
|    ! 0 | 7566 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7567 | `		return PH7_OK;` |
|      - | 7568 | `	}` |
|      - | 7569 | `	/* Extract the target string */` |
|     15 | 7570 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7571 | `	/* Extract the mask */` |
|     15 | 7572 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 7573 | `	if( iLen < 1 ){` |
|      - | 7574 | `		/* Nothing to process,return zero */` |
|    ! 0 | 7575 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7576 | `		return PH7_OK;` |
|      - | 7577 | `	}` |
|     15 | 7578 | `	if( iMasklen < 1 ){` |
|      - | 7579 | `		/* No given mask,return the string length */` |
|      3 | 7580 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 7581 | `		return PH7_OK;` |
|      - | 7582 | `	}` |
|     13 | 7583 | `	if( nArg > 2 ){` |
|      - | 7584 | `		int nOfft;` |
|      - | 7585 | `		/* Extract the offset */` |
|     11 | 7586 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 7587 | `		if( nOfft < 0 ){` |
|    ! 0 | 7588 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 7589 | `			if( zBase > zString ){` |
|    ! 0 | 7590 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 7591 | `				zString = zBase;` |
|    ! 0 | 7592 | `			}else{` |
|      - | 7593 | `				/* Invalid offset */` |
|    ! 0 | 7594 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7595 | `				return PH7_OK;` |
|      - | 7596 | `			}` |
|    ! 0 | 7597 | `		}else{` |
|     11 | 7598 | `			if( nOfft >= iLen ){` |
|      - | 7599 | `				/* Invalid offset */` |
|      3 | 7600 | `				ph7_result_int(pCtx,0);` |
|      3 | 7601 | `				return PH7_OK;` |
|    ! 0 | 7602 | `			}else{` |
|      - | 7603 | `				/* Update offset */` |
|      9 | 7604 | `				zString += nOfft;` |
|      9 | 7605 | `				iLen -= nOfft;` |
|      - | 7606 | `			}` |
|      - | 7607 | `		}` |
|      9 | 7608 | `		if( nArg > 3 ){` |
|      - | 7609 | `			int iUserlen;` |
|      - | 7610 | `			/* Extract the desired length */` |
|    ! 0 | 7611 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 7612 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 7613 | `				iLen = iUserlen;` |
|    ! 0 | 7614 | `			}` |
|    ! 0 | 7615 | `		}` |
|      4 | 7616 | `	}` |
|      - | 7617 | `	/* Point to the end of the string */` |
|     11 | 7618 | `	zEnd = &zString[iLen];` |
|      - | 7619 | `	/* Extract the first non-space token */` |
|     11 | 7620 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 7621 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 7622 | `		/* Compare against the current mask */` |
|     11 | 7623 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 7624 | `	}` |
|      - | 7625 | `	/* Longest match */` |
|     11 | 7626 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 7627 | `	return PH7_OK;` |
|      8 | 7628 | `}` |
|      - | 7629 | `/*` |
|      - | 7630 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 7631 | ` *  Search a string for any of a set of characters.` |
|      - | 7632 | ` * Parameters` |
|      - | 7633 | ` *  $haystack` |
|      - | 7634 | ` *   The string where char_list is looked for.` |
|      - | 7635 | ` *  $char_list` |
|      - | 7636 | ` *   This parameter is case sensitive.` |
|      - | 7637 | ` * Return` |
|      - | 7638 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 7639 | ` */` |
|      4 | 7640 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7641 | `{` |
|      - | 7642 | `	const char *zString,*zList,*zEnd;` |
|      - | 7643 | `	int iLen,iListLen,i,c;` |
|      - | 7644 | `	sxu32 nOfft,nMax;` |
|      - | 7645 | `	sxi32 rc;` |
|      5 | 7646 | `	if( nArg < 2 ){` |
|      - | 7647 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7648 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7649 | `		return PH7_OK;` |
|      - | 7650 | `	}` |
|      - | 7651 | `	/* Extract the haystack and the char list */` |
|      5 | 7652 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 7653 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 7654 | `	if( iLen < 1 ){` |
|      - | 7655 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 7656 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7657 | `		return PH7_OK;` |
|      - | 7658 | `	}` |
|      - | 7659 | `	/* Point to the end of the string */` |
|      5 | 7660 | `	zEnd = &zString[iLen];` |
|      5 | 7661 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 7662 | `	/* perform the requested operation */` |
|     15 | 7663 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 7664 | `		c = zList[i];` |
|     11 | 7665 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 7666 | `		if( rc == SXRET_OK ){` |
|      5 | 7667 | `			if( nMax < nOfft ){` |
|      3 | 7668 | `				nOfft = nMax;` |
|      1 | 7669 | `			}` |
|      2 | 7670 | `		}` |
|      6 | 7671 | `	}` |
|      5 | 7672 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 7673 | `		/* No such substring,return FALSE */` |
|      3 | 7674 | `		ph7_result_bool(pCtx,0);` |
|      2 | 7675 | `	}else{` |
|      - | 7676 | `		/* Return the substring */` |
|      3 | 7677 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 7678 | `	}` |
|      5 | 7679 | `	return PH7_OK;` |
|      3 | 7680 | `}` |
|      - | 7681 | `/* SPDX-SnippetBegin */` |
|      - | 7682 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 7683 | `/* SPDX-License-Identifier: blessing */` |
|      - | 7684 | `/*` |
|      - | 7685 | ` * string soundex(string $str)` |
|      - | 7686 | ` *  Calculate the soundex key of a string.` |
|      - | 7687 | ` * Parameters` |
|      - | 7688 | ` *  $str` |
|      - | 7689 | ` *   The input string.` |
|      - | 7690 | ` * Return` |
|      - | 7691 | ` *  Returns the soundex key as a string.` |
|      - | 7692 | ` * Note:` |
|      - | 7693 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 7694 | ` * source tree.` |
|      - | 7695 | ` */` |
|     22 | 7696 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7697 | `{` |
|      - | 7698 | `	const unsigned char *zIn;` |
|      - | 7699 | `	char zResult[8];` |
|      - | 7700 | `	int i, j;` |
|      - | 7701 | `	static const unsigned char iCode[] = {` |
|      - | 7702 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7703 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7704 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7705 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7706 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 7707 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 7708 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 7709 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 7710 | `	};` |
|     23 | 7711 | `	if( nArg < 1 ){` |
|      - | 7712 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7713 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7714 | `		return PH7_OK;` |
|      - | 7715 | `	}` |
|     23 | 7716 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     35 | 7717 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     23 | 7718 | `	if( zIn[i] ){` |
|     17 | 7719 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 7720 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 7721 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 7722 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 7723 | `			if( code>0 ){` |
|     45 | 7724 | `				if( code!=prevcode ){` |
|     33 | 7725 | `					prevcode = (unsigned char)code;` |
|     33 | 7726 | `					zResult[j++] = (char)code + '0';` |
|     16 | 7727 | `				}` |
|     23 | 7728 | `			}else{` |
|     49 | 7729 | `				prevcode = 0;` |
|      - | 7730 | `			}` |
|     47 | 7731 | `		}` |
|     33 | 7732 | `		while( j<4 ){` |
|     17 | 7733 | `			zResult[j++] = '0';` |
|      1 | 7734 | `		}` |
|     17 | 7735 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 7736 | `	}else{` |
|      - | 7737 | `	  /* No alphabetic character: PHP returns "0000" (not the SQLite "?000"). */` |
|      7 | 7738 | `	  ph7_result_string(pCtx,"0000",4);` |
|      - | 7739 | `	}` |
|     23 | 7740 | `	return PH7_OK;` |
|     12 | 7741 | `}` |
|      - | 7742 | `/* SPDX-SnippetEnd */` |
|      - | 7743 | `/*` |
|      - | 7744 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 7745 | ` *  Wraps a string to a given number of characters.` |
|      - | 7746 | ` * Parameters` |
|      - | 7747 | ` *  $str` |
|      - | 7748 | ` *   The input string.` |
|      - | 7749 | ` * $width` |
|      - | 7750 | ` *  The column width.` |
|      - | 7751 | ` * $break` |
|      - | 7752 | ` *  The line is broken using the optional break parameter.` |
|      - | 7753 | ` * Return` |
|      - | 7754 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 7755 | ` */` |
|     26 | 7756 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7757 | `{` |
|      - | 7758 | `	const char *zIn,*zBreak;` |
|      - | 7759 | `	SyBlob sWorker;` |
|      - | 7760 | `	int iLen,iBreaklen,iWidth,iCut,iStart,iSpace,iCur;` |
|      - | 7761 | `	sxi32 rc;` |
|     27 | 7762 | `	if( nArg < 1 ){` |
|      - | 7763 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7764 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7765 | `		return PH7_OK;` |
|      - | 7766 | `	}` |
|      - | 7767 | `	/* Extract the input string */` |
|     27 | 7768 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7769 | `	/* Width (default 75; PHP allows 0/negative — break at every space). */` |
|     27 | 7770 | `	iWidth = 75;` |
|     27 | 7771 | `	if( nArg > 1 ){` |
|     27 | 7772 | `		iWidth = ph7_value_to_int(apArg[1]);` |
|     13 | 7773 | `	}` |
|      - | 7774 | `	/* Break string (default "\n"). */` |
|     27 | 7775 | `	zBreak = "\n";` |
|     27 | 7776 | `	iBreaklen = (int)sizeof(char);` |
|     27 | 7777 | `	if( nArg > 2 ){` |
|     13 | 7778 | `		zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      6 | 7779 | `	}` |
|      - | 7780 | `	/* Cut long words? (default false). */` |
|     27 | 7781 | `	iCut = 0;` |
|     27 | 7782 | `	if( nArg > 3 ){` |
|      7 | 7783 | `		iCut = ph7_value_to_bool(apArg[3]);` |
|      3 | 7784 | `	}` |
|     27 | 7785 | `	if( iLen < 1 ){` |
|      - | 7786 | `		/* PHP returns the empty string for empty input before validating the other args. */` |
|      5 | 7787 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 7788 | `		return PH7_OK;` |
|      - | 7789 | `	}` |
|      - | 7790 | `	/* PHP 8 domain errors (catchable ValueError). */` |
|     23 | 7791 | `	if( iBreaklen < 1 ){` |
|      3 | 7792 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7793 | `			"wordwrap(): Argument #3 ($break) must not be empty");` |
|      - | 7794 | `	}` |
|     21 | 7795 | `	if( iWidth == 0 && iCut ){` |
|      3 | 7796 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7797 | `			"wordwrap(): Argument #4 ($cut_long_words) cannot be true when argument #2 ($width) is 0");` |
|      - | 7798 | `	}` |
|      - | 7799 | `	/*` |
|      - | 7800 | `	 * PHP's algorithm: a single left-to-right pass tracking the start of the` |
|      - | 7801 | `	 * current line (iStart) and the position of the last space seen on it` |
|      - | 7802 | `	 * (iSpace). A break is emitted when the line reaches the width, at the last` |
|      - | 7803 | `	 * space if there was one, otherwise (only when cut is enabled) hard at the` |
|      - | 7804 | `	 * boundary. An existing break sequence in the input resets the line.` |
|      - | 7805 | `	 */` |
|     19 | 7806 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     19 | 7807 | `	iStart = iSpace = iCur = 0;` |
|     19 | 7808 | `	rc = SXRET_OK;` |
|    551 | 7809 | `	while( iCur < iLen ){` |
|    533 | 7810 | `		if( iBreaklen <= iLen - iCur && SyMemcmp(&zIn[iCur],zBreak,(sxu32)iBreaklen) == 0 ){` |
|      - | 7811 | `			/* Existing break sequence in the input: copy it verbatim and reset the line. */` |
|    ! 0 | 7812 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart + iBreaklen));` |
|    ! 0 | 7813 | `			if( rc != SXRET_OK ){ goto oom; }` |
|    ! 0 | 7814 | `			iCur += iBreaklen;` |
|    ! 0 | 7815 | `			iStart = iSpace = iCur;` |
|    ! 0 | 7816 | `			continue;` |
|    533 | 7817 | `		}else if( zIn[iCur] == ' ' ){` |
|     67 | 7818 | `			if( iCur - iStart >= iWidth ){` |
|      - | 7819 | `				/* The line already fills the width at this space: break here (the space is consumed). */` |
|     13 | 7820 | `				rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     13 | 7821 | `				if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     13 | 7822 | `				if( rc != SXRET_OK ){ goto oom; }` |
|     13 | 7823 | `				iStart = iCur + 1;` |
|      6 | 7824 | `			}` |
|     67 | 7825 | `			iSpace = iCur;` |
|    500 | 7826 | `		}else if( iCut && iCur - iStart >= iWidth && iStart >= iSpace ){` |
|      - | 7827 | `			/* A word longer than the width with no space to break at: hard-cut at the boundary. */` |
|      7 | 7828 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|      7 | 7829 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|      7 | 7830 | `			if( rc != SXRET_OK ){ goto oom; }` |
|      7 | 7831 | `			iStart = iSpace = iCur;` |
|    464 | 7832 | `		}else if( iCur - iStart >= iWidth && iStart < iSpace ){` |
|      - | 7833 | `			/* Past the width mid-word: wrap back to the last space (which is consumed). */` |
|     17 | 7834 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iSpace - iStart));` |
|     17 | 7835 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     17 | 7836 | `			if( rc != SXRET_OK ){ goto oom; }` |
|     17 | 7837 | `			iStart = iSpace = iSpace + 1;` |
|      8 | 7838 | `		}` |
|    533 | 7839 | `		iCur++;` |
|      1 | 7840 | `	}` |
|      - | 7841 | `	/* Emit the trailing chunk. */` |
|     19 | 7842 | `	if( iStart < iCur ){` |
|     19 | 7843 | `		rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     19 | 7844 | `		if( rc != SXRET_OK ){ goto oom; }` |
|      9 | 7845 | `	}` |
|     19 | 7846 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|     19 | 7847 | `	SyBlobRelease(&sWorker);` |
|     19 | 7848 | `	return PH7_OK;` |
|    ! 0 | 7849 | `oom:` |
|    ! 0 | 7850 | `	SyBlobRelease(&sWorker);` |
|    ! 0 | 7851 | `	return PH7_ContextMemoryError(pCtx);` |
|     14 | 7852 | `}` |
|      - | 7853 | `/*` |
|      - | 7854 | ` * Check if the given character is a member of the given mask.` |
|      - | 7855 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 7856 | ` * Refer to [strtok()].` |
|      - | 7857 | ` */` |
|     30 | 7858 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 7859 | `{` |
|      - | 7860 | `	int i;` |
|     57 | 7861 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 7862 | `		if( c == zMask[i] ){` |
|     13 | 7863 | `			if( pOfft ){` |
|      5 | 7864 | `				*pOfft = i;` |
|      2 | 7865 | `			}` |
|     13 | 7866 | `			return TRUE;` |
|      - | 7867 | `		}` |
|     14 | 7868 | `	}` |
|     19 | 7869 | `	return FALSE;` |
|     16 | 7870 | `}` |
|      - | 7871 | `/*` |
|      - | 7872 | ` * Extract a single token from the input stream.` |
|      - | 7873 | ` * Refer to [strtok()].` |
|      - | 7874 | ` */` |
|      6 | 7875 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 7876 | `{` |
|      7 | 7877 | `	const char *zIn = *pzIn;` |
|      - | 7878 | `	const char *zPtr;` |
|      - | 7879 | `	/* Ignore leading delimiter */` |
|     11 | 7880 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 7881 | `		zIn++;` |
|      1 | 7882 | `	}` |
|      7 | 7883 | `	if( zIn >= zEnd ){` |
|      - | 7884 | `		/* End of input */` |
|    ! 0 | 7885 | `		return SXERR_EOF;` |
|      - | 7886 | `	}` |
|      7 | 7887 | `	zPtr = zIn;` |
|      - | 7888 | `	/* Extract the token */` |
|     13 | 7889 | `	while( zIn < zEnd ){` |
|     11 | 7890 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 7891 | `			/* UTF-8 stream */` |
|    ! 0 | 7892 | `			zIn++;` |
|    ! 0 | 7893 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 7894 | `		}else{` |
|     11 | 7895 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 7896 | `				break;` |
|      - | 7897 | `			}` |
|      7 | 7898 | `			zIn++;` |
|      - | 7899 | `		}` |
|      1 | 7900 | `	}` |
|      7 | 7901 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 7902 | `	/* Update the cursor */` |
|      7 | 7903 | `	*pzIn = zIn;` |
|      - | 7904 | `	/* Return to the caller */` |
|      7 | 7905 | `	return SXRET_OK;` |
|      4 | 7906 | `}` |
|      - | 7907 | `/* strtok auxiliary private data */` |
|      - | 7908 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 7909 | `struct strtok_aux_data` |
|      - | 7910 | `{` |
|      - | 7911 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 7912 | `	const char *zIn;   /* Current input stream */` |
|      - | 7913 | `	const char *zEnd;  /* End of input */` |
|      - | 7914 | `};` |
|      - | 7915 | `/*` |
|      - | 7916 | ` * string strtok(string $str,string $token)` |
|      - | 7917 | ` * string strtok(string $token)` |
|      - | 7918 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 7919 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 7920 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 7921 | ` *  words by using the space character as the token.` |
|      - | 7922 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 7923 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 7924 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 7925 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 7926 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 7927 | ` *  the argument are found.` |
|      - | 7928 | ` * Parameters` |
|      - | 7929 | ` *  $str` |
|      - | 7930 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 7931 | ` * $token` |
|      - | 7932 | ` *  The delimiter used when splitting up str.` |
|      - | 7933 | ` * Return` |
|      - | 7934 | ` *   Current token or FALSE on EOF.` |
|      - | 7935 | ` */` |
|      6 | 7936 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7937 | `{` |
|      - | 7938 | `	strtok_aux_data *pAux;` |
|      - | 7939 | `	const char *zMask;` |
|      - | 7940 | `	SyString sToken;` |
|      - | 7941 | `	int nMasklen;` |
|      - | 7942 | `	sxi32 rc;` |
|      7 | 7943 | `	if( nArg < 2 ){` |
|      - | 7944 | `		/* Extract top aux data */` |
|      5 | 7945 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      5 | 7946 | `		if( pAux == 0 ){` |
|      - | 7947 | `			/* No aux data,return FALSE */` |
|    ! 0 | 7948 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7949 | `			return PH7_OK;` |
|      - | 7950 | `		}` |
|      5 | 7951 | `		nMasklen = 0;` |
|      5 | 7952 | `		zMask = ""; /* cc warning */` |
|      5 | 7953 | `		if( nArg > 0 ){` |
|      - | 7954 | `			/* Extract the mask */` |
|      5 | 7955 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 7956 | `		}` |
|      5 | 7957 | `		if( nMasklen < 1 ){` |
|      - | 7958 | `			/* Invalid mask,return FALSE */` |
|    ! 0 | 7959 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 7960 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7961 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 7962 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7963 | `			return PH7_OK;` |
|      - | 7964 | `		}` |
|      - | 7965 | `		/* Extract the token */` |
|      5 | 7966 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 7967 | `		if( rc != SXRET_OK ){` |
|      - | 7968 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 7969 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 7970 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7971 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 7972 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7973 | `		}else{` |
|      - | 7974 | `			/* Return the extracted token */` |
|      5 | 7975 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 7976 | `		}` |
|      3 | 7977 | `	}else{` |
|      - | 7978 | `		const char *zInput,*zCur;` |
|      - | 7979 | `		char *zDup;` |
|      - | 7980 | `		int nLen;` |
|      - | 7981 | `		/* Extract the raw input */` |
|      3 | 7982 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 7983 | `		if( nLen < 1 ){` |
|      - | 7984 | `			/* Empty input,return FALSE */` |
|    ! 0 | 7985 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7986 | `			return PH7_OK;` |
|      - | 7987 | `		}` |
|      - | 7988 | `		/* Extract the mask */` |
|      3 | 7989 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 7990 | `		if( nMasklen < 1 ){` |
|      - | 7991 | `			/* Set a default mask */` |
|      - | 7992 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 7993 | `			zMask = TOK_MASK;` |
|    ! 0 | 7994 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 7995 | `#undef TOK_MASK` |
|    ! 0 | 7996 | `		}` |
|      - | 7997 | `		/* Extract a single token */` |
|      3 | 7998 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 7999 | `		if( rc != SXRET_OK ){` |
|      - | 8000 | `			/* Empty input */` |
|    ! 0 | 8001 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 8002 | `			return PH7_OK;` |
|    ! 0 | 8003 | `		}else{` |
|      - | 8004 | `			/* Return the extracted token */` |
|      3 | 8005 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 8006 | `		}` |
|      - | 8007 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 8008 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 8009 | `		if( pAux ){` |
|      3 | 8010 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 8011 | `			if( nLen < 1 ){` |
|    ! 0 | 8012 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 8013 | `				return PH7_OK;` |
|      - | 8014 | `			}` |
|      - | 8015 | `			/* Duplicate input */` |
|      3 | 8016 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 8017 | `			if( zDup  ){` |
|      3 | 8018 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 8019 | `				/* Register the aux data */` |
|      3 | 8020 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 8021 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 8022 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 8023 | `			}` |
|      1 | 8024 | `		}` |
|      - | 8025 | `	}` |
|      7 | 8026 | `	return PH7_OK;` |
|      4 | 8027 | `}` |
|      - | 8028 | `/*` |
|      - | 8029 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 8030 | ` *  Pad a string to a certain length with another string` |
|      - | 8031 | ` * Parameters` |
|      - | 8032 | ` *  $input` |
|      - | 8033 | ` *   The input string.` |
|      - | 8034 | ` * $pad_length` |
|      - | 8035 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 8036 | ` *   string, no padding takes place.` |
|      - | 8037 | ` * $pad_string` |
|      - | 8038 | ` *   Note:` |
|      - | 8039 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 8040 | ` *    divided by the pad_string's length.` |
|      - | 8041 | ` * $pad_type` |
|      - | 8042 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 8043 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 8044 | ` * Return` |
|      - | 8045 | ` *  The padded string.` |
|      - | 8046 | ` */` |
|     10 | 8047 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8048 | `{` |
|      - | 8049 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 8050 | `	const char *zIn,*zPad;` |
|     11 | 8051 | `	if( nArg < 2 ){` |
|      - | 8052 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 8053 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 8054 | `		return PH7_OK;` |
|      - | 8055 | `	}` |
|      - | 8056 | `	/* Extract the target string */` |
|     11 | 8057 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 8058 | `	/* Padding length */` |
|      - | 8059 | `	{` |
|     11 | 8060 | `		sxi64 iTmp = 0;` |
|     11 | 8061 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"str_pad",2,"$length","int",&iTmp);` |
|     11 | 8062 | `		if( rcArg != PH7_OK ){` |
|    ! 0 | 8063 | `			return rcArg;` |
|      - | 8064 | `		}` |
|     11 | 8065 | `		iRealPad = iPadlen = (int)iTmp;` |
|      - | 8066 | `	}` |
|     11 | 8067 | `	if( iPadlen > 0 ){` |
|      9 | 8068 | `		iPadlen -= iLen;` |
|      4 | 8069 | `	}` |
|     11 | 8070 | `	if( iPadlen < 1  ){` |
|      - | 8071 | `		/* Return the string verbatim */` |
|      5 | 8072 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      5 | 8073 | `		return PH7_OK;` |
|      - | 8074 | `	}` |
|      7 | 8075 | `	zPad = " "; /* Whitespace padding */` |
|      7 | 8076 | `	iStrpad = (int)sizeof(char);` |
|      7 | 8077 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      7 | 8078 | `	if( nArg > 2 ){` |
|      - | 8079 | `		/* Padding string */` |
|      7 | 8080 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      7 | 8081 | `		if( iStrpad < 1 ){` |
|      - | 8082 | `			/* An empty pad string throws a catchable ValueError in PHP 8` |
|      - | 8083 | `			 * (only reached once padding is actually required). */` |
|      3 | 8084 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 8085 | `				"str_pad(): Argument #3 ($pad_string) must not be empty");` |
|      - | 8086 | `		}` |
|      5 | 8087 | `		if( nArg > 3 ){` |
|      - | 8088 | `			/* Padd type */` |
|      5 | 8089 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 8090 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 8091 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 8092 | `			}` |
|      2 | 8093 | `		}` |
|      2 | 8094 | `	}` |
|      5 | 8095 | `	iDiv = 1;` |
|      5 | 8096 | `	if( iType == 2 ){` |
|    ! 0 | 8097 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 8098 | `	}` |
|      - | 8099 | `	/* Perform the requested operation */` |
|      5 | 8100 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 8101 | `		jPad = iStrpad;` |
|      5 | 8102 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 8103 | `			/* Padding */` |
|      5 | 8104 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 8105 | `				break;` |
|      - | 8106 | `			}` |
|      3 | 8107 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 8108 | `		}` |
|      3 | 8109 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 8110 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 8111 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 8112 | `				if( jPad > iStrpad ){` |
|    ! 0 | 8113 | `					jPad = iStrpad;` |
|    ! 0 | 8114 | `				}` |
|      3 | 8115 | `				if( jPad < 1){` |
|    ! 0 | 8116 | `					break;` |
|      - | 8117 | `				}` |
|      3 | 8118 | `				if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 8119 | `			}` |
|      1 | 8120 | `		}` |
|      1 | 8121 | `	}` |
|      5 | 8122 | `	if( iLen > 0 ){` |
|      - | 8123 | `		/* Append the input string */` |
|      5 | 8124 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 8125 | `	}` |
|      5 | 8126 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 8127 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 8128 | `			/* Padding */` |
|      5 | 8129 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 8130 | `				break;` |
|      - | 8131 | `			}` |
|      3 | 8132 | `			if( ph7_result_string(pCtx,zPad,iStrpad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 8133 | `		}` |
|      5 | 8134 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 8135 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 8136 | `			if( jPad > iStrpad ){` |
|    ! 0 | 8137 | `				jPad = iStrpad;` |
|    ! 0 | 8138 | `			}` |
|      3 | 8139 | `			if( jPad < 1){` |
|    ! 0 | 8140 | `				break;` |
|      - | 8141 | `			}` |
|      3 | 8142 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 8143 | `		}` |
|      1 | 8144 | `	}` |
|      5 | 8145 | `	return PH7_OK;` |
|      6 | 8146 | `}` |
|      - | 8147 | `/*` |
|      - | 8148 | ` * String replacement private data.` |
|      - | 8149 | ` */` |
|      - | 8150 | `typedef struct str_replace_data str_replace_data;` |
|      - | 8151 | `struct str_replace_data` |
|      - | 8152 | `{` |
|      - | 8153 | `	/* Used by the str_replace family to collect the search/replace arguments. */` |
|      - | 8154 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 8155 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 8156 | `	sxi32 rc;           /* Carries an allocation failure (SXERR_MEM) out of a walker */` |
|      - | 8157 | `};` |
|      - | 8158 | `/*` |
|      - | 8159 | ` * Remove a substring.` |
|      - | 8160 | ` */` |
|      - | 8161 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 8162 | `	for(;;){\` |
|      - | 8163 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 8164 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 8165 | `		++OFFT;\` |
|      - | 8166 | `	}\` |
|      - | 8167 | `}` |
|      - | 8168 | `/*` |
|      - | 8169 | ` * Shift right and insert algorithm.` |
|      - | 8170 | ` */` |
|      - | 8171 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 8172 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 8173 | `		for(;;){\` |
|      - | 8174 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 8175 | `			if(INLEN < 1 ) { break; }\` |
|      - | 8176 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 8177 | `			--INLEN; \` |
|      - | 8178 | `		}\` |
|      - | 8179 | `		for(;;){\` |
|      - | 8180 | `				if(ELEN < 1) { break; }\` |
|      - | 8181 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 8182 | `				OFFT++;\` |
|      - | 8183 | `				ENTRY++;\` |
|      - | 8184 | `				--ELEN;\` |
|      - | 8185 | `		}\` |
|      - | 8186 | `}` |
|      - | 8187 | `/*` |
|      - | 8188 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 8189 | ` * replacement string [i.e: zReplace].` |
|      - | 8190 | ` */` |
|     52 | 8191 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      5 | 8192 | `{` |
|     57 | 8193 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 8194 | `	sxu32 n,m;` |
|     57 | 8195 | `	n = SyBlobLength(pWorker);` |
|     57 | 8196 | `	m = nOfft;` |
|      - | 8197 | `	/* Delete the old entry */` |
|   6591 | 8198 | `	STRDEL(zInput,n,m,nLen);` |
|     57 | 8199 | `	SyBlobLength(pWorker) -= nLen;` |
|     57 | 8200 | `	if( nReplen > 0 ){` |
|     51 | 8201 | `		sxi32 iRep = nReplen;` |
|      - | 8202 | `		sxi32 rc;` |
|      - | 8203 | `		/*` |
|      - | 8204 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 8205 | `		 * string.` |
|      - | 8206 | `		 */` |
|     51 | 8207 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     51 | 8208 | `		if( rc != SXRET_OK ){` |
|      - | 8209 | `			/* Propagate the allocation failure so the caller can raise a fatal` |
|      - | 8210 | `			 * instead of returning a partially-replaced string as success. */` |
|    ! 0 | 8211 | `			return rc;` |
|      - | 8212 | `		}` |
|      - | 8213 | `		/* Perform the insertion now */` |
|     51 | 8214 | `		zInput = (char *)SyBlobData(pWorker);` |
|     51 | 8215 | `		n = SyBlobLength(pWorker);` |
|   6381 | 8216 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     51 | 8217 | `		SyBlobLength(pWorker) += nReplen;` |
|     23 | 8218 | `	}` |
|     57 | 8219 | `	return SXRET_OK;` |
|     31 | 8220 | `}` |
|      - | 8221 | `/*` |
|      - | 8222 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 8223 | ` * to collect search/replace string.` |
|      - | 8224 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 8225 | ` */` |
|    162 | 8226 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      5 | 8227 | `{` |
|    167 | 8228 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 8229 | `	SyString sWorker;` |
|      - | 8230 | `	const char *zIn;` |
|      - | 8231 | `	int nByte;` |
|      - | 8232 | `	/* Extract a string representation of the given argument */` |
|    167 | 8233 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|    167 | 8234 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|    167 | 8235 | `	if( nByte > 0 ){` |
|      - | 8236 | `		char *zDup;` |
|      - | 8237 | `		/* Duplicate the chunk */` |
|    165 | 8238 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 8239 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 8240 | `			);` |
|    165 | 8241 | `		if( zDup == 0 ){` |
|      - | 8242 | `			/* Allocation failure: carry it out and stop the walk so the caller` |
|      - | 8243 | `			 * raises a fatal instead of silently dropping a search/replace term. */` |
|    ! 0 | 8244 | `			pRep->rc = SXERR_MEM;` |
|    ! 0 | 8245 | `			return SXERR_MEM;` |
|      - | 8246 | `		}` |
|    165 | 8247 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 8248 | `		/* Save the chunk */` |
|    165 | 8249 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     80 | 8250 | `	}` |
|      - | 8251 | `	/* Save for later processing */` |
|    167 | 8252 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 8253 | `	/* All done */` |
|     81 | 8254 | `	SXUNUSED(pKey); /* cc warning */` |
|    167 | 8255 | `	return PH7_OK;` |
|     86 | 8256 | `}` |
|      - | 8257 | `/*` |
|      - | 8258 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 8259 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 8260 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 8261 | ` * Parameters` |
|      - | 8262 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 8263 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 8264 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 8265 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 8266 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 8267 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 8268 | ` * $search` |
|      - | 8269 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 8270 | ` *  to designate multiple needles.` |
|      - | 8271 | ` * $replace` |
|      - | 8272 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 8273 | ` *  to designate multiple replacements.` |
|      - | 8274 | ` * $subject` |
|      - | 8275 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 8276 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 8277 | ` *  of subject, and the return value is an array as well.` |
|      - | 8278 | ` * $count (Not used)` |
|      - | 8279 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 8280 | ` * Return` |
|      - | 8281 | ` * This function returns a string or an array with the replaced values.` |
|      - | 8282 | ` */` |
|  30134 | 8283 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 8284 | `{` |
|      - | 8285 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 8286 | `	ProcStringMatch xMatch;` |
|      - | 8287 | `	const char *zIn,*zFunc;` |
|      - | 8288 | `	str_replace_data sRep;` |
|      - | 8289 | `	SyBlob sWorker;` |
|      - | 8290 | `	SySet sReplace;` |
|      - | 8291 | `	SySet sSearch;` |
|      - | 8292 | `	int rep_str;` |
|      - | 8293 | `	int nByte;` |
|      - | 8294 | `	sxi32 rc;` |
|  30139 | 8295 | `	if( nArg < 3 ){` |
|      - | 8296 | `		/* Missing/Invalid arguments,return null */` |
|    ! 0 | 8297 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8298 | `		return PH7_OK;` |
|      - | 8299 | `	}` |
|      - | 8300 | `	/* Initialize fields */` |
|  30139 | 8301 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  30139 | 8302 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  30139 | 8303 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  30139 | 8304 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  30139 | 8305 | `	sRep.pCtx = pCtx;` |
|  30139 | 8306 | `	sRep.pCollector = &sSearch;` |
|  30139 | 8307 | `	rep_str = 0;` |
|      - | 8308 | `	/* Extract the subject */` |
|  30139 | 8309 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  30139 | 8310 | `	if( nByte < 1 ){` |
|      - | 8311 | `		/* Nothing to replace,return the empty string */` |
|     21 | 8312 | `		ph7_result_string(pCtx,"",0);` |
|     21 | 8313 | `		return PH7_OK;` |
|      - | 8314 | `	}` |
|      - | 8315 | `	/* Copy the subject */` |
|  30119 | 8316 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 8317 | `	/* Search string */` |
|  30119 | 8318 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 8319 | `		/* Collect search string */` |
|     81 | 8320 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|     43 | 8321 | `	}else{` |
|      - | 8322 | `		/* Single pattern */` |
|  30043 | 8323 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  30043 | 8324 | `		if( nByte < 1 ){` |
|      - | 8325 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 8326 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 8327 | `			return PH7_OK;` |
|      - | 8328 | `		}` |
|  30039 | 8329 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 8330 | `		/* Save for later processing */` |
|  30039 | 8331 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 8332 | `	}` |
|      - | 8333 | `	/* Replace string */` |
|  30115 | 8334 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 8335 | `		/* Collect replace string */` |
|      7 | 8336 | `		sRep.pCollector = &sReplace;` |
|      7 | 8337 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 8338 | `	}else{` |
|      - | 8339 | `		/* Single needle */` |
|  30109 | 8340 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  30109 | 8341 | `		rep_str = 1;` |
|  30109 | 8342 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 8343 | `		/* Save for later processing */` |
|  30109 | 8344 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 8345 | `	}` |
|      - | 8346 | `	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */` |
|  30115 | 8347 | `	if( sRep.rc != SXRET_OK ){` |
|    ! 0 | 8348 | `		SySetRelease(&sSearch);` |
|    ! 0 | 8349 | `		SySetRelease(&sReplace);` |
|    ! 0 | 8350 | `		SyBlobRelease(&sWorker);` |
|    ! 0 | 8351 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8352 | `	}` |
|      - | 8353 | `	/* Reset loop cursors */` |
|  30115 | 8354 | `	SySetResetCursor(&sSearch);` |
|  30115 | 8355 | `	SySetResetCursor(&sReplace);` |
|  30115 | 8356 | `	pReplace = pSearch = 0; /* cc warning */` |
|  30115 | 8357 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 8358 | `	/* Extract function name */` |
|  30115 | 8359 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 8360 | `	/* Set the default pattern match routine */` |
|  30115 | 8361 | `	xMatch = SyBlobSearch;` |
|  30115 | 8362 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 8363 | `		/* Case insensitive pattern match */` |
|     11 | 8364 | `		xMatch = iPatternMatch;` |
|      5 | 8365 | `	}` |
|      - | 8366 | `	/* Start the replace process */` |
|  60301 | 8367 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 8368 | `		sxu32 nCount,nOfft;` |
|  30191 | 8369 | `		if( pSearch->nByte <  1 ){` |
|      - | 8370 | `			/* Empty string,ignore */` |
|      3 | 8371 | `			continue;` |
|      - | 8372 | `		}` |
|      - | 8373 | `		/* Extract the replace string */` |
|  30189 | 8374 | `		if( rep_str ){` |
|  30179 | 8375 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|  15092 | 8376 | `		}else{` |
|     11 | 8377 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 8378 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 8379 | `				 * An empty string is used for the rest of replacement values` |
|      - | 8380 | `				 */` |
|      3 | 8381 | `				pReplace = 0;` |
|      1 | 8382 | `			}` |
|      - | 8383 | `		}` |
|  30189 | 8384 | `		if( pReplace == 0 ){` |
|      - | 8385 | `			/* Use an empty string instead */` |
|      3 | 8386 | `			pReplace = &sTemp;` |
|      1 | 8387 | `		}` |
|  30189 | 8388 | `		nOfft = nCount = 0;` |
|  15118 | 8389 | `		for(;;){` |
|  30241 | 8390 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 8391 | `				break;` |
|      - | 8392 | `			}` |
|      - | 8393 | `			/* Perform a pattern lookup */` |
|  45341 | 8394 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  30224 | 8395 | `				pSearch->nByte,&nOfft);` |
|  30229 | 8396 | `			if( rc != SXRET_OK ){` |
|      - | 8397 | `				/* Pattern not found */` |
|  30177 | 8398 | `				break;` |
|      - | 8399 | `			}` |
|      - | 8400 | `			/* Perform the replace operation */` |
|     57 | 8401 | `			rc = StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|     57 | 8402 | `			if( rc != SXRET_OK ){` |
|      - | 8403 | `				/* Allocation failure: surface a fatal instead of a partial result */` |
|    ! 0 | 8404 | `				SySetRelease(&sSearch);` |
|    ! 0 | 8405 | `				SySetRelease(&sReplace);` |
|    ! 0 | 8406 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8407 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8408 | `			}` |
|      - | 8409 | `			/* Increment offset counter */` |
|     57 | 8410 | `			nCount += nOfft + pReplace->nByte;` |
|      5 | 8411 | `		}` |
|      5 | 8412 | `	}` |
|      - | 8413 | `	/* All done,clean-up the mess left behind */` |
|  30115 | 8414 | `	rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  30115 | 8415 | `	SySetRelease(&sSearch);` |
|  30115 | 8416 | `	SySetRelease(&sReplace);` |
|  30115 | 8417 | `	SyBlobRelease(&sWorker);` |
|  30115 | 8418 | `	if( rc != PH7_OK ){` |
|    ! 0 | 8419 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8420 | `	}` |
|  30115 | 8421 | `	return PH7_OK;` |
|  15072 | 8422 | `}` |
|      - | 8423 | `/*` |
|      - | 8424 | ` * strtr() array form: a single (key,value) pair copied out of the replace_pairs` |
|      - | 8425 | ` * array. The bytes are owned by a persistent pool (see strtr_collect) rather than` |
|      - | 8426 | ` * the transient walker values, which HashmapWalk releases after each callback, so` |
|      - | 8427 | ` * we store byte offsets into that pool instead of raw pointers.` |
|      - | 8428 | ` */` |
|      - | 8429 | `typedef struct strtr_entry strtr_entry;` |
|      - | 8430 | `struct strtr_entry` |
|      - | 8431 | `{` |
|      - | 8432 | `	sxu32 nKeyOfft; /* Offset of the search key inside the pool */` |
|      - | 8433 | `	sxu32 nKeyLen;  /* Length of the search key */` |
|      - | 8434 | `	sxu32 nValOfft; /* Offset of the replacement inside the pool */` |
|      - | 8435 | `	sxu32 nValLen;  /* Length of the replacement */` |
|      - | 8436 | `};` |
|      - | 8437 | `typedef struct strtr_collect strtr_collect;` |
|      - | 8438 | `struct strtr_collect` |
|      - | 8439 | `{` |
|      - | 8440 | `	SyBlob *pPool;  /* Byte pool holding copied key + value bytes */` |
|      - | 8441 | `	SySet  *pTable; /* Set of strtr_entry (parallel offsets into pPool) */` |
|      - | 8442 | `	sxi32   rc;     /* Carries an allocation failure (SXERR_MEM) out of the walker */` |
|      - | 8443 | `};` |
|      - | 8444 | `/*` |
|      - | 8445 | ` * Collect one replace_pairs entry into the persistent pool/offset table.` |
|      - | 8446 | ` * PHP coerces both the key and the value to string (an integer key becomes its` |
|      - | 8447 | ` * decimal form) and ignores an empty-string key.` |
|      - | 8448 | ` */` |
|     20 | 8449 | `static int StrtrCollectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 8450 | `{` |
|     21 | 8451 | `	strtr_collect *pCol = (strtr_collect *)pUserData;` |
|      - | 8452 | `	const char *zKey,*zVal;` |
|      - | 8453 | `	strtr_entry sEnt;` |
|      - | 8454 | `	int nKey,nVal;` |
|     21 | 8455 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|     21 | 8456 | `	if( nKey < 1 ){` |
|      - | 8457 | `		/* PHP ignores an empty-string key (it also emits a warning we do not replicate). */` |
|      3 | 8458 | `		return PH7_OK;` |
|      - | 8459 | `	}` |
|     19 | 8460 | `	zVal = ph7_value_to_string(pData,&nVal);` |
|     19 | 8461 | `	sEnt.nKeyOfft = SyBlobLength(pCol->pPool);` |
|     19 | 8462 | `	sEnt.nKeyLen  = (sxu32)nKey;` |
|     19 | 8463 | `	if( SyBlobAppend(pCol->pPool,(const void *)zKey,(sxu32)nKey) != SXRET_OK ){` |
|    ! 0 | 8464 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8465 | `		return SXERR_ABORT;` |
|      - | 8466 | `	}` |
|     19 | 8467 | `	sEnt.nValOfft = SyBlobLength(pCol->pPool);` |
|     19 | 8468 | `	sEnt.nValLen  = (sxu32)nVal;` |
|     19 | 8469 | `	if( nVal > 0 && SyBlobAppend(pCol->pPool,(const void *)zVal,(sxu32)nVal) != SXRET_OK ){` |
|    ! 0 | 8470 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8471 | `		return SXERR_ABORT;` |
|      - | 8472 | `	}` |
|     19 | 8473 | `	if( SySetPut(pCol->pTable,(const void *)&sEnt) != SXRET_OK ){` |
|    ! 0 | 8474 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8475 | `		return SXERR_ABORT;` |
|      - | 8476 | `	}` |
|     19 | 8477 | `	return PH7_OK;` |
|     11 | 8478 | `}` |
|      - | 8479 | `/*` |
|      - | 8480 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 8481 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 8482 | ` *  Translate characters or replace substrings.` |
|      - | 8483 | ` * Parameters` |
|      - | 8484 | ` *  $str` |
|      - | 8485 | ` *  The string being translated.` |
|      - | 8486 | ` * $from` |
|      - | 8487 | ` *  The string being translated to to.` |
|      - | 8488 | ` * $to` |
|      - | 8489 | ` *  The string replacing from.` |
|      - | 8490 | ` * $replace_pairs` |
|      - | 8491 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 8492 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 8493 | ` * Return` |
|      - | 8494 | ` *  The translated string.` |
|      - | 8495 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 8496 | ` */` |
|     12 | 8497 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8498 | `{` |
|      - | 8499 | `	const char *zIn;` |
|      - | 8500 | `	int nLen;` |
|     13 | 8501 | `	if( nArg < 1 ){` |
|      - | 8502 | `		/* Nothing to replace,return FALSE */` |
|    ! 0 | 8503 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8504 | `		return PH7_OK;` |
|      - | 8505 | `	}` |
|     13 | 8506 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 8507 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 8508 | `		/* Invalid arguments */` |
|    ! 0 | 8509 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8510 | `		return PH7_OK;` |
|      - | 8511 | `	}` |
|     18 | 8512 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 8513 | `		strtr_collect sCol;` |
|      - | 8514 | `		SyBlob sPool,sWorker;` |
|      - | 8515 | `		SySet sTable;` |
|      - | 8516 | `		const char *zPool;` |
|      - | 8517 | `		strtr_entry *pEnt;` |
|      - | 8518 | `		sxi32 rc;` |
|      - | 8519 | `		int i,iRun;` |
|      - | 8520 | `		/*` |
|      - | 8521 | `		 * PHP's array-form strtr is a single left-to-right pass over the subject:` |
|      - | 8522 | `		 * at every position it substitutes the LONGEST replace_pairs key that` |
|      - | 8523 | `		 * matches there, then advances past the key (replacements are never` |
|      - | 8524 | `		 * rescanned). It is not a sequential per-key global replace. First copy` |
|      - | 8525 | `		 * the pairs into a persistent pool, then run that scan.` |
|      - | 8526 | `		 */` |
|     11 | 8527 | `		SyBlobInit(&sPool,&pCtx->pVm->sAllocator);` |
|     11 | 8528 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     11 | 8529 | `		SySetInit(&sTable,&pCtx->pVm->sAllocator,sizeof(strtr_entry));` |
|     11 | 8530 | `		sCol.pPool  = &sPool;` |
|     11 | 8531 | `		sCol.pTable = &sTable;` |
|     11 | 8532 | `		sCol.rc     = SXRET_OK;` |
|     11 | 8533 | `		ph7_array_walk(apArg[1],StrtrCollectWalker,&sCol);` |
|     11 | 8534 | `		if( sCol.rc != SXRET_OK ){` |
|      - | 8535 | `			/* Allocation failure while collecting the pairs: surface a fatal */` |
|    ! 0 | 8536 | `			SyBlobRelease(&sPool);` |
|    ! 0 | 8537 | `			SyBlobRelease(&sWorker);` |
|    ! 0 | 8538 | `			SySetRelease(&sTable);` |
|    ! 0 | 8539 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 8540 | `		}` |
|      - | 8541 | `		/* The pool is now stable, so offsets can be resolved against its base. */` |
|     11 | 8542 | `		zPool = (const char *)SyBlobData(&sPool);` |
|     11 | 8543 | `		rc = SXRET_OK;` |
|     11 | 8544 | `		iRun = 0; /* Start of the pending run of unmatched bytes copied verbatim. */` |
|     43 | 8545 | `		for( i = 0 ; i < nLen ; ){` |
|     33 | 8546 | `			strtr_entry *pBest = 0;` |
|     33 | 8547 | `			sxu32 nBest = 0;` |
|      - | 8548 | `			/* Pick the longest key that matches at the current position. */` |
|     33 | 8549 | `			SySetResetCursor(&sTable);` |
|     87 | 8550 | `			while( SXRET_OK == SySetGetNextEntry(&sTable,(void **)&pEnt) ){` |
|     54 | 8551 | `				if( pEnt->nKeyLen > nBest` |
|     50 | 8552 | `					&& pEnt->nKeyLen <= (sxu32)(nLen - i)` |
|     46 | 8553 | `					&& SyMemcmp(zPool + pEnt->nKeyOfft,zIn + i,pEnt->nKeyLen) == 0 ){` |
|     29 | 8554 | `					nBest = pEnt->nKeyLen;` |
|     29 | 8555 | `					pBest = pEnt;` |
|     14 | 8556 | `				}` |
|      1 | 8557 | `			}` |
|     33 | 8558 | `			if( pBest == 0 ){` |
|      - | 8559 | `				/* No key here: extend the literal run and copy it in one shot later. */` |
|      9 | 8560 | `				i++;` |
|      9 | 8561 | `				continue;` |
|      - | 8562 | `			}` |
|      - | 8563 | `			/* Flush the pending literal run, then the replacement. */` |
|     25 | 8564 | `			if( i > iRun ){` |
|      5 | 8565 | `				rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(i - iRun));` |
|      2 | 8566 | `			}` |
|     25 | 8567 | `			if( rc == SXRET_OK && pBest->nValLen > 0 ){` |
|     25 | 8568 | `				rc = SyBlobAppend(&sWorker,zPool + pBest->nValOfft,pBest->nValLen);` |
|     12 | 8569 | `			}` |
|     25 | 8570 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 8571 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 8572 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8573 | `				SySetRelease(&sTable);` |
|    ! 0 | 8574 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8575 | `			}` |
|     25 | 8576 | `			i += (int)pBest->nKeyLen;` |
|     25 | 8577 | `			iRun = i;` |
|      1 | 8578 | `		}` |
|      - | 8579 | `		/* Flush the trailing literal run. */` |
|     11 | 8580 | `		if( nLen > iRun ){` |
|      3 | 8581 | `			rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(nLen - iRun));` |
|      3 | 8582 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 8583 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 8584 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8585 | `				SySetRelease(&sTable);` |
|    ! 0 | 8586 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8587 | `			}` |
|      1 | 8588 | `		}` |
|      - | 8589 | `		/* All done, return the result string */` |
|     16 | 8590 | `		rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|     10 | 8591 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 8592 | `		/* Clean-up */` |
|     11 | 8593 | `		SyBlobRelease(&sPool);` |
|     11 | 8594 | `		SyBlobRelease(&sWorker);` |
|     11 | 8595 | `		SySetRelease(&sTable);` |
|     11 | 8596 | `		if( rc != PH7_OK ){` |
|    ! 0 | 8597 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 8598 | `		}` |
|      6 | 8599 | `	}else{` |
|      - | 8600 | `		int i,flen,tlen,c,iOfft;` |
|      - | 8601 | `		const char *zFrom,*zTo;` |
|      3 | 8602 | `		if( nArg < 3 ){` |
|      - | 8603 | `			/* Nothing to replace */` |
|    ! 0 | 8604 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8605 | `			return PH7_OK;` |
|      - | 8606 | `		}` |
|      - | 8607 | `		/* Extract given arguments */` |
|      3 | 8608 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 8609 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 8610 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 8611 | `			/* Nothing to replace */` |
|    ! 0 | 8612 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8613 | `			return PH7_OK;` |
|      - | 8614 | `		}` |
|      - | 8615 | `		/* Start the replace process */` |
|     13 | 8616 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 8617 | `			c = zIn[i];` |
|     11 | 8618 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 8619 | `				if ( iOfft < tlen ){` |
|      5 | 8620 | `					c = zTo[iOfft];` |
|      2 | 8621 | `				}` |
|      2 | 8622 | `			}` |
|     11 | 8623 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 8624 |  |
|      6 | 8625 | `		}` |
|      - | 8626 | `	}` |
|     13 | 8627 | `	return PH7_OK;` |
|      7 | 8628 | `}` |
|      - | 8629 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8630 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8631 | `/*` |
|      - | 8632 | ` * Parse an INI string.` |
|      - | 8633 |  |
|      - | 8634 | ` * According to wikipedia` |
|      - | 8635 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 8636 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 8637 | ` *  Format` |
|      - | 8638 | `*    Properties` |
|      - | 8639 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 8640 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 8641 | `*     Example:` |
|      - | 8642 | `*      name=value` |
|      - | 8643 | `*    Sections` |
|      - | 8644 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 8645 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 8646 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 8647 | `*     or the end of the file. Sections may not be nested.` |
|      - | 8648 | `*     Example:` |
|      - | 8649 | `*      [section]` |
|      - | 8650 | `*   Comments` |
|      - | 8651 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 8652 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 8653 | `*/` |
|     12 | 8654 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 8655 | `{` |
|      - | 8656 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 8657 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 8658 | `	SyHashEntry *pEntry;` |
|      - | 8659 | `	SyString sEntry;` |
|      - | 8660 | `	SyHash sHash;` |
|      - | 8661 | `	int c;` |
|      - | 8662 | `	/* Create an empty array and worker variables */` |
|     13 | 8663 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 8664 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 8665 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 8666 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 8667 | `		/* Out of memory: surface a fatal instead of returning FALSE */` |
|    ! 0 | 8668 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8669 | `	}` |
|     13 | 8670 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 8671 | `	pCur = pArray;` |
|      - | 8672 | `	/* Start the parse process */` |
|     21 | 8673 | `	for(;;){` |
|      - | 8674 | `		/* Ignore leading white spaces */` |
|     69 | 8675 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 8676 | `			zIn++;` |
|      1 | 8677 | `		}` |
|     43 | 8678 | `		if( zIn >= zEnd ){` |
|      - | 8679 | `			/* No more input to process */` |
|     13 | 8680 | `			break;` |
|      - | 8681 | `		}` |
|     31 | 8682 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 8683 | `			/* Comment til the end of line */` |
|    ! 0 | 8684 | `			zIn++;` |
|    ! 0 | 8685 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 8686 | `				zIn++;` |
|    ! 0 | 8687 | `			}` |
|    ! 0 | 8688 | `			continue;` |
|      - | 8689 | `		}` |
|      - | 8690 | `		/* Reset the string cursor of the working variable */` |
|     31 | 8691 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 8692 | `		if( zIn[0] == '[' ){` |
|      - | 8693 | `			/* Section: Extract the section name */` |
|      9 | 8694 | `			zIn++;` |
|      9 | 8695 | `			zCur = zIn;` |
|     73 | 8696 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 8697 | `				zIn++;` |
|      1 | 8698 | `			}` |
|      9 | 8699 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 8700 | `				/* Save the section name */` |
|      5 | 8701 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 8702 | `				SyStringFullTrim(&sEntry);` |
|      5 | 8703 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 8704 | `				if( sEntry.nByte > 0 ){` |
|      - | 8705 | `					/* Associate an array with the section */` |
|      5 | 8706 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 8707 | `					if( pSection ){` |
|      5 | 8708 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 8709 | `						pCur = pSection;` |
|      2 | 8710 | `					}` |
|      2 | 8711 | `				}` |
|      2 | 8712 | `			}` |
|      9 | 8713 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 8714 | `		}else{` |
|      - | 8715 | `			ph7_value *pOldCur;` |
|      - | 8716 | `			int is_array;` |
|      - | 8717 | `			int iLen;` |
|      - | 8718 | `			/* Properties */` |
|     23 | 8719 | `			is_array = 0;` |
|     23 | 8720 | `			zCur = zIn;` |
|     23 | 8721 | `			iLen = 0; /* cc warning */` |
|     23 | 8722 | `			pOldCur = pCur;` |
|    155 | 8723 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 8724 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 8725 | `					/* Array */` |
|    ! 0 | 8726 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 8727 | `					is_array = 1;` |
|    ! 0 | 8728 | `					if( iLen > 0 ){` |
|    ! 0 | 8729 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 8730 | `						/* Query the hashtable */` |
|    ! 0 | 8731 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 8732 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 8733 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 8734 | `						if( pEntry ){` |
|    ! 0 | 8735 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 8736 | `						}else{` |
|      - | 8737 | `							/* Create an empty array */` |
|    ! 0 | 8738 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 8739 | `							if( pvArr ){` |
|      - | 8740 | `								/* Save the entry */` |
|    ! 0 | 8741 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 8742 | `								/* Insert the entry */` |
|    ! 0 | 8743 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 8744 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 8745 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 8746 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 8747 | `							}` |
|      - | 8748 | `						}` |
|    ! 0 | 8749 | `						if( pvArr ){` |
|    ! 0 | 8750 | `							pCur = pvArr;` |
|    ! 0 | 8751 | `						}` |
|    ! 0 | 8752 | `					}` |
|    ! 0 | 8753 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 8754 | `						zIn++;` |
|    ! 0 | 8755 | `					}` |
|    ! 0 | 8756 | `				}` |
|    133 | 8757 | `				zIn++;` |
|      1 | 8758 | `			}` |
|     23 | 8759 | `			if( !is_array ){` |
|     23 | 8760 | `				iLen = (int)(zIn-zCur);` |
|     11 | 8761 | `			}` |
|      - | 8762 | `			/* Trim the key */` |
|     23 | 8763 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 8764 | `			SyStringFullTrim(&sEntry);` |
|     23 | 8765 | `			if( sEntry.nByte > 0 ){` |
|     23 | 8766 | `				if( !is_array ){` |
|      - | 8767 | `					/* Save the key name */` |
|     23 | 8768 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 8769 | `				}` |
|      - | 8770 | `				/* extract key value */` |
|     23 | 8771 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 8772 | `				zIn++; /* '=' */` |
|     39 | 8773 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 8774 | `					zIn++;` |
|      1 | 8775 | `				}` |
|     23 | 8776 | `				if( zIn < zEnd ){` |
|     21 | 8777 | `					zCur = zIn;` |
|     21 | 8778 | `					c = zIn[0];` |
|     21 | 8779 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 8780 | `						zIn++;` |
|      - | 8781 | `						/* Delimit the value */` |
|    ! 0 | 8782 | `						while( zIn < zEnd ){` |
|    ! 0 | 8783 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 8784 | `								break;` |
|      - | 8785 | `							}` |
|    ! 0 | 8786 | `							zIn++;` |
|    ! 0 | 8787 | `						}` |
|    ! 0 | 8788 | `						if( zIn < zEnd ){` |
|    ! 0 | 8789 | `							zIn++;` |
|    ! 0 | 8790 | `						}` |
|    ! 0 | 8791 | `					}else{` |
|    125 | 8792 | `						while( zIn < zEnd ){` |
|    123 | 8793 | `							if( zIn[0] == '\n' ){` |
|     19 | 8794 | `								if( zIn[-1] != '\\' ){` |
|     19 | 8795 | `									break;` |
|    ! 0 | 8796 | `								}` |
|    105 | 8797 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 8798 | `								/* Inline comments */` |
|    ! 0 | 8799 | `								break;` |
|      - | 8800 | `							}` |
|    105 | 8801 | `							zIn++;` |
|      1 | 8802 | `						}` |
|      - | 8803 | `					}` |
|      - | 8804 | `					/* Trim the value */` |
|     21 | 8805 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 8806 | `					SyStringFullTrim(&sEntry);` |
|     21 | 8807 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 8808 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 8809 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 8810 | `					}` |
|     21 | 8811 | `					if( sEntry.nByte > 0 ){` |
|     21 | 8812 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 8813 | `					}` |
|      - | 8814 | `					/* Insert the key and it's value */` |
|     21 | 8815 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 8816 | `				}` |
|     12 | 8817 | `			}else{` |
|    ! 0 | 8818 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 8819 | `					zIn++;` |
|    ! 0 | 8820 | `				}` |
|      - | 8821 | `			}` |
|     23 | 8822 | `			pCur = pOldCur;` |
|      - | 8823 | `		}` |
|      1 | 8824 | `	}` |
|     13 | 8825 | `	SyHashRelease(&sHash);` |
|      - | 8826 | `	/* Return the parse of the INI string */` |
|     13 | 8827 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 8828 | `	return SXRET_OK;` |
|      7 | 8829 | `}` |
|      - | 8830 | `/*` |
|      - | 8831 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 8832 | ` *  Parse a configuration string.` |
|      - | 8833 | ` * Parameters` |
|      - | 8834 | ` *  $ini` |
|      - | 8835 | ` *   The contents of the ini file being parsed.` |
|      - | 8836 | ` *  $process_sections` |
|      - | 8837 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 8838 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 8839 | ` *  $scanner_mode (Not used)` |
|      - | 8840 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 8841 | ` *   then option values will not be parsed.` |
|      - | 8842 | ` * Return` |
|      - | 8843 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 8844 | ` */` |
|     10 | 8845 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8846 | `{` |
|      - | 8847 | `	const char *zIni;` |
|      - | 8848 | `	int nByte;` |
|     11 | 8849 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8850 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 8851 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8852 | `		return PH7_OK;` |
|      - | 8853 | `	}` |
|      - | 8854 | `	/* Extract the raw INI buffer */` |
|     11 | 8855 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 8856 | `	/* Process the INI buffer; propagate an OOM abort so the fatal actually halts */` |
|     11 | 8857 | `	return PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|      6 | 8858 | `}` |
|      - | 8859 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8860 |  |
|      - | 8861 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8862 |  |
|      - | 8863 | `/*` |
|      - | 8864 | ` * Ctype Functions.` |
|      - | 8865 | ` * Status:` |
|      - | 8866 | ` *    Stable.` |
|      - | 8867 | ` */` |
|      - | 8868 | `/*` |
|      - | 8869 | ` * bool ctype_alnum(string $text)` |
|      - | 8870 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 8871 | ` * Parameters` |
|      - | 8872 | ` *  $text` |
|      - | 8873 | ` *   The tested string.` |
|      - | 8874 | ` * Return` |
|      - | 8875 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 8876 | ` */` |
|     72 | 8877 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8878 | `{` |
|      - | 8879 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8880 | `	int nLen;` |
|     73 | 8881 | `	if( nArg < 1 ){` |
|      - | 8882 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8883 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8884 | `		return PH7_OK;` |
|      - | 8885 | `	}` |
|      - | 8886 | `	/* Extract the target string */` |
|     73 | 8887 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     73 | 8888 | `	zEnd = &zIn[nLen];` |
|     73 | 8889 | `	if( nLen < 1 ){` |
|      - | 8890 | `		/* Empty string,return FALSE */` |
|      3 | 8891 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8892 | `		return PH7_OK;` |
|      - | 8893 | `	}` |
|      - | 8894 | `	/* Perform the requested operation */` |
|    110 | 8895 | `	for(;;){` |
|    221 | 8896 | `		if( zIn >= zEnd ){` |
|      - | 8897 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     65 | 8898 | `			ph7_result_bool(pCtx,1);` |
|     65 | 8899 | `			return PH7_OK;` |
|      - | 8900 | `		}` |
|    157 | 8901 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      7 | 8902 | `			break;` |
|      - | 8903 | `		}` |
|      - | 8904 | `		/* Point to the next character */` |
|    151 | 8905 | `		zIn++;` |
|      1 | 8906 | `	}` |
|      - | 8907 | `	/* The test failed,return FALSE */` |
|      7 | 8908 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8909 | `	return PH7_OK;` |
|     37 | 8910 | `}` |
|      - | 8911 | `/*` |
|      - | 8912 | ` * bool ctype_alpha(string $text)` |
|      - | 8913 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 8914 | ` * Parameters` |
|      - | 8915 | ` *  $text` |
|      - | 8916 | ` *   The tested string.` |
|      - | 8917 | ` * Return` |
|      - | 8918 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 8919 | ` */` |
|     16 | 8920 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8921 | `{` |
|      - | 8922 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8923 | `	int nLen;` |
|     17 | 8924 | `	if( nArg < 1 ){` |
|      - | 8925 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8926 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8927 | `		return PH7_OK;` |
|      - | 8928 | `	}` |
|      - | 8929 | `	/* Extract the target string */` |
|     17 | 8930 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8931 | `	zEnd = &zIn[nLen];` |
|     17 | 8932 | `	if( nLen < 1 ){` |
|      - | 8933 | `		/* Empty string,return FALSE */` |
|      3 | 8934 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8935 | `		return PH7_OK;` |
|      - | 8936 | `	}` |
|      - | 8937 | `	/* Perform the requested operation */` |
|     42 | 8938 | `	for(;;){` |
|     85 | 8939 | `		if( zIn >= zEnd ){` |
|      - | 8940 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8941 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8942 | `			return PH7_OK;` |
|      - | 8943 | `		}` |
|     77 | 8944 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 8945 | `			break;` |
|      - | 8946 | `		}` |
|      - | 8947 | `		/* Point to the next character */` |
|     71 | 8948 | `		zIn++;` |
|      1 | 8949 | `	}` |
|      - | 8950 | `	/* The test failed,return FALSE */` |
|      7 | 8951 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8952 | `	return PH7_OK;` |
|      9 | 8953 | `}` |
|      - | 8954 | `/*` |
|      - | 8955 | ` * bool ctype_cntrl(string $text)` |
|      - | 8956 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 8957 | ` * Parameters` |
|      - | 8958 | ` *  $text` |
|      - | 8959 | ` *   The tested string.` |
|      - | 8960 | ` * Return` |
|      - | 8961 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 8962 | ` */` |
|     16 | 8963 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8964 | `{` |
|      - | 8965 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8966 | `	int nLen;` |
|     17 | 8967 | `	if( nArg < 1 ){` |
|      - | 8968 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8969 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8970 | `		return PH7_OK;` |
|      - | 8971 | `	}` |
|      - | 8972 | `	/* Extract the target string */` |
|     17 | 8973 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8974 | `	zEnd = &zIn[nLen];` |
|     17 | 8975 | `	if( nLen < 1 ){` |
|      - | 8976 | `		/* Empty string,return FALSE */` |
|      3 | 8977 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8978 | `		return PH7_OK;` |
|      - | 8979 | `	}` |
|      - | 8980 | `	/* Perform the requested operation */` |
|     14 | 8981 | `	for(;;){` |
|     29 | 8982 | `		if( zIn >= zEnd ){` |
|      - | 8983 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8984 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8985 | `			return PH7_OK;` |
|      - | 8986 | `		}` |
|     21 | 8987 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8988 | `			/* UTF-8 stream  */` |
|    ! 0 | 8989 | `			break;` |
|      - | 8990 | `		}` |
|     21 | 8991 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 8992 | `			break;` |
|      - | 8993 | `		}` |
|      - | 8994 | `		/* Point to the next character */` |
|     15 | 8995 | `		zIn++;` |
|      1 | 8996 | `	}` |
|      - | 8997 | `	/* The test failed,return FALSE */` |
|      7 | 8998 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8999 | `	return PH7_OK;` |
|      9 | 9000 | `}` |
|      - | 9001 | `/*` |
|      - | 9002 | ` * bool ctype_digit(string $text)` |
|      - | 9003 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 9004 | ` * Parameters` |
|      - | 9005 | ` *  $text` |
|      - | 9006 | ` *   The tested string.` |
|      - | 9007 | ` * Return` |
|      - | 9008 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 9009 | ` */` |
|   2632 | 9010 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 9011 | `{` |
|      - | 9012 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9013 | `	int nLen;` |
|   2637 | 9014 | `	if( nArg < 1 ){` |
|      - | 9015 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9016 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9017 | `		return PH7_OK;` |
|      - | 9018 | `	}` |
|      - | 9019 | `	/* Extract the target string */` |
|   2637 | 9020 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   2637 | 9021 | `	zEnd = &zIn[nLen];` |
|   2637 | 9022 | `	if( nLen < 1 ){` |
|      - | 9023 | `		/* Empty string,return FALSE */` |
|      9 | 9024 | `		ph7_result_bool(pCtx,0);` |
|      9 | 9025 | `		return PH7_OK;` |
|      - | 9026 | `	}` |
|      - | 9027 | `	/* Perform the requested operation */` |
|   2421 | 9028 | `	for(;;){` |
|   4847 | 9029 | `		if( zIn >= zEnd ){` |
|      - | 9030 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   2155 | 9031 | `			ph7_result_bool(pCtx,1);` |
|   2155 | 9032 | `			return PH7_OK;` |
|      - | 9033 | `		}` |
|   2697 | 9034 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9035 | `			/* UTF-8 stream  */` |
|    ! 0 | 9036 | `			break;` |
|      - | 9037 | `		}` |
|   2697 | 9038 | `		if( !SyisDigit(zIn[0]) ){` |
|    479 | 9039 | `			break;` |
|      - | 9040 | `		}` |
|      - | 9041 | `		/* Point to the next character */` |
|   2223 | 9042 | `		zIn++;` |
|      5 | 9043 | `	}` |
|      - | 9044 | `	/* The test failed,return FALSE */` |
|    479 | 9045 | `	ph7_result_bool(pCtx,0);` |
|    479 | 9046 | `	return PH7_OK;` |
|   1321 | 9047 | `}` |
|      - | 9048 | `/*` |
|      - | 9049 | ` * bool ctype_xdigit(string $text)` |
|      - | 9050 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 9051 | ` * Parameters` |
|      - | 9052 | ` *  $text` |
|      - | 9053 | ` *   The tested string.` |
|      - | 9054 | ` * Return` |
|      - | 9055 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 9056 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 9057 | ` */` |
|     38 | 9058 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 9059 | `{` |
|      - | 9060 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9061 | `	int nLen;` |
|     40 | 9062 | `	if( nArg < 1 ){` |
|      - | 9063 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9064 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9065 | `		return PH7_OK;` |
|      - | 9066 | `	}` |
|      - | 9067 | `	/* Extract the target string */` |
|     40 | 9068 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     40 | 9069 | `	zEnd = &zIn[nLen];` |
|     40 | 9070 | `	if( nLen < 1 ){` |
|      - | 9071 | `		/* Empty string,return FALSE */` |
|      3 | 9072 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9073 | `		return PH7_OK;` |
|      - | 9074 | `	}` |
|      - | 9075 | `	/* Perform the requested operation */` |
|     76 | 9076 | `	for(;;){` |
|    154 | 9077 | `		if( zIn >= zEnd ){` |
|      - | 9078 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     32 | 9079 | `			ph7_result_bool(pCtx,1);` |
|     32 | 9080 | `			return PH7_OK;` |
|      - | 9081 | `		}` |
|    124 | 9082 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9083 | `			/* UTF-8 stream  */` |
|    ! 0 | 9084 | `			break;` |
|      - | 9085 | `		}` |
|    124 | 9086 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 9087 | `			break;` |
|      - | 9088 | `		}` |
|      - | 9089 | `		/* Point to the next character */` |
|    118 | 9090 | `		zIn++;` |
|      2 | 9091 | `	}` |
|      - | 9092 | `	/* The test failed,return FALSE */` |
|      7 | 9093 | `	ph7_result_bool(pCtx,0);` |
|      7 | 9094 | `	return PH7_OK;` |
|     21 | 9095 | `}` |
|      - | 9096 | `/*` |
|      - | 9097 | ` * bool ctype_graph(string $text)` |
|      - | 9098 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 9099 | ` * Parameters` |
|      - | 9100 | ` *  $text` |
|      - | 9101 | ` *   The tested string.` |
|      - | 9102 | ` * Return` |
|      - | 9103 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 9104 | ` * (no white space), FALSE otherwise.` |
|      - | 9105 | ` */` |
|     16 | 9106 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9107 | `{` |
|      - | 9108 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9109 | `	int nLen;` |
|     17 | 9110 | `	if( nArg < 1 ){` |
|      - | 9111 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9112 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9113 | `		return PH7_OK;` |
|      - | 9114 | `	}` |
|      - | 9115 | `	/* Extract the target string */` |
|     17 | 9116 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9117 | `	zEnd = &zIn[nLen];` |
|     17 | 9118 | `	if( nLen < 1 ){` |
|      - | 9119 | `		/* Empty string,return FALSE */` |
|      3 | 9120 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9121 | `		return PH7_OK;` |
|      - | 9122 | `	}` |
|      - | 9123 | `	/* Perform the requested operation */` |
|     57 | 9124 | `	for(;;){` |
|    115 | 9125 | `		if( zIn >= zEnd ){` |
|      - | 9126 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 9127 | `			ph7_result_bool(pCtx,1);` |
|      9 | 9128 | `			return PH7_OK;` |
|      - | 9129 | `		}` |
|    107 | 9130 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9131 | `			/* UTF-8 stream  */` |
|    ! 0 | 9132 | `			break;` |
|      - | 9133 | `		}` |
|    107 | 9134 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 9135 | `			break;` |
|      - | 9136 | `		}` |
|      - | 9137 | `		/* Point to the next character */` |
|    101 | 9138 | `		zIn++;` |
|      1 | 9139 | `	}` |
|      - | 9140 | `	/* The test failed,return FALSE */` |
|      7 | 9141 | `	ph7_result_bool(pCtx,0);` |
|      7 | 9142 | `	return PH7_OK;` |
|      9 | 9143 | `}` |
|      - | 9144 | `/*` |
|      - | 9145 | ` * bool ctype_print(string $text)` |
|      - | 9146 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 9147 | ` * Parameters` |
|      - | 9148 | ` *  $text` |
|      - | 9149 | ` *   The tested string.` |
|      - | 9150 | ` * Return` |
|      - | 9151 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 9152 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 9153 | ` *  or control function at all.` |
|      - | 9154 | ` */` |
|     16 | 9155 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9156 | `{` |
|      - | 9157 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9158 | `	int nLen;` |
|     17 | 9159 | `	if( nArg < 1 ){` |
|      - | 9160 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9161 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9162 | `		return PH7_OK;` |
|      - | 9163 | `	}` |
|      - | 9164 | `	/* Extract the target string */` |
|     17 | 9165 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9166 | `	zEnd = &zIn[nLen];` |
|     17 | 9167 | `	if( nLen < 1 ){` |
|      - | 9168 | `		/* Empty string,return FALSE */` |
|      3 | 9169 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9170 | `		return PH7_OK;` |
|      - | 9171 | `	}` |
|      - | 9172 | `	/* Perform the requested operation */` |
|     63 | 9173 | `	for(;;){` |
|    127 | 9174 | `		if( zIn >= zEnd ){` |
|      - | 9175 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 9176 | `			ph7_result_bool(pCtx,1);` |
|      9 | 9177 | `			return PH7_OK;` |
|      - | 9178 | `		}` |
|    119 | 9179 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9180 | `			/* UTF-8 stream  */` |
|    ! 0 | 9181 | `			break;` |
|      - | 9182 | `		}` |
|    119 | 9183 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 9184 | `			break;` |
|      - | 9185 | `		}` |
|      - | 9186 | `		/* Point to the next character */` |
|    113 | 9187 | `		zIn++;` |
|      1 | 9188 | `	}` |
|      - | 9189 | `	/* The test failed,return FALSE */` |
|      7 | 9190 | `	ph7_result_bool(pCtx,0);` |
|      7 | 9191 | `	return PH7_OK;` |
|      9 | 9192 | `}` |
|      - | 9193 | `/*` |
|      - | 9194 | ` * bool ctype_punct(string $text)` |
|      - | 9195 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 9196 | ` * Parameters` |
|      - | 9197 | ` *  $text` |
|      - | 9198 | ` *   The tested string.` |
|      - | 9199 | ` * Return` |
|      - | 9200 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 9201 | ` *  digit or blank, FALSE otherwise.` |
|      - | 9202 | ` */` |
|     18 | 9203 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9204 | `{` |
|      - | 9205 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9206 | `	int nLen;` |
|     19 | 9207 | `	if( nArg < 1 ){` |
|      - | 9208 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9209 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9210 | `		return PH7_OK;` |
|      - | 9211 | `	}` |
|      - | 9212 | `	/* Extract the target string */` |
|     19 | 9213 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 9214 | `	zEnd = &zIn[nLen];` |
|     19 | 9215 | `	if( nLen < 1 ){` |
|      - | 9216 | `		/* Empty string,return FALSE */` |
|      3 | 9217 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9218 | `		return PH7_OK;` |
|      - | 9219 | `	}` |
|      - | 9220 | `	/* Perform the requested operation */` |
|     38 | 9221 | `	for(;;){` |
|     77 | 9222 | `		if( zIn >= zEnd ){` |
|      - | 9223 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 9224 | `			ph7_result_bool(pCtx,1);` |
|      9 | 9225 | `			return PH7_OK;` |
|      - | 9226 | `		}` |
|     69 | 9227 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9228 | `			/* UTF-8 stream  */` |
|    ! 0 | 9229 | `			break;` |
|      - | 9230 | `		}` |
|     69 | 9231 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 9232 | `			break;` |
|      - | 9233 | `		}` |
|      - | 9234 | `		/* Point to the next character */` |
|     61 | 9235 | `		zIn++;` |
|      1 | 9236 | `	}` |
|      - | 9237 | `	/* The test failed,return FALSE */` |
|      9 | 9238 | `	ph7_result_bool(pCtx,0);` |
|      9 | 9239 | `	return PH7_OK;` |
|     10 | 9240 | `}` |
|      - | 9241 | `/*` |
|      - | 9242 | ` * bool ctype_space(string $text)` |
|      - | 9243 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 9244 | ` * Parameters` |
|      - | 9245 | ` *  $text` |
|      - | 9246 | ` *   The tested string.` |
|      - | 9247 | ` * Return` |
|      - | 9248 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 9249 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 9250 | ` *  and form feed characters.` |
|      - | 9251 | ` */` |
|  64463 | 9252 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 9253 | `{` |
|      - | 9254 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9255 | `	int nLen;` |
|  64468 | 9256 | `	if( nArg < 1 ){` |
|      - | 9257 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9258 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9259 | `		return PH7_OK;` |
|      - | 9260 | `	}` |
|      - | 9261 | `	/* Extract the target string */` |
|  64468 | 9262 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  64468 | 9263 | `	zEnd = &zIn[nLen];` |
|  64468 | 9264 | `	if( nLen < 1 ){` |
|      - | 9265 | `		/* Empty string,return FALSE */` |
|      3 | 9266 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9267 | `		return PH7_OK;` |
|      - | 9268 | `	}` |
|      - | 9269 | `	/* Perform the requested operation */` |
|  33145 | 9270 | `	for(;;){` |
|  66248 | 9271 | `		if( zIn >= zEnd ){` |
|      - | 9272 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1763 | 9273 | `			ph7_result_bool(pCtx,1);` |
|   1763 | 9274 | `			return PH7_OK;` |
|      - | 9275 | `		}` |
|  64490 | 9276 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9277 | `			/* UTF-8 stream  */` |
|    ! 0 | 9278 | `			break;` |
|      - | 9279 | `		}` |
|  64490 | 9280 | `		if( !SyisSpace(zIn[0]) ){` |
|  62708 | 9281 | `			break;` |
|      - | 9282 | `		}` |
|      - | 9283 | `		/* Point to the next character */` |
|   1787 | 9284 | `		zIn++;` |
|      5 | 9285 | `	}` |
|      - | 9286 | `	/* The test failed,return FALSE */` |
|  62708 | 9287 | `	ph7_result_bool(pCtx,0);` |
|  62708 | 9288 | `	return PH7_OK;` |
|  32260 | 9289 | `}` |
|      - | 9290 | `/*` |
|      - | 9291 | ` * bool ctype_lower(string $text)` |
|      - | 9292 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 9293 | ` * Parameters` |
|      - | 9294 | ` *  $text` |
|      - | 9295 | ` *   The tested string.` |
|      - | 9296 | ` * Return` |
|      - | 9297 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 9298 | ` */` |
|     16 | 9299 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9300 | `{` |
|      - | 9301 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9302 | `	int nLen;` |
|     17 | 9303 | `	if( nArg < 1 ){` |
|      - | 9304 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9305 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9306 | `		return PH7_OK;` |
|      - | 9307 | `	}` |
|      - | 9308 | `	/* Extract the target string */` |
|     17 | 9309 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9310 | `	zEnd = &zIn[nLen];` |
|     17 | 9311 | `	if( nLen < 1 ){` |
|      - | 9312 | `		/* Empty string,return FALSE */` |
|      3 | 9313 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9314 | `		return PH7_OK;` |
|      - | 9315 | `	}` |
|      - | 9316 | `	/* Perform the requested operation */` |
|     27 | 9317 | `	for(;;){` |
|     55 | 9318 | `		if( zIn >= zEnd ){` |
|      - | 9319 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 9320 | `			ph7_result_bool(pCtx,1);` |
|      5 | 9321 | `			return PH7_OK;` |
|      - | 9322 | `		}` |
|     51 | 9323 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 9324 | `			break;` |
|      - | 9325 | `		}` |
|      - | 9326 | `		/* Point to the next character */` |
|     41 | 9327 | `		zIn++;` |
|      1 | 9328 | `	}` |
|      - | 9329 | `	/* The test failed,return FALSE */` |
|     11 | 9330 | `	ph7_result_bool(pCtx,0);` |
|     11 | 9331 | `	return PH7_OK;` |
|      9 | 9332 | `}` |
|      - | 9333 | `/*` |
|      - | 9334 | ` * bool ctype_upper(string $text)` |
|      - | 9335 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 9336 | ` * Parameters` |
|      - | 9337 | ` *  $text` |
|      - | 9338 | ` *   The tested string.` |
|      - | 9339 | ` * Return` |
|      - | 9340 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 9341 | ` */` |
|     16 | 9342 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9343 | `{` |
|      - | 9344 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9345 | `	int nLen;` |
|     17 | 9346 | `	if( nArg < 1 ){` |
|      - | 9347 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9348 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9349 | `		return PH7_OK;` |
|      - | 9350 | `	}` |
|      - | 9351 | `	/* Extract the target string */` |
|     17 | 9352 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9353 | `	zEnd = &zIn[nLen];` |
|     17 | 9354 | `	if( nLen < 1 ){` |
|      - | 9355 | `		/* Empty string,return FALSE */` |
|      3 | 9356 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9357 | `		return PH7_OK;` |
|      - | 9358 | `	}` |
|      - | 9359 | `	/* Perform the requested operation */` |
|     28 | 9360 | `	for(;;){` |
|     57 | 9361 | `		if( zIn >= zEnd ){` |
|      - | 9362 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 9363 | `			ph7_result_bool(pCtx,1);` |
|      5 | 9364 | `			return PH7_OK;` |
|      - | 9365 | `		}` |
|     53 | 9366 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 9367 | `			break;` |
|      - | 9368 | `		}` |
|      - | 9369 | `		/* Point to the next character */` |
|     43 | 9370 | `		zIn++;` |
|      1 | 9371 | `	}` |
|      - | 9372 | `	/* The test failed,return FALSE */` |
|     11 | 9373 | `	ph7_result_bool(pCtx,0);` |
|     11 | 9374 | `	return PH7_OK;` |
|      9 | 9375 | `}` |
|      - | 9376 | `/* Date/Time functions moved to builtin_date.c */` |
|      - | 9377 | `/*` |
|      - | 9378 | ` * Section:` |
|      - | 9379 | ` *    URL handling Functions.` |
|      - | 9380 | ` * Status:` |
|      - | 9381 | ` *    Stable.` |
|      - | 9382 | ` */` |
|      - | 9383 | `/*` |
|      - | 9384 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 9385 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 9386 | ` */` |
|   1270 | 9387 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 9388 | `{` |
|      - | 9389 | `	/* Store in the call context result buffer */` |
|   1272 | 9390 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1272 | 9391 | `	return SXRET_OK;` |
|      2 | 9392 | `}` |
|      - | 9393 | `/*` |
|      - | 9394 | ` * string base64_encode(string $data)` |
|      - | 9395 | ` * string convert_uuencode(string $data)` |
|      - | 9396 | ` *  Encodes data with MIME base64` |
|      - | 9397 | ` * Parameter` |
|      - | 9398 | ` *  $data` |
|      - | 9399 | ` *    Data to encode` |
|      - | 9400 | ` * Return` |
|      - | 9401 | ` *  Encoded data or FALSE on failure.` |
|      - | 9402 | ` */` |
|      6 | 9403 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9404 | `{` |
|      - | 9405 | `	const char *zIn;` |
|      - | 9406 | `	int nLen;` |
|      7 | 9407 | `	if( nArg < 1 ){` |
|      - | 9408 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9409 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9410 | `		return PH7_OK;` |
|      - | 9411 | `	}` |
|      - | 9412 | `	/* Extract the input string */` |
|      7 | 9413 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 9414 | `	if( nLen < 1 ){` |
|      - | 9415 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 9416 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9417 | `		return PH7_OK;` |
|      - | 9418 | `	}` |
|      - | 9419 | `	/* Perform the BASE64 encoding */` |
|      7 | 9420 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 9421 | `	return PH7_OK;` |
|      4 | 9422 | `}` |
|      - | 9423 | `/*` |
|      - | 9424 | ` * string base64_decode(string $data)` |
|      - | 9425 | ` * string convert_uudecode(string $data)` |
|      - | 9426 | ` *  Decodes data encoded with MIME base64` |
|      - | 9427 | ` * Parameter` |
|      - | 9428 | ` *  $data` |
|      - | 9429 | ` *    Encoded data.` |
|      - | 9430 | ` * Return` |
|      - | 9431 | ` *  Returns the original data or FALSE on failure.` |
|      - | 9432 | ` */` |
|     34 | 9433 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 9434 | `{` |
|      - | 9435 | `	const char *zIn;` |
|      - | 9436 | `	int nLen;` |
|     36 | 9437 | `	if( nArg < 1 ){` |
|      - | 9438 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9439 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9440 | `		return PH7_OK;` |
|      - | 9441 | `	}` |
|      - | 9442 | `	/* Extract the input string */` |
|     36 | 9443 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 9444 | `	if( nLen < 1 ){` |
|      - | 9445 | `		/* php decodes the empty string to the EMPTY STRING, not FALSE (FALSE is reserved` |
|      - | 9446 | `		 * for input that cannot be decoded at all). */` |
|      3 | 9447 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 9448 | `		return PH7_OK;` |
|      - | 9449 | `	}` |
|      - | 9450 | `	/* Perform the BASE64 decoding */` |
|     34 | 9451 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 9452 | `	return PH7_OK;` |
|     19 | 9453 | `}` |
|      - | 9454 | `/*` |
|      - | 9455 | ` * string urlencode(string $str)` |
|      - | 9456 | ` *  URL encoding` |
|      - | 9457 | ` * Parameter` |
|      - | 9458 | ` *  $data` |
|      - | 9459 | ` *   Input string.` |
|      - | 9460 | ` * Return` |
|      - | 9461 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 9462 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 9463 | ` *  encoded as plus (+) signs.` |
|      - | 9464 | ` */` |
|    100 | 9465 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9466 | `{` |
|      - | 9467 | `	const char *zIn;` |
|      - | 9468 | `	int nLen;` |
|    101 | 9469 | `	if( nArg < 1 ){` |
|      - | 9470 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9471 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9472 | `		return PH7_OK;` |
|      - | 9473 | `	}` |
|      - | 9474 | `	/* Extract the input string */` |
|    101 | 9475 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|    101 | 9476 | `	if( nLen < 1 ){` |
|      - | 9477 | `		/* php returns an empty string for empty input, not FALSE */` |
|      3 | 9478 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 9479 | `		return PH7_OK;` |
|      - | 9480 | `	}` |
|      - | 9481 | `	/* Perform the URL encoding */` |
|     99 | 9482 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     99 | 9483 | `	return PH7_OK;` |
|     51 | 9484 | `}` |
|      - | 9485 | `/*` |
|      - | 9486 | ` * string rawurlencode(string $str)` |
|      - | 9487 | ` *  RFC 3986 URL encoding: spaces become %20 (not '+') and '~' is left intact.` |
|      - | 9488 | ` */` |
|     14 | 9489 | `static int PH7_builtin_rawurlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9490 | `{` |
|      - | 9491 | `	const char *zIn;` |
|      - | 9492 | `	int nLen;` |
|     15 | 9493 | `	if( nArg < 1 ){` |
|      - | 9494 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9495 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9496 | `		return PH7_OK;` |
|      - | 9497 | `	}` |
|      - | 9498 | `	/* Extract the input string */` |
|     15 | 9499 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 9500 | `	if( nLen < 1 ){` |
|      - | 9501 | `		/* php returns an empty string for empty input, not FALSE */` |
|      3 | 9502 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 9503 | `		return PH7_OK;` |
|      - | 9504 | `	}` |
|      - | 9505 | `	/* Perform the RFC 3986 URL encoding */` |
|     13 | 9506 | `	SyUriEncodeRaw(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     13 | 9507 | `	return PH7_OK;` |
|      8 | 9508 | `}` |
|      - | 9509 | `/*` |
|      - | 9510 | ` * string urldecode(string $str)` |
|      - | 9511 | ` *  Decodes any %## encoding in the given string.` |
|      - | 9512 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 9513 | ` * Parameter` |
|      - | 9514 | ` *  $data` |
|      - | 9515 | ` *    Input string.` |
|      - | 9516 | ` * Return` |
|      - | 9517 | ` *  Decoded URL or FALSE on failure.` |
|      - | 9518 | ` */` |
|    110 | 9519 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9520 | `{` |
|      - | 9521 | `	const char *zIn;` |
|      - | 9522 | `	int nLen;` |
|    111 | 9523 | `	if( nArg < 1 ){` |
|      - | 9524 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9525 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9526 | `		return PH7_OK;` |
|      - | 9527 | `	}` |
|      - | 9528 | `	/* Extract the input string */` |
|    111 | 9529 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|    111 | 9530 | `	if( nLen < 1 ){` |
|      - | 9531 | `		/* php returns an empty string for empty input, not FALSE */` |
|     17 | 9532 | `		ph7_result_string(pCtx,"",0);` |
|     17 | 9533 | `		return PH7_OK;` |
|      - | 9534 | `	}` |
|      - | 9535 | `	/* Perform the URL decoding */` |
|     95 | 9536 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|     95 | 9537 | `	return PH7_OK;` |
|     56 | 9538 | `}` |
|      - | 9539 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9540 | `/* Table of the built-in functions */` |
|      - | 9541 | `/*` |
|      - | 9542 | ` * int memory_get_usage([bool $real_usage = false])` |
|      - | 9543 | ` *  Amount of memory, in bytes, currently allocated to the script through PHL's` |
|      - | 9544 | ` *  memory backend. PHL tracks the backend's real allocated bytes, so the` |
|      - | 9545 | ` *  $real_usage flag has no effect here (php's non-real figure would be smaller,` |
|      - | 9546 | ` *  reflecting Zend's emalloc bookkeeping — recorded divergence).` |
|      - | 9547 | ` */` |
|    ! 0 | 9548 | `static int PH7_builtin_memory_get_usage(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 9549 | `{` |
|    ! 0 | 9550 | `	SXUNUSED(nArg);` |
|    ! 0 | 9551 | `	SXUNUSED(apArg);` |
|    ! 0 | 9552 | `	ph7_result_int64(pCtx,(ph7_int64)pCtx->pVm->sAllocator.nMemUsed);` |
|    ! 0 | 9553 | `	return PH7_OK;` |
|    ! 0 | 9554 | `}` |
|      - | 9555 | `/*` |
|      - | 9556 | ` * int memory_get_peak_usage([bool $real_usage = false])` |
|      - | 9557 | ` *  High-water mark of memory_get_usage() over the script's lifetime.` |
|      - | 9558 | ` */` |
|    ! 0 | 9559 | `static int PH7_builtin_memory_get_peak_usage(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 9560 | `{` |
|    ! 0 | 9561 | `	SXUNUSED(nArg);` |
|    ! 0 | 9562 | `	SXUNUSED(apArg);` |
|    ! 0 | 9563 | `	ph7_result_int64(pCtx,(ph7_int64)pCtx->pVm->sAllocator.nMemPeak);` |
|    ! 0 | 9564 | `	return PH7_OK;` |
|    ! 0 | 9565 | `}` |
|      - | 9566 | `/*` |
|      - | 9567 | ` * PHL frees values by reference count as they go out of scope, so there is no` |
|      - | 9568 | ` * mark-and-sweep cycle collector to drive. The gc_* family is provided for` |
|      - | 9569 | ` * source compatibility (real frameworks call it around test runs): the state is` |
|      - | 9570 | ` * observational and collection is a no-op. Recorded divergence from php, whose` |
|      - | 9571 | ` * collector actually reclaims reference cycles.` |
|      - | 9572 | ` */` |
|    ! 0 | 9573 | `static int PH7_builtin_gc_enable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 9574 | `{` |
|    ! 0 | 9575 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    ! 0 | 9576 | `	pCtx->pVm->bGcEnabled = 1;` |
|    ! 0 | 9577 | `	return PH7_OK;` |
|    ! 0 | 9578 | `}` |
|    ! 0 | 9579 | `static int PH7_builtin_gc_disable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 9580 | `{` |
|    ! 0 | 9581 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    ! 0 | 9582 | `	pCtx->pVm->bGcEnabled = 0;` |
|    ! 0 | 9583 | `	return PH7_OK;` |
|    ! 0 | 9584 | `}` |
|    ! 0 | 9585 | `static int PH7_builtin_gc_enabled(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 9586 | `{` |
|    ! 0 | 9587 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    ! 0 | 9588 | `	ph7_result_bool(pCtx,pCtx->pVm->bGcEnabled);` |
|    ! 0 | 9589 | `	return PH7_OK;` |
|    ! 0 | 9590 | `}` |
|    ! 0 | 9591 | `static int PH7_builtin_gc_collect_cycles(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 9592 | `{` |
|      - | 9593 | `	/* No cycle collector: nothing to reclaim. Returns the count collected (0). */` |
|    ! 0 | 9594 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    ! 0 | 9595 | `	ph7_result_int(pCtx,0);` |
|    ! 0 | 9596 | `	return PH7_OK;` |
|    ! 0 | 9597 | `}` |
|    ! 0 | 9598 | `static int PH7_builtin_gc_mem_caches(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 9599 | `{` |
|    ! 0 | 9600 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    ! 0 | 9601 | `	ph7_result_int(pCtx,0);` |
|    ! 0 | 9602 | `	return PH7_OK;` |
|    ! 0 | 9603 | `}` |
|      - | 9604 | `/*` |
|      - | 9605 | ` * array gc_status(void)` |
|      - | 9606 | ` *  php 8.3 shape. PHL never runs a collection, so every counter is zero and the` |
|      - | 9607 | ` *  timing fields are 0.0; 'running' reflects gc_enable()/gc_disable().` |
|      - | 9608 | ` */` |
|    ! 0 | 9609 | `static int PH7_builtin_gc_status(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 9610 | `{` |
|      - | 9611 | `	ph7_value *pArray,*pVal;` |
|    ! 0 | 9612 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    ! 0 | 9613 | `	pArray = ph7_context_new_array(pCtx);` |
|    ! 0 | 9614 | `	pVal = ph7_context_new_scalar(pCtx);` |
|    ! 0 | 9615 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|    ! 0 | 9616 | `		ph7_result_null(pCtx);` |
|    ! 0 | 9617 | `		return PH7_OK;` |
|      - | 9618 | `	}` |
|      - | 9619 | `	/* Key order matches php 8.3's gc_status(). */` |
|    ! 0 | 9620 | `	ph7_value_bool(pVal,pCtx->pVm->bGcEnabled); ph7_array_add_strkey_elem(pArray,"running",pVal);` |
|    ! 0 | 9621 | `	ph7_value_bool(pVal,0);      ph7_array_add_strkey_elem(pArray,"protected",pVal);` |
|    ! 0 | 9622 | `	ph7_value_bool(pVal,0);      ph7_array_add_strkey_elem(pArray,"full",pVal);` |
|    ! 0 | 9623 | `	ph7_value_int(pVal,0);       ph7_array_add_strkey_elem(pArray,"runs",pVal);` |
|    ! 0 | 9624 | `	ph7_value_int(pVal,0);       ph7_array_add_strkey_elem(pArray,"collected",pVal);` |
|    ! 0 | 9625 | `	ph7_value_int(pVal,1000);    ph7_array_add_strkey_elem(pArray,"threshold",pVal);` |
|    ! 0 | 9626 | `	ph7_value_int(pVal,0);       ph7_array_add_strkey_elem(pArray,"buffer_size",pVal);` |
|    ! 0 | 9627 | `	ph7_value_int(pVal,0);       ph7_array_add_strkey_elem(pArray,"roots",pVal);` |
|    ! 0 | 9628 | `	ph7_value_double(pVal,0.0);  ph7_array_add_strkey_elem(pArray,"application_time",pVal);` |
|    ! 0 | 9629 | `	ph7_value_double(pVal,0.0);  ph7_array_add_strkey_elem(pArray,"collector_time",pVal);` |
|    ! 0 | 9630 | `	ph7_value_double(pVal,0.0);  ph7_array_add_strkey_elem(pArray,"destructor_time",pVal);` |
|    ! 0 | 9631 | `	ph7_value_double(pVal,0.0);  ph7_array_add_strkey_elem(pArray,"free_time",pVal);` |
|    ! 0 | 9632 | `	ph7_context_release_value(pCtx,pVal);` |
|    ! 0 | 9633 | `	ph7_result_value(pCtx,pArray);` |
|    ! 0 | 9634 | `	return PH7_OK;` |
|    ! 0 | 9635 | `}` |
|      - | 9636 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 9637 | `	{ "memory_get_usage"     , PH7_builtin_memory_get_usage      },` |
|      - | 9638 | `	{ "memory_get_peak_usage", PH7_builtin_memory_get_peak_usage },` |
|      - | 9639 | `	{ "gc_enable"            , PH7_builtin_gc_enable             },` |
|      - | 9640 | `	{ "gc_disable"           , PH7_builtin_gc_disable            },` |
|      - | 9641 | `	{ "gc_enabled"           , PH7_builtin_gc_enabled            },` |
|      - | 9642 | `	{ "gc_collect_cycles"    , PH7_builtin_gc_collect_cycles     },` |
|      - | 9643 | `	{ "gc_mem_caches"        , PH7_builtin_gc_mem_caches         },` |
|      - | 9644 | `	{ "gc_status"            , PH7_builtin_gc_status             },` |
|      - | 9645 | `	   /* Variable handling functions */` |
|      - | 9646 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 9647 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 9648 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 9649 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 9650 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 9651 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 9652 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 9653 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 9654 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 9655 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 9656 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 9657 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 9658 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 9659 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 9660 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 9661 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 9662 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 9663 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 9664 | `	{ "boolval"    , PH7_builtin_boolval     },` |
|      - | 9665 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 9666 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9667 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 9668 | `	   /* Math functions */` |
|      - | 9669 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 9670 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 9671 | `	{ "acosh" ,   PH7_builtin_acosh        },` |
|      - | 9672 | `	{ "asinh" ,   PH7_builtin_asinh        },` |
|      - | 9673 | `	{ "atanh" ,   PH7_builtin_atanh        },` |
|      - | 9674 | `	{ "expm1" ,   PH7_builtin_expm1        },` |
|      - | 9675 | `	{ "log1p" ,   PH7_builtin_log1p        },` |
|      - | 9676 | `	{ "deg2rad" , PH7_builtin_deg2rad      },` |
|      - | 9677 | `	{ "rad2deg" , PH7_builtin_rad2deg      },` |
|      - | 9678 | `	{ "fpow" ,    PH7_builtin_fpow         },` |
|      - | 9679 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 9680 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 9681 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 9682 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 9683 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 9684 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 9685 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 9686 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 9687 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 9688 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 9689 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 9690 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 9691 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 9692 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 9693 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 9694 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 9695 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 9696 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 9697 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 9698 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 9699 | `	{ "round",    PH7_builtin_round        },` |
|      - | 9700 | `	{ "intdiv",   PH7_builtin_intdiv       },` |
|      - | 9701 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 9702 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 9703 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 9704 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 9705 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 9706 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 9707 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 9708 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 9709 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9710 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9711 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 9712 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9713 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9714 | `	   /* String handling functions */` |
|      - | 9715 |  |
|      - | 9716 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 9717 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 9718 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 9719 | `	{ "substr_replace",  PH7_builtin_substr_replace },` |
|      - | 9720 | `	{ "levenshtein",     PH7_builtin_levenshtein },` |
|      - | 9721 | `	{ "similar_text",    PH7_builtin_similar_text },` |
|      - | 9722 | `	{ "str_word_count",  PH7_builtin_str_word_count },` |
|      - | 9723 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 9724 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 9725 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 9726 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 9727 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 9728 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 9729 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 9730 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 9731 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 9732 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 9733 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 9734 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 9735 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 9736 | `	{ "strnatcmp"  , PH7_builtin_strnatcmp  },` |
|      - | 9737 | `	{ "strnatcasecmp", PH7_builtin_strnatcmp },` |
|      - | 9738 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 9739 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 9740 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 9741 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 9742 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 9743 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 9744 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 9745 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 9746 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 9747 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 9748 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 9749 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 9750 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 9751 | `	{ "mb_strtolower",PH7_builtin_mb_case_f }, /* UTF-8 only (builtin_mb.c) */` |
|      - | 9752 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 9753 | `	{ "mb_strtoupper",PH7_builtin_mb_case_f }, /* UTF-8 only (builtin_mb.c) */` |
|      - | 9754 | `	{ "mb_strlen",    PH7_builtin_mb_strlen_f },` |
|      - | 9755 | `	{ "mb_substr",    PH7_builtin_mb_substr_f },` |
|      - | 9756 | `	{ "mb_convert_case", PH7_builtin_mb_convert_case_f },` |
|      - | 9757 | `	{ "mb_strpos",    PH7_builtin_mb_strpos_f },` |
|      - | 9758 | `	{ "mb_stripos",   PH7_builtin_mb_strpos_f },` |
|      - | 9759 | `	{ "mb_strrpos",   PH7_builtin_mb_strpos_f },` |
|      - | 9760 | `	{ "mb_str_split", PH7_builtin_mb_str_split_f },` |
|      - | 9761 | `	{ "mb_internal_encoding", PH7_builtin_mb_internal_encoding_f },` |
|      - | 9762 | `	{ "mb_check_encoding",    PH7_builtin_mb_check_encoding_f },` |
|      - | 9763 | `	{ "mb_strwidth",  PH7_builtin_mb_strwidth_f },` |
|      - | 9764 | `	{ "mb_chr",       PH7_builtin_mb_chr_f   },` |
|      - | 9765 | `	{ "mb_ord",       PH7_builtin_mb_ord_f   },` |
|      - | 9766 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 9767 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 9768 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 9769 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 9770 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 9771 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 9772 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 9773 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 9774 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 9775 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 9776 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 9777 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 9778 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 9779 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 9780 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 9781 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 9782 | `	{ "str_contains", PH7_builtin_str_contains },` |
|      - | 9783 | `	{ "str_starts_with", PH7_builtin_str_starts_with },` |
|      - | 9784 | `	{ "str_ends_with", PH7_builtin_str_ends_with },` |
|      - | 9785 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 9786 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9787 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9788 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 9789 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 9790 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 9791 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 9792 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9793 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9794 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 9795 |  |
|      - | 9796 |  |
|      - | 9797 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 9798 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 9799 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 9800 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 9801 | `	{ "hash",         PH7_builtin_hash      },` |
|      - | 9802 | `	{ "hash_hmac",    PH7_builtin_hash_hmac },` |
|      - | 9803 | `	{ "hash_equals",  PH7_builtin_hash_equals },` |
|      - | 9804 | `	{ "hash_algos",   PH7_builtin_hash_algos },` |
|      - | 9805 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 9806 | `	{ "password_hash",         PH7_builtin_password_hash },` |
|      - | 9807 | `	{ "password_verify",       PH7_builtin_password_verify },` |
|      - | 9808 | `	{ "password_get_info",     PH7_builtin_password_get_info },` |
|      - | 9809 | `	{ "password_needs_rehash", PH7_builtin_password_needs_rehash },` |
|      - | 9810 | `	{ "filter_var",            PH7_builtin_filter_var },` |
|      - | 9811 | `	{ "filter_input",          PH7_builtin_filter_input },` |
|      - | 9812 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9813 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9814 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 9815 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 9816 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9817 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9818 |  |
|      - | 9819 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 9820 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 9821 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 9822 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 9823 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 9824 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 9825 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 9826 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 9827 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 9828 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 9829 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 9830 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 9831 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9832 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9833 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 9834 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9835 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9836 |  |
|      - | 9837 | `	         /* Ctype functions */` |
|      - | 9838 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 9839 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 9840 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 9841 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 9842 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 9843 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 9844 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 9845 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 9846 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 9847 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 9848 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 9849 | `	         /* Time functions */` |
|      - | 9850 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 9851 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 9852 | `	{ "hrtime",      PH7_builtin_hrtime       },` |
|      - | 9853 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 9854 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 9855 | `	{ "date",        PH7_builtin_date         },` |
|      - | 9856 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 9857 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 9858 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 9859 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 9860 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 9861 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 9862 | `	{ "date_default_timezone_get", PH7_builtin_date_default_timezone_get },` |
|      - | 9863 | `	{ "date_default_timezone_set", PH7_builtin_date_default_timezone_set },` |
|      - | 9864 | `	        /* URL functions */` |
|      - | 9865 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 9866 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 9867 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 9868 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 9869 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 9870 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 9871 | `	{ "rawurlencode", PH7_builtin_rawurlencode },` |
|      - | 9872 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 9873 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9874 | `};` |
|      - | 9875 | `/*` |
|      - | 9876 | ` * Register the built-in functions defined above,the array functions` |
|      - | 9877 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 9878 | ` */` |
|   3360 | 9879 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      5 | 9880 | `{` |
|      - | 9881 | `	sxu32 n;` |
| 688805 | 9882 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 685445 | 9883 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 342725 | 9884 | `	}` |
|      - | 9885 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   3365 | 9886 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 9887 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   3365 | 9888 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   3365 | 9889 | `}` |
|      - | 9890 |  |
