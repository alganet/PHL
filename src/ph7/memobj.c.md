# src/ph7/memobj.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 835/923 lines (90.47%)

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
|     2976 |   42 | `static sxi64 MemObjRealToInt(ph7_value *pObj)` |
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
|     2981 |   59 | `  ph7_real r = pObj->rVal;` |
|     2981 |   60 | `  if( r<(ph7_real)minInt ){` |
|        3 |   61 | `    return minInt;` |
|     2979 |   62 | `  }else if( r>(ph7_real)maxInt ){` |
|        - |   63 | `    /* minInt is correct here - not maxInt.  It turns out that assigning` |
|        - |   64 | `    ** a very large positive number to an integer results in a very large` |
|        - |   65 | `    ** negative integer.  This makes no sense, but it is what x86 hardware` |
|        - |   66 | `    ** does so for compatibility we will do the same in software. */` |
|      153 |   67 | `    return minInt;` |
|      ! 0 |   68 | `  }else{` |
|     2827 |   69 | `    return (sxi64)r;` |
|        - |   70 | `  }` |
|        - |   71 | `#endif` |
|     1493 |   72 | `}` |
|        - |   73 | `/*` |
|        - |   74 | ` * Convert a raw token value typically a stream of digit [i.e: hex,octal,binary or decimal]` |
|        - |   75 | ` * to a 64-bit integer.` |
|        - |   76 | ` */` |
|  1274726 |   77 | `PH7_PRIVATE sxi64 PH7_TokenValueToInt64(SyString *pVal)` |
|        5 |   78 | `{` |
|  1274731 |   79 | `	sxi64 iVal = 0;` |
|  1274731 |   80 | `	if( pVal->nByte <= 0 ){` |
|        7 |   81 | `		return 0;` |
|        - |   82 | `	}` |
|  1274725 |   83 | `	if( pVal->zString[0] == '0' ){` |
|        - |   84 | `		sxi32 c;` |
|   352353 |   85 | `		if( pVal->nByte == sizeof(char) ){` |
|   351939 |   86 | `			return 0;` |
|        - |   87 | `		}` |
|      415 |   88 | `		c = pVal->zString[1];` |
|      415 |   89 | `		if( c  == 'x' \|\| c == 'X' ){` |
|        - |   90 | `			/* Hex digit stream */` |
|       71 |   91 | `			SyHexStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|      380 |   92 | `		}else if( c == 'b' \|\| c == 'B' ){` |
|        - |   93 | `			/* Binary digit stream */` |
|      279 |   94 | `			SyBinaryStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|      140 |   95 | `		}else{` |
|        - |   96 | `			/* Octal digit stream */` |
|       67 |   97 | `			SyOctalStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|        - |   98 | `		}` |
|      208 |   99 | `	}else{` |
|        - |  100 | `		/* Decimal digit stream */` |
|   922377 |  101 | `		SyStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|        - |  102 | `	}` |
|   922791 |  103 | `	return iVal;` |
|   637368 |  104 | `}` |
|        - |  105 | `/*` |
|        - |  106 | ` * Return some kind of 64-bit integer value which is the best we can` |
|        - |  107 | ` * do at representing the value that pObj describes as a string` |
|        - |  108 | ` * representation.` |
|        - |  109 | ` */` |
|      490 |  110 | `static sxi64 MemObjStringToInt(ph7_value *pObj)` |
|        5 |  111 | `{` |
|        - |  112 | `	SyString sVal;` |
|      495 |  113 | `	SyStringInitFromBuf(&sVal,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|      495 |  114 | `	return PH7_TokenValueToInt64(&sVal);` |
|        5 |  115 | `}` |
|        - |  116 | `/*` |
|        - |  117 | ` * Call a magic class method [i.e: __toString(),__toInt(),...]` |
|        - |  118 | ` * Return SXRET_OK if the magic method is available and have been` |
|        - |  119 | ` * successfully called. Any other return value indicates failure.` |
|        - |  120 | ` */` |
|      178 |  121 | `static sxi32 MemObjCallClassCastMethod(` |
|        - |  122 | `	ph7_vm *pVm,               /* VM that trigger the invocation */` |
|        - |  123 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object] */` |
|        - |  124 | `	const char *zMethod,       /* Magic method name [i.e: __toString] */` |
|        - |  125 | `	sxu32 nLen,                /* Method name length */` |
|        - |  126 | `	ph7_value *pResult         /* OUT: Store the return value of the magic method here */` |
|        - |  127 | `	)` |
|        5 |  128 | `{` |
|        - |  129 | `	ph7_class_method *pMethod;` |
|        - |  130 | `	/* Check if the method is available */` |
|      183 |  131 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,zMethod,nLen);` |
|      183 |  132 | `	if( pMethod == 0 ){` |
|        - |  133 | `		/* No such method */` |
|        6 |  134 | `		return SXERR_NOTFOUND;` |
|        - |  135 | `	}` |
|        - |  136 | `	/* Invoke the desired method */` |
|      179 |  137 | `	PH7_VmCallClassMethod(&(*pVm),&(*pThis),pMethod,&(*pResult),0,0);` |
|        - |  138 | `	/* Method successfully called,pResult should hold the return value */` |
|      179 |  139 | `	return SXRET_OK;` |
|       94 |  140 | `}` |
|        - |  141 | `/*` |
|        - |  142 | ` * Return some kind of integer value which is the best we can` |
|        - |  143 | ` * do at representing the value that pObj describes as an integer.` |
|        - |  144 | ` * If pObj is an integer, then the value is exact. If pObj is` |
|        - |  145 | ` * a floating-point then  the value returned is the integer part.` |
|        - |  146 | ` * If pObj is a string, then we make an attempt to convert it into` |
|        - |  147 | ` * a integer and return that.` |
|        - |  148 | ` * If pObj represents a NULL value, return 0.` |
|        - |  149 | ` */` |
|      574 |  150 | `static sxi64 MemObjIntValue(ph7_value *pObj)` |
|        5 |  151 | `{` |
|        - |  152 | `	sxi32 iFlags;` |
|      579 |  153 | `	iFlags = pObj->iFlags;` |
|      579 |  154 | `	if (iFlags & MEMOBJ_REAL ){` |
|       39 |  155 | `		return MemObjRealToInt(&(*pObj));` |
|      541 |  156 | `	}else if( iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      140 |  157 | `		return pObj->x.iVal;` |
|      403 |  158 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|      381 |  159 | `		return MemObjStringToInt(&(*pObj));` |
|       23 |  160 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|       11 |  161 | `		return 0;` |
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
|      292 |  188 | `}` |
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
|     1766 |  199 | `static ph7_real MemObjRealValue(ph7_value *pObj)` |
|        5 |  200 | `{` |
|        - |  201 | `	sxi32 iFlags;` |
|     1771 |  202 | `	iFlags = pObj->iFlags;` |
|     1771 |  203 | `	if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  204 | `		return pObj->rVal;` |
|     1771 |  205 | `	}else if (iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      772 |  206 | `		return (ph7_real)pObj->x.iVal;` |
|     1000 |  207 | `	}else if (iFlags & MEMOBJ_STRING){` |
|        - |  208 | `		SyString sString;` |
|        - |  209 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  210 | `		ph7_real rVal = 0;` |
|        - |  211 | `#else` |
|      994 |  212 | `		ph7_real rVal = 0.0;` |
|        - |  213 | `#endif` |
|      994 |  214 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|      994 |  215 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|        - |  216 | `			/* Convert as much as we can */` |
|        - |  217 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  218 | `			rVal = MemObjStringToInt(&(*pObj));` |
|        - |  219 | `#else` |
|      994 |  220 | `			SyStrToReal(sString.zString,sString.nByte,(void *)&rVal,0);` |
|        - |  221 | `#endif` |
|      495 |  222 | `		}` |
|      994 |  223 | `		return rVal;` |
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
|      888 |  256 | `}` |
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
|      490 |  270 | `PH7_PRIVATE sxi32 PH7_PhpFloatShape(char *zBuf,sxi32 nLen,int bGeneric)` |
|        4 |  271 | `{` |
|        - |  272 | `	sxi32 iExp,i;` |
|      494 |  273 | `	iExp = nLen - 1;` |
|     4166 |  274 | `	while( iExp > 0 && zBuf[iExp] != 'e' && zBuf[iExp] != 'E' ){` |
|     3676 |  275 | `		iExp--;` |
|        4 |  276 | `	}` |
|      494 |  277 | `	if( iExp <= 0 ){` |
|      448 |  278 | `		return nLen; /* No exponent part (fixed notation) */` |
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
|      249 |  314 | `}` |
|        - |  315 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|        - |  316 | `/*` |
|        - |  317 | ` * Return the string representation of a given ph7_value.` |
|        - |  318 | ` * This function never fail and always return SXRET_OK.` |
|        - |  319 | ` */` |
|    56954 |  320 | `static sxi32 MemObjStringValue(SyBlob *pOut,ph7_value *pObj,sxu8 bStrictBool)` |
|        5 |  321 | `{` |
|    56959 |  322 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|        - |  323 | `		/* Handle special floating-point values first */` |
|      378 |  324 | `		if( PH7_IS_NAN(pObj->rVal) ){` |
|      ! 0 |  325 | `			SyBlobAppend(&(*pOut),"NAN",3);` |
|      378 |  326 | `		}else if( PH7_IS_INF(pObj->rVal) ){` |
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
|      374 |  341 | `			sxi32 n = (sxi32)snprintf(zNum,sizeof(zNum),"%.14G",pObj->rVal);` |
|      374 |  342 | `			if( n < 0 \|\| n >= (sxi32)sizeof(zNum) ){` |
|      ! 0 |  343 | `				n = (sxi32)SyStrlen(zNum);` |
|      ! 0 |  344 | `			}` |
|      374 |  345 | `			n = PH7_PhpFloatShape(zNum,n,TRUE);` |
|      374 |  346 | `			SyBlobAppend(&(*pOut),zNum,(sxu32)n);` |
|        - |  347 | `#else` |
|        - |  348 | `			SyBlobFormat(&(*pOut),"%.15g",pObj->rVal);` |
|        - |  349 | `#endif` |
|        4 |  350 | `		}` |
|    56772 |  351 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|    56023 |  352 | `		SyBlobFormat(&(*pOut),"%qd",pObj->x.iVal);` |
|        - |  353 | `		/* %qd (BSD quad) is equivalent to %lld in the libc printf */` |
|    28576 |  354 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|      361 |  355 | `		if( bStrictBool ){` |
|        - |  356 | `			/* Actual string cast: true -> "1", false -> "" (like PHP) */` |
|      255 |  357 | `			if( pObj->x.iVal ){` |
|       29 |  358 | `				SyBlobAppend(&(*pOut),"1",sizeof("1")-1);` |
|       13 |  359 | `			}` |
|        - |  360 | `			/* false produces empty string, nothing to append */` |
|      130 |  361 | `		}else{` |
|        - |  362 | `			/* Display path (var_dump, print_r): show TRUE/FALSE */` |
|      109 |  363 | `			if( pObj->x.iVal ){` |
|       65 |  364 | `				SyBlobAppend(&(*pOut),"TRUE",sizeof("TRUE")-1);` |
|       34 |  365 | `			}else{` |
|       46 |  366 | `				SyBlobAppend(&(*pOut),"FALSE",sizeof("FALSE")-1);` |
|        - |  367 | `			}` |
|        5 |  368 | `		}` |
|      389 |  369 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|      ! 0 |  370 | `		SyBlobAppend(&(*pOut),"Array",sizeof("Array")-1);` |
|      ! 0 |  371 | `		PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
|      211 |  372 | `	}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|        - |  373 | `		ph7_value sResult;` |
|        - |  374 | `		sxi32 rc;` |
|        - |  375 | `		/* Invoke the __toString() method if available */` |
|      169 |  376 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|      169 |  377 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|        - |  378 | `			"__toString",sizeof("__toString")-1,&sResult);` |
|      169 |  379 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_STRING) && SyBlobLength(&sResult.sBlob) > 0){` |
|        - |  380 | `			/* Expand method return value */` |
|       92 |  381 | `			SyBlobDup(&sResult.sBlob,pOut);` |
|       48 |  382 | `		}else{` |
|        - |  383 | `			/* Expand "Object" as requested by the PHP language reference manual */` |
|       80 |  384 | `			SyBlobAppend(&(*pOut),"Object",sizeof("Object")-1);` |
|        - |  385 | `		}` |
|      169 |  386 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|      169 |  387 | `		PH7_MemObjRelease(&sResult);` |
|      127 |  388 | `	}else if(pObj->iFlags & MEMOBJ_RES ){` |
|        3 |  389 | `		SyBlobFormat(&(*pOut),"ResourceID_%#x",pObj->x.pOther);` |
|        1 |  390 | `	}` |
|    56959 |  391 | `	return SXRET_OK;` |
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
|    15924 |  405 | `static sxi32 MemObjBooleanValue(ph7_value *pObj)` |
|        5 |  406 | `{` |
|        - |  407 | `	sxi32 iFlags;` |
|    15929 |  408 | `	iFlags = pObj->iFlags;` |
|    15929 |  409 | `	if (iFlags & MEMOBJ_REAL ){` |
|        - |  410 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  411 | `		return pObj->rVal ? 1 : 0;` |
|        - |  412 | `#else` |
|       12 |  413 | `		return pObj->rVal != 0.0 ? 1 : 0;` |
|        - |  414 | `#endif` |
|    15919 |  415 | `	}else if( iFlags & MEMOBJ_INT ){` |
|      277 |  416 | `		return pObj->x.iVal ? 1 : 0;` |
|    15647 |  417 | `	}else if (iFlags & MEMOBJ_STRING) {` |
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
|    15585 |  438 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|    14543 |  439 | `		return 0;` |
|     1047 |  440 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|       20 |  441 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|       20 |  442 | `		sxu32 n = pMap->nEntry;` |
|       20 |  443 | `		PH7_HashmapUnref(pMap);` |
|       20 |  444 | `		return n > 0 ? TRUE : FALSE;` |
|     1029 |  445 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
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
|     1023 |  460 | `	}else if(iFlags & MEMOBJ_RES ){` |
|     1023 |  461 | `		return pObj->x.pOther != 0;` |
|        - |  462 | `	}` |
|        - |  463 | `	/* NOT REACHED */` |
|      ! 0 |  464 | `	return 0;` |
|     7967 |  465 | `}` |
|        - |  466 | `/*` |
|        - |  467 | ` * If the ph7_value is of type real,try to make it an integer also.` |
|        - |  468 | ` */` |
|     2938 |  469 | `static sxi32 MemObjTryIntger(ph7_value *pObj)` |
|        5 |  470 | `{` |
|     2943 |  471 | `	pObj->x.iVal = MemObjRealToInt(&(*pObj));` |
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
|     2938 |  484 | `	if( pObj->rVal ==(ph7_real)pObj->x.iVal && pObj->x.iVal>SMALLEST_INT64` |
|     1540 |  485 | `      && pObj->x.iVal<LARGEST_INT64 ){` |
|     1529 |  486 | `		  pObj->iFlags \|= MEMOBJ_INT;` |
|      764 |  487 | `	}` |
|     2943 |  488 | `	return SXRET_OK;` |
|        5 |  489 | `}` |
|        - |  490 | `/*` |
|        - |  491 | ` * Convert a ph7_value to type integer.Invalidate any prior representations.` |
|        - |  492 | ` */` |
|   427902 |  493 | `PH7_PRIVATE sxi32 PH7_MemObjToInteger(ph7_value *pObj)` |
|        5 |  494 | `{` |
|   427907 |  495 | `	if( (pObj->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  496 | `		/* Preform the conversion */` |
|      579 |  497 | `		pObj->x.iVal = MemObjIntValue(&(*pObj));` |
|        - |  498 | `		/* Invalidate any prior representations */` |
|      579 |  499 | `		SyBlobRelease(&pObj->sBlob);` |
|      579 |  500 | `		MemObjSetType(pObj,MEMOBJ_INT);` |
|      287 |  501 | `	}` |
|   427907 |  502 | `	return SXRET_OK;` |
|        5 |  503 | `}` |
|        - |  504 | `/*` |
|        - |  505 | ` * Convert a ph7_value to type real (Try to get an integer representation also).` |
|        - |  506 | ` * Invalidate any prior representations` |
|        - |  507 | ` */` |
|     2668 |  508 | `PH7_PRIVATE sxi32 PH7_MemObjToReal(ph7_value *pObj)` |
|        5 |  509 | `{` |
|     2673 |  510 | `	if((pObj->iFlags & MEMOBJ_REAL) == 0 ){` |
|        - |  511 | `		/* Preform the conversion */` |
|     1771 |  512 | `		pObj->rVal = MemObjRealValue(&(*pObj));` |
|        - |  513 | `		/* Invalidate any prior representations */` |
|     1771 |  514 | `		SyBlobRelease(&pObj->sBlob);` |
|     1771 |  515 | `		MemObjSetType(pObj,MEMOBJ_REAL);` |
|        - |  516 | `		/* Try to get an integer representation */` |
|     1771 |  517 | `		MemObjTryIntger(&(*pObj));` |
|      883 |  518 | `	}` |
|     2673 |  519 | `	return SXRET_OK;` |
|        5 |  520 | `}` |
|        - |  521 | `/*` |
|        - |  522 | ` * Convert a ph7_value to type boolean.Invalidate any prior representations.` |
|        - |  523 | ` */` |
|    17398 |  524 | `PH7_PRIVATE sxi32 PH7_MemObjToBool(ph7_value *pObj)` |
|        5 |  525 | `{` |
|    17403 |  526 | `	if( (pObj->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        - |  527 | `		/* Preform the conversion */` |
|    15929 |  528 | `		pObj->x.iVal = MemObjBooleanValue(&(*pObj));` |
|        - |  529 | `		/* Invalidate any prior representations */` |
|    15929 |  530 | `		SyBlobRelease(&pObj->sBlob);` |
|    15929 |  531 | `		MemObjSetType(pObj,MEMOBJ_BOOL);` |
|     7962 |  532 | `	}` |
|    17403 |  533 | `	return SXRET_OK;` |
|        5 |  534 | `}` |
|        - |  535 | `/*` |
|        - |  536 | ` * Convert a ph7_value to type string.Prior representations are NOT invalidated.` |
|        - |  537 | ` */` |
|   834775 |  538 | `PH7_PRIVATE sxi32 PH7_MemObjToString(ph7_value *pObj)` |
|        5 |  539 | `{` |
|   834780 |  540 | `	sxi32 rc = SXRET_OK;` |
|   834780 |  541 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  542 | `		/* Perform the conversion */` |
|    56711 |  543 | `		SyBlobReset(&pObj->sBlob); /* Reset the internal buffer */` |
|    56711 |  544 | `		rc = MemObjStringValue(&pObj->sBlob,&(*pObj),TRUE);` |
|    56711 |  545 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|    28353 |  546 | `	}` |
|   834780 |  547 | `	return rc;` |
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
|      386 |  564 | `PH7_PRIVATE sxi32 PH7_MemObjToHashmap(ph7_value *pObj)` |
|        5 |  565 | `{` |
|      391 |  566 | `	if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  567 | `		ph7_hashmap *pMap;` |
|        - |  568 | `		/* Allocate a new hashmap instance */` |
|      251 |  569 | `		pMap = PH7_NewHashmap(pObj->pVm,0,0);` |
|      251 |  570 | `		if( pMap == 0 ){` |
|      ! 0 |  571 | `			return SXERR_MEM;` |
|        - |  572 | `		}` |
|      251 |  573 | `		if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
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
|      251 |  590 | `		PH7_MemObjRelease(pObj);` |
|      251 |  591 | `		MemObjSetType(pObj,MEMOBJ_HASHMAP);` |
|      251 |  592 | `		pObj->x.pOther = pMap;` |
|      123 |  593 | `	}` |
|      391 |  594 | `	return SXRET_OK;` |
|      198 |  595 | `}` |
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
|        - |  707 | ` * Return TRUE only if the entire string held by pValue (optionally surrounded` |
|        - |  708 | ` * by whitespace, with an optional sign) is a well-formed PHP numeric string.` |
|        - |  709 | ` * This mirrors PHP's is_numeric_string grammar used for is_numeric() and the` |
|        - |  710 | ` * loose-comparison numeric gate:` |
|        - |  711 | ` *` |
|        - |  712 | ` *   [ws] [sign] ( D+ [.D*] \| .D+ ) [ (e\|E) [sign] D+ ] [ws]   (whole string)` |
|        - |  713 | ` *` |
|        - |  714 | ` * Implemented directly rather than via SyStrIsNumeric — which returns OK on any` |
|        - |  715 | ` * numeric PREFIX (so it wrongly accepts "10abc"/"0x1A"/"0b101") and requires a` |
|        - |  716 | ` * leading digit (so it wrongly rejects ".5"/"-.5", valid in PHP). Unlike a` |
|        - |  717 | ` * strtod-based classifier this needs no NUL-terminated buffer. Returns FALSE for` |
|        - |  718 | ` * a non-string value.` |
|        - |  719 | ` */` |
|   231634 |  720 | `PH7_PRIVATE int PH7_MemObjStringIsNumeric(ph7_value *pValue)` |
|        5 |  721 | `{` |
|        - |  722 | `	const char *z, *zEnd;` |
|        - |  723 | `	sxu32 n;` |
|   231639 |  724 | `	int bDigit = 0;` |
|   231639 |  725 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  726 | `		return 0;` |
|        - |  727 | `	}` |
|   231639 |  728 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|   231639 |  729 | `	n = SyBlobLength(&pValue->sBlob);` |
|   231639 |  730 | `	if( n == 0 ){` |
|       24 |  731 | `		return 0;` |
|        - |  732 | `	}` |
|   231617 |  733 | `	zEnd = z + n;` |
|   231623 |  734 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){` |
|        8 |  735 | `		z++;` |
|        2 |  736 | `	}` |
|   231617 |  737 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|       49 |  738 | `		z++;` |
|       22 |  739 | `	}` |
|   231767 |  740 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|      153 |  741 | `		z++; bDigit = 1;` |
|        3 |  742 | `	}` |
|   231617 |  743 | `	if( z < zEnd && z[0] == '.' ){` |
|       40 |  744 | `		z++;` |
|       76 |  745 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|       38 |  746 | `			z++; bDigit = 1;` |
|        2 |  747 | `		}` |
|       19 |  748 | `	}` |
|        - |  749 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|   231617 |  750 | `	if( !bDigit ){` |
|   231493 |  751 | `		return 0;` |
|        - |  752 | `	}` |
|        - |  753 | `	/* Optional exponent — must carry at least one digit (rejects "1e", "1e+"). */` |
|      127 |  754 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|       14 |  755 | `		z++;` |
|       14 |  756 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|      ! 0 |  757 | `			z++;` |
|      ! 0 |  758 | `		}` |
|       14 |  759 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|        6 |  760 | `			return 0;` |
|        - |  761 | `		}` |
|       22 |  762 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|       14 |  763 | `			z++;` |
|        2 |  764 | `		}` |
|        4 |  765 | `	}` |
|        - |  766 | `	/* Trailing whitespace allowed; anything else means not a numeric string. */` |
|      129 |  767 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){` |
|        8 |  768 | `		z++;` |
|        2 |  769 | `	}` |
|      123 |  770 | `	return z == zEnd ? 1 : 0;` |
|   115842 |  771 | `}` |
|        - |  772 | `/*` |
|        - |  773 | ` * Check whether the ph7_value is numeric [i.e: int/float/bool] or looks` |
|        - |  774 | ` * like a numeric number [i.e: if the ph7_value is of type string.].` |
|        - |  775 | ` * Return TRUE if numeric.FALSE otherwise.` |
|        - |  776 | ` */` |
|   232184 |  777 | `PH7_PRIVATE sxi32 PH7_MemObjIsNumeric(ph7_value *pObj)` |
|        5 |  778 | `{` |
|   232189 |  779 | `	if( pObj->iFlags & ( MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      395 |  780 | `		return TRUE;` |
|   231799 |  781 | `	}else if( pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|      231 |  782 | `		return FALSE;` |
|   231573 |  783 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        - |  784 | `		/* TRUE only if the whole string is a well-formed PHP numeric string. */` |
|   231573 |  785 | `		return PH7_MemObjStringIsNumeric(pObj) ? TRUE : FALSE;` |
|        - |  786 | `	}` |
|        - |  787 | `	/* NOT REACHED */` |
|      ! 0 |  788 | `	return FALSE;` |
|   116117 |  789 | `}` |
|        - |  790 | `/*` |
|        - |  791 | ` * Check whether the ph7_value is empty.Return TRUE if empty.` |
|        - |  792 | ` * FALSE otherwise.` |
|        - |  793 | ` * An ph7_value is considered empty if the following are true:` |
|        - |  794 | ` * NULL value.` |
|        - |  795 | ` * Boolean FALSE.` |
|        - |  796 | ` * Integer/Float with a 0 (zero) value.` |
|        - |  797 | ` * An empty string or a stream of 0 (zero) [i.e: "0","00","000",...].` |
|        - |  798 | ` * An empty array.` |
|        - |  799 | ` * NOTE` |
|        - |  800 | ` *  OBJECT VALUE MUST NOT BE MODIFIED.` |
|        - |  801 | ` */` |
|    33740 |  802 | `PH7_PRIVATE sxi32 PH7_MemObjIsEmpty(ph7_value *pObj)` |
|        5 |  803 | `{` |
|    33745 |  804 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|       19 |  805 | `		return TRUE;` |
|    33729 |  806 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|       20 |  807 | `		return pObj->x.iVal == 0 ? TRUE : FALSE;` |
|    33711 |  808 | `	}else if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  809 | `		return pObj->rVal == (ph7_real)0 ? TRUE : FALSE;` |
|    33711 |  810 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|        5 |  811 | `		return !pObj->x.iVal;` |
|    33707 |  812 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|    27339 |  813 | `		if( SyBlobLength(&pObj->sBlob) <= 0 ){` |
|    21991 |  814 | `			return TRUE;` |
|      ! 0 |  815 | `		}else{` |
|        - |  816 | `			const char *zIn,*zEnd;` |
|     5353 |  817 | `			zIn = (const char *)SyBlobData(&pObj->sBlob);` |
|     5353 |  818 | `			zEnd = &zIn[SyBlobLength(&pObj->sBlob)];` |
|     5359 |  819 | `			while( zIn < zEnd ){` |
|     5359 |  820 | `				if( zIn[0] != '0' ){` |
|     5353 |  821 | `					break;` |
|        - |  822 | `				}` |
|        7 |  823 | `				zIn++;` |
|        1 |  824 | `			}` |
|     5353 |  825 | `			return zIn >= zEnd ? TRUE : FALSE;` |
|      ! 0 |  826 | `		}` |
|     6373 |  827 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|     6373 |  828 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|     6373 |  829 | `		return pMap->nEntry == 0 ? TRUE : FALSE;` |
|      ! 0 |  830 | `	}else if ( pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|      ! 0 |  831 | `		return FALSE;` |
|        - |  832 | `	}` |
|        - |  833 | `	/* Assume empty by default */` |
|      ! 0 |  834 | `	return TRUE;` |
|    16875 |  835 | `}` |
|        - |  836 | `/*` |
|        - |  837 | ` * Convert a ph7_value so that it has types MEMOBJ_REAL or MEMOBJ_INT` |
|        - |  838 | ` * or both.` |
|        - |  839 | ` * Invalidate any prior representations. Every effort is made to force` |
|        - |  840 | ` * the conversion, even if the input is a string that does not look` |
|        - |  841 | ` * completely like a number.Convert as much of the string as we can` |
|        - |  842 | ` * and ignore the rest.` |
|        - |  843 | ` */` |
|   455847 |  844 | `PH7_PRIVATE sxi32 PH7_MemObjToNumeric(ph7_value *pObj)` |
|        5 |  845 | `{` |
|   455852 |  846 | `	if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|   455724 |  847 | `		if( pObj->iFlags & (MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|        3 |  848 | `			if( pObj->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  849 | `				pObj->x.iVal = 0;` |
|      ! 0 |  850 | `			}` |
|        3 |  851 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|        1 |  852 | `		}` |
|        - |  853 | `		/* Already numeric */` |
|   455724 |  854 | `		return  SXRET_OK;` |
|        - |  855 | `	}` |
|      129 |  856 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|      129 |  857 | `		sxi32 rc = SXERR_INVALID;` |
|      129 |  858 | `		sxu8 bReal = FALSE;` |
|        - |  859 | `		SyString sString;` |
|      129 |  860 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|        - |  861 | `		/* Check if the given string looks like a numeric number */` |
|      129 |  862 | `		if( sString.nByte > 0 ){` |
|      129 |  863 | `			rc = SyStrIsNumeric(sString.zString,sString.nByte,&bReal,0);` |
|      129 |  864 | `			if( rc != SXRET_OK && !bReal ){` |
|        - |  865 | `				/* SyStrIsNumeric requires a leading digit, so it mis-classifies` |
|        - |  866 | `				 * a leading-decimal real such as ".5"/"-.5"/".5e2" (returns` |
|        - |  867 | `				 * non-OK with bReal FALSE) — PHP treats these as float. Detect` |
|        - |  868 | `				 * that shape so it coerces to real (strtod parses it) instead of` |
|        - |  869 | `				 * falling through to the int(0) "not a number" branch below. */` |
|        9 |  870 | `				const char *z = sString.zString;` |
|        9 |  871 | `				const char *zEnd = z + sString.nByte;` |
|        9 |  872 | `				while( z < zEnd && SyisSpace(z[0]) ){ z++; }` |
|        9 |  873 | `				if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){ z++; }` |
|        9 |  874 | `				if( z < zEnd && z[0] == '.' && (z + 1) < zEnd && SyisDigit(z[1]) ){` |
|        9 |  875 | `					bReal = TRUE;` |
|        4 |  876 | `				}` |
|        4 |  877 | `			}` |
|       64 |  878 | `		}` |
|      129 |  879 | `		if( bReal ){` |
|       15 |  880 | `			PH7_MemObjToReal(&(*pObj));` |
|        8 |  881 | `		}else{` |
|      115 |  882 | `			if( rc != SXRET_OK ){` |
|        - |  883 | `				/* The input does not look at all like a number,set the value to 0 */` |
|      ! 0 |  884 | `				pObj->x.iVal = 0;` |
|      ! 0 |  885 | `			}else{` |
|        - |  886 | `				/* Convert as much as we can */` |
|      115 |  887 | `				pObj->x.iVal = MemObjStringToInt(&(*pObj));` |
|        - |  888 | `			}` |
|      115 |  889 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|      115 |  890 | `			SyBlobRelease(&pObj->sBlob);` |
|        1 |  891 | `		}` |
|       64 |  892 | `	}else if(pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)){` |
|      ! 0 |  893 | `		PH7_MemObjToInteger(pObj);` |
|      ! 0 |  894 | `	}else{` |
|        - |  895 | `		/* Perform a blind cast */` |
|      ! 0 |  896 | `		PH7_MemObjToReal(&(*pObj));` |
|        - |  897 | `	}` |
|      129 |  898 | `	return SXRET_OK;` |
|   227971 |  899 | `}` |
|        - |  900 | `/*` |
|        - |  901 | ` * Apply Perl-style increment to a string ph7_value in place.` |
|        - |  902 | ` * Walks the bytes right-to-left: digits 0-8 / letters a-y, A-Y bump in` |
|        - |  903 | ` * place; '9' wraps to '0' with carry; 'z' to 'a'; 'Z' to 'A'. A non-` |
|        - |  904 | ` * alphanumeric byte stops the walk without prepending. If carry survives` |
|        - |  905 | ` * past index 0, prepend '1', 'a', or 'A' depending on the class of the` |
|        - |  906 | ` * last carried character. Empty strings become "1".` |
|        - |  907 | ` *` |
|        - |  908 | ` * Caller must ensure pObj is MEMOBJ_STRING and not a numeric string;` |
|        - |  909 | ` * this routine never reclassifies the type, so a result like "e0" stays` |
|        - |  910 | ` * a string even though it looks numeric.` |
|        - |  911 | ` */` |
|       48 |  912 | `PH7_PRIVATE sxi32 PH7_MemObjStringIncrement(ph7_value *pObj)` |
|        1 |  913 | `{` |
|        - |  914 | `	enum CarryClass { CARRY_NONE = 0, CARRY_LOWER, CARRY_UPPER, CARRY_DIGIT };` |
|       49 |  915 | `	enum CarryClass last_class = CARRY_NONE;` |
|        - |  916 | `	sxu32 nLen, pos;` |
|        - |  917 | `	sxu8 *zStr;` |
|       49 |  918 | `	int carry = 1;` |
|        - |  919 | `	int ch;` |
|        - |  920 | `	/* Force ownership: the blob may be SXBLOB_RDONLY (e.g., from` |
|        - |  921 | `	 * PH7_MemObjLoad), in which case BlobPrepareGrow copies on demand` |
|        - |  922 | `	 * and clears the flag.  On an already-owned blob with spare capacity` |
|        - |  923 | `	 * (the common case under PHL's growth allocator), this is a no-op` |
|        - |  924 | `	 * append; on an exact-fit owned blob it triggers a single realloc. */` |
|       49 |  925 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       45 |  926 | `		SyBlobNullAppend(&pObj->sBlob);` |
|       22 |  927 | `	}` |
|       49 |  928 | `	nLen = SyBlobLength(&pObj->sBlob);` |
|       49 |  929 | `	if( nLen == 0 ){` |
|        5 |  930 | `		SyBlobAppend(&pObj->sBlob,"1",sizeof(char));` |
|        5 |  931 | `		return SXRET_OK;` |
|        - |  932 | `	}` |
|       45 |  933 | `	zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|       45 |  934 | `	pos = nLen;` |
|       97 |  935 | `	while( pos > 0 ){` |
|       79 |  936 | `		pos--;` |
|       79 |  937 | `		ch = zStr[pos];` |
|       79 |  938 | `		if( ch >= 'a' && ch <= 'z' ){` |
|       45 |  939 | `			if( ch == 'z' ){` |
|       29 |  940 | `				zStr[pos] = 'a';` |
|       29 |  941 | `				last_class = CARRY_LOWER;` |
|       29 |  942 | `				continue;` |
|        - |  943 | `			}` |
|       17 |  944 | `			zStr[pos]++;` |
|       17 |  945 | `			carry = 0;` |
|       17 |  946 | `			break;` |
|       35 |  947 | `		}else if( ch >= 'A' && ch <= 'Z' ){` |
|       21 |  948 | `			if( ch == 'Z' ){` |
|       19 |  949 | `				zStr[pos] = 'A';` |
|       19 |  950 | `				last_class = CARRY_UPPER;` |
|       19 |  951 | `				continue;` |
|        - |  952 | `			}` |
|        3 |  953 | `			zStr[pos]++;` |
|        3 |  954 | `			carry = 0;` |
|        3 |  955 | `			break;` |
|       15 |  956 | `		}else if( ch >= '0' && ch <= '9' ){` |
|        7 |  957 | `			if( ch == '9' ){` |
|        7 |  958 | `				zStr[pos] = '0';` |
|        7 |  959 | `				last_class = CARRY_DIGIT;` |
|        7 |  960 | `				continue;` |
|        - |  961 | `			}` |
|      ! 0 |  962 | `			zStr[pos]++;` |
|      ! 0 |  963 | `			carry = 0;` |
|      ! 0 |  964 | `			break;` |
|      ! 0 |  965 | `		}else{` |
|        - |  966 | `			/* non-alphanumeric: stop without prepending */` |
|        9 |  967 | `			carry = 0;` |
|        9 |  968 | `			break;` |
|        - |  969 | `		}` |
|      ! 0 |  970 | `	}` |
|       45 |  971 | `	if( carry ){` |
|        - |  972 | `		sxu8 prepend;` |
|        - |  973 | `		sxu32 i;` |
|       19 |  974 | `		switch( last_class ){` |
|        9 |  975 | `			case CARRY_LOWER: prepend = (sxu8)'a'; break;` |
|       11 |  976 | `			case CARRY_UPPER: prepend = (sxu8)'A'; break;` |
|      ! 0 |  977 | `			default:          prepend = (sxu8)'1'; break;` |
|        - |  978 | `		}` |
|        - |  979 | `		/* Append a sentinel byte to grow nByte by 1 (capacity grows too). */` |
|       19 |  980 | `		SyBlobAppend(&pObj->sBlob,"\0",sizeof(char));` |
|       19 |  981 | `		zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|       19 |  982 | `		nLen = SyBlobLength(&pObj->sBlob);` |
|        - |  983 | `		/* Shift right by 1, walking from the end so overlapping is safe. */` |
|       57 |  984 | `		for( i = nLen - 1; i > 0; i-- ){` |
|       39 |  985 | `			zStr[i] = zStr[i - 1];` |
|       20 |  986 | `		}` |
|       19 |  987 | `		zStr[0] = prepend;` |
|        9 |  988 | `	}` |
|       45 |  989 | `	return SXRET_OK;` |
|       25 |  990 | `}` |
|        - |  991 | `/*` |
|        - |  992 | ` * Try a get an integer representation of the given ph7_value.` |
|        - |  993 | ` * If the ph7_value is not of type real,this function is a no-op.` |
|        - |  994 | ` */` |
|     1106 |  995 | `PH7_PRIVATE sxi32 PH7_MemObjTryInteger(ph7_value *pObj)` |
|        1 |  996 | `{` |
|     1107 |  997 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|        - |  998 | `		/* Work only with reals */` |
|     1107 |  999 | `		MemObjTryIntger(&(*pObj));` |
|      553 | 1000 | `	}` |
|     1107 | 1001 | `	return SXRET_OK;` |
|        1 | 1002 | `}` |
|        - | 1003 | `/*` |
|        - | 1004 | ` * Initialize a ph7_value to the null type.` |
|        - | 1005 | ` */` |
| 13647391 | 1006 | `PH7_PRIVATE sxi32 PH7_MemObjInit(ph7_vm *pVm,ph7_value *pObj)` |
|        5 | 1007 | `{` |
|        - | 1008 | `	/* Zero the structure */` |
| 13647396 | 1009 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1010 | `	/* Initialize fields */` |
| 13647396 | 1011 | `	pObj->pVm = pVm;` |
| 13647396 | 1012 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1013 | `	/* Set the NULL type */` |
| 13647396 | 1014 | `	pObj->iFlags = MEMOBJ_NULL;` |
| 13647396 | 1015 | `	return SXRET_OK;` |
|        5 | 1016 | `}` |
|        - | 1017 | `/*` |
|        - | 1018 | ` * Initialize a ph7_value to the integer type.` |
|        - | 1019 | ` */` |
|  3424068 | 1020 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromInt(ph7_vm *pVm,ph7_value *pObj,sxi64 iVal)` |
|        5 | 1021 | `{` |
|        - | 1022 | `	/* Zero the structure */` |
|  3424073 | 1023 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1024 | `	/* Initialize fields */` |
|  3424073 | 1025 | `	pObj->pVm = pVm;` |
|  3424073 | 1026 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1027 | `	/* Set the desired type */` |
|  3424073 | 1028 | `	pObj->x.iVal = iVal;` |
|  3424073 | 1029 | `	pObj->iFlags = MEMOBJ_INT;` |
|  3424073 | 1030 | `	return SXRET_OK;` |
|        5 | 1031 | `}` |
|        - | 1032 | `/*` |
|        - | 1033 | ` * Initialize a ph7_value to the boolean type.` |
|        - | 1034 | ` */` |
|    17292 | 1035 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromBool(ph7_vm *pVm,ph7_value *pObj,sxi32 iVal)` |
|        5 | 1036 | `{` |
|        - | 1037 | `	/* Zero the structure */` |
|    17297 | 1038 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1039 | `	/* Initialize fields */` |
|    17297 | 1040 | `	pObj->pVm = pVm;` |
|    17297 | 1041 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1042 | `	/* Set the desired type */` |
|    17297 | 1043 | `	pObj->x.iVal = iVal ? 1 : 0;` |
|    17297 | 1044 | `	pObj->iFlags = MEMOBJ_BOOL;` |
|    17297 | 1045 | `	return SXRET_OK;` |
|        5 | 1046 | `}` |
|        - | 1047 | `/*` |
|        - | 1048 | ` * Initialize a ph7_value to the real type.` |
|        - | 1049 | ` */` |
|       10 | 1050 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromReal(ph7_vm *pVm,ph7_value *pObj,ph7_real rVal)` |
|        1 | 1051 | `{` |
|        - | 1052 | `	/* Zero the structure */` |
|       11 | 1053 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1054 | `	/* Initialize fields */` |
|       11 | 1055 | `	pObj->pVm = pVm;` |
|       11 | 1056 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1057 | `	/* Set the desired type */` |
|       11 | 1058 | `	pObj->rVal = rVal;` |
|       11 | 1059 | `	pObj->iFlags = MEMOBJ_REAL;` |
|       11 | 1060 | `	return SXRET_OK;` |
|        1 | 1061 | `}` |
|        - | 1062 | `/*` |
|        - | 1063 | ` * Initialize a ph7_value to the array type.` |
|        - | 1064 | ` */` |
|    69420 | 1065 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromArray(ph7_vm *pVm,ph7_value *pObj,ph7_hashmap *pArray)` |
|        5 | 1066 | `{` |
|        - | 1067 | `	/* Zero the structure */` |
|    69425 | 1068 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1069 | `	/* Initialize fields */` |
|    69425 | 1070 | `	pObj->pVm = pVm;` |
|    69425 | 1071 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1072 | `	/* Set the desired type */` |
|    69425 | 1073 | `	pObj->iFlags = MEMOBJ_HASHMAP;` |
|    69425 | 1074 | `	pObj->x.pOther = pArray;` |
|    69425 | 1075 | `	return SXRET_OK;` |
|        5 | 1076 | `}` |
|        - | 1077 | `/*` |
|        - | 1078 | ` * Initialize a ph7_value to the string type.` |
|        - | 1079 | ` */` |
|  2114344 | 1080 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromString(ph7_vm *pVm,ph7_value *pObj,const SyString *pVal)` |
|        5 | 1081 | `{` |
|        - | 1082 | `	/* Zero the structure */` |
|  2114349 | 1083 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1084 | `	/* Initialize fields */` |
|  2114349 | 1085 | `	pObj->pVm = pVm;` |
|  2114349 | 1086 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|  2114349 | 1087 | `	if( pVal ){` |
|        - | 1088 | `		/* Append contents */` |
|   903803 | 1089 | `		SyBlobAppend(&pObj->sBlob,(const void *)pVal->zString,pVal->nByte);` |
|   451899 | 1090 | `	}` |
|        - | 1091 | `	/* Set the desired type */` |
|  2114349 | 1092 | `	pObj->iFlags = MEMOBJ_STRING;` |
|  2114349 | 1093 | `	return SXRET_OK;` |
|        5 | 1094 | `}` |
|        - | 1095 | `/*` |
|        - | 1096 | ` * Append some contents to the internal buffer of a given ph7_value.` |
|        - | 1097 | ` * If the given ph7_value is not of type string,this function` |
|        - | 1098 | ` * invalidate any prior representation and set the string type.` |
|        - | 1099 | ` * Then a simple append operation is performed.` |
|        - | 1100 | ` */` |
|  1400636 | 1101 | `PH7_PRIVATE sxi32 PH7_MemObjStringAppend(ph7_value *pObj,const char *zData,sxu32 nLen)` |
|        5 | 1102 | `{` |
|        - | 1103 | `	sxi32 rc;` |
|  1400641 | 1104 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 1105 | `		/* Invalidate any prior representation */` |
|     1987 | 1106 | `		PH7_MemObjRelease(pObj);` |
|     1987 | 1107 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|      991 | 1108 | `	}` |
|        - | 1109 | `	/* Append contents */` |
|  1400641 | 1110 | `	rc = SyBlobAppend(&pObj->sBlob,zData,nLen);` |
|  1400641 | 1111 | `	return rc;` |
|        5 | 1112 | `}` |
|        - | 1113 | `#if 0` |
|        - | 1114 | `/*` |
|        - | 1115 | ` * Format and append some contents to the internal buffer of a given ph7_value.` |
|        - | 1116 | ` * If the given ph7_value is not of type string,this function invalidate` |
|        - | 1117 | ` * any prior representation and set the string type.` |
|        - | 1118 | ` * Then a simple format and append operation is performed.` |
|        - | 1119 | ` */` |
|        - | 1120 | `PH7_PRIVATE sxi32 PH7_MemObjStringFormat(ph7_value *pObj,const char *zFormat,va_list ap)` |
|        - | 1121 | `{` |
|        - | 1122 | `	sxi32 rc;` |
|        - | 1123 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 1124 | `		/* Invalidate any prior representation */` |
|        - | 1125 | `		PH7_MemObjRelease(pObj);` |
|        - | 1126 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|        - | 1127 | `	}` |
|        - | 1128 | `	/* Format and append contents */` |
|        - | 1129 | `	rc = SyBlobFormatAp(&pObj->sBlob,zFormat,ap);` |
|        - | 1130 | `	return rc;` |
|        - | 1131 | `}` |
|        - | 1132 | `#endif` |
|        - | 1133 | `/*` |
|        - | 1134 | ` * Duplicate the contents of a ph7_value.` |
|        - | 1135 | ` */` |
|  5135089 | 1136 | `PH7_PRIVATE sxi32 PH7_MemObjStore(ph7_value *pSrc,ph7_value *pDest)` |
|        5 | 1137 | `{` |
|  5135094 | 1138 | `	ph7_class_instance *pObj = 0;` |
|  5135094 | 1139 | `	ph7_hashmap *pMap = 0;` |
|        - | 1140 | `	sxi32 rc;` |
|  5135094 | 1141 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1142 | `		/* Increment reference count */` |
|   205667 | 1143 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  5032263 | 1144 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|        - | 1145 | `		/* Increment reference count */` |
|     4737 | 1146 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|     2366 | 1147 | `	}` |
|  5135094 | 1148 | `	if( pDest->iFlags & MEMOBJ_HASHMAP ){` |
|    60427 | 1149 | `		pMap = (ph7_hashmap *)pDest->x.pOther;` |
|  5104883 | 1150 | `	}else if( pDest->iFlags & MEMOBJ_OBJ ){` |
|     5649 | 1151 | `		pObj = (ph7_class_instance *)pDest->x.pOther;` |
|     2822 | 1152 | `	}` |
|  5135094 | 1153 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|  5135094 | 1154 | `	pDest->iFlags &= ~MEMOBJ_AUX;` |
|  5135094 | 1155 | `	rc = SXRET_OK;` |
|  5135094 | 1156 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  3986463 | 1157 | `		SyBlobReset(&pDest->sBlob);` |
|  3986463 | 1158 | `		rc = SyBlobDup(&pSrc->sBlob,&pDest->sBlob);` |
|  1993234 | 1159 | `	}else{` |
|  1148636 | 1160 | `		if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|   284390 | 1161 | `			SyBlobRelease(&pDest->sBlob);` |
|   142235 | 1162 | `		}` |
|        - | 1163 | `	}` |
|  5135094 | 1164 | `	if( pMap ){` |
|    60427 | 1165 | `		PH7_HashmapUnref(pMap);` |
|  5104883 | 1166 | `	}else if( pObj ){` |
|     5649 | 1167 | `		PH7_ClassInstanceUnref(pObj);` |
|     2822 | 1168 | `	}` |
|  5135089 | 1169 | `	if( rc == SXRET_OK && (pDest->iFlags & MEMOBJ_HASHMAP)` |
|  2670418 | 1170 | `	 && pDest->pVm` |
|   205662 | 1171 | `	 && (ph7_hashmap *)pDest->x.pOther == pDest->pVm->pGlobal` |
|        - | 1172 | `	 /* Identity, not nIdx: transient values carry nIdx==0 (SyZero), which` |
|        - | 1173 | `	  * collides with a typical nGlobalIdx of 0 and would skip the snapshot` |
|        - | 1174 | `	  * for closure envs and other non-slot destinations. */` |
|   102840 | 1175 | `	 && pDest != (ph7_value *)SySetAt(&pDest->pVm->aMemObj,pDest->pVm->nGlobalIdx) ){` |
|        - | 1176 | `		/* php 8.1: a COPY of $GLOBALS ($snap = $GLOBALS, $a[] = $GLOBALS,` |
|        - | 1177 | `		 * by-value argument passing, return $GLOBALS, ...) is a by-value` |
|        - | 1178 | `		 * SNAPSHOT of the symbol table with its reference entries` |
|        - | 1179 | `		 * flattened — never a live alias. Materialize it here, the one` |
|        - | 1180 | `		 * store choke point (loads/subscript access keep sharing, so` |
|        - | 1181 | `		 * $GLOBALS[$k] reads and writes stay live). */` |
|        9 | 1182 | `		ph7_hashmap *pSnap = PH7_NewHashmap(pDest->pVm,0,0);` |
|        9 | 1183 | `		if( pSnap && PH7_HashmapDupMaterialized((ph7_hashmap *)pDest->x.pOther,pSnap) == SXRET_OK ){` |
|        9 | 1184 | `			PH7_HashmapUnref((ph7_hashmap *)pDest->x.pOther);` |
|        9 | 1185 | `			pDest->x.pOther = pSnap;` |
|        4 | 1186 | `		}else if( pSnap ){` |
|      ! 0 | 1187 | `			PH7_HashmapUnref(pSnap);` |
|      ! 0 | 1188 | `		}` |
|        4 | 1189 | `	}` |
|  5135094 | 1190 | `	return rc;` |
|        5 | 1191 | `}` |
|        - | 1192 | `/*` |
|        - | 1193 | ` * Duplicate the contents of a ph7_value but do not copy internal` |
|        - | 1194 | ` * buffer contents,simply point to it.` |
|        - | 1195 | ` */` |
|  7020998 | 1196 | `PH7_PRIVATE sxi32 PH7_MemObjLoad(ph7_value *pSrc,ph7_value *pDest)` |
|        5 | 1197 | `{` |
|  7021003 | 1198 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),` |
|        - | 1199 | `		sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|  7021003 | 1200 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1201 | `		/* Increment reference count */` |
|   480727 | 1202 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  6780642 | 1203 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|        - | 1204 | `		/* Increment reference count */` |
|    32029 | 1205 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|    16012 | 1206 | `	}` |
|  7021003 | 1207 | `	if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|       48 | 1208 | `		SyBlobRelease(&pDest->sBlob);` |
|       22 | 1209 | `	}` |
|  7021003 | 1210 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  3684107 | 1211 | `		SyBlobReadOnly(&pDest->sBlob,SyBlobData(&pSrc->sBlob),SyBlobLength(&pSrc->sBlob));` |
|  1842176 | 1212 | `	}` |
|  7021003 | 1213 | `	return SXRET_OK;` |
|        5 | 1214 | `}` |
|        - | 1215 | `/*` |
|        - | 1216 | ` * Invalidate any prior representation of a given ph7_value.` |
|        - | 1217 | ` */` |
| 17690199 | 1218 | `PH7_PRIVATE sxi32 PH7_MemObjRelease(ph7_value *pObj)` |
|        5 | 1219 | `{` |
| 17690204 | 1220 | `	if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
| 14772401 | 1221 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|   659793 | 1222 | `			PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
| 14442507 | 1223 | `		}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|    53909 | 1224 | `			PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|    26952 | 1225 | `		}` |
|        - | 1226 | `		/* Release the internal buffer */` |
| 14772401 | 1227 | `		SyBlobRelease(&pObj->sBlob);` |
|        - | 1228 | `		/* Invalidate any prior representation */` |
| 14772401 | 1229 | `		pObj->iFlags = MEMOBJ_NULL;` |
|  7386580 | 1230 | `	}` |
| 17690204 | 1231 | `	return SXRET_OK;` |
|        5 | 1232 | `}` |
|        - | 1233 | `/*` |
|        - | 1234 | ` * Compare two ph7_values.` |
|        - | 1235 | ` * Return 0 if the values are equals, > 0 if pObj1 is greater than pObj2` |
|        - | 1236 | ` * or < 0 if pObj2 is greater than pObj1.` |
|        - | 1237 | ` * Type comparison table taken from the PHP language reference manual.` |
|        - | 1238 | ` * Comparisons of $x with PHP functions Expression` |
|        - | 1239 | ` *              gettype() 	empty() 	is_null() 	isset() 	boolean : if($x)` |
|        - | 1240 | ` * $x = ""; 	string 	    TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1241 | ` * $x = null 	NULL 	    TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1242 | ` * var $x; 	    NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1243 | ` * $x is undefined 	NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1244 | ` *  $x = array(); 	array 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1245 | ` * $x = false; 	boolean 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1246 | ` * $x = true; 	boolean 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1247 | ` * $x = 1; 	    integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1248 | ` * $x = 42; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1249 | ` * $x = 0; 	    integer 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1250 | ` * $x = -1; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1251 | ` * $x = "1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1252 | ` * $x = "0"; 	string 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1253 | ` * $x = "-1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1254 | ` * $x = "php"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1255 | ` * $x = "true"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1256 | ` * $x = "false"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1257 | ` *      Loose comparisons with ==` |
|        - | 1258 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|        - | 1259 | ` * TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1260 | ` * FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|        - | 1261 | ` * 1 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1262 | ` * 0 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE` |
|        - | 1263 | ` * -1 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1264 | ` * "1" 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1265 | ` * "0" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1266 | ` * "-1" 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1267 | ` * NULL 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|        - | 1268 | ` * array() 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1269 | ` * "php" 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1270 | ` * "" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE` |
|        - | 1271 | ` *    Strict comparisons with ===` |
|        - | 1272 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|        - | 1273 | ` * TRUE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1274 | ` * FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1275 | ` * 1 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1276 | ` * 0 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1277 | ` * -1 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1278 | ` * "1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1279 | ` * "0" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1280 | ` * "-1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1281 | ` * NULL 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE` |
|        - | 1282 | ` * array() 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE` |
|        - | 1283 | ` * "php" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1284 | ` * "" 	    FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE` |
|        - | 1285 | ` */` |
|  1294251 | 1286 | `PH7_PRIVATE sxi32 PH7_MemObjCmp(ph7_value *pObj1,ph7_value *pObj2,int bStrict,int iNest)` |
|        5 | 1287 | `{` |
|        - | 1288 | `	sxi32 iComb;` |
|        - | 1289 | `	sxi32 rc;` |
|  1294256 | 1290 | `	if( bStrict ){` |
|        - | 1291 | `		sxi32 iF1,iF2;` |
|        - | 1292 | `		/* Strict comparisons with === */` |
|   675328 | 1293 | `		iF1 = pObj1->iFlags&~MEMOBJ_AUX;` |
|   675328 | 1294 | `		iF2 = pObj2->iFlags&~MEMOBJ_AUX;` |
|   675328 | 1295 | `		if( iF1 != iF2 ){` |
|        - | 1296 | `			/* Not of the same type */` |
|   188337 | 1297 | `			return 1;` |
|        - | 1298 | `		}` |
|   243496 | 1299 | `	}` |
|        - | 1300 | `	/* Combine flag together */` |
|  1105924 | 1301 | `	iComb = pObj1->iFlags\|pObj2->iFlags;` |
|  1105924 | 1302 | `	if( iComb & (MEMOBJ_NULL\|MEMOBJ_RES\|MEMOBJ_BOOL) ){` |
|        - | 1303 | `		/* Convert to boolean: Keep in mind FALSE < TRUE */` |
|    22559 | 1304 | `		if( (pObj1->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     8243 | 1305 | `			PH7_MemObjToBool(pObj1);` |
|     4119 | 1306 | `		}` |
|    22559 | 1307 | `		if( (pObj2->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     7275 | 1308 | `			PH7_MemObjToBool(pObj2);` |
|     3635 | 1309 | `		}` |
|    22559 | 1310 | `		return (sxi32)((pObj1->x.iVal != 0) - (pObj2->x.iVal != 0));` |
|  1083370 | 1311 | `	}else if ( iComb & MEMOBJ_HASHMAP ){` |
|        - | 1312 | `		/* Hashmap aka 'array' comparison */` |
|       31 | 1313 | `		if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1314 | `			/* Array is always greater */` |
|      ! 0 | 1315 | `			return -1;` |
|        - | 1316 | `		}` |
|       31 | 1317 | `		if( (pObj2->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1318 | `			/* Array is always greater */` |
|      ! 0 | 1319 | `			return 1;` |
|        - | 1320 | `		}` |
|        - | 1321 | `		/* Perform the comparison */` |
|       31 | 1322 | `		rc = PH7_HashmapCmp((ph7_hashmap *)pObj1->x.pOther,(ph7_hashmap *)pObj2->x.pOther,bStrict);` |
|       31 | 1323 | `		return rc;` |
|  1083340 | 1324 | `	}else if(iComb & MEMOBJ_OBJ ){` |
|        - | 1325 | `		/* Object comparison */` |
|      235 | 1326 | `		if( (pObj1->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 1327 | `			/* Object is always greater */` |
|      ! 0 | 1328 | `			return -1;` |
|        - | 1329 | `		}` |
|      235 | 1330 | `		if( (pObj2->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 1331 | `			/* Object is always greater */` |
|      ! 0 | 1332 | `			return 1;` |
|        - | 1333 | `		}` |
|        - | 1334 | `		/* Perform the comparison */` |
|      235 | 1335 | `		rc = PH7_ClassInstanceCmp((ph7_class_instance *)pObj1->x.pOther,(ph7_class_instance *)pObj2->x.pOther,bStrict,iNest);` |
|      235 | 1336 | `		return rc;` |
|  1083110 | 1337 | `	}else if ( iComb & MEMOBJ_STRING ){` |
|        - | 1338 | `		SyString s1,s2;` |
|   694755 | 1339 | `		if( !bStrict ){` |
|        - | 1340 | `			/*` |
|        - | 1341 | `			 * PHP 8 "saner string to number comparisons" (RFC): a numeric` |
|        - | 1342 | `			 * comparison is performed only when BOTH operands are numbers or` |
|        - | 1343 | `			 * numeric strings. A number compared with a NON-numeric string is` |
|        - | 1344 | `			 * compared as strings, with the number cast to its string form —` |
|        - | 1345 | `			 * so 0 == "abc" is false, "abc" < 10 is false, and max("abc",10)` |
|        - | 1346 | `			 * is "abc". (PHP 7 cast the non-numeric string to 0 and compared` |
|        - | 1347 | `			 * numerically; comparing when EITHER side was numeric is what this` |
|        - | 1348 | `			 * replaces.) Two non-numeric strings, or one numeric and one` |
|        - | 1349 | `			 * non-numeric string, still fall through to the string comparison` |
|        - | 1350 | `			 * below, unchanged.` |
|        - | 1351 | `			 */` |
|   231365 | 1352 | `			if( PH7_MemObjIsNumeric(pObj1) && PH7_MemObjIsNumeric(pObj2) ){` |
|        - | 1353 | `				/* Perform a numeric comparison */` |
|       29 | 1354 | `				goto Numeric;` |
|        - | 1355 | `			}` |
|   115686 | 1356 | `		}` |
|        - | 1357 | `		/* Perform a strict string comparison.*/` |
|   694727 | 1358 | `		if( (pObj1->iFlags&MEMOBJ_STRING) == 0 ){` |
|       19 | 1359 | `			PH7_MemObjToString(pObj1);` |
|        9 | 1360 | `		}` |
|   694727 | 1361 | `		if( (pObj2->iFlags&MEMOBJ_STRING) == 0 ){` |
|        5 | 1362 | `			PH7_MemObjToString(pObj2);` |
|        2 | 1363 | `		}` |
|   694727 | 1364 | `		SyStringInitFromBuf(&s1,SyBlobData(&pObj1->sBlob),SyBlobLength(&pObj1->sBlob));` |
|   694727 | 1365 | `		SyStringInitFromBuf(&s2,SyBlobData(&pObj2->sBlob),SyBlobLength(&pObj2->sBlob));` |
|        - | 1366 | `		/*` |
|        - | 1367 | `		 * Strings are compared using memcmp(). If one value is an exact prefix of the` |
|        - | 1368 | `		 * other, then the shorter value is less than the longer value.` |
|        - | 1369 | `		 */` |
|   694727 | 1370 | `		rc = SyMemcmp((const void *)s1.zString,(const void *)s2.zString,SXMIN(s1.nByte,s2.nByte));` |
|   694727 | 1371 | `		if( rc == 0 ){` |
|   232920 | 1372 | `			if( s1.nByte != s2.nByte ){` |
|     2138 | 1373 | `				rc = s1.nByte < s2.nByte ? -1 : 1;` |
|     1069 | 1374 | `			}` |
|   116460 | 1375 | `		}` |
|   694727 | 1376 | `		return rc;` |
|   388360 | 1377 | `	}else if( iComb & (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|   194134 | 1378 | `Numeric:` |
|        - | 1379 | `		/* Perform a numeric comparison if one of the operand is numeric(integer or real) */` |
|   388388 | 1380 | `		if( (pObj1->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|       11 | 1381 | `			PH7_MemObjToNumeric(pObj1);` |
|        5 | 1382 | `		}` |
|   388388 | 1383 | `		if( (pObj2->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|       19 | 1384 | `			PH7_MemObjToNumeric(pObj2);` |
|        9 | 1385 | `		}` |
|   388388 | 1386 | `		if( (pObj1->iFlags & pObj2->iFlags & MEMOBJ_INT) == 0) {` |
|        - | 1387 | `			/*` |
|        - | 1388 | `			 * Symisc eXtension to the PHP language:` |
|        - | 1389 | `			 *  Floating point comparison is introduced and works as expected.` |
|        - | 1390 | `			 */` |
|        - | 1391 | `			ph7_real r1,r2;` |
|        - | 1392 | `			/* Compare as reals */` |
|      261 | 1393 | `			if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|       11 | 1394 | `				PH7_MemObjToReal(pObj1);` |
|        5 | 1395 | `			}` |
|      261 | 1396 | `			r1 = pObj1->rVal;` |
|      261 | 1397 | `			if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|       51 | 1398 | `				PH7_MemObjToReal(pObj2);` |
|       25 | 1399 | `			}` |
|      261 | 1400 | `			r2 = pObj2->rVal;` |
|      261 | 1401 | `			if( PH7_IS_NAN(r1) \|\| PH7_IS_NAN(r2) ){` |
|        - | 1402 | `				/*` |
|        - | 1403 | `				 * Keep a strict three-way comparator contract even for NaN values.` |
|        - | 1404 | `				 * For ordering purposes, NaN compares equal to NaN and greater than` |
|        - | 1405 | `				 * any non-NaN numeric value.` |
|        - | 1406 | `				 */` |
|       45 | 1407 | `				if( PH7_IS_NAN(r1) ){` |
|       35 | 1408 | `					return PH7_IS_NAN(r2) ? 0 : 1;` |
|        - | 1409 | `				}` |
|       11 | 1410 | `				return -1;` |
|        - | 1411 | `			}` |
|      217 | 1412 | `			if( r1 > r2 ){` |
|       45 | 1413 | `				return 1;` |
|      173 | 1414 | `			}else if( r1 < r2 ){` |
|      125 | 1415 | `				return -1;` |
|        - | 1416 | `			}` |
|       49 | 1417 | `			return 0;` |
|      ! 0 | 1418 | `		}else{` |
|        - | 1419 | `			/* Integer comparison */` |
|   388128 | 1420 | `			if( pObj1->x.iVal > pObj2->x.iVal ){` |
|     6867 | 1421 | `				return 1;` |
|   381266 | 1422 | `			}else if( pObj1->x.iVal < pObj2->x.iVal ){` |
|   374825 | 1423 | `				return -1;` |
|        - | 1424 | `			}` |
|     6446 | 1425 | `			return 0;` |
|        - | 1426 | `		}` |
|        - | 1427 | `	}` |
|        - | 1428 | `	/* NOT REACHED */` |
|      ! 0 | 1429 | `	return 0;` |
|   647194 | 1430 | `}` |
|        - | 1431 | `/*` |
|        - | 1432 | ` * Perform an addition operation of two ph7_values.` |
|        - | 1433 | ` * The reason this function is implemented here rather than 'vm.c'` |
|        - | 1434 | ` * is that the '+' operator is overloaded.` |
|        - | 1435 | ` * That is,the '+' operator is used for arithmetic operation and also` |
|        - | 1436 | ` * used for operation on arrays [i.e: union]. When used with an array` |
|        - | 1437 | ` * The + operator returns the right-hand array appended to the left-hand array.` |
|        - | 1438 | ` * For keys that exist in both arrays, the elements from the left-hand array` |
|        - | 1439 | ` * will be used, and the matching elements from the right-hand array will` |
|        - | 1440 | ` * be ignored.` |
|        - | 1441 | ` * This function take care of handling all the scenarios.` |
|        - | 1442 | ` */` |
|     9992 | 1443 | `PH7_PRIVATE sxi32 PH7_MemObjAdd(ph7_value *pObj1,ph7_value *pObj2,int bAddStore)` |
|        5 | 1444 | `{` |
|     9997 | 1445 | `	if( ((pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1446 | `			/* Arithemtic operation */` |
|     6181 | 1447 | `			PH7_MemObjToNumeric(pObj1);` |
|     6181 | 1448 | `			PH7_MemObjToNumeric(pObj2);` |
|     6181 | 1449 | `			if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_REAL ){` |
|        - | 1450 | `				/* Floating point arithmetic */` |
|        - | 1451 | `				ph7_real a,b;` |
|       67 | 1452 | `				if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|       31 | 1453 | `					PH7_MemObjToReal(pObj1);` |
|       15 | 1454 | `				}` |
|       67 | 1455 | `				if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 | 1456 | `					PH7_MemObjToReal(pObj2);` |
|        2 | 1457 | `				}` |
|       67 | 1458 | `				a = pObj1->rVal;` |
|       67 | 1459 | `				b = pObj2->rVal;` |
|       67 | 1460 | `				pObj1->rVal = a+b;` |
|       67 | 1461 | `				MemObjSetType(pObj1,MEMOBJ_REAL);` |
|        - | 1462 | `				/* Try to get an integer representation also */` |
|       67 | 1463 | `				MemObjTryIntger(&(*pObj1));` |
|       34 | 1464 | `			}else{` |
|        - | 1465 | `				/* Integer arithmetic */` |
|        - | 1466 | `				sxi64 a,b;` |
|     6115 | 1467 | `				a = pObj1->x.iVal;` |
|     6115 | 1468 | `				b = pObj2->x.iVal;` |
|     6115 | 1469 | `				pObj1->x.iVal = a+b;` |
|     6115 | 1470 | `				MemObjSetType(pObj1,MEMOBJ_INT);` |
|        - | 1471 | `			}` |
|     3093 | 1472 | `	}else{` |
|     3821 | 1473 | `		if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP ){` |
|        - | 1474 | `			ph7_hashmap *pMap;` |
|        - | 1475 | `			sxi32 rc;` |
|     3821 | 1476 | `			if( bAddStore ){` |
|        - | 1477 | `				/* Do not duplicate the hashmap,use the left one since its an add&store operation.` |
|        - | 1478 | `				 */` |
|        3 | 1479 | `				if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1480 | `					/* Force a hashmap cast */` |
|      ! 0 | 1481 | `					rc = PH7_MemObjToHashmap(pObj1);` |
|      ! 0 | 1482 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 1483 | `						PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|      ! 0 | 1484 | `						return rc;` |
|        - | 1485 | `					}` |
|      ! 0 | 1486 | `				}` |
|        - | 1487 | `				/* COW separate before in-place mutation */` |
|        3 | 1488 | `				pMap = PH7_HashmapCowSeparate(pObj1->pVm,pObj1);` |
|        2 | 1489 | `			}else{` |
|        - | 1490 | `				/* Create a new hashmap */` |
|     3819 | 1491 | `				pMap = PH7_NewHashmap(pObj1->pVm,0,0);` |
|     3819 | 1492 | `				if( pMap == 0){` |
|      ! 0 | 1493 | `					PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|      ! 0 | 1494 | `					return SXERR_MEM;` |
|        - | 1495 | `				}` |
|        - | 1496 | `			}` |
|     3821 | 1497 | `			if( !bAddStore ){` |
|     3819 | 1498 | `				if(pObj1->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1499 | `					/* Perform a hashmap duplication */` |
|     3819 | 1500 | `					PH7_HashmapDup((ph7_hashmap *)pObj1->x.pOther,pMap);` |
|     1912 | 1501 | `				}else{` |
|      ! 0 | 1502 | `					if((pObj1->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 1503 | `						/* Simple insertion */` |
|      ! 0 | 1504 | `						PH7_HashmapInsert(pMap,0,pObj1);` |
|      ! 0 | 1505 | `					}` |
|        - | 1506 | `				}` |
|     1907 | 1507 | `			}` |
|        - | 1508 | `			/* Perform the union */` |
|     3821 | 1509 | `			if(pObj2->iFlags & MEMOBJ_HASHMAP ){` |
|     3821 | 1510 | `				PH7_HashmapUnion(pMap,(ph7_hashmap *)pObj2->x.pOther);` |
|     1913 | 1511 | `			}else{` |
|      ! 0 | 1512 | `				if((pObj2->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 1513 | `					/* Simple insertion */` |
|      ! 0 | 1514 | `					PH7_HashmapInsert(pMap,0,pObj2);` |
|      ! 0 | 1515 | `				}` |
|        - | 1516 | `			}` |
|        - | 1517 | `			/* Reflect the change */` |
|     3821 | 1518 | `			if( pObj1->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 1519 | `				SyBlobRelease(&pObj1->sBlob);` |
|      ! 0 | 1520 | `			}` |
|     3821 | 1521 | `			pObj1->x.pOther = pMap;` |
|     3821 | 1522 | `			MemObjSetType(pObj1,MEMOBJ_HASHMAP);` |
|     1908 | 1523 | `		}` |
|        - | 1524 | `	}` |
|     9997 | 1525 | `	return SXRET_OK;` |
|     5001 | 1526 | `}` |
|        - | 1527 | `/*` |
|        - | 1528 | ` * Return a printable representation of the type of a given` |
|        - | 1529 | ` * ph7_value.` |
|        - | 1530 | ` */` |
|      464 | 1531 | `PH7_PRIVATE const char * PH7_MemObjTypeDump(ph7_value *pVal)` |
|        4 | 1532 | `{` |
|      468 | 1533 | `	const char *zType = "";` |
|      468 | 1534 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|        5 | 1535 | `		zType = "null";` |
|      466 | 1536 | `	}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|        - | 1537 | `		/* REAL is authoritative over a cached MEMOBJ_INT: an integer-valued` |
|        - | 1538 | `		 * real (e.g. 1.0) is reported as "double", matching PHP's gettype(). */` |
|        7 | 1539 | `		zType = "double";` |
|      461 | 1540 | `	}else if( pVal->iFlags & MEMOBJ_INT ){` |
|      127 | 1541 | `		zType = "int";` |
|      396 | 1542 | `	}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|       76 | 1543 | `		zType = "string";` |
|      298 | 1544 | `	}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|      109 | 1545 | `		zType = "bool";` |
|      208 | 1546 | `	}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|       16 | 1547 | `		zType = "array";` |
|      148 | 1548 | `	}else if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      141 | 1549 | `		zType = "object";` |
|       69 | 1550 | `	}else if( pVal->iFlags & MEMOBJ_RES ){` |
|      ! 0 | 1551 | `		zType = "resource";` |
|      ! 0 | 1552 | `	}` |
|      468 | 1553 | `	return zType;` |
|        4 | 1554 | `}` |
|        - | 1555 | `/*` |
|        - | 1556 | ` * Dump a ph7_value [i.e: get a printable representation of it's type and contents.].` |
|        - | 1557 | ` * Store the dump in the given blob.` |
|        - | 1558 | ` */` |
|      478 | 1559 | `PH7_PRIVATE sxi32 PH7_MemObjDump(` |
|        - | 1560 | `	SyBlob *pOut,      /* Store the dump here */` |
|        - | 1561 | `	ph7_value *pObj,   /* Dump this */` |
|        - | 1562 | `	int ShowType,      /* TRUE to output value type */` |
|        - | 1563 | `	int nTab,          /* # of Whitespace to insert */` |
|        - | 1564 | `	int nDepth,        /* Nesting level */` |
|        - | 1565 | `	int isRef          /* TRUE if referenced object */` |
|        - | 1566 | `	)` |
|        4 | 1567 | `{` |
|      482 | 1568 | `	sxi32 rc = SXRET_OK;` |
|        - | 1569 | `	const char *zType;` |
|        - | 1570 | `	int i;` |
|     4598 | 1571 | `	for( i = 0 ; i < nTab ; i++ ){` |
|     4119 | 1572 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     2061 | 1573 | `	}` |
|      482 | 1574 | `	if( ShowType ){` |
|      434 | 1575 | `		if( isRef ){` |
|      ! 0 | 1576 | `			SyBlobAppend(&(*pOut),"&",sizeof(char));` |
|      ! 0 | 1577 | `		}` |
|        - | 1578 | `		/* Get value type first. var_dump() labels reals "float" (PHP), whereas` |
|        - | 1579 | `		 * gettype()/PH7_MemObjTypeDump use the legacy "double" spelling. */` |
|      434 | 1580 | `		if( (pObj->iFlags & MEMOBJ_REAL) && (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        5 | 1581 | `			zType = "float";` |
|        3 | 1582 | `		}else{` |
|      430 | 1583 | `			zType = PH7_MemObjTypeDump(pObj);` |
|        - | 1584 | `		}` |
|      434 | 1585 | `		SyBlobAppend(&(*pOut),zType,SyStrlen(zType));` |
|      215 | 1586 | `	}` |
|      482 | 1587 | `	if((pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|      480 | 1588 | `		if ( ShowType ){` |
|      432 | 1589 | `			SyBlobAppend(&(*pOut),"(",sizeof(char));` |
|      214 | 1590 | `		}` |
|      480 | 1591 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1592 | `			/* Dump hashmap entries */` |
|       24 | 1593 | `			rc = PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,ShowType,nTab+1,nDepth+1);` |
|      469 | 1594 | `		}else if(pObj->iFlags & MEMOBJ_OBJ ){` |
|        - | 1595 | `			/* Dump class instance attributes */` |
|      141 | 1596 | `			rc = PH7_ClassInstanceDump(&(*pOut),(ph7_class_instance *)pObj->x.pOther,ShowType,nTab+1,nDepth+1);` |
|       72 | 1597 | `		}else{` |
|      320 | 1598 | `			SyBlob *pContents = &pObj->sBlob;` |
|        - | 1599 | `			/* Get a printable representation of the contents */` |
|      320 | 1600 | `			if((pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|      252 | 1601 | `				MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|      128 | 1602 | `			}else{` |
|        - | 1603 | `				/* PHP format: string(N) "content" */` |
|       71 | 1604 | `				if( ShowType ){` |
|       53 | 1605 | `					SyBlobFormat(&(*pOut),"%u) \"",SyBlobLength(&pObj->sBlob));` |
|       25 | 1606 | `				}` |
|       71 | 1607 | `				if( SyBlobLength(pContents) > 0 ){` |
|       69 | 1608 | `					SyBlobAppend(&(*pOut),SyBlobData(pContents),SyBlobLength(pContents));` |
|       33 | 1609 | `				}` |
|       71 | 1610 | `				if( ShowType ){` |
|       53 | 1611 | `					SyBlobAppend(&(*pOut),"\"",sizeof(char));` |
|       25 | 1612 | `				}` |
|        - | 1613 | `			}` |
|        - | 1614 | `		}` |
|      480 | 1615 | `		if( ShowType ){` |
|        - | 1616 | `			/* Strings already emitted their own ')' as part of the` |
|        - | 1617 | `			 * "N) \"content\"" format above. */` |
|      432 | 1618 | `			if( (pObj->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_STRING)) == 0 ){` |
|      236 | 1619 | `				SyBlobAppend(&(*pOut),")",sizeof(char));` |
|      116 | 1620 | `			}` |
|      214 | 1621 | `		}` |
|      238 | 1622 | `	}` |
|        - | 1623 | `#ifdef __WINNT__` |
|        4 | 1624 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|        - | 1625 | `#else` |
|      478 | 1626 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        - | 1627 | `#endif` |
|      482 | 1628 | `	return rc;` |
|        4 | 1629 | `}` |
|        - | 1630 |  |
