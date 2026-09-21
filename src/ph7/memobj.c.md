# src/ph7/memobj.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 836/1018 lines (82.12%)

[Root index](../../index.md) | [Directory index](index.md)

|      Hits | Line | Source |
| --------: | ---: | :--- |
|         - |    1 | `/**` |
|         - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|         - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|         - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|         - |    5 | ` */` |
|         - |    6 | `#include "ph7int.h" /* This file handle low-level stuff related to indexed memory objects [i.e: ph7_value] */` |
|         - |    7 | `#include <stdio.h>  /* snprintf — the default float->string conversion needs` |
|         - |    8 | `                     * correctly-rounded digits like php (see MemObjStringValue) */` |
|         - |    9 | `#include <stdlib.h> /* strtod — var_dump's shortest-round-trip float shape` |
|         - |   10 | `                     * verifies each candidate by parsing it back */` |
|         - |   11 |  |
|         - |   12 | `/* Portable 64-bit overflow-detecting arithmetic for compilers that lack the` |
|         - |   13 | ` * GCC/Clang __builtin_*_overflow intrinsics (i.e. MSVC). The header exposes` |
|         - |   14 | ` * these through the PH7_{ADD,SUB,MUL}_OVERFLOW64 macros; the intrinsic path` |
|         - |   15 | ` * needs no out-of-line definition, so gate the whole block off there to avoid` |
|         - |   16 | ` * an unused-function warning. Each sets *pR to the two's-complement wrapped` |
|         - |   17 | ` * result and returns non-zero on overflow. The additive checks compute the` |
|         - |   18 | ` * wrapped result via unsigned math (no signed-overflow UB) and test the sign` |
|         - |   19 | ` * bits; the multiplicative check mirrors vm.c's proven bound-check form. */` |
|         - |   20 | `#if !(defined(__GNUC__) \|\| defined(__clang__))` |
|         - |   21 | `PH7_PRIVATE int PH7_AddOverflow64(sxi64 a,sxi64 b,sxi64 *pR)` |
|         5 |   22 | `{` |
|         5 |   23 | `	*pR = (sxi64)((sxu64)a + (sxu64)b);` |
|         - |   24 | `	/* Overflow iff the operands share a sign and the result's sign differs. */` |
|         5 |   25 | `	return ((a ^ *pR) & (b ^ *pR)) < 0;` |
|         5 |   26 | `}` |
|         - |   27 | `PH7_PRIVATE int PH7_SubOverflow64(sxi64 a,sxi64 b,sxi64 *pR)` |
|         5 |   28 | `{` |
|         5 |   29 | `	*pR = (sxi64)((sxu64)a - (sxu64)b);` |
|         - |   30 | `	/* Overflow iff the operands differ in sign and the result's sign differs` |
|         - |   31 | `	 * from the minuend's. */` |
|         5 |   32 | `	return ((a ^ b) & (a ^ *pR)) < 0;` |
|         5 |   33 | `}` |
|         - |   34 | `PH7_PRIVATE int PH7_MulOverflow64(sxi64 a,sxi64 b,sxi64 *pR)` |
|         5 |   35 | `{` |
|         5 |   36 | `	*pR = (sxi64)((sxu64)a * (sxu64)b);` |
|         5 |   37 | `	if( a == 0 \|\| b == 0 \|\| a == 1 \|\| b == 1 ){` |
|         4 |   38 | `		return 0;` |
|         - |   39 | `	}` |
|         4 |   40 | `	if( a == -1 ){` |
|         1 |   41 | `		return b == SMALLEST_INT64;` |
|         - |   42 | `	}` |
|         4 |   43 | `	if( b == -1 ){` |
|       ! 0 |   44 | `		return a == SMALLEST_INT64;` |
|         - |   45 | `	}` |
|         4 |   46 | `	if( a > 0 ){` |
|         4 |   47 | `		if( b > 0 ){` |
|         4 |   48 | `			return a > LARGEST_INT64 / b;` |
|       ! 0 |   49 | `		}else{` |
|         1 |   50 | `			return b < SMALLEST_INT64 / a;` |
|         - |   51 | `		}` |
|       ! 0 |   52 | `	}else{` |
|         1 |   53 | `		if( b > 0 ){` |
|         1 |   54 | `			return a < SMALLEST_INT64 / b;` |
|       ! 0 |   55 | `		}else{` |
|         1 |   56 | `			return b < LARGEST_INT64 / a;` |
|         - |   57 | `		}` |
|         - |   58 | `	}` |
|         5 |   59 | `}` |
|         - |   60 | `#endif` |
|         - |   61 |  |
|         - |   62 | `/* Provide PHP-style type names for values.  This utility may be reused` |
|         - |   63 | ` * by any subsystem that works with ph7_value.` |
|         - |   64 | ` */` |
|       524 |   65 | `PH7_PRIVATE const char *ph7_type_name(ph7_value *pVal)` |
|         5 |   66 | `{` |
|       529 |   67 | `	if( ph7_value_is_null(pVal) ) return "null";` |
|       481 |   68 | `	if( ph7_value_is_bool(pVal) ) return "bool";` |
|       473 |   69 | `	if( ph7_value_is_int(pVal) ) return "int";` |
|       337 |   70 | `	if( ph7_value_is_float(pVal) ) return "float";` |
|       317 |   71 | `	if( ph7_value_is_string(pVal) ) return "string";` |
|        49 |   72 | `	if( ph7_value_is_array(pVal) ) return "array";` |
|         3 |   73 | `	if( ph7_value_is_object(pVal) ) return "object";` |
|       ! 0 |   74 | `	if( ph7_value_is_resource(pVal) ) return "resource";` |
|       ! 0 |   75 | `	return "unknown";` |
|       267 |   76 | `}` |
|         - |   77 |  |
|         - |   78 | `/*` |
|         - |   79 | ` * Notes on memory objects [i.e: ph7_value].` |
|         - |   80 | ` * Internally, the PH7 virtual machine manipulates nearly all PHP values` |
|         - |   81 | ` * [i.e: string,int,float,resource,object,bool,null..] as ph7_values structures.` |
|         - |   82 | ` * Each ph7_values struct may cache multiple representations (string,` |
|         - |   83 | ` * integer etc.) of the same value.` |
|         - |   84 | ` */` |
|         - |   85 | `/*` |
|         - |   86 | ` * Convert a 64-bit IEEE double into a 64-bit signed integer.` |
|         - |   87 | ` * If the double is too large, return 0x8000000000000000.` |
|         - |   88 | ` *` |
|         - |   89 | ` * Most systems appear to do this simply by assigning ariables and without` |
|         - |   90 | ` * the extra range tests.` |
|         - |   91 | ` * But there are reports that windows throws an expection if the floating` |
|         - |   92 | ` * point value is out of range.` |
|         - |   93 | ` */` |
|     11460 |   94 | `static sxi64 MemObjRealToInt(ph7_value *pObj)` |
|         5 |   95 | `{` |
|         - |   96 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|         - |   97 | `	/* Real and 64bit integer are the same when floating point arithmetic` |
|         - |   98 | `	 * is omitted from the build.` |
|         - |   99 | `	 */` |
|         - |  100 | `	return pObj->rVal;` |
|         - |  101 | `#else` |
|         - |  102 | ` /*` |
|         - |  103 | `  ** Many compilers we encounter do not define constants for the` |
|         - |  104 | `  ** minimum and maximum 64-bit integers, or they define them` |
|         - |  105 | `  ** inconsistently.  And many do not understand the "LL" notation.` |
|         - |  106 | `  ** So we define our own static constants here using nothing` |
|         - |  107 | `  ** larger than a 32-bit integer constant.` |
|         - |  108 | `  */` |
|         - |  109 | `  static const sxi64 maxInt = LARGEST_INT64;` |
|         - |  110 | `  static const sxi64 minInt = SMALLEST_INT64;` |
|     11465 |  111 | `  ph7_real r = pObj->rVal;` |
|     11465 |  112 | `  if( r<(ph7_real)minInt ){` |
|         3 |  113 | `    return minInt;` |
|     11463 |  114 | `  }else if( r>(ph7_real)maxInt ){` |
|         - |  115 | `    /* minInt is correct here - not maxInt.  It turns out that assigning` |
|         - |  116 | `    ** a very large positive number to an integer results in a very large` |
|         - |  117 | `    ** negative integer.  This makes no sense, but it is what x86 hardware` |
|         - |  118 | `    ** does so for compatibility we will do the same in software. */` |
|       184 |  119 | `    return minInt;` |
|       ! 0 |  120 | `  }else{` |
|     11281 |  121 | `    return (sxi64)r;` |
|         - |  122 | `  }` |
|         - |  123 | `#endif` |
|      5735 |  124 | `}` |
|         - |  125 | `/*` |
|         - |  126 | ` * Convert a raw token value typically a stream of digit [i.e: hex,octal,binary or decimal]` |
|         - |  127 | ` * to a 64-bit integer.` |
|         - |  128 | ` */` |
|   4109858 |  129 | `PH7_PRIVATE sxi64 PH7_TokenValueToInt64(SyString *pVal)` |
|         5 |  130 | `{` |
|   4109863 |  131 | `	sxi64 iVal = 0;` |
|   4109863 |  132 | `	if( pVal->nByte <= 0 ){` |
|       ! 0 |  133 | `		return 0;` |
|         - |  134 | `	}` |
|   4109863 |  135 | `	if( pVal->zString[0] == '0' ){` |
|         - |  136 | `		sxi32 c;` |
|   1613549 |  137 | `		if( pVal->nByte == sizeof(char) ){` |
|   1497157 |  138 | `			return 0;` |
|         - |  139 | `		}` |
|    116397 |  140 | `		c = pVal->zString[1];` |
|    116397 |  141 | `		if( c  == 'x' \|\| c == 'X' ){` |
|         - |  142 | `			/* Hex digit stream */` |
|    111879 |  143 | `			SyHexStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|     60460 |  144 | `		}else if( c == 'b' \|\| c == 'B' ){` |
|         - |  145 | `			/* Binary digit stream */` |
|       285 |  146 | `			SyBinaryStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|      4381 |  147 | `		}else if( c == 'o' \|\| c == 'O' ){` |
|         - |  148 | `			/* PHP 8.1 explicit octal 0o/0O: skip the two-char prefix and parse the` |
|         - |  149 | `			 * remaining octal digits (SyOctalStrToInt64 expects no letter prefix). */` |
|        21 |  150 | `			if( pVal->nByte > 2 ){` |
|        21 |  151 | `				SyOctalStrToInt64(pVal->zString + 2,pVal->nByte - 2,(void *)&iVal,0);` |
|        10 |  152 | `			}` |
|        11 |  153 | `		}else{` |
|         - |  154 | `			/* Legacy octal digit stream (leading 0) */` |
|      4219 |  155 | `			SyOctalStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|         - |  156 | `		}` |
|     58201 |  157 | `	}else{` |
|         - |  158 | `		/* Decimal digit stream */` |
|   2496319 |  159 | `		SyStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|         - |  160 | `	}` |
|   2612711 |  161 | `	return iVal;` |
|   2054934 |  162 | `}` |
|         - |  163 | `/*` |
|         - |  164 | ` * Return some kind of 64-bit integer value which is the best we can` |
|         - |  165 | ` * do at representing the value that pObj describes as a string` |
|         - |  166 | ` * representation.` |
|         - |  167 | ` */` |
|      3741 |  168 | `static sxi64 MemObjStringToInt(ph7_value *pObj)` |
|         5 |  169 | `{` |
|      3746 |  170 | `	sxi64 iVal = 0;` |
|         - |  171 | `	/* A *string* is always read in base 10 by php: "012" is 12, "0x1A" and "0b11"` |
|         - |  172 | `	 * are 0. Only a source *literal* carries a base prefix, and that is decoded by` |
|         - |  173 | `	 * the compiler (PH7_TokenValueToInt64) -- not here. */` |
|      3746 |  174 | `	SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&iVal,0);` |
|      3746 |  175 | `	return iVal;` |
|         5 |  176 | `}` |
|         - |  177 | `/*` |
|         - |  178 | ` * Call a magic class method [i.e: __toString(),__toInt(),...]` |
|         - |  179 | ` * Return SXRET_OK if the magic method is available and have been` |
|         - |  180 | ` * successfully called. Any other return value indicates failure.` |
|         - |  181 | ` */` |
|       186 |  182 | `static sxi32 MemObjCallClassCastMethod(` |
|         - |  183 | `	ph7_vm *pVm,               /* VM that trigger the invocation */` |
|         - |  184 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object] */` |
|         - |  185 | `	const char *zMethod,       /* Magic method name [i.e: __toString] */` |
|         - |  186 | `	sxu32 nLen,                /* Method name length */` |
|         - |  187 | `	ph7_value *pResult         /* OUT: Store the return value of the magic method here */` |
|         - |  188 | `	)` |
|         5 |  189 | `{` |
|         - |  190 | `	ph7_class_method *pMethod;` |
|         - |  191 | `	/* Check if the method is available */` |
|       191 |  192 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,zMethod,nLen);` |
|       191 |  193 | `	if( pMethod == 0 ){` |
|         - |  194 | `		/* No such method */` |
|         3 |  195 | `		return SXERR_NOTFOUND;` |
|         - |  196 | `	}` |
|         - |  197 | `	/* Invoke the desired method */` |
|       189 |  198 | `	PH7_VmCallClassMethod(&(*pVm),&(*pThis),pMethod,&(*pResult),0,0);` |
|         - |  199 | `	/* Method successfully called,pResult should hold the return value */` |
|       189 |  200 | `	return SXRET_OK;` |
|        98 |  201 | `}` |
|         - |  202 | `/*` |
|         - |  203 | ` * Return some kind of integer value which is the best we can` |
|         - |  204 | ` * do at representing the value that pObj describes as an integer.` |
|         - |  205 | ` * If pObj is an integer, then the value is exact. If pObj is` |
|         - |  206 | ` * a floating-point then  the value returned is the integer part.` |
|         - |  207 | ` * If pObj is a string, then we make an attempt to convert it into` |
|         - |  208 | ` * a integer and return that.` |
|         - |  209 | ` * If pObj represents a NULL value, return 0.` |
|         - |  210 | ` */` |
|      1598 |  211 | `static sxi64 MemObjIntValue(ph7_value *pObj)` |
|         5 |  212 | `{` |
|         - |  213 | `	sxi32 iFlags;` |
|      1603 |  214 | `	iFlags = pObj->iFlags;` |
|      1603 |  215 | `	if (iFlags & MEMOBJ_REAL ){` |
|        18 |  216 | `		return MemObjRealToInt(&(*pObj));` |
|      1587 |  217 | `	}else if( iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|       175 |  218 | `		return pObj->x.iVal;` |
|      1415 |  219 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|      1397 |  220 | `		return MemObjStringToInt(&(*pObj));` |
|        20 |  221 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|         6 |  222 | `		return 0;` |
|        15 |  223 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|         - |  224 | `		/* php: (int) of an array is 0 when empty, 1 otherwise -- NOT the element` |
|         - |  225 | ``		 * count. PHL returned the count, so `(int)[1,2,3]` was 3. (bool) already`` |
|         - |  226 | `		 * followed php; int/float did not.) */` |
|         7 |  227 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|         7 |  228 | `		sxu32 n = pMap->nEntry;` |
|         7 |  229 | `		PH7_HashmapUnref(pMap);` |
|         7 |  230 | `		return n > 0 ? 1 : 0;` |
|         9 |  231 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|         - |  232 | `		/* php has NO __toInt(): casting an object to int warns and yields 1. PH7's` |
|         - |  233 | `		 * __toInt() was an extension that changed the meaning of valid php source` |
|         - |  234 | ``		 * (§10), so `(int)$obj` silently returned user data where php diagnoses. */`` |
|         7 |  235 | `		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|         7 |  236 | `		if( pInst && pInst->pClass ){` |
|        10 |  237 | `			VmErrorFormat(pObj->pVm,PH7_CTX_WARNING,` |
|         6 |  238 | `				"Object of class %z could not be converted to int",&pInst->pClass->sName);` |
|         3 |  239 | `		}` |
|         7 |  240 | `		PH7_ClassInstanceUnref(pInst);` |
|         7 |  241 | `		return 1;` |
|         3 |  242 | `	}else if(iFlags & MEMOBJ_RES ){` |
|         - |  243 | `		/* php casts a resource to its ID, not to 1: two distinct resources must not` |
|         - |  244 | `		 * compare equal, which they did while every one of them cast to 1. */` |
|         3 |  245 | `		return (sxi64)PH7_VmResourceId(pObj->pVm,pObj->x.pOther);` |
|         - |  246 | `	}` |
|         - |  247 | `	/* CANT HAPPEN */` |
|       ! 0 |  248 | `	return 0;` |
|       804 |  249 | `}` |
|         - |  250 | `/*` |
|         - |  251 | ` * Return some kind of real value which is the best we can` |
|         - |  252 | ` * do at representing the value that pObj describes as a real.` |
|         - |  253 | ` * If pObj is a real, then the value is exact.If pObj is an` |
|         - |  254 | ` * integer then the integer  is promoted to real and that value` |
|         - |  255 | ` * is returned.` |
|         - |  256 | ` * If pObj is a string, then we make an attempt to convert it` |
|         - |  257 | ` * into a real and return that.` |
|         - |  258 | ` * If pObj represents a NULL value, return 0.0` |
|         - |  259 | ` */` |
|     10260 |  260 | `static ph7_real MemObjRealValue(ph7_value *pObj)` |
|         5 |  261 | `{` |
|         - |  262 | `	sxi32 iFlags;` |
|     10265 |  263 | `	iFlags = pObj->iFlags;` |
|     10265 |  264 | `	if( iFlags & MEMOBJ_REAL ){` |
|       ! 0 |  265 | `		return pObj->rVal;` |
|     10265 |  266 | `	}else if (iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|       791 |  267 | `		return (ph7_real)pObj->x.iVal;` |
|      9479 |  268 | `	}else if (iFlags & MEMOBJ_STRING){` |
|         - |  269 | `		SyString sString;` |
|         - |  270 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|         - |  271 | `		ph7_real rVal = 0;` |
|         - |  272 | `#else` |
|      9471 |  273 | `		ph7_real rVal = 0.0;` |
|         - |  274 | `#endif` |
|      9471 |  275 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|      9471 |  276 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|         - |  277 | `			/* Convert as much as we can */` |
|         - |  278 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|         - |  279 | `			rVal = MemObjStringToInt(&(*pObj));` |
|         - |  280 | `#else` |
|      9471 |  281 | `			SyStrToReal(sString.zString,sString.nByte,(void *)&rVal,0);` |
|         - |  282 | `#endif` |
|      4733 |  283 | `		}` |
|      9471 |  284 | `		return rVal;` |
|         9 |  285 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|         - |  286 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|         - |  287 | `		return 0;` |
|         - |  288 | `#else` |
|       ! 0 |  289 | `		return 0.0;` |
|         - |  290 | `#endif` |
|         9 |  291 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|         - |  292 | `		/* php: (float) of an array is 0.0 when empty, 1.0 otherwise -- see the int` |
|         - |  293 | `		 * branch above. */` |
|       ! 0 |  294 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|       ! 0 |  295 | `		sxu32 n = pMap->nEntry;` |
|       ! 0 |  296 | `		PH7_HashmapUnref(pMap);` |
|       ! 0 |  297 | `		return n > 0 ? (ph7_real)1.0 : (ph7_real)0.0;` |
|         9 |  298 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|         - |  299 | `		/* php has NO __toFloat(): casting an object to float warns and yields 1.0. */` |
|         7 |  300 | `		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|         7 |  301 | `		if( pInst && pInst->pClass ){` |
|        10 |  302 | `			VmErrorFormat(pObj->pVm,PH7_CTX_WARNING,` |
|         6 |  303 | `				"Object of class %z could not be converted to float",&pInst->pClass->sName);` |
|         3 |  304 | `		}` |
|         7 |  305 | `		PH7_ClassInstanceUnref(pInst);` |
|         7 |  306 | `		return (ph7_real)1.0;` |
|         3 |  307 | `	}else if(iFlags & MEMOBJ_RES ){` |
|         3 |  308 | `		return (ph7_real)PH7_VmResourceId(pObj->pVm,pObj->x.pOther);` |
|         - |  309 | `	}` |
|         - |  310 | `	/* NOT REACHED  */` |
|       ! 0 |  311 | `	return 0;` |
|      5135 |  312 | `}` |
|         - |  313 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         - |  314 | `/*` |
|         - |  315 | ` * Post-process a libc-formatted float into php's exact shape (php_gcvt /` |
|         - |  316 | ` * smart_str_append_double semantics): strip the exponent's zero padding` |
|         - |  317 | ` * (libc's 1e+08 becomes php's 1e+8; a zero exponent stays e+0) and, when` |
|         - |  318 | ` * bGeneric is set (%g-style output, including the default float->string` |
|         - |  319 | ` * cast), make an exponent-form mantissa keep a fractional digit` |
|         - |  320 | ` * (1e+20 -> 1.0e+20). zBuf must be NUL-terminated with at least two bytes` |
|         - |  321 | ` * of spare capacity past the NUL. Returns the new length.` |
|         - |  322 | ` * Defined here (not builtin.c) because the float->string cast below needs it` |
|         - |  323 | ` * even when builtin.c's formatting region is compiled out` |
|         - |  324 | ` * (PH7_DISABLE_DISK_IO); the printf family reuses it from PH7_InputFormat.` |
|         - |  325 | ` */` |
|       536 |  326 | `PH7_PRIVATE sxi32 PH7_PhpFloatShape(char *zBuf,sxi32 nLen,int bGeneric)` |
|         4 |  327 | `{` |
|         - |  328 | `	sxi32 iExp,i;` |
|       540 |  329 | `	iExp = nLen - 1;` |
|      4498 |  330 | `	while( iExp > 0 && zBuf[iExp] != 'e' && zBuf[iExp] != 'E' ){` |
|      3962 |  331 | `		iExp--;` |
|         4 |  332 | `	}` |
|       540 |  333 | `	if( iExp <= 0 ){` |
|       494 |  334 | `		return nLen; /* No exponent part (fixed notation) */` |
|         - |  335 | `	}` |
|         - |  336 | `	{` |
|        47 |  337 | `		sxi32 iDig = iExp + 1;` |
|         - |  338 | `		sxi32 iFirst;` |
|        47 |  339 | `		if( zBuf[iDig] == '+' \|\| zBuf[iDig] == '-' ){` |
|        47 |  340 | `			iDig++;` |
|        23 |  341 | `		}` |
|        47 |  342 | `		iFirst = iDig;` |
|        83 |  343 | `		while( zBuf[iFirst] == '0' && iFirst + 1 < nLen` |
|        61 |  344 | `		 && zBuf[iFirst+1] >= '0' && zBuf[iFirst+1] <= '9' ){` |
|        25 |  345 | `			iFirst++;` |
|         1 |  346 | `		}` |
|        47 |  347 | `		if( iFirst > iDig ){` |
|        25 |  348 | `			sxi32 nStrip = iFirst - iDig;` |
|        73 |  349 | `			for( i = iDig ; i + nStrip <= nLen ; i++ ){` |
|        49 |  350 | `				zBuf[i] = zBuf[i+nStrip]; /* moves the NUL too */` |
|        25 |  351 | `			}` |
|        25 |  352 | `			nLen -= nStrip;` |
|        12 |  353 | `		}` |
|         - |  354 | `	}` |
|        47 |  355 | `	if( bGeneric ){` |
|        31 |  356 | `		int bHasDot = 0;` |
|        63 |  357 | `		for( i = 0 ; i < iExp ; i++ ){` |
|        45 |  358 | `			if( zBuf[i] == '.' ){ bHasDot = 1; break; }` |
|        17 |  359 | `		}` |
|        31 |  360 | `		if( !bHasDot ){` |
|       107 |  361 | `			for( i = nLen ; i >= iExp ; i-- ){` |
|        89 |  362 | `				zBuf[i+2] = zBuf[i]; /* moves the NUL too */` |
|        45 |  363 | `			}` |
|        19 |  364 | `			zBuf[iExp] = '.';` |
|        19 |  365 | `			zBuf[iExp+1] = '0';` |
|        19 |  366 | `			nLen += 2;` |
|         9 |  367 | `		}` |
|        15 |  368 | `	}` |
|        47 |  369 | `	return nLen;` |
|       272 |  370 | `}` |
|         - |  371 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|         - |  372 | `/*` |
|         - |  373 | ` * Return the string representation of a given ph7_value.` |
|         - |  374 | ` * This function never fail and always return SXRET_OK.` |
|         - |  375 | ` */` |
|     63178 |  376 | `static sxi32 MemObjStringValue(SyBlob *pOut,ph7_value *pObj,sxu8 bStrictBool)` |
|         5 |  377 | `{` |
|     63183 |  378 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         - |  379 | `		/* Handle special floating-point values first */` |
|       376 |  380 | `		if( PH7_IS_NAN(pObj->rVal) ){` |
|       ! 0 |  381 | `			SyBlobAppend(&(*pOut),"NAN",3);` |
|       376 |  382 | `		}else if( PH7_IS_INF(pObj->rVal) ){` |
|         5 |  383 | `			if( pObj->rVal < 0.0 ){` |
|       ! 0 |  384 | `				SyBlobAppend(&(*pOut),"-INF",4);` |
|       ! 0 |  385 | `			}else{` |
|         5 |  386 | `				SyBlobAppend(&(*pOut),"INF",3);` |
|         - |  387 | `			}` |
|         3 |  388 | `		}else{` |
|         - |  389 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         - |  390 | `			/* php's default float->string conversion (echo/concat/cast):` |
|         - |  391 | `			 * zend_gcvt with EG(precision)=14 and an uppercase exponent` |
|         - |  392 | `			 * marker (smart_str_append_double) — 1/3 -> "0.33333333333333",` |
|         - |  393 | `			 * 1e15 -> "1.0E+15", -0.0 -> "-0". libc snprintf supplies` |
|         - |  394 | `			 * correctly-rounded digits; PH7_PhpFloatShape applies php's` |
|         - |  395 | `			 * exponent/fraction quirks. */` |
|         - |  396 | `			char zNum[48]; /* %.14G peaks at ~22 bytes; +2 spare for ".0" */` |
|       372 |  397 | `			sxi32 n = (sxi32)snprintf(zNum,sizeof(zNum),"%.14G",pObj->rVal);` |
|       372 |  398 | `			if( n < 0 \|\| n >= (sxi32)sizeof(zNum) ){` |
|       ! 0 |  399 | `				n = (sxi32)SyStrlen(zNum);` |
|       ! 0 |  400 | `			}` |
|       372 |  401 | `			n = PH7_PhpFloatShape(zNum,n,TRUE);` |
|       372 |  402 | `			SyBlobAppend(&(*pOut),zNum,(sxu32)n);` |
|         - |  403 | `#else` |
|         - |  404 | `			SyBlobFormat(&(*pOut),"%.15g",pObj->rVal);` |
|         - |  405 | `#endif` |
|         4 |  406 | `		}` |
|     62997 |  407 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|     62421 |  408 | `		SyBlobFormat(&(*pOut),"%qd",pObj->x.iVal);` |
|         - |  409 | `		/* %qd (BSD quad) is equivalent to %lld in the libc printf */` |
|     31603 |  410 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|        53 |  411 | `		if( bStrictBool ){` |
|         - |  412 | `			/* Actual string cast: true -> "1", false -> "" (like PHP) */` |
|        53 |  413 | `			if( pObj->x.iVal ){` |
|        39 |  414 | `				SyBlobAppend(&(*pOut),"1",sizeof("1")-1);` |
|        18 |  415 | `			}` |
|         - |  416 | `			/* false produces empty string, nothing to append */` |
|        29 |  417 | `		}else{` |
|         - |  418 | `			/* Display path (var_dump, print_r): show TRUE/FALSE */` |
|       ! 0 |  419 | `			if( pObj->x.iVal ){` |
|       ! 0 |  420 | `				SyBlobAppend(&(*pOut),"TRUE",sizeof("TRUE")-1);` |
|       ! 0 |  421 | `			}else{` |
|       ! 0 |  422 | `				SyBlobAppend(&(*pOut),"FALSE",sizeof("FALSE")-1);` |
|         - |  423 | `			}` |
|         5 |  424 | `		}` |
|       371 |  425 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|        48 |  426 | `		SyBlobAppend(&(*pOut),"Array",sizeof("Array")-1);` |
|        48 |  427 | `		PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
|       324 |  428 | `	}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|         - |  429 | `		ph7_value sResult;` |
|         - |  430 | `		sxi32 rc;` |
|         - |  431 | `		/* Invoke the __toString() method if available */` |
|       191 |  432 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|       191 |  433 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|         - |  434 | `			"__toString",sizeof("__toString")-1,&sResult);` |
|       191 |  435 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_STRING) && SyBlobLength(&sResult.sBlob) > 0){` |
|         - |  436 | `			/* Expand method return value */` |
|       113 |  437 | `			SyBlobDup(&sResult.sBlob,pOut);` |
|        59 |  438 | `		}else{` |
|         - |  439 | `			/* Expand "Object" as requested by the PHP language reference manual */` |
|        81 |  440 | `			SyBlobAppend(&(*pOut),"Object",sizeof("Object")-1);` |
|         - |  441 | `		}` |
|       191 |  442 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|       191 |  443 | `		PH7_MemObjRelease(&sResult);` |
|       208 |  444 | `	}else if(pObj->iFlags & MEMOBJ_RES ){` |
|         - |  445 | `		/* php renders a resource as "Resource id #N" with its sequential id; the` |
|         - |  446 | `		 * old "ResourceID_0x<pointer>" leaked an address and matched nothing. */` |
|         5 |  447 | `		SyBlobFormat(&(*pOut),"Resource id #%u",PH7_VmResourceId(pObj->pVm,pObj->x.pOther));` |
|         2 |  448 | `	}` |
|     63183 |  449 | `	return SXRET_OK;` |
|         5 |  450 | `}` |
|         - |  451 | `/*` |
|         - |  452 | ` * Return some kind of boolean value which is the best we can do` |
|         - |  453 | ` * at representing the value that pObj describes as a boolean.` |
|         - |  454 | ` * When converting to boolean, the following values are considered FALSE` |
|         - |  455 | ` * (php's exact set):` |
|         - |  456 | ` * NULL` |
|         - |  457 | ` * the boolean FALSE itself.` |
|         - |  458 | ` * the integer 0 (zero).` |
|         - |  459 | ` * the real 0.0 (zero).` |
|         - |  460 | ` * the empty string "" and the string "0" (nothing else: "00", "0.0", " ",` |
|         - |  461 | ` * and "false" are all TRUE in php — the historical PH7 zero-stream and` |
|         - |  462 | ` * "false"/"on"/"yes" special cases changed the meaning of valid PHP source` |
|         - |  463 | ` * and were removed under the §10 PH7-ism policy).` |
|         - |  464 | ` * an array with zero elements.` |
|         - |  465 | ` */` |
|     57370 |  466 | `static sxi32 MemObjBooleanValue(ph7_value *pObj)` |
|         5 |  467 | `{` |
|         - |  468 | `	sxi32 iFlags;` |
|     57375 |  469 | `	iFlags = pObj->iFlags;` |
|     57375 |  470 | `	if (iFlags & MEMOBJ_REAL ){` |
|         - |  471 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|         - |  472 | `		return pObj->rVal ? 1 : 0;` |
|         - |  473 | `#else` |
|        17 |  474 | `		return pObj->rVal != 0.0 ? 1 : 0;` |
|         - |  475 | `#endif` |
|     57361 |  476 | `	}else if( iFlags & MEMOBJ_INT ){` |
|       985 |  477 | `		return pObj->x.iVal ? 1 : 0;` |
|     56381 |  478 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|         - |  479 | `		SyString sString;` |
|        96 |  480 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|         - |  481 | `		/* php: a string is FALSE iff it is empty or exactly "0" */` |
|        96 |  482 | `		if( sString.nByte == 0 ){` |
|        19 |  483 | `			return 0;` |
|         - |  484 | `		}` |
|        79 |  485 | `		if( sString.nByte == 1 && sString.zString[0] == '0' ){` |
|        10 |  486 | `			return 0;` |
|         - |  487 | `		}` |
|        71 |  488 | `		return 1;` |
|     56289 |  489 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|     54931 |  490 | `		return 0;` |
|      1363 |  491 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|        20 |  492 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|        20 |  493 | `		sxu32 n = pMap->nEntry;` |
|        20 |  494 | `		PH7_HashmapUnref(pMap);` |
|        20 |  495 | `		return n > 0 ? TRUE : FALSE;` |
|      1345 |  496 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|         - |  497 | `		/* php has NO __toBool(): an object is ALWAYS truthy, with no diagnostic.` |
|         - |  498 | ``		 * PH7's __toBool() could make `if ($obj)` take the other branch, so this`` |
|         - |  499 | `		 * extension changed control flow in valid php source. */` |
|       196 |  500 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|       196 |  501 | `		return 1;` |
|      1151 |  502 | `	}else if(iFlags & MEMOBJ_RES ){` |
|      1151 |  503 | `		return pObj->x.pOther != 0;` |
|         - |  504 | `	}` |
|         - |  505 | `	/* NOT REACHED */` |
|       ! 0 |  506 | `	return 0;` |
|     28690 |  507 | `}` |
|         - |  508 | `/*` |
|         - |  509 | ` * If the ph7_value is of type real,try to make it an integer also.` |
|         - |  510 | ` */` |
|     11444 |  511 | `static sxi32 MemObjTryIntger(ph7_value *pObj)` |
|         5 |  512 | `{` |
|     11449 |  513 | `	pObj->x.iVal = MemObjRealToInt(&(*pObj));` |
|         - |  514 | `  /* Only mark the value as an integer if` |
|         - |  515 | `  **` |
|         - |  516 | `  **    (1) the round-trip conversion real->int->real is a no-op, and` |
|         - |  517 | `  **    (2) The integer is neither the largest nor the smallest` |
|         - |  518 | `  **        possible integer` |
|         - |  519 | `  **` |
|         - |  520 | `  ** The second and third terms in the following conditional enforces` |
|         - |  521 | `  ** the second condition under the assumption that addition overflow causes` |
|         - |  522 | `  ** values to wrap around.  On x86 hardware, the third term is always` |
|         - |  523 | `  ** true and could be omitted.  But we leave it in because other` |
|         - |  524 | `  ** architectures might behave differently.` |
|         - |  525 | `  */` |
|     11444 |  526 | `	if( pObj->rVal ==(ph7_real)pObj->x.iVal && pObj->x.iVal>SMALLEST_INT64` |
|      9951 |  527 | `      && pObj->x.iVal<LARGEST_INT64 ){` |
|      9935 |  528 | `		  pObj->iFlags \|= MEMOBJ_INT;` |
|      4967 |  529 | `	}` |
|     11449 |  530 | `	return SXRET_OK;` |
|         5 |  531 | `}` |
|         - |  532 | `/*` |
|         - |  533 | ` * Convert a ph7_value to type integer.Invalidate any prior representations.` |
|         - |  534 | ` */` |
|    590339 |  535 | `PH7_PRIVATE sxi32 PH7_MemObjToInteger(ph7_value *pObj)` |
|         5 |  536 | `{` |
|    590344 |  537 | `	if( (pObj->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  538 | `		/* Preform the conversion */` |
|      1603 |  539 | `		pObj->x.iVal = MemObjIntValue(&(*pObj));` |
|         - |  540 | `		/* Invalidate any prior representations */` |
|      1603 |  541 | `		SyBlobRelease(&pObj->sBlob);` |
|      1603 |  542 | `		MemObjSetType(pObj,MEMOBJ_INT);` |
|       799 |  543 | `	}` |
|    590344 |  544 | `	return SXRET_OK;` |
|         5 |  545 | `}` |
|         - |  546 | `/*` |
|         - |  547 | ` * Convert a ph7_value to type real (Try to get an integer representation also).` |
|         - |  548 | ` * Invalidate any prior representations` |
|         - |  549 | ` */` |
|     11308 |  550 | `PH7_PRIVATE sxi32 PH7_MemObjToReal(ph7_value *pObj)` |
|         5 |  551 | `{` |
|     11313 |  552 | `	if((pObj->iFlags & MEMOBJ_REAL) == 0 ){` |
|         - |  553 | `		/* Preform the conversion */` |
|     10265 |  554 | `		pObj->rVal = MemObjRealValue(&(*pObj));` |
|         - |  555 | `		/* Invalidate any prior representations */` |
|     10265 |  556 | `		SyBlobRelease(&pObj->sBlob);` |
|     10265 |  557 | `		MemObjSetType(pObj,MEMOBJ_REAL);` |
|         - |  558 | `		/* Try to get an integer representation */` |
|     10265 |  559 | `		MemObjTryIntger(&(*pObj));` |
|      5130 |  560 | `	}` |
|     11313 |  561 | `	return SXRET_OK;` |
|         5 |  562 | `}` |
|         - |  563 | `/*` |
|         - |  564 | ` * Convert a ph7_value to type boolean.Invalidate any prior representations.` |
|         - |  565 | ` */` |
|     62038 |  566 | `PH7_PRIVATE sxi32 PH7_MemObjToBool(ph7_value *pObj)` |
|         5 |  567 | `{` |
|     62043 |  568 | `	if( (pObj->iFlags & MEMOBJ_BOOL) == 0 ){` |
|         - |  569 | `		/* Preform the conversion */` |
|     57375 |  570 | `		pObj->x.iVal = MemObjBooleanValue(&(*pObj));` |
|         - |  571 | `		/* Invalidate any prior representations */` |
|     57375 |  572 | `		SyBlobRelease(&pObj->sBlob);` |
|     57375 |  573 | `		MemObjSetType(pObj,MEMOBJ_BOOL);` |
|     28685 |  574 | `	}` |
|     62043 |  575 | `	return SXRET_OK;` |
|         5 |  576 | `}` |
|         - |  577 | `/*` |
|         - |  578 | ` * Convert a ph7_value to type string.Prior representations are NOT invalidated.` |
|         - |  579 | ` */` |
|   1194319 |  580 | `PH7_PRIVATE sxi32 PH7_MemObjToString(ph7_value *pObj)` |
|         5 |  581 | `{` |
|   1194324 |  582 | `	sxi32 rc = SXRET_OK;` |
|   1194324 |  583 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  584 | `		/* Perform the conversion */` |
|     63079 |  585 | `		SyBlobReset(&pObj->sBlob); /* Reset the internal buffer */` |
|     63079 |  586 | `		rc = MemObjStringValue(&pObj->sBlob,&(*pObj),TRUE);` |
|     63079 |  587 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|     31537 |  588 | `	}` |
|   1194324 |  589 | `	return rc;` |
|         5 |  590 | `}` |
|         - |  591 | `/*` |
|         - |  592 | ` * User-visible array->string coercion. php emits an E_WARNING` |
|         - |  593 | ` * "Array to string conversion" wherever an ARRAY is coerced to a string FOR` |
|         - |  594 | `` * THE USER -- echo/print, concatenation and `.=`, the (string) cast, string`` |
|         - |  595 | `` * interpolation "$arr", a variable-variable NAME `$$arr`, printf/sprintf %s,`` |
|         - |  596 | ` * implode(), and settype($x,'string') -- but it stays SILENT for the internal` |
|         - |  597 | ` * coercions that merely format a value for inspection or use it as a lookup` |
|         - |  598 | ` * key (print_r/var_export/serialize, array-key canonicalisation, sort` |
|         - |  599 | `` * comparisons, and the `ph7_value_to_string` embedder API). Those sites keep`` |
|         - |  600 | ` * the bare PH7_MemObjToString; the user-visible ones call this instead.` |
|         - |  601 | ` *` |
|         - |  602 | ` * Behaviour is otherwise identical to PH7_MemObjToString: a no-op when pObj is` |
|         - |  603 | ` * already a string, and OBJECTS are left to their own __toString /` |
|         - |  604 | ` * not-stringable path (only a MEMOBJ_HASHMAP warns here). The warning routes` |
|         - |  605 | ` * through pObj->pVm, which every VM-owned ph7_value carries.` |
|         - |  606 | ` */` |
|    491378 |  607 | `PH7_PRIVATE sxi32 PH7_MemObjToStringUV(ph7_value *pObj)` |
|         5 |  608 | `{` |
|    491383 |  609 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|    430235 |  610 | `		return SXRET_OK;` |
|         - |  611 | `	}` |
|     61153 |  612 | `	if( (pObj->iFlags & MEMOBJ_HASHMAP) && pObj->pVm ){` |
|        29 |  613 | `		PH7_VmThrowError(pObj->pVm,0,PH7_CTX_WARNING,"Array to string conversion");` |
|        14 |  614 | `	}` |
|     61153 |  615 | `	return PH7_MemObjToString(pObj);` |
|    245694 |  616 | `}` |
|         - |  617 | `/*` |
|         - |  618 | ` * Nullify a ph7_value.In other words invalidate any prior` |
|         - |  619 | ` * representation.` |
|         - |  620 | ` */` |
|         2 |  621 | `PH7_PRIVATE sxi32 PH7_MemObjToNull(ph7_value *pObj)` |
|         1 |  622 | `{` |
|         3 |  623 | `	return PH7_MemObjRelease(pObj);` |
|         1 |  624 | `}` |
|         - |  625 | `/*` |
|         - |  626 | ` * Convert a ph7_value to type array.Invalidate any prior representations.` |
|         - |  627 | `  * According to the PHP language reference manual.` |
|         - |  628 | `  *   For any of the types: integer, float, string, boolean converting a value` |
|         - |  629 | `  *   to an array results in an array with a single element with index zero` |
|         - |  630 | `  *   and the value of the scalar which was converted.` |
|         - |  631 | `  */` |
|       698 |  632 | `PH7_PRIVATE sxi32 PH7_MemObjToHashmap(ph7_value *pObj)` |
|         5 |  633 | `{` |
|       703 |  634 | `	if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - |  635 | `		ph7_hashmap *pMap;` |
|         - |  636 | `		/* Allocate a new hashmap instance */` |
|       497 |  637 | `		pMap = PH7_NewHashmap(pObj->pVm,0,0);` |
|       497 |  638 | `		if( pMap == 0 ){` |
|       ! 0 |  639 | `			return SXERR_MEM;` |
|         - |  640 | `		}` |
|       497 |  641 | `		if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|         - |  642 | `			/*` |
|         - |  643 | `			 * According to the PHP language reference manual.` |
|         - |  644 | `			 *   For any of the types: integer, float, string, boolean converting a value` |
|         - |  645 | `			 *   to an array results in an array with a single element with index zero` |
|         - |  646 | `			 *   and the value of the scalar which was converted.` |
|         - |  647 | `			 */` |
|        48 |  648 | `			if( pObj->iFlags & MEMOBJ_OBJ ){` |
|         - |  649 | `				/* Object cast */` |
|        32 |  650 | `				PH7_ClassInstanceToHashmap((ph7_class_instance *)pObj->x.pOther,pMap);` |
|        18 |  651 | `			}else{` |
|         - |  652 | `				/* Insert a single element */` |
|        18 |  653 | `				PH7_HashmapInsert(pMap,0/* Automatic index assign */,&(*pObj));` |
|         - |  654 | `			}` |
|        48 |  655 | `			SyBlobRelease(&pObj->sBlob);` |
|        22 |  656 | `		}` |
|         - |  657 | `		/* Invalidate any prior representation */` |
|       497 |  658 | `		PH7_MemObjRelease(pObj);` |
|       497 |  659 | `		MemObjSetType(pObj,MEMOBJ_HASHMAP);` |
|       497 |  660 | `		pObj->x.pOther = pMap;` |
|       246 |  661 | `	}` |
|       703 |  662 | `	return SXRET_OK;` |
|       354 |  663 | `}` |
|         - |  664 | `/* Per-entry callback for the array branch of the (object) cast: add one dynamic` |
|         - |  665 | ` * property to the target stdClass, named by the array key (rendered as a string,` |
|         - |  666 | ` * matching PHP) and holding a copy of the value. */` |
|         - |  667 | `struct VmObjCastData { ph7_vm *pVm; ph7_class_instance *pStd; };` |
|        66 |  668 | `static int VmArrayToObjectWalk(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|         3 |  669 | `{` |
|        69 |  670 | `	struct VmObjCastData *pData = (struct VmObjCastData *)pUserData;` |
|         - |  671 | `	ph7_value *pSlot;` |
|         - |  672 | `	/* pKey and pValue are walk-owned temporaries (PH7_HashmapWalk passes pointers to` |
|         - |  673 | `	 * its own stack-local sKey/sValue, not slots inside pVm->aMemObj), so they survive` |
|         - |  674 | `	 * the slot reservation inside PH7_VmCreateDynamicAttr — no snapshot needed. pKey is` |
|         - |  675 | `	 * safe to coerce in place. */` |
|        69 |  676 | `	PH7_MemObjToString(pKey);` |
|       102 |  677 | `	pSlot = PH7_VmCreateDynamicAttr(pData->pVm,pData->pStd,` |
|        66 |  678 | `		(const char *)SyBlobData(&pKey->sBlob),(sxu32)SyBlobLength(&pKey->sBlob),0);` |
|        69 |  679 | `	if( pSlot ){` |
|        69 |  680 | `		PH7_MemObjStore(pValue,pSlot);` |
|        33 |  681 | `	}` |
|        69 |  682 | `	return SXRET_OK;` |
|         3 |  683 | `}` |
|         - |  684 | `/*` |
|         - |  685 | ` * Convert a ph7_value to type object, invalidating any prior representation.` |
|         - |  686 | ` * The new object is a (PHP-empty) stdClass populated with dynamic properties,` |
|         - |  687 | ` * matching PHP's (object) cast:` |
|         - |  688 | ` *   - array  -> one property per entry (key rendered as a string -> name).` |
|         - |  689 | ` *   - scalar -> a single property named "scalar".` |
|         - |  690 | ` *   - null   -> an empty stdClass (no properties).` |
|         - |  691 | ` *   - object -> returned unchanged (the MEMOBJ_OBJ guard below).` |
|         - |  692 | ` */` |
|        52 |  693 | `PH7_PRIVATE sxi32 PH7_MemObjToObject(ph7_value *pObj)` |
|         3 |  694 | `{` |
|        55 |  695 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|         - |  696 | `		ph7_class_instance *pStd;` |
|         - |  697 | `		ph7_class *pClass;` |
|         - |  698 | `		ph7_vm *pVm;` |
|         - |  699 | `		/* Point to the underlying VM + the stdClass */` |
|        55 |  700 | `		pVm = pObj->pVm;` |
|        81 |  701 | `		pClass = pVm->pStdClass ? pVm->pStdClass` |
|        26 |  702 | `			: PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,0,0);` |
|        55 |  703 | `		if( pClass == 0 ){` |
|         - |  704 | `			/* Can't happen,load null instead */` |
|       ! 0 |  705 | `			PH7_MemObjRelease(pObj);` |
|       ! 0 |  706 | `			return SXRET_OK;` |
|         - |  707 | `		}` |
|         - |  708 | `		/* Instanciate a new (empty) stdClass object */` |
|        55 |  709 | `		pStd = PH7_NewClassInstance(pVm,pClass);` |
|        55 |  710 | `		if( pStd == 0 ){` |
|         - |  711 | `			/* Out of memory */` |
|       ! 0 |  712 | `			PH7_MemObjRelease(pObj);` |
|       ! 0 |  713 | `			return SXRET_OK;` |
|         - |  714 | `		}` |
|        55 |  715 | `		pStd->iRef = 1;` |
|        55 |  716 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         - |  717 | `			/* Array: one dynamic property per entry. */` |
|         - |  718 | `			struct VmObjCastData sData;` |
|        41 |  719 | `			sData.pVm = pVm;` |
|        41 |  720 | `			sData.pStd = pStd;` |
|        41 |  721 | `			ph7_array_walk(pObj,VmArrayToObjectWalk,&sData);` |
|        35 |  722 | `		}else if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|         - |  723 | `			/* Scalar (int/float/bool/string): a single "scalar" property. */` |
|        14 |  724 | `			ph7_value *pSlot = PH7_VmCreateDynamicAttr(pVm,pStd,"scalar",sizeof("scalar")-1,0);` |
|        14 |  725 | `			if( pSlot ){` |
|        14 |  726 | `				PH7_MemObjStore(pObj,pSlot);` |
|         6 |  727 | `			}` |
|         6 |  728 | `		}` |
|         - |  729 | `		/* (A NULL source yields an empty stdClass — nothing to populate.) */` |
|         - |  730 | `		/* Invalidate any prior representation */` |
|        55 |  731 | `		PH7_MemObjRelease(pObj);` |
|         - |  732 | `		/* Save the new instance */` |
|        55 |  733 | `		pObj->x.pOther = pStd;` |
|        55 |  734 | `		MemObjSetType(pObj,MEMOBJ_OBJ);` |
|        26 |  735 | `	}` |
|        55 |  736 | `	return SXRET_OK;` |
|        29 |  737 | `}` |
|         - |  738 | `/*` |
|         - |  739 | ` * Return a pointer to the appropriate convertion method associated` |
|         - |  740 | ` * with the given type.` |
|         - |  741 | ` * Note on type juggling.` |
|         - |  742 | ` * Accoding to the PHP language reference manual` |
|         - |  743 | ` *  PHP does not require (or support) explicit type definition in variable` |
|         - |  744 | ` *  declaration; a variable's type is determined by the context in which` |
|         - |  745 | ` *  the variable is used. That is to say, if a string value is assigned` |
|         - |  746 | ` *  to variable $var, $var becomes a string. If an integer value is then` |
|         - |  747 | ` *  assigned to $var, it becomes an integer.` |
|         - |  748 | ` */` |
|    100118 |  749 | `PH7_PRIVATE ProcMemObjCast PH7_MemObjCastMethod(sxi32 iFlags)` |
|         5 |  750 | `{` |
|    100123 |  751 | `	if( iFlags & MEMOBJ_STRING ){` |
|        28 |  752 | `		return PH7_MemObjToString;` |
|    100099 |  753 | `	}else if( iFlags & MEMOBJ_INT ){` |
|    100079 |  754 | `		return PH7_MemObjToInteger;` |
|        23 |  755 | `	}else if( iFlags & MEMOBJ_REAL ){` |
|        17 |  756 | `		return PH7_MemObjToReal;` |
|         8 |  757 | `	}else if( iFlags & MEMOBJ_BOOL ){` |
|       ! 0 |  758 | `		return PH7_MemObjToBool;` |
|         8 |  759 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|         8 |  760 | `		return PH7_MemObjToHashmap;` |
|       ! 0 |  761 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|       ! 0 |  762 | `		return PH7_MemObjToObject;` |
|       ! 0 |  763 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|         - |  764 | ``		/* `null` is a type, not a weak-coercion target: never silently cast a`` |
|         - |  765 | ``		 * value to null for a standalone `null` type hint. Return/property`` |
|         - |  766 | `		 * enforcement reject a non-null value before reaching here; this guards` |
|         - |  767 | `		 * the parameter default-value path from quietly nulling a non-null` |
|         - |  768 | `		 * default. */` |
|       ! 0 |  769 | `		return 0;` |
|         - |  770 | `	}` |
|         - |  771 | `	/* NULL cast */` |
|       ! 0 |  772 | `	return PH7_MemObjToNull;` |
|     50064 |  773 | `}` |
|         - |  774 | `/*` |
|         - |  775 | ` * Return TRUE only if the entire string held by pValue (optionally surrounded` |
|         - |  776 | ` * by whitespace, with an optional sign) is a well-formed PHP numeric string.` |
|         - |  777 | ` * This mirrors PHP's is_numeric_string grammar used for is_numeric() and the` |
|         - |  778 | ` * loose-comparison numeric gate:` |
|         - |  779 | ` *` |
|         - |  780 | ` *   [ws] [sign] ( D+ [.D*] \| .D+ ) [ (e\|E) [sign] D+ ] [ws]   (whole string)` |
|         - |  781 | ` *` |
|         - |  782 | ` * Implemented directly rather than via SyStrIsNumeric — which returns OK on any` |
|         - |  783 | ` * numeric PREFIX (so it wrongly accepts "10abc"/"0x1A"/"0b101") and requires a` |
|         - |  784 | ` * leading digit (so it wrongly rejects ".5"/"-.5", valid in PHP). Unlike a` |
|         - |  785 | ` * strtod-based classifier this needs no NUL-terminated buffer. Returns FALSE for` |
|         - |  786 | ` * a non-string value.` |
|         - |  787 | ` */` |
|         - |  788 | `/*` |
|         - |  789 | ` * Scan php's numeric-string grammar and report the longest numeric PREFIX.` |
|         - |  790 | ` * Returns 1 when the string starts with a number, 0 when nothing numeric is` |
|         - |  791 | ` * there at all ("abc", "", ".", "e5"). On success *pzTail points just past the` |
|         - |  792 | ` * prefix, so the caller can tell a fully numeric string ("1e3", " 5 ") from a` |
|         - |  793 | ` * merely leading-numeric one ("5abc", "1e", "0x1A") -- php warns on the latter` |
|         - |  794 | ` * and rejects a string with no prefix outright.` |
|         - |  795 | ` */` |
|    367023 |  796 | `PH7_PRIVATE int PH7_MemObjStringNumericPrefix(ph7_value *pValue,const char **pzTail)` |
|         5 |  797 | `{` |
|         - |  798 | `	const char *z, *zEnd;` |
|         - |  799 | `	sxu32 n;` |
|    367028 |  800 | `	int bDigit = 0;` |
|    367028 |  801 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 |  802 | `		return 0;` |
|         - |  803 | `	}` |
|    367028 |  804 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|    367028 |  805 | `	n = SyBlobLength(&pValue->sBlob);` |
|    367028 |  806 | `	if( n == 0 ){` |
|       606 |  807 | `		return 0;` |
|         - |  808 | `	}` |
|    366424 |  809 | `	zEnd = z + n;` |
|    366512 |  810 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){` |
|        90 |  811 | `		z++;` |
|         2 |  812 | `	}` |
|    366424 |  813 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|       215 |  814 | `		z++;` |
|       105 |  815 | `	}` |
|    371299 |  816 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|      4880 |  817 | `		z++; bDigit = 1;` |
|         5 |  818 | `	}` |
|    366424 |  819 | `	if( z < zEnd && z[0] == '.' ){` |
|      6238 |  820 | `		z++;` |
|      6312 |  821 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|        79 |  822 | `			z++; bDigit = 1;` |
|         5 |  823 | `		}` |
|      3333 |  824 | `	}` |
|         - |  825 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|    366424 |  826 | `	if( !bDigit ){` |
|    361657 |  827 | `		return 0;` |
|         - |  828 | `	}` |
|         - |  829 | `	/* Optional exponent — only joins the prefix if it carries a digit. "1e" has` |
|         - |  830 | `	 * the numeric prefix "1" with the 'e' left in the tail, exactly as php reads it. */` |
|      4772 |  831 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|        30 |  832 | `		const char *zExp = z;` |
|        30 |  833 | `		z++;` |
|        30 |  834 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|       ! 0 |  835 | `			z++;` |
|       ! 0 |  836 | `		}` |
|        30 |  837 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|        10 |  838 | `			z = zExp;` |
|         6 |  839 | `		}else{` |
|        46 |  840 | `			while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|        26 |  841 | `				z++;` |
|         2 |  842 | `			}` |
|         - |  843 | `		}` |
|        14 |  844 | `	}` |
|      4772 |  845 | `	if( pzTail ){` |
|      4772 |  846 | `		*pzTail = z;` |
|      2383 |  847 | `	}` |
|      4772 |  848 | `	return 1;` |
|    183443 |  849 | `}` |
|         - |  850 | `/*` |
|         - |  851 | ` * TRUE only if the WHOLE string is a well-formed php numeric string` |
|         - |  852 | ` * (trailing whitespace allowed, nothing else).` |
|         - |  853 | ` */` |
|    364606 |  854 | `PH7_PRIVATE int PH7_MemObjStringIsNumeric(ph7_value *pValue)` |
|         5 |  855 | `{` |
|    364611 |  856 | `	const char *zTail = 0, *zEnd;` |
|    364611 |  857 | `	if( !PH7_MemObjStringNumericPrefix(pValue,&zTail) ){` |
|    362251 |  858 | `		return 0;` |
|         - |  859 | `	}` |
|      2365 |  860 | `	zEnd = (const char *)SyBlobData(&pValue->sBlob) + SyBlobLength(&pValue->sBlob);` |
|      2371 |  861 | `	while( zTail < zEnd && (unsigned char)zTail[0] < 0xc0 && SyisSpace(zTail[0]) ){` |
|         8 |  862 | `		zTail++;` |
|         2 |  863 | `	}` |
|      2365 |  864 | `	return zTail == zEnd ? 1 : 0;` |
|    182235 |  865 | `}` |
|         - |  866 | `/*` |
|         - |  867 | ` * Check whether the ph7_value is numeric [i.e: int/float/bool] or looks` |
|         - |  868 | ` * like a numeric number [i.e: if the ph7_value is of type string.].` |
|         - |  869 | ` * Return TRUE if numeric.FALSE otherwise.` |
|         - |  870 | ` */` |
|    265622 |  871 | `PH7_PRIVATE sxi32 PH7_MemObjIsNumeric(ph7_value *pObj)` |
|         5 |  872 | `{` |
|    265627 |  873 | `	if( pObj->iFlags & ( MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|       707 |  874 | `		return TRUE;` |
|    264925 |  875 | `	}else if( pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|       491 |  876 | `		return FALSE;` |
|    264439 |  877 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|         - |  878 | `		/* TRUE only if the whole string is a well-formed PHP numeric string. */` |
|    264439 |  879 | `		return PH7_MemObjStringIsNumeric(pObj) ? TRUE : FALSE;` |
|         - |  880 | `	}` |
|         - |  881 | `	/* NOT REACHED */` |
|       ! 0 |  882 | `	return FALSE;` |
|    132743 |  883 | `}` |
|         - |  884 | `/*` |
|         - |  885 | ` * Check whether the ph7_value is empty.Return TRUE if empty.` |
|         - |  886 | ` * FALSE otherwise.` |
|         - |  887 | ` * An ph7_value is considered empty if the following are true:` |
|         - |  888 | ` * NULL value.` |
|         - |  889 | ` * Boolean FALSE.` |
|         - |  890 | ` * Integer/Float with a 0 (zero) value.` |
|         - |  891 | ` * An empty string or a stream of 0 (zero) [i.e: "0","00","000",...].` |
|         - |  892 | ` * An empty array.` |
|         - |  893 | ` * NOTE` |
|         - |  894 | ` *  OBJECT VALUE MUST NOT BE MODIFIED.` |
|         - |  895 | ` */` |
|     41092 |  896 | `PH7_PRIVATE sxi32 PH7_MemObjIsEmpty(ph7_value *pObj)` |
|         5 |  897 | `{` |
|     41097 |  898 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|        24 |  899 | `		return TRUE;` |
|     41077 |  900 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|        22 |  901 | `		return pObj->x.iVal == 0 ? TRUE : FALSE;` |
|     41057 |  902 | `	}else if( pObj->iFlags & MEMOBJ_REAL ){` |
|       ! 0 |  903 | `		return pObj->rVal == (ph7_real)0 ? TRUE : FALSE;` |
|     41057 |  904 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|         5 |  905 | `		return !pObj->x.iVal;` |
|     41053 |  906 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     27329 |  907 | `		if( SyBlobLength(&pObj->sBlob) <= 0 ){` |
|     21765 |  908 | `			return TRUE;` |
|       ! 0 |  909 | `		}else{` |
|         - |  910 | `			const char *zIn,*zEnd;` |
|      5569 |  911 | `			zIn = (const char *)SyBlobData(&pObj->sBlob);` |
|      5569 |  912 | `			zEnd = &zIn[SyBlobLength(&pObj->sBlob)];` |
|      5575 |  913 | `			while( zIn < zEnd ){` |
|      5575 |  914 | `				if( zIn[0] != '0' ){` |
|      5569 |  915 | `					break;` |
|         - |  916 | `				}` |
|         7 |  917 | `				zIn++;` |
|         1 |  918 | `			}` |
|      5569 |  919 | `			return zIn >= zEnd ? TRUE : FALSE;` |
|       ! 0 |  920 | `		}` |
|     13729 |  921 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|     13729 |  922 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|     13729 |  923 | `		return pMap->nEntry == 0 ? TRUE : FALSE;` |
|       ! 0 |  924 | `	}else if ( pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|       ! 0 |  925 | `		return FALSE;` |
|         - |  926 | `	}` |
|         - |  927 | `	/* Assume empty by default */` |
|       ! 0 |  928 | `	return TRUE;` |
|     20551 |  929 | `}` |
|         - |  930 | `/*` |
|         - |  931 | ` * Convert a ph7_value so that it has types MEMOBJ_REAL or MEMOBJ_INT` |
|         - |  932 | ` * or both.` |
|         - |  933 | ` * Invalidate any prior representations. Every effort is made to force` |
|         - |  934 | ` * the conversion, even if the input is a string that does not look` |
|         - |  935 | ` * completely like a number.Convert as much of the string as we can` |
|         - |  936 | ` * and ignore the rest.` |
|         - |  937 | ` */` |
|    723966 |  938 | `PH7_PRIVATE sxi32 PH7_MemObjToNumeric(ph7_value *pObj)` |
|         5 |  939 | `{` |
|    723971 |  940 | `	if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|    721602 |  941 | `		if( pObj->iFlags & (MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|        14 |  942 | `			if( pObj->iFlags & MEMOBJ_NULL ){` |
|        11 |  943 | `				pObj->x.iVal = 0;` |
|         5 |  944 | `			}` |
|        14 |  945 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|         6 |  946 | `		}` |
|         - |  947 | `		/* Already numeric */` |
|    721602 |  948 | `		return  SXRET_OK;` |
|         - |  949 | `	}` |
|      2373 |  950 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|      2373 |  951 | `		const char *zTail = 0;` |
|      2373 |  952 | `		int bNum, bReal = 0;` |
|         - |  953 | `		/* php reads the longest numeric PREFIX and its shape decides the type: a` |
|         - |  954 | `		 * '.' or a *complete* exponent inside that prefix makes it a float, else an` |
|         - |  955 | `		 * int. Deciding from the raw string instead mistyped "1e" as float(1) --` |
|         - |  956 | `		 * php sees the prefix "1" there and yields int(1). */` |
|      2373 |  957 | `		bNum = PH7_MemObjStringNumericPrefix(pObj,&zTail);` |
|      2373 |  958 | `		if( bNum ){` |
|      2373 |  959 | `			const char *z = (const char *)SyBlobData(&pObj->sBlob);` |
|      4828 |  960 | `			while( z < zTail ){` |
|      2479 |  961 | `				if( z[0] == '.' \|\| z[0] == 'e' \|\| z[0] == 'E' ){` |
|        22 |  962 | `					bReal = 1;` |
|        22 |  963 | `					break;` |
|         - |  964 | `				}` |
|      2459 |  965 | `				z++;` |
|         4 |  966 | `			}` |
|      1184 |  967 | `		}` |
|      2373 |  968 | `		if( bReal ){` |
|        22 |  969 | `			PH7_MemObjToReal(&(*pObj));` |
|        12 |  970 | `		}else{` |
|      2353 |  971 | `			if( !bNum ){` |
|         - |  972 | `				/* The input does not look at all like a number,set the value to 0 */` |
|       ! 0 |  973 | `				pObj->x.iVal = 0;` |
|       ! 0 |  974 | `			}else{` |
|         - |  975 | `				/* Convert as much as we can */` |
|      2353 |  976 | `				pObj->x.iVal = MemObjStringToInt(&(*pObj));` |
|         - |  977 | `			}` |
|      2353 |  978 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|      2353 |  979 | `			SyBlobRelease(&pObj->sBlob);` |
|         4 |  980 | `		}` |
|      1184 |  981 | `	}else if(pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)){` |
|       ! 0 |  982 | `		PH7_MemObjToInteger(pObj);` |
|       ! 0 |  983 | `	}else{` |
|         - |  984 | `		/* Perform a blind cast */` |
|       ! 0 |  985 | `		PH7_MemObjToReal(&(*pObj));` |
|         - |  986 | `	}` |
|      2373 |  987 | `	return SXRET_OK;` |
|    362388 |  988 | `}` |
|         - |  989 | `/*` |
|         - |  990 | ` * Apply Perl-style increment to a string ph7_value in place.` |
|         - |  991 | ` * Walks the bytes right-to-left: digits 0-8 / letters a-y, A-Y bump in` |
|         - |  992 | ` * place; '9' wraps to '0' with carry; 'z' to 'a'; 'Z' to 'A'. A non-` |
|         - |  993 | ` * alphanumeric byte stops the walk without prepending. If carry survives` |
|         - |  994 | ` * past index 0, prepend '1', 'a', or 'A' depending on the class of the` |
|         - |  995 | ` * last carried character. Empty strings become "1".` |
|         - |  996 | ` *` |
|         - |  997 | ` * Caller must ensure pObj is MEMOBJ_STRING and not a numeric string;` |
|         - |  998 | ` * this routine never reclassifies the type, so a result like "e0" stays` |
|         - |  999 | ` * a string even though it looks numeric.` |
|         - | 1000 | ` */` |
|       ! 0 | 1001 | `PH7_PRIVATE sxi32 PH7_MemObjStringIncrement(ph7_value *pObj)` |
|       ! 0 | 1002 | `{` |
|         - | 1003 | `	enum CarryClass { CARRY_NONE = 0, CARRY_LOWER, CARRY_UPPER, CARRY_DIGIT };` |
|       ! 0 | 1004 | `	enum CarryClass last_class = CARRY_NONE;` |
|         - | 1005 | `	sxu32 nLen, pos;` |
|         - | 1006 | `	sxu8 *zStr;` |
|       ! 0 | 1007 | `	int carry = 1;` |
|         - | 1008 | `	int ch;` |
|         - | 1009 | `	/* Force ownership: the blob may be SXBLOB_RDONLY (e.g., from` |
|         - | 1010 | `	 * PH7_MemObjLoad), in which case BlobPrepareGrow copies on demand` |
|         - | 1011 | `	 * and clears the flag.  On an already-owned blob with spare capacity` |
|         - | 1012 | `	 * (the common case under PHL's growth allocator), this is a no-op` |
|         - | 1013 | `	 * append; on an exact-fit owned blob it triggers a single realloc. */` |
|       ! 0 | 1014 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 1015 | `		SyBlobNullAppend(&pObj->sBlob);` |
|       ! 0 | 1016 | `	}` |
|       ! 0 | 1017 | `	nLen = SyBlobLength(&pObj->sBlob);` |
|       ! 0 | 1018 | `	if( nLen == 0 ){` |
|       ! 0 | 1019 | `		SyBlobAppend(&pObj->sBlob,"1",sizeof(char));` |
|       ! 0 | 1020 | `		return SXRET_OK;` |
|         - | 1021 | `	}` |
|       ! 0 | 1022 | `	zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|       ! 0 | 1023 | `	pos = nLen;` |
|       ! 0 | 1024 | `	while( pos > 0 ){` |
|       ! 0 | 1025 | `		pos--;` |
|       ! 0 | 1026 | `		ch = zStr[pos];` |
|       ! 0 | 1027 | `		if( ch >= 'a' && ch <= 'z' ){` |
|       ! 0 | 1028 | `			if( ch == 'z' ){` |
|       ! 0 | 1029 | `				zStr[pos] = 'a';` |
|       ! 0 | 1030 | `				last_class = CARRY_LOWER;` |
|       ! 0 | 1031 | `				continue;` |
|         - | 1032 | `			}` |
|       ! 0 | 1033 | `			zStr[pos]++;` |
|       ! 0 | 1034 | `			carry = 0;` |
|       ! 0 | 1035 | `			break;` |
|       ! 0 | 1036 | `		}else if( ch >= 'A' && ch <= 'Z' ){` |
|       ! 0 | 1037 | `			if( ch == 'Z' ){` |
|       ! 0 | 1038 | `				zStr[pos] = 'A';` |
|       ! 0 | 1039 | `				last_class = CARRY_UPPER;` |
|       ! 0 | 1040 | `				continue;` |
|         - | 1041 | `			}` |
|       ! 0 | 1042 | `			zStr[pos]++;` |
|       ! 0 | 1043 | `			carry = 0;` |
|       ! 0 | 1044 | `			break;` |
|       ! 0 | 1045 | `		}else if( ch >= '0' && ch <= '9' ){` |
|       ! 0 | 1046 | `			if( ch == '9' ){` |
|       ! 0 | 1047 | `				zStr[pos] = '0';` |
|       ! 0 | 1048 | `				last_class = CARRY_DIGIT;` |
|       ! 0 | 1049 | `				continue;` |
|         - | 1050 | `			}` |
|       ! 0 | 1051 | `			zStr[pos]++;` |
|       ! 0 | 1052 | `			carry = 0;` |
|       ! 0 | 1053 | `			break;` |
|       ! 0 | 1054 | `		}else{` |
|         - | 1055 | `			/* non-alphanumeric: stop without prepending */` |
|       ! 0 | 1056 | `			carry = 0;` |
|       ! 0 | 1057 | `			break;` |
|         - | 1058 | `		}` |
|       ! 0 | 1059 | `	}` |
|       ! 0 | 1060 | `	if( carry ){` |
|         - | 1061 | `		sxu8 prepend;` |
|         - | 1062 | `		sxu32 i;` |
|       ! 0 | 1063 | `		switch( last_class ){` |
|       ! 0 | 1064 | `			case CARRY_LOWER: prepend = (sxu8)'a'; break;` |
|       ! 0 | 1065 | `			case CARRY_UPPER: prepend = (sxu8)'A'; break;` |
|       ! 0 | 1066 | `			default:          prepend = (sxu8)'1'; break;` |
|         - | 1067 | `		}` |
|         - | 1068 | `		/* Append a sentinel byte to grow nByte by 1 (capacity grows too). */` |
|       ! 0 | 1069 | `		SyBlobAppend(&pObj->sBlob,"\0",sizeof(char));` |
|       ! 0 | 1070 | `		zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|       ! 0 | 1071 | `		nLen = SyBlobLength(&pObj->sBlob);` |
|         - | 1072 | `		/* Shift right by 1, walking from the end so overlapping is safe. */` |
|       ! 0 | 1073 | `		for( i = nLen - 1; i > 0; i-- ){` |
|       ! 0 | 1074 | `			zStr[i] = zStr[i - 1];` |
|       ! 0 | 1075 | `		}` |
|       ! 0 | 1076 | `		zStr[0] = prepend;` |
|       ! 0 | 1077 | `	}` |
|       ! 0 | 1078 | `	return SXRET_OK;` |
|       ! 0 | 1079 | `}` |
|         - | 1080 | `/*` |
|         - | 1081 | ` * Try a get an integer representation of the given ph7_value.` |
|         - | 1082 | ` * If the ph7_value is not of type real,this function is a no-op.` |
|         - | 1083 | ` */` |
|      1114 | 1084 | `PH7_PRIVATE sxi32 PH7_MemObjTryInteger(ph7_value *pObj)` |
|         4 | 1085 | `{` |
|      1118 | 1086 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         - | 1087 | `		/* Work only with reals */` |
|      1118 | 1088 | `		MemObjTryIntger(&(*pObj));` |
|       557 | 1089 | `	}` |
|      1118 | 1090 | `	return SXRET_OK;` |
|         4 | 1091 | `}` |
|         - | 1092 | `/*` |
|         - | 1093 | ` * Initialize a ph7_value to the null type.` |
|         - | 1094 | ` */` |
| 265525718 | 1095 | `PH7_PRIVATE sxi32 PH7_MemObjInit(ph7_vm *pVm,ph7_value *pObj)` |
|         5 | 1096 | `{` |
|         - | 1097 | `	/* Zero the structure */` |
| 265525723 | 1098 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1099 | `	/* Initialize fields */` |
| 265525723 | 1100 | `	pObj->pVm = pVm;` |
| 265525723 | 1101 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|         - | 1102 | `	/* Set the NULL type */` |
| 265525723 | 1103 | `	pObj->iFlags = MEMOBJ_NULL;` |
| 265525723 | 1104 | `	return SXRET_OK;` |
|         5 | 1105 | `}` |
|         - | 1106 | `/*` |
|         - | 1107 | ` * Initialize a ph7_value to the integer type.` |
|         - | 1108 | ` */` |
|   7705176 | 1109 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromInt(ph7_vm *pVm,ph7_value *pObj,sxi64 iVal)` |
|         5 | 1110 | `{` |
|         - | 1111 | `	/* Zero the structure */` |
|   7705181 | 1112 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1113 | `	/* Initialize fields */` |
|   7705181 | 1114 | `	pObj->pVm = pVm;` |
|   7705181 | 1115 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|         - | 1116 | `	/* Set the desired type */` |
|   7705181 | 1117 | `	pObj->x.iVal = iVal;` |
|   7705181 | 1118 | `	pObj->iFlags = MEMOBJ_INT;` |
|   7705181 | 1119 | `	return SXRET_OK;` |
|         5 | 1120 | `}` |
|         - | 1121 | `/*` |
|         - | 1122 | ` * Initialize a ph7_value to the boolean type.` |
|         - | 1123 | ` */` |
|     17490 | 1124 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromBool(ph7_vm *pVm,ph7_value *pObj,sxi32 iVal)` |
|         5 | 1125 | `{` |
|         - | 1126 | `	/* Zero the structure */` |
|     17495 | 1127 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1128 | `	/* Initialize fields */` |
|     17495 | 1129 | `	pObj->pVm = pVm;` |
|     17495 | 1130 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|         - | 1131 | `	/* Set the desired type */` |
|     17495 | 1132 | `	pObj->x.iVal = iVal ? 1 : 0;` |
|     17495 | 1133 | `	pObj->iFlags = MEMOBJ_BOOL;` |
|     17495 | 1134 | `	return SXRET_OK;` |
|         5 | 1135 | `}` |
|         - | 1136 | `/*` |
|         - | 1137 | ` * Initialize a ph7_value to the real type.` |
|         - | 1138 | ` */` |
|        10 | 1139 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromReal(ph7_vm *pVm,ph7_value *pObj,ph7_real rVal)` |
|         1 | 1140 | `{` |
|         - | 1141 | `	/* Zero the structure */` |
|        11 | 1142 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1143 | `	/* Initialize fields */` |
|        11 | 1144 | `	pObj->pVm = pVm;` |
|        11 | 1145 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|         - | 1146 | `	/* Set the desired type */` |
|        11 | 1147 | `	pObj->rVal = rVal;` |
|        11 | 1148 | `	pObj->iFlags = MEMOBJ_REAL;` |
|        11 | 1149 | `	return SXRET_OK;` |
|         1 | 1150 | `}` |
|         - | 1151 | `/*` |
|         - | 1152 | ` * Initialize a ph7_value to the array type.` |
|         - | 1153 | ` */` |
|   2132896 | 1154 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromArray(ph7_vm *pVm,ph7_value *pObj,ph7_hashmap *pArray)` |
|         5 | 1155 | `{` |
|         - | 1156 | `	/* Zero the structure */` |
|   2132901 | 1157 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1158 | `	/* Initialize fields */` |
|   2132901 | 1159 | `	pObj->pVm = pVm;` |
|   2132901 | 1160 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|         - | 1161 | `	/* Set the desired type */` |
|   2132901 | 1162 | `	pObj->iFlags = MEMOBJ_HASHMAP;` |
|   2132901 | 1163 | `	pObj->x.pOther = pArray;` |
|   2132901 | 1164 | `	return SXRET_OK;` |
|         5 | 1165 | `}` |
|         - | 1166 | `/*` |
|         - | 1167 | ` * Initialize a ph7_value to the string type.` |
|         - | 1168 | ` */` |
|  10034304 | 1169 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromString(ph7_vm *pVm,ph7_value *pObj,const SyString *pVal)` |
|         5 | 1170 | `{` |
|         - | 1171 | `	/* Zero the structure */` |
|  10034309 | 1172 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1173 | `	/* Initialize fields */` |
|  10034309 | 1174 | `	pObj->pVm = pVm;` |
|  10034309 | 1175 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|  10034309 | 1176 | `	if( pVal ){` |
|         - | 1177 | `		/* Append contents */` |
|   4552907 | 1178 | `		SyBlobAppend(&pObj->sBlob,(const void *)pVal->zString,pVal->nByte);` |
|   2276451 | 1179 | `	}` |
|         - | 1180 | `	/* Set the desired type */` |
|  10034309 | 1181 | `	pObj->iFlags = MEMOBJ_STRING;` |
|  10034309 | 1182 | `	return SXRET_OK;` |
|         5 | 1183 | `}` |
|         - | 1184 | `/*` |
|         - | 1185 | ` * Append some contents to the internal buffer of a given ph7_value.` |
|         - | 1186 | ` * If the given ph7_value is not of type string,this function` |
|         - | 1187 | ` * invalidate any prior representation and set the string type.` |
|         - | 1188 | ` * Then a simple append operation is performed.` |
|         - | 1189 | ` */` |
|   6129116 | 1190 | `PH7_PRIVATE sxi32 PH7_MemObjStringAppend(ph7_value *pObj,const char *zData,sxu32 nLen)` |
|         5 | 1191 | `{` |
|         - | 1192 | `	sxi32 rc;` |
|   6129121 | 1193 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - | 1194 | `		/* Invalidate any prior representation */` |
|    204893 | 1195 | `		PH7_MemObjRelease(pObj);` |
|    204893 | 1196 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|    102444 | 1197 | `	}` |
|         - | 1198 | `	/* Append contents */` |
|   6129121 | 1199 | `	rc = SyBlobAppend(&pObj->sBlob,zData,nLen);` |
|   6129121 | 1200 | `	return rc;` |
|         5 | 1201 | `}` |
|         - | 1202 | `#if 0` |
|         - | 1203 | `/*` |
|         - | 1204 | ` * Format and append some contents to the internal buffer of a given ph7_value.` |
|         - | 1205 | ` * If the given ph7_value is not of type string,this function invalidate` |
|         - | 1206 | ` * any prior representation and set the string type.` |
|         - | 1207 | ` * Then a simple format and append operation is performed.` |
|         - | 1208 | ` */` |
|         - | 1209 | `PH7_PRIVATE sxi32 PH7_MemObjStringFormat(ph7_value *pObj,const char *zFormat,va_list ap)` |
|         - | 1210 | `{` |
|         - | 1211 | `	sxi32 rc;` |
|         - | 1212 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - | 1213 | `		/* Invalidate any prior representation */` |
|         - | 1214 | `		PH7_MemObjRelease(pObj);` |
|         - | 1215 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|         - | 1216 | `	}` |
|         - | 1217 | `	/* Format and append contents */` |
|         - | 1218 | `	rc = SyBlobFormatAp(&pObj->sBlob,zFormat,ap);` |
|         - | 1219 | `	return rc;` |
|         - | 1220 | `}` |
|         - | 1221 | `#endif` |
|         - | 1222 | `/*` |
|         - | 1223 | ` * Duplicate the contents of a ph7_value.` |
|         - | 1224 | ` */` |
|  27650049 | 1225 | `PH7_PRIVATE sxi32 PH7_MemObjStore(ph7_value *pSrc,ph7_value *pDest)` |
|         5 | 1226 | `{` |
|  27650054 | 1227 | `	ph7_class_instance *pObj = 0;` |
|  27650054 | 1228 | `	ph7_hashmap *pMap = 0;` |
|         - | 1229 | `	sxi32 rc;` |
|  27650054 | 1230 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|         - | 1231 | `		/* Increment reference count */` |
|   2294459 | 1232 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  26502827 | 1233 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|         - | 1234 | `		/* Increment reference count */` |
|     13165 | 1235 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|      6580 | 1236 | `	}` |
|  27650054 | 1237 | `	if( pDest->iFlags & MEMOBJ_HASHMAP ){` |
|     84971 | 1238 | `		pMap = (ph7_hashmap *)pDest->x.pOther;` |
|  27607571 | 1239 | `	}else if( pDest->iFlags & MEMOBJ_OBJ ){` |
|      9113 | 1240 | `		pObj = (ph7_class_instance *)pDest->x.pOther;` |
|      4554 | 1241 | `	}` |
|  27650054 | 1242 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|  27650054 | 1243 | `	pDest->iFlags &= ~MEMOBJ_AUX;` |
|  27650054 | 1244 | `	rc = SXRET_OK;` |
|  27650054 | 1245 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|   9804672 | 1246 | `		SyBlobReset(&pDest->sBlob);` |
|   9804672 | 1247 | `		rc = SyBlobDup(&pSrc->sBlob,&pDest->sBlob);` |
|   4903056 | 1248 | `	}else{` |
|  17845387 | 1249 | `		if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|   1812055 | 1250 | `			SyBlobRelease(&pDest->sBlob);` |
|    906465 | 1251 | `		}` |
|         - | 1252 | `	}` |
|  27650054 | 1253 | `	if( pMap ){` |
|     84971 | 1254 | `		PH7_HashmapUnref(pMap);` |
|  27607571 | 1255 | `	}else if( pObj ){` |
|      9113 | 1256 | `		PH7_ClassInstanceUnref(pObj);` |
|      4554 | 1257 | `	}` |
|  27650049 | 1258 | `	if( rc == SXRET_OK && (pDest->iFlags & MEMOBJ_HASHMAP)` |
|  14974451 | 1259 | `	 && pDest->pVm` |
|   2294454 | 1260 | `	 && (ph7_hashmap *)pDest->x.pOther == pDest->pVm->pGlobal` |
|         - | 1261 | `	 /* Identity, not nIdx: transient values carry nIdx==0 (SyZero), which` |
|         - | 1262 | `	  * collides with a typical nGlobalIdx of 0 and would skip the snapshot` |
|         - | 1263 | `	  * for closure envs and other non-slot destinations. */` |
|   1147236 | 1264 | `	 && pDest != (ph7_value *)SySetAt(&pDest->pVm->aMemObj,pDest->pVm->nGlobalIdx) ){` |
|         - | 1265 | `		/* php 8.1: a COPY of $GLOBALS ($snap = $GLOBALS, $a[] = $GLOBALS,` |
|         - | 1266 | `		 * by-value argument passing, return $GLOBALS, ...) is a by-value` |
|         - | 1267 | `		 * SNAPSHOT of the symbol table with its reference entries` |
|         - | 1268 | `		 * flattened — never a live alias. Materialize it here, the one` |
|         - | 1269 | `		 * store choke point (loads/subscript access keep sharing, so` |
|         - | 1270 | `		 * $GLOBALS[$k] reads and writes stay live). */` |
|         9 | 1271 | `		ph7_hashmap *pSnap = PH7_NewHashmap(pDest->pVm,0,0);` |
|         9 | 1272 | `		if( pSnap && PH7_HashmapDupMaterialized((ph7_hashmap *)pDest->x.pOther,pSnap) == SXRET_OK ){` |
|         9 | 1273 | `			PH7_HashmapUnref((ph7_hashmap *)pDest->x.pOther);` |
|         9 | 1274 | `			pDest->x.pOther = pSnap;` |
|         4 | 1275 | `		}else if( pSnap ){` |
|       ! 0 | 1276 | `			PH7_HashmapUnref(pSnap);` |
|       ! 0 | 1277 | `		}` |
|         4 | 1278 | `	}` |
|  27650054 | 1279 | `	return rc;` |
|         5 | 1280 | `}` |
|         - | 1281 | `/*` |
|         - | 1282 | ` * Duplicate the contents of a ph7_value but do not copy internal` |
|         - | 1283 | ` * buffer contents,simply point to it.` |
|         - | 1284 | ` */` |
|  38755656 | 1285 | `PH7_PRIVATE sxi32 PH7_MemObjLoad(ph7_value *pSrc,ph7_value *pDest)` |
|         5 | 1286 | `{` |
|  38755661 | 1287 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),` |
|         - | 1288 | `		sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|         - | 1289 | `	/* D1 commit 2: a MEMOBJ_AUX_DEFPATH carrier OWNS its heap descriptor via x.pOther, and` |
|         - | 1290 | `	 * PH7_MemObjRelease frees it exactly once. An aliasing Load copies iFlags+x.pOther` |
|         - | 1291 | `	 * verbatim, so a Load-duplicated carrier would let two slots free the same descriptor.` |
|         - | 1292 | `	 * Carriers are transient (produced by LOAD_IDX/MEMBER, consumed at OP_CALL) and are never` |
|         - | 1293 | `	 * Load-copied today; strip the flag defensively so the invariant can't be violated. */` |
|  38755661 | 1294 | `	pDest->iFlags &= ~MEMOBJ_AUX_DEFPATH;` |
|  38755661 | 1295 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|         - | 1296 | `		/* Increment reference count */` |
|    686307 | 1297 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  38412510 | 1298 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|         - | 1299 | `		/* Increment reference count */` |
|   3348161 | 1300 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|   1674078 | 1301 | `	}` |
|  38755661 | 1302 | `	if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|        73 | 1303 | `		SyBlobRelease(&pDest->sBlob);` |
|        34 | 1304 | `	}` |
|  38755661 | 1305 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  17795049 | 1306 | `		SyBlobReadOnly(&pDest->sBlob,SyBlobData(&pSrc->sBlob),SyBlobLength(&pSrc->sBlob));` |
|   8899802 | 1307 | `	}` |
|  38755661 | 1308 | `	return SXRET_OK;` |
|         5 | 1309 | `}` |
|         - | 1310 | `/*` |
|         - | 1311 | ` * Invalidate any prior representation of a given ph7_value.` |
|         - | 1312 | ` */` |
| 157877059 | 1313 | `PH7_PRIVATE sxi32 PH7_MemObjRelease(ph7_value *pObj)` |
|         5 | 1314 | `{` |
| 157877064 | 1315 | `	if( pObj->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|         - | 1316 | `		/* D1 commit 2: a deferred element/property lvalue carrier OWNS a heap VmDeferredPath` |
|         - | 1317 | `		 * on a NULL-typed slot. Free it HERE, before the MEMOBJ_NULL short-circuit below —` |
|         - | 1318 | `		 * this is the universal release site every pop / abort / exception-unwind path routes` |
|         - | 1319 | `		 * through, so the descriptor never leaks even when OP_CALL never consumes it. */` |
|       ! 0 | 1320 | `		VmFreeDeferredPath((VmDeferredPath *)pObj->x.pOther);` |
|       ! 0 | 1321 | `		pObj->x.pOther = 0;` |
|       ! 0 | 1322 | `		pObj->iFlags &= ~MEMOBJ_AUX_DEFPATH;` |
|       ! 0 | 1323 | `	}` |
| 157877064 | 1324 | `	if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|  82005695 | 1325 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|   4990909 | 1326 | `			PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
|  79510243 | 1327 | `		}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|   9413235 | 1328 | `			PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|   4706615 | 1329 | `		}` |
|         - | 1330 | `		/* Release the internal buffer */` |
|  82005695 | 1331 | `		SyBlobRelease(&pObj->sBlob);` |
|         - | 1332 | `		/* Invalidate any prior representation */` |
|  82005695 | 1333 | `		pObj->iFlags = MEMOBJ_NULL;` |
|  41010697 | 1334 | `	}` |
| 157877064 | 1335 | `	return SXRET_OK;` |
|         5 | 1336 | `}` |
|         - | 1337 | `/*` |
|         - | 1338 | ` * Compare two ph7_values.` |
|         - | 1339 | ` * Return 0 if the values are equals, > 0 if pObj1 is greater than pObj2` |
|         - | 1340 | ` * or < 0 if pObj2 is greater than pObj1.` |
|         - | 1341 | ` * Type comparison table taken from the PHP language reference manual.` |
|         - | 1342 | ` * Comparisons of $x with PHP functions Expression` |
|         - | 1343 | ` *              gettype() 	empty() 	is_null() 	isset() 	boolean : if($x)` |
|         - | 1344 | ` * $x = ""; 	string 	    TRUE 	FALSE 	TRUE 	FALSE` |
|         - | 1345 | ` * $x = null 	NULL 	    TRUE 	TRUE 	FALSE 	FALSE` |
|         - | 1346 | ` * var $x; 	    NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|         - | 1347 | ` * $x is undefined 	NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|         - | 1348 | ` *  $x = array(); 	array 	TRUE 	FALSE 	TRUE 	FALSE` |
|         - | 1349 | ` * $x = false; 	boolean 	TRUE 	FALSE 	TRUE 	FALSE` |
|         - | 1350 | ` * $x = true; 	boolean 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1351 | ` * $x = 1; 	    integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1352 | ` * $x = 42; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1353 | ` * $x = 0; 	    integer 	TRUE 	FALSE 	TRUE 	FALSE` |
|         - | 1354 | ` * $x = -1; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1355 | ` * $x = "1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1356 | ` * $x = "0"; 	string 	TRUE 	FALSE 	TRUE 	FALSE` |
|         - | 1357 | ` * $x = "-1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1358 | ` * $x = "php"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1359 | ` * $x = "true"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1360 | ` * $x = "false"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1361 | ` *      Loose comparisons with ==` |
|         - | 1362 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|         - | 1363 | ` * TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE` |
|         - | 1364 | ` * FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|         - | 1365 | ` * 1 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1366 | ` * 0 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE` |
|         - | 1367 | ` * -1 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1368 | ` * "1" 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1369 | ` * "0" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1370 | ` * "-1" 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1371 | ` * NULL 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|         - | 1372 | ` * array() 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	FALSE` |
|         - | 1373 | ` * "php" 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|         - | 1374 | ` * "" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE` |
|         - | 1375 | ` *    Strict comparisons with ===` |
|         - | 1376 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|         - | 1377 | ` * TRUE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1378 | ` * FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1379 | ` * 1 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1380 | ` * 0 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1381 | ` * -1 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1382 | ` * "1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1383 | ` * "0" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1384 | ` * "-1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1385 | ` * NULL 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE` |
|         - | 1386 | ` * array() 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE` |
|         - | 1387 | ` * "php" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|         - | 1388 | ` * "" 	    FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE` |
|         - | 1389 | ` */` |
|   1846349 | 1390 | `PH7_PRIVATE sxi32 PH7_MemObjCmp(ph7_value *pObj1,ph7_value *pObj2,int bStrict,int iNest)` |
|         5 | 1391 | `{` |
|         - | 1392 | `	sxi32 iComb;` |
|         - | 1393 | `	sxi32 rc;` |
|   1846354 | 1394 | `	if( bStrict ){` |
|         - | 1395 | `		sxi32 iF1,iF2;` |
|         - | 1396 | `		/* Strict comparisons with === */` |
|    921913 | 1397 | `		iF1 = pObj1->iFlags&~MEMOBJ_AUX;` |
|    921913 | 1398 | `		iF2 = pObj2->iFlags&~MEMOBJ_AUX;` |
|    921913 | 1399 | `		if( iF1 != iF2 ){` |
|         - | 1400 | `			/* Not of the same type */` |
|    213859 | 1401 | `			return 1;` |
|         - | 1402 | `		}` |
|    354360 | 1403 | `	}` |
|         - | 1404 | `	/* Combine flag together */` |
|   1632500 | 1405 | `	iComb = pObj1->iFlags\|pObj2->iFlags;` |
|   1632495 | 1406 | `	if( !bStrict` |
|   1278801 | 1407 | `	 && (iComb & MEMOBJ_NULL) != 0` |
|    462683 | 1408 | `	 && (iComb & MEMOBJ_STRING) != 0` |
|        66 | 1409 | `	 && (iComb & (MEMOBJ_BOOL\|MEMOBJ_RES\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|         - | 1410 | `		/*` |
|         - | 1411 | `		 * PHP 8 comparison table: null loosely compared with a STRING is` |
|         - | 1412 | `		 * compared as the empty string (a string comparison), not through` |
|         - | 1413 | `		 * bool coercion — so null == "0" is FALSE and null < "0" is TRUE` |
|         - | 1414 | `		 * (php 7 and the historical PH7 behavior coerced both to bool,` |
|         - | 1415 | `		 * making any non-empty non-"0"-insensitive string "equal" to null).` |
|         - | 1416 | `		 * Convert the null side to "" and let the string branch below run.` |
|         - | 1417 | `		 */` |
|        45 | 1418 | `		if( pObj1->iFlags & MEMOBJ_NULL ){` |
|        35 | 1419 | `			PH7_MemObjToString(pObj1);` |
|        18 | 1420 | `		}else{` |
|        11 | 1421 | `			PH7_MemObjToString(pObj2);` |
|         - | 1422 | `		}` |
|        45 | 1423 | `		iComb = pObj1->iFlags\|pObj2->iFlags;` |
|        22 | 1424 | `	}` |
|   1632500 | 1425 | `	if( (pObj1->iFlags & MEMOBJ_RES) && (pObj2->iFlags & MEMOBJ_RES) ){` |
|         - | 1426 | `		/* php compares two resources by their ID. The boolean path below would` |
|         - | 1427 | `		 * call every live resource equal to every other, since all are truthy. */` |
|         5 | 1428 | `		sxu32 nId1 = PH7_VmResourceId(pObj1->pVm,pObj1->x.pOther);` |
|         5 | 1429 | `		sxu32 nId2 = PH7_VmResourceId(pObj2->pVm,pObj2->x.pOther);` |
|         5 | 1430 | `		return nId1 == nId2 ? 0 : (nId1 < nId2 ? -1 : 1);` |
|         - | 1431 | `	}` |
|   1632496 | 1432 | `	if( iComb & (MEMOBJ_NULL\|MEMOBJ_RES\|MEMOBJ_BOOL) ){` |
|         - | 1433 | `		/* Convert to boolean: Keep in mind FALSE < TRUE */` |
|     52359 | 1434 | `		if( (pObj1->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     28589 | 1435 | `			PH7_MemObjToBool(pObj1);` |
|     14292 | 1436 | `		}` |
|     52359 | 1437 | `		if( (pObj2->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     27481 | 1438 | `			PH7_MemObjToBool(pObj2);` |
|     13738 | 1439 | `		}` |
|     52359 | 1440 | `		return (sxi32)((pObj1->x.iVal != 0) - (pObj2->x.iVal != 0));` |
|   1580142 | 1441 | `	}else if ( iComb & MEMOBJ_HASHMAP ){` |
|         - | 1442 | `		/* Hashmap aka 'array' comparison */` |
|        71 | 1443 | `		if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 1444 | `			/* Array is always greater */` |
|       ! 0 | 1445 | `			return -1;` |
|         - | 1446 | `		}` |
|        71 | 1447 | `		if( (pObj2->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 1448 | `			/* Array is always greater */` |
|       ! 0 | 1449 | `			return 1;` |
|         - | 1450 | `		}` |
|         - | 1451 | `		/* Perform the comparison */` |
|        71 | 1452 | `		rc = PH7_HashmapCmp((ph7_hashmap *)pObj1->x.pOther,(ph7_hashmap *)pObj2->x.pOther,bStrict);` |
|        71 | 1453 | `		return rc;` |
|   1580074 | 1454 | `	}else if(iComb & MEMOBJ_OBJ ){` |
|         - | 1455 | `		/* Object comparison */` |
|       297 | 1456 | `		if( (pObj1->iFlags & MEMOBJ_OBJ) == 0 ){` |
|         - | 1457 | `			/* Object is always greater */` |
|       ! 0 | 1458 | `			return -1;` |
|         - | 1459 | `		}` |
|       297 | 1460 | `		if( (pObj2->iFlags & MEMOBJ_OBJ) == 0 ){` |
|         - | 1461 | `			/* Object is always greater */` |
|       ! 0 | 1462 | `			return 1;` |
|         - | 1463 | `		}` |
|         - | 1464 | `		/* Perform the comparison */` |
|       297 | 1465 | `		rc = PH7_ClassInstanceCmp((ph7_class_instance *)pObj1->x.pOther,(ph7_class_instance *)pObj2->x.pOther,bStrict,iNest);` |
|       297 | 1466 | `		return rc;` |
|   1579782 | 1467 | `	}else if ( iComb & MEMOBJ_STRING ){` |
|         - | 1468 | `		SyString s1,s2;` |
|    914301 | 1469 | `		if( !bStrict ){` |
|         - | 1470 | `			/*` |
|         - | 1471 | `			 * PHP 8 "saner string to number comparisons" (RFC): a numeric` |
|         - | 1472 | `			 * comparison is performed only when BOTH operands are numbers or` |
|         - | 1473 | `			 * numeric strings. A number compared with a NON-numeric string is` |
|         - | 1474 | `			 * compared as strings, with the number cast to its string form —` |
|         - | 1475 | `			 * so 0 == "abc" is false, "abc" < 10 is false, and max("abc",10)` |
|         - | 1476 | `			 * is "abc". (PHP 7 cast the non-numeric string to 0 and compared` |
|         - | 1477 | `			 * numerically; comparing when EITHER side was numeric is what this` |
|         - | 1478 | `			 * replaces.) Two non-numeric strings, or one numeric and one` |
|         - | 1479 | `			 * non-numeric string, still fall through to the string comparison` |
|         - | 1480 | `			 * below, unchanged.` |
|         - | 1481 | `			 */` |
|    262741 | 1482 | `			if( PH7_MemObjIsNumeric(pObj1) && PH7_MemObjIsNumeric(pObj2) ){` |
|         - | 1483 | `				/* Perform a numeric comparison */` |
|      1103 | 1484 | `				goto Numeric;` |
|         - | 1485 | `			}` |
|    130744 | 1486 | `		}` |
|         - | 1487 | `		/* Perform a strict string comparison.*/` |
|    913199 | 1488 | `		if( (pObj1->iFlags&MEMOBJ_STRING) == 0 ){` |
|        23 | 1489 | `			PH7_MemObjToString(pObj1);` |
|        11 | 1490 | `		}` |
|    913199 | 1491 | `		if( (pObj2->iFlags&MEMOBJ_STRING) == 0 ){` |
|         7 | 1492 | `			PH7_MemObjToString(pObj2);` |
|         3 | 1493 | `		}` |
|    913199 | 1494 | `		SyStringInitFromBuf(&s1,SyBlobData(&pObj1->sBlob),SyBlobLength(&pObj1->sBlob));` |
|    913199 | 1495 | `		SyStringInitFromBuf(&s2,SyBlobData(&pObj2->sBlob),SyBlobLength(&pObj2->sBlob));` |
|         - | 1496 | `		/*` |
|         - | 1497 | `		 * Strings are compared using memcmp(). If one value is an exact prefix of the` |
|         - | 1498 | `		 * other, then the shorter value is less than the longer value.` |
|         - | 1499 | `		 */` |
|    913199 | 1500 | `		rc = SyMemcmp((const void *)s1.zString,(const void *)s2.zString,SXMIN(s1.nByte,s2.nByte));` |
|    913199 | 1501 | `		if( rc == 0 ){` |
|    292199 | 1502 | `			if( s1.nByte != s2.nByte ){` |
|     19757 | 1503 | `				rc = s1.nByte < s2.nByte ? -1 : 1;` |
|      9880 | 1504 | `			}` |
|    146105 | 1505 | `		}` |
|    913199 | 1506 | `		return rc;` |
|    665486 | 1507 | `	}else if( iComb & (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|    332244 | 1508 | `Numeric:` |
|         - | 1509 | `		/* Perform a numeric comparison if one of the operand is numeric(integer or real) */` |
|    666588 | 1510 | `		if( (pObj1->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|      1083 | 1511 | `			PH7_MemObjToNumeric(pObj1);` |
|       541 | 1512 | `		}` |
|    666588 | 1513 | `		if( (pObj2->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|      1089 | 1514 | `			PH7_MemObjToNumeric(pObj2);` |
|       544 | 1515 | `		}` |
|    666588 | 1516 | `		if( (pObj1->iFlags & pObj2->iFlags & MEMOBJ_INT) == 0) {` |
|         - | 1517 | `			/*` |
|         - | 1518 | `			 * Symisc eXtension to the PHP language:` |
|         - | 1519 | `			 *  Floating point comparison is introduced and works as expected.` |
|         - | 1520 | `			 */` |
|         - | 1521 | `			ph7_real r1,r2;` |
|         - | 1522 | `			/* Compare as reals */` |
|       313 | 1523 | `			if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|        11 | 1524 | `				PH7_MemObjToReal(pObj1);` |
|         5 | 1525 | `			}` |
|       313 | 1526 | `			r1 = pObj1->rVal;` |
|       313 | 1527 | `			if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|        56 | 1528 | `				PH7_MemObjToReal(pObj2);` |
|        27 | 1529 | `			}` |
|       313 | 1530 | `			r2 = pObj2->rVal;` |
|       313 | 1531 | `			if( PH7_IS_NAN(r1) \|\| PH7_IS_NAN(r2) ){` |
|         - | 1532 | `				/*` |
|         - | 1533 | `				 * Keep a strict three-way comparator contract even for NaN values.` |
|         - | 1534 | `				 * For ordering purposes, NaN compares equal to NaN and greater than` |
|         - | 1535 | `				 * any non-NaN numeric value.` |
|         - | 1536 | `				 */` |
|        50 | 1537 | `				if( PH7_IS_NAN(r1) ){` |
|        40 | 1538 | `					return PH7_IS_NAN(r2) ? 0 : 1;` |
|         - | 1539 | `				}` |
|        11 | 1540 | `				return -1;` |
|         - | 1541 | `			}` |
|       265 | 1542 | `			if( r1 > r2 ){` |
|        57 | 1543 | `				return 1;` |
|       210 | 1544 | `			}else if( r1 < r2 ){` |
|       134 | 1545 | `				return -1;` |
|         - | 1546 | `			}` |
|        78 | 1547 | `			return 0;` |
|       ! 0 | 1548 | `		}else{` |
|         - | 1549 | `			/* Integer comparison */` |
|    666278 | 1550 | `			if( pObj1->x.iVal > pObj2->x.iVal ){` |
|      8177 | 1551 | `				return 1;` |
|    658106 | 1552 | `			}else if( pObj1->x.iVal < pObj2->x.iVal ){` |
|    638564 | 1553 | `				return -1;` |
|         - | 1554 | `			}` |
|     19547 | 1555 | `			return 0;` |
|         - | 1556 | `		}` |
|         - | 1557 | `	}` |
|         - | 1558 | `	/* NOT REACHED */` |
|       ! 0 | 1559 | `	return 0;` |
|    923936 | 1560 | `}` |
|         - | 1561 | `/*` |
|         - | 1562 | ` * Perform an addition operation of two ph7_values.` |
|         - | 1563 | ` * The reason this function is implemented here rather than 'vm.c'` |
|         - | 1564 | ` * is that the '+' operator is overloaded.` |
|         - | 1565 | ` * That is,the '+' operator is used for arithmetic operation and also` |
|         - | 1566 | ` * used for operation on arrays [i.e: union]. When used with an array` |
|         - | 1567 | ` * The + operator returns the right-hand array appended to the left-hand array.` |
|         - | 1568 | ` * For keys that exist in both arrays, the elements from the left-hand array` |
|         - | 1569 | ` * will be used, and the matching elements from the right-hand array will` |
|         - | 1570 | ` * be ignored.` |
|         - | 1571 | ` * This function take care of handling all the scenarios.` |
|         - | 1572 | ` */` |
|     23122 | 1573 | `PH7_PRIVATE sxi32 PH7_MemObjAdd(ph7_value *pObj1,ph7_value *pObj2,int bAddStore)` |
|         5 | 1574 | `{` |
|     23127 | 1575 | `	if( ((pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 1576 | `			/* Arithemtic operation */` |
|     19047 | 1577 | `			PH7_MemObjToNumeric(pObj1);` |
|     19047 | 1578 | `			PH7_MemObjToNumeric(pObj2);` |
|     19047 | 1579 | `			if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_REAL ){` |
|         - | 1580 | `				/* Floating point arithmetic */` |
|         - | 1581 | `				ph7_real a,b;` |
|        72 | 1582 | `				if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|        29 | 1583 | `					PH7_MemObjToReal(pObj1);` |
|        14 | 1584 | `				}` |
|        72 | 1585 | `				if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|         9 | 1586 | `					PH7_MemObjToReal(pObj2);` |
|         4 | 1587 | `				}` |
|        72 | 1588 | `				a = pObj1->rVal;` |
|        72 | 1589 | `				b = pObj2->rVal;` |
|        72 | 1590 | `				pObj1->rVal = a+b;` |
|        72 | 1591 | `				MemObjSetType(pObj1,MEMOBJ_REAL);` |
|         - | 1592 | `				/* Try to get an integer representation also */` |
|        72 | 1593 | `				MemObjTryIntger(&(*pObj1));` |
|        37 | 1594 | `			}else{` |
|         - | 1595 | `				/* Integer arithmetic; PHP promotes an overflowing sum to float.` |
|         - | 1596 | `				 * The integer-only build (PH7_OMIT_FLOATING_POINT) has no float` |
|         - | 1597 | `				 * type, so it wraps like OP_POW's OMIT path. */` |
|         - | 1598 | `				sxi64 a,b,r;` |
|     18977 | 1599 | `				a = pObj1->x.iVal;` |
|     18977 | 1600 | `				b = pObj2->x.iVal;` |
|     18977 | 1601 | `				if( PH7_ADD_OVERFLOW64(a,b,&r) ){` |
|         - | 1602 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         9 | 1603 | `					pObj1->rVal = (ph7_real)a + (ph7_real)b;` |
|         9 | 1604 | `					MemObjSetType(pObj1,MEMOBJ_REAL);` |
|         - | 1605 | `#else` |
|         - | 1606 | `					pObj1->x.iVal = r;` |
|         - | 1607 | `					MemObjSetType(pObj1,MEMOBJ_INT);` |
|         - | 1608 | `#endif` |
|         5 | 1609 | `				}else{` |
|     18969 | 1610 | `					pObj1->x.iVal = r;` |
|     18969 | 1611 | `					MemObjSetType(pObj1,MEMOBJ_INT);` |
|         - | 1612 | `				}` |
|         - | 1613 | `			}` |
|      9526 | 1614 | `	}else{` |
|      4085 | 1615 | `		if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP ){` |
|         - | 1616 | `			ph7_hashmap *pMap;` |
|         - | 1617 | `			sxi32 rc;` |
|      4085 | 1618 | `			if( bAddStore ){` |
|         - | 1619 | `				/* Do not duplicate the hashmap,use the left one since its an add&store operation.` |
|         - | 1620 | `				 */` |
|         3 | 1621 | `				if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 1622 | `					/* Force a hashmap cast */` |
|       ! 0 | 1623 | `					rc = PH7_MemObjToHashmap(pObj1);` |
|       ! 0 | 1624 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 1625 | `						PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|       ! 0 | 1626 | `						return rc;` |
|         - | 1627 | `					}` |
|       ! 0 | 1628 | `				}` |
|         - | 1629 | `				/* COW separate before in-place mutation */` |
|         3 | 1630 | `				pMap = PH7_HashmapCowSeparate(pObj1->pVm,pObj1);` |
|         2 | 1631 | `			}else{` |
|         - | 1632 | `				/* Create a new hashmap */` |
|      4083 | 1633 | `				pMap = PH7_NewHashmap(pObj1->pVm,0,0);` |
|      4083 | 1634 | `				if( pMap == 0){` |
|       ! 0 | 1635 | `					PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|       ! 0 | 1636 | `					return SXERR_MEM;` |
|         - | 1637 | `				}` |
|         - | 1638 | `			}` |
|      4085 | 1639 | `			if( !bAddStore ){` |
|      4083 | 1640 | `				if(pObj1->iFlags & MEMOBJ_HASHMAP ){` |
|         - | 1641 | `					/* Perform a hashmap duplication */` |
|      4083 | 1642 | `					PH7_HashmapDup((ph7_hashmap *)pObj1->x.pOther,pMap);` |
|      2044 | 1643 | `				}else{` |
|       ! 0 | 1644 | `					if((pObj1->iFlags & MEMOBJ_NULL) == 0 ){` |
|         - | 1645 | `						/* Simple insertion */` |
|       ! 0 | 1646 | `						PH7_HashmapInsert(pMap,0,pObj1);` |
|       ! 0 | 1647 | `					}` |
|         - | 1648 | `				}` |
|      2039 | 1649 | `			}` |
|         - | 1650 | `			/* Perform the union */` |
|      4085 | 1651 | `			if(pObj2->iFlags & MEMOBJ_HASHMAP ){` |
|      4085 | 1652 | `				PH7_HashmapUnion(pMap,(ph7_hashmap *)pObj2->x.pOther);` |
|      2045 | 1653 | `			}else{` |
|       ! 0 | 1654 | `				if((pObj2->iFlags & MEMOBJ_NULL) == 0 ){` |
|         - | 1655 | `					/* Simple insertion */` |
|       ! 0 | 1656 | `					PH7_HashmapInsert(pMap,0,pObj2);` |
|       ! 0 | 1657 | `				}` |
|         - | 1658 | `			}` |
|         - | 1659 | `			/* Reflect the change */` |
|      4085 | 1660 | `			if( pObj1->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 1661 | `				SyBlobRelease(&pObj1->sBlob);` |
|       ! 0 | 1662 | `			}` |
|      4085 | 1663 | `			pObj1->x.pOther = pMap;` |
|      4085 | 1664 | `			MemObjSetType(pObj1,MEMOBJ_HASHMAP);` |
|      2040 | 1665 | `		}` |
|         - | 1666 | `	}` |
|     23127 | 1667 | `	return SXRET_OK;` |
|     11566 | 1668 | `}` |
|         - | 1669 | `/*` |
|         - | 1670 | ` * Return a printable representation of the type of a given` |
|         - | 1671 | ` * ph7_value.` |
|         - | 1672 | ` */` |
|       ! 0 | 1673 | `PH7_PRIVATE const char * PH7_MemObjTypeDump(ph7_value *pVal)` |
|       ! 0 | 1674 | `{` |
|       ! 0 | 1675 | `	const char *zType = "";` |
|       ! 0 | 1676 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|       ! 0 | 1677 | `		zType = "null";` |
|       ! 0 | 1678 | `	}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|         - | 1679 | `		/* REAL is authoritative over a cached MEMOBJ_INT: an integer-valued` |
|         - | 1680 | `		 * real (e.g. 1.0) is reported as "double", matching PHP's gettype(). */` |
|       ! 0 | 1681 | `		zType = "double";` |
|       ! 0 | 1682 | `	}else if( pVal->iFlags & MEMOBJ_INT ){` |
|       ! 0 | 1683 | `		zType = "int";` |
|       ! 0 | 1684 | `	}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 1685 | `		zType = "string";` |
|       ! 0 | 1686 | `	}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|       ! 0 | 1687 | `		zType = "bool";` |
|       ! 0 | 1688 | `	}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|       ! 0 | 1689 | `		zType = "array";` |
|       ! 0 | 1690 | `	}else if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 1691 | `		zType = "object";` |
|       ! 0 | 1692 | `	}else if( pVal->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 1693 | `		zType = "resource";` |
|       ! 0 | 1694 | `	}` |
|       ! 0 | 1695 | `	return zType;` |
|       ! 0 | 1696 | `}` |
|         - | 1697 | `/*` |
|         - | 1698 | ` * Dump a ph7_value [i.e: get a printable representation of it's type and contents.].` |
|         - | 1699 | ` * Store the dump in the given blob.` |
|         - | 1700 | ` */` |
|         - | 1701 | `/*` |
|         - | 1702 | ` * php's var_dump float shape (serialize_precision = -1): the SHORTEST decimal` |
|         - | 1703 | ` * string that round-trips to the same double — 0.1+0.2 dumps every digit` |
|         - | 1704 | ` * (0.30000000000000004), 1.0 dumps "1" — pushed through the same` |
|         - | 1705 | ` * exponent/fraction normalization as echo (PH7_PhpFloatShape: uppercase E,` |
|         - | 1706 | ` * "1.0E+100"). Distinct from echo/casts, which use EG(precision)=14.` |
|         - | 1707 | ` */` |
|        62 | 1708 | `static void MemObjDumpRealValue(SyBlob *pOut,ph7_real rVal)` |
|         4 | 1709 | `{` |
|         - | 1710 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         - | 1711 | `	/* var_dump renders floats at serialize_precision = -1 — the SHORTEST decimal` |
|         - | 1712 | `	 * that round-trips, formatted by php's gcvt(ndigit=17) fixed-vs-exponential` |
|         - | 1713 | `	 * rule (exponential only when the leading-digit exponent e >= 17 or e <= -5,` |
|         - | 1714 | `	 * so 1500.0 -> "1500", 1e20 -> "1.0E+20"). That is exactly the shape serialize/` |
|         - | 1715 | `	 * var_export/json already emit, so share their helper. The old code searched` |
|         - | 1716 | `	 * "%.*G" from precision 1 upward, but %G's own exponential threshold moves with` |
|         - | 1717 | `	 * the precision, so a low-precision round-trip (1500.0 at %.2G) came back as` |
|         - | 1718 | `	 * "1.5E+3" — a rendering-only wrong answer this delegation removes. */` |
|        66 | 1719 | `	PH7_AppendShortestReal(pOut,rVal);` |
|         - | 1720 | `#else` |
|         - | 1721 | `	if( PH7_IS_NAN(rVal) ){` |
|         - | 1722 | `		SyBlobAppend(&(*pOut),"NAN",3);` |
|         - | 1723 | `	}else if( PH7_IS_INF(rVal) ){` |
|         - | 1724 | `		SyBlobAppend(&(*pOut),rVal < 0.0 ? "-INF" : "INF",rVal < 0.0 ? 4 : 3);` |
|         - | 1725 | `	}else{` |
|         - | 1726 | `		SyBlobFormat(&(*pOut),"%.15g",rVal);` |
|         - | 1727 | `	}` |
|         - | 1728 | `#endif` |
|        66 | 1729 | `}` |
|         - | 1730 | `/*` |
|         - | 1731 | ` * Emit a value's print_r INLINE representation (php: the echo conversion,` |
|         - | 1732 | ` * except true -> "1" and false/null -> ""). Containers never come through` |
|         - | 1733 | ` * here — the entry renderers recurse into the container dumpers instead.` |
|         - | 1734 | ` */` |
|       226 | 1735 | `PH7_PRIVATE void PH7_MemObjPrintRInline(SyBlob *pOut,ph7_value *pObj)` |
|         3 | 1736 | `{` |
|       229 | 1737 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|         7 | 1738 | `		return;` |
|         - | 1739 | `	}` |
|       223 | 1740 | `	if( pObj->iFlags & MEMOBJ_BOOL ){` |
|       ! 0 | 1741 | `		if( pObj->x.iVal != 0 ){` |
|       ! 0 | 1742 | `			SyBlobAppend(&(*pOut),"1",sizeof(char));` |
|       ! 0 | 1743 | `		}` |
|       ! 0 | 1744 | `		return;` |
|         - | 1745 | `	}` |
|       223 | 1746 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|         - | 1747 | `		/* Strings already hold their bytes (MemObjStringValue only CONVERTS` |
|         - | 1748 | `		 * non-strings into the output) */` |
|       118 | 1749 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       118 | 1750 | `			SyBlobAppend(&(*pOut),SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|        58 | 1751 | `		}` |
|       118 | 1752 | `		return;` |
|         - | 1753 | `	}` |
|       107 | 1754 | `	MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|       116 | 1755 | `}` |
|      2086 | 1756 | `PH7_PRIVATE sxi32 PH7_MemObjDump(` |
|         - | 1757 | `	SyBlob *pOut,      /* Store the dump here */` |
|         - | 1758 | `	ph7_value *pObj,   /* Dump this */` |
|         - | 1759 | `	int ShowType,      /* TRUE for var_dump; FALSE for print_r */` |
|         - | 1760 | `	int nTab,          /* Indent in SPACES: var_dump = this value's own line;` |
|         - | 1761 | `	                    * print_r = the container's parenthesis column */` |
|         - | 1762 | `	int nDepth,        /* Nesting level */` |
|         - | 1763 | `	int isRef          /* TRUE if referenced entry (var_dump prints '&') */` |
|         - | 1764 | `	)` |
|         5 | 1765 | `{` |
|      2091 | 1766 | `	sxi32 rc = SXRET_OK;` |
|         - | 1767 | `	int i;` |
|      2091 | 1768 | `	if( !ShowType ){` |
|         - | 1769 | `		/* ---- print_r ---- php prints scalars inline with NO newline; only` |
|         - | 1770 | `		 * containers render the Array/Object block (which the container` |
|         - | 1771 | `		 * dumpers terminate with ")\n"). References carry no marker. */` |
|       121 | 1772 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       114 | 1773 | `			return PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,FALSE,nTab,nDepth+1);` |
|         - | 1774 | `		}` |
|         8 | 1775 | `		if( (pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|         6 | 1776 | `			return PH7_ClassInstanceDump(&(*pOut),(ph7_class_instance *)pObj->x.pOther,FALSE,nTab,nDepth+1);` |
|         - | 1777 | `		}` |
|         3 | 1778 | `		PH7_MemObjPrintRInline(&(*pOut),pObj);` |
|         3 | 1779 | `		return SXRET_OK;` |
|         - | 1780 | `	}` |
|         - | 1781 | `	/* ---- var_dump ---- every value renders on its own line at nTab spaces,` |
|         - | 1782 | `	 * php's exact shapes: bool(true), NULL, int(n), float(shortest),` |
|         - | 1783 | `	 * string(N) "s", array(N) { … }, object(C)#id (n) { … }, &-references. */` |
|      6421 | 1784 | `	for( i = 0 ; i < nTab ; i++ ){` |
|      4453 | 1785 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      2229 | 1786 | `	}` |
|      1973 | 1787 | `	if( isRef ){` |
|        22 | 1788 | `		SyBlobAppend(&(*pOut),"&",sizeof(char));` |
|        10 | 1789 | `	}` |
|      1973 | 1790 | `	if( (pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|       141 | 1791 | `		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|       141 | 1792 | `		if( pInst->pClass->iFlags & PH7_CLASS_ENUM ){` |
|         - | 1793 | ``			/* php 8.1: var_dump of an enum case prints `enum(S::A)` — no body */`` |
|       ! 0 | 1794 | `			ph7_value *pName = PH7_EnumCaseNameValue(pInst);` |
|       ! 0 | 1795 | `			SyBlobFormat(&(*pOut),"enum(%z::",&pInst->pClass->sName);` |
|       ! 0 | 1796 | `			if( pName && SyBlobLength(&pName->sBlob) > 0 ){` |
|       ! 0 | 1797 | `				SyBlobAppend(&(*pOut),SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob));` |
|       ! 0 | 1798 | `			}` |
|       ! 0 | 1799 | `			SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       ! 0 | 1800 | `			return SXRET_OK;` |
|         - | 1801 | `		}` |
|       141 | 1802 | `		rc = PH7_ClassInstanceDump(&(*pOut),pInst,TRUE,nTab,nDepth+1);` |
|       141 | 1803 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       141 | 1804 | `		return rc;` |
|         - | 1805 | `	}` |
|      1835 | 1806 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|        59 | 1807 | `		SyBlobAppend(&(*pOut),"NULL\n",sizeof("NULL\n")-1);` |
|        59 | 1808 | `		return SXRET_OK;` |
|         - | 1809 | `	}` |
|      1781 | 1810 | `	if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       137 | 1811 | `		rc = PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,TRUE,nTab,nDepth+1);` |
|       137 | 1812 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       137 | 1813 | `		return rc;` |
|         - | 1814 | `	}` |
|      1649 | 1815 | `	if( pObj->iFlags & MEMOBJ_BOOL ){` |
|       509 | 1816 | `		if( pObj->x.iVal != 0 ){` |
|       315 | 1817 | `			SyBlobAppend(&(*pOut),"bool(true)\n",sizeof("bool(true)\n")-1);` |
|       160 | 1818 | `		}else{` |
|       199 | 1819 | `			SyBlobAppend(&(*pOut),"bool(false)\n",sizeof("bool(false)\n")-1);` |
|         - | 1820 | `		}` |
|       509 | 1821 | `		return SXRET_OK;` |
|         - | 1822 | `	}` |
|      1145 | 1823 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         - | 1824 | `		/* Checked BEFORE the int flag: an integer-valued real carries a cached` |
|         - | 1825 | `		 * MEMOBJ_INT view too, and php dumps it as float(1). */` |
|        66 | 1826 | `		SyBlobAppend(&(*pOut),"float(",sizeof("float(")-1);` |
|        66 | 1827 | `		MemObjDumpRealValue(&(*pOut),pObj->rVal);` |
|        66 | 1828 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        66 | 1829 | `		return SXRET_OK;` |
|         - | 1830 | `	}` |
|      1083 | 1831 | `	if( pObj->iFlags & MEMOBJ_INT ){` |
|       629 | 1832 | `		SyBlobFormat(&(*pOut),"int(%qd)",pObj->x.iVal);` |
|       629 | 1833 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       629 | 1834 | `		return SXRET_OK;` |
|         - | 1835 | `	}` |
|       459 | 1836 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|       459 | 1837 | `		SyBlobFormat(&(*pOut),"string(%u) \"",SyBlobLength(&pObj->sBlob));` |
|       459 | 1838 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       387 | 1839 | `			SyBlobAppend(&(*pOut),SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|       191 | 1840 | `		}` |
|       459 | 1841 | `		SyBlobAppend(&(*pOut),"\"\n",sizeof("\"\n")-1);` |
|       459 | 1842 | `		return SXRET_OK;` |
|         - | 1843 | `	}` |
|       ! 0 | 1844 | `	if( pObj->iFlags & MEMOBJ_RES ){` |
|         - | 1845 | ``		/* php: `resource(N) of type (stream)`, and `(Unknown)` once closed —`` |
|         - | 1846 | `		 * which is exactly what PH7_VfsResourceType() already reports. The old` |
|         - | 1847 | `		 * shape printed the heap pointer through the string cast instead. */` |
|       ! 0 | 1848 | `		SyBlobFormat(&(*pOut),"resource(%u) of type (%s)\n",` |
|       ! 0 | 1849 | `			PH7_VmResourceId(pObj->pVm,pObj->x.pOther),` |
|       ! 0 | 1850 | `			PH7_VfsResourceType(pObj->x.pOther));` |
|       ! 0 | 1851 | `		return SXRET_OK;` |
|         - | 1852 | `	}` |
|         - | 1853 | ``	/* Anything else: the legacy `type(value)` shape. */`` |
|         - | 1854 | `	{` |
|       ! 0 | 1855 | `		const char *zType = PH7_MemObjTypeDump(pObj);` |
|       ! 0 | 1856 | `		SyBlobAppend(&(*pOut),zType,SyStrlen(zType));` |
|       ! 0 | 1857 | `		SyBlobAppend(&(*pOut),"(",sizeof(char));` |
|       ! 0 | 1858 | `		MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|       ! 0 | 1859 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|         - | 1860 | `	}` |
|       ! 0 | 1861 | `	return rc;` |
|      1048 | 1862 | `}` |
|         - | 1863 |  |
