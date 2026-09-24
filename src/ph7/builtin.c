/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
/* filter_var(FILTER_VALIDATE_FLOAT) parses with libc strtod directly because it
 * needs errno==ERANGE to reject out-of-range magnitudes; SyStrToReal (also
 * strtod-backed nowadays) exposes no range-error signal. */
#include <stdlib.h>  /* strtod */
#include <math.h>    /* HUGE_VAL */
#include <errno.h>   /* ERANGE (strtod range-error signal) */
#include <stdio.h>   /* snprintf (printf-family float conversions — correctly
                      * rounded digits like php's zend_dtoa; see PH7_InputFormat) */
/* Shared ZPP helper for `int` parameters — defined OUTSIDE the
 * PH7_DISABLE_BUILTIN_FUNC guard because hashmap.c (array_slice) and
 * builtin_math.c (intdiv) call it and both compile in the tiny build. */
PH7_PRIVATE sxi32 PH7_IntArgResolve(
	ph7_context *pCtx,
	ph7_value *pArg,
	const char *zFunc,
	int iArgNum,
	const char *zParamName,
	const char *zTypeStr,
	sxi64 *pOut
){
	if( ph7_value_is_null(pArg) ){
		/* php only DEPRECATES passing null to a non-nullable internal param; PHL
		 * targets php's non-deprecated surface and rejects it with the TypeError
		 * php will eventually raise. */
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"%s(): Argument #%d (%s) must be of type %s, null given",
			zFunc,iArgNum,zParamName,zTypeStr
			);
	}
	if( ph7_value_is_float(pArg) ){
		double dVal = ph7_value_to_double(pArg);
		sxi64 iVal;
		/* php: NAN/INF/out-of-int64-range floats fail ZPP outright */
		if( dVal != dVal || dVal >= 9223372036854775808.0 || dVal < -9223372036854775808.0 ){
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"%s(): Argument #%d (%s) must be of type %s, float given",
				zFunc,iArgNum,zParamName,zTypeStr
				);
		}
		iVal = (sxi64)dVal;
		if( (double)iVal != dVal ){
			/* php DEPRECATES a lossy float->int; PHL rejects it (the value is not
			 * representable as int). */
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"%s(): Argument #%d (%s) must be of type %s, float given",
				zFunc,iArgNum,zParamName,zTypeStr
				);
		}
		*pOut = iVal;
		return PH7_OK;
	}
	if( ph7_value_is_string(pArg) ){
		const char *zNum;
		int nSlen;
		int i,bFloat = 0;
		if( !PH7_MemObjStringIsNumeric(pArg) ){
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"%s(): Argument #%d (%s) must be of type %s, string given",
				zFunc,iArgNum,zParamName,zTypeStr
				);
		}
		zNum = ph7_value_to_string(pArg,&nSlen);
		for( i = 0 ; i < nSlen ; i++ ){
			if( zNum[i] == '.' || zNum[i] == 'e' || zNum[i] == 'E' ){
				bFloat = 1;
				break;
			}
		}
		if( bFloat ){
			double dVal = 0;
			sxi64 iVal;
			SyStrToReal(zNum,(sxu32)nSlen,(void *)&dVal,0);
			if( dVal != dVal || dVal >= 9223372036854775808.0 || dVal < -9223372036854775808.0 ){
				return PH7_VmThrowException(pCtx,
					"TypeError",
					"%s(): Argument #%d (%s) must be of type %s, string given",
					zFunc,iArgNum,zParamName,zTypeStr
					);
			}
			iVal = (sxi64)dVal;
			if( (double)iVal != dVal ){
				/* php DEPRECATES a lossy float-string->int; PHL rejects it. */
				return PH7_VmThrowException(pCtx,
					"TypeError",
					"%s(): Argument #%d (%s) must be of type %s, string given",
					zFunc,iArgNum,zParamName,zTypeStr
					);
			}
			*pOut = iVal;
			return PH7_OK;
		}
		*pOut = ph7_value_to_int64(pArg);
		return PH7_OK;
	}
	if( !ph7_value_is_int(pArg) && !ph7_value_is_bool(pArg) ){
		/* Arrays, resources and objects: php names the class for objects */
		const char *zType = ph7_type_name(pArg);
		if( ph7_value_is_object(pArg) ){
			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;
			if( pInst && pInst->pClass ){
				zType = SyStringData(&pInst->pClass->sName);
			}
		}
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"%s(): Argument #%d (%s) must be of type %s, %s given",
			zFunc,iArgNum,zParamName,zTypeStr,zType
			);
	}
	*pOut = ph7_value_to_int64(pArg);
	return PH7_OK;
}

/* This file implement built-in 'foreign' functions for the PH7 engine */
/*
 * Section:
 *    Variable handling Functions.
 * Status:
 *    Stable.
 */
/*
 * bool is_bool($var)
 *  Finds out whether a variable is a boolean.
 * Parameters
 *   $var: The variable being evaluated.
 * Return
 *  TRUE if var is a boolean. False otherwise.
 */
