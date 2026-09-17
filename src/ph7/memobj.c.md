# src/ph7/memobj.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 840/1030 lines (81.55%)

[Root index](../../index.md) | [Directory index](index.md)

|     Hits | Line | Source |
| -------: | ---: | :--- |
|        - |    1 | `/**` |
|        - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|        - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|        - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|        - |    5 | ` */` |
|        - |    6 | `#include "ph7int.h" /* This file handle low-level stuff related to indexed memory objects [i.e: ph7_value] */` |
|        - |    7 | `#include <stdio.h>  /* snprintf — the default float->string conversion needs` |
|        - |    8 | `                     * correctly-rounded digits like php (see MemObjStringValue) */` |
|        - |    9 | `#include <stdlib.h> /* strtod — var_dump's shortest-round-trip float shape` |
|        - |   10 | `                     * verifies each candidate by parsing it back */` |
|        - |   11 |  |
|        - |   12 | `/* Portable 64-bit overflow-detecting arithmetic for compilers that lack the` |
|        - |   13 | ` * GCC/Clang __builtin_*_overflow intrinsics (i.e. MSVC). The header exposes` |
|        - |   14 | ` * these through the PH7_{ADD,SUB,MUL}_OVERFLOW64 macros; the intrinsic path` |
|        - |   15 | ` * needs no out-of-line definition, so gate the whole block off there to avoid` |
|        - |   16 | ` * an unused-function warning. Each sets *pR to the two's-complement wrapped` |
|        - |   17 | ` * result and returns non-zero on overflow. The additive checks compute the` |
|        - |   18 | ` * wrapped result via unsigned math (no signed-overflow UB) and test the sign` |
|        - |   19 | ` * bits; the multiplicative check mirrors vm.c's proven bound-check form. */` |
|        - |   20 | `#if !(defined(__GNUC__) \|\| defined(__clang__))` |
|        - |   21 | `PH7_PRIVATE int PH7_AddOverflow64(sxi64 a,sxi64 b,sxi64 *pR)` |
|        5 |   22 | `{` |
|        5 |   23 | `	*pR = (sxi64)((sxu64)a + (sxu64)b);` |
|        - |   24 | `	/* Overflow iff the operands share a sign and the result's sign differs. */` |
|        5 |   25 | `	return ((a ^ *pR) & (b ^ *pR)) < 0;` |
|        5 |   26 | `}` |
|        - |   27 | `PH7_PRIVATE int PH7_SubOverflow64(sxi64 a,sxi64 b,sxi64 *pR)` |
|        5 |   28 | `{` |
|        5 |   29 | `	*pR = (sxi64)((sxu64)a - (sxu64)b);` |
|        - |   30 | `	/* Overflow iff the operands differ in sign and the result's sign differs` |
|        - |   31 | `	 * from the minuend's. */` |
|        5 |   32 | `	return ((a ^ b) & (a ^ *pR)) < 0;` |
|        5 |   33 | `}` |
|        - |   34 | `PH7_PRIVATE int PH7_MulOverflow64(sxi64 a,sxi64 b,sxi64 *pR)` |
|        5 |   35 | `{` |
|        5 |   36 | `	*pR = (sxi64)((sxu64)a * (sxu64)b);` |
|        5 |   37 | `	if( a == 0 \|\| b == 0 \|\| a == 1 \|\| b == 1 ){` |
|        4 |   38 | `		return 0;` |
|        - |   39 | `	}` |
|        4 |   40 | `	if( a == -1 ){` |
|        1 |   41 | `		return b == SMALLEST_INT64;` |
|        - |   42 | `	}` |
|        4 |   43 | `	if( b == -1 ){` |
|      ! 0 |   44 | `		return a == SMALLEST_INT64;` |
|        - |   45 | `	}` |
|        4 |   46 | `	if( a > 0 ){` |
|        4 |   47 | `		if( b > 0 ){` |
|        4 |   48 | `			return a > LARGEST_INT64 / b;` |
|      ! 0 |   49 | `		}else{` |
|        1 |   50 | `			return b < SMALLEST_INT64 / a;` |
|        - |   51 | `		}` |
|      ! 0 |   52 | `	}else{` |
|        1 |   53 | `		if( b > 0 ){` |
|        1 |   54 | `			return a < SMALLEST_INT64 / b;` |
|      ! 0 |   55 | `		}else{` |
|        1 |   56 | `			return b < LARGEST_INT64 / a;` |
|        - |   57 | `		}` |
|        - |   58 | `	}` |
|        5 |   59 | `}` |
|        - |   60 | `#endif` |
|        - |   61 |  |
|        - |   62 | `/* Provide PHP-style type names for values.  This utility may be reused` |
|        - |   63 | ` * by any subsystem that works with ph7_value.` |
|        - |   64 | ` */` |
|      368 |   65 | `PH7_PRIVATE const char *ph7_type_name(ph7_value *pVal)` |
|        5 |   66 | `{` |
|      373 |   67 | `	if( ph7_value_is_null(pVal) ) return "null";` |
|      335 |   68 | `	if( ph7_value_is_bool(pVal) ) return "bool";` |
|      327 |   69 | `	if( ph7_value_is_int(pVal) ) return "int";` |
|      241 |   70 | `	if( ph7_value_is_float(pVal) ) return "float";` |
|      231 |   71 | `	if( ph7_value_is_string(pVal) ) return "string";` |
|       25 |   72 | `	if( ph7_value_is_array(pVal) ) return "array";` |
|        3 |   73 | `	if( ph7_value_is_object(pVal) ) return "object";` |
|      ! 0 |   74 | `	if( ph7_value_is_resource(pVal) ) return "resource";` |
|      ! 0 |   75 | `	return "unknown";` |
|      189 |   76 | `}` |
|        - |   77 |  |
|        - |   78 | `/*` |
|        - |   79 | ` * Notes on memory objects [i.e: ph7_value].` |
|        - |   80 | ` * Internally, the PH7 virtual machine manipulates nearly all PHP values` |
|        - |   81 | ` * [i.e: string,int,float,resource,object,bool,null..] as ph7_values structures.` |
|        - |   82 | ` * Each ph7_values struct may cache multiple representations (string,` |
|        - |   83 | ` * integer etc.) of the same value.` |
|        - |   84 | ` */` |
|        - |   85 | `/*` |
|        - |   86 | ` * Convert a 64-bit IEEE double into a 64-bit signed integer.` |
|        - |   87 | ` * If the double is too large, return 0x8000000000000000.` |
|        - |   88 | ` *` |
|        - |   89 | ` * Most systems appear to do this simply by assigning ariables and without` |
|        - |   90 | ` * the extra range tests.` |
|        - |   91 | ` * But there are reports that windows throws an expection if the floating` |
|        - |   92 | ` * point value is out of range.` |
|        - |   93 | ` */` |
|    10776 |   94 | `static sxi64 MemObjRealToInt(ph7_value *pObj)` |
|        5 |   95 | `{` |
|        - |   96 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |   97 | `	/* Real and 64bit integer are the same when floating point arithmetic` |
|        - |   98 | `	 * is omitted from the build.` |
|        - |   99 | `	 */` |
|        - |  100 | `	return pObj->rVal;` |
|        - |  101 | `#else` |
|        - |  102 | ` /*` |
|        - |  103 | `  ** Many compilers we encounter do not define constants for the` |
|        - |  104 | `  ** minimum and maximum 64-bit integers, or they define them` |
|        - |  105 | `  ** inconsistently.  And many do not understand the "LL" notation.` |
|        - |  106 | `  ** So we define our own static constants here using nothing` |
|        - |  107 | `  ** larger than a 32-bit integer constant.` |
|        - |  108 | `  */` |
|        - |  109 | `  static const sxi64 maxInt = LARGEST_INT64;` |
|        - |  110 | `  static const sxi64 minInt = SMALLEST_INT64;` |
|    10781 |  111 | `  ph7_real r = pObj->rVal;` |
|    10781 |  112 | `  if( r<(ph7_real)minInt ){` |
|        3 |  113 | `    return minInt;` |
|    10779 |  114 | `  }else if( r>(ph7_real)maxInt ){` |
|        - |  115 | `    /* minInt is correct here - not maxInt.  It turns out that assigning` |
|        - |  116 | `    ** a very large positive number to an integer results in a very large` |
|        - |  117 | `    ** negative integer.  This makes no sense, but it is what x86 hardware` |
|        - |  118 | `    ** does so for compatibility we will do the same in software. */` |
|      180 |  119 | `    return minInt;` |
|      ! 0 |  120 | `  }else{` |
|    10601 |  121 | `    return (sxi64)r;` |
|        - |  122 | `  }` |
|        - |  123 | `#endif` |
|     5393 |  124 | `}` |
|        - |  125 | `/*` |
|        - |  126 | ` * Convert a raw token value typically a stream of digit [i.e: hex,octal,binary or decimal]` |
|        - |  127 | ` * to a 64-bit integer.` |
|        - |  128 | ` */` |
|  3822540 |  129 | `PH7_PRIVATE sxi64 PH7_TokenValueToInt64(SyString *pVal)` |
|        5 |  130 | `{` |
|  3822545 |  131 | `	sxi64 iVal = 0;` |
|  3822545 |  132 | `	if( pVal->nByte <= 0 ){` |
|      ! 0 |  133 | `		return 0;` |
|        - |  134 | `	}` |
|  3822545 |  135 | `	if( pVal->zString[0] == '0' ){` |
|        - |  136 | `		sxi32 c;` |
|  1502831 |  137 | `		if( pVal->nByte == sizeof(char) ){` |
|  1393575 |  138 | `			return 0;` |
|        - |  139 | `		}` |
|   109261 |  140 | `		c = pVal->zString[1];` |
|   109261 |  141 | `		if( c  == 'x' \|\| c == 'X' ){` |
|        - |  142 | `			/* Hex digit stream */` |
|   105011 |  143 | `			SyHexStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|    56758 |  144 | `		}else if( c == 'b' \|\| c == 'B' ){` |
|        - |  145 | `			/* Binary digit stream */` |
|      281 |  146 | `			SyBinaryStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|     4115 |  147 | `		}else if( c == 'o' \|\| c == 'O' ){` |
|        - |  148 | `			/* PHP 8.1 explicit octal 0o/0O: skip the two-char prefix and parse the` |
|        - |  149 | `			 * remaining octal digits (SyOctalStrToInt64 expects no letter prefix). */` |
|       17 |  150 | `			if( pVal->nByte > 2 ){` |
|       17 |  151 | `				SyOctalStrToInt64(pVal->zString + 2,pVal->nByte - 2,(void *)&iVal,0);` |
|        8 |  152 | `			}` |
|        9 |  153 | `		}else{` |
|        - |  154 | `			/* Legacy octal digit stream (leading 0) */` |
|     3959 |  155 | `			SyOctalStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|        - |  156 | `		}` |
|    54633 |  157 | `	}else{` |
|        - |  158 | `		/* Decimal digit stream */` |
|  2319719 |  159 | `		SyStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|        - |  160 | `	}` |
|  2428975 |  161 | `	return iVal;` |
|  1911275 |  162 | `}` |
|        - |  163 | `/*` |
|        - |  164 | ` * Return some kind of 64-bit integer value which is the best we can` |
|        - |  165 | ` * do at representing the value that pObj describes as a string` |
|        - |  166 | ` * representation.` |
|        - |  167 | ` */` |
|     3701 |  168 | `static sxi64 MemObjStringToInt(ph7_value *pObj)` |
|        5 |  169 | `{` |
|     3706 |  170 | `	sxi64 iVal = 0;` |
|        - |  171 | `	/* A *string* is always read in base 10 by php: "012" is 12, "0x1A" and "0b11"` |
|        - |  172 | `	 * are 0. Only a source *literal* carries a base prefix, and that is decoded by` |
|        - |  173 | `	 * the compiler (PH7_TokenValueToInt64) -- not here. */` |
|     3706 |  174 | `	SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&iVal,0);` |
|     3706 |  175 | `	return iVal;` |
|        5 |  176 | `}` |
|        - |  177 | `/*` |
|        - |  178 | ` * Call a magic class method [i.e: __toString(),__toInt(),...]` |
|        - |  179 | ` * Return SXRET_OK if the magic method is available and have been` |
|        - |  180 | ` * successfully called. Any other return value indicates failure.` |
|        - |  181 | ` */` |
|      370 |  182 | `static sxi32 MemObjCallClassCastMethod(` |
|        - |  183 | `	ph7_vm *pVm,               /* VM that trigger the invocation */` |
|        - |  184 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object] */` |
|        - |  185 | `	const char *zMethod,       /* Magic method name [i.e: __toString] */` |
|        - |  186 | `	sxu32 nLen,                /* Method name length */` |
|        - |  187 | `	ph7_value *pResult         /* OUT: Store the return value of the magic method here */` |
|        - |  188 | `	)` |
|        5 |  189 | `{` |
|        - |  190 | `	ph7_class_method *pMethod;` |
|        - |  191 | `	/* Check if the method is available */` |
|      375 |  192 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,zMethod,nLen);` |
|      375 |  193 | `	if( pMethod == 0 ){` |
|        - |  194 | `		/* No such method */` |
|      188 |  195 | `		return SXERR_NOTFOUND;` |
|        - |  196 | `	}` |
|        - |  197 | `	/* Invoke the desired method */` |
|      189 |  198 | `	PH7_VmCallClassMethod(&(*pVm),&(*pThis),pMethod,&(*pResult),0,0);` |
|        - |  199 | `	/* Method successfully called,pResult should hold the return value */` |
|      189 |  200 | `	return SXRET_OK;` |
|      190 |  201 | `}` |
|        - |  202 | `/*` |
|        - |  203 | ` * Return some kind of integer value which is the best we can` |
|        - |  204 | ` * do at representing the value that pObj describes as an integer.` |
|        - |  205 | ` * If pObj is an integer, then the value is exact. If pObj is` |
|        - |  206 | ` * a floating-point then  the value returned is the integer part.` |
|        - |  207 | ` * If pObj is a string, then we make an attempt to convert it into` |
|        - |  208 | ` * a integer and return that.` |
|        - |  209 | ` * If pObj represents a NULL value, return 0.` |
|        - |  210 | ` */` |
|     1562 |  211 | `static sxi64 MemObjIntValue(ph7_value *pObj)` |
|        5 |  212 | `{` |
|        - |  213 | `	sxi32 iFlags;` |
|     1567 |  214 | `	iFlags = pObj->iFlags;` |
|     1567 |  215 | `	if (iFlags & MEMOBJ_REAL ){` |
|       23 |  216 | `		return MemObjRealToInt(&(*pObj));` |
|     1547 |  217 | `	}else if( iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      168 |  218 | `		return pObj->x.iVal;` |
|     1381 |  219 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|     1365 |  220 | `		return MemObjStringToInt(&(*pObj));` |
|       17 |  221 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|        5 |  222 | `		return 0;` |
|       13 |  223 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|        7 |  224 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|        7 |  225 | `		sxu32 n = pMap->nEntry;` |
|        7 |  226 | `		PH7_HashmapUnref(pMap);` |
|        - |  227 | `		/* Return total number of entries in the hashmap */` |
|        7 |  228 | `		return n;` |
|        7 |  229 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|        - |  230 | `		ph7_value sResult;` |
|        5 |  231 | `		sxi64 iVal = 1;` |
|        - |  232 | `		sxi32 rc;` |
|        - |  233 | `		/* Invoke the [__toInt()] magic method if available [note that this is a symisc extension]  */` |
|        5 |  234 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|        5 |  235 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|        - |  236 | `			"__toInt",sizeof("__toInt")-1,&sResult);` |
|        5 |  237 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_INT) ){` |
|        - |  238 | `			/* Extract method return value */` |
|        5 |  239 | `			iVal = sResult.x.iVal;` |
|        2 |  240 | `		}` |
|        5 |  241 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|        5 |  242 | `		PH7_MemObjRelease(&sResult);` |
|        5 |  243 | `		return iVal;` |
|        3 |  244 | `	}else if(iFlags & MEMOBJ_RES ){` |
|        - |  245 | `		/* php casts a resource to its ID, not to 1: two distinct resources must not` |
|        - |  246 | `		 * compare equal, which they did while every one of them cast to 1. */` |
|        3 |  247 | `		return (sxi64)PH7_VmResourceId(pObj->pVm,pObj->x.pOther);` |
|        - |  248 | `	}` |
|        - |  249 | `	/* CANT HAPPEN */` |
|      ! 0 |  250 | `	return 0;` |
|      786 |  251 | `}` |
|        - |  252 | `/*` |
|        - |  253 | ` * Return some kind of real value which is the best we can` |
|        - |  254 | ` * do at representing the value that pObj describes as a real.` |
|        - |  255 | ` * If pObj is a real, then the value is exact.If pObj is an` |
|        - |  256 | ` * integer then the integer  is promoted to real and that value` |
|        - |  257 | ` * is returned.` |
|        - |  258 | ` * If pObj is a string, then we make an attempt to convert it` |
|        - |  259 | ` * into a real and return that.` |
|        - |  260 | ` * If pObj represents a NULL value, return 0.0` |
|        - |  261 | ` */` |
|     9602 |  262 | `static ph7_real MemObjRealValue(ph7_value *pObj)` |
|        5 |  263 | `{` |
|        - |  264 | `	sxi32 iFlags;` |
|     9607 |  265 | `	iFlags = pObj->iFlags;` |
|     9607 |  266 | `	if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  267 | `		return pObj->rVal;` |
|     9607 |  268 | `	}else if (iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      767 |  269 | `		return (ph7_real)pObj->x.iVal;` |
|     8843 |  270 | `	}else if (iFlags & MEMOBJ_STRING){` |
|        - |  271 | `		SyString sString;` |
|        - |  272 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  273 | `		ph7_real rVal = 0;` |
|        - |  274 | `#else` |
|     8837 |  275 | `		ph7_real rVal = 0.0;` |
|        - |  276 | `#endif` |
|     8837 |  277 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|     8837 |  278 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|        - |  279 | `			/* Convert as much as we can */` |
|        - |  280 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  281 | `			rVal = MemObjStringToInt(&(*pObj));` |
|        - |  282 | `#else` |
|     8837 |  283 | `			SyStrToReal(sString.zString,sString.nByte,(void *)&rVal,0);` |
|        - |  284 | `#endif` |
|     4416 |  285 | `		}` |
|     8837 |  286 | `		return rVal;` |
|        7 |  287 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|        - |  288 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  289 | `		return 0;` |
|        - |  290 | `#else` |
|      ! 0 |  291 | `		return 0.0;` |
|        - |  292 | `#endif` |
|        7 |  293 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|        - |  294 | `		/* Return the total number of entries in the hashmap */` |
|      ! 0 |  295 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      ! 0 |  296 | `		ph7_real n = (ph7_real)pMap->nEntry;` |
|      ! 0 |  297 | `		PH7_HashmapUnref(pMap);` |
|      ! 0 |  298 | `		return n;` |
|        7 |  299 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|        - |  300 | `		ph7_value sResult;` |
|        5 |  301 | `		ph7_real rVal = 1;` |
|        - |  302 | `		sxi32 rc;` |
|        - |  303 | `		/* Invoke the [__toFloat()] magic method if available [note that this is a symisc extension]  */` |
|        5 |  304 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|        5 |  305 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|        - |  306 | `			"__toFloat",sizeof("__toFloat")-1,&sResult);` |
|        5 |  307 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_REAL) ){` |
|        - |  308 | `			/* Extract method return value */` |
|        5 |  309 | `			rVal = sResult.rVal;` |
|        2 |  310 | `		}` |
|        5 |  311 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|        5 |  312 | `		PH7_MemObjRelease(&sResult);` |
|        5 |  313 | `		return rVal;` |
|        3 |  314 | `	}else if(iFlags & MEMOBJ_RES ){` |
|        3 |  315 | `		return (ph7_real)PH7_VmResourceId(pObj->pVm,pObj->x.pOther);` |
|        - |  316 | `	}` |
|        - |  317 | `	/* NOT REACHED  */` |
|      ! 0 |  318 | `	return 0;` |
|     4806 |  319 | `}` |
|        - |  320 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |  321 | `/*` |
|        - |  322 | ` * Post-process a libc-formatted float into php's exact shape (php_gcvt /` |
|        - |  323 | ` * smart_str_append_double semantics): strip the exponent's zero padding` |
|        - |  324 | ` * (libc's 1e+08 becomes php's 1e+8; a zero exponent stays e+0) and, when` |
|        - |  325 | ` * bGeneric is set (%g-style output, including the default float->string` |
|        - |  326 | ` * cast), make an exponent-form mantissa keep a fractional digit` |
|        - |  327 | ` * (1e+20 -> 1.0e+20). zBuf must be NUL-terminated with at least two bytes` |
|        - |  328 | ` * of spare capacity past the NUL. Returns the new length.` |
|        - |  329 | ` * Defined here (not builtin.c) because the float->string cast below needs it` |
|        - |  330 | ` * even when builtin.c's formatting region is compiled out` |
|        - |  331 | ` * (PH7_DISABLE_DISK_IO); the printf family reuses it from PH7_InputFormat.` |
|        - |  332 | ` */` |
|      516 |  333 | `PH7_PRIVATE sxi32 PH7_PhpFloatShape(char *zBuf,sxi32 nLen,int bGeneric)` |
|        5 |  334 | `{` |
|        - |  335 | `	sxi32 iExp,i;` |
|      521 |  336 | `	iExp = nLen - 1;` |
|     4427 |  337 | `	while( iExp > 0 && zBuf[iExp] != 'e' && zBuf[iExp] != 'E' ){` |
|     3911 |  338 | `		iExp--;` |
|        5 |  339 | `	}` |
|      521 |  340 | `	if( iExp <= 0 ){` |
|      475 |  341 | `		return nLen; /* No exponent part (fixed notation) */` |
|        - |  342 | `	}` |
|        - |  343 | `	{` |
|       47 |  344 | `		sxi32 iDig = iExp + 1;` |
|        - |  345 | `		sxi32 iFirst;` |
|       47 |  346 | `		if( zBuf[iDig] == '+' \|\| zBuf[iDig] == '-' ){` |
|       47 |  347 | `			iDig++;` |
|       23 |  348 | `		}` |
|       47 |  349 | `		iFirst = iDig;` |
|       83 |  350 | `		while( zBuf[iFirst] == '0' && iFirst + 1 < nLen` |
|       61 |  351 | `		 && zBuf[iFirst+1] >= '0' && zBuf[iFirst+1] <= '9' ){` |
|       25 |  352 | `			iFirst++;` |
|        1 |  353 | `		}` |
|       47 |  354 | `		if( iFirst > iDig ){` |
|       25 |  355 | `			sxi32 nStrip = iFirst - iDig;` |
|       73 |  356 | `			for( i = iDig ; i + nStrip <= nLen ; i++ ){` |
|       49 |  357 | `				zBuf[i] = zBuf[i+nStrip]; /* moves the NUL too */` |
|       25 |  358 | `			}` |
|       25 |  359 | `			nLen -= nStrip;` |
|       12 |  360 | `		}` |
|        - |  361 | `	}` |
|       47 |  362 | `	if( bGeneric ){` |
|       31 |  363 | `		int bHasDot = 0;` |
|       63 |  364 | `		for( i = 0 ; i < iExp ; i++ ){` |
|       45 |  365 | `			if( zBuf[i] == '.' ){ bHasDot = 1; break; }` |
|       17 |  366 | `		}` |
|       31 |  367 | `		if( !bHasDot ){` |
|      107 |  368 | `			for( i = nLen ; i >= iExp ; i-- ){` |
|       89 |  369 | `				zBuf[i+2] = zBuf[i]; /* moves the NUL too */` |
|       45 |  370 | `			}` |
|       19 |  371 | `			zBuf[iExp] = '.';` |
|       19 |  372 | `			zBuf[iExp+1] = '0';` |
|       19 |  373 | `			nLen += 2;` |
|        9 |  374 | `		}` |
|       15 |  375 | `	}` |
|       47 |  376 | `	return nLen;` |
|      263 |  377 | `}` |
|        - |  378 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|        - |  379 | `/*` |
|        - |  380 | ` * Return the string representation of a given ph7_value.` |
|        - |  381 | ` * This function never fail and always return SXRET_OK.` |
|        - |  382 | ` */` |
|    61928 |  383 | `static sxi32 MemObjStringValue(SyBlob *pOut,ph7_value *pObj,sxu8 bStrictBool)` |
|        5 |  384 | `{` |
|    61933 |  385 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|        - |  386 | `		/* Handle special floating-point values first */` |
|      376 |  387 | `		if( PH7_IS_NAN(pObj->rVal) ){` |
|      ! 0 |  388 | `			SyBlobAppend(&(*pOut),"NAN",3);` |
|      376 |  389 | `		}else if( PH7_IS_INF(pObj->rVal) ){` |
|        5 |  390 | `			if( pObj->rVal < 0.0 ){` |
|      ! 0 |  391 | `				SyBlobAppend(&(*pOut),"-INF",4);` |
|      ! 0 |  392 | `			}else{` |
|        5 |  393 | `				SyBlobAppend(&(*pOut),"INF",3);` |
|        - |  394 | `			}` |
|        3 |  395 | `		}else{` |
|        - |  396 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |  397 | `			/* php's default float->string conversion (echo/concat/cast):` |
|        - |  398 | `			 * zend_gcvt with EG(precision)=14 and an uppercase exponent` |
|        - |  399 | `			 * marker (smart_str_append_double) — 1/3 -> "0.33333333333333",` |
|        - |  400 | `			 * 1e15 -> "1.0E+15", -0.0 -> "-0". libc snprintf supplies` |
|        - |  401 | `			 * correctly-rounded digits; PH7_PhpFloatShape applies php's` |
|        - |  402 | `			 * exponent/fraction quirks. */` |
|        - |  403 | `			char zNum[48]; /* %.14G peaks at ~22 bytes; +2 spare for ".0" */` |
|      372 |  404 | `			sxi32 n = (sxi32)snprintf(zNum,sizeof(zNum),"%.14G",pObj->rVal);` |
|      372 |  405 | `			if( n < 0 \|\| n >= (sxi32)sizeof(zNum) ){` |
|      ! 0 |  406 | `				n = (sxi32)SyStrlen(zNum);` |
|      ! 0 |  407 | `			}` |
|      372 |  408 | `			n = PH7_PhpFloatShape(zNum,n,TRUE);` |
|      372 |  409 | `			SyBlobAppend(&(*pOut),zNum,(sxu32)n);` |
|        - |  410 | `#else` |
|        - |  411 | `			SyBlobFormat(&(*pOut),"%.15g",pObj->rVal);` |
|        - |  412 | `#endif` |
|        4 |  413 | `		}` |
|    61747 |  414 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|    61273 |  415 | `		SyBlobFormat(&(*pOut),"%qd",pObj->x.iVal);` |
|        - |  416 | `		/* %qd (BSD quad) is equivalent to %lld in the libc printf */` |
|    30927 |  417 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|       47 |  418 | `		if( bStrictBool ){` |
|        - |  419 | `			/* Actual string cast: true -> "1", false -> "" (like PHP) */` |
|       47 |  420 | `			if( pObj->x.iVal ){` |
|       34 |  421 | `				SyBlobAppend(&(*pOut),"1",sizeof("1")-1);` |
|       16 |  422 | `			}` |
|        - |  423 | `			/* false produces empty string, nothing to append */` |
|       26 |  424 | `		}else{` |
|        - |  425 | `			/* Display path (var_dump, print_r): show TRUE/FALSE */` |
|      ! 0 |  426 | `			if( pObj->x.iVal ){` |
|      ! 0 |  427 | `				SyBlobAppend(&(*pOut),"TRUE",sizeof("TRUE")-1);` |
|      ! 0 |  428 | `			}else{` |
|      ! 0 |  429 | `				SyBlobAppend(&(*pOut),"FALSE",sizeof("FALSE")-1);` |
|        - |  430 | `			}` |
|        5 |  431 | `		}` |
|      272 |  432 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|      ! 0 |  433 | `		SyBlobAppend(&(*pOut),"Array",sizeof("Array")-1);` |
|      ! 0 |  434 | `		PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
|      251 |  435 | `	}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|        - |  436 | `		ph7_value sResult;` |
|        - |  437 | `		sxi32 rc;` |
|        - |  438 | `		/* Invoke the __toString() method if available */` |
|      179 |  439 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|      179 |  440 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|        - |  441 | `			"__toString",sizeof("__toString")-1,&sResult);` |
|      179 |  442 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_STRING) && SyBlobLength(&sResult.sBlob) > 0){` |
|        - |  443 | `			/* Expand method return value */` |
|      101 |  444 | `			SyBlobDup(&sResult.sBlob,pOut);` |
|       53 |  445 | `		}else{` |
|        - |  446 | `			/* Expand "Object" as requested by the PHP language reference manual */` |
|       81 |  447 | `			SyBlobAppend(&(*pOut),"Object",sizeof("Object")-1);` |
|        - |  448 | `		}` |
|      179 |  449 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|      179 |  450 | `		PH7_MemObjRelease(&sResult);` |
|      162 |  451 | `	}else if(pObj->iFlags & MEMOBJ_RES ){` |
|        - |  452 | `		/* php renders a resource as "Resource id #N" with its sequential id; the` |
|        - |  453 | `		 * old "ResourceID_0x<pointer>" leaked an address and matched nothing. */` |
|        5 |  454 | `		SyBlobFormat(&(*pOut),"Resource id #%u",PH7_VmResourceId(pObj->pVm,pObj->x.pOther));` |
|        2 |  455 | `	}` |
|    61933 |  456 | `	return SXRET_OK;` |
|        5 |  457 | `}` |
|        - |  458 | `/*` |
|        - |  459 | ` * Return some kind of boolean value which is the best we can do` |
|        - |  460 | ` * at representing the value that pObj describes as a boolean.` |
|        - |  461 | ` * When converting to boolean, the following values are considered FALSE` |
|        - |  462 | ` * (php's exact set):` |
|        - |  463 | ` * NULL` |
|        - |  464 | ` * the boolean FALSE itself.` |
|        - |  465 | ` * the integer 0 (zero).` |
|        - |  466 | ` * the real 0.0 (zero).` |
|        - |  467 | ` * the empty string "" and the string "0" (nothing else: "00", "0.0", " ",` |
|        - |  468 | ` * and "false" are all TRUE in php — the historical PH7 zero-stream and` |
|        - |  469 | ` * "false"/"on"/"yes" special cases changed the meaning of valid PHP source` |
|        - |  470 | ` * and were removed under the §10 PH7-ism policy).` |
|        - |  471 | ` * an array with zero elements.` |
|        - |  472 | ` */` |
|    46856 |  473 | `static sxi32 MemObjBooleanValue(ph7_value *pObj)` |
|        5 |  474 | `{` |
|        - |  475 | `	sxi32 iFlags;` |
|    46861 |  476 | `	iFlags = pObj->iFlags;` |
|    46861 |  477 | `	if (iFlags & MEMOBJ_REAL ){` |
|        - |  478 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  479 | `		return pObj->rVal ? 1 : 0;` |
|        - |  480 | `#else` |
|       14 |  481 | `		return pObj->rVal != 0.0 ? 1 : 0;` |
|        - |  482 | `#endif` |
|    46849 |  483 | `	}else if( iFlags & MEMOBJ_INT ){` |
|      921 |  484 | `		return pObj->x.iVal ? 1 : 0;` |
|    45933 |  485 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|        - |  486 | `		SyString sString;` |
|       89 |  487 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|        - |  488 | `		/* php: a string is FALSE iff it is empty or exactly "0" */` |
|       89 |  489 | `		if( sString.nByte == 0 ){` |
|       19 |  490 | `			return 0;` |
|        - |  491 | `		}` |
|       72 |  492 | `		if( sString.nByte == 1 && sString.zString[0] == '0' ){` |
|        7 |  493 | `			return 0;` |
|        - |  494 | `		}` |
|       66 |  495 | `		return 1;` |
|    45847 |  496 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|    44547 |  497 | `		return 0;` |
|     1305 |  498 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|       20 |  499 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|       20 |  500 | `		sxu32 n = pMap->nEntry;` |
|       20 |  501 | `		PH7_HashmapUnref(pMap);` |
|       20 |  502 | `		return n > 0 ? TRUE : FALSE;` |
|     1287 |  503 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|        - |  504 | `		ph7_value sResult;` |
|      190 |  505 | `		sxi32 iVal = 1;` |
|        - |  506 | `		sxi32 rc;` |
|        - |  507 | `		/* Invoke the __toBool() method if available [note that this is a symisc extension]  */` |
|      190 |  508 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|      190 |  509 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|        - |  510 | `			"__toBool",sizeof("__toBool")-1,&sResult);` |
|      190 |  511 | `		if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL)) ){` |
|        - |  512 | `			/* Extract method return value */` |
|        5 |  513 | `			iVal = (sxi32)(sResult.x.iVal != 0); /* Stupid cc warning -W -Wall -O6 */` |
|        2 |  514 | `		}` |
|      190 |  515 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|      190 |  516 | `		PH7_MemObjRelease(&sResult);` |
|      190 |  517 | `		return iVal;` |
|     1099 |  518 | `	}else if(iFlags & MEMOBJ_RES ){` |
|     1099 |  519 | `		return pObj->x.pOther != 0;` |
|        - |  520 | `	}` |
|        - |  521 | `	/* NOT REACHED */` |
|      ! 0 |  522 | `	return 0;` |
|    23433 |  523 | `}` |
|        - |  524 | `/*` |
|        - |  525 | ` * If the ph7_value is of type real,try to make it an integer also.` |
|        - |  526 | ` */` |
|    10756 |  527 | `static sxi32 MemObjTryIntger(ph7_value *pObj)` |
|        5 |  528 | `{` |
|    10761 |  529 | `	pObj->x.iVal = MemObjRealToInt(&(*pObj));` |
|        - |  530 | `  /* Only mark the value as an integer if` |
|        - |  531 | `  **` |
|        - |  532 | `  **    (1) the round-trip conversion real->int->real is a no-op, and` |
|        - |  533 | `  **    (2) The integer is neither the largest nor the smallest` |
|        - |  534 | `  **        possible integer` |
|        - |  535 | `  **` |
|        - |  536 | `  ** The second and third terms in the following conditional enforces` |
|        - |  537 | `  ** the second condition under the assumption that addition overflow causes` |
|        - |  538 | `  ** values to wrap around.  On x86 hardware, the third term is always` |
|        - |  539 | `  ** true and could be omitted.  But we leave it in because other` |
|        - |  540 | `  ** architectures might behave differently.` |
|        - |  541 | `  */` |
|    10756 |  542 | `	if( pObj->rVal ==(ph7_real)pObj->x.iVal && pObj->x.iVal>SMALLEST_INT64` |
|     9327 |  543 | `      && pObj->x.iVal<LARGEST_INT64 ){` |
|     9311 |  544 | `		  pObj->iFlags \|= MEMOBJ_INT;` |
|     4655 |  545 | `	}` |
|    10761 |  546 | `	return SXRET_OK;` |
|        5 |  547 | `}` |
|        - |  548 | `/*` |
|        - |  549 | ` * Convert a ph7_value to type integer.Invalidate any prior representations.` |
|        - |  550 | ` */` |
|   530625 |  551 | `PH7_PRIVATE sxi32 PH7_MemObjToInteger(ph7_value *pObj)` |
|        5 |  552 | `{` |
|   530630 |  553 | `	if( (pObj->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  554 | `		/* Preform the conversion */` |
|     1567 |  555 | `		pObj->x.iVal = MemObjIntValue(&(*pObj));` |
|        - |  556 | `		/* Invalidate any prior representations */` |
|     1567 |  557 | `		SyBlobRelease(&pObj->sBlob);` |
|     1567 |  558 | `		MemObjSetType(pObj,MEMOBJ_INT);` |
|      781 |  559 | `	}` |
|   530630 |  560 | `	return SXRET_OK;` |
|        5 |  561 | `}` |
|        - |  562 | `/*` |
|        - |  563 | ` * Convert a ph7_value to type real (Try to get an integer representation also).` |
|        - |  564 | ` * Invalidate any prior representations` |
|        - |  565 | ` */` |
|    10594 |  566 | `PH7_PRIVATE sxi32 PH7_MemObjToReal(ph7_value *pObj)` |
|        5 |  567 | `{` |
|    10599 |  568 | `	if((pObj->iFlags & MEMOBJ_REAL) == 0 ){` |
|        - |  569 | `		/* Preform the conversion */` |
|     9607 |  570 | `		pObj->rVal = MemObjRealValue(&(*pObj));` |
|        - |  571 | `		/* Invalidate any prior representations */` |
|     9607 |  572 | `		SyBlobRelease(&pObj->sBlob);` |
|     9607 |  573 | `		MemObjSetType(pObj,MEMOBJ_REAL);` |
|        - |  574 | `		/* Try to get an integer representation */` |
|     9607 |  575 | `		MemObjTryIntger(&(*pObj));` |
|     4801 |  576 | `	}` |
|    10599 |  577 | `	return SXRET_OK;` |
|        5 |  578 | `}` |
|        - |  579 | `/*` |
|        - |  580 | ` * Convert a ph7_value to type boolean.Invalidate any prior representations.` |
|        - |  581 | ` */` |
|    51052 |  582 | `PH7_PRIVATE sxi32 PH7_MemObjToBool(ph7_value *pObj)` |
|        5 |  583 | `{` |
|    51057 |  584 | `	if( (pObj->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        - |  585 | `		/* Preform the conversion */` |
|    46861 |  586 | `		pObj->x.iVal = MemObjBooleanValue(&(*pObj));` |
|        - |  587 | `		/* Invalidate any prior representations */` |
|    46861 |  588 | `		SyBlobRelease(&pObj->sBlob);` |
|    46861 |  589 | `		MemObjSetType(pObj,MEMOBJ_BOOL);` |
|    23428 |  590 | `	}` |
|    51057 |  591 | `	return SXRET_OK;` |
|        5 |  592 | `}` |
|        - |  593 | `/*` |
|        - |  594 | ` * Convert a ph7_value to type string.Prior representations are NOT invalidated.` |
|        - |  595 | ` */` |
|   959955 |  596 | `PH7_PRIVATE sxi32 PH7_MemObjToString(ph7_value *pObj)` |
|        5 |  597 | `{` |
|   959960 |  598 | `	sxi32 rc = SXRET_OK;` |
|   959960 |  599 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  600 | `		/* Perform the conversion */` |
|    61835 |  601 | `		SyBlobReset(&pObj->sBlob); /* Reset the internal buffer */` |
|    61835 |  602 | `		rc = MemObjStringValue(&pObj->sBlob,&(*pObj),TRUE);` |
|    61835 |  603 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|    30915 |  604 | `	}` |
|   959960 |  605 | `	return rc;` |
|        5 |  606 | `}` |
|        - |  607 | `/*` |
|        - |  608 | ` * Nullify a ph7_value.In other words invalidate any prior` |
|        - |  609 | ` * representation.` |
|        - |  610 | ` */` |
|      ! 0 |  611 | `PH7_PRIVATE sxi32 PH7_MemObjToNull(ph7_value *pObj)` |
|      ! 0 |  612 | `{` |
|      ! 0 |  613 | `	return PH7_MemObjRelease(pObj);` |
|      ! 0 |  614 | `}` |
|        - |  615 | `/*` |
|        - |  616 | ` * Convert a ph7_value to type array.Invalidate any prior representations.` |
|        - |  617 | `  * According to the PHP language reference manual.` |
|        - |  618 | `  *   For any of the types: integer, float, string, boolean converting a value` |
|        - |  619 | `  *   to an array results in an array with a single element with index zero` |
|        - |  620 | `  *   and the value of the scalar which was converted.` |
|        - |  621 | `  */` |
|      570 |  622 | `PH7_PRIVATE sxi32 PH7_MemObjToHashmap(ph7_value *pObj)` |
|        5 |  623 | `{` |
|      575 |  624 | `	if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  625 | `		ph7_hashmap *pMap;` |
|        - |  626 | `		/* Allocate a new hashmap instance */` |
|      377 |  627 | `		pMap = PH7_NewHashmap(pObj->pVm,0,0);` |
|      377 |  628 | `		if( pMap == 0 ){` |
|      ! 0 |  629 | `			return SXERR_MEM;` |
|        - |  630 | `		}` |
|      377 |  631 | `		if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|        - |  632 | `			/*` |
|        - |  633 | `			 * According to the PHP language reference manual.` |
|        - |  634 | `			 *   For any of the types: integer, float, string, boolean converting a value` |
|        - |  635 | `			 *   to an array results in an array with a single element with index zero` |
|        - |  636 | `			 *   and the value of the scalar which was converted.` |
|        - |  637 | `			 */` |
|       35 |  638 | `			if( pObj->iFlags & MEMOBJ_OBJ ){` |
|        - |  639 | `				/* Object cast */` |
|       23 |  640 | `				PH7_ClassInstanceToHashmap((ph7_class_instance *)pObj->x.pOther,pMap);` |
|       13 |  641 | `			}else{` |
|        - |  642 | `				/* Insert a single element */` |
|       13 |  643 | `				PH7_HashmapInsert(pMap,0/* Automatic index assign */,&(*pObj));` |
|        - |  644 | `			}` |
|       35 |  645 | `			SyBlobRelease(&pObj->sBlob);` |
|       16 |  646 | `		}` |
|        - |  647 | `		/* Invalidate any prior representation */` |
|      377 |  648 | `		PH7_MemObjRelease(pObj);` |
|      377 |  649 | `		MemObjSetType(pObj,MEMOBJ_HASHMAP);` |
|      377 |  650 | `		pObj->x.pOther = pMap;` |
|      186 |  651 | `	}` |
|      575 |  652 | `	return SXRET_OK;` |
|      290 |  653 | `}` |
|        - |  654 | `/* Per-entry callback for the array branch of the (object) cast: add one dynamic` |
|        - |  655 | ` * property to the target stdClass, named by the array key (rendered as a string,` |
|        - |  656 | ` * matching PHP) and holding a copy of the value. */` |
|        - |  657 | `struct VmObjCastData { ph7_vm *pVm; ph7_class_instance *pStd; };` |
|       62 |  658 | `static int VmArrayToObjectWalk(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        3 |  659 | `{` |
|       65 |  660 | `	struct VmObjCastData *pData = (struct VmObjCastData *)pUserData;` |
|        - |  661 | `	ph7_value *pSlot;` |
|        - |  662 | `	/* pKey and pValue are walk-owned temporaries (PH7_HashmapWalk passes pointers to` |
|        - |  663 | `	 * its own stack-local sKey/sValue, not slots inside pVm->aMemObj), so they survive` |
|        - |  664 | `	 * the slot reservation inside PH7_VmCreateDynamicAttr — no snapshot needed. pKey is` |
|        - |  665 | `	 * safe to coerce in place. */` |
|       65 |  666 | `	PH7_MemObjToString(pKey);` |
|       96 |  667 | `	pSlot = PH7_VmCreateDynamicAttr(pData->pVm,pData->pStd,` |
|       62 |  668 | `		(const char *)SyBlobData(&pKey->sBlob),(sxu32)SyBlobLength(&pKey->sBlob),0);` |
|       65 |  669 | `	if( pSlot ){` |
|       65 |  670 | `		PH7_MemObjStore(pValue,pSlot);` |
|       31 |  671 | `	}` |
|       65 |  672 | `	return SXRET_OK;` |
|        3 |  673 | `}` |
|        - |  674 | `/*` |
|        - |  675 | ` * Convert a ph7_value to type object, invalidating any prior representation.` |
|        - |  676 | ` * The new object is a (PHP-empty) stdClass populated with dynamic properties,` |
|        - |  677 | ` * matching PHP's (object) cast:` |
|        - |  678 | ` *   - array  -> one property per entry (key rendered as a string -> name).` |
|        - |  679 | ` *   - scalar -> a single property named "scalar".` |
|        - |  680 | ` *   - null   -> an empty stdClass (no properties).` |
|        - |  681 | ` *   - object -> returned unchanged (the MEMOBJ_OBJ guard below).` |
|        - |  682 | ` */` |
|       46 |  683 | `PH7_PRIVATE sxi32 PH7_MemObjToObject(ph7_value *pObj)` |
|        3 |  684 | `{` |
|       49 |  685 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  686 | `		ph7_class_instance *pStd;` |
|        - |  687 | `		ph7_class *pClass;` |
|        - |  688 | `		ph7_vm *pVm;` |
|        - |  689 | `		/* Point to the underlying VM + the stdClass */` |
|       49 |  690 | `		pVm = pObj->pVm;` |
|       72 |  691 | `		pClass = pVm->pStdClass ? pVm->pStdClass` |
|       23 |  692 | `			: PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,0,0);` |
|       49 |  693 | `		if( pClass == 0 ){` |
|        - |  694 | `			/* Can't happen,load null instead */` |
|      ! 0 |  695 | `			PH7_MemObjRelease(pObj);` |
|      ! 0 |  696 | `			return SXRET_OK;` |
|        - |  697 | `		}` |
|        - |  698 | `		/* Instanciate a new (empty) stdClass object */` |
|       49 |  699 | `		pStd = PH7_NewClassInstance(pVm,pClass);` |
|       49 |  700 | `		if( pStd == 0 ){` |
|        - |  701 | `			/* Out of memory */` |
|      ! 0 |  702 | `			PH7_MemObjRelease(pObj);` |
|      ! 0 |  703 | `			return SXRET_OK;` |
|        - |  704 | `		}` |
|       49 |  705 | `		pStd->iRef = 1;` |
|       49 |  706 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  707 | `			/* Array: one dynamic property per entry. */` |
|        - |  708 | `			struct VmObjCastData sData;` |
|       37 |  709 | `			sData.pVm = pVm;` |
|       37 |  710 | `			sData.pStd = pStd;` |
|       37 |  711 | `			ph7_array_walk(pObj,VmArrayToObjectWalk,&sData);` |
|       30 |  712 | `		}else if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  713 | `			/* Scalar (int/float/bool/string): a single "scalar" property. */` |
|       11 |  714 | `			ph7_value *pSlot = PH7_VmCreateDynamicAttr(pVm,pStd,"scalar",sizeof("scalar")-1,0);` |
|       11 |  715 | `			if( pSlot ){` |
|       11 |  716 | `				PH7_MemObjStore(pObj,pSlot);` |
|        5 |  717 | `			}` |
|        5 |  718 | `		}` |
|        - |  719 | `		/* (A NULL source yields an empty stdClass — nothing to populate.) */` |
|        - |  720 | `		/* Invalidate any prior representation */` |
|       49 |  721 | `		PH7_MemObjRelease(pObj);` |
|        - |  722 | `		/* Save the new instance */` |
|       49 |  723 | `		pObj->x.pOther = pStd;` |
|       49 |  724 | `		MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       23 |  725 | `	}` |
|       49 |  726 | `	return SXRET_OK;` |
|       26 |  727 | `}` |
|        - |  728 | `/*` |
|        - |  729 | ` * Return a pointer to the appropriate convertion method associated` |
|        - |  730 | ` * with the given type.` |
|        - |  731 | ` * Note on type juggling.` |
|        - |  732 | ` * Accoding to the PHP language reference manual` |
|        - |  733 | ` *  PHP does not require (or support) explicit type definition in variable` |
|        - |  734 | ` *  declaration; a variable's type is determined by the context in which` |
|        - |  735 | ` *  the variable is used. That is to say, if a string value is assigned` |
|        - |  736 | ` *  to variable $var, $var becomes a string. If an integer value is then` |
|        - |  737 | ` *  assigned to $var, it becomes an integer.` |
|        - |  738 | ` */` |
|       82 |  739 | `PH7_PRIVATE ProcMemObjCast PH7_MemObjCastMethod(sxi32 iFlags)` |
|        5 |  740 | `{` |
|       87 |  741 | `	if( iFlags & MEMOBJ_STRING ){` |
|       16 |  742 | `		return PH7_MemObjToString;` |
|       73 |  743 | `	}else if( iFlags & MEMOBJ_INT ){` |
|       57 |  744 | `		return PH7_MemObjToInteger;` |
|       19 |  745 | `	}else if( iFlags & MEMOBJ_REAL ){` |
|       17 |  746 | `		return PH7_MemObjToReal;` |
|        3 |  747 | `	}else if( iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  748 | `		return PH7_MemObjToBool;` |
|        3 |  749 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|        3 |  750 | `		return PH7_MemObjToHashmap;` |
|      ! 0 |  751 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  752 | `		return PH7_MemObjToObject;` |
|      ! 0 |  753 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|        - |  754 | ``		/* `null` is a type, not a weak-coercion target: never silently cast a`` |
|        - |  755 | ``		 * value to null for a standalone `null` type hint. Return/property`` |
|        - |  756 | `		 * enforcement reject a non-null value before reaching here; this guards` |
|        - |  757 | `		 * the parameter default-value path from quietly nulling a non-null` |
|        - |  758 | `		 * default. */` |
|      ! 0 |  759 | `		return 0;` |
|        - |  760 | `	}` |
|        - |  761 | `	/* NULL cast */` |
|      ! 0 |  762 | `	return PH7_MemObjToNull;` |
|       46 |  763 | `}` |
|        - |  764 | `/*` |
|        - |  765 | ` * Return TRUE only if the entire string held by pValue (optionally surrounded` |
|        - |  766 | ` * by whitespace, with an optional sign) is a well-formed PHP numeric string.` |
|        - |  767 | ` * This mirrors PHP's is_numeric_string grammar used for is_numeric() and the` |
|        - |  768 | ` * loose-comparison numeric gate:` |
|        - |  769 | ` *` |
|        - |  770 | ` *   [ws] [sign] ( D+ [.D*] \| .D+ ) [ (e\|E) [sign] D+ ] [ws]   (whole string)` |
|        - |  771 | ` *` |
|        - |  772 | ` * Implemented directly rather than via SyStrIsNumeric — which returns OK on any` |
|        - |  773 | ` * numeric PREFIX (so it wrongly accepts "10abc"/"0x1A"/"0b101") and requires a` |
|        - |  774 | ` * leading digit (so it wrongly rejects ".5"/"-.5", valid in PHP). Unlike a` |
|        - |  775 | ` * strtod-based classifier this needs no NUL-terminated buffer. Returns FALSE for` |
|        - |  776 | ` * a non-string value.` |
|        - |  777 | ` */` |
|        - |  778 | `/*` |
|        - |  779 | ` * Scan php's numeric-string grammar and report the longest numeric PREFIX.` |
|        - |  780 | ` * Returns 1 when the string starts with a number, 0 when nothing numeric is` |
|        - |  781 | ` * there at all ("abc", "", ".", "e5"). On success *pzTail points just past the` |
|        - |  782 | ` * prefix, so the caller can tell a fully numeric string ("1e3", " 5 ") from a` |
|        - |  783 | ` * merely leading-numeric one ("5abc", "1e", "0x1A") -- php warns on the latter` |
|        - |  784 | ` * and rejects a string with no prefix outright.` |
|        - |  785 | ` */` |
|   249630 |  786 | `PH7_PRIVATE int PH7_MemObjStringNumericPrefix(ph7_value *pValue,const char **pzTail)` |
|        5 |  787 | `{` |
|        - |  788 | `	const char *z, *zEnd;` |
|        - |  789 | `	sxu32 n;` |
|   249635 |  790 | `	int bDigit = 0;` |
|   249635 |  791 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  792 | `		return 0;` |
|        - |  793 | `	}` |
|   249635 |  794 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|   249635 |  795 | `	n = SyBlobLength(&pValue->sBlob);` |
|   249635 |  796 | `	if( n == 0 ){` |
|      603 |  797 | `		return 0;` |
|        - |  798 | `	}` |
|   249035 |  799 | `	zEnd = z + n;` |
|   249059 |  800 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){` |
|       26 |  801 | `		z++;` |
|        2 |  802 | `	}` |
|   249035 |  803 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|      215 |  804 | `		z++;` |
|      105 |  805 | `	}` |
|   253890 |  806 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|     4860 |  807 | `		z++; bDigit = 1;` |
|        5 |  808 | `	}` |
|   249035 |  809 | `	if( z < zEnd && z[0] == '.' ){` |
|     6030 |  810 | `		z++;` |
|     6104 |  811 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|       78 |  812 | `			z++; bDigit = 1;` |
|        4 |  813 | `		}` |
|     3217 |  814 | `	}` |
|        - |  815 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|   249035 |  816 | `	if( !bDigit ){` |
|   244288 |  817 | `		return 0;` |
|        - |  818 | `	}` |
|        - |  819 | `	/* Optional exponent — only joins the prefix if it carries a digit. "1e" has` |
|        - |  820 | `	 * the numeric prefix "1" with the 'e' left in the tail, exactly as php reads it. */` |
|     4752 |  821 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|       30 |  822 | `		const char *zExp = z;` |
|       30 |  823 | `		z++;` |
|       30 |  824 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|      ! 0 |  825 | `			z++;` |
|      ! 0 |  826 | `		}` |
|       30 |  827 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|       10 |  828 | `			z = zExp;` |
|        6 |  829 | `		}else{` |
|       46 |  830 | `			while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|       26 |  831 | `				z++;` |
|        2 |  832 | `			}` |
|        - |  833 | `		}` |
|       14 |  834 | `	}` |
|     4752 |  835 | `	if( pzTail ){` |
|     4752 |  836 | `		*pzTail = z;` |
|     2373 |  837 | `	}` |
|     4752 |  838 | `	return 1;` |
|   124709 |  839 | `}` |
|        - |  840 | `/*` |
|        - |  841 | ` * TRUE only if the WHOLE string is a well-formed php numeric string` |
|        - |  842 | ` * (trailing whitespace allowed, nothing else).` |
|        - |  843 | ` */` |
|   247221 |  844 | `PH7_PRIVATE int PH7_MemObjStringIsNumeric(ph7_value *pValue)` |
|        5 |  845 | `{` |
|   247226 |  846 | `	const char *zTail = 0, *zEnd;` |
|   247226 |  847 | `	if( !PH7_MemObjStringNumericPrefix(pValue,&zTail) ){` |
|   244878 |  848 | `		return 0;` |
|        - |  849 | `	}` |
|     2353 |  850 | `	zEnd = (const char *)SyBlobData(&pValue->sBlob) + SyBlobLength(&pValue->sBlob);` |
|     2359 |  851 | `	while( zTail < zEnd && (unsigned char)zTail[0] < 0xc0 && SyisSpace(zTail[0]) ){` |
|        8 |  852 | `		zTail++;` |
|        2 |  853 | `	}` |
|     2353 |  854 | `	return zTail == zEnd ? 1 : 0;` |
|   123505 |  855 | `}` |
|        - |  856 | `/*` |
|        - |  857 | ` * Check whether the ph7_value is numeric [i.e: int/float/bool] or looks` |
|        - |  858 | ` * like a numeric number [i.e: if the ph7_value is of type string.].` |
|        - |  859 | ` * Return TRUE if numeric.FALSE otherwise.` |
|        - |  860 | ` */` |
|   248209 |  861 | `PH7_PRIVATE sxi32 PH7_MemObjIsNumeric(ph7_value *pObj)` |
|        5 |  862 | `{` |
|   248214 |  863 | `	if( pObj->iFlags & ( MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      679 |  864 | `		return TRUE;` |
|   247540 |  865 | `	}else if( pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|      436 |  866 | `		return FALSE;` |
|   247108 |  867 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        - |  868 | `		/* TRUE only if the whole string is a well-formed PHP numeric string. */` |
|   247108 |  869 | `		return PH7_MemObjStringIsNumeric(pObj) ? TRUE : FALSE;` |
|        - |  870 | `	}` |
|        - |  871 | `	/* NOT REACHED */` |
|      ! 0 |  872 | `	return FALSE;` |
|   123999 |  873 | `}` |
|        - |  874 | `/*` |
|        - |  875 | ` * Check whether the ph7_value is empty.Return TRUE if empty.` |
|        - |  876 | ` * FALSE otherwise.` |
|        - |  877 | ` * An ph7_value is considered empty if the following are true:` |
|        - |  878 | ` * NULL value.` |
|        - |  879 | ` * Boolean FALSE.` |
|        - |  880 | ` * Integer/Float with a 0 (zero) value.` |
|        - |  881 | ` * An empty string or a stream of 0 (zero) [i.e: "0","00","000",...].` |
|        - |  882 | ` * An empty array.` |
|        - |  883 | ` * NOTE` |
|        - |  884 | ` *  OBJECT VALUE MUST NOT BE MODIFIED.` |
|        - |  885 | ` */` |
|    39590 |  886 | `PH7_PRIVATE sxi32 PH7_MemObjIsEmpty(ph7_value *pObj)` |
|        5 |  887 | `{` |
|    39595 |  888 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|       21 |  889 | `		return TRUE;` |
|    39577 |  890 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|       22 |  891 | `		return pObj->x.iVal == 0 ? TRUE : FALSE;` |
|    39557 |  892 | `	}else if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  893 | `		return pObj->rVal == (ph7_real)0 ? TRUE : FALSE;` |
|    39557 |  894 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|        5 |  895 | `		return !pObj->x.iVal;` |
|    39553 |  896 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|    26443 |  897 | `		if( SyBlobLength(&pObj->sBlob) <= 0 ){` |
|    21077 |  898 | `			return TRUE;` |
|      ! 0 |  899 | `		}else{` |
|        - |  900 | `			const char *zIn,*zEnd;` |
|     5371 |  901 | `			zIn = (const char *)SyBlobData(&pObj->sBlob);` |
|     5371 |  902 | `			zEnd = &zIn[SyBlobLength(&pObj->sBlob)];` |
|     5377 |  903 | `			while( zIn < zEnd ){` |
|     5377 |  904 | `				if( zIn[0] != '0' ){` |
|     5371 |  905 | `					break;` |
|        - |  906 | `				}` |
|        7 |  907 | `				zIn++;` |
|        1 |  908 | `			}` |
|     5371 |  909 | `			return zIn >= zEnd ? TRUE : FALSE;` |
|      ! 0 |  910 | `		}` |
|    13115 |  911 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|    13115 |  912 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|    13115 |  913 | `		return pMap->nEntry == 0 ? TRUE : FALSE;` |
|      ! 0 |  914 | `	}else if ( pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|      ! 0 |  915 | `		return FALSE;` |
|        - |  916 | `	}` |
|        - |  917 | `	/* Assume empty by default */` |
|      ! 0 |  918 | `	return TRUE;` |
|    19800 |  919 | `}` |
|        - |  920 | `/*` |
|        - |  921 | ` * Convert a ph7_value so that it has types MEMOBJ_REAL or MEMOBJ_INT` |
|        - |  922 | ` * or both.` |
|        - |  923 | ` * Invalidate any prior representations. Every effort is made to force` |
|        - |  924 | ` * the conversion, even if the input is a string that does not look` |
|        - |  925 | ` * completely like a number.Convert as much of the string as we can` |
|        - |  926 | ` * and ignore the rest.` |
|        - |  927 | ` */` |
|   518296 |  928 | `PH7_PRIVATE sxi32 PH7_MemObjToNumeric(ph7_value *pObj)` |
|        5 |  929 | `{` |
|   518301 |  930 | `	if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|   515940 |  931 | `		if( pObj->iFlags & (MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|        8 |  932 | `			if( pObj->iFlags & MEMOBJ_NULL ){` |
|        5 |  933 | `				pObj->x.iVal = 0;` |
|        2 |  934 | `			}` |
|        8 |  935 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|        3 |  936 | `		}` |
|        - |  937 | `		/* Already numeric */` |
|   515940 |  938 | `		return  SXRET_OK;` |
|        - |  939 | `	}` |
|     2365 |  940 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|     2365 |  941 | `		const char *zTail = 0;` |
|     2365 |  942 | `		int bNum, bReal = 0;` |
|        - |  943 | `		/* php reads the longest numeric PREFIX and its shape decides the type: a` |
|        - |  944 | `		 * '.' or a *complete* exponent inside that prefix makes it a float, else an` |
|        - |  945 | `		 * int. Deciding from the raw string instead mistyped "1e" as float(1) --` |
|        - |  946 | `		 * php sees the prefix "1" there and yields int(1). */` |
|     2365 |  947 | `		bNum = PH7_MemObjStringNumericPrefix(pObj,&zTail);` |
|     2365 |  948 | `		if( bNum ){` |
|     2365 |  949 | `			const char *z = (const char *)SyBlobData(&pObj->sBlob);` |
|     4812 |  950 | `			while( z < zTail ){` |
|     2471 |  951 | `				if( z[0] == '.' \|\| z[0] == 'e' \|\| z[0] == 'E' ){` |
|       22 |  952 | `					bReal = 1;` |
|       22 |  953 | `					break;` |
|        - |  954 | `				}` |
|     2451 |  955 | `				z++;` |
|        4 |  956 | `			}` |
|     1180 |  957 | `		}` |
|     2365 |  958 | `		if( bReal ){` |
|       22 |  959 | `			PH7_MemObjToReal(&(*pObj));` |
|       12 |  960 | `		}else{` |
|     2345 |  961 | `			if( !bNum ){` |
|        - |  962 | `				/* The input does not look at all like a number,set the value to 0 */` |
|      ! 0 |  963 | `				pObj->x.iVal = 0;` |
|      ! 0 |  964 | `			}else{` |
|        - |  965 | `				/* Convert as much as we can */` |
|     2345 |  966 | `				pObj->x.iVal = MemObjStringToInt(&(*pObj));` |
|        - |  967 | `			}` |
|     2345 |  968 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|     2345 |  969 | `			SyBlobRelease(&pObj->sBlob);` |
|        4 |  970 | `		}` |
|     1180 |  971 | `	}else if(pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)){` |
|      ! 0 |  972 | `		PH7_MemObjToInteger(pObj);` |
|      ! 0 |  973 | `	}else{` |
|        - |  974 | `		/* Perform a blind cast */` |
|      ! 0 |  975 | `		PH7_MemObjToReal(&(*pObj));` |
|        - |  976 | `	}` |
|     2365 |  977 | `	return SXRET_OK;` |
|   259368 |  978 | `}` |
|        - |  979 | `/*` |
|        - |  980 | ` * Apply Perl-style increment to a string ph7_value in place.` |
|        - |  981 | ` * Walks the bytes right-to-left: digits 0-8 / letters a-y, A-Y bump in` |
|        - |  982 | ` * place; '9' wraps to '0' with carry; 'z' to 'a'; 'Z' to 'A'. A non-` |
|        - |  983 | ` * alphanumeric byte stops the walk without prepending. If carry survives` |
|        - |  984 | ` * past index 0, prepend '1', 'a', or 'A' depending on the class of the` |
|        - |  985 | ` * last carried character. Empty strings become "1".` |
|        - |  986 | ` *` |
|        - |  987 | ` * Caller must ensure pObj is MEMOBJ_STRING and not a numeric string;` |
|        - |  988 | ` * this routine never reclassifies the type, so a result like "e0" stays` |
|        - |  989 | ` * a string even though it looks numeric.` |
|        - |  990 | ` */` |
|      ! 0 |  991 | `PH7_PRIVATE sxi32 PH7_MemObjStringIncrement(ph7_value *pObj)` |
|      ! 0 |  992 | `{` |
|        - |  993 | `	enum CarryClass { CARRY_NONE = 0, CARRY_LOWER, CARRY_UPPER, CARRY_DIGIT };` |
|      ! 0 |  994 | `	enum CarryClass last_class = CARRY_NONE;` |
|        - |  995 | `	sxu32 nLen, pos;` |
|        - |  996 | `	sxu8 *zStr;` |
|      ! 0 |  997 | `	int carry = 1;` |
|        - |  998 | `	int ch;` |
|        - |  999 | `	/* Force ownership: the blob may be SXBLOB_RDONLY (e.g., from` |
|        - | 1000 | `	 * PH7_MemObjLoad), in which case BlobPrepareGrow copies on demand` |
|        - | 1001 | `	 * and clears the flag.  On an already-owned blob with spare capacity` |
|        - | 1002 | `	 * (the common case under PHL's growth allocator), this is a no-op` |
|        - | 1003 | `	 * append; on an exact-fit owned blob it triggers a single realloc. */` |
|      ! 0 | 1004 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      ! 0 | 1005 | `		SyBlobNullAppend(&pObj->sBlob);` |
|      ! 0 | 1006 | `	}` |
|      ! 0 | 1007 | `	nLen = SyBlobLength(&pObj->sBlob);` |
|      ! 0 | 1008 | `	if( nLen == 0 ){` |
|      ! 0 | 1009 | `		SyBlobAppend(&pObj->sBlob,"1",sizeof(char));` |
|      ! 0 | 1010 | `		return SXRET_OK;` |
|        - | 1011 | `	}` |
|      ! 0 | 1012 | `	zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|      ! 0 | 1013 | `	pos = nLen;` |
|      ! 0 | 1014 | `	while( pos > 0 ){` |
|      ! 0 | 1015 | `		pos--;` |
|      ! 0 | 1016 | `		ch = zStr[pos];` |
|      ! 0 | 1017 | `		if( ch >= 'a' && ch <= 'z' ){` |
|      ! 0 | 1018 | `			if( ch == 'z' ){` |
|      ! 0 | 1019 | `				zStr[pos] = 'a';` |
|      ! 0 | 1020 | `				last_class = CARRY_LOWER;` |
|      ! 0 | 1021 | `				continue;` |
|        - | 1022 | `			}` |
|      ! 0 | 1023 | `			zStr[pos]++;` |
|      ! 0 | 1024 | `			carry = 0;` |
|      ! 0 | 1025 | `			break;` |
|      ! 0 | 1026 | `		}else if( ch >= 'A' && ch <= 'Z' ){` |
|      ! 0 | 1027 | `			if( ch == 'Z' ){` |
|      ! 0 | 1028 | `				zStr[pos] = 'A';` |
|      ! 0 | 1029 | `				last_class = CARRY_UPPER;` |
|      ! 0 | 1030 | `				continue;` |
|        - | 1031 | `			}` |
|      ! 0 | 1032 | `			zStr[pos]++;` |
|      ! 0 | 1033 | `			carry = 0;` |
|      ! 0 | 1034 | `			break;` |
|      ! 0 | 1035 | `		}else if( ch >= '0' && ch <= '9' ){` |
|      ! 0 | 1036 | `			if( ch == '9' ){` |
|      ! 0 | 1037 | `				zStr[pos] = '0';` |
|      ! 0 | 1038 | `				last_class = CARRY_DIGIT;` |
|      ! 0 | 1039 | `				continue;` |
|        - | 1040 | `			}` |
|      ! 0 | 1041 | `			zStr[pos]++;` |
|      ! 0 | 1042 | `			carry = 0;` |
|      ! 0 | 1043 | `			break;` |
|      ! 0 | 1044 | `		}else{` |
|        - | 1045 | `			/* non-alphanumeric: stop without prepending */` |
|      ! 0 | 1046 | `			carry = 0;` |
|      ! 0 | 1047 | `			break;` |
|        - | 1048 | `		}` |
|      ! 0 | 1049 | `	}` |
|      ! 0 | 1050 | `	if( carry ){` |
|        - | 1051 | `		sxu8 prepend;` |
|        - | 1052 | `		sxu32 i;` |
|      ! 0 | 1053 | `		switch( last_class ){` |
|      ! 0 | 1054 | `			case CARRY_LOWER: prepend = (sxu8)'a'; break;` |
|      ! 0 | 1055 | `			case CARRY_UPPER: prepend = (sxu8)'A'; break;` |
|      ! 0 | 1056 | `			default:          prepend = (sxu8)'1'; break;` |
|        - | 1057 | `		}` |
|        - | 1058 | `		/* Append a sentinel byte to grow nByte by 1 (capacity grows too). */` |
|      ! 0 | 1059 | `		SyBlobAppend(&pObj->sBlob,"\0",sizeof(char));` |
|      ! 0 | 1060 | `		zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|      ! 0 | 1061 | `		nLen = SyBlobLength(&pObj->sBlob);` |
|        - | 1062 | `		/* Shift right by 1, walking from the end so overlapping is safe. */` |
|      ! 0 | 1063 | `		for( i = nLen - 1; i > 0; i-- ){` |
|      ! 0 | 1064 | `			zStr[i] = zStr[i - 1];` |
|      ! 0 | 1065 | `		}` |
|      ! 0 | 1066 | `		zStr[0] = prepend;` |
|      ! 0 | 1067 | `	}` |
|      ! 0 | 1068 | `	return SXRET_OK;` |
|      ! 0 | 1069 | `}` |
|        - | 1070 | `/*` |
|        - | 1071 | ` * Try a get an integer representation of the given ph7_value.` |
|        - | 1072 | ` * If the ph7_value is not of type real,this function is a no-op.` |
|        - | 1073 | ` */` |
|     1086 | 1074 | `PH7_PRIVATE sxi32 PH7_MemObjTryInteger(ph7_value *pObj)` |
|        4 | 1075 | `{` |
|     1090 | 1076 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|        - | 1077 | `		/* Work only with reals */` |
|     1090 | 1078 | `		MemObjTryIntger(&(*pObj));` |
|      543 | 1079 | `	}` |
|     1090 | 1080 | `	return SXRET_OK;` |
|        4 | 1081 | `}` |
|        - | 1082 | `/*` |
|        - | 1083 | ` * Initialize a ph7_value to the null type.` |
|        - | 1084 | ` */` |
| 29277218 | 1085 | `PH7_PRIVATE sxi32 PH7_MemObjInit(ph7_vm *pVm,ph7_value *pObj)` |
|        5 | 1086 | `{` |
|        - | 1087 | `	/* Zero the structure */` |
| 29277223 | 1088 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1089 | `	/* Initialize fields */` |
| 29277223 | 1090 | `	pObj->pVm = pVm;` |
| 29277223 | 1091 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1092 | `	/* Set the NULL type */` |
| 29277223 | 1093 | `	pObj->iFlags = MEMOBJ_NULL;` |
| 29277223 | 1094 | `	return SXRET_OK;` |
|        5 | 1095 | `}` |
|        - | 1096 | `/*` |
|        - | 1097 | ` * Initialize a ph7_value to the integer type.` |
|        - | 1098 | ` */` |
|  5971686 | 1099 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromInt(ph7_vm *pVm,ph7_value *pObj,sxi64 iVal)` |
|        5 | 1100 | `{` |
|        - | 1101 | `	/* Zero the structure */` |
|  5971691 | 1102 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1103 | `	/* Initialize fields */` |
|  5971691 | 1104 | `	pObj->pVm = pVm;` |
|  5971691 | 1105 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1106 | `	/* Set the desired type */` |
|  5971691 | 1107 | `	pObj->x.iVal = iVal;` |
|  5971691 | 1108 | `	pObj->iFlags = MEMOBJ_INT;` |
|  5971691 | 1109 | `	return SXRET_OK;` |
|        5 | 1110 | `}` |
|        - | 1111 | `/*` |
|        - | 1112 | ` * Initialize a ph7_value to the boolean type.` |
|        - | 1113 | ` */` |
|    16838 | 1114 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromBool(ph7_vm *pVm,ph7_value *pObj,sxi32 iVal)` |
|        5 | 1115 | `{` |
|        - | 1116 | `	/* Zero the structure */` |
|    16843 | 1117 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1118 | `	/* Initialize fields */` |
|    16843 | 1119 | `	pObj->pVm = pVm;` |
|    16843 | 1120 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1121 | `	/* Set the desired type */` |
|    16843 | 1122 | `	pObj->x.iVal = iVal ? 1 : 0;` |
|    16843 | 1123 | `	pObj->iFlags = MEMOBJ_BOOL;` |
|    16843 | 1124 | `	return SXRET_OK;` |
|        5 | 1125 | `}` |
|        - | 1126 | `/*` |
|        - | 1127 | ` * Initialize a ph7_value to the real type.` |
|        - | 1128 | ` */` |
|       10 | 1129 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromReal(ph7_vm *pVm,ph7_value *pObj,ph7_real rVal)` |
|        1 | 1130 | `{` |
|        - | 1131 | `	/* Zero the structure */` |
|       11 | 1132 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1133 | `	/* Initialize fields */` |
|       11 | 1134 | `	pObj->pVm = pVm;` |
|       11 | 1135 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1136 | `	/* Set the desired type */` |
|       11 | 1137 | `	pObj->rVal = rVal;` |
|       11 | 1138 | `	pObj->iFlags = MEMOBJ_REAL;` |
|       11 | 1139 | `	return SXRET_OK;` |
|        1 | 1140 | `}` |
|        - | 1141 | `/*` |
|        - | 1142 | ` * Initialize a ph7_value to the array type.` |
|        - | 1143 | ` */` |
|    63804 | 1144 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromArray(ph7_vm *pVm,ph7_value *pObj,ph7_hashmap *pArray)` |
|        5 | 1145 | `{` |
|        - | 1146 | `	/* Zero the structure */` |
|    63809 | 1147 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1148 | `	/* Initialize fields */` |
|    63809 | 1149 | `	pObj->pVm = pVm;` |
|    63809 | 1150 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1151 | `	/* Set the desired type */` |
|    63809 | 1152 | `	pObj->iFlags = MEMOBJ_HASHMAP;` |
|    63809 | 1153 | `	pObj->x.pOther = pArray;` |
|    63809 | 1154 | `	return SXRET_OK;` |
|        5 | 1155 | `}` |
|        - | 1156 | `/*` |
|        - | 1157 | ` * Initialize a ph7_value to the string type.` |
|        - | 1158 | ` */` |
|  4912444 | 1159 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromString(ph7_vm *pVm,ph7_value *pObj,const SyString *pVal)` |
|        5 | 1160 | `{` |
|        - | 1161 | `	/* Zero the structure */` |
|  4912449 | 1162 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1163 | `	/* Initialize fields */` |
|  4912449 | 1164 | `	pObj->pVm = pVm;` |
|  4912449 | 1165 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|  4912449 | 1166 | `	if( pVal ){` |
|        - | 1167 | `		/* Append contents */` |
|  2480649 | 1168 | `		SyBlobAppend(&pObj->sBlob,(const void *)pVal->zString,pVal->nByte);` |
|  1240322 | 1169 | `	}` |
|        - | 1170 | `	/* Set the desired type */` |
|  4912449 | 1171 | `	pObj->iFlags = MEMOBJ_STRING;` |
|  4912449 | 1172 | `	return SXRET_OK;` |
|        5 | 1173 | `}` |
|        - | 1174 | `/*` |
|        - | 1175 | ` * Append some contents to the internal buffer of a given ph7_value.` |
|        - | 1176 | ` * If the given ph7_value is not of type string,this function` |
|        - | 1177 | ` * invalidate any prior representation and set the string type.` |
|        - | 1178 | ` * Then a simple append operation is performed.` |
|        - | 1179 | ` */` |
|  2852463 | 1180 | `PH7_PRIVATE sxi32 PH7_MemObjStringAppend(ph7_value *pObj,const char *zData,sxu32 nLen)` |
|        5 | 1181 | `{` |
|        - | 1182 | `	sxi32 rc;` |
|  2852468 | 1183 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 1184 | `		/* Invalidate any prior representation */` |
|     3745 | 1185 | `		PH7_MemObjRelease(pObj);` |
|     3745 | 1186 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|     1870 | 1187 | `	}` |
|        - | 1188 | `	/* Append contents */` |
|  2852468 | 1189 | `	rc = SyBlobAppend(&pObj->sBlob,zData,nLen);` |
|  2852468 | 1190 | `	return rc;` |
|        5 | 1191 | `}` |
|        - | 1192 | `#if 0` |
|        - | 1193 | `/*` |
|        - | 1194 | ` * Format and append some contents to the internal buffer of a given ph7_value.` |
|        - | 1195 | ` * If the given ph7_value is not of type string,this function invalidate` |
|        - | 1196 | ` * any prior representation and set the string type.` |
|        - | 1197 | ` * Then a simple format and append operation is performed.` |
|        - | 1198 | ` */` |
|        - | 1199 | `PH7_PRIVATE sxi32 PH7_MemObjStringFormat(ph7_value *pObj,const char *zFormat,va_list ap)` |
|        - | 1200 | `{` |
|        - | 1201 | `	sxi32 rc;` |
|        - | 1202 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 1203 | `		/* Invalidate any prior representation */` |
|        - | 1204 | `		PH7_MemObjRelease(pObj);` |
|        - | 1205 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|        - | 1206 | `	}` |
|        - | 1207 | `	/* Format and append contents */` |
|        - | 1208 | `	rc = SyBlobFormatAp(&pObj->sBlob,zFormat,ap);` |
|        - | 1209 | `	return rc;` |
|        - | 1210 | `}` |
|        - | 1211 | `#endif` |
|        - | 1212 | `/*` |
|        - | 1213 | ` * Duplicate the contents of a ph7_value.` |
|        - | 1214 | ` */` |
|  6013088 | 1215 | `PH7_PRIVATE sxi32 PH7_MemObjStore(ph7_value *pSrc,ph7_value *pDest)` |
|        5 | 1216 | `{` |
|  6013093 | 1217 | `	ph7_class_instance *pObj = 0;` |
|  6013093 | 1218 | `	ph7_hashmap *pMap = 0;` |
|        - | 1219 | `	sxi32 rc;` |
|  6013093 | 1220 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1221 | `		/* Increment reference count */` |
|   218177 | 1222 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  5904007 | 1223 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|        - | 1224 | `		/* Increment reference count */` |
|    12299 | 1225 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|     6147 | 1226 | `	}` |
|  6013093 | 1227 | `	if( pDest->iFlags & MEMOBJ_HASHMAP ){` |
|    80827 | 1228 | `		pMap = (ph7_hashmap *)pDest->x.pOther;` |
|  5972682 | 1229 | `	}else if( pDest->iFlags & MEMOBJ_OBJ ){` |
|     8763 | 1230 | `		pObj = (ph7_class_instance *)pDest->x.pOther;` |
|     4379 | 1231 | `	}` |
|  6013093 | 1232 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|  6013093 | 1233 | `	pDest->iFlags &= ~MEMOBJ_AUX;` |
|  6013093 | 1234 | `	rc = SXRET_OK;` |
|  6013093 | 1235 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  4251500 | 1236 | `		SyBlobReset(&pDest->sBlob);` |
|  4251500 | 1237 | `		rc = SyBlobDup(&pSrc->sBlob,&pDest->sBlob);` |
|  2126061 | 1238 | `	}else{` |
|  1761598 | 1239 | `		if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|   319820 | 1240 | `			SyBlobRelease(&pDest->sBlob);` |
|   160170 | 1241 | `		}` |
|        - | 1242 | `	}` |
|  6013093 | 1243 | `	if( pMap ){` |
|    80827 | 1244 | `		PH7_HashmapUnref(pMap);` |
|  5972682 | 1245 | `	}else if( pObj ){` |
|     8763 | 1246 | `		PH7_ClassInstanceUnref(pObj);` |
|     4379 | 1247 | `	}` |
|  6013088 | 1248 | `	if( rc == SXRET_OK && (pDest->iFlags & MEMOBJ_HASHMAP)` |
|  3116715 | 1249 | `	 && pDest->pVm` |
|   218172 | 1250 | `	 && (ph7_hashmap *)pDest->x.pOther == pDest->pVm->pGlobal` |
|        - | 1251 | `	 /* Identity, not nIdx: transient values carry nIdx==0 (SyZero), which` |
|        - | 1252 | `	  * collides with a typical nGlobalIdx of 0 and would skip the snapshot` |
|        - | 1253 | `	  * for closure envs and other non-slot destinations. */` |
|   109095 | 1254 | `	 && pDest != (ph7_value *)SySetAt(&pDest->pVm->aMemObj,pDest->pVm->nGlobalIdx) ){` |
|        - | 1255 | `		/* php 8.1: a COPY of $GLOBALS ($snap = $GLOBALS, $a[] = $GLOBALS,` |
|        - | 1256 | `		 * by-value argument passing, return $GLOBALS, ...) is a by-value` |
|        - | 1257 | `		 * SNAPSHOT of the symbol table with its reference entries` |
|        - | 1258 | `		 * flattened — never a live alias. Materialize it here, the one` |
|        - | 1259 | `		 * store choke point (loads/subscript access keep sharing, so` |
|        - | 1260 | `		 * $GLOBALS[$k] reads and writes stay live). */` |
|        9 | 1261 | `		ph7_hashmap *pSnap = PH7_NewHashmap(pDest->pVm,0,0);` |
|        9 | 1262 | `		if( pSnap && PH7_HashmapDupMaterialized((ph7_hashmap *)pDest->x.pOther,pSnap) == SXRET_OK ){` |
|        9 | 1263 | `			PH7_HashmapUnref((ph7_hashmap *)pDest->x.pOther);` |
|        9 | 1264 | `			pDest->x.pOther = pSnap;` |
|        4 | 1265 | `		}else if( pSnap ){` |
|      ! 0 | 1266 | `			PH7_HashmapUnref(pSnap);` |
|      ! 0 | 1267 | `		}` |
|        4 | 1268 | `	}` |
|  6013093 | 1269 | `	return rc;` |
|        5 | 1270 | `}` |
|        - | 1271 | `/*` |
|        - | 1272 | ` * Duplicate the contents of a ph7_value but do not copy internal` |
|        - | 1273 | ` * buffer contents,simply point to it.` |
|        - | 1274 | ` */` |
|  8253998 | 1275 | `PH7_PRIVATE sxi32 PH7_MemObjLoad(ph7_value *pSrc,ph7_value *pDest)` |
|        5 | 1276 | `{` |
|  8254003 | 1277 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),` |
|        - | 1278 | `		sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|  8254003 | 1279 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1280 | `		/* Increment reference count */` |
|   539073 | 1281 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  7984469 | 1282 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|        - | 1283 | `		/* Increment reference count */` |
|    56201 | 1284 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|    28098 | 1285 | `	}` |
|  8254003 | 1286 | `	if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|       46 | 1287 | `		SyBlobRelease(&pDest->sBlob);` |
|       21 | 1288 | `	}` |
|  8254003 | 1289 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  4354450 | 1290 | `		SyBlobReadOnly(&pDest->sBlob,SyBlobData(&pSrc->sBlob),SyBlobLength(&pSrc->sBlob));` |
|  2178349 | 1291 | `	}` |
|  8254003 | 1292 | `	return SXRET_OK;` |
|        5 | 1293 | `}` |
|        - | 1294 | `/*` |
|        - | 1295 | ` * Invalidate any prior representation of a given ph7_value.` |
|        - | 1296 | ` */` |
| 20930205 | 1297 | `PH7_PRIVATE sxi32 PH7_MemObjRelease(ph7_value *pObj)` |
|        5 | 1298 | `{` |
| 20930210 | 1299 | `	if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
| 17581288 | 1300 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|   704459 | 1301 | `			PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
| 17229061 | 1302 | `		}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|   105087 | 1303 | `			PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|    52541 | 1304 | `		}` |
|        - | 1305 | `		/* Release the internal buffer */` |
| 17581288 | 1306 | `		SyBlobRelease(&pObj->sBlob);` |
|        - | 1307 | `		/* Invalidate any prior representation */` |
| 17581288 | 1308 | `		pObj->iFlags = MEMOBJ_NULL;` |
|  8794922 | 1309 | `	}` |
| 20930210 | 1310 | `	return SXRET_OK;` |
|        5 | 1311 | `}` |
|        - | 1312 | `/*` |
|        - | 1313 | ` * Compare two ph7_values.` |
|        - | 1314 | ` * Return 0 if the values are equals, > 0 if pObj1 is greater than pObj2` |
|        - | 1315 | ` * or < 0 if pObj2 is greater than pObj1.` |
|        - | 1316 | ` * Type comparison table taken from the PHP language reference manual.` |
|        - | 1317 | ` * Comparisons of $x with PHP functions Expression` |
|        - | 1318 | ` *              gettype() 	empty() 	is_null() 	isset() 	boolean : if($x)` |
|        - | 1319 | ` * $x = ""; 	string 	    TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1320 | ` * $x = null 	NULL 	    TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1321 | ` * var $x; 	    NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1322 | ` * $x is undefined 	NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1323 | ` *  $x = array(); 	array 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1324 | ` * $x = false; 	boolean 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1325 | ` * $x = true; 	boolean 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1326 | ` * $x = 1; 	    integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1327 | ` * $x = 42; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1328 | ` * $x = 0; 	    integer 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1329 | ` * $x = -1; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1330 | ` * $x = "1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1331 | ` * $x = "0"; 	string 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1332 | ` * $x = "-1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1333 | ` * $x = "php"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1334 | ` * $x = "true"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1335 | ` * $x = "false"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1336 | ` *      Loose comparisons with ==` |
|        - | 1337 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|        - | 1338 | ` * TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1339 | ` * FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|        - | 1340 | ` * 1 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1341 | ` * 0 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE` |
|        - | 1342 | ` * -1 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1343 | ` * "1" 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1344 | ` * "0" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1345 | ` * "-1" 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1346 | ` * NULL 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|        - | 1347 | ` * array() 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1348 | ` * "php" 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1349 | ` * "" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE` |
|        - | 1350 | ` *    Strict comparisons with ===` |
|        - | 1351 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|        - | 1352 | ` * TRUE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1353 | ` * FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1354 | ` * 1 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1355 | ` * 0 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1356 | ` * -1 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1357 | ` * "1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1358 | ` * "0" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1359 | ` * "-1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1360 | ` * NULL 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE` |
|        - | 1361 | ` * array() 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE` |
|        - | 1362 | ` * "php" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1363 | ` * "" 	    FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE` |
|        - | 1364 | ` */` |
|  1553223 | 1365 | `PH7_PRIVATE sxi32 PH7_MemObjCmp(ph7_value *pObj1,ph7_value *pObj2,int bStrict,int iNest)` |
|        5 | 1366 | `{` |
|        - | 1367 | `	sxi32 iComb;` |
|        - | 1368 | `	sxi32 rc;` |
|  1553228 | 1369 | `	if( bStrict ){` |
|        - | 1370 | `		sxi32 iF1,iF2;` |
|        - | 1371 | `		/* Strict comparisons with === */` |
|   841123 | 1372 | `		iF1 = pObj1->iFlags&~MEMOBJ_AUX;` |
|   841123 | 1373 | `		iF2 = pObj2->iFlags&~MEMOBJ_AUX;` |
|   841123 | 1374 | `		if( iF1 != iF2 ){` |
|        - | 1375 | `			/* Not of the same type */` |
|   195569 | 1376 | `			return 1;` |
|        - | 1377 | `		}` |
|   322983 | 1378 | `	}` |
|        - | 1379 | `	/* Combine flag together */` |
|  1357664 | 1380 | `	iComb = pObj1->iFlags\|pObj2->iFlags;` |
|  1357659 | 1381 | `	if( !bStrict` |
|  1035088 | 1382 | `	 && (iComb & MEMOBJ_NULL) != 0` |
|   356290 | 1383 | `	 && (iComb & MEMOBJ_STRING) != 0` |
|       66 | 1384 | `	 && (iComb & (MEMOBJ_BOOL\|MEMOBJ_RES\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|        - | 1385 | `		/*` |
|        - | 1386 | `		 * PHP 8 comparison table: null loosely compared with a STRING is` |
|        - | 1387 | `		 * compared as the empty string (a string comparison), not through` |
|        - | 1388 | `		 * bool coercion — so null == "0" is FALSE and null < "0" is TRUE` |
|        - | 1389 | `		 * (php 7 and the historical PH7 behavior coerced both to bool,` |
|        - | 1390 | `		 * making any non-empty non-"0"-insensitive string "equal" to null).` |
|        - | 1391 | `		 * Convert the null side to "" and let the string branch below run.` |
|        - | 1392 | `		 */` |
|       45 | 1393 | `		if( pObj1->iFlags & MEMOBJ_NULL ){` |
|       35 | 1394 | `			PH7_MemObjToString(pObj1);` |
|       18 | 1395 | `		}else{` |
|       11 | 1396 | `			PH7_MemObjToString(pObj2);` |
|        - | 1397 | `		}` |
|       45 | 1398 | `		iComb = pObj1->iFlags\|pObj2->iFlags;` |
|       22 | 1399 | `	}` |
|  1357664 | 1400 | `	if( (pObj1->iFlags & MEMOBJ_RES) && (pObj2->iFlags & MEMOBJ_RES) ){` |
|        - | 1401 | `		/* php compares two resources by their ID. The boolean path below would` |
|        - | 1402 | `		 * call every live resource equal to every other, since all are truthy. */` |
|        5 | 1403 | `		sxu32 nId1 = PH7_VmResourceId(pObj1->pVm,pObj1->x.pOther);` |
|        5 | 1404 | `		sxu32 nId2 = PH7_VmResourceId(pObj2->pVm,pObj2->x.pOther);` |
|        5 | 1405 | `		return nId1 == nId2 ? 0 : (nId1 < nId2 ? -1 : 1);` |
|        - | 1406 | `	}` |
|  1357660 | 1407 | `	if( iComb & (MEMOBJ_NULL\|MEMOBJ_RES\|MEMOBJ_BOOL) ){` |
|        - | 1408 | `		/* Convert to boolean: Keep in mind FALSE < TRUE */` |
|    38275 | 1409 | `		if( (pObj1->iFlags & MEMOBJ_BOOL) == 0 ){` |
|    23341 | 1410 | `			PH7_MemObjToBool(pObj1);` |
|    11668 | 1411 | `		}` |
|    38275 | 1412 | `		if( (pObj2->iFlags & MEMOBJ_BOOL) == 0 ){` |
|    22285 | 1413 | `			PH7_MemObjToBool(pObj2);` |
|    11140 | 1414 | `		}` |
|    38275 | 1415 | `		return (sxi32)((pObj1->x.iVal != 0) - (pObj2->x.iVal != 0));` |
|  1319390 | 1416 | `	}else if ( iComb & MEMOBJ_HASHMAP ){` |
|        - | 1417 | `		/* Hashmap aka 'array' comparison */` |
|       56 | 1418 | `		if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1419 | `			/* Array is always greater */` |
|      ! 0 | 1420 | `			return -1;` |
|        - | 1421 | `		}` |
|       56 | 1422 | `		if( (pObj2->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1423 | `			/* Array is always greater */` |
|      ! 0 | 1424 | `			return 1;` |
|        - | 1425 | `		}` |
|        - | 1426 | `		/* Perform the comparison */` |
|       56 | 1427 | `		rc = PH7_HashmapCmp((ph7_hashmap *)pObj1->x.pOther,(ph7_hashmap *)pObj2->x.pOther,bStrict);` |
|       56 | 1428 | `		return rc;` |
|  1319336 | 1429 | `	}else if(iComb & MEMOBJ_OBJ ){` |
|        - | 1430 | `		/* Object comparison */` |
|      295 | 1431 | `		if( (pObj1->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 1432 | `			/* Object is always greater */` |
|      ! 0 | 1433 | `			return -1;` |
|        - | 1434 | `		}` |
|      295 | 1435 | `		if( (pObj2->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 1436 | `			/* Object is always greater */` |
|      ! 0 | 1437 | `			return 1;` |
|        - | 1438 | `		}` |
|        - | 1439 | `		/* Perform the comparison */` |
|      295 | 1440 | `		rc = PH7_ClassInstanceCmp((ph7_class_instance *)pObj1->x.pOther,(ph7_class_instance *)pObj2->x.pOther,bStrict,iNest);` |
|      295 | 1441 | `		return rc;` |
|  1319046 | 1442 | `	}else if ( iComb & MEMOBJ_STRING ){` |
|        - | 1443 | `		SyString s1,s2;` |
|   849952 | 1444 | `		if( !bStrict ){` |
|        - | 1445 | `			/*` |
|        - | 1446 | `			 * PHP 8 "saner string to number comparisons" (RFC): a numeric` |
|        - | 1447 | `			 * comparison is performed only when BOTH operands are numbers or` |
|        - | 1448 | `			 * numeric strings. A number compared with a NON-numeric string is` |
|        - | 1449 | `			 * compared as strings, with the number cast to its string form —` |
|        - | 1450 | `			 * so 0 == "abc" is false, "abc" < 10 is false, and max("abc",10)` |
|        - | 1451 | `			 * is "abc". (PHP 7 cast the non-numeric string to 0 and compared` |
|        - | 1452 | `			 * numerically; comparing when EITHER side was numeric is what this` |
|        - | 1453 | `			 * replaces.) Two non-numeric strings, or one numeric and one` |
|        - | 1454 | `			 * non-numeric string, still fall through to the string comparison` |
|        - | 1455 | `			 * below, unchanged.` |
|        - | 1456 | `			 */` |
|   245580 | 1457 | `			if( PH7_MemObjIsNumeric(pObj1) && PH7_MemObjIsNumeric(pObj2) ){` |
|        - | 1458 | `				/* Perform a numeric comparison */` |
|     1099 | 1459 | `				goto Numeric;` |
|        - | 1460 | `			}` |
|   122128 | 1461 | `		}` |
|        - | 1462 | `		/* Perform a strict string comparison.*/` |
|   848854 | 1463 | `		if( (pObj1->iFlags&MEMOBJ_STRING) == 0 ){` |
|       23 | 1464 | `			PH7_MemObjToString(pObj1);` |
|       11 | 1465 | `		}` |
|   848854 | 1466 | `		if( (pObj2->iFlags&MEMOBJ_STRING) == 0 ){` |
|        7 | 1467 | `			PH7_MemObjToString(pObj2);` |
|        3 | 1468 | `		}` |
|   848854 | 1469 | `		SyStringInitFromBuf(&s1,SyBlobData(&pObj1->sBlob),SyBlobLength(&pObj1->sBlob));` |
|   848854 | 1470 | `		SyStringInitFromBuf(&s2,SyBlobData(&pObj2->sBlob),SyBlobLength(&pObj2->sBlob));` |
|        - | 1471 | `		/*` |
|        - | 1472 | `		 * Strings are compared using memcmp(). If one value is an exact prefix of the` |
|        - | 1473 | `		 * other, then the shorter value is less than the longer value.` |
|        - | 1474 | `		 */` |
|   848854 | 1475 | `		rc = SyMemcmp((const void *)s1.zString,(const void *)s2.zString,SXMIN(s1.nByte,s2.nByte));` |
|   848854 | 1476 | `		if( rc == 0 ){` |
|   278323 | 1477 | `			if( s1.nByte != s2.nByte ){` |
|    18699 | 1478 | `				rc = s1.nByte < s2.nByte ? -1 : 1;` |
|     9349 | 1479 | `			}` |
|   139161 | 1480 | `		}` |
|   848854 | 1481 | `		return rc;` |
|   469099 | 1482 | `	}else if( iComb & (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|   234238 | 1483 | `Numeric:` |
|        - | 1484 | `		/* Perform a numeric comparison if one of the operand is numeric(integer or real) */` |
|   470197 | 1485 | `		if( (pObj1->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|     1081 | 1486 | `			PH7_MemObjToNumeric(pObj1);` |
|      540 | 1487 | `		}` |
|   470197 | 1488 | `		if( (pObj2->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|     1087 | 1489 | `			PH7_MemObjToNumeric(pObj2);` |
|      543 | 1490 | `		}` |
|   470197 | 1491 | `		if( (pObj1->iFlags & pObj2->iFlags & MEMOBJ_INT) == 0) {` |
|        - | 1492 | `			/*` |
|        - | 1493 | `			 * Symisc eXtension to the PHP language:` |
|        - | 1494 | `			 *  Floating point comparison is introduced and works as expected.` |
|        - | 1495 | `			 */` |
|        - | 1496 | `			ph7_real r1,r2;` |
|        - | 1497 | `			/* Compare as reals */` |
|      310 | 1498 | `			if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|       11 | 1499 | `				PH7_MemObjToReal(pObj1);` |
|        5 | 1500 | `			}` |
|      310 | 1501 | `			r1 = pObj1->rVal;` |
|      310 | 1502 | `			if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|       56 | 1503 | `				PH7_MemObjToReal(pObj2);` |
|       27 | 1504 | `			}` |
|      310 | 1505 | `			r2 = pObj2->rVal;` |
|      310 | 1506 | `			if( PH7_IS_NAN(r1) \|\| PH7_IS_NAN(r2) ){` |
|        - | 1507 | `				/*` |
|        - | 1508 | `				 * Keep a strict three-way comparator contract even for NaN values.` |
|        - | 1509 | `				 * For ordering purposes, NaN compares equal to NaN and greater than` |
|        - | 1510 | `				 * any non-NaN numeric value.` |
|        - | 1511 | `				 */` |
|       50 | 1512 | `				if( PH7_IS_NAN(r1) ){` |
|       40 | 1513 | `					return PH7_IS_NAN(r2) ? 0 : 1;` |
|        - | 1514 | `				}` |
|       11 | 1515 | `				return -1;` |
|        - | 1516 | `			}` |
|      262 | 1517 | `			if( r1 > r2 ){` |
|       54 | 1518 | `				return 1;` |
|      210 | 1519 | `			}else if( r1 < r2 ){` |
|      134 | 1520 | `				return -1;` |
|        - | 1521 | `			}` |
|       78 | 1522 | `			return 0;` |
|      ! 0 | 1523 | `		}else{` |
|        - | 1524 | `			/* Integer comparison */` |
|   469889 | 1525 | `			if( pObj1->x.iVal > pObj2->x.iVal ){` |
|     7675 | 1526 | `				return 1;` |
|   462219 | 1527 | `			}else if( pObj1->x.iVal < pObj2->x.iVal ){` |
|   451225 | 1528 | `				return -1;` |
|        - | 1529 | `			}` |
|    10999 | 1530 | `			return 0;` |
|        - | 1531 | `		}` |
|        - | 1532 | `	}` |
|        - | 1533 | `	/* NOT REACHED */` |
|      ! 0 | 1534 | `	return 0;` |
|   777021 | 1535 | `}` |
|        - | 1536 | `/*` |
|        - | 1537 | ` * Perform an addition operation of two ph7_values.` |
|        - | 1538 | ` * The reason this function is implemented here rather than 'vm.c'` |
|        - | 1539 | ` * is that the '+' operator is overloaded.` |
|        - | 1540 | ` * That is,the '+' operator is used for arithmetic operation and also` |
|        - | 1541 | ` * used for operation on arrays [i.e: union]. When used with an array` |
|        - | 1542 | ` * The + operator returns the right-hand array appended to the left-hand array.` |
|        - | 1543 | ` * For keys that exist in both arrays, the elements from the left-hand array` |
|        - | 1544 | ` * will be used, and the matching elements from the right-hand array will` |
|        - | 1545 | ` * be ignored.` |
|        - | 1546 | ` * This function take care of handling all the scenarios.` |
|        - | 1547 | ` */` |
|    14666 | 1548 | `PH7_PRIVATE sxi32 PH7_MemObjAdd(ph7_value *pObj1,ph7_value *pObj2,int bAddStore)` |
|        5 | 1549 | `{` |
|    14671 | 1550 | `	if( ((pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1551 | `			/* Arithemtic operation */` |
|    10847 | 1552 | `			PH7_MemObjToNumeric(pObj1);` |
|    10847 | 1553 | `			PH7_MemObjToNumeric(pObj2);` |
|    10847 | 1554 | `			if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_REAL ){` |
|        - | 1555 | `				/* Floating point arithmetic */` |
|        - | 1556 | `				ph7_real a,b;` |
|       69 | 1557 | `				if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|       29 | 1558 | `					PH7_MemObjToReal(pObj1);` |
|       14 | 1559 | `				}` |
|       69 | 1560 | `				if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|        9 | 1561 | `					PH7_MemObjToReal(pObj2);` |
|        4 | 1562 | `				}` |
|       69 | 1563 | `				a = pObj1->rVal;` |
|       69 | 1564 | `				b = pObj2->rVal;` |
|       69 | 1565 | `				pObj1->rVal = a+b;` |
|       69 | 1566 | `				MemObjSetType(pObj1,MEMOBJ_REAL);` |
|        - | 1567 | `				/* Try to get an integer representation also */` |
|       69 | 1568 | `				MemObjTryIntger(&(*pObj1));` |
|       35 | 1569 | `			}else{` |
|        - | 1570 | `				/* Integer arithmetic; PHP promotes an overflowing sum to float.` |
|        - | 1571 | `				 * The integer-only build (PH7_OMIT_FLOATING_POINT) has no float` |
|        - | 1572 | `				 * type, so it wraps like OP_POW's OMIT path. */` |
|        - | 1573 | `				sxi64 a,b,r;` |
|    10779 | 1574 | `				a = pObj1->x.iVal;` |
|    10779 | 1575 | `				b = pObj2->x.iVal;` |
|    10779 | 1576 | `				if( PH7_ADD_OVERFLOW64(a,b,&r) ){` |
|        - | 1577 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        9 | 1578 | `					pObj1->rVal = (ph7_real)a + (ph7_real)b;` |
|        9 | 1579 | `					MemObjSetType(pObj1,MEMOBJ_REAL);` |
|        - | 1580 | `#else` |
|        - | 1581 | `					pObj1->x.iVal = r;` |
|        - | 1582 | `					MemObjSetType(pObj1,MEMOBJ_INT);` |
|        - | 1583 | `#endif` |
|        5 | 1584 | `				}else{` |
|    10771 | 1585 | `					pObj1->x.iVal = r;` |
|    10771 | 1586 | `					MemObjSetType(pObj1,MEMOBJ_INT);` |
|        - | 1587 | `				}` |
|        - | 1588 | `			}` |
|     5426 | 1589 | `	}else{` |
|     3829 | 1590 | `		if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP ){` |
|        - | 1591 | `			ph7_hashmap *pMap;` |
|        - | 1592 | `			sxi32 rc;` |
|     3829 | 1593 | `			if( bAddStore ){` |
|        - | 1594 | `				/* Do not duplicate the hashmap,use the left one since its an add&store operation.` |
|        - | 1595 | `				 */` |
|        3 | 1596 | `				if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1597 | `					/* Force a hashmap cast */` |
|      ! 0 | 1598 | `					rc = PH7_MemObjToHashmap(pObj1);` |
|      ! 0 | 1599 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 1600 | `						PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|      ! 0 | 1601 | `						return rc;` |
|        - | 1602 | `					}` |
|      ! 0 | 1603 | `				}` |
|        - | 1604 | `				/* COW separate before in-place mutation */` |
|        3 | 1605 | `				pMap = PH7_HashmapCowSeparate(pObj1->pVm,pObj1);` |
|        2 | 1606 | `			}else{` |
|        - | 1607 | `				/* Create a new hashmap */` |
|     3827 | 1608 | `				pMap = PH7_NewHashmap(pObj1->pVm,0,0);` |
|     3827 | 1609 | `				if( pMap == 0){` |
|      ! 0 | 1610 | `					PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|      ! 0 | 1611 | `					return SXERR_MEM;` |
|        - | 1612 | `				}` |
|        - | 1613 | `			}` |
|     3829 | 1614 | `			if( !bAddStore ){` |
|     3827 | 1615 | `				if(pObj1->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1616 | `					/* Perform a hashmap duplication */` |
|     3827 | 1617 | `					PH7_HashmapDup((ph7_hashmap *)pObj1->x.pOther,pMap);` |
|     1916 | 1618 | `				}else{` |
|      ! 0 | 1619 | `					if((pObj1->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 1620 | `						/* Simple insertion */` |
|      ! 0 | 1621 | `						PH7_HashmapInsert(pMap,0,pObj1);` |
|      ! 0 | 1622 | `					}` |
|        - | 1623 | `				}` |
|     1911 | 1624 | `			}` |
|        - | 1625 | `			/* Perform the union */` |
|     3829 | 1626 | `			if(pObj2->iFlags & MEMOBJ_HASHMAP ){` |
|     3829 | 1627 | `				PH7_HashmapUnion(pMap,(ph7_hashmap *)pObj2->x.pOther);` |
|     1917 | 1628 | `			}else{` |
|      ! 0 | 1629 | `				if((pObj2->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 1630 | `					/* Simple insertion */` |
|      ! 0 | 1631 | `					PH7_HashmapInsert(pMap,0,pObj2);` |
|      ! 0 | 1632 | `				}` |
|        - | 1633 | `			}` |
|        - | 1634 | `			/* Reflect the change */` |
|     3829 | 1635 | `			if( pObj1->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 1636 | `				SyBlobRelease(&pObj1->sBlob);` |
|      ! 0 | 1637 | `			}` |
|     3829 | 1638 | `			pObj1->x.pOther = pMap;` |
|     3829 | 1639 | `			MemObjSetType(pObj1,MEMOBJ_HASHMAP);` |
|     1912 | 1640 | `		}` |
|        - | 1641 | `	}` |
|    14671 | 1642 | `	return SXRET_OK;` |
|     7338 | 1643 | `}` |
|        - | 1644 | `/*` |
|        - | 1645 | ` * Return a printable representation of the type of a given` |
|        - | 1646 | ` * ph7_value.` |
|        - | 1647 | ` */` |
|      ! 0 | 1648 | `PH7_PRIVATE const char * PH7_MemObjTypeDump(ph7_value *pVal)` |
|      ! 0 | 1649 | `{` |
|      ! 0 | 1650 | `	const char *zType = "";` |
|      ! 0 | 1651 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|      ! 0 | 1652 | `		zType = "null";` |
|      ! 0 | 1653 | `	}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|        - | 1654 | `		/* REAL is authoritative over a cached MEMOBJ_INT: an integer-valued` |
|        - | 1655 | `		 * real (e.g. 1.0) is reported as "double", matching PHP's gettype(). */` |
|      ! 0 | 1656 | `		zType = "double";` |
|      ! 0 | 1657 | `	}else if( pVal->iFlags & MEMOBJ_INT ){` |
|      ! 0 | 1658 | `		zType = "int";` |
|      ! 0 | 1659 | `	}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 1660 | `		zType = "string";` |
|      ! 0 | 1661 | `	}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 | 1662 | `		zType = "bool";` |
|      ! 0 | 1663 | `	}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|      ! 0 | 1664 | `		zType = "array";` |
|      ! 0 | 1665 | `	}else if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 1666 | `		zType = "object";` |
|      ! 0 | 1667 | `	}else if( pVal->iFlags & MEMOBJ_RES ){` |
|      ! 0 | 1668 | `		zType = "resource";` |
|      ! 0 | 1669 | `	}` |
|      ! 0 | 1670 | `	return zType;` |
|      ! 0 | 1671 | `}` |
|        - | 1672 | `/*` |
|        - | 1673 | ` * Dump a ph7_value [i.e: get a printable representation of it's type and contents.].` |
|        - | 1674 | ` * Store the dump in the given blob.` |
|        - | 1675 | ` */` |
|        - | 1676 | `/*` |
|        - | 1677 | ` * php's var_dump float shape (serialize_precision = -1): the SHORTEST decimal` |
|        - | 1678 | ` * string that round-trips to the same double — 0.1+0.2 dumps every digit` |
|        - | 1679 | ` * (0.30000000000000004), 1.0 dumps "1" — pushed through the same` |
|        - | 1680 | ` * exponent/fraction normalization as echo (PH7_PhpFloatShape: uppercase E,` |
|        - | 1681 | ` * "1.0E+100"). Distinct from echo/casts, which use EG(precision)=14.` |
|        - | 1682 | ` */` |
|        4 | 1683 | `static void MemObjDumpRealValue(SyBlob *pOut,ph7_real rVal)` |
|        1 | 1684 | `{` |
|        5 | 1685 | `	if( PH7_IS_NAN(rVal) ){` |
|      ! 0 | 1686 | `		SyBlobAppend(&(*pOut),"NAN",3);` |
|      ! 0 | 1687 | `		return;` |
|        - | 1688 | `	}` |
|        5 | 1689 | `	if( PH7_IS_INF(rVal) ){` |
|      ! 0 | 1690 | `		SyBlobAppend(&(*pOut),rVal < 0.0 ? "-INF" : "INF",rVal < 0.0 ? 4 : 3);` |
|      ! 0 | 1691 | `		return;` |
|        - | 1692 | `	}` |
|        - | 1693 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - | 1694 | `	{` |
|        - | 1695 | `		char zNum[48];` |
|        5 | 1696 | `		sxi32 n = 0;` |
|        - | 1697 | `		int p;` |
|        7 | 1698 | `		for( p = 1 ; p <= 17 ; p++ ){` |
|        7 | 1699 | `			n = (sxi32)snprintf(zNum,sizeof(zNum),"%.*G",p,rVal);` |
|        7 | 1700 | `			if( n < 0 \|\| n >= (sxi32)sizeof(zNum) ){` |
|      ! 0 | 1701 | `				n = (sxi32)SyStrlen(zNum);` |
|      ! 0 | 1702 | `			}` |
|        7 | 1703 | `			if( strtod(zNum,0) == rVal ){` |
|        5 | 1704 | `				break; /* shortest round-trip found */` |
|        - | 1705 | `			}` |
|        2 | 1706 | `		}` |
|        5 | 1707 | `		n = PH7_PhpFloatShape(zNum,n,TRUE);` |
|        5 | 1708 | `		SyBlobAppend(&(*pOut),zNum,(sxu32)n);` |
|        - | 1709 | `	}` |
|        - | 1710 | `#else` |
|        - | 1711 | `	SyBlobFormat(&(*pOut),"%.15g",rVal);` |
|        - | 1712 | `#endif` |
|        3 | 1713 | `}` |
|        - | 1714 | `/*` |
|        - | 1715 | ` * Emit a value's print_r INLINE representation (php: the echo conversion,` |
|        - | 1716 | ` * except true -> "1" and false/null -> ""). Containers never come through` |
|        - | 1717 | ` * here — the entry renderers recurse into the container dumpers instead.` |
|        - | 1718 | ` */` |
|      210 | 1719 | `PH7_PRIVATE void PH7_MemObjPrintRInline(SyBlob *pOut,ph7_value *pObj)` |
|        2 | 1720 | `{` |
|      212 | 1721 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|        7 | 1722 | `		return;` |
|        - | 1723 | `	}` |
|      206 | 1724 | `	if( pObj->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 | 1725 | `		if( pObj->x.iVal != 0 ){` |
|      ! 0 | 1726 | `			SyBlobAppend(&(*pOut),"1",sizeof(char));` |
|      ! 0 | 1727 | `		}` |
|      ! 0 | 1728 | `		return;` |
|        - | 1729 | `	}` |
|      206 | 1730 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|        - | 1731 | `		/* Strings already hold their bytes (MemObjStringValue only CONVERTS` |
|        - | 1732 | `		 * non-strings into the output) */` |
|      108 | 1733 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      108 | 1734 | `			SyBlobAppend(&(*pOut),SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|       53 | 1735 | `		}` |
|      108 | 1736 | `		return;` |
|        - | 1737 | `	}` |
|      100 | 1738 | `	MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|      107 | 1739 | `}` |
|     1006 | 1740 | `PH7_PRIVATE sxi32 PH7_MemObjDump(` |
|        - | 1741 | `	SyBlob *pOut,      /* Store the dump here */` |
|        - | 1742 | `	ph7_value *pObj,   /* Dump this */` |
|        - | 1743 | `	int ShowType,      /* TRUE for var_dump; FALSE for print_r */` |
|        - | 1744 | `	int nTab,          /* Indent in SPACES: var_dump = this value's own line;` |
|        - | 1745 | `	                    * print_r = the container's parenthesis column */` |
|        - | 1746 | `	int nDepth,        /* Nesting level */` |
|        - | 1747 | `	int isRef          /* TRUE if referenced entry (var_dump prints '&') */` |
|        - | 1748 | `	)` |
|        4 | 1749 | `{` |
|     1010 | 1750 | `	sxi32 rc = SXRET_OK;` |
|        - | 1751 | `	int i;` |
|     1010 | 1752 | `	if( !ShowType ){` |
|        - | 1753 | `		/* ---- print_r ---- php prints scalars inline with NO newline; only` |
|        - | 1754 | `		 * containers render the Array/Object block (which the container` |
|        - | 1755 | `		 * dumpers terminate with ")\n"). References carry no marker. */` |
|      110 | 1756 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|      103 | 1757 | `			return PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,FALSE,nTab,nDepth+1);` |
|        - | 1758 | `		}` |
|        8 | 1759 | `		if( (pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|        6 | 1760 | `			return PH7_ClassInstanceDump(&(*pOut),(ph7_class_instance *)pObj->x.pOther,FALSE,nTab,nDepth+1);` |
|        - | 1761 | `		}` |
|        3 | 1762 | `		PH7_MemObjPrintRInline(&(*pOut),pObj);` |
|        3 | 1763 | `		return SXRET_OK;` |
|        - | 1764 | `	}` |
|        - | 1765 | `	/* ---- var_dump ---- every value renders on its own line at nTab spaces,` |
|        - | 1766 | `	 * php's exact shapes: bool(true), NULL, int(n), float(shortest),` |
|        - | 1767 | `	 * string(N) "s", array(N) { … }, object(C)#id (n) { … }, &-references. */` |
|     5002 | 1768 | `	for( i = 0 ; i < nTab ; i++ ){` |
|     4103 | 1769 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     2053 | 1770 | `	}` |
|      902 | 1771 | `	if( isRef ){` |
|        7 | 1772 | `		SyBlobAppend(&(*pOut),"&",sizeof(char));` |
|        3 | 1773 | `	}` |
|      902 | 1774 | `	if( (pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|      139 | 1775 | `		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|      139 | 1776 | `		if( pInst->pClass->iFlags & PH7_CLASS_ENUM ){` |
|        - | 1777 | ``			/* php 8.1: var_dump of an enum case prints `enum(S::A)` — no body */`` |
|      ! 0 | 1778 | `			ph7_value *pName = PH7_EnumCaseNameValue(pInst);` |
|      ! 0 | 1779 | `			SyBlobFormat(&(*pOut),"enum(%z::",&pInst->pClass->sName);` |
|      ! 0 | 1780 | `			if( pName && SyBlobLength(&pName->sBlob) > 0 ){` |
|      ! 0 | 1781 | `				SyBlobAppend(&(*pOut),SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob));` |
|      ! 0 | 1782 | `			}` |
|      ! 0 | 1783 | `			SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|      ! 0 | 1784 | `			return SXRET_OK;` |
|        - | 1785 | `		}` |
|      139 | 1786 | `		rc = PH7_ClassInstanceDump(&(*pOut),pInst,TRUE,nTab,nDepth+1);` |
|      139 | 1787 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      139 | 1788 | `		return rc;` |
|        - | 1789 | `	}` |
|      766 | 1790 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|       23 | 1791 | `		SyBlobAppend(&(*pOut),"NULL\n",sizeof("NULL\n")-1);` |
|       23 | 1792 | `		return SXRET_OK;` |
|        - | 1793 | `	}` |
|      744 | 1794 | `	if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       25 | 1795 | `		rc = PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,TRUE,nTab,nDepth+1);` |
|       25 | 1796 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       25 | 1797 | `		return rc;` |
|        - | 1798 | `	}` |
|      720 | 1799 | `	if( pObj->iFlags & MEMOBJ_BOOL ){` |
|      281 | 1800 | `		if( pObj->x.iVal != 0 ){` |
|      185 | 1801 | `			SyBlobAppend(&(*pOut),"bool(true)\n",sizeof("bool(true)\n")-1);` |
|       94 | 1802 | `		}else{` |
|       98 | 1803 | `			SyBlobAppend(&(*pOut),"bool(false)\n",sizeof("bool(false)\n")-1);` |
|        - | 1804 | `		}` |
|      281 | 1805 | `		return SXRET_OK;` |
|        - | 1806 | `	}` |
|      442 | 1807 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|        - | 1808 | `		/* Checked BEFORE the int flag: an integer-valued real carries a cached` |
|        - | 1809 | `		 * MEMOBJ_INT view too, and php dumps it as float(1). */` |
|        5 | 1810 | `		SyBlobAppend(&(*pOut),"float(",sizeof("float(")-1);` |
|        5 | 1811 | `		MemObjDumpRealValue(&(*pOut),pObj->rVal);` |
|        5 | 1812 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        5 | 1813 | `		return SXRET_OK;` |
|        - | 1814 | `	}` |
|      438 | 1815 | `	if( pObj->iFlags & MEMOBJ_INT ){` |
|      268 | 1816 | `		SyBlobFormat(&(*pOut),"int(%qd)",pObj->x.iVal);` |
|      268 | 1817 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      268 | 1818 | `		return SXRET_OK;` |
|        - | 1819 | `	}` |
|      173 | 1820 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|      173 | 1821 | `		SyBlobFormat(&(*pOut),"string(%u) \"",SyBlobLength(&pObj->sBlob));` |
|      173 | 1822 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      153 | 1823 | `			SyBlobAppend(&(*pOut),SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|       75 | 1824 | `		}` |
|      173 | 1825 | `		SyBlobAppend(&(*pOut),"\"\n",sizeof("\"\n")-1);` |
|      173 | 1826 | `		return SXRET_OK;` |
|        - | 1827 | `	}` |
|      ! 0 | 1828 | `	if( pObj->iFlags & MEMOBJ_RES ){` |
|        - | 1829 | ``		/* php: `resource(N) of type (stream)`, and `(Unknown)` once closed —`` |
|        - | 1830 | `		 * which is exactly what PH7_VfsResourceType() already reports. The old` |
|        - | 1831 | `		 * shape printed the heap pointer through the string cast instead. */` |
|      ! 0 | 1832 | `		SyBlobFormat(&(*pOut),"resource(%u) of type (%s)\n",` |
|      ! 0 | 1833 | `			PH7_VmResourceId(pObj->pVm,pObj->x.pOther),` |
|      ! 0 | 1834 | `			PH7_VfsResourceType(pObj->x.pOther));` |
|      ! 0 | 1835 | `		return SXRET_OK;` |
|        - | 1836 | `	}` |
|        - | 1837 | ``	/* Anything else: the legacy `type(value)` shape. */`` |
|        - | 1838 | `	{` |
|      ! 0 | 1839 | `		const char *zType = PH7_MemObjTypeDump(pObj);` |
|      ! 0 | 1840 | `		SyBlobAppend(&(*pOut),zType,SyStrlen(zType));` |
|      ! 0 | 1841 | `		SyBlobAppend(&(*pOut),"(",sizeof(char));` |
|      ! 0 | 1842 | `		MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|      ! 0 | 1843 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        - | 1844 | `	}` |
|      ! 0 | 1845 | `	return rc;` |
|      507 | 1846 | `}` |
|        - | 1847 |  |
