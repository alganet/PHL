# src/ph7/memobj.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 724/811 lines (89.27%)

[Root index](../../index.md) | [Directory index](index.md)

|     Hits | Line | Source |
| -------: | ---: | :--- |
|        - |    1 | `/**` |
|        - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|        - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|        - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|        - |    5 | ` */` |
|        - |    6 | `#include "ph7int.h" /* This file handle low-level stuff related to indexed memory objects [i.e: ph7_value] */` |
|        - |    7 |  |
|        - |    8 | `/* Provide PHP-style type names for values.  This utility may be reused` |
|        - |    9 | ` * by any subsystem that works with ph7_value.` |
|        - |   10 | ` */` |
|      354 |   11 | `PH7_PRIVATE const char *ph7_type_name(ph7_value *pVal)` |
|        5 |   12 | `{` |
|      359 |   13 | `	if( ph7_value_is_null(pVal) ) return "null";` |
|      333 |   14 | `	if( ph7_value_is_bool(pVal) ) return "bool";` |
|      325 |   15 | `	if( ph7_value_is_int(pVal) ) return "int";` |
|      275 |   16 | `	if( ph7_value_is_float(pVal) ) return "float";` |
|      271 |   17 | `	if( ph7_value_is_string(pVal) ) return "string";` |
|      109 |   18 | `	if( ph7_value_is_array(pVal) ) return "array";` |
|       30 |   19 | `	if( ph7_value_is_object(pVal) ) return "object";` |
|        3 |   20 | `	if( ph7_value_is_resource(pVal) ) return "resource";` |
|      ! 0 |   21 | `	return "unknown";` |
|      182 |   22 | `}` |
|        - |   23 |  |
|        - |   24 | `/*` |
|        - |   25 | ` * Notes on memory objects [i.e: ph7_value].` |
|        - |   26 | ` * Internally, the PH7 virtual machine manipulates nearly all PHP values` |
|        - |   27 | ` * [i.e: string,int,float,resource,object,bool,null..] as ph7_values structures.` |
|        - |   28 | ` * Each ph7_values struct may cache multiple representations (string,` |
|        - |   29 | ` * integer etc.) of the same value.` |
|        - |   30 | ` */` |
|        - |   31 | `/*` |
|        - |   32 | ` * Convert a 64-bit IEEE double into a 64-bit signed integer.` |
|        - |   33 | ` * If the double is too large, return 0x8000000000000000.` |
|        - |   34 | ` *` |
|        - |   35 | ` * Most systems appear to do this simply by assigning ariables and without` |
|        - |   36 | ` * the extra range tests.` |
|        - |   37 | ` * But there are reports that windows throws an expection if the floating` |
|        - |   38 | ` * point value is out of range.` |
|        - |   39 | ` */` |
|     2654 |   40 | `static sxi64 MemObjRealToInt(ph7_value *pObj)` |
|        4 |   41 | `{` |
|        - |   42 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |   43 | `	/* Real and 64bit integer are the same when floating point arithmetic` |
|        - |   44 | `	 * is omitted from the build.` |
|        - |   45 | `	 */` |
|        - |   46 | `	return pObj->rVal;` |
|        - |   47 | `#else` |
|        - |   48 | ` /*` |
|        - |   49 | `  ** Many compilers we encounter do not define constants for the` |
|        - |   50 | `  ** minimum and maximum 64-bit integers, or they define them` |
|        - |   51 | `  ** inconsistently.  And many do not understand the "LL" notation.` |
|        - |   52 | `  ** So we define our own static constants here using nothing` |
|        - |   53 | `  ** larger than a 32-bit integer constant.` |
|        - |   54 | `  */` |
|        - |   55 | `  static const sxi64 maxInt = LARGEST_INT64;` |
|        - |   56 | `  static const sxi64 minInt = SMALLEST_INT64;` |
|     2658 |   57 | `  ph7_real r = pObj->rVal;` |
|     2658 |   58 | `  if( r<(ph7_real)minInt ){` |
|        3 |   59 | `    return minInt;` |
|     2656 |   60 | `  }else if( r>(ph7_real)maxInt ){` |
|        - |   61 | `    /* minInt is correct here - not maxInt.  It turns out that assigning` |
|        - |   62 | `    ** a very large positive number to an integer results in a very large` |
|        - |   63 | `    ** negative integer.  This makes no sense, but it is what x86 hardware` |
|        - |   64 | `    ** does so for compatibility we will do the same in software. */` |
|       71 |   65 | `    return minInt;` |
|      ! 0 |   66 | `  }else{` |
|     2586 |   67 | `    return (sxi64)r;` |
|        - |   68 | `  }` |
|        - |   69 | `#endif` |
|     1331 |   70 | `}` |
|        - |   71 | `/*` |
|        - |   72 | ` * Convert a raw token value typically a stream of digit [i.e: hex,octal,binary or decimal]` |
|        - |   73 | ` * to a 64-bit integer.` |
|        - |   74 | ` */` |
|   134816 |   75 | `PH7_PRIVATE sxi64 PH7_TokenValueToInt64(SyString *pVal)` |
|        5 |   76 | `{` |
|   134821 |   77 | `	sxi64 iVal = 0;` |
|   134821 |   78 | `	if( pVal->nByte <= 0 ){` |
|        7 |   79 | `		return 0;` |
|        - |   80 | `	}` |
|   134815 |   81 | `	if( pVal->zString[0] == '0' ){` |
|        - |   82 | `		sxi32 c;` |
|    55797 |   83 | `		if( pVal->nByte == sizeof(char) ){` |
|    55395 |   84 | `			return 0;` |
|        - |   85 | `		}` |
|      403 |   86 | `		c = pVal->zString[1];` |
|      403 |   87 | `		if( c  == 'x' \|\| c == 'X' ){` |
|        - |   88 | `			/* Hex digit stream */` |
|       69 |   89 | `			SyHexStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|      369 |   90 | `		}else if( c == 'b' \|\| c == 'B' ){` |
|        - |   91 | `			/* Binary digit stream */` |
|      277 |   92 | `			SyBinaryStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|      139 |   93 | `		}else{` |
|        - |   94 | `			/* Octal digit stream */` |
|       59 |   95 | `			SyOctalStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|        - |   96 | `		}` |
|      202 |   97 | `	}else{` |
|        - |   98 | `		/* Decimal digit stream */` |
|    79023 |   99 | `		SyStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|        - |  100 | `	}` |
|    79425 |  101 | `	return iVal;` |
|    67413 |  102 | `}` |
|        - |  103 | `/*` |
|        - |  104 | ` * Return some kind of 64-bit integer value which is the best we can` |
|        - |  105 | ` * do at representing the value that pObj describes as a string` |
|        - |  106 | ` * representation.` |
|        - |  107 | ` */` |
|      424 |  108 | `static sxi64 MemObjStringToInt(ph7_value *pObj)` |
|        5 |  109 | `{` |
|        - |  110 | `	SyString sVal;` |
|      429 |  111 | `	SyStringInitFromBuf(&sVal,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|      429 |  112 | `	return PH7_TokenValueToInt64(&sVal);` |
|        5 |  113 | `}` |
|        - |  114 | `/*` |
|        - |  115 | ` * Call a magic class method [i.e: __toString(),__toInt(),...]` |
|        - |  116 | ` * Return SXRET_OK if the magic method is available and have been` |
|        - |  117 | ` * successfully called. Any other return value indicates failure.` |
|        - |  118 | ` */` |
|       88 |  119 | `static sxi32 MemObjCallClassCastMethod(` |
|        - |  120 | `	ph7_vm *pVm,               /* VM that trigger the invocation */` |
|        - |  121 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object] */` |
|        - |  122 | `	const char *zMethod,       /* Magic method name [i.e: __toString] */` |
|        - |  123 | `	sxu32 nLen,                /* Method name length */` |
|        - |  124 | `	ph7_value *pResult         /* OUT: Store the return value of the magic method here */` |
|        - |  125 | `	)` |
|        5 |  126 | `{` |
|        - |  127 | `	ph7_class_method *pMethod;` |
|        - |  128 | `	/* Check if the method is available */` |
|       93 |  129 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,zMethod,nLen);` |
|       93 |  130 | `	if( pMethod == 0 ){` |
|        - |  131 | `		/* No such method */` |
|        6 |  132 | `		return SXERR_NOTFOUND;` |
|        - |  133 | `	}` |
|        - |  134 | `	/* Invoke the desired method */` |
|       89 |  135 | `	PH7_VmCallClassMethod(&(*pVm),&(*pThis),pMethod,&(*pResult),0,0);` |
|        - |  136 | `	/* Method successfully called,pResult should hold the return value */` |
|       89 |  137 | `	return SXRET_OK;` |
|       49 |  138 | `}` |
|        - |  139 | `/*` |
|        - |  140 | ` * Return some kind of integer value which is the best we can` |
|        - |  141 | ` * do at representing the value that pObj describes as an integer.` |
|        - |  142 | ` * If pObj is an integer, then the value is exact. If pObj is` |
|        - |  143 | ` * a floating-point then  the value returned is the integer part.` |
|        - |  144 | ` * If pObj is a string, then we make an attempt to convert it into` |
|        - |  145 | ` * a integer and return that.` |
|        - |  146 | ` * If pObj represents a NULL value, return 0.` |
|        - |  147 | ` */` |
|      492 |  148 | `static sxi64 MemObjIntValue(ph7_value *pObj)` |
|        5 |  149 | `{` |
|        - |  150 | `	sxi32 iFlags;` |
|      497 |  151 | `	iFlags = pObj->iFlags;` |
|      497 |  152 | `	if (iFlags & MEMOBJ_REAL ){` |
|       33 |  153 | `		return MemObjRealToInt(&(*pObj));` |
|      465 |  154 | `	}else if( iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      122 |  155 | `		return pObj->x.iVal;` |
|      345 |  156 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|      325 |  157 | `		return MemObjStringToInt(&(*pObj));` |
|       21 |  158 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|        9 |  159 | `		return 0;` |
|       13 |  160 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|        7 |  161 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|        7 |  162 | `		sxu32 n = pMap->nEntry;` |
|        7 |  163 | `		PH7_HashmapUnref(pMap);` |
|        - |  164 | `		/* Return total number of entries in the hashmap */` |
|        7 |  165 | `		return n;` |
|        7 |  166 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|        - |  167 | `		ph7_value sResult;` |
|        5 |  168 | `		sxi64 iVal = 1;` |
|        - |  169 | `		sxi32 rc;` |
|        - |  170 | `		/* Invoke the [__toInt()] magic method if available [note that this is a symisc extension]  */` |
|        5 |  171 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|        5 |  172 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|        - |  173 | `			"__toInt",sizeof("__toInt")-1,&sResult);` |
|        5 |  174 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_INT) ){` |
|        - |  175 | `			/* Extract method return value */` |
|        5 |  176 | `			iVal = sResult.x.iVal;` |
|        2 |  177 | `		}` |
|        5 |  178 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|        5 |  179 | `		PH7_MemObjRelease(&sResult);` |
|        5 |  180 | `		return iVal;` |
|        3 |  181 | `	}else if(iFlags & MEMOBJ_RES ){` |
|        3 |  182 | `		return pObj->x.pOther != 0;` |
|        - |  183 | `	}` |
|        - |  184 | `	/* CANT HAPPEN */` |
|      ! 0 |  185 | `	return 0;` |
|      251 |  186 | `}` |
|        - |  187 | `/*` |
|        - |  188 | ` * Return some kind of real value which is the best we can` |
|        - |  189 | ` * do at representing the value that pObj describes as a real.` |
|        - |  190 | ` * If pObj is a real, then the value is exact.If pObj is an` |
|        - |  191 | ` * integer then the integer  is promoted to real and that value` |
|        - |  192 | ` * is returned.` |
|        - |  193 | ` * If pObj is a string, then we make an attempt to convert it` |
|        - |  194 | ` * into a real and return that.` |
|        - |  195 | ` * If pObj represents a NULL value, return 0.0` |
|        - |  196 | ` */` |
|     1532 |  197 | `static ph7_real MemObjRealValue(ph7_value *pObj)` |
|        4 |  198 | `{` |
|        - |  199 | `	sxi32 iFlags;` |
|     1536 |  200 | `	iFlags = pObj->iFlags;` |
|     1536 |  201 | `	if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  202 | `		return pObj->rVal;` |
|     1536 |  203 | `	}else if (iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      728 |  204 | `		return (ph7_real)pObj->x.iVal;` |
|      810 |  205 | `	}else if (iFlags & MEMOBJ_STRING){` |
|        - |  206 | `		SyString sString;` |
|        - |  207 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  208 | `		ph7_real rVal = 0;` |
|        - |  209 | `#else` |
|      804 |  210 | `		ph7_real rVal = 0.0;` |
|        - |  211 | `#endif` |
|      804 |  212 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|      804 |  213 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|        - |  214 | `			/* Convert as much as we can */` |
|        - |  215 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  216 | `			rVal = MemObjStringToInt(&(*pObj));` |
|        - |  217 | `#else` |
|      804 |  218 | `			SyStrToReal(sString.zString,sString.nByte,(void *)&rVal,0);` |
|        - |  219 | `#endif` |
|      400 |  220 | `		}` |
|      804 |  221 | `		return rVal;` |
|        7 |  222 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|        - |  223 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  224 | `		return 0;` |
|        - |  225 | `#else` |
|      ! 0 |  226 | `		return 0.0;` |
|        - |  227 | `#endif` |
|        7 |  228 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|        - |  229 | `		/* Return the total number of entries in the hashmap */` |
|      ! 0 |  230 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      ! 0 |  231 | `		ph7_real n = (ph7_real)pMap->nEntry;` |
|      ! 0 |  232 | `		PH7_HashmapUnref(pMap);` |
|      ! 0 |  233 | `		return n;` |
|        7 |  234 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|        - |  235 | `		ph7_value sResult;` |
|        5 |  236 | `		ph7_real rVal = 1;` |
|        - |  237 | `		sxi32 rc;` |
|        - |  238 | `		/* Invoke the [__toFloat()] magic method if available [note that this is a symisc extension]  */` |
|        5 |  239 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|        5 |  240 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|        - |  241 | `			"__toFloat",sizeof("__toFloat")-1,&sResult);` |
|        5 |  242 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_REAL) ){` |
|        - |  243 | `			/* Extract method return value */` |
|        5 |  244 | `			rVal = sResult.rVal;` |
|        2 |  245 | `		}` |
|        5 |  246 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|        5 |  247 | `		PH7_MemObjRelease(&sResult);` |
|        5 |  248 | `		return rVal;` |
|        3 |  249 | `	}else if(iFlags & MEMOBJ_RES ){` |
|        3 |  250 | `		return (ph7_real)(pObj->x.pOther != 0);` |
|        - |  251 | `	}` |
|        - |  252 | `	/* NOT REACHED  */` |
|      ! 0 |  253 | `	return 0;` |
|      770 |  254 | `}` |
|        - |  255 | `/*` |
|        - |  256 | ` * Return the string representation of a given ph7_value.` |
|        - |  257 | ` * This function never fail and always return SXRET_OK.` |
|        - |  258 | ` */` |
|    56178 |  259 | `static sxi32 MemObjStringValue(SyBlob *pOut,ph7_value *pObj,sxu8 bStrictBool)` |
|        5 |  260 | `{` |
|    56183 |  261 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|        - |  262 | `		/* Handle special floating-point values first */` |
|      338 |  263 | `		if( PH7_IS_NAN(pObj->rVal) ){` |
|      ! 0 |  264 | `			SyBlobAppend(&(*pOut),"NAN",3);` |
|      338 |  265 | `		}else if( PH7_IS_INF(pObj->rVal) ){` |
|      ! 0 |  266 | `			if( pObj->rVal < 0.0 ){` |
|      ! 0 |  267 | `				SyBlobAppend(&(*pOut),"-INF",4);` |
|      ! 0 |  268 | `			}else{` |
|      ! 0 |  269 | `				SyBlobAppend(&(*pOut),"INF",3);` |
|        - |  270 | `			}` |
|      ! 0 |  271 | `		}else{` |
|      338 |  272 | `			SyBlobFormat(&(*pOut),"%.15g",pObj->rVal);` |
|        4 |  273 | `		}` |
|    56016 |  274 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|    55409 |  275 | `		SyBlobFormat(&(*pOut),"%qd",pObj->x.iVal);` |
|        - |  276 | `		/* %qd (BSD quad) is equivalent to %lld in the libc printf */` |
|    28147 |  277 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|      329 |  278 | `		if( bStrictBool ){` |
|        - |  279 | `			/* Actual string cast: true -> "1", false -> "" (like PHP) */` |
|      223 |  280 | `			if( pObj->x.iVal ){` |
|       21 |  281 | `				SyBlobAppend(&(*pOut),"1",sizeof("1")-1);` |
|        9 |  282 | `			}` |
|        - |  283 | `			/* false produces empty string, nothing to append */` |
|      114 |  284 | `		}else{` |
|        - |  285 | `			/* Display path (var_dump, print_r): show TRUE/FALSE */` |
|      109 |  286 | `			if( pObj->x.iVal ){` |
|       65 |  287 | `				SyBlobAppend(&(*pOut),"TRUE",sizeof("TRUE")-1);` |
|       34 |  288 | `			}else{` |
|       46 |  289 | `				SyBlobAppend(&(*pOut),"FALSE",sizeof("FALSE")-1);` |
|        - |  290 | `			}` |
|        5 |  291 | `		}` |
|      283 |  292 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|      ! 0 |  293 | `		SyBlobAppend(&(*pOut),"Array",sizeof("Array")-1);` |
|      ! 0 |  294 | `		PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
|      121 |  295 | `	}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|        - |  296 | `		ph7_value sResult;` |
|        - |  297 | `		sxi32 rc;` |
|        - |  298 | `		/* Invoke the __toString() method if available */` |
|       79 |  299 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|       79 |  300 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|        - |  301 | `			"__toString",sizeof("__toString")-1,&sResult);` |
|       79 |  302 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_STRING) && SyBlobLength(&sResult.sBlob) > 0){` |
|        - |  303 | `			/* Expand method return value */` |
|       75 |  304 | `			SyBlobDup(&sResult.sBlob,pOut);` |
|       40 |  305 | `		}else{` |
|        - |  306 | `			/* Expand "Object" as requested by the PHP language reference manual */` |
|        6 |  307 | `			SyBlobAppend(&(*pOut),"Object",sizeof("Object")-1);` |
|        - |  308 | `		}` |
|       79 |  309 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|       79 |  310 | `		PH7_MemObjRelease(&sResult);` |
|       82 |  311 | `	}else if(pObj->iFlags & MEMOBJ_RES ){` |
|        3 |  312 | `		SyBlobFormat(&(*pOut),"ResourceID_%#x",pObj->x.pOther);` |
|        1 |  313 | `	}` |
|    56183 |  314 | `	return SXRET_OK;` |
|        5 |  315 | `}` |
|        - |  316 | `/*` |
|        - |  317 | ` * Return some kind of boolean value which is the best we can do` |
|        - |  318 | ` * at representing the value that pObj describes as a boolean.` |
|        - |  319 | ` * When converting to boolean, the following values are considered FALSE:` |
|        - |  320 | ` * NULL` |
|        - |  321 | ` * the boolean FALSE itself.` |
|        - |  322 | ` * the integer 0 (zero).` |
|        - |  323 | ` * the real 0.0 (zero).` |
|        - |  324 | ` * the empty string,a stream of zero [i.e: "0","00","000",...] and the string` |
|        - |  325 | ` * "false".` |
|        - |  326 | ` * an array with zero elements.` |
|        - |  327 | ` */` |
|    13996 |  328 | `static sxi32 MemObjBooleanValue(ph7_value *pObj)` |
|        5 |  329 | `{` |
|        - |  330 | `	sxi32 iFlags;` |
|    14001 |  331 | `	iFlags = pObj->iFlags;` |
|    14001 |  332 | `	if (iFlags & MEMOBJ_REAL ){` |
|        - |  333 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  334 | `		return pObj->rVal ? 1 : 0;` |
|        - |  335 | `#else` |
|       12 |  336 | `		return pObj->rVal != 0.0 ? 1 : 0;` |
|        - |  337 | `#endif` |
|    13991 |  338 | `	}else if( iFlags & MEMOBJ_INT ){` |
|      125 |  339 | `		return pObj->x.iVal ? 1 : 0;` |
|    13871 |  340 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|        - |  341 | `		SyString sString;` |
|       65 |  342 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|       65 |  343 | `		if( sString.nByte == 0 ){` |
|        - |  344 | `			/* Empty string */` |
|       15 |  345 | `			return 0;` |
|       50 |  346 | `		}else if( (sString.nByte == sizeof("true") - 1 && SyStrnicmp(sString.zString,"true",sizeof("true")-1) == 0) \|\|` |
|       53 |  347 | `			(sString.nByte == sizeof("on") - 1 && SyStrnicmp(sString.zString,"on",sizeof("on")-1) == 0) \|\|` |
|       48 |  348 | `			(sString.nByte == sizeof("yes") - 1 && SyStrnicmp(sString.zString,"yes",sizeof("yes")-1) == 0) ){` |
|        5 |  349 | `				return 1;` |
|       48 |  350 | `		}else if( sString.nByte == sizeof("false") - 1 && SyStrnicmp(sString.zString,"false",sizeof("false")-1) == 0 ){` |
|      ! 0 |  351 | `			return 0;` |
|      ! 0 |  352 | `		}else{` |
|        - |  353 | `			const char *zIn,*zEnd;` |
|       48 |  354 | `			zIn = sString.zString;` |
|       48 |  355 | `			zEnd = &zIn[sString.nByte];` |
|       48 |  356 | `			while( zIn < zEnd && zIn[0] == '0' ){` |
|      ! 0 |  357 | `				zIn++;` |
|      ! 0 |  358 | `			}` |
|       48 |  359 | `			return zIn >= zEnd ? 0 : 1;` |
|      ! 0 |  360 | `		}` |
|    13809 |  361 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|    12771 |  362 | `		return 0;` |
|     1043 |  363 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|       20 |  364 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|       20 |  365 | `		sxu32 n = pMap->nEntry;` |
|       20 |  366 | `		PH7_HashmapUnref(pMap);` |
|       20 |  367 | `		return n > 0 ? TRUE : FALSE;` |
|     1025 |  368 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|        - |  369 | `		ph7_value sResult;` |
|        7 |  370 | `		sxi32 iVal = 1;` |
|        - |  371 | `		sxi32 rc;` |
|        - |  372 | `		/* Invoke the __toBool() method if available [note that this is a symisc extension]  */` |
|        7 |  373 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|        7 |  374 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|        - |  375 | `			"__toBool",sizeof("__toBool")-1,&sResult);` |
|        7 |  376 | `		if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL)) ){` |
|        - |  377 | `			/* Extract method return value */` |
|        5 |  378 | `			iVal = (sxi32)(sResult.x.iVal != 0); /* Stupid cc warning -W -Wall -O6 */` |
|        2 |  379 | `		}` |
|        7 |  380 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|        7 |  381 | `		PH7_MemObjRelease(&sResult);` |
|        7 |  382 | `		return iVal;` |
|     1019 |  383 | `	}else if(iFlags & MEMOBJ_RES ){` |
|     1019 |  384 | `		return pObj->x.pOther != 0;` |
|        - |  385 | `	}` |
|        - |  386 | `	/* NOT REACHED */` |
|      ! 0 |  387 | `	return 0;` |
|     7003 |  388 | `}` |
|        - |  389 | `/*` |
|        - |  390 | ` * If the ph7_value is of type real,try to make it an integer also.` |
|        - |  391 | ` */` |
|     2622 |  392 | `static sxi32 MemObjTryIntger(ph7_value *pObj)` |
|        4 |  393 | `{` |
|     2626 |  394 | `	pObj->x.iVal = MemObjRealToInt(&(*pObj));` |
|        - |  395 | `  /* Only mark the value as an integer if` |
|        - |  396 | `  **` |
|        - |  397 | `  **    (1) the round-trip conversion real->int->real is a no-op, and` |
|        - |  398 | `  **    (2) The integer is neither the largest nor the smallest` |
|        - |  399 | `  **        possible integer` |
|        - |  400 | `  **` |
|        - |  401 | `  ** The second and third terms in the following conditional enforces` |
|        - |  402 | `  ** the second condition under the assumption that addition overflow causes` |
|        - |  403 | `  ** values to wrap around.  On x86 hardware, the third term is always` |
|        - |  404 | `  ** true and could be omitted.  But we leave it in because other` |
|        - |  405 | `  ** architectures might behave differently.` |
|        - |  406 | `  */` |
|     2622 |  407 | `	if( pObj->rVal ==(ph7_real)pObj->x.iVal && pObj->x.iVal>SMALLEST_INT64` |
|     1443 |  408 | `      && pObj->x.iVal<LARGEST_INT64 ){` |
|     1441 |  409 | `		  pObj->iFlags \|= MEMOBJ_INT;` |
|      721 |  410 | `	}` |
|     2626 |  411 | `	return SXRET_OK;` |
|        4 |  412 | `}` |
|        - |  413 | `/*` |
|        - |  414 | ` * Convert a ph7_value to type integer.Invalidate any prior representations.` |
|        - |  415 | ` */` |
|   403688 |  416 | `PH7_PRIVATE sxi32 PH7_MemObjToInteger(ph7_value *pObj)` |
|        5 |  417 | `{` |
|   403693 |  418 | `	if( (pObj->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  419 | `		/* Preform the conversion */` |
|      497 |  420 | `		pObj->x.iVal = MemObjIntValue(&(*pObj));` |
|        - |  421 | `		/* Invalidate any prior representations */` |
|      497 |  422 | `		SyBlobRelease(&pObj->sBlob);` |
|      497 |  423 | `		MemObjSetType(pObj,MEMOBJ_INT);` |
|      246 |  424 | `	}` |
|   403693 |  425 | `	return SXRET_OK;` |
|        5 |  426 | `}` |
|        - |  427 | `/*` |
|        - |  428 | ` * Convert a ph7_value to type real (Try to get an integer representation also).` |
|        - |  429 | ` * Invalidate any prior representations` |
|        - |  430 | ` */` |
|     2290 |  431 | `PH7_PRIVATE sxi32 PH7_MemObjToReal(ph7_value *pObj)` |
|        4 |  432 | `{` |
|     2294 |  433 | `	if((pObj->iFlags & MEMOBJ_REAL) == 0 ){` |
|        - |  434 | `		/* Preform the conversion */` |
|     1536 |  435 | `		pObj->rVal = MemObjRealValue(&(*pObj));` |
|        - |  436 | `		/* Invalidate any prior representations */` |
|     1536 |  437 | `		SyBlobRelease(&pObj->sBlob);` |
|     1536 |  438 | `		MemObjSetType(pObj,MEMOBJ_REAL);` |
|        - |  439 | `		/* Try to get an integer representation */` |
|     1536 |  440 | `		MemObjTryIntger(&(*pObj));` |
|      766 |  441 | `	}` |
|     2294 |  442 | `	return SXRET_OK;` |
|        4 |  443 | `}` |
|        - |  444 | `/*` |
|        - |  445 | ` * Convert a ph7_value to type boolean.Invalidate any prior representations.` |
|        - |  446 | ` */` |
|    15412 |  447 | `PH7_PRIVATE sxi32 PH7_MemObjToBool(ph7_value *pObj)` |
|        5 |  448 | `{` |
|    15417 |  449 | `	if( (pObj->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        - |  450 | `		/* Preform the conversion */` |
|    14001 |  451 | `		pObj->x.iVal = MemObjBooleanValue(&(*pObj));` |
|        - |  452 | `		/* Invalidate any prior representations */` |
|    14001 |  453 | `		SyBlobRelease(&pObj->sBlob);` |
|    14001 |  454 | `		MemObjSetType(pObj,MEMOBJ_BOOL);` |
|     6998 |  455 | `	}` |
|    15417 |  456 | `	return SXRET_OK;` |
|        5 |  457 | `}` |
|        - |  458 | `/*` |
|        - |  459 | ` * Convert a ph7_value to type string.Prior representations are NOT invalidated.` |
|        - |  460 | ` */` |
|   808221 |  461 | `PH7_PRIVATE sxi32 PH7_MemObjToString(ph7_value *pObj)` |
|        5 |  462 | `{` |
|   808226 |  463 | `	sxi32 rc = SXRET_OK;` |
|   808226 |  464 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  465 | `		/* Perform the conversion */` |
|    55965 |  466 | `		SyBlobReset(&pObj->sBlob); /* Reset the internal buffer */` |
|    55965 |  467 | `		rc = MemObjStringValue(&pObj->sBlob,&(*pObj),TRUE);` |
|    55965 |  468 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|    27980 |  469 | `	}` |
|   808226 |  470 | `	return rc;` |
|        5 |  471 | `}` |
|        - |  472 | `/*` |
|        - |  473 | ` * Nullify a ph7_value.In other words invalidate any prior` |
|        - |  474 | ` * representation.` |
|        - |  475 | ` */` |
|      ! 0 |  476 | `PH7_PRIVATE sxi32 PH7_MemObjToNull(ph7_value *pObj)` |
|      ! 0 |  477 | `{` |
|      ! 0 |  478 | `	return PH7_MemObjRelease(pObj);` |
|      ! 0 |  479 | `}` |
|        - |  480 | `/*` |
|        - |  481 | ` * Convert a ph7_value to type array.Invalidate any prior representations.` |
|        - |  482 | `  * According to the PHP language reference manual.` |
|        - |  483 | `  *   For any of the types: integer, float, string, boolean converting a value` |
|        - |  484 | `  *   to an array results in an array with a single element with index zero` |
|        - |  485 | `  *   and the value of the scalar which was converted.` |
|        - |  486 | `  */` |
|      322 |  487 | `PH7_PRIVATE sxi32 PH7_MemObjToHashmap(ph7_value *pObj)` |
|        5 |  488 | `{` |
|      327 |  489 | `	if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  490 | `		ph7_hashmap *pMap;` |
|        - |  491 | `		/* Allocate a new hashmap instance */` |
|      209 |  492 | `		pMap = PH7_NewHashmap(pObj->pVm,0,0);` |
|      209 |  493 | `		if( pMap == 0 ){` |
|      ! 0 |  494 | `			return SXERR_MEM;` |
|        - |  495 | `		}` |
|      209 |  496 | `		if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|        - |  497 | `			/*` |
|        - |  498 | `			 * According to the PHP language reference manual.` |
|        - |  499 | `			 *   For any of the types: integer, float, string, boolean converting a value` |
|        - |  500 | `			 *   to an array results in an array with a single element with index zero` |
|        - |  501 | `			 *   and the value of the scalar which was converted.` |
|        - |  502 | `			 */` |
|       27 |  503 | `			if( pObj->iFlags & MEMOBJ_OBJ ){` |
|        - |  504 | `				/* Object cast */` |
|       13 |  505 | `				PH7_ClassInstanceToHashmap((ph7_class_instance *)pObj->x.pOther,pMap);` |
|        7 |  506 | `			}else{` |
|        - |  507 | `				/* Insert a single element */` |
|       15 |  508 | `				PH7_HashmapInsert(pMap,0/* Automatic index assign */,&(*pObj));` |
|        - |  509 | `			}` |
|       27 |  510 | `			SyBlobRelease(&pObj->sBlob);` |
|       13 |  511 | `		}` |
|        - |  512 | `		/* Invalidate any prior representation */` |
|      209 |  513 | `		PH7_MemObjRelease(pObj);` |
|      209 |  514 | `		MemObjSetType(pObj,MEMOBJ_HASHMAP);` |
|      209 |  515 | `		pObj->x.pOther = pMap;` |
|      102 |  516 | `	}` |
|      327 |  517 | `	return SXRET_OK;` |
|      166 |  518 | `}` |
|        - |  519 | `/* Per-entry callback for the array branch of the (object) cast: add one dynamic` |
|        - |  520 | ` * property to the target stdClass, named by the array key (rendered as a string,` |
|        - |  521 | ` * matching PHP) and holding a copy of the value. */` |
|        - |  522 | `struct VmObjCastData { ph7_vm *pVm; ph7_class_instance *pStd; };` |
|       38 |  523 | `static int VmArrayToObjectWalk(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  524 | `{` |
|       39 |  525 | `	struct VmObjCastData *pData = (struct VmObjCastData *)pUserData;` |
|        - |  526 | `	ph7_value *pSlot;` |
|        - |  527 | `	/* pKey and pValue are walk-owned temporaries (PH7_HashmapWalk passes pointers to` |
|        - |  528 | `	 * its own stack-local sKey/sValue, not slots inside pVm->aMemObj), so they survive` |
|        - |  529 | `	 * the slot reservation inside PH7_VmCreateDynamicAttr — no snapshot needed. pKey is` |
|        - |  530 | `	 * safe to coerce in place. */` |
|       39 |  531 | `	PH7_MemObjToString(pKey);` |
|       58 |  532 | `	pSlot = PH7_VmCreateDynamicAttr(pData->pVm,pData->pStd,` |
|       38 |  533 | `		(const char *)SyBlobData(&pKey->sBlob),(sxu32)SyBlobLength(&pKey->sBlob),0);` |
|       39 |  534 | `	if( pSlot ){` |
|       39 |  535 | `		PH7_MemObjStore(pValue,pSlot);` |
|       19 |  536 | `	}` |
|       39 |  537 | `	return SXRET_OK;` |
|        1 |  538 | `}` |
|        - |  539 | `/*` |
|        - |  540 | ` * Convert a ph7_value to type object, invalidating any prior representation.` |
|        - |  541 | ` * The new object is a (PHP-empty) stdClass populated with dynamic properties,` |
|        - |  542 | ` * matching PHP's (object) cast:` |
|        - |  543 | ` *   - array  -> one property per entry (key rendered as a string -> name).` |
|        - |  544 | ` *   - scalar -> a single property named "scalar".` |
|        - |  545 | ` *   - null   -> an empty stdClass (no properties).` |
|        - |  546 | ` *   - object -> returned unchanged (the MEMOBJ_OBJ guard below).` |
|        - |  547 | ` */` |
|       34 |  548 | `PH7_PRIVATE sxi32 PH7_MemObjToObject(ph7_value *pObj)` |
|        1 |  549 | `{` |
|       35 |  550 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  551 | `		ph7_class_instance *pStd;` |
|        - |  552 | `		ph7_class *pClass;` |
|        - |  553 | `		ph7_vm *pVm;` |
|        - |  554 | `		/* Point to the underlying VM + the stdClass */` |
|       35 |  555 | `		pVm = pObj->pVm;` |
|       52 |  556 | `		pClass = pVm->pStdClass ? pVm->pStdClass` |
|       17 |  557 | `			: PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,0,0);` |
|       35 |  558 | `		if( pClass == 0 ){` |
|        - |  559 | `			/* Can't happen,load null instead */` |
|      ! 0 |  560 | `			PH7_MemObjRelease(pObj);` |
|      ! 0 |  561 | `			return SXRET_OK;` |
|        - |  562 | `		}` |
|        - |  563 | `		/* Instanciate a new (empty) stdClass object */` |
|       35 |  564 | `		pStd = PH7_NewClassInstance(pVm,pClass);` |
|       35 |  565 | `		if( pStd == 0 ){` |
|        - |  566 | `			/* Out of memory */` |
|      ! 0 |  567 | `			PH7_MemObjRelease(pObj);` |
|      ! 0 |  568 | `			return SXRET_OK;` |
|        - |  569 | `		}` |
|       35 |  570 | `		pStd->iRef = 1;` |
|       35 |  571 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  572 | `			/* Array: one dynamic property per entry. */` |
|        - |  573 | `			struct VmObjCastData sData;` |
|       23 |  574 | `			sData.pVm = pVm;` |
|       23 |  575 | `			sData.pStd = pStd;` |
|       23 |  576 | `			ph7_array_walk(pObj,VmArrayToObjectWalk,&sData);` |
|       24 |  577 | `		}else if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  578 | `			/* Scalar (int/float/bool/string): a single "scalar" property. */` |
|       11 |  579 | `			ph7_value *pSlot = PH7_VmCreateDynamicAttr(pVm,pStd,"scalar",sizeof("scalar")-1,0);` |
|       11 |  580 | `			if( pSlot ){` |
|       11 |  581 | `				PH7_MemObjStore(pObj,pSlot);` |
|        5 |  582 | `			}` |
|        5 |  583 | `		}` |
|        - |  584 | `		/* (A NULL source yields an empty stdClass — nothing to populate.) */` |
|        - |  585 | `		/* Invalidate any prior representation */` |
|       35 |  586 | `		PH7_MemObjRelease(pObj);` |
|        - |  587 | `		/* Save the new instance */` |
|       35 |  588 | `		pObj->x.pOther = pStd;` |
|       35 |  589 | `		MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       17 |  590 | `	}` |
|       35 |  591 | `	return SXRET_OK;` |
|       18 |  592 | `}` |
|        - |  593 | `/*` |
|        - |  594 | ` * Return a pointer to the appropriate convertion method associated` |
|        - |  595 | ` * with the given type.` |
|        - |  596 | ` * Note on type juggling.` |
|        - |  597 | ` * Accoding to the PHP language reference manual` |
|        - |  598 | ` *  PHP does not require (or support) explicit type definition in variable` |
|        - |  599 | ` *  declaration; a variable's type is determined by the context in which` |
|        - |  600 | ` *  the variable is used. That is to say, if a string value is assigned` |
|        - |  601 | ` *  to variable $var, $var becomes a string. If an integer value is then` |
|        - |  602 | ` *  assigned to $var, it becomes an integer.` |
|        - |  603 | ` */` |
|       72 |  604 | `PH7_PRIVATE ProcMemObjCast PH7_MemObjCastMethod(sxi32 iFlags)` |
|        5 |  605 | `{` |
|       77 |  606 | `	if( iFlags & MEMOBJ_STRING ){` |
|       14 |  607 | `		return PH7_MemObjToString;` |
|       65 |  608 | `	}else if( iFlags & MEMOBJ_INT ){` |
|       49 |  609 | `		return PH7_MemObjToInteger;` |
|       19 |  610 | `	}else if( iFlags & MEMOBJ_REAL ){` |
|       16 |  611 | `		return PH7_MemObjToReal;` |
|        3 |  612 | `	}else if( iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  613 | `		return PH7_MemObjToBool;` |
|        3 |  614 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|        3 |  615 | `		return PH7_MemObjToHashmap;` |
|      ! 0 |  616 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  617 | `		return PH7_MemObjToObject;` |
|      ! 0 |  618 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|        - |  619 | ``		/* `null` is a type, not a weak-coercion target: never silently cast a`` |
|        - |  620 | ``		 * value to null for a standalone `null` type hint. Return/property`` |
|        - |  621 | `		 * enforcement reject a non-null value before reaching here; this guards` |
|        - |  622 | `		 * the parameter default-value path from quietly nulling a non-null` |
|        - |  623 | `		 * default. */` |
|      ! 0 |  624 | `		return 0;` |
|        - |  625 | `	}` |
|        - |  626 | `	/* NULL cast */` |
|      ! 0 |  627 | `	return PH7_MemObjToNull;` |
|       41 |  628 | `}` |
|        - |  629 | `/*` |
|        - |  630 | ` * Check whether the ph7_value is numeric [i.e: int/float/bool] or looks` |
|        - |  631 | ` * like a numeric number [i.e: if the ph7_value is of type string.].` |
|        - |  632 | ` * Return TRUE if numeric.FALSE otherwise.` |
|        - |  633 | ` */` |
|   454984 |  634 | `PH7_PRIVATE sxi32 PH7_MemObjIsNumeric(ph7_value *pObj)` |
|        5 |  635 | `{` |
|   454989 |  636 | `	if( pObj->iFlags & ( MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      293 |  637 | `		return TRUE;` |
|   454701 |  638 | `	}else if( pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|      175 |  639 | `		return FALSE;` |
|   454531 |  640 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        - |  641 | `		SyString sStr;` |
|        - |  642 | `		sxi32 rc;` |
|   454531 |  643 | `		SyStringInitFromBuf(&sStr,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|   454531 |  644 | `		if( sStr.nByte <= 0 ){` |
|        - |  645 | `			/* Empty string */` |
|       25 |  646 | `			return FALSE;` |
|        - |  647 | `		}` |
|        - |  648 | `		/* Check if the string representation looks like a numeric number */` |
|   454507 |  649 | `		rc = SyStrIsNumeric(sStr.zString,sStr.nByte,0,0);` |
|   454507 |  650 | `		return rc == SXRET_OK ? TRUE : FALSE;` |
|        - |  651 | `	}` |
|        - |  652 | `	/* NOT REACHED */` |
|      ! 0 |  653 | `	return FALSE;` |
|   227529 |  654 | `}` |
|        - |  655 | `/*` |
|        - |  656 | ` * Check whether the ph7_value is empty.Return TRUE if empty.` |
|        - |  657 | ` * FALSE otherwise.` |
|        - |  658 | ` * An ph7_value is considered empty if the following are true:` |
|        - |  659 | ` * NULL value.` |
|        - |  660 | ` * Boolean FALSE.` |
|        - |  661 | ` * Integer/Float with a 0 (zero) value.` |
|        - |  662 | ` * An empty string or a stream of 0 (zero) [i.e: "0","00","000",...].` |
|        - |  663 | ` * An empty array.` |
|        - |  664 | ` * NOTE` |
|        - |  665 | ` *  OBJECT VALUE MUST NOT BE MODIFIED.` |
|        - |  666 | ` */` |
|    33194 |  667 | `PH7_PRIVATE sxi32 PH7_MemObjIsEmpty(ph7_value *pObj)` |
|        5 |  668 | `{` |
|    33199 |  669 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|       19 |  670 | `		return TRUE;` |
|    33183 |  671 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|       20 |  672 | `		return pObj->x.iVal == 0 ? TRUE : FALSE;` |
|    33165 |  673 | `	}else if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  674 | `		return pObj->rVal == (ph7_real)0 ? TRUE : FALSE;` |
|    33165 |  675 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|        5 |  676 | `		return !pObj->x.iVal;` |
|    33161 |  677 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|    26929 |  678 | `		if( SyBlobLength(&pObj->sBlob) <= 0 ){` |
|    21605 |  679 | `			return TRUE;` |
|      ! 0 |  680 | `		}else{` |
|        - |  681 | `			const char *zIn,*zEnd;` |
|     5329 |  682 | `			zIn = (const char *)SyBlobData(&pObj->sBlob);` |
|     5329 |  683 | `			zEnd = &zIn[SyBlobLength(&pObj->sBlob)];` |
|     5335 |  684 | `			while( zIn < zEnd ){` |
|     5335 |  685 | `				if( zIn[0] != '0' ){` |
|     5329 |  686 | `					break;` |
|        - |  687 | `				}` |
|        7 |  688 | `				zIn++;` |
|        1 |  689 | `			}` |
|     5329 |  690 | `			return zIn >= zEnd ? TRUE : FALSE;` |
|      ! 0 |  691 | `		}` |
|     6237 |  692 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|     6237 |  693 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|     6237 |  694 | `		return pMap->nEntry == 0 ? TRUE : FALSE;` |
|      ! 0 |  695 | `	}else if ( pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|      ! 0 |  696 | `		return FALSE;` |
|        - |  697 | `	}` |
|        - |  698 | `	/* Assume empty by default */` |
|      ! 0 |  699 | `	return TRUE;` |
|    16602 |  700 | `}` |
|        - |  701 | `/*` |
|        - |  702 | ` * Convert a ph7_value so that it has types MEMOBJ_REAL or MEMOBJ_INT` |
|        - |  703 | ` * or both.` |
|        - |  704 | ` * Invalidate any prior representations. Every effort is made to force` |
|        - |  705 | ` * the conversion, even if the input is a string that does not look` |
|        - |  706 | ` * completely like a number.Convert as much of the string as we can` |
|        - |  707 | ` * and ignore the rest.` |
|        - |  708 | ` */` |
|   440795 |  709 | `PH7_PRIVATE sxi32 PH7_MemObjToNumeric(ph7_value *pObj)` |
|        5 |  710 | `{` |
|   440800 |  711 | `	if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|   440688 |  712 | `		if( pObj->iFlags & (MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|        3 |  713 | `			if( pObj->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  714 | `				pObj->x.iVal = 0;` |
|      ! 0 |  715 | `			}` |
|        3 |  716 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|        1 |  717 | `		}` |
|        - |  718 | `		/* Already numeric */` |
|   440688 |  719 | `		return  SXRET_OK;` |
|        - |  720 | `	}` |
|      113 |  721 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|      113 |  722 | `		sxi32 rc = SXERR_INVALID;` |
|      113 |  723 | `		sxu8 bReal = FALSE;` |
|        - |  724 | `		SyString sString;` |
|      113 |  725 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|        - |  726 | `		/* Check if the given string looks like a numeric number */` |
|      113 |  727 | `		if( sString.nByte > 0 ){` |
|      111 |  728 | `			rc = SyStrIsNumeric(sString.zString,sString.nByte,&bReal,0);` |
|       55 |  729 | `		}` |
|      113 |  730 | `		if( bReal ){` |
|        7 |  731 | `			PH7_MemObjToReal(&(*pObj));` |
|        4 |  732 | `		}else{` |
|      107 |  733 | `			if( rc != SXRET_OK ){` |
|        - |  734 | `				/* The input does not look at all like a number,set the value to 0 */` |
|        3 |  735 | `				pObj->x.iVal = 0;` |
|        2 |  736 | `			}else{` |
|        - |  737 | `				/* Convert as much as we can */` |
|      105 |  738 | `				pObj->x.iVal = MemObjStringToInt(&(*pObj));` |
|        - |  739 | `			}` |
|      107 |  740 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|      107 |  741 | `			SyBlobRelease(&pObj->sBlob);` |
|        1 |  742 | `		}` |
|       56 |  743 | `	}else if(pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)){` |
|      ! 0 |  744 | `		PH7_MemObjToInteger(pObj);` |
|      ! 0 |  745 | `	}else{` |
|        - |  746 | `		/* Perform a blind cast */` |
|      ! 0 |  747 | `		PH7_MemObjToReal(&(*pObj));` |
|        - |  748 | `	}` |
|      113 |  749 | `	return SXRET_OK;` |
|   220445 |  750 | `}` |
|        - |  751 | `/*` |
|        - |  752 | ` * Apply Perl-style increment to a string ph7_value in place.` |
|        - |  753 | ` * Walks the bytes right-to-left: digits 0-8 / letters a-y, A-Y bump in` |
|        - |  754 | ` * place; '9' wraps to '0' with carry; 'z' to 'a'; 'Z' to 'A'. A non-` |
|        - |  755 | ` * alphanumeric byte stops the walk without prepending. If carry survives` |
|        - |  756 | ` * past index 0, prepend '1', 'a', or 'A' depending on the class of the` |
|        - |  757 | ` * last carried character. Empty strings become "1".` |
|        - |  758 | ` *` |
|        - |  759 | ` * Caller must ensure pObj is MEMOBJ_STRING and not a numeric string;` |
|        - |  760 | ` * this routine never reclassifies the type, so a result like "e0" stays` |
|        - |  761 | ` * a string even though it looks numeric.` |
|        - |  762 | ` */` |
|       48 |  763 | `PH7_PRIVATE sxi32 PH7_MemObjStringIncrement(ph7_value *pObj)` |
|        1 |  764 | `{` |
|        - |  765 | `	enum CarryClass { CARRY_NONE = 0, CARRY_LOWER, CARRY_UPPER, CARRY_DIGIT };` |
|       49 |  766 | `	enum CarryClass last_class = CARRY_NONE;` |
|        - |  767 | `	sxu32 nLen, pos;` |
|        - |  768 | `	sxu8 *zStr;` |
|       49 |  769 | `	int carry = 1;` |
|        - |  770 | `	int ch;` |
|        - |  771 | `	/* Force ownership: the blob may be SXBLOB_RDONLY (e.g., from` |
|        - |  772 | `	 * PH7_MemObjLoad), in which case BlobPrepareGrow copies on demand` |
|        - |  773 | `	 * and clears the flag.  On an already-owned blob with spare capacity` |
|        - |  774 | `	 * (the common case under PHL's growth allocator), this is a no-op` |
|        - |  775 | `	 * append; on an exact-fit owned blob it triggers a single realloc. */` |
|       49 |  776 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       45 |  777 | `		SyBlobNullAppend(&pObj->sBlob);` |
|       22 |  778 | `	}` |
|       49 |  779 | `	nLen = SyBlobLength(&pObj->sBlob);` |
|       49 |  780 | `	if( nLen == 0 ){` |
|        5 |  781 | `		SyBlobAppend(&pObj->sBlob,"1",sizeof(char));` |
|        5 |  782 | `		return SXRET_OK;` |
|        - |  783 | `	}` |
|       45 |  784 | `	zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|       45 |  785 | `	pos = nLen;` |
|       97 |  786 | `	while( pos > 0 ){` |
|       79 |  787 | `		pos--;` |
|       79 |  788 | `		ch = zStr[pos];` |
|       79 |  789 | `		if( ch >= 'a' && ch <= 'z' ){` |
|       45 |  790 | `			if( ch == 'z' ){` |
|       29 |  791 | `				zStr[pos] = 'a';` |
|       29 |  792 | `				last_class = CARRY_LOWER;` |
|       29 |  793 | `				continue;` |
|        - |  794 | `			}` |
|       17 |  795 | `			zStr[pos]++;` |
|       17 |  796 | `			carry = 0;` |
|       17 |  797 | `			break;` |
|       35 |  798 | `		}else if( ch >= 'A' && ch <= 'Z' ){` |
|       21 |  799 | `			if( ch == 'Z' ){` |
|       19 |  800 | `				zStr[pos] = 'A';` |
|       19 |  801 | `				last_class = CARRY_UPPER;` |
|       19 |  802 | `				continue;` |
|        - |  803 | `			}` |
|        3 |  804 | `			zStr[pos]++;` |
|        3 |  805 | `			carry = 0;` |
|        3 |  806 | `			break;` |
|       15 |  807 | `		}else if( ch >= '0' && ch <= '9' ){` |
|        7 |  808 | `			if( ch == '9' ){` |
|        7 |  809 | `				zStr[pos] = '0';` |
|        7 |  810 | `				last_class = CARRY_DIGIT;` |
|        7 |  811 | `				continue;` |
|        - |  812 | `			}` |
|      ! 0 |  813 | `			zStr[pos]++;` |
|      ! 0 |  814 | `			carry = 0;` |
|      ! 0 |  815 | `			break;` |
|      ! 0 |  816 | `		}else{` |
|        - |  817 | `			/* non-alphanumeric: stop without prepending */` |
|        9 |  818 | `			carry = 0;` |
|        9 |  819 | `			break;` |
|        - |  820 | `		}` |
|      ! 0 |  821 | `	}` |
|       45 |  822 | `	if( carry ){` |
|        - |  823 | `		sxu8 prepend;` |
|        - |  824 | `		sxu32 i;` |
|       19 |  825 | `		switch( last_class ){` |
|        9 |  826 | `			case CARRY_LOWER: prepend = (sxu8)'a'; break;` |
|       11 |  827 | `			case CARRY_UPPER: prepend = (sxu8)'A'; break;` |
|      ! 0 |  828 | `			default:          prepend = (sxu8)'1'; break;` |
|        - |  829 | `		}` |
|        - |  830 | `		/* Append a sentinel byte to grow nByte by 1 (capacity grows too). */` |
|       19 |  831 | `		SyBlobAppend(&pObj->sBlob,"\0",sizeof(char));` |
|       19 |  832 | `		zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|       19 |  833 | `		nLen = SyBlobLength(&pObj->sBlob);` |
|        - |  834 | `		/* Shift right by 1, walking from the end so overlapping is safe. */` |
|       57 |  835 | `		for( i = nLen - 1; i > 0; i-- ){` |
|       39 |  836 | `			zStr[i] = zStr[i - 1];` |
|       20 |  837 | `		}` |
|       19 |  838 | `		zStr[0] = prepend;` |
|        9 |  839 | `	}` |
|       45 |  840 | `	return SXRET_OK;` |
|       25 |  841 | `}` |
|        - |  842 | `/*` |
|        - |  843 | ` * Try a get an integer representation of the given ph7_value.` |
|        - |  844 | ` * If the ph7_value is not of type real,this function is a no-op.` |
|        - |  845 | ` */` |
|     1030 |  846 | `PH7_PRIVATE sxi32 PH7_MemObjTryInteger(ph7_value *pObj)` |
|        1 |  847 | `{` |
|     1031 |  848 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|        - |  849 | `		/* Work only with reals */` |
|     1031 |  850 | `		MemObjTryIntger(&(*pObj));` |
|      515 |  851 | `	}` |
|     1031 |  852 | `	return SXRET_OK;` |
|        1 |  853 | `}` |
|        - |  854 | `/*` |
|        - |  855 | ` * Initialize a ph7_value to the null type.` |
|        - |  856 | ` */` |
|  8636599 |  857 | `PH7_PRIVATE sxi32 PH7_MemObjInit(ph7_vm *pVm,ph7_value *pObj)` |
|        5 |  858 | `{` |
|        - |  859 | `	/* Zero the structure */` |
|  8636604 |  860 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - |  861 | `	/* Initialize fields */` |
|  8636604 |  862 | `	pObj->pVm = pVm;` |
|  8636604 |  863 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - |  864 | `	/* Set the NULL type */` |
|  8636604 |  865 | `	pObj->iFlags = MEMOBJ_NULL;` |
|  8636604 |  866 | `	return SXRET_OK;` |
|        5 |  867 | `}` |
|        - |  868 | `/*` |
|        - |  869 | ` * Initialize a ph7_value to the integer type.` |
|        - |  870 | ` */` |
|   166794 |  871 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromInt(ph7_vm *pVm,ph7_value *pObj,sxi64 iVal)` |
|        5 |  872 | `{` |
|        - |  873 | `	/* Zero the structure */` |
|   166799 |  874 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - |  875 | `	/* Initialize fields */` |
|   166799 |  876 | `	pObj->pVm = pVm;` |
|   166799 |  877 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - |  878 | `	/* Set the desired type */` |
|   166799 |  879 | `	pObj->x.iVal = iVal;` |
|   166799 |  880 | `	pObj->iFlags = MEMOBJ_INT;` |
|   166799 |  881 | `	return SXRET_OK;` |
|        5 |  882 | `}` |
|        - |  883 | `/*` |
|        - |  884 | ` * Initialize a ph7_value to the boolean type.` |
|        - |  885 | ` */` |
|    17102 |  886 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromBool(ph7_vm *pVm,ph7_value *pObj,sxi32 iVal)` |
|        5 |  887 | `{` |
|        - |  888 | `	/* Zero the structure */` |
|    17107 |  889 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - |  890 | `	/* Initialize fields */` |
|    17107 |  891 | `	pObj->pVm = pVm;` |
|    17107 |  892 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - |  893 | `	/* Set the desired type */` |
|    17107 |  894 | `	pObj->x.iVal = iVal ? 1 : 0;` |
|    17107 |  895 | `	pObj->iFlags = MEMOBJ_BOOL;` |
|    17107 |  896 | `	return SXRET_OK;` |
|        5 |  897 | `}` |
|        - |  898 | `#if 0` |
|        - |  899 | `/*` |
|        - |  900 | ` * Initialize a ph7_value to the real type.` |
|        - |  901 | ` */` |
|        - |  902 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromReal(ph7_vm *pVm,ph7_value *pObj,ph7_real rVal)` |
|        - |  903 | `{` |
|        - |  904 | `	/* Zero the structure */` |
|        - |  905 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - |  906 | `	/* Initialize fields */` |
|        - |  907 | `	pObj->pVm = pVm;` |
|        - |  908 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - |  909 | `	/* Set the desired type */` |
|        - |  910 | `	pObj->rVal = rVal;` |
|        - |  911 | `	pObj->iFlags = MEMOBJ_REAL;` |
|        - |  912 | `	return SXRET_OK;` |
|        - |  913 | `}` |
|        - |  914 | `#endif` |
|        - |  915 | `/*` |
|        - |  916 | ` * Initialize a ph7_value to the array type.` |
|        - |  917 | ` */` |
|    50680 |  918 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromArray(ph7_vm *pVm,ph7_value *pObj,ph7_hashmap *pArray)` |
|        5 |  919 | `{` |
|        - |  920 | `	/* Zero the structure */` |
|    50685 |  921 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - |  922 | `	/* Initialize fields */` |
|    50685 |  923 | `	pObj->pVm = pVm;` |
|    50685 |  924 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - |  925 | `	/* Set the desired type */` |
|    50685 |  926 | `	pObj->iFlags = MEMOBJ_HASHMAP;` |
|    50685 |  927 | `	pObj->x.pOther = pArray;` |
|    50685 |  928 | `	return SXRET_OK;` |
|        5 |  929 | `}` |
|        - |  930 | `/*` |
|        - |  931 | ` * Initialize a ph7_value to the string type.` |
|        - |  932 | ` */` |
|   530974 |  933 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromString(ph7_vm *pVm,ph7_value *pObj,const SyString *pVal)` |
|        5 |  934 | `{` |
|        - |  935 | `	/* Zero the structure */` |
|   530979 |  936 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - |  937 | `	/* Initialize fields */` |
|   530979 |  938 | `	pObj->pVm = pVm;` |
|   530979 |  939 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|   530979 |  940 | `	if( pVal ){` |
|        - |  941 | `		/* Append contents */` |
|   328511 |  942 | `		SyBlobAppend(&pObj->sBlob,(const void *)pVal->zString,pVal->nByte);` |
|   164253 |  943 | `	}` |
|        - |  944 | `	/* Set the desired type */` |
|   530979 |  945 | `	pObj->iFlags = MEMOBJ_STRING;` |
|   530979 |  946 | `	return SXRET_OK;` |
|        5 |  947 | `}` |
|        - |  948 | `/*` |
|        - |  949 | ` * Append some contents to the internal buffer of a given ph7_value.` |
|        - |  950 | ` * If the given ph7_value is not of type string,this function` |
|        - |  951 | ` * invalidate any prior representation and set the string type.` |
|        - |  952 | ` * Then a simple append operation is performed.` |
|        - |  953 | ` */` |
|   385790 |  954 | `PH7_PRIVATE sxi32 PH7_MemObjStringAppend(ph7_value *pObj,const char *zData,sxu32 nLen)` |
|        5 |  955 | `{` |
|        - |  956 | `	sxi32 rc;` |
|   385795 |  957 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  958 | `		/* Invalidate any prior representation */` |
|     1839 |  959 | `		PH7_MemObjRelease(pObj);` |
|     1839 |  960 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|      917 |  961 | `	}` |
|        - |  962 | `	/* Append contents */` |
|   385795 |  963 | `	rc = SyBlobAppend(&pObj->sBlob,zData,nLen);` |
|   385795 |  964 | `	return rc;` |
|        5 |  965 | `}` |
|        - |  966 | `#if 0` |
|        - |  967 | `/*` |
|        - |  968 | ` * Format and append some contents to the internal buffer of a given ph7_value.` |
|        - |  969 | ` * If the given ph7_value is not of type string,this function invalidate` |
|        - |  970 | ` * any prior representation and set the string type.` |
|        - |  971 | ` * Then a simple format and append operation is performed.` |
|        - |  972 | ` */` |
|        - |  973 | `PH7_PRIVATE sxi32 PH7_MemObjStringFormat(ph7_value *pObj,const char *zFormat,va_list ap)` |
|        - |  974 | `{` |
|        - |  975 | `	sxi32 rc;` |
|        - |  976 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  977 | `		/* Invalidate any prior representation */` |
|        - |  978 | `		PH7_MemObjRelease(pObj);` |
|        - |  979 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|        - |  980 | `	}` |
|        - |  981 | `	/* Format and append contents */` |
|        - |  982 | `	rc = SyBlobFormatAp(&pObj->sBlob,zFormat,ap);` |
|        - |  983 | `	return rc;` |
|        - |  984 | `}` |
|        - |  985 | `#endif` |
|        - |  986 | `/*` |
|        - |  987 | ` * Duplicate the contents of a ph7_value.` |
|        - |  988 | ` */` |
|  4727273 |  989 | `PH7_PRIVATE sxi32 PH7_MemObjStore(ph7_value *pSrc,ph7_value *pDest)` |
|        5 |  990 | `{` |
|  4727278 |  991 | `	ph7_class_instance *pObj = 0;` |
|  4727278 |  992 | `	ph7_hashmap *pMap = 0;` |
|        - |  993 | `	sxi32 rc;` |
|  4727278 |  994 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  995 | `		/* Increment reference count */` |
|   176363 |  996 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  4639099 |  997 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|        - |  998 | `		/* Increment reference count */` |
|     3359 |  999 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|     1677 | 1000 | `	}` |
|  4727278 | 1001 | `	if( pDest->iFlags & MEMOBJ_HASHMAP ){` |
|    57887 | 1002 | `		pMap = (ph7_hashmap *)pDest->x.pOther;` |
|  4698337 | 1003 | `	}else if( pDest->iFlags & MEMOBJ_OBJ ){` |
|     4949 | 1004 | `		pObj = (ph7_class_instance *)pDest->x.pOther;` |
|     2472 | 1005 | `	}` |
|  4727278 | 1006 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|  4727278 | 1007 | `	pDest->iFlags &= ~MEMOBJ_AUX;` |
|  4727278 | 1008 | `	rc = SXRET_OK;` |
|  4727278 | 1009 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  3902087 | 1010 | `		SyBlobReset(&pDest->sBlob);` |
|  3902087 | 1011 | `		rc = SyBlobDup(&pSrc->sBlob,&pDest->sBlob);` |
|  1951046 | 1012 | `	}else{` |
|   825196 | 1013 | `		if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|   274732 | 1014 | `			SyBlobRelease(&pDest->sBlob);` |
|   137406 | 1015 | `		}` |
|        - | 1016 | `	}` |
|  4727278 | 1017 | `	if( pMap ){` |
|    57887 | 1018 | `		PH7_HashmapUnref(pMap);` |
|  4698337 | 1019 | `	}else if( pObj ){` |
|     4949 | 1020 | `		PH7_ClassInstanceUnref(pObj);` |
|     2472 | 1021 | `	}` |
|  4727278 | 1022 | `	return rc;` |
|        5 | 1023 | `}` |
|        - | 1024 | `/*` |
|        - | 1025 | ` * Duplicate the contents of a ph7_value but do not copy internal` |
|        - | 1026 | ` * buffer contents,simply point to it.` |
|        - | 1027 | ` */` |
|  6504550 | 1028 | `PH7_PRIVATE sxi32 PH7_MemObjLoad(ph7_value *pSrc,ph7_value *pDest)` |
|        5 | 1029 | `{` |
|  6504555 | 1030 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),` |
|        - | 1031 | `		sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|  6504555 | 1032 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1033 | `		/* Increment reference count */` |
|   459445 | 1034 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  6274835 | 1035 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|        - | 1036 | `		/* Increment reference count */` |
|    21559 | 1037 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|    10777 | 1038 | `	}` |
|  6504555 | 1039 | `	if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|       40 | 1040 | `		SyBlobRelease(&pDest->sBlob);` |
|       18 | 1041 | `	}` |
|  6504555 | 1042 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  3515409 | 1043 | `		SyBlobReadOnly(&pDest->sBlob,SyBlobData(&pSrc->sBlob),SyBlobLength(&pSrc->sBlob));` |
|  1757819 | 1044 | `	}` |
|  6504555 | 1045 | `	return SXRET_OK;` |
|        5 | 1046 | `}` |
|        - | 1047 | `/*` |
|        - | 1048 | ` * Invalidate any prior representation of a given ph7_value.` |
|        - | 1049 | ` */` |
| 13803879 | 1050 | `PH7_PRIVATE sxi32 PH7_MemObjRelease(ph7_value *pObj)` |
|        5 | 1051 | `{` |
| 13803884 | 1052 | `	if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
| 11753039 | 1053 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|   597373 | 1054 | `			PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
| 11454355 | 1055 | `		}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|    36839 | 1056 | `			PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|    18417 | 1057 | `		}` |
|        - | 1058 | `		/* Release the internal buffer */` |
| 11753039 | 1059 | `		SyBlobRelease(&pObj->sBlob);` |
|        - | 1060 | `		/* Invalidate any prior representation */` |
| 11753039 | 1061 | `		pObj->iFlags = MEMOBJ_NULL;` |
|  5876889 | 1062 | `	}` |
| 13803884 | 1063 | `	return SXRET_OK;` |
|        5 | 1064 | `}` |
|        - | 1065 | `/*` |
|        - | 1066 | ` * Compare two ph7_values.` |
|        - | 1067 | ` * Return 0 if the values are equals, > 0 if pObj1 is greater than pObj2` |
|        - | 1068 | ` * or < 0 if pObj2 is greater than pObj1.` |
|        - | 1069 | ` * Type comparison table taken from the PHP language reference manual.` |
|        - | 1070 | ` * Comparisons of $x with PHP functions Expression` |
|        - | 1071 | ` *              gettype() 	empty() 	is_null() 	isset() 	boolean : if($x)` |
|        - | 1072 | ` * $x = ""; 	string 	    TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1073 | ` * $x = null 	NULL 	    TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1074 | ` * var $x; 	    NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1075 | ` * $x is undefined 	NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1076 | ` *  $x = array(); 	array 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1077 | ` * $x = false; 	boolean 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1078 | ` * $x = true; 	boolean 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1079 | ` * $x = 1; 	    integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1080 | ` * $x = 42; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1081 | ` * $x = 0; 	    integer 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1082 | ` * $x = -1; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1083 | ` * $x = "1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1084 | ` * $x = "0"; 	string 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1085 | ` * $x = "-1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1086 | ` * $x = "php"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1087 | ` * $x = "true"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1088 | ` * $x = "false"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1089 | ` *      Loose comparisons with ==` |
|        - | 1090 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|        - | 1091 | ` * TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1092 | ` * FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|        - | 1093 | ` * 1 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1094 | ` * 0 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE` |
|        - | 1095 | ` * -1 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1096 | ` * "1" 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1097 | ` * "0" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1098 | ` * "-1" 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1099 | ` * NULL 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|        - | 1100 | ` * array() 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1101 | ` * "php" 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1102 | ` * "" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE` |
|        - | 1103 | ` *    Strict comparisons with ===` |
|        - | 1104 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|        - | 1105 | ` * TRUE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1106 | ` * FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1107 | ` * 1 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1108 | ` * 0 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1109 | ` * -1 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1110 | ` * "1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1111 | ` * "0" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1112 | ` * "-1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1113 | ` * NULL 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE` |
|        - | 1114 | ` * array() 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE` |
|        - | 1115 | ` * "php" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1116 | ` * "" 	    FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE` |
|        - | 1117 | ` */` |
|  1241217 | 1118 | `PH7_PRIVATE sxi32 PH7_MemObjCmp(ph7_value *pObj1,ph7_value *pObj2,int bStrict,int iNest)` |
|        5 | 1119 | `{` |
|        - | 1120 | `	sxi32 iComb;` |
|        - | 1121 | `	sxi32 rc;` |
|  1241222 | 1122 | `	if( bStrict ){` |
|        - | 1123 | `		sxi32 iF1,iF2;` |
|        - | 1124 | `		/* Strict comparisons with === */` |
|   640563 | 1125 | `		iF1 = pObj1->iFlags&~MEMOBJ_AUX;` |
|   640563 | 1126 | `		iF2 = pObj2->iFlags&~MEMOBJ_AUX;` |
|   640563 | 1127 | `		if( iF1 != iF2 ){` |
|        - | 1128 | `			/* Not of the same type */` |
|   179783 | 1129 | `			return 1;` |
|        - | 1130 | `		}` |
|   230390 | 1131 | `	}` |
|        - | 1132 | `	/* Combine flag together */` |
|  1061444 | 1133 | `	iComb = pObj1->iFlags\|pObj2->iFlags;` |
|  1061444 | 1134 | `	if( iComb & (MEMOBJ_NULL\|MEMOBJ_RES\|MEMOBJ_BOOL) ){` |
|        - | 1135 | `		/* Convert to boolean: Keep in mind FALSE < TRUE */` |
|    20697 | 1136 | `		if( (pObj1->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     7351 | 1137 | `			PH7_MemObjToBool(pObj1);` |
|     3673 | 1138 | `		}` |
|    20697 | 1139 | `		if( (pObj2->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     6385 | 1140 | `			PH7_MemObjToBool(pObj2);` |
|     3190 | 1141 | `		}` |
|    20697 | 1142 | `		return (sxi32)((pObj1->x.iVal != 0) - (pObj2->x.iVal != 0));` |
|  1040752 | 1143 | `	}else if ( iComb & MEMOBJ_HASHMAP ){` |
|        - | 1144 | `		/* Hashmap aka 'array' comparison */` |
|       29 | 1145 | `		if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1146 | `			/* Array is always greater */` |
|      ! 0 | 1147 | `			return -1;` |
|        - | 1148 | `		}` |
|       29 | 1149 | `		if( (pObj2->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1150 | `			/* Array is always greater */` |
|      ! 0 | 1151 | `			return 1;` |
|        - | 1152 | `		}` |
|        - | 1153 | `		/* Perform the comparison */` |
|       29 | 1154 | `		rc = PH7_HashmapCmp((ph7_hashmap *)pObj1->x.pOther,(ph7_hashmap *)pObj2->x.pOther,bStrict);` |
|       29 | 1155 | `		return rc;` |
|  1040724 | 1156 | `	}else if(iComb & MEMOBJ_OBJ ){` |
|        - | 1157 | `		/* Object comparison */` |
|      229 | 1158 | `		if( (pObj1->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 1159 | `			/* Object is always greater */` |
|      ! 0 | 1160 | `			return -1;` |
|        - | 1161 | `		}` |
|      229 | 1162 | `		if( (pObj2->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 1163 | `			/* Object is always greater */` |
|      ! 0 | 1164 | `			return 1;` |
|        - | 1165 | `		}` |
|        - | 1166 | `		/* Perform the comparison */` |
|      229 | 1167 | `		rc = PH7_ClassInstanceCmp((ph7_class_instance *)pObj1->x.pOther,(ph7_class_instance *)pObj2->x.pOther,bStrict,iNest);` |
|      229 | 1168 | `		return rc;` |
|  1040500 | 1169 | `	}else if ( iComb & MEMOBJ_STRING ){` |
|        - | 1170 | `		SyString s1,s2;` |
|   666653 | 1171 | `		if( !bStrict ){` |
|        - | 1172 | `			/*` |
|        - | 1173 | `			 * According to the PHP language reference manual:` |
|        - | 1174 | `			 *` |
|        - | 1175 | `			 *  If you compare a number with a string or the comparison involves numerical` |
|        - | 1176 | `			 *  strings, then each string is converted to a number and the comparison` |
|        - | 1177 | `			 *  performed numerically.` |
|        - | 1178 | `			 */` |
|   227215 | 1179 | `			if( PH7_MemObjIsNumeric(pObj1) ){` |
|        - | 1180 | `				/* Perform a numeric comparison */` |
|       17 | 1181 | `				goto Numeric;` |
|        - | 1182 | `			}` |
|   227199 | 1183 | `			if( PH7_MemObjIsNumeric(pObj2) ){` |
|        - | 1184 | `				/* Perform a numeric comparison */` |
|      ! 0 | 1185 | `				goto Numeric;` |
|        - | 1186 | `			}` |
|   113613 | 1187 | `		}` |
|        - | 1188 | `		/* Perform a strict string comparison.*/` |
|   666637 | 1189 | `		if( (pObj1->iFlags&MEMOBJ_STRING) == 0 ){` |
|      ! 0 | 1190 | `			PH7_MemObjToString(pObj1);` |
|      ! 0 | 1191 | `		}` |
|   666637 | 1192 | `		if( (pObj2->iFlags&MEMOBJ_STRING) == 0 ){` |
|      ! 0 | 1193 | `			PH7_MemObjToString(pObj2);` |
|      ! 0 | 1194 | `		}` |
|   666637 | 1195 | `		SyStringInitFromBuf(&s1,SyBlobData(&pObj1->sBlob),SyBlobLength(&pObj1->sBlob));` |
|   666637 | 1196 | `		SyStringInitFromBuf(&s2,SyBlobData(&pObj2->sBlob),SyBlobLength(&pObj2->sBlob));` |
|        - | 1197 | `		/*` |
|        - | 1198 | `		 * Strings are compared using memcmp(). If one value is an exact prefix of the` |
|        - | 1199 | `		 * other, then the shorter value is less than the longer value.` |
|        - | 1200 | `		 */` |
|   666637 | 1201 | `		rc = SyMemcmp((const void *)s1.zString,(const void *)s2.zString,SXMIN(s1.nByte,s2.nByte));` |
|   666637 | 1202 | `		if( rc == 0 ){` |
|   229552 | 1203 | `			if( s1.nByte != s2.nByte ){` |
|     1656 | 1204 | `				rc = s1.nByte < s2.nByte ? -1 : 1;` |
|      828 | 1205 | `			}` |
|   114776 | 1206 | `		}` |
|   666637 | 1207 | `		return rc;` |
|   373852 | 1208 | `	}else if( iComb & (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|   186881 | 1209 | `Numeric:` |
|        - | 1210 | `		/* Perform a numeric comparison if one of the operand is numeric(integer or real) */` |
|   373868 | 1211 | `		if( (pObj1->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|        5 | 1212 | `			PH7_MemObjToNumeric(pObj1);` |
|        2 | 1213 | `		}` |
|   373868 | 1214 | `		if( (pObj2->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|       15 | 1215 | `			PH7_MemObjToNumeric(pObj2);` |
|        7 | 1216 | `		}` |
|   373868 | 1217 | `		if( (pObj1->iFlags & pObj2->iFlags & MEMOBJ_INT) == 0) {` |
|        - | 1218 | `			/*` |
|        - | 1219 | `			 * Symisc eXtension to the PHP language:` |
|        - | 1220 | `			 *  Floating point comparison is introduced and works as expected.` |
|        - | 1221 | `			 */` |
|        - | 1222 | `			ph7_real r1,r2;` |
|        - | 1223 | `			/* Compare as reals */` |
|      225 | 1224 | `			if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|       11 | 1225 | `				PH7_MemObjToReal(pObj1);` |
|        5 | 1226 | `			}` |
|      225 | 1227 | `			r1 = pObj1->rVal;` |
|      225 | 1228 | `			if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|       37 | 1229 | `				PH7_MemObjToReal(pObj2);` |
|       18 | 1230 | `			}` |
|      225 | 1231 | `			r2 = pObj2->rVal;` |
|      225 | 1232 | `			if( PH7_IS_NAN(r1) \|\| PH7_IS_NAN(r2) ){` |
|        - | 1233 | `				/*` |
|        - | 1234 | `				 * Keep a strict three-way comparator contract even for NaN values.` |
|        - | 1235 | `				 * For ordering purposes, NaN compares equal to NaN and greater than` |
|        - | 1236 | `				 * any non-NaN numeric value.` |
|        - | 1237 | `				 */` |
|       45 | 1238 | `				if( PH7_IS_NAN(r1) ){` |
|       35 | 1239 | `					return PH7_IS_NAN(r2) ? 0 : 1;` |
|        - | 1240 | `				}` |
|       11 | 1241 | `				return -1;` |
|        - | 1242 | `			}` |
|      181 | 1243 | `			if( r1 > r2 ){` |
|       31 | 1244 | `				return 1;` |
|      151 | 1245 | `			}else if( r1 < r2 ){` |
|      119 | 1246 | `				return -1;` |
|        - | 1247 | `			}` |
|       33 | 1248 | `			return 0;` |
|      ! 0 | 1249 | `		}else{` |
|        - | 1250 | `			/* Integer comparison */` |
|   373644 | 1251 | `			if( pObj1->x.iVal > pObj2->x.iVal ){` |
|     5270 | 1252 | `				return 1;` |
|   368379 | 1253 | `			}else if( pObj1->x.iVal < pObj2->x.iVal ){` |
|   363008 | 1254 | `				return -1;` |
|        - | 1255 | `			}` |
|     5376 | 1256 | `			return 0;` |
|        - | 1257 | `		}` |
|        - | 1258 | `	}` |
|        - | 1259 | `	/* NOT REACHED */` |
|      ! 0 | 1260 | `	return 0;` |
|   620672 | 1261 | `}` |
|        - | 1262 | `/*` |
|        - | 1263 | ` * Perform an addition operation of two ph7_values.` |
|        - | 1264 | ` * The reason this function is implemented here rather than 'vm.c'` |
|        - | 1265 | ` * is that the '+' operator is overloaded.` |
|        - | 1266 | ` * That is,the '+' operator is used for arithmetic operation and also` |
|        - | 1267 | ` * used for operation on arrays [i.e: union]. When used with an array` |
|        - | 1268 | ` * The + operator returns the right-hand array appended to the left-hand array.` |
|        - | 1269 | ` * For keys that exist in both arrays, the elements from the left-hand array` |
|        - | 1270 | ` * will be used, and the matching elements from the right-hand array will` |
|        - | 1271 | ` * be ignored.` |
|        - | 1272 | ` * This function take care of handling all the scenarios.` |
|        - | 1273 | ` */` |
|     9680 | 1274 | `PH7_PRIVATE sxi32 PH7_MemObjAdd(ph7_value *pObj1,ph7_value *pObj2,int bAddStore)` |
|        5 | 1275 | `{` |
|     9685 | 1276 | `	if( ((pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1277 | `			/* Arithemtic operation */` |
|     5887 | 1278 | `			PH7_MemObjToNumeric(pObj1);` |
|     5887 | 1279 | `			PH7_MemObjToNumeric(pObj2);` |
|     5887 | 1280 | `			if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_REAL ){` |
|        - | 1281 | `				/* Floating point arithmetic */` |
|        - | 1282 | `				ph7_real a,b;` |
|       61 | 1283 | `				if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|       31 | 1284 | `					PH7_MemObjToReal(pObj1);` |
|       15 | 1285 | `				}` |
|       61 | 1286 | `				if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 | 1287 | `					PH7_MemObjToReal(pObj2);` |
|        1 | 1288 | `				}` |
|       61 | 1289 | `				a = pObj1->rVal;` |
|       61 | 1290 | `				b = pObj2->rVal;` |
|       61 | 1291 | `				pObj1->rVal = a+b;` |
|       61 | 1292 | `				MemObjSetType(pObj1,MEMOBJ_REAL);` |
|        - | 1293 | `				/* Try to get an integer representation also */` |
|       61 | 1294 | `				MemObjTryIntger(&(*pObj1));` |
|       31 | 1295 | `			}else{` |
|        - | 1296 | `				/* Integer arithmetic */` |
|        - | 1297 | `				sxi64 a,b;` |
|     5827 | 1298 | `				a = pObj1->x.iVal;` |
|     5827 | 1299 | `				b = pObj2->x.iVal;` |
|     5827 | 1300 | `				pObj1->x.iVal = a+b;` |
|     5827 | 1301 | `				MemObjSetType(pObj1,MEMOBJ_INT);` |
|        - | 1302 | `			}` |
|     2946 | 1303 | `	}else{` |
|     3803 | 1304 | `		if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP ){` |
|        - | 1305 | `			ph7_hashmap *pMap;` |
|        - | 1306 | `			sxi32 rc;` |
|     3803 | 1307 | `			if( bAddStore ){` |
|        - | 1308 | `				/* Do not duplicate the hashmap,use the left one since its an add&store operation.` |
|        - | 1309 | `				 */` |
|        3 | 1310 | `				if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1311 | `					/* Force a hashmap cast */` |
|      ! 0 | 1312 | `					rc = PH7_MemObjToHashmap(pObj1);` |
|      ! 0 | 1313 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 1314 | `						PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|      ! 0 | 1315 | `						return rc;` |
|        - | 1316 | `					}` |
|      ! 0 | 1317 | `				}` |
|        - | 1318 | `				/* COW separate before in-place mutation */` |
|        3 | 1319 | `				pMap = PH7_HashmapCowSeparate(pObj1->pVm,pObj1);` |
|        2 | 1320 | `			}else{` |
|        - | 1321 | `				/* Create a new hashmap */` |
|     3801 | 1322 | `				pMap = PH7_NewHashmap(pObj1->pVm,0,0);` |
|     3801 | 1323 | `				if( pMap == 0){` |
|      ! 0 | 1324 | `					PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|      ! 0 | 1325 | `					return SXERR_MEM;` |
|        - | 1326 | `				}` |
|        - | 1327 | `			}` |
|     3803 | 1328 | `			if( !bAddStore ){` |
|     3801 | 1329 | `				if(pObj1->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1330 | `					/* Perform a hashmap duplication */` |
|     3801 | 1331 | `					PH7_HashmapDup((ph7_hashmap *)pObj1->x.pOther,pMap);` |
|     1903 | 1332 | `				}else{` |
|      ! 0 | 1333 | `					if((pObj1->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 1334 | `						/* Simple insertion */` |
|      ! 0 | 1335 | `						PH7_HashmapInsert(pMap,0,pObj1);` |
|      ! 0 | 1336 | `					}` |
|        - | 1337 | `				}` |
|     1898 | 1338 | `			}` |
|        - | 1339 | `			/* Perform the union */` |
|     3803 | 1340 | `			if(pObj2->iFlags & MEMOBJ_HASHMAP ){` |
|     3803 | 1341 | `				PH7_HashmapUnion(pMap,(ph7_hashmap *)pObj2->x.pOther);` |
|     1904 | 1342 | `			}else{` |
|      ! 0 | 1343 | `				if((pObj2->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 1344 | `					/* Simple insertion */` |
|      ! 0 | 1345 | `					PH7_HashmapInsert(pMap,0,pObj2);` |
|      ! 0 | 1346 | `				}` |
|        - | 1347 | `			}` |
|        - | 1348 | `			/* Reflect the change */` |
|     3803 | 1349 | `			if( pObj1->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 1350 | `				SyBlobRelease(&pObj1->sBlob);` |
|      ! 0 | 1351 | `			}` |
|     3803 | 1352 | `			pObj1->x.pOther = pMap;` |
|     3803 | 1353 | `			MemObjSetType(pObj1,MEMOBJ_HASHMAP);` |
|     1899 | 1354 | `		}` |
|        - | 1355 | `	}` |
|     9685 | 1356 | `	return SXRET_OK;` |
|     4845 | 1357 | `}` |
|        - | 1358 | `/*` |
|        - | 1359 | ` * Return a printable representation of the type of a given` |
|        - | 1360 | ` * ph7_value.` |
|        - | 1361 | ` */` |
|      436 | 1362 | `PH7_PRIVATE const char * PH7_MemObjTypeDump(ph7_value *pVal)` |
|        5 | 1363 | `{` |
|      441 | 1364 | `	const char *zType = "";` |
|      441 | 1365 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|        5 | 1366 | `		zType = "null";` |
|      439 | 1367 | `	}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|        - | 1368 | `		/* REAL is authoritative over a cached MEMOBJ_INT: an integer-valued` |
|        - | 1369 | `		 * real (e.g. 1.0) is reported as "double", matching PHP's gettype(). */` |
|        7 | 1370 | `		zType = "double";` |
|      434 | 1371 | `	}else if( pVal->iFlags & MEMOBJ_INT ){` |
|       99 | 1372 | `		zType = "int";` |
|      383 | 1373 | `	}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|       77 | 1374 | `		zType = "string";` |
|      298 | 1375 | `	}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|      109 | 1376 | `		zType = "bool";` |
|      209 | 1377 | `	}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|       16 | 1378 | `		zType = "array";` |
|      148 | 1379 | `	}else if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      141 | 1380 | `		zType = "object";` |
|       69 | 1381 | `	}else if( pVal->iFlags & MEMOBJ_RES ){` |
|      ! 0 | 1382 | `		zType = "resource";` |
|      ! 0 | 1383 | `	}` |
|      441 | 1384 | `	return zType;` |
|        5 | 1385 | `}` |
|        - | 1386 | `/*` |
|        - | 1387 | ` * Dump a ph7_value [i.e: get a printable representation of it's type and contents.].` |
|        - | 1388 | ` * Store the dump in the given blob.` |
|        - | 1389 | ` */` |
|      448 | 1390 | `PH7_PRIVATE sxi32 PH7_MemObjDump(` |
|        - | 1391 | `	SyBlob *pOut,      /* Store the dump here */` |
|        - | 1392 | `	ph7_value *pObj,   /* Dump this */` |
|        - | 1393 | `	int ShowType,      /* TRUE to output value type */` |
|        - | 1394 | `	int nTab,          /* # of Whitespace to insert */` |
|        - | 1395 | `	int nDepth,        /* Nesting level */` |
|        - | 1396 | `	int isRef          /* TRUE if referenced object */` |
|        - | 1397 | `	)` |
|        4 | 1398 | `{` |
|      452 | 1399 | `	sxi32 rc = SXRET_OK;` |
|        - | 1400 | `	const char *zType;` |
|        - | 1401 | `	int i;` |
|     4568 | 1402 | `	for( i = 0 ; i < nTab ; i++ ){` |
|     4120 | 1403 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     2062 | 1404 | `	}` |
|      452 | 1405 | `	if( ShowType ){` |
|      404 | 1406 | `		if( isRef ){` |
|      ! 0 | 1407 | `			SyBlobAppend(&(*pOut),"&",sizeof(char));` |
|      ! 0 | 1408 | `		}` |
|        - | 1409 | `		/* Get value type first. var_dump() labels reals "float" (PHP), whereas` |
|        - | 1410 | `		 * gettype()/PH7_MemObjTypeDump use the legacy "double" spelling. */` |
|      404 | 1411 | `		if( (pObj->iFlags & MEMOBJ_REAL) && (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        3 | 1412 | `			zType = "float";` |
|        2 | 1413 | `		}else{` |
|      402 | 1414 | `			zType = PH7_MemObjTypeDump(pObj);` |
|        - | 1415 | `		}` |
|      404 | 1416 | `		SyBlobAppend(&(*pOut),zType,SyStrlen(zType));` |
|      200 | 1417 | `	}` |
|      452 | 1418 | `	if((pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|      450 | 1419 | `		if ( ShowType ){` |
|      402 | 1420 | `			SyBlobAppend(&(*pOut),"(",sizeof(char));` |
|      199 | 1421 | `		}` |
|      450 | 1422 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1423 | `			/* Dump hashmap entries */` |
|       24 | 1424 | `			rc = PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,ShowType,nTab+1,nDepth+1);` |
|      439 | 1425 | `		}else if(pObj->iFlags & MEMOBJ_OBJ ){` |
|        - | 1426 | `			/* Dump class instance attributes */` |
|      141 | 1427 | `			rc = PH7_ClassInstanceDump(&(*pOut),(ph7_class_instance *)pObj->x.pOther,ShowType,nTab+1,nDepth+1);` |
|       72 | 1428 | `		}else{` |
|      290 | 1429 | `			SyBlob *pContents = &pObj->sBlob;` |
|        - | 1430 | `			/* Get a printable representation of the contents */` |
|      290 | 1431 | `			if((pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|      222 | 1432 | `				MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|      113 | 1433 | `			}else{` |
|        - | 1434 | `				/* PHP format: string(N) "content" */` |
|       72 | 1435 | `				if( ShowType ){` |
|       54 | 1436 | `					SyBlobFormat(&(*pOut),"%u) \"",SyBlobLength(&pObj->sBlob));` |
|       25 | 1437 | `				}` |
|       72 | 1438 | `				if( SyBlobLength(pContents) > 0 ){` |
|       70 | 1439 | `					SyBlobAppend(&(*pOut),SyBlobData(pContents),SyBlobLength(pContents));` |
|       33 | 1440 | `				}` |
|       72 | 1441 | `				if( ShowType ){` |
|       54 | 1442 | `					SyBlobAppend(&(*pOut),"\"",sizeof(char));` |
|       25 | 1443 | `				}` |
|        - | 1444 | `			}` |
|        - | 1445 | `		}` |
|      450 | 1446 | `		if( ShowType ){` |
|        - | 1447 | `			/* Strings already emitted their own ')' as part of the` |
|        - | 1448 | `			 * "N) \"content\"" format above. */` |
|      402 | 1449 | `			if( (pObj->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_STRING)) == 0 ){` |
|      206 | 1450 | `				SyBlobAppend(&(*pOut),")",sizeof(char));` |
|      101 | 1451 | `			}` |
|      199 | 1452 | `		}` |
|      223 | 1453 | `	}` |
|        - | 1454 | `#ifdef __WINNT__` |
|        4 | 1455 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|        - | 1456 | `#else` |
|      448 | 1457 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        - | 1458 | `#endif` |
|      452 | 1459 | `	return rc;` |
|        4 | 1460 | `}` |
|        - | 1461 |  |
