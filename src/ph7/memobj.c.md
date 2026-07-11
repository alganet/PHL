# src/ph7/memobj.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 780/868 lines (89.86%)

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
|        - |    9 |  |
|        - |   10 | `/* Provide PHP-style type names for values.  This utility may be reused` |
|        - |   11 | ` * by any subsystem that works with ph7_value.` |
|        - |   12 | ` */` |
|      416 |   13 | `PH7_PRIVATE const char *ph7_type_name(ph7_value *pVal)` |
|        5 |   14 | `{` |
|      421 |   15 | `	if( ph7_value_is_null(pVal) ) return "null";` |
|      389 |   16 | `	if( ph7_value_is_bool(pVal) ) return "bool";` |
|      381 |   17 | `	if( ph7_value_is_int(pVal) ) return "int";` |
|      323 |   18 | `	if( ph7_value_is_float(pVal) ) return "float";` |
|      313 |   19 | `	if( ph7_value_is_string(pVal) ) return "string";` |
|      131 |   20 | `	if( ph7_value_is_array(pVal) ) return "array";` |
|       30 |   21 | `	if( ph7_value_is_object(pVal) ) return "object";` |
|        3 |   22 | `	if( ph7_value_is_resource(pVal) ) return "resource";` |
|      ! 0 |   23 | `	return "unknown";` |
|      213 |   24 | `}` |
|        - |   25 |  |
|        - |   26 | `/*` |
|        - |   27 | ` * Notes on memory objects [i.e: ph7_value].` |
|        - |   28 | ` * Internally, the PH7 virtual machine manipulates nearly all PHP values` |
|        - |   29 | ` * [i.e: string,int,float,resource,object,bool,null..] as ph7_values structures.` |
|        - |   30 | ` * Each ph7_values struct may cache multiple representations (string,` |
|        - |   31 | ` * integer etc.) of the same value.` |
|        - |   32 | ` */` |
|        - |   33 | `/*` |
|        - |   34 | ` * Convert a 64-bit IEEE double into a 64-bit signed integer.` |
|        - |   35 | ` * If the double is too large, return 0x8000000000000000.` |
|        - |   36 | ` *` |
|        - |   37 | ` * Most systems appear to do this simply by assigning ariables and without` |
|        - |   38 | ` * the extra range tests.` |
|        - |   39 | ` * But there are reports that windows throws an expection if the floating` |
|        - |   40 | ` * point value is out of range.` |
|        - |   41 | ` */` |
|     2908 |   42 | `static sxi64 MemObjRealToInt(ph7_value *pObj)` |
|        5 |   43 | `{` |
|        - |   44 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |   45 | `	/* Real and 64bit integer are the same when floating point arithmetic` |
|        - |   46 | `	 * is omitted from the build.` |
|        - |   47 | `	 */` |
|        - |   48 | `	return pObj->rVal;` |
|        - |   49 | `#else` |
|        - |   50 | ` /*` |
|        - |   51 | `  ** Many compilers we encounter do not define constants for the` |
|        - |   52 | `  ** minimum and maximum 64-bit integers, or they define them` |
|        - |   53 | `  ** inconsistently.  And many do not understand the "LL" notation.` |
|        - |   54 | `  ** So we define our own static constants here using nothing` |
|        - |   55 | `  ** larger than a 32-bit integer constant.` |
|        - |   56 | `  */` |
|        - |   57 | `  static const sxi64 maxInt = LARGEST_INT64;` |
|        - |   58 | `  static const sxi64 minInt = SMALLEST_INT64;` |
|     2913 |   59 | `  ph7_real r = pObj->rVal;` |
|     2913 |   60 | `  if( r<(ph7_real)minInt ){` |
|        3 |   61 | `    return minInt;` |
|     2911 |   62 | `  }else if( r>(ph7_real)maxInt ){` |
|        - |   63 | `    /* minInt is correct here - not maxInt.  It turns out that assigning` |
|        - |   64 | `    ** a very large positive number to an integer results in a very large` |
|        - |   65 | `    ** negative integer.  This makes no sense, but it is what x86 hardware` |
|        - |   66 | `    ** does so for compatibility we will do the same in software. */` |
|      135 |   67 | `    return minInt;` |
|      ! 0 |   68 | `  }else{` |
|     2777 |   69 | `    return (sxi64)r;` |
|        - |   70 | `  }` |
|        - |   71 | `#endif` |
|     1459 |   72 | `}` |
|        - |   73 | `/*` |
|        - |   74 | ` * Convert a raw token value typically a stream of digit [i.e: hex,octal,binary or decimal]` |
|        - |   75 | ` * to a 64-bit integer.` |
|        - |   76 | ` */` |
|   135828 |   77 | `PH7_PRIVATE sxi64 PH7_TokenValueToInt64(SyString *pVal)` |
|        5 |   78 | `{` |
|   135833 |   79 | `	sxi64 iVal = 0;` |
|   135833 |   80 | `	if( pVal->nByte <= 0 ){` |
|        7 |   81 | `		return 0;` |
|        - |   82 | `	}` |
|   135827 |   83 | `	if( pVal->zString[0] == '0' ){` |
|        - |   84 | `		sxi32 c;` |
|    56091 |   85 | `		if( pVal->nByte == sizeof(char) ){` |
|    55689 |   86 | `			return 0;` |
|        - |   87 | `		}` |
|      403 |   88 | `		c = pVal->zString[1];` |
|      403 |   89 | `		if( c  == 'x' \|\| c == 'X' ){` |
|        - |   90 | `			/* Hex digit stream */` |
|       69 |   91 | `			SyHexStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|      369 |   92 | `		}else if( c == 'b' \|\| c == 'B' ){` |
|        - |   93 | `			/* Binary digit stream */` |
|      277 |   94 | `			SyBinaryStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|      139 |   95 | `		}else{` |
|        - |   96 | `			/* Octal digit stream */` |
|       59 |   97 | `			SyOctalStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|        - |   98 | `		}` |
|      202 |   99 | `	}else{` |
|        - |  100 | `		/* Decimal digit stream */` |
|    79741 |  101 | `		SyStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|        - |  102 | `	}` |
|    80143 |  103 | `	return iVal;` |
|    67919 |  104 | `}` |
|        - |  105 | `/*` |
|        - |  106 | ` * Return some kind of 64-bit integer value which is the best we can` |
|        - |  107 | ` * do at representing the value that pObj describes as a string` |
|        - |  108 | ` * representation.` |
|        - |  109 | ` */` |
|      434 |  110 | `static sxi64 MemObjStringToInt(ph7_value *pObj)` |
|        5 |  111 | `{` |
|        - |  112 | `	SyString sVal;` |
|      439 |  113 | `	SyStringInitFromBuf(&sVal,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|      439 |  114 | `	return PH7_TokenValueToInt64(&sVal);` |
|        5 |  115 | `}` |
|        - |  116 | `/*` |
|        - |  117 | ` * Call a magic class method [i.e: __toString(),__toInt(),...]` |
|        - |  118 | ` * Return SXRET_OK if the magic method is available and have been` |
|        - |  119 | ` * successfully called. Any other return value indicates failure.` |
|        - |  120 | ` */` |
|       88 |  121 | `static sxi32 MemObjCallClassCastMethod(` |
|        - |  122 | `	ph7_vm *pVm,               /* VM that trigger the invocation */` |
|        - |  123 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object] */` |
|        - |  124 | `	const char *zMethod,       /* Magic method name [i.e: __toString] */` |
|        - |  125 | `	sxu32 nLen,                /* Method name length */` |
|        - |  126 | `	ph7_value *pResult         /* OUT: Store the return value of the magic method here */` |
|        - |  127 | `	)` |
|        5 |  128 | `{` |
|        - |  129 | `	ph7_class_method *pMethod;` |
|        - |  130 | `	/* Check if the method is available */` |
|       93 |  131 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,zMethod,nLen);` |
|       93 |  132 | `	if( pMethod == 0 ){` |
|        - |  133 | `		/* No such method */` |
|        6 |  134 | `		return SXERR_NOTFOUND;` |
|        - |  135 | `	}` |
|        - |  136 | `	/* Invoke the desired method */` |
|       89 |  137 | `	PH7_VmCallClassMethod(&(*pVm),&(*pThis),pMethod,&(*pResult),0,0);` |
|        - |  138 | `	/* Method successfully called,pResult should hold the return value */` |
|       89 |  139 | `	return SXRET_OK;` |
|       49 |  140 | `}` |
|        - |  141 | `/*` |
|        - |  142 | ` * Return some kind of integer value which is the best we can` |
|        - |  143 | ` * do at representing the value that pObj describes as an integer.` |
|        - |  144 | ` * If pObj is an integer, then the value is exact. If pObj is` |
|        - |  145 | ` * a floating-point then  the value returned is the integer part.` |
|        - |  146 | ` * If pObj is a string, then we make an attempt to convert it into` |
|        - |  147 | ` * a integer and return that.` |
|        - |  148 | ` * If pObj represents a NULL value, return 0.` |
|        - |  149 | ` */` |
|      502 |  150 | `static sxi64 MemObjIntValue(ph7_value *pObj)` |
|        5 |  151 | `{` |
|        - |  152 | `	sxi32 iFlags;` |
|      507 |  153 | `	iFlags = pObj->iFlags;` |
|      507 |  154 | `	if (iFlags & MEMOBJ_REAL ){` |
|       33 |  155 | `		return MemObjRealToInt(&(*pObj));` |
|      475 |  156 | `	}else if( iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      126 |  157 | `		return pObj->x.iVal;` |
|      351 |  158 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|      331 |  159 | `		return MemObjStringToInt(&(*pObj));` |
|       21 |  160 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|        9 |  161 | `		return 0;` |
|       13 |  162 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|        7 |  163 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|        7 |  164 | `		sxu32 n = pMap->nEntry;` |
|        7 |  165 | `		PH7_HashmapUnref(pMap);` |
|        - |  166 | `		/* Return total number of entries in the hashmap */` |
|        7 |  167 | `		return n;` |
|        7 |  168 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|        - |  169 | `		ph7_value sResult;` |
|        5 |  170 | `		sxi64 iVal = 1;` |
|        - |  171 | `		sxi32 rc;` |
|        - |  172 | `		/* Invoke the [__toInt()] magic method if available [note that this is a symisc extension]  */` |
|        5 |  173 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|        5 |  174 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|        - |  175 | `			"__toInt",sizeof("__toInt")-1,&sResult);` |
|        5 |  176 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_INT) ){` |
|        - |  177 | `			/* Extract method return value */` |
|        5 |  178 | `			iVal = sResult.x.iVal;` |
|        2 |  179 | `		}` |
|        5 |  180 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|        5 |  181 | `		PH7_MemObjRelease(&sResult);` |
|        5 |  182 | `		return iVal;` |
|        3 |  183 | `	}else if(iFlags & MEMOBJ_RES ){` |
|        3 |  184 | `		return pObj->x.pOther != 0;` |
|        - |  185 | `	}` |
|        - |  186 | `	/* CANT HAPPEN */` |
|      ! 0 |  187 | `	return 0;` |
|      256 |  188 | `}` |
|        - |  189 | `/*` |
|        - |  190 | ` * Return some kind of real value which is the best we can` |
|        - |  191 | ` * do at representing the value that pObj describes as a real.` |
|        - |  192 | ` * If pObj is a real, then the value is exact.If pObj is an` |
|        - |  193 | ` * integer then the integer  is promoted to real and that value` |
|        - |  194 | ` * is returned.` |
|        - |  195 | ` * If pObj is a string, then we make an attempt to convert it` |
|        - |  196 | ` * into a real and return that.` |
|        - |  197 | ` * If pObj represents a NULL value, return 0.0` |
|        - |  198 | ` */` |
|     1714 |  199 | `static ph7_real MemObjRealValue(ph7_value *pObj)` |
|        5 |  200 | `{` |
|        - |  201 | `	sxi32 iFlags;` |
|     1719 |  202 | `	iFlags = pObj->iFlags;` |
|     1719 |  203 | `	if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  204 | `		return pObj->rVal;` |
|     1719 |  205 | `	}else if (iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      756 |  206 | `		return (ph7_real)pObj->x.iVal;` |
|      965 |  207 | `	}else if (iFlags & MEMOBJ_STRING){` |
|        - |  208 | `		SyString sString;` |
|        - |  209 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  210 | `		ph7_real rVal = 0;` |
|        - |  211 | `#else` |
|      959 |  212 | `		ph7_real rVal = 0.0;` |
|        - |  213 | `#endif` |
|      959 |  214 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|      959 |  215 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|        - |  216 | `			/* Convert as much as we can */` |
|        - |  217 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  218 | `			rVal = MemObjStringToInt(&(*pObj));` |
|        - |  219 | `#else` |
|      959 |  220 | `			SyStrToReal(sString.zString,sString.nByte,(void *)&rVal,0);` |
|        - |  221 | `#endif` |
|      477 |  222 | `		}` |
|      959 |  223 | `		return rVal;` |
|        7 |  224 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|        - |  225 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  226 | `		return 0;` |
|        - |  227 | `#else` |
|      ! 0 |  228 | `		return 0.0;` |
|        - |  229 | `#endif` |
|        7 |  230 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|        - |  231 | `		/* Return the total number of entries in the hashmap */` |
|      ! 0 |  232 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      ! 0 |  233 | `		ph7_real n = (ph7_real)pMap->nEntry;` |
|      ! 0 |  234 | `		PH7_HashmapUnref(pMap);` |
|      ! 0 |  235 | `		return n;` |
|        7 |  236 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|        - |  237 | `		ph7_value sResult;` |
|        5 |  238 | `		ph7_real rVal = 1;` |
|        - |  239 | `		sxi32 rc;` |
|        - |  240 | `		/* Invoke the [__toFloat()] magic method if available [note that this is a symisc extension]  */` |
|        5 |  241 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|        5 |  242 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|        - |  243 | `			"__toFloat",sizeof("__toFloat")-1,&sResult);` |
|        5 |  244 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_REAL) ){` |
|        - |  245 | `			/* Extract method return value */` |
|        5 |  246 | `			rVal = sResult.rVal;` |
|        2 |  247 | `		}` |
|        5 |  248 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|        5 |  249 | `		PH7_MemObjRelease(&sResult);` |
|        5 |  250 | `		return rVal;` |
|        3 |  251 | `	}else if(iFlags & MEMOBJ_RES ){` |
|        3 |  252 | `		return (ph7_real)(pObj->x.pOther != 0);` |
|        - |  253 | `	}` |
|        - |  254 | `	/* NOT REACHED  */` |
|      ! 0 |  255 | `	return 0;` |
|      862 |  256 | `}` |
|        - |  257 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |  258 | `/*` |
|        - |  259 | ` * Post-process a libc-formatted float into php's exact shape (php_gcvt /` |
|        - |  260 | ` * smart_str_append_double semantics): strip the exponent's zero padding` |
|        - |  261 | ` * (libc's 1e+08 becomes php's 1e+8; a zero exponent stays e+0) and, when` |
|        - |  262 | ` * bGeneric is set (%g-style output, including the default float->string` |
|        - |  263 | ` * cast), make an exponent-form mantissa keep a fractional digit` |
|        - |  264 | ` * (1e+20 -> 1.0e+20). zBuf must be NUL-terminated with at least two bytes` |
|        - |  265 | ` * of spare capacity past the NUL. Returns the new length.` |
|        - |  266 | ` * Defined here (not builtin.c) because the float->string cast below needs it` |
|        - |  267 | ` * even when builtin.c's formatting region is compiled out` |
|        - |  268 | ` * (PH7_DISABLE_DISK_IO); the printf family reuses it from PH7_InputFormat.` |
|        - |  269 | ` */` |
|      488 |  270 | `PH7_PRIVATE sxi32 PH7_PhpFloatShape(char *zBuf,sxi32 nLen,int bGeneric)` |
|        4 |  271 | `{` |
|        - |  272 | `	sxi32 iExp,i;` |
|      492 |  273 | `	iExp = nLen - 1;` |
|     4160 |  274 | `	while( iExp > 0 && zBuf[iExp] != 'e' && zBuf[iExp] != 'E' ){` |
|     3672 |  275 | `		iExp--;` |
|        4 |  276 | `	}` |
|      492 |  277 | `	if( iExp <= 0 ){` |
|      446 |  278 | `		return nLen; /* No exponent part (fixed notation) */` |
|        - |  279 | `	}` |
|        - |  280 | `	{` |
|       47 |  281 | `		sxi32 iDig = iExp + 1;` |
|        - |  282 | `		sxi32 iFirst;` |
|       47 |  283 | `		if( zBuf[iDig] == '+' \|\| zBuf[iDig] == '-' ){` |
|       47 |  284 | `			iDig++;` |
|       23 |  285 | `		}` |
|       47 |  286 | `		iFirst = iDig;` |
|       83 |  287 | `		while( zBuf[iFirst] == '0' && iFirst + 1 < nLen` |
|       61 |  288 | `		 && zBuf[iFirst+1] >= '0' && zBuf[iFirst+1] <= '9' ){` |
|       25 |  289 | `			iFirst++;` |
|        1 |  290 | `		}` |
|       47 |  291 | `		if( iFirst > iDig ){` |
|       25 |  292 | `			sxi32 nStrip = iFirst - iDig;` |
|       73 |  293 | `			for( i = iDig ; i + nStrip <= nLen ; i++ ){` |
|       49 |  294 | `				zBuf[i] = zBuf[i+nStrip]; /* moves the NUL too */` |
|       25 |  295 | `			}` |
|       25 |  296 | `			nLen -= nStrip;` |
|       12 |  297 | `		}` |
|        - |  298 | `	}` |
|       47 |  299 | `	if( bGeneric ){` |
|       31 |  300 | `		int bHasDot = 0;` |
|       63 |  301 | `		for( i = 0 ; i < iExp ; i++ ){` |
|       45 |  302 | `			if( zBuf[i] == '.' ){ bHasDot = 1; break; }` |
|       17 |  303 | `		}` |
|       31 |  304 | `		if( !bHasDot ){` |
|      107 |  305 | `			for( i = nLen ; i >= iExp ; i-- ){` |
|       89 |  306 | `				zBuf[i+2] = zBuf[i]; /* moves the NUL too */` |
|       45 |  307 | `			}` |
|       19 |  308 | `			zBuf[iExp] = '.';` |
|       19 |  309 | `			zBuf[iExp+1] = '0';` |
|       19 |  310 | `			nLen += 2;` |
|        9 |  311 | `		}` |
|       15 |  312 | `	}` |
|       47 |  313 | `	return nLen;` |
|      248 |  314 | `}` |
|        - |  315 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|        - |  316 | `/*` |
|        - |  317 | ` * Return the string representation of a given ph7_value.` |
|        - |  318 | ` * This function never fail and always return SXRET_OK.` |
|        - |  319 | ` */` |
|    56444 |  320 | `static sxi32 MemObjStringValue(SyBlob *pOut,ph7_value *pObj,sxu8 bStrictBool)` |
|        5 |  321 | `{` |
|    56449 |  322 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|        - |  323 | `		/* Handle special floating-point values first */` |
|      376 |  324 | `		if( PH7_IS_NAN(pObj->rVal) ){` |
|      ! 0 |  325 | `			SyBlobAppend(&(*pOut),"NAN",3);` |
|      376 |  326 | `		}else if( PH7_IS_INF(pObj->rVal) ){` |
|        5 |  327 | `			if( pObj->rVal < 0.0 ){` |
|      ! 0 |  328 | `				SyBlobAppend(&(*pOut),"-INF",4);` |
|      ! 0 |  329 | `			}else{` |
|        5 |  330 | `				SyBlobAppend(&(*pOut),"INF",3);` |
|        - |  331 | `			}` |
|        3 |  332 | `		}else{` |
|        - |  333 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |  334 | `			/* php's default float->string conversion (echo/concat/cast):` |
|        - |  335 | `			 * zend_gcvt with EG(precision)=14 and an uppercase exponent` |
|        - |  336 | `			 * marker (smart_str_append_double) — 1/3 -> "0.33333333333333",` |
|        - |  337 | `			 * 1e15 -> "1.0E+15", -0.0 -> "-0". libc snprintf supplies` |
|        - |  338 | `			 * correctly-rounded digits; PH7_PhpFloatShape applies php's` |
|        - |  339 | `			 * exponent/fraction quirks. */` |
|        - |  340 | `			char zNum[48]; /* %.14G peaks at ~22 bytes; +2 spare for ".0" */` |
|      372 |  341 | `			sxi32 n = (sxi32)snprintf(zNum,sizeof(zNum),"%.14G",pObj->rVal);` |
|      372 |  342 | `			if( n < 0 \|\| n >= (sxi32)sizeof(zNum) ){` |
|      ! 0 |  343 | `				n = (sxi32)SyStrlen(zNum);` |
|      ! 0 |  344 | `			}` |
|      372 |  345 | `			n = PH7_PhpFloatShape(zNum,n,TRUE);` |
|      372 |  346 | `			SyBlobAppend(&(*pOut),zNum,(sxu32)n);` |
|        - |  347 | `#else` |
|        - |  348 | `			SyBlobFormat(&(*pOut),"%.15g",pObj->rVal);` |
|        - |  349 | `#endif` |
|        4 |  350 | `		}` |
|    56263 |  351 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|    55607 |  352 | `		SyBlobFormat(&(*pOut),"%qd",pObj->x.iVal);` |
|        - |  353 | `		/* %qd (BSD quad) is equivalent to %lld in the libc printf */` |
|    28276 |  354 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|      359 |  355 | `		if( bStrictBool ){` |
|        - |  356 | `			/* Actual string cast: true -> "1", false -> "" (like PHP) */` |
|      253 |  357 | `			if( pObj->x.iVal ){` |
|       29 |  358 | `				SyBlobAppend(&(*pOut),"1",sizeof("1")-1);` |
|       13 |  359 | `			}` |
|        - |  360 | `			/* false produces empty string, nothing to append */` |
|      129 |  361 | `		}else{` |
|        - |  362 | `			/* Display path (var_dump, print_r): show TRUE/FALSE */` |
|      109 |  363 | `			if( pObj->x.iVal ){` |
|       65 |  364 | `				SyBlobAppend(&(*pOut),"TRUE",sizeof("TRUE")-1);` |
|       34 |  365 | `			}else{` |
|       46 |  366 | `				SyBlobAppend(&(*pOut),"FALSE",sizeof("FALSE")-1);` |
|        - |  367 | `			}` |
|        5 |  368 | `		}` |
|      298 |  369 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|      ! 0 |  370 | `		SyBlobAppend(&(*pOut),"Array",sizeof("Array")-1);` |
|      ! 0 |  371 | `		PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
|      121 |  372 | `	}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|        - |  373 | `		ph7_value sResult;` |
|        - |  374 | `		sxi32 rc;` |
|        - |  375 | `		/* Invoke the __toString() method if available */` |
|       79 |  376 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|       79 |  377 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|        - |  378 | `			"__toString",sizeof("__toString")-1,&sResult);` |
|       79 |  379 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_STRING) && SyBlobLength(&sResult.sBlob) > 0){` |
|        - |  380 | `			/* Expand method return value */` |
|       75 |  381 | `			SyBlobDup(&sResult.sBlob,pOut);` |
|       40 |  382 | `		}else{` |
|        - |  383 | `			/* Expand "Object" as requested by the PHP language reference manual */` |
|        6 |  384 | `			SyBlobAppend(&(*pOut),"Object",sizeof("Object")-1);` |
|        - |  385 | `		}` |
|       79 |  386 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|       79 |  387 | `		PH7_MemObjRelease(&sResult);` |
|       82 |  388 | `	}else if(pObj->iFlags & MEMOBJ_RES ){` |
|        3 |  389 | `		SyBlobFormat(&(*pOut),"ResourceID_%#x",pObj->x.pOther);` |
|        1 |  390 | `	}` |
|    56449 |  391 | `	return SXRET_OK;` |
|        5 |  392 | `}` |
|        - |  393 | `/*` |
|        - |  394 | ` * Return some kind of boolean value which is the best we can do` |
|        - |  395 | ` * at representing the value that pObj describes as a boolean.` |
|        - |  396 | ` * When converting to boolean, the following values are considered FALSE:` |
|        - |  397 | ` * NULL` |
|        - |  398 | ` * the boolean FALSE itself.` |
|        - |  399 | ` * the integer 0 (zero).` |
|        - |  400 | ` * the real 0.0 (zero).` |
|        - |  401 | ` * the empty string,a stream of zero [i.e: "0","00","000",...] and the string` |
|        - |  402 | ` * "false".` |
|        - |  403 | ` * an array with zero elements.` |
|        - |  404 | ` */` |
|    14096 |  405 | `static sxi32 MemObjBooleanValue(ph7_value *pObj)` |
|        5 |  406 | `{` |
|        - |  407 | `	sxi32 iFlags;` |
|    14101 |  408 | `	iFlags = pObj->iFlags;` |
|    14101 |  409 | `	if (iFlags & MEMOBJ_REAL ){` |
|        - |  410 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  411 | `		return pObj->rVal ? 1 : 0;` |
|        - |  412 | `#else` |
|       12 |  413 | `		return pObj->rVal != 0.0 ? 1 : 0;` |
|        - |  414 | `#endif` |
|    14091 |  415 | `	}else if( iFlags & MEMOBJ_INT ){` |
|      131 |  416 | `		return pObj->x.iVal ? 1 : 0;` |
|    13965 |  417 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|        - |  418 | `		SyString sString;` |
|       65 |  419 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|       65 |  420 | `		if( sString.nByte == 0 ){` |
|        - |  421 | `			/* Empty string */` |
|       15 |  422 | `			return 0;` |
|       50 |  423 | `		}else if( (sString.nByte == sizeof("true") - 1 && SyStrnicmp(sString.zString,"true",sizeof("true")-1) == 0) \|\|` |
|       53 |  424 | `			(sString.nByte == sizeof("on") - 1 && SyStrnicmp(sString.zString,"on",sizeof("on")-1) == 0) \|\|` |
|       48 |  425 | `			(sString.nByte == sizeof("yes") - 1 && SyStrnicmp(sString.zString,"yes",sizeof("yes")-1) == 0) ){` |
|        5 |  426 | `				return 1;` |
|       48 |  427 | `		}else if( sString.nByte == sizeof("false") - 1 && SyStrnicmp(sString.zString,"false",sizeof("false")-1) == 0 ){` |
|      ! 0 |  428 | `			return 0;` |
|      ! 0 |  429 | `		}else{` |
|        - |  430 | `			const char *zIn,*zEnd;` |
|       48 |  431 | `			zIn = sString.zString;` |
|       48 |  432 | `			zEnd = &zIn[sString.nByte];` |
|       48 |  433 | `			while( zIn < zEnd && zIn[0] == '0' ){` |
|      ! 0 |  434 | `				zIn++;` |
|      ! 0 |  435 | `			}` |
|       48 |  436 | `			return zIn >= zEnd ? 0 : 1;` |
|      ! 0 |  437 | `		}` |
|    13903 |  438 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|    12863 |  439 | `		return 0;` |
|     1045 |  440 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|       20 |  441 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|       20 |  442 | `		sxu32 n = pMap->nEntry;` |
|       20 |  443 | `		PH7_HashmapUnref(pMap);` |
|       20 |  444 | `		return n > 0 ? TRUE : FALSE;` |
|     1027 |  445 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|        - |  446 | `		ph7_value sResult;` |
|        7 |  447 | `		sxi32 iVal = 1;` |
|        - |  448 | `		sxi32 rc;` |
|        - |  449 | `		/* Invoke the __toBool() method if available [note that this is a symisc extension]  */` |
|        7 |  450 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|        7 |  451 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|        - |  452 | `			"__toBool",sizeof("__toBool")-1,&sResult);` |
|        7 |  453 | `		if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL)) ){` |
|        - |  454 | `			/* Extract method return value */` |
|        5 |  455 | `			iVal = (sxi32)(sResult.x.iVal != 0); /* Stupid cc warning -W -Wall -O6 */` |
|        2 |  456 | `		}` |
|        7 |  457 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|        7 |  458 | `		PH7_MemObjRelease(&sResult);` |
|        7 |  459 | `		return iVal;` |
|     1021 |  460 | `	}else if(iFlags & MEMOBJ_RES ){` |
|     1021 |  461 | `		return pObj->x.pOther != 0;` |
|        - |  462 | `	}` |
|        - |  463 | `	/* NOT REACHED */` |
|      ! 0 |  464 | `	return 0;` |
|     7053 |  465 | `}` |
|        - |  466 | `/*` |
|        - |  467 | ` * If the ph7_value is of type real,try to make it an integer also.` |
|        - |  468 | ` */` |
|     2876 |  469 | `static sxi32 MemObjTryIntger(ph7_value *pObj)` |
|        5 |  470 | `{` |
|     2881 |  471 | `	pObj->x.iVal = MemObjRealToInt(&(*pObj));` |
|        - |  472 | `  /* Only mark the value as an integer if` |
|        - |  473 | `  **` |
|        - |  474 | `  **    (1) the round-trip conversion real->int->real is a no-op, and` |
|        - |  475 | `  **    (2) The integer is neither the largest nor the smallest` |
|        - |  476 | `  **        possible integer` |
|        - |  477 | `  **` |
|        - |  478 | `  ** The second and third terms in the following conditional enforces` |
|        - |  479 | `  ** the second condition under the assumption that addition overflow causes` |
|        - |  480 | `  ** values to wrap around.  On x86 hardware, the third term is always` |
|        - |  481 | `  ** true and could be omitted.  But we leave it in because other` |
|        - |  482 | `  ** architectures might behave differently.` |
|        - |  483 | `  */` |
|     2876 |  484 | `	if( pObj->rVal ==(ph7_real)pObj->x.iVal && pObj->x.iVal>SMALLEST_INT64` |
|     1520 |  485 | `      && pObj->x.iVal<LARGEST_INT64 ){` |
|     1518 |  486 | `		  pObj->iFlags \|= MEMOBJ_INT;` |
|      759 |  487 | `	}` |
|     2881 |  488 | `	return SXRET_OK;` |
|        5 |  489 | `}` |
|        - |  490 | `/*` |
|        - |  491 | ` * Convert a ph7_value to type integer.Invalidate any prior representations.` |
|        - |  492 | ` */` |
|   411188 |  493 | `PH7_PRIVATE sxi32 PH7_MemObjToInteger(ph7_value *pObj)` |
|        5 |  494 | `{` |
|   411193 |  495 | `	if( (pObj->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  496 | `		/* Preform the conversion */` |
|      507 |  497 | `		pObj->x.iVal = MemObjIntValue(&(*pObj));` |
|        - |  498 | `		/* Invalidate any prior representations */` |
|      507 |  499 | `		SyBlobRelease(&pObj->sBlob);` |
|      507 |  500 | `		MemObjSetType(pObj,MEMOBJ_INT);` |
|      251 |  501 | `	}` |
|   411193 |  502 | `	return SXRET_OK;` |
|        5 |  503 | `}` |
|        - |  504 | `/*` |
|        - |  505 | ` * Convert a ph7_value to type real (Try to get an integer representation also).` |
|        - |  506 | ` * Invalidate any prior representations` |
|        - |  507 | ` */` |
|     2616 |  508 | `PH7_PRIVATE sxi32 PH7_MemObjToReal(ph7_value *pObj)` |
|        5 |  509 | `{` |
|     2621 |  510 | `	if((pObj->iFlags & MEMOBJ_REAL) == 0 ){` |
|        - |  511 | `		/* Preform the conversion */` |
|     1719 |  512 | `		pObj->rVal = MemObjRealValue(&(*pObj));` |
|        - |  513 | `		/* Invalidate any prior representations */` |
|     1719 |  514 | `		SyBlobRelease(&pObj->sBlob);` |
|     1719 |  515 | `		MemObjSetType(pObj,MEMOBJ_REAL);` |
|        - |  516 | `		/* Try to get an integer representation */` |
|     1719 |  517 | `		MemObjTryIntger(&(*pObj));` |
|      857 |  518 | `	}` |
|     2621 |  519 | `	return SXRET_OK;` |
|        5 |  520 | `}` |
|        - |  521 | `/*` |
|        - |  522 | ` * Convert a ph7_value to type boolean.Invalidate any prior representations.` |
|        - |  523 | ` */` |
|    15542 |  524 | `PH7_PRIVATE sxi32 PH7_MemObjToBool(ph7_value *pObj)` |
|        5 |  525 | `{` |
|    15547 |  526 | `	if( (pObj->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        - |  527 | `		/* Preform the conversion */` |
|    14101 |  528 | `		pObj->x.iVal = MemObjBooleanValue(&(*pObj));` |
|        - |  529 | `		/* Invalidate any prior representations */` |
|    14101 |  530 | `		SyBlobRelease(&pObj->sBlob);` |
|    14101 |  531 | `		MemObjSetType(pObj,MEMOBJ_BOOL);` |
|     7048 |  532 | `	}` |
|    15547 |  533 | `	return SXRET_OK;` |
|        5 |  534 | `}` |
|        - |  535 | `/*` |
|        - |  536 | ` * Convert a ph7_value to type string.Prior representations are NOT invalidated.` |
|        - |  537 | ` */` |
|   816001 |  538 | `PH7_PRIVATE sxi32 PH7_MemObjToString(ph7_value *pObj)` |
|        5 |  539 | `{` |
|   816006 |  540 | `	sxi32 rc = SXRET_OK;` |
|   816006 |  541 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  542 | `		/* Perform the conversion */` |
|    56201 |  543 | `		SyBlobReset(&pObj->sBlob); /* Reset the internal buffer */` |
|    56201 |  544 | `		rc = MemObjStringValue(&pObj->sBlob,&(*pObj),TRUE);` |
|    56201 |  545 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|    28098 |  546 | `	}` |
|   816006 |  547 | `	return rc;` |
|        5 |  548 | `}` |
|        - |  549 | `/*` |
|        - |  550 | ` * Nullify a ph7_value.In other words invalidate any prior` |
|        - |  551 | ` * representation.` |
|        - |  552 | ` */` |
|      ! 0 |  553 | `PH7_PRIVATE sxi32 PH7_MemObjToNull(ph7_value *pObj)` |
|      ! 0 |  554 | `{` |
|      ! 0 |  555 | `	return PH7_MemObjRelease(pObj);` |
|      ! 0 |  556 | `}` |
|        - |  557 | `/*` |
|        - |  558 | ` * Convert a ph7_value to type array.Invalidate any prior representations.` |
|        - |  559 | `  * According to the PHP language reference manual.` |
|        - |  560 | `  *   For any of the types: integer, float, string, boolean converting a value` |
|        - |  561 | `  *   to an array results in an array with a single element with index zero` |
|        - |  562 | `  *   and the value of the scalar which was converted.` |
|        - |  563 | `  */` |
|      322 |  564 | `PH7_PRIVATE sxi32 PH7_MemObjToHashmap(ph7_value *pObj)` |
|        5 |  565 | `{` |
|      327 |  566 | `	if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  567 | `		ph7_hashmap *pMap;` |
|        - |  568 | `		/* Allocate a new hashmap instance */` |
|      209 |  569 | `		pMap = PH7_NewHashmap(pObj->pVm,0,0);` |
|      209 |  570 | `		if( pMap == 0 ){` |
|      ! 0 |  571 | `			return SXERR_MEM;` |
|        - |  572 | `		}` |
|      209 |  573 | `		if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|        - |  574 | `			/*` |
|        - |  575 | `			 * According to the PHP language reference manual.` |
|        - |  576 | `			 *   For any of the types: integer, float, string, boolean converting a value` |
|        - |  577 | `			 *   to an array results in an array with a single element with index zero` |
|        - |  578 | `			 *   and the value of the scalar which was converted.` |
|        - |  579 | `			 */` |
|       27 |  580 | `			if( pObj->iFlags & MEMOBJ_OBJ ){` |
|        - |  581 | `				/* Object cast */` |
|       13 |  582 | `				PH7_ClassInstanceToHashmap((ph7_class_instance *)pObj->x.pOther,pMap);` |
|        7 |  583 | `			}else{` |
|        - |  584 | `				/* Insert a single element */` |
|       15 |  585 | `				PH7_HashmapInsert(pMap,0/* Automatic index assign */,&(*pObj));` |
|        - |  586 | `			}` |
|       27 |  587 | `			SyBlobRelease(&pObj->sBlob);` |
|       13 |  588 | `		}` |
|        - |  589 | `		/* Invalidate any prior representation */` |
|      209 |  590 | `		PH7_MemObjRelease(pObj);` |
|      209 |  591 | `		MemObjSetType(pObj,MEMOBJ_HASHMAP);` |
|      209 |  592 | `		pObj->x.pOther = pMap;` |
|      102 |  593 | `	}` |
|      327 |  594 | `	return SXRET_OK;` |
|      166 |  595 | `}` |
|        - |  596 | `/* Per-entry callback for the array branch of the (object) cast: add one dynamic` |
|        - |  597 | ` * property to the target stdClass, named by the array key (rendered as a string,` |
|        - |  598 | ` * matching PHP) and holding a copy of the value. */` |
|        - |  599 | `struct VmObjCastData { ph7_vm *pVm; ph7_class_instance *pStd; };` |
|       38 |  600 | `static int VmArrayToObjectWalk(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  601 | `{` |
|       39 |  602 | `	struct VmObjCastData *pData = (struct VmObjCastData *)pUserData;` |
|        - |  603 | `	ph7_value *pSlot;` |
|        - |  604 | `	/* pKey and pValue are walk-owned temporaries (PH7_HashmapWalk passes pointers to` |
|        - |  605 | `	 * its own stack-local sKey/sValue, not slots inside pVm->aMemObj), so they survive` |
|        - |  606 | `	 * the slot reservation inside PH7_VmCreateDynamicAttr — no snapshot needed. pKey is` |
|        - |  607 | `	 * safe to coerce in place. */` |
|       39 |  608 | `	PH7_MemObjToString(pKey);` |
|       58 |  609 | `	pSlot = PH7_VmCreateDynamicAttr(pData->pVm,pData->pStd,` |
|       38 |  610 | `		(const char *)SyBlobData(&pKey->sBlob),(sxu32)SyBlobLength(&pKey->sBlob),0);` |
|       39 |  611 | `	if( pSlot ){` |
|       39 |  612 | `		PH7_MemObjStore(pValue,pSlot);` |
|       19 |  613 | `	}` |
|       39 |  614 | `	return SXRET_OK;` |
|        1 |  615 | `}` |
|        - |  616 | `/*` |
|        - |  617 | ` * Convert a ph7_value to type object, invalidating any prior representation.` |
|        - |  618 | ` * The new object is a (PHP-empty) stdClass populated with dynamic properties,` |
|        - |  619 | ` * matching PHP's (object) cast:` |
|        - |  620 | ` *   - array  -> one property per entry (key rendered as a string -> name).` |
|        - |  621 | ` *   - scalar -> a single property named "scalar".` |
|        - |  622 | ` *   - null   -> an empty stdClass (no properties).` |
|        - |  623 | ` *   - object -> returned unchanged (the MEMOBJ_OBJ guard below).` |
|        - |  624 | ` */` |
|       34 |  625 | `PH7_PRIVATE sxi32 PH7_MemObjToObject(ph7_value *pObj)` |
|        1 |  626 | `{` |
|       35 |  627 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  628 | `		ph7_class_instance *pStd;` |
|        - |  629 | `		ph7_class *pClass;` |
|        - |  630 | `		ph7_vm *pVm;` |
|        - |  631 | `		/* Point to the underlying VM + the stdClass */` |
|       35 |  632 | `		pVm = pObj->pVm;` |
|       52 |  633 | `		pClass = pVm->pStdClass ? pVm->pStdClass` |
|       17 |  634 | `			: PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,0,0);` |
|       35 |  635 | `		if( pClass == 0 ){` |
|        - |  636 | `			/* Can't happen,load null instead */` |
|      ! 0 |  637 | `			PH7_MemObjRelease(pObj);` |
|      ! 0 |  638 | `			return SXRET_OK;` |
|        - |  639 | `		}` |
|        - |  640 | `		/* Instanciate a new (empty) stdClass object */` |
|       35 |  641 | `		pStd = PH7_NewClassInstance(pVm,pClass);` |
|       35 |  642 | `		if( pStd == 0 ){` |
|        - |  643 | `			/* Out of memory */` |
|      ! 0 |  644 | `			PH7_MemObjRelease(pObj);` |
|      ! 0 |  645 | `			return SXRET_OK;` |
|        - |  646 | `		}` |
|       35 |  647 | `		pStd->iRef = 1;` |
|       35 |  648 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  649 | `			/* Array: one dynamic property per entry. */` |
|        - |  650 | `			struct VmObjCastData sData;` |
|       23 |  651 | `			sData.pVm = pVm;` |
|       23 |  652 | `			sData.pStd = pStd;` |
|       23 |  653 | `			ph7_array_walk(pObj,VmArrayToObjectWalk,&sData);` |
|       24 |  654 | `		}else if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  655 | `			/* Scalar (int/float/bool/string): a single "scalar" property. */` |
|       11 |  656 | `			ph7_value *pSlot = PH7_VmCreateDynamicAttr(pVm,pStd,"scalar",sizeof("scalar")-1,0);` |
|       11 |  657 | `			if( pSlot ){` |
|       11 |  658 | `				PH7_MemObjStore(pObj,pSlot);` |
|        5 |  659 | `			}` |
|        5 |  660 | `		}` |
|        - |  661 | `		/* (A NULL source yields an empty stdClass — nothing to populate.) */` |
|        - |  662 | `		/* Invalidate any prior representation */` |
|       35 |  663 | `		PH7_MemObjRelease(pObj);` |
|        - |  664 | `		/* Save the new instance */` |
|       35 |  665 | `		pObj->x.pOther = pStd;` |
|       35 |  666 | `		MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       17 |  667 | `	}` |
|       35 |  668 | `	return SXRET_OK;` |
|       18 |  669 | `}` |
|        - |  670 | `/*` |
|        - |  671 | ` * Return a pointer to the appropriate convertion method associated` |
|        - |  672 | ` * with the given type.` |
|        - |  673 | ` * Note on type juggling.` |
|        - |  674 | ` * Accoding to the PHP language reference manual` |
|        - |  675 | ` *  PHP does not require (or support) explicit type definition in variable` |
|        - |  676 | ` *  declaration; a variable's type is determined by the context in which` |
|        - |  677 | ` *  the variable is used. That is to say, if a string value is assigned` |
|        - |  678 | ` *  to variable $var, $var becomes a string. If an integer value is then` |
|        - |  679 | ` *  assigned to $var, it becomes an integer.` |
|        - |  680 | ` */` |
|       72 |  681 | `PH7_PRIVATE ProcMemObjCast PH7_MemObjCastMethod(sxi32 iFlags)` |
|        5 |  682 | `{` |
|       77 |  683 | `	if( iFlags & MEMOBJ_STRING ){` |
|       14 |  684 | `		return PH7_MemObjToString;` |
|       65 |  685 | `	}else if( iFlags & MEMOBJ_INT ){` |
|       49 |  686 | `		return PH7_MemObjToInteger;` |
|       19 |  687 | `	}else if( iFlags & MEMOBJ_REAL ){` |
|       16 |  688 | `		return PH7_MemObjToReal;` |
|        3 |  689 | `	}else if( iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  690 | `		return PH7_MemObjToBool;` |
|        3 |  691 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|        3 |  692 | `		return PH7_MemObjToHashmap;` |
|      ! 0 |  693 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  694 | `		return PH7_MemObjToObject;` |
|      ! 0 |  695 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|        - |  696 | ``		/* `null` is a type, not a weak-coercion target: never silently cast a`` |
|        - |  697 | ``		 * value to null for a standalone `null` type hint. Return/property`` |
|        - |  698 | `		 * enforcement reject a non-null value before reaching here; this guards` |
|        - |  699 | `		 * the parameter default-value path from quietly nulling a non-null` |
|        - |  700 | `		 * default. */` |
|      ! 0 |  701 | `		return 0;` |
|        - |  702 | `	}` |
|        - |  703 | `	/* NULL cast */` |
|      ! 0 |  704 | `	return PH7_MemObjToNull;` |
|       41 |  705 | `}` |
|        - |  706 | `/*` |
|        - |  707 | ` * Check whether the ph7_value is numeric [i.e: int/float/bool] or looks` |
|        - |  708 | ` * like a numeric number [i.e: if the ph7_value is of type string.].` |
|        - |  709 | ` * Return TRUE if numeric.FALSE otherwise.` |
|        - |  710 | ` */` |
|   457814 |  711 | `PH7_PRIVATE sxi32 PH7_MemObjIsNumeric(ph7_value *pObj)` |
|        5 |  712 | `{` |
|   457819 |  713 | `	if( pObj->iFlags & ( MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      311 |  714 | `		return TRUE;` |
|   457513 |  715 | `	}else if( pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|      181 |  716 | `		return FALSE;` |
|   457337 |  717 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        - |  718 | `		SyString sStr;` |
|        - |  719 | `		sxi32 rc;` |
|   457337 |  720 | `		SyStringInitFromBuf(&sStr,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|   457337 |  721 | `		if( sStr.nByte <= 0 ){` |
|        - |  722 | `			/* Empty string */` |
|       25 |  723 | `			return FALSE;` |
|        - |  724 | `		}` |
|        - |  725 | `		/* Check if the string representation looks like a numeric number */` |
|   457313 |  726 | `		rc = SyStrIsNumeric(sStr.zString,sStr.nByte,0,0);` |
|   457313 |  727 | `		return rc == SXRET_OK ? TRUE : FALSE;` |
|        - |  728 | `	}` |
|        - |  729 | `	/* NOT REACHED */` |
|      ! 0 |  730 | `	return FALSE;` |
|   228925 |  731 | `}` |
|        - |  732 | `/*` |
|        - |  733 | ` * Check whether the ph7_value is empty.Return TRUE if empty.` |
|        - |  734 | ` * FALSE otherwise.` |
|        - |  735 | ` * An ph7_value is considered empty if the following are true:` |
|        - |  736 | ` * NULL value.` |
|        - |  737 | ` * Boolean FALSE.` |
|        - |  738 | ` * Integer/Float with a 0 (zero) value.` |
|        - |  739 | ` * An empty string or a stream of 0 (zero) [i.e: "0","00","000",...].` |
|        - |  740 | ` * An empty array.` |
|        - |  741 | ` * NOTE` |
|        - |  742 | ` *  OBJECT VALUE MUST NOT BE MODIFIED.` |
|        - |  743 | ` */` |
|    33376 |  744 | `PH7_PRIVATE sxi32 PH7_MemObjIsEmpty(ph7_value *pObj)` |
|        5 |  745 | `{` |
|    33381 |  746 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|       19 |  747 | `		return TRUE;` |
|    33365 |  748 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|       20 |  749 | `		return pObj->x.iVal == 0 ? TRUE : FALSE;` |
|    33347 |  750 | `	}else if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  751 | `		return pObj->rVal == (ph7_real)0 ? TRUE : FALSE;` |
|    33347 |  752 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|        5 |  753 | `		return !pObj->x.iVal;` |
|    33343 |  754 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|    27063 |  755 | `		if( SyBlobLength(&pObj->sBlob) <= 0 ){` |
|    21719 |  756 | `			return TRUE;` |
|      ! 0 |  757 | `		}else{` |
|        - |  758 | `			const char *zIn,*zEnd;` |
|     5349 |  759 | `			zIn = (const char *)SyBlobData(&pObj->sBlob);` |
|     5349 |  760 | `			zEnd = &zIn[SyBlobLength(&pObj->sBlob)];` |
|     5355 |  761 | `			while( zIn < zEnd ){` |
|     5355 |  762 | `				if( zIn[0] != '0' ){` |
|     5349 |  763 | `					break;` |
|        - |  764 | `				}` |
|        7 |  765 | `				zIn++;` |
|        1 |  766 | `			}` |
|     5349 |  767 | `			return zIn >= zEnd ? TRUE : FALSE;` |
|      ! 0 |  768 | `		}` |
|     6285 |  769 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|     6285 |  770 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|     6285 |  771 | `		return pMap->nEntry == 0 ? TRUE : FALSE;` |
|      ! 0 |  772 | `	}else if ( pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|      ! 0 |  773 | `		return FALSE;` |
|        - |  774 | `	}` |
|        - |  775 | `	/* Assume empty by default */` |
|      ! 0 |  776 | `	return TRUE;` |
|    16693 |  777 | `}` |
|        - |  778 | `/*` |
|        - |  779 | ` * Convert a ph7_value so that it has types MEMOBJ_REAL or MEMOBJ_INT` |
|        - |  780 | ` * or both.` |
|        - |  781 | ` * Invalidate any prior representations. Every effort is made to force` |
|        - |  782 | ` * the conversion, even if the input is a string that does not look` |
|        - |  783 | ` * completely like a number.Convert as much of the string as we can` |
|        - |  784 | ` * and ignore the rest.` |
|        - |  785 | ` */` |
|   447189 |  786 | `PH7_PRIVATE sxi32 PH7_MemObjToNumeric(ph7_value *pObj)` |
|        5 |  787 | `{` |
|   447194 |  788 | `	if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|   447078 |  789 | `		if( pObj->iFlags & (MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|        3 |  790 | `			if( pObj->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  791 | `				pObj->x.iVal = 0;` |
|      ! 0 |  792 | `			}` |
|        3 |  793 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|        1 |  794 | `		}` |
|        - |  795 | `		/* Already numeric */` |
|   447078 |  796 | `		return  SXRET_OK;` |
|        - |  797 | `	}` |
|      117 |  798 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|      117 |  799 | `		sxi32 rc = SXERR_INVALID;` |
|      117 |  800 | `		sxu8 bReal = FALSE;` |
|        - |  801 | `		SyString sString;` |
|      117 |  802 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|        - |  803 | `		/* Check if the given string looks like a numeric number */` |
|      117 |  804 | `		if( sString.nByte > 0 ){` |
|      115 |  805 | `			rc = SyStrIsNumeric(sString.zString,sString.nByte,&bReal,0);` |
|       57 |  806 | `		}` |
|      117 |  807 | `		if( bReal ){` |
|        7 |  808 | `			PH7_MemObjToReal(&(*pObj));` |
|        4 |  809 | `		}else{` |
|      111 |  810 | `			if( rc != SXRET_OK ){` |
|        - |  811 | `				/* The input does not look at all like a number,set the value to 0 */` |
|        3 |  812 | `				pObj->x.iVal = 0;` |
|        2 |  813 | `			}else{` |
|        - |  814 | `				/* Convert as much as we can */` |
|      109 |  815 | `				pObj->x.iVal = MemObjStringToInt(&(*pObj));` |
|        - |  816 | `			}` |
|      111 |  817 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|      111 |  818 | `			SyBlobRelease(&pObj->sBlob);` |
|        1 |  819 | `		}` |
|       58 |  820 | `	}else if(pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)){` |
|      ! 0 |  821 | `		PH7_MemObjToInteger(pObj);` |
|      ! 0 |  822 | `	}else{` |
|        - |  823 | `		/* Perform a blind cast */` |
|      ! 0 |  824 | `		PH7_MemObjToReal(&(*pObj));` |
|        - |  825 | `	}` |
|      117 |  826 | `	return SXRET_OK;` |
|   223643 |  827 | `}` |
|        - |  828 | `/*` |
|        - |  829 | ` * Apply Perl-style increment to a string ph7_value in place.` |
|        - |  830 | ` * Walks the bytes right-to-left: digits 0-8 / letters a-y, A-Y bump in` |
|        - |  831 | ` * place; '9' wraps to '0' with carry; 'z' to 'a'; 'Z' to 'A'. A non-` |
|        - |  832 | ` * alphanumeric byte stops the walk without prepending. If carry survives` |
|        - |  833 | ` * past index 0, prepend '1', 'a', or 'A' depending on the class of the` |
|        - |  834 | ` * last carried character. Empty strings become "1".` |
|        - |  835 | ` *` |
|        - |  836 | ` * Caller must ensure pObj is MEMOBJ_STRING and not a numeric string;` |
|        - |  837 | ` * this routine never reclassifies the type, so a result like "e0" stays` |
|        - |  838 | ` * a string even though it looks numeric.` |
|        - |  839 | ` */` |
|       48 |  840 | `PH7_PRIVATE sxi32 PH7_MemObjStringIncrement(ph7_value *pObj)` |
|        1 |  841 | `{` |
|        - |  842 | `	enum CarryClass { CARRY_NONE = 0, CARRY_LOWER, CARRY_UPPER, CARRY_DIGIT };` |
|       49 |  843 | `	enum CarryClass last_class = CARRY_NONE;` |
|        - |  844 | `	sxu32 nLen, pos;` |
|        - |  845 | `	sxu8 *zStr;` |
|       49 |  846 | `	int carry = 1;` |
|        - |  847 | `	int ch;` |
|        - |  848 | `	/* Force ownership: the blob may be SXBLOB_RDONLY (e.g., from` |
|        - |  849 | `	 * PH7_MemObjLoad), in which case BlobPrepareGrow copies on demand` |
|        - |  850 | `	 * and clears the flag.  On an already-owned blob with spare capacity` |
|        - |  851 | `	 * (the common case under PHL's growth allocator), this is a no-op` |
|        - |  852 | `	 * append; on an exact-fit owned blob it triggers a single realloc. */` |
|       49 |  853 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       45 |  854 | `		SyBlobNullAppend(&pObj->sBlob);` |
|       22 |  855 | `	}` |
|       49 |  856 | `	nLen = SyBlobLength(&pObj->sBlob);` |
|       49 |  857 | `	if( nLen == 0 ){` |
|        5 |  858 | `		SyBlobAppend(&pObj->sBlob,"1",sizeof(char));` |
|        5 |  859 | `		return SXRET_OK;` |
|        - |  860 | `	}` |
|       45 |  861 | `	zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|       45 |  862 | `	pos = nLen;` |
|       97 |  863 | `	while( pos > 0 ){` |
|       79 |  864 | `		pos--;` |
|       79 |  865 | `		ch = zStr[pos];` |
|       79 |  866 | `		if( ch >= 'a' && ch <= 'z' ){` |
|       45 |  867 | `			if( ch == 'z' ){` |
|       29 |  868 | `				zStr[pos] = 'a';` |
|       29 |  869 | `				last_class = CARRY_LOWER;` |
|       29 |  870 | `				continue;` |
|        - |  871 | `			}` |
|       17 |  872 | `			zStr[pos]++;` |
|       17 |  873 | `			carry = 0;` |
|       17 |  874 | `			break;` |
|       35 |  875 | `		}else if( ch >= 'A' && ch <= 'Z' ){` |
|       21 |  876 | `			if( ch == 'Z' ){` |
|       19 |  877 | `				zStr[pos] = 'A';` |
|       19 |  878 | `				last_class = CARRY_UPPER;` |
|       19 |  879 | `				continue;` |
|        - |  880 | `			}` |
|        3 |  881 | `			zStr[pos]++;` |
|        3 |  882 | `			carry = 0;` |
|        3 |  883 | `			break;` |
|       15 |  884 | `		}else if( ch >= '0' && ch <= '9' ){` |
|        7 |  885 | `			if( ch == '9' ){` |
|        7 |  886 | `				zStr[pos] = '0';` |
|        7 |  887 | `				last_class = CARRY_DIGIT;` |
|        7 |  888 | `				continue;` |
|        - |  889 | `			}` |
|      ! 0 |  890 | `			zStr[pos]++;` |
|      ! 0 |  891 | `			carry = 0;` |
|      ! 0 |  892 | `			break;` |
|      ! 0 |  893 | `		}else{` |
|        - |  894 | `			/* non-alphanumeric: stop without prepending */` |
|        9 |  895 | `			carry = 0;` |
|        9 |  896 | `			break;` |
|        - |  897 | `		}` |
|      ! 0 |  898 | `	}` |
|       45 |  899 | `	if( carry ){` |
|        - |  900 | `		sxu8 prepend;` |
|        - |  901 | `		sxu32 i;` |
|       19 |  902 | `		switch( last_class ){` |
|        9 |  903 | `			case CARRY_LOWER: prepend = (sxu8)'a'; break;` |
|       11 |  904 | `			case CARRY_UPPER: prepend = (sxu8)'A'; break;` |
|      ! 0 |  905 | `			default:          prepend = (sxu8)'1'; break;` |
|        - |  906 | `		}` |
|        - |  907 | `		/* Append a sentinel byte to grow nByte by 1 (capacity grows too). */` |
|       19 |  908 | `		SyBlobAppend(&pObj->sBlob,"\0",sizeof(char));` |
|       19 |  909 | `		zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|       19 |  910 | `		nLen = SyBlobLength(&pObj->sBlob);` |
|        - |  911 | `		/* Shift right by 1, walking from the end so overlapping is safe. */` |
|       57 |  912 | `		for( i = nLen - 1; i > 0; i-- ){` |
|       39 |  913 | `			zStr[i] = zStr[i - 1];` |
|       20 |  914 | `		}` |
|       19 |  915 | `		zStr[0] = prepend;` |
|        9 |  916 | `	}` |
|       45 |  917 | `	return SXRET_OK;` |
|       25 |  918 | `}` |
|        - |  919 | `/*` |
|        - |  920 | ` * Try a get an integer representation of the given ph7_value.` |
|        - |  921 | ` * If the ph7_value is not of type real,this function is a no-op.` |
|        - |  922 | ` */` |
|     1098 |  923 | `PH7_PRIVATE sxi32 PH7_MemObjTryInteger(ph7_value *pObj)` |
|        1 |  924 | `{` |
|     1099 |  925 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|        - |  926 | `		/* Work only with reals */` |
|     1099 |  927 | `		MemObjTryIntger(&(*pObj));` |
|      549 |  928 | `	}` |
|     1099 |  929 | `	return SXRET_OK;` |
|        1 |  930 | `}` |
|        - |  931 | `/*` |
|        - |  932 | ` * Initialize a ph7_value to the null type.` |
|        - |  933 | ` */` |
|  8627245 |  934 | `PH7_PRIVATE sxi32 PH7_MemObjInit(ph7_vm *pVm,ph7_value *pObj)` |
|        5 |  935 | `{` |
|        - |  936 | `	/* Zero the structure */` |
|  8627250 |  937 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - |  938 | `	/* Initialize fields */` |
|  8627250 |  939 | `	pObj->pVm = pVm;` |
|  8627250 |  940 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - |  941 | `	/* Set the NULL type */` |
|  8627250 |  942 | `	pObj->iFlags = MEMOBJ_NULL;` |
|  8627250 |  943 | `	return SXRET_OK;` |
|        5 |  944 | `}` |
|        - |  945 | `/*` |
|        - |  946 | ` * Initialize a ph7_value to the integer type.` |
|        - |  947 | ` */` |
|   167836 |  948 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromInt(ph7_vm *pVm,ph7_value *pObj,sxi64 iVal)` |
|        5 |  949 | `{` |
|        - |  950 | `	/* Zero the structure */` |
|   167841 |  951 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - |  952 | `	/* Initialize fields */` |
|   167841 |  953 | `	pObj->pVm = pVm;` |
|   167841 |  954 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - |  955 | `	/* Set the desired type */` |
|   167841 |  956 | `	pObj->x.iVal = iVal;` |
|   167841 |  957 | `	pObj->iFlags = MEMOBJ_INT;` |
|   167841 |  958 | `	return SXRET_OK;` |
|        5 |  959 | `}` |
|        - |  960 | `/*` |
|        - |  961 | ` * Initialize a ph7_value to the boolean type.` |
|        - |  962 | ` */` |
|    17186 |  963 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromBool(ph7_vm *pVm,ph7_value *pObj,sxi32 iVal)` |
|        5 |  964 | `{` |
|        - |  965 | `	/* Zero the structure */` |
|    17191 |  966 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - |  967 | `	/* Initialize fields */` |
|    17191 |  968 | `	pObj->pVm = pVm;` |
|    17191 |  969 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - |  970 | `	/* Set the desired type */` |
|    17191 |  971 | `	pObj->x.iVal = iVal ? 1 : 0;` |
|    17191 |  972 | `	pObj->iFlags = MEMOBJ_BOOL;` |
|    17191 |  973 | `	return SXRET_OK;` |
|        5 |  974 | `}` |
|        - |  975 | `#if 0` |
|        - |  976 | `/*` |
|        - |  977 | ` * Initialize a ph7_value to the real type.` |
|        - |  978 | ` */` |
|        - |  979 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromReal(ph7_vm *pVm,ph7_value *pObj,ph7_real rVal)` |
|        - |  980 | `{` |
|        - |  981 | `	/* Zero the structure */` |
|        - |  982 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - |  983 | `	/* Initialize fields */` |
|        - |  984 | `	pObj->pVm = pVm;` |
|        - |  985 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - |  986 | `	/* Set the desired type */` |
|        - |  987 | `	pObj->rVal = rVal;` |
|        - |  988 | `	pObj->iFlags = MEMOBJ_REAL;` |
|        - |  989 | `	return SXRET_OK;` |
|        - |  990 | `}` |
|        - |  991 | `#endif` |
|        - |  992 | `/*` |
|        - |  993 | ` * Initialize a ph7_value to the array type.` |
|        - |  994 | ` */` |
|    51396 |  995 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromArray(ph7_vm *pVm,ph7_value *pObj,ph7_hashmap *pArray)` |
|        5 |  996 | `{` |
|        - |  997 | `	/* Zero the structure */` |
|    51401 |  998 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - |  999 | `	/* Initialize fields */` |
|    51401 | 1000 | `	pObj->pVm = pVm;` |
|    51401 | 1001 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1002 | `	/* Set the desired type */` |
|    51401 | 1003 | `	pObj->iFlags = MEMOBJ_HASHMAP;` |
|    51401 | 1004 | `	pObj->x.pOther = pArray;` |
|    51401 | 1005 | `	return SXRET_OK;` |
|        5 | 1006 | `}` |
|        - | 1007 | `/*` |
|        - | 1008 | ` * Initialize a ph7_value to the string type.` |
|        - | 1009 | ` */` |
|   608652 | 1010 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromString(ph7_vm *pVm,ph7_value *pObj,const SyString *pVal)` |
|        5 | 1011 | `{` |
|        - | 1012 | `	/* Zero the structure */` |
|   608657 | 1013 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1014 | `	/* Initialize fields */` |
|   608657 | 1015 | `	pObj->pVm = pVm;` |
|   608657 | 1016 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|   608657 | 1017 | `	if( pVal ){` |
|        - | 1018 | `		/* Append contents */` |
|   345859 | 1019 | `		SyBlobAppend(&pObj->sBlob,(const void *)pVal->zString,pVal->nByte);` |
|   172927 | 1020 | `	}` |
|        - | 1021 | `	/* Set the desired type */` |
|   608657 | 1022 | `	pObj->iFlags = MEMOBJ_STRING;` |
|   608657 | 1023 | `	return SXRET_OK;` |
|        5 | 1024 | `}` |
|        - | 1025 | `/*` |
|        - | 1026 | ` * Append some contents to the internal buffer of a given ph7_value.` |
|        - | 1027 | ` * If the given ph7_value is not of type string,this function` |
|        - | 1028 | ` * invalidate any prior representation and set the string type.` |
|        - | 1029 | ` * Then a simple append operation is performed.` |
|        - | 1030 | ` */` |
|   447120 | 1031 | `PH7_PRIVATE sxi32 PH7_MemObjStringAppend(ph7_value *pObj,const char *zData,sxu32 nLen)` |
|        5 | 1032 | `{` |
|        - | 1033 | `	sxi32 rc;` |
|   447125 | 1034 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 1035 | `		/* Invalidate any prior representation */` |
|     1943 | 1036 | `		PH7_MemObjRelease(pObj);` |
|     1943 | 1037 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|      969 | 1038 | `	}` |
|        - | 1039 | `	/* Append contents */` |
|   447125 | 1040 | `	rc = SyBlobAppend(&pObj->sBlob,zData,nLen);` |
|   447125 | 1041 | `	return rc;` |
|        5 | 1042 | `}` |
|        - | 1043 | `#if 0` |
|        - | 1044 | `/*` |
|        - | 1045 | ` * Format and append some contents to the internal buffer of a given ph7_value.` |
|        - | 1046 | ` * If the given ph7_value is not of type string,this function invalidate` |
|        - | 1047 | ` * any prior representation and set the string type.` |
|        - | 1048 | ` * Then a simple format and append operation is performed.` |
|        - | 1049 | ` */` |
|        - | 1050 | `PH7_PRIVATE sxi32 PH7_MemObjStringFormat(ph7_value *pObj,const char *zFormat,va_list ap)` |
|        - | 1051 | `{` |
|        - | 1052 | `	sxi32 rc;` |
|        - | 1053 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 1054 | `		/* Invalidate any prior representation */` |
|        - | 1055 | `		PH7_MemObjRelease(pObj);` |
|        - | 1056 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|        - | 1057 | `	}` |
|        - | 1058 | `	/* Format and append contents */` |
|        - | 1059 | `	rc = SyBlobFormatAp(&pObj->sBlob,zFormat,ap);` |
|        - | 1060 | `	return rc;` |
|        - | 1061 | `}` |
|        - | 1062 | `#endif` |
|        - | 1063 | `/*` |
|        - | 1064 | ` * Duplicate the contents of a ph7_value.` |
|        - | 1065 | ` */` |
|  4770021 | 1066 | `PH7_PRIVATE sxi32 PH7_MemObjStore(ph7_value *pSrc,ph7_value *pDest)` |
|        5 | 1067 | `{` |
|  4770026 | 1068 | `	ph7_class_instance *pObj = 0;` |
|  4770026 | 1069 | `	ph7_hashmap *pMap = 0;` |
|        - | 1070 | `	sxi32 rc;` |
|  4770026 | 1071 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1072 | `		/* Increment reference count */` |
|   178639 | 1073 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  4680709 | 1074 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|        - | 1075 | `		/* Increment reference count */` |
|     3469 | 1076 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|     1732 | 1077 | `	}` |
|  4770026 | 1078 | `	if( pDest->iFlags & MEMOBJ_HASHMAP ){` |
|    58487 | 1079 | `		pMap = (ph7_hashmap *)pDest->x.pOther;` |
|  4740785 | 1080 | `	}else if( pDest->iFlags & MEMOBJ_OBJ ){` |
|     5033 | 1081 | `		pObj = (ph7_class_instance *)pDest->x.pOther;` |
|     2514 | 1082 | `	}` |
|  4770026 | 1083 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|  4770026 | 1084 | `	pDest->iFlags &= ~MEMOBJ_AUX;` |
|  4770026 | 1085 | `	rc = SXRET_OK;` |
|  4770026 | 1086 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  3922497 | 1087 | `		SyBlobReset(&pDest->sBlob);` |
|  3922497 | 1088 | `		rc = SyBlobDup(&pSrc->sBlob,&pDest->sBlob);` |
|  1961251 | 1089 | `	}else{` |
|   847534 | 1090 | `		if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|   277832 | 1091 | `			SyBlobRelease(&pDest->sBlob);` |
|   138957 | 1092 | `		}` |
|        - | 1093 | `	}` |
|  4770026 | 1094 | `	if( pMap ){` |
|    58487 | 1095 | `		PH7_HashmapUnref(pMap);` |
|  4740785 | 1096 | `	}else if( pObj ){` |
|     5033 | 1097 | `		PH7_ClassInstanceUnref(pObj);` |
|     2514 | 1098 | `	}` |
|  4770021 | 1099 | `	if( rc == SXRET_OK && (pDest->iFlags & MEMOBJ_HASHMAP)` |
|  2474371 | 1100 | `	 && pDest->pVm` |
|   178634 | 1101 | `	 && (ph7_hashmap *)pDest->x.pOther == pDest->pVm->pGlobal` |
|        - | 1102 | `	 /* Identity, not nIdx: transient values carry nIdx==0 (SyZero), which` |
|        - | 1103 | `	  * collides with a typical nGlobalIdx of 0 and would skip the snapshot` |
|        - | 1104 | `	  * for closure envs and other non-slot destinations. */` |
|    89326 | 1105 | `	 && pDest != (ph7_value *)SySetAt(&pDest->pVm->aMemObj,pDest->pVm->nGlobalIdx) ){` |
|        - | 1106 | `		/* php 8.1: a COPY of $GLOBALS ($snap = $GLOBALS, $a[] = $GLOBALS,` |
|        - | 1107 | `		 * by-value argument passing, return $GLOBALS, ...) is a by-value` |
|        - | 1108 | `		 * SNAPSHOT of the symbol table with its reference entries` |
|        - | 1109 | `		 * flattened — never a live alias. Materialize it here, the one` |
|        - | 1110 | `		 * store choke point (loads/subscript access keep sharing, so` |
|        - | 1111 | `		 * $GLOBALS[$k] reads and writes stay live). */` |
|        9 | 1112 | `		ph7_hashmap *pSnap = PH7_NewHashmap(pDest->pVm,0,0);` |
|        9 | 1113 | `		if( pSnap && PH7_HashmapDupMaterialized((ph7_hashmap *)pDest->x.pOther,pSnap) == SXRET_OK ){` |
|        9 | 1114 | `			PH7_HashmapUnref((ph7_hashmap *)pDest->x.pOther);` |
|        9 | 1115 | `			pDest->x.pOther = pSnap;` |
|        4 | 1116 | `		}else if( pSnap ){` |
|      ! 0 | 1117 | `			PH7_HashmapUnref(pSnap);` |
|      ! 0 | 1118 | `		}` |
|        4 | 1119 | `	}` |
|  4770026 | 1120 | `	return rc;` |
|        5 | 1121 | `}` |
|        - | 1122 | `/*` |
|        - | 1123 | ` * Duplicate the contents of a ph7_value but do not copy internal` |
|        - | 1124 | ` * buffer contents,simply point to it.` |
|        - | 1125 | ` */` |
|  6587022 | 1126 | `PH7_PRIVATE sxi32 PH7_MemObjLoad(ph7_value *pSrc,ph7_value *pDest)` |
|        5 | 1127 | `{` |
|  6587027 | 1128 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),` |
|        - | 1129 | `		sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|  6587027 | 1130 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1131 | `		/* Increment reference count */` |
|   463065 | 1132 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  6355497 | 1133 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|        - | 1134 | `		/* Increment reference count */` |
|    23225 | 1135 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|    11610 | 1136 | `	}` |
|  6587027 | 1137 | `	if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|       40 | 1138 | `		SyBlobRelease(&pDest->sBlob);` |
|       18 | 1139 | `	}` |
|  6587027 | 1140 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  3554259 | 1141 | `		SyBlobReadOnly(&pDest->sBlob,SyBlobData(&pSrc->sBlob),SyBlobLength(&pSrc->sBlob));` |
|  1777227 | 1142 | `	}` |
|  6587027 | 1143 | `	return SXRET_OK;` |
|        5 | 1144 | `}` |
|        - | 1145 | `/*` |
|        - | 1146 | ` * Invalidate any prior representation of a given ph7_value.` |
|        - | 1147 | ` */` |
| 14398075 | 1148 | `PH7_PRIVATE sxi32 PH7_MemObjRelease(ph7_value *pObj)` |
|        5 | 1149 | `{` |
| 14398080 | 1150 | `	if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
| 11886829 | 1151 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|   603187 | 1152 | `			PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
| 11585238 | 1153 | `		}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|    39493 | 1154 | `			PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|    19744 | 1155 | `		}` |
|        - | 1156 | `		/* Release the internal buffer */` |
| 11886829 | 1157 | `		SyBlobRelease(&pObj->sBlob);` |
|        - | 1158 | `		/* Invalidate any prior representation */` |
| 11886829 | 1159 | `		pObj->iFlags = MEMOBJ_NULL;` |
|  5943771 | 1160 | `	}` |
| 14398080 | 1161 | `	return SXRET_OK;` |
|        5 | 1162 | `}` |
|        - | 1163 | `/*` |
|        - | 1164 | ` * Compare two ph7_values.` |
|        - | 1165 | ` * Return 0 if the values are equals, > 0 if pObj1 is greater than pObj2` |
|        - | 1166 | ` * or < 0 if pObj2 is greater than pObj1.` |
|        - | 1167 | ` * Type comparison table taken from the PHP language reference manual.` |
|        - | 1168 | ` * Comparisons of $x with PHP functions Expression` |
|        - | 1169 | ` *              gettype() 	empty() 	is_null() 	isset() 	boolean : if($x)` |
|        - | 1170 | ` * $x = ""; 	string 	    TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1171 | ` * $x = null 	NULL 	    TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1172 | ` * var $x; 	    NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1173 | ` * $x is undefined 	NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1174 | ` *  $x = array(); 	array 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1175 | ` * $x = false; 	boolean 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1176 | ` * $x = true; 	boolean 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1177 | ` * $x = 1; 	    integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1178 | ` * $x = 42; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1179 | ` * $x = 0; 	    integer 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1180 | ` * $x = -1; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1181 | ` * $x = "1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1182 | ` * $x = "0"; 	string 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1183 | ` * $x = "-1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1184 | ` * $x = "php"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1185 | ` * $x = "true"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1186 | ` * $x = "false"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1187 | ` *      Loose comparisons with ==` |
|        - | 1188 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|        - | 1189 | ` * TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1190 | ` * FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|        - | 1191 | ` * 1 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1192 | ` * 0 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE` |
|        - | 1193 | ` * -1 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1194 | ` * "1" 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1195 | ` * "0" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1196 | ` * "-1" 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1197 | ` * NULL 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|        - | 1198 | ` * array() 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1199 | ` * "php" 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1200 | ` * "" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE` |
|        - | 1201 | ` *    Strict comparisons with ===` |
|        - | 1202 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|        - | 1203 | ` * TRUE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1204 | ` * FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1205 | ` * 1 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1206 | ` * 0 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1207 | ` * -1 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1208 | ` * "1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1209 | ` * "0" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1210 | ` * "-1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1211 | ` * NULL 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE` |
|        - | 1212 | ` * array() 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE` |
|        - | 1213 | ` * "php" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1214 | ` * "" 	    FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE` |
|        - | 1215 | ` */` |
|  1255182 | 1216 | `PH7_PRIVATE sxi32 PH7_MemObjCmp(ph7_value *pObj1,ph7_value *pObj2,int bStrict,int iNest)` |
|        5 | 1217 | `{` |
|        - | 1218 | `	sxi32 iComb;` |
|        - | 1219 | `	sxi32 rc;` |
|  1255187 | 1220 | `	if( bStrict ){` |
|        - | 1221 | `		sxi32 iF1,iF2;` |
|        - | 1222 | `		/* Strict comparisons with === */` |
|   646618 | 1223 | `		iF1 = pObj1->iFlags&~MEMOBJ_AUX;` |
|   646618 | 1224 | `		iF2 = pObj2->iFlags&~MEMOBJ_AUX;` |
|   646618 | 1225 | `		if( iF1 != iF2 ){` |
|        - | 1226 | `			/* Not of the same type */` |
|   181391 | 1227 | `			return 1;` |
|        - | 1228 | `		}` |
|   232613 | 1229 | `	}` |
|        - | 1230 | `	/* Combine flag together */` |
|  1073801 | 1231 | `	iComb = pObj1->iFlags\|pObj2->iFlags;` |
|  1073801 | 1232 | `	if( iComb & (MEMOBJ_NULL\|MEMOBJ_RES\|MEMOBJ_BOOL) ){` |
|        - | 1233 | `		/* Convert to boolean: Keep in mind FALSE < TRUE */` |
|    20851 | 1234 | `		if( (pObj1->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     7401 | 1235 | `			PH7_MemObjToBool(pObj1);` |
|     3698 | 1236 | `		}` |
|    20851 | 1237 | `		if( (pObj2->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     6435 | 1238 | `			PH7_MemObjToBool(pObj2);` |
|     3215 | 1239 | `		}` |
|    20851 | 1240 | `		return (sxi32)((pObj1->x.iVal != 0) - (pObj2->x.iVal != 0));` |
|  1052955 | 1241 | `	}else if ( iComb & MEMOBJ_HASHMAP ){` |
|        - | 1242 | `		/* Hashmap aka 'array' comparison */` |
|       29 | 1243 | `		if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1244 | `			/* Array is always greater */` |
|      ! 0 | 1245 | `			return -1;` |
|        - | 1246 | `		}` |
|       29 | 1247 | `		if( (pObj2->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1248 | `			/* Array is always greater */` |
|      ! 0 | 1249 | `			return 1;` |
|        - | 1250 | `		}` |
|        - | 1251 | `		/* Perform the comparison */` |
|       29 | 1252 | `		rc = PH7_HashmapCmp((ph7_hashmap *)pObj1->x.pOther,(ph7_hashmap *)pObj2->x.pOther,bStrict);` |
|       29 | 1253 | `		return rc;` |
|  1052927 | 1254 | `	}else if(iComb & MEMOBJ_OBJ ){` |
|        - | 1255 | `		/* Object comparison */` |
|      229 | 1256 | `		if( (pObj1->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 1257 | `			/* Object is always greater */` |
|      ! 0 | 1258 | `			return -1;` |
|        - | 1259 | `		}` |
|      229 | 1260 | `		if( (pObj2->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 1261 | `			/* Object is always greater */` |
|      ! 0 | 1262 | `			return 1;` |
|        - | 1263 | `		}` |
|        - | 1264 | `		/* Perform the comparison */` |
|      229 | 1265 | `		rc = PH7_ClassInstanceCmp((ph7_class_instance *)pObj1->x.pOther,(ph7_class_instance *)pObj2->x.pOther,bStrict,iNest);` |
|      229 | 1266 | `		return rc;` |
|  1052703 | 1267 | `	}else if ( iComb & MEMOBJ_STRING ){` |
|        - | 1268 | `		SyString s1,s2;` |
|   672228 | 1269 | `		if( !bStrict ){` |
|        - | 1270 | `			/*` |
|        - | 1271 | `			 * According to the PHP language reference manual:` |
|        - | 1272 | `			 *` |
|        - | 1273 | `			 *  If you compare a number with a string or the comparison involves numerical` |
|        - | 1274 | `			 *  strings, then each string is converted to a number and the comparison` |
|        - | 1275 | `			 *  performed numerically.` |
|        - | 1276 | `			 */` |
|   228614 | 1277 | `			if( PH7_MemObjIsNumeric(pObj1) ){` |
|        - | 1278 | `				/* Perform a numeric comparison */` |
|       17 | 1279 | `				goto Numeric;` |
|        - | 1280 | `			}` |
|   228598 | 1281 | `			if( PH7_MemObjIsNumeric(pObj2) ){` |
|        - | 1282 | `				/* Perform a numeric comparison */` |
|      ! 0 | 1283 | `				goto Numeric;` |
|        - | 1284 | `			}` |
|   114303 | 1285 | `		}` |
|        - | 1286 | `		/* Perform a strict string comparison.*/` |
|   672212 | 1287 | `		if( (pObj1->iFlags&MEMOBJ_STRING) == 0 ){` |
|      ! 0 | 1288 | `			PH7_MemObjToString(pObj1);` |
|      ! 0 | 1289 | `		}` |
|   672212 | 1290 | `		if( (pObj2->iFlags&MEMOBJ_STRING) == 0 ){` |
|      ! 0 | 1291 | `			PH7_MemObjToString(pObj2);` |
|      ! 0 | 1292 | `		}` |
|   672212 | 1293 | `		SyStringInitFromBuf(&s1,SyBlobData(&pObj1->sBlob),SyBlobLength(&pObj1->sBlob));` |
|   672212 | 1294 | `		SyStringInitFromBuf(&s2,SyBlobData(&pObj2->sBlob),SyBlobLength(&pObj2->sBlob));` |
|        - | 1295 | `		/*` |
|        - | 1296 | `		 * Strings are compared using memcmp(). If one value is an exact prefix of the` |
|        - | 1297 | `		 * other, then the shorter value is less than the longer value.` |
|        - | 1298 | `		 */` |
|   672212 | 1299 | `		rc = SyMemcmp((const void *)s1.zString,(const void *)s2.zString,SXMIN(s1.nByte,s2.nByte));` |
|   672212 | 1300 | `		if( rc == 0 ){` |
|   230710 | 1301 | `			if( s1.nByte != s2.nByte ){` |
|     1662 | 1302 | `				rc = s1.nByte < s2.nByte ? -1 : 1;` |
|      831 | 1303 | `			}` |
|   115355 | 1304 | `		}` |
|   672212 | 1305 | `		return rc;` |
|   380480 | 1306 | `	}else if( iComb & (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|   190195 | 1307 | `Numeric:` |
|        - | 1308 | `		/* Perform a numeric comparison if one of the operand is numeric(integer or real) */` |
|   380496 | 1309 | `		if( (pObj1->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|        5 | 1310 | `			PH7_MemObjToNumeric(pObj1);` |
|        2 | 1311 | `		}` |
|   380496 | 1312 | `		if( (pObj2->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|       15 | 1313 | `			PH7_MemObjToNumeric(pObj2);` |
|        7 | 1314 | `		}` |
|   380496 | 1315 | `		if( (pObj1->iFlags & pObj2->iFlags & MEMOBJ_INT) == 0) {` |
|        - | 1316 | `			/*` |
|        - | 1317 | `			 * Symisc eXtension to the PHP language:` |
|        - | 1318 | `			 *  Floating point comparison is introduced and works as expected.` |
|        - | 1319 | `			 */` |
|        - | 1320 | `			ph7_real r1,r2;` |
|        - | 1321 | `			/* Compare as reals */` |
|      233 | 1322 | `			if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|       11 | 1323 | `				PH7_MemObjToReal(pObj1);` |
|        5 | 1324 | `			}` |
|      233 | 1325 | `			r1 = pObj1->rVal;` |
|      233 | 1326 | `			if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|       37 | 1327 | `				PH7_MemObjToReal(pObj2);` |
|       18 | 1328 | `			}` |
|      233 | 1329 | `			r2 = pObj2->rVal;` |
|      233 | 1330 | `			if( PH7_IS_NAN(r1) \|\| PH7_IS_NAN(r2) ){` |
|        - | 1331 | `				/*` |
|        - | 1332 | `				 * Keep a strict three-way comparator contract even for NaN values.` |
|        - | 1333 | `				 * For ordering purposes, NaN compares equal to NaN and greater than` |
|        - | 1334 | `				 * any non-NaN numeric value.` |
|        - | 1335 | `				 */` |
|       45 | 1336 | `				if( PH7_IS_NAN(r1) ){` |
|       35 | 1337 | `					return PH7_IS_NAN(r2) ? 0 : 1;` |
|        - | 1338 | `				}` |
|       11 | 1339 | `				return -1;` |
|        - | 1340 | `			}` |
|      189 | 1341 | `			if( r1 > r2 ){` |
|       35 | 1342 | `				return 1;` |
|      155 | 1343 | `			}else if( r1 < r2 ){` |
|      121 | 1344 | `				return -1;` |
|        - | 1345 | `			}` |
|       35 | 1346 | `			return 0;` |
|      ! 0 | 1347 | `		}else{` |
|        - | 1348 | `			/* Integer comparison */` |
|   380264 | 1349 | `			if( pObj1->x.iVal > pObj2->x.iVal ){` |
|     6698 | 1350 | `				return 1;` |
|   373571 | 1351 | `			}else if( pObj1->x.iVal < pObj2->x.iVal ){` |
|   367632 | 1352 | `				return -1;` |
|        - | 1353 | `			}` |
|     5944 | 1354 | `			return 0;` |
|        - | 1355 | `		}` |
|        - | 1356 | `	}` |
|        - | 1357 | `	/* NOT REACHED */` |
|      ! 0 | 1358 | `	return 0;` |
|   627645 | 1359 | `}` |
|        - | 1360 | `/*` |
|        - | 1361 | ` * Perform an addition operation of two ph7_values.` |
|        - | 1362 | ` * The reason this function is implemented here rather than 'vm.c'` |
|        - | 1363 | ` * is that the '+' operator is overloaded.` |
|        - | 1364 | ` * That is,the '+' operator is used for arithmetic operation and also` |
|        - | 1365 | ` * used for operation on arrays [i.e: union]. When used with an array` |
|        - | 1366 | ` * The + operator returns the right-hand array appended to the left-hand array.` |
|        - | 1367 | ` * For keys that exist in both arrays, the elements from the left-hand array` |
|        - | 1368 | ` * will be used, and the matching elements from the right-hand array will` |
|        - | 1369 | ` * be ignored.` |
|        - | 1370 | ` * This function take care of handling all the scenarios.` |
|        - | 1371 | ` */` |
|     9700 | 1372 | `PH7_PRIVATE sxi32 PH7_MemObjAdd(ph7_value *pObj1,ph7_value *pObj2,int bAddStore)` |
|        5 | 1373 | `{` |
|     9705 | 1374 | `	if( ((pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1375 | `			/* Arithemtic operation */` |
|     5891 | 1376 | `			PH7_MemObjToNumeric(pObj1);` |
|     5891 | 1377 | `			PH7_MemObjToNumeric(pObj2);` |
|     5891 | 1378 | `			if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_REAL ){` |
|        - | 1379 | `				/* Floating point arithmetic */` |
|        - | 1380 | `				ph7_real a,b;` |
|       65 | 1381 | `				if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|       31 | 1382 | `					PH7_MemObjToReal(pObj1);` |
|       15 | 1383 | `				}` |
|       65 | 1384 | `				if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 | 1385 | `					PH7_MemObjToReal(pObj2);` |
|        1 | 1386 | `				}` |
|       65 | 1387 | `				a = pObj1->rVal;` |
|       65 | 1388 | `				b = pObj2->rVal;` |
|       65 | 1389 | `				pObj1->rVal = a+b;` |
|       65 | 1390 | `				MemObjSetType(pObj1,MEMOBJ_REAL);` |
|        - | 1391 | `				/* Try to get an integer representation also */` |
|       65 | 1392 | `				MemObjTryIntger(&(*pObj1));` |
|       33 | 1393 | `			}else{` |
|        - | 1394 | `				/* Integer arithmetic */` |
|        - | 1395 | `				sxi64 a,b;` |
|     5827 | 1396 | `				a = pObj1->x.iVal;` |
|     5827 | 1397 | `				b = pObj2->x.iVal;` |
|     5827 | 1398 | `				pObj1->x.iVal = a+b;` |
|     5827 | 1399 | `				MemObjSetType(pObj1,MEMOBJ_INT);` |
|        - | 1400 | `			}` |
|     2948 | 1401 | `	}else{` |
|     3819 | 1402 | `		if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP ){` |
|        - | 1403 | `			ph7_hashmap *pMap;` |
|        - | 1404 | `			sxi32 rc;` |
|     3819 | 1405 | `			if( bAddStore ){` |
|        - | 1406 | `				/* Do not duplicate the hashmap,use the left one since its an add&store operation.` |
|        - | 1407 | `				 */` |
|        3 | 1408 | `				if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1409 | `					/* Force a hashmap cast */` |
|      ! 0 | 1410 | `					rc = PH7_MemObjToHashmap(pObj1);` |
|      ! 0 | 1411 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 1412 | `						PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|      ! 0 | 1413 | `						return rc;` |
|        - | 1414 | `					}` |
|      ! 0 | 1415 | `				}` |
|        - | 1416 | `				/* COW separate before in-place mutation */` |
|        3 | 1417 | `				pMap = PH7_HashmapCowSeparate(pObj1->pVm,pObj1);` |
|        2 | 1418 | `			}else{` |
|        - | 1419 | `				/* Create a new hashmap */` |
|     3817 | 1420 | `				pMap = PH7_NewHashmap(pObj1->pVm,0,0);` |
|     3817 | 1421 | `				if( pMap == 0){` |
|      ! 0 | 1422 | `					PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|      ! 0 | 1423 | `					return SXERR_MEM;` |
|        - | 1424 | `				}` |
|        - | 1425 | `			}` |
|     3819 | 1426 | `			if( !bAddStore ){` |
|     3817 | 1427 | `				if(pObj1->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1428 | `					/* Perform a hashmap duplication */` |
|     3817 | 1429 | `					PH7_HashmapDup((ph7_hashmap *)pObj1->x.pOther,pMap);` |
|     1911 | 1430 | `				}else{` |
|      ! 0 | 1431 | `					if((pObj1->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 1432 | `						/* Simple insertion */` |
|      ! 0 | 1433 | `						PH7_HashmapInsert(pMap,0,pObj1);` |
|      ! 0 | 1434 | `					}` |
|        - | 1435 | `				}` |
|     1906 | 1436 | `			}` |
|        - | 1437 | `			/* Perform the union */` |
|     3819 | 1438 | `			if(pObj2->iFlags & MEMOBJ_HASHMAP ){` |
|     3819 | 1439 | `				PH7_HashmapUnion(pMap,(ph7_hashmap *)pObj2->x.pOther);` |
|     1912 | 1440 | `			}else{` |
|      ! 0 | 1441 | `				if((pObj2->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 1442 | `					/* Simple insertion */` |
|      ! 0 | 1443 | `					PH7_HashmapInsert(pMap,0,pObj2);` |
|      ! 0 | 1444 | `				}` |
|        - | 1445 | `			}` |
|        - | 1446 | `			/* Reflect the change */` |
|     3819 | 1447 | `			if( pObj1->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 1448 | `				SyBlobRelease(&pObj1->sBlob);` |
|      ! 0 | 1449 | `			}` |
|     3819 | 1450 | `			pObj1->x.pOther = pMap;` |
|     3819 | 1451 | `			MemObjSetType(pObj1,MEMOBJ_HASHMAP);` |
|     1907 | 1452 | `		}` |
|        - | 1453 | `	}` |
|     9705 | 1454 | `	return SXRET_OK;` |
|     4855 | 1455 | `}` |
|        - | 1456 | `/*` |
|        - | 1457 | ` * Return a printable representation of the type of a given` |
|        - | 1458 | ` * ph7_value.` |
|        - | 1459 | ` */` |
|      464 | 1460 | `PH7_PRIVATE const char * PH7_MemObjTypeDump(ph7_value *pVal)` |
|        4 | 1461 | `{` |
|      468 | 1462 | `	const char *zType = "";` |
|      468 | 1463 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|        5 | 1464 | `		zType = "null";` |
|      466 | 1465 | `	}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|        - | 1466 | `		/* REAL is authoritative over a cached MEMOBJ_INT: an integer-valued` |
|        - | 1467 | `		 * real (e.g. 1.0) is reported as "double", matching PHP's gettype(). */` |
|        7 | 1468 | `		zType = "double";` |
|      461 | 1469 | `	}else if( pVal->iFlags & MEMOBJ_INT ){` |
|      127 | 1470 | `		zType = "int";` |
|      396 | 1471 | `	}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|       76 | 1472 | `		zType = "string";` |
|      298 | 1473 | `	}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|      109 | 1474 | `		zType = "bool";` |
|      208 | 1475 | `	}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|       16 | 1476 | `		zType = "array";` |
|      148 | 1477 | `	}else if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      141 | 1478 | `		zType = "object";` |
|       69 | 1479 | `	}else if( pVal->iFlags & MEMOBJ_RES ){` |
|      ! 0 | 1480 | `		zType = "resource";` |
|      ! 0 | 1481 | `	}` |
|      468 | 1482 | `	return zType;` |
|        4 | 1483 | `}` |
|        - | 1484 | `/*` |
|        - | 1485 | ` * Dump a ph7_value [i.e: get a printable representation of it's type and contents.].` |
|        - | 1486 | ` * Store the dump in the given blob.` |
|        - | 1487 | ` */` |
|      478 | 1488 | `PH7_PRIVATE sxi32 PH7_MemObjDump(` |
|        - | 1489 | `	SyBlob *pOut,      /* Store the dump here */` |
|        - | 1490 | `	ph7_value *pObj,   /* Dump this */` |
|        - | 1491 | `	int ShowType,      /* TRUE to output value type */` |
|        - | 1492 | `	int nTab,          /* # of Whitespace to insert */` |
|        - | 1493 | `	int nDepth,        /* Nesting level */` |
|        - | 1494 | `	int isRef          /* TRUE if referenced object */` |
|        - | 1495 | `	)` |
|        4 | 1496 | `{` |
|      482 | 1497 | `	sxi32 rc = SXRET_OK;` |
|        - | 1498 | `	const char *zType;` |
|        - | 1499 | `	int i;` |
|     4598 | 1500 | `	for( i = 0 ; i < nTab ; i++ ){` |
|     4119 | 1501 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     2061 | 1502 | `	}` |
|      482 | 1503 | `	if( ShowType ){` |
|      434 | 1504 | `		if( isRef ){` |
|      ! 0 | 1505 | `			SyBlobAppend(&(*pOut),"&",sizeof(char));` |
|      ! 0 | 1506 | `		}` |
|        - | 1507 | `		/* Get value type first. var_dump() labels reals "float" (PHP), whereas` |
|        - | 1508 | `		 * gettype()/PH7_MemObjTypeDump use the legacy "double" spelling. */` |
|      434 | 1509 | `		if( (pObj->iFlags & MEMOBJ_REAL) && (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        5 | 1510 | `			zType = "float";` |
|        3 | 1511 | `		}else{` |
|      430 | 1512 | `			zType = PH7_MemObjTypeDump(pObj);` |
|        - | 1513 | `		}` |
|      434 | 1514 | `		SyBlobAppend(&(*pOut),zType,SyStrlen(zType));` |
|      215 | 1515 | `	}` |
|      482 | 1516 | `	if((pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|      480 | 1517 | `		if ( ShowType ){` |
|      432 | 1518 | `			SyBlobAppend(&(*pOut),"(",sizeof(char));` |
|      214 | 1519 | `		}` |
|      480 | 1520 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1521 | `			/* Dump hashmap entries */` |
|       24 | 1522 | `			rc = PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,ShowType,nTab+1,nDepth+1);` |
|      469 | 1523 | `		}else if(pObj->iFlags & MEMOBJ_OBJ ){` |
|        - | 1524 | `			/* Dump class instance attributes */` |
|      141 | 1525 | `			rc = PH7_ClassInstanceDump(&(*pOut),(ph7_class_instance *)pObj->x.pOther,ShowType,nTab+1,nDepth+1);` |
|       72 | 1526 | `		}else{` |
|      320 | 1527 | `			SyBlob *pContents = &pObj->sBlob;` |
|        - | 1528 | `			/* Get a printable representation of the contents */` |
|      320 | 1529 | `			if((pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|      252 | 1530 | `				MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|      128 | 1531 | `			}else{` |
|        - | 1532 | `				/* PHP format: string(N) "content" */` |
|       71 | 1533 | `				if( ShowType ){` |
|       53 | 1534 | `					SyBlobFormat(&(*pOut),"%u) \"",SyBlobLength(&pObj->sBlob));` |
|       25 | 1535 | `				}` |
|       71 | 1536 | `				if( SyBlobLength(pContents) > 0 ){` |
|       69 | 1537 | `					SyBlobAppend(&(*pOut),SyBlobData(pContents),SyBlobLength(pContents));` |
|       33 | 1538 | `				}` |
|       71 | 1539 | `				if( ShowType ){` |
|       53 | 1540 | `					SyBlobAppend(&(*pOut),"\"",sizeof(char));` |
|       25 | 1541 | `				}` |
|        - | 1542 | `			}` |
|        - | 1543 | `		}` |
|      480 | 1544 | `		if( ShowType ){` |
|        - | 1545 | `			/* Strings already emitted their own ')' as part of the` |
|        - | 1546 | `			 * "N) \"content\"" format above. */` |
|      432 | 1547 | `			if( (pObj->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_STRING)) == 0 ){` |
|      236 | 1548 | `				SyBlobAppend(&(*pOut),")",sizeof(char));` |
|      116 | 1549 | `			}` |
|      214 | 1550 | `		}` |
|      238 | 1551 | `	}` |
|        - | 1552 | `#ifdef __WINNT__` |
|        4 | 1553 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|        - | 1554 | `#else` |
|      478 | 1555 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        - | 1556 | `#endif` |
|      482 | 1557 | `	return rc;` |
|        4 | 1558 | `}` |
|        - | 1559 |  |