static int PH7_builtin_is_bool(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume false by default */
	if( nArg > 0 ){
		res = ph7_value_is_bool(apArg[0]);
	}
	/* Query result */
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * bool is_float($var)
 * bool is_double($var)
 *  Finds out whether a variable is a float.
 * Parameters
 *   $var: The variable being evaluated.
 * Return
 *  TRUE if var is a float. False otherwise.
 */
static int PH7_builtin_is_float(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume false by default */
	if( nArg > 0 ){
		res = ph7_value_is_float(apArg[0]);
	}
	/* Query result */
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * bool is_int($var)
 * bool is_integer($var)
 * bool is_long($var)
 *  Finds out whether a variable is an integer.
 * Parameters
 *   $var: The variable being evaluated.
 * Return
 *  TRUE if var is an integer. False otherwise.
 */
static int PH7_builtin_is_int(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume false by default */
	if( nArg > 0 ){
		/* Strict PHP identity: a float is never an int, even when it holds an
		 * integer value (1.0). An integer-valued real carries both MEMOBJ_INT
		 * (cached) and MEMOBJ_REAL, so REAL must be excluded here. */
		res = ph7_value_is_int(apArg[0]) && !ph7_value_is_float(apArg[0]);
	}
	/* Query result */
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * bool is_string($var)
 *  Finds out whether a variable is a string.
 * Parameters
 *   $var: The variable being evaluated.
 * Return
 *  TRUE if var is string. False otherwise.
 */
static int PH7_builtin_is_string(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume false by default */
	if( nArg > 0 ){
		res = ph7_value_is_string(apArg[0]);
	}
	/* Query result */
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * bool is_null($var)
 *  Finds out whether a variable is NULL.
 * Parameters
 *   $var: The variable being evaluated.
 * Return
 *  TRUE if var is NULL. False otherwise.
 */
static int PH7_builtin_is_null(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume false by default */
	if( nArg > 0 ){
		res = ph7_value_is_null(apArg[0]);
	}
	/* Query result */
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * bool is_numeric($var)
 *  Find out whether a variable is NULL.
 * Parameters
 *  $var: The variable being evaluated.
 * Return
 *  True if var is numeric. False otherwise.
 */
static int PH7_builtin_is_numeric(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume false by default */
	if( nArg > 0 ){
		/* Strict PHP semantics: only int/float and numeric strings are numeric.
		 * PHL's lenient helper also reports booleans as numeric (they coerce for
		 * arithmetic), but php's is_numeric() rejects true/false, so exclude
		 * MEMOBJ_BOOL here. */
		res = ph7_value_is_numeric(apArg[0]) && !ph7_value_is_bool(apArg[0]);
	}
	/* Query result */
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * bool is_scalar($var)
 *  Find out whether a variable is a scalar.
 * Parameters
 *  $var: The variable being evaluated.
 * Return
 *  True if var is scalar. False otherwise.
 */
static int PH7_builtin_is_scalar(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume false by default */
	if( nArg > 0 ){
		/* Strict PHP semantics: scalars are int/float/string/bool. PHL's
		 * MEMOBJ_SCALAR bucket also includes NULL, but php's is_scalar(null) is
		 * false, so exclude the NULL case. */
		res = ph7_value_is_scalar(apArg[0]) && !ph7_value_is_null(apArg[0]);
	}
	/* Query result */
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * bool is_array($var)
 *  Find out whether a variable is an array.
 * Parameters
 *  $var: The variable being evaluated.
 * Return
 *  True if var is an array. False otherwise.
 */
static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume false by default */
	if( nArg > 0 ){
		res = ph7_value_is_array(apArg[0]);
	}
	/* Query result */
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * bool is_object($var)
 *  Find out whether a variable is an object.
 * Parameters
 *  $var: The variable being evaluated.
 * Return
 *  True if var is an object. False otherwise.
 */
static int PH7_builtin_is_object(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume false by default */
	if( nArg > 0 ){
		res = ph7_value_is_object(apArg[0]);
	}
	/* Query result */
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * bool is_resource($var)
 *  Find out whether a variable is a resource.
 * Parameters
 *  $var: The variable being evaluated.
 * Return
 *  True if a resource. False otherwise.
 */
static int PH7_builtin_is_resource(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume false by default */
	if( nArg > 0 && ph7_value_is_resource(apArg[0]) ){
		/* A handle closed via fclose()/closedir()/pclose() is no longer a
		 * live resource — php's is_resource() returns false for it. */
		res = !PH7_VfsResourceIsClosed(apArg[0]->x.pOther);
	}
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * float floatval($var)
 *  Get float value of a variable.
 * Parameter
 *  $var: The variable being processed.
 * Return
 *  the float value of a variable.
 */
static int PH7_builtin_floatval(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg < 1 ){
		/* return 0.0 */
		ph7_result_double(pCtx,0);
	}else{
		double dval;
		/* Perform the cast */
		dval = ph7_value_to_double(apArg[0]);
		ph7_result_double(pCtx,dval);
	}
	return PH7_OK;
}
/*
 * int intval($var)
 *  Get integer value of a variable.
 * Parameter
 *  $var: The variable being processed.
 * Return
 *  the int value of a variable.
 */
static int PH7_builtin_intval(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg < 1 ){
		/* return 0 */
		ph7_result_int(pCtx,0);
	}else{
		sxi64 iVal;
		/* Perform the cast */
		iVal = ph7_value_to_int64(apArg[0]);
		ph7_result_int64(pCtx,iVal);
	}
	return PH7_OK;
}
/*
 * string strval($var)
 *  Get the string representation of a variable.
 * Parameter
 *  $var: The variable being processed.
 * Return
 *  the string value of a variable.
 */
static int PH7_builtin_strval(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg < 1 ){
		/* return NULL */
		ph7_result_null(pCtx);
	}else{
		const char *zVal;
		int iLen = 0; /* cc -O6 warning */
		/* Perform the cast. It is the USER-VISIBLE one: strval() is php's
		 * (string) cast spelled as a function, so an object with no
		 * __toString() throws there too (it used to answer "Object"). */
		sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[0],&zVal,&iLen);
		if( rcSv != SXRET_OK ){
			return rcSv;
		}
		ph7_result_string(pCtx,zVal,iLen);
	}
	return PH7_OK;
}
/*
 * bool boolval($var)
 *  Get the boolean value of a variable.
 * Parameter
 *  $var: The variable being processed.
 * Return
 *  the bool value of a variable.
 */
static int PH7_builtin_boolval(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int bVal;
	if( nArg != 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"boolval() expects exactly 1 argument, %d given",
			nArg
			);
	}
	/* Perform the cast */
	bVal = ph7_value_to_bool(apArg[0]);
	ph7_result_bool(pCtx,bVal);
	return PH7_OK;
}
/*
 * bool empty($var)
 *  Determine whether a variable is empty.
 * Parameters
 *   $var: The variable being checked.
 * Return
 *  0 if var has a non-empty and non-zero value.1 otherwise.
 */
static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 1; /* Assume empty by default */
	if( nArg > 0 ){
		res = ph7_value_is_empty(apArg[0]);
	}
	ph7_result_bool(pCtx,res);
	return PH7_OK;

}
#ifndef PH7_DISABLE_BUILTIN_FUNC
#define PH7_NEED_BUILTIN_REG 1
#endif
#ifndef PH7_DISABLE_DISK_IO
#define PH7_NEED_FMT_AND_INI 1
#endif

/* Math functions moved to builtin_math.c */

/* Table of the built-in functions */
/*
 * int memory_get_usage([bool $real_usage = false])
 *  Amount of memory, in bytes, currently allocated to the script through PHL's
 *  memory backend. PHL tracks the backend's real allocated bytes, so the
 *  $real_usage flag has no effect here (php's non-real figure would be smaller,
 *  reflecting Zend's emalloc bookkeeping — recorded divergence).
 */
static int PH7_builtin_memory_get_usage(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_int64(pCtx,(ph7_int64)pCtx->pVm->sAllocator.nMemUsed);
	return PH7_OK;
}
/*
 * int memory_get_peak_usage([bool $real_usage = false])
 *  High-water mark of memory_get_usage() over the script's lifetime.
 */
static int PH7_builtin_memory_get_peak_usage(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_int64(pCtx,(ph7_int64)pCtx->pVm->sAllocator.nMemPeak);
	return PH7_OK;
}
/*
 * void memory_reset_peak_usage()
 *  Reset the peak memory usage (memory_get_peak_usage) back to the current
 *  live usage — php 8.2. Frameworks call it between tests to measure per-test
 *  peaks.
 */
static int PH7_builtin_memory_reset_peak_usage(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	pCtx->pVm->sAllocator.nMemPeak = pCtx->pVm->sAllocator.nMemUsed;
	return PH7_OK;
}
/*
 * PHL frees values by reference count as they go out of scope, so there is no
 * mark-and-sweep cycle collector to drive. The gc_* family is provided for
 * source compatibility (real frameworks call it around test runs): the state is
 * observational and collection is a no-op. Recorded divergence from php, whose
 * collector actually reclaims reference cycles.
 */
static int PH7_builtin_gc_enable(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	pCtx->pVm->bGcEnabled = 1;
	return PH7_OK;
}
static int PH7_builtin_gc_disable(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	pCtx->pVm->bGcEnabled = 0;
	return PH7_OK;
}
static int PH7_builtin_gc_enabled(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	ph7_result_bool(pCtx,pCtx->pVm->bGcEnabled);
	return PH7_OK;
}
static int PH7_builtin_gc_collect_cycles(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	/* No cycle collector: nothing to reclaim. Returns the count collected (0). */
	SXUNUSED(nArg); SXUNUSED(apArg);
	ph7_result_int(pCtx,0);
	return PH7_OK;
}
static int PH7_builtin_gc_mem_caches(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	ph7_result_int(pCtx,0);
	return PH7_OK;
}
/*
 * array gc_status(void)
 *  php 8.3 shape. PHL never runs a collection, so every counter is zero and the
 *  timing fields are 0.0; 'running' reflects gc_enable()/gc_disable().
 */
static int PH7_builtin_gc_status(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pArray,*pVal;
	SXUNUSED(nArg); SXUNUSED(apArg);
	pArray = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pVal == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Key order matches php 8.3's gc_status(). */
	ph7_value_bool(pVal,pCtx->pVm->bGcEnabled); ph7_array_add_strkey_elem(pArray,"running",pVal);
	ph7_value_bool(pVal,0);      ph7_array_add_strkey_elem(pArray,"protected",pVal);
	ph7_value_bool(pVal,0);      ph7_array_add_strkey_elem(pArray,"full",pVal);
	ph7_value_int(pVal,0);       ph7_array_add_strkey_elem(pArray,"runs",pVal);
	ph7_value_int(pVal,0);       ph7_array_add_strkey_elem(pArray,"collected",pVal);
	ph7_value_int(pVal,1000);    ph7_array_add_strkey_elem(pArray,"threshold",pVal);
	ph7_value_int(pVal,0);       ph7_array_add_strkey_elem(pArray,"buffer_size",pVal);
	ph7_value_int(pVal,0);       ph7_array_add_strkey_elem(pArray,"roots",pVal);
	ph7_value_double(pVal,0.0);  ph7_array_add_strkey_elem(pArray,"application_time",pVal);
	ph7_value_double(pVal,0.0);  ph7_array_add_strkey_elem(pArray,"collector_time",pVal);
	ph7_value_double(pVal,0.0);  ph7_array_add_strkey_elem(pArray,"destructor_time",pVal);
	ph7_value_double(pVal,0.0);  ph7_array_add_strkey_elem(pArray,"free_time",pVal);
	ph7_context_release_value(pCtx,pVal);
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
static const ph7_builtin_func aBuiltInFunc[] = {
	{ "memory_get_usage"     , PH7_builtin_memory_get_usage      },
	{ "memory_get_peak_usage", PH7_builtin_memory_get_peak_usage },
	{ "memory_reset_peak_usage", PH7_builtin_memory_reset_peak_usage },
	{ "gc_enable"            , PH7_builtin_gc_enable             },
	{ "gc_disable"           , PH7_builtin_gc_disable            },
	{ "gc_enabled"           , PH7_builtin_gc_enabled            },
	{ "gc_collect_cycles"    , PH7_builtin_gc_collect_cycles     },
	{ "gc_mem_caches"        , PH7_builtin_gc_mem_caches         },
	{ "gc_status"            , PH7_builtin_gc_status             },
	   /* Variable handling functions */
	{ "is_bool"    , PH7_builtin_is_bool     },
	{ "is_float"   , PH7_builtin_is_float    },
	{ "is_double"  , PH7_builtin_is_float    },
	{ "is_int"     , PH7_builtin_is_int      },
	{ "is_integer" , PH7_builtin_is_int      },
	{ "is_long"    , PH7_builtin_is_int      },
	{ "is_string"  , PH7_builtin_is_string   },
	{ "is_null"    , PH7_builtin_is_null     },
	{ "is_numeric" , PH7_builtin_is_numeric  },
	{ "is_scalar"  , PH7_builtin_is_scalar   },
	{ "is_array"   , PH7_builtin_is_array    },
	{ "is_object"  , PH7_builtin_is_object   },
	{ "is_resource", PH7_builtin_is_resource },
	{ "floatval"   , PH7_builtin_floatval    },
	{ "intval"     , PH7_builtin_intval      },
	{ "strval"     , PH7_builtin_strval      },
	{ "boolval"    , PH7_builtin_boolval     },
	{ "empty"      , PH7_builtin_empty       },
#ifdef PH7_NEED_BUILTIN_REG
#ifdef PH7_ENABLE_MATH_FUNC
	   /* Math functions */
	{ "abs"  ,    PH7_builtin_abs          },
	{ "sqrt" ,    PH7_builtin_sqrt         },
	{ "acosh" ,   PH7_builtin_acosh        },
	{ "asinh" ,   PH7_builtin_asinh        },
	{ "atanh" ,   PH7_builtin_atanh        },
	{ "expm1" ,   PH7_builtin_expm1        },
	{ "log1p" ,   PH7_builtin_log1p        },
	{ "deg2rad" , PH7_builtin_deg2rad      },
	{ "rad2deg" , PH7_builtin_rad2deg      },
	{ "fpow" ,    PH7_builtin_fpow         },
	{ "exp"  ,    PH7_builtin_exp          },
	{ "floor",    PH7_builtin_floor        },
	{ "cos"  ,    PH7_builtin_cos          },
	{ "sin"  ,    PH7_builtin_sin          },
	{ "acos" ,    PH7_builtin_acos         },
	{ "asin" ,    PH7_builtin_asin         },
	{ "cosh" ,    PH7_builtin_cosh         },
	{ "sinh" ,    PH7_builtin_sinh         },
	{ "ceil" ,    PH7_builtin_ceil         },
	{ "tan"  ,    PH7_builtin_tan          },
	{ "tanh" ,    PH7_builtin_tanh         },
	{ "atan" ,    PH7_builtin_atan         },
	{ "atan2",    PH7_builtin_atan2        },
	{ "log"  ,    PH7_builtin_log          },
	{ "log10" ,   PH7_builtin_log10        },
	{ "pow"  ,    PH7_builtin_pow          },
	{ "pi",       PH7_builtin_pi           },
	{ "fmod",     PH7_builtin_fmod         },
	{ "hypot",    PH7_builtin_hypot        },
#endif /* PH7_ENABLE_MATH_FUNC */
	{ "round",    PH7_builtin_round        },
	{ "intdiv",   PH7_builtin_intdiv       },
	{ "number_format", PH7_builtin_number_format },
	{ "dechex", PH7_builtin_dechex         },
	{ "decoct", PH7_builtin_decoct         },
	{ "decbin", PH7_builtin_decbin         },
	{ "hexdec", PH7_builtin_hexdec         },
	{ "bindec", PH7_builtin_bindec         },
	{ "octdec", PH7_builtin_octdec         },
	{ "srand",  PH7_builtin_srand          },
	{ "mt_srand",PH7_builtin_srand         },
#endif /* PH7_NEED_BUILTIN_REG */
#ifdef PH7_NEED_FMT_AND_INI
	{ "base_convert", PH7_builtin_base_convert },
#endif /* PH7_NEED_FMT_AND_INI */
#ifdef PH7_NEED_BUILTIN_REG
	   /* String handling functions */

	{ "substr",          PH7_builtin_substr     },
	{ "substr_compare",  PH7_builtin_substr_compare },
	{ "substr_count",    PH7_builtin_substr_count },
	{ "substr_replace",  PH7_builtin_substr_replace },
	{ "levenshtein",     PH7_builtin_levenshtein },
	{ "similar_text",    PH7_builtin_similar_text },
	{ "str_word_count",  PH7_builtin_str_word_count },
	{ "chunk_split",     PH7_builtin_chunk_split},
	{ "addslashes" ,     PH7_builtin_addslashes },
	{ "addcslashes",     PH7_builtin_addcslashes},
	{ "quotemeta",       PH7_builtin_quotemeta  },
	{ "stripslashes",    PH7_builtin_stripslashes },
	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },
	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },
	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },
	{ "htmlentities",PH7_builtin_htmlentities},
	{ "html_entity_decode", PH7_builtin_html_entity_decode},
	{ "strlen"     , PH7_builtin_strlen     },
	{ "strcmp"     , PH7_builtin_strcmp     },
	{ "strcoll"    , PH7_builtin_strcmp     },
	{ "strnatcmp"  , PH7_builtin_strnatcmp  },
	{ "strnatcasecmp", PH7_builtin_strnatcmp },
	{ "version_compare", PH7_builtin_version_compare },
	{ "strncmp"    , PH7_builtin_strncmp    },
	{ "strcasecmp" , PH7_builtin_strcasecmp },
	{ "strncasecmp", PH7_builtin_strncasecmp},
	{ "implode"    , PH7_builtin_implode    },
	{ "join"       , PH7_builtin_implode    },
	{ "implode_recursive" , PH7_builtin_implode_recursive },
	{ "join_recursive"    , PH7_builtin_implode_recursive },
	{ "explode"     , PH7_builtin_explode    },
	{ "trim"        , PH7_builtin_trim       },
	{ "rtrim"       , PH7_builtin_rtrim      },
	{ "chop"        , PH7_builtin_rtrim      },
	{ "ltrim"       , PH7_builtin_ltrim      },
	{ "strtolower",   PH7_builtin_strtolower },
	{ "mb_strtolower",PH7_builtin_mb_case_f }, /* UTF-8 only (builtin_mb.c) */
	{ "strtoupper",   PH7_builtin_strtoupper },
	{ "mb_strtoupper",PH7_builtin_mb_case_f }, /* UTF-8 only (builtin_mb.c) */
	{ "mb_strlen",    PH7_builtin_mb_strlen_f },
	{ "mb_substr",    PH7_builtin_mb_substr_f },
	{ "mb_convert_case", PH7_builtin_mb_convert_case_f },
	{ "mb_strpos",    PH7_builtin_mb_strpos_f },
	{ "mb_stripos",   PH7_builtin_mb_strpos_f },
	{ "mb_strrpos",   PH7_builtin_mb_strpos_f },
	{ "mb_strripos",  PH7_builtin_mb_strpos_f },
	{ "mb_strstr",    PH7_builtin_mb_strstr_f },
	{ "mb_stristr",   PH7_builtin_mb_strstr_f },
	{ "mb_strrchr",   PH7_builtin_mb_strstr_f },
	{ "mb_strrichr",  PH7_builtin_mb_strstr_f },
	{ "mb_substr_count", PH7_builtin_mb_substr_count_f },
	{ "mb_str_pad",   PH7_builtin_mb_str_pad_f },
	{ "mb_strcut",    PH7_builtin_mb_strcut_f },
	{ "mb_strimwidth", PH7_builtin_mb_strimwidth_f },
	{ "mb_str_split", PH7_builtin_mb_str_split_f },
	{ "mb_trim",      PH7_builtin_mb_trim_f  },
	{ "mb_ltrim",     PH7_builtin_mb_trim_f  },
	{ "mb_rtrim",     PH7_builtin_mb_trim_f  },
	{ "mb_internal_encoding", PH7_builtin_mb_internal_encoding_f },
	{ "mb_substitute_character", PH7_builtin_mb_substitute_character_f },
	{ "mb_scrub",     PH7_builtin_mb_scrub_f },
	{ "mb_check_encoding",    PH7_builtin_mb_check_encoding_f },
	{ "mb_strwidth",  PH7_builtin_mb_strwidth_f },
	{ "mb_chr",       PH7_builtin_mb_chr_f   },
	{ "mb_ord",       PH7_builtin_mb_ord_f   },
	{ "mb_ucfirst",   PH7_builtin_mb_ucfirst_f },
	{ "mb_lcfirst",   PH7_builtin_mb_ucfirst_f },
	{ "mb_detect_encoding", PH7_builtin_mb_detect_encoding_f },
	{ "mb_convert_encoding", PH7_builtin_mb_convert_encoding_f },
	{ "ucfirst",      PH7_builtin_ucfirst    },
	{ "lcfirst",      PH7_builtin_lcfirst    },
	{ "ord",          PH7_builtin_ord        },
	{ "chr",          PH7_builtin_chr        },
	{ "bin2hex",      PH7_builtin_bin2hex    },
	{ "strstr",       PH7_builtin_strstr     },
	{ "stristr",      PH7_builtin_stristr    },
	{ "strchr",       PH7_builtin_strstr     },
	{ "strpos",       PH7_builtin_strpos     },
	{ "stripos",      PH7_builtin_stripos    },
	{ "strrpos",      PH7_builtin_strrpos    },
	{ "strripos",     PH7_builtin_strripos   },
	{ "strrchr",      PH7_builtin_strrchr    },
	{ "strrev",       PH7_builtin_strrev     },
	{ "ucwords",      PH7_builtin_ucwords    },
	{ "str_repeat",   PH7_builtin_str_repeat },
	{ "str_contains", PH7_builtin_str_contains },
	{ "str_starts_with", PH7_builtin_str_starts_with },
	{ "str_ends_with", PH7_builtin_str_ends_with },
	{ "nl2br",        PH7_builtin_nl2br      },
#endif /* PH7_NEED_BUILTIN_REG */
#ifdef PH7_NEED_FMT_AND_INI
	{ "sprintf",      PH7_builtin_sprintf    },
	{ "printf",       PH7_builtin_printf     },
	{ "vprintf",      PH7_builtin_vprintf    },
	{ "vsprintf",     PH7_builtin_vsprintf   },
#endif /* PH7_NEED_FMT_AND_INI */
#ifdef PH7_NEED_BUILTIN_REG
	{ "size_format",  PH7_builtin_size_format},


#ifndef PH7_DISABLE_HASH_FUNC
	{ "md5",          PH7_builtin_md5       },
	{ "sha1",         PH7_builtin_sha1      },
	{ "crc32",        PH7_builtin_crc32     },
	{ "hash",         PH7_builtin_hash      },
	{ "hash_hmac",    PH7_builtin_hash_hmac },
	{ "hash_equals",  PH7_builtin_hash_equals },
	{ "hash_algos",   PH7_builtin_hash_algos },
#endif /* PH7_DISABLE_HASH_FUNC */
	{ "password_hash",         PH7_builtin_password_hash },
	{ "password_verify",       PH7_builtin_password_verify },
	{ "password_get_info",     PH7_builtin_password_get_info },
	{ "password_needs_rehash", PH7_builtin_password_needs_rehash },
	{ "filter_var",            PH7_builtin_filter_var },
	{ "filter_input",          PH7_builtin_filter_input },
#endif /* PH7_NEED_BUILTIN_REG */
#ifdef PH7_NEED_FMT_AND_INI
	{ "str_getcsv",   PH7_builtin_str_getcsv },
	{ "strip_tags",   PH7_builtin_strip_tags },
#endif /* PH7_NEED_FMT_AND_INI */
#ifdef PH7_NEED_BUILTIN_REG

	{ "str_shuffle",  PH7_builtin_str_shuffle},
	{ "str_split",    PH7_builtin_str_split  },
	{ "count_chars",  PH7_builtin_count_chars},
	{ "strspn",       PH7_builtin_strspn     },
	{ "strcspn",      PH7_builtin_strcspn    },
	{ "strpbrk",      PH7_builtin_strpbrk    },
	{ "soundex",      PH7_builtin_soundex    },
	{ "str_rot13",    PH7_builtin_str_rot13  },
	{ "metaphone",    PH7_builtin_metaphone  },
	{ "pack",         PH7_builtin_pack       },
	{ "unpack",       PH7_builtin_unpack     },
	{ "wordwrap",     PH7_builtin_wordwrap   },
	{ "strtok",       PH7_builtin_strtok     },
	{ "str_pad",      PH7_builtin_str_pad    },
	{ "str_replace",  PH7_builtin_str_replace},
	{ "str_ireplace", PH7_builtin_str_replace},
	{ "strtr",        PH7_builtin_strtr      },
#endif /* PH7_NEED_BUILTIN_REG */
#ifdef PH7_NEED_FMT_AND_INI
	{ "parse_ini_string", PH7_builtin_parse_ini_string},
#endif /* PH7_NEED_FMT_AND_INI */
#ifdef PH7_NEED_BUILTIN_REG

	         /* Ctype functions */
	{ "ctype_alnum", PH7_builtin_ctype_alnum },
	{ "ctype_alpha", PH7_builtin_ctype_alpha },
	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },
	{ "ctype_digit", PH7_builtin_ctype_digit },
	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},
	{ "ctype_graph", PH7_builtin_ctype_graph },
	{ "ctype_print", PH7_builtin_ctype_print },
	{ "ctype_punct", PH7_builtin_ctype_punct },
	{ "ctype_space", PH7_builtin_ctype_space },
	{ "ctype_lower", PH7_builtin_ctype_lower },
	{ "ctype_upper", PH7_builtin_ctype_upper },
	         /* Time functions */
	{ "time"    ,    PH7_builtin_time         },
	{ "microtime",   PH7_builtin_microtime    },
	{ "hrtime",      PH7_builtin_hrtime       },
	{ "getdate" ,    PH7_builtin_getdate      },
	{ "gettimeofday",PH7_builtin_gettimeofday },
	{ "date",        PH7_builtin_date         },
	{ "idate",       PH7_builtin_idate        },
	{ "gmdate",      PH7_builtin_gmdate       },
	{ "localtime",   PH7_builtin_localtime    },
	{ "mktime",      PH7_builtin_mktime       },
	{ "gmmktime",    PH7_builtin_mktime       },
	{ "date_default_timezone_get", PH7_builtin_date_default_timezone_get },
	{ "date_default_timezone_set", PH7_builtin_date_default_timezone_set },
	        /* URL functions */
	{ "base64_encode",PH7_builtin_base64_encode },
	{ "base64_decode",PH7_builtin_base64_decode },
	{ "convert_uuencode",PH7_builtin_convert_uuencode },
	{ "convert_uudecode",PH7_builtin_convert_uudecode },
	{ "urlencode",    PH7_builtin_urlencode },
	{ "urldecode",    PH7_builtin_urldecode },
	{ "rawurlencode", PH7_builtin_rawurlencode },
	{ "http_build_query", PH7_builtin_http_build_query },
	{ "parse_str",    PH7_builtin_parse_str  },
	{ "rawurldecode", PH7_builtin_rawurldecode },
