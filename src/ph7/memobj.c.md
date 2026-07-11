# src/ph7/memobj.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 770/856 lines (89.95%)

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
|      354 |   13 | `PH7_PRIVATE const char *ph7_type_name(ph7_value *pVal)` |
|        5 |   14 | `{` |
|      359 |   15 | `	if( ph7_value_is_null(pVal) ) return "null";` |
|      333 |   16 | `	if( ph7_value_is_bool(pVal) ) return "bool";` |
|      325 |   17 | `	if( ph7_value_is_int(pVal) ) return "int";` |
|      275 |   18 | `	if( ph7_value_is_float(pVal) ) return "float";` |
|      271 |   19 | `	if( ph7_value_is_string(pVal) ) return "string";` |
|      109 |   20 | `	if( ph7_value_is_array(pVal) ) return "array";` |
|       30 |   21 | `	if( ph7_value_is_object(pVal) ) return "object";` |
|        3 |   22 | `	if( ph7_value_is_resource(pVal) ) return "resource";` |
|      ! 0 |   23 | `	return "unknown";` |
|      182 |   24 | `}` |
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
|     2864 |   42 | `static sxi64 MemObjRealToInt(ph7_value *pObj)` |
|        4 |   43 | `{` |
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
|     2868 |   59 | `  ph7_real r = pObj->rVal;` |
|     2868 |   60 | `  if( r<(ph7_real)minInt ){` |
|        3 |   61 | `    return minInt;` |
|     2866 |   62 | `  }else if( r>(ph7_real)maxInt ){` |
|        - |   63 | `    /* minInt is correct here - not maxInt.  It turns out that assigning` |
|        - |   64 | `    ** a very large positive number to an integer results in a very large` |
|        - |   65 | `    ** negative integer.  This makes no sense, but it is what x86 hardware` |
|        - |   66 | `    ** does so for compatibility we will do the same in software. */` |
|      135 |   67 | `    return minInt;` |
|      ! 0 |   68 | `  }else{` |
|     2732 |   69 | `    return (sxi64)r;` |
|        - |   70 | `  }` |
|        - |   71 | `#endif` |
|     1436 |   72 | `}` |
|        - |   73 | `/*` |
|        - |   74 | ` * Convert a raw token value typically a stream of digit [i.e: hex,octal,binary or decimal]` |
|        - |   75 | ` * to a 64-bit integer.` |
|        - |   76 | ` */` |
|   134832 |   77 | `PH7_PRIVATE sxi64 PH7_TokenValueToInt64(SyString *pVal)` |
|        5 |   78 | `{` |
|   134837 |   79 | `	sxi64 iVal = 0;` |
|   134837 |   80 | `	if( pVal->nByte <= 0 ){` |
|        7 |   81 | `		return 0;` |
|        - |   82 | `	}` |
|   134831 |   83 | `	if( pVal->zString[0] == '0' ){` |
|        - |   84 | `		sxi32 c;` |
|    55801 |   85 | `		if( pVal->nByte == sizeof(char) ){` |
|    55397 |   86 | `			return 0;` |
|        - |   87 | `		}` |
|      405 |   88 | `		c = pVal->zString[1];` |
|      405 |   89 | `		if( c  == 'x' \|\| c == 'X' ){` |
|        - |   90 | `			/* Hex digit stream */` |
|       69 |   91 | `			SyHexStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|      371 |   92 | `		}else if( c == 'b' \|\| c == 'B' ){` |
|        - |   93 | `			/* Binary digit stream */` |
|      277 |   94 | `			SyBinaryStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|      139 |   95 | `		}else{` |
|        - |   96 | `			/* Octal digit stream */` |
|       61 |   97 | `			SyOctalStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|        - |   98 | `		}` |
|      203 |   99 | `	}else{` |
|        - |  100 | `		/* Decimal digit stream */` |
|    79035 |  101 | `		SyStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|        - |  102 | `	}` |
|    79439 |  103 | `	return iVal;` |
|    67421 |  104 | `}` |
|        - |  105 | `/*` |
|        - |  106 | ` * Return some kind of 64-bit integer value which is the best we can` |
|        - |  107 | ` * do at representing the value that pObj describes as a string` |
|        - |  108 | ` * representation.` |
|        - |  109 | ` */` |
|      424 |  110 | `static sxi64 MemObjStringToInt(ph7_value *pObj)` |
|        5 |  111 | `{` |
|        - |  112 | `	SyString sVal;` |
|      429 |  113 | `	SyStringInitFromBuf(&sVal,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|      429 |  114 | `	return PH7_TokenValueToInt64(&sVal);` |
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
|      492 |  150 | `static sxi64 MemObjIntValue(ph7_value *pObj)` |
|        5 |  151 | `{` |
|        - |  152 | `	sxi32 iFlags;` |
|      497 |  153 | `	iFlags = pObj->iFlags;` |
|      497 |  154 | `	if (iFlags & MEMOBJ_REAL ){` |
|       33 |  155 | `		return MemObjRealToInt(&(*pObj));` |
|      465 |  156 | `	}else if( iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      122 |  157 | `		return pObj->x.iVal;` |
|      345 |  158 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|      325 |  159 | `		return MemObjStringToInt(&(*pObj));` |
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
|      251 |  188 | `}` |
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
|     1674 |  199 | `static ph7_real MemObjRealValue(ph7_value *pObj)` |
|        4 |  200 | `{` |
|        - |  201 | `	sxi32 iFlags;` |
|     1678 |  202 | `	iFlags = pObj->iFlags;` |
|     1678 |  203 | `	if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  204 | `		return pObj->rVal;` |
|     1678 |  205 | `	}else if (iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      738 |  206 | `		return (ph7_real)pObj->x.iVal;` |
|      942 |  207 | `	}else if (iFlags & MEMOBJ_STRING){` |
|        - |  208 | `		SyString sString;` |
|        - |  209 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  210 | `		ph7_real rVal = 0;` |
|        - |  211 | `#else` |
|      936 |  212 | `		ph7_real rVal = 0.0;` |
|        - |  213 | `#endif` |
|      936 |  214 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|      936 |  215 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|        - |  216 | `			/* Convert as much as we can */` |
|        - |  217 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  218 | `			rVal = MemObjStringToInt(&(*pObj));` |
|        - |  219 | `#else` |
|      936 |  220 | `			SyStrToReal(sString.zString,sString.nByte,(void *)&rVal,0);` |
|        - |  221 | `#endif` |
|      466 |  222 | `		}` |
|      936 |  223 | `		return rVal;` |
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
|      841 |  256 | `}` |
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
|      476 |  270 | `PH7_PRIVATE sxi32 PH7_PhpFloatShape(char *zBuf,sxi32 nLen,int bGeneric)` |
|        4 |  271 | `{` |
|        - |  272 | `	sxi32 iExp,i;` |
|      480 |  273 | `	iExp = nLen - 1;` |
|     4100 |  274 | `	while( iExp > 0 && zBuf[iExp] != 'e' && zBuf[iExp] != 'E' ){` |
|     3623 |  275 | `		iExp--;` |
|        3 |  276 | `	}` |
|      480 |  277 | `	if( iExp <= 0 ){` |
|      436 |  278 | `		return nLen; /* No exponent part (fixed notation) */` |
|        - |  279 | `	}` |
|        - |  280 | `	{` |
|       45 |  281 | `		sxi32 iDig = iExp + 1;` |
|        - |  282 | `		sxi32 iFirst;` |
|       45 |  283 | `		if( zBuf[iDig] == '+' \|\| zBuf[iDig] == '-' ){` |
|       45 |  284 | `			iDig++;` |
|       22 |  285 | `		}` |
|       45 |  286 | `		iFirst = iDig;` |
|       78 |  287 | `		while( zBuf[iFirst] == '0' && iFirst + 1 < nLen` |
|       57 |  288 | `		 && zBuf[iFirst+1] >= '0' && zBuf[iFirst+1] <= '9' ){` |
|       23 |  289 | `			iFirst++;` |
|        1 |  290 | `		}` |
|       45 |  291 | `		if( iFirst > iDig ){` |
|       23 |  292 | `			sxi32 nStrip = iFirst - iDig;` |
|       67 |  293 | `			for( i = iDig ; i + nStrip <= nLen ; i++ ){` |
|       45 |  294 | `				zBuf[i] = zBuf[i+nStrip]; /* moves the NUL too */` |
|       23 |  295 | `			}` |
|       23 |  296 | `			nLen -= nStrip;` |
|       11 |  297 | `		}` |
|        - |  298 | `	}` |
|       45 |  299 | `	if( bGeneric ){` |
|       29 |  300 | `		int bHasDot = 0;` |
|       59 |  301 | `		for( i = 0 ; i < iExp ; i++ ){` |
|       41 |  302 | `			if( zBuf[i] == '.' ){ bHasDot = 1; break; }` |
|       16 |  303 | `		}` |
|       29 |  304 | `		if( !bHasDot ){` |
|      107 |  305 | `			for( i = nLen ; i >= iExp ; i-- ){` |
|       89 |  306 | `				zBuf[i+2] = zBuf[i]; /* moves the NUL too */` |
|       45 |  307 | `			}` |
|       19 |  308 | `			zBuf[iExp] = '.';` |
|       19 |  309 | `			zBuf[iExp+1] = '0';` |
|       19 |  310 | `			nLen += 2;` |
|        9 |  311 | `		}` |
|       14 |  312 | `	}` |
|       45 |  313 | `	return nLen;` |
|      242 |  314 | `}` |
|        - |  315 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|        - |  316 | `/*` |
|        - |  317 | ` * Return the string representation of a given ph7_value.` |
|        - |  318 | ` * This function never fail and always return SXRET_OK.` |
|        - |  319 | ` */` |
|    56216 |  320 | `static sxi32 MemObjStringValue(SyBlob *pOut,ph7_value *pObj,sxu8 bStrictBool)` |
|        5 |  321 | `{` |
|    56221 |  322 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|        - |  323 | `		/* Handle special floating-point values first */` |
|      372 |  324 | `		if( PH7_IS_NAN(pObj->rVal) ){` |
|      ! 0 |  325 | `			SyBlobAppend(&(*pOut),"NAN",3);` |
|      372 |  326 | `		}else if( PH7_IS_INF(pObj->rVal) ){` |
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
|      368 |  341 | `			sxi32 n = (sxi32)snprintf(zNum,sizeof(zNum),"%.14G",pObj->rVal);` |
|      368 |  342 | `			if( n < 0 \|\| n >= (sxi32)sizeof(zNum) ){` |
|      ! 0 |  343 | `				n = (sxi32)SyStrlen(zNum);` |
|      ! 0 |  344 | `			}` |
|      368 |  345 | `			n = PH7_PhpFloatShape(zNum,n,TRUE);` |
|      368 |  346 | `			SyBlobAppend(&(*pOut),zNum,(sxu32)n);` |
|        - |  347 | `#else` |
|        - |  348 | `			SyBlobFormat(&(*pOut),"%.15g",pObj->rVal);` |
|        - |  349 | `#endif` |
|        4 |  350 | `		}` |
|    56037 |  351 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|    55413 |  352 | `		SyBlobFormat(&(*pOut),"%qd",pObj->x.iVal);` |
|        - |  353 | `		/* %qd (BSD quad) is equivalent to %lld in the libc printf */` |
|    28149 |  354 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|      329 |  355 | `		if( bStrictBool ){` |
|        - |  356 | `			/* Actual string cast: true -> "1", false -> "" (like PHP) */` |
|      223 |  357 | `			if( pObj->x.iVal ){` |
|       21 |  358 | `				SyBlobAppend(&(*pOut),"1",sizeof("1")-1);` |
|        9 |  359 | `			}` |
|        - |  360 | `			/* false produces empty string, nothing to append */` |
|      114 |  361 | `		}else{` |
|        - |  362 | `			/* Display path (var_dump, print_r): show TRUE/FALSE */` |
|      109 |  363 | `			if( pObj->x.iVal ){` |
|       65 |  364 | `				SyBlobAppend(&(*pOut),"TRUE",sizeof("TRUE")-1);` |
|       34 |  365 | `			}else{` |
|       46 |  366 | `				SyBlobAppend(&(*pOut),"FALSE",sizeof("FALSE")-1);` |
|        - |  367 | `			}` |
|        5 |  368 | `		}` |
|      283 |  369 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
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
|    56221 |  391 | `	return SXRET_OK;` |
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
|    14004 |  405 | `static sxi32 MemObjBooleanValue(ph7_value *pObj)` |
|        5 |  406 | `{` |
|        - |  407 | `	sxi32 iFlags;` |
|    14009 |  408 | `	iFlags = pObj->iFlags;` |
|    14009 |  409 | `	if (iFlags & MEMOBJ_REAL ){` |
|        - |  410 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  411 | `		return pObj->rVal ? 1 : 0;` |
|        - |  412 | `#else` |
|       12 |  413 | `		return pObj->rVal != 0.0 ? 1 : 0;` |
|        - |  414 | `#endif` |
|    13999 |  415 | `	}else if( iFlags & MEMOBJ_INT ){` |
|      125 |  416 | `		return pObj->x.iVal ? 1 : 0;` |
|    13879 |  417 | `	}else if (iFlags & MEMOBJ_STRING) {` |
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
|    13817 |  438 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|    12779 |  439 | `		return 0;` |
|     1043 |  440 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|       20 |  441 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|       20 |  442 | `		sxu32 n = pMap->nEntry;` |
|       20 |  443 | `		PH7_HashmapUnref(pMap);` |
|       20 |  444 | `		return n > 0 ? TRUE : FALSE;` |
|     1025 |  445 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
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
|     1019 |  460 | `	}else if(iFlags & MEMOBJ_RES ){` |
|     1019 |  461 | `		return pObj->x.pOther != 0;` |
|        - |  462 | `	}` |
|        - |  463 | `	/* NOT REACHED */` |
|      ! 0 |  464 | `	return 0;` |
|     7007 |  465 | `}` |
|        - |  466 | `/*` |
|        - |  467 | ` * If the ph7_value is of type real,try to make it an integer also.` |
|        - |  468 | ` */` |
|     2832 |  469 | `static sxi32 MemObjTryIntger(ph7_value *pObj)` |
|        4 |  470 | `{` |
|     2836 |  471 | `	pObj->x.iVal = MemObjRealToInt(&(*pObj));` |
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
|     2832 |  484 | `	if( pObj->rVal ==(ph7_real)pObj->x.iVal && pObj->x.iVal>SMALLEST_INT64` |
|     1497 |  485 | `      && pObj->x.iVal<LARGEST_INT64 ){` |
|     1495 |  486 | `		  pObj->iFlags \|= MEMOBJ_INT;` |
|      748 |  487 | `	}` |
|     2836 |  488 | `	return SXRET_OK;` |
|        4 |  489 | `}` |
|        - |  490 | `/*` |
|        - |  491 | ` * Convert a ph7_value to type integer.Invalidate any prior representations.` |
|        - |  492 | ` */` |
|   403798 |  493 | `PH7_PRIVATE sxi32 PH7_MemObjToInteger(ph7_value *pObj)` |
|        5 |  494 | `{` |
|   403803 |  495 | `	if( (pObj->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  496 | `		/* Preform the conversion */` |
|      497 |  497 | `		pObj->x.iVal = MemObjIntValue(&(*pObj));` |
|        - |  498 | `		/* Invalidate any prior representations */` |
|      497 |  499 | `		SyBlobRelease(&pObj->sBlob);` |
|      497 |  500 | `		MemObjSetType(pObj,MEMOBJ_INT);` |
|      246 |  501 | `	}` |
|   403803 |  502 | `	return SXRET_OK;` |
|        5 |  503 | `}` |
|        - |  504 | `/*` |
|        - |  505 | ` * Convert a ph7_value to type real (Try to get an integer representation also).` |
|        - |  506 | ` * Invalidate any prior representations` |
|        - |  507 | ` */` |
|     2568 |  508 | `PH7_PRIVATE sxi32 PH7_MemObjToReal(ph7_value *pObj)` |
|        4 |  509 | `{` |
|     2572 |  510 | `	if((pObj->iFlags & MEMOBJ_REAL) == 0 ){` |
|        - |  511 | `		/* Preform the conversion */` |
|     1678 |  512 | `		pObj->rVal = MemObjRealValue(&(*pObj));` |
|        - |  513 | `		/* Invalidate any prior representations */` |
|     1678 |  514 | `		SyBlobRelease(&pObj->sBlob);` |
|     1678 |  515 | `		MemObjSetType(pObj,MEMOBJ_REAL);` |
|        - |  516 | `		/* Try to get an integer representation */` |
|     1678 |  517 | `		MemObjTryIntger(&(*pObj));` |
|      837 |  518 | `	}` |
|     2572 |  519 | `	return SXRET_OK;` |
|        4 |  520 | `}` |
|        - |  521 | `/*` |
|        - |  522 | ` * Convert a ph7_value to type boolean.Invalidate any prior representations.` |
|        - |  523 | ` */` |
|    15424 |  524 | `PH7_PRIVATE sxi32 PH7_MemObjToBool(ph7_value *pObj)` |
|        5 |  525 | `{` |
|    15429 |  526 | `	if( (pObj->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        - |  527 | `		/* Preform the conversion */` |
|    14009 |  528 | `		pObj->x.iVal = MemObjBooleanValue(&(*pObj));` |
|        - |  529 | `		/* Invalidate any prior representations */` |
|    14009 |  530 | `		SyBlobRelease(&pObj->sBlob);` |
|    14009 |  531 | `		MemObjSetType(pObj,MEMOBJ_BOOL);` |
|     7002 |  532 | `	}` |
|    15429 |  533 | `	return SXRET_OK;` |
|        5 |  534 | `}` |
|        - |  535 | `/*` |
|        - |  536 | ` * Convert a ph7_value to type string.Prior representations are NOT invalidated.` |
|        - |  537 | ` */` |
|   808445 |  538 | `PH7_PRIVATE sxi32 PH7_MemObjToString(ph7_value *pObj)` |
|        5 |  539 | `{` |
|   808450 |  540 | `	sxi32 rc = SXRET_OK;` |
|   808450 |  541 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  542 | `		/* Perform the conversion */` |
|    56003 |  543 | `		SyBlobReset(&pObj->sBlob); /* Reset the internal buffer */` |
|    56003 |  544 | `		rc = MemObjStringValue(&pObj->sBlob,&(*pObj),TRUE);` |
|    56003 |  545 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|    27999 |  546 | `	}` |
|   808450 |  547 | `	return rc;` |
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
|   455272 |  711 | `PH7_PRIVATE sxi32 PH7_MemObjIsNumeric(ph7_value *pObj)` |
|        5 |  712 | `{` |
|   455277 |  713 | `	if( pObj->iFlags & ( MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      309 |  714 | `		return TRUE;` |
|   454973 |  715 | `	}else if( pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|      179 |  716 | `		return FALSE;` |
|   454799 |  717 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        - |  718 | `		SyString sStr;` |
|        - |  719 | `		sxi32 rc;` |
|   454799 |  720 | `		SyStringInitFromBuf(&sStr,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|   454799 |  721 | `		if( sStr.nByte <= 0 ){` |
|        - |  722 | `			/* Empty string */` |
|       25 |  723 | `			return FALSE;` |
|        - |  724 | `		}` |
|        - |  725 | `		/* Check if the string representation looks like a numeric number */` |
|   454775 |  726 | `		rc = SyStrIsNumeric(sStr.zString,sStr.nByte,0,0);` |
|   454775 |  727 | `		return rc == SXRET_OK ? TRUE : FALSE;` |
|        - |  728 | `	}` |
|        - |  729 | `	/* NOT REACHED */` |
|      ! 0 |  730 | `	return FALSE;` |
|   227669 |  731 | `}` |
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
|    33208 |  744 | `PH7_PRIVATE sxi32 PH7_MemObjIsEmpty(ph7_value *pObj)` |
|        5 |  745 | `{` |
|    33213 |  746 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|       19 |  747 | `		return TRUE;` |
|    33197 |  748 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|       20 |  749 | `		return pObj->x.iVal == 0 ? TRUE : FALSE;` |
|    33179 |  750 | `	}else if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  751 | `		return pObj->rVal == (ph7_real)0 ? TRUE : FALSE;` |
|    33179 |  752 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|        5 |  753 | `		return !pObj->x.iVal;` |
|    33175 |  754 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|    26939 |  755 | `		if( SyBlobLength(&pObj->sBlob) <= 0 ){` |
|    21617 |  756 | `			return TRUE;` |
|      ! 0 |  757 | `		}else{` |
|        - |  758 | `			const char *zIn,*zEnd;` |
|     5327 |  759 | `			zIn = (const char *)SyBlobData(&pObj->sBlob);` |
|     5327 |  760 | `			zEnd = &zIn[SyBlobLength(&pObj->sBlob)];` |
|     5333 |  761 | `			while( zIn < zEnd ){` |
|     5333 |  762 | `				if( zIn[0] != '0' ){` |
|     5327 |  763 | `					break;` |
|        - |  764 | `				}` |
|        7 |  765 | `				zIn++;` |
|        1 |  766 | `			}` |
|     5327 |  767 | `			return zIn >= zEnd ? TRUE : FALSE;` |
|      ! 0 |  768 | `		}` |
|     6241 |  769 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|     6241 |  770 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|     6241 |  771 | `		return pMap->nEntry == 0 ? TRUE : FALSE;` |
|      ! 0 |  772 | `	}else if ( pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|      ! 0 |  773 | `		return FALSE;` |
|        - |  774 | `	}` |
|        - |  775 | `	/* Assume empty by default */` |
|      ! 0 |  776 | `	return TRUE;` |
|    16609 |  777 | `}` |
|        - |  778 | `/*` |
|        - |  779 | ` * Convert a ph7_value so that it has types MEMOBJ_REAL or MEMOBJ_INT` |
|        - |  780 | ` * or both.` |
|        - |  781 | ` * Invalidate any prior representations. Every effort is made to force` |
|        - |  782 | ` * the conversion, even if the input is a string that does not look` |
|        - |  783 | ` * completely like a number.Convert as much of the string as we can` |
|        - |  784 | ` * and ignore the rest.` |
|        - |  785 | ` */` |
|   440841 |  786 | `PH7_PRIVATE sxi32 PH7_MemObjToNumeric(ph7_value *pObj)` |
|        5 |  787 | `{` |
|   440846 |  788 | `	if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|   440734 |  789 | `		if( pObj->iFlags & (MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|        3 |  790 | `			if( pObj->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  791 | `				pObj->x.iVal = 0;` |
|      ! 0 |  792 | `			}` |
|        3 |  793 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|        1 |  794 | `		}` |
|        - |  795 | `		/* Already numeric */` |
|   440734 |  796 | `		return  SXRET_OK;` |
|        - |  797 | `	}` |
|      113 |  798 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|      113 |  799 | `		sxi32 rc = SXERR_INVALID;` |
|      113 |  800 | `		sxu8 bReal = FALSE;` |
|        - |  801 | `		SyString sString;` |
|      113 |  802 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|        - |  803 | `		/* Check if the given string looks like a numeric number */` |
|      113 |  804 | `		if( sString.nByte > 0 ){` |
|      111 |  805 | `			rc = SyStrIsNumeric(sString.zString,sString.nByte,&bReal,0);` |
|       55 |  806 | `		}` |
|      113 |  807 | `		if( bReal ){` |
|        7 |  808 | `			PH7_MemObjToReal(&(*pObj));` |
|        4 |  809 | `		}else{` |
|      107 |  810 | `			if( rc != SXRET_OK ){` |
|        - |  811 | `				/* The input does not look at all like a number,set the value to 0 */` |
|        3 |  812 | `				pObj->x.iVal = 0;` |
|        2 |  813 | `			}else{` |
|        - |  814 | `				/* Convert as much as we can */` |
|      105 |  815 | `				pObj->x.iVal = MemObjStringToInt(&(*pObj));` |
|        - |  816 | `			}` |
|      107 |  817 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|      107 |  818 | `			SyBlobRelease(&pObj->sBlob);` |
|        1 |  819 | `		}` |
|       56 |  820 | `	}else if(pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)){` |
|      ! 0 |  821 | `		PH7_MemObjToInteger(pObj);` |
|      ! 0 |  822 | `	}else{` |
|        - |  823 | `		/* Perform a blind cast */` |
|      ! 0 |  824 | `		PH7_MemObjToReal(&(*pObj));` |
|        - |  825 | `	}` |
|      113 |  826 | `	return SXRET_OK;` |
|   220468 |  827 | `}` |
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
|     1094 |  923 | `PH7_PRIVATE sxi32 PH7_MemObjTryInteger(ph7_value *pObj)` |
|        1 |  924 | `{` |
|     1095 |  925 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|        - |  926 | `		/* Work only with reals */` |
|     1095 |  927 | `		MemObjTryIntger(&(*pObj));` |
|      547 |  928 | `	}` |
|     1095 |  929 | `	return SXRET_OK;` |
|        1 |  930 | `}` |
|        - |  931 | `/*` |
|        - |  932 | ` * Initialize a ph7_value to the null type.` |
|        - |  933 | ` */` |
|  8639153 |  934 | `PH7_PRIVATE sxi32 PH7_MemObjInit(ph7_vm *pVm,ph7_value *pObj)` |
|        5 |  935 | `{` |
|        - |  936 | `	/* Zero the structure */` |
|  8639158 |  937 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - |  938 | `	/* Initialize fields */` |
|  8639158 |  939 | `	pObj->pVm = pVm;` |
|  8639158 |  940 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - |  941 | `	/* Set the NULL type */` |
|  8639158 |  942 | `	pObj->iFlags = MEMOBJ_NULL;` |
|  8639158 |  943 | `	return SXRET_OK;` |
|        5 |  944 | `}` |
|        - |  945 | `/*` |
|        - |  946 | ` * Initialize a ph7_value to the integer type.` |
|        - |  947 | ` */` |
|   166814 |  948 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromInt(ph7_vm *pVm,ph7_value *pObj,sxi64 iVal)` |
|        5 |  949 | `{` |
|        - |  950 | `	/* Zero the structure */` |
|   166819 |  951 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - |  952 | `	/* Initialize fields */` |
|   166819 |  953 | `	pObj->pVm = pVm;` |
|   166819 |  954 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - |  955 | `	/* Set the desired type */` |
|   166819 |  956 | `	pObj->x.iVal = iVal;` |
|   166819 |  957 | `	pObj->iFlags = MEMOBJ_INT;` |
|   166819 |  958 | `	return SXRET_OK;` |
|        5 |  959 | `}` |
|        - |  960 | `/*` |
|        - |  961 | ` * Initialize a ph7_value to the boolean type.` |
|        - |  962 | ` */` |
|    17108 |  963 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromBool(ph7_vm *pVm,ph7_value *pObj,sxi32 iVal)` |
|        5 |  964 | `{` |
|        - |  965 | `	/* Zero the structure */` |
|    17113 |  966 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - |  967 | `	/* Initialize fields */` |
|    17113 |  968 | `	pObj->pVm = pVm;` |
|    17113 |  969 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - |  970 | `	/* Set the desired type */` |
|    17113 |  971 | `	pObj->x.iVal = iVal ? 1 : 0;` |
|    17113 |  972 | `	pObj->iFlags = MEMOBJ_BOOL;` |
|    17113 |  973 | `	return SXRET_OK;` |
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
|    50684 |  995 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromArray(ph7_vm *pVm,ph7_value *pObj,ph7_hashmap *pArray)` |
|        5 |  996 | `{` |
|        - |  997 | `	/* Zero the structure */` |
|    50689 |  998 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - |  999 | `	/* Initialize fields */` |
|    50689 | 1000 | `	pObj->pVm = pVm;` |
|    50689 | 1001 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1002 | `	/* Set the desired type */` |
|    50689 | 1003 | `	pObj->iFlags = MEMOBJ_HASHMAP;` |
|    50689 | 1004 | `	pObj->x.pOther = pArray;` |
|    50689 | 1005 | `	return SXRET_OK;` |
|        5 | 1006 | `}` |
|        - | 1007 | `/*` |
|        - | 1008 | ` * Initialize a ph7_value to the string type.` |
|        - | 1009 | ` */` |
|   531240 | 1010 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromString(ph7_vm *pVm,ph7_value *pObj,const SyString *pVal)` |
|        5 | 1011 | `{` |
|        - | 1012 | `	/* Zero the structure */` |
|   531245 | 1013 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1014 | `	/* Initialize fields */` |
|   531245 | 1015 | `	pObj->pVm = pVm;` |
|   531245 | 1016 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|   531245 | 1017 | `	if( pVal ){` |
|        - | 1018 | `		/* Append contents */` |
|   328639 | 1019 | `		SyBlobAppend(&pObj->sBlob,(const void *)pVal->zString,pVal->nByte);` |
|   164317 | 1020 | `	}` |
|        - | 1021 | `	/* Set the desired type */` |
|   531245 | 1022 | `	pObj->iFlags = MEMOBJ_STRING;` |
|   531245 | 1023 | `	return SXRET_OK;` |
|        5 | 1024 | `}` |
|        - | 1025 | `/*` |
|        - | 1026 | ` * Append some contents to the internal buffer of a given ph7_value.` |
|        - | 1027 | ` * If the given ph7_value is not of type string,this function` |
|        - | 1028 | ` * invalidate any prior representation and set the string type.` |
|        - | 1029 | ` * Then a simple append operation is performed.` |
|        - | 1030 | ` */` |
|   386008 | 1031 | `PH7_PRIVATE sxi32 PH7_MemObjStringAppend(ph7_value *pObj,const char *zData,sxu32 nLen)` |
|        5 | 1032 | `{` |
|        - | 1033 | `	sxi32 rc;` |
|   386013 | 1034 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 1035 | `		/* Invalidate any prior representation */` |
|     1843 | 1036 | `		PH7_MemObjRelease(pObj);` |
|     1843 | 1037 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|      919 | 1038 | `	}` |
|        - | 1039 | `	/* Append contents */` |
|   386013 | 1040 | `	rc = SyBlobAppend(&pObj->sBlob,zData,nLen);` |
|   386013 | 1041 | `	return rc;` |
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
|  4728081 | 1066 | `PH7_PRIVATE sxi32 PH7_MemObjStore(ph7_value *pSrc,ph7_value *pDest)` |
|        5 | 1067 | `{` |
|  4728086 | 1068 | `	ph7_class_instance *pObj = 0;` |
|  4728086 | 1069 | `	ph7_hashmap *pMap = 0;` |
|        - | 1070 | `	sxi32 rc;` |
|  4728086 | 1071 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1072 | `		/* Increment reference count */` |
|   176413 | 1073 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  4639882 | 1074 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|        - | 1075 | `		/* Increment reference count */` |
|     3361 | 1076 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|     1678 | 1077 | `	}` |
|  4728086 | 1078 | `	if( pDest->iFlags & MEMOBJ_HASHMAP ){` |
|    57917 | 1079 | `		pMap = (ph7_hashmap *)pDest->x.pOther;` |
|  4699130 | 1080 | `	}else if( pDest->iFlags & MEMOBJ_OBJ ){` |
|     4951 | 1081 | `		pObj = (ph7_class_instance *)pDest->x.pOther;` |
|     2473 | 1082 | `	}` |
|  4728086 | 1083 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|  4728086 | 1084 | `	pDest->iFlags &= ~MEMOBJ_AUX;` |
|  4728086 | 1085 | `	rc = SXRET_OK;` |
|  4728086 | 1086 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  3902727 | 1087 | `		SyBlobReset(&pDest->sBlob);` |
|  3902727 | 1088 | `		rc = SyBlobDup(&pSrc->sBlob,&pDest->sBlob);` |
|  1951366 | 1089 | `	}else{` |
|   825364 | 1090 | `		if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|   274776 | 1091 | `			SyBlobRelease(&pDest->sBlob);` |
|   137428 | 1092 | `		}` |
|        - | 1093 | `	}` |
|  4728086 | 1094 | `	if( pMap ){` |
|    57917 | 1095 | `		PH7_HashmapUnref(pMap);` |
|  4699130 | 1096 | `	}else if( pObj ){` |
|     4951 | 1097 | `		PH7_ClassInstanceUnref(pObj);` |
|     2473 | 1098 | `	}` |
|  4728086 | 1099 | `	return rc;` |
|        5 | 1100 | `}` |
|        - | 1101 | `/*` |
|        - | 1102 | ` * Duplicate the contents of a ph7_value but do not copy internal` |
|        - | 1103 | ` * buffer contents,simply point to it.` |
|        - | 1104 | ` */` |
|  6505980 | 1105 | `PH7_PRIVATE sxi32 PH7_MemObjLoad(ph7_value *pSrc,ph7_value *pDest)` |
|        5 | 1106 | `{` |
|  6505985 | 1107 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),` |
|        - | 1108 | `		sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|  6505985 | 1109 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1110 | `		/* Increment reference count */` |
|   459595 | 1111 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  6276190 | 1112 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|        - | 1113 | `		/* Increment reference count */` |
|    21559 | 1114 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|    10777 | 1115 | `	}` |
|  6505985 | 1116 | `	if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|       40 | 1117 | `		SyBlobRelease(&pDest->sBlob);` |
|       18 | 1118 | `	}` |
|  6505985 | 1119 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  3516557 | 1120 | `		SyBlobReadOnly(&pDest->sBlob,SyBlobData(&pSrc->sBlob),SyBlobLength(&pSrc->sBlob));` |
|  1758389 | 1121 | `	}` |
|  6505985 | 1122 | `	return SXRET_OK;` |
|        5 | 1123 | `}` |
|        - | 1124 | `/*` |
|        - | 1125 | ` * Invalidate any prior representation of a given ph7_value.` |
|        - | 1126 | ` */` |
| 13806675 | 1127 | `PH7_PRIVATE sxi32 PH7_MemObjRelease(ph7_value *pObj)` |
|        5 | 1128 | `{` |
| 13806680 | 1129 | `	if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
| 11755089 | 1130 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|   597563 | 1131 | `			PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
| 11456310 | 1132 | `		}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|    36841 | 1133 | `			PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|    18418 | 1134 | `		}` |
|        - | 1135 | `		/* Release the internal buffer */` |
| 11755089 | 1136 | `		SyBlobRelease(&pObj->sBlob);` |
|        - | 1137 | `		/* Invalidate any prior representation */` |
| 11755089 | 1138 | `		pObj->iFlags = MEMOBJ_NULL;` |
|  5877909 | 1139 | `	}` |
| 13806680 | 1140 | `	return SXRET_OK;` |
|        5 | 1141 | `}` |
|        - | 1142 | `/*` |
|        - | 1143 | ` * Compare two ph7_values.` |
|        - | 1144 | ` * Return 0 if the values are equals, > 0 if pObj1 is greater than pObj2` |
|        - | 1145 | ` * or < 0 if pObj2 is greater than pObj1.` |
|        - | 1146 | ` * Type comparison table taken from the PHP language reference manual.` |
|        - | 1147 | ` * Comparisons of $x with PHP functions Expression` |
|        - | 1148 | ` *              gettype() 	empty() 	is_null() 	isset() 	boolean : if($x)` |
|        - | 1149 | ` * $x = ""; 	string 	    TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1150 | ` * $x = null 	NULL 	    TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1151 | ` * var $x; 	    NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1152 | ` * $x is undefined 	NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1153 | ` *  $x = array(); 	array 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1154 | ` * $x = false; 	boolean 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1155 | ` * $x = true; 	boolean 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1156 | ` * $x = 1; 	    integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1157 | ` * $x = 42; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1158 | ` * $x = 0; 	    integer 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1159 | ` * $x = -1; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1160 | ` * $x = "1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1161 | ` * $x = "0"; 	string 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1162 | ` * $x = "-1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1163 | ` * $x = "php"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1164 | ` * $x = "true"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1165 | ` * $x = "false"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1166 | ` *      Loose comparisons with ==` |
|        - | 1167 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|        - | 1168 | ` * TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1169 | ` * FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|        - | 1170 | ` * 1 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1171 | ` * 0 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE` |
|        - | 1172 | ` * -1 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1173 | ` * "1" 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1174 | ` * "0" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1175 | ` * "-1" 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1176 | ` * NULL 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|        - | 1177 | ` * array() 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1178 | ` * "php" 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1179 | ` * "" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE` |
|        - | 1180 | ` *    Strict comparisons with ===` |
|        - | 1181 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|        - | 1182 | ` * TRUE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1183 | ` * FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1184 | ` * 1 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1185 | ` * 0 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1186 | ` * -1 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1187 | ` * "1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1188 | ` * "0" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1189 | ` * "-1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1190 | ` * NULL 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE` |
|        - | 1191 | ` * array() 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE` |
|        - | 1192 | ` * "php" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1193 | ` * "" 	    FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE` |
|        - | 1194 | ` */` |
|  1241376 | 1195 | `PH7_PRIVATE sxi32 PH7_MemObjCmp(ph7_value *pObj1,ph7_value *pObj2,int bStrict,int iNest)` |
|        5 | 1196 | `{` |
|        - | 1197 | `	sxi32 iComb;` |
|        - | 1198 | `	sxi32 rc;` |
|  1241381 | 1199 | `	if( bStrict ){` |
|        - | 1200 | `		sxi32 iF1,iF2;` |
|        - | 1201 | `		/* Strict comparisons with === */` |
|   640617 | 1202 | `		iF1 = pObj1->iFlags&~MEMOBJ_AUX;` |
|   640617 | 1203 | `		iF2 = pObj2->iFlags&~MEMOBJ_AUX;` |
|   640617 | 1204 | `		if( iF1 != iF2 ){` |
|        - | 1205 | `			/* Not of the same type */` |
|   179761 | 1206 | `			return 1;` |
|        - | 1207 | `		}` |
|   230428 | 1208 | `	}` |
|        - | 1209 | `	/* Combine flag together */` |
|  1061625 | 1210 | `	iComb = pObj1->iFlags\|pObj2->iFlags;` |
|  1061625 | 1211 | `	if( iComb & (MEMOBJ_NULL\|MEMOBJ_RES\|MEMOBJ_BOOL) ){` |
|        - | 1212 | `		/* Convert to boolean: Keep in mind FALSE < TRUE */` |
|    20711 | 1213 | `		if( (pObj1->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     7355 | 1214 | `			PH7_MemObjToBool(pObj1);` |
|     3675 | 1215 | `		}` |
|    20711 | 1216 | `		if( (pObj2->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     6389 | 1217 | `			PH7_MemObjToBool(pObj2);` |
|     3192 | 1218 | `		}` |
|    20711 | 1219 | `		return (sxi32)((pObj1->x.iVal != 0) - (pObj2->x.iVal != 0));` |
|  1040919 | 1220 | `	}else if ( iComb & MEMOBJ_HASHMAP ){` |
|        - | 1221 | `		/* Hashmap aka 'array' comparison */` |
|       29 | 1222 | `		if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1223 | `			/* Array is always greater */` |
|      ! 0 | 1224 | `			return -1;` |
|        - | 1225 | `		}` |
|       29 | 1226 | `		if( (pObj2->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1227 | `			/* Array is always greater */` |
|      ! 0 | 1228 | `			return 1;` |
|        - | 1229 | `		}` |
|        - | 1230 | `		/* Perform the comparison */` |
|       29 | 1231 | `		rc = PH7_HashmapCmp((ph7_hashmap *)pObj1->x.pOther,(ph7_hashmap *)pObj2->x.pOther,bStrict);` |
|       29 | 1232 | `		return rc;` |
|  1040891 | 1233 | `	}else if(iComb & MEMOBJ_OBJ ){` |
|        - | 1234 | `		/* Object comparison */` |
|      229 | 1235 | `		if( (pObj1->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 1236 | `			/* Object is always greater */` |
|      ! 0 | 1237 | `			return -1;` |
|        - | 1238 | `		}` |
|      229 | 1239 | `		if( (pObj2->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 1240 | `			/* Object is always greater */` |
|      ! 0 | 1241 | `			return 1;` |
|        - | 1242 | `		}` |
|        - | 1243 | `		/* Perform the comparison */` |
|      229 | 1244 | `		rc = PH7_ClassInstanceCmp((ph7_class_instance *)pObj1->x.pOther,(ph7_class_instance *)pObj2->x.pOther,bStrict,iNest);` |
|      229 | 1245 | `		return rc;` |
|  1040667 | 1246 | `	}else if ( iComb & MEMOBJ_STRING ){` |
|        - | 1247 | `		SyString s1,s2;` |
|   666841 | 1248 | `		if( !bStrict ){` |
|        - | 1249 | `			/*` |
|        - | 1250 | `			 * According to the PHP language reference manual:` |
|        - | 1251 | `			 *` |
|        - | 1252 | `			 *  If you compare a number with a string or the comparison involves numerical` |
|        - | 1253 | `			 *  strings, then each string is converted to a number and the comparison` |
|        - | 1254 | `			 *  performed numerically.` |
|        - | 1255 | `			 */` |
|   227345 | 1256 | `			if( PH7_MemObjIsNumeric(pObj1) ){` |
|        - | 1257 | `				/* Perform a numeric comparison */` |
|       17 | 1258 | `				goto Numeric;` |
|        - | 1259 | `			}` |
|   227329 | 1260 | `			if( PH7_MemObjIsNumeric(pObj2) ){` |
|        - | 1261 | `				/* Perform a numeric comparison */` |
|      ! 0 | 1262 | `				goto Numeric;` |
|        - | 1263 | `			}` |
|   113676 | 1264 | `		}` |
|        - | 1265 | `		/* Perform a strict string comparison.*/` |
|   666825 | 1266 | `		if( (pObj1->iFlags&MEMOBJ_STRING) == 0 ){` |
|      ! 0 | 1267 | `			PH7_MemObjToString(pObj1);` |
|      ! 0 | 1268 | `		}` |
|   666825 | 1269 | `		if( (pObj2->iFlags&MEMOBJ_STRING) == 0 ){` |
|      ! 0 | 1270 | `			PH7_MemObjToString(pObj2);` |
|      ! 0 | 1271 | `		}` |
|   666825 | 1272 | `		SyStringInitFromBuf(&s1,SyBlobData(&pObj1->sBlob),SyBlobLength(&pObj1->sBlob));` |
|   666825 | 1273 | `		SyStringInitFromBuf(&s2,SyBlobData(&pObj2->sBlob),SyBlobLength(&pObj2->sBlob));` |
|        - | 1274 | `		/*` |
|        - | 1275 | `		 * Strings are compared using memcmp(). If one value is an exact prefix of the` |
|        - | 1276 | `		 * other, then the shorter value is less than the longer value.` |
|        - | 1277 | `		 */` |
|   666825 | 1278 | `		rc = SyMemcmp((const void *)s1.zString,(const void *)s2.zString,SXMIN(s1.nByte,s2.nByte));` |
|   666825 | 1279 | `		if( rc == 0 ){` |
|   229604 | 1280 | `			if( s1.nByte != s2.nByte ){` |
|     1656 | 1281 | `				rc = s1.nByte < s2.nByte ? -1 : 1;` |
|      828 | 1282 | `			}` |
|   114802 | 1283 | `		}` |
|   666825 | 1284 | `		return rc;` |
|   373831 | 1285 | `	}else if( iComb & (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|   186871 | 1286 | `Numeric:` |
|        - | 1287 | `		/* Perform a numeric comparison if one of the operand is numeric(integer or real) */` |
|   373847 | 1288 | `		if( (pObj1->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|        5 | 1289 | `			PH7_MemObjToNumeric(pObj1);` |
|        2 | 1290 | `		}` |
|   373847 | 1291 | `		if( (pObj2->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|       15 | 1292 | `			PH7_MemObjToNumeric(pObj2);` |
|        7 | 1293 | `		}` |
|   373847 | 1294 | `		if( (pObj1->iFlags & pObj2->iFlags & MEMOBJ_INT) == 0) {` |
|        - | 1295 | `			/*` |
|        - | 1296 | `			 * Symisc eXtension to the PHP language:` |
|        - | 1297 | `			 *  Floating point comparison is introduced and works as expected.` |
|        - | 1298 | `			 */` |
|        - | 1299 | `			ph7_real r1,r2;` |
|        - | 1300 | `			/* Compare as reals */` |
|      233 | 1301 | `			if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|       11 | 1302 | `				PH7_MemObjToReal(pObj1);` |
|        5 | 1303 | `			}` |
|      233 | 1304 | `			r1 = pObj1->rVal;` |
|      233 | 1305 | `			if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|       37 | 1306 | `				PH7_MemObjToReal(pObj2);` |
|       18 | 1307 | `			}` |
|      233 | 1308 | `			r2 = pObj2->rVal;` |
|      233 | 1309 | `			if( PH7_IS_NAN(r1) \|\| PH7_IS_NAN(r2) ){` |
|        - | 1310 | `				/*` |
|        - | 1311 | `				 * Keep a strict three-way comparator contract even for NaN values.` |
|        - | 1312 | `				 * For ordering purposes, NaN compares equal to NaN and greater than` |
|        - | 1313 | `				 * any non-NaN numeric value.` |
|        - | 1314 | `				 */` |
|       45 | 1315 | `				if( PH7_IS_NAN(r1) ){` |
|       35 | 1316 | `					return PH7_IS_NAN(r2) ? 0 : 1;` |
|        - | 1317 | `				}` |
|       11 | 1318 | `				return -1;` |
|        - | 1319 | `			}` |
|      189 | 1320 | `			if( r1 > r2 ){` |
|       35 | 1321 | `				return 1;` |
|      155 | 1322 | `			}else if( r1 < r2 ){` |
|      121 | 1323 | `				return -1;` |
|        - | 1324 | `			}` |
|       35 | 1325 | `			return 0;` |
|      ! 0 | 1326 | `		}else{` |
|        - | 1327 | `			/* Integer comparison */` |
|   373615 | 1328 | `			if( pObj1->x.iVal > pObj2->x.iVal ){` |
|     5275 | 1329 | `				return 1;` |
|   368345 | 1330 | `			}else if( pObj1->x.iVal < pObj2->x.iVal ){` |
|   362983 | 1331 | `				return -1;` |
|        - | 1332 | `			}` |
|     5367 | 1333 | `			return 0;` |
|        - | 1334 | `		}` |
|        - | 1335 | `	}` |
|        - | 1336 | `	/* NOT REACHED */` |
|      ! 0 | 1337 | `	return 0;` |
|   620749 | 1338 | `}` |
|        - | 1339 | `/*` |
|        - | 1340 | ` * Perform an addition operation of two ph7_values.` |
|        - | 1341 | ` * The reason this function is implemented here rather than 'vm.c'` |
|        - | 1342 | ` * is that the '+' operator is overloaded.` |
|        - | 1343 | ` * That is,the '+' operator is used for arithmetic operation and also` |
|        - | 1344 | ` * used for operation on arrays [i.e: union]. When used with an array` |
|        - | 1345 | ` * The + operator returns the right-hand array appended to the left-hand array.` |
|        - | 1346 | ` * For keys that exist in both arrays, the elements from the left-hand array` |
|        - | 1347 | ` * will be used, and the matching elements from the right-hand array will` |
|        - | 1348 | ` * be ignored.` |
|        - | 1349 | ` * This function take care of handling all the scenarios.` |
|        - | 1350 | ` */` |
|     9684 | 1351 | `PH7_PRIVATE sxi32 PH7_MemObjAdd(ph7_value *pObj1,ph7_value *pObj2,int bAddStore)` |
|        5 | 1352 | `{` |
|     9689 | 1353 | `	if( ((pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1354 | `			/* Arithemtic operation */` |
|     5891 | 1355 | `			PH7_MemObjToNumeric(pObj1);` |
|     5891 | 1356 | `			PH7_MemObjToNumeric(pObj2);` |
|     5891 | 1357 | `			if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_REAL ){` |
|        - | 1358 | `				/* Floating point arithmetic */` |
|        - | 1359 | `				ph7_real a,b;` |
|       65 | 1360 | `				if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|       31 | 1361 | `					PH7_MemObjToReal(pObj1);` |
|       15 | 1362 | `				}` |
|       65 | 1363 | `				if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 | 1364 | `					PH7_MemObjToReal(pObj2);` |
|        1 | 1365 | `				}` |
|       65 | 1366 | `				a = pObj1->rVal;` |
|       65 | 1367 | `				b = pObj2->rVal;` |
|       65 | 1368 | `				pObj1->rVal = a+b;` |
|       65 | 1369 | `				MemObjSetType(pObj1,MEMOBJ_REAL);` |
|        - | 1370 | `				/* Try to get an integer representation also */` |
|       65 | 1371 | `				MemObjTryIntger(&(*pObj1));` |
|       33 | 1372 | `			}else{` |
|        - | 1373 | `				/* Integer arithmetic */` |
|        - | 1374 | `				sxi64 a,b;` |
|     5827 | 1375 | `				a = pObj1->x.iVal;` |
|     5827 | 1376 | `				b = pObj2->x.iVal;` |
|     5827 | 1377 | `				pObj1->x.iVal = a+b;` |
|     5827 | 1378 | `				MemObjSetType(pObj1,MEMOBJ_INT);` |
|        - | 1379 | `			}` |
|     2948 | 1380 | `	}else{` |
|     3803 | 1381 | `		if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP ){` |
|        - | 1382 | `			ph7_hashmap *pMap;` |
|        - | 1383 | `			sxi32 rc;` |
|     3803 | 1384 | `			if( bAddStore ){` |
|        - | 1385 | `				/* Do not duplicate the hashmap,use the left one since its an add&store operation.` |
|        - | 1386 | `				 */` |
|        3 | 1387 | `				if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1388 | `					/* Force a hashmap cast */` |
|      ! 0 | 1389 | `					rc = PH7_MemObjToHashmap(pObj1);` |
|      ! 0 | 1390 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 1391 | `						PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|      ! 0 | 1392 | `						return rc;` |
|        - | 1393 | `					}` |
|      ! 0 | 1394 | `				}` |
|        - | 1395 | `				/* COW separate before in-place mutation */` |
|        3 | 1396 | `				pMap = PH7_HashmapCowSeparate(pObj1->pVm,pObj1);` |
|        2 | 1397 | `			}else{` |
|        - | 1398 | `				/* Create a new hashmap */` |
|     3801 | 1399 | `				pMap = PH7_NewHashmap(pObj1->pVm,0,0);` |
|     3801 | 1400 | `				if( pMap == 0){` |
|      ! 0 | 1401 | `					PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|      ! 0 | 1402 | `					return SXERR_MEM;` |
|        - | 1403 | `				}` |
|        - | 1404 | `			}` |
|     3803 | 1405 | `			if( !bAddStore ){` |
|     3801 | 1406 | `				if(pObj1->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1407 | `					/* Perform a hashmap duplication */` |
|     3801 | 1408 | `					PH7_HashmapDup((ph7_hashmap *)pObj1->x.pOther,pMap);` |
|     1903 | 1409 | `				}else{` |
|      ! 0 | 1410 | `					if((pObj1->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 1411 | `						/* Simple insertion */` |
|      ! 0 | 1412 | `						PH7_HashmapInsert(pMap,0,pObj1);` |
|      ! 0 | 1413 | `					}` |
|        - | 1414 | `				}` |
|     1898 | 1415 | `			}` |
|        - | 1416 | `			/* Perform the union */` |
|     3803 | 1417 | `			if(pObj2->iFlags & MEMOBJ_HASHMAP ){` |
|     3803 | 1418 | `				PH7_HashmapUnion(pMap,(ph7_hashmap *)pObj2->x.pOther);` |
|     1904 | 1419 | `			}else{` |
|      ! 0 | 1420 | `				if((pObj2->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 1421 | `					/* Simple insertion */` |
|      ! 0 | 1422 | `					PH7_HashmapInsert(pMap,0,pObj2);` |
|      ! 0 | 1423 | `				}` |
|        - | 1424 | `			}` |
|        - | 1425 | `			/* Reflect the change */` |
|     3803 | 1426 | `			if( pObj1->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 1427 | `				SyBlobRelease(&pObj1->sBlob);` |
|      ! 0 | 1428 | `			}` |
|     3803 | 1429 | `			pObj1->x.pOther = pMap;` |
|     3803 | 1430 | `			MemObjSetType(pObj1,MEMOBJ_HASHMAP);` |
|     1899 | 1431 | `		}` |
|        - | 1432 | `	}` |
|     9689 | 1433 | `	return SXRET_OK;` |
|     4847 | 1434 | `}` |
|        - | 1435 | `/*` |
|        - | 1436 | ` * Return a printable representation of the type of a given` |
|        - | 1437 | ` * ph7_value.` |
|        - | 1438 | ` */` |
|      436 | 1439 | `PH7_PRIVATE const char * PH7_MemObjTypeDump(ph7_value *pVal)` |
|        5 | 1440 | `{` |
|      441 | 1441 | `	const char *zType = "";` |
|      441 | 1442 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|        5 | 1443 | `		zType = "null";` |
|      439 | 1444 | `	}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|        - | 1445 | `		/* REAL is authoritative over a cached MEMOBJ_INT: an integer-valued` |
|        - | 1446 | `		 * real (e.g. 1.0) is reported as "double", matching PHP's gettype(). */` |
|        7 | 1447 | `		zType = "double";` |
|      434 | 1448 | `	}else if( pVal->iFlags & MEMOBJ_INT ){` |
|       99 | 1449 | `		zType = "int";` |
|      383 | 1450 | `	}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|       77 | 1451 | `		zType = "string";` |
|      298 | 1452 | `	}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|      109 | 1453 | `		zType = "bool";` |
|      209 | 1454 | `	}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|       16 | 1455 | `		zType = "array";` |
|      148 | 1456 | `	}else if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      141 | 1457 | `		zType = "object";` |
|       69 | 1458 | `	}else if( pVal->iFlags & MEMOBJ_RES ){` |
|      ! 0 | 1459 | `		zType = "resource";` |
|      ! 0 | 1460 | `	}` |
|      441 | 1461 | `	return zType;` |
|        5 | 1462 | `}` |
|        - | 1463 | `/*` |
|        - | 1464 | ` * Dump a ph7_value [i.e: get a printable representation of it's type and contents.].` |
|        - | 1465 | ` * Store the dump in the given blob.` |
|        - | 1466 | ` */` |
|      448 | 1467 | `PH7_PRIVATE sxi32 PH7_MemObjDump(` |
|        - | 1468 | `	SyBlob *pOut,      /* Store the dump here */` |
|        - | 1469 | `	ph7_value *pObj,   /* Dump this */` |
|        - | 1470 | `	int ShowType,      /* TRUE to output value type */` |
|        - | 1471 | `	int nTab,          /* # of Whitespace to insert */` |
|        - | 1472 | `	int nDepth,        /* Nesting level */` |
|        - | 1473 | `	int isRef          /* TRUE if referenced object */` |
|        - | 1474 | `	)` |
|        4 | 1475 | `{` |
|      452 | 1476 | `	sxi32 rc = SXRET_OK;` |
|        - | 1477 | `	const char *zType;` |
|        - | 1478 | `	int i;` |
|     4568 | 1479 | `	for( i = 0 ; i < nTab ; i++ ){` |
|     4120 | 1480 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     2062 | 1481 | `	}` |
|      452 | 1482 | `	if( ShowType ){` |
|      404 | 1483 | `		if( isRef ){` |
|      ! 0 | 1484 | `			SyBlobAppend(&(*pOut),"&",sizeof(char));` |
|      ! 0 | 1485 | `		}` |
|        - | 1486 | `		/* Get value type first. var_dump() labels reals "float" (PHP), whereas` |
|        - | 1487 | `		 * gettype()/PH7_MemObjTypeDump use the legacy "double" spelling. */` |
|      404 | 1488 | `		if( (pObj->iFlags & MEMOBJ_REAL) && (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        3 | 1489 | `			zType = "float";` |
|        2 | 1490 | `		}else{` |
|      402 | 1491 | `			zType = PH7_MemObjTypeDump(pObj);` |
|        - | 1492 | `		}` |
|      404 | 1493 | `		SyBlobAppend(&(*pOut),zType,SyStrlen(zType));` |
|      200 | 1494 | `	}` |
|      452 | 1495 | `	if((pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|      450 | 1496 | `		if ( ShowType ){` |
|      402 | 1497 | `			SyBlobAppend(&(*pOut),"(",sizeof(char));` |
|      199 | 1498 | `		}` |
|      450 | 1499 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1500 | `			/* Dump hashmap entries */` |
|       24 | 1501 | `			rc = PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,ShowType,nTab+1,nDepth+1);` |
|      439 | 1502 | `		}else if(pObj->iFlags & MEMOBJ_OBJ ){` |
|        - | 1503 | `			/* Dump class instance attributes */` |
|      141 | 1504 | `			rc = PH7_ClassInstanceDump(&(*pOut),(ph7_class_instance *)pObj->x.pOther,ShowType,nTab+1,nDepth+1);` |
|       72 | 1505 | `		}else{` |
|      290 | 1506 | `			SyBlob *pContents = &pObj->sBlob;` |
|        - | 1507 | `			/* Get a printable representation of the contents */` |
|      290 | 1508 | `			if((pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|      222 | 1509 | `				MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|      113 | 1510 | `			}else{` |
|        - | 1511 | `				/* PHP format: string(N) "content" */` |
|       72 | 1512 | `				if( ShowType ){` |
|       54 | 1513 | `					SyBlobFormat(&(*pOut),"%u) \"",SyBlobLength(&pObj->sBlob));` |
|       25 | 1514 | `				}` |
|       72 | 1515 | `				if( SyBlobLength(pContents) > 0 ){` |
|       70 | 1516 | `					SyBlobAppend(&(*pOut),SyBlobData(pContents),SyBlobLength(pContents));` |
|       33 | 1517 | `				}` |
|       72 | 1518 | `				if( ShowType ){` |
|       54 | 1519 | `					SyBlobAppend(&(*pOut),"\"",sizeof(char));` |
|       25 | 1520 | `				}` |
|        - | 1521 | `			}` |
|        - | 1522 | `		}` |
|      450 | 1523 | `		if( ShowType ){` |
|        - | 1524 | `			/* Strings already emitted their own ')' as part of the` |
|        - | 1525 | `			 * "N) \"content\"" format above. */` |
|      402 | 1526 | `			if( (pObj->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_STRING)) == 0 ){` |
|      206 | 1527 | `				SyBlobAppend(&(*pOut),")",sizeof(char));` |
|      101 | 1528 | `			}` |
|      199 | 1529 | `		}` |
|      223 | 1530 | `	}` |
|        - | 1531 | `#ifdef __WINNT__` |
|        4 | 1532 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|        - | 1533 | `#else` |
|      448 | 1534 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        - | 1535 | `#endif` |
|      452 | 1536 | `	return rc;` |
|        4 | 1537 | `}` |
|        - | 1538 |  |
