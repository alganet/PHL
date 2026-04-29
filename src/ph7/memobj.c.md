# src/ph7/memobj.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 703/791 lines (88.87%)

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
|      258 |   11 | `PH7_PRIVATE const char *ph7_type_name(ph7_value *pVal)` |
|        2 |   12 |  |
|      260 |   13 | `	if( ph7_value_is_null(pVal) ) return "null";` |
|      250 |   14 | `	if( ph7_value_is_bool(pVal) ) return "bool";` |
|      242 |   15 | `	if( ph7_value_is_int(pVal) ) return "int";` |
|      214 |   16 | `	if( ph7_value_is_float(pVal) ) return "float";` |
|      212 |   17 | `	if( ph7_value_is_string(pVal) ) return "string";` |
|       92 |   18 | `	if( ph7_value_is_array(pVal) ) return "array";` |
|       26 |   19 | `	if( ph7_value_is_object(pVal) ) return "object";` |
|        3 |   20 | `	if( ph7_value_is_resource(pVal) ) return "resource";` |
|      ! 0 |   21 | `	return "unknown";` |
|      131 |   22 |  |
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
|     1934 |   40 | `static sxi64 MemObjRealToInt(ph7_value *pObj)` |
|        2 |   41 |  |
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
|     1936 |   57 | `  ph7_real r = pObj->rVal;` |
|     1936 |   58 | `  if( r<(ph7_real)minInt ){` |
|      ! 0 |   59 | `    return minInt;` |
|     1936 |   60 | `  }else if( r>(ph7_real)maxInt ){` |
|        - |   61 | `    /* minInt is correct here - not maxInt.  It turns out that assigning` |
|        - |   62 | `    ** a very large positive number to an integer results in a very large` |
|        - |   63 | `    ** negative integer.  This makes no sense, but it is what x86 hardware` |
|        - |   64 | `    ** does so for compatibility we will do the same in software. */` |
|       23 |   65 | `    return minInt;` |
|      ! 0 |   66 | `  }else{` |
|     1914 |   67 | `    return (sxi64)r;` |
|        - |   68 | `  }` |
|        - |   69 | `#endif` |
|      969 |   70 |  |
|        - |   71 | `/*` |
|        - |   72 | ` * Convert a raw token value typically a stream of digit [i.e: hex,octal,binary or decimal]` |
|        - |   73 | ` * to a 64-bit integer.` |
|        - |   74 | ` */` |
|   105422 |   75 | `PH7_PRIVATE sxi64 PH7_TokenValueToInt64(SyString *pVal)` |
|        2 |   76 |  |
|   105424 |   77 | `	sxi64 iVal = 0;` |
|   105424 |   78 | `	if( pVal->nByte <= 0 ){` |
|        7 |   79 | `		return 0;` |
|        - |   80 | `	}` |
|   105418 |   81 | `	if( pVal->zString[0] == '0' ){` |
|        - |   82 | `		sxi32 c;` |
|    43740 |   83 | `		if( pVal->nByte == sizeof(char) ){` |
|    43338 |   84 | `			return 0;` |
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
|    61680 |   99 | `		SyStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|        - |  100 | `	}` |
|    62082 |  101 | `	return iVal;` |
|    52713 |  102 |  |
|        - |  103 | `/*` |
|        - |  104 | ` * Return some kind of 64-bit integer value which is the best we can` |
|        - |  105 | ` * do at representing the value that pObj describes as a string` |
|        - |  106 | ` * representation.` |
|        - |  107 | ` */` |
|      392 |  108 | `static sxi64 MemObjStringToInt(ph7_value *pObj)` |
|        2 |  109 |  |
|        - |  110 | `	SyString sVal;` |
|      394 |  111 | `	SyStringInitFromBuf(&sVal,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|      394 |  112 | `	return PH7_TokenValueToInt64(&sVal);` |
|        2 |  113 |  |
|        - |  114 | `/*` |
|        - |  115 | ` * Call a magic class method [i.e: __toString(),__toInt(),...]` |
|        - |  116 | ` * Return SXRET_OK if the magic method is available and have been` |
|        - |  117 | ` * successfully called. Any other return value indicates failure.` |
|        - |  118 | ` */` |
|       86 |  119 | `static sxi32 MemObjCallClassCastMethod(` |
|        - |  120 | `	ph7_vm *pVm,               /* VM that trigger the invocation */` |
|        - |  121 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object] */` |
|        - |  122 | `	const char *zMethod,       /* Magic method name [i.e: __toString] */` |
|        - |  123 | `	sxu32 nLen,                /* Method name length */` |
|        - |  124 | `	ph7_value *pResult         /* OUT: Store the return value of the magic method here */` |
|        - |  125 | `	)` |
|        2 |  126 |  |
|        - |  127 | `	ph7_class_method *pMethod;` |
|        - |  128 | `	/* Check if the method is available */` |
|       88 |  129 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,zMethod,nLen);` |
|       88 |  130 | `	if( pMethod == 0 ){` |
|        - |  131 | `		/* No such method */` |
|        3 |  132 | `		return SXERR_NOTFOUND;` |
|        - |  133 | `	}` |
|        - |  134 | `	/* Invoke the desired method */` |
|       86 |  135 | `	PH7_VmCallClassMethod(&(*pVm),&(*pThis),pMethod,&(*pResult),0,0);` |
|        - |  136 | `	/* Method successfully called,pResult should hold the return value */` |
|       86 |  137 | `	return SXRET_OK;` |
|       45 |  138 |  |
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
|        2 |  149 |  |
|        - |  150 | `	sxi32 iFlags;` |
|      482 |  151 | `	iFlags = pObj->iFlags;` |
|      482 |  152 | `	if (iFlags & MEMOBJ_REAL ){` |
|       31 |  153 | `		return MemObjRealToInt(&(*pObj));` |
|      452 |  154 | `	}else if( iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      117 |  155 | `		return pObj->x.iVal;` |
|      336 |  156 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|      314 |  157 | `		return MemObjStringToInt(&(*pObj));` |
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
|      242 |  186 |  |
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
|     1182 |  197 | `static ph7_real MemObjRealValue(ph7_value *pObj)` |
|        2 |  198 |  |
|        - |  199 | `	sxi32 iFlags;` |
|     1184 |  200 | `	iFlags = pObj->iFlags;` |
|     1184 |  201 | `	if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  202 | `		return pObj->rVal;` |
|     1184 |  203 | `	}else if (iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      636 |  204 | `		return (ph7_real)pObj->x.iVal;` |
|      550 |  205 | `	}else if (iFlags & MEMOBJ_STRING){` |
|        - |  206 | `		SyString sString;` |
|        - |  207 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  208 | `		ph7_real rVal = 0;` |
|        - |  209 | `#else` |
|      544 |  210 | `		ph7_real rVal = 0.0;` |
|        - |  211 | `#endif` |
|      544 |  212 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|      544 |  213 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|        - |  214 | `			/* Convert as much as we can */` |
|        - |  215 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  216 | `			rVal = MemObjStringToInt(&(*pObj));` |
|        - |  217 | `#else` |
|      544 |  218 | `			SyStrToReal(sString.zString,sString.nByte,(void *)&rVal,0);` |
|        - |  219 | `#endif` |
|      271 |  220 | `		}` |
|      544 |  221 | `		return rVal;` |
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
|      593 |  254 |  |
|        - |  255 | `/*` |
|        - |  256 | ` * Return the string representation of a given ph7_value.` |
|        - |  257 | ` * This function never fail and always return SXRET_OK.` |
|        - |  258 | ` */` |
|    54364 |  259 | `static sxi32 MemObjStringValue(SyBlob *pOut,ph7_value *pObj,sxu8 bStrictBool)` |
|        2 |  260 |  |
|    54366 |  261 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|        - |  262 | `		/* Handle special floating-point values first */` |
|      236 |  263 | `		if( PH7_IS_NAN(pObj->rVal) ){` |
|      ! 0 |  264 | `			SyBlobAppend(&(*pOut),"NAN",3);` |
|      236 |  265 | `		}else if( PH7_IS_INF(pObj->rVal) ){` |
|      ! 0 |  266 | `			if( pObj->rVal < 0.0 ){` |
|      ! 0 |  267 | `				SyBlobAppend(&(*pOut),"-INF",4);` |
|      ! 0 |  268 | `			}else{` |
|      ! 0 |  269 | `				SyBlobAppend(&(*pOut),"INF",3);` |
|        - |  270 | `			}` |
|      ! 0 |  271 | `		}else{` |
|      236 |  272 | `			SyBlobFormat(&(*pOut),"%.15g",pObj->rVal);` |
|        2 |  273 | `		}` |
|    54249 |  274 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|    53776 |  275 | `		SyBlobFormat(&(*pOut),"%qd",pObj->x.iVal);` |
|        - |  276 | `		/* %qd (BSD quad) is equivalent to %lld in the libc printf */` |
|    27245 |  277 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|      238 |  278 | `		if( bStrictBool ){` |
|        - |  279 | `			/* Actual string cast: true -> "1", false -> "" (like PHP) */` |
|      130 |  280 | `			if( pObj->x.iVal ){` |
|       18 |  281 | `				SyBlobAppend(&(*pOut),"1",sizeof("1")-1);` |
|        8 |  282 | `			}` |
|        - |  283 | `			/* false produces empty string, nothing to append */` |
|       66 |  284 | `		}else{` |
|        - |  285 | `			/* Display path (var_dump, print_r): show TRUE/FALSE */` |
|      110 |  286 | `			if( pObj->x.iVal ){` |
|       64 |  287 | `				SyBlobAppend(&(*pOut),"TRUE",sizeof("TRUE")-1);` |
|       33 |  288 | `			}else{` |
|       48 |  289 | `				SyBlobAppend(&(*pOut),"FALSE",sizeof("FALSE")-1);` |
|        - |  290 | `			}` |
|        2 |  291 | `		}` |
|      240 |  292 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|        3 |  293 | `		SyBlobAppend(&(*pOut),"Array",sizeof("Array")-1);` |
|        3 |  294 | `		PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
|      121 |  295 | `	}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|        - |  296 | `		ph7_value sResult;` |
|        - |  297 | `		sxi32 rc;` |
|        - |  298 | `		/* Invoke the __toString() method if available */` |
|       74 |  299 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|       74 |  300 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|        - |  301 | `			"__toString",sizeof("__toString")-1,&sResult);` |
|       74 |  302 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_STRING) && SyBlobLength(&sResult.sBlob) > 0){` |
|        - |  303 | `			/* Expand method return value */` |
|       70 |  304 | `			SyBlobDup(&sResult.sBlob,pOut);` |
|       36 |  305 | `		}else{` |
|        - |  306 | `			/* Expand "Object" as requested by the PHP language reference manual */` |
|        5 |  307 | `			SyBlobAppend(&(*pOut),"Object",sizeof("Object")-1);` |
|        - |  308 | `		}` |
|       74 |  309 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|       74 |  310 | `		PH7_MemObjRelease(&sResult);` |
|       84 |  311 | `	}else if(pObj->iFlags & MEMOBJ_RES ){` |
|        3 |  312 | `		SyBlobFormat(&(*pOut),"ResourceID_%#x",pObj->x.pOther);` |
|        1 |  313 | `	}` |
|    54366 |  314 | `	return SXRET_OK;` |
|        2 |  315 |  |
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
|    12242 |  328 | `static sxi32 MemObjBooleanValue(ph7_value *pObj)` |
|        2 |  329 |  |
|        - |  330 | `	sxi32 iFlags;` |
|    12244 |  331 | `	iFlags = pObj->iFlags;` |
|    12244 |  332 | `	if (iFlags & MEMOBJ_REAL ){` |
|        - |  333 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  334 | `		return pObj->rVal ? 1 : 0;` |
|        - |  335 | `#else` |
|       12 |  336 | `		return pObj->rVal != 0.0 ? 1 : 0;` |
|        - |  337 | `#endif` |
|    12234 |  338 | `	}else if( iFlags & MEMOBJ_INT ){` |
|      110 |  339 | `		return pObj->x.iVal ? 1 : 0;` |
|    12126 |  340 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|        - |  341 | `		SyString sString;` |
|       66 |  342 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|       66 |  343 | `		if( sString.nByte == 0 ){` |
|        - |  344 | `			/* Empty string */` |
|       16 |  345 | `			return 0;` |
|       53 |  346 | `		}else if( (sString.nByte == sizeof("true") - 1 && SyStrnicmp(sString.zString,"true",sizeof("true")-1) == 0) \|\|` |
|       50 |  347 | `			(sString.nByte == sizeof("on") - 1 && SyStrnicmp(sString.zString,"on",sizeof("on")-1) == 0) \|\|` |
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
|    12062 |  361 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|    11084 |  362 | `		return 0;` |
|      980 |  363 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|       20 |  364 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|       20 |  365 | `		sxu32 n = pMap->nEntry;` |
|       20 |  366 | `		PH7_HashmapUnref(pMap);` |
|       20 |  367 | `		return n > 0 ? TRUE : FALSE;` |
|      962 |  368 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
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
|      956 |  383 | `	}else if(iFlags & MEMOBJ_RES ){` |
|      956 |  384 | `		return pObj->x.pOther != 0;` |
|        - |  385 | `	}` |
|        - |  386 | `	/* NOT REACHED */` |
|      ! 0 |  387 | `	return 0;` |
|     6123 |  388 |  |
|        - |  389 | `/*` |
|        - |  390 | ` * If the ph7_value is of type real,try to make it an integer also.` |
|        - |  391 | ` */` |
|     1904 |  392 | `static sxi32 MemObjTryIntger(ph7_value *pObj)` |
|        2 |  393 |  |
|     1906 |  394 | `	pObj->x.iVal = MemObjRealToInt(&(*pObj));` |
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
|     1904 |  407 | `	if( pObj->rVal ==(ph7_real)pObj->x.iVal && pObj->x.iVal>SMALLEST_INT64` |
|     1089 |  408 | `      && pObj->x.iVal<LARGEST_INT64 ){` |
|     1088 |  409 | `		  pObj->iFlags \|= MEMOBJ_INT;` |
|      545 |  410 | `	}` |
|     1906 |  411 | `	return SXRET_OK;` |
|        2 |  412 |  |
|        - |  413 | `/*` |
|        - |  414 | ` * Convert a ph7_value to type integer.Invalidate any prior representations.` |
|        - |  415 | ` */` |
|   342420 |  416 | `PH7_PRIVATE sxi32 PH7_MemObjToInteger(ph7_value *pObj)` |
|        2 |  417 |  |
|   342422 |  418 | `	if( (pObj->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  419 | `		/* Preform the conversion */` |
|      482 |  420 | `		pObj->x.iVal = MemObjIntValue(&(*pObj));` |
|        - |  421 | `		/* Invalidate any prior representations */` |
|      482 |  422 | `		SyBlobRelease(&pObj->sBlob);` |
|      482 |  423 | `		MemObjSetType(pObj,MEMOBJ_INT);` |
|      240 |  424 | `	}` |
|   342422 |  425 | `	return SXRET_OK;` |
|        2 |  426 |  |
|        - |  427 | `/*` |
|        - |  428 | ` * Convert a ph7_value to type real (Try to get an integer representation also).` |
|        - |  429 | ` * Invalidate any prior representations` |
|        - |  430 | ` */` |
|     1418 |  431 | `PH7_PRIVATE sxi32 PH7_MemObjToReal(ph7_value *pObj)` |
|        2 |  432 |  |
|     1420 |  433 | `	if((pObj->iFlags & MEMOBJ_REAL) == 0 ){` |
|        - |  434 | `		/* Preform the conversion */` |
|     1184 |  435 | `		pObj->rVal = MemObjRealValue(&(*pObj));` |
|        - |  436 | `		/* Invalidate any prior representations */` |
|     1184 |  437 | `		SyBlobRelease(&pObj->sBlob);` |
|     1184 |  438 | `		MemObjSetType(pObj,MEMOBJ_REAL);` |
|        - |  439 | `		/* Try to get an integer representation */` |
|     1184 |  440 | `		MemObjTryIntger(&(*pObj));` |
|      591 |  441 | `	}` |
|     1420 |  442 | `	return SXRET_OK;` |
|        2 |  443 |  |
|        - |  444 | `/*` |
|        - |  445 | ` * Convert a ph7_value to type boolean.Invalidate any prior representations.` |
|        - |  446 | ` */` |
|    12466 |  447 | `PH7_PRIVATE sxi32 PH7_MemObjToBool(ph7_value *pObj)` |
|        2 |  448 |  |
|    12468 |  449 | `	if( (pObj->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        - |  450 | `		/* Preform the conversion */` |
|    12244 |  451 | `		pObj->x.iVal = MemObjBooleanValue(&(*pObj));` |
|        - |  452 | `		/* Invalidate any prior representations */` |
|    12244 |  453 | `		SyBlobRelease(&pObj->sBlob);` |
|    12244 |  454 | `		MemObjSetType(pObj,MEMOBJ_BOOL);` |
|     6121 |  455 | `	}` |
|    12468 |  456 | `	return SXRET_OK;` |
|        2 |  457 |  |
|        - |  458 | `/*` |
|        - |  459 | ` * Convert a ph7_value to type string.Prior representations are NOT invalidated.` |
|        - |  460 | ` */` |
|   698130 |  461 | `PH7_PRIVATE sxi32 PH7_MemObjToString(ph7_value *pObj)` |
|        2 |  462 |  |
|   698132 |  463 | `	sxi32 rc = SXRET_OK;` |
|   698132 |  464 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  465 | `		/* Perform the conversion */` |
|    54146 |  466 | `		SyBlobReset(&pObj->sBlob); /* Reset the internal buffer */` |
|    54146 |  467 | `		rc = MemObjStringValue(&pObj->sBlob,&(*pObj),TRUE);` |
|    54146 |  468 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|    27072 |  469 | `	}` |
|   698132 |  470 | `	return rc;` |
|        2 |  471 |  |
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
|      116 |  487 | `PH7_PRIVATE sxi32 PH7_MemObjToHashmap(ph7_value *pObj)` |
|        2 |  488 |  |
|      118 |  489 | `	if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  490 | `		ph7_hashmap *pMap;` |
|        - |  491 | `		/* Allocate a new hashmap instance */` |
|       82 |  492 | `		pMap = PH7_NewHashmap(pObj->pVm,0,0);` |
|       82 |  493 | `		if( pMap == 0 ){` |
|      ! 0 |  494 | `			return SXERR_MEM;` |
|        - |  495 | `		}` |
|       82 |  496 | `		if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|        - |  497 | `			/*` |
|        - |  498 | `			 * According to the PHP language reference manual.` |
|        - |  499 | `			 *   For any of the types: integer, float, string, boolean converting a value` |
|        - |  500 | `			 *   to an array results in an array with a single element with index zero` |
|        - |  501 | `			 *   and the value of the scalar which was converted.` |
|        - |  502 | `			 */` |
|       19 |  503 | `			if( pObj->iFlags & MEMOBJ_OBJ ){` |
|        - |  504 | `				/* Object cast */` |
|        7 |  505 | `				PH7_ClassInstanceToHashmap((ph7_class_instance *)pObj->x.pOther,pMap);` |
|        4 |  506 | `			}else{` |
|        - |  507 | `				/* Insert a single element */` |
|       13 |  508 | `				PH7_HashmapInsert(pMap,0/* Automatic index assign */,&(*pObj));` |
|        - |  509 | `			}` |
|       19 |  510 | `			SyBlobRelease(&pObj->sBlob);` |
|        9 |  511 | `		}` |
|        - |  512 | `		/* Invalidate any prior representation */` |
|       82 |  513 | `		PH7_MemObjRelease(pObj);` |
|       82 |  514 | `		MemObjSetType(pObj,MEMOBJ_HASHMAP);` |
|       82 |  515 | `		pObj->x.pOther = pMap;` |
|       40 |  516 | `	}` |
|      118 |  517 | `	return SXRET_OK;` |
|       60 |  518 |  |
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
|       52 |  588 | `PH7_PRIVATE ProcMemObjCast PH7_MemObjCastMethod(sxi32 iFlags)` |
|        2 |  589 |  |
|       54 |  590 | `	if( iFlags & MEMOBJ_STRING ){` |
|       16 |  591 | `		return PH7_MemObjToString;` |
|       40 |  592 | `	}else if( iFlags & MEMOBJ_INT ){` |
|       32 |  593 | `		return PH7_MemObjToInteger;` |
|       10 |  594 | `	}else if( iFlags & MEMOBJ_REAL ){` |
|        8 |  595 | `		return PH7_MemObjToReal;` |
|        3 |  596 | `	}else if( iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  597 | `		return PH7_MemObjToBool;` |
|        3 |  598 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|        3 |  599 | `		return PH7_MemObjToHashmap;` |
|      ! 0 |  600 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  601 | `		return PH7_MemObjToObject;` |
|        - |  602 | `	}` |
|        - |  603 | `	/* NULL cast */` |
|      ! 0 |  604 | `	return PH7_MemObjToNull;` |
|       28 |  605 |  |
|        - |  606 | `/*` |
|        - |  607 | ` * Check whether the ph7_value is numeric [i.e: int/float/bool] or looks` |
|        - |  608 | ` * like a numeric number [i.e: if the ph7_value is of type string.].` |
|        - |  609 | ` * Return TRUE if numeric.FALSE otherwise.` |
|        - |  610 | ` */` |
|   393786 |  611 | `PH7_PRIVATE sxi32 PH7_MemObjIsNumeric(ph7_value *pObj)` |
|        2 |  612 |  |
|   393788 |  613 | `	if( pObj->iFlags & ( MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      200 |  614 | `		return TRUE;` |
|   393590 |  615 | `	}else if( pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|       16 |  616 | `		return FALSE;` |
|   393576 |  617 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        - |  618 | `		SyString sStr;` |
|        - |  619 | `		sxi32 rc;` |
|   393576 |  620 | `		SyStringInitFromBuf(&sStr,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|   393576 |  621 | `		if( sStr.nByte <= 0 ){` |
|        - |  622 | `			/* Empty string */` |
|       73 |  623 | `			return FALSE;` |
|        - |  624 | `		}` |
|        - |  625 | `		/* Check if the string representation looks like a numeric number */` |
|   393504 |  626 | `		rc = SyStrIsNumeric(sStr.zString,sStr.nByte,0,0);` |
|   393504 |  627 | `		return rc == SXRET_OK ? TRUE : FALSE;` |
|        - |  628 | `	}` |
|        - |  629 | `	/* NOT REACHED */` |
|      ! 0 |  630 | `	return FALSE;` |
|   196875 |  631 |  |
|        - |  632 | `/*` |
|        - |  633 | ` * Check whether the ph7_value is empty.Return TRUE if empty.` |
|        - |  634 | ` * FALSE otherwise.` |
|        - |  635 | ` * An ph7_value is considered empty if the following are true:` |
|        - |  636 | ` * NULL value.` |
|        - |  637 | ` * Boolean FALSE.` |
|        - |  638 | ` * Integer/Float with a 0 (zero) value.` |
|        - |  639 | ` * An empty string or a stream of 0 (zero) [i.e: "0","00","000",...].` |
|        - |  640 | ` * An empty array.` |
|        - |  641 | ` * NOTE` |
|        - |  642 | ` *  OBJECT VALUE MUST NOT BE MODIFIED.` |
|        - |  643 | ` */` |
|    24590 |  644 | `PH7_PRIVATE sxi32 PH7_MemObjIsEmpty(ph7_value *pObj)` |
|        2 |  645 |  |
|    24592 |  646 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|       10 |  647 | `		return TRUE;` |
|    24584 |  648 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|       13 |  649 | `		return pObj->x.iVal == 0 ? TRUE : FALSE;` |
|    24572 |  650 | `	}else if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  651 | `		return pObj->rVal == (ph7_real)0 ? TRUE : FALSE;` |
|    24572 |  652 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|        5 |  653 | `		return !pObj->x.iVal;` |
|    24568 |  654 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|    24536 |  655 | `		if( SyBlobLength(&pObj->sBlob) <= 0 ){` |
|    20148 |  656 | `			return TRUE;` |
|      ! 0 |  657 | `		}else{` |
|        - |  658 | `			const char *zIn,*zEnd;` |
|     4390 |  659 | `			zIn = (const char *)SyBlobData(&pObj->sBlob);` |
|     4390 |  660 | `			zEnd = &zIn[SyBlobLength(&pObj->sBlob)];` |
|     4396 |  661 | `			while( zIn < zEnd ){` |
|     4396 |  662 | `				if( zIn[0] != '0' ){` |
|     4390 |  663 | `					break;` |
|        - |  664 | `				}` |
|        7 |  665 | `				zIn++;` |
|        1 |  666 | `			}` |
|     4390 |  667 | `			return zIn >= zEnd ? TRUE : FALSE;` |
|      ! 0 |  668 | `		}` |
|       34 |  669 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       34 |  670 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|       34 |  671 | `		return pMap->nEntry == 0 ? TRUE : FALSE;` |
|      ! 0 |  672 | `	}else if ( pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|      ! 0 |  673 | `		return FALSE;` |
|        - |  674 | `	}` |
|        - |  675 | `	/* Assume empty by default */` |
|      ! 0 |  676 | `	return TRUE;` |
|    12297 |  677 |  |
|        - |  678 | `/*` |
|        - |  679 | ` * Convert a ph7_value so that it has types MEMOBJ_REAL or MEMOBJ_INT` |
|        - |  680 | ` * or both.` |
|        - |  681 | ` * Invalidate any prior representations. Every effort is made to force` |
|        - |  682 | ` * the conversion, even if the input is a string that does not look` |
|        - |  683 | ` * completely like a number.Convert as much of the string as we can` |
|        - |  684 | ` * and ignore the rest.` |
|        - |  685 | ` */` |
|   399956 |  686 | `PH7_PRIVATE sxi32 PH7_MemObjToNumeric(ph7_value *pObj)` |
|        2 |  687 |  |
|   399958 |  688 | `	if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|   399872 |  689 | `		if( pObj->iFlags & (MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|        3 |  690 | `			if( pObj->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  691 | `				pObj->x.iVal = 0;` |
|      ! 0 |  692 | `			}` |
|        3 |  693 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|        1 |  694 | `		}` |
|        - |  695 | `		/* Already numeric */` |
|   399872 |  696 | `		return  SXRET_OK;` |
|        - |  697 | `	}` |
|       87 |  698 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|       87 |  699 | `		sxi32 rc = SXERR_INVALID;` |
|       87 |  700 | `		sxu8 bReal = FALSE;` |
|        - |  701 | `		SyString sString;` |
|       87 |  702 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|        - |  703 | `		/* Check if the given string looks like a numeric number */` |
|       87 |  704 | `		if( sString.nByte > 0 ){` |
|       87 |  705 | `			rc = SyStrIsNumeric(sString.zString,sString.nByte,&bReal,0);` |
|       43 |  706 | `		}` |
|       87 |  707 | `		if( bReal ){` |
|        7 |  708 | `			PH7_MemObjToReal(&(*pObj));` |
|        4 |  709 | `		}else{` |
|       81 |  710 | `			if( rc != SXRET_OK ){` |
|        - |  711 | `				/* The input does not look at all like a number,set the value to 0 */` |
|      ! 0 |  712 | `				pObj->x.iVal = 0;` |
|      ! 0 |  713 | `			}else{` |
|        - |  714 | `				/* Convert as much as we can */` |
|       81 |  715 | `				pObj->x.iVal = MemObjStringToInt(&(*pObj));` |
|        - |  716 | `			}` |
|       81 |  717 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|       81 |  718 | `			SyBlobRelease(&pObj->sBlob);` |
|        1 |  719 | `		}` |
|       43 |  720 | `	}else if(pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)){` |
|      ! 0 |  721 | `		PH7_MemObjToInteger(pObj);` |
|      ! 0 |  722 | `	}else{` |
|        - |  723 | `		/* Perform a blind cast */` |
|      ! 0 |  724 | `		PH7_MemObjToReal(&(*pObj));` |
|        - |  725 | `	}` |
|       87 |  726 | `	return SXRET_OK;` |
|   200002 |  727 |  |
|        - |  728 | `/*` |
|        - |  729 | ` * Apply Perl-style increment to a string ph7_value in place.` |
|        - |  730 | ` * Walks the bytes right-to-left: digits 0-8 / letters a-y, A-Y bump in` |
|        - |  731 | ` * place; '9' wraps to '0' with carry; 'z' to 'a'; 'Z' to 'A'. A non-` |
|        - |  732 | ` * alphanumeric byte stops the walk without prepending. If carry survives` |
|        - |  733 | ` * past index 0, prepend '1', 'a', or 'A' depending on the class of the` |
|        - |  734 | ` * last carried character. Empty strings become "1".` |
|        - |  735 | ` *` |
|        - |  736 | ` * Caller must ensure pObj is MEMOBJ_STRING and not a numeric string;` |
|        - |  737 | ` * this routine never reclassifies the type, so a result like "e0" stays` |
|        - |  738 | ` * a string even though it looks numeric.` |
|        - |  739 | ` */` |
|       48 |  740 | `PH7_PRIVATE sxi32 PH7_MemObjStringIncrement(ph7_value *pObj)` |
|        1 |  741 |  |
|        - |  742 | `	enum CarryClass { CARRY_NONE = 0, CARRY_LOWER, CARRY_UPPER, CARRY_DIGIT };` |
|       49 |  743 | `	enum CarryClass last_class = CARRY_NONE;` |
|        - |  744 | `	sxu32 nLen, pos;` |
|        - |  745 | `	sxu8 *zStr;` |
|       49 |  746 | `	int carry = 1;` |
|        - |  747 | `	int ch;` |
|        - |  748 | `	/* Force ownership: the blob may be SXBLOB_RDONLY (e.g., from` |
|        - |  749 | `	 * PH7_MemObjLoad), in which case BlobPrepareGrow copies on demand` |
|        - |  750 | `	 * and clears the flag.  On an already-owned blob with spare capacity` |
|        - |  751 | `	 * (the common case under PHL's growth allocator), this is a no-op` |
|        - |  752 | `	 * append; on an exact-fit owned blob it triggers a single realloc. */` |
|       49 |  753 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       45 |  754 | `		SyBlobNullAppend(&pObj->sBlob);` |
|       22 |  755 | `	}` |
|       49 |  756 | `	nLen = SyBlobLength(&pObj->sBlob);` |
|       49 |  757 | `	if( nLen == 0 ){` |
|        5 |  758 | `		SyBlobAppend(&pObj->sBlob,"1",sizeof(char));` |
|        5 |  759 | `		return SXRET_OK;` |
|        - |  760 | `	}` |
|       45 |  761 | `	zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|       45 |  762 | `	pos = nLen;` |
|       97 |  763 | `	while( pos > 0 ){` |
|       79 |  764 | `		pos--;` |
|       79 |  765 | `		ch = zStr[pos];` |
|       79 |  766 | `		if( ch >= 'a' && ch <= 'z' ){` |
|       45 |  767 | `			if( ch == 'z' ){` |
|       29 |  768 | `				zStr[pos] = 'a';` |
|       29 |  769 | `				last_class = CARRY_LOWER;` |
|       29 |  770 | `				continue;` |
|        - |  771 | `			}` |
|       17 |  772 | `			zStr[pos]++;` |
|       17 |  773 | `			carry = 0;` |
|       17 |  774 | `			break;` |
|       35 |  775 | `		}else if( ch >= 'A' && ch <= 'Z' ){` |
|       21 |  776 | `			if( ch == 'Z' ){` |
|       19 |  777 | `				zStr[pos] = 'A';` |
|       19 |  778 | `				last_class = CARRY_UPPER;` |
|       19 |  779 | `				continue;` |
|        - |  780 | `			}` |
|        3 |  781 | `			zStr[pos]++;` |
|        3 |  782 | `			carry = 0;` |
|        3 |  783 | `			break;` |
|       15 |  784 | `		}else if( ch >= '0' && ch <= '9' ){` |
|        7 |  785 | `			if( ch == '9' ){` |
|        7 |  786 | `				zStr[pos] = '0';` |
|        7 |  787 | `				last_class = CARRY_DIGIT;` |
|        7 |  788 | `				continue;` |
|        - |  789 | `			}` |
|      ! 0 |  790 | `			zStr[pos]++;` |
|      ! 0 |  791 | `			carry = 0;` |
|      ! 0 |  792 | `			break;` |
|      ! 0 |  793 | `		}else{` |
|        - |  794 | `			/* non-alphanumeric: stop without prepending */` |
|        9 |  795 | `			carry = 0;` |
|        9 |  796 | `			break;` |
|        - |  797 | `		}` |
|      ! 0 |  798 | `	}` |
|       45 |  799 | `	if( carry ){` |
|        - |  800 | `		sxu8 prepend;` |
|        - |  801 | `		sxu32 i;` |
|       19 |  802 | `		switch( last_class ){` |
|        9 |  803 | `			case CARRY_LOWER: prepend = (sxu8)'a'; break;` |
|       11 |  804 | `			case CARRY_UPPER: prepend = (sxu8)'A'; break;` |
|      ! 0 |  805 | `			default:          prepend = (sxu8)'1'; break;` |
|        - |  806 | `		}` |
|        - |  807 | `		/* Append a sentinel byte to grow nByte by 1 (capacity grows too). */` |
|       19 |  808 | `		SyBlobAppend(&pObj->sBlob,"\0",sizeof(char));` |
|       19 |  809 | `		zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|       19 |  810 | `		nLen = SyBlobLength(&pObj->sBlob);` |
|        - |  811 | `		/* Shift right by 1, walking from the end so overlapping is safe. */` |
|       57 |  812 | `		for( i = nLen - 1; i > 0; i-- ){` |
|       39 |  813 | `			zStr[i] = zStr[i - 1];` |
|       20 |  814 | `		}` |
|       19 |  815 | `		zStr[0] = prepend;` |
|        9 |  816 | `	}` |
|       45 |  817 | `	return SXRET_OK;` |
|       25 |  818 |  |
|        - |  819 | `/*` |
|        - |  820 | ` * Try a get an integer representation of the given ph7_value.` |
|        - |  821 | ` * If the ph7_value is not of type real,this function is a no-op.` |
|        - |  822 | ` */` |
|      696 |  823 | `PH7_PRIVATE sxi32 PH7_MemObjTryInteger(ph7_value *pObj)` |
|        1 |  824 |  |
|      697 |  825 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|        - |  826 | `		/* Work only with reals */` |
|      697 |  827 | `		MemObjTryIntger(&(*pObj));` |
|      348 |  828 | `	}` |
|      697 |  829 | `	return SXRET_OK;` |
|        1 |  830 |  |
|        - |  831 | `/*` |
|        - |  832 | ` * Initialize a ph7_value to the null type.` |
|        - |  833 | ` */` |
|  6988582 |  834 | `PH7_PRIVATE sxi32 PH7_MemObjInit(ph7_vm *pVm,ph7_value *pObj)` |
|        2 |  835 |  |
|        - |  836 | `	/* Zero the structure */` |
|  6988584 |  837 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - |  838 | `	/* Initialize fields */` |
|  6988584 |  839 | `	pObj->pVm = pVm;` |
|  6988584 |  840 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - |  841 | `	/* Set the NULL type */` |
|  6988584 |  842 | `	pObj->iFlags = MEMOBJ_NULL;` |
|  6988584 |  843 | `	return SXRET_OK;` |
|        2 |  844 |  |
|        - |  845 | `/*` |
|        - |  846 | ` * Initialize a ph7_value to the integer type.` |
|        - |  847 | ` */` |
|   134684 |  848 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromInt(ph7_vm *pVm,ph7_value *pObj,sxi64 iVal)` |
|        2 |  849 |  |
|        - |  850 | `	/* Zero the structure */` |
|   134686 |  851 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - |  852 | `	/* Initialize fields */` |
|   134686 |  853 | `	pObj->pVm = pVm;` |
|   134686 |  854 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - |  855 | `	/* Set the desired type */` |
|   134686 |  856 | `	pObj->x.iVal = iVal;` |
|   134686 |  857 | `	pObj->iFlags = MEMOBJ_INT;` |
|   134686 |  858 | `	return SXRET_OK;` |
|        2 |  859 |  |
|        - |  860 | `/*` |
|        - |  861 | ` * Initialize a ph7_value to the boolean type.` |
|        - |  862 | ` */` |
|    15260 |  863 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromBool(ph7_vm *pVm,ph7_value *pObj,sxi32 iVal)` |
|        2 |  864 |  |
|        - |  865 | `	/* Zero the structure */` |
|    15262 |  866 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - |  867 | `	/* Initialize fields */` |
|    15262 |  868 | `	pObj->pVm = pVm;` |
|    15262 |  869 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - |  870 | `	/* Set the desired type */` |
|    15262 |  871 | `	pObj->x.iVal = iVal ? 1 : 0;` |
|    15262 |  872 | `	pObj->iFlags = MEMOBJ_BOOL;` |
|    15262 |  873 | `	return SXRET_OK;` |
|        2 |  874 |  |
|        - |  875 | `#if 0` |
|        - |  876 | `/*` |
|        - |  877 | ` * Initialize a ph7_value to the real type.` |
|        - |  878 | ` */` |
|        - |  879 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromReal(ph7_vm *pVm,ph7_value *pObj,ph7_real rVal)` |
|        - |  880 |  |
|        - |  881 | `	/* Zero the structure */` |
|        - |  882 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - |  883 | `	/* Initialize fields */` |
|        - |  884 | `	pObj->pVm = pVm;` |
|        - |  885 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - |  886 | `	/* Set the desired type */` |
|        - |  887 | `	pObj->rVal = rVal;` |
|        - |  888 | `	pObj->iFlags = MEMOBJ_REAL;` |
|        - |  889 | `	return SXRET_OK;` |
|        - |  890 |  |
|        - |  891 | `#endif` |
|        - |  892 | `/*` |
|        - |  893 | ` * Initialize a ph7_value to the array type.` |
|        - |  894 | ` */` |
|    38640 |  895 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromArray(ph7_vm *pVm,ph7_value *pObj,ph7_hashmap *pArray)` |
|        2 |  896 |  |
|        - |  897 | `	/* Zero the structure */` |
|    38642 |  898 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - |  899 | `	/* Initialize fields */` |
|    38642 |  900 | `	pObj->pVm = pVm;` |
|    38642 |  901 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - |  902 | `	/* Set the desired type */` |
|    38642 |  903 | `	pObj->iFlags = MEMOBJ_HASHMAP;` |
|    38642 |  904 | `	pObj->x.pOther = pArray;` |
|    38642 |  905 | `	return SXRET_OK;` |
|        2 |  906 |  |
|        - |  907 | `/*` |
|        - |  908 | ` * Initialize a ph7_value to the string type.` |
|        - |  909 | ` */` |
|   360690 |  910 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromString(ph7_vm *pVm,ph7_value *pObj,const SyString *pVal)` |
|        2 |  911 |  |
|        - |  912 | `	/* Zero the structure */` |
|   360692 |  913 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - |  914 | `	/* Initialize fields */` |
|   360692 |  915 | `	pObj->pVm = pVm;` |
|   360692 |  916 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|   360692 |  917 | `	if( pVal ){` |
|        - |  918 | `		/* Append contents */` |
|   248840 |  919 | `		SyBlobAppend(&pObj->sBlob,(const void *)pVal->zString,pVal->nByte);` |
|   124419 |  920 | `	}` |
|        - |  921 | `	/* Set the desired type */` |
|   360692 |  922 | `	pObj->iFlags = MEMOBJ_STRING;` |
|   360692 |  923 | `	return SXRET_OK;` |
|        2 |  924 |  |
|        - |  925 | `/*` |
|        - |  926 | ` * Append some contents to the internal buffer of a given ph7_value.` |
|        - |  927 | ` * If the given ph7_value is not of type string,this function` |
|        - |  928 | ` * invalidate any prior representation and set the string type.` |
|        - |  929 | ` * Then a simple append operation is performed.` |
|        - |  930 | ` */` |
|   263924 |  931 | `PH7_PRIVATE sxi32 PH7_MemObjStringAppend(ph7_value *pObj,const char *zData,sxu32 nLen)` |
|        2 |  932 |  |
|        - |  933 | `	sxi32 rc;` |
|   263926 |  934 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  935 | `		/* Invalidate any prior representation */` |
|       92 |  936 | `		PH7_MemObjRelease(pObj);` |
|       92 |  937 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|       45 |  938 | `	}` |
|        - |  939 | `	/* Append contents */` |
|   263926 |  940 | `	rc = SyBlobAppend(&pObj->sBlob,zData,nLen);` |
|   263926 |  941 | `	return rc;` |
|        2 |  942 |  |
|        - |  943 | `#if 0` |
|        - |  944 | `/*` |
|        - |  945 | ` * Format and append some contents to the internal buffer of a given ph7_value.` |
|        - |  946 | ` * If the given ph7_value is not of type string,this function invalidate` |
|        - |  947 | ` * any prior representation and set the string type.` |
|        - |  948 | ` * Then a simple format and append operation is performed.` |
|        - |  949 | ` */` |
|        - |  950 | `PH7_PRIVATE sxi32 PH7_MemObjStringFormat(ph7_value *pObj,const char *zFormat,va_list ap)` |
|        - |  951 |  |
|        - |  952 | `	sxi32 rc;` |
|        - |  953 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  954 | `		/* Invalidate any prior representation */` |
|        - |  955 | `		PH7_MemObjRelease(pObj);` |
|        - |  956 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|        - |  957 | `	}` |
|        - |  958 | `	/* Format and append contents */` |
|        - |  959 | `	rc = SyBlobFormatAp(&pObj->sBlob,zFormat,ap);` |
|        - |  960 | `	return rc;` |
|        - |  961 |  |
|        - |  962 | `#endif` |
|        - |  963 | `/*` |
|        - |  964 | ` * Duplicate the contents of a ph7_value.` |
|        - |  965 | ` */` |
|  4284056 |  966 | `PH7_PRIVATE sxi32 PH7_MemObjStore(ph7_value *pSrc,ph7_value *pDest)` |
|        2 |  967 |  |
|  4284058 |  968 | `	ph7_class_instance *pObj = 0;` |
|  4284058 |  969 | `	ph7_hashmap *pMap = 0;` |
|        - |  970 | `	sxi32 rc;` |
|  4284058 |  971 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  972 | `		/* Increment reference count */` |
|   134142 |  973 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  4216988 |  974 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|        - |  975 | `		/* Increment reference count */` |
|     1672 |  976 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|      835 |  977 | `	}` |
|  4284058 |  978 | `	if( pDest->iFlags & MEMOBJ_HASHMAP ){` |
|    40156 |  979 | `		pMap = (ph7_hashmap *)pDest->x.pOther;` |
|  4263981 |  980 | `	}else if( pDest->iFlags & MEMOBJ_OBJ ){` |
|     1486 |  981 | `		pObj = (ph7_class_instance *)pDest->x.pOther;` |
|      742 |  982 | `	}` |
|  4284058 |  983 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|  4284058 |  984 | `	pDest->iFlags &= ~MEMOBJ_AUX;` |
|  4284058 |  985 | `	rc = SXRET_OK;` |
|  4284058 |  986 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  3630170 |  987 | `		SyBlobReset(&pDest->sBlob);` |
|  3630170 |  988 | `		rc = SyBlobDup(&pSrc->sBlob,&pDest->sBlob);` |
|  1815086 |  989 | `	}else{` |
|   653890 |  990 | `		if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|   246706 |  991 | `			SyBlobRelease(&pDest->sBlob);` |
|   123374 |  992 | `		}` |
|        - |  993 | `	}` |
|  4284058 |  994 | `	if( pMap ){` |
|    40156 |  995 | `		PH7_HashmapUnref(pMap);` |
|  4263981 |  996 | `	}else if( pObj ){` |
|     1486 |  997 | `		PH7_ClassInstanceUnref(pObj);` |
|      742 |  998 | `	}` |
|  4284058 |  999 | `	return rc;` |
|        2 | 1000 |  |
|        - | 1001 | `/*` |
|        - | 1002 | ` * Duplicate the contents of a ph7_value but do not copy internal` |
|        - | 1003 | ` * buffer contents,simply point to it.` |
|        - | 1004 | ` */` |
|  5578162 | 1005 | `PH7_PRIVATE sxi32 PH7_MemObjLoad(ph7_value *pSrc,ph7_value *pDest)` |
|        2 | 1006 |  |
|  5578164 | 1007 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),` |
|        - | 1008 | `		sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|  5578164 | 1009 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1010 | `		/* Increment reference count */` |
|   385030 | 1011 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  5385650 | 1012 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|        - | 1013 | `		/* Increment reference count */` |
|     9158 | 1014 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|     4578 | 1015 | `	}` |
|  5578164 | 1016 | `	if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|       30 | 1017 | `		SyBlobRelease(&pDest->sBlob);` |
|       14 | 1018 | `	}` |
|  5578164 | 1019 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  2988530 | 1020 | `		SyBlobReadOnly(&pDest->sBlob,SyBlobData(&pSrc->sBlob),SyBlobLength(&pSrc->sBlob));` |
|  1494288 | 1021 | `	}` |
|  5578164 | 1022 | `	return SXRET_OK;` |
|        2 | 1023 |  |
|        - | 1024 | `/*` |
|        - | 1025 | ` * Invalidate any prior representation of a given ph7_value.` |
|        - | 1026 | ` */` |
| 11146514 | 1027 | `PH7_PRIVATE sxi32 PH7_MemObjRelease(ph7_value *pObj)` |
|        2 | 1028 |  |
| 11146516 | 1029 | `	if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
| 10284530 | 1030 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|   483152 | 1031 | `			PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
| 10042955 | 1032 | `		}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|    15814 | 1033 | `			PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|     7906 | 1034 | `		}` |
|        - | 1035 | `		/* Release the internal buffer */` |
| 10284530 | 1036 | `		SyBlobRelease(&pObj->sBlob);` |
|        - | 1037 | `		/* Invalidate any prior representation */` |
| 10284530 | 1038 | `		pObj->iFlags = MEMOBJ_NULL;` |
|  5142420 | 1039 | `	}` |
| 11146516 | 1040 | `	return SXRET_OK;` |
|        2 | 1041 |  |
|        - | 1042 | `/*` |
|        - | 1043 | ` * Compare two ph7_values.` |
|        - | 1044 | ` * Return 0 if the values are equals, > 0 if pObj1 is greater than pObj2` |
|        - | 1045 | ` * or < 0 if pObj2 is greater than pObj1.` |
|        - | 1046 | ` * Type comparison table taken from the PHP language reference manual.` |
|        - | 1047 | ` * Comparisons of $x with PHP functions Expression` |
|        - | 1048 | ` *              gettype() 	empty() 	is_null() 	isset() 	boolean : if($x)` |
|        - | 1049 | ` * $x = ""; 	string 	    TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1050 | ` * $x = null 	NULL 	    TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1051 | ` * var $x; 	    NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1052 | ` * $x is undefined 	NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1053 | ` *  $x = array(); 	array 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1054 | ` * $x = false; 	boolean 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1055 | ` * $x = true; 	boolean 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1056 | ` * $x = 1; 	    integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1057 | ` * $x = 42; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1058 | ` * $x = 0; 	    integer 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1059 | ` * $x = -1; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1060 | ` * $x = "1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1061 | ` * $x = "0"; 	string 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1062 | ` * $x = "-1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1063 | ` * $x = "php"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1064 | ` * $x = "true"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1065 | ` * $x = "false"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1066 | ` *      Loose comparisons with ==` |
|        - | 1067 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|        - | 1068 | ` * TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1069 | ` * FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|        - | 1070 | ` * 1 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1071 | ` * 0 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE` |
|        - | 1072 | ` * -1 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1073 | ` * "1" 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1074 | ` * "0" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1075 | ` * "-1" 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1076 | ` * NULL 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|        - | 1077 | ` * array() 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1078 | ` * "php" 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1079 | ` * "" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE` |
|        - | 1080 | ` *    Strict comparisons with ===` |
|        - | 1081 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|        - | 1082 | ` * TRUE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1083 | ` * FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1084 | ` * 1 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1085 | ` * 0 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1086 | ` * -1 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1087 | ` * "1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1088 | ` * "0" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1089 | ` * "-1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1090 | ` * NULL 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE` |
|        - | 1091 | ` * array() 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE` |
|        - | 1092 | ` * "php" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1093 | ` * "" 	    FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE` |
|        - | 1094 | ` */` |
|  1090158 | 1095 | `PH7_PRIVATE sxi32 PH7_MemObjCmp(ph7_value *pObj1,ph7_value *pObj2,int bStrict,int iNest)` |
|        2 | 1096 |  |
|        - | 1097 | `	sxi32 iComb;` |
|        - | 1098 | `	sxi32 rc;` |
|  1090160 | 1099 | `	if( bStrict ){` |
|        - | 1100 | `		sxi32 iF1,iF2;` |
|        - | 1101 | `		/* Strict comparisons with === */` |
|   555212 | 1102 | `		iF1 = pObj1->iFlags&~MEMOBJ_AUX;` |
|   555212 | 1103 | `		iF2 = pObj2->iFlags&~MEMOBJ_AUX;` |
|   555212 | 1104 | `		if( iF1 != iF2 ){` |
|        - | 1105 | `			/* Not of the same type */` |
|   150572 | 1106 | `			return 1;` |
|        - | 1107 | `		}` |
|   202320 | 1108 | `	}` |
|        - | 1109 | `	/* Combine flag together */` |
|   939590 | 1110 | `	iComb = pObj1->iFlags\|pObj2->iFlags;` |
|   939590 | 1111 | `	if( iComb & (MEMOBJ_NULL\|MEMOBJ_RES\|MEMOBJ_BOOL) ){` |
|        - | 1112 | `		/* Convert to boolean: Keep in mind FALSE < TRUE */` |
|    18292 | 1113 | `		if( (pObj1->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     6448 | 1114 | `			PH7_MemObjToBool(pObj1);` |
|     3223 | 1115 | `		}` |
|    18292 | 1116 | `		if( (pObj2->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     5540 | 1117 | `			PH7_MemObjToBool(pObj2);` |
|     2769 | 1118 | `		}` |
|    18292 | 1119 | `		return (sxi32)((pObj1->x.iVal != 0) - (pObj2->x.iVal != 0));` |
|   921300 | 1120 | `	}else if ( iComb & MEMOBJ_HASHMAP ){` |
|        - | 1121 | `		/* Hashmap aka 'array' comparison */` |
|       19 | 1122 | `		if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1123 | `			/* Array is always greater */` |
|      ! 0 | 1124 | `			return -1;` |
|        - | 1125 | `		}` |
|       19 | 1126 | `		if( (pObj2->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1127 | `			/* Array is always greater */` |
|      ! 0 | 1128 | `			return 1;` |
|        - | 1129 | `		}` |
|        - | 1130 | `		/* Perform the comparison */` |
|       19 | 1131 | `		rc = PH7_HashmapCmp((ph7_hashmap *)pObj1->x.pOther,(ph7_hashmap *)pObj2->x.pOther,bStrict);` |
|       19 | 1132 | `		return rc;` |
|   921282 | 1133 | `	}else if(iComb & MEMOBJ_OBJ ){` |
|        - | 1134 | `		/* Object comparison */` |
|      176 | 1135 | `		if( (pObj1->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 1136 | `			/* Object is always greater */` |
|      ! 0 | 1137 | `			return -1;` |
|        - | 1138 | `		}` |
|      176 | 1139 | `		if( (pObj2->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 1140 | `			/* Object is always greater */` |
|      ! 0 | 1141 | `			return 1;` |
|        - | 1142 | `		}` |
|        - | 1143 | `		/* Perform the comparison */` |
|      176 | 1144 | `		rc = PH7_ClassInstanceCmp((ph7_class_instance *)pObj1->x.pOther,(ph7_class_instance *)pObj2->x.pOther,bStrict,iNest);` |
|      176 | 1145 | `		return rc;` |
|   921108 | 1146 | `	}else if ( iComb & MEMOBJ_STRING ){` |
|        - | 1147 | `		SyString s1,s2;` |
|   583274 | 1148 | `		if( !bStrict ){` |
|        - | 1149 | `			/*` |
|        - | 1150 | `			 * According to the PHP language reference manual:` |
|        - | 1151 | `			 *` |
|        - | 1152 | `			 *  If you compare a number with a string or the comparison involves numerical` |
|        - | 1153 | `			 *  strings, then each string is converted to a number and the comparison` |
|        - | 1154 | `			 *  performed numerically.` |
|        - | 1155 | `			 */` |
|   196786 | 1156 | `			if( PH7_MemObjIsNumeric(pObj1) ){` |
|        - | 1157 | `				/* Perform a numeric comparison */` |
|       13 | 1158 | `				goto Numeric;` |
|        - | 1159 | `			}` |
|   196774 | 1160 | `			if( PH7_MemObjIsNumeric(pObj2) ){` |
|        - | 1161 | `				/* Perform a numeric comparison */` |
|      ! 0 | 1162 | `				goto Numeric;` |
|        - | 1163 | `			}` |
|    98376 | 1164 | `		}` |
|        - | 1165 | `		/* Perform a strict string comparison.*/` |
|   583262 | 1166 | `		if( (pObj1->iFlags&MEMOBJ_STRING) == 0 ){` |
|      ! 0 | 1167 | `			PH7_MemObjToString(pObj1);` |
|      ! 0 | 1168 | `		}` |
|   583262 | 1169 | `		if( (pObj2->iFlags&MEMOBJ_STRING) == 0 ){` |
|      ! 0 | 1170 | `			PH7_MemObjToString(pObj2);` |
|      ! 0 | 1171 | `		}` |
|   583262 | 1172 | `		SyStringInitFromBuf(&s1,SyBlobData(&pObj1->sBlob),SyBlobLength(&pObj1->sBlob));` |
|   583262 | 1173 | `		SyStringInitFromBuf(&s2,SyBlobData(&pObj2->sBlob),SyBlobLength(&pObj2->sBlob));` |
|        - | 1174 | `		/*` |
|        - | 1175 | `		 * Strings are compared using memcmp(). If one value is an exact prefix of the` |
|        - | 1176 | `		 * other, then the shorter value is less than the longer value.` |
|        - | 1177 | `		 */` |
|   583262 | 1178 | `		rc = SyMemcmp((const void *)s1.zString,(const void *)s2.zString,SXMIN(s1.nByte,s2.nByte));` |
|   583262 | 1179 | `		if( rc == 0 ){` |
|   207557 | 1180 | `			if( s1.nByte != s2.nByte ){` |
|     1521 | 1181 | `				rc = s1.nByte < s2.nByte ? -1 : 1;` |
|      760 | 1182 | `			}` |
|   103778 | 1183 | `		}` |
|   583262 | 1184 | `		return rc;` |
|   337836 | 1185 | `	}else if( iComb & (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|   168895 | 1186 | `Numeric:` |
|        - | 1187 | `		/* Perform a numeric comparison if one of the operand is numeric(integer or real) */` |
|   337848 | 1188 | `		if( (pObj1->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|        3 | 1189 | `			PH7_MemObjToNumeric(pObj1);` |
|        1 | 1190 | `		}` |
|   337848 | 1191 | `		if( (pObj2->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|       11 | 1192 | `			PH7_MemObjToNumeric(pObj2);` |
|        5 | 1193 | `		}` |
|   337848 | 1194 | `		if( (pObj1->iFlags & pObj2->iFlags & MEMOBJ_INT) == 0) {` |
|        - | 1195 | `			/*` |
|        - | 1196 | `			 * Symisc eXtension to the PHP language:` |
|        - | 1197 | `			 *  Floating point comparison is introduced and works as expected.` |
|        - | 1198 | `			 */` |
|        - | 1199 | `			ph7_real r1,r2;` |
|        - | 1200 | `			/* Compare as reals */` |
|      181 | 1201 | `			if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|       11 | 1202 | `				PH7_MemObjToReal(pObj1);` |
|        5 | 1203 | `			}` |
|      181 | 1204 | `			r1 = pObj1->rVal;` |
|      181 | 1205 | `			if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|       25 | 1206 | `				PH7_MemObjToReal(pObj2);` |
|       12 | 1207 | `			}` |
|      181 | 1208 | `			r2 = pObj2->rVal;` |
|      181 | 1209 | `			if( PH7_IS_NAN(r1) \|\| PH7_IS_NAN(r2) ){` |
|        - | 1210 | `				/*` |
|        - | 1211 | `				 * Keep a strict three-way comparator contract even for NaN values.` |
|        - | 1212 | `				 * For ordering purposes, NaN compares equal to NaN and greater than` |
|        - | 1213 | `				 * any non-NaN numeric value.` |
|        - | 1214 | `				 */` |
|       45 | 1215 | `				if( PH7_IS_NAN(r1) ){` |
|       35 | 1216 | `					return PH7_IS_NAN(r2) ? 0 : 1;` |
|        - | 1217 | `				}` |
|       11 | 1218 | `				return -1;` |
|        - | 1219 | `			}` |
|      137 | 1220 | `			if( r1 > r2 ){` |
|       13 | 1221 | `				return 1;` |
|      125 | 1222 | `			}else if( r1 < r2 ){` |
|      103 | 1223 | `				return -1;` |
|        - | 1224 | `			}` |
|       23 | 1225 | `			return 0;` |
|      ! 0 | 1226 | `		}else{` |
|        - | 1227 | `			/* Integer comparison */` |
|   337668 | 1228 | `			if( pObj1->x.iVal > pObj2->x.iVal ){` |
|     2343 | 1229 | `				return 1;` |
|   335327 | 1230 | `			}else if( pObj1->x.iVal < pObj2->x.iVal ){` |
|   331496 | 1231 | `				return -1;` |
|        - | 1232 | `			}` |
|     3833 | 1233 | `			return 0;` |
|        - | 1234 | `		}` |
|        - | 1235 | `	}` |
|        - | 1236 | `	/* NOT REACHED */` |
|      ! 0 | 1237 | `	return 0;` |
|   545093 | 1238 |  |
|        - | 1239 | `/*` |
|        - | 1240 | ` * Perform an addition operation of two ph7_values.` |
|        - | 1241 | ` * The reason this function is implemented here rather than 'vm.c'` |
|        - | 1242 | ` * is that the '+' operator is overloaded.` |
|        - | 1243 | ` * That is,the '+' operator is used for arithmetic operation and also` |
|        - | 1244 | ` * used for operation on arrays [i.e: union]. When used with an array` |
|        - | 1245 | ` * The + operator returns the right-hand array appended to the left-hand array.` |
|        - | 1246 | ` * For keys that exist in both arrays, the elements from the left-hand array` |
|        - | 1247 | ` * will be used, and the matching elements from the right-hand array will` |
|        - | 1248 | ` * be ignored.` |
|        - | 1249 | ` * This function take care of handling all the scenarios.` |
|        - | 1250 | ` */` |
|     2030 | 1251 | `PH7_PRIVATE sxi32 PH7_MemObjAdd(ph7_value *pObj1,ph7_value *pObj2,int bAddStore)` |
|        2 | 1252 |  |
|     2032 | 1253 | `	if( ((pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1254 | `			/* Arithemtic operation */` |
|     2022 | 1255 | `			PH7_MemObjToNumeric(pObj1);` |
|     2022 | 1256 | `			PH7_MemObjToNumeric(pObj2);` |
|     2022 | 1257 | `			if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_REAL ){` |
|        - | 1258 | `				/* Floating point arithmetic */` |
|        - | 1259 | `				ph7_real a,b;` |
|       27 | 1260 | `				if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 | 1261 | `					PH7_MemObjToReal(pObj1);` |
|        3 | 1262 | `				}` |
|       27 | 1263 | `				if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 | 1264 | `					PH7_MemObjToReal(pObj2);` |
|        1 | 1265 | `				}` |
|       27 | 1266 | `				a = pObj1->rVal;` |
|       27 | 1267 | `				b = pObj2->rVal;` |
|       27 | 1268 | `				pObj1->rVal = a+b;` |
|       27 | 1269 | `				MemObjSetType(pObj1,MEMOBJ_REAL);` |
|        - | 1270 | `				/* Try to get an integer representation also */` |
|       27 | 1271 | `				MemObjTryIntger(&(*pObj1));` |
|       14 | 1272 | `			}else{` |
|        - | 1273 | `				/* Integer arithmetic */` |
|        - | 1274 | `				sxi64 a,b;` |
|     1996 | 1275 | `				a = pObj1->x.iVal;` |
|     1996 | 1276 | `				b = pObj2->x.iVal;` |
|     1996 | 1277 | `				pObj1->x.iVal = a+b;` |
|     1996 | 1278 | `				MemObjSetType(pObj1,MEMOBJ_INT);` |
|        - | 1279 | `			}` |
|     1012 | 1280 | `	}else{` |
|       12 | 1281 | `		if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP ){` |
|        - | 1282 | `			ph7_hashmap *pMap;` |
|        - | 1283 | `			sxi32 rc;` |
|       12 | 1284 | `			if( bAddStore ){` |
|        - | 1285 | `				/* Do not duplicate the hashmap,use the left one since its an add&store operation.` |
|        - | 1286 | `				 */` |
|        3 | 1287 | `				if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1288 | `					/* Force a hashmap cast */` |
|      ! 0 | 1289 | `					rc = PH7_MemObjToHashmap(pObj1);` |
|      ! 0 | 1290 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 1291 | `						PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|      ! 0 | 1292 | `						return rc;` |
|        - | 1293 | `					}` |
|      ! 0 | 1294 | `				}` |
|        - | 1295 | `				/* COW separate before in-place mutation */` |
|        3 | 1296 | `				pMap = PH7_HashmapCowSeparate(pObj1->pVm,pObj1);` |
|        2 | 1297 | `			}else{` |
|        - | 1298 | `				/* Create a new hashmap */` |
|       10 | 1299 | `				pMap = PH7_NewHashmap(pObj1->pVm,0,0);` |
|       10 | 1300 | `				if( pMap == 0){` |
|      ! 0 | 1301 | `					PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|      ! 0 | 1302 | `					return SXERR_MEM;` |
|        - | 1303 | `				}` |
|        - | 1304 | `			}` |
|       12 | 1305 | `			if( !bAddStore ){` |
|       10 | 1306 | `				if(pObj1->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1307 | `					/* Perform a hashmap duplication */` |
|       10 | 1308 | `					PH7_HashmapDup((ph7_hashmap *)pObj1->x.pOther,pMap);` |
|        6 | 1309 | `				}else{` |
|      ! 0 | 1310 | `					if((pObj1->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 1311 | `						/* Simple insertion */` |
|      ! 0 | 1312 | `						PH7_HashmapInsert(pMap,0,pObj1);` |
|      ! 0 | 1313 | `					}` |
|        - | 1314 | `				}` |
|        4 | 1315 | `			}` |
|        - | 1316 | `			/* Perform the union */` |
|       12 | 1317 | `			if(pObj2->iFlags & MEMOBJ_HASHMAP ){` |
|       12 | 1318 | `				PH7_HashmapUnion(pMap,(ph7_hashmap *)pObj2->x.pOther);` |
|        7 | 1319 | `			}else{` |
|      ! 0 | 1320 | `				if((pObj2->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 1321 | `					/* Simple insertion */` |
|      ! 0 | 1322 | `					PH7_HashmapInsert(pMap,0,pObj2);` |
|      ! 0 | 1323 | `				}` |
|        - | 1324 | `			}` |
|        - | 1325 | `			/* Reflect the change */` |
|       12 | 1326 | `			if( pObj1->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 1327 | `				SyBlobRelease(&pObj1->sBlob);` |
|      ! 0 | 1328 | `			}` |
|       12 | 1329 | `			pObj1->x.pOther = pMap;` |
|       12 | 1330 | `			MemObjSetType(pObj1,MEMOBJ_HASHMAP);` |
|        5 | 1331 | `		}` |
|        - | 1332 | `	}` |
|     2032 | 1333 | `	return SXRET_OK;` |
|     1017 | 1334 |  |
|        - | 1335 | `/*` |
|        - | 1336 | ` * Return a printable representation of the type of a given` |
|        - | 1337 | ` * ph7_value.` |
|        - | 1338 | ` */` |
|      414 | 1339 | `PH7_PRIVATE const char * PH7_MemObjTypeDump(ph7_value *pVal)` |
|        2 | 1340 |  |
|      416 | 1341 | `	const char *zType = "";` |
|      416 | 1342 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|        7 | 1343 | `		zType = "null";` |
|      413 | 1344 | `	}else if( pVal->iFlags & MEMOBJ_INT ){` |
|       96 | 1345 | `		zType = "int";` |
|      363 | 1346 | `	}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|        7 | 1347 | `		zType = "double";` |
|      313 | 1348 | `	}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|       58 | 1349 | `		zType = "string";` |
|      282 | 1350 | `	}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|      110 | 1351 | `		zType = "bool";` |
|      200 | 1352 | `	}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|       16 | 1353 | `		zType = "array";` |
|      138 | 1354 | `	}else if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      131 | 1355 | `		zType = "object";` |
|       65 | 1356 | `	}else if( pVal->iFlags & MEMOBJ_RES ){` |
|      ! 0 | 1357 | `		zType = "resource";` |
|      ! 0 | 1358 | `	}` |
|      416 | 1359 | `	return zType;` |
|        2 | 1360 |  |
|        - | 1361 | `/*` |
|        - | 1362 | ` * Dump a ph7_value [i.e: get a printable representation of it's type and contents.].` |
|        - | 1363 | ` * Store the dump in the given blob.` |
|        - | 1364 | ` */` |
|      472 | 1365 | `PH7_PRIVATE sxi32 PH7_MemObjDump(` |
|        - | 1366 | `	SyBlob *pOut,      /* Store the dump here */` |
|        - | 1367 | `	ph7_value *pObj,   /* Dump this */` |
|        - | 1368 | `	int ShowType,      /* TRUE to output value type */` |
|        - | 1369 | `	int nTab,          /* # of Whitespace to insert */` |
|        - | 1370 | `	int nDepth,        /* Nesting level */` |
|        - | 1371 | `	int isRef          /* TRUE if referenced object */` |
|        - | 1372 | `	)` |
|        2 | 1373 |  |
|      474 | 1374 | `	sxi32 rc = SXRET_OK;` |
|        - | 1375 | `	const char *zType;` |
|        - | 1376 | `	int i;` |
|     4654 | 1377 | `	for( i = 0 ; i < nTab ; i++ ){` |
|     4182 | 1378 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     2092 | 1379 | `	}` |
|      474 | 1380 | `	if( ShowType ){` |
|      384 | 1381 | `		if( isRef ){` |
|      ! 0 | 1382 | `			SyBlobAppend(&(*pOut),"&",sizeof(char));` |
|      ! 0 | 1383 | `		}` |
|        - | 1384 | `		/* Get value type first */` |
|      384 | 1385 | `		zType = PH7_MemObjTypeDump(pObj);` |
|      384 | 1386 | `		SyBlobAppend(&(*pOut),zType,SyStrlen(zType));` |
|      191 | 1387 | `	}` |
|      474 | 1388 | `	if((pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|      470 | 1389 | `		if ( ShowType ){` |
|      380 | 1390 | `			SyBlobAppend(&(*pOut),"(",sizeof(char));` |
|      189 | 1391 | `		}` |
|      470 | 1392 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1393 | `			/* Dump hashmap entries */` |
|       28 | 1394 | `			rc = PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,ShowType,nTab+1,nDepth+1);` |
|      457 | 1395 | `		}else if(pObj->iFlags & MEMOBJ_OBJ ){` |
|        - | 1396 | `			/* Dump class instance attributes */` |
|      133 | 1397 | `			rc = PH7_ClassInstanceDump(&(*pOut),(ph7_class_instance *)pObj->x.pOther,ShowType,nTab+1,nDepth+1);` |
|       67 | 1398 | `		}else{` |
|      312 | 1399 | `			SyBlob *pContents = &pObj->sBlob;` |
|        - | 1400 | `			/* Get a printable representation of the contents */` |
|      312 | 1401 | `			if((pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|      222 | 1402 | `				MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|      112 | 1403 | `			}else{` |
|        - | 1404 | `				/* PHP format: string(N) "content" */` |
|       92 | 1405 | `				if( ShowType ){` |
|       36 | 1406 | `					SyBlobFormat(&(*pOut),"%u) \"",SyBlobLength(&pObj->sBlob));` |
|       17 | 1407 | `				}` |
|       92 | 1408 | `				if( SyBlobLength(pContents) > 0 ){` |
|       82 | 1409 | `					SyBlobAppend(&(*pOut),SyBlobData(pContents),SyBlobLength(pContents));` |
|       40 | 1410 | `				}` |
|       92 | 1411 | `				if( ShowType ){` |
|       36 | 1412 | `					SyBlobAppend(&(*pOut),"\"",sizeof(char));` |
|       17 | 1413 | `				}` |
|        - | 1414 | `			}` |
|        - | 1415 | `		}` |
|      470 | 1416 | `		if( ShowType ){` |
|        - | 1417 | `			/* Strings already emitted their own ')' as part of the` |
|        - | 1418 | `			 * "N) \"content\"" format above. */` |
|      380 | 1419 | `			if( (pObj->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_STRING)) == 0 ){` |
|      204 | 1420 | `				SyBlobAppend(&(*pOut),")",sizeof(char));` |
|      101 | 1421 | `			}` |
|      189 | 1422 | `		}` |
|      234 | 1423 | `	}` |
|        - | 1424 | `#ifdef __WINNT__` |
|        2 | 1425 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|        - | 1426 | `#else` |
|      472 | 1427 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        - | 1428 | `#endif` |
|      474 | 1429 | `	return rc;` |
|        2 | 1430 |  |
|        - | 1431 |  |