#endif /* PH7_NEED_BUILTIN_REG */
};
/*
 * Register the built-in functions defined above,the array functions
 * defined in hashmap.c and the IO functions defined in vfs.c.
 */
PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)
{
	sxu32 n;
	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){
		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);
	}
	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */
	PH7_RegisterHashmapFunctions(&(*pVm));
	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */
	PH7_RegisterIORoutine(&(*pVm));
}

/*
 * UTF-8 codepoint reader shared by the glob/fnmatch matcher in vfs.c.
 * Relocated here from the removed vm_xml.c when the legacy xml_* API was
 * dropped; the utf8_encode()/utf8_decode() builtins it once served were
 * removed in turn (superseded by mb_convert_encoding()),
 * leaving only this public-domain SQLite reader.
 */
/* SPDX-SnippetBegin */
/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */
/* SPDX-License-Identifier: blessing */
/*
 * UTF-8 decoding routine extracted from the sqlite3 source tree.
 * Original author: D. Richard Hipp (http://www.sqlite.org)
 * Status: Public Domain
 */
/*
** This lookup table is used to help decode the first byte of
** a multi-byte UTF8 character.
*/
static const unsigned char UtfTrans1[] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
  0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x00, 0x00,
};
/*
** Translate a single UTF-8 character.  Return the unicode value.
**
** During translation, assume that the byte that zTerm points
** is a 0x00.
**
** Write a pointer to the next unread byte back into *pzNext.
**
** Notes On Invalid UTF-8:
**
**  *  This routine never allows a 7-bit character (0x00 through 0x7f) to
**     be encoded as a multi-byte character.  Any multi-byte character that
**     attempts to encode a value between 0x00 and 0x7f is rendered as 0xfffd.
**
**  *  This routine never allows a UTF16 surrogate value to be encoded.
**     If a multi-byte character attempts to encode a value between
**     0xd800 and 0xe000 then it is rendered as 0xfffd.
**
**  *  Bytes in the range of 0x80 through 0xbf which occur as the first
**     byte of a character are interpreted as single-byte characters
**     and rendered as themselves even though they are technically
**     invalid characters.
**
**  *  This routine accepts an infinite number of different UTF8 encodings
**     for unicode values 0x80 and greater.  It do not change over-length
**     encodings to 0xfffd as some systems recommend.
*/
#define READ_UTF8(zIn, zTerm, c)                           \
  c = *(zIn++);                                            \
  if( c>=0xc0 ){                                           \
    c = UtfTrans1[c-0xc0];                                 \
    while( zIn!=zTerm && (*zIn & 0xc0)==0x80 ){            \
      c = (c<<6) + (0x3f & *(zIn++));                      \
    }                                                      \
    if( c<0x80                                             \
        || (c&0xFFFFF800)==0xD800                          \
        || (c&0xFFFFFFFE)==0xFFFE ){  c = 0xFFFD; }        \
  }
