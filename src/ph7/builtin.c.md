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
| 492536 |   18 | `PH7_PRIVATE sxi32 PH7_IntArgResolve(` |
|      - |   19 | `	ph7_context *pCtx,` |
|      - |   20 | `	ph7_value *pArg,` |
|      - |   21 | `	const char *zFunc,` |
|      - |   22 | `	int iArgNum,` |
|      - |   23 | `	const char *zParamName,` |
|      - |   24 | `	const char *zTypeStr,` |
|      - |   25 | `	sxi64 *pOut` |
|      5 |   26 | `){` |
| 492541 |   27 | `	if( ph7_value_is_null(pArg) ){` |
|    ! 0 |   28 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|      - |   29 | `			"%s(): Passing null to parameter #%d (%s) of type %s is deprecated",` |
|    ! 0 |   30 | `			zFunc,iArgNum,zParamName,zTypeStr` |
|      - |   31 | `			);` |
|    ! 0 |   32 | `		*pOut = 0;` |
|    ! 0 |   33 | `		return PH7_OK;` |
|      - |   34 | `	}` |
| 492541 |   35 | `	if( ph7_value_is_float(pArg) ){` |
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
| 492535 |   56 | `	if( ph7_value_is_string(pArg) ){` |
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
| 492521 |   98 | `	if( !ph7_value_is_int(pArg) && !ph7_value_is_bool(pArg) ){` |
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
| 492521 |  113 | `	*pOut = ph7_value_to_int64(pArg);` |
| 492521 |  114 | `	return PH7_OK;` |
| 246273 |  115 | `}` |
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
|    924 |  178 | `static int PH7_builtin_is_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  179 | `{` |
|    928 |  180 | `	int res = 0; /* Assume false by default */` |
|    928 |  181 | `	if( nArg > 0 ){` |
|      - |  182 | `		/* Strict PHP identity: a float is never an int, even when it holds an` |
|      - |  183 | `		 * integer value (1.0). An integer-valued real carries both MEMOBJ_INT` |
|      - |  184 | `		 * (cached) and MEMOBJ_REAL, so REAL must be excluded here. */` |
|    928 |  185 | `		res = ph7_value_is_int(apArg[0]) && !ph7_value_is_float(apArg[0]);` |
|    462 |  186 | `	}` |
|      - |  187 | `	/* Query result */` |
|    928 |  188 | `	ph7_result_bool(pCtx,res);` |
|    928 |  189 | `	return PH7_OK;` |
|      4 |  190 | `}` |
|      - |  191 | `/*` |
|      - |  192 | ` * bool is_string($var)` |
|      - |  193 | ` *  Finds out whether a variable is a string.` |
|      - |  194 | ` * Parameters` |
|      - |  195 | ` *   $var: The variable being evaluated.` |
|      - |  196 | ` * Return` |
|      - |  197 | ` *  TRUE if var is string. False otherwise.` |
|      - |  198 | ` */` |
|   1114 |  199 | `static int PH7_builtin_is_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  200 | `{` |
|   1118 |  201 | `	int res = 0; /* Assume false by default */` |
|   1118 |  202 | `	if( nArg > 0 ){` |
|   1118 |  203 | `		res = ph7_value_is_string(apArg[0]);` |
|    557 |  204 | `	}` |
|      - |  205 | `	/* Query result */` |
|   1118 |  206 | `	ph7_result_bool(pCtx,res);` |
|   1118 |  207 | `	return PH7_OK;` |
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
|    760 |  278 | `static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  279 | `{` |
|    765 |  280 | `	int res = 0; /* Assume false by default */` |
|    765 |  281 | `	if( nArg > 0 ){` |
|    765 |  282 | `		res = ph7_value_is_array(apArg[0]);` |
|    380 |  283 | `	}` |
|      - |  284 | `	/* Query result */` |
|    765 |  285 | `	ph7_result_bool(pCtx,res);` |
|    765 |  286 | `	return PH7_OK;` |
|      5 |  287 | `}` |
|      - |  288 | `/*` |
|      - |  289 | ` * bool is_object($var)` |
|      - |  290 | ` *  Find out whether a variable is an object.` |
|      - |  291 | ` * Parameters` |
|      - |  292 | ` *  $var: The variable being evaluated.` |
|      - |  293 | ` * Return` |
|      - |  294 | ` *  True if var is an object. False otherwise.` |
|      - |  295 | ` */` |
|   1560 |  296 | `static int PH7_builtin_is_object(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  297 | `{` |
|   1563 |  298 | `	int res = 0; /* Assume false by default */` |
|   1563 |  299 | `	if( nArg > 0 ){` |
|   1563 |  300 | `		res = ph7_value_is_object(apArg[0]);` |
|    780 |  301 | `	}` |
|      - |  302 | `	/* Query result */` |
|   1563 |  303 | `	ph7_result_bool(pCtx,res);` |
|   1563 |  304 | `	return PH7_OK;` |
|      3 |  305 | `}` |
|      - |  306 | `/*` |
|      - |  307 | ` * bool is_resource($var)` |
|      - |  308 | ` *  Find out whether a variable is a resource.` |
|      - |  309 | ` * Parameters` |
|      - |  310 | ` *  $var: The variable being evaluated.` |
|      - |  311 | ` * Return` |
|      - |  312 | ` *  True if a resource. False otherwise.` |
|      - |  313 | ` */` |
|     62 |  314 | `static int PH7_builtin_is_resource(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  315 | `{` |
|     65 |  316 | `	int res = 0; /* Assume false by default */` |
|     65 |  317 | `	if( nArg > 0 ){` |
|     65 |  318 | `		res = ph7_value_is_resource(apArg[0]);` |
|     31 |  319 | `	}` |
|     65 |  320 | `	ph7_result_bool(pCtx,res);` |
|     65 |  321 | `	return PH7_OK;` |
|      3 |  322 | `}` |
|      - |  323 | `/*` |
|      - |  324 | ` * float floatval($var)` |
|      - |  325 | ` *  Get float value of a variable.` |
|      - |  326 | ` * Parameter` |
|      - |  327 | ` *  $var: The variable being processed.` |
|      - |  328 | ` * Return` |
|      - |  329 | ` *  the float value of a variable.` |
|      - |  330 | ` */` |
|      4 |  331 | `static int PH7_builtin_floatval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  332 | `{` |
|      5 |  333 | `	if( nArg < 1 ){` |
|      - |  334 | `		/* return 0.0 */` |
|    ! 0 |  335 | `		ph7_result_double(pCtx,0);` |
|    ! 0 |  336 | `	}else{` |
|      - |  337 | `		double dval;` |
|      - |  338 | `		/* Perform the cast */` |
|      5 |  339 | `		dval = ph7_value_to_double(apArg[0]);` |
|      5 |  340 | `		ph7_result_double(pCtx,dval);` |
|      - |  341 | `	}` |
|      5 |  342 | `	return PH7_OK;` |
|      1 |  343 | `}` |
|      - |  344 | `/*` |
|      - |  345 | ` * int intval($var)` |
|      - |  346 | ` *  Get integer value of a variable.` |
|      - |  347 | ` * Parameter` |
|      - |  348 | ` *  $var: The variable being processed.` |
|      - |  349 | ` * Return` |
|      - |  350 | ` *  the int value of a variable.` |
|      - |  351 | ` */` |
|     50 |  352 | `static int PH7_builtin_intval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  353 | `{` |
|     51 |  354 | `	if( nArg < 1 ){` |
|      - |  355 | `		/* return 0 */` |
|    ! 0 |  356 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  357 | `	}else{` |
|      - |  358 | `		sxi64 iVal;` |
|      - |  359 | `		/* Perform the cast */` |
|     51 |  360 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|     51 |  361 | `		ph7_result_int64(pCtx,iVal);` |
|      - |  362 | `	}` |
|     51 |  363 | `	return PH7_OK;` |
|      1 |  364 | `}` |
|      - |  365 | `/*` |
|      - |  366 | ` * string strval($var)` |
|      - |  367 | ` *  Get the string representation of a variable.` |
|      - |  368 | ` * Parameter` |
|      - |  369 | ` *  $var: The variable being processed.` |
|      - |  370 | ` * Return` |
|      - |  371 | ` *  the string value of a variable.` |
|      - |  372 | ` */` |
|      2 |  373 | `static int PH7_builtin_strval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  374 | `{` |
|      3 |  375 | `	if( nArg < 1 ){` |
|      - |  376 | `		/* return NULL */` |
|    ! 0 |  377 | `		ph7_result_null(pCtx);` |
|    ! 0 |  378 | `	}else{` |
|      - |  379 | `		const char *zVal;` |
|      3 |  380 | `		int iLen = 0; /* cc -O6 warning */` |
|      - |  381 | `		/* Perform the cast */` |
|      3 |  382 | `		zVal = ph7_value_to_string(apArg[0],&iLen);` |
|      3 |  383 | `		ph7_result_string(pCtx,zVal,iLen);` |
|      - |  384 | `	}` |
|      3 |  385 | `	return PH7_OK;` |
|      1 |  386 | `}` |
|      - |  387 | `/*` |
|      - |  388 | ` * bool boolval($var)` |
|      - |  389 | ` *  Get the boolean value of a variable.` |
|      - |  390 | ` * Parameter` |
|      - |  391 | ` *  $var: The variable being processed.` |
|      - |  392 | ` * Return` |
|      - |  393 | ` *  the bool value of a variable.` |
|      - |  394 | ` */` |
|     14 |  395 | `static int PH7_builtin_boolval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  396 | `{` |
|      - |  397 | `	int bVal;` |
|     15 |  398 | `	if( nArg != 1 ){` |
|    ! 0 |  399 | `		return PH7_VmThrowException(pCtx,` |
|      - |  400 | `			"ArgumentCountError",` |
|      - |  401 | `			"boolval() expects exactly 1 argument, %d given",` |
|    ! 0 |  402 | `			nArg` |
|      - |  403 | `			);` |
|      - |  404 | `	}` |
|      - |  405 | `	/* Perform the cast */` |
|     15 |  406 | `	bVal = ph7_value_to_bool(apArg[0]);` |
|     15 |  407 | `	ph7_result_bool(pCtx,bVal);` |
|     15 |  408 | `	return PH7_OK;` |
|      8 |  409 | `}` |
|      - |  410 | `/*` |
|      - |  411 | ` * bool empty($var)` |
|      - |  412 | ` *  Determine whether a variable is empty.` |
|      - |  413 | ` * Parameters` |
|      - |  414 | ` *   $var: The variable being checked.` |
|      - |  415 | ` * Return` |
|      - |  416 | ` *  0 if var has a non-empty and non-zero value.1 otherwise.` |
|      - |  417 | ` */` |
|  40754 |  418 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  419 | `{` |
|  40759 |  420 | `	int res = 1; /* Assume empty by default */` |
|  40759 |  421 | `	if( nArg > 0 ){` |
|  40757 |  422 | `		res = ph7_value_is_empty(apArg[0]);` |
|  20376 |  423 | `	}` |
|  40759 |  424 | `	ph7_result_bool(pCtx,res);` |
|  40759 |  425 | `	return PH7_OK;` |
|      - |  426 |  |
|      5 |  427 | `}` |
|      - |  428 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |  429 | `#define PH7_NEED_BUILTIN_REG 1` |
|      - |  430 | `#endif` |
|      - |  431 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - |  432 | `#define PH7_NEED_FMT_AND_INI 1` |
|      - |  433 | `#endif` |
|      - |  434 |  |
|      - |  435 | `/* Math functions moved to builtin_math.c */` |
|      - |  436 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - |  437 | `/*` |
|      - |  438 | ` * Section:` |
|      - |  439 | ` *    String handling Functions.` |
|      - |  440 | ` * Status:` |
|      - |  441 | ` *    Stable.` |
|      - |  442 | ` */` |
|      - |  443 | `/*` |
|      - |  444 | ` * string substr(string $string,int $start[, int $length ])` |
|      - |  445 | ` *  Return part of a string.` |
|      - |  446 | ` * Parameters` |
|      - |  447 | ` *  $string` |
|      - |  448 | ` *   The input string. Must be one character or longer.` |
|      - |  449 | ` * $start` |
|      - |  450 | ` *   If start is non-negative, the returned string will start at the start'th position` |
|      - |  451 | ` *   in string, counting from zero. For instance, in the string 'abcdef', the character` |
|      - |  452 | ` *   at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - |  453 | ` *   If start is negative, the returned string will start at the start'th character` |
|      - |  454 | ` *   from the end of string.` |
|      - |  455 | ` *   If string is less than or equal to start characters long, FALSE will be returned.` |
|      - |  456 | ` * $length` |
|      - |  457 | ` *   If length is given and is positive, the string returned will contain at most length` |
|      - |  458 | ` *   characters beginning from start (depending on the length of string).` |
|      - |  459 | ` *   If length is given and is negative, then that many characters will be omitted from` |
|      - |  460 | ` *   the end of string (after the start position has been calculated when a start is negative).` |
|      - |  461 | ` *   If start denotes the position of this truncation or beyond, false will be returned.` |
|      - |  462 | ` *   If length is given and is 0, FALSE or NULL an empty string will be returned.` |
|      - |  463 | ` *   If length is omitted, the substring starting from start until the end of the string` |
|      - |  464 | ` *   will be returned.` |
|      - |  465 | ` * Return` |
|      - |  466 | ` *  Returns the extracted part of string, or FALSE on failure or an empty string.` |
|      - |  467 | ` */` |
| 271512 |  468 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  469 | `{` |
|      - |  470 | `	const char *zSource;` |
|      - |  471 | `	int nSrcLen;` |
|      - |  472 | `	sxi64 iStart,iEnd;` |
| 271517 |  473 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"substr",1,"$string"); }` |
| 271517 |  474 | `	if( nArg < 2 ){` |
|      - |  475 | `		/* Arity is enforced at the call boundary; nothing sensible to return here. */` |
|    ! 0 |  476 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 |  477 | `		return PH7_OK;` |
|      - |  478 | `	}` |
|      - |  479 | `	/* Extract the target string */` |
| 271517 |  480 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|      - |  481 | `	/* Extract the offset */` |
|      - |  482 | `	{` |
| 271517 |  483 | `		sxi64 iTmp = 0;` |
| 271517 |  484 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"substr",2,"$offset","int",&iTmp);` |
| 271517 |  485 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |  486 | `			return rcArg;` |
|      - |  487 | `		}` |
| 271517 |  488 | `		iStart = iTmp;` |
|      - |  489 | `	}` |
|      - |  490 | `	/*` |
|      - |  491 | `	 * php 8 never answers substr() with FALSE — every out-of-range window simply` |
|      - |  492 | `	 * clamps to the empty string (substr("",0), substr("abc",5) and` |
|      - |  493 | `	 * substr("abc",1,-5) are all ""). PH7 returned FALSE for each of those, which` |
|      - |  494 | `	 * then flowed on as a bool into string context.` |
|      - |  495 | `	 *` |
|      - |  496 | `	 * A negative offset counts back from the end (clamped to 0); a negative length` |
|      - |  497 | `	 * leaves that many bytes off the end. Computed in sxi64 so an INT64 offset or` |
|      - |  498 | `	 * length cannot overflow the window arithmetic.` |
|      - |  499 | `	 */` |
| 271517 |  500 | `	if( iStart < 0 ){` |
|  33811 |  501 | `		iStart += nSrcLen;` |
|  33811 |  502 | `		if( iStart < 0 ){` |
|      5 |  503 | `			iStart = 0;` |
|      7 |  504 | `		}` |
| 254614 |  505 | `	}else if( iStart > nSrcLen ){` |
|      7 |  506 | `		iStart = nSrcLen;` |
|      3 |  507 | `	}` |
| 271517 |  508 | `	iEnd = nSrcLen;` |
| 271517 |  509 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
| 199989 |  510 | `		sxi64 iLen = 0;` |
| 199989 |  511 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[2],"substr",3,"$length","?int",&iLen);` |
| 199989 |  512 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |  513 | `			return rcArg;` |
|      - |  514 | `		}` |
| 199989 |  515 | `		if( iLen < 0 ){` |
|  33443 |  516 | `			iEnd = (sxi64)nSrcLen + iLen;` |
| 183270 |  517 | `		}else if( iLen > (sxi64)nSrcLen - iStart ){` |
|  19129 |  518 | `			iEnd = nSrcLen;` |
|   9567 |  519 | `		}else{` |
| 147427 |  520 | `			iEnd = iStart + iLen;` |
|      - |  521 | `		}` |
|  99992 |  522 | `	}` |
| 271517 |  523 | `	if( iEnd < iStart ){` |
|      3 |  524 | `		iEnd = iStart;` |
|      1 |  525 | `	}` |
| 271517 |  526 | `	ph7_result_string(pCtx,&zSource[iStart],(int)(iEnd - iStart));` |
| 271517 |  527 | `	return PH7_OK;` |
| 135761 |  528 | `}` |
|      - |  529 | `/*` |
|      - |  530 | ` * int substr_compare(string $main_str,string $str ,int $offset[,int $length[,bool $case_insensitivity = false ]])` |
|      - |  531 | ` *  Binary safe comparison of two strings from an offset, up to length characters.` |
|      - |  532 | ` * Parameters` |
|      - |  533 | ` *  $main_str` |
|      - |  534 | ` *  The main string being compared.` |
|      - |  535 | ` *  $str` |
|      - |  536 | ` *   The secondary string being compared.` |
|      - |  537 | ` * $offset` |
|      - |  538 | ` *  The start position for the comparison. If negative, it starts counting from` |
|      - |  539 | ` *  the end of the string.` |
|      - |  540 | ` * $length` |
|      - |  541 | ` *  The length of the comparison. The default value is the largest of the length` |
|      - |  542 | ` *  of the str compared to the length of main_str less the offset.` |
|      - |  543 | ` * $case_insensitivity` |
|      - |  544 | ` *  If case_insensitivity is TRUE, comparison is case insensitive.` |
|      - |  545 | ` * Return` |
|      - |  546 | ` *  Returns < 0 if main_str from position offset is less than str, > 0 if it is greater than` |
|      - |  547 | ` *  str, and 0 if they are equal. If offset is equal to or greater than the length of main_str` |
|      - |  548 | ` *  or length is set and is less than 1, substr_compare() prints a warning and returns FALSE.` |
|      - |  549 | ` */` |
|     20 |  550 | `static int PH7_builtin_substr_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  551 | `{` |
|      - |  552 | `	const char *zSource,*zSub;` |
|      - |  553 | `	int nSrcLen,nSubLen;` |
|      - |  554 | `	sxi64 iOfft,iLen,l1,l2,nCmp;` |
|     21 |  555 | `	int iCase = 0;` |
|      - |  556 | `	int rc;` |
|     21 |  557 | `	if( nArg < 3 ){` |
|    ! 0 |  558 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  559 | `		return PH7_OK;` |
|      - |  560 | `	}` |
|     21 |  561 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|     21 |  562 | `	zSub    = ph7_value_to_string(apArg[1],&nSubLen);` |
|      - |  563 | `	{` |
|     21 |  564 | `		sxi64 iTmp = 0;` |
|     21 |  565 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[2],"substr_compare",3,"$offset","int",&iTmp);` |
|     21 |  566 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |  567 | `			return rcArg;` |
|      - |  568 | `		}` |
|     21 |  569 | `		iOfft = iTmp;` |
|      - |  570 | `	}` |
|     21 |  571 | `	if( iOfft < 0 ){` |
|      5 |  572 | `		iOfft += nSrcLen;` |
|      5 |  573 | `		if( iOfft < 0 ){` |
|      3 |  574 | `			iOfft = 0;` |
|      1 |  575 | `		}` |
|      2 |  576 | `	}` |
|     21 |  577 | `	if( iOfft > nSrcLen ){` |
|      - |  578 | `		/* php rejects an offset past the end of the haystack outright */` |
|    ! 0 |  579 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  580 | `			"substr_compare(): Argument #3 ($offset) must be contained in argument #1 ($haystack)");` |
|      - |  581 | `	}` |
|      - |  582 | `	/* A NULL/absent length compares as far as the longer of the two operands reaches */` |
|     21 |  583 | `	iLen = (sxi64)nSrcLen - iOfft;` |
|     21 |  584 | `	if( iLen < nSubLen ){` |
|      5 |  585 | `		iLen = nSubLen;` |
|      2 |  586 | `	}` |
|     21 |  587 | `	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){` |
|     11 |  588 | `		sxi64 iTmp = 0;` |
|     11 |  589 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[3],"substr_compare",4,"$length","?int",&iTmp);` |
|     11 |  590 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |  591 | `			return rcArg;` |
|      - |  592 | `		}` |
|     11 |  593 | `		if( iTmp < 0 ){` |
|      3 |  594 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  595 | `				"substr_compare(): Argument #4 ($length) must be greater than or equal to 0");` |
|      - |  596 | `		}` |
|      9 |  597 | `		iLen = iTmp;` |
|      4 |  598 | `	}` |
|     19 |  599 | `	if( nArg > 4 ){` |
|      5 |  600 | `		iCase = ph7_value_to_bool(apArg[4]);` |
|      2 |  601 | `	}` |
|      - |  602 | `	/* Each side contributes at most what it actually has left */` |
|     19 |  603 | `	l1 = (sxi64)nSrcLen - iOfft;` |
|     19 |  604 | `	if( l1 > iLen ){ l1 = iLen; }` |
|     19 |  605 | `	l2 = nSubLen;` |
|     19 |  606 | `	if( l2 > iLen ){ l2 = iLen; }` |
|     19 |  607 | `	nCmp = (l1 < l2) ? l1 : l2;` |
|     19 |  608 | `	if( iCase ){` |
|      3 |  609 | `		rc = SyStrnicmp(&zSource[iOfft],zSub,(sxu32)nCmp);` |
|      2 |  610 | `	}else{` |
|     17 |  611 | `		rc = SyStrncmp(&zSource[iOfft],zSub,(sxu32)nCmp);` |
|      - |  612 | `	}` |
|     19 |  613 | `	if( rc == 0 ){` |
|      - |  614 | `		/* Prefixes equal: php falls back to a THREE-WAY compare of the lengths, so this` |
|      - |  615 | `		 * arm is normalized to -1/0/1 (substr_compare("abc","",0) is 1, not 3). */` |
|      9 |  616 | `		rc = (l1 == l2) ? 0 : (l1 < l2 ? -1 : 1);` |
|      4 |  617 | `	}` |
|      - |  618 | `	/* ...but when the prefixes differ php returns the RAW byte difference, not its sign:` |
|      - |  619 | `	 * substr_compare("abc","def",1,10) is -2 ('b' - 'd'), which is what SyMemcmp gives. */` |
|     19 |  620 | `	ph7_result_int(pCtx,rc);` |
|     19 |  621 | `	return PH7_OK;` |
|     11 |  622 | `}` |
|      - |  623 | `/*` |
|      - |  624 | ` * int substr_count(string $haystack,string $needle[,int $offset = 0 [,int $length ]])` |
|      - |  625 | ` *  Count the number of substring occurrences.` |
|      - |  626 | ` * Parameters` |
|      - |  627 | ` * $haystack` |
|      - |  628 | ` *   The string to search in` |
|      - |  629 | ` * $needle` |
|      - |  630 | ` *   The substring to search for` |
|      - |  631 | ` * $offset` |
|      - |  632 | ` *  The offset where to start counting` |
|      - |  633 | ` * $length (NOT USED)` |
|      - |  634 | ` *  The maximum length after the specified offset to search for the substring.` |
|      - |  635 | ` *  It outputs a warning if the offset plus the length is greater than the haystack length.` |
|      - |  636 | ` * Return` |
|      - |  637 | ` *  Toral number of substring occurrences.` |
|      - |  638 | ` */` |
|     26 |  639 | `static int PH7_builtin_substr_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  640 | `{` |
|      - |  641 | `	const char *zText,*zPattern,*zEnd;` |
|      - |  642 | `	int nTextlen,nPatlen;` |
|     27 |  643 | `	int iCount = 0;` |
|      - |  644 | `	sxu32 nOfft;` |
|      - |  645 | `	sxi32 rc;` |
|     27 |  646 | `	if( nArg < 2 ){` |
|      - |  647 | `		/* Missing arguments */` |
|    ! 0 |  648 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  649 | `		return PH7_OK;` |
|      - |  650 | `	}` |
|      - |  651 | `	/* Point to the haystack */` |
|     27 |  652 | `	zText = ph7_value_to_string(apArg[0],&nTextlen);` |
|      - |  653 | `	/* Point to the neddle */` |
|     27 |  654 | `	zPattern = ph7_value_to_string(apArg[1],&nPatlen);` |
|     27 |  655 | `	if( nPatlen < 1 ){` |
|      - |  656 | `		/* Empty needle: PHP 8 throws a catchable ValueError. */` |
|      3 |  657 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  658 | `			"substr_count(): Argument #2 ($needle) must not be empty");` |
|      - |  659 | `	}` |
|      - |  660 | `	/* Apply the optional $offset/$length window before searching. PHP 8 validates` |
|      - |  661 | `	 * both against the haystack (a negative value counts from the end) and throws a` |
|      - |  662 | `	 * catchable ValueError when the result falls outside it — this happens before the` |
|      - |  663 | `	 * needle-fits check, so it fires even when the needle is longer than the haystack. */` |
|     25 |  664 | `	if( nArg > 2 ){` |
|     19 |  665 | `		ph7_int64 iOfft = ph7_value_to_int64(apArg[2]);` |
|     19 |  666 | `		if( iOfft < 0 ){` |
|      5 |  667 | `			iOfft += nTextlen;` |
|      2 |  668 | `		}` |
|     19 |  669 | `		if( iOfft < 0 \|\| iOfft > nTextlen ){` |
|      3 |  670 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  671 | `				"substr_count(): Argument #3 ($offset) must be contained in argument #1 ($haystack)");` |
|      - |  672 | `		}` |
|      - |  673 | `		/* Point to the desired offset and shrink the remaining region */` |
|     17 |  674 | `		zText = &zText[iOfft];` |
|     17 |  675 | `		nTextlen -= (int)iOfft;` |
|      8 |  676 | `	}` |
|     23 |  677 | `	if( nArg > 3 ){` |
|     15 |  678 | `		ph7_int64 nLen = ph7_value_to_int64(apArg[3]);` |
|     15 |  679 | `		if( nLen < 0 ){` |
|      - |  680 | `			/* Negative length is relative to the end of the (offset) haystack */` |
|      5 |  681 | `			nLen += nTextlen;` |
|      2 |  682 | `		}` |
|     15 |  683 | `		if( nLen < 0 \|\| nLen > nTextlen ){` |
|      5 |  684 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  685 | `				"substr_count(): Argument #4 ($length) must be contained in argument #1 ($haystack)");` |
|      - |  686 | `		}` |
|     11 |  687 | `		nTextlen = (int)nLen;` |
|      5 |  688 | `	}` |
|     19 |  689 | `	if( nTextlen < 1 \|\| nPatlen > nTextlen ){` |
|      - |  690 | `		/* The windowed haystack can't contain the needle: zero matches */` |
|      3 |  691 | `		ph7_result_int(pCtx,0);` |
|      3 |  692 | `		return PH7_OK;` |
|      - |  693 | `	}` |
|      - |  694 | `	/* Point to the end of the windowed haystack */` |
|     17 |  695 | `	zEnd = &zText[nTextlen];` |
|      - |  696 | `	/* Perform the search */` |
|     17 |  697 | `	for(;;){` |
|     35 |  698 | `		rc = SyBlobSearch((const void *)zText,(sxu32)(zEnd-zText),(const void *)zPattern,nPatlen,&nOfft);` |
|     35 |  699 | `		if( rc != SXRET_OK ){` |
|      - |  700 | `			/* Pattern not found,break immediately */` |
|     13 |  701 | `			break;` |
|      - |  702 | `		}` |
|      - |  703 | `		/* Increment counter and update the offset */` |
|     23 |  704 | `		iCount++;` |
|     23 |  705 | `		zText += nOfft + nPatlen;` |
|     23 |  706 | `		if( zText >= zEnd ){` |
|      5 |  707 | `			break;` |
|      - |  708 | `		}` |
|      1 |  709 | `	}` |
|      - |  710 | `	/* Pattern count */` |
|     17 |  711 | `	ph7_result_int(pCtx,iCount);` |
|     17 |  712 | `	return PH7_OK;` |
|     14 |  713 | `}` |
|      - |  714 | `/* Forward declarations: defined with the trim/addcslashes and str_contains` |
|      - |  715 | ` * families below. */` |
|      - |  716 | `static void PH7_BuildCharMask(ph7_context *pCtx,const char *zList,int nLen,char aMask[256]);` |
|      - |  717 | `/*` |
|      - |  718 | ` * php 8.1 null-to-non-nullable ZPP deprecation, notice-only form for the` |
|      - |  719 | ` * legacy string builtins that still coerce null to "" themselves: emit` |
|      - |  720 | ``  * `f(): Passing null to parameter #N ($name) of type string is deprecated` `` |
|      - |  721 | ` * when the arg is an actual null, leaving the resolution unchanged.` |
|      - |  722 | ` */` |
| 399336 |  723 | `static void StrNullArgNotice(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNum,const char *zParamName)` |
|      5 |  724 | `{` |
| 399341 |  725 | `	if( ph7_value_is_null(pArg) ){` |
|     22 |  726 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|      - |  727 | `			"%s(): Passing null to parameter #%d (%s) of type string is deprecated",` |
|      7 |  728 | `			zFunc,iArgNum,zParamName);` |
|      7 |  729 | `	}` |
| 399341 |  730 | `}` |
|      - |  731 | `static sxi32 StrPredicateResolveArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,` |
|      - |  732 | `	int iArgNum,const char *zParamName,const char *zTypeStr,const char *zNullMsg,` |
|      - |  733 | `	ph7_value *pTmp,const char **pzOut,int *pnOut);` |
|      - |  734 | `/*` |
|      - |  735 | ` * Validate and resolve an int-typed builtin parameter with php-8 ZPP weak-mode` |
|      - |  736 | ` * semantics: ints and bools pass through; null emits the 8.1 deprecation and` |
|      - |  737 | ` * resolves to 0; floats and float-strings convert, with the implicit-conversion` |
|      - |  738 | ` * E_DEPRECATED when lossy and a TypeError when NAN/INF/out of int range;` |
|      - |  739 | ` * integral numeric strings convert exactly; everything else (arrays, resources,` |
|      - |  740 | ` * objects, non-numeric strings) is a TypeError naming zTypeStr (e.g. "int",` |
|      - |  741 | ` * "array\|int"). Returns PH7_OK with *pOut set, or the throw status.` |
|      - |  742 | ` */` |
|      - |  743 | `/*` |
|      - |  744 | ` * Normalize a substr_replace() offset/length pair against a string of nStrLen` |
|      - |  745 | ` * bytes, exactly like PHP: a negative offset counts from the end (clamped to 0),` |
|      - |  746 | ` * an offset past the end clamps to the end; a negative length leaves that many` |
|      - |  747 | ` * bytes off the end of the remaining region (clamped to 0), and the length is` |
|      - |  748 | ` * finally clamped to the remaining region. Written without f+l additions so an` |
|      - |  749 | ` * INT64_MAX length cannot overflow.` |
|      - |  750 | ` */` |
|     60 |  751 | `static void SubstrReplaceWindow(sxi64 *pF,sxi64 *pL,int nStrLen)` |
|      1 |  752 | `{` |
|     61 |  753 | `	sxi64 f = *pF,l = *pL;` |
|     61 |  754 | `	if( f < 0 ){` |
|      9 |  755 | `		f += nStrLen;` |
|      9 |  756 | `		if( f < 0 ){` |
|      5 |  757 | `			f = 0;` |
|      3 |  758 | `		}` |
|     57 |  759 | `	}else if( f > nStrLen ){` |
|      5 |  760 | `		f = nStrLen;` |
|      2 |  761 | `	}` |
|     61 |  762 | `	if( l < 0 ){` |
|      7 |  763 | `		l += nStrLen - f;` |
|      7 |  764 | `		if( l < 0 ){` |
|      5 |  765 | `			l = 0;` |
|      2 |  766 | `		}` |
|      3 |  767 | `	}` |
|     61 |  768 | `	if( l > nStrLen - f ){` |
|     25 |  769 | `		l = nStrLen - f;` |
|     12 |  770 | `	}` |
|     61 |  771 | `	*pF = f;` |
|     61 |  772 | `	*pL = l;` |
|     61 |  773 | `}` |
|      - |  774 | `/* A replacement string collected out of substr_replace()'s $replace array.` |
|      - |  775 | ` * The bytes live in a shared pool blob (walker values are transient), so the` |
|      - |  776 | ` * item stores pool offsets, mirroring the strtr_entry technique. */` |
|      - |  777 | `typedef struct substr_repl_item substr_repl_item;` |
|      - |  778 | `struct substr_repl_item` |
|      - |  779 | `{` |
|      - |  780 | `	sxu32 nOfft; /* Offset of the string inside the pool */` |
|      - |  781 | `	sxu32 nLen;  /* Length of the string */` |
|      - |  782 | `};` |
|      - |  783 | `typedef struct substr_replace_collect substr_replace_collect;` |
|      - |  784 | `struct substr_replace_collect` |
|      - |  785 | `{` |
|      - |  786 | `	SyBlob *pPool;  /* Byte pool for string items (string walker only) */` |
|      - |  787 | `	SySet *pSet;    /* substr_repl_item set (string) or sxi64 set (int) */` |
|      - |  788 | `	sxi32 rc;       /* SXRET_OK or SXERR_MEM on collector failure */` |
|      - |  789 | `};` |
|      - |  790 | `/* ph7_array_walk() callback: append one $replace element to the pool. */` |
|      6 |  791 | `static int SubstrReplaceStrWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 |  792 | `{` |
|      7 |  793 | `	substr_replace_collect *pCol = (substr_replace_collect *)pUserData;` |
|      - |  794 | `	substr_repl_item sItem;` |
|      - |  795 | `	const char *zStr;` |
|      - |  796 | `	int nLen;` |
|      3 |  797 | `	SXUNUSED(pKey);` |
|      7 |  798 | `	zStr = ph7_value_to_string(pData,&nLen);` |
|      7 |  799 | `	sItem.nOfft = SyBlobLength(pCol->pPool);` |
|      7 |  800 | `	sItem.nLen = (sxu32)nLen;` |
|      7 |  801 | `	if( nLen > 0 && SXRET_OK != SyBlobAppend(pCol->pPool,(const void *)zStr,(sxu32)nLen) ){` |
|    ! 0 |  802 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 |  803 | `		return SXERR_ABORT;` |
|      - |  804 | `	}` |
|      7 |  805 | `	if( SXRET_OK != SySetPut(pCol->pSet,(const void *)&sItem) ){` |
|    ! 0 |  806 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 |  807 | `		return SXERR_ABORT;` |
|      - |  808 | `	}` |
|      7 |  809 | `	return PH7_OK;` |
|      4 |  810 | `}` |
|      - |  811 | `/* ph7_array_walk() callback: collect one $offset/$length element as an int. */` |
|     12 |  812 | `static int SubstrReplaceIntWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 |  813 | `{` |
|     13 |  814 | `	substr_replace_collect *pCol = (substr_replace_collect *)pUserData;` |
|     13 |  815 | `	sxi64 iVal = ph7_value_to_int64(pData);` |
|      6 |  816 | `	SXUNUSED(pKey);` |
|     13 |  817 | `	if( SXRET_OK != SySetPut(pCol->pSet,(const void *)&iVal) ){` |
|    ! 0 |  818 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 |  819 | `		return SXERR_ABORT;` |
|      - |  820 | `	}` |
|     13 |  821 | `	return PH7_OK;` |
|      7 |  822 | `}` |
|      - |  823 | `/* Per-element state while walking substr_replace()'s array $string. */` |
|      - |  824 | `typedef struct substr_replace_ctx substr_replace_ctx;` |
|      - |  825 | `struct substr_replace_ctx` |
|      - |  826 | `{` |
|      - |  827 | `	ph7_value *pResult;   /* Result array (keys preserved) */` |
|      - |  828 | `	ph7_value *pScratch;  /* Reusable string value for each element */` |
|      - |  829 | `	SyBlob *pReplPool;    /* Pool behind aRepl items */` |
|      - |  830 | `	SySet *pRepl;         /* substr_repl_item set or NULL when $replace is scalar */` |
|      - |  831 | `	SySet *pFrom;         /* sxi64 set or NULL when $offset is scalar */` |
|      - |  832 | `	SySet *pLen;          /* sxi64 set or NULL when $length is scalar/absent */` |
|      - |  833 | `	sxu32 iReplCur;       /* Next-position cursors into the three sets */` |
|      - |  834 | `	sxu32 iFromCur;` |
|      - |  835 | `	sxu32 iLenCur;` |
|      - |  836 | `	const char *zRepl;    /* Scalar $replace */` |
|      - |  837 | `	int nRepl;` |
|      - |  838 | `	sxi64 iFrom;          /* Scalar $offset */` |
|      - |  839 | `	sxi64 iLen;           /* Scalar $length */` |
|      - |  840 | `	int bLenGiven;        /* FALSE: $length absent/null -> element length */` |
|      - |  841 | `	sxi32 rc;             /* SXRET_OK or SXERR_MEM */` |
|      - |  842 | `};` |
|      - |  843 | `/*` |
|      - |  844 | ` * ph7_array_walk() callback over the array $string: replace the window of one` |
|      - |  845 | ` * element and insert the result under the element's original key. Array-form` |
|      - |  846 | ` * $replace/$offset/$length are consumed positionally; when a set runs out PHP` |
|      - |  847 | ` * falls back to ""/0/element-length respectively.` |
|      - |  848 | ` */` |
|     24 |  849 | `static int SubstrReplaceElemWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 |  850 | `{` |
|     25 |  851 | `	substr_replace_ctx *pRep = (substr_replace_ctx *)pUserData;` |
|      - |  852 | `	const char *zStr,*zRepl;` |
|      - |  853 | `	sxi64 f,l;` |
|      - |  854 | `	int nLen,nRepl;` |
|     25 |  855 | `	zStr = ph7_value_to_string(pData,&nLen);` |
|      - |  856 | `	/* Positional $replace element ("" when exhausted) */` |
|     25 |  857 | `	if( pRep->pRepl ){` |
|     11 |  858 | `		if( pRep->iReplCur < SySetUsed(pRep->pRepl) ){` |
|      7 |  859 | `			substr_repl_item *pItem = (substr_repl_item *)SySetAt(pRep->pRepl,pRep->iReplCur++);` |
|      7 |  860 | `			zRepl = (const char *)SyBlobDataAt(pRep->pReplPool,pItem->nOfft);` |
|      7 |  861 | `			nRepl = (int)pItem->nLen;` |
|      4 |  862 | `		}else{` |
|      5 |  863 | `			zRepl = "";` |
|      5 |  864 | `			nRepl = 0;` |
|      - |  865 | `		}` |
|      6 |  866 | `	}else{` |
|     15 |  867 | `		zRepl = pRep->zRepl;` |
|     15 |  868 | `		nRepl = pRep->nRepl;` |
|      - |  869 | `	}` |
|      - |  870 | `	/* Positional $offset element (0 when exhausted) */` |
|     25 |  871 | `	if( pRep->pFrom ){` |
|     13 |  872 | `		sxi64 *pVal = 0;` |
|     13 |  873 | `		if( pRep->iFromCur < SySetUsed(pRep->pFrom) ){` |
|      9 |  874 | `			pVal = (sxi64 *)SySetAt(pRep->pFrom,pRep->iFromCur++);` |
|      4 |  875 | `		}` |
|     13 |  876 | `		f = pVal ? *pVal : 0;` |
|      7 |  877 | `	}else{` |
|     13 |  878 | `		f = pRep->iFrom;` |
|      - |  879 | `	}` |
|      - |  880 | `	/* Positional $length element (element length when exhausted) */` |
|     25 |  881 | `	if( pRep->pLen ){` |
|      7 |  882 | `		sxi64 *pVal = 0;` |
|      7 |  883 | `		if( pRep->iLenCur < SySetUsed(pRep->pLen) ){` |
|      5 |  884 | `			pVal = (sxi64 *)SySetAt(pRep->pLen,pRep->iLenCur++);` |
|      2 |  885 | `		}` |
|      7 |  886 | `		l = pVal ? *pVal : nLen;` |
|      4 |  887 | `	}else{` |
|     19 |  888 | `		l = pRep->bLenGiven ? pRep->iLen : nLen;` |
|      - |  889 | `	}` |
|     25 |  890 | `	SubstrReplaceWindow(&f,&l,nLen);` |
|      - |  891 | `	/* Assemble prefix + replacement + suffix in the scratch value */` |
|     25 |  892 | `	ph7_value_reset_string_cursor(pRep->pScratch);` |
|     24 |  893 | `	if( (f > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,zStr,(int)f))` |
|     24 |  894 | `	 \|\| (nRepl > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,zRepl,nRepl))` |
|     40 |  895 | `	 \|\| (nLen - (int)(f+l) > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,&zStr[f+l],nLen - (int)(f+l))) ){` |
|     30 |  896 | `		pRep->rc = SXERR_MEM;` |
|     30 |  897 | `		return SXERR_ABORT;` |
|      - |  898 | `	}` |
|     25 |  899 | `	if( SXRET_OK != ph7_array_add_elem(pRep->pResult,pKey,pRep->pScratch) ){` |
|    ! 0 |  900 | `		pRep->rc = SXERR_MEM;` |
|    ! 0 |  901 | `		return SXERR_ABORT;` |
|      - |  902 | `	}` |
|     25 |  903 | `	return PH7_OK;` |
|     43 |  904 | `}` |
|      - |  905 | `/*` |
|      - |  906 | ` * mixed substr_replace(array\|string $string,array\|string $replace,array\|int $offset[,array\|int\|null $length = null])` |
|      - |  907 | ` *  Replace text within a portion of a string.` |
|      - |  908 | ` * Parameters` |
|      - |  909 | ` *  $string` |
|      - |  910 | ` *   The input string or an array of strings (each element is processed with` |
|      - |  911 | ` *   its own positional replace/offset/length when those are arrays too).` |
|      - |  912 | ` *  $replace` |
|      - |  913 | ` *   The replacement string. When $string is scalar and $replace is an array,` |
|      - |  914 | ` *   only its first element is used (PHP quirk).` |
|      - |  915 | ` *  $offset` |
|      - |  916 | ` *   Window start; negative counts from the end of the string.` |
|      - |  917 | ` *  $length` |
|      - |  918 | ` *   Window length; negative leaves that many bytes at the end; null/absent` |
|      - |  919 | ` *   means "to the end of the string".` |
|      - |  920 | ` * Return` |
|      - |  921 | ` *  The processed string, or an array of processed strings (keys preserved).` |
|      - |  922 | ` * Errors` |
|      - |  923 | ` *  ArgumentCountError on fewer than 3 arguments; TypeError when an array` |
|      - |  924 | ` *  $offset/$length is combined with a scalar $string.` |
|      - |  925 | ` */` |
|     58 |  926 | `static int PH7_builtin_substr_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  927 | `{` |
|      - |  928 | `	ph7_value sStrTmp,sReplTmp;` |
|     59 |  929 | `	const char *zStr = 0,*zRepl = 0;` |
|     59 |  930 | `	int nLen = 0,nRepl = 0;` |
|      - |  931 | `	int bLenGiven;` |
|     59 |  932 | `	sxi64 f = 0,l = 0;` |
|      - |  933 | `	sxi32 rc;` |
|     59 |  934 | `	if( nArg < 3 ){` |
|    ! 0 |  935 | `		return PH7_VmThrowException(pCtx,` |
|      - |  936 | `			"ArgumentCountError",` |
|      - |  937 | `			"substr_replace() expects at least 3 arguments, %d given",` |
|    ! 0 |  938 | `			nArg` |
|      - |  939 | `			);` |
|      - |  940 | `	}` |
|      - |  941 | `	/* $length counts as given unless absent or null (php: ?null semantics) */` |
|     59 |  942 | `	bLenGiven = (nArg > 3 && !ph7_value_is_null(apArg[3]));` |
|      - |  943 | `	/* php ZPP validates all four args, in order, before the body runs: the` |
|      - |  944 | `	 * non-array forms resolve here (null deprecation, __toString objects,` |
|      - |  945 | `	 * numeric strings), arrays pass through to the per-mode handling. */` |
|     59 |  946 | `	PH7_MemObjInit(pCtx->pVm,&sStrTmp);` |
|     59 |  947 | `	PH7_MemObjInit(pCtx->pVm,&sReplTmp);` |
|     59 |  948 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     45 |  949 | `		rc = StrPredicateResolveArg(pCtx,apArg[0],"substr_replace",1,"$string","array\|string",` |
|      - |  950 | `			"substr_replace(): Passing null to parameter #1 ($string) "` |
|      - |  951 | `			"of type array\|string is deprecated",` |
|      - |  952 | `			&sStrTmp,&zStr,&nLen);` |
|     45 |  953 | `		if( rc != PH7_OK ) goto out;` |
|     22 |  954 | `	}` |
|     59 |  955 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|     51 |  956 | `		rc = StrPredicateResolveArg(pCtx,apArg[1],"substr_replace",2,"$replace","array\|string",` |
|      - |  957 | `			"substr_replace(): Passing null to parameter #2 ($replace) "` |
|      - |  958 | `			"of type array\|string is deprecated",` |
|      - |  959 | `			&sReplTmp,&zRepl,&nRepl);` |
|     51 |  960 | `		if( rc != PH7_OK ) goto out;` |
|     25 |  961 | `	}` |
|     59 |  962 | `	if( !ph7_value_is_array(apArg[2]) ){` |
|     51 |  963 | `		rc = PH7_IntArgResolve(pCtx,apArg[2],"substr_replace",3,"$offset","array\|int",&f);` |
|     51 |  964 | `		if( rc != PH7_OK ) goto out;` |
|     24 |  965 | `	}` |
|     57 |  966 | `	if( bLenGiven && !ph7_value_is_array(apArg[3]) ){` |
|     31 |  967 | `		rc = PH7_IntArgResolve(pCtx,apArg[3],"substr_replace",4,"$length","array\|int\|null",&l);` |
|     31 |  968 | `		if( rc != PH7_OK ) goto out;` |
|     14 |  969 | `	}` |
|     55 |  970 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - |  971 | `		/* Array form: process each element, preserving keys */` |
|      - |  972 | `		substr_replace_ctx sRep;` |
|      - |  973 | `		substr_replace_collect sCol;` |
|      - |  974 | `		SyBlob sReplPool;` |
|      - |  975 | `		SySet sRepl,sFrom,sLen;` |
|      - |  976 | `		ph7_value *pResult,*pScratch;` |
|     15 |  977 | `		sxi32 rcWalk = SXRET_OK;` |
|     15 |  978 | `		SyBlobInit(&sReplPool,&pCtx->pVm->sAllocator);` |
|     15 |  979 | `		SySetInit(&sRepl,&pCtx->pVm->sAllocator,sizeof(substr_repl_item));` |
|     15 |  980 | `		SySetInit(&sFrom,&pCtx->pVm->sAllocator,sizeof(sxi64));` |
|     15 |  981 | `		SySetInit(&sLen,&pCtx->pVm->sAllocator,sizeof(sxi64));` |
|     15 |  982 | `		SyZero(&sRep,sizeof(substr_replace_ctx));` |
|     15 |  983 | `		sRep.bLenGiven = bLenGiven;` |
|     15 |  984 | `		sCol.rc = SXRET_OK;` |
|      - |  985 | `		/* Collect array-form $replace/$offset/$length positionally; the` |
|      - |  986 | `		 * scalar forms were already resolved above. */` |
|     15 |  987 | `		if( ph7_value_is_array(apArg[1]) ){` |
|      5 |  988 | `			sCol.pPool = &sReplPool;` |
|      5 |  989 | `			sCol.pSet = &sRepl;` |
|      5 |  990 | `			ph7_array_walk(apArg[1],SubstrReplaceStrWalker,&sCol);` |
|      5 |  991 | `			sRep.pRepl = &sRepl;` |
|      5 |  992 | `			sRep.pReplPool = &sReplPool;` |
|      3 |  993 | `		}else{` |
|     11 |  994 | `			sRep.zRepl = zRepl;` |
|     11 |  995 | `			sRep.nRepl = nRepl;` |
|      - |  996 | `		}` |
|     15 |  997 | `		if( sCol.rc == SXRET_OK && ph7_value_is_array(apArg[2]) ){` |
|      7 |  998 | `			sCol.pSet = &sFrom;` |
|      7 |  999 | `			ph7_array_walk(apArg[2],SubstrReplaceIntWalker,&sCol);` |
|      7 | 1000 | `			sRep.pFrom = &sFrom;` |
|      4 | 1001 | `		}else{` |
|      9 | 1002 | `			sRep.iFrom = f;` |
|      - | 1003 | `		}` |
|     15 | 1004 | `		if( sCol.rc == SXRET_OK && bLenGiven ){` |
|      9 | 1005 | `			if( ph7_value_is_array(apArg[3]) ){` |
|      5 | 1006 | `				sCol.pSet = &sLen;` |
|      5 | 1007 | `				ph7_array_walk(apArg[3],SubstrReplaceIntWalker,&sCol);` |
|      5 | 1008 | `				sRep.pLen = &sLen;` |
|      3 | 1009 | `			}else{` |
|      5 | 1010 | `				sRep.iLen = l;` |
|      - | 1011 | `			}` |
|      4 | 1012 | `		}` |
|     15 | 1013 | `		pResult = ph7_context_new_array(pCtx);` |
|     15 | 1014 | `		pScratch = ph7_context_new_scalar(pCtx);` |
|     15 | 1015 | `		if( sCol.rc != SXRET_OK \|\| pResult == 0 \|\| pScratch == 0 ){` |
|    ! 0 | 1016 | `			rcWalk = SXERR_MEM;` |
|    ! 0 | 1017 | `		}else{` |
|     15 | 1018 | `			sRep.pResult = pResult;` |
|     15 | 1019 | `			sRep.pScratch = pScratch;` |
|     15 | 1020 | `			ph7_value_string(pScratch,"",0); /* Force string representation */` |
|     15 | 1021 | `			ph7_array_walk(apArg[0],SubstrReplaceElemWalker,&sRep);` |
|     15 | 1022 | `			rcWalk = sRep.rc;` |
|      - | 1023 | `		}` |
|     15 | 1024 | `		SyBlobRelease(&sReplPool);` |
|     15 | 1025 | `		SySetRelease(&sRepl);` |
|     15 | 1026 | `		SySetRelease(&sFrom);` |
|     15 | 1027 | `		SySetRelease(&sLen);` |
|     15 | 1028 | `		if( rcWalk != SXRET_OK ){` |
|    ! 0 | 1029 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1030 | `			goto out;` |
|      - | 1031 | `		}` |
|     15 | 1032 | `		ph7_result_value(pCtx,pResult);` |
|     15 | 1033 | `		rc = PH7_OK;` |
|     15 | 1034 | `		goto out;` |
|      - | 1035 | `	}` |
|      - | 1036 | `	/* Scalar form: array $offset/$length are a TypeError, array $replace` |
|      - | 1037 | `	 * degrades to its first element (php quirk). */` |
|     41 | 1038 | `	if( ph7_value_is_array(apArg[2]) ){` |
|      3 | 1039 | `		rc = PH7_VmThrowException(pCtx,` |
|      - | 1040 | `			"TypeError",` |
|      - | 1041 | `			"substr_replace(): Argument #3 ($offset) cannot be an array when working on a single string"` |
|      - | 1042 | `			);` |
|      3 | 1043 | `		goto out;` |
|      - | 1044 | `	}` |
|     39 | 1045 | `	if( bLenGiven && ph7_value_is_array(apArg[3]) ){` |
|      3 | 1046 | `		rc = PH7_VmThrowException(pCtx,` |
|      - | 1047 | `			"TypeError",` |
|      - | 1048 | `			"substr_replace(): Argument #4 ($length) cannot be an array when working on a single string"` |
|      - | 1049 | `			);` |
|      3 | 1050 | `		goto out;` |
|      - | 1051 | `	}` |
|     37 | 1052 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 1053 | `		/* First element of the replace array, or "" when empty */` |
|      5 | 1054 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      5 | 1055 | `		zRepl = "";` |
|      5 | 1056 | `		nRepl = 0;` |
|      5 | 1057 | `		if( pMap->pFirst ){` |
|      3 | 1058 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pMap->pFirst->nValIdx);` |
|      3 | 1059 | `			if( pVal ){` |
|      3 | 1060 | `				zRepl = ph7_value_to_string(pVal,&nRepl);` |
|      1 | 1061 | `			}` |
|      1 | 1062 | `		}` |
|      2 | 1063 | `	}` |
|     37 | 1064 | `	if( !bLenGiven ){` |
|     15 | 1065 | `		l = nLen;` |
|      7 | 1066 | `	}` |
|     37 | 1067 | `	SubstrReplaceWindow(&f,&l,nLen);` |
|      - | 1068 | `	/* Assemble prefix + replacement + suffix straight into the call result` |
|      - | 1069 | `	 * (ph7_result_string appends), no scratch buffer needed. */` |
|     37 | 1070 | `	rc = SXRET_OK;` |
|     37 | 1071 | `	if( f > 0 ){` |
|     29 | 1072 | `		rc = ph7_result_string(pCtx,zStr,(int)f);` |
|     14 | 1073 | `	}` |
|     37 | 1074 | `	if( rc == SXRET_OK && nRepl > 0 ){` |
|     33 | 1075 | `		rc = ph7_result_string(pCtx,zRepl,nRepl);` |
|     16 | 1076 | `	}` |
|     37 | 1077 | `	if( rc == SXRET_OK && nLen - (int)(f+l) > 0 ){` |
|     17 | 1078 | `		rc = ph7_result_string(pCtx,&zStr[f+l],nLen - (int)(f+l));` |
|      8 | 1079 | `	}` |
|     37 | 1080 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 1081 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1082 | `		goto out;` |
|      - | 1083 | `	}` |
|      - | 1084 | `	/* Force a string result even when all three segments are empty */` |
|     37 | 1085 | `	rc = ph7_result_string(pCtx,"",0);` |
|     37 | 1086 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 1087 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1088 | `		goto out;` |
|      - | 1089 | `	}` |
|     37 | 1090 | `	rc = PH7_OK;` |
|     29 | 1091 | `out:` |
|     59 | 1092 | `	PH7_MemObjRelease(&sStrTmp);` |
|     59 | 1093 | `	PH7_MemObjRelease(&sReplTmp);` |
|     59 | 1094 | `	return rc;` |
|     30 | 1095 | `}` |
|      - | 1096 | `/*` |
|      - | 1097 | ` * int levenshtein(string $string1,string $string2[,int $insertion_cost = 1[,int $replacement_cost = 1[,int $deletion_cost = 1]]])` |
|      - | 1098 | ` *  Calculate the Levenshtein distance between two strings, byte per byte` |
|      - | 1099 | ` *  (case-sensitive), with optional per-operation costs. Mirrors PHP's` |
|      - | 1100 | ` *  reference_levdist(): two rolling rows over string2.` |
|      - | 1101 | ` * Return` |
|      - | 1102 | ` *  The minimal number of weighted edit operations turning $string1 into` |
|      - | 1103 | ` *  $string2.` |
|      - | 1104 | ` */` |
|     34 | 1105 | `static int PH7_builtin_levenshtein(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1106 | `{` |
|      - | 1107 | `	static const char *azParam[] = { "$insertion_cost","$replacement_cost","$deletion_cost" };` |
|      - | 1108 | `	const char *zStr1,*zStr2;` |
|     35 | 1109 | `	sxi64 iCostIns = 1,iCostRep = 1,iCostDel = 1;` |
|      - | 1110 | `	sxi64 *p1,*p2,*pTmp;` |
|      - | 1111 | `	sxi64 c0,c1,c2;` |
|      - | 1112 | `	ph7_value sTmp1,sTmp2;` |
|      - | 1113 | `	int nLen1,nLen2;` |
|      - | 1114 | `	int i1,i2;` |
|      - | 1115 | `	sxi32 rc;` |
|      - | 1116 | `	int i;` |
|     35 | 1117 | `	if( nArg < 2 ){` |
|    ! 0 | 1118 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1119 | `			"ArgumentCountError",` |
|      - | 1120 | `			"levenshtein() expects at least 2 arguments, %d given",` |
|    ! 0 | 1121 | `			nArg` |
|      - | 1122 | `			);` |
|      - | 1123 | `	}` |
|      - | 1124 | `	/* $string1/$string2: null deprecates to "", __toString objects resolve,` |
|      - | 1125 | `	 * everything non-stringish is a TypeError (php ZPP weak mode). */` |
|     35 | 1126 | `	PH7_MemObjInit(pCtx->pVm,&sTmp1);` |
|     35 | 1127 | `	PH7_MemObjInit(pCtx->pVm,&sTmp2);` |
|     35 | 1128 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"levenshtein",1,"$string1","string",` |
|      - | 1129 | `		"levenshtein(): Passing null to parameter #1 ($string1) "` |
|      - | 1130 | `		"of type string is deprecated",` |
|      - | 1131 | `		&sTmp1,&zStr1,&nLen1);` |
|     35 | 1132 | `	if( rc != PH7_OK ) goto out;` |
|     35 | 1133 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"levenshtein",2,"$string2","string",` |
|      - | 1134 | `		"levenshtein(): Passing null to parameter #2 ($string2) "` |
|      - | 1135 | `		"of type string is deprecated",` |
|      - | 1136 | `		&sTmp2,&zStr2,&nLen2);` |
|     35 | 1137 | `	if( rc != PH7_OK ) goto out;` |
|      - | 1138 | `	/* Optional integer costs */` |
|     57 | 1139 | `	for( i = 2 ; i < nArg && i < 5 ; i++ ){` |
|      - | 1140 | `		sxi64 iVal;` |
|     31 | 1141 | `		rc = PH7_IntArgResolve(pCtx,apArg[i],"levenshtein",i+1,azParam[i-2],"int",&iVal);` |
|     31 | 1142 | `		if( rc != PH7_OK ) goto out;` |
|     23 | 1143 | `		if( i == 2 ){` |
|     11 | 1144 | `			iCostIns = iVal;` |
|     18 | 1145 | `		}else if( i == 3 ){` |
|      7 | 1146 | `			iCostRep = iVal;` |
|      4 | 1147 | `		}else{` |
|      7 | 1148 | `			iCostDel = iVal;` |
|      - | 1149 | `		}` |
|     12 | 1150 | `	}` |
|     27 | 1151 | `	if( nLen1 == 0 ){` |
|      3 | 1152 | `		ph7_result_int64(pCtx,(sxi64)nLen2 * iCostIns);` |
|      3 | 1153 | `		rc = PH7_OK;` |
|      3 | 1154 | `		goto out;` |
|      - | 1155 | `	}` |
|     25 | 1156 | `	if( nLen2 == 0 ){` |
|      3 | 1157 | `		ph7_result_int64(pCtx,(sxi64)nLen1 * iCostDel);` |
|      3 | 1158 | `		rc = PH7_OK;` |
|      3 | 1159 | `		goto out;` |
|      - | 1160 | `	}` |
|      - | 1161 | `	/* Two rolling DP rows over string2 (auto-released on return). Reject a` |
|      - | 1162 | `	 * string2 long enough to overflow the 32-bit allocation size. */` |
|     23 | 1163 | `	if( (sxu32)nLen2 >= (SXU32_HIGH / sizeof(sxi64)) - 1 ){` |
|    ! 0 | 1164 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1165 | `		goto out;` |
|      - | 1166 | `	}` |
|     23 | 1167 | `	p1 = (sxi64 *)ph7_context_alloc_chunk(pCtx,(unsigned int)(sizeof(sxi64) * (sxu32)(nLen2 + 1)),FALSE,TRUE);` |
|     23 | 1168 | `	p2 = (sxi64 *)ph7_context_alloc_chunk(pCtx,(unsigned int)(sizeof(sxi64) * (sxu32)(nLen2 + 1)),FALSE,TRUE);` |
|     23 | 1169 | `	if( p1 == 0 \|\| p2 == 0 ){` |
|    ! 0 | 1170 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1171 | `		goto out;` |
|      - | 1172 | `	}` |
|    733 | 1173 | `	for( i2 = 0 ; i2 <= nLen2 ; i2++ ){` |
|    711 | 1174 | `		p1[i2] = (sxi64)i2 * iCostIns;` |
|    356 | 1175 | `	}` |
|    707 | 1176 | `	for( i1 = 0 ; i1 < nLen1 ; i1++ ){` |
|    685 | 1177 | `		p2[0] = p1[0] + iCostDel;` |
| 181111 | 1178 | `		for( i2 = 0 ; i2 < nLen2 ; i2++ ){` |
| 180427 | 1179 | `			c0 = p1[i2] + ((zStr1[i1] == zStr2[i2]) ? 0 : iCostRep);` |
| 180427 | 1180 | `			c1 = p1[i2 + 1] + iCostDel;` |
| 180427 | 1181 | `			if( c1 < c0 ){` |
|  45393 | 1182 | `				c0 = c1;` |
|  22696 | 1183 | `			}` |
| 180427 | 1184 | `			c2 = p2[i2] + iCostIns;` |
| 180427 | 1185 | `			if( c2 < c0 ){` |
|  44809 | 1186 | `				c0 = c2;` |
|  22404 | 1187 | `			}` |
| 180427 | 1188 | `			p2[i2 + 1] = c0;` |
|  90214 | 1189 | `		}` |
|    685 | 1190 | `		pTmp = p1;` |
|    685 | 1191 | `		p1 = p2;` |
|    685 | 1192 | `		p2 = pTmp;` |
|    343 | 1193 | `	}` |
|     23 | 1194 | `	ph7_result_int64(pCtx,p1[nLen2]);` |
|     23 | 1195 | `	rc = PH7_OK;` |
|     17 | 1196 | `out:` |
|     35 | 1197 | `	PH7_MemObjRelease(&sTmp1);` |
|     35 | 1198 | `	PH7_MemObjRelease(&sTmp2);` |
|     35 | 1199 | `	return rc;` |
|     18 | 1200 | `}` |
|      - | 1201 | `/*` |
|      - | 1202 | ` * Longest common substring scan behind similar_text() — a faithful port of` |
|      - | 1203 | ` * PHP's php_similar_str(): O(n*m) scan recording the first longest run.` |
|      - | 1204 | ` */` |
|     26 | 1205 | `static void SimilarStr(const char *zTxt1,int nLen1,const char *zTxt2,int nLen2,` |
|      - | 1206 | `	int *pPos1,int *pPos2,int *pMax,int *pCount)` |
|      1 | 1207 | `{` |
|      - | 1208 | `	const char *p,*q;` |
|     27 | 1209 | `	const char *zEnd1 = &zTxt1[nLen1];` |
|     27 | 1210 | `	const char *zEnd2 = &zTxt2[nLen2];` |
|      - | 1211 | `	int l;` |
|     27 | 1212 | `	*pMax = 0;` |
|     27 | 1213 | `	*pCount = 0;` |
|    143 | 1214 | `	for( p = zTxt1 ; p < zEnd1 ; p++ ){` |
|    843 | 1215 | `		for( q = zTxt2 ; q < zEnd2 ; q++ ){` |
|    999 | 1216 | `			for( l = 0 ; (p+l < zEnd1) && (q+l < zEnd2) && (p[l] == q[l]) ; l++ );` |
|    727 | 1217 | `			if( l > *pMax ){` |
|     25 | 1218 | `				*pMax = l;` |
|     25 | 1219 | `				*pCount += 1;` |
|     25 | 1220 | `				*pPos1 = (int)(p - zTxt1);` |
|     25 | 1221 | `				*pPos2 = (int)(q - zTxt2);` |
|     12 | 1222 | `			}` |
|    364 | 1223 | `		}` |
|     59 | 1224 | `	}` |
|     27 | 1225 | `}` |
|      - | 1226 | `/*` |
|      - | 1227 | ` * Recursive divide-and-conquer behind similar_text() — a faithful port of` |
|      - | 1228 | `` * PHP's php_similar_char(), including its quirky `count > 1` guard on the`` |
|      - | 1229 | ` * left-side recursion.` |
|      - | 1230 | ` */` |
|     26 | 1231 | `static int SimilarChar(const char *zTxt1,int nLen1,const char *zTxt2,int nLen2)` |
|      1 | 1232 | `{` |
|      - | 1233 | `	int nSum;` |
|     27 | 1234 | `	int nPos1 = 0,nPos2 = 0,nMax,nCount;` |
|     27 | 1235 | `	SimilarStr(zTxt1,nLen1,zTxt2,nLen2,&nPos1,&nPos2,&nMax,&nCount);` |
|     27 | 1236 | `	if( (nSum = nMax) != 0 ){` |
|     25 | 1237 | `		if( nPos1 && nPos2 && nCount > 1 ){` |
|    ! 0 | 1238 | `			nSum += SimilarChar(zTxt1,nPos1,zTxt2,nPos2);` |
|    ! 0 | 1239 | `		}` |
|     25 | 1240 | `		if( (nPos1 + nMax < nLen1) && (nPos2 + nMax < nLen2) ){` |
|     13 | 1241 | `			nSum += SimilarChar(&zTxt1[nPos1 + nMax],nLen1 - nPos1 - nMax,` |
|      8 | 1242 | `				&zTxt2[nPos2 + nMax],nLen2 - nPos2 - nMax);` |
|      4 | 1243 | `		}` |
|     12 | 1244 | `	}` |
|     27 | 1245 | `	return nSum;` |
|      1 | 1246 | `}` |
|      - | 1247 | `/*` |
|      - | 1248 | ` * int similar_text(string $string1,string $string2[,float &$percent])` |
|      - | 1249 | ` *  Calculate the similarity between two strings, as the number of matching` |
|      - | 1250 | ` *  characters found by PHP's greedy longest-common-substring recursion.` |
|      - | 1251 | ` *  When $percent is given it receives the similarity in percent:` |
|      - | 1252 | ` *  matching * 200 / (len1 + len2).` |
|      - | 1253 | ` * Return` |
|      - | 1254 | ` *  The number of matching characters in both strings.` |
|      - | 1255 | ` */` |
|     22 | 1256 | `static int PH7_builtin_similar_text(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1257 | `{` |
|      - | 1258 | `	const char *zStr1,*zStr2;` |
|      - | 1259 | `	ph7_value sTmp1,sTmp2;` |
|      - | 1260 | `	int nLen1,nLen2;` |
|      - | 1261 | `	int nSim;` |
|      - | 1262 | `	sxi32 rc;` |
|     23 | 1263 | `	if( nArg < 2 ){` |
|    ! 0 | 1264 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1265 | `			"ArgumentCountError",` |
|      - | 1266 | `			"similar_text() expects at least 2 arguments, %d given",` |
|    ! 0 | 1267 | `			nArg` |
|      - | 1268 | `			);` |
|      - | 1269 | `	}` |
|     23 | 1270 | `	PH7_MemObjInit(pCtx->pVm,&sTmp1);` |
|     23 | 1271 | `	PH7_MemObjInit(pCtx->pVm,&sTmp2);` |
|     23 | 1272 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"similar_text",1,"$string1","string",` |
|      - | 1273 | `		"similar_text(): Passing null to parameter #1 ($string1) "` |
|      - | 1274 | `		"of type string is deprecated",` |
|      - | 1275 | `		&sTmp1,&zStr1,&nLen1);` |
|     23 | 1276 | `	if( rc != PH7_OK ) goto out;` |
|     23 | 1277 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"similar_text",2,"$string2","string",` |
|      - | 1278 | `		"similar_text(): Passing null to parameter #2 ($string2) "` |
|      - | 1279 | `		"of type string is deprecated",` |
|      - | 1280 | `		&sTmp2,&zStr2,&nLen2);` |
|     23 | 1281 | `	if( rc != PH7_OK ) goto out;` |
|     23 | 1282 | `	if( nLen1 + nLen2 == 0 ){` |
|      5 | 1283 | `		nSim = 0;` |
|      3 | 1284 | `	}else{` |
|     19 | 1285 | `		nSim = SimilarChar(zStr1,nLen1,zStr2,nLen2);` |
|      - | 1286 | `	}` |
|     23 | 1287 | `	if( nArg > 2 ){` |
|      - | 1288 | `		/* Write the percentage through the by-ref out-param */` |
|      7 | 1289 | `		ph7_value *pPercent = ph7_context_new_scalar(pCtx);` |
|      7 | 1290 | `		if( pPercent == 0 ){` |
|    ! 0 | 1291 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1292 | `			goto out;` |
|    ! 0 | 1293 | `		}else{` |
|      7 | 1294 | `			double dPct = (nLen1 + nLen2 == 0) ? 0.0 : (double)nSim * 200.0 / (double)(nLen1 + nLen2);` |
|      7 | 1295 | `			ph7_value_double(pPercent,dPct);` |
|      7 | 1296 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[2],pPercent);` |
|      - | 1297 | `		}` |
|      3 | 1298 | `	}` |
|     23 | 1299 | `	ph7_result_int(pCtx,nSim);` |
|     23 | 1300 | `	rc = PH7_OK;` |
|     11 | 1301 | `out:` |
|     23 | 1302 | `	PH7_MemObjRelease(&sTmp1);` |
|     23 | 1303 | `	PH7_MemObjRelease(&sTmp2);` |
|     23 | 1304 | `	return rc;` |
|     12 | 1305 | `}` |
|      - | 1306 | `/*` |
|      - | 1307 | ` * array\|int str_word_count(string $string[,int $format = 0[,?string $characters = null]])` |
|      - | 1308 | ` *  Count (or return) the words inside a string. A word is a run of alphabetic` |
|      - | 1309 | ` *  characters, which may contain (but not start the string with) "'" and "-";` |
|      - | 1310 | ` *  $characters adds extra bytes to the word set ("a..z" ranges supported, as` |
|      - | 1311 | ` *  in PHP's php_charmask).` |
|      - | 1312 | ` *  $format: 0 -> word count, 1 -> array of words, 2 -> array of words keyed` |
|      - | 1313 | ` *  by their byte position in $string.` |
|      - | 1314 | ` * Errors` |
|      - | 1315 | ` *  ValueError when $format is not 0, 1 or 2.` |
|      - | 1316 | ` */` |
|     44 | 1317 | `static int PH7_builtin_str_word_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1318 | `{` |
|      - | 1319 | `	const char *zIn,*zEnd,*zPtr;` |
|     45 | 1320 | `	ph7_value *pArray = 0,*pValue = 0;` |
|      - | 1321 | `	ph7_value sTmp,sListTmp;` |
|      - | 1322 | `	char aMask[256];` |
|     45 | 1323 | `	int bMask = 0;` |
|     45 | 1324 | `	int iFormat = 0;` |
|     45 | 1325 | `	int nCount = 0;` |
|      - | 1326 | `	int nLen;` |
|      - | 1327 | `	sxi32 rc;` |
|     45 | 1328 | `	if( nArg < 1 ){` |
|    ! 0 | 1329 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1330 | `			"ArgumentCountError",` |
|      - | 1331 | `			"str_word_count() expects at least 1 argument, %d given",` |
|    ! 0 | 1332 | `			nArg` |
|      - | 1333 | `			);` |
|      - | 1334 | `	}` |
|     45 | 1335 | `	PH7_MemObjInit(pCtx->pVm,&sTmp);` |
|     45 | 1336 | `	PH7_MemObjInit(pCtx->pVm,&sListTmp);` |
|     45 | 1337 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_word_count",1,"$string","string",` |
|      - | 1338 | `		"str_word_count(): Passing null to parameter #1 ($string) "` |
|      - | 1339 | `		"of type string is deprecated",` |
|      - | 1340 | `		&sTmp,&zIn,&nLen);` |
|     45 | 1341 | `	if( rc != PH7_OK ) goto out;` |
|     45 | 1342 | `	if( nArg > 1 ){` |
|      - | 1343 | `		sxi64 iVal;` |
|     31 | 1344 | `		rc = PH7_IntArgResolve(pCtx,apArg[1],"str_word_count",2,"$format","int",&iVal);` |
|     33 | 1345 | `		if( rc != PH7_OK ) goto out;` |
|     29 | 1346 | `		if( iVal < 0 \|\| iVal > 2 ){` |
|      5 | 1347 | `			rc = PH7_VmThrowException(pCtx,` |
|      - | 1348 | `				"ValueError",` |
|      - | 1349 | `				"str_word_count(): Argument #2 ($format) must be a valid format value"` |
|      - | 1350 | `				);` |
|      5 | 1351 | `			goto out;` |
|      - | 1352 | `		}` |
|     25 | 1353 | `		iFormat = (int)iVal;` |
|     12 | 1354 | `	}` |
|     39 | 1355 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      - | 1356 | `		/* $characters is ?string: null (skipped above) simply keeps the` |
|      - | 1357 | `		 * default word set, no deprecation. */` |
|      - | 1358 | `		const char *zList;` |
|      - | 1359 | `		int nList;` |
|     13 | 1360 | `		rc = StrPredicateResolveArg(pCtx,apArg[2],"str_word_count",3,"$characters","?string",` |
|      - | 1361 | `			"" /* unreachable: null never gets here */,` |
|      - | 1362 | `			&sListTmp,&zList,&nList);` |
|     13 | 1363 | `		if( rc != PH7_OK ) goto out;` |
|     13 | 1364 | `		PH7_BuildCharMask(pCtx,zList,nList,aMask);` |
|     13 | 1365 | `		bMask = 1;` |
|      6 | 1366 | `	}` |
|     39 | 1367 | `	if( iFormat != 0 ){` |
|     25 | 1368 | `		pArray = ph7_context_new_array(pCtx);` |
|     25 | 1369 | `		pValue = ph7_context_new_scalar(pCtx);` |
|     25 | 1370 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1371 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1372 | `			goto out;` |
|      - | 1373 | `		}` |
|     12 | 1374 | `	}` |
|     39 | 1375 | `	zPtr = zIn;` |
|     39 | 1376 | `	zEnd = &zIn[nLen];` |
|     39 | 1377 | `	if( nLen > 0 ){` |
|      - | 1378 | `		/* php: the string's first byte cannot be ' or -, and its last byte` |
|      - | 1379 | `		 * cannot be -, unless the charlist explicitly allows them. */` |
|     33 | 1380 | `		if( (zPtr[0] == '\'' && (!bMask \|\| !aMask[(unsigned char)'\''])) \|\|` |
|     28 | 1381 | `			(zPtr[0] == '-'  && (!bMask \|\| !aMask[(unsigned char)'-'])) ){` |
|      9 | 1382 | `			zPtr++;` |
|      4 | 1383 | `		}` |
|     33 | 1384 | `		if( zEnd[-1] == '-' && (!bMask \|\| !aMask[(unsigned char)'-']) ){` |
|      9 | 1385 | `			zEnd--;` |
|      4 | 1386 | `		}` |
|     16 | 1387 | `	}` |
|    135 | 1388 | `	while( zPtr < zEnd ){` |
|     91 | 1389 | `		const char *zStart = zPtr;` |
|    477 | 1390 | `		while( zPtr < zEnd && ( SyisAlpha((unsigned char)zPtr[0])` |
|    253 | 1391 | `			\|\| (bMask && aMask[(unsigned char)zPtr[0]])` |
|     98 | 1392 | `			\|\| zPtr[0] == '\'' \|\| zPtr[0] == '-' ) ){` |
|    339 | 1393 | `			zPtr++;` |
|      1 | 1394 | `		}` |
|     97 | 1395 | `		if( zPtr > zStart ){` |
|     91 | 1396 | `			if( iFormat == 0 ){` |
|     19 | 1397 | `				nCount++;` |
|     10 | 1398 | `			}else{` |
|     73 | 1399 | `				ph7_value_reset_string_cursor(pValue);` |
|     73 | 1400 | `				if( SXRET_OK != ph7_value_string(pValue,zStart,(int)(zPtr-zStart)) ){` |
|    ! 0 | 1401 | `					rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1402 | `					goto out;` |
|      - | 1403 | `				}` |
|     73 | 1404 | `				if( iFormat == 1 ){` |
|     59 | 1405 | `					if( SXRET_OK != ph7_array_add_elem(pArray,0,pValue) ){` |
|    ! 0 | 1406 | `						rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1407 | `						goto out;` |
|      - | 1408 | `					}` |
|     30 | 1409 | `				}else{` |
|     15 | 1410 | `					if( SXRET_OK != ph7_array_add_intkey_elem(pArray,(int)(zStart-zIn),pValue) ){` |
|    ! 0 | 1411 | `						rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1412 | `						goto out;` |
|      - | 1413 | `					}` |
|      - | 1414 | `				}` |
|      - | 1415 | `			}` |
|     45 | 1416 | `		}` |
|     97 | 1417 | `		zPtr++;` |
|      1 | 1418 | `	}` |
|     37 | 1419 | `	if( iFormat == 0 ){` |
|     13 | 1420 | `		ph7_result_int(pCtx,nCount);` |
|      7 | 1421 | `	}else{` |
|     25 | 1422 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1423 | `	}` |
|     37 | 1424 | `	rc = PH7_OK;` |
|     21 | 1425 | `out:` |
|     43 | 1426 | `	PH7_MemObjRelease(&sTmp);` |
|     43 | 1427 | `	PH7_MemObjRelease(&sListTmp);` |
|     43 | 1428 | `	return rc;` |
|     22 | 1429 | `}` |
|      - | 1430 | `/*` |
|      - | 1431 | ` * string chunk_split(string $body[,int $chunklen = 76 [, string $end = "\r\n" ]])` |
|      - | 1432 | ` *   Split a string into smaller chunks.` |
|      - | 1433 | ` * Parameters` |
|      - | 1434 | ` *  $body` |
|      - | 1435 | ` *   The string to be chunked.` |
|      - | 1436 | ` * $chunklen` |
|      - | 1437 | ` *   The chunk length.` |
|      - | 1438 | ` * $end` |
|      - | 1439 | ` *   The line ending sequence.` |
|      - | 1440 | ` * Return` |
|      - | 1441 | ` *  The chunked string or NULL on failure.` |
|      - | 1442 | ` */` |
|     14 | 1443 | `static int PH7_builtin_chunk_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1444 | `{` |
|     15 | 1445 | `	const char *zIn,*zEnd,*zSep = "\r\n";` |
|      - | 1446 | `	int nSepLen,nChunkLen,nLen;` |
|      - | 1447 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1448 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     15 | 1449 | `	if( nArg < 1 ){` |
|      - | 1450 | `		/* Nothing to split,return null */` |
|    ! 0 | 1451 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1452 | `		return PH7_OK;` |
|      - | 1453 | `	}` |
|      - | 1454 | `	/* initialize/Extract arguments */` |
|     15 | 1455 | `	nSepLen = (int)sizeof("\r\n") - 1;` |
|     15 | 1456 | `	nChunkLen = 76;` |
|     15 | 1457 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 1458 | `	zEnd = &zIn[nLen];` |
|     15 | 1459 | `	if( nArg > 1 ){` |
|      - | 1460 | `		/* Chunk length */` |
|     13 | 1461 | `		nChunkLen = ph7_value_to_int(apArg[1]);` |
|     13 | 1462 | `		if( nChunkLen < 1 ){` |
|      - | 1463 | `			/* PHP 8 throws a catchable ValueError for a non-positive length. */` |
|      3 | 1464 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1465 | `				"chunk_split(): Argument #2 ($length) must be greater than 0");` |
|      - | 1466 | `		}` |
|     11 | 1467 | `		if( nArg > 2 ){` |
|      - | 1468 | `			/* Separator */` |
|      9 | 1469 | `			zSep = ph7_value_to_string(apArg[2],&nSepLen);` |
|      9 | 1470 | `			if( nSepLen < 1 ){` |
|      - | 1471 | `				/* Switch back to the default separator */` |
|      3 | 1472 | `				zSep = "\r\n";` |
|      3 | 1473 | `				nSepLen = (int)sizeof("\r\n") - 1;` |
|      1 | 1474 | `			}` |
|      4 | 1475 | `		}` |
|      5 | 1476 | `	}` |
|      - | 1477 | `	/* Perform the requested operation */` |
|     13 | 1478 | `	if( nChunkLen > nLen ){` |
|      - | 1479 | `		/* Nothing to split,return the string and the separator */` |
|      9 | 1480 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nLen,zIn,nSepLen,zSep);` |
|      9 | 1481 | `		return PH7_OK;` |
|      - | 1482 | `	}` |
|     17 | 1483 | `	while( zIn < zEnd ){` |
|     13 | 1484 | `		if( nChunkLen > (int)(zEnd-zIn) ){` |
|      3 | 1485 | `			nChunkLen = (int)(zEnd - zIn);` |
|      1 | 1486 | `		}` |
|      - | 1487 | `		/* Append the chunk and the separator */` |
|     13 | 1488 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nChunkLen,zIn,nSepLen,zSep);` |
|      - | 1489 | `		/* Point beyond the chunk */` |
|     13 | 1490 | `		zIn += nChunkLen;` |
|      1 | 1491 | `	}` |
|      5 | 1492 | `	return PH7_OK;` |
|      8 | 1493 | `}` |
|      - | 1494 | `/*` |
|      - | 1495 | ` * string addslashes(string $str)` |
|      - | 1496 | ` *  Quote string with slashes.` |
|      - | 1497 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1498 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1499 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1500 | ` * Parameter` |
|      - | 1501 | ` *  str: The string to be escaped.` |
|      - | 1502 | ` * Return` |
|      - | 1503 | ` *  Returns the escaped string` |
|      - | 1504 | ` */` |
|     20 | 1505 | `static int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1506 | `{` |
|      - | 1507 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1508 | `	int nLen;` |
|      - | 1509 | `	/* PHP enforces exactly one argument. */` |
|     22 | 1510 | `	if( nArg != 1 ){` |
|      4 | 1511 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1512 | `			"ArgumentCountError",` |
|      - | 1513 | `			"addslashes() expects exactly 1 argument, %d given",` |
|      1 | 1514 | `			nArg` |
|      - | 1515 | `			);` |
|      - | 1516 | `	}` |
|      - | 1517 | `	/* NULL is deprecated and treated as an empty string; other invalid` |
|      - | 1518 | `	 * types still produce a TypeError. */` |
|     19 | 1519 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 1520 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 1521 | `			E_DEPRECATED,` |
|      - | 1522 | `			"addslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - | 1523 | `			);` |
|      - | 1524 | `		/* fall through so conversion below yields empty string */` |
|      1 | 1525 | `	}` |
|      - | 1526 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     27 | 1527 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     28 | 1528 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     18 | 1529 | `	    ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 1530 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1531 | `			"TypeError",` |
|      - | 1532 | `			"addslashes(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 1533 | `			ph7_type_name(apArg[0])` |
|      - | 1534 | `			);` |
|      - | 1535 | `	}` |
|      - | 1536 | `	/* Convert to string representation first and obtain length. */` |
|     19 | 1537 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 1538 | `	if( nLen < 1 ){` |
|      - | 1539 | `		/* Return the empty string */` |
|      5 | 1540 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 1541 | `		return PH7_OK;` |
|      - | 1542 | `	}` |
|     15 | 1543 | `	zEnd = &zIn[nLen];` |
|     15 | 1544 | `	zCur = 0; /* cc warning */` |
|     20 | 1545 | `	for(;;){` |
|     41 | 1546 | `		if( zIn >= zEnd ){` |
|      - | 1547 | `			/* No more input */` |
|     15 | 1548 | `			break;` |
|      - | 1549 | `		}` |
|     27 | 1550 | `		zCur = zIn;` |
|      - | 1551 | `		/* scan until a character that needs escaping (', ", \\, or NUL) */` |
|     89 | 1552 | `		while( zIn < zEnd && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '\\' && zIn[0] != '\0' ){` |
|     63 | 1553 | `			zIn++;` |
|      1 | 1554 | `		}` |
|     27 | 1555 | `		if( zIn > zCur ){` |
|      - | 1556 | `			/* Append raw contents */` |
|     23 | 1557 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 1558 | `		}` |
|     27 | 1559 | `		if( zIn < zEnd ){` |
|     17 | 1560 | `			int c = zIn[0];` |
|     17 | 1561 | `			if( c == '\0' ){` |
|      - | 1562 | `				/* PHP escapes NUL as "\\0" (two characters) */` |
|      3 | 1563 | `				ph7_result_string(pCtx,"\\0",2);` |
|      2 | 1564 | `			}else{` |
|     15 | 1565 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - | 1566 | `			}` |
|      8 | 1567 | `		}` |
|     27 | 1568 | `		zIn++;` |
|      1 | 1569 | `	}` |
|     15 | 1570 | `	return PH7_OK;` |
|     12 | 1571 | `}` |
|      - | 1572 | `/*` |
|      - | 1573 | ``  * Build a 256-entry membership mask from a PHP charlist, expanding `a..z` `` |
|      - | 1574 | ` * byte ranges exactly like PHP's php_charmask(). On return aMask[c] != 0 iff` |
|      - | 1575 | ` * the byte c belongs to the set. Emits the PHP-exact warnings for the three` |
|      - | 1576 | ` * malformed-range shapes (ph7_context_throw_error_format prepends the active` |
|      - | 1577 | ` * function name, so the messages omit it); on a bad range the surrounding` |
|      - | 1578 | ` * bytes are still added and the scan never aborts. Reads only within` |
|      - | 1579 | ` * [zList, zList+nLen).` |
|      - | 1580 | ` *` |
|      - | 1581 | ` * Use ONLY for the builtins whose charlist expands ranges the way PHP's` |
|      - | 1582 | ` * php_charmask() does: trim/ltrim/rtrim/addcslashes (and quotemeta, whose set` |
|      - | 1583 | ` * is a fixed literal with no ".."). Do NOT route strspn/strcspn/strtok/strpbrk` |
|      - | 1584 | ` * through this — PHP treats their charlists literally, so expanding "a..z" here` |
|      - | 1585 | ` * would be a behavior regression plus spurious "Invalid '..'-range" warnings.` |
|      - | 1586 | ` */` |
|    236 | 1587 | `static void PH7_BuildCharMask(ph7_context *pCtx,const char *zList,int nLen,char aMask[256])` |
|      4 | 1588 | `{` |
|    240 | 1589 | `	const unsigned char *zIn  = (const unsigned char *)zList;` |
|    240 | 1590 | `	const unsigned char *zEnd = zIn + (nLen > 0 ? nLen : 0);` |
|    240 | 1591 | `	SyZero(aMask,256);` |
|    638 | 1592 | `	for( ; zIn < zEnd ; zIn++ ){` |
|    402 | 1593 | `		int c = zIn[0];` |
|    402 | 1594 | `		if( zIn + 3 < zEnd && zIn[1] == '.' && zIn[2] == '.' && zIn[3] >= c ){` |
|      - | 1595 | `			/* Valid incrementing range c..zIn[3] */` |
|     22 | 1596 | `			int hi = zIn[3],k;` |
|    386 | 1597 | `			for( k = c ; k <= hi ; k++ ){` |
|    366 | 1598 | `				aMask[k] = 1;` |
|    184 | 1599 | `			}` |
|     22 | 1600 | `			zIn += 3; /* the loop's ++ then steps past the range end */` |
|    401 | 1601 | `		}else if( zIn + 1 < zEnd && zIn[0] == '.' && zIn[1] == '.' ){` |
|      - | 1602 | `			/* Malformed range: mirror php_charmask's three diagnostics. */` |
|      - | 1603 | `			const char *zMsg;` |
|     20 | 1604 | `			if( (const unsigned char *)zList >= zIn ){` |
|      6 | 1605 | `				zMsg = "no character to the left of '..'";` |
|     18 | 1606 | `			}else if( zIn + 2 >= zEnd ){` |
|      6 | 1607 | `				zMsg = "no character to the right of '..'";` |
|     14 | 1608 | `			}else if( zIn[-1] > zIn[2] ){` |
|     12 | 1609 | `				zMsg = "'..'-range needs to be incrementing";` |
|      7 | 1610 | `			}else{` |
|    ! 0 | 1611 | `				zMsg = 0; /* catch-all (e.g. a..b..c) */` |
|      - | 1612 | `			}` |
|     20 | 1613 | `			if( zMsg ){` |
|     29 | 1614 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      9 | 1615 | `					"Invalid '..'-range, %s",zMsg);` |
|     11 | 1616 | `			}else{` |
|    ! 0 | 1617 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1618 | `					"Invalid '..'-range");` |
|      - | 1619 | `			}` |
|      - | 1620 | `			/* Do not consume the dots: the loop's ++ steps one byte so the` |
|      - | 1621 | `			 * dots are re-scanned as literals, exactly like php_charmask. */` |
|     11 | 1622 | `		}else{` |
|    364 | 1623 | `			aMask[c] = 1;` |
|      - | 1624 | `		}` |
|    203 | 1625 | `	}` |
|    240 | 1626 | `}` |
|      - | 1627 | `/*` |
|      - | 1628 | ` * string addcslashes(string $str,string $charlist)` |
|      - | 1629 | ` *  Quote string with slashes in a C style.` |
|      - | 1630 | ` * Parameter` |
|      - | 1631 | ` *  $str:` |
|      - | 1632 | ` *    The string to be escaped.` |
|      - | 1633 | ` *  $charlist:` |
|      - | 1634 | ` *    A list of characters to be escaped. If charlist contains characters \n, \r etc.` |
|      - | 1635 | ` *    they are converted in C-like style, while other non-alphanumeric characters` |
|      - | 1636 | ` *    with ASCII codes lower than 32 and higher than 126 converted to octal representation.` |
|      - | 1637 | ` * Return` |
|      - | 1638 | ` *  Returns the escaped string.` |
|      - | 1639 | ` * Note:` |
|      - | 1640 | ` *  Character ranges [i.e: 'A..Z'] are supported (see PH7_BuildCharMask).` |
|      - | 1641 | ` */` |
|     34 | 1642 | `static int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1643 | `{` |
|      - | 1644 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|      - | 1645 | `	char aMask[256];` |
|      - | 1646 | `	int nLen,nMask;` |
|      - | 1647 | `	/* PHP enforces exactly two arguments. */` |
|     38 | 1648 | `	if( nArg != 2 ){` |
|      4 | 1649 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1650 | `			"ArgumentCountError",` |
|      - | 1651 | `			"addcslashes() expects exactly 2 arguments, %d given",` |
|      1 | 1652 | `			nArg` |
|      - | 1653 | `			);` |
|      - | 1654 | `	}` |
|      - | 1655 | `	/* First argument must be a string-ish value.  NULL is deprecated and` |
|      - | 1656 | `	 * treated as the empty string (PHP 8.1). */` |
|     35 | 1657 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      - | 1658 | `		/* Emit deprecation only once, similar to PHP behaviour. */` |
|      3 | 1659 | `		PH7_VmThrowError(pCtx->pVm,0,/* iErr will be patched to 8192 below */` |
|      - | 1660 | `			E_DEPRECATED,` |
|      - | 1661 | `			"addcslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - | 1662 | `			);` |
|      - | 1663 | `		/* treat as empty string; fall through to conversion logic */` |
|     47 | 1664 | `	} else if( ph7_value_is_array(apArg[0]) \|\|` |
|     48 | 1665 | `	          ph7_value_is_object(apArg[0]) \|\|` |
|     30 | 1666 | `	          ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 1667 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1668 | `			"TypeError",` |
|      - | 1669 | `			"addcslashes(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 1670 | `			ph7_type_name(apArg[0])` |
|      - | 1671 | `			);` |
|      - | 1672 | `	}` |
|      - | 1673 | `	/* Second argument must be a string.  NULL is deprecated and treated as` |
|      - | 1674 | `	 * an empty mask per PHP semantics.  Arrays/objects/resources still` |
|      - | 1675 | `	 * trigger a TypeError. */` |
|     35 | 1676 | `	if( ph7_value_is_null(apArg[1]) ){` |
|      3 | 1677 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 1678 | `			E_DEPRECATED,` |
|      - | 1679 | `			"addcslashes(): Passing null to parameter #2 ($characters) of type string is deprecated"` |
|      - | 1680 | `			);` |
|      - | 1681 | `		/* allow through so it becomes empty string below */` |
|     47 | 1682 | `	} else if( ph7_value_is_array(apArg[1]) \|\|` |
|     48 | 1683 | `	          ph7_value_is_object(apArg[1]) \|\|` |
|     30 | 1684 | `	          ph7_value_is_resource(apArg[1]) ){` |
|    ! 0 | 1685 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1686 | `			"TypeError",` |
|      - | 1687 | `			"addcslashes(): Argument #2 ($characters) must be of type string, %s given",` |
|    ! 0 | 1688 | `			ph7_type_name(apArg[1])` |
|      - | 1689 | `			);` |
|      - | 1690 | `	}` |
|      - | 1691 | `	/* Extract the string to process */` |
|     35 | 1692 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1693 | `	/* NULL would never reach here due to the check above. */` |
|     35 | 1694 | `	if( nLen < 1 ){` |
|      - | 1695 | `		/* Empty string returns itself. */` |
|      5 | 1696 | `		ph7_result_string(pCtx,zIn,nLen);` |
|      5 | 1697 | `		return PH7_OK;` |
|      - | 1698 | `	}` |
|      - | 1699 | ``	/* Extract the desired mask and expand any `a..z` ranges into a lookup. */`` |
|     31 | 1700 | `	zMask = ph7_value_to_string(apArg[1],&nMask);` |
|     31 | 1701 | `	PH7_BuildCharMask(pCtx,zMask,nMask,aMask);` |
|     31 | 1702 | `	zEnd = &zIn[nLen];` |
|     31 | 1703 | `	zCur = 0; /* cc warning */` |
|     37 | 1704 | `	for(;;){` |
|     77 | 1705 | `		if( zIn >= zEnd ){` |
|      - | 1706 | `			/* No more input */` |
|     31 | 1707 | `			break;` |
|      - | 1708 | `		}` |
|     49 | 1709 | `		zCur = zIn;` |
|    125 | 1710 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|     79 | 1711 | `			zIn++;` |
|      3 | 1712 | `		}` |
|     49 | 1713 | `		if( zIn > zCur ){` |
|      - | 1714 | `			/* Append raw contents */` |
|     43 | 1715 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     20 | 1716 | `		}` |
|     49 | 1717 | `		if( zIn < zEnd ){` |
|      - | 1718 | `			/* Make sure we treat the byte as unsigned to avoid negative values` |
|      - | 1719 | `			 * on platforms where char is signed. */` |
|     29 | 1720 | `			int c = (unsigned char)zIn[0];` |
|      - | 1721 | `			/* Handle special C-like escapes for common control characters first.` |
|      - | 1722 | `			 * PHP outputs "\n" "\r" "\t" "\v" "\f" when those chars are` |
|      - | 1723 | `			 * in the mask. NUL is left to the octal conversion below. */` |
|     29 | 1724 | `			if( c == '\n' ){` |
|      3 | 1725 | `				ph7_result_string(pCtx,"\\n",2);` |
|     28 | 1726 | `			}else if( c == '\r' ){` |
|      3 | 1727 | `				ph7_result_string(pCtx,"\\r",2);` |
|     26 | 1728 | `			}else if( c == '\t' ){` |
|      3 | 1729 | `				ph7_result_string(pCtx,"\\t",2);` |
|     24 | 1730 | `			}else if( c == '\v' ){` |
|      3 | 1731 | `				ph7_result_string(pCtx,"\\v",2);` |
|     22 | 1732 | `			}else if( c == '\f' ){` |
|      3 | 1733 | `				ph7_result_string(pCtx,"\\f",2);` |
|     20 | 1734 | `			}else if( c > 126 \|\| (c < 32 && (!SyisAlphaNum(c)/*EBCDIC*/ && !SyisSpace(c))) ){` |
|      - | 1735 | `				/* Convert to octal.  PHP always emits three-digit zero-padded` |
|      - | 1736 | `				 * octal escapes (\001 not \1). */` |
|      7 | 1737 | `				ph7_result_string_format(pCtx,"\\%03o",c);` |
|      4 | 1738 | `			}else{` |
|     13 | 1739 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - | 1740 | `			}` |
|     13 | 1741 | `		}` |
|     49 | 1742 | `		zIn++;` |
|      3 | 1743 | `	}` |
|     31 | 1744 | `	return PH7_OK;` |
|     21 | 1745 | `}` |
|      - | 1746 | `/*` |
|      - | 1747 | ` * string quotemeta(string $str)` |
|      - | 1748 | ` *  Quote meta characters.` |
|      - | 1749 | ` * Parameter` |
|      - | 1750 | ` *  $str:` |
|      - | 1751 | ` *    The string to be escaped.` |
|      - | 1752 | ` * Return` |
|      - | 1753 | ` *  Returns the escaped string.` |
|      - | 1754 | `*/` |
|     10 | 1755 | `static int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1756 | `{` |
|      - | 1757 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1758 | `	char aMask[256];` |
|      - | 1759 | `	int nLen;` |
|     12 | 1760 | `	if( nArg < 1 ){` |
|      - | 1761 | `		/* Nothing to process,retun NULL */` |
|    ! 0 | 1762 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1763 | `		return PH7_OK;` |
|      - | 1764 | `	}` |
|      - | 1765 | `	/* Extract the string to process */` |
|     12 | 1766 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     12 | 1767 | `	if( nLen < 1 ){` |
|      - | 1768 | `		/* Return the empty string */` |
|      3 | 1769 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1770 | `		return PH7_OK;` |
|      - | 1771 | `	}` |
|      - | 1772 | `	/* Fixed meta-character set (no ranges); build the lookup once. */` |
|     10 | 1773 | `	PH7_BuildCharMask(pCtx,".\\+*?[^]($)",(int)sizeof(".\\+*?[^]($)")-1,aMask);` |
|     10 | 1774 | `	zEnd = &zIn[nLen];` |
|     10 | 1775 | `	zCur = 0; /* cc warning */` |
|     22 | 1776 | `	for(;;){` |
|     46 | 1777 | `		if( zIn >= zEnd ){` |
|      - | 1778 | `			/* No more input */` |
|     10 | 1779 | `			break;` |
|      - | 1780 | `		}` |
|     38 | 1781 | `		zCur = zIn;` |
|     76 | 1782 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|     40 | 1783 | `			zIn++;` |
|      2 | 1784 | `		}` |
|     38 | 1785 | `		if( zIn > zCur ){` |
|      - | 1786 | `			/* Append raw contents */` |
|     20 | 1787 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      9 | 1788 | `		}` |
|     38 | 1789 | `		if( zIn < zEnd ){` |
|     36 | 1790 | `			int c = zIn[0];` |
|     36 | 1791 | `			ph7_result_string_format(pCtx,"\\%c",c);` |
|     17 | 1792 | `		}` |
|     38 | 1793 | `		zIn++;` |
|      2 | 1794 | `	}` |
|     10 | 1795 | `	return PH7_OK;` |
|      7 | 1796 | `}` |
|      - | 1797 | `/*` |
|      - | 1798 | ` * string stripslashes(string $str)` |
|      - | 1799 | ` *  Un-quotes a quoted string.` |
|      - | 1800 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1801 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1802 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1803 | ` * Parameter` |
|      - | 1804 | ` *  $str` |
|      - | 1805 | ` *   The input string.` |
|      - | 1806 | ` * Return` |
|      - | 1807 | ` *  Returns a string with backslashes stripped off.` |
|      - | 1808 | ` */` |
|      6 | 1809 | `static int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1810 | `{` |
|      - | 1811 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1812 | `	int nLen;` |
|      7 | 1813 | `	if( nArg < 1 ){` |
|      - | 1814 | `		/* Nothing to process,retun NULL */` |
|    ! 0 | 1815 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1816 | `		return PH7_OK;` |
|      - | 1817 | `	}` |
|      - | 1818 | `	/* Extract the string to process */` |
|      7 | 1819 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 1820 | `	if( zIn == 0 ){` |
|    ! 0 | 1821 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1822 | `		return PH7_OK;` |
|      - | 1823 | `	}` |
|      7 | 1824 | `	zEnd = &zIn[nLen];` |
|      7 | 1825 | `	zCur = 0; /* cc warning */` |
|      - | 1826 | `	/* Encode the string */` |
|      4 | 1827 | `	for(;;){` |
|      9 | 1828 | `		if( zIn >= zEnd ){` |
|      - | 1829 | `			/* No more input */` |
|      5 | 1830 | `			break;` |
|      - | 1831 | `		}` |
|      5 | 1832 | `		zCur = zIn;` |
|     17 | 1833 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|     13 | 1834 | `			zIn++;` |
|      1 | 1835 | `		}` |
|      5 | 1836 | `		if( zIn > zCur ){` |
|      - | 1837 | `			/* Append raw contents */` |
|      5 | 1838 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 1839 | `		}` |
|      5 | 1840 | `		if( &zIn[1] < zEnd ){` |
|      3 | 1841 | `			int c = zIn[1];` |
|      3 | 1842 | `			if( c == '\'' \|\| c == '"' \|\| c == '\\' ){` |
|      - | 1843 | `				/* Ignore the backslash */` |
|      3 | 1844 | `				zIn++;` |
|      1 | 1845 | `			}` |
|      2 | 1846 | `		}else{` |
|      3 | 1847 | `			break;` |
|      - | 1848 | `		}` |
|      1 | 1849 | `	}` |
|      7 | 1850 | `	return PH7_OK;` |
|      4 | 1851 | `}` |
|      - | 1852 | `/*` |
|      - | 1853 | ` * UTF-8-aware HTML entity machinery, shared by htmlspecialchars/htmlentities/` |
|      - | 1854 | ` * htmlspecialchars_decode/html_entity_decode/get_html_translation_table.` |
|      - | 1855 | ` * The implementations live further down in this file, next to the filter_var` |
|      - | 1856 | ` * FULL_SPECIAL_CHARS machinery they reuse (aHtml401Ent[]/FvHtml401Lookup()/` |
|      - | 1857 | ` * FvUtf8Next()). Semantics are byte-exact vs php 8.5.7; PHL is UTF-8-only` |
|      - | 1858 | ` * so every charset argument other than a UTF-8 alias gets PHP's` |
|      - | 1859 | ` * unsupported-charset warning and is treated as UTF-8.` |
|      - | 1860 | ` *` |
|      - | 1861 | ` * Flag model (the PHP-exact ENT_* values, see constant.c): bit 1 = encode/` |
|      - | 1862 | ` * decode single quotes, bit 2 = double quotes (ENT_QUOTES=3, ENT_COMPAT=2,` |
|      - | 1863 | ` * ENT_NOQUOTES=0); bits 16\|32 select the doctype (0=HTML401, 16=XML1,` |
|      - | 1864 | ` * 32=XHTML, 48=HTML5); ENT_IGNORE=4 drops invalid UTF-8 bytes (wins over` |
|      - | 1865 | ` * ENT_SUBSTITUTE=8, which replaces each with U+FFFD; with neither set the` |
|      - | 1866 | ` * whole result collapses to ""); ENT_DISALLOWED=128 substitutes valid but` |
|      - | 1867 | ` * doctype-disallowed codepoints. The shared default is` |
|      - | 1868 | ` * ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 = 11.` |
|      - | 1869 | ` */` |
|      - | 1870 | `static void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,int iFlags,int bAll,int bDoubleEncode);` |
|      - | 1871 | `static void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,int iFlags,int bFull);` |
|      - | 1872 | `static void HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx);` |
|      - | 1873 | `static void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags);` |
|      - | 1874 | `/*` |
|      - | 1875 | ` * string htmlspecialchars(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1876 | ` *                         [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])` |
|      - | 1877 | ` *  Convert the special characters & < > " ' to HTML entities.` |
|      - | 1878 | ` * Return` |
|      - | 1879 | ` *  The escaped string or NULL on failure.` |
|      - | 1880 | ` */` |
|     42 | 1881 | `static int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1882 | `{` |
|     43 | 1883 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1884 | `	const char *zIn;` |
|     43 | 1885 | `	int nLen,bDouble = 1;` |
|      - | 1886 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1887 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     43 | 1888 | `	if( nArg < 1 ){` |
|      - | 1889 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1890 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1891 | `		return PH7_OK;` |
|      - | 1892 | `	}` |
|      - | 1893 | `	/* Extract the target string */` |
|     43 | 1894 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     43 | 1895 | `	if( nArg > 1 ){` |
|     35 | 1896 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     17 | 1897 | `	}` |
|     43 | 1898 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     43 | 1899 | `	if( nArg > 3 ){` |
|      7 | 1900 | `		bDouble = ph7_value_to_bool(apArg[3]);` |
|      3 | 1901 | `	}` |
|     43 | 1902 | `	HtmlEscape(pCtx,zIn,nLen,iFlags,0,bDouble);` |
|     43 | 1903 | `	return PH7_OK;` |
|     22 | 1904 | `}` |
|      - | 1905 | `/*` |
|      - | 1906 | ` * string htmlspecialchars_decode(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401])` |
|      - | 1907 | ` *  Convert the special HTML entities (&amp; &lt; &gt; &quot; and the` |
|      - | 1908 | ` *  numeric/doctype forms of the two quotes) back to characters.` |
|      - | 1909 | ` * Return` |
|      - | 1910 | ` *  The unescaped string or NULL on failure.` |
|      - | 1911 | ` */` |
|     22 | 1912 | `static int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1913 | `{` |
|     23 | 1914 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1915 | `	const char *zIn;` |
|      - | 1916 | `	int nLen;` |
|      - | 1917 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1918 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     23 | 1919 | `	if( nArg < 1 ){` |
|      - | 1920 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1921 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1922 | `		return PH7_OK;` |
|      - | 1923 | `	}` |
|      - | 1924 | `	/* Extract the target string */` |
|     23 | 1925 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 1926 | `	if( nArg > 1 ){` |
|      9 | 1927 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      4 | 1928 | `	}` |
|     23 | 1929 | `	HtmlUnescape(pCtx,zIn,nLen,iFlags,0);` |
|     23 | 1930 | `	return PH7_OK;` |
|     12 | 1931 | `}` |
|      - | 1932 | `/*` |
|      - | 1933 | ` * array get_html_translation_table(int $table = HTML_SPECIALCHARS` |
|      - | 1934 | ` *      [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 [, string $encoding = "UTF-8"]])` |
|      - | 1935 | ` *  Return the translation table used by htmlspecialchars() (HTML_SPECIALCHARS)` |
|      - | 1936 | ` *  or htmlentities() (HTML_ENTITIES) as character => entity pairs.` |
|      - | 1937 | ` * Return` |
|      - | 1938 | ` *  The translation table as an array or NULL on failure.` |
|      - | 1939 | ` */` |
|     12 | 1940 | `static int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1941 | `{` |
|     13 | 1942 | `	int iTable = 0; /* HTML_SPECIALCHARS */` |
|     13 | 1943 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|     13 | 1944 | `	if( nArg > 0 ){` |
|     11 | 1945 | `		iTable = ph7_value_to_int(apArg[0]);` |
|      5 | 1946 | `	}` |
|     13 | 1947 | `	if( nArg > 1 ){` |
|      9 | 1948 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      4 | 1949 | `	}` |
|     13 | 1950 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     13 | 1951 | `	HtmlTranslationTable(pCtx,iTable,iFlags);` |
|     13 | 1952 | `	return PH7_OK;` |
|      1 | 1953 | `}` |
|      - | 1954 | `/*` |
|      - | 1955 | ` * string htmlentities(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1956 | ` *                     [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])` |
|      - | 1957 | ` *  Convert all applicable characters to HTML entities: the specials plus` |
|      - | 1958 | ` *  every codepoint with an HTML 4.01 named entity (aHtml401Ent[]).` |
|      - | 1959 | ` * Return` |
|      - | 1960 | ` *  The encoded string or NULL on failure.` |
|      - | 1961 | ` */` |
|     30 | 1962 | `static int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1963 | `{` |
|     31 | 1964 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1965 | `	const char *zIn;` |
|     31 | 1966 | `	int nLen,bDouble = 1;` |
|      - | 1967 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1968 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     31 | 1969 | `	if( nArg < 1 ){` |
|      - | 1970 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1971 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1972 | `		return PH7_OK;` |
|      - | 1973 | `	}` |
|      - | 1974 | `	/* Extract the target string */` |
|     31 | 1975 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 1976 | `	if( nArg > 1 ){` |
|     19 | 1977 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      9 | 1978 | `	}` |
|     31 | 1979 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     31 | 1980 | `	if( nArg > 3 ){` |
|      3 | 1981 | `		bDouble = ph7_value_to_bool(apArg[3]);` |
|      1 | 1982 | `	}` |
|     31 | 1983 | `	HtmlEscape(pCtx,zIn,nLen,iFlags,1,bDouble);` |
|     31 | 1984 | `	return PH7_OK;` |
|     16 | 1985 | `}` |
|      - | 1986 | `/*` |
|      - | 1987 | ` * string html_entity_decode(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1988 | ` *                           [, string $encoding = "UTF-8"]])` |
|      - | 1989 | ` *  Convert HTML entities (named — case-sensitive — and numeric, decimal or` |
|      - | 1990 | ` *  hex) back to their UTF-8 characters. The reverse of htmlentities().` |
|      - | 1991 | ` * Return` |
|      - | 1992 | ` *  The decoded string or NULL on failure.` |
|      - | 1993 | ` */` |
|     58 | 1994 | `static int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1995 | `{` |
|     59 | 1996 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1997 | `	const char *zIn;` |
|      - | 1998 | `	int nLen;` |
|      - | 1999 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 2000 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     59 | 2001 | `	if( nArg < 1 ){` |
|      - | 2002 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 2003 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2004 | `		return PH7_OK;` |
|      - | 2005 | `	}` |
|      - | 2006 | `	/* Extract the target string */` |
|     59 | 2007 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     59 | 2008 | `	if( nArg > 1 ){` |
|     27 | 2009 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     13 | 2010 | `	}` |
|     59 | 2011 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     59 | 2012 | `	HtmlUnescape(pCtx,zIn,nLen,iFlags,1);` |
|     59 | 2013 | `	return PH7_OK;` |
|     30 | 2014 | `}` |
|      - | 2015 | `/*` |
|      - | 2016 | ` * int strlen($string)` |
|      - | 2017 | ` *  return the length of the given string.` |
|      - | 2018 | ` * Parameter` |
|      - | 2019 | ` *  string: The string being measured for length.` |
|      - | 2020 | ` * Return` |
|      - | 2021 | ` *  length of the given string.` |
|      - | 2022 | ` */` |
|  75806 | 2023 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2024 | `{` |
|  75811 | 2025 | `	int iLen = 0;` |
|  75811 | 2026 | `	if( nArg > 0 ){` |
|  75811 | 2027 | `		StrNullArgNotice(pCtx,apArg[0],"strlen",1,"$string");` |
|  75811 | 2028 | `		ph7_value_to_string(apArg[0],&iLen);` |
|  37903 | 2029 | `	}` |
|      - | 2030 | `	/* String length */` |
|  75811 | 2031 | `	ph7_result_int(pCtx,iLen);` |
|  75811 | 2032 | `	return PH7_OK;` |
|      5 | 2033 | `}` |
|      - | 2034 | `/*` |
|      - | 2035 | ` * int strcmp(string $str1,string $str2)` |
|      - | 2036 | ` *  Perform a binary safe string comparison.` |
|      - | 2037 | ` * Parameter` |
|      - | 2038 | ` *  str1: The first string` |
|      - | 2039 | ` *  str2: The second string` |
|      - | 2040 | ` * Return` |
|      - | 2041 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2042 | ` *  than str2, and 0 if they are equal.` |
|      - | 2043 | ` */` |
|     72 | 2044 | `static int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2045 | `{` |
|      - | 2046 | `	const char *z1,*z2;` |
|      - | 2047 | `	int n1,n2;` |
|      - | 2048 | `	int res;` |
|     73 | 2049 | `	if( nArg < 2 ){` |
|    ! 0 | 2050 | `		res = nArg == 0 ? 0 : 1;` |
|    ! 0 | 2051 | `		ph7_result_int(pCtx,res);` |
|    ! 0 | 2052 | `		return PH7_OK;` |
|      - | 2053 | `	}` |
|      - | 2054 | `	/* Perform the comparison */` |
|     73 | 2055 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     73 | 2056 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     73 | 2057 | `	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2058 | `	/* Comparison result */` |
|     73 | 2059 | `	ph7_result_int(pCtx,res);` |
|     73 | 2060 | `	return PH7_OK;` |
|     37 | 2061 | `}` |
|      - | 2062 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 2063 | `/*` |
|      - | 2064 | ` * The natural-order comparison core lives OUTSIDE the PH7_DISABLE_BUILTIN_FUNC` |
|      - | 2065 | ` * guard: hashmap.c's SORT_NATURAL path (always compiled) calls PH7_StrNatCmp, so` |
|      - | 2066 | ` * it must exist in the tiny build too. [[tiny-build-disk-io-guard-fragility]]` |
|      - | 2067 | ` */` |
|      - | 2068 | `/*` |
|      - | 2069 | ` * Natural-order comparison core (Martin Pool's natcompare as adapted by php's` |
|      - | 2070 | ` * ext/standard/strnatcmp.c): digit runs compare numerically — the longer run` |
|      - | 2071 | ` * wins, a leading zero flips to fractional first-difference-wins semantics —` |
|      - | 2072 | ` * everything else compares bytewise with whitespace skipped.` |
|      - | 2073 | ` */` |
|     42 | 2074 | `static int StrNatCompareRight(const char **pa,const char *aEnd,const char **pb,const char *bEnd)` |
|      1 | 2075 | `{` |
|     43 | 2076 | `	int bias = 0;` |
|     71 | 2077 | `	for(;;){` |
|     93 | 2078 | `		int da = (*pa < aEnd) && SyisDigit(**pa);` |
|     93 | 2079 | `		int db = (*pb < bEnd) && SyisDigit(**pb);` |
|     93 | 2080 | `		if( !da && !db ){ return bias; }` |
|     73 | 2081 | `		if( !da ){ return -1; }` |
|     59 | 2082 | `		if( !db ){ return 1; }` |
|     51 | 2083 | `		if( **pa < **pb ){ if( !bias ){ bias = -1; } }` |
|     39 | 2084 | `		else if( **pa > **pb ){ if( !bias ){ bias = 1; } }` |
|     51 | 2085 | `		(*pa)++;` |
|     51 | 2086 | `		(*pb)++;` |
|      1 | 2087 | `	}` |
|     22 | 2088 | `}` |
|      4 | 2089 | `static int StrNatCompareLeft(const char **pa,const char *aEnd,const char **pb,const char *bEnd)` |
|      1 | 2090 | `{` |
|      2 | 2091 | `	for(;;){` |
|      5 | 2092 | `		int da = (*pa < aEnd) && SyisDigit(**pa);` |
|      5 | 2093 | `		int db = (*pb < bEnd) && SyisDigit(**pb);` |
|      5 | 2094 | `		if( !da && !db ){ return 0; }` |
|      5 | 2095 | `		if( !da ){ return -1; }` |
|      5 | 2096 | `		if( !db ){ return 1; }` |
|      5 | 2097 | `		if( **pa < **pb ){ return -1; }` |
|    ! 0 | 2098 | `		if( **pa > **pb ){ return 1; }` |
|    ! 0 | 2099 | `		(*pa)++;` |
|    ! 0 | 2100 | `		(*pb)++;` |
|    ! 0 | 2101 | `	}` |
|      3 | 2102 | `}` |
|     48 | 2103 | `PH7_PRIVATE int PH7_StrNatCmp(const char *zA,int nA,const char *zB,int nB,int bFold)` |
|      1 | 2104 | `{` |
|     49 | 2105 | `	const char *a = zA,*aEnd = &zA[nA];` |
|     49 | 2106 | `	const char *b = zB,*bEnd = &zB[nB];` |
|    146 | 2107 | `	for(;;){` |
|      - | 2108 | `		int ca,cb;` |
|    175 | 2109 | `		while( a < aEnd && SyisSpace(a[0]) ){ a++; }` |
|    173 | 2110 | `		while( b < bEnd && SyisSpace(b[0]) ){ b++; }` |
|    173 | 2111 | `		ca = (a < aEnd) ? (unsigned char)a[0] : 0;` |
|    173 | 2112 | `		cb = (b < bEnd) ? (unsigned char)b[0] : 0;` |
|    173 | 2113 | `		if( SyisDigit(ca) && SyisDigit(cb) ){` |
|     45 | 2114 | `			int r = (ca == '0' \|\| cb == '0')` |
|      4 | 2115 | `				? StrNatCompareLeft(&a,aEnd,&b,bEnd)` |
|     65 | 2116 | `				: StrNatCompareRight(&a,aEnd,&b,bEnd);` |
|     47 | 2117 | `			if( r ){ return r; }` |
|      5 | 2118 | `			continue;` |
|      - | 2119 | `		}` |
|    127 | 2120 | `		if( ca == 0 && cb == 0 ){ return 0; }` |
|    121 | 2121 | `		if( bFold ){` |
|     67 | 2122 | `			ca = SyToLower(ca);` |
|     67 | 2123 | `			cb = SyToLower(cb);` |
|     33 | 2124 | `		}` |
|    121 | 2125 | `		if( ca < cb ){ return -1; }` |
|    121 | 2126 | `		if( ca > cb ){ return 1; }` |
|    121 | 2127 | `		a++;` |
|    121 | 2128 | `		b++;` |
|      1 | 2129 | `	}` |
|     25 | 2130 | `}` |
|      - | 2131 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 2132 | `/*` |
|      - | 2133 | ` * int strnatcmp(string $string1, string $string2)` |
|      - | 2134 | ` * int strnatcasecmp(string $string1, string $string2)` |
|      - | 2135 | ` *  Natural-order string comparison ("img2" < "img10"), case folded for the` |
|      - | 2136 | ` *  latter. php 8.2+ normalizes the result to -1/0/1.` |
|      - | 2137 | ` */` |
|     20 | 2138 | `static int PH7_builtin_strnatcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2139 | `{` |
|      - | 2140 | `	const char *z1,*z2,*zFunc;` |
|      - | 2141 | `	int n1,n2,bFold;` |
|     21 | 2142 | `	if( nArg < 2 ){` |
|    ! 0 | 2143 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2144 | `		return PH7_OK;` |
|      - | 2145 | `	}` |
|     21 | 2146 | `	zFunc = ph7_function_name(pCtx);` |
|     21 | 2147 | `	bFold = zFunc[sizeof("strnat")-1] == 'c'; /* strnatCasecmp */` |
|     21 | 2148 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     21 | 2149 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     21 | 2150 | `	ph7_result_int(pCtx,PH7_StrNatCmp(z1,n1,z2,n2,bFold));` |
|     21 | 2151 | `	return PH7_OK;` |
|     11 | 2152 | `}` |
|      - | 2153 | `/*` |
|      - | 2154 | ` * int strncmp(string $str1,string $str2,int n)` |
|      - | 2155 | ` *  Perform a binary safe string comparison of the first n characters.` |
|      - | 2156 | ` * Parameter` |
|      - | 2157 | ` *  str1: The first string` |
|      - | 2158 | ` *  str2: The second string` |
|      - | 2159 | ` * Return` |
|      - | 2160 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2161 | ` *  than str2, and 0 if they are equal.` |
|      - | 2162 | ` */` |
|    366 | 2163 | `static int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2164 | `{` |
|      - | 2165 | `	const char *z1,*z2;` |
|      - | 2166 | `	int res;` |
|      - | 2167 | `	int n;` |
|    368 | 2168 | `	if( nArg < 3 ){` |
|      - | 2169 | `		/* Perform a standard comparison */` |
|    ! 0 | 2170 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|      - | 2171 | `	}` |
|      - | 2172 | `	/* Desired comparison length */` |
|    368 | 2173 | `	n  = ph7_value_to_int(apArg[2]);` |
|    368 | 2174 | `	if( n < 0 ){` |
|      - | 2175 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|      4 | 2176 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2177 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|      1 | 2178 | `			ph7_function_name(pCtx));` |
|      - | 2179 | `	}` |
|      - | 2180 | `	/* Perform the comparison */` |
|    366 | 2181 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|    366 | 2182 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|    366 | 2183 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|      - | 2184 | `	/* Comparison result */` |
|    366 | 2185 | `	ph7_result_int(pCtx,res);` |
|    366 | 2186 | `	return PH7_OK;` |
|    185 | 2187 | `}` |
|      - | 2188 | `/*` |
|      - | 2189 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|      - | 2190 | ` *  Perform a binary safe case-insensitive string comparison.` |
|      - | 2191 | ` * Parameter` |
|      - | 2192 | ` *  str1: The first string` |
|      - | 2193 | ` *  str2: The second string` |
|      - | 2194 | ` * Return` |
|      - | 2195 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2196 | ` *  than str2, and 0 if they are equal.` |
|      - | 2197 | ` */` |
|    152 | 2198 | `static int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2199 | `{` |
|      - | 2200 | `	const char *z1,*z2;` |
|      - | 2201 | `	int n1,n2;` |
|      - | 2202 | `	int res;` |
|    153 | 2203 | `	if( nArg < 2 ){` |
|    ! 0 | 2204 | `		res = nArg == 0 ? 0 : 1;` |
|    ! 0 | 2205 | `		ph7_result_int(pCtx,res);` |
|    ! 0 | 2206 | `		return PH7_OK;` |
|      - | 2207 | `	}` |
|      - | 2208 | `	/* Perform the comparison */` |
|    153 | 2209 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|    153 | 2210 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|    153 | 2211 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2212 | `	/* Comparison result */` |
|    153 | 2213 | `	ph7_result_int(pCtx,res);` |
|    153 | 2214 | `	return PH7_OK;` |
|     77 | 2215 | `}` |
|      - | 2216 | `/*` |
|      - | 2217 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|      - | 2218 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|      - | 2219 | ` * Parameter` |
|      - | 2220 | ` *  $str1: The first string` |
|      - | 2221 | ` *  $str2: The second string` |
|      - | 2222 | ` *  $len:  The length of strings to be used in the comparison.` |
|      - | 2223 | ` * Return` |
|      - | 2224 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2225 | ` *  than str2, and 0 if they are equal.` |
|      - | 2226 | ` */` |
|     40 | 2227 | `static int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2228 | `{` |
|      - | 2229 | `	const char *z1,*z2;` |
|      - | 2230 | `	int res;` |
|      - | 2231 | `	int n;` |
|     45 | 2232 | `	if( nArg < 3 ){` |
|      - | 2233 | `		/* Perform a standard comparison */` |
|    ! 0 | 2234 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 2235 | `	}` |
|      - | 2236 | `	/* Desired comparison length */` |
|     45 | 2237 | `	n  = ph7_value_to_int(apArg[2]);` |
|     45 | 2238 | `	if( n < 0 ){` |
|      - | 2239 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|      4 | 2240 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2241 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|      1 | 2242 | `			ph7_function_name(pCtx));` |
|      - | 2243 | `	}` |
|      - | 2244 | `	/* Perform the comparison */` |
|     43 | 2245 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     43 | 2246 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     43 | 2247 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 2248 | `	/* Comparison result */` |
|     43 | 2249 | `	ph7_result_int(pCtx,res);` |
|     43 | 2250 | `	return PH7_OK;` |
|     25 | 2251 | `}` |
|      - | 2252 | `/*` |
|      - | 2253 | ` * Implode context [i.e: it's private data].` |
|      - | 2254 | ` * A pointer to the following structure is forwarded` |
|      - | 2255 | ` * verbatim to the array walker callback defined below.` |
|      - | 2256 | ` */` |
|      - | 2257 | `struct implode_data {` |
|      - | 2258 | `	ph7_context *pCtx;    /* Call context */` |
|      - | 2259 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|      - | 2260 | `	const char *zSep;     /* Arguments separator if any */` |
|      - | 2261 | `	int nSeplen;          /* Separator length */` |
|      - | 2262 | `	int bFirst;           /* TRUE if first call */` |
|      - | 2263 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|      - | 2264 | `	sxi32 rc;             /* Captured allocation rc; SXERR_MEM => the builtin raises an OOM fatal */` |
|      - | 2265 | `};` |
|      - | 2266 | `/*` |
|      - | 2267 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 2268 | ` * The following routine is invoked for each array entry passed` |
|      - | 2269 | ` * to the implode() function.` |
|      - | 2270 | ` */` |
| 156856 | 2271 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      5 | 2272 | `{` |
|  78428 | 2273 | `	SXUNUSED(pKey);` |
| 156861 | 2274 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 2275 | `	const char *zData;` |
|      - | 2276 | `	int nLen;` |
| 156861 | 2277 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 2278 | `		if( pData->nSeplen > 0 ){` |
|      3 | 2279 | `			if( !pData->bFirst ){` |
|      - | 2280 | `				/* append the separator first */` |
|      3 | 2281 | `				if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 2282 | `					pData->rc = SXERR_MEM;` |
|    ! 0 | 2283 | `					return PH7_ABORT;` |
|      - | 2284 | `				}` |
|      2 | 2285 | `			}else{` |
|    ! 0 | 2286 | `				pData->bFirst = 0;` |
|      - | 2287 | `			}` |
|      1 | 2288 | `		}` |
|      - | 2289 | `		/* Recurse */` |
|      3 | 2290 | `		pData->bFirst = 1;` |
|      3 | 2291 | `		pData->nRecCount++;` |
|      3 | 2292 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 2293 | `		pData->nRecCount--;` |
|      - | 2294 | `		/* Propagate an allocation failure surfaced deeper in the recursion. */` |
|      3 | 2295 | `		if( pData->rc != SXRET_OK ){` |
|    ! 0 | 2296 | `			return PH7_ABORT;` |
|      - | 2297 | `		}` |
|      3 | 2298 | `		return PH7_OK;` |
|      - | 2299 | `	}` |
|      - | 2300 | `	/* Extract the string representation of the entry value */` |
| 156859 | 2301 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 2302 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 156859 | 2303 | `	if( pData->bFirst ){` |
|  34033 | 2304 | `		pData->bFirst = 0;` |
| 139845 | 2305 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 2306 | `		/* append the separator first */` |
| 122751 | 2307 | `		if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 2308 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 2309 | `			return PH7_ABORT;` |
|      - | 2310 | `		}` |
|  61373 | 2311 | `	}` |
|      - | 2312 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 156859 | 2313 | `	if( nLen > 0 ){` |
| 144043 | 2314 | `		if( ph7_result_string(pData->pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2315 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 2316 | `			return PH7_ABORT;` |
|      - | 2317 | `		}` |
|  72019 | 2318 | `	}` |
| 156859 | 2319 | `	return PH7_OK;` |
|  78433 | 2320 | `}` |
|      - | 2321 | `/*` |
|      - | 2322 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 2323 | ` * string implode(array $pieces,...)` |
|      - | 2324 | ` *  Join array elements with a string.` |
|      - | 2325 | ` * $glue` |
|      - | 2326 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 2327 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 2328 | ` * $pieces` |
|      - | 2329 | ` *   The array of strings to implode.` |
|      - | 2330 | ` * Return` |
|      - | 2331 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 2332 | ` *  order, with the glue string between each element.` |
|      - | 2333 | ` */` |
|  34056 | 2334 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2335 | `{` |
|      - | 2336 | `	struct implode_data imp_data;` |
|  34061 | 2337 | `	int i = 1;` |
|  34061 | 2338 | `	if( nArg < 1 ){` |
|      - | 2339 | `		/* Missing argument,return NULL */` |
|    ! 0 | 2340 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2341 | `		return PH7_OK;` |
|      - | 2342 | `	}` |
|      - | 2343 | `	/* Prepare the implode context */` |
|  34061 | 2344 | `	imp_data.pCtx = pCtx;` |
|  34061 | 2345 | `	imp_data.bRecursive = 0;` |
|  34061 | 2346 | `	imp_data.bFirst = 1;` |
|  34061 | 2347 | `	imp_data.nRecCount = 0;` |
|  34061 | 2348 | `	imp_data.rc = SXRET_OK;` |
|  34061 | 2349 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  34059 | 2350 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  34059 | 2351 | `		if( nArg > 1 && !ph7_value_is_array(apArg[1]) && !ph7_value_is_null(apArg[1]) ){` |
|      - | 2352 | `			/* php: implode($glue, $pieces) requires an ARRAY. PH7 stringified whatever it` |
|      - | 2353 | `			 * was handed, so implode(",", 5) quietly returned "5". */` |
|      - | 2354 | `			char zBuf[64];` |
|      4 | 2355 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 2356 | `				"implode(): Argument #2 ($array) must be of type ?array, %s given",` |
|      2 | 2357 | `				VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 2358 | `		}` |
|  17031 | 2359 | `	}else{` |
|      3 | 2360 | `		imp_data.zSep = 0;` |
|      3 | 2361 | `		imp_data.nSeplen = 0;` |
|      3 | 2362 | `		i = 0;` |
|      - | 2363 | `	}` |
|  34059 | 2364 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 2365 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2366 | `	}` |
|      - | 2367 | `	/* Start the 'join' process */` |
|  68113 | 2368 | `	while( i < nArg ){` |
|  34059 | 2369 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2370 | `			/* Iterate throw array entries */` |
|  34059 | 2371 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 2372 | `			/* Surface a callback allocation failure as a fatal */` |
|  34059 | 2373 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 2374 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2375 | `			}` |
|  17032 | 2376 | `		}else{` |
|      - | 2377 | `			const char *zData;` |
|      - | 2378 | `			int nLen;` |
|      - | 2379 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 2380 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2381 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 2382 | `			if( imp_data.bFirst ){` |
|    ! 0 | 2383 | `				imp_data.bFirst = 0;` |
|    ! 0 | 2384 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2385 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 2386 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2387 | `				}` |
|    ! 0 | 2388 | `			}` |
|      - | 2389 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 2390 | `			if( nLen > 0 ){` |
|    ! 0 | 2391 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2392 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2393 | `				}` |
|    ! 0 | 2394 | `			}` |
|      - | 2395 | `		}` |
|  34059 | 2396 | `		i++;` |
|      5 | 2397 | `	}` |
|  34059 | 2398 | `	return PH7_OK;` |
|  17033 | 2399 | `}` |
|      - | 2400 | `/*` |
|      - | 2401 | ` * Symisc eXtension:` |
|      - | 2402 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 2403 | ` * Purpose` |
|      - | 2404 | ` *  Same as implode() but recurse on arrays.` |
|      - | 2405 | ` * Example:` |
|      - | 2406 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 2407 | ` *   echo implode_recursive("/",$a);` |
|      - | 2408 | ` *   Will output` |
|      - | 2409 | ` *     usr/home/dean.` |
|      - | 2410 | ` *   While the standard implode would produce.` |
|      - | 2411 | ` *    usr/Array.` |
|      - | 2412 | ` * Parameter` |
|      - | 2413 | ` *  Refer to implode().` |
|      - | 2414 | ` * Return` |
|      - | 2415 | ` *  Refer to implode().` |
|      - | 2416 | ` */` |
|     12 | 2417 | `static int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2418 | `{` |
|      - | 2419 | `	struct implode_data imp_data;` |
|     13 | 2420 | `	int i = 1;` |
|     13 | 2421 | `	if( nArg < 1 ){` |
|      - | 2422 | `		/* Missing argument,return NULL */` |
|      3 | 2423 | `		ph7_result_null(pCtx);` |
|      3 | 2424 | `		return PH7_OK;` |
|      - | 2425 | `	}` |
|      - | 2426 | `	/* Prepare the implode context */` |
|     11 | 2427 | `	imp_data.pCtx = pCtx;` |
|     11 | 2428 | `	imp_data.bRecursive = 1;` |
|     11 | 2429 | `	imp_data.bFirst = 1;` |
|     11 | 2430 | `	imp_data.nRecCount = 0;` |
|     11 | 2431 | `	imp_data.rc = SXRET_OK;` |
|     11 | 2432 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 2433 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 2434 | `	}else{` |
|    ! 0 | 2435 | `		imp_data.zSep = 0;` |
|    ! 0 | 2436 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 2437 | `		i = 0;` |
|      - | 2438 | `	}` |
|     11 | 2439 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 2440 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2441 | `	}` |
|      - | 2442 | `	/* Start the 'join' process */` |
|     21 | 2443 | `	while( i < nArg ){` |
|     11 | 2444 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2445 | `			/* Iterate throw array entries */` |
|      3 | 2446 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 2447 | `			/* Surface a callback allocation failure as a fatal */` |
|      3 | 2448 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 2449 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2450 | `			}` |
|      2 | 2451 | `		}else{` |
|      - | 2452 | `			const char *zData;` |
|      - | 2453 | `			int nLen;` |
|      - | 2454 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 2455 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2456 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 2457 | `			if( imp_data.bFirst ){` |
|      9 | 2458 | `				imp_data.bFirst = 0;` |
|      4 | 2459 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2460 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 2461 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2462 | `				}` |
|    ! 0 | 2463 | `			}` |
|      - | 2464 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 2465 | `			if( nLen > 0 ){` |
|      9 | 2466 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2467 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2468 | `				}` |
|      4 | 2469 | `			}` |
|      - | 2470 | `		}` |
|     11 | 2471 | `		i++;` |
|      1 | 2472 | `	}` |
|     11 | 2473 | `	return PH7_OK;` |
|      7 | 2474 | `}` |
|      - | 2475 | `/*` |
|      - | 2476 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 2477 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 2478 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 2479 | ` * Parameters` |
|      - | 2480 | ` *  $delimiter` |
|      - | 2481 | ` *   The boundary string.` |
|      - | 2482 | ` * $string` |
|      - | 2483 | ` *   The input string.` |
|      - | 2484 | ` * $limit` |
|      - | 2485 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 2486 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 2487 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 2488 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 2489 | ` * Returns` |
|      - | 2490 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 2491 | ` *  on boundaries formed by the delimiter.` |
|      - | 2492 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 2493 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 2494 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 2495 | ` *  will be returned.` |
|      - | 2496 | ` * NOTE:` |
|      - | 2497 | ` *  Negative limit is not supported.` |
|      - | 2498 | ` */` |
|   6954 | 2499 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2500 | `{` |
|      - | 2501 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 2502 | `	int nDelim,nStrlen,iLimit;` |
|      - | 2503 | `	ph7_value *pArray;` |
|      - | 2504 | `	ph7_value *pValue;` |
|      - | 2505 | `	sxu32 nOfft;` |
|      - | 2506 | `	sxi32 rc;` |
|   6959 | 2507 | `	if( nArg < 2 ){` |
|      - | 2508 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2509 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2510 | `		return PH7_OK;` |
|      - | 2511 | `	}` |
|      - | 2512 | `	/* Extract the delimiter */` |
|   6959 | 2513 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   6959 | 2514 | `	if( nDelim < 1 ){` |
|      - | 2515 | `		/* Empty delimiter: PHP 8 throws a catchable ValueError. */` |
|      5 | 2516 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2517 | `			"explode(): Argument #1 ($separator) must not be empty");` |
|      - | 2518 | `	}` |
|      - | 2519 | `	/* Extract the string */` |
|   6955 | 2520 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   6955 | 2521 | `	if( nStrlen < 1 ){` |
|      - | 2522 | `		/* Empty string: normally an array with a single empty element (PHP behavior).` |
|      - | 2523 | `		 * A negative limit drops the last -limit components, so the sole empty` |
|      - | 2524 | `		 * component is dropped and the result is an empty array. */` |
|     13 | 2525 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|     13 | 2526 | `		if( pArrayTmp == 0 ){` |
|      - | 2527 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 2528 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2529 | `			return PH7_OK;` |
|      - | 2530 | `		}` |
|     13 | 2531 | `		if( !(nArg > 2 && ph7_value_to_int(apArg[2]) < 0) ){` |
|     11 | 2532 | `			ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|     11 | 2533 | `			if( pValueTmp == 0 ){` |
|      - | 2534 | `				/* Out of memory,return FALSE */` |
|    ! 0 | 2535 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 2536 | `				return PH7_OK;` |
|      - | 2537 | `			}` |
|     11 | 2538 | `			ph7_value_string(pValueTmp, "", 0);` |
|     11 | 2539 | `			if( ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp) != SXRET_OK ){` |
|    ! 0 | 2540 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2541 | `			}` |
|      5 | 2542 | `		}` |
|     13 | 2543 | `		ph7_result_value(pCtx, pArrayTmp);` |
|     13 | 2544 | `		return PH7_OK;` |
|      - | 2545 | `	}` |
|      - | 2546 | `	/* Point to the end of the string */` |
|   6943 | 2547 | `	zEnd = &zString[nStrlen];` |
|      - | 2548 | `	/* Create the array */` |
|   6943 | 2549 | `	pArray =  ph7_context_new_array(pCtx);` |
|   6943 | 2550 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   6943 | 2551 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 2552 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 2553 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2554 | `		return PH7_OK;` |
|      - | 2555 | `	}` |
|      - | 2556 | `	/* Set a defualt limit */` |
|   6943 | 2557 | `	iLimit = SXI32_HIGH;` |
|   6943 | 2558 | `	if( nArg > 2 ){` |
|     40 | 2559 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|     40 | 2560 | `		if( iLimit < 0 ){` |
|      - | 2561 | `			/* Negative limit: keep all components except the last -iLimit (PHP).` |
|      - | 2562 | `			 * Pre-count the components (delimiters + 1), then emit only the first` |
|      - | 2563 | `			 * nKeep CLEAN components — no trailing-remainder merge (the difference` |
|      - | 2564 | `			 * from the positive path). nKeep <= 0 drops everything -> empty array. */` |
|     17 | 2565 | `			int nTotal = 1,nKeep;` |
|     17 | 2566 | `			const char *zScan = zString;` |
|      - | 2567 | `			sxu32 nScanOfft;` |
|     57 | 2568 | `			while( SyBlobSearch(zScan,(sxu32)(zEnd - zScan),zDelim,nDelim,&nScanOfft) == SXRET_OK ){` |
|     41 | 2569 | `				nTotal++;` |
|     41 | 2570 | `				zScan = &zScan[nScanOfft + nDelim];` |
|      1 | 2571 | `			}` |
|     17 | 2572 | `			nKeep = nTotal + iLimit; /* iLimit < 0, so this is nTotal - (-iLimit) */` |
|     49 | 2573 | `			while( nKeep > (int)ph7_array_count(pArray)` |
|     39 | 2574 | `				&& SyBlobSearch(zString,(sxu32)(zEnd - zString),zDelim,nDelim,&nOfft) == SXRET_OK ){` |
|      - | 2575 | `				/* Emit the next clean component */` |
|     23 | 2576 | `				zCur = &zString[nOfft];` |
|     23 | 2577 | `				ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|     23 | 2578 | `				if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2579 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2580 | `				}` |
|     23 | 2581 | `				zString = &zCur[nDelim];` |
|     23 | 2582 | `				ph7_value_reset_string_cursor(pValue);` |
|      1 | 2583 | `			}` |
|     17 | 2584 | `			ph7_result_value(pCtx,pArray);` |
|     17 | 2585 | `			return PH7_OK;` |
|      - | 2586 | `		}` |
|     24 | 2587 | `		if( iLimit == 0 ){` |
|      5 | 2588 | `			iLimit = 1;` |
|      2 | 2589 | `		}` |
|     24 | 2590 | `		iLimit--;` |
|     10 | 2591 | `	}` |
|      - | 2592 | `	/* Start exploding */` |
|  83472 | 2593 | `	for(;;){` |
| 166949 | 2594 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 166949 | 2595 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 2596 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   6927 | 2597 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   6927 | 2598 | `			if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2599 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2600 | `			}` |
|   6927 | 2601 | `			break;` |
|      - | 2602 | `		}` |
|      - | 2603 | `		/* Point to the desired offset */` |
| 160027 | 2604 | `		zCur = &zString[nOfft];` |
|      - | 2605 | `		/* Perform the store operation (may be empty) */` |
| 160027 | 2606 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 160027 | 2607 | `		if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2608 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 2609 | `		}` |
|      - | 2610 | `		/* Point beyond the delimiter */` |
| 160027 | 2611 | `		zString = &zCur[nDelim];` |
|      - | 2612 | `		/* Reset the cursor */` |
| 160027 | 2613 | `		ph7_value_reset_string_cursor(pValue);` |
|      5 | 2614 | `	}` |
|      - | 2615 | `	/* Return the freshly created array */` |
|   6927 | 2616 | `	ph7_result_value(pCtx,pArray);` |
|      - | 2617 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 2618 | `	 * released as soon we return from this foregin function.` |
|      - | 2619 | `	 */` |
|   6927 | 2620 | `	return PH7_OK;` |
|   3482 | 2621 | `}` |
|      - | 2622 | `/*` |
|      - | 2623 | ` * string trim(string $str[,string $charlist ])` |
|      - | 2624 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2625 | ` * Parameters` |
|      - | 2626 | ` *  $str` |
|      - | 2627 | ` *   The string that will be trimmed.` |
|      - | 2628 | ` * $charlist` |
|      - | 2629 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2630 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2631 | ` *   With .. you can specify a range of characters.` |
|      - | 2632 | ` * Returns.` |
|      - | 2633 | ` *  Thr processed string.` |
|      - | 2634 | ` * NOTE:` |
|      - | 2635 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2636 | ` */` |
|  14664 | 2637 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2638 | `{` |
|  14669 | 2639 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"trim",1,"$string"); }` |
|      - | 2640 | `	const char *zString;` |
|      - | 2641 | `	int nLen;` |
|  14669 | 2642 | `	if( nArg < 1 ){` |
|      - | 2643 | `		/* Missing arguments,return null */` |
|    ! 0 | 2644 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2645 | `		return PH7_OK;` |
|      - | 2646 | `	}` |
|      - | 2647 | `	/* Extract the target string */` |
|  14669 | 2648 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  14669 | 2649 | `	if( nLen < 1 ){` |
|      - | 2650 | `		/* Empty string,return */` |
|    753 | 2651 | `		ph7_result_string(pCtx,"",0);` |
|    753 | 2652 | `		return PH7_OK;` |
|      - | 2653 | `	}` |
|      - | 2654 | `	/* Start the trim process */` |
|  13921 | 2655 | `	if( nArg < 2 ){` |
|      - | 2656 | `		SyString sStr;` |
|      - | 2657 | `		/* Remove white spaces and NUL bytes */` |
|  13891 | 2658 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  34867 | 2659 | `		SyStringFullTrimSafe(&sStr);` |
|  13891 | 2660 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   6948 | 2661 | `	}else{` |
|      - | 2662 | `		/* Char list */` |
|      - | 2663 | `		const char *zList;` |
|      - | 2664 | `		int nListlen;` |
|     33 | 2665 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     33 | 2666 | `		if( nListlen < 1 ){` |
|      - | 2667 | `			/* Return the string unchanged */` |
|      6 | 2668 | `			ph7_result_string(pCtx,zString,nLen);` |
|      4 | 2669 | `		}else{` |
|      - | 2670 | `			char aMask[256];` |
|     29 | 2671 | `			const char *zEnd = &zString[nLen];` |
|     29 | 2672 | `			const char *zCur = zString;` |
|     29 | 2673 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2674 | `			/* Left trim */` |
|     79 | 2675 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     53 | 2676 | `				zCur++;` |
|      3 | 2677 | `			}` |
|      - | 2678 | `			/* Right trim */` |
|     79 | 2679 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     53 | 2680 | `				zEnd--;` |
|      3 | 2681 | `			}` |
|     29 | 2682 | `			if( zCur >= zEnd ){` |
|      - | 2683 | `				/* Return the empty string */` |
|    ! 0 | 2684 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2685 | `			}else{` |
|     29 | 2686 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2687 | `			}` |
|      - | 2688 | `		}` |
|      - | 2689 | `	}` |
|  13921 | 2690 | `	return PH7_OK;` |
|   7337 | 2691 | `}` |
|      - | 2692 | `/*` |
|      - | 2693 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 2694 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 2695 | ` * Parameters` |
|      - | 2696 | ` *  $str` |
|      - | 2697 | ` *   The string that will be trimmed.` |
|      - | 2698 | ` * $charlist` |
|      - | 2699 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2700 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2701 | ` *   With .. you can specify a range of characters.` |
|      - | 2702 | ` * Returns.` |
|      - | 2703 | ` *  Thr processed string.` |
|      - | 2704 | ` * NOTE:` |
|      - | 2705 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2706 | ` */` |
|    168 | 2707 | `static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2708 | `{` |
|    171 | 2709 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"rtrim",1,"$string"); }` |
|      - | 2710 | `	const char *zString;` |
|      - | 2711 | `	int nLen;` |
|    171 | 2712 | `	if( nArg < 1 ){` |
|      - | 2713 | `		/* Missing arguments,return null */` |
|    ! 0 | 2714 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2715 | `		return PH7_OK;` |
|      - | 2716 | `	}` |
|      - | 2717 | `	/* Extract the target string */` |
|    171 | 2718 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    171 | 2719 | `	if( nLen < 1 ){` |
|      - | 2720 | `		/* Empty string,return */` |
|      7 | 2721 | `		ph7_result_string(pCtx,"",0);` |
|      7 | 2722 | `		return PH7_OK;` |
|      - | 2723 | `	}` |
|      - | 2724 | `	/* Start the trim process */` |
|    165 | 2725 | `	if( nArg < 2 ){` |
|      - | 2726 | `		SyString sStr;` |
|      - | 2727 | `		/* Remove white spaces and NUL bytes*/` |
|     19 | 2728 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     48 | 2729 | `		SyStringRightTrimSafe(&sStr);` |
|     19 | 2730 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|     10 | 2731 | `	}else{` |
|      - | 2732 | `		/* Char list */` |
|      - | 2733 | `		const char *zList;` |
|      - | 2734 | `		int nListlen;` |
|    147 | 2735 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|    147 | 2736 | `		if( nListlen < 1 ){` |
|      - | 2737 | `			/* Return the string unchanged */` |
|    ! 0 | 2738 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 2739 | `		}else{` |
|      - | 2740 | `			char aMask[256];` |
|    147 | 2741 | `			const char *zEnd = &zString[nLen];` |
|    147 | 2742 | `			const char *zCur = zString;` |
|    147 | 2743 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2744 | `			/* Right trim */` |
|    165 | 2745 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     20 | 2746 | `				zEnd--;` |
|      2 | 2747 | `			}` |
|    147 | 2748 | `			if( zEnd <= zCur ){` |
|      - | 2749 | `				/* Return the empty string */` |
|    ! 0 | 2750 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2751 | `			}else{` |
|    147 | 2752 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2753 | `			}` |
|      - | 2754 | `		}` |
|      - | 2755 | `	}` |
|    165 | 2756 | `	return PH7_OK;` |
|     87 | 2757 | `}` |
|      - | 2758 | `/*` |
|      - | 2759 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 2760 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2761 | ` * Parameters` |
|      - | 2762 | ` *  $str` |
|      - | 2763 | ` *   The string that will be trimmed.` |
|      - | 2764 | ` * $charlist` |
|      - | 2765 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2766 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2767 | ` *   With .. you can specify a range of characters.` |
|      - | 2768 | ` * Returns.` |
|      - | 2769 | ` *  Thr processed string.` |
|      - | 2770 | ` * NOTE:` |
|      - | 2771 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2772 | ` */` |
|     42 | 2773 | `static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2774 | `{` |
|     47 | 2775 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"ltrim",1,"$string"); }` |
|      - | 2776 | `	const char *zString;` |
|      - | 2777 | `	int nLen;` |
|     47 | 2778 | `	if( nArg < 1 ){` |
|      - | 2779 | `		/* Missing arguments,return null */` |
|    ! 0 | 2780 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2781 | `		return PH7_OK;` |
|      - | 2782 | `	}` |
|      - | 2783 | `	/* Extract the target string */` |
|     47 | 2784 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     47 | 2785 | `	if( nLen < 1 ){` |
|      - | 2786 | `		/* Empty string,return */` |
|     23 | 2787 | `		ph7_result_string(pCtx,"",0);` |
|     23 | 2788 | `		return PH7_OK;` |
|      - | 2789 | `	}` |
|      - | 2790 | `	/* Start the trim process */` |
|     28 | 2791 | `	if( nArg < 2 ){` |
|      - | 2792 | `		SyString sStr;` |
|      - | 2793 | `		/* Remove white spaces and NUL byte */` |
|      5 | 2794 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     13 | 2795 | `		SyStringLeftTrimSafe(&sStr);` |
|      5 | 2796 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      3 | 2797 | `	}else{` |
|      - | 2798 | `		/* Char list */` |
|      - | 2799 | `		const char *zList;` |
|      - | 2800 | `		int nListlen;` |
|     24 | 2801 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     24 | 2802 | `		if( nListlen < 1 ){` |
|      - | 2803 | `			/* Return the string unchanged */` |
|      3 | 2804 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2805 | `		}else{` |
|      - | 2806 | `			char aMask[256];` |
|     22 | 2807 | `			const char *zEnd = &zString[nLen];` |
|     22 | 2808 | `			const char *zCur = zString;` |
|     22 | 2809 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2810 | `			/* Left trim */` |
|     56 | 2811 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     38 | 2812 | `				zCur++;` |
|      4 | 2813 | `			}` |
|     22 | 2814 | `			if( zCur >= zEnd ){` |
|      - | 2815 | `				/* Return the empty string */` |
|    ! 0 | 2816 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2817 | `			}else{` |
|     22 | 2818 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2819 | `			}` |
|      - | 2820 | `		}` |
|      - | 2821 | `	}` |
|     28 | 2822 | `	return PH7_OK;` |
|     26 | 2823 | `}` |
|      - | 2824 | `/*` |
|      - | 2825 | ` * string strtolower(string $str)` |
|      - | 2826 | ` *  Make a string lowercase.` |
|      - | 2827 | ` * Parameters` |
|      - | 2828 | ` *  $str` |
|      - | 2829 | ` *   The input string.` |
|      - | 2830 | ` * Returns.` |
|      - | 2831 | ` *  The lowercased string.` |
|      - | 2832 | ` */` |
|  33944 | 2833 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2834 | `{` |
|  33949 | 2835 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtolower",1,"$string"); }` |
|      - | 2836 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2837 | `	int nLen;` |
|  33949 | 2838 | `	if( nArg < 1 ){` |
|      - | 2839 | `		/* Missing arguments,return null */` |
|    ! 0 | 2840 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2841 | `		return PH7_OK;` |
|      - | 2842 | `	}` |
|      - | 2843 | `	/* Extract the target string */` |
|  33949 | 2844 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  33949 | 2845 | `	if( nLen < 1 ){` |
|      - | 2846 | `		/* Empty string,return */` |
|      5 | 2847 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 2848 | `		return PH7_OK;` |
|      - | 2849 | `	}` |
|      - | 2850 | `	/* Perform the requested operation */` |
|  33945 | 2851 | `	zEnd = &zString[nLen];` |
| 107090 | 2852 | `	for(;;){` |
| 214185 | 2853 | `		if( zString >= zEnd ){` |
|      - | 2854 | `			/* No more input,break immediately */` |
|  33945 | 2855 | `			break;` |
|      - | 2856 | `		}` |
| 180245 | 2857 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2858 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2859 | `			zCur = zString;` |
|    ! 0 | 2860 | `			zString++;` |
|    ! 0 | 2861 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2862 | `				zString++;` |
|    ! 0 | 2863 | `			}` |
|      - | 2864 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2865 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2866 | `		}else{` |
| 180245 | 2867 | `			int c = zString[0];` |
| 180245 | 2868 | `			if( SyisUpper(c) ){` |
| 177361 | 2869 | `				c = SyToLower(zString[0]);` |
|  88678 | 2870 | `			}` |
|      - | 2871 | `			/* Append character */` |
| 180245 | 2872 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2873 | `			/* Advance the cursor */` |
| 180245 | 2874 | `			zString++;` |
|      - | 2875 | `		}` |
|      5 | 2876 | `	}` |
|  33945 | 2877 | `	return PH7_OK;` |
|  16977 | 2878 | `}` |
|      - | 2879 | `/*` |
|      - | 2880 | ` * string strtolower(string $str)` |
|      - | 2881 | ` *  Make a string uppercase.` |
|      - | 2882 | ` * Parameters` |
|      - | 2883 | ` *  $str` |
|      - | 2884 | ` *   The input string.` |
|      - | 2885 | ` * Returns.` |
|      - | 2886 | ` *  The uppercased string.` |
|      - | 2887 | ` */` |
|     76 | 2888 | `static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2889 | `{` |
|     80 | 2890 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtoupper",1,"$string"); }` |
|      - | 2891 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2892 | `	int nLen;` |
|     80 | 2893 | `	if( nArg < 1 ){` |
|      - | 2894 | `		/* Missing arguments,return null */` |
|    ! 0 | 2895 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2896 | `		return PH7_OK;` |
|      - | 2897 | `	}` |
|      - | 2898 | `	/* Extract the target string */` |
|     80 | 2899 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     80 | 2900 | `	if( nLen < 1 ){` |
|      - | 2901 | `		/* Empty string,return */` |
|      5 | 2902 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 2903 | `		return PH7_OK;` |
|      - | 2904 | `	}` |
|      - | 2905 | `	/* Perform the requested operation */` |
|     76 | 2906 | `	zEnd = &zString[nLen];` |
|    148 | 2907 | `	for(;;){` |
|    300 | 2908 | `		if( zString >= zEnd ){` |
|      - | 2909 | `			/* No more input,break immediately */` |
|     76 | 2910 | `			break;` |
|      - | 2911 | `		}` |
|    228 | 2912 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2913 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2914 | `			zCur = zString;` |
|    ! 0 | 2915 | `			zString++;` |
|    ! 0 | 2916 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2917 | `				zString++;` |
|    ! 0 | 2918 | `			}` |
|      - | 2919 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2920 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2921 | `		}else{` |
|    228 | 2922 | `			int c = zString[0];` |
|    228 | 2923 | `			if( SyisLower(c) ){` |
|    212 | 2924 | `				c = SyToUpper(zString[0]);` |
|    104 | 2925 | `			}` |
|      - | 2926 | `			/* Append character */` |
|    228 | 2927 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2928 | `			/* Advance the cursor */` |
|    228 | 2929 | `			zString++;` |
|      - | 2930 | `		}` |
|      4 | 2931 | `	}` |
|     76 | 2932 | `	return PH7_OK;` |
|     42 | 2933 | `}` |
|      - | 2934 | `/*` |
|      - | 2935 | ` * string ucfirst(string $str)` |
|      - | 2936 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 2937 | ` *  character is alphabetic.` |
|      - | 2938 | ` * Parameters` |
|      - | 2939 | ` *  $str` |
|      - | 2940 | ` *   The input string.` |
|      - | 2941 | ` * Returns.` |
|      - | 2942 | ` *  The processed string.` |
|      - | 2943 | ` */` |
|      4 | 2944 | `static int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2945 | `{` |
|      - | 2946 | `	const char *zString,*zEnd;` |
|      - | 2947 | `	int nLen,c;` |
|      5 | 2948 | `	if( nArg < 1 ){` |
|      - | 2949 | `		/* Missing arguments,return null */` |
|    ! 0 | 2950 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2951 | `		return PH7_OK;` |
|      - | 2952 | `	}` |
|      - | 2953 | `	/* Extract the target string */` |
|      5 | 2954 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2955 | `	if( nLen < 1 ){` |
|      - | 2956 | `		/* Empty string,return */` |
|      3 | 2957 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2958 | `		return PH7_OK;` |
|      - | 2959 | `	}` |
|      - | 2960 | `	/* Perform the requested operation */` |
|      3 | 2961 | `	zEnd = &zString[nLen];` |
|      3 | 2962 | `	c = zString[0];` |
|      3 | 2963 | `	if( SyisLower(c) ){` |
|      3 | 2964 | `		c = SyToUpper(c);` |
|      1 | 2965 | `	}` |
|      - | 2966 | `	/* Append the first character */` |
|      3 | 2967 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2968 | `	zString++;` |
|      3 | 2969 | `	if( zString < zEnd ){` |
|      - | 2970 | `		/* Append the rest of the input verbatim */` |
|      3 | 2971 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2972 | `	}` |
|      3 | 2973 | `	return PH7_OK;` |
|      3 | 2974 | `}` |
|      - | 2975 | `/*` |
|      - | 2976 | ` * string lcfirst(string $str)` |
|      - | 2977 | ` *  Make a string's first character lowercase.` |
|      - | 2978 | ` * Parameters` |
|      - | 2979 | ` *  $str` |
|      - | 2980 | ` *   The input string.` |
|      - | 2981 | ` * Returns.` |
|      - | 2982 | ` *  The processed string.` |
|      - | 2983 | ` */` |
|      4 | 2984 | `static int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2985 | `{` |
|      - | 2986 | `	const char *zString,*zEnd;` |
|      - | 2987 | `	int nLen,c;` |
|      5 | 2988 | `	if( nArg < 1 ){` |
|      - | 2989 | `		/* Missing arguments,return null */` |
|    ! 0 | 2990 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2991 | `		return PH7_OK;` |
|      - | 2992 | `	}` |
|      - | 2993 | `	/* Extract the target string */` |
|      5 | 2994 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2995 | `	if( nLen < 1 ){` |
|      - | 2996 | `		/* Empty string,return */` |
|      3 | 2997 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2998 | `		return PH7_OK;` |
|      - | 2999 | `	}` |
|      - | 3000 | `	/* Perform the requested operation */` |
|      3 | 3001 | `	zEnd = &zString[nLen];` |
|      3 | 3002 | `	c = zString[0];` |
|      3 | 3003 | `	if( SyisUpper(c) ){` |
|      3 | 3004 | `		c = SyToLower(c);` |
|      1 | 3005 | `	}` |
|      - | 3006 | `	/* Append the first character */` |
|      3 | 3007 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 3008 | `	zString++;` |
|      3 | 3009 | `	if( zString < zEnd ){` |
|      - | 3010 | `		/* Append the rest of the input verbatim */` |
|      3 | 3011 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 3012 | `	}` |
|      3 | 3013 | `	return PH7_OK;` |
|      3 | 3014 | `}` |
|      - | 3015 | `/*` |
|      - | 3016 | ` * int ord(string $string)` |
|      - | 3017 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 3018 | ` *  Passing null, an empty string, or a multi-byte string emits` |
|      - | 3019 | ` *  E_DEPRECATED to match PHP 8.4+ behaviour.` |
|      - | 3020 | ` * Parameters` |
|      - | 3021 | ` *  $string` |
|      - | 3022 | ` *   The input string.` |
|      - | 3023 | ` * Returns` |
|      - | 3024 | ` *  The ASCII value as an integer.` |
|      - | 3025 | ` */` |
|    226 | 3026 | `static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 3027 | `{` |
|      - | 3028 | `	const char *zString;` |
|      - | 3029 | `	int nLen,c;` |
|      - | 3030 | `	/* PHP requires exactly one argument. */` |
|    230 | 3031 | `	if( nArg != 1 ){` |
|      4 | 3032 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3033 | `			"ArgumentCountError",` |
|      - | 3034 | `			"ord() expects exactly 1 argument, %d given",` |
|      1 | 3035 | `			nArg` |
|      - | 3036 | `			);` |
|      - | 3037 | `	}` |
|      - | 3038 | `	/* Passing null is deprecated (E_DEPRECATED).  PHP emits this before` |
|      - | 3039 | `	 * the empty-string deprecation, so we check null first. */` |
|    227 | 3040 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 3041 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 3042 | `			"ord(): Passing null to parameter #1 ($character) "` |
|      - | 3043 | `			"of type string is deprecated"` |
|      - | 3044 | `			);` |
|      1 | 3045 | `	}` |
|      - | 3046 | `	/* Extract the target string */` |
|    227 | 3047 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    227 | 3048 | `	if( nLen < 1 ){` |
|      - | 3049 | `		/* Empty string is deprecated (E_DEPRECATED). */` |
|      5 | 3050 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 3051 | `			"ord(): Providing an empty string is deprecated"` |
|      - | 3052 | `			);` |
|      5 | 3053 | `		ph7_result_int(pCtx,0);` |
|      5 | 3054 | `		return PH7_OK;` |
|      - | 3055 | `	}` |
|      - | 3056 | `	/* A string longer than one byte is deprecated (E_DEPRECATED). */` |
|    223 | 3057 | `	if( nLen > 1 ){` |
|      7 | 3058 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 3059 | `			"ord(): Providing a string that is not one byte long "` |
|      - | 3060 | `			"is deprecated. Use ord($str[0]) instead"` |
|      - | 3061 | `			);` |
|      3 | 3062 | `	}` |
|      - | 3063 | `	/* Extract the ASCII value of the first character */` |
|    223 | 3064 | `	c = (unsigned char)zString[0];` |
|      - | 3065 | `	/* Return that value */` |
|    223 | 3066 | `	ph7_result_int(pCtx,c);` |
|    223 | 3067 | `	return PH7_OK;` |
|    117 | 3068 | `}` |
|      - | 3069 | `/*` |
|      - | 3070 | ` * string chr(int $codepoint)` |
|      - | 3071 | ` *  Returns a one-character string containing the character specified` |
|      - | 3072 | ` *  by the given codepoint.  Any integer is accepted; values outside` |
|      - | 3073 | ` *  the [0, 255] range emit an E_DEPRECATED and are masked with & 0xFF.` |
|      - | 3074 | ` * Parameters` |
|      - | 3075 | ` *  $codepoint` |
|      - | 3076 | ` *   An integer codepoint.  Values outside 0-255 are deprecated and` |
|      - | 3077 | ` *   will be constrained to a single byte.` |
|      - | 3078 | ` * Returns` |
|      - | 3079 | ` *  A single-character string.` |
|      - | 3080 | ` */` |
|   7170 | 3081 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3082 | `{` |
|      - | 3083 | `	int c;` |
|      - | 3084 | `	unsigned char ch;` |
|      - | 3085 | `	/* PHP requires exactly one argument. */` |
|   7173 | 3086 | `	if( nArg != 1 ){` |
|      4 | 3087 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3088 | `			"ArgumentCountError",` |
|      - | 3089 | `			"chr() expects exactly 1 argument, %d given",` |
|      1 | 3090 | `			nArg` |
|      - | 3091 | `			);` |
|      - | 3092 | `	}` |
|      - | 3093 | `	/* Implicit float-to-int conversion loses precision (E_DEPRECATED).` |
|      - | 3094 | `	 * PHP does not prefix this message with "chr():", so we call` |
|      - | 3095 | `	 * PH7_VmThrowError() with a NULL function name to avoid the` |
|      - | 3096 | `	 * automatic prefix that ph7_context_throw_error*() would add. */` |
|   7171 | 3097 | `	if( ph7_value_is_float(apArg[0]) ){` |
|      - | 3098 | `		char zBuf[120];` |
|      4 | 3099 | `		SyBufferFormat(zBuf,sizeof(zBuf),` |
|      - | 3100 | `			"Implicit conversion from float %g to int loses precision",` |
|      1 | 3101 | `			ph7_value_to_double(apArg[0])` |
|      - | 3102 | `			);` |
|      3 | 3103 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zBuf);` |
|      1 | 3104 | `	}` |
|      - | 3105 | `	/* Extract the codepoint. */` |
|   7171 | 3106 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 3107 | `	/* Out-of-range codepoint (E_DEPRECATED), then mask to a single byte.` |
|      - | 3108 | `	 * PHP includes "chr(): " in the $errstr passed to set_error_handler,` |
|      - | 3109 | `	 * so we embed the prefix in the message and pass NULL as the function` |
|      - | 3110 | `	 * name to avoid the API double-prefixing it. */` |
|   7171 | 3111 | `	if( c < 0 \|\| c > 255 ){` |
|      5 | 3112 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 3113 | `			E_DEPRECATED,` |
|      - | 3114 | `			"chr(): Providing a value not in-between 0 and 255 is deprecated, "` |
|      - | 3115 | `			"this is because a byte value must be in the [0, 255] interval. "` |
|      - | 3116 | `			"The value used will be constrained using % 256"` |
|      - | 3117 | `			);` |
|      2 | 3118 | `	}` |
|      - | 3119 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|      - | 3120 | `	 * when taking the address of a wider int. */` |
|   7171 | 3121 | `	ch = (unsigned char)(c & 0xFF);` |
|      - | 3122 | `	/* Return the specified character */` |
|   7171 | 3123 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|   7171 | 3124 | `	return PH7_OK;` |
|   3588 | 3125 | `}` |
|      - | 3126 | `/*` |
|      - | 3127 | ` * Binary to hex consumer callback.` |
|      - | 3128 | ` * This callback is the default consumer used by the hash functions` |
|      - | 3129 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 3130 | ` */` |
|   3170 | 3131 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 3132 | `{` |
|      - | 3133 | `	/* Append hex chunk verbatim */` |
|   3172 | 3134 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   3172 | 3135 | `	return SXRET_OK;` |
|      2 | 3136 | `}` |
|      - | 3137 |  |
|      - | 3138 | `/*` |
|      - | 3139 | ` * string bin2hex(string $str)` |
|      - | 3140 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 3141 | ` * Parameters` |
|      - | 3142 | ` *  $str` |
|      - | 3143 | ` *   The input string.` |
|      - | 3144 | ` * Returns.` |
|      - | 3145 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 3146 | ` */` |
|    152 | 3147 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3148 | `{` |
|      - | 3149 | `	const char *zString;` |
|      - | 3150 | `	int nLen;` |
|      - | 3151 | `	/* PHP 8 requires exactly one argument (ArgumentCountError). */` |
|    155 | 3152 | `	if( nArg != 1 ){` |
|      4 | 3153 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3154 | `			"ArgumentCountError",` |
|      - | 3155 | `			"bin2hex() expects exactly 1 argument, %d given",` |
|      1 | 3156 | `			nArg` |
|      - | 3157 | `			);` |
|      - | 3158 | `	}` |
|      - | 3159 | `	/* In PHP 8, bin2hex() is strict about its parameter type.` |
|      - | 3160 | `	 * Array/Resource values are not allowed and trigger a TypeError.` |
|      - | 3161 | `	 * Objects without __toString() must also raise a TypeError.` |
|      - | 3162 | `	 */` |
|    227 | 3163 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) \|\|` |
|     75 | 3164 | `		( ph7_value_is_object(apArg[0]) &&` |
|    ! 0 | 3165 | `		  ((ph7_class_instance *)apArg[0]->x.pOther) != 0 &&` |
|    ! 0 | 3166 | `		  PH7_ClassExtractMethod(((ph7_class_instance *)apArg[0]->x.pOther)->pClass,` |
|    ! 0 | 3167 | `			"__toString",sizeof("__toString")-1) == 0` |
|      - | 3168 | `		)` |
|      - | 3169 | `	){` |
|    ! 0 | 3170 | `		const char *zType = ph7_type_name(apArg[0]);` |
|    ! 0 | 3171 | `		if( ph7_value_is_object(apArg[0]) ){` |
|    ! 0 | 3172 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    ! 0 | 3173 | `			if( pInst && pInst->pClass ){` |
|    ! 0 | 3174 | `				zType = SyStringData(&pInst->pClass->sName);` |
|    ! 0 | 3175 | `			}` |
|    ! 0 | 3176 | `		}` |
|    ! 0 | 3177 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3178 | `			"TypeError",` |
|      - | 3179 | `			"bin2hex(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 3180 | `			zType` |
|      - | 3181 | `			);` |
|      - | 3182 | `	}` |
|      - | 3183 | `	/* Extract the target string */` |
|    152 | 3184 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    152 | 3185 | `	if( nLen < 1 ){` |
|      - | 3186 | `		/* Empty string,return */` |
|     13 | 3187 | `		ph7_result_string(pCtx,"",0);` |
|     13 | 3188 | `		return PH7_OK;` |
|      - | 3189 | `	}` |
|      - | 3190 | `	/* Perform the requested operation */` |
|    140 | 3191 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|    140 | 3192 | `	return PH7_OK;` |
|     79 | 3193 | `}` |
|      - | 3194 |  |
|      - | 3195 | `/* Search callback signature */` |
|      - | 3196 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 3197 | `/*` |
|      - | 3198 | ` * Case-insensitive pattern match.` |
|      - | 3199 | ` * Brute force is the default search method used here.` |
|      - | 3200 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 3201 | ` * well for short/medium texts on modern hardware.` |
|      - | 3202 | ` */` |
|    298 | 3203 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      2 | 3204 | `{` |
|    300 | 3205 | `	const char *zpIn = (const char *)pPattern;` |
|    300 | 3206 | `	const char *zIn = (const char *)pText;` |
|    300 | 3207 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    300 | 3208 | `	const char *zEnd = &zIn[nLen];` |
|      - | 3209 | `	const char *zPtr,*zPtr2;` |
|      - | 3210 | `	int c,d;` |
|    300 | 3211 | `	if( iPatLen > nLen ){` |
|      - | 3212 | `		/* Don't bother processing */` |
|     67 | 3213 | `		return SXERR_NOTFOUND;` |
|      - | 3214 | `	}` |
|    860 | 3215 | `	for(;;){` |
|   1722 | 3216 | `		if( zIn >= zEnd ){` |
|    194 | 3217 | `			break;` |
|      - | 3218 | `		}` |
|   1530 | 3219 | `		c = SyToLower(zIn[0]);` |
|   1530 | 3220 | `		d = SyToLower(zpIn[0]);` |
|   1530 | 3221 | `		if( c == d ){` |
|    182 | 3222 | `			zPtr   = &zIn[1];` |
|    182 | 3223 | `			zPtr2  = &zpIn[1];` |
|    141 | 3224 | `			for(;;){` |
|    284 | 3225 | `				if( zPtr2 >= zpEnd ){` |
|      - | 3226 | `					/* Pattern found */` |
|     41 | 3227 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 3228 | `					return SXRET_OK;` |
|      - | 3229 | `				}` |
|    244 | 3230 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 3231 | `					break;` |
|      - | 3232 | `				}` |
|    244 | 3233 | `				c = SyToLower(zPtr[0]);` |
|    244 | 3234 | `				d = SyToLower(zPtr2[0]);` |
|    244 | 3235 | `				if( c != d ){` |
|    142 | 3236 | `					break;` |
|      - | 3237 | `				}` |
|    103 | 3238 | `				zPtr++; zPtr2++;` |
|      1 | 3239 | `			}` |
|     70 | 3240 | `		}` |
|   1490 | 3241 | `		zIn++;` |
|      2 | 3242 | `	}` |
|      - | 3243 | `	/* Pattern not found */` |
|    194 | 3244 | `	return SXERR_NOTFOUND;` |
|    151 | 3245 | `}` |
|      - | 3246 | `/*` |
|      - | 3247 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3248 | ` *  Find the first occurrence of a string.` |
|      - | 3249 | ` * Parameters` |
|      - | 3250 | ` *  $haystack` |
|      - | 3251 | ` *   The input string.` |
|      - | 3252 | ` * $needle` |
|      - | 3253 | ` *   Search pattern (must be a string).` |
|      - | 3254 | ` * $before_needle` |
|      - | 3255 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3256 | ` *   of the needle (excluding the needle).` |
|      - | 3257 | ` * Return` |
|      - | 3258 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3259 | ` */` |
|      6 | 3260 | `static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3261 | `{` |
|      7 | 3262 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3263 | `	const char *zBlob,*zPattern;` |
|      - | 3264 | `	int nLen,nPatLen;` |
|      - | 3265 | `	sxu32 nOfft;` |
|      - | 3266 | `	sxi32 rc;` |
|      7 | 3267 | `	if( nArg < 2 ){` |
|      - | 3268 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3269 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3270 | `		return PH7_OK;` |
|      - | 3271 | `	}` |
|      - | 3272 | `	/* Extract the needle and the haystack */` |
|      7 | 3273 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 3274 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 3275 | `	nOfft = 0; /* cc warning */` |
|      9 | 3276 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3277 | `		int before = 0;` |
|      - | 3278 | `		/* Perform the lookup */` |
|      5 | 3279 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3280 | `		if( rc != SXRET_OK ){` |
|      - | 3281 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3282 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3283 | `			return PH7_OK;` |
|      - | 3284 | `		}` |
|      - | 3285 | `		/* Return the portion of the string */` |
|      5 | 3286 | `		if( nArg > 2 ){` |
|      3 | 3287 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3288 | `		}` |
|      5 | 3289 | `		if( before ){` |
|      3 | 3290 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3291 | `		}else{` |
|      3 | 3292 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3293 | `		}` |
|      3 | 3294 | `	}else{` |
|      3 | 3295 | `		ph7_result_bool(pCtx,0);` |
|      - | 3296 | `	}` |
|      7 | 3297 | `	return PH7_OK;` |
|      4 | 3298 | `}` |
|      - | 3299 | `/*` |
|      - | 3300 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3301 | ` *  Case-insensitive strstr().` |
|      - | 3302 | ` * Parameters` |
|      - | 3303 | ` *  $haystack` |
|      - | 3304 | ` *   The input string.` |
|      - | 3305 | ` * $needle` |
|      - | 3306 | ` *   Search pattern (must be a string).` |
|      - | 3307 | ` * $before_needle` |
|      - | 3308 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3309 | ` *   of the needle (excluding the needle).` |
|      - | 3310 | ` * Return` |
|      - | 3311 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3312 | ` */` |
|      4 | 3313 | `static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3314 | `{` |
|      5 | 3315 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3316 | `	const char *zBlob,*zPattern;` |
|      - | 3317 | `	int nLen,nPatLen;` |
|      - | 3318 | `	sxu32 nOfft;` |
|      - | 3319 | `	sxi32 rc;` |
|      5 | 3320 | `	if( nArg < 2 ){` |
|      - | 3321 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3322 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3323 | `		return PH7_OK;` |
|      - | 3324 | `	}` |
|      - | 3325 | `	/* Extract the needle and the haystack */` |
|      5 | 3326 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3327 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      5 | 3328 | `	nOfft = 0; /* cc warning */` |
|      7 | 3329 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3330 | `		int before = 0;` |
|      - | 3331 | `		/* Perform the lookup */` |
|      5 | 3332 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3333 | `		if( rc != SXRET_OK ){` |
|      - | 3334 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3335 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3336 | `			return PH7_OK;` |
|      - | 3337 | `		}` |
|      - | 3338 | `		/* Return the portion of the string */` |
|      5 | 3339 | `		if( nArg > 2 ){` |
|      3 | 3340 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3341 | `		}` |
|      5 | 3342 | `		if( before ){` |
|      3 | 3343 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3344 | `		}else{` |
|      3 | 3345 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3346 | `		}` |
|      3 | 3347 | `	}else{` |
|    ! 0 | 3348 | `		ph7_result_bool(pCtx,0);` |
|      - | 3349 | `	}` |
|      5 | 3350 | `	return PH7_OK;` |
|      3 | 3351 | `}` |
|      - | 3352 | `/*` |
|      - | 3353 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3354 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 3355 | ` * Parameters` |
|      - | 3356 | ` *  $haystack` |
|      - | 3357 | ` *   The input string.` |
|      - | 3358 | ` * $needle` |
|      - | 3359 | ` *   Search pattern (must be a string).` |
|      - | 3360 | ` * $offset` |
|      - | 3361 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3362 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3363 | ` *   of haystack.` |
|      - | 3364 | ` * Return` |
|      - | 3365 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3366 | ` */` |
|   1562 | 3367 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3368 | `{` |
|   1567 | 3369 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strpos",1,"$haystack"); }` |
|   1567 | 3370 | `	if( nArg > 1 ){ StrNullArgNotice(pCtx,apArg[1],"strpos",2,"$needle"); }` |
|   1567 | 3371 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3372 | `	const char *zBlob,*zPattern;` |
|      - | 3373 | `	int nLen,nPatLen,nStart;` |
|      - | 3374 | `	sxu32 nOfft;` |
|      - | 3375 | `	sxi32 rc;` |
|   1567 | 3376 | `	if( nArg < 2 ){` |
|      - | 3377 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3378 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3379 | `		return PH7_OK;` |
|      - | 3380 | `	}` |
|      - | 3381 | `	/* Extract the needle and the haystack */` |
|   1567 | 3382 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|   1567 | 3383 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|   1567 | 3384 | `	nOfft = 0; /* cc warning */` |
|   1567 | 3385 | `	nStart = 0;` |
|      - | 3386 | `	/* Peek the starting offset if available */` |
|   1567 | 3387 | `	if( nArg > 2 ){` |
|     15 | 3388 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3389 | `		if( nStart < 0 ){` |
|    ! 0 | 3390 | `			nStart = -nStart;` |
|    ! 0 | 3391 | `		}` |
|     15 | 3392 | `		if( nStart >= nLen ){` |
|      - | 3393 | `			/* Invalid offset */` |
|    ! 0 | 3394 | `			nStart = 0;` |
|    ! 0 | 3395 | `		}else{` |
|     15 | 3396 | `			zBlob += nStart;` |
|     15 | 3397 | `			nLen -= nStart;` |
|      - | 3398 | `		}` |
|      7 | 3399 | `	}` |
|   1567 | 3400 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3401 | `		/* Perform the lookup */` |
|   1563 | 3402 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|   1563 | 3403 | `		if( rc != SXRET_OK ){` |
|      - | 3404 | `			/* Pattern not found,return FALSE */` |
|    803 | 3405 | `			ph7_result_bool(pCtx,0);` |
|    803 | 3406 | `			return PH7_OK;` |
|      - | 3407 | `		}` |
|      - | 3408 | `		/* Return the pattern position */` |
|    764 | 3409 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|    384 | 3410 | `	}else{` |
|      5 | 3411 | `		ph7_result_bool(pCtx,0);` |
|      - | 3412 | `	}` |
|    768 | 3413 | `	return PH7_OK;` |
|    786 | 3414 | `}` |
|      - | 3415 | `/*` |
|      - | 3416 | ` * Validate and resolve a single string-typed parameter for str_contains/` |
|      - | 3417 | ` * str_starts_with/str_ends_with. Emits an E_DEPRECATED notice for null` |
|      - | 3418 | ` * (matching PHP 8.1+; falls through with an empty string), and throws` |
|      - | 3419 | ` * TypeError for arrays, resources, and objects without __toString.` |
|      - | 3420 | ` *` |
|      - | 3421 | ` * For objects with __toString, invokes the method directly into pTmp and` |
|      - | 3422 | ` * uses its raw byte buffer. This preserves empty results, which the` |
|      - | 3423 | ` * engine's MemObjStringValue otherwise replaces with the literal "Object".` |
|      - | 3424 | ` *` |
|      - | 3425 | ` * On success, pzOut/pnOut point at the resolved byte buffer; the buffer` |
|      - | 3426 | ` * is valid until pTmp is released or pArg is mutated.` |
|      - | 3427 | ` */` |
|    668 | 3428 | `static sxi32 StrPredicateResolveArg(` |
|      - | 3429 | `	ph7_context *pCtx,` |
|      - | 3430 | `	ph7_value *pArg,` |
|      - | 3431 | `	const char *zFunc,` |
|      - | 3432 | `	int iArgNum,` |
|      - | 3433 | `	const char *zParamName,` |
|      - | 3434 | `	const char *zTypeStr, /* Declared type in the TypeError, e.g. "string" / "?string" */` |
|      - | 3435 | `	const char *zNullMsg,` |
|      - | 3436 | `	ph7_value *pTmp,` |
|      - | 3437 | `	const char **pzOut,` |
|      - | 3438 | `	int *pnOut` |
|      2 | 3439 | `){` |
|    670 | 3440 | `	if( ph7_value_is_null(pArg) ){` |
|     13 | 3441 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zNullMsg);` |
|     13 | 3442 | `		*pzOut = "";` |
|     13 | 3443 | `		*pnOut = 0;` |
|     13 | 3444 | `		return PH7_OK;` |
|      - | 3445 | `	}` |
|   1010 | 3446 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_resource(pArg) \|\|` |
|    656 | 3447 | `	    ( ph7_value_is_object(pArg) &&` |
|     72 | 3448 | `	      ((ph7_class_instance *)pArg->x.pOther) != 0 &&` |
|     48 | 3449 | `	      PH7_ClassExtractMethod(((ph7_class_instance *)pArg->x.pOther)->pClass,` |
|     24 | 3450 | `	        "__toString",sizeof("__toString")-1) == 0` |
|      - | 3451 | `	    )` |
|      - | 3452 | `	){` |
|    ! 0 | 3453 | `		const char *zType = ph7_type_name(pArg);` |
|    ! 0 | 3454 | `		if( ph7_value_is_object(pArg) ){` |
|    ! 0 | 3455 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|    ! 0 | 3456 | `			if( pInst && pInst->pClass ){` |
|    ! 0 | 3457 | `				zType = SyStringData(&pInst->pClass->sName);` |
|    ! 0 | 3458 | `			}` |
|    ! 0 | 3459 | `		}` |
|    ! 0 | 3460 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3461 | `			"TypeError",` |
|      - | 3462 | `			"%s(): Argument #%d (%s) must be of type %s, %s given",` |
|    ! 0 | 3463 | `			zFunc, iArgNum, zParamName, zTypeStr, zType` |
|      - | 3464 | `			);` |
|      - | 3465 | `	}` |
|    658 | 3466 | `	if( ph7_value_is_object(pArg) ){` |
|     49 | 3467 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     49 | 3468 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pInst->pClass,` |
|      - | 3469 | `			"__toString",sizeof("__toString")-1);` |
|     49 | 3470 | `		PH7_VmCallClassMethod(pCtx->pVm,pInst,pMethod,pTmp,0,0);` |
|     49 | 3471 | `		*pzOut = (const char *)SyBlobData(&pTmp->sBlob);` |
|     49 | 3472 | `		*pnOut = (int)SyBlobLength(&pTmp->sBlob);` |
|     49 | 3473 | `		return PH7_OK;` |
|      - | 3474 | `	}` |
|    610 | 3475 | `	*pzOut = ph7_value_to_string(pArg,pnOut);` |
|    610 | 3476 | `	return PH7_OK;` |
|    336 | 3477 | `}` |
|      - | 3478 | `/*` |
|      - | 3479 | ` * bool str_contains(string $haystack, string $needle)` |
|      - | 3480 | ` *  Determine if a string contains a given substring (PHP 8.0).` |
|      - | 3481 | ` * Return` |
|      - | 3482 | ` *  TRUE if needle occurs in haystack. An empty needle always returns TRUE.` |
|      - | 3483 | ` */` |
|     92 | 3484 | `static int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3485 | `{` |
|      - | 3486 | `	const char *zHaystack,*zNeedle;` |
|      - | 3487 | `	int nHayLen,nNeedleLen;` |
|      - | 3488 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3489 | `	sxi32 rc;` |
|     95 | 3490 | `	if( nArg != 2 ){` |
|      8 | 3491 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3492 | `			"ArgumentCountError",` |
|      - | 3493 | `			"str_contains() expects exactly 2 arguments, %d given",` |
|      2 | 3494 | `			nArg` |
|      - | 3495 | `			);` |
|      - | 3496 | `	}` |
|     90 | 3497 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     90 | 3498 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     90 | 3499 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_contains",1,"$haystack","string",` |
|      - | 3500 | `		"str_contains(): Passing null to parameter #1 ($haystack) "` |
|      - | 3501 | `		"of type string is deprecated",` |
|      - | 3502 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     90 | 3503 | `	if( rc != PH7_OK ) goto out;` |
|     90 | 3504 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_contains",2,"$needle","string",` |
|      - | 3505 | `		"str_contains(): Passing null to parameter #2 ($needle) "` |
|      - | 3506 | `		"of type string is deprecated",` |
|      - | 3507 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     90 | 3508 | `	if( rc != PH7_OK ) goto out;` |
|     90 | 3509 | `	if( nNeedleLen < 1 ){` |
|     13 | 3510 | `		ph7_result_bool(pCtx,1);` |
|     84 | 3511 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 3512 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3513 | `	}else{` |
|    104 | 3514 | `		sxi32 srch = SyBlobSearch((const void *)zHaystack,(sxu32)nHayLen,` |
|     34 | 3515 | `		                          (const void *)zNeedle,(sxu32)nNeedleLen,0);` |
|     70 | 3516 | `		ph7_result_bool(pCtx,srch == SXRET_OK ? 1 : 0);` |
|      - | 3517 | `	}` |
|     90 | 3518 | `	rc = PH7_OK;` |
|     44 | 3519 | `out:` |
|     90 | 3520 | `	PH7_MemObjRelease(&sHayTmp);` |
|     90 | 3521 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     90 | 3522 | `	return rc;` |
|     49 | 3523 | `}` |
|      - | 3524 | `/*` |
|      - | 3525 | ` * bool str_starts_with(string $haystack, string $needle)` |
|      - | 3526 | ` *  Check if a string starts with a given substring (PHP 8.0).` |
|      - | 3527 | ` * Return` |
|      - | 3528 | ` *  TRUE if haystack begins with needle. An empty needle always returns TRUE.` |
|      - | 3529 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 3530 | ` */` |
|     62 | 3531 | `static int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3532 | `{` |
|      - | 3533 | `	const char *zHaystack,*zNeedle;` |
|      - | 3534 | `	int nHayLen,nNeedleLen;` |
|      - | 3535 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3536 | `	sxi32 rc;` |
|     64 | 3537 | `	if( nArg != 2 ){` |
|      8 | 3538 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3539 | `			"ArgumentCountError",` |
|      - | 3540 | `			"str_starts_with() expects exactly 2 arguments, %d given",` |
|      2 | 3541 | `			nArg` |
|      - | 3542 | `			);` |
|      - | 3543 | `	}` |
|     59 | 3544 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     59 | 3545 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     59 | 3546 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_starts_with",1,"$haystack","string",` |
|      - | 3547 | `		"str_starts_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 3548 | `		"of type string is deprecated",` |
|      - | 3549 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     59 | 3550 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3551 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_starts_with",2,"$needle","string",` |
|      - | 3552 | `		"str_starts_with(): Passing null to parameter #2 ($needle) "` |
|      - | 3553 | `		"of type string is deprecated",` |
|      - | 3554 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     59 | 3555 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3556 | `	if( nNeedleLen < 1 ){` |
|     13 | 3557 | `		ph7_result_bool(pCtx,1);` |
|     53 | 3558 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 3559 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3560 | `	}else{` |
|     58 | 3561 | `		ph7_result_bool(pCtx,` |
|     38 | 3562 | `			SyMemcmp(zHaystack,zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 3563 | `	}` |
|     59 | 3564 | `	rc = PH7_OK;` |
|     29 | 3565 | `out:` |
|     59 | 3566 | `	PH7_MemObjRelease(&sHayTmp);` |
|     59 | 3567 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     59 | 3568 | `	return rc;` |
|     33 | 3569 | `}` |
|      - | 3570 | `/*` |
|      - | 3571 | ` * bool str_ends_with(string $haystack, string $needle)` |
|      - | 3572 | ` *  Check if a string ends with a given substring (PHP 8.0).` |
|      - | 3573 | ` * Return` |
|      - | 3574 | ` *  TRUE if haystack ends with needle. An empty needle always returns TRUE.` |
|      - | 3575 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 3576 | ` */` |
|     62 | 3577 | `static int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3578 | `{` |
|      - | 3579 | `	const char *zHaystack,*zNeedle;` |
|      - | 3580 | `	int nHayLen,nNeedleLen;` |
|      - | 3581 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3582 | `	sxi32 rc;` |
|     64 | 3583 | `	if( nArg != 2 ){` |
|      8 | 3584 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3585 | `			"ArgumentCountError",` |
|      - | 3586 | `			"str_ends_with() expects exactly 2 arguments, %d given",` |
|      2 | 3587 | `			nArg` |
|      - | 3588 | `			);` |
|      - | 3589 | `	}` |
|     59 | 3590 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     59 | 3591 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     59 | 3592 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_ends_with",1,"$haystack","string",` |
|      - | 3593 | `		"str_ends_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 3594 | `		"of type string is deprecated",` |
|      - | 3595 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     59 | 3596 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3597 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_ends_with",2,"$needle","string",` |
|      - | 3598 | `		"str_ends_with(): Passing null to parameter #2 ($needle) "` |
|      - | 3599 | `		"of type string is deprecated",` |
|      - | 3600 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     59 | 3601 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3602 | `	if( nNeedleLen < 1 ){` |
|     13 | 3603 | `		ph7_result_bool(pCtx,1);` |
|     53 | 3604 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 3605 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3606 | `	}else{` |
|     58 | 3607 | `		ph7_result_bool(pCtx,` |
|     38 | 3608 | `			SyMemcmp(zHaystack + (nHayLen - nNeedleLen),zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 3609 | `	}` |
|     59 | 3610 | `	rc = PH7_OK;` |
|     29 | 3611 | `out:` |
|     59 | 3612 | `	PH7_MemObjRelease(&sHayTmp);` |
|     59 | 3613 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     59 | 3614 | `	return rc;` |
|     33 | 3615 | `}` |
|      - | 3616 | `/*` |
|      - | 3617 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3618 | ` *  Case-insensitive strpos.` |
|      - | 3619 | ` * Parameters` |
|      - | 3620 | ` *  $haystack` |
|      - | 3621 | ` *   The input string.` |
|      - | 3622 | ` * $needle` |
|      - | 3623 | ` *   Search pattern (must be a string).` |
|      - | 3624 | ` * $offset` |
|      - | 3625 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3626 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3627 | ` *   of haystack.` |
|      - | 3628 | ` * Return` |
|      - | 3629 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3630 | ` */` |
|    196 | 3631 | `static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3632 | `{` |
|    198 | 3633 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3634 | `	const char *zBlob,*zPattern;` |
|      - | 3635 | `	int nLen,nPatLen,nStart;` |
|      - | 3636 | `	sxu32 nOfft;` |
|      - | 3637 | `	sxi32 rc;` |
|    198 | 3638 | `	if( nArg < 2 ){` |
|      - | 3639 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3640 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3641 | `		return PH7_OK;` |
|      - | 3642 | `	}` |
|      - | 3643 | `	/* Extract the needle and the haystack */` |
|    198 | 3644 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|    198 | 3645 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|    198 | 3646 | `	nOfft = 0; /* cc warning */` |
|    198 | 3647 | `	nStart = 0;` |
|      - | 3648 | `	/* Peek the starting offset if available */` |
|    198 | 3649 | `	if( nArg > 2 ){` |
|      5 | 3650 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 3651 | `		if( nStart < 0 ){` |
|      3 | 3652 | `			nStart = -nStart;` |
|      1 | 3653 | `		}` |
|      5 | 3654 | `		if( nStart >= nLen ){` |
|      - | 3655 | `			/* Invalid offset */` |
|    ! 0 | 3656 | `			nStart = 0;` |
|    ! 0 | 3657 | `		}else{` |
|      5 | 3658 | `			zBlob += nStart;` |
|      5 | 3659 | `			nLen -= nStart;` |
|      - | 3660 | `		}` |
|      2 | 3661 | `	}` |
|    198 | 3662 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3663 | `		/* Perform the lookup */` |
|    198 | 3664 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|    198 | 3665 | `		if( rc != SXRET_OK ){` |
|      - | 3666 | `			/* Pattern not found,return FALSE */` |
|    184 | 3667 | `			ph7_result_bool(pCtx,0);` |
|    184 | 3668 | `			return PH7_OK;` |
|      - | 3669 | `		}` |
|      - | 3670 | `		/* Return the pattern position */` |
|     15 | 3671 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 3672 | `	}else{` |
|    ! 0 | 3673 | `		ph7_result_bool(pCtx,0);` |
|      - | 3674 | `	}` |
|     15 | 3675 | `	return PH7_OK;` |
|    100 | 3676 | `}` |
|      - | 3677 | `/*` |
|      - | 3678 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3679 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 3680 | ` * Parameters` |
|      - | 3681 | ` *  $haystack` |
|      - | 3682 | ` *   The input string.` |
|      - | 3683 | ` * $needle` |
|      - | 3684 | ` *   Search pattern (must be a string).` |
|      - | 3685 | ` * $offset` |
|      - | 3686 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3687 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3688 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3689 | ` * Return` |
|      - | 3690 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3691 | ` */` |
|     42 | 3692 | `static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3693 | `{` |
|      - | 3694 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     43 | 3695 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3696 | `	int nLen,nPatLen;` |
|      - | 3697 | `	sxu32 nOfft;` |
|      - | 3698 | `	sxi32 rc;` |
|     43 | 3699 | `	if( nArg < 2 ){` |
|      - | 3700 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3701 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3702 | `		return PH7_OK;` |
|      - | 3703 | `	}` |
|      - | 3704 | `	/* Extract the needle and the haystack */` |
|     43 | 3705 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     43 | 3706 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3707 | `	/* Point to the end of the pattern */` |
|     43 | 3708 | `	zPtr = &zBlob[nLen - 1];` |
|     43 | 3709 | `	zEnd = &zBlob[nLen];` |
|      - | 3710 | `	/* Save the starting posistion */` |
|     43 | 3711 | `	zStart = zBlob;` |
|     43 | 3712 | `	nOfft = 0; /* cc warning */` |
|      - | 3713 | `	/* Peek the starting offset if available */` |
|     43 | 3714 | `	if( nArg > 2 ){` |
|      - | 3715 | `		int nStart;` |
|     21 | 3716 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 3717 | `		if( nStart < 0 ){` |
|     11 | 3718 | `			nStart = -nStart;` |
|     11 | 3719 | `			if( nStart >= nLen ){` |
|      - | 3720 | `				/* Invalid offset */` |
|      3 | 3721 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3722 | `				return PH7_OK;` |
|    ! 0 | 3723 | `			}else{` |
|      9 | 3724 | `				nLen -= nStart;` |
|      9 | 3725 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 3726 | `				zEnd = &zBlob[nLen];` |
|      - | 3727 | `			}` |
|      5 | 3728 | `		}else{` |
|     11 | 3729 | `			if( nStart >= nLen ){` |
|      - | 3730 | `				/* Invalid offset */` |
|      5 | 3731 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3732 | `				return PH7_OK;` |
|    ! 0 | 3733 | `			}else{` |
|      7 | 3734 | `				zBlob += nStart;` |
|      7 | 3735 | `				nLen -= nStart;` |
|      - | 3736 | `			}` |
|      - | 3737 | `		}` |
|      7 | 3738 | `	}` |
|     37 | 3739 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3740 | `		/* Perform the lookup */` |
|    123 | 3741 | `		for(;;){` |
|    247 | 3742 | `			if( zBlob >= zPtr ){` |
|     21 | 3743 | `				break;` |
|      - | 3744 | `			}` |
|    227 | 3745 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    227 | 3746 | `			if( rc == SXRET_OK ){` |
|      - | 3747 | `				/* Pattern found,return it's position */` |
|     15 | 3748 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     15 | 3749 | `				return PH7_OK;` |
|      - | 3750 | `			}` |
|    213 | 3751 | `			zPtr--;` |
|      1 | 3752 | `		}` |
|      - | 3753 | `		/* Pattern not found,return FALSE */` |
|     21 | 3754 | `		ph7_result_bool(pCtx,0);` |
|     11 | 3755 | `	}else{` |
|      3 | 3756 | `		ph7_result_bool(pCtx,0);` |
|      - | 3757 | `	}` |
|     23 | 3758 | `	return PH7_OK;` |
|     22 | 3759 | `}` |
|      - | 3760 | `/*` |
|      - | 3761 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3762 | ` *  Case-insensitive strrpos.` |
|      - | 3763 | ` * Parameters` |
|      - | 3764 | ` *  $haystack` |
|      - | 3765 | ` *   The input string.` |
|      - | 3766 | ` * $needle` |
|      - | 3767 | ` *   Search pattern (must be a string).` |
|      - | 3768 | ` * $offset` |
|      - | 3769 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3770 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3771 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3772 | ` * Return` |
|      - | 3773 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3774 | ` */` |
|     26 | 3775 | `static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3776 | `{` |
|      - | 3777 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     27 | 3778 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3779 | `	int nLen,nPatLen;` |
|      - | 3780 | `	sxu32 nOfft;` |
|      - | 3781 | `	sxi32 rc;` |
|     27 | 3782 | `	if( nArg < 2 ){` |
|      - | 3783 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3784 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3785 | `		return PH7_OK;` |
|      - | 3786 | `	}` |
|      - | 3787 | `	/* Extract the needle and the haystack */` |
|     27 | 3788 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 3789 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3790 | `	/* Point to the end of the pattern */` |
|     27 | 3791 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 3792 | `	zEnd = &zBlob[nLen];` |
|      - | 3793 | `	/* Save the starting posistion */` |
|     27 | 3794 | `	zStart = zBlob;` |
|     27 | 3795 | `	nOfft = 0; /* cc warning */` |
|      - | 3796 | `	/* Peek the starting offset if available */` |
|     27 | 3797 | `	if( nArg > 2 ){` |
|      - | 3798 | `		int nStart;` |
|     15 | 3799 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3800 | `		if( nStart < 0 ){` |
|      7 | 3801 | `			nStart = -nStart;` |
|      7 | 3802 | `			if( nStart >= nLen ){` |
|      - | 3803 | `				/* Invalid offset */` |
|      3 | 3804 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3805 | `				return PH7_OK;` |
|    ! 0 | 3806 | `			}else{` |
|      5 | 3807 | `				nLen -= nStart;` |
|      5 | 3808 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 3809 | `				zEnd = &zBlob[nLen];` |
|      - | 3810 | `			}` |
|      3 | 3811 | `		}else{` |
|      9 | 3812 | `			if( nStart >= nLen ){` |
|      - | 3813 | `				/* Invalid offset */` |
|      5 | 3814 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3815 | `				return PH7_OK;` |
|    ! 0 | 3816 | `			}else{` |
|      5 | 3817 | `				zBlob += nStart;` |
|      5 | 3818 | `				nLen -= nStart;` |
|      - | 3819 | `			}` |
|      - | 3820 | `		}` |
|      4 | 3821 | `	}` |
|     21 | 3822 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3823 | `		/* Perform the lookup */` |
|     44 | 3824 | `		for(;;){` |
|     89 | 3825 | `			if( zBlob >= zPtr ){` |
|      9 | 3826 | `				break;` |
|      - | 3827 | `			}` |
|     81 | 3828 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 3829 | `			if( rc == SXRET_OK ){` |
|      - | 3830 | `				/* Pattern found,return it's position */` |
|     11 | 3831 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 3832 | `				return PH7_OK;` |
|      - | 3833 | `			}` |
|     71 | 3834 | `			zPtr--;` |
|      1 | 3835 | `		}` |
|      - | 3836 | `		/* Pattern not found,return FALSE */` |
|      9 | 3837 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3838 | `	}else{` |
|      3 | 3839 | `		ph7_result_bool(pCtx,0);` |
|      - | 3840 | `	}` |
|     11 | 3841 | `	return PH7_OK;` |
|     14 | 3842 | `}` |
|      - | 3843 | `/*` |
|      - | 3844 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 3845 | ` *  Find the last occurrence of a character in a string.` |
|      - | 3846 | ` * Parameters` |
|      - | 3847 | ` *  $haystack` |
|      - | 3848 | ` *   The input string.` |
|      - | 3849 | ` * $needle` |
|      - | 3850 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 3851 | ` *  This behavior is different from that of strstr().` |
|      - | 3852 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 3853 | ` *  as the ordinal value of a character.` |
|      - | 3854 | ` * Return` |
|      - | 3855 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 3856 | ` */` |
|     22 | 3857 | `static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3858 | `{` |
|      - | 3859 | `	const char *zBlob;` |
|      - | 3860 | `	int nLen,c;` |
|     23 | 3861 | `	if( nArg < 2 ){` |
|      - | 3862 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3863 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3864 | `		return PH7_OK;` |
|      - | 3865 | `	}` |
|      - | 3866 | `	/* Extract the haystack */` |
|     23 | 3867 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3868 | `	c = 0; /* cc warning */` |
|     23 | 3869 | `	if( nLen > 0 ){` |
|      - | 3870 | `		sxu32 nOfft;` |
|      - | 3871 | `		sxi32 rc;` |
|     21 | 3872 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 3873 | `			const char *zPattern;` |
|     11 | 3874 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 3875 | `														 * for NULL pointer.` |
|      - | 3876 | `														 */` |
|     11 | 3877 | `			c = zPattern[0];` |
|      6 | 3878 | `		}else{` |
|      - | 3879 | `			/* Int cast */` |
|     11 | 3880 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 3881 | `		}` |
|      - | 3882 | `		/* Perform the lookup */` |
|     21 | 3883 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 3884 | `		if( rc != SXRET_OK ){` |
|      - | 3885 | `			/* No such entry,return FALSE */` |
|      7 | 3886 | `			ph7_result_bool(pCtx,0);` |
|      7 | 3887 | `			return PH7_OK;` |
|      - | 3888 | `		}` |
|      - | 3889 | `		/* Return the string portion */` |
|     15 | 3890 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 3891 | `	}else{` |
|      3 | 3892 | `		ph7_result_bool(pCtx,0);` |
|      - | 3893 | `	}` |
|     17 | 3894 | `	return PH7_OK;` |
|     12 | 3895 | `}` |
|      - | 3896 | `/*` |
|      - | 3897 | ` * string strrev(string $string)` |
|      - | 3898 | ` *  Reverse a string.` |
|      - | 3899 | ` * Parameters` |
|      - | 3900 | ` *  $string` |
|      - | 3901 | ` *   String to be reversed.` |
|      - | 3902 | ` * Return` |
|      - | 3903 | ` *  The reversed string.` |
|      - | 3904 | ` */` |
|      2 | 3905 | `static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3906 | `{` |
|      - | 3907 | `	const char *zIn,*zEnd;` |
|      - | 3908 | `	int nLen,c;` |
|      3 | 3909 | `	if( nArg < 1 ){` |
|      - | 3910 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3911 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3912 | `		return PH7_OK;` |
|      - | 3913 | `	}` |
|      - | 3914 | `	/* Extract the target string */` |
|      3 | 3915 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3916 | `	if( nLen < 1 ){` |
|      - | 3917 | `		/* Empty string Return null */` |
|    ! 0 | 3918 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3919 | `		return PH7_OK;` |
|      - | 3920 | `	}` |
|      - | 3921 | `	/* Perform the requested operation */` |
|      3 | 3922 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 3923 | `	for(;;){` |
|      9 | 3924 | `		if( zEnd < zIn ){` |
|      - | 3925 | `			/* No more input to process */` |
|      3 | 3926 | `			break;` |
|      - | 3927 | `		}` |
|      - | 3928 | `		/* Append current character */` |
|      7 | 3929 | `		c = zEnd[0];` |
|      7 | 3930 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 3931 | `		zEnd--;` |
|      1 | 3932 | `	}` |
|      3 | 3933 | `	return PH7_OK;` |
|      2 | 3934 | `}` |
|      - | 3935 | `/*` |
|      - | 3936 | ` * string ucwords(string $string [, string $separators = " \t\r\n\f\v"])` |
|      - | 3937 | ` *  Uppercase the first character of each word in a string.` |
|      - | 3938 | ` *  A word begins at the start of the string and after any character present in` |
|      - | 3939 | ` *  $separators. The default separators are the whitespace characters (space,` |
|      - | 3940 | ` *  horizontal tab, carriage return, newline, form-feed and vertical tab); an` |
|      - | 3941 | ` *  explicit $separators argument REPLACES them (an empty string leaves only the` |
|      - | 3942 | ` *  very first character upper-cased). Like PHP, this is byte-based: only ASCII` |
|      - | 3943 | ` *  bytes are upper-cased and a byte is a separator only if it appears in the set.` |
|      - | 3944 | ` * Parameters` |
|      - | 3945 | ` *  $string` |
|      - | 3946 | ` *   The input string.` |
|      - | 3947 | ` *  $separators` |
|      - | 3948 | ` *   The optional word-boundary characters.` |
|      - | 3949 | ` * Return` |
|      - | 3950 | ` *  The modified string.` |
|      - | 3951 | ` */` |
|     22 | 3952 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3953 | `{` |
|      - | 3954 | `	const char *zIn;` |
|      - | 3955 | `	int nLen,i,iStart;` |
|      - | 3956 | `	char aDelim[256];` |
|     23 | 3957 | `	if( nArg < 1 ){` |
|      - | 3958 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3959 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3960 | `		return PH7_OK;` |
|      - | 3961 | `	}` |
|      - | 3962 | `	/* Build the separator membership table: an explicit $separators argument` |
|      - | 3963 | `	 * replaces the default whitespace set (an empty string clears it). */` |
|     23 | 3964 | `	SyZero(aDelim,(sxu32)sizeof(aDelim));` |
|     23 | 3965 | `	if( nArg > 1 ){` |
|      - | 3966 | `		int nDelim;` |
|      9 | 3967 | `		const char *zDelim = ph7_value_to_string(apArg[1],&nDelim);` |
|     17 | 3968 | `		for( i = 0 ; i < nDelim ; i++ ){` |
|      9 | 3969 | `			aDelim[(unsigned char)zDelim[i]] = 1;` |
|      5 | 3970 | `		}` |
|      5 | 3971 | `	}else{` |
|     15 | 3972 | `		aDelim[(unsigned char)' ']  = 1;` |
|     15 | 3973 | `		aDelim[(unsigned char)'\t'] = 1;` |
|     15 | 3974 | `		aDelim[(unsigned char)'\r'] = 1;` |
|     15 | 3975 | `		aDelim[(unsigned char)'\n'] = 1;` |
|     15 | 3976 | `		aDelim[(unsigned char)'\f'] = 1;` |
|     15 | 3977 | `		aDelim[(unsigned char)'\v'] = 1;` |
|      - | 3978 | `	}` |
|      - | 3979 | `	/* Extract the target string */` |
|     23 | 3980 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3981 | `	if( nLen < 1 ){` |
|      - | 3982 | `		/* Empty string – match PHP semantics */` |
|      3 | 3983 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3984 | `		return PH7_OK;` |
|      - | 3985 | `	}` |
|      - | 3986 | `	/* Upper-case the first byte of each word (the leading byte, or any byte that` |
|      - | 3987 | `	 * follows a separator), appending the untouched runs in between verbatim. */` |
|     21 | 3988 | `	iStart = 0;` |
|    309 | 3989 | `	for( i = 0 ; i < nLen ; i++ ){` |
|    289 | 3990 | `		int c = (unsigned char)zIn[i];` |
|    289 | 3991 | `		if( (i == 0 \|\| aDelim[(unsigned char)zIn[i-1]]) && c < 0x80 && SyisLower(c) ){` |
|     53 | 3992 | `			char up = (char)SyToUpper(c);` |
|     53 | 3993 | `			if( i > iStart ){` |
|     35 | 3994 | `				ph7_result_string(pCtx,&zIn[iStart],i - iStart);` |
|     17 | 3995 | `			}` |
|     53 | 3996 | `			ph7_result_string(pCtx,&up,1);` |
|     53 | 3997 | `			iStart = i + 1;` |
|     26 | 3998 | `		}` |
|    145 | 3999 | `	}` |
|     21 | 4000 | `	if( nLen > iStart ){` |
|     21 | 4001 | `		ph7_result_string(pCtx,&zIn[iStart],nLen - iStart);` |
|     10 | 4002 | `	}` |
|     21 | 4003 | `	return PH7_OK;` |
|     12 | 4004 | `}` |
|      - | 4005 | `/*` |
|      - | 4006 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 4007 | ` *  Returns input repeated multiplier times.` |
|      - | 4008 | ` * Parameters` |
|      - | 4009 | ` *  $string` |
|      - | 4010 | ` *   String to be repeated.` |
|      - | 4011 | ` * $multiplier` |
|      - | 4012 | ` *  Number of time the input string should be repeated.` |
|      - | 4013 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 4014 | ` *  to 0, the function will return an empty string.` |
|      - | 4015 | ` * Return` |
|      - | 4016 | ` *  The repeated string.` |
|      - | 4017 | ` */` |
|  20438 | 4018 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4019 | `{` |
|      - | 4020 | `	const char *zIn;` |
|      - | 4021 | `	int nLen;` |
|      - | 4022 | `	ph7_int64 nMul;` |
|      - | 4023 | `	int rc;` |
|  20440 | 4024 | `	if( nArg < 2 ){` |
|      - | 4025 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 4026 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4027 | `		return PH7_OK;` |
|      - | 4028 | `	}` |
|      - | 4029 | `	/* Extract the target string */` |
|  20440 | 4030 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 4031 | `	/* Resolve $times through the shared ZPP helper so a lossy float / float-string` |
|      - | 4032 | `	 * carries php's precision deprecation and NAN/INF/non-numeric fail with php's` |
|      - | 4033 | `	 * TypeError — a bare ph7_value_to_int64() coerced them silently. */` |
|      - | 4034 | `	{` |
|  20440 | 4035 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"str_repeat",2,"$times","int",&nMul);` |
|  20440 | 4036 | `		if( rcArg != PH7_OK ){` |
|    ! 0 | 4037 | `			return rcArg;` |
|      - | 4038 | `		}` |
|      - | 4039 | `	}` |
|  20440 | 4040 | `	if( nMul < 0 ){` |
|      3 | 4041 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4042 | `			"str_repeat(): Argument #2 ($times) must be greater than or equal to 0");` |
|      - | 4043 | `	}` |
|  20438 | 4044 | `	if( nLen < 1 \|\| nMul < 1 ){` |
|      - | 4045 | `		/* Empty input or a zero multiplier yields the empty string (PHP). */` |
|    ! 0 | 4046 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4047 | `		return PH7_OK;` |
|      - | 4048 | `	}` |
|      - | 4049 | `	/* Perform the requested operation */` |
| 223888 | 4050 | `	for(;;){` |
| 447778 | 4051 | `		if( !nMul ){` |
|  20438 | 4052 | `			break;` |
|      - | 4053 | `		}` |
|      - | 4054 | `		/* Append the copy */` |
| 427342 | 4055 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 427342 | 4056 | `		if( rc != PH7_OK ){` |
|      - | 4057 | `			/* Allocation failed: surface a fatal instead of returning a` |
|      - | 4058 | `			 * silently-truncated string with a success status. */` |
|    ! 0 | 4059 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 4060 | `		}` |
| 427342 | 4061 | `		nMul--;` |
|      2 | 4062 | `	}` |
|  20438 | 4063 | `	return PH7_OK;` |
|  10221 | 4064 | `}` |
|      - | 4065 | `/*` |
|      - | 4066 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 4067 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 4068 | ` * Parameters` |
|      - | 4069 | ` *  $string` |
|      - | 4070 | ` *   The input string.` |
|      - | 4071 | ` * $is_xhtml` |
|      - | 4072 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 4073 | ` * Return` |
|      - | 4074 | ` *  The processed string.` |
|      - | 4075 | ` */` |
|      4 | 4076 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4077 | `{` |
|      - | 4078 | `	const char *zIn,*zCur,*zEnd;` |
|      5 | 4079 | `	int is_xhtml = 1; /* Default to XHTML-style '<br />' like PHP */` |
|      - | 4080 | `	int nLen;` |
|      5 | 4081 | `	if( nArg < 1 ){` |
|      - | 4082 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4083 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4084 | `		return PH7_OK;` |
|      - | 4085 | `	}` |
|      - | 4086 | `	/* Extract the target string */` |
|      5 | 4087 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 4088 | `	if( nLen < 1 ){` |
|      - | 4089 | `		/* Empty string,return null */` |
|    ! 0 | 4090 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4091 | `		return PH7_OK;` |
|      - | 4092 | `	}` |
|      5 | 4093 | `	if( nArg > 1 ){` |
|      3 | 4094 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 4095 | `	}` |
|      5 | 4096 | `	zEnd = &zIn[nLen];` |
|      - | 4097 | `	/* Perform the requested operation */` |
|      4 | 4098 | `	for(;;){` |
|      9 | 4099 | `		zCur = zIn;` |
|      - | 4100 | `		/* Delimit the string */` |
|     21 | 4101 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 4102 | `			zIn++;` |
|      1 | 4103 | `		}` |
|      9 | 4104 | `		if( zCur < zIn ){` |
|      - | 4105 | `			/* Output chunk verbatim */` |
|      9 | 4106 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 4107 | `		}` |
|      9 | 4108 | `		if( zIn >= zEnd ){` |
|      - | 4109 | `			/* No more input to process */` |
|      5 | 4110 | `			break;` |
|      - | 4111 | `		}` |
|      - | 4112 | `		/* Output the HTML line break */` |
|      - | 4113 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br />' (space before the slash), otherwise use '<br>' */` |
|      5 | 4114 | `		if( is_xhtml ){` |
|      3 | 4115 | `			ph7_result_string(pCtx,"<br />",(int)sizeof("<br />")-1);` |
|      2 | 4116 | `		}else{` |
|      3 | 4117 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 4118 | `		}` |
|      5 | 4119 | `		zCur = zIn;` |
|      - | 4120 | `		/* Append trailing line */` |
|     11 | 4121 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 4122 | `			zIn++;` |
|      1 | 4123 | `		}` |
|      5 | 4124 | `		if( zCur < zIn ){` |
|      - | 4125 | `			/* Output chunk verbatim */` |
|      5 | 4126 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 4127 | `		}` |
|      1 | 4128 | `	}` |
|      5 | 4129 | `	return PH7_OK;` |
|      3 | 4130 | `}` |
|      - | 4131 | `/*` |
|      - | 4132 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 4133 | ` *  According to the PHP reference manual.` |
|      - | 4134 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 4135 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 4136 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 4137 | ` * This applies to both sprintf() and printf().` |
|      - | 4138 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 4139 | ` * or more of these elements, in order:` |
|      - | 4140 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 4141 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 4142 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 4143 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 4144 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 4145 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 4146 | ` *   it with a single quote ('). See the examples below.` |
|      - | 4147 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 4148 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 4149 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 4150 | ` *   should result in.` |
|      - | 4151 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 4152 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 4153 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 4154 | ` *   limit to the string.` |
|      - | 4155 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 4156 | ` *       % - a literal percent character. No argument is required.` |
|      - | 4157 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 4158 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 4159 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 4160 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 4161 | ` * 	     for the number of digits after the decimal point.` |
|      - | 4162 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 4163 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 4164 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 4165 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 4166 | ` *       g - shorter of %e and %f.` |
|      - | 4167 | ` *       G - shorter of %E and %f.` |
|      - | 4168 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 4169 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 4170 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 4171 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 4172 | ` */` |
|      - | 4173 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 4174 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 4175 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 4176 | `/*` |
|      - | 4177 | `** Conversion types fall into various categories as defined by the` |
|      - | 4178 | `** following enumeration.` |
|      - | 4179 | `*/` |
|      - | 4180 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 4181 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 4182 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 4183 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 4184 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 4185 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 4186 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 4187 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 4188 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 4189 |  |
|      - | 4190 | `/*` |
|      - | 4191 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 4192 | `*/` |
|      - | 4193 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 4194 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 4195 | `/*` |
|      - | 4196 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 4197 | `** by an instance of the following structure` |
|      - | 4198 | `*/` |
|      - | 4199 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 4200 | `struct ph7_fmt_info` |
|      - | 4201 | `{` |
|      - | 4202 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 4203 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 4204 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 4205 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 4206 | `  char *charset; /* The character set for conversion */` |
|      - | 4207 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 4208 | `};` |
|      - | 4209 | `/* PH7_PhpFloatShape (php's float-shape post-processing) lives in memobj.c —` |
|      - | 4210 | ` * the default float->string cast needs it even when this whole formatting` |
|      - | 4211 | ` * region is compiled out by PH7_DISABLE_DISK_IO. */` |
|      - | 4212 | `/*` |
|      - | 4213 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 4214 | ` * used conversion types first.` |
|      - | 4215 | ` */` |
|      - | 4216 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 4217 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 4218 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 4219 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 4220 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 4221 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 4222 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 4223 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 4224 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 4225 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 4226 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 4227 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 4228 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 4229 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 4230 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 4231 | `  /* php's 'h'/'H' are the locale-independent twins of 'g'/'G'; PHL always` |
|      - | 4232 | `   * formats in the C locale, so they behave identically. */` |
|      - | 4233 | `  {  'h',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 4234 | `  {  'H',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 4235 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 4236 | `};` |
|      - | 4237 | `/*` |
|      - | 4238 | ` * PHP 8 raises a catchable ValueError for an unknown conversion specifier` |
|      - | 4239 | ` * (e.g. "%y", or the C-ism "%#x" — '#' is not a php flag). Because printf()` |
|      - | 4240 | ` * and fprintf() stream their output incrementally while sprintf() buffers it,` |
|      - | 4241 | ` * every format builtin calls PH7_FormatValidate (below) to check the whole` |
|      - | 4242 | ` * format string BEFORE formatting so the throw happens with no partial output` |
|      - | 4243 | ` * escaping (php buffers the entire result and only emits it on success). This` |
|      - | 4244 | ` * scan mirrors the specifier-locating logic of the main format loop below.` |
|      - | 4245 | ` * On the first unknown specifier, stores it in *pBad and returns TRUE; returns` |
|      - | 4246 | ` * FALSE when every specifier is known. (A found-flag rather than a sentinel` |
|      - | 4247 | ` * char, so a NUL specifier byte — "%\0" — is still reported, not mistaken for` |
|      - | 4248 | ` * "all valid".)` |
|      - | 4249 | ` */` |
|    498 | 4250 | `static int FormatUnknownSpec(const char *zIn,int nByte,int *pBad)` |
|      3 | 4251 | `{` |
|    501 | 4252 | `	const char *zEnd = &zIn[nByte];` |
|      - | 4253 | `	int c,idx;` |
|   3865 | 4254 | `	while( zIn < zEnd ){` |
|   3387 | 4255 | `		if( zIn[0] != '%' ){` |
|   2429 | 4256 | `			zIn++;` |
|   2429 | 4257 | `			continue;` |
|      - | 4258 | `		}` |
|    959 | 4259 | `		zIn++; /* jump the percent sign */` |
|      - | 4260 | `		/* php-supported flags: '-', '+', ' ', '0' and the "'<pad>'" custom-pad` |
|      - | 4261 | `		 * form. '#' is intentionally NOT treated as a flag so it surfaces as an` |
|      - | 4262 | `		 * unknown specifier, matching php. */` |
|   1199 | 4263 | `		while( zIn < zEnd ){` |
|   1197 | 4264 | `			c = zIn[0];` |
|   1197 | 4265 | `			if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|    229 | 4266 | `				zIn++;` |
|    229 | 4267 | `				continue;` |
|      - | 4268 | `			}` |
|    969 | 4269 | `			if( c=='\'' ){` |
|     13 | 4270 | `				zIn++;` |
|     13 | 4271 | `				if( zIn < zEnd ){` |
|     13 | 4272 | `					zIn++; /* the custom pad character */` |
|      6 | 4273 | `				}` |
|     13 | 4274 | `				continue;` |
|      - | 4275 | `			}` |
|    957 | 4276 | `			break;` |
|    ! 0 | 4277 | `		}` |
|      - | 4278 | `		/* field width */` |
|   1273 | 4279 | `		while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|    315 | 4280 | `			zIn++;` |
|      1 | 4281 | `		}` |
|      - | 4282 | `		/* positional specifier ($) — php parses flags AFTER it (e.g. "%1$-10s"),` |
|      - | 4283 | `		 * so skip the full flag set and width again, mirroring the main loop. */` |
|    959 | 4284 | `		if( zIn < zEnd && zIn[0]=='$' ){` |
|     15 | 4285 | `			zIn++;` |
|     17 | 4286 | `			while( zIn < zEnd ){` |
|     17 | 4287 | `				c = zIn[0];` |
|     17 | 4288 | `				if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|    ! 0 | 4289 | `					zIn++;` |
|    ! 0 | 4290 | `					continue;` |
|      - | 4291 | `				}` |
|     17 | 4292 | `				if( c=='\'' ){` |
|      3 | 4293 | `					zIn++;` |
|      3 | 4294 | `					if( zIn < zEnd ){` |
|      3 | 4295 | `						zIn++;` |
|      1 | 4296 | `					}` |
|      3 | 4297 | `					continue;` |
|      - | 4298 | `				}` |
|     15 | 4299 | `				break;` |
|    ! 0 | 4300 | `			}` |
|     23 | 4301 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|      9 | 4302 | `				zIn++;` |
|      1 | 4303 | `			}` |
|      7 | 4304 | `		}` |
|      - | 4305 | `		/* precision */` |
|    959 | 4306 | `		if( zIn < zEnd && zIn[0]=='.' ){` |
|    113 | 4307 | `			zIn++;` |
|    243 | 4308 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|    133 | 4309 | `				zIn++;` |
|      3 | 4310 | `			}` |
|     55 | 4311 | `		}` |
|      - | 4312 | `		/* a single 'l' length modifier (ignored, php compat) */` |
|    959 | 4313 | `		if( zIn < zEnd && zIn[0]=='l' ){` |
|     11 | 4314 | `			zIn++;` |
|      5 | 4315 | `		}` |
|    959 | 4316 | `		if( zIn >= zEnd ){` |
|      - | 4317 | `			/* A dangling '%' with no specifier: PHL's legacy path silently` |
|      - | 4318 | `			 * truncates here (recorded residual); nothing to validate. */` |
|      3 | 4319 | `			break;` |
|      - | 4320 | `		}` |
|    957 | 4321 | `		c = zIn[0];` |
|    957 | 4322 | `		zIn++; /* jump the conversion specifier */` |
|   3801 | 4323 | `		for( idx = 0 ; idx < (int)SX_ARRAYSIZE(aFmt) ; idx++ ){` |
|   3783 | 4324 | `			if( c == aFmt[idx].fmttype ){` |
|    939 | 4325 | `				break;` |
|      - | 4326 | `			}` |
|   1425 | 4327 | `		}` |
|    957 | 4328 | `		if( idx >= (int)SX_ARRAYSIZE(aFmt) ){` |
|     19 | 4329 | `			*pBad = c; /* unknown specifier */` |
|     19 | 4330 | `			return TRUE;` |
|      - | 4331 | `		}` |
|      3 | 4332 | `	}` |
|    483 | 4333 | `	return FALSE;` |
|    252 | 4334 | `}` |
|      - | 4335 | `/*` |
|      - | 4336 | ` * Validate a printf-style format string. PHP 8 raises a catchable ValueError for` |
|      - | 4337 | ` * an unknown conversion specifier, thrown before any output is produced. Every` |
|      - | 4338 | ` * format builtin (sprintf/printf/vprintf/vsprintf/fprintf/vfprintf) calls this` |
|      - | 4339 | ` * up-front, then propagates the returned status verbatim (PH7_EXCEPTION when the` |
|      - | 4340 | ` * throw is caught in place, PH7_ABORT when it goes uncaught).` |
|      - | 4341 | ` * Returns PH7_OK when the format is valid.` |
|      - | 4342 | ` */` |
|    498 | 4343 | `PH7_PRIVATE sxi32 PH7_FormatValidate(ph7_context *pCtx,const char *zFormat,int nByte)` |
|      3 | 4344 | `{` |
|    501 | 4345 | `	int badSpec = 0;` |
|    501 | 4346 | `	if( FormatUnknownSpec(zFormat,nByte,&badSpec) ){` |
|     28 | 4347 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      9 | 4348 | `			"Unknown format specifier \"%c\"",badSpec);` |
|      - | 4349 | `	}` |
|    483 | 4350 | `	return PH7_OK;` |
|    252 | 4351 | `}` |
|      - | 4352 | `/*` |
|      - | 4353 | ` * Count the number of VALUE arguments a format string needs: the greater of the` |
|      - | 4354 | ` * sequential (non-positional) conversion count and the highest positional index` |
|      - | 4355 | `` * (`%N$`). `%%` consumes nothing. Mirrors FormatUnknownSpec's specifier walk.`` |
|      - | 4356 | ` */` |
|    480 | 4357 | `static int FormatRequiredArgs(const char *zIn,int nByte)` |
|      3 | 4358 | `{` |
|    483 | 4359 | `	const char *zEnd = &zIn[nByte];` |
|    483 | 4360 | `	int c,seq = 0,maxpos = 0;` |
|   3833 | 4361 | `	while( zIn < zEnd ){` |
|   3355 | 4362 | `		int numVal = 0,pos = 0;` |
|   3355 | 4363 | `		if( zIn[0] != '%' ){` |
|   2415 | 4364 | `			zIn++;` |
|   2415 | 4365 | `			continue;` |
|      - | 4366 | `		}` |
|    941 | 4367 | `		zIn++; /* jump the percent sign */` |
|      - | 4368 | `		/* leading flags (incl. the "'<pad>'" custom-pad form) */` |
|   1181 | 4369 | `		while( zIn < zEnd ){` |
|   1179 | 4370 | `			c = zIn[0];` |
|   1179 | 4371 | `			if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){ zIn++; continue; }` |
|    951 | 4372 | `			if( c=='\'' ){ zIn++; if( zIn < zEnd ){ zIn++; } continue; }` |
|    939 | 4373 | `			break;` |
|    ! 0 | 4374 | `		}` |
|      - | 4375 | `		/* leading number: a positional index when a '$' follows, else the width */` |
|   1255 | 4376 | `		while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|    315 | 4377 | `			numVal = numVal*10 + (zIn[0]-'0');` |
|    315 | 4378 | `			zIn++;` |
|      1 | 4379 | `		}` |
|    941 | 4380 | `		if( zIn < zEnd && zIn[0]=='$' ){` |
|     15 | 4381 | `			pos = numVal;` |
|     15 | 4382 | `			zIn++;` |
|      - | 4383 | `			/* flags then width may follow the positional marker */` |
|     17 | 4384 | `			while( zIn < zEnd ){` |
|     17 | 4385 | `				c = zIn[0];` |
|     17 | 4386 | `				if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){ zIn++; continue; }` |
|     17 | 4387 | `				if( c=='\'' ){ zIn++; if( zIn < zEnd ){ zIn++; } continue; }` |
|     15 | 4388 | `				break;` |
|    ! 0 | 4389 | `			}` |
|     23 | 4390 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){ zIn++; }` |
|      7 | 4391 | `		}` |
|      - | 4392 | `		/* precision */` |
|    941 | 4393 | `		if( zIn < zEnd && zIn[0]=='.' ){` |
|    113 | 4394 | `			zIn++;` |
|    243 | 4395 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){ zIn++; }` |
|     55 | 4396 | `		}` |
|      - | 4397 | `		/* a single 'l' length modifier (ignored, php compat) */` |
|    941 | 4398 | `		if( zIn < zEnd && zIn[0]=='l' ){ zIn++; }` |
|    941 | 4399 | `		if( zIn >= zEnd ){ break; }` |
|    939 | 4400 | `		c = zIn[0];` |
|    939 | 4401 | `		zIn++; /* jump the conversion specifier */` |
|    939 | 4402 | `		if( c == '%' ){ continue; } /* %% consumes no argument */` |
|    931 | 4403 | `		if( pos > 0 ){` |
|     15 | 4404 | `			if( pos > maxpos ){ maxpos = pos; }` |
|      8 | 4405 | `		}else{` |
|    917 | 4406 | `			seq++;` |
|      - | 4407 | `		}` |
|      3 | 4408 | `	}` |
|    483 | 4409 | `	return seq > maxpos ? seq : maxpos;` |
|      3 | 4410 | `}` |
|      - | 4411 | `/*` |
|      - | 4412 | ` * PHP 8: a printf-family call with fewer VALUE arguments than the format needs` |
|      - | 4413 | ` * throws BEFORE any output. The non-vararg family (sprintf/printf/fprintf) raises` |
|      - | 4414 | ` * ArgumentCountError counting the format itself ("N arguments are required, M` |
|      - | 4415 | ` * given"); the vararg family (vsprintf/vprintf/vfprintf) raises a ValueError over` |
|      - | 4416 | ` * the values array ("The arguments array must contain N items, M given"). nValues` |
|      - | 4417 | ` * is the count of value arguments actually supplied; nFixed is the number of` |
|      - | 4418 | ` * fixed leading parameters counted in the ArgumentCountError totals (1 for the` |
|      - | 4419 | ` * $format of sprintf/printf, 2 for fprintf's $stream + $format — the vararg` |
|      - | 4420 | ` * ValueError counts only the array, so nFixed is ignored there). Returns PH7_OK` |
|      - | 4421 | ` * when enough.` |
|      - | 4422 | ` */` |
|    480 | 4423 | `PH7_PRIVATE sxi32 PH7_FormatCheckArgCount(ph7_context *pCtx,const char *zFormat,int nByte,int nValues,int nFixed,int bVararg)` |
|      3 | 4424 | `{` |
|    483 | 4425 | `	int required = FormatRequiredArgs(zFormat,nByte);` |
|    483 | 4426 | `	if( nValues < required ){` |
|     21 | 4427 | `		if( bVararg ){` |
|     10 | 4428 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      3 | 4429 | `				"The arguments array must contain %d items, %d given",required,nValues);` |
|      - | 4430 | `		}` |
|     22 | 4431 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|      7 | 4432 | `			"%d arguments are required, %d given",required+nFixed,nValues+nFixed);` |
|      - | 4433 | `	}` |
|    463 | 4434 | `	return PH7_OK;` |
|    243 | 4435 | `}` |
|      - | 4436 | `/*` |
|      - | 4437 | `` * PHP 8: a printf-family `$format` argument is a `string` parameter — scalars`` |
|      - | 4438 | ` * (int/float/bool) and null coerce to a string, but an array/object/resource` |
|      - | 4439 | ` * raises a catchable TypeError. iArg is the 1-based argument position ($format` |
|      - | 4440 | ` * is #1 for sprintf/printf/vprintf/vsprintf, #2 for fprintf/vfprintf). Returns` |
|      - | 4441 | ` * PH7_OK when the value is string-coercible (the caller then uses` |
|      - | 4442 | ` * ph7_value_to_string, which renders scalars/null verbatim).` |
|      - | 4443 | ` */` |
|      - | 4444 | `/*` |
|      - | 4445 | ` * php 8: a stream parameter that is not a resource is a TypeError, not a warning` |
|      - | 4446 | ` * with a 0 return -- the caller never learned its write went nowhere.` |
|      - | 4447 | ` */` |
|     24 | 4448 | `PH7_PRIVATE sxi32 PH7_CheckStreamArg(ph7_context *pCtx,ph7_value *pArg,int iArg,const char *zName)` |
|      1 | 4449 | `{` |
|     25 | 4450 | `	if( !ph7_value_is_resource(pArg) ){` |
|      - | 4451 | `		char zBuf[64];` |
|      4 | 4452 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4453 | `			"%s(): Argument #%d ($%s) must be of type resource, %s given",` |
|      1 | 4454 | `			ph7_function_name(pCtx),iArg,zName,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|      - | 4455 | `	}` |
|     23 | 4456 | `	return PH7_OK;` |
|     13 | 4457 | `}` |
|    510 | 4458 | `PH7_PRIVATE sxi32 PH7_FormatCheckFormatArg(ph7_context *pCtx,ph7_value *pArg,int iArg)` |
|      3 | 4459 | `{` |
|    513 | 4460 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg) \|\| ph7_value_is_resource(pArg) ){` |
|      - | 4461 | `		char zBuf[64];` |
|    ! 0 | 4462 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4463 | `			"%s(): Argument #%d ($format) must be of type string, %s given",` |
|    ! 0 | 4464 | `			ph7_function_name(pCtx),iArg,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|      - | 4465 | `	}` |
|    513 | 4466 | `	return PH7_OK;` |
|    258 | 4467 | `}` |
|      - | 4468 | `/*` |
|      - | 4469 | ` * Format a given string.` |
|      - | 4470 | ` * The root program.  All variations call this core.` |
|      - | 4471 | ` * INPUTS:` |
|      - | 4472 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 4473 | ` *            1. A pointer to the call context.` |
|      - | 4474 | ` *            2. A pointer to the list of characters to be output` |
|      - | 4475 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 4476 | ` *            3. An integer number of characters to be output.` |
|      - | 4477 | ` *               (Note: This number might be zero.)` |
|      - | 4478 | ` *            4. Upper layer private data.` |
|      - | 4479 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 4480 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 4481 | ` */` |
|    460 | 4482 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 4483 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 4484 | `	ph7_context *pCtx,  /* call context */` |
|      - | 4485 | `	const char *zIn,    /* Format string */` |
|      - | 4486 | `	int nByte,          /* Format string length */` |
|      - | 4487 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 4488 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 4489 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 4490 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 4491 | `	)` |
|      3 | 4492 | `{` |
|    463 | 4493 | `	char spaces[] = "                                                  ";` |
|      - | 4494 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    463 | 4495 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 4496 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 4497 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 4498 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 4499 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 4500 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 4501 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 4502 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 4503 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 4504 | `	ph7_int64 iVal;` |
|      - | 4505 | `	int precision;           /* Precision of the current field */` |
|      - | 4506 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 4507 | `	int c,rc,n;` |
|      - | 4508 | `	int length;              /* Length of the field */` |
|      - | 4509 | `	int prefix;` |
|      - | 4510 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 4511 | `	int width;               /* Width of the current field */` |
|      - | 4512 | `	int idx;` |
|    463 | 4513 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 4514 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 4515 | `	/* An unknown conversion specifier is rejected up-front by PH7_FormatValidate()` |
|      - | 4516 | `	 * (called by every format builtin before this routine), so the specifier set` |
|      - | 4517 | `	 * seen here is always valid. */` |
|      - | 4518 | `	/* Start the format process */` |
|    682 | 4519 | `	for(;;){` |
|   1367 | 4520 | `		zCur = zIn;` |
|   3773 | 4521 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|   2407 | 4522 | `			zIn++;` |
|      1 | 4523 | `		}` |
|   1367 | 4524 | `		if( zCur < zIn ){` |
|      - | 4525 | `			/* Consume chunk verbatim */` |
|    793 | 4526 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|    793 | 4527 | `			if( rc != SXRET_OK ){` |
|      - | 4528 | `				/* Callback requested an abort (e.g. an allocation failure) */` |
|    ! 0 | 4529 | `				break;` |
|      - | 4530 | `			}` |
|    396 | 4531 | `		}` |
|   1367 | 4532 | `		if( zIn >= zEnd ){` |
|      - | 4533 | `			/* No more input to process,break immediately */` |
|    461 | 4534 | `			break;` |
|      - | 4535 | `		}` |
|      - | 4536 | `		/* Find out what flags are present */` |
|    909 | 4537 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    906 | 4538 | `			flag_alternateform = flag_zeropad = 0;` |
|      - | 4539 | `		/* Reset the pad buffer to spaces: a custom pad char ('X) — or the string` |
|      - | 4540 | `		 * zero-pad below — from a PREVIOUS specifier must not bleed into this one.` |
|      - | 4541 | `		 * php resets the pad character for every specifier. */` |
|  46209 | 4542 | `		for( idx = 0 ; idx < etSPACESIZE ; ++idx ){ spaces[idx] = ' '; }` |
|    909 | 4543 | `		zIn++; /* Jump the precent sign */` |
|    453 | 4544 | `		do{` |
|   1149 | 4545 | `			c = zIn[0];` |
|   1149 | 4546 | `			switch( c ){` |
|     19 | 4547 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      7 | 4548 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 4549 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|    199 | 4550 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|      6 | 4551 | `			case '\'':` |
|     13 | 4552 | `				zIn++;` |
|     13 | 4553 | `				if( zIn < zEnd ){` |
|      - | 4554 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|     13 | 4555 | `					c = zIn[0];` |
|    613 | 4556 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    601 | 4557 | `						spaces[idx] = (char)c;` |
|    301 | 4558 | `					}` |
|     13 | 4559 | `					c = 0;` |
|      6 | 4560 | `				}` |
|     12 | 4561 | `				break;` |
|    906 | 4562 | `			default:                                       break;` |
|      - | 4563 | `			}` |
|   1149 | 4564 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 4565 | `		/* Get the field width */` |
|    909 | 4566 | `		width = 0;` |
|   1670 | 4567 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    309 | 4568 | `			width = width*10 + (zIn[0] - '0');` |
|    309 | 4569 | `			zIn++;` |
|      1 | 4570 | `		}` |
|    909 | 4571 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 4572 | `			/* Position specifer */` |
|      9 | 4573 | `			if( width > 0 ){` |
|      9 | 4574 | `				n = width;` |
|      9 | 4575 | `				if( vf && n > 0 ){` |
|    ! 0 | 4576 | `					n--;` |
|    ! 0 | 4577 | `				}` |
|      4 | 4578 | `			}` |
|      9 | 4579 | `			zIn++;` |
|      9 | 4580 | `			width = 0;` |
|      - | 4581 | `			/* php's grammar is %argnum$<flags><width>: the flags come AFTER the` |
|      - | 4582 | `			 * positional, so re-parse the full flag set here (e.g. "%1$-10s"),` |
|      - | 4583 | `			 * not just zero-padding. */` |
|      4 | 4584 | `			do{` |
|     11 | 4585 | `				c = zIn[0];` |
|     11 | 4586 | `				switch( c ){` |
|    ! 0 | 4587 | `				case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|    ! 0 | 4588 | `				case '+':   flag_plussign = 1;        c = 0;   break;` |
|    ! 0 | 4589 | `				case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|    ! 0 | 4590 | `				case '0':   flag_zeropad = 1;         c = 0;   break;` |
|      1 | 4591 | `				case '\'':` |
|      3 | 4592 | `					zIn++;` |
|      3 | 4593 | `					if( zIn < zEnd ){` |
|      3 | 4594 | `						c = zIn[0];` |
|    103 | 4595 | `						for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    101 | 4596 | `							spaces[idx] = (char)c;` |
|     51 | 4597 | `						}` |
|      3 | 4598 | `						c = 0;` |
|      1 | 4599 | `					}` |
|      2 | 4600 | `					break;` |
|      8 | 4601 | `				default:                                       break;` |
|      - | 4602 | `				}` |
|     11 | 4603 | `			}while( c==0 && (zIn++ < zEnd) );` |
|     21 | 4604 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|      9 | 4605 | `				width = width*10 + (zIn[0] - '0');` |
|      9 | 4606 | `				zIn++;` |
|      1 | 4607 | `			}` |
|      4 | 4608 | `		}` |
|    909 | 4609 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 4610 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 4611 | `		}` |
|      - | 4612 | `		/* Get the precision */` |
|    909 | 4613 | `		precision = -1;` |
|    909 | 4614 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|    113 | 4615 | `			precision = 0;` |
|    113 | 4616 | `			zIn++;` |
|    298 | 4617 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    133 | 4618 | `				precision = precision*10 + (zIn[0] - '0');` |
|    133 | 4619 | `				zIn++;` |
|      3 | 4620 | `			}` |
|     55 | 4621 | `		}` |
|      - | 4622 | `		/* Consume a single 'l' length modifier (a C-ism php accepts and ignores,` |
|      - | 4623 | `		 * e.g. "%ld"); PH7_FormatValidate mirrors this. Exactly one is skipped:` |
|      - | 4624 | `		 * in "%lld" the second 'l' becomes the (unknown) specifier, just like php. */` |
|    909 | 4625 | `		if( zIn < zEnd && zIn[0] == 'l' ){` |
|      9 | 4626 | `			zIn++;` |
|      4 | 4627 | `		}` |
|    909 | 4628 | `		if( zIn >= zEnd ){` |
|      - | 4629 | `			/* No more input */` |
|      3 | 4630 | `			break;` |
|      - | 4631 | `		}` |
|      - | 4632 | `		/* Fetch the info entry for the field */` |
|    907 | 4633 | `		pInfo = 0;` |
|    907 | 4634 | `		xtype = PH7_FMT_ERROR;` |
|    907 | 4635 | `		c = zIn[0];` |
|    907 | 4636 | `		zIn++; /* Jump the format specifer */` |
|   3431 | 4637 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|   3431 | 4638 | `			if( c==aFmt[idx].fmttype ){` |
|    907 | 4639 | `				pInfo = &aFmt[idx];` |
|    907 | 4640 | `				xtype = pInfo->type;` |
|    907 | 4641 | `				break;` |
|      - | 4642 | `			}` |
|   1265 | 4643 | `		}` |
|    907 | 4644 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    907 | 4645 | `		length = 0;` |
|      - | 4646 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 4647 | `		 /*` |
|      - | 4648 | `		  ** At this point, variables are initialized as follows:` |
|      - | 4649 | `		  **` |
|      - | 4650 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 4651 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 4652 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 4653 | `		  **                               field width was negative.` |
|      - | 4654 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 4655 | `		  **                               the conversion character.` |
|      - | 4656 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 4657 | `		  **   width                       The specified field width.  This is` |
|      - | 4658 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 4659 | `		  **   precision                   The specified precision.  The default` |
|      - | 4660 | `		  **                               is -1.` |
|      - | 4661 | `		  */` |
|    907 | 4662 | `		switch(xtype){` |
|      4 | 4663 | `		case PH7_FMT_PERCENT:` |
|      - | 4664 | `			/* A literal percent character */` |
|      9 | 4665 | `			zWorker[0] = '%';` |
|      9 | 4666 | `			length = (int)sizeof(char);` |
|      9 | 4667 | `			break;` |
|      2 | 4668 | `		case PH7_FMT_CHARX:` |
|      - | 4669 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 4670 | `			 * with that ASCII value` |
|      - | 4671 | `			 */` |
|      5 | 4672 | `			pArg = NEXT_ARG;` |
|      5 | 4673 | `			if( pArg == 0 ){` |
|    ! 0 | 4674 | `				c = 0;` |
|    ! 0 | 4675 | `			}else{` |
|      5 | 4676 | `				c = ph7_value_to_int(pArg);` |
|      - | 4677 | `			}` |
|      - | 4678 | `			/* NUL byte is an acceptable value */` |
|      5 | 4679 | `			zWorker[0] = (char)c;` |
|      5 | 4680 | `			length = (int)sizeof(char);` |
|      5 | 4681 | `			break;` |
|    188 | 4682 | `		case PH7_FMT_STRING:` |
|      - | 4683 | `			/* the argument is treated as and presented as a string */` |
|    377 | 4684 | `			pArg = NEXT_ARG;` |
|    377 | 4685 | `			if( pArg == 0 ){` |
|    ! 0 | 4686 | `				length = 0;` |
|    ! 0 | 4687 | `			}else{` |
|    377 | 4688 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 4689 | `			}` |
|    377 | 4690 | `			if( length < 1 ){` |
|      - | 4691 | `				/* An empty %s substitutes NOTHING in php. PH7 substituted a single` |
|      - | 4692 | `				 * SPACE here, so printf("[%s]","") printed "[ ]" and any format with an` |
|      - | 4693 | `				 * absent optional part gained a stray space. */` |
|      9 | 4694 | `				zBuf = "";` |
|      9 | 4695 | `				length = 0;` |
|      4 | 4696 | `			}` |
|    377 | 4697 | `			if( precision>=0 && precision<length ){` |
|      3 | 4698 | `				length = precision;` |
|      1 | 4699 | `			}` |
|    377 | 4700 | `			if( flag_zeropad ){` |
|      - | 4701 | `				/* zero-padding works on strings too */` |
|    103 | 4702 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    101 | 4703 | `					spaces[idx] = '0';` |
|     51 | 4704 | `				}` |
|      1 | 4705 | `			}` |
|    377 | 4706 | `			break;` |
|    158 | 4707 | `		case PH7_FMT_RADIX:` |
|    317 | 4708 | `			pArg = NEXT_ARG;` |
|    317 | 4709 | `			if( pArg == 0 ){` |
|    ! 0 | 4710 | `				iVal = 0;` |
|    ! 0 | 4711 | `			}else{` |
|    317 | 4712 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 4713 | `			}` |
|      - | 4714 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|    317 | 4715 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 4716 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 4717 | `			}` |
|      - | 4718 | `#if 1` |
|      - | 4719 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 4720 | `        ** I think this is stupid.*/` |
|    317 | 4721 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 4722 | `#else` |
|      - | 4723 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 4724 | `        ** but leave the prefix for hex.*/` |
|      - | 4725 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 4726 | `#endif` |
|    317 | 4727 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|    293 | 4728 | `          if( iVal<0 ){` |
|     25 | 4729 | `            iVal = -iVal;` |
|      - | 4730 | `			/* Ticket 1433-003 */` |
|     25 | 4731 | `			if( iVal < 0 ){` |
|      - | 4732 | `				/* Overflow */` |
|    ! 0 | 4733 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4734 | `			}` |
|     25 | 4735 | `            prefix = '-';` |
|    281 | 4736 | `          }else if( flag_plussign )  prefix = '+';` |
|    267 | 4737 | `          else if( flag_blanksign )  prefix = ' ';` |
|    265 | 4738 | `          else                       prefix = 0;` |
|    147 | 4739 | `        }else{` |
|     25 | 4740 | `			if( iVal<0 ){` |
|    ! 0 | 4741 | `				iVal = -iVal;` |
|      - | 4742 | `				/* Ticket 1433-003 */` |
|    ! 0 | 4743 | `				if( iVal < 0 ){` |
|      - | 4744 | `					/* Overflow */` |
|    ! 0 | 4745 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4746 | `				}` |
|    ! 0 | 4747 | `			}` |
|     25 | 4748 | `			prefix = 0;` |
|      - | 4749 | `		}` |
|    317 | 4750 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|    185 | 4751 | `          precision = width-(prefix!=0);` |
|     92 | 4752 | `        }` |
|    317 | 4753 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 4754 | `        {` |
|      - | 4755 | `          register char *cset;      /* Use registers for speed */` |
|      - | 4756 | `          register int base;` |
|    317 | 4757 | `          cset = pInfo->charset;` |
|    317 | 4758 | `          base = pInfo->base;` |
|    158 | 4759 | `          do{                                           /* Convert to ascii */` |
|    393 | 4760 | `            *(--zBuf) = cset[iVal%base];` |
|    393 | 4761 | `            iVal = iVal/base;` |
|    393 | 4762 | `          }while( iVal>0 );` |
|      - | 4763 | `        }` |
|    317 | 4764 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    519 | 4765 | `        for(idx=precision-length; idx>0; idx--){` |
|    203 | 4766 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|    102 | 4767 | `        }` |
|    317 | 4768 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|    317 | 4769 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 4770 | `          char *pre, x;` |
|    ! 0 | 4771 | `          pre = pInfo->prefix;` |
|    ! 0 | 4772 | `          if( *zBuf!=pre[0] ){` |
|    ! 0 | 4773 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|    ! 0 | 4774 | `          }` |
|    ! 0 | 4775 | `        }` |
|    317 | 4776 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    317 | 4777 | `		break;` |
|    100 | 4778 | `		case PH7_FMT_FLOAT:` |
|      - | 4779 | `		case PH7_FMT_EXP:` |
|      - | 4780 | `		case PH7_FMT_GENERIC:{` |
|      - | 4781 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 4782 | `		double realvalue;` |
|      - | 4783 | `		char zFmt[8];` |
|      - | 4784 | `		int nOut, nFmt;` |
|    203 | 4785 | `		pArg = NEXT_ARG;` |
|    203 | 4786 | `		if( pArg == 0 ){` |
|    ! 0 | 4787 | `			realvalue = 0;` |
|    ! 0 | 4788 | `		}else{` |
|    203 | 4789 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 4790 | `		}` |
|      - | 4791 | `		/* php prints the IEEE specials bare — NaN / INF / -INF with no width` |
|      - | 4792 | `		 * padding, precision, or sign flags (php_sprintf_appenddouble). */` |
|    203 | 4793 | `		if( PH7_IS_NAN(realvalue) ){` |
|     21 | 4794 | `			zBuf = "NaN";` |
|     21 | 4795 | `			length = 3;` |
|     21 | 4796 | `			width = 0;` |
|     21 | 4797 | `			break;` |
|      - | 4798 | `		}` |
|    183 | 4799 | `		if( PH7_IS_INF(realvalue) ){` |
|     37 | 4800 | `			if( realvalue < 0.0 ){` |
|     15 | 4801 | `				zBuf = "-INF";` |
|     15 | 4802 | `				length = 4;` |
|      8 | 4803 | `			}else{` |
|     23 | 4804 | `				zBuf = "INF";` |
|     23 | 4805 | `				length = 3;` |
|      - | 4806 | `			}` |
|     37 | 4807 | `			width = 0;` |
|     37 | 4808 | `			break;` |
|      - | 4809 | `		}` |
|    147 | 4810 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|    147 | 4811 | `		if( precision > 53 ){` |
|      - | 4812 | `			/* php's FORMAT_CONV_MAX_PRECISION cap, with the same E_NOTICE` |
|      - | 4813 | `			 * (message prefixed with the active function's name, like` |
|      - | 4814 | `			 * php_error_docref). */` |
|      - | 4815 | `			char zMsg[160];` |
|      4 | 4816 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - | 4817 | `				"%z(): Requested precision of %d digits was truncated to PHP maximum of %d digits",` |
|      2 | 4818 | `				&pCtx->pFunc->sName,precision,53);` |
|      3 | 4819 | `			PH7_VmThrowError(pCtx->pVm,0,E_NOTICE,zMsg);` |
|      3 | 4820 | `			precision = 53;` |
|      1 | 4821 | `		}` |
|      - | 4822 | ``		/* php's %f/%e extract the sign via `num < 0`, so negative zero prints`` |
|      - | 4823 | `		 * unsigned there — while %g (php_gcvt on the raw value) keeps "-0". */` |
|    147 | 4824 | `		if( xtype!=PH7_FMT_GENERIC && realvalue == 0.0 ){` |
|      9 | 4825 | `			realvalue = 0.0;` |
|      4 | 4826 | `		}` |
|      - | 4827 | `		/* php's float conversions are correctly rounded (zend_dtoa); use libc` |
|      - | 4828 | `		 * snprintf as the digit engine (the byte-exact-floats rule — the old` |
|      - | 4829 | `		 * hand-rolled vxGetdigit loop stopped at 16 significant digits, so` |
|      - | 4830 | `		 * e.g. %f of 1e308 printed zeros where php prints the exact binary64` |
|      - | 4831 | `		 * expansion), then post-process into php's exact shapes below. */` |
|    147 | 4832 | `		nFmt = 0;` |
|    147 | 4833 | `		zFmt[nFmt++] = '%';` |
|    147 | 4834 | `		if( flag_alternateform ) zFmt[nFmt++] = '#';` |
|      - | 4835 | `		/* php's ' ' flag selects space PADDING (its default), not C's` |
|      - | 4836 | `		 * space-for-positive-sign — so flag_blanksign is NOT forwarded. */` |
|    147 | 4837 | `		if( flag_plussign ) zFmt[nFmt++] = '+';` |
|    147 | 4838 | `		zFmt[nFmt++] = '.';` |
|    147 | 4839 | `		zFmt[nFmt++] = '*';` |
|    195 | 4840 | `		zFmt[nFmt++] = (char)(xtype==PH7_FMT_FLOAT ? 'f' :` |
|     32 | 4841 | `			(xtype==PH7_FMT_EXP ? ((pInfo->charset[0]=='E') ? 'E' : 'e')` |
|     32 | 4842 | `			                    : ((pInfo->charset[0]=='E') ? 'G' : 'g')));` |
|    147 | 4843 | `		zFmt[nFmt] = 0;` |
|    147 | 4844 | `		nOut = snprintf(zWorker,sizeof(zWorker),zFmt,precision,realvalue);` |
|    147 | 4845 | `		if( nOut < 0 \|\| nOut >= (int)sizeof(zWorker) ){` |
|      - | 4846 | `			/* Cannot happen with precision capped at 53 (%f of DBL_MAX is` |
|      - | 4847 | `			 * ~365 bytes); keep the truncated output rather than overrun. */` |
|    ! 0 | 4848 | `			nOut = (int)SyStrlen(zWorker);` |
|    ! 0 | 4849 | `		}` |
|    147 | 4850 | `		nOut = (int)PH7_PhpFloatShape(zWorker,(sxi32)nOut,xtype==PH7_FMT_GENERIC);` |
|    147 | 4851 | `		zBuf = zWorker;` |
|    147 | 4852 | `		length = nOut;` |
|      - | 4853 | `		/* Let the zero-pad block below insert zeros between the sign (written` |
|      - | 4854 | `		 * by snprintf) and the first digit, as before. */` |
|    147 | 4855 | `		prefix = (zWorker[0]=='-' \|\| zWorker[0]=='+' \|\| zWorker[0]==' ') ? zWorker[0] : 0;` |
|      - | 4856 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 4857 | `        ** set and we are not left justified */` |
|    147 | 4858 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 4859 | `          int i;` |
|      9 | 4860 | `          int nPad = width - length;` |
|     63 | 4861 | `          for(i=width; i>=nPad; i--){` |
|     55 | 4862 | `            zBuf[i] = zBuf[i-nPad];` |
|     28 | 4863 | `          }` |
|      9 | 4864 | `          i = prefix!=0;` |
|     39 | 4865 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      9 | 4866 | `          length = width;` |
|      4 | 4867 | `        }` |
|      - | 4868 | `#else` |
|      - | 4869 | `         zBuf = " ";` |
|      - | 4870 | `		 length = (int)sizeof(char);` |
|      - | 4871 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|    147 | 4872 | `		 break;` |
|      - | 4873 | `							 }` |
|    ! 0 | 4874 | `		default:` |
|      - | 4875 | `			/* Unreachable: PH7_FormatValidate() rejects unknown specifiers with a` |
|      - | 4876 | `			 * catchable ValueError before formatting begins. Kept as a defensive` |
|      - | 4877 | `			 * no-op that emits nothing. */` |
|    ! 0 | 4878 | `			length = 0;` |
|    ! 0 | 4879 | `			break;` |
|      - | 4880 | `		}` |
|      - | 4881 | `		 /*` |
|      - | 4882 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 4883 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 4884 | `		 ** the output.` |
|      - | 4885 | `		 */` |
|    907 | 4886 | `    if( !flag_leftjustify ){` |
|      - | 4887 | `      register int nspace;` |
|    889 | 4888 | `      nspace = width-length;` |
|    889 | 4889 | `      if( nspace>0 ){` |
|     37 | 4890 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4891 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4892 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4893 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4894 | `			}` |
|    ! 0 | 4895 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4896 | `        }` |
|     37 | 4897 | `        if( nspace>0 ){` |
|     37 | 4898 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|     37 | 4899 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4900 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4901 | `			}` |
|     18 | 4902 | `		}` |
|     18 | 4903 | `      }` |
|    443 | 4904 | `    }` |
|    907 | 4905 | `    if( length>0 ){` |
|    899 | 4906 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    899 | 4907 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 4908 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4909 | `		}` |
|    448 | 4910 | `    }` |
|    907 | 4911 | `    if( flag_leftjustify ){` |
|      - | 4912 | `      register int nspace;` |
|     19 | 4913 | `      nspace = width-length;` |
|     19 | 4914 | `      if( nspace>0 ){` |
|     15 | 4915 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4916 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4917 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4918 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4919 | `			}` |
|    ! 0 | 4920 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4921 | `        }` |
|     15 | 4922 | `        if( nspace>0 ){` |
|     15 | 4923 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|     15 | 4924 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4925 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4926 | `			}` |
|      7 | 4927 | `		}` |
|      7 | 4928 | `      }` |
|      9 | 4929 | `    }` |
|      3 | 4930 | ` }/* for(;;) */` |
|    463 | 4931 | `	return SXRET_OK;` |
|    233 | 4932 | `}` |
|      - | 4933 | `/*` |
|      - | 4934 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 4935 | ` */` |
|    534 | 4936 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      3 | 4937 | `{` |
|      - | 4938 | `	/* pUserData points to the caller's allocation-rc slot so an OOM during the` |
|      - | 4939 | `	 * result append is surfaced (the builtin raises a fatal); returning the` |
|      - | 4940 | `	 * non-OK rc also stops the format loop. */` |
|    537 | 4941 | `	sxi32 *pRc = (sxi32 *)pUserData;` |
|    537 | 4942 | `	*pRc = ph7_result_string(pCtx,zInput,nLen);` |
|    537 | 4943 | `	return *pRc;` |
|      3 | 4944 | `}` |
|      - | 4945 | `/*` |
|      - | 4946 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 4947 | ` *  Return a formatted string.` |
|      - | 4948 | ` * Parameters` |
|      - | 4949 | ` *  $format` |
|      - | 4950 | ` *    The format string (see block comment above)` |
|      - | 4951 | ` * Return` |
|      - | 4952 | ` *  A string produced according to the formatting string format.` |
|      - | 4953 | ` */` |
|    254 | 4954 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4955 | `{` |
|      - | 4956 | `	const char *zFormat;` |
|    257 | 4957 | `	sxi32 rc = SXRET_OK;` |
|      - | 4958 | `	int nLen;` |
|    257 | 4959 | `	if( nArg < 1 ){` |
|      - | 4960 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4961 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4962 | `		return PH7_OK;` |
|      - | 4963 | `	}` |
|      - | 4964 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|    257 | 4965 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    257 | 4966 | `	if( rc != PH7_OK ){` |
|    ! 0 | 4967 | `		return rc;` |
|      - | 4968 | `	}` |
|      - | 4969 | `	/* Extract the string format (scalars/null coerce). */` |
|    257 | 4970 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    257 | 4971 | `	if( nLen < 1 ){` |
|      - | 4972 | `		/* Empty string */` |
|    ! 0 | 4973 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4974 | `		return PH7_OK;` |
|      - | 4975 | `	}` |
|      - | 4976 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4977 | `	 * output; propagate the throw status verbatim. */` |
|    257 | 4978 | `	rc = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    257 | 4979 | `	if( rc != PH7_OK ){` |
|     17 | 4980 | `		return rc;` |
|      - | 4981 | `	}` |
|      - | 4982 | `	/* PHP 8: too few value arguments is a catchable ArgumentCountError before output. */` |
|    241 | 4983 | `	rc = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg-1,1,FALSE);` |
|    241 | 4984 | `	if( rc != PH7_OK ){` |
|     11 | 4985 | `		return rc;` |
|      - | 4986 | `	}` |
|      - | 4987 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|    231 | 4988 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&rc,FALSE);` |
|    231 | 4989 | `	if( rc != SXRET_OK ){` |
|      - | 4990 | `		/* The result append ran out of memory: raise a fatal rather than` |
|      - | 4991 | `		 * returning a silently-truncated string. */` |
|    ! 0 | 4992 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4993 | `	}` |
|    231 | 4994 | `	return PH7_OK;` |
|    130 | 4995 | `}` |
|      - | 4996 | `/*` |
|      - | 4997 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 4998 | ` */` |
|   1174 | 4999 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 5000 | `{` |
|   1175 | 5001 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 5002 | `	/* Call the VM output consumer directly */` |
|   1175 | 5003 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 5004 | `	/* Increment counter */` |
|   1175 | 5005 | `	*pCounter += nLen;` |
|   1175 | 5006 | `	return PH7_OK;` |
|      1 | 5007 | `}` |
|      - | 5008 | `/*` |
|      - | 5009 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 5010 | ` *  Output a formatted string.` |
|      - | 5011 | ` * Parameters` |
|      - | 5012 | ` *  $format` |
|      - | 5013 | ` *   See sprintf() for a description of format.` |
|      - | 5014 | ` * Return` |
|      - | 5015 | ` *  The length of the outputted string.` |
|      - | 5016 | ` */` |
|    206 | 5017 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5018 | `{` |
|    207 | 5019 | `	ph7_int64 nCounter = 0;` |
|      - | 5020 | `	const char *zFormat;` |
|      - | 5021 | `	int nLen;` |
|    207 | 5022 | `	if( nArg < 1 ){` |
|      - | 5023 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 5024 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5025 | `		return PH7_OK;` |
|      - | 5026 | `	}` |
|      - | 5027 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|      - | 5028 | `	{` |
|    207 | 5029 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    207 | 5030 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 5031 | `			return rcf;` |
|      - | 5032 | `		}` |
|      - | 5033 | `	}` |
|      - | 5034 | `	/* Extract the string format (scalars/null coerce). */` |
|    207 | 5035 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    207 | 5036 | `	if( nLen < 1 ){` |
|      - | 5037 | `		/* Empty string */` |
|    ! 0 | 5038 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5039 | `		return PH7_OK;` |
|      - | 5040 | `	}` |
|      - | 5041 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 5042 | `	 * output; propagate the throw status verbatim. */` |
|      - | 5043 | `	{` |
|    207 | 5044 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    207 | 5045 | `		if( rcv != PH7_OK ){` |
|    ! 0 | 5046 | `			return rcv;` |
|      - | 5047 | `		}` |
|      - | 5048 | `		/* PHP 8: too few value arguments is a catchable ArgumentCountError before output. */` |
|    207 | 5049 | `		rcv = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg-1,1,FALSE);` |
|    207 | 5050 | `		if( rcv != PH7_OK ){` |
|      3 | 5051 | `			return rcv;` |
|      - | 5052 | `		}` |
|      - | 5053 | `	}` |
|      - | 5054 | `	/* Format the string */` |
|    205 | 5055 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 5056 | `	/* Return the length of the outputted string */` |
|    205 | 5057 | `	ph7_result_int64(pCtx,nCounter);` |
|    205 | 5058 | `	return PH7_OK;` |
|    104 | 5059 | `}` |
|      - | 5060 | `/*` |
|      - | 5061 | ` * int vprintf(string $format,array $args)` |
|      - | 5062 | ` *  Output a formatted string.` |
|      - | 5063 | ` * Parameters` |
|      - | 5064 | ` *  $format` |
|      - | 5065 | ` *   See sprintf() for a description of format.` |
|      - | 5066 | ` * Return` |
|      - | 5067 | ` *  The length of the outputted string.` |
|      - | 5068 | ` */` |
|      4 | 5069 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5070 | `{` |
|      5 | 5071 | `	ph7_int64 nCounter = 0;` |
|      - | 5072 | `	const char *zFormat;` |
|      - | 5073 | `	ph7_hashmap *pMap;` |
|      - | 5074 | `	SySet sArg;` |
|      - | 5075 | `	int nLen,n;` |
|      - | 5076 | `	sxi32 rcFmt;` |
|      5 | 5077 | `	if( nArg < 2 ){` |
|      - | 5078 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 5079 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5080 | `		return PH7_OK;` |
|      - | 5081 | `	}` |
|      - | 5082 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|      5 | 5083 | `	rcFmt = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|      5 | 5084 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 5085 | `		return rcFmt;` |
|      - | 5086 | `	}` |
|      5 | 5087 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 5088 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 5089 | `		char zBuf[64];` |
|      4 | 5090 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5091 | `			"vprintf(): Argument #2 ($values) must be of type array, %s given",` |
|      2 | 5092 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 5093 | `	}` |
|      - | 5094 | `	/* Extract the string format (scalars/null coerce). */` |
|      3 | 5095 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5096 | `	if( nLen < 1 ){` |
|      - | 5097 | `		/* Empty string */` |
|    ! 0 | 5098 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5099 | `		return PH7_OK;` |
|      - | 5100 | `	}` |
|      - | 5101 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 5102 | `	 * output; propagate the throw status verbatim. */` |
|      3 | 5103 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      3 | 5104 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 5105 | `		return rcFmt;` |
|      - | 5106 | `	}` |
|      - | 5107 | `	/* Point to the hashmap */` |
|      3 | 5108 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 5109 | `	/* PHP 8: too few items in the $values array is a catchable ValueError before output.` |
|      - | 5110 | `	 * Checked on the entry count before materialising the value set. */` |
|      3 | 5111 | `	rcFmt = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);` |
|      3 | 5112 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 5113 | `		return rcFmt;` |
|      - | 5114 | `	}` |
|      - | 5115 | `	/* Extract arguments from the hashmap */` |
|      3 | 5116 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 5117 | `	/* Format the string */` |
|      3 | 5118 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 5119 | `	/* Release the container */` |
|      3 | 5120 | `	SySetRelease(&sArg);` |
|      - | 5121 | `	/* Return the length of the outputted string */` |
|      3 | 5122 | `	ph7_result_int64(pCtx,nCounter);` |
|      3 | 5123 | `	return PH7_OK;` |
|      3 | 5124 | `}` |
|      - | 5125 | `/*` |
|      - | 5126 | ` * int vsprintf(string $format,array $args)` |
|      - | 5127 | ` *  Output a formatted string.` |
|      - | 5128 | ` * Parameters` |
|      - | 5129 | ` *  $format` |
|      - | 5130 | ` *   See sprintf() for a description of format.` |
|      - | 5131 | ` * Return` |
|      - | 5132 | ` *  A string produced according to the formatting string format.` |
|      - | 5133 | ` */` |
|     24 | 5134 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5135 | `{` |
|      - | 5136 | `	const char *zFormat;` |
|      - | 5137 | `	ph7_hashmap *pMap;` |
|      - | 5138 | `	SySet sArg;` |
|     25 | 5139 | `	sxi32 rc = SXRET_OK;` |
|      - | 5140 | `	sxi32 rcFmt;` |
|      - | 5141 | `	int nLen,n;` |
|     25 | 5142 | `	if( nArg < 2 ){` |
|      - | 5143 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5144 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5145 | `		return PH7_OK;` |
|      - | 5146 | `	}` |
|      - | 5147 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|     25 | 5148 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|     25 | 5149 | `	if( rc != PH7_OK ){` |
|    ! 0 | 5150 | `		return rc;` |
|      - | 5151 | `	}` |
|     25 | 5152 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 5153 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 5154 | `		char zBuf[64];` |
|     16 | 5155 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5156 | `			"vsprintf(): Argument #2 ($values) must be of type array, %s given",` |
|     10 | 5157 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 5158 | `	}` |
|      - | 5159 | `	/* Extract the string format (scalars/null coerce). */` |
|     15 | 5160 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 5161 | `	if( nLen < 1 ){` |
|      - | 5162 | `		/* Empty string */` |
|    ! 0 | 5163 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5164 | `		return PH7_OK;` |
|      - | 5165 | `	}` |
|      - | 5166 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 5167 | `	 * output; propagate the throw status verbatim. */` |
|     15 | 5168 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|     15 | 5169 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 5170 | `		return rcFmt;` |
|      - | 5171 | `	}` |
|      - | 5172 | `	/* Point to hashmap */` |
|     15 | 5173 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 5174 | `	/* PHP 8: too few items in the $values array is a catchable ValueError before output. */` |
|     15 | 5175 | `	rcFmt = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);` |
|     15 | 5176 | `	if( rcFmt != PH7_OK ){` |
|      5 | 5177 | `		return rcFmt;` |
|      - | 5178 | `	}` |
|      - | 5179 | `	/* Extract arguments from the hashmap */` |
|     11 | 5180 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 5181 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|     11 | 5182 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&rc,TRUE);` |
|      - | 5183 | `	/* Release the container */` |
|     11 | 5184 | `	SySetRelease(&sArg);` |
|     11 | 5185 | `	if( rc != SXRET_OK ){` |
|      - | 5186 | `		/* The result append ran out of memory: raise a fatal. */` |
|    ! 0 | 5187 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5188 | `	}` |
|     11 | 5189 | `	return PH7_OK;` |
|     13 | 5190 | `}` |
|      - | 5191 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 5192 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 5193 | `/*` |
|      - | 5194 | ` * Symisc eXtension.` |
|      - | 5195 | ` * string size_format(int64 $size)` |
|      - | 5196 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 5197 | ` *  Example:` |
|      - | 5198 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 5199 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 5200 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 5201 | ` * Parameter` |
|      - | 5202 | ` *  $size` |
|      - | 5203 | ` *    Entity size in bytes.` |
|      - | 5204 | ` * Return` |
|      - | 5205 | ` *   Formatted string representation of the given size.` |
|      - | 5206 | ` */` |
|     24 | 5207 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5208 | `{` |
|      - | 5209 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 5210 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 5211 | `	sxi32 nRest,i_32;` |
|      - | 5212 | `	ph7_int64 iSize;` |
|     25 | 5213 | `	int c = -1; /* index in zUnit[] */` |
|      - | 5214 |  |
|     25 | 5215 | `	if( nArg < 1 ){` |
|      - | 5216 | `		/* Missing argument,return the empty string */` |
|      3 | 5217 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5218 | `		return PH7_OK;` |
|      - | 5219 | `	}` |
|      - | 5220 | `	/* Extract the given size */` |
|     23 | 5221 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 5222 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 5223 | `		/* Don't bother formatting,return immediately */` |
|      5 | 5224 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 5225 | `		return PH7_OK;` |
|      - | 5226 | `	}` |
|     19 | 5227 | `	for(;;){` |
|     39 | 5228 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 5229 | `		iSize >>= 10;` |
|     39 | 5230 | `		c++;` |
|     39 | 5231 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 5232 | `			break;` |
|      - | 5233 | `		}` |
|      1 | 5234 | `	}` |
|     19 | 5235 | `	nRest /= 100;` |
|     19 | 5236 | `	if( nRest > 9 ){` |
|    ! 0 | 5237 | `		nRest = 9;` |
|    ! 0 | 5238 | `	}` |
|     19 | 5239 | `	if( iSize > 999 ){` |
|    ! 0 | 5240 | `		c++;` |
|    ! 0 | 5241 | `		nRest = 9;` |
|    ! 0 | 5242 | `		iSize = 0;` |
|    ! 0 | 5243 | `	}` |
|     19 | 5244 | `	i_32 = (sxi32)iSize;` |
|      - | 5245 | `	/* Format */` |
|     19 | 5246 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 5247 | `	return PH7_OK;` |
|     13 | 5248 | `}` |
|      - | 5249 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 5250 | `/*` |
|      - | 5251 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 5252 | ` *   Calculate the md5 hash of a string.` |
|      - | 5253 | ` * Parameter` |
|      - | 5254 | ` *  $str` |
|      - | 5255 | ` *   Input string` |
|      - | 5256 | ` * $raw_output` |
|      - | 5257 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 5258 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 5259 | ` * Return` |
|      - | 5260 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 5261 | ` */` |
|     12 | 5262 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5263 | `{` |
|      - | 5264 | `	unsigned char zDigest[16];` |
|     13 | 5265 | `	int raw_output = FALSE;` |
|      - | 5266 | `	const void *pIn;` |
|      - | 5267 | `	int nLen;` |
|     13 | 5268 | `	if( nArg < 1 ){` |
|      - | 5269 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5270 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5271 | `		return PH7_OK;` |
|      - | 5272 | `	}` |
|      - | 5273 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 5274 | `	 * digest in PHP — d41d8cd9… — so it must NOT short-circuit). */` |
|     13 | 5275 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 5276 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 5277 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 5278 | `	}` |
|      - | 5279 | `	/* Compute the MD5 digest */` |
|     13 | 5280 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|     13 | 5281 | `	if( raw_output ){` |
|      - | 5282 | `		/* Output raw digest */` |
|      5 | 5283 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 5284 | `	}else{` |
|      - | 5285 | `		/* Perform a binary to hex conversion */` |
|      9 | 5286 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 5287 | `	}` |
|     13 | 5288 | `	return PH7_OK;` |
|      7 | 5289 | `}` |
|      - | 5290 | `/*` |
|      - | 5291 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 5292 | ` *   Calculate the sha1 hash of a string.` |
|      - | 5293 | ` * Parameter` |
|      - | 5294 | ` *  $str` |
|      - | 5295 | ` *   Input string` |
|      - | 5296 | ` * $raw_output` |
|      - | 5297 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 5298 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 5299 | ` * Return` |
|      - | 5300 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 5301 | ` */` |
|     10 | 5302 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5303 | `{` |
|      - | 5304 | `	unsigned char zDigest[20];` |
|     11 | 5305 | `	int raw_output = FALSE;` |
|      - | 5306 | `	const void *pIn;` |
|      - | 5307 | `	int nLen;` |
|     11 | 5308 | `	if( nArg < 1 ){` |
|      - | 5309 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5310 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5311 | `		return PH7_OK;` |
|      - | 5312 | `	}` |
|      - | 5313 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 5314 | `	 * digest in PHP — da39a3ee… — so it must NOT short-circuit). */` |
|     11 | 5315 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 5316 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 5317 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 5318 | `	}` |
|      - | 5319 | `	/* Compute the SHA1 digest */` |
|     11 | 5320 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|     11 | 5321 | `	if( raw_output ){` |
|      - | 5322 | `		/* Output raw digest */` |
|      5 | 5323 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 5324 | `	}else{` |
|      - | 5325 | `		/* Perform a binary to hex conversion */` |
|      7 | 5326 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 5327 | `	}` |
|     11 | 5328 | `	return PH7_OK;` |
|      6 | 5329 | `}` |
|      - | 5330 | `/*` |
|      - | 5331 | ` * int64 crc32(string $str)` |
|      - | 5332 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 5333 | ` * Parameter` |
|      - | 5334 | ` *  $str` |
|      - | 5335 | ` *   Input string` |
|      - | 5336 | ` * Return` |
|      - | 5337 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 5338 | ` */` |
|      2 | 5339 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5340 | `{` |
|      - | 5341 | `	const void *pIn;` |
|      - | 5342 | `	sxu32 nCRC;` |
|      - | 5343 | `	int nLen;` |
|      3 | 5344 | `	if( nArg < 1 ){` |
|      - | 5345 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 5346 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5347 | `		return PH7_OK;` |
|      - | 5348 | `	}` |
|      - | 5349 | `	/* Extract the input string */` |
|      3 | 5350 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5351 | `	if( nLen < 1 ){` |
|      - | 5352 | `		/* crc32("") is 0 in PHP, so this short-circuit is correct here — unlike` |
|      - | 5353 | `		 * md5()/sha1(), whose empty-string digests are non-zero. */` |
|    ! 0 | 5354 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5355 | `		return PH7_OK;` |
|      - | 5356 | `	}` |
|      - | 5357 | `	/* Calculate the sum */` |
|      3 | 5358 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 5359 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 5360 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 5361 | `	return PH7_OK;` |
|      2 | 5362 | `}` |
|      - | 5363 | `/*` |
|      - | 5364 | ` * The hash() family (hash/hash_hmac/hash_equals/hash_algos). Each algorithm is` |
|      - | 5365 | ` * described by a small record so one dispatch (and one generic HMAC) serves them` |
|      - | 5366 | ` * all. Thin adapters normalize the differing context types and the reversed` |
|      - | 5367 | ` * MD5Final argument order behind a uniform Init/Update/Final over a HashCtx union.` |
|      - | 5368 | ` */` |
|     11 | 5369 | `static void HashMd5Init(HashCtx *c){ MD5Init(&c->md5); }` |
|     15 | 5370 | `static void HashMd5Update(HashCtx *c,const unsigned char *d,unsigned int n){ MD5Update(&c->md5,d,n); }` |
|     11 | 5371 | `static void HashMd5Final(HashCtx *c,unsigned char *o){ MD5Final(o,&c->md5); }` |
|     11 | 5372 | `static void HashSha1Init(HashCtx *c){ SHA1Init(&c->sha1); }` |
|     15 | 5373 | `static void HashSha1Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA1Update(&c->sha1,d,n); }` |
|     11 | 5374 | `static void HashSha1Final(HashCtx *c,unsigned char *o){ SHA1Final(&c->sha1,o); }` |
|      9 | 5375 | `static void HashSha224Init(HashCtx *c){ SHA224Init(&c->sha256); }` |
|     33 | 5376 | `static void HashSha256Init(HashCtx *c){ SHA256Init(&c->sha256); }` |
|     57 | 5377 | `static void HashSha256Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA256Update(&c->sha256,d,n); }` |
|     41 | 5378 | `static void HashSha256Final(HashCtx *c,unsigned char *o){ SHA256Final(&c->sha256,o); }` |
|      9 | 5379 | `static void HashSha384Init(HashCtx *c){ SHA384Init(&c->sha512); }` |
|     15 | 5380 | `static void HashSha512Init(HashCtx *c){ SHA512Init(&c->sha512); }` |
|     27 | 5381 | `static void HashSha512Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA512Update(&c->sha512,d,n); }` |
|     23 | 5382 | `static void HashSha512Final(HashCtx *c,unsigned char *o){ SHA512Final(&c->sha512,o); }` |
|      - | 5383 | `typedef struct HashAlgo HashAlgo;` |
|      - | 5384 | `struct HashAlgo {` |
|      - | 5385 | `	const char *zName;   /* lowercase canonical name */` |
|      - | 5386 | `	int nDigestLen;      /* output bytes: 16/20/28/32/48/64 */` |
|      - | 5387 | `	int nBlockLen;       /* internal block bytes (for HMAC): 64 or 128 */` |
|      - | 5388 | `	void (*xInit)(HashCtx *);` |
|      - | 5389 | `	void (*xUpdate)(HashCtx *,const unsigned char *,unsigned int);` |
|      - | 5390 | `	void (*xFinal)(HashCtx *,unsigned char *);` |
|      - | 5391 | `};` |
|      - | 5392 | `static const HashAlgo aHashAlgo[] = {` |
|      - | 5393 | `	{ "md5",    16, 64,  HashMd5Init,    HashMd5Update,    HashMd5Final    },` |
|      - | 5394 | `	{ "sha1",   20, 64,  HashSha1Init,   HashSha1Update,   HashSha1Final   },` |
|      - | 5395 | `	{ "sha224", 28, 64,  HashSha224Init, HashSha256Update, HashSha256Final },` |
|      - | 5396 | `	{ "sha256", 32, 64,  HashSha256Init, HashSha256Update, HashSha256Final },` |
|      - | 5397 | `	{ "sha384", 48, 128, HashSha384Init, HashSha512Update, HashSha512Final },` |
|      - | 5398 | `	{ "sha512", 64, 128, HashSha512Init, HashSha512Update, HashSha512Final },` |
|      - | 5399 | `};` |
|      - | 5400 | `/* Case-insensitive algorithm lookup (PHP accepts 'SHA256' etc.). */` |
|     73 | 5401 | `static const HashAlgo * HashFindAlgo(const char *zName,int nLen){` |
|      - | 5402 | `	sxu32 i;` |
|    279 | 5403 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|    272 | 5404 | `		if( (int)SyStrlen(aHashAlgo[i].zName) == nLen` |
|    211 | 5405 | `			&& SyStrnicmp(aHashAlgo[i].zName,zName,(sxu32)nLen) == 0 ){` |
|     67 | 5406 | `			return &aHashAlgo[i];` |
|      - | 5407 | `		}` |
|    106 | 5408 | `	}` |
|      6 | 5409 | `	return 0;` |
|     38 | 5410 | `}` |
|      - | 5411 | `/*` |
|      - | 5412 | ` * string hash(string $algo,string $data[,bool $binary = false])` |
|      - | 5413 | ` *   Generate a hash value (message digest).` |
|      - | 5414 | ` */` |
|     54 | 5415 | `static int PH7_builtin_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5416 | `{` |
|      - | 5417 | `	const HashAlgo *pAlgo;` |
|      - | 5418 | `	const char *zAlgo,*zData;` |
|     56 | 5419 | `	int nAlgoLen,nDataLen,raw_output = FALSE;` |
|      - | 5420 | `	HashCtx sCtx;` |
|      - | 5421 | `	unsigned char zDigest[64];` |
|     56 | 5422 | `	if( nArg < 2 ){` |
|    ! 0 | 5423 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5424 | `			"hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5425 | `	}` |
|     56 | 5426 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     56 | 5427 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     56 | 5428 | `	if( pAlgo == 0 ){` |
|      3 | 5429 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5430 | `			"hash(): Argument #1 ($algo) must be a valid hashing algorithm");` |
|      - | 5431 | `	}` |
|     53 | 5432 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     53 | 5433 | `	if( nArg > 2 ){` |
|      9 | 5434 | `		raw_output = ph7_value_to_bool(apArg[2]);` |
|      4 | 5435 | `	}` |
|     53 | 5436 | `	pAlgo->xInit(&sCtx);` |
|     53 | 5437 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     53 | 5438 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     53 | 5439 | `	if( raw_output ){` |
|      9 | 5440 | `		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);` |
|      5 | 5441 | `	}else{` |
|     45 | 5442 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);` |
|      - | 5443 | `	}` |
|     53 | 5444 | `	return PH7_OK;` |
|     29 | 5445 | `}` |
|      - | 5446 | `/*` |
|      - | 5447 | ` * string hash_hmac(string $algo,string $data,string $key[,bool $binary = false])` |
|      - | 5448 | ` *   Generate a keyed hash value using the HMAC method (RFC 2104).` |
|      - | 5449 | ` */` |
|     16 | 5450 | `static int PH7_builtin_hash_hmac(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5451 | `{` |
|      - | 5452 | `	const HashAlgo *pAlgo;` |
|      - | 5453 | `	const char *zAlgo,*zData,*zKey;` |
|     18 | 5454 | `	int nAlgoLen,nDataLen,nKeyLen,raw_output = FALSE;` |
|      - | 5455 | `	HashCtx sCtx;` |
|      - | 5456 | `	unsigned char zKeyBlock[128],zIpad[128],zOpad[128],zInner[64],zDigest[64];` |
|      - | 5457 | `	int i,nBlock,nDigest;` |
|     18 | 5458 | `	if( nArg < 3 ){` |
|    ! 0 | 5459 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5460 | `			"hash_hmac() expects at least 3 arguments, %d given",nArg);` |
|      - | 5461 | `	}` |
|     18 | 5462 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     18 | 5463 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     18 | 5464 | `	if( pAlgo == 0 ){` |
|      3 | 5465 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5466 | `			"hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");` |
|      - | 5467 | `	}` |
|     15 | 5468 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     15 | 5469 | `	zKey = ph7_value_to_string(apArg[2],&nKeyLen);` |
|     15 | 5470 | `	if( nArg > 3 ){` |
|      3 | 5471 | `		raw_output = ph7_value_to_bool(apArg[3]);` |
|      1 | 5472 | `	}` |
|     15 | 5473 | `	nBlock = pAlgo->nBlockLen;` |
|     15 | 5474 | `	nDigest = pAlgo->nDigestLen;` |
|      - | 5475 | `	/* Reduce the key to a single block: hash it if longer than the block, then` |
|      - | 5476 | `	 * zero-pad (a short or empty key is just zero-padded). */` |
|     15 | 5477 | `	SyZero(zKeyBlock,sizeof(zKeyBlock));` |
|     15 | 5478 | `	if( nKeyLen > nBlock ){` |
|      3 | 5479 | `		pAlgo->xInit(&sCtx);` |
|      3 | 5480 | `		pAlgo->xUpdate(&sCtx,(const unsigned char *)zKey,(unsigned int)nKeyLen);` |
|      3 | 5481 | `		pAlgo->xFinal(&sCtx,zKeyBlock);` |
|     14 | 5482 | `	}else if( nKeyLen > 0 ){` |
|     11 | 5483 | `		SyMemcpy(zKey,zKeyBlock,(sxu32)nKeyLen);` |
|      5 | 5484 | `	}` |
|   1039 | 5485 | `	for( i = 0; i < nBlock; i++ ){` |
|   1025 | 5486 | `		zIpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x36);` |
|   1025 | 5487 | `		zOpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x5c);` |
|    513 | 5488 | `	}` |
|      - | 5489 | `	/* inner = H((key ^ ipad) \|\| data) */` |
|     15 | 5490 | `	pAlgo->xInit(&sCtx);` |
|     15 | 5491 | `	pAlgo->xUpdate(&sCtx,zIpad,(unsigned int)nBlock);` |
|     15 | 5492 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     15 | 5493 | `	pAlgo->xFinal(&sCtx,zInner);` |
|      - | 5494 | `	/* out = H((key ^ opad) \|\| inner) */` |
|     15 | 5495 | `	pAlgo->xInit(&sCtx);` |
|     15 | 5496 | `	pAlgo->xUpdate(&sCtx,zOpad,(unsigned int)nBlock);` |
|     15 | 5497 | `	pAlgo->xUpdate(&sCtx,zInner,(unsigned int)nDigest);` |
|     15 | 5498 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     15 | 5499 | `	if( raw_output ){` |
|      3 | 5500 | `		ph7_result_string(pCtx,(const char *)zDigest,nDigest);` |
|      2 | 5501 | `	}else{` |
|     13 | 5502 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)nDigest,HashConsumer,pCtx);` |
|      - | 5503 | `	}` |
|     15 | 5504 | `	return PH7_OK;` |
|     10 | 5505 | `}` |
|      - | 5506 | `/*` |
|      - | 5507 | ` * bool hash_equals(string $known_string,string $user_string)` |
|      - | 5508 | ` *   Timing-attack-safe string comparison.` |
|      - | 5509 | ` */` |
|     12 | 5510 | `static int PH7_builtin_hash_equals(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5511 | `{` |
|      - | 5512 | `	const char *zKnown,*zUser;` |
|      - | 5513 | `	int nKnown,nUser,i;` |
|     14 | 5514 | `	volatile unsigned char vDiff = 0;` |
|     14 | 5515 | `	if( nArg < 2 ){` |
|    ! 0 | 5516 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5517 | `			"hash_equals() expects exactly 2 arguments, %d given",nArg);` |
|      - | 5518 | `	}` |
|     14 | 5519 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      4 | 5520 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5521 | `			"hash_equals(): Argument #1 ($known_string) must be of type string, %s given",` |
|      1 | 5522 | `			ph7_type_name(apArg[0]));` |
|      - | 5523 | `	}` |
|     11 | 5524 | `	if( !ph7_value_is_string(apArg[1]) ){` |
|    ! 0 | 5525 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5526 | `			"hash_equals(): Argument #2 ($user_string) must be of type string, %s given",` |
|    ! 0 | 5527 | `			ph7_type_name(apArg[1]));` |
|      - | 5528 | `	}` |
|     11 | 5529 | `	zKnown = ph7_value_to_string(apArg[0],&nKnown);` |
|     11 | 5530 | `	zUser = ph7_value_to_string(apArg[1],&nUser);` |
|     11 | 5531 | `	if( nKnown != nUser ){` |
|      5 | 5532 | `		ph7_result_bool(pCtx,0);` |
|      5 | 5533 | `		return PH7_OK;` |
|      - | 5534 | `	}` |
|      - | 5535 | `	/* Constant-time: read every byte, never short-circuit. */` |
|     19 | 5536 | `	for( i = 0; i < nKnown; i++ ){` |
|     13 | 5537 | `		vDiff \|= (unsigned char)(zKnown[i] ^ zUser[i]);` |
|      7 | 5538 | `	}` |
|      7 | 5539 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|      7 | 5540 | `	return PH7_OK;` |
|      8 | 5541 | `}` |
|      - | 5542 | `/*` |
|      - | 5543 | ` * array hash_algos(void)` |
|      - | 5544 | ` *   Return a list of the registered hashing algorithms.` |
|      - | 5545 | ` */` |
|      2 | 5546 | `static int PH7_builtin_hash_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5547 | `{` |
|      - | 5548 | `	ph7_value *pArray,*pValue;` |
|      - | 5549 | `	sxu32 i;` |
|      1 | 5550 | `	SXUNUSED(nArg);` |
|      1 | 5551 | `	SXUNUSED(apArg);` |
|      3 | 5552 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 5553 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 5554 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 5555 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5556 | `		return PH7_OK;` |
|      - | 5557 | `	}` |
|     15 | 5558 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|     13 | 5559 | `		ph7_value_string(pValue,aHashAlgo[i].zName,-1);` |
|     13 | 5560 | `		ph7_array_add_elem(pArray,0 /* Automatic 0-based index */,pValue);` |
|     13 | 5561 | `		ph7_value_reset_string_cursor(pValue);` |
|      7 | 5562 | `	}` |
|      3 | 5563 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5564 | `	return PH7_OK;` |
|      2 | 5565 | `}` |
|      - | 5566 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 5567 | `/*` |
|      - | 5568 | ` * password_* (bcrypt). These live in ext/standard in real PHP — outside the` |
|      - | 5569 | ` * hash extension — so they are NOT guarded by PH7_DISABLE_HASH_FUNC.` |
|      - | 5570 | ` */` |
|      - | 5571 | `/*` |
|      - | 5572 | ` * Parse a bcrypt crypt string. Returns TRUE and fills *piCost when zHash is a` |
|      - | 5573 | ` * well-formed "$2?$NN$"+53-char bcrypt hash (60 bytes, valid minor, cost 4..31).` |
|      - | 5574 | ` */` |
|     40 | 5575 | `static int BcryptParseHash(const char *zHash,int nHash,int *piCost)` |
|      1 | 5576 | `{` |
|      - | 5577 | `	int iCost;` |
|     40 | 5578 | `	if( nHash != 60 \|\| zHash[0] != '$' \|\| zHash[1] != '2' \|\| zHash[3] != '$'` |
|     29 | 5579 | `		\|\| (zHash[2] != 'a' && zHash[2] != 'b' && zHash[2] != 'x' && zHash[2] != 'y') ){` |
|     13 | 5580 | `		return FALSE;` |
|      - | 5581 | `	}` |
|     29 | 5582 | `	if( zHash[4] < '0' \|\| zHash[4] > '9' \|\| zHash[5] < '0' \|\| zHash[5] > '9' \|\| zHash[6] != '$' ){` |
|    ! 0 | 5583 | `		return FALSE;` |
|      - | 5584 | `	}` |
|     29 | 5585 | `	iCost = (zHash[4]-'0')*10 + (zHash[5]-'0');` |
|     29 | 5586 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      3 | 5587 | `		return FALSE;` |
|      - | 5588 | `	}` |
|     27 | 5589 | `	if( piCost ){ *piCost = iCost; }` |
|     27 | 5590 | `	return TRUE;` |
|     21 | 5591 | `}` |
|      - | 5592 | `/*` |
|      - | 5593 | ` * TRUE if the $algo argument selects bcrypt: null (PASSWORD_DEFAULT) or the` |
|      - | 5594 | ` * "2y" id (PASSWORD_BCRYPT/PASSWORD_DEFAULT). bcrypt is the only supported algo.` |
|      - | 5595 | ` */` |
|     20 | 5596 | `static int BcryptIsBcryptAlgo(ph7_value *pAlgo)` |
|      3 | 5597 | `{` |
|     23 | 5598 | `	if( ph7_value_is_null(pAlgo) ){` |
|    ! 0 | 5599 | `		return TRUE;` |
|      - | 5600 | `	}` |
|     23 | 5601 | `	if( ph7_value_is_string(pAlgo) ){` |
|      - | 5602 | `		int nAlgo;` |
|     23 | 5603 | `		const char *zAlgo = ph7_value_to_string(pAlgo,&nAlgo);` |
|     23 | 5604 | `		return ( nAlgo == 2 && zAlgo[0] == '2' && zAlgo[1] == 'y' );` |
|      - | 5605 | `	}` |
|    ! 0 | 5606 | `	return FALSE;` |
|     13 | 5607 | `}` |
|      - | 5608 | `/*` |
|      - | 5609 | ` * bool\|string password_hash(string $password,string\|int\|null $algo[,array $options])` |
|      - | 5610 | ` *  Create a bcrypt hash of the password.` |
|      - | 5611 | ` */` |
|     16 | 5612 | `static int PH7_builtin_password_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 5613 | `{` |
|      - | 5614 | `	const char *zPwd;` |
|     19 | 5615 | `	int nPwd,iCost = 12;` |
|      - | 5616 | `	unsigned char aSalt[16];` |
|      - | 5617 | `	char zHash[60];` |
|     19 | 5618 | `	if( nArg < 2 ){` |
|    ! 0 | 5619 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5620 | `			"password_hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5621 | `	}` |
|     19 | 5622 | `	if( !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      3 | 5623 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5624 | `			"password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm");` |
|      - | 5625 | `	}` |
|      - | 5626 | `	/* cost from $options['cost'] (default 12). */` |
|     16 | 5627 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|     14 | 5628 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|     14 | 5629 | `		if( pCost ){ iCost = ph7_value_to_int(pCost); }` |
|      6 | 5630 | `	}` |
|     16 | 5631 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      4 | 5632 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 5633 | `			"Invalid bcrypt cost parameter specified: %d",iCost);` |
|      - | 5634 | `	}` |
|     13 | 5635 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     13 | 5636 | `	if( SyOSCSPRNG(aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 5637 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - | 5638 | `			"password_hash(): unable to gather sufficient entropy for the salt");` |
|      - | 5639 | `	}` |
|     13 | 5640 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zHash) != SXRET_OK ){` |
|    ! 0 | 5641 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5642 | `		return PH7_OK;` |
|      - | 5643 | `	}` |
|     13 | 5644 | `	ph7_result_string(pCtx,zHash,(int)sizeof(zHash));` |
|     13 | 5645 | `	return PH7_OK;` |
|     11 | 5646 | `}` |
|      - | 5647 | `/*` |
|      - | 5648 | ` * bool password_verify(string $password,string $hash)` |
|      - | 5649 | ` *  Verify a password against a bcrypt hash. Never throws on a malformed hash.` |
|      - | 5650 | ` */` |
|     28 | 5651 | `static int PH7_builtin_password_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5652 | `{` |
|      - | 5653 | `	const char *zPwd,*zHash;` |
|      - | 5654 | `	int nPwd,nHash,iCost,i;` |
|      - | 5655 | `	unsigned char aSalt[16];` |
|      - | 5656 | `	char zComputed[60];` |
|     29 | 5657 | `	volatile unsigned char vDiff = 0;` |
|     29 | 5658 | `	if( nArg < 2 ){` |
|    ! 0 | 5659 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5660 | `			"password_verify() expects exactly 2 arguments, %d given",nArg);` |
|      - | 5661 | `	}` |
|     29 | 5662 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     29 | 5663 | `	zHash = ph7_value_to_string(apArg[1],&nHash);` |
|     29 | 5664 | `	if( !BcryptParseHash(zHash,nHash,&iCost) ){` |
|     11 | 5665 | `		ph7_result_bool(pCtx,0);` |
|     11 | 5666 | `		return PH7_OK;` |
|      - | 5667 | `	}` |
|      - | 5668 | `	/* Recover the 16 salt bytes from the 22-char salt field [7..28]. */` |
|     19 | 5669 | `	if( SyBcryptB64Decode(&zHash[7],22,aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 5670 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5671 | `		return PH7_OK;` |
|      - | 5672 | `	}` |
|     19 | 5673 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zComputed) != SXRET_OK ){` |
|    ! 0 | 5674 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5675 | `		return PH7_OK;` |
|      - | 5676 | `	}` |
|      - | 5677 | `	/* Constant-time compare of the 31-char hash field [29..59] only — sidesteps` |
|      - | 5678 | `	 * salt re-canonicalisation and any "$2a"/"$2y" prefix difference. */` |
|    577 | 5679 | `	for( i = 29; i < 60; i++ ){` |
|    559 | 5680 | `		vDiff \|= (unsigned char)(zComputed[i] ^ zHash[i]);` |
|    280 | 5681 | `	}` |
|     19 | 5682 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|     19 | 5683 | `	return PH7_OK;` |
|     15 | 5684 | `}` |
|      - | 5685 | `/*` |
|      - | 5686 | ` * array password_get_info(string $hash)` |
|      - | 5687 | ` *  Return ["algo"=>id\|null, "algoName"=>name, "options"=>[...]].` |
|      - | 5688 | ` */` |
|      6 | 5689 | `static int PH7_builtin_password_get_info(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5690 | `{` |
|      7 | 5691 | `	const char *zHash = "";` |
|      7 | 5692 | `	int nHash,iCost = 0,bBcrypt = 0;` |
|      - | 5693 | `	ph7_value *pArray,*pOptions,*pVal;` |
|      7 | 5694 | `	if( nArg > 0 ){` |
|      7 | 5695 | `		zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 5696 | `		bBcrypt = BcryptParseHash(zHash,nHash,&iCost);` |
|      3 | 5697 | `	}` |
|      7 | 5698 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 5699 | `	pOptions = ph7_context_new_array(pCtx);` |
|      7 | 5700 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      7 | 5701 | `	if( pArray == 0 \|\| pOptions == 0 \|\| pVal == 0 ){` |
|    ! 0 | 5702 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5703 | `		return PH7_OK;` |
|      - | 5704 | `	}` |
|      7 | 5705 | `	if( bBcrypt ){` |
|      5 | 5706 | `		ph7_value_string(pVal,&zHash[1],2);            /* algo "2y"/"2a" */` |
|      5 | 5707 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      5 | 5708 | `		ph7_value_reset_string_cursor(pVal);` |
|      5 | 5709 | `		ph7_value_string(pVal,"bcrypt",(int)sizeof("bcrypt")-1);` |
|      5 | 5710 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      5 | 5711 | `		ph7_value_int(pVal,iCost);` |
|      5 | 5712 | `		ph7_array_add_strkey_elem(pOptions,"cost",pVal);` |
|      3 | 5713 | `	}else{` |
|      3 | 5714 | `		ph7_value_null(pVal);                          /* algo => null */` |
|      3 | 5715 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      3 | 5716 | `		ph7_value_string(pVal,"unknown",(int)sizeof("unknown")-1);` |
|      3 | 5717 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      - | 5718 | `	}` |
|      7 | 5719 | `	ph7_array_add_strkey_elem(pArray,"options",pOptions);` |
|      7 | 5720 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 5721 | `	return PH7_OK;` |
|      4 | 5722 | `}` |
|      - | 5723 | `/*` |
|      - | 5724 | ` * bool password_needs_rehash(string $hash,string\|int\|null $algo[,array $options])` |
|      - | 5725 | ` *  True if the hash was not made with the given algo/options.` |
|      - | 5726 | ` */` |
|      6 | 5727 | `static int PH7_builtin_password_needs_rehash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5728 | `{` |
|      - | 5729 | `	const char *zHash;` |
|      7 | 5730 | `	int nHash,iCost = 0,iWantCost = 12;` |
|      7 | 5731 | `	if( nArg < 2 ){` |
|    ! 0 | 5732 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5733 | `			"password_needs_rehash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5734 | `	}` |
|      7 | 5735 | `	zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 5736 | `	if( !BcryptParseHash(zHash,nHash,&iCost) \|\| !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      - | 5737 | `		/* A non-bcrypt hash, or a request for a different algo → needs rehash. */` |
|      3 | 5738 | `		ph7_result_bool(pCtx,1);` |
|      3 | 5739 | `		return PH7_OK;` |
|      - | 5740 | `	}` |
|      5 | 5741 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|      5 | 5742 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|      5 | 5743 | `		if( pCost ){ iWantCost = ph7_value_to_int(pCost); }` |
|      2 | 5744 | `	}` |
|      5 | 5745 | `	ph7_result_bool(pCtx,iCost != iWantCost);` |
|      5 | 5746 | `	return PH7_OK;` |
|      4 | 5747 | `}` |
|      - | 5748 | `/*` |
|      - | 5749 | ` * filter_var() — input validation and sanitization (the ext/filter API).` |
|      - | 5750 | ` *` |
|      - | 5751 | ` * Filter and flag identifiers (values match PHP 8.5; the constants themselves` |
|      - | 5752 | ` * are registered in constant.c). The validate filters are hand-rolled rather` |
|      - | 5753 | ` * than delegating to SyStrToInt64/SyStrToReal: the former silently skips leading` |
|      - | 5754 | ` * zeros and cannot signal overflow, and the latter treats ',' as a decimal point` |
|      - | 5755 | ` * unconditionally — neither matches PHP's filter semantics.` |
|      - | 5756 | ` */` |
|      - | 5757 | `#define FV_VALIDATE_INT     257` |
|      - | 5758 | `#define FV_VALIDATE_BOOLEAN 258` |
|      - | 5759 | `#define FV_VALIDATE_FLOAT   259` |
|      - | 5760 | `#define FV_VALIDATE_REGEXP  272` |
|      - | 5761 | `#define FV_VALIDATE_URL     273` |
|      - | 5762 | `#define FV_VALIDATE_EMAIL   274` |
|      - | 5763 | `#define FV_VALIDATE_IP      275` |
|      - | 5764 | `#define FV_VALIDATE_MAC     276` |
|      - | 5765 | `#define FV_VALIDATE_DOMAIN  277` |
|      - | 5766 | `#define FV_SANITIZE_SPECIAL_CHARS      515` |
|      - | 5767 | `#define FV_DEFAULT          516 /* == FILTER_UNSAFE_RAW: pass the value through */` |
|      - | 5768 | `#define FV_SANITIZE_EMAIL   517` |
|      - | 5769 | `#define FV_SANITIZE_URL     518` |
|      - | 5770 | `#define FV_SANITIZE_NUMBER_INT   519` |
|      - | 5771 | `#define FV_SANITIZE_NUMBER_FLOAT 520` |
|      - | 5772 | `#define FV_SANITIZE_FULL_SPECIAL_CHARS 522` |
|      - | 5773 | `#define FV_FLAG_ALLOW_OCTAL  1` |
|      - | 5774 | `#define FV_FLAG_ALLOW_HEX    2` |
|      - | 5775 | `#define FV_FLAG_STRIP_LOW    4` |
|      - | 5776 | `#define FV_FLAG_STRIP_HIGH   8` |
|      - | 5777 | `#define FV_FLAG_ENCODE_LOW   16` |
|      - | 5778 | `#define FV_FLAG_ENCODE_HIGH  32` |
|      - | 5779 | `#define FV_FLAG_ENCODE_AMP   64` |
|      - | 5780 | `#define FV_FLAG_NO_ENCODE_QUOTES 128` |
|      - | 5781 | `#define FV_FLAG_STRIP_BACKTICK   512` |
|      - | 5782 | `#define FV_FLAG_ALLOW_FRACTION   4096` |
|      - | 5783 | `#define FV_FLAG_ALLOW_THOUSAND   8192` |
|      - | 5784 | `#define FV_FLAG_ALLOW_SCIENTIFIC 16384` |
|      - | 5785 | `#define FV_FLAG_IPV4  1048576` |
|      - | 5786 | `#define FV_FLAG_IPV6  2097152` |
|      - | 5787 | `#define FV_NULL_ON_FAILURE 134217728` |
|      - | 5788 | `/* The subset of flags the UNSAFE_RAW/DEFAULT string filter (FvSanitizeString)` |
|      - | 5789 | ` * acts on: when none are set the filter is a verbatim pass-through, so FV_DEFAULT` |
|      - | 5790 | ` * can shortcut. Keep this in sync with FvSanitizeString's flag handling. */` |
|      - | 5791 | `#define FV_FLAG_STRING_MASK (FV_FLAG_STRIP_LOW\|FV_FLAG_STRIP_HIGH\|FV_FLAG_STRIP_BACKTICK \` |
|      - | 5792 | `                            \|FV_FLAG_ENCODE_LOW\|FV_FLAG_ENCODE_HIGH\|FV_FLAG_ENCODE_AMP)` |
|      - | 5793 |  |
|      - | 5794 | `/* Trim leading/trailing PHP whitespace, adjusting the (*pz,*pn) view in place.` |
|      - | 5795 | ` * SyisSpace (isspace) matches PHP's filter whitespace set " \t\n\r\v\f". */` |
|    153 | 5796 | `static void FvTrim(const char **pz,int *pn){` |
|    153 | 5797 | `	const char *z = *pz;` |
|    153 | 5798 | `	int n = *pn;` |
|    157 | 5799 | `	while( n>0 && SyisSpace((unsigned char)z[0]) ){ z++; n--; }` |
|    161 | 5800 | `	while( n>0 && SyisSpace((unsigned char)z[n-1]) ){ n--; }` |
|    153 | 5801 | `	*pz = z; *pn = n;` |
|    153 | 5802 | `}` |
|      - | 5803 | `/* FILTER_VALIDATE_INT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     57 | 5804 | `static int FvValidateInt(const char *z,int n,int flags,ph7_int64 *pOut){` |
|     57 | 5805 | `	int neg = 0, i;` |
|     57 | 5806 | `	sxu64 u = 0;` |
|     57 | 5807 | `	FvTrim(&z,&n);` |
|     57 | 5808 | `	if( n==0 ){ return 0; }` |
|     51 | 5809 | `	if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; n--; }` |
|     51 | 5810 | `	if( n==0 ){ return 0; }` |
|     49 | 5811 | `	if( (flags & FV_FLAG_ALLOW_HEX) && n>=2 && z[0]=='0' && (z[1]=='x'\|\|z[1]=='X') ){` |
|      3 | 5812 | `		z += 2; n -= 2;` |
|      3 | 5813 | `		if( n==0 ){ return 0; }` |
|      7 | 5814 | `		for( i=0; i<n; i++ ){` |
|      5 | 5815 | `			int h = SyHexToint((unsigned char)z[i]);` |
|      5 | 5816 | `			if( h<0 ){ return 0; }` |
|      5 | 5817 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)h)/16 ){ return 0; }` |
|      5 | 5818 | `			u = u*16 + (sxu64)h;` |
|      3 | 5819 | `		}` |
|     48 | 5820 | `	}else if( (flags & FV_FLAG_ALLOW_OCTAL) && z[0]=='0' ){` |
|      9 | 5821 | `		for( i=0; i<n; i++ ){` |
|      7 | 5822 | `			if( z[i]<'0' \|\| z[i]>'7' ){ return 0; }` |
|      7 | 5823 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/8 ){ return 0; }` |
|      7 | 5824 | `			u = u*8 + (sxu64)(z[i]-'0');` |
|      4 | 5825 | `		}` |
|      2 | 5826 | `	}else{` |
|     45 | 5827 | `		if( z[0]=='0' && n>1 ){ return 0; } /* a leading zero is rejected in base 10 */` |
|    201 | 5828 | `		for( i=0; i<n; i++ ){` |
|    173 | 5829 | `			if( !SyisDigit((unsigned char)z[i]) ){ return 0; }` |
|    161 | 5830 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/10 ){ return 0; }` |
|    161 | 5831 | `			u = u*10 + (sxu64)(z[i]-'0');` |
|     81 | 5832 | `		}` |
|      - | 5833 | `	}` |
|     33 | 5834 | `	if( neg ){` |
|      5 | 5835 | `		if( u > 0x8000000000000000ULL ){ return 0; }` |
|      5 | 5836 | `		*pOut = (ph7_int64)(0ULL - u); /* two's-complement negate in unsigned space */` |
|      3 | 5837 | `	}else{` |
|     29 | 5838 | `		if( u > 0x7FFFFFFFFFFFFFFFULL ){ return 0; }` |
|     27 | 5839 | `		*pOut = (ph7_int64)u;` |
|      - | 5840 | `	}` |
|     31 | 5841 | `	return 1;` |
|     29 | 5842 | `}` |
|      - | 5843 | `/* FILTER_VALIDATE_FLOAT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     69 | 5844 | `static int FvValidateFloat(const char *z,int n,int flags,double *pOut){` |
|      - | 5845 | `	char zBuf[512];` |
|     69 | 5846 | `	int i, m = 0, seenDigit = 0;` |
|     69 | 5847 | `	const char *zv; int nv; double d = 0;` |
|     69 | 5848 | `	FvTrim(&z,&n);` |
|      - | 5849 | `	/* Bound the input: zBuf[512] holds the thousand-separator-stripped copy, and` |
|      - | 5850 | `	 * the cap also rejects the pathological 500+ digit floats PHP refuses. */` |
|     69 | 5851 | `	if( n==0 \|\| n>500 ){ return 0; }` |
|     69 | 5852 | `	if( flags & FV_FLAG_ALLOW_THOUSAND ){` |
|      - | 5853 | `		/* Commas are optional, but when present they must group the integer part` |
|      - | 5854 | `		 * into a leading run of 1..3 digits followed by groups of exactly 3` |
|      - | 5855 | `		 * ("1,000" ok, "1,5"/"1234,567" rejected). Strip them into zBuf and reject` |
|      - | 5856 | `		 * a comma anywhere in the fractional/exponent tail. */` |
|     25 | 5857 | `		int s = 0, intEnd, segStart, segIdx, hasComma = 0;` |
|     25 | 5858 | `		if( s<n && (z[s]=='+'\|\|z[s]=='-') ){ zBuf[m++] = z[s]; s++; }` |
|     25 | 5859 | `		intEnd = s;` |
|    167 | 5860 | `		while( intEnd<n && z[intEnd]!='.' && z[intEnd]!='e' && z[intEnd]!='E' ){` |
|    143 | 5861 | `			if( z[intEnd]==',' ){ hasComma = 1; }` |
|    143 | 5862 | `			intEnd++;` |
|      1 | 5863 | `		}` |
|     25 | 5864 | `		if( hasComma ){` |
|     25 | 5865 | `			segStart = s; segIdx = 0;` |
|    165 | 5866 | `			for( i=s; i<=intEnd; i++ ){` |
|    151 | 5867 | `				if( i==intEnd \|\| z[i]==',' ){` |
|     49 | 5868 | `					int segLen = i - segStart, k;` |
|     49 | 5869 | `					if( segIdx==0 ){ if( segLen<1 \|\| segLen>3 ){ return 0; } }` |
|     25 | 5870 | `					else if( segLen!=3 ){ return 0; }` |
|    119 | 5871 | `					for( k=segStart; k<i; k++ ){` |
|     81 | 5872 | `						if( !SyisDigit((unsigned char)z[k]) ){ return 0; }` |
|     81 | 5873 | `						zBuf[m++] = z[k];` |
|     41 | 5874 | `					}` |
|     39 | 5875 | `					segStart = i+1; segIdx++;` |
|     19 | 5876 | `				}` |
|     71 | 5877 | `			}` |
|      8 | 5878 | `		}else{` |
|    ! 0 | 5879 | `			for( i=s; i<intEnd; i++ ){ zBuf[m++] = z[i]; }` |
|      - | 5880 | `		}` |
|     27 | 5881 | `		for( i=intEnd; i<n; i++ ){` |
|     13 | 5882 | `			if( z[i]==',' ){ return 0; }` |
|     13 | 5883 | `			zBuf[m++] = z[i];` |
|      7 | 5884 | `		}` |
|     15 | 5885 | `		zv = zBuf; nv = m;` |
|      8 | 5886 | `	}else{` |
|     45 | 5887 | `		zv = z; nv = n;` |
|      - | 5888 | `	}` |
|     59 | 5889 | `	i = 0;` |
|     59 | 5890 | `	if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|    167 | 5891 | `	while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     59 | 5892 | `	if( i<nv && zv[i]=='.' ){` |
|     21 | 5893 | `		i++;` |
|     39 | 5894 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     10 | 5895 | `	}` |
|     59 | 5896 | `	if( !seenDigit ){ return 0; }` |
|     57 | 5897 | `	if( i<nv && (zv[i]=='e'\|\|zv[i]=='E') ){` |
|     29 | 5898 | `		i++;` |
|     29 | 5899 | `		if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|     29 | 5900 | `		if( i>=nv \|\| !SyisDigit((unsigned char)zv[i]) ){ return 0; }` |
|    105 | 5901 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; }` |
|     14 | 5902 | `	}` |
|     57 | 5903 | `	if( i!=nv ){ return 0; } /* trailing junk */` |
|      - | 5904 | `	/* The grammar above guarantees zv[0..nv) is a clean ASCII decimal float (no hex /` |
|      - | 5905 | `	 * inf / nan / trailing junk), so it is safe to hand to libc strtod, which — unlike` |
|      - | 5906 | `	 * SyStrToReal (15 sig-digits + exponent clamped to 308, so it silently saturates` |
|      - | 5907 | `	 * overflowing magnitudes to a finite value) — is overflow/underflow-aware and` |
|      - | 5908 | `	 * correctly rounded. strtod needs a NUL-terminated string: the ALLOW_THOUSAND path` |
|      - | 5909 | `	 * already built the span in zBuf (zv==zBuf); the plain path must copy it there (z is` |
|      - | 5910 | `	 * const + not NUL-terminated). nv <= n <= 500 < sizeof(zBuf) by the cap above.` |
|      - | 5911 | `	 * Matches PHP 8.5 byte-for-byte: reject overflow (-> +/-INF) and total underflow` |
|      - | 5912 | `	 * (-> 0.0), keep subnormals (nonzero, errno==ERANGE) and a genuine "0" (errno==0). */` |
|     53 | 5913 | `	if( zv != zBuf ){ SyMemcpy(zv,zBuf,(sxu32)nv); }` |
|     53 | 5914 | `	zBuf[nv] = 0;` |
|     53 | 5915 | `	errno = 0;` |
|     53 | 5916 | `	d = strtod(zBuf,0);` |
|     53 | 5917 | `	if( errno == ERANGE && (d == HUGE_VAL \|\| d == -HUGE_VAL \|\| d == 0.0) ){` |
|     15 | 5918 | `		return 0;` |
|      - | 5919 | `	}` |
|     39 | 5920 | `	*pOut = d;` |
|     39 | 5921 | `	return 1;` |
|     35 | 5922 | `}` |
|      - | 5923 | `/* FILTER_VALIDATE_BOOLEAN. Returns 1 if the string is recognized (sets *pBool),` |
|      - | 5924 | ` * 0 if it is unrecognized (the failure path). "0"/"false"/"" are recognized as` |
|      - | 5925 | ` * false, NOT failures. */` |
|     33 | 5926 | `static int FvValidateBool(const char *z,int n,int *pBool){` |
|     33 | 5927 | `	FvTrim(&z,&n);` |
|     32 | 5928 | `	if( (n==1 && z[0]=='1') \|\| (n==4 && SyStrnicmp(z,"true",4)==0)` |
|     25 | 5929 | `	    \|\| (n==2 && SyStrnicmp(z,"on",2)==0) \|\| (n==3 && SyStrnicmp(z,"yes",3)==0) ){` |
|     11 | 5930 | `		*pBool = 1; return 1;` |
|      - | 5931 | `	}` |
|     22 | 5932 | `	if( n==0 \|\| (n==1 && z[0]=='0') \|\| (n==5 && SyStrnicmp(z,"false",5)==0)` |
|     11 | 5933 | `	    \|\| (n==3 && SyStrnicmp(z,"off",3)==0) \|\| (n==2 && SyStrnicmp(z,"no",2)==0) ){` |
|     11 | 5934 | `		*pBool = 0; return 1;` |
|      - | 5935 | `	}` |
|      9 | 5936 | `	return 0;` |
|     15 | 5937 | `}` |
|      - | 5938 | `/* IPv4 dotted-quad: exactly 4 octets 0..255, no leading zeros. */` |
|     33 | 5939 | `static int FvValidateIp4(const char *z,int n){` |
|     33 | 5940 | `	int i = 0, parts = 0;` |
|     77 | 5941 | `	while( i<n ){` |
|     65 | 5942 | `		int val = 0, digits = 0, start = i;` |
|    143 | 5943 | `		while( i<n && SyisDigit((unsigned char)z[i]) ){` |
|     85 | 5944 | `			val = val*10 + (z[i]-'0');` |
|     85 | 5945 | `			if( val>255 ){ return 0; }` |
|     79 | 5946 | `			digits++; i++;` |
|      1 | 5947 | `		}` |
|     59 | 5948 | `		if( digits==0 \|\| digits>3 ){ return 0; }` |
|     49 | 5949 | `		if( digits>1 && z[start]=='0' ){ return 0; } /* leading zero */` |
|     45 | 5950 | `		parts++;` |
|     45 | 5951 | `		if( parts>4 ){ return 0; }` |
|     45 | 5952 | `		if( i<n ){` |
|     33 | 5953 | `			if( z[i]!='.' ){ return 0; }` |
|     33 | 5954 | `			i++;` |
|     33 | 5955 | `			if( i>=n ){ return 0; } /* trailing dot */` |
|     16 | 5956 | `		}` |
|      1 | 5957 | `	}` |
|     13 | 5958 | `	return parts==4;` |
|     17 | 5959 | `}` |
|      - | 5960 | `/* A colon-separated run of IPv6 hextets with no "::" (n may be 0 -> 0 groups),` |
|      - | 5961 | ` * allowing a trailing embedded IPv4. Returns the 16-bit group count or -1. */` |
|     19 | 5962 | `static int FvIp6Hextets(const char *z,int n){` |
|     19 | 5963 | `	int i = 0, segStart = 0, groups = 0;` |
|     19 | 5964 | `	if( n==0 ){ return 0; }` |
|    145 | 5965 | `	while( i<=n ){` |
|    133 | 5966 | `		if( i==n \|\| z[i]==':' ){` |
|     23 | 5967 | `			int segLen = i - segStart, j, isV4 = 0;` |
|     23 | 5968 | `			if( segLen==0 ){ return -1; } /* an empty hextet (stray ':') */` |
|     77 | 5969 | `			for( j=segStart; j<i; j++ ){ if( z[j]=='.' ){ isV4 = 1; break; } }` |
|     23 | 5970 | `			if( isV4 ){` |
|     11 | 5971 | `				if( i!=n ){ return -1; } /* IPv4 only as the final token */` |
|     11 | 5972 | `				if( !FvValidateIp4(z+segStart,segLen) ){ return -1; }` |
|      5 | 5973 | `				groups += 2;` |
|      3 | 5974 | `			}else{` |
|     13 | 5975 | `				if( segLen>4 ){ return -1; }` |
|     47 | 5976 | `				for( j=segStart; j<i; j++ ){ if( SyHexToint((unsigned char)z[j])<0 ){ return -1; } }` |
|     13 | 5977 | `				groups++;` |
|      - | 5978 | `			}` |
|     17 | 5979 | `			segStart = i+1;` |
|      8 | 5980 | `		}` |
|    127 | 5981 | `		i++;` |
|      1 | 5982 | `	}` |
|     13 | 5983 | `	return groups;` |
|     10 | 5984 | `}` |
|      - | 5985 | `/* IPv6: at most one "::" zero-run; 8 groups exactly, or fewer when "::" present. */` |
|     19 | 5986 | `static int FvValidateIp6(const char *z,int n){` |
|     19 | 5987 | `	const char *zDbl = 0;` |
|      - | 5988 | `	int i, ga, gb;` |
|    139 | 5989 | `	for( i=0; i+1<n; i++ ){` |
|    123 | 5990 | `		if( z[i]==':' && z[i+1]==':' ){` |
|     13 | 5991 | `			if( zDbl ){ return 0; } /* a second "::" is invalid */` |
|     11 | 5992 | `			zDbl = z+i;` |
|      5 | 5993 | `		}` |
|     61 | 5994 | `	}` |
|     17 | 5995 | `	if( zDbl==0 ){` |
|      9 | 5996 | `		return FvIp6Hextets(z,n)==8;` |
|    ! 0 | 5997 | `	}else{` |
|      9 | 5998 | `		int lenA = (int)(zDbl - z);` |
|      9 | 5999 | `		int lenB = n - lenA - 2;` |
|      9 | 6000 | `		ga = (lenA==0) ? 0 : FvIp6Hextets(z,lenA);` |
|      9 | 6001 | `		gb = (lenB==0) ? 0 : FvIp6Hextets(zDbl+2,lenB);` |
|      9 | 6002 | `		if( ga<0 \|\| gb<0 ){ return 0; }` |
|      9 | 6003 | `		return (ga+gb)<=7; /* "::" stands for at least one zero group */` |
|      - | 6004 | `	}` |
|     10 | 6005 | `}` |
|     25 | 6006 | `static int FvValidateIp(const char *z,int n,int flags){` |
|     25 | 6007 | `	int v4 = (flags & FV_FLAG_IPV4), v6 = (flags & FV_FLAG_IPV6);` |
|     25 | 6008 | `	if( !v4 && !v6 ){ v4 = v6 = 1; } /* default accepts either family */` |
|     25 | 6009 | `	if( v4 && FvValidateIp4(z,n) ){ return 1; }` |
|     21 | 6010 | `	if( v6 && FvValidateIp6(z,n) ){ return 1; }` |
|     13 | 6011 | `	return 0;` |
|     13 | 6012 | `}` |
|      - | 6013 | `/* FILTER_VALIDATE_MAC: 17-char colon- or dash-separated hex (XX:XX:..:XX). */` |
|     11 | 6014 | `static int FvValidateMac(const char *z,int n){` |
|      - | 6015 | `	char sep;` |
|      - | 6016 | `	int i;` |
|     11 | 6017 | `	if( n!=17 ){ return 0; }` |
|      7 | 6018 | `	sep = z[2];` |
|      7 | 6019 | `	if( sep!=':' && sep!='-' ){ return 0; }` |
|    105 | 6020 | `	for( i=0; i<17; i++ ){` |
|    101 | 6021 | `		if( (i%3)==2 ){ if( z[i]!=sep ){ return 0; } }` |
|     71 | 6022 | `		else if( SyHexToint((unsigned char)z[i])<0 ){ return 0; }` |
|     50 | 6023 | `	}` |
|      5 | 6024 | `	return 1;` |
|      6 | 6025 | `}` |
|      - | 6026 | `/* FILTER_VALIDATE_EMAIL (best-effort: covers the common cases, not quoted local` |
|      - | 6027 | ` * parts or IP-literal domains). */` |
|     28 | 6028 | `static int FvValidateEmail(const char *z,int n){` |
|     28 | 6029 | `	int at = -1, i, localLen, domLen, labelStart, dotCount = 0;` |
|      - | 6030 | `	const char *zDom;` |
|     28 | 6031 | `	if( n==0 \|\| n>320 ){ return 0; }` |
|    201 | 6032 | `	for( i=0; i<n; i++ ){` |
|    181 | 6033 | `		if( z[i]=='@' ){ if( at>=0 ){ return 0; } at = i; }` |
|     91 | 6034 | `	}` |
|     21 | 6035 | `	if( at<=0 \|\| at==n-1 ){ return 0; } /* one '@', non-empty local and domain */` |
|     21 | 6036 | `	localLen = at;` |
|     21 | 6037 | `	zDom = z + at + 1;` |
|     21 | 6038 | `	domLen = n - at - 1;` |
|     21 | 6039 | `	if( z[0]=='.' \|\| z[at-1]=='.' ){ return 0; }` |
|     57 | 6040 | `	for( i=0; i<localLen; i++ ){` |
|     43 | 6041 | `		unsigned char c = (unsigned char)z[i];` |
|     43 | 6042 | `		if( c<=' ' ){ return 0; }` |
|     41 | 6043 | `		if( c=='.' && i+1<localLen && z[i+1]=='.' ){ return 0; }` |
|     20 | 6044 | `	}` |
|     15 | 6045 | `	if( zDom[0]=='.' \|\| zDom[domLen-1]=='.' ){ return 0; }` |
|     13 | 6046 | `	labelStart = 0;` |
|     85 | 6047 | `	for( i=0; i<=domLen; i++ ){` |
|     75 | 6048 | `		if( i==domLen \|\| zDom[i]=='.' ){` |
|     25 | 6049 | `			int ll = i - labelStart;` |
|     25 | 6050 | `			if( ll==0 ){ return 0; } /* consecutive dots */` |
|     23 | 6051 | `			if( zDom[labelStart]=='-' \|\| zDom[i-1]=='-' ){ return 0; }` |
|     23 | 6052 | `			if( i<domLen ){ dotCount++; }` |
|     23 | 6053 | `			labelStart = i+1;` |
|     12 | 6054 | `		}else{` |
|     51 | 6055 | `			unsigned char c = (unsigned char)zDom[i];` |
|     51 | 6056 | `			if( !((c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9')\|\|c=='-') ){ return 0; }` |
|      - | 6057 | `		}` |
|     37 | 6058 | `	}` |
|     11 | 6059 | `	if( dotCount<1 ){ return 0; } /* PHP requires a dot in the domain (any TLD length) */` |
|      9 | 6060 | `	return 1;` |
|     15 | 6061 | `}` |
|      - | 6062 | `/* FILTER_VALIDATE_DOMAIN (lenient, matching PHP without FILTER_FLAG_HOSTNAME). */` |
|     11 | 6063 | `static int FvValidateDomain(const char *z,int n){` |
|      - | 6064 | `	int i;` |
|     11 | 6065 | `	if( n<1 \|\| n>253 \|\| z[0]=='.' ){ return 0; }` |
|     81 | 6066 | `	for( i=0; i<n; i++ ){` |
|     75 | 6067 | `		unsigned char c = (unsigned char)z[i];` |
|     75 | 6068 | `		if( c<=' ' ){ return 0; }` |
|     75 | 6069 | `		if( c=='.' && i+1<n && z[i+1]=='.' ){ return 0; }` |
|     37 | 6070 | `	}` |
|      7 | 6071 | `	return 1;` |
|      6 | 6072 | `}` |
|      - | 6073 | `/* FILTER_VALIDATE_URL: require a scheme and a host (PHP's filter is itself` |
|      - | 6074 | ` * parse_url-based, so PH7_VmHttpSplitURI tracks it closely). */` |
|     15 | 6075 | `static int FvValidateUrl(const char *z,int n){` |
|      - | 6076 | `	SyhttpUri sUri;` |
|     15 | 6077 | `	if( n==0 ){ return 0; }` |
|     15 | 6078 | `	SyZero(&sUri,(sxu32)sizeof(sUri));` |
|     15 | 6079 | `	if( PH7_VmHttpSplitURI(&sUri,z,(sxu32)n)!=SXRET_OK ){ return 0; }` |
|     15 | 6080 | `	return sUri.sScheme.nByte!=0 && sUri.sHost.nByte!=0;` |
|      8 | 6081 | `}` |
|      - | 6082 | `/* The Fv sanitizers build their result by appending directly to the call` |
|      - | 6083 | ` * context (ph7_result_string accumulates, like htmlspecialchars), emitting each` |
|      - | 6084 | ` * kept run in one call and seeding "" so an all-stripped input yields "". */` |
|      - | 6085 | `/* SANITIZE_NUMBER_INT (isFloat=0) / SANITIZE_NUMBER_FLOAT (isFloat=1). */` |
|     37 | 6086 | `static void FvSanitizeNumber(ph7_context *pCtx,const char *z,int n,int isFloat,int flags){` |
|     37 | 6087 | `	int i, runStart = 0;` |
|     37 | 6088 | `	ph7_result_string(pCtx,"",0);` |
|     97 | 6089 | `	for( i=0; i<n; i++ ){` |
|     91 | 6090 | `		char c = z[i];` |
|     91 | 6091 | `		int keep = (c>='0'&&c<='9') \|\| c=='+' \|\| c=='-';` |
|     91 | 6092 | `		if( !keep && isFloat ){` |
|     38 | 6093 | `			keep = (c=='.' && (flags & FV_FLAG_ALLOW_FRACTION))` |
|     23 | 6094 | `			    \|\| (c==',' && (flags & FV_FLAG_ALLOW_THOUSAND))` |
|     36 | 6095 | `			    \|\| ((c=='e'\|\|c=='E') && (flags & FV_FLAG_ALLOW_SCIENTIFIC));` |
|     12 | 6096 | `		}` |
|     61 | 6097 | `		if( !keep ){` |
|     33 | 6098 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     33 | 6099 | `			runStart = i+1;` |
|     16 | 6100 | `		}` |
|     31 | 6101 | `	}` |
|      7 | 6102 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      7 | 6103 | `}` |
|      - | 6104 | `/* Return non-zero when byte c must be stripped under the STRIP_* flags. Shared` |
|      - | 6105 | ` * by the UNSAFE_RAW string filter and SANITIZE_SPECIAL_CHARS. STRIP_LOW drops` |
|      - | 6106 | `` * bytes <32, STRIP_HIGH drops bytes >=127 (incl. DEL), STRIP_BACKTICK drops '`'.`` |
|      - | 6107 | ` * Matches php_filter_strip(); verified byte-exact vs php 8.5.7. */` |
|    287 | 6108 | `static int FvStripByte(unsigned char c,int flags){` |
|    287 | 6109 | `	if( (flags & FV_FLAG_STRIP_LOW)      && c<32 )    { return 1; }` |
|    281 | 6110 | `	if( (flags & FV_FLAG_STRIP_HIGH)     && c>=127 )  { return 1; }` |
|    269 | 6111 | `	if( (flags & FV_FLAG_STRIP_BACKTICK) && c==0x60 ) { return 1; }` |
|    267 | 6112 | `	return 0;` |
|    144 | 6113 | `}` |
|      - | 6114 | `/* FILTER_UNSAFE_RAW / FILTER_DEFAULT with flags: no default transform, but the` |
|      - | 6115 | ` * STRIP/ENCODE flags apply. Precedence (per php_filter_unsafe_raw, verified` |
|      - | 6116 | ` * vs php 8.5.7): a byte is first tested for stripping; a surviving byte is then` |
|      - | 6117 | ` * encoded as a decimal numeric entity if ENCODE_LOW (<32) / ENCODE_HIGH (>=127)` |
|      - | 6118 | ` * is set, and '&' becomes "&#38;" under ENCODE_AMP. So STRIP_LOW\|ENCODE_LOW` |
|      - | 6119 | ` * strips (nothing left to encode). Bytes are treated individually — ENCODE_HIGH` |
|      - | 6120 | ` * numeric-encodes each byte of a multibyte sequence separately, not the codepoint. */` |
|     25 | 6121 | `static void FvSanitizeString(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 6122 | `	int i, runStart = 0;` |
|     25 | 6123 | `	ph7_result_string(pCtx,"",0);` |
|    193 | 6124 | `	for( i=0; i<n; i++ ){` |
|    179 | 6125 | `		unsigned char c = (unsigned char)z[i];` |
|    179 | 6126 | `		if( FvStripByte(c,flags) ){` |
|     13 | 6127 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     13 | 6128 | `			runStart = i+1;` |
|     13 | 6129 | `			continue;` |
|      - | 6130 | `		}` |
|    167 | 6131 | `		if( c=='&' && (flags & FV_FLAG_ENCODE_AMP) ){` |
|      3 | 6132 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      3 | 6133 | `			ph7_result_string(pCtx,"&#38;",-1);` |
|      3 | 6134 | `			runStart = i+1;` |
|    166 | 6135 | `		}else if( (c<32 && (flags & FV_FLAG_ENCODE_LOW))` |
|    164 | 6136 | `		       \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     37 | 6137 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 6138 | `			ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|      9 | 6139 | `			runStart = i+1;` |
|      4 | 6140 | `		}` |
|     79 | 6141 | `	}` |
|     15 | 6142 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     15 | 6143 | `}` |
|      - | 6144 | `/* FILTER_SANITIZE_SPECIAL_CHARS: encode <>&"' and every control byte <32 as a` |
|      - | 6145 | ` * decimal numeric entity (&#60; &#38; &#34; ...). The STRIP_* flags remove bytes` |
|      - | 6146 | ` * before encoding; ENCODE_HIGH numeric-encodes surviving bytes >=127. Bytes >=128` |
|      - | 6147 | ` * are otherwise passed through verbatim (this filter is NOT UTF-8-aware — only the` |
|      - | 6148 | ` * FULL variant is). Byte-exact vs php 8.5.7. */` |
|     13 | 6149 | `static void FvSanitizeSpecial(ph7_context *pCtx,const char *z,int n,int flags){` |
|     13 | 6150 | `	int i, runStart = 0;` |
|      - | 6151 | `	const char *zEnt;` |
|     13 | 6152 | `	ph7_result_string(pCtx,"",0);` |
|    131 | 6153 | `	for( i=0; i<n; i++ ){` |
|    119 | 6154 | `		unsigned char c = (unsigned char)z[i];` |
|    119 | 6155 | `		if( FvStripByte(c,flags) ){` |
|      9 | 6156 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 6157 | `			runStart = i+1;` |
|      9 | 6158 | `			continue;` |
|      - | 6159 | `		}` |
|    111 | 6160 | `		switch( c ){` |
|      3 | 6161 | `		case '<':  zEnt = "&#60;"; break;` |
|      3 | 6162 | `		case '>':  zEnt = "&#62;"; break;` |
|     11 | 6163 | `		case '&':  zEnt = "&#38;"; break;` |
|      3 | 6164 | `		case '"':  zEnt = "&#34;"; break;` |
|      3 | 6165 | `		case '\'': zEnt = "&#39;"; break;` |
|     46 | 6166 | `		default:` |
|      - | 6167 | `			/* Control bytes <32 are always numeric-encoded; bytes >=127 only when` |
|      - | 6168 | `			 * ENCODE_HIGH is set. Everything else stays in the current run. */` |
|     93 | 6169 | `			if( c<32 \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     17 | 6170 | `				if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     17 | 6171 | `				ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|     17 | 6172 | `				runStart = i+1;` |
|      8 | 6173 | `			}` |
|     93 | 6174 | `			continue; /* keep in the current run */` |
|      - | 6175 | `		}` |
|     19 | 6176 | `		if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     19 | 6177 | `		ph7_result_string(pCtx,zEnt,-1); /* -1: length from strlen */` |
|     19 | 6178 | `		runStart = i+1;` |
|     10 | 6179 | `	}` |
|     13 | 6180 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     13 | 6181 | `}` |
|      - | 6182 | `/* HTML 4.01 named-entity table (codepoint -> "&name;") used by the UTF-8-aware` |
|      - | 6183 | ` * FULL_SPECIAL_CHARS filter, sorted ascending by codepoint for binary search.` |
|      - | 6184 | ` * Generated from php 8.5.7 (the exact set php_escape_html_entities emits for the` |
|      - | 6185 | ` * default document type); the five inline specials <>&"' are handled separately,` |
|      - | 6186 | ` * so every entry here is a codepoint >=0xA0. 248 rows. */` |
|      - | 6187 | `static const struct { sxu32 cp; const char *zEnt; } aHtml401Ent[] = {` |
|      - | 6188 | `	{0x00A0,"&nbsp;"},{0x00A1,"&iexcl;"},{0x00A2,"&cent;"},{0x00A3,"&pound;"},` |
|      - | 6189 | `	{0x00A4,"&curren;"},{0x00A5,"&yen;"},{0x00A6,"&brvbar;"},{0x00A7,"&sect;"},` |
|      - | 6190 | `	{0x00A8,"&uml;"},{0x00A9,"&copy;"},{0x00AA,"&ordf;"},{0x00AB,"&laquo;"},` |
|      - | 6191 | `	{0x00AC,"&not;"},{0x00AD,"&shy;"},{0x00AE,"&reg;"},{0x00AF,"&macr;"},` |
|      - | 6192 | `	{0x00B0,"&deg;"},{0x00B1,"&plusmn;"},{0x00B2,"&sup2;"},{0x00B3,"&sup3;"},` |
|      - | 6193 | `	{0x00B4,"&acute;"},{0x00B5,"&micro;"},{0x00B6,"&para;"},{0x00B7,"&middot;"},` |
|      - | 6194 | `	{0x00B8,"&cedil;"},{0x00B9,"&sup1;"},{0x00BA,"&ordm;"},{0x00BB,"&raquo;"},` |
|      - | 6195 | `	{0x00BC,"&frac14;"},{0x00BD,"&frac12;"},{0x00BE,"&frac34;"},{0x00BF,"&iquest;"},` |
|      - | 6196 | `	{0x00C0,"&Agrave;"},{0x00C1,"&Aacute;"},{0x00C2,"&Acirc;"},{0x00C3,"&Atilde;"},` |
|      - | 6197 | `	{0x00C4,"&Auml;"},{0x00C5,"&Aring;"},{0x00C6,"&AElig;"},{0x00C7,"&Ccedil;"},` |
|      - | 6198 | `	{0x00C8,"&Egrave;"},{0x00C9,"&Eacute;"},{0x00CA,"&Ecirc;"},{0x00CB,"&Euml;"},` |
|      - | 6199 | `	{0x00CC,"&Igrave;"},{0x00CD,"&Iacute;"},{0x00CE,"&Icirc;"},{0x00CF,"&Iuml;"},` |
|      - | 6200 | `	{0x00D0,"&ETH;"},{0x00D1,"&Ntilde;"},{0x00D2,"&Ograve;"},{0x00D3,"&Oacute;"},` |
|      - | 6201 | `	{0x00D4,"&Ocirc;"},{0x00D5,"&Otilde;"},{0x00D6,"&Ouml;"},{0x00D7,"&times;"},` |
|      - | 6202 | `	{0x00D8,"&Oslash;"},{0x00D9,"&Ugrave;"},{0x00DA,"&Uacute;"},{0x00DB,"&Ucirc;"},` |
|      - | 6203 | `	{0x00DC,"&Uuml;"},{0x00DD,"&Yacute;"},{0x00DE,"&THORN;"},{0x00DF,"&szlig;"},` |
|      - | 6204 | `	{0x00E0,"&agrave;"},{0x00E1,"&aacute;"},{0x00E2,"&acirc;"},{0x00E3,"&atilde;"},` |
|      - | 6205 | `	{0x00E4,"&auml;"},{0x00E5,"&aring;"},{0x00E6,"&aelig;"},{0x00E7,"&ccedil;"},` |
|      - | 6206 | `	{0x00E8,"&egrave;"},{0x00E9,"&eacute;"},{0x00EA,"&ecirc;"},{0x00EB,"&euml;"},` |
|      - | 6207 | `	{0x00EC,"&igrave;"},{0x00ED,"&iacute;"},{0x00EE,"&icirc;"},{0x00EF,"&iuml;"},` |
|      - | 6208 | `	{0x00F0,"&eth;"},{0x00F1,"&ntilde;"},{0x00F2,"&ograve;"},{0x00F3,"&oacute;"},` |
|      - | 6209 | `	{0x00F4,"&ocirc;"},{0x00F5,"&otilde;"},{0x00F6,"&ouml;"},{0x00F7,"&divide;"},` |
|      - | 6210 | `	{0x00F8,"&oslash;"},{0x00F9,"&ugrave;"},{0x00FA,"&uacute;"},{0x00FB,"&ucirc;"},` |
|      - | 6211 | `	{0x00FC,"&uuml;"},{0x00FD,"&yacute;"},{0x00FE,"&thorn;"},{0x00FF,"&yuml;"},` |
|      - | 6212 | `	{0x0152,"&OElig;"},{0x0153,"&oelig;"},{0x0160,"&Scaron;"},{0x0161,"&scaron;"},` |
|      - | 6213 | `	{0x0178,"&Yuml;"},{0x0192,"&fnof;"},{0x02C6,"&circ;"},{0x02DC,"&tilde;"},` |
|      - | 6214 | `	{0x0391,"&Alpha;"},{0x0392,"&Beta;"},{0x0393,"&Gamma;"},{0x0394,"&Delta;"},` |
|      - | 6215 | `	{0x0395,"&Epsilon;"},{0x0396,"&Zeta;"},{0x0397,"&Eta;"},{0x0398,"&Theta;"},` |
|      - | 6216 | `	{0x0399,"&Iota;"},{0x039A,"&Kappa;"},{0x039B,"&Lambda;"},{0x039C,"&Mu;"},` |
|      - | 6217 | `	{0x039D,"&Nu;"},{0x039E,"&Xi;"},{0x039F,"&Omicron;"},{0x03A0,"&Pi;"},` |
|      - | 6218 | `	{0x03A1,"&Rho;"},{0x03A3,"&Sigma;"},{0x03A4,"&Tau;"},{0x03A5,"&Upsilon;"},` |
|      - | 6219 | `	{0x03A6,"&Phi;"},{0x03A7,"&Chi;"},{0x03A8,"&Psi;"},{0x03A9,"&Omega;"},` |
|      - | 6220 | `	{0x03B1,"&alpha;"},{0x03B2,"&beta;"},{0x03B3,"&gamma;"},{0x03B4,"&delta;"},` |
|      - | 6221 | `	{0x03B5,"&epsilon;"},{0x03B6,"&zeta;"},{0x03B7,"&eta;"},{0x03B8,"&theta;"},` |
|      - | 6222 | `	{0x03B9,"&iota;"},{0x03BA,"&kappa;"},{0x03BB,"&lambda;"},{0x03BC,"&mu;"},` |
|      - | 6223 | `	{0x03BD,"&nu;"},{0x03BE,"&xi;"},{0x03BF,"&omicron;"},{0x03C0,"&pi;"},` |
|      - | 6224 | `	{0x03C1,"&rho;"},{0x03C2,"&sigmaf;"},{0x03C3,"&sigma;"},{0x03C4,"&tau;"},` |
|      - | 6225 | `	{0x03C5,"&upsilon;"},{0x03C6,"&phi;"},{0x03C7,"&chi;"},{0x03C8,"&psi;"},` |
|      - | 6226 | `	{0x03C9,"&omega;"},{0x03D1,"&thetasym;"},{0x03D2,"&upsih;"},{0x03D6,"&piv;"},` |
|      - | 6227 | `	{0x2002,"&ensp;"},{0x2003,"&emsp;"},{0x2009,"&thinsp;"},{0x200C,"&zwnj;"},` |
|      - | 6228 | `	{0x200D,"&zwj;"},{0x200E,"&lrm;"},{0x200F,"&rlm;"},{0x2013,"&ndash;"},` |
|      - | 6229 | `	{0x2014,"&mdash;"},{0x2018,"&lsquo;"},{0x2019,"&rsquo;"},{0x201A,"&sbquo;"},` |
|      - | 6230 | `	{0x201C,"&ldquo;"},{0x201D,"&rdquo;"},{0x201E,"&bdquo;"},{0x2020,"&dagger;"},` |
|      - | 6231 | `	{0x2021,"&Dagger;"},{0x2022,"&bull;"},{0x2026,"&hellip;"},{0x2030,"&permil;"},` |
|      - | 6232 | `	{0x2032,"&prime;"},{0x2033,"&Prime;"},{0x2039,"&lsaquo;"},{0x203A,"&rsaquo;"},` |
|      - | 6233 | `	{0x203E,"&oline;"},{0x2044,"&frasl;"},{0x20AC,"&euro;"},{0x2111,"&image;"},` |
|      - | 6234 | `	{0x2118,"&weierp;"},{0x211C,"&real;"},{0x2122,"&trade;"},{0x2135,"&alefsym;"},` |
|      - | 6235 | `	{0x2190,"&larr;"},{0x2191,"&uarr;"},{0x2192,"&rarr;"},{0x2193,"&darr;"},` |
|      - | 6236 | `	{0x2194,"&harr;"},{0x21B5,"&crarr;"},{0x21D0,"&lArr;"},{0x21D1,"&uArr;"},` |
|      - | 6237 | `	{0x21D2,"&rArr;"},{0x21D3,"&dArr;"},{0x21D4,"&hArr;"},{0x2200,"&forall;"},` |
|      - | 6238 | `	{0x2202,"&part;"},{0x2203,"&exist;"},{0x2205,"&empty;"},{0x2207,"&nabla;"},` |
|      - | 6239 | `	{0x2208,"&isin;"},{0x2209,"&notin;"},{0x220B,"&ni;"},{0x220F,"&prod;"},` |
|      - | 6240 | `	{0x2211,"&sum;"},{0x2212,"&minus;"},{0x2217,"&lowast;"},{0x221A,"&radic;"},` |
|      - | 6241 | `	{0x221D,"&prop;"},{0x221E,"&infin;"},{0x2220,"&ang;"},{0x2227,"&and;"},` |
|      - | 6242 | `	{0x2228,"&or;"},{0x2229,"&cap;"},{0x222A,"&cup;"},{0x222B,"&int;"},` |
|      - | 6243 | `	{0x2234,"&there4;"},{0x223C,"&sim;"},{0x2245,"&cong;"},{0x2248,"&asymp;"},` |
|      - | 6244 | `	{0x2260,"&ne;"},{0x2261,"&equiv;"},{0x2264,"&le;"},{0x2265,"&ge;"},` |
|      - | 6245 | `	{0x2282,"&sub;"},{0x2283,"&sup;"},{0x2284,"&nsub;"},{0x2286,"&sube;"},` |
|      - | 6246 | `	{0x2287,"&supe;"},{0x2295,"&oplus;"},{0x2297,"&otimes;"},{0x22A5,"&perp;"},` |
|      - | 6247 | `	{0x22C5,"&sdot;"},{0x2308,"&lceil;"},{0x2309,"&rceil;"},{0x230A,"&lfloor;"},` |
|      - | 6248 | `	{0x230B,"&rfloor;"},{0x2329,"&lang;"},{0x232A,"&rang;"},{0x25CA,"&loz;"},` |
|      - | 6249 | `	{0x2660,"&spades;"},{0x2663,"&clubs;"},{0x2665,"&hearts;"},{0x2666,"&diams;"}` |
|      - | 6250 | `};` |
|      - | 6251 | `/* Binary-search aHtml401Ent[] for cp; return its "&name;" entity or 0. */` |
|     41 | 6252 | `static const char *FvHtml401Lookup(sxu32 cp){` |
|     41 | 6253 | `	int lo = 0, hi = (int)SX_ARRAYSIZE(aHtml401Ent) - 1;` |
|    323 | 6254 | `	while( lo <= hi ){` |
|    309 | 6255 | `		int mid = (lo + hi) / 2;` |
|    309 | 6256 | `		sxu32 c = aHtml401Ent[mid].cp;` |
|    309 | 6257 | `		if( c == cp ){ return aHtml401Ent[mid].zEnt; }` |
|    283 | 6258 | `		if( c < cp ){ lo = mid + 1; } else { hi = mid - 1; }` |
|      1 | 6259 | `	}` |
|     15 | 6260 | `	return 0;` |
|     21 | 6261 | `}` |
|      - | 6262 | `/* Decode one strict-UTF-8 sequence at p (< zEnd). On success returns its byte` |
|      - | 6263 | ` * length (1..4) and sets *pCp to the codepoint; on any malformed, overlong,` |
|      - | 6264 | ` * surrogate, truncated or out-of-range (>U+10FFFF) sequence returns 0. Matches` |
|      - | 6265 | ` * PHP's UTF-8 validation used by FULL_SPECIAL_CHARS (verified vs php 8.5.7). */` |
|    101 | 6266 | `static int FvUtf8Next(const unsigned char *p,const unsigned char *zEnd,sxu32 *pCp){` |
|    101 | 6267 | `	unsigned char c = p[0];` |
|    101 | 6268 | `	if( c < 0x80 ){ *pCp = c; return 1; }` |
|    101 | 6269 | `	if( c < 0xC2 ){ return 0; }              /* 0x80-0xBF stray cont / 0xC0-0xC1 overlong */` |
|     99 | 6270 | `	if( c < 0xE0 ){                          /* 2-byte: U+0080..U+07FF */` |
|     47 | 6271 | `		if( zEnd-p < 2 \|\| (p[1]&0xC0)!=0x80 ){ return 0; }` |
|     45 | 6272 | `		*pCp = ((sxu32)(c&0x1F)<<6) \| (p[1]&0x3F);` |
|     45 | 6273 | `		return 2;` |
|      - | 6274 | `	}` |
|     53 | 6275 | `	if( c < 0xF0 ){                          /* 3-byte: U+0800..U+FFFF minus surrogates */` |
|      - | 6276 | `		sxu32 cp;` |
|     47 | 6277 | `		if( zEnd-p < 3 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 ){ return 0; }` |
|     33 | 6278 | `		cp = ((sxu32)(c&0x0F)<<12) \| ((sxu32)(p[1]&0x3F)<<6) \| (p[2]&0x3F);` |
|     33 | 6279 | `		if( cp < 0x800 \|\| (cp>=0xD800 && cp<=0xDFFF) ){ return 0; }` |
|     29 | 6280 | `		*pCp = cp;` |
|     29 | 6281 | `		return 3;` |
|      - | 6282 | `	}` |
|      7 | 6283 | `	if( c < 0xF5 ){                          /* 4-byte: U+10000..U+10FFFF */` |
|      - | 6284 | `		sxu32 cp;` |
|      5 | 6285 | `		if( zEnd-p < 4 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 \|\| (p[3]&0xC0)!=0x80 ){ return 0; }` |
|      5 | 6286 | `		cp = ((sxu32)(c&0x07)<<18) \| ((sxu32)(p[1]&0x3F)<<12) \| ((sxu32)(p[2]&0x3F)<<6) \| (p[3]&0x3F);` |
|      5 | 6287 | `		if( cp < 0x10000 \|\| cp > 0x10FFFF ){ return 0; }` |
|      5 | 6288 | `		*pCp = cp;` |
|      5 | 6289 | `		return 4;` |
|      - | 6290 | `	}` |
|      3 | 6291 | `	return 0;                                /* 0xF5-0xFF */` |
|     51 | 6292 | `}` |
|      - | 6293 | `/* FILTER_SANITIZE_FULL_SPECIAL_CHARS: htmlentities-style, UTF-8-aware. Encodes` |
|      - | 6294 | ` * <>&"' as named entities ("'" -> &#039;; quotes suppressed under NO_ENCODE_QUOTES),` |
|      - | 6295 | ` * and every valid UTF-8 codepoint with an HTML 4.01 named entity as that entity;` |
|      - | 6296 | ` * valid codepoints without a named entity (and low control bytes) pass through` |
|      - | 6297 | ` * verbatim. If the input contains ANY invalid UTF-8 the whole result is "".` |
|      - | 6298 | ` * The STRIP/ENCODE flags do NOT apply to this filter (only NO_ENCODE_QUOTES).` |
|      - | 6299 | ` * php's filter does NOT re-encode valid pre-existing entities ("&amp;" stays,` |
|      - | 6300 | ` * "&bogus;" becomes "&amp;bogus;"), i.e. double_encode=false semantics —` |
|      - | 6301 | ` * exactly htmlentities(ENT_QUOTES\|ENT_HTML401, double_encode: false), so this` |
|      - | 6302 | ` * delegates to the shared encoder. Byte-exact vs php 8.5.7. */` |
|     25 | 6303 | `static void FvSanitizeFull(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 6304 | `	int iEntFlags = (flags & FV_FLAG_NO_ENCODE_QUOTES) ? 0 : PH7_ENT_QUOTES;` |
|     25 | 6305 | `	HtmlEscape(pCtx,z,n,iEntFlags,1/*bAll*/,0/*bDoubleEncode*/);` |
|     25 | 6306 | `}` |
|      - | 6307 | `/* ---------------------------------------------------------------------------` |
|      - | 6308 | ` * UTF-8-aware HTML entity core (htmlspecialchars/htmlentities family).` |
|      - | 6309 | ` * Prototyped next to the five builtins earlier in this file; lives here so it` |
|      - | 6310 | ` * can share aHtml401Ent[]/FvHtml401Lookup()/FvUtf8Next() with the filter_var` |
|      - | 6311 | ` * FULL_SPECIAL_CHARS filter above. Byte-exact vs php 8.5.7 (oracle-swept).` |
|      - | 6312 | ` * ------------------------------------------------------------------------ */` |
|      - | 6313 | `/* Encode cp as UTF-8 into zBuf (>= 4 bytes); return the byte length 1..4.` |
|      - | 6314 | ` * Thin wrapper over the engine-wide SX_WRITE_UTF8 (sxmacros.h). */` |
|    585 | 6315 | `static int HtmlCpUtf8(sxu32 cp,char *zBuf){` |
|    585 | 6316 | `	sxu8 *z = (sxu8 *)zBuf;` |
|    585 | 6317 | `	SX_WRITE_UTF8(z,cp);` |
|    585 | 6318 | `	return (int)(z - (sxu8 *)zBuf);` |
|      1 | 6319 | `}` |
|      - | 6320 | `/* Doctype-allowed codepoint test (php's unicode_cp_is_allowed) — gates what a` |
|      - | 6321 | ` * numeric reference may DECODE to. Oracle-pinned per doctype: HTML401` |
|      - | 6322 | ` * disallows C0 (except TAB/LF/CR) and DEL..U+009F; XML1 and XHTML share the` |
|      - | 6323 | ` * XML rules — DEL..U+009F allowed, U+FFFE/U+FFFF excluded; HTML5 swaps CR` |
|      - | 6324 | ` * for FF (0x0C) and excludes the noncharacters (U+FDD0..U+FDEF and every` |
|      - | 6325 | ` * U+xFFFE/U+xFFFF). Surrogates are disallowed everywhere. */` |
|     91 | 6326 | `static int HtmlCpAllowed(sxu32 cp,int iFlags){` |
|     91 | 6327 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|     91 | 6328 | `	if( cp==0x09 \|\| cp==0x0A ){ return 1; }` |
|     87 | 6329 | `	if( cp==0x0D ){ return iDoc != PH7_ENT_DOC_HTML5; }` |
|     85 | 6330 | `	if( cp==0x0C ){ return iDoc == PH7_ENT_DOC_HTML5; }` |
|     85 | 6331 | `	if( cp < 0x20 \|\| cp > 0x10FFFF ){ return 0; }` |
|     79 | 6332 | `	if( cp>=0xD800 && cp<=0xDFFF ){ return 0; }` |
|     77 | 6333 | `	if( cp>=0x7F && cp<=0x9F ){ return iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML; }` |
|     71 | 6334 | `	if( iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML ){` |
|    ! 0 | 6335 | `		return cp!=0xFFFE && cp!=0xFFFF;` |
|      - | 6336 | `	}` |
|     71 | 6337 | `	if( iDoc == PH7_ENT_DOC_HTML5 ){` |
|      9 | 6338 | `		if( cp>=0xFDD0 && cp<=0xFDEF ){ return 0; }` |
|      9 | 6339 | `		if( (cp & 0xFFFF) >= 0xFFFE ){ return 0; }` |
|      4 | 6340 | `	}` |
|     71 | 6341 | `	return 1;` |
|     46 | 6342 | `}` |
|      - | 6343 | `/* The ENT_DISALLOWED gate for RAW characters on the ENCODE side. Same as the` |
|      - | 6344 | ` * decode gate except CR under HTML5: php's encode-side unicode_cp_is_allowed` |
|      - | 6345 | ` * keeps a literal "\r" verbatim under ENT_HTML5\|ENT_DISALLOWED while the` |
|      - | 6346 | ` * decode side leaves "&#13;" un-decoded (oracle-pinned at flags 176). */` |
|      9 | 6347 | `static int HtmlCpAllowedEncode(sxu32 cp,int iFlags){` |
|      9 | 6348 | `	if( cp==0x0D && (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 ){ return 1; }` |
|      9 | 6349 | `	return HtmlCpAllowed(cp,iFlags);` |
|      5 | 6350 | `}` |
|      - | 6351 | `/* Numeric-reference validity for the double_encode=false "is this already a` |
|      - | 6352 | ` * valid entity" test — a MUCH looser predicate than the decode gate above:` |
|      - | 6353 | ` * any codepoint <= U+10FFFF is valid (controls and surrogates included, every` |
|      - | 6354 | ` * doctype). ENT_DISALLOWED re-tightens non-HTML401 doctypes to the decode` |
|      - | 6355 | ` * gate, except that HTML5 exempts surrogates. All oracle-pinned: &#0; and` |
|      - | 6356 | ` * &#xD800; stay verbatim at flags 11 and 139; flags -1 (HTML5+DISALLOWED)` |
|      - | 6357 | ` * re-encodes &#0; and &#x10FFFF; but still keeps &#xD800;; flags 144` |
|      - | 6358 | ` * (XML1+DISALLOWED) re-encodes &#xD800;. */` |
|      9 | 6359 | `static int HtmlNumericAllowed(sxu32 cp,int iFlags){` |
|      9 | 6360 | `	if( cp > 0x10FFFF ){ return 0; }` |
|      7 | 6361 | `	if( (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML401 ){ return 1; /* never tightened */ }` |
|    ! 0 | 6362 | `	if( (iFlags & PH7_ENT_DISALLOWED)` |
|    ! 0 | 6363 | `	 && !((iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 && cp>=0xD800 && cp<=0xDFFF)` |
|    ! 0 | 6364 | `	 && !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|    ! 0 | 6365 | `	return 1;` |
|      5 | 6366 | `}` |
|      - | 6367 | `/* How many bytes the malformed UTF-8 sequence at p consumes — php's` |
|      - | 6368 | ` * get_next_char failure step (one U+FFFD substitution / one ENT_IGNORE drop` |
|      - | 6369 | ` * per MAXIMAL invalid subpart, not per byte): a prefix-valid sequence eats` |
|      - | 6370 | ` * its continuation bytes ("\xE0\x80\xAF" is ONE unit) while a byte that could` |
|      - | 6371 | ` * start a new sequence is left for the next round. */` |
|      5 | 6372 | `static int HtmlUtf8Trail(unsigned char c){ return c>=0x80 && c<=0xBF; }` |
|     11 | 6373 | `static int HtmlUtf8Lead(unsigned char c){ return c<0x80 \|\| (c>=0xC2 && c<=0xF4); }` |
|     15 | 6374 | `static int HtmlUtf8FailAdvance(const unsigned char *p,const unsigned char *zEnd){` |
|     15 | 6375 | `	unsigned char c = p[0];` |
|     15 | 6376 | `	int nAvail = (int)(zEnd - p);` |
|     15 | 6377 | `	if( c < 0xC2 \|\| c > 0xF4 ){ return 1; } /* stray trail / C0-C1 / F5-FF */` |
|     13 | 6378 | `	if( c < 0xE0 ){` |
|      3 | 6379 | `		if( nAvail < 2 ){ return 1; }` |
|      3 | 6380 | `		return HtmlUtf8Lead(p[1]) ? 1 : 2;` |
|      - | 6381 | `	}` |
|     11 | 6382 | `	if( c < 0xF0 ){` |
|     11 | 6383 | `		if( nAvail >= 3 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) ){` |
|      3 | 6384 | `			return 3; /* complete but overlong/surrogate */` |
|      - | 6385 | `		}` |
|      9 | 6386 | `		if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 6387 | `		if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 6388 | `		return 3;` |
|      - | 6389 | `	}` |
|    ! 0 | 6390 | `	if( nAvail >= 4 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) && HtmlUtf8Trail(p[3]) ){` |
|    ! 0 | 6391 | `		return 4; /* complete but overlong / > U+10FFFF */` |
|      - | 6392 | `	}` |
|    ! 0 | 6393 | `	if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 6394 | `	if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 6395 | `	if( nAvail < 4 \|\| HtmlUtf8Lead(p[3]) ){ return 3; }` |
|    ! 0 | 6396 | `	return 4;` |
|      8 | 6397 | `}` |
|      - | 6398 | `/* The basic special entities, shared by named matching, the hsc_decode` |
|      - | 6399 | ` * numeric whitelist and the translation-table builder so the sets can never` |
|      - | 6400 | ` * drift apart. (&apos; is not an HTML 4.01 entity — doctype-gated below.) */` |
|      - | 6401 | `static const struct { const char *zEnt; int n; sxu32 cp; } aHtmlSpecEnt[] = {` |
|      - | 6402 | `	{"&amp;",5,38},{"&lt;",4,60},{"&gt;",4,62},{"&quot;",6,34},{"&apos;",6,39}` |
|      - | 6403 | `};` |
|      - | 6404 | `/* Does this doctype consult the named-entity table (aHtml401Ent)? XML 1.0 has` |
|      - | 6405 | ` * no named entities beyond the specials; XHTML/HTML5 are approximated by the` |
|      - | 6406 | ` * HTML 4.01 table (documented divergence). */` |
|     63 | 6407 | `static int HtmlDocHasNamedTable(int iDoc){` |
|     63 | 6408 | `	return iDoc != PH7_ENT_DOC_XML1;` |
|      1 | 6409 | `}` |
|      - | 6410 | `/* The single-quote entity per doctype. Oracle-pinned asymmetry: for every` |
|      - | 6411 | ` * non-HTML401 doctype htmlspecialchars emits &apos; while htmlentities` |
|      - | 6412 | ` * (bEntities) keeps &#039; under XHTML too. The translation table mirrors` |
|      - | 6413 | ` * whichever function the requested table belongs to. */` |
|     29 | 6414 | `static const char *HtmlAposEntity(int iDoc,int bEntities){` |
|     29 | 6415 | `	if( iDoc == PH7_ENT_DOC_HTML401 \|\| (bEntities && iDoc == PH7_ENT_DOC_XHTML) ){` |
|     21 | 6416 | `		return "&#039;";` |
|      - | 6417 | `	}` |
|      9 | 6418 | `	return "&apos;";` |
|     15 | 6419 | `}` |
|      - | 6420 | `/* Try to parse one HTML entity at z (z[0]=='&', z < zEnd). bFull selects the` |
|      - | 6421 | ` * html_entity_decode set (doctype named table + any allowed numeric ref) vs` |
|      - | 6422 | ` * the htmlspecialchars_decode set (the basic specials + quote numerics only).` |
|      - | 6423 | ` * Named matching is case-SENSITIVE and the ';' is required (both PHP-exact);` |
|      - | 6424 | ` * numeric refs accept dec/hex (x or X) with any number of leading zeros but` |
|      - | 6425 | ` * reject out-of-range, surrogate and doctype-disallowed codepoints (the` |
|      - | 6426 | ` * caller then leaves the source verbatim). Quote-flag gating is NOT applied` |
|      - | 6427 | ` * here — the same routine doubles as the "is this a valid entity" test for` |
|      - | 6428 | ` * double_encode=false, which ignores the quote bits (oracle-pinned).` |
|      - | 6429 | ` * bEncodeCheck selects the looser HtmlNumericAllowed predicate used by that` |
|      - | 6430 | ` * double_encode test; decode callers pass 0 for the HtmlCpAllowed gate.` |
|      - | 6431 | ` * On success sets *pCp / *pnConsumed and returns 1. */` |
|    172 | 6432 | `static int HtmlParseEntity(const unsigned char *z,const unsigned char *zEnd,` |
|      1 | 6433 | `                           int iFlags,int bFull,int bEncodeCheck,sxu32 *pCp,int *pnConsumed){` |
|    173 | 6434 | `	int nAvail = (int)(zEnd - z);` |
|    173 | 6435 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6436 | `	sxu32 n;` |
|    173 | 6437 | `	if( nAvail < 4 ){ return 0; } /* shortest entities: &lt; &#9; */` |
|    169 | 6438 | `	if( z[1] == '#' ){` |
|      - | 6439 | `		/* Numeric reference */` |
|     89 | 6440 | `		sxu32 cp = 0;` |
|     89 | 6441 | `		int i = 2, bHex = 0, nDig = 0;` |
|     89 | 6442 | `		if( z[i]=='x' \|\| z[i]=='X' ){ bHex = 1; i++; }` |
|    317 | 6443 | `		for( ; i < nAvail && z[i] != ';' ; i++ ){` |
|      - | 6444 | `			int v;` |
|    221 | 6445 | `			unsigned char c = z[i];` |
|    221 | 6446 | `			if( c>='0' && c<='9' ){ v = c - '0'; }` |
|     17 | 6447 | `			else if( bHex && c>='a' && c<='f' ){ v = c - 'a' + 10; }` |
|     17 | 6448 | `			else if( bHex && c>='A' && c<='F' ){ v = c - 'A' + 10; }` |
|    ! 0 | 6449 | `			else { return 0; }` |
|      - | 6450 | `			/* Stop accumulating once out of range (keeps validating the shape;` |
|      - | 6451 | `			 * max intermediate is 0x10FFFF*16+15, no sxu32 overflow). */` |
|    221 | 6452 | `			if( cp <= 0x10FFFF ){ cp = cp * (bHex ? 16 : 10) + (sxu32)v; }` |
|    221 | 6453 | `			nDig++;` |
|    111 | 6454 | `		}` |
|     97 | 6455 | `		if( nDig == 0 \|\| i >= nAvail ){ return 0; } /* no digits / no ';' */` |
|     97 | 6456 | `		if( bEncodeCheck ? !HtmlNumericAllowed(cp,iFlags) : !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|     83 | 6457 | `		if( !bFull ){` |
|      - | 6458 | `			/* hsc_decode: numeric refs to the five specials only. */` |
|     99 | 6459 | `			for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) && aHtmlSpecEnt[n].cp != cp ; n++ ){}` |
|     25 | 6460 | `			if( n >= SX_ARRAYSIZE(aHtmlSpecEnt) ){ return 0; }` |
|     11 | 6461 | `		}` |
|     75 | 6462 | `		*pCp = cp;` |
|     75 | 6463 | `		*pnConsumed = i + 1;` |
|     75 | 6464 | `		return 1;` |
|      - | 6465 | `	}` |
|      - | 6466 | `	/* Named reference — every entity name starts with a letter, so anything` |
|      - | 6467 | `	 * else can bail out before touching the tables. */` |
|     81 | 6468 | `	if( !((z[1]>='a' && z[1]<='z') \|\| (z[1]>='A' && z[1]<='Z')) ){ return 0; }` |
|    287 | 6469 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) ; n++ ){` |
|    265 | 6470 | `		if( aHtmlSpecEnt[n].cp == 39 && iDoc == PH7_ENT_DOC_HTML401 ){ continue; }` |
|    243 | 6471 | `		if( nAvail >= aHtmlSpecEnt[n].n && SyMemcmp(z,aHtmlSpecEnt[n].zEnt,(sxu32)aHtmlSpecEnt[n].n) == 0 ){` |
|     53 | 6472 | `			*pCp = aHtmlSpecEnt[n].cp;` |
|     53 | 6473 | `			*pnConsumed = aHtmlSpecEnt[n].n;` |
|     53 | 6474 | `			return 1;` |
|      - | 6475 | `		}` |
|     96 | 6476 | `	}` |
|     23 | 6477 | `	if( bFull && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 6478 | `		/* Linear scan of the 248-row table: runs only at '&'-then-letter` |
|      - | 6479 | `		 * positions and guarantees the decode set can never drift from the` |
|      - | 6480 | `		 * encode table. The first-letter guard skips the SyStrlen/SyMemcmp` |
|      - | 6481 | `		 * for ~96% of rows. */` |
|   3369 | 6482 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|      - | 6483 | `			sxu32 nEnt;` |
|   3357 | 6484 | `			if( z[1] != (unsigned char)aHtml401Ent[n].zEnt[1] ){ continue; }` |
|    121 | 6485 | `			nEnt = SyStrlen(aHtml401Ent[n].zEnt);` |
|    121 | 6486 | `			if( (sxu32)nAvail >= nEnt && SyMemcmp(z,aHtml401Ent[n].zEnt,nEnt) == 0 ){` |
|      7 | 6487 | `				*pCp = aHtml401Ent[n].cp;` |
|      7 | 6488 | `				*pnConsumed = (int)nEnt;` |
|      7 | 6489 | `				return 1;` |
|      - | 6490 | `			}` |
|     58 | 6491 | `		}` |
|      6 | 6492 | `	}` |
|     17 | 6493 | `	return 0;` |
|     88 | 6494 | `}` |
|      - | 6495 | `/* Shared encoder for htmlspecialchars (bAll=0) and htmlentities (bAll=1).` |
|      - | 6496 | ` * Invalid UTF-8 policy: ENT_IGNORE drops the byte (and wins over SUBSTITUTE),` |
|      - | 6497 | ` * ENT_SUBSTITUTE emits one U+FFFD per invalid byte, neither -> the whole` |
|      - | 6498 | ` * result is "" (pre-validated in a first pass: the accumulating result API` |
|      - | 6499 | ` * cannot roll back — same reason FvSanitizeFull is two-pass). */` |
|     96 | 6500 | `static void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 6501 | `                       int iFlags,int bAll,int bDoubleEncode){` |
|     97 | 6502 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     97 | 6503 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|      - | 6504 | `	const unsigned char *runStart;` |
|     97 | 6505 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6506 | `	sxu32 cp;` |
|     97 | 6507 | `	if( (iFlags & (PH7_ENT_IGNORE\|PH7_ENT_SUBSTITUTE)) == 0 ){` |
|      - | 6508 | `		/* Pass 1: any malformed sequence rejects the entire input. ASCII` |
|      - | 6509 | `		 * bytes cannot be malformed, so skip them without the decoder. */` |
|    381 | 6510 | `		while( p < zEnd ){` |
|      - | 6511 | `			int len;` |
|    323 | 6512 | `			if( *p < 0x80 ){ p++; continue; }` |
|     37 | 6513 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     37 | 6514 | `			if( len == 0 ){ ph7_result_string(pCtx,"",0); return; }` |
|     27 | 6515 | `			p += len;` |
|      1 | 6516 | `		}` |
|     59 | 6517 | `		p = (const unsigned char *)zIn;` |
|     29 | 6518 | `	}` |
|     87 | 6519 | `	runStart = p;` |
|     87 | 6520 | `	ph7_result_string(pCtx,"",0);` |
|    463 | 6521 | `	while( p < zEnd ){` |
|    377 | 6522 | `		const char *zEnt = 0;` |
|      - | 6523 | `		int len;` |
|    377 | 6524 | `		if( *p < 0x80 ){` |
|    313 | 6525 | `			len = 1;` |
|    313 | 6526 | `			switch( *p ){` |
|     25 | 6527 | `			case '<': zEnt = "&lt;"; break;` |
|     25 | 6528 | `			case '>': zEnt = "&gt;"; break;` |
|     18 | 6529 | `			case '&':` |
|     37 | 6530 | `				zEnt = "&amp;";` |
|     37 | 6531 | `				if( !bDoubleEncode ){` |
|      - | 6532 | `					sxu32 eCp; int nEat;` |
|     25 | 6533 | `					if( HtmlParseEntity(p,zEnd,iFlags,1,1,&eCp,&nEat) ){` |
|      - | 6534 | `						/* A valid existing entity: keep it verbatim. */` |
|     13 | 6535 | `						zEnt = 0;` |
|     13 | 6536 | `						len = nEat;` |
|      6 | 6537 | `					}` |
|     12 | 6538 | `				}` |
|     37 | 6539 | `				break;` |
|     10 | 6540 | `			case '"':` |
|     21 | 6541 | `				if( iFlags & PH7_ENT_QUOTE_DOUBLE ){ zEnt = "&quot;"; }` |
|     21 | 6542 | `				break;` |
|     12 | 6543 | `			case '\'':` |
|     25 | 6544 | `				if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|     23 | 6545 | `					zEnt = HtmlAposEntity(iDoc,bAll);` |
|     11 | 6546 | `				}` |
|     25 | 6547 | `				break;` |
|     92 | 6548 | `			default:` |
|    185 | 6549 | `				if( (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode((sxu32)*p,iFlags) ){` |
|    ! 0 | 6550 | `					zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 6551 | `				}` |
|    184 | 6552 | `				break;` |
|      - | 6553 | `			}` |
|    157 | 6554 | `		}else{` |
|     65 | 6555 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     65 | 6556 | `			if( len == 0 ){` |
|      - | 6557 | `				/* Malformed subpart (IGNORE or SUBSTITUTE is set, else pass 1` |
|      - | 6558 | `				 * would have rejected): drop it or emit ONE U+FFFD for the` |
|      - | 6559 | `				 * whole unit (php substitutes per maximal invalid subpart). */` |
|     15 | 6560 | `				if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|     15 | 6561 | `				if( (iFlags & PH7_ENT_IGNORE) == 0 ){ ph7_result_string(pCtx,"\xEF\xBF\xBD",3); }` |
|     15 | 6562 | `				p += HtmlUtf8FailAdvance(p,zEnd);` |
|     15 | 6563 | `				runStart = p;` |
|     15 | 6564 | `				continue;` |
|      - | 6565 | `			}` |
|     51 | 6566 | `			if( bAll && HtmlDocHasNamedTable(iDoc) ){` |
|     41 | 6567 | `				zEnt = FvHtml401Lookup(cp);` |
|     20 | 6568 | `			}` |
|     51 | 6569 | `			if( zEnt == 0 && (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode(cp,iFlags) ){` |
|    ! 0 | 6570 | `				zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 6571 | `			}` |
|      - | 6572 | `		}` |
|    363 | 6573 | `		if( zEnt ){` |
|    135 | 6574 | `			if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|    135 | 6575 | `			ph7_result_string(pCtx,zEnt,-1);` |
|    135 | 6576 | `			runStart = p + len;` |
|     67 | 6577 | `		}` |
|    363 | 6578 | `		p += len;` |
|      1 | 6579 | `	}` |
|     87 | 6580 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     49 | 6581 | `}` |
|      - | 6582 | `/* Shared decoder for html_entity_decode (bFull=1) and htmlspecialchars_decode` |
|      - | 6583 | ` * (bFull=0). Quote refs (cp 34/39, named or numeric) are gated by the quote` |
|      - | 6584 | ` * bits and left verbatim when suppressed; an invalid entity leaves its '&'` |
|      - | 6585 | ` * verbatim and rescans right after it, which also yields PHP's no-double-` |
|      - | 6586 | ` * decode behavior ("&amp;lt;" -> "&lt;"). */` |
|     84 | 6587 | `static void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 6588 | `                         int iFlags,int bFull){` |
|     85 | 6589 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     85 | 6590 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|     85 | 6591 | `	const unsigned char *runStart = p;` |
|     85 | 6592 | `	ph7_result_string(pCtx,"",0);` |
|    565 | 6593 | `	while( p < zEnd ){` |
|      - | 6594 | `		sxu32 cp;` |
|      - | 6595 | `		int nEat;` |
|    516 | 6596 | `		if( *p != '&' ){ p++; continue; }` |
|    155 | 6597 | `		if( !HtmlParseEntity(p,zEnd,iFlags,bFull,0,&cp,&nEat) ){ p++; continue; }` |
|    124 | 6598 | `		if( (cp == 34 && (iFlags & PH7_ENT_QUOTE_DOUBLE) == 0)` |
|    117 | 6599 | `		 \|\| (cp == 39 && (iFlags & PH7_ENT_QUOTE_SINGLE) == 0) ){` |
|      - | 6600 | `			/* Suppressed quote: leave the entity source verbatim. */` |
|     37 | 6601 | `			p += nEat;` |
|     37 | 6602 | `			continue;` |
|      - | 6603 | `		}` |
|     89 | 6604 | `		if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|      - | 6605 | `		{` |
|      - | 6606 | `			char zBuf[4];` |
|     89 | 6607 | `			int n = HtmlCpUtf8(cp,zBuf);` |
|     89 | 6608 | `			ph7_result_string(pCtx,zBuf,n);` |
|      - | 6609 | `		}` |
|     89 | 6610 | `		p += nEat;` |
|     89 | 6611 | `		runStart = p;` |
|      1 | 6612 | `	}` |
|     81 | 6613 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     81 | 6614 | `}` |
|      - | 6615 | `/* Validate the optional charset argument at apArg[idx]: UTF-8 aliases (and` |
|      - | 6616 | ` * ""/NULL meaning the default) are accepted; anything else — including` |
|      - | 6617 | ` * php-supported single-byte charsets like ISO-8859-1, PHL is UTF-8-only by` |
|      - | 6618 | ` * policy — raises PHP's unsupported-charset warning and is treated as` |
|      - | 6619 | ` * UTF-8 (ph7_context_throw_error_format prepends the function name). */` |
|    143 | 6620 | `static void HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx){` |
|      - | 6621 | `	const char *zCs;` |
|      - | 6622 | `	int nCs;` |
|    150 | 6623 | `	if( nArg <= idx \|\| ph7_value_is_null(apArg[idx]) ){ return; }` |
|     15 | 6624 | `	zCs = ph7_value_to_string(apArg[idx],&nCs);` |
|     15 | 6625 | `	if( nCs == 0 ){ return; } /* "" selects the default charset (UTF-8) */` |
|     13 | 6626 | `	if( nCs == 5 && SyStrnicmp(zCs,"UTF-8",5) == 0 ){` |
|     13 | 6627 | `		return; /* php accepts only "UTF-8" (any case) silently — "UTF8" warns */` |
|      - | 6628 | `	}` |
|    ! 0 | 6629 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 6630 | `		"Charset \"%.*s\" is not supported, assuming UTF-8",nCs,zCs);` |
|     72 | 6631 | `}` |
|      - | 6632 | `/* get_html_translation_table() worker: character (UTF-8 bytes) => entity.` |
|      - | 6633 | ` * The five specials come first in byte order, then — for HTML_ENTITIES with a` |
|      - | 6634 | ` * named-table doctype — the 248 aHtml401Ent rows ascending (oracle-pinned` |
|      - | 6635 | ` * ordering; 253 entries under the defaults). */` |
|    549 | 6636 | `static void HtmlTableAdd(ph7_value *pArray,ph7_value *pValue,const char *zKey,const char *zEnt){` |
|    549 | 6637 | `	ph7_value_string(pValue,zEnt,-1);` |
|    549 | 6638 | `	ph7_array_add_strkey_elem(pArray,zKey,pValue);` |
|    549 | 6639 | `	ph7_value_reset_string_cursor(pValue);` |
|    549 | 6640 | `}` |
|     13 | 6641 | `static void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags){` |
|      - | 6642 | `	ph7_value *pArray,*pValue;` |
|     13 | 6643 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6644 | `	sxu32 n;` |
|     13 | 6645 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 6646 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 6647 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|    ! 0 | 6648 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6649 | `		return;` |
|      - | 6650 | `	}` |
|     13 | 6651 | `	if( iFlags & PH7_ENT_QUOTE_DOUBLE ){` |
|     11 | 6652 | `		HtmlTableAdd(pArray,pValue,"\"","&quot;");` |
|      5 | 6653 | `	}` |
|     13 | 6654 | `	HtmlTableAdd(pArray,pValue,"&","&amp;");` |
|     13 | 6655 | `	if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|      - | 6656 | `		/* The apostrophe row mirrors the function each table belongs to:` |
|      - | 6657 | `		 * SPECIALCHARS follows htmlspecialchars, ENTITIES follows` |
|      - | 6658 | `		 * htmlentities (oracle-pinned at flags 35). */` |
|      7 | 6659 | `		HtmlTableAdd(pArray,pValue,"'",HtmlAposEntity(iDoc,iTable != 0));` |
|      3 | 6660 | `	}` |
|     13 | 6661 | `	HtmlTableAdd(pArray,pValue,"<","&lt;");` |
|     13 | 6662 | `	HtmlTableAdd(pArray,pValue,">","&gt;");` |
|     13 | 6663 | `	if( iTable != 0 /*php: any non-HTML_SPECIALCHARS table => entities*/ && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 6664 | `		char zKey[8];` |
|    499 | 6665 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|    497 | 6666 | `			int nK = HtmlCpUtf8(aHtml401Ent[n].cp,zKey);` |
|    497 | 6667 | `			zKey[nK] = 0;` |
|    497 | 6668 | `			HtmlTableAdd(pArray,pValue,zKey,aHtml401Ent[n].zEnt);` |
|    249 | 6669 | `		}` |
|      1 | 6670 | `	}` |
|     13 | 6671 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 6672 | `}` |
|     25 | 6673 | `static int FvEmailAllowed(unsigned char c){` |
|     25 | 6674 | `	if( (c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9') ){ return 1; }` |
|     16 | 6675 | `	return c=='!'\|\|c=='#'\|\|c=='$'\|\|c=='%'\|\|c=='&'\|\|c=='\''\|\|c=='*'\|\|c=='+'` |
|     10 | 6676 | ``	    \|\| c=='-'\|\|c=='='\|\|c=='?'\|\|c=='^'\|\|c=='_'\|\|c=='`'\|\|c=='{'\|\|c=='\|'`` |
|     15 | 6677 | `	    \|\| c=='}'\|\|c=='~'\|\|c=='@'\|\|c=='.'\|\|c=='['\|\|c==']';` |
|     13 | 6678 | `}` |
|     23 | 6679 | `static int FvUrlAllowed(unsigned char c){` |
|     23 | 6680 | `	return c>=33 && c<=126; /* PHP keeps every printable ASCII byte except space */` |
|      1 | 6681 | `}` |
|      - | 6682 | `/* SANITIZE_EMAIL (isUrl=0) / SANITIZE_URL (isUrl=1): strip disallowed bytes. */` |
|      5 | 6683 | `static void FvSanitizeChars(ph7_context *pCtx,const char *z,int n,int isUrl){` |
|      5 | 6684 | `	int i, runStart = 0;` |
|      5 | 6685 | `	ph7_result_string(pCtx,"",0);` |
|     51 | 6686 | `	for( i=0; i<n; i++ ){` |
|     47 | 6687 | `		unsigned char c = (unsigned char)z[i];` |
|     47 | 6688 | `		if( !(isUrl ? FvUrlAllowed(c) : FvEmailAllowed(c)) ){` |
|     11 | 6689 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     11 | 6690 | `			runStart = i+1;` |
|      5 | 6691 | `		}` |
|     24 | 6692 | `	}` |
|      5 | 6693 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      5 | 6694 | `}` |
|      - | 6695 | `/*` |
|      - | 6696 | ` * Apply the selected filter to one already-resolved input value and write the` |
|      - | 6697 | ` * result into pCtx. Shared by filter_var() and filter_input(): the caller has` |
|      - | 6698 | ` * already parsed $filter/$flags/$options. On validation failure the 'default'` |
|      - | 6699 | ` * option (if any) is returned, else null when FILTER_NULL_ON_FAILURE is set,` |
|      - | 6700 | ` * else false. A validating filter that passes returns the (string) input` |
|      - | 6701 | ` * unchanged; a sanitizer writes its transformed output directly.` |
|      - | 6702 | ` */` |
|    316 | 6703 | `static int FvApplyFilter(ph7_context *pCtx,ph7_value *pInput,` |
|      - | 6704 | `                         int iFilter,int iFlags,ph7_value *pOpts,` |
|      - | 6705 | `                         ph7_value *pDefault)` |
|      3 | 6706 | `{` |
|    319 | 6707 | `	int bNull = (iFlags & FV_NULL_ON_FAILURE) ? 1 : 0;` |
|      - | 6708 | `	const char *zVal; int nVal;` |
|      - | 6709 | `	/* An array/object input fails every scalar filter. */` |
|    319 | 6710 | `	if( ph7_value_is_array(pInput) ){ goto fail; }` |
|    317 | 6711 | `	zVal = ph7_value_to_string(pInput,&nVal);` |
|    317 | 6712 | `	switch( iFilter ){` |
|     28 | 6713 | `	case FV_VALIDATE_INT: {` |
|      - | 6714 | `		ph7_int64 v;` |
|     58 | 6715 | `		if( !FvValidateInt(zVal,nVal,iFlags,&v) ){ goto fail; }` |
|     31 | 6716 | `		if( pOpts ){` |
|      7 | 6717 | `			ph7_value *pMin = ph7_array_fetch(pOpts,"min_range",(int)sizeof("min_range")-1);` |
|      7 | 6718 | `			ph7_value *pMax = ph7_array_fetch(pOpts,"max_range",(int)sizeof("max_range")-1);` |
|      7 | 6719 | `			if( pMin && v<ph7_value_to_int64(pMin) ){ goto fail; }` |
|      7 | 6720 | `			if( pMax && v>ph7_value_to_int64(pMax) ){ goto fail; }` |
|      2 | 6721 | `		}` |
|     29 | 6722 | `		ph7_result_int64(pCtx,v);` |
|     29 | 6723 | `		return PH7_OK;` |
|      - | 6724 | `	}` |
|     34 | 6725 | `	case FV_VALIDATE_FLOAT: {` |
|      - | 6726 | `		double d;` |
|     69 | 6727 | `		if( !FvValidateFloat(zVal,nVal,iFlags,&d) ){ goto fail; }` |
|     39 | 6728 | `		ph7_result_double(pCtx,d);` |
|     39 | 6729 | `		return PH7_OK;` |
|      - | 6730 | `	}` |
|     14 | 6731 | `	case FV_VALIDATE_BOOLEAN: {` |
|      - | 6732 | `		int b;` |
|     29 | 6733 | `		if( !FvValidateBool(zVal,nVal,&b) ){ goto fail; }` |
|     21 | 6734 | `		ph7_result_bool(pCtx,b);` |
|     21 | 6735 | `		return PH7_OK;` |
|      - | 6736 | `	}` |
|     25 | 6737 | `	case FV_VALIDATE_IP:     if( !FvValidateIp(zVal,nVal,iFlags) ){ goto fail; } goto pass;` |
|     11 | 6738 | `	case FV_VALIDATE_MAC:    if( !FvValidateMac(zVal,nVal) ){ goto fail; }       goto pass;` |
|     28 | 6739 | `	case FV_VALIDATE_EMAIL:  if( !FvValidateEmail(zVal,nVal) ){ goto fail; }     goto pass;` |
|     11 | 6740 | `	case FV_VALIDATE_DOMAIN: if( !FvValidateDomain(zVal,nVal) ){ goto fail; }    goto pass;` |
|     15 | 6741 | `	case FV_VALIDATE_URL:    if( !FvValidateUrl(zVal,nVal) ){ goto fail; }       goto pass;` |
|      3 | 6742 | `	case FV_VALIDATE_REGEXP: {` |
|      - | 6743 | `#ifdef PH7_ENABLE_PCRE` |
|      8 | 6744 | `		ph7_value *pRe = pOpts ? ph7_array_fetch(pOpts,"regexp",(int)sizeof("regexp")-1) : 0;` |
|      8 | 6745 | `		const char *zRe; int nRe, matched = 0;` |
|      8 | 6746 | `		if( pRe==0 ){` |
|      3 | 6747 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6748 | `				"filter_var(): \"regexp\" option is missing");` |
|      - | 6749 | `		}` |
|      5 | 6750 | `		zRe = ph7_value_to_string(pRe,&nRe);` |
|      5 | 6751 | `		if( PH7_PcreMatchQuiet(pCtx,zRe,nRe,zVal,nVal,&matched)!=SXRET_OK \|\| !matched ){ goto fail; }` |
|      3 | 6752 | `		goto pass;` |
|      - | 6753 | `#else` |
|      - | 6754 | `		goto fail;` |
|      - | 6755 | `#endif` |
|      - | 6756 | `	}` |
|      3 | 6757 | `	case FV_SANITIZE_NUMBER_INT:   FvSanitizeNumber(pCtx,zVal,nVal,0,0);      return PH7_OK;` |
|      5 | 6758 | `	case FV_SANITIZE_NUMBER_FLOAT: FvSanitizeNumber(pCtx,zVal,nVal,1,iFlags); return PH7_OK;` |
|     13 | 6759 | `	case FV_SANITIZE_SPECIAL_CHARS:      FvSanitizeSpecial(pCtx,zVal,nVal,iFlags); return PH7_OK;` |
|     25 | 6760 | `	case FV_SANITIZE_FULL_SPECIAL_CHARS: FvSanitizeFull(pCtx,zVal,nVal,iFlags);    return PH7_OK;` |
|      3 | 6761 | `	case FV_SANITIZE_EMAIL: FvSanitizeChars(pCtx,zVal,nVal,0); return PH7_OK;` |
|      3 | 6762 | `	case FV_SANITIZE_URL:   FvSanitizeChars(pCtx,zVal,nVal,1); return PH7_OK;` |
|     13 | 6763 | `	case FV_DEFAULT:` |
|      - | 6764 | `		/* FILTER_UNSAFE_RAW / FILTER_DEFAULT: pass through unchanged unless a` |
|      - | 6765 | `		 * STRIP/ENCODE flag is set, in which case apply the string filter. */` |
|     28 | 6766 | `		if( iFlags & FV_FLAG_STRING_MASK ){` |
|     15 | 6767 | `			FvSanitizeString(pCtx,zVal,nVal,iFlags);` |
|     15 | 6768 | `			return PH7_OK;` |
|      - | 6769 | `		}` |
|     14 | 6770 | `		goto pass;` |
|    ! 0 | 6771 | `	default:` |
|    ! 0 | 6772 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 6773 | `			"Unknown filter with ID %d",iFilter);` |
|    ! 0 | 6774 | `		break; /* unknown filter id -> fail */` |
|    ! 0 | 6775 | `	}` |
|     58 | 6776 | `fail:` |
|    118 | 6777 | `	if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|    114 | 6778 | `	else if( bNull ){ ph7_result_null(pCtx); }` |
|    108 | 6779 | `	else { ph7_result_bool(pCtx,0); }` |
|    118 | 6780 | `	return PH7_OK;` |
|     26 | 6781 | `pass: /* validation passed: return the (string) input unchanged */` |
|     54 | 6782 | `	ph7_result_string(pCtx,zVal,nVal);` |
|     54 | 6783 | `	return PH7_OK;` |
|    161 | 6784 | `}` |
|      - | 6785 | `/*` |
|      - | 6786 | ` * Parse the ($filter, $options) pair shared by filter_var()/filter_input() out` |
|      - | 6787 | ` * of apArg[iBase] ($filter) and apArg[iBase+1] ($options): $options is either a` |
|      - | 6788 | ` * plain flags int, or an array with 'flags' and an 'options' sub-array (whose` |
|      - | 6789 | ` * 'default' entry is the fallback value). Fills the four output pointers;` |
|      - | 6790 | ` * unset outputs keep the caller-provided defaults.` |
|      - | 6791 | ` */` |
|    328 | 6792 | `static void FvParseFilterArgs(int nArg,ph7_value **apArg,int iBase,` |
|      - | 6793 | `                              int *piFilter,int *piFlags,` |
|      - | 6794 | `                              ph7_value **ppOpts,ph7_value **ppDefault)` |
|      3 | 6795 | `{` |
|    331 | 6796 | `	if( nArg>iBase ){ *piFilter = ph7_value_to_int(apArg[iBase]); }` |
|    331 | 6797 | `	if( nArg>iBase+1 ){` |
|     88 | 6798 | `		if( ph7_value_is_array(apArg[iBase+1]) ){` |
|     42 | 6799 | `			ph7_value *pF = ph7_array_fetch(apArg[iBase+1],"flags",(int)sizeof("flags")-1);` |
|     42 | 6800 | `			if( pF ){ *piFlags = ph7_value_to_int(pF); }` |
|     42 | 6801 | `			*ppOpts = ph7_array_fetch(apArg[iBase+1],"options",(int)sizeof("options")-1);` |
|     42 | 6802 | `			if( *ppOpts && !ph7_value_is_array(*ppOpts) ){ *ppOpts = 0; }` |
|     42 | 6803 | `			if( *ppOpts ){ *ppDefault = ph7_array_fetch(*ppOpts,"default",(int)sizeof("default")-1); }` |
|     22 | 6804 | `		}else{` |
|     48 | 6805 | `			*piFlags = ph7_value_to_int(apArg[iBase+1]);` |
|      - | 6806 | `		}` |
|     43 | 6807 | `	}` |
|    331 | 6808 | `}` |
|      - | 6809 | `/*` |
|      - | 6810 | ` * filter_var($value, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 6811 | ` *  Validate or sanitize a value; see FvApplyFilter for the failure semantics.` |
|      - | 6812 | ` */` |
|    306 | 6813 | `static int PH7_builtin_filter_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6814 | `{` |
|    308 | 6815 | `	int iFilter = FV_DEFAULT, iFlags = 0;` |
|    308 | 6816 | `	ph7_value *pOpts = 0, *pDefault = 0;` |
|    308 | 6817 | `	if( nArg<1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    308 | 6818 | `	FvParseFilterArgs(nArg,apArg,1,&iFilter,&iFlags,&pOpts,&pDefault);` |
|    308 | 6819 | `	return FvApplyFilter(pCtx,apArg[0],iFilter,iFlags,pOpts,pDefault);` |
|    155 | 6820 | `}` |
|      - | 6821 | `/*` |
|      - | 6822 | ` * filter_input($type, $var_name, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 6823 | ` *  Look up $var_name in the requested INPUT_* superglobal, then apply the` |
|      - | 6824 | ` *  filter. Semantics verified byte-for-byte against php 8.5:` |
|      - | 6825 | ` *   - variable NOT set: 'default' option wins, else false when` |
|      - | 6826 | ` *     FILTER_NULL_ON_FAILURE is set, else null. (Note the null/false roles are` |
|      - | 6827 | ` *     INVERTED relative to a present value that fails validation, which yields` |
|      - | 6828 | ` *     default > null-if-NULL_ON_FAILURE > false via FvApplyFilter.)` |
|      - | 6829 | ` *   - variable present: delegate to FvApplyFilter.` |
|      - | 6830 | ` *  Divergence: php reads a SAPI snapshot of the original request variables` |
|      - | 6831 | ` *  captured at startup; PHL reads the live superglobal. In CLI they match for` |
|      - | 6832 | ` *  the SAPI-registered keys (SCRIPT_NAME/PHP_SELF/DOCUMENT_ROOT); keys added` |
|      - | 6833 | ` *  only to the live $_SERVER (REQUEST_TIME/PWD/…) are visible here but not in` |
|      - | 6834 | ` *  php's snapshot.` |
|      - | 6835 | ` */` |
|     24 | 6836 | `static int PH7_builtin_filter_input(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6837 | `{` |
|     26 | 6838 | `	int iType, iFilter = FV_DEFAULT, iFlags = 0;` |
|     26 | 6839 | `	ph7_value *pOpts = 0, *pDefault = 0, *pSuper, *pElem;` |
|      - | 6840 | `	const char *zVar, *zSuper; int nVar; sxu32 nSuper;` |
|     26 | 6841 | `	if( nArg<2 ){` |
|    ! 0 | 6842 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 6843 | `			"filter_input() expects at least 2 arguments, %d given",nArg);` |
|      - | 6844 | `	}` |
|     26 | 6845 | `	iType = ph7_value_to_int(apArg[0]);` |
|     26 | 6846 | `	switch( iType ){` |
|      3 | 6847 | `	case 0: zSuper = "_POST";   nSuper = (sxu32)sizeof("_POST")-1;   break; /* INPUT_POST */` |
|      3 | 6848 | `	case 1: zSuper = "_GET";    nSuper = (sxu32)sizeof("_GET")-1;    break; /* INPUT_GET */` |
|    ! 0 | 6849 | `	case 2: zSuper = "_COOKIE"; nSuper = (sxu32)sizeof("_COOKIE")-1; break; /* INPUT_COOKIE */` |
|    ! 0 | 6850 | `	case 4: zSuper = "_ENV";    nSuper = (sxu32)sizeof("_ENV")-1;    break; /* INPUT_ENV */` |
|     19 | 6851 | `	case 5: zSuper = "_SERVER"; nSuper = (sxu32)sizeof("_SERVER")-1; break; /* INPUT_SERVER */` |
|      1 | 6852 | `	default:` |
|      3 | 6853 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6854 | `			"filter_input(): Argument #1 ($type) must be an INPUT_* constant");` |
|      - | 6855 | `	}` |
|     23 | 6856 | `	zVar = ph7_value_to_string(apArg[1],&nVar);` |
|     23 | 6857 | `	FvParseFilterArgs(nArg,apArg,2,&iFilter,&iFlags,&pOpts,&pDefault);` |
|      - | 6858 | `	/* Resolve the variable from the superglobal (missing/non-array -> not set). */` |
|     23 | 6859 | `	pSuper = PH7_VmExtractSuper(pCtx->pVm,zSuper,nSuper);` |
|     23 | 6860 | `	pElem = (pSuper && ph7_value_is_array(pSuper))` |
|     33 | 6861 | `		? ph7_array_fetch(pSuper,zVar,nVar) : 0;` |
|     23 | 6862 | `	if( pElem==0 ){` |
|      - | 6863 | `		/* Variable not set: default > false(if NULL_ON_FAILURE) > null. Note the` |
|      - | 6864 | `		 * false/null roles are inverted vs FvApplyFilter's present-but-fails path. */` |
|     13 | 6865 | `		if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|      9 | 6866 | `		else if( iFlags & FV_NULL_ON_FAILURE ){ ph7_result_bool(pCtx,0); }` |
|      7 | 6867 | `		else { ph7_result_null(pCtx); }` |
|     13 | 6868 | `		return PH7_OK;` |
|      - | 6869 | `	}` |
|     11 | 6870 | `	return FvApplyFilter(pCtx,pElem,iFilter,iFlags,pOpts,pDefault);` |
|     14 | 6871 | `}` |
|      - | 6872 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6873 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6874 | `/*` |
|      - | 6875 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 6876 |  |
|      - | 6877 | ` */` |
|      4 | 6878 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 6879 | `	const char *zInput, /* Raw input */` |
|      - | 6880 | `	int nByte,  /* Input length */` |
|      - | 6881 | `	int delim,  /* Delimiter */` |
|      - | 6882 | `	int encl,   /* Enclosure */` |
|      - | 6883 | `	int escape,  /* Escape character */` |
|      - | 6884 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 6885 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 6886 | `	)` |
|      1 | 6887 | `{` |
|      5 | 6888 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 6889 | `	const char *zIn = zInput;` |
|      - | 6890 | `	const char *zPtr;` |
|      - | 6891 | `	int isEnc;` |
|      - | 6892 | `	/* Start processing */` |
|      8 | 6893 | `	for(;;){` |
|     17 | 6894 | `		if( zIn >= zEnd ){` |
|      - | 6895 | `			/* No more input to process */` |
|      5 | 6896 | `			break;` |
|      - | 6897 | `		}` |
|     13 | 6898 | `		isEnc = 0;` |
|     13 | 6899 | `		zPtr = zIn;` |
|      - | 6900 | `		/* Find the first delimiter */` |
|     27 | 6901 | `		while( zIn < zEnd ){` |
|     23 | 6902 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 6903 | `				/* Delimiter found,break imediately */` |
|      5 | 6904 | `				break;` |
|     15 | 6905 | `			}else if( zIn[0] == encl ){` |
|      - | 6906 | `				/* Inside enclosure? */` |
|    ! 0 | 6907 | `				isEnc = !isEnc;` |
|     15 | 6908 | `			}else if( zIn[0] == escape ){` |
|      - | 6909 | `				/* Escape sequence */` |
|    ! 0 | 6910 | `				zIn++;` |
|    ! 0 | 6911 | `			}` |
|      - | 6912 | `			/* Advance the cursor */` |
|     15 | 6913 | `			zIn++;` |
|      1 | 6914 | `		}` |
|     13 | 6915 | `		if( zIn > zPtr ){` |
|     13 | 6916 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 6917 | `			sxi32 rc;` |
|      - | 6918 | `			/* Invoke the supllied callback */` |
|     13 | 6919 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 6920 | `				zPtr++;` |
|    ! 0 | 6921 | `				nByteChunk-=2;` |
|    ! 0 | 6922 | `			}` |
|     13 | 6923 | `			if( nByteChunk > 0 ){` |
|     13 | 6924 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 6925 | `				if( rc == SXERR_ABORT ){` |
|      - | 6926 | `					/* User callback request an operation abort */` |
|    ! 0 | 6927 | `					break;` |
|      - | 6928 | `				}` |
|      6 | 6929 | `			}` |
|      6 | 6930 | `		}` |
|      - | 6931 | `		/* Ignore trailing delimiter */` |
|     21 | 6932 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 6933 | `			zIn++;` |
|      1 | 6934 | `		}` |
|      1 | 6935 | `	}` |
|      5 | 6936 | `	return SXRET_OK;` |
|      1 | 6937 | `}` |
|      - | 6938 | `/*` |
|      - | 6939 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 6940 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 6941 | ` * argument to this callback.` |
|      - | 6942 | ` */` |
|     12 | 6943 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 6944 | `{` |
|     13 | 6945 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 6946 | `	ph7_value sEntry;` |
|      - | 6947 | `	SyString sToken;` |
|      - | 6948 | `	/* Insert the token in the given array */` |
|     13 | 6949 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 6950 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 6951 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 6952 | `	if( sToken.nByte < 1){` |
|    ! 0 | 6953 | `		return SXRET_OK;` |
|      - | 6954 | `	}` |
|     13 | 6955 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 6956 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 6957 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 6958 | `	return SXRET_OK;` |
|      7 | 6959 | `}` |
|      - | 6960 | `/*` |
|      - | 6961 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 6962 | ` *  Parse a CSV string into an array.` |
|      - | 6963 | ` * Parameters` |
|      - | 6964 | ` *  $input` |
|      - | 6965 | ` *   The string to parse.` |
|      - | 6966 | ` *  $delimiter` |
|      - | 6967 | ` *   Set the field delimiter (one character only).` |
|      - | 6968 | ` *  $enclosure` |
|      - | 6969 | ` *   Set the field enclosure character (one character only).` |
|      - | 6970 | ` *  $escape` |
|      - | 6971 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 6972 | ` * Return` |
|      - | 6973 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 6974 | ` */` |
|      2 | 6975 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6976 | `{` |
|      - | 6977 | `	const char *zInput,*zPtr;` |
|      - | 6978 | `	ph7_value *pArray;` |
|      3 | 6979 | `	int delim  = ',';   /* Delimiter */` |
|      3 | 6980 | `	int encl   = '"' ;  /* Enclosure */` |
|      3 | 6981 | `	int escape = '\\';  /* Escape character */` |
|      - | 6982 | `	int nLen;` |
|      3 | 6983 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6984 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 6985 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6986 | `		return PH7_OK;` |
|      - | 6987 | `	}` |
|      - | 6988 | `	/* Extract the raw input */` |
|      3 | 6989 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 6990 | `	if( nArg > 1 ){` |
|      - | 6991 | `		int i;` |
|      3 | 6992 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 6993 | `			/* Extract the delimiter */` |
|      3 | 6994 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 6995 | `			if( i > 0 ){` |
|      3 | 6996 | `				delim = zPtr[0];` |
|      1 | 6997 | `			}` |
|      1 | 6998 | `		}` |
|      3 | 6999 | `		if( nArg > 2 ){` |
|      3 | 7000 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 7001 | `				/* Extract the enclosure */` |
|      3 | 7002 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 7003 | `				if( i > 0 ){` |
|      3 | 7004 | `					encl = zPtr[0];` |
|      1 | 7005 | `				}` |
|      1 | 7006 | `			}` |
|      3 | 7007 | `			if( nArg > 3 ){` |
|      3 | 7008 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 7009 | `					/* Extract the escape character */` |
|      3 | 7010 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 7011 | `					if( i > 0 ){` |
|      3 | 7012 | `						escape = zPtr[0];` |
|      1 | 7013 | `					}` |
|      1 | 7014 | `				}` |
|      1 | 7015 | `			}` |
|      1 | 7016 | `		}` |
|      1 | 7017 | `	}` |
|      - | 7018 | `	/* Create our array */` |
|      3 | 7019 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 7020 | `	if( pArray == 0 ){` |
|      - | 7021 | `		/* Surface a fatal instead of silently returning null on OOM */` |
|    ! 0 | 7022 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7023 | `	}` |
|      - | 7024 | `	/* Parse the raw input */` |
|      3 | 7025 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 7026 | `	/* Return the freshly created array */` |
|      3 | 7027 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 7028 | `	return PH7_OK;` |
|      2 | 7029 | `}` |
|      - | 7030 | `/*` |
|      - | 7031 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 7032 | ` * container.` |
|      - | 7033 | ` * Refer to [strip_tags()].` |
|      - | 7034 | ` */` |
|     10 | 7035 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 7036 | `{` |
|     11 | 7037 | `	const char *zEnd = &zTag[nByte];` |
|      - | 7038 | `	const char *zPtr;` |
|      - | 7039 | `	SyString sEntry;` |
|      - | 7040 | `	/* Strip tags */` |
|     10 | 7041 | `	for(;;){` |
|     45 | 7042 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 7043 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 7044 | `				zTag++;` |
|      1 | 7045 | `		}` |
|     21 | 7046 | `		if( zTag >= zEnd ){` |
|     11 | 7047 | `			break;` |
|      - | 7048 | `		}` |
|     11 | 7049 | `		zPtr = zTag;` |
|      - | 7050 | `		/* Delimit the tag */` |
|     25 | 7051 | `		while(zTag < zEnd ){` |
|     25 | 7052 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 7053 | `				/* UTF-8 stream */` |
|      3 | 7054 | `				zTag++;` |
|      5 | 7055 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 7056 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 7057 | `				break;` |
|    ! 0 | 7058 | `			}else{` |
|     13 | 7059 | `				zTag++;` |
|      - | 7060 | `			}` |
|      1 | 7061 | `		}` |
|     11 | 7062 | `		if( zTag > zPtr ){` |
|      - | 7063 | `			/* Perform the insertion */` |
|     11 | 7064 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 7065 | `			SyStringFullTrim(&sEntry);` |
|     11 | 7066 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 7067 | `		}` |
|      - | 7068 | `		/* Jump the trailing '>' */` |
|     11 | 7069 | `		zTag++;` |
|      1 | 7070 | `	}` |
|     11 | 7071 | `	return SXRET_OK;` |
|      1 | 7072 | `}` |
|      - | 7073 | `/*` |
|      - | 7074 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 7075 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 7076 | ` * Refer to [strip_tags()].` |
|      - | 7077 | ` */` |
|     36 | 7078 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 7079 | `{` |
|     37 | 7080 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 7081 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 7082 | `		SyString sTag;` |
|     85 | 7083 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 7084 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 7085 | `			zTag++;` |
|      1 | 7086 | `		}` |
|      - | 7087 | `		/* Delimit the tag */` |
|     25 | 7088 | `		zCur = zTag;` |
|     77 | 7089 | `		while(zTag < zEnd ){` |
|     77 | 7090 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 7091 | `				/* UTF-8 stream */` |
|      5 | 7092 | `				zTag++;` |
|      9 | 7093 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 7094 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 7095 | `				break;` |
|    ! 0 | 7096 | `			}else{` |
|     49 | 7097 | `				zTag++;` |
|      - | 7098 | `			}` |
|      1 | 7099 | `		}` |
|     25 | 7100 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 7101 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 7102 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 7103 | `		if( sTag.nByte > 0 ){` |
|      - | 7104 | `			SyString *aEntry,*pEntry;` |
|      - | 7105 | `			sxi32 rc;` |
|      - | 7106 | `			sxu32 n;` |
|      - | 7107 | `			/* Perform the lookup */` |
|     25 | 7108 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 7109 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 7110 | `				pEntry = &aEntry[n];` |
|      - | 7111 | `				/* Do the comparison */` |
|     25 | 7112 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 7113 | `				if( !rc ){` |
|     21 | 7114 | `					return SXRET_OK;` |
|      - | 7115 | `				}` |
|      3 | 7116 | `			}` |
|      2 | 7117 | `		}` |
|      2 | 7118 | `	}` |
|      - | 7119 | `	/* No such tag */` |
|     17 | 7120 | `	return SXERR_NOTFOUND;` |
|     19 | 7121 | `}` |
|      - | 7122 | `/*` |
|      - | 7123 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 7124 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 7125 | ` * Refer to [strip_tags()].` |
|      - | 7126 | ` */` |
|     16 | 7127 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 7128 | `{` |
|     17 | 7129 | `	const char *zEnd = &zIn[nByte];` |
|      - | 7130 | `	const char *zPtr,*zTag;` |
|      - | 7131 | `	SySet sSet;` |
|      - | 7132 | `	/* initialize the set of allowed tags */` |
|     17 | 7133 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 7134 | `	if( nTaglen > 0 ){` |
|      - | 7135 | `		/* Set of allowed tags */` |
|     11 | 7136 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 7137 | `	}` |
|      - | 7138 | `	/* Set the empty string */` |
|     17 | 7139 | `	ph7_result_string(pCtx,"",0);` |
|      - | 7140 | `	/* Start processing */` |
|     26 | 7141 | `	for(;;){` |
|     53 | 7142 | `		if(zIn >= zEnd){` |
|      - | 7143 | `			/* No more input to process */` |
|     15 | 7144 | `			break;` |
|      - | 7145 | `		}` |
|     39 | 7146 | `		zPtr = zIn;` |
|      - | 7147 | `		/* Find a tag */` |
|    133 | 7148 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 7149 | `			zIn++;` |
|      1 | 7150 | `		}` |
|     39 | 7151 | `		if( zIn > zPtr ){` |
|      - | 7152 | `			/* Consume raw input */` |
|     21 | 7153 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 7154 | `		}` |
|      - | 7155 | `		/* Ignore trailing null bytes */` |
|     39 | 7156 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 7157 | `			zIn++;` |
|    ! 0 | 7158 | `		}` |
|     39 | 7159 | `		if(zIn >= zEnd){` |
|      - | 7160 | `			/* No more input to process */` |
|      3 | 7161 | `			break;` |
|      - | 7162 | `		}` |
|     37 | 7163 | `		if( zIn[0] == '<' ){` |
|      - | 7164 | `			sxi32 rc;` |
|     37 | 7165 | `			zTag = zIn++;` |
|      - | 7166 | `			/* Delimit the tag */` |
|    127 | 7167 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 7168 | `				zIn++;` |
|      1 | 7169 | `			}` |
|     37 | 7170 | `			if( zIn < zEnd ){` |
|     37 | 7171 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 7172 | `			}` |
|      - | 7173 | `			/* Query the set */` |
|     37 | 7174 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 7175 | `			if( rc == SXRET_OK ){` |
|      - | 7176 | `				/* Keep the tag */` |
|     21 | 7177 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 7178 | `			}` |
|     18 | 7179 | `		}` |
|      1 | 7180 | `	}` |
|      - | 7181 | `	/* Cleanup */` |
|     17 | 7182 | `	SySetRelease(&sSet);` |
|     17 | 7183 | `	return SXRET_OK;` |
|      1 | 7184 | `}` |
|      - | 7185 | `/*` |
|      - | 7186 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 7187 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 7188 | ` * Parameters` |
|      - | 7189 | ` *  $str` |
|      - | 7190 | ` *  The input string.` |
|      - | 7191 | ` * $allowable_tags` |
|      - | 7192 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 7193 | ` * Return` |
|      - | 7194 | ` *  Returns the stripped string.` |
|      - | 7195 | ` */` |
|     14 | 7196 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7197 | `{` |
|     15 | 7198 | `	const char *zTaglist = 0;` |
|      - | 7199 | `	const char *zString;` |
|     15 | 7200 | `	int nTaglen = 0;` |
|      - | 7201 | `	int nLen;` |
|     15 | 7202 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7203 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 7204 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7205 | `		return PH7_OK;` |
|      - | 7206 | `	}` |
|      - | 7207 | `	/* Point to the raw string */` |
|     15 | 7208 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 7209 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 7210 | `		/* Allowed tag */` |
|     11 | 7211 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 7212 | `	}` |
|      - | 7213 | `	/* Process input */` |
|     15 | 7214 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 7215 | `	return PH7_OK;` |
|      8 | 7216 | `}` |
|      - | 7217 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7218 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7219 | `/*` |
|      - | 7220 | ` * string str_shuffle(string $str)` |
|      - | 7221 |  |
|      - | 7222 | ` *  Randomly shuffles a string.` |
|      - | 7223 | ` * Parameters` |
|      - | 7224 | ` *  $str` |
|      - | 7225 | ` *   The input string.` |
|      - | 7226 | ` * Return` |
|      - | 7227 | ` *  Returns the shuffled string.` |
|      - | 7228 | ` */` |
|     10 | 7229 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7230 | `{` |
|      - | 7231 | `	const char *zString;` |
|      - | 7232 | `	int nLen,i,c;` |
|      - | 7233 | `	sxu32 iR;` |
|     11 | 7234 | `	if( nArg < 1 ){` |
|      - | 7235 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7236 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7237 | `		return PH7_OK;` |
|      - | 7238 | `	}` |
|      - | 7239 | `	/* Extract the target string */` |
|     11 | 7240 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 7241 | `	if( nLen < 1 ){` |
|      - | 7242 | `		/* Nothing to shuffle */` |
|      3 | 7243 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 7244 | `		return PH7_OK;` |
|      - | 7245 | `	}` |
|      - | 7246 | `	/* Shuffle the string */` |
|     43 | 7247 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 7248 | `		/* Generate a random number first */` |
|     35 | 7249 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 7250 | `		/* Extract a random offset */` |
|     35 | 7251 | `		c = zString[iR % nLen];` |
|      - | 7252 | `		/* Append it */` |
|     35 | 7253 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 7254 | `	}` |
|      9 | 7255 | `	return PH7_OK;` |
|      6 | 7256 | `}` |
|      - | 7257 | `/*` |
|      - | 7258 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 7259 | ` *  Convert a string to an array.` |
|      - | 7260 | ` * Parameters` |
|      - | 7261 | ` * $string` |
|      - | 7262 | ` *  The input string.` |
|      - | 7263 | ` * $split_length` |
|      - | 7264 | ` *  Maximum length of the chunk.` |
|      - | 7265 | ` * Return` |
|      - | 7266 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 7267 | ` *  except possibly the last one which may be shorter.` |
|      - | 7268 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 7269 | ` *  as the first (and only) array element.` |
|      - | 7270 | ` *  An empty string returns an empty array.` |
|      - | 7271 | ` * Errors` |
|      - | 7272 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 7273 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 7274 | ` *  ValueError if $split_length is less than 1.` |
|      - | 7275 | ` */` |
|     26 | 7276 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 7277 | `{` |
|      - | 7278 | `	const char *zString,*zEnd;` |
|      - | 7279 | `	ph7_value *pArray,*pValue;` |
|      - | 7280 | `	int split_len;` |
|      - | 7281 | `	int nLen;` |
|     29 | 7282 | `	if( nArg < 1 ){` |
|    ! 0 | 7283 | `		return PH7_VmThrowException(pCtx,` |
|      - | 7284 | `			"ArgumentCountError",` |
|      - | 7285 | `			"str_split() expects at least 1 argument, %d given",` |
|    ! 0 | 7286 | `			nArg` |
|      - | 7287 | `			);` |
|      - | 7288 | `	}` |
|      - | 7289 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     39 | 7290 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     42 | 7291 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     26 | 7292 | `	    ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 7293 | `		return PH7_VmThrowException(pCtx,` |
|      - | 7294 | `			"TypeError",` |
|      - | 7295 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 7296 | `			ph7_type_name(apArg[0])` |
|      - | 7297 | `			);` |
|      - | 7298 | `	}` |
|      - | 7299 | `	/* Point to the target string */` |
|     29 | 7300 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     29 | 7301 | `	split_len = (int)sizeof(char);` |
|     29 | 7302 | `	if( nArg > 1 ){` |
|      - | 7303 | `		/* Split length */` |
|     17 | 7304 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     17 | 7305 | `		if( split_len < 1 ){` |
|      6 | 7306 | `			return PH7_VmThrowException(pCtx,` |
|      - | 7307 | `				"ValueError",` |
|      - | 7308 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 7309 | `				);` |
|      - | 7310 | `		}` |
|     11 | 7311 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 7312 | `			split_len = nLen;` |
|      1 | 7313 | `		}` |
|      5 | 7314 | `	}` |
|      - | 7315 | `	/* Create the array and the scalar value */` |
|     23 | 7316 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 7317 | `	/*Chunk value */` |
|     23 | 7318 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     23 | 7319 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 7320 | `		/* Return FALSE */` |
|    ! 0 | 7321 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7322 | `		return PH7_OK;` |
|      - | 7323 | `	}` |
|      - | 7324 | `	/* Point to the end of the string */` |
|     23 | 7325 | `	zEnd = &zString[nLen];` |
|      - | 7326 | `	/* Perform the requested operation */` |
|    131 | 7327 | `	for(;;){` |
|      - | 7328 | `		int nMax;` |
|    143 | 7329 | `		if( zString >= zEnd ){` |
|      - | 7330 | `			/* No more input to process */` |
|     23 | 7331 | `			break;` |
|      - | 7332 | `		}` |
|    121 | 7333 | `		nMax = (int)(zEnd-zString);` |
|    121 | 7334 | `		if( nMax < split_len ){` |
|      3 | 7335 | `			split_len = nMax;` |
|      1 | 7336 | `		}` |
|      - | 7337 | `		/* Copy the current chunk */` |
|    121 | 7338 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 7339 | `		/* Insert it */` |
|    121 | 7340 | `		if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){ /* Will make it's own copy */` |
|    ! 0 | 7341 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 7342 | `		}` |
|      - | 7343 | `		/* reset the string cursor */` |
|    121 | 7344 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 7345 | `		/* Update position */` |
|    121 | 7346 | `		zString += split_len;` |
|      1 | 7347 | `	}` |
|      - | 7348 | `	/*` |
|      - | 7349 | `	 * Return the array.` |
|      - | 7350 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 7351 | `	 * upon we return from this function.` |
|      - | 7352 | `	 */` |
|     23 | 7353 | `	ph7_result_value(pCtx,pArray);` |
|     23 | 7354 | `	return PH7_OK;` |
|     16 | 7355 | `}` |
|      - | 7356 | `/*` |
|      - | 7357 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 7358 | ` * Refer to [strspn()].` |
|      - | 7359 | ` */` |
|     28 | 7360 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 7361 | `{` |
|     29 | 7362 | `	const char *zIn = *pzIn;` |
|      - | 7363 | `	const char *zPtr;` |
|      - | 7364 | `	/* Ignore leading white spaces */` |
|     29 | 7365 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 7366 | `		zIn++;` |
|    ! 0 | 7367 | `	}` |
|     29 | 7368 | `	if( zIn >= zEnd ){` |
|      - | 7369 | `		/* End of input */` |
|    ! 0 | 7370 | `		return SXERR_EOF;` |
|      - | 7371 | `	}` |
|     29 | 7372 | `	zPtr = zIn;` |
|      - | 7373 | `	/* Extract the token */` |
|    201 | 7374 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 7375 | `		zIn++;` |
|      1 | 7376 | `	}` |
|     29 | 7377 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 7378 | `	/* Synchronize pointers */` |
|     29 | 7379 | `	*pzIn = zIn;` |
|      - | 7380 | `	/* Return to the caller */` |
|     29 | 7381 | `	return SXRET_OK;` |
|     15 | 7382 | `}` |
|      - | 7383 | `/*` |
|      - | 7384 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 7385 | ` * return the longest match.` |
|      - | 7386 | ` * Refer to [strspn()].` |
|      - | 7387 | ` */` |
|     18 | 7388 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 7389 | `{` |
|     19 | 7390 | `	const char *zEnd = &zString[nLen];` |
|     19 | 7391 | `	const char *zIn = zString;` |
|      - | 7392 | `	int i,c;` |
|     45 | 7393 | `	for(;;){` |
|     91 | 7394 | `		if( zString >= zEnd ){` |
|      7 | 7395 | `			break;` |
|      - | 7396 | `		}` |
|      - | 7397 | `		/* Extract current character */` |
|     85 | 7398 | `		c = zString[0];` |
|      - | 7399 | `		/* Perform the lookup */` |
|    383 | 7400 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 7401 | `			if( c == zMask[i] ){` |
|      - | 7402 | `				/* Character found */` |
|     73 | 7403 | `				break;` |
|      - | 7404 | `			}` |
|    150 | 7405 | `		}` |
|     85 | 7406 | `		if( i >= nMaskLen ){` |
|      - | 7407 | `			/* Character not in the current mask,break immediately */` |
|     13 | 7408 | `			break;` |
|      - | 7409 | `		}` |
|      - | 7410 | `		/* Advance cursor */` |
|     73 | 7411 | `		zString++;` |
|      1 | 7412 | `	}` |
|      - | 7413 | `	/* Longest match */` |
|     19 | 7414 | `	return (int)(zString-zIn);` |
|      1 | 7415 | `}` |
|      - | 7416 | `/*` |
|      - | 7417 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 7418 | ` * Refer to [strcspn()].` |
|      - | 7419 | ` */` |
|     10 | 7420 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 7421 | `{` |
|     11 | 7422 | `	const char *zEnd = &zString[nLen];` |
|     11 | 7423 | `	const char *zIn = zString;` |
|      - | 7424 | `	int i,c;` |
|     12 | 7425 | `	for(;;){` |
|     25 | 7426 | `		if( zString >= zEnd ){` |
|      3 | 7427 | `			break;` |
|      - | 7428 | `		}` |
|      - | 7429 | `		/* Extract current character */` |
|     23 | 7430 | `		c = zString[0];` |
|      - | 7431 | `		/* Perform the lookup */` |
|     51 | 7432 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 7433 | `			if( c == zMask[i] ){` |
|      9 | 7434 | `				break;` |
|      - | 7435 | `			}` |
|     15 | 7436 | `		}` |
|     23 | 7437 | `		if( i < nMaskLen ){` |
|      - | 7438 | `			/* Character in the current mask,break immediately */` |
|      9 | 7439 | `			break;` |
|      - | 7440 | `		}` |
|      - | 7441 | `		/* Advance cursor */` |
|     15 | 7442 | `		zString++;` |
|      1 | 7443 | `	}` |
|      - | 7444 | `	/* Longest match */` |
|     11 | 7445 | `	return (int)(zString-zIn);` |
|      1 | 7446 | `}` |
|      - | 7447 | `/*` |
|      - | 7448 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 7449 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 7450 | ` *  of characters contained within a given mask.` |
|      - | 7451 | ` * Parameters` |
|      - | 7452 | ` * $str` |
|      - | 7453 | ` *  The input string.` |
|      - | 7454 | ` * $mask` |
|      - | 7455 | ` *  The list of allowable characters.` |
|      - | 7456 | ` * $start` |
|      - | 7457 | ` *  The position in subject to start searching.` |
|      - | 7458 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 7459 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 7460 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 7461 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 7462 | ` *  start'th position from the end of subject.` |
|      - | 7463 | ` * $length` |
|      - | 7464 | ` *  The length of the segment from subject to examine.` |
|      - | 7465 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 7466 | ` *  characters after the starting position.` |
|      - | 7467 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 7468 | ` *  position up to length characters from the end of subject.` |
|      - | 7469 | ` * Return` |
|      - | 7470 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 7471 | ` * in mask.` |
|      - | 7472 | ` */` |
|     24 | 7473 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7474 | `{` |
|      - | 7475 | `	const char *zString,*zMask,*zEnd;` |
|      - | 7476 | `	int iMasklen,iLen;` |
|      - | 7477 | `	SyString sToken;` |
|     25 | 7478 | `	int iCount = 0;` |
|      - | 7479 | `	int rc;` |
|     25 | 7480 | `	if( nArg < 2 ){` |
|      - | 7481 | `		/* Missing agruments,return zero */` |
|    ! 0 | 7482 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7483 | `		return PH7_OK;` |
|      - | 7484 | `	}` |
|      - | 7485 | `	/* Extract the target string */` |
|     25 | 7486 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7487 | `	/* Extract the mask */` |
|     25 | 7488 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 7489 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 7490 | `		/* Nothing to process,return zero */` |
|      7 | 7491 | `		ph7_result_int(pCtx,0);` |
|      7 | 7492 | `		return PH7_OK;` |
|      - | 7493 | `	}` |
|     19 | 7494 | `	if( nArg > 2 ){` |
|      - | 7495 | `		int nOfft;` |
|      - | 7496 | `		/* Extract the offset */` |
|      9 | 7497 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 7498 | `		if( nOfft < 0 ){` |
|    ! 0 | 7499 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 7500 | `			if( zBase > zString ){` |
|    ! 0 | 7501 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 7502 | `				zString = zBase;` |
|    ! 0 | 7503 | `			}else{` |
|      - | 7504 | `				/* Invalid offset */` |
|    ! 0 | 7505 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7506 | `				return PH7_OK;` |
|      - | 7507 | `			}` |
|    ! 0 | 7508 | `		}else{` |
|      9 | 7509 | `			if( nOfft >= iLen ){` |
|      - | 7510 | `				/* Invalid offset */` |
|    ! 0 | 7511 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7512 | `				return PH7_OK;` |
|    ! 0 | 7513 | `			}else{` |
|      - | 7514 | `				/* Update offset */` |
|      9 | 7515 | `				zString += nOfft;` |
|      9 | 7516 | `				iLen -= nOfft;` |
|      - | 7517 | `			}` |
|      - | 7518 | `		}` |
|      9 | 7519 | `		if( nArg > 3 ){` |
|      - | 7520 | `			int iUserlen;` |
|      - | 7521 | `			/* Extract the desired length */` |
|      9 | 7522 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 7523 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 7524 | `				iLen = iUserlen;` |
|      2 | 7525 | `			}` |
|      4 | 7526 | `		}` |
|      4 | 7527 | `	}` |
|      - | 7528 | `	/* Point to the end of the string */` |
|     19 | 7529 | `	zEnd = &zString[iLen];` |
|      - | 7530 | `	/* Extract the first non-space token */` |
|     19 | 7531 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 7532 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 7533 | `		/* Compare against the current mask */` |
|     19 | 7534 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 7535 | `	}` |
|      - | 7536 | `	/* Longest match */` |
|     19 | 7537 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 7538 | `	return PH7_OK;` |
|     13 | 7539 | `}` |
|      - | 7540 | `/*` |
|      - | 7541 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 7542 | ` *  Find length of initial segment not matching mask.` |
|      - | 7543 | ` * Parameters` |
|      - | 7544 | ` * $str` |
|      - | 7545 | ` *  The input string.` |
|      - | 7546 | ` * $mask` |
|      - | 7547 | ` *  The list of not allowed characters.` |
|      - | 7548 | ` * $start` |
|      - | 7549 | ` *  The position in subject to start searching.` |
|      - | 7550 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 7551 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 7552 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 7553 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 7554 | ` *  start'th position from the end of subject.` |
|      - | 7555 | ` * $length` |
|      - | 7556 | ` *  The length of the segment from subject to examine.` |
|      - | 7557 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 7558 | ` *  characters after the starting position.` |
|      - | 7559 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 7560 | ` *  position up to length characters from the end of subject.` |
|      - | 7561 | ` * Return` |
|      - | 7562 | ` *  Returns the length of the segment as an integer.` |
|      - | 7563 | ` */` |
|     14 | 7564 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7565 | `{` |
|      - | 7566 | `	const char *zString,*zMask,*zEnd;` |
|      - | 7567 | `	int iMasklen,iLen;` |
|      - | 7568 | `	SyString sToken;` |
|     15 | 7569 | `	int iCount = 0;` |
|      - | 7570 | `	int rc;` |
|     15 | 7571 | `	if( nArg < 2 ){` |
|      - | 7572 | `		/* Missing agruments,return zero */` |
|    ! 0 | 7573 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7574 | `		return PH7_OK;` |
|      - | 7575 | `	}` |
|      - | 7576 | `	/* Extract the target string */` |
|     15 | 7577 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7578 | `	/* Extract the mask */` |
|     15 | 7579 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 7580 | `	if( iLen < 1 ){` |
|      - | 7581 | `		/* Nothing to process,return zero */` |
|    ! 0 | 7582 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7583 | `		return PH7_OK;` |
|      - | 7584 | `	}` |
|     15 | 7585 | `	if( iMasklen < 1 ){` |
|      - | 7586 | `		/* No given mask,return the string length */` |
|      3 | 7587 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 7588 | `		return PH7_OK;` |
|      - | 7589 | `	}` |
|     13 | 7590 | `	if( nArg > 2 ){` |
|      - | 7591 | `		int nOfft;` |
|      - | 7592 | `		/* Extract the offset */` |
|     11 | 7593 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 7594 | `		if( nOfft < 0 ){` |
|    ! 0 | 7595 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 7596 | `			if( zBase > zString ){` |
|    ! 0 | 7597 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 7598 | `				zString = zBase;` |
|    ! 0 | 7599 | `			}else{` |
|      - | 7600 | `				/* Invalid offset */` |
|    ! 0 | 7601 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7602 | `				return PH7_OK;` |
|      - | 7603 | `			}` |
|    ! 0 | 7604 | `		}else{` |
|     11 | 7605 | `			if( nOfft >= iLen ){` |
|      - | 7606 | `				/* Invalid offset */` |
|      3 | 7607 | `				ph7_result_int(pCtx,0);` |
|      3 | 7608 | `				return PH7_OK;` |
|    ! 0 | 7609 | `			}else{` |
|      - | 7610 | `				/* Update offset */` |
|      9 | 7611 | `				zString += nOfft;` |
|      9 | 7612 | `				iLen -= nOfft;` |
|      - | 7613 | `			}` |
|      - | 7614 | `		}` |
|      9 | 7615 | `		if( nArg > 3 ){` |
|      - | 7616 | `			int iUserlen;` |
|      - | 7617 | `			/* Extract the desired length */` |
|    ! 0 | 7618 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 7619 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 7620 | `				iLen = iUserlen;` |
|    ! 0 | 7621 | `			}` |
|    ! 0 | 7622 | `		}` |
|      4 | 7623 | `	}` |
|      - | 7624 | `	/* Point to the end of the string */` |
|     11 | 7625 | `	zEnd = &zString[iLen];` |
|      - | 7626 | `	/* Extract the first non-space token */` |
|     11 | 7627 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 7628 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 7629 | `		/* Compare against the current mask */` |
|     11 | 7630 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 7631 | `	}` |
|      - | 7632 | `	/* Longest match */` |
|     11 | 7633 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 7634 | `	return PH7_OK;` |
|      8 | 7635 | `}` |
|      - | 7636 | `/*` |
|      - | 7637 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 7638 | ` *  Search a string for any of a set of characters.` |
|      - | 7639 | ` * Parameters` |
|      - | 7640 | ` *  $haystack` |
|      - | 7641 | ` *   The string where char_list is looked for.` |
|      - | 7642 | ` *  $char_list` |
|      - | 7643 | ` *   This parameter is case sensitive.` |
|      - | 7644 | ` * Return` |
|      - | 7645 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 7646 | ` */` |
|      4 | 7647 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7648 | `{` |
|      - | 7649 | `	const char *zString,*zList,*zEnd;` |
|      - | 7650 | `	int iLen,iListLen,i,c;` |
|      - | 7651 | `	sxu32 nOfft,nMax;` |
|      - | 7652 | `	sxi32 rc;` |
|      5 | 7653 | `	if( nArg < 2 ){` |
|      - | 7654 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7655 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7656 | `		return PH7_OK;` |
|      - | 7657 | `	}` |
|      - | 7658 | `	/* Extract the haystack and the char list */` |
|      5 | 7659 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 7660 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 7661 | `	if( iLen < 1 ){` |
|      - | 7662 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 7663 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7664 | `		return PH7_OK;` |
|      - | 7665 | `	}` |
|      - | 7666 | `	/* Point to the end of the string */` |
|      5 | 7667 | `	zEnd = &zString[iLen];` |
|      5 | 7668 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 7669 | `	/* perform the requested operation */` |
|     15 | 7670 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 7671 | `		c = zList[i];` |
|     11 | 7672 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 7673 | `		if( rc == SXRET_OK ){` |
|      5 | 7674 | `			if( nMax < nOfft ){` |
|      3 | 7675 | `				nOfft = nMax;` |
|      1 | 7676 | `			}` |
|      2 | 7677 | `		}` |
|      6 | 7678 | `	}` |
|      5 | 7679 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 7680 | `		/* No such substring,return FALSE */` |
|      3 | 7681 | `		ph7_result_bool(pCtx,0);` |
|      2 | 7682 | `	}else{` |
|      - | 7683 | `		/* Return the substring */` |
|      3 | 7684 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 7685 | `	}` |
|      5 | 7686 | `	return PH7_OK;` |
|      3 | 7687 | `}` |
|      - | 7688 | `/* SPDX-SnippetBegin */` |
|      - | 7689 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 7690 | `/* SPDX-License-Identifier: blessing */` |
|      - | 7691 | `/*` |
|      - | 7692 | ` * string soundex(string $str)` |
|      - | 7693 | ` *  Calculate the soundex key of a string.` |
|      - | 7694 | ` * Parameters` |
|      - | 7695 | ` *  $str` |
|      - | 7696 | ` *   The input string.` |
|      - | 7697 | ` * Return` |
|      - | 7698 | ` *  Returns the soundex key as a string.` |
|      - | 7699 | ` * Note:` |
|      - | 7700 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 7701 | ` * source tree.` |
|      - | 7702 | ` */` |
|     22 | 7703 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7704 | `{` |
|      - | 7705 | `	const unsigned char *zIn;` |
|      - | 7706 | `	char zResult[8];` |
|      - | 7707 | `	int i, j;` |
|      - | 7708 | `	static const unsigned char iCode[] = {` |
|      - | 7709 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7710 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7711 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7712 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7713 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 7714 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 7715 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 7716 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 7717 | `	};` |
|     23 | 7718 | `	if( nArg < 1 ){` |
|      - | 7719 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7720 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7721 | `		return PH7_OK;` |
|      - | 7722 | `	}` |
|     23 | 7723 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     35 | 7724 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     23 | 7725 | `	if( zIn[i] ){` |
|     17 | 7726 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 7727 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 7728 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 7729 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 7730 | `			if( code>0 ){` |
|     45 | 7731 | `				if( code!=prevcode ){` |
|     33 | 7732 | `					prevcode = (unsigned char)code;` |
|     33 | 7733 | `					zResult[j++] = (char)code + '0';` |
|     16 | 7734 | `				}` |
|     23 | 7735 | `			}else{` |
|     49 | 7736 | `				prevcode = 0;` |
|      - | 7737 | `			}` |
|     47 | 7738 | `		}` |
|     33 | 7739 | `		while( j<4 ){` |
|     17 | 7740 | `			zResult[j++] = '0';` |
|      1 | 7741 | `		}` |
|     17 | 7742 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 7743 | `	}else{` |
|      - | 7744 | `	  /* No alphabetic character: PHP returns "0000" (not the SQLite "?000"). */` |
|      7 | 7745 | `	  ph7_result_string(pCtx,"0000",4);` |
|      - | 7746 | `	}` |
|     23 | 7747 | `	return PH7_OK;` |
|     12 | 7748 | `}` |
|      - | 7749 | `/* SPDX-SnippetEnd */` |
|      - | 7750 | `/*` |
|      - | 7751 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 7752 | ` *  Wraps a string to a given number of characters.` |
|      - | 7753 | ` * Parameters` |
|      - | 7754 | ` *  $str` |
|      - | 7755 | ` *   The input string.` |
|      - | 7756 | ` * $width` |
|      - | 7757 | ` *  The column width.` |
|      - | 7758 | ` * $break` |
|      - | 7759 | ` *  The line is broken using the optional break parameter.` |
|      - | 7760 | ` * Return` |
|      - | 7761 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 7762 | ` */` |
|     26 | 7763 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7764 | `{` |
|      - | 7765 | `	const char *zIn,*zBreak;` |
|      - | 7766 | `	SyBlob sWorker;` |
|      - | 7767 | `	int iLen,iBreaklen,iWidth,iCut,iStart,iSpace,iCur;` |
|      - | 7768 | `	sxi32 rc;` |
|     27 | 7769 | `	if( nArg < 1 ){` |
|      - | 7770 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7771 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7772 | `		return PH7_OK;` |
|      - | 7773 | `	}` |
|      - | 7774 | `	/* Extract the input string */` |
|     27 | 7775 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7776 | `	/* Width (default 75; PHP allows 0/negative — break at every space). */` |
|     27 | 7777 | `	iWidth = 75;` |
|     27 | 7778 | `	if( nArg > 1 ){` |
|     27 | 7779 | `		iWidth = ph7_value_to_int(apArg[1]);` |
|     13 | 7780 | `	}` |
|      - | 7781 | `	/* Break string (default "\n"). */` |
|     27 | 7782 | `	zBreak = "\n";` |
|     27 | 7783 | `	iBreaklen = (int)sizeof(char);` |
|     27 | 7784 | `	if( nArg > 2 ){` |
|     13 | 7785 | `		zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      6 | 7786 | `	}` |
|      - | 7787 | `	/* Cut long words? (default false). */` |
|     27 | 7788 | `	iCut = 0;` |
|     27 | 7789 | `	if( nArg > 3 ){` |
|      7 | 7790 | `		iCut = ph7_value_to_bool(apArg[3]);` |
|      3 | 7791 | `	}` |
|     27 | 7792 | `	if( iLen < 1 ){` |
|      - | 7793 | `		/* PHP returns the empty string for empty input before validating the other args. */` |
|      5 | 7794 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 7795 | `		return PH7_OK;` |
|      - | 7796 | `	}` |
|      - | 7797 | `	/* PHP 8 domain errors (catchable ValueError). */` |
|     23 | 7798 | `	if( iBreaklen < 1 ){` |
|      3 | 7799 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7800 | `			"wordwrap(): Argument #3 ($break) must not be empty");` |
|      - | 7801 | `	}` |
|     21 | 7802 | `	if( iWidth == 0 && iCut ){` |
|      3 | 7803 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7804 | `			"wordwrap(): Argument #4 ($cut_long_words) cannot be true when argument #2 ($width) is 0");` |
|      - | 7805 | `	}` |
|      - | 7806 | `	/*` |
|      - | 7807 | `	 * PHP's algorithm: a single left-to-right pass tracking the start of the` |
|      - | 7808 | `	 * current line (iStart) and the position of the last space seen on it` |
|      - | 7809 | `	 * (iSpace). A break is emitted when the line reaches the width, at the last` |
|      - | 7810 | `	 * space if there was one, otherwise (only when cut is enabled) hard at the` |
|      - | 7811 | `	 * boundary. An existing break sequence in the input resets the line.` |
|      - | 7812 | `	 */` |
|     19 | 7813 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     19 | 7814 | `	iStart = iSpace = iCur = 0;` |
|     19 | 7815 | `	rc = SXRET_OK;` |
|    551 | 7816 | `	while( iCur < iLen ){` |
|    533 | 7817 | `		if( iBreaklen <= iLen - iCur && SyMemcmp(&zIn[iCur],zBreak,(sxu32)iBreaklen) == 0 ){` |
|      - | 7818 | `			/* Existing break sequence in the input: copy it verbatim and reset the line. */` |
|    ! 0 | 7819 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart + iBreaklen));` |
|    ! 0 | 7820 | `			if( rc != SXRET_OK ){ goto oom; }` |
|    ! 0 | 7821 | `			iCur += iBreaklen;` |
|    ! 0 | 7822 | `			iStart = iSpace = iCur;` |
|    ! 0 | 7823 | `			continue;` |
|    533 | 7824 | `		}else if( zIn[iCur] == ' ' ){` |
|     67 | 7825 | `			if( iCur - iStart >= iWidth ){` |
|      - | 7826 | `				/* The line already fills the width at this space: break here (the space is consumed). */` |
|     13 | 7827 | `				rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     13 | 7828 | `				if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     13 | 7829 | `				if( rc != SXRET_OK ){ goto oom; }` |
|     13 | 7830 | `				iStart = iCur + 1;` |
|      6 | 7831 | `			}` |
|     67 | 7832 | `			iSpace = iCur;` |
|    500 | 7833 | `		}else if( iCut && iCur - iStart >= iWidth && iStart >= iSpace ){` |
|      - | 7834 | `			/* A word longer than the width with no space to break at: hard-cut at the boundary. */` |
|      7 | 7835 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|      7 | 7836 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|      7 | 7837 | `			if( rc != SXRET_OK ){ goto oom; }` |
|      7 | 7838 | `			iStart = iSpace = iCur;` |
|    464 | 7839 | `		}else if( iCur - iStart >= iWidth && iStart < iSpace ){` |
|      - | 7840 | `			/* Past the width mid-word: wrap back to the last space (which is consumed). */` |
|     17 | 7841 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iSpace - iStart));` |
|     17 | 7842 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     17 | 7843 | `			if( rc != SXRET_OK ){ goto oom; }` |
|     17 | 7844 | `			iStart = iSpace = iSpace + 1;` |
|      8 | 7845 | `		}` |
|    533 | 7846 | `		iCur++;` |
|      1 | 7847 | `	}` |
|      - | 7848 | `	/* Emit the trailing chunk. */` |
|     19 | 7849 | `	if( iStart < iCur ){` |
|     19 | 7850 | `		rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     19 | 7851 | `		if( rc != SXRET_OK ){ goto oom; }` |
|      9 | 7852 | `	}` |
|     19 | 7853 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|     19 | 7854 | `	SyBlobRelease(&sWorker);` |
|     19 | 7855 | `	return PH7_OK;` |
|    ! 0 | 7856 | `oom:` |
|    ! 0 | 7857 | `	SyBlobRelease(&sWorker);` |
|    ! 0 | 7858 | `	return PH7_ContextMemoryError(pCtx);` |
|     14 | 7859 | `}` |
|      - | 7860 | `/*` |
|      - | 7861 | ` * Check if the given character is a member of the given mask.` |
|      - | 7862 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 7863 | ` * Refer to [strtok()].` |
|      - | 7864 | ` */` |
|     30 | 7865 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 7866 | `{` |
|      - | 7867 | `	int i;` |
|     57 | 7868 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 7869 | `		if( c == zMask[i] ){` |
|     13 | 7870 | `			if( pOfft ){` |
|      5 | 7871 | `				*pOfft = i;` |
|      2 | 7872 | `			}` |
|     13 | 7873 | `			return TRUE;` |
|      - | 7874 | `		}` |
|     14 | 7875 | `	}` |
|     19 | 7876 | `	return FALSE;` |
|     16 | 7877 | `}` |
|      - | 7878 | `/*` |
|      - | 7879 | ` * Extract a single token from the input stream.` |
|      - | 7880 | ` * Refer to [strtok()].` |
|      - | 7881 | ` */` |
|      6 | 7882 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 7883 | `{` |
|      7 | 7884 | `	const char *zIn = *pzIn;` |
|      - | 7885 | `	const char *zPtr;` |
|      - | 7886 | `	/* Ignore leading delimiter */` |
|     11 | 7887 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 7888 | `		zIn++;` |
|      1 | 7889 | `	}` |
|      7 | 7890 | `	if( zIn >= zEnd ){` |
|      - | 7891 | `		/* End of input */` |
|    ! 0 | 7892 | `		return SXERR_EOF;` |
|      - | 7893 | `	}` |
|      7 | 7894 | `	zPtr = zIn;` |
|      - | 7895 | `	/* Extract the token */` |
|     13 | 7896 | `	while( zIn < zEnd ){` |
|     11 | 7897 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 7898 | `			/* UTF-8 stream */` |
|    ! 0 | 7899 | `			zIn++;` |
|    ! 0 | 7900 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 7901 | `		}else{` |
|     11 | 7902 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 7903 | `				break;` |
|      - | 7904 | `			}` |
|      7 | 7905 | `			zIn++;` |
|      - | 7906 | `		}` |
|      1 | 7907 | `	}` |
|      7 | 7908 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 7909 | `	/* Update the cursor */` |
|      7 | 7910 | `	*pzIn = zIn;` |
|      - | 7911 | `	/* Return to the caller */` |
|      7 | 7912 | `	return SXRET_OK;` |
|      4 | 7913 | `}` |
|      - | 7914 | `/* strtok auxiliary private data */` |
|      - | 7915 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 7916 | `struct strtok_aux_data` |
|      - | 7917 | `{` |
|      - | 7918 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 7919 | `	const char *zIn;   /* Current input stream */` |
|      - | 7920 | `	const char *zEnd;  /* End of input */` |
|      - | 7921 | `};` |
|      - | 7922 | `/*` |
|      - | 7923 | ` * string strtok(string $str,string $token)` |
|      - | 7924 | ` * string strtok(string $token)` |
|      - | 7925 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 7926 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 7927 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 7928 | ` *  words by using the space character as the token.` |
|      - | 7929 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 7930 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 7931 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 7932 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 7933 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 7934 | ` *  the argument are found.` |
|      - | 7935 | ` * Parameters` |
|      - | 7936 | ` *  $str` |
|      - | 7937 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 7938 | ` * $token` |
|      - | 7939 | ` *  The delimiter used when splitting up str.` |
|      - | 7940 | ` * Return` |
|      - | 7941 | ` *   Current token or FALSE on EOF.` |
|      - | 7942 | ` */` |
|      6 | 7943 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7944 | `{` |
|      - | 7945 | `	strtok_aux_data *pAux;` |
|      - | 7946 | `	const char *zMask;` |
|      - | 7947 | `	SyString sToken;` |
|      - | 7948 | `	int nMasklen;` |
|      - | 7949 | `	sxi32 rc;` |
|      7 | 7950 | `	if( nArg < 2 ){` |
|      - | 7951 | `		/* Extract top aux data */` |
|      5 | 7952 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      5 | 7953 | `		if( pAux == 0 ){` |
|      - | 7954 | `			/* No aux data,return FALSE */` |
|    ! 0 | 7955 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7956 | `			return PH7_OK;` |
|      - | 7957 | `		}` |
|      5 | 7958 | `		nMasklen = 0;` |
|      5 | 7959 | `		zMask = ""; /* cc warning */` |
|      5 | 7960 | `		if( nArg > 0 ){` |
|      - | 7961 | `			/* Extract the mask */` |
|      5 | 7962 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 7963 | `		}` |
|      5 | 7964 | `		if( nMasklen < 1 ){` |
|      - | 7965 | `			/* Invalid mask,return FALSE */` |
|    ! 0 | 7966 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 7967 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7968 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 7969 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7970 | `			return PH7_OK;` |
|      - | 7971 | `		}` |
|      - | 7972 | `		/* Extract the token */` |
|      5 | 7973 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 7974 | `		if( rc != SXRET_OK ){` |
|      - | 7975 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 7976 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 7977 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7978 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 7979 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7980 | `		}else{` |
|      - | 7981 | `			/* Return the extracted token */` |
|      5 | 7982 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 7983 | `		}` |
|      3 | 7984 | `	}else{` |
|      - | 7985 | `		const char *zInput,*zCur;` |
|      - | 7986 | `		char *zDup;` |
|      - | 7987 | `		int nLen;` |
|      - | 7988 | `		/* Extract the raw input */` |
|      3 | 7989 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 7990 | `		if( nLen < 1 ){` |
|      - | 7991 | `			/* Empty input,return FALSE */` |
|    ! 0 | 7992 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7993 | `			return PH7_OK;` |
|      - | 7994 | `		}` |
|      - | 7995 | `		/* Extract the mask */` |
|      3 | 7996 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 7997 | `		if( nMasklen < 1 ){` |
|      - | 7998 | `			/* Set a default mask */` |
|      - | 7999 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 8000 | `			zMask = TOK_MASK;` |
|    ! 0 | 8001 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 8002 | `#undef TOK_MASK` |
|    ! 0 | 8003 | `		}` |
|      - | 8004 | `		/* Extract a single token */` |
|      3 | 8005 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 8006 | `		if( rc != SXRET_OK ){` |
|      - | 8007 | `			/* Empty input */` |
|    ! 0 | 8008 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 8009 | `			return PH7_OK;` |
|    ! 0 | 8010 | `		}else{` |
|      - | 8011 | `			/* Return the extracted token */` |
|      3 | 8012 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 8013 | `		}` |
|      - | 8014 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 8015 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 8016 | `		if( pAux ){` |
|      3 | 8017 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 8018 | `			if( nLen < 1 ){` |
|    ! 0 | 8019 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 8020 | `				return PH7_OK;` |
|      - | 8021 | `			}` |
|      - | 8022 | `			/* Duplicate input */` |
|      3 | 8023 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 8024 | `			if( zDup  ){` |
|      3 | 8025 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 8026 | `				/* Register the aux data */` |
|      3 | 8027 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 8028 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 8029 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 8030 | `			}` |
|      1 | 8031 | `		}` |
|      - | 8032 | `	}` |
|      7 | 8033 | `	return PH7_OK;` |
|      4 | 8034 | `}` |
|      - | 8035 | `/*` |
|      - | 8036 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 8037 | ` *  Pad a string to a certain length with another string` |
|      - | 8038 | ` * Parameters` |
|      - | 8039 | ` *  $input` |
|      - | 8040 | ` *   The input string.` |
|      - | 8041 | ` * $pad_length` |
|      - | 8042 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 8043 | ` *   string, no padding takes place.` |
|      - | 8044 | ` * $pad_string` |
|      - | 8045 | ` *   Note:` |
|      - | 8046 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 8047 | ` *    divided by the pad_string's length.` |
|      - | 8048 | ` * $pad_type` |
|      - | 8049 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 8050 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 8051 | ` * Return` |
|      - | 8052 | ` *  The padded string.` |
|      - | 8053 | ` */` |
|     10 | 8054 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8055 | `{` |
|      - | 8056 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 8057 | `	const char *zIn,*zPad;` |
|     11 | 8058 | `	if( nArg < 2 ){` |
|      - | 8059 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 8060 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 8061 | `		return PH7_OK;` |
|      - | 8062 | `	}` |
|      - | 8063 | `	/* Extract the target string */` |
|     11 | 8064 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 8065 | `	/* Padding length */` |
|      - | 8066 | `	{` |
|     11 | 8067 | `		sxi64 iTmp = 0;` |
|     11 | 8068 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"str_pad",2,"$length","int",&iTmp);` |
|     11 | 8069 | `		if( rcArg != PH7_OK ){` |
|    ! 0 | 8070 | `			return rcArg;` |
|      - | 8071 | `		}` |
|     11 | 8072 | `		iRealPad = iPadlen = (int)iTmp;` |
|      - | 8073 | `	}` |
|     11 | 8074 | `	if( iPadlen > 0 ){` |
|      9 | 8075 | `		iPadlen -= iLen;` |
|      4 | 8076 | `	}` |
|     11 | 8077 | `	if( iPadlen < 1  ){` |
|      - | 8078 | `		/* Return the string verbatim */` |
|      5 | 8079 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      5 | 8080 | `		return PH7_OK;` |
|      - | 8081 | `	}` |
|      7 | 8082 | `	zPad = " "; /* Whitespace padding */` |
|      7 | 8083 | `	iStrpad = (int)sizeof(char);` |
|      7 | 8084 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      7 | 8085 | `	if( nArg > 2 ){` |
|      - | 8086 | `		/* Padding string */` |
|      7 | 8087 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      7 | 8088 | `		if( iStrpad < 1 ){` |
|      - | 8089 | `			/* An empty pad string throws a catchable ValueError in PHP 8` |
|      - | 8090 | `			 * (only reached once padding is actually required). */` |
|      3 | 8091 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 8092 | `				"str_pad(): Argument #3 ($pad_string) must not be empty");` |
|      - | 8093 | `		}` |
|      5 | 8094 | `		if( nArg > 3 ){` |
|      - | 8095 | `			/* Padd type */` |
|      5 | 8096 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 8097 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 8098 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 8099 | `			}` |
|      2 | 8100 | `		}` |
|      2 | 8101 | `	}` |
|      5 | 8102 | `	iDiv = 1;` |
|      5 | 8103 | `	if( iType == 2 ){` |
|    ! 0 | 8104 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 8105 | `	}` |
|      - | 8106 | `	/* Perform the requested operation */` |
|      5 | 8107 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 8108 | `		jPad = iStrpad;` |
|      5 | 8109 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 8110 | `			/* Padding */` |
|      5 | 8111 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 8112 | `				break;` |
|      - | 8113 | `			}` |
|      3 | 8114 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 8115 | `		}` |
|      3 | 8116 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 8117 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 8118 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 8119 | `				if( jPad > iStrpad ){` |
|    ! 0 | 8120 | `					jPad = iStrpad;` |
|    ! 0 | 8121 | `				}` |
|      3 | 8122 | `				if( jPad < 1){` |
|    ! 0 | 8123 | `					break;` |
|      - | 8124 | `				}` |
|      3 | 8125 | `				if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 8126 | `			}` |
|      1 | 8127 | `		}` |
|      1 | 8128 | `	}` |
|      5 | 8129 | `	if( iLen > 0 ){` |
|      - | 8130 | `		/* Append the input string */` |
|      5 | 8131 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 8132 | `	}` |
|      5 | 8133 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 8134 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 8135 | `			/* Padding */` |
|      5 | 8136 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 8137 | `				break;` |
|      - | 8138 | `			}` |
|      3 | 8139 | `			if( ph7_result_string(pCtx,zPad,iStrpad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 8140 | `		}` |
|      5 | 8141 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 8142 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 8143 | `			if( jPad > iStrpad ){` |
|    ! 0 | 8144 | `				jPad = iStrpad;` |
|    ! 0 | 8145 | `			}` |
|      3 | 8146 | `			if( jPad < 1){` |
|    ! 0 | 8147 | `				break;` |
|      - | 8148 | `			}` |
|      3 | 8149 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 8150 | `		}` |
|      1 | 8151 | `	}` |
|      5 | 8152 | `	return PH7_OK;` |
|      6 | 8153 | `}` |
|      - | 8154 | `/*` |
|      - | 8155 | ` * String replacement private data.` |
|      - | 8156 | ` */` |
|      - | 8157 | `typedef struct str_replace_data str_replace_data;` |
|      - | 8158 | `struct str_replace_data` |
|      - | 8159 | `{` |
|      - | 8160 | `	/* Used by the str_replace family to collect the search/replace arguments. */` |
|      - | 8161 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 8162 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 8163 | `	sxi32 rc;           /* Carries an allocation failure (SXERR_MEM) out of a walker */` |
|      - | 8164 | `};` |
|      - | 8165 | `/*` |
|      - | 8166 | ` * Remove a substring.` |
|      - | 8167 | ` */` |
|      - | 8168 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 8169 | `	for(;;){\` |
|      - | 8170 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 8171 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 8172 | `		++OFFT;\` |
|      - | 8173 | `	}\` |
|      - | 8174 | `}` |
|      - | 8175 | `/*` |
|      - | 8176 | ` * Shift right and insert algorithm.` |
|      - | 8177 | ` */` |
|      - | 8178 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 8179 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 8180 | `		for(;;){\` |
|      - | 8181 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 8182 | `			if(INLEN < 1 ) { break; }\` |
|      - | 8183 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 8184 | `			--INLEN; \` |
|      - | 8185 | `		}\` |
|      - | 8186 | `		for(;;){\` |
|      - | 8187 | `				if(ELEN < 1) { break; }\` |
|      - | 8188 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 8189 | `				OFFT++;\` |
|      - | 8190 | `				ENTRY++;\` |
|      - | 8191 | `				--ELEN;\` |
|      - | 8192 | `		}\` |
|      - | 8193 | `}` |
|      - | 8194 | `/*` |
|      - | 8195 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 8196 | ` * replacement string [i.e: zReplace].` |
|      - | 8197 | ` */` |
|     52 | 8198 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      5 | 8199 | `{` |
|     57 | 8200 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 8201 | `	sxu32 n,m;` |
|     57 | 8202 | `	n = SyBlobLength(pWorker);` |
|     57 | 8203 | `	m = nOfft;` |
|      - | 8204 | `	/* Delete the old entry */` |
|   6591 | 8205 | `	STRDEL(zInput,n,m,nLen);` |
|     57 | 8206 | `	SyBlobLength(pWorker) -= nLen;` |
|     57 | 8207 | `	if( nReplen > 0 ){` |
|     51 | 8208 | `		sxi32 iRep = nReplen;` |
|      - | 8209 | `		sxi32 rc;` |
|      - | 8210 | `		/*` |
|      - | 8211 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 8212 | `		 * string.` |
|      - | 8213 | `		 */` |
|     51 | 8214 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     51 | 8215 | `		if( rc != SXRET_OK ){` |
|      - | 8216 | `			/* Propagate the allocation failure so the caller can raise a fatal` |
|      - | 8217 | `			 * instead of returning a partially-replaced string as success. */` |
|    ! 0 | 8218 | `			return rc;` |
|      - | 8219 | `		}` |
|      - | 8220 | `		/* Perform the insertion now */` |
|     51 | 8221 | `		zInput = (char *)SyBlobData(pWorker);` |
|     51 | 8222 | `		n = SyBlobLength(pWorker);` |
|   6381 | 8223 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     51 | 8224 | `		SyBlobLength(pWorker) += nReplen;` |
|     23 | 8225 | `	}` |
|     57 | 8226 | `	return SXRET_OK;` |
|     31 | 8227 | `}` |
|      - | 8228 | `/*` |
|      - | 8229 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 8230 | ` * to collect search/replace string.` |
|      - | 8231 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 8232 | ` */` |
|    162 | 8233 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      5 | 8234 | `{` |
|    167 | 8235 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 8236 | `	SyString sWorker;` |
|      - | 8237 | `	const char *zIn;` |
|      - | 8238 | `	int nByte;` |
|      - | 8239 | `	/* Extract a string representation of the given argument */` |
|    167 | 8240 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|    167 | 8241 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|    167 | 8242 | `	if( nByte > 0 ){` |
|      - | 8243 | `		char *zDup;` |
|      - | 8244 | `		/* Duplicate the chunk */` |
|    165 | 8245 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 8246 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 8247 | `			);` |
|    165 | 8248 | `		if( zDup == 0 ){` |
|      - | 8249 | `			/* Allocation failure: carry it out and stop the walk so the caller` |
|      - | 8250 | `			 * raises a fatal instead of silently dropping a search/replace term. */` |
|    ! 0 | 8251 | `			pRep->rc = SXERR_MEM;` |
|    ! 0 | 8252 | `			return SXERR_MEM;` |
|      - | 8253 | `		}` |
|    165 | 8254 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 8255 | `		/* Save the chunk */` |
|    165 | 8256 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     80 | 8257 | `	}` |
|      - | 8258 | `	/* Save for later processing */` |
|    167 | 8259 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 8260 | `	/* All done */` |
|     81 | 8261 | `	SXUNUSED(pKey); /* cc warning */` |
|    167 | 8262 | `	return PH7_OK;` |
|     86 | 8263 | `}` |
|      - | 8264 | `/*` |
|      - | 8265 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 8266 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 8267 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 8268 | ` * Parameters` |
|      - | 8269 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 8270 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 8271 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 8272 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 8273 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 8274 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 8275 | ` * $search` |
|      - | 8276 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 8277 | ` *  to designate multiple needles.` |
|      - | 8278 | ` * $replace` |
|      - | 8279 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 8280 | ` *  to designate multiple replacements.` |
|      - | 8281 | ` * $subject` |
|      - | 8282 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 8283 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 8284 | ` *  of subject, and the return value is an array as well.` |
|      - | 8285 | ` * $count (Not used)` |
|      - | 8286 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 8287 | ` * Return` |
|      - | 8288 | ` * This function returns a string or an array with the replaced values.` |
|      - | 8289 | ` */` |
|  30420 | 8290 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 8291 | `{` |
|      - | 8292 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 8293 | `	ProcStringMatch xMatch;` |
|      - | 8294 | `	const char *zIn,*zFunc;` |
|      - | 8295 | `	str_replace_data sRep;` |
|      - | 8296 | `	SyBlob sWorker;` |
|      - | 8297 | `	SySet sReplace;` |
|      - | 8298 | `	SySet sSearch;` |
|      - | 8299 | `	int rep_str;` |
|      - | 8300 | `	int nByte;` |
|      - | 8301 | `	sxi32 rc;` |
|  30425 | 8302 | `	if( nArg < 3 ){` |
|      - | 8303 | `		/* Missing/Invalid arguments,return null */` |
|    ! 0 | 8304 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8305 | `		return PH7_OK;` |
|      - | 8306 | `	}` |
|      - | 8307 | `	/* Initialize fields */` |
|  30425 | 8308 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  30425 | 8309 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  30425 | 8310 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  30425 | 8311 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  30425 | 8312 | `	sRep.pCtx = pCtx;` |
|  30425 | 8313 | `	sRep.pCollector = &sSearch;` |
|  30425 | 8314 | `	rep_str = 0;` |
|      - | 8315 | `	/* Extract the subject */` |
|  30425 | 8316 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  30425 | 8317 | `	if( nByte < 1 ){` |
|      - | 8318 | `		/* Nothing to replace,return the empty string */` |
|     21 | 8319 | `		ph7_result_string(pCtx,"",0);` |
|     21 | 8320 | `		return PH7_OK;` |
|      - | 8321 | `	}` |
|      - | 8322 | `	/* Copy the subject */` |
|  30405 | 8323 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 8324 | `	/* Search string */` |
|  30405 | 8325 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 8326 | `		/* Collect search string */` |
|     81 | 8327 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|     43 | 8328 | `	}else{` |
|      - | 8329 | `		/* Single pattern */` |
|  30329 | 8330 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  30329 | 8331 | `		if( nByte < 1 ){` |
|      - | 8332 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 8333 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 8334 | `			return PH7_OK;` |
|      - | 8335 | `		}` |
|  30325 | 8336 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 8337 | `		/* Save for later processing */` |
|  30325 | 8338 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 8339 | `	}` |
|      - | 8340 | `	/* Replace string */` |
|  30401 | 8341 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 8342 | `		/* Collect replace string */` |
|      7 | 8343 | `		sRep.pCollector = &sReplace;` |
|      7 | 8344 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 8345 | `	}else{` |
|      - | 8346 | `		/* Single needle */` |
|  30395 | 8347 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  30395 | 8348 | `		rep_str = 1;` |
|  30395 | 8349 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 8350 | `		/* Save for later processing */` |
|  30395 | 8351 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 8352 | `	}` |
|      - | 8353 | `	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */` |
|  30401 | 8354 | `	if( sRep.rc != SXRET_OK ){` |
|    ! 0 | 8355 | `		SySetRelease(&sSearch);` |
|    ! 0 | 8356 | `		SySetRelease(&sReplace);` |
|    ! 0 | 8357 | `		SyBlobRelease(&sWorker);` |
|    ! 0 | 8358 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8359 | `	}` |
|      - | 8360 | `	/* Reset loop cursors */` |
|  30401 | 8361 | `	SySetResetCursor(&sSearch);` |
|  30401 | 8362 | `	SySetResetCursor(&sReplace);` |
|  30401 | 8363 | `	pReplace = pSearch = 0; /* cc warning */` |
|  30401 | 8364 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 8365 | `	/* Extract function name */` |
|  30401 | 8366 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 8367 | `	/* Set the default pattern match routine */` |
|  30401 | 8368 | `	xMatch = SyBlobSearch;` |
|  30401 | 8369 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 8370 | `		/* Case insensitive pattern match */` |
|     11 | 8371 | `		xMatch = iPatternMatch;` |
|      5 | 8372 | `	}` |
|      - | 8373 | `	/* Start the replace process */` |
|  60873 | 8374 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 8375 | `		sxu32 nCount,nOfft;` |
|  30477 | 8376 | `		if( pSearch->nByte <  1 ){` |
|      - | 8377 | `			/* Empty string,ignore */` |
|      3 | 8378 | `			continue;` |
|      - | 8379 | `		}` |
|      - | 8380 | `		/* Extract the replace string */` |
|  30475 | 8381 | `		if( rep_str ){` |
|  30465 | 8382 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|  15235 | 8383 | `		}else{` |
|     11 | 8384 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 8385 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 8386 | `				 * An empty string is used for the rest of replacement values` |
|      - | 8387 | `				 */` |
|      3 | 8388 | `				pReplace = 0;` |
|      1 | 8389 | `			}` |
|      - | 8390 | `		}` |
|  30475 | 8391 | `		if( pReplace == 0 ){` |
|      - | 8392 | `			/* Use an empty string instead */` |
|      3 | 8393 | `			pReplace = &sTemp;` |
|      1 | 8394 | `		}` |
|  30475 | 8395 | `		nOfft = nCount = 0;` |
|  15261 | 8396 | `		for(;;){` |
|  30527 | 8397 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 8398 | `				break;` |
|      - | 8399 | `			}` |
|      - | 8400 | `			/* Perform a pattern lookup */` |
|  45770 | 8401 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  30510 | 8402 | `				pSearch->nByte,&nOfft);` |
|  30515 | 8403 | `			if( rc != SXRET_OK ){` |
|      - | 8404 | `				/* Pattern not found */` |
|  30463 | 8405 | `				break;` |
|      - | 8406 | `			}` |
|      - | 8407 | `			/* Perform the replace operation */` |
|     57 | 8408 | `			rc = StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|     57 | 8409 | `			if( rc != SXRET_OK ){` |
|      - | 8410 | `				/* Allocation failure: surface a fatal instead of a partial result */` |
|    ! 0 | 8411 | `				SySetRelease(&sSearch);` |
|    ! 0 | 8412 | `				SySetRelease(&sReplace);` |
|    ! 0 | 8413 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8414 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8415 | `			}` |
|      - | 8416 | `			/* Increment offset counter */` |
|     57 | 8417 | `			nCount += nOfft + pReplace->nByte;` |
|      5 | 8418 | `		}` |
|      5 | 8419 | `	}` |
|      - | 8420 | `	/* All done,clean-up the mess left behind */` |
|  30401 | 8421 | `	rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  30401 | 8422 | `	SySetRelease(&sSearch);` |
|  30401 | 8423 | `	SySetRelease(&sReplace);` |
|  30401 | 8424 | `	SyBlobRelease(&sWorker);` |
|  30401 | 8425 | `	if( rc != PH7_OK ){` |
|    ! 0 | 8426 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8427 | `	}` |
|  30401 | 8428 | `	return PH7_OK;` |
|  15215 | 8429 | `}` |
|      - | 8430 | `/*` |
|      - | 8431 | ` * strtr() array form: a single (key,value) pair copied out of the replace_pairs` |
|      - | 8432 | ` * array. The bytes are owned by a persistent pool (see strtr_collect) rather than` |
|      - | 8433 | ` * the transient walker values, which HashmapWalk releases after each callback, so` |
|      - | 8434 | ` * we store byte offsets into that pool instead of raw pointers.` |
|      - | 8435 | ` */` |
|      - | 8436 | `typedef struct strtr_entry strtr_entry;` |
|      - | 8437 | `struct strtr_entry` |
|      - | 8438 | `{` |
|      - | 8439 | `	sxu32 nKeyOfft; /* Offset of the search key inside the pool */` |
|      - | 8440 | `	sxu32 nKeyLen;  /* Length of the search key */` |
|      - | 8441 | `	sxu32 nValOfft; /* Offset of the replacement inside the pool */` |
|      - | 8442 | `	sxu32 nValLen;  /* Length of the replacement */` |
|      - | 8443 | `};` |
|      - | 8444 | `typedef struct strtr_collect strtr_collect;` |
|      - | 8445 | `struct strtr_collect` |
|      - | 8446 | `{` |
|      - | 8447 | `	SyBlob *pPool;  /* Byte pool holding copied key + value bytes */` |
|      - | 8448 | `	SySet  *pTable; /* Set of strtr_entry (parallel offsets into pPool) */` |
|      - | 8449 | `	sxi32   rc;     /* Carries an allocation failure (SXERR_MEM) out of the walker */` |
|      - | 8450 | `};` |
|      - | 8451 | `/*` |
|      - | 8452 | ` * Collect one replace_pairs entry into the persistent pool/offset table.` |
|      - | 8453 | ` * PHP coerces both the key and the value to string (an integer key becomes its` |
|      - | 8454 | ` * decimal form) and ignores an empty-string key.` |
|      - | 8455 | ` */` |
|     20 | 8456 | `static int StrtrCollectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 8457 | `{` |
|     21 | 8458 | `	strtr_collect *pCol = (strtr_collect *)pUserData;` |
|      - | 8459 | `	const char *zKey,*zVal;` |
|      - | 8460 | `	strtr_entry sEnt;` |
|      - | 8461 | `	int nKey,nVal;` |
|     21 | 8462 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|     21 | 8463 | `	if( nKey < 1 ){` |
|      - | 8464 | `		/* PHP ignores an empty-string key (it also emits a warning we do not replicate). */` |
|      3 | 8465 | `		return PH7_OK;` |
|      - | 8466 | `	}` |
|     19 | 8467 | `	zVal = ph7_value_to_string(pData,&nVal);` |
|     19 | 8468 | `	sEnt.nKeyOfft = SyBlobLength(pCol->pPool);` |
|     19 | 8469 | `	sEnt.nKeyLen  = (sxu32)nKey;` |
|     19 | 8470 | `	if( SyBlobAppend(pCol->pPool,(const void *)zKey,(sxu32)nKey) != SXRET_OK ){` |
|    ! 0 | 8471 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8472 | `		return SXERR_ABORT;` |
|      - | 8473 | `	}` |
|     19 | 8474 | `	sEnt.nValOfft = SyBlobLength(pCol->pPool);` |
|     19 | 8475 | `	sEnt.nValLen  = (sxu32)nVal;` |
|     19 | 8476 | `	if( nVal > 0 && SyBlobAppend(pCol->pPool,(const void *)zVal,(sxu32)nVal) != SXRET_OK ){` |
|    ! 0 | 8477 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8478 | `		return SXERR_ABORT;` |
|      - | 8479 | `	}` |
|     19 | 8480 | `	if( SySetPut(pCol->pTable,(const void *)&sEnt) != SXRET_OK ){` |
|    ! 0 | 8481 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8482 | `		return SXERR_ABORT;` |
|      - | 8483 | `	}` |
|     19 | 8484 | `	return PH7_OK;` |
|     11 | 8485 | `}` |
|      - | 8486 | `/*` |
|      - | 8487 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 8488 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 8489 | ` *  Translate characters or replace substrings.` |
|      - | 8490 | ` * Parameters` |
|      - | 8491 | ` *  $str` |
|      - | 8492 | ` *  The string being translated.` |
|      - | 8493 | ` * $from` |
|      - | 8494 | ` *  The string being translated to to.` |
|      - | 8495 | ` * $to` |
|      - | 8496 | ` *  The string replacing from.` |
|      - | 8497 | ` * $replace_pairs` |
|      - | 8498 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 8499 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 8500 | ` * Return` |
|      - | 8501 | ` *  The translated string.` |
|      - | 8502 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 8503 | ` */` |
|     12 | 8504 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8505 | `{` |
|      - | 8506 | `	const char *zIn;` |
|      - | 8507 | `	int nLen;` |
|     13 | 8508 | `	if( nArg < 1 ){` |
|      - | 8509 | `		/* Nothing to replace,return FALSE */` |
|    ! 0 | 8510 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8511 | `		return PH7_OK;` |
|      - | 8512 | `	}` |
|     13 | 8513 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 8514 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 8515 | `		/* Invalid arguments */` |
|    ! 0 | 8516 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8517 | `		return PH7_OK;` |
|      - | 8518 | `	}` |
|     18 | 8519 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 8520 | `		strtr_collect sCol;` |
|      - | 8521 | `		SyBlob sPool,sWorker;` |
|      - | 8522 | `		SySet sTable;` |
|      - | 8523 | `		const char *zPool;` |
|      - | 8524 | `		strtr_entry *pEnt;` |
|      - | 8525 | `		sxi32 rc;` |
|      - | 8526 | `		int i,iRun;` |
|      - | 8527 | `		/*` |
|      - | 8528 | `		 * PHP's array-form strtr is a single left-to-right pass over the subject:` |
|      - | 8529 | `		 * at every position it substitutes the LONGEST replace_pairs key that` |
|      - | 8530 | `		 * matches there, then advances past the key (replacements are never` |
|      - | 8531 | `		 * rescanned). It is not a sequential per-key global replace. First copy` |
|      - | 8532 | `		 * the pairs into a persistent pool, then run that scan.` |
|      - | 8533 | `		 */` |
|     11 | 8534 | `		SyBlobInit(&sPool,&pCtx->pVm->sAllocator);` |
|     11 | 8535 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     11 | 8536 | `		SySetInit(&sTable,&pCtx->pVm->sAllocator,sizeof(strtr_entry));` |
|     11 | 8537 | `		sCol.pPool  = &sPool;` |
|     11 | 8538 | `		sCol.pTable = &sTable;` |
|     11 | 8539 | `		sCol.rc     = SXRET_OK;` |
|     11 | 8540 | `		ph7_array_walk(apArg[1],StrtrCollectWalker,&sCol);` |
|     11 | 8541 | `		if( sCol.rc != SXRET_OK ){` |
|      - | 8542 | `			/* Allocation failure while collecting the pairs: surface a fatal */` |
|    ! 0 | 8543 | `			SyBlobRelease(&sPool);` |
|    ! 0 | 8544 | `			SyBlobRelease(&sWorker);` |
|    ! 0 | 8545 | `			SySetRelease(&sTable);` |
|    ! 0 | 8546 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 8547 | `		}` |
|      - | 8548 | `		/* The pool is now stable, so offsets can be resolved against its base. */` |
|     11 | 8549 | `		zPool = (const char *)SyBlobData(&sPool);` |
|     11 | 8550 | `		rc = SXRET_OK;` |
|     11 | 8551 | `		iRun = 0; /* Start of the pending run of unmatched bytes copied verbatim. */` |
|     43 | 8552 | `		for( i = 0 ; i < nLen ; ){` |
|     33 | 8553 | `			strtr_entry *pBest = 0;` |
|     33 | 8554 | `			sxu32 nBest = 0;` |
|      - | 8555 | `			/* Pick the longest key that matches at the current position. */` |
|     33 | 8556 | `			SySetResetCursor(&sTable);` |
|     87 | 8557 | `			while( SXRET_OK == SySetGetNextEntry(&sTable,(void **)&pEnt) ){` |
|     54 | 8558 | `				if( pEnt->nKeyLen > nBest` |
|     50 | 8559 | `					&& pEnt->nKeyLen <= (sxu32)(nLen - i)` |
|     46 | 8560 | `					&& SyMemcmp(zPool + pEnt->nKeyOfft,zIn + i,pEnt->nKeyLen) == 0 ){` |
|     29 | 8561 | `					nBest = pEnt->nKeyLen;` |
|     29 | 8562 | `					pBest = pEnt;` |
|     14 | 8563 | `				}` |
|      1 | 8564 | `			}` |
|     33 | 8565 | `			if( pBest == 0 ){` |
|      - | 8566 | `				/* No key here: extend the literal run and copy it in one shot later. */` |
|      9 | 8567 | `				i++;` |
|      9 | 8568 | `				continue;` |
|      - | 8569 | `			}` |
|      - | 8570 | `			/* Flush the pending literal run, then the replacement. */` |
|     25 | 8571 | `			if( i > iRun ){` |
|      5 | 8572 | `				rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(i - iRun));` |
|      2 | 8573 | `			}` |
|     25 | 8574 | `			if( rc == SXRET_OK && pBest->nValLen > 0 ){` |
|     25 | 8575 | `				rc = SyBlobAppend(&sWorker,zPool + pBest->nValOfft,pBest->nValLen);` |
|     12 | 8576 | `			}` |
|     25 | 8577 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 8578 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 8579 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8580 | `				SySetRelease(&sTable);` |
|    ! 0 | 8581 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8582 | `			}` |
|     25 | 8583 | `			i += (int)pBest->nKeyLen;` |
|     25 | 8584 | `			iRun = i;` |
|      1 | 8585 | `		}` |
|      - | 8586 | `		/* Flush the trailing literal run. */` |
|     11 | 8587 | `		if( nLen > iRun ){` |
|      3 | 8588 | `			rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(nLen - iRun));` |
|      3 | 8589 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 8590 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 8591 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8592 | `				SySetRelease(&sTable);` |
|    ! 0 | 8593 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8594 | `			}` |
|      1 | 8595 | `		}` |
|      - | 8596 | `		/* All done, return the result string */` |
|     16 | 8597 | `		rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|     10 | 8598 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 8599 | `		/* Clean-up */` |
|     11 | 8600 | `		SyBlobRelease(&sPool);` |
|     11 | 8601 | `		SyBlobRelease(&sWorker);` |
|     11 | 8602 | `		SySetRelease(&sTable);` |
|     11 | 8603 | `		if( rc != PH7_OK ){` |
|    ! 0 | 8604 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 8605 | `		}` |
|      6 | 8606 | `	}else{` |
|      - | 8607 | `		int i,flen,tlen,c,iOfft;` |
|      - | 8608 | `		const char *zFrom,*zTo;` |
|      3 | 8609 | `		if( nArg < 3 ){` |
|      - | 8610 | `			/* Nothing to replace */` |
|    ! 0 | 8611 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8612 | `			return PH7_OK;` |
|      - | 8613 | `		}` |
|      - | 8614 | `		/* Extract given arguments */` |
|      3 | 8615 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 8616 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 8617 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 8618 | `			/* Nothing to replace */` |
|    ! 0 | 8619 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8620 | `			return PH7_OK;` |
|      - | 8621 | `		}` |
|      - | 8622 | `		/* Start the replace process */` |
|     13 | 8623 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 8624 | `			c = zIn[i];` |
|     11 | 8625 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 8626 | `				if ( iOfft < tlen ){` |
|      5 | 8627 | `					c = zTo[iOfft];` |
|      2 | 8628 | `				}` |
|      2 | 8629 | `			}` |
|     11 | 8630 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 8631 |  |
|      6 | 8632 | `		}` |
|      - | 8633 | `	}` |
|     13 | 8634 | `	return PH7_OK;` |
|      7 | 8635 | `}` |
|      - | 8636 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8637 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8638 | `/*` |
|      - | 8639 | ` * Parse an INI string.` |
|      - | 8640 |  |
|      - | 8641 | ` * According to wikipedia` |
|      - | 8642 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 8643 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 8644 | ` *  Format` |
|      - | 8645 | `*    Properties` |
|      - | 8646 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 8647 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 8648 | `*     Example:` |
|      - | 8649 | `*      name=value` |
|      - | 8650 | `*    Sections` |
|      - | 8651 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 8652 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 8653 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 8654 | `*     or the end of the file. Sections may not be nested.` |
|      - | 8655 | `*     Example:` |
|      - | 8656 | `*      [section]` |
|      - | 8657 | `*   Comments` |
|      - | 8658 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 8659 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 8660 | `*/` |
|     12 | 8661 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 8662 | `{` |
|      - | 8663 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 8664 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 8665 | `	SyHashEntry *pEntry;` |
|      - | 8666 | `	SyString sEntry;` |
|      - | 8667 | `	SyHash sHash;` |
|      - | 8668 | `	int c;` |
|      - | 8669 | `	/* Create an empty array and worker variables */` |
|     13 | 8670 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 8671 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 8672 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 8673 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 8674 | `		/* Out of memory: surface a fatal instead of returning FALSE */` |
|    ! 0 | 8675 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8676 | `	}` |
|     13 | 8677 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 8678 | `	pCur = pArray;` |
|      - | 8679 | `	/* Start the parse process */` |
|     21 | 8680 | `	for(;;){` |
|      - | 8681 | `		/* Ignore leading white spaces */` |
|     69 | 8682 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 8683 | `			zIn++;` |
|      1 | 8684 | `		}` |
|     43 | 8685 | `		if( zIn >= zEnd ){` |
|      - | 8686 | `			/* No more input to process */` |
|     13 | 8687 | `			break;` |
|      - | 8688 | `		}` |
|     31 | 8689 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 8690 | `			/* Comment til the end of line */` |
|    ! 0 | 8691 | `			zIn++;` |
|    ! 0 | 8692 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 8693 | `				zIn++;` |
|    ! 0 | 8694 | `			}` |
|    ! 0 | 8695 | `			continue;` |
|      - | 8696 | `		}` |
|      - | 8697 | `		/* Reset the string cursor of the working variable */` |
|     31 | 8698 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 8699 | `		if( zIn[0] == '[' ){` |
|      - | 8700 | `			/* Section: Extract the section name */` |
|      9 | 8701 | `			zIn++;` |
|      9 | 8702 | `			zCur = zIn;` |
|     73 | 8703 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 8704 | `				zIn++;` |
|      1 | 8705 | `			}` |
|      9 | 8706 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 8707 | `				/* Save the section name */` |
|      5 | 8708 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 8709 | `				SyStringFullTrim(&sEntry);` |
|      5 | 8710 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 8711 | `				if( sEntry.nByte > 0 ){` |
|      - | 8712 | `					/* Associate an array with the section */` |
|      5 | 8713 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 8714 | `					if( pSection ){` |
|      5 | 8715 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 8716 | `						pCur = pSection;` |
|      2 | 8717 | `					}` |
|      2 | 8718 | `				}` |
|      2 | 8719 | `			}` |
|      9 | 8720 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 8721 | `		}else{` |
|      - | 8722 | `			ph7_value *pOldCur;` |
|      - | 8723 | `			int is_array;` |
|      - | 8724 | `			int iLen;` |
|      - | 8725 | `			/* Properties */` |
|     23 | 8726 | `			is_array = 0;` |
|     23 | 8727 | `			zCur = zIn;` |
|     23 | 8728 | `			iLen = 0; /* cc warning */` |
|     23 | 8729 | `			pOldCur = pCur;` |
|    155 | 8730 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 8731 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 8732 | `					/* Array */` |
|    ! 0 | 8733 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 8734 | `					is_array = 1;` |
|    ! 0 | 8735 | `					if( iLen > 0 ){` |
|    ! 0 | 8736 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 8737 | `						/* Query the hashtable */` |
|    ! 0 | 8738 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 8739 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 8740 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 8741 | `						if( pEntry ){` |
|    ! 0 | 8742 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 8743 | `						}else{` |
|      - | 8744 | `							/* Create an empty array */` |
|    ! 0 | 8745 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 8746 | `							if( pvArr ){` |
|      - | 8747 | `								/* Save the entry */` |
|    ! 0 | 8748 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 8749 | `								/* Insert the entry */` |
|    ! 0 | 8750 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 8751 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 8752 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 8753 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 8754 | `							}` |
|      - | 8755 | `						}` |
|    ! 0 | 8756 | `						if( pvArr ){` |
|    ! 0 | 8757 | `							pCur = pvArr;` |
|    ! 0 | 8758 | `						}` |
|    ! 0 | 8759 | `					}` |
|    ! 0 | 8760 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 8761 | `						zIn++;` |
|    ! 0 | 8762 | `					}` |
|    ! 0 | 8763 | `				}` |
|    133 | 8764 | `				zIn++;` |
|      1 | 8765 | `			}` |
|     23 | 8766 | `			if( !is_array ){` |
|     23 | 8767 | `				iLen = (int)(zIn-zCur);` |
|     11 | 8768 | `			}` |
|      - | 8769 | `			/* Trim the key */` |
|     23 | 8770 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 8771 | `			SyStringFullTrim(&sEntry);` |
|     23 | 8772 | `			if( sEntry.nByte > 0 ){` |
|     23 | 8773 | `				if( !is_array ){` |
|      - | 8774 | `					/* Save the key name */` |
|     23 | 8775 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 8776 | `				}` |
|      - | 8777 | `				/* extract key value */` |
|     23 | 8778 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 8779 | `				zIn++; /* '=' */` |
|     39 | 8780 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 8781 | `					zIn++;` |
|      1 | 8782 | `				}` |
|     23 | 8783 | `				if( zIn < zEnd ){` |
|     21 | 8784 | `					zCur = zIn;` |
|     21 | 8785 | `					c = zIn[0];` |
|     21 | 8786 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 8787 | `						zIn++;` |
|      - | 8788 | `						/* Delimit the value */` |
|    ! 0 | 8789 | `						while( zIn < zEnd ){` |
|    ! 0 | 8790 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 8791 | `								break;` |
|      - | 8792 | `							}` |
|    ! 0 | 8793 | `							zIn++;` |
|    ! 0 | 8794 | `						}` |
|    ! 0 | 8795 | `						if( zIn < zEnd ){` |
|    ! 0 | 8796 | `							zIn++;` |
|    ! 0 | 8797 | `						}` |
|    ! 0 | 8798 | `					}else{` |
|    125 | 8799 | `						while( zIn < zEnd ){` |
|    123 | 8800 | `							if( zIn[0] == '\n' ){` |
|     19 | 8801 | `								if( zIn[-1] != '\\' ){` |
|     19 | 8802 | `									break;` |
|    ! 0 | 8803 | `								}` |
|    105 | 8804 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 8805 | `								/* Inline comments */` |
|    ! 0 | 8806 | `								break;` |
|      - | 8807 | `							}` |
|    105 | 8808 | `							zIn++;` |
|      1 | 8809 | `						}` |
|      - | 8810 | `					}` |
|      - | 8811 | `					/* Trim the value */` |
|     21 | 8812 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 8813 | `					SyStringFullTrim(&sEntry);` |
|     21 | 8814 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 8815 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 8816 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 8817 | `					}` |
|     21 | 8818 | `					if( sEntry.nByte > 0 ){` |
|     21 | 8819 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 8820 | `					}` |
|      - | 8821 | `					/* Insert the key and it's value */` |
|     21 | 8822 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 8823 | `				}` |
|     12 | 8824 | `			}else{` |
|    ! 0 | 8825 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 8826 | `					zIn++;` |
|    ! 0 | 8827 | `				}` |
|      - | 8828 | `			}` |
|     23 | 8829 | `			pCur = pOldCur;` |
|      - | 8830 | `		}` |
|      1 | 8831 | `	}` |
|     13 | 8832 | `	SyHashRelease(&sHash);` |
|      - | 8833 | `	/* Return the parse of the INI string */` |
|     13 | 8834 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 8835 | `	return SXRET_OK;` |
|      7 | 8836 | `}` |
|      - | 8837 | `/*` |
|      - | 8838 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 8839 | ` *  Parse a configuration string.` |
|      - | 8840 | ` * Parameters` |
|      - | 8841 | ` *  $ini` |
|      - | 8842 | ` *   The contents of the ini file being parsed.` |
|      - | 8843 | ` *  $process_sections` |
|      - | 8844 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 8845 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 8846 | ` *  $scanner_mode (Not used)` |
|      - | 8847 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 8848 | ` *   then option values will not be parsed.` |
|      - | 8849 | ` * Return` |
|      - | 8850 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 8851 | ` */` |
|     10 | 8852 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8853 | `{` |
|      - | 8854 | `	const char *zIni;` |
|      - | 8855 | `	int nByte;` |
|     11 | 8856 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8857 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 8858 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8859 | `		return PH7_OK;` |
|      - | 8860 | `	}` |
|      - | 8861 | `	/* Extract the raw INI buffer */` |
|     11 | 8862 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 8863 | `	/* Process the INI buffer; propagate an OOM abort so the fatal actually halts */` |
|     11 | 8864 | `	return PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|      6 | 8865 | `}` |
|      - | 8866 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8867 |  |
|      - | 8868 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8869 |  |
|      - | 8870 | `/*` |
|      - | 8871 | ` * Ctype Functions.` |
|      - | 8872 | ` * Status:` |
|      - | 8873 | ` *    Stable.` |
|      - | 8874 | ` */` |
|      - | 8875 | `/*` |
|      - | 8876 | ` * bool ctype_alnum(string $text)` |
|      - | 8877 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 8878 | ` * Parameters` |
|      - | 8879 | ` *  $text` |
|      - | 8880 | ` *   The tested string.` |
|      - | 8881 | ` * Return` |
|      - | 8882 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 8883 | ` */` |
|     72 | 8884 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8885 | `{` |
|      - | 8886 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8887 | `	int nLen;` |
|     73 | 8888 | `	if( nArg < 1 ){` |
|      - | 8889 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8890 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8891 | `		return PH7_OK;` |
|      - | 8892 | `	}` |
|      - | 8893 | `	/* Extract the target string */` |
|     73 | 8894 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     73 | 8895 | `	zEnd = &zIn[nLen];` |
|     73 | 8896 | `	if( nLen < 1 ){` |
|      - | 8897 | `		/* Empty string,return FALSE */` |
|      3 | 8898 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8899 | `		return PH7_OK;` |
|      - | 8900 | `	}` |
|      - | 8901 | `	/* Perform the requested operation */` |
|    110 | 8902 | `	for(;;){` |
|    221 | 8903 | `		if( zIn >= zEnd ){` |
|      - | 8904 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     65 | 8905 | `			ph7_result_bool(pCtx,1);` |
|     65 | 8906 | `			return PH7_OK;` |
|      - | 8907 | `		}` |
|    157 | 8908 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      7 | 8909 | `			break;` |
|      - | 8910 | `		}` |
|      - | 8911 | `		/* Point to the next character */` |
|    151 | 8912 | `		zIn++;` |
|      1 | 8913 | `	}` |
|      - | 8914 | `	/* The test failed,return FALSE */` |
|      7 | 8915 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8916 | `	return PH7_OK;` |
|     37 | 8917 | `}` |
|      - | 8918 | `/*` |
|      - | 8919 | ` * bool ctype_alpha(string $text)` |
|      - | 8920 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 8921 | ` * Parameters` |
|      - | 8922 | ` *  $text` |
|      - | 8923 | ` *   The tested string.` |
|      - | 8924 | ` * Return` |
|      - | 8925 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 8926 | ` */` |
|     16 | 8927 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8928 | `{` |
|      - | 8929 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8930 | `	int nLen;` |
|     17 | 8931 | `	if( nArg < 1 ){` |
|      - | 8932 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8933 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8934 | `		return PH7_OK;` |
|      - | 8935 | `	}` |
|      - | 8936 | `	/* Extract the target string */` |
|     17 | 8937 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8938 | `	zEnd = &zIn[nLen];` |
|     17 | 8939 | `	if( nLen < 1 ){` |
|      - | 8940 | `		/* Empty string,return FALSE */` |
|      3 | 8941 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8942 | `		return PH7_OK;` |
|      - | 8943 | `	}` |
|      - | 8944 | `	/* Perform the requested operation */` |
|     42 | 8945 | `	for(;;){` |
|     85 | 8946 | `		if( zIn >= zEnd ){` |
|      - | 8947 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8948 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8949 | `			return PH7_OK;` |
|      - | 8950 | `		}` |
|     77 | 8951 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 8952 | `			break;` |
|      - | 8953 | `		}` |
|      - | 8954 | `		/* Point to the next character */` |
|     71 | 8955 | `		zIn++;` |
|      1 | 8956 | `	}` |
|      - | 8957 | `	/* The test failed,return FALSE */` |
|      7 | 8958 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8959 | `	return PH7_OK;` |
|      9 | 8960 | `}` |
|      - | 8961 | `/*` |
|      - | 8962 | ` * bool ctype_cntrl(string $text)` |
|      - | 8963 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 8964 | ` * Parameters` |
|      - | 8965 | ` *  $text` |
|      - | 8966 | ` *   The tested string.` |
|      - | 8967 | ` * Return` |
|      - | 8968 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 8969 | ` */` |
|     16 | 8970 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8971 | `{` |
|      - | 8972 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8973 | `	int nLen;` |
|     17 | 8974 | `	if( nArg < 1 ){` |
|      - | 8975 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8976 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8977 | `		return PH7_OK;` |
|      - | 8978 | `	}` |
|      - | 8979 | `	/* Extract the target string */` |
|     17 | 8980 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8981 | `	zEnd = &zIn[nLen];` |
|     17 | 8982 | `	if( nLen < 1 ){` |
|      - | 8983 | `		/* Empty string,return FALSE */` |
|      3 | 8984 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8985 | `		return PH7_OK;` |
|      - | 8986 | `	}` |
|      - | 8987 | `	/* Perform the requested operation */` |
|     14 | 8988 | `	for(;;){` |
|     29 | 8989 | `		if( zIn >= zEnd ){` |
|      - | 8990 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8991 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8992 | `			return PH7_OK;` |
|      - | 8993 | `		}` |
|     21 | 8994 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8995 | `			/* UTF-8 stream  */` |
|    ! 0 | 8996 | `			break;` |
|      - | 8997 | `		}` |
|     21 | 8998 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 8999 | `			break;` |
|      - | 9000 | `		}` |
|      - | 9001 | `		/* Point to the next character */` |
|     15 | 9002 | `		zIn++;` |
|      1 | 9003 | `	}` |
|      - | 9004 | `	/* The test failed,return FALSE */` |
|      7 | 9005 | `	ph7_result_bool(pCtx,0);` |
|      7 | 9006 | `	return PH7_OK;` |
|      9 | 9007 | `}` |
|      - | 9008 | `/*` |
|      - | 9009 | ` * bool ctype_digit(string $text)` |
|      - | 9010 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 9011 | ` * Parameters` |
|      - | 9012 | ` *  $text` |
|      - | 9013 | ` *   The tested string.` |
|      - | 9014 | ` * Return` |
|      - | 9015 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 9016 | ` */` |
|   2632 | 9017 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 9018 | `{` |
|      - | 9019 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9020 | `	int nLen;` |
|   2637 | 9021 | `	if( nArg < 1 ){` |
|      - | 9022 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9023 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9024 | `		return PH7_OK;` |
|      - | 9025 | `	}` |
|      - | 9026 | `	/* Extract the target string */` |
|   2637 | 9027 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   2637 | 9028 | `	zEnd = &zIn[nLen];` |
|   2637 | 9029 | `	if( nLen < 1 ){` |
|      - | 9030 | `		/* Empty string,return FALSE */` |
|      9 | 9031 | `		ph7_result_bool(pCtx,0);` |
|      9 | 9032 | `		return PH7_OK;` |
|      - | 9033 | `	}` |
|      - | 9034 | `	/* Perform the requested operation */` |
|   2421 | 9035 | `	for(;;){` |
|   4847 | 9036 | `		if( zIn >= zEnd ){` |
|      - | 9037 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   2155 | 9038 | `			ph7_result_bool(pCtx,1);` |
|   2155 | 9039 | `			return PH7_OK;` |
|      - | 9040 | `		}` |
|   2697 | 9041 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9042 | `			/* UTF-8 stream  */` |
|    ! 0 | 9043 | `			break;` |
|      - | 9044 | `		}` |
|   2697 | 9045 | `		if( !SyisDigit(zIn[0]) ){` |
|    479 | 9046 | `			break;` |
|      - | 9047 | `		}` |
|      - | 9048 | `		/* Point to the next character */` |
|   2223 | 9049 | `		zIn++;` |
|      5 | 9050 | `	}` |
|      - | 9051 | `	/* The test failed,return FALSE */` |
|    479 | 9052 | `	ph7_result_bool(pCtx,0);` |
|    479 | 9053 | `	return PH7_OK;` |
|   1321 | 9054 | `}` |
|      - | 9055 | `/*` |
|      - | 9056 | ` * bool ctype_xdigit(string $text)` |
|      - | 9057 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 9058 | ` * Parameters` |
|      - | 9059 | ` *  $text` |
|      - | 9060 | ` *   The tested string.` |
|      - | 9061 | ` * Return` |
|      - | 9062 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 9063 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 9064 | ` */` |
|     38 | 9065 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 9066 | `{` |
|      - | 9067 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9068 | `	int nLen;` |
|     40 | 9069 | `	if( nArg < 1 ){` |
|      - | 9070 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9071 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9072 | `		return PH7_OK;` |
|      - | 9073 | `	}` |
|      - | 9074 | `	/* Extract the target string */` |
|     40 | 9075 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     40 | 9076 | `	zEnd = &zIn[nLen];` |
|     40 | 9077 | `	if( nLen < 1 ){` |
|      - | 9078 | `		/* Empty string,return FALSE */` |
|      3 | 9079 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9080 | `		return PH7_OK;` |
|      - | 9081 | `	}` |
|      - | 9082 | `	/* Perform the requested operation */` |
|     76 | 9083 | `	for(;;){` |
|    154 | 9084 | `		if( zIn >= zEnd ){` |
|      - | 9085 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     32 | 9086 | `			ph7_result_bool(pCtx,1);` |
|     32 | 9087 | `			return PH7_OK;` |
|      - | 9088 | `		}` |
|    124 | 9089 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9090 | `			/* UTF-8 stream  */` |
|    ! 0 | 9091 | `			break;` |
|      - | 9092 | `		}` |
|    124 | 9093 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 9094 | `			break;` |
|      - | 9095 | `		}` |
|      - | 9096 | `		/* Point to the next character */` |
|    118 | 9097 | `		zIn++;` |
|      2 | 9098 | `	}` |
|      - | 9099 | `	/* The test failed,return FALSE */` |
|      7 | 9100 | `	ph7_result_bool(pCtx,0);` |
|      7 | 9101 | `	return PH7_OK;` |
|     21 | 9102 | `}` |
|      - | 9103 | `/*` |
|      - | 9104 | ` * bool ctype_graph(string $text)` |
|      - | 9105 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 9106 | ` * Parameters` |
|      - | 9107 | ` *  $text` |
|      - | 9108 | ` *   The tested string.` |
|      - | 9109 | ` * Return` |
|      - | 9110 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 9111 | ` * (no white space), FALSE otherwise.` |
|      - | 9112 | ` */` |
|     16 | 9113 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9114 | `{` |
|      - | 9115 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9116 | `	int nLen;` |
|     17 | 9117 | `	if( nArg < 1 ){` |
|      - | 9118 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9119 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9120 | `		return PH7_OK;` |
|      - | 9121 | `	}` |
|      - | 9122 | `	/* Extract the target string */` |
|     17 | 9123 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9124 | `	zEnd = &zIn[nLen];` |
|     17 | 9125 | `	if( nLen < 1 ){` |
|      - | 9126 | `		/* Empty string,return FALSE */` |
|      3 | 9127 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9128 | `		return PH7_OK;` |
|      - | 9129 | `	}` |
|      - | 9130 | `	/* Perform the requested operation */` |
|     57 | 9131 | `	for(;;){` |
|    115 | 9132 | `		if( zIn >= zEnd ){` |
|      - | 9133 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 9134 | `			ph7_result_bool(pCtx,1);` |
|      9 | 9135 | `			return PH7_OK;` |
|      - | 9136 | `		}` |
|    107 | 9137 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9138 | `			/* UTF-8 stream  */` |
|    ! 0 | 9139 | `			break;` |
|      - | 9140 | `		}` |
|    107 | 9141 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 9142 | `			break;` |
|      - | 9143 | `		}` |
|      - | 9144 | `		/* Point to the next character */` |
|    101 | 9145 | `		zIn++;` |
|      1 | 9146 | `	}` |
|      - | 9147 | `	/* The test failed,return FALSE */` |
|      7 | 9148 | `	ph7_result_bool(pCtx,0);` |
|      7 | 9149 | `	return PH7_OK;` |
|      9 | 9150 | `}` |
|      - | 9151 | `/*` |
|      - | 9152 | ` * bool ctype_print(string $text)` |
|      - | 9153 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 9154 | ` * Parameters` |
|      - | 9155 | ` *  $text` |
|      - | 9156 | ` *   The tested string.` |
|      - | 9157 | ` * Return` |
|      - | 9158 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 9159 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 9160 | ` *  or control function at all.` |
|      - | 9161 | ` */` |
|     16 | 9162 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9163 | `{` |
|      - | 9164 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9165 | `	int nLen;` |
|     17 | 9166 | `	if( nArg < 1 ){` |
|      - | 9167 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9168 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9169 | `		return PH7_OK;` |
|      - | 9170 | `	}` |
|      - | 9171 | `	/* Extract the target string */` |
|     17 | 9172 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9173 | `	zEnd = &zIn[nLen];` |
|     17 | 9174 | `	if( nLen < 1 ){` |
|      - | 9175 | `		/* Empty string,return FALSE */` |
|      3 | 9176 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9177 | `		return PH7_OK;` |
|      - | 9178 | `	}` |
|      - | 9179 | `	/* Perform the requested operation */` |
|     63 | 9180 | `	for(;;){` |
|    127 | 9181 | `		if( zIn >= zEnd ){` |
|      - | 9182 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 9183 | `			ph7_result_bool(pCtx,1);` |
|      9 | 9184 | `			return PH7_OK;` |
|      - | 9185 | `		}` |
|    119 | 9186 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9187 | `			/* UTF-8 stream  */` |
|    ! 0 | 9188 | `			break;` |
|      - | 9189 | `		}` |
|    119 | 9190 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 9191 | `			break;` |
|      - | 9192 | `		}` |
|      - | 9193 | `		/* Point to the next character */` |
|    113 | 9194 | `		zIn++;` |
|      1 | 9195 | `	}` |
|      - | 9196 | `	/* The test failed,return FALSE */` |
|      7 | 9197 | `	ph7_result_bool(pCtx,0);` |
|      7 | 9198 | `	return PH7_OK;` |
|      9 | 9199 | `}` |
|      - | 9200 | `/*` |
|      - | 9201 | ` * bool ctype_punct(string $text)` |
|      - | 9202 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 9203 | ` * Parameters` |
|      - | 9204 | ` *  $text` |
|      - | 9205 | ` *   The tested string.` |
|      - | 9206 | ` * Return` |
|      - | 9207 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 9208 | ` *  digit or blank, FALSE otherwise.` |
|      - | 9209 | ` */` |
|     18 | 9210 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9211 | `{` |
|      - | 9212 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9213 | `	int nLen;` |
|     19 | 9214 | `	if( nArg < 1 ){` |
|      - | 9215 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9216 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9217 | `		return PH7_OK;` |
|      - | 9218 | `	}` |
|      - | 9219 | `	/* Extract the target string */` |
|     19 | 9220 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 9221 | `	zEnd = &zIn[nLen];` |
|     19 | 9222 | `	if( nLen < 1 ){` |
|      - | 9223 | `		/* Empty string,return FALSE */` |
|      3 | 9224 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9225 | `		return PH7_OK;` |
|      - | 9226 | `	}` |
|      - | 9227 | `	/* Perform the requested operation */` |
|     38 | 9228 | `	for(;;){` |
|     77 | 9229 | `		if( zIn >= zEnd ){` |
|      - | 9230 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 9231 | `			ph7_result_bool(pCtx,1);` |
|      9 | 9232 | `			return PH7_OK;` |
|      - | 9233 | `		}` |
|     69 | 9234 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9235 | `			/* UTF-8 stream  */` |
|    ! 0 | 9236 | `			break;` |
|      - | 9237 | `		}` |
|     69 | 9238 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 9239 | `			break;` |
|      - | 9240 | `		}` |
|      - | 9241 | `		/* Point to the next character */` |
|     61 | 9242 | `		zIn++;` |
|      1 | 9243 | `	}` |
|      - | 9244 | `	/* The test failed,return FALSE */` |
|      9 | 9245 | `	ph7_result_bool(pCtx,0);` |
|      9 | 9246 | `	return PH7_OK;` |
|     10 | 9247 | `}` |
|      - | 9248 | `/*` |
|      - | 9249 | ` * bool ctype_space(string $text)` |
|      - | 9250 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 9251 | ` * Parameters` |
|      - | 9252 | ` *  $text` |
|      - | 9253 | ` *   The tested string.` |
|      - | 9254 | ` * Return` |
|      - | 9255 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 9256 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 9257 | ` *  and form feed characters.` |
|      - | 9258 | ` */` |
|  64445 | 9259 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 9260 | `{` |
|      - | 9261 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9262 | `	int nLen;` |
|  64450 | 9263 | `	if( nArg < 1 ){` |
|      - | 9264 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9265 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9266 | `		return PH7_OK;` |
|      - | 9267 | `	}` |
|      - | 9268 | `	/* Extract the target string */` |
|  64450 | 9269 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  64450 | 9270 | `	zEnd = &zIn[nLen];` |
|  64450 | 9271 | `	if( nLen < 1 ){` |
|      - | 9272 | `		/* Empty string,return FALSE */` |
|      3 | 9273 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9274 | `		return PH7_OK;` |
|      - | 9275 | `	}` |
|      - | 9276 | `	/* Perform the requested operation */` |
|  33134 | 9277 | `	for(;;){` |
|  66226 | 9278 | `		if( zIn >= zEnd ){` |
|      - | 9279 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1759 | 9280 | `			ph7_result_bool(pCtx,1);` |
|   1759 | 9281 | `			return PH7_OK;` |
|      - | 9282 | `		}` |
|  64472 | 9283 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9284 | `			/* UTF-8 stream  */` |
|    ! 0 | 9285 | `			break;` |
|      - | 9286 | `		}` |
|  64472 | 9287 | `		if( !SyisSpace(zIn[0]) ){` |
|  62694 | 9288 | `			break;` |
|      - | 9289 | `		}` |
|      - | 9290 | `		/* Point to the next character */` |
|   1783 | 9291 | `		zIn++;` |
|      5 | 9292 | `	}` |
|      - | 9293 | `	/* The test failed,return FALSE */` |
|  62694 | 9294 | `	ph7_result_bool(pCtx,0);` |
|  62694 | 9295 | `	return PH7_OK;` |
|  32251 | 9296 | `}` |
|      - | 9297 | `/*` |
|      - | 9298 | ` * bool ctype_lower(string $text)` |
|      - | 9299 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 9300 | ` * Parameters` |
|      - | 9301 | ` *  $text` |
|      - | 9302 | ` *   The tested string.` |
|      - | 9303 | ` * Return` |
|      - | 9304 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 9305 | ` */` |
|     16 | 9306 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9307 | `{` |
|      - | 9308 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9309 | `	int nLen;` |
|     17 | 9310 | `	if( nArg < 1 ){` |
|      - | 9311 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9312 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9313 | `		return PH7_OK;` |
|      - | 9314 | `	}` |
|      - | 9315 | `	/* Extract the target string */` |
|     17 | 9316 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9317 | `	zEnd = &zIn[nLen];` |
|     17 | 9318 | `	if( nLen < 1 ){` |
|      - | 9319 | `		/* Empty string,return FALSE */` |
|      3 | 9320 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9321 | `		return PH7_OK;` |
|      - | 9322 | `	}` |
|      - | 9323 | `	/* Perform the requested operation */` |
|     27 | 9324 | `	for(;;){` |
|     55 | 9325 | `		if( zIn >= zEnd ){` |
|      - | 9326 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 9327 | `			ph7_result_bool(pCtx,1);` |
|      5 | 9328 | `			return PH7_OK;` |
|      - | 9329 | `		}` |
|     51 | 9330 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 9331 | `			break;` |
|      - | 9332 | `		}` |
|      - | 9333 | `		/* Point to the next character */` |
|     41 | 9334 | `		zIn++;` |
|      1 | 9335 | `	}` |
|      - | 9336 | `	/* The test failed,return FALSE */` |
|     11 | 9337 | `	ph7_result_bool(pCtx,0);` |
|     11 | 9338 | `	return PH7_OK;` |
|      9 | 9339 | `}` |
|      - | 9340 | `/*` |
|      - | 9341 | ` * bool ctype_upper(string $text)` |
|      - | 9342 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 9343 | ` * Parameters` |
|      - | 9344 | ` *  $text` |
|      - | 9345 | ` *   The tested string.` |
|      - | 9346 | ` * Return` |
|      - | 9347 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 9348 | ` */` |
|     16 | 9349 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9350 | `{` |
|      - | 9351 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9352 | `	int nLen;` |
|     17 | 9353 | `	if( nArg < 1 ){` |
|      - | 9354 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9355 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9356 | `		return PH7_OK;` |
|      - | 9357 | `	}` |
|      - | 9358 | `	/* Extract the target string */` |
|     17 | 9359 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9360 | `	zEnd = &zIn[nLen];` |
|     17 | 9361 | `	if( nLen < 1 ){` |
|      - | 9362 | `		/* Empty string,return FALSE */` |
|      3 | 9363 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9364 | `		return PH7_OK;` |
|      - | 9365 | `	}` |
|      - | 9366 | `	/* Perform the requested operation */` |
|     28 | 9367 | `	for(;;){` |
|     57 | 9368 | `		if( zIn >= zEnd ){` |
|      - | 9369 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 9370 | `			ph7_result_bool(pCtx,1);` |
|      5 | 9371 | `			return PH7_OK;` |
|      - | 9372 | `		}` |
|     53 | 9373 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 9374 | `			break;` |
|      - | 9375 | `		}` |
|      - | 9376 | `		/* Point to the next character */` |
|     43 | 9377 | `		zIn++;` |
|      1 | 9378 | `	}` |
|      - | 9379 | `	/* The test failed,return FALSE */` |
|     11 | 9380 | `	ph7_result_bool(pCtx,0);` |
|     11 | 9381 | `	return PH7_OK;` |
|      9 | 9382 | `}` |
|      - | 9383 | `/* Date/Time functions moved to builtin_date.c */` |
|      - | 9384 | `/*` |
|      - | 9385 | ` * Section:` |
|      - | 9386 | ` *    URL handling Functions.` |
|      - | 9387 | ` * Status:` |
|      - | 9388 | ` *    Stable.` |
|      - | 9389 | ` */` |
|      - | 9390 | `/*` |
|      - | 9391 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 9392 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 9393 | ` */` |
|   1270 | 9394 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 9395 | `{` |
|      - | 9396 | `	/* Store in the call context result buffer */` |
|   1272 | 9397 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1272 | 9398 | `	return SXRET_OK;` |
|      2 | 9399 | `}` |
|      - | 9400 | `/*` |
|      - | 9401 | ` * string base64_encode(string $data)` |
|      - | 9402 | ` * string convert_uuencode(string $data)` |
|      - | 9403 | ` *  Encodes data with MIME base64` |
|      - | 9404 | ` * Parameter` |
|      - | 9405 | ` *  $data` |
|      - | 9406 | ` *    Data to encode` |
|      - | 9407 | ` * Return` |
|      - | 9408 | ` *  Encoded data or FALSE on failure.` |
|      - | 9409 | ` */` |
|      6 | 9410 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9411 | `{` |
|      - | 9412 | `	const char *zIn;` |
|      - | 9413 | `	int nLen;` |
|      7 | 9414 | `	if( nArg < 1 ){` |
|      - | 9415 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9416 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9417 | `		return PH7_OK;` |
|      - | 9418 | `	}` |
|      - | 9419 | `	/* Extract the input string */` |
|      7 | 9420 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 9421 | `	if( nLen < 1 ){` |
|      - | 9422 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 9423 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9424 | `		return PH7_OK;` |
|      - | 9425 | `	}` |
|      - | 9426 | `	/* Perform the BASE64 encoding */` |
|      7 | 9427 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 9428 | `	return PH7_OK;` |
|      4 | 9429 | `}` |
|      - | 9430 | `/*` |
|      - | 9431 | ` * string base64_decode(string $data)` |
|      - | 9432 | ` * string convert_uudecode(string $data)` |
|      - | 9433 | ` *  Decodes data encoded with MIME base64` |
|      - | 9434 | ` * Parameter` |
|      - | 9435 | ` *  $data` |
|      - | 9436 | ` *    Encoded data.` |
|      - | 9437 | ` * Return` |
|      - | 9438 | ` *  Returns the original data or FALSE on failure.` |
|      - | 9439 | ` */` |
|     34 | 9440 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 9441 | `{` |
|      - | 9442 | `	const char *zIn;` |
|      - | 9443 | `	int nLen;` |
|     36 | 9444 | `	if( nArg < 1 ){` |
|      - | 9445 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9446 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9447 | `		return PH7_OK;` |
|      - | 9448 | `	}` |
|      - | 9449 | `	/* Extract the input string */` |
|     36 | 9450 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 9451 | `	if( nLen < 1 ){` |
|      - | 9452 | `		/* php decodes the empty string to the EMPTY STRING, not FALSE (FALSE is reserved` |
|      - | 9453 | `		 * for input that cannot be decoded at all). */` |
|      3 | 9454 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 9455 | `		return PH7_OK;` |
|      - | 9456 | `	}` |
|      - | 9457 | `	/* Perform the BASE64 decoding */` |
|     34 | 9458 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 9459 | `	return PH7_OK;` |
|     19 | 9460 | `}` |
|      - | 9461 | `/*` |
|      - | 9462 | ` * string urlencode(string $str)` |
|      - | 9463 | ` *  URL encoding` |
|      - | 9464 | ` * Parameter` |
|      - | 9465 | ` *  $data` |
|      - | 9466 | ` *   Input string.` |
|      - | 9467 | ` * Return` |
|      - | 9468 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 9469 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 9470 | ` *  encoded as plus (+) signs.` |
|      - | 9471 | ` */` |
|    100 | 9472 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9473 | `{` |
|      - | 9474 | `	const char *zIn;` |
|      - | 9475 | `	int nLen;` |
|    101 | 9476 | `	if( nArg < 1 ){` |
|      - | 9477 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9478 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9479 | `		return PH7_OK;` |
|      - | 9480 | `	}` |
|      - | 9481 | `	/* Extract the input string */` |
|    101 | 9482 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|    101 | 9483 | `	if( nLen < 1 ){` |
|      - | 9484 | `		/* php returns an empty string for empty input, not FALSE */` |
|      3 | 9485 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 9486 | `		return PH7_OK;` |
|      - | 9487 | `	}` |
|      - | 9488 | `	/* Perform the URL encoding */` |
|     99 | 9489 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     99 | 9490 | `	return PH7_OK;` |
|     51 | 9491 | `}` |
|      - | 9492 | `/*` |
|      - | 9493 | ` * string rawurlencode(string $str)` |
|      - | 9494 | ` *  RFC 3986 URL encoding: spaces become %20 (not '+') and '~' is left intact.` |
|      - | 9495 | ` */` |
|     14 | 9496 | `static int PH7_builtin_rawurlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9497 | `{` |
|      - | 9498 | `	const char *zIn;` |
|      - | 9499 | `	int nLen;` |
|     15 | 9500 | `	if( nArg < 1 ){` |
|      - | 9501 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9502 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9503 | `		return PH7_OK;` |
|      - | 9504 | `	}` |
|      - | 9505 | `	/* Extract the input string */` |
|     15 | 9506 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 9507 | `	if( nLen < 1 ){` |
|      - | 9508 | `		/* php returns an empty string for empty input, not FALSE */` |
|      3 | 9509 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 9510 | `		return PH7_OK;` |
|      - | 9511 | `	}` |
|      - | 9512 | `	/* Perform the RFC 3986 URL encoding */` |
|     13 | 9513 | `	SyUriEncodeRaw(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     13 | 9514 | `	return PH7_OK;` |
|      8 | 9515 | `}` |
|      - | 9516 | `/*` |
|      - | 9517 | ` * string urldecode(string $str)` |
|      - | 9518 | ` *  Decodes any %## encoding in the given string.` |
|      - | 9519 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 9520 | ` * Parameter` |
|      - | 9521 | ` *  $data` |
|      - | 9522 | ` *    Input string.` |
|      - | 9523 | ` * Return` |
|      - | 9524 | ` *  Decoded URL or FALSE on failure.` |
|      - | 9525 | ` */` |
|    110 | 9526 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9527 | `{` |
|      - | 9528 | `	const char *zIn;` |
|      - | 9529 | `	int nLen;` |
|    111 | 9530 | `	if( nArg < 1 ){` |
|      - | 9531 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9532 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9533 | `		return PH7_OK;` |
|      - | 9534 | `	}` |
|      - | 9535 | `	/* Extract the input string */` |
|    111 | 9536 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|    111 | 9537 | `	if( nLen < 1 ){` |
|      - | 9538 | `		/* php returns an empty string for empty input, not FALSE */` |
|     17 | 9539 | `		ph7_result_string(pCtx,"",0);` |
|     17 | 9540 | `		return PH7_OK;` |
|      - | 9541 | `	}` |
|      - | 9542 | `	/* Perform the URL decoding */` |
|     95 | 9543 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|     95 | 9544 | `	return PH7_OK;` |
|     56 | 9545 | `}` |
|      - | 9546 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9547 | `/* Table of the built-in functions */` |
|      - | 9548 | `/*` |
|      - | 9549 | ` * int memory_get_usage([bool $real_usage = false])` |
|      - | 9550 | ` *  Amount of memory, in bytes, currently allocated to the script through PHL's` |
|      - | 9551 | ` *  memory backend. PHL tracks the backend's real allocated bytes, so the` |
|      - | 9552 | ` *  $real_usage flag has no effect here (php's non-real figure would be smaller,` |
|      - | 9553 | ` *  reflecting Zend's emalloc bookkeeping — recorded divergence).` |
|      - | 9554 | ` */` |
|    ! 0 | 9555 | `static int PH7_builtin_memory_get_usage(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 9556 | `{` |
|    ! 0 | 9557 | `	SXUNUSED(nArg);` |
|    ! 0 | 9558 | `	SXUNUSED(apArg);` |
|    ! 0 | 9559 | `	ph7_result_int64(pCtx,(ph7_int64)pCtx->pVm->sAllocator.nMemUsed);` |
|    ! 0 | 9560 | `	return PH7_OK;` |
|    ! 0 | 9561 | `}` |
|      - | 9562 | `/*` |
|      - | 9563 | ` * int memory_get_peak_usage([bool $real_usage = false])` |
|      - | 9564 | ` *  High-water mark of memory_get_usage() over the script's lifetime.` |
|      - | 9565 | ` */` |
|      4 | 9566 | `static int PH7_builtin_memory_get_peak_usage(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9567 | `{` |
|      2 | 9568 | `	SXUNUSED(nArg);` |
|      2 | 9569 | `	SXUNUSED(apArg);` |
|      5 | 9570 | `	ph7_result_int64(pCtx,(ph7_int64)pCtx->pVm->sAllocator.nMemPeak);` |
|      5 | 9571 | `	return PH7_OK;` |
|      1 | 9572 | `}` |
|      - | 9573 | `/*` |
|      - | 9574 | ` * void memory_reset_peak_usage()` |
|      - | 9575 | ` *  Reset the peak memory usage (memory_get_peak_usage) back to the current` |
|      - | 9576 | ` *  live usage — php 8.2. Frameworks call it between tests to measure per-test` |
|      - | 9577 | ` *  peaks.` |
|      - | 9578 | ` */` |
|      4 | 9579 | `static int PH7_builtin_memory_reset_peak_usage(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9580 | `{` |
|      2 | 9581 | `	SXUNUSED(nArg);` |
|      2 | 9582 | `	SXUNUSED(apArg);` |
|      5 | 9583 | `	pCtx->pVm->sAllocator.nMemPeak = pCtx->pVm->sAllocator.nMemUsed;` |
|      5 | 9584 | `	return PH7_OK;` |
|      1 | 9585 | `}` |
|      - | 9586 | `/*` |
|      - | 9587 | ` * PHL frees values by reference count as they go out of scope, so there is no` |
|      - | 9588 | ` * mark-and-sweep cycle collector to drive. The gc_* family is provided for` |
|      - | 9589 | ` * source compatibility (real frameworks call it around test runs): the state is` |
|      - | 9590 | ` * observational and collection is a no-op. Recorded divergence from php, whose` |
|      - | 9591 | ` * collector actually reclaims reference cycles.` |
|      - | 9592 | ` */` |
|    ! 0 | 9593 | `static int PH7_builtin_gc_enable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 9594 | `{` |
|    ! 0 | 9595 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    ! 0 | 9596 | `	pCtx->pVm->bGcEnabled = 1;` |
|    ! 0 | 9597 | `	return PH7_OK;` |
|    ! 0 | 9598 | `}` |
|    ! 0 | 9599 | `static int PH7_builtin_gc_disable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 9600 | `{` |
|    ! 0 | 9601 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    ! 0 | 9602 | `	pCtx->pVm->bGcEnabled = 0;` |
|    ! 0 | 9603 | `	return PH7_OK;` |
|    ! 0 | 9604 | `}` |
|    ! 0 | 9605 | `static int PH7_builtin_gc_enabled(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 9606 | `{` |
|    ! 0 | 9607 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    ! 0 | 9608 | `	ph7_result_bool(pCtx,pCtx->pVm->bGcEnabled);` |
|    ! 0 | 9609 | `	return PH7_OK;` |
|    ! 0 | 9610 | `}` |
|    ! 0 | 9611 | `static int PH7_builtin_gc_collect_cycles(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 9612 | `{` |
|      - | 9613 | `	/* No cycle collector: nothing to reclaim. Returns the count collected (0). */` |
|    ! 0 | 9614 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    ! 0 | 9615 | `	ph7_result_int(pCtx,0);` |
|    ! 0 | 9616 | `	return PH7_OK;` |
|    ! 0 | 9617 | `}` |
|    ! 0 | 9618 | `static int PH7_builtin_gc_mem_caches(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 9619 | `{` |
|    ! 0 | 9620 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    ! 0 | 9621 | `	ph7_result_int(pCtx,0);` |
|    ! 0 | 9622 | `	return PH7_OK;` |
|    ! 0 | 9623 | `}` |
|      - | 9624 | `/*` |
|      - | 9625 | ` * array gc_status(void)` |
|      - | 9626 | ` *  php 8.3 shape. PHL never runs a collection, so every counter is zero and the` |
|      - | 9627 | ` *  timing fields are 0.0; 'running' reflects gc_enable()/gc_disable().` |
|      - | 9628 | ` */` |
|    ! 0 | 9629 | `static int PH7_builtin_gc_status(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    ! 0 | 9630 | `{` |
|      - | 9631 | `	ph7_value *pArray,*pVal;` |
|    ! 0 | 9632 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    ! 0 | 9633 | `	pArray = ph7_context_new_array(pCtx);` |
|    ! 0 | 9634 | `	pVal = ph7_context_new_scalar(pCtx);` |
|    ! 0 | 9635 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|    ! 0 | 9636 | `		ph7_result_null(pCtx);` |
|    ! 0 | 9637 | `		return PH7_OK;` |
|      - | 9638 | `	}` |
|      - | 9639 | `	/* Key order matches php 8.3's gc_status(). */` |
|    ! 0 | 9640 | `	ph7_value_bool(pVal,pCtx->pVm->bGcEnabled); ph7_array_add_strkey_elem(pArray,"running",pVal);` |
|    ! 0 | 9641 | `	ph7_value_bool(pVal,0);      ph7_array_add_strkey_elem(pArray,"protected",pVal);` |
|    ! 0 | 9642 | `	ph7_value_bool(pVal,0);      ph7_array_add_strkey_elem(pArray,"full",pVal);` |
|    ! 0 | 9643 | `	ph7_value_int(pVal,0);       ph7_array_add_strkey_elem(pArray,"runs",pVal);` |
|    ! 0 | 9644 | `	ph7_value_int(pVal,0);       ph7_array_add_strkey_elem(pArray,"collected",pVal);` |
|    ! 0 | 9645 | `	ph7_value_int(pVal,1000);    ph7_array_add_strkey_elem(pArray,"threshold",pVal);` |
|    ! 0 | 9646 | `	ph7_value_int(pVal,0);       ph7_array_add_strkey_elem(pArray,"buffer_size",pVal);` |
|    ! 0 | 9647 | `	ph7_value_int(pVal,0);       ph7_array_add_strkey_elem(pArray,"roots",pVal);` |
|    ! 0 | 9648 | `	ph7_value_double(pVal,0.0);  ph7_array_add_strkey_elem(pArray,"application_time",pVal);` |
|    ! 0 | 9649 | `	ph7_value_double(pVal,0.0);  ph7_array_add_strkey_elem(pArray,"collector_time",pVal);` |
|    ! 0 | 9650 | `	ph7_value_double(pVal,0.0);  ph7_array_add_strkey_elem(pArray,"destructor_time",pVal);` |
|    ! 0 | 9651 | `	ph7_value_double(pVal,0.0);  ph7_array_add_strkey_elem(pArray,"free_time",pVal);` |
|    ! 0 | 9652 | `	ph7_context_release_value(pCtx,pVal);` |
|    ! 0 | 9653 | `	ph7_result_value(pCtx,pArray);` |
|    ! 0 | 9654 | `	return PH7_OK;` |
|    ! 0 | 9655 | `}` |
|      - | 9656 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 9657 | `	{ "memory_get_usage"     , PH7_builtin_memory_get_usage      },` |
|      - | 9658 | `	{ "memory_get_peak_usage", PH7_builtin_memory_get_peak_usage },` |
|      - | 9659 | `	{ "memory_reset_peak_usage", PH7_builtin_memory_reset_peak_usage },` |
|      - | 9660 | `	{ "gc_enable"            , PH7_builtin_gc_enable             },` |
|      - | 9661 | `	{ "gc_disable"           , PH7_builtin_gc_disable            },` |
|      - | 9662 | `	{ "gc_enabled"           , PH7_builtin_gc_enabled            },` |
|      - | 9663 | `	{ "gc_collect_cycles"    , PH7_builtin_gc_collect_cycles     },` |
|      - | 9664 | `	{ "gc_mem_caches"        , PH7_builtin_gc_mem_caches         },` |
|      - | 9665 | `	{ "gc_status"            , PH7_builtin_gc_status             },` |
|      - | 9666 | `	   /* Variable handling functions */` |
|      - | 9667 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 9668 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 9669 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 9670 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 9671 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 9672 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 9673 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 9674 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 9675 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 9676 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 9677 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 9678 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 9679 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 9680 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 9681 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 9682 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 9683 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 9684 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 9685 | `	{ "boolval"    , PH7_builtin_boolval     },` |
|      - | 9686 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 9687 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9688 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 9689 | `	   /* Math functions */` |
|      - | 9690 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 9691 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 9692 | `	{ "acosh" ,   PH7_builtin_acosh        },` |
|      - | 9693 | `	{ "asinh" ,   PH7_builtin_asinh        },` |
|      - | 9694 | `	{ "atanh" ,   PH7_builtin_atanh        },` |
|      - | 9695 | `	{ "expm1" ,   PH7_builtin_expm1        },` |
|      - | 9696 | `	{ "log1p" ,   PH7_builtin_log1p        },` |
|      - | 9697 | `	{ "deg2rad" , PH7_builtin_deg2rad      },` |
|      - | 9698 | `	{ "rad2deg" , PH7_builtin_rad2deg      },` |
|      - | 9699 | `	{ "fpow" ,    PH7_builtin_fpow         },` |
|      - | 9700 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 9701 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 9702 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 9703 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 9704 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 9705 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 9706 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 9707 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 9708 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 9709 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 9710 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 9711 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 9712 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 9713 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 9714 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 9715 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 9716 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 9717 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 9718 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 9719 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 9720 | `	{ "round",    PH7_builtin_round        },` |
|      - | 9721 | `	{ "intdiv",   PH7_builtin_intdiv       },` |
|      - | 9722 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 9723 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 9724 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 9725 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 9726 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 9727 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 9728 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 9729 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 9730 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9731 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9732 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 9733 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9734 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9735 | `	   /* String handling functions */` |
|      - | 9736 |  |
|      - | 9737 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 9738 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 9739 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 9740 | `	{ "substr_replace",  PH7_builtin_substr_replace },` |
|      - | 9741 | `	{ "levenshtein",     PH7_builtin_levenshtein },` |
|      - | 9742 | `	{ "similar_text",    PH7_builtin_similar_text },` |
|      - | 9743 | `	{ "str_word_count",  PH7_builtin_str_word_count },` |
|      - | 9744 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 9745 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 9746 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 9747 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 9748 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 9749 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 9750 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 9751 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 9752 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 9753 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 9754 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 9755 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 9756 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 9757 | `	{ "strnatcmp"  , PH7_builtin_strnatcmp  },` |
|      - | 9758 | `	{ "strnatcasecmp", PH7_builtin_strnatcmp },` |
|      - | 9759 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 9760 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 9761 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 9762 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 9763 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 9764 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 9765 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 9766 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 9767 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 9768 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 9769 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 9770 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 9771 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 9772 | `	{ "mb_strtolower",PH7_builtin_mb_case_f }, /* UTF-8 only (builtin_mb.c) */` |
|      - | 9773 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 9774 | `	{ "mb_strtoupper",PH7_builtin_mb_case_f }, /* UTF-8 only (builtin_mb.c) */` |
|      - | 9775 | `	{ "mb_strlen",    PH7_builtin_mb_strlen_f },` |
|      - | 9776 | `	{ "mb_substr",    PH7_builtin_mb_substr_f },` |
|      - | 9777 | `	{ "mb_convert_case", PH7_builtin_mb_convert_case_f },` |
|      - | 9778 | `	{ "mb_strpos",    PH7_builtin_mb_strpos_f },` |
|      - | 9779 | `	{ "mb_stripos",   PH7_builtin_mb_strpos_f },` |
|      - | 9780 | `	{ "mb_strrpos",   PH7_builtin_mb_strpos_f },` |
|      - | 9781 | `	{ "mb_str_split", PH7_builtin_mb_str_split_f },` |
|      - | 9782 | `	{ "mb_internal_encoding", PH7_builtin_mb_internal_encoding_f },` |
|      - | 9783 | `	{ "mb_check_encoding",    PH7_builtin_mb_check_encoding_f },` |
|      - | 9784 | `	{ "mb_strwidth",  PH7_builtin_mb_strwidth_f },` |
|      - | 9785 | `	{ "mb_chr",       PH7_builtin_mb_chr_f   },` |
|      - | 9786 | `	{ "mb_ord",       PH7_builtin_mb_ord_f   },` |
|      - | 9787 | `	{ "mb_detect_encoding", PH7_builtin_mb_detect_encoding_f },` |
|      - | 9788 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 9789 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 9790 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 9791 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 9792 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 9793 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 9794 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 9795 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 9796 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 9797 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 9798 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 9799 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 9800 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 9801 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 9802 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 9803 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 9804 | `	{ "str_contains", PH7_builtin_str_contains },` |
|      - | 9805 | `	{ "str_starts_with", PH7_builtin_str_starts_with },` |
|      - | 9806 | `	{ "str_ends_with", PH7_builtin_str_ends_with },` |
|      - | 9807 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 9808 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9809 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9810 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 9811 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 9812 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 9813 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 9814 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9815 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9816 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 9817 |  |
|      - | 9818 |  |
|      - | 9819 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 9820 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 9821 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 9822 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 9823 | `	{ "hash",         PH7_builtin_hash      },` |
|      - | 9824 | `	{ "hash_hmac",    PH7_builtin_hash_hmac },` |
|      - | 9825 | `	{ "hash_equals",  PH7_builtin_hash_equals },` |
|      - | 9826 | `	{ "hash_algos",   PH7_builtin_hash_algos },` |
|      - | 9827 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 9828 | `	{ "password_hash",         PH7_builtin_password_hash },` |
|      - | 9829 | `	{ "password_verify",       PH7_builtin_password_verify },` |
|      - | 9830 | `	{ "password_get_info",     PH7_builtin_password_get_info },` |
|      - | 9831 | `	{ "password_needs_rehash", PH7_builtin_password_needs_rehash },` |
|      - | 9832 | `	{ "filter_var",            PH7_builtin_filter_var },` |
|      - | 9833 | `	{ "filter_input",          PH7_builtin_filter_input },` |
|      - | 9834 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9835 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9836 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 9837 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 9838 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9839 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9840 |  |
|      - | 9841 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 9842 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 9843 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 9844 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 9845 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 9846 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 9847 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 9848 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 9849 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 9850 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 9851 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 9852 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 9853 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9854 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9855 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 9856 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9857 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9858 |  |
|      - | 9859 | `	         /* Ctype functions */` |
|      - | 9860 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 9861 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 9862 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 9863 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 9864 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 9865 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 9866 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 9867 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 9868 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 9869 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 9870 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 9871 | `	         /* Time functions */` |
|      - | 9872 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 9873 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 9874 | `	{ "hrtime",      PH7_builtin_hrtime       },` |
|      - | 9875 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 9876 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 9877 | `	{ "date",        PH7_builtin_date         },` |
|      - | 9878 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 9879 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 9880 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 9881 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 9882 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 9883 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 9884 | `	{ "date_default_timezone_get", PH7_builtin_date_default_timezone_get },` |
|      - | 9885 | `	{ "date_default_timezone_set", PH7_builtin_date_default_timezone_set },` |
|      - | 9886 | `	        /* URL functions */` |
|      - | 9887 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 9888 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 9889 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 9890 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 9891 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 9892 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 9893 | `	{ "rawurlencode", PH7_builtin_rawurlencode },` |
|      - | 9894 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 9895 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9896 | `};` |
|      - | 9897 | `/*` |
|      - | 9898 | ` * Register the built-in functions defined above,the array functions` |
|      - | 9899 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 9900 | ` */` |
|   3368 | 9901 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      5 | 9902 | `{` |
|      - | 9903 | `	sxu32 n;` |
| 697181 | 9904 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 693813 | 9905 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 346909 | 9906 | `	}` |
|      - | 9907 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   3373 | 9908 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 9909 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   3373 | 9910 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   3373 | 9911 | `}` |
|      - | 9912 |  |
