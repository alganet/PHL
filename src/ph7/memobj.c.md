# src/ph7/memobj.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 839/922 lines (91.00%)

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
|      452 |   13 | `PH7_PRIVATE const char *ph7_type_name(ph7_value *pVal)` |
|        5 |   14 | `{` |
|      457 |   15 | `	if( ph7_value_is_null(pVal) ) return "null";` |
|      415 |   16 | `	if( ph7_value_is_bool(pVal) ) return "bool";` |
|      407 |   17 | `	if( ph7_value_is_int(pVal) ) return "int";` |
|      349 |   18 | `	if( ph7_value_is_float(pVal) ) return "float";` |
|      339 |   19 | `	if( ph7_value_is_string(pVal) ) return "string";` |
|      153 |   20 | `	if( ph7_value_is_array(pVal) ) return "array";` |
|       42 |   21 | `	if( ph7_value_is_object(pVal) ) return "object";` |
|        3 |   22 | `	if( ph7_value_is_resource(pVal) ) return "resource";` |
|      ! 0 |   23 | `	return "unknown";` |
|      231 |   24 | `}` |
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
|     2996 |   42 | `static sxi64 MemObjRealToInt(ph7_value *pObj)` |
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
|     3001 |   59 | `  ph7_real r = pObj->rVal;` |
|     3001 |   60 | `  if( r<(ph7_real)minInt ){` |
|        3 |   61 | `    return minInt;` |
|     2999 |   62 | `  }else if( r>(ph7_real)maxInt ){` |
|        - |   63 | `    /* minInt is correct here - not maxInt.  It turns out that assigning` |
|        - |   64 | `    ** a very large positive number to an integer results in a very large` |
|        - |   65 | `    ** negative integer.  This makes no sense, but it is what x86 hardware` |
|        - |   66 | `    ** does so for compatibility we will do the same in software. */` |
|      155 |   67 | `    return minInt;` |
|      ! 0 |   68 | `  }else{` |
|     2845 |   69 | `    return (sxi64)r;` |
|        - |   70 | `  }` |
|        - |   71 | `#endif` |
|     1503 |   72 | `}` |
|        - |   73 | `/*` |
|        - |   74 | ` * Convert a raw token value typically a stream of digit [i.e: hex,octal,binary or decimal]` |
|        - |   75 | ` * to a 64-bit integer.` |
|        - |   76 | ` */` |
|  1279830 |   77 | `PH7_PRIVATE sxi64 PH7_TokenValueToInt64(SyString *pVal)` |
|        5 |   78 | `{` |
|  1279835 |   79 | `	sxi64 iVal = 0;` |
|  1279835 |   80 | `	if( pVal->nByte <= 0 ){` |
|        7 |   81 | `		return 0;` |
|        - |   82 | `	}` |
|  1279829 |   83 | `	if( pVal->zString[0] == '0' ){` |
|        - |   84 | `		sxi32 c;` |
|   353671 |   85 | `		if( pVal->nByte == sizeof(char) ){` |
|   353259 |   86 | `			return 0;` |
|        - |   87 | `		}` |
|      413 |   88 | `		c = pVal->zString[1];` |
|      413 |   89 | `		if( c  == 'x' \|\| c == 'X' ){` |
|        - |   90 | `			/* Hex digit stream */` |
|       71 |   91 | `			SyHexStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|      378 |   92 | `		}else if( c == 'b' \|\| c == 'B' ){` |
|        - |   93 | `			/* Binary digit stream */` |
|      279 |   94 | `			SyBinaryStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|      140 |   95 | `		}else{` |
|        - |   96 | `			/* Octal digit stream */` |
|       65 |   97 | `			SyOctalStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|        - |   98 | `		}` |
|      207 |   99 | `	}else{` |
|        - |  100 | `		/* Decimal digit stream */` |
|   926163 |  101 | `		SyStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|        - |  102 | `	}` |
|   926575 |  103 | `	return iVal;` |
|   639920 |  104 | `}` |
|        - |  105 | `/*` |
|        - |  106 | ` * Return some kind of 64-bit integer value which is the best we can` |
|        - |  107 | ` * do at representing the value that pObj describes as a string` |
|        - |  108 | ` * representation.` |
|        - |  109 | ` */` |
|      500 |  110 | `static sxi64 MemObjStringToInt(ph7_value *pObj)` |
|        5 |  111 | `{` |
|        - |  112 | `	SyString sVal;` |
|      505 |  113 | `	SyStringInitFromBuf(&sVal,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|      505 |  114 | `	return PH7_TokenValueToInt64(&sVal);` |
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
|      586 |  150 | `static sxi64 MemObjIntValue(ph7_value *pObj)` |
|        5 |  151 | `{` |
|        - |  152 | `	sxi32 iFlags;` |
|      591 |  153 | `	iFlags = pObj->iFlags;` |
|      591 |  154 | `	if (iFlags & MEMOBJ_REAL ){` |
|       39 |  155 | `		return MemObjRealToInt(&(*pObj));` |
|      553 |  156 | `	}else if( iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      142 |  157 | `		return pObj->x.iVal;` |
|      413 |  158 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|      391 |  159 | `		return MemObjStringToInt(&(*pObj));` |
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
|      298 |  188 | `}` |
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
|     1772 |  199 | `static ph7_real MemObjRealValue(ph7_value *pObj)` |
|        5 |  200 | `{` |
|        - |  201 | `	sxi32 iFlags;` |
|     1777 |  202 | `	iFlags = pObj->iFlags;` |
|     1777 |  203 | `	if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  204 | `		return pObj->rVal;` |
|     1777 |  205 | `	}else if (iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      772 |  206 | `		return (ph7_real)pObj->x.iVal;` |
|     1006 |  207 | `	}else if (iFlags & MEMOBJ_STRING){` |
|        - |  208 | `		SyString sString;` |
|        - |  209 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  210 | `		ph7_real rVal = 0;` |
|        - |  211 | `#else` |
|     1000 |  212 | `		ph7_real rVal = 0.0;` |
|        - |  213 | `#endif` |
|     1000 |  214 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|     1000 |  215 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|        - |  216 | `			/* Convert as much as we can */` |
|        - |  217 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  218 | `			rVal = MemObjStringToInt(&(*pObj));` |
|        - |  219 | `#else` |
|     1000 |  220 | `			SyStrToReal(sString.zString,sString.nByte,(void *)&rVal,0);` |
|        - |  221 | `#endif` |
|      498 |  222 | `		}` |
|     1000 |  223 | `		return rVal;` |
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
|      891 |  256 | `}` |
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
|      498 |  270 | `PH7_PRIVATE sxi32 PH7_PhpFloatShape(char *zBuf,sxi32 nLen,int bGeneric)` |
|        4 |  271 | `{` |
|        - |  272 | `	sxi32 iExp,i;` |
|      502 |  273 | `	iExp = nLen - 1;` |
|     4198 |  274 | `	while( iExp > 0 && zBuf[iExp] != 'e' && zBuf[iExp] != 'E' ){` |
|     3700 |  275 | `		iExp--;` |
|        4 |  276 | `	}` |
|      502 |  277 | `	if( iExp <= 0 ){` |
|      456 |  278 | `		return nLen; /* No exponent part (fixed notation) */` |
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
|      253 |  314 | `}` |
|        - |  315 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|        - |  316 | `/*` |
|        - |  317 | ` * Return the string representation of a given ph7_value.` |
|        - |  318 | ` * This function never fail and always return SXRET_OK.` |
|        - |  319 | ` */` |
|    57314 |  320 | `static sxi32 MemObjStringValue(SyBlob *pOut,ph7_value *pObj,sxu8 bStrictBool)` |
|        5 |  321 | `{` |
|    57319 |  322 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|        - |  323 | `		/* Handle special floating-point values first */` |
|      386 |  324 | `		if( PH7_IS_NAN(pObj->rVal) ){` |
|      ! 0 |  325 | `			SyBlobAppend(&(*pOut),"NAN",3);` |
|      386 |  326 | `		}else if( PH7_IS_INF(pObj->rVal) ){` |
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
|      382 |  341 | `			sxi32 n = (sxi32)snprintf(zNum,sizeof(zNum),"%.14G",pObj->rVal);` |
|      382 |  342 | `			if( n < 0 \|\| n >= (sxi32)sizeof(zNum) ){` |
|      ! 0 |  343 | `				n = (sxi32)SyStrlen(zNum);` |
|      ! 0 |  344 | `			}` |
|      382 |  345 | `			n = PH7_PhpFloatShape(zNum,n,TRUE);` |
|      382 |  346 | `			SyBlobAppend(&(*pOut),zNum,(sxu32)n);` |
|        - |  347 | `#else` |
|        - |  348 | `			SyBlobFormat(&(*pOut),"%.15g",pObj->rVal);` |
|        - |  349 | `#endif` |
|        4 |  350 | `		}` |
|    57128 |  351 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|    56331 |  352 | `		SyBlobFormat(&(*pOut),"%qd",pObj->x.iVal);` |
|        - |  353 | `		/* %qd (BSD quad) is equivalent to %lld in the libc printf */` |
|    28774 |  354 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
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
|      433 |  369 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|      ! 0 |  370 | `		SyBlobAppend(&(*pOut),"Array",sizeof("Array")-1);` |
|      ! 0 |  371 | `		PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
|      255 |  372 | `	}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
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
|       79 |  384 | `			SyBlobAppend(&(*pOut),"Object",sizeof("Object")-1);` |
|        - |  385 | `		}` |
|      169 |  386 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|      169 |  387 | `		PH7_MemObjRelease(&sResult);` |
|      171 |  388 | `	}else if(pObj->iFlags & MEMOBJ_RES ){` |
|        3 |  389 | `		SyBlobFormat(&(*pOut),"ResourceID_%#x",pObj->x.pOther);` |
|        1 |  390 | `	}` |
|    57319 |  391 | `	return SXRET_OK;` |
|        5 |  392 | `}` |
|        - |  393 | `/*` |
|        - |  394 | ` * Return some kind of boolean value which is the best we can do` |
|        - |  395 | ` * at representing the value that pObj describes as a boolean.` |
|        - |  396 | ` * When converting to boolean, the following values are considered FALSE` |
|        - |  397 | ` * (php's exact set):` |
|        - |  398 | ` * NULL` |
|        - |  399 | ` * the boolean FALSE itself.` |
|        - |  400 | ` * the integer 0 (zero).` |
|        - |  401 | ` * the real 0.0 (zero).` |
|        - |  402 | ` * the empty string "" and the string "0" (nothing else: "00", "0.0", " ",` |
|        - |  403 | ` * and "false" are all TRUE in php — the historical PH7 zero-stream and` |
|        - |  404 | ` * "false"/"on"/"yes" special cases changed the meaning of valid PHP source` |
|        - |  405 | ` * and were removed under the §10 PH7-ism policy).` |
|        - |  406 | ` * an array with zero elements.` |
|        - |  407 | ` */` |
|    16118 |  408 | `static sxi32 MemObjBooleanValue(ph7_value *pObj)` |
|        5 |  409 | `{` |
|        - |  410 | `	sxi32 iFlags;` |
|    16123 |  411 | `	iFlags = pObj->iFlags;` |
|    16123 |  412 | `	if (iFlags & MEMOBJ_REAL ){` |
|        - |  413 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  414 | `		return pObj->rVal ? 1 : 0;` |
|        - |  415 | `#else` |
|       14 |  416 | `		return pObj->rVal != 0.0 ? 1 : 0;` |
|        - |  417 | `#endif` |
|    16111 |  418 | `	}else if( iFlags & MEMOBJ_INT ){` |
|      285 |  419 | `		return pObj->x.iVal ? 1 : 0;` |
|    15831 |  420 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|        - |  421 | `		SyString sString;` |
|      111 |  422 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|        - |  423 | `		/* php: a string is FALSE iff it is empty or exactly "0" */` |
|      111 |  424 | `		if( sString.nByte == 0 ){` |
|       19 |  425 | `			return 0;` |
|        - |  426 | `		}` |
|       94 |  427 | `		if( sString.nByte == 1 && sString.zString[0] == '0' ){` |
|        7 |  428 | `			return 0;` |
|        - |  429 | `		}` |
|       88 |  430 | `		return 1;` |
|    15723 |  431 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|    14671 |  432 | `		return 0;` |
|     1057 |  433 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|       22 |  434 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|       22 |  435 | `		sxu32 n = pMap->nEntry;` |
|       22 |  436 | `		PH7_HashmapUnref(pMap);` |
|       22 |  437 | `		return n > 0 ? TRUE : FALSE;` |
|     1037 |  438 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|        - |  439 | `		ph7_value sResult;` |
|        7 |  440 | `		sxi32 iVal = 1;` |
|        - |  441 | `		sxi32 rc;` |
|        - |  442 | `		/* Invoke the __toBool() method if available [note that this is a symisc extension]  */` |
|        7 |  443 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|        7 |  444 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|        - |  445 | `			"__toBool",sizeof("__toBool")-1,&sResult);` |
|        7 |  446 | `		if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL)) ){` |
|        - |  447 | `			/* Extract method return value */` |
|        5 |  448 | `			iVal = (sxi32)(sResult.x.iVal != 0); /* Stupid cc warning -W -Wall -O6 */` |
|        2 |  449 | `		}` |
|        7 |  450 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|        7 |  451 | `		PH7_MemObjRelease(&sResult);` |
|        7 |  452 | `		return iVal;` |
|     1031 |  453 | `	}else if(iFlags & MEMOBJ_RES ){` |
|     1031 |  454 | `		return pObj->x.pOther != 0;` |
|        - |  455 | `	}` |
|        - |  456 | `	/* NOT REACHED */` |
|      ! 0 |  457 | `	return 0;` |
|     8064 |  458 | `}` |
|        - |  459 | `/*` |
|        - |  460 | ` * If the ph7_value is of type real,try to make it an integer also.` |
|        - |  461 | ` */` |
|     2958 |  462 | `static sxi32 MemObjTryIntger(ph7_value *pObj)` |
|        5 |  463 | `{` |
|     2963 |  464 | `	pObj->x.iVal = MemObjRealToInt(&(*pObj));` |
|        - |  465 | `  /* Only mark the value as an integer if` |
|        - |  466 | `  **` |
|        - |  467 | `  **    (1) the round-trip conversion real->int->real is a no-op, and` |
|        - |  468 | `  **    (2) The integer is neither the largest nor the smallest` |
|        - |  469 | `  **        possible integer` |
|        - |  470 | `  **` |
|        - |  471 | `  ** The second and third terms in the following conditional enforces` |
|        - |  472 | `  ** the second condition under the assumption that addition overflow causes` |
|        - |  473 | `  ** values to wrap around.  On x86 hardware, the third term is always` |
|        - |  474 | `  ** true and could be omitted.  But we leave it in because other` |
|        - |  475 | `  ** architectures might behave differently.` |
|        - |  476 | `  */` |
|     2958 |  477 | `	if( pObj->rVal ==(ph7_real)pObj->x.iVal && pObj->x.iVal>SMALLEST_INT64` |
|     1550 |  478 | `      && pObj->x.iVal<LARGEST_INT64 ){` |
|     1539 |  479 | `		  pObj->iFlags \|= MEMOBJ_INT;` |
|      769 |  480 | `	}` |
|     2963 |  481 | `	return SXRET_OK;` |
|        5 |  482 | `}` |
|        - |  483 | `/*` |
|        - |  484 | ` * Convert a ph7_value to type integer.Invalidate any prior representations.` |
|        - |  485 | ` */` |
|   433682 |  486 | `PH7_PRIVATE sxi32 PH7_MemObjToInteger(ph7_value *pObj)` |
|        5 |  487 | `{` |
|   433687 |  488 | `	if( (pObj->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  489 | `		/* Preform the conversion */` |
|      591 |  490 | `		pObj->x.iVal = MemObjIntValue(&(*pObj));` |
|        - |  491 | `		/* Invalidate any prior representations */` |
|      591 |  492 | `		SyBlobRelease(&pObj->sBlob);` |
|      591 |  493 | `		MemObjSetType(pObj,MEMOBJ_INT);` |
|      293 |  494 | `	}` |
|   433687 |  495 | `	return SXRET_OK;` |
|        5 |  496 | `}` |
|        - |  497 | `/*` |
|        - |  498 | ` * Convert a ph7_value to type real (Try to get an integer representation also).` |
|        - |  499 | ` * Invalidate any prior representations` |
|        - |  500 | ` */` |
|     2684 |  501 | `PH7_PRIVATE sxi32 PH7_MemObjToReal(ph7_value *pObj)` |
|        5 |  502 | `{` |
|     2689 |  503 | `	if((pObj->iFlags & MEMOBJ_REAL) == 0 ){` |
|        - |  504 | `		/* Preform the conversion */` |
|     1777 |  505 | `		pObj->rVal = MemObjRealValue(&(*pObj));` |
|        - |  506 | `		/* Invalidate any prior representations */` |
|     1777 |  507 | `		SyBlobRelease(&pObj->sBlob);` |
|     1777 |  508 | `		MemObjSetType(pObj,MEMOBJ_REAL);` |
|        - |  509 | `		/* Try to get an integer representation */` |
|     1777 |  510 | `		MemObjTryIntger(&(*pObj));` |
|      886 |  511 | `	}` |
|     2689 |  512 | `	return SXRET_OK;` |
|        5 |  513 | `}` |
|        - |  514 | `/*` |
|        - |  515 | ` * Convert a ph7_value to type boolean.Invalidate any prior representations.` |
|        - |  516 | ` */` |
|    17730 |  517 | `PH7_PRIVATE sxi32 PH7_MemObjToBool(ph7_value *pObj)` |
|        5 |  518 | `{` |
|    17735 |  519 | `	if( (pObj->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        - |  520 | `		/* Preform the conversion */` |
|    16123 |  521 | `		pObj->x.iVal = MemObjBooleanValue(&(*pObj));` |
|        - |  522 | `		/* Invalidate any prior representations */` |
|    16123 |  523 | `		SyBlobRelease(&pObj->sBlob);` |
|    16123 |  524 | `		MemObjSetType(pObj,MEMOBJ_BOOL);` |
|     8059 |  525 | `	}` |
|    17735 |  526 | `	return SXRET_OK;` |
|        5 |  527 | `}` |
|        - |  528 | `/*` |
|        - |  529 | ` * Convert a ph7_value to type string.Prior representations are NOT invalidated.` |
|        - |  530 | ` */` |
|   842843 |  531 | `PH7_PRIVATE sxi32 PH7_MemObjToString(ph7_value *pObj)` |
|        5 |  532 | `{` |
|   842848 |  533 | `	sxi32 rc = SXRET_OK;` |
|   842848 |  534 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  535 | `		/* Perform the conversion */` |
|    57071 |  536 | `		SyBlobReset(&pObj->sBlob); /* Reset the internal buffer */` |
|    57071 |  537 | `		rc = MemObjStringValue(&pObj->sBlob,&(*pObj),TRUE);` |
|    57071 |  538 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|    28533 |  539 | `	}` |
|   842848 |  540 | `	return rc;` |
|        5 |  541 | `}` |
|        - |  542 | `/*` |
|        - |  543 | ` * Nullify a ph7_value.In other words invalidate any prior` |
|        - |  544 | ` * representation.` |
|        - |  545 | ` */` |
|      ! 0 |  546 | `PH7_PRIVATE sxi32 PH7_MemObjToNull(ph7_value *pObj)` |
|      ! 0 |  547 | `{` |
|      ! 0 |  548 | `	return PH7_MemObjRelease(pObj);` |
|      ! 0 |  549 | `}` |
|        - |  550 | `/*` |
|        - |  551 | ` * Convert a ph7_value to type array.Invalidate any prior representations.` |
|        - |  552 | `  * According to the PHP language reference manual.` |
|        - |  553 | `  *   For any of the types: integer, float, string, boolean converting a value` |
|        - |  554 | `  *   to an array results in an array with a single element with index zero` |
|        - |  555 | `  *   and the value of the scalar which was converted.` |
|        - |  556 | `  */` |
|      398 |  557 | `PH7_PRIVATE sxi32 PH7_MemObjToHashmap(ph7_value *pObj)` |
|        5 |  558 | `{` |
|      403 |  559 | `	if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  560 | `		ph7_hashmap *pMap;` |
|        - |  561 | `		/* Allocate a new hashmap instance */` |
|      261 |  562 | `		pMap = PH7_NewHashmap(pObj->pVm,0,0);` |
|      261 |  563 | `		if( pMap == 0 ){` |
|      ! 0 |  564 | `			return SXERR_MEM;` |
|        - |  565 | `		}` |
|      261 |  566 | `		if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|        - |  567 | `			/*` |
|        - |  568 | `			 * According to the PHP language reference manual.` |
|        - |  569 | `			 *   For any of the types: integer, float, string, boolean converting a value` |
|        - |  570 | `			 *   to an array results in an array with a single element with index zero` |
|        - |  571 | `			 *   and the value of the scalar which was converted.` |
|        - |  572 | `			 */` |
|       27 |  573 | `			if( pObj->iFlags & MEMOBJ_OBJ ){` |
|        - |  574 | `				/* Object cast */` |
|       13 |  575 | `				PH7_ClassInstanceToHashmap((ph7_class_instance *)pObj->x.pOther,pMap);` |
|        7 |  576 | `			}else{` |
|        - |  577 | `				/* Insert a single element */` |
|       15 |  578 | `				PH7_HashmapInsert(pMap,0/* Automatic index assign */,&(*pObj));` |
|        - |  579 | `			}` |
|       27 |  580 | `			SyBlobRelease(&pObj->sBlob);` |
|       13 |  581 | `		}` |
|        - |  582 | `		/* Invalidate any prior representation */` |
|      261 |  583 | `		PH7_MemObjRelease(pObj);` |
|      261 |  584 | `		MemObjSetType(pObj,MEMOBJ_HASHMAP);` |
|      261 |  585 | `		pObj->x.pOther = pMap;` |
|      128 |  586 | `	}` |
|      403 |  587 | `	return SXRET_OK;` |
|      204 |  588 | `}` |
|        - |  589 | `/* Per-entry callback for the array branch of the (object) cast: add one dynamic` |
|        - |  590 | ` * property to the target stdClass, named by the array key (rendered as a string,` |
|        - |  591 | ` * matching PHP) and holding a copy of the value. */` |
|        - |  592 | `struct VmObjCastData { ph7_vm *pVm; ph7_class_instance *pStd; };` |
|       38 |  593 | `static int VmArrayToObjectWalk(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  594 | `{` |
|       39 |  595 | `	struct VmObjCastData *pData = (struct VmObjCastData *)pUserData;` |
|        - |  596 | `	ph7_value *pSlot;` |
|        - |  597 | `	/* pKey and pValue are walk-owned temporaries (PH7_HashmapWalk passes pointers to` |
|        - |  598 | `	 * its own stack-local sKey/sValue, not slots inside pVm->aMemObj), so they survive` |
|        - |  599 | `	 * the slot reservation inside PH7_VmCreateDynamicAttr — no snapshot needed. pKey is` |
|        - |  600 | `	 * safe to coerce in place. */` |
|       39 |  601 | `	PH7_MemObjToString(pKey);` |
|       58 |  602 | `	pSlot = PH7_VmCreateDynamicAttr(pData->pVm,pData->pStd,` |
|       38 |  603 | `		(const char *)SyBlobData(&pKey->sBlob),(sxu32)SyBlobLength(&pKey->sBlob),0);` |
|       39 |  604 | `	if( pSlot ){` |
|       39 |  605 | `		PH7_MemObjStore(pValue,pSlot);` |
|       19 |  606 | `	}` |
|       39 |  607 | `	return SXRET_OK;` |
|        1 |  608 | `}` |
|        - |  609 | `/*` |
|        - |  610 | ` * Convert a ph7_value to type object, invalidating any prior representation.` |
|        - |  611 | ` * The new object is a (PHP-empty) stdClass populated with dynamic properties,` |
|        - |  612 | ` * matching PHP's (object) cast:` |
|        - |  613 | ` *   - array  -> one property per entry (key rendered as a string -> name).` |
|        - |  614 | ` *   - scalar -> a single property named "scalar".` |
|        - |  615 | ` *   - null   -> an empty stdClass (no properties).` |
|        - |  616 | ` *   - object -> returned unchanged (the MEMOBJ_OBJ guard below).` |
|        - |  617 | ` */` |
|       34 |  618 | `PH7_PRIVATE sxi32 PH7_MemObjToObject(ph7_value *pObj)` |
|        1 |  619 | `{` |
|       35 |  620 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  621 | `		ph7_class_instance *pStd;` |
|        - |  622 | `		ph7_class *pClass;` |
|        - |  623 | `		ph7_vm *pVm;` |
|        - |  624 | `		/* Point to the underlying VM + the stdClass */` |
|       35 |  625 | `		pVm = pObj->pVm;` |
|       52 |  626 | `		pClass = pVm->pStdClass ? pVm->pStdClass` |
|       17 |  627 | `			: PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,0,0);` |
|       35 |  628 | `		if( pClass == 0 ){` |
|        - |  629 | `			/* Can't happen,load null instead */` |
|      ! 0 |  630 | `			PH7_MemObjRelease(pObj);` |
|      ! 0 |  631 | `			return SXRET_OK;` |
|        - |  632 | `		}` |
|        - |  633 | `		/* Instanciate a new (empty) stdClass object */` |
|       35 |  634 | `		pStd = PH7_NewClassInstance(pVm,pClass);` |
|       35 |  635 | `		if( pStd == 0 ){` |
|        - |  636 | `			/* Out of memory */` |
|      ! 0 |  637 | `			PH7_MemObjRelease(pObj);` |
|      ! 0 |  638 | `			return SXRET_OK;` |
|        - |  639 | `		}` |
|       35 |  640 | `		pStd->iRef = 1;` |
|       35 |  641 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  642 | `			/* Array: one dynamic property per entry. */` |
|        - |  643 | `			struct VmObjCastData sData;` |
|       23 |  644 | `			sData.pVm = pVm;` |
|       23 |  645 | `			sData.pStd = pStd;` |
|       23 |  646 | `			ph7_array_walk(pObj,VmArrayToObjectWalk,&sData);` |
|       24 |  647 | `		}else if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  648 | `			/* Scalar (int/float/bool/string): a single "scalar" property. */` |
|       11 |  649 | `			ph7_value *pSlot = PH7_VmCreateDynamicAttr(pVm,pStd,"scalar",sizeof("scalar")-1,0);` |
|       11 |  650 | `			if( pSlot ){` |
|       11 |  651 | `				PH7_MemObjStore(pObj,pSlot);` |
|        5 |  652 | `			}` |
|        5 |  653 | `		}` |
|        - |  654 | `		/* (A NULL source yields an empty stdClass — nothing to populate.) */` |
|        - |  655 | `		/* Invalidate any prior representation */` |
|       35 |  656 | `		PH7_MemObjRelease(pObj);` |
|        - |  657 | `		/* Save the new instance */` |
|       35 |  658 | `		pObj->x.pOther = pStd;` |
|       35 |  659 | `		MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       17 |  660 | `	}` |
|       35 |  661 | `	return SXRET_OK;` |
|       18 |  662 | `}` |
|        - |  663 | `/*` |
|        - |  664 | ` * Return a pointer to the appropriate convertion method associated` |
|        - |  665 | ` * with the given type.` |
|        - |  666 | ` * Note on type juggling.` |
|        - |  667 | ` * Accoding to the PHP language reference manual` |
|        - |  668 | ` *  PHP does not require (or support) explicit type definition in variable` |
|        - |  669 | ` *  declaration; a variable's type is determined by the context in which` |
|        - |  670 | ` *  the variable is used. That is to say, if a string value is assigned` |
|        - |  671 | ` *  to variable $var, $var becomes a string. If an integer value is then` |
|        - |  672 | ` *  assigned to $var, it becomes an integer.` |
|        - |  673 | ` */` |
|       78 |  674 | `PH7_PRIVATE ProcMemObjCast PH7_MemObjCastMethod(sxi32 iFlags)` |
|        5 |  675 | `{` |
|       83 |  676 | `	if( iFlags & MEMOBJ_STRING ){` |
|       14 |  677 | `		return PH7_MemObjToString;` |
|       71 |  678 | `	}else if( iFlags & MEMOBJ_INT ){` |
|       55 |  679 | `		return PH7_MemObjToInteger;` |
|       19 |  680 | `	}else if( iFlags & MEMOBJ_REAL ){` |
|       16 |  681 | `		return PH7_MemObjToReal;` |
|        3 |  682 | `	}else if( iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  683 | `		return PH7_MemObjToBool;` |
|        3 |  684 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|        3 |  685 | `		return PH7_MemObjToHashmap;` |
|      ! 0 |  686 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  687 | `		return PH7_MemObjToObject;` |
|      ! 0 |  688 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|        - |  689 | ``		/* `null` is a type, not a weak-coercion target: never silently cast a`` |
|        - |  690 | ``		 * value to null for a standalone `null` type hint. Return/property`` |
|        - |  691 | `		 * enforcement reject a non-null value before reaching here; this guards` |
|        - |  692 | `		 * the parameter default-value path from quietly nulling a non-null` |
|        - |  693 | `		 * default. */` |
|      ! 0 |  694 | `		return 0;` |
|        - |  695 | `	}` |
|        - |  696 | `	/* NULL cast */` |
|      ! 0 |  697 | `	return PH7_MemObjToNull;` |
|       44 |  698 | `}` |
|        - |  699 | `/*` |
|        - |  700 | ` * Return TRUE only if the entire string held by pValue (optionally surrounded` |
|        - |  701 | ` * by whitespace, with an optional sign) is a well-formed PHP numeric string.` |
|        - |  702 | ` * This mirrors PHP's is_numeric_string grammar used for is_numeric() and the` |
|        - |  703 | ` * loose-comparison numeric gate:` |
|        - |  704 | ` *` |
|        - |  705 | ` *   [ws] [sign] ( D+ [.D*] \| .D+ ) [ (e\|E) [sign] D+ ] [ws]   (whole string)` |
|        - |  706 | ` *` |
|        - |  707 | ` * Implemented directly rather than via SyStrIsNumeric — which returns OK on any` |
|        - |  708 | ` * numeric PREFIX (so it wrongly accepts "10abc"/"0x1A"/"0b101") and requires a` |
|        - |  709 | ` * leading digit (so it wrongly rejects ".5"/"-.5", valid in PHP). Unlike a` |
|        - |  710 | ` * strtod-based classifier this needs no NUL-terminated buffer. Returns FALSE for` |
|        - |  711 | ` * a non-string value.` |
|        - |  712 | ` */` |
|   233303 |  713 | `PH7_PRIVATE int PH7_MemObjStringIsNumeric(ph7_value *pValue)` |
|        5 |  714 | `{` |
|        - |  715 | `	const char *z, *zEnd;` |
|        - |  716 | `	sxu32 n;` |
|   233308 |  717 | `	int bDigit = 0;` |
|   233308 |  718 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  719 | `		return 0;` |
|        - |  720 | `	}` |
|   233308 |  721 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|   233308 |  722 | `	n = SyBlobLength(&pValue->sBlob);` |
|   233308 |  723 | `	if( n == 0 ){` |
|       68 |  724 | `		return 0;` |
|        - |  725 | `	}` |
|   233242 |  726 | `	zEnd = z + n;` |
|   233248 |  727 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){` |
|        8 |  728 | `		z++;` |
|        2 |  729 | `	}` |
|   233242 |  730 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|       53 |  731 | `		z++;` |
|       24 |  732 | `	}` |
|   233418 |  733 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|      179 |  734 | `		z++; bDigit = 1;` |
|        3 |  735 | `	}` |
|   233242 |  736 | `	if( z < zEnd && z[0] == '.' ){` |
|       40 |  737 | `		z++;` |
|       76 |  738 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|       38 |  739 | `			z++; bDigit = 1;` |
|        2 |  740 | `		}` |
|       19 |  741 | `	}` |
|        - |  742 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|   233242 |  743 | `	if( !bDigit ){` |
|   233098 |  744 | `		return 0;` |
|        - |  745 | `	}` |
|        - |  746 | `	/* Optional exponent — must carry at least one digit (rejects "1e", "1e+"). */` |
|      147 |  747 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|       14 |  748 | `		z++;` |
|       14 |  749 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|      ! 0 |  750 | `			z++;` |
|      ! 0 |  751 | `		}` |
|       14 |  752 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|        6 |  753 | `			return 0;` |
|        - |  754 | `		}` |
|       22 |  755 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|       14 |  756 | `			z++;` |
|        2 |  757 | `		}` |
|        4 |  758 | `	}` |
|        - |  759 | `	/* Trailing whitespace allowed; anything else means not a numeric string. */` |
|      149 |  760 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){` |
|        8 |  761 | `		z++;` |
|        2 |  762 | `	}` |
|      143 |  763 | `	return z == zEnd ? 1 : 0;` |
|   116672 |  764 | `}` |
|        - |  765 | `/*` |
|        - |  766 | ` * Check whether the ph7_value is numeric [i.e: int/float/bool] or looks` |
|        - |  767 | ` * like a numeric number [i.e: if the ph7_value is of type string.].` |
|        - |  768 | ` * Return TRUE if numeric.FALSE otherwise.` |
|        - |  769 | ` */` |
|   233897 |  770 | `PH7_PRIVATE sxi32 PH7_MemObjIsNumeric(ph7_value *pObj)` |
|        5 |  771 | `{` |
|   233902 |  772 | `	if( pObj->iFlags & ( MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      415 |  773 | `		return TRUE;` |
|   233492 |  774 | `	}else if( pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|      277 |  775 | `		return FALSE;` |
|   233220 |  776 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        - |  777 | `		/* TRUE only if the whole string is a well-formed PHP numeric string. */` |
|   233220 |  778 | `		return PH7_MemObjStringIsNumeric(pObj) ? TRUE : FALSE;` |
|        - |  779 | `	}` |
|        - |  780 | `	/* NOT REACHED */` |
|      ! 0 |  781 | `	return FALSE;` |
|   116969 |  782 | `}` |
|        - |  783 | `/*` |
|        - |  784 | ` * Check whether the ph7_value is empty.Return TRUE if empty.` |
|        - |  785 | ` * FALSE otherwise.` |
|        - |  786 | ` * An ph7_value is considered empty if the following are true:` |
|        - |  787 | ` * NULL value.` |
|        - |  788 | ` * Boolean FALSE.` |
|        - |  789 | ` * Integer/Float with a 0 (zero) value.` |
|        - |  790 | ` * An empty string or a stream of 0 (zero) [i.e: "0","00","000",...].` |
|        - |  791 | ` * An empty array.` |
|        - |  792 | ` * NOTE` |
|        - |  793 | ` *  OBJECT VALUE MUST NOT BE MODIFIED.` |
|        - |  794 | ` */` |
|    33960 |  795 | `PH7_PRIVATE sxi32 PH7_MemObjIsEmpty(ph7_value *pObj)` |
|        5 |  796 | `{` |
|    33965 |  797 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|       19 |  798 | `		return TRUE;` |
|    33949 |  799 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|       22 |  800 | `		return pObj->x.iVal == 0 ? TRUE : FALSE;` |
|    33929 |  801 | `	}else if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  802 | `		return pObj->rVal == (ph7_real)0 ? TRUE : FALSE;` |
|    33929 |  803 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|        5 |  804 | `		return !pObj->x.iVal;` |
|    33925 |  805 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|    27511 |  806 | `		if( SyBlobLength(&pObj->sBlob) <= 0 ){` |
|    22151 |  807 | `			return TRUE;` |
|      ! 0 |  808 | `		}else{` |
|        - |  809 | `			const char *zIn,*zEnd;` |
|     5365 |  810 | `			zIn = (const char *)SyBlobData(&pObj->sBlob);` |
|     5365 |  811 | `			zEnd = &zIn[SyBlobLength(&pObj->sBlob)];` |
|     5371 |  812 | `			while( zIn < zEnd ){` |
|     5371 |  813 | `				if( zIn[0] != '0' ){` |
|     5365 |  814 | `					break;` |
|        - |  815 | `				}` |
|        7 |  816 | `				zIn++;` |
|        1 |  817 | `			}` |
|     5365 |  818 | `			return zIn >= zEnd ? TRUE : FALSE;` |
|      ! 0 |  819 | `		}` |
|     6419 |  820 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|     6419 |  821 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|     6419 |  822 | `		return pMap->nEntry == 0 ? TRUE : FALSE;` |
|      ! 0 |  823 | `	}else if ( pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|      ! 0 |  824 | `		return FALSE;` |
|        - |  825 | `	}` |
|        - |  826 | `	/* Assume empty by default */` |
|      ! 0 |  827 | `	return TRUE;` |
|    16985 |  828 | `}` |
|        - |  829 | `/*` |
|        - |  830 | ` * Convert a ph7_value so that it has types MEMOBJ_REAL or MEMOBJ_INT` |
|        - |  831 | ` * or both.` |
|        - |  832 | ` * Invalidate any prior representations. Every effort is made to force` |
|        - |  833 | ` * the conversion, even if the input is a string that does not look` |
|        - |  834 | ` * completely like a number.Convert as much of the string as we can` |
|        - |  835 | ` * and ignore the rest.` |
|        - |  836 | ` */` |
|   455535 |  837 | `PH7_PRIVATE sxi32 PH7_MemObjToNumeric(ph7_value *pObj)` |
|        5 |  838 | `{` |
|   455540 |  839 | `	if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|   455412 |  840 | `		if( pObj->iFlags & (MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|        3 |  841 | `			if( pObj->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  842 | `				pObj->x.iVal = 0;` |
|      ! 0 |  843 | `			}` |
|        3 |  844 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|        1 |  845 | `		}` |
|        - |  846 | `		/* Already numeric */` |
|   455412 |  847 | `		return  SXRET_OK;` |
|        - |  848 | `	}` |
|      129 |  849 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|      129 |  850 | `		sxi32 rc = SXERR_INVALID;` |
|      129 |  851 | `		sxu8 bReal = FALSE;` |
|        - |  852 | `		SyString sString;` |
|      129 |  853 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|        - |  854 | `		/* Check if the given string looks like a numeric number */` |
|      129 |  855 | `		if( sString.nByte > 0 ){` |
|      129 |  856 | `			rc = SyStrIsNumeric(sString.zString,sString.nByte,&bReal,0);` |
|      129 |  857 | `			if( rc != SXRET_OK && !bReal ){` |
|        - |  858 | `				/* SyStrIsNumeric requires a leading digit, so it mis-classifies` |
|        - |  859 | `				 * a leading-decimal real such as ".5"/"-.5"/".5e2" (returns` |
|        - |  860 | `				 * non-OK with bReal FALSE) — PHP treats these as float. Detect` |
|        - |  861 | `				 * that shape so it coerces to real (strtod parses it) instead of` |
|        - |  862 | `				 * falling through to the int(0) "not a number" branch below. */` |
|        9 |  863 | `				const char *z = sString.zString;` |
|        9 |  864 | `				const char *zEnd = z + sString.nByte;` |
|        9 |  865 | `				while( z < zEnd && SyisSpace(z[0]) ){ z++; }` |
|        9 |  866 | `				if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){ z++; }` |
|        9 |  867 | `				if( z < zEnd && z[0] == '.' && (z + 1) < zEnd && SyisDigit(z[1]) ){` |
|        9 |  868 | `					bReal = TRUE;` |
|        4 |  869 | `				}` |
|        4 |  870 | `			}` |
|       64 |  871 | `		}` |
|      129 |  872 | `		if( bReal ){` |
|       15 |  873 | `			PH7_MemObjToReal(&(*pObj));` |
|        8 |  874 | `		}else{` |
|      115 |  875 | `			if( rc != SXRET_OK ){` |
|        - |  876 | `				/* The input does not look at all like a number,set the value to 0 */` |
|      ! 0 |  877 | `				pObj->x.iVal = 0;` |
|      ! 0 |  878 | `			}else{` |
|        - |  879 | `				/* Convert as much as we can */` |
|      115 |  880 | `				pObj->x.iVal = MemObjStringToInt(&(*pObj));` |
|        - |  881 | `			}` |
|      115 |  882 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|      115 |  883 | `			SyBlobRelease(&pObj->sBlob);` |
|        1 |  884 | `		}` |
|       64 |  885 | `	}else if(pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)){` |
|      ! 0 |  886 | `		PH7_MemObjToInteger(pObj);` |
|      ! 0 |  887 | `	}else{` |
|        - |  888 | `		/* Perform a blind cast */` |
|      ! 0 |  889 | `		PH7_MemObjToReal(&(*pObj));` |
|        - |  890 | `	}` |
|      129 |  891 | `	return SXRET_OK;` |
|   227815 |  892 | `}` |
|        - |  893 | `/*` |
|        - |  894 | ` * Apply Perl-style increment to a string ph7_value in place.` |
|        - |  895 | ` * Walks the bytes right-to-left: digits 0-8 / letters a-y, A-Y bump in` |
|        - |  896 | ` * place; '9' wraps to '0' with carry; 'z' to 'a'; 'Z' to 'A'. A non-` |
|        - |  897 | ` * alphanumeric byte stops the walk without prepending. If carry survives` |
|        - |  898 | ` * past index 0, prepend '1', 'a', or 'A' depending on the class of the` |
|        - |  899 | ` * last carried character. Empty strings become "1".` |
|        - |  900 | ` *` |
|        - |  901 | ` * Caller must ensure pObj is MEMOBJ_STRING and not a numeric string;` |
|        - |  902 | ` * this routine never reclassifies the type, so a result like "e0" stays` |
|        - |  903 | ` * a string even though it looks numeric.` |
|        - |  904 | ` */` |
|       48 |  905 | `PH7_PRIVATE sxi32 PH7_MemObjStringIncrement(ph7_value *pObj)` |
|        1 |  906 | `{` |
|        - |  907 | `	enum CarryClass { CARRY_NONE = 0, CARRY_LOWER, CARRY_UPPER, CARRY_DIGIT };` |
|       49 |  908 | `	enum CarryClass last_class = CARRY_NONE;` |
|        - |  909 | `	sxu32 nLen, pos;` |
|        - |  910 | `	sxu8 *zStr;` |
|       49 |  911 | `	int carry = 1;` |
|        - |  912 | `	int ch;` |
|        - |  913 | `	/* Force ownership: the blob may be SXBLOB_RDONLY (e.g., from` |
|        - |  914 | `	 * PH7_MemObjLoad), in which case BlobPrepareGrow copies on demand` |
|        - |  915 | `	 * and clears the flag.  On an already-owned blob with spare capacity` |
|        - |  916 | `	 * (the common case under PHL's growth allocator), this is a no-op` |
|        - |  917 | `	 * append; on an exact-fit owned blob it triggers a single realloc. */` |
|       49 |  918 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       45 |  919 | `		SyBlobNullAppend(&pObj->sBlob);` |
|       22 |  920 | `	}` |
|       49 |  921 | `	nLen = SyBlobLength(&pObj->sBlob);` |
|       49 |  922 | `	if( nLen == 0 ){` |
|        5 |  923 | `		SyBlobAppend(&pObj->sBlob,"1",sizeof(char));` |
|        5 |  924 | `		return SXRET_OK;` |
|        - |  925 | `	}` |
|       45 |  926 | `	zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|       45 |  927 | `	pos = nLen;` |
|       97 |  928 | `	while( pos > 0 ){` |
|       79 |  929 | `		pos--;` |
|       79 |  930 | `		ch = zStr[pos];` |
|       79 |  931 | `		if( ch >= 'a' && ch <= 'z' ){` |
|       45 |  932 | `			if( ch == 'z' ){` |
|       29 |  933 | `				zStr[pos] = 'a';` |
|       29 |  934 | `				last_class = CARRY_LOWER;` |
|       29 |  935 | `				continue;` |
|        - |  936 | `			}` |
|       17 |  937 | `			zStr[pos]++;` |
|       17 |  938 | `			carry = 0;` |
|       17 |  939 | `			break;` |
|       35 |  940 | `		}else if( ch >= 'A' && ch <= 'Z' ){` |
|       21 |  941 | `			if( ch == 'Z' ){` |
|       19 |  942 | `				zStr[pos] = 'A';` |
|       19 |  943 | `				last_class = CARRY_UPPER;` |
|       19 |  944 | `				continue;` |
|        - |  945 | `			}` |
|        3 |  946 | `			zStr[pos]++;` |
|        3 |  947 | `			carry = 0;` |
|        3 |  948 | `			break;` |
|       15 |  949 | `		}else if( ch >= '0' && ch <= '9' ){` |
|        7 |  950 | `			if( ch == '9' ){` |
|        7 |  951 | `				zStr[pos] = '0';` |
|        7 |  952 | `				last_class = CARRY_DIGIT;` |
|        7 |  953 | `				continue;` |
|        - |  954 | `			}` |
|      ! 0 |  955 | `			zStr[pos]++;` |
|      ! 0 |  956 | `			carry = 0;` |
|      ! 0 |  957 | `			break;` |
|      ! 0 |  958 | `		}else{` |
|        - |  959 | `			/* non-alphanumeric: stop without prepending */` |
|        9 |  960 | `			carry = 0;` |
|        9 |  961 | `			break;` |
|        - |  962 | `		}` |
|      ! 0 |  963 | `	}` |
|       45 |  964 | `	if( carry ){` |
|        - |  965 | `		sxu8 prepend;` |
|        - |  966 | `		sxu32 i;` |
|       19 |  967 | `		switch( last_class ){` |
|        9 |  968 | `			case CARRY_LOWER: prepend = (sxu8)'a'; break;` |
|       11 |  969 | `			case CARRY_UPPER: prepend = (sxu8)'A'; break;` |
|      ! 0 |  970 | `			default:          prepend = (sxu8)'1'; break;` |
|        - |  971 | `		}` |
|        - |  972 | `		/* Append a sentinel byte to grow nByte by 1 (capacity grows too). */` |
|       19 |  973 | `		SyBlobAppend(&pObj->sBlob,"\0",sizeof(char));` |
|       19 |  974 | `		zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|       19 |  975 | `		nLen = SyBlobLength(&pObj->sBlob);` |
|        - |  976 | `		/* Shift right by 1, walking from the end so overlapping is safe. */` |
|       57 |  977 | `		for( i = nLen - 1; i > 0; i-- ){` |
|       39 |  978 | `			zStr[i] = zStr[i - 1];` |
|       20 |  979 | `		}` |
|       19 |  980 | `		zStr[0] = prepend;` |
|        9 |  981 | `	}` |
|       45 |  982 | `	return SXRET_OK;` |
|       25 |  983 | `}` |
|        - |  984 | `/*` |
|        - |  985 | ` * Try a get an integer representation of the given ph7_value.` |
|        - |  986 | ` * If the ph7_value is not of type real,this function is a no-op.` |
|        - |  987 | ` */` |
|     1120 |  988 | `PH7_PRIVATE sxi32 PH7_MemObjTryInteger(ph7_value *pObj)` |
|        1 |  989 | `{` |
|     1121 |  990 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|        - |  991 | `		/* Work only with reals */` |
|     1121 |  992 | `		MemObjTryIntger(&(*pObj));` |
|      560 |  993 | `	}` |
|     1121 |  994 | `	return SXRET_OK;` |
|        1 |  995 | `}` |
|        - |  996 | `/*` |
|        - |  997 | ` * Initialize a ph7_value to the null type.` |
|        - |  998 | ` */` |
| 14413455 |  999 | `PH7_PRIVATE sxi32 PH7_MemObjInit(ph7_vm *pVm,ph7_value *pObj)` |
|        5 | 1000 | `{` |
|        - | 1001 | `	/* Zero the structure */` |
| 14413460 | 1002 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1003 | `	/* Initialize fields */` |
| 14413460 | 1004 | `	pObj->pVm = pVm;` |
| 14413460 | 1005 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1006 | `	/* Set the NULL type */` |
| 14413460 | 1007 | `	pObj->iFlags = MEMOBJ_NULL;` |
| 14413460 | 1008 | `	return SXRET_OK;` |
|        5 | 1009 | `}` |
|        - | 1010 | `/*` |
|        - | 1011 | ` * Initialize a ph7_value to the integer type.` |
|        - | 1012 | ` */` |
|  3429268 | 1013 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromInt(ph7_vm *pVm,ph7_value *pObj,sxi64 iVal)` |
|        5 | 1014 | `{` |
|        - | 1015 | `	/* Zero the structure */` |
|  3429273 | 1016 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1017 | `	/* Initialize fields */` |
|  3429273 | 1018 | `	pObj->pVm = pVm;` |
|  3429273 | 1019 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1020 | `	/* Set the desired type */` |
|  3429273 | 1021 | `	pObj->x.iVal = iVal;` |
|  3429273 | 1022 | `	pObj->iFlags = MEMOBJ_INT;` |
|  3429273 | 1023 | `	return SXRET_OK;` |
|        5 | 1024 | `}` |
|        - | 1025 | `/*` |
|        - | 1026 | ` * Initialize a ph7_value to the boolean type.` |
|        - | 1027 | ` */` |
|    17386 | 1028 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromBool(ph7_vm *pVm,ph7_value *pObj,sxi32 iVal)` |
|        5 | 1029 | `{` |
|        - | 1030 | `	/* Zero the structure */` |
|    17391 | 1031 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1032 | `	/* Initialize fields */` |
|    17391 | 1033 | `	pObj->pVm = pVm;` |
|    17391 | 1034 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1035 | `	/* Set the desired type */` |
|    17391 | 1036 | `	pObj->x.iVal = iVal ? 1 : 0;` |
|    17391 | 1037 | `	pObj->iFlags = MEMOBJ_BOOL;` |
|    17391 | 1038 | `	return SXRET_OK;` |
|        5 | 1039 | `}` |
|        - | 1040 | `/*` |
|        - | 1041 | ` * Initialize a ph7_value to the real type.` |
|        - | 1042 | ` */` |
|       10 | 1043 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromReal(ph7_vm *pVm,ph7_value *pObj,ph7_real rVal)` |
|        1 | 1044 | `{` |
|        - | 1045 | `	/* Zero the structure */` |
|       11 | 1046 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1047 | `	/* Initialize fields */` |
|       11 | 1048 | `	pObj->pVm = pVm;` |
|       11 | 1049 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1050 | `	/* Set the desired type */` |
|       11 | 1051 | `	pObj->rVal = rVal;` |
|       11 | 1052 | `	pObj->iFlags = MEMOBJ_REAL;` |
|       11 | 1053 | `	return SXRET_OK;` |
|        1 | 1054 | `}` |
|        - | 1055 | `/*` |
|        - | 1056 | ` * Initialize a ph7_value to the array type.` |
|        - | 1057 | ` */` |
|    69996 | 1058 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromArray(ph7_vm *pVm,ph7_value *pObj,ph7_hashmap *pArray)` |
|        5 | 1059 | `{` |
|        - | 1060 | `	/* Zero the structure */` |
|    70001 | 1061 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1062 | `	/* Initialize fields */` |
|    70001 | 1063 | `	pObj->pVm = pVm;` |
|    70001 | 1064 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1065 | `	/* Set the desired type */` |
|    70001 | 1066 | `	pObj->iFlags = MEMOBJ_HASHMAP;` |
|    70001 | 1067 | `	pObj->x.pOther = pArray;` |
|    70001 | 1068 | `	return SXRET_OK;` |
|        5 | 1069 | `}` |
|        - | 1070 | `/*` |
|        - | 1071 | ` * Initialize a ph7_value to the string type.` |
|        - | 1072 | ` */` |
|  2124010 | 1073 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromString(ph7_vm *pVm,ph7_value *pObj,const SyString *pVal)` |
|        5 | 1074 | `{` |
|        - | 1075 | `	/* Zero the structure */` |
|  2124015 | 1076 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1077 | `	/* Initialize fields */` |
|  2124015 | 1078 | `	pObj->pVm = pVm;` |
|  2124015 | 1079 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|  2124015 | 1080 | `	if( pVal ){` |
|        - | 1081 | `		/* Append contents */` |
|   907389 | 1082 | `		SyBlobAppend(&pObj->sBlob,(const void *)pVal->zString,pVal->nByte);` |
|   453692 | 1083 | `	}` |
|        - | 1084 | `	/* Set the desired type */` |
|  2124015 | 1085 | `	pObj->iFlags = MEMOBJ_STRING;` |
|  2124015 | 1086 | `	return SXRET_OK;` |
|        5 | 1087 | `}` |
|        - | 1088 | `/*` |
|        - | 1089 | ` * Append some contents to the internal buffer of a given ph7_value.` |
|        - | 1090 | ` * If the given ph7_value is not of type string,this function` |
|        - | 1091 | ` * invalidate any prior representation and set the string type.` |
|        - | 1092 | ` * Then a simple append operation is performed.` |
|        - | 1093 | ` */` |
|  1407580 | 1094 | `PH7_PRIVATE sxi32 PH7_MemObjStringAppend(ph7_value *pObj,const char *zData,sxu32 nLen)` |
|        5 | 1095 | `{` |
|        - | 1096 | `	sxi32 rc;` |
|  1407585 | 1097 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 1098 | `		/* Invalidate any prior representation */` |
|     2055 | 1099 | `		PH7_MemObjRelease(pObj);` |
|     2055 | 1100 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|     1025 | 1101 | `	}` |
|        - | 1102 | `	/* Append contents */` |
|  1407585 | 1103 | `	rc = SyBlobAppend(&pObj->sBlob,zData,nLen);` |
|  1407585 | 1104 | `	return rc;` |
|        5 | 1105 | `}` |
|        - | 1106 | `#if 0` |
|        - | 1107 | `/*` |
|        - | 1108 | ` * Format and append some contents to the internal buffer of a given ph7_value.` |
|        - | 1109 | ` * If the given ph7_value is not of type string,this function invalidate` |
|        - | 1110 | ` * any prior representation and set the string type.` |
|        - | 1111 | ` * Then a simple format and append operation is performed.` |
|        - | 1112 | ` */` |
|        - | 1113 | `PH7_PRIVATE sxi32 PH7_MemObjStringFormat(ph7_value *pObj,const char *zFormat,va_list ap)` |
|        - | 1114 | `{` |
|        - | 1115 | `	sxi32 rc;` |
|        - | 1116 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 1117 | `		/* Invalidate any prior representation */` |
|        - | 1118 | `		PH7_MemObjRelease(pObj);` |
|        - | 1119 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|        - | 1120 | `	}` |
|        - | 1121 | `	/* Format and append contents */` |
|        - | 1122 | `	rc = SyBlobFormatAp(&pObj->sBlob,zFormat,ap);` |
|        - | 1123 | `	return rc;` |
|        - | 1124 | `}` |
|        - | 1125 | `#endif` |
|        - | 1126 | `/*` |
|        - | 1127 | ` * Duplicate the contents of a ph7_value.` |
|        - | 1128 | ` */` |
|  5175227 | 1129 | `PH7_PRIVATE sxi32 PH7_MemObjStore(ph7_value *pSrc,ph7_value *pDest)` |
|        5 | 1130 | `{` |
|  5175232 | 1131 | `	ph7_class_instance *pObj = 0;` |
|  5175232 | 1132 | `	ph7_hashmap *pMap = 0;` |
|        - | 1133 | `	sxi32 rc;` |
|  5175232 | 1134 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1135 | `		/* Increment reference count */` |
|   207353 | 1136 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  5071558 | 1137 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|        - | 1138 | `		/* Increment reference count */` |
|     4817 | 1139 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|     2406 | 1140 | `	}` |
|  5175232 | 1141 | `	if( pDest->iFlags & MEMOBJ_HASHMAP ){` |
|    61011 | 1142 | `		pMap = (ph7_hashmap *)pDest->x.pOther;` |
|  5144729 | 1143 | `	}else if( pDest->iFlags & MEMOBJ_OBJ ){` |
|     5849 | 1144 | `		pObj = (ph7_class_instance *)pDest->x.pOther;` |
|     2922 | 1145 | `	}` |
|  5175232 | 1146 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|  5175232 | 1147 | `	pDest->iFlags &= ~MEMOBJ_AUX;` |
|  5175232 | 1148 | `	rc = SXRET_OK;` |
|  5175232 | 1149 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  4018233 | 1150 | `		SyBlobReset(&pDest->sBlob);` |
|  4018233 | 1151 | `		rc = SyBlobDup(&pSrc->sBlob,&pDest->sBlob);` |
|  2009119 | 1152 | `	}else{` |
|  1157004 | 1153 | `		if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|   285928 | 1154 | `			SyBlobRelease(&pDest->sBlob);` |
|   143004 | 1155 | `		}` |
|        - | 1156 | `	}` |
|  5175232 | 1157 | `	if( pMap ){` |
|    61011 | 1158 | `		PH7_HashmapUnref(pMap);` |
|  5144729 | 1159 | `	}else if( pObj ){` |
|     5849 | 1160 | `		PH7_ClassInstanceUnref(pObj);` |
|     2922 | 1161 | `	}` |
|  5175227 | 1162 | `	if( rc == SXRET_OK && (pDest->iFlags & MEMOBJ_HASHMAP)` |
|  2691330 | 1163 | `	 && pDest->pVm` |
|   207348 | 1164 | `	 && (ph7_hashmap *)pDest->x.pOther == pDest->pVm->pGlobal` |
|        - | 1165 | `	 /* Identity, not nIdx: transient values carry nIdx==0 (SyZero), which` |
|        - | 1166 | `	  * collides with a typical nGlobalIdx of 0 and would skip the snapshot` |
|        - | 1167 | `	  * for closure envs and other non-slot destinations. */` |
|   103683 | 1168 | `	 && pDest != (ph7_value *)SySetAt(&pDest->pVm->aMemObj,pDest->pVm->nGlobalIdx) ){` |
|        - | 1169 | `		/* php 8.1: a COPY of $GLOBALS ($snap = $GLOBALS, $a[] = $GLOBALS,` |
|        - | 1170 | `		 * by-value argument passing, return $GLOBALS, ...) is a by-value` |
|        - | 1171 | `		 * SNAPSHOT of the symbol table with its reference entries` |
|        - | 1172 | `		 * flattened — never a live alias. Materialize it here, the one` |
|        - | 1173 | `		 * store choke point (loads/subscript access keep sharing, so` |
|        - | 1174 | `		 * $GLOBALS[$k] reads and writes stay live). */` |
|        9 | 1175 | `		ph7_hashmap *pSnap = PH7_NewHashmap(pDest->pVm,0,0);` |
|        9 | 1176 | `		if( pSnap && PH7_HashmapDupMaterialized((ph7_hashmap *)pDest->x.pOther,pSnap) == SXRET_OK ){` |
|        9 | 1177 | `			PH7_HashmapUnref((ph7_hashmap *)pDest->x.pOther);` |
|        9 | 1178 | `			pDest->x.pOther = pSnap;` |
|        4 | 1179 | `		}else if( pSnap ){` |
|      ! 0 | 1180 | `			PH7_HashmapUnref(pSnap);` |
|      ! 0 | 1181 | `		}` |
|        4 | 1182 | `	}` |
|  5175232 | 1183 | `	return rc;` |
|        5 | 1184 | `}` |
|        - | 1185 | `/*` |
|        - | 1186 | ` * Duplicate the contents of a ph7_value but do not copy internal` |
|        - | 1187 | ` * buffer contents,simply point to it.` |
|        - | 1188 | ` */` |
|  7071532 | 1189 | `PH7_PRIVATE sxi32 PH7_MemObjLoad(ph7_value *pSrc,ph7_value *pDest)` |
|        5 | 1190 | `{` |
|  7071537 | 1191 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),` |
|        - | 1192 | `		sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|  7071537 | 1193 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1194 | `		/* Increment reference count */` |
|   485237 | 1195 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  6828921 | 1196 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|        - | 1197 | `		/* Increment reference count */` |
|    33071 | 1198 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|    16533 | 1199 | `	}` |
|  7071537 | 1200 | `	if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|       48 | 1201 | `		SyBlobRelease(&pDest->sBlob);` |
|       22 | 1202 | `	}` |
|  7071537 | 1203 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  3719537 | 1204 | `		SyBlobReadOnly(&pDest->sBlob,SyBlobData(&pSrc->sBlob),SyBlobLength(&pSrc->sBlob));` |
|  1859882 | 1205 | `	}` |
|  7071537 | 1206 | `	return SXRET_OK;` |
|        5 | 1207 | `}` |
|        - | 1208 | `/*` |
|        - | 1209 | ` * Invalidate any prior representation of a given ph7_value.` |
|        - | 1210 | ` */` |
| 17236357 | 1211 | `PH7_PRIVATE sxi32 PH7_MemObjRelease(ph7_value *pObj)` |
|        5 | 1212 | `{` |
| 17236362 | 1213 | `	if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
| 14855765 | 1214 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|   666003 | 1215 | `			PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
| 14522766 | 1216 | `		}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|    55723 | 1217 | `			PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|    27859 | 1218 | `		}` |
|        - | 1219 | `		/* Release the internal buffer */` |
| 14855765 | 1220 | `		SyBlobRelease(&pObj->sBlob);` |
|        - | 1221 | `		/* Invalidate any prior representation */` |
| 14855765 | 1222 | `		pObj->iFlags = MEMOBJ_NULL;` |
|  7428251 | 1223 | `	}` |
| 17236362 | 1224 | `	return SXRET_OK;` |
|        5 | 1225 | `}` |
|        - | 1226 | `/*` |
|        - | 1227 | ` * Compare two ph7_values.` |
|        - | 1228 | ` * Return 0 if the values are equals, > 0 if pObj1 is greater than pObj2` |
|        - | 1229 | ` * or < 0 if pObj2 is greater than pObj1.` |
|        - | 1230 | ` * Type comparison table taken from the PHP language reference manual.` |
|        - | 1231 | ` * Comparisons of $x with PHP functions Expression` |
|        - | 1232 | ` *              gettype() 	empty() 	is_null() 	isset() 	boolean : if($x)` |
|        - | 1233 | ` * $x = ""; 	string 	    TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1234 | ` * $x = null 	NULL 	    TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1235 | ` * var $x; 	    NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1236 | ` * $x is undefined 	NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1237 | ` *  $x = array(); 	array 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1238 | ` * $x = false; 	boolean 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1239 | ` * $x = true; 	boolean 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1240 | ` * $x = 1; 	    integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1241 | ` * $x = 42; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1242 | ` * $x = 0; 	    integer 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1243 | ` * $x = -1; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1244 | ` * $x = "1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1245 | ` * $x = "0"; 	string 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1246 | ` * $x = "-1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1247 | ` * $x = "php"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1248 | ` * $x = "true"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1249 | ` * $x = "false"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1250 | ` *      Loose comparisons with ==` |
|        - | 1251 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|        - | 1252 | ` * TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1253 | ` * FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|        - | 1254 | ` * 1 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1255 | ` * 0 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE` |
|        - | 1256 | ` * -1 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1257 | ` * "1" 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1258 | ` * "0" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1259 | ` * "-1" 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1260 | ` * NULL 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|        - | 1261 | ` * array() 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1262 | ` * "php" 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1263 | ` * "" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE` |
|        - | 1264 | ` *    Strict comparisons with ===` |
|        - | 1265 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|        - | 1266 | ` * TRUE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1267 | ` * FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1268 | ` * 1 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1269 | ` * 0 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1270 | ` * -1 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1271 | ` * "1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1272 | ` * "0" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1273 | ` * "-1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1274 | ` * NULL 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE` |
|        - | 1275 | ` * array() 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE` |
|        - | 1276 | ` * "php" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1277 | ` * "" 	    FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE` |
|        - | 1278 | ` */` |
|  1299888 | 1279 | `PH7_PRIVATE sxi32 PH7_MemObjCmp(ph7_value *pObj1,ph7_value *pObj2,int bStrict,int iNest)` |
|        5 | 1280 | `{` |
|        - | 1281 | `	sxi32 iComb;` |
|        - | 1282 | `	sxi32 rc;` |
|  1299893 | 1283 | `	if( bStrict ){` |
|        - | 1284 | `		sxi32 iF1,iF2;` |
|        - | 1285 | `		/* Strict comparisons with === */` |
|   680275 | 1286 | `		iF1 = pObj1->iFlags&~MEMOBJ_AUX;` |
|   680275 | 1287 | `		iF2 = pObj2->iFlags&~MEMOBJ_AUX;` |
|   680275 | 1288 | `		if( iF1 != iF2 ){` |
|        - | 1289 | `			/* Not of the same type */` |
|   191115 | 1290 | `			return 1;` |
|        - | 1291 | `		}` |
|   244580 | 1292 | `	}` |
|        - | 1293 | `	/* Combine flag together */` |
|  1108783 | 1294 | `	iComb = pObj1->iFlags\|pObj2->iFlags;` |
|  1108778 | 1295 | `	if( !bStrict` |
|   864198 | 1296 | `	 && (iComb & MEMOBJ_NULL) != 0` |
|   309905 | 1297 | `	 && (iComb & MEMOBJ_STRING) != 0` |
|       65 | 1298 | `	 && (iComb & (MEMOBJ_BOOL\|MEMOBJ_RES\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|        - | 1299 | `		/*` |
|        - | 1300 | `		 * PHP 8 comparison table: null loosely compared with a STRING is` |
|        - | 1301 | `		 * compared as the empty string (a string comparison), not through` |
|        - | 1302 | `		 * bool coercion — so null == "0" is FALSE and null < "0" is TRUE` |
|        - | 1303 | `		 * (php 7 and the historical PH7 behavior coerced both to bool,` |
|        - | 1304 | `		 * making any non-empty non-"0"-insensitive string "equal" to null).` |
|        - | 1305 | `		 * Convert the null side to "" and let the string branch below run.` |
|        - | 1306 | `		 */` |
|       45 | 1307 | `		if( pObj1->iFlags & MEMOBJ_NULL ){` |
|       35 | 1308 | `			PH7_MemObjToString(pObj1);` |
|       18 | 1309 | `		}else{` |
|       11 | 1310 | `			PH7_MemObjToString(pObj2);` |
|        - | 1311 | `		}` |
|       45 | 1312 | `		iComb = pObj1->iFlags\|pObj2->iFlags;` |
|       22 | 1313 | `	}` |
|  1108783 | 1314 | `	if( iComb & (MEMOBJ_NULL\|MEMOBJ_RES\|MEMOBJ_BOOL) ){` |
|        - | 1315 | `		/* Convert to boolean: Keep in mind FALSE < TRUE */` |
|    22735 | 1316 | `		if( (pObj1->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     8327 | 1317 | `			PH7_MemObjToBool(pObj1);` |
|     4161 | 1318 | `		}` |
|    22735 | 1319 | `		if( (pObj2->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     7347 | 1320 | `			PH7_MemObjToBool(pObj2);` |
|     3671 | 1321 | `		}` |
|    22735 | 1322 | `		return (sxi32)((pObj1->x.iVal != 0) - (pObj2->x.iVal != 0));` |
|  1086053 | 1323 | `	}else if ( iComb & MEMOBJ_HASHMAP ){` |
|        - | 1324 | `		/* Hashmap aka 'array' comparison */` |
|       31 | 1325 | `		if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1326 | `			/* Array is always greater */` |
|      ! 0 | 1327 | `			return -1;` |
|        - | 1328 | `		}` |
|       31 | 1329 | `		if( (pObj2->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1330 | `			/* Array is always greater */` |
|      ! 0 | 1331 | `			return 1;` |
|        - | 1332 | `		}` |
|        - | 1333 | `		/* Perform the comparison */` |
|       31 | 1334 | `		rc = PH7_HashmapCmp((ph7_hashmap *)pObj1->x.pOther,(ph7_hashmap *)pObj2->x.pOther,bStrict);` |
|       31 | 1335 | `		return rc;` |
|  1086023 | 1336 | `	}else if(iComb & MEMOBJ_OBJ ){` |
|        - | 1337 | `		/* Object comparison */` |
|      235 | 1338 | `		if( (pObj1->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 1339 | `			/* Object is always greater */` |
|      ! 0 | 1340 | `			return -1;` |
|        - | 1341 | `		}` |
|      235 | 1342 | `		if( (pObj2->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 1343 | `			/* Object is always greater */` |
|      ! 0 | 1344 | `			return 1;` |
|        - | 1345 | `		}` |
|        - | 1346 | `		/* Perform the comparison */` |
|      235 | 1347 | `		rc = PH7_ClassInstanceCmp((ph7_class_instance *)pObj1->x.pOther,(ph7_class_instance *)pObj2->x.pOther,bStrict,iNest);` |
|      235 | 1348 | `		return rc;` |
|  1085793 | 1349 | `	}else if ( iComb & MEMOBJ_STRING ){` |
|        - | 1350 | `		SyString s1,s2;` |
|   698312 | 1351 | `		if( !bStrict ){` |
|        - | 1352 | `			/*` |
|        - | 1353 | `			 * PHP 8 "saner string to number comparisons" (RFC): a numeric` |
|        - | 1354 | `			 * comparison is performed only when BOTH operands are numbers or` |
|        - | 1355 | `			 * numeric strings. A number compared with a NON-numeric string is` |
|        - | 1356 | `			 * compared as strings, with the number cast to its string form —` |
|        - | 1357 | `			 * so 0 == "abc" is false, "abc" < 10 is false, and max("abc",10)` |
|        - | 1358 | `			 * is "abc". (PHP 7 cast the non-numeric string to 0 and compared` |
|        - | 1359 | `			 * numerically; comparing when EITHER side was numeric is what this` |
|        - | 1360 | `			 * replaces.) Two non-numeric strings, or one numeric and one` |
|        - | 1361 | `			 * non-numeric string, still fall through to the string comparison` |
|        - | 1362 | `			 * below, unchanged.` |
|        - | 1363 | `			 */` |
|   232902 | 1364 | `			if( PH7_MemObjIsNumeric(pObj1) && PH7_MemObjIsNumeric(pObj2) ){` |
|        - | 1365 | `				/* Perform a numeric comparison */` |
|       29 | 1366 | `				goto Numeric;` |
|        - | 1367 | `			}` |
|   116450 | 1368 | `		}` |
|        - | 1369 | `		/* Perform a strict string comparison.*/` |
|   698284 | 1370 | `		if( (pObj1->iFlags&MEMOBJ_STRING) == 0 ){` |
|       19 | 1371 | `			PH7_MemObjToString(pObj1);` |
|        9 | 1372 | `		}` |
|   698284 | 1373 | `		if( (pObj2->iFlags&MEMOBJ_STRING) == 0 ){` |
|        5 | 1374 | `			PH7_MemObjToString(pObj2);` |
|        2 | 1375 | `		}` |
|   698284 | 1376 | `		SyStringInitFromBuf(&s1,SyBlobData(&pObj1->sBlob),SyBlobLength(&pObj1->sBlob));` |
|   698284 | 1377 | `		SyStringInitFromBuf(&s2,SyBlobData(&pObj2->sBlob),SyBlobLength(&pObj2->sBlob));` |
|        - | 1378 | `		/*` |
|        - | 1379 | `		 * Strings are compared using memcmp(). If one value is an exact prefix of the` |
|        - | 1380 | `		 * other, then the shorter value is less than the longer value.` |
|        - | 1381 | `		 */` |
|   698284 | 1382 | `		rc = SyMemcmp((const void *)s1.zString,(const void *)s2.zString,SXMIN(s1.nByte,s2.nByte));` |
|   698284 | 1383 | `		if( rc == 0 ){` |
|   233336 | 1384 | `			if( s1.nByte != s2.nByte ){` |
|     2170 | 1385 | `				rc = s1.nByte < s2.nByte ? -1 : 1;` |
|     1083 | 1386 | `			}` |
|   116666 | 1387 | `		}` |
|   698284 | 1388 | `		return rc;` |
|   387486 | 1389 | `	}else if( iComb & (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|   193698 | 1390 | `Numeric:` |
|        - | 1391 | `		/* Perform a numeric comparison if one of the operand is numeric(integer or real) */` |
|   387514 | 1392 | `		if( (pObj1->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|       11 | 1393 | `			PH7_MemObjToNumeric(pObj1);` |
|        5 | 1394 | `		}` |
|   387514 | 1395 | `		if( (pObj2->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|       19 | 1396 | `			PH7_MemObjToNumeric(pObj2);` |
|        9 | 1397 | `		}` |
|   387514 | 1398 | `		if( (pObj1->iFlags & pObj2->iFlags & MEMOBJ_INT) == 0) {` |
|        - | 1399 | `			/*` |
|        - | 1400 | `			 * Symisc eXtension to the PHP language:` |
|        - | 1401 | `			 *  Floating point comparison is introduced and works as expected.` |
|        - | 1402 | `			 */` |
|        - | 1403 | `			ph7_real r1,r2;` |
|        - | 1404 | `			/* Compare as reals */` |
|      261 | 1405 | `			if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|       11 | 1406 | `				PH7_MemObjToReal(pObj1);` |
|        5 | 1407 | `			}` |
|      261 | 1408 | `			r1 = pObj1->rVal;` |
|      261 | 1409 | `			if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|       51 | 1410 | `				PH7_MemObjToReal(pObj2);` |
|       25 | 1411 | `			}` |
|      261 | 1412 | `			r2 = pObj2->rVal;` |
|      261 | 1413 | `			if( PH7_IS_NAN(r1) \|\| PH7_IS_NAN(r2) ){` |
|        - | 1414 | `				/*` |
|        - | 1415 | `				 * Keep a strict three-way comparator contract even for NaN values.` |
|        - | 1416 | `				 * For ordering purposes, NaN compares equal to NaN and greater than` |
|        - | 1417 | `				 * any non-NaN numeric value.` |
|        - | 1418 | `				 */` |
|       45 | 1419 | `				if( PH7_IS_NAN(r1) ){` |
|       35 | 1420 | `					return PH7_IS_NAN(r2) ? 0 : 1;` |
|        - | 1421 | `				}` |
|       11 | 1422 | `				return -1;` |
|        - | 1423 | `			}` |
|      217 | 1424 | `			if( r1 > r2 ){` |
|       45 | 1425 | `				return 1;` |
|      173 | 1426 | `			}else if( r1 < r2 ){` |
|      125 | 1427 | `				return -1;` |
|        - | 1428 | `			}` |
|       49 | 1429 | `			return 0;` |
|      ! 0 | 1430 | `		}else{` |
|        - | 1431 | `			/* Integer comparison */` |
|   387254 | 1432 | `			if( pObj1->x.iVal > pObj2->x.iVal ){` |
|     5894 | 1433 | `				return 1;` |
|   381365 | 1434 | `			}else if( pObj1->x.iVal < pObj2->x.iVal ){` |
|   373877 | 1435 | `				return -1;` |
|        - | 1436 | `			}` |
|     7493 | 1437 | `			return 0;` |
|        - | 1438 | `		}` |
|        - | 1439 | `	}` |
|        - | 1440 | `	/* NOT REACHED */` |
|      ! 0 | 1441 | `	return 0;` |
|   650007 | 1442 | `}` |
|        - | 1443 | `/*` |
|        - | 1444 | ` * Perform an addition operation of two ph7_values.` |
|        - | 1445 | ` * The reason this function is implemented here rather than 'vm.c'` |
|        - | 1446 | ` * is that the '+' operator is overloaded.` |
|        - | 1447 | ` * That is,the '+' operator is used for arithmetic operation and also` |
|        - | 1448 | ` * used for operation on arrays [i.e: union]. When used with an array` |
|        - | 1449 | ` * The + operator returns the right-hand array appended to the left-hand array.` |
|        - | 1450 | ` * For keys that exist in both arrays, the elements from the left-hand array` |
|        - | 1451 | ` * will be used, and the matching elements from the right-hand array will` |
|        - | 1452 | ` * be ignored.` |
|        - | 1453 | ` * This function take care of handling all the scenarios.` |
|        - | 1454 | ` */` |
|    10030 | 1455 | `PH7_PRIVATE sxi32 PH7_MemObjAdd(ph7_value *pObj1,ph7_value *pObj2,int bAddStore)` |
|        5 | 1456 | `{` |
|    10035 | 1457 | `	if( ((pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1458 | `			/* Arithemtic operation */` |
|     6205 | 1459 | `			PH7_MemObjToNumeric(pObj1);` |
|     6205 | 1460 | `			PH7_MemObjToNumeric(pObj2);` |
|     6205 | 1461 | `			if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_REAL ){` |
|        - | 1462 | `				/* Floating point arithmetic */` |
|        - | 1463 | `				ph7_real a,b;` |
|       67 | 1464 | `				if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|       31 | 1465 | `					PH7_MemObjToReal(pObj1);` |
|       15 | 1466 | `				}` |
|       67 | 1467 | `				if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 | 1468 | `					PH7_MemObjToReal(pObj2);` |
|        2 | 1469 | `				}` |
|       67 | 1470 | `				a = pObj1->rVal;` |
|       67 | 1471 | `				b = pObj2->rVal;` |
|       67 | 1472 | `				pObj1->rVal = a+b;` |
|       67 | 1473 | `				MemObjSetType(pObj1,MEMOBJ_REAL);` |
|        - | 1474 | `				/* Try to get an integer representation also */` |
|       67 | 1475 | `				MemObjTryIntger(&(*pObj1));` |
|       34 | 1476 | `			}else{` |
|        - | 1477 | `				/* Integer arithmetic */` |
|        - | 1478 | `				sxi64 a,b;` |
|     6139 | 1479 | `				a = pObj1->x.iVal;` |
|     6139 | 1480 | `				b = pObj2->x.iVal;` |
|     6139 | 1481 | `				pObj1->x.iVal = a+b;` |
|     6139 | 1482 | `				MemObjSetType(pObj1,MEMOBJ_INT);` |
|        - | 1483 | `			}` |
|     3105 | 1484 | `	}else{` |
|     3835 | 1485 | `		if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP ){` |
|        - | 1486 | `			ph7_hashmap *pMap;` |
|        - | 1487 | `			sxi32 rc;` |
|     3835 | 1488 | `			if( bAddStore ){` |
|        - | 1489 | `				/* Do not duplicate the hashmap,use the left one since its an add&store operation.` |
|        - | 1490 | `				 */` |
|        3 | 1491 | `				if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1492 | `					/* Force a hashmap cast */` |
|      ! 0 | 1493 | `					rc = PH7_MemObjToHashmap(pObj1);` |
|      ! 0 | 1494 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 1495 | `						PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|      ! 0 | 1496 | `						return rc;` |
|        - | 1497 | `					}` |
|      ! 0 | 1498 | `				}` |
|        - | 1499 | `				/* COW separate before in-place mutation */` |
|        3 | 1500 | `				pMap = PH7_HashmapCowSeparate(pObj1->pVm,pObj1);` |
|        2 | 1501 | `			}else{` |
|        - | 1502 | `				/* Create a new hashmap */` |
|     3833 | 1503 | `				pMap = PH7_NewHashmap(pObj1->pVm,0,0);` |
|     3833 | 1504 | `				if( pMap == 0){` |
|      ! 0 | 1505 | `					PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|      ! 0 | 1506 | `					return SXERR_MEM;` |
|        - | 1507 | `				}` |
|        - | 1508 | `			}` |
|     3835 | 1509 | `			if( !bAddStore ){` |
|     3833 | 1510 | `				if(pObj1->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1511 | `					/* Perform a hashmap duplication */` |
|     3833 | 1512 | `					PH7_HashmapDup((ph7_hashmap *)pObj1->x.pOther,pMap);` |
|     1919 | 1513 | `				}else{` |
|      ! 0 | 1514 | `					if((pObj1->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 1515 | `						/* Simple insertion */` |
|      ! 0 | 1516 | `						PH7_HashmapInsert(pMap,0,pObj1);` |
|      ! 0 | 1517 | `					}` |
|        - | 1518 | `				}` |
|     1914 | 1519 | `			}` |
|        - | 1520 | `			/* Perform the union */` |
|     3835 | 1521 | `			if(pObj2->iFlags & MEMOBJ_HASHMAP ){` |
|     3835 | 1522 | `				PH7_HashmapUnion(pMap,(ph7_hashmap *)pObj2->x.pOther);` |
|     1920 | 1523 | `			}else{` |
|      ! 0 | 1524 | `				if((pObj2->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 1525 | `					/* Simple insertion */` |
|      ! 0 | 1526 | `					PH7_HashmapInsert(pMap,0,pObj2);` |
|      ! 0 | 1527 | `				}` |
|        - | 1528 | `			}` |
|        - | 1529 | `			/* Reflect the change */` |
|     3835 | 1530 | `			if( pObj1->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 1531 | `				SyBlobRelease(&pObj1->sBlob);` |
|      ! 0 | 1532 | `			}` |
|     3835 | 1533 | `			pObj1->x.pOther = pMap;` |
|     3835 | 1534 | `			MemObjSetType(pObj1,MEMOBJ_HASHMAP);` |
|     1915 | 1535 | `		}` |
|        - | 1536 | `	}` |
|    10035 | 1537 | `	return SXRET_OK;` |
|     5020 | 1538 | `}` |
|        - | 1539 | `/*` |
|        - | 1540 | ` * Return a printable representation of the type of a given` |
|        - | 1541 | ` * ph7_value.` |
|        - | 1542 | ` */` |
|      464 | 1543 | `PH7_PRIVATE const char * PH7_MemObjTypeDump(ph7_value *pVal)` |
|        4 | 1544 | `{` |
|      468 | 1545 | `	const char *zType = "";` |
|      468 | 1546 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|        5 | 1547 | `		zType = "null";` |
|      466 | 1548 | `	}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|        - | 1549 | `		/* REAL is authoritative over a cached MEMOBJ_INT: an integer-valued` |
|        - | 1550 | `		 * real (e.g. 1.0) is reported as "double", matching PHP's gettype(). */` |
|        7 | 1551 | `		zType = "double";` |
|      461 | 1552 | `	}else if( pVal->iFlags & MEMOBJ_INT ){` |
|      127 | 1553 | `		zType = "int";` |
|      396 | 1554 | `	}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|       76 | 1555 | `		zType = "string";` |
|      298 | 1556 | `	}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|      109 | 1557 | `		zType = "bool";` |
|      208 | 1558 | `	}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|       16 | 1559 | `		zType = "array";` |
|      148 | 1560 | `	}else if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      141 | 1561 | `		zType = "object";` |
|       69 | 1562 | `	}else if( pVal->iFlags & MEMOBJ_RES ){` |
|      ! 0 | 1563 | `		zType = "resource";` |
|      ! 0 | 1564 | `	}` |
|      468 | 1565 | `	return zType;` |
|        4 | 1566 | `}` |
|        - | 1567 | `/*` |
|        - | 1568 | ` * Dump a ph7_value [i.e: get a printable representation of it's type and contents.].` |
|        - | 1569 | ` * Store the dump in the given blob.` |
|        - | 1570 | ` */` |
|      478 | 1571 | `PH7_PRIVATE sxi32 PH7_MemObjDump(` |
|        - | 1572 | `	SyBlob *pOut,      /* Store the dump here */` |
|        - | 1573 | `	ph7_value *pObj,   /* Dump this */` |
|        - | 1574 | `	int ShowType,      /* TRUE to output value type */` |
|        - | 1575 | `	int nTab,          /* # of Whitespace to insert */` |
|        - | 1576 | `	int nDepth,        /* Nesting level */` |
|        - | 1577 | `	int isRef          /* TRUE if referenced object */` |
|        - | 1578 | `	)` |
|        4 | 1579 | `{` |
|      482 | 1580 | `	sxi32 rc = SXRET_OK;` |
|        - | 1581 | `	const char *zType;` |
|        - | 1582 | `	int i;` |
|     4598 | 1583 | `	for( i = 0 ; i < nTab ; i++ ){` |
|     4119 | 1584 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     2061 | 1585 | `	}` |
|      482 | 1586 | `	if( ShowType ){` |
|      434 | 1587 | `		if( isRef ){` |
|      ! 0 | 1588 | `			SyBlobAppend(&(*pOut),"&",sizeof(char));` |
|      ! 0 | 1589 | `		}` |
|        - | 1590 | `		/* Get value type first. var_dump() labels reals "float" (PHP), whereas` |
|        - | 1591 | `		 * gettype()/PH7_MemObjTypeDump use the legacy "double" spelling. */` |
|      434 | 1592 | `		if( (pObj->iFlags & MEMOBJ_REAL) && (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        5 | 1593 | `			zType = "float";` |
|        3 | 1594 | `		}else{` |
|      430 | 1595 | `			zType = PH7_MemObjTypeDump(pObj);` |
|        - | 1596 | `		}` |
|      434 | 1597 | `		SyBlobAppend(&(*pOut),zType,SyStrlen(zType));` |
|      215 | 1598 | `	}` |
|      482 | 1599 | `	if((pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|      480 | 1600 | `		if ( ShowType ){` |
|      432 | 1601 | `			SyBlobAppend(&(*pOut),"(",sizeof(char));` |
|      214 | 1602 | `		}` |
|      480 | 1603 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1604 | `			/* Dump hashmap entries */` |
|       24 | 1605 | `			rc = PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,ShowType,nTab+1,nDepth+1);` |
|      469 | 1606 | `		}else if(pObj->iFlags & MEMOBJ_OBJ ){` |
|        - | 1607 | `			/* Dump class instance attributes */` |
|      141 | 1608 | `			rc = PH7_ClassInstanceDump(&(*pOut),(ph7_class_instance *)pObj->x.pOther,ShowType,nTab+1,nDepth+1);` |
|       72 | 1609 | `		}else{` |
|      320 | 1610 | `			SyBlob *pContents = &pObj->sBlob;` |
|        - | 1611 | `			/* Get a printable representation of the contents */` |
|      320 | 1612 | `			if((pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|      252 | 1613 | `				MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|      128 | 1614 | `			}else{` |
|        - | 1615 | `				/* PHP format: string(N) "content" */` |
|       71 | 1616 | `				if( ShowType ){` |
|       53 | 1617 | `					SyBlobFormat(&(*pOut),"%u) \"",SyBlobLength(&pObj->sBlob));` |
|       25 | 1618 | `				}` |
|       71 | 1619 | `				if( SyBlobLength(pContents) > 0 ){` |
|       69 | 1620 | `					SyBlobAppend(&(*pOut),SyBlobData(pContents),SyBlobLength(pContents));` |
|       33 | 1621 | `				}` |
|       71 | 1622 | `				if( ShowType ){` |
|       53 | 1623 | `					SyBlobAppend(&(*pOut),"\"",sizeof(char));` |
|       25 | 1624 | `				}` |
|        - | 1625 | `			}` |
|        - | 1626 | `		}` |
|      480 | 1627 | `		if( ShowType ){` |
|        - | 1628 | `			/* Strings already emitted their own ')' as part of the` |
|        - | 1629 | `			 * "N) \"content\"" format above. */` |
|      432 | 1630 | `			if( (pObj->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_STRING)) == 0 ){` |
|      236 | 1631 | `				SyBlobAppend(&(*pOut),")",sizeof(char));` |
|      116 | 1632 | `			}` |
|      214 | 1633 | `		}` |
|      238 | 1634 | `	}` |
|        - | 1635 | `#ifdef __WINNT__` |
|        4 | 1636 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|        - | 1637 | `#else` |
|      478 | 1638 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        - | 1639 | `#endif` |
|      482 | 1640 | `	return rc;` |
|        4 | 1641 | `}` |
|        - | 1642 |  |
