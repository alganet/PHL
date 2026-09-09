# src/ph7/memobj.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 885/1017 lines (87.02%)

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
|        4 |   35 | `{` |
|        4 |   36 | `	*pR = (sxi64)((sxu64)a * (sxu64)b);` |
|        4 |   37 | `	if( a == 0 \|\| b == 0 \|\| a == 1 \|\| b == 1 ){` |
|        3 |   38 | `		return 0;` |
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
|        4 |   59 | `}` |
|        - |   60 | `#endif` |
|        - |   61 |  |
|        - |   62 | `/* Provide PHP-style type names for values.  This utility may be reused` |
|        - |   63 | ` * by any subsystem that works with ph7_value.` |
|        - |   64 | ` */` |
|      372 |   65 | `PH7_PRIVATE const char *ph7_type_name(ph7_value *pVal)` |
|        5 |   66 | `{` |
|      377 |   67 | `	if( ph7_value_is_null(pVal) ) return "null";` |
|      329 |   68 | `	if( ph7_value_is_bool(pVal) ) return "bool";` |
|      321 |   69 | `	if( ph7_value_is_int(pVal) ) return "int";` |
|      235 |   70 | `	if( ph7_value_is_float(pVal) ) return "float";` |
|      225 |   71 | `	if( ph7_value_is_string(pVal) ) return "string";` |
|       25 |   72 | `	if( ph7_value_is_array(pVal) ) return "array";` |
|        3 |   73 | `	if( ph7_value_is_object(pVal) ) return "object";` |
|      ! 0 |   74 | `	if( ph7_value_is_resource(pVal) ) return "resource";` |
|      ! 0 |   75 | `	return "unknown";` |
|      191 |   76 | `}` |
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
|    10668 |   94 | `static sxi64 MemObjRealToInt(ph7_value *pObj)` |
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
|    10673 |  111 | `  ph7_real r = pObj->rVal;` |
|    10673 |  112 | `  if( r<(ph7_real)minInt ){` |
|        3 |  113 | `    return minInt;` |
|    10671 |  114 | `  }else if( r>(ph7_real)maxInt ){` |
|        - |  115 | `    /* minInt is correct here - not maxInt.  It turns out that assigning` |
|        - |  116 | `    ** a very large positive number to an integer results in a very large` |
|        - |  117 | `    ** negative integer.  This makes no sense, but it is what x86 hardware` |
|        - |  118 | `    ** does so for compatibility we will do the same in software. */` |
|      176 |  119 | `    return minInt;` |
|      ! 0 |  120 | `  }else{` |
|    10497 |  121 | `    return (sxi64)r;` |
|        - |  122 | `  }` |
|        - |  123 | `#endif` |
|     5339 |  124 | `}` |
|        - |  125 | `/*` |
|        - |  126 | ` * Convert a raw token value typically a stream of digit [i.e: hex,octal,binary or decimal]` |
|        - |  127 | ` * to a 64-bit integer.` |
|        - |  128 | ` */` |
|  3350858 |  129 | `PH7_PRIVATE sxi64 PH7_TokenValueToInt64(SyString *pVal)` |
|        5 |  130 | `{` |
|  3350863 |  131 | `	sxi64 iVal = 0;` |
|  3350863 |  132 | `	if( pVal->nByte <= 0 ){` |
|      ! 0 |  133 | `		return 0;` |
|        - |  134 | `	}` |
|  3350863 |  135 | `	if( pVal->zString[0] == '0' ){` |
|        - |  136 | `		sxi32 c;` |
|  1317831 |  137 | `		if( pVal->nByte == sizeof(char) ){` |
|  1210669 |  138 | `			return 0;` |
|        - |  139 | `		}` |
|   107167 |  140 | `		c = pVal->zString[1];` |
|   107167 |  141 | `		if( c  == 'x' \|\| c == 'X' ){` |
|        - |  142 | `			/* Hex digit stream */` |
|   103011 |  143 | `			SyHexStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|    55664 |  144 | `		}else if( c == 'b' \|\| c == 'B' ){` |
|        - |  145 | `			/* Binary digit stream */` |
|      279 |  146 | `			SyBinaryStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|      140 |  147 | `		}else{` |
|        - |  148 | `			/* Octal digit stream */` |
|     3883 |  149 | `			SyOctalStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|        - |  150 | `		}` |
|    53586 |  151 | `	}else{` |
|        - |  152 | `		/* Decimal digit stream */` |
|  2033037 |  153 | `		SyStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|        - |  154 | `	}` |
|  2140199 |  155 | `	return iVal;` |
|  1675434 |  156 | `}` |
|        - |  157 | `/*` |
|        - |  158 | ` * Return some kind of 64-bit integer value which is the best we can` |
|        - |  159 | ` * do at representing the value that pObj describes as a string` |
|        - |  160 | ` * representation.` |
|        - |  161 | ` */` |
|      872 |  162 | `static sxi64 MemObjStringToInt(ph7_value *pObj)` |
|        5 |  163 | `{` |
|      877 |  164 | `	sxi64 iVal = 0;` |
|        - |  165 | `	/* A *string* is always read in base 10 by php: "012" is 12, "0x1A" and "0b11"` |
|        - |  166 | `	 * are 0. Only a source *literal* carries a base prefix, and that is decoded by` |
|        - |  167 | `	 * the compiler (PH7_TokenValueToInt64) -- not here. */` |
|      877 |  168 | `	SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&iVal,0);` |
|      877 |  169 | `	return iVal;` |
|        5 |  170 | `}` |
|        - |  171 | `/*` |
|        - |  172 | ` * Call a magic class method [i.e: __toString(),__toInt(),...]` |
|        - |  173 | ` * Return SXRET_OK if the magic method is available and have been` |
|        - |  174 | ` * successfully called. Any other return value indicates failure.` |
|        - |  175 | ` */` |
|      356 |  176 | `static sxi32 MemObjCallClassCastMethod(` |
|        - |  177 | `	ph7_vm *pVm,               /* VM that trigger the invocation */` |
|        - |  178 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object] */` |
|        - |  179 | `	const char *zMethod,       /* Magic method name [i.e: __toString] */` |
|        - |  180 | `	sxu32 nLen,                /* Method name length */` |
|        - |  181 | `	ph7_value *pResult         /* OUT: Store the return value of the magic method here */` |
|        - |  182 | `	)` |
|        5 |  183 | `{` |
|        - |  184 | `	ph7_class_method *pMethod;` |
|        - |  185 | `	/* Check if the method is available */` |
|      361 |  186 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,zMethod,nLen);` |
|      361 |  187 | `	if( pMethod == 0 ){` |
|        - |  188 | `		/* No such method */` |
|      174 |  189 | `		return SXERR_NOTFOUND;` |
|        - |  190 | `	}` |
|        - |  191 | `	/* Invoke the desired method */` |
|      189 |  192 | `	PH7_VmCallClassMethod(&(*pVm),&(*pThis),pMethod,&(*pResult),0,0);` |
|        - |  193 | `	/* Method successfully called,pResult should hold the return value */` |
|      189 |  194 | `	return SXRET_OK;` |
|      183 |  195 | `}` |
|        - |  196 | `/*` |
|        - |  197 | ` * Return some kind of integer value which is the best we can` |
|        - |  198 | ` * do at representing the value that pObj describes as an integer.` |
|        - |  199 | ` * If pObj is an integer, then the value is exact. If pObj is` |
|        - |  200 | ` * a floating-point then  the value returned is the integer part.` |
|        - |  201 | ` * If pObj is a string, then we make an attempt to convert it into` |
|        - |  202 | ` * a integer and return that.` |
|        - |  203 | ` * If pObj represents a NULL value, return 0.` |
|        - |  204 | ` */` |
|      908 |  205 | `static sxi64 MemObjIntValue(ph7_value *pObj)` |
|        5 |  206 | `{` |
|        - |  207 | `	sxi32 iFlags;` |
|      913 |  208 | `	iFlags = pObj->iFlags;` |
|      913 |  209 | `	if (iFlags & MEMOBJ_REAL ){` |
|       41 |  210 | `		return MemObjRealToInt(&(*pObj));` |
|      873 |  211 | `	}else if( iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      160 |  212 | `		return pObj->x.iVal;` |
|      715 |  213 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|      697 |  214 | `		return MemObjStringToInt(&(*pObj));` |
|       19 |  215 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|        7 |  216 | `		return 0;` |
|       13 |  217 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|        7 |  218 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|        7 |  219 | `		sxu32 n = pMap->nEntry;` |
|        7 |  220 | `		PH7_HashmapUnref(pMap);` |
|        - |  221 | `		/* Return total number of entries in the hashmap */` |
|        7 |  222 | `		return n;` |
|        7 |  223 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|        - |  224 | `		ph7_value sResult;` |
|        5 |  225 | `		sxi64 iVal = 1;` |
|        - |  226 | `		sxi32 rc;` |
|        - |  227 | `		/* Invoke the [__toInt()] magic method if available [note that this is a symisc extension]  */` |
|        5 |  228 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|        5 |  229 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|        - |  230 | `			"__toInt",sizeof("__toInt")-1,&sResult);` |
|        5 |  231 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_INT) ){` |
|        - |  232 | `			/* Extract method return value */` |
|        5 |  233 | `			iVal = sResult.x.iVal;` |
|        2 |  234 | `		}` |
|        5 |  235 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|        5 |  236 | `		PH7_MemObjRelease(&sResult);` |
|        5 |  237 | `		return iVal;` |
|        3 |  238 | `	}else if(iFlags & MEMOBJ_RES ){` |
|        3 |  239 | `		return pObj->x.pOther != 0;` |
|        - |  240 | `	}` |
|        - |  241 | `	/* CANT HAPPEN */` |
|      ! 0 |  242 | `	return 0;` |
|      459 |  243 | `}` |
|        - |  244 | `/*` |
|        - |  245 | ` * Return some kind of real value which is the best we can` |
|        - |  246 | ` * do at representing the value that pObj describes as a real.` |
|        - |  247 | ` * If pObj is a real, then the value is exact.If pObj is an` |
|        - |  248 | ` * integer then the integer  is promoted to real and that value` |
|        - |  249 | ` * is returned.` |
|        - |  250 | ` * If pObj is a string, then we make an attempt to convert it` |
|        - |  251 | ` * into a real and return that.` |
|        - |  252 | ` * If pObj represents a NULL value, return 0.0` |
|        - |  253 | ` */` |
|     9456 |  254 | `static ph7_real MemObjRealValue(ph7_value *pObj)` |
|        5 |  255 | `{` |
|        - |  256 | `	sxi32 iFlags;` |
|     9461 |  257 | `	iFlags = pObj->iFlags;` |
|     9461 |  258 | `	if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  259 | `		return pObj->rVal;` |
|     9461 |  260 | `	}else if (iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      771 |  261 | `		return (ph7_real)pObj->x.iVal;` |
|     8693 |  262 | `	}else if (iFlags & MEMOBJ_STRING){` |
|        - |  263 | `		SyString sString;` |
|        - |  264 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  265 | `		ph7_real rVal = 0;` |
|        - |  266 | `#else` |
|     8687 |  267 | `		ph7_real rVal = 0.0;` |
|        - |  268 | `#endif` |
|     8687 |  269 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|     8687 |  270 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|        - |  271 | `			/* Convert as much as we can */` |
|        - |  272 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  273 | `			rVal = MemObjStringToInt(&(*pObj));` |
|        - |  274 | `#else` |
|     8687 |  275 | `			SyStrToReal(sString.zString,sString.nByte,(void *)&rVal,0);` |
|        - |  276 | `#endif` |
|     4341 |  277 | `		}` |
|     8687 |  278 | `		return rVal;` |
|        7 |  279 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|        - |  280 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  281 | `		return 0;` |
|        - |  282 | `#else` |
|      ! 0 |  283 | `		return 0.0;` |
|        - |  284 | `#endif` |
|        7 |  285 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|        - |  286 | `		/* Return the total number of entries in the hashmap */` |
|      ! 0 |  287 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      ! 0 |  288 | `		ph7_real n = (ph7_real)pMap->nEntry;` |
|      ! 0 |  289 | `		PH7_HashmapUnref(pMap);` |
|      ! 0 |  290 | `		return n;` |
|        7 |  291 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|        - |  292 | `		ph7_value sResult;` |
|        5 |  293 | `		ph7_real rVal = 1;` |
|        - |  294 | `		sxi32 rc;` |
|        - |  295 | `		/* Invoke the [__toFloat()] magic method if available [note that this is a symisc extension]  */` |
|        5 |  296 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|        5 |  297 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|        - |  298 | `			"__toFloat",sizeof("__toFloat")-1,&sResult);` |
|        5 |  299 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_REAL) ){` |
|        - |  300 | `			/* Extract method return value */` |
|        5 |  301 | `			rVal = sResult.rVal;` |
|        2 |  302 | `		}` |
|        5 |  303 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|        5 |  304 | `		PH7_MemObjRelease(&sResult);` |
|        5 |  305 | `		return rVal;` |
|        3 |  306 | `	}else if(iFlags & MEMOBJ_RES ){` |
|        3 |  307 | `		return (ph7_real)(pObj->x.pOther != 0);` |
|        - |  308 | `	}` |
|        - |  309 | `	/* NOT REACHED  */` |
|      ! 0 |  310 | `	return 0;` |
|     4733 |  311 | `}` |
|        - |  312 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |  313 | `/*` |
|        - |  314 | ` * Post-process a libc-formatted float into php's exact shape (php_gcvt /` |
|        - |  315 | ` * smart_str_append_double semantics): strip the exponent's zero padding` |
|        - |  316 | ` * (libc's 1e+08 becomes php's 1e+8; a zero exponent stays e+0) and, when` |
|        - |  317 | ` * bGeneric is set (%g-style output, including the default float->string` |
|        - |  318 | ` * cast), make an exponent-form mantissa keep a fractional digit` |
|        - |  319 | ` * (1e+20 -> 1.0e+20). zBuf must be NUL-terminated with at least two bytes` |
|        - |  320 | ` * of spare capacity past the NUL. Returns the new length.` |
|        - |  321 | ` * Defined here (not builtin.c) because the float->string cast below needs it` |
|        - |  322 | ` * even when builtin.c's formatting region is compiled out` |
|        - |  323 | ` * (PH7_DISABLE_DISK_IO); the printf family reuses it from PH7_InputFormat.` |
|        - |  324 | ` */` |
|      514 |  325 | `PH7_PRIVATE sxi32 PH7_PhpFloatShape(char *zBuf,sxi32 nLen,int bGeneric)` |
|        5 |  326 | `{` |
|        - |  327 | `	sxi32 iExp,i;` |
|      519 |  328 | `	iExp = nLen - 1;` |
|     4417 |  329 | `	while( iExp > 0 && zBuf[iExp] != 'e' && zBuf[iExp] != 'E' ){` |
|     3903 |  330 | `		iExp--;` |
|        5 |  331 | `	}` |
|      519 |  332 | `	if( iExp <= 0 ){` |
|      473 |  333 | `		return nLen; /* No exponent part (fixed notation) */` |
|        - |  334 | `	}` |
|        - |  335 | `	{` |
|       47 |  336 | `		sxi32 iDig = iExp + 1;` |
|        - |  337 | `		sxi32 iFirst;` |
|       47 |  338 | `		if( zBuf[iDig] == '+' \|\| zBuf[iDig] == '-' ){` |
|       47 |  339 | `			iDig++;` |
|       23 |  340 | `		}` |
|       47 |  341 | `		iFirst = iDig;` |
|       83 |  342 | `		while( zBuf[iFirst] == '0' && iFirst + 1 < nLen` |
|       61 |  343 | `		 && zBuf[iFirst+1] >= '0' && zBuf[iFirst+1] <= '9' ){` |
|       25 |  344 | `			iFirst++;` |
|        1 |  345 | `		}` |
|       47 |  346 | `		if( iFirst > iDig ){` |
|       25 |  347 | `			sxi32 nStrip = iFirst - iDig;` |
|       73 |  348 | `			for( i = iDig ; i + nStrip <= nLen ; i++ ){` |
|       49 |  349 | `				zBuf[i] = zBuf[i+nStrip]; /* moves the NUL too */` |
|       25 |  350 | `			}` |
|       25 |  351 | `			nLen -= nStrip;` |
|       12 |  352 | `		}` |
|        - |  353 | `	}` |
|       47 |  354 | `	if( bGeneric ){` |
|       31 |  355 | `		int bHasDot = 0;` |
|       63 |  356 | `		for( i = 0 ; i < iExp ; i++ ){` |
|       45 |  357 | `			if( zBuf[i] == '.' ){ bHasDot = 1; break; }` |
|       17 |  358 | `		}` |
|       31 |  359 | `		if( !bHasDot ){` |
|      107 |  360 | `			for( i = nLen ; i >= iExp ; i-- ){` |
|       89 |  361 | `				zBuf[i+2] = zBuf[i]; /* moves the NUL too */` |
|       45 |  362 | `			}` |
|       19 |  363 | `			zBuf[iExp] = '.';` |
|       19 |  364 | `			zBuf[iExp+1] = '0';` |
|       19 |  365 | `			nLen += 2;` |
|        9 |  366 | `		}` |
|       15 |  367 | `	}` |
|       47 |  368 | `	return nLen;` |
|      262 |  369 | `}` |
|        - |  370 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|        - |  371 | `/*` |
|        - |  372 | ` * Return the string representation of a given ph7_value.` |
|        - |  373 | ` * This function never fail and always return SXRET_OK.` |
|        - |  374 | ` */` |
|    59342 |  375 | `static sxi32 MemObjStringValue(SyBlob *pOut,ph7_value *pObj,sxu8 bStrictBool)` |
|        5 |  376 | `{` |
|    59347 |  377 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|        - |  378 | `		/* Handle special floating-point values first */` |
|      374 |  379 | `		if( PH7_IS_NAN(pObj->rVal) ){` |
|      ! 0 |  380 | `			SyBlobAppend(&(*pOut),"NAN",3);` |
|      374 |  381 | `		}else if( PH7_IS_INF(pObj->rVal) ){` |
|        5 |  382 | `			if( pObj->rVal < 0.0 ){` |
|      ! 0 |  383 | `				SyBlobAppend(&(*pOut),"-INF",4);` |
|      ! 0 |  384 | `			}else{` |
|        5 |  385 | `				SyBlobAppend(&(*pOut),"INF",3);` |
|        - |  386 | `			}` |
|        3 |  387 | `		}else{` |
|        - |  388 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |  389 | `			/* php's default float->string conversion (echo/concat/cast):` |
|        - |  390 | `			 * zend_gcvt with EG(precision)=14 and an uppercase exponent` |
|        - |  391 | `			 * marker (smart_str_append_double) — 1/3 -> "0.33333333333333",` |
|        - |  392 | `			 * 1e15 -> "1.0E+15", -0.0 -> "-0". libc snprintf supplies` |
|        - |  393 | `			 * correctly-rounded digits; PH7_PhpFloatShape applies php's` |
|        - |  394 | `			 * exponent/fraction quirks. */` |
|        - |  395 | `			char zNum[48]; /* %.14G peaks at ~22 bytes; +2 spare for ".0" */` |
|      370 |  396 | `			sxi32 n = (sxi32)snprintf(zNum,sizeof(zNum),"%.14G",pObj->rVal);` |
|      370 |  397 | `			if( n < 0 \|\| n >= (sxi32)sizeof(zNum) ){` |
|      ! 0 |  398 | `				n = (sxi32)SyStrlen(zNum);` |
|      ! 0 |  399 | `			}` |
|      370 |  400 | `			n = PH7_PhpFloatShape(zNum,n,TRUE);` |
|      370 |  401 | `			SyBlobAppend(&(*pOut),zNum,(sxu32)n);` |
|        - |  402 | `#else` |
|        - |  403 | `			SyBlobFormat(&(*pOut),"%.15g",pObj->rVal);` |
|        - |  404 | `#endif` |
|        4 |  405 | `		}` |
|    59162 |  406 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|    58671 |  407 | `		SyBlobFormat(&(*pOut),"%qd",pObj->x.iVal);` |
|        - |  408 | `		/* %qd (BSD quad) is equivalent to %lld in the libc printf */` |
|    29644 |  409 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|       45 |  410 | `		if( bStrictBool ){` |
|        - |  411 | `			/* Actual string cast: true -> "1", false -> "" (like PHP) */` |
|       45 |  412 | `			if( pObj->x.iVal ){` |
|       32 |  413 | `				SyBlobAppend(&(*pOut),"1",sizeof("1")-1);` |
|       15 |  414 | `			}` |
|        - |  415 | `			/* false produces empty string, nothing to append */` |
|       25 |  416 | `		}else{` |
|        - |  417 | `			/* Display path (var_dump, print_r): show TRUE/FALSE */` |
|      ! 0 |  418 | `			if( pObj->x.iVal ){` |
|      ! 0 |  419 | `				SyBlobAppend(&(*pOut),"TRUE",sizeof("TRUE")-1);` |
|      ! 0 |  420 | `			}else{` |
|      ! 0 |  421 | `				SyBlobAppend(&(*pOut),"FALSE",sizeof("FALSE")-1);` |
|        - |  422 | `			}` |
|        5 |  423 | `		}` |
|      291 |  424 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|      ! 0 |  425 | `		SyBlobAppend(&(*pOut),"Array",sizeof("Array")-1);` |
|      ! 0 |  426 | `		PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
|      271 |  427 | `	}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|        - |  428 | `		ph7_value sResult;` |
|        - |  429 | `		sxi32 rc;` |
|        - |  430 | `		/* Invoke the __toString() method if available */` |
|      179 |  431 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|      179 |  432 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|        - |  433 | `			"__toString",sizeof("__toString")-1,&sResult);` |
|      179 |  434 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_STRING) && SyBlobLength(&sResult.sBlob) > 0){` |
|        - |  435 | `			/* Expand method return value */` |
|      100 |  436 | `			SyBlobDup(&sResult.sBlob,pOut);` |
|       52 |  437 | `		}else{` |
|        - |  438 | `			/* Expand "Object" as requested by the PHP language reference manual */` |
|       82 |  439 | `			SyBlobAppend(&(*pOut),"Object",sizeof("Object")-1);` |
|        - |  440 | `		}` |
|      179 |  441 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|      179 |  442 | `		PH7_MemObjRelease(&sResult);` |
|      180 |  443 | `	}else if(pObj->iFlags & MEMOBJ_RES ){` |
|        3 |  444 | `		SyBlobFormat(&(*pOut),"ResourceID_%#x",pObj->x.pOther);` |
|        1 |  445 | `	}` |
|    59347 |  446 | `	return SXRET_OK;` |
|        5 |  447 | `}` |
|        - |  448 | `/*` |
|        - |  449 | ` * Return some kind of boolean value which is the best we can do` |
|        - |  450 | ` * at representing the value that pObj describes as a boolean.` |
|        - |  451 | ` * When converting to boolean, the following values are considered FALSE` |
|        - |  452 | ` * (php's exact set):` |
|        - |  453 | ` * NULL` |
|        - |  454 | ` * the boolean FALSE itself.` |
|        - |  455 | ` * the integer 0 (zero).` |
|        - |  456 | ` * the real 0.0 (zero).` |
|        - |  457 | ` * the empty string "" and the string "0" (nothing else: "00", "0.0", " ",` |
|        - |  458 | ` * and "false" are all TRUE in php — the historical PH7 zero-stream and` |
|        - |  459 | ` * "false"/"on"/"yes" special cases changed the meaning of valid PHP source` |
|        - |  460 | ` * and were removed under the §10 PH7-ism policy).` |
|        - |  461 | ` * an array with zero elements.` |
|        - |  462 | ` */` |
|    17914 |  463 | `static sxi32 MemObjBooleanValue(ph7_value *pObj)` |
|        5 |  464 | `{` |
|        - |  465 | `	sxi32 iFlags;` |
|    17919 |  466 | `	iFlags = pObj->iFlags;` |
|    17919 |  467 | `	if (iFlags & MEMOBJ_REAL ){` |
|        - |  468 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  469 | `		return pObj->rVal ? 1 : 0;` |
|        - |  470 | `#else` |
|       14 |  471 | `		return pObj->rVal != 0.0 ? 1 : 0;` |
|        - |  472 | `#endif` |
|    17907 |  473 | `	}else if( iFlags & MEMOBJ_INT ){` |
|      823 |  474 | `		return pObj->x.iVal ? 1 : 0;` |
|    17089 |  475 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|        - |  476 | `		SyString sString;` |
|      111 |  477 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|        - |  478 | `		/* php: a string is FALSE iff it is empty or exactly "0" */` |
|      111 |  479 | `		if( sString.nByte == 0 ){` |
|       19 |  480 | `			return 0;` |
|        - |  481 | `		}` |
|       94 |  482 | `		if( sString.nByte == 1 && sString.zString[0] == '0' ){` |
|        7 |  483 | `			return 0;` |
|        - |  484 | `		}` |
|       88 |  485 | `		return 1;` |
|    16981 |  486 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|    15711 |  487 | `		return 0;` |
|     1275 |  488 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|       22 |  489 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|       22 |  490 | `		sxu32 n = pMap->nEntry;` |
|       22 |  491 | `		PH7_HashmapUnref(pMap);` |
|       22 |  492 | `		return n > 0 ? TRUE : FALSE;` |
|     1255 |  493 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|        - |  494 | `		ph7_value sResult;` |
|      176 |  495 | `		sxi32 iVal = 1;` |
|        - |  496 | `		sxi32 rc;` |
|        - |  497 | `		/* Invoke the __toBool() method if available [note that this is a symisc extension]  */` |
|      176 |  498 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|      176 |  499 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|        - |  500 | `			"__toBool",sizeof("__toBool")-1,&sResult);` |
|      176 |  501 | `		if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL)) ){` |
|        - |  502 | `			/* Extract method return value */` |
|        5 |  503 | `			iVal = (sxi32)(sResult.x.iVal != 0); /* Stupid cc warning -W -Wall -O6 */` |
|        2 |  504 | `		}` |
|      176 |  505 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|      176 |  506 | `		PH7_MemObjRelease(&sResult);` |
|      176 |  507 | `		return iVal;` |
|     1081 |  508 | `	}else if(iFlags & MEMOBJ_RES ){` |
|     1081 |  509 | `		return pObj->x.pOther != 0;` |
|        - |  510 | `	}` |
|        - |  511 | `	/* NOT REACHED */` |
|      ! 0 |  512 | `	return 0;` |
|     8962 |  513 | `}` |
|        - |  514 | `/*` |
|        - |  515 | ` * If the ph7_value is of type real,try to make it an integer also.` |
|        - |  516 | ` */` |
|    10628 |  517 | `static sxi32 MemObjTryIntger(ph7_value *pObj)` |
|        5 |  518 | `{` |
|    10633 |  519 | `	pObj->x.iVal = MemObjRealToInt(&(*pObj));` |
|        - |  520 | `  /* Only mark the value as an integer if` |
|        - |  521 | `  **` |
|        - |  522 | `  **    (1) the round-trip conversion real->int->real is a no-op, and` |
|        - |  523 | `  **    (2) The integer is neither the largest nor the smallest` |
|        - |  524 | `  **        possible integer` |
|        - |  525 | `  **` |
|        - |  526 | `  ** The second and third terms in the following conditional enforces` |
|        - |  527 | `  ** the second condition under the assumption that addition overflow causes` |
|        - |  528 | `  ** values to wrap around.  On x86 hardware, the third term is always` |
|        - |  529 | `  ** true and could be omitted.  But we leave it in because other` |
|        - |  530 | `  ** architectures might behave differently.` |
|        - |  531 | `  */` |
|    10628 |  532 | `	if( pObj->rVal ==(ph7_real)pObj->x.iVal && pObj->x.iVal>SMALLEST_INT64` |
|     9185 |  533 | `      && pObj->x.iVal<LARGEST_INT64 ){` |
|     9169 |  534 | `		  pObj->iFlags \|= MEMOBJ_INT;` |
|     4584 |  535 | `	}` |
|    10633 |  536 | `	return SXRET_OK;` |
|        5 |  537 | `}` |
|        - |  538 | `/*` |
|        - |  539 | ` * Convert a ph7_value to type integer.Invalidate any prior representations.` |
|        - |  540 | ` */` |
|   515462 |  541 | `PH7_PRIVATE sxi32 PH7_MemObjToInteger(ph7_value *pObj)` |
|        5 |  542 | `{` |
|   515467 |  543 | `	if( (pObj->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  544 | `		/* Preform the conversion */` |
|      913 |  545 | `		pObj->x.iVal = MemObjIntValue(&(*pObj));` |
|        - |  546 | `		/* Invalidate any prior representations */` |
|      913 |  547 | `		SyBlobRelease(&pObj->sBlob);` |
|      913 |  548 | `		MemObjSetType(pObj,MEMOBJ_INT);` |
|      454 |  549 | `	}` |
|   515467 |  550 | `	return SXRET_OK;` |
|        5 |  551 | `}` |
|        - |  552 | `/*` |
|        - |  553 | ` * Convert a ph7_value to type real (Try to get an integer representation also).` |
|        - |  554 | ` * Invalidate any prior representations` |
|        - |  555 | ` */` |
|    10450 |  556 | `PH7_PRIVATE sxi32 PH7_MemObjToReal(ph7_value *pObj)` |
|        5 |  557 | `{` |
|    10455 |  558 | `	if((pObj->iFlags & MEMOBJ_REAL) == 0 ){` |
|        - |  559 | `		/* Preform the conversion */` |
|     9461 |  560 | `		pObj->rVal = MemObjRealValue(&(*pObj));` |
|        - |  561 | `		/* Invalidate any prior representations */` |
|     9461 |  562 | `		SyBlobRelease(&pObj->sBlob);` |
|     9461 |  563 | `		MemObjSetType(pObj,MEMOBJ_REAL);` |
|        - |  564 | `		/* Try to get an integer representation */` |
|     9461 |  565 | `		MemObjTryIntger(&(*pObj));` |
|     4728 |  566 | `	}` |
|    10455 |  567 | `	return SXRET_OK;` |
|        5 |  568 | `}` |
|        - |  569 | `/*` |
|        - |  570 | ` * Convert a ph7_value to type boolean.Invalidate any prior representations.` |
|        - |  571 | ` */` |
|    21220 |  572 | `PH7_PRIVATE sxi32 PH7_MemObjToBool(ph7_value *pObj)` |
|        5 |  573 | `{` |
|    21225 |  574 | `	if( (pObj->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        - |  575 | `		/* Preform the conversion */` |
|    17919 |  576 | `		pObj->x.iVal = MemObjBooleanValue(&(*pObj));` |
|        - |  577 | `		/* Invalidate any prior representations */` |
|    17919 |  578 | `		SyBlobRelease(&pObj->sBlob);` |
|    17919 |  579 | `		MemObjSetType(pObj,MEMOBJ_BOOL);` |
|     8957 |  580 | `	}` |
|    21225 |  581 | `	return SXRET_OK;` |
|        5 |  582 | `}` |
|        - |  583 | `/*` |
|        - |  584 | ` * Convert a ph7_value to type string.Prior representations are NOT invalidated.` |
|        - |  585 | ` */` |
|   972239 |  586 | `PH7_PRIVATE sxi32 PH7_MemObjToString(ph7_value *pObj)` |
|        5 |  587 | `{` |
|   972244 |  588 | `	sxi32 rc = SXRET_OK;` |
|   972244 |  589 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  590 | `		/* Perform the conversion */` |
|    59253 |  591 | `		SyBlobReset(&pObj->sBlob); /* Reset the internal buffer */` |
|    59253 |  592 | `		rc = MemObjStringValue(&pObj->sBlob,&(*pObj),TRUE);` |
|    59253 |  593 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|    29624 |  594 | `	}` |
|   972244 |  595 | `	return rc;` |
|        5 |  596 | `}` |
|        - |  597 | `/*` |
|        - |  598 | ` * Nullify a ph7_value.In other words invalidate any prior` |
|        - |  599 | ` * representation.` |
|        - |  600 | ` */` |
|      ! 0 |  601 | `PH7_PRIVATE sxi32 PH7_MemObjToNull(ph7_value *pObj)` |
|      ! 0 |  602 | `{` |
|      ! 0 |  603 | `	return PH7_MemObjRelease(pObj);` |
|      ! 0 |  604 | `}` |
|        - |  605 | `/*` |
|        - |  606 | ` * Convert a ph7_value to type array.Invalidate any prior representations.` |
|        - |  607 | `  * According to the PHP language reference manual.` |
|        - |  608 | `  *   For any of the types: integer, float, string, boolean converting a value` |
|        - |  609 | `  *   to an array results in an array with a single element with index zero` |
|        - |  610 | `  *   and the value of the scalar which was converted.` |
|        - |  611 | `  */` |
|      550 |  612 | `PH7_PRIVATE sxi32 PH7_MemObjToHashmap(ph7_value *pObj)` |
|        4 |  613 | `{` |
|      554 |  614 | `	if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  615 | `		ph7_hashmap *pMap;` |
|        - |  616 | `		/* Allocate a new hashmap instance */` |
|      358 |  617 | `		pMap = PH7_NewHashmap(pObj->pVm,0,0);` |
|      358 |  618 | `		if( pMap == 0 ){` |
|      ! 0 |  619 | `			return SXERR_MEM;` |
|        - |  620 | `		}` |
|      358 |  621 | `		if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|        - |  622 | `			/*` |
|        - |  623 | `			 * According to the PHP language reference manual.` |
|        - |  624 | `			 *   For any of the types: integer, float, string, boolean converting a value` |
|        - |  625 | `			 *   to an array results in an array with a single element with index zero` |
|        - |  626 | `			 *   and the value of the scalar which was converted.` |
|        - |  627 | `			 */` |
|       29 |  628 | `			if( pObj->iFlags & MEMOBJ_OBJ ){` |
|        - |  629 | `				/* Object cast */` |
|       15 |  630 | `				PH7_ClassInstanceToHashmap((ph7_class_instance *)pObj->x.pOther,pMap);` |
|        8 |  631 | `			}else{` |
|        - |  632 | `				/* Insert a single element */` |
|       15 |  633 | `				PH7_HashmapInsert(pMap,0/* Automatic index assign */,&(*pObj));` |
|        - |  634 | `			}` |
|       29 |  635 | `			SyBlobRelease(&pObj->sBlob);` |
|       14 |  636 | `		}` |
|        - |  637 | `		/* Invalidate any prior representation */` |
|      358 |  638 | `		PH7_MemObjRelease(pObj);` |
|      358 |  639 | `		MemObjSetType(pObj,MEMOBJ_HASHMAP);` |
|      358 |  640 | `		pObj->x.pOther = pMap;` |
|      177 |  641 | `	}` |
|      554 |  642 | `	return SXRET_OK;` |
|      279 |  643 | `}` |
|        - |  644 | `/* Per-entry callback for the array branch of the (object) cast: add one dynamic` |
|        - |  645 | ` * property to the target stdClass, named by the array key (rendered as a string,` |
|        - |  646 | ` * matching PHP) and holding a copy of the value. */` |
|        - |  647 | `struct VmObjCastData { ph7_vm *pVm; ph7_class_instance *pStd; };` |
|       38 |  648 | `static int VmArrayToObjectWalk(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  649 | `{` |
|       39 |  650 | `	struct VmObjCastData *pData = (struct VmObjCastData *)pUserData;` |
|        - |  651 | `	ph7_value *pSlot;` |
|        - |  652 | `	/* pKey and pValue are walk-owned temporaries (PH7_HashmapWalk passes pointers to` |
|        - |  653 | `	 * its own stack-local sKey/sValue, not slots inside pVm->aMemObj), so they survive` |
|        - |  654 | `	 * the slot reservation inside PH7_VmCreateDynamicAttr — no snapshot needed. pKey is` |
|        - |  655 | `	 * safe to coerce in place. */` |
|       39 |  656 | `	PH7_MemObjToString(pKey);` |
|       58 |  657 | `	pSlot = PH7_VmCreateDynamicAttr(pData->pVm,pData->pStd,` |
|       38 |  658 | `		(const char *)SyBlobData(&pKey->sBlob),(sxu32)SyBlobLength(&pKey->sBlob),0);` |
|       39 |  659 | `	if( pSlot ){` |
|       39 |  660 | `		PH7_MemObjStore(pValue,pSlot);` |
|       19 |  661 | `	}` |
|       39 |  662 | `	return SXRET_OK;` |
|        1 |  663 | `}` |
|        - |  664 | `/*` |
|        - |  665 | ` * Convert a ph7_value to type object, invalidating any prior representation.` |
|        - |  666 | ` * The new object is a (PHP-empty) stdClass populated with dynamic properties,` |
|        - |  667 | ` * matching PHP's (object) cast:` |
|        - |  668 | ` *   - array  -> one property per entry (key rendered as a string -> name).` |
|        - |  669 | ` *   - scalar -> a single property named "scalar".` |
|        - |  670 | ` *   - null   -> an empty stdClass (no properties).` |
|        - |  671 | ` *   - object -> returned unchanged (the MEMOBJ_OBJ guard below).` |
|        - |  672 | ` */` |
|       34 |  673 | `PH7_PRIVATE sxi32 PH7_MemObjToObject(ph7_value *pObj)` |
|        1 |  674 | `{` |
|       35 |  675 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  676 | `		ph7_class_instance *pStd;` |
|        - |  677 | `		ph7_class *pClass;` |
|        - |  678 | `		ph7_vm *pVm;` |
|        - |  679 | `		/* Point to the underlying VM + the stdClass */` |
|       35 |  680 | `		pVm = pObj->pVm;` |
|       52 |  681 | `		pClass = pVm->pStdClass ? pVm->pStdClass` |
|       17 |  682 | `			: PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,0,0);` |
|       35 |  683 | `		if( pClass == 0 ){` |
|        - |  684 | `			/* Can't happen,load null instead */` |
|      ! 0 |  685 | `			PH7_MemObjRelease(pObj);` |
|      ! 0 |  686 | `			return SXRET_OK;` |
|        - |  687 | `		}` |
|        - |  688 | `		/* Instanciate a new (empty) stdClass object */` |
|       35 |  689 | `		pStd = PH7_NewClassInstance(pVm,pClass);` |
|       35 |  690 | `		if( pStd == 0 ){` |
|        - |  691 | `			/* Out of memory */` |
|      ! 0 |  692 | `			PH7_MemObjRelease(pObj);` |
|      ! 0 |  693 | `			return SXRET_OK;` |
|        - |  694 | `		}` |
|       35 |  695 | `		pStd->iRef = 1;` |
|       35 |  696 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  697 | `			/* Array: one dynamic property per entry. */` |
|        - |  698 | `			struct VmObjCastData sData;` |
|       23 |  699 | `			sData.pVm = pVm;` |
|       23 |  700 | `			sData.pStd = pStd;` |
|       23 |  701 | `			ph7_array_walk(pObj,VmArrayToObjectWalk,&sData);` |
|       24 |  702 | `		}else if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  703 | `			/* Scalar (int/float/bool/string): a single "scalar" property. */` |
|       11 |  704 | `			ph7_value *pSlot = PH7_VmCreateDynamicAttr(pVm,pStd,"scalar",sizeof("scalar")-1,0);` |
|       11 |  705 | `			if( pSlot ){` |
|       11 |  706 | `				PH7_MemObjStore(pObj,pSlot);` |
|        5 |  707 | `			}` |
|        5 |  708 | `		}` |
|        - |  709 | `		/* (A NULL source yields an empty stdClass — nothing to populate.) */` |
|        - |  710 | `		/* Invalidate any prior representation */` |
|       35 |  711 | `		PH7_MemObjRelease(pObj);` |
|        - |  712 | `		/* Save the new instance */` |
|       35 |  713 | `		pObj->x.pOther = pStd;` |
|       35 |  714 | `		MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       17 |  715 | `	}` |
|       35 |  716 | `	return SXRET_OK;` |
|       18 |  717 | `}` |
|        - |  718 | `/*` |
|        - |  719 | ` * Return a pointer to the appropriate convertion method associated` |
|        - |  720 | ` * with the given type.` |
|        - |  721 | ` * Note on type juggling.` |
|        - |  722 | ` * Accoding to the PHP language reference manual` |
|        - |  723 | ` *  PHP does not require (or support) explicit type definition in variable` |
|        - |  724 | ` *  declaration; a variable's type is determined by the context in which` |
|        - |  725 | ` *  the variable is used. That is to say, if a string value is assigned` |
|        - |  726 | ` *  to variable $var, $var becomes a string. If an integer value is then` |
|        - |  727 | ` *  assigned to $var, it becomes an integer.` |
|        - |  728 | ` */` |
|       84 |  729 | `PH7_PRIVATE ProcMemObjCast PH7_MemObjCastMethod(sxi32 iFlags)` |
|        5 |  730 | `{` |
|       89 |  731 | `	if( iFlags & MEMOBJ_STRING ){` |
|       16 |  732 | `		return PH7_MemObjToString;` |
|       75 |  733 | `	}else if( iFlags & MEMOBJ_INT ){` |
|       59 |  734 | `		return PH7_MemObjToInteger;` |
|       20 |  735 | `	}else if( iFlags & MEMOBJ_REAL ){` |
|       17 |  736 | `		return PH7_MemObjToReal;` |
|        3 |  737 | `	}else if( iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  738 | `		return PH7_MemObjToBool;` |
|        3 |  739 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|        3 |  740 | `		return PH7_MemObjToHashmap;` |
|      ! 0 |  741 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  742 | `		return PH7_MemObjToObject;` |
|      ! 0 |  743 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|        - |  744 | ``		/* `null` is a type, not a weak-coercion target: never silently cast a`` |
|        - |  745 | ``		 * value to null for a standalone `null` type hint. Return/property`` |
|        - |  746 | `		 * enforcement reject a non-null value before reaching here; this guards` |
|        - |  747 | `		 * the parameter default-value path from quietly nulling a non-null` |
|        - |  748 | `		 * default. */` |
|      ! 0 |  749 | `		return 0;` |
|        - |  750 | `	}` |
|        - |  751 | `	/* NULL cast */` |
|      ! 0 |  752 | `	return PH7_MemObjToNull;` |
|       47 |  753 | `}` |
|        - |  754 | `/*` |
|        - |  755 | ` * Return TRUE only if the entire string held by pValue (optionally surrounded` |
|        - |  756 | ` * by whitespace, with an optional sign) is a well-formed PHP numeric string.` |
|        - |  757 | ` * This mirrors PHP's is_numeric_string grammar used for is_numeric() and the` |
|        - |  758 | ` * loose-comparison numeric gate:` |
|        - |  759 | ` *` |
|        - |  760 | ` *   [ws] [sign] ( D+ [.D*] \| .D+ ) [ (e\|E) [sign] D+ ] [ws]   (whole string)` |
|        - |  761 | ` *` |
|        - |  762 | ` * Implemented directly rather than via SyStrIsNumeric — which returns OK on any` |
|        - |  763 | ` * numeric PREFIX (so it wrongly accepts "10abc"/"0x1A"/"0b101") and requires a` |
|        - |  764 | ` * leading digit (so it wrongly rejects ".5"/"-.5", valid in PHP). Unlike a` |
|        - |  765 | ` * strtod-based classifier this needs no NUL-terminated buffer. Returns FALSE for` |
|        - |  766 | ` * a non-string value.` |
|        - |  767 | ` */` |
|        - |  768 | `/*` |
|        - |  769 | ` * Scan php's numeric-string grammar and report the longest numeric PREFIX.` |
|        - |  770 | ` * Returns 1 when the string starts with a number, 0 when nothing numeric is` |
|        - |  771 | ` * there at all ("abc", "", ".", "e5"). On success *pzTail points just past the` |
|        - |  772 | ` * prefix, so the caller can tell a fully numeric string ("1e3", " 5 ") from a` |
|        - |  773 | ` * merely leading-numeric one ("5abc", "1e", "0x1A") -- php warns on the latter` |
|        - |  774 | ` * and rejects a string with no prefix outright.` |
|        - |  775 | ` */` |
|   237628 |  776 | `PH7_PRIVATE int PH7_MemObjStringNumericPrefix(ph7_value *pValue,const char **pzTail)` |
|        5 |  777 | `{` |
|        - |  778 | `	const char *z, *zEnd;` |
|        - |  779 | `	sxu32 n;` |
|   237633 |  780 | `	int bDigit = 0;` |
|   237633 |  781 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  782 | `		return 0;` |
|        - |  783 | `	}` |
|   237633 |  784 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|   237633 |  785 | `	n = SyBlobLength(&pValue->sBlob);` |
|   237633 |  786 | `	if( n == 0 ){` |
|       81 |  787 | `		return 0;` |
|        - |  788 | `	}` |
|   237555 |  789 | `	zEnd = z + n;` |
|   237581 |  790 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){` |
|       28 |  791 | `		z++;` |
|        2 |  792 | `	}` |
|   237555 |  793 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|       59 |  794 | `		z++;` |
|       27 |  795 | `	}` |
|   238093 |  796 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|      542 |  797 | `		z++; bDigit = 1;` |
|        4 |  798 | `	}` |
|   237555 |  799 | `	if( z < zEnd && z[0] == '.' ){` |
|       77 |  800 | `		z++;` |
|      147 |  801 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|       73 |  802 | `			z++; bDigit = 1;` |
|        3 |  803 | `		}` |
|       36 |  804 | `	}` |
|        - |  805 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|   237555 |  806 | `	if( !bDigit ){` |
|   237119 |  807 | `		return 0;` |
|        - |  808 | `	}` |
|        - |  809 | `	/* Optional exponent — only joins the prefix if it carries a digit. "1e" has` |
|        - |  810 | `	 * the numeric prefix "1" with the 'e' left in the tail, exactly as php reads it. */` |
|      440 |  811 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|       28 |  812 | `		const char *zExp = z;` |
|       28 |  813 | `		z++;` |
|       28 |  814 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|      ! 0 |  815 | `			z++;` |
|      ! 0 |  816 | `		}` |
|       28 |  817 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|       10 |  818 | `			z = zExp;` |
|        6 |  819 | `		}else{` |
|       42 |  820 | `			while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|       24 |  821 | `				z++;` |
|        2 |  822 | `			}` |
|        - |  823 | `		}` |
|       13 |  824 | `	}` |
|      440 |  825 | `	if( pzTail ){` |
|      440 |  826 | `		*pzTail = z;` |
|      218 |  827 | `	}` |
|      440 |  828 | `	return 1;` |
|   118789 |  829 | `}` |
|        - |  830 | `/*` |
|        - |  831 | ` * TRUE only if the WHOLE string is a well-formed php numeric string` |
|        - |  832 | ` * (trailing whitespace allowed, nothing else).` |
|        - |  833 | ` */` |
|   237378 |  834 | `PH7_PRIVATE int PH7_MemObjStringIsNumeric(ph7_value *pValue)` |
|        5 |  835 | `{` |
|   237383 |  836 | `	const char *zTail = 0, *zEnd;` |
|   237383 |  837 | `	if( !PH7_MemObjStringNumericPrefix(pValue,&zTail) ){` |
|   237187 |  838 | `		return 0;` |
|        - |  839 | `	}` |
|      200 |  840 | `	zEnd = (const char *)SyBlobData(&pValue->sBlob) + SyBlobLength(&pValue->sBlob);` |
|      206 |  841 | `	while( zTail < zEnd && (unsigned char)zTail[0] < 0xc0 && SyisSpace(zTail[0]) ){` |
|        8 |  842 | `		zTail++;` |
|        2 |  843 | `	}` |
|      200 |  844 | `	return zTail == zEnd ? 1 : 0;` |
|   118664 |  845 | `}` |
|        - |  846 | `/*` |
|        - |  847 | ` * Check whether the ph7_value is numeric [i.e: int/float/bool] or looks` |
|        - |  848 | ` * like a numeric number [i.e: if the ph7_value is of type string.].` |
|        - |  849 | ` * Return TRUE if numeric.FALSE otherwise.` |
|        - |  850 | ` */` |
|   238226 |  851 | `PH7_PRIVATE sxi32 PH7_MemObjIsNumeric(ph7_value *pObj)` |
|        5 |  852 | `{` |
|   238231 |  853 | `	if( pObj->iFlags & ( MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      597 |  854 | `		return TRUE;` |
|   237639 |  855 | `	}else if( pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|      374 |  856 | `		return FALSE;` |
|   237267 |  857 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        - |  858 | `		/* TRUE only if the whole string is a well-formed PHP numeric string. */` |
|   237267 |  859 | `		return PH7_MemObjStringIsNumeric(pObj) ? TRUE : FALSE;` |
|        - |  860 | `	}` |
|        - |  861 | `	/* NOT REACHED */` |
|      ! 0 |  862 | `	return FALSE;` |
|   119088 |  863 | `}` |
|        - |  864 | `/*` |
|        - |  865 | ` * Check whether the ph7_value is empty.Return TRUE if empty.` |
|        - |  866 | ` * FALSE otherwise.` |
|        - |  867 | ` * An ph7_value is considered empty if the following are true:` |
|        - |  868 | ` * NULL value.` |
|        - |  869 | ` * Boolean FALSE.` |
|        - |  870 | ` * Integer/Float with a 0 (zero) value.` |
|        - |  871 | ` * An empty string or a stream of 0 (zero) [i.e: "0","00","000",...].` |
|        - |  872 | ` * An empty array.` |
|        - |  873 | ` * NOTE` |
|        - |  874 | ` *  OBJECT VALUE MUST NOT BE MODIFIED.` |
|        - |  875 | ` */` |
|    33654 |  876 | `PH7_PRIVATE sxi32 PH7_MemObjIsEmpty(ph7_value *pObj)` |
|        5 |  877 | `{` |
|    33659 |  878 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|       21 |  879 | `		return TRUE;` |
|    33641 |  880 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|       22 |  881 | `		return pObj->x.iVal == 0 ? TRUE : FALSE;` |
|    33621 |  882 | `	}else if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  883 | `		return pObj->rVal == (ph7_real)0 ? TRUE : FALSE;` |
|    33621 |  884 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|        5 |  885 | `		return !pObj->x.iVal;` |
|    33617 |  886 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|    27033 |  887 | `		if( SyBlobLength(&pObj->sBlob) <= 0 ){` |
|    21515 |  888 | `			return TRUE;` |
|      ! 0 |  889 | `		}else{` |
|        - |  890 | `			const char *zIn,*zEnd;` |
|     5523 |  891 | `			zIn = (const char *)SyBlobData(&pObj->sBlob);` |
|     5523 |  892 | `			zEnd = &zIn[SyBlobLength(&pObj->sBlob)];` |
|     5529 |  893 | `			while( zIn < zEnd ){` |
|     5529 |  894 | `				if( zIn[0] != '0' ){` |
|     5523 |  895 | `					break;` |
|        - |  896 | `				}` |
|        7 |  897 | `				zIn++;` |
|        1 |  898 | `			}` |
|     5523 |  899 | `			return zIn >= zEnd ? TRUE : FALSE;` |
|      ! 0 |  900 | `		}` |
|     6589 |  901 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|     6589 |  902 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|     6589 |  903 | `		return pMap->nEntry == 0 ? TRUE : FALSE;` |
|      ! 0 |  904 | `	}else if ( pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|      ! 0 |  905 | `		return FALSE;` |
|        - |  906 | `	}` |
|        - |  907 | `	/* Assume empty by default */` |
|      ! 0 |  908 | `	return TRUE;` |
|    16832 |  909 | `}` |
|        - |  910 | `/*` |
|        - |  911 | ` * Convert a ph7_value so that it has types MEMOBJ_REAL or MEMOBJ_INT` |
|        - |  912 | ` * or both.` |
|        - |  913 | ` * Invalidate any prior representations. Every effort is made to force` |
|        - |  914 | ` * the conversion, even if the input is a string that does not look` |
|        - |  915 | ` * completely like a number.Convert as much of the string as we can` |
|        - |  916 | ` * and ignore the rest.` |
|        - |  917 | ` */` |
|   584969 |  918 | `PH7_PRIVATE sxi32 PH7_MemObjToNumeric(ph7_value *pObj)` |
|        5 |  919 | `{` |
|   584974 |  920 | `	if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|   584772 |  921 | `		if( pObj->iFlags & (MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|        5 |  922 | `			if( pObj->iFlags & MEMOBJ_NULL ){` |
|        3 |  923 | `				pObj->x.iVal = 0;` |
|        1 |  924 | `			}` |
|        5 |  925 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|        2 |  926 | `		}` |
|        - |  927 | `		/* Already numeric */` |
|   584772 |  928 | `		return  SXRET_OK;` |
|        - |  929 | `	}` |
|      206 |  930 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|      206 |  931 | `		const char *zTail = 0;` |
|      206 |  932 | `		int bNum, bReal = 0;` |
|        - |  933 | `		/* php reads the longest numeric PREFIX and its shape decides the type: a` |
|        - |  934 | `		 * '.' or a *complete* exponent inside that prefix makes it a float, else an` |
|        - |  935 | `		 * int. Deciding from the raw string instead mistyped "1e" as float(1) --` |
|        - |  936 | `		 * php sees the prefix "1" there and yields int(1). */` |
|      206 |  937 | `		bNum = PH7_MemObjStringNumericPrefix(pObj,&zTail);` |
|      206 |  938 | `		if( bNum ){` |
|      206 |  939 | `			const char *z = (const char *)SyBlobData(&pObj->sBlob);` |
|      486 |  940 | `			while( z < zTail ){` |
|      306 |  941 | `				if( z[0] == '.' \|\| z[0] == 'e' \|\| z[0] == 'E' ){` |
|       23 |  942 | `					bReal = 1;` |
|       23 |  943 | `					break;` |
|        - |  944 | `				}` |
|      284 |  945 | `				z++;` |
|        4 |  946 | `			}` |
|      101 |  947 | `		}` |
|      206 |  948 | `		if( bReal ){` |
|       23 |  949 | `			PH7_MemObjToReal(&(*pObj));` |
|       12 |  950 | `		}else{` |
|      184 |  951 | `			if( !bNum ){` |
|        - |  952 | `				/* The input does not look at all like a number,set the value to 0 */` |
|      ! 0 |  953 | `				pObj->x.iVal = 0;` |
|      ! 0 |  954 | `			}else{` |
|        - |  955 | `				/* Convert as much as we can */` |
|      184 |  956 | `				pObj->x.iVal = MemObjStringToInt(&(*pObj));` |
|        - |  957 | `			}` |
|      184 |  958 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|      184 |  959 | `			SyBlobRelease(&pObj->sBlob);` |
|        4 |  960 | `		}` |
|      101 |  961 | `	}else if(pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)){` |
|      ! 0 |  962 | `		PH7_MemObjToInteger(pObj);` |
|      ! 0 |  963 | `	}else{` |
|        - |  964 | `		/* Perform a blind cast */` |
|      ! 0 |  965 | `		PH7_MemObjToReal(&(*pObj));` |
|        - |  966 | `	}` |
|      206 |  967 | `	return SXRET_OK;` |
|   292513 |  968 | `}` |
|        - |  969 | `/*` |
|        - |  970 | ` * Apply Perl-style increment to a string ph7_value in place.` |
|        - |  971 | ` * Walks the bytes right-to-left: digits 0-8 / letters a-y, A-Y bump in` |
|        - |  972 | ` * place; '9' wraps to '0' with carry; 'z' to 'a'; 'Z' to 'A'. A non-` |
|        - |  973 | ` * alphanumeric byte stops the walk without prepending. If carry survives` |
|        - |  974 | ` * past index 0, prepend '1', 'a', or 'A' depending on the class of the` |
|        - |  975 | ` * last carried character. Empty strings become "1".` |
|        - |  976 | ` *` |
|        - |  977 | ` * Caller must ensure pObj is MEMOBJ_STRING and not a numeric string;` |
|        - |  978 | ` * this routine never reclassifies the type, so a result like "e0" stays` |
|        - |  979 | ` * a string even though it looks numeric.` |
|        - |  980 | ` */` |
|       50 |  981 | `PH7_PRIVATE sxi32 PH7_MemObjStringIncrement(ph7_value *pObj)` |
|        2 |  982 | `{` |
|        - |  983 | `	enum CarryClass { CARRY_NONE = 0, CARRY_LOWER, CARRY_UPPER, CARRY_DIGIT };` |
|       52 |  984 | `	enum CarryClass last_class = CARRY_NONE;` |
|        - |  985 | `	sxu32 nLen, pos;` |
|        - |  986 | `	sxu8 *zStr;` |
|       52 |  987 | `	int carry = 1;` |
|        - |  988 | `	int ch;` |
|        - |  989 | `	/* Force ownership: the blob may be SXBLOB_RDONLY (e.g., from` |
|        - |  990 | `	 * PH7_MemObjLoad), in which case BlobPrepareGrow copies on demand` |
|        - |  991 | `	 * and clears the flag.  On an already-owned blob with spare capacity` |
|        - |  992 | `	 * (the common case under PHL's growth allocator), this is a no-op` |
|        - |  993 | `	 * append; on an exact-fit owned blob it triggers a single realloc. */` |
|       52 |  994 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       48 |  995 | `		SyBlobNullAppend(&pObj->sBlob);` |
|       23 |  996 | `	}` |
|       52 |  997 | `	nLen = SyBlobLength(&pObj->sBlob);` |
|       52 |  998 | `	if( nLen == 0 ){` |
|        5 |  999 | `		SyBlobAppend(&pObj->sBlob,"1",sizeof(char));` |
|        5 | 1000 | `		return SXRET_OK;` |
|        - | 1001 | `	}` |
|       48 | 1002 | `	zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|       48 | 1003 | `	pos = nLen;` |
|      100 | 1004 | `	while( pos > 0 ){` |
|       82 | 1005 | `		pos--;` |
|       82 | 1006 | `		ch = zStr[pos];` |
|       82 | 1007 | `		if( ch >= 'a' && ch <= 'z' ){` |
|       48 | 1008 | `			if( ch == 'z' ){` |
|       29 | 1009 | `				zStr[pos] = 'a';` |
|       29 | 1010 | `				last_class = CARRY_LOWER;` |
|       29 | 1011 | `				continue;` |
|        - | 1012 | `			}` |
|       20 | 1013 | `			zStr[pos]++;` |
|       20 | 1014 | `			carry = 0;` |
|       20 | 1015 | `			break;` |
|       35 | 1016 | `		}else if( ch >= 'A' && ch <= 'Z' ){` |
|       21 | 1017 | `			if( ch == 'Z' ){` |
|       19 | 1018 | `				zStr[pos] = 'A';` |
|       19 | 1019 | `				last_class = CARRY_UPPER;` |
|       19 | 1020 | `				continue;` |
|        - | 1021 | `			}` |
|        3 | 1022 | `			zStr[pos]++;` |
|        3 | 1023 | `			carry = 0;` |
|        3 | 1024 | `			break;` |
|       15 | 1025 | `		}else if( ch >= '0' && ch <= '9' ){` |
|        7 | 1026 | `			if( ch == '9' ){` |
|        7 | 1027 | `				zStr[pos] = '0';` |
|        7 | 1028 | `				last_class = CARRY_DIGIT;` |
|        7 | 1029 | `				continue;` |
|        - | 1030 | `			}` |
|      ! 0 | 1031 | `			zStr[pos]++;` |
|      ! 0 | 1032 | `			carry = 0;` |
|      ! 0 | 1033 | `			break;` |
|      ! 0 | 1034 | `		}else{` |
|        - | 1035 | `			/* non-alphanumeric: stop without prepending */` |
|        9 | 1036 | `			carry = 0;` |
|        9 | 1037 | `			break;` |
|        - | 1038 | `		}` |
|      ! 0 | 1039 | `	}` |
|       48 | 1040 | `	if( carry ){` |
|        - | 1041 | `		sxu8 prepend;` |
|        - | 1042 | `		sxu32 i;` |
|       19 | 1043 | `		switch( last_class ){` |
|        9 | 1044 | `			case CARRY_LOWER: prepend = (sxu8)'a'; break;` |
|       11 | 1045 | `			case CARRY_UPPER: prepend = (sxu8)'A'; break;` |
|      ! 0 | 1046 | `			default:          prepend = (sxu8)'1'; break;` |
|        - | 1047 | `		}` |
|        - | 1048 | `		/* Append a sentinel byte to grow nByte by 1 (capacity grows too). */` |
|       19 | 1049 | `		SyBlobAppend(&pObj->sBlob,"\0",sizeof(char));` |
|       19 | 1050 | `		zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|       19 | 1051 | `		nLen = SyBlobLength(&pObj->sBlob);` |
|        - | 1052 | `		/* Shift right by 1, walking from the end so overlapping is safe. */` |
|       57 | 1053 | `		for( i = nLen - 1; i > 0; i-- ){` |
|       39 | 1054 | `			zStr[i] = zStr[i - 1];` |
|       20 | 1055 | `		}` |
|       19 | 1056 | `		zStr[0] = prepend;` |
|        9 | 1057 | `	}` |
|       48 | 1058 | `	return SXRET_OK;` |
|       27 | 1059 | `}` |
|        - | 1060 | `/*` |
|        - | 1061 | ` * Try a get an integer representation of the given ph7_value.` |
|        - | 1062 | ` * If the ph7_value is not of type real,this function is a no-op.` |
|        - | 1063 | ` */` |
|     1104 | 1064 | `PH7_PRIVATE sxi32 PH7_MemObjTryInteger(ph7_value *pObj)` |
|        4 | 1065 | `{` |
|     1108 | 1066 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|        - | 1067 | `		/* Work only with reals */` |
|     1108 | 1068 | `		MemObjTryIntger(&(*pObj));` |
|      552 | 1069 | `	}` |
|     1108 | 1070 | `	return SXRET_OK;` |
|        4 | 1071 | `}` |
|        - | 1072 | `/*` |
|        - | 1073 | ` * Initialize a ph7_value to the null type.` |
|        - | 1074 | ` */` |
| 23805019 | 1075 | `PH7_PRIVATE sxi32 PH7_MemObjInit(ph7_vm *pVm,ph7_value *pObj)` |
|        5 | 1076 | `{` |
|        - | 1077 | `	/* Zero the structure */` |
| 23805024 | 1078 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1079 | `	/* Initialize fields */` |
| 23805024 | 1080 | `	pObj->pVm = pVm;` |
| 23805024 | 1081 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1082 | `	/* Set the NULL type */` |
| 23805024 | 1083 | `	pObj->iFlags = MEMOBJ_NULL;` |
| 23805024 | 1084 | `	return SXRET_OK;` |
|        5 | 1085 | `}` |
|        - | 1086 | `/*` |
|        - | 1087 | ` * Initialize a ph7_value to the integer type.` |
|        - | 1088 | ` */` |
|  5496902 | 1089 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromInt(ph7_vm *pVm,ph7_value *pObj,sxi64 iVal)` |
|        5 | 1090 | `{` |
|        - | 1091 | `	/* Zero the structure */` |
|  5496907 | 1092 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1093 | `	/* Initialize fields */` |
|  5496907 | 1094 | `	pObj->pVm = pVm;` |
|  5496907 | 1095 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1096 | `	/* Set the desired type */` |
|  5496907 | 1097 | `	pObj->x.iVal = iVal;` |
|  5496907 | 1098 | `	pObj->iFlags = MEMOBJ_INT;` |
|  5496907 | 1099 | `	return SXRET_OK;` |
|        5 | 1100 | `}` |
|        - | 1101 | `/*` |
|        - | 1102 | ` * Initialize a ph7_value to the boolean type.` |
|        - | 1103 | ` */` |
|    17100 | 1104 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromBool(ph7_vm *pVm,ph7_value *pObj,sxi32 iVal)` |
|        5 | 1105 | `{` |
|        - | 1106 | `	/* Zero the structure */` |
|    17105 | 1107 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1108 | `	/* Initialize fields */` |
|    17105 | 1109 | `	pObj->pVm = pVm;` |
|    17105 | 1110 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1111 | `	/* Set the desired type */` |
|    17105 | 1112 | `	pObj->x.iVal = iVal ? 1 : 0;` |
|    17105 | 1113 | `	pObj->iFlags = MEMOBJ_BOOL;` |
|    17105 | 1114 | `	return SXRET_OK;` |
|        5 | 1115 | `}` |
|        - | 1116 | `/*` |
|        - | 1117 | ` * Initialize a ph7_value to the real type.` |
|        - | 1118 | ` */` |
|       10 | 1119 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromReal(ph7_vm *pVm,ph7_value *pObj,ph7_real rVal)` |
|        1 | 1120 | `{` |
|        - | 1121 | `	/* Zero the structure */` |
|       11 | 1122 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1123 | `	/* Initialize fields */` |
|       11 | 1124 | `	pObj->pVm = pVm;` |
|       11 | 1125 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1126 | `	/* Set the desired type */` |
|       11 | 1127 | `	pObj->rVal = rVal;` |
|       11 | 1128 | `	pObj->iFlags = MEMOBJ_REAL;` |
|       11 | 1129 | `	return SXRET_OK;` |
|        1 | 1130 | `}` |
|        - | 1131 | `/*` |
|        - | 1132 | ` * Initialize a ph7_value to the array type.` |
|        - | 1133 | ` */` |
|    73888 | 1134 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromArray(ph7_vm *pVm,ph7_value *pObj,ph7_hashmap *pArray)` |
|        5 | 1135 | `{` |
|        - | 1136 | `	/* Zero the structure */` |
|    73893 | 1137 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1138 | `	/* Initialize fields */` |
|    73893 | 1139 | `	pObj->pVm = pVm;` |
|    73893 | 1140 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1141 | `	/* Set the desired type */` |
|    73893 | 1142 | `	pObj->iFlags = MEMOBJ_HASHMAP;` |
|    73893 | 1143 | `	pObj->x.pOther = pArray;` |
|    73893 | 1144 | `	return SXRET_OK;` |
|        5 | 1145 | `}` |
|        - | 1146 | `/*` |
|        - | 1147 | ` * Initialize a ph7_value to the string type.` |
|        - | 1148 | ` */` |
|  4047230 | 1149 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromString(ph7_vm *pVm,ph7_value *pObj,const SyString *pVal)` |
|        5 | 1150 | `{` |
|        - | 1151 | `	/* Zero the structure */` |
|  4047235 | 1152 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1153 | `	/* Initialize fields */` |
|  4047235 | 1154 | `	pObj->pVm = pVm;` |
|  4047235 | 1155 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|  4047235 | 1156 | `	if( pVal ){` |
|        - | 1157 | `		/* Append contents */` |
|  1904319 | 1158 | `		SyBlobAppend(&pObj->sBlob,(const void *)pVal->zString,pVal->nByte);` |
|   952157 | 1159 | `	}` |
|        - | 1160 | `	/* Set the desired type */` |
|  4047235 | 1161 | `	pObj->iFlags = MEMOBJ_STRING;` |
|  4047235 | 1162 | `	return SXRET_OK;` |
|        5 | 1163 | `}` |
|        - | 1164 | `/*` |
|        - | 1165 | ` * Append some contents to the internal buffer of a given ph7_value.` |
|        - | 1166 | ` * If the given ph7_value is not of type string,this function` |
|        - | 1167 | ` * invalidate any prior representation and set the string type.` |
|        - | 1168 | ` * Then a simple append operation is performed.` |
|        - | 1169 | ` */` |
|  2534776 | 1170 | `PH7_PRIVATE sxi32 PH7_MemObjStringAppend(ph7_value *pObj,const char *zData,sxu32 nLen)` |
|        5 | 1171 | `{` |
|        - | 1172 | `	sxi32 rc;` |
|  2534781 | 1173 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 1174 | `		/* Invalidate any prior representation */` |
|     3261 | 1175 | `		PH7_MemObjRelease(pObj);` |
|     3261 | 1176 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|     1628 | 1177 | `	}` |
|        - | 1178 | `	/* Append contents */` |
|  2534781 | 1179 | `	rc = SyBlobAppend(&pObj->sBlob,zData,nLen);` |
|  2534781 | 1180 | `	return rc;` |
|        5 | 1181 | `}` |
|        - | 1182 | `#if 0` |
|        - | 1183 | `/*` |
|        - | 1184 | ` * Format and append some contents to the internal buffer of a given ph7_value.` |
|        - | 1185 | ` * If the given ph7_value is not of type string,this function invalidate` |
|        - | 1186 | ` * any prior representation and set the string type.` |
|        - | 1187 | ` * Then a simple format and append operation is performed.` |
|        - | 1188 | ` */` |
|        - | 1189 | `PH7_PRIVATE sxi32 PH7_MemObjStringFormat(ph7_value *pObj,const char *zFormat,va_list ap)` |
|        - | 1190 | `{` |
|        - | 1191 | `	sxi32 rc;` |
|        - | 1192 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 1193 | `		/* Invalidate any prior representation */` |
|        - | 1194 | `		PH7_MemObjRelease(pObj);` |
|        - | 1195 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|        - | 1196 | `	}` |
|        - | 1197 | `	/* Format and append contents */` |
|        - | 1198 | `	rc = SyBlobFormatAp(&pObj->sBlob,zFormat,ap);` |
|        - | 1199 | `	return rc;` |
|        - | 1200 | `}` |
|        - | 1201 | `#endif` |
|        - | 1202 | `/*` |
|        - | 1203 | ` * Duplicate the contents of a ph7_value.` |
|        - | 1204 | ` */` |
|  5498443 | 1205 | `PH7_PRIVATE sxi32 PH7_MemObjStore(ph7_value *pSrc,ph7_value *pDest)` |
|        5 | 1206 | `{` |
|  5498448 | 1207 | `	ph7_class_instance *pObj = 0;` |
|  5498448 | 1208 | `	ph7_hashmap *pMap = 0;` |
|        - | 1209 | `	sxi32 rc;` |
|  5498448 | 1210 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1211 | `		/* Increment reference count */` |
|   214231 | 1212 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  5391335 | 1213 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|        - | 1214 | `		/* Increment reference count */` |
|     9443 | 1215 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|     4719 | 1216 | `	}` |
|  5498448 | 1217 | `	if( pDest->iFlags & MEMOBJ_HASHMAP ){` |
|    65233 | 1218 | `		pMap = (ph7_hashmap *)pDest->x.pOther;` |
|  5465834 | 1219 | `	}else if( pDest->iFlags & MEMOBJ_OBJ ){` |
|     8205 | 1220 | `		pObj = (ph7_class_instance *)pDest->x.pOther;` |
|     4100 | 1221 | `	}` |
|  5498448 | 1222 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|  5498448 | 1223 | `	pDest->iFlags &= ~MEMOBJ_AUX;` |
|  5498448 | 1224 | `	rc = SXRET_OK;` |
|  5498448 | 1225 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  4215387 | 1226 | `		SyBlobReset(&pDest->sBlob);` |
|  4215387 | 1227 | `		rc = SyBlobDup(&pSrc->sBlob,&pDest->sBlob);` |
|  2107696 | 1228 | `	}else{` |
|  1283066 | 1229 | `		if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|   352586 | 1230 | `			SyBlobRelease(&pDest->sBlob);` |
|   176314 | 1231 | `		}` |
|        - | 1232 | `	}` |
|  5498448 | 1233 | `	if( pMap ){` |
|    65233 | 1234 | `		PH7_HashmapUnref(pMap);` |
|  5465834 | 1235 | `	}else if( pObj ){` |
|     8205 | 1236 | `		PH7_ClassInstanceUnref(pObj);` |
|     4100 | 1237 | `	}` |
|  5498443 | 1238 | `	if( rc == SXRET_OK && (pDest->iFlags & MEMOBJ_HASHMAP)` |
|  2856358 | 1239 | `	 && pDest->pVm` |
|   214226 | 1240 | `	 && (ph7_hashmap *)pDest->x.pOther == pDest->pVm->pGlobal` |
|        - | 1241 | `	 /* Identity, not nIdx: transient values carry nIdx==0 (SyZero), which` |
|        - | 1242 | `	  * collides with a typical nGlobalIdx of 0 and would skip the snapshot` |
|        - | 1243 | `	  * for closure envs and other non-slot destinations. */` |
|   107122 | 1244 | `	 && pDest != (ph7_value *)SySetAt(&pDest->pVm->aMemObj,pDest->pVm->nGlobalIdx) ){` |
|        - | 1245 | `		/* php 8.1: a COPY of $GLOBALS ($snap = $GLOBALS, $a[] = $GLOBALS,` |
|        - | 1246 | `		 * by-value argument passing, return $GLOBALS, ...) is a by-value` |
|        - | 1247 | `		 * SNAPSHOT of the symbol table with its reference entries` |
|        - | 1248 | `		 * flattened — never a live alias. Materialize it here, the one` |
|        - | 1249 | `		 * store choke point (loads/subscript access keep sharing, so` |
|        - | 1250 | `		 * $GLOBALS[$k] reads and writes stay live). */` |
|        9 | 1251 | `		ph7_hashmap *pSnap = PH7_NewHashmap(pDest->pVm,0,0);` |
|        9 | 1252 | `		if( pSnap && PH7_HashmapDupMaterialized((ph7_hashmap *)pDest->x.pOther,pSnap) == SXRET_OK ){` |
|        9 | 1253 | `			PH7_HashmapUnref((ph7_hashmap *)pDest->x.pOther);` |
|        9 | 1254 | `			pDest->x.pOther = pSnap;` |
|        4 | 1255 | `		}else if( pSnap ){` |
|      ! 0 | 1256 | `			PH7_HashmapUnref(pSnap);` |
|      ! 0 | 1257 | `		}` |
|        4 | 1258 | `	}` |
|  5498448 | 1259 | `	return rc;` |
|        5 | 1260 | `}` |
|        - | 1261 | `/*` |
|        - | 1262 | ` * Duplicate the contents of a ph7_value but do not copy internal` |
|        - | 1263 | ` * buffer contents,simply point to it.` |
|        - | 1264 | ` */` |
|  8424592 | 1265 | `PH7_PRIVATE sxi32 PH7_MemObjLoad(ph7_value *pSrc,ph7_value *pDest)` |
|        5 | 1266 | `{` |
|  8424597 | 1267 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),` |
|        - | 1268 | `		sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|  8424597 | 1269 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1270 | `		/* Increment reference count */` |
|   509171 | 1271 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  8170014 | 1272 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|        - | 1273 | `		/* Increment reference count */` |
|    46167 | 1274 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|    23081 | 1275 | `	}` |
|  8424597 | 1276 | `	if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|       46 | 1277 | `		SyBlobRelease(&pDest->sBlob);` |
|       21 | 1278 | `	}` |
|  8424597 | 1279 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  4369189 | 1280 | `		SyBlobReadOnly(&pDest->sBlob,SyBlobData(&pSrc->sBlob),SyBlobLength(&pSrc->sBlob));` |
|  2184579 | 1281 | `	}` |
|  8424597 | 1282 | `	return SXRET_OK;` |
|        5 | 1283 | `}` |
|        - | 1284 | `/*` |
|        - | 1285 | ` * Invalidate any prior representation of a given ph7_value.` |
|        - | 1286 | ` */` |
| 20056537 | 1287 | `PH7_PRIVATE sxi32 PH7_MemObjRelease(ph7_value *pObj)` |
|        5 | 1288 | `{` |
| 20056542 | 1289 | `	if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
| 17122241 | 1290 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|   698165 | 1291 | `			PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
| 16773161 | 1292 | `		}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|    87131 | 1293 | `			PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|    43563 | 1294 | `		}` |
|        - | 1295 | `		/* Release the internal buffer */` |
| 17122241 | 1296 | `		SyBlobRelease(&pObj->sBlob);` |
|        - | 1297 | `		/* Invalidate any prior representation */` |
| 17122241 | 1298 | `		pObj->iFlags = MEMOBJ_NULL;` |
|  8561245 | 1299 | `	}` |
| 20056542 | 1300 | `	return SXRET_OK;` |
|        5 | 1301 | `}` |
|        - | 1302 | `/*` |
|        - | 1303 | ` * Compare two ph7_values.` |
|        - | 1304 | ` * Return 0 if the values are equals, > 0 if pObj1 is greater than pObj2` |
|        - | 1305 | ` * or < 0 if pObj2 is greater than pObj1.` |
|        - | 1306 | ` * Type comparison table taken from the PHP language reference manual.` |
|        - | 1307 | ` * Comparisons of $x with PHP functions Expression` |
|        - | 1308 | ` *              gettype() 	empty() 	is_null() 	isset() 	boolean : if($x)` |
|        - | 1309 | ` * $x = ""; 	string 	    TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1310 | ` * $x = null 	NULL 	    TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1311 | ` * var $x; 	    NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1312 | ` * $x is undefined 	NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1313 | ` *  $x = array(); 	array 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1314 | ` * $x = false; 	boolean 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1315 | ` * $x = true; 	boolean 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1316 | ` * $x = 1; 	    integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1317 | ` * $x = 42; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1318 | ` * $x = 0; 	    integer 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1319 | ` * $x = -1; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1320 | ` * $x = "1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1321 | ` * $x = "0"; 	string 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1322 | ` * $x = "-1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1323 | ` * $x = "php"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1324 | ` * $x = "true"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1325 | ` * $x = "false"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1326 | ` *      Loose comparisons with ==` |
|        - | 1327 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|        - | 1328 | ` * TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1329 | ` * FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|        - | 1330 | ` * 1 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1331 | ` * 0 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE` |
|        - | 1332 | ` * -1 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1333 | ` * "1" 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1334 | ` * "0" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1335 | ` * "-1" 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1336 | ` * NULL 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|        - | 1337 | ` * array() 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1338 | ` * "php" 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1339 | ` * "" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE` |
|        - | 1340 | ` *    Strict comparisons with ===` |
|        - | 1341 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|        - | 1342 | ` * TRUE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1343 | ` * FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1344 | ` * 1 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1345 | ` * 0 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1346 | ` * -1 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1347 | ` * "1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1348 | ` * "0" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1349 | ` * "-1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1350 | ` * NULL 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE` |
|        - | 1351 | ` * array() 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE` |
|        - | 1352 | ` * "php" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1353 | ` * "" 	    FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE` |
|        - | 1354 | ` */` |
|  1635580 | 1355 | `PH7_PRIVATE sxi32 PH7_MemObjCmp(ph7_value *pObj1,ph7_value *pObj2,int bStrict,int iNest)` |
|        5 | 1356 | `{` |
|        - | 1357 | `	sxi32 iComb;` |
|        - | 1358 | `	sxi32 rc;` |
|  1635585 | 1359 | `	if( bStrict ){` |
|        - | 1360 | `		sxi32 iF1,iF2;` |
|        - | 1361 | `		/* Strict comparisons with === */` |
|   839095 | 1362 | `		iF1 = pObj1->iFlags&~MEMOBJ_AUX;` |
|   839095 | 1363 | `		iF2 = pObj2->iFlags&~MEMOBJ_AUX;` |
|   839095 | 1364 | `		if( iF1 != iF2 ){` |
|        - | 1365 | `			/* Not of the same type */` |
|   190021 | 1366 | `			return 1;` |
|        - | 1367 | `		}` |
|   324537 | 1368 | `	}` |
|        - | 1369 | `	/* Combine flag together */` |
|  1445569 | 1370 | `	iComb = pObj1->iFlags\|pObj2->iFlags;` |
|  1445564 | 1371 | `	if( !bStrict` |
|  1121027 | 1372 | `	 && (iComb & MEMOBJ_NULL) != 0` |
|   398277 | 1373 | `	 && (iComb & MEMOBJ_STRING) != 0` |
|       66 | 1374 | `	 && (iComb & (MEMOBJ_BOOL\|MEMOBJ_RES\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|        - | 1375 | `		/*` |
|        - | 1376 | `		 * PHP 8 comparison table: null loosely compared with a STRING is` |
|        - | 1377 | `		 * compared as the empty string (a string comparison), not through` |
|        - | 1378 | `		 * bool coercion — so null == "0" is FALSE and null < "0" is TRUE` |
|        - | 1379 | `		 * (php 7 and the historical PH7 behavior coerced both to bool,` |
|        - | 1380 | `		 * making any non-empty non-"0"-insensitive string "equal" to null).` |
|        - | 1381 | `		 * Convert the null side to "" and let the string branch below run.` |
|        - | 1382 | `		 */` |
|       45 | 1383 | `		if( pObj1->iFlags & MEMOBJ_NULL ){` |
|       35 | 1384 | `			PH7_MemObjToString(pObj1);` |
|       18 | 1385 | `		}else{` |
|       11 | 1386 | `			PH7_MemObjToString(pObj2);` |
|        - | 1387 | `		}` |
|       45 | 1388 | `		iComb = pObj1->iFlags\|pObj2->iFlags;` |
|       22 | 1389 | `	}` |
|  1445569 | 1390 | `	if( iComb & (MEMOBJ_NULL\|MEMOBJ_RES\|MEMOBJ_BOOL) ){` |
|        - | 1391 | `		/* Convert to boolean: Keep in mind FALSE < TRUE */` |
|    23783 | 1392 | `		if( (pObj1->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     8897 | 1393 | `			PH7_MemObjToBool(pObj1);` |
|     4446 | 1394 | `		}` |
|    23783 | 1395 | `		if( (pObj2->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     7867 | 1396 | `			PH7_MemObjToBool(pObj2);` |
|     3931 | 1397 | `		}` |
|    23783 | 1398 | `		return (sxi32)((pObj1->x.iVal != 0) - (pObj2->x.iVal != 0));` |
|  1421791 | 1399 | `	}else if ( iComb & MEMOBJ_HASHMAP ){` |
|        - | 1400 | `		/* Hashmap aka 'array' comparison */` |
|       31 | 1401 | `		if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1402 | `			/* Array is always greater */` |
|      ! 0 | 1403 | `			return -1;` |
|        - | 1404 | `		}` |
|       31 | 1405 | `		if( (pObj2->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1406 | `			/* Array is always greater */` |
|      ! 0 | 1407 | `			return 1;` |
|        - | 1408 | `		}` |
|        - | 1409 | `		/* Perform the comparison */` |
|       31 | 1410 | `		rc = PH7_HashmapCmp((ph7_hashmap *)pObj1->x.pOther,(ph7_hashmap *)pObj2->x.pOther,bStrict);` |
|       31 | 1411 | `		return rc;` |
|  1421761 | 1412 | `	}else if(iComb & MEMOBJ_OBJ ){` |
|        - | 1413 | `		/* Object comparison */` |
|      277 | 1414 | `		if( (pObj1->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 1415 | `			/* Object is always greater */` |
|      ! 0 | 1416 | `			return -1;` |
|        - | 1417 | `		}` |
|      277 | 1418 | `		if( (pObj2->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 1419 | `			/* Object is always greater */` |
|      ! 0 | 1420 | `			return 1;` |
|        - | 1421 | `		}` |
|        - | 1422 | `		/* Perform the comparison */` |
|      277 | 1423 | `		rc = PH7_ClassInstanceCmp((ph7_class_instance *)pObj1->x.pOther,(ph7_class_instance *)pObj2->x.pOther,bStrict,iNest);` |
|      277 | 1424 | `		return rc;` |
|  1421489 | 1425 | `	}else if ( iComb & MEMOBJ_STRING ){` |
|        - | 1426 | `		SyString s1,s2;` |
|   859935 | 1427 | `		if( !bStrict ){` |
|        - | 1428 | `			/*` |
|        - | 1429 | `			 * PHP 8 "saner string to number comparisons" (RFC): a numeric` |
|        - | 1430 | `			 * comparison is performed only when BOTH operands are numbers or` |
|        - | 1431 | `			 * numeric strings. A number compared with a NON-numeric string is` |
|        - | 1432 | `			 * compared as strings, with the number cast to its string form —` |
|        - | 1433 | `			 * so 0 == "abc" is false, "abc" < 10 is false, and max("abc",10)` |
|        - | 1434 | `			 * is "abc". (PHP 7 cast the non-numeric string to 0 and compared` |
|        - | 1435 | `			 * numerically; comparing when EITHER side was numeric is what this` |
|        - | 1436 | `			 * replaces.) Two non-numeric strings, or one numeric and one` |
|        - | 1437 | `			 * non-numeric string, still fall through to the string comparison` |
|        - | 1438 | `			 * below, unchanged.` |
|        - | 1439 | `			 */` |
|   236847 | 1440 | `			if( PH7_MemObjIsNumeric(pObj1) && PH7_MemObjIsNumeric(pObj2) ){` |
|        - | 1441 | `				/* Perform a numeric comparison */` |
|       31 | 1442 | `				goto Numeric;` |
|        - | 1443 | `			}` |
|   118376 | 1444 | `		}` |
|        - | 1445 | `		/* Perform a strict string comparison.*/` |
|   859905 | 1446 | `		if( (pObj1->iFlags&MEMOBJ_STRING) == 0 ){` |
|       23 | 1447 | `			PH7_MemObjToString(pObj1);` |
|       11 | 1448 | `		}` |
|   859905 | 1449 | `		if( (pObj2->iFlags&MEMOBJ_STRING) == 0 ){` |
|        7 | 1450 | `			PH7_MemObjToString(pObj2);` |
|        3 | 1451 | `		}` |
|   859905 | 1452 | `		SyStringInitFromBuf(&s1,SyBlobData(&pObj1->sBlob),SyBlobLength(&pObj1->sBlob));` |
|   859905 | 1453 | `		SyStringInitFromBuf(&s2,SyBlobData(&pObj2->sBlob),SyBlobLength(&pObj2->sBlob));` |
|        - | 1454 | `		/*` |
|        - | 1455 | `		 * Strings are compared using memcmp(). If one value is an exact prefix of the` |
|        - | 1456 | `		 * other, then the shorter value is less than the longer value.` |
|        - | 1457 | `		 */` |
|   859905 | 1458 | `		rc = SyMemcmp((const void *)s1.zString,(const void *)s2.zString,SXMIN(s1.nByte,s2.nByte));` |
|   859905 | 1459 | `		if( rc == 0 ){` |
|   285527 | 1460 | `			if( s1.nByte != s2.nByte ){` |
|    15711 | 1461 | `				rc = s1.nByte < s2.nByte ? -1 : 1;` |
|     7854 | 1462 | `			}` |
|   142762 | 1463 | `		}` |
|   859905 | 1464 | `		return rc;` |
|   561559 | 1465 | `	}else if( iComb & (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|   280754 | 1466 | `Numeric:` |
|        - | 1467 | `		/* Perform a numeric comparison if one of the operand is numeric(integer or real) */` |
|   561589 | 1468 | `		if( (pObj1->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|       13 | 1469 | `			PH7_MemObjToNumeric(pObj1);` |
|        6 | 1470 | `		}` |
|   561589 | 1471 | `		if( (pObj2->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|       19 | 1472 | `			PH7_MemObjToNumeric(pObj2);` |
|        9 | 1473 | `		}` |
|   561589 | 1474 | `		if( (pObj1->iFlags & pObj2->iFlags & MEMOBJ_INT) == 0) {` |
|        - | 1475 | `			/*` |
|        - | 1476 | `			 * Symisc eXtension to the PHP language:` |
|        - | 1477 | `			 *  Floating point comparison is introduced and works as expected.` |
|        - | 1478 | `			 */` |
|        - | 1479 | `			ph7_real r1,r2;` |
|        - | 1480 | `			/* Compare as reals */` |
|      314 | 1481 | `			if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|       11 | 1482 | `				PH7_MemObjToReal(pObj1);` |
|        5 | 1483 | `			}` |
|      314 | 1484 | `			r1 = pObj1->rVal;` |
|      314 | 1485 | `			if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|       56 | 1486 | `				PH7_MemObjToReal(pObj2);` |
|       27 | 1487 | `			}` |
|      314 | 1488 | `			r2 = pObj2->rVal;` |
|      314 | 1489 | `			if( PH7_IS_NAN(r1) \|\| PH7_IS_NAN(r2) ){` |
|        - | 1490 | `				/*` |
|        - | 1491 | `				 * Keep a strict three-way comparator contract even for NaN values.` |
|        - | 1492 | `				 * For ordering purposes, NaN compares equal to NaN and greater than` |
|        - | 1493 | `				 * any non-NaN numeric value.` |
|        - | 1494 | `				 */` |
|       50 | 1495 | `				if( PH7_IS_NAN(r1) ){` |
|       40 | 1496 | `					return PH7_IS_NAN(r2) ? 0 : 1;` |
|        - | 1497 | `				}` |
|       11 | 1498 | `				return -1;` |
|        - | 1499 | `			}` |
|      266 | 1500 | `			if( r1 > r2 ){` |
|       54 | 1501 | `				return 1;` |
|      214 | 1502 | `			}else if( r1 < r2 ){` |
|      134 | 1503 | `				return -1;` |
|        - | 1504 | `			}` |
|       82 | 1505 | `			return 0;` |
|      ! 0 | 1506 | `		}else{` |
|        - | 1507 | `			/* Integer comparison */` |
|   561277 | 1508 | `			if( pObj1->x.iVal > pObj2->x.iVal ){` |
|     7026 | 1509 | `				return 1;` |
|   554256 | 1510 | `			}else if( pObj1->x.iVal < pObj2->x.iVal ){` |
|   544308 | 1511 | `				return -1;` |
|        - | 1512 | `			}` |
|     9953 | 1513 | `			return 0;` |
|        - | 1514 | `		}` |
|        - | 1515 | `	}` |
|        - | 1516 | `	/* NOT REACHED */` |
|      ! 0 | 1517 | `	return 0;` |
|   817788 | 1518 | `}` |
|        - | 1519 | `/*` |
|        - | 1520 | ` * Perform an addition operation of two ph7_values.` |
|        - | 1521 | ` * The reason this function is implemented here rather than 'vm.c'` |
|        - | 1522 | ` * is that the '+' operator is overloaded.` |
|        - | 1523 | ` * That is,the '+' operator is used for arithmetic operation and also` |
|        - | 1524 | ` * used for operation on arrays [i.e: union]. When used with an array` |
|        - | 1525 | ` * The + operator returns the right-hand array appended to the left-hand array.` |
|        - | 1526 | ` * For keys that exist in both arrays, the elements from the left-hand array` |
|        - | 1527 | ` * will be used, and the matching elements from the right-hand array will` |
|        - | 1528 | ` * be ignored.` |
|        - | 1529 | ` * This function take care of handling all the scenarios.` |
|        - | 1530 | ` */` |
|    11130 | 1531 | `PH7_PRIVATE sxi32 PH7_MemObjAdd(ph7_value *pObj1,ph7_value *pObj2,int bAddStore)` |
|        5 | 1532 | `{` |
|    11135 | 1533 | `	if( ((pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1534 | `			/* Arithemtic operation */` |
|     7371 | 1535 | `			PH7_MemObjToNumeric(pObj1);` |
|     7371 | 1536 | `			PH7_MemObjToNumeric(pObj2);` |
|     7371 | 1537 | `			if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_REAL ){` |
|        - | 1538 | `				/* Floating point arithmetic */` |
|        - | 1539 | `				ph7_real a,b;` |
|       69 | 1540 | `				if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|       29 | 1541 | `					PH7_MemObjToReal(pObj1);` |
|       14 | 1542 | `				}` |
|       69 | 1543 | `				if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|        9 | 1544 | `					PH7_MemObjToReal(pObj2);` |
|        4 | 1545 | `				}` |
|       69 | 1546 | `				a = pObj1->rVal;` |
|       69 | 1547 | `				b = pObj2->rVal;` |
|       69 | 1548 | `				pObj1->rVal = a+b;` |
|       69 | 1549 | `				MemObjSetType(pObj1,MEMOBJ_REAL);` |
|        - | 1550 | `				/* Try to get an integer representation also */` |
|       69 | 1551 | `				MemObjTryIntger(&(*pObj1));` |
|       35 | 1552 | `			}else{` |
|        - | 1553 | `				/* Integer arithmetic; PHP promotes an overflowing sum to float.` |
|        - | 1554 | `				 * The integer-only build (PH7_OMIT_FLOATING_POINT) has no float` |
|        - | 1555 | `				 * type, so it wraps like OP_POW's OMIT path. */` |
|        - | 1556 | `				sxi64 a,b,r;` |
|     7303 | 1557 | `				a = pObj1->x.iVal;` |
|     7303 | 1558 | `				b = pObj2->x.iVal;` |
|     7303 | 1559 | `				if( PH7_ADD_OVERFLOW64(a,b,&r) ){` |
|        - | 1560 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        9 | 1561 | `					pObj1->rVal = (ph7_real)a + (ph7_real)b;` |
|        9 | 1562 | `					MemObjSetType(pObj1,MEMOBJ_REAL);` |
|        - | 1563 | `#else` |
|        - | 1564 | `					pObj1->x.iVal = r;` |
|        - | 1565 | `					MemObjSetType(pObj1,MEMOBJ_INT);` |
|        - | 1566 | `#endif` |
|        5 | 1567 | `				}else{` |
|     7295 | 1568 | `					pObj1->x.iVal = r;` |
|     7295 | 1569 | `					MemObjSetType(pObj1,MEMOBJ_INT);` |
|        - | 1570 | `				}` |
|        - | 1571 | `			}` |
|     3688 | 1572 | `	}else{` |
|     3769 | 1573 | `		if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP ){` |
|        - | 1574 | `			ph7_hashmap *pMap;` |
|        - | 1575 | `			sxi32 rc;` |
|     3769 | 1576 | `			if( bAddStore ){` |
|        - | 1577 | `				/* Do not duplicate the hashmap,use the left one since its an add&store operation.` |
|        - | 1578 | `				 */` |
|        3 | 1579 | `				if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1580 | `					/* Force a hashmap cast */` |
|      ! 0 | 1581 | `					rc = PH7_MemObjToHashmap(pObj1);` |
|      ! 0 | 1582 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 1583 | `						PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|      ! 0 | 1584 | `						return rc;` |
|        - | 1585 | `					}` |
|      ! 0 | 1586 | `				}` |
|        - | 1587 | `				/* COW separate before in-place mutation */` |
|        3 | 1588 | `				pMap = PH7_HashmapCowSeparate(pObj1->pVm,pObj1);` |
|        2 | 1589 | `			}else{` |
|        - | 1590 | `				/* Create a new hashmap */` |
|     3767 | 1591 | `				pMap = PH7_NewHashmap(pObj1->pVm,0,0);` |
|     3767 | 1592 | `				if( pMap == 0){` |
|      ! 0 | 1593 | `					PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|      ! 0 | 1594 | `					return SXERR_MEM;` |
|        - | 1595 | `				}` |
|        - | 1596 | `			}` |
|     3769 | 1597 | `			if( !bAddStore ){` |
|     3767 | 1598 | `				if(pObj1->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1599 | `					/* Perform a hashmap duplication */` |
|     3767 | 1600 | `					PH7_HashmapDup((ph7_hashmap *)pObj1->x.pOther,pMap);` |
|     1886 | 1601 | `				}else{` |
|      ! 0 | 1602 | `					if((pObj1->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 1603 | `						/* Simple insertion */` |
|      ! 0 | 1604 | `						PH7_HashmapInsert(pMap,0,pObj1);` |
|      ! 0 | 1605 | `					}` |
|        - | 1606 | `				}` |
|     1881 | 1607 | `			}` |
|        - | 1608 | `			/* Perform the union */` |
|     3769 | 1609 | `			if(pObj2->iFlags & MEMOBJ_HASHMAP ){` |
|     3769 | 1610 | `				PH7_HashmapUnion(pMap,(ph7_hashmap *)pObj2->x.pOther);` |
|     1887 | 1611 | `			}else{` |
|      ! 0 | 1612 | `				if((pObj2->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 1613 | `					/* Simple insertion */` |
|      ! 0 | 1614 | `					PH7_HashmapInsert(pMap,0,pObj2);` |
|      ! 0 | 1615 | `				}` |
|        - | 1616 | `			}` |
|        - | 1617 | `			/* Reflect the change */` |
|     3769 | 1618 | `			if( pObj1->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 1619 | `				SyBlobRelease(&pObj1->sBlob);` |
|      ! 0 | 1620 | `			}` |
|     3769 | 1621 | `			pObj1->x.pOther = pMap;` |
|     3769 | 1622 | `			MemObjSetType(pObj1,MEMOBJ_HASHMAP);` |
|     1882 | 1623 | `		}` |
|        - | 1624 | `	}` |
|    11135 | 1625 | `	return SXRET_OK;` |
|     5570 | 1626 | `}` |
|        - | 1627 | `/*` |
|        - | 1628 | ` * Return a printable representation of the type of a given` |
|        - | 1629 | ` * ph7_value.` |
|        - | 1630 | ` */` |
|      ! 0 | 1631 | `PH7_PRIVATE const char * PH7_MemObjTypeDump(ph7_value *pVal)` |
|      ! 0 | 1632 | `{` |
|      ! 0 | 1633 | `	const char *zType = "";` |
|      ! 0 | 1634 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|      ! 0 | 1635 | `		zType = "null";` |
|      ! 0 | 1636 | `	}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|        - | 1637 | `		/* REAL is authoritative over a cached MEMOBJ_INT: an integer-valued` |
|        - | 1638 | `		 * real (e.g. 1.0) is reported as "double", matching PHP's gettype(). */` |
|      ! 0 | 1639 | `		zType = "double";` |
|      ! 0 | 1640 | `	}else if( pVal->iFlags & MEMOBJ_INT ){` |
|      ! 0 | 1641 | `		zType = "int";` |
|      ! 0 | 1642 | `	}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 1643 | `		zType = "string";` |
|      ! 0 | 1644 | `	}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 | 1645 | `		zType = "bool";` |
|      ! 0 | 1646 | `	}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|      ! 0 | 1647 | `		zType = "array";` |
|      ! 0 | 1648 | `	}else if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 1649 | `		zType = "object";` |
|      ! 0 | 1650 | `	}else if( pVal->iFlags & MEMOBJ_RES ){` |
|      ! 0 | 1651 | `		zType = "resource";` |
|      ! 0 | 1652 | `	}` |
|      ! 0 | 1653 | `	return zType;` |
|      ! 0 | 1654 | `}` |
|        - | 1655 | `/*` |
|        - | 1656 | ` * Dump a ph7_value [i.e: get a printable representation of it's type and contents.].` |
|        - | 1657 | ` * Store the dump in the given blob.` |
|        - | 1658 | ` */` |
|        - | 1659 | `/*` |
|        - | 1660 | ` * php's var_dump float shape (serialize_precision = -1): the SHORTEST decimal` |
|        - | 1661 | ` * string that round-trips to the same double — 0.1+0.2 dumps every digit` |
|        - | 1662 | ` * (0.30000000000000004), 1.0 dumps "1" — pushed through the same` |
|        - | 1663 | ` * exponent/fraction normalization as echo (PH7_PhpFloatShape: uppercase E,` |
|        - | 1664 | ` * "1.0E+100"). Distinct from echo/casts, which use EG(precision)=14.` |
|        - | 1665 | ` */` |
|        4 | 1666 | `static void MemObjDumpRealValue(SyBlob *pOut,ph7_real rVal)` |
|        1 | 1667 | `{` |
|        5 | 1668 | `	if( PH7_IS_NAN(rVal) ){` |
|      ! 0 | 1669 | `		SyBlobAppend(&(*pOut),"NAN",3);` |
|      ! 0 | 1670 | `		return;` |
|        - | 1671 | `	}` |
|        5 | 1672 | `	if( PH7_IS_INF(rVal) ){` |
|      ! 0 | 1673 | `		SyBlobAppend(&(*pOut),rVal < 0.0 ? "-INF" : "INF",rVal < 0.0 ? 4 : 3);` |
|      ! 0 | 1674 | `		return;` |
|        - | 1675 | `	}` |
|        - | 1676 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - | 1677 | `	{` |
|        - | 1678 | `		char zNum[48];` |
|        5 | 1679 | `		sxi32 n = 0;` |
|        - | 1680 | `		int p;` |
|        7 | 1681 | `		for( p = 1 ; p <= 17 ; p++ ){` |
|        7 | 1682 | `			n = (sxi32)snprintf(zNum,sizeof(zNum),"%.*G",p,rVal);` |
|        7 | 1683 | `			if( n < 0 \|\| n >= (sxi32)sizeof(zNum) ){` |
|      ! 0 | 1684 | `				n = (sxi32)SyStrlen(zNum);` |
|      ! 0 | 1685 | `			}` |
|        7 | 1686 | `			if( strtod(zNum,0) == rVal ){` |
|        5 | 1687 | `				break; /* shortest round-trip found */` |
|        - | 1688 | `			}` |
|        2 | 1689 | `		}` |
|        5 | 1690 | `		n = PH7_PhpFloatShape(zNum,n,TRUE);` |
|        5 | 1691 | `		SyBlobAppend(&(*pOut),zNum,(sxu32)n);` |
|        - | 1692 | `	}` |
|        - | 1693 | `#else` |
|        - | 1694 | `	SyBlobFormat(&(*pOut),"%.15g",rVal);` |
|        - | 1695 | `#endif` |
|        3 | 1696 | `}` |
|        - | 1697 | `/*` |
|        - | 1698 | ` * Emit a value's print_r INLINE representation (php: the echo conversion,` |
|        - | 1699 | ` * except true -> "1" and false/null -> ""). Containers never come through` |
|        - | 1700 | ` * here — the entry renderers recurse into the container dumpers instead.` |
|        - | 1701 | ` */` |
|      218 | 1702 | `PH7_PRIVATE void PH7_MemObjPrintRInline(SyBlob *pOut,ph7_value *pObj)` |
|        2 | 1703 | `{` |
|      220 | 1704 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|        7 | 1705 | `		return;` |
|        - | 1706 | `	}` |
|      214 | 1707 | `	if( pObj->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 | 1708 | `		if( pObj->x.iVal != 0 ){` |
|      ! 0 | 1709 | `			SyBlobAppend(&(*pOut),"1",sizeof(char));` |
|      ! 0 | 1710 | `		}` |
|      ! 0 | 1711 | `		return;` |
|        - | 1712 | `	}` |
|      214 | 1713 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|        - | 1714 | `		/* Strings already hold their bytes (MemObjStringValue only CONVERTS` |
|        - | 1715 | `		 * non-strings into the output) */` |
|      120 | 1716 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      120 | 1717 | `			SyBlobAppend(&(*pOut),SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|       59 | 1718 | `		}` |
|      120 | 1719 | `		return;` |
|        - | 1720 | `	}` |
|       96 | 1721 | `	MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|      111 | 1722 | `}` |
|      526 | 1723 | `PH7_PRIVATE sxi32 PH7_MemObjDump(` |
|        - | 1724 | `	SyBlob *pOut,      /* Store the dump here */` |
|        - | 1725 | `	ph7_value *pObj,   /* Dump this */` |
|        - | 1726 | `	int ShowType,      /* TRUE for var_dump; FALSE for print_r */` |
|        - | 1727 | `	int nTab,          /* Indent in SPACES: var_dump = this value's own line;` |
|        - | 1728 | `	                    * print_r = the container's parenthesis column */` |
|        - | 1729 | `	int nDepth,        /* Nesting level */` |
|        - | 1730 | `	int isRef          /* TRUE if referenced entry (var_dump prints '&') */` |
|        - | 1731 | `	)` |
|        4 | 1732 | `{` |
|      530 | 1733 | `	sxi32 rc = SXRET_OK;` |
|        - | 1734 | `	int i;` |
|      530 | 1735 | `	if( !ShowType ){` |
|        - | 1736 | `		/* ---- print_r ---- php prints scalars inline with NO newline; only` |
|        - | 1737 | `		 * containers render the Array/Object block (which the container` |
|        - | 1738 | `		 * dumpers terminate with ")\n"). References carry no marker. */` |
|      112 | 1739 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|      105 | 1740 | `			return PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,FALSE,nTab,nDepth+1);` |
|        - | 1741 | `		}` |
|        8 | 1742 | `		if( (pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|        6 | 1743 | `			return PH7_ClassInstanceDump(&(*pOut),(ph7_class_instance *)pObj->x.pOther,FALSE,nTab,nDepth+1);` |
|        - | 1744 | `		}` |
|        3 | 1745 | `		PH7_MemObjPrintRInline(&(*pOut),pObj);` |
|        3 | 1746 | `		return SXRET_OK;` |
|        - | 1747 | `	}` |
|        - | 1748 | `	/* ---- var_dump ---- every value renders on its own line at nTab spaces,` |
|        - | 1749 | `	 * php's exact shapes: bool(true), NULL, int(n), float(shortest),` |
|        - | 1750 | `	 * string(N) "s", array(N) { … }, object(C)#id (n) { … }, &-references. */` |
|     4476 | 1751 | `	for( i = 0 ; i < nTab ; i++ ){` |
|     4059 | 1752 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     2031 | 1753 | `	}` |
|      420 | 1754 | `	if( isRef ){` |
|        7 | 1755 | `		SyBlobAppend(&(*pOut),"&",sizeof(char));` |
|        3 | 1756 | `	}` |
|      420 | 1757 | `	if( (pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|      139 | 1758 | `		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|      139 | 1759 | `		if( pInst->pClass->iFlags & PH7_CLASS_ENUM ){` |
|        - | 1760 | ``			/* php 8.1: var_dump of an enum case prints `enum(S::A)` — no body */`` |
|      ! 0 | 1761 | `			ph7_value *pName = PH7_EnumCaseNameValue(pInst);` |
|      ! 0 | 1762 | `			SyBlobFormat(&(*pOut),"enum(%z::",&pInst->pClass->sName);` |
|      ! 0 | 1763 | `			if( pName && SyBlobLength(&pName->sBlob) > 0 ){` |
|      ! 0 | 1764 | `				SyBlobAppend(&(*pOut),SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob));` |
|      ! 0 | 1765 | `			}` |
|      ! 0 | 1766 | `			SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|      ! 0 | 1767 | `			return SXRET_OK;` |
|        - | 1768 | `		}` |
|      139 | 1769 | `		rc = PH7_ClassInstanceDump(&(*pOut),pInst,TRUE,nTab,nDepth+1);` |
|      139 | 1770 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      139 | 1771 | `		return rc;` |
|        - | 1772 | `	}` |
|      284 | 1773 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|        3 | 1774 | `		SyBlobAppend(&(*pOut),"NULL\n",sizeof("NULL\n")-1);` |
|        3 | 1775 | `		return SXRET_OK;` |
|        - | 1776 | `	}` |
|      282 | 1777 | `	if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       14 | 1778 | `		rc = PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,TRUE,nTab,nDepth+1);` |
|       14 | 1779 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       14 | 1780 | `		return rc;` |
|        - | 1781 | `	}` |
|      270 | 1782 | `	if( pObj->iFlags & MEMOBJ_BOOL ){` |
|      101 | 1783 | `		if( pObj->x.iVal != 0 ){` |
|       65 | 1784 | `			SyBlobAppend(&(*pOut),"bool(true)\n",sizeof("bool(true)\n")-1);` |
|       34 | 1785 | `		}else{` |
|       38 | 1786 | `			SyBlobAppend(&(*pOut),"bool(false)\n",sizeof("bool(false)\n")-1);` |
|        - | 1787 | `		}` |
|      101 | 1788 | `		return SXRET_OK;` |
|        - | 1789 | `	}` |
|      171 | 1790 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|        - | 1791 | `		/* Checked BEFORE the int flag: an integer-valued real carries a cached` |
|        - | 1792 | `		 * MEMOBJ_INT view too, and php dumps it as float(1). */` |
|        5 | 1793 | `		SyBlobAppend(&(*pOut),"float(",sizeof("float(")-1);` |
|        5 | 1794 | `		MemObjDumpRealValue(&(*pOut),pObj->rVal);` |
|        5 | 1795 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        5 | 1796 | `		return SXRET_OK;` |
|        - | 1797 | `	}` |
|      167 | 1798 | `	if( pObj->iFlags & MEMOBJ_INT ){` |
|      113 | 1799 | `		SyBlobFormat(&(*pOut),"int(%qd)",pObj->x.iVal);` |
|      113 | 1800 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      113 | 1801 | `		return SXRET_OK;` |
|        - | 1802 | `	}` |
|       57 | 1803 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|       57 | 1804 | `		SyBlobFormat(&(*pOut),"string(%u) \"",SyBlobLength(&pObj->sBlob));` |
|       57 | 1805 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       51 | 1806 | `			SyBlobAppend(&(*pOut),SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|       24 | 1807 | `		}` |
|       57 | 1808 | `		SyBlobAppend(&(*pOut),"\"\n",sizeof("\"\n")-1);` |
|       57 | 1809 | `		return SXRET_OK;` |
|        - | 1810 | `	}` |
|        - | 1811 | ``	/* Resources and anything else: the legacy `type(value)` shape (php's`` |
|        - | 1812 | ``	 * `resource(N) of type (stream)` needs the §8 typed-resource model). */`` |
|        - | 1813 | `	{` |
|      ! 0 | 1814 | `		const char *zType = PH7_MemObjTypeDump(pObj);` |
|      ! 0 | 1815 | `		SyBlobAppend(&(*pOut),zType,SyStrlen(zType));` |
|      ! 0 | 1816 | `		SyBlobAppend(&(*pOut),"(",sizeof(char));` |
|      ! 0 | 1817 | `		MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|      ! 0 | 1818 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        - | 1819 | `	}` |
|      ! 0 | 1820 | `	return rc;` |
|      267 | 1821 | `}` |
|        - | 1822 |  |
