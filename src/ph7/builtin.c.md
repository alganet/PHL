# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4305/5043 lines (85.37%)

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
| 479858 |   18 | `PH7_PRIVATE sxi32 PH7_IntArgResolve(` |
|      - |   19 | `	ph7_context *pCtx,` |
|      - |   20 | `	ph7_value *pArg,` |
|      - |   21 | `	const char *zFunc,` |
|      - |   22 | `	int iArgNum,` |
|      - |   23 | `	const char *zParamName,` |
|      - |   24 | `	const char *zTypeStr,` |
|      - |   25 | `	sxi64 *pOut` |
|      5 |   26 | `){` |
| 479863 |   27 | `	if( ph7_value_is_null(pArg) ){` |
|    ! 0 |   28 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|      - |   29 | `			"%s(): Passing null to parameter #%d (%s) of type %s is deprecated",` |
|    ! 0 |   30 | `			zFunc,iArgNum,zParamName,zTypeStr` |
|      - |   31 | `			);` |
|    ! 0 |   32 | `		*pOut = 0;` |
|    ! 0 |   33 | `		return PH7_OK;` |
|      - |   34 | `	}` |
| 479863 |   35 | `	if( ph7_value_is_float(pArg) ){` |
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
| 479857 |   56 | `	if( ph7_value_is_string(pArg) ){` |
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
| 479843 |   98 | `	if( !ph7_value_is_int(pArg) && !ph7_value_is_bool(pArg) ){` |
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
| 479843 |  113 | `	*pOut = ph7_value_to_int64(pArg);` |
| 479843 |  114 | `	return PH7_OK;` |
| 239934 |  115 | `}` |
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
|    868 |  178 | `static int PH7_builtin_is_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  179 | `{` |
|    871 |  180 | `	int res = 0; /* Assume false by default */` |
|    871 |  181 | `	if( nArg > 0 ){` |
|      - |  182 | `		/* Strict PHP identity: a float is never an int, even when it holds an` |
|      - |  183 | `		 * integer value (1.0). An integer-valued real carries both MEMOBJ_INT` |
|      - |  184 | `		 * (cached) and MEMOBJ_REAL, so REAL must be excluded here. */` |
|    871 |  185 | `		res = ph7_value_is_int(apArg[0]) && !ph7_value_is_float(apArg[0]);` |
|    434 |  186 | `	}` |
|      - |  187 | `	/* Query result */` |
|    871 |  188 | `	ph7_result_bool(pCtx,res);` |
|    871 |  189 | `	return PH7_OK;` |
|      3 |  190 | `}` |
|      - |  191 | `/*` |
|      - |  192 | ` * bool is_string($var)` |
|      - |  193 | ` *  Finds out whether a variable is a string.` |
|      - |  194 | ` * Parameters` |
|      - |  195 | ` *   $var: The variable being evaluated.` |
|      - |  196 | ` * Return` |
|      - |  197 | ` *  TRUE if var is string. False otherwise.` |
|      - |  198 | ` */` |
|    762 |  199 | `static int PH7_builtin_is_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  200 | `{` |
|    765 |  201 | `	int res = 0; /* Assume false by default */` |
|    765 |  202 | `	if( nArg > 0 ){` |
|    765 |  203 | `		res = ph7_value_is_string(apArg[0]);` |
|    381 |  204 | `	}` |
|      - |  205 | `	/* Query result */` |
|    765 |  206 | `	ph7_result_bool(pCtx,res);` |
|    765 |  207 | `	return PH7_OK;` |
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
|    648 |  271 | `static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  272 | `{` |
|    652 |  273 | `	int res = 0; /* Assume false by default */` |
|    652 |  274 | `	if( nArg > 0 ){` |
|    652 |  275 | `		res = ph7_value_is_array(apArg[0]);` |
|    324 |  276 | `	}` |
|      - |  277 | `	/* Query result */` |
|    652 |  278 | `	ph7_result_bool(pCtx,res);` |
|    652 |  279 | `	return PH7_OK;` |
|      4 |  280 | `}` |
|      - |  281 | `/*` |
|      - |  282 | ` * bool is_object($var)` |
|      - |  283 | ` *  Find out whether a variable is an object.` |
|      - |  284 | ` * Parameters` |
|      - |  285 | ` *  $var: The variable being evaluated.` |
|      - |  286 | ` * Return` |
|      - |  287 | ` *  True if var is an object. False otherwise.` |
|      - |  288 | ` */` |
|    450 |  289 | `static int PH7_builtin_is_object(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  290 | `{` |
|    453 |  291 | `	int res = 0; /* Assume false by default */` |
|    453 |  292 | `	if( nArg > 0 ){` |
|    453 |  293 | `		res = ph7_value_is_object(apArg[0]);` |
|    225 |  294 | `	}` |
|      - |  295 | `	/* Query result */` |
|    453 |  296 | `	ph7_result_bool(pCtx,res);` |
|    453 |  297 | `	return PH7_OK;` |
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
|  33442 |  411 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  412 | `{` |
|  33447 |  413 | `	int res = 1; /* Assume empty by default */` |
|  33447 |  414 | `	if( nArg > 0 ){` |
|  33445 |  415 | `		res = ph7_value_is_empty(apArg[0]);` |
|  16720 |  416 | `	}` |
|  33447 |  417 | `	ph7_result_bool(pCtx,res);` |
|  33447 |  418 | `	return PH7_OK;` |
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
| 264704 |  461 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  462 | `{` |
|      - |  463 | `	const char *zSource;` |
|      - |  464 | `	int nSrcLen;` |
|      - |  465 | `	sxi64 iStart,iEnd;` |
| 264709 |  466 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"substr",1,"$string"); }` |
| 264709 |  467 | `	if( nArg < 2 ){` |
|      - |  468 | `		/* Arity is enforced at the call boundary; nothing sensible to return here. */` |
|    ! 0 |  469 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 |  470 | `		return PH7_OK;` |
|      - |  471 | `	}` |
|      - |  472 | `	/* Extract the target string */` |
| 264709 |  473 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|      - |  474 | `	/* Extract the offset */` |
|      - |  475 | `	{` |
| 264709 |  476 | `		sxi64 iTmp = 0;` |
| 264709 |  477 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"substr",2,"$offset","int",&iTmp);` |
| 264709 |  478 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |  479 | `			return rcArg;` |
|      - |  480 | `		}` |
| 264709 |  481 | `		iStart = iTmp;` |
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
| 264709 |  493 | `	if( iStart < 0 ){` |
|  32847 |  494 | `		iStart += nSrcLen;` |
|  32847 |  495 | `		if( iStart < 0 ){` |
|      5 |  496 | `			iStart = 0;` |
|      7 |  497 | `		}` |
| 248288 |  498 | `	}else if( iStart > nSrcLen ){` |
|      7 |  499 | `		iStart = nSrcLen;` |
|      3 |  500 | `	}` |
| 264709 |  501 | `	iEnd = nSrcLen;` |
| 264709 |  502 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
| 194215 |  503 | `		sxi64 iLen = 0;` |
| 194215 |  504 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[2],"substr",3,"$length","?int",&iLen);` |
| 194215 |  505 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |  506 | `			return rcArg;` |
|      - |  507 | `		}` |
| 194215 |  508 | `		if( iLen < 0 ){` |
|  32779 |  509 | `			iEnd = (sxi64)nSrcLen + iLen;` |
| 177828 |  510 | `		}else if( iLen > (sxi64)nSrcLen - iStart ){` |
|  18703 |  511 | `			iEnd = nSrcLen;` |
|   9354 |  512 | `		}else{` |
| 142743 |  513 | `			iEnd = iStart + iLen;` |
|      - |  514 | `		}` |
|  97105 |  515 | `	}` |
| 264709 |  516 | `	if( iEnd < iStart ){` |
|      3 |  517 | `		iEnd = iStart;` |
|      1 |  518 | `	}` |
| 264709 |  519 | `	ph7_result_string(pCtx,&zSource[iStart],(int)(iEnd - iStart));` |
| 264709 |  520 | `	return PH7_OK;` |
| 132357 |  521 | `}` |
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
| 390746 |  716 | `static void StrNullArgNotice(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNum,const char *zParamName)` |
|      5 |  717 | `{` |
| 390751 |  718 | `	if( ph7_value_is_null(pArg) ){` |
|     22 |  719 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|      - |  720 | `			"%s(): Passing null to parameter #%d (%s) of type string is deprecated",` |
|      7 |  721 | `			zFunc,iArgNum,zParamName);` |
|      7 |  722 | `	}` |
| 390751 |  723 | `}` |
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
|    230 | 1580 | `static void PH7_BuildCharMask(ph7_context *pCtx,const char *zList,int nLen,char aMask[256])` |
|      5 | 1581 | `{` |
|    235 | 1582 | `	const unsigned char *zIn  = (const unsigned char *)zList;` |
|    235 | 1583 | `	const unsigned char *zEnd = zIn + (nLen > 0 ? nLen : 0);` |
|    235 | 1584 | `	SyZero(aMask,256);` |
|    627 | 1585 | `	for( ; zIn < zEnd ; zIn++ ){` |
|    397 | 1586 | `		int c = zIn[0];` |
|    397 | 1587 | `		if( zIn + 3 < zEnd && zIn[1] == '.' && zIn[2] == '.' && zIn[3] >= c ){` |
|      - | 1588 | `			/* Valid incrementing range c..zIn[3] */` |
|     22 | 1589 | `			int hi = zIn[3],k;` |
|    386 | 1590 | `			for( k = c ; k <= hi ; k++ ){` |
|    366 | 1591 | `				aMask[k] = 1;` |
|    184 | 1592 | `			}` |
|     22 | 1593 | `			zIn += 3; /* the loop's ++ then steps past the range end */` |
|    396 | 1594 | `		}else if( zIn + 1 < zEnd && zIn[0] == '.' && zIn[1] == '.' ){` |
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
|    359 | 1616 | `			aMask[c] = 1;` |
|      - | 1617 | `		}` |
|    201 | 1618 | `	}` |
|    235 | 1619 | `}` |
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
|      4 | 1636 | `{` |
|      - | 1637 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|      - | 1638 | `	char aMask[256];` |
|      - | 1639 | `	int nLen,nMask;` |
|      - | 1640 | `	/* PHP enforces exactly two arguments. */` |
|     38 | 1641 | `	if( nArg != 2 ){` |
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
|     21 | 1738 | `}` |
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
|  75228 | 2016 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2017 | `{` |
|  75233 | 2018 | `	int iLen = 0;` |
|  75233 | 2019 | `	if( nArg > 0 ){` |
|  75233 | 2020 | `		StrNullArgNotice(pCtx,apArg[0],"strlen",1,"$string");` |
|  75233 | 2021 | `		ph7_value_to_string(apArg[0],&iLen);` |
|  37614 | 2022 | `	}` |
|      - | 2023 | `	/* String length */` |
|  75233 | 2024 | `	ph7_result_int(pCtx,iLen);` |
|  75233 | 2025 | `	return PH7_OK;` |
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
|      - | 2055 | `/*` |
|      - | 2056 | ` * Natural-order comparison core (Martin Pool's natcompare as adapted by php's` |
|      - | 2057 | ` * ext/standard/strnatcmp.c): digit runs compare numerically — the longer run` |
|      - | 2058 | ` * wins, a leading zero flips to fractional first-difference-wins semantics —` |
|      - | 2059 | ` * everything else compares bytewise with whitespace skipped.` |
|      - | 2060 | ` */` |
|     16 | 2061 | `static int StrNatCompareRight(const char **pa,const char *aEnd,const char **pb,const char *bEnd)` |
|      1 | 2062 | `{` |
|     17 | 2063 | `	int bias = 0;` |
|     30 | 2064 | `	for(;;){` |
|     39 | 2065 | `		int da = (*pa < aEnd) && SyisDigit(**pa);` |
|     39 | 2066 | `		int db = (*pb < bEnd) && SyisDigit(**pb);` |
|     39 | 2067 | `		if( !da && !db ){ return bias; }` |
|     31 | 2068 | `		if( !da ){ return -1; }` |
|     25 | 2069 | `		if( !db ){ return 1; }` |
|     23 | 2070 | `		if( **pa < **pb ){ if( !bias ){ bias = -1; } }` |
|     21 | 2071 | `		else if( **pa > **pb ){ if( !bias ){ bias = 1; } }` |
|     23 | 2072 | `		(*pa)++;` |
|     23 | 2073 | `		(*pb)++;` |
|      1 | 2074 | `	}` |
|      9 | 2075 | `}` |
|      2 | 2076 | `static int StrNatCompareLeft(const char **pa,const char *aEnd,const char **pb,const char *bEnd)` |
|      1 | 2077 | `{` |
|      1 | 2078 | `	for(;;){` |
|      3 | 2079 | `		int da = (*pa < aEnd) && SyisDigit(**pa);` |
|      3 | 2080 | `		int db = (*pb < bEnd) && SyisDigit(**pb);` |
|      3 | 2081 | `		if( !da && !db ){ return 0; }` |
|      3 | 2082 | `		if( !da ){ return -1; }` |
|      3 | 2083 | `		if( !db ){ return 1; }` |
|      3 | 2084 | `		if( **pa < **pb ){ return -1; }` |
|    ! 0 | 2085 | `		if( **pa > **pb ){ return 1; }` |
|    ! 0 | 2086 | `		(*pa)++;` |
|    ! 0 | 2087 | `		(*pb)++;` |
|    ! 0 | 2088 | `	}` |
|      2 | 2089 | `}` |
|     20 | 2090 | `static int StrNatCmpCore(const char *zA,int nA,const char *zB,int nB,int bFold)` |
|      1 | 2091 | `{` |
|     21 | 2092 | `	const char *a = zA,*aEnd = &zA[nA];` |
|     21 | 2093 | `	const char *b = zB,*bEnd = &zB[nB];` |
|     59 | 2094 | `	for(;;){` |
|      - | 2095 | `		int ca,cb;` |
|     73 | 2096 | `		while( a < aEnd && SyisSpace(a[0]) ){ a++; }` |
|     71 | 2097 | `		while( b < bEnd && SyisSpace(b[0]) ){ b++; }` |
|     71 | 2098 | `		ca = (a < aEnd) ? (unsigned char)a[0] : 0;` |
|     71 | 2099 | `		cb = (b < bEnd) ? (unsigned char)b[0] : 0;` |
|     71 | 2100 | `		if( SyisDigit(ca) && SyisDigit(cb) ){` |
|     18 | 2101 | `			int r = (ca == '0' \|\| cb == '0')` |
|      2 | 2102 | `				? StrNatCompareLeft(&a,aEnd,&b,bEnd)` |
|     25 | 2103 | `				: StrNatCompareRight(&a,aEnd,&b,bEnd);` |
|     19 | 2104 | `			if( r ){ return r; }` |
|      3 | 2105 | `			continue;` |
|      - | 2106 | `		}` |
|     53 | 2107 | `		if( ca == 0 && cb == 0 ){ return 0; }` |
|     49 | 2108 | `		if( bFold ){` |
|     49 | 2109 | `			ca = SyToLower(ca);` |
|     49 | 2110 | `			cb = SyToLower(cb);` |
|     24 | 2111 | `		}` |
|     49 | 2112 | `		if( ca < cb ){ return -1; }` |
|     49 | 2113 | `		if( ca > cb ){ return 1; }` |
|     49 | 2114 | `		a++;` |
|     49 | 2115 | `		b++;` |
|      1 | 2116 | `	}` |
|     11 | 2117 | `}` |
|      - | 2118 | `/*` |
|      - | 2119 | ` * int strnatcmp(string $string1, string $string2)` |
|      - | 2120 | ` * int strnatcasecmp(string $string1, string $string2)` |
|      - | 2121 | ` *  Natural-order string comparison ("img2" < "img10"), case folded for the` |
|      - | 2122 | ` *  latter. php 8.2+ normalizes the result to -1/0/1.` |
|      - | 2123 | ` */` |
|     20 | 2124 | `static int PH7_builtin_strnatcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2125 | `{` |
|      - | 2126 | `	const char *z1,*z2,*zFunc;` |
|      - | 2127 | `	int n1,n2,bFold;` |
|     21 | 2128 | `	if( nArg < 2 ){` |
|    ! 0 | 2129 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2130 | `		return PH7_OK;` |
|      - | 2131 | `	}` |
|     21 | 2132 | `	zFunc = ph7_function_name(pCtx);` |
|     21 | 2133 | `	bFold = zFunc[sizeof("strnat")-1] == 'c'; /* strnatCasecmp */` |
|     21 | 2134 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     21 | 2135 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     21 | 2136 | `	ph7_result_int(pCtx,StrNatCmpCore(z1,n1,z2,n2,bFold));` |
|     21 | 2137 | `	return PH7_OK;` |
|     11 | 2138 | `}` |
|      - | 2139 | `/*` |
|      - | 2140 | ` * int strncmp(string $str1,string $str2,int n)` |
|      - | 2141 | ` *  Perform a binary safe string comparison of the first n characters.` |
|      - | 2142 | ` * Parameter` |
|      - | 2143 | ` *  str1: The first string` |
|      - | 2144 | ` *  str2: The second string` |
|      - | 2145 | ` * Return` |
|      - | 2146 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2147 | ` *  than str2, and 0 if they are equal.` |
|      - | 2148 | ` */` |
|     66 | 2149 | `static int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2150 | `{` |
|      - | 2151 | `	const char *z1,*z2;` |
|      - | 2152 | `	int res;` |
|      - | 2153 | `	int n;` |
|     68 | 2154 | `	if( nArg < 3 ){` |
|      - | 2155 | `		/* Perform a standard comparison */` |
|    ! 0 | 2156 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|      - | 2157 | `	}` |
|      - | 2158 | `	/* Desired comparison length */` |
|     68 | 2159 | `	n  = ph7_value_to_int(apArg[2]);` |
|     68 | 2160 | `	if( n < 0 ){` |
|      - | 2161 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|      4 | 2162 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2163 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|      1 | 2164 | `			ph7_function_name(pCtx));` |
|      - | 2165 | `	}` |
|      - | 2166 | `	/* Perform the comparison */` |
|     66 | 2167 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     66 | 2168 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     66 | 2169 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|      - | 2170 | `	/* Comparison result */` |
|     66 | 2171 | `	ph7_result_int(pCtx,res);` |
|     66 | 2172 | `	return PH7_OK;` |
|     35 | 2173 | `}` |
|      - | 2174 | `/*` |
|      - | 2175 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|      - | 2176 | ` *  Perform a binary safe case-insensitive string comparison.` |
|      - | 2177 | ` * Parameter` |
|      - | 2178 | ` *  str1: The first string` |
|      - | 2179 | ` *  str2: The second string` |
|      - | 2180 | ` * Return` |
|      - | 2181 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2182 | ` *  than str2, and 0 if they are equal.` |
|      - | 2183 | ` */` |
|    140 | 2184 | `static int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2185 | `{` |
|      - | 2186 | `	const char *z1,*z2;` |
|      - | 2187 | `	int n1,n2;` |
|      - | 2188 | `	int res;` |
|    141 | 2189 | `	if( nArg < 2 ){` |
|    ! 0 | 2190 | `		res = nArg == 0 ? 0 : 1;` |
|    ! 0 | 2191 | `		ph7_result_int(pCtx,res);` |
|    ! 0 | 2192 | `		return PH7_OK;` |
|      - | 2193 | `	}` |
|      - | 2194 | `	/* Perform the comparison */` |
|    141 | 2195 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|    141 | 2196 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|    141 | 2197 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2198 | `	/* Comparison result */` |
|    141 | 2199 | `	ph7_result_int(pCtx,res);` |
|    141 | 2200 | `	return PH7_OK;` |
|     71 | 2201 | `}` |
|      - | 2202 | `/*` |
|      - | 2203 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|      - | 2204 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|      - | 2205 | ` * Parameter` |
|      - | 2206 | ` *  $str1: The first string` |
|      - | 2207 | ` *  $str2: The second string` |
|      - | 2208 | ` *  $len:  The length of strings to be used in the comparison.` |
|      - | 2209 | ` * Return` |
|      - | 2210 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2211 | ` *  than str2, and 0 if they are equal.` |
|      - | 2212 | ` */` |
|     40 | 2213 | `static int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2214 | `{` |
|      - | 2215 | `	const char *z1,*z2;` |
|      - | 2216 | `	int res;` |
|      - | 2217 | `	int n;` |
|     45 | 2218 | `	if( nArg < 3 ){` |
|      - | 2219 | `		/* Perform a standard comparison */` |
|    ! 0 | 2220 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 2221 | `	}` |
|      - | 2222 | `	/* Desired comparison length */` |
|     45 | 2223 | `	n  = ph7_value_to_int(apArg[2]);` |
|     45 | 2224 | `	if( n < 0 ){` |
|      - | 2225 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|      4 | 2226 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2227 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|      1 | 2228 | `			ph7_function_name(pCtx));` |
|      - | 2229 | `	}` |
|      - | 2230 | `	/* Perform the comparison */` |
|     43 | 2231 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     43 | 2232 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     43 | 2233 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 2234 | `	/* Comparison result */` |
|     43 | 2235 | `	ph7_result_int(pCtx,res);` |
|     43 | 2236 | `	return PH7_OK;` |
|     25 | 2237 | `}` |
|      - | 2238 | `/*` |
|      - | 2239 | ` * Implode context [i.e: it's private data].` |
|      - | 2240 | ` * A pointer to the following structure is forwarded` |
|      - | 2241 | ` * verbatim to the array walker callback defined below.` |
|      - | 2242 | ` */` |
|      - | 2243 | `struct implode_data {` |
|      - | 2244 | `	ph7_context *pCtx;    /* Call context */` |
|      - | 2245 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|      - | 2246 | `	const char *zSep;     /* Arguments separator if any */` |
|      - | 2247 | `	int nSeplen;          /* Separator length */` |
|      - | 2248 | `	int bFirst;           /* TRUE if first call */` |
|      - | 2249 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|      - | 2250 | `	sxi32 rc;             /* Captured allocation rc; SXERR_MEM => the builtin raises an OOM fatal */` |
|      - | 2251 | `};` |
|      - | 2252 | `/*` |
|      - | 2253 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 2254 | ` * The following routine is invoked for each array entry passed` |
|      - | 2255 | ` * to the implode() function.` |
|      - | 2256 | ` */` |
| 149666 | 2257 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      5 | 2258 | `{` |
|  74833 | 2259 | `	SXUNUSED(pKey);` |
| 149671 | 2260 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 2261 | `	const char *zData;` |
|      - | 2262 | `	int nLen;` |
| 149671 | 2263 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 2264 | `		if( pData->nSeplen > 0 ){` |
|      3 | 2265 | `			if( !pData->bFirst ){` |
|      - | 2266 | `				/* append the separator first */` |
|      3 | 2267 | `				if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 2268 | `					pData->rc = SXERR_MEM;` |
|    ! 0 | 2269 | `					return PH7_ABORT;` |
|      - | 2270 | `				}` |
|      2 | 2271 | `			}else{` |
|    ! 0 | 2272 | `				pData->bFirst = 0;` |
|      - | 2273 | `			}` |
|      1 | 2274 | `		}` |
|      - | 2275 | `		/* Recurse */` |
|      3 | 2276 | `		pData->bFirst = 1;` |
|      3 | 2277 | `		pData->nRecCount++;` |
|      3 | 2278 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 2279 | `		pData->nRecCount--;` |
|      - | 2280 | `		/* Propagate an allocation failure surfaced deeper in the recursion. */` |
|      3 | 2281 | `		if( pData->rc != SXRET_OK ){` |
|    ! 0 | 2282 | `			return PH7_ABORT;` |
|      - | 2283 | `		}` |
|      3 | 2284 | `		return PH7_OK;` |
|      - | 2285 | `	}` |
|      - | 2286 | `	/* Extract the string representation of the entry value */` |
| 149669 | 2287 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 2288 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 149669 | 2289 | `	if( pData->bFirst ){` |
|  33257 | 2290 | `		pData->bFirst = 0;` |
| 133043 | 2291 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 2292 | `		/* append the separator first */` |
| 116401 | 2293 | `		if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 2294 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 2295 | `			return PH7_ABORT;` |
|      - | 2296 | `		}` |
|  58198 | 2297 | `	}` |
|      - | 2298 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 149669 | 2299 | `	if( nLen > 0 ){` |
| 137039 | 2300 | `		if( ph7_result_string(pData->pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2301 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 2302 | `			return PH7_ABORT;` |
|      - | 2303 | `		}` |
|  68517 | 2304 | `	}` |
| 149669 | 2305 | `	return PH7_OK;` |
|  74838 | 2306 | `}` |
|      - | 2307 | `/*` |
|      - | 2308 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 2309 | ` * string implode(array $pieces,...)` |
|      - | 2310 | ` *  Join array elements with a string.` |
|      - | 2311 | ` * $glue` |
|      - | 2312 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 2313 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 2314 | ` * $pieces` |
|      - | 2315 | ` *   The array of strings to implode.` |
|      - | 2316 | ` * Return` |
|      - | 2317 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 2318 | ` *  order, with the glue string between each element.` |
|      - | 2319 | ` */` |
|  33278 | 2320 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2321 | `{` |
|      - | 2322 | `	struct implode_data imp_data;` |
|  33283 | 2323 | `	int i = 1;` |
|  33283 | 2324 | `	if( nArg < 1 ){` |
|      - | 2325 | `		/* Missing argument,return NULL */` |
|    ! 0 | 2326 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2327 | `		return PH7_OK;` |
|      - | 2328 | `	}` |
|      - | 2329 | `	/* Prepare the implode context */` |
|  33283 | 2330 | `	imp_data.pCtx = pCtx;` |
|  33283 | 2331 | `	imp_data.bRecursive = 0;` |
|  33283 | 2332 | `	imp_data.bFirst = 1;` |
|  33283 | 2333 | `	imp_data.nRecCount = 0;` |
|  33283 | 2334 | `	imp_data.rc = SXRET_OK;` |
|  33283 | 2335 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  33281 | 2336 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  33281 | 2337 | `		if( nArg > 1 && !ph7_value_is_array(apArg[1]) && !ph7_value_is_null(apArg[1]) ){` |
|      - | 2338 | `			/* php: implode($glue, $pieces) requires an ARRAY. PH7 stringified whatever it` |
|      - | 2339 | `			 * was handed, so implode(",", 5) quietly returned "5". */` |
|      - | 2340 | `			char zBuf[64];` |
|      4 | 2341 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 2342 | `				"implode(): Argument #2 ($array) must be of type ?array, %s given",` |
|      2 | 2343 | `				VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 2344 | `		}` |
|  16642 | 2345 | `	}else{` |
|      3 | 2346 | `		imp_data.zSep = 0;` |
|      3 | 2347 | `		imp_data.nSeplen = 0;` |
|      3 | 2348 | `		i = 0;` |
|      - | 2349 | `	}` |
|  33281 | 2350 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 2351 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2352 | `	}` |
|      - | 2353 | `	/* Start the 'join' process */` |
|  66557 | 2354 | `	while( i < nArg ){` |
|  33281 | 2355 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2356 | `			/* Iterate throw array entries */` |
|  33281 | 2357 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 2358 | `			/* Surface a callback allocation failure as a fatal */` |
|  33281 | 2359 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 2360 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2361 | `			}` |
|  16643 | 2362 | `		}else{` |
|      - | 2363 | `			const char *zData;` |
|      - | 2364 | `			int nLen;` |
|      - | 2365 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 2366 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2367 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 2368 | `			if( imp_data.bFirst ){` |
|    ! 0 | 2369 | `				imp_data.bFirst = 0;` |
|    ! 0 | 2370 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2371 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 2372 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2373 | `				}` |
|    ! 0 | 2374 | `			}` |
|      - | 2375 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 2376 | `			if( nLen > 0 ){` |
|    ! 0 | 2377 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2378 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2379 | `				}` |
|    ! 0 | 2380 | `			}` |
|      - | 2381 | `		}` |
|  33281 | 2382 | `		i++;` |
|      5 | 2383 | `	}` |
|  33281 | 2384 | `	return PH7_OK;` |
|  16644 | 2385 | `}` |
|      - | 2386 | `/*` |
|      - | 2387 | ` * Symisc eXtension:` |
|      - | 2388 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 2389 | ` * Purpose` |
|      - | 2390 | ` *  Same as implode() but recurse on arrays.` |
|      - | 2391 | ` * Example:` |
|      - | 2392 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 2393 | ` *   echo implode_recursive("/",$a);` |
|      - | 2394 | ` *   Will output` |
|      - | 2395 | ` *     usr/home/dean.` |
|      - | 2396 | ` *   While the standard implode would produce.` |
|      - | 2397 | ` *    usr/Array.` |
|      - | 2398 | ` * Parameter` |
|      - | 2399 | ` *  Refer to implode().` |
|      - | 2400 | ` * Return` |
|      - | 2401 | ` *  Refer to implode().` |
|      - | 2402 | ` */` |
|     12 | 2403 | `static int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2404 | `{` |
|      - | 2405 | `	struct implode_data imp_data;` |
|     13 | 2406 | `	int i = 1;` |
|     13 | 2407 | `	if( nArg < 1 ){` |
|      - | 2408 | `		/* Missing argument,return NULL */` |
|      3 | 2409 | `		ph7_result_null(pCtx);` |
|      3 | 2410 | `		return PH7_OK;` |
|      - | 2411 | `	}` |
|      - | 2412 | `	/* Prepare the implode context */` |
|     11 | 2413 | `	imp_data.pCtx = pCtx;` |
|     11 | 2414 | `	imp_data.bRecursive = 1;` |
|     11 | 2415 | `	imp_data.bFirst = 1;` |
|     11 | 2416 | `	imp_data.nRecCount = 0;` |
|     11 | 2417 | `	imp_data.rc = SXRET_OK;` |
|     11 | 2418 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 2419 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 2420 | `	}else{` |
|    ! 0 | 2421 | `		imp_data.zSep = 0;` |
|    ! 0 | 2422 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 2423 | `		i = 0;` |
|      - | 2424 | `	}` |
|     11 | 2425 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 2426 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2427 | `	}` |
|      - | 2428 | `	/* Start the 'join' process */` |
|     21 | 2429 | `	while( i < nArg ){` |
|     11 | 2430 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2431 | `			/* Iterate throw array entries */` |
|      3 | 2432 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 2433 | `			/* Surface a callback allocation failure as a fatal */` |
|      3 | 2434 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 2435 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2436 | `			}` |
|      2 | 2437 | `		}else{` |
|      - | 2438 | `			const char *zData;` |
|      - | 2439 | `			int nLen;` |
|      - | 2440 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 2441 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2442 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 2443 | `			if( imp_data.bFirst ){` |
|      9 | 2444 | `				imp_data.bFirst = 0;` |
|      4 | 2445 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2446 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 2447 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2448 | `				}` |
|    ! 0 | 2449 | `			}` |
|      - | 2450 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 2451 | `			if( nLen > 0 ){` |
|      9 | 2452 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2453 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2454 | `				}` |
|      4 | 2455 | `			}` |
|      - | 2456 | `		}` |
|     11 | 2457 | `		i++;` |
|      1 | 2458 | `	}` |
|     11 | 2459 | `	return PH7_OK;` |
|      7 | 2460 | `}` |
|      - | 2461 | `/*` |
|      - | 2462 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 2463 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 2464 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 2465 | ` * Parameters` |
|      - | 2466 | ` *  $delimiter` |
|      - | 2467 | ` *   The boundary string.` |
|      - | 2468 | ` * $string` |
|      - | 2469 | ` *   The input string.` |
|      - | 2470 | ` * $limit` |
|      - | 2471 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 2472 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 2473 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 2474 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 2475 | ` * Returns` |
|      - | 2476 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 2477 | ` *  on boundaries formed by the delimiter.` |
|      - | 2478 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 2479 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 2480 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 2481 | ` *  will be returned.` |
|      - | 2482 | ` * NOTE:` |
|      - | 2483 | ` *  Negative limit is not supported.` |
|      - | 2484 | ` */` |
|   6606 | 2485 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2486 | `{` |
|      - | 2487 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 2488 | `	int nDelim,nStrlen,iLimit;` |
|      - | 2489 | `	ph7_value *pArray;` |
|      - | 2490 | `	ph7_value *pValue;` |
|      - | 2491 | `	sxu32 nOfft;` |
|      - | 2492 | `	sxi32 rc;` |
|   6611 | 2493 | `	if( nArg < 2 ){` |
|      - | 2494 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2495 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2496 | `		return PH7_OK;` |
|      - | 2497 | `	}` |
|      - | 2498 | `	/* Extract the delimiter */` |
|   6611 | 2499 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   6611 | 2500 | `	if( nDelim < 1 ){` |
|      - | 2501 | `		/* Empty delimiter: PHP 8 throws a catchable ValueError. */` |
|      5 | 2502 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2503 | `			"explode(): Argument #1 ($separator) must not be empty");` |
|      - | 2504 | `	}` |
|      - | 2505 | `	/* Extract the string */` |
|   6607 | 2506 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   6607 | 2507 | `	if( nStrlen < 1 ){` |
|      - | 2508 | `		/* Empty string: normally an array with a single empty element (PHP behavior).` |
|      - | 2509 | `		 * A negative limit drops the last -limit components, so the sole empty` |
|      - | 2510 | `		 * component is dropped and the result is an empty array. */` |
|      7 | 2511 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|      7 | 2512 | `		if( pArrayTmp == 0 ){` |
|      - | 2513 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 2514 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2515 | `			return PH7_OK;` |
|      - | 2516 | `		}` |
|      7 | 2517 | `		if( !(nArg > 2 && ph7_value_to_int(apArg[2]) < 0) ){` |
|      5 | 2518 | `			ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|      5 | 2519 | `			if( pValueTmp == 0 ){` |
|      - | 2520 | `				/* Out of memory,return FALSE */` |
|    ! 0 | 2521 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 2522 | `				return PH7_OK;` |
|      - | 2523 | `			}` |
|      5 | 2524 | `			ph7_value_string(pValueTmp, "", 0);` |
|      5 | 2525 | `			if( ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp) != SXRET_OK ){` |
|    ! 0 | 2526 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2527 | `			}` |
|      2 | 2528 | `		}` |
|      7 | 2529 | `		ph7_result_value(pCtx, pArrayTmp);` |
|      7 | 2530 | `		return PH7_OK;` |
|      - | 2531 | `	}` |
|      - | 2532 | `	/* Point to the end of the string */` |
|   6601 | 2533 | `	zEnd = &zString[nStrlen];` |
|      - | 2534 | `	/* Create the array */` |
|   6601 | 2535 | `	pArray =  ph7_context_new_array(pCtx);` |
|   6601 | 2536 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   6601 | 2537 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 2538 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 2539 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2540 | `		return PH7_OK;` |
|      - | 2541 | `	}` |
|      - | 2542 | `	/* Set a defualt limit */` |
|   6601 | 2543 | `	iLimit = SXI32_HIGH;` |
|   6601 | 2544 | `	if( nArg > 2 ){` |
|     38 | 2545 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|     38 | 2546 | `		if( iLimit < 0 ){` |
|      - | 2547 | `			/* Negative limit: keep all components except the last -iLimit (PHP).` |
|      - | 2548 | `			 * Pre-count the components (delimiters + 1), then emit only the first` |
|      - | 2549 | `			 * nKeep CLEAN components — no trailing-remainder merge (the difference` |
|      - | 2550 | `			 * from the positive path). nKeep <= 0 drops everything -> empty array. */` |
|     17 | 2551 | `			int nTotal = 1,nKeep;` |
|     17 | 2552 | `			const char *zScan = zString;` |
|      - | 2553 | `			sxu32 nScanOfft;` |
|     57 | 2554 | `			while( SyBlobSearch(zScan,(sxu32)(zEnd - zScan),zDelim,nDelim,&nScanOfft) == SXRET_OK ){` |
|     41 | 2555 | `				nTotal++;` |
|     41 | 2556 | `				zScan = &zScan[nScanOfft + nDelim];` |
|      1 | 2557 | `			}` |
|     17 | 2558 | `			nKeep = nTotal + iLimit; /* iLimit < 0, so this is nTotal - (-iLimit) */` |
|     49 | 2559 | `			while( nKeep > (int)ph7_array_count(pArray)` |
|     39 | 2560 | `				&& SyBlobSearch(zString,(sxu32)(zEnd - zString),zDelim,nDelim,&nOfft) == SXRET_OK ){` |
|      - | 2561 | `				/* Emit the next clean component */` |
|     23 | 2562 | `				zCur = &zString[nOfft];` |
|     23 | 2563 | `				ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|     23 | 2564 | `				if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2565 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2566 | `				}` |
|     23 | 2567 | `				zString = &zCur[nDelim];` |
|     23 | 2568 | `				ph7_value_reset_string_cursor(pValue);` |
|      1 | 2569 | `			}` |
|     17 | 2570 | `			ph7_result_value(pCtx,pArray);` |
|     17 | 2571 | `			return PH7_OK;` |
|      - | 2572 | `		}` |
|     22 | 2573 | `		if( iLimit == 0 ){` |
|      5 | 2574 | `			iLimit = 1;` |
|      2 | 2575 | `		}` |
|     22 | 2576 | `		iLimit--;` |
|      9 | 2577 | `	}` |
|      - | 2578 | `	/* Start exploding */` |
|  80627 | 2579 | `	for(;;){` |
| 161259 | 2580 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 161259 | 2581 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 2582 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   6585 | 2583 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   6585 | 2584 | `			if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2585 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2586 | `			}` |
|   6585 | 2587 | `			break;` |
|      - | 2588 | `		}` |
|      - | 2589 | `		/* Point to the desired offset */` |
| 154679 | 2590 | `		zCur = &zString[nOfft];` |
|      - | 2591 | `		/* Perform the store operation (may be empty) */` |
| 154679 | 2592 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 154679 | 2593 | `		if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2594 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 2595 | `		}` |
|      - | 2596 | `		/* Point beyond the delimiter */` |
| 154679 | 2597 | `		zString = &zCur[nDelim];` |
|      - | 2598 | `		/* Reset the cursor */` |
| 154679 | 2599 | `		ph7_value_reset_string_cursor(pValue);` |
|      5 | 2600 | `	}` |
|      - | 2601 | `	/* Return the freshly created array */` |
|   6585 | 2602 | `	ph7_result_value(pCtx,pArray);` |
|      - | 2603 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 2604 | `	 * released as soon we return from this foregin function.` |
|      - | 2605 | `	 */` |
|   6585 | 2606 | `	return PH7_OK;` |
|   3308 | 2607 | `}` |
|      - | 2608 | `/*` |
|      - | 2609 | ` * string trim(string $str[,string $charlist ])` |
|      - | 2610 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2611 | ` * Parameters` |
|      - | 2612 | ` *  $str` |
|      - | 2613 | ` *   The string that will be trimmed.` |
|      - | 2614 | ` * $charlist` |
|      - | 2615 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2616 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2617 | ` *   With .. you can specify a range of characters.` |
|      - | 2618 | ` * Returns.` |
|      - | 2619 | ` *  Thr processed string.` |
|      - | 2620 | ` * NOTE:` |
|      - | 2621 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2622 | ` */` |
|  14360 | 2623 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2624 | `{` |
|  14365 | 2625 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"trim",1,"$string"); }` |
|      - | 2626 | `	const char *zString;` |
|      - | 2627 | `	int nLen;` |
|  14365 | 2628 | `	if( nArg < 1 ){` |
|      - | 2629 | `		/* Missing arguments,return null */` |
|    ! 0 | 2630 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2631 | `		return PH7_OK;` |
|      - | 2632 | `	}` |
|      - | 2633 | `	/* Extract the target string */` |
|  14365 | 2634 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  14365 | 2635 | `	if( nLen < 1 ){` |
|      - | 2636 | `		/* Empty string,return */` |
|    755 | 2637 | `		ph7_result_string(pCtx,"",0);` |
|    755 | 2638 | `		return PH7_OK;` |
|      - | 2639 | `	}` |
|      - | 2640 | `	/* Start the trim process */` |
|  13615 | 2641 | `	if( nArg < 2 ){` |
|      - | 2642 | `		SyString sStr;` |
|      - | 2643 | `		/* Remove white spaces and NUL bytes */` |
|  13585 | 2644 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  34071 | 2645 | `		SyStringFullTrimSafe(&sStr);` |
|  13585 | 2646 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   6795 | 2647 | `	}else{` |
|      - | 2648 | `		/* Char list */` |
|      - | 2649 | `		const char *zList;` |
|      - | 2650 | `		int nListlen;` |
|     33 | 2651 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     33 | 2652 | `		if( nListlen < 1 ){` |
|      - | 2653 | `			/* Return the string unchanged */` |
|      6 | 2654 | `			ph7_result_string(pCtx,zString,nLen);` |
|      4 | 2655 | `		}else{` |
|      - | 2656 | `			char aMask[256];` |
|     29 | 2657 | `			const char *zEnd = &zString[nLen];` |
|     29 | 2658 | `			const char *zCur = zString;` |
|     29 | 2659 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2660 | `			/* Left trim */` |
|     79 | 2661 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     53 | 2662 | `				zCur++;` |
|      3 | 2663 | `			}` |
|      - | 2664 | `			/* Right trim */` |
|     79 | 2665 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     53 | 2666 | `				zEnd--;` |
|      3 | 2667 | `			}` |
|     29 | 2668 | `			if( zCur >= zEnd ){` |
|      - | 2669 | `				/* Return the empty string */` |
|    ! 0 | 2670 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2671 | `			}else{` |
|     29 | 2672 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2673 | `			}` |
|      - | 2674 | `		}` |
|      - | 2675 | `	}` |
|  13615 | 2676 | `	return PH7_OK;` |
|   7185 | 2677 | `}` |
|      - | 2678 | `/*` |
|      - | 2679 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 2680 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 2681 | ` * Parameters` |
|      - | 2682 | ` *  $str` |
|      - | 2683 | ` *   The string that will be trimmed.` |
|      - | 2684 | ` * $charlist` |
|      - | 2685 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2686 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2687 | ` *   With .. you can specify a range of characters.` |
|      - | 2688 | ` * Returns.` |
|      - | 2689 | ` *  Thr processed string.` |
|      - | 2690 | ` * NOTE:` |
|      - | 2691 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2692 | ` */` |
|    162 | 2693 | `static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2694 | `{` |
|    166 | 2695 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"rtrim",1,"$string"); }` |
|      - | 2696 | `	const char *zString;` |
|      - | 2697 | `	int nLen;` |
|    166 | 2698 | `	if( nArg < 1 ){` |
|      - | 2699 | `		/* Missing arguments,return null */` |
|    ! 0 | 2700 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2701 | `		return PH7_OK;` |
|      - | 2702 | `	}` |
|      - | 2703 | `	/* Extract the target string */` |
|    166 | 2704 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    166 | 2705 | `	if( nLen < 1 ){` |
|      - | 2706 | `		/* Empty string,return */` |
|      7 | 2707 | `		ph7_result_string(pCtx,"",0);` |
|      7 | 2708 | `		return PH7_OK;` |
|      - | 2709 | `	}` |
|      - | 2710 | `	/* Start the trim process */` |
|    160 | 2711 | `	if( nArg < 2 ){` |
|      - | 2712 | `		SyString sStr;` |
|      - | 2713 | `		/* Remove white spaces and NUL bytes*/` |
|     19 | 2714 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     48 | 2715 | `		SyStringRightTrimSafe(&sStr);` |
|     19 | 2716 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|     10 | 2717 | `	}else{` |
|      - | 2718 | `		/* Char list */` |
|      - | 2719 | `		const char *zList;` |
|      - | 2720 | `		int nListlen;` |
|    142 | 2721 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|    142 | 2722 | `		if( nListlen < 1 ){` |
|      - | 2723 | `			/* Return the string unchanged */` |
|    ! 0 | 2724 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 2725 | `		}else{` |
|      - | 2726 | `			char aMask[256];` |
|    142 | 2727 | `			const char *zEnd = &zString[nLen];` |
|    142 | 2728 | `			const char *zCur = zString;` |
|    142 | 2729 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2730 | `			/* Right trim */` |
|    160 | 2731 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     22 | 2732 | `				zEnd--;` |
|      4 | 2733 | `			}` |
|    142 | 2734 | `			if( zEnd <= zCur ){` |
|      - | 2735 | `				/* Return the empty string */` |
|    ! 0 | 2736 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2737 | `			}else{` |
|    142 | 2738 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2739 | `			}` |
|      - | 2740 | `		}` |
|      - | 2741 | `	}` |
|    160 | 2742 | `	return PH7_OK;` |
|     85 | 2743 | `}` |
|      - | 2744 | `/*` |
|      - | 2745 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 2746 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2747 | ` * Parameters` |
|      - | 2748 | ` *  $str` |
|      - | 2749 | ` *   The string that will be trimmed.` |
|      - | 2750 | ` * $charlist` |
|      - | 2751 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2752 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2753 | ` *   With .. you can specify a range of characters.` |
|      - | 2754 | ` * Returns.` |
|      - | 2755 | ` *  Thr processed string.` |
|      - | 2756 | ` * NOTE:` |
|      - | 2757 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2758 | ` */` |
|     42 | 2759 | `static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2760 | `{` |
|     47 | 2761 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"ltrim",1,"$string"); }` |
|      - | 2762 | `	const char *zString;` |
|      - | 2763 | `	int nLen;` |
|     47 | 2764 | `	if( nArg < 1 ){` |
|      - | 2765 | `		/* Missing arguments,return null */` |
|    ! 0 | 2766 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2767 | `		return PH7_OK;` |
|      - | 2768 | `	}` |
|      - | 2769 | `	/* Extract the target string */` |
|     47 | 2770 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     47 | 2771 | `	if( nLen < 1 ){` |
|      - | 2772 | `		/* Empty string,return */` |
|     23 | 2773 | `		ph7_result_string(pCtx,"",0);` |
|     23 | 2774 | `		return PH7_OK;` |
|      - | 2775 | `	}` |
|      - | 2776 | `	/* Start the trim process */` |
|     29 | 2777 | `	if( nArg < 2 ){` |
|      - | 2778 | `		SyString sStr;` |
|      - | 2779 | `		/* Remove white spaces and NUL byte */` |
|      5 | 2780 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     13 | 2781 | `		SyStringLeftTrimSafe(&sStr);` |
|      5 | 2782 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      3 | 2783 | `	}else{` |
|      - | 2784 | `		/* Char list */` |
|      - | 2785 | `		const char *zList;` |
|      - | 2786 | `		int nListlen;` |
|     25 | 2787 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     25 | 2788 | `		if( nListlen < 1 ){` |
|      - | 2789 | `			/* Return the string unchanged */` |
|      3 | 2790 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2791 | `		}else{` |
|      - | 2792 | `			char aMask[256];` |
|     23 | 2793 | `			const char *zEnd = &zString[nLen];` |
|     23 | 2794 | `			const char *zCur = zString;` |
|     23 | 2795 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2796 | `			/* Left trim */` |
|     57 | 2797 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     39 | 2798 | `				zCur++;` |
|      5 | 2799 | `			}` |
|     23 | 2800 | `			if( zCur >= zEnd ){` |
|      - | 2801 | `				/* Return the empty string */` |
|    ! 0 | 2802 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2803 | `			}else{` |
|     23 | 2804 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2805 | `			}` |
|      - | 2806 | `		}` |
|      - | 2807 | `	}` |
|     29 | 2808 | `	return PH7_OK;` |
|     26 | 2809 | `}` |
|      - | 2810 | `/*` |
|      - | 2811 | ` * string strtolower(string $str)` |
|      - | 2812 | ` *  Make a string lowercase.` |
|      - | 2813 | ` * Parameters` |
|      - | 2814 | ` *  $str` |
|      - | 2815 | ` *   The input string.` |
|      - | 2816 | ` * Returns.` |
|      - | 2817 | ` *  The lowercased string.` |
|      - | 2818 | ` */` |
|  33244 | 2819 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2820 | `{` |
|  33249 | 2821 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtolower",1,"$string"); }` |
|      - | 2822 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2823 | `	int nLen;` |
|  33249 | 2824 | `	if( nArg < 1 ){` |
|      - | 2825 | `		/* Missing arguments,return null */` |
|    ! 0 | 2826 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2827 | `		return PH7_OK;` |
|      - | 2828 | `	}` |
|      - | 2829 | `	/* Extract the target string */` |
|  33249 | 2830 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  33249 | 2831 | `	if( nLen < 1 ){` |
|      - | 2832 | `		/* Empty string,return */` |
|      5 | 2833 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 2834 | `		return PH7_OK;` |
|      - | 2835 | `	}` |
|      - | 2836 | `	/* Perform the requested operation */` |
|  33245 | 2837 | `	zEnd = &zString[nLen];` |
| 104837 | 2838 | `	for(;;){` |
| 209679 | 2839 | `		if( zString >= zEnd ){` |
|      - | 2840 | `			/* No more input,break immediately */` |
|  33245 | 2841 | `			break;` |
|      - | 2842 | `		}` |
| 176439 | 2843 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2844 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2845 | `			zCur = zString;` |
|    ! 0 | 2846 | `			zString++;` |
|    ! 0 | 2847 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2848 | `				zString++;` |
|    ! 0 | 2849 | `			}` |
|      - | 2850 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2851 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2852 | `		}else{` |
| 176439 | 2853 | `			int c = zString[0];` |
| 176439 | 2854 | `			if( SyisUpper(c) ){` |
| 173883 | 2855 | `				c = SyToLower(zString[0]);` |
|  86939 | 2856 | `			}` |
|      - | 2857 | `			/* Append character */` |
| 176439 | 2858 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2859 | `			/* Advance the cursor */` |
| 176439 | 2860 | `			zString++;` |
|      - | 2861 | `		}` |
|      5 | 2862 | `	}` |
|  33245 | 2863 | `	return PH7_OK;` |
|  16627 | 2864 | `}` |
|      - | 2865 | `/*` |
|      - | 2866 | ` * string strtolower(string $str)` |
|      - | 2867 | ` *  Make a string uppercase.` |
|      - | 2868 | ` * Parameters` |
|      - | 2869 | ` *  $str` |
|      - | 2870 | ` *   The input string.` |
|      - | 2871 | ` * Returns.` |
|      - | 2872 | ` *  The uppercased string.` |
|      - | 2873 | ` */` |
|     70 | 2874 | `static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2875 | `{` |
|     75 | 2876 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtoupper",1,"$string"); }` |
|      - | 2877 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2878 | `	int nLen;` |
|     75 | 2879 | `	if( nArg < 1 ){` |
|      - | 2880 | `		/* Missing arguments,return null */` |
|    ! 0 | 2881 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2882 | `		return PH7_OK;` |
|      - | 2883 | `	}` |
|      - | 2884 | `	/* Extract the target string */` |
|     75 | 2885 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     75 | 2886 | `	if( nLen < 1 ){` |
|      - | 2887 | `		/* Empty string,return */` |
|      5 | 2888 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 2889 | `		return PH7_OK;` |
|      - | 2890 | `	}` |
|      - | 2891 | `	/* Perform the requested operation */` |
|     71 | 2892 | `	zEnd = &zString[nLen];` |
|    139 | 2893 | `	for(;;){` |
|    283 | 2894 | `		if( zString >= zEnd ){` |
|      - | 2895 | `			/* No more input,break immediately */` |
|     71 | 2896 | `			break;` |
|      - | 2897 | `		}` |
|    217 | 2898 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2899 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2900 | `			zCur = zString;` |
|    ! 0 | 2901 | `			zString++;` |
|    ! 0 | 2902 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2903 | `				zString++;` |
|    ! 0 | 2904 | `			}` |
|      - | 2905 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2906 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2907 | `		}else{` |
|    217 | 2908 | `			int c = zString[0];` |
|    217 | 2909 | `			if( SyisLower(c) ){` |
|    204 | 2910 | `				c = SyToUpper(zString[0]);` |
|    100 | 2911 | `			}` |
|      - | 2912 | `			/* Append character */` |
|    217 | 2913 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2914 | `			/* Advance the cursor */` |
|    217 | 2915 | `			zString++;` |
|      - | 2916 | `		}` |
|      5 | 2917 | `	}` |
|     71 | 2918 | `	return PH7_OK;` |
|     40 | 2919 | `}` |
|      - | 2920 | `/*` |
|      - | 2921 | ` * string ucfirst(string $str)` |
|      - | 2922 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 2923 | ` *  character is alphabetic.` |
|      - | 2924 | ` * Parameters` |
|      - | 2925 | ` *  $str` |
|      - | 2926 | ` *   The input string.` |
|      - | 2927 | ` * Returns.` |
|      - | 2928 | ` *  The processed string.` |
|      - | 2929 | ` */` |
|      4 | 2930 | `static int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2931 | `{` |
|      - | 2932 | `	const char *zString,*zEnd;` |
|      - | 2933 | `	int nLen,c;` |
|      5 | 2934 | `	if( nArg < 1 ){` |
|      - | 2935 | `		/* Missing arguments,return null */` |
|    ! 0 | 2936 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2937 | `		return PH7_OK;` |
|      - | 2938 | `	}` |
|      - | 2939 | `	/* Extract the target string */` |
|      5 | 2940 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2941 | `	if( nLen < 1 ){` |
|      - | 2942 | `		/* Empty string,return */` |
|      3 | 2943 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2944 | `		return PH7_OK;` |
|      - | 2945 | `	}` |
|      - | 2946 | `	/* Perform the requested operation */` |
|      3 | 2947 | `	zEnd = &zString[nLen];` |
|      3 | 2948 | `	c = zString[0];` |
|      3 | 2949 | `	if( SyisLower(c) ){` |
|      3 | 2950 | `		c = SyToUpper(c);` |
|      1 | 2951 | `	}` |
|      - | 2952 | `	/* Append the first character */` |
|      3 | 2953 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2954 | `	zString++;` |
|      3 | 2955 | `	if( zString < zEnd ){` |
|      - | 2956 | `		/* Append the rest of the input verbatim */` |
|      3 | 2957 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2958 | `	}` |
|      3 | 2959 | `	return PH7_OK;` |
|      3 | 2960 | `}` |
|      - | 2961 | `/*` |
|      - | 2962 | ` * string lcfirst(string $str)` |
|      - | 2963 | ` *  Make a string's first character lowercase.` |
|      - | 2964 | ` * Parameters` |
|      - | 2965 | ` *  $str` |
|      - | 2966 | ` *   The input string.` |
|      - | 2967 | ` * Returns.` |
|      - | 2968 | ` *  The processed string.` |
|      - | 2969 | ` */` |
|      4 | 2970 | `static int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2971 | `{` |
|      - | 2972 | `	const char *zString,*zEnd;` |
|      - | 2973 | `	int nLen,c;` |
|      5 | 2974 | `	if( nArg < 1 ){` |
|      - | 2975 | `		/* Missing arguments,return null */` |
|    ! 0 | 2976 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2977 | `		return PH7_OK;` |
|      - | 2978 | `	}` |
|      - | 2979 | `	/* Extract the target string */` |
|      5 | 2980 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2981 | `	if( nLen < 1 ){` |
|      - | 2982 | `		/* Empty string,return */` |
|      3 | 2983 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2984 | `		return PH7_OK;` |
|      - | 2985 | `	}` |
|      - | 2986 | `	/* Perform the requested operation */` |
|      3 | 2987 | `	zEnd = &zString[nLen];` |
|      3 | 2988 | `	c = zString[0];` |
|      3 | 2989 | `	if( SyisUpper(c) ){` |
|      3 | 2990 | `		c = SyToLower(c);` |
|      1 | 2991 | `	}` |
|      - | 2992 | `	/* Append the first character */` |
|      3 | 2993 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2994 | `	zString++;` |
|      3 | 2995 | `	if( zString < zEnd ){` |
|      - | 2996 | `		/* Append the rest of the input verbatim */` |
|      3 | 2997 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2998 | `	}` |
|      3 | 2999 | `	return PH7_OK;` |
|      3 | 3000 | `}` |
|      - | 3001 | `/*` |
|      - | 3002 | ` * int ord(string $string)` |
|      - | 3003 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 3004 | ` *  Passing null, an empty string, or a multi-byte string emits` |
|      - | 3005 | ` *  E_DEPRECATED to match PHP 8.4+ behaviour.` |
|      - | 3006 | ` * Parameters` |
|      - | 3007 | ` *  $string` |
|      - | 3008 | ` *   The input string.` |
|      - | 3009 | ` * Returns` |
|      - | 3010 | ` *  The ASCII value as an integer.` |
|      - | 3011 | ` */` |
|    182 | 3012 | `static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3013 | `{` |
|      - | 3014 | `	const char *zString;` |
|      - | 3015 | `	int nLen,c;` |
|      - | 3016 | `	/* PHP requires exactly one argument. */` |
|    185 | 3017 | `	if( nArg != 1 ){` |
|      4 | 3018 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3019 | `			"ArgumentCountError",` |
|      - | 3020 | `			"ord() expects exactly 1 argument, %d given",` |
|      1 | 3021 | `			nArg` |
|      - | 3022 | `			);` |
|      - | 3023 | `	}` |
|      - | 3024 | `	/* Passing null is deprecated (E_DEPRECATED).  PHP emits this before` |
|      - | 3025 | `	 * the empty-string deprecation, so we check null first. */` |
|    182 | 3026 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 3027 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 3028 | `			"ord(): Passing null to parameter #1 ($character) "` |
|      - | 3029 | `			"of type string is deprecated"` |
|      - | 3030 | `			);` |
|      1 | 3031 | `	}` |
|      - | 3032 | `	/* Extract the target string */` |
|    182 | 3033 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    182 | 3034 | `	if( nLen < 1 ){` |
|      - | 3035 | `		/* Empty string is deprecated (E_DEPRECATED). */` |
|      5 | 3036 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 3037 | `			"ord(): Providing an empty string is deprecated"` |
|      - | 3038 | `			);` |
|      5 | 3039 | `		ph7_result_int(pCtx,0);` |
|      5 | 3040 | `		return PH7_OK;` |
|      - | 3041 | `	}` |
|      - | 3042 | `	/* A string longer than one byte is deprecated (E_DEPRECATED). */` |
|    178 | 3043 | `	if( nLen > 1 ){` |
|      7 | 3044 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 3045 | `			"ord(): Providing a string that is not one byte long "` |
|      - | 3046 | `			"is deprecated. Use ord($str[0]) instead"` |
|      - | 3047 | `			);` |
|      3 | 3048 | `	}` |
|      - | 3049 | `	/* Extract the ASCII value of the first character */` |
|    178 | 3050 | `	c = (unsigned char)zString[0];` |
|      - | 3051 | `	/* Return that value */` |
|    178 | 3052 | `	ph7_result_int(pCtx,c);` |
|    178 | 3053 | `	return PH7_OK;` |
|     94 | 3054 | `}` |
|      - | 3055 | `/*` |
|      - | 3056 | ` * string chr(int $codepoint)` |
|      - | 3057 | ` *  Returns a one-character string containing the character specified` |
|      - | 3058 | ` *  by the given codepoint.  Any integer is accepted; values outside` |
|      - | 3059 | ` *  the [0, 255] range emit an E_DEPRECATED and are masked with & 0xFF.` |
|      - | 3060 | ` * Parameters` |
|      - | 3061 | ` *  $codepoint` |
|      - | 3062 | ` *   An integer codepoint.  Values outside 0-255 are deprecated and` |
|      - | 3063 | ` *   will be constrained to a single byte.` |
|      - | 3064 | ` * Returns` |
|      - | 3065 | ` *  A single-character string.` |
|      - | 3066 | ` */` |
|   7134 | 3067 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 3068 | `{` |
|      - | 3069 | `	int c;` |
|      - | 3070 | `	unsigned char ch;` |
|      - | 3071 | `	/* PHP requires exactly one argument. */` |
|   7138 | 3072 | `	if( nArg != 1 ){` |
|      4 | 3073 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3074 | `			"ArgumentCountError",` |
|      - | 3075 | `			"chr() expects exactly 1 argument, %d given",` |
|      1 | 3076 | `			nArg` |
|      - | 3077 | `			);` |
|      - | 3078 | `	}` |
|      - | 3079 | `	/* Implicit float-to-int conversion loses precision (E_DEPRECATED).` |
|      - | 3080 | `	 * PHP does not prefix this message with "chr():", so we call` |
|      - | 3081 | `	 * PH7_VmThrowError() with a NULL function name to avoid the` |
|      - | 3082 | `	 * automatic prefix that ph7_context_throw_error*() would add. */` |
|   7135 | 3083 | `	if( ph7_value_is_float(apArg[0]) ){` |
|      - | 3084 | `		char zBuf[120];` |
|      4 | 3085 | `		SyBufferFormat(zBuf,sizeof(zBuf),` |
|      - | 3086 | `			"Implicit conversion from float %g to int loses precision",` |
|      1 | 3087 | `			ph7_value_to_double(apArg[0])` |
|      - | 3088 | `			);` |
|      3 | 3089 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zBuf);` |
|      1 | 3090 | `	}` |
|      - | 3091 | `	/* Extract the codepoint. */` |
|   7135 | 3092 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 3093 | `	/* Out-of-range codepoint (E_DEPRECATED), then mask to a single byte.` |
|      - | 3094 | `	 * PHP includes "chr(): " in the $errstr passed to set_error_handler,` |
|      - | 3095 | `	 * so we embed the prefix in the message and pass NULL as the function` |
|      - | 3096 | `	 * name to avoid the API double-prefixing it. */` |
|   7135 | 3097 | `	if( c < 0 \|\| c > 255 ){` |
|      5 | 3098 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 3099 | `			E_DEPRECATED,` |
|      - | 3100 | `			"chr(): Providing a value not in-between 0 and 255 is deprecated, "` |
|      - | 3101 | `			"this is because a byte value must be in the [0, 255] interval. "` |
|      - | 3102 | `			"The value used will be constrained using % 256"` |
|      - | 3103 | `			);` |
|      2 | 3104 | `	}` |
|      - | 3105 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|      - | 3106 | `	 * when taking the address of a wider int. */` |
|   7135 | 3107 | `	ch = (unsigned char)(c & 0xFF);` |
|      - | 3108 | `	/* Return the specified character */` |
|   7135 | 3109 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|   7135 | 3110 | `	return PH7_OK;` |
|   3571 | 3111 | `}` |
|      - | 3112 | `/*` |
|      - | 3113 | ` * Binary to hex consumer callback.` |
|      - | 3114 | ` * This callback is the default consumer used by the hash functions` |
|      - | 3115 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 3116 | ` */` |
|   3128 | 3117 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      3 | 3118 | `{` |
|      - | 3119 | `	/* Append hex chunk verbatim */` |
|   3131 | 3120 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   3131 | 3121 | `	return SXRET_OK;` |
|      3 | 3122 | `}` |
|      - | 3123 |  |
|      - | 3124 | `/*` |
|      - | 3125 | ` * string bin2hex(string $str)` |
|      - | 3126 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 3127 | ` * Parameters` |
|      - | 3128 | ` *  $str` |
|      - | 3129 | ` *   The input string.` |
|      - | 3130 | ` * Returns.` |
|      - | 3131 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 3132 | ` */` |
|    132 | 3133 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 3134 | `{` |
|      - | 3135 | `	const char *zString;` |
|      - | 3136 | `	int nLen;` |
|      - | 3137 | `	/* PHP 8 requires exactly one argument (ArgumentCountError). */` |
|    136 | 3138 | `	if( nArg != 1 ){` |
|      4 | 3139 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3140 | `			"ArgumentCountError",` |
|      - | 3141 | `			"bin2hex() expects exactly 1 argument, %d given",` |
|      1 | 3142 | `			nArg` |
|      - | 3143 | `			);` |
|      - | 3144 | `	}` |
|      - | 3145 | `	/* In PHP 8, bin2hex() is strict about its parameter type.` |
|      - | 3146 | `	 * Array/Resource values are not allowed and trigger a TypeError.` |
|      - | 3147 | `	 * Objects without __toString() must also raise a TypeError.` |
|      - | 3148 | `	 */` |
|    198 | 3149 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) \|\|` |
|     65 | 3150 | `		( ph7_value_is_object(apArg[0]) &&` |
|    ! 0 | 3151 | `		  ((ph7_class_instance *)apArg[0]->x.pOther) != 0 &&` |
|    ! 0 | 3152 | `		  PH7_ClassExtractMethod(((ph7_class_instance *)apArg[0]->x.pOther)->pClass,` |
|    ! 0 | 3153 | `			"__toString",sizeof("__toString")-1) == 0` |
|      - | 3154 | `		)` |
|      - | 3155 | `	){` |
|    ! 0 | 3156 | `		const char *zType = ph7_type_name(apArg[0]);` |
|    ! 0 | 3157 | `		if( ph7_value_is_object(apArg[0]) ){` |
|    ! 0 | 3158 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    ! 0 | 3159 | `			if( pInst && pInst->pClass ){` |
|    ! 0 | 3160 | `				zType = SyStringData(&pInst->pClass->sName);` |
|    ! 0 | 3161 | `			}` |
|    ! 0 | 3162 | `		}` |
|    ! 0 | 3163 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3164 | `			"TypeError",` |
|      - | 3165 | `			"bin2hex(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 3166 | `			zType` |
|      - | 3167 | `			);` |
|      - | 3168 | `	}` |
|      - | 3169 | `	/* Extract the target string */` |
|    133 | 3170 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    133 | 3171 | `	if( nLen < 1 ){` |
|      - | 3172 | `		/* Empty string,return */` |
|     13 | 3173 | `		ph7_result_string(pCtx,"",0);` |
|     13 | 3174 | `		return PH7_OK;` |
|      - | 3175 | `	}` |
|      - | 3176 | `	/* Perform the requested operation */` |
|    121 | 3177 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|    121 | 3178 | `	return PH7_OK;` |
|     70 | 3179 | `}` |
|      - | 3180 |  |
|      - | 3181 | `/* Search callback signature */` |
|      - | 3182 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 3183 | `/*` |
|      - | 3184 | ` * Case-insensitive pattern match.` |
|      - | 3185 | ` * Brute force is the default search method used here.` |
|      - | 3186 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 3187 | ` * well for short/medium texts on modern hardware.` |
|      - | 3188 | ` */` |
|    298 | 3189 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      2 | 3190 | `{` |
|    300 | 3191 | `	const char *zpIn = (const char *)pPattern;` |
|    300 | 3192 | `	const char *zIn = (const char *)pText;` |
|    300 | 3193 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    300 | 3194 | `	const char *zEnd = &zIn[nLen];` |
|      - | 3195 | `	const char *zPtr,*zPtr2;` |
|      - | 3196 | `	int c,d;` |
|    300 | 3197 | `	if( iPatLen > nLen ){` |
|      - | 3198 | `		/* Don't bother processing */` |
|     67 | 3199 | `		return SXERR_NOTFOUND;` |
|      - | 3200 | `	}` |
|    860 | 3201 | `	for(;;){` |
|   1722 | 3202 | `		if( zIn >= zEnd ){` |
|    194 | 3203 | `			break;` |
|      - | 3204 | `		}` |
|   1530 | 3205 | `		c = SyToLower(zIn[0]);` |
|   1530 | 3206 | `		d = SyToLower(zpIn[0]);` |
|   1530 | 3207 | `		if( c == d ){` |
|    182 | 3208 | `			zPtr   = &zIn[1];` |
|    182 | 3209 | `			zPtr2  = &zpIn[1];` |
|    141 | 3210 | `			for(;;){` |
|    284 | 3211 | `				if( zPtr2 >= zpEnd ){` |
|      - | 3212 | `					/* Pattern found */` |
|     41 | 3213 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 3214 | `					return SXRET_OK;` |
|      - | 3215 | `				}` |
|    244 | 3216 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 3217 | `					break;` |
|      - | 3218 | `				}` |
|    244 | 3219 | `				c = SyToLower(zPtr[0]);` |
|    244 | 3220 | `				d = SyToLower(zPtr2[0]);` |
|    244 | 3221 | `				if( c != d ){` |
|    142 | 3222 | `					break;` |
|      - | 3223 | `				}` |
|    103 | 3224 | `				zPtr++; zPtr2++;` |
|      1 | 3225 | `			}` |
|     70 | 3226 | `		}` |
|   1490 | 3227 | `		zIn++;` |
|      2 | 3228 | `	}` |
|      - | 3229 | `	/* Pattern not found */` |
|    194 | 3230 | `	return SXERR_NOTFOUND;` |
|    151 | 3231 | `}` |
|      - | 3232 | `/*` |
|      - | 3233 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3234 | ` *  Find the first occurrence of a string.` |
|      - | 3235 | ` * Parameters` |
|      - | 3236 | ` *  $haystack` |
|      - | 3237 | ` *   The input string.` |
|      - | 3238 | ` * $needle` |
|      - | 3239 | ` *   Search pattern (must be a string).` |
|      - | 3240 | ` * $before_needle` |
|      - | 3241 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3242 | ` *   of the needle (excluding the needle).` |
|      - | 3243 | ` * Return` |
|      - | 3244 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3245 | ` */` |
|      6 | 3246 | `static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3247 | `{` |
|      7 | 3248 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3249 | `	const char *zBlob,*zPattern;` |
|      - | 3250 | `	int nLen,nPatLen;` |
|      - | 3251 | `	sxu32 nOfft;` |
|      - | 3252 | `	sxi32 rc;` |
|      7 | 3253 | `	if( nArg < 2 ){` |
|      - | 3254 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3255 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3256 | `		return PH7_OK;` |
|      - | 3257 | `	}` |
|      - | 3258 | `	/* Extract the needle and the haystack */` |
|      7 | 3259 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 3260 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 3261 | `	nOfft = 0; /* cc warning */` |
|      9 | 3262 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3263 | `		int before = 0;` |
|      - | 3264 | `		/* Perform the lookup */` |
|      5 | 3265 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3266 | `		if( rc != SXRET_OK ){` |
|      - | 3267 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3268 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3269 | `			return PH7_OK;` |
|      - | 3270 | `		}` |
|      - | 3271 | `		/* Return the portion of the string */` |
|      5 | 3272 | `		if( nArg > 2 ){` |
|      3 | 3273 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3274 | `		}` |
|      5 | 3275 | `		if( before ){` |
|      3 | 3276 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3277 | `		}else{` |
|      3 | 3278 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3279 | `		}` |
|      3 | 3280 | `	}else{` |
|      3 | 3281 | `		ph7_result_bool(pCtx,0);` |
|      - | 3282 | `	}` |
|      7 | 3283 | `	return PH7_OK;` |
|      4 | 3284 | `}` |
|      - | 3285 | `/*` |
|      - | 3286 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3287 | ` *  Case-insensitive strstr().` |
|      - | 3288 | ` * Parameters` |
|      - | 3289 | ` *  $haystack` |
|      - | 3290 | ` *   The input string.` |
|      - | 3291 | ` * $needle` |
|      - | 3292 | ` *   Search pattern (must be a string).` |
|      - | 3293 | ` * $before_needle` |
|      - | 3294 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3295 | ` *   of the needle (excluding the needle).` |
|      - | 3296 | ` * Return` |
|      - | 3297 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3298 | ` */` |
|      4 | 3299 | `static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3300 | `{` |
|      5 | 3301 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3302 | `	const char *zBlob,*zPattern;` |
|      - | 3303 | `	int nLen,nPatLen;` |
|      - | 3304 | `	sxu32 nOfft;` |
|      - | 3305 | `	sxi32 rc;` |
|      5 | 3306 | `	if( nArg < 2 ){` |
|      - | 3307 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3308 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3309 | `		return PH7_OK;` |
|      - | 3310 | `	}` |
|      - | 3311 | `	/* Extract the needle and the haystack */` |
|      5 | 3312 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3313 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      5 | 3314 | `	nOfft = 0; /* cc warning */` |
|      7 | 3315 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3316 | `		int before = 0;` |
|      - | 3317 | `		/* Perform the lookup */` |
|      5 | 3318 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3319 | `		if( rc != SXRET_OK ){` |
|      - | 3320 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3321 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3322 | `			return PH7_OK;` |
|      - | 3323 | `		}` |
|      - | 3324 | `		/* Return the portion of the string */` |
|      5 | 3325 | `		if( nArg > 2 ){` |
|      3 | 3326 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3327 | `		}` |
|      5 | 3328 | `		if( before ){` |
|      3 | 3329 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3330 | `		}else{` |
|      3 | 3331 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3332 | `		}` |
|      3 | 3333 | `	}else{` |
|    ! 0 | 3334 | `		ph7_result_bool(pCtx,0);` |
|      - | 3335 | `	}` |
|      5 | 3336 | `	return PH7_OK;` |
|      3 | 3337 | `}` |
|      - | 3338 | `/*` |
|      - | 3339 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3340 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 3341 | ` * Parameters` |
|      - | 3342 | ` *  $haystack` |
|      - | 3343 | ` *   The input string.` |
|      - | 3344 | ` * $needle` |
|      - | 3345 | ` *   Search pattern (must be a string).` |
|      - | 3346 | ` * $offset` |
|      - | 3347 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3348 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3349 | ` *   of haystack.` |
|      - | 3350 | ` * Return` |
|      - | 3351 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3352 | ` */` |
|   1468 | 3353 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3354 | `{` |
|   1473 | 3355 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strpos",1,"$haystack"); }` |
|   1473 | 3356 | `	if( nArg > 1 ){ StrNullArgNotice(pCtx,apArg[1],"strpos",2,"$needle"); }` |
|   1473 | 3357 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3358 | `	const char *zBlob,*zPattern;` |
|      - | 3359 | `	int nLen,nPatLen,nStart;` |
|      - | 3360 | `	sxu32 nOfft;` |
|      - | 3361 | `	sxi32 rc;` |
|   1473 | 3362 | `	if( nArg < 2 ){` |
|      - | 3363 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3364 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3365 | `		return PH7_OK;` |
|      - | 3366 | `	}` |
|      - | 3367 | `	/* Extract the needle and the haystack */` |
|   1473 | 3368 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|   1473 | 3369 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|   1473 | 3370 | `	nOfft = 0; /* cc warning */` |
|   1473 | 3371 | `	nStart = 0;` |
|      - | 3372 | `	/* Peek the starting offset if available */` |
|   1473 | 3373 | `	if( nArg > 2 ){` |
|     15 | 3374 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3375 | `		if( nStart < 0 ){` |
|    ! 0 | 3376 | `			nStart = -nStart;` |
|    ! 0 | 3377 | `		}` |
|     15 | 3378 | `		if( nStart >= nLen ){` |
|      - | 3379 | `			/* Invalid offset */` |
|    ! 0 | 3380 | `			nStart = 0;` |
|    ! 0 | 3381 | `		}else{` |
|     15 | 3382 | `			zBlob += nStart;` |
|     15 | 3383 | `			nLen -= nStart;` |
|      - | 3384 | `		}` |
|      7 | 3385 | `	}` |
|   1473 | 3386 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3387 | `		/* Perform the lookup */` |
|   1471 | 3388 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|   1471 | 3389 | `		if( rc != SXRET_OK ){` |
|      - | 3390 | `			/* Pattern not found,return FALSE */` |
|    779 | 3391 | `			ph7_result_bool(pCtx,0);` |
|    779 | 3392 | `			return PH7_OK;` |
|      - | 3393 | `		}` |
|      - | 3394 | `		/* Return the pattern position */` |
|    696 | 3395 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|    350 | 3396 | `	}else{` |
|      3 | 3397 | `		ph7_result_bool(pCtx,0);` |
|      - | 3398 | `	}` |
|    698 | 3399 | `	return PH7_OK;` |
|    739 | 3400 | `}` |
|      - | 3401 | `/*` |
|      - | 3402 | ` * Validate and resolve a single string-typed parameter for str_contains/` |
|      - | 3403 | ` * str_starts_with/str_ends_with. Emits an E_DEPRECATED notice for null` |
|      - | 3404 | ` * (matching PHP 8.1+; falls through with an empty string), and throws` |
|      - | 3405 | ` * TypeError for arrays, resources, and objects without __toString.` |
|      - | 3406 | ` *` |
|      - | 3407 | ` * For objects with __toString, invokes the method directly into pTmp and` |
|      - | 3408 | ` * uses its raw byte buffer. This preserves empty results, which the` |
|      - | 3409 | ` * engine's MemObjStringValue otherwise replaces with the literal "Object".` |
|      - | 3410 | ` *` |
|      - | 3411 | ` * On success, pzOut/pnOut point at the resolved byte buffer; the buffer` |
|      - | 3412 | ` * is valid until pTmp is released or pArg is mutated.` |
|      - | 3413 | ` */` |
|    668 | 3414 | `static sxi32 StrPredicateResolveArg(` |
|      - | 3415 | `	ph7_context *pCtx,` |
|      - | 3416 | `	ph7_value *pArg,` |
|      - | 3417 | `	const char *zFunc,` |
|      - | 3418 | `	int iArgNum,` |
|      - | 3419 | `	const char *zParamName,` |
|      - | 3420 | `	const char *zTypeStr, /* Declared type in the TypeError, e.g. "string" / "?string" */` |
|      - | 3421 | `	const char *zNullMsg,` |
|      - | 3422 | `	ph7_value *pTmp,` |
|      - | 3423 | `	const char **pzOut,` |
|      - | 3424 | `	int *pnOut` |
|      2 | 3425 | `){` |
|    670 | 3426 | `	if( ph7_value_is_null(pArg) ){` |
|     13 | 3427 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zNullMsg);` |
|     13 | 3428 | `		*pzOut = "";` |
|     13 | 3429 | `		*pnOut = 0;` |
|     13 | 3430 | `		return PH7_OK;` |
|      - | 3431 | `	}` |
|   1010 | 3432 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_resource(pArg) \|\|` |
|    656 | 3433 | `	    ( ph7_value_is_object(pArg) &&` |
|     72 | 3434 | `	      ((ph7_class_instance *)pArg->x.pOther) != 0 &&` |
|     48 | 3435 | `	      PH7_ClassExtractMethod(((ph7_class_instance *)pArg->x.pOther)->pClass,` |
|     24 | 3436 | `	        "__toString",sizeof("__toString")-1) == 0` |
|      - | 3437 | `	    )` |
|      - | 3438 | `	){` |
|    ! 0 | 3439 | `		const char *zType = ph7_type_name(pArg);` |
|    ! 0 | 3440 | `		if( ph7_value_is_object(pArg) ){` |
|    ! 0 | 3441 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|    ! 0 | 3442 | `			if( pInst && pInst->pClass ){` |
|    ! 0 | 3443 | `				zType = SyStringData(&pInst->pClass->sName);` |
|    ! 0 | 3444 | `			}` |
|    ! 0 | 3445 | `		}` |
|    ! 0 | 3446 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3447 | `			"TypeError",` |
|      - | 3448 | `			"%s(): Argument #%d (%s) must be of type %s, %s given",` |
|    ! 0 | 3449 | `			zFunc, iArgNum, zParamName, zTypeStr, zType` |
|      - | 3450 | `			);` |
|      - | 3451 | `	}` |
|    658 | 3452 | `	if( ph7_value_is_object(pArg) ){` |
|     49 | 3453 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     49 | 3454 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pInst->pClass,` |
|      - | 3455 | `			"__toString",sizeof("__toString")-1);` |
|     49 | 3456 | `		PH7_VmCallClassMethod(pCtx->pVm,pInst,pMethod,pTmp,0,0);` |
|     49 | 3457 | `		*pzOut = (const char *)SyBlobData(&pTmp->sBlob);` |
|     49 | 3458 | `		*pnOut = (int)SyBlobLength(&pTmp->sBlob);` |
|     49 | 3459 | `		return PH7_OK;` |
|      - | 3460 | `	}` |
|    610 | 3461 | `	*pzOut = ph7_value_to_string(pArg,pnOut);` |
|    610 | 3462 | `	return PH7_OK;` |
|    336 | 3463 | `}` |
|      - | 3464 | `/*` |
|      - | 3465 | ` * bool str_contains(string $haystack, string $needle)` |
|      - | 3466 | ` *  Determine if a string contains a given substring (PHP 8.0).` |
|      - | 3467 | ` * Return` |
|      - | 3468 | ` *  TRUE if needle occurs in haystack. An empty needle always returns TRUE.` |
|      - | 3469 | ` */` |
|     92 | 3470 | `static int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3471 | `{` |
|      - | 3472 | `	const char *zHaystack,*zNeedle;` |
|      - | 3473 | `	int nHayLen,nNeedleLen;` |
|      - | 3474 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3475 | `	sxi32 rc;` |
|     95 | 3476 | `	if( nArg != 2 ){` |
|      8 | 3477 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3478 | `			"ArgumentCountError",` |
|      - | 3479 | `			"str_contains() expects exactly 2 arguments, %d given",` |
|      2 | 3480 | `			nArg` |
|      - | 3481 | `			);` |
|      - | 3482 | `	}` |
|     90 | 3483 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     90 | 3484 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     90 | 3485 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_contains",1,"$haystack","string",` |
|      - | 3486 | `		"str_contains(): Passing null to parameter #1 ($haystack) "` |
|      - | 3487 | `		"of type string is deprecated",` |
|      - | 3488 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     90 | 3489 | `	if( rc != PH7_OK ) goto out;` |
|     90 | 3490 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_contains",2,"$needle","string",` |
|      - | 3491 | `		"str_contains(): Passing null to parameter #2 ($needle) "` |
|      - | 3492 | `		"of type string is deprecated",` |
|      - | 3493 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     90 | 3494 | `	if( rc != PH7_OK ) goto out;` |
|     90 | 3495 | `	if( nNeedleLen < 1 ){` |
|     13 | 3496 | `		ph7_result_bool(pCtx,1);` |
|     84 | 3497 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 3498 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3499 | `	}else{` |
|    104 | 3500 | `		sxi32 srch = SyBlobSearch((const void *)zHaystack,(sxu32)nHayLen,` |
|     34 | 3501 | `		                          (const void *)zNeedle,(sxu32)nNeedleLen,0);` |
|     70 | 3502 | `		ph7_result_bool(pCtx,srch == SXRET_OK ? 1 : 0);` |
|      - | 3503 | `	}` |
|     90 | 3504 | `	rc = PH7_OK;` |
|     44 | 3505 | `out:` |
|     90 | 3506 | `	PH7_MemObjRelease(&sHayTmp);` |
|     90 | 3507 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     90 | 3508 | `	return rc;` |
|     49 | 3509 | `}` |
|      - | 3510 | `/*` |
|      - | 3511 | ` * bool str_starts_with(string $haystack, string $needle)` |
|      - | 3512 | ` *  Check if a string starts with a given substring (PHP 8.0).` |
|      - | 3513 | ` * Return` |
|      - | 3514 | ` *  TRUE if haystack begins with needle. An empty needle always returns TRUE.` |
|      - | 3515 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 3516 | ` */` |
|     62 | 3517 | `static int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3518 | `{` |
|      - | 3519 | `	const char *zHaystack,*zNeedle;` |
|      - | 3520 | `	int nHayLen,nNeedleLen;` |
|      - | 3521 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3522 | `	sxi32 rc;` |
|     64 | 3523 | `	if( nArg != 2 ){` |
|      8 | 3524 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3525 | `			"ArgumentCountError",` |
|      - | 3526 | `			"str_starts_with() expects exactly 2 arguments, %d given",` |
|      2 | 3527 | `			nArg` |
|      - | 3528 | `			);` |
|      - | 3529 | `	}` |
|     59 | 3530 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     59 | 3531 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     59 | 3532 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_starts_with",1,"$haystack","string",` |
|      - | 3533 | `		"str_starts_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 3534 | `		"of type string is deprecated",` |
|      - | 3535 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     59 | 3536 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3537 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_starts_with",2,"$needle","string",` |
|      - | 3538 | `		"str_starts_with(): Passing null to parameter #2 ($needle) "` |
|      - | 3539 | `		"of type string is deprecated",` |
|      - | 3540 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     59 | 3541 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3542 | `	if( nNeedleLen < 1 ){` |
|     13 | 3543 | `		ph7_result_bool(pCtx,1);` |
|     53 | 3544 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 3545 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3546 | `	}else{` |
|     58 | 3547 | `		ph7_result_bool(pCtx,` |
|     38 | 3548 | `			SyMemcmp(zHaystack,zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 3549 | `	}` |
|     59 | 3550 | `	rc = PH7_OK;` |
|     29 | 3551 | `out:` |
|     59 | 3552 | `	PH7_MemObjRelease(&sHayTmp);` |
|     59 | 3553 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     59 | 3554 | `	return rc;` |
|     33 | 3555 | `}` |
|      - | 3556 | `/*` |
|      - | 3557 | ` * bool str_ends_with(string $haystack, string $needle)` |
|      - | 3558 | ` *  Check if a string ends with a given substring (PHP 8.0).` |
|      - | 3559 | ` * Return` |
|      - | 3560 | ` *  TRUE if haystack ends with needle. An empty needle always returns TRUE.` |
|      - | 3561 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 3562 | ` */` |
|     62 | 3563 | `static int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3564 | `{` |
|      - | 3565 | `	const char *zHaystack,*zNeedle;` |
|      - | 3566 | `	int nHayLen,nNeedleLen;` |
|      - | 3567 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3568 | `	sxi32 rc;` |
|     64 | 3569 | `	if( nArg != 2 ){` |
|      8 | 3570 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3571 | `			"ArgumentCountError",` |
|      - | 3572 | `			"str_ends_with() expects exactly 2 arguments, %d given",` |
|      2 | 3573 | `			nArg` |
|      - | 3574 | `			);` |
|      - | 3575 | `	}` |
|     59 | 3576 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     59 | 3577 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     59 | 3578 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_ends_with",1,"$haystack","string",` |
|      - | 3579 | `		"str_ends_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 3580 | `		"of type string is deprecated",` |
|      - | 3581 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     59 | 3582 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3583 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_ends_with",2,"$needle","string",` |
|      - | 3584 | `		"str_ends_with(): Passing null to parameter #2 ($needle) "` |
|      - | 3585 | `		"of type string is deprecated",` |
|      - | 3586 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     59 | 3587 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3588 | `	if( nNeedleLen < 1 ){` |
|     13 | 3589 | `		ph7_result_bool(pCtx,1);` |
|     53 | 3590 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 3591 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3592 | `	}else{` |
|     58 | 3593 | `		ph7_result_bool(pCtx,` |
|     38 | 3594 | `			SyMemcmp(zHaystack + (nHayLen - nNeedleLen),zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 3595 | `	}` |
|     59 | 3596 | `	rc = PH7_OK;` |
|     29 | 3597 | `out:` |
|     59 | 3598 | `	PH7_MemObjRelease(&sHayTmp);` |
|     59 | 3599 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     59 | 3600 | `	return rc;` |
|     33 | 3601 | `}` |
|      - | 3602 | `/*` |
|      - | 3603 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3604 | ` *  Case-insensitive strpos.` |
|      - | 3605 | ` * Parameters` |
|      - | 3606 | ` *  $haystack` |
|      - | 3607 | ` *   The input string.` |
|      - | 3608 | ` * $needle` |
|      - | 3609 | ` *   Search pattern (must be a string).` |
|      - | 3610 | ` * $offset` |
|      - | 3611 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3612 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3613 | ` *   of haystack.` |
|      - | 3614 | ` * Return` |
|      - | 3615 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3616 | ` */` |
|    196 | 3617 | `static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3618 | `{` |
|    198 | 3619 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3620 | `	const char *zBlob,*zPattern;` |
|      - | 3621 | `	int nLen,nPatLen,nStart;` |
|      - | 3622 | `	sxu32 nOfft;` |
|      - | 3623 | `	sxi32 rc;` |
|    198 | 3624 | `	if( nArg < 2 ){` |
|      - | 3625 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3626 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3627 | `		return PH7_OK;` |
|      - | 3628 | `	}` |
|      - | 3629 | `	/* Extract the needle and the haystack */` |
|    198 | 3630 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|    198 | 3631 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|    198 | 3632 | `	nOfft = 0; /* cc warning */` |
|    198 | 3633 | `	nStart = 0;` |
|      - | 3634 | `	/* Peek the starting offset if available */` |
|    198 | 3635 | `	if( nArg > 2 ){` |
|      5 | 3636 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 3637 | `		if( nStart < 0 ){` |
|      3 | 3638 | `			nStart = -nStart;` |
|      1 | 3639 | `		}` |
|      5 | 3640 | `		if( nStart >= nLen ){` |
|      - | 3641 | `			/* Invalid offset */` |
|    ! 0 | 3642 | `			nStart = 0;` |
|    ! 0 | 3643 | `		}else{` |
|      5 | 3644 | `			zBlob += nStart;` |
|      5 | 3645 | `			nLen -= nStart;` |
|      - | 3646 | `		}` |
|      2 | 3647 | `	}` |
|    198 | 3648 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3649 | `		/* Perform the lookup */` |
|    198 | 3650 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|    198 | 3651 | `		if( rc != SXRET_OK ){` |
|      - | 3652 | `			/* Pattern not found,return FALSE */` |
|    184 | 3653 | `			ph7_result_bool(pCtx,0);` |
|    184 | 3654 | `			return PH7_OK;` |
|      - | 3655 | `		}` |
|      - | 3656 | `		/* Return the pattern position */` |
|     15 | 3657 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 3658 | `	}else{` |
|    ! 0 | 3659 | `		ph7_result_bool(pCtx,0);` |
|      - | 3660 | `	}` |
|     15 | 3661 | `	return PH7_OK;` |
|    100 | 3662 | `}` |
|      - | 3663 | `/*` |
|      - | 3664 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3665 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 3666 | ` * Parameters` |
|      - | 3667 | ` *  $haystack` |
|      - | 3668 | ` *   The input string.` |
|      - | 3669 | ` * $needle` |
|      - | 3670 | ` *   Search pattern (must be a string).` |
|      - | 3671 | ` * $offset` |
|      - | 3672 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3673 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3674 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3675 | ` * Return` |
|      - | 3676 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3677 | ` */` |
|     40 | 3678 | `static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3679 | `{` |
|      - | 3680 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     41 | 3681 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3682 | `	int nLen,nPatLen;` |
|      - | 3683 | `	sxu32 nOfft;` |
|      - | 3684 | `	sxi32 rc;` |
|     41 | 3685 | `	if( nArg < 2 ){` |
|      - | 3686 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3687 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3688 | `		return PH7_OK;` |
|      - | 3689 | `	}` |
|      - | 3690 | `	/* Extract the needle and the haystack */` |
|     41 | 3691 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     41 | 3692 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3693 | `	/* Point to the end of the pattern */` |
|     41 | 3694 | `	zPtr = &zBlob[nLen - 1];` |
|     41 | 3695 | `	zEnd = &zBlob[nLen];` |
|      - | 3696 | `	/* Save the starting posistion */` |
|     41 | 3697 | `	zStart = zBlob;` |
|     41 | 3698 | `	nOfft = 0; /* cc warning */` |
|      - | 3699 | `	/* Peek the starting offset if available */` |
|     41 | 3700 | `	if( nArg > 2 ){` |
|      - | 3701 | `		int nStart;` |
|     21 | 3702 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 3703 | `		if( nStart < 0 ){` |
|     11 | 3704 | `			nStart = -nStart;` |
|     11 | 3705 | `			if( nStart >= nLen ){` |
|      - | 3706 | `				/* Invalid offset */` |
|      3 | 3707 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3708 | `				return PH7_OK;` |
|    ! 0 | 3709 | `			}else{` |
|      9 | 3710 | `				nLen -= nStart;` |
|      9 | 3711 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 3712 | `				zEnd = &zBlob[nLen];` |
|      - | 3713 | `			}` |
|      5 | 3714 | `		}else{` |
|     11 | 3715 | `			if( nStart >= nLen ){` |
|      - | 3716 | `				/* Invalid offset */` |
|      5 | 3717 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3718 | `				return PH7_OK;` |
|    ! 0 | 3719 | `			}else{` |
|      7 | 3720 | `				zBlob += nStart;` |
|      7 | 3721 | `				nLen -= nStart;` |
|      - | 3722 | `			}` |
|      - | 3723 | `		}` |
|      7 | 3724 | `	}` |
|     35 | 3725 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3726 | `		/* Perform the lookup */` |
|    121 | 3727 | `		for(;;){` |
|    243 | 3728 | `			if( zBlob >= zPtr ){` |
|     21 | 3729 | `				break;` |
|      - | 3730 | `			}` |
|    223 | 3731 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    223 | 3732 | `			if( rc == SXRET_OK ){` |
|      - | 3733 | `				/* Pattern found,return it's position */` |
|     13 | 3734 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     13 | 3735 | `				return PH7_OK;` |
|      - | 3736 | `			}` |
|    211 | 3737 | `			zPtr--;` |
|      1 | 3738 | `		}` |
|      - | 3739 | `		/* Pattern not found,return FALSE */` |
|     21 | 3740 | `		ph7_result_bool(pCtx,0);` |
|     11 | 3741 | `	}else{` |
|      3 | 3742 | `		ph7_result_bool(pCtx,0);` |
|      - | 3743 | `	}` |
|     23 | 3744 | `	return PH7_OK;` |
|     21 | 3745 | `}` |
|      - | 3746 | `/*` |
|      - | 3747 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3748 | ` *  Case-insensitive strrpos.` |
|      - | 3749 | ` * Parameters` |
|      - | 3750 | ` *  $haystack` |
|      - | 3751 | ` *   The input string.` |
|      - | 3752 | ` * $needle` |
|      - | 3753 | ` *   Search pattern (must be a string).` |
|      - | 3754 | ` * $offset` |
|      - | 3755 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3756 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3757 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3758 | ` * Return` |
|      - | 3759 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3760 | ` */` |
|     26 | 3761 | `static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3762 | `{` |
|      - | 3763 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     27 | 3764 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3765 | `	int nLen,nPatLen;` |
|      - | 3766 | `	sxu32 nOfft;` |
|      - | 3767 | `	sxi32 rc;` |
|     27 | 3768 | `	if( nArg < 2 ){` |
|      - | 3769 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3770 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3771 | `		return PH7_OK;` |
|      - | 3772 | `	}` |
|      - | 3773 | `	/* Extract the needle and the haystack */` |
|     27 | 3774 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 3775 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3776 | `	/* Point to the end of the pattern */` |
|     27 | 3777 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 3778 | `	zEnd = &zBlob[nLen];` |
|      - | 3779 | `	/* Save the starting posistion */` |
|     27 | 3780 | `	zStart = zBlob;` |
|     27 | 3781 | `	nOfft = 0; /* cc warning */` |
|      - | 3782 | `	/* Peek the starting offset if available */` |
|     27 | 3783 | `	if( nArg > 2 ){` |
|      - | 3784 | `		int nStart;` |
|     15 | 3785 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3786 | `		if( nStart < 0 ){` |
|      7 | 3787 | `			nStart = -nStart;` |
|      7 | 3788 | `			if( nStart >= nLen ){` |
|      - | 3789 | `				/* Invalid offset */` |
|      3 | 3790 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3791 | `				return PH7_OK;` |
|    ! 0 | 3792 | `			}else{` |
|      5 | 3793 | `				nLen -= nStart;` |
|      5 | 3794 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 3795 | `				zEnd = &zBlob[nLen];` |
|      - | 3796 | `			}` |
|      3 | 3797 | `		}else{` |
|      9 | 3798 | `			if( nStart >= nLen ){` |
|      - | 3799 | `				/* Invalid offset */` |
|      5 | 3800 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3801 | `				return PH7_OK;` |
|    ! 0 | 3802 | `			}else{` |
|      5 | 3803 | `				zBlob += nStart;` |
|      5 | 3804 | `				nLen -= nStart;` |
|      - | 3805 | `			}` |
|      - | 3806 | `		}` |
|      4 | 3807 | `	}` |
|     21 | 3808 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3809 | `		/* Perform the lookup */` |
|     44 | 3810 | `		for(;;){` |
|     89 | 3811 | `			if( zBlob >= zPtr ){` |
|      9 | 3812 | `				break;` |
|      - | 3813 | `			}` |
|     81 | 3814 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 3815 | `			if( rc == SXRET_OK ){` |
|      - | 3816 | `				/* Pattern found,return it's position */` |
|     11 | 3817 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 3818 | `				return PH7_OK;` |
|      - | 3819 | `			}` |
|     71 | 3820 | `			zPtr--;` |
|      1 | 3821 | `		}` |
|      - | 3822 | `		/* Pattern not found,return FALSE */` |
|      9 | 3823 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3824 | `	}else{` |
|      3 | 3825 | `		ph7_result_bool(pCtx,0);` |
|      - | 3826 | `	}` |
|     11 | 3827 | `	return PH7_OK;` |
|     14 | 3828 | `}` |
|      - | 3829 | `/*` |
|      - | 3830 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 3831 | ` *  Find the last occurrence of a character in a string.` |
|      - | 3832 | ` * Parameters` |
|      - | 3833 | ` *  $haystack` |
|      - | 3834 | ` *   The input string.` |
|      - | 3835 | ` * $needle` |
|      - | 3836 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 3837 | ` *  This behavior is different from that of strstr().` |
|      - | 3838 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 3839 | ` *  as the ordinal value of a character.` |
|      - | 3840 | ` * Return` |
|      - | 3841 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 3842 | ` */` |
|     22 | 3843 | `static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3844 | `{` |
|      - | 3845 | `	const char *zBlob;` |
|      - | 3846 | `	int nLen,c;` |
|     23 | 3847 | `	if( nArg < 2 ){` |
|      - | 3848 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3849 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3850 | `		return PH7_OK;` |
|      - | 3851 | `	}` |
|      - | 3852 | `	/* Extract the haystack */` |
|     23 | 3853 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3854 | `	c = 0; /* cc warning */` |
|     23 | 3855 | `	if( nLen > 0 ){` |
|      - | 3856 | `		sxu32 nOfft;` |
|      - | 3857 | `		sxi32 rc;` |
|     21 | 3858 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 3859 | `			const char *zPattern;` |
|     11 | 3860 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 3861 | `														 * for NULL pointer.` |
|      - | 3862 | `														 */` |
|     11 | 3863 | `			c = zPattern[0];` |
|      6 | 3864 | `		}else{` |
|      - | 3865 | `			/* Int cast */` |
|     11 | 3866 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 3867 | `		}` |
|      - | 3868 | `		/* Perform the lookup */` |
|     21 | 3869 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 3870 | `		if( rc != SXRET_OK ){` |
|      - | 3871 | `			/* No such entry,return FALSE */` |
|      7 | 3872 | `			ph7_result_bool(pCtx,0);` |
|      7 | 3873 | `			return PH7_OK;` |
|      - | 3874 | `		}` |
|      - | 3875 | `		/* Return the string portion */` |
|     15 | 3876 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 3877 | `	}else{` |
|      3 | 3878 | `		ph7_result_bool(pCtx,0);` |
|      - | 3879 | `	}` |
|     17 | 3880 | `	return PH7_OK;` |
|     12 | 3881 | `}` |
|      - | 3882 | `/*` |
|      - | 3883 | ` * string strrev(string $string)` |
|      - | 3884 | ` *  Reverse a string.` |
|      - | 3885 | ` * Parameters` |
|      - | 3886 | ` *  $string` |
|      - | 3887 | ` *   String to be reversed.` |
|      - | 3888 | ` * Return` |
|      - | 3889 | ` *  The reversed string.` |
|      - | 3890 | ` */` |
|      2 | 3891 | `static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3892 | `{` |
|      - | 3893 | `	const char *zIn,*zEnd;` |
|      - | 3894 | `	int nLen,c;` |
|      3 | 3895 | `	if( nArg < 1 ){` |
|      - | 3896 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3897 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3898 | `		return PH7_OK;` |
|      - | 3899 | `	}` |
|      - | 3900 | `	/* Extract the target string */` |
|      3 | 3901 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3902 | `	if( nLen < 1 ){` |
|      - | 3903 | `		/* Empty string Return null */` |
|    ! 0 | 3904 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3905 | `		return PH7_OK;` |
|      - | 3906 | `	}` |
|      - | 3907 | `	/* Perform the requested operation */` |
|      3 | 3908 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 3909 | `	for(;;){` |
|      9 | 3910 | `		if( zEnd < zIn ){` |
|      - | 3911 | `			/* No more input to process */` |
|      3 | 3912 | `			break;` |
|      - | 3913 | `		}` |
|      - | 3914 | `		/* Append current character */` |
|      7 | 3915 | `		c = zEnd[0];` |
|      7 | 3916 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 3917 | `		zEnd--;` |
|      1 | 3918 | `	}` |
|      3 | 3919 | `	return PH7_OK;` |
|      2 | 3920 | `}` |
|      - | 3921 | `/*` |
|      - | 3922 | ` * string ucwords(string $string [, string $separators = " \t\r\n\f\v"])` |
|      - | 3923 | ` *  Uppercase the first character of each word in a string.` |
|      - | 3924 | ` *  A word begins at the start of the string and after any character present in` |
|      - | 3925 | ` *  $separators. The default separators are the whitespace characters (space,` |
|      - | 3926 | ` *  horizontal tab, carriage return, newline, form-feed and vertical tab); an` |
|      - | 3927 | ` *  explicit $separators argument REPLACES them (an empty string leaves only the` |
|      - | 3928 | ` *  very first character upper-cased). Like PHP, this is byte-based: only ASCII` |
|      - | 3929 | ` *  bytes are upper-cased and a byte is a separator only if it appears in the set.` |
|      - | 3930 | ` * Parameters` |
|      - | 3931 | ` *  $string` |
|      - | 3932 | ` *   The input string.` |
|      - | 3933 | ` *  $separators` |
|      - | 3934 | ` *   The optional word-boundary characters.` |
|      - | 3935 | ` * Return` |
|      - | 3936 | ` *  The modified string.` |
|      - | 3937 | ` */` |
|     22 | 3938 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3939 | `{` |
|      - | 3940 | `	const char *zIn;` |
|      - | 3941 | `	int nLen,i,iStart;` |
|      - | 3942 | `	char aDelim[256];` |
|     23 | 3943 | `	if( nArg < 1 ){` |
|      - | 3944 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3945 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3946 | `		return PH7_OK;` |
|      - | 3947 | `	}` |
|      - | 3948 | `	/* Build the separator membership table: an explicit $separators argument` |
|      - | 3949 | `	 * replaces the default whitespace set (an empty string clears it). */` |
|     23 | 3950 | `	SyZero(aDelim,(sxu32)sizeof(aDelim));` |
|     23 | 3951 | `	if( nArg > 1 ){` |
|      - | 3952 | `		int nDelim;` |
|      9 | 3953 | `		const char *zDelim = ph7_value_to_string(apArg[1],&nDelim);` |
|     17 | 3954 | `		for( i = 0 ; i < nDelim ; i++ ){` |
|      9 | 3955 | `			aDelim[(unsigned char)zDelim[i]] = 1;` |
|      5 | 3956 | `		}` |
|      5 | 3957 | `	}else{` |
|     15 | 3958 | `		aDelim[(unsigned char)' ']  = 1;` |
|     15 | 3959 | `		aDelim[(unsigned char)'\t'] = 1;` |
|     15 | 3960 | `		aDelim[(unsigned char)'\r'] = 1;` |
|     15 | 3961 | `		aDelim[(unsigned char)'\n'] = 1;` |
|     15 | 3962 | `		aDelim[(unsigned char)'\f'] = 1;` |
|     15 | 3963 | `		aDelim[(unsigned char)'\v'] = 1;` |
|      - | 3964 | `	}` |
|      - | 3965 | `	/* Extract the target string */` |
|     23 | 3966 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3967 | `	if( nLen < 1 ){` |
|      - | 3968 | `		/* Empty string – match PHP semantics */` |
|      3 | 3969 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3970 | `		return PH7_OK;` |
|      - | 3971 | `	}` |
|      - | 3972 | `	/* Upper-case the first byte of each word (the leading byte, or any byte that` |
|      - | 3973 | `	 * follows a separator), appending the untouched runs in between verbatim. */` |
|     21 | 3974 | `	iStart = 0;` |
|    309 | 3975 | `	for( i = 0 ; i < nLen ; i++ ){` |
|    289 | 3976 | `		int c = (unsigned char)zIn[i];` |
|    289 | 3977 | `		if( (i == 0 \|\| aDelim[(unsigned char)zIn[i-1]]) && c < 0x80 && SyisLower(c) ){` |
|     53 | 3978 | `			char up = (char)SyToUpper(c);` |
|     53 | 3979 | `			if( i > iStart ){` |
|     35 | 3980 | `				ph7_result_string(pCtx,&zIn[iStart],i - iStart);` |
|     17 | 3981 | `			}` |
|     53 | 3982 | `			ph7_result_string(pCtx,&up,1);` |
|     53 | 3983 | `			iStart = i + 1;` |
|     26 | 3984 | `		}` |
|    145 | 3985 | `	}` |
|     21 | 3986 | `	if( nLen > iStart ){` |
|     21 | 3987 | `		ph7_result_string(pCtx,&zIn[iStart],nLen - iStart);` |
|     10 | 3988 | `	}` |
|     21 | 3989 | `	return PH7_OK;` |
|     12 | 3990 | `}` |
|      - | 3991 | `/*` |
|      - | 3992 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3993 | ` *  Returns input repeated multiplier times.` |
|      - | 3994 | ` * Parameters` |
|      - | 3995 | ` *  $string` |
|      - | 3996 | ` *   String to be repeated.` |
|      - | 3997 | ` * $multiplier` |
|      - | 3998 | ` *  Number of time the input string should be repeated.` |
|      - | 3999 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 4000 | ` *  to 0, the function will return an empty string.` |
|      - | 4001 | ` * Return` |
|      - | 4002 | ` *  The repeated string.` |
|      - | 4003 | ` */` |
|  20434 | 4004 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4005 | `{` |
|      - | 4006 | `	const char *zIn;` |
|      - | 4007 | `	int nLen;` |
|      - | 4008 | `	ph7_int64 nMul;` |
|      - | 4009 | `	int rc;` |
|  20436 | 4010 | `	if( nArg < 2 ){` |
|      - | 4011 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 4012 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4013 | `		return PH7_OK;` |
|      - | 4014 | `	}` |
|      - | 4015 | `	/* Extract the target string */` |
|  20436 | 4016 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 4017 | `	/* Resolve $times through the shared ZPP helper so a lossy float / float-string` |
|      - | 4018 | `	 * carries php's precision deprecation and NAN/INF/non-numeric fail with php's` |
|      - | 4019 | `	 * TypeError — a bare ph7_value_to_int64() coerced them silently. */` |
|      - | 4020 | `	{` |
|  20436 | 4021 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"str_repeat",2,"$times","int",&nMul);` |
|  20436 | 4022 | `		if( rcArg != PH7_OK ){` |
|    ! 0 | 4023 | `			return rcArg;` |
|      - | 4024 | `		}` |
|      - | 4025 | `	}` |
|  20436 | 4026 | `	if( nMul < 0 ){` |
|      3 | 4027 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4028 | `			"str_repeat(): Argument #2 ($times) must be greater than or equal to 0");` |
|      - | 4029 | `	}` |
|  20434 | 4030 | `	if( nLen < 1 \|\| nMul < 1 ){` |
|      - | 4031 | `		/* Empty input or a zero multiplier yields the empty string (PHP). */` |
|    ! 0 | 4032 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4033 | `		return PH7_OK;` |
|      - | 4034 | `	}` |
|      - | 4035 | `	/* Perform the requested operation */` |
| 221930 | 4036 | `	for(;;){` |
| 443862 | 4037 | `		if( !nMul ){` |
|  20434 | 4038 | `			break;` |
|      - | 4039 | `		}` |
|      - | 4040 | `		/* Append the copy */` |
| 423430 | 4041 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 423430 | 4042 | `		if( rc != PH7_OK ){` |
|      - | 4043 | `			/* Allocation failed: surface a fatal instead of returning a` |
|      - | 4044 | `			 * silently-truncated string with a success status. */` |
|    ! 0 | 4045 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 4046 | `		}` |
| 423430 | 4047 | `		nMul--;` |
|      2 | 4048 | `	}` |
|  20434 | 4049 | `	return PH7_OK;` |
|  10219 | 4050 | `}` |
|      - | 4051 | `/*` |
|      - | 4052 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 4053 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 4054 | ` * Parameters` |
|      - | 4055 | ` *  $string` |
|      - | 4056 | ` *   The input string.` |
|      - | 4057 | ` * $is_xhtml` |
|      - | 4058 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 4059 | ` * Return` |
|      - | 4060 | ` *  The processed string.` |
|      - | 4061 | ` */` |
|      4 | 4062 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4063 | `{` |
|      - | 4064 | `	const char *zIn,*zCur,*zEnd;` |
|      5 | 4065 | `	int is_xhtml = 1; /* Default to XHTML-style '<br />' like PHP */` |
|      - | 4066 | `	int nLen;` |
|      5 | 4067 | `	if( nArg < 1 ){` |
|      - | 4068 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4069 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4070 | `		return PH7_OK;` |
|      - | 4071 | `	}` |
|      - | 4072 | `	/* Extract the target string */` |
|      5 | 4073 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 4074 | `	if( nLen < 1 ){` |
|      - | 4075 | `		/* Empty string,return null */` |
|    ! 0 | 4076 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4077 | `		return PH7_OK;` |
|      - | 4078 | `	}` |
|      5 | 4079 | `	if( nArg > 1 ){` |
|      3 | 4080 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 4081 | `	}` |
|      5 | 4082 | `	zEnd = &zIn[nLen];` |
|      - | 4083 | `	/* Perform the requested operation */` |
|      4 | 4084 | `	for(;;){` |
|      9 | 4085 | `		zCur = zIn;` |
|      - | 4086 | `		/* Delimit the string */` |
|     21 | 4087 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 4088 | `			zIn++;` |
|      1 | 4089 | `		}` |
|      9 | 4090 | `		if( zCur < zIn ){` |
|      - | 4091 | `			/* Output chunk verbatim */` |
|      9 | 4092 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 4093 | `		}` |
|      9 | 4094 | `		if( zIn >= zEnd ){` |
|      - | 4095 | `			/* No more input to process */` |
|      5 | 4096 | `			break;` |
|      - | 4097 | `		}` |
|      - | 4098 | `		/* Output the HTML line break */` |
|      - | 4099 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br />' (space before the slash), otherwise use '<br>' */` |
|      5 | 4100 | `		if( is_xhtml ){` |
|      3 | 4101 | `			ph7_result_string(pCtx,"<br />",(int)sizeof("<br />")-1);` |
|      2 | 4102 | `		}else{` |
|      3 | 4103 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 4104 | `		}` |
|      5 | 4105 | `		zCur = zIn;` |
|      - | 4106 | `		/* Append trailing line */` |
|     11 | 4107 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 4108 | `			zIn++;` |
|      1 | 4109 | `		}` |
|      5 | 4110 | `		if( zCur < zIn ){` |
|      - | 4111 | `			/* Output chunk verbatim */` |
|      5 | 4112 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 4113 | `		}` |
|      1 | 4114 | `	}` |
|      5 | 4115 | `	return PH7_OK;` |
|      3 | 4116 | `}` |
|      - | 4117 | `/*` |
|      - | 4118 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 4119 | ` *  According to the PHP reference manual.` |
|      - | 4120 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 4121 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 4122 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 4123 | ` * This applies to both sprintf() and printf().` |
|      - | 4124 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 4125 | ` * or more of these elements, in order:` |
|      - | 4126 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 4127 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 4128 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 4129 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 4130 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 4131 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 4132 | ` *   it with a single quote ('). See the examples below.` |
|      - | 4133 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 4134 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 4135 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 4136 | ` *   should result in.` |
|      - | 4137 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 4138 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 4139 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 4140 | ` *   limit to the string.` |
|      - | 4141 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 4142 | ` *       % - a literal percent character. No argument is required.` |
|      - | 4143 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 4144 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 4145 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 4146 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 4147 | ` * 	     for the number of digits after the decimal point.` |
|      - | 4148 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 4149 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 4150 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 4151 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 4152 | ` *       g - shorter of %e and %f.` |
|      - | 4153 | ` *       G - shorter of %E and %f.` |
|      - | 4154 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 4155 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 4156 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 4157 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 4158 | ` */` |
|      - | 4159 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 4160 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 4161 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 4162 | `/*` |
|      - | 4163 | `** Conversion types fall into various categories as defined by the` |
|      - | 4164 | `** following enumeration.` |
|      - | 4165 | `*/` |
|      - | 4166 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 4167 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 4168 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 4169 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 4170 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 4171 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 4172 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 4173 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 4174 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 4175 |  |
|      - | 4176 | `/*` |
|      - | 4177 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 4178 | `*/` |
|      - | 4179 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 4180 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 4181 | `/*` |
|      - | 4182 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 4183 | `** by an instance of the following structure` |
|      - | 4184 | `*/` |
|      - | 4185 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 4186 | `struct ph7_fmt_info` |
|      - | 4187 | `{` |
|      - | 4188 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 4189 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 4190 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 4191 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 4192 | `  char *charset; /* The character set for conversion */` |
|      - | 4193 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 4194 | `};` |
|      - | 4195 | `/* PH7_PhpFloatShape (php's float-shape post-processing) lives in memobj.c —` |
|      - | 4196 | ` * the default float->string cast needs it even when this whole formatting` |
|      - | 4197 | ` * region is compiled out by PH7_DISABLE_DISK_IO. */` |
|      - | 4198 | `/*` |
|      - | 4199 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 4200 | ` * used conversion types first.` |
|      - | 4201 | ` */` |
|      - | 4202 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 4203 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 4204 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 4205 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 4206 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 4207 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 4208 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 4209 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 4210 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 4211 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 4212 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 4213 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 4214 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 4215 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 4216 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 4217 | `  /* php's 'h'/'H' are the locale-independent twins of 'g'/'G'; PHL always` |
|      - | 4218 | `   * formats in the C locale, so they behave identically. */` |
|      - | 4219 | `  {  'h',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 4220 | `  {  'H',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 4221 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 4222 | `};` |
|      - | 4223 | `/*` |
|      - | 4224 | ` * PHP 8 raises a catchable ValueError for an unknown conversion specifier` |
|      - | 4225 | ` * (e.g. "%y", or the C-ism "%#x" — '#' is not a php flag). Because printf()` |
|      - | 4226 | ` * and fprintf() stream their output incrementally while sprintf() buffers it,` |
|      - | 4227 | ` * every format builtin calls PH7_FormatValidate (below) to check the whole` |
|      - | 4228 | ` * format string BEFORE formatting so the throw happens with no partial output` |
|      - | 4229 | ` * escaping (php buffers the entire result and only emits it on success). This` |
|      - | 4230 | ` * scan mirrors the specifier-locating logic of the main format loop below.` |
|      - | 4231 | ` * On the first unknown specifier, stores it in *pBad and returns TRUE; returns` |
|      - | 4232 | ` * FALSE when every specifier is known. (A found-flag rather than a sentinel` |
|      - | 4233 | ` * char, so a NUL specifier byte — "%\0" — is still reported, not mistaken for` |
|      - | 4234 | ` * "all valid".)` |
|      - | 4235 | ` */` |
|    428 | 4236 | `static int FormatUnknownSpec(const char *zIn,int nByte,int *pBad)` |
|      2 | 4237 | `{` |
|    430 | 4238 | `	const char *zEnd = &zIn[nByte];` |
|      - | 4239 | `	int c,idx;` |
|   3610 | 4240 | `	while( zIn < zEnd ){` |
|   3202 | 4241 | `		if( zIn[0] != '%' ){` |
|   2369 | 4242 | `			zIn++;` |
|   2369 | 4243 | `			continue;` |
|      - | 4244 | `		}` |
|    834 | 4245 | `		zIn++; /* jump the percent sign */` |
|      - | 4246 | `		/* php-supported flags: '-', '+', ' ', '0' and the "'<pad>'" custom-pad` |
|      - | 4247 | `		 * form. '#' is intentionally NOT treated as a flag so it surfaces as an` |
|      - | 4248 | `		 * unknown specifier, matching php. */` |
|   1018 | 4249 | `		while( zIn < zEnd ){` |
|   1016 | 4250 | `			c = zIn[0];` |
|   1016 | 4251 | `			if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|    185 | 4252 | `				zIn++;` |
|    185 | 4253 | `				continue;` |
|      - | 4254 | `			}` |
|    832 | 4255 | `			if( c=='\'' ){` |
|    ! 0 | 4256 | `				zIn++;` |
|    ! 0 | 4257 | `				if( zIn < zEnd ){` |
|    ! 0 | 4258 | `					zIn++; /* the custom pad character */` |
|    ! 0 | 4259 | `				}` |
|    ! 0 | 4260 | `				continue;` |
|      - | 4261 | `			}` |
|    832 | 4262 | `			break;` |
|    ! 0 | 4263 | `		}` |
|      - | 4264 | `		/* field width */` |
|   1050 | 4265 | `		while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|    217 | 4266 | `			zIn++;` |
|      1 | 4267 | `		}` |
|      - | 4268 | `		/* positional specifier ($) — php parses flags AFTER it (e.g. "%1$-10s"),` |
|      - | 4269 | `		 * so skip the full flag set and width again, mirroring the main loop. */` |
|    834 | 4270 | `		if( zIn < zEnd && zIn[0]=='$' ){` |
|    ! 0 | 4271 | `			zIn++;` |
|    ! 0 | 4272 | `			while( zIn < zEnd ){` |
|    ! 0 | 4273 | `				c = zIn[0];` |
|    ! 0 | 4274 | `				if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|    ! 0 | 4275 | `					zIn++;` |
|    ! 0 | 4276 | `					continue;` |
|      - | 4277 | `				}` |
|    ! 0 | 4278 | `				if( c=='\'' ){` |
|    ! 0 | 4279 | `					zIn++;` |
|    ! 0 | 4280 | `					if( zIn < zEnd ){` |
|    ! 0 | 4281 | `						zIn++;` |
|    ! 0 | 4282 | `					}` |
|    ! 0 | 4283 | `					continue;` |
|      - | 4284 | `				}` |
|    ! 0 | 4285 | `				break;` |
|    ! 0 | 4286 | `			}` |
|    ! 0 | 4287 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|    ! 0 | 4288 | `				zIn++;` |
|    ! 0 | 4289 | `			}` |
|    ! 0 | 4290 | `		}` |
|      - | 4291 | `		/* precision */` |
|    834 | 4292 | `		if( zIn < zEnd && zIn[0]=='.' ){` |
|    100 | 4293 | `			zIn++;` |
|    208 | 4294 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|    110 | 4295 | `				zIn++;` |
|      2 | 4296 | `			}` |
|     49 | 4297 | `		}` |
|      - | 4298 | `		/* a single 'l' length modifier (ignored, php compat) */` |
|    834 | 4299 | `		if( zIn < zEnd && zIn[0]=='l' ){` |
|     11 | 4300 | `			zIn++;` |
|      5 | 4301 | `		}` |
|    834 | 4302 | `		if( zIn >= zEnd ){` |
|      - | 4303 | `			/* A dangling '%' with no specifier: PHL's legacy path silently` |
|      - | 4304 | `			 * truncates here (recorded residual); nothing to validate. */` |
|      3 | 4305 | `			break;` |
|      - | 4306 | `		}` |
|    832 | 4307 | `		c = zIn[0];` |
|    832 | 4308 | `		zIn++; /* jump the conversion specifier */` |
|   3486 | 4309 | `		for( idx = 0 ; idx < (int)SX_ARRAYSIZE(aFmt) ; idx++ ){` |
|   3468 | 4310 | `			if( c == aFmt[idx].fmttype ){` |
|    814 | 4311 | `				break;` |
|      - | 4312 | `			}` |
|   1329 | 4313 | `		}` |
|    832 | 4314 | `		if( idx >= (int)SX_ARRAYSIZE(aFmt) ){` |
|     19 | 4315 | `			*pBad = c; /* unknown specifier */` |
|     19 | 4316 | `			return TRUE;` |
|      - | 4317 | `		}` |
|      2 | 4318 | `	}` |
|    412 | 4319 | `	return FALSE;` |
|    216 | 4320 | `}` |
|      - | 4321 | `/*` |
|      - | 4322 | ` * Validate a printf-style format string. PHP 8 raises a catchable ValueError for` |
|      - | 4323 | ` * an unknown conversion specifier, thrown before any output is produced. Every` |
|      - | 4324 | ` * format builtin (sprintf/printf/vprintf/vsprintf/fprintf/vfprintf) calls this` |
|      - | 4325 | ` * up-front, then propagates the returned status verbatim (PH7_EXCEPTION when the` |
|      - | 4326 | ` * throw is caught in place, PH7_ABORT when it goes uncaught).` |
|      - | 4327 | ` * Returns PH7_OK when the format is valid.` |
|      - | 4328 | ` */` |
|    428 | 4329 | `PH7_PRIVATE sxi32 PH7_FormatValidate(ph7_context *pCtx,const char *zFormat,int nByte)` |
|      2 | 4330 | `{` |
|    430 | 4331 | `	int badSpec = 0;` |
|    430 | 4332 | `	if( FormatUnknownSpec(zFormat,nByte,&badSpec) ){` |
|     28 | 4333 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      9 | 4334 | `			"Unknown format specifier \"%c\"",badSpec);` |
|      - | 4335 | `	}` |
|    412 | 4336 | `	return PH7_OK;` |
|    216 | 4337 | `}` |
|      - | 4338 | `/*` |
|      - | 4339 | `` * PHP 8: a printf-family `$format` argument is a `string` parameter — scalars`` |
|      - | 4340 | ` * (int/float/bool) and null coerce to a string, but an array/object/resource` |
|      - | 4341 | ` * raises a catchable TypeError. iArg is the 1-based argument position ($format` |
|      - | 4342 | ` * is #1 for sprintf/printf/vprintf/vsprintf, #2 for fprintf/vfprintf). Returns` |
|      - | 4343 | ` * PH7_OK when the value is string-coercible (the caller then uses` |
|      - | 4344 | ` * ph7_value_to_string, which renders scalars/null verbatim).` |
|      - | 4345 | ` */` |
|      - | 4346 | `/*` |
|      - | 4347 | ` * php 8: a stream parameter that is not a resource is a TypeError, not a warning` |
|      - | 4348 | ` * with a 0 return -- the caller never learned its write went nowhere.` |
|      - | 4349 | ` */` |
|     20 | 4350 | `PH7_PRIVATE sxi32 PH7_CheckStreamArg(ph7_context *pCtx,ph7_value *pArg,int iArg,const char *zName)` |
|      1 | 4351 | `{` |
|     21 | 4352 | `	if( !ph7_value_is_resource(pArg) ){` |
|      - | 4353 | `		char zBuf[64];` |
|      4 | 4354 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4355 | `			"%s(): Argument #%d ($%s) must be of type resource, %s given",` |
|      1 | 4356 | `			ph7_function_name(pCtx),iArg,zName,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|      - | 4357 | `	}` |
|     19 | 4358 | `	return PH7_OK;` |
|     11 | 4359 | `}` |
|    440 | 4360 | `PH7_PRIVATE sxi32 PH7_FormatCheckFormatArg(ph7_context *pCtx,ph7_value *pArg,int iArg)` |
|      2 | 4361 | `{` |
|    442 | 4362 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg) \|\| ph7_value_is_resource(pArg) ){` |
|      - | 4363 | `		char zBuf[64];` |
|    ! 0 | 4364 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4365 | `			"%s(): Argument #%d ($format) must be of type string, %s given",` |
|    ! 0 | 4366 | `			ph7_function_name(pCtx),iArg,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|      - | 4367 | `	}` |
|    442 | 4368 | `	return PH7_OK;` |
|    222 | 4369 | `}` |
|      - | 4370 | `/*` |
|      - | 4371 | ` * Format a given string.` |
|      - | 4372 | ` * The root program.  All variations call this core.` |
|      - | 4373 | ` * INPUTS:` |
|      - | 4374 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 4375 | ` *            1. A pointer to the call context.` |
|      - | 4376 | ` *            2. A pointer to the list of characters to be output` |
|      - | 4377 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 4378 | ` *            3. An integer number of characters to be output.` |
|      - | 4379 | ` *               (Note: This number might be zero.)` |
|      - | 4380 | ` *            4. Upper layer private data.` |
|      - | 4381 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 4382 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 4383 | ` */` |
|    410 | 4384 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 4385 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 4386 | `	ph7_context *pCtx,  /* call context */` |
|      - | 4387 | `	const char *zIn,    /* Format string */` |
|      - | 4388 | `	int nByte,          /* Format string length */` |
|      - | 4389 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 4390 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 4391 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 4392 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 4393 | `	)` |
|      2 | 4394 | `{` |
|    412 | 4395 | `	char spaces[] = "                                                  ";` |
|      - | 4396 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    412 | 4397 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 4398 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 4399 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 4400 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 4401 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 4402 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 4403 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 4404 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 4405 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 4406 | `	ph7_int64 iVal;` |
|      - | 4407 | `	int precision;           /* Precision of the current field */` |
|      - | 4408 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 4409 | `	int c,rc,n;` |
|      - | 4410 | `	int length;              /* Length of the field */` |
|      - | 4411 | `	int prefix;` |
|      - | 4412 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 4413 | `	int width;               /* Width of the current field */` |
|      - | 4414 | `	int idx;` |
|    412 | 4415 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 4416 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 4417 | `	/* An unknown conversion specifier is rejected up-front by PH7_FormatValidate()` |
|      - | 4418 | `	 * (called by every format builtin before this routine), so the specifier set` |
|      - | 4419 | `	 * seen here is always valid. */` |
|      - | 4420 | `	/* Start the format process */` |
|    611 | 4421 | `	for(;;){` |
|   1224 | 4422 | `		zCur = zIn;` |
|   3578 | 4423 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|   2355 | 4424 | `			zIn++;` |
|      1 | 4425 | `		}` |
|   1224 | 4426 | `		if( zCur < zIn ){` |
|      - | 4427 | `			/* Consume chunk verbatim */` |
|    749 | 4428 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|    749 | 4429 | `			if( rc != SXRET_OK ){` |
|      - | 4430 | `				/* Callback requested an abort (e.g. an allocation failure) */` |
|    ! 0 | 4431 | `				break;` |
|      - | 4432 | `			}` |
|    374 | 4433 | `		}` |
|   1224 | 4434 | `		if( zIn >= zEnd ){` |
|      - | 4435 | `			/* No more input to process,break immediately */` |
|    410 | 4436 | `			break;` |
|      - | 4437 | `		}` |
|      - | 4438 | `		/* Find out what flags are present */` |
|    816 | 4439 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    814 | 4440 | `			flag_alternateform = flag_zeropad = 0;` |
|    816 | 4441 | `		zIn++; /* Jump the precent sign */` |
|    407 | 4442 | `		do{` |
|   1000 | 4443 | `			c = zIn[0];` |
|   1000 | 4444 | `			switch( c ){` |
|     15 | 4445 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      7 | 4446 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 4447 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|    159 | 4448 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 4449 | `			case '\'':` |
|    ! 0 | 4450 | `				zIn++;` |
|    ! 0 | 4451 | `				if( zIn < zEnd ){` |
|      - | 4452 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    ! 0 | 4453 | `					c = zIn[0];` |
|    ! 0 | 4454 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4455 | `						spaces[idx] = (char)c;` |
|    ! 0 | 4456 | `					}` |
|    ! 0 | 4457 | `					c = 0;` |
|    ! 0 | 4458 | `				}` |
|    ! 0 | 4459 | `				break;` |
|    814 | 4460 | `			default:                                       break;` |
|      - | 4461 | `			}` |
|   1000 | 4462 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 4463 | `		/* Get the field width */` |
|    816 | 4464 | `		width = 0;` |
|   1439 | 4465 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    217 | 4466 | `			width = width*10 + (zIn[0] - '0');` |
|    217 | 4467 | `			zIn++;` |
|      1 | 4468 | `		}` |
|    816 | 4469 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 4470 | `			/* Position specifer */` |
|    ! 0 | 4471 | `			if( width > 0 ){` |
|    ! 0 | 4472 | `				n = width;` |
|    ! 0 | 4473 | `				if( vf && n > 0 ){` |
|    ! 0 | 4474 | `					n--;` |
|    ! 0 | 4475 | `				}` |
|    ! 0 | 4476 | `			}` |
|    ! 0 | 4477 | `			zIn++;` |
|    ! 0 | 4478 | `			width = 0;` |
|      - | 4479 | `			/* php's grammar is %argnum$<flags><width>: the flags come AFTER the` |
|      - | 4480 | `			 * positional, so re-parse the full flag set here (e.g. "%1$-10s"),` |
|      - | 4481 | `			 * not just zero-padding. */` |
|    ! 0 | 4482 | `			do{` |
|    ! 0 | 4483 | `				c = zIn[0];` |
|    ! 0 | 4484 | `				switch( c ){` |
|    ! 0 | 4485 | `				case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|    ! 0 | 4486 | `				case '+':   flag_plussign = 1;        c = 0;   break;` |
|    ! 0 | 4487 | `				case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|    ! 0 | 4488 | `				case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 4489 | `				case '\'':` |
|    ! 0 | 4490 | `					zIn++;` |
|    ! 0 | 4491 | `					if( zIn < zEnd ){` |
|    ! 0 | 4492 | `						c = zIn[0];` |
|    ! 0 | 4493 | `						for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4494 | `							spaces[idx] = (char)c;` |
|    ! 0 | 4495 | `						}` |
|    ! 0 | 4496 | `						c = 0;` |
|    ! 0 | 4497 | `					}` |
|    ! 0 | 4498 | `					break;` |
|    ! 0 | 4499 | `				default:                                       break;` |
|      - | 4500 | `				}` |
|    ! 0 | 4501 | `			}while( c==0 && (zIn++ < zEnd) );` |
|    ! 0 | 4502 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    ! 0 | 4503 | `				width = width*10 + (zIn[0] - '0');` |
|    ! 0 | 4504 | `				zIn++;` |
|    ! 0 | 4505 | `			}` |
|    ! 0 | 4506 | `		}` |
|    816 | 4507 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 4508 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 4509 | `		}` |
|      - | 4510 | `		/* Get the precision */` |
|    816 | 4511 | `		precision = -1;` |
|    816 | 4512 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|    100 | 4513 | `			precision = 0;` |
|    100 | 4514 | `			zIn++;` |
|    257 | 4515 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    110 | 4516 | `				precision = precision*10 + (zIn[0] - '0');` |
|    110 | 4517 | `				zIn++;` |
|      2 | 4518 | `			}` |
|     49 | 4519 | `		}` |
|      - | 4520 | `		/* Consume a single 'l' length modifier (a C-ism php accepts and ignores,` |
|      - | 4521 | `		 * e.g. "%ld"); PH7_FormatValidate mirrors this. Exactly one is skipped:` |
|      - | 4522 | `		 * in "%lld" the second 'l' becomes the (unknown) specifier, just like php. */` |
|    816 | 4523 | `		if( zIn < zEnd && zIn[0] == 'l' ){` |
|      9 | 4524 | `			zIn++;` |
|      4 | 4525 | `		}` |
|    816 | 4526 | `		if( zIn >= zEnd ){` |
|      - | 4527 | `			/* No more input */` |
|      3 | 4528 | `			break;` |
|      - | 4529 | `		}` |
|      - | 4530 | `		/* Fetch the info entry for the field */` |
|    814 | 4531 | `		pInfo = 0;` |
|    814 | 4532 | `		xtype = PH7_FMT_ERROR;` |
|    814 | 4533 | `		c = zIn[0];` |
|    814 | 4534 | `		zIn++; /* Jump the format specifer */` |
|   3162 | 4535 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|   3162 | 4536 | `			if( c==aFmt[idx].fmttype ){` |
|    814 | 4537 | `				pInfo = &aFmt[idx];` |
|    814 | 4538 | `				xtype = pInfo->type;` |
|    814 | 4539 | `				break;` |
|      - | 4540 | `			}` |
|   1176 | 4541 | `		}` |
|    814 | 4542 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    814 | 4543 | `		length = 0;` |
|      - | 4544 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 4545 | `		 /*` |
|      - | 4546 | `		  ** At this point, variables are initialized as follows:` |
|      - | 4547 | `		  **` |
|      - | 4548 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 4549 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 4550 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 4551 | `		  **                               field width was negative.` |
|      - | 4552 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 4553 | `		  **                               the conversion character.` |
|      - | 4554 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 4555 | `		  **   width                       The specified field width.  This is` |
|      - | 4556 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 4557 | `		  **   precision                   The specified precision.  The default` |
|      - | 4558 | `		  **                               is -1.` |
|      - | 4559 | `		  */` |
|    814 | 4560 | `		switch(xtype){` |
|      3 | 4561 | `		case PH7_FMT_PERCENT:` |
|      - | 4562 | `			/* A literal percent character */` |
|      7 | 4563 | `			zWorker[0] = '%';` |
|      7 | 4564 | `			length = (int)sizeof(char);` |
|      7 | 4565 | `			break;` |
|      3 | 4566 | `		case PH7_FMT_CHARX:` |
|      - | 4567 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 4568 | `			 * with that ASCII value` |
|      - | 4569 | `			 */` |
|      7 | 4570 | `			pArg = NEXT_ARG;` |
|      7 | 4571 | `			if( pArg == 0 ){` |
|      3 | 4572 | `				c = 0;` |
|      2 | 4573 | `			}else{` |
|      5 | 4574 | `				c = ph7_value_to_int(pArg);` |
|      - | 4575 | `			}` |
|      - | 4576 | `			/* NUL byte is an acceptable value */` |
|      7 | 4577 | `			zWorker[0] = (char)c;` |
|      7 | 4578 | `			length = (int)sizeof(char);` |
|      7 | 4579 | `			break;` |
|    170 | 4580 | `		case PH7_FMT_STRING:` |
|      - | 4581 | `			/* the argument is treated as and presented as a string */` |
|    341 | 4582 | `			pArg = NEXT_ARG;` |
|    341 | 4583 | `			if( pArg == 0 ){` |
|    ! 0 | 4584 | `				length = 0;` |
|    ! 0 | 4585 | `			}else{` |
|    341 | 4586 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 4587 | `			}` |
|    341 | 4588 | `			if( length < 1 ){` |
|      - | 4589 | `				/* An empty %s substitutes NOTHING in php. PH7 substituted a single` |
|      - | 4590 | `				 * SPACE here, so printf("[%s]","") printed "[ ]" and any format with an` |
|      - | 4591 | `				 * absent optional part gained a stray space. */` |
|      9 | 4592 | `				zBuf = "";` |
|      9 | 4593 | `				length = 0;` |
|      4 | 4594 | `			}` |
|    341 | 4595 | `			if( precision>=0 && precision<length ){` |
|      3 | 4596 | `				length = precision;` |
|      1 | 4597 | `			}` |
|    341 | 4598 | `			if( flag_zeropad ){` |
|      - | 4599 | `				/* zero-padding works on strings too */` |
|    ! 0 | 4600 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4601 | `					spaces[idx] = '0';` |
|    ! 0 | 4602 | `				}` |
|    ! 0 | 4603 | `			}` |
|    341 | 4604 | `			break;` |
|    136 | 4605 | `		case PH7_FMT_RADIX:` |
|    273 | 4606 | `			pArg = NEXT_ARG;` |
|    273 | 4607 | `			if( pArg == 0 ){` |
|    ! 0 | 4608 | `				iVal = 0;` |
|    ! 0 | 4609 | `			}else{` |
|    273 | 4610 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 4611 | `			}` |
|      - | 4612 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|    273 | 4613 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 4614 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 4615 | `			}` |
|      - | 4616 | `#if 1` |
|      - | 4617 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 4618 | `        ** I think this is stupid.*/` |
|    273 | 4619 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 4620 | `#else` |
|      - | 4621 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 4622 | `        ** but leave the prefix for hex.*/` |
|      - | 4623 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 4624 | `#endif` |
|    273 | 4625 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|    249 | 4626 | `          if( iVal<0 ){` |
|     25 | 4627 | `            iVal = -iVal;` |
|      - | 4628 | `			/* Ticket 1433-003 */` |
|     25 | 4629 | `			if( iVal < 0 ){` |
|      - | 4630 | `				/* Overflow */` |
|    ! 0 | 4631 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4632 | `			}` |
|     25 | 4633 | `            prefix = '-';` |
|    237 | 4634 | `          }else if( flag_plussign )  prefix = '+';` |
|    223 | 4635 | `          else if( flag_blanksign )  prefix = ' ';` |
|    221 | 4636 | `          else                       prefix = 0;` |
|    125 | 4637 | `        }else{` |
|     25 | 4638 | `			if( iVal<0 ){` |
|    ! 0 | 4639 | `				iVal = -iVal;` |
|      - | 4640 | `				/* Ticket 1433-003 */` |
|    ! 0 | 4641 | `				if( iVal < 0 ){` |
|      - | 4642 | `					/* Overflow */` |
|    ! 0 | 4643 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4644 | `				}` |
|    ! 0 | 4645 | `			}` |
|     25 | 4646 | `			prefix = 0;` |
|      - | 4647 | `		}` |
|    273 | 4648 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|    149 | 4649 | `          precision = width-(prefix!=0);` |
|     74 | 4650 | `        }` |
|    273 | 4651 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 4652 | `        {` |
|      - | 4653 | `          register char *cset;      /* Use registers for speed */` |
|      - | 4654 | `          register int base;` |
|    273 | 4655 | `          cset = pInfo->charset;` |
|    273 | 4656 | `          base = pInfo->base;` |
|    136 | 4657 | `          do{                                           /* Convert to ascii */` |
|    349 | 4658 | `            *(--zBuf) = cset[iVal%base];` |
|    349 | 4659 | `            iVal = iVal/base;` |
|    349 | 4660 | `          }while( iVal>0 );` |
|      - | 4661 | `        }` |
|    273 | 4662 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    439 | 4663 | `        for(idx=precision-length; idx>0; idx--){` |
|    167 | 4664 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|     84 | 4665 | `        }` |
|    273 | 4666 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|    273 | 4667 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 4668 | `          char *pre, x;` |
|    ! 0 | 4669 | `          pre = pInfo->prefix;` |
|    ! 0 | 4670 | `          if( *zBuf!=pre[0] ){` |
|    ! 0 | 4671 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|    ! 0 | 4672 | `          }` |
|    ! 0 | 4673 | `        }` |
|    273 | 4674 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    273 | 4675 | `		break;` |
|     94 | 4676 | `		case PH7_FMT_FLOAT:` |
|      - | 4677 | `		case PH7_FMT_EXP:` |
|      - | 4678 | `		case PH7_FMT_GENERIC:{` |
|      - | 4679 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 4680 | `		double realvalue;` |
|      - | 4681 | `		char zFmt[8];` |
|      - | 4682 | `		int nOut, nFmt;` |
|    190 | 4683 | `		pArg = NEXT_ARG;` |
|    190 | 4684 | `		if( pArg == 0 ){` |
|    ! 0 | 4685 | `			realvalue = 0;` |
|    ! 0 | 4686 | `		}else{` |
|    190 | 4687 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 4688 | `		}` |
|      - | 4689 | `		/* php prints the IEEE specials bare — NaN / INF / -INF with no width` |
|      - | 4690 | `		 * padding, precision, or sign flags (php_sprintf_appenddouble). */` |
|    190 | 4691 | `		if( PH7_IS_NAN(realvalue) ){` |
|     21 | 4692 | `			zBuf = "NaN";` |
|     21 | 4693 | `			length = 3;` |
|     21 | 4694 | `			width = 0;` |
|     21 | 4695 | `			break;` |
|      - | 4696 | `		}` |
|    170 | 4697 | `		if( PH7_IS_INF(realvalue) ){` |
|     37 | 4698 | `			if( realvalue < 0.0 ){` |
|     15 | 4699 | `				zBuf = "-INF";` |
|     15 | 4700 | `				length = 4;` |
|      8 | 4701 | `			}else{` |
|     23 | 4702 | `				zBuf = "INF";` |
|     23 | 4703 | `				length = 3;` |
|      - | 4704 | `			}` |
|     37 | 4705 | `			width = 0;` |
|     37 | 4706 | `			break;` |
|      - | 4707 | `		}` |
|    134 | 4708 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|    134 | 4709 | `		if( precision > 53 ){` |
|      - | 4710 | `			/* php's FORMAT_CONV_MAX_PRECISION cap, with the same E_NOTICE` |
|      - | 4711 | `			 * (message prefixed with the active function's name, like` |
|      - | 4712 | `			 * php_error_docref). */` |
|      - | 4713 | `			char zMsg[160];` |
|      4 | 4714 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - | 4715 | `				"%z(): Requested precision of %d digits was truncated to PHP maximum of %d digits",` |
|      2 | 4716 | `				&pCtx->pFunc->sName,precision,53);` |
|      3 | 4717 | `			PH7_VmThrowError(pCtx->pVm,0,E_NOTICE,zMsg);` |
|      3 | 4718 | `			precision = 53;` |
|      1 | 4719 | `		}` |
|      - | 4720 | ``		/* php's %f/%e extract the sign via `num < 0`, so negative zero prints`` |
|      - | 4721 | `		 * unsigned there — while %g (php_gcvt on the raw value) keeps "-0". */` |
|    134 | 4722 | `		if( xtype!=PH7_FMT_GENERIC && realvalue == 0.0 ){` |
|      9 | 4723 | `			realvalue = 0.0;` |
|      4 | 4724 | `		}` |
|      - | 4725 | `		/* php's float conversions are correctly rounded (zend_dtoa); use libc` |
|      - | 4726 | `		 * snprintf as the digit engine (the byte-exact-floats rule — the old` |
|      - | 4727 | `		 * hand-rolled vxGetdigit loop stopped at 16 significant digits, so` |
|      - | 4728 | `		 * e.g. %f of 1e308 printed zeros where php prints the exact binary64` |
|      - | 4729 | `		 * expansion), then post-process into php's exact shapes below. */` |
|    134 | 4730 | `		nFmt = 0;` |
|    134 | 4731 | `		zFmt[nFmt++] = '%';` |
|    134 | 4732 | `		if( flag_alternateform ) zFmt[nFmt++] = '#';` |
|      - | 4733 | `		/* php's ' ' flag selects space PADDING (its default), not C's` |
|      - | 4734 | `		 * space-for-positive-sign — so flag_blanksign is NOT forwarded. */` |
|    134 | 4735 | `		if( flag_plussign ) zFmt[nFmt++] = '+';` |
|    134 | 4736 | `		zFmt[nFmt++] = '.';` |
|    134 | 4737 | `		zFmt[nFmt++] = '*';` |
|    178 | 4738 | `		zFmt[nFmt++] = (char)(xtype==PH7_FMT_FLOAT ? 'f' :` |
|     30 | 4739 | `			(xtype==PH7_FMT_EXP ? ((pInfo->charset[0]=='E') ? 'E' : 'e')` |
|     28 | 4740 | `			                    : ((pInfo->charset[0]=='E') ? 'G' : 'g')));` |
|    134 | 4741 | `		zFmt[nFmt] = 0;` |
|    134 | 4742 | `		nOut = snprintf(zWorker,sizeof(zWorker),zFmt,precision,realvalue);` |
|    134 | 4743 | `		if( nOut < 0 \|\| nOut >= (int)sizeof(zWorker) ){` |
|      - | 4744 | `			/* Cannot happen with precision capped at 53 (%f of DBL_MAX is` |
|      - | 4745 | `			 * ~365 bytes); keep the truncated output rather than overrun. */` |
|    ! 0 | 4746 | `			nOut = (int)SyStrlen(zWorker);` |
|    ! 0 | 4747 | `		}` |
|    134 | 4748 | `		nOut = (int)PH7_PhpFloatShape(zWorker,(sxi32)nOut,xtype==PH7_FMT_GENERIC);` |
|    134 | 4749 | `		zBuf = zWorker;` |
|    134 | 4750 | `		length = nOut;` |
|      - | 4751 | `		/* Let the zero-pad block below insert zeros between the sign (written` |
|      - | 4752 | `		 * by snprintf) and the first digit, as before. */` |
|    134 | 4753 | `		prefix = (zWorker[0]=='-' \|\| zWorker[0]=='+' \|\| zWorker[0]==' ') ? zWorker[0] : 0;` |
|      - | 4754 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 4755 | `        ** set and we are not left justified */` |
|    134 | 4756 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 4757 | `          int i;` |
|      7 | 4758 | `          int nPad = width - length;` |
|     51 | 4759 | `          for(i=width; i>=nPad; i--){` |
|     45 | 4760 | `            zBuf[i] = zBuf[i-nPad];` |
|     23 | 4761 | `          }` |
|      7 | 4762 | `          i = prefix!=0;` |
|     29 | 4763 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      7 | 4764 | `          length = width;` |
|      3 | 4765 | `        }` |
|      - | 4766 | `#else` |
|      - | 4767 | `         zBuf = " ";` |
|      - | 4768 | `		 length = (int)sizeof(char);` |
|      - | 4769 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|    134 | 4770 | `		 break;` |
|      - | 4771 | `							 }` |
|    ! 0 | 4772 | `		default:` |
|      - | 4773 | `			/* Unreachable: PH7_FormatValidate() rejects unknown specifiers with a` |
|      - | 4774 | `			 * catchable ValueError before formatting begins. Kept as a defensive` |
|      - | 4775 | `			 * no-op that emits nothing. */` |
|    ! 0 | 4776 | `			length = 0;` |
|    ! 0 | 4777 | `			break;` |
|      - | 4778 | `		}` |
|      - | 4779 | `		 /*` |
|      - | 4780 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 4781 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 4782 | `		 ** the output.` |
|      - | 4783 | `		 */` |
|    814 | 4784 | `    if( !flag_leftjustify ){` |
|      - | 4785 | `      register int nspace;` |
|    800 | 4786 | `      nspace = width-length;` |
|    800 | 4787 | `      if( nspace>0 ){` |
|      7 | 4788 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4789 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4790 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4791 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4792 | `			}` |
|    ! 0 | 4793 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4794 | `        }` |
|      7 | 4795 | `        if( nspace>0 ){` |
|      7 | 4796 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      7 | 4797 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4798 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4799 | `			}` |
|      3 | 4800 | `		}` |
|      3 | 4801 | `      }` |
|    399 | 4802 | `    }` |
|    814 | 4803 | `    if( length>0 ){` |
|    806 | 4804 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    806 | 4805 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 4806 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4807 | `		}` |
|    402 | 4808 | `    }` |
|    814 | 4809 | `    if( flag_leftjustify ){` |
|      - | 4810 | `      register int nspace;` |
|     15 | 4811 | `      nspace = width-length;` |
|     15 | 4812 | `      if( nspace>0 ){` |
|     11 | 4813 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4814 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4815 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4816 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4817 | `			}` |
|    ! 0 | 4818 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4819 | `        }` |
|     11 | 4820 | `        if( nspace>0 ){` |
|     11 | 4821 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|     11 | 4822 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4823 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4824 | `			}` |
|      5 | 4825 | `		}` |
|      5 | 4826 | `      }` |
|      7 | 4827 | `    }` |
|      2 | 4828 | ` }/* for(;;) */` |
|    412 | 4829 | `	return SXRET_OK;` |
|    207 | 4830 | `}` |
|      - | 4831 | `/*` |
|      - | 4832 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 4833 | ` */` |
|    364 | 4834 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      2 | 4835 | `{` |
|      - | 4836 | `	/* pUserData points to the caller's allocation-rc slot so an OOM during the` |
|      - | 4837 | `	 * result append is surfaced (the builtin raises a fatal); returning the` |
|      - | 4838 | `	 * non-OK rc also stops the format loop. */` |
|    366 | 4839 | `	sxi32 *pRc = (sxi32 *)pUserData;` |
|    366 | 4840 | `	*pRc = ph7_result_string(pCtx,zInput,nLen);` |
|    366 | 4841 | `	return *pRc;` |
|      2 | 4842 | `}` |
|      - | 4843 | `/*` |
|      - | 4844 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 4845 | ` *  Return a formatted string.` |
|      - | 4846 | ` * Parameters` |
|      - | 4847 | ` *  $format` |
|      - | 4848 | ` *    The format string (see block comment above)` |
|      - | 4849 | ` * Return` |
|      - | 4850 | ` *  A string produced according to the formatting string format.` |
|      - | 4851 | ` */` |
|    196 | 4852 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4853 | `{` |
|      - | 4854 | `	const char *zFormat;` |
|    198 | 4855 | `	sxi32 rc = SXRET_OK;` |
|      - | 4856 | `	int nLen;` |
|    198 | 4857 | `	if( nArg < 1 ){` |
|      - | 4858 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4859 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4860 | `		return PH7_OK;` |
|      - | 4861 | `	}` |
|      - | 4862 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|    198 | 4863 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    198 | 4864 | `	if( rc != PH7_OK ){` |
|    ! 0 | 4865 | `		return rc;` |
|      - | 4866 | `	}` |
|      - | 4867 | `	/* Extract the string format (scalars/null coerce). */` |
|    198 | 4868 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    198 | 4869 | `	if( nLen < 1 ){` |
|      - | 4870 | `		/* Empty string */` |
|    ! 0 | 4871 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4872 | `		return PH7_OK;` |
|      - | 4873 | `	}` |
|      - | 4874 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4875 | `	 * output; propagate the throw status verbatim. */` |
|    198 | 4876 | `	rc = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    198 | 4877 | `	if( rc != PH7_OK ){` |
|     17 | 4878 | `		return rc;` |
|      - | 4879 | `	}` |
|      - | 4880 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|    182 | 4881 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&rc,FALSE);` |
|    182 | 4882 | `	if( rc != SXRET_OK ){` |
|      - | 4883 | `		/* The result append ran out of memory: raise a fatal rather than` |
|      - | 4884 | `		 * returning a silently-truncated string. */` |
|    ! 0 | 4885 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4886 | `	}` |
|    182 | 4887 | `	return PH7_OK;` |
|    100 | 4888 | `}` |
|      - | 4889 | `/*` |
|      - | 4890 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 4891 | ` */` |
|   1174 | 4892 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4893 | `{` |
|   1175 | 4894 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 4895 | `	/* Call the VM output consumer directly */` |
|   1175 | 4896 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 4897 | `	/* Increment counter */` |
|   1175 | 4898 | `	*pCounter += nLen;` |
|   1175 | 4899 | `	return PH7_OK;` |
|      1 | 4900 | `}` |
|      - | 4901 | `/*` |
|      - | 4902 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 4903 | ` *  Output a formatted string.` |
|      - | 4904 | ` * Parameters` |
|      - | 4905 | ` *  $format` |
|      - | 4906 | ` *   See sprintf() for a description of format.` |
|      - | 4907 | ` * Return` |
|      - | 4908 | ` *  The length of the outputted string.` |
|      - | 4909 | ` */` |
|    204 | 4910 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4911 | `{` |
|    205 | 4912 | `	ph7_int64 nCounter = 0;` |
|      - | 4913 | `	const char *zFormat;` |
|      - | 4914 | `	int nLen;` |
|    205 | 4915 | `	if( nArg < 1 ){` |
|      - | 4916 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 4917 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4918 | `		return PH7_OK;` |
|      - | 4919 | `	}` |
|      - | 4920 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|      - | 4921 | `	{` |
|    205 | 4922 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    205 | 4923 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 4924 | `			return rcf;` |
|      - | 4925 | `		}` |
|      - | 4926 | `	}` |
|      - | 4927 | `	/* Extract the string format (scalars/null coerce). */` |
|    205 | 4928 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    205 | 4929 | `	if( nLen < 1 ){` |
|      - | 4930 | `		/* Empty string */` |
|    ! 0 | 4931 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4932 | `		return PH7_OK;` |
|      - | 4933 | `	}` |
|      - | 4934 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4935 | `	 * output; propagate the throw status verbatim. */` |
|      - | 4936 | `	{` |
|    205 | 4937 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    205 | 4938 | `		if( rcv != PH7_OK ){` |
|    ! 0 | 4939 | `			return rcv;` |
|      - | 4940 | `		}` |
|      - | 4941 | `	}` |
|      - | 4942 | `	/* Format the string */` |
|    205 | 4943 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 4944 | `	/* Return the length of the outputted string */` |
|    205 | 4945 | `	ph7_result_int64(pCtx,nCounter);` |
|    205 | 4946 | `	return PH7_OK;` |
|    103 | 4947 | `}` |
|      - | 4948 | `/*` |
|      - | 4949 | ` * int vprintf(string $format,array $args)` |
|      - | 4950 | ` *  Output a formatted string.` |
|      - | 4951 | ` * Parameters` |
|      - | 4952 | ` *  $format` |
|      - | 4953 | ` *   See sprintf() for a description of format.` |
|      - | 4954 | ` * Return` |
|      - | 4955 | ` *  The length of the outputted string.` |
|      - | 4956 | ` */` |
|      4 | 4957 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4958 | `{` |
|      5 | 4959 | `	ph7_int64 nCounter = 0;` |
|      - | 4960 | `	const char *zFormat;` |
|      - | 4961 | `	ph7_hashmap *pMap;` |
|      - | 4962 | `	SySet sArg;` |
|      - | 4963 | `	int nLen,n;` |
|      - | 4964 | `	sxi32 rcFmt;` |
|      5 | 4965 | `	if( nArg < 2 ){` |
|      - | 4966 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 4967 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4968 | `		return PH7_OK;` |
|      - | 4969 | `	}` |
|      - | 4970 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|      5 | 4971 | `	rcFmt = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|      5 | 4972 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 4973 | `		return rcFmt;` |
|      - | 4974 | `	}` |
|      5 | 4975 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 4976 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 4977 | `		char zBuf[64];` |
|      4 | 4978 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4979 | `			"vprintf(): Argument #2 ($values) must be of type array, %s given",` |
|      2 | 4980 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 4981 | `	}` |
|      - | 4982 | `	/* Extract the string format (scalars/null coerce). */` |
|      3 | 4983 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4984 | `	if( nLen < 1 ){` |
|      - | 4985 | `		/* Empty string */` |
|    ! 0 | 4986 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4987 | `		return PH7_OK;` |
|      - | 4988 | `	}` |
|      - | 4989 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4990 | `	 * output; propagate the throw status verbatim. */` |
|      3 | 4991 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      3 | 4992 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 4993 | `		return rcFmt;` |
|      - | 4994 | `	}` |
|      - | 4995 | `	/* Point to the hashmap */` |
|      3 | 4996 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4997 | `	/* Extract arguments from the hashmap */` |
|      3 | 4998 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4999 | `	/* Format the string */` |
|      3 | 5000 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 5001 | `	/* Release the container */` |
|      3 | 5002 | `	SySetRelease(&sArg);` |
|      - | 5003 | `	/* Return the length of the outputted string */` |
|      3 | 5004 | `	ph7_result_int64(pCtx,nCounter);` |
|      3 | 5005 | `	return PH7_OK;` |
|      3 | 5006 | `}` |
|      - | 5007 | `/*` |
|      - | 5008 | ` * int vsprintf(string $format,array $args)` |
|      - | 5009 | ` *  Output a formatted string.` |
|      - | 5010 | ` * Parameters` |
|      - | 5011 | ` *  $format` |
|      - | 5012 | ` *   See sprintf() for a description of format.` |
|      - | 5013 | ` * Return` |
|      - | 5014 | ` *  A string produced according to the formatting string format.` |
|      - | 5015 | ` */` |
|     18 | 5016 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5017 | `{` |
|      - | 5018 | `	const char *zFormat;` |
|      - | 5019 | `	ph7_hashmap *pMap;` |
|      - | 5020 | `	SySet sArg;` |
|     19 | 5021 | `	sxi32 rc = SXRET_OK;` |
|      - | 5022 | `	sxi32 rcFmt;` |
|      - | 5023 | `	int nLen,n;` |
|     19 | 5024 | `	if( nArg < 2 ){` |
|      - | 5025 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5026 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5027 | `		return PH7_OK;` |
|      - | 5028 | `	}` |
|      - | 5029 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|     19 | 5030 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|     19 | 5031 | `	if( rc != PH7_OK ){` |
|    ! 0 | 5032 | `		return rc;` |
|      - | 5033 | `	}` |
|     19 | 5034 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 5035 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 5036 | `		char zBuf[64];` |
|     16 | 5037 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5038 | `			"vsprintf(): Argument #2 ($values) must be of type array, %s given",` |
|     10 | 5039 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 5040 | `	}` |
|      - | 5041 | `	/* Extract the string format (scalars/null coerce). */` |
|      9 | 5042 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 5043 | `	if( nLen < 1 ){` |
|      - | 5044 | `		/* Empty string */` |
|    ! 0 | 5045 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5046 | `		return PH7_OK;` |
|      - | 5047 | `	}` |
|      - | 5048 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 5049 | `	 * output; propagate the throw status verbatim. */` |
|      9 | 5050 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      9 | 5051 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 5052 | `		return rcFmt;` |
|      - | 5053 | `	}` |
|      - | 5054 | `	/* Point to hashmap */` |
|      9 | 5055 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 5056 | `	/* Extract arguments from the hashmap */` |
|      9 | 5057 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 5058 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|      9 | 5059 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&rc,TRUE);` |
|      - | 5060 | `	/* Release the container */` |
|      9 | 5061 | `	SySetRelease(&sArg);` |
|      9 | 5062 | `	if( rc != SXRET_OK ){` |
|      - | 5063 | `		/* The result append ran out of memory: raise a fatal. */` |
|    ! 0 | 5064 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5065 | `	}` |
|      9 | 5066 | `	return PH7_OK;` |
|     10 | 5067 | `}` |
|      - | 5068 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 5069 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 5070 | `/*` |
|      - | 5071 | ` * Symisc eXtension.` |
|      - | 5072 | ` * string size_format(int64 $size)` |
|      - | 5073 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 5074 | ` *  Example:` |
|      - | 5075 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 5076 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 5077 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 5078 | ` * Parameter` |
|      - | 5079 | ` *  $size` |
|      - | 5080 | ` *    Entity size in bytes.` |
|      - | 5081 | ` * Return` |
|      - | 5082 | ` *   Formatted string representation of the given size.` |
|      - | 5083 | ` */` |
|     24 | 5084 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5085 | `{` |
|      - | 5086 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 5087 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 5088 | `	sxi32 nRest,i_32;` |
|      - | 5089 | `	ph7_int64 iSize;` |
|     25 | 5090 | `	int c = -1; /* index in zUnit[] */` |
|      - | 5091 |  |
|     25 | 5092 | `	if( nArg < 1 ){` |
|      - | 5093 | `		/* Missing argument,return the empty string */` |
|      3 | 5094 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5095 | `		return PH7_OK;` |
|      - | 5096 | `	}` |
|      - | 5097 | `	/* Extract the given size */` |
|     23 | 5098 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 5099 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 5100 | `		/* Don't bother formatting,return immediately */` |
|      5 | 5101 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 5102 | `		return PH7_OK;` |
|      - | 5103 | `	}` |
|     19 | 5104 | `	for(;;){` |
|     39 | 5105 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 5106 | `		iSize >>= 10;` |
|     39 | 5107 | `		c++;` |
|     39 | 5108 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 5109 | `			break;` |
|      - | 5110 | `		}` |
|      1 | 5111 | `	}` |
|     19 | 5112 | `	nRest /= 100;` |
|     19 | 5113 | `	if( nRest > 9 ){` |
|    ! 0 | 5114 | `		nRest = 9;` |
|    ! 0 | 5115 | `	}` |
|     19 | 5116 | `	if( iSize > 999 ){` |
|    ! 0 | 5117 | `		c++;` |
|    ! 0 | 5118 | `		nRest = 9;` |
|    ! 0 | 5119 | `		iSize = 0;` |
|    ! 0 | 5120 | `	}` |
|     19 | 5121 | `	i_32 = (sxi32)iSize;` |
|      - | 5122 | `	/* Format */` |
|     19 | 5123 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 5124 | `	return PH7_OK;` |
|     13 | 5125 | `}` |
|      - | 5126 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 5127 | `/*` |
|      - | 5128 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 5129 | ` *   Calculate the md5 hash of a string.` |
|      - | 5130 | ` * Parameter` |
|      - | 5131 | ` *  $str` |
|      - | 5132 | ` *   Input string` |
|      - | 5133 | ` * $raw_output` |
|      - | 5134 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 5135 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 5136 | ` * Return` |
|      - | 5137 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 5138 | ` */` |
|     12 | 5139 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5140 | `{` |
|      - | 5141 | `	unsigned char zDigest[16];` |
|     13 | 5142 | `	int raw_output = FALSE;` |
|      - | 5143 | `	const void *pIn;` |
|      - | 5144 | `	int nLen;` |
|     13 | 5145 | `	if( nArg < 1 ){` |
|      - | 5146 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5147 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5148 | `		return PH7_OK;` |
|      - | 5149 | `	}` |
|      - | 5150 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 5151 | `	 * digest in PHP — d41d8cd9… — so it must NOT short-circuit). */` |
|     13 | 5152 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 5153 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 5154 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 5155 | `	}` |
|      - | 5156 | `	/* Compute the MD5 digest */` |
|     13 | 5157 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|     13 | 5158 | `	if( raw_output ){` |
|      - | 5159 | `		/* Output raw digest */` |
|      5 | 5160 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 5161 | `	}else{` |
|      - | 5162 | `		/* Perform a binary to hex conversion */` |
|      9 | 5163 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 5164 | `	}` |
|     13 | 5165 | `	return PH7_OK;` |
|      7 | 5166 | `}` |
|      - | 5167 | `/*` |
|      - | 5168 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 5169 | ` *   Calculate the sha1 hash of a string.` |
|      - | 5170 | ` * Parameter` |
|      - | 5171 | ` *  $str` |
|      - | 5172 | ` *   Input string` |
|      - | 5173 | ` * $raw_output` |
|      - | 5174 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 5175 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 5176 | ` * Return` |
|      - | 5177 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 5178 | ` */` |
|     10 | 5179 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5180 | `{` |
|      - | 5181 | `	unsigned char zDigest[20];` |
|     11 | 5182 | `	int raw_output = FALSE;` |
|      - | 5183 | `	const void *pIn;` |
|      - | 5184 | `	int nLen;` |
|     11 | 5185 | `	if( nArg < 1 ){` |
|      - | 5186 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5187 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5188 | `		return PH7_OK;` |
|      - | 5189 | `	}` |
|      - | 5190 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 5191 | `	 * digest in PHP — da39a3ee… — so it must NOT short-circuit). */` |
|     11 | 5192 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 5193 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 5194 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 5195 | `	}` |
|      - | 5196 | `	/* Compute the SHA1 digest */` |
|     11 | 5197 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|     11 | 5198 | `	if( raw_output ){` |
|      - | 5199 | `		/* Output raw digest */` |
|      5 | 5200 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 5201 | `	}else{` |
|      - | 5202 | `		/* Perform a binary to hex conversion */` |
|      7 | 5203 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 5204 | `	}` |
|     11 | 5205 | `	return PH7_OK;` |
|      6 | 5206 | `}` |
|      - | 5207 | `/*` |
|      - | 5208 | ` * int64 crc32(string $str)` |
|      - | 5209 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 5210 | ` * Parameter` |
|      - | 5211 | ` *  $str` |
|      - | 5212 | ` *   Input string` |
|      - | 5213 | ` * Return` |
|      - | 5214 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 5215 | ` */` |
|      2 | 5216 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5217 | `{` |
|      - | 5218 | `	const void *pIn;` |
|      - | 5219 | `	sxu32 nCRC;` |
|      - | 5220 | `	int nLen;` |
|      3 | 5221 | `	if( nArg < 1 ){` |
|      - | 5222 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 5223 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5224 | `		return PH7_OK;` |
|      - | 5225 | `	}` |
|      - | 5226 | `	/* Extract the input string */` |
|      3 | 5227 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5228 | `	if( nLen < 1 ){` |
|      - | 5229 | `		/* crc32("") is 0 in PHP, so this short-circuit is correct here — unlike` |
|      - | 5230 | `		 * md5()/sha1(), whose empty-string digests are non-zero. */` |
|    ! 0 | 5231 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5232 | `		return PH7_OK;` |
|      - | 5233 | `	}` |
|      - | 5234 | `	/* Calculate the sum */` |
|      3 | 5235 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 5236 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 5237 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 5238 | `	return PH7_OK;` |
|      2 | 5239 | `}` |
|      - | 5240 | `/*` |
|      - | 5241 | ` * The hash() family (hash/hash_hmac/hash_equals/hash_algos). Each algorithm is` |
|      - | 5242 | ` * described by a small record so one dispatch (and one generic HMAC) serves them` |
|      - | 5243 | ` * all. Thin adapters normalize the differing context types and the reversed` |
|      - | 5244 | ` * MD5Final argument order behind a uniform Init/Update/Final over a HashCtx union.` |
|      - | 5245 | ` */` |
|     11 | 5246 | `static void HashMd5Init(HashCtx *c){ MD5Init(&c->md5); }` |
|     15 | 5247 | `static void HashMd5Update(HashCtx *c,const unsigned char *d,unsigned int n){ MD5Update(&c->md5,d,n); }` |
|     11 | 5248 | `static void HashMd5Final(HashCtx *c,unsigned char *o){ MD5Final(o,&c->md5); }` |
|     11 | 5249 | `static void HashSha1Init(HashCtx *c){ SHA1Init(&c->sha1); }` |
|     15 | 5250 | `static void HashSha1Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA1Update(&c->sha1,d,n); }` |
|     11 | 5251 | `static void HashSha1Final(HashCtx *c,unsigned char *o){ SHA1Final(&c->sha1,o); }` |
|      9 | 5252 | `static void HashSha224Init(HashCtx *c){ SHA224Init(&c->sha256); }` |
|     33 | 5253 | `static void HashSha256Init(HashCtx *c){ SHA256Init(&c->sha256); }` |
|     57 | 5254 | `static void HashSha256Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA256Update(&c->sha256,d,n); }` |
|     41 | 5255 | `static void HashSha256Final(HashCtx *c,unsigned char *o){ SHA256Final(&c->sha256,o); }` |
|      9 | 5256 | `static void HashSha384Init(HashCtx *c){ SHA384Init(&c->sha512); }` |
|     15 | 5257 | `static void HashSha512Init(HashCtx *c){ SHA512Init(&c->sha512); }` |
|     27 | 5258 | `static void HashSha512Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA512Update(&c->sha512,d,n); }` |
|     23 | 5259 | `static void HashSha512Final(HashCtx *c,unsigned char *o){ SHA512Final(&c->sha512,o); }` |
|      - | 5260 | `typedef struct HashAlgo HashAlgo;` |
|      - | 5261 | `struct HashAlgo {` |
|      - | 5262 | `	const char *zName;   /* lowercase canonical name */` |
|      - | 5263 | `	int nDigestLen;      /* output bytes: 16/20/28/32/48/64 */` |
|      - | 5264 | `	int nBlockLen;       /* internal block bytes (for HMAC): 64 or 128 */` |
|      - | 5265 | `	void (*xInit)(HashCtx *);` |
|      - | 5266 | `	void (*xUpdate)(HashCtx *,const unsigned char *,unsigned int);` |
|      - | 5267 | `	void (*xFinal)(HashCtx *,unsigned char *);` |
|      - | 5268 | `};` |
|      - | 5269 | `static const HashAlgo aHashAlgo[] = {` |
|      - | 5270 | `	{ "md5",    16, 64,  HashMd5Init,    HashMd5Update,    HashMd5Final    },` |
|      - | 5271 | `	{ "sha1",   20, 64,  HashSha1Init,   HashSha1Update,   HashSha1Final   },` |
|      - | 5272 | `	{ "sha224", 28, 64,  HashSha224Init, HashSha256Update, HashSha256Final },` |
|      - | 5273 | `	{ "sha256", 32, 64,  HashSha256Init, HashSha256Update, HashSha256Final },` |
|      - | 5274 | `	{ "sha384", 48, 128, HashSha384Init, HashSha512Update, HashSha512Final },` |
|      - | 5275 | `	{ "sha512", 64, 128, HashSha512Init, HashSha512Update, HashSha512Final },` |
|      - | 5276 | `};` |
|      - | 5277 | `/* Case-insensitive algorithm lookup (PHP accepts 'SHA256' etc.). */` |
|     73 | 5278 | `static const HashAlgo * HashFindAlgo(const char *zName,int nLen){` |
|      - | 5279 | `	sxu32 i;` |
|    279 | 5280 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|    272 | 5281 | `		if( (int)SyStrlen(aHashAlgo[i].zName) == nLen` |
|    211 | 5282 | `			&& SyStrnicmp(aHashAlgo[i].zName,zName,(sxu32)nLen) == 0 ){` |
|     67 | 5283 | `			return &aHashAlgo[i];` |
|      - | 5284 | `		}` |
|    106 | 5285 | `	}` |
|      6 | 5286 | `	return 0;` |
|     38 | 5287 | `}` |
|      - | 5288 | `/*` |
|      - | 5289 | ` * string hash(string $algo,string $data[,bool $binary = false])` |
|      - | 5290 | ` *   Generate a hash value (message digest).` |
|      - | 5291 | ` */` |
|     54 | 5292 | `static int PH7_builtin_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5293 | `{` |
|      - | 5294 | `	const HashAlgo *pAlgo;` |
|      - | 5295 | `	const char *zAlgo,*zData;` |
|     56 | 5296 | `	int nAlgoLen,nDataLen,raw_output = FALSE;` |
|      - | 5297 | `	HashCtx sCtx;` |
|      - | 5298 | `	unsigned char zDigest[64];` |
|     56 | 5299 | `	if( nArg < 2 ){` |
|    ! 0 | 5300 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5301 | `			"hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5302 | `	}` |
|     56 | 5303 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     56 | 5304 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     56 | 5305 | `	if( pAlgo == 0 ){` |
|      3 | 5306 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5307 | `			"hash(): Argument #1 ($algo) must be a valid hashing algorithm");` |
|      - | 5308 | `	}` |
|     53 | 5309 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     53 | 5310 | `	if( nArg > 2 ){` |
|      9 | 5311 | `		raw_output = ph7_value_to_bool(apArg[2]);` |
|      4 | 5312 | `	}` |
|     53 | 5313 | `	pAlgo->xInit(&sCtx);` |
|     53 | 5314 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     53 | 5315 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     53 | 5316 | `	if( raw_output ){` |
|      9 | 5317 | `		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);` |
|      5 | 5318 | `	}else{` |
|     45 | 5319 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);` |
|      - | 5320 | `	}` |
|     53 | 5321 | `	return PH7_OK;` |
|     29 | 5322 | `}` |
|      - | 5323 | `/*` |
|      - | 5324 | ` * string hash_hmac(string $algo,string $data,string $key[,bool $binary = false])` |
|      - | 5325 | ` *   Generate a keyed hash value using the HMAC method (RFC 2104).` |
|      - | 5326 | ` */` |
|     16 | 5327 | `static int PH7_builtin_hash_hmac(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5328 | `{` |
|      - | 5329 | `	const HashAlgo *pAlgo;` |
|      - | 5330 | `	const char *zAlgo,*zData,*zKey;` |
|     18 | 5331 | `	int nAlgoLen,nDataLen,nKeyLen,raw_output = FALSE;` |
|      - | 5332 | `	HashCtx sCtx;` |
|      - | 5333 | `	unsigned char zKeyBlock[128],zIpad[128],zOpad[128],zInner[64],zDigest[64];` |
|      - | 5334 | `	int i,nBlock,nDigest;` |
|     18 | 5335 | `	if( nArg < 3 ){` |
|    ! 0 | 5336 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5337 | `			"hash_hmac() expects at least 3 arguments, %d given",nArg);` |
|      - | 5338 | `	}` |
|     18 | 5339 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     18 | 5340 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     18 | 5341 | `	if( pAlgo == 0 ){` |
|      3 | 5342 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5343 | `			"hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");` |
|      - | 5344 | `	}` |
|     15 | 5345 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     15 | 5346 | `	zKey = ph7_value_to_string(apArg[2],&nKeyLen);` |
|     15 | 5347 | `	if( nArg > 3 ){` |
|      3 | 5348 | `		raw_output = ph7_value_to_bool(apArg[3]);` |
|      1 | 5349 | `	}` |
|     15 | 5350 | `	nBlock = pAlgo->nBlockLen;` |
|     15 | 5351 | `	nDigest = pAlgo->nDigestLen;` |
|      - | 5352 | `	/* Reduce the key to a single block: hash it if longer than the block, then` |
|      - | 5353 | `	 * zero-pad (a short or empty key is just zero-padded). */` |
|     15 | 5354 | `	SyZero(zKeyBlock,sizeof(zKeyBlock));` |
|     15 | 5355 | `	if( nKeyLen > nBlock ){` |
|      3 | 5356 | `		pAlgo->xInit(&sCtx);` |
|      3 | 5357 | `		pAlgo->xUpdate(&sCtx,(const unsigned char *)zKey,(unsigned int)nKeyLen);` |
|      3 | 5358 | `		pAlgo->xFinal(&sCtx,zKeyBlock);` |
|     14 | 5359 | `	}else if( nKeyLen > 0 ){` |
|     11 | 5360 | `		SyMemcpy(zKey,zKeyBlock,(sxu32)nKeyLen);` |
|      5 | 5361 | `	}` |
|   1039 | 5362 | `	for( i = 0; i < nBlock; i++ ){` |
|   1025 | 5363 | `		zIpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x36);` |
|   1025 | 5364 | `		zOpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x5c);` |
|    513 | 5365 | `	}` |
|      - | 5366 | `	/* inner = H((key ^ ipad) \|\| data) */` |
|     15 | 5367 | `	pAlgo->xInit(&sCtx);` |
|     15 | 5368 | `	pAlgo->xUpdate(&sCtx,zIpad,(unsigned int)nBlock);` |
|     15 | 5369 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     15 | 5370 | `	pAlgo->xFinal(&sCtx,zInner);` |
|      - | 5371 | `	/* out = H((key ^ opad) \|\| inner) */` |
|     15 | 5372 | `	pAlgo->xInit(&sCtx);` |
|     15 | 5373 | `	pAlgo->xUpdate(&sCtx,zOpad,(unsigned int)nBlock);` |
|     15 | 5374 | `	pAlgo->xUpdate(&sCtx,zInner,(unsigned int)nDigest);` |
|     15 | 5375 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     15 | 5376 | `	if( raw_output ){` |
|      3 | 5377 | `		ph7_result_string(pCtx,(const char *)zDigest,nDigest);` |
|      2 | 5378 | `	}else{` |
|     13 | 5379 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)nDigest,HashConsumer,pCtx);` |
|      - | 5380 | `	}` |
|     15 | 5381 | `	return PH7_OK;` |
|     10 | 5382 | `}` |
|      - | 5383 | `/*` |
|      - | 5384 | ` * bool hash_equals(string $known_string,string $user_string)` |
|      - | 5385 | ` *   Timing-attack-safe string comparison.` |
|      - | 5386 | ` */` |
|     12 | 5387 | `static int PH7_builtin_hash_equals(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5388 | `{` |
|      - | 5389 | `	const char *zKnown,*zUser;` |
|      - | 5390 | `	int nKnown,nUser,i;` |
|     14 | 5391 | `	volatile unsigned char vDiff = 0;` |
|     14 | 5392 | `	if( nArg < 2 ){` |
|    ! 0 | 5393 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5394 | `			"hash_equals() expects exactly 2 arguments, %d given",nArg);` |
|      - | 5395 | `	}` |
|     14 | 5396 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      4 | 5397 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5398 | `			"hash_equals(): Argument #1 ($known_string) must be of type string, %s given",` |
|      1 | 5399 | `			ph7_type_name(apArg[0]));` |
|      - | 5400 | `	}` |
|     11 | 5401 | `	if( !ph7_value_is_string(apArg[1]) ){` |
|    ! 0 | 5402 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5403 | `			"hash_equals(): Argument #2 ($user_string) must be of type string, %s given",` |
|    ! 0 | 5404 | `			ph7_type_name(apArg[1]));` |
|      - | 5405 | `	}` |
|     11 | 5406 | `	zKnown = ph7_value_to_string(apArg[0],&nKnown);` |
|     11 | 5407 | `	zUser = ph7_value_to_string(apArg[1],&nUser);` |
|     11 | 5408 | `	if( nKnown != nUser ){` |
|      5 | 5409 | `		ph7_result_bool(pCtx,0);` |
|      5 | 5410 | `		return PH7_OK;` |
|      - | 5411 | `	}` |
|      - | 5412 | `	/* Constant-time: read every byte, never short-circuit. */` |
|     19 | 5413 | `	for( i = 0; i < nKnown; i++ ){` |
|     13 | 5414 | `		vDiff \|= (unsigned char)(zKnown[i] ^ zUser[i]);` |
|      7 | 5415 | `	}` |
|      7 | 5416 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|      7 | 5417 | `	return PH7_OK;` |
|      8 | 5418 | `}` |
|      - | 5419 | `/*` |
|      - | 5420 | ` * array hash_algos(void)` |
|      - | 5421 | ` *   Return a list of the registered hashing algorithms.` |
|      - | 5422 | ` */` |
|      2 | 5423 | `static int PH7_builtin_hash_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5424 | `{` |
|      - | 5425 | `	ph7_value *pArray,*pValue;` |
|      - | 5426 | `	sxu32 i;` |
|      1 | 5427 | `	SXUNUSED(nArg);` |
|      1 | 5428 | `	SXUNUSED(apArg);` |
|      3 | 5429 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 5430 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 5431 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 5432 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5433 | `		return PH7_OK;` |
|      - | 5434 | `	}` |
|     15 | 5435 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|     13 | 5436 | `		ph7_value_string(pValue,aHashAlgo[i].zName,-1);` |
|     13 | 5437 | `		ph7_array_add_elem(pArray,0 /* Automatic 0-based index */,pValue);` |
|     13 | 5438 | `		ph7_value_reset_string_cursor(pValue);` |
|      7 | 5439 | `	}` |
|      3 | 5440 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5441 | `	return PH7_OK;` |
|      2 | 5442 | `}` |
|      - | 5443 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 5444 | `/*` |
|      - | 5445 | ` * password_* (bcrypt). These live in ext/standard in real PHP — outside the` |
|      - | 5446 | ` * hash extension — so they are NOT guarded by PH7_DISABLE_HASH_FUNC.` |
|      - | 5447 | ` */` |
|      - | 5448 | `/*` |
|      - | 5449 | ` * Parse a bcrypt crypt string. Returns TRUE and fills *piCost when zHash is a` |
|      - | 5450 | ` * well-formed "$2?$NN$"+53-char bcrypt hash (60 bytes, valid minor, cost 4..31).` |
|      - | 5451 | ` */` |
|     40 | 5452 | `static int BcryptParseHash(const char *zHash,int nHash,int *piCost)` |
|      1 | 5453 | `{` |
|      - | 5454 | `	int iCost;` |
|     40 | 5455 | `	if( nHash != 60 \|\| zHash[0] != '$' \|\| zHash[1] != '2' \|\| zHash[3] != '$'` |
|     29 | 5456 | `		\|\| (zHash[2] != 'a' && zHash[2] != 'b' && zHash[2] != 'x' && zHash[2] != 'y') ){` |
|     13 | 5457 | `		return FALSE;` |
|      - | 5458 | `	}` |
|     29 | 5459 | `	if( zHash[4] < '0' \|\| zHash[4] > '9' \|\| zHash[5] < '0' \|\| zHash[5] > '9' \|\| zHash[6] != '$' ){` |
|    ! 0 | 5460 | `		return FALSE;` |
|      - | 5461 | `	}` |
|     29 | 5462 | `	iCost = (zHash[4]-'0')*10 + (zHash[5]-'0');` |
|     29 | 5463 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      3 | 5464 | `		return FALSE;` |
|      - | 5465 | `	}` |
|     27 | 5466 | `	if( piCost ){ *piCost = iCost; }` |
|     27 | 5467 | `	return TRUE;` |
|     21 | 5468 | `}` |
|      - | 5469 | `/*` |
|      - | 5470 | ` * TRUE if the $algo argument selects bcrypt: null (PASSWORD_DEFAULT) or the` |
|      - | 5471 | ` * "2y" id (PASSWORD_BCRYPT/PASSWORD_DEFAULT). bcrypt is the only supported algo.` |
|      - | 5472 | ` */` |
|     20 | 5473 | `static int BcryptIsBcryptAlgo(ph7_value *pAlgo)` |
|      3 | 5474 | `{` |
|     23 | 5475 | `	if( ph7_value_is_null(pAlgo) ){` |
|    ! 0 | 5476 | `		return TRUE;` |
|      - | 5477 | `	}` |
|     23 | 5478 | `	if( ph7_value_is_string(pAlgo) ){` |
|      - | 5479 | `		int nAlgo;` |
|     23 | 5480 | `		const char *zAlgo = ph7_value_to_string(pAlgo,&nAlgo);` |
|     23 | 5481 | `		return ( nAlgo == 2 && zAlgo[0] == '2' && zAlgo[1] == 'y' );` |
|      - | 5482 | `	}` |
|    ! 0 | 5483 | `	return FALSE;` |
|     13 | 5484 | `}` |
|      - | 5485 | `/*` |
|      - | 5486 | ` * bool\|string password_hash(string $password,string\|int\|null $algo[,array $options])` |
|      - | 5487 | ` *  Create a bcrypt hash of the password.` |
|      - | 5488 | ` */` |
|     16 | 5489 | `static int PH7_builtin_password_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 5490 | `{` |
|      - | 5491 | `	const char *zPwd;` |
|     19 | 5492 | `	int nPwd,iCost = 12;` |
|      - | 5493 | `	unsigned char aSalt[16];` |
|      - | 5494 | `	char zHash[60];` |
|     19 | 5495 | `	if( nArg < 2 ){` |
|    ! 0 | 5496 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5497 | `			"password_hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5498 | `	}` |
|     19 | 5499 | `	if( !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      3 | 5500 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5501 | `			"password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm");` |
|      - | 5502 | `	}` |
|      - | 5503 | `	/* cost from $options['cost'] (default 12). */` |
|     16 | 5504 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|     14 | 5505 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|     14 | 5506 | `		if( pCost ){ iCost = ph7_value_to_int(pCost); }` |
|      6 | 5507 | `	}` |
|     16 | 5508 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      4 | 5509 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 5510 | `			"Invalid bcrypt cost parameter specified: %d",iCost);` |
|      - | 5511 | `	}` |
|     13 | 5512 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     13 | 5513 | `	if( SyOSCSPRNG(aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 5514 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - | 5515 | `			"password_hash(): unable to gather sufficient entropy for the salt");` |
|      - | 5516 | `	}` |
|     13 | 5517 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zHash) != SXRET_OK ){` |
|    ! 0 | 5518 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5519 | `		return PH7_OK;` |
|      - | 5520 | `	}` |
|     13 | 5521 | `	ph7_result_string(pCtx,zHash,(int)sizeof(zHash));` |
|     13 | 5522 | `	return PH7_OK;` |
|     11 | 5523 | `}` |
|      - | 5524 | `/*` |
|      - | 5525 | ` * bool password_verify(string $password,string $hash)` |
|      - | 5526 | ` *  Verify a password against a bcrypt hash. Never throws on a malformed hash.` |
|      - | 5527 | ` */` |
|     28 | 5528 | `static int PH7_builtin_password_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5529 | `{` |
|      - | 5530 | `	const char *zPwd,*zHash;` |
|      - | 5531 | `	int nPwd,nHash,iCost,i;` |
|      - | 5532 | `	unsigned char aSalt[16];` |
|      - | 5533 | `	char zComputed[60];` |
|     29 | 5534 | `	volatile unsigned char vDiff = 0;` |
|     29 | 5535 | `	if( nArg < 2 ){` |
|    ! 0 | 5536 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5537 | `			"password_verify() expects exactly 2 arguments, %d given",nArg);` |
|      - | 5538 | `	}` |
|     29 | 5539 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     29 | 5540 | `	zHash = ph7_value_to_string(apArg[1],&nHash);` |
|     29 | 5541 | `	if( !BcryptParseHash(zHash,nHash,&iCost) ){` |
|     11 | 5542 | `		ph7_result_bool(pCtx,0);` |
|     11 | 5543 | `		return PH7_OK;` |
|      - | 5544 | `	}` |
|      - | 5545 | `	/* Recover the 16 salt bytes from the 22-char salt field [7..28]. */` |
|     19 | 5546 | `	if( SyBcryptB64Decode(&zHash[7],22,aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 5547 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5548 | `		return PH7_OK;` |
|      - | 5549 | `	}` |
|     19 | 5550 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zComputed) != SXRET_OK ){` |
|    ! 0 | 5551 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5552 | `		return PH7_OK;` |
|      - | 5553 | `	}` |
|      - | 5554 | `	/* Constant-time compare of the 31-char hash field [29..59] only — sidesteps` |
|      - | 5555 | `	 * salt re-canonicalisation and any "$2a"/"$2y" prefix difference. */` |
|    577 | 5556 | `	for( i = 29; i < 60; i++ ){` |
|    559 | 5557 | `		vDiff \|= (unsigned char)(zComputed[i] ^ zHash[i]);` |
|    280 | 5558 | `	}` |
|     19 | 5559 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|     19 | 5560 | `	return PH7_OK;` |
|     15 | 5561 | `}` |
|      - | 5562 | `/*` |
|      - | 5563 | ` * array password_get_info(string $hash)` |
|      - | 5564 | ` *  Return ["algo"=>id\|null, "algoName"=>name, "options"=>[...]].` |
|      - | 5565 | ` */` |
|      6 | 5566 | `static int PH7_builtin_password_get_info(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5567 | `{` |
|      7 | 5568 | `	const char *zHash = "";` |
|      7 | 5569 | `	int nHash,iCost = 0,bBcrypt = 0;` |
|      - | 5570 | `	ph7_value *pArray,*pOptions,*pVal;` |
|      7 | 5571 | `	if( nArg > 0 ){` |
|      7 | 5572 | `		zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 5573 | `		bBcrypt = BcryptParseHash(zHash,nHash,&iCost);` |
|      3 | 5574 | `	}` |
|      7 | 5575 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 5576 | `	pOptions = ph7_context_new_array(pCtx);` |
|      7 | 5577 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      7 | 5578 | `	if( pArray == 0 \|\| pOptions == 0 \|\| pVal == 0 ){` |
|    ! 0 | 5579 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5580 | `		return PH7_OK;` |
|      - | 5581 | `	}` |
|      7 | 5582 | `	if( bBcrypt ){` |
|      5 | 5583 | `		ph7_value_string(pVal,&zHash[1],2);            /* algo "2y"/"2a" */` |
|      5 | 5584 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      5 | 5585 | `		ph7_value_reset_string_cursor(pVal);` |
|      5 | 5586 | `		ph7_value_string(pVal,"bcrypt",(int)sizeof("bcrypt")-1);` |
|      5 | 5587 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      5 | 5588 | `		ph7_value_int(pVal,iCost);` |
|      5 | 5589 | `		ph7_array_add_strkey_elem(pOptions,"cost",pVal);` |
|      3 | 5590 | `	}else{` |
|      3 | 5591 | `		ph7_value_null(pVal);                          /* algo => null */` |
|      3 | 5592 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      3 | 5593 | `		ph7_value_string(pVal,"unknown",(int)sizeof("unknown")-1);` |
|      3 | 5594 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      - | 5595 | `	}` |
|      7 | 5596 | `	ph7_array_add_strkey_elem(pArray,"options",pOptions);` |
|      7 | 5597 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 5598 | `	return PH7_OK;` |
|      4 | 5599 | `}` |
|      - | 5600 | `/*` |
|      - | 5601 | ` * bool password_needs_rehash(string $hash,string\|int\|null $algo[,array $options])` |
|      - | 5602 | ` *  True if the hash was not made with the given algo/options.` |
|      - | 5603 | ` */` |
|      6 | 5604 | `static int PH7_builtin_password_needs_rehash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5605 | `{` |
|      - | 5606 | `	const char *zHash;` |
|      7 | 5607 | `	int nHash,iCost = 0,iWantCost = 12;` |
|      7 | 5608 | `	if( nArg < 2 ){` |
|    ! 0 | 5609 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5610 | `			"password_needs_rehash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5611 | `	}` |
|      7 | 5612 | `	zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 5613 | `	if( !BcryptParseHash(zHash,nHash,&iCost) \|\| !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      - | 5614 | `		/* A non-bcrypt hash, or a request for a different algo → needs rehash. */` |
|      3 | 5615 | `		ph7_result_bool(pCtx,1);` |
|      3 | 5616 | `		return PH7_OK;` |
|      - | 5617 | `	}` |
|      5 | 5618 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|      5 | 5619 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|      5 | 5620 | `		if( pCost ){ iWantCost = ph7_value_to_int(pCost); }` |
|      2 | 5621 | `	}` |
|      5 | 5622 | `	ph7_result_bool(pCtx,iCost != iWantCost);` |
|      5 | 5623 | `	return PH7_OK;` |
|      4 | 5624 | `}` |
|      - | 5625 | `/*` |
|      - | 5626 | ` * filter_var() — input validation and sanitization (the ext/filter API).` |
|      - | 5627 | ` *` |
|      - | 5628 | ` * Filter and flag identifiers (values match PHP 8.5; the constants themselves` |
|      - | 5629 | ` * are registered in constant.c). The validate filters are hand-rolled rather` |
|      - | 5630 | ` * than delegating to SyStrToInt64/SyStrToReal: the former silently skips leading` |
|      - | 5631 | ` * zeros and cannot signal overflow, and the latter treats ',' as a decimal point` |
|      - | 5632 | ` * unconditionally — neither matches PHP's filter semantics.` |
|      - | 5633 | ` */` |
|      - | 5634 | `#define FV_VALIDATE_INT     257` |
|      - | 5635 | `#define FV_VALIDATE_BOOLEAN 258` |
|      - | 5636 | `#define FV_VALIDATE_FLOAT   259` |
|      - | 5637 | `#define FV_VALIDATE_REGEXP  272` |
|      - | 5638 | `#define FV_VALIDATE_URL     273` |
|      - | 5639 | `#define FV_VALIDATE_EMAIL   274` |
|      - | 5640 | `#define FV_VALIDATE_IP      275` |
|      - | 5641 | `#define FV_VALIDATE_MAC     276` |
|      - | 5642 | `#define FV_VALIDATE_DOMAIN  277` |
|      - | 5643 | `#define FV_SANITIZE_SPECIAL_CHARS      515` |
|      - | 5644 | `#define FV_DEFAULT          516 /* == FILTER_UNSAFE_RAW: pass the value through */` |
|      - | 5645 | `#define FV_SANITIZE_EMAIL   517` |
|      - | 5646 | `#define FV_SANITIZE_URL     518` |
|      - | 5647 | `#define FV_SANITIZE_NUMBER_INT   519` |
|      - | 5648 | `#define FV_SANITIZE_NUMBER_FLOAT 520` |
|      - | 5649 | `#define FV_SANITIZE_FULL_SPECIAL_CHARS 522` |
|      - | 5650 | `#define FV_FLAG_ALLOW_OCTAL  1` |
|      - | 5651 | `#define FV_FLAG_ALLOW_HEX    2` |
|      - | 5652 | `#define FV_FLAG_STRIP_LOW    4` |
|      - | 5653 | `#define FV_FLAG_STRIP_HIGH   8` |
|      - | 5654 | `#define FV_FLAG_ENCODE_LOW   16` |
|      - | 5655 | `#define FV_FLAG_ENCODE_HIGH  32` |
|      - | 5656 | `#define FV_FLAG_ENCODE_AMP   64` |
|      - | 5657 | `#define FV_FLAG_NO_ENCODE_QUOTES 128` |
|      - | 5658 | `#define FV_FLAG_STRIP_BACKTICK   512` |
|      - | 5659 | `#define FV_FLAG_ALLOW_FRACTION   4096` |
|      - | 5660 | `#define FV_FLAG_ALLOW_THOUSAND   8192` |
|      - | 5661 | `#define FV_FLAG_ALLOW_SCIENTIFIC 16384` |
|      - | 5662 | `#define FV_FLAG_IPV4  1048576` |
|      - | 5663 | `#define FV_FLAG_IPV6  2097152` |
|      - | 5664 | `#define FV_NULL_ON_FAILURE 134217728` |
|      - | 5665 | `/* The subset of flags the UNSAFE_RAW/DEFAULT string filter (FvSanitizeString)` |
|      - | 5666 | ` * acts on: when none are set the filter is a verbatim pass-through, so FV_DEFAULT` |
|      - | 5667 | ` * can shortcut. Keep this in sync with FvSanitizeString's flag handling. */` |
|      - | 5668 | `#define FV_FLAG_STRING_MASK (FV_FLAG_STRIP_LOW\|FV_FLAG_STRIP_HIGH\|FV_FLAG_STRIP_BACKTICK \` |
|      - | 5669 | `                            \|FV_FLAG_ENCODE_LOW\|FV_FLAG_ENCODE_HIGH\|FV_FLAG_ENCODE_AMP)` |
|      - | 5670 |  |
|      - | 5671 | `/* Trim leading/trailing PHP whitespace, adjusting the (*pz,*pn) view in place.` |
|      - | 5672 | ` * SyisSpace (isspace) matches PHP's filter whitespace set " \t\n\r\v\f". */` |
|    153 | 5673 | `static void FvTrim(const char **pz,int *pn){` |
|    153 | 5674 | `	const char *z = *pz;` |
|    153 | 5675 | `	int n = *pn;` |
|    157 | 5676 | `	while( n>0 && SyisSpace((unsigned char)z[0]) ){ z++; n--; }` |
|    161 | 5677 | `	while( n>0 && SyisSpace((unsigned char)z[n-1]) ){ n--; }` |
|    153 | 5678 | `	*pz = z; *pn = n;` |
|    153 | 5679 | `}` |
|      - | 5680 | `/* FILTER_VALIDATE_INT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     57 | 5681 | `static int FvValidateInt(const char *z,int n,int flags,ph7_int64 *pOut){` |
|     57 | 5682 | `	int neg = 0, i;` |
|     57 | 5683 | `	sxu64 u = 0;` |
|     57 | 5684 | `	FvTrim(&z,&n);` |
|     57 | 5685 | `	if( n==0 ){ return 0; }` |
|     51 | 5686 | `	if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; n--; }` |
|     51 | 5687 | `	if( n==0 ){ return 0; }` |
|     49 | 5688 | `	if( (flags & FV_FLAG_ALLOW_HEX) && n>=2 && z[0]=='0' && (z[1]=='x'\|\|z[1]=='X') ){` |
|      3 | 5689 | `		z += 2; n -= 2;` |
|      3 | 5690 | `		if( n==0 ){ return 0; }` |
|      7 | 5691 | `		for( i=0; i<n; i++ ){` |
|      5 | 5692 | `			int h = SyHexToint((unsigned char)z[i]);` |
|      5 | 5693 | `			if( h<0 ){ return 0; }` |
|      5 | 5694 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)h)/16 ){ return 0; }` |
|      5 | 5695 | `			u = u*16 + (sxu64)h;` |
|      3 | 5696 | `		}` |
|     48 | 5697 | `	}else if( (flags & FV_FLAG_ALLOW_OCTAL) && z[0]=='0' ){` |
|      9 | 5698 | `		for( i=0; i<n; i++ ){` |
|      7 | 5699 | `			if( z[i]<'0' \|\| z[i]>'7' ){ return 0; }` |
|      7 | 5700 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/8 ){ return 0; }` |
|      7 | 5701 | `			u = u*8 + (sxu64)(z[i]-'0');` |
|      4 | 5702 | `		}` |
|      2 | 5703 | `	}else{` |
|     45 | 5704 | `		if( z[0]=='0' && n>1 ){ return 0; } /* a leading zero is rejected in base 10 */` |
|    201 | 5705 | `		for( i=0; i<n; i++ ){` |
|    173 | 5706 | `			if( !SyisDigit((unsigned char)z[i]) ){ return 0; }` |
|    161 | 5707 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/10 ){ return 0; }` |
|    161 | 5708 | `			u = u*10 + (sxu64)(z[i]-'0');` |
|     81 | 5709 | `		}` |
|      - | 5710 | `	}` |
|     33 | 5711 | `	if( neg ){` |
|      5 | 5712 | `		if( u > 0x8000000000000000ULL ){ return 0; }` |
|      5 | 5713 | `		*pOut = (ph7_int64)(0ULL - u); /* two's-complement negate in unsigned space */` |
|      3 | 5714 | `	}else{` |
|     29 | 5715 | `		if( u > 0x7FFFFFFFFFFFFFFFULL ){ return 0; }` |
|     27 | 5716 | `		*pOut = (ph7_int64)u;` |
|      - | 5717 | `	}` |
|     31 | 5718 | `	return 1;` |
|     29 | 5719 | `}` |
|      - | 5720 | `/* FILTER_VALIDATE_FLOAT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     69 | 5721 | `static int FvValidateFloat(const char *z,int n,int flags,double *pOut){` |
|      - | 5722 | `	char zBuf[512];` |
|     69 | 5723 | `	int i, m = 0, seenDigit = 0;` |
|     69 | 5724 | `	const char *zv; int nv; double d = 0;` |
|     69 | 5725 | `	FvTrim(&z,&n);` |
|      - | 5726 | `	/* Bound the input: zBuf[512] holds the thousand-separator-stripped copy, and` |
|      - | 5727 | `	 * the cap also rejects the pathological 500+ digit floats PHP refuses. */` |
|     69 | 5728 | `	if( n==0 \|\| n>500 ){ return 0; }` |
|     69 | 5729 | `	if( flags & FV_FLAG_ALLOW_THOUSAND ){` |
|      - | 5730 | `		/* Commas are optional, but when present they must group the integer part` |
|      - | 5731 | `		 * into a leading run of 1..3 digits followed by groups of exactly 3` |
|      - | 5732 | `		 * ("1,000" ok, "1,5"/"1234,567" rejected). Strip them into zBuf and reject` |
|      - | 5733 | `		 * a comma anywhere in the fractional/exponent tail. */` |
|     25 | 5734 | `		int s = 0, intEnd, segStart, segIdx, hasComma = 0;` |
|     25 | 5735 | `		if( s<n && (z[s]=='+'\|\|z[s]=='-') ){ zBuf[m++] = z[s]; s++; }` |
|     25 | 5736 | `		intEnd = s;` |
|    167 | 5737 | `		while( intEnd<n && z[intEnd]!='.' && z[intEnd]!='e' && z[intEnd]!='E' ){` |
|    143 | 5738 | `			if( z[intEnd]==',' ){ hasComma = 1; }` |
|    143 | 5739 | `			intEnd++;` |
|      1 | 5740 | `		}` |
|     25 | 5741 | `		if( hasComma ){` |
|     25 | 5742 | `			segStart = s; segIdx = 0;` |
|    165 | 5743 | `			for( i=s; i<=intEnd; i++ ){` |
|    151 | 5744 | `				if( i==intEnd \|\| z[i]==',' ){` |
|     49 | 5745 | `					int segLen = i - segStart, k;` |
|     49 | 5746 | `					if( segIdx==0 ){ if( segLen<1 \|\| segLen>3 ){ return 0; } }` |
|     25 | 5747 | `					else if( segLen!=3 ){ return 0; }` |
|    119 | 5748 | `					for( k=segStart; k<i; k++ ){` |
|     81 | 5749 | `						if( !SyisDigit((unsigned char)z[k]) ){ return 0; }` |
|     81 | 5750 | `						zBuf[m++] = z[k];` |
|     41 | 5751 | `					}` |
|     39 | 5752 | `					segStart = i+1; segIdx++;` |
|     19 | 5753 | `				}` |
|     71 | 5754 | `			}` |
|      8 | 5755 | `		}else{` |
|    ! 0 | 5756 | `			for( i=s; i<intEnd; i++ ){ zBuf[m++] = z[i]; }` |
|      - | 5757 | `		}` |
|     27 | 5758 | `		for( i=intEnd; i<n; i++ ){` |
|     13 | 5759 | `			if( z[i]==',' ){ return 0; }` |
|     13 | 5760 | `			zBuf[m++] = z[i];` |
|      7 | 5761 | `		}` |
|     15 | 5762 | `		zv = zBuf; nv = m;` |
|      8 | 5763 | `	}else{` |
|     45 | 5764 | `		zv = z; nv = n;` |
|      - | 5765 | `	}` |
|     59 | 5766 | `	i = 0;` |
|     59 | 5767 | `	if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|    167 | 5768 | `	while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     59 | 5769 | `	if( i<nv && zv[i]=='.' ){` |
|     21 | 5770 | `		i++;` |
|     39 | 5771 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     10 | 5772 | `	}` |
|     59 | 5773 | `	if( !seenDigit ){ return 0; }` |
|     57 | 5774 | `	if( i<nv && (zv[i]=='e'\|\|zv[i]=='E') ){` |
|     29 | 5775 | `		i++;` |
|     29 | 5776 | `		if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|     29 | 5777 | `		if( i>=nv \|\| !SyisDigit((unsigned char)zv[i]) ){ return 0; }` |
|    105 | 5778 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; }` |
|     14 | 5779 | `	}` |
|     57 | 5780 | `	if( i!=nv ){ return 0; } /* trailing junk */` |
|      - | 5781 | `	/* The grammar above guarantees zv[0..nv) is a clean ASCII decimal float (no hex /` |
|      - | 5782 | `	 * inf / nan / trailing junk), so it is safe to hand to libc strtod, which — unlike` |
|      - | 5783 | `	 * SyStrToReal (15 sig-digits + exponent clamped to 308, so it silently saturates` |
|      - | 5784 | `	 * overflowing magnitudes to a finite value) — is overflow/underflow-aware and` |
|      - | 5785 | `	 * correctly rounded. strtod needs a NUL-terminated string: the ALLOW_THOUSAND path` |
|      - | 5786 | `	 * already built the span in zBuf (zv==zBuf); the plain path must copy it there (z is` |
|      - | 5787 | `	 * const + not NUL-terminated). nv <= n <= 500 < sizeof(zBuf) by the cap above.` |
|      - | 5788 | `	 * Matches PHP 8.5 byte-for-byte: reject overflow (-> +/-INF) and total underflow` |
|      - | 5789 | `	 * (-> 0.0), keep subnormals (nonzero, errno==ERANGE) and a genuine "0" (errno==0). */` |
|     53 | 5790 | `	if( zv != zBuf ){ SyMemcpy(zv,zBuf,(sxu32)nv); }` |
|     53 | 5791 | `	zBuf[nv] = 0;` |
|     53 | 5792 | `	errno = 0;` |
|     53 | 5793 | `	d = strtod(zBuf,0);` |
|     53 | 5794 | `	if( errno == ERANGE && (d == HUGE_VAL \|\| d == -HUGE_VAL \|\| d == 0.0) ){` |
|     15 | 5795 | `		return 0;` |
|      - | 5796 | `	}` |
|     39 | 5797 | `	*pOut = d;` |
|     39 | 5798 | `	return 1;` |
|     35 | 5799 | `}` |
|      - | 5800 | `/* FILTER_VALIDATE_BOOLEAN. Returns 1 if the string is recognized (sets *pBool),` |
|      - | 5801 | ` * 0 if it is unrecognized (the failure path). "0"/"false"/"" are recognized as` |
|      - | 5802 | ` * false, NOT failures. */` |
|     33 | 5803 | `static int FvValidateBool(const char *z,int n,int *pBool){` |
|     33 | 5804 | `	FvTrim(&z,&n);` |
|     32 | 5805 | `	if( (n==1 && z[0]=='1') \|\| (n==4 && SyStrnicmp(z,"true",4)==0)` |
|     25 | 5806 | `	    \|\| (n==2 && SyStrnicmp(z,"on",2)==0) \|\| (n==3 && SyStrnicmp(z,"yes",3)==0) ){` |
|     11 | 5807 | `		*pBool = 1; return 1;` |
|      - | 5808 | `	}` |
|     22 | 5809 | `	if( n==0 \|\| (n==1 && z[0]=='0') \|\| (n==5 && SyStrnicmp(z,"false",5)==0)` |
|     11 | 5810 | `	    \|\| (n==3 && SyStrnicmp(z,"off",3)==0) \|\| (n==2 && SyStrnicmp(z,"no",2)==0) ){` |
|     11 | 5811 | `		*pBool = 0; return 1;` |
|      - | 5812 | `	}` |
|      9 | 5813 | `	return 0;` |
|     15 | 5814 | `}` |
|      - | 5815 | `/* IPv4 dotted-quad: exactly 4 octets 0..255, no leading zeros. */` |
|     33 | 5816 | `static int FvValidateIp4(const char *z,int n){` |
|     33 | 5817 | `	int i = 0, parts = 0;` |
|     77 | 5818 | `	while( i<n ){` |
|     65 | 5819 | `		int val = 0, digits = 0, start = i;` |
|    143 | 5820 | `		while( i<n && SyisDigit((unsigned char)z[i]) ){` |
|     85 | 5821 | `			val = val*10 + (z[i]-'0');` |
|     85 | 5822 | `			if( val>255 ){ return 0; }` |
|     79 | 5823 | `			digits++; i++;` |
|      1 | 5824 | `		}` |
|     59 | 5825 | `		if( digits==0 \|\| digits>3 ){ return 0; }` |
|     49 | 5826 | `		if( digits>1 && z[start]=='0' ){ return 0; } /* leading zero */` |
|     45 | 5827 | `		parts++;` |
|     45 | 5828 | `		if( parts>4 ){ return 0; }` |
|     45 | 5829 | `		if( i<n ){` |
|     33 | 5830 | `			if( z[i]!='.' ){ return 0; }` |
|     33 | 5831 | `			i++;` |
|     33 | 5832 | `			if( i>=n ){ return 0; } /* trailing dot */` |
|     16 | 5833 | `		}` |
|      1 | 5834 | `	}` |
|     13 | 5835 | `	return parts==4;` |
|     17 | 5836 | `}` |
|      - | 5837 | `/* A colon-separated run of IPv6 hextets with no "::" (n may be 0 -> 0 groups),` |
|      - | 5838 | ` * allowing a trailing embedded IPv4. Returns the 16-bit group count or -1. */` |
|     19 | 5839 | `static int FvIp6Hextets(const char *z,int n){` |
|     19 | 5840 | `	int i = 0, segStart = 0, groups = 0;` |
|     19 | 5841 | `	if( n==0 ){ return 0; }` |
|    145 | 5842 | `	while( i<=n ){` |
|    133 | 5843 | `		if( i==n \|\| z[i]==':' ){` |
|     23 | 5844 | `			int segLen = i - segStart, j, isV4 = 0;` |
|     23 | 5845 | `			if( segLen==0 ){ return -1; } /* an empty hextet (stray ':') */` |
|     77 | 5846 | `			for( j=segStart; j<i; j++ ){ if( z[j]=='.' ){ isV4 = 1; break; } }` |
|     23 | 5847 | `			if( isV4 ){` |
|     11 | 5848 | `				if( i!=n ){ return -1; } /* IPv4 only as the final token */` |
|     11 | 5849 | `				if( !FvValidateIp4(z+segStart,segLen) ){ return -1; }` |
|      5 | 5850 | `				groups += 2;` |
|      3 | 5851 | `			}else{` |
|     13 | 5852 | `				if( segLen>4 ){ return -1; }` |
|     47 | 5853 | `				for( j=segStart; j<i; j++ ){ if( SyHexToint((unsigned char)z[j])<0 ){ return -1; } }` |
|     13 | 5854 | `				groups++;` |
|      - | 5855 | `			}` |
|     17 | 5856 | `			segStart = i+1;` |
|      8 | 5857 | `		}` |
|    127 | 5858 | `		i++;` |
|      1 | 5859 | `	}` |
|     13 | 5860 | `	return groups;` |
|     10 | 5861 | `}` |
|      - | 5862 | `/* IPv6: at most one "::" zero-run; 8 groups exactly, or fewer when "::" present. */` |
|     19 | 5863 | `static int FvValidateIp6(const char *z,int n){` |
|     19 | 5864 | `	const char *zDbl = 0;` |
|      - | 5865 | `	int i, ga, gb;` |
|    139 | 5866 | `	for( i=0; i+1<n; i++ ){` |
|    123 | 5867 | `		if( z[i]==':' && z[i+1]==':' ){` |
|     13 | 5868 | `			if( zDbl ){ return 0; } /* a second "::" is invalid */` |
|     11 | 5869 | `			zDbl = z+i;` |
|      5 | 5870 | `		}` |
|     61 | 5871 | `	}` |
|     17 | 5872 | `	if( zDbl==0 ){` |
|      9 | 5873 | `		return FvIp6Hextets(z,n)==8;` |
|    ! 0 | 5874 | `	}else{` |
|      9 | 5875 | `		int lenA = (int)(zDbl - z);` |
|      9 | 5876 | `		int lenB = n - lenA - 2;` |
|      9 | 5877 | `		ga = (lenA==0) ? 0 : FvIp6Hextets(z,lenA);` |
|      9 | 5878 | `		gb = (lenB==0) ? 0 : FvIp6Hextets(zDbl+2,lenB);` |
|      9 | 5879 | `		if( ga<0 \|\| gb<0 ){ return 0; }` |
|      9 | 5880 | `		return (ga+gb)<=7; /* "::" stands for at least one zero group */` |
|      - | 5881 | `	}` |
|     10 | 5882 | `}` |
|     25 | 5883 | `static int FvValidateIp(const char *z,int n,int flags){` |
|     25 | 5884 | `	int v4 = (flags & FV_FLAG_IPV4), v6 = (flags & FV_FLAG_IPV6);` |
|     25 | 5885 | `	if( !v4 && !v6 ){ v4 = v6 = 1; } /* default accepts either family */` |
|     25 | 5886 | `	if( v4 && FvValidateIp4(z,n) ){ return 1; }` |
|     21 | 5887 | `	if( v6 && FvValidateIp6(z,n) ){ return 1; }` |
|     13 | 5888 | `	return 0;` |
|     13 | 5889 | `}` |
|      - | 5890 | `/* FILTER_VALIDATE_MAC: 17-char colon- or dash-separated hex (XX:XX:..:XX). */` |
|     11 | 5891 | `static int FvValidateMac(const char *z,int n){` |
|      - | 5892 | `	char sep;` |
|      - | 5893 | `	int i;` |
|     11 | 5894 | `	if( n!=17 ){ return 0; }` |
|      7 | 5895 | `	sep = z[2];` |
|      7 | 5896 | `	if( sep!=':' && sep!='-' ){ return 0; }` |
|    105 | 5897 | `	for( i=0; i<17; i++ ){` |
|    101 | 5898 | `		if( (i%3)==2 ){ if( z[i]!=sep ){ return 0; } }` |
|     71 | 5899 | `		else if( SyHexToint((unsigned char)z[i])<0 ){ return 0; }` |
|     50 | 5900 | `	}` |
|      5 | 5901 | `	return 1;` |
|      6 | 5902 | `}` |
|      - | 5903 | `/* FILTER_VALIDATE_EMAIL (best-effort: covers the common cases, not quoted local` |
|      - | 5904 | ` * parts or IP-literal domains). */` |
|     28 | 5905 | `static int FvValidateEmail(const char *z,int n){` |
|     28 | 5906 | `	int at = -1, i, localLen, domLen, labelStart, dotCount = 0;` |
|      - | 5907 | `	const char *zDom;` |
|     28 | 5908 | `	if( n==0 \|\| n>320 ){ return 0; }` |
|    201 | 5909 | `	for( i=0; i<n; i++ ){` |
|    181 | 5910 | `		if( z[i]=='@' ){ if( at>=0 ){ return 0; } at = i; }` |
|     91 | 5911 | `	}` |
|     21 | 5912 | `	if( at<=0 \|\| at==n-1 ){ return 0; } /* one '@', non-empty local and domain */` |
|     21 | 5913 | `	localLen = at;` |
|     21 | 5914 | `	zDom = z + at + 1;` |
|     21 | 5915 | `	domLen = n - at - 1;` |
|     21 | 5916 | `	if( z[0]=='.' \|\| z[at-1]=='.' ){ return 0; }` |
|     57 | 5917 | `	for( i=0; i<localLen; i++ ){` |
|     43 | 5918 | `		unsigned char c = (unsigned char)z[i];` |
|     43 | 5919 | `		if( c<=' ' ){ return 0; }` |
|     41 | 5920 | `		if( c=='.' && i+1<localLen && z[i+1]=='.' ){ return 0; }` |
|     20 | 5921 | `	}` |
|     15 | 5922 | `	if( zDom[0]=='.' \|\| zDom[domLen-1]=='.' ){ return 0; }` |
|     13 | 5923 | `	labelStart = 0;` |
|     85 | 5924 | `	for( i=0; i<=domLen; i++ ){` |
|     75 | 5925 | `		if( i==domLen \|\| zDom[i]=='.' ){` |
|     25 | 5926 | `			int ll = i - labelStart;` |
|     25 | 5927 | `			if( ll==0 ){ return 0; } /* consecutive dots */` |
|     23 | 5928 | `			if( zDom[labelStart]=='-' \|\| zDom[i-1]=='-' ){ return 0; }` |
|     23 | 5929 | `			if( i<domLen ){ dotCount++; }` |
|     23 | 5930 | `			labelStart = i+1;` |
|     12 | 5931 | `		}else{` |
|     51 | 5932 | `			unsigned char c = (unsigned char)zDom[i];` |
|     51 | 5933 | `			if( !((c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9')\|\|c=='-') ){ return 0; }` |
|      - | 5934 | `		}` |
|     37 | 5935 | `	}` |
|     11 | 5936 | `	if( dotCount<1 ){ return 0; } /* PHP requires a dot in the domain (any TLD length) */` |
|      9 | 5937 | `	return 1;` |
|     15 | 5938 | `}` |
|      - | 5939 | `/* FILTER_VALIDATE_DOMAIN (lenient, matching PHP without FILTER_FLAG_HOSTNAME). */` |
|     11 | 5940 | `static int FvValidateDomain(const char *z,int n){` |
|      - | 5941 | `	int i;` |
|     11 | 5942 | `	if( n<1 \|\| n>253 \|\| z[0]=='.' ){ return 0; }` |
|     81 | 5943 | `	for( i=0; i<n; i++ ){` |
|     75 | 5944 | `		unsigned char c = (unsigned char)z[i];` |
|     75 | 5945 | `		if( c<=' ' ){ return 0; }` |
|     75 | 5946 | `		if( c=='.' && i+1<n && z[i+1]=='.' ){ return 0; }` |
|     37 | 5947 | `	}` |
|      7 | 5948 | `	return 1;` |
|      6 | 5949 | `}` |
|      - | 5950 | `/* FILTER_VALIDATE_URL: require a scheme and a host (PHP's filter is itself` |
|      - | 5951 | ` * parse_url-based, so PH7_VmHttpSplitURI tracks it closely). */` |
|     15 | 5952 | `static int FvValidateUrl(const char *z,int n){` |
|      - | 5953 | `	SyhttpUri sUri;` |
|     15 | 5954 | `	if( n==0 ){ return 0; }` |
|     15 | 5955 | `	SyZero(&sUri,(sxu32)sizeof(sUri));` |
|     15 | 5956 | `	if( PH7_VmHttpSplitURI(&sUri,z,(sxu32)n)!=SXRET_OK ){ return 0; }` |
|     15 | 5957 | `	return sUri.sScheme.nByte!=0 && sUri.sHost.nByte!=0;` |
|      8 | 5958 | `}` |
|      - | 5959 | `/* The Fv sanitizers build their result by appending directly to the call` |
|      - | 5960 | ` * context (ph7_result_string accumulates, like htmlspecialchars), emitting each` |
|      - | 5961 | ` * kept run in one call and seeding "" so an all-stripped input yields "". */` |
|      - | 5962 | `/* SANITIZE_NUMBER_INT (isFloat=0) / SANITIZE_NUMBER_FLOAT (isFloat=1). */` |
|     37 | 5963 | `static void FvSanitizeNumber(ph7_context *pCtx,const char *z,int n,int isFloat,int flags){` |
|     37 | 5964 | `	int i, runStart = 0;` |
|     37 | 5965 | `	ph7_result_string(pCtx,"",0);` |
|     97 | 5966 | `	for( i=0; i<n; i++ ){` |
|     91 | 5967 | `		char c = z[i];` |
|     91 | 5968 | `		int keep = (c>='0'&&c<='9') \|\| c=='+' \|\| c=='-';` |
|     91 | 5969 | `		if( !keep && isFloat ){` |
|     38 | 5970 | `			keep = (c=='.' && (flags & FV_FLAG_ALLOW_FRACTION))` |
|     23 | 5971 | `			    \|\| (c==',' && (flags & FV_FLAG_ALLOW_THOUSAND))` |
|     36 | 5972 | `			    \|\| ((c=='e'\|\|c=='E') && (flags & FV_FLAG_ALLOW_SCIENTIFIC));` |
|     12 | 5973 | `		}` |
|     61 | 5974 | `		if( !keep ){` |
|     33 | 5975 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     33 | 5976 | `			runStart = i+1;` |
|     16 | 5977 | `		}` |
|     31 | 5978 | `	}` |
|      7 | 5979 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      7 | 5980 | `}` |
|      - | 5981 | `/* Return non-zero when byte c must be stripped under the STRIP_* flags. Shared` |
|      - | 5982 | ` * by the UNSAFE_RAW string filter and SANITIZE_SPECIAL_CHARS. STRIP_LOW drops` |
|      - | 5983 | `` * bytes <32, STRIP_HIGH drops bytes >=127 (incl. DEL), STRIP_BACKTICK drops '`'.`` |
|      - | 5984 | ` * Matches php_filter_strip(); verified byte-exact vs php 8.5.7. */` |
|    287 | 5985 | `static int FvStripByte(unsigned char c,int flags){` |
|    287 | 5986 | `	if( (flags & FV_FLAG_STRIP_LOW)      && c<32 )    { return 1; }` |
|    281 | 5987 | `	if( (flags & FV_FLAG_STRIP_HIGH)     && c>=127 )  { return 1; }` |
|    269 | 5988 | `	if( (flags & FV_FLAG_STRIP_BACKTICK) && c==0x60 ) { return 1; }` |
|    267 | 5989 | `	return 0;` |
|    144 | 5990 | `}` |
|      - | 5991 | `/* FILTER_UNSAFE_RAW / FILTER_DEFAULT with flags: no default transform, but the` |
|      - | 5992 | ` * STRIP/ENCODE flags apply. Precedence (per php_filter_unsafe_raw, verified` |
|      - | 5993 | ` * vs php 8.5.7): a byte is first tested for stripping; a surviving byte is then` |
|      - | 5994 | ` * encoded as a decimal numeric entity if ENCODE_LOW (<32) / ENCODE_HIGH (>=127)` |
|      - | 5995 | ` * is set, and '&' becomes "&#38;" under ENCODE_AMP. So STRIP_LOW\|ENCODE_LOW` |
|      - | 5996 | ` * strips (nothing left to encode). Bytes are treated individually — ENCODE_HIGH` |
|      - | 5997 | ` * numeric-encodes each byte of a multibyte sequence separately, not the codepoint. */` |
|     25 | 5998 | `static void FvSanitizeString(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 5999 | `	int i, runStart = 0;` |
|     25 | 6000 | `	ph7_result_string(pCtx,"",0);` |
|    193 | 6001 | `	for( i=0; i<n; i++ ){` |
|    179 | 6002 | `		unsigned char c = (unsigned char)z[i];` |
|    179 | 6003 | `		if( FvStripByte(c,flags) ){` |
|     13 | 6004 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     13 | 6005 | `			runStart = i+1;` |
|     13 | 6006 | `			continue;` |
|      - | 6007 | `		}` |
|    167 | 6008 | `		if( c=='&' && (flags & FV_FLAG_ENCODE_AMP) ){` |
|      3 | 6009 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      3 | 6010 | `			ph7_result_string(pCtx,"&#38;",-1);` |
|      3 | 6011 | `			runStart = i+1;` |
|    166 | 6012 | `		}else if( (c<32 && (flags & FV_FLAG_ENCODE_LOW))` |
|    164 | 6013 | `		       \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     37 | 6014 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 6015 | `			ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|      9 | 6016 | `			runStart = i+1;` |
|      4 | 6017 | `		}` |
|     79 | 6018 | `	}` |
|     15 | 6019 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     15 | 6020 | `}` |
|      - | 6021 | `/* FILTER_SANITIZE_SPECIAL_CHARS: encode <>&"' and every control byte <32 as a` |
|      - | 6022 | ` * decimal numeric entity (&#60; &#38; &#34; ...). The STRIP_* flags remove bytes` |
|      - | 6023 | ` * before encoding; ENCODE_HIGH numeric-encodes surviving bytes >=127. Bytes >=128` |
|      - | 6024 | ` * are otherwise passed through verbatim (this filter is NOT UTF-8-aware — only the` |
|      - | 6025 | ` * FULL variant is). Byte-exact vs php 8.5.7. */` |
|     13 | 6026 | `static void FvSanitizeSpecial(ph7_context *pCtx,const char *z,int n,int flags){` |
|     13 | 6027 | `	int i, runStart = 0;` |
|      - | 6028 | `	const char *zEnt;` |
|     13 | 6029 | `	ph7_result_string(pCtx,"",0);` |
|    131 | 6030 | `	for( i=0; i<n; i++ ){` |
|    119 | 6031 | `		unsigned char c = (unsigned char)z[i];` |
|    119 | 6032 | `		if( FvStripByte(c,flags) ){` |
|      9 | 6033 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 6034 | `			runStart = i+1;` |
|      9 | 6035 | `			continue;` |
|      - | 6036 | `		}` |
|    111 | 6037 | `		switch( c ){` |
|      3 | 6038 | `		case '<':  zEnt = "&#60;"; break;` |
|      3 | 6039 | `		case '>':  zEnt = "&#62;"; break;` |
|     11 | 6040 | `		case '&':  zEnt = "&#38;"; break;` |
|      3 | 6041 | `		case '"':  zEnt = "&#34;"; break;` |
|      3 | 6042 | `		case '\'': zEnt = "&#39;"; break;` |
|     46 | 6043 | `		default:` |
|      - | 6044 | `			/* Control bytes <32 are always numeric-encoded; bytes >=127 only when` |
|      - | 6045 | `			 * ENCODE_HIGH is set. Everything else stays in the current run. */` |
|     93 | 6046 | `			if( c<32 \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     17 | 6047 | `				if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     17 | 6048 | `				ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|     17 | 6049 | `				runStart = i+1;` |
|      8 | 6050 | `			}` |
|     93 | 6051 | `			continue; /* keep in the current run */` |
|      - | 6052 | `		}` |
|     19 | 6053 | `		if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     19 | 6054 | `		ph7_result_string(pCtx,zEnt,-1); /* -1: length from strlen */` |
|     19 | 6055 | `		runStart = i+1;` |
|     10 | 6056 | `	}` |
|     13 | 6057 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     13 | 6058 | `}` |
|      - | 6059 | `/* HTML 4.01 named-entity table (codepoint -> "&name;") used by the UTF-8-aware` |
|      - | 6060 | ` * FULL_SPECIAL_CHARS filter, sorted ascending by codepoint for binary search.` |
|      - | 6061 | ` * Generated from php 8.5.7 (the exact set php_escape_html_entities emits for the` |
|      - | 6062 | ` * default document type); the five inline specials <>&"' are handled separately,` |
|      - | 6063 | ` * so every entry here is a codepoint >=0xA0. 248 rows. */` |
|      - | 6064 | `static const struct { sxu32 cp; const char *zEnt; } aHtml401Ent[] = {` |
|      - | 6065 | `	{0x00A0,"&nbsp;"},{0x00A1,"&iexcl;"},{0x00A2,"&cent;"},{0x00A3,"&pound;"},` |
|      - | 6066 | `	{0x00A4,"&curren;"},{0x00A5,"&yen;"},{0x00A6,"&brvbar;"},{0x00A7,"&sect;"},` |
|      - | 6067 | `	{0x00A8,"&uml;"},{0x00A9,"&copy;"},{0x00AA,"&ordf;"},{0x00AB,"&laquo;"},` |
|      - | 6068 | `	{0x00AC,"&not;"},{0x00AD,"&shy;"},{0x00AE,"&reg;"},{0x00AF,"&macr;"},` |
|      - | 6069 | `	{0x00B0,"&deg;"},{0x00B1,"&plusmn;"},{0x00B2,"&sup2;"},{0x00B3,"&sup3;"},` |
|      - | 6070 | `	{0x00B4,"&acute;"},{0x00B5,"&micro;"},{0x00B6,"&para;"},{0x00B7,"&middot;"},` |
|      - | 6071 | `	{0x00B8,"&cedil;"},{0x00B9,"&sup1;"},{0x00BA,"&ordm;"},{0x00BB,"&raquo;"},` |
|      - | 6072 | `	{0x00BC,"&frac14;"},{0x00BD,"&frac12;"},{0x00BE,"&frac34;"},{0x00BF,"&iquest;"},` |
|      - | 6073 | `	{0x00C0,"&Agrave;"},{0x00C1,"&Aacute;"},{0x00C2,"&Acirc;"},{0x00C3,"&Atilde;"},` |
|      - | 6074 | `	{0x00C4,"&Auml;"},{0x00C5,"&Aring;"},{0x00C6,"&AElig;"},{0x00C7,"&Ccedil;"},` |
|      - | 6075 | `	{0x00C8,"&Egrave;"},{0x00C9,"&Eacute;"},{0x00CA,"&Ecirc;"},{0x00CB,"&Euml;"},` |
|      - | 6076 | `	{0x00CC,"&Igrave;"},{0x00CD,"&Iacute;"},{0x00CE,"&Icirc;"},{0x00CF,"&Iuml;"},` |
|      - | 6077 | `	{0x00D0,"&ETH;"},{0x00D1,"&Ntilde;"},{0x00D2,"&Ograve;"},{0x00D3,"&Oacute;"},` |
|      - | 6078 | `	{0x00D4,"&Ocirc;"},{0x00D5,"&Otilde;"},{0x00D6,"&Ouml;"},{0x00D7,"&times;"},` |
|      - | 6079 | `	{0x00D8,"&Oslash;"},{0x00D9,"&Ugrave;"},{0x00DA,"&Uacute;"},{0x00DB,"&Ucirc;"},` |
|      - | 6080 | `	{0x00DC,"&Uuml;"},{0x00DD,"&Yacute;"},{0x00DE,"&THORN;"},{0x00DF,"&szlig;"},` |
|      - | 6081 | `	{0x00E0,"&agrave;"},{0x00E1,"&aacute;"},{0x00E2,"&acirc;"},{0x00E3,"&atilde;"},` |
|      - | 6082 | `	{0x00E4,"&auml;"},{0x00E5,"&aring;"},{0x00E6,"&aelig;"},{0x00E7,"&ccedil;"},` |
|      - | 6083 | `	{0x00E8,"&egrave;"},{0x00E9,"&eacute;"},{0x00EA,"&ecirc;"},{0x00EB,"&euml;"},` |
|      - | 6084 | `	{0x00EC,"&igrave;"},{0x00ED,"&iacute;"},{0x00EE,"&icirc;"},{0x00EF,"&iuml;"},` |
|      - | 6085 | `	{0x00F0,"&eth;"},{0x00F1,"&ntilde;"},{0x00F2,"&ograve;"},{0x00F3,"&oacute;"},` |
|      - | 6086 | `	{0x00F4,"&ocirc;"},{0x00F5,"&otilde;"},{0x00F6,"&ouml;"},{0x00F7,"&divide;"},` |
|      - | 6087 | `	{0x00F8,"&oslash;"},{0x00F9,"&ugrave;"},{0x00FA,"&uacute;"},{0x00FB,"&ucirc;"},` |
|      - | 6088 | `	{0x00FC,"&uuml;"},{0x00FD,"&yacute;"},{0x00FE,"&thorn;"},{0x00FF,"&yuml;"},` |
|      - | 6089 | `	{0x0152,"&OElig;"},{0x0153,"&oelig;"},{0x0160,"&Scaron;"},{0x0161,"&scaron;"},` |
|      - | 6090 | `	{0x0178,"&Yuml;"},{0x0192,"&fnof;"},{0x02C6,"&circ;"},{0x02DC,"&tilde;"},` |
|      - | 6091 | `	{0x0391,"&Alpha;"},{0x0392,"&Beta;"},{0x0393,"&Gamma;"},{0x0394,"&Delta;"},` |
|      - | 6092 | `	{0x0395,"&Epsilon;"},{0x0396,"&Zeta;"},{0x0397,"&Eta;"},{0x0398,"&Theta;"},` |
|      - | 6093 | `	{0x0399,"&Iota;"},{0x039A,"&Kappa;"},{0x039B,"&Lambda;"},{0x039C,"&Mu;"},` |
|      - | 6094 | `	{0x039D,"&Nu;"},{0x039E,"&Xi;"},{0x039F,"&Omicron;"},{0x03A0,"&Pi;"},` |
|      - | 6095 | `	{0x03A1,"&Rho;"},{0x03A3,"&Sigma;"},{0x03A4,"&Tau;"},{0x03A5,"&Upsilon;"},` |
|      - | 6096 | `	{0x03A6,"&Phi;"},{0x03A7,"&Chi;"},{0x03A8,"&Psi;"},{0x03A9,"&Omega;"},` |
|      - | 6097 | `	{0x03B1,"&alpha;"},{0x03B2,"&beta;"},{0x03B3,"&gamma;"},{0x03B4,"&delta;"},` |
|      - | 6098 | `	{0x03B5,"&epsilon;"},{0x03B6,"&zeta;"},{0x03B7,"&eta;"},{0x03B8,"&theta;"},` |
|      - | 6099 | `	{0x03B9,"&iota;"},{0x03BA,"&kappa;"},{0x03BB,"&lambda;"},{0x03BC,"&mu;"},` |
|      - | 6100 | `	{0x03BD,"&nu;"},{0x03BE,"&xi;"},{0x03BF,"&omicron;"},{0x03C0,"&pi;"},` |
|      - | 6101 | `	{0x03C1,"&rho;"},{0x03C2,"&sigmaf;"},{0x03C3,"&sigma;"},{0x03C4,"&tau;"},` |
|      - | 6102 | `	{0x03C5,"&upsilon;"},{0x03C6,"&phi;"},{0x03C7,"&chi;"},{0x03C8,"&psi;"},` |
|      - | 6103 | `	{0x03C9,"&omega;"},{0x03D1,"&thetasym;"},{0x03D2,"&upsih;"},{0x03D6,"&piv;"},` |
|      - | 6104 | `	{0x2002,"&ensp;"},{0x2003,"&emsp;"},{0x2009,"&thinsp;"},{0x200C,"&zwnj;"},` |
|      - | 6105 | `	{0x200D,"&zwj;"},{0x200E,"&lrm;"},{0x200F,"&rlm;"},{0x2013,"&ndash;"},` |
|      - | 6106 | `	{0x2014,"&mdash;"},{0x2018,"&lsquo;"},{0x2019,"&rsquo;"},{0x201A,"&sbquo;"},` |
|      - | 6107 | `	{0x201C,"&ldquo;"},{0x201D,"&rdquo;"},{0x201E,"&bdquo;"},{0x2020,"&dagger;"},` |
|      - | 6108 | `	{0x2021,"&Dagger;"},{0x2022,"&bull;"},{0x2026,"&hellip;"},{0x2030,"&permil;"},` |
|      - | 6109 | `	{0x2032,"&prime;"},{0x2033,"&Prime;"},{0x2039,"&lsaquo;"},{0x203A,"&rsaquo;"},` |
|      - | 6110 | `	{0x203E,"&oline;"},{0x2044,"&frasl;"},{0x20AC,"&euro;"},{0x2111,"&image;"},` |
|      - | 6111 | `	{0x2118,"&weierp;"},{0x211C,"&real;"},{0x2122,"&trade;"},{0x2135,"&alefsym;"},` |
|      - | 6112 | `	{0x2190,"&larr;"},{0x2191,"&uarr;"},{0x2192,"&rarr;"},{0x2193,"&darr;"},` |
|      - | 6113 | `	{0x2194,"&harr;"},{0x21B5,"&crarr;"},{0x21D0,"&lArr;"},{0x21D1,"&uArr;"},` |
|      - | 6114 | `	{0x21D2,"&rArr;"},{0x21D3,"&dArr;"},{0x21D4,"&hArr;"},{0x2200,"&forall;"},` |
|      - | 6115 | `	{0x2202,"&part;"},{0x2203,"&exist;"},{0x2205,"&empty;"},{0x2207,"&nabla;"},` |
|      - | 6116 | `	{0x2208,"&isin;"},{0x2209,"&notin;"},{0x220B,"&ni;"},{0x220F,"&prod;"},` |
|      - | 6117 | `	{0x2211,"&sum;"},{0x2212,"&minus;"},{0x2217,"&lowast;"},{0x221A,"&radic;"},` |
|      - | 6118 | `	{0x221D,"&prop;"},{0x221E,"&infin;"},{0x2220,"&ang;"},{0x2227,"&and;"},` |
|      - | 6119 | `	{0x2228,"&or;"},{0x2229,"&cap;"},{0x222A,"&cup;"},{0x222B,"&int;"},` |
|      - | 6120 | `	{0x2234,"&there4;"},{0x223C,"&sim;"},{0x2245,"&cong;"},{0x2248,"&asymp;"},` |
|      - | 6121 | `	{0x2260,"&ne;"},{0x2261,"&equiv;"},{0x2264,"&le;"},{0x2265,"&ge;"},` |
|      - | 6122 | `	{0x2282,"&sub;"},{0x2283,"&sup;"},{0x2284,"&nsub;"},{0x2286,"&sube;"},` |
|      - | 6123 | `	{0x2287,"&supe;"},{0x2295,"&oplus;"},{0x2297,"&otimes;"},{0x22A5,"&perp;"},` |
|      - | 6124 | `	{0x22C5,"&sdot;"},{0x2308,"&lceil;"},{0x2309,"&rceil;"},{0x230A,"&lfloor;"},` |
|      - | 6125 | `	{0x230B,"&rfloor;"},{0x2329,"&lang;"},{0x232A,"&rang;"},{0x25CA,"&loz;"},` |
|      - | 6126 | `	{0x2660,"&spades;"},{0x2663,"&clubs;"},{0x2665,"&hearts;"},{0x2666,"&diams;"}` |
|      - | 6127 | `};` |
|      - | 6128 | `/* Binary-search aHtml401Ent[] for cp; return its "&name;" entity or 0. */` |
|     41 | 6129 | `static const char *FvHtml401Lookup(sxu32 cp){` |
|     41 | 6130 | `	int lo = 0, hi = (int)SX_ARRAYSIZE(aHtml401Ent) - 1;` |
|    323 | 6131 | `	while( lo <= hi ){` |
|    309 | 6132 | `		int mid = (lo + hi) / 2;` |
|    309 | 6133 | `		sxu32 c = aHtml401Ent[mid].cp;` |
|    309 | 6134 | `		if( c == cp ){ return aHtml401Ent[mid].zEnt; }` |
|    283 | 6135 | `		if( c < cp ){ lo = mid + 1; } else { hi = mid - 1; }` |
|      1 | 6136 | `	}` |
|     15 | 6137 | `	return 0;` |
|     21 | 6138 | `}` |
|      - | 6139 | `/* Decode one strict-UTF-8 sequence at p (< zEnd). On success returns its byte` |
|      - | 6140 | ` * length (1..4) and sets *pCp to the codepoint; on any malformed, overlong,` |
|      - | 6141 | ` * surrogate, truncated or out-of-range (>U+10FFFF) sequence returns 0. Matches` |
|      - | 6142 | ` * PHP's UTF-8 validation used by FULL_SPECIAL_CHARS (verified vs php 8.5.7). */` |
|    101 | 6143 | `static int FvUtf8Next(const unsigned char *p,const unsigned char *zEnd,sxu32 *pCp){` |
|    101 | 6144 | `	unsigned char c = p[0];` |
|    101 | 6145 | `	if( c < 0x80 ){ *pCp = c; return 1; }` |
|    101 | 6146 | `	if( c < 0xC2 ){ return 0; }              /* 0x80-0xBF stray cont / 0xC0-0xC1 overlong */` |
|     99 | 6147 | `	if( c < 0xE0 ){                          /* 2-byte: U+0080..U+07FF */` |
|     47 | 6148 | `		if( zEnd-p < 2 \|\| (p[1]&0xC0)!=0x80 ){ return 0; }` |
|     45 | 6149 | `		*pCp = ((sxu32)(c&0x1F)<<6) \| (p[1]&0x3F);` |
|     45 | 6150 | `		return 2;` |
|      - | 6151 | `	}` |
|     53 | 6152 | `	if( c < 0xF0 ){                          /* 3-byte: U+0800..U+FFFF minus surrogates */` |
|      - | 6153 | `		sxu32 cp;` |
|     47 | 6154 | `		if( zEnd-p < 3 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 ){ return 0; }` |
|     33 | 6155 | `		cp = ((sxu32)(c&0x0F)<<12) \| ((sxu32)(p[1]&0x3F)<<6) \| (p[2]&0x3F);` |
|     33 | 6156 | `		if( cp < 0x800 \|\| (cp>=0xD800 && cp<=0xDFFF) ){ return 0; }` |
|     29 | 6157 | `		*pCp = cp;` |
|     29 | 6158 | `		return 3;` |
|      - | 6159 | `	}` |
|      7 | 6160 | `	if( c < 0xF5 ){                          /* 4-byte: U+10000..U+10FFFF */` |
|      - | 6161 | `		sxu32 cp;` |
|      5 | 6162 | `		if( zEnd-p < 4 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 \|\| (p[3]&0xC0)!=0x80 ){ return 0; }` |
|      5 | 6163 | `		cp = ((sxu32)(c&0x07)<<18) \| ((sxu32)(p[1]&0x3F)<<12) \| ((sxu32)(p[2]&0x3F)<<6) \| (p[3]&0x3F);` |
|      5 | 6164 | `		if( cp < 0x10000 \|\| cp > 0x10FFFF ){ return 0; }` |
|      5 | 6165 | `		*pCp = cp;` |
|      5 | 6166 | `		return 4;` |
|      - | 6167 | `	}` |
|      3 | 6168 | `	return 0;                                /* 0xF5-0xFF */` |
|     51 | 6169 | `}` |
|      - | 6170 | `/* FILTER_SANITIZE_FULL_SPECIAL_CHARS: htmlentities-style, UTF-8-aware. Encodes` |
|      - | 6171 | ` * <>&"' as named entities ("'" -> &#039;; quotes suppressed under NO_ENCODE_QUOTES),` |
|      - | 6172 | ` * and every valid UTF-8 codepoint with an HTML 4.01 named entity as that entity;` |
|      - | 6173 | ` * valid codepoints without a named entity (and low control bytes) pass through` |
|      - | 6174 | ` * verbatim. If the input contains ANY invalid UTF-8 the whole result is "".` |
|      - | 6175 | ` * The STRIP/ENCODE flags do NOT apply to this filter (only NO_ENCODE_QUOTES).` |
|      - | 6176 | ` * php's filter does NOT re-encode valid pre-existing entities ("&amp;" stays,` |
|      - | 6177 | ` * "&bogus;" becomes "&amp;bogus;"), i.e. double_encode=false semantics —` |
|      - | 6178 | ` * exactly htmlentities(ENT_QUOTES\|ENT_HTML401, double_encode: false), so this` |
|      - | 6179 | ` * delegates to the shared encoder. Byte-exact vs php 8.5.7. */` |
|     25 | 6180 | `static void FvSanitizeFull(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 6181 | `	int iEntFlags = (flags & FV_FLAG_NO_ENCODE_QUOTES) ? 0 : PH7_ENT_QUOTES;` |
|     25 | 6182 | `	HtmlEscape(pCtx,z,n,iEntFlags,1/*bAll*/,0/*bDoubleEncode*/);` |
|     25 | 6183 | `}` |
|      - | 6184 | `/* ---------------------------------------------------------------------------` |
|      - | 6185 | ` * UTF-8-aware HTML entity core (htmlspecialchars/htmlentities family).` |
|      - | 6186 | ` * Prototyped next to the five builtins earlier in this file; lives here so it` |
|      - | 6187 | ` * can share aHtml401Ent[]/FvHtml401Lookup()/FvUtf8Next() with the filter_var` |
|      - | 6188 | ` * FULL_SPECIAL_CHARS filter above. Byte-exact vs php 8.5.7 (oracle-swept).` |
|      - | 6189 | ` * ------------------------------------------------------------------------ */` |
|      - | 6190 | `/* Encode cp as UTF-8 into zBuf (>= 4 bytes); return the byte length 1..4.` |
|      - | 6191 | ` * Thin wrapper over the engine-wide SX_WRITE_UTF8 (sxmacros.h). */` |
|    585 | 6192 | `static int HtmlCpUtf8(sxu32 cp,char *zBuf){` |
|    585 | 6193 | `	sxu8 *z = (sxu8 *)zBuf;` |
|    585 | 6194 | `	SX_WRITE_UTF8(z,cp);` |
|    585 | 6195 | `	return (int)(z - (sxu8 *)zBuf);` |
|      1 | 6196 | `}` |
|      - | 6197 | `/* Doctype-allowed codepoint test (php's unicode_cp_is_allowed) — gates what a` |
|      - | 6198 | ` * numeric reference may DECODE to. Oracle-pinned per doctype: HTML401` |
|      - | 6199 | ` * disallows C0 (except TAB/LF/CR) and DEL..U+009F; XML1 and XHTML share the` |
|      - | 6200 | ` * XML rules — DEL..U+009F allowed, U+FFFE/U+FFFF excluded; HTML5 swaps CR` |
|      - | 6201 | ` * for FF (0x0C) and excludes the noncharacters (U+FDD0..U+FDEF and every` |
|      - | 6202 | ` * U+xFFFE/U+xFFFF). Surrogates are disallowed everywhere. */` |
|     91 | 6203 | `static int HtmlCpAllowed(sxu32 cp,int iFlags){` |
|     91 | 6204 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|     91 | 6205 | `	if( cp==0x09 \|\| cp==0x0A ){ return 1; }` |
|     87 | 6206 | `	if( cp==0x0D ){ return iDoc != PH7_ENT_DOC_HTML5; }` |
|     85 | 6207 | `	if( cp==0x0C ){ return iDoc == PH7_ENT_DOC_HTML5; }` |
|     85 | 6208 | `	if( cp < 0x20 \|\| cp > 0x10FFFF ){ return 0; }` |
|     79 | 6209 | `	if( cp>=0xD800 && cp<=0xDFFF ){ return 0; }` |
|     77 | 6210 | `	if( cp>=0x7F && cp<=0x9F ){ return iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML; }` |
|     71 | 6211 | `	if( iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML ){` |
|    ! 0 | 6212 | `		return cp!=0xFFFE && cp!=0xFFFF;` |
|      - | 6213 | `	}` |
|     71 | 6214 | `	if( iDoc == PH7_ENT_DOC_HTML5 ){` |
|      9 | 6215 | `		if( cp>=0xFDD0 && cp<=0xFDEF ){ return 0; }` |
|      9 | 6216 | `		if( (cp & 0xFFFF) >= 0xFFFE ){ return 0; }` |
|      4 | 6217 | `	}` |
|     71 | 6218 | `	return 1;` |
|     46 | 6219 | `}` |
|      - | 6220 | `/* The ENT_DISALLOWED gate for RAW characters on the ENCODE side. Same as the` |
|      - | 6221 | ` * decode gate except CR under HTML5: php's encode-side unicode_cp_is_allowed` |
|      - | 6222 | ` * keeps a literal "\r" verbatim under ENT_HTML5\|ENT_DISALLOWED while the` |
|      - | 6223 | ` * decode side leaves "&#13;" un-decoded (oracle-pinned at flags 176). */` |
|      9 | 6224 | `static int HtmlCpAllowedEncode(sxu32 cp,int iFlags){` |
|      9 | 6225 | `	if( cp==0x0D && (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 ){ return 1; }` |
|      9 | 6226 | `	return HtmlCpAllowed(cp,iFlags);` |
|      5 | 6227 | `}` |
|      - | 6228 | `/* Numeric-reference validity for the double_encode=false "is this already a` |
|      - | 6229 | ` * valid entity" test — a MUCH looser predicate than the decode gate above:` |
|      - | 6230 | ` * any codepoint <= U+10FFFF is valid (controls and surrogates included, every` |
|      - | 6231 | ` * doctype). ENT_DISALLOWED re-tightens non-HTML401 doctypes to the decode` |
|      - | 6232 | ` * gate, except that HTML5 exempts surrogates. All oracle-pinned: &#0; and` |
|      - | 6233 | ` * &#xD800; stay verbatim at flags 11 and 139; flags -1 (HTML5+DISALLOWED)` |
|      - | 6234 | ` * re-encodes &#0; and &#x10FFFF; but still keeps &#xD800;; flags 144` |
|      - | 6235 | ` * (XML1+DISALLOWED) re-encodes &#xD800;. */` |
|      9 | 6236 | `static int HtmlNumericAllowed(sxu32 cp,int iFlags){` |
|      9 | 6237 | `	if( cp > 0x10FFFF ){ return 0; }` |
|      7 | 6238 | `	if( (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML401 ){ return 1; /* never tightened */ }` |
|    ! 0 | 6239 | `	if( (iFlags & PH7_ENT_DISALLOWED)` |
|    ! 0 | 6240 | `	 && !((iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 && cp>=0xD800 && cp<=0xDFFF)` |
|    ! 0 | 6241 | `	 && !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|    ! 0 | 6242 | `	return 1;` |
|      5 | 6243 | `}` |
|      - | 6244 | `/* How many bytes the malformed UTF-8 sequence at p consumes — php's` |
|      - | 6245 | ` * get_next_char failure step (one U+FFFD substitution / one ENT_IGNORE drop` |
|      - | 6246 | ` * per MAXIMAL invalid subpart, not per byte): a prefix-valid sequence eats` |
|      - | 6247 | ` * its continuation bytes ("\xE0\x80\xAF" is ONE unit) while a byte that could` |
|      - | 6248 | ` * start a new sequence is left for the next round. */` |
|      5 | 6249 | `static int HtmlUtf8Trail(unsigned char c){ return c>=0x80 && c<=0xBF; }` |
|     11 | 6250 | `static int HtmlUtf8Lead(unsigned char c){ return c<0x80 \|\| (c>=0xC2 && c<=0xF4); }` |
|     15 | 6251 | `static int HtmlUtf8FailAdvance(const unsigned char *p,const unsigned char *zEnd){` |
|     15 | 6252 | `	unsigned char c = p[0];` |
|     15 | 6253 | `	int nAvail = (int)(zEnd - p);` |
|     15 | 6254 | `	if( c < 0xC2 \|\| c > 0xF4 ){ return 1; } /* stray trail / C0-C1 / F5-FF */` |
|     13 | 6255 | `	if( c < 0xE0 ){` |
|      3 | 6256 | `		if( nAvail < 2 ){ return 1; }` |
|      3 | 6257 | `		return HtmlUtf8Lead(p[1]) ? 1 : 2;` |
|      - | 6258 | `	}` |
|     11 | 6259 | `	if( c < 0xF0 ){` |
|     11 | 6260 | `		if( nAvail >= 3 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) ){` |
|      3 | 6261 | `			return 3; /* complete but overlong/surrogate */` |
|      - | 6262 | `		}` |
|      9 | 6263 | `		if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 6264 | `		if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 6265 | `		return 3;` |
|      - | 6266 | `	}` |
|    ! 0 | 6267 | `	if( nAvail >= 4 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) && HtmlUtf8Trail(p[3]) ){` |
|    ! 0 | 6268 | `		return 4; /* complete but overlong / > U+10FFFF */` |
|      - | 6269 | `	}` |
|    ! 0 | 6270 | `	if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 6271 | `	if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 6272 | `	if( nAvail < 4 \|\| HtmlUtf8Lead(p[3]) ){ return 3; }` |
|    ! 0 | 6273 | `	return 4;` |
|      8 | 6274 | `}` |
|      - | 6275 | `/* The basic special entities, shared by named matching, the hsc_decode` |
|      - | 6276 | ` * numeric whitelist and the translation-table builder so the sets can never` |
|      - | 6277 | ` * drift apart. (&apos; is not an HTML 4.01 entity — doctype-gated below.) */` |
|      - | 6278 | `static const struct { const char *zEnt; int n; sxu32 cp; } aHtmlSpecEnt[] = {` |
|      - | 6279 | `	{"&amp;",5,38},{"&lt;",4,60},{"&gt;",4,62},{"&quot;",6,34},{"&apos;",6,39}` |
|      - | 6280 | `};` |
|      - | 6281 | `/* Does this doctype consult the named-entity table (aHtml401Ent)? XML 1.0 has` |
|      - | 6282 | ` * no named entities beyond the specials; XHTML/HTML5 are approximated by the` |
|      - | 6283 | ` * HTML 4.01 table (documented divergence). */` |
|     63 | 6284 | `static int HtmlDocHasNamedTable(int iDoc){` |
|     63 | 6285 | `	return iDoc != PH7_ENT_DOC_XML1;` |
|      1 | 6286 | `}` |
|      - | 6287 | `/* The single-quote entity per doctype. Oracle-pinned asymmetry: for every` |
|      - | 6288 | ` * non-HTML401 doctype htmlspecialchars emits &apos; while htmlentities` |
|      - | 6289 | ` * (bEntities) keeps &#039; under XHTML too. The translation table mirrors` |
|      - | 6290 | ` * whichever function the requested table belongs to. */` |
|     29 | 6291 | `static const char *HtmlAposEntity(int iDoc,int bEntities){` |
|     29 | 6292 | `	if( iDoc == PH7_ENT_DOC_HTML401 \|\| (bEntities && iDoc == PH7_ENT_DOC_XHTML) ){` |
|     21 | 6293 | `		return "&#039;";` |
|      - | 6294 | `	}` |
|      9 | 6295 | `	return "&apos;";` |
|     15 | 6296 | `}` |
|      - | 6297 | `/* Try to parse one HTML entity at z (z[0]=='&', z < zEnd). bFull selects the` |
|      - | 6298 | ` * html_entity_decode set (doctype named table + any allowed numeric ref) vs` |
|      - | 6299 | ` * the htmlspecialchars_decode set (the basic specials + quote numerics only).` |
|      - | 6300 | ` * Named matching is case-SENSITIVE and the ';' is required (both PHP-exact);` |
|      - | 6301 | ` * numeric refs accept dec/hex (x or X) with any number of leading zeros but` |
|      - | 6302 | ` * reject out-of-range, surrogate and doctype-disallowed codepoints (the` |
|      - | 6303 | ` * caller then leaves the source verbatim). Quote-flag gating is NOT applied` |
|      - | 6304 | ` * here — the same routine doubles as the "is this a valid entity" test for` |
|      - | 6305 | ` * double_encode=false, which ignores the quote bits (oracle-pinned).` |
|      - | 6306 | ` * bEncodeCheck selects the looser HtmlNumericAllowed predicate used by that` |
|      - | 6307 | ` * double_encode test; decode callers pass 0 for the HtmlCpAllowed gate.` |
|      - | 6308 | ` * On success sets *pCp / *pnConsumed and returns 1. */` |
|    172 | 6309 | `static int HtmlParseEntity(const unsigned char *z,const unsigned char *zEnd,` |
|      1 | 6310 | `                           int iFlags,int bFull,int bEncodeCheck,sxu32 *pCp,int *pnConsumed){` |
|    173 | 6311 | `	int nAvail = (int)(zEnd - z);` |
|    173 | 6312 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6313 | `	sxu32 n;` |
|    173 | 6314 | `	if( nAvail < 4 ){ return 0; } /* shortest entities: &lt; &#9; */` |
|    169 | 6315 | `	if( z[1] == '#' ){` |
|      - | 6316 | `		/* Numeric reference */` |
|     89 | 6317 | `		sxu32 cp = 0;` |
|     89 | 6318 | `		int i = 2, bHex = 0, nDig = 0;` |
|     89 | 6319 | `		if( z[i]=='x' \|\| z[i]=='X' ){ bHex = 1; i++; }` |
|    317 | 6320 | `		for( ; i < nAvail && z[i] != ';' ; i++ ){` |
|      - | 6321 | `			int v;` |
|    221 | 6322 | `			unsigned char c = z[i];` |
|    221 | 6323 | `			if( c>='0' && c<='9' ){ v = c - '0'; }` |
|     17 | 6324 | `			else if( bHex && c>='a' && c<='f' ){ v = c - 'a' + 10; }` |
|     17 | 6325 | `			else if( bHex && c>='A' && c<='F' ){ v = c - 'A' + 10; }` |
|    ! 0 | 6326 | `			else { return 0; }` |
|      - | 6327 | `			/* Stop accumulating once out of range (keeps validating the shape;` |
|      - | 6328 | `			 * max intermediate is 0x10FFFF*16+15, no sxu32 overflow). */` |
|    221 | 6329 | `			if( cp <= 0x10FFFF ){ cp = cp * (bHex ? 16 : 10) + (sxu32)v; }` |
|    221 | 6330 | `			nDig++;` |
|    111 | 6331 | `		}` |
|     97 | 6332 | `		if( nDig == 0 \|\| i >= nAvail ){ return 0; } /* no digits / no ';' */` |
|     97 | 6333 | `		if( bEncodeCheck ? !HtmlNumericAllowed(cp,iFlags) : !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|     83 | 6334 | `		if( !bFull ){` |
|      - | 6335 | `			/* hsc_decode: numeric refs to the five specials only. */` |
|     99 | 6336 | `			for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) && aHtmlSpecEnt[n].cp != cp ; n++ ){}` |
|     25 | 6337 | `			if( n >= SX_ARRAYSIZE(aHtmlSpecEnt) ){ return 0; }` |
|     11 | 6338 | `		}` |
|     75 | 6339 | `		*pCp = cp;` |
|     75 | 6340 | `		*pnConsumed = i + 1;` |
|     75 | 6341 | `		return 1;` |
|      - | 6342 | `	}` |
|      - | 6343 | `	/* Named reference — every entity name starts with a letter, so anything` |
|      - | 6344 | `	 * else can bail out before touching the tables. */` |
|     81 | 6345 | `	if( !((z[1]>='a' && z[1]<='z') \|\| (z[1]>='A' && z[1]<='Z')) ){ return 0; }` |
|    287 | 6346 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) ; n++ ){` |
|    265 | 6347 | `		if( aHtmlSpecEnt[n].cp == 39 && iDoc == PH7_ENT_DOC_HTML401 ){ continue; }` |
|    243 | 6348 | `		if( nAvail >= aHtmlSpecEnt[n].n && SyMemcmp(z,aHtmlSpecEnt[n].zEnt,(sxu32)aHtmlSpecEnt[n].n) == 0 ){` |
|     53 | 6349 | `			*pCp = aHtmlSpecEnt[n].cp;` |
|     53 | 6350 | `			*pnConsumed = aHtmlSpecEnt[n].n;` |
|     53 | 6351 | `			return 1;` |
|      - | 6352 | `		}` |
|     96 | 6353 | `	}` |
|     23 | 6354 | `	if( bFull && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 6355 | `		/* Linear scan of the 248-row table: runs only at '&'-then-letter` |
|      - | 6356 | `		 * positions and guarantees the decode set can never drift from the` |
|      - | 6357 | `		 * encode table. The first-letter guard skips the SyStrlen/SyMemcmp` |
|      - | 6358 | `		 * for ~96% of rows. */` |
|   3369 | 6359 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|      - | 6360 | `			sxu32 nEnt;` |
|   3357 | 6361 | `			if( z[1] != (unsigned char)aHtml401Ent[n].zEnt[1] ){ continue; }` |
|    121 | 6362 | `			nEnt = SyStrlen(aHtml401Ent[n].zEnt);` |
|    121 | 6363 | `			if( (sxu32)nAvail >= nEnt && SyMemcmp(z,aHtml401Ent[n].zEnt,nEnt) == 0 ){` |
|      7 | 6364 | `				*pCp = aHtml401Ent[n].cp;` |
|      7 | 6365 | `				*pnConsumed = (int)nEnt;` |
|      7 | 6366 | `				return 1;` |
|      - | 6367 | `			}` |
|     58 | 6368 | `		}` |
|      6 | 6369 | `	}` |
|     17 | 6370 | `	return 0;` |
|     88 | 6371 | `}` |
|      - | 6372 | `/* Shared encoder for htmlspecialchars (bAll=0) and htmlentities (bAll=1).` |
|      - | 6373 | ` * Invalid UTF-8 policy: ENT_IGNORE drops the byte (and wins over SUBSTITUTE),` |
|      - | 6374 | ` * ENT_SUBSTITUTE emits one U+FFFD per invalid byte, neither -> the whole` |
|      - | 6375 | ` * result is "" (pre-validated in a first pass: the accumulating result API` |
|      - | 6376 | ` * cannot roll back — same reason FvSanitizeFull is two-pass). */` |
|     96 | 6377 | `static void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 6378 | `                       int iFlags,int bAll,int bDoubleEncode){` |
|     97 | 6379 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     97 | 6380 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|      - | 6381 | `	const unsigned char *runStart;` |
|     97 | 6382 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6383 | `	sxu32 cp;` |
|     97 | 6384 | `	if( (iFlags & (PH7_ENT_IGNORE\|PH7_ENT_SUBSTITUTE)) == 0 ){` |
|      - | 6385 | `		/* Pass 1: any malformed sequence rejects the entire input. ASCII` |
|      - | 6386 | `		 * bytes cannot be malformed, so skip them without the decoder. */` |
|    381 | 6387 | `		while( p < zEnd ){` |
|      - | 6388 | `			int len;` |
|    323 | 6389 | `			if( *p < 0x80 ){ p++; continue; }` |
|     37 | 6390 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     37 | 6391 | `			if( len == 0 ){ ph7_result_string(pCtx,"",0); return; }` |
|     27 | 6392 | `			p += len;` |
|      1 | 6393 | `		}` |
|     59 | 6394 | `		p = (const unsigned char *)zIn;` |
|     29 | 6395 | `	}` |
|     87 | 6396 | `	runStart = p;` |
|     87 | 6397 | `	ph7_result_string(pCtx,"",0);` |
|    463 | 6398 | `	while( p < zEnd ){` |
|    377 | 6399 | `		const char *zEnt = 0;` |
|      - | 6400 | `		int len;` |
|    377 | 6401 | `		if( *p < 0x80 ){` |
|    313 | 6402 | `			len = 1;` |
|    313 | 6403 | `			switch( *p ){` |
|     25 | 6404 | `			case '<': zEnt = "&lt;"; break;` |
|     25 | 6405 | `			case '>': zEnt = "&gt;"; break;` |
|     18 | 6406 | `			case '&':` |
|     37 | 6407 | `				zEnt = "&amp;";` |
|     37 | 6408 | `				if( !bDoubleEncode ){` |
|      - | 6409 | `					sxu32 eCp; int nEat;` |
|     25 | 6410 | `					if( HtmlParseEntity(p,zEnd,iFlags,1,1,&eCp,&nEat) ){` |
|      - | 6411 | `						/* A valid existing entity: keep it verbatim. */` |
|     13 | 6412 | `						zEnt = 0;` |
|     13 | 6413 | `						len = nEat;` |
|      6 | 6414 | `					}` |
|     12 | 6415 | `				}` |
|     37 | 6416 | `				break;` |
|     10 | 6417 | `			case '"':` |
|     21 | 6418 | `				if( iFlags & PH7_ENT_QUOTE_DOUBLE ){ zEnt = "&quot;"; }` |
|     21 | 6419 | `				break;` |
|     12 | 6420 | `			case '\'':` |
|     25 | 6421 | `				if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|     23 | 6422 | `					zEnt = HtmlAposEntity(iDoc,bAll);` |
|     11 | 6423 | `				}` |
|     25 | 6424 | `				break;` |
|     92 | 6425 | `			default:` |
|    185 | 6426 | `				if( (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode((sxu32)*p,iFlags) ){` |
|    ! 0 | 6427 | `					zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 6428 | `				}` |
|    184 | 6429 | `				break;` |
|      - | 6430 | `			}` |
|    157 | 6431 | `		}else{` |
|     65 | 6432 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     65 | 6433 | `			if( len == 0 ){` |
|      - | 6434 | `				/* Malformed subpart (IGNORE or SUBSTITUTE is set, else pass 1` |
|      - | 6435 | `				 * would have rejected): drop it or emit ONE U+FFFD for the` |
|      - | 6436 | `				 * whole unit (php substitutes per maximal invalid subpart). */` |
|     15 | 6437 | `				if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|     15 | 6438 | `				if( (iFlags & PH7_ENT_IGNORE) == 0 ){ ph7_result_string(pCtx,"\xEF\xBF\xBD",3); }` |
|     15 | 6439 | `				p += HtmlUtf8FailAdvance(p,zEnd);` |
|     15 | 6440 | `				runStart = p;` |
|     15 | 6441 | `				continue;` |
|      - | 6442 | `			}` |
|     51 | 6443 | `			if( bAll && HtmlDocHasNamedTable(iDoc) ){` |
|     41 | 6444 | `				zEnt = FvHtml401Lookup(cp);` |
|     20 | 6445 | `			}` |
|     51 | 6446 | `			if( zEnt == 0 && (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode(cp,iFlags) ){` |
|    ! 0 | 6447 | `				zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 6448 | `			}` |
|      - | 6449 | `		}` |
|    363 | 6450 | `		if( zEnt ){` |
|    135 | 6451 | `			if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|    135 | 6452 | `			ph7_result_string(pCtx,zEnt,-1);` |
|    135 | 6453 | `			runStart = p + len;` |
|     67 | 6454 | `		}` |
|    363 | 6455 | `		p += len;` |
|      1 | 6456 | `	}` |
|     87 | 6457 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     49 | 6458 | `}` |
|      - | 6459 | `/* Shared decoder for html_entity_decode (bFull=1) and htmlspecialchars_decode` |
|      - | 6460 | ` * (bFull=0). Quote refs (cp 34/39, named or numeric) are gated by the quote` |
|      - | 6461 | ` * bits and left verbatim when suppressed; an invalid entity leaves its '&'` |
|      - | 6462 | ` * verbatim and rescans right after it, which also yields PHP's no-double-` |
|      - | 6463 | ` * decode behavior ("&amp;lt;" -> "&lt;"). */` |
|     84 | 6464 | `static void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 6465 | `                         int iFlags,int bFull){` |
|     85 | 6466 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     85 | 6467 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|     85 | 6468 | `	const unsigned char *runStart = p;` |
|     85 | 6469 | `	ph7_result_string(pCtx,"",0);` |
|    565 | 6470 | `	while( p < zEnd ){` |
|      - | 6471 | `		sxu32 cp;` |
|      - | 6472 | `		int nEat;` |
|    516 | 6473 | `		if( *p != '&' ){ p++; continue; }` |
|    155 | 6474 | `		if( !HtmlParseEntity(p,zEnd,iFlags,bFull,0,&cp,&nEat) ){ p++; continue; }` |
|    124 | 6475 | `		if( (cp == 34 && (iFlags & PH7_ENT_QUOTE_DOUBLE) == 0)` |
|    117 | 6476 | `		 \|\| (cp == 39 && (iFlags & PH7_ENT_QUOTE_SINGLE) == 0) ){` |
|      - | 6477 | `			/* Suppressed quote: leave the entity source verbatim. */` |
|     37 | 6478 | `			p += nEat;` |
|     37 | 6479 | `			continue;` |
|      - | 6480 | `		}` |
|     89 | 6481 | `		if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|      - | 6482 | `		{` |
|      - | 6483 | `			char zBuf[4];` |
|     89 | 6484 | `			int n = HtmlCpUtf8(cp,zBuf);` |
|     89 | 6485 | `			ph7_result_string(pCtx,zBuf,n);` |
|      - | 6486 | `		}` |
|     89 | 6487 | `		p += nEat;` |
|     89 | 6488 | `		runStart = p;` |
|      1 | 6489 | `	}` |
|     81 | 6490 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     81 | 6491 | `}` |
|      - | 6492 | `/* Validate the optional charset argument at apArg[idx]: UTF-8 aliases (and` |
|      - | 6493 | ` * ""/NULL meaning the default) are accepted; anything else — including` |
|      - | 6494 | ` * php-supported single-byte charsets like ISO-8859-1, PHL is UTF-8-only by` |
|      - | 6495 | ` * policy — raises PHP's unsupported-charset warning and is treated as` |
|      - | 6496 | ` * UTF-8 (ph7_context_throw_error_format prepends the function name). */` |
|    143 | 6497 | `static void HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx){` |
|      - | 6498 | `	const char *zCs;` |
|      - | 6499 | `	int nCs;` |
|    150 | 6500 | `	if( nArg <= idx \|\| ph7_value_is_null(apArg[idx]) ){ return; }` |
|     15 | 6501 | `	zCs = ph7_value_to_string(apArg[idx],&nCs);` |
|     15 | 6502 | `	if( nCs == 0 ){ return; } /* "" selects the default charset (UTF-8) */` |
|     13 | 6503 | `	if( nCs == 5 && SyStrnicmp(zCs,"UTF-8",5) == 0 ){` |
|     13 | 6504 | `		return; /* php accepts only "UTF-8" (any case) silently — "UTF8" warns */` |
|      - | 6505 | `	}` |
|    ! 0 | 6506 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 6507 | `		"Charset \"%.*s\" is not supported, assuming UTF-8",nCs,zCs);` |
|     72 | 6508 | `}` |
|      - | 6509 | `/* get_html_translation_table() worker: character (UTF-8 bytes) => entity.` |
|      - | 6510 | ` * The five specials come first in byte order, then — for HTML_ENTITIES with a` |
|      - | 6511 | ` * named-table doctype — the 248 aHtml401Ent rows ascending (oracle-pinned` |
|      - | 6512 | ` * ordering; 253 entries under the defaults). */` |
|    549 | 6513 | `static void HtmlTableAdd(ph7_value *pArray,ph7_value *pValue,const char *zKey,const char *zEnt){` |
|    549 | 6514 | `	ph7_value_string(pValue,zEnt,-1);` |
|    549 | 6515 | `	ph7_array_add_strkey_elem(pArray,zKey,pValue);` |
|    549 | 6516 | `	ph7_value_reset_string_cursor(pValue);` |
|    549 | 6517 | `}` |
|     13 | 6518 | `static void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags){` |
|      - | 6519 | `	ph7_value *pArray,*pValue;` |
|     13 | 6520 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6521 | `	sxu32 n;` |
|     13 | 6522 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 6523 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 6524 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|    ! 0 | 6525 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6526 | `		return;` |
|      - | 6527 | `	}` |
|     13 | 6528 | `	if( iFlags & PH7_ENT_QUOTE_DOUBLE ){` |
|     11 | 6529 | `		HtmlTableAdd(pArray,pValue,"\"","&quot;");` |
|      5 | 6530 | `	}` |
|     13 | 6531 | `	HtmlTableAdd(pArray,pValue,"&","&amp;");` |
|     13 | 6532 | `	if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|      - | 6533 | `		/* The apostrophe row mirrors the function each table belongs to:` |
|      - | 6534 | `		 * SPECIALCHARS follows htmlspecialchars, ENTITIES follows` |
|      - | 6535 | `		 * htmlentities (oracle-pinned at flags 35). */` |
|      7 | 6536 | `		HtmlTableAdd(pArray,pValue,"'",HtmlAposEntity(iDoc,iTable != 0));` |
|      3 | 6537 | `	}` |
|     13 | 6538 | `	HtmlTableAdd(pArray,pValue,"<","&lt;");` |
|     13 | 6539 | `	HtmlTableAdd(pArray,pValue,">","&gt;");` |
|     13 | 6540 | `	if( iTable != 0 /*php: any non-HTML_SPECIALCHARS table => entities*/ && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 6541 | `		char zKey[8];` |
|    499 | 6542 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|    497 | 6543 | `			int nK = HtmlCpUtf8(aHtml401Ent[n].cp,zKey);` |
|    497 | 6544 | `			zKey[nK] = 0;` |
|    497 | 6545 | `			HtmlTableAdd(pArray,pValue,zKey,aHtml401Ent[n].zEnt);` |
|    249 | 6546 | `		}` |
|      1 | 6547 | `	}` |
|     13 | 6548 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 6549 | `}` |
|     25 | 6550 | `static int FvEmailAllowed(unsigned char c){` |
|     25 | 6551 | `	if( (c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9') ){ return 1; }` |
|     16 | 6552 | `	return c=='!'\|\|c=='#'\|\|c=='$'\|\|c=='%'\|\|c=='&'\|\|c=='\''\|\|c=='*'\|\|c=='+'` |
|     10 | 6553 | ``	    \|\| c=='-'\|\|c=='='\|\|c=='?'\|\|c=='^'\|\|c=='_'\|\|c=='`'\|\|c=='{'\|\|c=='\|'`` |
|     15 | 6554 | `	    \|\| c=='}'\|\|c=='~'\|\|c=='@'\|\|c=='.'\|\|c=='['\|\|c==']';` |
|     13 | 6555 | `}` |
|     23 | 6556 | `static int FvUrlAllowed(unsigned char c){` |
|     23 | 6557 | `	return c>=33 && c<=126; /* PHP keeps every printable ASCII byte except space */` |
|      1 | 6558 | `}` |
|      - | 6559 | `/* SANITIZE_EMAIL (isUrl=0) / SANITIZE_URL (isUrl=1): strip disallowed bytes. */` |
|      5 | 6560 | `static void FvSanitizeChars(ph7_context *pCtx,const char *z,int n,int isUrl){` |
|      5 | 6561 | `	int i, runStart = 0;` |
|      5 | 6562 | `	ph7_result_string(pCtx,"",0);` |
|     51 | 6563 | `	for( i=0; i<n; i++ ){` |
|     47 | 6564 | `		unsigned char c = (unsigned char)z[i];` |
|     47 | 6565 | `		if( !(isUrl ? FvUrlAllowed(c) : FvEmailAllowed(c)) ){` |
|     11 | 6566 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     11 | 6567 | `			runStart = i+1;` |
|      5 | 6568 | `		}` |
|     24 | 6569 | `	}` |
|      5 | 6570 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      5 | 6571 | `}` |
|      - | 6572 | `/*` |
|      - | 6573 | ` * Apply the selected filter to one already-resolved input value and write the` |
|      - | 6574 | ` * result into pCtx. Shared by filter_var() and filter_input(): the caller has` |
|      - | 6575 | ` * already parsed $filter/$flags/$options. On validation failure the 'default'` |
|      - | 6576 | ` * option (if any) is returned, else null when FILTER_NULL_ON_FAILURE is set,` |
|      - | 6577 | ` * else false. A validating filter that passes returns the (string) input` |
|      - | 6578 | ` * unchanged; a sanitizer writes its transformed output directly.` |
|      - | 6579 | ` */` |
|    316 | 6580 | `static int FvApplyFilter(ph7_context *pCtx,ph7_value *pInput,` |
|      - | 6581 | `                         int iFilter,int iFlags,ph7_value *pOpts,` |
|      - | 6582 | `                         ph7_value *pDefault)` |
|      3 | 6583 | `{` |
|    319 | 6584 | `	int bNull = (iFlags & FV_NULL_ON_FAILURE) ? 1 : 0;` |
|      - | 6585 | `	const char *zVal; int nVal;` |
|      - | 6586 | `	/* An array/object input fails every scalar filter. */` |
|    319 | 6587 | `	if( ph7_value_is_array(pInput) ){ goto fail; }` |
|    317 | 6588 | `	zVal = ph7_value_to_string(pInput,&nVal);` |
|    317 | 6589 | `	switch( iFilter ){` |
|     28 | 6590 | `	case FV_VALIDATE_INT: {` |
|      - | 6591 | `		ph7_int64 v;` |
|     58 | 6592 | `		if( !FvValidateInt(zVal,nVal,iFlags,&v) ){ goto fail; }` |
|     31 | 6593 | `		if( pOpts ){` |
|      7 | 6594 | `			ph7_value *pMin = ph7_array_fetch(pOpts,"min_range",(int)sizeof("min_range")-1);` |
|      7 | 6595 | `			ph7_value *pMax = ph7_array_fetch(pOpts,"max_range",(int)sizeof("max_range")-1);` |
|      7 | 6596 | `			if( pMin && v<ph7_value_to_int64(pMin) ){ goto fail; }` |
|      7 | 6597 | `			if( pMax && v>ph7_value_to_int64(pMax) ){ goto fail; }` |
|      2 | 6598 | `		}` |
|     29 | 6599 | `		ph7_result_int64(pCtx,v);` |
|     29 | 6600 | `		return PH7_OK;` |
|      - | 6601 | `	}` |
|     34 | 6602 | `	case FV_VALIDATE_FLOAT: {` |
|      - | 6603 | `		double d;` |
|     69 | 6604 | `		if( !FvValidateFloat(zVal,nVal,iFlags,&d) ){ goto fail; }` |
|     39 | 6605 | `		ph7_result_double(pCtx,d);` |
|     39 | 6606 | `		return PH7_OK;` |
|      - | 6607 | `	}` |
|     14 | 6608 | `	case FV_VALIDATE_BOOLEAN: {` |
|      - | 6609 | `		int b;` |
|     29 | 6610 | `		if( !FvValidateBool(zVal,nVal,&b) ){ goto fail; }` |
|     21 | 6611 | `		ph7_result_bool(pCtx,b);` |
|     21 | 6612 | `		return PH7_OK;` |
|      - | 6613 | `	}` |
|     25 | 6614 | `	case FV_VALIDATE_IP:     if( !FvValidateIp(zVal,nVal,iFlags) ){ goto fail; } goto pass;` |
|     11 | 6615 | `	case FV_VALIDATE_MAC:    if( !FvValidateMac(zVal,nVal) ){ goto fail; }       goto pass;` |
|     28 | 6616 | `	case FV_VALIDATE_EMAIL:  if( !FvValidateEmail(zVal,nVal) ){ goto fail; }     goto pass;` |
|     11 | 6617 | `	case FV_VALIDATE_DOMAIN: if( !FvValidateDomain(zVal,nVal) ){ goto fail; }    goto pass;` |
|     15 | 6618 | `	case FV_VALIDATE_URL:    if( !FvValidateUrl(zVal,nVal) ){ goto fail; }       goto pass;` |
|      3 | 6619 | `	case FV_VALIDATE_REGEXP: {` |
|      - | 6620 | `#ifdef PH7_ENABLE_PCRE` |
|      8 | 6621 | `		ph7_value *pRe = pOpts ? ph7_array_fetch(pOpts,"regexp",(int)sizeof("regexp")-1) : 0;` |
|      8 | 6622 | `		const char *zRe; int nRe, matched = 0;` |
|      8 | 6623 | `		if( pRe==0 ){` |
|      3 | 6624 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6625 | `				"filter_var(): \"regexp\" option is missing");` |
|      - | 6626 | `		}` |
|      5 | 6627 | `		zRe = ph7_value_to_string(pRe,&nRe);` |
|      5 | 6628 | `		if( PH7_PcreMatchQuiet(pCtx,zRe,nRe,zVal,nVal,&matched)!=SXRET_OK \|\| !matched ){ goto fail; }` |
|      3 | 6629 | `		goto pass;` |
|      - | 6630 | `#else` |
|      - | 6631 | `		goto fail;` |
|      - | 6632 | `#endif` |
|      - | 6633 | `	}` |
|      3 | 6634 | `	case FV_SANITIZE_NUMBER_INT:   FvSanitizeNumber(pCtx,zVal,nVal,0,0);      return PH7_OK;` |
|      5 | 6635 | `	case FV_SANITIZE_NUMBER_FLOAT: FvSanitizeNumber(pCtx,zVal,nVal,1,iFlags); return PH7_OK;` |
|     13 | 6636 | `	case FV_SANITIZE_SPECIAL_CHARS:      FvSanitizeSpecial(pCtx,zVal,nVal,iFlags); return PH7_OK;` |
|     25 | 6637 | `	case FV_SANITIZE_FULL_SPECIAL_CHARS: FvSanitizeFull(pCtx,zVal,nVal,iFlags);    return PH7_OK;` |
|      3 | 6638 | `	case FV_SANITIZE_EMAIL: FvSanitizeChars(pCtx,zVal,nVal,0); return PH7_OK;` |
|      3 | 6639 | `	case FV_SANITIZE_URL:   FvSanitizeChars(pCtx,zVal,nVal,1); return PH7_OK;` |
|     13 | 6640 | `	case FV_DEFAULT:` |
|      - | 6641 | `		/* FILTER_UNSAFE_RAW / FILTER_DEFAULT: pass through unchanged unless a` |
|      - | 6642 | `		 * STRIP/ENCODE flag is set, in which case apply the string filter. */` |
|     28 | 6643 | `		if( iFlags & FV_FLAG_STRING_MASK ){` |
|     15 | 6644 | `			FvSanitizeString(pCtx,zVal,nVal,iFlags);` |
|     15 | 6645 | `			return PH7_OK;` |
|      - | 6646 | `		}` |
|     14 | 6647 | `		goto pass;` |
|    ! 0 | 6648 | `	default:` |
|    ! 0 | 6649 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 6650 | `			"Unknown filter with ID %d",iFilter);` |
|    ! 0 | 6651 | `		break; /* unknown filter id -> fail */` |
|    ! 0 | 6652 | `	}` |
|     58 | 6653 | `fail:` |
|    118 | 6654 | `	if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|    114 | 6655 | `	else if( bNull ){ ph7_result_null(pCtx); }` |
|    108 | 6656 | `	else { ph7_result_bool(pCtx,0); }` |
|    118 | 6657 | `	return PH7_OK;` |
|     26 | 6658 | `pass: /* validation passed: return the (string) input unchanged */` |
|     54 | 6659 | `	ph7_result_string(pCtx,zVal,nVal);` |
|     54 | 6660 | `	return PH7_OK;` |
|    161 | 6661 | `}` |
|      - | 6662 | `/*` |
|      - | 6663 | ` * Parse the ($filter, $options) pair shared by filter_var()/filter_input() out` |
|      - | 6664 | ` * of apArg[iBase] ($filter) and apArg[iBase+1] ($options): $options is either a` |
|      - | 6665 | ` * plain flags int, or an array with 'flags' and an 'options' sub-array (whose` |
|      - | 6666 | ` * 'default' entry is the fallback value). Fills the four output pointers;` |
|      - | 6667 | ` * unset outputs keep the caller-provided defaults.` |
|      - | 6668 | ` */` |
|    328 | 6669 | `static void FvParseFilterArgs(int nArg,ph7_value **apArg,int iBase,` |
|      - | 6670 | `                              int *piFilter,int *piFlags,` |
|      - | 6671 | `                              ph7_value **ppOpts,ph7_value **ppDefault)` |
|      3 | 6672 | `{` |
|    331 | 6673 | `	if( nArg>iBase ){ *piFilter = ph7_value_to_int(apArg[iBase]); }` |
|    331 | 6674 | `	if( nArg>iBase+1 ){` |
|     88 | 6675 | `		if( ph7_value_is_array(apArg[iBase+1]) ){` |
|     42 | 6676 | `			ph7_value *pF = ph7_array_fetch(apArg[iBase+1],"flags",(int)sizeof("flags")-1);` |
|     42 | 6677 | `			if( pF ){ *piFlags = ph7_value_to_int(pF); }` |
|     42 | 6678 | `			*ppOpts = ph7_array_fetch(apArg[iBase+1],"options",(int)sizeof("options")-1);` |
|     42 | 6679 | `			if( *ppOpts && !ph7_value_is_array(*ppOpts) ){ *ppOpts = 0; }` |
|     42 | 6680 | `			if( *ppOpts ){ *ppDefault = ph7_array_fetch(*ppOpts,"default",(int)sizeof("default")-1); }` |
|     22 | 6681 | `		}else{` |
|     48 | 6682 | `			*piFlags = ph7_value_to_int(apArg[iBase+1]);` |
|      - | 6683 | `		}` |
|     43 | 6684 | `	}` |
|    331 | 6685 | `}` |
|      - | 6686 | `/*` |
|      - | 6687 | ` * filter_var($value, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 6688 | ` *  Validate or sanitize a value; see FvApplyFilter for the failure semantics.` |
|      - | 6689 | ` */` |
|    306 | 6690 | `static int PH7_builtin_filter_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6691 | `{` |
|    308 | 6692 | `	int iFilter = FV_DEFAULT, iFlags = 0;` |
|    308 | 6693 | `	ph7_value *pOpts = 0, *pDefault = 0;` |
|    308 | 6694 | `	if( nArg<1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    308 | 6695 | `	FvParseFilterArgs(nArg,apArg,1,&iFilter,&iFlags,&pOpts,&pDefault);` |
|    308 | 6696 | `	return FvApplyFilter(pCtx,apArg[0],iFilter,iFlags,pOpts,pDefault);` |
|    155 | 6697 | `}` |
|      - | 6698 | `/*` |
|      - | 6699 | ` * filter_input($type, $var_name, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 6700 | ` *  Look up $var_name in the requested INPUT_* superglobal, then apply the` |
|      - | 6701 | ` *  filter. Semantics verified byte-for-byte against php 8.5:` |
|      - | 6702 | ` *   - variable NOT set: 'default' option wins, else false when` |
|      - | 6703 | ` *     FILTER_NULL_ON_FAILURE is set, else null. (Note the null/false roles are` |
|      - | 6704 | ` *     INVERTED relative to a present value that fails validation, which yields` |
|      - | 6705 | ` *     default > null-if-NULL_ON_FAILURE > false via FvApplyFilter.)` |
|      - | 6706 | ` *   - variable present: delegate to FvApplyFilter.` |
|      - | 6707 | ` *  Divergence: php reads a SAPI snapshot of the original request variables` |
|      - | 6708 | ` *  captured at startup; PHL reads the live superglobal. In CLI they match for` |
|      - | 6709 | ` *  the SAPI-registered keys (SCRIPT_NAME/PHP_SELF/DOCUMENT_ROOT); keys added` |
|      - | 6710 | ` *  only to the live $_SERVER (REQUEST_TIME/PWD/…) are visible here but not in` |
|      - | 6711 | ` *  php's snapshot.` |
|      - | 6712 | ` */` |
|     24 | 6713 | `static int PH7_builtin_filter_input(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6714 | `{` |
|     26 | 6715 | `	int iType, iFilter = FV_DEFAULT, iFlags = 0;` |
|     26 | 6716 | `	ph7_value *pOpts = 0, *pDefault = 0, *pSuper, *pElem;` |
|      - | 6717 | `	const char *zVar, *zSuper; int nVar; sxu32 nSuper;` |
|     26 | 6718 | `	if( nArg<2 ){` |
|    ! 0 | 6719 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 6720 | `			"filter_input() expects at least 2 arguments, %d given",nArg);` |
|      - | 6721 | `	}` |
|     26 | 6722 | `	iType = ph7_value_to_int(apArg[0]);` |
|     26 | 6723 | `	switch( iType ){` |
|      3 | 6724 | `	case 0: zSuper = "_POST";   nSuper = (sxu32)sizeof("_POST")-1;   break; /* INPUT_POST */` |
|      3 | 6725 | `	case 1: zSuper = "_GET";    nSuper = (sxu32)sizeof("_GET")-1;    break; /* INPUT_GET */` |
|    ! 0 | 6726 | `	case 2: zSuper = "_COOKIE"; nSuper = (sxu32)sizeof("_COOKIE")-1; break; /* INPUT_COOKIE */` |
|    ! 0 | 6727 | `	case 4: zSuper = "_ENV";    nSuper = (sxu32)sizeof("_ENV")-1;    break; /* INPUT_ENV */` |
|     19 | 6728 | `	case 5: zSuper = "_SERVER"; nSuper = (sxu32)sizeof("_SERVER")-1; break; /* INPUT_SERVER */` |
|      1 | 6729 | `	default:` |
|      3 | 6730 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6731 | `			"filter_input(): Argument #1 ($type) must be an INPUT_* constant");` |
|      - | 6732 | `	}` |
|     23 | 6733 | `	zVar = ph7_value_to_string(apArg[1],&nVar);` |
|     23 | 6734 | `	FvParseFilterArgs(nArg,apArg,2,&iFilter,&iFlags,&pOpts,&pDefault);` |
|      - | 6735 | `	/* Resolve the variable from the superglobal (missing/non-array -> not set). */` |
|     23 | 6736 | `	pSuper = PH7_VmExtractSuper(pCtx->pVm,zSuper,nSuper);` |
|     23 | 6737 | `	pElem = (pSuper && ph7_value_is_array(pSuper))` |
|     33 | 6738 | `		? ph7_array_fetch(pSuper,zVar,nVar) : 0;` |
|     23 | 6739 | `	if( pElem==0 ){` |
|      - | 6740 | `		/* Variable not set: default > false(if NULL_ON_FAILURE) > null. Note the` |
|      - | 6741 | `		 * false/null roles are inverted vs FvApplyFilter's present-but-fails path. */` |
|     13 | 6742 | `		if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|      9 | 6743 | `		else if( iFlags & FV_NULL_ON_FAILURE ){ ph7_result_bool(pCtx,0); }` |
|      7 | 6744 | `		else { ph7_result_null(pCtx); }` |
|     13 | 6745 | `		return PH7_OK;` |
|      - | 6746 | `	}` |
|     11 | 6747 | `	return FvApplyFilter(pCtx,pElem,iFilter,iFlags,pOpts,pDefault);` |
|     14 | 6748 | `}` |
|      - | 6749 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6750 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6751 | `/*` |
|      - | 6752 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 6753 |  |
|      - | 6754 | ` */` |
|      4 | 6755 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 6756 | `	const char *zInput, /* Raw input */` |
|      - | 6757 | `	int nByte,  /* Input length */` |
|      - | 6758 | `	int delim,  /* Delimiter */` |
|      - | 6759 | `	int encl,   /* Enclosure */` |
|      - | 6760 | `	int escape,  /* Escape character */` |
|      - | 6761 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 6762 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 6763 | `	)` |
|      1 | 6764 | `{` |
|      5 | 6765 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 6766 | `	const char *zIn = zInput;` |
|      - | 6767 | `	const char *zPtr;` |
|      - | 6768 | `	int isEnc;` |
|      - | 6769 | `	/* Start processing */` |
|      8 | 6770 | `	for(;;){` |
|     17 | 6771 | `		if( zIn >= zEnd ){` |
|      - | 6772 | `			/* No more input to process */` |
|      5 | 6773 | `			break;` |
|      - | 6774 | `		}` |
|     13 | 6775 | `		isEnc = 0;` |
|     13 | 6776 | `		zPtr = zIn;` |
|      - | 6777 | `		/* Find the first delimiter */` |
|     27 | 6778 | `		while( zIn < zEnd ){` |
|     23 | 6779 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 6780 | `				/* Delimiter found,break imediately */` |
|      5 | 6781 | `				break;` |
|     15 | 6782 | `			}else if( zIn[0] == encl ){` |
|      - | 6783 | `				/* Inside enclosure? */` |
|    ! 0 | 6784 | `				isEnc = !isEnc;` |
|     15 | 6785 | `			}else if( zIn[0] == escape ){` |
|      - | 6786 | `				/* Escape sequence */` |
|    ! 0 | 6787 | `				zIn++;` |
|    ! 0 | 6788 | `			}` |
|      - | 6789 | `			/* Advance the cursor */` |
|     15 | 6790 | `			zIn++;` |
|      1 | 6791 | `		}` |
|     13 | 6792 | `		if( zIn > zPtr ){` |
|     13 | 6793 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 6794 | `			sxi32 rc;` |
|      - | 6795 | `			/* Invoke the supllied callback */` |
|     13 | 6796 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 6797 | `				zPtr++;` |
|    ! 0 | 6798 | `				nByteChunk-=2;` |
|    ! 0 | 6799 | `			}` |
|     13 | 6800 | `			if( nByteChunk > 0 ){` |
|     13 | 6801 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 6802 | `				if( rc == SXERR_ABORT ){` |
|      - | 6803 | `					/* User callback request an operation abort */` |
|    ! 0 | 6804 | `					break;` |
|      - | 6805 | `				}` |
|      6 | 6806 | `			}` |
|      6 | 6807 | `		}` |
|      - | 6808 | `		/* Ignore trailing delimiter */` |
|     21 | 6809 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 6810 | `			zIn++;` |
|      1 | 6811 | `		}` |
|      1 | 6812 | `	}` |
|      5 | 6813 | `	return SXRET_OK;` |
|      1 | 6814 | `}` |
|      - | 6815 | `/*` |
|      - | 6816 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 6817 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 6818 | ` * argument to this callback.` |
|      - | 6819 | ` */` |
|     12 | 6820 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 6821 | `{` |
|     13 | 6822 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 6823 | `	ph7_value sEntry;` |
|      - | 6824 | `	SyString sToken;` |
|      - | 6825 | `	/* Insert the token in the given array */` |
|     13 | 6826 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 6827 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 6828 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 6829 | `	if( sToken.nByte < 1){` |
|    ! 0 | 6830 | `		return SXRET_OK;` |
|      - | 6831 | `	}` |
|     13 | 6832 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 6833 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 6834 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 6835 | `	return SXRET_OK;` |
|      7 | 6836 | `}` |
|      - | 6837 | `/*` |
|      - | 6838 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 6839 | ` *  Parse a CSV string into an array.` |
|      - | 6840 | ` * Parameters` |
|      - | 6841 | ` *  $input` |
|      - | 6842 | ` *   The string to parse.` |
|      - | 6843 | ` *  $delimiter` |
|      - | 6844 | ` *   Set the field delimiter (one character only).` |
|      - | 6845 | ` *  $enclosure` |
|      - | 6846 | ` *   Set the field enclosure character (one character only).` |
|      - | 6847 | ` *  $escape` |
|      - | 6848 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 6849 | ` * Return` |
|      - | 6850 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 6851 | ` */` |
|      2 | 6852 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6853 | `{` |
|      - | 6854 | `	const char *zInput,*zPtr;` |
|      - | 6855 | `	ph7_value *pArray;` |
|      3 | 6856 | `	int delim  = ',';   /* Delimiter */` |
|      3 | 6857 | `	int encl   = '"' ;  /* Enclosure */` |
|      3 | 6858 | `	int escape = '\\';  /* Escape character */` |
|      - | 6859 | `	int nLen;` |
|      3 | 6860 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6861 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 6862 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6863 | `		return PH7_OK;` |
|      - | 6864 | `	}` |
|      - | 6865 | `	/* Extract the raw input */` |
|      3 | 6866 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 6867 | `	if( nArg > 1 ){` |
|      - | 6868 | `		int i;` |
|      3 | 6869 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 6870 | `			/* Extract the delimiter */` |
|      3 | 6871 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 6872 | `			if( i > 0 ){` |
|      3 | 6873 | `				delim = zPtr[0];` |
|      1 | 6874 | `			}` |
|      1 | 6875 | `		}` |
|      3 | 6876 | `		if( nArg > 2 ){` |
|      3 | 6877 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 6878 | `				/* Extract the enclosure */` |
|      3 | 6879 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 6880 | `				if( i > 0 ){` |
|      3 | 6881 | `					encl = zPtr[0];` |
|      1 | 6882 | `				}` |
|      1 | 6883 | `			}` |
|      3 | 6884 | `			if( nArg > 3 ){` |
|      3 | 6885 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 6886 | `					/* Extract the escape character */` |
|      3 | 6887 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 6888 | `					if( i > 0 ){` |
|      3 | 6889 | `						escape = zPtr[0];` |
|      1 | 6890 | `					}` |
|      1 | 6891 | `				}` |
|      1 | 6892 | `			}` |
|      1 | 6893 | `		}` |
|      1 | 6894 | `	}` |
|      - | 6895 | `	/* Create our array */` |
|      3 | 6896 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 6897 | `	if( pArray == 0 ){` |
|      - | 6898 | `		/* Surface a fatal instead of silently returning null on OOM */` |
|    ! 0 | 6899 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6900 | `	}` |
|      - | 6901 | `	/* Parse the raw input */` |
|      3 | 6902 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 6903 | `	/* Return the freshly created array */` |
|      3 | 6904 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 6905 | `	return PH7_OK;` |
|      2 | 6906 | `}` |
|      - | 6907 | `/*` |
|      - | 6908 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 6909 | ` * container.` |
|      - | 6910 | ` * Refer to [strip_tags()].` |
|      - | 6911 | ` */` |
|     10 | 6912 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 6913 | `{` |
|     11 | 6914 | `	const char *zEnd = &zTag[nByte];` |
|      - | 6915 | `	const char *zPtr;` |
|      - | 6916 | `	SyString sEntry;` |
|      - | 6917 | `	/* Strip tags */` |
|     10 | 6918 | `	for(;;){` |
|     45 | 6919 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 6920 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 6921 | `				zTag++;` |
|      1 | 6922 | `		}` |
|     21 | 6923 | `		if( zTag >= zEnd ){` |
|     11 | 6924 | `			break;` |
|      - | 6925 | `		}` |
|     11 | 6926 | `		zPtr = zTag;` |
|      - | 6927 | `		/* Delimit the tag */` |
|     25 | 6928 | `		while(zTag < zEnd ){` |
|     25 | 6929 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 6930 | `				/* UTF-8 stream */` |
|      3 | 6931 | `				zTag++;` |
|      5 | 6932 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 6933 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 6934 | `				break;` |
|    ! 0 | 6935 | `			}else{` |
|     13 | 6936 | `				zTag++;` |
|      - | 6937 | `			}` |
|      1 | 6938 | `		}` |
|     11 | 6939 | `		if( zTag > zPtr ){` |
|      - | 6940 | `			/* Perform the insertion */` |
|     11 | 6941 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 6942 | `			SyStringFullTrim(&sEntry);` |
|     11 | 6943 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 6944 | `		}` |
|      - | 6945 | `		/* Jump the trailing '>' */` |
|     11 | 6946 | `		zTag++;` |
|      1 | 6947 | `	}` |
|     11 | 6948 | `	return SXRET_OK;` |
|      1 | 6949 | `}` |
|      - | 6950 | `/*` |
|      - | 6951 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 6952 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 6953 | ` * Refer to [strip_tags()].` |
|      - | 6954 | ` */` |
|     36 | 6955 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 6956 | `{` |
|     37 | 6957 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 6958 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 6959 | `		SyString sTag;` |
|     85 | 6960 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 6961 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 6962 | `			zTag++;` |
|      1 | 6963 | `		}` |
|      - | 6964 | `		/* Delimit the tag */` |
|     25 | 6965 | `		zCur = zTag;` |
|     77 | 6966 | `		while(zTag < zEnd ){` |
|     77 | 6967 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 6968 | `				/* UTF-8 stream */` |
|      5 | 6969 | `				zTag++;` |
|      9 | 6970 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 6971 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 6972 | `				break;` |
|    ! 0 | 6973 | `			}else{` |
|     49 | 6974 | `				zTag++;` |
|      - | 6975 | `			}` |
|      1 | 6976 | `		}` |
|     25 | 6977 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 6978 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 6979 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 6980 | `		if( sTag.nByte > 0 ){` |
|      - | 6981 | `			SyString *aEntry,*pEntry;` |
|      - | 6982 | `			sxi32 rc;` |
|      - | 6983 | `			sxu32 n;` |
|      - | 6984 | `			/* Perform the lookup */` |
|     25 | 6985 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 6986 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 6987 | `				pEntry = &aEntry[n];` |
|      - | 6988 | `				/* Do the comparison */` |
|     25 | 6989 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 6990 | `				if( !rc ){` |
|     21 | 6991 | `					return SXRET_OK;` |
|      - | 6992 | `				}` |
|      3 | 6993 | `			}` |
|      2 | 6994 | `		}` |
|      2 | 6995 | `	}` |
|      - | 6996 | `	/* No such tag */` |
|     17 | 6997 | `	return SXERR_NOTFOUND;` |
|     19 | 6998 | `}` |
|      - | 6999 | `/*` |
|      - | 7000 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 7001 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 7002 | ` * Refer to [strip_tags()].` |
|      - | 7003 | ` */` |
|     16 | 7004 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 7005 | `{` |
|     17 | 7006 | `	const char *zEnd = &zIn[nByte];` |
|      - | 7007 | `	const char *zPtr,*zTag;` |
|      - | 7008 | `	SySet sSet;` |
|      - | 7009 | `	/* initialize the set of allowed tags */` |
|     17 | 7010 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 7011 | `	if( nTaglen > 0 ){` |
|      - | 7012 | `		/* Set of allowed tags */` |
|     11 | 7013 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 7014 | `	}` |
|      - | 7015 | `	/* Set the empty string */` |
|     17 | 7016 | `	ph7_result_string(pCtx,"",0);` |
|      - | 7017 | `	/* Start processing */` |
|     26 | 7018 | `	for(;;){` |
|     53 | 7019 | `		if(zIn >= zEnd){` |
|      - | 7020 | `			/* No more input to process */` |
|     15 | 7021 | `			break;` |
|      - | 7022 | `		}` |
|     39 | 7023 | `		zPtr = zIn;` |
|      - | 7024 | `		/* Find a tag */` |
|    133 | 7025 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 7026 | `			zIn++;` |
|      1 | 7027 | `		}` |
|     39 | 7028 | `		if( zIn > zPtr ){` |
|      - | 7029 | `			/* Consume raw input */` |
|     21 | 7030 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 7031 | `		}` |
|      - | 7032 | `		/* Ignore trailing null bytes */` |
|     39 | 7033 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 7034 | `			zIn++;` |
|    ! 0 | 7035 | `		}` |
|     39 | 7036 | `		if(zIn >= zEnd){` |
|      - | 7037 | `			/* No more input to process */` |
|      3 | 7038 | `			break;` |
|      - | 7039 | `		}` |
|     37 | 7040 | `		if( zIn[0] == '<' ){` |
|      - | 7041 | `			sxi32 rc;` |
|     37 | 7042 | `			zTag = zIn++;` |
|      - | 7043 | `			/* Delimit the tag */` |
|    127 | 7044 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 7045 | `				zIn++;` |
|      1 | 7046 | `			}` |
|     37 | 7047 | `			if( zIn < zEnd ){` |
|     37 | 7048 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 7049 | `			}` |
|      - | 7050 | `			/* Query the set */` |
|     37 | 7051 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 7052 | `			if( rc == SXRET_OK ){` |
|      - | 7053 | `				/* Keep the tag */` |
|     21 | 7054 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 7055 | `			}` |
|     18 | 7056 | `		}` |
|      1 | 7057 | `	}` |
|      - | 7058 | `	/* Cleanup */` |
|     17 | 7059 | `	SySetRelease(&sSet);` |
|     17 | 7060 | `	return SXRET_OK;` |
|      1 | 7061 | `}` |
|      - | 7062 | `/*` |
|      - | 7063 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 7064 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 7065 | ` * Parameters` |
|      - | 7066 | ` *  $str` |
|      - | 7067 | ` *  The input string.` |
|      - | 7068 | ` * $allowable_tags` |
|      - | 7069 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 7070 | ` * Return` |
|      - | 7071 | ` *  Returns the stripped string.` |
|      - | 7072 | ` */` |
|     14 | 7073 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7074 | `{` |
|     15 | 7075 | `	const char *zTaglist = 0;` |
|      - | 7076 | `	const char *zString;` |
|     15 | 7077 | `	int nTaglen = 0;` |
|      - | 7078 | `	int nLen;` |
|     15 | 7079 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7080 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 7081 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7082 | `		return PH7_OK;` |
|      - | 7083 | `	}` |
|      - | 7084 | `	/* Point to the raw string */` |
|     15 | 7085 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 7086 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 7087 | `		/* Allowed tag */` |
|     11 | 7088 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 7089 | `	}` |
|      - | 7090 | `	/* Process input */` |
|     15 | 7091 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 7092 | `	return PH7_OK;` |
|      8 | 7093 | `}` |
|      - | 7094 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7095 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7096 | `/*` |
|      - | 7097 | ` * string str_shuffle(string $str)` |
|      - | 7098 |  |
|      - | 7099 | ` *  Randomly shuffles a string.` |
|      - | 7100 | ` * Parameters` |
|      - | 7101 | ` *  $str` |
|      - | 7102 | ` *   The input string.` |
|      - | 7103 | ` * Return` |
|      - | 7104 | ` *  Returns the shuffled string.` |
|      - | 7105 | ` */` |
|     10 | 7106 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7107 | `{` |
|      - | 7108 | `	const char *zString;` |
|      - | 7109 | `	int nLen,i,c;` |
|      - | 7110 | `	sxu32 iR;` |
|     11 | 7111 | `	if( nArg < 1 ){` |
|      - | 7112 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7113 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7114 | `		return PH7_OK;` |
|      - | 7115 | `	}` |
|      - | 7116 | `	/* Extract the target string */` |
|     11 | 7117 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 7118 | `	if( nLen < 1 ){` |
|      - | 7119 | `		/* Nothing to shuffle */` |
|      3 | 7120 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 7121 | `		return PH7_OK;` |
|      - | 7122 | `	}` |
|      - | 7123 | `	/* Shuffle the string */` |
|     43 | 7124 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 7125 | `		/* Generate a random number first */` |
|     35 | 7126 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 7127 | `		/* Extract a random offset */` |
|     35 | 7128 | `		c = zString[iR % nLen];` |
|      - | 7129 | `		/* Append it */` |
|     35 | 7130 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 7131 | `	}` |
|      9 | 7132 | `	return PH7_OK;` |
|      6 | 7133 | `}` |
|      - | 7134 | `/*` |
|      - | 7135 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 7136 | ` *  Convert a string to an array.` |
|      - | 7137 | ` * Parameters` |
|      - | 7138 | ` * $string` |
|      - | 7139 | ` *  The input string.` |
|      - | 7140 | ` * $split_length` |
|      - | 7141 | ` *  Maximum length of the chunk.` |
|      - | 7142 | ` * Return` |
|      - | 7143 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 7144 | ` *  except possibly the last one which may be shorter.` |
|      - | 7145 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 7146 | ` *  as the first (and only) array element.` |
|      - | 7147 | ` *  An empty string returns an empty array.` |
|      - | 7148 | ` * Errors` |
|      - | 7149 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 7150 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 7151 | ` *  ValueError if $split_length is less than 1.` |
|      - | 7152 | ` */` |
|     24 | 7153 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 7154 | `{` |
|      - | 7155 | `	const char *zString,*zEnd;` |
|      - | 7156 | `	ph7_value *pArray,*pValue;` |
|      - | 7157 | `	int split_len;` |
|      - | 7158 | `	int nLen;` |
|     27 | 7159 | `	if( nArg < 1 ){` |
|    ! 0 | 7160 | `		return PH7_VmThrowException(pCtx,` |
|      - | 7161 | `			"ArgumentCountError",` |
|      - | 7162 | `			"str_split() expects at least 1 argument, %d given",` |
|    ! 0 | 7163 | `			nArg` |
|      - | 7164 | `			);` |
|      - | 7165 | `	}` |
|      - | 7166 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     36 | 7167 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     39 | 7168 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     24 | 7169 | `	    ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 7170 | `		return PH7_VmThrowException(pCtx,` |
|      - | 7171 | `			"TypeError",` |
|      - | 7172 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 7173 | `			ph7_type_name(apArg[0])` |
|      - | 7174 | `			);` |
|      - | 7175 | `	}` |
|      - | 7176 | `	/* Point to the target string */` |
|     27 | 7177 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 7178 | `	split_len = (int)sizeof(char);` |
|     27 | 7179 | `	if( nArg > 1 ){` |
|      - | 7180 | `		/* Split length */` |
|     17 | 7181 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     17 | 7182 | `		if( split_len < 1 ){` |
|      6 | 7183 | `			return PH7_VmThrowException(pCtx,` |
|      - | 7184 | `				"ValueError",` |
|      - | 7185 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 7186 | `				);` |
|      - | 7187 | `		}` |
|     11 | 7188 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 7189 | `			split_len = nLen;` |
|      1 | 7190 | `		}` |
|      5 | 7191 | `	}` |
|      - | 7192 | `	/* Create the array and the scalar value */` |
|     21 | 7193 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 7194 | `	/*Chunk value */` |
|     21 | 7195 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     21 | 7196 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 7197 | `		/* Return FALSE */` |
|    ! 0 | 7198 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7199 | `		return PH7_OK;` |
|      - | 7200 | `	}` |
|      - | 7201 | `	/* Point to the end of the string */` |
|     21 | 7202 | `	zEnd = &zString[nLen];` |
|      - | 7203 | `	/* Perform the requested operation */` |
|     48 | 7204 | `	for(;;){` |
|      - | 7205 | `		int nMax;` |
|     59 | 7206 | `		if( zString >= zEnd ){` |
|      - | 7207 | `			/* No more input to process */` |
|     21 | 7208 | `			break;` |
|      - | 7209 | `		}` |
|     39 | 7210 | `		nMax = (int)(zEnd-zString);` |
|     39 | 7211 | `		if( nMax < split_len ){` |
|      3 | 7212 | `			split_len = nMax;` |
|      1 | 7213 | `		}` |
|      - | 7214 | `		/* Copy the current chunk */` |
|     39 | 7215 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 7216 | `		/* Insert it */` |
|     39 | 7217 | `		if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){ /* Will make it's own copy */` |
|    ! 0 | 7218 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 7219 | `		}` |
|      - | 7220 | `		/* reset the string cursor */` |
|     39 | 7221 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 7222 | `		/* Update position */` |
|     39 | 7223 | `		zString += split_len;` |
|      1 | 7224 | `	}` |
|      - | 7225 | `	/*` |
|      - | 7226 | `	 * Return the array.` |
|      - | 7227 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 7228 | `	 * upon we return from this function.` |
|      - | 7229 | `	 */` |
|     21 | 7230 | `	ph7_result_value(pCtx,pArray);` |
|     21 | 7231 | `	return PH7_OK;` |
|     15 | 7232 | `}` |
|      - | 7233 | `/*` |
|      - | 7234 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 7235 | ` * Refer to [strspn()].` |
|      - | 7236 | ` */` |
|     28 | 7237 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 7238 | `{` |
|     29 | 7239 | `	const char *zIn = *pzIn;` |
|      - | 7240 | `	const char *zPtr;` |
|      - | 7241 | `	/* Ignore leading white spaces */` |
|     29 | 7242 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 7243 | `		zIn++;` |
|    ! 0 | 7244 | `	}` |
|     29 | 7245 | `	if( zIn >= zEnd ){` |
|      - | 7246 | `		/* End of input */` |
|    ! 0 | 7247 | `		return SXERR_EOF;` |
|      - | 7248 | `	}` |
|     29 | 7249 | `	zPtr = zIn;` |
|      - | 7250 | `	/* Extract the token */` |
|    201 | 7251 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 7252 | `		zIn++;` |
|      1 | 7253 | `	}` |
|     29 | 7254 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 7255 | `	/* Synchronize pointers */` |
|     29 | 7256 | `	*pzIn = zIn;` |
|      - | 7257 | `	/* Return to the caller */` |
|     29 | 7258 | `	return SXRET_OK;` |
|     15 | 7259 | `}` |
|      - | 7260 | `/*` |
|      - | 7261 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 7262 | ` * return the longest match.` |
|      - | 7263 | ` * Refer to [strspn()].` |
|      - | 7264 | ` */` |
|     18 | 7265 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 7266 | `{` |
|     19 | 7267 | `	const char *zEnd = &zString[nLen];` |
|     19 | 7268 | `	const char *zIn = zString;` |
|      - | 7269 | `	int i,c;` |
|     45 | 7270 | `	for(;;){` |
|     91 | 7271 | `		if( zString >= zEnd ){` |
|      7 | 7272 | `			break;` |
|      - | 7273 | `		}` |
|      - | 7274 | `		/* Extract current character */` |
|     85 | 7275 | `		c = zString[0];` |
|      - | 7276 | `		/* Perform the lookup */` |
|    383 | 7277 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 7278 | `			if( c == zMask[i] ){` |
|      - | 7279 | `				/* Character found */` |
|     73 | 7280 | `				break;` |
|      - | 7281 | `			}` |
|    150 | 7282 | `		}` |
|     85 | 7283 | `		if( i >= nMaskLen ){` |
|      - | 7284 | `			/* Character not in the current mask,break immediately */` |
|     13 | 7285 | `			break;` |
|      - | 7286 | `		}` |
|      - | 7287 | `		/* Advance cursor */` |
|     73 | 7288 | `		zString++;` |
|      1 | 7289 | `	}` |
|      - | 7290 | `	/* Longest match */` |
|     19 | 7291 | `	return (int)(zString-zIn);` |
|      1 | 7292 | `}` |
|      - | 7293 | `/*` |
|      - | 7294 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 7295 | ` * Refer to [strcspn()].` |
|      - | 7296 | ` */` |
|     10 | 7297 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 7298 | `{` |
|     11 | 7299 | `	const char *zEnd = &zString[nLen];` |
|     11 | 7300 | `	const char *zIn = zString;` |
|      - | 7301 | `	int i,c;` |
|     12 | 7302 | `	for(;;){` |
|     25 | 7303 | `		if( zString >= zEnd ){` |
|      3 | 7304 | `			break;` |
|      - | 7305 | `		}` |
|      - | 7306 | `		/* Extract current character */` |
|     23 | 7307 | `		c = zString[0];` |
|      - | 7308 | `		/* Perform the lookup */` |
|     51 | 7309 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 7310 | `			if( c == zMask[i] ){` |
|      9 | 7311 | `				break;` |
|      - | 7312 | `			}` |
|     15 | 7313 | `		}` |
|     23 | 7314 | `		if( i < nMaskLen ){` |
|      - | 7315 | `			/* Character in the current mask,break immediately */` |
|      9 | 7316 | `			break;` |
|      - | 7317 | `		}` |
|      - | 7318 | `		/* Advance cursor */` |
|     15 | 7319 | `		zString++;` |
|      1 | 7320 | `	}` |
|      - | 7321 | `	/* Longest match */` |
|     11 | 7322 | `	return (int)(zString-zIn);` |
|      1 | 7323 | `}` |
|      - | 7324 | `/*` |
|      - | 7325 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 7326 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 7327 | ` *  of characters contained within a given mask.` |
|      - | 7328 | ` * Parameters` |
|      - | 7329 | ` * $str` |
|      - | 7330 | ` *  The input string.` |
|      - | 7331 | ` * $mask` |
|      - | 7332 | ` *  The list of allowable characters.` |
|      - | 7333 | ` * $start` |
|      - | 7334 | ` *  The position in subject to start searching.` |
|      - | 7335 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 7336 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 7337 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 7338 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 7339 | ` *  start'th position from the end of subject.` |
|      - | 7340 | ` * $length` |
|      - | 7341 | ` *  The length of the segment from subject to examine.` |
|      - | 7342 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 7343 | ` *  characters after the starting position.` |
|      - | 7344 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 7345 | ` *  position up to length characters from the end of subject.` |
|      - | 7346 | ` * Return` |
|      - | 7347 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 7348 | ` * in mask.` |
|      - | 7349 | ` */` |
|     24 | 7350 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7351 | `{` |
|      - | 7352 | `	const char *zString,*zMask,*zEnd;` |
|      - | 7353 | `	int iMasklen,iLen;` |
|      - | 7354 | `	SyString sToken;` |
|     25 | 7355 | `	int iCount = 0;` |
|      - | 7356 | `	int rc;` |
|     25 | 7357 | `	if( nArg < 2 ){` |
|      - | 7358 | `		/* Missing agruments,return zero */` |
|    ! 0 | 7359 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7360 | `		return PH7_OK;` |
|      - | 7361 | `	}` |
|      - | 7362 | `	/* Extract the target string */` |
|     25 | 7363 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7364 | `	/* Extract the mask */` |
|     25 | 7365 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 7366 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 7367 | `		/* Nothing to process,return zero */` |
|      7 | 7368 | `		ph7_result_int(pCtx,0);` |
|      7 | 7369 | `		return PH7_OK;` |
|      - | 7370 | `	}` |
|     19 | 7371 | `	if( nArg > 2 ){` |
|      - | 7372 | `		int nOfft;` |
|      - | 7373 | `		/* Extract the offset */` |
|      9 | 7374 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 7375 | `		if( nOfft < 0 ){` |
|    ! 0 | 7376 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 7377 | `			if( zBase > zString ){` |
|    ! 0 | 7378 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 7379 | `				zString = zBase;` |
|    ! 0 | 7380 | `			}else{` |
|      - | 7381 | `				/* Invalid offset */` |
|    ! 0 | 7382 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7383 | `				return PH7_OK;` |
|      - | 7384 | `			}` |
|    ! 0 | 7385 | `		}else{` |
|      9 | 7386 | `			if( nOfft >= iLen ){` |
|      - | 7387 | `				/* Invalid offset */` |
|    ! 0 | 7388 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7389 | `				return PH7_OK;` |
|    ! 0 | 7390 | `			}else{` |
|      - | 7391 | `				/* Update offset */` |
|      9 | 7392 | `				zString += nOfft;` |
|      9 | 7393 | `				iLen -= nOfft;` |
|      - | 7394 | `			}` |
|      - | 7395 | `		}` |
|      9 | 7396 | `		if( nArg > 3 ){` |
|      - | 7397 | `			int iUserlen;` |
|      - | 7398 | `			/* Extract the desired length */` |
|      9 | 7399 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 7400 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 7401 | `				iLen = iUserlen;` |
|      2 | 7402 | `			}` |
|      4 | 7403 | `		}` |
|      4 | 7404 | `	}` |
|      - | 7405 | `	/* Point to the end of the string */` |
|     19 | 7406 | `	zEnd = &zString[iLen];` |
|      - | 7407 | `	/* Extract the first non-space token */` |
|     19 | 7408 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 7409 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 7410 | `		/* Compare against the current mask */` |
|     19 | 7411 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 7412 | `	}` |
|      - | 7413 | `	/* Longest match */` |
|     19 | 7414 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 7415 | `	return PH7_OK;` |
|     13 | 7416 | `}` |
|      - | 7417 | `/*` |
|      - | 7418 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 7419 | ` *  Find length of initial segment not matching mask.` |
|      - | 7420 | ` * Parameters` |
|      - | 7421 | ` * $str` |
|      - | 7422 | ` *  The input string.` |
|      - | 7423 | ` * $mask` |
|      - | 7424 | ` *  The list of not allowed characters.` |
|      - | 7425 | ` * $start` |
|      - | 7426 | ` *  The position in subject to start searching.` |
|      - | 7427 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 7428 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 7429 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 7430 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 7431 | ` *  start'th position from the end of subject.` |
|      - | 7432 | ` * $length` |
|      - | 7433 | ` *  The length of the segment from subject to examine.` |
|      - | 7434 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 7435 | ` *  characters after the starting position.` |
|      - | 7436 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 7437 | ` *  position up to length characters from the end of subject.` |
|      - | 7438 | ` * Return` |
|      - | 7439 | ` *  Returns the length of the segment as an integer.` |
|      - | 7440 | ` */` |
|     14 | 7441 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7442 | `{` |
|      - | 7443 | `	const char *zString,*zMask,*zEnd;` |
|      - | 7444 | `	int iMasklen,iLen;` |
|      - | 7445 | `	SyString sToken;` |
|     15 | 7446 | `	int iCount = 0;` |
|      - | 7447 | `	int rc;` |
|     15 | 7448 | `	if( nArg < 2 ){` |
|      - | 7449 | `		/* Missing agruments,return zero */` |
|    ! 0 | 7450 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7451 | `		return PH7_OK;` |
|      - | 7452 | `	}` |
|      - | 7453 | `	/* Extract the target string */` |
|     15 | 7454 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7455 | `	/* Extract the mask */` |
|     15 | 7456 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 7457 | `	if( iLen < 1 ){` |
|      - | 7458 | `		/* Nothing to process,return zero */` |
|    ! 0 | 7459 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7460 | `		return PH7_OK;` |
|      - | 7461 | `	}` |
|     15 | 7462 | `	if( iMasklen < 1 ){` |
|      - | 7463 | `		/* No given mask,return the string length */` |
|      3 | 7464 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 7465 | `		return PH7_OK;` |
|      - | 7466 | `	}` |
|     13 | 7467 | `	if( nArg > 2 ){` |
|      - | 7468 | `		int nOfft;` |
|      - | 7469 | `		/* Extract the offset */` |
|     11 | 7470 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 7471 | `		if( nOfft < 0 ){` |
|    ! 0 | 7472 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 7473 | `			if( zBase > zString ){` |
|    ! 0 | 7474 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 7475 | `				zString = zBase;` |
|    ! 0 | 7476 | `			}else{` |
|      - | 7477 | `				/* Invalid offset */` |
|    ! 0 | 7478 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7479 | `				return PH7_OK;` |
|      - | 7480 | `			}` |
|    ! 0 | 7481 | `		}else{` |
|     11 | 7482 | `			if( nOfft >= iLen ){` |
|      - | 7483 | `				/* Invalid offset */` |
|      3 | 7484 | `				ph7_result_int(pCtx,0);` |
|      3 | 7485 | `				return PH7_OK;` |
|    ! 0 | 7486 | `			}else{` |
|      - | 7487 | `				/* Update offset */` |
|      9 | 7488 | `				zString += nOfft;` |
|      9 | 7489 | `				iLen -= nOfft;` |
|      - | 7490 | `			}` |
|      - | 7491 | `		}` |
|      9 | 7492 | `		if( nArg > 3 ){` |
|      - | 7493 | `			int iUserlen;` |
|      - | 7494 | `			/* Extract the desired length */` |
|    ! 0 | 7495 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 7496 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 7497 | `				iLen = iUserlen;` |
|    ! 0 | 7498 | `			}` |
|    ! 0 | 7499 | `		}` |
|      4 | 7500 | `	}` |
|      - | 7501 | `	/* Point to the end of the string */` |
|     11 | 7502 | `	zEnd = &zString[iLen];` |
|      - | 7503 | `	/* Extract the first non-space token */` |
|     11 | 7504 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 7505 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 7506 | `		/* Compare against the current mask */` |
|     11 | 7507 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 7508 | `	}` |
|      - | 7509 | `	/* Longest match */` |
|     11 | 7510 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 7511 | `	return PH7_OK;` |
|      8 | 7512 | `}` |
|      - | 7513 | `/*` |
|      - | 7514 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 7515 | ` *  Search a string for any of a set of characters.` |
|      - | 7516 | ` * Parameters` |
|      - | 7517 | ` *  $haystack` |
|      - | 7518 | ` *   The string where char_list is looked for.` |
|      - | 7519 | ` *  $char_list` |
|      - | 7520 | ` *   This parameter is case sensitive.` |
|      - | 7521 | ` * Return` |
|      - | 7522 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 7523 | ` */` |
|      4 | 7524 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7525 | `{` |
|      - | 7526 | `	const char *zString,*zList,*zEnd;` |
|      - | 7527 | `	int iLen,iListLen,i,c;` |
|      - | 7528 | `	sxu32 nOfft,nMax;` |
|      - | 7529 | `	sxi32 rc;` |
|      5 | 7530 | `	if( nArg < 2 ){` |
|      - | 7531 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7532 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7533 | `		return PH7_OK;` |
|      - | 7534 | `	}` |
|      - | 7535 | `	/* Extract the haystack and the char list */` |
|      5 | 7536 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 7537 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 7538 | `	if( iLen < 1 ){` |
|      - | 7539 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 7540 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7541 | `		return PH7_OK;` |
|      - | 7542 | `	}` |
|      - | 7543 | `	/* Point to the end of the string */` |
|      5 | 7544 | `	zEnd = &zString[iLen];` |
|      5 | 7545 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 7546 | `	/* perform the requested operation */` |
|     15 | 7547 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 7548 | `		c = zList[i];` |
|     11 | 7549 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 7550 | `		if( rc == SXRET_OK ){` |
|      5 | 7551 | `			if( nMax < nOfft ){` |
|      3 | 7552 | `				nOfft = nMax;` |
|      1 | 7553 | `			}` |
|      2 | 7554 | `		}` |
|      6 | 7555 | `	}` |
|      5 | 7556 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 7557 | `		/* No such substring,return FALSE */` |
|      3 | 7558 | `		ph7_result_bool(pCtx,0);` |
|      2 | 7559 | `	}else{` |
|      - | 7560 | `		/* Return the substring */` |
|      3 | 7561 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 7562 | `	}` |
|      5 | 7563 | `	return PH7_OK;` |
|      3 | 7564 | `}` |
|      - | 7565 | `/* SPDX-SnippetBegin */` |
|      - | 7566 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 7567 | `/* SPDX-License-Identifier: blessing */` |
|      - | 7568 | `/*` |
|      - | 7569 | ` * string soundex(string $str)` |
|      - | 7570 | ` *  Calculate the soundex key of a string.` |
|      - | 7571 | ` * Parameters` |
|      - | 7572 | ` *  $str` |
|      - | 7573 | ` *   The input string.` |
|      - | 7574 | ` * Return` |
|      - | 7575 | ` *  Returns the soundex key as a string.` |
|      - | 7576 | ` * Note:` |
|      - | 7577 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 7578 | ` * source tree.` |
|      - | 7579 | ` */` |
|     22 | 7580 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7581 | `{` |
|      - | 7582 | `	const unsigned char *zIn;` |
|      - | 7583 | `	char zResult[8];` |
|      - | 7584 | `	int i, j;` |
|      - | 7585 | `	static const unsigned char iCode[] = {` |
|      - | 7586 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7587 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7588 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7589 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7590 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 7591 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 7592 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 7593 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 7594 | `	};` |
|     23 | 7595 | `	if( nArg < 1 ){` |
|      - | 7596 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7597 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7598 | `		return PH7_OK;` |
|      - | 7599 | `	}` |
|     23 | 7600 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     35 | 7601 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     23 | 7602 | `	if( zIn[i] ){` |
|     17 | 7603 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 7604 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 7605 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 7606 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 7607 | `			if( code>0 ){` |
|     45 | 7608 | `				if( code!=prevcode ){` |
|     33 | 7609 | `					prevcode = (unsigned char)code;` |
|     33 | 7610 | `					zResult[j++] = (char)code + '0';` |
|     16 | 7611 | `				}` |
|     23 | 7612 | `			}else{` |
|     49 | 7613 | `				prevcode = 0;` |
|      - | 7614 | `			}` |
|     47 | 7615 | `		}` |
|     33 | 7616 | `		while( j<4 ){` |
|     17 | 7617 | `			zResult[j++] = '0';` |
|      1 | 7618 | `		}` |
|     17 | 7619 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 7620 | `	}else{` |
|      - | 7621 | `	  /* No alphabetic character: PHP returns "0000" (not the SQLite "?000"). */` |
|      7 | 7622 | `	  ph7_result_string(pCtx,"0000",4);` |
|      - | 7623 | `	}` |
|     23 | 7624 | `	return PH7_OK;` |
|     12 | 7625 | `}` |
|      - | 7626 | `/* SPDX-SnippetEnd */` |
|      - | 7627 | `/*` |
|      - | 7628 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 7629 | ` *  Wraps a string to a given number of characters.` |
|      - | 7630 | ` * Parameters` |
|      - | 7631 | ` *  $str` |
|      - | 7632 | ` *   The input string.` |
|      - | 7633 | ` * $width` |
|      - | 7634 | ` *  The column width.` |
|      - | 7635 | ` * $break` |
|      - | 7636 | ` *  The line is broken using the optional break parameter.` |
|      - | 7637 | ` * Return` |
|      - | 7638 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 7639 | ` */` |
|     26 | 7640 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7641 | `{` |
|      - | 7642 | `	const char *zIn,*zBreak;` |
|      - | 7643 | `	SyBlob sWorker;` |
|      - | 7644 | `	int iLen,iBreaklen,iWidth,iCut,iStart,iSpace,iCur;` |
|      - | 7645 | `	sxi32 rc;` |
|     27 | 7646 | `	if( nArg < 1 ){` |
|      - | 7647 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7648 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7649 | `		return PH7_OK;` |
|      - | 7650 | `	}` |
|      - | 7651 | `	/* Extract the input string */` |
|     27 | 7652 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7653 | `	/* Width (default 75; PHP allows 0/negative — break at every space). */` |
|     27 | 7654 | `	iWidth = 75;` |
|     27 | 7655 | `	if( nArg > 1 ){` |
|     27 | 7656 | `		iWidth = ph7_value_to_int(apArg[1]);` |
|     13 | 7657 | `	}` |
|      - | 7658 | `	/* Break string (default "\n"). */` |
|     27 | 7659 | `	zBreak = "\n";` |
|     27 | 7660 | `	iBreaklen = (int)sizeof(char);` |
|     27 | 7661 | `	if( nArg > 2 ){` |
|     13 | 7662 | `		zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      6 | 7663 | `	}` |
|      - | 7664 | `	/* Cut long words? (default false). */` |
|     27 | 7665 | `	iCut = 0;` |
|     27 | 7666 | `	if( nArg > 3 ){` |
|      7 | 7667 | `		iCut = ph7_value_to_bool(apArg[3]);` |
|      3 | 7668 | `	}` |
|     27 | 7669 | `	if( iLen < 1 ){` |
|      - | 7670 | `		/* PHP returns the empty string for empty input before validating the other args. */` |
|      5 | 7671 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 7672 | `		return PH7_OK;` |
|      - | 7673 | `	}` |
|      - | 7674 | `	/* PHP 8 domain errors (catchable ValueError). */` |
|     23 | 7675 | `	if( iBreaklen < 1 ){` |
|      3 | 7676 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7677 | `			"wordwrap(): Argument #3 ($break) must not be empty");` |
|      - | 7678 | `	}` |
|     21 | 7679 | `	if( iWidth == 0 && iCut ){` |
|      3 | 7680 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7681 | `			"wordwrap(): Argument #4 ($cut_long_words) cannot be true when argument #2 ($width) is 0");` |
|      - | 7682 | `	}` |
|      - | 7683 | `	/*` |
|      - | 7684 | `	 * PHP's algorithm: a single left-to-right pass tracking the start of the` |
|      - | 7685 | `	 * current line (iStart) and the position of the last space seen on it` |
|      - | 7686 | `	 * (iSpace). A break is emitted when the line reaches the width, at the last` |
|      - | 7687 | `	 * space if there was one, otherwise (only when cut is enabled) hard at the` |
|      - | 7688 | `	 * boundary. An existing break sequence in the input resets the line.` |
|      - | 7689 | `	 */` |
|     19 | 7690 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     19 | 7691 | `	iStart = iSpace = iCur = 0;` |
|     19 | 7692 | `	rc = SXRET_OK;` |
|    551 | 7693 | `	while( iCur < iLen ){` |
|    533 | 7694 | `		if( iBreaklen <= iLen - iCur && SyMemcmp(&zIn[iCur],zBreak,(sxu32)iBreaklen) == 0 ){` |
|      - | 7695 | `			/* Existing break sequence in the input: copy it verbatim and reset the line. */` |
|    ! 0 | 7696 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart + iBreaklen));` |
|    ! 0 | 7697 | `			if( rc != SXRET_OK ){ goto oom; }` |
|    ! 0 | 7698 | `			iCur += iBreaklen;` |
|    ! 0 | 7699 | `			iStart = iSpace = iCur;` |
|    ! 0 | 7700 | `			continue;` |
|    533 | 7701 | `		}else if( zIn[iCur] == ' ' ){` |
|     67 | 7702 | `			if( iCur - iStart >= iWidth ){` |
|      - | 7703 | `				/* The line already fills the width at this space: break here (the space is consumed). */` |
|     13 | 7704 | `				rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     13 | 7705 | `				if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     13 | 7706 | `				if( rc != SXRET_OK ){ goto oom; }` |
|     13 | 7707 | `				iStart = iCur + 1;` |
|      6 | 7708 | `			}` |
|     67 | 7709 | `			iSpace = iCur;` |
|    500 | 7710 | `		}else if( iCut && iCur - iStart >= iWidth && iStart >= iSpace ){` |
|      - | 7711 | `			/* A word longer than the width with no space to break at: hard-cut at the boundary. */` |
|      7 | 7712 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|      7 | 7713 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|      7 | 7714 | `			if( rc != SXRET_OK ){ goto oom; }` |
|      7 | 7715 | `			iStart = iSpace = iCur;` |
|    464 | 7716 | `		}else if( iCur - iStart >= iWidth && iStart < iSpace ){` |
|      - | 7717 | `			/* Past the width mid-word: wrap back to the last space (which is consumed). */` |
|     17 | 7718 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iSpace - iStart));` |
|     17 | 7719 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     17 | 7720 | `			if( rc != SXRET_OK ){ goto oom; }` |
|     17 | 7721 | `			iStart = iSpace = iSpace + 1;` |
|      8 | 7722 | `		}` |
|    533 | 7723 | `		iCur++;` |
|      1 | 7724 | `	}` |
|      - | 7725 | `	/* Emit the trailing chunk. */` |
|     19 | 7726 | `	if( iStart < iCur ){` |
|     19 | 7727 | `		rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     19 | 7728 | `		if( rc != SXRET_OK ){ goto oom; }` |
|      9 | 7729 | `	}` |
|     19 | 7730 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|     19 | 7731 | `	SyBlobRelease(&sWorker);` |
|     19 | 7732 | `	return PH7_OK;` |
|    ! 0 | 7733 | `oom:` |
|    ! 0 | 7734 | `	SyBlobRelease(&sWorker);` |
|    ! 0 | 7735 | `	return PH7_ContextMemoryError(pCtx);` |
|     14 | 7736 | `}` |
|      - | 7737 | `/*` |
|      - | 7738 | ` * Check if the given character is a member of the given mask.` |
|      - | 7739 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 7740 | ` * Refer to [strtok()].` |
|      - | 7741 | ` */` |
|     30 | 7742 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 7743 | `{` |
|      - | 7744 | `	int i;` |
|     57 | 7745 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 7746 | `		if( c == zMask[i] ){` |
|     13 | 7747 | `			if( pOfft ){` |
|      5 | 7748 | `				*pOfft = i;` |
|      2 | 7749 | `			}` |
|     13 | 7750 | `			return TRUE;` |
|      - | 7751 | `		}` |
|     14 | 7752 | `	}` |
|     19 | 7753 | `	return FALSE;` |
|     16 | 7754 | `}` |
|      - | 7755 | `/*` |
|      - | 7756 | ` * Extract a single token from the input stream.` |
|      - | 7757 | ` * Refer to [strtok()].` |
|      - | 7758 | ` */` |
|      6 | 7759 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 7760 | `{` |
|      7 | 7761 | `	const char *zIn = *pzIn;` |
|      - | 7762 | `	const char *zPtr;` |
|      - | 7763 | `	/* Ignore leading delimiter */` |
|     11 | 7764 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 7765 | `		zIn++;` |
|      1 | 7766 | `	}` |
|      7 | 7767 | `	if( zIn >= zEnd ){` |
|      - | 7768 | `		/* End of input */` |
|    ! 0 | 7769 | `		return SXERR_EOF;` |
|      - | 7770 | `	}` |
|      7 | 7771 | `	zPtr = zIn;` |
|      - | 7772 | `	/* Extract the token */` |
|     13 | 7773 | `	while( zIn < zEnd ){` |
|     11 | 7774 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 7775 | `			/* UTF-8 stream */` |
|    ! 0 | 7776 | `			zIn++;` |
|    ! 0 | 7777 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 7778 | `		}else{` |
|     11 | 7779 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 7780 | `				break;` |
|      - | 7781 | `			}` |
|      7 | 7782 | `			zIn++;` |
|      - | 7783 | `		}` |
|      1 | 7784 | `	}` |
|      7 | 7785 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 7786 | `	/* Update the cursor */` |
|      7 | 7787 | `	*pzIn = zIn;` |
|      - | 7788 | `	/* Return to the caller */` |
|      7 | 7789 | `	return SXRET_OK;` |
|      4 | 7790 | `}` |
|      - | 7791 | `/* strtok auxiliary private data */` |
|      - | 7792 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 7793 | `struct strtok_aux_data` |
|      - | 7794 | `{` |
|      - | 7795 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 7796 | `	const char *zIn;   /* Current input stream */` |
|      - | 7797 | `	const char *zEnd;  /* End of input */` |
|      - | 7798 | `};` |
|      - | 7799 | `/*` |
|      - | 7800 | ` * string strtok(string $str,string $token)` |
|      - | 7801 | ` * string strtok(string $token)` |
|      - | 7802 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 7803 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 7804 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 7805 | ` *  words by using the space character as the token.` |
|      - | 7806 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 7807 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 7808 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 7809 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 7810 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 7811 | ` *  the argument are found.` |
|      - | 7812 | ` * Parameters` |
|      - | 7813 | ` *  $str` |
|      - | 7814 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 7815 | ` * $token` |
|      - | 7816 | ` *  The delimiter used when splitting up str.` |
|      - | 7817 | ` * Return` |
|      - | 7818 | ` *   Current token or FALSE on EOF.` |
|      - | 7819 | ` */` |
|      6 | 7820 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7821 | `{` |
|      - | 7822 | `	strtok_aux_data *pAux;` |
|      - | 7823 | `	const char *zMask;` |
|      - | 7824 | `	SyString sToken;` |
|      - | 7825 | `	int nMasklen;` |
|      - | 7826 | `	sxi32 rc;` |
|      7 | 7827 | `	if( nArg < 2 ){` |
|      - | 7828 | `		/* Extract top aux data */` |
|      5 | 7829 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      5 | 7830 | `		if( pAux == 0 ){` |
|      - | 7831 | `			/* No aux data,return FALSE */` |
|    ! 0 | 7832 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7833 | `			return PH7_OK;` |
|      - | 7834 | `		}` |
|      5 | 7835 | `		nMasklen = 0;` |
|      5 | 7836 | `		zMask = ""; /* cc warning */` |
|      5 | 7837 | `		if( nArg > 0 ){` |
|      - | 7838 | `			/* Extract the mask */` |
|      5 | 7839 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 7840 | `		}` |
|      5 | 7841 | `		if( nMasklen < 1 ){` |
|      - | 7842 | `			/* Invalid mask,return FALSE */` |
|    ! 0 | 7843 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 7844 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7845 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 7846 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7847 | `			return PH7_OK;` |
|      - | 7848 | `		}` |
|      - | 7849 | `		/* Extract the token */` |
|      5 | 7850 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 7851 | `		if( rc != SXRET_OK ){` |
|      - | 7852 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 7853 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 7854 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7855 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 7856 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7857 | `		}else{` |
|      - | 7858 | `			/* Return the extracted token */` |
|      5 | 7859 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 7860 | `		}` |
|      3 | 7861 | `	}else{` |
|      - | 7862 | `		const char *zInput,*zCur;` |
|      - | 7863 | `		char *zDup;` |
|      - | 7864 | `		int nLen;` |
|      - | 7865 | `		/* Extract the raw input */` |
|      3 | 7866 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 7867 | `		if( nLen < 1 ){` |
|      - | 7868 | `			/* Empty input,return FALSE */` |
|    ! 0 | 7869 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7870 | `			return PH7_OK;` |
|      - | 7871 | `		}` |
|      - | 7872 | `		/* Extract the mask */` |
|      3 | 7873 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 7874 | `		if( nMasklen < 1 ){` |
|      - | 7875 | `			/* Set a default mask */` |
|      - | 7876 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 7877 | `			zMask = TOK_MASK;` |
|    ! 0 | 7878 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 7879 | `#undef TOK_MASK` |
|    ! 0 | 7880 | `		}` |
|      - | 7881 | `		/* Extract a single token */` |
|      3 | 7882 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 7883 | `		if( rc != SXRET_OK ){` |
|      - | 7884 | `			/* Empty input */` |
|    ! 0 | 7885 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7886 | `			return PH7_OK;` |
|    ! 0 | 7887 | `		}else{` |
|      - | 7888 | `			/* Return the extracted token */` |
|      3 | 7889 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 7890 | `		}` |
|      - | 7891 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 7892 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 7893 | `		if( pAux ){` |
|      3 | 7894 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 7895 | `			if( nLen < 1 ){` |
|    ! 0 | 7896 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7897 | `				return PH7_OK;` |
|      - | 7898 | `			}` |
|      - | 7899 | `			/* Duplicate input */` |
|      3 | 7900 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 7901 | `			if( zDup  ){` |
|      3 | 7902 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 7903 | `				/* Register the aux data */` |
|      3 | 7904 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 7905 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 7906 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 7907 | `			}` |
|      1 | 7908 | `		}` |
|      - | 7909 | `	}` |
|      7 | 7910 | `	return PH7_OK;` |
|      4 | 7911 | `}` |
|      - | 7912 | `/*` |
|      - | 7913 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 7914 | ` *  Pad a string to a certain length with another string` |
|      - | 7915 | ` * Parameters` |
|      - | 7916 | ` *  $input` |
|      - | 7917 | ` *   The input string.` |
|      - | 7918 | ` * $pad_length` |
|      - | 7919 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 7920 | ` *   string, no padding takes place.` |
|      - | 7921 | ` * $pad_string` |
|      - | 7922 | ` *   Note:` |
|      - | 7923 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 7924 | ` *    divided by the pad_string's length.` |
|      - | 7925 | ` * $pad_type` |
|      - | 7926 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 7927 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 7928 | ` * Return` |
|      - | 7929 | ` *  The padded string.` |
|      - | 7930 | ` */` |
|     10 | 7931 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7932 | `{` |
|      - | 7933 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 7934 | `	const char *zIn,*zPad;` |
|     11 | 7935 | `	if( nArg < 2 ){` |
|      - | 7936 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7937 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7938 | `		return PH7_OK;` |
|      - | 7939 | `	}` |
|      - | 7940 | `	/* Extract the target string */` |
|     11 | 7941 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7942 | `	/* Padding length */` |
|      - | 7943 | `	{` |
|     11 | 7944 | `		sxi64 iTmp = 0;` |
|     11 | 7945 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"str_pad",2,"$length","int",&iTmp);` |
|     11 | 7946 | `		if( rcArg != PH7_OK ){` |
|    ! 0 | 7947 | `			return rcArg;` |
|      - | 7948 | `		}` |
|     11 | 7949 | `		iRealPad = iPadlen = (int)iTmp;` |
|      - | 7950 | `	}` |
|     11 | 7951 | `	if( iPadlen > 0 ){` |
|      9 | 7952 | `		iPadlen -= iLen;` |
|      4 | 7953 | `	}` |
|     11 | 7954 | `	if( iPadlen < 1  ){` |
|      - | 7955 | `		/* Return the string verbatim */` |
|      5 | 7956 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      5 | 7957 | `		return PH7_OK;` |
|      - | 7958 | `	}` |
|      7 | 7959 | `	zPad = " "; /* Whitespace padding */` |
|      7 | 7960 | `	iStrpad = (int)sizeof(char);` |
|      7 | 7961 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      7 | 7962 | `	if( nArg > 2 ){` |
|      - | 7963 | `		/* Padding string */` |
|      7 | 7964 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      7 | 7965 | `		if( iStrpad < 1 ){` |
|      - | 7966 | `			/* An empty pad string throws a catchable ValueError in PHP 8` |
|      - | 7967 | `			 * (only reached once padding is actually required). */` |
|      3 | 7968 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7969 | `				"str_pad(): Argument #3 ($pad_string) must not be empty");` |
|      - | 7970 | `		}` |
|      5 | 7971 | `		if( nArg > 3 ){` |
|      - | 7972 | `			/* Padd type */` |
|      5 | 7973 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 7974 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 7975 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 7976 | `			}` |
|      2 | 7977 | `		}` |
|      2 | 7978 | `	}` |
|      5 | 7979 | `	iDiv = 1;` |
|      5 | 7980 | `	if( iType == 2 ){` |
|    ! 0 | 7981 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 7982 | `	}` |
|      - | 7983 | `	/* Perform the requested operation */` |
|      5 | 7984 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 7985 | `		jPad = iStrpad;` |
|      5 | 7986 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 7987 | `			/* Padding */` |
|      5 | 7988 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 7989 | `				break;` |
|      - | 7990 | `			}` |
|      3 | 7991 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 7992 | `		}` |
|      3 | 7993 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 7994 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 7995 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 7996 | `				if( jPad > iStrpad ){` |
|    ! 0 | 7997 | `					jPad = iStrpad;` |
|    ! 0 | 7998 | `				}` |
|      3 | 7999 | `				if( jPad < 1){` |
|    ! 0 | 8000 | `					break;` |
|      - | 8001 | `				}` |
|      3 | 8002 | `				if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 8003 | `			}` |
|      1 | 8004 | `		}` |
|      1 | 8005 | `	}` |
|      5 | 8006 | `	if( iLen > 0 ){` |
|      - | 8007 | `		/* Append the input string */` |
|      5 | 8008 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 8009 | `	}` |
|      5 | 8010 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 8011 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 8012 | `			/* Padding */` |
|      5 | 8013 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 8014 | `				break;` |
|      - | 8015 | `			}` |
|      3 | 8016 | `			if( ph7_result_string(pCtx,zPad,iStrpad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 8017 | `		}` |
|      5 | 8018 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 8019 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 8020 | `			if( jPad > iStrpad ){` |
|    ! 0 | 8021 | `				jPad = iStrpad;` |
|    ! 0 | 8022 | `			}` |
|      3 | 8023 | `			if( jPad < 1){` |
|    ! 0 | 8024 | `				break;` |
|      - | 8025 | `			}` |
|      3 | 8026 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 8027 | `		}` |
|      1 | 8028 | `	}` |
|      5 | 8029 | `	return PH7_OK;` |
|      6 | 8030 | `}` |
|      - | 8031 | `/*` |
|      - | 8032 | ` * String replacement private data.` |
|      - | 8033 | ` */` |
|      - | 8034 | `typedef struct str_replace_data str_replace_data;` |
|      - | 8035 | `struct str_replace_data` |
|      - | 8036 | `{` |
|      - | 8037 | `	/* Used by the str_replace family to collect the search/replace arguments. */` |
|      - | 8038 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 8039 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 8040 | `	sxi32 rc;           /* Carries an allocation failure (SXERR_MEM) out of a walker */` |
|      - | 8041 | `};` |
|      - | 8042 | `/*` |
|      - | 8043 | ` * Remove a substring.` |
|      - | 8044 | ` */` |
|      - | 8045 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 8046 | `	for(;;){\` |
|      - | 8047 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 8048 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 8049 | `		++OFFT;\` |
|      - | 8050 | `	}\` |
|      - | 8051 | `}` |
|      - | 8052 | `/*` |
|      - | 8053 | ` * Shift right and insert algorithm.` |
|      - | 8054 | ` */` |
|      - | 8055 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 8056 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 8057 | `		for(;;){\` |
|      - | 8058 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 8059 | `			if(INLEN < 1 ) { break; }\` |
|      - | 8060 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 8061 | `			--INLEN; \` |
|      - | 8062 | `		}\` |
|      - | 8063 | `		for(;;){\` |
|      - | 8064 | `				if(ELEN < 1) { break; }\` |
|      - | 8065 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 8066 | `				OFFT++;\` |
|      - | 8067 | `				ENTRY++;\` |
|      - | 8068 | `				--ELEN;\` |
|      - | 8069 | `		}\` |
|      - | 8070 | `}` |
|      - | 8071 | `/*` |
|      - | 8072 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 8073 | ` * replacement string [i.e: zReplace].` |
|      - | 8074 | ` */` |
|     48 | 8075 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      5 | 8076 | `{` |
|     53 | 8077 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 8078 | `	sxu32 n,m;` |
|     53 | 8079 | `	n = SyBlobLength(pWorker);` |
|     53 | 8080 | `	m = nOfft;` |
|      - | 8081 | `	/* Delete the old entry */` |
|   6583 | 8082 | `	STRDEL(zInput,n,m,nLen);` |
|     53 | 8083 | `	SyBlobLength(pWorker) -= nLen;` |
|     53 | 8084 | `	if( nReplen > 0 ){` |
|     47 | 8085 | `		sxi32 iRep = nReplen;` |
|      - | 8086 | `		sxi32 rc;` |
|      - | 8087 | `		/*` |
|      - | 8088 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 8089 | `		 * string.` |
|      - | 8090 | `		 */` |
|     47 | 8091 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     47 | 8092 | `		if( rc != SXRET_OK ){` |
|      - | 8093 | `			/* Propagate the allocation failure so the caller can raise a fatal` |
|      - | 8094 | `			 * instead of returning a partially-replaced string as success. */` |
|    ! 0 | 8095 | `			return rc;` |
|      - | 8096 | `		}` |
|      - | 8097 | `		/* Perform the insertion now */` |
|     47 | 8098 | `		zInput = (char *)SyBlobData(pWorker);` |
|     47 | 8099 | `		n = SyBlobLength(pWorker);` |
|   6369 | 8100 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     47 | 8101 | `		SyBlobLength(pWorker) += nReplen;` |
|     21 | 8102 | `	}` |
|     53 | 8103 | `	return SXRET_OK;` |
|     29 | 8104 | `}` |
|      - | 8105 | `/*` |
|      - | 8106 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 8107 | ` * to collect search/replace string.` |
|      - | 8108 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 8109 | ` */` |
|     90 | 8110 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      5 | 8111 | `{` |
|     95 | 8112 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 8113 | `	SyString sWorker;` |
|      - | 8114 | `	const char *zIn;` |
|      - | 8115 | `	int nByte;` |
|      - | 8116 | `	/* Extract a string representation of the given argument */` |
|     95 | 8117 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     95 | 8118 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     95 | 8119 | `	if( nByte > 0 ){` |
|      - | 8120 | `		char *zDup;` |
|      - | 8121 | `		/* Duplicate the chunk */` |
|     93 | 8122 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 8123 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 8124 | `			);` |
|     93 | 8125 | `		if( zDup == 0 ){` |
|      - | 8126 | `			/* Allocation failure: carry it out and stop the walk so the caller` |
|      - | 8127 | `			 * raises a fatal instead of silently dropping a search/replace term. */` |
|    ! 0 | 8128 | `			pRep->rc = SXERR_MEM;` |
|    ! 0 | 8129 | `			return SXERR_MEM;` |
|      - | 8130 | `		}` |
|     93 | 8131 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 8132 | `		/* Save the chunk */` |
|     93 | 8133 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     44 | 8134 | `	}` |
|      - | 8135 | `	/* Save for later processing */` |
|     95 | 8136 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 8137 | `	/* All done */` |
|     45 | 8138 | `	SXUNUSED(pKey); /* cc warning */` |
|     95 | 8139 | `	return PH7_OK;` |
|     50 | 8140 | `}` |
|      - | 8141 | `/*` |
|      - | 8142 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 8143 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 8144 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 8145 | ` * Parameters` |
|      - | 8146 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 8147 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 8148 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 8149 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 8150 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 8151 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 8152 | ` * $search` |
|      - | 8153 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 8154 | ` *  to designate multiple needles.` |
|      - | 8155 | ` * $replace` |
|      - | 8156 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 8157 | ` *  to designate multiple replacements.` |
|      - | 8158 | ` * $subject` |
|      - | 8159 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 8160 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 8161 | ` *  of subject, and the return value is an array as well.` |
|      - | 8162 | ` * $count (Not used)` |
|      - | 8163 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 8164 | ` * Return` |
|      - | 8165 | ` * This function returns a string or an array with the replaced values.` |
|      - | 8166 | ` */` |
|  29806 | 8167 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 8168 | `{` |
|      - | 8169 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 8170 | `	ProcStringMatch xMatch;` |
|      - | 8171 | `	const char *zIn,*zFunc;` |
|      - | 8172 | `	str_replace_data sRep;` |
|      - | 8173 | `	SyBlob sWorker;` |
|      - | 8174 | `	SySet sReplace;` |
|      - | 8175 | `	SySet sSearch;` |
|      - | 8176 | `	int rep_str;` |
|      - | 8177 | `	int nByte;` |
|      - | 8178 | `	sxi32 rc;` |
|  29811 | 8179 | `	if( nArg < 3 ){` |
|      - | 8180 | `		/* Missing/Invalid arguments,return null */` |
|    ! 0 | 8181 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8182 | `		return PH7_OK;` |
|      - | 8183 | `	}` |
|      - | 8184 | `	/* Initialize fields */` |
|  29811 | 8185 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  29811 | 8186 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  29811 | 8187 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  29811 | 8188 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  29811 | 8189 | `	sRep.pCtx = pCtx;` |
|  29811 | 8190 | `	sRep.pCollector = &sSearch;` |
|  29811 | 8191 | `	rep_str = 0;` |
|      - | 8192 | `	/* Extract the subject */` |
|  29811 | 8193 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  29811 | 8194 | `	if( nByte < 1 ){` |
|      - | 8195 | `		/* Nothing to replace,return the empty string */` |
|     21 | 8196 | `		ph7_result_string(pCtx,"",0);` |
|     21 | 8197 | `		return PH7_OK;` |
|      - | 8198 | `	}` |
|      - | 8199 | `	/* Copy the subject */` |
|  29791 | 8200 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 8201 | `	/* Search string */` |
|  29791 | 8202 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 8203 | `		/* Collect search string */` |
|     45 | 8204 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|     25 | 8205 | `	}else{` |
|      - | 8206 | `		/* Single pattern */` |
|  29751 | 8207 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  29751 | 8208 | `		if( nByte < 1 ){` |
|      - | 8209 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 8210 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 8211 | `			return PH7_OK;` |
|      - | 8212 | `		}` |
|  29747 | 8213 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 8214 | `		/* Save for later processing */` |
|  29747 | 8215 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 8216 | `	}` |
|      - | 8217 | `	/* Replace string */` |
|  29787 | 8218 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 8219 | `		/* Collect replace string */` |
|      7 | 8220 | `		sRep.pCollector = &sReplace;` |
|      7 | 8221 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 8222 | `	}else{` |
|      - | 8223 | `		/* Single needle */` |
|  29781 | 8224 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  29781 | 8225 | `		rep_str = 1;` |
|  29781 | 8226 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 8227 | `		/* Save for later processing */` |
|  29781 | 8228 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 8229 | `	}` |
|      - | 8230 | `	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */` |
|  29787 | 8231 | `	if( sRep.rc != SXRET_OK ){` |
|    ! 0 | 8232 | `		SySetRelease(&sSearch);` |
|    ! 0 | 8233 | `		SySetRelease(&sReplace);` |
|    ! 0 | 8234 | `		SyBlobRelease(&sWorker);` |
|    ! 0 | 8235 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8236 | `	}` |
|      - | 8237 | `	/* Reset loop cursors */` |
|  29787 | 8238 | `	SySetResetCursor(&sSearch);` |
|  29787 | 8239 | `	SySetResetCursor(&sReplace);` |
|  29787 | 8240 | `	pReplace = pSearch = 0; /* cc warning */` |
|  29787 | 8241 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 8242 | `	/* Extract function name */` |
|  29787 | 8243 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 8244 | `	/* Set the default pattern match routine */` |
|  29787 | 8245 | `	xMatch = SyBlobSearch;` |
|  29787 | 8246 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 8247 | `		/* Case insensitive pattern match */` |
|     11 | 8248 | `		xMatch = iPatternMatch;` |
|      5 | 8249 | `	}` |
|      - | 8250 | `	/* Start the replace process */` |
|  59609 | 8251 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 8252 | `		sxu32 nCount,nOfft;` |
|  29827 | 8253 | `		if( pSearch->nByte <  1 ){` |
|      - | 8254 | `			/* Empty string,ignore */` |
|      3 | 8255 | `			continue;` |
|      - | 8256 | `		}` |
|      - | 8257 | `		/* Extract the replace string */` |
|  29825 | 8258 | `		if( rep_str ){` |
|  29815 | 8259 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|  14910 | 8260 | `		}else{` |
|     11 | 8261 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 8262 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 8263 | `				 * An empty string is used for the rest of replacement values` |
|      - | 8264 | `				 */` |
|      3 | 8265 | `				pReplace = 0;` |
|      1 | 8266 | `			}` |
|      - | 8267 | `		}` |
|  29825 | 8268 | `		if( pReplace == 0 ){` |
|      - | 8269 | `			/* Use an empty string instead */` |
|      3 | 8270 | `			pReplace = &sTemp;` |
|      1 | 8271 | `		}` |
|  29825 | 8272 | `		nOfft = nCount = 0;` |
|  14934 | 8273 | `		for(;;){` |
|  29873 | 8274 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 8275 | `				break;` |
|      - | 8276 | `			}` |
|      - | 8277 | `			/* Perform a pattern lookup */` |
|  44789 | 8278 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  29856 | 8279 | `				pSearch->nByte,&nOfft);` |
|  29861 | 8280 | `			if( rc != SXRET_OK ){` |
|      - | 8281 | `				/* Pattern not found */` |
|  29813 | 8282 | `				break;` |
|      - | 8283 | `			}` |
|      - | 8284 | `			/* Perform the replace operation */` |
|     53 | 8285 | `			rc = StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|     53 | 8286 | `			if( rc != SXRET_OK ){` |
|      - | 8287 | `				/* Allocation failure: surface a fatal instead of a partial result */` |
|    ! 0 | 8288 | `				SySetRelease(&sSearch);` |
|    ! 0 | 8289 | `				SySetRelease(&sReplace);` |
|    ! 0 | 8290 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8291 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8292 | `			}` |
|      - | 8293 | `			/* Increment offset counter */` |
|     53 | 8294 | `			nCount += nOfft + pReplace->nByte;` |
|      5 | 8295 | `		}` |
|      5 | 8296 | `	}` |
|      - | 8297 | `	/* All done,clean-up the mess left behind */` |
|  29787 | 8298 | `	rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  29787 | 8299 | `	SySetRelease(&sSearch);` |
|  29787 | 8300 | `	SySetRelease(&sReplace);` |
|  29787 | 8301 | `	SyBlobRelease(&sWorker);` |
|  29787 | 8302 | `	if( rc != PH7_OK ){` |
|    ! 0 | 8303 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8304 | `	}` |
|  29787 | 8305 | `	return PH7_OK;` |
|  14908 | 8306 | `}` |
|      - | 8307 | `/*` |
|      - | 8308 | ` * strtr() array form: a single (key,value) pair copied out of the replace_pairs` |
|      - | 8309 | ` * array. The bytes are owned by a persistent pool (see strtr_collect) rather than` |
|      - | 8310 | ` * the transient walker values, which HashmapWalk releases after each callback, so` |
|      - | 8311 | ` * we store byte offsets into that pool instead of raw pointers.` |
|      - | 8312 | ` */` |
|      - | 8313 | `typedef struct strtr_entry strtr_entry;` |
|      - | 8314 | `struct strtr_entry` |
|      - | 8315 | `{` |
|      - | 8316 | `	sxu32 nKeyOfft; /* Offset of the search key inside the pool */` |
|      - | 8317 | `	sxu32 nKeyLen;  /* Length of the search key */` |
|      - | 8318 | `	sxu32 nValOfft; /* Offset of the replacement inside the pool */` |
|      - | 8319 | `	sxu32 nValLen;  /* Length of the replacement */` |
|      - | 8320 | `};` |
|      - | 8321 | `typedef struct strtr_collect strtr_collect;` |
|      - | 8322 | `struct strtr_collect` |
|      - | 8323 | `{` |
|      - | 8324 | `	SyBlob *pPool;  /* Byte pool holding copied key + value bytes */` |
|      - | 8325 | `	SySet  *pTable; /* Set of strtr_entry (parallel offsets into pPool) */` |
|      - | 8326 | `	sxi32   rc;     /* Carries an allocation failure (SXERR_MEM) out of the walker */` |
|      - | 8327 | `};` |
|      - | 8328 | `/*` |
|      - | 8329 | ` * Collect one replace_pairs entry into the persistent pool/offset table.` |
|      - | 8330 | ` * PHP coerces both the key and the value to string (an integer key becomes its` |
|      - | 8331 | ` * decimal form) and ignores an empty-string key.` |
|      - | 8332 | ` */` |
|     20 | 8333 | `static int StrtrCollectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 8334 | `{` |
|     21 | 8335 | `	strtr_collect *pCol = (strtr_collect *)pUserData;` |
|      - | 8336 | `	const char *zKey,*zVal;` |
|      - | 8337 | `	strtr_entry sEnt;` |
|      - | 8338 | `	int nKey,nVal;` |
|     21 | 8339 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|     21 | 8340 | `	if( nKey < 1 ){` |
|      - | 8341 | `		/* PHP ignores an empty-string key (it also emits a warning we do not replicate). */` |
|      3 | 8342 | `		return PH7_OK;` |
|      - | 8343 | `	}` |
|     19 | 8344 | `	zVal = ph7_value_to_string(pData,&nVal);` |
|     19 | 8345 | `	sEnt.nKeyOfft = SyBlobLength(pCol->pPool);` |
|     19 | 8346 | `	sEnt.nKeyLen  = (sxu32)nKey;` |
|     19 | 8347 | `	if( SyBlobAppend(pCol->pPool,(const void *)zKey,(sxu32)nKey) != SXRET_OK ){` |
|    ! 0 | 8348 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8349 | `		return SXERR_ABORT;` |
|      - | 8350 | `	}` |
|     19 | 8351 | `	sEnt.nValOfft = SyBlobLength(pCol->pPool);` |
|     19 | 8352 | `	sEnt.nValLen  = (sxu32)nVal;` |
|     19 | 8353 | `	if( nVal > 0 && SyBlobAppend(pCol->pPool,(const void *)zVal,(sxu32)nVal) != SXRET_OK ){` |
|    ! 0 | 8354 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8355 | `		return SXERR_ABORT;` |
|      - | 8356 | `	}` |
|     19 | 8357 | `	if( SySetPut(pCol->pTable,(const void *)&sEnt) != SXRET_OK ){` |
|    ! 0 | 8358 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8359 | `		return SXERR_ABORT;` |
|      - | 8360 | `	}` |
|     19 | 8361 | `	return PH7_OK;` |
|     11 | 8362 | `}` |
|      - | 8363 | `/*` |
|      - | 8364 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 8365 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 8366 | ` *  Translate characters or replace substrings.` |
|      - | 8367 | ` * Parameters` |
|      - | 8368 | ` *  $str` |
|      - | 8369 | ` *  The string being translated.` |
|      - | 8370 | ` * $from` |
|      - | 8371 | ` *  The string being translated to to.` |
|      - | 8372 | ` * $to` |
|      - | 8373 | ` *  The string replacing from.` |
|      - | 8374 | ` * $replace_pairs` |
|      - | 8375 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 8376 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 8377 | ` * Return` |
|      - | 8378 | ` *  The translated string.` |
|      - | 8379 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 8380 | ` */` |
|     12 | 8381 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8382 | `{` |
|      - | 8383 | `	const char *zIn;` |
|      - | 8384 | `	int nLen;` |
|     13 | 8385 | `	if( nArg < 1 ){` |
|      - | 8386 | `		/* Nothing to replace,return FALSE */` |
|    ! 0 | 8387 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8388 | `		return PH7_OK;` |
|      - | 8389 | `	}` |
|     13 | 8390 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 8391 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 8392 | `		/* Invalid arguments */` |
|    ! 0 | 8393 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8394 | `		return PH7_OK;` |
|      - | 8395 | `	}` |
|     18 | 8396 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 8397 | `		strtr_collect sCol;` |
|      - | 8398 | `		SyBlob sPool,sWorker;` |
|      - | 8399 | `		SySet sTable;` |
|      - | 8400 | `		const char *zPool;` |
|      - | 8401 | `		strtr_entry *pEnt;` |
|      - | 8402 | `		sxi32 rc;` |
|      - | 8403 | `		int i,iRun;` |
|      - | 8404 | `		/*` |
|      - | 8405 | `		 * PHP's array-form strtr is a single left-to-right pass over the subject:` |
|      - | 8406 | `		 * at every position it substitutes the LONGEST replace_pairs key that` |
|      - | 8407 | `		 * matches there, then advances past the key (replacements are never` |
|      - | 8408 | `		 * rescanned). It is not a sequential per-key global replace. First copy` |
|      - | 8409 | `		 * the pairs into a persistent pool, then run that scan.` |
|      - | 8410 | `		 */` |
|     11 | 8411 | `		SyBlobInit(&sPool,&pCtx->pVm->sAllocator);` |
|     11 | 8412 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     11 | 8413 | `		SySetInit(&sTable,&pCtx->pVm->sAllocator,sizeof(strtr_entry));` |
|     11 | 8414 | `		sCol.pPool  = &sPool;` |
|     11 | 8415 | `		sCol.pTable = &sTable;` |
|     11 | 8416 | `		sCol.rc     = SXRET_OK;` |
|     11 | 8417 | `		ph7_array_walk(apArg[1],StrtrCollectWalker,&sCol);` |
|     11 | 8418 | `		if( sCol.rc != SXRET_OK ){` |
|      - | 8419 | `			/* Allocation failure while collecting the pairs: surface a fatal */` |
|    ! 0 | 8420 | `			SyBlobRelease(&sPool);` |
|    ! 0 | 8421 | `			SyBlobRelease(&sWorker);` |
|    ! 0 | 8422 | `			SySetRelease(&sTable);` |
|    ! 0 | 8423 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 8424 | `		}` |
|      - | 8425 | `		/* The pool is now stable, so offsets can be resolved against its base. */` |
|     11 | 8426 | `		zPool = (const char *)SyBlobData(&sPool);` |
|     11 | 8427 | `		rc = SXRET_OK;` |
|     11 | 8428 | `		iRun = 0; /* Start of the pending run of unmatched bytes copied verbatim. */` |
|     43 | 8429 | `		for( i = 0 ; i < nLen ; ){` |
|     33 | 8430 | `			strtr_entry *pBest = 0;` |
|     33 | 8431 | `			sxu32 nBest = 0;` |
|      - | 8432 | `			/* Pick the longest key that matches at the current position. */` |
|     33 | 8433 | `			SySetResetCursor(&sTable);` |
|     87 | 8434 | `			while( SXRET_OK == SySetGetNextEntry(&sTable,(void **)&pEnt) ){` |
|     54 | 8435 | `				if( pEnt->nKeyLen > nBest` |
|     50 | 8436 | `					&& pEnt->nKeyLen <= (sxu32)(nLen - i)` |
|     46 | 8437 | `					&& SyMemcmp(zPool + pEnt->nKeyOfft,zIn + i,pEnt->nKeyLen) == 0 ){` |
|     29 | 8438 | `					nBest = pEnt->nKeyLen;` |
|     29 | 8439 | `					pBest = pEnt;` |
|     14 | 8440 | `				}` |
|      1 | 8441 | `			}` |
|     33 | 8442 | `			if( pBest == 0 ){` |
|      - | 8443 | `				/* No key here: extend the literal run and copy it in one shot later. */` |
|      9 | 8444 | `				i++;` |
|      9 | 8445 | `				continue;` |
|      - | 8446 | `			}` |
|      - | 8447 | `			/* Flush the pending literal run, then the replacement. */` |
|     25 | 8448 | `			if( i > iRun ){` |
|      5 | 8449 | `				rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(i - iRun));` |
|      2 | 8450 | `			}` |
|     25 | 8451 | `			if( rc == SXRET_OK && pBest->nValLen > 0 ){` |
|     25 | 8452 | `				rc = SyBlobAppend(&sWorker,zPool + pBest->nValOfft,pBest->nValLen);` |
|     12 | 8453 | `			}` |
|     25 | 8454 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 8455 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 8456 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8457 | `				SySetRelease(&sTable);` |
|    ! 0 | 8458 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8459 | `			}` |
|     25 | 8460 | `			i += (int)pBest->nKeyLen;` |
|     25 | 8461 | `			iRun = i;` |
|      1 | 8462 | `		}` |
|      - | 8463 | `		/* Flush the trailing literal run. */` |
|     11 | 8464 | `		if( nLen > iRun ){` |
|      3 | 8465 | `			rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(nLen - iRun));` |
|      3 | 8466 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 8467 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 8468 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8469 | `				SySetRelease(&sTable);` |
|    ! 0 | 8470 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8471 | `			}` |
|      1 | 8472 | `		}` |
|      - | 8473 | `		/* All done, return the result string */` |
|     16 | 8474 | `		rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|     10 | 8475 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 8476 | `		/* Clean-up */` |
|     11 | 8477 | `		SyBlobRelease(&sPool);` |
|     11 | 8478 | `		SyBlobRelease(&sWorker);` |
|     11 | 8479 | `		SySetRelease(&sTable);` |
|     11 | 8480 | `		if( rc != PH7_OK ){` |
|    ! 0 | 8481 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 8482 | `		}` |
|      6 | 8483 | `	}else{` |
|      - | 8484 | `		int i,flen,tlen,c,iOfft;` |
|      - | 8485 | `		const char *zFrom,*zTo;` |
|      3 | 8486 | `		if( nArg < 3 ){` |
|      - | 8487 | `			/* Nothing to replace */` |
|    ! 0 | 8488 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8489 | `			return PH7_OK;` |
|      - | 8490 | `		}` |
|      - | 8491 | `		/* Extract given arguments */` |
|      3 | 8492 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 8493 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 8494 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 8495 | `			/* Nothing to replace */` |
|    ! 0 | 8496 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8497 | `			return PH7_OK;` |
|      - | 8498 | `		}` |
|      - | 8499 | `		/* Start the replace process */` |
|     13 | 8500 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 8501 | `			c = zIn[i];` |
|     11 | 8502 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 8503 | `				if ( iOfft < tlen ){` |
|      5 | 8504 | `					c = zTo[iOfft];` |
|      2 | 8505 | `				}` |
|      2 | 8506 | `			}` |
|     11 | 8507 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 8508 |  |
|      6 | 8509 | `		}` |
|      - | 8510 | `	}` |
|     13 | 8511 | `	return PH7_OK;` |
|      7 | 8512 | `}` |
|      - | 8513 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8514 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8515 | `/*` |
|      - | 8516 | ` * Parse an INI string.` |
|      - | 8517 |  |
|      - | 8518 | ` * According to wikipedia` |
|      - | 8519 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 8520 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 8521 | ` *  Format` |
|      - | 8522 | `*    Properties` |
|      - | 8523 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 8524 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 8525 | `*     Example:` |
|      - | 8526 | `*      name=value` |
|      - | 8527 | `*    Sections` |
|      - | 8528 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 8529 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 8530 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 8531 | `*     or the end of the file. Sections may not be nested.` |
|      - | 8532 | `*     Example:` |
|      - | 8533 | `*      [section]` |
|      - | 8534 | `*   Comments` |
|      - | 8535 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 8536 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 8537 | `*/` |
|     12 | 8538 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 8539 | `{` |
|      - | 8540 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 8541 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 8542 | `	SyHashEntry *pEntry;` |
|      - | 8543 | `	SyString sEntry;` |
|      - | 8544 | `	SyHash sHash;` |
|      - | 8545 | `	int c;` |
|      - | 8546 | `	/* Create an empty array and worker variables */` |
|     13 | 8547 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 8548 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 8549 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 8550 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 8551 | `		/* Out of memory: surface a fatal instead of returning FALSE */` |
|    ! 0 | 8552 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8553 | `	}` |
|     13 | 8554 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 8555 | `	pCur = pArray;` |
|      - | 8556 | `	/* Start the parse process */` |
|     21 | 8557 | `	for(;;){` |
|      - | 8558 | `		/* Ignore leading white spaces */` |
|     69 | 8559 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 8560 | `			zIn++;` |
|      1 | 8561 | `		}` |
|     43 | 8562 | `		if( zIn >= zEnd ){` |
|      - | 8563 | `			/* No more input to process */` |
|     13 | 8564 | `			break;` |
|      - | 8565 | `		}` |
|     31 | 8566 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 8567 | `			/* Comment til the end of line */` |
|    ! 0 | 8568 | `			zIn++;` |
|    ! 0 | 8569 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 8570 | `				zIn++;` |
|    ! 0 | 8571 | `			}` |
|    ! 0 | 8572 | `			continue;` |
|      - | 8573 | `		}` |
|      - | 8574 | `		/* Reset the string cursor of the working variable */` |
|     31 | 8575 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 8576 | `		if( zIn[0] == '[' ){` |
|      - | 8577 | `			/* Section: Extract the section name */` |
|      9 | 8578 | `			zIn++;` |
|      9 | 8579 | `			zCur = zIn;` |
|     73 | 8580 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 8581 | `				zIn++;` |
|      1 | 8582 | `			}` |
|      9 | 8583 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 8584 | `				/* Save the section name */` |
|      5 | 8585 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 8586 | `				SyStringFullTrim(&sEntry);` |
|      5 | 8587 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 8588 | `				if( sEntry.nByte > 0 ){` |
|      - | 8589 | `					/* Associate an array with the section */` |
|      5 | 8590 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 8591 | `					if( pSection ){` |
|      5 | 8592 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 8593 | `						pCur = pSection;` |
|      2 | 8594 | `					}` |
|      2 | 8595 | `				}` |
|      2 | 8596 | `			}` |
|      9 | 8597 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 8598 | `		}else{` |
|      - | 8599 | `			ph7_value *pOldCur;` |
|      - | 8600 | `			int is_array;` |
|      - | 8601 | `			int iLen;` |
|      - | 8602 | `			/* Properties */` |
|     23 | 8603 | `			is_array = 0;` |
|     23 | 8604 | `			zCur = zIn;` |
|     23 | 8605 | `			iLen = 0; /* cc warning */` |
|     23 | 8606 | `			pOldCur = pCur;` |
|    155 | 8607 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 8608 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 8609 | `					/* Array */` |
|    ! 0 | 8610 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 8611 | `					is_array = 1;` |
|    ! 0 | 8612 | `					if( iLen > 0 ){` |
|    ! 0 | 8613 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 8614 | `						/* Query the hashtable */` |
|    ! 0 | 8615 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 8616 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 8617 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 8618 | `						if( pEntry ){` |
|    ! 0 | 8619 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 8620 | `						}else{` |
|      - | 8621 | `							/* Create an empty array */` |
|    ! 0 | 8622 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 8623 | `							if( pvArr ){` |
|      - | 8624 | `								/* Save the entry */` |
|    ! 0 | 8625 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 8626 | `								/* Insert the entry */` |
|    ! 0 | 8627 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 8628 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 8629 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 8630 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 8631 | `							}` |
|      - | 8632 | `						}` |
|    ! 0 | 8633 | `						if( pvArr ){` |
|    ! 0 | 8634 | `							pCur = pvArr;` |
|    ! 0 | 8635 | `						}` |
|    ! 0 | 8636 | `					}` |
|    ! 0 | 8637 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 8638 | `						zIn++;` |
|    ! 0 | 8639 | `					}` |
|    ! 0 | 8640 | `				}` |
|    133 | 8641 | `				zIn++;` |
|      1 | 8642 | `			}` |
|     23 | 8643 | `			if( !is_array ){` |
|     23 | 8644 | `				iLen = (int)(zIn-zCur);` |
|     11 | 8645 | `			}` |
|      - | 8646 | `			/* Trim the key */` |
|     23 | 8647 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 8648 | `			SyStringFullTrim(&sEntry);` |
|     23 | 8649 | `			if( sEntry.nByte > 0 ){` |
|     23 | 8650 | `				if( !is_array ){` |
|      - | 8651 | `					/* Save the key name */` |
|     23 | 8652 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 8653 | `				}` |
|      - | 8654 | `				/* extract key value */` |
|     23 | 8655 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 8656 | `				zIn++; /* '=' */` |
|     39 | 8657 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 8658 | `					zIn++;` |
|      1 | 8659 | `				}` |
|     23 | 8660 | `				if( zIn < zEnd ){` |
|     21 | 8661 | `					zCur = zIn;` |
|     21 | 8662 | `					c = zIn[0];` |
|     21 | 8663 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 8664 | `						zIn++;` |
|      - | 8665 | `						/* Delimit the value */` |
|    ! 0 | 8666 | `						while( zIn < zEnd ){` |
|    ! 0 | 8667 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 8668 | `								break;` |
|      - | 8669 | `							}` |
|    ! 0 | 8670 | `							zIn++;` |
|    ! 0 | 8671 | `						}` |
|    ! 0 | 8672 | `						if( zIn < zEnd ){` |
|    ! 0 | 8673 | `							zIn++;` |
|    ! 0 | 8674 | `						}` |
|    ! 0 | 8675 | `					}else{` |
|    125 | 8676 | `						while( zIn < zEnd ){` |
|    123 | 8677 | `							if( zIn[0] == '\n' ){` |
|     19 | 8678 | `								if( zIn[-1] != '\\' ){` |
|     19 | 8679 | `									break;` |
|    ! 0 | 8680 | `								}` |
|    105 | 8681 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 8682 | `								/* Inline comments */` |
|    ! 0 | 8683 | `								break;` |
|      - | 8684 | `							}` |
|    105 | 8685 | `							zIn++;` |
|      1 | 8686 | `						}` |
|      - | 8687 | `					}` |
|      - | 8688 | `					/* Trim the value */` |
|     21 | 8689 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 8690 | `					SyStringFullTrim(&sEntry);` |
|     21 | 8691 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 8692 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 8693 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 8694 | `					}` |
|     21 | 8695 | `					if( sEntry.nByte > 0 ){` |
|     21 | 8696 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 8697 | `					}` |
|      - | 8698 | `					/* Insert the key and it's value */` |
|     21 | 8699 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 8700 | `				}` |
|     12 | 8701 | `			}else{` |
|    ! 0 | 8702 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 8703 | `					zIn++;` |
|    ! 0 | 8704 | `				}` |
|      - | 8705 | `			}` |
|     23 | 8706 | `			pCur = pOldCur;` |
|      - | 8707 | `		}` |
|      1 | 8708 | `	}` |
|     13 | 8709 | `	SyHashRelease(&sHash);` |
|      - | 8710 | `	/* Return the parse of the INI string */` |
|     13 | 8711 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 8712 | `	return SXRET_OK;` |
|      7 | 8713 | `}` |
|      - | 8714 | `/*` |
|      - | 8715 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 8716 | ` *  Parse a configuration string.` |
|      - | 8717 | ` * Parameters` |
|      - | 8718 | ` *  $ini` |
|      - | 8719 | ` *   The contents of the ini file being parsed.` |
|      - | 8720 | ` *  $process_sections` |
|      - | 8721 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 8722 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 8723 | ` *  $scanner_mode (Not used)` |
|      - | 8724 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 8725 | ` *   then option values will not be parsed.` |
|      - | 8726 | ` * Return` |
|      - | 8727 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 8728 | ` */` |
|     10 | 8729 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8730 | `{` |
|      - | 8731 | `	const char *zIni;` |
|      - | 8732 | `	int nByte;` |
|     11 | 8733 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8734 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 8735 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8736 | `		return PH7_OK;` |
|      - | 8737 | `	}` |
|      - | 8738 | `	/* Extract the raw INI buffer */` |
|     11 | 8739 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 8740 | `	/* Process the INI buffer; propagate an OOM abort so the fatal actually halts */` |
|     11 | 8741 | `	return PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|      6 | 8742 | `}` |
|      - | 8743 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8744 |  |
|      - | 8745 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8746 |  |
|      - | 8747 | `/*` |
|      - | 8748 | ` * Ctype Functions.` |
|      - | 8749 | ` * Status:` |
|      - | 8750 | ` *    Stable.` |
|      - | 8751 | ` */` |
|      - | 8752 | `/*` |
|      - | 8753 | ` * bool ctype_alnum(string $text)` |
|      - | 8754 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 8755 | ` * Parameters` |
|      - | 8756 | ` *  $text` |
|      - | 8757 | ` *   The tested string.` |
|      - | 8758 | ` * Return` |
|      - | 8759 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 8760 | ` */` |
|     14 | 8761 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8762 | `{` |
|      - | 8763 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8764 | `	int nLen;` |
|     15 | 8765 | `	if( nArg < 1 ){` |
|      - | 8766 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8767 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8768 | `		return PH7_OK;` |
|      - | 8769 | `	}` |
|      - | 8770 | `	/* Extract the target string */` |
|     15 | 8771 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 8772 | `	zEnd = &zIn[nLen];` |
|     15 | 8773 | `	if( nLen < 1 ){` |
|      - | 8774 | `		/* Empty string,return FALSE */` |
|      3 | 8775 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8776 | `		return PH7_OK;` |
|      - | 8777 | `	}` |
|      - | 8778 | `	/* Perform the requested operation */` |
|     32 | 8779 | `	for(;;){` |
|     65 | 8780 | `		if( zIn >= zEnd ){` |
|      - | 8781 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8782 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8783 | `			return PH7_OK;` |
|      - | 8784 | `		}` |
|     57 | 8785 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 8786 | `			break;` |
|      - | 8787 | `		}` |
|      - | 8788 | `		/* Point to the next character */` |
|     53 | 8789 | `		zIn++;` |
|      1 | 8790 | `	}` |
|      - | 8791 | `	/* The test failed,return FALSE */` |
|      5 | 8792 | `	ph7_result_bool(pCtx,0);` |
|      5 | 8793 | `	return PH7_OK;` |
|      8 | 8794 | `}` |
|      - | 8795 | `/*` |
|      - | 8796 | ` * bool ctype_alpha(string $text)` |
|      - | 8797 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 8798 | ` * Parameters` |
|      - | 8799 | ` *  $text` |
|      - | 8800 | ` *   The tested string.` |
|      - | 8801 | ` * Return` |
|      - | 8802 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 8803 | ` */` |
|     16 | 8804 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8805 | `{` |
|      - | 8806 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8807 | `	int nLen;` |
|     17 | 8808 | `	if( nArg < 1 ){` |
|      - | 8809 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8810 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8811 | `		return PH7_OK;` |
|      - | 8812 | `	}` |
|      - | 8813 | `	/* Extract the target string */` |
|     17 | 8814 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8815 | `	zEnd = &zIn[nLen];` |
|     17 | 8816 | `	if( nLen < 1 ){` |
|      - | 8817 | `		/* Empty string,return FALSE */` |
|      3 | 8818 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8819 | `		return PH7_OK;` |
|      - | 8820 | `	}` |
|      - | 8821 | `	/* Perform the requested operation */` |
|     42 | 8822 | `	for(;;){` |
|     85 | 8823 | `		if( zIn >= zEnd ){` |
|      - | 8824 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8825 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8826 | `			return PH7_OK;` |
|      - | 8827 | `		}` |
|     77 | 8828 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 8829 | `			break;` |
|      - | 8830 | `		}` |
|      - | 8831 | `		/* Point to the next character */` |
|     71 | 8832 | `		zIn++;` |
|      1 | 8833 | `	}` |
|      - | 8834 | `	/* The test failed,return FALSE */` |
|      7 | 8835 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8836 | `	return PH7_OK;` |
|      9 | 8837 | `}` |
|      - | 8838 | `/*` |
|      - | 8839 | ` * bool ctype_cntrl(string $text)` |
|      - | 8840 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 8841 | ` * Parameters` |
|      - | 8842 | ` *  $text` |
|      - | 8843 | ` *   The tested string.` |
|      - | 8844 | ` * Return` |
|      - | 8845 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 8846 | ` */` |
|     16 | 8847 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8848 | `{` |
|      - | 8849 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8850 | `	int nLen;` |
|     17 | 8851 | `	if( nArg < 1 ){` |
|      - | 8852 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8853 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8854 | `		return PH7_OK;` |
|      - | 8855 | `	}` |
|      - | 8856 | `	/* Extract the target string */` |
|     17 | 8857 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8858 | `	zEnd = &zIn[nLen];` |
|     17 | 8859 | `	if( nLen < 1 ){` |
|      - | 8860 | `		/* Empty string,return FALSE */` |
|      3 | 8861 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8862 | `		return PH7_OK;` |
|      - | 8863 | `	}` |
|      - | 8864 | `	/* Perform the requested operation */` |
|     14 | 8865 | `	for(;;){` |
|     29 | 8866 | `		if( zIn >= zEnd ){` |
|      - | 8867 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8868 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8869 | `			return PH7_OK;` |
|      - | 8870 | `		}` |
|     21 | 8871 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8872 | `			/* UTF-8 stream  */` |
|    ! 0 | 8873 | `			break;` |
|      - | 8874 | `		}` |
|     21 | 8875 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 8876 | `			break;` |
|      - | 8877 | `		}` |
|      - | 8878 | `		/* Point to the next character */` |
|     15 | 8879 | `		zIn++;` |
|      1 | 8880 | `	}` |
|      - | 8881 | `	/* The test failed,return FALSE */` |
|      7 | 8882 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8883 | `	return PH7_OK;` |
|      9 | 8884 | `}` |
|      - | 8885 | `/*` |
|      - | 8886 | ` * bool ctype_digit(string $text)` |
|      - | 8887 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 8888 | ` * Parameters` |
|      - | 8889 | ` *  $text` |
|      - | 8890 | ` *   The tested string.` |
|      - | 8891 | ` * Return` |
|      - | 8892 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 8893 | ` */` |
|   2088 | 8894 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 8895 | `{` |
|      - | 8896 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8897 | `	int nLen;` |
|   2093 | 8898 | `	if( nArg < 1 ){` |
|      - | 8899 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8900 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8901 | `		return PH7_OK;` |
|      - | 8902 | `	}` |
|      - | 8903 | `	/* Extract the target string */` |
|   2093 | 8904 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   2093 | 8905 | `	zEnd = &zIn[nLen];` |
|   2093 | 8906 | `	if( nLen < 1 ){` |
|      - | 8907 | `		/* Empty string,return FALSE */` |
|      3 | 8908 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8909 | `		return PH7_OK;` |
|      - | 8910 | `	}` |
|      - | 8911 | `	/* Perform the requested operation */` |
|   1928 | 8912 | `	for(;;){` |
|   3861 | 8913 | `		if( zIn >= zEnd ){` |
|      - | 8914 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1725 | 8915 | `			ph7_result_bool(pCtx,1);` |
|   1725 | 8916 | `			return PH7_OK;` |
|      - | 8917 | `		}` |
|   2141 | 8918 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8919 | `			/* UTF-8 stream  */` |
|    ! 0 | 8920 | `			break;` |
|      - | 8921 | `		}` |
|   2141 | 8922 | `		if( !SyisDigit(zIn[0]) ){` |
|    371 | 8923 | `			break;` |
|      - | 8924 | `		}` |
|      - | 8925 | `		/* Point to the next character */` |
|   1775 | 8926 | `		zIn++;` |
|      5 | 8927 | `	}` |
|      - | 8928 | `	/* The test failed,return FALSE */` |
|    371 | 8929 | `	ph7_result_bool(pCtx,0);` |
|    371 | 8930 | `	return PH7_OK;` |
|   1049 | 8931 | `}` |
|      - | 8932 | `/*` |
|      - | 8933 | ` * bool ctype_xdigit(string $text)` |
|      - | 8934 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 8935 | ` * Parameters` |
|      - | 8936 | ` *  $text` |
|      - | 8937 | ` *   The tested string.` |
|      - | 8938 | ` * Return` |
|      - | 8939 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 8940 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 8941 | ` */` |
|     38 | 8942 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 8943 | `{` |
|      - | 8944 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8945 | `	int nLen;` |
|     40 | 8946 | `	if( nArg < 1 ){` |
|      - | 8947 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8948 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8949 | `		return PH7_OK;` |
|      - | 8950 | `	}` |
|      - | 8951 | `	/* Extract the target string */` |
|     40 | 8952 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     40 | 8953 | `	zEnd = &zIn[nLen];` |
|     40 | 8954 | `	if( nLen < 1 ){` |
|      - | 8955 | `		/* Empty string,return FALSE */` |
|      3 | 8956 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8957 | `		return PH7_OK;` |
|      - | 8958 | `	}` |
|      - | 8959 | `	/* Perform the requested operation */` |
|     76 | 8960 | `	for(;;){` |
|    154 | 8961 | `		if( zIn >= zEnd ){` |
|      - | 8962 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     32 | 8963 | `			ph7_result_bool(pCtx,1);` |
|     32 | 8964 | `			return PH7_OK;` |
|      - | 8965 | `		}` |
|    124 | 8966 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8967 | `			/* UTF-8 stream  */` |
|    ! 0 | 8968 | `			break;` |
|      - | 8969 | `		}` |
|    124 | 8970 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 8971 | `			break;` |
|      - | 8972 | `		}` |
|      - | 8973 | `		/* Point to the next character */` |
|    118 | 8974 | `		zIn++;` |
|      2 | 8975 | `	}` |
|      - | 8976 | `	/* The test failed,return FALSE */` |
|      7 | 8977 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8978 | `	return PH7_OK;` |
|     21 | 8979 | `}` |
|      - | 8980 | `/*` |
|      - | 8981 | ` * bool ctype_graph(string $text)` |
|      - | 8982 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 8983 | ` * Parameters` |
|      - | 8984 | ` *  $text` |
|      - | 8985 | ` *   The tested string.` |
|      - | 8986 | ` * Return` |
|      - | 8987 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 8988 | ` * (no white space), FALSE otherwise.` |
|      - | 8989 | ` */` |
|     16 | 8990 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8991 | `{` |
|      - | 8992 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8993 | `	int nLen;` |
|     17 | 8994 | `	if( nArg < 1 ){` |
|      - | 8995 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8996 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8997 | `		return PH7_OK;` |
|      - | 8998 | `	}` |
|      - | 8999 | `	/* Extract the target string */` |
|     17 | 9000 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9001 | `	zEnd = &zIn[nLen];` |
|     17 | 9002 | `	if( nLen < 1 ){` |
|      - | 9003 | `		/* Empty string,return FALSE */` |
|      3 | 9004 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9005 | `		return PH7_OK;` |
|      - | 9006 | `	}` |
|      - | 9007 | `	/* Perform the requested operation */` |
|     57 | 9008 | `	for(;;){` |
|    115 | 9009 | `		if( zIn >= zEnd ){` |
|      - | 9010 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 9011 | `			ph7_result_bool(pCtx,1);` |
|      9 | 9012 | `			return PH7_OK;` |
|      - | 9013 | `		}` |
|    107 | 9014 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9015 | `			/* UTF-8 stream  */` |
|    ! 0 | 9016 | `			break;` |
|      - | 9017 | `		}` |
|    107 | 9018 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 9019 | `			break;` |
|      - | 9020 | `		}` |
|      - | 9021 | `		/* Point to the next character */` |
|    101 | 9022 | `		zIn++;` |
|      1 | 9023 | `	}` |
|      - | 9024 | `	/* The test failed,return FALSE */` |
|      7 | 9025 | `	ph7_result_bool(pCtx,0);` |
|      7 | 9026 | `	return PH7_OK;` |
|      9 | 9027 | `}` |
|      - | 9028 | `/*` |
|      - | 9029 | ` * bool ctype_print(string $text)` |
|      - | 9030 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 9031 | ` * Parameters` |
|      - | 9032 | ` *  $text` |
|      - | 9033 | ` *   The tested string.` |
|      - | 9034 | ` * Return` |
|      - | 9035 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 9036 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 9037 | ` *  or control function at all.` |
|      - | 9038 | ` */` |
|     16 | 9039 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9040 | `{` |
|      - | 9041 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9042 | `	int nLen;` |
|     17 | 9043 | `	if( nArg < 1 ){` |
|      - | 9044 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9045 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9046 | `		return PH7_OK;` |
|      - | 9047 | `	}` |
|      - | 9048 | `	/* Extract the target string */` |
|     17 | 9049 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9050 | `	zEnd = &zIn[nLen];` |
|     17 | 9051 | `	if( nLen < 1 ){` |
|      - | 9052 | `		/* Empty string,return FALSE */` |
|      3 | 9053 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9054 | `		return PH7_OK;` |
|      - | 9055 | `	}` |
|      - | 9056 | `	/* Perform the requested operation */` |
|     63 | 9057 | `	for(;;){` |
|    127 | 9058 | `		if( zIn >= zEnd ){` |
|      - | 9059 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 9060 | `			ph7_result_bool(pCtx,1);` |
|      9 | 9061 | `			return PH7_OK;` |
|      - | 9062 | `		}` |
|    119 | 9063 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9064 | `			/* UTF-8 stream  */` |
|    ! 0 | 9065 | `			break;` |
|      - | 9066 | `		}` |
|    119 | 9067 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 9068 | `			break;` |
|      - | 9069 | `		}` |
|      - | 9070 | `		/* Point to the next character */` |
|    113 | 9071 | `		zIn++;` |
|      1 | 9072 | `	}` |
|      - | 9073 | `	/* The test failed,return FALSE */` |
|      7 | 9074 | `	ph7_result_bool(pCtx,0);` |
|      7 | 9075 | `	return PH7_OK;` |
|      9 | 9076 | `}` |
|      - | 9077 | `/*` |
|      - | 9078 | ` * bool ctype_punct(string $text)` |
|      - | 9079 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 9080 | ` * Parameters` |
|      - | 9081 | ` *  $text` |
|      - | 9082 | ` *   The tested string.` |
|      - | 9083 | ` * Return` |
|      - | 9084 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 9085 | ` *  digit or blank, FALSE otherwise.` |
|      - | 9086 | ` */` |
|     18 | 9087 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9088 | `{` |
|      - | 9089 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9090 | `	int nLen;` |
|     19 | 9091 | `	if( nArg < 1 ){` |
|      - | 9092 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9093 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9094 | `		return PH7_OK;` |
|      - | 9095 | `	}` |
|      - | 9096 | `	/* Extract the target string */` |
|     19 | 9097 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 9098 | `	zEnd = &zIn[nLen];` |
|     19 | 9099 | `	if( nLen < 1 ){` |
|      - | 9100 | `		/* Empty string,return FALSE */` |
|      3 | 9101 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9102 | `		return PH7_OK;` |
|      - | 9103 | `	}` |
|      - | 9104 | `	/* Perform the requested operation */` |
|     38 | 9105 | `	for(;;){` |
|     77 | 9106 | `		if( zIn >= zEnd ){` |
|      - | 9107 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 9108 | `			ph7_result_bool(pCtx,1);` |
|      9 | 9109 | `			return PH7_OK;` |
|      - | 9110 | `		}` |
|     69 | 9111 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9112 | `			/* UTF-8 stream  */` |
|    ! 0 | 9113 | `			break;` |
|      - | 9114 | `		}` |
|     69 | 9115 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 9116 | `			break;` |
|      - | 9117 | `		}` |
|      - | 9118 | `		/* Point to the next character */` |
|     61 | 9119 | `		zIn++;` |
|      1 | 9120 | `	}` |
|      - | 9121 | `	/* The test failed,return FALSE */` |
|      9 | 9122 | `	ph7_result_bool(pCtx,0);` |
|      9 | 9123 | `	return PH7_OK;` |
|     10 | 9124 | `}` |
|      - | 9125 | `/*` |
|      - | 9126 | ` * bool ctype_space(string $text)` |
|      - | 9127 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 9128 | ` * Parameters` |
|      - | 9129 | ` *  $text` |
|      - | 9130 | ` *   The tested string.` |
|      - | 9131 | ` * Return` |
|      - | 9132 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 9133 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 9134 | ` *  and form feed characters.` |
|      - | 9135 | ` */` |
|  64463 | 9136 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 9137 | `{` |
|      - | 9138 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9139 | `	int nLen;` |
|  64468 | 9140 | `	if( nArg < 1 ){` |
|      - | 9141 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9142 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9143 | `		return PH7_OK;` |
|      - | 9144 | `	}` |
|      - | 9145 | `	/* Extract the target string */` |
|  64468 | 9146 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  64468 | 9147 | `	zEnd = &zIn[nLen];` |
|  64468 | 9148 | `	if( nLen < 1 ){` |
|      - | 9149 | `		/* Empty string,return FALSE */` |
|      3 | 9150 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9151 | `		return PH7_OK;` |
|      - | 9152 | `	}` |
|      - | 9153 | `	/* Perform the requested operation */` |
|  33145 | 9154 | `	for(;;){` |
|  66248 | 9155 | `		if( zIn >= zEnd ){` |
|      - | 9156 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1763 | 9157 | `			ph7_result_bool(pCtx,1);` |
|   1763 | 9158 | `			return PH7_OK;` |
|      - | 9159 | `		}` |
|  64490 | 9160 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9161 | `			/* UTF-8 stream  */` |
|    ! 0 | 9162 | `			break;` |
|      - | 9163 | `		}` |
|  64490 | 9164 | `		if( !SyisSpace(zIn[0]) ){` |
|  62708 | 9165 | `			break;` |
|      - | 9166 | `		}` |
|      - | 9167 | `		/* Point to the next character */` |
|   1787 | 9168 | `		zIn++;` |
|      5 | 9169 | `	}` |
|      - | 9170 | `	/* The test failed,return FALSE */` |
|  62708 | 9171 | `	ph7_result_bool(pCtx,0);` |
|  62708 | 9172 | `	return PH7_OK;` |
|  32260 | 9173 | `}` |
|      - | 9174 | `/*` |
|      - | 9175 | ` * bool ctype_lower(string $text)` |
|      - | 9176 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 9177 | ` * Parameters` |
|      - | 9178 | ` *  $text` |
|      - | 9179 | ` *   The tested string.` |
|      - | 9180 | ` * Return` |
|      - | 9181 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 9182 | ` */` |
|     16 | 9183 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9184 | `{` |
|      - | 9185 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9186 | `	int nLen;` |
|     17 | 9187 | `	if( nArg < 1 ){` |
|      - | 9188 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9189 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9190 | `		return PH7_OK;` |
|      - | 9191 | `	}` |
|      - | 9192 | `	/* Extract the target string */` |
|     17 | 9193 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9194 | `	zEnd = &zIn[nLen];` |
|     17 | 9195 | `	if( nLen < 1 ){` |
|      - | 9196 | `		/* Empty string,return FALSE */` |
|      3 | 9197 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9198 | `		return PH7_OK;` |
|      - | 9199 | `	}` |
|      - | 9200 | `	/* Perform the requested operation */` |
|     27 | 9201 | `	for(;;){` |
|     55 | 9202 | `		if( zIn >= zEnd ){` |
|      - | 9203 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 9204 | `			ph7_result_bool(pCtx,1);` |
|      5 | 9205 | `			return PH7_OK;` |
|      - | 9206 | `		}` |
|     51 | 9207 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 9208 | `			break;` |
|      - | 9209 | `		}` |
|      - | 9210 | `		/* Point to the next character */` |
|     41 | 9211 | `		zIn++;` |
|      1 | 9212 | `	}` |
|      - | 9213 | `	/* The test failed,return FALSE */` |
|     11 | 9214 | `	ph7_result_bool(pCtx,0);` |
|     11 | 9215 | `	return PH7_OK;` |
|      9 | 9216 | `}` |
|      - | 9217 | `/*` |
|      - | 9218 | ` * bool ctype_upper(string $text)` |
|      - | 9219 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 9220 | ` * Parameters` |
|      - | 9221 | ` *  $text` |
|      - | 9222 | ` *   The tested string.` |
|      - | 9223 | ` * Return` |
|      - | 9224 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 9225 | ` */` |
|     16 | 9226 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9227 | `{` |
|      - | 9228 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9229 | `	int nLen;` |
|     17 | 9230 | `	if( nArg < 1 ){` |
|      - | 9231 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9232 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9233 | `		return PH7_OK;` |
|      - | 9234 | `	}` |
|      - | 9235 | `	/* Extract the target string */` |
|     17 | 9236 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9237 | `	zEnd = &zIn[nLen];` |
|     17 | 9238 | `	if( nLen < 1 ){` |
|      - | 9239 | `		/* Empty string,return FALSE */` |
|      3 | 9240 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9241 | `		return PH7_OK;` |
|      - | 9242 | `	}` |
|      - | 9243 | `	/* Perform the requested operation */` |
|     28 | 9244 | `	for(;;){` |
|     57 | 9245 | `		if( zIn >= zEnd ){` |
|      - | 9246 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 9247 | `			ph7_result_bool(pCtx,1);` |
|      5 | 9248 | `			return PH7_OK;` |
|      - | 9249 | `		}` |
|     53 | 9250 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 9251 | `			break;` |
|      - | 9252 | `		}` |
|      - | 9253 | `		/* Point to the next character */` |
|     43 | 9254 | `		zIn++;` |
|      1 | 9255 | `	}` |
|      - | 9256 | `	/* The test failed,return FALSE */` |
|     11 | 9257 | `	ph7_result_bool(pCtx,0);` |
|     11 | 9258 | `	return PH7_OK;` |
|      9 | 9259 | `}` |
|      - | 9260 | `/* Date/Time functions moved to builtin_date.c */` |
|      - | 9261 | `/*` |
|      - | 9262 | ` * Section:` |
|      - | 9263 | ` *    URL handling Functions.` |
|      - | 9264 | ` * Status:` |
|      - | 9265 | ` *    Stable.` |
|      - | 9266 | ` */` |
|      - | 9267 | `/*` |
|      - | 9268 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 9269 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 9270 | ` */` |
|   1026 | 9271 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 9272 | `{` |
|      - | 9273 | `	/* Store in the call context result buffer */` |
|   1028 | 9274 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 9275 | `	return SXRET_OK;` |
|      2 | 9276 | `}` |
|      - | 9277 | `/*` |
|      - | 9278 | ` * string base64_encode(string $data)` |
|      - | 9279 | ` * string convert_uuencode(string $data)` |
|      - | 9280 | ` *  Encodes data with MIME base64` |
|      - | 9281 | ` * Parameter` |
|      - | 9282 | ` *  $data` |
|      - | 9283 | ` *    Data to encode` |
|      - | 9284 | ` * Return` |
|      - | 9285 | ` *  Encoded data or FALSE on failure.` |
|      - | 9286 | ` */` |
|      6 | 9287 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9288 | `{` |
|      - | 9289 | `	const char *zIn;` |
|      - | 9290 | `	int nLen;` |
|      7 | 9291 | `	if( nArg < 1 ){` |
|      - | 9292 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9293 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9294 | `		return PH7_OK;` |
|      - | 9295 | `	}` |
|      - | 9296 | `	/* Extract the input string */` |
|      7 | 9297 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 9298 | `	if( nLen < 1 ){` |
|      - | 9299 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 9300 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9301 | `		return PH7_OK;` |
|      - | 9302 | `	}` |
|      - | 9303 | `	/* Perform the BASE64 encoding */` |
|      7 | 9304 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 9305 | `	return PH7_OK;` |
|      4 | 9306 | `}` |
|      - | 9307 | `/*` |
|      - | 9308 | ` * string base64_decode(string $data)` |
|      - | 9309 | ` * string convert_uudecode(string $data)` |
|      - | 9310 | ` *  Decodes data encoded with MIME base64` |
|      - | 9311 | ` * Parameter` |
|      - | 9312 | ` *  $data` |
|      - | 9313 | ` *    Encoded data.` |
|      - | 9314 | ` * Return` |
|      - | 9315 | ` *  Returns the original data or FALSE on failure.` |
|      - | 9316 | ` */` |
|     34 | 9317 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 9318 | `{` |
|      - | 9319 | `	const char *zIn;` |
|      - | 9320 | `	int nLen;` |
|     36 | 9321 | `	if( nArg < 1 ){` |
|      - | 9322 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9323 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9324 | `		return PH7_OK;` |
|      - | 9325 | `	}` |
|      - | 9326 | `	/* Extract the input string */` |
|     36 | 9327 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 9328 | `	if( nLen < 1 ){` |
|      - | 9329 | `		/* php decodes the empty string to the EMPTY STRING, not FALSE (FALSE is reserved` |
|      - | 9330 | `		 * for input that cannot be decoded at all). */` |
|      3 | 9331 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 9332 | `		return PH7_OK;` |
|      - | 9333 | `	}` |
|      - | 9334 | `	/* Perform the BASE64 decoding */` |
|     34 | 9335 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 9336 | `	return PH7_OK;` |
|     19 | 9337 | `}` |
|      - | 9338 | `/*` |
|      - | 9339 | ` * string urlencode(string $str)` |
|      - | 9340 | ` *  URL encoding` |
|      - | 9341 | ` * Parameter` |
|      - | 9342 | ` *  $data` |
|      - | 9343 | ` *   Input string.` |
|      - | 9344 | ` * Return` |
|      - | 9345 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 9346 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 9347 | ` *  encoded as plus (+) signs.` |
|      - | 9348 | ` */` |
|      4 | 9349 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9350 | `{` |
|      - | 9351 | `	const char *zIn;` |
|      - | 9352 | `	int nLen;` |
|      5 | 9353 | `	if( nArg < 1 ){` |
|      - | 9354 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9355 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9356 | `		return PH7_OK;` |
|      - | 9357 | `	}` |
|      - | 9358 | `	/* Extract the input string */` |
|      5 | 9359 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 9360 | `	if( nLen < 1 ){` |
|      - | 9361 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 9362 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9363 | `		return PH7_OK;` |
|      - | 9364 | `	}` |
|      - | 9365 | `	/* Perform the URL encoding */` |
|      5 | 9366 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 9367 | `	return PH7_OK;` |
|      3 | 9368 | `}` |
|      - | 9369 | `/*` |
|      - | 9370 | ` * string urldecode(string $str)` |
|      - | 9371 | ` *  Decodes any %## encoding in the given string.` |
|      - | 9372 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 9373 | ` * Parameter` |
|      - | 9374 | ` *  $data` |
|      - | 9375 | ` *    Input string.` |
|      - | 9376 | ` * Return` |
|      - | 9377 | ` *  Decoded URL or FALSE on failure.` |
|      - | 9378 | ` */` |
|      6 | 9379 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9380 | `{` |
|      - | 9381 | `	const char *zIn;` |
|      - | 9382 | `	int nLen;` |
|      7 | 9383 | `	if( nArg < 1 ){` |
|      - | 9384 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9385 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9386 | `		return PH7_OK;` |
|      - | 9387 | `	}` |
|      - | 9388 | `	/* Extract the input string */` |
|      7 | 9389 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 9390 | `	if( nLen < 1 ){` |
|      - | 9391 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 9392 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9393 | `		return PH7_OK;` |
|      - | 9394 | `	}` |
|      - | 9395 | `	/* Perform the URL decoding */` |
|      7 | 9396 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 9397 | `	return PH7_OK;` |
|      4 | 9398 | `}` |
|      - | 9399 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9400 | `/* Table of the built-in functions */` |
|      - | 9401 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 9402 | `	   /* Variable handling functions */` |
|      - | 9403 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 9404 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 9405 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 9406 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 9407 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 9408 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 9409 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 9410 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 9411 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 9412 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 9413 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 9414 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 9415 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 9416 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 9417 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 9418 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 9419 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 9420 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 9421 | `	{ "boolval"    , PH7_builtin_boolval     },` |
|      - | 9422 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 9423 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9424 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 9425 | `	   /* Math functions */` |
|      - | 9426 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 9427 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 9428 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 9429 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 9430 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 9431 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 9432 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 9433 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 9434 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 9435 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 9436 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 9437 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 9438 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 9439 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 9440 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 9441 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 9442 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 9443 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 9444 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 9445 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 9446 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 9447 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 9448 | `	{ "round",    PH7_builtin_round        },` |
|      - | 9449 | `	{ "intdiv",   PH7_builtin_intdiv       },` |
|      - | 9450 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 9451 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 9452 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 9453 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 9454 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 9455 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 9456 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 9457 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 9458 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9459 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9460 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 9461 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9462 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9463 | `	   /* String handling functions */` |
|      - | 9464 |  |
|      - | 9465 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 9466 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 9467 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 9468 | `	{ "substr_replace",  PH7_builtin_substr_replace },` |
|      - | 9469 | `	{ "levenshtein",     PH7_builtin_levenshtein },` |
|      - | 9470 | `	{ "similar_text",    PH7_builtin_similar_text },` |
|      - | 9471 | `	{ "str_word_count",  PH7_builtin_str_word_count },` |
|      - | 9472 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 9473 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 9474 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 9475 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 9476 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 9477 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 9478 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 9479 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 9480 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 9481 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 9482 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 9483 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 9484 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 9485 | `	{ "strnatcmp"  , PH7_builtin_strnatcmp  },` |
|      - | 9486 | `	{ "strnatcasecmp", PH7_builtin_strnatcmp },` |
|      - | 9487 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 9488 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 9489 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 9490 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 9491 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 9492 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 9493 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 9494 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 9495 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 9496 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 9497 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 9498 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 9499 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 9500 | `	{ "mb_strtolower",PH7_builtin_mb_case_f }, /* UTF-8 only (builtin_mb.c) */` |
|      - | 9501 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 9502 | `	{ "mb_strtoupper",PH7_builtin_mb_case_f }, /* UTF-8 only (builtin_mb.c) */` |
|      - | 9503 | `	{ "mb_strlen",    PH7_builtin_mb_strlen_f },` |
|      - | 9504 | `	{ "mb_substr",    PH7_builtin_mb_substr_f },` |
|      - | 9505 | `	{ "mb_convert_case", PH7_builtin_mb_convert_case_f },` |
|      - | 9506 | `	{ "mb_strpos",    PH7_builtin_mb_strpos_f },` |
|      - | 9507 | `	{ "mb_stripos",   PH7_builtin_mb_strpos_f },` |
|      - | 9508 | `	{ "mb_strrpos",   PH7_builtin_mb_strpos_f },` |
|      - | 9509 | `	{ "mb_str_split", PH7_builtin_mb_str_split_f },` |
|      - | 9510 | `	{ "mb_internal_encoding", PH7_builtin_mb_internal_encoding_f },` |
|      - | 9511 | `	{ "mb_check_encoding",    PH7_builtin_mb_check_encoding_f },` |
|      - | 9512 | `	{ "mb_strwidth",  PH7_builtin_mb_strwidth_f },` |
|      - | 9513 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 9514 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 9515 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 9516 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 9517 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 9518 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 9519 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 9520 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 9521 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 9522 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 9523 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 9524 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 9525 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 9526 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 9527 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 9528 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 9529 | `	{ "str_contains", PH7_builtin_str_contains },` |
|      - | 9530 | `	{ "str_starts_with", PH7_builtin_str_starts_with },` |
|      - | 9531 | `	{ "str_ends_with", PH7_builtin_str_ends_with },` |
|      - | 9532 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 9533 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9534 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9535 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 9536 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 9537 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 9538 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 9539 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9540 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9541 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 9542 |  |
|      - | 9543 |  |
|      - | 9544 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 9545 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 9546 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 9547 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 9548 | `	{ "hash",         PH7_builtin_hash      },` |
|      - | 9549 | `	{ "hash_hmac",    PH7_builtin_hash_hmac },` |
|      - | 9550 | `	{ "hash_equals",  PH7_builtin_hash_equals },` |
|      - | 9551 | `	{ "hash_algos",   PH7_builtin_hash_algos },` |
|      - | 9552 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 9553 | `	{ "password_hash",         PH7_builtin_password_hash },` |
|      - | 9554 | `	{ "password_verify",       PH7_builtin_password_verify },` |
|      - | 9555 | `	{ "password_get_info",     PH7_builtin_password_get_info },` |
|      - | 9556 | `	{ "password_needs_rehash", PH7_builtin_password_needs_rehash },` |
|      - | 9557 | `	{ "filter_var",            PH7_builtin_filter_var },` |
|      - | 9558 | `	{ "filter_input",          PH7_builtin_filter_input },` |
|      - | 9559 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9560 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9561 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 9562 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 9563 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9564 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9565 |  |
|      - | 9566 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 9567 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 9568 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 9569 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 9570 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 9571 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 9572 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 9573 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 9574 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 9575 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 9576 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 9577 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 9578 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9579 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9580 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 9581 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9582 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9583 |  |
|      - | 9584 | `	         /* Ctype functions */` |
|      - | 9585 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 9586 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 9587 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 9588 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 9589 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 9590 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 9591 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 9592 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 9593 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 9594 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 9595 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 9596 | `	         /* Time functions */` |
|      - | 9597 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 9598 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 9599 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 9600 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 9601 | `	{ "date",        PH7_builtin_date         },` |
|      - | 9602 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 9603 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 9604 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 9605 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 9606 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 9607 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 9608 | `	{ "date_default_timezone_get", PH7_builtin_date_default_timezone_get },` |
|      - | 9609 | `	{ "date_default_timezone_set", PH7_builtin_date_default_timezone_set },` |
|      - | 9610 | `	        /* URL functions */` |
|      - | 9611 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 9612 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 9613 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 9614 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 9615 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 9616 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 9617 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 9618 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 9619 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9620 | `};` |
|      - | 9621 | `/*` |
|      - | 9622 | ` * Register the built-in functions defined above,the array functions` |
|      - | 9623 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 9624 | ` */` |
|   3348 | 9625 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      5 | 9626 | `{` |
|      - | 9627 | `	sxu32 n;` |
| 622733 | 9628 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 619385 | 9629 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 309695 | 9630 | `	}` |
|      - | 9631 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   3353 | 9632 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 9633 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   3353 | 9634 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   3353 | 9635 | `}` |
|      - | 9636 |  |
