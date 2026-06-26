# src/ph7/memobj.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 706/796 lines (88.69%)

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
|      298 |   11 | `PH7_PRIVATE const char *ph7_type_name(ph7_value *pVal)` |
|        5 |   12 |  |
|      303 |   13 | `	if( ph7_value_is_null(pVal) ) return "null";` |
|      291 |   14 | `	if( ph7_value_is_bool(pVal) ) return "bool";` |
|      283 |   15 | `	if( ph7_value_is_int(pVal) ) return "int";` |
|      235 |   16 | `	if( ph7_value_is_float(pVal) ) return "float";` |
|      231 |   17 | `	if( ph7_value_is_string(pVal) ) return "string";` |
|      101 |   18 | `	if( ph7_value_is_array(pVal) ) return "array";` |
|       30 |   19 | `	if( ph7_value_is_object(pVal) ) return "object";` |
|        3 |   20 | `	if( ph7_value_is_resource(pVal) ) return "resource";` |
|      ! 0 |   21 | `	return "unknown";` |
|      154 |   22 |  |
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
|     2228 |   40 | `static sxi64 MemObjRealToInt(ph7_value *pObj)` |
|        4 |   41 |  |
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
|     2232 |   57 | `  ph7_real r = pObj->rVal;` |
|     2232 |   58 | `  if( r<(ph7_real)minInt ){` |
|      ! 0 |   59 | `    return minInt;` |
|     2232 |   60 | `  }else if( r>(ph7_real)maxInt ){` |
|        - |   61 | `    /* minInt is correct here - not maxInt.  It turns out that assigning` |
|        - |   62 | `    ** a very large positive number to an integer results in a very large` |
|        - |   63 | `    ** negative integer.  This makes no sense, but it is what x86 hardware` |
|        - |   64 | `    ** does so for compatibility we will do the same in software. */` |
|       37 |   65 | `    return minInt;` |
|      ! 0 |   66 | `  }else{` |
|     2196 |   67 | `    return (sxi64)r;` |
|        - |   68 | `  }` |
|        - |   69 | `#endif` |
|     1118 |   70 |  |
|        - |   71 | `/*` |
|        - |   72 | ` * Convert a raw token value typically a stream of digit [i.e: hex,octal,binary or decimal]` |
|        - |   73 | ` * to a 64-bit integer.` |
|        - |   74 | ` */` |
|   123718 |   75 | `PH7_PRIVATE sxi64 PH7_TokenValueToInt64(SyString *pVal)` |
|        5 |   76 |  |
|   123723 |   77 | `	sxi64 iVal = 0;` |
|   123723 |   78 | `	if( pVal->nByte <= 0 ){` |
|        7 |   79 | `		return 0;` |
|        - |   80 | `	}` |
|   123717 |   81 | `	if( pVal->zString[0] == '0' ){` |
|        - |   82 | `		sxi32 c;` |
|    51342 |   83 | `		if( pVal->nByte == sizeof(char) ){` |
|    50939 |   84 | `			return 0;` |
|        - |   85 | `		}` |
|      404 |   86 | `		c = pVal->zString[1];` |
|      404 |   87 | `		if( c  == 'x' \|\| c == 'X' ){` |
|        - |   88 | `			/* Hex digit stream */` |
|       69 |   89 | `			SyHexStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|      370 |   90 | `		}else if( c == 'b' \|\| c == 'B' ){` |
|        - |   91 | `			/* Binary digit stream */` |
|      277 |   92 | `			SyBinaryStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|      139 |   93 | `		}else{` |
|        - |   94 | `			/* Octal digit stream */` |
|       60 |   95 | `			SyOctalStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|        - |   96 | `		}` |
|      203 |   97 | `	}else{` |
|        - |   98 | `		/* Decimal digit stream */` |
|    72380 |   99 | `		SyStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|        - |  100 | `	}` |
|    72783 |  101 | `	return iVal;` |
|    61864 |  102 |  |
|        - |  103 | `/*` |
|        - |  104 | ` * Return some kind of 64-bit integer value which is the best we can` |
|        - |  105 | ` * do at representing the value that pObj describes as a string` |
|        - |  106 | ` * representation.` |
|        - |  107 | ` */` |
|      412 |  108 | `static sxi64 MemObjStringToInt(ph7_value *pObj)` |
|        5 |  109 |  |
|        - |  110 | `	SyString sVal;` |
|      417 |  111 | `	SyStringInitFromBuf(&sVal,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|      417 |  112 | `	return PH7_TokenValueToInt64(&sVal);` |
|        5 |  113 |  |
|        - |  114 | `/*` |
|        - |  115 | ` * Call a magic class method [i.e: __toString(),__toInt(),...]` |
|        - |  116 | ` * Return SXRET_OK if the magic method is available and have been` |
|        - |  117 | ` * successfully called. Any other return value indicates failure.` |
|        - |  118 | ` */` |
|       92 |  119 | `static sxi32 MemObjCallClassCastMethod(` |
|        - |  120 | `	ph7_vm *pVm,               /* VM that trigger the invocation */` |
|        - |  121 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object] */` |
|        - |  122 | `	const char *zMethod,       /* Magic method name [i.e: __toString] */` |
|        - |  123 | `	sxu32 nLen,                /* Method name length */` |
|        - |  124 | `	ph7_value *pResult         /* OUT: Store the return value of the magic method here */` |
|        - |  125 | `	)` |
|        5 |  126 |  |
|        - |  127 | `	ph7_class_method *pMethod;` |
|        - |  128 | `	/* Check if the method is available */` |
|       97 |  129 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,zMethod,nLen);` |
|       97 |  130 | `	if( pMethod == 0 ){` |
|        - |  131 | `		/* No such method */` |
|        3 |  132 | `		return SXERR_NOTFOUND;` |
|        - |  133 | `	}` |
|        - |  134 | `	/* Invoke the desired method */` |
|       95 |  135 | `	PH7_VmCallClassMethod(&(*pVm),&(*pThis),pMethod,&(*pResult),0,0);` |
|        - |  136 | `	/* Method successfully called,pResult should hold the return value */` |
|       95 |  137 | `	return SXRET_OK;` |
|       51 |  138 |  |
|        - |  139 | `/*` |
|        - |  140 | ` * Return some kind of integer value which is the best we can` |
|        - |  141 | ` * do at representing the value that pObj describes as an integer.` |
|        - |  142 | ` * If pObj is an integer, then the value is exact. If pObj is` |
|        - |  143 | ` * a floating-point then  the value returned is the integer part.` |
|        - |  144 | ` * If pObj is a string, then we make an attempt to convert it into` |
|        - |  145 | ` * a integer and return that.` |
|        - |  146 | ` * If pObj represents a NULL value, return 0.` |
|        - |  147 | ` */` |
|      480 |  148 | `static sxi64 MemObjIntValue(ph7_value *pObj)` |
|        5 |  149 |  |
|        - |  150 | `	sxi32 iFlags;` |
|      485 |  151 | `	iFlags = pObj->iFlags;` |
|      485 |  152 | `	if (iFlags & MEMOBJ_REAL ){` |
|       31 |  153 | `		return MemObjRealToInt(&(*pObj));` |
|      455 |  154 | `	}else if( iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      117 |  155 | `		return pObj->x.iVal;` |
|      339 |  156 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|      317 |  157 | `		return MemObjStringToInt(&(*pObj));` |
|       23 |  158 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|       11 |  159 | `		return 0;` |
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
|      245 |  186 |  |
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
|     1358 |  197 | `static ph7_real MemObjRealValue(ph7_value *pObj)` |
|        4 |  198 |  |
|        - |  199 | `	sxi32 iFlags;` |
|     1362 |  200 | `	iFlags = pObj->iFlags;` |
|     1362 |  201 | `	if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  202 | `		return pObj->rVal;` |
|     1362 |  203 | `	}else if (iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      692 |  204 | `		return (ph7_real)pObj->x.iVal;` |
|      672 |  205 | `	}else if (iFlags & MEMOBJ_STRING){` |
|        - |  206 | `		SyString sString;` |
|        - |  207 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  208 | `		ph7_real rVal = 0;` |
|        - |  209 | `#else` |
|      666 |  210 | `		ph7_real rVal = 0.0;` |
|        - |  211 | `#endif` |
|      666 |  212 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|      666 |  213 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|        - |  214 | `			/* Convert as much as we can */` |
|        - |  215 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  216 | `			rVal = MemObjStringToInt(&(*pObj));` |
|        - |  217 | `#else` |
|      666 |  218 | `			SyStrToReal(sString.zString,sString.nByte,(void *)&rVal,0);` |
|        - |  219 | `#endif` |
|      331 |  220 | `		}` |
|      666 |  221 | `		return rVal;` |
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
|      683 |  254 |  |
|        - |  255 | `/*` |
|        - |  256 | ` * Return the string representation of a given ph7_value.` |
|        - |  257 | ` * This function never fail and always return SXRET_OK.` |
|        - |  258 | ` */` |
|    55392 |  259 | `static sxi32 MemObjStringValue(SyBlob *pOut,ph7_value *pObj,sxu8 bStrictBool)` |
|        5 |  260 |  |
|    55397 |  261 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|        - |  262 | `		/* Handle special floating-point values first */` |
|      299 |  263 | `		if( PH7_IS_NAN(pObj->rVal) ){` |
|      ! 0 |  264 | `			SyBlobAppend(&(*pOut),"NAN",3);` |
|      299 |  265 | `		}else if( PH7_IS_INF(pObj->rVal) ){` |
|      ! 0 |  266 | `			if( pObj->rVal < 0.0 ){` |
|      ! 0 |  267 | `				SyBlobAppend(&(*pOut),"-INF",4);` |
|      ! 0 |  268 | `			}else{` |
|      ! 0 |  269 | `				SyBlobAppend(&(*pOut),"INF",3);` |
|        - |  270 | `			}` |
|      ! 0 |  271 | `		}else{` |
|      299 |  272 | `			SyBlobFormat(&(*pOut),"%.15g",pObj->rVal);` |
|        3 |  273 | `		}` |
|    55249 |  274 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|    54661 |  275 | `		SyBlobFormat(&(*pOut),"%qd",pObj->x.iVal);` |
|        - |  276 | `		/* %qd (BSD quad) is equivalent to %lld in the libc printf */` |
|    27773 |  277 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|      315 |  278 | `		if( bStrictBool ){` |
|        - |  279 | `			/* Actual string cast: true -> "1", false -> "" (like PHP) */` |
|      207 |  280 | `			if( pObj->x.iVal ){` |
|       20 |  281 | `				SyBlobAppend(&(*pOut),"1",sizeof("1")-1);` |
|        9 |  282 | `			}` |
|        - |  283 | `			/* false produces empty string, nothing to append */` |
|      106 |  284 | `		}else{` |
|        - |  285 | `			/* Display path (var_dump, print_r): show TRUE/FALSE */` |
|      111 |  286 | `			if( pObj->x.iVal ){` |
|       65 |  287 | `				SyBlobAppend(&(*pOut),"TRUE",sizeof("TRUE")-1);` |
|       34 |  288 | `			}else{` |
|       48 |  289 | `				SyBlobAppend(&(*pOut),"FALSE",sizeof("FALSE")-1);` |
|        - |  290 | `			}` |
|        5 |  291 | `		}` |
|      290 |  292 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|        3 |  293 | `		SyBlobAppend(&(*pOut),"Array",sizeof("Array")-1);` |
|        3 |  294 | `		PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
|      134 |  295 | `	}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|        - |  296 | `		ph7_value sResult;` |
|        - |  297 | `		sxi32 rc;` |
|        - |  298 | `		/* Invoke the __toString() method if available */` |
|       83 |  299 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|       83 |  300 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|        - |  301 | `			"__toString",sizeof("__toString")-1,&sResult);` |
|       83 |  302 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_STRING) && SyBlobLength(&sResult.sBlob) > 0){` |
|        - |  303 | `			/* Expand method return value */` |
|       79 |  304 | `			SyBlobDup(&sResult.sBlob,pOut);` |
|       42 |  305 | `		}else{` |
|        - |  306 | `			/* Expand "Object" as requested by the PHP language reference manual */` |
|        5 |  307 | `			SyBlobAppend(&(*pOut),"Object",sizeof("Object")-1);` |
|        - |  308 | `		}` |
|       83 |  309 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|       83 |  310 | `		PH7_MemObjRelease(&sResult);` |
|       92 |  311 | `	}else if(pObj->iFlags & MEMOBJ_RES ){` |
|        3 |  312 | `		SyBlobFormat(&(*pOut),"ResourceID_%#x",pObj->x.pOther);` |
|        1 |  313 | `	}` |
|    55397 |  314 | `	return SXRET_OK;` |
|        5 |  315 |  |
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
|    13560 |  328 | `static sxi32 MemObjBooleanValue(ph7_value *pObj)` |
|        5 |  329 |  |
|        - |  330 | `	sxi32 iFlags;` |
|    13565 |  331 | `	iFlags = pObj->iFlags;` |
|    13565 |  332 | `	if (iFlags & MEMOBJ_REAL ){` |
|        - |  333 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  334 | `		return pObj->rVal ? 1 : 0;` |
|        - |  335 | `#else` |
|       12 |  336 | `		return pObj->rVal != 0.0 ? 1 : 0;` |
|        - |  337 | `#endif` |
|    13555 |  338 | `	}else if( iFlags & MEMOBJ_INT ){` |
|      115 |  339 | `		return pObj->x.iVal ? 1 : 0;` |
|    13445 |  340 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|        - |  341 | `		SyString sString;` |
|       67 |  342 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|       67 |  343 | `		if( sString.nByte == 0 ){` |
|        - |  344 | `			/* Empty string */` |
|       17 |  345 | `			return 0;` |
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
|    13381 |  361 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|    12347 |  362 | `		return 0;` |
|     1039 |  363 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|       20 |  364 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|       20 |  365 | `		sxu32 n = pMap->nEntry;` |
|       20 |  366 | `		PH7_HashmapUnref(pMap);` |
|       20 |  367 | `		return n > 0 ? TRUE : FALSE;` |
|     1021 |  368 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
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
|     1015 |  383 | `	}else if(iFlags & MEMOBJ_RES ){` |
|     1015 |  384 | `		return pObj->x.pOther != 0;` |
|        - |  385 | `	}` |
|        - |  386 | `	/* NOT REACHED */` |
|      ! 0 |  387 | `	return 0;` |
|     6785 |  388 |  |
|        - |  389 | `/*` |
|        - |  390 | ` * If the ph7_value is of type real,try to make it an integer also.` |
|        - |  391 | ` */` |
|     2198 |  392 | `static sxi32 MemObjTryIntger(ph7_value *pObj)` |
|        4 |  393 |  |
|     2202 |  394 | `	pObj->x.iVal = MemObjRealToInt(&(*pObj));` |
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
|     2198 |  407 | `	if( pObj->rVal ==(ph7_real)pObj->x.iVal && pObj->x.iVal>SMALLEST_INT64` |
|     1241 |  408 | `      && pObj->x.iVal<LARGEST_INT64 ){` |
|     1240 |  409 | `		  pObj->iFlags \|= MEMOBJ_INT;` |
|      620 |  410 | `	}` |
|     2202 |  411 | `	return SXRET_OK;` |
|        4 |  412 |  |
|        - |  413 | `/*` |
|        - |  414 | ` * Convert a ph7_value to type integer.Invalidate any prior representations.` |
|        - |  415 | ` */` |
|   382872 |  416 | `PH7_PRIVATE sxi32 PH7_MemObjToInteger(ph7_value *pObj)` |
|        5 |  417 |  |
|   382877 |  418 | `	if( (pObj->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  419 | `		/* Preform the conversion */` |
|      485 |  420 | `		pObj->x.iVal = MemObjIntValue(&(*pObj));` |
|        - |  421 | `		/* Invalidate any prior representations */` |
|      485 |  422 | `		SyBlobRelease(&pObj->sBlob);` |
|      485 |  423 | `		MemObjSetType(pObj,MEMOBJ_INT);` |
|      240 |  424 | `	}` |
|   382877 |  425 | `	return SXRET_OK;` |
|        5 |  426 |  |
|        - |  427 | `/*` |
|        - |  428 | ` * Convert a ph7_value to type real (Try to get an integer representation also).` |
|        - |  429 | ` * Invalidate any prior representations` |
|        - |  430 | ` */` |
|     1678 |  431 | `PH7_PRIVATE sxi32 PH7_MemObjToReal(ph7_value *pObj)` |
|        4 |  432 |  |
|     1682 |  433 | `	if((pObj->iFlags & MEMOBJ_REAL) == 0 ){` |
|        - |  434 | `		/* Preform the conversion */` |
|     1362 |  435 | `		pObj->rVal = MemObjRealValue(&(*pObj));` |
|        - |  436 | `		/* Invalidate any prior representations */` |
|     1362 |  437 | `		SyBlobRelease(&pObj->sBlob);` |
|     1362 |  438 | `		MemObjSetType(pObj,MEMOBJ_REAL);` |
|        - |  439 | `		/* Try to get an integer representation */` |
|     1362 |  440 | `		MemObjTryIntger(&(*pObj));` |
|      679 |  441 | `	}` |
|     1682 |  442 | `	return SXRET_OK;` |
|        4 |  443 |  |
|        - |  444 | `/*` |
|        - |  445 | ` * Convert a ph7_value to type boolean.Invalidate any prior representations.` |
|        - |  446 | ` */` |
|    14240 |  447 | `PH7_PRIVATE sxi32 PH7_MemObjToBool(ph7_value *pObj)` |
|        5 |  448 |  |
|    14245 |  449 | `	if( (pObj->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        - |  450 | `		/* Preform the conversion */` |
|    13565 |  451 | `		pObj->x.iVal = MemObjBooleanValue(&(*pObj));` |
|        - |  452 | `		/* Invalidate any prior representations */` |
|    13565 |  453 | `		SyBlobRelease(&pObj->sBlob);` |
|    13565 |  454 | `		MemObjSetType(pObj,MEMOBJ_BOOL);` |
|     6780 |  455 | `	}` |
|    14245 |  456 | `	return SXRET_OK;` |
|        5 |  457 |  |
|        - |  458 | `/*` |
|        - |  459 | ` * Convert a ph7_value to type string.Prior representations are NOT invalidated.` |
|        - |  460 | ` */` |
|   764159 |  461 | `PH7_PRIVATE sxi32 PH7_MemObjToString(ph7_value *pObj)` |
|        5 |  462 |  |
|   764164 |  463 | `	sxi32 rc = SXRET_OK;` |
|   764164 |  464 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  465 | `		/* Perform the conversion */` |
|    55175 |  466 | `		SyBlobReset(&pObj->sBlob); /* Reset the internal buffer */` |
|    55175 |  467 | `		rc = MemObjStringValue(&pObj->sBlob,&(*pObj),TRUE);` |
|    55175 |  468 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|    27585 |  469 | `	}` |
|   764164 |  470 | `	return rc;` |
|        5 |  471 |  |
|        - |  472 | `/*` |
|        - |  473 | ` * Nullify a ph7_value.In other words invalidate any prior` |
|        - |  474 | ` * representation.` |
|        - |  475 | ` */` |
|      ! 0 |  476 | `PH7_PRIVATE sxi32 PH7_MemObjToNull(ph7_value *pObj)` |
|      ! 0 |  477 |  |
|      ! 0 |  478 | `	return PH7_MemObjRelease(pObj);` |
|      ! 0 |  479 |  |
|        - |  480 | `/*` |
|        - |  481 | ` * Convert a ph7_value to type array.Invalidate any prior representations.` |
|        - |  482 | `  * According to the PHP language reference manual.` |
|        - |  483 | `  *   For any of the types: integer, float, string, boolean converting a value` |
|        - |  484 | `  *   to an array results in an array with a single element with index zero` |
|        - |  485 | `  *   and the value of the scalar which was converted.` |
|        - |  486 | `  */` |
|      128 |  487 | `PH7_PRIVATE sxi32 PH7_MemObjToHashmap(ph7_value *pObj)` |
|        4 |  488 |  |
|      132 |  489 | `	if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  490 | `		ph7_hashmap *pMap;` |
|        - |  491 | `		/* Allocate a new hashmap instance */` |
|       94 |  492 | `		pMap = PH7_NewHashmap(pObj->pVm,0,0);` |
|       94 |  493 | `		if( pMap == 0 ){` |
|      ! 0 |  494 | `			return SXERR_MEM;` |
|        - |  495 | `		}` |
|       94 |  496 | `		if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|        - |  497 | `			/*` |
|        - |  498 | `			 * According to the PHP language reference manual.` |
|        - |  499 | `			 *   For any of the types: integer, float, string, boolean converting a value` |
|        - |  500 | `			 *   to an array results in an array with a single element with index zero` |
|        - |  501 | `			 *   and the value of the scalar which was converted.` |
|        - |  502 | `			 */` |
|       21 |  503 | `			if( pObj->iFlags & MEMOBJ_OBJ ){` |
|        - |  504 | `				/* Object cast */` |
|        7 |  505 | `				PH7_ClassInstanceToHashmap((ph7_class_instance *)pObj->x.pOther,pMap);` |
|        4 |  506 | `			}else{` |
|        - |  507 | `				/* Insert a single element */` |
|       15 |  508 | `				PH7_HashmapInsert(pMap,0/* Automatic index assign */,&(*pObj));` |
|        - |  509 | `			}` |
|       21 |  510 | `			SyBlobRelease(&pObj->sBlob);` |
|       10 |  511 | `		}` |
|        - |  512 | `		/* Invalidate any prior representation */` |
|       94 |  513 | `		PH7_MemObjRelease(pObj);` |
|       94 |  514 | `		MemObjSetType(pObj,MEMOBJ_HASHMAP);` |
|       94 |  515 | `		pObj->x.pOther = pMap;` |
|       45 |  516 | `	}` |
|      132 |  517 | `	return SXRET_OK;` |
|       68 |  518 |  |
|        - |  519 | `/*` |
|        - |  520 | ` * Convert a ph7_value to type object.Invalidate any prior representations.` |
|        - |  521 | ` * The new object is instantiated from the builtin stdClass().` |
|        - |  522 | ` * The stdClass() class have a single attribute which is '$value'. This attribute` |
|        - |  523 | ` * hold a copy of the converted ph7_value.` |
|        - |  524 | ` * The internal of the stdClass is as follows:` |
|        - |  525 | ` * class stdClass{` |
|        - |  526 | ` *	 public $value;` |
|        - |  527 | ` *	 public function __toInt(){ return (int)$this->value; }` |
|        - |  528 | ` *	 public function __toBool(){ return (bool)$this->value; }` |
|        - |  529 | ` *	 public function __toFloat(){ return (float)$this->value; }` |
|        - |  530 | ` *	 public function __toString(){ return (string)$this->value; }` |
|        - |  531 | ` *	 function __construct($v){ $this->value = $v; }"` |
|        - |  532 | ` *  }` |
|        - |  533 | ` * Refer to the official documentation for more information.` |
|        - |  534 | ` */` |
|       16 |  535 | `PH7_PRIVATE sxi32 PH7_MemObjToObject(ph7_value *pObj)` |
|        1 |  536 |  |
|       17 |  537 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  538 | `		ph7_class_instance *pStd;` |
|        - |  539 | `		ph7_class_method *pCons;` |
|        - |  540 | `		ph7_class *pClass;` |
|        - |  541 | `		ph7_vm *pVm;` |
|        - |  542 | `		/* Point to the underlying VM */` |
|       17 |  543 | `		pVm = pObj->pVm;` |
|        - |  544 | `		/* Point to the stdClass() */` |
|       17 |  545 | `		pClass = PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,0,0);` |
|       17 |  546 | `		if( pClass == 0 ){` |
|        - |  547 | `			/* Can't happen,load null instead */` |
|      ! 0 |  548 | `			PH7_MemObjRelease(pObj);` |
|      ! 0 |  549 | `			return SXRET_OK;` |
|        - |  550 | `		}` |
|        - |  551 | `		/* Instanciate a new stdClass() object */` |
|       17 |  552 | `		pStd = PH7_NewClassInstance(pVm,pClass);` |
|       17 |  553 | `		if( pStd == 0 ){` |
|        - |  554 | `			/* Out of memory */` |
|      ! 0 |  555 | `			PH7_MemObjRelease(pObj);` |
|      ! 0 |  556 | `			return SXRET_OK;` |
|        - |  557 | `		}` |
|        - |  558 | `		/* Check if a constructor is available */` |
|       17 |  559 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       17 |  560 | `		if( pCons ){` |
|        - |  561 | `			ph7_value *apArg[2];` |
|        - |  562 | `			/* Invoke the constructor with one argument */` |
|       17 |  563 | `			apArg[0] = pObj;` |
|       17 |  564 | `			PH7_VmCallClassMethod(pVm,pStd,pCons,0,1,apArg);` |
|       17 |  565 | `			if( pStd->iRef < 1 ){` |
|      ! 0 |  566 | `				pStd->iRef = 1;` |
|      ! 0 |  567 | `			}` |
|        8 |  568 | `		}` |
|        - |  569 | `		/* Invalidate any prior representation */` |
|       17 |  570 | `		PH7_MemObjRelease(pObj);` |
|        - |  571 | `		/* Save the new instance */` |
|       17 |  572 | `		pObj->x.pOther = pStd;` |
|       17 |  573 | `		MemObjSetType(pObj,MEMOBJ_OBJ);` |
|        8 |  574 | `	}` |
|       17 |  575 | `	return SXRET_OK;` |
|        9 |  576 |  |
|        - |  577 | `/*` |
|        - |  578 | ` * Return a pointer to the appropriate convertion method associated` |
|        - |  579 | ` * with the given type.` |
|        - |  580 | ` * Note on type juggling.` |
|        - |  581 | ` * Accoding to the PHP language reference manual` |
|        - |  582 | ` *  PHP does not require (or support) explicit type definition in variable` |
|        - |  583 | ` *  declaration; a variable's type is determined by the context in which` |
|        - |  584 | ` *  the variable is used. That is to say, if a string value is assigned` |
|        - |  585 | ` *  to variable $var, $var becomes a string. If an integer value is then` |
|        - |  586 | ` *  assigned to $var, it becomes an integer.` |
|        - |  587 | ` */` |
|       56 |  588 | `PH7_PRIVATE ProcMemObjCast PH7_MemObjCastMethod(sxi32 iFlags)` |
|        5 |  589 |  |
|       61 |  590 | `	if( iFlags & MEMOBJ_STRING ){` |
|       16 |  591 | `		return PH7_MemObjToString;` |
|       47 |  592 | `	}else if( iFlags & MEMOBJ_INT ){` |
|       39 |  593 | `		return PH7_MemObjToInteger;` |
|       11 |  594 | `	}else if( iFlags & MEMOBJ_REAL ){` |
|        8 |  595 | `		return PH7_MemObjToReal;` |
|        3 |  596 | `	}else if( iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  597 | `		return PH7_MemObjToBool;` |
|        3 |  598 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|        3 |  599 | `		return PH7_MemObjToHashmap;` |
|      ! 0 |  600 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  601 | `		return PH7_MemObjToObject;` |
|      ! 0 |  602 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|        - |  603 | ``		/* `null` is a type, not a weak-coercion target: never silently cast a`` |
|        - |  604 | ``		 * value to null for a standalone `null` type hint. Return/property`` |
|        - |  605 | `		 * enforcement reject a non-null value before reaching here; this guards` |
|        - |  606 | `		 * the parameter default-value path from quietly nulling a non-null` |
|        - |  607 | `		 * default. */` |
|      ! 0 |  608 | `		return 0;` |
|        - |  609 | `	}` |
|        - |  610 | `	/* NULL cast */` |
|      ! 0 |  611 | `	return PH7_MemObjToNull;` |
|       33 |  612 |  |
|        - |  613 | `/*` |
|        - |  614 | ` * Check whether the ph7_value is numeric [i.e: int/float/bool] or looks` |
|        - |  615 | ` * like a numeric number [i.e: if the ph7_value is of type string.].` |
|        - |  616 | ` * Return TRUE if numeric.FALSE otherwise.` |
|        - |  617 | ` */` |
|   443548 |  618 | `PH7_PRIVATE sxi32 PH7_MemObjIsNumeric(ph7_value *pObj)` |
|        5 |  619 |  |
|   443553 |  620 | `	if( pObj->iFlags & ( MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      241 |  621 | `		return TRUE;` |
|   443317 |  622 | `	}else if( pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|       83 |  623 | `		return FALSE;` |
|   443239 |  624 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        - |  625 | `		SyString sStr;` |
|        - |  626 | `		sxi32 rc;` |
|   443239 |  627 | `		SyStringInitFromBuf(&sStr,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|   443239 |  628 | `		if( sStr.nByte <= 0 ){` |
|        - |  629 | `			/* Empty string */` |
|       73 |  630 | `			return FALSE;` |
|        - |  631 | `		}` |
|        - |  632 | `		/* Check if the string representation looks like a numeric number */` |
|   443167 |  633 | `		rc = SyStrIsNumeric(sStr.zString,sStr.nByte,0,0);` |
|   443167 |  634 | `		return rc == SXRET_OK ? TRUE : FALSE;` |
|        - |  635 | `	}` |
|        - |  636 | `	/* NOT REACHED */` |
|      ! 0 |  637 | `	return FALSE;` |
|   221811 |  638 |  |
|        - |  639 | `/*` |
|        - |  640 | ` * Check whether the ph7_value is empty.Return TRUE if empty.` |
|        - |  641 | ` * FALSE otherwise.` |
|        - |  642 | ` * An ph7_value is considered empty if the following are true:` |
|        - |  643 | ` * NULL value.` |
|        - |  644 | ` * Boolean FALSE.` |
|        - |  645 | ` * Integer/Float with a 0 (zero) value.` |
|        - |  646 | ` * An empty string or a stream of 0 (zero) [i.e: "0","00","000",...].` |
|        - |  647 | ` * An empty array.` |
|        - |  648 | ` * NOTE` |
|        - |  649 | ` *  OBJECT VALUE MUST NOT BE MODIFIED.` |
|        - |  650 | ` */` |
|    27034 |  651 | `PH7_PRIVATE sxi32 PH7_MemObjIsEmpty(ph7_value *pObj)` |
|        5 |  652 |  |
|    27039 |  653 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|       13 |  654 | `		return TRUE;` |
|    27029 |  655 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|       18 |  656 | `		return pObj->x.iVal == 0 ? TRUE : FALSE;` |
|    27013 |  657 | `	}else if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  658 | `		return pObj->rVal == (ph7_real)0 ? TRUE : FALSE;` |
|    27013 |  659 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|        5 |  660 | `		return !pObj->x.iVal;` |
|    27009 |  661 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|    26975 |  662 | `		if( SyBlobLength(&pObj->sBlob) <= 0 ){` |
|    21987 |  663 | `			return TRUE;` |
|      ! 0 |  664 | `		}else{` |
|        - |  665 | `			const char *zIn,*zEnd;` |
|     4993 |  666 | `			zIn = (const char *)SyBlobData(&pObj->sBlob);` |
|     4993 |  667 | `			zEnd = &zIn[SyBlobLength(&pObj->sBlob)];` |
|     4999 |  668 | `			while( zIn < zEnd ){` |
|     4999 |  669 | `				if( zIn[0] != '0' ){` |
|     4993 |  670 | `					break;` |
|        - |  671 | `				}` |
|        7 |  672 | `				zIn++;` |
|        1 |  673 | `			}` |
|     4993 |  674 | `			return zIn >= zEnd ? TRUE : FALSE;` |
|      ! 0 |  675 | `		}` |
|       39 |  676 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       39 |  677 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|       39 |  678 | `		return pMap->nEntry == 0 ? TRUE : FALSE;` |
|      ! 0 |  679 | `	}else if ( pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|      ! 0 |  680 | `		return FALSE;` |
|        - |  681 | `	}` |
|        - |  682 | `	/* Assume empty by default */` |
|      ! 0 |  683 | `	return TRUE;` |
|    13522 |  684 |  |
|        - |  685 | `/*` |
|        - |  686 | ` * Convert a ph7_value so that it has types MEMOBJ_REAL or MEMOBJ_INT` |
|        - |  687 | ` * or both.` |
|        - |  688 | ` * Invalidate any prior representations. Every effort is made to force` |
|        - |  689 | ` * the conversion, even if the input is a string that does not look` |
|        - |  690 | ` * completely like a number.Convert as much of the string as we can` |
|        - |  691 | ` * and ignore the rest.` |
|        - |  692 | ` */` |
|   421863 |  693 | `PH7_PRIVATE sxi32 PH7_MemObjToNumeric(ph7_value *pObj)` |
|        5 |  694 |  |
|   421868 |  695 | `	if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|   421762 |  696 | `		if( pObj->iFlags & (MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|        3 |  697 | `			if( pObj->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  698 | `				pObj->x.iVal = 0;` |
|      ! 0 |  699 | `			}` |
|        3 |  700 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|        1 |  701 | `		}` |
|        - |  702 | `		/* Already numeric */` |
|   421762 |  703 | `		return  SXRET_OK;` |
|        - |  704 | `	}` |
|      107 |  705 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|      107 |  706 | `		sxi32 rc = SXERR_INVALID;` |
|      107 |  707 | `		sxu8 bReal = FALSE;` |
|        - |  708 | `		SyString sString;` |
|      107 |  709 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|        - |  710 | `		/* Check if the given string looks like a numeric number */` |
|      107 |  711 | `		if( sString.nByte > 0 ){` |
|      107 |  712 | `			rc = SyStrIsNumeric(sString.zString,sString.nByte,&bReal,0);` |
|       53 |  713 | `		}` |
|      107 |  714 | `		if( bReal ){` |
|        7 |  715 | `			PH7_MemObjToReal(&(*pObj));` |
|        4 |  716 | `		}else{` |
|      101 |  717 | `			if( rc != SXRET_OK ){` |
|        - |  718 | `				/* The input does not look at all like a number,set the value to 0 */` |
|      ! 0 |  719 | `				pObj->x.iVal = 0;` |
|      ! 0 |  720 | `			}else{` |
|        - |  721 | `				/* Convert as much as we can */` |
|      101 |  722 | `				pObj->x.iVal = MemObjStringToInt(&(*pObj));` |
|        - |  723 | `			}` |
|      101 |  724 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|      101 |  725 | `			SyBlobRelease(&pObj->sBlob);` |
|        1 |  726 | `		}` |
|       53 |  727 | `	}else if(pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)){` |
|      ! 0 |  728 | `		PH7_MemObjToInteger(pObj);` |
|      ! 0 |  729 | `	}else{` |
|        - |  730 | `		/* Perform a blind cast */` |
|      ! 0 |  731 | `		PH7_MemObjToReal(&(*pObj));` |
|        - |  732 | `	}` |
|      107 |  733 | `	return SXRET_OK;` |
|   210980 |  734 |  |
|        - |  735 | `/*` |
|        - |  736 | ` * Apply Perl-style increment to a string ph7_value in place.` |
|        - |  737 | ` * Walks the bytes right-to-left: digits 0-8 / letters a-y, A-Y bump in` |
|        - |  738 | ` * place; '9' wraps to '0' with carry; 'z' to 'a'; 'Z' to 'A'. A non-` |
|        - |  739 | ` * alphanumeric byte stops the walk without prepending. If carry survives` |
|        - |  740 | ` * past index 0, prepend '1', 'a', or 'A' depending on the class of the` |
|        - |  741 | ` * last carried character. Empty strings become "1".` |
|        - |  742 | ` *` |
|        - |  743 | ` * Caller must ensure pObj is MEMOBJ_STRING and not a numeric string;` |
|        - |  744 | ` * this routine never reclassifies the type, so a result like "e0" stays` |
|        - |  745 | ` * a string even though it looks numeric.` |
|        - |  746 | ` */` |
|       48 |  747 | `PH7_PRIVATE sxi32 PH7_MemObjStringIncrement(ph7_value *pObj)` |
|        1 |  748 |  |
|        - |  749 | `	enum CarryClass { CARRY_NONE = 0, CARRY_LOWER, CARRY_UPPER, CARRY_DIGIT };` |
|       49 |  750 | `	enum CarryClass last_class = CARRY_NONE;` |
|        - |  751 | `	sxu32 nLen, pos;` |
|        - |  752 | `	sxu8 *zStr;` |
|       49 |  753 | `	int carry = 1;` |
|        - |  754 | `	int ch;` |
|        - |  755 | `	/* Force ownership: the blob may be SXBLOB_RDONLY (e.g., from` |
|        - |  756 | `	 * PH7_MemObjLoad), in which case BlobPrepareGrow copies on demand` |
|        - |  757 | `	 * and clears the flag.  On an already-owned blob with spare capacity` |
|        - |  758 | `	 * (the common case under PHL's growth allocator), this is a no-op` |
|        - |  759 | `	 * append; on an exact-fit owned blob it triggers a single realloc. */` |
|       49 |  760 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       45 |  761 | `		SyBlobNullAppend(&pObj->sBlob);` |
|       22 |  762 | `	}` |
|       49 |  763 | `	nLen = SyBlobLength(&pObj->sBlob);` |
|       49 |  764 | `	if( nLen == 0 ){` |
|        5 |  765 | `		SyBlobAppend(&pObj->sBlob,"1",sizeof(char));` |
|        5 |  766 | `		return SXRET_OK;` |
|        - |  767 | `	}` |
|       45 |  768 | `	zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|       45 |  769 | `	pos = nLen;` |
|       97 |  770 | `	while( pos > 0 ){` |
|       79 |  771 | `		pos--;` |
|       79 |  772 | `		ch = zStr[pos];` |
|       79 |  773 | `		if( ch >= 'a' && ch <= 'z' ){` |
|       45 |  774 | `			if( ch == 'z' ){` |
|       29 |  775 | `				zStr[pos] = 'a';` |
|       29 |  776 | `				last_class = CARRY_LOWER;` |
|       29 |  777 | `				continue;` |
|        - |  778 | `			}` |
|       17 |  779 | `			zStr[pos]++;` |
|       17 |  780 | `			carry = 0;` |
|       17 |  781 | `			break;` |
|       35 |  782 | `		}else if( ch >= 'A' && ch <= 'Z' ){` |
|       21 |  783 | `			if( ch == 'Z' ){` |
|       19 |  784 | `				zStr[pos] = 'A';` |
|       19 |  785 | `				last_class = CARRY_UPPER;` |
|       19 |  786 | `				continue;` |
|        - |  787 | `			}` |
|        3 |  788 | `			zStr[pos]++;` |
|        3 |  789 | `			carry = 0;` |
|        3 |  790 | `			break;` |
|       15 |  791 | `		}else if( ch >= '0' && ch <= '9' ){` |
|        7 |  792 | `			if( ch == '9' ){` |
|        7 |  793 | `				zStr[pos] = '0';` |
|        7 |  794 | `				last_class = CARRY_DIGIT;` |
|        7 |  795 | `				continue;` |
|        - |  796 | `			}` |
|      ! 0 |  797 | `			zStr[pos]++;` |
|      ! 0 |  798 | `			carry = 0;` |
|      ! 0 |  799 | `			break;` |
|      ! 0 |  800 | `		}else{` |
|        - |  801 | `			/* non-alphanumeric: stop without prepending */` |
|        9 |  802 | `			carry = 0;` |
|        9 |  803 | `			break;` |
|        - |  804 | `		}` |
|      ! 0 |  805 | `	}` |
|       45 |  806 | `	if( carry ){` |
|        - |  807 | `		sxu8 prepend;` |
|        - |  808 | `		sxu32 i;` |
|       19 |  809 | `		switch( last_class ){` |
|        9 |  810 | `			case CARRY_LOWER: prepend = (sxu8)'a'; break;` |
|       11 |  811 | `			case CARRY_UPPER: prepend = (sxu8)'A'; break;` |
|      ! 0 |  812 | `			default:          prepend = (sxu8)'1'; break;` |
|        - |  813 | `		}` |
|        - |  814 | `		/* Append a sentinel byte to grow nByte by 1 (capacity grows too). */` |
|       19 |  815 | `		SyBlobAppend(&pObj->sBlob,"\0",sizeof(char));` |
|       19 |  816 | `		zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|       19 |  817 | `		nLen = SyBlobLength(&pObj->sBlob);` |
|        - |  818 | `		/* Shift right by 1, walking from the end so overlapping is safe. */` |
|       57 |  819 | `		for( i = nLen - 1; i > 0; i-- ){` |
|       39 |  820 | `			zStr[i] = zStr[i - 1];` |
|       20 |  821 | `		}` |
|       19 |  822 | `		zStr[0] = prepend;` |
|        9 |  823 | `	}` |
|       45 |  824 | `	return SXRET_OK;` |
|       25 |  825 |  |
|        - |  826 | `/*` |
|        - |  827 | ` * Try a get an integer representation of the given ph7_value.` |
|        - |  828 | ` * If the ph7_value is not of type real,this function is a no-op.` |
|        - |  829 | ` */` |
|      784 |  830 | `PH7_PRIVATE sxi32 PH7_MemObjTryInteger(ph7_value *pObj)` |
|        1 |  831 |  |
|      785 |  832 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|        - |  833 | `		/* Work only with reals */` |
|      785 |  834 | `		MemObjTryIntger(&(*pObj));` |
|      392 |  835 | `	}` |
|      785 |  836 | `	return SXRET_OK;` |
|        1 |  837 |  |
|        - |  838 | `/*` |
|        - |  839 | ` * Initialize a ph7_value to the null type.` |
|        - |  840 | ` */` |
|  7825221 |  841 | `PH7_PRIVATE sxi32 PH7_MemObjInit(ph7_vm *pVm,ph7_value *pObj)` |
|        5 |  842 |  |
|        - |  843 | `	/* Zero the structure */` |
|  7825226 |  844 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - |  845 | `	/* Initialize fields */` |
|  7825226 |  846 | `	pObj->pVm = pVm;` |
|  7825226 |  847 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - |  848 | `	/* Set the NULL type */` |
|  7825226 |  849 | `	pObj->iFlags = MEMOBJ_NULL;` |
|  7825226 |  850 | `	return SXRET_OK;` |
|        5 |  851 |  |
|        - |  852 | `/*` |
|        - |  853 | ` * Initialize a ph7_value to the integer type.` |
|        - |  854 | ` */` |
|   154610 |  855 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromInt(ph7_vm *pVm,ph7_value *pObj,sxi64 iVal)` |
|        5 |  856 |  |
|        - |  857 | `	/* Zero the structure */` |
|   154615 |  858 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - |  859 | `	/* Initialize fields */` |
|   154615 |  860 | `	pObj->pVm = pVm;` |
|   154615 |  861 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - |  862 | `	/* Set the desired type */` |
|   154615 |  863 | `	pObj->x.iVal = iVal;` |
|   154615 |  864 | `	pObj->iFlags = MEMOBJ_INT;` |
|   154615 |  865 | `	return SXRET_OK;` |
|        5 |  866 |  |
|        - |  867 | `/*` |
|        - |  868 | ` * Initialize a ph7_value to the boolean type.` |
|        - |  869 | ` */` |
|    16982 |  870 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromBool(ph7_vm *pVm,ph7_value *pObj,sxi32 iVal)` |
|        5 |  871 |  |
|        - |  872 | `	/* Zero the structure */` |
|    16987 |  873 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - |  874 | `	/* Initialize fields */` |
|    16987 |  875 | `	pObj->pVm = pVm;` |
|    16987 |  876 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - |  877 | `	/* Set the desired type */` |
|    16987 |  878 | `	pObj->x.iVal = iVal ? 1 : 0;` |
|    16987 |  879 | `	pObj->iFlags = MEMOBJ_BOOL;` |
|    16987 |  880 | `	return SXRET_OK;` |
|        5 |  881 |  |
|        - |  882 | `#if 0` |
|        - |  883 | `/*` |
|        - |  884 | ` * Initialize a ph7_value to the real type.` |
|        - |  885 | ` */` |
|        - |  886 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromReal(ph7_vm *pVm,ph7_value *pObj,ph7_real rVal)` |
|        - |  887 |  |
|        - |  888 | `	/* Zero the structure */` |
|        - |  889 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - |  890 | `	/* Initialize fields */` |
|        - |  891 | `	pObj->pVm = pVm;` |
|        - |  892 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - |  893 | `	/* Set the desired type */` |
|        - |  894 | `	pObj->rVal = rVal;` |
|        - |  895 | `	pObj->iFlags = MEMOBJ_REAL;` |
|        - |  896 | `	return SXRET_OK;` |
|        - |  897 |  |
|        - |  898 | `#endif` |
|        - |  899 | `/*` |
|        - |  900 | ` * Initialize a ph7_value to the array type.` |
|        - |  901 | ` */` |
|    45520 |  902 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromArray(ph7_vm *pVm,ph7_value *pObj,ph7_hashmap *pArray)` |
|        5 |  903 |  |
|        - |  904 | `	/* Zero the structure */` |
|    45525 |  905 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - |  906 | `	/* Initialize fields */` |
|    45525 |  907 | `	pObj->pVm = pVm;` |
|    45525 |  908 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - |  909 | `	/* Set the desired type */` |
|    45525 |  910 | `	pObj->iFlags = MEMOBJ_HASHMAP;` |
|    45525 |  911 | `	pObj->x.pOther = pArray;` |
|    45525 |  912 | `	return SXRET_OK;` |
|        5 |  913 |  |
|        - |  914 | `/*` |
|        - |  915 | ` * Initialize a ph7_value to the string type.` |
|        - |  916 | ` */` |
|   464122 |  917 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromString(ph7_vm *pVm,ph7_value *pObj,const SyString *pVal)` |
|        5 |  918 |  |
|        - |  919 | `	/* Zero the structure */` |
|   464127 |  920 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - |  921 | `	/* Initialize fields */` |
|   464127 |  922 | `	pObj->pVm = pVm;` |
|   464127 |  923 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|   464127 |  924 | `	if( pVal ){` |
|        - |  925 | `		/* Append contents */` |
|   291597 |  926 | `		SyBlobAppend(&pObj->sBlob,(const void *)pVal->zString,pVal->nByte);` |
|   145796 |  927 | `	}` |
|        - |  928 | `	/* Set the desired type */` |
|   464127 |  929 | `	pObj->iFlags = MEMOBJ_STRING;` |
|   464127 |  930 | `	return SXRET_OK;` |
|        5 |  931 |  |
|        - |  932 | `/*` |
|        - |  933 | ` * Append some contents to the internal buffer of a given ph7_value.` |
|        - |  934 | ` * If the given ph7_value is not of type string,this function` |
|        - |  935 | ` * invalidate any prior representation and set the string type.` |
|        - |  936 | ` * Then a simple append operation is performed.` |
|        - |  937 | ` */` |
|   334230 |  938 | `PH7_PRIVATE sxi32 PH7_MemObjStringAppend(ph7_value *pObj,const char *zData,sxu32 nLen)` |
|        5 |  939 |  |
|        - |  940 | `	sxi32 rc;` |
|   334235 |  941 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  942 | `		/* Invalidate any prior representation */` |
|      199 |  943 | `		PH7_MemObjRelease(pObj);` |
|      199 |  944 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|       97 |  945 | `	}` |
|        - |  946 | `	/* Append contents */` |
|   334235 |  947 | `	rc = SyBlobAppend(&pObj->sBlob,zData,nLen);` |
|   334235 |  948 | `	return rc;` |
|        5 |  949 |  |
|        - |  950 | `#if 0` |
|        - |  951 | `/*` |
|        - |  952 | ` * Format and append some contents to the internal buffer of a given ph7_value.` |
|        - |  953 | ` * If the given ph7_value is not of type string,this function invalidate` |
|        - |  954 | ` * any prior representation and set the string type.` |
|        - |  955 | ` * Then a simple format and append operation is performed.` |
|        - |  956 | ` */` |
|        - |  957 | `PH7_PRIVATE sxi32 PH7_MemObjStringFormat(ph7_value *pObj,const char *zFormat,va_list ap)` |
|        - |  958 |  |
|        - |  959 | `	sxi32 rc;` |
|        - |  960 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  961 | `		/* Invalidate any prior representation */` |
|        - |  962 | `		PH7_MemObjRelease(pObj);` |
|        - |  963 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|        - |  964 | `	}` |
|        - |  965 | `	/* Format and append contents */` |
|        - |  966 | `	rc = SyBlobFormatAp(&pObj->sBlob,zFormat,ap);` |
|        - |  967 | `	return rc;` |
|        - |  968 |  |
|        - |  969 | `#endif` |
|        - |  970 | `/*` |
|        - |  971 | ` * Duplicate the contents of a ph7_value.` |
|        - |  972 | ` */` |
|  4539549 |  973 | `PH7_PRIVATE sxi32 PH7_MemObjStore(ph7_value *pSrc,ph7_value *pDest)` |
|        5 |  974 |  |
|  4539554 |  975 | `	ph7_class_instance *pObj = 0;` |
|  4539554 |  976 | `	ph7_hashmap *pMap = 0;` |
|        - |  977 | `	sxi32 rc;` |
|  4539554 |  978 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  979 | `		/* Increment reference count */` |
|   149259 |  980 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  4464927 |  981 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|        - |  982 | `		/* Increment reference count */` |
|     2013 |  983 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|     1004 |  984 | `	}` |
|  4539554 |  985 | `	if( pDest->iFlags & MEMOBJ_HASHMAP ){` |
|    44363 |  986 | `		pMap = (ph7_hashmap *)pDest->x.pOther;` |
|  4517375 |  987 | `	}else if( pDest->iFlags & MEMOBJ_OBJ ){` |
|     2459 |  988 | `		pObj = (ph7_class_instance *)pDest->x.pOther;` |
|     1227 |  989 | `	}` |
|  4539554 |  990 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|  4539554 |  991 | `	pDest->iFlags &= ~MEMOBJ_AUX;` |
|  4539554 |  992 | `	rc = SXRET_OK;` |
|  4539554 |  993 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  3809281 |  994 | `		SyBlobReset(&pDest->sBlob);` |
|  3809281 |  995 | `		rc = SyBlobDup(&pSrc->sBlob,&pDest->sBlob);` |
|  1904643 |  996 | `	}else{` |
|   730278 |  997 | `		if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|   267080 |  998 | `			SyBlobRelease(&pDest->sBlob);` |
|   133581 |  999 | `		}` |
|        - | 1000 | `	}` |
|  4539554 | 1001 | `	if( pMap ){` |
|    44363 | 1002 | `		PH7_HashmapUnref(pMap);` |
|  4517375 | 1003 | `	}else if( pObj ){` |
|     2459 | 1004 | `		PH7_ClassInstanceUnref(pObj);` |
|     1227 | 1005 | `	}` |
|  4539554 | 1006 | `	return rc;` |
|        5 | 1007 |  |
|        - | 1008 | `/*` |
|        - | 1009 | ` * Duplicate the contents of a ph7_value but do not copy internal` |
|        - | 1010 | ` * buffer contents,simply point to it.` |
|        - | 1011 | ` */` |
|  6100272 | 1012 | `PH7_PRIVATE sxi32 PH7_MemObjLoad(ph7_value *pSrc,ph7_value *pDest)` |
|        5 | 1013 |  |
|  6100277 | 1014 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),` |
|        - | 1015 | `		sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|  6100277 | 1016 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1017 | `		/* Increment reference count */` |
|   422567 | 1018 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  5888996 | 1019 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|        - | 1020 | `		/* Increment reference count */` |
|    12749 | 1021 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|     6372 | 1022 | `	}` |
|  6100277 | 1023 | `	if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|       32 | 1024 | `		SyBlobRelease(&pDest->sBlob);` |
|       14 | 1025 | `	}` |
|  6100277 | 1026 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  3308897 | 1027 | `		SyBlobReadOnly(&pDest->sBlob,SyBlobData(&pSrc->sBlob),SyBlobLength(&pSrc->sBlob));` |
|  1654565 | 1028 | `	}` |
|  6100277 | 1029 | `	return SXRET_OK;` |
|        5 | 1030 |  |
|        - | 1031 | `/*` |
|        - | 1032 | ` * Invalidate any prior representation of a given ph7_value.` |
|        - | 1033 | ` */` |
| 12081379 | 1034 | `PH7_PRIVATE sxi32 PH7_MemObjRelease(ph7_value *pObj)` |
|        5 | 1035 |  |
| 12081384 | 1036 | `	if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
| 11117231 | 1037 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|   533161 | 1038 | `			PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
| 10850653 | 1039 | `		}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|    22467 | 1040 | `			PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|    11231 | 1041 | `		}` |
|        - | 1042 | `		/* Release the internal buffer */` |
| 11117231 | 1043 | `		SyBlobRelease(&pObj->sBlob);` |
|        - | 1044 | `		/* Invalidate any prior representation */` |
| 11117231 | 1045 | `		pObj->iFlags = MEMOBJ_NULL;` |
|  5558990 | 1046 | `	}` |
| 12081384 | 1047 | `	return SXRET_OK;` |
|        5 | 1048 |  |
|        - | 1049 | `/*` |
|        - | 1050 | ` * Compare two ph7_values.` |
|        - | 1051 | ` * Return 0 if the values are equals, > 0 if pObj1 is greater than pObj2` |
|        - | 1052 | ` * or < 0 if pObj2 is greater than pObj1.` |
|        - | 1053 | ` * Type comparison table taken from the PHP language reference manual.` |
|        - | 1054 | ` * Comparisons of $x with PHP functions Expression` |
|        - | 1055 | ` *              gettype() 	empty() 	is_null() 	isset() 	boolean : if($x)` |
|        - | 1056 | ` * $x = ""; 	string 	    TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1057 | ` * $x = null 	NULL 	    TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1058 | ` * var $x; 	    NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1059 | ` * $x is undefined 	NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1060 | ` *  $x = array(); 	array 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1061 | ` * $x = false; 	boolean 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1062 | ` * $x = true; 	boolean 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1063 | ` * $x = 1; 	    integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1064 | ` * $x = 42; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1065 | ` * $x = 0; 	    integer 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1066 | ` * $x = -1; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1067 | ` * $x = "1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1068 | ` * $x = "0"; 	string 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1069 | ` * $x = "-1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1070 | ` * $x = "php"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1071 | ` * $x = "true"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1072 | ` * $x = "false"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1073 | ` *      Loose comparisons with ==` |
|        - | 1074 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|        - | 1075 | ` * TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1076 | ` * FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|        - | 1077 | ` * 1 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1078 | ` * 0 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE` |
|        - | 1079 | ` * -1 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1080 | ` * "1" 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1081 | ` * "0" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1082 | ` * "-1" 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1083 | ` * NULL 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|        - | 1084 | ` * array() 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1085 | ` * "php" 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1086 | ` * "" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE` |
|        - | 1087 | ` *    Strict comparisons with ===` |
|        - | 1088 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|        - | 1089 | ` * TRUE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1090 | ` * FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1091 | ` * 1 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1092 | ` * 0 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1093 | ` * -1 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1094 | ` * "1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1095 | ` * "0" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1096 | ` * "-1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1097 | ` * NULL 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE` |
|        - | 1098 | ` * array() 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE` |
|        - | 1099 | ` * "php" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1100 | ` * "" 	    FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE` |
|        - | 1101 | ` */` |
|  1186952 | 1102 | `PH7_PRIVATE sxi32 PH7_MemObjCmp(ph7_value *pObj1,ph7_value *pObj2,int bStrict,int iNest)` |
|        5 | 1103 |  |
|        - | 1104 | `	sxi32 iComb;` |
|        - | 1105 | `	sxi32 rc;` |
|  1186957 | 1106 | `	if( bStrict ){` |
|        - | 1107 | `		sxi32 iF1,iF2;` |
|        - | 1108 | `		/* Strict comparisons with === */` |
|   611315 | 1109 | `		iF1 = pObj1->iFlags&~MEMOBJ_AUX;` |
|   611315 | 1110 | `		iF2 = pObj2->iFlags&~MEMOBJ_AUX;` |
|   611315 | 1111 | `		if( iF1 != iF2 ){` |
|        - | 1112 | `			/* Not of the same type */` |
|   169923 | 1113 | `			return 1;` |
|        - | 1114 | `		}` |
|   220696 | 1115 | `	}` |
|        - | 1116 | `	/* Combine flag together */` |
|  1017039 | 1117 | `	iComb = pObj1->iFlags\|pObj2->iFlags;` |
|  1017039 | 1118 | `	if( iComb & (MEMOBJ_NULL\|MEMOBJ_RES\|MEMOBJ_BOOL) ){` |
|        - | 1119 | `		/* Convert to boolean: Keep in mind FALSE < TRUE */` |
|    20387 | 1120 | `		if( (pObj1->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     7137 | 1121 | `			PH7_MemObjToBool(pObj1);` |
|     3566 | 1122 | `		}` |
|    20387 | 1123 | `		if( (pObj2->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     6173 | 1124 | `			PH7_MemObjToBool(pObj2);` |
|     3084 | 1125 | `		}` |
|    20387 | 1126 | `		return (sxi32)((pObj1->x.iVal != 0) - (pObj2->x.iVal != 0));` |
|   996657 | 1127 | `	}else if ( iComb & MEMOBJ_HASHMAP ){` |
|        - | 1128 | `		/* Hashmap aka 'array' comparison */` |
|       27 | 1129 | `		if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1130 | `			/* Array is always greater */` |
|      ! 0 | 1131 | `			return -1;` |
|        - | 1132 | `		}` |
|       27 | 1133 | `		if( (pObj2->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1134 | `			/* Array is always greater */` |
|      ! 0 | 1135 | `			return 1;` |
|        - | 1136 | `		}` |
|        - | 1137 | `		/* Perform the comparison */` |
|       27 | 1138 | `		rc = PH7_HashmapCmp((ph7_hashmap *)pObj1->x.pOther,(ph7_hashmap *)pObj2->x.pOther,bStrict);` |
|       27 | 1139 | `		return rc;` |
|   996631 | 1140 | `	}else if(iComb & MEMOBJ_OBJ ){` |
|        - | 1141 | `		/* Object comparison */` |
|      177 | 1142 | `		if( (pObj1->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 1143 | `			/* Object is always greater */` |
|      ! 0 | 1144 | `			return -1;` |
|        - | 1145 | `		}` |
|      177 | 1146 | `		if( (pObj2->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 1147 | `			/* Object is always greater */` |
|      ! 0 | 1148 | `			return 1;` |
|        - | 1149 | `		}` |
|        - | 1150 | `		/* Perform the comparison */` |
|      177 | 1151 | `		rc = PH7_ClassInstanceCmp((ph7_class_instance *)pObj1->x.pOther,(ph7_class_instance *)pObj2->x.pOther,bStrict,iNest);` |
|      177 | 1152 | `		return rc;` |
|   996457 | 1153 | `	}else if ( iComb & MEMOBJ_STRING ){` |
|        - | 1154 | `		SyString s1,s2;` |
|   642731 | 1155 | `		if( !bStrict ){` |
|        - | 1156 | `			/*` |
|        - | 1157 | `			 * According to the PHP language reference manual:` |
|        - | 1158 | `			 *` |
|        - | 1159 | `			 *  If you compare a number with a string or the comparison involves numerical` |
|        - | 1160 | `			 *  strings, then each string is converted to a number and the comparison` |
|        - | 1161 | `			 *  performed numerically.` |
|        - | 1162 | `			 */` |
|   221601 | 1163 | `			if( PH7_MemObjIsNumeric(pObj1) ){` |
|        - | 1164 | `				/* Perform a numeric comparison */` |
|       13 | 1165 | `				goto Numeric;` |
|        - | 1166 | `			}` |
|   221589 | 1167 | `			if( PH7_MemObjIsNumeric(pObj2) ){` |
|        - | 1168 | `				/* Perform a numeric comparison */` |
|      ! 0 | 1169 | `				goto Numeric;` |
|        - | 1170 | `			}` |
|   110808 | 1171 | `		}` |
|        - | 1172 | `		/* Perform a strict string comparison.*/` |
|   642719 | 1173 | `		if( (pObj1->iFlags&MEMOBJ_STRING) == 0 ){` |
|      ! 0 | 1174 | `			PH7_MemObjToString(pObj1);` |
|      ! 0 | 1175 | `		}` |
|   642719 | 1176 | `		if( (pObj2->iFlags&MEMOBJ_STRING) == 0 ){` |
|      ! 0 | 1177 | `			PH7_MemObjToString(pObj2);` |
|      ! 0 | 1178 | `		}` |
|   642719 | 1179 | `		SyStringInitFromBuf(&s1,SyBlobData(&pObj1->sBlob),SyBlobLength(&pObj1->sBlob));` |
|   642719 | 1180 | `		SyStringInitFromBuf(&s2,SyBlobData(&pObj2->sBlob),SyBlobLength(&pObj2->sBlob));` |
|        - | 1181 | `		/*` |
|        - | 1182 | `		 * Strings are compared using memcmp(). If one value is an exact prefix of the` |
|        - | 1183 | `		 * other, then the shorter value is less than the longer value.` |
|        - | 1184 | `		 */` |
|   642719 | 1185 | `		rc = SyMemcmp((const void *)s1.zString,(const void *)s2.zString,SXMIN(s1.nByte,s2.nByte));` |
|   642719 | 1186 | `		if( rc == 0 ){` |
|   224504 | 1187 | `			if( s1.nByte != s2.nByte ){` |
|     1622 | 1188 | `				rc = s1.nByte < s2.nByte ? -1 : 1;` |
|      811 | 1189 | `			}` |
|   112252 | 1190 | `		}` |
|   642719 | 1191 | `		return rc;` |
|   353731 | 1192 | `	}else if( iComb & (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|   176821 | 1193 | `Numeric:` |
|        - | 1194 | `		/* Perform a numeric comparison if one of the operand is numeric(integer or real) */` |
|   353743 | 1195 | `		if( (pObj1->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|        3 | 1196 | `			PH7_MemObjToNumeric(pObj1);` |
|        1 | 1197 | `		}` |
|   353743 | 1198 | `		if( (pObj2->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|       11 | 1199 | `			PH7_MemObjToNumeric(pObj2);` |
|        5 | 1200 | `		}` |
|   353743 | 1201 | `		if( (pObj1->iFlags & pObj2->iFlags & MEMOBJ_INT) == 0) {` |
|        - | 1202 | `			/*` |
|        - | 1203 | `			 * Symisc eXtension to the PHP language:` |
|        - | 1204 | `			 *  Floating point comparison is introduced and works as expected.` |
|        - | 1205 | `			 */` |
|        - | 1206 | `			ph7_real r1,r2;` |
|        - | 1207 | `			/* Compare as reals */` |
|      201 | 1208 | `			if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|       11 | 1209 | `				PH7_MemObjToReal(pObj1);` |
|        5 | 1210 | `			}` |
|      201 | 1211 | `			r1 = pObj1->rVal;` |
|      201 | 1212 | `			if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|       37 | 1213 | `				PH7_MemObjToReal(pObj2);` |
|       18 | 1214 | `			}` |
|      201 | 1215 | `			r2 = pObj2->rVal;` |
|      201 | 1216 | `			if( PH7_IS_NAN(r1) \|\| PH7_IS_NAN(r2) ){` |
|        - | 1217 | `				/*` |
|        - | 1218 | `				 * Keep a strict three-way comparator contract even for NaN values.` |
|        - | 1219 | `				 * For ordering purposes, NaN compares equal to NaN and greater than` |
|        - | 1220 | `				 * any non-NaN numeric value.` |
|        - | 1221 | `				 */` |
|       45 | 1222 | `				if( PH7_IS_NAN(r1) ){` |
|       35 | 1223 | `					return PH7_IS_NAN(r2) ? 0 : 1;` |
|        - | 1224 | `				}` |
|       11 | 1225 | `				return -1;` |
|        - | 1226 | `			}` |
|      157 | 1227 | `			if( r1 > r2 ){` |
|       19 | 1228 | `				return 1;` |
|      139 | 1229 | `			}else if( r1 < r2 ){` |
|      109 | 1230 | `				return -1;` |
|        - | 1231 | `			}` |
|       31 | 1232 | `			return 0;` |
|      ! 0 | 1233 | `		}else{` |
|        - | 1234 | `			/* Integer comparison */` |
|   353543 | 1235 | `			if( pObj1->x.iVal > pObj2->x.iVal ){` |
|     2412 | 1236 | `				return 1;` |
|   351136 | 1237 | `			}else if( pObj1->x.iVal < pObj2->x.iVal ){` |
|   347073 | 1238 | `				return -1;` |
|        - | 1239 | `			}` |
|     4068 | 1240 | `			return 0;` |
|        - | 1241 | `		}` |
|        - | 1242 | `	}` |
|        - | 1243 | `	/* NOT REACHED */` |
|      ! 0 | 1244 | `	return 0;` |
|   593539 | 1245 |  |
|        - | 1246 | `/*` |
|        - | 1247 | ` * Perform an addition operation of two ph7_values.` |
|        - | 1248 | ` * The reason this function is implemented here rather than 'vm.c'` |
|        - | 1249 | ` * is that the '+' operator is overloaded.` |
|        - | 1250 | ` * That is,the '+' operator is used for arithmetic operation and also` |
|        - | 1251 | ` * used for operation on arrays [i.e: union]. When used with an array` |
|        - | 1252 | ` * The + operator returns the right-hand array appended to the left-hand array.` |
|        - | 1253 | ` * For keys that exist in both arrays, the elements from the left-hand array` |
|        - | 1254 | ` * will be used, and the matching elements from the right-hand array will` |
|        - | 1255 | ` * be ignored.` |
|        - | 1256 | ` * This function take care of handling all the scenarios.` |
|        - | 1257 | ` */` |
|     2110 | 1258 | `PH7_PRIVATE sxi32 PH7_MemObjAdd(ph7_value *pObj1,ph7_value *pObj2,int bAddStore)` |
|        5 | 1259 |  |
|     2115 | 1260 | `	if( ((pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1261 | `			/* Arithemtic operation */` |
|     2105 | 1262 | `			PH7_MemObjToNumeric(pObj1);` |
|     2105 | 1263 | `			PH7_MemObjToNumeric(pObj2);` |
|     2105 | 1264 | `			if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_REAL ){` |
|        - | 1265 | `				/* Floating point arithmetic */` |
|        - | 1266 | `				ph7_real a,b;` |
|       57 | 1267 | `				if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|       31 | 1268 | `					PH7_MemObjToReal(pObj1);` |
|       15 | 1269 | `				}` |
|       57 | 1270 | `				if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 | 1271 | `					PH7_MemObjToReal(pObj2);` |
|        1 | 1272 | `				}` |
|       57 | 1273 | `				a = pObj1->rVal;` |
|       57 | 1274 | `				b = pObj2->rVal;` |
|       57 | 1275 | `				pObj1->rVal = a+b;` |
|       57 | 1276 | `				MemObjSetType(pObj1,MEMOBJ_REAL);` |
|        - | 1277 | `				/* Try to get an integer representation also */` |
|       57 | 1278 | `				MemObjTryIntger(&(*pObj1));` |
|       29 | 1279 | `			}else{` |
|        - | 1280 | `				/* Integer arithmetic */` |
|        - | 1281 | `				sxi64 a,b;` |
|     2049 | 1282 | `				a = pObj1->x.iVal;` |
|     2049 | 1283 | `				b = pObj2->x.iVal;` |
|     2049 | 1284 | `				pObj1->x.iVal = a+b;` |
|     2049 | 1285 | `				MemObjSetType(pObj1,MEMOBJ_INT);` |
|        - | 1286 | `			}` |
|     1055 | 1287 | `	}else{` |
|       12 | 1288 | `		if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP ){` |
|        - | 1289 | `			ph7_hashmap *pMap;` |
|        - | 1290 | `			sxi32 rc;` |
|       12 | 1291 | `			if( bAddStore ){` |
|        - | 1292 | `				/* Do not duplicate the hashmap,use the left one since its an add&store operation.` |
|        - | 1293 | `				 */` |
|        3 | 1294 | `				if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1295 | `					/* Force a hashmap cast */` |
|      ! 0 | 1296 | `					rc = PH7_MemObjToHashmap(pObj1);` |
|      ! 0 | 1297 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 1298 | `						PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|      ! 0 | 1299 | `						return rc;` |
|        - | 1300 | `					}` |
|      ! 0 | 1301 | `				}` |
|        - | 1302 | `				/* COW separate before in-place mutation */` |
|        3 | 1303 | `				pMap = PH7_HashmapCowSeparate(pObj1->pVm,pObj1);` |
|        2 | 1304 | `			}else{` |
|        - | 1305 | `				/* Create a new hashmap */` |
|       10 | 1306 | `				pMap = PH7_NewHashmap(pObj1->pVm,0,0);` |
|       10 | 1307 | `				if( pMap == 0){` |
|      ! 0 | 1308 | `					PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|      ! 0 | 1309 | `					return SXERR_MEM;` |
|        - | 1310 | `				}` |
|        - | 1311 | `			}` |
|       12 | 1312 | `			if( !bAddStore ){` |
|       10 | 1313 | `				if(pObj1->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1314 | `					/* Perform a hashmap duplication */` |
|       10 | 1315 | `					PH7_HashmapDup((ph7_hashmap *)pObj1->x.pOther,pMap);` |
|        6 | 1316 | `				}else{` |
|      ! 0 | 1317 | `					if((pObj1->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 1318 | `						/* Simple insertion */` |
|      ! 0 | 1319 | `						PH7_HashmapInsert(pMap,0,pObj1);` |
|      ! 0 | 1320 | `					}` |
|        - | 1321 | `				}` |
|        4 | 1322 | `			}` |
|        - | 1323 | `			/* Perform the union */` |
|       12 | 1324 | `			if(pObj2->iFlags & MEMOBJ_HASHMAP ){` |
|       12 | 1325 | `				PH7_HashmapUnion(pMap,(ph7_hashmap *)pObj2->x.pOther);` |
|        7 | 1326 | `			}else{` |
|      ! 0 | 1327 | `				if((pObj2->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 1328 | `					/* Simple insertion */` |
|      ! 0 | 1329 | `					PH7_HashmapInsert(pMap,0,pObj2);` |
|      ! 0 | 1330 | `				}` |
|        - | 1331 | `			}` |
|        - | 1332 | `			/* Reflect the change */` |
|       12 | 1333 | `			if( pObj1->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 1334 | `				SyBlobRelease(&pObj1->sBlob);` |
|      ! 0 | 1335 | `			}` |
|       12 | 1336 | `			pObj1->x.pOther = pMap;` |
|       12 | 1337 | `			MemObjSetType(pObj1,MEMOBJ_HASHMAP);` |
|        5 | 1338 | `		}` |
|        - | 1339 | `	}` |
|     2115 | 1340 | `	return SXRET_OK;` |
|     1060 | 1341 |  |
|        - | 1342 | `/*` |
|        - | 1343 | ` * Return a printable representation of the type of a given` |
|        - | 1344 | ` * ph7_value.` |
|        - | 1345 | ` */` |
|      426 | 1346 | `PH7_PRIVATE const char * PH7_MemObjTypeDump(ph7_value *pVal)` |
|        5 | 1347 |  |
|      431 | 1348 | `	const char *zType = "";` |
|      431 | 1349 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|        7 | 1350 | `		zType = "null";` |
|      428 | 1351 | `	}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|        - | 1352 | `		/* REAL is authoritative over a cached MEMOBJ_INT: an integer-valued` |
|        - | 1353 | `		 * real (e.g. 1.0) is reported as "double", matching PHP's gettype(). */` |
|        7 | 1354 | `		zType = "double";` |
|      422 | 1355 | `	}else if( pVal->iFlags & MEMOBJ_INT ){` |
|      101 | 1356 | `		zType = "int";` |
|      370 | 1357 | `	}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|       64 | 1358 | `		zType = "string";` |
|      291 | 1359 | `	}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|      111 | 1360 | `		zType = "bool";` |
|      206 | 1361 | `	}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|       16 | 1362 | `		zType = "array";` |
|      144 | 1363 | `	}else if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      137 | 1364 | `		zType = "object";` |
|       67 | 1365 | `	}else if( pVal->iFlags & MEMOBJ_RES ){` |
|      ! 0 | 1366 | `		zType = "resource";` |
|      ! 0 | 1367 | `	}` |
|      431 | 1368 | `	return zType;` |
|        5 | 1369 |  |
|        - | 1370 | `/*` |
|        - | 1371 | ` * Dump a ph7_value [i.e: get a printable representation of it's type and contents.].` |
|        - | 1372 | ` * Store the dump in the given blob.` |
|        - | 1373 | ` */` |
|      484 | 1374 | `PH7_PRIVATE sxi32 PH7_MemObjDump(` |
|        - | 1375 | `	SyBlob *pOut,      /* Store the dump here */` |
|        - | 1376 | `	ph7_value *pObj,   /* Dump this */` |
|        - | 1377 | `	int ShowType,      /* TRUE to output value type */` |
|        - | 1378 | `	int nTab,          /* # of Whitespace to insert */` |
|        - | 1379 | `	int nDepth,        /* Nesting level */` |
|        - | 1380 | `	int isRef          /* TRUE if referenced object */` |
|        - | 1381 | `	)` |
|        5 | 1382 |  |
|      489 | 1383 | `	sxi32 rc = SXRET_OK;` |
|        - | 1384 | `	const char *zType;` |
|        - | 1385 | `	int i;` |
|     4685 | 1386 | `	for( i = 0 ; i < nTab ; i++ ){` |
|     4200 | 1387 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     2102 | 1388 | `	}` |
|      489 | 1389 | `	if( ShowType ){` |
|      399 | 1390 | `		if( isRef ){` |
|      ! 0 | 1391 | `			SyBlobAppend(&(*pOut),"&",sizeof(char));` |
|      ! 0 | 1392 | `		}` |
|        - | 1393 | `		/* Get value type first. var_dump() labels reals "float" (PHP), whereas` |
|        - | 1394 | `		 * gettype()/PH7_MemObjTypeDump use the legacy "double" spelling. */` |
|      399 | 1395 | `		if( (pObj->iFlags & MEMOBJ_REAL) && (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        3 | 1396 | `			zType = "float";` |
|        2 | 1397 | `		}else{` |
|      397 | 1398 | `			zType = PH7_MemObjTypeDump(pObj);` |
|        - | 1399 | `		}` |
|      399 | 1400 | `		SyBlobAppend(&(*pOut),zType,SyStrlen(zType));` |
|      197 | 1401 | `	}` |
|      489 | 1402 | `	if((pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|      485 | 1403 | `		if ( ShowType ){` |
|      395 | 1404 | `			SyBlobAppend(&(*pOut),"(",sizeof(char));` |
|      195 | 1405 | `		}` |
|      485 | 1406 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1407 | `			/* Dump hashmap entries */` |
|       26 | 1408 | `			rc = PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,ShowType,nTab+1,nDepth+1);` |
|      473 | 1409 | `		}else if(pObj->iFlags & MEMOBJ_OBJ ){` |
|        - | 1410 | `			/* Dump class instance attributes */` |
|      141 | 1411 | `			rc = PH7_ClassInstanceDump(&(*pOut),(ph7_class_instance *)pObj->x.pOther,ShowType,nTab+1,nDepth+1);` |
|       72 | 1412 | `		}else{` |
|      323 | 1413 | `			SyBlob *pContents = &pObj->sBlob;` |
|        - | 1414 | `			/* Get a printable representation of the contents */` |
|      323 | 1415 | `			if((pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|      226 | 1416 | `				MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|      115 | 1417 | `			}else{` |
|        - | 1418 | `				/* PHP format: string(N) "content" */` |
|      100 | 1419 | `				if( ShowType ){` |
|       42 | 1420 | `					SyBlobFormat(&(*pOut),"%u) \"",SyBlobLength(&pObj->sBlob));` |
|       19 | 1421 | `				}` |
|      100 | 1422 | `				if( SyBlobLength(pContents) > 0 ){` |
|       90 | 1423 | `					SyBlobAppend(&(*pOut),SyBlobData(pContents),SyBlobLength(pContents));` |
|       43 | 1424 | `				}` |
|      100 | 1425 | `				if( ShowType ){` |
|       42 | 1426 | `					SyBlobAppend(&(*pOut),"\"",sizeof(char));` |
|       19 | 1427 | `				}` |
|        - | 1428 | `			}` |
|        - | 1429 | `		}` |
|      485 | 1430 | `		if( ShowType ){` |
|        - | 1431 | `			/* Strings already emitted their own ')' as part of the` |
|        - | 1432 | `			 * "N) \"content\"" format above. */` |
|      395 | 1433 | `			if( (pObj->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_STRING)) == 0 ){` |
|      210 | 1434 | `				SyBlobAppend(&(*pOut),")",sizeof(char));` |
|      103 | 1435 | `			}` |
|      195 | 1436 | `		}` |
|      240 | 1437 | `	}` |
|        - | 1438 | `#ifdef __WINNT__` |
|        5 | 1439 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|        - | 1440 | `#else` |
|      484 | 1441 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        - | 1442 | `#endif` |
|      489 | 1443 | `	return rc;` |
|        5 | 1444 |  |
|        - | 1445 |  |
