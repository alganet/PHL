# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 311/399 lines (77.94%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|       - |    7 | `/* filter_var(FILTER_VALIDATE_FLOAT) parses with libc strtod directly because it` |
|       - |    8 | ` * needs errno==ERANGE to reject out-of-range magnitudes; SyStrToReal (also` |
|       - |    9 | ` * strtod-backed nowadays) exposes no range-error signal. */` |
|       - |   10 | `#include <stdlib.h>  /* strtod */` |
|       - |   11 | `#include <math.h>    /* HUGE_VAL */` |
|       - |   12 | `#include <errno.h>   /* ERANGE (strtod range-error signal) */` |
|       - |   13 | `#include <stdio.h>   /* snprintf (printf-family float conversions — correctly` |
|       - |   14 | `                      * rounded digits like php's zend_dtoa; see PH7_InputFormat) */` |
|       - |   15 | ``/* Shared ZPP helper for `int` parameters — defined OUTSIDE the`` |
|       - |   16 | ` * PH7_DISABLE_BUILTIN_FUNC guard because hashmap.c (array_slice) and` |
|       - |   17 | ` * builtin_math.c (intdiv) call it and both compile in the tiny build. */` |
|  766123 |   18 | `PH7_PRIVATE sxi32 PH7_IntArgResolve(` |
|       - |   19 | `	ph7_context *pCtx,` |
|       - |   20 | `	ph7_value *pArg,` |
|       - |   21 | `	const char *zFunc,` |
|       - |   22 | `	int iArgNum,` |
|       - |   23 | `	const char *zParamName,` |
|       - |   24 | `	const char *zTypeStr,` |
|       - |   25 | `	sxi64 *pOut` |
|       5 |   26 | `){` |
|  766128 |   27 | `	if( ph7_value_is_null(pArg) ){` |
|       - |   28 | `		/* php only DEPRECATES passing null to a non-nullable internal param; PHL` |
|       - |   29 | `		 * targets php's non-deprecated surface and rejects it with the TypeError` |
|       - |   30 | `		 * php will eventually raise. */` |
|     ! 0 |   31 | `		return PH7_VmThrowException(pCtx,` |
|       - |   32 | `			"TypeError",` |
|       - |   33 | `			"%s(): Argument #%d (%s) must be of type %s, null given",` |
|     ! 0 |   34 | `			zFunc,iArgNum,zParamName,zTypeStr` |
|       - |   35 | `			);` |
|       - |   36 | `	}` |
|  766128 |   37 | `	if( ph7_value_is_float(pArg) ){` |
|      24 |   38 | `		double dVal = ph7_value_to_double(pArg);` |
|       - |   39 | `		sxi64 iVal;` |
|       - |   40 | `		/* php: NAN/INF/out-of-int64-range floats fail ZPP outright */` |
|      24 |   41 | `		if( dVal != dVal \|\| dVal >= 9223372036854775808.0 \|\| dVal < -9223372036854775808.0 ){` |
|     ! 0 |   42 | `			return PH7_VmThrowException(pCtx,` |
|       - |   43 | `				"TypeError",` |
|       - |   44 | `				"%s(): Argument #%d (%s) must be of type %s, float given",` |
|     ! 0 |   45 | `				zFunc,iArgNum,zParamName,zTypeStr` |
|       - |   46 | `				);` |
|       - |   47 | `		}` |
|      24 |   48 | `		iVal = (sxi64)dVal;` |
|      24 |   49 | `		if( (double)iVal != dVal ){` |
|       - |   50 | `			/* php DEPRECATES a lossy float->int; PHL rejects it (the value is not` |
|       - |   51 | `			 * representable as int). */` |
|     ! 0 |   52 | `			return PH7_VmThrowException(pCtx,` |
|       - |   53 | `				"TypeError",` |
|       - |   54 | `				"%s(): Argument #%d (%s) must be of type %s, float given",` |
|     ! 0 |   55 | `				zFunc,iArgNum,zParamName,zTypeStr` |
|       - |   56 | `				);` |
|       - |   57 | `		}` |
|      24 |   58 | `		*pOut = iVal;` |
|      24 |   59 | `		return PH7_OK;` |
|       - |   60 | `	}` |
|  766106 |   61 | `	if( ph7_value_is_string(pArg) ){` |
|       - |   62 | `		const char *zNum;` |
|       - |   63 | `		int nSlen;` |
|      58 |   64 | `		int i,bFloat = 0;` |
|      58 |   65 | `		if( !PH7_MemObjStringIsNumeric(pArg) ){` |
|      31 |   66 | `			return PH7_VmThrowException(pCtx,` |
|       - |   67 | `				"TypeError",` |
|       - |   68 | `				"%s(): Argument #%d (%s) must be of type %s, string given",` |
|      10 |   69 | `				zFunc,iArgNum,zParamName,zTypeStr` |
|       - |   70 | `				);` |
|       - |   71 | `		}` |
|      38 |   72 | `		zNum = ph7_value_to_string(pArg,&nSlen);` |
|      88 |   73 | `		for( i = 0 ; i < nSlen ; i++ ){` |
|      56 |   74 | `			if( zNum[i] == '.' \|\| zNum[i] == 'e' \|\| zNum[i] == 'E' ){` |
|       5 |   75 | `				bFloat = 1;` |
|       5 |   76 | `				break;` |
|       - |   77 | `			}` |
|      27 |   78 | `		}` |
|      38 |   79 | `		if( bFloat ){` |
|       5 |   80 | `			double dVal = 0;` |
|       - |   81 | `			sxi64 iVal;` |
|       5 |   82 | `			SyStrToReal(zNum,(sxu32)nSlen,(void *)&dVal,0);` |
|       5 |   83 | `			if( dVal != dVal \|\| dVal >= 9223372036854775808.0 \|\| dVal < -9223372036854775808.0 ){` |
|     ! 0 |   84 | `				return PH7_VmThrowException(pCtx,` |
|       - |   85 | `					"TypeError",` |
|       - |   86 | `					"%s(): Argument #%d (%s) must be of type %s, string given",` |
|     ! 0 |   87 | `					zFunc,iArgNum,zParamName,zTypeStr` |
|       - |   88 | `					);` |
|       - |   89 | `			}` |
|       5 |   90 | `			iVal = (sxi64)dVal;` |
|       5 |   91 | `			if( (double)iVal != dVal ){` |
|       - |   92 | `				/* php DEPRECATES a lossy float-string->int; PHL rejects it. */` |
|     ! 0 |   93 | `				return PH7_VmThrowException(pCtx,` |
|       - |   94 | `					"TypeError",` |
|       - |   95 | `					"%s(): Argument #%d (%s) must be of type %s, string given",` |
|     ! 0 |   96 | `					zFunc,iArgNum,zParamName,zTypeStr` |
|       - |   97 | `					);` |
|       - |   98 | `			}` |
|       5 |   99 | `			*pOut = iVal;` |
|       5 |  100 | `			return PH7_OK;` |
|       - |  101 | `		}` |
|      34 |  102 | `		*pOut = ph7_value_to_int64(pArg);` |
|      34 |  103 | `		return PH7_OK;` |
|       - |  104 | `	}` |
|  766050 |  105 | `	if( !ph7_value_is_int(pArg) && !ph7_value_is_bool(pArg) ){` |
|       - |  106 | `		/* Arrays, resources and objects: php names the class for objects */` |
|     ! 0 |  107 | `		const char *zType = ph7_type_name(pArg);` |
|     ! 0 |  108 | `		if( ph7_value_is_object(pArg) ){` |
|     ! 0 |  109 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     ! 0 |  110 | `			if( pInst && pInst->pClass ){` |
|     ! 0 |  111 | `				zType = SyStringData(&pInst->pClass->sName);` |
|     ! 0 |  112 | `			}` |
|     ! 0 |  113 | `		}` |
|     ! 0 |  114 | `		return PH7_VmThrowException(pCtx,` |
|       - |  115 | `			"TypeError",` |
|       - |  116 | `			"%s(): Argument #%d (%s) must be of type %s, %s given",` |
|     ! 0 |  117 | `			zFunc,iArgNum,zParamName,zTypeStr,zType` |
|       - |  118 | `			);` |
|       - |  119 | `	}` |
|  766050 |  120 | `	*pOut = ph7_value_to_int64(pArg);` |
|  766050 |  121 | `	return PH7_OK;` |
|  383289 |  122 | `}` |
|       - |  123 |  |
|       - |  124 | `/* This file implement built-in 'foreign' functions for the PH7 engine */` |
|       - |  125 | `/*` |
|       - |  126 | ` * Section:` |
|       - |  127 | ` *    Variable handling Functions.` |
|       - |  128 | ` * Status:` |
|       - |  129 | ` *    Stable.` |
|       - |  130 | ` */` |
|       - |  131 | `/*` |
|       - |  132 | ` * bool is_bool($var)` |
|       - |  133 | ` *  Finds out whether a variable is a boolean.` |
|       - |  134 | ` * Parameters` |
|       - |  135 | ` *   $var: The variable being evaluated.` |
|       - |  136 | ` * Return` |
|       - |  137 | ` *  TRUE if var is a boolean. False otherwise.` |
|       - |  138 | ` */` |
|     224 |  139 | `static int PH7_builtin_is_bool(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  140 | `{` |
|     226 |  141 | `	int res = 0; /* Assume false by default */` |
|     226 |  142 | `	if( nArg > 0 ){` |
|     226 |  143 | `		res = ph7_value_is_bool(apArg[0]);` |
|     112 |  144 | `	}` |
|       - |  145 | `	/* Query result */` |
|     226 |  146 | `	ph7_result_bool(pCtx,res);` |
|     226 |  147 | `	return PH7_OK;` |
|       2 |  148 | `}` |
|       - |  149 | `/*` |
|       - |  150 | ` * bool is_float($var)` |
|       - |  151 | ` * bool is_double($var)` |
|       - |  152 | ` *  Finds out whether a variable is a float.` |
|       - |  153 | ` * Parameters` |
|       - |  154 | ` *   $var: The variable being evaluated.` |
|       - |  155 | ` * Return` |
|       - |  156 | ` *  TRUE if var is a float. False otherwise.` |
|       - |  157 | ` */` |
|     412 |  158 | `static int PH7_builtin_is_float(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  159 | `{` |
|     413 |  160 | `	int res = 0; /* Assume false by default */` |
|     413 |  161 | `	if( nArg > 0 ){` |
|     413 |  162 | `		res = ph7_value_is_float(apArg[0]);` |
|     206 |  163 | `	}` |
|       - |  164 | `	/* Query result */` |
|     413 |  165 | `	ph7_result_bool(pCtx,res);` |
|     413 |  166 | `	return PH7_OK;` |
|       1 |  167 | `}` |
|       - |  168 | `/*` |
|       - |  169 | ` * bool is_int($var)` |
|       - |  170 | ` * bool is_integer($var)` |
|       - |  171 | ` * bool is_long($var)` |
|       - |  172 | ` *  Finds out whether a variable is an integer.` |
|       - |  173 | ` * Parameters` |
|       - |  174 | ` *   $var: The variable being evaluated.` |
|       - |  175 | ` * Return` |
|       - |  176 | ` *  TRUE if var is an integer. False otherwise.` |
|       - |  177 | ` */` |
|     748 |  178 | `static int PH7_builtin_is_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  179 | `{` |
|     752 |  180 | `	int res = 0; /* Assume false by default */` |
|     752 |  181 | `	if( nArg > 0 ){` |
|       - |  182 | `		/* Strict PHP identity: a float is never an int, even when it holds an` |
|       - |  183 | `		 * integer value (1.0). An integer-valued real carries both MEMOBJ_INT` |
|       - |  184 | `		 * (cached) and MEMOBJ_REAL, so REAL must be excluded here. */` |
|     752 |  185 | `		res = ph7_value_is_int(apArg[0]) && !ph7_value_is_float(apArg[0]);` |
|     372 |  186 | `	}` |
|       - |  187 | `	/* Query result */` |
|     752 |  188 | `	ph7_result_bool(pCtx,res);` |
|     752 |  189 | `	return PH7_OK;` |
|       4 |  190 | `}` |
|       - |  191 | `/*` |
|       - |  192 | ` * bool is_string($var)` |
|       - |  193 | ` *  Finds out whether a variable is a string.` |
|       - |  194 | ` * Parameters` |
|       - |  195 | ` *   $var: The variable being evaluated.` |
|       - |  196 | ` * Return` |
|       - |  197 | ` *  TRUE if var is string. False otherwise.` |
|       - |  198 | ` */` |
|     542 |  199 | `static int PH7_builtin_is_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  200 | `{` |
|     546 |  201 | `	int res = 0; /* Assume false by default */` |
|     546 |  202 | `	if( nArg > 0 ){` |
|     546 |  203 | `		res = ph7_value_is_string(apArg[0]);` |
|     271 |  204 | `	}` |
|       - |  205 | `	/* Query result */` |
|     546 |  206 | `	ph7_result_bool(pCtx,res);` |
|     546 |  207 | `	return PH7_OK;` |
|       4 |  208 | `}` |
|       - |  209 | `/*` |
|       - |  210 | ` * bool is_null($var)` |
|       - |  211 | ` *  Finds out whether a variable is NULL.` |
|       - |  212 | ` * Parameters` |
|       - |  213 | ` *   $var: The variable being evaluated.` |
|       - |  214 | ` * Return` |
|       - |  215 | ` *  TRUE if var is NULL. False otherwise.` |
|       - |  216 | ` */` |
|      82 |  217 | `static int PH7_builtin_is_null(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  218 | `{` |
|      85 |  219 | `	int res = 0; /* Assume false by default */` |
|      85 |  220 | `	if( nArg > 0 ){` |
|      85 |  221 | `		res = ph7_value_is_null(apArg[0]);` |
|      41 |  222 | `	}` |
|       - |  223 | `	/* Query result */` |
|      85 |  224 | `	ph7_result_bool(pCtx,res);` |
|      85 |  225 | `	return PH7_OK;` |
|       3 |  226 | `}` |
|       - |  227 | `/*` |
|       - |  228 | ` * bool is_numeric($var)` |
|       - |  229 | ` *  Find out whether a variable is NULL.` |
|       - |  230 | ` * Parameters` |
|       - |  231 | ` *  $var: The variable being evaluated.` |
|       - |  232 | ` * Return` |
|       - |  233 | ` *  True if var is numeric. False otherwise.` |
|       - |  234 | ` */` |
|      94 |  235 | `static int PH7_builtin_is_numeric(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  236 | `{` |
|      99 |  237 | `	int res = 0; /* Assume false by default */` |
|      99 |  238 | `	if( nArg > 0 ){` |
|       - |  239 | `		/* Strict PHP semantics: only int/float and numeric strings are numeric.` |
|       - |  240 | `		 * PHL's lenient helper also reports booleans as numeric (they coerce for` |
|       - |  241 | `		 * arithmetic), but php's is_numeric() rejects true/false, so exclude` |
|       - |  242 | `		 * MEMOBJ_BOOL here. */` |
|      99 |  243 | `		res = ph7_value_is_numeric(apArg[0]) && !ph7_value_is_bool(apArg[0]);` |
|      47 |  244 | `	}` |
|       - |  245 | `	/* Query result */` |
|      99 |  246 | `	ph7_result_bool(pCtx,res);` |
|      99 |  247 | `	return PH7_OK;` |
|       5 |  248 | `}` |
|       - |  249 | `/*` |
|       - |  250 | ` * bool is_scalar($var)` |
|       - |  251 | ` *  Find out whether a variable is a scalar.` |
|       - |  252 | ` * Parameters` |
|       - |  253 | ` *  $var: The variable being evaluated.` |
|       - |  254 | ` * Return` |
|       - |  255 | ` *  True if var is scalar. False otherwise.` |
|       - |  256 | ` */` |
|      30 |  257 | `static int PH7_builtin_is_scalar(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  258 | `{` |
|      31 |  259 | `	int res = 0; /* Assume false by default */` |
|      31 |  260 | `	if( nArg > 0 ){` |
|       - |  261 | `		/* Strict PHP semantics: scalars are int/float/string/bool. PHL's` |
|       - |  262 | `		 * MEMOBJ_SCALAR bucket also includes NULL, but php's is_scalar(null) is` |
|       - |  263 | `		 * false, so exclude the NULL case. */` |
|      31 |  264 | `		res = ph7_value_is_scalar(apArg[0]) && !ph7_value_is_null(apArg[0]);` |
|      15 |  265 | `	}` |
|       - |  266 | `	/* Query result */` |
|      31 |  267 | `	ph7_result_bool(pCtx,res);` |
|      31 |  268 | `	return PH7_OK;` |
|       1 |  269 | `}` |
|       - |  270 | `/*` |
|       - |  271 | ` * bool is_array($var)` |
|       - |  272 | ` *  Find out whether a variable is an array.` |
|       - |  273 | ` * Parameters` |
|       - |  274 | ` *  $var: The variable being evaluated.` |
|       - |  275 | ` * Return` |
|       - |  276 | ` *  True if var is an array. False otherwise.` |
|       - |  277 | ` */` |
|     350 |  278 | `static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  279 | `{` |
|     354 |  280 | `	int res = 0; /* Assume false by default */` |
|     354 |  281 | `	if( nArg > 0 ){` |
|     354 |  282 | `		res = ph7_value_is_array(apArg[0]);` |
|     175 |  283 | `	}` |
|       - |  284 | `	/* Query result */` |
|     354 |  285 | `	ph7_result_bool(pCtx,res);` |
|     354 |  286 | `	return PH7_OK;` |
|       4 |  287 | `}` |
|       - |  288 | `/*` |
|       - |  289 | ` * bool is_object($var)` |
|       - |  290 | ` *  Find out whether a variable is an object.` |
|       - |  291 | ` * Parameters` |
|       - |  292 | ` *  $var: The variable being evaluated.` |
|       - |  293 | ` * Return` |
|       - |  294 | ` *  True if var is an object. False otherwise.` |
|       - |  295 | ` */` |
|     258 |  296 | `static int PH7_builtin_is_object(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  297 | `{` |
|     263 |  298 | `	int res = 0; /* Assume false by default */` |
|     263 |  299 | `	if( nArg > 0 ){` |
|     263 |  300 | `		res = ph7_value_is_object(apArg[0]);` |
|     129 |  301 | `	}` |
|       - |  302 | `	/* Query result */` |
|     263 |  303 | `	ph7_result_bool(pCtx,res);` |
|     263 |  304 | `	return PH7_OK;` |
|       5 |  305 | `}` |
|       - |  306 | `/*` |
|       - |  307 | ` * bool is_resource($var)` |
|       - |  308 | ` *  Find out whether a variable is a resource.` |
|       - |  309 | ` * Parameters` |
|       - |  310 | ` *  $var: The variable being evaluated.` |
|       - |  311 | ` * Return` |
|       - |  312 | ` *  True if a resource. False otherwise.` |
|       - |  313 | ` */` |
|     194 |  314 | `static int PH7_builtin_is_resource(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  315 | `{` |
|     199 |  316 | `	int res = 0; /* Assume false by default */` |
|     199 |  317 | `	if( nArg > 0 && ph7_value_is_resource(apArg[0]) ){` |
|       - |  318 | `		/* A handle closed via fclose()/closedir()/pclose() is no longer a` |
|       - |  319 | `		 * live resource — php's is_resource() returns false for it. */` |
|     157 |  320 | `		res = !PH7_VfsResourceIsClosed(apArg[0]->x.pOther);` |
|      76 |  321 | `	}` |
|     199 |  322 | `	ph7_result_bool(pCtx,res);` |
|     199 |  323 | `	return PH7_OK;` |
|       5 |  324 | `}` |
|       - |  325 | `/*` |
|       - |  326 | ` * float floatval($var)` |
|       - |  327 | ` *  Get float value of a variable.` |
|       - |  328 | ` * Parameter` |
|       - |  329 | ` *  $var: The variable being processed.` |
|       - |  330 | ` * Return` |
|       - |  331 | ` *  the float value of a variable.` |
|       - |  332 | ` */` |
|       4 |  333 | `static int PH7_builtin_floatval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  334 | `{` |
|       5 |  335 | `	if( nArg < 1 ){` |
|       - |  336 | `		/* return 0.0 */` |
|     ! 0 |  337 | `		ph7_result_double(pCtx,0);` |
|     ! 0 |  338 | `	}else{` |
|       - |  339 | `		double dval;` |
|       - |  340 | `		/* Perform the cast */` |
|       5 |  341 | `		dval = ph7_value_to_double(apArg[0]);` |
|       5 |  342 | `		ph7_result_double(pCtx,dval);` |
|       - |  343 | `	}` |
|       5 |  344 | `	return PH7_OK;` |
|       1 |  345 | `}` |
|       - |  346 | `/*` |
|       - |  347 | ` * One C strtol() run, which is what php's intval() calls (ZEND_STRTOL in` |
|       - |  348 | ` * ext/standard/type.c). The rules are strtol's, not php's own numeric-string` |
|       - |  349 | ` * ones, and every one of them is observable:` |
|       - |  350 | ` *  - leading whitespace, then at most ONE sign;` |
|       - |  351 | ` *  - base 16 skips an optional "0x"/"0X"; base 0 PICKS the base from the same` |
|       - |  352 | ` *    prefix ("0x" -> 16, a leading "0" -> 8, otherwise 10);` |
|       - |  353 | ` *  - the scan stops at the first byte the base cannot spell, so "12ag" in base` |
|       - |  354 | ` *    16 is 0x12a and "0x0x1" is 0;` |
|       - |  355 | ` *  - a base outside 2..36 makes strtol answer 0 -- php raises nothing for it;` |
|       - |  356 | ` *  - the result SATURATES at PHP_INT_MAX/PHP_INT_MIN instead of wrapping.` |
|       - |  357 | `` * `iPreSign` is the sign php pastes in FRONT of the string it hands over (see`` |
|       - |  358 | ` * IntvalStrToInt64): a sign already consumed, so neither whitespace nor a` |
|       - |  359 | ` * second sign may follow it.` |
|       - |  360 | ` */` |
|     178 |  361 | `static sxi64 IntvalStrtol(int iPreSign,const char *zIn,int nLen,int iBase)` |
|       1 |  362 | `{` |
|     179 |  363 | `	sxu64 uLimit,uCutoff,uAcc = 0;` |
|     179 |  364 | `	int iCutlim,iSign = 1,bAny = 0,bOvf = 0;` |
|     179 |  365 | `	int i = 0;` |
|     179 |  366 | `	if( iPreSign ){` |
|      15 |  367 | `		iSign = (iPreSign == '-') ? -1 : 1;` |
|       8 |  368 | `	}else{` |
|     187 |  369 | `		while( i < nLen && SyisSpace((unsigned char)zIn[i]) ){` |
|      23 |  370 | `			i++;` |
|       1 |  371 | `		}` |
|     165 |  372 | `		if( i < nLen && (zIn[i] == '-' \|\| zIn[i] == '+') ){` |
|      33 |  373 | `			iSign = (zIn[i] == '-') ? -1 : 1;` |
|      33 |  374 | `			i++;` |
|      16 |  375 | `		}` |
|       - |  376 | `	}` |
|     178 |  377 | `	if( (iBase == 0 \|\| iBase == 16) && i + 1 < nLen` |
|     127 |  378 | `	 && zIn[i] == '0' && (zIn[i+1] == 'x' \|\| zIn[i+1] == 'X') ){` |
|      15 |  379 | `		i += 2;` |
|      15 |  380 | `		iBase = 16;` |
|     172 |  381 | `	}else if( (iBase == 0 \|\| iBase == 2) && i + 2 < nLen` |
|     114 |  382 | `	 && zIn[i] == '0' && (zIn[i+1] == 'b' \|\| zIn[i+1] == 'B')` |
|      52 |  383 | `	 && (zIn[i+2] == '0' \|\| zIn[i+2] == '1') ){` |
|       - |  384 | `		/* The conversion accepts a binary prefix of its own, on TOP of the one` |
|       - |  385 | `		 * IntvalStrToInt64 strips -- which is why intval("0b0b1",2) is 1 and a` |
|       - |  386 | `		 * THIRD prefix stops the scan: intval("0b0b0b1",2) is 0. It is also the` |
|       - |  387 | `		 * only prefix reader a base that NARROWED to 0 or 2 gets, since the` |
|       - |  388 | `		 * strip upstream reads the base at full width. */` |
|      39 |  389 | `		i += 2;` |
|      39 |  390 | `		iBase = 2;` |
|     140 |  391 | `	}else if( iBase == 0 ){` |
|      15 |  392 | `		iBase = (i < nLen && zIn[i] == '0') ? 8 : 10;` |
|       7 |  393 | `	}` |
|     173 |  394 | `	if( iBase < 2 \|\| iBase > 36 ){` |
|       9 |  395 | `		return 0;` |
|       - |  396 | `	}` |
|       - |  397 | `	/* strtol's own overflow test: the magnitude a negative result may reach is` |
|       - |  398 | `	 * one larger than a positive one, so the cutoff is computed per sign. */` |
|     171 |  399 | `	uLimit  = (iSign < 0) ? (sxu64)SXI64_HIGH + 1 : (sxu64)SXI64_HIGH;` |
|     171 |  400 | `	uCutoff = uLimit / (sxu64)iBase;` |
|     171 |  401 | `	iCutlim = (int)(uLimit % (sxu64)iBase);` |
|     583 |  402 | `	for( ; i < nLen ; ++i ){` |
|     469 |  403 | `		int c = (unsigned char)zIn[i];` |
|     469 |  404 | `		if( c >= '0' && c <= '9' ){` |
|     269 |  405 | `			c -= '0';` |
|     335 |  406 | `		}else if( c >= 'A' && c <= 'Z' ){` |
|       5 |  407 | `			c -= 'A' - 10;` |
|     199 |  408 | `		}else if( c >= 'a' && c <= 'z' ){` |
|     183 |  409 | `			c -= 'a' - 10;` |
|      92 |  410 | `		}else{` |
|       8 |  411 | `			break;` |
|       - |  412 | `		}` |
|     455 |  413 | `		if( c >= iBase ){` |
|      43 |  414 | `			break;` |
|       - |  415 | `		}` |
|     413 |  416 | `		bAny = 1;` |
|     413 |  417 | `		if( bOvf \|\| uAcc > uCutoff \|\| (uAcc == uCutoff && c > iCutlim) ){` |
|      11 |  418 | `			bOvf = 1;   /* keep consuming digits, the answer is pinned */` |
|      11 |  419 | `			continue;` |
|       - |  420 | `		}` |
|     403 |  421 | `		uAcc = uAcc * (sxu64)iBase + (sxu64)c;` |
|     202 |  422 | `	}` |
|     171 |  423 | `	if( bOvf ){` |
|       7 |  424 | `		return (iSign < 0) ? (-(sxi64)SXI64_HIGH - 1) : (sxi64)SXI64_HIGH;` |
|       - |  425 | `	}` |
|     165 |  426 | `	if( !bAny ){` |
|      23 |  427 | `		return 0;` |
|       - |  428 | `	}` |
|     143 |  429 | `	if( iSign < 0 ){` |
|       - |  430 | `		/* uAcc may be exactly 2^63 here, which no sxi64 holds: PHP_INT_MIN is` |
|       - |  431 | `		 * its negation and negating the SIGNED value would be undefined. */` |
|      21 |  432 | `		return (uAcc == (sxu64)SXI64_HIGH + 1) ? (-(sxi64)SXI64_HIGH - 1) : -(sxi64)uAcc;` |
|       - |  433 | `	}` |
|     123 |  434 | `	return (sxi64)uAcc;` |
|      90 |  435 | `}` |
|       - |  436 | `/*` |
|       - |  437 | ` * php's intval() string path. strtol() knows "0x" but not "0b", so php strips a` |
|       - |  438 | ` * binary prefix ITSELF -- for base 2 and for base 0 -- by building a fresh` |
|       - |  439 | ` * string out of the sign it found and the bytes past the "0b", and running` |
|       - |  440 | ` * strtol over THAT. The rebuild is observable, because strtol then runs its` |
|       - |  441 | `` * whole prelude again over the remainder: `intval("0b-1",2)` is -1 and`` |
|       - |  442 | `` * `intval("0b 1",0)` is 1, while a sign BEFORE the prefix is already spent, so`` |
|       - |  443 | `` * `intval("-0b-1",2)` is 0. php hands strtol() the C string, so an embedded NUL`` |
|       - |  444 | ` * truncates: intval("12\0 34",16) is 0x12. That truncation is copied too.` |
|       - |  445 | ` */` |
|     178 |  446 | `static sxi64 IntvalStrToInt64(const char *zIn,int nLen,sxi64 iBase64,int iBase)` |
|       1 |  447 | `{` |
|     179 |  448 | `	int i = 0;` |
|       - |  449 | `	/* An embedded NUL ends the string for strtol(). */` |
|    1031 |  450 | `	while( i < nLen && zIn[i] != 0 ){` |
|     853 |  451 | `		i++;` |
|       1 |  452 | `	}` |
|     179 |  453 | `	nLen = i;` |
|     179 |  454 | `	i = 0;` |
|     189 |  455 | `	while( i < nLen && SyisSpace((unsigned char)zIn[i]) ){` |
|      11 |  456 | `		i++;` |
|       1 |  457 | `	}` |
|       - |  458 | `	/* php's own strip reads the base at FULL width, one step before the narrowing` |
|       - |  459 | `	 * cast the conversion below gets -- so base 2^32+2 does not strip here even` |
|       - |  460 | `	 * though it converts in base 2. Only when something FOLLOWS the prefix, too:` |
|       - |  461 | `	 * "0b" alone stays a base-2 zero. */` |
|     179 |  462 | `	if( (iBase64 == 0 \|\| iBase64 == 2) && nLen - i > 2 ){` |
|      83 |  463 | `		int off = (zIn[i] == '-' \|\| zIn[i] == '+') ? 1 : 0;` |
|      83 |  464 | `		if( zIn[i+off] == '0' && (zIn[i+off+1] == 'b' \|\| zIn[i+off+1] == 'B') ){` |
|      73 |  465 | `			int iPreSign = off ? (unsigned char)zIn[i] : 0;` |
|      73 |  466 | `			i += off + 2;` |
|      73 |  467 | `			return IntvalStrtol(iPreSign,&zIn[i],nLen - i,2);` |
|       - |  468 | `		}` |
|       5 |  469 | `	}` |
|     107 |  470 | `	return IntvalStrtol(0,zIn,nLen,iBase);` |
|      90 |  471 | `}` |
|       - |  472 | `/*` |
|       - |  473 | ` * int intval(mixed $value, int $base = 10)` |
|       - |  474 | ` *  Get integer value of a variable.` |
|       - |  475 | ` * Parameters` |
|       - |  476 | ` *  $value: The variable being processed.` |
|       - |  477 | ` *  $base: The base $value is written in -- read ONLY when $value is a string` |
|       - |  478 | ` *   and the base is not 10. Every other value takes the ordinary int cast and` |
|       - |  479 | ` *   ignores $base entirely, an out-of-range one included.` |
|       - |  480 | ` * Return` |
|       - |  481 | ` *  the int value of a variable.` |
|       - |  482 | ` */` |
|     352 |  483 | `static int PH7_builtin_intval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  484 | `{` |
|     354 |  485 | `	if( nArg < 1 ){` |
|       - |  486 | `		/* return 0 */` |
|     ! 0 |  487 | `		ph7_result_int(pCtx,0);` |
|     ! 0 |  488 | `	}else{` |
|       - |  489 | `		sxi64 iVal;` |
|     354 |  490 | `		sxi64 iBase = 10;` |
|     354 |  491 | `		if( nArg > 1 ){` |
|     203 |  492 | `			iBase = ph7_value_to_int64(apArg[1]);` |
|     101 |  493 | `		}` |
|     443 |  494 | `		if( iBase != 10 && ph7_value_is_string(apArg[0]) ){` |
|       - |  495 | `			/* The only path php reads $base on -- and the "is it 10?" test above is` |
|       - |  496 | `			 * the LAST thing to see the argument at full width. Everything past it` |
|       - |  497 | ``			 * is strtol's `int base` parameter, which php reaches through a plain`` |
|       - |  498 | `			 * narrowing cast, so a base of 2^32+16 really does read as 16 and` |
|       - |  499 | `			 * PHP_INT_MIN really does read as 0 (auto-detect). Spelled through` |
|       - |  500 | `			 * unsigned arithmetic because the two's-complement wrap of an` |
|       - |  501 | `			 * out-of-range signed conversion is implementation-defined. */` |
|       - |  502 | `			int nLen;` |
|     179 |  503 | `			const char *zVal = ph7_value_to_string(apArg[0],&nLen);` |
|     179 |  504 | `			sxu32 uB = (sxu32)((sxu64)iBase & 0xFFFFFFFF);` |
|     179 |  505 | `			int iB = (uB <= (sxu32)SXI32_HIGH)` |
|      89 |  506 | `				? (int)uB : -(int)(SXU32_HIGH - uB) - 1;` |
|     179 |  507 | `			iVal = IntvalStrToInt64(zVal,nLen,iBase,iB);` |
|      90 |  508 | `		}else{` |
|       - |  509 | `			/* Perform the cast */` |
|     176 |  510 | `			iVal = ph7_value_to_int64(apArg[0]);` |
|       - |  511 | `		}` |
|     354 |  512 | `		ph7_result_int64(pCtx,iVal);` |
|       - |  513 | `	}` |
|     354 |  514 | `	return PH7_OK;` |
|       2 |  515 | `}` |
|       - |  516 | `/*` |
|       - |  517 | ` * string strval($var)` |
|       - |  518 | ` *  Get the string representation of a variable.` |
|       - |  519 | ` * Parameter` |
|       - |  520 | ` *  $var: The variable being processed.` |
|       - |  521 | ` * Return` |
|       - |  522 | ` *  the string value of a variable.` |
|       - |  523 | ` */` |
|       6 |  524 | `static int PH7_builtin_strval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  525 | `{` |
|       8 |  526 | `	if( nArg < 1 ){` |
|       - |  527 | `		/* return NULL */` |
|     ! 0 |  528 | `		ph7_result_null(pCtx);` |
|     ! 0 |  529 | `	}else{` |
|       - |  530 | `		const char *zVal;` |
|       8 |  531 | `		int iLen = 0; /* cc -O6 warning */` |
|       - |  532 | `		/* Perform the cast. It is the USER-VISIBLE one: strval() is php's` |
|       - |  533 | `		 * (string) cast spelled as a function, so an object with no` |
|       - |  534 | `		 * __toString() throws there too (it used to answer "Object"). */` |
|       8 |  535 | `		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&zVal,&iLen);` |
|       8 |  536 | `		if( rcSv != SXRET_OK ){` |
|       3 |  537 | `			return rcSv;` |
|       - |  538 | `		}` |
|       6 |  539 | `		ph7_result_string(pCtx,zVal,iLen);` |
|       - |  540 | `	}` |
|       6 |  541 | `	return PH7_OK;` |
|       5 |  542 | `}` |
|       - |  543 | `/*` |
|       - |  544 | ` * bool boolval($var)` |
|       - |  545 | ` *  Get the boolean value of a variable.` |
|       - |  546 | ` * Parameter` |
|       - |  547 | ` *  $var: The variable being processed.` |
|       - |  548 | ` * Return` |
|       - |  549 | ` *  the bool value of a variable.` |
|       - |  550 | ` */` |
|      22 |  551 | `static int PH7_builtin_boolval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  552 | `{` |
|       - |  553 | `	int bVal;` |
|      23 |  554 | `	if( nArg != 1 ){` |
|     ! 0 |  555 | `		return PH7_VmThrowException(pCtx,` |
|       - |  556 | `			"ArgumentCountError",` |
|       - |  557 | `			"boolval() expects exactly 1 argument, %d given",` |
|     ! 0 |  558 | `			nArg` |
|       - |  559 | `			);` |
|       - |  560 | `	}` |
|       - |  561 | `	/* Perform the cast */` |
|      23 |  562 | `	bVal = ph7_value_to_bool(apArg[0]);` |
|      23 |  563 | `	ph7_result_bool(pCtx,bVal);` |
|      23 |  564 | `	return PH7_OK;` |
|      12 |  565 | `}` |
|       - |  566 | `/*` |
|       - |  567 | ` * bool empty($var)` |
|       - |  568 | ` *  Determine whether a variable is empty.` |
|       - |  569 | ` * Parameters` |
|       - |  570 | ` *   $var: The variable being checked.` |
|       - |  571 | ` * Return` |
|       - |  572 | ` *  0 if var has a non-empty and non-zero value.1 otherwise.` |
|       - |  573 | ` */` |
|   47672 |  574 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  575 | `{` |
|   47677 |  576 | `	int res = 1; /* Assume empty by default */` |
|   47677 |  577 | `	if( nArg > 0 ){` |
|   47677 |  578 | `		res = ph7_value_is_empty(apArg[0]);` |
|   23836 |  579 | `	}` |
|   47677 |  580 | `	ph7_result_bool(pCtx,res);` |
|   47677 |  581 | `	return PH7_OK;` |
|       - |  582 |  |
|       5 |  583 | `}` |
|       - |  584 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|       - |  585 | `#define PH7_NEED_BUILTIN_REG 1` |
|       - |  586 | `#endif` |
|       - |  587 | `#ifndef PH7_DISABLE_DISK_IO` |
|       - |  588 | `#define PH7_NEED_FMT_AND_INI 1` |
|       - |  589 | `#endif` |
|       - |  590 |  |
|       - |  591 | `/* Math functions moved to builtin_math.c */` |
|       - |  592 |  |
|       - |  593 | `/* Table of the built-in functions */` |
|       - |  594 | `/*` |
|       - |  595 | ` * int memory_get_usage([bool $real_usage = false])` |
|       - |  596 | ` *  Amount of memory, in bytes, currently allocated to the script through PHL's` |
|       - |  597 | ` *  memory backend. PHL tracks the backend's real allocated bytes, so the` |
|       - |  598 | ` *  $real_usage flag has no effect here (php's non-real figure would be smaller,` |
|       - |  599 | ` *  reflecting Zend's emalloc bookkeeping — recorded divergence).` |
|       - |  600 | ` */` |
|     ! 0 |  601 | `static int PH7_builtin_memory_get_usage(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 |  602 | `{` |
|     ! 0 |  603 | `	SXUNUSED(nArg);` |
|     ! 0 |  604 | `	SXUNUSED(apArg);` |
|     ! 0 |  605 | `	ph7_result_int64(pCtx,(ph7_int64)pCtx->pVm->sAllocator.nMemUsed);` |
|     ! 0 |  606 | `	return PH7_OK;` |
|     ! 0 |  607 | `}` |
|       - |  608 | `/*` |
|       - |  609 | ` * int memory_get_peak_usage([bool $real_usage = false])` |
|       - |  610 | ` *  High-water mark of memory_get_usage() over the script's lifetime.` |
|       - |  611 | ` */` |
|       4 |  612 | `static int PH7_builtin_memory_get_peak_usage(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  613 | `{` |
|       2 |  614 | `	SXUNUSED(nArg);` |
|       2 |  615 | `	SXUNUSED(apArg);` |
|       5 |  616 | `	ph7_result_int64(pCtx,(ph7_int64)pCtx->pVm->sAllocator.nMemPeak);` |
|       5 |  617 | `	return PH7_OK;` |
|       1 |  618 | `}` |
|       - |  619 | `/*` |
|       - |  620 | ` * void memory_reset_peak_usage()` |
|       - |  621 | ` *  Reset the peak memory usage (memory_get_peak_usage) back to the current` |
|       - |  622 | ` *  live usage — php 8.2. Frameworks call it between tests to measure per-test` |
|       - |  623 | ` *  peaks.` |
|       - |  624 | ` */` |
|       4 |  625 | `static int PH7_builtin_memory_reset_peak_usage(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  626 | `{` |
|       2 |  627 | `	SXUNUSED(nArg);` |
|       2 |  628 | `	SXUNUSED(apArg);` |
|       5 |  629 | `	pCtx->pVm->sAllocator.nMemPeak = pCtx->pVm->sAllocator.nMemUsed;` |
|       5 |  630 | `	return PH7_OK;` |
|       1 |  631 | `}` |
|       - |  632 | `/*` |
|       - |  633 | ` * PHL frees values by reference count as they go out of scope, so there is no` |
|       - |  634 | ` * mark-and-sweep cycle collector to drive. The gc_* family is provided for` |
|       - |  635 | ` * source compatibility (real frameworks call it around test runs): the state is` |
|       - |  636 | ` * observational and collection is a no-op. Recorded divergence from php, whose` |
|       - |  637 | ` * collector actually reclaims reference cycles.` |
|       - |  638 | ` */` |
|     ! 0 |  639 | `static int PH7_builtin_gc_enable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 |  640 | `{` |
|     ! 0 |  641 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     ! 0 |  642 | `	pCtx->pVm->bGcEnabled = 1;` |
|     ! 0 |  643 | `	return PH7_OK;` |
|     ! 0 |  644 | `}` |
|     ! 0 |  645 | `static int PH7_builtin_gc_disable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 |  646 | `{` |
|     ! 0 |  647 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     ! 0 |  648 | `	pCtx->pVm->bGcEnabled = 0;` |
|     ! 0 |  649 | `	return PH7_OK;` |
|     ! 0 |  650 | `}` |
|     ! 0 |  651 | `static int PH7_builtin_gc_enabled(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 |  652 | `{` |
|     ! 0 |  653 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     ! 0 |  654 | `	ph7_result_bool(pCtx,pCtx->pVm->bGcEnabled);` |
|     ! 0 |  655 | `	return PH7_OK;` |
|     ! 0 |  656 | `}` |
|     ! 0 |  657 | `static int PH7_builtin_gc_collect_cycles(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 |  658 | `{` |
|       - |  659 | `	/* No cycle collector: nothing to reclaim. Returns the count collected (0). */` |
|     ! 0 |  660 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     ! 0 |  661 | `	ph7_result_int(pCtx,0);` |
|     ! 0 |  662 | `	return PH7_OK;` |
|     ! 0 |  663 | `}` |
|     ! 0 |  664 | `static int PH7_builtin_gc_mem_caches(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 |  665 | `{` |
|     ! 0 |  666 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     ! 0 |  667 | `	ph7_result_int(pCtx,0);` |
|     ! 0 |  668 | `	return PH7_OK;` |
|     ! 0 |  669 | `}` |
|       - |  670 | `/*` |
|       - |  671 | ` * array gc_status(void)` |
|       - |  672 | ` *  php 8.3 shape. PHL never runs a collection, so every counter is zero and the` |
|       - |  673 | ` *  timing fields are 0.0; 'running' reflects gc_enable()/gc_disable().` |
|       - |  674 | ` */` |
|     ! 0 |  675 | `static int PH7_builtin_gc_status(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 |  676 | `{` |
|       - |  677 | `	ph7_value *pArray,*pVal;` |
|     ! 0 |  678 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     ! 0 |  679 | `	pArray = ph7_context_new_array(pCtx);` |
|     ! 0 |  680 | `	pVal = ph7_context_new_scalar(pCtx);` |
|     ! 0 |  681 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|     ! 0 |  682 | `		ph7_result_null(pCtx);` |
|     ! 0 |  683 | `		return PH7_OK;` |
|       - |  684 | `	}` |
|       - |  685 | `	/* Key order matches php 8.3's gc_status(). */` |
|     ! 0 |  686 | `	ph7_value_bool(pVal,pCtx->pVm->bGcEnabled); ph7_array_add_strkey_elem(pArray,"running",pVal);` |
|     ! 0 |  687 | `	ph7_value_bool(pVal,0);      ph7_array_add_strkey_elem(pArray,"protected",pVal);` |
|     ! 0 |  688 | `	ph7_value_bool(pVal,0);      ph7_array_add_strkey_elem(pArray,"full",pVal);` |
|     ! 0 |  689 | `	ph7_value_int(pVal,0);       ph7_array_add_strkey_elem(pArray,"runs",pVal);` |
|     ! 0 |  690 | `	ph7_value_int(pVal,0);       ph7_array_add_strkey_elem(pArray,"collected",pVal);` |
|     ! 0 |  691 | `	ph7_value_int(pVal,1000);    ph7_array_add_strkey_elem(pArray,"threshold",pVal);` |
|     ! 0 |  692 | `	ph7_value_int(pVal,0);       ph7_array_add_strkey_elem(pArray,"buffer_size",pVal);` |
|     ! 0 |  693 | `	ph7_value_int(pVal,0);       ph7_array_add_strkey_elem(pArray,"roots",pVal);` |
|     ! 0 |  694 | `	ph7_value_double(pVal,0.0);  ph7_array_add_strkey_elem(pArray,"application_time",pVal);` |
|     ! 0 |  695 | `	ph7_value_double(pVal,0.0);  ph7_array_add_strkey_elem(pArray,"collector_time",pVal);` |
|     ! 0 |  696 | `	ph7_value_double(pVal,0.0);  ph7_array_add_strkey_elem(pArray,"destructor_time",pVal);` |
|     ! 0 |  697 | `	ph7_value_double(pVal,0.0);  ph7_array_add_strkey_elem(pArray,"free_time",pVal);` |
|     ! 0 |  698 | `	ph7_context_release_value(pCtx,pVal);` |
|     ! 0 |  699 | `	ph7_result_value(pCtx,pArray);` |
|     ! 0 |  700 | `	return PH7_OK;` |
|     ! 0 |  701 | `}` |
|       - |  702 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|       - |  703 | `	{ "memory_get_usage"     , PH7_builtin_memory_get_usage      },` |
|       - |  704 | `	{ "memory_get_peak_usage", PH7_builtin_memory_get_peak_usage },` |
|       - |  705 | `	{ "memory_reset_peak_usage", PH7_builtin_memory_reset_peak_usage },` |
|       - |  706 | `	{ "gc_enable"            , PH7_builtin_gc_enable             },` |
|       - |  707 | `	{ "gc_disable"           , PH7_builtin_gc_disable            },` |
|       - |  708 | `	{ "gc_enabled"           , PH7_builtin_gc_enabled            },` |
|       - |  709 | `	{ "gc_collect_cycles"    , PH7_builtin_gc_collect_cycles     },` |
|       - |  710 | `	{ "gc_mem_caches"        , PH7_builtin_gc_mem_caches         },` |
|       - |  711 | `	{ "gc_status"            , PH7_builtin_gc_status             },` |
|       - |  712 | `	   /* Variable handling functions */` |
|       - |  713 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|       - |  714 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|       - |  715 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|       - |  716 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|       - |  717 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|       - |  718 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|       - |  719 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|       - |  720 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|       - |  721 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|       - |  722 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|       - |  723 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|       - |  724 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|       - |  725 | `	{ "is_resource", PH7_builtin_is_resource },` |
|       - |  726 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|       - |  727 | `	{ "intval"     , PH7_builtin_intval      },` |
|       - |  728 | `	{ "strval"     , PH7_builtin_strval      },` |
|       - |  729 | `	{ "boolval"    , PH7_builtin_boolval     },` |
|       - |  730 | `	{ "empty"      , PH7_builtin_empty       },` |
|       - |  731 | `#ifdef PH7_NEED_BUILTIN_REG` |
|       - |  732 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|       - |  733 | `	   /* Math functions */` |
|       - |  734 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|       - |  735 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|       - |  736 | `	{ "acosh" ,   PH7_builtin_acosh        },` |
|       - |  737 | `	{ "asinh" ,   PH7_builtin_asinh        },` |
|       - |  738 | `	{ "atanh" ,   PH7_builtin_atanh        },` |
|       - |  739 | `	{ "expm1" ,   PH7_builtin_expm1        },` |
|       - |  740 | `	{ "log1p" ,   PH7_builtin_log1p        },` |
|       - |  741 | `	{ "deg2rad" , PH7_builtin_deg2rad      },` |
|       - |  742 | `	{ "rad2deg" , PH7_builtin_rad2deg      },` |
|       - |  743 | `	{ "fpow" ,    PH7_builtin_fpow         },` |
|       - |  744 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|       - |  745 | `	{ "floor",    PH7_builtin_floor        },` |
|       - |  746 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|       - |  747 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|       - |  748 | `	{ "acos" ,    PH7_builtin_acos         },` |
|       - |  749 | `	{ "asin" ,    PH7_builtin_asin         },` |
|       - |  750 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|       - |  751 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|       - |  752 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|       - |  753 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|       - |  754 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|       - |  755 | `	{ "atan" ,    PH7_builtin_atan         },` |
|       - |  756 | `	{ "atan2",    PH7_builtin_atan2        },` |
|       - |  757 | `	{ "log"  ,    PH7_builtin_log          },` |
|       - |  758 | `	{ "log10" ,   PH7_builtin_log10        },` |
|       - |  759 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|       - |  760 | `	{ "pi",       PH7_builtin_pi           },` |
|       - |  761 | `	{ "fmod",     PH7_builtin_fmod         },` |
|       - |  762 | `	{ "hypot",    PH7_builtin_hypot        },` |
|       - |  763 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|       - |  764 | `	{ "round",    PH7_builtin_round        },` |
|       - |  765 | `	{ "intdiv",   PH7_builtin_intdiv       },` |
|       - |  766 | `	{ "number_format", PH7_builtin_number_format },` |
|       - |  767 | `	{ "dechex", PH7_builtin_dechex         },` |
|       - |  768 | `	{ "decoct", PH7_builtin_decoct         },` |
|       - |  769 | `	{ "decbin", PH7_builtin_decbin         },` |
|       - |  770 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|       - |  771 | `	{ "bindec", PH7_builtin_bindec         },` |
|       - |  772 | `	{ "octdec", PH7_builtin_octdec         },` |
|       - |  773 | `	{ "srand",  PH7_builtin_srand          },` |
|       - |  774 | `	{ "mt_srand",PH7_builtin_srand         },` |
|       - |  775 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|       - |  776 | `#ifdef PH7_NEED_FMT_AND_INI` |
|       - |  777 | `	{ "base_convert", PH7_builtin_base_convert },` |
|       - |  778 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|       - |  779 | `#ifdef PH7_NEED_BUILTIN_REG` |
|       - |  780 | `	   /* String handling functions */` |
|       - |  781 |  |
|       - |  782 | `	{ "substr",          PH7_builtin_substr     },` |
|       - |  783 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|       - |  784 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|       - |  785 | `	{ "substr_replace",  PH7_builtin_substr_replace },` |
|       - |  786 | `	{ "levenshtein",     PH7_builtin_levenshtein },` |
|       - |  787 | `	{ "similar_text",    PH7_builtin_similar_text },` |
|       - |  788 | `	{ "str_word_count",  PH7_builtin_str_word_count },` |
|       - |  789 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|       - |  790 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|       - |  791 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|       - |  792 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|       - |  793 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|       - |  794 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|       - |  795 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|       - |  796 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|       - |  797 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|       - |  798 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|       - |  799 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|       - |  800 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|       - |  801 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|       - |  802 | `	{ "strnatcmp"  , PH7_builtin_strnatcmp  },` |
|       - |  803 | `	{ "strnatcasecmp", PH7_builtin_strnatcmp },` |
|       - |  804 | `	{ "version_compare", PH7_builtin_version_compare },` |
|       - |  805 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|       - |  806 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|       - |  807 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|       - |  808 | `	{ "implode"    , PH7_builtin_implode    },` |
|       - |  809 | `	{ "join"       , PH7_builtin_implode    },` |
|       - |  810 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|       - |  811 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|       - |  812 | `	{ "explode"     , PH7_builtin_explode    },` |
|       - |  813 | `	{ "trim"        , PH7_builtin_trim       },` |
|       - |  814 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|       - |  815 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|       - |  816 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|       - |  817 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|       - |  818 | `	{ "mb_strtolower",PH7_builtin_mb_case_f }, /* UTF-8 only (builtin_mb.c) */` |
|       - |  819 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|       - |  820 | `	{ "mb_strtoupper",PH7_builtin_mb_case_f }, /* UTF-8 only (builtin_mb.c) */` |
|       - |  821 | `	{ "mb_strlen",    PH7_builtin_mb_strlen_f },` |
|       - |  822 | `	{ "mb_substr",    PH7_builtin_mb_substr_f },` |
|       - |  823 | `	{ "mb_convert_case", PH7_builtin_mb_convert_case_f },` |
|       - |  824 | `	{ "mb_strpos",    PH7_builtin_mb_strpos_f },` |
|       - |  825 | `	{ "mb_stripos",   PH7_builtin_mb_strpos_f },` |
|       - |  826 | `	{ "mb_strrpos",   PH7_builtin_mb_strpos_f },` |
|       - |  827 | `	{ "mb_strripos",  PH7_builtin_mb_strpos_f },` |
|       - |  828 | `	{ "mb_strstr",    PH7_builtin_mb_strstr_f },` |
|       - |  829 | `	{ "mb_stristr",   PH7_builtin_mb_strstr_f },` |
|       - |  830 | `	{ "mb_strrchr",   PH7_builtin_mb_strstr_f },` |
|       - |  831 | `	{ "mb_strrichr",  PH7_builtin_mb_strstr_f },` |
|       - |  832 | `	{ "mb_substr_count", PH7_builtin_mb_substr_count_f },` |
|       - |  833 | `	{ "mb_str_pad",   PH7_builtin_mb_str_pad_f },` |
|       - |  834 | `	{ "mb_strcut",    PH7_builtin_mb_strcut_f },` |
|       - |  835 | `	{ "mb_strimwidth", PH7_builtin_mb_strimwidth_f },` |
|       - |  836 | `	{ "mb_str_split", PH7_builtin_mb_str_split_f },` |
|       - |  837 | `	{ "mb_trim",      PH7_builtin_mb_trim_f  },` |
|       - |  838 | `	{ "mb_ltrim",     PH7_builtin_mb_trim_f  },` |
|       - |  839 | `	{ "mb_rtrim",     PH7_builtin_mb_trim_f  },` |
|       - |  840 | `	{ "mb_internal_encoding", PH7_builtin_mb_internal_encoding_f },` |
|       - |  841 | `	{ "mb_substitute_character", PH7_builtin_mb_substitute_character_f },` |
|       - |  842 | `	{ "mb_scrub",     PH7_builtin_mb_scrub_f },` |
|       - |  843 | `	{ "mb_check_encoding",    PH7_builtin_mb_check_encoding_f },` |
|       - |  844 | `	{ "mb_strwidth",  PH7_builtin_mb_strwidth_f },` |
|       - |  845 | `	{ "mb_chr",       PH7_builtin_mb_chr_f   },` |
|       - |  846 | `	{ "mb_ord",       PH7_builtin_mb_ord_f   },` |
|       - |  847 | `	{ "mb_ucfirst",   PH7_builtin_mb_ucfirst_f },` |
|       - |  848 | `	{ "mb_lcfirst",   PH7_builtin_mb_ucfirst_f },` |
|       - |  849 | `	{ "mb_detect_encoding", PH7_builtin_mb_detect_encoding_f },` |
|       - |  850 | `	{ "mb_convert_encoding", PH7_builtin_mb_convert_encoding_f },` |
|       - |  851 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|       - |  852 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|       - |  853 | `	{ "ord",          PH7_builtin_ord        },` |
|       - |  854 | `	{ "chr",          PH7_builtin_chr        },` |
|       - |  855 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|       - |  856 | `	{ "strstr",       PH7_builtin_strstr     },` |
|       - |  857 | `	{ "stristr",      PH7_builtin_stristr    },` |
|       - |  858 | `	{ "strchr",       PH7_builtin_strstr     },` |
|       - |  859 | `	{ "strpos",       PH7_builtin_strpos     },` |
|       - |  860 | `	{ "stripos",      PH7_builtin_stripos    },` |
|       - |  861 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|       - |  862 | `	{ "strripos",     PH7_builtin_strripos   },` |
|       - |  863 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|       - |  864 | `	{ "strrev",       PH7_builtin_strrev     },` |
|       - |  865 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|       - |  866 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|       - |  867 | `	{ "str_contains", PH7_builtin_str_contains },` |
|       - |  868 | `	{ "str_starts_with", PH7_builtin_str_starts_with },` |
|       - |  869 | `	{ "str_ends_with", PH7_builtin_str_ends_with },` |
|       - |  870 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|       - |  871 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|       - |  872 | `#ifdef PH7_NEED_FMT_AND_INI` |
|       - |  873 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|       - |  874 | `	{ "printf",       PH7_builtin_printf     },` |
|       - |  875 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|       - |  876 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|       - |  877 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|       - |  878 | `#ifdef PH7_NEED_BUILTIN_REG` |
|       - |  879 | `	{ "size_format",  PH7_builtin_size_format},` |
|       - |  880 |  |
|       - |  881 |  |
|       - |  882 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|       - |  883 | `	{ "md5",          PH7_builtin_md5       },` |
|       - |  884 | `	{ "sha1",         PH7_builtin_sha1      },` |
|       - |  885 | `	{ "crc32",        PH7_builtin_crc32     },` |
|       - |  886 | `	{ "hash",         PH7_builtin_hash      },` |
|       - |  887 | `	{ "hash_hmac",    PH7_builtin_hash_hmac },` |
|       - |  888 | `	{ "hash_equals",  PH7_builtin_hash_equals },` |
|       - |  889 | `	{ "hash_algos",   PH7_builtin_hash_algos },` |
|       - |  890 | `	{ "hash_hmac_algos", PH7_builtin_hash_hmac_algos },` |
|       - |  891 | `	{ "hash_init",    PH7_builtin_hash_init },` |
|       - |  892 | `	{ "hash_update",  PH7_builtin_hash_update },` |
|       - |  893 | `	{ "hash_final",   PH7_builtin_hash_final },` |
|       - |  894 | `	{ "hash_copy",    PH7_builtin_hash_copy },` |
|       - |  895 | `	{ "hash_pbkdf2",  PH7_builtin_hash_pbkdf2 },` |
|       - |  896 | `	{ "hash_hkdf",    PH7_builtin_hash_hkdf },` |
|       - |  897 | `	{ "crypt",        PH7_builtin_crypt     },` |
|       - |  898 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|       - |  899 | `	{ "password_hash",         PH7_builtin_password_hash },` |
|       - |  900 | `	{ "password_verify",       PH7_builtin_password_verify },` |
|       - |  901 | `	{ "password_get_info",     PH7_builtin_password_get_info },` |
|       - |  902 | `	{ "password_needs_rehash", PH7_builtin_password_needs_rehash },` |
|       - |  903 | `	{ "password_algos",        PH7_builtin_password_algos },` |
|       - |  904 | `	{ "filter_var",            PH7_builtin_filter_var },` |
|       - |  905 | `	{ "filter_input",          PH7_builtin_filter_input },` |
|       - |  906 | `	{ "filter_list",           PH7_builtin_filter_list },` |
|       - |  907 | `	{ "filter_id",             PH7_builtin_filter_id },` |
|       - |  908 | `	{ "filter_has_var",        PH7_builtin_filter_has_var },` |
|       - |  909 | `	{ "filter_var_array",      PH7_builtin_filter_var_array },` |
|       - |  910 | `	{ "filter_input_array",    PH7_builtin_filter_input_array },` |
|       - |  911 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|       - |  912 | `#ifdef PH7_NEED_FMT_AND_INI` |
|       - |  913 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|       - |  914 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|       - |  915 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|       - |  916 | `#ifdef PH7_NEED_BUILTIN_REG` |
|       - |  917 |  |
|       - |  918 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|       - |  919 | `	{ "str_split",    PH7_builtin_str_split  },` |
|       - |  920 | `	{ "count_chars",  PH7_builtin_count_chars},` |
|       - |  921 | `	{ "strspn",       PH7_builtin_strspn     },` |
|       - |  922 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|       - |  923 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|       - |  924 | `	{ "soundex",      PH7_builtin_soundex    },` |
|       - |  925 | `	{ "str_rot13",    PH7_builtin_str_rot13  },` |
|       - |  926 | `	{ "metaphone",    PH7_builtin_metaphone  },` |
|       - |  927 | `	{ "pack",         PH7_builtin_pack       },` |
|       - |  928 | `	{ "unpack",       PH7_builtin_unpack     },` |
|       - |  929 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|       - |  930 | `	{ "strtok",       PH7_builtin_strtok     },` |
|       - |  931 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|       - |  932 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|       - |  933 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|       - |  934 | `	{ "strtr",        PH7_builtin_strtr      },` |
|       - |  935 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|       - |  936 | `#ifdef PH7_NEED_FMT_AND_INI` |
|       - |  937 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|       - |  938 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|       - |  939 | `#ifdef PH7_NEED_BUILTIN_REG` |
|       - |  940 |  |
|       - |  941 | `	         /* Ctype functions */` |
|       - |  942 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|       - |  943 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|       - |  944 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|       - |  945 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|       - |  946 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|       - |  947 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|       - |  948 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|       - |  949 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|       - |  950 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|       - |  951 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|       - |  952 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|       - |  953 | `	         /* Time functions */` |
|       - |  954 | `	{ "time"    ,    PH7_builtin_time         },` |
|       - |  955 | `	{ "microtime",   PH7_builtin_microtime    },` |
|       - |  956 | `	{ "hrtime",      PH7_builtin_hrtime       },` |
|       - |  957 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|       - |  958 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|       - |  959 | `	{ "date",        PH7_builtin_date         },` |
|       - |  960 | `	{ "idate",       PH7_builtin_idate        },` |
|       - |  961 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|       - |  962 | `	{ "localtime",   PH7_builtin_localtime    },` |
|       - |  963 | `	{ "mktime",      PH7_builtin_mktime       },` |
|       - |  964 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|       - |  965 | `	{ "date_default_timezone_get", PH7_builtin_date_default_timezone_get },` |
|       - |  966 | `	{ "date_default_timezone_set", PH7_builtin_date_default_timezone_set },` |
|       - |  967 | `	        /* URL functions */` |
|       - |  968 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|       - |  969 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|       - |  970 | `	{ "convert_uuencode",PH7_builtin_convert_uuencode },` |
|       - |  971 | `	{ "convert_uudecode",PH7_builtin_convert_uudecode },` |
|       - |  972 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|       - |  973 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|       - |  974 | `	{ "rawurlencode", PH7_builtin_rawurlencode },` |
|       - |  975 | `	{ "http_build_query", PH7_builtin_http_build_query },` |
|       - |  976 | `	{ "parse_str",    PH7_builtin_parse_str  },` |
|       - |  977 | `	{ "rawurldecode", PH7_builtin_rawurldecode },` |
|       - |  978 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|       - |  979 | `};` |
|       - |  980 | `/*` |
|       - |  981 | ` * Register the built-in functions defined above,the array functions` |
|       - |  982 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|       - |  983 | ` */` |
|    4552 |  984 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|       5 |  985 | `{` |
|       - |  986 | `	sxu32 n;` |
| 1110693 |  987 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 1106141 |  988 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
|  553073 |  989 | `	}` |
|       - |  990 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|    4557 |  991 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|       - |  992 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|    4557 |  993 | `	PH7_RegisterIORoutine(&(*pVm));` |
|    4557 |  994 | `}` |
|       - |  995 |  |
|       - |  996 | `/*` |
|       - |  997 | ` * UTF-8 codepoint reader shared by the glob/fnmatch matcher in vfs.c.` |
|       - |  998 | ` * Relocated here from the removed vm_xml.c when the legacy xml_* API was` |
|       - |  999 | ` * dropped; the utf8_encode()/utf8_decode() builtins it once served were` |
|       - | 1000 | ` * removed in turn (superseded by mb_convert_encoding()),` |
|       - | 1001 | ` * leaving only this public-domain SQLite reader.` |
|       - | 1002 | ` */` |
|       - | 1003 | `/* SPDX-SnippetBegin */` |
|       - | 1004 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|       - | 1005 | `/* SPDX-License-Identifier: blessing */` |
|       - | 1006 | `/*` |
|       - | 1007 | ` * UTF-8 decoding routine extracted from the sqlite3 source tree.` |
|       - | 1008 | ` * Original author: D. Richard Hipp (http://www.sqlite.org)` |
|       - | 1009 | ` * Status: Public Domain` |
|       - | 1010 | ` */` |
|       - | 1011 | `/*` |
|       - | 1012 | `** This lookup table is used to help decode the first byte of` |
|       - | 1013 | `** a multi-byte UTF8 character.` |
|       - | 1014 | `*/` |
|       - | 1015 | `static const unsigned char UtfTrans1[] = {` |
|       - | 1016 | `  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,` |
|       - | 1017 | `  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,` |
|       - | 1018 | `  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,` |
|       - | 1019 | `  0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,` |
|       - | 1020 | `  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,` |
|       - | 1021 | `  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,` |
|       - | 1022 | `  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,` |
|       - | 1023 | `  0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x00, 0x00,` |
|       - | 1024 | `};` |
|       - | 1025 | `/*` |
|       - | 1026 | `** Translate a single UTF-8 character.  Return the unicode value.` |
|       - | 1027 | `**` |
|       - | 1028 | `** During translation, assume that the byte that zTerm points` |
|       - | 1029 | `** is a 0x00.` |
|       - | 1030 | `**` |
|       - | 1031 | `** Write a pointer to the next unread byte back into *pzNext.` |
|       - | 1032 | `**` |
|       - | 1033 | `** Notes On Invalid UTF-8:` |
|       - | 1034 | `**` |
|       - | 1035 | `**  *  This routine never allows a 7-bit character (0x00 through 0x7f) to` |
|       - | 1036 | `**     be encoded as a multi-byte character.  Any multi-byte character that` |
|       - | 1037 | `**     attempts to encode a value between 0x00 and 0x7f is rendered as 0xfffd.` |
|       - | 1038 | `**` |
|       - | 1039 | `**  *  This routine never allows a UTF16 surrogate value to be encoded.` |
|       - | 1040 | `**     If a multi-byte character attempts to encode a value between` |
|       - | 1041 | `**     0xd800 and 0xe000 then it is rendered as 0xfffd.` |
|       - | 1042 | `**` |
|       - | 1043 | `**  *  Bytes in the range of 0x80 through 0xbf which occur as the first` |
|       - | 1044 | `**     byte of a character are interpreted as single-byte characters` |
|       - | 1045 | `**     and rendered as themselves even though they are technically` |
|       - | 1046 | `**     invalid characters.` |
|       - | 1047 | `**` |
|       - | 1048 | `**  *  This routine accepts an infinite number of different UTF8 encodings` |
|       - | 1049 | `**     for unicode values 0x80 and greater.  It do not change over-length` |
|       - | 1050 | `**     encodings to 0xfffd as some systems recommend.` |
|       - | 1051 | `*/` |
|       - | 1052 | `#define READ_UTF8(zIn, zTerm, c)                           \` |
|       - | 1053 | `  c = *(zIn++);                                            \` |
|       - | 1054 | `  if( c>=0xc0 ){                                           \` |
|       - | 1055 | `    c = UtfTrans1[c-0xc0];                                 \` |
|       - | 1056 | `    while( zIn!=zTerm && (*zIn & 0xc0)==0x80 ){            \` |
|       - | 1057 | `      c = (c<<6) + (0x3f & *(zIn++));                      \` |
|       - | 1058 | `    }                                                      \` |
|       - | 1059 | `    if( c<0x80                                             \` |
|       - | 1060 | `        \|\| (c&0xFFFFF800)==0xD800                          \` |
|       - | 1061 | `        \|\| (c&0xFFFFFFFE)==0xFFFE ){  c = 0xFFFD; }        \` |
|       - | 1062 | `  }` |
|     678 | 1063 | `PH7_PRIVATE int PH7_Utf8Read(` |
|       - | 1064 | `  const unsigned char *z,         /* First byte of UTF-8 character */` |
|       - | 1065 | `  const unsigned char *zTerm,     /* Pretend this byte is 0x00 */` |
|       - | 1066 | `  const unsigned char **pzNext    /* Write first byte past UTF-8 char here */` |
|       3 | 1067 | `){` |
|       - | 1068 | `  int c;` |
|     681 | 1069 | `  READ_UTF8(z, zTerm, c);` |
|     681 | 1070 | `  *pzNext = z;` |
|     681 | 1071 | `  return c;` |
|       3 | 1072 | `}` |
|       - | 1073 | `/* SPDX-SnippetEnd */` |
|       - | 1074 | `/*` |
|       - | 1075 | ` * Read one STRICTLY well-formed UTF-8 sequence from z[0..n-1].` |
|       - | 1076 | ` *` |
|       - | 1077 | ` * Unlike PH7_Utf8Read above (the lenient SQLite reader, which renders anything` |
|       - | 1078 | ` * dubious as U+FFFD and happily accepts over-long forms), this one implements` |
|       - | 1079 | ` * the RFC 3629 / Unicode "Table 3-7 well-formed byte sequences" rule php uses` |
|       - | 1080 | ` * wherever it has to decide whether a php string really is UTF-8:` |
|       - | 1081 | ` *` |
|       - | 1082 | ` *   00..7F                          one byte` |
|       - | 1083 | ` *   C2..DF  80..BF                  (C0/C1 are over-long two-byte forms)` |
|       - | 1084 | ` *   E0      A0..BF  80..BF          (E0 80..9F is over-long)` |
|       - | 1085 | ` *   E1..EC  80..BF  80..BF` |
|       - | 1086 | ` *   ED      80..9F  80..BF          (ED A0..BF is a UTF-16 surrogate)` |
|       - | 1087 | ` *   EE..EF  80..BF  80..BF` |
|       - | 1088 | ` *   F0      90..BF  80..BF  80..BF  (F0 80..8F is over-long)` |
|       - | 1089 | ` *   F1..F3  80..BF  80..BF  80..BF` |
|       - | 1090 | ` *   F4      80..8F  80..BF  80..BF  (past U+10FFFF)` |
|       - | 1091 | ` *` |
|       - | 1092 | ` * Returns the code point and writes the sequence length to *pLen. On an` |
|       - | 1093 | ` * ill-formed sequence it returns -1 and writes 1, so a caller can apply its own` |
|       - | 1094 | ` * php policy to the single offending byte (json_encode: JSON_ERROR_UTF8 or the` |
|       - | 1095 | ` * JSON_INVALID_UTF8_* substitution; mb_strtolower: '?') and resume at the next` |
|       - | 1096 | ` * byte exactly like php does. n must be >= 1.` |
|       - | 1097 | ` */` |
|   13114 | 1098 | `PH7_PRIVATE sxi32 PH7_Utf8ReadStrict(const unsigned char *z,sxu32 n,sxu32 *pLen)` |
|       2 | 1099 | `{` |
|   13116 | 1100 | `	sxu32 c = z[0];` |
|   13116 | 1101 | `	*pLen = 1;` |
|   13116 | 1102 | `	if( c < 0x80 ){` |
|   11043 | 1103 | `		return (sxi32)c;` |
|       - | 1104 | `	}` |
|    2074 | 1105 | `	if( c >= 0xC2 && c <= 0xDF ){` |
|    1104 | 1106 | `		if( n < 2 \|\| (z[1] & 0xC0) != 0x80 ){` |
|      63 | 1107 | `			return -1;` |
|       - | 1108 | `		}` |
|    1042 | 1109 | `		*pLen = 2;` |
|    1042 | 1110 | `		return (sxi32)(((c & 0x1F) << 6) \| (z[1] & 0x3F));` |
|       - | 1111 | `	}` |
|     971 | 1112 | `	if( c >= 0xE0 && c <= 0xEF ){` |
|     381 | 1113 | `		sxu32 iLow = (c == 0xE0) ? 0xA0 : 0x80;` |
|     381 | 1114 | `		sxu32 iHigh = (c == 0xED) ? 0x9F : 0xBF;` |
|     381 | 1115 | `		if( n < 3 \|\| z[1] < iLow \|\| z[1] > iHigh \|\| (z[2] & 0xC0) != 0x80 ){` |
|      99 | 1116 | `			return -1;` |
|       - | 1117 | `		}` |
|     283 | 1118 | `		*pLen = 3;` |
|     283 | 1119 | `		return (sxi32)(((c & 0x0F) << 12) \| ((z[1] & 0x3F) << 6) \| (z[2] & 0x3F));` |
|       - | 1120 | `	}` |
|     591 | 1121 | `	if( c >= 0xF0 && c <= 0xF4 ){` |
|     103 | 1122 | `		sxu32 iLow = (c == 0xF0) ? 0x90 : 0x80;` |
|     103 | 1123 | `		sxu32 iHigh = (c == 0xF4) ? 0x8F : 0xBF;` |
|     102 | 1124 | `		if( n < 4 \|\| z[1] < iLow \|\| z[1] > iHigh` |
|      79 | 1125 | `		 \|\| (z[2] & 0xC0) != 0x80 \|\| (z[3] & 0xC0) != 0x80 ){` |
|      31 | 1126 | `			return -1;` |
|       - | 1127 | `		}` |
|      73 | 1128 | `		*pLen = 4;` |
|     109 | 1129 | `		return (sxi32)(((c & 0x07) << 18) \| ((z[1] & 0x3F) << 12)` |
|      72 | 1130 | `			\| ((z[2] & 0x3F) << 6) \| (z[3] & 0x3F));` |
|       - | 1131 | `	}` |
|     489 | 1132 | `	return -1; /* 80..C1 as a lead byte, or F5..FF */` |
|    7518 | 1133 | `}` |
|       - | 1134 |  |