PH7_PRIVATE int PH7_Utf8Read(
  const unsigned char *z,         /* First byte of UTF-8 character */
  const unsigned char *zTerm,     /* Pretend this byte is 0x00 */
  const unsigned char **pzNext    /* Write first byte past UTF-8 char here */
){
  int c;
  READ_UTF8(z, zTerm, c);
  *pzNext = z;
  return c;
}
/* SPDX-SnippetEnd */
/*
 * Read one STRICTLY well-formed UTF-8 sequence from z[0..n-1].
 *
 * Unlike PH7_Utf8Read above (the lenient SQLite reader, which renders anything
 * dubious as U+FFFD and happily accepts over-long forms), this one implements
 * the RFC 3629 / Unicode "Table 3-7 well-formed byte sequences" rule php uses
 * wherever it has to decide whether a php string really is UTF-8:
 *
 *   00..7F                          one byte
 *   C2..DF  80..BF                  (C0/C1 are over-long two-byte forms)
 *   E0      A0..BF  80..BF          (E0 80..9F is over-long)
 *   E1..EC  80..BF  80..BF
 *   ED      80..9F  80..BF          (ED A0..BF is a UTF-16 surrogate)
 *   EE..EF  80..BF  80..BF
 *   F0      90..BF  80..BF  80..BF  (F0 80..8F is over-long)
 *   F1..F3  80..BF  80..BF  80..BF
 *   F4      80..8F  80..BF  80..BF  (past U+10FFFF)
 *
 * Returns the code point and writes the sequence length to *pLen. On an
 * ill-formed sequence it returns -1 and writes 1, so a caller can apply its own
 * php policy to the single offending byte (json_encode: JSON_ERROR_UTF8 or the
 * JSON_INVALID_UTF8_* substitution; mb_strtolower: '?') and resume at the next
 * byte exactly like php does. n must be >= 1.
 */
