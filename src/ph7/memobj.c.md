# src/ph7/memobj.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 829/1019 lines (81.35%)

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
|        3 |   40 | `	if( a == -1 ){` |
|        1 |   41 | `		return b == SMALLEST_INT64;` |
|        - |   42 | `	}` |
|        3 |   43 | `	if( b == -1 ){` |
|      ! 0 |   44 | `		return a == SMALLEST_INT64;` |
|        - |   45 | `	}` |
|        3 |   46 | `	if( a > 0 ){` |
|        3 |   47 | `		if( b > 0 ){` |
|        3 |   48 | `			return a > LARGEST_INT64 / b;` |
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
|      368 |   65 | `PH7_PRIVATE const char *ph7_type_name(ph7_value *pVal)` |
|        5 |   66 | `{` |
|      373 |   67 | `	if( ph7_value_is_null(pVal) ) return "null";` |
|      335 |   68 | `	if( ph7_value_is_bool(pVal) ) return "bool";` |
|      327 |   69 | `	if( ph7_value_is_int(pVal) ) return "int";` |
|      241 |   70 | `	if( ph7_value_is_float(pVal) ) return "float";` |
|      231 |   71 | `	if( ph7_value_is_string(pVal) ) return "string";` |
|       24 |   72 | `	if( ph7_value_is_array(pVal) ) return "array";` |
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
|    10802 |   94 | `static sxi64 MemObjRealToInt(ph7_value *pObj)` |
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
|    10807 |  111 | `  ph7_real r = pObj->rVal;` |
|    10807 |  112 | `  if( r<(ph7_real)minInt ){` |
|        3 |  113 | `    return minInt;` |
|    10805 |  114 | `  }else if( r>(ph7_real)maxInt ){` |
|        - |  115 | `    /* minInt is correct here - not maxInt.  It turns out that assigning` |
|        - |  116 | `    ** a very large positive number to an integer results in a very large` |
|        - |  117 | `    ** negative integer.  This makes no sense, but it is what x86 hardware` |
|        - |  118 | `    ** does so for compatibility we will do the same in software. */` |
|      180 |  119 | `    return minInt;` |
|      ! 0 |  120 | `  }else{` |
|    10627 |  121 | `    return (sxi64)r;` |
|        - |  122 | `  }` |
|        - |  123 | `#endif` |
|     5406 |  124 | `}` |
|        - |  125 | `/*` |
|        - |  126 | ` * Convert a raw token value typically a stream of digit [i.e: hex,octal,binary or decimal]` |
|        - |  127 | ` * to a 64-bit integer.` |
|        - |  128 | ` */` |
|  3834284 |  129 | `PH7_PRIVATE sxi64 PH7_TokenValueToInt64(SyString *pVal)` |
|        5 |  130 | `{` |
|  3834289 |  131 | `	sxi64 iVal = 0;` |
|  3834289 |  132 | `	if( pVal->nByte <= 0 ){` |
|      ! 0 |  133 | `		return 0;` |
|        - |  134 | `	}` |
|  3834289 |  135 | `	if( pVal->zString[0] == '0' ){` |
|        - |  136 | `		sxi32 c;` |
|  1507471 |  137 | `		if( pVal->nByte == sizeof(char) ){` |
|  1397879 |  138 | `			return 0;` |
|        - |  139 | `		}` |
|   109597 |  140 | `		c = pVal->zString[1];` |
|   109597 |  141 | `		if( c  == 'x' \|\| c == 'X' ){` |
|        - |  142 | `			/* Hex digit stream */` |
|   105335 |  143 | `			SyHexStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|    56932 |  144 | `		}else if( c == 'b' \|\| c == 'B' ){` |
|        - |  145 | `			/* Binary digit stream */` |
|      281 |  146 | `			SyBinaryStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|     4127 |  147 | `		}else if( c == 'o' \|\| c == 'O' ){` |
|        - |  148 | `			/* PHP 8.1 explicit octal 0o/0O: skip the two-char prefix and parse the` |
|        - |  149 | `			 * remaining octal digits (SyOctalStrToInt64 expects no letter prefix). */` |
|       17 |  150 | `			if( pVal->nByte > 2 ){` |
|       17 |  151 | `				SyOctalStrToInt64(pVal->zString + 2,pVal->nByte - 2,(void *)&iVal,0);` |
|        8 |  152 | `			}` |
|        9 |  153 | `		}else{` |
|        - |  154 | `			/* Legacy octal digit stream (leading 0) */` |
|     3971 |  155 | `			SyOctalStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|        - |  156 | `		}` |
|    54801 |  157 | `	}else{` |
|        - |  158 | `		/* Decimal digit stream */` |
|  2326823 |  159 | `		SyStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|        - |  160 | `	}` |
|  2436415 |  161 | `	return iVal;` |
|  1917147 |  162 | `}` |
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
|      174 |  182 | `static sxi32 MemObjCallClassCastMethod(` |
|        - |  183 | `	ph7_vm *pVm,               /* VM that trigger the invocation */` |
|        - |  184 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object] */` |
|        - |  185 | `	const char *zMethod,       /* Magic method name [i.e: __toString] */` |
|        - |  186 | `	sxu32 nLen,                /* Method name length */` |
|        - |  187 | `	ph7_value *pResult         /* OUT: Store the return value of the magic method here */` |
|        - |  188 | `	)` |
|        5 |  189 | `{` |
|        - |  190 | `	ph7_class_method *pMethod;` |
|        - |  191 | `	/* Check if the method is available */` |
|      179 |  192 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,zMethod,nLen);` |
|      179 |  193 | `	if( pMethod == 0 ){` |
|        - |  194 | `		/* No such method */` |
|        3 |  195 | `		return SXERR_NOTFOUND;` |
|        - |  196 | `	}` |
|        - |  197 | `	/* Invoke the desired method */` |
|      177 |  198 | `	PH7_VmCallClassMethod(&(*pVm),&(*pThis),pMethod,&(*pResult),0,0);` |
|        - |  199 | `	/* Method successfully called,pResult should hold the return value */` |
|      177 |  200 | `	return SXRET_OK;` |
|       92 |  201 | `}` |
|        - |  202 | `/*` |
|        - |  203 | ` * Return some kind of integer value which is the best we can` |
|        - |  204 | ` * do at representing the value that pObj describes as an integer.` |
|        - |  205 | ` * If pObj is an integer, then the value is exact. If pObj is` |
|        - |  206 | ` * a floating-point then  the value returned is the integer part.` |
|        - |  207 | ` * If pObj is a string, then we make an attempt to convert it into` |
|        - |  208 | ` * a integer and return that.` |
|        - |  209 | ` * If pObj represents a NULL value, return 0.` |
|        - |  210 | ` */` |
|     1564 |  211 | `static sxi64 MemObjIntValue(ph7_value *pObj)` |
|        5 |  212 | `{` |
|        - |  213 | `	sxi32 iFlags;` |
|     1569 |  214 | `	iFlags = pObj->iFlags;` |
|     1569 |  215 | `	if (iFlags & MEMOBJ_REAL ){` |
|       23 |  216 | `		return MemObjRealToInt(&(*pObj));` |
|     1549 |  217 | `	}else if( iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      168 |  218 | `		return pObj->x.iVal;` |
|     1383 |  219 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|     1365 |  220 | `		return MemObjStringToInt(&(*pObj));` |
|       19 |  221 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|        5 |  222 | `		return 0;` |
|       15 |  223 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|        7 |  224 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|        7 |  225 | `		sxu32 n = pMap->nEntry;` |
|        7 |  226 | `		PH7_HashmapUnref(pMap);` |
|        - |  227 | `		/* Return total number of entries in the hashmap */` |
|        7 |  228 | `		return n;` |
|        9 |  229 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|        - |  230 | `		/* php has NO __toInt(): casting an object to int warns and yields 1. PH7's` |
|        - |  231 | `		 * __toInt() was an extension that changed the meaning of valid php source` |
|        - |  232 | ``		 * (§10), so `(int)$obj` silently returned user data where php diagnoses. */`` |
|        7 |  233 | `		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|        7 |  234 | `		if( pInst && pInst->pClass ){` |
|       10 |  235 | `			VmErrorFormat(pObj->pVm,PH7_CTX_WARNING,` |
|        6 |  236 | `				"Object of class %z could not be converted to int",&pInst->pClass->sName);` |
|        3 |  237 | `		}` |
|        7 |  238 | `		PH7_ClassInstanceUnref(pInst);` |
|        7 |  239 | `		return 1;` |
|        3 |  240 | `	}else if(iFlags & MEMOBJ_RES ){` |
|        - |  241 | `		/* php casts a resource to its ID, not to 1: two distinct resources must not` |
|        - |  242 | `		 * compare equal, which they did while every one of them cast to 1. */` |
|        3 |  243 | `		return (sxi64)PH7_VmResourceId(pObj->pVm,pObj->x.pOther);` |
|        - |  244 | `	}` |
|        - |  245 | `	/* CANT HAPPEN */` |
|      ! 0 |  246 | `	return 0;` |
|      787 |  247 | `}` |
|        - |  248 | `/*` |
|        - |  249 | ` * Return some kind of real value which is the best we can` |
|        - |  250 | ` * do at representing the value that pObj describes as a real.` |
|        - |  251 | ` * If pObj is a real, then the value is exact.If pObj is an` |
|        - |  252 | ` * integer then the integer  is promoted to real and that value` |
|        - |  253 | ` * is returned.` |
|        - |  254 | ` * If pObj is a string, then we make an attempt to convert it` |
|        - |  255 | ` * into a real and return that.` |
|        - |  256 | ` * If pObj represents a NULL value, return 0.0` |
|        - |  257 | ` */` |
|     9628 |  258 | `static ph7_real MemObjRealValue(ph7_value *pObj)` |
|        5 |  259 | `{` |
|        - |  260 | `	sxi32 iFlags;` |
|     9633 |  261 | `	iFlags = pObj->iFlags;` |
|     9633 |  262 | `	if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  263 | `		return pObj->rVal;` |
|     9633 |  264 | `	}else if (iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      767 |  265 | `		return (ph7_real)pObj->x.iVal;` |
|     8869 |  266 | `	}else if (iFlags & MEMOBJ_STRING){` |
|        - |  267 | `		SyString sString;` |
|        - |  268 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  269 | `		ph7_real rVal = 0;` |
|        - |  270 | `#else` |
|     8861 |  271 | `		ph7_real rVal = 0.0;` |
|        - |  272 | `#endif` |
|     8861 |  273 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|     8861 |  274 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|        - |  275 | `			/* Convert as much as we can */` |
|        - |  276 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  277 | `			rVal = MemObjStringToInt(&(*pObj));` |
|        - |  278 | `#else` |
|     8861 |  279 | `			SyStrToReal(sString.zString,sString.nByte,(void *)&rVal,0);` |
|        - |  280 | `#endif` |
|     4428 |  281 | `		}` |
|     8861 |  282 | `		return rVal;` |
|        9 |  283 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|        - |  284 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  285 | `		return 0;` |
|        - |  286 | `#else` |
|      ! 0 |  287 | `		return 0.0;` |
|        - |  288 | `#endif` |
|        9 |  289 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|        - |  290 | `		/* Return the total number of entries in the hashmap */` |
|      ! 0 |  291 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      ! 0 |  292 | `		ph7_real n = (ph7_real)pMap->nEntry;` |
|      ! 0 |  293 | `		PH7_HashmapUnref(pMap);` |
|      ! 0 |  294 | `		return n;` |
|        9 |  295 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|        - |  296 | `		/* php has NO __toFloat(): casting an object to float warns and yields 1.0. */` |
|        7 |  297 | `		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|        7 |  298 | `		if( pInst && pInst->pClass ){` |
|       10 |  299 | `			VmErrorFormat(pObj->pVm,PH7_CTX_WARNING,` |
|        6 |  300 | `				"Object of class %z could not be converted to float",&pInst->pClass->sName);` |
|        3 |  301 | `		}` |
|        7 |  302 | `		PH7_ClassInstanceUnref(pInst);` |
|        7 |  303 | `		return (ph7_real)1.0;` |
|        3 |  304 | `	}else if(iFlags & MEMOBJ_RES ){` |
|        3 |  305 | `		return (ph7_real)PH7_VmResourceId(pObj->pVm,pObj->x.pOther);` |
|        - |  306 | `	}` |
|        - |  307 | `	/* NOT REACHED  */` |
|      ! 0 |  308 | `	return 0;` |
|     4819 |  309 | `}` |
|        - |  310 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |  311 | `/*` |
|        - |  312 | ` * Post-process a libc-formatted float into php's exact shape (php_gcvt /` |
|        - |  313 | ` * smart_str_append_double semantics): strip the exponent's zero padding` |
|        - |  314 | ` * (libc's 1e+08 becomes php's 1e+8; a zero exponent stays e+0) and, when` |
|        - |  315 | ` * bGeneric is set (%g-style output, including the default float->string` |
|        - |  316 | ` * cast), make an exponent-form mantissa keep a fractional digit` |
|        - |  317 | ` * (1e+20 -> 1.0e+20). zBuf must be NUL-terminated with at least two bytes` |
|        - |  318 | ` * of spare capacity past the NUL. Returns the new length.` |
|        - |  319 | ` * Defined here (not builtin.c) because the float->string cast below needs it` |
|        - |  320 | ` * even when builtin.c's formatting region is compiled out` |
|        - |  321 | ` * (PH7_DISABLE_DISK_IO); the printf family reuses it from PH7_InputFormat.` |
|        - |  322 | ` */` |
|      516 |  323 | `PH7_PRIVATE sxi32 PH7_PhpFloatShape(char *zBuf,sxi32 nLen,int bGeneric)` |
|        5 |  324 | `{` |
|        - |  325 | `	sxi32 iExp,i;` |
|      521 |  326 | `	iExp = nLen - 1;` |
|     4415 |  327 | `	while( iExp > 0 && zBuf[iExp] != 'e' && zBuf[iExp] != 'E' ){` |
|     3899 |  328 | `		iExp--;` |
|        5 |  329 | `	}` |
|      521 |  330 | `	if( iExp <= 0 ){` |
|      475 |  331 | `		return nLen; /* No exponent part (fixed notation) */` |
|        - |  332 | `	}` |
|        - |  333 | `	{` |
|       47 |  334 | `		sxi32 iDig = iExp + 1;` |
|        - |  335 | `		sxi32 iFirst;` |
|       47 |  336 | `		if( zBuf[iDig] == '+' \|\| zBuf[iDig] == '-' ){` |
|       47 |  337 | `			iDig++;` |
|       23 |  338 | `		}` |
|       47 |  339 | `		iFirst = iDig;` |
|       83 |  340 | `		while( zBuf[iFirst] == '0' && iFirst + 1 < nLen` |
|       61 |  341 | `		 && zBuf[iFirst+1] >= '0' && zBuf[iFirst+1] <= '9' ){` |
|       25 |  342 | `			iFirst++;` |
|        1 |  343 | `		}` |
|       47 |  344 | `		if( iFirst > iDig ){` |
|       25 |  345 | `			sxi32 nStrip = iFirst - iDig;` |
|       73 |  346 | `			for( i = iDig ; i + nStrip <= nLen ; i++ ){` |
|       49 |  347 | `				zBuf[i] = zBuf[i+nStrip]; /* moves the NUL too */` |
|       25 |  348 | `			}` |
|       25 |  349 | `			nLen -= nStrip;` |
|       12 |  350 | `		}` |
|        - |  351 | `	}` |
|       47 |  352 | `	if( bGeneric ){` |
|       31 |  353 | `		int bHasDot = 0;` |
|       63 |  354 | `		for( i = 0 ; i < iExp ; i++ ){` |
|       45 |  355 | `			if( zBuf[i] == '.' ){ bHasDot = 1; break; }` |
|       17 |  356 | `		}` |
|       31 |  357 | `		if( !bHasDot ){` |
|      107 |  358 | `			for( i = nLen ; i >= iExp ; i-- ){` |
|       89 |  359 | `				zBuf[i+2] = zBuf[i]; /* moves the NUL too */` |
|       45 |  360 | `			}` |
|       19 |  361 | `			zBuf[iExp] = '.';` |
|       19 |  362 | `			zBuf[iExp+1] = '0';` |
|       19 |  363 | `			nLen += 2;` |
|        9 |  364 | `		}` |
|       15 |  365 | `	}` |
|       47 |  366 | `	return nLen;` |
|      263 |  367 | `}` |
|        - |  368 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|        - |  369 | `/*` |
|        - |  370 | ` * Return the string representation of a given ph7_value.` |
|        - |  371 | ` * This function never fail and always return SXRET_OK.` |
|        - |  372 | ` */` |
|    61920 |  373 | `static sxi32 MemObjStringValue(SyBlob *pOut,ph7_value *pObj,sxu8 bStrictBool)` |
|        5 |  374 | `{` |
|    61925 |  375 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|        - |  376 | `		/* Handle special floating-point values first */` |
|      372 |  377 | `		if( PH7_IS_NAN(pObj->rVal) ){` |
|      ! 0 |  378 | `			SyBlobAppend(&(*pOut),"NAN",3);` |
|      372 |  379 | `		}else if( PH7_IS_INF(pObj->rVal) ){` |
|        5 |  380 | `			if( pObj->rVal < 0.0 ){` |
|      ! 0 |  381 | `				SyBlobAppend(&(*pOut),"-INF",4);` |
|      ! 0 |  382 | `			}else{` |
|        5 |  383 | `				SyBlobAppend(&(*pOut),"INF",3);` |
|        - |  384 | `			}` |
|        3 |  385 | `		}else{` |
|        - |  386 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |  387 | `			/* php's default float->string conversion (echo/concat/cast):` |
|        - |  388 | `			 * zend_gcvt with EG(precision)=14 and an uppercase exponent` |
|        - |  389 | `			 * marker (smart_str_append_double) — 1/3 -> "0.33333333333333",` |
|        - |  390 | `			 * 1e15 -> "1.0E+15", -0.0 -> "-0". libc snprintf supplies` |
|        - |  391 | `			 * correctly-rounded digits; PH7_PhpFloatShape applies php's` |
|        - |  392 | `			 * exponent/fraction quirks. */` |
|        - |  393 | `			char zNum[48]; /* %.14G peaks at ~22 bytes; +2 spare for ".0" */` |
|      368 |  394 | `			sxi32 n = (sxi32)snprintf(zNum,sizeof(zNum),"%.14G",pObj->rVal);` |
|      368 |  395 | `			if( n < 0 \|\| n >= (sxi32)sizeof(zNum) ){` |
|      ! 0 |  396 | `				n = (sxi32)SyStrlen(zNum);` |
|      ! 0 |  397 | `			}` |
|      368 |  398 | `			n = PH7_PhpFloatShape(zNum,n,TRUE);` |
|      368 |  399 | `			SyBlobAppend(&(*pOut),zNum,(sxu32)n);` |
|        - |  400 | `#else` |
|        - |  401 | `			SyBlobFormat(&(*pOut),"%.15g",pObj->rVal);` |
|        - |  402 | `#endif` |
|        4 |  403 | `		}` |
|    61741 |  404 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|    61269 |  405 | `		SyBlobFormat(&(*pOut),"%qd",pObj->x.iVal);` |
|        - |  406 | `		/* %qd (BSD quad) is equivalent to %lld in the libc printf */` |
|    30925 |  407 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|       47 |  408 | `		if( bStrictBool ){` |
|        - |  409 | `			/* Actual string cast: true -> "1", false -> "" (like PHP) */` |
|       47 |  410 | `			if( pObj->x.iVal ){` |
|       34 |  411 | `				SyBlobAppend(&(*pOut),"1",sizeof("1")-1);` |
|       16 |  412 | `			}` |
|        - |  413 | `			/* false produces empty string, nothing to append */` |
|       26 |  414 | `		}else{` |
|        - |  415 | `			/* Display path (var_dump, print_r): show TRUE/FALSE */` |
|      ! 0 |  416 | `			if( pObj->x.iVal ){` |
|      ! 0 |  417 | `				SyBlobAppend(&(*pOut),"TRUE",sizeof("TRUE")-1);` |
|      ! 0 |  418 | `			}else{` |
|      ! 0 |  419 | `				SyBlobAppend(&(*pOut),"FALSE",sizeof("FALSE")-1);` |
|        - |  420 | `			}` |
|        5 |  421 | `		}` |
|      272 |  422 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|      ! 0 |  423 | `		SyBlobAppend(&(*pOut),"Array",sizeof("Array")-1);` |
|      ! 0 |  424 | `		PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
|      251 |  425 | `	}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|        - |  426 | `		ph7_value sResult;` |
|        - |  427 | `		sxi32 rc;` |
|        - |  428 | `		/* Invoke the __toString() method if available */` |
|      179 |  429 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|      179 |  430 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|        - |  431 | `			"__toString",sizeof("__toString")-1,&sResult);` |
|      179 |  432 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_STRING) && SyBlobLength(&sResult.sBlob) > 0){` |
|        - |  433 | `			/* Expand method return value */` |
|      100 |  434 | `			SyBlobDup(&sResult.sBlob,pOut);` |
|       52 |  435 | `		}else{` |
|        - |  436 | `			/* Expand "Object" as requested by the PHP language reference manual */` |
|       81 |  437 | `			SyBlobAppend(&(*pOut),"Object",sizeof("Object")-1);` |
|        - |  438 | `		}` |
|      179 |  439 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|      179 |  440 | `		PH7_MemObjRelease(&sResult);` |
|      162 |  441 | `	}else if(pObj->iFlags & MEMOBJ_RES ){` |
|        - |  442 | `		/* php renders a resource as "Resource id #N" with its sequential id; the` |
|        - |  443 | `		 * old "ResourceID_0x<pointer>" leaked an address and matched nothing. */` |
|        5 |  444 | `		SyBlobFormat(&(*pOut),"Resource id #%u",PH7_VmResourceId(pObj->pVm,pObj->x.pOther));` |
|        2 |  445 | `	}` |
|    61925 |  446 | `	return SXRET_OK;` |
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
|    46934 |  463 | `static sxi32 MemObjBooleanValue(ph7_value *pObj)` |
|        5 |  464 | `{` |
|        - |  465 | `	sxi32 iFlags;` |
|    46939 |  466 | `	iFlags = pObj->iFlags;` |
|    46939 |  467 | `	if (iFlags & MEMOBJ_REAL ){` |
|        - |  468 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  469 | `		return pObj->rVal ? 1 : 0;` |
|        - |  470 | `#else` |
|       14 |  471 | `		return pObj->rVal != 0.0 ? 1 : 0;` |
|        - |  472 | `#endif` |
|    46927 |  473 | `	}else if( iFlags & MEMOBJ_INT ){` |
|      929 |  474 | `		return pObj->x.iVal ? 1 : 0;` |
|    46003 |  475 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|        - |  476 | `		SyString sString;` |
|       89 |  477 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|        - |  478 | `		/* php: a string is FALSE iff it is empty or exactly "0" */` |
|       89 |  479 | `		if( sString.nByte == 0 ){` |
|       19 |  480 | `			return 0;` |
|        - |  481 | `		}` |
|       72 |  482 | `		if( sString.nByte == 1 && sString.zString[0] == '0' ){` |
|        7 |  483 | `			return 0;` |
|        - |  484 | `		}` |
|       66 |  485 | `		return 1;` |
|    45917 |  486 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|    44611 |  487 | `		return 0;` |
|     1311 |  488 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|       20 |  489 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|       20 |  490 | `		sxu32 n = pMap->nEntry;` |
|       20 |  491 | `		PH7_HashmapUnref(pMap);` |
|       20 |  492 | `		return n > 0 ? TRUE : FALSE;` |
|     1293 |  493 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|        - |  494 | `		/* php has NO __toBool(): an object is ALWAYS truthy, with no diagnostic.` |
|        - |  495 | ``		 * PH7's __toBool() could make `if ($obj)` take the other branch, so this`` |
|        - |  496 | `		 * extension changed control flow in valid php source. */` |
|      196 |  497 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|      196 |  498 | `		return 1;` |
|     1099 |  499 | `	}else if(iFlags & MEMOBJ_RES ){` |
|     1099 |  500 | `		return pObj->x.pOther != 0;` |
|        - |  501 | `	}` |
|        - |  502 | `	/* NOT REACHED */` |
|      ! 0 |  503 | `	return 0;` |
|    23472 |  504 | `}` |
|        - |  505 | `/*` |
|        - |  506 | ` * If the ph7_value is of type real,try to make it an integer also.` |
|        - |  507 | ` */` |
|    10782 |  508 | `static sxi32 MemObjTryIntger(ph7_value *pObj)` |
|        5 |  509 | `{` |
|    10787 |  510 | `	pObj->x.iVal = MemObjRealToInt(&(*pObj));` |
|        - |  511 | `  /* Only mark the value as an integer if` |
|        - |  512 | `  **` |
|        - |  513 | `  **    (1) the round-trip conversion real->int->real is a no-op, and` |
|        - |  514 | `  **    (2) The integer is neither the largest nor the smallest` |
|        - |  515 | `  **        possible integer` |
|        - |  516 | `  **` |
|        - |  517 | `  ** The second and third terms in the following conditional enforces` |
|        - |  518 | `  ** the second condition under the assumption that addition overflow causes` |
|        - |  519 | `  ** values to wrap around.  On x86 hardware, the third term is always` |
|        - |  520 | `  ** true and could be omitted.  But we leave it in because other` |
|        - |  521 | `  ** architectures might behave differently.` |
|        - |  522 | `  */` |
|    10782 |  523 | `	if( pObj->rVal ==(ph7_real)pObj->x.iVal && pObj->x.iVal>SMALLEST_INT64` |
|     9359 |  524 | `      && pObj->x.iVal<LARGEST_INT64 ){` |
|     9343 |  525 | `		  pObj->iFlags \|= MEMOBJ_INT;` |
|     4671 |  526 | `	}` |
|    10787 |  527 | `	return SXRET_OK;` |
|        5 |  528 | `}` |
|        - |  529 | `/*` |
|        - |  530 | ` * Convert a ph7_value to type integer.Invalidate any prior representations.` |
|        - |  531 | ` */` |
|   531746 |  532 | `PH7_PRIVATE sxi32 PH7_MemObjToInteger(ph7_value *pObj)` |
|        5 |  533 | `{` |
|   531751 |  534 | `	if( (pObj->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  535 | `		/* Preform the conversion */` |
|     1569 |  536 | `		pObj->x.iVal = MemObjIntValue(&(*pObj));` |
|        - |  537 | `		/* Invalidate any prior representations */` |
|     1569 |  538 | `		SyBlobRelease(&pObj->sBlob);` |
|     1569 |  539 | `		MemObjSetType(pObj,MEMOBJ_INT);` |
|      782 |  540 | `	}` |
|   531751 |  541 | `	return SXRET_OK;` |
|        5 |  542 | `}` |
|        - |  543 | `/*` |
|        - |  544 | ` * Convert a ph7_value to type real (Try to get an integer representation also).` |
|        - |  545 | ` * Invalidate any prior representations` |
|        - |  546 | ` */` |
|    10620 |  547 | `PH7_PRIVATE sxi32 PH7_MemObjToReal(ph7_value *pObj)` |
|        5 |  548 | `{` |
|    10625 |  549 | `	if((pObj->iFlags & MEMOBJ_REAL) == 0 ){` |
|        - |  550 | `		/* Preform the conversion */` |
|     9633 |  551 | `		pObj->rVal = MemObjRealValue(&(*pObj));` |
|        - |  552 | `		/* Invalidate any prior representations */` |
|     9633 |  553 | `		SyBlobRelease(&pObj->sBlob);` |
|     9633 |  554 | `		MemObjSetType(pObj,MEMOBJ_REAL);` |
|        - |  555 | `		/* Try to get an integer representation */` |
|     9633 |  556 | `		MemObjTryIntger(&(*pObj));` |
|     4814 |  557 | `	}` |
|    10625 |  558 | `	return SXRET_OK;` |
|        5 |  559 | `}` |
|        - |  560 | `/*` |
|        - |  561 | ` * Convert a ph7_value to type boolean.Invalidate any prior representations.` |
|        - |  562 | ` */` |
|    51130 |  563 | `PH7_PRIVATE sxi32 PH7_MemObjToBool(ph7_value *pObj)` |
|        5 |  564 | `{` |
|    51135 |  565 | `	if( (pObj->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        - |  566 | `		/* Preform the conversion */` |
|    46939 |  567 | `		pObj->x.iVal = MemObjBooleanValue(&(*pObj));` |
|        - |  568 | `		/* Invalidate any prior representations */` |
|    46939 |  569 | `		SyBlobRelease(&pObj->sBlob);` |
|    46939 |  570 | `		MemObjSetType(pObj,MEMOBJ_BOOL);` |
|    23467 |  571 | `	}` |
|    51135 |  572 | `	return SXRET_OK;` |
|        5 |  573 | `}` |
|        - |  574 | `/*` |
|        - |  575 | ` * Convert a ph7_value to type string.Prior representations are NOT invalidated.` |
|        - |  576 | ` */` |
|   961303 |  577 | `PH7_PRIVATE sxi32 PH7_MemObjToString(ph7_value *pObj)` |
|        5 |  578 | `{` |
|   961308 |  579 | `	sxi32 rc = SXRET_OK;` |
|   961308 |  580 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  581 | `		/* Perform the conversion */` |
|    61827 |  582 | `		SyBlobReset(&pObj->sBlob); /* Reset the internal buffer */` |
|    61827 |  583 | `		rc = MemObjStringValue(&pObj->sBlob,&(*pObj),TRUE);` |
|    61827 |  584 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|    30911 |  585 | `	}` |
|   961308 |  586 | `	return rc;` |
|        5 |  587 | `}` |
|        - |  588 | `/*` |
|        - |  589 | ` * Nullify a ph7_value.In other words invalidate any prior` |
|        - |  590 | ` * representation.` |
|        - |  591 | ` */` |
|      ! 0 |  592 | `PH7_PRIVATE sxi32 PH7_MemObjToNull(ph7_value *pObj)` |
|      ! 0 |  593 | `{` |
|      ! 0 |  594 | `	return PH7_MemObjRelease(pObj);` |
|      ! 0 |  595 | `}` |
|        - |  596 | `/*` |
|        - |  597 | ` * Convert a ph7_value to type array.Invalidate any prior representations.` |
|        - |  598 | `  * According to the PHP language reference manual.` |
|        - |  599 | `  *   For any of the types: integer, float, string, boolean converting a value` |
|        - |  600 | `  *   to an array results in an array with a single element with index zero` |
|        - |  601 | `  *   and the value of the scalar which was converted.` |
|        - |  602 | `  */` |
|      570 |  603 | `PH7_PRIVATE sxi32 PH7_MemObjToHashmap(ph7_value *pObj)` |
|        5 |  604 | `{` |
|      575 |  605 | `	if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  606 | `		ph7_hashmap *pMap;` |
|        - |  607 | `		/* Allocate a new hashmap instance */` |
|      377 |  608 | `		pMap = PH7_NewHashmap(pObj->pVm,0,0);` |
|      377 |  609 | `		if( pMap == 0 ){` |
|      ! 0 |  610 | `			return SXERR_MEM;` |
|        - |  611 | `		}` |
|      377 |  612 | `		if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|        - |  613 | `			/*` |
|        - |  614 | `			 * According to the PHP language reference manual.` |
|        - |  615 | `			 *   For any of the types: integer, float, string, boolean converting a value` |
|        - |  616 | `			 *   to an array results in an array with a single element with index zero` |
|        - |  617 | `			 *   and the value of the scalar which was converted.` |
|        - |  618 | `			 */` |
|       34 |  619 | `			if( pObj->iFlags & MEMOBJ_OBJ ){` |
|        - |  620 | `				/* Object cast */` |
|       22 |  621 | `				PH7_ClassInstanceToHashmap((ph7_class_instance *)pObj->x.pOther,pMap);` |
|       12 |  622 | `			}else{` |
|        - |  623 | `				/* Insert a single element */` |
|       13 |  624 | `				PH7_HashmapInsert(pMap,0/* Automatic index assign */,&(*pObj));` |
|        - |  625 | `			}` |
|       34 |  626 | `			SyBlobRelease(&pObj->sBlob);` |
|       16 |  627 | `		}` |
|        - |  628 | `		/* Invalidate any prior representation */` |
|      377 |  629 | `		PH7_MemObjRelease(pObj);` |
|      377 |  630 | `		MemObjSetType(pObj,MEMOBJ_HASHMAP);` |
|      377 |  631 | `		pObj->x.pOther = pMap;` |
|      186 |  632 | `	}` |
|      575 |  633 | `	return SXRET_OK;` |
|      290 |  634 | `}` |
|        - |  635 | `/* Per-entry callback for the array branch of the (object) cast: add one dynamic` |
|        - |  636 | ` * property to the target stdClass, named by the array key (rendered as a string,` |
|        - |  637 | ` * matching PHP) and holding a copy of the value. */` |
|        - |  638 | `struct VmObjCastData { ph7_vm *pVm; ph7_class_instance *pStd; };` |
|       62 |  639 | `static int VmArrayToObjectWalk(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        3 |  640 | `{` |
|       65 |  641 | `	struct VmObjCastData *pData = (struct VmObjCastData *)pUserData;` |
|        - |  642 | `	ph7_value *pSlot;` |
|        - |  643 | `	/* pKey and pValue are walk-owned temporaries (PH7_HashmapWalk passes pointers to` |
|        - |  644 | `	 * its own stack-local sKey/sValue, not slots inside pVm->aMemObj), so they survive` |
|        - |  645 | `	 * the slot reservation inside PH7_VmCreateDynamicAttr — no snapshot needed. pKey is` |
|        - |  646 | `	 * safe to coerce in place. */` |
|       65 |  647 | `	PH7_MemObjToString(pKey);` |
|       96 |  648 | `	pSlot = PH7_VmCreateDynamicAttr(pData->pVm,pData->pStd,` |
|       62 |  649 | `		(const char *)SyBlobData(&pKey->sBlob),(sxu32)SyBlobLength(&pKey->sBlob),0);` |
|       65 |  650 | `	if( pSlot ){` |
|       65 |  651 | `		PH7_MemObjStore(pValue,pSlot);` |
|       31 |  652 | `	}` |
|       65 |  653 | `	return SXRET_OK;` |
|        3 |  654 | `}` |
|        - |  655 | `/*` |
|        - |  656 | ` * Convert a ph7_value to type object, invalidating any prior representation.` |
|        - |  657 | ` * The new object is a (PHP-empty) stdClass populated with dynamic properties,` |
|        - |  658 | ` * matching PHP's (object) cast:` |
|        - |  659 | ` *   - array  -> one property per entry (key rendered as a string -> name).` |
|        - |  660 | ` *   - scalar -> a single property named "scalar".` |
|        - |  661 | ` *   - null   -> an empty stdClass (no properties).` |
|        - |  662 | ` *   - object -> returned unchanged (the MEMOBJ_OBJ guard below).` |
|        - |  663 | ` */` |
|       46 |  664 | `PH7_PRIVATE sxi32 PH7_MemObjToObject(ph7_value *pObj)` |
|        3 |  665 | `{` |
|       49 |  666 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  667 | `		ph7_class_instance *pStd;` |
|        - |  668 | `		ph7_class *pClass;` |
|        - |  669 | `		ph7_vm *pVm;` |
|        - |  670 | `		/* Point to the underlying VM + the stdClass */` |
|       49 |  671 | `		pVm = pObj->pVm;` |
|       72 |  672 | `		pClass = pVm->pStdClass ? pVm->pStdClass` |
|       23 |  673 | `			: PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,0,0);` |
|       49 |  674 | `		if( pClass == 0 ){` |
|        - |  675 | `			/* Can't happen,load null instead */` |
|      ! 0 |  676 | `			PH7_MemObjRelease(pObj);` |
|      ! 0 |  677 | `			return SXRET_OK;` |
|        - |  678 | `		}` |
|        - |  679 | `		/* Instanciate a new (empty) stdClass object */` |
|       49 |  680 | `		pStd = PH7_NewClassInstance(pVm,pClass);` |
|       49 |  681 | `		if( pStd == 0 ){` |
|        - |  682 | `			/* Out of memory */` |
|      ! 0 |  683 | `			PH7_MemObjRelease(pObj);` |
|      ! 0 |  684 | `			return SXRET_OK;` |
|        - |  685 | `		}` |
|       49 |  686 | `		pStd->iRef = 1;` |
|       49 |  687 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  688 | `			/* Array: one dynamic property per entry. */` |
|        - |  689 | `			struct VmObjCastData sData;` |
|       37 |  690 | `			sData.pVm = pVm;` |
|       37 |  691 | `			sData.pStd = pStd;` |
|       37 |  692 | `			ph7_array_walk(pObj,VmArrayToObjectWalk,&sData);` |
|       30 |  693 | `		}else if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  694 | `			/* Scalar (int/float/bool/string): a single "scalar" property. */` |
|       11 |  695 | `			ph7_value *pSlot = PH7_VmCreateDynamicAttr(pVm,pStd,"scalar",sizeof("scalar")-1,0);` |
|       11 |  696 | `			if( pSlot ){` |
|       11 |  697 | `				PH7_MemObjStore(pObj,pSlot);` |
|        5 |  698 | `			}` |
|        5 |  699 | `		}` |
|        - |  700 | `		/* (A NULL source yields an empty stdClass — nothing to populate.) */` |
|        - |  701 | `		/* Invalidate any prior representation */` |
|       49 |  702 | `		PH7_MemObjRelease(pObj);` |
|        - |  703 | `		/* Save the new instance */` |
|       49 |  704 | `		pObj->x.pOther = pStd;` |
|       49 |  705 | `		MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       23 |  706 | `	}` |
|       49 |  707 | `	return SXRET_OK;` |
|       26 |  708 | `}` |
|        - |  709 | `/*` |
|        - |  710 | ` * Return a pointer to the appropriate convertion method associated` |
|        - |  711 | ` * with the given type.` |
|        - |  712 | ` * Note on type juggling.` |
|        - |  713 | ` * Accoding to the PHP language reference manual` |
|        - |  714 | ` *  PHP does not require (or support) explicit type definition in variable` |
|        - |  715 | ` *  declaration; a variable's type is determined by the context in which` |
|        - |  716 | ` *  the variable is used. That is to say, if a string value is assigned` |
|        - |  717 | ` *  to variable $var, $var becomes a string. If an integer value is then` |
|        - |  718 | ` *  assigned to $var, it becomes an integer.` |
|        - |  719 | ` */` |
|       82 |  720 | `PH7_PRIVATE ProcMemObjCast PH7_MemObjCastMethod(sxi32 iFlags)` |
|        5 |  721 | `{` |
|       87 |  722 | `	if( iFlags & MEMOBJ_STRING ){` |
|       16 |  723 | `		return PH7_MemObjToString;` |
|       73 |  724 | `	}else if( iFlags & MEMOBJ_INT ){` |
|       57 |  725 | `		return PH7_MemObjToInteger;` |
|       20 |  726 | `	}else if( iFlags & MEMOBJ_REAL ){` |
|       17 |  727 | `		return PH7_MemObjToReal;` |
|        3 |  728 | `	}else if( iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  729 | `		return PH7_MemObjToBool;` |
|        3 |  730 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|        3 |  731 | `		return PH7_MemObjToHashmap;` |
|      ! 0 |  732 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  733 | `		return PH7_MemObjToObject;` |
|      ! 0 |  734 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|        - |  735 | ``		/* `null` is a type, not a weak-coercion target: never silently cast a`` |
|        - |  736 | ``		 * value to null for a standalone `null` type hint. Return/property`` |
|        - |  737 | `		 * enforcement reject a non-null value before reaching here; this guards` |
|        - |  738 | `		 * the parameter default-value path from quietly nulling a non-null` |
|        - |  739 | `		 * default. */` |
|      ! 0 |  740 | `		return 0;` |
|        - |  741 | `	}` |
|        - |  742 | `	/* NULL cast */` |
|      ! 0 |  743 | `	return PH7_MemObjToNull;` |
|       46 |  744 | `}` |
|        - |  745 | `/*` |
|        - |  746 | ` * Return TRUE only if the entire string held by pValue (optionally surrounded` |
|        - |  747 | ` * by whitespace, with an optional sign) is a well-formed PHP numeric string.` |
|        - |  748 | ` * This mirrors PHP's is_numeric_string grammar used for is_numeric() and the` |
|        - |  749 | ` * loose-comparison numeric gate:` |
|        - |  750 | ` *` |
|        - |  751 | ` *   [ws] [sign] ( D+ [.D*] \| .D+ ) [ (e\|E) [sign] D+ ] [ws]   (whole string)` |
|        - |  752 | ` *` |
|        - |  753 | ` * Implemented directly rather than via SyStrIsNumeric — which returns OK on any` |
|        - |  754 | ` * numeric PREFIX (so it wrongly accepts "10abc"/"0x1A"/"0b101") and requires a` |
|        - |  755 | ` * leading digit (so it wrongly rejects ".5"/"-.5", valid in PHP). Unlike a` |
|        - |  756 | ` * strtod-based classifier this needs no NUL-terminated buffer. Returns FALSE for` |
|        - |  757 | ` * a non-string value.` |
|        - |  758 | ` */` |
|        - |  759 | `/*` |
|        - |  760 | ` * Scan php's numeric-string grammar and report the longest numeric PREFIX.` |
|        - |  761 | ` * Returns 1 when the string starts with a number, 0 when nothing numeric is` |
|        - |  762 | ` * there at all ("abc", "", ".", "e5"). On success *pzTail points just past the` |
|        - |  763 | ` * prefix, so the caller can tell a fully numeric string ("1e3", " 5 ") from a` |
|        - |  764 | ` * merely leading-numeric one ("5abc", "1e", "0x1A") -- php warns on the latter` |
|        - |  765 | ` * and rejects a string with no prefix outright.` |
|        - |  766 | ` */` |
|   249745 |  767 | `PH7_PRIVATE int PH7_MemObjStringNumericPrefix(ph7_value *pValue,const char **pzTail)` |
|        5 |  768 | `{` |
|        - |  769 | `	const char *z, *zEnd;` |
|        - |  770 | `	sxu32 n;` |
|   249750 |  771 | `	int bDigit = 0;` |
|   249750 |  772 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  773 | `		return 0;` |
|        - |  774 | `	}` |
|   249750 |  775 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|   249750 |  776 | `	n = SyBlobLength(&pValue->sBlob);` |
|   249750 |  777 | `	if( n == 0 ){` |
|      603 |  778 | `		return 0;` |
|        - |  779 | `	}` |
|   249150 |  780 | `	zEnd = z + n;` |
|   249174 |  781 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){` |
|       26 |  782 | `		z++;` |
|        2 |  783 | `	}` |
|   249150 |  784 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|      215 |  785 | `		z++;` |
|      105 |  786 | `	}` |
|   254005 |  787 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|     4859 |  788 | `		z++; bDigit = 1;` |
|        4 |  789 | `	}` |
|   249150 |  790 | `	if( z < zEnd && z[0] == '.' ){` |
|     6030 |  791 | `		z++;` |
|     6104 |  792 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|       77 |  793 | `			z++; bDigit = 1;` |
|        3 |  794 | `		}` |
|     3217 |  795 | `	}` |
|        - |  796 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|   249150 |  797 | `	if( !bDigit ){` |
|   244403 |  798 | `		return 0;` |
|        - |  799 | `	}` |
|        - |  800 | `	/* Optional exponent — only joins the prefix if it carries a digit. "1e" has` |
|        - |  801 | `	 * the numeric prefix "1" with the 'e' left in the tail, exactly as php reads it. */` |
|     4751 |  802 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|       30 |  803 | `		const char *zExp = z;` |
|       30 |  804 | `		z++;` |
|       30 |  805 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|      ! 0 |  806 | `			z++;` |
|      ! 0 |  807 | `		}` |
|       30 |  808 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|       10 |  809 | `			z = zExp;` |
|        6 |  810 | `		}else{` |
|       46 |  811 | `			while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|       26 |  812 | `				z++;` |
|        2 |  813 | `			}` |
|        - |  814 | `		}` |
|       14 |  815 | `	}` |
|     4751 |  816 | `	if( pzTail ){` |
|     4751 |  817 | `		*pzTail = z;` |
|     2373 |  818 | `	}` |
|     4751 |  819 | `	return 1;` |
|   124762 |  820 | `}` |
|        - |  821 | `/*` |
|        - |  822 | ` * TRUE only if the WHOLE string is a well-formed php numeric string` |
|        - |  823 | ` * (trailing whitespace allowed, nothing else).` |
|        - |  824 | ` */` |
|   247336 |  825 | `PH7_PRIVATE int PH7_MemObjStringIsNumeric(ph7_value *pValue)` |
|        5 |  826 | `{` |
|   247341 |  827 | `	const char *zTail = 0, *zEnd;` |
|   247341 |  828 | `	if( !PH7_MemObjStringNumericPrefix(pValue,&zTail) ){` |
|   244993 |  829 | `		return 0;` |
|        - |  830 | `	}` |
|     2352 |  831 | `	zEnd = (const char *)SyBlobData(&pValue->sBlob) + SyBlobLength(&pValue->sBlob);` |
|     2358 |  832 | `	while( zTail < zEnd && (unsigned char)zTail[0] < 0xc0 && SyisSpace(zTail[0]) ){` |
|        8 |  833 | `		zTail++;` |
|        2 |  834 | `	}` |
|     2352 |  835 | `	return zTail == zEnd ? 1 : 0;` |
|   123558 |  836 | `}` |
|        - |  837 | `/*` |
|        - |  838 | ` * Check whether the ph7_value is numeric [i.e: int/float/bool] or looks` |
|        - |  839 | ` * like a numeric number [i.e: if the ph7_value is of type string.].` |
|        - |  840 | ` * Return TRUE if numeric.FALSE otherwise.` |
|        - |  841 | ` */` |
|   248324 |  842 | `PH7_PRIVATE sxi32 PH7_MemObjIsNumeric(ph7_value *pObj)` |
|        5 |  843 | `{` |
|   248329 |  844 | `	if( pObj->iFlags & ( MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      679 |  845 | `		return TRUE;` |
|   247655 |  846 | `	}else if( pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|      436 |  847 | `		return FALSE;` |
|   247223 |  848 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        - |  849 | `		/* TRUE only if the whole string is a well-formed PHP numeric string. */` |
|   247223 |  850 | `		return PH7_MemObjStringIsNumeric(pObj) ? TRUE : FALSE;` |
|        - |  851 | `	}` |
|        - |  852 | `	/* NOT REACHED */` |
|      ! 0 |  853 | `	return FALSE;` |
|   124052 |  854 | `}` |
|        - |  855 | `/*` |
|        - |  856 | ` * Check whether the ph7_value is empty.Return TRUE if empty.` |
|        - |  857 | ` * FALSE otherwise.` |
|        - |  858 | ` * An ph7_value is considered empty if the following are true:` |
|        - |  859 | ` * NULL value.` |
|        - |  860 | ` * Boolean FALSE.` |
|        - |  861 | ` * Integer/Float with a 0 (zero) value.` |
|        - |  862 | ` * An empty string or a stream of 0 (zero) [i.e: "0","00","000",...].` |
|        - |  863 | ` * An empty array.` |
|        - |  864 | ` * NOTE` |
|        - |  865 | ` *  OBJECT VALUE MUST NOT BE MODIFIED.` |
|        - |  866 | ` */` |
|    39594 |  867 | `PH7_PRIVATE sxi32 PH7_MemObjIsEmpty(ph7_value *pObj)` |
|        5 |  868 | `{` |
|    39599 |  869 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|       21 |  870 | `		return TRUE;` |
|    39581 |  871 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|       22 |  872 | `		return pObj->x.iVal == 0 ? TRUE : FALSE;` |
|    39561 |  873 | `	}else if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  874 | `		return pObj->rVal == (ph7_real)0 ? TRUE : FALSE;` |
|    39561 |  875 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|        5 |  876 | `		return !pObj->x.iVal;` |
|    39557 |  877 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|    26439 |  878 | `		if( SyBlobLength(&pObj->sBlob) <= 0 ){` |
|    21057 |  879 | `			return TRUE;` |
|      ! 0 |  880 | `		}else{` |
|        - |  881 | `			const char *zIn,*zEnd;` |
|     5387 |  882 | `			zIn = (const char *)SyBlobData(&pObj->sBlob);` |
|     5387 |  883 | `			zEnd = &zIn[SyBlobLength(&pObj->sBlob)];` |
|     5393 |  884 | `			while( zIn < zEnd ){` |
|     5393 |  885 | `				if( zIn[0] != '0' ){` |
|     5387 |  886 | `					break;` |
|        - |  887 | `				}` |
|        7 |  888 | `				zIn++;` |
|        1 |  889 | `			}` |
|     5387 |  890 | `			return zIn >= zEnd ? TRUE : FALSE;` |
|      ! 0 |  891 | `		}` |
|    13123 |  892 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|    13123 |  893 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|    13123 |  894 | `		return pMap->nEntry == 0 ? TRUE : FALSE;` |
|      ! 0 |  895 | `	}else if ( pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|      ! 0 |  896 | `		return FALSE;` |
|        - |  897 | `	}` |
|        - |  898 | `	/* Assume empty by default */` |
|      ! 0 |  899 | `	return TRUE;` |
|    19802 |  900 | `}` |
|        - |  901 | `/*` |
|        - |  902 | ` * Convert a ph7_value so that it has types MEMOBJ_REAL or MEMOBJ_INT` |
|        - |  903 | ` * or both.` |
|        - |  904 | ` * Invalidate any prior representations. Every effort is made to force` |
|        - |  905 | ` * the conversion, even if the input is a string that does not look` |
|        - |  906 | ` * completely like a number.Convert as much of the string as we can` |
|        - |  907 | ` * and ignore the rest.` |
|        - |  908 | ` */` |
|   519216 |  909 | `PH7_PRIVATE sxi32 PH7_MemObjToNumeric(ph7_value *pObj)` |
|        5 |  910 | `{` |
|   519221 |  911 | `	if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|   516860 |  912 | `		if( pObj->iFlags & (MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|        8 |  913 | `			if( pObj->iFlags & MEMOBJ_NULL ){` |
|        5 |  914 | `				pObj->x.iVal = 0;` |
|        2 |  915 | `			}` |
|        8 |  916 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|        3 |  917 | `		}` |
|        - |  918 | `		/* Already numeric */` |
|   516860 |  919 | `		return  SXRET_OK;` |
|        - |  920 | `	}` |
|     2365 |  921 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|     2365 |  922 | `		const char *zTail = 0;` |
|     2365 |  923 | `		int bNum, bReal = 0;` |
|        - |  924 | `		/* php reads the longest numeric PREFIX and its shape decides the type: a` |
|        - |  925 | `		 * '.' or a *complete* exponent inside that prefix makes it a float, else an` |
|        - |  926 | `		 * int. Deciding from the raw string instead mistyped "1e" as float(1) --` |
|        - |  927 | `		 * php sees the prefix "1" there and yields int(1). */` |
|     2365 |  928 | `		bNum = PH7_MemObjStringNumericPrefix(pObj,&zTail);` |
|     2365 |  929 | `		if( bNum ){` |
|     2365 |  930 | `			const char *z = (const char *)SyBlobData(&pObj->sBlob);` |
|     4812 |  931 | `			while( z < zTail ){` |
|     2471 |  932 | `				if( z[0] == '.' \|\| z[0] == 'e' \|\| z[0] == 'E' ){` |
|       22 |  933 | `					bReal = 1;` |
|       22 |  934 | `					break;` |
|        - |  935 | `				}` |
|     2451 |  936 | `				z++;` |
|        4 |  937 | `			}` |
|     1180 |  938 | `		}` |
|     2365 |  939 | `		if( bReal ){` |
|       22 |  940 | `			PH7_MemObjToReal(&(*pObj));` |
|       12 |  941 | `		}else{` |
|     2345 |  942 | `			if( !bNum ){` |
|        - |  943 | `				/* The input does not look at all like a number,set the value to 0 */` |
|      ! 0 |  944 | `				pObj->x.iVal = 0;` |
|      ! 0 |  945 | `			}else{` |
|        - |  946 | `				/* Convert as much as we can */` |
|     2345 |  947 | `				pObj->x.iVal = MemObjStringToInt(&(*pObj));` |
|        - |  948 | `			}` |
|     2345 |  949 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|     2345 |  950 | `			SyBlobRelease(&pObj->sBlob);` |
|        4 |  951 | `		}` |
|     1180 |  952 | `	}else if(pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)){` |
|      ! 0 |  953 | `		PH7_MemObjToInteger(pObj);` |
|      ! 0 |  954 | `	}else{` |
|        - |  955 | `		/* Perform a blind cast */` |
|      ! 0 |  956 | `		PH7_MemObjToReal(&(*pObj));` |
|        - |  957 | `	}` |
|     2365 |  958 | `	return SXRET_OK;` |
|   259828 |  959 | `}` |
|        - |  960 | `/*` |
|        - |  961 | ` * Apply Perl-style increment to a string ph7_value in place.` |
|        - |  962 | ` * Walks the bytes right-to-left: digits 0-8 / letters a-y, A-Y bump in` |
|        - |  963 | ` * place; '9' wraps to '0' with carry; 'z' to 'a'; 'Z' to 'A'. A non-` |
|        - |  964 | ` * alphanumeric byte stops the walk without prepending. If carry survives` |
|        - |  965 | ` * past index 0, prepend '1', 'a', or 'A' depending on the class of the` |
|        - |  966 | ` * last carried character. Empty strings become "1".` |
|        - |  967 | ` *` |
|        - |  968 | ` * Caller must ensure pObj is MEMOBJ_STRING and not a numeric string;` |
|        - |  969 | ` * this routine never reclassifies the type, so a result like "e0" stays` |
|        - |  970 | ` * a string even though it looks numeric.` |
|        - |  971 | ` */` |
|      ! 0 |  972 | `PH7_PRIVATE sxi32 PH7_MemObjStringIncrement(ph7_value *pObj)` |
|      ! 0 |  973 | `{` |
|        - |  974 | `	enum CarryClass { CARRY_NONE = 0, CARRY_LOWER, CARRY_UPPER, CARRY_DIGIT };` |
|      ! 0 |  975 | `	enum CarryClass last_class = CARRY_NONE;` |
|        - |  976 | `	sxu32 nLen, pos;` |
|        - |  977 | `	sxu8 *zStr;` |
|      ! 0 |  978 | `	int carry = 1;` |
|        - |  979 | `	int ch;` |
|        - |  980 | `	/* Force ownership: the blob may be SXBLOB_RDONLY (e.g., from` |
|        - |  981 | `	 * PH7_MemObjLoad), in which case BlobPrepareGrow copies on demand` |
|        - |  982 | `	 * and clears the flag.  On an already-owned blob with spare capacity` |
|        - |  983 | `	 * (the common case under PHL's growth allocator), this is a no-op` |
|        - |  984 | `	 * append; on an exact-fit owned blob it triggers a single realloc. */` |
|      ! 0 |  985 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      ! 0 |  986 | `		SyBlobNullAppend(&pObj->sBlob);` |
|      ! 0 |  987 | `	}` |
|      ! 0 |  988 | `	nLen = SyBlobLength(&pObj->sBlob);` |
|      ! 0 |  989 | `	if( nLen == 0 ){` |
|      ! 0 |  990 | `		SyBlobAppend(&pObj->sBlob,"1",sizeof(char));` |
|      ! 0 |  991 | `		return SXRET_OK;` |
|        - |  992 | `	}` |
|      ! 0 |  993 | `	zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|      ! 0 |  994 | `	pos = nLen;` |
|      ! 0 |  995 | `	while( pos > 0 ){` |
|      ! 0 |  996 | `		pos--;` |
|      ! 0 |  997 | `		ch = zStr[pos];` |
|      ! 0 |  998 | `		if( ch >= 'a' && ch <= 'z' ){` |
|      ! 0 |  999 | `			if( ch == 'z' ){` |
|      ! 0 | 1000 | `				zStr[pos] = 'a';` |
|      ! 0 | 1001 | `				last_class = CARRY_LOWER;` |
|      ! 0 | 1002 | `				continue;` |
|        - | 1003 | `			}` |
|      ! 0 | 1004 | `			zStr[pos]++;` |
|      ! 0 | 1005 | `			carry = 0;` |
|      ! 0 | 1006 | `			break;` |
|      ! 0 | 1007 | `		}else if( ch >= 'A' && ch <= 'Z' ){` |
|      ! 0 | 1008 | `			if( ch == 'Z' ){` |
|      ! 0 | 1009 | `				zStr[pos] = 'A';` |
|      ! 0 | 1010 | `				last_class = CARRY_UPPER;` |
|      ! 0 | 1011 | `				continue;` |
|        - | 1012 | `			}` |
|      ! 0 | 1013 | `			zStr[pos]++;` |
|      ! 0 | 1014 | `			carry = 0;` |
|      ! 0 | 1015 | `			break;` |
|      ! 0 | 1016 | `		}else if( ch >= '0' && ch <= '9' ){` |
|      ! 0 | 1017 | `			if( ch == '9' ){` |
|      ! 0 | 1018 | `				zStr[pos] = '0';` |
|      ! 0 | 1019 | `				last_class = CARRY_DIGIT;` |
|      ! 0 | 1020 | `				continue;` |
|        - | 1021 | `			}` |
|      ! 0 | 1022 | `			zStr[pos]++;` |
|      ! 0 | 1023 | `			carry = 0;` |
|      ! 0 | 1024 | `			break;` |
|      ! 0 | 1025 | `		}else{` |
|        - | 1026 | `			/* non-alphanumeric: stop without prepending */` |
|      ! 0 | 1027 | `			carry = 0;` |
|      ! 0 | 1028 | `			break;` |
|        - | 1029 | `		}` |
|      ! 0 | 1030 | `	}` |
|      ! 0 | 1031 | `	if( carry ){` |
|        - | 1032 | `		sxu8 prepend;` |
|        - | 1033 | `		sxu32 i;` |
|      ! 0 | 1034 | `		switch( last_class ){` |
|      ! 0 | 1035 | `			case CARRY_LOWER: prepend = (sxu8)'a'; break;` |
|      ! 0 | 1036 | `			case CARRY_UPPER: prepend = (sxu8)'A'; break;` |
|      ! 0 | 1037 | `			default:          prepend = (sxu8)'1'; break;` |
|        - | 1038 | `		}` |
|        - | 1039 | `		/* Append a sentinel byte to grow nByte by 1 (capacity grows too). */` |
|      ! 0 | 1040 | `		SyBlobAppend(&pObj->sBlob,"\0",sizeof(char));` |
|      ! 0 | 1041 | `		zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|      ! 0 | 1042 | `		nLen = SyBlobLength(&pObj->sBlob);` |
|        - | 1043 | `		/* Shift right by 1, walking from the end so overlapping is safe. */` |
|      ! 0 | 1044 | `		for( i = nLen - 1; i > 0; i-- ){` |
|      ! 0 | 1045 | `			zStr[i] = zStr[i - 1];` |
|      ! 0 | 1046 | `		}` |
|      ! 0 | 1047 | `		zStr[0] = prepend;` |
|      ! 0 | 1048 | `	}` |
|      ! 0 | 1049 | `	return SXRET_OK;` |
|      ! 0 | 1050 | `}` |
|        - | 1051 | `/*` |
|        - | 1052 | ` * Try a get an integer representation of the given ph7_value.` |
|        - | 1053 | ` * If the ph7_value is not of type real,this function is a no-op.` |
|        - | 1054 | ` */` |
|     1086 | 1055 | `PH7_PRIVATE sxi32 PH7_MemObjTryInteger(ph7_value *pObj)` |
|        4 | 1056 | `{` |
|     1090 | 1057 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|        - | 1058 | `		/* Work only with reals */` |
|     1090 | 1059 | `		MemObjTryIntger(&(*pObj));` |
|      543 | 1060 | `	}` |
|     1090 | 1061 | `	return SXRET_OK;` |
|        4 | 1062 | `}` |
|        - | 1063 | `/*` |
|        - | 1064 | ` * Initialize a ph7_value to the null type.` |
|        - | 1065 | ` */` |
| 29311808 | 1066 | `PH7_PRIVATE sxi32 PH7_MemObjInit(ph7_vm *pVm,ph7_value *pObj)` |
|        5 | 1067 | `{` |
|        - | 1068 | `	/* Zero the structure */` |
| 29311813 | 1069 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1070 | `	/* Initialize fields */` |
| 29311813 | 1071 | `	pObj->pVm = pVm;` |
| 29311813 | 1072 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1073 | `	/* Set the NULL type */` |
| 29311813 | 1074 | `	pObj->iFlags = MEMOBJ_NULL;` |
| 29311813 | 1075 | `	return SXRET_OK;` |
|        5 | 1076 | `}` |
|        - | 1077 | `/*` |
|        - | 1078 | ` * Initialize a ph7_value to the integer type.` |
|        - | 1079 | ` */` |
|  5983472 | 1080 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromInt(ph7_vm *pVm,ph7_value *pObj,sxi64 iVal)` |
|        5 | 1081 | `{` |
|        - | 1082 | `	/* Zero the structure */` |
|  5983477 | 1083 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1084 | `	/* Initialize fields */` |
|  5983477 | 1085 | `	pObj->pVm = pVm;` |
|  5983477 | 1086 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1087 | `	/* Set the desired type */` |
|  5983477 | 1088 | `	pObj->x.iVal = iVal;` |
|  5983477 | 1089 | `	pObj->iFlags = MEMOBJ_INT;` |
|  5983477 | 1090 | `	return SXRET_OK;` |
|        5 | 1091 | `}` |
|        - | 1092 | `/*` |
|        - | 1093 | ` * Initialize a ph7_value to the boolean type.` |
|        - | 1094 | ` */` |
|    16848 | 1095 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromBool(ph7_vm *pVm,ph7_value *pObj,sxi32 iVal)` |
|        5 | 1096 | `{` |
|        - | 1097 | `	/* Zero the structure */` |
|    16853 | 1098 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1099 | `	/* Initialize fields */` |
|    16853 | 1100 | `	pObj->pVm = pVm;` |
|    16853 | 1101 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1102 | `	/* Set the desired type */` |
|    16853 | 1103 | `	pObj->x.iVal = iVal ? 1 : 0;` |
|    16853 | 1104 | `	pObj->iFlags = MEMOBJ_BOOL;` |
|    16853 | 1105 | `	return SXRET_OK;` |
|        5 | 1106 | `}` |
|        - | 1107 | `/*` |
|        - | 1108 | ` * Initialize a ph7_value to the real type.` |
|        - | 1109 | ` */` |
|       10 | 1110 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromReal(ph7_vm *pVm,ph7_value *pObj,ph7_real rVal)` |
|        1 | 1111 | `{` |
|        - | 1112 | `	/* Zero the structure */` |
|       11 | 1113 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1114 | `	/* Initialize fields */` |
|       11 | 1115 | `	pObj->pVm = pVm;` |
|       11 | 1116 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1117 | `	/* Set the desired type */` |
|       11 | 1118 | `	pObj->rVal = rVal;` |
|       11 | 1119 | `	pObj->iFlags = MEMOBJ_REAL;` |
|       11 | 1120 | `	return SXRET_OK;` |
|        1 | 1121 | `}` |
|        - | 1122 | `/*` |
|        - | 1123 | ` * Initialize a ph7_value to the array type.` |
|        - | 1124 | ` */` |
|    63880 | 1125 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromArray(ph7_vm *pVm,ph7_value *pObj,ph7_hashmap *pArray)` |
|        5 | 1126 | `{` |
|        - | 1127 | `	/* Zero the structure */` |
|    63885 | 1128 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1129 | `	/* Initialize fields */` |
|    63885 | 1130 | `	pObj->pVm = pVm;` |
|    63885 | 1131 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1132 | `	/* Set the desired type */` |
|    63885 | 1133 | `	pObj->iFlags = MEMOBJ_HASHMAP;` |
|    63885 | 1134 | `	pObj->x.pOther = pArray;` |
|    63885 | 1135 | `	return SXRET_OK;` |
|        5 | 1136 | `}` |
|        - | 1137 | `/*` |
|        - | 1138 | ` * Initialize a ph7_value to the string type.` |
|        - | 1139 | ` */` |
|  4927130 | 1140 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromString(ph7_vm *pVm,ph7_value *pObj,const SyString *pVal)` |
|        5 | 1141 | `{` |
|        - | 1142 | `	/* Zero the structure */` |
|  4927135 | 1143 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1144 | `	/* Initialize fields */` |
|  4927135 | 1145 | `	pObj->pVm = pVm;` |
|  4927135 | 1146 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|  4927135 | 1147 | `	if( pVal ){` |
|        - | 1148 | `		/* Append contents */` |
|  2488231 | 1149 | `		SyBlobAppend(&pObj->sBlob,(const void *)pVal->zString,pVal->nByte);` |
|  1244113 | 1150 | `	}` |
|        - | 1151 | `	/* Set the desired type */` |
|  4927135 | 1152 | `	pObj->iFlags = MEMOBJ_STRING;` |
|  4927135 | 1153 | `	return SXRET_OK;` |
|        5 | 1154 | `}` |
|        - | 1155 | `/*` |
|        - | 1156 | ` * Append some contents to the internal buffer of a given ph7_value.` |
|        - | 1157 | ` * If the given ph7_value is not of type string,this function` |
|        - | 1158 | ` * invalidate any prior representation and set the string type.` |
|        - | 1159 | ` * Then a simple append operation is performed.` |
|        - | 1160 | ` */` |
|  2860400 | 1161 | `PH7_PRIVATE sxi32 PH7_MemObjStringAppend(ph7_value *pObj,const char *zData,sxu32 nLen)` |
|        5 | 1162 | `{` |
|        - | 1163 | `	sxi32 rc;` |
|  2860405 | 1164 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 1165 | `		/* Invalidate any prior representation */` |
|     3755 | 1166 | `		PH7_MemObjRelease(pObj);` |
|     3755 | 1167 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|     1875 | 1168 | `	}` |
|        - | 1169 | `	/* Append contents */` |
|  2860405 | 1170 | `	rc = SyBlobAppend(&pObj->sBlob,zData,nLen);` |
|  2860405 | 1171 | `	return rc;` |
|        5 | 1172 | `}` |
|        - | 1173 | `#if 0` |
|        - | 1174 | `/*` |
|        - | 1175 | ` * Format and append some contents to the internal buffer of a given ph7_value.` |
|        - | 1176 | ` * If the given ph7_value is not of type string,this function invalidate` |
|        - | 1177 | ` * any prior representation and set the string type.` |
|        - | 1178 | ` * Then a simple format and append operation is performed.` |
|        - | 1179 | ` */` |
|        - | 1180 | `PH7_PRIVATE sxi32 PH7_MemObjStringFormat(ph7_value *pObj,const char *zFormat,va_list ap)` |
|        - | 1181 | `{` |
|        - | 1182 | `	sxi32 rc;` |
|        - | 1183 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 1184 | `		/* Invalidate any prior representation */` |
|        - | 1185 | `		PH7_MemObjRelease(pObj);` |
|        - | 1186 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|        - | 1187 | `	}` |
|        - | 1188 | `	/* Format and append contents */` |
|        - | 1189 | `	rc = SyBlobFormatAp(&pObj->sBlob,zFormat,ap);` |
|        - | 1190 | `	return rc;` |
|        - | 1191 | `}` |
|        - | 1192 | `#endif` |
|        - | 1193 | `/*` |
|        - | 1194 | ` * Duplicate the contents of a ph7_value.` |
|        - | 1195 | ` */` |
|  6016630 | 1196 | `PH7_PRIVATE sxi32 PH7_MemObjStore(ph7_value *pSrc,ph7_value *pDest)` |
|        5 | 1197 | `{` |
|  6016635 | 1198 | `	ph7_class_instance *pObj = 0;` |
|  6016635 | 1199 | `	ph7_hashmap *pMap = 0;` |
|        - | 1200 | `	sxi32 rc;` |
|  6016635 | 1201 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1202 | `		/* Increment reference count */` |
|   218347 | 1203 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  5907464 | 1204 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|        - | 1205 | `		/* Increment reference count */` |
|    12309 | 1206 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|     6152 | 1207 | `	}` |
|  6016635 | 1208 | `	if( pDest->iFlags & MEMOBJ_HASHMAP ){` |
|    80861 | 1209 | `		pMap = (ph7_hashmap *)pDest->x.pOther;` |
|  5976207 | 1210 | `	}else if( pDest->iFlags & MEMOBJ_OBJ ){` |
|     8775 | 1211 | `		pObj = (ph7_class_instance *)pDest->x.pOther;` |
|     4385 | 1212 | `	}` |
|  6016635 | 1213 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|  6016635 | 1214 | `	pDest->iFlags &= ~MEMOBJ_AUX;` |
|  6016635 | 1215 | `	rc = SXRET_OK;` |
|  6016635 | 1216 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  4254085 | 1217 | `		SyBlobReset(&pDest->sBlob);` |
|  4254085 | 1218 | `		rc = SyBlobDup(&pSrc->sBlob,&pDest->sBlob);` |
|  2127354 | 1219 | `	}else{` |
|  1762555 | 1220 | `		if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|   319968 | 1221 | `			SyBlobRelease(&pDest->sBlob);` |
|   160244 | 1222 | `		}` |
|        - | 1223 | `	}` |
|  6016635 | 1224 | `	if( pMap ){` |
|    80861 | 1225 | `		PH7_HashmapUnref(pMap);` |
|  5976207 | 1226 | `	}else if( pObj ){` |
|     8775 | 1227 | `		PH7_ClassInstanceUnref(pObj);` |
|     4385 | 1228 | `	}` |
|  6016630 | 1229 | `	if( rc == SXRET_OK && (pDest->iFlags & MEMOBJ_HASHMAP)` |
|  3118572 | 1230 | `	 && pDest->pVm` |
|   218342 | 1231 | `	 && (ph7_hashmap *)pDest->x.pOther == pDest->pVm->pGlobal` |
|        - | 1232 | `	 /* Identity, not nIdx: transient values carry nIdx==0 (SyZero), which` |
|        - | 1233 | `	  * collides with a typical nGlobalIdx of 0 and would skip the snapshot` |
|        - | 1234 | `	  * for closure envs and other non-slot destinations. */` |
|   109180 | 1235 | `	 && pDest != (ph7_value *)SySetAt(&pDest->pVm->aMemObj,pDest->pVm->nGlobalIdx) ){` |
|        - | 1236 | `		/* php 8.1: a COPY of $GLOBALS ($snap = $GLOBALS, $a[] = $GLOBALS,` |
|        - | 1237 | `		 * by-value argument passing, return $GLOBALS, ...) is a by-value` |
|        - | 1238 | `		 * SNAPSHOT of the symbol table with its reference entries` |
|        - | 1239 | `		 * flattened — never a live alias. Materialize it here, the one` |
|        - | 1240 | `		 * store choke point (loads/subscript access keep sharing, so` |
|        - | 1241 | `		 * $GLOBALS[$k] reads and writes stay live). */` |
|        9 | 1242 | `		ph7_hashmap *pSnap = PH7_NewHashmap(pDest->pVm,0,0);` |
|        9 | 1243 | `		if( pSnap && PH7_HashmapDupMaterialized((ph7_hashmap *)pDest->x.pOther,pSnap) == SXRET_OK ){` |
|        9 | 1244 | `			PH7_HashmapUnref((ph7_hashmap *)pDest->x.pOther);` |
|        9 | 1245 | `			pDest->x.pOther = pSnap;` |
|        4 | 1246 | `		}else if( pSnap ){` |
|      ! 0 | 1247 | `			PH7_HashmapUnref(pSnap);` |
|      ! 0 | 1248 | `		}` |
|        4 | 1249 | `	}` |
|  6016635 | 1250 | `	return rc;` |
|        5 | 1251 | `}` |
|        - | 1252 | `/*` |
|        - | 1253 | ` * Duplicate the contents of a ph7_value but do not copy internal` |
|        - | 1254 | ` * buffer contents,simply point to it.` |
|        - | 1255 | ` */` |
|  8267616 | 1256 | `PH7_PRIVATE sxi32 PH7_MemObjLoad(ph7_value *pSrc,ph7_value *pDest)` |
|        5 | 1257 | `{` |
|  8267621 | 1258 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),` |
|        - | 1259 | `		sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|  8267621 | 1260 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1261 | `		/* Increment reference count */` |
|   539777 | 1262 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  7997735 | 1263 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|        - | 1264 | `		/* Increment reference count */` |
|    56205 | 1265 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|    28100 | 1266 | `	}` |
|  8267621 | 1267 | `	if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|       46 | 1268 | `		SyBlobRelease(&pDest->sBlob);` |
|       21 | 1269 | `	}` |
|  8267621 | 1270 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  4360917 | 1271 | `		SyBlobReadOnly(&pDest->sBlob,SyBlobData(&pSrc->sBlob),SyBlobLength(&pSrc->sBlob));` |
|  2181575 | 1272 | `	}` |
|  8267621 | 1273 | `	return SXRET_OK;` |
|        5 | 1274 | `}` |
|        - | 1275 | `/*` |
|        - | 1276 | ` * Invalidate any prior representation of a given ph7_value.` |
|        - | 1277 | ` */` |
| 20952361 | 1278 | `PH7_PRIVATE sxi32 PH7_MemObjRelease(ph7_value *pObj)` |
|        5 | 1279 | `{` |
| 20952366 | 1280 | `	if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
| 17600898 | 1281 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|   705347 | 1282 | `			PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
| 17248227 | 1283 | `		}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|   105067 | 1284 | `			PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|    52531 | 1285 | `		}` |
|        - | 1286 | `		/* Release the internal buffer */` |
| 17600898 | 1287 | `		SyBlobRelease(&pObj->sBlob);` |
|        - | 1288 | `		/* Invalidate any prior representation */` |
| 17600898 | 1289 | `		pObj->iFlags = MEMOBJ_NULL;` |
|  8804719 | 1290 | `	}` |
| 20952366 | 1291 | `	return SXRET_OK;` |
|        5 | 1292 | `}` |
|        - | 1293 | `/*` |
|        - | 1294 | ` * Compare two ph7_values.` |
|        - | 1295 | ` * Return 0 if the values are equals, > 0 if pObj1 is greater than pObj2` |
|        - | 1296 | ` * or < 0 if pObj2 is greater than pObj1.` |
|        - | 1297 | ` * Type comparison table taken from the PHP language reference manual.` |
|        - | 1298 | ` * Comparisons of $x with PHP functions Expression` |
|        - | 1299 | ` *              gettype() 	empty() 	is_null() 	isset() 	boolean : if($x)` |
|        - | 1300 | ` * $x = ""; 	string 	    TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1301 | ` * $x = null 	NULL 	    TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1302 | ` * var $x; 	    NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1303 | ` * $x is undefined 	NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1304 | ` *  $x = array(); 	array 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1305 | ` * $x = false; 	boolean 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1306 | ` * $x = true; 	boolean 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1307 | ` * $x = 1; 	    integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1308 | ` * $x = 42; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1309 | ` * $x = 0; 	    integer 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1310 | ` * $x = -1; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1311 | ` * $x = "1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1312 | ` * $x = "0"; 	string 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1313 | ` * $x = "-1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1314 | ` * $x = "php"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1315 | ` * $x = "true"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1316 | ` * $x = "false"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1317 | ` *      Loose comparisons with ==` |
|        - | 1318 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|        - | 1319 | ` * TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1320 | ` * FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|        - | 1321 | ` * 1 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1322 | ` * 0 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE` |
|        - | 1323 | ` * -1 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1324 | ` * "1" 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1325 | ` * "0" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1326 | ` * "-1" 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1327 | ` * NULL 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|        - | 1328 | ` * array() 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1329 | ` * "php" 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1330 | ` * "" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE` |
|        - | 1331 | ` *    Strict comparisons with ===` |
|        - | 1332 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|        - | 1333 | ` * TRUE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1334 | ` * FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1335 | ` * 1 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1336 | ` * 0 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1337 | ` * -1 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1338 | ` * "1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1339 | ` * "0" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1340 | ` * "-1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1341 | ` * NULL 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE` |
|        - | 1342 | ` * array() 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE` |
|        - | 1343 | ` * "php" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1344 | ` * "" 	    FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE` |
|        - | 1345 | ` */` |
|  1556375 | 1346 | `PH7_PRIVATE sxi32 PH7_MemObjCmp(ph7_value *pObj1,ph7_value *pObj2,int bStrict,int iNest)` |
|        5 | 1347 | `{` |
|        - | 1348 | `	sxi32 iComb;` |
|        - | 1349 | `	sxi32 rc;` |
|  1556380 | 1350 | `	if( bStrict ){` |
|        - | 1351 | `		sxi32 iF1,iF2;` |
|        - | 1352 | `		/* Strict comparisons with === */` |
|   843180 | 1353 | `		iF1 = pObj1->iFlags&~MEMOBJ_AUX;` |
|   843180 | 1354 | `		iF2 = pObj2->iFlags&~MEMOBJ_AUX;` |
|   843180 | 1355 | `		if( iF1 != iF2 ){` |
|        - | 1356 | `			/* Not of the same type */` |
|   196105 | 1357 | `			return 1;` |
|        - | 1358 | `		}` |
|   323743 | 1359 | `	}` |
|        - | 1360 | `	/* Combine flag together */` |
|  1360280 | 1361 | `	iComb = pObj1->iFlags\|pObj2->iFlags;` |
|  1360275 | 1362 | `	if( !bStrict` |
|  1036943 | 1363 | `	 && (iComb & MEMOBJ_NULL) != 0` |
|   356832 | 1364 | `	 && (iComb & MEMOBJ_STRING) != 0` |
|       66 | 1365 | `	 && (iComb & (MEMOBJ_BOOL\|MEMOBJ_RES\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|        - | 1366 | `		/*` |
|        - | 1367 | `		 * PHP 8 comparison table: null loosely compared with a STRING is` |
|        - | 1368 | `		 * compared as the empty string (a string comparison), not through` |
|        - | 1369 | `		 * bool coercion — so null == "0" is FALSE and null < "0" is TRUE` |
|        - | 1370 | `		 * (php 7 and the historical PH7 behavior coerced both to bool,` |
|        - | 1371 | `		 * making any non-empty non-"0"-insensitive string "equal" to null).` |
|        - | 1372 | `		 * Convert the null side to "" and let the string branch below run.` |
|        - | 1373 | `		 */` |
|       45 | 1374 | `		if( pObj1->iFlags & MEMOBJ_NULL ){` |
|       35 | 1375 | `			PH7_MemObjToString(pObj1);` |
|       18 | 1376 | `		}else{` |
|       11 | 1377 | `			PH7_MemObjToString(pObj2);` |
|        - | 1378 | `		}` |
|       45 | 1379 | `		iComb = pObj1->iFlags\|pObj2->iFlags;` |
|       22 | 1380 | `	}` |
|  1360280 | 1381 | `	if( (pObj1->iFlags & MEMOBJ_RES) && (pObj2->iFlags & MEMOBJ_RES) ){` |
|        - | 1382 | `		/* php compares two resources by their ID. The boolean path below would` |
|        - | 1383 | `		 * call every live resource equal to every other, since all are truthy. */` |
|        5 | 1384 | `		sxu32 nId1 = PH7_VmResourceId(pObj1->pVm,pObj1->x.pOther);` |
|        5 | 1385 | `		sxu32 nId2 = PH7_VmResourceId(pObj2->pVm,pObj2->x.pOther);` |
|        5 | 1386 | `		return nId1 == nId2 ? 0 : (nId1 < nId2 ? -1 : 1);` |
|        - | 1387 | `	}` |
|  1360276 | 1388 | `	if( iComb & (MEMOBJ_NULL\|MEMOBJ_RES\|MEMOBJ_BOOL) ){` |
|        - | 1389 | `		/* Convert to boolean: Keep in mind FALSE < TRUE */` |
|    38315 | 1390 | `		if( (pObj1->iFlags & MEMOBJ_BOOL) == 0 ){` |
|    23373 | 1391 | `			PH7_MemObjToBool(pObj1);` |
|    11684 | 1392 | `		}` |
|    38315 | 1393 | `		if( (pObj2->iFlags & MEMOBJ_BOOL) == 0 ){` |
|    22317 | 1394 | `			PH7_MemObjToBool(pObj2);` |
|    11156 | 1395 | `		}` |
|    38315 | 1396 | `		return (sxi32)((pObj1->x.iVal != 0) - (pObj2->x.iVal != 0));` |
|  1321966 | 1397 | `	}else if ( iComb & MEMOBJ_HASHMAP ){` |
|        - | 1398 | `		/* Hashmap aka 'array' comparison */` |
|       56 | 1399 | `		if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1400 | `			/* Array is always greater */` |
|      ! 0 | 1401 | `			return -1;` |
|        - | 1402 | `		}` |
|       56 | 1403 | `		if( (pObj2->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1404 | `			/* Array is always greater */` |
|      ! 0 | 1405 | `			return 1;` |
|        - | 1406 | `		}` |
|        - | 1407 | `		/* Perform the comparison */` |
|       56 | 1408 | `		rc = PH7_HashmapCmp((ph7_hashmap *)pObj1->x.pOther,(ph7_hashmap *)pObj2->x.pOther,bStrict);` |
|       56 | 1409 | `		return rc;` |
|  1321912 | 1410 | `	}else if(iComb & MEMOBJ_OBJ ){` |
|        - | 1411 | `		/* Object comparison */` |
|      295 | 1412 | `		if( (pObj1->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 1413 | `			/* Object is always greater */` |
|      ! 0 | 1414 | `			return -1;` |
|        - | 1415 | `		}` |
|      295 | 1416 | `		if( (pObj2->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 1417 | `			/* Object is always greater */` |
|      ! 0 | 1418 | `			return 1;` |
|        - | 1419 | `		}` |
|        - | 1420 | `		/* Perform the comparison */` |
|      295 | 1421 | `		rc = PH7_ClassInstanceCmp((ph7_class_instance *)pObj1->x.pOther,(ph7_class_instance *)pObj2->x.pOther,bStrict,iNest);` |
|      295 | 1422 | `		return rc;` |
|  1321622 | 1423 | `	}else if ( iComb & MEMOBJ_STRING ){` |
|        - | 1424 | `		SyString s1,s2;` |
|   851549 | 1425 | `		if( !bStrict ){` |
|        - | 1426 | `			/*` |
|        - | 1427 | `			 * PHP 8 "saner string to number comparisons" (RFC): a numeric` |
|        - | 1428 | `			 * comparison is performed only when BOTH operands are numbers or` |
|        - | 1429 | `			 * numeric strings. A number compared with a NON-numeric string is` |
|        - | 1430 | `			 * compared as strings, with the number cast to its string form —` |
|        - | 1431 | `			 * so 0 == "abc" is false, "abc" < 10 is false, and max("abc",10)` |
|        - | 1432 | `			 * is "abc". (PHP 7 cast the non-numeric string to 0 and compared` |
|        - | 1433 | `			 * numerically; comparing when EITHER side was numeric is what this` |
|        - | 1434 | `			 * replaces.) Two non-numeric strings, or one numeric and one` |
|        - | 1435 | `			 * non-numeric string, still fall through to the string comparison` |
|        - | 1436 | `			 * below, unchanged.` |
|        - | 1437 | `			 */` |
|   245695 | 1438 | `			if( PH7_MemObjIsNumeric(pObj1) && PH7_MemObjIsNumeric(pObj2) ){` |
|        - | 1439 | `				/* Perform a numeric comparison */` |
|     1099 | 1440 | `				goto Numeric;` |
|        - | 1441 | `			}` |
|   122181 | 1442 | `		}` |
|        - | 1443 | `		/* Perform a strict string comparison.*/` |
|   850451 | 1444 | `		if( (pObj1->iFlags&MEMOBJ_STRING) == 0 ){` |
|       23 | 1445 | `			PH7_MemObjToString(pObj1);` |
|       11 | 1446 | `		}` |
|   850451 | 1447 | `		if( (pObj2->iFlags&MEMOBJ_STRING) == 0 ){` |
|        7 | 1448 | `			PH7_MemObjToString(pObj2);` |
|        3 | 1449 | `		}` |
|   850451 | 1450 | `		SyStringInitFromBuf(&s1,SyBlobData(&pObj1->sBlob),SyBlobLength(&pObj1->sBlob));` |
|   850451 | 1451 | `		SyStringInitFromBuf(&s2,SyBlobData(&pObj2->sBlob),SyBlobLength(&pObj2->sBlob));` |
|        - | 1452 | `		/*` |
|        - | 1453 | `		 * Strings are compared using memcmp(). If one value is an exact prefix of the` |
|        - | 1454 | `		 * other, then the shorter value is less than the longer value.` |
|        - | 1455 | `		 */` |
|   850451 | 1456 | `		rc = SyMemcmp((const void *)s1.zString,(const void *)s2.zString,SXMIN(s1.nByte,s2.nByte));` |
|   850451 | 1457 | `		if( rc == 0 ){` |
|   278839 | 1458 | `			if( s1.nByte != s2.nByte ){` |
|    18711 | 1459 | `				rc = s1.nByte < s2.nByte ? -1 : 1;` |
|     9355 | 1460 | `			}` |
|   139419 | 1461 | `		}` |
|   850451 | 1462 | `		return rc;` |
|   470078 | 1463 | `	}else if( iComb & (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|   234729 | 1464 | `Numeric:` |
|        - | 1465 | `		/* Perform a numeric comparison if one of the operand is numeric(integer or real) */` |
|   471176 | 1466 | `		if( (pObj1->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|     1081 | 1467 | `			PH7_MemObjToNumeric(pObj1);` |
|      540 | 1468 | `		}` |
|   471176 | 1469 | `		if( (pObj2->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|     1087 | 1470 | `			PH7_MemObjToNumeric(pObj2);` |
|      543 | 1471 | `		}` |
|   471176 | 1472 | `		if( (pObj1->iFlags & pObj2->iFlags & MEMOBJ_INT) == 0) {` |
|        - | 1473 | `			/*` |
|        - | 1474 | `			 * Symisc eXtension to the PHP language:` |
|        - | 1475 | `			 *  Floating point comparison is introduced and works as expected.` |
|        - | 1476 | `			 */` |
|        - | 1477 | `			ph7_real r1,r2;` |
|        - | 1478 | `			/* Compare as reals */` |
|      310 | 1479 | `			if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|       11 | 1480 | `				PH7_MemObjToReal(pObj1);` |
|        5 | 1481 | `			}` |
|      310 | 1482 | `			r1 = pObj1->rVal;` |
|      310 | 1483 | `			if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|       56 | 1484 | `				PH7_MemObjToReal(pObj2);` |
|       27 | 1485 | `			}` |
|      310 | 1486 | `			r2 = pObj2->rVal;` |
|      310 | 1487 | `			if( PH7_IS_NAN(r1) \|\| PH7_IS_NAN(r2) ){` |
|        - | 1488 | `				/*` |
|        - | 1489 | `				 * Keep a strict three-way comparator contract even for NaN values.` |
|        - | 1490 | `				 * For ordering purposes, NaN compares equal to NaN and greater than` |
|        - | 1491 | `				 * any non-NaN numeric value.` |
|        - | 1492 | `				 */` |
|       50 | 1493 | `				if( PH7_IS_NAN(r1) ){` |
|       40 | 1494 | `					return PH7_IS_NAN(r2) ? 0 : 1;` |
|        - | 1495 | `				}` |
|       11 | 1496 | `				return -1;` |
|        - | 1497 | `			}` |
|      262 | 1498 | `			if( r1 > r2 ){` |
|       54 | 1499 | `				return 1;` |
|      210 | 1500 | `			}else if( r1 < r2 ){` |
|      134 | 1501 | `				return -1;` |
|        - | 1502 | `			}` |
|       78 | 1503 | `			return 0;` |
|      ! 0 | 1504 | `		}else{` |
|        - | 1505 | `			/* Integer comparison */` |
|   470868 | 1506 | `			if( pObj1->x.iVal > pObj2->x.iVal ){` |
|     7668 | 1507 | `				return 1;` |
|   463205 | 1508 | `			}else if( pObj1->x.iVal < pObj2->x.iVal ){` |
|   452199 | 1509 | `				return -1;` |
|        - | 1510 | `			}` |
|    11011 | 1511 | `			return 0;` |
|        - | 1512 | `		}` |
|        - | 1513 | `	}` |
|        - | 1514 | `	/* NOT REACHED */` |
|      ! 0 | 1515 | `	return 0;` |
|   778591 | 1516 | `}` |
|        - | 1517 | `/*` |
|        - | 1518 | ` * Perform an addition operation of two ph7_values.` |
|        - | 1519 | ` * The reason this function is implemented here rather than 'vm.c'` |
|        - | 1520 | ` * is that the '+' operator is overloaded.` |
|        - | 1521 | ` * That is,the '+' operator is used for arithmetic operation and also` |
|        - | 1522 | ` * used for operation on arrays [i.e: union]. When used with an array` |
|        - | 1523 | ` * The + operator returns the right-hand array appended to the left-hand array.` |
|        - | 1524 | ` * For keys that exist in both arrays, the elements from the left-hand array` |
|        - | 1525 | ` * will be used, and the matching elements from the right-hand array will` |
|        - | 1526 | ` * be ignored.` |
|        - | 1527 | ` * This function take care of handling all the scenarios.` |
|        - | 1528 | ` */` |
|    14678 | 1529 | `PH7_PRIVATE sxi32 PH7_MemObjAdd(ph7_value *pObj1,ph7_value *pObj2,int bAddStore)` |
|        5 | 1530 | `{` |
|    14683 | 1531 | `	if( ((pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1532 | `			/* Arithemtic operation */` |
|    10847 | 1533 | `			PH7_MemObjToNumeric(pObj1);` |
|    10847 | 1534 | `			PH7_MemObjToNumeric(pObj2);` |
|    10847 | 1535 | `			if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_REAL ){` |
|        - | 1536 | `				/* Floating point arithmetic */` |
|        - | 1537 | `				ph7_real a,b;` |
|       69 | 1538 | `				if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|       29 | 1539 | `					PH7_MemObjToReal(pObj1);` |
|       14 | 1540 | `				}` |
|       69 | 1541 | `				if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|        9 | 1542 | `					PH7_MemObjToReal(pObj2);` |
|        4 | 1543 | `				}` |
|       69 | 1544 | `				a = pObj1->rVal;` |
|       69 | 1545 | `				b = pObj2->rVal;` |
|       69 | 1546 | `				pObj1->rVal = a+b;` |
|       69 | 1547 | `				MemObjSetType(pObj1,MEMOBJ_REAL);` |
|        - | 1548 | `				/* Try to get an integer representation also */` |
|       69 | 1549 | `				MemObjTryIntger(&(*pObj1));` |
|       35 | 1550 | `			}else{` |
|        - | 1551 | `				/* Integer arithmetic; PHP promotes an overflowing sum to float.` |
|        - | 1552 | `				 * The integer-only build (PH7_OMIT_FLOATING_POINT) has no float` |
|        - | 1553 | `				 * type, so it wraps like OP_POW's OMIT path. */` |
|        - | 1554 | `				sxi64 a,b,r;` |
|    10779 | 1555 | `				a = pObj1->x.iVal;` |
|    10779 | 1556 | `				b = pObj2->x.iVal;` |
|    10779 | 1557 | `				if( PH7_ADD_OVERFLOW64(a,b,&r) ){` |
|        - | 1558 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        9 | 1559 | `					pObj1->rVal = (ph7_real)a + (ph7_real)b;` |
|        9 | 1560 | `					MemObjSetType(pObj1,MEMOBJ_REAL);` |
|        - | 1561 | `#else` |
|        - | 1562 | `					pObj1->x.iVal = r;` |
|        - | 1563 | `					MemObjSetType(pObj1,MEMOBJ_INT);` |
|        - | 1564 | `#endif` |
|        5 | 1565 | `				}else{` |
|    10771 | 1566 | `					pObj1->x.iVal = r;` |
|    10771 | 1567 | `					MemObjSetType(pObj1,MEMOBJ_INT);` |
|        - | 1568 | `				}` |
|        - | 1569 | `			}` |
|     5426 | 1570 | `	}else{` |
|     3841 | 1571 | `		if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP ){` |
|        - | 1572 | `			ph7_hashmap *pMap;` |
|        - | 1573 | `			sxi32 rc;` |
|     3841 | 1574 | `			if( bAddStore ){` |
|        - | 1575 | `				/* Do not duplicate the hashmap,use the left one since its an add&store operation.` |
|        - | 1576 | `				 */` |
|        3 | 1577 | `				if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1578 | `					/* Force a hashmap cast */` |
|      ! 0 | 1579 | `					rc = PH7_MemObjToHashmap(pObj1);` |
|      ! 0 | 1580 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 1581 | `						PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|      ! 0 | 1582 | `						return rc;` |
|        - | 1583 | `					}` |
|      ! 0 | 1584 | `				}` |
|        - | 1585 | `				/* COW separate before in-place mutation */` |
|        3 | 1586 | `				pMap = PH7_HashmapCowSeparate(pObj1->pVm,pObj1);` |
|        2 | 1587 | `			}else{` |
|        - | 1588 | `				/* Create a new hashmap */` |
|     3839 | 1589 | `				pMap = PH7_NewHashmap(pObj1->pVm,0,0);` |
|     3839 | 1590 | `				if( pMap == 0){` |
|      ! 0 | 1591 | `					PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|      ! 0 | 1592 | `					return SXERR_MEM;` |
|        - | 1593 | `				}` |
|        - | 1594 | `			}` |
|     3841 | 1595 | `			if( !bAddStore ){` |
|     3839 | 1596 | `				if(pObj1->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1597 | `					/* Perform a hashmap duplication */` |
|     3839 | 1598 | `					PH7_HashmapDup((ph7_hashmap *)pObj1->x.pOther,pMap);` |
|     1922 | 1599 | `				}else{` |
|      ! 0 | 1600 | `					if((pObj1->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 1601 | `						/* Simple insertion */` |
|      ! 0 | 1602 | `						PH7_HashmapInsert(pMap,0,pObj1);` |
|      ! 0 | 1603 | `					}` |
|        - | 1604 | `				}` |
|     1917 | 1605 | `			}` |
|        - | 1606 | `			/* Perform the union */` |
|     3841 | 1607 | `			if(pObj2->iFlags & MEMOBJ_HASHMAP ){` |
|     3841 | 1608 | `				PH7_HashmapUnion(pMap,(ph7_hashmap *)pObj2->x.pOther);` |
|     1923 | 1609 | `			}else{` |
|      ! 0 | 1610 | `				if((pObj2->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 1611 | `					/* Simple insertion */` |
|      ! 0 | 1612 | `					PH7_HashmapInsert(pMap,0,pObj2);` |
|      ! 0 | 1613 | `				}` |
|        - | 1614 | `			}` |
|        - | 1615 | `			/* Reflect the change */` |
|     3841 | 1616 | `			if( pObj1->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 1617 | `				SyBlobRelease(&pObj1->sBlob);` |
|      ! 0 | 1618 | `			}` |
|     3841 | 1619 | `			pObj1->x.pOther = pMap;` |
|     3841 | 1620 | `			MemObjSetType(pObj1,MEMOBJ_HASHMAP);` |
|     1918 | 1621 | `		}` |
|        - | 1622 | `	}` |
|    14683 | 1623 | `	return SXRET_OK;` |
|     7344 | 1624 | `}` |
|        - | 1625 | `/*` |
|        - | 1626 | ` * Return a printable representation of the type of a given` |
|        - | 1627 | ` * ph7_value.` |
|        - | 1628 | ` */` |
|      ! 0 | 1629 | `PH7_PRIVATE const char * PH7_MemObjTypeDump(ph7_value *pVal)` |
|      ! 0 | 1630 | `{` |
|      ! 0 | 1631 | `	const char *zType = "";` |
|      ! 0 | 1632 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|      ! 0 | 1633 | `		zType = "null";` |
|      ! 0 | 1634 | `	}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|        - | 1635 | `		/* REAL is authoritative over a cached MEMOBJ_INT: an integer-valued` |
|        - | 1636 | `		 * real (e.g. 1.0) is reported as "double", matching PHP's gettype(). */` |
|      ! 0 | 1637 | `		zType = "double";` |
|      ! 0 | 1638 | `	}else if( pVal->iFlags & MEMOBJ_INT ){` |
|      ! 0 | 1639 | `		zType = "int";` |
|      ! 0 | 1640 | `	}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 1641 | `		zType = "string";` |
|      ! 0 | 1642 | `	}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 | 1643 | `		zType = "bool";` |
|      ! 0 | 1644 | `	}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|      ! 0 | 1645 | `		zType = "array";` |
|      ! 0 | 1646 | `	}else if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 1647 | `		zType = "object";` |
|      ! 0 | 1648 | `	}else if( pVal->iFlags & MEMOBJ_RES ){` |
|      ! 0 | 1649 | `		zType = "resource";` |
|      ! 0 | 1650 | `	}` |
|      ! 0 | 1651 | `	return zType;` |
|      ! 0 | 1652 | `}` |
|        - | 1653 | `/*` |
|        - | 1654 | ` * Dump a ph7_value [i.e: get a printable representation of it's type and contents.].` |
|        - | 1655 | ` * Store the dump in the given blob.` |
|        - | 1656 | ` */` |
|        - | 1657 | `/*` |
|        - | 1658 | ` * php's var_dump float shape (serialize_precision = -1): the SHORTEST decimal` |
|        - | 1659 | ` * string that round-trips to the same double — 0.1+0.2 dumps every digit` |
|        - | 1660 | ` * (0.30000000000000004), 1.0 dumps "1" — pushed through the same` |
|        - | 1661 | ` * exponent/fraction normalization as echo (PH7_PhpFloatShape: uppercase E,` |
|        - | 1662 | ` * "1.0E+100"). Distinct from echo/casts, which use EG(precision)=14.` |
|        - | 1663 | ` */` |
|        8 | 1664 | `static void MemObjDumpRealValue(SyBlob *pOut,ph7_real rVal)` |
|        1 | 1665 | `{` |
|        9 | 1666 | `	if( PH7_IS_NAN(rVal) ){` |
|      ! 0 | 1667 | `		SyBlobAppend(&(*pOut),"NAN",3);` |
|      ! 0 | 1668 | `		return;` |
|        - | 1669 | `	}` |
|        9 | 1670 | `	if( PH7_IS_INF(rVal) ){` |
|      ! 0 | 1671 | `		SyBlobAppend(&(*pOut),rVal < 0.0 ? "-INF" : "INF",rVal < 0.0 ? 4 : 3);` |
|      ! 0 | 1672 | `		return;` |
|        - | 1673 | `	}` |
|        - | 1674 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - | 1675 | `	{` |
|        - | 1676 | `		char zNum[48];` |
|        9 | 1677 | `		sxi32 n = 0;` |
|        - | 1678 | `		int p;` |
|       11 | 1679 | `		for( p = 1 ; p <= 17 ; p++ ){` |
|       11 | 1680 | `			n = (sxi32)snprintf(zNum,sizeof(zNum),"%.*G",p,rVal);` |
|       11 | 1681 | `			if( n < 0 \|\| n >= (sxi32)sizeof(zNum) ){` |
|      ! 0 | 1682 | `				n = (sxi32)SyStrlen(zNum);` |
|      ! 0 | 1683 | `			}` |
|       11 | 1684 | `			if( strtod(zNum,0) == rVal ){` |
|        9 | 1685 | `				break; /* shortest round-trip found */` |
|        - | 1686 | `			}` |
|        2 | 1687 | `		}` |
|        9 | 1688 | `		n = PH7_PhpFloatShape(zNum,n,TRUE);` |
|        9 | 1689 | `		SyBlobAppend(&(*pOut),zNum,(sxu32)n);` |
|        - | 1690 | `	}` |
|        - | 1691 | `#else` |
|        - | 1692 | `	SyBlobFormat(&(*pOut),"%.15g",rVal);` |
|        - | 1693 | `#endif` |
|        5 | 1694 | `}` |
|        - | 1695 | `/*` |
|        - | 1696 | ` * Emit a value's print_r INLINE representation (php: the echo conversion,` |
|        - | 1697 | ` * except true -> "1" and false/null -> ""). Containers never come through` |
|        - | 1698 | ` * here — the entry renderers recurse into the container dumpers instead.` |
|        - | 1699 | ` */` |
|      210 | 1700 | `PH7_PRIVATE void PH7_MemObjPrintRInline(SyBlob *pOut,ph7_value *pObj)` |
|        2 | 1701 | `{` |
|      212 | 1702 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|        7 | 1703 | `		return;` |
|        - | 1704 | `	}` |
|      206 | 1705 | `	if( pObj->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 | 1706 | `		if( pObj->x.iVal != 0 ){` |
|      ! 0 | 1707 | `			SyBlobAppend(&(*pOut),"1",sizeof(char));` |
|      ! 0 | 1708 | `		}` |
|      ! 0 | 1709 | `		return;` |
|        - | 1710 | `	}` |
|      206 | 1711 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|        - | 1712 | `		/* Strings already hold their bytes (MemObjStringValue only CONVERTS` |
|        - | 1713 | `		 * non-strings into the output) */` |
|      108 | 1714 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      108 | 1715 | `			SyBlobAppend(&(*pOut),SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|       53 | 1716 | `		}` |
|      108 | 1717 | `		return;` |
|        - | 1718 | `	}` |
|      100 | 1719 | `	MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|      107 | 1720 | `}` |
|     1028 | 1721 | `PH7_PRIVATE sxi32 PH7_MemObjDump(` |
|        - | 1722 | `	SyBlob *pOut,      /* Store the dump here */` |
|        - | 1723 | `	ph7_value *pObj,   /* Dump this */` |
|        - | 1724 | `	int ShowType,      /* TRUE for var_dump; FALSE for print_r */` |
|        - | 1725 | `	int nTab,          /* Indent in SPACES: var_dump = this value's own line;` |
|        - | 1726 | `	                    * print_r = the container's parenthesis column */` |
|        - | 1727 | `	int nDepth,        /* Nesting level */` |
|        - | 1728 | `	int isRef          /* TRUE if referenced entry (var_dump prints '&') */` |
|        - | 1729 | `	)` |
|        5 | 1730 | `{` |
|     1033 | 1731 | `	sxi32 rc = SXRET_OK;` |
|        - | 1732 | `	int i;` |
|     1033 | 1733 | `	if( !ShowType ){` |
|        - | 1734 | `		/* ---- print_r ---- php prints scalars inline with NO newline; only` |
|        - | 1735 | `		 * containers render the Array/Object block (which the container` |
|        - | 1736 | `		 * dumpers terminate with ")\n"). References carry no marker. */` |
|      110 | 1737 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|      103 | 1738 | `			return PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,FALSE,nTab,nDepth+1);` |
|        - | 1739 | `		}` |
|        8 | 1740 | `		if( (pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|        6 | 1741 | `			return PH7_ClassInstanceDump(&(*pOut),(ph7_class_instance *)pObj->x.pOther,FALSE,nTab,nDepth+1);` |
|        - | 1742 | `		}` |
|        3 | 1743 | `		PH7_MemObjPrintRInline(&(*pOut),pObj);` |
|        3 | 1744 | `		return SXRET_OK;` |
|        - | 1745 | `	}` |
|        - | 1746 | `	/* ---- var_dump ---- every value renders on its own line at nTab spaces,` |
|        - | 1747 | `	 * php's exact shapes: bool(true), NULL, int(n), float(shortest),` |
|        - | 1748 | `	 * string(N) "s", array(N) { … }, object(C)#id (n) { … }, &-references. */` |
|     5025 | 1749 | `	for( i = 0 ; i < nTab ; i++ ){` |
|     4103 | 1750 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     2053 | 1751 | `	}` |
|      925 | 1752 | `	if( isRef ){` |
|        7 | 1753 | `		SyBlobAppend(&(*pOut),"&",sizeof(char));` |
|        3 | 1754 | `	}` |
|      925 | 1755 | `	if( (pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|      139 | 1756 | `		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|      139 | 1757 | `		if( pInst->pClass->iFlags & PH7_CLASS_ENUM ){` |
|        - | 1758 | ``			/* php 8.1: var_dump of an enum case prints `enum(S::A)` — no body */`` |
|      ! 0 | 1759 | `			ph7_value *pName = PH7_EnumCaseNameValue(pInst);` |
|      ! 0 | 1760 | `			SyBlobFormat(&(*pOut),"enum(%z::",&pInst->pClass->sName);` |
|      ! 0 | 1761 | `			if( pName && SyBlobLength(&pName->sBlob) > 0 ){` |
|      ! 0 | 1762 | `				SyBlobAppend(&(*pOut),SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob));` |
|      ! 0 | 1763 | `			}` |
|      ! 0 | 1764 | `			SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|      ! 0 | 1765 | `			return SXRET_OK;` |
|        - | 1766 | `		}` |
|      139 | 1767 | `		rc = PH7_ClassInstanceDump(&(*pOut),pInst,TRUE,nTab,nDepth+1);` |
|      139 | 1768 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      139 | 1769 | `		return rc;` |
|        - | 1770 | `	}` |
|      789 | 1771 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|       23 | 1772 | `		SyBlobAppend(&(*pOut),"NULL\n",sizeof("NULL\n")-1);` |
|       23 | 1773 | `		return SXRET_OK;` |
|        - | 1774 | `	}` |
|      767 | 1775 | `	if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       25 | 1776 | `		rc = PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,TRUE,nTab,nDepth+1);` |
|       25 | 1777 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       25 | 1778 | `		return rc;` |
|        - | 1779 | `	}` |
|      743 | 1780 | `	if( pObj->iFlags & MEMOBJ_BOOL ){` |
|      296 | 1781 | `		if( pObj->x.iVal != 0 ){` |
|      198 | 1782 | `			SyBlobAppend(&(*pOut),"bool(true)\n",sizeof("bool(true)\n")-1);` |
|      101 | 1783 | `		}else{` |
|      101 | 1784 | `			SyBlobAppend(&(*pOut),"bool(false)\n",sizeof("bool(false)\n")-1);` |
|        - | 1785 | `		}` |
|      296 | 1786 | `		return SXRET_OK;` |
|        - | 1787 | `	}` |
|      450 | 1788 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|        - | 1789 | `		/* Checked BEFORE the int flag: an integer-valued real carries a cached` |
|        - | 1790 | `		 * MEMOBJ_INT view too, and php dumps it as float(1). */` |
|        9 | 1791 | `		SyBlobAppend(&(*pOut),"float(",sizeof("float(")-1);` |
|        9 | 1792 | `		MemObjDumpRealValue(&(*pOut),pObj->rVal);` |
|        9 | 1793 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        9 | 1794 | `		return SXRET_OK;` |
|        - | 1795 | `	}` |
|      442 | 1796 | `	if( pObj->iFlags & MEMOBJ_INT ){` |
|      272 | 1797 | `		SyBlobFormat(&(*pOut),"int(%qd)",pObj->x.iVal);` |
|      272 | 1798 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      272 | 1799 | `		return SXRET_OK;` |
|        - | 1800 | `	}` |
|      174 | 1801 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|      174 | 1802 | `		SyBlobFormat(&(*pOut),"string(%u) \"",SyBlobLength(&pObj->sBlob));` |
|      174 | 1803 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      154 | 1804 | `			SyBlobAppend(&(*pOut),SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|       75 | 1805 | `		}` |
|      174 | 1806 | `		SyBlobAppend(&(*pOut),"\"\n",sizeof("\"\n")-1);` |
|      174 | 1807 | `		return SXRET_OK;` |
|        - | 1808 | `	}` |
|      ! 0 | 1809 | `	if( pObj->iFlags & MEMOBJ_RES ){` |
|        - | 1810 | ``		/* php: `resource(N) of type (stream)`, and `(Unknown)` once closed —`` |
|        - | 1811 | `		 * which is exactly what PH7_VfsResourceType() already reports. The old` |
|        - | 1812 | `		 * shape printed the heap pointer through the string cast instead. */` |
|      ! 0 | 1813 | `		SyBlobFormat(&(*pOut),"resource(%u) of type (%s)\n",` |
|      ! 0 | 1814 | `			PH7_VmResourceId(pObj->pVm,pObj->x.pOther),` |
|      ! 0 | 1815 | `			PH7_VfsResourceType(pObj->x.pOther));` |
|      ! 0 | 1816 | `		return SXRET_OK;` |
|        - | 1817 | `	}` |
|        - | 1818 | ``	/* Anything else: the legacy `type(value)` shape. */`` |
|        - | 1819 | `	{` |
|      ! 0 | 1820 | `		const char *zType = PH7_MemObjTypeDump(pObj);` |
|      ! 0 | 1821 | `		SyBlobAppend(&(*pOut),zType,SyStrlen(zType));` |
|      ! 0 | 1822 | `		SyBlobAppend(&(*pOut),"(",sizeof(char));` |
|      ! 0 | 1823 | `		MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|      ! 0 | 1824 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        - | 1825 | `	}` |
|      ! 0 | 1826 | `	return rc;` |
|      519 | 1827 | `}` |
|        - | 1828 |  |
