# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4459/5201 lines (85.73%)

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
| 494466 |   18 | `PH7_PRIVATE sxi32 PH7_IntArgResolve(` |
|      - |   19 | `	ph7_context *pCtx,` |
|      - |   20 | `	ph7_value *pArg,` |
|      - |   21 | `	const char *zFunc,` |
|      - |   22 | `	int iArgNum,` |
|      - |   23 | `	const char *zParamName,` |
|      - |   24 | `	const char *zTypeStr,` |
|      - |   25 | `	sxi64 *pOut` |
|      5 |   26 | `){` |
| 494471 |   27 | `	if( ph7_value_is_null(pArg) ){` |
|    ! 0 |   28 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|      - |   29 | `			"%s(): Passing null to parameter #%d (%s) of type %s is deprecated",` |
|    ! 0 |   30 | `			zFunc,iArgNum,zParamName,zTypeStr` |
|      - |   31 | `			);` |
|    ! 0 |   32 | `		*pOut = 0;` |
|    ! 0 |   33 | `		return PH7_OK;` |
|      - |   34 | `	}` |
| 494471 |   35 | `	if( ph7_value_is_float(pArg) ){` |
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
| 494465 |   56 | `	if( ph7_value_is_string(pArg) ){` |
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
| 494451 |   98 | `	if( !ph7_value_is_int(pArg) && !ph7_value_is_bool(pArg) ){` |
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
| 494451 |  113 | `	*pOut = ph7_value_to_int64(pArg);` |
| 494451 |  114 | `	return PH7_OK;` |
| 247238 |  115 | `}` |
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
|     74 |  138 | `static int PH7_builtin_is_bool(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  139 | `{` |
|     76 |  140 | `	int res = 0; /* Assume false by default */` |
|     76 |  141 | `	if( nArg > 0 ){` |
|     76 |  142 | `		res = ph7_value_is_bool(apArg[0]);` |
|     37 |  143 | `	}` |
|      - |  144 | `	/* Query result */` |
|     76 |  145 | `	ph7_result_bool(pCtx,res);` |
|     76 |  146 | `	return PH7_OK;` |
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
|    310 |  158 | `static int PH7_builtin_is_float(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  159 | `{` |
|    311 |  160 | `	int res = 0; /* Assume false by default */` |
|    311 |  161 | `	if( nArg > 0 ){` |
|    311 |  162 | `		res = ph7_value_is_float(apArg[0]);` |
|    155 |  163 | `	}` |
|      - |  164 | `	/* Query result */` |
|    311 |  165 | `	ph7_result_bool(pCtx,res);` |
|    311 |  166 | `	return PH7_OK;` |
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
|    926 |  178 | `static int PH7_builtin_is_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  179 | `{` |
|    930 |  180 | `	int res = 0; /* Assume false by default */` |
|    930 |  181 | `	if( nArg > 0 ){` |
|      - |  182 | `		/* Strict PHP identity: a float is never an int, even when it holds an` |
|      - |  183 | `		 * integer value (1.0). An integer-valued real carries both MEMOBJ_INT` |
|      - |  184 | `		 * (cached) and MEMOBJ_REAL, so REAL must be excluded here. */` |
|    930 |  185 | `		res = ph7_value_is_int(apArg[0]) && !ph7_value_is_float(apArg[0]);` |
|    463 |  186 | `	}` |
|      - |  187 | `	/* Query result */` |
|    930 |  188 | `	ph7_result_bool(pCtx,res);` |
|    930 |  189 | `	return PH7_OK;` |
|      4 |  190 | `}` |
|      - |  191 | `/*` |
|      - |  192 | ` * bool is_string($var)` |
|      - |  193 | ` *  Finds out whether a variable is a string.` |
|      - |  194 | ` * Parameters` |
|      - |  195 | ` *   $var: The variable being evaluated.` |
|      - |  196 | ` * Return` |
|      - |  197 | ` *  TRUE if var is string. False otherwise.` |
|      - |  198 | ` */` |
|   1116 |  199 | `static int PH7_builtin_is_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  200 | `{` |
|   1120 |  201 | `	int res = 0; /* Assume false by default */` |
|   1120 |  202 | `	if( nArg > 0 ){` |
|   1120 |  203 | `		res = ph7_value_is_string(apArg[0]);` |
|    558 |  204 | `	}` |
|      - |  205 | `	/* Query result */` |
|   1120 |  206 | `	ph7_result_bool(pCtx,res);` |
|   1120 |  207 | `	return PH7_OK;` |
|      4 |  208 | `}` |
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
|     94 |  235 | `static int PH7_builtin_is_numeric(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  236 | `{` |
|     99 |  237 | `	int res = 0; /* Assume false by default */` |
|     99 |  238 | `	if( nArg > 0 ){` |
|      - |  239 | `		/* Strict PHP semantics: only int/float and numeric strings are numeric.` |
|      - |  240 | `		 * PHL's lenient helper also reports booleans as numeric (they coerce for` |
|      - |  241 | `		 * arithmetic), but php's is_numeric() rejects true/false, so exclude` |
|      - |  242 | `		 * MEMOBJ_BOOL here. */` |
|     99 |  243 | `		res = ph7_value_is_numeric(apArg[0]) && !ph7_value_is_bool(apArg[0]);` |
|     47 |  244 | `	}` |
|      - |  245 | `	/* Query result */` |
|     99 |  246 | `	ph7_result_bool(pCtx,res);` |
|     99 |  247 | `	return PH7_OK;` |
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
|    808 |  278 | `static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  279 | `{` |
|    812 |  280 | `	int res = 0; /* Assume false by default */` |
|    812 |  281 | `	if( nArg > 0 ){` |
|    812 |  282 | `		res = ph7_value_is_array(apArg[0]);` |
|    404 |  283 | `	}` |
|      - |  284 | `	/* Query result */` |
|    812 |  285 | `	ph7_result_bool(pCtx,res);` |
|    812 |  286 | `	return PH7_OK;` |
|      4 |  287 | `}` |
|      - |  288 | `/*` |
|      - |  289 | ` * bool is_object($var)` |
|      - |  290 | ` *  Find out whether a variable is an object.` |
|      - |  291 | ` * Parameters` |
|      - |  292 | ` *  $var: The variable being evaluated.` |
|      - |  293 | ` * Return` |
|      - |  294 | ` *  True if var is an object. False otherwise.` |
|      - |  295 | ` */` |
|   1570 |  296 | `static int PH7_builtin_is_object(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  297 | `{` |
|   1573 |  298 | `	int res = 0; /* Assume false by default */` |
|   1573 |  299 | `	if( nArg > 0 ){` |
|   1573 |  300 | `		res = ph7_value_is_object(apArg[0]);` |
|    785 |  301 | `	}` |
|      - |  302 | `	/* Query result */` |
|   1573 |  303 | `	ph7_result_bool(pCtx,res);` |
|   1573 |  304 | `	return PH7_OK;` |
|      3 |  305 | `}` |
|      - |  306 | `/*` |
|      - |  307 | ` * bool is_resource($var)` |
|      - |  308 | ` *  Find out whether a variable is a resource.` |
|      - |  309 | ` * Parameters` |
|      - |  310 | ` *  $var: The variable being evaluated.` |
|      - |  311 | ` * Return` |
|      - |  312 | ` *  True if a resource. False otherwise.` |
|      - |  313 | ` */` |
|     78 |  314 | `static int PH7_builtin_is_resource(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  315 | `{` |
|     82 |  316 | `	int res = 0; /* Assume false by default */` |
|     82 |  317 | `	if( nArg > 0 && ph7_value_is_resource(apArg[0]) ){` |
|      - |  318 | `		/* A handle closed via fclose()/closedir()/pclose() is no longer a` |
|      - |  319 | `		 * live resource — php's is_resource() returns false for it. */` |
|     57 |  320 | `		res = !PH7_VfsResourceIsClosed(apArg[0]->x.pOther);` |
|     27 |  321 | `	}` |
|     82 |  322 | `	ph7_result_bool(pCtx,res);` |
|     82 |  323 | `	return PH7_OK;` |
|      4 |  324 | `}` |
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
|     50 |  354 | `static int PH7_builtin_intval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  355 | `{` |
|     51 |  356 | `	if( nArg < 1 ){` |
|      - |  357 | `		/* return 0 */` |
|    ! 0 |  358 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  359 | `	}else{` |
|      - |  360 | `		sxi64 iVal;` |
|      - |  361 | `		/* Perform the cast */` |
|     51 |  362 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|     51 |  363 | `		ph7_result_int64(pCtx,iVal);` |
|      - |  364 | `	}` |
|     51 |  365 | `	return PH7_OK;` |
|      1 |  366 | `}` |
|      - |  367 | `/*` |
|      - |  368 | ` * string strval($var)` |
|      - |  369 | ` *  Get the string representation of a variable.` |
|      - |  370 | ` * Parameter` |
|      - |  371 | ` *  $var: The variable being processed.` |
|      - |  372 | ` * Return` |
|      - |  373 | ` *  the string value of a variable.` |
|      - |  374 | ` */` |
|      2 |  375 | `static int PH7_builtin_strval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  376 | `{` |
|      3 |  377 | `	if( nArg < 1 ){` |
|      - |  378 | `		/* return NULL */` |
|    ! 0 |  379 | `		ph7_result_null(pCtx);` |
|    ! 0 |  380 | `	}else{` |
|      - |  381 | `		const char *zVal;` |
|      3 |  382 | `		int iLen = 0; /* cc -O6 warning */` |
|      - |  383 | `		/* Perform the cast */` |
|      3 |  384 | `		zVal = ph7_value_to_string(apArg[0],&iLen);` |
|      3 |  385 | `		ph7_result_string(pCtx,zVal,iLen);` |
|      - |  386 | `	}` |
|      3 |  387 | `	return PH7_OK;` |
|      1 |  388 | `}` |
|      - |  389 | `/*` |
|      - |  390 | ` * bool boolval($var)` |
|      - |  391 | ` *  Get the boolean value of a variable.` |
|      - |  392 | ` * Parameter` |
|      - |  393 | ` *  $var: The variable being processed.` |
|      - |  394 | ` * Return` |
|      - |  395 | ` *  the bool value of a variable.` |
|      - |  396 | ` */` |
|     14 |  397 | `static int PH7_builtin_boolval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  398 | `{` |
|      - |  399 | `	int bVal;` |
|     15 |  400 | `	if( nArg != 1 ){` |
|    ! 0 |  401 | `		return PH7_VmThrowException(pCtx,` |
|      - |  402 | `			"ArgumentCountError",` |
|      - |  403 | `			"boolval() expects exactly 1 argument, %d given",` |
|    ! 0 |  404 | `			nArg` |
|      - |  405 | `			);` |
|      - |  406 | `	}` |
|      - |  407 | `	/* Perform the cast */` |
|     15 |  408 | `	bVal = ph7_value_to_bool(apArg[0]);` |
|     15 |  409 | `	ph7_result_bool(pCtx,bVal);` |
|     15 |  410 | `	return PH7_OK;` |
|      8 |  411 | `}` |
|      - |  412 | `/*` |
|      - |  413 | ` * bool empty($var)` |
|      - |  414 | ` *  Determine whether a variable is empty.` |
|      - |  415 | ` * Parameters` |
|      - |  416 | ` *   $var: The variable being checked.` |
|      - |  417 | ` * Return` |
|      - |  418 | ` *  0 if var has a non-empty and non-zero value.1 otherwise.` |
|      - |  419 | ` */` |
|  40894 |  420 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  421 | `{` |
|  40899 |  422 | `	int res = 1; /* Assume empty by default */` |
|  40899 |  423 | `	if( nArg > 0 ){` |
|  40897 |  424 | `		res = ph7_value_is_empty(apArg[0]);` |
|  20446 |  425 | `	}` |
|  40899 |  426 | `	ph7_result_bool(pCtx,res);` |
|  40899 |  427 | `	return PH7_OK;` |
|      - |  428 |  |
|      5 |  429 | `}` |
|      - |  430 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |  431 | `#define PH7_NEED_BUILTIN_REG 1` |
|      - |  432 | `#endif` |
|      - |  433 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - |  434 | `#define PH7_NEED_FMT_AND_INI 1` |
|      - |  435 | `#endif` |
|      - |  436 |  |
|      - |  437 | `/* Math functions moved to builtin_math.c */` |
|      - |  438 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - |  439 | `/*` |
|      - |  440 | ` * Section:` |
|      - |  441 | ` *    String handling Functions.` |
|      - |  442 | ` * Status:` |
|      - |  443 | ` *    Stable.` |
|      - |  444 | ` */` |
|      - |  445 | `/*` |
|      - |  446 | ` * string substr(string $string,int $start[, int $length ])` |
|      - |  447 | ` *  Return part of a string.` |
|      - |  448 | ` * Parameters` |
|      - |  449 | ` *  $string` |
|      - |  450 | ` *   The input string. Must be one character or longer.` |
|      - |  451 | ` * $start` |
|      - |  452 | ` *   If start is non-negative, the returned string will start at the start'th position` |
|      - |  453 | ` *   in string, counting from zero. For instance, in the string 'abcdef', the character` |
|      - |  454 | ` *   at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - |  455 | ` *   If start is negative, the returned string will start at the start'th character` |
|      - |  456 | ` *   from the end of string.` |
|      - |  457 | ` *   If string is less than or equal to start characters long, FALSE will be returned.` |
|      - |  458 | ` * $length` |
|      - |  459 | ` *   If length is given and is positive, the string returned will contain at most length` |
|      - |  460 | ` *   characters beginning from start (depending on the length of string).` |
|      - |  461 | ` *   If length is given and is negative, then that many characters will be omitted from` |
|      - |  462 | ` *   the end of string (after the start position has been calculated when a start is negative).` |
|      - |  463 | ` *   If start denotes the position of this truncation or beyond, false will be returned.` |
|      - |  464 | ` *   If length is given and is 0, FALSE or NULL an empty string will be returned.` |
|      - |  465 | ` *   If length is omitted, the substring starting from start until the end of the string` |
|      - |  466 | ` *   will be returned.` |
|      - |  467 | ` * Return` |
|      - |  468 | ` *  Returns the extracted part of string, or FALSE on failure or an empty string.` |
|      - |  469 | ` */` |
| 272540 |  470 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  471 | `{` |
|      - |  472 | `	const char *zSource;` |
|      - |  473 | `	int nSrcLen;` |
|      - |  474 | `	sxi64 iStart,iEnd;` |
| 272545 |  475 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"substr",1,"$string"); }` |
| 272545 |  476 | `	if( nArg < 2 ){` |
|      - |  477 | `		/* Arity is enforced at the call boundary; nothing sensible to return here. */` |
|    ! 0 |  478 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 |  479 | `		return PH7_OK;` |
|      - |  480 | `	}` |
|      - |  481 | `	/* Extract the target string */` |
| 272545 |  482 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|      - |  483 | `	/* Extract the offset */` |
|      - |  484 | `	{` |
| 272545 |  485 | `		sxi64 iTmp = 0;` |
| 272545 |  486 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"substr",2,"$offset","int",&iTmp);` |
| 272545 |  487 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |  488 | `			return rcArg;` |
|      - |  489 | `		}` |
| 272545 |  490 | `		iStart = iTmp;` |
|      - |  491 | `	}` |
|      - |  492 | `	/*` |
|      - |  493 | `	 * php 8 never answers substr() with FALSE — every out-of-range window simply` |
|      - |  494 | `	 * clamps to the empty string (substr("",0), substr("abc",5) and` |
|      - |  495 | `	 * substr("abc",1,-5) are all ""). PH7 returned FALSE for each of those, which` |
|      - |  496 | `	 * then flowed on as a bool into string context.` |
|      - |  497 | `	 *` |
|      - |  498 | `	 * A negative offset counts back from the end (clamped to 0); a negative length` |
|      - |  499 | `	 * leaves that many bytes off the end. Computed in sxi64 so an INT64 offset or` |
|      - |  500 | `	 * length cannot overflow the window arithmetic.` |
|      - |  501 | `	 */` |
| 272545 |  502 | `	if( iStart < 0 ){` |
|  33915 |  503 | `		iStart += nSrcLen;` |
|  33915 |  504 | `		if( iStart < 0 ){` |
|      5 |  505 | `			iStart = 0;` |
|      7 |  506 | `		}` |
| 255590 |  507 | `	}else if( iStart > nSrcLen ){` |
|      7 |  508 | `		iStart = nSrcLen;` |
|      3 |  509 | `	}` |
| 272545 |  510 | `	iEnd = nSrcLen;` |
| 272545 |  511 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
| 200891 |  512 | `		sxi64 iLen = 0;` |
| 200891 |  513 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[2],"substr",3,"$length","?int",&iLen);` |
| 200891 |  514 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |  515 | `			return rcArg;` |
|      - |  516 | `		}` |
| 200891 |  517 | `		if( iLen < 0 ){` |
|  33547 |  518 | `			iEnd = (sxi64)nSrcLen + iLen;` |
| 184120 |  519 | `		}else if( iLen > (sxi64)nSrcLen - iStart ){` |
|  19167 |  520 | `			iEnd = nSrcLen;` |
|   9586 |  521 | `		}else{` |
| 148187 |  522 | `			iEnd = iStart + iLen;` |
|      - |  523 | `		}` |
| 100443 |  524 | `	}` |
| 272545 |  525 | `	if( iEnd < iStart ){` |
|      3 |  526 | `		iEnd = iStart;` |
|      1 |  527 | `	}` |
| 272545 |  528 | `	ph7_result_string(pCtx,&zSource[iStart],(int)(iEnd - iStart));` |
| 272545 |  529 | `	return PH7_OK;` |
| 136275 |  530 | `}` |
|      - |  531 | `/*` |
|      - |  532 | ` * int substr_compare(string $main_str,string $str ,int $offset[,int $length[,bool $case_insensitivity = false ]])` |
|      - |  533 | ` *  Binary safe comparison of two strings from an offset, up to length characters.` |
|      - |  534 | ` * Parameters` |
|      - |  535 | ` *  $main_str` |
|      - |  536 | ` *  The main string being compared.` |
|      - |  537 | ` *  $str` |
|      - |  538 | ` *   The secondary string being compared.` |
|      - |  539 | ` * $offset` |
|      - |  540 | ` *  The start position for the comparison. If negative, it starts counting from` |
|      - |  541 | ` *  the end of the string.` |
|      - |  542 | ` * $length` |
|      - |  543 | ` *  The length of the comparison. The default value is the largest of the length` |
|      - |  544 | ` *  of the str compared to the length of main_str less the offset.` |
|      - |  545 | ` * $case_insensitivity` |
|      - |  546 | ` *  If case_insensitivity is TRUE, comparison is case insensitive.` |
|      - |  547 | ` * Return` |
|      - |  548 | ` *  Returns < 0 if main_str from position offset is less than str, > 0 if it is greater than` |
|      - |  549 | ` *  str, and 0 if they are equal. If offset is equal to or greater than the length of main_str` |
|      - |  550 | ` *  or length is set and is less than 1, substr_compare() prints a warning and returns FALSE.` |
|      - |  551 | ` */` |
|     20 |  552 | `static int PH7_builtin_substr_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  553 | `{` |
|      - |  554 | `	const char *zSource,*zSub;` |
|      - |  555 | `	int nSrcLen,nSubLen;` |
|      - |  556 | `	sxi64 iOfft,iLen,l1,l2,nCmp;` |
|     21 |  557 | `	int iCase = 0;` |
|      - |  558 | `	int rc;` |
|     21 |  559 | `	if( nArg < 3 ){` |
|    ! 0 |  560 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  561 | `		return PH7_OK;` |
|      - |  562 | `	}` |
|     21 |  563 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|     21 |  564 | `	zSub    = ph7_value_to_string(apArg[1],&nSubLen);` |
|      - |  565 | `	{` |
|     21 |  566 | `		sxi64 iTmp = 0;` |
|     21 |  567 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[2],"substr_compare",3,"$offset","int",&iTmp);` |
|     21 |  568 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |  569 | `			return rcArg;` |
|      - |  570 | `		}` |
|     21 |  571 | `		iOfft = iTmp;` |
|      - |  572 | `	}` |
|     21 |  573 | `	if( iOfft < 0 ){` |
|      5 |  574 | `		iOfft += nSrcLen;` |
|      5 |  575 | `		if( iOfft < 0 ){` |
|      3 |  576 | `			iOfft = 0;` |
|      1 |  577 | `		}` |
|      2 |  578 | `	}` |
|     21 |  579 | `	if( iOfft > nSrcLen ){` |
|      - |  580 | `		/* php rejects an offset past the end of the haystack outright */` |
|    ! 0 |  581 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  582 | `			"substr_compare(): Argument #3 ($offset) must be contained in argument #1 ($haystack)");` |
|      - |  583 | `	}` |
|      - |  584 | `	/* A NULL/absent length compares as far as the longer of the two operands reaches */` |
|     21 |  585 | `	iLen = (sxi64)nSrcLen - iOfft;` |
|     21 |  586 | `	if( iLen < nSubLen ){` |
|      5 |  587 | `		iLen = nSubLen;` |
|      2 |  588 | `	}` |
|     21 |  589 | `	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){` |
|     11 |  590 | `		sxi64 iTmp = 0;` |
|     11 |  591 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[3],"substr_compare",4,"$length","?int",&iTmp);` |
|     11 |  592 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |  593 | `			return rcArg;` |
|      - |  594 | `		}` |
|     11 |  595 | `		if( iTmp < 0 ){` |
|      3 |  596 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  597 | `				"substr_compare(): Argument #4 ($length) must be greater than or equal to 0");` |
|      - |  598 | `		}` |
|      9 |  599 | `		iLen = iTmp;` |
|      4 |  600 | `	}` |
|     19 |  601 | `	if( nArg > 4 ){` |
|      5 |  602 | `		iCase = ph7_value_to_bool(apArg[4]);` |
|      2 |  603 | `	}` |
|      - |  604 | `	/* Each side contributes at most what it actually has left */` |
|     19 |  605 | `	l1 = (sxi64)nSrcLen - iOfft;` |
|     19 |  606 | `	if( l1 > iLen ){ l1 = iLen; }` |
|     19 |  607 | `	l2 = nSubLen;` |
|     19 |  608 | `	if( l2 > iLen ){ l2 = iLen; }` |
|     19 |  609 | `	nCmp = (l1 < l2) ? l1 : l2;` |
|     19 |  610 | `	if( iCase ){` |
|      3 |  611 | `		rc = SyStrnicmp(&zSource[iOfft],zSub,(sxu32)nCmp);` |
|      2 |  612 | `	}else{` |
|     17 |  613 | `		rc = SyStrncmp(&zSource[iOfft],zSub,(sxu32)nCmp);` |
|      - |  614 | `	}` |
|     19 |  615 | `	if( rc == 0 ){` |
|      - |  616 | `		/* Prefixes equal: php falls back to a THREE-WAY compare of the lengths, so this` |
|      - |  617 | `		 * arm is normalized to -1/0/1 (substr_compare("abc","",0) is 1, not 3). */` |
|      9 |  618 | `		rc = (l1 == l2) ? 0 : (l1 < l2 ? -1 : 1);` |
|      4 |  619 | `	}` |
|      - |  620 | `	/* ...but when the prefixes differ php returns the RAW byte difference, not its sign:` |
|      - |  621 | `	 * substr_compare("abc","def",1,10) is -2 ('b' - 'd'), which is what SyMemcmp gives. */` |
|     19 |  622 | `	ph7_result_int(pCtx,rc);` |
|     19 |  623 | `	return PH7_OK;` |
|     11 |  624 | `}` |
|      - |  625 | `/*` |
|      - |  626 | ` * int substr_count(string $haystack,string $needle[,int $offset = 0 [,int $length ]])` |
|      - |  627 | ` *  Count the number of substring occurrences.` |
|      - |  628 | ` * Parameters` |
|      - |  629 | ` * $haystack` |
|      - |  630 | ` *   The string to search in` |
|      - |  631 | ` * $needle` |
|      - |  632 | ` *   The substring to search for` |
|      - |  633 | ` * $offset` |
|      - |  634 | ` *  The offset where to start counting` |
|      - |  635 | ` * $length (NOT USED)` |
|      - |  636 | ` *  The maximum length after the specified offset to search for the substring.` |
|      - |  637 | ` *  It outputs a warning if the offset plus the length is greater than the haystack length.` |
|      - |  638 | ` * Return` |
|      - |  639 | ` *  Toral number of substring occurrences.` |
|      - |  640 | ` */` |
|     26 |  641 | `static int PH7_builtin_substr_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  642 | `{` |
|      - |  643 | `	const char *zText,*zPattern,*zEnd;` |
|      - |  644 | `	int nTextlen,nPatlen;` |
|     27 |  645 | `	int iCount = 0;` |
|      - |  646 | `	sxu32 nOfft;` |
|      - |  647 | `	sxi32 rc;` |
|     27 |  648 | `	if( nArg < 2 ){` |
|      - |  649 | `		/* Missing arguments */` |
|    ! 0 |  650 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  651 | `		return PH7_OK;` |
|      - |  652 | `	}` |
|      - |  653 | `	/* Point to the haystack */` |
|     27 |  654 | `	zText = ph7_value_to_string(apArg[0],&nTextlen);` |
|      - |  655 | `	/* Point to the neddle */` |
|     27 |  656 | `	zPattern = ph7_value_to_string(apArg[1],&nPatlen);` |
|     27 |  657 | `	if( nPatlen < 1 ){` |
|      - |  658 | `		/* Empty needle: PHP 8 throws a catchable ValueError. */` |
|      3 |  659 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  660 | `			"substr_count(): Argument #2 ($needle) must not be empty");` |
|      - |  661 | `	}` |
|      - |  662 | `	/* Apply the optional $offset/$length window before searching. PHP 8 validates` |
|      - |  663 | `	 * both against the haystack (a negative value counts from the end) and throws a` |
|      - |  664 | `	 * catchable ValueError when the result falls outside it — this happens before the` |
|      - |  665 | `	 * needle-fits check, so it fires even when the needle is longer than the haystack. */` |
|     25 |  666 | `	if( nArg > 2 ){` |
|     19 |  667 | `		ph7_int64 iOfft = ph7_value_to_int64(apArg[2]);` |
|     19 |  668 | `		if( iOfft < 0 ){` |
|      5 |  669 | `			iOfft += nTextlen;` |
|      2 |  670 | `		}` |
|     19 |  671 | `		if( iOfft < 0 \|\| iOfft > nTextlen ){` |
|      3 |  672 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  673 | `				"substr_count(): Argument #3 ($offset) must be contained in argument #1 ($haystack)");` |
|      - |  674 | `		}` |
|      - |  675 | `		/* Point to the desired offset and shrink the remaining region */` |
|     17 |  676 | `		zText = &zText[iOfft];` |
|     17 |  677 | `		nTextlen -= (int)iOfft;` |
|      8 |  678 | `	}` |
|     23 |  679 | `	if( nArg > 3 ){` |
|     15 |  680 | `		ph7_int64 nLen = ph7_value_to_int64(apArg[3]);` |
|     15 |  681 | `		if( nLen < 0 ){` |
|      - |  682 | `			/* Negative length is relative to the end of the (offset) haystack */` |
|      5 |  683 | `			nLen += nTextlen;` |
|      2 |  684 | `		}` |
|     15 |  685 | `		if( nLen < 0 \|\| nLen > nTextlen ){` |
|      5 |  686 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  687 | `				"substr_count(): Argument #4 ($length) must be contained in argument #1 ($haystack)");` |
|      - |  688 | `		}` |
|     11 |  689 | `		nTextlen = (int)nLen;` |
|      5 |  690 | `	}` |
|     19 |  691 | `	if( nTextlen < 1 \|\| nPatlen > nTextlen ){` |
|      - |  692 | `		/* The windowed haystack can't contain the needle: zero matches */` |
|      3 |  693 | `		ph7_result_int(pCtx,0);` |
|      3 |  694 | `		return PH7_OK;` |
|      - |  695 | `	}` |
|      - |  696 | `	/* Point to the end of the windowed haystack */` |
|     17 |  697 | `	zEnd = &zText[nTextlen];` |
|      - |  698 | `	/* Perform the search */` |
|     17 |  699 | `	for(;;){` |
|     35 |  700 | `		rc = SyBlobSearch((const void *)zText,(sxu32)(zEnd-zText),(const void *)zPattern,nPatlen,&nOfft);` |
|     35 |  701 | `		if( rc != SXRET_OK ){` |
|      - |  702 | `			/* Pattern not found,break immediately */` |
|     13 |  703 | `			break;` |
|      - |  704 | `		}` |
|      - |  705 | `		/* Increment counter and update the offset */` |
|     23 |  706 | `		iCount++;` |
|     23 |  707 | `		zText += nOfft + nPatlen;` |
|     23 |  708 | `		if( zText >= zEnd ){` |
|      5 |  709 | `			break;` |
|      - |  710 | `		}` |
|      1 |  711 | `	}` |
|      - |  712 | `	/* Pattern count */` |
|     17 |  713 | `	ph7_result_int(pCtx,iCount);` |
|     17 |  714 | `	return PH7_OK;` |
|     14 |  715 | `}` |
|      - |  716 | `/* Forward declarations: defined with the trim/addcslashes and str_contains` |
|      - |  717 | ` * families below. */` |
|      - |  718 | `static void PH7_BuildCharMask(ph7_context *pCtx,const char *zList,int nLen,char aMask[256]);` |
|      - |  719 | `/*` |
|      - |  720 | ` * php 8.1 null-to-non-nullable ZPP deprecation, notice-only form for the` |
|      - |  721 | ` * legacy string builtins that still coerce null to "" themselves: emit` |
|      - |  722 | ``  * `f(): Passing null to parameter #N ($name) of type string is deprecated` `` |
|      - |  723 | ` * when the arg is an actual null, leaving the resolution unchanged.` |
|      - |  724 | ` */` |
| 400594 |  725 | `static void StrNullArgNotice(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNum,const char *zParamName)` |
|      5 |  726 | `{` |
| 400599 |  727 | `	if( ph7_value_is_null(pArg) ){` |
|     22 |  728 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|      - |  729 | `			"%s(): Passing null to parameter #%d (%s) of type string is deprecated",` |
|      7 |  730 | `			zFunc,iArgNum,zParamName);` |
|      7 |  731 | `	}` |
| 400599 |  732 | `}` |
|      - |  733 | `static sxi32 StrPredicateResolveArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,` |
|      - |  734 | `	int iArgNum,const char *zParamName,const char *zTypeStr,const char *zNullMsg,` |
|      - |  735 | `	ph7_value *pTmp,const char **pzOut,int *pnOut);` |
|      - |  736 | `/*` |
|      - |  737 | ` * Validate and resolve an int-typed builtin parameter with php-8 ZPP weak-mode` |
|      - |  738 | ` * semantics: ints and bools pass through; null emits the 8.1 deprecation and` |
|      - |  739 | ` * resolves to 0; floats and float-strings convert, with the implicit-conversion` |
|      - |  740 | ` * E_DEPRECATED when lossy and a TypeError when NAN/INF/out of int range;` |
|      - |  741 | ` * integral numeric strings convert exactly; everything else (arrays, resources,` |
|      - |  742 | ` * objects, non-numeric strings) is a TypeError naming zTypeStr (e.g. "int",` |
|      - |  743 | ` * "array\|int"). Returns PH7_OK with *pOut set, or the throw status.` |
|      - |  744 | ` */` |
|      - |  745 | `/*` |
|      - |  746 | ` * Normalize a substr_replace() offset/length pair against a string of nStrLen` |
|      - |  747 | ` * bytes, exactly like PHP: a negative offset counts from the end (clamped to 0),` |
|      - |  748 | ` * an offset past the end clamps to the end; a negative length leaves that many` |
|      - |  749 | ` * bytes off the end of the remaining region (clamped to 0), and the length is` |
|      - |  750 | ` * finally clamped to the remaining region. Written without f+l additions so an` |
|      - |  751 | ` * INT64_MAX length cannot overflow.` |
|      - |  752 | ` */` |
|     60 |  753 | `static void SubstrReplaceWindow(sxi64 *pF,sxi64 *pL,int nStrLen)` |
|      1 |  754 | `{` |
|     61 |  755 | `	sxi64 f = *pF,l = *pL;` |
|     61 |  756 | `	if( f < 0 ){` |
|      9 |  757 | `		f += nStrLen;` |
|      9 |  758 | `		if( f < 0 ){` |
|      5 |  759 | `			f = 0;` |
|      3 |  760 | `		}` |
|     57 |  761 | `	}else if( f > nStrLen ){` |
|      5 |  762 | `		f = nStrLen;` |
|      2 |  763 | `	}` |
|     61 |  764 | `	if( l < 0 ){` |
|      7 |  765 | `		l += nStrLen - f;` |
|      7 |  766 | `		if( l < 0 ){` |
|      5 |  767 | `			l = 0;` |
|      2 |  768 | `		}` |
|      3 |  769 | `	}` |
|     61 |  770 | `	if( l > nStrLen - f ){` |
|     25 |  771 | `		l = nStrLen - f;` |
|     12 |  772 | `	}` |
|     61 |  773 | `	*pF = f;` |
|     61 |  774 | `	*pL = l;` |
|     61 |  775 | `}` |
|      - |  776 | `/* A replacement string collected out of substr_replace()'s $replace array.` |
|      - |  777 | ` * The bytes live in a shared pool blob (walker values are transient), so the` |
|      - |  778 | ` * item stores pool offsets, mirroring the strtr_entry technique. */` |
|      - |  779 | `typedef struct substr_repl_item substr_repl_item;` |
|      - |  780 | `struct substr_repl_item` |
|      - |  781 | `{` |
|      - |  782 | `	sxu32 nOfft; /* Offset of the string inside the pool */` |
|      - |  783 | `	sxu32 nLen;  /* Length of the string */` |
|      - |  784 | `};` |
|      - |  785 | `typedef struct substr_replace_collect substr_replace_collect;` |
|      - |  786 | `struct substr_replace_collect` |
|      - |  787 | `{` |
|      - |  788 | `	SyBlob *pPool;  /* Byte pool for string items (string walker only) */` |
|      - |  789 | `	SySet *pSet;    /* substr_repl_item set (string) or sxi64 set (int) */` |
|      - |  790 | `	sxi32 rc;       /* SXRET_OK or SXERR_MEM on collector failure */` |
|      - |  791 | `};` |
|      - |  792 | `/* ph7_array_walk() callback: append one $replace element to the pool. */` |
|      6 |  793 | `static int SubstrReplaceStrWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 |  794 | `{` |
|      7 |  795 | `	substr_replace_collect *pCol = (substr_replace_collect *)pUserData;` |
|      - |  796 | `	substr_repl_item sItem;` |
|      - |  797 | `	const char *zStr;` |
|      - |  798 | `	int nLen;` |
|      3 |  799 | `	SXUNUSED(pKey);` |
|      7 |  800 | `	zStr = ph7_value_to_string(pData,&nLen);` |
|      7 |  801 | `	sItem.nOfft = SyBlobLength(pCol->pPool);` |
|      7 |  802 | `	sItem.nLen = (sxu32)nLen;` |
|      7 |  803 | `	if( nLen > 0 && SXRET_OK != SyBlobAppend(pCol->pPool,(const void *)zStr,(sxu32)nLen) ){` |
|    ! 0 |  804 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 |  805 | `		return SXERR_ABORT;` |
|      - |  806 | `	}` |
|      7 |  807 | `	if( SXRET_OK != SySetPut(pCol->pSet,(const void *)&sItem) ){` |
|    ! 0 |  808 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 |  809 | `		return SXERR_ABORT;` |
|      - |  810 | `	}` |
|      7 |  811 | `	return PH7_OK;` |
|      4 |  812 | `}` |
|      - |  813 | `/* ph7_array_walk() callback: collect one $offset/$length element as an int. */` |
|     12 |  814 | `static int SubstrReplaceIntWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 |  815 | `{` |
|     13 |  816 | `	substr_replace_collect *pCol = (substr_replace_collect *)pUserData;` |
|     13 |  817 | `	sxi64 iVal = ph7_value_to_int64(pData);` |
|      6 |  818 | `	SXUNUSED(pKey);` |
|     13 |  819 | `	if( SXRET_OK != SySetPut(pCol->pSet,(const void *)&iVal) ){` |
|    ! 0 |  820 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 |  821 | `		return SXERR_ABORT;` |
|      - |  822 | `	}` |
|     13 |  823 | `	return PH7_OK;` |
|      7 |  824 | `}` |
|      - |  825 | `/* Per-element state while walking substr_replace()'s array $string. */` |
|      - |  826 | `typedef struct substr_replace_ctx substr_replace_ctx;` |
|      - |  827 | `struct substr_replace_ctx` |
|      - |  828 | `{` |
|      - |  829 | `	ph7_value *pResult;   /* Result array (keys preserved) */` |
|      - |  830 | `	ph7_value *pScratch;  /* Reusable string value for each element */` |
|      - |  831 | `	SyBlob *pReplPool;    /* Pool behind aRepl items */` |
|      - |  832 | `	SySet *pRepl;         /* substr_repl_item set or NULL when $replace is scalar */` |
|      - |  833 | `	SySet *pFrom;         /* sxi64 set or NULL when $offset is scalar */` |
|      - |  834 | `	SySet *pLen;          /* sxi64 set or NULL when $length is scalar/absent */` |
|      - |  835 | `	sxu32 iReplCur;       /* Next-position cursors into the three sets */` |
|      - |  836 | `	sxu32 iFromCur;` |
|      - |  837 | `	sxu32 iLenCur;` |
|      - |  838 | `	const char *zRepl;    /* Scalar $replace */` |
|      - |  839 | `	int nRepl;` |
|      - |  840 | `	sxi64 iFrom;          /* Scalar $offset */` |
|      - |  841 | `	sxi64 iLen;           /* Scalar $length */` |
|      - |  842 | `	int bLenGiven;        /* FALSE: $length absent/null -> element length */` |
|      - |  843 | `	sxi32 rc;             /* SXRET_OK or SXERR_MEM */` |
|      - |  844 | `};` |
|      - |  845 | `/*` |
|      - |  846 | ` * ph7_array_walk() callback over the array $string: replace the window of one` |
|      - |  847 | ` * element and insert the result under the element's original key. Array-form` |
|      - |  848 | ` * $replace/$offset/$length are consumed positionally; when a set runs out PHP` |
|      - |  849 | ` * falls back to ""/0/element-length respectively.` |
|      - |  850 | ` */` |
|     24 |  851 | `static int SubstrReplaceElemWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 |  852 | `{` |
|     25 |  853 | `	substr_replace_ctx *pRep = (substr_replace_ctx *)pUserData;` |
|      - |  854 | `	const char *zStr,*zRepl;` |
|      - |  855 | `	sxi64 f,l;` |
|      - |  856 | `	int nLen,nRepl;` |
|     25 |  857 | `	zStr = ph7_value_to_string(pData,&nLen);` |
|      - |  858 | `	/* Positional $replace element ("" when exhausted) */` |
|     25 |  859 | `	if( pRep->pRepl ){` |
|     11 |  860 | `		if( pRep->iReplCur < SySetUsed(pRep->pRepl) ){` |
|      7 |  861 | `			substr_repl_item *pItem = (substr_repl_item *)SySetAt(pRep->pRepl,pRep->iReplCur++);` |
|      7 |  862 | `			zRepl = (const char *)SyBlobDataAt(pRep->pReplPool,pItem->nOfft);` |
|      7 |  863 | `			nRepl = (int)pItem->nLen;` |
|      4 |  864 | `		}else{` |
|      5 |  865 | `			zRepl = "";` |
|      5 |  866 | `			nRepl = 0;` |
|      - |  867 | `		}` |
|      6 |  868 | `	}else{` |
|     15 |  869 | `		zRepl = pRep->zRepl;` |
|     15 |  870 | `		nRepl = pRep->nRepl;` |
|      - |  871 | `	}` |
|      - |  872 | `	/* Positional $offset element (0 when exhausted) */` |
|     25 |  873 | `	if( pRep->pFrom ){` |
|     13 |  874 | `		sxi64 *pVal = 0;` |
|     13 |  875 | `		if( pRep->iFromCur < SySetUsed(pRep->pFrom) ){` |
|      9 |  876 | `			pVal = (sxi64 *)SySetAt(pRep->pFrom,pRep->iFromCur++);` |
|      4 |  877 | `		}` |
|     13 |  878 | `		f = pVal ? *pVal : 0;` |
|      7 |  879 | `	}else{` |
|     13 |  880 | `		f = pRep->iFrom;` |
|      - |  881 | `	}` |
|      - |  882 | `	/* Positional $length element (element length when exhausted) */` |
|     25 |  883 | `	if( pRep->pLen ){` |
|      7 |  884 | `		sxi64 *pVal = 0;` |
|      7 |  885 | `		if( pRep->iLenCur < SySetUsed(pRep->pLen) ){` |
|      5 |  886 | `			pVal = (sxi64 *)SySetAt(pRep->pLen,pRep->iLenCur++);` |
|      2 |  887 | `		}` |
|      7 |  888 | `		l = pVal ? *pVal : nLen;` |
|      4 |  889 | `	}else{` |
|     19 |  890 | `		l = pRep->bLenGiven ? pRep->iLen : nLen;` |
|      - |  891 | `	}` |
|     25 |  892 | `	SubstrReplaceWindow(&f,&l,nLen);` |
|      - |  893 | `	/* Assemble prefix + replacement + suffix in the scratch value */` |
|     25 |  894 | `	ph7_value_reset_string_cursor(pRep->pScratch);` |
|     24 |  895 | `	if( (f > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,zStr,(int)f))` |
|     24 |  896 | `	 \|\| (nRepl > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,zRepl,nRepl))` |
|     40 |  897 | `	 \|\| (nLen - (int)(f+l) > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,&zStr[f+l],nLen - (int)(f+l))) ){` |
|     30 |  898 | `		pRep->rc = SXERR_MEM;` |
|     30 |  899 | `		return SXERR_ABORT;` |
|      - |  900 | `	}` |
|     25 |  901 | `	if( SXRET_OK != ph7_array_add_elem(pRep->pResult,pKey,pRep->pScratch) ){` |
|    ! 0 |  902 | `		pRep->rc = SXERR_MEM;` |
|    ! 0 |  903 | `		return SXERR_ABORT;` |
|      - |  904 | `	}` |
|     25 |  905 | `	return PH7_OK;` |
|     43 |  906 | `}` |
|      - |  907 | `/*` |
|      - |  908 | ` * mixed substr_replace(array\|string $string,array\|string $replace,array\|int $offset[,array\|int\|null $length = null])` |
|      - |  909 | ` *  Replace text within a portion of a string.` |
|      - |  910 | ` * Parameters` |
|      - |  911 | ` *  $string` |
|      - |  912 | ` *   The input string or an array of strings (each element is processed with` |
|      - |  913 | ` *   its own positional replace/offset/length when those are arrays too).` |
|      - |  914 | ` *  $replace` |
|      - |  915 | ` *   The replacement string. When $string is scalar and $replace is an array,` |
|      - |  916 | ` *   only its first element is used (PHP quirk).` |
|      - |  917 | ` *  $offset` |
|      - |  918 | ` *   Window start; negative counts from the end of the string.` |
|      - |  919 | ` *  $length` |
|      - |  920 | ` *   Window length; negative leaves that many bytes at the end; null/absent` |
|      - |  921 | ` *   means "to the end of the string".` |
|      - |  922 | ` * Return` |
|      - |  923 | ` *  The processed string, or an array of processed strings (keys preserved).` |
|      - |  924 | ` * Errors` |
|      - |  925 | ` *  ArgumentCountError on fewer than 3 arguments; TypeError when an array` |
|      - |  926 | ` *  $offset/$length is combined with a scalar $string.` |
|      - |  927 | ` */` |
|     58 |  928 | `static int PH7_builtin_substr_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  929 | `{` |
|      - |  930 | `	ph7_value sStrTmp,sReplTmp;` |
|     59 |  931 | `	const char *zStr = 0,*zRepl = 0;` |
|     59 |  932 | `	int nLen = 0,nRepl = 0;` |
|      - |  933 | `	int bLenGiven;` |
|     59 |  934 | `	sxi64 f = 0,l = 0;` |
|      - |  935 | `	sxi32 rc;` |
|     59 |  936 | `	if( nArg < 3 ){` |
|    ! 0 |  937 | `		return PH7_VmThrowException(pCtx,` |
|      - |  938 | `			"ArgumentCountError",` |
|      - |  939 | `			"substr_replace() expects at least 3 arguments, %d given",` |
|    ! 0 |  940 | `			nArg` |
|      - |  941 | `			);` |
|      - |  942 | `	}` |
|      - |  943 | `	/* $length counts as given unless absent or null (php: ?null semantics) */` |
|     59 |  944 | `	bLenGiven = (nArg > 3 && !ph7_value_is_null(apArg[3]));` |
|      - |  945 | `	/* php ZPP validates all four args, in order, before the body runs: the` |
|      - |  946 | `	 * non-array forms resolve here (null deprecation, __toString objects,` |
|      - |  947 | `	 * numeric strings), arrays pass through to the per-mode handling. */` |
|     59 |  948 | `	PH7_MemObjInit(pCtx->pVm,&sStrTmp);` |
|     59 |  949 | `	PH7_MemObjInit(pCtx->pVm,&sReplTmp);` |
|     59 |  950 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     45 |  951 | `		rc = StrPredicateResolveArg(pCtx,apArg[0],"substr_replace",1,"$string","array\|string",` |
|      - |  952 | `			"substr_replace(): Passing null to parameter #1 ($string) "` |
|      - |  953 | `			"of type array\|string is deprecated",` |
|      - |  954 | `			&sStrTmp,&zStr,&nLen);` |
|     45 |  955 | `		if( rc != PH7_OK ) goto out;` |
|     22 |  956 | `	}` |
|     59 |  957 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|     51 |  958 | `		rc = StrPredicateResolveArg(pCtx,apArg[1],"substr_replace",2,"$replace","array\|string",` |
|      - |  959 | `			"substr_replace(): Passing null to parameter #2 ($replace) "` |
|      - |  960 | `			"of type array\|string is deprecated",` |
|      - |  961 | `			&sReplTmp,&zRepl,&nRepl);` |
|     51 |  962 | `		if( rc != PH7_OK ) goto out;` |
|     25 |  963 | `	}` |
|     59 |  964 | `	if( !ph7_value_is_array(apArg[2]) ){` |
|     51 |  965 | `		rc = PH7_IntArgResolve(pCtx,apArg[2],"substr_replace",3,"$offset","array\|int",&f);` |
|     51 |  966 | `		if( rc != PH7_OK ) goto out;` |
|     24 |  967 | `	}` |
|     57 |  968 | `	if( bLenGiven && !ph7_value_is_array(apArg[3]) ){` |
|     31 |  969 | `		rc = PH7_IntArgResolve(pCtx,apArg[3],"substr_replace",4,"$length","array\|int\|null",&l);` |
|     31 |  970 | `		if( rc != PH7_OK ) goto out;` |
|     14 |  971 | `	}` |
|     55 |  972 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - |  973 | `		/* Array form: process each element, preserving keys */` |
|      - |  974 | `		substr_replace_ctx sRep;` |
|      - |  975 | `		substr_replace_collect sCol;` |
|      - |  976 | `		SyBlob sReplPool;` |
|      - |  977 | `		SySet sRepl,sFrom,sLen;` |
|      - |  978 | `		ph7_value *pResult,*pScratch;` |
|     15 |  979 | `		sxi32 rcWalk = SXRET_OK;` |
|     15 |  980 | `		SyBlobInit(&sReplPool,&pCtx->pVm->sAllocator);` |
|     15 |  981 | `		SySetInit(&sRepl,&pCtx->pVm->sAllocator,sizeof(substr_repl_item));` |
|     15 |  982 | `		SySetInit(&sFrom,&pCtx->pVm->sAllocator,sizeof(sxi64));` |
|     15 |  983 | `		SySetInit(&sLen,&pCtx->pVm->sAllocator,sizeof(sxi64));` |
|     15 |  984 | `		SyZero(&sRep,sizeof(substr_replace_ctx));` |
|     15 |  985 | `		sRep.bLenGiven = bLenGiven;` |
|     15 |  986 | `		sCol.rc = SXRET_OK;` |
|      - |  987 | `		/* Collect array-form $replace/$offset/$length positionally; the` |
|      - |  988 | `		 * scalar forms were already resolved above. */` |
|     15 |  989 | `		if( ph7_value_is_array(apArg[1]) ){` |
|      5 |  990 | `			sCol.pPool = &sReplPool;` |
|      5 |  991 | `			sCol.pSet = &sRepl;` |
|      5 |  992 | `			ph7_array_walk(apArg[1],SubstrReplaceStrWalker,&sCol);` |
|      5 |  993 | `			sRep.pRepl = &sRepl;` |
|      5 |  994 | `			sRep.pReplPool = &sReplPool;` |
|      3 |  995 | `		}else{` |
|     11 |  996 | `			sRep.zRepl = zRepl;` |
|     11 |  997 | `			sRep.nRepl = nRepl;` |
|      - |  998 | `		}` |
|     15 |  999 | `		if( sCol.rc == SXRET_OK && ph7_value_is_array(apArg[2]) ){` |
|      7 | 1000 | `			sCol.pSet = &sFrom;` |
|      7 | 1001 | `			ph7_array_walk(apArg[2],SubstrReplaceIntWalker,&sCol);` |
|      7 | 1002 | `			sRep.pFrom = &sFrom;` |
|      4 | 1003 | `		}else{` |
|      9 | 1004 | `			sRep.iFrom = f;` |
|      - | 1005 | `		}` |
|     15 | 1006 | `		if( sCol.rc == SXRET_OK && bLenGiven ){` |
|      9 | 1007 | `			if( ph7_value_is_array(apArg[3]) ){` |
|      5 | 1008 | `				sCol.pSet = &sLen;` |
|      5 | 1009 | `				ph7_array_walk(apArg[3],SubstrReplaceIntWalker,&sCol);` |
|      5 | 1010 | `				sRep.pLen = &sLen;` |
|      3 | 1011 | `			}else{` |
|      5 | 1012 | `				sRep.iLen = l;` |
|      - | 1013 | `			}` |
|      4 | 1014 | `		}` |
|     15 | 1015 | `		pResult = ph7_context_new_array(pCtx);` |
|     15 | 1016 | `		pScratch = ph7_context_new_scalar(pCtx);` |
|     15 | 1017 | `		if( sCol.rc != SXRET_OK \|\| pResult == 0 \|\| pScratch == 0 ){` |
|    ! 0 | 1018 | `			rcWalk = SXERR_MEM;` |
|    ! 0 | 1019 | `		}else{` |
|     15 | 1020 | `			sRep.pResult = pResult;` |
|     15 | 1021 | `			sRep.pScratch = pScratch;` |
|     15 | 1022 | `			ph7_value_string(pScratch,"",0); /* Force string representation */` |
|     15 | 1023 | `			ph7_array_walk(apArg[0],SubstrReplaceElemWalker,&sRep);` |
|     15 | 1024 | `			rcWalk = sRep.rc;` |
|      - | 1025 | `		}` |
|     15 | 1026 | `		SyBlobRelease(&sReplPool);` |
|     15 | 1027 | `		SySetRelease(&sRepl);` |
|     15 | 1028 | `		SySetRelease(&sFrom);` |
|     15 | 1029 | `		SySetRelease(&sLen);` |
|     15 | 1030 | `		if( rcWalk != SXRET_OK ){` |
|    ! 0 | 1031 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1032 | `			goto out;` |
|      - | 1033 | `		}` |
|     15 | 1034 | `		ph7_result_value(pCtx,pResult);` |
|     15 | 1035 | `		rc = PH7_OK;` |
|     15 | 1036 | `		goto out;` |
|      - | 1037 | `	}` |
|      - | 1038 | `	/* Scalar form: array $offset/$length are a TypeError, array $replace` |
|      - | 1039 | `	 * degrades to its first element (php quirk). */` |
|     41 | 1040 | `	if( ph7_value_is_array(apArg[2]) ){` |
|      3 | 1041 | `		rc = PH7_VmThrowException(pCtx,` |
|      - | 1042 | `			"TypeError",` |
|      - | 1043 | `			"substr_replace(): Argument #3 ($offset) cannot be an array when working on a single string"` |
|      - | 1044 | `			);` |
|      3 | 1045 | `		goto out;` |
|      - | 1046 | `	}` |
|     39 | 1047 | `	if( bLenGiven && ph7_value_is_array(apArg[3]) ){` |
|      3 | 1048 | `		rc = PH7_VmThrowException(pCtx,` |
|      - | 1049 | `			"TypeError",` |
|      - | 1050 | `			"substr_replace(): Argument #4 ($length) cannot be an array when working on a single string"` |
|      - | 1051 | `			);` |
|      3 | 1052 | `		goto out;` |
|      - | 1053 | `	}` |
|     37 | 1054 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 1055 | `		/* First element of the replace array, or "" when empty */` |
|      5 | 1056 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      5 | 1057 | `		zRepl = "";` |
|      5 | 1058 | `		nRepl = 0;` |
|      5 | 1059 | `		if( pMap->pFirst ){` |
|      3 | 1060 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pMap->pFirst->nValIdx);` |
|      3 | 1061 | `			if( pVal ){` |
|      3 | 1062 | `				zRepl = ph7_value_to_string(pVal,&nRepl);` |
|      1 | 1063 | `			}` |
|      1 | 1064 | `		}` |
|      2 | 1065 | `	}` |
|     37 | 1066 | `	if( !bLenGiven ){` |
|     15 | 1067 | `		l = nLen;` |
|      7 | 1068 | `	}` |
|     37 | 1069 | `	SubstrReplaceWindow(&f,&l,nLen);` |
|      - | 1070 | `	/* Assemble prefix + replacement + suffix straight into the call result` |
|      - | 1071 | `	 * (ph7_result_string appends), no scratch buffer needed. */` |
|     37 | 1072 | `	rc = SXRET_OK;` |
|     37 | 1073 | `	if( f > 0 ){` |
|     29 | 1074 | `		rc = ph7_result_string(pCtx,zStr,(int)f);` |
|     14 | 1075 | `	}` |
|     37 | 1076 | `	if( rc == SXRET_OK && nRepl > 0 ){` |
|     33 | 1077 | `		rc = ph7_result_string(pCtx,zRepl,nRepl);` |
|     16 | 1078 | `	}` |
|     37 | 1079 | `	if( rc == SXRET_OK && nLen - (int)(f+l) > 0 ){` |
|     17 | 1080 | `		rc = ph7_result_string(pCtx,&zStr[f+l],nLen - (int)(f+l));` |
|      8 | 1081 | `	}` |
|     37 | 1082 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 1083 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1084 | `		goto out;` |
|      - | 1085 | `	}` |
|      - | 1086 | `	/* Force a string result even when all three segments are empty */` |
|     37 | 1087 | `	rc = ph7_result_string(pCtx,"",0);` |
|     37 | 1088 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 1089 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1090 | `		goto out;` |
|      - | 1091 | `	}` |
|     37 | 1092 | `	rc = PH7_OK;` |
|     29 | 1093 | `out:` |
|     59 | 1094 | `	PH7_MemObjRelease(&sStrTmp);` |
|     59 | 1095 | `	PH7_MemObjRelease(&sReplTmp);` |
|     59 | 1096 | `	return rc;` |
|     30 | 1097 | `}` |
|      - | 1098 | `/*` |
|      - | 1099 | ` * int levenshtein(string $string1,string $string2[,int $insertion_cost = 1[,int $replacement_cost = 1[,int $deletion_cost = 1]]])` |
|      - | 1100 | ` *  Calculate the Levenshtein distance between two strings, byte per byte` |
|      - | 1101 | ` *  (case-sensitive), with optional per-operation costs. Mirrors PHP's` |
|      - | 1102 | ` *  reference_levdist(): two rolling rows over string2.` |
|      - | 1103 | ` * Return` |
|      - | 1104 | ` *  The minimal number of weighted edit operations turning $string1 into` |
|      - | 1105 | ` *  $string2.` |
|      - | 1106 | ` */` |
|     34 | 1107 | `static int PH7_builtin_levenshtein(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1108 | `{` |
|      - | 1109 | `	static const char *azParam[] = { "$insertion_cost","$replacement_cost","$deletion_cost" };` |
|      - | 1110 | `	const char *zStr1,*zStr2;` |
|     35 | 1111 | `	sxi64 iCostIns = 1,iCostRep = 1,iCostDel = 1;` |
|      - | 1112 | `	sxi64 *p1,*p2,*pTmp;` |
|      - | 1113 | `	sxi64 c0,c1,c2;` |
|      - | 1114 | `	ph7_value sTmp1,sTmp2;` |
|      - | 1115 | `	int nLen1,nLen2;` |
|      - | 1116 | `	int i1,i2;` |
|      - | 1117 | `	sxi32 rc;` |
|      - | 1118 | `	int i;` |
|     35 | 1119 | `	if( nArg < 2 ){` |
|    ! 0 | 1120 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1121 | `			"ArgumentCountError",` |
|      - | 1122 | `			"levenshtein() expects at least 2 arguments, %d given",` |
|    ! 0 | 1123 | `			nArg` |
|      - | 1124 | `			);` |
|      - | 1125 | `	}` |
|      - | 1126 | `	/* $string1/$string2: null deprecates to "", __toString objects resolve,` |
|      - | 1127 | `	 * everything non-stringish is a TypeError (php ZPP weak mode). */` |
|     35 | 1128 | `	PH7_MemObjInit(pCtx->pVm,&sTmp1);` |
|     35 | 1129 | `	PH7_MemObjInit(pCtx->pVm,&sTmp2);` |
|     35 | 1130 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"levenshtein",1,"$string1","string",` |
|      - | 1131 | `		"levenshtein(): Passing null to parameter #1 ($string1) "` |
|      - | 1132 | `		"of type string is deprecated",` |
|      - | 1133 | `		&sTmp1,&zStr1,&nLen1);` |
|     35 | 1134 | `	if( rc != PH7_OK ) goto out;` |
|     35 | 1135 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"levenshtein",2,"$string2","string",` |
|      - | 1136 | `		"levenshtein(): Passing null to parameter #2 ($string2) "` |
|      - | 1137 | `		"of type string is deprecated",` |
|      - | 1138 | `		&sTmp2,&zStr2,&nLen2);` |
|     35 | 1139 | `	if( rc != PH7_OK ) goto out;` |
|      - | 1140 | `	/* Optional integer costs */` |
|     57 | 1141 | `	for( i = 2 ; i < nArg && i < 5 ; i++ ){` |
|      - | 1142 | `		sxi64 iVal;` |
|     31 | 1143 | `		rc = PH7_IntArgResolve(pCtx,apArg[i],"levenshtein",i+1,azParam[i-2],"int",&iVal);` |
|     31 | 1144 | `		if( rc != PH7_OK ) goto out;` |
|     23 | 1145 | `		if( i == 2 ){` |
|     11 | 1146 | `			iCostIns = iVal;` |
|     18 | 1147 | `		}else if( i == 3 ){` |
|      7 | 1148 | `			iCostRep = iVal;` |
|      4 | 1149 | `		}else{` |
|      7 | 1150 | `			iCostDel = iVal;` |
|      - | 1151 | `		}` |
|     12 | 1152 | `	}` |
|     27 | 1153 | `	if( nLen1 == 0 ){` |
|      3 | 1154 | `		ph7_result_int64(pCtx,(sxi64)nLen2 * iCostIns);` |
|      3 | 1155 | `		rc = PH7_OK;` |
|      3 | 1156 | `		goto out;` |
|      - | 1157 | `	}` |
|     25 | 1158 | `	if( nLen2 == 0 ){` |
|      3 | 1159 | `		ph7_result_int64(pCtx,(sxi64)nLen1 * iCostDel);` |
|      3 | 1160 | `		rc = PH7_OK;` |
|      3 | 1161 | `		goto out;` |
|      - | 1162 | `	}` |
|      - | 1163 | `	/* Two rolling DP rows over string2 (auto-released on return). Reject a` |
|      - | 1164 | `	 * string2 long enough to overflow the 32-bit allocation size. */` |
|     23 | 1165 | `	if( (sxu32)nLen2 >= (SXU32_HIGH / sizeof(sxi64)) - 1 ){` |
|    ! 0 | 1166 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1167 | `		goto out;` |
|      - | 1168 | `	}` |
|     23 | 1169 | `	p1 = (sxi64 *)ph7_context_alloc_chunk(pCtx,(unsigned int)(sizeof(sxi64) * (sxu32)(nLen2 + 1)),FALSE,TRUE);` |
|     23 | 1170 | `	p2 = (sxi64 *)ph7_context_alloc_chunk(pCtx,(unsigned int)(sizeof(sxi64) * (sxu32)(nLen2 + 1)),FALSE,TRUE);` |
|     23 | 1171 | `	if( p1 == 0 \|\| p2 == 0 ){` |
|    ! 0 | 1172 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1173 | `		goto out;` |
|      - | 1174 | `	}` |
|    733 | 1175 | `	for( i2 = 0 ; i2 <= nLen2 ; i2++ ){` |
|    711 | 1176 | `		p1[i2] = (sxi64)i2 * iCostIns;` |
|    356 | 1177 | `	}` |
|    707 | 1178 | `	for( i1 = 0 ; i1 < nLen1 ; i1++ ){` |
|    685 | 1179 | `		p2[0] = p1[0] + iCostDel;` |
| 181111 | 1180 | `		for( i2 = 0 ; i2 < nLen2 ; i2++ ){` |
| 180427 | 1181 | `			c0 = p1[i2] + ((zStr1[i1] == zStr2[i2]) ? 0 : iCostRep);` |
| 180427 | 1182 | `			c1 = p1[i2 + 1] + iCostDel;` |
| 180427 | 1183 | `			if( c1 < c0 ){` |
|  45393 | 1184 | `				c0 = c1;` |
|  22696 | 1185 | `			}` |
| 180427 | 1186 | `			c2 = p2[i2] + iCostIns;` |
| 180427 | 1187 | `			if( c2 < c0 ){` |
|  44809 | 1188 | `				c0 = c2;` |
|  22404 | 1189 | `			}` |
| 180427 | 1190 | `			p2[i2 + 1] = c0;` |
|  90214 | 1191 | `		}` |
|    685 | 1192 | `		pTmp = p1;` |
|    685 | 1193 | `		p1 = p2;` |
|    685 | 1194 | `		p2 = pTmp;` |
|    343 | 1195 | `	}` |
|     23 | 1196 | `	ph7_result_int64(pCtx,p1[nLen2]);` |
|     23 | 1197 | `	rc = PH7_OK;` |
|     17 | 1198 | `out:` |
|     35 | 1199 | `	PH7_MemObjRelease(&sTmp1);` |
|     35 | 1200 | `	PH7_MemObjRelease(&sTmp2);` |
|     35 | 1201 | `	return rc;` |
|     18 | 1202 | `}` |
|      - | 1203 | `/*` |
|      - | 1204 | ` * Longest common substring scan behind similar_text() — a faithful port of` |
|      - | 1205 | ` * PHP's php_similar_str(): O(n*m) scan recording the first longest run.` |
|      - | 1206 | ` */` |
|     26 | 1207 | `static void SimilarStr(const char *zTxt1,int nLen1,const char *zTxt2,int nLen2,` |
|      - | 1208 | `	int *pPos1,int *pPos2,int *pMax,int *pCount)` |
|      1 | 1209 | `{` |
|      - | 1210 | `	const char *p,*q;` |
|     27 | 1211 | `	const char *zEnd1 = &zTxt1[nLen1];` |
|     27 | 1212 | `	const char *zEnd2 = &zTxt2[nLen2];` |
|      - | 1213 | `	int l;` |
|     27 | 1214 | `	*pMax = 0;` |
|     27 | 1215 | `	*pCount = 0;` |
|    143 | 1216 | `	for( p = zTxt1 ; p < zEnd1 ; p++ ){` |
|    843 | 1217 | `		for( q = zTxt2 ; q < zEnd2 ; q++ ){` |
|    999 | 1218 | `			for( l = 0 ; (p+l < zEnd1) && (q+l < zEnd2) && (p[l] == q[l]) ; l++ );` |
|    727 | 1219 | `			if( l > *pMax ){` |
|     25 | 1220 | `				*pMax = l;` |
|     25 | 1221 | `				*pCount += 1;` |
|     25 | 1222 | `				*pPos1 = (int)(p - zTxt1);` |
|     25 | 1223 | `				*pPos2 = (int)(q - zTxt2);` |
|     12 | 1224 | `			}` |
|    364 | 1225 | `		}` |
|     59 | 1226 | `	}` |
|     27 | 1227 | `}` |
|      - | 1228 | `/*` |
|      - | 1229 | ` * Recursive divide-and-conquer behind similar_text() — a faithful port of` |
|      - | 1230 | `` * PHP's php_similar_char(), including its quirky `count > 1` guard on the`` |
|      - | 1231 | ` * left-side recursion.` |
|      - | 1232 | ` */` |
|     26 | 1233 | `static int SimilarChar(const char *zTxt1,int nLen1,const char *zTxt2,int nLen2)` |
|      1 | 1234 | `{` |
|      - | 1235 | `	int nSum;` |
|     27 | 1236 | `	int nPos1 = 0,nPos2 = 0,nMax,nCount;` |
|     27 | 1237 | `	SimilarStr(zTxt1,nLen1,zTxt2,nLen2,&nPos1,&nPos2,&nMax,&nCount);` |
|     27 | 1238 | `	if( (nSum = nMax) != 0 ){` |
|     25 | 1239 | `		if( nPos1 && nPos2 && nCount > 1 ){` |
|    ! 0 | 1240 | `			nSum += SimilarChar(zTxt1,nPos1,zTxt2,nPos2);` |
|    ! 0 | 1241 | `		}` |
|     25 | 1242 | `		if( (nPos1 + nMax < nLen1) && (nPos2 + nMax < nLen2) ){` |
|     13 | 1243 | `			nSum += SimilarChar(&zTxt1[nPos1 + nMax],nLen1 - nPos1 - nMax,` |
|      8 | 1244 | `				&zTxt2[nPos2 + nMax],nLen2 - nPos2 - nMax);` |
|      4 | 1245 | `		}` |
|     12 | 1246 | `	}` |
|     27 | 1247 | `	return nSum;` |
|      1 | 1248 | `}` |
|      - | 1249 | `/*` |
|      - | 1250 | ` * int similar_text(string $string1,string $string2[,float &$percent])` |
|      - | 1251 | ` *  Calculate the similarity between two strings, as the number of matching` |
|      - | 1252 | ` *  characters found by PHP's greedy longest-common-substring recursion.` |
|      - | 1253 | ` *  When $percent is given it receives the similarity in percent:` |
|      - | 1254 | ` *  matching * 200 / (len1 + len2).` |
|      - | 1255 | ` * Return` |
|      - | 1256 | ` *  The number of matching characters in both strings.` |
|      - | 1257 | ` */` |
|     22 | 1258 | `static int PH7_builtin_similar_text(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1259 | `{` |
|      - | 1260 | `	const char *zStr1,*zStr2;` |
|      - | 1261 | `	ph7_value sTmp1,sTmp2;` |
|      - | 1262 | `	int nLen1,nLen2;` |
|      - | 1263 | `	int nSim;` |
|      - | 1264 | `	sxi32 rc;` |
|     23 | 1265 | `	if( nArg < 2 ){` |
|    ! 0 | 1266 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1267 | `			"ArgumentCountError",` |
|      - | 1268 | `			"similar_text() expects at least 2 arguments, %d given",` |
|    ! 0 | 1269 | `			nArg` |
|      - | 1270 | `			);` |
|      - | 1271 | `	}` |
|     23 | 1272 | `	PH7_MemObjInit(pCtx->pVm,&sTmp1);` |
|     23 | 1273 | `	PH7_MemObjInit(pCtx->pVm,&sTmp2);` |
|     23 | 1274 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"similar_text",1,"$string1","string",` |
|      - | 1275 | `		"similar_text(): Passing null to parameter #1 ($string1) "` |
|      - | 1276 | `		"of type string is deprecated",` |
|      - | 1277 | `		&sTmp1,&zStr1,&nLen1);` |
|     23 | 1278 | `	if( rc != PH7_OK ) goto out;` |
|     23 | 1279 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"similar_text",2,"$string2","string",` |
|      - | 1280 | `		"similar_text(): Passing null to parameter #2 ($string2) "` |
|      - | 1281 | `		"of type string is deprecated",` |
|      - | 1282 | `		&sTmp2,&zStr2,&nLen2);` |
|     23 | 1283 | `	if( rc != PH7_OK ) goto out;` |
|     23 | 1284 | `	if( nLen1 + nLen2 == 0 ){` |
|      5 | 1285 | `		nSim = 0;` |
|      3 | 1286 | `	}else{` |
|     19 | 1287 | `		nSim = SimilarChar(zStr1,nLen1,zStr2,nLen2);` |
|      - | 1288 | `	}` |
|     23 | 1289 | `	if( nArg > 2 ){` |
|      - | 1290 | `		/* Write the percentage through the by-ref out-param */` |
|      7 | 1291 | `		ph7_value *pPercent = ph7_context_new_scalar(pCtx);` |
|      7 | 1292 | `		if( pPercent == 0 ){` |
|    ! 0 | 1293 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1294 | `			goto out;` |
|    ! 0 | 1295 | `		}else{` |
|      7 | 1296 | `			double dPct = (nLen1 + nLen2 == 0) ? 0.0 : (double)nSim * 200.0 / (double)(nLen1 + nLen2);` |
|      7 | 1297 | `			ph7_value_double(pPercent,dPct);` |
|      7 | 1298 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[2],pPercent);` |
|      - | 1299 | `		}` |
|      3 | 1300 | `	}` |
|     23 | 1301 | `	ph7_result_int(pCtx,nSim);` |
|     23 | 1302 | `	rc = PH7_OK;` |
|     11 | 1303 | `out:` |
|     23 | 1304 | `	PH7_MemObjRelease(&sTmp1);` |
|     23 | 1305 | `	PH7_MemObjRelease(&sTmp2);` |
|     23 | 1306 | `	return rc;` |
|     12 | 1307 | `}` |
|      - | 1308 | `/*` |
|      - | 1309 | ` * array\|int str_word_count(string $string[,int $format = 0[,?string $characters = null]])` |
|      - | 1310 | ` *  Count (or return) the words inside a string. A word is a run of alphabetic` |
|      - | 1311 | ` *  characters, which may contain (but not start the string with) "'" and "-";` |
|      - | 1312 | ` *  $characters adds extra bytes to the word set ("a..z" ranges supported, as` |
|      - | 1313 | ` *  in PHP's php_charmask).` |
|      - | 1314 | ` *  $format: 0 -> word count, 1 -> array of words, 2 -> array of words keyed` |
|      - | 1315 | ` *  by their byte position in $string.` |
|      - | 1316 | ` * Errors` |
|      - | 1317 | ` *  ValueError when $format is not 0, 1 or 2.` |
|      - | 1318 | ` */` |
|     44 | 1319 | `static int PH7_builtin_str_word_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1320 | `{` |
|      - | 1321 | `	const char *zIn,*zEnd,*zPtr;` |
|     45 | 1322 | `	ph7_value *pArray = 0,*pValue = 0;` |
|      - | 1323 | `	ph7_value sTmp,sListTmp;` |
|      - | 1324 | `	char aMask[256];` |
|     45 | 1325 | `	int bMask = 0;` |
|     45 | 1326 | `	int iFormat = 0;` |
|     45 | 1327 | `	int nCount = 0;` |
|      - | 1328 | `	int nLen;` |
|      - | 1329 | `	sxi32 rc;` |
|     45 | 1330 | `	if( nArg < 1 ){` |
|    ! 0 | 1331 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1332 | `			"ArgumentCountError",` |
|      - | 1333 | `			"str_word_count() expects at least 1 argument, %d given",` |
|    ! 0 | 1334 | `			nArg` |
|      - | 1335 | `			);` |
|      - | 1336 | `	}` |
|     45 | 1337 | `	PH7_MemObjInit(pCtx->pVm,&sTmp);` |
|     45 | 1338 | `	PH7_MemObjInit(pCtx->pVm,&sListTmp);` |
|     45 | 1339 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_word_count",1,"$string","string",` |
|      - | 1340 | `		"str_word_count(): Passing null to parameter #1 ($string) "` |
|      - | 1341 | `		"of type string is deprecated",` |
|      - | 1342 | `		&sTmp,&zIn,&nLen);` |
|     45 | 1343 | `	if( rc != PH7_OK ) goto out;` |
|     45 | 1344 | `	if( nArg > 1 ){` |
|      - | 1345 | `		sxi64 iVal;` |
|     31 | 1346 | `		rc = PH7_IntArgResolve(pCtx,apArg[1],"str_word_count",2,"$format","int",&iVal);` |
|     33 | 1347 | `		if( rc != PH7_OK ) goto out;` |
|     29 | 1348 | `		if( iVal < 0 \|\| iVal > 2 ){` |
|      5 | 1349 | `			rc = PH7_VmThrowException(pCtx,` |
|      - | 1350 | `				"ValueError",` |
|      - | 1351 | `				"str_word_count(): Argument #2 ($format) must be a valid format value"` |
|      - | 1352 | `				);` |
|      5 | 1353 | `			goto out;` |
|      - | 1354 | `		}` |
|     25 | 1355 | `		iFormat = (int)iVal;` |
|     12 | 1356 | `	}` |
|     39 | 1357 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      - | 1358 | `		/* $characters is ?string: null (skipped above) simply keeps the` |
|      - | 1359 | `		 * default word set, no deprecation. */` |
|      - | 1360 | `		const char *zList;` |
|      - | 1361 | `		int nList;` |
|     13 | 1362 | `		rc = StrPredicateResolveArg(pCtx,apArg[2],"str_word_count",3,"$characters","?string",` |
|      - | 1363 | `			"" /* unreachable: null never gets here */,` |
|      - | 1364 | `			&sListTmp,&zList,&nList);` |
|     13 | 1365 | `		if( rc != PH7_OK ) goto out;` |
|     13 | 1366 | `		PH7_BuildCharMask(pCtx,zList,nList,aMask);` |
|     13 | 1367 | `		bMask = 1;` |
|      6 | 1368 | `	}` |
|     39 | 1369 | `	if( iFormat != 0 ){` |
|     25 | 1370 | `		pArray = ph7_context_new_array(pCtx);` |
|     25 | 1371 | `		pValue = ph7_context_new_scalar(pCtx);` |
|     25 | 1372 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1373 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1374 | `			goto out;` |
|      - | 1375 | `		}` |
|     12 | 1376 | `	}` |
|     39 | 1377 | `	zPtr = zIn;` |
|     39 | 1378 | `	zEnd = &zIn[nLen];` |
|     39 | 1379 | `	if( nLen > 0 ){` |
|      - | 1380 | `		/* php: the string's first byte cannot be ' or -, and its last byte` |
|      - | 1381 | `		 * cannot be -, unless the charlist explicitly allows them. */` |
|     33 | 1382 | `		if( (zPtr[0] == '\'' && (!bMask \|\| !aMask[(unsigned char)'\''])) \|\|` |
|     28 | 1383 | `			(zPtr[0] == '-'  && (!bMask \|\| !aMask[(unsigned char)'-'])) ){` |
|      9 | 1384 | `			zPtr++;` |
|      4 | 1385 | `		}` |
|     33 | 1386 | `		if( zEnd[-1] == '-' && (!bMask \|\| !aMask[(unsigned char)'-']) ){` |
|      9 | 1387 | `			zEnd--;` |
|      4 | 1388 | `		}` |
|     16 | 1389 | `	}` |
|    135 | 1390 | `	while( zPtr < zEnd ){` |
|     91 | 1391 | `		const char *zStart = zPtr;` |
|    477 | 1392 | `		while( zPtr < zEnd && ( SyisAlpha((unsigned char)zPtr[0])` |
|    253 | 1393 | `			\|\| (bMask && aMask[(unsigned char)zPtr[0]])` |
|     98 | 1394 | `			\|\| zPtr[0] == '\'' \|\| zPtr[0] == '-' ) ){` |
|    339 | 1395 | `			zPtr++;` |
|      1 | 1396 | `		}` |
|     97 | 1397 | `		if( zPtr > zStart ){` |
|     91 | 1398 | `			if( iFormat == 0 ){` |
|     19 | 1399 | `				nCount++;` |
|     10 | 1400 | `			}else{` |
|     73 | 1401 | `				ph7_value_reset_string_cursor(pValue);` |
|     73 | 1402 | `				if( SXRET_OK != ph7_value_string(pValue,zStart,(int)(zPtr-zStart)) ){` |
|    ! 0 | 1403 | `					rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1404 | `					goto out;` |
|      - | 1405 | `				}` |
|     73 | 1406 | `				if( iFormat == 1 ){` |
|     59 | 1407 | `					if( SXRET_OK != ph7_array_add_elem(pArray,0,pValue) ){` |
|    ! 0 | 1408 | `						rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1409 | `						goto out;` |
|      - | 1410 | `					}` |
|     30 | 1411 | `				}else{` |
|     15 | 1412 | `					if( SXRET_OK != ph7_array_add_intkey_elem(pArray,(int)(zStart-zIn),pValue) ){` |
|    ! 0 | 1413 | `						rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1414 | `						goto out;` |
|      - | 1415 | `					}` |
|      - | 1416 | `				}` |
|      - | 1417 | `			}` |
|     45 | 1418 | `		}` |
|     97 | 1419 | `		zPtr++;` |
|      1 | 1420 | `	}` |
|     37 | 1421 | `	if( iFormat == 0 ){` |
|     13 | 1422 | `		ph7_result_int(pCtx,nCount);` |
|      7 | 1423 | `	}else{` |
|     25 | 1424 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1425 | `	}` |
|     37 | 1426 | `	rc = PH7_OK;` |
|     21 | 1427 | `out:` |
|     43 | 1428 | `	PH7_MemObjRelease(&sTmp);` |
|     43 | 1429 | `	PH7_MemObjRelease(&sListTmp);` |
|     43 | 1430 | `	return rc;` |
|     22 | 1431 | `}` |
|      - | 1432 | `/*` |
|      - | 1433 | ` * string chunk_split(string $body[,int $chunklen = 76 [, string $end = "\r\n" ]])` |
|      - | 1434 | ` *   Split a string into smaller chunks.` |
|      - | 1435 | ` * Parameters` |
|      - | 1436 | ` *  $body` |
|      - | 1437 | ` *   The string to be chunked.` |
|      - | 1438 | ` * $chunklen` |
|      - | 1439 | ` *   The chunk length.` |
|      - | 1440 | ` * $end` |
|      - | 1441 | ` *   The line ending sequence.` |
|      - | 1442 | ` * Return` |
|      - | 1443 | ` *  The chunked string or NULL on failure.` |
|      - | 1444 | ` */` |
|     14 | 1445 | `static int PH7_builtin_chunk_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1446 | `{` |
|     15 | 1447 | `	const char *zIn,*zEnd,*zSep = "\r\n";` |
|      - | 1448 | `	int nSepLen,nChunkLen,nLen;` |
|      - | 1449 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1450 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     15 | 1451 | `	if( nArg < 1 ){` |
|      - | 1452 | `		/* Nothing to split,return null */` |
|    ! 0 | 1453 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1454 | `		return PH7_OK;` |
|      - | 1455 | `	}` |
|      - | 1456 | `	/* initialize/Extract arguments */` |
|     15 | 1457 | `	nSepLen = (int)sizeof("\r\n") - 1;` |
|     15 | 1458 | `	nChunkLen = 76;` |
|     15 | 1459 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 1460 | `	zEnd = &zIn[nLen];` |
|     15 | 1461 | `	if( nArg > 1 ){` |
|      - | 1462 | `		/* Chunk length */` |
|     13 | 1463 | `		nChunkLen = ph7_value_to_int(apArg[1]);` |
|     13 | 1464 | `		if( nChunkLen < 1 ){` |
|      - | 1465 | `			/* PHP 8 throws a catchable ValueError for a non-positive length. */` |
|      3 | 1466 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1467 | `				"chunk_split(): Argument #2 ($length) must be greater than 0");` |
|      - | 1468 | `		}` |
|     11 | 1469 | `		if( nArg > 2 ){` |
|      - | 1470 | `			/* Separator */` |
|      9 | 1471 | `			zSep = ph7_value_to_string(apArg[2],&nSepLen);` |
|      9 | 1472 | `			if( nSepLen < 1 ){` |
|      - | 1473 | `				/* Switch back to the default separator */` |
|      3 | 1474 | `				zSep = "\r\n";` |
|      3 | 1475 | `				nSepLen = (int)sizeof("\r\n") - 1;` |
|      1 | 1476 | `			}` |
|      4 | 1477 | `		}` |
|      5 | 1478 | `	}` |
|      - | 1479 | `	/* Perform the requested operation */` |
|     13 | 1480 | `	if( nChunkLen > nLen ){` |
|      - | 1481 | `		/* Nothing to split,return the string and the separator */` |
|      9 | 1482 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nLen,zIn,nSepLen,zSep);` |
|      9 | 1483 | `		return PH7_OK;` |
|      - | 1484 | `	}` |
|     17 | 1485 | `	while( zIn < zEnd ){` |
|     13 | 1486 | `		if( nChunkLen > (int)(zEnd-zIn) ){` |
|      3 | 1487 | `			nChunkLen = (int)(zEnd - zIn);` |
|      1 | 1488 | `		}` |
|      - | 1489 | `		/* Append the chunk and the separator */` |
|     13 | 1490 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nChunkLen,zIn,nSepLen,zSep);` |
|      - | 1491 | `		/* Point beyond the chunk */` |
|     13 | 1492 | `		zIn += nChunkLen;` |
|      1 | 1493 | `	}` |
|      5 | 1494 | `	return PH7_OK;` |
|      8 | 1495 | `}` |
|      - | 1496 | `/*` |
|      - | 1497 | ` * string addslashes(string $str)` |
|      - | 1498 | ` *  Quote string with slashes.` |
|      - | 1499 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1500 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1501 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1502 | ` * Parameter` |
|      - | 1503 | ` *  str: The string to be escaped.` |
|      - | 1504 | ` * Return` |
|      - | 1505 | ` *  Returns the escaped string` |
|      - | 1506 | ` */` |
|     20 | 1507 | `static int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1508 | `{` |
|      - | 1509 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1510 | `	int nLen;` |
|      - | 1511 | `	/* PHP enforces exactly one argument. */` |
|     22 | 1512 | `	if( nArg != 1 ){` |
|      4 | 1513 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1514 | `			"ArgumentCountError",` |
|      - | 1515 | `			"addslashes() expects exactly 1 argument, %d given",` |
|      1 | 1516 | `			nArg` |
|      - | 1517 | `			);` |
|      - | 1518 | `	}` |
|      - | 1519 | `	/* NULL is deprecated and treated as an empty string; other invalid` |
|      - | 1520 | `	 * types still produce a TypeError. */` |
|     19 | 1521 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 1522 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 1523 | `			E_DEPRECATED,` |
|      - | 1524 | `			"addslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - | 1525 | `			);` |
|      - | 1526 | `		/* fall through so conversion below yields empty string */` |
|      1 | 1527 | `	}` |
|      - | 1528 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     27 | 1529 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     28 | 1530 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     18 | 1531 | `	    ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 1532 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1533 | `			"TypeError",` |
|      - | 1534 | `			"addslashes(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 1535 | `			ph7_type_name(apArg[0])` |
|      - | 1536 | `			);` |
|      - | 1537 | `	}` |
|      - | 1538 | `	/* Convert to string representation first and obtain length. */` |
|     19 | 1539 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 1540 | `	if( nLen < 1 ){` |
|      - | 1541 | `		/* Return the empty string */` |
|      5 | 1542 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 1543 | `		return PH7_OK;` |
|      - | 1544 | `	}` |
|     15 | 1545 | `	zEnd = &zIn[nLen];` |
|     15 | 1546 | `	zCur = 0; /* cc warning */` |
|     20 | 1547 | `	for(;;){` |
|     41 | 1548 | `		if( zIn >= zEnd ){` |
|      - | 1549 | `			/* No more input */` |
|     15 | 1550 | `			break;` |
|      - | 1551 | `		}` |
|     27 | 1552 | `		zCur = zIn;` |
|      - | 1553 | `		/* scan until a character that needs escaping (', ", \\, or NUL) */` |
|     89 | 1554 | `		while( zIn < zEnd && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '\\' && zIn[0] != '\0' ){` |
|     63 | 1555 | `			zIn++;` |
|      1 | 1556 | `		}` |
|     27 | 1557 | `		if( zIn > zCur ){` |
|      - | 1558 | `			/* Append raw contents */` |
|     23 | 1559 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 1560 | `		}` |
|     27 | 1561 | `		if( zIn < zEnd ){` |
|     17 | 1562 | `			int c = zIn[0];` |
|     17 | 1563 | `			if( c == '\0' ){` |
|      - | 1564 | `				/* PHP escapes NUL as "\\0" (two characters) */` |
|      3 | 1565 | `				ph7_result_string(pCtx,"\\0",2);` |
|      2 | 1566 | `			}else{` |
|     15 | 1567 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - | 1568 | `			}` |
|      8 | 1569 | `		}` |
|     27 | 1570 | `		zIn++;` |
|      1 | 1571 | `	}` |
|     15 | 1572 | `	return PH7_OK;` |
|     12 | 1573 | `}` |
|      - | 1574 | `/*` |
|      - | 1575 | ``  * Build a 256-entry membership mask from a PHP charlist, expanding `a..z` `` |
|      - | 1576 | ` * byte ranges exactly like PHP's php_charmask(). On return aMask[c] != 0 iff` |
|      - | 1577 | ` * the byte c belongs to the set. Emits the PHP-exact warnings for the three` |
|      - | 1578 | ` * malformed-range shapes (ph7_context_throw_error_format prepends the active` |
|      - | 1579 | ` * function name, so the messages omit it); on a bad range the surrounding` |
|      - | 1580 | ` * bytes are still added and the scan never aborts. Reads only within` |
|      - | 1581 | ` * [zList, zList+nLen).` |
|      - | 1582 | ` *` |
|      - | 1583 | ` * Use ONLY for the builtins whose charlist expands ranges the way PHP's` |
|      - | 1584 | ` * php_charmask() does: trim/ltrim/rtrim/addcslashes (and quotemeta, whose set` |
|      - | 1585 | ` * is a fixed literal with no ".."). Do NOT route strspn/strcspn/strtok/strpbrk` |
|      - | 1586 | ` * through this — PHP treats their charlists literally, so expanding "a..z" here` |
|      - | 1587 | ` * would be a behavior regression plus spurious "Invalid '..'-range" warnings.` |
|      - | 1588 | ` */` |
|    240 | 1589 | `static void PH7_BuildCharMask(ph7_context *pCtx,const char *zList,int nLen,char aMask[256])` |
|      5 | 1590 | `{` |
|    245 | 1591 | `	const unsigned char *zIn  = (const unsigned char *)zList;` |
|    245 | 1592 | `	const unsigned char *zEnd = zIn + (nLen > 0 ? nLen : 0);` |
|    245 | 1593 | `	SyZero(aMask,256);` |
|    653 | 1594 | `	for( ; zIn < zEnd ; zIn++ ){` |
|    413 | 1595 | `		int c = zIn[0];` |
|    413 | 1596 | `		if( zIn + 3 < zEnd && zIn[1] == '.' && zIn[2] == '.' && zIn[3] >= c ){` |
|      - | 1597 | `			/* Valid incrementing range c..zIn[3] */` |
|     22 | 1598 | `			int hi = zIn[3],k;` |
|    386 | 1599 | `			for( k = c ; k <= hi ; k++ ){` |
|    366 | 1600 | `				aMask[k] = 1;` |
|    184 | 1601 | `			}` |
|     22 | 1602 | `			zIn += 3; /* the loop's ++ then steps past the range end */` |
|    412 | 1603 | `		}else if( zIn + 1 < zEnd && zIn[0] == '.' && zIn[1] == '.' ){` |
|      - | 1604 | `			/* Malformed range: mirror php_charmask's three diagnostics. */` |
|      - | 1605 | `			const char *zMsg;` |
|     20 | 1606 | `			if( (const unsigned char *)zList >= zIn ){` |
|      6 | 1607 | `				zMsg = "no character to the left of '..'";` |
|     18 | 1608 | `			}else if( zIn + 2 >= zEnd ){` |
|      6 | 1609 | `				zMsg = "no character to the right of '..'";` |
|     14 | 1610 | `			}else if( zIn[-1] > zIn[2] ){` |
|     12 | 1611 | `				zMsg = "'..'-range needs to be incrementing";` |
|      7 | 1612 | `			}else{` |
|    ! 0 | 1613 | `				zMsg = 0; /* catch-all (e.g. a..b..c) */` |
|      - | 1614 | `			}` |
|     20 | 1615 | `			if( zMsg ){` |
|     29 | 1616 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      9 | 1617 | `					"Invalid '..'-range, %s",zMsg);` |
|     11 | 1618 | `			}else{` |
|    ! 0 | 1619 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1620 | `					"Invalid '..'-range");` |
|      - | 1621 | `			}` |
|      - | 1622 | `			/* Do not consume the dots: the loop's ++ steps one byte so the` |
|      - | 1623 | `			 * dots are re-scanned as literals, exactly like php_charmask. */` |
|     11 | 1624 | `		}else{` |
|    375 | 1625 | `			aMask[c] = 1;` |
|      - | 1626 | `		}` |
|    209 | 1627 | `	}` |
|    245 | 1628 | `}` |
|      - | 1629 | `/*` |
|      - | 1630 | ` * string addcslashes(string $str,string $charlist)` |
|      - | 1631 | ` *  Quote string with slashes in a C style.` |
|      - | 1632 | ` * Parameter` |
|      - | 1633 | ` *  $str:` |
|      - | 1634 | ` *    The string to be escaped.` |
|      - | 1635 | ` *  $charlist:` |
|      - | 1636 | ` *    A list of characters to be escaped. If charlist contains characters \n, \r etc.` |
|      - | 1637 | ` *    they are converted in C-like style, while other non-alphanumeric characters` |
|      - | 1638 | ` *    with ASCII codes lower than 32 and higher than 126 converted to octal representation.` |
|      - | 1639 | ` * Return` |
|      - | 1640 | ` *  Returns the escaped string.` |
|      - | 1641 | ` * Note:` |
|      - | 1642 | ` *  Character ranges [i.e: 'A..Z'] are supported (see PH7_BuildCharMask).` |
|      - | 1643 | ` */` |
|     34 | 1644 | `static int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1645 | `{` |
|      - | 1646 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|      - | 1647 | `	char aMask[256];` |
|      - | 1648 | `	int nLen,nMask;` |
|      - | 1649 | `	/* PHP enforces exactly two arguments. */` |
|     37 | 1650 | `	if( nArg != 2 ){` |
|      4 | 1651 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1652 | `			"ArgumentCountError",` |
|      - | 1653 | `			"addcslashes() expects exactly 2 arguments, %d given",` |
|      1 | 1654 | `			nArg` |
|      - | 1655 | `			);` |
|      - | 1656 | `	}` |
|      - | 1657 | `	/* First argument must be a string-ish value.  NULL is deprecated and` |
|      - | 1658 | `	 * treated as the empty string (PHP 8.1). */` |
|     35 | 1659 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      - | 1660 | `		/* Emit deprecation only once, similar to PHP behaviour. */` |
|      3 | 1661 | `		PH7_VmThrowError(pCtx->pVm,0,/* iErr will be patched to 8192 below */` |
|      - | 1662 | `			E_DEPRECATED,` |
|      - | 1663 | `			"addcslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - | 1664 | `			);` |
|      - | 1665 | `		/* treat as empty string; fall through to conversion logic */` |
|     47 | 1666 | `	} else if( ph7_value_is_array(apArg[0]) \|\|` |
|     48 | 1667 | `	          ph7_value_is_object(apArg[0]) \|\|` |
|     30 | 1668 | `	          ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 1669 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1670 | `			"TypeError",` |
|      - | 1671 | `			"addcslashes(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 1672 | `			ph7_type_name(apArg[0])` |
|      - | 1673 | `			);` |
|      - | 1674 | `	}` |
|      - | 1675 | `	/* Second argument must be a string.  NULL is deprecated and treated as` |
|      - | 1676 | `	 * an empty mask per PHP semantics.  Arrays/objects/resources still` |
|      - | 1677 | `	 * trigger a TypeError. */` |
|     35 | 1678 | `	if( ph7_value_is_null(apArg[1]) ){` |
|      3 | 1679 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 1680 | `			E_DEPRECATED,` |
|      - | 1681 | `			"addcslashes(): Passing null to parameter #2 ($characters) of type string is deprecated"` |
|      - | 1682 | `			);` |
|      - | 1683 | `		/* allow through so it becomes empty string below */` |
|     47 | 1684 | `	} else if( ph7_value_is_array(apArg[1]) \|\|` |
|     48 | 1685 | `	          ph7_value_is_object(apArg[1]) \|\|` |
|     30 | 1686 | `	          ph7_value_is_resource(apArg[1]) ){` |
|    ! 0 | 1687 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1688 | `			"TypeError",` |
|      - | 1689 | `			"addcslashes(): Argument #2 ($characters) must be of type string, %s given",` |
|    ! 0 | 1690 | `			ph7_type_name(apArg[1])` |
|      - | 1691 | `			);` |
|      - | 1692 | `	}` |
|      - | 1693 | `	/* Extract the string to process */` |
|     35 | 1694 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1695 | `	/* NULL would never reach here due to the check above. */` |
|     35 | 1696 | `	if( nLen < 1 ){` |
|      - | 1697 | `		/* Empty string returns itself. */` |
|      5 | 1698 | `		ph7_result_string(pCtx,zIn,nLen);` |
|      5 | 1699 | `		return PH7_OK;` |
|      - | 1700 | `	}` |
|      - | 1701 | ``	/* Extract the desired mask and expand any `a..z` ranges into a lookup. */`` |
|     31 | 1702 | `	zMask = ph7_value_to_string(apArg[1],&nMask);` |
|     31 | 1703 | `	PH7_BuildCharMask(pCtx,zMask,nMask,aMask);` |
|     31 | 1704 | `	zEnd = &zIn[nLen];` |
|     31 | 1705 | `	zCur = 0; /* cc warning */` |
|     37 | 1706 | `	for(;;){` |
|     77 | 1707 | `		if( zIn >= zEnd ){` |
|      - | 1708 | `			/* No more input */` |
|     31 | 1709 | `			break;` |
|      - | 1710 | `		}` |
|     49 | 1711 | `		zCur = zIn;` |
|    125 | 1712 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|     79 | 1713 | `			zIn++;` |
|      3 | 1714 | `		}` |
|     49 | 1715 | `		if( zIn > zCur ){` |
|      - | 1716 | `			/* Append raw contents */` |
|     43 | 1717 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     20 | 1718 | `		}` |
|     49 | 1719 | `		if( zIn < zEnd ){` |
|      - | 1720 | `			/* Make sure we treat the byte as unsigned to avoid negative values` |
|      - | 1721 | `			 * on platforms where char is signed. */` |
|     29 | 1722 | `			int c = (unsigned char)zIn[0];` |
|      - | 1723 | `			/* Handle special C-like escapes for common control characters first.` |
|      - | 1724 | `			 * PHP outputs "\n" "\r" "\t" "\v" "\f" when those chars are` |
|      - | 1725 | `			 * in the mask. NUL is left to the octal conversion below. */` |
|     29 | 1726 | `			if( c == '\n' ){` |
|      3 | 1727 | `				ph7_result_string(pCtx,"\\n",2);` |
|     28 | 1728 | `			}else if( c == '\r' ){` |
|      3 | 1729 | `				ph7_result_string(pCtx,"\\r",2);` |
|     26 | 1730 | `			}else if( c == '\t' ){` |
|      3 | 1731 | `				ph7_result_string(pCtx,"\\t",2);` |
|     24 | 1732 | `			}else if( c == '\v' ){` |
|      3 | 1733 | `				ph7_result_string(pCtx,"\\v",2);` |
|     22 | 1734 | `			}else if( c == '\f' ){` |
|      3 | 1735 | `				ph7_result_string(pCtx,"\\f",2);` |
|     20 | 1736 | `			}else if( c > 126 \|\| (c < 32 && (!SyisAlphaNum(c)/*EBCDIC*/ && !SyisSpace(c))) ){` |
|      - | 1737 | `				/* Convert to octal.  PHP always emits three-digit zero-padded` |
|      - | 1738 | `				 * octal escapes (\001 not \1). */` |
|      7 | 1739 | `				ph7_result_string_format(pCtx,"\\%03o",c);` |
|      4 | 1740 | `			}else{` |
|     13 | 1741 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - | 1742 | `			}` |
|     13 | 1743 | `		}` |
|     49 | 1744 | `		zIn++;` |
|      3 | 1745 | `	}` |
|     31 | 1746 | `	return PH7_OK;` |
|     20 | 1747 | `}` |
|      - | 1748 | `/*` |
|      - | 1749 | ` * string quotemeta(string $str)` |
|      - | 1750 | ` *  Quote meta characters.` |
|      - | 1751 | ` * Parameter` |
|      - | 1752 | ` *  $str:` |
|      - | 1753 | ` *    The string to be escaped.` |
|      - | 1754 | ` * Return` |
|      - | 1755 | ` *  Returns the escaped string.` |
|      - | 1756 | `*/` |
|     10 | 1757 | `static int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1758 | `{` |
|      - | 1759 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1760 | `	char aMask[256];` |
|      - | 1761 | `	int nLen;` |
|     12 | 1762 | `	if( nArg < 1 ){` |
|      - | 1763 | `		/* Nothing to process,retun NULL */` |
|    ! 0 | 1764 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1765 | `		return PH7_OK;` |
|      - | 1766 | `	}` |
|      - | 1767 | `	/* Extract the string to process */` |
|     12 | 1768 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     12 | 1769 | `	if( nLen < 1 ){` |
|      - | 1770 | `		/* Return the empty string */` |
|      3 | 1771 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1772 | `		return PH7_OK;` |
|      - | 1773 | `	}` |
|      - | 1774 | `	/* Fixed meta-character set (no ranges); build the lookup once. */` |
|     10 | 1775 | `	PH7_BuildCharMask(pCtx,".\\+*?[^]($)",(int)sizeof(".\\+*?[^]($)")-1,aMask);` |
|     10 | 1776 | `	zEnd = &zIn[nLen];` |
|     10 | 1777 | `	zCur = 0; /* cc warning */` |
|     22 | 1778 | `	for(;;){` |
|     46 | 1779 | `		if( zIn >= zEnd ){` |
|      - | 1780 | `			/* No more input */` |
|     10 | 1781 | `			break;` |
|      - | 1782 | `		}` |
|     38 | 1783 | `		zCur = zIn;` |
|     76 | 1784 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|     40 | 1785 | `			zIn++;` |
|      2 | 1786 | `		}` |
|     38 | 1787 | `		if( zIn > zCur ){` |
|      - | 1788 | `			/* Append raw contents */` |
|     20 | 1789 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      9 | 1790 | `		}` |
|     38 | 1791 | `		if( zIn < zEnd ){` |
|     36 | 1792 | `			int c = zIn[0];` |
|     36 | 1793 | `			ph7_result_string_format(pCtx,"\\%c",c);` |
|     17 | 1794 | `		}` |
|     38 | 1795 | `		zIn++;` |
|      2 | 1796 | `	}` |
|     10 | 1797 | `	return PH7_OK;` |
|      7 | 1798 | `}` |
|      - | 1799 | `/*` |
|      - | 1800 | ` * string stripslashes(string $str)` |
|      - | 1801 | ` *  Un-quotes a quoted string.` |
|      - | 1802 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1803 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1804 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1805 | ` * Parameter` |
|      - | 1806 | ` *  $str` |
|      - | 1807 | ` *   The input string.` |
|      - | 1808 | ` * Return` |
|      - | 1809 | ` *  Returns a string with backslashes stripped off.` |
|      - | 1810 | ` */` |
|      6 | 1811 | `static int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1812 | `{` |
|      - | 1813 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1814 | `	int nLen;` |
|      7 | 1815 | `	if( nArg < 1 ){` |
|      - | 1816 | `		/* Nothing to process,retun NULL */` |
|    ! 0 | 1817 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1818 | `		return PH7_OK;` |
|      - | 1819 | `	}` |
|      - | 1820 | `	/* Extract the string to process */` |
|      7 | 1821 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 1822 | `	if( zIn == 0 ){` |
|    ! 0 | 1823 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1824 | `		return PH7_OK;` |
|      - | 1825 | `	}` |
|      7 | 1826 | `	zEnd = &zIn[nLen];` |
|      7 | 1827 | `	zCur = 0; /* cc warning */` |
|      - | 1828 | `	/* Encode the string */` |
|      4 | 1829 | `	for(;;){` |
|      9 | 1830 | `		if( zIn >= zEnd ){` |
|      - | 1831 | `			/* No more input */` |
|      5 | 1832 | `			break;` |
|      - | 1833 | `		}` |
|      5 | 1834 | `		zCur = zIn;` |
|     17 | 1835 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|     13 | 1836 | `			zIn++;` |
|      1 | 1837 | `		}` |
|      5 | 1838 | `		if( zIn > zCur ){` |
|      - | 1839 | `			/* Append raw contents */` |
|      5 | 1840 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 1841 | `		}` |
|      5 | 1842 | `		if( &zIn[1] < zEnd ){` |
|      3 | 1843 | `			int c = zIn[1];` |
|      3 | 1844 | `			if( c == '\'' \|\| c == '"' \|\| c == '\\' ){` |
|      - | 1845 | `				/* Ignore the backslash */` |
|      3 | 1846 | `				zIn++;` |
|      1 | 1847 | `			}` |
|      2 | 1848 | `		}else{` |
|      3 | 1849 | `			break;` |
|      - | 1850 | `		}` |
|      1 | 1851 | `	}` |
|      7 | 1852 | `	return PH7_OK;` |
|      4 | 1853 | `}` |
|      - | 1854 | `/*` |
|      - | 1855 | ` * UTF-8-aware HTML entity machinery, shared by htmlspecialchars/htmlentities/` |
|      - | 1856 | ` * htmlspecialchars_decode/html_entity_decode/get_html_translation_table.` |
|      - | 1857 | ` * The implementations live further down in this file, next to the filter_var` |
|      - | 1858 | ` * FULL_SPECIAL_CHARS machinery they reuse (aHtml401Ent[]/FvHtml401Lookup()/` |
|      - | 1859 | ` * FvUtf8Next()). Semantics are byte-exact vs php 8.5.7; PHL is UTF-8-only` |
|      - | 1860 | ` * so every charset argument other than a UTF-8 alias gets PHP's` |
|      - | 1861 | ` * unsupported-charset warning and is treated as UTF-8.` |
|      - | 1862 | ` *` |
|      - | 1863 | ` * Flag model (the PHP-exact ENT_* values, see constant.c): bit 1 = encode/` |
|      - | 1864 | ` * decode single quotes, bit 2 = double quotes (ENT_QUOTES=3, ENT_COMPAT=2,` |
|      - | 1865 | ` * ENT_NOQUOTES=0); bits 16\|32 select the doctype (0=HTML401, 16=XML1,` |
|      - | 1866 | ` * 32=XHTML, 48=HTML5); ENT_IGNORE=4 drops invalid UTF-8 bytes (wins over` |
|      - | 1867 | ` * ENT_SUBSTITUTE=8, which replaces each with U+FFFD; with neither set the` |
|      - | 1868 | ` * whole result collapses to ""); ENT_DISALLOWED=128 substitutes valid but` |
|      - | 1869 | ` * doctype-disallowed codepoints. The shared default is` |
|      - | 1870 | ` * ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 = 11.` |
|      - | 1871 | ` */` |
|      - | 1872 | `static void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,int iFlags,int bAll,int bDoubleEncode);` |
|      - | 1873 | `static void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,int iFlags,int bFull);` |
|      - | 1874 | `static void HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx);` |
|      - | 1875 | `static void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags);` |
|      - | 1876 | `/*` |
|      - | 1877 | ` * string htmlspecialchars(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1878 | ` *                         [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])` |
|      - | 1879 | ` *  Convert the special characters & < > " ' to HTML entities.` |
|      - | 1880 | ` * Return` |
|      - | 1881 | ` *  The escaped string or NULL on failure.` |
|      - | 1882 | ` */` |
|     42 | 1883 | `static int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1884 | `{` |
|     43 | 1885 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1886 | `	const char *zIn;` |
|     43 | 1887 | `	int nLen,bDouble = 1;` |
|      - | 1888 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1889 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     43 | 1890 | `	if( nArg < 1 ){` |
|      - | 1891 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1892 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1893 | `		return PH7_OK;` |
|      - | 1894 | `	}` |
|      - | 1895 | `	/* Extract the target string */` |
|     43 | 1896 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     43 | 1897 | `	if( nArg > 1 ){` |
|     35 | 1898 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     17 | 1899 | `	}` |
|     43 | 1900 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     43 | 1901 | `	if( nArg > 3 ){` |
|      7 | 1902 | `		bDouble = ph7_value_to_bool(apArg[3]);` |
|      3 | 1903 | `	}` |
|     43 | 1904 | `	HtmlEscape(pCtx,zIn,nLen,iFlags,0,bDouble);` |
|     43 | 1905 | `	return PH7_OK;` |
|     22 | 1906 | `}` |
|      - | 1907 | `/*` |
|      - | 1908 | ` * string htmlspecialchars_decode(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401])` |
|      - | 1909 | ` *  Convert the special HTML entities (&amp; &lt; &gt; &quot; and the` |
|      - | 1910 | ` *  numeric/doctype forms of the two quotes) back to characters.` |
|      - | 1911 | ` * Return` |
|      - | 1912 | ` *  The unescaped string or NULL on failure.` |
|      - | 1913 | ` */` |
|     22 | 1914 | `static int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1915 | `{` |
|     23 | 1916 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1917 | `	const char *zIn;` |
|      - | 1918 | `	int nLen;` |
|      - | 1919 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1920 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     23 | 1921 | `	if( nArg < 1 ){` |
|      - | 1922 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1923 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1924 | `		return PH7_OK;` |
|      - | 1925 | `	}` |
|      - | 1926 | `	/* Extract the target string */` |
|     23 | 1927 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 1928 | `	if( nArg > 1 ){` |
|      9 | 1929 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      4 | 1930 | `	}` |
|     23 | 1931 | `	HtmlUnescape(pCtx,zIn,nLen,iFlags,0);` |
|     23 | 1932 | `	return PH7_OK;` |
|     12 | 1933 | `}` |
|      - | 1934 | `/*` |
|      - | 1935 | ` * array get_html_translation_table(int $table = HTML_SPECIALCHARS` |
|      - | 1936 | ` *      [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 [, string $encoding = "UTF-8"]])` |
|      - | 1937 | ` *  Return the translation table used by htmlspecialchars() (HTML_SPECIALCHARS)` |
|      - | 1938 | ` *  or htmlentities() (HTML_ENTITIES) as character => entity pairs.` |
|      - | 1939 | ` * Return` |
|      - | 1940 | ` *  The translation table as an array or NULL on failure.` |
|      - | 1941 | ` */` |
|     12 | 1942 | `static int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1943 | `{` |
|     13 | 1944 | `	int iTable = 0; /* HTML_SPECIALCHARS */` |
|     13 | 1945 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|     13 | 1946 | `	if( nArg > 0 ){` |
|     11 | 1947 | `		iTable = ph7_value_to_int(apArg[0]);` |
|      5 | 1948 | `	}` |
|     13 | 1949 | `	if( nArg > 1 ){` |
|      9 | 1950 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      4 | 1951 | `	}` |
|     13 | 1952 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     13 | 1953 | `	HtmlTranslationTable(pCtx,iTable,iFlags);` |
|     13 | 1954 | `	return PH7_OK;` |
|      1 | 1955 | `}` |
|      - | 1956 | `/*` |
|      - | 1957 | ` * string htmlentities(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1958 | ` *                     [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])` |
|      - | 1959 | ` *  Convert all applicable characters to HTML entities: the specials plus` |
|      - | 1960 | ` *  every codepoint with an HTML 4.01 named entity (aHtml401Ent[]).` |
|      - | 1961 | ` * Return` |
|      - | 1962 | ` *  The encoded string or NULL on failure.` |
|      - | 1963 | ` */` |
|     30 | 1964 | `static int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1965 | `{` |
|     31 | 1966 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1967 | `	const char *zIn;` |
|     31 | 1968 | `	int nLen,bDouble = 1;` |
|      - | 1969 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1970 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     31 | 1971 | `	if( nArg < 1 ){` |
|      - | 1972 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1973 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1974 | `		return PH7_OK;` |
|      - | 1975 | `	}` |
|      - | 1976 | `	/* Extract the target string */` |
|     31 | 1977 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 1978 | `	if( nArg > 1 ){` |
|     19 | 1979 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      9 | 1980 | `	}` |
|     31 | 1981 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     31 | 1982 | `	if( nArg > 3 ){` |
|      3 | 1983 | `		bDouble = ph7_value_to_bool(apArg[3]);` |
|      1 | 1984 | `	}` |
|     31 | 1985 | `	HtmlEscape(pCtx,zIn,nLen,iFlags,1,bDouble);` |
|     31 | 1986 | `	return PH7_OK;` |
|     16 | 1987 | `}` |
|      - | 1988 | `/*` |
|      - | 1989 | ` * string html_entity_decode(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1990 | ` *                           [, string $encoding = "UTF-8"]])` |
|      - | 1991 | ` *  Convert HTML entities (named — case-sensitive — and numeric, decimal or` |
|      - | 1992 | ` *  hex) back to their UTF-8 characters. The reverse of htmlentities().` |
|      - | 1993 | ` * Return` |
|      - | 1994 | ` *  The decoded string or NULL on failure.` |
|      - | 1995 | ` */` |
|     58 | 1996 | `static int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1997 | `{` |
|     59 | 1998 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1999 | `	const char *zIn;` |
|      - | 2000 | `	int nLen;` |
|      - | 2001 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 2002 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     59 | 2003 | `	if( nArg < 1 ){` |
|      - | 2004 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 2005 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2006 | `		return PH7_OK;` |
|      - | 2007 | `	}` |
|      - | 2008 | `	/* Extract the target string */` |
|     59 | 2009 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     59 | 2010 | `	if( nArg > 1 ){` |
|     27 | 2011 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     13 | 2012 | `	}` |
|     59 | 2013 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     59 | 2014 | `	HtmlUnescape(pCtx,zIn,nLen,iFlags,1);` |
|     59 | 2015 | `	return PH7_OK;` |
|     30 | 2016 | `}` |
|      - | 2017 | `/*` |
|      - | 2018 | ` * int strlen($string)` |
|      - | 2019 | ` *  return the length of the given string.` |
|      - | 2020 | ` * Parameter` |
|      - | 2021 | ` *  string: The string being measured for length.` |
|      - | 2022 | ` * Return` |
|      - | 2023 | ` *  length of the given string.` |
|      - | 2024 | ` */` |
|  75874 | 2025 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2026 | `{` |
|  75879 | 2027 | `	int iLen = 0;` |
|  75879 | 2028 | `	if( nArg > 0 ){` |
|  75879 | 2029 | `		StrNullArgNotice(pCtx,apArg[0],"strlen",1,"$string");` |
|  75879 | 2030 | `		ph7_value_to_string(apArg[0],&iLen);` |
|  37937 | 2031 | `	}` |
|      - | 2032 | `	/* String length */` |
|  75879 | 2033 | `	ph7_result_int(pCtx,iLen);` |
|  75879 | 2034 | `	return PH7_OK;` |
|      5 | 2035 | `}` |
|      - | 2036 | `/*` |
|      - | 2037 | ` * int strcmp(string $str1,string $str2)` |
|      - | 2038 | ` *  Perform a binary safe string comparison.` |
|      - | 2039 | ` * Parameter` |
|      - | 2040 | ` *  str1: The first string` |
|      - | 2041 | ` *  str2: The second string` |
|      - | 2042 | ` * Return` |
|      - | 2043 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2044 | ` *  than str2, and 0 if they are equal.` |
|      - | 2045 | ` */` |
|     72 | 2046 | `static int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2047 | `{` |
|      - | 2048 | `	const char *z1,*z2;` |
|      - | 2049 | `	int n1,n2;` |
|      - | 2050 | `	int res;` |
|     73 | 2051 | `	if( nArg < 2 ){` |
|    ! 0 | 2052 | `		res = nArg == 0 ? 0 : 1;` |
|    ! 0 | 2053 | `		ph7_result_int(pCtx,res);` |
|    ! 0 | 2054 | `		return PH7_OK;` |
|      - | 2055 | `	}` |
|      - | 2056 | `	/* Perform the comparison */` |
|     73 | 2057 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     73 | 2058 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     73 | 2059 | `	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2060 | `	/* Comparison result */` |
|     73 | 2061 | `	ph7_result_int(pCtx,res);` |
|     73 | 2062 | `	return PH7_OK;` |
|     37 | 2063 | `}` |
|      - | 2064 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 2065 | `/*` |
|      - | 2066 | ` * The natural-order comparison core lives OUTSIDE the PH7_DISABLE_BUILTIN_FUNC` |
|      - | 2067 | ` * guard: hashmap.c's SORT_NATURAL path (always compiled) calls PH7_StrNatCmp, so` |
|      - | 2068 | ` * it must exist in the tiny build too. [[tiny-build-disk-io-guard-fragility]]` |
|      - | 2069 | ` */` |
|      - | 2070 | `/*` |
|      - | 2071 | ` * Natural-order comparison core (Martin Pool's natcompare as adapted by php's` |
|      - | 2072 | ` * ext/standard/strnatcmp.c): digit runs compare numerically — the longer run` |
|      - | 2073 | ` * wins, a leading zero flips to fractional first-difference-wins semantics —` |
|      - | 2074 | ` * everything else compares bytewise with whitespace skipped.` |
|      - | 2075 | ` */` |
|     42 | 2076 | `static int StrNatCompareRight(const char **pa,const char *aEnd,const char **pb,const char *bEnd)` |
|      1 | 2077 | `{` |
|     43 | 2078 | `	int bias = 0;` |
|     71 | 2079 | `	for(;;){` |
|     93 | 2080 | `		int da = (*pa < aEnd) && SyisDigit(**pa);` |
|     93 | 2081 | `		int db = (*pb < bEnd) && SyisDigit(**pb);` |
|     93 | 2082 | `		if( !da && !db ){ return bias; }` |
|     73 | 2083 | `		if( !da ){ return -1; }` |
|     59 | 2084 | `		if( !db ){ return 1; }` |
|     51 | 2085 | `		if( **pa < **pb ){ if( !bias ){ bias = -1; } }` |
|     39 | 2086 | `		else if( **pa > **pb ){ if( !bias ){ bias = 1; } }` |
|     51 | 2087 | `		(*pa)++;` |
|     51 | 2088 | `		(*pb)++;` |
|      1 | 2089 | `	}` |
|     22 | 2090 | `}` |
|      4 | 2091 | `static int StrNatCompareLeft(const char **pa,const char *aEnd,const char **pb,const char *bEnd)` |
|      1 | 2092 | `{` |
|      2 | 2093 | `	for(;;){` |
|      5 | 2094 | `		int da = (*pa < aEnd) && SyisDigit(**pa);` |
|      5 | 2095 | `		int db = (*pb < bEnd) && SyisDigit(**pb);` |
|      5 | 2096 | `		if( !da && !db ){ return 0; }` |
|      5 | 2097 | `		if( !da ){ return -1; }` |
|      5 | 2098 | `		if( !db ){ return 1; }` |
|      5 | 2099 | `		if( **pa < **pb ){ return -1; }` |
|    ! 0 | 2100 | `		if( **pa > **pb ){ return 1; }` |
|    ! 0 | 2101 | `		(*pa)++;` |
|    ! 0 | 2102 | `		(*pb)++;` |
|    ! 0 | 2103 | `	}` |
|      3 | 2104 | `}` |
|     48 | 2105 | `PH7_PRIVATE int PH7_StrNatCmp(const char *zA,int nA,const char *zB,int nB,int bFold)` |
|      1 | 2106 | `{` |
|     49 | 2107 | `	const char *a = zA,*aEnd = &zA[nA];` |
|     49 | 2108 | `	const char *b = zB,*bEnd = &zB[nB];` |
|    146 | 2109 | `	for(;;){` |
|      - | 2110 | `		int ca,cb;` |
|    175 | 2111 | `		while( a < aEnd && SyisSpace(a[0]) ){ a++; }` |
|    173 | 2112 | `		while( b < bEnd && SyisSpace(b[0]) ){ b++; }` |
|    173 | 2113 | `		ca = (a < aEnd) ? (unsigned char)a[0] : 0;` |
|    173 | 2114 | `		cb = (b < bEnd) ? (unsigned char)b[0] : 0;` |
|    173 | 2115 | `		if( SyisDigit(ca) && SyisDigit(cb) ){` |
|     45 | 2116 | `			int r = (ca == '0' \|\| cb == '0')` |
|      4 | 2117 | `				? StrNatCompareLeft(&a,aEnd,&b,bEnd)` |
|     65 | 2118 | `				: StrNatCompareRight(&a,aEnd,&b,bEnd);` |
|     47 | 2119 | `			if( r ){ return r; }` |
|      5 | 2120 | `			continue;` |
|      - | 2121 | `		}` |
|    127 | 2122 | `		if( ca == 0 && cb == 0 ){ return 0; }` |
|    121 | 2123 | `		if( bFold ){` |
|     67 | 2124 | `			ca = SyToLower(ca);` |
|     67 | 2125 | `			cb = SyToLower(cb);` |
|     33 | 2126 | `		}` |
|    121 | 2127 | `		if( ca < cb ){ return -1; }` |
|    121 | 2128 | `		if( ca > cb ){ return 1; }` |
|    121 | 2129 | `		a++;` |
|    121 | 2130 | `		b++;` |
|      1 | 2131 | `	}` |
|     25 | 2132 | `}` |
|      - | 2133 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 2134 | `/*` |
|      - | 2135 | ` * int strnatcmp(string $string1, string $string2)` |
|      - | 2136 | ` * int strnatcasecmp(string $string1, string $string2)` |
|      - | 2137 | ` *  Natural-order string comparison ("img2" < "img10"), case folded for the` |
|      - | 2138 | ` *  latter. php 8.2+ normalizes the result to -1/0/1.` |
|      - | 2139 | ` */` |
|     20 | 2140 | `static int PH7_builtin_strnatcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2141 | `{` |
|      - | 2142 | `	const char *z1,*z2,*zFunc;` |
|      - | 2143 | `	int n1,n2,bFold;` |
|     21 | 2144 | `	if( nArg < 2 ){` |
|    ! 0 | 2145 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2146 | `		return PH7_OK;` |
|      - | 2147 | `	}` |
|     21 | 2148 | `	zFunc = ph7_function_name(pCtx);` |
|     21 | 2149 | `	bFold = zFunc[sizeof("strnat")-1] == 'c'; /* strnatCasecmp */` |
|     21 | 2150 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     21 | 2151 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     21 | 2152 | `	ph7_result_int(pCtx,PH7_StrNatCmp(z1,n1,z2,n2,bFold));` |
|     21 | 2153 | `	return PH7_OK;` |
|     11 | 2154 | `}` |
|      - | 2155 | `/*` |
|      - | 2156 | ` * int strncmp(string $str1,string $str2,int n)` |
|      - | 2157 | ` *  Perform a binary safe string comparison of the first n characters.` |
|      - | 2158 | ` * Parameter` |
|      - | 2159 | ` *  str1: The first string` |
|      - | 2160 | ` *  str2: The second string` |
|      - | 2161 | ` * Return` |
|      - | 2162 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2163 | ` *  than str2, and 0 if they are equal.` |
|      - | 2164 | ` */` |
|    366 | 2165 | `static int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2166 | `{` |
|      - | 2167 | `	const char *z1,*z2;` |
|      - | 2168 | `	int res;` |
|      - | 2169 | `	int n;` |
|    368 | 2170 | `	if( nArg < 3 ){` |
|      - | 2171 | `		/* Perform a standard comparison */` |
|    ! 0 | 2172 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|      - | 2173 | `	}` |
|      - | 2174 | `	/* Desired comparison length */` |
|    368 | 2175 | `	n  = ph7_value_to_int(apArg[2]);` |
|    368 | 2176 | `	if( n < 0 ){` |
|      - | 2177 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|      4 | 2178 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2179 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|      1 | 2180 | `			ph7_function_name(pCtx));` |
|      - | 2181 | `	}` |
|      - | 2182 | `	/* Perform the comparison */` |
|    366 | 2183 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|    366 | 2184 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|    366 | 2185 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|      - | 2186 | `	/* Comparison result */` |
|    366 | 2187 | `	ph7_result_int(pCtx,res);` |
|    366 | 2188 | `	return PH7_OK;` |
|    185 | 2189 | `}` |
|      - | 2190 | `/*` |
|      - | 2191 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|      - | 2192 | ` *  Perform a binary safe case-insensitive string comparison.` |
|      - | 2193 | ` * Parameter` |
|      - | 2194 | ` *  str1: The first string` |
|      - | 2195 | ` *  str2: The second string` |
|      - | 2196 | ` * Return` |
|      - | 2197 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2198 | ` *  than str2, and 0 if they are equal.` |
|      - | 2199 | ` */` |
|    152 | 2200 | `static int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2201 | `{` |
|      - | 2202 | `	const char *z1,*z2;` |
|      - | 2203 | `	int n1,n2;` |
|      - | 2204 | `	int res;` |
|    153 | 2205 | `	if( nArg < 2 ){` |
|    ! 0 | 2206 | `		res = nArg == 0 ? 0 : 1;` |
|    ! 0 | 2207 | `		ph7_result_int(pCtx,res);` |
|    ! 0 | 2208 | `		return PH7_OK;` |
|      - | 2209 | `	}` |
|      - | 2210 | `	/* Perform the comparison */` |
|    153 | 2211 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|    153 | 2212 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|    153 | 2213 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2214 | `	/* Comparison result */` |
|    153 | 2215 | `	ph7_result_int(pCtx,res);` |
|    153 | 2216 | `	return PH7_OK;` |
|     77 | 2217 | `}` |
|      - | 2218 | `/*` |
|      - | 2219 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|      - | 2220 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|      - | 2221 | ` * Parameter` |
|      - | 2222 | ` *  $str1: The first string` |
|      - | 2223 | ` *  $str2: The second string` |
|      - | 2224 | ` *  $len:  The length of strings to be used in the comparison.` |
|      - | 2225 | ` * Return` |
|      - | 2226 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2227 | ` *  than str2, and 0 if they are equal.` |
|      - | 2228 | ` */` |
|     42 | 2229 | `static int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2230 | `{` |
|      - | 2231 | `	const char *z1,*z2;` |
|      - | 2232 | `	int res;` |
|      - | 2233 | `	int n;` |
|     47 | 2234 | `	if( nArg < 3 ){` |
|      - | 2235 | `		/* Perform a standard comparison */` |
|    ! 0 | 2236 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 2237 | `	}` |
|      - | 2238 | `	/* Desired comparison length */` |
|     47 | 2239 | `	n  = ph7_value_to_int(apArg[2]);` |
|     47 | 2240 | `	if( n < 0 ){` |
|      - | 2241 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|      4 | 2242 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2243 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|      1 | 2244 | `			ph7_function_name(pCtx));` |
|      - | 2245 | `	}` |
|      - | 2246 | `	/* Perform the comparison */` |
|     45 | 2247 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     45 | 2248 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     45 | 2249 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 2250 | `	/* Comparison result */` |
|     45 | 2251 | `	ph7_result_int(pCtx,res);` |
|     45 | 2252 | `	return PH7_OK;` |
|     26 | 2253 | `}` |
|      - | 2254 | `/*` |
|      - | 2255 | ` * Implode context [i.e: it's private data].` |
|      - | 2256 | ` * A pointer to the following structure is forwarded` |
|      - | 2257 | ` * verbatim to the array walker callback defined below.` |
|      - | 2258 | ` */` |
|      - | 2259 | `struct implode_data {` |
|      - | 2260 | `	ph7_context *pCtx;    /* Call context */` |
|      - | 2261 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|      - | 2262 | `	const char *zSep;     /* Arguments separator if any */` |
|      - | 2263 | `	int nSeplen;          /* Separator length */` |
|      - | 2264 | `	int bFirst;           /* TRUE if first call */` |
|      - | 2265 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|      - | 2266 | `	sxi32 rc;             /* Captured allocation rc; SXERR_MEM => the builtin raises an OOM fatal */` |
|      - | 2267 | `};` |
|      - | 2268 | `/*` |
|      - | 2269 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 2270 | ` * The following routine is invoked for each array entry passed` |
|      - | 2271 | ` * to the implode() function.` |
|      - | 2272 | ` */` |
| 157610 | 2273 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      5 | 2274 | `{` |
|  78805 | 2275 | `	SXUNUSED(pKey);` |
| 157615 | 2276 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 2277 | `	const char *zData;` |
|      - | 2278 | `	int nLen;` |
| 157615 | 2279 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 2280 | `		if( pData->nSeplen > 0 ){` |
|      3 | 2281 | `			if( !pData->bFirst ){` |
|      - | 2282 | `				/* append the separator first */` |
|      3 | 2283 | `				if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 2284 | `					pData->rc = SXERR_MEM;` |
|    ! 0 | 2285 | `					return PH7_ABORT;` |
|      - | 2286 | `				}` |
|      2 | 2287 | `			}else{` |
|    ! 0 | 2288 | `				pData->bFirst = 0;` |
|      - | 2289 | `			}` |
|      1 | 2290 | `		}` |
|      - | 2291 | `		/* Recurse */` |
|      3 | 2292 | `		pData->bFirst = 1;` |
|      3 | 2293 | `		pData->nRecCount++;` |
|      3 | 2294 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 2295 | `		pData->nRecCount--;` |
|      - | 2296 | `		/* Propagate an allocation failure surfaced deeper in the recursion. */` |
|      3 | 2297 | `		if( pData->rc != SXRET_OK ){` |
|    ! 0 | 2298 | `			return PH7_ABORT;` |
|      - | 2299 | `		}` |
|      3 | 2300 | `		return PH7_OK;` |
|      - | 2301 | `	}` |
|      - | 2302 | `	/* Extract the string representation of the entry value */` |
| 157613 | 2303 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 2304 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 157613 | 2305 | `	if( pData->bFirst ){` |
|  34157 | 2306 | `		pData->bFirst = 0;` |
| 140537 | 2307 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 2308 | `		/* append the separator first */` |
| 123381 | 2309 | `		if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 2310 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 2311 | `			return PH7_ABORT;` |
|      - | 2312 | `		}` |
|  61688 | 2313 | `	}` |
|      - | 2314 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 157613 | 2315 | `	if( nLen > 0 ){` |
| 144773 | 2316 | `		if( ph7_result_string(pData->pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2317 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 2318 | `			return PH7_ABORT;` |
|      - | 2319 | `		}` |
|  72384 | 2320 | `	}` |
| 157613 | 2321 | `	return PH7_OK;` |
|  78810 | 2322 | `}` |
|      - | 2323 | `/*` |
|      - | 2324 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 2325 | ` * string implode(array $pieces,...)` |
|      - | 2326 | ` *  Join array elements with a string.` |
|      - | 2327 | ` * $glue` |
|      - | 2328 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 2329 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 2330 | ` * $pieces` |
|      - | 2331 | ` *   The array of strings to implode.` |
|      - | 2332 | ` * Return` |
|      - | 2333 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 2334 | ` *  order, with the glue string between each element.` |
|      - | 2335 | ` */` |
|  34180 | 2336 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2337 | `{` |
|      - | 2338 | `	struct implode_data imp_data;` |
|  34185 | 2339 | `	int i = 1;` |
|  34185 | 2340 | `	if( nArg < 1 ){` |
|      - | 2341 | `		/* Missing argument,return NULL */` |
|    ! 0 | 2342 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2343 | `		return PH7_OK;` |
|      - | 2344 | `	}` |
|      - | 2345 | `	/* Prepare the implode context */` |
|  34185 | 2346 | `	imp_data.pCtx = pCtx;` |
|  34185 | 2347 | `	imp_data.bRecursive = 0;` |
|  34185 | 2348 | `	imp_data.bFirst = 1;` |
|  34185 | 2349 | `	imp_data.nRecCount = 0;` |
|  34185 | 2350 | `	imp_data.rc = SXRET_OK;` |
|  34185 | 2351 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  34183 | 2352 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  34183 | 2353 | `		if( nArg > 1 && !ph7_value_is_array(apArg[1]) && !ph7_value_is_null(apArg[1]) ){` |
|      - | 2354 | `			/* php: implode($glue, $pieces) requires an ARRAY. PH7 stringified whatever it` |
|      - | 2355 | `			 * was handed, so implode(",", 5) quietly returned "5". */` |
|      - | 2356 | `			char zBuf[64];` |
|      4 | 2357 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 2358 | `				"implode(): Argument #2 ($array) must be of type ?array, %s given",` |
|      2 | 2359 | `				VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 2360 | `		}` |
|  17093 | 2361 | `	}else{` |
|      3 | 2362 | `		imp_data.zSep = 0;` |
|      3 | 2363 | `		imp_data.nSeplen = 0;` |
|      3 | 2364 | `		i = 0;` |
|      - | 2365 | `	}` |
|  34183 | 2366 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 2367 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2368 | `	}` |
|      - | 2369 | `	/* Start the 'join' process */` |
|  68361 | 2370 | `	while( i < nArg ){` |
|  34183 | 2371 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2372 | `			/* Iterate throw array entries */` |
|  34183 | 2373 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 2374 | `			/* Surface a callback allocation failure as a fatal */` |
|  34183 | 2375 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 2376 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2377 | `			}` |
|  17094 | 2378 | `		}else{` |
|      - | 2379 | `			const char *zData;` |
|      - | 2380 | `			int nLen;` |
|      - | 2381 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 2382 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2383 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 2384 | `			if( imp_data.bFirst ){` |
|    ! 0 | 2385 | `				imp_data.bFirst = 0;` |
|    ! 0 | 2386 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2387 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 2388 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2389 | `				}` |
|    ! 0 | 2390 | `			}` |
|      - | 2391 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 2392 | `			if( nLen > 0 ){` |
|    ! 0 | 2393 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2394 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2395 | `				}` |
|    ! 0 | 2396 | `			}` |
|      - | 2397 | `		}` |
|  34183 | 2398 | `		i++;` |
|      5 | 2399 | `	}` |
|  34183 | 2400 | `	return PH7_OK;` |
|  17095 | 2401 | `}` |
|      - | 2402 | `/*` |
|      - | 2403 | ` * Symisc eXtension:` |
|      - | 2404 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 2405 | ` * Purpose` |
|      - | 2406 | ` *  Same as implode() but recurse on arrays.` |
|      - | 2407 | ` * Example:` |
|      - | 2408 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 2409 | ` *   echo implode_recursive("/",$a);` |
|      - | 2410 | ` *   Will output` |
|      - | 2411 | ` *     usr/home/dean.` |
|      - | 2412 | ` *   While the standard implode would produce.` |
|      - | 2413 | ` *    usr/Array.` |
|      - | 2414 | ` * Parameter` |
|      - | 2415 | ` *  Refer to implode().` |
|      - | 2416 | ` * Return` |
|      - | 2417 | ` *  Refer to implode().` |
|      - | 2418 | ` */` |
|     12 | 2419 | `static int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2420 | `{` |
|      - | 2421 | `	struct implode_data imp_data;` |
|     13 | 2422 | `	int i = 1;` |
|     13 | 2423 | `	if( nArg < 1 ){` |
|      - | 2424 | `		/* Missing argument,return NULL */` |
|      3 | 2425 | `		ph7_result_null(pCtx);` |
|      3 | 2426 | `		return PH7_OK;` |
|      - | 2427 | `	}` |
|      - | 2428 | `	/* Prepare the implode context */` |
|     11 | 2429 | `	imp_data.pCtx = pCtx;` |
|     11 | 2430 | `	imp_data.bRecursive = 1;` |
|     11 | 2431 | `	imp_data.bFirst = 1;` |
|     11 | 2432 | `	imp_data.nRecCount = 0;` |
|     11 | 2433 | `	imp_data.rc = SXRET_OK;` |
|     11 | 2434 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 2435 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 2436 | `	}else{` |
|    ! 0 | 2437 | `		imp_data.zSep = 0;` |
|    ! 0 | 2438 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 2439 | `		i = 0;` |
|      - | 2440 | `	}` |
|     11 | 2441 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 2442 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2443 | `	}` |
|      - | 2444 | `	/* Start the 'join' process */` |
|     21 | 2445 | `	while( i < nArg ){` |
|     11 | 2446 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2447 | `			/* Iterate throw array entries */` |
|      3 | 2448 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 2449 | `			/* Surface a callback allocation failure as a fatal */` |
|      3 | 2450 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 2451 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2452 | `			}` |
|      2 | 2453 | `		}else{` |
|      - | 2454 | `			const char *zData;` |
|      - | 2455 | `			int nLen;` |
|      - | 2456 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 2457 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2458 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 2459 | `			if( imp_data.bFirst ){` |
|      9 | 2460 | `				imp_data.bFirst = 0;` |
|      4 | 2461 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2462 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 2463 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2464 | `				}` |
|    ! 0 | 2465 | `			}` |
|      - | 2466 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 2467 | `			if( nLen > 0 ){` |
|      9 | 2468 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2469 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2470 | `				}` |
|      4 | 2471 | `			}` |
|      - | 2472 | `		}` |
|     11 | 2473 | `		i++;` |
|      1 | 2474 | `	}` |
|     11 | 2475 | `	return PH7_OK;` |
|      7 | 2476 | `}` |
|      - | 2477 | `/*` |
|      - | 2478 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 2479 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 2480 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 2481 | ` * Parameters` |
|      - | 2482 | ` *  $delimiter` |
|      - | 2483 | ` *   The boundary string.` |
|      - | 2484 | ` * $string` |
|      - | 2485 | ` *   The input string.` |
|      - | 2486 | ` * $limit` |
|      - | 2487 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 2488 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 2489 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 2490 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 2491 | ` * Returns` |
|      - | 2492 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 2493 | ` *  on boundaries formed by the delimiter.` |
|      - | 2494 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 2495 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 2496 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 2497 | ` *  will be returned.` |
|      - | 2498 | ` * NOTE:` |
|      - | 2499 | ` *  Negative limit is not supported.` |
|      - | 2500 | ` */` |
|   6978 | 2501 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2502 | `{` |
|      - | 2503 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 2504 | `	int nDelim,nStrlen,iLimit;` |
|      - | 2505 | `	ph7_value *pArray;` |
|      - | 2506 | `	ph7_value *pValue;` |
|      - | 2507 | `	sxu32 nOfft;` |
|      - | 2508 | `	sxi32 rc;` |
|   6983 | 2509 | `	if( nArg < 2 ){` |
|      - | 2510 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2511 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2512 | `		return PH7_OK;` |
|      - | 2513 | `	}` |
|      - | 2514 | `	/* Extract the delimiter */` |
|   6983 | 2515 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   6983 | 2516 | `	if( nDelim < 1 ){` |
|      - | 2517 | `		/* Empty delimiter: PHP 8 throws a catchable ValueError. */` |
|      5 | 2518 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2519 | `			"explode(): Argument #1 ($separator) must not be empty");` |
|      - | 2520 | `	}` |
|      - | 2521 | `	/* Extract the string */` |
|   6979 | 2522 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   6979 | 2523 | `	if( nStrlen < 1 ){` |
|      - | 2524 | `		/* Empty string: normally an array with a single empty element (PHP behavior).` |
|      - | 2525 | `		 * A negative limit drops the last -limit components, so the sole empty` |
|      - | 2526 | `		 * component is dropped and the result is an empty array. */` |
|     13 | 2527 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|     13 | 2528 | `		if( pArrayTmp == 0 ){` |
|      - | 2529 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 2530 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2531 | `			return PH7_OK;` |
|      - | 2532 | `		}` |
|     13 | 2533 | `		if( !(nArg > 2 && ph7_value_to_int(apArg[2]) < 0) ){` |
|     11 | 2534 | `			ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|     11 | 2535 | `			if( pValueTmp == 0 ){` |
|      - | 2536 | `				/* Out of memory,return FALSE */` |
|    ! 0 | 2537 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 2538 | `				return PH7_OK;` |
|      - | 2539 | `			}` |
|     11 | 2540 | `			ph7_value_string(pValueTmp, "", 0);` |
|     11 | 2541 | `			if( ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp) != SXRET_OK ){` |
|    ! 0 | 2542 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2543 | `			}` |
|      5 | 2544 | `		}` |
|     13 | 2545 | `		ph7_result_value(pCtx, pArrayTmp);` |
|     13 | 2546 | `		return PH7_OK;` |
|      - | 2547 | `	}` |
|      - | 2548 | `	/* Point to the end of the string */` |
|   6967 | 2549 | `	zEnd = &zString[nStrlen];` |
|      - | 2550 | `	/* Create the array */` |
|   6967 | 2551 | `	pArray =  ph7_context_new_array(pCtx);` |
|   6967 | 2552 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   6967 | 2553 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 2554 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 2555 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2556 | `		return PH7_OK;` |
|      - | 2557 | `	}` |
|      - | 2558 | `	/* Set a defualt limit */` |
|   6967 | 2559 | `	iLimit = SXI32_HIGH;` |
|   6967 | 2560 | `	if( nArg > 2 ){` |
|     40 | 2561 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|     40 | 2562 | `		if( iLimit < 0 ){` |
|      - | 2563 | `			/* Negative limit: keep all components except the last -iLimit (PHP).` |
|      - | 2564 | `			 * Pre-count the components (delimiters + 1), then emit only the first` |
|      - | 2565 | `			 * nKeep CLEAN components — no trailing-remainder merge (the difference` |
|      - | 2566 | `			 * from the positive path). nKeep <= 0 drops everything -> empty array. */` |
|     17 | 2567 | `			int nTotal = 1,nKeep;` |
|     17 | 2568 | `			const char *zScan = zString;` |
|      - | 2569 | `			sxu32 nScanOfft;` |
|     57 | 2570 | `			while( SyBlobSearch(zScan,(sxu32)(zEnd - zScan),zDelim,nDelim,&nScanOfft) == SXRET_OK ){` |
|     41 | 2571 | `				nTotal++;` |
|     41 | 2572 | `				zScan = &zScan[nScanOfft + nDelim];` |
|      1 | 2573 | `			}` |
|     17 | 2574 | `			nKeep = nTotal + iLimit; /* iLimit < 0, so this is nTotal - (-iLimit) */` |
|     49 | 2575 | `			while( nKeep > (int)ph7_array_count(pArray)` |
|     39 | 2576 | `				&& SyBlobSearch(zString,(sxu32)(zEnd - zString),zDelim,nDelim,&nOfft) == SXRET_OK ){` |
|      - | 2577 | `				/* Emit the next clean component */` |
|     23 | 2578 | `				zCur = &zString[nOfft];` |
|     23 | 2579 | `				ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|     23 | 2580 | `				if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2581 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2582 | `				}` |
|     23 | 2583 | `				zString = &zCur[nDelim];` |
|     23 | 2584 | `				ph7_value_reset_string_cursor(pValue);` |
|      1 | 2585 | `			}` |
|     17 | 2586 | `			ph7_result_value(pCtx,pArray);` |
|     17 | 2587 | `			return PH7_OK;` |
|      - | 2588 | `		}` |
|     24 | 2589 | `		if( iLimit == 0 ){` |
|      5 | 2590 | `			iLimit = 1;` |
|      2 | 2591 | `		}` |
|     24 | 2592 | `		iLimit--;` |
|     10 | 2593 | `	}` |
|      - | 2594 | `	/* Start exploding */` |
|  83865 | 2595 | `	for(;;){` |
| 167735 | 2596 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 167735 | 2597 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 2598 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   6951 | 2599 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   6951 | 2600 | `			if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2601 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2602 | `			}` |
|   6951 | 2603 | `			break;` |
|      - | 2604 | `		}` |
|      - | 2605 | `		/* Point to the desired offset */` |
| 160789 | 2606 | `		zCur = &zString[nOfft];` |
|      - | 2607 | `		/* Perform the store operation (may be empty) */` |
| 160789 | 2608 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 160789 | 2609 | `		if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2610 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 2611 | `		}` |
|      - | 2612 | `		/* Point beyond the delimiter */` |
| 160789 | 2613 | `		zString = &zCur[nDelim];` |
|      - | 2614 | `		/* Reset the cursor */` |
| 160789 | 2615 | `		ph7_value_reset_string_cursor(pValue);` |
|      5 | 2616 | `	}` |
|      - | 2617 | `	/* Return the freshly created array */` |
|   6951 | 2618 | `	ph7_result_value(pCtx,pArray);` |
|      - | 2619 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 2620 | `	 * released as soon we return from this foregin function.` |
|      - | 2621 | `	 */` |
|   6951 | 2622 | `	return PH7_OK;` |
|   3494 | 2623 | `}` |
|      - | 2624 | `/*` |
|      - | 2625 | ` * string trim(string $str[,string $charlist ])` |
|      - | 2626 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2627 | ` * Parameters` |
|      - | 2628 | ` *  $str` |
|      - | 2629 | ` *   The string that will be trimmed.` |
|      - | 2630 | ` * $charlist` |
|      - | 2631 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2632 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2633 | ` *   With .. you can specify a range of characters.` |
|      - | 2634 | ` * Returns.` |
|      - | 2635 | ` *  Thr processed string.` |
|      - | 2636 | ` * NOTE:` |
|      - | 2637 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2638 | ` */` |
|  14718 | 2639 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2640 | `{` |
|  14723 | 2641 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"trim",1,"$string"); }` |
|      - | 2642 | `	const char *zString;` |
|      - | 2643 | `	int nLen;` |
|  14723 | 2644 | `	if( nArg < 1 ){` |
|      - | 2645 | `		/* Missing arguments,return null */` |
|    ! 0 | 2646 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2647 | `		return PH7_OK;` |
|      - | 2648 | `	}` |
|      - | 2649 | `	/* Extract the target string */` |
|  14723 | 2650 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  14723 | 2651 | `	if( nLen < 1 ){` |
|      - | 2652 | `		/* Empty string,return */` |
|    759 | 2653 | `		ph7_result_string(pCtx,"",0);` |
|    759 | 2654 | `		return PH7_OK;` |
|      - | 2655 | `	}` |
|      - | 2656 | `	/* Start the trim process */` |
|  13969 | 2657 | `	if( nArg < 2 ){` |
|      - | 2658 | `		SyString sStr;` |
|      - | 2659 | `		/* Remove white spaces and NUL bytes */` |
|  13939 | 2660 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  34999 | 2661 | `		SyStringFullTrimSafe(&sStr);` |
|  13939 | 2662 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   6972 | 2663 | `	}else{` |
|      - | 2664 | `		/* Char list */` |
|      - | 2665 | `		const char *zList;` |
|      - | 2666 | `		int nListlen;` |
|     33 | 2667 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     33 | 2668 | `		if( nListlen < 1 ){` |
|      - | 2669 | `			/* Return the string unchanged */` |
|      6 | 2670 | `			ph7_result_string(pCtx,zString,nLen);` |
|      4 | 2671 | `		}else{` |
|      - | 2672 | `			char aMask[256];` |
|     29 | 2673 | `			const char *zEnd = &zString[nLen];` |
|     29 | 2674 | `			const char *zCur = zString;` |
|     29 | 2675 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2676 | `			/* Left trim */` |
|     79 | 2677 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     53 | 2678 | `				zCur++;` |
|      3 | 2679 | `			}` |
|      - | 2680 | `			/* Right trim */` |
|     79 | 2681 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     53 | 2682 | `				zEnd--;` |
|      3 | 2683 | `			}` |
|     29 | 2684 | `			if( zCur >= zEnd ){` |
|      - | 2685 | `				/* Return the empty string */` |
|    ! 0 | 2686 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2687 | `			}else{` |
|     29 | 2688 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2689 | `			}` |
|      - | 2690 | `		}` |
|      - | 2691 | `	}` |
|  13969 | 2692 | `	return PH7_OK;` |
|   7364 | 2693 | `}` |
|      - | 2694 | `/*` |
|      - | 2695 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 2696 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 2697 | ` * Parameters` |
|      - | 2698 | ` *  $str` |
|      - | 2699 | ` *   The string that will be trimmed.` |
|      - | 2700 | ` * $charlist` |
|      - | 2701 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2702 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2703 | ` *   With .. you can specify a range of characters.` |
|      - | 2704 | ` * Returns.` |
|      - | 2705 | ` *  Thr processed string.` |
|      - | 2706 | ` * NOTE:` |
|      - | 2707 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2708 | ` */` |
|    170 | 2709 | `static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2710 | `{` |
|    174 | 2711 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"rtrim",1,"$string"); }` |
|      - | 2712 | `	const char *zString;` |
|      - | 2713 | `	int nLen;` |
|    174 | 2714 | `	if( nArg < 1 ){` |
|      - | 2715 | `		/* Missing arguments,return null */` |
|    ! 0 | 2716 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2717 | `		return PH7_OK;` |
|      - | 2718 | `	}` |
|      - | 2719 | `	/* Extract the target string */` |
|    174 | 2720 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    174 | 2721 | `	if( nLen < 1 ){` |
|      - | 2722 | `		/* Empty string,return */` |
|      7 | 2723 | `		ph7_result_string(pCtx,"",0);` |
|      7 | 2724 | `		return PH7_OK;` |
|      - | 2725 | `	}` |
|      - | 2726 | `	/* Start the trim process */` |
|    168 | 2727 | `	if( nArg < 2 ){` |
|      - | 2728 | `		SyString sStr;` |
|      - | 2729 | `		/* Remove white spaces and NUL bytes*/` |
|     19 | 2730 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     48 | 2731 | `		SyStringRightTrimSafe(&sStr);` |
|     19 | 2732 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|     10 | 2733 | `	}else{` |
|      - | 2734 | `		/* Char list */` |
|      - | 2735 | `		const char *zList;` |
|      - | 2736 | `		int nListlen;` |
|    150 | 2737 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|    150 | 2738 | `		if( nListlen < 1 ){` |
|      - | 2739 | `			/* Return the string unchanged */` |
|    ! 0 | 2740 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 2741 | `		}else{` |
|      - | 2742 | `			char aMask[256];` |
|    150 | 2743 | `			const char *zEnd = &zString[nLen];` |
|    150 | 2744 | `			const char *zCur = zString;` |
|    150 | 2745 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2746 | `			/* Right trim */` |
|    168 | 2747 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     21 | 2748 | `				zEnd--;` |
|      3 | 2749 | `			}` |
|    150 | 2750 | `			if( zEnd <= zCur ){` |
|      - | 2751 | `				/* Return the empty string */` |
|    ! 0 | 2752 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2753 | `			}else{` |
|    150 | 2754 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2755 | `			}` |
|      - | 2756 | `		}` |
|      - | 2757 | `	}` |
|    168 | 2758 | `	return PH7_OK;` |
|     89 | 2759 | `}` |
|      - | 2760 | `/*` |
|      - | 2761 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 2762 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2763 | ` * Parameters` |
|      - | 2764 | ` *  $str` |
|      - | 2765 | ` *   The string that will be trimmed.` |
|      - | 2766 | ` * $charlist` |
|      - | 2767 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2768 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2769 | ` *   With .. you can specify a range of characters.` |
|      - | 2770 | ` * Returns.` |
|      - | 2771 | ` *  Thr processed string.` |
|      - | 2772 | ` * NOTE:` |
|      - | 2773 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2774 | ` */` |
|     44 | 2775 | `static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2776 | `{` |
|     49 | 2777 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"ltrim",1,"$string"); }` |
|      - | 2778 | `	const char *zString;` |
|      - | 2779 | `	int nLen;` |
|     49 | 2780 | `	if( nArg < 1 ){` |
|      - | 2781 | `		/* Missing arguments,return null */` |
|    ! 0 | 2782 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2783 | `		return PH7_OK;` |
|      - | 2784 | `	}` |
|      - | 2785 | `	/* Extract the target string */` |
|     49 | 2786 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     49 | 2787 | `	if( nLen < 1 ){` |
|      - | 2788 | `		/* Empty string,return */` |
|     23 | 2789 | `		ph7_result_string(pCtx,"",0);` |
|     23 | 2790 | `		return PH7_OK;` |
|      - | 2791 | `	}` |
|      - | 2792 | `	/* Start the trim process */` |
|     30 | 2793 | `	if( nArg < 2 ){` |
|      - | 2794 | `		SyString sStr;` |
|      - | 2795 | `		/* Remove white spaces and NUL byte */` |
|      5 | 2796 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     13 | 2797 | `		SyStringLeftTrimSafe(&sStr);` |
|      5 | 2798 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      3 | 2799 | `	}else{` |
|      - | 2800 | `		/* Char list */` |
|      - | 2801 | `		const char *zList;` |
|      - | 2802 | `		int nListlen;` |
|     26 | 2803 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     26 | 2804 | `		if( nListlen < 1 ){` |
|      - | 2805 | `			/* Return the string unchanged */` |
|      3 | 2806 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2807 | `		}else{` |
|      - | 2808 | `			char aMask[256];` |
|     24 | 2809 | `			const char *zEnd = &zString[nLen];` |
|     24 | 2810 | `			const char *zCur = zString;` |
|     24 | 2811 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2812 | `			/* Left trim */` |
|     62 | 2813 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     42 | 2814 | `				zCur++;` |
|      4 | 2815 | `			}` |
|     24 | 2816 | `			if( zCur >= zEnd ){` |
|      - | 2817 | `				/* Return the empty string */` |
|    ! 0 | 2818 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2819 | `			}else{` |
|     24 | 2820 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2821 | `			}` |
|      - | 2822 | `		}` |
|      - | 2823 | `	}` |
|     30 | 2824 | `	return PH7_OK;` |
|     27 | 2825 | `}` |
|      - | 2826 | `/*` |
|      - | 2827 | ` * string strtolower(string $str)` |
|      - | 2828 | ` *  Make a string lowercase.` |
|      - | 2829 | ` * Parameters` |
|      - | 2830 | ` *  $str` |
|      - | 2831 | ` *   The input string.` |
|      - | 2832 | ` * Returns.` |
|      - | 2833 | ` *  The lowercased string.` |
|      - | 2834 | ` */` |
|  34048 | 2835 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2836 | `{` |
|  34053 | 2837 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtolower",1,"$string"); }` |
|      - | 2838 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2839 | `	int nLen;` |
|  34053 | 2840 | `	if( nArg < 1 ){` |
|      - | 2841 | `		/* Missing arguments,return null */` |
|    ! 0 | 2842 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2843 | `		return PH7_OK;` |
|      - | 2844 | `	}` |
|      - | 2845 | `	/* Extract the target string */` |
|  34053 | 2846 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  34053 | 2847 | `	if( nLen < 1 ){` |
|      - | 2848 | `		/* Empty string,return */` |
|      5 | 2849 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 2850 | `		return PH7_OK;` |
|      - | 2851 | `	}` |
|      - | 2852 | `	/* Perform the requested operation */` |
|  34049 | 2853 | `	zEnd = &zString[nLen];` |
| 107408 | 2854 | `	for(;;){` |
| 214821 | 2855 | `		if( zString >= zEnd ){` |
|      - | 2856 | `			/* No more input,break immediately */` |
|  34049 | 2857 | `			break;` |
|      - | 2858 | `		}` |
| 180777 | 2859 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2860 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2861 | `			zCur = zString;` |
|    ! 0 | 2862 | `			zString++;` |
|    ! 0 | 2863 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2864 | `				zString++;` |
|    ! 0 | 2865 | `			}` |
|      - | 2866 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2867 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2868 | `		}else{` |
| 180777 | 2869 | `			int c = zString[0];` |
| 180777 | 2870 | `			if( SyisUpper(c) ){` |
| 177893 | 2871 | `				c = SyToLower(zString[0]);` |
|  88944 | 2872 | `			}` |
|      - | 2873 | `			/* Append character */` |
| 180777 | 2874 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2875 | `			/* Advance the cursor */` |
| 180777 | 2876 | `			zString++;` |
|      - | 2877 | `		}` |
|      5 | 2878 | `	}` |
|  34049 | 2879 | `	return PH7_OK;` |
|  17029 | 2880 | `}` |
|      - | 2881 | `/*` |
|      - | 2882 | ` * string strtolower(string $str)` |
|      - | 2883 | ` *  Make a string uppercase.` |
|      - | 2884 | ` * Parameters` |
|      - | 2885 | ` *  $str` |
|      - | 2886 | ` *   The input string.` |
|      - | 2887 | ` * Returns.` |
|      - | 2888 | ` *  The uppercased string.` |
|      - | 2889 | ` */` |
|     76 | 2890 | `static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2891 | `{` |
|     80 | 2892 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtoupper",1,"$string"); }` |
|      - | 2893 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2894 | `	int nLen;` |
|     80 | 2895 | `	if( nArg < 1 ){` |
|      - | 2896 | `		/* Missing arguments,return null */` |
|    ! 0 | 2897 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2898 | `		return PH7_OK;` |
|      - | 2899 | `	}` |
|      - | 2900 | `	/* Extract the target string */` |
|     80 | 2901 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     80 | 2902 | `	if( nLen < 1 ){` |
|      - | 2903 | `		/* Empty string,return */` |
|      5 | 2904 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 2905 | `		return PH7_OK;` |
|      - | 2906 | `	}` |
|      - | 2907 | `	/* Perform the requested operation */` |
|     76 | 2908 | `	zEnd = &zString[nLen];` |
|    148 | 2909 | `	for(;;){` |
|    300 | 2910 | `		if( zString >= zEnd ){` |
|      - | 2911 | `			/* No more input,break immediately */` |
|     76 | 2912 | `			break;` |
|      - | 2913 | `		}` |
|    228 | 2914 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2915 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2916 | `			zCur = zString;` |
|    ! 0 | 2917 | `			zString++;` |
|    ! 0 | 2918 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2919 | `				zString++;` |
|    ! 0 | 2920 | `			}` |
|      - | 2921 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2922 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2923 | `		}else{` |
|    228 | 2924 | `			int c = zString[0];` |
|    228 | 2925 | `			if( SyisLower(c) ){` |
|    212 | 2926 | `				c = SyToUpper(zString[0]);` |
|    104 | 2927 | `			}` |
|      - | 2928 | `			/* Append character */` |
|    228 | 2929 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2930 | `			/* Advance the cursor */` |
|    228 | 2931 | `			zString++;` |
|      - | 2932 | `		}` |
|      4 | 2933 | `	}` |
|     76 | 2934 | `	return PH7_OK;` |
|     42 | 2935 | `}` |
|      - | 2936 | `/*` |
|      - | 2937 | ` * string ucfirst(string $str)` |
|      - | 2938 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 2939 | ` *  character is alphabetic.` |
|      - | 2940 | ` * Parameters` |
|      - | 2941 | ` *  $str` |
|      - | 2942 | ` *   The input string.` |
|      - | 2943 | ` * Returns.` |
|      - | 2944 | ` *  The processed string.` |
|      - | 2945 | ` */` |
|      4 | 2946 | `static int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2947 | `{` |
|      - | 2948 | `	const char *zString,*zEnd;` |
|      - | 2949 | `	int nLen,c;` |
|      5 | 2950 | `	if( nArg < 1 ){` |
|      - | 2951 | `		/* Missing arguments,return null */` |
|    ! 0 | 2952 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2953 | `		return PH7_OK;` |
|      - | 2954 | `	}` |
|      - | 2955 | `	/* Extract the target string */` |
|      5 | 2956 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2957 | `	if( nLen < 1 ){` |
|      - | 2958 | `		/* Empty string,return */` |
|      3 | 2959 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2960 | `		return PH7_OK;` |
|      - | 2961 | `	}` |
|      - | 2962 | `	/* Perform the requested operation */` |
|      3 | 2963 | `	zEnd = &zString[nLen];` |
|      3 | 2964 | `	c = zString[0];` |
|      3 | 2965 | `	if( SyisLower(c) ){` |
|      3 | 2966 | `		c = SyToUpper(c);` |
|      1 | 2967 | `	}` |
|      - | 2968 | `	/* Append the first character */` |
|      3 | 2969 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2970 | `	zString++;` |
|      3 | 2971 | `	if( zString < zEnd ){` |
|      - | 2972 | `		/* Append the rest of the input verbatim */` |
|      3 | 2973 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2974 | `	}` |
|      3 | 2975 | `	return PH7_OK;` |
|      3 | 2976 | `}` |
|      - | 2977 | `/*` |
|      - | 2978 | ` * string lcfirst(string $str)` |
|      - | 2979 | ` *  Make a string's first character lowercase.` |
|      - | 2980 | ` * Parameters` |
|      - | 2981 | ` *  $str` |
|      - | 2982 | ` *   The input string.` |
|      - | 2983 | ` * Returns.` |
|      - | 2984 | ` *  The processed string.` |
|      - | 2985 | ` */` |
|      4 | 2986 | `static int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2987 | `{` |
|      - | 2988 | `	const char *zString,*zEnd;` |
|      - | 2989 | `	int nLen,c;` |
|      5 | 2990 | `	if( nArg < 1 ){` |
|      - | 2991 | `		/* Missing arguments,return null */` |
|    ! 0 | 2992 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2993 | `		return PH7_OK;` |
|      - | 2994 | `	}` |
|      - | 2995 | `	/* Extract the target string */` |
|      5 | 2996 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2997 | `	if( nLen < 1 ){` |
|      - | 2998 | `		/* Empty string,return */` |
|      3 | 2999 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3000 | `		return PH7_OK;` |
|      - | 3001 | `	}` |
|      - | 3002 | `	/* Perform the requested operation */` |
|      3 | 3003 | `	zEnd = &zString[nLen];` |
|      3 | 3004 | `	c = zString[0];` |
|      3 | 3005 | `	if( SyisUpper(c) ){` |
|      3 | 3006 | `		c = SyToLower(c);` |
|      1 | 3007 | `	}` |
|      - | 3008 | `	/* Append the first character */` |
|      3 | 3009 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 3010 | `	zString++;` |
|      3 | 3011 | `	if( zString < zEnd ){` |
|      - | 3012 | `		/* Append the rest of the input verbatim */` |
|      3 | 3013 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 3014 | `	}` |
|      3 | 3015 | `	return PH7_OK;` |
|      3 | 3016 | `}` |
|      - | 3017 | `/*` |
|      - | 3018 | ` * int ord(string $string)` |
|      - | 3019 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 3020 | ` *  Passing null, an empty string, or a multi-byte string emits` |
|      - | 3021 | ` *  E_DEPRECATED to match PHP 8.4+ behaviour.` |
|      - | 3022 | ` * Parameters` |
|      - | 3023 | ` *  $string` |
|      - | 3024 | ` *   The input string.` |
|      - | 3025 | ` * Returns` |
|      - | 3026 | ` *  The ASCII value as an integer.` |
|      - | 3027 | ` */` |
|    226 | 3028 | `static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3029 | `{` |
|      - | 3030 | `	const char *zString;` |
|      - | 3031 | `	int nLen,c;` |
|      - | 3032 | `	/* PHP requires exactly one argument. */` |
|    229 | 3033 | `	if( nArg != 1 ){` |
|      4 | 3034 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3035 | `			"ArgumentCountError",` |
|      - | 3036 | `			"ord() expects exactly 1 argument, %d given",` |
|      1 | 3037 | `			nArg` |
|      - | 3038 | `			);` |
|      - | 3039 | `	}` |
|      - | 3040 | `	/* Passing null is deprecated (E_DEPRECATED).  PHP emits this before` |
|      - | 3041 | `	 * the empty-string deprecation, so we check null first. */` |
|    227 | 3042 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 3043 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 3044 | `			"ord(): Passing null to parameter #1 ($character) "` |
|      - | 3045 | `			"of type string is deprecated"` |
|      - | 3046 | `			);` |
|      1 | 3047 | `	}` |
|      - | 3048 | `	/* Extract the target string */` |
|    227 | 3049 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    227 | 3050 | `	if( nLen < 1 ){` |
|      - | 3051 | `		/* Empty string is deprecated (E_DEPRECATED). */` |
|      5 | 3052 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 3053 | `			"ord(): Providing an empty string is deprecated"` |
|      - | 3054 | `			);` |
|      5 | 3055 | `		ph7_result_int(pCtx,0);` |
|      5 | 3056 | `		return PH7_OK;` |
|      - | 3057 | `	}` |
|      - | 3058 | `	/* A string longer than one byte is deprecated (E_DEPRECATED). */` |
|    223 | 3059 | `	if( nLen > 1 ){` |
|      7 | 3060 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 3061 | `			"ord(): Providing a string that is not one byte long "` |
|      - | 3062 | `			"is deprecated. Use ord($str[0]) instead"` |
|      - | 3063 | `			);` |
|      3 | 3064 | `	}` |
|      - | 3065 | `	/* Extract the ASCII value of the first character */` |
|    223 | 3066 | `	c = (unsigned char)zString[0];` |
|      - | 3067 | `	/* Return that value */` |
|    223 | 3068 | `	ph7_result_int(pCtx,c);` |
|    223 | 3069 | `	return PH7_OK;` |
|    116 | 3070 | `}` |
|      - | 3071 | `/*` |
|      - | 3072 | ` * string chr(int $codepoint)` |
|      - | 3073 | ` *  Returns a one-character string containing the character specified` |
|      - | 3074 | ` *  by the given codepoint.  Any integer is accepted; values outside` |
|      - | 3075 | ` *  the [0, 255] range emit an E_DEPRECATED and are masked with & 0xFF.` |
|      - | 3076 | ` * Parameters` |
|      - | 3077 | ` *  $codepoint` |
|      - | 3078 | ` *   An integer codepoint.  Values outside 0-255 are deprecated and` |
|      - | 3079 | ` *   will be constrained to a single byte.` |
|      - | 3080 | ` * Returns` |
|      - | 3081 | ` *  A single-character string.` |
|      - | 3082 | ` */` |
|   7170 | 3083 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3084 | `{` |
|      - | 3085 | `	int c;` |
|      - | 3086 | `	unsigned char ch;` |
|      - | 3087 | `	/* PHP requires exactly one argument. */` |
|   7173 | 3088 | `	if( nArg != 1 ){` |
|      4 | 3089 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3090 | `			"ArgumentCountError",` |
|      - | 3091 | `			"chr() expects exactly 1 argument, %d given",` |
|      1 | 3092 | `			nArg` |
|      - | 3093 | `			);` |
|      - | 3094 | `	}` |
|      - | 3095 | `	/* Implicit float-to-int conversion loses precision (E_DEPRECATED).` |
|      - | 3096 | `	 * PHP does not prefix this message with "chr():", so we call` |
|      - | 3097 | `	 * PH7_VmThrowError() with a NULL function name to avoid the` |
|      - | 3098 | `	 * automatic prefix that ph7_context_throw_error*() would add. */` |
|   7171 | 3099 | `	if( ph7_value_is_float(apArg[0]) ){` |
|      - | 3100 | `		char zBuf[120];` |
|      4 | 3101 | `		SyBufferFormat(zBuf,sizeof(zBuf),` |
|      - | 3102 | `			"Implicit conversion from float %g to int loses precision",` |
|      1 | 3103 | `			ph7_value_to_double(apArg[0])` |
|      - | 3104 | `			);` |
|      3 | 3105 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zBuf);` |
|      1 | 3106 | `	}` |
|      - | 3107 | `	/* Extract the codepoint. */` |
|   7171 | 3108 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 3109 | `	/* Out-of-range codepoint (E_DEPRECATED), then mask to a single byte.` |
|      - | 3110 | `	 * PHP includes "chr(): " in the $errstr passed to set_error_handler,` |
|      - | 3111 | `	 * so we embed the prefix in the message and pass NULL as the function` |
|      - | 3112 | `	 * name to avoid the API double-prefixing it. */` |
|   7171 | 3113 | `	if( c < 0 \|\| c > 255 ){` |
|      5 | 3114 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 3115 | `			E_DEPRECATED,` |
|      - | 3116 | `			"chr(): Providing a value not in-between 0 and 255 is deprecated, "` |
|      - | 3117 | `			"this is because a byte value must be in the [0, 255] interval. "` |
|      - | 3118 | `			"The value used will be constrained using % 256"` |
|      - | 3119 | `			);` |
|      2 | 3120 | `	}` |
|      - | 3121 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|      - | 3122 | `	 * when taking the address of a wider int. */` |
|   7171 | 3123 | `	ch = (unsigned char)(c & 0xFF);` |
|      - | 3124 | `	/* Return the specified character */` |
|   7171 | 3125 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|   7171 | 3126 | `	return PH7_OK;` |
|   3588 | 3127 | `}` |
|      - | 3128 | `/*` |
|      - | 3129 | ` * Binary to hex consumer callback.` |
|      - | 3130 | ` * This callback is the default consumer used by the hash functions` |
|      - | 3131 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 3132 | ` */` |
|   3170 | 3133 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      3 | 3134 | `{` |
|      - | 3135 | `	/* Append hex chunk verbatim */` |
|   3173 | 3136 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   3173 | 3137 | `	return SXRET_OK;` |
|      3 | 3138 | `}` |
|      - | 3139 |  |
|      - | 3140 | `/*` |
|      - | 3141 | ` * string bin2hex(string $str)` |
|      - | 3142 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 3143 | ` * Parameters` |
|      - | 3144 | ` *  $str` |
|      - | 3145 | ` *   The input string.` |
|      - | 3146 | ` * Returns.` |
|      - | 3147 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 3148 | ` */` |
|    152 | 3149 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3150 | `{` |
|      - | 3151 | `	const char *zString;` |
|      - | 3152 | `	int nLen;` |
|      - | 3153 | `	/* PHP 8 requires exactly one argument (ArgumentCountError). */` |
|    155 | 3154 | `	if( nArg != 1 ){` |
|      4 | 3155 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3156 | `			"ArgumentCountError",` |
|      - | 3157 | `			"bin2hex() expects exactly 1 argument, %d given",` |
|      1 | 3158 | `			nArg` |
|      - | 3159 | `			);` |
|      - | 3160 | `	}` |
|      - | 3161 | `	/* In PHP 8, bin2hex() is strict about its parameter type.` |
|      - | 3162 | `	 * Array/Resource values are not allowed and trigger a TypeError.` |
|      - | 3163 | `	 * Objects without __toString() must also raise a TypeError.` |
|      - | 3164 | `	 */` |
|    228 | 3165 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) \|\|` |
|     75 | 3166 | `		( ph7_value_is_object(apArg[0]) &&` |
|    ! 0 | 3167 | `		  ((ph7_class_instance *)apArg[0]->x.pOther) != 0 &&` |
|    ! 0 | 3168 | `		  PH7_ClassExtractMethod(((ph7_class_instance *)apArg[0]->x.pOther)->pClass,` |
|    ! 0 | 3169 | `			"__toString",sizeof("__toString")-1) == 0` |
|      - | 3170 | `		)` |
|      - | 3171 | `	){` |
|    ! 0 | 3172 | `		const char *zType = ph7_type_name(apArg[0]);` |
|    ! 0 | 3173 | `		if( ph7_value_is_object(apArg[0]) ){` |
|    ! 0 | 3174 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    ! 0 | 3175 | `			if( pInst && pInst->pClass ){` |
|    ! 0 | 3176 | `				zType = SyStringData(&pInst->pClass->sName);` |
|    ! 0 | 3177 | `			}` |
|    ! 0 | 3178 | `		}` |
|    ! 0 | 3179 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3180 | `			"TypeError",` |
|      - | 3181 | `			"bin2hex(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 3182 | `			zType` |
|      - | 3183 | `			);` |
|      - | 3184 | `	}` |
|      - | 3185 | `	/* Extract the target string */` |
|    153 | 3186 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    153 | 3187 | `	if( nLen < 1 ){` |
|      - | 3188 | `		/* Empty string,return */` |
|     13 | 3189 | `		ph7_result_string(pCtx,"",0);` |
|     13 | 3190 | `		return PH7_OK;` |
|      - | 3191 | `	}` |
|      - | 3192 | `	/* Perform the requested operation */` |
|    141 | 3193 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|    141 | 3194 | `	return PH7_OK;` |
|     79 | 3195 | `}` |
|      - | 3196 |  |
|      - | 3197 | `/* Search callback signature */` |
|      - | 3198 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 3199 | `/*` |
|      - | 3200 | ` * Case-insensitive pattern match.` |
|      - | 3201 | ` * Brute force is the default search method used here.` |
|      - | 3202 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 3203 | ` * well for short/medium texts on modern hardware.` |
|      - | 3204 | ` */` |
|    298 | 3205 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      2 | 3206 | `{` |
|    300 | 3207 | `	const char *zpIn = (const char *)pPattern;` |
|    300 | 3208 | `	const char *zIn = (const char *)pText;` |
|    300 | 3209 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    300 | 3210 | `	const char *zEnd = &zIn[nLen];` |
|      - | 3211 | `	const char *zPtr,*zPtr2;` |
|      - | 3212 | `	int c,d;` |
|    300 | 3213 | `	if( iPatLen > nLen ){` |
|      - | 3214 | `		/* Don't bother processing */` |
|     67 | 3215 | `		return SXERR_NOTFOUND;` |
|      - | 3216 | `	}` |
|    860 | 3217 | `	for(;;){` |
|   1722 | 3218 | `		if( zIn >= zEnd ){` |
|    194 | 3219 | `			break;` |
|      - | 3220 | `		}` |
|   1530 | 3221 | `		c = SyToLower(zIn[0]);` |
|   1530 | 3222 | `		d = SyToLower(zpIn[0]);` |
|   1530 | 3223 | `		if( c == d ){` |
|    182 | 3224 | `			zPtr   = &zIn[1];` |
|    182 | 3225 | `			zPtr2  = &zpIn[1];` |
|    141 | 3226 | `			for(;;){` |
|    284 | 3227 | `				if( zPtr2 >= zpEnd ){` |
|      - | 3228 | `					/* Pattern found */` |
|     41 | 3229 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 3230 | `					return SXRET_OK;` |
|      - | 3231 | `				}` |
|    244 | 3232 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 3233 | `					break;` |
|      - | 3234 | `				}` |
|    244 | 3235 | `				c = SyToLower(zPtr[0]);` |
|    244 | 3236 | `				d = SyToLower(zPtr2[0]);` |
|    244 | 3237 | `				if( c != d ){` |
|    142 | 3238 | `					break;` |
|      - | 3239 | `				}` |
|    103 | 3240 | `				zPtr++; zPtr2++;` |
|      1 | 3241 | `			}` |
|     70 | 3242 | `		}` |
|   1490 | 3243 | `		zIn++;` |
|      2 | 3244 | `	}` |
|      - | 3245 | `	/* Pattern not found */` |
|    194 | 3246 | `	return SXERR_NOTFOUND;` |
|    151 | 3247 | `}` |
|      - | 3248 | `/*` |
|      - | 3249 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3250 | ` *  Find the first occurrence of a string.` |
|      - | 3251 | ` * Parameters` |
|      - | 3252 | ` *  $haystack` |
|      - | 3253 | ` *   The input string.` |
|      - | 3254 | ` * $needle` |
|      - | 3255 | ` *   Search pattern (must be a string).` |
|      - | 3256 | ` * $before_needle` |
|      - | 3257 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3258 | ` *   of the needle (excluding the needle).` |
|      - | 3259 | ` * Return` |
|      - | 3260 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3261 | ` */` |
|      6 | 3262 | `static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3263 | `{` |
|      7 | 3264 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3265 | `	const char *zBlob,*zPattern;` |
|      - | 3266 | `	int nLen,nPatLen;` |
|      - | 3267 | `	sxu32 nOfft;` |
|      - | 3268 | `	sxi32 rc;` |
|      7 | 3269 | `	if( nArg < 2 ){` |
|      - | 3270 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3271 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3272 | `		return PH7_OK;` |
|      - | 3273 | `	}` |
|      - | 3274 | `	/* Extract the needle and the haystack */` |
|      7 | 3275 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 3276 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 3277 | `	nOfft = 0; /* cc warning */` |
|      9 | 3278 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3279 | `		int before = 0;` |
|      - | 3280 | `		/* Perform the lookup */` |
|      5 | 3281 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3282 | `		if( rc != SXRET_OK ){` |
|      - | 3283 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3284 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3285 | `			return PH7_OK;` |
|      - | 3286 | `		}` |
|      - | 3287 | `		/* Return the portion of the string */` |
|      5 | 3288 | `		if( nArg > 2 ){` |
|      3 | 3289 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3290 | `		}` |
|      5 | 3291 | `		if( before ){` |
|      3 | 3292 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3293 | `		}else{` |
|      3 | 3294 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3295 | `		}` |
|      3 | 3296 | `	}else{` |
|      3 | 3297 | `		ph7_result_bool(pCtx,0);` |
|      - | 3298 | `	}` |
|      7 | 3299 | `	return PH7_OK;` |
|      4 | 3300 | `}` |
|      - | 3301 | `/*` |
|      - | 3302 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3303 | ` *  Case-insensitive strstr().` |
|      - | 3304 | ` * Parameters` |
|      - | 3305 | ` *  $haystack` |
|      - | 3306 | ` *   The input string.` |
|      - | 3307 | ` * $needle` |
|      - | 3308 | ` *   Search pattern (must be a string).` |
|      - | 3309 | ` * $before_needle` |
|      - | 3310 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3311 | ` *   of the needle (excluding the needle).` |
|      - | 3312 | ` * Return` |
|      - | 3313 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3314 | ` */` |
|      4 | 3315 | `static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3316 | `{` |
|      5 | 3317 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3318 | `	const char *zBlob,*zPattern;` |
|      - | 3319 | `	int nLen,nPatLen;` |
|      - | 3320 | `	sxu32 nOfft;` |
|      - | 3321 | `	sxi32 rc;` |
|      5 | 3322 | `	if( nArg < 2 ){` |
|      - | 3323 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3324 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3325 | `		return PH7_OK;` |
|      - | 3326 | `	}` |
|      - | 3327 | `	/* Extract the needle and the haystack */` |
|      5 | 3328 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3329 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      5 | 3330 | `	nOfft = 0; /* cc warning */` |
|      7 | 3331 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3332 | `		int before = 0;` |
|      - | 3333 | `		/* Perform the lookup */` |
|      5 | 3334 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3335 | `		if( rc != SXRET_OK ){` |
|      - | 3336 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3337 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3338 | `			return PH7_OK;` |
|      - | 3339 | `		}` |
|      - | 3340 | `		/* Return the portion of the string */` |
|      5 | 3341 | `		if( nArg > 2 ){` |
|      3 | 3342 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3343 | `		}` |
|      5 | 3344 | `		if( before ){` |
|      3 | 3345 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3346 | `		}else{` |
|      3 | 3347 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3348 | `		}` |
|      3 | 3349 | `	}else{` |
|    ! 0 | 3350 | `		ph7_result_bool(pCtx,0);` |
|      - | 3351 | `	}` |
|      5 | 3352 | `	return PH7_OK;` |
|      3 | 3353 | `}` |
|      - | 3354 | `/*` |
|      - | 3355 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3356 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 3357 | ` * Parameters` |
|      - | 3358 | ` *  $haystack` |
|      - | 3359 | ` *   The input string.` |
|      - | 3360 | ` * $needle` |
|      - | 3361 | ` *   Search pattern (must be a string).` |
|      - | 3362 | ` * $offset` |
|      - | 3363 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3364 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3365 | ` *   of haystack.` |
|      - | 3366 | ` * Return` |
|      - | 3367 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3368 | ` */` |
|   1562 | 3369 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3370 | `{` |
|   1567 | 3371 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strpos",1,"$haystack"); }` |
|   1567 | 3372 | `	if( nArg > 1 ){ StrNullArgNotice(pCtx,apArg[1],"strpos",2,"$needle"); }` |
|   1567 | 3373 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3374 | `	const char *zBlob,*zPattern;` |
|      - | 3375 | `	int nLen,nPatLen,nStart;` |
|      - | 3376 | `	sxu32 nOfft;` |
|      - | 3377 | `	sxi32 rc;` |
|   1567 | 3378 | `	if( nArg < 2 ){` |
|      - | 3379 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3380 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3381 | `		return PH7_OK;` |
|      - | 3382 | `	}` |
|      - | 3383 | `	/* Extract the needle and the haystack */` |
|   1567 | 3384 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|   1567 | 3385 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|   1567 | 3386 | `	nOfft = 0; /* cc warning */` |
|   1567 | 3387 | `	nStart = 0;` |
|      - | 3388 | `	/* Peek the starting offset if available */` |
|   1567 | 3389 | `	if( nArg > 2 ){` |
|     15 | 3390 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3391 | `		if( nStart < 0 ){` |
|    ! 0 | 3392 | `			nStart = -nStart;` |
|    ! 0 | 3393 | `		}` |
|     15 | 3394 | `		if( nStart >= nLen ){` |
|      - | 3395 | `			/* Invalid offset */` |
|    ! 0 | 3396 | `			nStart = 0;` |
|    ! 0 | 3397 | `		}else{` |
|     15 | 3398 | `			zBlob += nStart;` |
|     15 | 3399 | `			nLen -= nStart;` |
|      - | 3400 | `		}` |
|      7 | 3401 | `	}` |
|   1567 | 3402 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3403 | `		/* Perform the lookup */` |
|   1563 | 3404 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|   1563 | 3405 | `		if( rc != SXRET_OK ){` |
|      - | 3406 | `			/* Pattern not found,return FALSE */` |
|    803 | 3407 | `			ph7_result_bool(pCtx,0);` |
|    803 | 3408 | `			return PH7_OK;` |
|      - | 3409 | `		}` |
|      - | 3410 | `		/* Return the pattern position */` |
|    765 | 3411 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|    385 | 3412 | `	}else{` |
|      5 | 3413 | `		ph7_result_bool(pCtx,0);` |
|      - | 3414 | `	}` |
|    769 | 3415 | `	return PH7_OK;` |
|    786 | 3416 | `}` |
|      - | 3417 | `/*` |
|      - | 3418 | ` * Validate and resolve a single string-typed parameter for str_contains/` |
|      - | 3419 | ` * str_starts_with/str_ends_with. Emits an E_DEPRECATED notice for null` |
|      - | 3420 | ` * (matching PHP 8.1+; falls through with an empty string), and throws` |
|      - | 3421 | ` * TypeError for arrays, resources, and objects without __toString.` |
|      - | 3422 | ` *` |
|      - | 3423 | ` * For objects with __toString, invokes the method directly into pTmp and` |
|      - | 3424 | ` * uses its raw byte buffer. This preserves empty results, which the` |
|      - | 3425 | ` * engine's MemObjStringValue otherwise replaces with the literal "Object".` |
|      - | 3426 | ` *` |
|      - | 3427 | ` * On success, pzOut/pnOut point at the resolved byte buffer; the buffer` |
|      - | 3428 | ` * is valid until pTmp is released or pArg is mutated.` |
|      - | 3429 | ` */` |
|    668 | 3430 | `static sxi32 StrPredicateResolveArg(` |
|      - | 3431 | `	ph7_context *pCtx,` |
|      - | 3432 | `	ph7_value *pArg,` |
|      - | 3433 | `	const char *zFunc,` |
|      - | 3434 | `	int iArgNum,` |
|      - | 3435 | `	const char *zParamName,` |
|      - | 3436 | `	const char *zTypeStr, /* Declared type in the TypeError, e.g. "string" / "?string" */` |
|      - | 3437 | `	const char *zNullMsg,` |
|      - | 3438 | `	ph7_value *pTmp,` |
|      - | 3439 | `	const char **pzOut,` |
|      - | 3440 | `	int *pnOut` |
|      2 | 3441 | `){` |
|    670 | 3442 | `	if( ph7_value_is_null(pArg) ){` |
|     13 | 3443 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zNullMsg);` |
|     13 | 3444 | `		*pzOut = "";` |
|     13 | 3445 | `		*pnOut = 0;` |
|     13 | 3446 | `		return PH7_OK;` |
|      - | 3447 | `	}` |
|   1010 | 3448 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_resource(pArg) \|\|` |
|    656 | 3449 | `	    ( ph7_value_is_object(pArg) &&` |
|     72 | 3450 | `	      ((ph7_class_instance *)pArg->x.pOther) != 0 &&` |
|     48 | 3451 | `	      PH7_ClassExtractMethod(((ph7_class_instance *)pArg->x.pOther)->pClass,` |
|     24 | 3452 | `	        "__toString",sizeof("__toString")-1) == 0` |
|      - | 3453 | `	    )` |
|      - | 3454 | `	){` |
|    ! 0 | 3455 | `		const char *zType = ph7_type_name(pArg);` |
|    ! 0 | 3456 | `		if( ph7_value_is_object(pArg) ){` |
|    ! 0 | 3457 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|    ! 0 | 3458 | `			if( pInst && pInst->pClass ){` |
|    ! 0 | 3459 | `				zType = SyStringData(&pInst->pClass->sName);` |
|    ! 0 | 3460 | `			}` |
|    ! 0 | 3461 | `		}` |
|    ! 0 | 3462 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3463 | `			"TypeError",` |
|      - | 3464 | `			"%s(): Argument #%d (%s) must be of type %s, %s given",` |
|    ! 0 | 3465 | `			zFunc, iArgNum, zParamName, zTypeStr, zType` |
|      - | 3466 | `			);` |
|      - | 3467 | `	}` |
|    658 | 3468 | `	if( ph7_value_is_object(pArg) ){` |
|     49 | 3469 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     49 | 3470 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pInst->pClass,` |
|      - | 3471 | `			"__toString",sizeof("__toString")-1);` |
|     49 | 3472 | `		PH7_VmCallClassMethod(pCtx->pVm,pInst,pMethod,pTmp,0,0);` |
|     49 | 3473 | `		*pzOut = (const char *)SyBlobData(&pTmp->sBlob);` |
|     49 | 3474 | `		*pnOut = (int)SyBlobLength(&pTmp->sBlob);` |
|     49 | 3475 | `		return PH7_OK;` |
|      - | 3476 | `	}` |
|    610 | 3477 | `	*pzOut = ph7_value_to_string(pArg,pnOut);` |
|    610 | 3478 | `	return PH7_OK;` |
|    336 | 3479 | `}` |
|      - | 3480 | `/*` |
|      - | 3481 | ` * bool str_contains(string $haystack, string $needle)` |
|      - | 3482 | ` *  Determine if a string contains a given substring (PHP 8.0).` |
|      - | 3483 | ` * Return` |
|      - | 3484 | ` *  TRUE if needle occurs in haystack. An empty needle always returns TRUE.` |
|      - | 3485 | ` */` |
|     92 | 3486 | `static int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3487 | `{` |
|      - | 3488 | `	const char *zHaystack,*zNeedle;` |
|      - | 3489 | `	int nHayLen,nNeedleLen;` |
|      - | 3490 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3491 | `	sxi32 rc;` |
|     95 | 3492 | `	if( nArg != 2 ){` |
|      8 | 3493 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3494 | `			"ArgumentCountError",` |
|      - | 3495 | `			"str_contains() expects exactly 2 arguments, %d given",` |
|      2 | 3496 | `			nArg` |
|      - | 3497 | `			);` |
|      - | 3498 | `	}` |
|     90 | 3499 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     90 | 3500 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     90 | 3501 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_contains",1,"$haystack","string",` |
|      - | 3502 | `		"str_contains(): Passing null to parameter #1 ($haystack) "` |
|      - | 3503 | `		"of type string is deprecated",` |
|      - | 3504 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     90 | 3505 | `	if( rc != PH7_OK ) goto out;` |
|     90 | 3506 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_contains",2,"$needle","string",` |
|      - | 3507 | `		"str_contains(): Passing null to parameter #2 ($needle) "` |
|      - | 3508 | `		"of type string is deprecated",` |
|      - | 3509 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     90 | 3510 | `	if( rc != PH7_OK ) goto out;` |
|     90 | 3511 | `	if( nNeedleLen < 1 ){` |
|     13 | 3512 | `		ph7_result_bool(pCtx,1);` |
|     84 | 3513 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 3514 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3515 | `	}else{` |
|    104 | 3516 | `		sxi32 srch = SyBlobSearch((const void *)zHaystack,(sxu32)nHayLen,` |
|     34 | 3517 | `		                          (const void *)zNeedle,(sxu32)nNeedleLen,0);` |
|     70 | 3518 | `		ph7_result_bool(pCtx,srch == SXRET_OK ? 1 : 0);` |
|      - | 3519 | `	}` |
|     90 | 3520 | `	rc = PH7_OK;` |
|     44 | 3521 | `out:` |
|     90 | 3522 | `	PH7_MemObjRelease(&sHayTmp);` |
|     90 | 3523 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     90 | 3524 | `	return rc;` |
|     49 | 3525 | `}` |
|      - | 3526 | `/*` |
|      - | 3527 | ` * bool str_starts_with(string $haystack, string $needle)` |
|      - | 3528 | ` *  Check if a string starts with a given substring (PHP 8.0).` |
|      - | 3529 | ` * Return` |
|      - | 3530 | ` *  TRUE if haystack begins with needle. An empty needle always returns TRUE.` |
|      - | 3531 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 3532 | ` */` |
|     62 | 3533 | `static int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3534 | `{` |
|      - | 3535 | `	const char *zHaystack,*zNeedle;` |
|      - | 3536 | `	int nHayLen,nNeedleLen;` |
|      - | 3537 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3538 | `	sxi32 rc;` |
|     64 | 3539 | `	if( nArg != 2 ){` |
|      8 | 3540 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3541 | `			"ArgumentCountError",` |
|      - | 3542 | `			"str_starts_with() expects exactly 2 arguments, %d given",` |
|      2 | 3543 | `			nArg` |
|      - | 3544 | `			);` |
|      - | 3545 | `	}` |
|     59 | 3546 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     59 | 3547 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     59 | 3548 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_starts_with",1,"$haystack","string",` |
|      - | 3549 | `		"str_starts_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 3550 | `		"of type string is deprecated",` |
|      - | 3551 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     59 | 3552 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3553 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_starts_with",2,"$needle","string",` |
|      - | 3554 | `		"str_starts_with(): Passing null to parameter #2 ($needle) "` |
|      - | 3555 | `		"of type string is deprecated",` |
|      - | 3556 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     59 | 3557 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3558 | `	if( nNeedleLen < 1 ){` |
|     13 | 3559 | `		ph7_result_bool(pCtx,1);` |
|     53 | 3560 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 3561 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3562 | `	}else{` |
|     58 | 3563 | `		ph7_result_bool(pCtx,` |
|     38 | 3564 | `			SyMemcmp(zHaystack,zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 3565 | `	}` |
|     59 | 3566 | `	rc = PH7_OK;` |
|     29 | 3567 | `out:` |
|     59 | 3568 | `	PH7_MemObjRelease(&sHayTmp);` |
|     59 | 3569 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     59 | 3570 | `	return rc;` |
|     33 | 3571 | `}` |
|      - | 3572 | `/*` |
|      - | 3573 | ` * bool str_ends_with(string $haystack, string $needle)` |
|      - | 3574 | ` *  Check if a string ends with a given substring (PHP 8.0).` |
|      - | 3575 | ` * Return` |
|      - | 3576 | ` *  TRUE if haystack ends with needle. An empty needle always returns TRUE.` |
|      - | 3577 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 3578 | ` */` |
|     62 | 3579 | `static int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3580 | `{` |
|      - | 3581 | `	const char *zHaystack,*zNeedle;` |
|      - | 3582 | `	int nHayLen,nNeedleLen;` |
|      - | 3583 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3584 | `	sxi32 rc;` |
|     64 | 3585 | `	if( nArg != 2 ){` |
|      8 | 3586 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3587 | `			"ArgumentCountError",` |
|      - | 3588 | `			"str_ends_with() expects exactly 2 arguments, %d given",` |
|      2 | 3589 | `			nArg` |
|      - | 3590 | `			);` |
|      - | 3591 | `	}` |
|     59 | 3592 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     59 | 3593 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     59 | 3594 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_ends_with",1,"$haystack","string",` |
|      - | 3595 | `		"str_ends_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 3596 | `		"of type string is deprecated",` |
|      - | 3597 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     59 | 3598 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3599 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_ends_with",2,"$needle","string",` |
|      - | 3600 | `		"str_ends_with(): Passing null to parameter #2 ($needle) "` |
|      - | 3601 | `		"of type string is deprecated",` |
|      - | 3602 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     59 | 3603 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3604 | `	if( nNeedleLen < 1 ){` |
|     13 | 3605 | `		ph7_result_bool(pCtx,1);` |
|     53 | 3606 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 3607 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3608 | `	}else{` |
|     58 | 3609 | `		ph7_result_bool(pCtx,` |
|     38 | 3610 | `			SyMemcmp(zHaystack + (nHayLen - nNeedleLen),zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 3611 | `	}` |
|     59 | 3612 | `	rc = PH7_OK;` |
|     29 | 3613 | `out:` |
|     59 | 3614 | `	PH7_MemObjRelease(&sHayTmp);` |
|     59 | 3615 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     59 | 3616 | `	return rc;` |
|     33 | 3617 | `}` |
|      - | 3618 | `/*` |
|      - | 3619 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3620 | ` *  Case-insensitive strpos.` |
|      - | 3621 | ` * Parameters` |
|      - | 3622 | ` *  $haystack` |
|      - | 3623 | ` *   The input string.` |
|      - | 3624 | ` * $needle` |
|      - | 3625 | ` *   Search pattern (must be a string).` |
|      - | 3626 | ` * $offset` |
|      - | 3627 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3628 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3629 | ` *   of haystack.` |
|      - | 3630 | ` * Return` |
|      - | 3631 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3632 | ` */` |
|    196 | 3633 | `static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3634 | `{` |
|    198 | 3635 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3636 | `	const char *zBlob,*zPattern;` |
|      - | 3637 | `	int nLen,nPatLen,nStart;` |
|      - | 3638 | `	sxu32 nOfft;` |
|      - | 3639 | `	sxi32 rc;` |
|    198 | 3640 | `	if( nArg < 2 ){` |
|      - | 3641 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3642 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3643 | `		return PH7_OK;` |
|      - | 3644 | `	}` |
|      - | 3645 | `	/* Extract the needle and the haystack */` |
|    198 | 3646 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|    198 | 3647 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|    198 | 3648 | `	nOfft = 0; /* cc warning */` |
|    198 | 3649 | `	nStart = 0;` |
|      - | 3650 | `	/* Peek the starting offset if available */` |
|    198 | 3651 | `	if( nArg > 2 ){` |
|      5 | 3652 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 3653 | `		if( nStart < 0 ){` |
|      3 | 3654 | `			nStart = -nStart;` |
|      1 | 3655 | `		}` |
|      5 | 3656 | `		if( nStart >= nLen ){` |
|      - | 3657 | `			/* Invalid offset */` |
|    ! 0 | 3658 | `			nStart = 0;` |
|    ! 0 | 3659 | `		}else{` |
|      5 | 3660 | `			zBlob += nStart;` |
|      5 | 3661 | `			nLen -= nStart;` |
|      - | 3662 | `		}` |
|      2 | 3663 | `	}` |
|    198 | 3664 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3665 | `		/* Perform the lookup */` |
|    198 | 3666 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|    198 | 3667 | `		if( rc != SXRET_OK ){` |
|      - | 3668 | `			/* Pattern not found,return FALSE */` |
|    184 | 3669 | `			ph7_result_bool(pCtx,0);` |
|    184 | 3670 | `			return PH7_OK;` |
|      - | 3671 | `		}` |
|      - | 3672 | `		/* Return the pattern position */` |
|     15 | 3673 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 3674 | `	}else{` |
|    ! 0 | 3675 | `		ph7_result_bool(pCtx,0);` |
|      - | 3676 | `	}` |
|     15 | 3677 | `	return PH7_OK;` |
|    100 | 3678 | `}` |
|      - | 3679 | `/*` |
|      - | 3680 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3681 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 3682 | ` * Parameters` |
|      - | 3683 | ` *  $haystack` |
|      - | 3684 | ` *   The input string.` |
|      - | 3685 | ` * $needle` |
|      - | 3686 | ` *   Search pattern (must be a string).` |
|      - | 3687 | ` * $offset` |
|      - | 3688 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3689 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3690 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3691 | ` * Return` |
|      - | 3692 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3693 | ` */` |
|     48 | 3694 | `static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3695 | `{` |
|      - | 3696 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     49 | 3697 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3698 | `	int nLen,nPatLen;` |
|      - | 3699 | `	sxu32 nOfft;` |
|      - | 3700 | `	sxi32 rc;` |
|     49 | 3701 | `	if( nArg < 2 ){` |
|      - | 3702 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3703 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3704 | `		return PH7_OK;` |
|      - | 3705 | `	}` |
|      - | 3706 | `	/* Extract the needle and the haystack */` |
|     49 | 3707 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     49 | 3708 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3709 | `	/* Point to the end of the pattern */` |
|     49 | 3710 | `	zPtr = &zBlob[nLen - 1];` |
|     49 | 3711 | `	zEnd = &zBlob[nLen];` |
|      - | 3712 | `	/* Save the starting posistion */` |
|     49 | 3713 | `	zStart = zBlob;` |
|     49 | 3714 | `	nOfft = 0; /* cc warning */` |
|      - | 3715 | `	/* Peek the starting offset if available */` |
|     49 | 3716 | `	if( nArg > 2 ){` |
|      - | 3717 | `		int nStart;` |
|     21 | 3718 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 3719 | `		if( nStart < 0 ){` |
|     11 | 3720 | `			nStart = -nStart;` |
|     11 | 3721 | `			if( nStart >= nLen ){` |
|      - | 3722 | `				/* Invalid offset */` |
|      3 | 3723 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3724 | `				return PH7_OK;` |
|    ! 0 | 3725 | `			}else{` |
|      9 | 3726 | `				nLen -= nStart;` |
|      9 | 3727 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 3728 | `				zEnd = &zBlob[nLen];` |
|      - | 3729 | `			}` |
|      5 | 3730 | `		}else{` |
|     11 | 3731 | `			if( nStart >= nLen ){` |
|      - | 3732 | `				/* Invalid offset */` |
|      5 | 3733 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3734 | `				return PH7_OK;` |
|    ! 0 | 3735 | `			}else{` |
|      7 | 3736 | `				zBlob += nStart;` |
|      7 | 3737 | `				nLen -= nStart;` |
|      - | 3738 | `			}` |
|      - | 3739 | `		}` |
|      7 | 3740 | `	}` |
|     43 | 3741 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3742 | `		/* Perform the lookup */` |
|    130 | 3743 | `		for(;;){` |
|    261 | 3744 | `			if( zBlob >= zPtr ){` |
|     21 | 3745 | `				break;` |
|      - | 3746 | `			}` |
|    241 | 3747 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    241 | 3748 | `			if( rc == SXRET_OK ){` |
|      - | 3749 | `				/* Pattern found,return it's position */` |
|     21 | 3750 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     21 | 3751 | `				return PH7_OK;` |
|      - | 3752 | `			}` |
|    221 | 3753 | `			zPtr--;` |
|      1 | 3754 | `		}` |
|      - | 3755 | `		/* Pattern not found,return FALSE */` |
|     21 | 3756 | `		ph7_result_bool(pCtx,0);` |
|     11 | 3757 | `	}else{` |
|      3 | 3758 | `		ph7_result_bool(pCtx,0);` |
|      - | 3759 | `	}` |
|     23 | 3760 | `	return PH7_OK;` |
|     25 | 3761 | `}` |
|      - | 3762 | `/*` |
|      - | 3763 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3764 | ` *  Case-insensitive strrpos.` |
|      - | 3765 | ` * Parameters` |
|      - | 3766 | ` *  $haystack` |
|      - | 3767 | ` *   The input string.` |
|      - | 3768 | ` * $needle` |
|      - | 3769 | ` *   Search pattern (must be a string).` |
|      - | 3770 | ` * $offset` |
|      - | 3771 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3772 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3773 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3774 | ` * Return` |
|      - | 3775 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3776 | ` */` |
|     26 | 3777 | `static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3778 | `{` |
|      - | 3779 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     27 | 3780 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3781 | `	int nLen,nPatLen;` |
|      - | 3782 | `	sxu32 nOfft;` |
|      - | 3783 | `	sxi32 rc;` |
|     27 | 3784 | `	if( nArg < 2 ){` |
|      - | 3785 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3786 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3787 | `		return PH7_OK;` |
|      - | 3788 | `	}` |
|      - | 3789 | `	/* Extract the needle and the haystack */` |
|     27 | 3790 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 3791 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3792 | `	/* Point to the end of the pattern */` |
|     27 | 3793 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 3794 | `	zEnd = &zBlob[nLen];` |
|      - | 3795 | `	/* Save the starting posistion */` |
|     27 | 3796 | `	zStart = zBlob;` |
|     27 | 3797 | `	nOfft = 0; /* cc warning */` |
|      - | 3798 | `	/* Peek the starting offset if available */` |
|     27 | 3799 | `	if( nArg > 2 ){` |
|      - | 3800 | `		int nStart;` |
|     15 | 3801 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3802 | `		if( nStart < 0 ){` |
|      7 | 3803 | `			nStart = -nStart;` |
|      7 | 3804 | `			if( nStart >= nLen ){` |
|      - | 3805 | `				/* Invalid offset */` |
|      3 | 3806 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3807 | `				return PH7_OK;` |
|    ! 0 | 3808 | `			}else{` |
|      5 | 3809 | `				nLen -= nStart;` |
|      5 | 3810 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 3811 | `				zEnd = &zBlob[nLen];` |
|      - | 3812 | `			}` |
|      3 | 3813 | `		}else{` |
|      9 | 3814 | `			if( nStart >= nLen ){` |
|      - | 3815 | `				/* Invalid offset */` |
|      5 | 3816 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3817 | `				return PH7_OK;` |
|    ! 0 | 3818 | `			}else{` |
|      5 | 3819 | `				zBlob += nStart;` |
|      5 | 3820 | `				nLen -= nStart;` |
|      - | 3821 | `			}` |
|      - | 3822 | `		}` |
|      4 | 3823 | `	}` |
|     21 | 3824 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3825 | `		/* Perform the lookup */` |
|     44 | 3826 | `		for(;;){` |
|     89 | 3827 | `			if( zBlob >= zPtr ){` |
|      9 | 3828 | `				break;` |
|      - | 3829 | `			}` |
|     81 | 3830 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 3831 | `			if( rc == SXRET_OK ){` |
|      - | 3832 | `				/* Pattern found,return it's position */` |
|     11 | 3833 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 3834 | `				return PH7_OK;` |
|      - | 3835 | `			}` |
|     71 | 3836 | `			zPtr--;` |
|      1 | 3837 | `		}` |
|      - | 3838 | `		/* Pattern not found,return FALSE */` |
|      9 | 3839 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3840 | `	}else{` |
|      3 | 3841 | `		ph7_result_bool(pCtx,0);` |
|      - | 3842 | `	}` |
|     11 | 3843 | `	return PH7_OK;` |
|     14 | 3844 | `}` |
|      - | 3845 | `/*` |
|      - | 3846 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 3847 | ` *  Find the last occurrence of a character in a string.` |
|      - | 3848 | ` * Parameters` |
|      - | 3849 | ` *  $haystack` |
|      - | 3850 | ` *   The input string.` |
|      - | 3851 | ` * $needle` |
|      - | 3852 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 3853 | ` *  This behavior is different from that of strstr().` |
|      - | 3854 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 3855 | ` *  as the ordinal value of a character.` |
|      - | 3856 | ` * Return` |
|      - | 3857 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 3858 | ` */` |
|     22 | 3859 | `static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3860 | `{` |
|      - | 3861 | `	const char *zBlob;` |
|      - | 3862 | `	int nLen,c;` |
|     23 | 3863 | `	if( nArg < 2 ){` |
|      - | 3864 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3865 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3866 | `		return PH7_OK;` |
|      - | 3867 | `	}` |
|      - | 3868 | `	/* Extract the haystack */` |
|     23 | 3869 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3870 | `	c = 0; /* cc warning */` |
|     23 | 3871 | `	if( nLen > 0 ){` |
|      - | 3872 | `		sxu32 nOfft;` |
|      - | 3873 | `		sxi32 rc;` |
|     21 | 3874 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 3875 | `			const char *zPattern;` |
|     11 | 3876 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 3877 | `														 * for NULL pointer.` |
|      - | 3878 | `														 */` |
|     11 | 3879 | `			c = zPattern[0];` |
|      6 | 3880 | `		}else{` |
|      - | 3881 | `			/* Int cast */` |
|     11 | 3882 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 3883 | `		}` |
|      - | 3884 | `		/* Perform the lookup */` |
|     21 | 3885 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 3886 | `		if( rc != SXRET_OK ){` |
|      - | 3887 | `			/* No such entry,return FALSE */` |
|      7 | 3888 | `			ph7_result_bool(pCtx,0);` |
|      7 | 3889 | `			return PH7_OK;` |
|      - | 3890 | `		}` |
|      - | 3891 | `		/* Return the string portion */` |
|     15 | 3892 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 3893 | `	}else{` |
|      3 | 3894 | `		ph7_result_bool(pCtx,0);` |
|      - | 3895 | `	}` |
|     17 | 3896 | `	return PH7_OK;` |
|     12 | 3897 | `}` |
|      - | 3898 | `/*` |
|      - | 3899 | ` * string strrev(string $string)` |
|      - | 3900 | ` *  Reverse a string.` |
|      - | 3901 | ` * Parameters` |
|      - | 3902 | ` *  $string` |
|      - | 3903 | ` *   String to be reversed.` |
|      - | 3904 | ` * Return` |
|      - | 3905 | ` *  The reversed string.` |
|      - | 3906 | ` */` |
|      2 | 3907 | `static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3908 | `{` |
|      - | 3909 | `	const char *zIn,*zEnd;` |
|      - | 3910 | `	int nLen,c;` |
|      3 | 3911 | `	if( nArg < 1 ){` |
|      - | 3912 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3913 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3914 | `		return PH7_OK;` |
|      - | 3915 | `	}` |
|      - | 3916 | `	/* Extract the target string */` |
|      3 | 3917 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3918 | `	if( nLen < 1 ){` |
|      - | 3919 | `		/* Empty string Return null */` |
|    ! 0 | 3920 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3921 | `		return PH7_OK;` |
|      - | 3922 | `	}` |
|      - | 3923 | `	/* Perform the requested operation */` |
|      3 | 3924 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 3925 | `	for(;;){` |
|      9 | 3926 | `		if( zEnd < zIn ){` |
|      - | 3927 | `			/* No more input to process */` |
|      3 | 3928 | `			break;` |
|      - | 3929 | `		}` |
|      - | 3930 | `		/* Append current character */` |
|      7 | 3931 | `		c = zEnd[0];` |
|      7 | 3932 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 3933 | `		zEnd--;` |
|      1 | 3934 | `	}` |
|      3 | 3935 | `	return PH7_OK;` |
|      2 | 3936 | `}` |
|      - | 3937 | `/*` |
|      - | 3938 | ` * string ucwords(string $string [, string $separators = " \t\r\n\f\v"])` |
|      - | 3939 | ` *  Uppercase the first character of each word in a string.` |
|      - | 3940 | ` *  A word begins at the start of the string and after any character present in` |
|      - | 3941 | ` *  $separators. The default separators are the whitespace characters (space,` |
|      - | 3942 | ` *  horizontal tab, carriage return, newline, form-feed and vertical tab); an` |
|      - | 3943 | ` *  explicit $separators argument REPLACES them (an empty string leaves only the` |
|      - | 3944 | ` *  very first character upper-cased). Like PHP, this is byte-based: only ASCII` |
|      - | 3945 | ` *  bytes are upper-cased and a byte is a separator only if it appears in the set.` |
|      - | 3946 | ` * Parameters` |
|      - | 3947 | ` *  $string` |
|      - | 3948 | ` *   The input string.` |
|      - | 3949 | ` *  $separators` |
|      - | 3950 | ` *   The optional word-boundary characters.` |
|      - | 3951 | ` * Return` |
|      - | 3952 | ` *  The modified string.` |
|      - | 3953 | ` */` |
|     22 | 3954 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3955 | `{` |
|      - | 3956 | `	const char *zIn;` |
|      - | 3957 | `	int nLen,i,iStart;` |
|      - | 3958 | `	char aDelim[256];` |
|     23 | 3959 | `	if( nArg < 1 ){` |
|      - | 3960 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3961 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3962 | `		return PH7_OK;` |
|      - | 3963 | `	}` |
|      - | 3964 | `	/* Build the separator membership table: an explicit $separators argument` |
|      - | 3965 | `	 * replaces the default whitespace set (an empty string clears it). */` |
|     23 | 3966 | `	SyZero(aDelim,(sxu32)sizeof(aDelim));` |
|     23 | 3967 | `	if( nArg > 1 ){` |
|      - | 3968 | `		int nDelim;` |
|      9 | 3969 | `		const char *zDelim = ph7_value_to_string(apArg[1],&nDelim);` |
|     17 | 3970 | `		for( i = 0 ; i < nDelim ; i++ ){` |
|      9 | 3971 | `			aDelim[(unsigned char)zDelim[i]] = 1;` |
|      5 | 3972 | `		}` |
|      5 | 3973 | `	}else{` |
|     15 | 3974 | `		aDelim[(unsigned char)' ']  = 1;` |
|     15 | 3975 | `		aDelim[(unsigned char)'\t'] = 1;` |
|     15 | 3976 | `		aDelim[(unsigned char)'\r'] = 1;` |
|     15 | 3977 | `		aDelim[(unsigned char)'\n'] = 1;` |
|     15 | 3978 | `		aDelim[(unsigned char)'\f'] = 1;` |
|     15 | 3979 | `		aDelim[(unsigned char)'\v'] = 1;` |
|      - | 3980 | `	}` |
|      - | 3981 | `	/* Extract the target string */` |
|     23 | 3982 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3983 | `	if( nLen < 1 ){` |
|      - | 3984 | `		/* Empty string – match PHP semantics */` |
|      3 | 3985 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3986 | `		return PH7_OK;` |
|      - | 3987 | `	}` |
|      - | 3988 | `	/* Upper-case the first byte of each word (the leading byte, or any byte that` |
|      - | 3989 | `	 * follows a separator), appending the untouched runs in between verbatim. */` |
|     21 | 3990 | `	iStart = 0;` |
|    309 | 3991 | `	for( i = 0 ; i < nLen ; i++ ){` |
|    289 | 3992 | `		int c = (unsigned char)zIn[i];` |
|    289 | 3993 | `		if( (i == 0 \|\| aDelim[(unsigned char)zIn[i-1]]) && c < 0x80 && SyisLower(c) ){` |
|     53 | 3994 | `			char up = (char)SyToUpper(c);` |
|     53 | 3995 | `			if( i > iStart ){` |
|     35 | 3996 | `				ph7_result_string(pCtx,&zIn[iStart],i - iStart);` |
|     17 | 3997 | `			}` |
|     53 | 3998 | `			ph7_result_string(pCtx,&up,1);` |
|     53 | 3999 | `			iStart = i + 1;` |
|     26 | 4000 | `		}` |
|    145 | 4001 | `	}` |
|     21 | 4002 | `	if( nLen > iStart ){` |
|     21 | 4003 | `		ph7_result_string(pCtx,&zIn[iStart],nLen - iStart);` |
|     10 | 4004 | `	}` |
|     21 | 4005 | `	return PH7_OK;` |
|     12 | 4006 | `}` |
|      - | 4007 | `/*` |
|      - | 4008 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 4009 | ` *  Returns input repeated multiplier times.` |
|      - | 4010 | ` * Parameters` |
|      - | 4011 | ` *  $string` |
|      - | 4012 | ` *   String to be repeated.` |
|      - | 4013 | ` * $multiplier` |
|      - | 4014 | ` *  Number of time the input string should be repeated.` |
|      - | 4015 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 4016 | ` *  to 0, the function will return an empty string.` |
|      - | 4017 | ` * Return` |
|      - | 4018 | ` *  The repeated string.` |
|      - | 4019 | ` */` |
|  20438 | 4020 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4021 | `{` |
|      - | 4022 | `	const char *zIn;` |
|      - | 4023 | `	int nLen;` |
|      - | 4024 | `	ph7_int64 nMul;` |
|      - | 4025 | `	int rc;` |
|  20440 | 4026 | `	if( nArg < 2 ){` |
|      - | 4027 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 4028 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4029 | `		return PH7_OK;` |
|      - | 4030 | `	}` |
|      - | 4031 | `	/* Extract the target string */` |
|  20440 | 4032 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 4033 | `	/* Resolve $times through the shared ZPP helper so a lossy float / float-string` |
|      - | 4034 | `	 * carries php's precision deprecation and NAN/INF/non-numeric fail with php's` |
|      - | 4035 | `	 * TypeError — a bare ph7_value_to_int64() coerced them silently. */` |
|      - | 4036 | `	{` |
|  20440 | 4037 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"str_repeat",2,"$times","int",&nMul);` |
|  20440 | 4038 | `		if( rcArg != PH7_OK ){` |
|    ! 0 | 4039 | `			return rcArg;` |
|      - | 4040 | `		}` |
|      - | 4041 | `	}` |
|  20440 | 4042 | `	if( nMul < 0 ){` |
|      3 | 4043 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4044 | `			"str_repeat(): Argument #2 ($times) must be greater than or equal to 0");` |
|      - | 4045 | `	}` |
|  20438 | 4046 | `	if( nLen < 1 \|\| nMul < 1 ){` |
|      - | 4047 | `		/* Empty input or a zero multiplier yields the empty string (PHP). */` |
|    ! 0 | 4048 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4049 | `		return PH7_OK;` |
|      - | 4050 | `	}` |
|      - | 4051 | `	/* Perform the requested operation */` |
| 223888 | 4052 | `	for(;;){` |
| 447778 | 4053 | `		if( !nMul ){` |
|  20438 | 4054 | `			break;` |
|      - | 4055 | `		}` |
|      - | 4056 | `		/* Append the copy */` |
| 427342 | 4057 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 427342 | 4058 | `		if( rc != PH7_OK ){` |
|      - | 4059 | `			/* Allocation failed: surface a fatal instead of returning a` |
|      - | 4060 | `			 * silently-truncated string with a success status. */` |
|    ! 0 | 4061 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 4062 | `		}` |
| 427342 | 4063 | `		nMul--;` |
|      2 | 4064 | `	}` |
|  20438 | 4065 | `	return PH7_OK;` |
|  10221 | 4066 | `}` |
|      - | 4067 | `/*` |
|      - | 4068 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 4069 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 4070 | ` * Parameters` |
|      - | 4071 | ` *  $string` |
|      - | 4072 | ` *   The input string.` |
|      - | 4073 | ` * $is_xhtml` |
|      - | 4074 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 4075 | ` * Return` |
|      - | 4076 | ` *  The processed string.` |
|      - | 4077 | ` */` |
|      4 | 4078 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4079 | `{` |
|      - | 4080 | `	const char *zIn,*zCur,*zEnd;` |
|      5 | 4081 | `	int is_xhtml = 1; /* Default to XHTML-style '<br />' like PHP */` |
|      - | 4082 | `	int nLen;` |
|      5 | 4083 | `	if( nArg < 1 ){` |
|      - | 4084 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4085 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4086 | `		return PH7_OK;` |
|      - | 4087 | `	}` |
|      - | 4088 | `	/* Extract the target string */` |
|      5 | 4089 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 4090 | `	if( nLen < 1 ){` |
|      - | 4091 | `		/* Empty string,return null */` |
|    ! 0 | 4092 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4093 | `		return PH7_OK;` |
|      - | 4094 | `	}` |
|      5 | 4095 | `	if( nArg > 1 ){` |
|      3 | 4096 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 4097 | `	}` |
|      5 | 4098 | `	zEnd = &zIn[nLen];` |
|      - | 4099 | `	/* Perform the requested operation */` |
|      4 | 4100 | `	for(;;){` |
|      9 | 4101 | `		zCur = zIn;` |
|      - | 4102 | `		/* Delimit the string */` |
|     21 | 4103 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 4104 | `			zIn++;` |
|      1 | 4105 | `		}` |
|      9 | 4106 | `		if( zCur < zIn ){` |
|      - | 4107 | `			/* Output chunk verbatim */` |
|      9 | 4108 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 4109 | `		}` |
|      9 | 4110 | `		if( zIn >= zEnd ){` |
|      - | 4111 | `			/* No more input to process */` |
|      5 | 4112 | `			break;` |
|      - | 4113 | `		}` |
|      - | 4114 | `		/* Output the HTML line break */` |
|      - | 4115 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br />' (space before the slash), otherwise use '<br>' */` |
|      5 | 4116 | `		if( is_xhtml ){` |
|      3 | 4117 | `			ph7_result_string(pCtx,"<br />",(int)sizeof("<br />")-1);` |
|      2 | 4118 | `		}else{` |
|      3 | 4119 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 4120 | `		}` |
|      5 | 4121 | `		zCur = zIn;` |
|      - | 4122 | `		/* Append trailing line */` |
|     11 | 4123 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 4124 | `			zIn++;` |
|      1 | 4125 | `		}` |
|      5 | 4126 | `		if( zCur < zIn ){` |
|      - | 4127 | `			/* Output chunk verbatim */` |
|      5 | 4128 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 4129 | `		}` |
|      1 | 4130 | `	}` |
|      5 | 4131 | `	return PH7_OK;` |
|      3 | 4132 | `}` |
|      - | 4133 | `/*` |
|      - | 4134 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 4135 | ` *  According to the PHP reference manual.` |
|      - | 4136 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 4137 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 4138 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 4139 | ` * This applies to both sprintf() and printf().` |
|      - | 4140 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 4141 | ` * or more of these elements, in order:` |
|      - | 4142 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 4143 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 4144 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 4145 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 4146 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 4147 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 4148 | ` *   it with a single quote ('). See the examples below.` |
|      - | 4149 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 4150 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 4151 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 4152 | ` *   should result in.` |
|      - | 4153 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 4154 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 4155 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 4156 | ` *   limit to the string.` |
|      - | 4157 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 4158 | ` *       % - a literal percent character. No argument is required.` |
|      - | 4159 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 4160 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 4161 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 4162 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 4163 | ` * 	     for the number of digits after the decimal point.` |
|      - | 4164 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 4165 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 4166 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 4167 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 4168 | ` *       g - shorter of %e and %f.` |
|      - | 4169 | ` *       G - shorter of %E and %f.` |
|      - | 4170 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 4171 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 4172 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 4173 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 4174 | ` */` |
|      - | 4175 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 4176 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 4177 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 4178 | `/*` |
|      - | 4179 | `** Conversion types fall into various categories as defined by the` |
|      - | 4180 | `** following enumeration.` |
|      - | 4181 | `*/` |
|      - | 4182 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 4183 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 4184 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 4185 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 4186 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 4187 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 4188 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 4189 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 4190 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 4191 |  |
|      - | 4192 | `/*` |
|      - | 4193 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 4194 | `*/` |
|      - | 4195 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 4196 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 4197 | `/*` |
|      - | 4198 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 4199 | `** by an instance of the following structure` |
|      - | 4200 | `*/` |
|      - | 4201 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 4202 | `struct ph7_fmt_info` |
|      - | 4203 | `{` |
|      - | 4204 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 4205 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 4206 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 4207 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 4208 | `  char *charset; /* The character set for conversion */` |
|      - | 4209 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 4210 | `};` |
|      - | 4211 | `/* PH7_PhpFloatShape (php's float-shape post-processing) lives in memobj.c —` |
|      - | 4212 | ` * the default float->string cast needs it even when this whole formatting` |
|      - | 4213 | ` * region is compiled out by PH7_DISABLE_DISK_IO. */` |
|      - | 4214 | `/*` |
|      - | 4215 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 4216 | ` * used conversion types first.` |
|      - | 4217 | ` */` |
|      - | 4218 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 4219 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 4220 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 4221 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 4222 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 4223 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 4224 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 4225 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 4226 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 4227 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 4228 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 4229 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 4230 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 4231 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 4232 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 4233 | `  /* php's 'h'/'H' are the locale-independent twins of 'g'/'G'; PHL always` |
|      - | 4234 | `   * formats in the C locale, so they behave identically. */` |
|      - | 4235 | `  {  'h',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 4236 | `  {  'H',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 4237 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 4238 | `};` |
|      - | 4239 | `/*` |
|      - | 4240 | ` * PHP 8 raises a catchable ValueError for an unknown conversion specifier` |
|      - | 4241 | ` * (e.g. "%y", or the C-ism "%#x" — '#' is not a php flag). Because printf()` |
|      - | 4242 | ` * and fprintf() stream their output incrementally while sprintf() buffers it,` |
|      - | 4243 | ` * every format builtin calls PH7_FormatValidate (below) to check the whole` |
|      - | 4244 | ` * format string BEFORE formatting so the throw happens with no partial output` |
|      - | 4245 | ` * escaping (php buffers the entire result and only emits it on success). This` |
|      - | 4246 | ` * scan mirrors the specifier-locating logic of the main format loop below.` |
|      - | 4247 | ` * On the first unknown specifier, stores it in *pBad and returns TRUE; returns` |
|      - | 4248 | ` * FALSE when every specifier is known. (A found-flag rather than a sentinel` |
|      - | 4249 | ` * char, so a NUL specifier byte — "%\0" — is still reported, not mistaken for` |
|      - | 4250 | ` * "all valid".)` |
|      - | 4251 | ` */` |
|    498 | 4252 | `static int FormatUnknownSpec(const char *zIn,int nByte,int *pBad)` |
|      3 | 4253 | `{` |
|    501 | 4254 | `	const char *zEnd = &zIn[nByte];` |
|      - | 4255 | `	int c,idx;` |
|   3865 | 4256 | `	while( zIn < zEnd ){` |
|   3387 | 4257 | `		if( zIn[0] != '%' ){` |
|   2429 | 4258 | `			zIn++;` |
|   2429 | 4259 | `			continue;` |
|      - | 4260 | `		}` |
|    959 | 4261 | `		zIn++; /* jump the percent sign */` |
|      - | 4262 | `		/* php-supported flags: '-', '+', ' ', '0' and the "'<pad>'" custom-pad` |
|      - | 4263 | `		 * form. '#' is intentionally NOT treated as a flag so it surfaces as an` |
|      - | 4264 | `		 * unknown specifier, matching php. */` |
|   1199 | 4265 | `		while( zIn < zEnd ){` |
|   1197 | 4266 | `			c = zIn[0];` |
|   1197 | 4267 | `			if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|    229 | 4268 | `				zIn++;` |
|    229 | 4269 | `				continue;` |
|      - | 4270 | `			}` |
|    969 | 4271 | `			if( c=='\'' ){` |
|     13 | 4272 | `				zIn++;` |
|     13 | 4273 | `				if( zIn < zEnd ){` |
|     13 | 4274 | `					zIn++; /* the custom pad character */` |
|      6 | 4275 | `				}` |
|     13 | 4276 | `				continue;` |
|      - | 4277 | `			}` |
|    957 | 4278 | `			break;` |
|    ! 0 | 4279 | `		}` |
|      - | 4280 | `		/* field width */` |
|   1273 | 4281 | `		while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|    315 | 4282 | `			zIn++;` |
|      1 | 4283 | `		}` |
|      - | 4284 | `		/* positional specifier ($) — php parses flags AFTER it (e.g. "%1$-10s"),` |
|      - | 4285 | `		 * so skip the full flag set and width again, mirroring the main loop. */` |
|    959 | 4286 | `		if( zIn < zEnd && zIn[0]=='$' ){` |
|     15 | 4287 | `			zIn++;` |
|     17 | 4288 | `			while( zIn < zEnd ){` |
|     17 | 4289 | `				c = zIn[0];` |
|     17 | 4290 | `				if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|    ! 0 | 4291 | `					zIn++;` |
|    ! 0 | 4292 | `					continue;` |
|      - | 4293 | `				}` |
|     17 | 4294 | `				if( c=='\'' ){` |
|      3 | 4295 | `					zIn++;` |
|      3 | 4296 | `					if( zIn < zEnd ){` |
|      3 | 4297 | `						zIn++;` |
|      1 | 4298 | `					}` |
|      3 | 4299 | `					continue;` |
|      - | 4300 | `				}` |
|     15 | 4301 | `				break;` |
|    ! 0 | 4302 | `			}` |
|     23 | 4303 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|      9 | 4304 | `				zIn++;` |
|      1 | 4305 | `			}` |
|      7 | 4306 | `		}` |
|      - | 4307 | `		/* precision */` |
|    959 | 4308 | `		if( zIn < zEnd && zIn[0]=='.' ){` |
|    113 | 4309 | `			zIn++;` |
|    243 | 4310 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|    133 | 4311 | `				zIn++;` |
|      3 | 4312 | `			}` |
|     55 | 4313 | `		}` |
|      - | 4314 | `		/* a single 'l' length modifier (ignored, php compat) */` |
|    959 | 4315 | `		if( zIn < zEnd && zIn[0]=='l' ){` |
|     11 | 4316 | `			zIn++;` |
|      5 | 4317 | `		}` |
|    959 | 4318 | `		if( zIn >= zEnd ){` |
|      - | 4319 | `			/* A dangling '%' with no specifier: PHL's legacy path silently` |
|      - | 4320 | `			 * truncates here (recorded residual); nothing to validate. */` |
|      3 | 4321 | `			break;` |
|      - | 4322 | `		}` |
|    957 | 4323 | `		c = zIn[0];` |
|    957 | 4324 | `		zIn++; /* jump the conversion specifier */` |
|   3801 | 4325 | `		for( idx = 0 ; idx < (int)SX_ARRAYSIZE(aFmt) ; idx++ ){` |
|   3783 | 4326 | `			if( c == aFmt[idx].fmttype ){` |
|    939 | 4327 | `				break;` |
|      - | 4328 | `			}` |
|   1425 | 4329 | `		}` |
|    957 | 4330 | `		if( idx >= (int)SX_ARRAYSIZE(aFmt) ){` |
|     19 | 4331 | `			*pBad = c; /* unknown specifier */` |
|     19 | 4332 | `			return TRUE;` |
|      - | 4333 | `		}` |
|      3 | 4334 | `	}` |
|    483 | 4335 | `	return FALSE;` |
|    252 | 4336 | `}` |
|      - | 4337 | `/*` |
|      - | 4338 | ` * Validate a printf-style format string. PHP 8 raises a catchable ValueError for` |
|      - | 4339 | ` * an unknown conversion specifier, thrown before any output is produced. Every` |
|      - | 4340 | ` * format builtin (sprintf/printf/vprintf/vsprintf/fprintf/vfprintf) calls this` |
|      - | 4341 | ` * up-front, then propagates the returned status verbatim (PH7_EXCEPTION when the` |
|      - | 4342 | ` * throw is caught in place, PH7_ABORT when it goes uncaught).` |
|      - | 4343 | ` * Returns PH7_OK when the format is valid.` |
|      - | 4344 | ` */` |
|    498 | 4345 | `PH7_PRIVATE sxi32 PH7_FormatValidate(ph7_context *pCtx,const char *zFormat,int nByte)` |
|      3 | 4346 | `{` |
|    501 | 4347 | `	int badSpec = 0;` |
|    501 | 4348 | `	if( FormatUnknownSpec(zFormat,nByte,&badSpec) ){` |
|     28 | 4349 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      9 | 4350 | `			"Unknown format specifier \"%c\"",badSpec);` |
|      - | 4351 | `	}` |
|    483 | 4352 | `	return PH7_OK;` |
|    252 | 4353 | `}` |
|      - | 4354 | `/*` |
|      - | 4355 | ` * Count the number of VALUE arguments a format string needs: the greater of the` |
|      - | 4356 | ` * sequential (non-positional) conversion count and the highest positional index` |
|      - | 4357 | `` * (`%N$`). `%%` consumes nothing. Mirrors FormatUnknownSpec's specifier walk.`` |
|      - | 4358 | ` */` |
|    480 | 4359 | `static int FormatRequiredArgs(const char *zIn,int nByte)` |
|      3 | 4360 | `{` |
|    483 | 4361 | `	const char *zEnd = &zIn[nByte];` |
|    483 | 4362 | `	int c,seq = 0,maxpos = 0;` |
|   3833 | 4363 | `	while( zIn < zEnd ){` |
|   3355 | 4364 | `		int numVal = 0,pos = 0;` |
|   3355 | 4365 | `		if( zIn[0] != '%' ){` |
|   2415 | 4366 | `			zIn++;` |
|   2415 | 4367 | `			continue;` |
|      - | 4368 | `		}` |
|    941 | 4369 | `		zIn++; /* jump the percent sign */` |
|      - | 4370 | `		/* leading flags (incl. the "'<pad>'" custom-pad form) */` |
|   1181 | 4371 | `		while( zIn < zEnd ){` |
|   1179 | 4372 | `			c = zIn[0];` |
|   1179 | 4373 | `			if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){ zIn++; continue; }` |
|    951 | 4374 | `			if( c=='\'' ){ zIn++; if( zIn < zEnd ){ zIn++; } continue; }` |
|    939 | 4375 | `			break;` |
|    ! 0 | 4376 | `		}` |
|      - | 4377 | `		/* leading number: a positional index when a '$' follows, else the width */` |
|   1255 | 4378 | `		while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|    315 | 4379 | `			numVal = numVal*10 + (zIn[0]-'0');` |
|    315 | 4380 | `			zIn++;` |
|      1 | 4381 | `		}` |
|    941 | 4382 | `		if( zIn < zEnd && zIn[0]=='$' ){` |
|     15 | 4383 | `			pos = numVal;` |
|     15 | 4384 | `			zIn++;` |
|      - | 4385 | `			/* flags then width may follow the positional marker */` |
|     17 | 4386 | `			while( zIn < zEnd ){` |
|     17 | 4387 | `				c = zIn[0];` |
|     17 | 4388 | `				if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){ zIn++; continue; }` |
|     17 | 4389 | `				if( c=='\'' ){ zIn++; if( zIn < zEnd ){ zIn++; } continue; }` |
|     15 | 4390 | `				break;` |
|    ! 0 | 4391 | `			}` |
|     23 | 4392 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){ zIn++; }` |
|      7 | 4393 | `		}` |
|      - | 4394 | `		/* precision */` |
|    941 | 4395 | `		if( zIn < zEnd && zIn[0]=='.' ){` |
|    113 | 4396 | `			zIn++;` |
|    243 | 4397 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){ zIn++; }` |
|     55 | 4398 | `		}` |
|      - | 4399 | `		/* a single 'l' length modifier (ignored, php compat) */` |
|    941 | 4400 | `		if( zIn < zEnd && zIn[0]=='l' ){ zIn++; }` |
|    941 | 4401 | `		if( zIn >= zEnd ){ break; }` |
|    939 | 4402 | `		c = zIn[0];` |
|    939 | 4403 | `		zIn++; /* jump the conversion specifier */` |
|    939 | 4404 | `		if( c == '%' ){ continue; } /* %% consumes no argument */` |
|    931 | 4405 | `		if( pos > 0 ){` |
|     15 | 4406 | `			if( pos > maxpos ){ maxpos = pos; }` |
|      8 | 4407 | `		}else{` |
|    917 | 4408 | `			seq++;` |
|      - | 4409 | `		}` |
|      3 | 4410 | `	}` |
|    483 | 4411 | `	return seq > maxpos ? seq : maxpos;` |
|      3 | 4412 | `}` |
|      - | 4413 | `/*` |
|      - | 4414 | ` * PHP 8: a printf-family call with fewer VALUE arguments than the format needs` |
|      - | 4415 | ` * throws BEFORE any output. The non-vararg family (sprintf/printf/fprintf) raises` |
|      - | 4416 | ` * ArgumentCountError counting the format itself ("N arguments are required, M` |
|      - | 4417 | ` * given"); the vararg family (vsprintf/vprintf/vfprintf) raises a ValueError over` |
|      - | 4418 | ` * the values array ("The arguments array must contain N items, M given"). nValues` |
|      - | 4419 | ` * is the count of value arguments actually supplied; nFixed is the number of` |
|      - | 4420 | ` * fixed leading parameters counted in the ArgumentCountError totals (1 for the` |
|      - | 4421 | ` * $format of sprintf/printf, 2 for fprintf's $stream + $format — the vararg` |
|      - | 4422 | ` * ValueError counts only the array, so nFixed is ignored there). Returns PH7_OK` |
|      - | 4423 | ` * when enough.` |
|      - | 4424 | ` */` |
|    480 | 4425 | `PH7_PRIVATE sxi32 PH7_FormatCheckArgCount(ph7_context *pCtx,const char *zFormat,int nByte,int nValues,int nFixed,int bVararg)` |
|      3 | 4426 | `{` |
|    483 | 4427 | `	int required = FormatRequiredArgs(zFormat,nByte);` |
|    483 | 4428 | `	if( nValues < required ){` |
|     21 | 4429 | `		if( bVararg ){` |
|     10 | 4430 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      3 | 4431 | `				"The arguments array must contain %d items, %d given",required,nValues);` |
|      - | 4432 | `		}` |
|     22 | 4433 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|      7 | 4434 | `			"%d arguments are required, %d given",required+nFixed,nValues+nFixed);` |
|      - | 4435 | `	}` |
|    463 | 4436 | `	return PH7_OK;` |
|    243 | 4437 | `}` |
|      - | 4438 | `/*` |
|      - | 4439 | `` * PHP 8: a printf-family `$format` argument is a `string` parameter — scalars`` |
|      - | 4440 | ` * (int/float/bool) and null coerce to a string, but an array/object/resource` |
|      - | 4441 | ` * raises a catchable TypeError. iArg is the 1-based argument position ($format` |
|      - | 4442 | ` * is #1 for sprintf/printf/vprintf/vsprintf, #2 for fprintf/vfprintf). Returns` |
|      - | 4443 | ` * PH7_OK when the value is string-coercible (the caller then uses` |
|      - | 4444 | ` * ph7_value_to_string, which renders scalars/null verbatim).` |
|      - | 4445 | ` */` |
|      - | 4446 | `/*` |
|      - | 4447 | ` * php 8: a stream parameter that is not a resource is a TypeError, not a warning` |
|      - | 4448 | ` * with a 0 return -- the caller never learned its write went nowhere.` |
|      - | 4449 | ` */` |
|     24 | 4450 | `PH7_PRIVATE sxi32 PH7_CheckStreamArg(ph7_context *pCtx,ph7_value *pArg,int iArg,const char *zName)` |
|      1 | 4451 | `{` |
|     25 | 4452 | `	if( !ph7_value_is_resource(pArg) ){` |
|      - | 4453 | `		char zBuf[64];` |
|      4 | 4454 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4455 | `			"%s(): Argument #%d ($%s) must be of type resource, %s given",` |
|      1 | 4456 | `			ph7_function_name(pCtx),iArg,zName,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|      - | 4457 | `	}` |
|     23 | 4458 | `	return PH7_OK;` |
|     13 | 4459 | `}` |
|    510 | 4460 | `PH7_PRIVATE sxi32 PH7_FormatCheckFormatArg(ph7_context *pCtx,ph7_value *pArg,int iArg)` |
|      3 | 4461 | `{` |
|    513 | 4462 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg) \|\| ph7_value_is_resource(pArg) ){` |
|      - | 4463 | `		char zBuf[64];` |
|    ! 0 | 4464 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4465 | `			"%s(): Argument #%d ($format) must be of type string, %s given",` |
|    ! 0 | 4466 | `			ph7_function_name(pCtx),iArg,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|      - | 4467 | `	}` |
|    513 | 4468 | `	return PH7_OK;` |
|    258 | 4469 | `}` |
|      - | 4470 | `/*` |
|      - | 4471 | ` * Format a given string.` |
|      - | 4472 | ` * The root program.  All variations call this core.` |
|      - | 4473 | ` * INPUTS:` |
|      - | 4474 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 4475 | ` *            1. A pointer to the call context.` |
|      - | 4476 | ` *            2. A pointer to the list of characters to be output` |
|      - | 4477 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 4478 | ` *            3. An integer number of characters to be output.` |
|      - | 4479 | ` *               (Note: This number might be zero.)` |
|      - | 4480 | ` *            4. Upper layer private data.` |
|      - | 4481 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 4482 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 4483 | ` */` |
|    460 | 4484 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 4485 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 4486 | `	ph7_context *pCtx,  /* call context */` |
|      - | 4487 | `	const char *zIn,    /* Format string */` |
|      - | 4488 | `	int nByte,          /* Format string length */` |
|      - | 4489 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 4490 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 4491 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 4492 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 4493 | `	)` |
|      3 | 4494 | `{` |
|    463 | 4495 | `	char spaces[] = "                                                  ";` |
|      - | 4496 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    463 | 4497 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 4498 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 4499 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 4500 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 4501 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 4502 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 4503 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 4504 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 4505 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 4506 | `	ph7_int64 iVal;` |
|      - | 4507 | `	int precision;           /* Precision of the current field */` |
|      - | 4508 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 4509 | `	int c,rc,n;` |
|      - | 4510 | `	int length;              /* Length of the field */` |
|      - | 4511 | `	int prefix;` |
|      - | 4512 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 4513 | `	int width;               /* Width of the current field */` |
|      - | 4514 | `	int idx;` |
|    463 | 4515 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 4516 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 4517 | `	/* An unknown conversion specifier is rejected up-front by PH7_FormatValidate()` |
|      - | 4518 | `	 * (called by every format builtin before this routine), so the specifier set` |
|      - | 4519 | `	 * seen here is always valid. */` |
|      - | 4520 | `	/* Start the format process */` |
|    682 | 4521 | `	for(;;){` |
|   1367 | 4522 | `		zCur = zIn;` |
|   3773 | 4523 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|   2407 | 4524 | `			zIn++;` |
|      1 | 4525 | `		}` |
|   1367 | 4526 | `		if( zCur < zIn ){` |
|      - | 4527 | `			/* Consume chunk verbatim */` |
|    793 | 4528 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|    793 | 4529 | `			if( rc != SXRET_OK ){` |
|      - | 4530 | `				/* Callback requested an abort (e.g. an allocation failure) */` |
|    ! 0 | 4531 | `				break;` |
|      - | 4532 | `			}` |
|    396 | 4533 | `		}` |
|   1367 | 4534 | `		if( zIn >= zEnd ){` |
|      - | 4535 | `			/* No more input to process,break immediately */` |
|    461 | 4536 | `			break;` |
|      - | 4537 | `		}` |
|      - | 4538 | `		/* Find out what flags are present */` |
|    909 | 4539 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    906 | 4540 | `			flag_alternateform = flag_zeropad = 0;` |
|      - | 4541 | `		/* Reset the pad buffer to spaces: a custom pad char ('X) — or the string` |
|      - | 4542 | `		 * zero-pad below — from a PREVIOUS specifier must not bleed into this one.` |
|      - | 4543 | `		 * php resets the pad character for every specifier. */` |
|  46209 | 4544 | `		for( idx = 0 ; idx < etSPACESIZE ; ++idx ){ spaces[idx] = ' '; }` |
|    909 | 4545 | `		zIn++; /* Jump the precent sign */` |
|    453 | 4546 | `		do{` |
|   1149 | 4547 | `			c = zIn[0];` |
|   1149 | 4548 | `			switch( c ){` |
|     19 | 4549 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      7 | 4550 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 4551 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|    199 | 4552 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|      6 | 4553 | `			case '\'':` |
|     13 | 4554 | `				zIn++;` |
|     13 | 4555 | `				if( zIn < zEnd ){` |
|      - | 4556 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|     13 | 4557 | `					c = zIn[0];` |
|    613 | 4558 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    601 | 4559 | `						spaces[idx] = (char)c;` |
|    301 | 4560 | `					}` |
|     13 | 4561 | `					c = 0;` |
|      6 | 4562 | `				}` |
|     12 | 4563 | `				break;` |
|    906 | 4564 | `			default:                                       break;` |
|      - | 4565 | `			}` |
|   1149 | 4566 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 4567 | `		/* Get the field width */` |
|    909 | 4568 | `		width = 0;` |
|   1670 | 4569 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    309 | 4570 | `			width = width*10 + (zIn[0] - '0');` |
|    309 | 4571 | `			zIn++;` |
|      1 | 4572 | `		}` |
|    909 | 4573 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 4574 | `			/* Position specifer */` |
|      9 | 4575 | `			if( width > 0 ){` |
|      9 | 4576 | `				n = width;` |
|      9 | 4577 | `				if( vf && n > 0 ){` |
|    ! 0 | 4578 | `					n--;` |
|    ! 0 | 4579 | `				}` |
|      4 | 4580 | `			}` |
|      9 | 4581 | `			zIn++;` |
|      9 | 4582 | `			width = 0;` |
|      - | 4583 | `			/* php's grammar is %argnum$<flags><width>: the flags come AFTER the` |
|      - | 4584 | `			 * positional, so re-parse the full flag set here (e.g. "%1$-10s"),` |
|      - | 4585 | `			 * not just zero-padding. */` |
|      4 | 4586 | `			do{` |
|     11 | 4587 | `				c = zIn[0];` |
|     11 | 4588 | `				switch( c ){` |
|    ! 0 | 4589 | `				case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|    ! 0 | 4590 | `				case '+':   flag_plussign = 1;        c = 0;   break;` |
|    ! 0 | 4591 | `				case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|    ! 0 | 4592 | `				case '0':   flag_zeropad = 1;         c = 0;   break;` |
|      1 | 4593 | `				case '\'':` |
|      3 | 4594 | `					zIn++;` |
|      3 | 4595 | `					if( zIn < zEnd ){` |
|      3 | 4596 | `						c = zIn[0];` |
|    103 | 4597 | `						for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    101 | 4598 | `							spaces[idx] = (char)c;` |
|     51 | 4599 | `						}` |
|      3 | 4600 | `						c = 0;` |
|      1 | 4601 | `					}` |
|      2 | 4602 | `					break;` |
|      8 | 4603 | `				default:                                       break;` |
|      - | 4604 | `				}` |
|     11 | 4605 | `			}while( c==0 && (zIn++ < zEnd) );` |
|     21 | 4606 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|      9 | 4607 | `				width = width*10 + (zIn[0] - '0');` |
|      9 | 4608 | `				zIn++;` |
|      1 | 4609 | `			}` |
|      4 | 4610 | `		}` |
|    909 | 4611 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 4612 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 4613 | `		}` |
|      - | 4614 | `		/* Get the precision */` |
|    909 | 4615 | `		precision = -1;` |
|    909 | 4616 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|    113 | 4617 | `			precision = 0;` |
|    113 | 4618 | `			zIn++;` |
|    298 | 4619 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    133 | 4620 | `				precision = precision*10 + (zIn[0] - '0');` |
|    133 | 4621 | `				zIn++;` |
|      3 | 4622 | `			}` |
|     55 | 4623 | `		}` |
|      - | 4624 | `		/* Consume a single 'l' length modifier (a C-ism php accepts and ignores,` |
|      - | 4625 | `		 * e.g. "%ld"); PH7_FormatValidate mirrors this. Exactly one is skipped:` |
|      - | 4626 | `		 * in "%lld" the second 'l' becomes the (unknown) specifier, just like php. */` |
|    909 | 4627 | `		if( zIn < zEnd && zIn[0] == 'l' ){` |
|      9 | 4628 | `			zIn++;` |
|      4 | 4629 | `		}` |
|    909 | 4630 | `		if( zIn >= zEnd ){` |
|      - | 4631 | `			/* No more input */` |
|      3 | 4632 | `			break;` |
|      - | 4633 | `		}` |
|      - | 4634 | `		/* Fetch the info entry for the field */` |
|    907 | 4635 | `		pInfo = 0;` |
|    907 | 4636 | `		xtype = PH7_FMT_ERROR;` |
|    907 | 4637 | `		c = zIn[0];` |
|    907 | 4638 | `		zIn++; /* Jump the format specifer */` |
|   3431 | 4639 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|   3431 | 4640 | `			if( c==aFmt[idx].fmttype ){` |
|    907 | 4641 | `				pInfo = &aFmt[idx];` |
|    907 | 4642 | `				xtype = pInfo->type;` |
|    907 | 4643 | `				break;` |
|      - | 4644 | `			}` |
|   1265 | 4645 | `		}` |
|    907 | 4646 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    907 | 4647 | `		length = 0;` |
|      - | 4648 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 4649 | `		 /*` |
|      - | 4650 | `		  ** At this point, variables are initialized as follows:` |
|      - | 4651 | `		  **` |
|      - | 4652 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 4653 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 4654 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 4655 | `		  **                               field width was negative.` |
|      - | 4656 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 4657 | `		  **                               the conversion character.` |
|      - | 4658 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 4659 | `		  **   width                       The specified field width.  This is` |
|      - | 4660 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 4661 | `		  **   precision                   The specified precision.  The default` |
|      - | 4662 | `		  **                               is -1.` |
|      - | 4663 | `		  */` |
|    907 | 4664 | `		switch(xtype){` |
|      4 | 4665 | `		case PH7_FMT_PERCENT:` |
|      - | 4666 | `			/* A literal percent character */` |
|      9 | 4667 | `			zWorker[0] = '%';` |
|      9 | 4668 | `			length = (int)sizeof(char);` |
|      9 | 4669 | `			break;` |
|      2 | 4670 | `		case PH7_FMT_CHARX:` |
|      - | 4671 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 4672 | `			 * with that ASCII value` |
|      - | 4673 | `			 */` |
|      5 | 4674 | `			pArg = NEXT_ARG;` |
|      5 | 4675 | `			if( pArg == 0 ){` |
|    ! 0 | 4676 | `				c = 0;` |
|    ! 0 | 4677 | `			}else{` |
|      5 | 4678 | `				c = ph7_value_to_int(pArg);` |
|      - | 4679 | `			}` |
|      - | 4680 | `			/* NUL byte is an acceptable value */` |
|      5 | 4681 | `			zWorker[0] = (char)c;` |
|      5 | 4682 | `			length = (int)sizeof(char);` |
|      5 | 4683 | `			break;` |
|    188 | 4684 | `		case PH7_FMT_STRING:` |
|      - | 4685 | `			/* the argument is treated as and presented as a string */` |
|    377 | 4686 | `			pArg = NEXT_ARG;` |
|    377 | 4687 | `			if( pArg == 0 ){` |
|    ! 0 | 4688 | `				length = 0;` |
|    ! 0 | 4689 | `			}else{` |
|    377 | 4690 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 4691 | `			}` |
|    377 | 4692 | `			if( length < 1 ){` |
|      - | 4693 | `				/* An empty %s substitutes NOTHING in php. PH7 substituted a single` |
|      - | 4694 | `				 * SPACE here, so printf("[%s]","") printed "[ ]" and any format with an` |
|      - | 4695 | `				 * absent optional part gained a stray space. */` |
|      9 | 4696 | `				zBuf = "";` |
|      9 | 4697 | `				length = 0;` |
|      4 | 4698 | `			}` |
|    377 | 4699 | `			if( precision>=0 && precision<length ){` |
|      3 | 4700 | `				length = precision;` |
|      1 | 4701 | `			}` |
|    377 | 4702 | `			if( flag_zeropad ){` |
|      - | 4703 | `				/* zero-padding works on strings too */` |
|    103 | 4704 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    101 | 4705 | `					spaces[idx] = '0';` |
|     51 | 4706 | `				}` |
|      1 | 4707 | `			}` |
|    377 | 4708 | `			break;` |
|    158 | 4709 | `		case PH7_FMT_RADIX:` |
|    317 | 4710 | `			pArg = NEXT_ARG;` |
|    317 | 4711 | `			if( pArg == 0 ){` |
|    ! 0 | 4712 | `				iVal = 0;` |
|    ! 0 | 4713 | `			}else{` |
|    317 | 4714 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 4715 | `			}` |
|      - | 4716 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|    317 | 4717 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 4718 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 4719 | `			}` |
|      - | 4720 | `#if 1` |
|      - | 4721 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 4722 | `        ** I think this is stupid.*/` |
|    317 | 4723 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 4724 | `#else` |
|      - | 4725 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 4726 | `        ** but leave the prefix for hex.*/` |
|      - | 4727 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 4728 | `#endif` |
|    317 | 4729 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|    293 | 4730 | `          if( iVal<0 ){` |
|     25 | 4731 | `            iVal = -iVal;` |
|      - | 4732 | `			/* Ticket 1433-003 */` |
|     25 | 4733 | `			if( iVal < 0 ){` |
|      - | 4734 | `				/* Overflow */` |
|    ! 0 | 4735 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4736 | `			}` |
|     25 | 4737 | `            prefix = '-';` |
|    281 | 4738 | `          }else if( flag_plussign )  prefix = '+';` |
|    267 | 4739 | `          else if( flag_blanksign )  prefix = ' ';` |
|    265 | 4740 | `          else                       prefix = 0;` |
|    147 | 4741 | `        }else{` |
|     25 | 4742 | `			if( iVal<0 ){` |
|    ! 0 | 4743 | `				iVal = -iVal;` |
|      - | 4744 | `				/* Ticket 1433-003 */` |
|    ! 0 | 4745 | `				if( iVal < 0 ){` |
|      - | 4746 | `					/* Overflow */` |
|    ! 0 | 4747 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4748 | `				}` |
|    ! 0 | 4749 | `			}` |
|     25 | 4750 | `			prefix = 0;` |
|      - | 4751 | `		}` |
|    317 | 4752 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|    185 | 4753 | `          precision = width-(prefix!=0);` |
|     92 | 4754 | `        }` |
|    317 | 4755 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 4756 | `        {` |
|      - | 4757 | `          register char *cset;      /* Use registers for speed */` |
|      - | 4758 | `          register int base;` |
|    317 | 4759 | `          cset = pInfo->charset;` |
|    317 | 4760 | `          base = pInfo->base;` |
|    158 | 4761 | `          do{                                           /* Convert to ascii */` |
|    393 | 4762 | `            *(--zBuf) = cset[iVal%base];` |
|    393 | 4763 | `            iVal = iVal/base;` |
|    393 | 4764 | `          }while( iVal>0 );` |
|      - | 4765 | `        }` |
|    317 | 4766 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    519 | 4767 | `        for(idx=precision-length; idx>0; idx--){` |
|    203 | 4768 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|    102 | 4769 | `        }` |
|    317 | 4770 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|    317 | 4771 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 4772 | `          char *pre, x;` |
|    ! 0 | 4773 | `          pre = pInfo->prefix;` |
|    ! 0 | 4774 | `          if( *zBuf!=pre[0] ){` |
|    ! 0 | 4775 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|    ! 0 | 4776 | `          }` |
|    ! 0 | 4777 | `        }` |
|    317 | 4778 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    317 | 4779 | `		break;` |
|    100 | 4780 | `		case PH7_FMT_FLOAT:` |
|      - | 4781 | `		case PH7_FMT_EXP:` |
|      - | 4782 | `		case PH7_FMT_GENERIC:{` |
|      - | 4783 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 4784 | `		double realvalue;` |
|      - | 4785 | `		char zFmt[8];` |
|      - | 4786 | `		int nOut, nFmt;` |
|    203 | 4787 | `		pArg = NEXT_ARG;` |
|    203 | 4788 | `		if( pArg == 0 ){` |
|    ! 0 | 4789 | `			realvalue = 0;` |
|    ! 0 | 4790 | `		}else{` |
|    203 | 4791 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 4792 | `		}` |
|      - | 4793 | `		/* php prints the IEEE specials bare — NaN / INF / -INF with no width` |
|      - | 4794 | `		 * padding, precision, or sign flags (php_sprintf_appenddouble). */` |
|    203 | 4795 | `		if( PH7_IS_NAN(realvalue) ){` |
|     21 | 4796 | `			zBuf = "NaN";` |
|     21 | 4797 | `			length = 3;` |
|     21 | 4798 | `			width = 0;` |
|     21 | 4799 | `			break;` |
|      - | 4800 | `		}` |
|    183 | 4801 | `		if( PH7_IS_INF(realvalue) ){` |
|     37 | 4802 | `			if( realvalue < 0.0 ){` |
|     15 | 4803 | `				zBuf = "-INF";` |
|     15 | 4804 | `				length = 4;` |
|      8 | 4805 | `			}else{` |
|     23 | 4806 | `				zBuf = "INF";` |
|     23 | 4807 | `				length = 3;` |
|      - | 4808 | `			}` |
|     37 | 4809 | `			width = 0;` |
|     37 | 4810 | `			break;` |
|      - | 4811 | `		}` |
|    147 | 4812 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|    147 | 4813 | `		if( precision > 53 ){` |
|      - | 4814 | `			/* php's FORMAT_CONV_MAX_PRECISION cap, with the same E_NOTICE` |
|      - | 4815 | `			 * (message prefixed with the active function's name, like` |
|      - | 4816 | `			 * php_error_docref). */` |
|      - | 4817 | `			char zMsg[160];` |
|      4 | 4818 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - | 4819 | `				"%z(): Requested precision of %d digits was truncated to PHP maximum of %d digits",` |
|      2 | 4820 | `				&pCtx->pFunc->sName,precision,53);` |
|      3 | 4821 | `			PH7_VmThrowError(pCtx->pVm,0,E_NOTICE,zMsg);` |
|      3 | 4822 | `			precision = 53;` |
|      1 | 4823 | `		}` |
|      - | 4824 | ``		/* php's %f/%e extract the sign via `num < 0`, so negative zero prints`` |
|      - | 4825 | `		 * unsigned there — while %g (php_gcvt on the raw value) keeps "-0". */` |
|    147 | 4826 | `		if( xtype!=PH7_FMT_GENERIC && realvalue == 0.0 ){` |
|      9 | 4827 | `			realvalue = 0.0;` |
|      4 | 4828 | `		}` |
|      - | 4829 | `		/* php's float conversions are correctly rounded (zend_dtoa); use libc` |
|      - | 4830 | `		 * snprintf as the digit engine (the byte-exact-floats rule — the old` |
|      - | 4831 | `		 * hand-rolled vxGetdigit loop stopped at 16 significant digits, so` |
|      - | 4832 | `		 * e.g. %f of 1e308 printed zeros where php prints the exact binary64` |
|      - | 4833 | `		 * expansion), then post-process into php's exact shapes below. */` |
|    147 | 4834 | `		nFmt = 0;` |
|    147 | 4835 | `		zFmt[nFmt++] = '%';` |
|    147 | 4836 | `		if( flag_alternateform ) zFmt[nFmt++] = '#';` |
|      - | 4837 | `		/* php's ' ' flag selects space PADDING (its default), not C's` |
|      - | 4838 | `		 * space-for-positive-sign — so flag_blanksign is NOT forwarded. */` |
|    147 | 4839 | `		if( flag_plussign ) zFmt[nFmt++] = '+';` |
|    147 | 4840 | `		zFmt[nFmt++] = '.';` |
|    147 | 4841 | `		zFmt[nFmt++] = '*';` |
|    195 | 4842 | `		zFmt[nFmt++] = (char)(xtype==PH7_FMT_FLOAT ? 'f' :` |
|     32 | 4843 | `			(xtype==PH7_FMT_EXP ? ((pInfo->charset[0]=='E') ? 'E' : 'e')` |
|     32 | 4844 | `			                    : ((pInfo->charset[0]=='E') ? 'G' : 'g')));` |
|    147 | 4845 | `		zFmt[nFmt] = 0;` |
|    147 | 4846 | `		nOut = snprintf(zWorker,sizeof(zWorker),zFmt,precision,realvalue);` |
|    147 | 4847 | `		if( nOut < 0 \|\| nOut >= (int)sizeof(zWorker) ){` |
|      - | 4848 | `			/* Cannot happen with precision capped at 53 (%f of DBL_MAX is` |
|      - | 4849 | `			 * ~365 bytes); keep the truncated output rather than overrun. */` |
|    ! 0 | 4850 | `			nOut = (int)SyStrlen(zWorker);` |
|    ! 0 | 4851 | `		}` |
|    147 | 4852 | `		nOut = (int)PH7_PhpFloatShape(zWorker,(sxi32)nOut,xtype==PH7_FMT_GENERIC);` |
|    147 | 4853 | `		zBuf = zWorker;` |
|    147 | 4854 | `		length = nOut;` |
|      - | 4855 | `		/* Let the zero-pad block below insert zeros between the sign (written` |
|      - | 4856 | `		 * by snprintf) and the first digit, as before. */` |
|    147 | 4857 | `		prefix = (zWorker[0]=='-' \|\| zWorker[0]=='+' \|\| zWorker[0]==' ') ? zWorker[0] : 0;` |
|      - | 4858 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 4859 | `        ** set and we are not left justified */` |
|    147 | 4860 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 4861 | `          int i;` |
|      9 | 4862 | `          int nPad = width - length;` |
|     63 | 4863 | `          for(i=width; i>=nPad; i--){` |
|     55 | 4864 | `            zBuf[i] = zBuf[i-nPad];` |
|     28 | 4865 | `          }` |
|      9 | 4866 | `          i = prefix!=0;` |
|     39 | 4867 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      9 | 4868 | `          length = width;` |
|      4 | 4869 | `        }` |
|      - | 4870 | `#else` |
|      - | 4871 | `         zBuf = " ";` |
|      - | 4872 | `		 length = (int)sizeof(char);` |
|      - | 4873 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|    147 | 4874 | `		 break;` |
|      - | 4875 | `							 }` |
|    ! 0 | 4876 | `		default:` |
|      - | 4877 | `			/* Unreachable: PH7_FormatValidate() rejects unknown specifiers with a` |
|      - | 4878 | `			 * catchable ValueError before formatting begins. Kept as a defensive` |
|      - | 4879 | `			 * no-op that emits nothing. */` |
|    ! 0 | 4880 | `			length = 0;` |
|    ! 0 | 4881 | `			break;` |
|      - | 4882 | `		}` |
|      - | 4883 | `		 /*` |
|      - | 4884 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 4885 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 4886 | `		 ** the output.` |
|      - | 4887 | `		 */` |
|    907 | 4888 | `    if( !flag_leftjustify ){` |
|      - | 4889 | `      register int nspace;` |
|    889 | 4890 | `      nspace = width-length;` |
|    889 | 4891 | `      if( nspace>0 ){` |
|     37 | 4892 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4893 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4894 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4895 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4896 | `			}` |
|    ! 0 | 4897 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4898 | `        }` |
|     37 | 4899 | `        if( nspace>0 ){` |
|     37 | 4900 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|     37 | 4901 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4902 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4903 | `			}` |
|     18 | 4904 | `		}` |
|     18 | 4905 | `      }` |
|    443 | 4906 | `    }` |
|    907 | 4907 | `    if( length>0 ){` |
|    899 | 4908 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    899 | 4909 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 4910 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4911 | `		}` |
|    448 | 4912 | `    }` |
|    907 | 4913 | `    if( flag_leftjustify ){` |
|      - | 4914 | `      register int nspace;` |
|     19 | 4915 | `      nspace = width-length;` |
|     19 | 4916 | `      if( nspace>0 ){` |
|     15 | 4917 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4918 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4919 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4920 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4921 | `			}` |
|    ! 0 | 4922 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4923 | `        }` |
|     15 | 4924 | `        if( nspace>0 ){` |
|     15 | 4925 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|     15 | 4926 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4927 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4928 | `			}` |
|      7 | 4929 | `		}` |
|      7 | 4930 | `      }` |
|      9 | 4931 | `    }` |
|      3 | 4932 | ` }/* for(;;) */` |
|    463 | 4933 | `	return SXRET_OK;` |
|    233 | 4934 | `}` |
|      - | 4935 | `/*` |
|      - | 4936 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 4937 | ` */` |
|    534 | 4938 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      3 | 4939 | `{` |
|      - | 4940 | `	/* pUserData points to the caller's allocation-rc slot so an OOM during the` |
|      - | 4941 | `	 * result append is surfaced (the builtin raises a fatal); returning the` |
|      - | 4942 | `	 * non-OK rc also stops the format loop. */` |
|    537 | 4943 | `	sxi32 *pRc = (sxi32 *)pUserData;` |
|    537 | 4944 | `	*pRc = ph7_result_string(pCtx,zInput,nLen);` |
|    537 | 4945 | `	return *pRc;` |
|      3 | 4946 | `}` |
|      - | 4947 | `/*` |
|      - | 4948 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 4949 | ` *  Return a formatted string.` |
|      - | 4950 | ` * Parameters` |
|      - | 4951 | ` *  $format` |
|      - | 4952 | ` *    The format string (see block comment above)` |
|      - | 4953 | ` * Return` |
|      - | 4954 | ` *  A string produced according to the formatting string format.` |
|      - | 4955 | ` */` |
|    254 | 4956 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4957 | `{` |
|      - | 4958 | `	const char *zFormat;` |
|    257 | 4959 | `	sxi32 rc = SXRET_OK;` |
|      - | 4960 | `	int nLen;` |
|    257 | 4961 | `	if( nArg < 1 ){` |
|      - | 4962 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4963 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4964 | `		return PH7_OK;` |
|      - | 4965 | `	}` |
|      - | 4966 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|    257 | 4967 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    257 | 4968 | `	if( rc != PH7_OK ){` |
|    ! 0 | 4969 | `		return rc;` |
|      - | 4970 | `	}` |
|      - | 4971 | `	/* Extract the string format (scalars/null coerce). */` |
|    257 | 4972 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    257 | 4973 | `	if( nLen < 1 ){` |
|      - | 4974 | `		/* Empty string */` |
|    ! 0 | 4975 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4976 | `		return PH7_OK;` |
|      - | 4977 | `	}` |
|      - | 4978 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4979 | `	 * output; propagate the throw status verbatim. */` |
|    257 | 4980 | `	rc = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    257 | 4981 | `	if( rc != PH7_OK ){` |
|     17 | 4982 | `		return rc;` |
|      - | 4983 | `	}` |
|      - | 4984 | `	/* PHP 8: too few value arguments is a catchable ArgumentCountError before output. */` |
|    241 | 4985 | `	rc = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg-1,1,FALSE);` |
|    241 | 4986 | `	if( rc != PH7_OK ){` |
|     11 | 4987 | `		return rc;` |
|      - | 4988 | `	}` |
|      - | 4989 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|    231 | 4990 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&rc,FALSE);` |
|    231 | 4991 | `	if( rc != SXRET_OK ){` |
|      - | 4992 | `		/* The result append ran out of memory: raise a fatal rather than` |
|      - | 4993 | `		 * returning a silently-truncated string. */` |
|    ! 0 | 4994 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4995 | `	}` |
|    231 | 4996 | `	return PH7_OK;` |
|    130 | 4997 | `}` |
|      - | 4998 | `/*` |
|      - | 4999 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 5000 | ` */` |
|   1174 | 5001 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 5002 | `{` |
|   1175 | 5003 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 5004 | `	/* Call the VM output consumer directly */` |
|   1175 | 5005 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 5006 | `	/* Increment counter */` |
|   1175 | 5007 | `	*pCounter += nLen;` |
|   1175 | 5008 | `	return PH7_OK;` |
|      1 | 5009 | `}` |
|      - | 5010 | `/*` |
|      - | 5011 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 5012 | ` *  Output a formatted string.` |
|      - | 5013 | ` * Parameters` |
|      - | 5014 | ` *  $format` |
|      - | 5015 | ` *   See sprintf() for a description of format.` |
|      - | 5016 | ` * Return` |
|      - | 5017 | ` *  The length of the outputted string.` |
|      - | 5018 | ` */` |
|    206 | 5019 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5020 | `{` |
|    207 | 5021 | `	ph7_int64 nCounter = 0;` |
|      - | 5022 | `	const char *zFormat;` |
|      - | 5023 | `	int nLen;` |
|    207 | 5024 | `	if( nArg < 1 ){` |
|      - | 5025 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 5026 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5027 | `		return PH7_OK;` |
|      - | 5028 | `	}` |
|      - | 5029 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|      - | 5030 | `	{` |
|    207 | 5031 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    207 | 5032 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 5033 | `			return rcf;` |
|      - | 5034 | `		}` |
|      - | 5035 | `	}` |
|      - | 5036 | `	/* Extract the string format (scalars/null coerce). */` |
|    207 | 5037 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    207 | 5038 | `	if( nLen < 1 ){` |
|      - | 5039 | `		/* Empty string */` |
|    ! 0 | 5040 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5041 | `		return PH7_OK;` |
|      - | 5042 | `	}` |
|      - | 5043 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 5044 | `	 * output; propagate the throw status verbatim. */` |
|      - | 5045 | `	{` |
|    207 | 5046 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    207 | 5047 | `		if( rcv != PH7_OK ){` |
|    ! 0 | 5048 | `			return rcv;` |
|      - | 5049 | `		}` |
|      - | 5050 | `		/* PHP 8: too few value arguments is a catchable ArgumentCountError before output. */` |
|    207 | 5051 | `		rcv = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg-1,1,FALSE);` |
|    207 | 5052 | `		if( rcv != PH7_OK ){` |
|      3 | 5053 | `			return rcv;` |
|      - | 5054 | `		}` |
|      - | 5055 | `	}` |
|      - | 5056 | `	/* Format the string */` |
|    205 | 5057 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 5058 | `	/* Return the length of the outputted string */` |
|    205 | 5059 | `	ph7_result_int64(pCtx,nCounter);` |
|    205 | 5060 | `	return PH7_OK;` |
|    104 | 5061 | `}` |
|      - | 5062 | `/*` |
|      - | 5063 | ` * int vprintf(string $format,array $args)` |
|      - | 5064 | ` *  Output a formatted string.` |
|      - | 5065 | ` * Parameters` |
|      - | 5066 | ` *  $format` |
|      - | 5067 | ` *   See sprintf() for a description of format.` |
|      - | 5068 | ` * Return` |
|      - | 5069 | ` *  The length of the outputted string.` |
|      - | 5070 | ` */` |
|      4 | 5071 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5072 | `{` |
|      5 | 5073 | `	ph7_int64 nCounter = 0;` |
|      - | 5074 | `	const char *zFormat;` |
|      - | 5075 | `	ph7_hashmap *pMap;` |
|      - | 5076 | `	SySet sArg;` |
|      - | 5077 | `	int nLen,n;` |
|      - | 5078 | `	sxi32 rcFmt;` |
|      5 | 5079 | `	if( nArg < 2 ){` |
|      - | 5080 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 5081 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5082 | `		return PH7_OK;` |
|      - | 5083 | `	}` |
|      - | 5084 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|      5 | 5085 | `	rcFmt = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|      5 | 5086 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 5087 | `		return rcFmt;` |
|      - | 5088 | `	}` |
|      5 | 5089 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 5090 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 5091 | `		char zBuf[64];` |
|      4 | 5092 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5093 | `			"vprintf(): Argument #2 ($values) must be of type array, %s given",` |
|      2 | 5094 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 5095 | `	}` |
|      - | 5096 | `	/* Extract the string format (scalars/null coerce). */` |
|      3 | 5097 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5098 | `	if( nLen < 1 ){` |
|      - | 5099 | `		/* Empty string */` |
|    ! 0 | 5100 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5101 | `		return PH7_OK;` |
|      - | 5102 | `	}` |
|      - | 5103 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 5104 | `	 * output; propagate the throw status verbatim. */` |
|      3 | 5105 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      3 | 5106 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 5107 | `		return rcFmt;` |
|      - | 5108 | `	}` |
|      - | 5109 | `	/* Point to the hashmap */` |
|      3 | 5110 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 5111 | `	/* PHP 8: too few items in the $values array is a catchable ValueError before output.` |
|      - | 5112 | `	 * Checked on the entry count before materialising the value set. */` |
|      3 | 5113 | `	rcFmt = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);` |
|      3 | 5114 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 5115 | `		return rcFmt;` |
|      - | 5116 | `	}` |
|      - | 5117 | `	/* Extract arguments from the hashmap */` |
|      3 | 5118 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 5119 | `	/* Format the string */` |
|      3 | 5120 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 5121 | `	/* Release the container */` |
|      3 | 5122 | `	SySetRelease(&sArg);` |
|      - | 5123 | `	/* Return the length of the outputted string */` |
|      3 | 5124 | `	ph7_result_int64(pCtx,nCounter);` |
|      3 | 5125 | `	return PH7_OK;` |
|      3 | 5126 | `}` |
|      - | 5127 | `/*` |
|      - | 5128 | ` * int vsprintf(string $format,array $args)` |
|      - | 5129 | ` *  Output a formatted string.` |
|      - | 5130 | ` * Parameters` |
|      - | 5131 | ` *  $format` |
|      - | 5132 | ` *   See sprintf() for a description of format.` |
|      - | 5133 | ` * Return` |
|      - | 5134 | ` *  A string produced according to the formatting string format.` |
|      - | 5135 | ` */` |
|     24 | 5136 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5137 | `{` |
|      - | 5138 | `	const char *zFormat;` |
|      - | 5139 | `	ph7_hashmap *pMap;` |
|      - | 5140 | `	SySet sArg;` |
|     25 | 5141 | `	sxi32 rc = SXRET_OK;` |
|      - | 5142 | `	sxi32 rcFmt;` |
|      - | 5143 | `	int nLen,n;` |
|     25 | 5144 | `	if( nArg < 2 ){` |
|      - | 5145 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5146 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5147 | `		return PH7_OK;` |
|      - | 5148 | `	}` |
|      - | 5149 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|     25 | 5150 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|     25 | 5151 | `	if( rc != PH7_OK ){` |
|    ! 0 | 5152 | `		return rc;` |
|      - | 5153 | `	}` |
|     25 | 5154 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 5155 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 5156 | `		char zBuf[64];` |
|     16 | 5157 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5158 | `			"vsprintf(): Argument #2 ($values) must be of type array, %s given",` |
|     10 | 5159 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 5160 | `	}` |
|      - | 5161 | `	/* Extract the string format (scalars/null coerce). */` |
|     15 | 5162 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 5163 | `	if( nLen < 1 ){` |
|      - | 5164 | `		/* Empty string */` |
|    ! 0 | 5165 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5166 | `		return PH7_OK;` |
|      - | 5167 | `	}` |
|      - | 5168 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 5169 | `	 * output; propagate the throw status verbatim. */` |
|     15 | 5170 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|     15 | 5171 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 5172 | `		return rcFmt;` |
|      - | 5173 | `	}` |
|      - | 5174 | `	/* Point to hashmap */` |
|     15 | 5175 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 5176 | `	/* PHP 8: too few items in the $values array is a catchable ValueError before output. */` |
|     15 | 5177 | `	rcFmt = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);` |
|     15 | 5178 | `	if( rcFmt != PH7_OK ){` |
|      5 | 5179 | `		return rcFmt;` |
|      - | 5180 | `	}` |
|      - | 5181 | `	/* Extract arguments from the hashmap */` |
|     11 | 5182 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 5183 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|     11 | 5184 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&rc,TRUE);` |
|      - | 5185 | `	/* Release the container */` |
|     11 | 5186 | `	SySetRelease(&sArg);` |
|     11 | 5187 | `	if( rc != SXRET_OK ){` |
|      - | 5188 | `		/* The result append ran out of memory: raise a fatal. */` |
|    ! 0 | 5189 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5190 | `	}` |
|     11 | 5191 | `	return PH7_OK;` |
|     13 | 5192 | `}` |
|      - | 5193 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 5194 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 5195 | `/*` |
|      - | 5196 | ` * Symisc eXtension.` |
|      - | 5197 | ` * string size_format(int64 $size)` |
|      - | 5198 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 5199 | ` *  Example:` |
|      - | 5200 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 5201 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 5202 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 5203 | ` * Parameter` |
|      - | 5204 | ` *  $size` |
|      - | 5205 | ` *    Entity size in bytes.` |
|      - | 5206 | ` * Return` |
|      - | 5207 | ` *   Formatted string representation of the given size.` |
|      - | 5208 | ` */` |
|     24 | 5209 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5210 | `{` |
|      - | 5211 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 5212 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 5213 | `	sxi32 nRest,i_32;` |
|      - | 5214 | `	ph7_int64 iSize;` |
|     25 | 5215 | `	int c = -1; /* index in zUnit[] */` |
|      - | 5216 |  |
|     25 | 5217 | `	if( nArg < 1 ){` |
|      - | 5218 | `		/* Missing argument,return the empty string */` |
|      3 | 5219 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5220 | `		return PH7_OK;` |
|      - | 5221 | `	}` |
|      - | 5222 | `	/* Extract the given size */` |
|     23 | 5223 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 5224 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 5225 | `		/* Don't bother formatting,return immediately */` |
|      5 | 5226 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 5227 | `		return PH7_OK;` |
|      - | 5228 | `	}` |
|     19 | 5229 | `	for(;;){` |
|     39 | 5230 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 5231 | `		iSize >>= 10;` |
|     39 | 5232 | `		c++;` |
|     39 | 5233 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 5234 | `			break;` |
|      - | 5235 | `		}` |
|      1 | 5236 | `	}` |
|     19 | 5237 | `	nRest /= 100;` |
|     19 | 5238 | `	if( nRest > 9 ){` |
|    ! 0 | 5239 | `		nRest = 9;` |
|    ! 0 | 5240 | `	}` |
|     19 | 5241 | `	if( iSize > 999 ){` |
|    ! 0 | 5242 | `		c++;` |
|    ! 0 | 5243 | `		nRest = 9;` |
|    ! 0 | 5244 | `		iSize = 0;` |
|    ! 0 | 5245 | `	}` |
|     19 | 5246 | `	i_32 = (sxi32)iSize;` |
|      - | 5247 | `	/* Format */` |
|     19 | 5248 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 5249 | `	return PH7_OK;` |
|     13 | 5250 | `}` |
|      - | 5251 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 5252 | `/*` |
|      - | 5253 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 5254 | ` *   Calculate the md5 hash of a string.` |
|      - | 5255 | ` * Parameter` |
|      - | 5256 | ` *  $str` |
|      - | 5257 | ` *   Input string` |
|      - | 5258 | ` * $raw_output` |
|      - | 5259 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 5260 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 5261 | ` * Return` |
|      - | 5262 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 5263 | ` */` |
|     12 | 5264 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5265 | `{` |
|      - | 5266 | `	unsigned char zDigest[16];` |
|     13 | 5267 | `	int raw_output = FALSE;` |
|      - | 5268 | `	const void *pIn;` |
|      - | 5269 | `	int nLen;` |
|     13 | 5270 | `	if( nArg < 1 ){` |
|      - | 5271 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5272 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5273 | `		return PH7_OK;` |
|      - | 5274 | `	}` |
|      - | 5275 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 5276 | `	 * digest in PHP — d41d8cd9… — so it must NOT short-circuit). */` |
|     13 | 5277 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 5278 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 5279 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 5280 | `	}` |
|      - | 5281 | `	/* Compute the MD5 digest */` |
|     13 | 5282 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|     13 | 5283 | `	if( raw_output ){` |
|      - | 5284 | `		/* Output raw digest */` |
|      5 | 5285 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 5286 | `	}else{` |
|      - | 5287 | `		/* Perform a binary to hex conversion */` |
|      9 | 5288 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 5289 | `	}` |
|     13 | 5290 | `	return PH7_OK;` |
|      7 | 5291 | `}` |
|      - | 5292 | `/*` |
|      - | 5293 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 5294 | ` *   Calculate the sha1 hash of a string.` |
|      - | 5295 | ` * Parameter` |
|      - | 5296 | ` *  $str` |
|      - | 5297 | ` *   Input string` |
|      - | 5298 | ` * $raw_output` |
|      - | 5299 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 5300 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 5301 | ` * Return` |
|      - | 5302 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 5303 | ` */` |
|     10 | 5304 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5305 | `{` |
|      - | 5306 | `	unsigned char zDigest[20];` |
|     11 | 5307 | `	int raw_output = FALSE;` |
|      - | 5308 | `	const void *pIn;` |
|      - | 5309 | `	int nLen;` |
|     11 | 5310 | `	if( nArg < 1 ){` |
|      - | 5311 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5312 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5313 | `		return PH7_OK;` |
|      - | 5314 | `	}` |
|      - | 5315 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 5316 | `	 * digest in PHP — da39a3ee… — so it must NOT short-circuit). */` |
|     11 | 5317 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 5318 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 5319 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 5320 | `	}` |
|      - | 5321 | `	/* Compute the SHA1 digest */` |
|     11 | 5322 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|     11 | 5323 | `	if( raw_output ){` |
|      - | 5324 | `		/* Output raw digest */` |
|      5 | 5325 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 5326 | `	}else{` |
|      - | 5327 | `		/* Perform a binary to hex conversion */` |
|      7 | 5328 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 5329 | `	}` |
|     11 | 5330 | `	return PH7_OK;` |
|      6 | 5331 | `}` |
|      - | 5332 | `/*` |
|      - | 5333 | ` * int64 crc32(string $str)` |
|      - | 5334 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 5335 | ` * Parameter` |
|      - | 5336 | ` *  $str` |
|      - | 5337 | ` *   Input string` |
|      - | 5338 | ` * Return` |
|      - | 5339 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 5340 | ` */` |
|      2 | 5341 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5342 | `{` |
|      - | 5343 | `	const void *pIn;` |
|      - | 5344 | `	sxu32 nCRC;` |
|      - | 5345 | `	int nLen;` |
|      3 | 5346 | `	if( nArg < 1 ){` |
|      - | 5347 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 5348 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5349 | `		return PH7_OK;` |
|      - | 5350 | `	}` |
|      - | 5351 | `	/* Extract the input string */` |
|      3 | 5352 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5353 | `	if( nLen < 1 ){` |
|      - | 5354 | `		/* crc32("") is 0 in PHP, so this short-circuit is correct here — unlike` |
|      - | 5355 | `		 * md5()/sha1(), whose empty-string digests are non-zero. */` |
|    ! 0 | 5356 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5357 | `		return PH7_OK;` |
|      - | 5358 | `	}` |
|      - | 5359 | `	/* Calculate the sum */` |
|      3 | 5360 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 5361 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 5362 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 5363 | `	return PH7_OK;` |
|      2 | 5364 | `}` |
|      - | 5365 | `/*` |
|      - | 5366 | ` * The hash() family (hash/hash_hmac/hash_equals/hash_algos). Each algorithm is` |
|      - | 5367 | ` * described by a small record so one dispatch (and one generic HMAC) serves them` |
|      - | 5368 | ` * all. Thin adapters normalize the differing context types and the reversed` |
|      - | 5369 | ` * MD5Final argument order behind a uniform Init/Update/Final over a HashCtx union.` |
|      - | 5370 | ` */` |
|     11 | 5371 | `static void HashMd5Init(HashCtx *c){ MD5Init(&c->md5); }` |
|     15 | 5372 | `static void HashMd5Update(HashCtx *c,const unsigned char *d,unsigned int n){ MD5Update(&c->md5,d,n); }` |
|     11 | 5373 | `static void HashMd5Final(HashCtx *c,unsigned char *o){ MD5Final(o,&c->md5); }` |
|     11 | 5374 | `static void HashSha1Init(HashCtx *c){ SHA1Init(&c->sha1); }` |
|     15 | 5375 | `static void HashSha1Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA1Update(&c->sha1,d,n); }` |
|     11 | 5376 | `static void HashSha1Final(HashCtx *c,unsigned char *o){ SHA1Final(&c->sha1,o); }` |
|      9 | 5377 | `static void HashSha224Init(HashCtx *c){ SHA224Init(&c->sha256); }` |
|     33 | 5378 | `static void HashSha256Init(HashCtx *c){ SHA256Init(&c->sha256); }` |
|     57 | 5379 | `static void HashSha256Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA256Update(&c->sha256,d,n); }` |
|     41 | 5380 | `static void HashSha256Final(HashCtx *c,unsigned char *o){ SHA256Final(&c->sha256,o); }` |
|      9 | 5381 | `static void HashSha384Init(HashCtx *c){ SHA384Init(&c->sha512); }` |
|     15 | 5382 | `static void HashSha512Init(HashCtx *c){ SHA512Init(&c->sha512); }` |
|     27 | 5383 | `static void HashSha512Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA512Update(&c->sha512,d,n); }` |
|     23 | 5384 | `static void HashSha512Final(HashCtx *c,unsigned char *o){ SHA512Final(&c->sha512,o); }` |
|      - | 5385 | `typedef struct HashAlgo HashAlgo;` |
|      - | 5386 | `struct HashAlgo {` |
|      - | 5387 | `	const char *zName;   /* lowercase canonical name */` |
|      - | 5388 | `	int nDigestLen;      /* output bytes: 16/20/28/32/48/64 */` |
|      - | 5389 | `	int nBlockLen;       /* internal block bytes (for HMAC): 64 or 128 */` |
|      - | 5390 | `	void (*xInit)(HashCtx *);` |
|      - | 5391 | `	void (*xUpdate)(HashCtx *,const unsigned char *,unsigned int);` |
|      - | 5392 | `	void (*xFinal)(HashCtx *,unsigned char *);` |
|      - | 5393 | `};` |
|      - | 5394 | `static const HashAlgo aHashAlgo[] = {` |
|      - | 5395 | `	{ "md5",    16, 64,  HashMd5Init,    HashMd5Update,    HashMd5Final    },` |
|      - | 5396 | `	{ "sha1",   20, 64,  HashSha1Init,   HashSha1Update,   HashSha1Final   },` |
|      - | 5397 | `	{ "sha224", 28, 64,  HashSha224Init, HashSha256Update, HashSha256Final },` |
|      - | 5398 | `	{ "sha256", 32, 64,  HashSha256Init, HashSha256Update, HashSha256Final },` |
|      - | 5399 | `	{ "sha384", 48, 128, HashSha384Init, HashSha512Update, HashSha512Final },` |
|      - | 5400 | `	{ "sha512", 64, 128, HashSha512Init, HashSha512Update, HashSha512Final },` |
|      - | 5401 | `};` |
|      - | 5402 | `/* Case-insensitive algorithm lookup (PHP accepts 'SHA256' etc.). */` |
|     73 | 5403 | `static const HashAlgo * HashFindAlgo(const char *zName,int nLen){` |
|      - | 5404 | `	sxu32 i;` |
|    279 | 5405 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|    272 | 5406 | `		if( (int)SyStrlen(aHashAlgo[i].zName) == nLen` |
|    211 | 5407 | `			&& SyStrnicmp(aHashAlgo[i].zName,zName,(sxu32)nLen) == 0 ){` |
|     67 | 5408 | `			return &aHashAlgo[i];` |
|      - | 5409 | `		}` |
|    106 | 5410 | `	}` |
|      6 | 5411 | `	return 0;` |
|     38 | 5412 | `}` |
|      - | 5413 | `/*` |
|      - | 5414 | ` * string hash(string $algo,string $data[,bool $binary = false])` |
|      - | 5415 | ` *   Generate a hash value (message digest).` |
|      - | 5416 | ` */` |
|     54 | 5417 | `static int PH7_builtin_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5418 | `{` |
|      - | 5419 | `	const HashAlgo *pAlgo;` |
|      - | 5420 | `	const char *zAlgo,*zData;` |
|     56 | 5421 | `	int nAlgoLen,nDataLen,raw_output = FALSE;` |
|      - | 5422 | `	HashCtx sCtx;` |
|      - | 5423 | `	unsigned char zDigest[64];` |
|     56 | 5424 | `	if( nArg < 2 ){` |
|    ! 0 | 5425 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5426 | `			"hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5427 | `	}` |
|     56 | 5428 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     56 | 5429 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     56 | 5430 | `	if( pAlgo == 0 ){` |
|      3 | 5431 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5432 | `			"hash(): Argument #1 ($algo) must be a valid hashing algorithm");` |
|      - | 5433 | `	}` |
|     53 | 5434 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     53 | 5435 | `	if( nArg > 2 ){` |
|      9 | 5436 | `		raw_output = ph7_value_to_bool(apArg[2]);` |
|      4 | 5437 | `	}` |
|     53 | 5438 | `	pAlgo->xInit(&sCtx);` |
|     53 | 5439 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     53 | 5440 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     53 | 5441 | `	if( raw_output ){` |
|      9 | 5442 | `		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);` |
|      5 | 5443 | `	}else{` |
|     45 | 5444 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);` |
|      - | 5445 | `	}` |
|     53 | 5446 | `	return PH7_OK;` |
|     29 | 5447 | `}` |
|      - | 5448 | `/*` |
|      - | 5449 | ` * string hash_hmac(string $algo,string $data,string $key[,bool $binary = false])` |
|      - | 5450 | ` *   Generate a keyed hash value using the HMAC method (RFC 2104).` |
|      - | 5451 | ` */` |
|     16 | 5452 | `static int PH7_builtin_hash_hmac(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5453 | `{` |
|      - | 5454 | `	const HashAlgo *pAlgo;` |
|      - | 5455 | `	const char *zAlgo,*zData,*zKey;` |
|     18 | 5456 | `	int nAlgoLen,nDataLen,nKeyLen,raw_output = FALSE;` |
|      - | 5457 | `	HashCtx sCtx;` |
|      - | 5458 | `	unsigned char zKeyBlock[128],zIpad[128],zOpad[128],zInner[64],zDigest[64];` |
|      - | 5459 | `	int i,nBlock,nDigest;` |
|     18 | 5460 | `	if( nArg < 3 ){` |
|    ! 0 | 5461 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5462 | `			"hash_hmac() expects at least 3 arguments, %d given",nArg);` |
|      - | 5463 | `	}` |
|     18 | 5464 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     18 | 5465 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     18 | 5466 | `	if( pAlgo == 0 ){` |
|      3 | 5467 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5468 | `			"hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");` |
|      - | 5469 | `	}` |
|     15 | 5470 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     15 | 5471 | `	zKey = ph7_value_to_string(apArg[2],&nKeyLen);` |
|     15 | 5472 | `	if( nArg > 3 ){` |
|      3 | 5473 | `		raw_output = ph7_value_to_bool(apArg[3]);` |
|      1 | 5474 | `	}` |
|     15 | 5475 | `	nBlock = pAlgo->nBlockLen;` |
|     15 | 5476 | `	nDigest = pAlgo->nDigestLen;` |
|      - | 5477 | `	/* Reduce the key to a single block: hash it if longer than the block, then` |
|      - | 5478 | `	 * zero-pad (a short or empty key is just zero-padded). */` |
|     15 | 5479 | `	SyZero(zKeyBlock,sizeof(zKeyBlock));` |
|     15 | 5480 | `	if( nKeyLen > nBlock ){` |
|      3 | 5481 | `		pAlgo->xInit(&sCtx);` |
|      3 | 5482 | `		pAlgo->xUpdate(&sCtx,(const unsigned char *)zKey,(unsigned int)nKeyLen);` |
|      3 | 5483 | `		pAlgo->xFinal(&sCtx,zKeyBlock);` |
|     14 | 5484 | `	}else if( nKeyLen > 0 ){` |
|     11 | 5485 | `		SyMemcpy(zKey,zKeyBlock,(sxu32)nKeyLen);` |
|      5 | 5486 | `	}` |
|   1039 | 5487 | `	for( i = 0; i < nBlock; i++ ){` |
|   1025 | 5488 | `		zIpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x36);` |
|   1025 | 5489 | `		zOpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x5c);` |
|    513 | 5490 | `	}` |
|      - | 5491 | `	/* inner = H((key ^ ipad) \|\| data) */` |
|     15 | 5492 | `	pAlgo->xInit(&sCtx);` |
|     15 | 5493 | `	pAlgo->xUpdate(&sCtx,zIpad,(unsigned int)nBlock);` |
|     15 | 5494 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     15 | 5495 | `	pAlgo->xFinal(&sCtx,zInner);` |
|      - | 5496 | `	/* out = H((key ^ opad) \|\| inner) */` |
|     15 | 5497 | `	pAlgo->xInit(&sCtx);` |
|     15 | 5498 | `	pAlgo->xUpdate(&sCtx,zOpad,(unsigned int)nBlock);` |
|     15 | 5499 | `	pAlgo->xUpdate(&sCtx,zInner,(unsigned int)nDigest);` |
|     15 | 5500 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     15 | 5501 | `	if( raw_output ){` |
|      3 | 5502 | `		ph7_result_string(pCtx,(const char *)zDigest,nDigest);` |
|      2 | 5503 | `	}else{` |
|     13 | 5504 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)nDigest,HashConsumer,pCtx);` |
|      - | 5505 | `	}` |
|     15 | 5506 | `	return PH7_OK;` |
|     10 | 5507 | `}` |
|      - | 5508 | `/*` |
|      - | 5509 | ` * bool hash_equals(string $known_string,string $user_string)` |
|      - | 5510 | ` *   Timing-attack-safe string comparison.` |
|      - | 5511 | ` */` |
|     12 | 5512 | `static int PH7_builtin_hash_equals(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5513 | `{` |
|      - | 5514 | `	const char *zKnown,*zUser;` |
|      - | 5515 | `	int nKnown,nUser,i;` |
|     14 | 5516 | `	volatile unsigned char vDiff = 0;` |
|     14 | 5517 | `	if( nArg < 2 ){` |
|    ! 0 | 5518 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5519 | `			"hash_equals() expects exactly 2 arguments, %d given",nArg);` |
|      - | 5520 | `	}` |
|     14 | 5521 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      4 | 5522 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5523 | `			"hash_equals(): Argument #1 ($known_string) must be of type string, %s given",` |
|      1 | 5524 | `			ph7_type_name(apArg[0]));` |
|      - | 5525 | `	}` |
|     11 | 5526 | `	if( !ph7_value_is_string(apArg[1]) ){` |
|    ! 0 | 5527 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5528 | `			"hash_equals(): Argument #2 ($user_string) must be of type string, %s given",` |
|    ! 0 | 5529 | `			ph7_type_name(apArg[1]));` |
|      - | 5530 | `	}` |
|     11 | 5531 | `	zKnown = ph7_value_to_string(apArg[0],&nKnown);` |
|     11 | 5532 | `	zUser = ph7_value_to_string(apArg[1],&nUser);` |
|     11 | 5533 | `	if( nKnown != nUser ){` |
|      5 | 5534 | `		ph7_result_bool(pCtx,0);` |
|      5 | 5535 | `		return PH7_OK;` |
|      - | 5536 | `	}` |
|      - | 5537 | `	/* Constant-time: read every byte, never short-circuit. */` |
|     19 | 5538 | `	for( i = 0; i < nKnown; i++ ){` |
|     13 | 5539 | `		vDiff \|= (unsigned char)(zKnown[i] ^ zUser[i]);` |
|      7 | 5540 | `	}` |
|      7 | 5541 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|      7 | 5542 | `	return PH7_OK;` |
|      8 | 5543 | `}` |
|      - | 5544 | `/*` |
|      - | 5545 | ` * array hash_algos(void)` |
|      - | 5546 | ` *   Return a list of the registered hashing algorithms.` |
|      - | 5547 | ` */` |
|      2 | 5548 | `static int PH7_builtin_hash_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5549 | `{` |
|      - | 5550 | `	ph7_value *pArray,*pValue;` |
|      - | 5551 | `	sxu32 i;` |
|      1 | 5552 | `	SXUNUSED(nArg);` |
|      1 | 5553 | `	SXUNUSED(apArg);` |
|      3 | 5554 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 5555 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 5556 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 5557 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5558 | `		return PH7_OK;` |
|      - | 5559 | `	}` |
|     15 | 5560 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|     13 | 5561 | `		ph7_value_string(pValue,aHashAlgo[i].zName,-1);` |
|     13 | 5562 | `		ph7_array_add_elem(pArray,0 /* Automatic 0-based index */,pValue);` |
|     13 | 5563 | `		ph7_value_reset_string_cursor(pValue);` |
|      7 | 5564 | `	}` |
|      3 | 5565 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5566 | `	return PH7_OK;` |
|      2 | 5567 | `}` |
|      - | 5568 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 5569 | `/*` |
|      - | 5570 | ` * password_* (bcrypt). These live in ext/standard in real PHP — outside the` |
|      - | 5571 | ` * hash extension — so they are NOT guarded by PH7_DISABLE_HASH_FUNC.` |
|      - | 5572 | ` */` |
|      - | 5573 | `/*` |
|      - | 5574 | ` * Parse a bcrypt crypt string. Returns TRUE and fills *piCost when zHash is a` |
|      - | 5575 | ` * well-formed "$2?$NN$"+53-char bcrypt hash (60 bytes, valid minor, cost 4..31).` |
|      - | 5576 | ` */` |
|     40 | 5577 | `static int BcryptParseHash(const char *zHash,int nHash,int *piCost)` |
|      1 | 5578 | `{` |
|      - | 5579 | `	int iCost;` |
|     40 | 5580 | `	if( nHash != 60 \|\| zHash[0] != '$' \|\| zHash[1] != '2' \|\| zHash[3] != '$'` |
|     29 | 5581 | `		\|\| (zHash[2] != 'a' && zHash[2] != 'b' && zHash[2] != 'x' && zHash[2] != 'y') ){` |
|     13 | 5582 | `		return FALSE;` |
|      - | 5583 | `	}` |
|     29 | 5584 | `	if( zHash[4] < '0' \|\| zHash[4] > '9' \|\| zHash[5] < '0' \|\| zHash[5] > '9' \|\| zHash[6] != '$' ){` |
|    ! 0 | 5585 | `		return FALSE;` |
|      - | 5586 | `	}` |
|     29 | 5587 | `	iCost = (zHash[4]-'0')*10 + (zHash[5]-'0');` |
|     29 | 5588 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      3 | 5589 | `		return FALSE;` |
|      - | 5590 | `	}` |
|     27 | 5591 | `	if( piCost ){ *piCost = iCost; }` |
|     27 | 5592 | `	return TRUE;` |
|     21 | 5593 | `}` |
|      - | 5594 | `/*` |
|      - | 5595 | ` * TRUE if the $algo argument selects bcrypt: null (PASSWORD_DEFAULT) or the` |
|      - | 5596 | ` * "2y" id (PASSWORD_BCRYPT/PASSWORD_DEFAULT). bcrypt is the only supported algo.` |
|      - | 5597 | ` */` |
|     20 | 5598 | `static int BcryptIsBcryptAlgo(ph7_value *pAlgo)` |
|      3 | 5599 | `{` |
|     23 | 5600 | `	if( ph7_value_is_null(pAlgo) ){` |
|    ! 0 | 5601 | `		return TRUE;` |
|      - | 5602 | `	}` |
|     23 | 5603 | `	if( ph7_value_is_string(pAlgo) ){` |
|      - | 5604 | `		int nAlgo;` |
|     23 | 5605 | `		const char *zAlgo = ph7_value_to_string(pAlgo,&nAlgo);` |
|     23 | 5606 | `		return ( nAlgo == 2 && zAlgo[0] == '2' && zAlgo[1] == 'y' );` |
|      - | 5607 | `	}` |
|    ! 0 | 5608 | `	return FALSE;` |
|     13 | 5609 | `}` |
|      - | 5610 | `/*` |
|      - | 5611 | ` * bool\|string password_hash(string $password,string\|int\|null $algo[,array $options])` |
|      - | 5612 | ` *  Create a bcrypt hash of the password.` |
|      - | 5613 | ` */` |
|     16 | 5614 | `static int PH7_builtin_password_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 5615 | `{` |
|      - | 5616 | `	const char *zPwd;` |
|     19 | 5617 | `	int nPwd,iCost = 12;` |
|      - | 5618 | `	unsigned char aSalt[16];` |
|      - | 5619 | `	char zHash[60];` |
|     19 | 5620 | `	if( nArg < 2 ){` |
|    ! 0 | 5621 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5622 | `			"password_hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5623 | `	}` |
|     19 | 5624 | `	if( !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      3 | 5625 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5626 | `			"password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm");` |
|      - | 5627 | `	}` |
|      - | 5628 | `	/* cost from $options['cost'] (default 12). */` |
|     16 | 5629 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|     14 | 5630 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|     14 | 5631 | `		if( pCost ){ iCost = ph7_value_to_int(pCost); }` |
|      6 | 5632 | `	}` |
|     16 | 5633 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      4 | 5634 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 5635 | `			"Invalid bcrypt cost parameter specified: %d",iCost);` |
|      - | 5636 | `	}` |
|     13 | 5637 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     13 | 5638 | `	if( SyOSCSPRNG(aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 5639 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - | 5640 | `			"password_hash(): unable to gather sufficient entropy for the salt");` |
|      - | 5641 | `	}` |
|     13 | 5642 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zHash) != SXRET_OK ){` |
|    ! 0 | 5643 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5644 | `		return PH7_OK;` |
|      - | 5645 | `	}` |
|     13 | 5646 | `	ph7_result_string(pCtx,zHash,(int)sizeof(zHash));` |
|     13 | 5647 | `	return PH7_OK;` |
|     11 | 5648 | `}` |
|      - | 5649 | `/*` |
|      - | 5650 | ` * bool password_verify(string $password,string $hash)` |
|      - | 5651 | ` *  Verify a password against a bcrypt hash. Never throws on a malformed hash.` |
|      - | 5652 | ` */` |
|     28 | 5653 | `static int PH7_builtin_password_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5654 | `{` |
|      - | 5655 | `	const char *zPwd,*zHash;` |
|      - | 5656 | `	int nPwd,nHash,iCost,i;` |
|      - | 5657 | `	unsigned char aSalt[16];` |
|      - | 5658 | `	char zComputed[60];` |
|     29 | 5659 | `	volatile unsigned char vDiff = 0;` |
|     29 | 5660 | `	if( nArg < 2 ){` |
|    ! 0 | 5661 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5662 | `			"password_verify() expects exactly 2 arguments, %d given",nArg);` |
|      - | 5663 | `	}` |
|     29 | 5664 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     29 | 5665 | `	zHash = ph7_value_to_string(apArg[1],&nHash);` |
|     29 | 5666 | `	if( !BcryptParseHash(zHash,nHash,&iCost) ){` |
|     11 | 5667 | `		ph7_result_bool(pCtx,0);` |
|     11 | 5668 | `		return PH7_OK;` |
|      - | 5669 | `	}` |
|      - | 5670 | `	/* Recover the 16 salt bytes from the 22-char salt field [7..28]. */` |
|     19 | 5671 | `	if( SyBcryptB64Decode(&zHash[7],22,aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 5672 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5673 | `		return PH7_OK;` |
|      - | 5674 | `	}` |
|     19 | 5675 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zComputed) != SXRET_OK ){` |
|    ! 0 | 5676 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5677 | `		return PH7_OK;` |
|      - | 5678 | `	}` |
|      - | 5679 | `	/* Constant-time compare of the 31-char hash field [29..59] only — sidesteps` |
|      - | 5680 | `	 * salt re-canonicalisation and any "$2a"/"$2y" prefix difference. */` |
|    577 | 5681 | `	for( i = 29; i < 60; i++ ){` |
|    559 | 5682 | `		vDiff \|= (unsigned char)(zComputed[i] ^ zHash[i]);` |
|    280 | 5683 | `	}` |
|     19 | 5684 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|     19 | 5685 | `	return PH7_OK;` |
|     15 | 5686 | `}` |
|      - | 5687 | `/*` |
|      - | 5688 | ` * array password_get_info(string $hash)` |
|      - | 5689 | ` *  Return ["algo"=>id\|null, "algoName"=>name, "options"=>[...]].` |
|      - | 5690 | ` */` |
|      6 | 5691 | `static int PH7_builtin_password_get_info(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5692 | `{` |
|      7 | 5693 | `	const char *zHash = "";` |
|      7 | 5694 | `	int nHash,iCost = 0,bBcrypt = 0;` |
|      - | 5695 | `	ph7_value *pArray,*pOptions,*pVal;` |
|      7 | 5696 | `	if( nArg > 0 ){` |
|      7 | 5697 | `		zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 5698 | `		bBcrypt = BcryptParseHash(zHash,nHash,&iCost);` |
|      3 | 5699 | `	}` |
|      7 | 5700 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 5701 | `	pOptions = ph7_context_new_array(pCtx);` |
|      7 | 5702 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      7 | 5703 | `	if( pArray == 0 \|\| pOptions == 0 \|\| pVal == 0 ){` |
|    ! 0 | 5704 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5705 | `		return PH7_OK;` |
|      - | 5706 | `	}` |
|      7 | 5707 | `	if( bBcrypt ){` |
|      5 | 5708 | `		ph7_value_string(pVal,&zHash[1],2);            /* algo "2y"/"2a" */` |
|      5 | 5709 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      5 | 5710 | `		ph7_value_reset_string_cursor(pVal);` |
|      5 | 5711 | `		ph7_value_string(pVal,"bcrypt",(int)sizeof("bcrypt")-1);` |
|      5 | 5712 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      5 | 5713 | `		ph7_value_int(pVal,iCost);` |
|      5 | 5714 | `		ph7_array_add_strkey_elem(pOptions,"cost",pVal);` |
|      3 | 5715 | `	}else{` |
|      3 | 5716 | `		ph7_value_null(pVal);                          /* algo => null */` |
|      3 | 5717 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      3 | 5718 | `		ph7_value_string(pVal,"unknown",(int)sizeof("unknown")-1);` |
|      3 | 5719 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      - | 5720 | `	}` |
|      7 | 5721 | `	ph7_array_add_strkey_elem(pArray,"options",pOptions);` |
|      7 | 5722 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 5723 | `	return PH7_OK;` |
|      4 | 5724 | `}` |
|      - | 5725 | `/*` |
|      - | 5726 | ` * bool password_needs_rehash(string $hash,string\|int\|null $algo[,array $options])` |
|      - | 5727 | ` *  True if the hash was not made with the given algo/options.` |
|      - | 5728 | ` */` |
|      6 | 5729 | `static int PH7_builtin_password_needs_rehash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5730 | `{` |
|      - | 5731 | `	const char *zHash;` |
|      7 | 5732 | `	int nHash,iCost = 0,iWantCost = 12;` |
|      7 | 5733 | `	if( nArg < 2 ){` |
|    ! 0 | 5734 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5735 | `			"password_needs_rehash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5736 | `	}` |
|      7 | 5737 | `	zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 5738 | `	if( !BcryptParseHash(zHash,nHash,&iCost) \|\| !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      - | 5739 | `		/* A non-bcrypt hash, or a request for a different algo → needs rehash. */` |
|      3 | 5740 | `		ph7_result_bool(pCtx,1);` |
|      3 | 5741 | `		return PH7_OK;` |
|      - | 5742 | `	}` |
|      5 | 5743 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|      5 | 5744 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|      5 | 5745 | `		if( pCost ){ iWantCost = ph7_value_to_int(pCost); }` |
|      2 | 5746 | `	}` |
|      5 | 5747 | `	ph7_result_bool(pCtx,iCost != iWantCost);` |
|      5 | 5748 | `	return PH7_OK;` |
|      4 | 5749 | `}` |
|      - | 5750 | `/*` |
|      - | 5751 | ` * filter_var() — input validation and sanitization (the ext/filter API).` |
|      - | 5752 | ` *` |
|      - | 5753 | ` * Filter and flag identifiers (values match PHP 8.5; the constants themselves` |
|      - | 5754 | ` * are registered in constant.c). The validate filters are hand-rolled rather` |
|      - | 5755 | ` * than delegating to SyStrToInt64/SyStrToReal: the former silently skips leading` |
|      - | 5756 | ` * zeros and cannot signal overflow, and the latter treats ',' as a decimal point` |
|      - | 5757 | ` * unconditionally — neither matches PHP's filter semantics.` |
|      - | 5758 | ` */` |
|      - | 5759 | `#define FV_VALIDATE_INT     257` |
|      - | 5760 | `#define FV_VALIDATE_BOOLEAN 258` |
|      - | 5761 | `#define FV_VALIDATE_FLOAT   259` |
|      - | 5762 | `#define FV_VALIDATE_REGEXP  272` |
|      - | 5763 | `#define FV_VALIDATE_URL     273` |
|      - | 5764 | `#define FV_VALIDATE_EMAIL   274` |
|      - | 5765 | `#define FV_VALIDATE_IP      275` |
|      - | 5766 | `#define FV_VALIDATE_MAC     276` |
|      - | 5767 | `#define FV_VALIDATE_DOMAIN  277` |
|      - | 5768 | `#define FV_SANITIZE_SPECIAL_CHARS      515` |
|      - | 5769 | `#define FV_DEFAULT          516 /* == FILTER_UNSAFE_RAW: pass the value through */` |
|      - | 5770 | `#define FV_SANITIZE_EMAIL   517` |
|      - | 5771 | `#define FV_SANITIZE_URL     518` |
|      - | 5772 | `#define FV_SANITIZE_NUMBER_INT   519` |
|      - | 5773 | `#define FV_SANITIZE_NUMBER_FLOAT 520` |
|      - | 5774 | `#define FV_SANITIZE_FULL_SPECIAL_CHARS 522` |
|      - | 5775 | `#define FV_FLAG_ALLOW_OCTAL  1` |
|      - | 5776 | `#define FV_FLAG_ALLOW_HEX    2` |
|      - | 5777 | `#define FV_FLAG_STRIP_LOW    4` |
|      - | 5778 | `#define FV_FLAG_STRIP_HIGH   8` |
|      - | 5779 | `#define FV_FLAG_ENCODE_LOW   16` |
|      - | 5780 | `#define FV_FLAG_ENCODE_HIGH  32` |
|      - | 5781 | `#define FV_FLAG_ENCODE_AMP   64` |
|      - | 5782 | `#define FV_FLAG_NO_ENCODE_QUOTES 128` |
|      - | 5783 | `#define FV_FLAG_STRIP_BACKTICK   512` |
|      - | 5784 | `#define FV_FLAG_ALLOW_FRACTION   4096` |
|      - | 5785 | `#define FV_FLAG_ALLOW_THOUSAND   8192` |
|      - | 5786 | `#define FV_FLAG_ALLOW_SCIENTIFIC 16384` |
|      - | 5787 | `#define FV_FLAG_IPV4  1048576` |
|      - | 5788 | `#define FV_FLAG_IPV6  2097152` |
|      - | 5789 | `#define FV_NULL_ON_FAILURE 134217728` |
|      - | 5790 | `/* The subset of flags the UNSAFE_RAW/DEFAULT string filter (FvSanitizeString)` |
|      - | 5791 | ` * acts on: when none are set the filter is a verbatim pass-through, so FV_DEFAULT` |
|      - | 5792 | ` * can shortcut. Keep this in sync with FvSanitizeString's flag handling. */` |
|      - | 5793 | `#define FV_FLAG_STRING_MASK (FV_FLAG_STRIP_LOW\|FV_FLAG_STRIP_HIGH\|FV_FLAG_STRIP_BACKTICK \` |
|      - | 5794 | `                            \|FV_FLAG_ENCODE_LOW\|FV_FLAG_ENCODE_HIGH\|FV_FLAG_ENCODE_AMP)` |
|      - | 5795 |  |
|      - | 5796 | `/* Trim leading/trailing PHP whitespace, adjusting the (*pz,*pn) view in place.` |
|      - | 5797 | ` * SyisSpace (isspace) matches PHP's filter whitespace set " \t\n\r\v\f". */` |
|    153 | 5798 | `static void FvTrim(const char **pz,int *pn){` |
|    153 | 5799 | `	const char *z = *pz;` |
|    153 | 5800 | `	int n = *pn;` |
|    157 | 5801 | `	while( n>0 && SyisSpace((unsigned char)z[0]) ){ z++; n--; }` |
|    161 | 5802 | `	while( n>0 && SyisSpace((unsigned char)z[n-1]) ){ n--; }` |
|    153 | 5803 | `	*pz = z; *pn = n;` |
|    153 | 5804 | `}` |
|      - | 5805 | `/* FILTER_VALIDATE_INT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     57 | 5806 | `static int FvValidateInt(const char *z,int n,int flags,ph7_int64 *pOut){` |
|     57 | 5807 | `	int neg = 0, i;` |
|     57 | 5808 | `	sxu64 u = 0;` |
|     57 | 5809 | `	FvTrim(&z,&n);` |
|     57 | 5810 | `	if( n==0 ){ return 0; }` |
|     51 | 5811 | `	if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; n--; }` |
|     51 | 5812 | `	if( n==0 ){ return 0; }` |
|     49 | 5813 | `	if( (flags & FV_FLAG_ALLOW_HEX) && n>=2 && z[0]=='0' && (z[1]=='x'\|\|z[1]=='X') ){` |
|      3 | 5814 | `		z += 2; n -= 2;` |
|      3 | 5815 | `		if( n==0 ){ return 0; }` |
|      7 | 5816 | `		for( i=0; i<n; i++ ){` |
|      5 | 5817 | `			int h = SyHexToint((unsigned char)z[i]);` |
|      5 | 5818 | `			if( h<0 ){ return 0; }` |
|      5 | 5819 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)h)/16 ){ return 0; }` |
|      5 | 5820 | `			u = u*16 + (sxu64)h;` |
|      3 | 5821 | `		}` |
|     48 | 5822 | `	}else if( (flags & FV_FLAG_ALLOW_OCTAL) && z[0]=='0' ){` |
|      9 | 5823 | `		for( i=0; i<n; i++ ){` |
|      7 | 5824 | `			if( z[i]<'0' \|\| z[i]>'7' ){ return 0; }` |
|      7 | 5825 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/8 ){ return 0; }` |
|      7 | 5826 | `			u = u*8 + (sxu64)(z[i]-'0');` |
|      4 | 5827 | `		}` |
|      2 | 5828 | `	}else{` |
|     45 | 5829 | `		if( z[0]=='0' && n>1 ){ return 0; } /* a leading zero is rejected in base 10 */` |
|    201 | 5830 | `		for( i=0; i<n; i++ ){` |
|    173 | 5831 | `			if( !SyisDigit((unsigned char)z[i]) ){ return 0; }` |
|    161 | 5832 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/10 ){ return 0; }` |
|    161 | 5833 | `			u = u*10 + (sxu64)(z[i]-'0');` |
|     81 | 5834 | `		}` |
|      - | 5835 | `	}` |
|     33 | 5836 | `	if( neg ){` |
|      5 | 5837 | `		if( u > 0x8000000000000000ULL ){ return 0; }` |
|      5 | 5838 | `		*pOut = (ph7_int64)(0ULL - u); /* two's-complement negate in unsigned space */` |
|      3 | 5839 | `	}else{` |
|     29 | 5840 | `		if( u > 0x7FFFFFFFFFFFFFFFULL ){ return 0; }` |
|     27 | 5841 | `		*pOut = (ph7_int64)u;` |
|      - | 5842 | `	}` |
|     31 | 5843 | `	return 1;` |
|     29 | 5844 | `}` |
|      - | 5845 | `/* FILTER_VALIDATE_FLOAT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     69 | 5846 | `static int FvValidateFloat(const char *z,int n,int flags,double *pOut){` |
|      - | 5847 | `	char zBuf[512];` |
|     69 | 5848 | `	int i, m = 0, seenDigit = 0;` |
|     69 | 5849 | `	const char *zv; int nv; double d = 0;` |
|     69 | 5850 | `	FvTrim(&z,&n);` |
|      - | 5851 | `	/* Bound the input: zBuf[512] holds the thousand-separator-stripped copy, and` |
|      - | 5852 | `	 * the cap also rejects the pathological 500+ digit floats PHP refuses. */` |
|     69 | 5853 | `	if( n==0 \|\| n>500 ){ return 0; }` |
|     69 | 5854 | `	if( flags & FV_FLAG_ALLOW_THOUSAND ){` |
|      - | 5855 | `		/* Commas are optional, but when present they must group the integer part` |
|      - | 5856 | `		 * into a leading run of 1..3 digits followed by groups of exactly 3` |
|      - | 5857 | `		 * ("1,000" ok, "1,5"/"1234,567" rejected). Strip them into zBuf and reject` |
|      - | 5858 | `		 * a comma anywhere in the fractional/exponent tail. */` |
|     25 | 5859 | `		int s = 0, intEnd, segStart, segIdx, hasComma = 0;` |
|     25 | 5860 | `		if( s<n && (z[s]=='+'\|\|z[s]=='-') ){ zBuf[m++] = z[s]; s++; }` |
|     25 | 5861 | `		intEnd = s;` |
|    167 | 5862 | `		while( intEnd<n && z[intEnd]!='.' && z[intEnd]!='e' && z[intEnd]!='E' ){` |
|    143 | 5863 | `			if( z[intEnd]==',' ){ hasComma = 1; }` |
|    143 | 5864 | `			intEnd++;` |
|      1 | 5865 | `		}` |
|     25 | 5866 | `		if( hasComma ){` |
|     25 | 5867 | `			segStart = s; segIdx = 0;` |
|    165 | 5868 | `			for( i=s; i<=intEnd; i++ ){` |
|    151 | 5869 | `				if( i==intEnd \|\| z[i]==',' ){` |
|     49 | 5870 | `					int segLen = i - segStart, k;` |
|     49 | 5871 | `					if( segIdx==0 ){ if( segLen<1 \|\| segLen>3 ){ return 0; } }` |
|     25 | 5872 | `					else if( segLen!=3 ){ return 0; }` |
|    119 | 5873 | `					for( k=segStart; k<i; k++ ){` |
|     81 | 5874 | `						if( !SyisDigit((unsigned char)z[k]) ){ return 0; }` |
|     81 | 5875 | `						zBuf[m++] = z[k];` |
|     41 | 5876 | `					}` |
|     39 | 5877 | `					segStart = i+1; segIdx++;` |
|     19 | 5878 | `				}` |
|     71 | 5879 | `			}` |
|      8 | 5880 | `		}else{` |
|    ! 0 | 5881 | `			for( i=s; i<intEnd; i++ ){ zBuf[m++] = z[i]; }` |
|      - | 5882 | `		}` |
|     27 | 5883 | `		for( i=intEnd; i<n; i++ ){` |
|     13 | 5884 | `			if( z[i]==',' ){ return 0; }` |
|     13 | 5885 | `			zBuf[m++] = z[i];` |
|      7 | 5886 | `		}` |
|     15 | 5887 | `		zv = zBuf; nv = m;` |
|      8 | 5888 | `	}else{` |
|     45 | 5889 | `		zv = z; nv = n;` |
|      - | 5890 | `	}` |
|     59 | 5891 | `	i = 0;` |
|     59 | 5892 | `	if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|    167 | 5893 | `	while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     59 | 5894 | `	if( i<nv && zv[i]=='.' ){` |
|     21 | 5895 | `		i++;` |
|     39 | 5896 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     10 | 5897 | `	}` |
|     59 | 5898 | `	if( !seenDigit ){ return 0; }` |
|     57 | 5899 | `	if( i<nv && (zv[i]=='e'\|\|zv[i]=='E') ){` |
|     29 | 5900 | `		i++;` |
|     29 | 5901 | `		if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|     29 | 5902 | `		if( i>=nv \|\| !SyisDigit((unsigned char)zv[i]) ){ return 0; }` |
|    105 | 5903 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; }` |
|     14 | 5904 | `	}` |
|     57 | 5905 | `	if( i!=nv ){ return 0; } /* trailing junk */` |
|      - | 5906 | `	/* The grammar above guarantees zv[0..nv) is a clean ASCII decimal float (no hex /` |
|      - | 5907 | `	 * inf / nan / trailing junk), so it is safe to hand to libc strtod, which — unlike` |
|      - | 5908 | `	 * SyStrToReal (15 sig-digits + exponent clamped to 308, so it silently saturates` |
|      - | 5909 | `	 * overflowing magnitudes to a finite value) — is overflow/underflow-aware and` |
|      - | 5910 | `	 * correctly rounded. strtod needs a NUL-terminated string: the ALLOW_THOUSAND path` |
|      - | 5911 | `	 * already built the span in zBuf (zv==zBuf); the plain path must copy it there (z is` |
|      - | 5912 | `	 * const + not NUL-terminated). nv <= n <= 500 < sizeof(zBuf) by the cap above.` |
|      - | 5913 | `	 * Matches PHP 8.5 byte-for-byte: reject overflow (-> +/-INF) and total underflow` |
|      - | 5914 | `	 * (-> 0.0), keep subnormals (nonzero, errno==ERANGE) and a genuine "0" (errno==0). */` |
|     53 | 5915 | `	if( zv != zBuf ){ SyMemcpy(zv,zBuf,(sxu32)nv); }` |
|     53 | 5916 | `	zBuf[nv] = 0;` |
|     53 | 5917 | `	errno = 0;` |
|     53 | 5918 | `	d = strtod(zBuf,0);` |
|     53 | 5919 | `	if( errno == ERANGE && (d == HUGE_VAL \|\| d == -HUGE_VAL \|\| d == 0.0) ){` |
|     15 | 5920 | `		return 0;` |
|      - | 5921 | `	}` |
|     39 | 5922 | `	*pOut = d;` |
|     39 | 5923 | `	return 1;` |
|     35 | 5924 | `}` |
|      - | 5925 | `/* FILTER_VALIDATE_BOOLEAN. Returns 1 if the string is recognized (sets *pBool),` |
|      - | 5926 | ` * 0 if it is unrecognized (the failure path). "0"/"false"/"" are recognized as` |
|      - | 5927 | ` * false, NOT failures. */` |
|     33 | 5928 | `static int FvValidateBool(const char *z,int n,int *pBool){` |
|     33 | 5929 | `	FvTrim(&z,&n);` |
|     32 | 5930 | `	if( (n==1 && z[0]=='1') \|\| (n==4 && SyStrnicmp(z,"true",4)==0)` |
|     25 | 5931 | `	    \|\| (n==2 && SyStrnicmp(z,"on",2)==0) \|\| (n==3 && SyStrnicmp(z,"yes",3)==0) ){` |
|     11 | 5932 | `		*pBool = 1; return 1;` |
|      - | 5933 | `	}` |
|     22 | 5934 | `	if( n==0 \|\| (n==1 && z[0]=='0') \|\| (n==5 && SyStrnicmp(z,"false",5)==0)` |
|     11 | 5935 | `	    \|\| (n==3 && SyStrnicmp(z,"off",3)==0) \|\| (n==2 && SyStrnicmp(z,"no",2)==0) ){` |
|     11 | 5936 | `		*pBool = 0; return 1;` |
|      - | 5937 | `	}` |
|      9 | 5938 | `	return 0;` |
|     15 | 5939 | `}` |
|      - | 5940 | `/* IPv4 dotted-quad: exactly 4 octets 0..255, no leading zeros. */` |
|     33 | 5941 | `static int FvValidateIp4(const char *z,int n){` |
|     33 | 5942 | `	int i = 0, parts = 0;` |
|     77 | 5943 | `	while( i<n ){` |
|     65 | 5944 | `		int val = 0, digits = 0, start = i;` |
|    143 | 5945 | `		while( i<n && SyisDigit((unsigned char)z[i]) ){` |
|     85 | 5946 | `			val = val*10 + (z[i]-'0');` |
|     85 | 5947 | `			if( val>255 ){ return 0; }` |
|     79 | 5948 | `			digits++; i++;` |
|      1 | 5949 | `		}` |
|     59 | 5950 | `		if( digits==0 \|\| digits>3 ){ return 0; }` |
|     49 | 5951 | `		if( digits>1 && z[start]=='0' ){ return 0; } /* leading zero */` |
|     45 | 5952 | `		parts++;` |
|     45 | 5953 | `		if( parts>4 ){ return 0; }` |
|     45 | 5954 | `		if( i<n ){` |
|     33 | 5955 | `			if( z[i]!='.' ){ return 0; }` |
|     33 | 5956 | `			i++;` |
|     33 | 5957 | `			if( i>=n ){ return 0; } /* trailing dot */` |
|     16 | 5958 | `		}` |
|      1 | 5959 | `	}` |
|     13 | 5960 | `	return parts==4;` |
|     17 | 5961 | `}` |
|      - | 5962 | `/* A colon-separated run of IPv6 hextets with no "::" (n may be 0 -> 0 groups),` |
|      - | 5963 | ` * allowing a trailing embedded IPv4. Returns the 16-bit group count or -1. */` |
|     19 | 5964 | `static int FvIp6Hextets(const char *z,int n){` |
|     19 | 5965 | `	int i = 0, segStart = 0, groups = 0;` |
|     19 | 5966 | `	if( n==0 ){ return 0; }` |
|    145 | 5967 | `	while( i<=n ){` |
|    133 | 5968 | `		if( i==n \|\| z[i]==':' ){` |
|     23 | 5969 | `			int segLen = i - segStart, j, isV4 = 0;` |
|     23 | 5970 | `			if( segLen==0 ){ return -1; } /* an empty hextet (stray ':') */` |
|     77 | 5971 | `			for( j=segStart; j<i; j++ ){ if( z[j]=='.' ){ isV4 = 1; break; } }` |
|     23 | 5972 | `			if( isV4 ){` |
|     11 | 5973 | `				if( i!=n ){ return -1; } /* IPv4 only as the final token */` |
|     11 | 5974 | `				if( !FvValidateIp4(z+segStart,segLen) ){ return -1; }` |
|      5 | 5975 | `				groups += 2;` |
|      3 | 5976 | `			}else{` |
|     13 | 5977 | `				if( segLen>4 ){ return -1; }` |
|     47 | 5978 | `				for( j=segStart; j<i; j++ ){ if( SyHexToint((unsigned char)z[j])<0 ){ return -1; } }` |
|     13 | 5979 | `				groups++;` |
|      - | 5980 | `			}` |
|     17 | 5981 | `			segStart = i+1;` |
|      8 | 5982 | `		}` |
|    127 | 5983 | `		i++;` |
|      1 | 5984 | `	}` |
|     13 | 5985 | `	return groups;` |
|     10 | 5986 | `}` |
|      - | 5987 | `/* IPv6: at most one "::" zero-run; 8 groups exactly, or fewer when "::" present. */` |
|     19 | 5988 | `static int FvValidateIp6(const char *z,int n){` |
|     19 | 5989 | `	const char *zDbl = 0;` |
|      - | 5990 | `	int i, ga, gb;` |
|    139 | 5991 | `	for( i=0; i+1<n; i++ ){` |
|    123 | 5992 | `		if( z[i]==':' && z[i+1]==':' ){` |
|     13 | 5993 | `			if( zDbl ){ return 0; } /* a second "::" is invalid */` |
|     11 | 5994 | `			zDbl = z+i;` |
|      5 | 5995 | `		}` |
|     61 | 5996 | `	}` |
|     17 | 5997 | `	if( zDbl==0 ){` |
|      9 | 5998 | `		return FvIp6Hextets(z,n)==8;` |
|    ! 0 | 5999 | `	}else{` |
|      9 | 6000 | `		int lenA = (int)(zDbl - z);` |
|      9 | 6001 | `		int lenB = n - lenA - 2;` |
|      9 | 6002 | `		ga = (lenA==0) ? 0 : FvIp6Hextets(z,lenA);` |
|      9 | 6003 | `		gb = (lenB==0) ? 0 : FvIp6Hextets(zDbl+2,lenB);` |
|      9 | 6004 | `		if( ga<0 \|\| gb<0 ){ return 0; }` |
|      9 | 6005 | `		return (ga+gb)<=7; /* "::" stands for at least one zero group */` |
|      - | 6006 | `	}` |
|     10 | 6007 | `}` |
|     25 | 6008 | `static int FvValidateIp(const char *z,int n,int flags){` |
|     25 | 6009 | `	int v4 = (flags & FV_FLAG_IPV4), v6 = (flags & FV_FLAG_IPV6);` |
|     25 | 6010 | `	if( !v4 && !v6 ){ v4 = v6 = 1; } /* default accepts either family */` |
|     25 | 6011 | `	if( v4 && FvValidateIp4(z,n) ){ return 1; }` |
|     21 | 6012 | `	if( v6 && FvValidateIp6(z,n) ){ return 1; }` |
|     13 | 6013 | `	return 0;` |
|     13 | 6014 | `}` |
|      - | 6015 | `/* FILTER_VALIDATE_MAC: 17-char colon- or dash-separated hex (XX:XX:..:XX). */` |
|     11 | 6016 | `static int FvValidateMac(const char *z,int n){` |
|      - | 6017 | `	char sep;` |
|      - | 6018 | `	int i;` |
|     11 | 6019 | `	if( n!=17 ){ return 0; }` |
|      7 | 6020 | `	sep = z[2];` |
|      7 | 6021 | `	if( sep!=':' && sep!='-' ){ return 0; }` |
|    105 | 6022 | `	for( i=0; i<17; i++ ){` |
|    101 | 6023 | `		if( (i%3)==2 ){ if( z[i]!=sep ){ return 0; } }` |
|     71 | 6024 | `		else if( SyHexToint((unsigned char)z[i])<0 ){ return 0; }` |
|     50 | 6025 | `	}` |
|      5 | 6026 | `	return 1;` |
|      6 | 6027 | `}` |
|      - | 6028 | `/* FILTER_VALIDATE_EMAIL (best-effort: covers the common cases, not quoted local` |
|      - | 6029 | ` * parts or IP-literal domains). */` |
|     28 | 6030 | `static int FvValidateEmail(const char *z,int n){` |
|     28 | 6031 | `	int at = -1, i, localLen, domLen, labelStart, dotCount = 0;` |
|      - | 6032 | `	const char *zDom;` |
|     28 | 6033 | `	if( n==0 \|\| n>320 ){ return 0; }` |
|    201 | 6034 | `	for( i=0; i<n; i++ ){` |
|    181 | 6035 | `		if( z[i]=='@' ){ if( at>=0 ){ return 0; } at = i; }` |
|     91 | 6036 | `	}` |
|     21 | 6037 | `	if( at<=0 \|\| at==n-1 ){ return 0; } /* one '@', non-empty local and domain */` |
|     21 | 6038 | `	localLen = at;` |
|     21 | 6039 | `	zDom = z + at + 1;` |
|     21 | 6040 | `	domLen = n - at - 1;` |
|     21 | 6041 | `	if( z[0]=='.' \|\| z[at-1]=='.' ){ return 0; }` |
|     57 | 6042 | `	for( i=0; i<localLen; i++ ){` |
|     43 | 6043 | `		unsigned char c = (unsigned char)z[i];` |
|     43 | 6044 | `		if( c<=' ' ){ return 0; }` |
|     41 | 6045 | `		if( c=='.' && i+1<localLen && z[i+1]=='.' ){ return 0; }` |
|     20 | 6046 | `	}` |
|     15 | 6047 | `	if( zDom[0]=='.' \|\| zDom[domLen-1]=='.' ){ return 0; }` |
|     13 | 6048 | `	labelStart = 0;` |
|     85 | 6049 | `	for( i=0; i<=domLen; i++ ){` |
|     75 | 6050 | `		if( i==domLen \|\| zDom[i]=='.' ){` |
|     25 | 6051 | `			int ll = i - labelStart;` |
|     25 | 6052 | `			if( ll==0 ){ return 0; } /* consecutive dots */` |
|     23 | 6053 | `			if( zDom[labelStart]=='-' \|\| zDom[i-1]=='-' ){ return 0; }` |
|     23 | 6054 | `			if( i<domLen ){ dotCount++; }` |
|     23 | 6055 | `			labelStart = i+1;` |
|     12 | 6056 | `		}else{` |
|     51 | 6057 | `			unsigned char c = (unsigned char)zDom[i];` |
|     51 | 6058 | `			if( !((c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9')\|\|c=='-') ){ return 0; }` |
|      - | 6059 | `		}` |
|     37 | 6060 | `	}` |
|     11 | 6061 | `	if( dotCount<1 ){ return 0; } /* PHP requires a dot in the domain (any TLD length) */` |
|      9 | 6062 | `	return 1;` |
|     15 | 6063 | `}` |
|      - | 6064 | `/* FILTER_VALIDATE_DOMAIN (lenient, matching PHP without FILTER_FLAG_HOSTNAME). */` |
|     11 | 6065 | `static int FvValidateDomain(const char *z,int n){` |
|      - | 6066 | `	int i;` |
|     11 | 6067 | `	if( n<1 \|\| n>253 \|\| z[0]=='.' ){ return 0; }` |
|     81 | 6068 | `	for( i=0; i<n; i++ ){` |
|     75 | 6069 | `		unsigned char c = (unsigned char)z[i];` |
|     75 | 6070 | `		if( c<=' ' ){ return 0; }` |
|     75 | 6071 | `		if( c=='.' && i+1<n && z[i+1]=='.' ){ return 0; }` |
|     37 | 6072 | `	}` |
|      7 | 6073 | `	return 1;` |
|      6 | 6074 | `}` |
|      - | 6075 | `/* FILTER_VALIDATE_URL: require a scheme and a host (PHP's filter is itself` |
|      - | 6076 | ` * parse_url-based, so PH7_VmHttpSplitURI tracks it closely). */` |
|     15 | 6077 | `static int FvValidateUrl(const char *z,int n){` |
|      - | 6078 | `	SyhttpUri sUri;` |
|     15 | 6079 | `	if( n==0 ){ return 0; }` |
|     15 | 6080 | `	SyZero(&sUri,(sxu32)sizeof(sUri));` |
|     15 | 6081 | `	if( PH7_VmHttpSplitURI(&sUri,z,(sxu32)n)!=SXRET_OK ){ return 0; }` |
|     15 | 6082 | `	return sUri.sScheme.nByte!=0 && sUri.sHost.nByte!=0;` |
|      8 | 6083 | `}` |
|      - | 6084 | `/* The Fv sanitizers build their result by appending directly to the call` |
|      - | 6085 | ` * context (ph7_result_string accumulates, like htmlspecialchars), emitting each` |
|      - | 6086 | ` * kept run in one call and seeding "" so an all-stripped input yields "". */` |
|      - | 6087 | `/* SANITIZE_NUMBER_INT (isFloat=0) / SANITIZE_NUMBER_FLOAT (isFloat=1). */` |
|     37 | 6088 | `static void FvSanitizeNumber(ph7_context *pCtx,const char *z,int n,int isFloat,int flags){` |
|     37 | 6089 | `	int i, runStart = 0;` |
|     37 | 6090 | `	ph7_result_string(pCtx,"",0);` |
|     97 | 6091 | `	for( i=0; i<n; i++ ){` |
|     91 | 6092 | `		char c = z[i];` |
|     91 | 6093 | `		int keep = (c>='0'&&c<='9') \|\| c=='+' \|\| c=='-';` |
|     91 | 6094 | `		if( !keep && isFloat ){` |
|     38 | 6095 | `			keep = (c=='.' && (flags & FV_FLAG_ALLOW_FRACTION))` |
|     23 | 6096 | `			    \|\| (c==',' && (flags & FV_FLAG_ALLOW_THOUSAND))` |
|     36 | 6097 | `			    \|\| ((c=='e'\|\|c=='E') && (flags & FV_FLAG_ALLOW_SCIENTIFIC));` |
|     12 | 6098 | `		}` |
|     61 | 6099 | `		if( !keep ){` |
|     33 | 6100 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     33 | 6101 | `			runStart = i+1;` |
|     16 | 6102 | `		}` |
|     31 | 6103 | `	}` |
|      7 | 6104 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      7 | 6105 | `}` |
|      - | 6106 | `/* Return non-zero when byte c must be stripped under the STRIP_* flags. Shared` |
|      - | 6107 | ` * by the UNSAFE_RAW string filter and SANITIZE_SPECIAL_CHARS. STRIP_LOW drops` |
|      - | 6108 | `` * bytes <32, STRIP_HIGH drops bytes >=127 (incl. DEL), STRIP_BACKTICK drops '`'.`` |
|      - | 6109 | ` * Matches php_filter_strip(); verified byte-exact vs php 8.5.7. */` |
|    287 | 6110 | `static int FvStripByte(unsigned char c,int flags){` |
|    287 | 6111 | `	if( (flags & FV_FLAG_STRIP_LOW)      && c<32 )    { return 1; }` |
|    281 | 6112 | `	if( (flags & FV_FLAG_STRIP_HIGH)     && c>=127 )  { return 1; }` |
|    269 | 6113 | `	if( (flags & FV_FLAG_STRIP_BACKTICK) && c==0x60 ) { return 1; }` |
|    267 | 6114 | `	return 0;` |
|    144 | 6115 | `}` |
|      - | 6116 | `/* FILTER_UNSAFE_RAW / FILTER_DEFAULT with flags: no default transform, but the` |
|      - | 6117 | ` * STRIP/ENCODE flags apply. Precedence (per php_filter_unsafe_raw, verified` |
|      - | 6118 | ` * vs php 8.5.7): a byte is first tested for stripping; a surviving byte is then` |
|      - | 6119 | ` * encoded as a decimal numeric entity if ENCODE_LOW (<32) / ENCODE_HIGH (>=127)` |
|      - | 6120 | ` * is set, and '&' becomes "&#38;" under ENCODE_AMP. So STRIP_LOW\|ENCODE_LOW` |
|      - | 6121 | ` * strips (nothing left to encode). Bytes are treated individually — ENCODE_HIGH` |
|      - | 6122 | ` * numeric-encodes each byte of a multibyte sequence separately, not the codepoint. */` |
|     25 | 6123 | `static void FvSanitizeString(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 6124 | `	int i, runStart = 0;` |
|     25 | 6125 | `	ph7_result_string(pCtx,"",0);` |
|    193 | 6126 | `	for( i=0; i<n; i++ ){` |
|    179 | 6127 | `		unsigned char c = (unsigned char)z[i];` |
|    179 | 6128 | `		if( FvStripByte(c,flags) ){` |
|     13 | 6129 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     13 | 6130 | `			runStart = i+1;` |
|     13 | 6131 | `			continue;` |
|      - | 6132 | `		}` |
|    167 | 6133 | `		if( c=='&' && (flags & FV_FLAG_ENCODE_AMP) ){` |
|      3 | 6134 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      3 | 6135 | `			ph7_result_string(pCtx,"&#38;",-1);` |
|      3 | 6136 | `			runStart = i+1;` |
|    166 | 6137 | `		}else if( (c<32 && (flags & FV_FLAG_ENCODE_LOW))` |
|    164 | 6138 | `		       \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     37 | 6139 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 6140 | `			ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|      9 | 6141 | `			runStart = i+1;` |
|      4 | 6142 | `		}` |
|     79 | 6143 | `	}` |
|     15 | 6144 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     15 | 6145 | `}` |
|      - | 6146 | `/* FILTER_SANITIZE_SPECIAL_CHARS: encode <>&"' and every control byte <32 as a` |
|      - | 6147 | ` * decimal numeric entity (&#60; &#38; &#34; ...). The STRIP_* flags remove bytes` |
|      - | 6148 | ` * before encoding; ENCODE_HIGH numeric-encodes surviving bytes >=127. Bytes >=128` |
|      - | 6149 | ` * are otherwise passed through verbatim (this filter is NOT UTF-8-aware — only the` |
|      - | 6150 | ` * FULL variant is). Byte-exact vs php 8.5.7. */` |
|     13 | 6151 | `static void FvSanitizeSpecial(ph7_context *pCtx,const char *z,int n,int flags){` |
|     13 | 6152 | `	int i, runStart = 0;` |
|      - | 6153 | `	const char *zEnt;` |
|     13 | 6154 | `	ph7_result_string(pCtx,"",0);` |
|    131 | 6155 | `	for( i=0; i<n; i++ ){` |
|    119 | 6156 | `		unsigned char c = (unsigned char)z[i];` |
|    119 | 6157 | `		if( FvStripByte(c,flags) ){` |
|      9 | 6158 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 6159 | `			runStart = i+1;` |
|      9 | 6160 | `			continue;` |
|      - | 6161 | `		}` |
|    111 | 6162 | `		switch( c ){` |
|      3 | 6163 | `		case '<':  zEnt = "&#60;"; break;` |
|      3 | 6164 | `		case '>':  zEnt = "&#62;"; break;` |
|     11 | 6165 | `		case '&':  zEnt = "&#38;"; break;` |
|      3 | 6166 | `		case '"':  zEnt = "&#34;"; break;` |
|      3 | 6167 | `		case '\'': zEnt = "&#39;"; break;` |
|     46 | 6168 | `		default:` |
|      - | 6169 | `			/* Control bytes <32 are always numeric-encoded; bytes >=127 only when` |
|      - | 6170 | `			 * ENCODE_HIGH is set. Everything else stays in the current run. */` |
|     93 | 6171 | `			if( c<32 \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     17 | 6172 | `				if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     17 | 6173 | `				ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|     17 | 6174 | `				runStart = i+1;` |
|      8 | 6175 | `			}` |
|     93 | 6176 | `			continue; /* keep in the current run */` |
|      - | 6177 | `		}` |
|     19 | 6178 | `		if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     19 | 6179 | `		ph7_result_string(pCtx,zEnt,-1); /* -1: length from strlen */` |
|     19 | 6180 | `		runStart = i+1;` |
|     10 | 6181 | `	}` |
|     13 | 6182 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     13 | 6183 | `}` |
|      - | 6184 | `/* HTML 4.01 named-entity table (codepoint -> "&name;") used by the UTF-8-aware` |
|      - | 6185 | ` * FULL_SPECIAL_CHARS filter, sorted ascending by codepoint for binary search.` |
|      - | 6186 | ` * Generated from php 8.5.7 (the exact set php_escape_html_entities emits for the` |
|      - | 6187 | ` * default document type); the five inline specials <>&"' are handled separately,` |
|      - | 6188 | ` * so every entry here is a codepoint >=0xA0. 248 rows. */` |
|      - | 6189 | `static const struct { sxu32 cp; const char *zEnt; } aHtml401Ent[] = {` |
|      - | 6190 | `	{0x00A0,"&nbsp;"},{0x00A1,"&iexcl;"},{0x00A2,"&cent;"},{0x00A3,"&pound;"},` |
|      - | 6191 | `	{0x00A4,"&curren;"},{0x00A5,"&yen;"},{0x00A6,"&brvbar;"},{0x00A7,"&sect;"},` |
|      - | 6192 | `	{0x00A8,"&uml;"},{0x00A9,"&copy;"},{0x00AA,"&ordf;"},{0x00AB,"&laquo;"},` |
|      - | 6193 | `	{0x00AC,"&not;"},{0x00AD,"&shy;"},{0x00AE,"&reg;"},{0x00AF,"&macr;"},` |
|      - | 6194 | `	{0x00B0,"&deg;"},{0x00B1,"&plusmn;"},{0x00B2,"&sup2;"},{0x00B3,"&sup3;"},` |
|      - | 6195 | `	{0x00B4,"&acute;"},{0x00B5,"&micro;"},{0x00B6,"&para;"},{0x00B7,"&middot;"},` |
|      - | 6196 | `	{0x00B8,"&cedil;"},{0x00B9,"&sup1;"},{0x00BA,"&ordm;"},{0x00BB,"&raquo;"},` |
|      - | 6197 | `	{0x00BC,"&frac14;"},{0x00BD,"&frac12;"},{0x00BE,"&frac34;"},{0x00BF,"&iquest;"},` |
|      - | 6198 | `	{0x00C0,"&Agrave;"},{0x00C1,"&Aacute;"},{0x00C2,"&Acirc;"},{0x00C3,"&Atilde;"},` |
|      - | 6199 | `	{0x00C4,"&Auml;"},{0x00C5,"&Aring;"},{0x00C6,"&AElig;"},{0x00C7,"&Ccedil;"},` |
|      - | 6200 | `	{0x00C8,"&Egrave;"},{0x00C9,"&Eacute;"},{0x00CA,"&Ecirc;"},{0x00CB,"&Euml;"},` |
|      - | 6201 | `	{0x00CC,"&Igrave;"},{0x00CD,"&Iacute;"},{0x00CE,"&Icirc;"},{0x00CF,"&Iuml;"},` |
|      - | 6202 | `	{0x00D0,"&ETH;"},{0x00D1,"&Ntilde;"},{0x00D2,"&Ograve;"},{0x00D3,"&Oacute;"},` |
|      - | 6203 | `	{0x00D4,"&Ocirc;"},{0x00D5,"&Otilde;"},{0x00D6,"&Ouml;"},{0x00D7,"&times;"},` |
|      - | 6204 | `	{0x00D8,"&Oslash;"},{0x00D9,"&Ugrave;"},{0x00DA,"&Uacute;"},{0x00DB,"&Ucirc;"},` |
|      - | 6205 | `	{0x00DC,"&Uuml;"},{0x00DD,"&Yacute;"},{0x00DE,"&THORN;"},{0x00DF,"&szlig;"},` |
|      - | 6206 | `	{0x00E0,"&agrave;"},{0x00E1,"&aacute;"},{0x00E2,"&acirc;"},{0x00E3,"&atilde;"},` |
|      - | 6207 | `	{0x00E4,"&auml;"},{0x00E5,"&aring;"},{0x00E6,"&aelig;"},{0x00E7,"&ccedil;"},` |
|      - | 6208 | `	{0x00E8,"&egrave;"},{0x00E9,"&eacute;"},{0x00EA,"&ecirc;"},{0x00EB,"&euml;"},` |
|      - | 6209 | `	{0x00EC,"&igrave;"},{0x00ED,"&iacute;"},{0x00EE,"&icirc;"},{0x00EF,"&iuml;"},` |
|      - | 6210 | `	{0x00F0,"&eth;"},{0x00F1,"&ntilde;"},{0x00F2,"&ograve;"},{0x00F3,"&oacute;"},` |
|      - | 6211 | `	{0x00F4,"&ocirc;"},{0x00F5,"&otilde;"},{0x00F6,"&ouml;"},{0x00F7,"&divide;"},` |
|      - | 6212 | `	{0x00F8,"&oslash;"},{0x00F9,"&ugrave;"},{0x00FA,"&uacute;"},{0x00FB,"&ucirc;"},` |
|      - | 6213 | `	{0x00FC,"&uuml;"},{0x00FD,"&yacute;"},{0x00FE,"&thorn;"},{0x00FF,"&yuml;"},` |
|      - | 6214 | `	{0x0152,"&OElig;"},{0x0153,"&oelig;"},{0x0160,"&Scaron;"},{0x0161,"&scaron;"},` |
|      - | 6215 | `	{0x0178,"&Yuml;"},{0x0192,"&fnof;"},{0x02C6,"&circ;"},{0x02DC,"&tilde;"},` |
|      - | 6216 | `	{0x0391,"&Alpha;"},{0x0392,"&Beta;"},{0x0393,"&Gamma;"},{0x0394,"&Delta;"},` |
|      - | 6217 | `	{0x0395,"&Epsilon;"},{0x0396,"&Zeta;"},{0x0397,"&Eta;"},{0x0398,"&Theta;"},` |
|      - | 6218 | `	{0x0399,"&Iota;"},{0x039A,"&Kappa;"},{0x039B,"&Lambda;"},{0x039C,"&Mu;"},` |
|      - | 6219 | `	{0x039D,"&Nu;"},{0x039E,"&Xi;"},{0x039F,"&Omicron;"},{0x03A0,"&Pi;"},` |
|      - | 6220 | `	{0x03A1,"&Rho;"},{0x03A3,"&Sigma;"},{0x03A4,"&Tau;"},{0x03A5,"&Upsilon;"},` |
|      - | 6221 | `	{0x03A6,"&Phi;"},{0x03A7,"&Chi;"},{0x03A8,"&Psi;"},{0x03A9,"&Omega;"},` |
|      - | 6222 | `	{0x03B1,"&alpha;"},{0x03B2,"&beta;"},{0x03B3,"&gamma;"},{0x03B4,"&delta;"},` |
|      - | 6223 | `	{0x03B5,"&epsilon;"},{0x03B6,"&zeta;"},{0x03B7,"&eta;"},{0x03B8,"&theta;"},` |
|      - | 6224 | `	{0x03B9,"&iota;"},{0x03BA,"&kappa;"},{0x03BB,"&lambda;"},{0x03BC,"&mu;"},` |
|      - | 6225 | `	{0x03BD,"&nu;"},{0x03BE,"&xi;"},{0x03BF,"&omicron;"},{0x03C0,"&pi;"},` |
|      - | 6226 | `	{0x03C1,"&rho;"},{0x03C2,"&sigmaf;"},{0x03C3,"&sigma;"},{0x03C4,"&tau;"},` |
|      - | 6227 | `	{0x03C5,"&upsilon;"},{0x03C6,"&phi;"},{0x03C7,"&chi;"},{0x03C8,"&psi;"},` |
|      - | 6228 | `	{0x03C9,"&omega;"},{0x03D1,"&thetasym;"},{0x03D2,"&upsih;"},{0x03D6,"&piv;"},` |
|      - | 6229 | `	{0x2002,"&ensp;"},{0x2003,"&emsp;"},{0x2009,"&thinsp;"},{0x200C,"&zwnj;"},` |
|      - | 6230 | `	{0x200D,"&zwj;"},{0x200E,"&lrm;"},{0x200F,"&rlm;"},{0x2013,"&ndash;"},` |
|      - | 6231 | `	{0x2014,"&mdash;"},{0x2018,"&lsquo;"},{0x2019,"&rsquo;"},{0x201A,"&sbquo;"},` |
|      - | 6232 | `	{0x201C,"&ldquo;"},{0x201D,"&rdquo;"},{0x201E,"&bdquo;"},{0x2020,"&dagger;"},` |
|      - | 6233 | `	{0x2021,"&Dagger;"},{0x2022,"&bull;"},{0x2026,"&hellip;"},{0x2030,"&permil;"},` |
|      - | 6234 | `	{0x2032,"&prime;"},{0x2033,"&Prime;"},{0x2039,"&lsaquo;"},{0x203A,"&rsaquo;"},` |
|      - | 6235 | `	{0x203E,"&oline;"},{0x2044,"&frasl;"},{0x20AC,"&euro;"},{0x2111,"&image;"},` |
|      - | 6236 | `	{0x2118,"&weierp;"},{0x211C,"&real;"},{0x2122,"&trade;"},{0x2135,"&alefsym;"},` |
|      - | 6237 | `	{0x2190,"&larr;"},{0x2191,"&uarr;"},{0x2192,"&rarr;"},{0x2193,"&darr;"},` |
|      - | 6238 | `	{0x2194,"&harr;"},{0x21B5,"&crarr;"},{0x21D0,"&lArr;"},{0x21D1,"&uArr;"},` |
|      - | 6239 | `	{0x21D2,"&rArr;"},{0x21D3,"&dArr;"},{0x21D4,"&hArr;"},{0x2200,"&forall;"},` |
|      - | 6240 | `	{0x2202,"&part;"},{0x2203,"&exist;"},{0x2205,"&empty;"},{0x2207,"&nabla;"},` |
|      - | 6241 | `	{0x2208,"&isin;"},{0x2209,"&notin;"},{0x220B,"&ni;"},{0x220F,"&prod;"},` |
|      - | 6242 | `	{0x2211,"&sum;"},{0x2212,"&minus;"},{0x2217,"&lowast;"},{0x221A,"&radic;"},` |
|      - | 6243 | `	{0x221D,"&prop;"},{0x221E,"&infin;"},{0x2220,"&ang;"},{0x2227,"&and;"},` |
|      - | 6244 | `	{0x2228,"&or;"},{0x2229,"&cap;"},{0x222A,"&cup;"},{0x222B,"&int;"},` |
|      - | 6245 | `	{0x2234,"&there4;"},{0x223C,"&sim;"},{0x2245,"&cong;"},{0x2248,"&asymp;"},` |
|      - | 6246 | `	{0x2260,"&ne;"},{0x2261,"&equiv;"},{0x2264,"&le;"},{0x2265,"&ge;"},` |
|      - | 6247 | `	{0x2282,"&sub;"},{0x2283,"&sup;"},{0x2284,"&nsub;"},{0x2286,"&sube;"},` |
|      - | 6248 | `	{0x2287,"&supe;"},{0x2295,"&oplus;"},{0x2297,"&otimes;"},{0x22A5,"&perp;"},` |
|      - | 6249 | `	{0x22C5,"&sdot;"},{0x2308,"&lceil;"},{0x2309,"&rceil;"},{0x230A,"&lfloor;"},` |
|      - | 6250 | `	{0x230B,"&rfloor;"},{0x2329,"&lang;"},{0x232A,"&rang;"},{0x25CA,"&loz;"},` |
|      - | 6251 | `	{0x2660,"&spades;"},{0x2663,"&clubs;"},{0x2665,"&hearts;"},{0x2666,"&diams;"}` |
|      - | 6252 | `};` |
|      - | 6253 | `/* Binary-search aHtml401Ent[] for cp; return its "&name;" entity or 0. */` |
|     41 | 6254 | `static const char *FvHtml401Lookup(sxu32 cp){` |
|     41 | 6255 | `	int lo = 0, hi = (int)SX_ARRAYSIZE(aHtml401Ent) - 1;` |
|    323 | 6256 | `	while( lo <= hi ){` |
|    309 | 6257 | `		int mid = (lo + hi) / 2;` |
|    309 | 6258 | `		sxu32 c = aHtml401Ent[mid].cp;` |
|    309 | 6259 | `		if( c == cp ){ return aHtml401Ent[mid].zEnt; }` |
|    283 | 6260 | `		if( c < cp ){ lo = mid + 1; } else { hi = mid - 1; }` |
|      1 | 6261 | `	}` |
|     15 | 6262 | `	return 0;` |
|     21 | 6263 | `}` |
|      - | 6264 | `/* Decode one strict-UTF-8 sequence at p (< zEnd). On success returns its byte` |
|      - | 6265 | ` * length (1..4) and sets *pCp to the codepoint; on any malformed, overlong,` |
|      - | 6266 | ` * surrogate, truncated or out-of-range (>U+10FFFF) sequence returns 0. Matches` |
|      - | 6267 | ` * PHP's UTF-8 validation used by FULL_SPECIAL_CHARS (verified vs php 8.5.7). */` |
|    101 | 6268 | `static int FvUtf8Next(const unsigned char *p,const unsigned char *zEnd,sxu32 *pCp){` |
|    101 | 6269 | `	unsigned char c = p[0];` |
|    101 | 6270 | `	if( c < 0x80 ){ *pCp = c; return 1; }` |
|    101 | 6271 | `	if( c < 0xC2 ){ return 0; }              /* 0x80-0xBF stray cont / 0xC0-0xC1 overlong */` |
|     99 | 6272 | `	if( c < 0xE0 ){                          /* 2-byte: U+0080..U+07FF */` |
|     47 | 6273 | `		if( zEnd-p < 2 \|\| (p[1]&0xC0)!=0x80 ){ return 0; }` |
|     45 | 6274 | `		*pCp = ((sxu32)(c&0x1F)<<6) \| (p[1]&0x3F);` |
|     45 | 6275 | `		return 2;` |
|      - | 6276 | `	}` |
|     53 | 6277 | `	if( c < 0xF0 ){                          /* 3-byte: U+0800..U+FFFF minus surrogates */` |
|      - | 6278 | `		sxu32 cp;` |
|     47 | 6279 | `		if( zEnd-p < 3 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 ){ return 0; }` |
|     33 | 6280 | `		cp = ((sxu32)(c&0x0F)<<12) \| ((sxu32)(p[1]&0x3F)<<6) \| (p[2]&0x3F);` |
|     33 | 6281 | `		if( cp < 0x800 \|\| (cp>=0xD800 && cp<=0xDFFF) ){ return 0; }` |
|     29 | 6282 | `		*pCp = cp;` |
|     29 | 6283 | `		return 3;` |
|      - | 6284 | `	}` |
|      7 | 6285 | `	if( c < 0xF5 ){                          /* 4-byte: U+10000..U+10FFFF */` |
|      - | 6286 | `		sxu32 cp;` |
|      5 | 6287 | `		if( zEnd-p < 4 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 \|\| (p[3]&0xC0)!=0x80 ){ return 0; }` |
|      5 | 6288 | `		cp = ((sxu32)(c&0x07)<<18) \| ((sxu32)(p[1]&0x3F)<<12) \| ((sxu32)(p[2]&0x3F)<<6) \| (p[3]&0x3F);` |
|      5 | 6289 | `		if( cp < 0x10000 \|\| cp > 0x10FFFF ){ return 0; }` |
|      5 | 6290 | `		*pCp = cp;` |
|      5 | 6291 | `		return 4;` |
|      - | 6292 | `	}` |
|      3 | 6293 | `	return 0;                                /* 0xF5-0xFF */` |
|     51 | 6294 | `}` |
|      - | 6295 | `/* FILTER_SANITIZE_FULL_SPECIAL_CHARS: htmlentities-style, UTF-8-aware. Encodes` |
|      - | 6296 | ` * <>&"' as named entities ("'" -> &#039;; quotes suppressed under NO_ENCODE_QUOTES),` |
|      - | 6297 | ` * and every valid UTF-8 codepoint with an HTML 4.01 named entity as that entity;` |
|      - | 6298 | ` * valid codepoints without a named entity (and low control bytes) pass through` |
|      - | 6299 | ` * verbatim. If the input contains ANY invalid UTF-8 the whole result is "".` |
|      - | 6300 | ` * The STRIP/ENCODE flags do NOT apply to this filter (only NO_ENCODE_QUOTES).` |
|      - | 6301 | ` * php's filter does NOT re-encode valid pre-existing entities ("&amp;" stays,` |
|      - | 6302 | ` * "&bogus;" becomes "&amp;bogus;"), i.e. double_encode=false semantics —` |
|      - | 6303 | ` * exactly htmlentities(ENT_QUOTES\|ENT_HTML401, double_encode: false), so this` |
|      - | 6304 | ` * delegates to the shared encoder. Byte-exact vs php 8.5.7. */` |
|     25 | 6305 | `static void FvSanitizeFull(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 6306 | `	int iEntFlags = (flags & FV_FLAG_NO_ENCODE_QUOTES) ? 0 : PH7_ENT_QUOTES;` |
|     25 | 6307 | `	HtmlEscape(pCtx,z,n,iEntFlags,1/*bAll*/,0/*bDoubleEncode*/);` |
|     25 | 6308 | `}` |
|      - | 6309 | `/* ---------------------------------------------------------------------------` |
|      - | 6310 | ` * UTF-8-aware HTML entity core (htmlspecialchars/htmlentities family).` |
|      - | 6311 | ` * Prototyped next to the five builtins earlier in this file; lives here so it` |
|      - | 6312 | ` * can share aHtml401Ent[]/FvHtml401Lookup()/FvUtf8Next() with the filter_var` |
|      - | 6313 | ` * FULL_SPECIAL_CHARS filter above. Byte-exact vs php 8.5.7 (oracle-swept).` |
|      - | 6314 | ` * ------------------------------------------------------------------------ */` |
|      - | 6315 | `/* Encode cp as UTF-8 into zBuf (>= 4 bytes); return the byte length 1..4.` |
|      - | 6316 | ` * Thin wrapper over the engine-wide SX_WRITE_UTF8 (sxmacros.h). */` |
|    585 | 6317 | `static int HtmlCpUtf8(sxu32 cp,char *zBuf){` |
|    585 | 6318 | `	sxu8 *z = (sxu8 *)zBuf;` |
|    585 | 6319 | `	SX_WRITE_UTF8(z,cp);` |
|    585 | 6320 | `	return (int)(z - (sxu8 *)zBuf);` |
|      1 | 6321 | `}` |
|      - | 6322 | `/* Doctype-allowed codepoint test (php's unicode_cp_is_allowed) — gates what a` |
|      - | 6323 | ` * numeric reference may DECODE to. Oracle-pinned per doctype: HTML401` |
|      - | 6324 | ` * disallows C0 (except TAB/LF/CR) and DEL..U+009F; XML1 and XHTML share the` |
|      - | 6325 | ` * XML rules — DEL..U+009F allowed, U+FFFE/U+FFFF excluded; HTML5 swaps CR` |
|      - | 6326 | ` * for FF (0x0C) and excludes the noncharacters (U+FDD0..U+FDEF and every` |
|      - | 6327 | ` * U+xFFFE/U+xFFFF). Surrogates are disallowed everywhere. */` |
|     91 | 6328 | `static int HtmlCpAllowed(sxu32 cp,int iFlags){` |
|     91 | 6329 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|     91 | 6330 | `	if( cp==0x09 \|\| cp==0x0A ){ return 1; }` |
|     87 | 6331 | `	if( cp==0x0D ){ return iDoc != PH7_ENT_DOC_HTML5; }` |
|     85 | 6332 | `	if( cp==0x0C ){ return iDoc == PH7_ENT_DOC_HTML5; }` |
|     85 | 6333 | `	if( cp < 0x20 \|\| cp > 0x10FFFF ){ return 0; }` |
|     79 | 6334 | `	if( cp>=0xD800 && cp<=0xDFFF ){ return 0; }` |
|     77 | 6335 | `	if( cp>=0x7F && cp<=0x9F ){ return iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML; }` |
|     71 | 6336 | `	if( iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML ){` |
|    ! 0 | 6337 | `		return cp!=0xFFFE && cp!=0xFFFF;` |
|      - | 6338 | `	}` |
|     71 | 6339 | `	if( iDoc == PH7_ENT_DOC_HTML5 ){` |
|      9 | 6340 | `		if( cp>=0xFDD0 && cp<=0xFDEF ){ return 0; }` |
|      9 | 6341 | `		if( (cp & 0xFFFF) >= 0xFFFE ){ return 0; }` |
|      4 | 6342 | `	}` |
|     71 | 6343 | `	return 1;` |
|     46 | 6344 | `}` |
|      - | 6345 | `/* The ENT_DISALLOWED gate for RAW characters on the ENCODE side. Same as the` |
|      - | 6346 | ` * decode gate except CR under HTML5: php's encode-side unicode_cp_is_allowed` |
|      - | 6347 | ` * keeps a literal "\r" verbatim under ENT_HTML5\|ENT_DISALLOWED while the` |
|      - | 6348 | ` * decode side leaves "&#13;" un-decoded (oracle-pinned at flags 176). */` |
|      9 | 6349 | `static int HtmlCpAllowedEncode(sxu32 cp,int iFlags){` |
|      9 | 6350 | `	if( cp==0x0D && (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 ){ return 1; }` |
|      9 | 6351 | `	return HtmlCpAllowed(cp,iFlags);` |
|      5 | 6352 | `}` |
|      - | 6353 | `/* Numeric-reference validity for the double_encode=false "is this already a` |
|      - | 6354 | ` * valid entity" test — a MUCH looser predicate than the decode gate above:` |
|      - | 6355 | ` * any codepoint <= U+10FFFF is valid (controls and surrogates included, every` |
|      - | 6356 | ` * doctype). ENT_DISALLOWED re-tightens non-HTML401 doctypes to the decode` |
|      - | 6357 | ` * gate, except that HTML5 exempts surrogates. All oracle-pinned: &#0; and` |
|      - | 6358 | ` * &#xD800; stay verbatim at flags 11 and 139; flags -1 (HTML5+DISALLOWED)` |
|      - | 6359 | ` * re-encodes &#0; and &#x10FFFF; but still keeps &#xD800;; flags 144` |
|      - | 6360 | ` * (XML1+DISALLOWED) re-encodes &#xD800;. */` |
|      9 | 6361 | `static int HtmlNumericAllowed(sxu32 cp,int iFlags){` |
|      9 | 6362 | `	if( cp > 0x10FFFF ){ return 0; }` |
|      7 | 6363 | `	if( (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML401 ){ return 1; /* never tightened */ }` |
|    ! 0 | 6364 | `	if( (iFlags & PH7_ENT_DISALLOWED)` |
|    ! 0 | 6365 | `	 && !((iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 && cp>=0xD800 && cp<=0xDFFF)` |
|    ! 0 | 6366 | `	 && !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|    ! 0 | 6367 | `	return 1;` |
|      5 | 6368 | `}` |
|      - | 6369 | `/* How many bytes the malformed UTF-8 sequence at p consumes — php's` |
|      - | 6370 | ` * get_next_char failure step (one U+FFFD substitution / one ENT_IGNORE drop` |
|      - | 6371 | ` * per MAXIMAL invalid subpart, not per byte): a prefix-valid sequence eats` |
|      - | 6372 | ` * its continuation bytes ("\xE0\x80\xAF" is ONE unit) while a byte that could` |
|      - | 6373 | ` * start a new sequence is left for the next round. */` |
|      5 | 6374 | `static int HtmlUtf8Trail(unsigned char c){ return c>=0x80 && c<=0xBF; }` |
|     11 | 6375 | `static int HtmlUtf8Lead(unsigned char c){ return c<0x80 \|\| (c>=0xC2 && c<=0xF4); }` |
|     15 | 6376 | `static int HtmlUtf8FailAdvance(const unsigned char *p,const unsigned char *zEnd){` |
|     15 | 6377 | `	unsigned char c = p[0];` |
|     15 | 6378 | `	int nAvail = (int)(zEnd - p);` |
|     15 | 6379 | `	if( c < 0xC2 \|\| c > 0xF4 ){ return 1; } /* stray trail / C0-C1 / F5-FF */` |
|     13 | 6380 | `	if( c < 0xE0 ){` |
|      3 | 6381 | `		if( nAvail < 2 ){ return 1; }` |
|      3 | 6382 | `		return HtmlUtf8Lead(p[1]) ? 1 : 2;` |
|      - | 6383 | `	}` |
|     11 | 6384 | `	if( c < 0xF0 ){` |
|     11 | 6385 | `		if( nAvail >= 3 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) ){` |
|      3 | 6386 | `			return 3; /* complete but overlong/surrogate */` |
|      - | 6387 | `		}` |
|      9 | 6388 | `		if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 6389 | `		if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 6390 | `		return 3;` |
|      - | 6391 | `	}` |
|    ! 0 | 6392 | `	if( nAvail >= 4 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) && HtmlUtf8Trail(p[3]) ){` |
|    ! 0 | 6393 | `		return 4; /* complete but overlong / > U+10FFFF */` |
|      - | 6394 | `	}` |
|    ! 0 | 6395 | `	if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 6396 | `	if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 6397 | `	if( nAvail < 4 \|\| HtmlUtf8Lead(p[3]) ){ return 3; }` |
|    ! 0 | 6398 | `	return 4;` |
|      8 | 6399 | `}` |
|      - | 6400 | `/* The basic special entities, shared by named matching, the hsc_decode` |
|      - | 6401 | ` * numeric whitelist and the translation-table builder so the sets can never` |
|      - | 6402 | ` * drift apart. (&apos; is not an HTML 4.01 entity — doctype-gated below.) */` |
|      - | 6403 | `static const struct { const char *zEnt; int n; sxu32 cp; } aHtmlSpecEnt[] = {` |
|      - | 6404 | `	{"&amp;",5,38},{"&lt;",4,60},{"&gt;",4,62},{"&quot;",6,34},{"&apos;",6,39}` |
|      - | 6405 | `};` |
|      - | 6406 | `/* Does this doctype consult the named-entity table (aHtml401Ent)? XML 1.0 has` |
|      - | 6407 | ` * no named entities beyond the specials; XHTML/HTML5 are approximated by the` |
|      - | 6408 | ` * HTML 4.01 table (documented divergence). */` |
|     63 | 6409 | `static int HtmlDocHasNamedTable(int iDoc){` |
|     63 | 6410 | `	return iDoc != PH7_ENT_DOC_XML1;` |
|      1 | 6411 | `}` |
|      - | 6412 | `/* The single-quote entity per doctype. Oracle-pinned asymmetry: for every` |
|      - | 6413 | ` * non-HTML401 doctype htmlspecialchars emits &apos; while htmlentities` |
|      - | 6414 | ` * (bEntities) keeps &#039; under XHTML too. The translation table mirrors` |
|      - | 6415 | ` * whichever function the requested table belongs to. */` |
|     29 | 6416 | `static const char *HtmlAposEntity(int iDoc,int bEntities){` |
|     29 | 6417 | `	if( iDoc == PH7_ENT_DOC_HTML401 \|\| (bEntities && iDoc == PH7_ENT_DOC_XHTML) ){` |
|     21 | 6418 | `		return "&#039;";` |
|      - | 6419 | `	}` |
|      9 | 6420 | `	return "&apos;";` |
|     15 | 6421 | `}` |
|      - | 6422 | `/* Try to parse one HTML entity at z (z[0]=='&', z < zEnd). bFull selects the` |
|      - | 6423 | ` * html_entity_decode set (doctype named table + any allowed numeric ref) vs` |
|      - | 6424 | ` * the htmlspecialchars_decode set (the basic specials + quote numerics only).` |
|      - | 6425 | ` * Named matching is case-SENSITIVE and the ';' is required (both PHP-exact);` |
|      - | 6426 | ` * numeric refs accept dec/hex (x or X) with any number of leading zeros but` |
|      - | 6427 | ` * reject out-of-range, surrogate and doctype-disallowed codepoints (the` |
|      - | 6428 | ` * caller then leaves the source verbatim). Quote-flag gating is NOT applied` |
|      - | 6429 | ` * here — the same routine doubles as the "is this a valid entity" test for` |
|      - | 6430 | ` * double_encode=false, which ignores the quote bits (oracle-pinned).` |
|      - | 6431 | ` * bEncodeCheck selects the looser HtmlNumericAllowed predicate used by that` |
|      - | 6432 | ` * double_encode test; decode callers pass 0 for the HtmlCpAllowed gate.` |
|      - | 6433 | ` * On success sets *pCp / *pnConsumed and returns 1. */` |
|    172 | 6434 | `static int HtmlParseEntity(const unsigned char *z,const unsigned char *zEnd,` |
|      1 | 6435 | `                           int iFlags,int bFull,int bEncodeCheck,sxu32 *pCp,int *pnConsumed){` |
|    173 | 6436 | `	int nAvail = (int)(zEnd - z);` |
|    173 | 6437 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6438 | `	sxu32 n;` |
|    173 | 6439 | `	if( nAvail < 4 ){ return 0; } /* shortest entities: &lt; &#9; */` |
|    169 | 6440 | `	if( z[1] == '#' ){` |
|      - | 6441 | `		/* Numeric reference */` |
|     89 | 6442 | `		sxu32 cp = 0;` |
|     89 | 6443 | `		int i = 2, bHex = 0, nDig = 0;` |
|     89 | 6444 | `		if( z[i]=='x' \|\| z[i]=='X' ){ bHex = 1; i++; }` |
|    317 | 6445 | `		for( ; i < nAvail && z[i] != ';' ; i++ ){` |
|      - | 6446 | `			int v;` |
|    221 | 6447 | `			unsigned char c = z[i];` |
|    221 | 6448 | `			if( c>='0' && c<='9' ){ v = c - '0'; }` |
|     17 | 6449 | `			else if( bHex && c>='a' && c<='f' ){ v = c - 'a' + 10; }` |
|     17 | 6450 | `			else if( bHex && c>='A' && c<='F' ){ v = c - 'A' + 10; }` |
|    ! 0 | 6451 | `			else { return 0; }` |
|      - | 6452 | `			/* Stop accumulating once out of range (keeps validating the shape;` |
|      - | 6453 | `			 * max intermediate is 0x10FFFF*16+15, no sxu32 overflow). */` |
|    221 | 6454 | `			if( cp <= 0x10FFFF ){ cp = cp * (bHex ? 16 : 10) + (sxu32)v; }` |
|    221 | 6455 | `			nDig++;` |
|    111 | 6456 | `		}` |
|     97 | 6457 | `		if( nDig == 0 \|\| i >= nAvail ){ return 0; } /* no digits / no ';' */` |
|     97 | 6458 | `		if( bEncodeCheck ? !HtmlNumericAllowed(cp,iFlags) : !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|     83 | 6459 | `		if( !bFull ){` |
|      - | 6460 | `			/* hsc_decode: numeric refs to the five specials only. */` |
|     99 | 6461 | `			for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) && aHtmlSpecEnt[n].cp != cp ; n++ ){}` |
|     25 | 6462 | `			if( n >= SX_ARRAYSIZE(aHtmlSpecEnt) ){ return 0; }` |
|     11 | 6463 | `		}` |
|     75 | 6464 | `		*pCp = cp;` |
|     75 | 6465 | `		*pnConsumed = i + 1;` |
|     75 | 6466 | `		return 1;` |
|      - | 6467 | `	}` |
|      - | 6468 | `	/* Named reference — every entity name starts with a letter, so anything` |
|      - | 6469 | `	 * else can bail out before touching the tables. */` |
|     81 | 6470 | `	if( !((z[1]>='a' && z[1]<='z') \|\| (z[1]>='A' && z[1]<='Z')) ){ return 0; }` |
|    287 | 6471 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) ; n++ ){` |
|    265 | 6472 | `		if( aHtmlSpecEnt[n].cp == 39 && iDoc == PH7_ENT_DOC_HTML401 ){ continue; }` |
|    243 | 6473 | `		if( nAvail >= aHtmlSpecEnt[n].n && SyMemcmp(z,aHtmlSpecEnt[n].zEnt,(sxu32)aHtmlSpecEnt[n].n) == 0 ){` |
|     53 | 6474 | `			*pCp = aHtmlSpecEnt[n].cp;` |
|     53 | 6475 | `			*pnConsumed = aHtmlSpecEnt[n].n;` |
|     53 | 6476 | `			return 1;` |
|      - | 6477 | `		}` |
|     96 | 6478 | `	}` |
|     23 | 6479 | `	if( bFull && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 6480 | `		/* Linear scan of the 248-row table: runs only at '&'-then-letter` |
|      - | 6481 | `		 * positions and guarantees the decode set can never drift from the` |
|      - | 6482 | `		 * encode table. The first-letter guard skips the SyStrlen/SyMemcmp` |
|      - | 6483 | `		 * for ~96% of rows. */` |
|   3369 | 6484 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|      - | 6485 | `			sxu32 nEnt;` |
|   3357 | 6486 | `			if( z[1] != (unsigned char)aHtml401Ent[n].zEnt[1] ){ continue; }` |
|    121 | 6487 | `			nEnt = SyStrlen(aHtml401Ent[n].zEnt);` |
|    121 | 6488 | `			if( (sxu32)nAvail >= nEnt && SyMemcmp(z,aHtml401Ent[n].zEnt,nEnt) == 0 ){` |
|      7 | 6489 | `				*pCp = aHtml401Ent[n].cp;` |
|      7 | 6490 | `				*pnConsumed = (int)nEnt;` |
|      7 | 6491 | `				return 1;` |
|      - | 6492 | `			}` |
|     58 | 6493 | `		}` |
|      6 | 6494 | `	}` |
|     17 | 6495 | `	return 0;` |
|     88 | 6496 | `}` |
|      - | 6497 | `/* Shared encoder for htmlspecialchars (bAll=0) and htmlentities (bAll=1).` |
|      - | 6498 | ` * Invalid UTF-8 policy: ENT_IGNORE drops the byte (and wins over SUBSTITUTE),` |
|      - | 6499 | ` * ENT_SUBSTITUTE emits one U+FFFD per invalid byte, neither -> the whole` |
|      - | 6500 | ` * result is "" (pre-validated in a first pass: the accumulating result API` |
|      - | 6501 | ` * cannot roll back — same reason FvSanitizeFull is two-pass). */` |
|     96 | 6502 | `static void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 6503 | `                       int iFlags,int bAll,int bDoubleEncode){` |
|     97 | 6504 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     97 | 6505 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|      - | 6506 | `	const unsigned char *runStart;` |
|     97 | 6507 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6508 | `	sxu32 cp;` |
|     97 | 6509 | `	if( (iFlags & (PH7_ENT_IGNORE\|PH7_ENT_SUBSTITUTE)) == 0 ){` |
|      - | 6510 | `		/* Pass 1: any malformed sequence rejects the entire input. ASCII` |
|      - | 6511 | `		 * bytes cannot be malformed, so skip them without the decoder. */` |
|    381 | 6512 | `		while( p < zEnd ){` |
|      - | 6513 | `			int len;` |
|    323 | 6514 | `			if( *p < 0x80 ){ p++; continue; }` |
|     37 | 6515 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     37 | 6516 | `			if( len == 0 ){ ph7_result_string(pCtx,"",0); return; }` |
|     27 | 6517 | `			p += len;` |
|      1 | 6518 | `		}` |
|     59 | 6519 | `		p = (const unsigned char *)zIn;` |
|     29 | 6520 | `	}` |
|     87 | 6521 | `	runStart = p;` |
|     87 | 6522 | `	ph7_result_string(pCtx,"",0);` |
|    463 | 6523 | `	while( p < zEnd ){` |
|    377 | 6524 | `		const char *zEnt = 0;` |
|      - | 6525 | `		int len;` |
|    377 | 6526 | `		if( *p < 0x80 ){` |
|    313 | 6527 | `			len = 1;` |
|    313 | 6528 | `			switch( *p ){` |
|     25 | 6529 | `			case '<': zEnt = "&lt;"; break;` |
|     25 | 6530 | `			case '>': zEnt = "&gt;"; break;` |
|     18 | 6531 | `			case '&':` |
|     37 | 6532 | `				zEnt = "&amp;";` |
|     37 | 6533 | `				if( !bDoubleEncode ){` |
|      - | 6534 | `					sxu32 eCp; int nEat;` |
|     25 | 6535 | `					if( HtmlParseEntity(p,zEnd,iFlags,1,1,&eCp,&nEat) ){` |
|      - | 6536 | `						/* A valid existing entity: keep it verbatim. */` |
|     13 | 6537 | `						zEnt = 0;` |
|     13 | 6538 | `						len = nEat;` |
|      6 | 6539 | `					}` |
|     12 | 6540 | `				}` |
|     37 | 6541 | `				break;` |
|     10 | 6542 | `			case '"':` |
|     21 | 6543 | `				if( iFlags & PH7_ENT_QUOTE_DOUBLE ){ zEnt = "&quot;"; }` |
|     21 | 6544 | `				break;` |
|     12 | 6545 | `			case '\'':` |
|     25 | 6546 | `				if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|     23 | 6547 | `					zEnt = HtmlAposEntity(iDoc,bAll);` |
|     11 | 6548 | `				}` |
|     25 | 6549 | `				break;` |
|     92 | 6550 | `			default:` |
|    185 | 6551 | `				if( (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode((sxu32)*p,iFlags) ){` |
|    ! 0 | 6552 | `					zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 6553 | `				}` |
|    184 | 6554 | `				break;` |
|      - | 6555 | `			}` |
|    157 | 6556 | `		}else{` |
|     65 | 6557 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     65 | 6558 | `			if( len == 0 ){` |
|      - | 6559 | `				/* Malformed subpart (IGNORE or SUBSTITUTE is set, else pass 1` |
|      - | 6560 | `				 * would have rejected): drop it or emit ONE U+FFFD for the` |
|      - | 6561 | `				 * whole unit (php substitutes per maximal invalid subpart). */` |
|     15 | 6562 | `				if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|     15 | 6563 | `				if( (iFlags & PH7_ENT_IGNORE) == 0 ){ ph7_result_string(pCtx,"\xEF\xBF\xBD",3); }` |
|     15 | 6564 | `				p += HtmlUtf8FailAdvance(p,zEnd);` |
|     15 | 6565 | `				runStart = p;` |
|     15 | 6566 | `				continue;` |
|      - | 6567 | `			}` |
|     51 | 6568 | `			if( bAll && HtmlDocHasNamedTable(iDoc) ){` |
|     41 | 6569 | `				zEnt = FvHtml401Lookup(cp);` |
|     20 | 6570 | `			}` |
|     51 | 6571 | `			if( zEnt == 0 && (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode(cp,iFlags) ){` |
|    ! 0 | 6572 | `				zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 6573 | `			}` |
|      - | 6574 | `		}` |
|    363 | 6575 | `		if( zEnt ){` |
|    135 | 6576 | `			if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|    135 | 6577 | `			ph7_result_string(pCtx,zEnt,-1);` |
|    135 | 6578 | `			runStart = p + len;` |
|     67 | 6579 | `		}` |
|    363 | 6580 | `		p += len;` |
|      1 | 6581 | `	}` |
|     87 | 6582 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     49 | 6583 | `}` |
|      - | 6584 | `/* Shared decoder for html_entity_decode (bFull=1) and htmlspecialchars_decode` |
|      - | 6585 | ` * (bFull=0). Quote refs (cp 34/39, named or numeric) are gated by the quote` |
|      - | 6586 | ` * bits and left verbatim when suppressed; an invalid entity leaves its '&'` |
|      - | 6587 | ` * verbatim and rescans right after it, which also yields PHP's no-double-` |
|      - | 6588 | ` * decode behavior ("&amp;lt;" -> "&lt;"). */` |
|     84 | 6589 | `static void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 6590 | `                         int iFlags,int bFull){` |
|     85 | 6591 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     85 | 6592 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|     85 | 6593 | `	const unsigned char *runStart = p;` |
|     85 | 6594 | `	ph7_result_string(pCtx,"",0);` |
|    565 | 6595 | `	while( p < zEnd ){` |
|      - | 6596 | `		sxu32 cp;` |
|      - | 6597 | `		int nEat;` |
|    516 | 6598 | `		if( *p != '&' ){ p++; continue; }` |
|    155 | 6599 | `		if( !HtmlParseEntity(p,zEnd,iFlags,bFull,0,&cp,&nEat) ){ p++; continue; }` |
|    124 | 6600 | `		if( (cp == 34 && (iFlags & PH7_ENT_QUOTE_DOUBLE) == 0)` |
|    117 | 6601 | `		 \|\| (cp == 39 && (iFlags & PH7_ENT_QUOTE_SINGLE) == 0) ){` |
|      - | 6602 | `			/* Suppressed quote: leave the entity source verbatim. */` |
|     37 | 6603 | `			p += nEat;` |
|     37 | 6604 | `			continue;` |
|      - | 6605 | `		}` |
|     89 | 6606 | `		if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|      - | 6607 | `		{` |
|      - | 6608 | `			char zBuf[4];` |
|     89 | 6609 | `			int n = HtmlCpUtf8(cp,zBuf);` |
|     89 | 6610 | `			ph7_result_string(pCtx,zBuf,n);` |
|      - | 6611 | `		}` |
|     89 | 6612 | `		p += nEat;` |
|     89 | 6613 | `		runStart = p;` |
|      1 | 6614 | `	}` |
|     81 | 6615 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     81 | 6616 | `}` |
|      - | 6617 | `/* Validate the optional charset argument at apArg[idx]: UTF-8 aliases (and` |
|      - | 6618 | ` * ""/NULL meaning the default) are accepted; anything else — including` |
|      - | 6619 | ` * php-supported single-byte charsets like ISO-8859-1, PHL is UTF-8-only by` |
|      - | 6620 | ` * policy — raises PHP's unsupported-charset warning and is treated as` |
|      - | 6621 | ` * UTF-8 (ph7_context_throw_error_format prepends the function name). */` |
|    143 | 6622 | `static void HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx){` |
|      - | 6623 | `	const char *zCs;` |
|      - | 6624 | `	int nCs;` |
|    150 | 6625 | `	if( nArg <= idx \|\| ph7_value_is_null(apArg[idx]) ){ return; }` |
|     15 | 6626 | `	zCs = ph7_value_to_string(apArg[idx],&nCs);` |
|     15 | 6627 | `	if( nCs == 0 ){ return; } /* "" selects the default charset (UTF-8) */` |
|     13 | 6628 | `	if( nCs == 5 && SyStrnicmp(zCs,"UTF-8",5) == 0 ){` |
|     13 | 6629 | `		return; /* php accepts only "UTF-8" (any case) silently — "UTF8" warns */` |
|      - | 6630 | `	}` |
|    ! 0 | 6631 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 6632 | `		"Charset \"%.*s\" is not supported, assuming UTF-8",nCs,zCs);` |
|     72 | 6633 | `}` |
|      - | 6634 | `/* get_html_translation_table() worker: character (UTF-8 bytes) => entity.` |
|      - | 6635 | ` * The five specials come first in byte order, then — for HTML_ENTITIES with a` |
|      - | 6636 | ` * named-table doctype — the 248 aHtml401Ent rows ascending (oracle-pinned` |
|      - | 6637 | ` * ordering; 253 entries under the defaults). */` |
|    549 | 6638 | `static void HtmlTableAdd(ph7_value *pArray,ph7_value *pValue,const char *zKey,const char *zEnt){` |
|    549 | 6639 | `	ph7_value_string(pValue,zEnt,-1);` |
|    549 | 6640 | `	ph7_array_add_strkey_elem(pArray,zKey,pValue);` |
|    549 | 6641 | `	ph7_value_reset_string_cursor(pValue);` |
|    549 | 6642 | `}` |
|     13 | 6643 | `static void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags){` |
|      - | 6644 | `	ph7_value *pArray,*pValue;` |
|     13 | 6645 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6646 | `	sxu32 n;` |
|     13 | 6647 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 6648 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 6649 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|    ! 0 | 6650 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6651 | `		return;` |
|      - | 6652 | `	}` |
|     13 | 6653 | `	if( iFlags & PH7_ENT_QUOTE_DOUBLE ){` |
|     11 | 6654 | `		HtmlTableAdd(pArray,pValue,"\"","&quot;");` |
|      5 | 6655 | `	}` |
|     13 | 6656 | `	HtmlTableAdd(pArray,pValue,"&","&amp;");` |
|     13 | 6657 | `	if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|      - | 6658 | `		/* The apostrophe row mirrors the function each table belongs to:` |
|      - | 6659 | `		 * SPECIALCHARS follows htmlspecialchars, ENTITIES follows` |
|      - | 6660 | `		 * htmlentities (oracle-pinned at flags 35). */` |
|      7 | 6661 | `		HtmlTableAdd(pArray,pValue,"'",HtmlAposEntity(iDoc,iTable != 0));` |
|      3 | 6662 | `	}` |
|     13 | 6663 | `	HtmlTableAdd(pArray,pValue,"<","&lt;");` |
|     13 | 6664 | `	HtmlTableAdd(pArray,pValue,">","&gt;");` |
|     13 | 6665 | `	if( iTable != 0 /*php: any non-HTML_SPECIALCHARS table => entities*/ && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 6666 | `		char zKey[8];` |
|    499 | 6667 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|    497 | 6668 | `			int nK = HtmlCpUtf8(aHtml401Ent[n].cp,zKey);` |
|    497 | 6669 | `			zKey[nK] = 0;` |
|    497 | 6670 | `			HtmlTableAdd(pArray,pValue,zKey,aHtml401Ent[n].zEnt);` |
|    249 | 6671 | `		}` |
|      1 | 6672 | `	}` |
|     13 | 6673 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 6674 | `}` |
|     25 | 6675 | `static int FvEmailAllowed(unsigned char c){` |
|     25 | 6676 | `	if( (c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9') ){ return 1; }` |
|     16 | 6677 | `	return c=='!'\|\|c=='#'\|\|c=='$'\|\|c=='%'\|\|c=='&'\|\|c=='\''\|\|c=='*'\|\|c=='+'` |
|     10 | 6678 | ``	    \|\| c=='-'\|\|c=='='\|\|c=='?'\|\|c=='^'\|\|c=='_'\|\|c=='`'\|\|c=='{'\|\|c=='\|'`` |
|     15 | 6679 | `	    \|\| c=='}'\|\|c=='~'\|\|c=='@'\|\|c=='.'\|\|c=='['\|\|c==']';` |
|     13 | 6680 | `}` |
|     23 | 6681 | `static int FvUrlAllowed(unsigned char c){` |
|     23 | 6682 | `	return c>=33 && c<=126; /* PHP keeps every printable ASCII byte except space */` |
|      1 | 6683 | `}` |
|      - | 6684 | `/* SANITIZE_EMAIL (isUrl=0) / SANITIZE_URL (isUrl=1): strip disallowed bytes. */` |
|      5 | 6685 | `static void FvSanitizeChars(ph7_context *pCtx,const char *z,int n,int isUrl){` |
|      5 | 6686 | `	int i, runStart = 0;` |
|      5 | 6687 | `	ph7_result_string(pCtx,"",0);` |
|     51 | 6688 | `	for( i=0; i<n; i++ ){` |
|     47 | 6689 | `		unsigned char c = (unsigned char)z[i];` |
|     47 | 6690 | `		if( !(isUrl ? FvUrlAllowed(c) : FvEmailAllowed(c)) ){` |
|     11 | 6691 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     11 | 6692 | `			runStart = i+1;` |
|      5 | 6693 | `		}` |
|     24 | 6694 | `	}` |
|      5 | 6695 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      5 | 6696 | `}` |
|      - | 6697 | `/*` |
|      - | 6698 | ` * Apply the selected filter to one already-resolved input value and write the` |
|      - | 6699 | ` * result into pCtx. Shared by filter_var() and filter_input(): the caller has` |
|      - | 6700 | ` * already parsed $filter/$flags/$options. On validation failure the 'default'` |
|      - | 6701 | ` * option (if any) is returned, else null when FILTER_NULL_ON_FAILURE is set,` |
|      - | 6702 | ` * else false. A validating filter that passes returns the (string) input` |
|      - | 6703 | ` * unchanged; a sanitizer writes its transformed output directly.` |
|      - | 6704 | ` */` |
|    316 | 6705 | `static int FvApplyFilter(ph7_context *pCtx,ph7_value *pInput,` |
|      - | 6706 | `                         int iFilter,int iFlags,ph7_value *pOpts,` |
|      - | 6707 | `                         ph7_value *pDefault)` |
|      3 | 6708 | `{` |
|    319 | 6709 | `	int bNull = (iFlags & FV_NULL_ON_FAILURE) ? 1 : 0;` |
|      - | 6710 | `	const char *zVal; int nVal;` |
|      - | 6711 | `	/* An array/object input fails every scalar filter. */` |
|    319 | 6712 | `	if( ph7_value_is_array(pInput) ){ goto fail; }` |
|    317 | 6713 | `	zVal = ph7_value_to_string(pInput,&nVal);` |
|    317 | 6714 | `	switch( iFilter ){` |
|     28 | 6715 | `	case FV_VALIDATE_INT: {` |
|      - | 6716 | `		ph7_int64 v;` |
|     58 | 6717 | `		if( !FvValidateInt(zVal,nVal,iFlags,&v) ){ goto fail; }` |
|     31 | 6718 | `		if( pOpts ){` |
|      7 | 6719 | `			ph7_value *pMin = ph7_array_fetch(pOpts,"min_range",(int)sizeof("min_range")-1);` |
|      7 | 6720 | `			ph7_value *pMax = ph7_array_fetch(pOpts,"max_range",(int)sizeof("max_range")-1);` |
|      7 | 6721 | `			if( pMin && v<ph7_value_to_int64(pMin) ){ goto fail; }` |
|      7 | 6722 | `			if( pMax && v>ph7_value_to_int64(pMax) ){ goto fail; }` |
|      2 | 6723 | `		}` |
|     29 | 6724 | `		ph7_result_int64(pCtx,v);` |
|     29 | 6725 | `		return PH7_OK;` |
|      - | 6726 | `	}` |
|     34 | 6727 | `	case FV_VALIDATE_FLOAT: {` |
|      - | 6728 | `		double d;` |
|     69 | 6729 | `		if( !FvValidateFloat(zVal,nVal,iFlags,&d) ){ goto fail; }` |
|     39 | 6730 | `		ph7_result_double(pCtx,d);` |
|     39 | 6731 | `		return PH7_OK;` |
|      - | 6732 | `	}` |
|     14 | 6733 | `	case FV_VALIDATE_BOOLEAN: {` |
|      - | 6734 | `		int b;` |
|     29 | 6735 | `		if( !FvValidateBool(zVal,nVal,&b) ){ goto fail; }` |
|     21 | 6736 | `		ph7_result_bool(pCtx,b);` |
|     21 | 6737 | `		return PH7_OK;` |
|      - | 6738 | `	}` |
|     25 | 6739 | `	case FV_VALIDATE_IP:     if( !FvValidateIp(zVal,nVal,iFlags) ){ goto fail; } goto pass;` |
|     11 | 6740 | `	case FV_VALIDATE_MAC:    if( !FvValidateMac(zVal,nVal) ){ goto fail; }       goto pass;` |
|     28 | 6741 | `	case FV_VALIDATE_EMAIL:  if( !FvValidateEmail(zVal,nVal) ){ goto fail; }     goto pass;` |
|     11 | 6742 | `	case FV_VALIDATE_DOMAIN: if( !FvValidateDomain(zVal,nVal) ){ goto fail; }    goto pass;` |
|     15 | 6743 | `	case FV_VALIDATE_URL:    if( !FvValidateUrl(zVal,nVal) ){ goto fail; }       goto pass;` |
|      3 | 6744 | `	case FV_VALIDATE_REGEXP: {` |
|      - | 6745 | `#ifdef PH7_ENABLE_PCRE` |
|      8 | 6746 | `		ph7_value *pRe = pOpts ? ph7_array_fetch(pOpts,"regexp",(int)sizeof("regexp")-1) : 0;` |
|      8 | 6747 | `		const char *zRe; int nRe, matched = 0;` |
|      8 | 6748 | `		if( pRe==0 ){` |
|      3 | 6749 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6750 | `				"filter_var(): \"regexp\" option is missing");` |
|      - | 6751 | `		}` |
|      5 | 6752 | `		zRe = ph7_value_to_string(pRe,&nRe);` |
|      5 | 6753 | `		if( PH7_PcreMatchQuiet(pCtx,zRe,nRe,zVal,nVal,&matched)!=SXRET_OK \|\| !matched ){ goto fail; }` |
|      3 | 6754 | `		goto pass;` |
|      - | 6755 | `#else` |
|      - | 6756 | `		goto fail;` |
|      - | 6757 | `#endif` |
|      - | 6758 | `	}` |
|      3 | 6759 | `	case FV_SANITIZE_NUMBER_INT:   FvSanitizeNumber(pCtx,zVal,nVal,0,0);      return PH7_OK;` |
|      5 | 6760 | `	case FV_SANITIZE_NUMBER_FLOAT: FvSanitizeNumber(pCtx,zVal,nVal,1,iFlags); return PH7_OK;` |
|     13 | 6761 | `	case FV_SANITIZE_SPECIAL_CHARS:      FvSanitizeSpecial(pCtx,zVal,nVal,iFlags); return PH7_OK;` |
|     25 | 6762 | `	case FV_SANITIZE_FULL_SPECIAL_CHARS: FvSanitizeFull(pCtx,zVal,nVal,iFlags);    return PH7_OK;` |
|      3 | 6763 | `	case FV_SANITIZE_EMAIL: FvSanitizeChars(pCtx,zVal,nVal,0); return PH7_OK;` |
|      3 | 6764 | `	case FV_SANITIZE_URL:   FvSanitizeChars(pCtx,zVal,nVal,1); return PH7_OK;` |
|     13 | 6765 | `	case FV_DEFAULT:` |
|      - | 6766 | `		/* FILTER_UNSAFE_RAW / FILTER_DEFAULT: pass through unchanged unless a` |
|      - | 6767 | `		 * STRIP/ENCODE flag is set, in which case apply the string filter. */` |
|     28 | 6768 | `		if( iFlags & FV_FLAG_STRING_MASK ){` |
|     15 | 6769 | `			FvSanitizeString(pCtx,zVal,nVal,iFlags);` |
|     15 | 6770 | `			return PH7_OK;` |
|      - | 6771 | `		}` |
|     14 | 6772 | `		goto pass;` |
|    ! 0 | 6773 | `	default:` |
|    ! 0 | 6774 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 6775 | `			"Unknown filter with ID %d",iFilter);` |
|    ! 0 | 6776 | `		break; /* unknown filter id -> fail */` |
|    ! 0 | 6777 | `	}` |
|     58 | 6778 | `fail:` |
|    118 | 6779 | `	if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|    114 | 6780 | `	else if( bNull ){ ph7_result_null(pCtx); }` |
|    108 | 6781 | `	else { ph7_result_bool(pCtx,0); }` |
|    118 | 6782 | `	return PH7_OK;` |
|     26 | 6783 | `pass: /* validation passed: return the (string) input unchanged */` |
|     54 | 6784 | `	ph7_result_string(pCtx,zVal,nVal);` |
|     54 | 6785 | `	return PH7_OK;` |
|    161 | 6786 | `}` |
|      - | 6787 | `/*` |
|      - | 6788 | ` * Parse the ($filter, $options) pair shared by filter_var()/filter_input() out` |
|      - | 6789 | ` * of apArg[iBase] ($filter) and apArg[iBase+1] ($options): $options is either a` |
|      - | 6790 | ` * plain flags int, or an array with 'flags' and an 'options' sub-array (whose` |
|      - | 6791 | ` * 'default' entry is the fallback value). Fills the four output pointers;` |
|      - | 6792 | ` * unset outputs keep the caller-provided defaults.` |
|      - | 6793 | ` */` |
|    328 | 6794 | `static void FvParseFilterArgs(int nArg,ph7_value **apArg,int iBase,` |
|      - | 6795 | `                              int *piFilter,int *piFlags,` |
|      - | 6796 | `                              ph7_value **ppOpts,ph7_value **ppDefault)` |
|      3 | 6797 | `{` |
|    331 | 6798 | `	if( nArg>iBase ){ *piFilter = ph7_value_to_int(apArg[iBase]); }` |
|    331 | 6799 | `	if( nArg>iBase+1 ){` |
|     88 | 6800 | `		if( ph7_value_is_array(apArg[iBase+1]) ){` |
|     42 | 6801 | `			ph7_value *pF = ph7_array_fetch(apArg[iBase+1],"flags",(int)sizeof("flags")-1);` |
|     42 | 6802 | `			if( pF ){ *piFlags = ph7_value_to_int(pF); }` |
|     42 | 6803 | `			*ppOpts = ph7_array_fetch(apArg[iBase+1],"options",(int)sizeof("options")-1);` |
|     42 | 6804 | `			if( *ppOpts && !ph7_value_is_array(*ppOpts) ){ *ppOpts = 0; }` |
|     42 | 6805 | `			if( *ppOpts ){ *ppDefault = ph7_array_fetch(*ppOpts,"default",(int)sizeof("default")-1); }` |
|     22 | 6806 | `		}else{` |
|     48 | 6807 | `			*piFlags = ph7_value_to_int(apArg[iBase+1]);` |
|      - | 6808 | `		}` |
|     43 | 6809 | `	}` |
|    331 | 6810 | `}` |
|      - | 6811 | `/*` |
|      - | 6812 | ` * filter_var($value, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 6813 | ` *  Validate or sanitize a value; see FvApplyFilter for the failure semantics.` |
|      - | 6814 | ` */` |
|    306 | 6815 | `static int PH7_builtin_filter_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6816 | `{` |
|    308 | 6817 | `	int iFilter = FV_DEFAULT, iFlags = 0;` |
|    308 | 6818 | `	ph7_value *pOpts = 0, *pDefault = 0;` |
|    308 | 6819 | `	if( nArg<1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    308 | 6820 | `	FvParseFilterArgs(nArg,apArg,1,&iFilter,&iFlags,&pOpts,&pDefault);` |
|    308 | 6821 | `	return FvApplyFilter(pCtx,apArg[0],iFilter,iFlags,pOpts,pDefault);` |
|    155 | 6822 | `}` |
|      - | 6823 | `/*` |
|      - | 6824 | ` * filter_input($type, $var_name, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 6825 | ` *  Look up $var_name in the requested INPUT_* superglobal, then apply the` |
|      - | 6826 | ` *  filter. Semantics verified byte-for-byte against php 8.5:` |
|      - | 6827 | ` *   - variable NOT set: 'default' option wins, else false when` |
|      - | 6828 | ` *     FILTER_NULL_ON_FAILURE is set, else null. (Note the null/false roles are` |
|      - | 6829 | ` *     INVERTED relative to a present value that fails validation, which yields` |
|      - | 6830 | ` *     default > null-if-NULL_ON_FAILURE > false via FvApplyFilter.)` |
|      - | 6831 | ` *   - variable present: delegate to FvApplyFilter.` |
|      - | 6832 | ` *  Divergence: php reads a SAPI snapshot of the original request variables` |
|      - | 6833 | ` *  captured at startup; PHL reads the live superglobal. In CLI they match for` |
|      - | 6834 | ` *  the SAPI-registered keys (SCRIPT_NAME/PHP_SELF/DOCUMENT_ROOT); keys added` |
|      - | 6835 | ` *  only to the live $_SERVER (REQUEST_TIME/PWD/…) are visible here but not in` |
|      - | 6836 | ` *  php's snapshot.` |
|      - | 6837 | ` */` |
|     24 | 6838 | `static int PH7_builtin_filter_input(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6839 | `{` |
|     26 | 6840 | `	int iType, iFilter = FV_DEFAULT, iFlags = 0;` |
|     26 | 6841 | `	ph7_value *pOpts = 0, *pDefault = 0, *pSuper, *pElem;` |
|      - | 6842 | `	const char *zVar, *zSuper; int nVar; sxu32 nSuper;` |
|     26 | 6843 | `	if( nArg<2 ){` |
|    ! 0 | 6844 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 6845 | `			"filter_input() expects at least 2 arguments, %d given",nArg);` |
|      - | 6846 | `	}` |
|     26 | 6847 | `	iType = ph7_value_to_int(apArg[0]);` |
|     26 | 6848 | `	switch( iType ){` |
|      3 | 6849 | `	case 0: zSuper = "_POST";   nSuper = (sxu32)sizeof("_POST")-1;   break; /* INPUT_POST */` |
|      3 | 6850 | `	case 1: zSuper = "_GET";    nSuper = (sxu32)sizeof("_GET")-1;    break; /* INPUT_GET */` |
|    ! 0 | 6851 | `	case 2: zSuper = "_COOKIE"; nSuper = (sxu32)sizeof("_COOKIE")-1; break; /* INPUT_COOKIE */` |
|    ! 0 | 6852 | `	case 4: zSuper = "_ENV";    nSuper = (sxu32)sizeof("_ENV")-1;    break; /* INPUT_ENV */` |
|     19 | 6853 | `	case 5: zSuper = "_SERVER"; nSuper = (sxu32)sizeof("_SERVER")-1; break; /* INPUT_SERVER */` |
|      1 | 6854 | `	default:` |
|      3 | 6855 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6856 | `			"filter_input(): Argument #1 ($type) must be an INPUT_* constant");` |
|      - | 6857 | `	}` |
|     23 | 6858 | `	zVar = ph7_value_to_string(apArg[1],&nVar);` |
|     23 | 6859 | `	FvParseFilterArgs(nArg,apArg,2,&iFilter,&iFlags,&pOpts,&pDefault);` |
|      - | 6860 | `	/* Resolve the variable from the superglobal (missing/non-array -> not set). */` |
|     23 | 6861 | `	pSuper = PH7_VmExtractSuper(pCtx->pVm,zSuper,nSuper);` |
|     23 | 6862 | `	pElem = (pSuper && ph7_value_is_array(pSuper))` |
|     33 | 6863 | `		? ph7_array_fetch(pSuper,zVar,nVar) : 0;` |
|     23 | 6864 | `	if( pElem==0 ){` |
|      - | 6865 | `		/* Variable not set: default > false(if NULL_ON_FAILURE) > null. Note the` |
|      - | 6866 | `		 * false/null roles are inverted vs FvApplyFilter's present-but-fails path. */` |
|     13 | 6867 | `		if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|      9 | 6868 | `		else if( iFlags & FV_NULL_ON_FAILURE ){ ph7_result_bool(pCtx,0); }` |
|      7 | 6869 | `		else { ph7_result_null(pCtx); }` |
|     13 | 6870 | `		return PH7_OK;` |
|      - | 6871 | `	}` |
|     11 | 6872 | `	return FvApplyFilter(pCtx,pElem,iFilter,iFlags,pOpts,pDefault);` |
|     14 | 6873 | `}` |
|      - | 6874 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6875 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6876 | `/*` |
|      - | 6877 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 6878 |  |
|      - | 6879 | ` */` |
|      4 | 6880 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 6881 | `	const char *zInput, /* Raw input */` |
|      - | 6882 | `	int nByte,  /* Input length */` |
|      - | 6883 | `	int delim,  /* Delimiter */` |
|      - | 6884 | `	int encl,   /* Enclosure */` |
|      - | 6885 | `	int escape,  /* Escape character */` |
|      - | 6886 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 6887 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 6888 | `	)` |
|      1 | 6889 | `{` |
|      5 | 6890 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 6891 | `	const char *zIn = zInput;` |
|      - | 6892 | `	const char *zPtr;` |
|      - | 6893 | `	int isEnc;` |
|      - | 6894 | `	/* Start processing */` |
|      8 | 6895 | `	for(;;){` |
|     17 | 6896 | `		if( zIn >= zEnd ){` |
|      - | 6897 | `			/* No more input to process */` |
|      5 | 6898 | `			break;` |
|      - | 6899 | `		}` |
|     13 | 6900 | `		isEnc = 0;` |
|     13 | 6901 | `		zPtr = zIn;` |
|      - | 6902 | `		/* Find the first delimiter */` |
|     27 | 6903 | `		while( zIn < zEnd ){` |
|     23 | 6904 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 6905 | `				/* Delimiter found,break imediately */` |
|      5 | 6906 | `				break;` |
|     15 | 6907 | `			}else if( zIn[0] == encl ){` |
|      - | 6908 | `				/* Inside enclosure? */` |
|    ! 0 | 6909 | `				isEnc = !isEnc;` |
|     15 | 6910 | `			}else if( zIn[0] == escape ){` |
|      - | 6911 | `				/* Escape sequence */` |
|    ! 0 | 6912 | `				zIn++;` |
|    ! 0 | 6913 | `			}` |
|      - | 6914 | `			/* Advance the cursor */` |
|     15 | 6915 | `			zIn++;` |
|      1 | 6916 | `		}` |
|     13 | 6917 | `		if( zIn > zPtr ){` |
|     13 | 6918 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 6919 | `			sxi32 rc;` |
|      - | 6920 | `			/* Invoke the supllied callback */` |
|     13 | 6921 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 6922 | `				zPtr++;` |
|    ! 0 | 6923 | `				nByteChunk-=2;` |
|    ! 0 | 6924 | `			}` |
|     13 | 6925 | `			if( nByteChunk > 0 ){` |
|     13 | 6926 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 6927 | `				if( rc == SXERR_ABORT ){` |
|      - | 6928 | `					/* User callback request an operation abort */` |
|    ! 0 | 6929 | `					break;` |
|      - | 6930 | `				}` |
|      6 | 6931 | `			}` |
|      6 | 6932 | `		}` |
|      - | 6933 | `		/* Ignore trailing delimiter */` |
|     21 | 6934 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 6935 | `			zIn++;` |
|      1 | 6936 | `		}` |
|      1 | 6937 | `	}` |
|      5 | 6938 | `	return SXRET_OK;` |
|      1 | 6939 | `}` |
|      - | 6940 | `/*` |
|      - | 6941 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 6942 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 6943 | ` * argument to this callback.` |
|      - | 6944 | ` */` |
|     12 | 6945 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 6946 | `{` |
|     13 | 6947 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 6948 | `	ph7_value sEntry;` |
|      - | 6949 | `	SyString sToken;` |
|      - | 6950 | `	/* Insert the token in the given array */` |
|     13 | 6951 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 6952 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 6953 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 6954 | `	if( sToken.nByte < 1){` |
|    ! 0 | 6955 | `		return SXRET_OK;` |
|      - | 6956 | `	}` |
|     13 | 6957 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 6958 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 6959 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 6960 | `	return SXRET_OK;` |
|      7 | 6961 | `}` |
|      - | 6962 | `/*` |
|      - | 6963 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 6964 | ` *  Parse a CSV string into an array.` |
|      - | 6965 | ` * Parameters` |
|      - | 6966 | ` *  $input` |
|      - | 6967 | ` *   The string to parse.` |
|      - | 6968 | ` *  $delimiter` |
|      - | 6969 | ` *   Set the field delimiter (one character only).` |
|      - | 6970 | ` *  $enclosure` |
|      - | 6971 | ` *   Set the field enclosure character (one character only).` |
|      - | 6972 | ` *  $escape` |
|      - | 6973 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 6974 | ` * Return` |
|      - | 6975 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 6976 | ` */` |
|      2 | 6977 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6978 | `{` |
|      - | 6979 | `	const char *zInput,*zPtr;` |
|      - | 6980 | `	ph7_value *pArray;` |
|      3 | 6981 | `	int delim  = ',';   /* Delimiter */` |
|      3 | 6982 | `	int encl   = '"' ;  /* Enclosure */` |
|      3 | 6983 | `	int escape = '\\';  /* Escape character */` |
|      - | 6984 | `	int nLen;` |
|      3 | 6985 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6986 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 6987 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6988 | `		return PH7_OK;` |
|      - | 6989 | `	}` |
|      - | 6990 | `	/* Extract the raw input */` |
|      3 | 6991 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 6992 | `	if( nArg > 1 ){` |
|      - | 6993 | `		int i;` |
|      3 | 6994 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 6995 | `			/* Extract the delimiter */` |
|      3 | 6996 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 6997 | `			if( i > 0 ){` |
|      3 | 6998 | `				delim = zPtr[0];` |
|      1 | 6999 | `			}` |
|      1 | 7000 | `		}` |
|      3 | 7001 | `		if( nArg > 2 ){` |
|      3 | 7002 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 7003 | `				/* Extract the enclosure */` |
|      3 | 7004 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 7005 | `				if( i > 0 ){` |
|      3 | 7006 | `					encl = zPtr[0];` |
|      1 | 7007 | `				}` |
|      1 | 7008 | `			}` |
|      3 | 7009 | `			if( nArg > 3 ){` |
|      3 | 7010 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 7011 | `					/* Extract the escape character */` |
|      3 | 7012 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 7013 | `					if( i > 0 ){` |
|      3 | 7014 | `						escape = zPtr[0];` |
|      1 | 7015 | `					}` |
|      1 | 7016 | `				}` |
|      1 | 7017 | `			}` |
|      1 | 7018 | `		}` |
|      1 | 7019 | `	}` |
|      - | 7020 | `	/* Create our array */` |
|      3 | 7021 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 7022 | `	if( pArray == 0 ){` |
|      - | 7023 | `		/* Surface a fatal instead of silently returning null on OOM */` |
|    ! 0 | 7024 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7025 | `	}` |
|      - | 7026 | `	/* Parse the raw input */` |
|      3 | 7027 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 7028 | `	/* Return the freshly created array */` |
|      3 | 7029 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 7030 | `	return PH7_OK;` |
|      2 | 7031 | `}` |
|      - | 7032 | `/*` |
|      - | 7033 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 7034 | ` * container.` |
|      - | 7035 | ` * Refer to [strip_tags()].` |
|      - | 7036 | ` */` |
|     10 | 7037 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 7038 | `{` |
|     11 | 7039 | `	const char *zEnd = &zTag[nByte];` |
|      - | 7040 | `	const char *zPtr;` |
|      - | 7041 | `	SyString sEntry;` |
|      - | 7042 | `	/* Strip tags */` |
|     10 | 7043 | `	for(;;){` |
|     45 | 7044 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 7045 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 7046 | `				zTag++;` |
|      1 | 7047 | `		}` |
|     21 | 7048 | `		if( zTag >= zEnd ){` |
|     11 | 7049 | `			break;` |
|      - | 7050 | `		}` |
|     11 | 7051 | `		zPtr = zTag;` |
|      - | 7052 | `		/* Delimit the tag */` |
|     25 | 7053 | `		while(zTag < zEnd ){` |
|     25 | 7054 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 7055 | `				/* UTF-8 stream */` |
|      3 | 7056 | `				zTag++;` |
|      5 | 7057 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 7058 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 7059 | `				break;` |
|    ! 0 | 7060 | `			}else{` |
|     13 | 7061 | `				zTag++;` |
|      - | 7062 | `			}` |
|      1 | 7063 | `		}` |
|     11 | 7064 | `		if( zTag > zPtr ){` |
|      - | 7065 | `			/* Perform the insertion */` |
|     11 | 7066 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 7067 | `			SyStringFullTrim(&sEntry);` |
|     11 | 7068 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 7069 | `		}` |
|      - | 7070 | `		/* Jump the trailing '>' */` |
|     11 | 7071 | `		zTag++;` |
|      1 | 7072 | `	}` |
|     11 | 7073 | `	return SXRET_OK;` |
|      1 | 7074 | `}` |
|      - | 7075 | `/*` |
|      - | 7076 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 7077 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 7078 | ` * Refer to [strip_tags()].` |
|      - | 7079 | ` */` |
|     36 | 7080 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 7081 | `{` |
|     37 | 7082 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 7083 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 7084 | `		SyString sTag;` |
|     85 | 7085 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 7086 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 7087 | `			zTag++;` |
|      1 | 7088 | `		}` |
|      - | 7089 | `		/* Delimit the tag */` |
|     25 | 7090 | `		zCur = zTag;` |
|     77 | 7091 | `		while(zTag < zEnd ){` |
|     77 | 7092 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 7093 | `				/* UTF-8 stream */` |
|      5 | 7094 | `				zTag++;` |
|      9 | 7095 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 7096 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 7097 | `				break;` |
|    ! 0 | 7098 | `			}else{` |
|     49 | 7099 | `				zTag++;` |
|      - | 7100 | `			}` |
|      1 | 7101 | `		}` |
|     25 | 7102 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 7103 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 7104 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 7105 | `		if( sTag.nByte > 0 ){` |
|      - | 7106 | `			SyString *aEntry,*pEntry;` |
|      - | 7107 | `			sxi32 rc;` |
|      - | 7108 | `			sxu32 n;` |
|      - | 7109 | `			/* Perform the lookup */` |
|     25 | 7110 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 7111 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 7112 | `				pEntry = &aEntry[n];` |
|      - | 7113 | `				/* Do the comparison */` |
|     25 | 7114 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 7115 | `				if( !rc ){` |
|     21 | 7116 | `					return SXRET_OK;` |
|      - | 7117 | `				}` |
|      3 | 7118 | `			}` |
|      2 | 7119 | `		}` |
|      2 | 7120 | `	}` |
|      - | 7121 | `	/* No such tag */` |
|     17 | 7122 | `	return SXERR_NOTFOUND;` |
|     19 | 7123 | `}` |
|      - | 7124 | `/*` |
|      - | 7125 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 7126 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 7127 | ` * Refer to [strip_tags()].` |
|      - | 7128 | ` */` |
|     16 | 7129 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 7130 | `{` |
|     17 | 7131 | `	const char *zEnd = &zIn[nByte];` |
|      - | 7132 | `	const char *zPtr,*zTag;` |
|      - | 7133 | `	SySet sSet;` |
|      - | 7134 | `	/* initialize the set of allowed tags */` |
|     17 | 7135 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 7136 | `	if( nTaglen > 0 ){` |
|      - | 7137 | `		/* Set of allowed tags */` |
|     11 | 7138 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 7139 | `	}` |
|      - | 7140 | `	/* Set the empty string */` |
|     17 | 7141 | `	ph7_result_string(pCtx,"",0);` |
|      - | 7142 | `	/* Start processing */` |
|     26 | 7143 | `	for(;;){` |
|     53 | 7144 | `		if(zIn >= zEnd){` |
|      - | 7145 | `			/* No more input to process */` |
|     15 | 7146 | `			break;` |
|      - | 7147 | `		}` |
|     39 | 7148 | `		zPtr = zIn;` |
|      - | 7149 | `		/* Find a tag */` |
|    133 | 7150 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 7151 | `			zIn++;` |
|      1 | 7152 | `		}` |
|     39 | 7153 | `		if( zIn > zPtr ){` |
|      - | 7154 | `			/* Consume raw input */` |
|     21 | 7155 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 7156 | `		}` |
|      - | 7157 | `		/* Ignore trailing null bytes */` |
|     39 | 7158 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 7159 | `			zIn++;` |
|    ! 0 | 7160 | `		}` |
|     39 | 7161 | `		if(zIn >= zEnd){` |
|      - | 7162 | `			/* No more input to process */` |
|      3 | 7163 | `			break;` |
|      - | 7164 | `		}` |
|     37 | 7165 | `		if( zIn[0] == '<' ){` |
|      - | 7166 | `			sxi32 rc;` |
|     37 | 7167 | `			zTag = zIn++;` |
|      - | 7168 | `			/* Delimit the tag */` |
|    127 | 7169 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 7170 | `				zIn++;` |
|      1 | 7171 | `			}` |
|     37 | 7172 | `			if( zIn < zEnd ){` |
|     37 | 7173 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 7174 | `			}` |
|      - | 7175 | `			/* Query the set */` |
|     37 | 7176 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 7177 | `			if( rc == SXRET_OK ){` |
|      - | 7178 | `				/* Keep the tag */` |
|     21 | 7179 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 7180 | `			}` |
|     18 | 7181 | `		}` |
|      1 | 7182 | `	}` |
|      - | 7183 | `	/* Cleanup */` |
|     17 | 7184 | `	SySetRelease(&sSet);` |
|     17 | 7185 | `	return SXRET_OK;` |
|      1 | 7186 | `}` |
|      - | 7187 | `/*` |
|      - | 7188 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 7189 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 7190 | ` * Parameters` |
|      - | 7191 | ` *  $str` |
|      - | 7192 | ` *  The input string.` |
|      - | 7193 | ` * $allowable_tags` |
|      - | 7194 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 7195 | ` * Return` |
|      - | 7196 | ` *  Returns the stripped string.` |
|      - | 7197 | ` */` |
|     14 | 7198 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7199 | `{` |
|     15 | 7200 | `	const char *zTaglist = 0;` |
|      - | 7201 | `	const char *zString;` |
|     15 | 7202 | `	int nTaglen = 0;` |
|      - | 7203 | `	int nLen;` |
|     15 | 7204 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7205 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 7206 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7207 | `		return PH7_OK;` |
|      - | 7208 | `	}` |
|      - | 7209 | `	/* Point to the raw string */` |
|     15 | 7210 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 7211 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 7212 | `		/* Allowed tag */` |
|     11 | 7213 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 7214 | `	}` |
|      - | 7215 | `	/* Process input */` |
|     15 | 7216 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 7217 | `	return PH7_OK;` |
|      8 | 7218 | `}` |
|      - | 7219 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7220 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7221 | `/*` |
|      - | 7222 | ` * string str_shuffle(string $str)` |
|      - | 7223 |  |
|      - | 7224 | ` *  Randomly shuffles a string.` |
|      - | 7225 | ` * Parameters` |
|      - | 7226 | ` *  $str` |
|      - | 7227 | ` *   The input string.` |
|      - | 7228 | ` * Return` |
|      - | 7229 | ` *  Returns the shuffled string.` |
|      - | 7230 | ` */` |
|     10 | 7231 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7232 | `{` |
|      - | 7233 | `	const char *zString;` |
|      - | 7234 | `	int nLen,i,c;` |
|      - | 7235 | `	sxu32 iR;` |
|     11 | 7236 | `	if( nArg < 1 ){` |
|      - | 7237 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7238 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7239 | `		return PH7_OK;` |
|      - | 7240 | `	}` |
|      - | 7241 | `	/* Extract the target string */` |
|     11 | 7242 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 7243 | `	if( nLen < 1 ){` |
|      - | 7244 | `		/* Nothing to shuffle */` |
|      3 | 7245 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 7246 | `		return PH7_OK;` |
|      - | 7247 | `	}` |
|      - | 7248 | `	/* Shuffle the string */` |
|     43 | 7249 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 7250 | `		/* Generate a random number first */` |
|     35 | 7251 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 7252 | `		/* Extract a random offset */` |
|     35 | 7253 | `		c = zString[iR % nLen];` |
|      - | 7254 | `		/* Append it */` |
|     35 | 7255 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 7256 | `	}` |
|      9 | 7257 | `	return PH7_OK;` |
|      6 | 7258 | `}` |
|      - | 7259 | `/*` |
|      - | 7260 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 7261 | ` *  Convert a string to an array.` |
|      - | 7262 | ` * Parameters` |
|      - | 7263 | ` * $string` |
|      - | 7264 | ` *  The input string.` |
|      - | 7265 | ` * $split_length` |
|      - | 7266 | ` *  Maximum length of the chunk.` |
|      - | 7267 | ` * Return` |
|      - | 7268 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 7269 | ` *  except possibly the last one which may be shorter.` |
|      - | 7270 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 7271 | ` *  as the first (and only) array element.` |
|      - | 7272 | ` *  An empty string returns an empty array.` |
|      - | 7273 | ` * Errors` |
|      - | 7274 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 7275 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 7276 | ` *  ValueError if $split_length is less than 1.` |
|      - | 7277 | ` */` |
|     26 | 7278 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 7279 | `{` |
|      - | 7280 | `	const char *zString,*zEnd;` |
|      - | 7281 | `	ph7_value *pArray,*pValue;` |
|      - | 7282 | `	int split_len;` |
|      - | 7283 | `	int nLen;` |
|     29 | 7284 | `	if( nArg < 1 ){` |
|    ! 0 | 7285 | `		return PH7_VmThrowException(pCtx,` |
|      - | 7286 | `			"ArgumentCountError",` |
|      - | 7287 | `			"str_split() expects at least 1 argument, %d given",` |
|    ! 0 | 7288 | `			nArg` |
|      - | 7289 | `			);` |
|      - | 7290 | `	}` |
|      - | 7291 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     39 | 7292 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     42 | 7293 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     26 | 7294 | `	    ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 7295 | `		return PH7_VmThrowException(pCtx,` |
|      - | 7296 | `			"TypeError",` |
|      - | 7297 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 7298 | `			ph7_type_name(apArg[0])` |
|      - | 7299 | `			);` |
|      - | 7300 | `	}` |
|      - | 7301 | `	/* Point to the target string */` |
|     29 | 7302 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     29 | 7303 | `	split_len = (int)sizeof(char);` |
|     29 | 7304 | `	if( nArg > 1 ){` |
|      - | 7305 | `		/* Split length */` |
|     17 | 7306 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     17 | 7307 | `		if( split_len < 1 ){` |
|      6 | 7308 | `			return PH7_VmThrowException(pCtx,` |
|      - | 7309 | `				"ValueError",` |
|      - | 7310 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 7311 | `				);` |
|      - | 7312 | `		}` |
|     11 | 7313 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 7314 | `			split_len = nLen;` |
|      1 | 7315 | `		}` |
|      5 | 7316 | `	}` |
|      - | 7317 | `	/* Create the array and the scalar value */` |
|     23 | 7318 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 7319 | `	/*Chunk value */` |
|     23 | 7320 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     23 | 7321 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 7322 | `		/* Return FALSE */` |
|    ! 0 | 7323 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7324 | `		return PH7_OK;` |
|      - | 7325 | `	}` |
|      - | 7326 | `	/* Point to the end of the string */` |
|     23 | 7327 | `	zEnd = &zString[nLen];` |
|      - | 7328 | `	/* Perform the requested operation */` |
|    131 | 7329 | `	for(;;){` |
|      - | 7330 | `		int nMax;` |
|    143 | 7331 | `		if( zString >= zEnd ){` |
|      - | 7332 | `			/* No more input to process */` |
|     23 | 7333 | `			break;` |
|      - | 7334 | `		}` |
|    121 | 7335 | `		nMax = (int)(zEnd-zString);` |
|    121 | 7336 | `		if( nMax < split_len ){` |
|      3 | 7337 | `			split_len = nMax;` |
|      1 | 7338 | `		}` |
|      - | 7339 | `		/* Copy the current chunk */` |
|    121 | 7340 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 7341 | `		/* Insert it */` |
|    121 | 7342 | `		if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){ /* Will make it's own copy */` |
|    ! 0 | 7343 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 7344 | `		}` |
|      - | 7345 | `		/* reset the string cursor */` |
|    121 | 7346 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 7347 | `		/* Update position */` |
|    121 | 7348 | `		zString += split_len;` |
|      1 | 7349 | `	}` |
|      - | 7350 | `	/*` |
|      - | 7351 | `	 * Return the array.` |
|      - | 7352 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 7353 | `	 * upon we return from this function.` |
|      - | 7354 | `	 */` |
|     23 | 7355 | `	ph7_result_value(pCtx,pArray);` |
|     23 | 7356 | `	return PH7_OK;` |
|     16 | 7357 | `}` |
|      - | 7358 | `/*` |
|      - | 7359 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 7360 | ` * Refer to [strspn()].` |
|      - | 7361 | ` */` |
|     28 | 7362 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 7363 | `{` |
|     29 | 7364 | `	const char *zIn = *pzIn;` |
|      - | 7365 | `	const char *zPtr;` |
|      - | 7366 | `	/* Ignore leading white spaces */` |
|     29 | 7367 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 7368 | `		zIn++;` |
|    ! 0 | 7369 | `	}` |
|     29 | 7370 | `	if( zIn >= zEnd ){` |
|      - | 7371 | `		/* End of input */` |
|    ! 0 | 7372 | `		return SXERR_EOF;` |
|      - | 7373 | `	}` |
|     29 | 7374 | `	zPtr = zIn;` |
|      - | 7375 | `	/* Extract the token */` |
|    201 | 7376 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 7377 | `		zIn++;` |
|      1 | 7378 | `	}` |
|     29 | 7379 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 7380 | `	/* Synchronize pointers */` |
|     29 | 7381 | `	*pzIn = zIn;` |
|      - | 7382 | `	/* Return to the caller */` |
|     29 | 7383 | `	return SXRET_OK;` |
|     15 | 7384 | `}` |
|      - | 7385 | `/*` |
|      - | 7386 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 7387 | ` * return the longest match.` |
|      - | 7388 | ` * Refer to [strspn()].` |
|      - | 7389 | ` */` |
|     18 | 7390 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 7391 | `{` |
|     19 | 7392 | `	const char *zEnd = &zString[nLen];` |
|     19 | 7393 | `	const char *zIn = zString;` |
|      - | 7394 | `	int i,c;` |
|     45 | 7395 | `	for(;;){` |
|     91 | 7396 | `		if( zString >= zEnd ){` |
|      7 | 7397 | `			break;` |
|      - | 7398 | `		}` |
|      - | 7399 | `		/* Extract current character */` |
|     85 | 7400 | `		c = zString[0];` |
|      - | 7401 | `		/* Perform the lookup */` |
|    383 | 7402 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 7403 | `			if( c == zMask[i] ){` |
|      - | 7404 | `				/* Character found */` |
|     73 | 7405 | `				break;` |
|      - | 7406 | `			}` |
|    150 | 7407 | `		}` |
|     85 | 7408 | `		if( i >= nMaskLen ){` |
|      - | 7409 | `			/* Character not in the current mask,break immediately */` |
|     13 | 7410 | `			break;` |
|      - | 7411 | `		}` |
|      - | 7412 | `		/* Advance cursor */` |
|     73 | 7413 | `		zString++;` |
|      1 | 7414 | `	}` |
|      - | 7415 | `	/* Longest match */` |
|     19 | 7416 | `	return (int)(zString-zIn);` |
|      1 | 7417 | `}` |
|      - | 7418 | `/*` |
|      - | 7419 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 7420 | ` * Refer to [strcspn()].` |
|      - | 7421 | ` */` |
|     10 | 7422 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 7423 | `{` |
|     11 | 7424 | `	const char *zEnd = &zString[nLen];` |
|     11 | 7425 | `	const char *zIn = zString;` |
|      - | 7426 | `	int i,c;` |
|     12 | 7427 | `	for(;;){` |
|     25 | 7428 | `		if( zString >= zEnd ){` |
|      3 | 7429 | `			break;` |
|      - | 7430 | `		}` |
|      - | 7431 | `		/* Extract current character */` |
|     23 | 7432 | `		c = zString[0];` |
|      - | 7433 | `		/* Perform the lookup */` |
|     51 | 7434 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 7435 | `			if( c == zMask[i] ){` |
|      9 | 7436 | `				break;` |
|      - | 7437 | `			}` |
|     15 | 7438 | `		}` |
|     23 | 7439 | `		if( i < nMaskLen ){` |
|      - | 7440 | `			/* Character in the current mask,break immediately */` |
|      9 | 7441 | `			break;` |
|      - | 7442 | `		}` |
|      - | 7443 | `		/* Advance cursor */` |
|     15 | 7444 | `		zString++;` |
|      1 | 7445 | `	}` |
|      - | 7446 | `	/* Longest match */` |
|     11 | 7447 | `	return (int)(zString-zIn);` |
|      1 | 7448 | `}` |
|      - | 7449 | `/*` |
|      - | 7450 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 7451 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 7452 | ` *  of characters contained within a given mask.` |
|      - | 7453 | ` * Parameters` |
|      - | 7454 | ` * $str` |
|      - | 7455 | ` *  The input string.` |
|      - | 7456 | ` * $mask` |
|      - | 7457 | ` *  The list of allowable characters.` |
|      - | 7458 | ` * $start` |
|      - | 7459 | ` *  The position in subject to start searching.` |
|      - | 7460 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 7461 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 7462 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 7463 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 7464 | ` *  start'th position from the end of subject.` |
|      - | 7465 | ` * $length` |
|      - | 7466 | ` *  The length of the segment from subject to examine.` |
|      - | 7467 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 7468 | ` *  characters after the starting position.` |
|      - | 7469 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 7470 | ` *  position up to length characters from the end of subject.` |
|      - | 7471 | ` * Return` |
|      - | 7472 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 7473 | ` * in mask.` |
|      - | 7474 | ` */` |
|     24 | 7475 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7476 | `{` |
|      - | 7477 | `	const char *zString,*zMask,*zEnd;` |
|      - | 7478 | `	int iMasklen,iLen;` |
|      - | 7479 | `	SyString sToken;` |
|     25 | 7480 | `	int iCount = 0;` |
|      - | 7481 | `	int rc;` |
|     25 | 7482 | `	if( nArg < 2 ){` |
|      - | 7483 | `		/* Missing agruments,return zero */` |
|    ! 0 | 7484 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7485 | `		return PH7_OK;` |
|      - | 7486 | `	}` |
|      - | 7487 | `	/* Extract the target string */` |
|     25 | 7488 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7489 | `	/* Extract the mask */` |
|     25 | 7490 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 7491 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 7492 | `		/* Nothing to process,return zero */` |
|      7 | 7493 | `		ph7_result_int(pCtx,0);` |
|      7 | 7494 | `		return PH7_OK;` |
|      - | 7495 | `	}` |
|     19 | 7496 | `	if( nArg > 2 ){` |
|      - | 7497 | `		int nOfft;` |
|      - | 7498 | `		/* Extract the offset */` |
|      9 | 7499 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 7500 | `		if( nOfft < 0 ){` |
|    ! 0 | 7501 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 7502 | `			if( zBase > zString ){` |
|    ! 0 | 7503 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 7504 | `				zString = zBase;` |
|    ! 0 | 7505 | `			}else{` |
|      - | 7506 | `				/* Invalid offset */` |
|    ! 0 | 7507 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7508 | `				return PH7_OK;` |
|      - | 7509 | `			}` |
|    ! 0 | 7510 | `		}else{` |
|      9 | 7511 | `			if( nOfft >= iLen ){` |
|      - | 7512 | `				/* Invalid offset */` |
|    ! 0 | 7513 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7514 | `				return PH7_OK;` |
|    ! 0 | 7515 | `			}else{` |
|      - | 7516 | `				/* Update offset */` |
|      9 | 7517 | `				zString += nOfft;` |
|      9 | 7518 | `				iLen -= nOfft;` |
|      - | 7519 | `			}` |
|      - | 7520 | `		}` |
|      9 | 7521 | `		if( nArg > 3 ){` |
|      - | 7522 | `			int iUserlen;` |
|      - | 7523 | `			/* Extract the desired length */` |
|      9 | 7524 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 7525 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 7526 | `				iLen = iUserlen;` |
|      2 | 7527 | `			}` |
|      4 | 7528 | `		}` |
|      4 | 7529 | `	}` |
|      - | 7530 | `	/* Point to the end of the string */` |
|     19 | 7531 | `	zEnd = &zString[iLen];` |
|      - | 7532 | `	/* Extract the first non-space token */` |
|     19 | 7533 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 7534 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 7535 | `		/* Compare against the current mask */` |
|     19 | 7536 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 7537 | `	}` |
|      - | 7538 | `	/* Longest match */` |
|     19 | 7539 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 7540 | `	return PH7_OK;` |
|     13 | 7541 | `}` |
|      - | 7542 | `/*` |
|      - | 7543 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 7544 | ` *  Find length of initial segment not matching mask.` |
|      - | 7545 | ` * Parameters` |
|      - | 7546 | ` * $str` |
|      - | 7547 | ` *  The input string.` |
|      - | 7548 | ` * $mask` |
|      - | 7549 | ` *  The list of not allowed characters.` |
|      - | 7550 | ` * $start` |
|      - | 7551 | ` *  The position in subject to start searching.` |
|      - | 7552 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 7553 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 7554 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 7555 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 7556 | ` *  start'th position from the end of subject.` |
|      - | 7557 | ` * $length` |
|      - | 7558 | ` *  The length of the segment from subject to examine.` |
|      - | 7559 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 7560 | ` *  characters after the starting position.` |
|      - | 7561 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 7562 | ` *  position up to length characters from the end of subject.` |
|      - | 7563 | ` * Return` |
|      - | 7564 | ` *  Returns the length of the segment as an integer.` |
|      - | 7565 | ` */` |
|     14 | 7566 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7567 | `{` |
|      - | 7568 | `	const char *zString,*zMask,*zEnd;` |
|      - | 7569 | `	int iMasklen,iLen;` |
|      - | 7570 | `	SyString sToken;` |
|     15 | 7571 | `	int iCount = 0;` |
|      - | 7572 | `	int rc;` |
|     15 | 7573 | `	if( nArg < 2 ){` |
|      - | 7574 | `		/* Missing agruments,return zero */` |
|    ! 0 | 7575 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7576 | `		return PH7_OK;` |
|      - | 7577 | `	}` |
|      - | 7578 | `	/* Extract the target string */` |
|     15 | 7579 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7580 | `	/* Extract the mask */` |
|     15 | 7581 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 7582 | `	if( iLen < 1 ){` |
|      - | 7583 | `		/* Nothing to process,return zero */` |
|    ! 0 | 7584 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7585 | `		return PH7_OK;` |
|      - | 7586 | `	}` |
|     15 | 7587 | `	if( iMasklen < 1 ){` |
|      - | 7588 | `		/* No given mask,return the string length */` |
|      3 | 7589 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 7590 | `		return PH7_OK;` |
|      - | 7591 | `	}` |
|     13 | 7592 | `	if( nArg > 2 ){` |
|      - | 7593 | `		int nOfft;` |
|      - | 7594 | `		/* Extract the offset */` |
|     11 | 7595 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 7596 | `		if( nOfft < 0 ){` |
|    ! 0 | 7597 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 7598 | `			if( zBase > zString ){` |
|    ! 0 | 7599 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 7600 | `				zString = zBase;` |
|    ! 0 | 7601 | `			}else{` |
|      - | 7602 | `				/* Invalid offset */` |
|    ! 0 | 7603 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7604 | `				return PH7_OK;` |
|      - | 7605 | `			}` |
|    ! 0 | 7606 | `		}else{` |
|     11 | 7607 | `			if( nOfft >= iLen ){` |
|      - | 7608 | `				/* Invalid offset */` |
|      3 | 7609 | `				ph7_result_int(pCtx,0);` |
|      3 | 7610 | `				return PH7_OK;` |
|    ! 0 | 7611 | `			}else{` |
|      - | 7612 | `				/* Update offset */` |
|      9 | 7613 | `				zString += nOfft;` |
|      9 | 7614 | `				iLen -= nOfft;` |
|      - | 7615 | `			}` |
|      - | 7616 | `		}` |
|      9 | 7617 | `		if( nArg > 3 ){` |
|      - | 7618 | `			int iUserlen;` |
|      - | 7619 | `			/* Extract the desired length */` |
|    ! 0 | 7620 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 7621 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 7622 | `				iLen = iUserlen;` |
|    ! 0 | 7623 | `			}` |
|    ! 0 | 7624 | `		}` |
|      4 | 7625 | `	}` |
|      - | 7626 | `	/* Point to the end of the string */` |
|     11 | 7627 | `	zEnd = &zString[iLen];` |
|      - | 7628 | `	/* Extract the first non-space token */` |
|     11 | 7629 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 7630 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 7631 | `		/* Compare against the current mask */` |
|     11 | 7632 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 7633 | `	}` |
|      - | 7634 | `	/* Longest match */` |
|     11 | 7635 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 7636 | `	return PH7_OK;` |
|      8 | 7637 | `}` |
|      - | 7638 | `/*` |
|      - | 7639 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 7640 | ` *  Search a string for any of a set of characters.` |
|      - | 7641 | ` * Parameters` |
|      - | 7642 | ` *  $haystack` |
|      - | 7643 | ` *   The string where char_list is looked for.` |
|      - | 7644 | ` *  $char_list` |
|      - | 7645 | ` *   This parameter is case sensitive.` |
|      - | 7646 | ` * Return` |
|      - | 7647 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 7648 | ` */` |
|      4 | 7649 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7650 | `{` |
|      - | 7651 | `	const char *zString,*zList,*zEnd;` |
|      - | 7652 | `	int iLen,iListLen,i,c;` |
|      - | 7653 | `	sxu32 nOfft,nMax;` |
|      - | 7654 | `	sxi32 rc;` |
|      5 | 7655 | `	if( nArg < 2 ){` |
|      - | 7656 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7657 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7658 | `		return PH7_OK;` |
|      - | 7659 | `	}` |
|      - | 7660 | `	/* Extract the haystack and the char list */` |
|      5 | 7661 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 7662 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 7663 | `	if( iLen < 1 ){` |
|      - | 7664 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 7665 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7666 | `		return PH7_OK;` |
|      - | 7667 | `	}` |
|      - | 7668 | `	/* Point to the end of the string */` |
|      5 | 7669 | `	zEnd = &zString[iLen];` |
|      5 | 7670 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 7671 | `	/* perform the requested operation */` |
|     15 | 7672 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 7673 | `		c = zList[i];` |
|     11 | 7674 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 7675 | `		if( rc == SXRET_OK ){` |
|      5 | 7676 | `			if( nMax < nOfft ){` |
|      3 | 7677 | `				nOfft = nMax;` |
|      1 | 7678 | `			}` |
|      2 | 7679 | `		}` |
|      6 | 7680 | `	}` |
|      5 | 7681 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 7682 | `		/* No such substring,return FALSE */` |
|      3 | 7683 | `		ph7_result_bool(pCtx,0);` |
|      2 | 7684 | `	}else{` |
|      - | 7685 | `		/* Return the substring */` |
|      3 | 7686 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 7687 | `	}` |
|      5 | 7688 | `	return PH7_OK;` |
|      3 | 7689 | `}` |
|      - | 7690 | `/* SPDX-SnippetBegin */` |
|      - | 7691 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 7692 | `/* SPDX-License-Identifier: blessing */` |
|      - | 7693 | `/*` |
|      - | 7694 | ` * string soundex(string $str)` |
|      - | 7695 | ` *  Calculate the soundex key of a string.` |
|      - | 7696 | ` * Parameters` |
|      - | 7697 | ` *  $str` |
|      - | 7698 | ` *   The input string.` |
|      - | 7699 | ` * Return` |
|      - | 7700 | ` *  Returns the soundex key as a string.` |
|      - | 7701 | ` * Note:` |
|      - | 7702 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 7703 | ` * source tree.` |
|      - | 7704 | ` */` |
|     22 | 7705 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7706 | `{` |
|      - | 7707 | `	const unsigned char *zIn;` |
|      - | 7708 | `	char zResult[8];` |
|      - | 7709 | `	int i, j;` |
|      - | 7710 | `	static const unsigned char iCode[] = {` |
|      - | 7711 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7712 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7713 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7714 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7715 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 7716 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 7717 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 7718 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 7719 | `	};` |
|     23 | 7720 | `	if( nArg < 1 ){` |
|      - | 7721 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7722 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7723 | `		return PH7_OK;` |
|      - | 7724 | `	}` |
|     23 | 7725 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     35 | 7726 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     23 | 7727 | `	if( zIn[i] ){` |
|     17 | 7728 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 7729 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 7730 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 7731 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 7732 | `			if( code>0 ){` |
|     45 | 7733 | `				if( code!=prevcode ){` |
|     33 | 7734 | `					prevcode = (unsigned char)code;` |
|     33 | 7735 | `					zResult[j++] = (char)code + '0';` |
|     16 | 7736 | `				}` |
|     23 | 7737 | `			}else{` |
|     49 | 7738 | `				prevcode = 0;` |
|      - | 7739 | `			}` |
|     47 | 7740 | `		}` |
|     33 | 7741 | `		while( j<4 ){` |
|     17 | 7742 | `			zResult[j++] = '0';` |
|      1 | 7743 | `		}` |
|     17 | 7744 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 7745 | `	}else{` |
|      - | 7746 | `	  /* No alphabetic character: PHP returns "0000" (not the SQLite "?000"). */` |
|      7 | 7747 | `	  ph7_result_string(pCtx,"0000",4);` |
|      - | 7748 | `	}` |
|     23 | 7749 | `	return PH7_OK;` |
|     12 | 7750 | `}` |
|      - | 7751 | `/* SPDX-SnippetEnd */` |
|      - | 7752 | `/*` |
|      - | 7753 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 7754 | ` *  Wraps a string to a given number of characters.` |
|      - | 7755 | ` * Parameters` |
|      - | 7756 | ` *  $str` |
|      - | 7757 | ` *   The input string.` |
|      - | 7758 | ` * $width` |
|      - | 7759 | ` *  The column width.` |
|      - | 7760 | ` * $break` |
|      - | 7761 | ` *  The line is broken using the optional break parameter.` |
|      - | 7762 | ` * Return` |
|      - | 7763 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 7764 | ` */` |
|     26 | 7765 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7766 | `{` |
|      - | 7767 | `	const char *zIn,*zBreak;` |
|      - | 7768 | `	SyBlob sWorker;` |
|      - | 7769 | `	int iLen,iBreaklen,iWidth,iCut,iStart,iSpace,iCur;` |
|      - | 7770 | `	sxi32 rc;` |
|     27 | 7771 | `	if( nArg < 1 ){` |
|      - | 7772 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7773 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7774 | `		return PH7_OK;` |
|      - | 7775 | `	}` |
|      - | 7776 | `	/* Extract the input string */` |
|     27 | 7777 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7778 | `	/* Width (default 75; PHP allows 0/negative — break at every space). */` |
|     27 | 7779 | `	iWidth = 75;` |
|     27 | 7780 | `	if( nArg > 1 ){` |
|     27 | 7781 | `		iWidth = ph7_value_to_int(apArg[1]);` |
|     13 | 7782 | `	}` |
|      - | 7783 | `	/* Break string (default "\n"). */` |
|     27 | 7784 | `	zBreak = "\n";` |
|     27 | 7785 | `	iBreaklen = (int)sizeof(char);` |
|     27 | 7786 | `	if( nArg > 2 ){` |
|     13 | 7787 | `		zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      6 | 7788 | `	}` |
|      - | 7789 | `	/* Cut long words? (default false). */` |
|     27 | 7790 | `	iCut = 0;` |
|     27 | 7791 | `	if( nArg > 3 ){` |
|      7 | 7792 | `		iCut = ph7_value_to_bool(apArg[3]);` |
|      3 | 7793 | `	}` |
|     27 | 7794 | `	if( iLen < 1 ){` |
|      - | 7795 | `		/* PHP returns the empty string for empty input before validating the other args. */` |
|      5 | 7796 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 7797 | `		return PH7_OK;` |
|      - | 7798 | `	}` |
|      - | 7799 | `	/* PHP 8 domain errors (catchable ValueError). */` |
|     23 | 7800 | `	if( iBreaklen < 1 ){` |
|      3 | 7801 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7802 | `			"wordwrap(): Argument #3 ($break) must not be empty");` |
|      - | 7803 | `	}` |
|     21 | 7804 | `	if( iWidth == 0 && iCut ){` |
|      3 | 7805 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7806 | `			"wordwrap(): Argument #4 ($cut_long_words) cannot be true when argument #2 ($width) is 0");` |
|      - | 7807 | `	}` |
|      - | 7808 | `	/*` |
|      - | 7809 | `	 * PHP's algorithm: a single left-to-right pass tracking the start of the` |
|      - | 7810 | `	 * current line (iStart) and the position of the last space seen on it` |
|      - | 7811 | `	 * (iSpace). A break is emitted when the line reaches the width, at the last` |
|      - | 7812 | `	 * space if there was one, otherwise (only when cut is enabled) hard at the` |
|      - | 7813 | `	 * boundary. An existing break sequence in the input resets the line.` |
|      - | 7814 | `	 */` |
|     19 | 7815 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     19 | 7816 | `	iStart = iSpace = iCur = 0;` |
|     19 | 7817 | `	rc = SXRET_OK;` |
|    551 | 7818 | `	while( iCur < iLen ){` |
|    533 | 7819 | `		if( iBreaklen <= iLen - iCur && SyMemcmp(&zIn[iCur],zBreak,(sxu32)iBreaklen) == 0 ){` |
|      - | 7820 | `			/* Existing break sequence in the input: copy it verbatim and reset the line. */` |
|    ! 0 | 7821 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart + iBreaklen));` |
|    ! 0 | 7822 | `			if( rc != SXRET_OK ){ goto oom; }` |
|    ! 0 | 7823 | `			iCur += iBreaklen;` |
|    ! 0 | 7824 | `			iStart = iSpace = iCur;` |
|    ! 0 | 7825 | `			continue;` |
|    533 | 7826 | `		}else if( zIn[iCur] == ' ' ){` |
|     67 | 7827 | `			if( iCur - iStart >= iWidth ){` |
|      - | 7828 | `				/* The line already fills the width at this space: break here (the space is consumed). */` |
|     13 | 7829 | `				rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     13 | 7830 | `				if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     13 | 7831 | `				if( rc != SXRET_OK ){ goto oom; }` |
|     13 | 7832 | `				iStart = iCur + 1;` |
|      6 | 7833 | `			}` |
|     67 | 7834 | `			iSpace = iCur;` |
|    500 | 7835 | `		}else if( iCut && iCur - iStart >= iWidth && iStart >= iSpace ){` |
|      - | 7836 | `			/* A word longer than the width with no space to break at: hard-cut at the boundary. */` |
|      7 | 7837 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|      7 | 7838 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|      7 | 7839 | `			if( rc != SXRET_OK ){ goto oom; }` |
|      7 | 7840 | `			iStart = iSpace = iCur;` |
|    464 | 7841 | `		}else if( iCur - iStart >= iWidth && iStart < iSpace ){` |
|      - | 7842 | `			/* Past the width mid-word: wrap back to the last space (which is consumed). */` |
|     17 | 7843 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iSpace - iStart));` |
|     17 | 7844 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     17 | 7845 | `			if( rc != SXRET_OK ){ goto oom; }` |
|     17 | 7846 | `			iStart = iSpace = iSpace + 1;` |
|      8 | 7847 | `		}` |
|    533 | 7848 | `		iCur++;` |
|      1 | 7849 | `	}` |
|      - | 7850 | `	/* Emit the trailing chunk. */` |
|     19 | 7851 | `	if( iStart < iCur ){` |
|     19 | 7852 | `		rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     19 | 7853 | `		if( rc != SXRET_OK ){ goto oom; }` |
|      9 | 7854 | `	}` |
|     19 | 7855 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|     19 | 7856 | `	SyBlobRelease(&sWorker);` |
|     19 | 7857 | `	return PH7_OK;` |
|    ! 0 | 7858 | `oom:` |
|    ! 0 | 7859 | `	SyBlobRelease(&sWorker);` |
|    ! 0 | 7860 | `	return PH7_ContextMemoryError(pCtx);` |
|     14 | 7861 | `}` |
|      - | 7862 | `/*` |
|      - | 7863 | ` * Check if the given character is a member of the given mask.` |
|      - | 7864 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 7865 | ` * Refer to [strtok()].` |
|      - | 7866 | ` */` |
|     30 | 7867 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 7868 | `{` |
|      - | 7869 | `	int i;` |
|     57 | 7870 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 7871 | `		if( c == zMask[i] ){` |
|     13 | 7872 | `			if( pOfft ){` |
|      5 | 7873 | `				*pOfft = i;` |
|      2 | 7874 | `			}` |
|     13 | 7875 | `			return TRUE;` |
|      - | 7876 | `		}` |
|     14 | 7877 | `	}` |
|     19 | 7878 | `	return FALSE;` |
|     16 | 7879 | `}` |
|      - | 7880 | `/*` |
|      - | 7881 | ` * Extract a single token from the input stream.` |
|      - | 7882 | ` * Refer to [strtok()].` |
|      - | 7883 | ` */` |
|      6 | 7884 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 7885 | `{` |
|      7 | 7886 | `	const char *zIn = *pzIn;` |
|      - | 7887 | `	const char *zPtr;` |
|      - | 7888 | `	/* Ignore leading delimiter */` |
|     11 | 7889 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 7890 | `		zIn++;` |
|      1 | 7891 | `	}` |
|      7 | 7892 | `	if( zIn >= zEnd ){` |
|      - | 7893 | `		/* End of input */` |
|    ! 0 | 7894 | `		return SXERR_EOF;` |
|      - | 7895 | `	}` |
|      7 | 7896 | `	zPtr = zIn;` |
|      - | 7897 | `	/* Extract the token */` |
|     13 | 7898 | `	while( zIn < zEnd ){` |
|     11 | 7899 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 7900 | `			/* UTF-8 stream */` |
|    ! 0 | 7901 | `			zIn++;` |
|    ! 0 | 7902 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 7903 | `		}else{` |
|     11 | 7904 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 7905 | `				break;` |
|      - | 7906 | `			}` |
|      7 | 7907 | `			zIn++;` |
|      - | 7908 | `		}` |
|      1 | 7909 | `	}` |
|      7 | 7910 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 7911 | `	/* Update the cursor */` |
|      7 | 7912 | `	*pzIn = zIn;` |
|      - | 7913 | `	/* Return to the caller */` |
|      7 | 7914 | `	return SXRET_OK;` |
|      4 | 7915 | `}` |
|      - | 7916 | `/* strtok auxiliary private data */` |
|      - | 7917 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 7918 | `struct strtok_aux_data` |
|      - | 7919 | `{` |
|      - | 7920 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 7921 | `	const char *zIn;   /* Current input stream */` |
|      - | 7922 | `	const char *zEnd;  /* End of input */` |
|      - | 7923 | `};` |
|      - | 7924 | `/*` |
|      - | 7925 | ` * string strtok(string $str,string $token)` |
|      - | 7926 | ` * string strtok(string $token)` |
|      - | 7927 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 7928 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 7929 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 7930 | ` *  words by using the space character as the token.` |
|      - | 7931 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 7932 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 7933 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 7934 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 7935 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 7936 | ` *  the argument are found.` |
|      - | 7937 | ` * Parameters` |
|      - | 7938 | ` *  $str` |
|      - | 7939 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 7940 | ` * $token` |
|      - | 7941 | ` *  The delimiter used when splitting up str.` |
|      - | 7942 | ` * Return` |
|      - | 7943 | ` *   Current token or FALSE on EOF.` |
|      - | 7944 | ` */` |
|      6 | 7945 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7946 | `{` |
|      - | 7947 | `	strtok_aux_data *pAux;` |
|      - | 7948 | `	const char *zMask;` |
|      - | 7949 | `	SyString sToken;` |
|      - | 7950 | `	int nMasklen;` |
|      - | 7951 | `	sxi32 rc;` |
|      7 | 7952 | `	if( nArg < 2 ){` |
|      - | 7953 | `		/* Extract top aux data */` |
|      5 | 7954 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      5 | 7955 | `		if( pAux == 0 ){` |
|      - | 7956 | `			/* No aux data,return FALSE */` |
|    ! 0 | 7957 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7958 | `			return PH7_OK;` |
|      - | 7959 | `		}` |
|      5 | 7960 | `		nMasklen = 0;` |
|      5 | 7961 | `		zMask = ""; /* cc warning */` |
|      5 | 7962 | `		if( nArg > 0 ){` |
|      - | 7963 | `			/* Extract the mask */` |
|      5 | 7964 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 7965 | `		}` |
|      5 | 7966 | `		if( nMasklen < 1 ){` |
|      - | 7967 | `			/* Invalid mask,return FALSE */` |
|    ! 0 | 7968 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 7969 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7970 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 7971 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7972 | `			return PH7_OK;` |
|      - | 7973 | `		}` |
|      - | 7974 | `		/* Extract the token */` |
|      5 | 7975 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 7976 | `		if( rc != SXRET_OK ){` |
|      - | 7977 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 7978 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 7979 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7980 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 7981 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7982 | `		}else{` |
|      - | 7983 | `			/* Return the extracted token */` |
|      5 | 7984 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 7985 | `		}` |
|      3 | 7986 | `	}else{` |
|      - | 7987 | `		const char *zInput,*zCur;` |
|      - | 7988 | `		char *zDup;` |
|      - | 7989 | `		int nLen;` |
|      - | 7990 | `		/* Extract the raw input */` |
|      3 | 7991 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 7992 | `		if( nLen < 1 ){` |
|      - | 7993 | `			/* Empty input,return FALSE */` |
|    ! 0 | 7994 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7995 | `			return PH7_OK;` |
|      - | 7996 | `		}` |
|      - | 7997 | `		/* Extract the mask */` |
|      3 | 7998 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 7999 | `		if( nMasklen < 1 ){` |
|      - | 8000 | `			/* Set a default mask */` |
|      - | 8001 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 8002 | `			zMask = TOK_MASK;` |
|    ! 0 | 8003 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 8004 | `#undef TOK_MASK` |
|    ! 0 | 8005 | `		}` |
|      - | 8006 | `		/* Extract a single token */` |
|      3 | 8007 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 8008 | `		if( rc != SXRET_OK ){` |
|      - | 8009 | `			/* Empty input */` |
|    ! 0 | 8010 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 8011 | `			return PH7_OK;` |
|    ! 0 | 8012 | `		}else{` |
|      - | 8013 | `			/* Return the extracted token */` |
|      3 | 8014 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 8015 | `		}` |
|      - | 8016 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 8017 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 8018 | `		if( pAux ){` |
|      3 | 8019 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 8020 | `			if( nLen < 1 ){` |
|    ! 0 | 8021 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 8022 | `				return PH7_OK;` |
|      - | 8023 | `			}` |
|      - | 8024 | `			/* Duplicate input */` |
|      3 | 8025 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 8026 | `			if( zDup  ){` |
|      3 | 8027 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 8028 | `				/* Register the aux data */` |
|      3 | 8029 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 8030 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 8031 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 8032 | `			}` |
|      1 | 8033 | `		}` |
|      - | 8034 | `	}` |
|      7 | 8035 | `	return PH7_OK;` |
|      4 | 8036 | `}` |
|      - | 8037 | `/*` |
|      - | 8038 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 8039 | ` *  Pad a string to a certain length with another string` |
|      - | 8040 | ` * Parameters` |
|      - | 8041 | ` *  $input` |
|      - | 8042 | ` *   The input string.` |
|      - | 8043 | ` * $pad_length` |
|      - | 8044 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 8045 | ` *   string, no padding takes place.` |
|      - | 8046 | ` * $pad_string` |
|      - | 8047 | ` *   Note:` |
|      - | 8048 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 8049 | ` *    divided by the pad_string's length.` |
|      - | 8050 | ` * $pad_type` |
|      - | 8051 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 8052 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 8053 | ` * Return` |
|      - | 8054 | ` *  The padded string.` |
|      - | 8055 | ` */` |
|     10 | 8056 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8057 | `{` |
|      - | 8058 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 8059 | `	const char *zIn,*zPad;` |
|     11 | 8060 | `	if( nArg < 2 ){` |
|      - | 8061 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 8062 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 8063 | `		return PH7_OK;` |
|      - | 8064 | `	}` |
|      - | 8065 | `	/* Extract the target string */` |
|     11 | 8066 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 8067 | `	/* Padding length */` |
|      - | 8068 | `	{` |
|     11 | 8069 | `		sxi64 iTmp = 0;` |
|     11 | 8070 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"str_pad",2,"$length","int",&iTmp);` |
|     11 | 8071 | `		if( rcArg != PH7_OK ){` |
|    ! 0 | 8072 | `			return rcArg;` |
|      - | 8073 | `		}` |
|     11 | 8074 | `		iRealPad = iPadlen = (int)iTmp;` |
|      - | 8075 | `	}` |
|     11 | 8076 | `	if( iPadlen > 0 ){` |
|      9 | 8077 | `		iPadlen -= iLen;` |
|      4 | 8078 | `	}` |
|     11 | 8079 | `	if( iPadlen < 1  ){` |
|      - | 8080 | `		/* Return the string verbatim */` |
|      5 | 8081 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      5 | 8082 | `		return PH7_OK;` |
|      - | 8083 | `	}` |
|      7 | 8084 | `	zPad = " "; /* Whitespace padding */` |
|      7 | 8085 | `	iStrpad = (int)sizeof(char);` |
|      7 | 8086 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      7 | 8087 | `	if( nArg > 2 ){` |
|      - | 8088 | `		/* Padding string */` |
|      7 | 8089 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      7 | 8090 | `		if( iStrpad < 1 ){` |
|      - | 8091 | `			/* An empty pad string throws a catchable ValueError in PHP 8` |
|      - | 8092 | `			 * (only reached once padding is actually required). */` |
|      3 | 8093 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 8094 | `				"str_pad(): Argument #3 ($pad_string) must not be empty");` |
|      - | 8095 | `		}` |
|      5 | 8096 | `		if( nArg > 3 ){` |
|      - | 8097 | `			/* Padd type */` |
|      5 | 8098 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 8099 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 8100 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 8101 | `			}` |
|      2 | 8102 | `		}` |
|      2 | 8103 | `	}` |
|      5 | 8104 | `	iDiv = 1;` |
|      5 | 8105 | `	if( iType == 2 ){` |
|    ! 0 | 8106 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 8107 | `	}` |
|      - | 8108 | `	/* Perform the requested operation */` |
|      5 | 8109 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 8110 | `		jPad = iStrpad;` |
|      5 | 8111 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 8112 | `			/* Padding */` |
|      5 | 8113 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 8114 | `				break;` |
|      - | 8115 | `			}` |
|      3 | 8116 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 8117 | `		}` |
|      3 | 8118 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 8119 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 8120 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 8121 | `				if( jPad > iStrpad ){` |
|    ! 0 | 8122 | `					jPad = iStrpad;` |
|    ! 0 | 8123 | `				}` |
|      3 | 8124 | `				if( jPad < 1){` |
|    ! 0 | 8125 | `					break;` |
|      - | 8126 | `				}` |
|      3 | 8127 | `				if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 8128 | `			}` |
|      1 | 8129 | `		}` |
|      1 | 8130 | `	}` |
|      5 | 8131 | `	if( iLen > 0 ){` |
|      - | 8132 | `		/* Append the input string */` |
|      5 | 8133 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 8134 | `	}` |
|      5 | 8135 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 8136 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 8137 | `			/* Padding */` |
|      5 | 8138 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 8139 | `				break;` |
|      - | 8140 | `			}` |
|      3 | 8141 | `			if( ph7_result_string(pCtx,zPad,iStrpad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 8142 | `		}` |
|      5 | 8143 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 8144 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 8145 | `			if( jPad > iStrpad ){` |
|    ! 0 | 8146 | `				jPad = iStrpad;` |
|    ! 0 | 8147 | `			}` |
|      3 | 8148 | `			if( jPad < 1){` |
|    ! 0 | 8149 | `				break;` |
|      - | 8150 | `			}` |
|      3 | 8151 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 8152 | `		}` |
|      1 | 8153 | `	}` |
|      5 | 8154 | `	return PH7_OK;` |
|      6 | 8155 | `}` |
|      - | 8156 | `/*` |
|      - | 8157 | ` * String replacement private data.` |
|      - | 8158 | ` */` |
|      - | 8159 | `typedef struct str_replace_data str_replace_data;` |
|      - | 8160 | `struct str_replace_data` |
|      - | 8161 | `{` |
|      - | 8162 | `	/* Used by the str_replace family to collect the search/replace arguments. */` |
|      - | 8163 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 8164 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 8165 | `	sxi32 rc;           /* Carries an allocation failure (SXERR_MEM) out of a walker */` |
|      - | 8166 | `};` |
|      - | 8167 | `/*` |
|      - | 8168 | ` * Remove a substring.` |
|      - | 8169 | ` */` |
|      - | 8170 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 8171 | `	for(;;){\` |
|      - | 8172 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 8173 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 8174 | `		++OFFT;\` |
|      - | 8175 | `	}\` |
|      - | 8176 | `}` |
|      - | 8177 | `/*` |
|      - | 8178 | ` * Shift right and insert algorithm.` |
|      - | 8179 | ` */` |
|      - | 8180 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 8181 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 8182 | `		for(;;){\` |
|      - | 8183 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 8184 | `			if(INLEN < 1 ) { break; }\` |
|      - | 8185 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 8186 | `			--INLEN; \` |
|      - | 8187 | `		}\` |
|      - | 8188 | `		for(;;){\` |
|      - | 8189 | `				if(ELEN < 1) { break; }\` |
|      - | 8190 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 8191 | `				OFFT++;\` |
|      - | 8192 | `				ENTRY++;\` |
|      - | 8193 | `				--ELEN;\` |
|      - | 8194 | `		}\` |
|      - | 8195 | `}` |
|      - | 8196 | `/*` |
|      - | 8197 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 8198 | ` * replacement string [i.e: zReplace].` |
|      - | 8199 | ` */` |
|     52 | 8200 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      5 | 8201 | `{` |
|     57 | 8202 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 8203 | `	sxu32 n,m;` |
|     57 | 8204 | `	n = SyBlobLength(pWorker);` |
|     57 | 8205 | `	m = nOfft;` |
|      - | 8206 | `	/* Delete the old entry */` |
|   6591 | 8207 | `	STRDEL(zInput,n,m,nLen);` |
|     57 | 8208 | `	SyBlobLength(pWorker) -= nLen;` |
|     57 | 8209 | `	if( nReplen > 0 ){` |
|     51 | 8210 | `		sxi32 iRep = nReplen;` |
|      - | 8211 | `		sxi32 rc;` |
|      - | 8212 | `		/*` |
|      - | 8213 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 8214 | `		 * string.` |
|      - | 8215 | `		 */` |
|     51 | 8216 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     51 | 8217 | `		if( rc != SXRET_OK ){` |
|      - | 8218 | `			/* Propagate the allocation failure so the caller can raise a fatal` |
|      - | 8219 | `			 * instead of returning a partially-replaced string as success. */` |
|    ! 0 | 8220 | `			return rc;` |
|      - | 8221 | `		}` |
|      - | 8222 | `		/* Perform the insertion now */` |
|     51 | 8223 | `		zInput = (char *)SyBlobData(pWorker);` |
|     51 | 8224 | `		n = SyBlobLength(pWorker);` |
|   6381 | 8225 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     51 | 8226 | `		SyBlobLength(pWorker) += nReplen;` |
|     23 | 8227 | `	}` |
|     57 | 8228 | `	return SXRET_OK;` |
|     31 | 8229 | `}` |
|      - | 8230 | `/*` |
|      - | 8231 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 8232 | ` * to collect search/replace string.` |
|      - | 8233 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 8234 | ` */` |
|    166 | 8235 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      5 | 8236 | `{` |
|    171 | 8237 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 8238 | `	SyString sWorker;` |
|      - | 8239 | `	const char *zIn;` |
|      - | 8240 | `	int nByte;` |
|      - | 8241 | `	/* Extract a string representation of the given argument */` |
|    171 | 8242 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|    171 | 8243 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|    171 | 8244 | `	if( nByte > 0 ){` |
|      - | 8245 | `		char *zDup;` |
|      - | 8246 | `		/* Duplicate the chunk */` |
|    169 | 8247 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 8248 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 8249 | `			);` |
|    169 | 8250 | `		if( zDup == 0 ){` |
|      - | 8251 | `			/* Allocation failure: carry it out and stop the walk so the caller` |
|      - | 8252 | `			 * raises a fatal instead of silently dropping a search/replace term. */` |
|    ! 0 | 8253 | `			pRep->rc = SXERR_MEM;` |
|    ! 0 | 8254 | `			return SXERR_MEM;` |
|      - | 8255 | `		}` |
|    169 | 8256 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 8257 | `		/* Save the chunk */` |
|    169 | 8258 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     82 | 8259 | `	}` |
|      - | 8260 | `	/* Save for later processing */` |
|    171 | 8261 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 8262 | `	/* All done */` |
|     83 | 8263 | `	SXUNUSED(pKey); /* cc warning */` |
|    171 | 8264 | `	return PH7_OK;` |
|     88 | 8265 | `}` |
|      - | 8266 | `/*` |
|      - | 8267 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 8268 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 8269 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 8270 | ` * Parameters` |
|      - | 8271 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 8272 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 8273 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 8274 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 8275 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 8276 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 8277 | ` * $search` |
|      - | 8278 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 8279 | ` *  to designate multiple needles.` |
|      - | 8280 | ` * $replace` |
|      - | 8281 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 8282 | ` *  to designate multiple replacements.` |
|      - | 8283 | ` * $subject` |
|      - | 8284 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 8285 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 8286 | ` *  of subject, and the return value is an array as well.` |
|      - | 8287 | ` * $count (Not used)` |
|      - | 8288 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 8289 | ` * Return` |
|      - | 8290 | ` * This function returns a string or an array with the replaced values.` |
|      - | 8291 | ` */` |
|  30536 | 8292 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 8293 | `{` |
|      - | 8294 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 8295 | `	ProcStringMatch xMatch;` |
|      - | 8296 | `	const char *zIn,*zFunc;` |
|      - | 8297 | `	str_replace_data sRep;` |
|      - | 8298 | `	SyBlob sWorker;` |
|      - | 8299 | `	SySet sReplace;` |
|      - | 8300 | `	SySet sSearch;` |
|      - | 8301 | `	int rep_str;` |
|      - | 8302 | `	int nByte;` |
|      - | 8303 | `	sxi32 rc;` |
|  30541 | 8304 | `	if( nArg < 3 ){` |
|      - | 8305 | `		/* Missing/Invalid arguments,return null */` |
|    ! 0 | 8306 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8307 | `		return PH7_OK;` |
|      - | 8308 | `	}` |
|      - | 8309 | `	/* Initialize fields */` |
|  30541 | 8310 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  30541 | 8311 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  30541 | 8312 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  30541 | 8313 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  30541 | 8314 | `	sRep.pCtx = pCtx;` |
|  30541 | 8315 | `	sRep.pCollector = &sSearch;` |
|  30541 | 8316 | `	rep_str = 0;` |
|      - | 8317 | `	/* Extract the subject */` |
|  30541 | 8318 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  30541 | 8319 | `	if( nByte < 1 ){` |
|      - | 8320 | `		/* Nothing to replace,return the empty string */` |
|     21 | 8321 | `		ph7_result_string(pCtx,"",0);` |
|     21 | 8322 | `		return PH7_OK;` |
|      - | 8323 | `	}` |
|      - | 8324 | `	/* Copy the subject */` |
|  30521 | 8325 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 8326 | `	/* Search string */` |
|  30521 | 8327 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 8328 | `		/* Collect search string */` |
|     83 | 8329 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|     44 | 8330 | `	}else{` |
|      - | 8331 | `		/* Single pattern */` |
|  30443 | 8332 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  30443 | 8333 | `		if( nByte < 1 ){` |
|      - | 8334 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 8335 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 8336 | `			return PH7_OK;` |
|      - | 8337 | `		}` |
|  30439 | 8338 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 8339 | `		/* Save for later processing */` |
|  30439 | 8340 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 8341 | `	}` |
|      - | 8342 | `	/* Replace string */` |
|  30517 | 8343 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 8344 | `		/* Collect replace string */` |
|      7 | 8345 | `		sRep.pCollector = &sReplace;` |
|      7 | 8346 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 8347 | `	}else{` |
|      - | 8348 | `		/* Single needle */` |
|  30511 | 8349 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  30511 | 8350 | `		rep_str = 1;` |
|  30511 | 8351 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 8352 | `		/* Save for later processing */` |
|  30511 | 8353 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 8354 | `	}` |
|      - | 8355 | `	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */` |
|  30517 | 8356 | `	if( sRep.rc != SXRET_OK ){` |
|    ! 0 | 8357 | `		SySetRelease(&sSearch);` |
|    ! 0 | 8358 | `		SySetRelease(&sReplace);` |
|    ! 0 | 8359 | `		SyBlobRelease(&sWorker);` |
|    ! 0 | 8360 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8361 | `	}` |
|      - | 8362 | `	/* Reset loop cursors */` |
|  30517 | 8363 | `	SySetResetCursor(&sSearch);` |
|  30517 | 8364 | `	SySetResetCursor(&sReplace);` |
|  30517 | 8365 | `	pReplace = pSearch = 0; /* cc warning */` |
|  30517 | 8366 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 8367 | `	/* Extract function name */` |
|  30517 | 8368 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 8369 | `	/* Set the default pattern match routine */` |
|  30517 | 8370 | `	xMatch = SyBlobSearch;` |
|  30517 | 8371 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 8372 | `		/* Case insensitive pattern match */` |
|     11 | 8373 | `		xMatch = iPatternMatch;` |
|      5 | 8374 | `	}` |
|      - | 8375 | `	/* Start the replace process */` |
|  61107 | 8376 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 8377 | `		sxu32 nCount,nOfft;` |
|  30595 | 8378 | `		if( pSearch->nByte <  1 ){` |
|      - | 8379 | `			/* Empty string,ignore */` |
|      3 | 8380 | `			continue;` |
|      - | 8381 | `		}` |
|      - | 8382 | `		/* Extract the replace string */` |
|  30593 | 8383 | `		if( rep_str ){` |
|  30583 | 8384 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|  15294 | 8385 | `		}else{` |
|     11 | 8386 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 8387 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 8388 | `				 * An empty string is used for the rest of replacement values` |
|      - | 8389 | `				 */` |
|      3 | 8390 | `				pReplace = 0;` |
|      1 | 8391 | `			}` |
|      - | 8392 | `		}` |
|  30593 | 8393 | `		if( pReplace == 0 ){` |
|      - | 8394 | `			/* Use an empty string instead */` |
|      3 | 8395 | `			pReplace = &sTemp;` |
|      1 | 8396 | `		}` |
|  30593 | 8397 | `		nOfft = nCount = 0;` |
|  15320 | 8398 | `		for(;;){` |
|  30645 | 8399 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 8400 | `				break;` |
|      - | 8401 | `			}` |
|      - | 8402 | `			/* Perform a pattern lookup */` |
|  45947 | 8403 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  30628 | 8404 | `				pSearch->nByte,&nOfft);` |
|  30633 | 8405 | `			if( rc != SXRET_OK ){` |
|      - | 8406 | `				/* Pattern not found */` |
|  30581 | 8407 | `				break;` |
|      - | 8408 | `			}` |
|      - | 8409 | `			/* Perform the replace operation */` |
|     57 | 8410 | `			rc = StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|     57 | 8411 | `			if( rc != SXRET_OK ){` |
|      - | 8412 | `				/* Allocation failure: surface a fatal instead of a partial result */` |
|    ! 0 | 8413 | `				SySetRelease(&sSearch);` |
|    ! 0 | 8414 | `				SySetRelease(&sReplace);` |
|    ! 0 | 8415 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8416 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8417 | `			}` |
|      - | 8418 | `			/* Increment offset counter */` |
|     57 | 8419 | `			nCount += nOfft + pReplace->nByte;` |
|      5 | 8420 | `		}` |
|      5 | 8421 | `	}` |
|      - | 8422 | `	/* All done,clean-up the mess left behind */` |
|  30517 | 8423 | `	rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  30517 | 8424 | `	SySetRelease(&sSearch);` |
|  30517 | 8425 | `	SySetRelease(&sReplace);` |
|  30517 | 8426 | `	SyBlobRelease(&sWorker);` |
|  30517 | 8427 | `	if( rc != PH7_OK ){` |
|    ! 0 | 8428 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8429 | `	}` |
|  30517 | 8430 | `	return PH7_OK;` |
|  15273 | 8431 | `}` |
|      - | 8432 | `/*` |
|      - | 8433 | ` * strtr() array form: a single (key,value) pair copied out of the replace_pairs` |
|      - | 8434 | ` * array. The bytes are owned by a persistent pool (see strtr_collect) rather than` |
|      - | 8435 | ` * the transient walker values, which HashmapWalk releases after each callback, so` |
|      - | 8436 | ` * we store byte offsets into that pool instead of raw pointers.` |
|      - | 8437 | ` */` |
|      - | 8438 | `typedef struct strtr_entry strtr_entry;` |
|      - | 8439 | `struct strtr_entry` |
|      - | 8440 | `{` |
|      - | 8441 | `	sxu32 nKeyOfft; /* Offset of the search key inside the pool */` |
|      - | 8442 | `	sxu32 nKeyLen;  /* Length of the search key */` |
|      - | 8443 | `	sxu32 nValOfft; /* Offset of the replacement inside the pool */` |
|      - | 8444 | `	sxu32 nValLen;  /* Length of the replacement */` |
|      - | 8445 | `};` |
|      - | 8446 | `typedef struct strtr_collect strtr_collect;` |
|      - | 8447 | `struct strtr_collect` |
|      - | 8448 | `{` |
|      - | 8449 | `	SyBlob *pPool;  /* Byte pool holding copied key + value bytes */` |
|      - | 8450 | `	SySet  *pTable; /* Set of strtr_entry (parallel offsets into pPool) */` |
|      - | 8451 | `	sxi32   rc;     /* Carries an allocation failure (SXERR_MEM) out of the walker */` |
|      - | 8452 | `};` |
|      - | 8453 | `/*` |
|      - | 8454 | ` * Collect one replace_pairs entry into the persistent pool/offset table.` |
|      - | 8455 | ` * PHP coerces both the key and the value to string (an integer key becomes its` |
|      - | 8456 | ` * decimal form) and ignores an empty-string key.` |
|      - | 8457 | ` */` |
|     20 | 8458 | `static int StrtrCollectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 8459 | `{` |
|     21 | 8460 | `	strtr_collect *pCol = (strtr_collect *)pUserData;` |
|      - | 8461 | `	const char *zKey,*zVal;` |
|      - | 8462 | `	strtr_entry sEnt;` |
|      - | 8463 | `	int nKey,nVal;` |
|     21 | 8464 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|     21 | 8465 | `	if( nKey < 1 ){` |
|      - | 8466 | `		/* PHP ignores an empty-string key (it also emits a warning we do not replicate). */` |
|      3 | 8467 | `		return PH7_OK;` |
|      - | 8468 | `	}` |
|     19 | 8469 | `	zVal = ph7_value_to_string(pData,&nVal);` |
|     19 | 8470 | `	sEnt.nKeyOfft = SyBlobLength(pCol->pPool);` |
|     19 | 8471 | `	sEnt.nKeyLen  = (sxu32)nKey;` |
|     19 | 8472 | `	if( SyBlobAppend(pCol->pPool,(const void *)zKey,(sxu32)nKey) != SXRET_OK ){` |
|    ! 0 | 8473 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8474 | `		return SXERR_ABORT;` |
|      - | 8475 | `	}` |
|     19 | 8476 | `	sEnt.nValOfft = SyBlobLength(pCol->pPool);` |
|     19 | 8477 | `	sEnt.nValLen  = (sxu32)nVal;` |
|     19 | 8478 | `	if( nVal > 0 && SyBlobAppend(pCol->pPool,(const void *)zVal,(sxu32)nVal) != SXRET_OK ){` |
|    ! 0 | 8479 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8480 | `		return SXERR_ABORT;` |
|      - | 8481 | `	}` |
|     19 | 8482 | `	if( SySetPut(pCol->pTable,(const void *)&sEnt) != SXRET_OK ){` |
|    ! 0 | 8483 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8484 | `		return SXERR_ABORT;` |
|      - | 8485 | `	}` |
|     19 | 8486 | `	return PH7_OK;` |
|     11 | 8487 | `}` |
|      - | 8488 | `/*` |
|      - | 8489 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 8490 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 8491 | ` *  Translate characters or replace substrings.` |
|      - | 8492 | ` * Parameters` |
|      - | 8493 | ` *  $str` |
|      - | 8494 | ` *  The string being translated.` |
|      - | 8495 | ` * $from` |
|      - | 8496 | ` *  The string being translated to to.` |
|      - | 8497 | ` * $to` |
|      - | 8498 | ` *  The string replacing from.` |
|      - | 8499 | ` * $replace_pairs` |
|      - | 8500 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 8501 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 8502 | ` * Return` |
|      - | 8503 | ` *  The translated string.` |
|      - | 8504 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 8505 | ` */` |
|     12 | 8506 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8507 | `{` |
|      - | 8508 | `	const char *zIn;` |
|      - | 8509 | `	int nLen;` |
|     13 | 8510 | `	if( nArg < 1 ){` |
|      - | 8511 | `		/* Nothing to replace,return FALSE */` |
|    ! 0 | 8512 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8513 | `		return PH7_OK;` |
|      - | 8514 | `	}` |
|     13 | 8515 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 8516 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 8517 | `		/* Invalid arguments */` |
|    ! 0 | 8518 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8519 | `		return PH7_OK;` |
|      - | 8520 | `	}` |
|     18 | 8521 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 8522 | `		strtr_collect sCol;` |
|      - | 8523 | `		SyBlob sPool,sWorker;` |
|      - | 8524 | `		SySet sTable;` |
|      - | 8525 | `		const char *zPool;` |
|      - | 8526 | `		strtr_entry *pEnt;` |
|      - | 8527 | `		sxi32 rc;` |
|      - | 8528 | `		int i,iRun;` |
|      - | 8529 | `		/*` |
|      - | 8530 | `		 * PHP's array-form strtr is a single left-to-right pass over the subject:` |
|      - | 8531 | `		 * at every position it substitutes the LONGEST replace_pairs key that` |
|      - | 8532 | `		 * matches there, then advances past the key (replacements are never` |
|      - | 8533 | `		 * rescanned). It is not a sequential per-key global replace. First copy` |
|      - | 8534 | `		 * the pairs into a persistent pool, then run that scan.` |
|      - | 8535 | `		 */` |
|     11 | 8536 | `		SyBlobInit(&sPool,&pCtx->pVm->sAllocator);` |
|     11 | 8537 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     11 | 8538 | `		SySetInit(&sTable,&pCtx->pVm->sAllocator,sizeof(strtr_entry));` |
|     11 | 8539 | `		sCol.pPool  = &sPool;` |
|     11 | 8540 | `		sCol.pTable = &sTable;` |
|     11 | 8541 | `		sCol.rc     = SXRET_OK;` |
|     11 | 8542 | `		ph7_array_walk(apArg[1],StrtrCollectWalker,&sCol);` |
|     11 | 8543 | `		if( sCol.rc != SXRET_OK ){` |
|      - | 8544 | `			/* Allocation failure while collecting the pairs: surface a fatal */` |
|    ! 0 | 8545 | `			SyBlobRelease(&sPool);` |
|    ! 0 | 8546 | `			SyBlobRelease(&sWorker);` |
|    ! 0 | 8547 | `			SySetRelease(&sTable);` |
|    ! 0 | 8548 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 8549 | `		}` |
|      - | 8550 | `		/* The pool is now stable, so offsets can be resolved against its base. */` |
|     11 | 8551 | `		zPool = (const char *)SyBlobData(&sPool);` |
|     11 | 8552 | `		rc = SXRET_OK;` |
|     11 | 8553 | `		iRun = 0; /* Start of the pending run of unmatched bytes copied verbatim. */` |
|     43 | 8554 | `		for( i = 0 ; i < nLen ; ){` |
|     33 | 8555 | `			strtr_entry *pBest = 0;` |
|     33 | 8556 | `			sxu32 nBest = 0;` |
|      - | 8557 | `			/* Pick the longest key that matches at the current position. */` |
|     33 | 8558 | `			SySetResetCursor(&sTable);` |
|     87 | 8559 | `			while( SXRET_OK == SySetGetNextEntry(&sTable,(void **)&pEnt) ){` |
|     54 | 8560 | `				if( pEnt->nKeyLen > nBest` |
|     50 | 8561 | `					&& pEnt->nKeyLen <= (sxu32)(nLen - i)` |
|     46 | 8562 | `					&& SyMemcmp(zPool + pEnt->nKeyOfft,zIn + i,pEnt->nKeyLen) == 0 ){` |
|     29 | 8563 | `					nBest = pEnt->nKeyLen;` |
|     29 | 8564 | `					pBest = pEnt;` |
|     14 | 8565 | `				}` |
|      1 | 8566 | `			}` |
|     33 | 8567 | `			if( pBest == 0 ){` |
|      - | 8568 | `				/* No key here: extend the literal run and copy it in one shot later. */` |
|      9 | 8569 | `				i++;` |
|      9 | 8570 | `				continue;` |
|      - | 8571 | `			}` |
|      - | 8572 | `			/* Flush the pending literal run, then the replacement. */` |
|     25 | 8573 | `			if( i > iRun ){` |
|      5 | 8574 | `				rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(i - iRun));` |
|      2 | 8575 | `			}` |
|     25 | 8576 | `			if( rc == SXRET_OK && pBest->nValLen > 0 ){` |
|     25 | 8577 | `				rc = SyBlobAppend(&sWorker,zPool + pBest->nValOfft,pBest->nValLen);` |
|     12 | 8578 | `			}` |
|     25 | 8579 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 8580 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 8581 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8582 | `				SySetRelease(&sTable);` |
|    ! 0 | 8583 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8584 | `			}` |
|     25 | 8585 | `			i += (int)pBest->nKeyLen;` |
|     25 | 8586 | `			iRun = i;` |
|      1 | 8587 | `		}` |
|      - | 8588 | `		/* Flush the trailing literal run. */` |
|     11 | 8589 | `		if( nLen > iRun ){` |
|      3 | 8590 | `			rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(nLen - iRun));` |
|      3 | 8591 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 8592 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 8593 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8594 | `				SySetRelease(&sTable);` |
|    ! 0 | 8595 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8596 | `			}` |
|      1 | 8597 | `		}` |
|      - | 8598 | `		/* All done, return the result string */` |
|     16 | 8599 | `		rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|     10 | 8600 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 8601 | `		/* Clean-up */` |
|     11 | 8602 | `		SyBlobRelease(&sPool);` |
|     11 | 8603 | `		SyBlobRelease(&sWorker);` |
|     11 | 8604 | `		SySetRelease(&sTable);` |
|     11 | 8605 | `		if( rc != PH7_OK ){` |
|    ! 0 | 8606 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 8607 | `		}` |
|      6 | 8608 | `	}else{` |
|      - | 8609 | `		int i,flen,tlen,c,iOfft;` |
|      - | 8610 | `		const char *zFrom,*zTo;` |
|      3 | 8611 | `		if( nArg < 3 ){` |
|      - | 8612 | `			/* Nothing to replace */` |
|    ! 0 | 8613 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8614 | `			return PH7_OK;` |
|      - | 8615 | `		}` |
|      - | 8616 | `		/* Extract given arguments */` |
|      3 | 8617 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 8618 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 8619 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 8620 | `			/* Nothing to replace */` |
|    ! 0 | 8621 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8622 | `			return PH7_OK;` |
|      - | 8623 | `		}` |
|      - | 8624 | `		/* Start the replace process */` |
|     13 | 8625 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 8626 | `			c = zIn[i];` |
|     11 | 8627 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 8628 | `				if ( iOfft < tlen ){` |
|      5 | 8629 | `					c = zTo[iOfft];` |
|      2 | 8630 | `				}` |
|      2 | 8631 | `			}` |
|     11 | 8632 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 8633 |  |
|      6 | 8634 | `		}` |
|      - | 8635 | `	}` |
|     13 | 8636 | `	return PH7_OK;` |
|      7 | 8637 | `}` |
|      - | 8638 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8639 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8640 | `/*` |
|      - | 8641 | ` * Parse an INI string.` |
|      - | 8642 |  |
|      - | 8643 | ` * According to wikipedia` |
|      - | 8644 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 8645 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 8646 | ` *  Format` |
|      - | 8647 | `*    Properties` |
|      - | 8648 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 8649 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 8650 | `*     Example:` |
|      - | 8651 | `*      name=value` |
|      - | 8652 | `*    Sections` |
|      - | 8653 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 8654 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 8655 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 8656 | `*     or the end of the file. Sections may not be nested.` |
|      - | 8657 | `*     Example:` |
|      - | 8658 | `*      [section]` |
|      - | 8659 | `*   Comments` |
|      - | 8660 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 8661 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 8662 | `*/` |
|     12 | 8663 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 8664 | `{` |
|      - | 8665 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 8666 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 8667 | `	SyHashEntry *pEntry;` |
|      - | 8668 | `	SyString sEntry;` |
|      - | 8669 | `	SyHash sHash;` |
|      - | 8670 | `	int c;` |
|      - | 8671 | `	/* Create an empty array and worker variables */` |
|     13 | 8672 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 8673 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 8674 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 8675 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 8676 | `		/* Out of memory: surface a fatal instead of returning FALSE */` |
|    ! 0 | 8677 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8678 | `	}` |
|     13 | 8679 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 8680 | `	pCur = pArray;` |
|      - | 8681 | `	/* Start the parse process */` |
|     21 | 8682 | `	for(;;){` |
|      - | 8683 | `		/* Ignore leading white spaces */` |
|     69 | 8684 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 8685 | `			zIn++;` |
|      1 | 8686 | `		}` |
|     43 | 8687 | `		if( zIn >= zEnd ){` |
|      - | 8688 | `			/* No more input to process */` |
|     13 | 8689 | `			break;` |
|      - | 8690 | `		}` |
|     31 | 8691 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 8692 | `			/* Comment til the end of line */` |
|    ! 0 | 8693 | `			zIn++;` |
|    ! 0 | 8694 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 8695 | `				zIn++;` |
|    ! 0 | 8696 | `			}` |
|    ! 0 | 8697 | `			continue;` |
|      - | 8698 | `		}` |
|      - | 8699 | `		/* Reset the string cursor of the working variable */` |
|     31 | 8700 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 8701 | `		if( zIn[0] == '[' ){` |
|      - | 8702 | `			/* Section: Extract the section name */` |
|      9 | 8703 | `			zIn++;` |
|      9 | 8704 | `			zCur = zIn;` |
|     73 | 8705 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 8706 | `				zIn++;` |
|      1 | 8707 | `			}` |
|      9 | 8708 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 8709 | `				/* Save the section name */` |
|      5 | 8710 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 8711 | `				SyStringFullTrim(&sEntry);` |
|      5 | 8712 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 8713 | `				if( sEntry.nByte > 0 ){` |
|      - | 8714 | `					/* Associate an array with the section */` |
|      5 | 8715 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 8716 | `					if( pSection ){` |
|      5 | 8717 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 8718 | `						pCur = pSection;` |
|      2 | 8719 | `					}` |
|      2 | 8720 | `				}` |
|      2 | 8721 | `			}` |
|      9 | 8722 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 8723 | `		}else{` |
|      - | 8724 | `			ph7_value *pOldCur;` |
|      - | 8725 | `			int is_array;` |
|      - | 8726 | `			int iLen;` |
|      - | 8727 | `			/* Properties */` |
|     23 | 8728 | `			is_array = 0;` |
|     23 | 8729 | `			zCur = zIn;` |
|     23 | 8730 | `			iLen = 0; /* cc warning */` |
|     23 | 8731 | `			pOldCur = pCur;` |
|    155 | 8732 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 8733 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 8734 | `					/* Array */` |
|    ! 0 | 8735 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 8736 | `					is_array = 1;` |
|    ! 0 | 8737 | `					if( iLen > 0 ){` |
|    ! 0 | 8738 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 8739 | `						/* Query the hashtable */` |
|    ! 0 | 8740 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 8741 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 8742 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 8743 | `						if( pEntry ){` |
|    ! 0 | 8744 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 8745 | `						}else{` |
|      - | 8746 | `							/* Create an empty array */` |
|    ! 0 | 8747 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 8748 | `							if( pvArr ){` |
|      - | 8749 | `								/* Save the entry */` |
|    ! 0 | 8750 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 8751 | `								/* Insert the entry */` |
|    ! 0 | 8752 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 8753 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 8754 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 8755 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 8756 | `							}` |
|      - | 8757 | `						}` |
|    ! 0 | 8758 | `						if( pvArr ){` |
|    ! 0 | 8759 | `							pCur = pvArr;` |
|    ! 0 | 8760 | `						}` |
|    ! 0 | 8761 | `					}` |
|    ! 0 | 8762 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 8763 | `						zIn++;` |
|    ! 0 | 8764 | `					}` |
|    ! 0 | 8765 | `				}` |
|    133 | 8766 | `				zIn++;` |
|      1 | 8767 | `			}` |
|     23 | 8768 | `			if( !is_array ){` |
|     23 | 8769 | `				iLen = (int)(zIn-zCur);` |
|     11 | 8770 | `			}` |
|      - | 8771 | `			/* Trim the key */` |
|     23 | 8772 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 8773 | `			SyStringFullTrim(&sEntry);` |
|     23 | 8774 | `			if( sEntry.nByte > 0 ){` |
|     23 | 8775 | `				if( !is_array ){` |
|      - | 8776 | `					/* Save the key name */` |
|     23 | 8777 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 8778 | `				}` |
|      - | 8779 | `				/* extract key value */` |
|     23 | 8780 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 8781 | `				zIn++; /* '=' */` |
|     39 | 8782 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 8783 | `					zIn++;` |
|      1 | 8784 | `				}` |
|     23 | 8785 | `				if( zIn < zEnd ){` |
|     21 | 8786 | `					zCur = zIn;` |
|     21 | 8787 | `					c = zIn[0];` |
|     21 | 8788 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 8789 | `						zIn++;` |
|      - | 8790 | `						/* Delimit the value */` |
|    ! 0 | 8791 | `						while( zIn < zEnd ){` |
|    ! 0 | 8792 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 8793 | `								break;` |
|      - | 8794 | `							}` |
|    ! 0 | 8795 | `							zIn++;` |
|    ! 0 | 8796 | `						}` |
|    ! 0 | 8797 | `						if( zIn < zEnd ){` |
|    ! 0 | 8798 | `							zIn++;` |
|    ! 0 | 8799 | `						}` |
|    ! 0 | 8800 | `					}else{` |
|    125 | 8801 | `						while( zIn < zEnd ){` |
|    123 | 8802 | `							if( zIn[0] == '\n' ){` |
|     19 | 8803 | `								if( zIn[-1] != '\\' ){` |
|     19 | 8804 | `									break;` |
|    ! 0 | 8805 | `								}` |
|    105 | 8806 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 8807 | `								/* Inline comments */` |
|    ! 0 | 8808 | `								break;` |
|      - | 8809 | `							}` |
|    105 | 8810 | `							zIn++;` |
|      1 | 8811 | `						}` |
|      - | 8812 | `					}` |
|      - | 8813 | `					/* Trim the value */` |
|     21 | 8814 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 8815 | `					SyStringFullTrim(&sEntry);` |
|     21 | 8816 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 8817 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 8818 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 8819 | `					}` |
|     21 | 8820 | `					if( sEntry.nByte > 0 ){` |
|     21 | 8821 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 8822 | `					}` |
|      - | 8823 | `					/* Insert the key and it's value */` |
|     21 | 8824 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 8825 | `				}` |
|     12 | 8826 | `			}else{` |
|    ! 0 | 8827 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 8828 | `					zIn++;` |
|    ! 0 | 8829 | `				}` |
|      - | 8830 | `			}` |
|     23 | 8831 | `			pCur = pOldCur;` |
|      - | 8832 | `		}` |
|      1 | 8833 | `	}` |
|     13 | 8834 | `	SyHashRelease(&sHash);` |
|      - | 8835 | `	/* Return the parse of the INI string */` |
|     13 | 8836 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 8837 | `	return SXRET_OK;` |
|      7 | 8838 | `}` |
|      - | 8839 | `/*` |
|      - | 8840 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 8841 | ` *  Parse a configuration string.` |
|      - | 8842 | ` * Parameters` |
|      - | 8843 | ` *  $ini` |
|      - | 8844 | ` *   The contents of the ini file being parsed.` |
|      - | 8845 | ` *  $process_sections` |
|      - | 8846 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 8847 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 8848 | ` *  $scanner_mode (Not used)` |
|      - | 8849 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 8850 | ` *   then option values will not be parsed.` |
|      - | 8851 | ` * Return` |
|      - | 8852 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 8853 | ` */` |
|     10 | 8854 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8855 | `{` |
|      - | 8856 | `	const char *zIni;` |
|      - | 8857 | `	int nByte;` |
|     11 | 8858 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8859 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 8860 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8861 | `		return PH7_OK;` |
|      - | 8862 | `	}` |
|      - | 8863 | `	/* Extract the raw INI buffer */` |
|     11 | 8864 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 8865 | `	/* Process the INI buffer; propagate an OOM abort so the fatal actually halts */` |
|     11 | 8866 | `	return PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|      6 | 8867 | `}` |
|      - | 8868 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8869 |  |
|      - | 8870 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8871 |  |
|      - | 8872 | `/*` |
|      - | 8873 | ` * Ctype Functions.` |
|      - | 8874 | ` * Status:` |
|      - | 8875 | ` *    Stable.` |
|      - | 8876 | ` */` |
|      - | 8877 | `/*` |
|      - | 8878 | ` * bool ctype_alnum(string $text)` |
|      - | 8879 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 8880 | ` * Parameters` |
|      - | 8881 | ` *  $text` |
|      - | 8882 | ` *   The tested string.` |
|      - | 8883 | ` * Return` |
|      - | 8884 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 8885 | ` */` |
|     72 | 8886 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8887 | `{` |
|      - | 8888 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8889 | `	int nLen;` |
|     73 | 8890 | `	if( nArg < 1 ){` |
|      - | 8891 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8892 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8893 | `		return PH7_OK;` |
|      - | 8894 | `	}` |
|      - | 8895 | `	/* Extract the target string */` |
|     73 | 8896 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     73 | 8897 | `	zEnd = &zIn[nLen];` |
|     73 | 8898 | `	if( nLen < 1 ){` |
|      - | 8899 | `		/* Empty string,return FALSE */` |
|      3 | 8900 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8901 | `		return PH7_OK;` |
|      - | 8902 | `	}` |
|      - | 8903 | `	/* Perform the requested operation */` |
|    110 | 8904 | `	for(;;){` |
|    221 | 8905 | `		if( zIn >= zEnd ){` |
|      - | 8906 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     65 | 8907 | `			ph7_result_bool(pCtx,1);` |
|     65 | 8908 | `			return PH7_OK;` |
|      - | 8909 | `		}` |
|    157 | 8910 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      7 | 8911 | `			break;` |
|      - | 8912 | `		}` |
|      - | 8913 | `		/* Point to the next character */` |
|    151 | 8914 | `		zIn++;` |
|      1 | 8915 | `	}` |
|      - | 8916 | `	/* The test failed,return FALSE */` |
|      7 | 8917 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8918 | `	return PH7_OK;` |
|     37 | 8919 | `}` |
|      - | 8920 | `/*` |
|      - | 8921 | ` * bool ctype_alpha(string $text)` |
|      - | 8922 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 8923 | ` * Parameters` |
|      - | 8924 | ` *  $text` |
|      - | 8925 | ` *   The tested string.` |
|      - | 8926 | ` * Return` |
|      - | 8927 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 8928 | ` */` |
|     16 | 8929 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8930 | `{` |
|      - | 8931 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8932 | `	int nLen;` |
|     17 | 8933 | `	if( nArg < 1 ){` |
|      - | 8934 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8935 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8936 | `		return PH7_OK;` |
|      - | 8937 | `	}` |
|      - | 8938 | `	/* Extract the target string */` |
|     17 | 8939 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8940 | `	zEnd = &zIn[nLen];` |
|     17 | 8941 | `	if( nLen < 1 ){` |
|      - | 8942 | `		/* Empty string,return FALSE */` |
|      3 | 8943 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8944 | `		return PH7_OK;` |
|      - | 8945 | `	}` |
|      - | 8946 | `	/* Perform the requested operation */` |
|     42 | 8947 | `	for(;;){` |
|     85 | 8948 | `		if( zIn >= zEnd ){` |
|      - | 8949 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8950 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8951 | `			return PH7_OK;` |
|      - | 8952 | `		}` |
|     77 | 8953 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 8954 | `			break;` |
|      - | 8955 | `		}` |
|      - | 8956 | `		/* Point to the next character */` |
|     71 | 8957 | `		zIn++;` |
|      1 | 8958 | `	}` |
|      - | 8959 | `	/* The test failed,return FALSE */` |
|      7 | 8960 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8961 | `	return PH7_OK;` |
|      9 | 8962 | `}` |
|      - | 8963 | `/*` |
|      - | 8964 | ` * bool ctype_cntrl(string $text)` |
|      - | 8965 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 8966 | ` * Parameters` |
|      - | 8967 | ` *  $text` |
|      - | 8968 | ` *   The tested string.` |
|      - | 8969 | ` * Return` |
|      - | 8970 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 8971 | ` */` |
|     16 | 8972 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8973 | `{` |
|      - | 8974 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8975 | `	int nLen;` |
|     17 | 8976 | `	if( nArg < 1 ){` |
|      - | 8977 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8978 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8979 | `		return PH7_OK;` |
|      - | 8980 | `	}` |
|      - | 8981 | `	/* Extract the target string */` |
|     17 | 8982 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8983 | `	zEnd = &zIn[nLen];` |
|     17 | 8984 | `	if( nLen < 1 ){` |
|      - | 8985 | `		/* Empty string,return FALSE */` |
|      3 | 8986 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8987 | `		return PH7_OK;` |
|      - | 8988 | `	}` |
|      - | 8989 | `	/* Perform the requested operation */` |
|     14 | 8990 | `	for(;;){` |
|     29 | 8991 | `		if( zIn >= zEnd ){` |
|      - | 8992 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8993 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8994 | `			return PH7_OK;` |
|      - | 8995 | `		}` |
|     21 | 8996 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8997 | `			/* UTF-8 stream  */` |
|    ! 0 | 8998 | `			break;` |
|      - | 8999 | `		}` |
|     21 | 9000 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 9001 | `			break;` |
|      - | 9002 | `		}` |
|      - | 9003 | `		/* Point to the next character */` |
|     15 | 9004 | `		zIn++;` |
|      1 | 9005 | `	}` |
|      - | 9006 | `	/* The test failed,return FALSE */` |
|      7 | 9007 | `	ph7_result_bool(pCtx,0);` |
|      7 | 9008 | `	return PH7_OK;` |
|      9 | 9009 | `}` |
|      - | 9010 | `/*` |
|      - | 9011 | ` * bool ctype_digit(string $text)` |
|      - | 9012 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 9013 | ` * Parameters` |
|      - | 9014 | ` *  $text` |
|      - | 9015 | ` *   The tested string.` |
|      - | 9016 | ` * Return` |
|      - | 9017 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 9018 | ` */` |
|   2632 | 9019 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 9020 | `{` |
|      - | 9021 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9022 | `	int nLen;` |
|   2637 | 9023 | `	if( nArg < 1 ){` |
|      - | 9024 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9025 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9026 | `		return PH7_OK;` |
|      - | 9027 | `	}` |
|      - | 9028 | `	/* Extract the target string */` |
|   2637 | 9029 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   2637 | 9030 | `	zEnd = &zIn[nLen];` |
|   2637 | 9031 | `	if( nLen < 1 ){` |
|      - | 9032 | `		/* Empty string,return FALSE */` |
|      9 | 9033 | `		ph7_result_bool(pCtx,0);` |
|      9 | 9034 | `		return PH7_OK;` |
|      - | 9035 | `	}` |
|      - | 9036 | `	/* Perform the requested operation */` |
|   2421 | 9037 | `	for(;;){` |
|   4847 | 9038 | `		if( zIn >= zEnd ){` |
|      - | 9039 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   2155 | 9040 | `			ph7_result_bool(pCtx,1);` |
|   2155 | 9041 | `			return PH7_OK;` |
|      - | 9042 | `		}` |
|   2697 | 9043 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9044 | `			/* UTF-8 stream  */` |
|    ! 0 | 9045 | `			break;` |
|      - | 9046 | `		}` |
|   2697 | 9047 | `		if( !SyisDigit(zIn[0]) ){` |
|    479 | 9048 | `			break;` |
|      - | 9049 | `		}` |
|      - | 9050 | `		/* Point to the next character */` |
|   2223 | 9051 | `		zIn++;` |
|      5 | 9052 | `	}` |
|      - | 9053 | `	/* The test failed,return FALSE */` |
|    479 | 9054 | `	ph7_result_bool(pCtx,0);` |
|    479 | 9055 | `	return PH7_OK;` |
|   1321 | 9056 | `}` |
|      - | 9057 | `/*` |
|      - | 9058 | ` * bool ctype_xdigit(string $text)` |
|      - | 9059 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 9060 | ` * Parameters` |
|      - | 9061 | ` *  $text` |
|      - | 9062 | ` *   The tested string.` |
|      - | 9063 | ` * Return` |
|      - | 9064 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 9065 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 9066 | ` */` |
|     38 | 9067 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 9068 | `{` |
|      - | 9069 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9070 | `	int nLen;` |
|     40 | 9071 | `	if( nArg < 1 ){` |
|      - | 9072 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9073 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9074 | `		return PH7_OK;` |
|      - | 9075 | `	}` |
|      - | 9076 | `	/* Extract the target string */` |
|     40 | 9077 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     40 | 9078 | `	zEnd = &zIn[nLen];` |
|     40 | 9079 | `	if( nLen < 1 ){` |
|      - | 9080 | `		/* Empty string,return FALSE */` |
|      3 | 9081 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9082 | `		return PH7_OK;` |
|      - | 9083 | `	}` |
|      - | 9084 | `	/* Perform the requested operation */` |
|     76 | 9085 | `	for(;;){` |
|    154 | 9086 | `		if( zIn >= zEnd ){` |
|      - | 9087 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     32 | 9088 | `			ph7_result_bool(pCtx,1);` |
|     32 | 9089 | `			return PH7_OK;` |
|      - | 9090 | `		}` |
|    124 | 9091 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9092 | `			/* UTF-8 stream  */` |
|    ! 0 | 9093 | `			break;` |
|      - | 9094 | `		}` |
|    124 | 9095 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 9096 | `			break;` |
|      - | 9097 | `		}` |
|      - | 9098 | `		/* Point to the next character */` |
|    118 | 9099 | `		zIn++;` |
|      2 | 9100 | `	}` |
|      - | 9101 | `	/* The test failed,return FALSE */` |
|      7 | 9102 | `	ph7_result_bool(pCtx,0);` |
|      7 | 9103 | `	return PH7_OK;` |
|     21 | 9104 | `}` |
|      - | 9105 | `/*` |
|      - | 9106 | ` * bool ctype_graph(string $text)` |
|      - | 9107 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 9108 | ` * Parameters` |
|      - | 9109 | ` *  $text` |
|      - | 9110 | ` *   The tested string.` |
|      - | 9111 | ` * Return` |
|      - | 9112 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 9113 | ` * (no white space), FALSE otherwise.` |
|      - | 9114 | ` */` |
|     16 | 9115 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9116 | `{` |
|      - | 9117 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9118 | `	int nLen;` |
|     17 | 9119 | `	if( nArg < 1 ){` |
|      - | 9120 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9121 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9122 | `		return PH7_OK;` |
|      - | 9123 | `	}` |
|      - | 9124 | `	/* Extract the target string */` |
|     17 | 9125 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9126 | `	zEnd = &zIn[nLen];` |
|     17 | 9127 | `	if( nLen < 1 ){` |
|      - | 9128 | `		/* Empty string,return FALSE */` |
|      3 | 9129 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9130 | `		return PH7_OK;` |
|      - | 9131 | `	}` |
|      - | 9132 | `	/* Perform the requested operation */` |
|     57 | 9133 | `	for(;;){` |
|    115 | 9134 | `		if( zIn >= zEnd ){` |
|      - | 9135 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 9136 | `			ph7_result_bool(pCtx,1);` |
|      9 | 9137 | `			return PH7_OK;` |
|      - | 9138 | `		}` |
|    107 | 9139 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9140 | `			/* UTF-8 stream  */` |
|    ! 0 | 9141 | `			break;` |
|      - | 9142 | `		}` |
|    107 | 9143 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 9144 | `			break;` |
|      - | 9145 | `		}` |
|      - | 9146 | `		/* Point to the next character */` |
|    101 | 9147 | `		zIn++;` |
|      1 | 9148 | `	}` |
|      - | 9149 | `	/* The test failed,return FALSE */` |
|      7 | 9150 | `	ph7_result_bool(pCtx,0);` |
|      7 | 9151 | `	return PH7_OK;` |
|      9 | 9152 | `}` |
|      - | 9153 | `/*` |
|      - | 9154 | ` * bool ctype_print(string $text)` |
|      - | 9155 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 9156 | ` * Parameters` |
|      - | 9157 | ` *  $text` |
|      - | 9158 | ` *   The tested string.` |
|      - | 9159 | ` * Return` |
|      - | 9160 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 9161 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 9162 | ` *  or control function at all.` |
|      - | 9163 | ` */` |
|     16 | 9164 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9165 | `{` |
|      - | 9166 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9167 | `	int nLen;` |
|     17 | 9168 | `	if( nArg < 1 ){` |
|      - | 9169 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9170 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9171 | `		return PH7_OK;` |
|      - | 9172 | `	}` |
|      - | 9173 | `	/* Extract the target string */` |
|     17 | 9174 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9175 | `	zEnd = &zIn[nLen];` |
|     17 | 9176 | `	if( nLen < 1 ){` |
|      - | 9177 | `		/* Empty string,return FALSE */` |
|      3 | 9178 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9179 | `		return PH7_OK;` |
|      - | 9180 | `	}` |
|      - | 9181 | `	/* Perform the requested operation */` |
|     63 | 9182 | `	for(;;){` |
|    127 | 9183 | `		if( zIn >= zEnd ){` |
|      - | 9184 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 9185 | `			ph7_result_bool(pCtx,1);` |
|      9 | 9186 | `			return PH7_OK;` |
|      - | 9187 | `		}` |
|    119 | 9188 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9189 | `			/* UTF-8 stream  */` |
|    ! 0 | 9190 | `			break;` |
|      - | 9191 | `		}` |
|    119 | 9192 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 9193 | `			break;` |
|      - | 9194 | `		}` |
|      - | 9195 | `		/* Point to the next character */` |
|    113 | 9196 | `		zIn++;` |
|      1 | 9197 | `	}` |
|      - | 9198 | `	/* The test failed,return FALSE */` |
|      7 | 9199 | `	ph7_result_bool(pCtx,0);` |
|      7 | 9200 | `	return PH7_OK;` |
|      9 | 9201 | `}` |
|      - | 9202 | `/*` |
|      - | 9203 | ` * bool ctype_punct(string $text)` |
|      - | 9204 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 9205 | ` * Parameters` |
|      - | 9206 | ` *  $text` |
|      - | 9207 | ` *   The tested string.` |
|      - | 9208 | ` * Return` |
|      - | 9209 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 9210 | ` *  digit or blank, FALSE otherwise.` |
|      - | 9211 | ` */` |
|     18 | 9212 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9213 | `{` |
|      - | 9214 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9215 | `	int nLen;` |
|     19 | 9216 | `	if( nArg < 1 ){` |
|      - | 9217 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9218 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9219 | `		return PH7_OK;` |
|      - | 9220 | `	}` |
|      - | 9221 | `	/* Extract the target string */` |
|     19 | 9222 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 9223 | `	zEnd = &zIn[nLen];` |
|     19 | 9224 | `	if( nLen < 1 ){` |
|      - | 9225 | `		/* Empty string,return FALSE */` |
|      3 | 9226 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9227 | `		return PH7_OK;` |
|      - | 9228 | `	}` |
|      - | 9229 | `	/* Perform the requested operation */` |
|     38 | 9230 | `	for(;;){` |
|     77 | 9231 | `		if( zIn >= zEnd ){` |
|      - | 9232 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 9233 | `			ph7_result_bool(pCtx,1);` |
|      9 | 9234 | `			return PH7_OK;` |
|      - | 9235 | `		}` |
|     69 | 9236 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9237 | `			/* UTF-8 stream  */` |
|    ! 0 | 9238 | `			break;` |
|      - | 9239 | `		}` |
|     69 | 9240 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 9241 | `			break;` |
|      - | 9242 | `		}` |
|      - | 9243 | `		/* Point to the next character */` |
|     61 | 9244 | `		zIn++;` |
|      1 | 9245 | `	}` |
|      - | 9246 | `	/* The test failed,return FALSE */` |
|      9 | 9247 | `	ph7_result_bool(pCtx,0);` |
|      9 | 9248 | `	return PH7_OK;` |
|     10 | 9249 | `}` |
|      - | 9250 | `/*` |
|      - | 9251 | ` * bool ctype_space(string $text)` |
|      - | 9252 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 9253 | ` * Parameters` |
|      - | 9254 | ` *  $text` |
|      - | 9255 | ` *   The tested string.` |
|      - | 9256 | ` * Return` |
|      - | 9257 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 9258 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 9259 | ` *  and form feed characters.` |
|      - | 9260 | ` */` |
|  64445 | 9261 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 9262 | `{` |
|      - | 9263 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9264 | `	int nLen;` |
|  64450 | 9265 | `	if( nArg < 1 ){` |
|      - | 9266 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9267 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9268 | `		return PH7_OK;` |
|      - | 9269 | `	}` |
|      - | 9270 | `	/* Extract the target string */` |
|  64450 | 9271 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  64450 | 9272 | `	zEnd = &zIn[nLen];` |
|  64450 | 9273 | `	if( nLen < 1 ){` |
|      - | 9274 | `		/* Empty string,return FALSE */` |
|      3 | 9275 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9276 | `		return PH7_OK;` |
|      - | 9277 | `	}` |
|      - | 9278 | `	/* Perform the requested operation */` |
|  33134 | 9279 | `	for(;;){` |
|  66226 | 9280 | `		if( zIn >= zEnd ){` |
|      - | 9281 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1759 | 9282 | `			ph7_result_bool(pCtx,1);` |
|   1759 | 9283 | `			return PH7_OK;` |
|      - | 9284 | `		}` |
|  64472 | 9285 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9286 | `			/* UTF-8 stream  */` |
|    ! 0 | 9287 | `			break;` |
|      - | 9288 | `		}` |
|  64472 | 9289 | `		if( !SyisSpace(zIn[0]) ){` |
|  62694 | 9290 | `			break;` |
|      - | 9291 | `		}` |
|      - | 9292 | `		/* Point to the next character */` |
|   1783 | 9293 | `		zIn++;` |
|      5 | 9294 | `	}` |
|      - | 9295 | `	/* The test failed,return FALSE */` |
|  62694 | 9296 | `	ph7_result_bool(pCtx,0);` |
|  62694 | 9297 | `	return PH7_OK;` |
|  32251 | 9298 | `}` |
|      - | 9299 | `/*` |
|      - | 9300 | ` * bool ctype_lower(string $text)` |
|      - | 9301 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 9302 | ` * Parameters` |
|      - | 9303 | ` *  $text` |
|      - | 9304 | ` *   The tested string.` |
|      - | 9305 | ` * Return` |
|      - | 9306 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 9307 | ` */` |
|     16 | 9308 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9309 | `{` |
|      - | 9310 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9311 | `	int nLen;` |
|     17 | 9312 | `	if( nArg < 1 ){` |
|      - | 9313 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9314 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9315 | `		return PH7_OK;` |
|      - | 9316 | `	}` |
|      - | 9317 | `	/* Extract the target string */` |
|     17 | 9318 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9319 | `	zEnd = &zIn[nLen];` |
|     17 | 9320 | `	if( nLen < 1 ){` |
|      - | 9321 | `		/* Empty string,return FALSE */` |
|      3 | 9322 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9323 | `		return PH7_OK;` |
|      - | 9324 | `	}` |
|      - | 9325 | `	/* Perform the requested operation */` |
|     27 | 9326 | `	for(;;){` |
|     55 | 9327 | `		if( zIn >= zEnd ){` |
|      - | 9328 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 9329 | `			ph7_result_bool(pCtx,1);` |
|      5 | 9330 | `			return PH7_OK;` |
|      - | 9331 | `		}` |
|     51 | 9332 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 9333 | `			break;` |
|      - | 9334 | `		}` |
|      - | 9335 | `		/* Point to the next character */` |
|     41 | 9336 | `		zIn++;` |
|      1 | 9337 | `	}` |
|      - | 9338 | `	/* The test failed,return FALSE */` |
|     11 | 9339 | `	ph7_result_bool(pCtx,0);` |
|     11 | 9340 | `	return PH7_OK;` |
|      9 | 9341 | `}` |
|      - | 9342 | `/*` |
|      - | 9343 | ` * bool ctype_upper(string $text)` |
|      - | 9344 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 9345 | ` * Parameters` |
|      - | 9346 | ` *  $text` |
|      - | 9347 | ` *   The tested string.` |
|      - | 9348 | ` * Return` |
|      - | 9349 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 9350 | ` */` |
|     16 | 9351 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9352 | `{` |
|      - | 9353 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9354 | `	int nLen;` |
|     17 | 9355 | `	if( nArg < 1 ){` |
|      - | 9356 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9357 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9358 | `		return PH7_OK;` |
|      - | 9359 | `	}` |
|      - | 9360 | `	/* Extract the target string */` |
|     17 | 9361 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9362 | `	zEnd = &zIn[nLen];` |
|     17 | 9363 | `	if( nLen < 1 ){` |
|      - | 9364 | `		/* Empty string,return FALSE */` |
|      3 | 9365 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9366 | `		return PH7_OK;` |
|      - | 9367 | `	}` |
|      - | 9368 | `	/* Perform the requested operation */` |
|     28 | 9369 | `	for(;;){` |
|     57 | 9370 | `		if( zIn >= zEnd ){` |
|      - | 9371 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 9372 | `			ph7_result_bool(pCtx,1);` |
|      5 | 9373 | `			return PH7_OK;` |
|      - | 9374 | `		}` |
|     53 | 9375 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 9376 | `			break;` |
|      - | 9377 | `		}` |
|      - | 9378 | `		/* Point to the next character */` |
|     43 | 9379 | `		zIn++;` |
|      1 | 9380 | `	}` |
|      - | 9381 | `	/* The test failed,return FALSE */` |
|     11 | 9382 | `	ph7_result_bool(pCtx,0);` |
|     11 | 9383 | `	return PH7_OK;` |
|      9 | 9384 | `}` |
|      - | 9385 | `/* Date/Time functions moved to builtin_date.c */` |
|      - | 9386 | `/*` |
|      - | 9387 | ` * Section:` |
|      - | 9388 | ` *    URL handling Functions.` |
|      - | 9389 | ` * Status:` |
|      - | 9390 | ` *    Stable.` |
|      - | 9391 | ` */` |
|      - | 9392 | `/*` |
|      - | 9393 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 9394 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 9395 | ` */` |
|   1270 | 9396 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 9397 | `{` |
|      - | 9398 | `	/* Store in the call context result buffer */` |
|   1272 | 9399 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1272 | 9400 | `	return SXRET_OK;` |
|      2 | 9401 | `}` |
|      - | 9402 | `/*` |
|      - | 9403 | ` * string base64_encode(string $data)` |
|      - | 9404 | ` * string convert_uuencode(string $data)` |
|      - | 9405 | ` *  Encodes data with MIME base64` |
|      - | 9406 | ` * Parameter` |
|      - | 9407 | ` *  $data` |
|      - | 9408 | ` *    Data to encode` |
|      - | 9409 | ` * Return` |
|      - | 9410 | ` *  Encoded data or FALSE on failure.` |
|      - | 9411 | ` */` |
|      6 | 9412 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9413 | `{` |
|      - | 9414 | `	const char *zIn;` |
|      - | 9415 | `	int nLen;` |
|      7 | 9416 | `	if( nArg < 1 ){` |
|      - | 9417 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9418 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9419 | `		return PH7_OK;` |
|      - | 9420 | `	}` |
|      - | 9421 | `	/* Extract the input string */` |
|      7 | 9422 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 9423 | `	if( nLen < 1 ){` |
|      - | 9424 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 9425 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9426 | `		return PH7_OK;` |
|      - | 9427 | `	}` |
|      - | 9428 | `	/* Perform the BASE64 encoding */` |
|      7 | 9429 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 9430 | `	return PH7_OK;` |
|      4 | 9431 | `}` |
|      - | 9432 | `/*` |
|      - | 9433 | ` * string base64_decode(string $data)` |
|      - | 9434 | ` * string convert_uudecode(string $data)` |
|      - | 9435 | ` *  Decodes data encoded with MIME base64` |
|      - | 9436 | ` * Parameter` |
|      - | 9437 | ` *  $data` |
|      - | 9438 | ` *    Encoded data.` |
|      - | 9439 | ` * Return` |
|      - | 9440 | ` *  Returns the original data or FALSE on failure.` |
|      - | 9441 | ` */` |
|     34 | 9442 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 9443 | `{` |
|      - | 9444 | `	const char *zIn;` |
|      - | 9445 | `	int nLen;` |
|     36 | 9446 | `	if( nArg < 1 ){` |
|      - | 9447 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9448 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9449 | `		return PH7_OK;` |
|      - | 9450 | `	}` |
|      - | 9451 | `	/* Extract the input string */` |
|     36 | 9452 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 9453 | `	if( nLen < 1 ){` |
|      - | 9454 | `		/* php decodes the empty string to the EMPTY STRING, not FALSE (FALSE is reserved` |
|      - | 9455 | `		 * for input that cannot be decoded at all). */` |
|      3 | 9456 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 9457 | `		return PH7_OK;` |
|      - | 9458 | `	}` |
|      - | 9459 | `	/* Perform the BASE64 decoding */` |
|     34 | 9460 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 9461 | `	return PH7_OK;` |
|     19 | 9462 | `}` |
|      - | 9463 | `/*` |
|      - | 9464 | ` * string urlencode(string $str)` |
|      - | 9465 | ` *  URL encoding` |
|      - | 9466 | ` * Parameter` |
|      - | 9467 | ` *  $data` |
|      - | 9468 | ` *   Input string.` |
|      - | 9469 | ` * Return` |
|      - | 9470 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 9471 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 9472 | ` *  encoded as plus (+) signs.` |
|      - | 9473 | ` */` |
|    100 | 9474 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9475 | `{` |
|      - | 9476 | `	const char *zIn;` |
|      - | 9477 | `	int nLen;` |
|    101 | 9478 | `	if( nArg < 1 ){` |
|      - | 9479 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9480 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9481 | `		return PH7_OK;` |
|      - | 9482 | `	}` |
|      - | 9483 | `	/* Extract the input string */` |
|    101 | 9484 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|    101 | 9485 | `	if( nLen < 1 ){` |
|      - | 9486 | `		/* php returns an empty string for empty input, not FALSE */` |
|      3 | 9487 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 9488 | `		return PH7_OK;` |
|      - | 9489 | `	}` |
|      - | 9490 | `	/* Perform the URL encoding */` |
|     99 | 9491 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     99 | 9492 | `	return PH7_OK;` |
|     51 | 9493 | `}` |
|      - | 9494 | `/*` |
|      - | 9495 | ` * string rawurlencode(string $str)` |
|      - | 9496 | ` *  RFC 3986 URL encoding: spaces become %20 (not '+') and '~' is left intact.` |
|      - | 9497 | ` */` |
|     14 | 9498 | `static int PH7_builtin_rawurlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9499 | `{` |
|      - | 9500 | `	const char *zIn;` |
|      - | 9501 | `	int nLen;` |
|     15 | 9502 | `	if( nArg < 1 ){` |
|      - | 9503 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9504 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9505 | `		return PH7_OK;` |
|      - | 9506 | `	}` |
|      - | 9507 | `	/* Extract the input string */` |
|     15 | 9508 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 9509 | `	if( nLen < 1 ){` |
|      - | 9510 | `		/* php returns an empty string for empty input, not FALSE */` |
|      3 | 9511 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 9512 | `		return PH7_OK;` |
|      - | 9513 | `	}` |
|      - | 9514 | `	/* Perform the RFC 3986 URL encoding */` |
|     13 | 9515 | `	SyUriEncodeRaw(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     13 | 9516 | `	return PH7_OK;` |
|      8 | 9517 | `}` |
|      - | 9518 | `/*` |
|      - | 9519 | ` * string urldecode(string $str)` |
|      - | 9520 | ` *  Decodes any %## encoding in the given string.` |
|      - | 9521 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 9522 | ` * Parameter` |
|      - | 9523 | ` *  $data` |
|      - | 9524 | ` *    Input string.` |
|      - | 9525 | ` * Return` |
|      - | 9526 | ` *  Decoded URL or FALSE on failure.` |
|      - | 9527 | ` */` |
|    110 | 9528 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9529 | `{` |
|      - | 9530 | `	const char *zIn;` |
|      - | 9531 | `	int nLen;` |
|    111 | 9532 | `	if( nArg < 1 ){` |
|      - | 9533 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9534 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9535 | `		return PH7_OK;` |
|      - | 9536 | `	}` |
|      - | 9537 | `	/* Extract the input string */` |
|    111 | 9538 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|    111 | 9539 | `	if( nLen < 1 ){` |
|      - | 9540 | `		/* php returns an empty string for empty input, not FALSE */` |
|     17 | 9541 | `		ph7_result_string(pCtx,"",0);` |
|     17 | 9542 | `		return PH7_OK;` |
|      - | 9543 | `	}` |
|      - | 9544 | `	/* Perform the URL decoding */` |
|     95 | 9545 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|     95 | 9546 | `	return PH7_OK;` |
|     56 | 9547 | `}` |
|      - | 9548 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9549 | `/* Table of the built-in functions */` |
|      - | 9550 | `/*` |
|      - | 9551 | ` * int memory_get_usage([bool $real_usage = false])` |
|      - | 9552 | ` *  Amount of memory, in bytes, currently allocated to the script through PHL's` |
|      - | 9553 | ` *  memory backend. PHL tracks the backend's real allocated bytes, so the` |
|      - | 9554 | ` *  $real_usage flag has no effect here (php's non-real figure would be smaller,` |
|      - | 9555 | ` *  reflecting Zend's emalloc bookkeeping — recorded divergence).` |
|      - | 9556 | ` */` |
|    ! 0 | 9557 | `static int PH7_builtin_memory_get_usage(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 9558 | `{` |
|    ! 0 | 9559 | `	SXUNUSED(nArg);` |
|    ! 0 | 9560 | `	SXUNUSED(apArg);` |
|    ! 0 | 9561 | `	ph7_result_int64(pCtx,(ph7_int64)pCtx->pVm->sAllocator.nMemUsed);` |
|    ! 0 | 9562 | `	return PH7_OK;` |
|    ! 0 | 9563 | `}` |
|      - | 9564 | `/*` |
|      - | 9565 | ` * int memory_get_peak_usage([bool $real_usage = false])` |
|      - | 9566 | ` *  High-water mark of memory_get_usage() over the script's lifetime.` |
|      - | 9567 | ` */` |
|      4 | 9568 | `static int PH7_builtin_memory_get_peak_usage(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9569 | `{` |
|      2 | 9570 | `	SXUNUSED(nArg);` |
|      2 | 9571 | `	SXUNUSED(apArg);` |
|      5 | 9572 | `	ph7_result_int64(pCtx,(ph7_int64)pCtx->pVm->sAllocator.nMemPeak);` |
|      5 | 9573 | `	return PH7_OK;` |
|      1 | 9574 | `}` |
|      - | 9575 | `/*` |
|      - | 9576 | ` * void memory_reset_peak_usage()` |
|      - | 9577 | ` *  Reset the peak memory usage (memory_get_peak_usage) back to the current` |
|      - | 9578 | ` *  live usage — php 8.2. Frameworks call it between tests to measure per-test` |
|      - | 9579 | ` *  peaks.` |
|      - | 9580 | ` */` |
|      4 | 9581 | `static int PH7_builtin_memory_reset_peak_usage(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9582 | `{` |
|      2 | 9583 | `	SXUNUSED(nArg);` |
|      2 | 9584 | `	SXUNUSED(apArg);` |
|      5 | 9585 | `	pCtx->pVm->sAllocator.nMemPeak = pCtx->pVm->sAllocator.nMemUsed;` |
|      5 | 9586 | `	return PH7_OK;` |
|      1 | 9587 | `}` |
|      - | 9588 | `/*` |
|      - | 9589 | ` * PHL frees values by reference count as they go out of scope, so there is no` |
|      - | 9590 | ` * mark-and-sweep cycle collector to drive. The gc_* family is provided for` |
|      - | 9591 | ` * source compatibility (real frameworks call it around test runs): the state is` |
|      - | 9592 | ` * observational and collection is a no-op. Recorded divergence from php, whose` |
|      - | 9593 | ` * collector actually reclaims reference cycles.` |
|      - | 9594 | ` */` |
|    ! 0 | 9595 | `static int PH7_builtin_gc_enable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 9596 | `{` |
|    ! 0 | 9597 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    ! 0 | 9598 | `	pCtx->pVm->bGcEnabled = 1;` |
|    ! 0 | 9599 | `	return PH7_OK;` |
|    ! 0 | 9600 | `}` |
|    ! 0 | 9601 | `static int PH7_builtin_gc_disable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 9602 | `{` |
|    ! 0 | 9603 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    ! 0 | 9604 | `	pCtx->pVm->bGcEnabled = 0;` |
|    ! 0 | 9605 | `	return PH7_OK;` |
|    ! 0 | 9606 | `}` |
|    ! 0 | 9607 | `static int PH7_builtin_gc_enabled(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 9608 | `{` |
|    ! 0 | 9609 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    ! 0 | 9610 | `	ph7_result_bool(pCtx,pCtx->pVm->bGcEnabled);` |
|    ! 0 | 9611 | `	return PH7_OK;` |
|    ! 0 | 9612 | `}` |
|    ! 0 | 9613 | `static int PH7_builtin_gc_collect_cycles(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 9614 | `{` |
|      - | 9615 | `	/* No cycle collector: nothing to reclaim. Returns the count collected (0). */` |
|    ! 0 | 9616 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    ! 0 | 9617 | `	ph7_result_int(pCtx,0);` |
|    ! 0 | 9618 | `	return PH7_OK;` |
|    ! 0 | 9619 | `}` |
|    ! 0 | 9620 | `static int PH7_builtin_gc_mem_caches(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 9621 | `{` |
|    ! 0 | 9622 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    ! 0 | 9623 | `	ph7_result_int(pCtx,0);` |
|    ! 0 | 9624 | `	return PH7_OK;` |
|    ! 0 | 9625 | `}` |
|      - | 9626 | `/*` |
|      - | 9627 | ` * array gc_status(void)` |
|      - | 9628 | ` *  php 8.3 shape. PHL never runs a collection, so every counter is zero and the` |
|      - | 9629 | ` *  timing fields are 0.0; 'running' reflects gc_enable()/gc_disable().` |
|      - | 9630 | ` */` |
|    ! 0 | 9631 | `static int PH7_builtin_gc_status(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 9632 | `{` |
|      - | 9633 | `	ph7_value *pArray,*pVal;` |
|    ! 0 | 9634 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    ! 0 | 9635 | `	pArray = ph7_context_new_array(pCtx);` |
|    ! 0 | 9636 | `	pVal = ph7_context_new_scalar(pCtx);` |
|    ! 0 | 9637 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|    ! 0 | 9638 | `		ph7_result_null(pCtx);` |
|    ! 0 | 9639 | `		return PH7_OK;` |
|      - | 9640 | `	}` |
|      - | 9641 | `	/* Key order matches php 8.3's gc_status(). */` |
|    ! 0 | 9642 | `	ph7_value_bool(pVal,pCtx->pVm->bGcEnabled); ph7_array_add_strkey_elem(pArray,"running",pVal);` |
|    ! 0 | 9643 | `	ph7_value_bool(pVal,0);      ph7_array_add_strkey_elem(pArray,"protected",pVal);` |
|    ! 0 | 9644 | `	ph7_value_bool(pVal,0);      ph7_array_add_strkey_elem(pArray,"full",pVal);` |
|    ! 0 | 9645 | `	ph7_value_int(pVal,0);       ph7_array_add_strkey_elem(pArray,"runs",pVal);` |
|    ! 0 | 9646 | `	ph7_value_int(pVal,0);       ph7_array_add_strkey_elem(pArray,"collected",pVal);` |
|    ! 0 | 9647 | `	ph7_value_int(pVal,1000);    ph7_array_add_strkey_elem(pArray,"threshold",pVal);` |
|    ! 0 | 9648 | `	ph7_value_int(pVal,0);       ph7_array_add_strkey_elem(pArray,"buffer_size",pVal);` |
|    ! 0 | 9649 | `	ph7_value_int(pVal,0);       ph7_array_add_strkey_elem(pArray,"roots",pVal);` |
|    ! 0 | 9650 | `	ph7_value_double(pVal,0.0);  ph7_array_add_strkey_elem(pArray,"application_time",pVal);` |
|    ! 0 | 9651 | `	ph7_value_double(pVal,0.0);  ph7_array_add_strkey_elem(pArray,"collector_time",pVal);` |
|    ! 0 | 9652 | `	ph7_value_double(pVal,0.0);  ph7_array_add_strkey_elem(pArray,"destructor_time",pVal);` |
|    ! 0 | 9653 | `	ph7_value_double(pVal,0.0);  ph7_array_add_strkey_elem(pArray,"free_time",pVal);` |
|    ! 0 | 9654 | `	ph7_context_release_value(pCtx,pVal);` |
|    ! 0 | 9655 | `	ph7_result_value(pCtx,pArray);` |
|    ! 0 | 9656 | `	return PH7_OK;` |
|    ! 0 | 9657 | `}` |
|      - | 9658 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 9659 | `	{ "memory_get_usage"     , PH7_builtin_memory_get_usage      },` |
|      - | 9660 | `	{ "memory_get_peak_usage", PH7_builtin_memory_get_peak_usage },` |
|      - | 9661 | `	{ "memory_reset_peak_usage", PH7_builtin_memory_reset_peak_usage },` |
|      - | 9662 | `	{ "gc_enable"            , PH7_builtin_gc_enable             },` |
|      - | 9663 | `	{ "gc_disable"           , PH7_builtin_gc_disable            },` |
|      - | 9664 | `	{ "gc_enabled"           , PH7_builtin_gc_enabled            },` |
|      - | 9665 | `	{ "gc_collect_cycles"    , PH7_builtin_gc_collect_cycles     },` |
|      - | 9666 | `	{ "gc_mem_caches"        , PH7_builtin_gc_mem_caches         },` |
|      - | 9667 | `	{ "gc_status"            , PH7_builtin_gc_status             },` |
|      - | 9668 | `	   /* Variable handling functions */` |
|      - | 9669 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 9670 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 9671 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 9672 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 9673 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 9674 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 9675 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 9676 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 9677 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 9678 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 9679 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 9680 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 9681 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 9682 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 9683 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 9684 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 9685 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 9686 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 9687 | `	{ "boolval"    , PH7_builtin_boolval     },` |
|      - | 9688 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 9689 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9690 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 9691 | `	   /* Math functions */` |
|      - | 9692 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 9693 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 9694 | `	{ "acosh" ,   PH7_builtin_acosh        },` |
|      - | 9695 | `	{ "asinh" ,   PH7_builtin_asinh        },` |
|      - | 9696 | `	{ "atanh" ,   PH7_builtin_atanh        },` |
|      - | 9697 | `	{ "expm1" ,   PH7_builtin_expm1        },` |
|      - | 9698 | `	{ "log1p" ,   PH7_builtin_log1p        },` |
|      - | 9699 | `	{ "deg2rad" , PH7_builtin_deg2rad      },` |
|      - | 9700 | `	{ "rad2deg" , PH7_builtin_rad2deg      },` |
|      - | 9701 | `	{ "fpow" ,    PH7_builtin_fpow         },` |
|      - | 9702 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 9703 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 9704 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 9705 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 9706 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 9707 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 9708 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 9709 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 9710 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 9711 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 9712 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 9713 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 9714 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 9715 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 9716 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 9717 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 9718 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 9719 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 9720 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 9721 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 9722 | `	{ "round",    PH7_builtin_round        },` |
|      - | 9723 | `	{ "intdiv",   PH7_builtin_intdiv       },` |
|      - | 9724 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 9725 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 9726 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 9727 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 9728 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 9729 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 9730 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 9731 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 9732 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9733 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9734 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 9735 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9736 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9737 | `	   /* String handling functions */` |
|      - | 9738 |  |
|      - | 9739 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 9740 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 9741 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 9742 | `	{ "substr_replace",  PH7_builtin_substr_replace },` |
|      - | 9743 | `	{ "levenshtein",     PH7_builtin_levenshtein },` |
|      - | 9744 | `	{ "similar_text",    PH7_builtin_similar_text },` |
|      - | 9745 | `	{ "str_word_count",  PH7_builtin_str_word_count },` |
|      - | 9746 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 9747 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 9748 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 9749 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 9750 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 9751 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 9752 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 9753 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 9754 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 9755 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 9756 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 9757 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 9758 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 9759 | `	{ "strnatcmp"  , PH7_builtin_strnatcmp  },` |
|      - | 9760 | `	{ "strnatcasecmp", PH7_builtin_strnatcmp },` |
|      - | 9761 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 9762 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 9763 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 9764 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 9765 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 9766 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 9767 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 9768 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 9769 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 9770 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 9771 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 9772 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 9773 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 9774 | `	{ "mb_strtolower",PH7_builtin_mb_case_f }, /* UTF-8 only (builtin_mb.c) */` |
|      - | 9775 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 9776 | `	{ "mb_strtoupper",PH7_builtin_mb_case_f }, /* UTF-8 only (builtin_mb.c) */` |
|      - | 9777 | `	{ "mb_strlen",    PH7_builtin_mb_strlen_f },` |
|      - | 9778 | `	{ "mb_substr",    PH7_builtin_mb_substr_f },` |
|      - | 9779 | `	{ "mb_convert_case", PH7_builtin_mb_convert_case_f },` |
|      - | 9780 | `	{ "mb_strpos",    PH7_builtin_mb_strpos_f },` |
|      - | 9781 | `	{ "mb_stripos",   PH7_builtin_mb_strpos_f },` |
|      - | 9782 | `	{ "mb_strrpos",   PH7_builtin_mb_strpos_f },` |
|      - | 9783 | `	{ "mb_str_split", PH7_builtin_mb_str_split_f },` |
|      - | 9784 | `	{ "mb_internal_encoding", PH7_builtin_mb_internal_encoding_f },` |
|      - | 9785 | `	{ "mb_check_encoding",    PH7_builtin_mb_check_encoding_f },` |
|      - | 9786 | `	{ "mb_strwidth",  PH7_builtin_mb_strwidth_f },` |
|      - | 9787 | `	{ "mb_chr",       PH7_builtin_mb_chr_f   },` |
|      - | 9788 | `	{ "mb_ord",       PH7_builtin_mb_ord_f   },` |
|      - | 9789 | `	{ "mb_detect_encoding", PH7_builtin_mb_detect_encoding_f },` |
|      - | 9790 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 9791 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 9792 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 9793 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 9794 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 9795 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 9796 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 9797 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 9798 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 9799 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 9800 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 9801 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 9802 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 9803 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 9804 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 9805 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 9806 | `	{ "str_contains", PH7_builtin_str_contains },` |
|      - | 9807 | `	{ "str_starts_with", PH7_builtin_str_starts_with },` |
|      - | 9808 | `	{ "str_ends_with", PH7_builtin_str_ends_with },` |
|      - | 9809 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 9810 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9811 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9812 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 9813 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 9814 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 9815 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 9816 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9817 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9818 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 9819 |  |
|      - | 9820 |  |
|      - | 9821 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 9822 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 9823 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 9824 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 9825 | `	{ "hash",         PH7_builtin_hash      },` |
|      - | 9826 | `	{ "hash_hmac",    PH7_builtin_hash_hmac },` |
|      - | 9827 | `	{ "hash_equals",  PH7_builtin_hash_equals },` |
|      - | 9828 | `	{ "hash_algos",   PH7_builtin_hash_algos },` |
|      - | 9829 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 9830 | `	{ "password_hash",         PH7_builtin_password_hash },` |
|      - | 9831 | `	{ "password_verify",       PH7_builtin_password_verify },` |
|      - | 9832 | `	{ "password_get_info",     PH7_builtin_password_get_info },` |
|      - | 9833 | `	{ "password_needs_rehash", PH7_builtin_password_needs_rehash },` |
|      - | 9834 | `	{ "filter_var",            PH7_builtin_filter_var },` |
|      - | 9835 | `	{ "filter_input",          PH7_builtin_filter_input },` |
|      - | 9836 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9837 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9838 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 9839 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 9840 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9841 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9842 |  |
|      - | 9843 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 9844 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 9845 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 9846 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 9847 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 9848 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 9849 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 9850 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 9851 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 9852 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 9853 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 9854 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 9855 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9856 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9857 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 9858 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9859 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9860 |  |
|      - | 9861 | `	         /* Ctype functions */` |
|      - | 9862 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 9863 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 9864 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 9865 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 9866 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 9867 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 9868 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 9869 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 9870 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 9871 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 9872 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 9873 | `	         /* Time functions */` |
|      - | 9874 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 9875 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 9876 | `	{ "hrtime",      PH7_builtin_hrtime       },` |
|      - | 9877 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 9878 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 9879 | `	{ "date",        PH7_builtin_date         },` |
|      - | 9880 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 9881 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 9882 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 9883 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 9884 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 9885 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 9886 | `	{ "date_default_timezone_get", PH7_builtin_date_default_timezone_get },` |
|      - | 9887 | `	{ "date_default_timezone_set", PH7_builtin_date_default_timezone_set },` |
|      - | 9888 | `	        /* URL functions */` |
|      - | 9889 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 9890 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 9891 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 9892 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 9893 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 9894 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 9895 | `	{ "rawurlencode", PH7_builtin_rawurlencode },` |
|      - | 9896 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 9897 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9898 | `};` |
|      - | 9899 | `/*` |
|      - | 9900 | ` * Register the built-in functions defined above,the array functions` |
|      - | 9901 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 9902 | ` */` |
|   3402 | 9903 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      5 | 9904 | `{` |
|      - | 9905 | `	sxu32 n;` |
| 704219 | 9906 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 700817 | 9907 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 350411 | 9908 | `	}` |
|      - | 9909 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   3407 | 9910 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 9911 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   3407 | 9912 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   3407 | 9913 | `}` |
|      - | 9914 |  |