PH7_PRIVATE sxi32 PH7_Utf8ReadStrict(const unsigned char *z,sxu32 n,sxu32 *pLen)
{
	sxu32 c = z[0];
	*pLen = 1;
	if( c < 0x80 ){
		return (sxi32)c;
	}
	if( c >= 0xC2 && c <= 0xDF ){
		if( n < 2 || (z[1] & 0xC0) != 0x80 ){
			return -1;
		}
		*pLen = 2;
		return (sxi32)(((c & 0x1F) << 6) | (z[1] & 0x3F));
	}
	if( c >= 0xE0 && c <= 0xEF ){
		sxu32 iLow = (c == 0xE0) ? 0xA0 : 0x80;
		sxu32 iHigh = (c == 0xED) ? 0x9F : 0xBF;
		if( n < 3 || z[1] < iLow || z[1] > iHigh || (z[2] & 0xC0) != 0x80 ){
			return -1;
		}
		*pLen = 3;
		return (sxi32)(((c & 0x0F) << 12) | ((z[1] & 0x3F) << 6) | (z[2] & 0x3F));
	}
	if( c >= 0xF0 && c <= 0xF4 ){
		sxu32 iLow = (c == 0xF0) ? 0x90 : 0x80;
		sxu32 iHigh = (c == 0xF4) ? 0x8F : 0xBF;
		if( n < 4 || z[1] < iLow || z[1] > iHigh
		 || (z[2] & 0xC0) != 0x80 || (z[3] & 0xC0) != 0x80 ){
			return -1;
		}
		*pLen = 4;
		return (sxi32)(((c & 0x07) << 18) | ((z[1] & 0x3F) << 12)
			| ((z[2] & 0x3F) << 6) | (z[3] & 0x3F));
	}
	return -1; /* 80..C1 as a lead byte, or F5..FF */
}
