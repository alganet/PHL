# src/ph7/memobj.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 630/732 lines (86.07%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h" /* This file handle low-level stuff related to indexed memory objects [i.e: ph7_value] */` |
|       - |    7 |  |
|       - |    8 | `/* Provide PHP-style type names for values.  This utility may be reused` |
|       - |    9 | ` * by any subsystem that works with ph7_value.` |
|       - |   10 | ` */` |
|      98 |   11 | `PH7_PRIVATE const char *ph7_type_name(ph7_value *pVal)` |
|       1 |   12 |  |
|      99 |   13 | `	if( ph7_value_is_null(pVal) ) return "null";` |
|      99 |   14 | `	if( ph7_value_is_bool(pVal) ) return "bool";` |
|      99 |   15 | `	if( ph7_value_is_int(pVal) ) return "int";` |
|      85 |   16 | `	if( ph7_value_is_float(pVal) ) return "float";` |
|      85 |   17 | `	if( ph7_value_is_string(pVal) ) return "string";` |
|      23 |   18 | `	if( ph7_value_is_array(pVal) ) return "array";` |
|       5 |   19 | `	if( ph7_value_is_object(pVal) ) return "object";` |
|     ! 0 |   20 | `	if( ph7_value_is_resource(pVal) ) return "resource";` |
|     ! 0 |   21 | `	return "unknown";` |
|      50 |   22 |  |
|       - |   23 |  |
|       - |   24 | `/*` |
|       - |   25 | ` * Notes on memory objects [i.e: ph7_value].` |
|       - |   26 | ` * Internally, the PH7 virtual machine manipulates nearly all PHP values` |
|       - |   27 | ` * [i.e: string,int,float,resource,object,bool,null..] as ph7_values structures.` |
|       - |   28 | ` * Each ph7_values struct may cache multiple representations (string,` |
|       - |   29 | ` * integer etc.) of the same value.` |
|       - |   30 | ` */` |
|       - |   31 | `/*` |
|       - |   32 | ` * Convert a 64-bit IEEE double into a 64-bit signed integer.` |
|       - |   33 | ` * If the double is too large, return 0x8000000000000000.` |
|       - |   34 | ` *` |
|       - |   35 | ` * Most systems appear to do this simply by assigning ariables and without` |
|       - |   36 | ` * the extra range tests.` |
|       - |   37 | ` * But there are reports that windows throws an expection if the floating` |
|       - |   38 | ` * point value is out of range.` |
|       - |   39 | ` */` |
|    1324 |   40 | `static sxi64 MemObjRealToInt(ph7_value *pObj)` |
|       2 |   41 |  |
|       - |   42 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|       - |   43 | `	/* Real and 64bit integer are the same when floating point arithmetic` |
|       - |   44 | `	 * is omitted from the build.` |
|       - |   45 | `	 */` |
|       - |   46 | `	return pObj->rVal;` |
|       - |   47 | `#else` |
|       - |   48 | ` /*` |
|       - |   49 | `  ** Many compilers we encounter do not define constants for the` |
|       - |   50 | `  ** minimum and maximum 64-bit integers, or they define them` |
|       - |   51 | `  ** inconsistently.  And many do not understand the "LL" notation.` |
|       - |   52 | `  ** So we define our own static constants here using nothing` |
|       - |   53 | `  ** larger than a 32-bit integer constant.` |
|       - |   54 | `  */` |
|       - |   55 | `  static const sxi64 maxInt = LARGEST_INT64;` |
|       - |   56 | `  static const sxi64 minInt = SMALLEST_INT64;` |
|    1326 |   57 | `  ph7_real r = pObj->rVal;` |
|    1326 |   58 | `  if( r<(ph7_real)minInt ){` |
|     ! 0 |   59 | `    return minInt;` |
|    1326 |   60 | `  }else if( r>(ph7_real)maxInt ){` |
|       - |   61 | `    /* minInt is correct here - not maxInt.  It turns out that assigning` |
|       - |   62 | `    ** a very large positive number to an integer results in a very large` |
|       - |   63 | `    ** negative integer.  This makes no sense, but it is what x86 hardware` |
|       - |   64 | `    ** does so for compatibility we will do the same in software. */` |
|       7 |   65 | `    return minInt;` |
|     ! 0 |   66 | `  }else{` |
|    1320 |   67 | `    return (sxi64)r;` |
|       - |   68 | `  }` |
|       - |   69 | `#endif` |
|     664 |   70 |  |
|       - |   71 | `/*` |
|       - |   72 | ` * Convert a raw token value typically a stream of digit [i.e: hex,octal,binary or decimal]` |
|       - |   73 | ` * to a 64-bit integer.` |
|       - |   74 | ` */` |
|   55958 |   75 | `PH7_PRIVATE sxi64 PH7_TokenValueToInt64(SyString *pVal)` |
|       2 |   76 |  |
|   55960 |   77 | `	sxi64 iVal = 0;` |
|   55960 |   78 | `	if( pVal->nByte <= 0 ){` |
|       7 |   79 | `		return 0;` |
|       - |   80 | `	}` |
|   55954 |   81 | `	if( pVal->zString[0] == '0' ){` |
|       - |   82 | `		sxi32 c;` |
|   21085 |   83 | `		if( pVal->nByte == sizeof(char) ){` |
|   21020 |   84 | `			return 0;` |
|       - |   85 | `		}` |
|      66 |   86 | `		c = pVal->zString[1];` |
|      66 |   87 | `		if( c  == 'x' \|\| c == 'X' ){` |
|       - |   88 | `			/* Hex digit stream */` |
|      13 |   89 | `			SyHexStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|      60 |   90 | `		}else if( c == 'b' \|\| c == 'B' ){` |
|       - |   91 | `			/* Binary digit stream */` |
|      31 |   92 | `			SyBinaryStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|      16 |   93 | `		}else{` |
|       - |   94 | `			/* Octal digit stream */` |
|      24 |   95 | `			SyOctalStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|       - |   96 | `		}` |
|      33 |   97 | `	}else{` |
|       - |   98 | `		/* Decimal digit stream */` |
|   34871 |   99 | `		SyStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|       - |  100 | `	}` |
|   34936 |  101 | `	return iVal;` |
|   27981 |  102 |  |
|       - |  103 | `/*` |
|       - |  104 | ` * Return some kind of 64-bit integer value which is the best we can` |
|       - |  105 | ` * do at representing the value that pObj describes as a string` |
|       - |  106 | ` * representation.` |
|       - |  107 | ` */` |
|     294 |  108 | `static sxi64 MemObjStringToInt(ph7_value *pObj)` |
|       1 |  109 |  |
|       - |  110 | `	SyString sVal;` |
|     295 |  111 | `	SyStringInitFromBuf(&sVal,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|     295 |  112 | `	return PH7_TokenValueToInt64(&sVal);` |
|       1 |  113 |  |
|       - |  114 | `/*` |
|       - |  115 | ` * Call a magic class method [i.e: __toString(),__toInt(),...]` |
|       - |  116 | ` * Return SXRET_OK if the magic method is available and have been` |
|       - |  117 | ` * successfully called. Any other return value indicates failure.` |
|       - |  118 | ` */` |
|      84 |  119 | `static sxi32 MemObjCallClassCastMethod(` |
|       - |  120 | `	ph7_vm *pVm,               /* VM that trigger the invocation */` |
|       - |  121 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object] */` |
|       - |  122 | `	const char *zMethod,       /* Magic method name [i.e: __toString] */` |
|       - |  123 | `	sxu32 nLen,                /* Method name length */` |
|       - |  124 | `	ph7_value *pResult         /* OUT: Store the return value of the magic method here */` |
|       - |  125 | `	)` |
|       2 |  126 |  |
|       - |  127 | `	ph7_class_method *pMethod;` |
|       - |  128 | `	/* Check if the method is available */` |
|      86 |  129 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,zMethod,nLen);` |
|      86 |  130 | `	if( pMethod == 0 ){` |
|       - |  131 | `		/* No such method */` |
|     ! 0 |  132 | `		return SXERR_NOTFOUND;` |
|       - |  133 | `	}` |
|       - |  134 | `	/* Invoke the desired method */` |
|      86 |  135 | `	PH7_VmCallClassMethod(&(*pVm),&(*pThis),pMethod,&(*pResult),0,0);` |
|       - |  136 | `	/* Method successfully called,pResult should hold the return value */` |
|      86 |  137 | `	return SXRET_OK;` |
|      44 |  138 |  |
|       - |  139 | `/*` |
|       - |  140 | ` * Return some kind of integer value which is the best we can` |
|       - |  141 | ` * do at representing the value that pObj describes as an integer.` |
|       - |  142 | ` * If pObj is an integer, then the value is exact. If pObj is` |
|       - |  143 | ` * a floating-point then  the value returned is the integer part.` |
|       - |  144 | ` * If pObj is a string, then we make an attempt to convert it into` |
|       - |  145 | ` * a integer and return that.` |
|       - |  146 | ` * If pObj represents a NULL value, return 0.` |
|       - |  147 | ` */` |
|     340 |  148 | `static sxi64 MemObjIntValue(ph7_value *pObj)` |
|       2 |  149 |  |
|       - |  150 | `	sxi32 iFlags;` |
|     342 |  151 | `	iFlags = pObj->iFlags;` |
|     342 |  152 | `	if (iFlags & MEMOBJ_REAL ){` |
|      27 |  153 | `		return MemObjRealToInt(&(*pObj));` |
|     316 |  154 | `	}else if( iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      31 |  155 | `		return pObj->x.iVal;` |
|     286 |  156 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|     263 |  157 | `		return MemObjStringToInt(&(*pObj));` |
|      24 |  158 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|      12 |  159 | `		return 0;` |
|      13 |  160 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|       7 |  161 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|       7 |  162 | `		sxu32 n = pMap->nEntry;` |
|       7 |  163 | `		PH7_HashmapUnref(pMap);` |
|       - |  164 | `		/* Return total number of entries in the hashmap */` |
|       7 |  165 | `		return n;` |
|       7 |  166 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|       - |  167 | `		ph7_value sResult;` |
|       5 |  168 | `		sxi64 iVal = 1;` |
|       - |  169 | `		sxi32 rc;` |
|       - |  170 | `		/* Invoke the [__toInt()] magic method if available [note that this is a symisc extension]  */` |
|       5 |  171 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|       5 |  172 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|       - |  173 | `			"__toInt",sizeof("__toInt")-1,&sResult);` |
|       5 |  174 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_INT) ){` |
|       - |  175 | `			/* Extract method return value */` |
|       5 |  176 | `			iVal = sResult.x.iVal;` |
|       2 |  177 | `		}` |
|       5 |  178 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|       5 |  179 | `		PH7_MemObjRelease(&sResult);` |
|       5 |  180 | `		return iVal;` |
|       3 |  181 | `	}else if(iFlags & MEMOBJ_RES ){` |
|       3 |  182 | `		return pObj->x.pOther != 0;` |
|       - |  183 | `	}` |
|       - |  184 | `	/* CANT HAPPEN */` |
|     ! 0 |  185 | `	return 0;` |
|     172 |  186 |  |
|       - |  187 | `/*` |
|       - |  188 | ` * Return some kind of real value which is the best we can` |
|       - |  189 | ` * do at representing the value that pObj describes as a real.` |
|       - |  190 | ` * If pObj is a real, then the value is exact.If pObj is an` |
|       - |  191 | ` * integer then the integer  is promoted to real and that value` |
|       - |  192 | ` * is returned.` |
|       - |  193 | ` * If pObj is a string, then we make an attempt to convert it` |
|       - |  194 | ` * into a real and return that.` |
|       - |  195 | ` * If pObj represents a NULL value, return 0.0` |
|       - |  196 | ` */` |
|     682 |  197 | `static ph7_real MemObjRealValue(ph7_value *pObj)` |
|       2 |  198 |  |
|       - |  199 | `	sxi32 iFlags;` |
|     684 |  200 | `	iFlags = pObj->iFlags;` |
|     684 |  201 | `	if( iFlags & MEMOBJ_REAL ){` |
|     ! 0 |  202 | `		return pObj->rVal;` |
|     684 |  203 | `	}else if (iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     272 |  204 | `		return (ph7_real)pObj->x.iVal;` |
|     413 |  205 | `	}else if (iFlags & MEMOBJ_STRING){` |
|       - |  206 | `		SyString sString;` |
|       - |  207 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|       - |  208 | `		ph7_real rVal = 0;` |
|       - |  209 | `#else` |
|     407 |  210 | `		ph7_real rVal = 0.0;` |
|       - |  211 | `#endif` |
|     407 |  212 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|     407 |  213 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       - |  214 | `			/* Convert as much as we can */` |
|       - |  215 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|       - |  216 | `			rVal = MemObjStringToInt(&(*pObj));` |
|       - |  217 | `#else` |
|     407 |  218 | `			SyStrToReal(sString.zString,sString.nByte,(void *)&rVal,0);` |
|       - |  219 | `#endif` |
|     203 |  220 | `		}` |
|     407 |  221 | `		return rVal;` |
|       7 |  222 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|       - |  223 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|       - |  224 | `		return 0;` |
|       - |  225 | `#else` |
|     ! 0 |  226 | `		return 0.0;` |
|       - |  227 | `#endif` |
|       7 |  228 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|       - |  229 | `		/* Return the total number of entries in the hashmap */` |
|     ! 0 |  230 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|     ! 0 |  231 | `		ph7_real n = (ph7_real)pMap->nEntry;` |
|     ! 0 |  232 | `		PH7_HashmapUnref(pMap);` |
|     ! 0 |  233 | `		return n;` |
|       7 |  234 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|       - |  235 | `		ph7_value sResult;` |
|       5 |  236 | `		ph7_real rVal = 1;` |
|       - |  237 | `		sxi32 rc;` |
|       - |  238 | `		/* Invoke the [__toFloat()] magic method if available [note that this is a symisc extension]  */` |
|       5 |  239 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|       5 |  240 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|       - |  241 | `			"__toFloat",sizeof("__toFloat")-1,&sResult);` |
|       5 |  242 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_REAL) ){` |
|       - |  243 | `			/* Extract method return value */` |
|       5 |  244 | `			rVal = sResult.rVal;` |
|       2 |  245 | `		}` |
|       5 |  246 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|       5 |  247 | `		PH7_MemObjRelease(&sResult);` |
|       5 |  248 | `		return rVal;` |
|       3 |  249 | `	}else if(iFlags & MEMOBJ_RES ){` |
|       3 |  250 | `		return (ph7_real)(pObj->x.pOther != 0);` |
|       - |  251 | `	}` |
|       - |  252 | `	/* NOT REACHED  */` |
|     ! 0 |  253 | `	return 0;` |
|     343 |  254 |  |
|       - |  255 | `/*` |
|       - |  256 | ` * Return the string representation of a given ph7_value.` |
|       - |  257 | ` * This function never fail and always return SXRET_OK.` |
|       - |  258 | ` */` |
|   51864 |  259 | `static sxi32 MemObjStringValue(SyBlob *pOut,ph7_value *pObj,sxu8 bStrictBool)` |
|       2 |  260 |  |
|   51866 |  261 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|       - |  262 | `		/* Handle special floating-point values first */` |
|     104 |  263 | `		if( PH7_IS_NAN(pObj->rVal) ){` |
|     ! 0 |  264 | `			SyBlobAppend(&(*pOut),"NAN",3);` |
|     104 |  265 | `		}else if( PH7_IS_INF(pObj->rVal) ){` |
|     ! 0 |  266 | `			if( pObj->rVal < 0.0 ){` |
|     ! 0 |  267 | `				SyBlobAppend(&(*pOut),"-INF",4);` |
|     ! 0 |  268 | `			}else{` |
|     ! 0 |  269 | `				SyBlobAppend(&(*pOut),"INF",3);` |
|       - |  270 | `			}` |
|     ! 0 |  271 | `		}else{` |
|     104 |  272 | `			SyBlobFormat(&(*pOut),"%.15g",pObj->rVal);` |
|       2 |  273 | `		}` |
|   51815 |  274 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|   51542 |  275 | `		SyBlobFormat(&(*pOut),"%qd",pObj->x.iVal);` |
|       - |  276 | `		/* %qd (BSD quad) is equivalent to %lld in the libc printf */` |
|   25994 |  277 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|     120 |  278 | `		if( bStrictBool ){` |
|       - |  279 | `			/* Actual string cast: true -> "1", false -> "" (like PHP) */` |
|      12 |  280 | `			if( pObj->x.iVal ){` |
|       3 |  281 | `				SyBlobAppend(&(*pOut),"1",sizeof("1")-1);` |
|       1 |  282 | `			}` |
|       - |  283 | `			/* false produces empty string, nothing to append */` |
|       7 |  284 | `		}else{` |
|       - |  285 | `			/* Display path (var_dump, print_r): show TRUE/FALSE */` |
|     110 |  286 | `			if( pObj->x.iVal ){` |
|      64 |  287 | `				SyBlobAppend(&(*pOut),"TRUE",sizeof("TRUE")-1);` |
|      33 |  288 | `			}else{` |
|      48 |  289 | `				SyBlobAppend(&(*pOut),"FALSE",sizeof("FALSE")-1);` |
|       - |  290 | `			}` |
|       2 |  291 | `		}` |
|     165 |  292 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 |  293 | `		SyBlobAppend(&(*pOut),"Array",sizeof("Array")-1);` |
|       3 |  294 | `		PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
|     105 |  295 | `	}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       - |  296 | `		ph7_value sResult;` |
|       - |  297 | `		sxi32 rc;` |
|       - |  298 | `		/* Invoke the __toString() method if available */` |
|      74 |  299 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|      74 |  300 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|       - |  301 | `			"__toString",sizeof("__toString")-1,&sResult);` |
|      74 |  302 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_STRING) && SyBlobLength(&sResult.sBlob) > 0){` |
|       - |  303 | `			/* Expand method return value */` |
|      70 |  304 | `			SyBlobDup(&sResult.sBlob,pOut);` |
|      36 |  305 | `		}else{` |
|       - |  306 | `			/* Expand "Object" as requested by the PHP language reference manual */` |
|       5 |  307 | `			SyBlobAppend(&(*pOut),"Object",sizeof("Object")-1);` |
|       - |  308 | `		}` |
|      74 |  309 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|      74 |  310 | `		PH7_MemObjRelease(&sResult);` |
|      68 |  311 | `	}else if(pObj->iFlags & MEMOBJ_RES ){` |
|       3 |  312 | `		SyBlobFormat(&(*pOut),"ResourceID_%#x",pObj->x.pOther);` |
|       1 |  313 | `	}` |
|   51866 |  314 | `	return SXRET_OK;` |
|       2 |  315 |  |
|       - |  316 | `/*` |
|       - |  317 | ` * Return some kind of boolean value which is the best we can do` |
|       - |  318 | ` * at representing the value that pObj describes as a boolean.` |
|       - |  319 | ` * When converting to boolean, the following values are considered FALSE:` |
|       - |  320 | ` * NULL` |
|       - |  321 | ` * the boolean FALSE itself.` |
|       - |  322 | ` * the integer 0 (zero).` |
|       - |  323 | ` * the real 0.0 (zero).` |
|       - |  324 | ` * the empty string,a stream of zero [i.e: "0","00","000",...] and the string` |
|       - |  325 | ` * "false".` |
|       - |  326 | ` * an array with zero elements.` |
|       - |  327 | ` */` |
|    8472 |  328 | `static sxi32 MemObjBooleanValue(ph7_value *pObj)` |
|       2 |  329 |  |
|       - |  330 | `	sxi32 iFlags;` |
|    8474 |  331 | `	iFlags = pObj->iFlags;` |
|    8474 |  332 | `	if (iFlags & MEMOBJ_REAL ){` |
|       - |  333 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|       - |  334 | `		return pObj->rVal ? 1 : 0;` |
|       - |  335 | `#else` |
|       5 |  336 | `		return pObj->rVal != 0.0 ? 1 : 0;` |
|       - |  337 | `#endif` |
|    8470 |  338 | `	}else if( iFlags & MEMOBJ_INT ){` |
|      39 |  339 | `		return pObj->x.iVal ? 1 : 0;` |
|    8432 |  340 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|       - |  341 | `		SyString sString;` |
|      35 |  342 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|      35 |  343 | `		if( sString.nByte == 0 ){` |
|       - |  344 | `			/* Empty string */` |
|       5 |  345 | `			return 0;` |
|      31 |  346 | `		}else if( (sString.nByte == sizeof("true") - 1 && SyStrnicmp(sString.zString,"true",sizeof("true")-1) == 0) \|\|` |
|      31 |  347 | `			(sString.nByte == sizeof("on") - 1 && SyStrnicmp(sString.zString,"on",sizeof("on")-1) == 0) \|\|` |
|      30 |  348 | `			(sString.nByte == sizeof("yes") - 1 && SyStrnicmp(sString.zString,"yes",sizeof("yes")-1) == 0) ){` |
|     ! 0 |  349 | `				return 1;` |
|      31 |  350 | `		}else if( sString.nByte == sizeof("false") - 1 && SyStrnicmp(sString.zString,"false",sizeof("false")-1) == 0 ){` |
|     ! 0 |  351 | `			return 0;` |
|     ! 0 |  352 | `		}else{` |
|       - |  353 | `			const char *zIn,*zEnd;` |
|      31 |  354 | `			zIn = sString.zString;` |
|      31 |  355 | `			zEnd = &zIn[sString.nByte];` |
|      31 |  356 | `			while( zIn < zEnd && zIn[0] == '0' ){` |
|     ! 0 |  357 | `				zIn++;` |
|     ! 0 |  358 | `			}` |
|      31 |  359 | `			return zIn >= zEnd ? 0 : 1;` |
|     ! 0 |  360 | `		}` |
|    8398 |  361 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|    7522 |  362 | `		return 0;` |
|     878 |  363 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|      15 |  364 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      15 |  365 | `		sxu32 n = pMap->nEntry;` |
|      15 |  366 | `		PH7_HashmapUnref(pMap);` |
|      15 |  367 | `		return n > 0 ? TRUE : FALSE;` |
|     864 |  368 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|       - |  369 | `		ph7_value sResult;` |
|       5 |  370 | `		sxi32 iVal = 1;` |
|       - |  371 | `		sxi32 rc;` |
|       - |  372 | `		/* Invoke the __toBool() method if available [note that this is a symisc extension]  */` |
|       5 |  373 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|       5 |  374 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|       - |  375 | `			"__toBool",sizeof("__toBool")-1,&sResult);` |
|       5 |  376 | `		if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL)) ){` |
|       - |  377 | `			/* Extract method return value */` |
|       5 |  378 | `			iVal = (sxi32)(sResult.x.iVal != 0); /* Stupid cc warning -W -Wall -O6 */` |
|       2 |  379 | `		}` |
|       5 |  380 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|       5 |  381 | `		PH7_MemObjRelease(&sResult);` |
|       5 |  382 | `		return iVal;` |
|     860 |  383 | `	}else if(iFlags & MEMOBJ_RES ){` |
|     860 |  384 | `		return pObj->x.pOther != 0;` |
|       - |  385 | `	}` |
|       - |  386 | `	/* NOT REACHED */` |
|     ! 0 |  387 | `	return 0;` |
|    4238 |  388 |  |
|       - |  389 | `/*` |
|       - |  390 | ` * If the ph7_value is of type real,try to make it an integer also.` |
|       - |  391 | ` */` |
|    1298 |  392 | `static sxi32 MemObjTryIntger(ph7_value *pObj)` |
|       2 |  393 |  |
|    1300 |  394 | `	pObj->x.iVal = MemObjRealToInt(&(*pObj));` |
|       - |  395 | `  /* Only mark the value as an integer if` |
|       - |  396 | `  **` |
|       - |  397 | `  **    (1) the round-trip conversion real->int->real is a no-op, and` |
|       - |  398 | `  **    (2) The integer is neither the largest nor the smallest` |
|       - |  399 | `  **        possible integer` |
|       - |  400 | `  **` |
|       - |  401 | `  ** The second and third terms in the following conditional enforces` |
|       - |  402 | `  ** the second condition under the assumption that addition overflow causes` |
|       - |  403 | `  ** values to wrap around.  On x86 hardware, the third term is always` |
|       - |  404 | `  ** true and could be omitted.  But we leave it in because other` |
|       - |  405 | `  ** architectures might behave differently.` |
|       - |  406 | `  */` |
|    1298 |  407 | `	if( pObj->rVal ==(ph7_real)pObj->x.iVal && pObj->x.iVal>SMALLEST_INT64` |
|     666 |  408 | `      && pObj->x.iVal<LARGEST_INT64 ){` |
|     666 |  409 | `		  pObj->iFlags \|= MEMOBJ_INT;` |
|     334 |  410 | `	}` |
|    1300 |  411 | `	return SXRET_OK;` |
|       2 |  412 |  |
|       - |  413 | `/*` |
|       - |  414 | ` * Convert a ph7_value to type integer.Invalidate any prior representations.` |
|       - |  415 | ` */` |
|  242506 |  416 | `PH7_PRIVATE sxi32 PH7_MemObjToInteger(ph7_value *pObj)` |
|       2 |  417 |  |
|  242508 |  418 | `	if( (pObj->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  419 | `		/* Preform the conversion */` |
|     342 |  420 | `		pObj->x.iVal = MemObjIntValue(&(*pObj));` |
|       - |  421 | `		/* Invalidate any prior representations */` |
|     342 |  422 | `		SyBlobRelease(&pObj->sBlob);` |
|     342 |  423 | `		MemObjSetType(pObj,MEMOBJ_INT);` |
|     170 |  424 | `	}` |
|  242508 |  425 | `	return SXRET_OK;` |
|       2 |  426 |  |
|       - |  427 | `/*` |
|       - |  428 | ` * Convert a ph7_value to type real (Try to get an integer representation also).` |
|       - |  429 | ` * Invalidate any prior representations` |
|       - |  430 | ` */` |
|     896 |  431 | `PH7_PRIVATE sxi32 PH7_MemObjToReal(ph7_value *pObj)` |
|       2 |  432 |  |
|     898 |  433 | `	if((pObj->iFlags & MEMOBJ_REAL) == 0 ){` |
|       - |  434 | `		/* Preform the conversion */` |
|     684 |  435 | `		pObj->rVal = MemObjRealValue(&(*pObj));` |
|       - |  436 | `		/* Invalidate any prior representations */` |
|     684 |  437 | `		SyBlobRelease(&pObj->sBlob);` |
|     684 |  438 | `		MemObjSetType(pObj,MEMOBJ_REAL);` |
|       - |  439 | `		/* Try to get an integer representation */` |
|     684 |  440 | `		MemObjTryIntger(&(*pObj));` |
|     341 |  441 | `	}` |
|     898 |  442 | `	return SXRET_OK;` |
|       2 |  443 |  |
|       - |  444 | `/*` |
|       - |  445 | ` * Convert a ph7_value to type boolean.Invalidate any prior representations.` |
|       - |  446 | ` */` |
|    8590 |  447 | `PH7_PRIVATE sxi32 PH7_MemObjToBool(ph7_value *pObj)` |
|       2 |  448 |  |
|    8592 |  449 | `	if( (pObj->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       - |  450 | `		/* Preform the conversion */` |
|    8474 |  451 | `		pObj->x.iVal = MemObjBooleanValue(&(*pObj));` |
|       - |  452 | `		/* Invalidate any prior representations */` |
|    8474 |  453 | `		SyBlobRelease(&pObj->sBlob);` |
|    8474 |  454 | `		MemObjSetType(pObj,MEMOBJ_BOOL);` |
|    4236 |  455 | `	}` |
|    8592 |  456 | `	return SXRET_OK;` |
|       2 |  457 |  |
|       - |  458 | `/*` |
|       - |  459 | ` * Convert a ph7_value to type string.Prior representations are NOT invalidated.` |
|       - |  460 | ` */` |
|  529332 |  461 | `PH7_PRIVATE sxi32 PH7_MemObjToString(ph7_value *pObj)` |
|       2 |  462 |  |
|  529334 |  463 | `	sxi32 rc = SXRET_OK;` |
|  529334 |  464 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  465 | `		/* Perform the conversion */` |
|   51646 |  466 | `		SyBlobReset(&pObj->sBlob); /* Reset the internal buffer */` |
|   51646 |  467 | `		rc = MemObjStringValue(&pObj->sBlob,&(*pObj),TRUE);` |
|   51646 |  468 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|   25822 |  469 | `	}` |
|  529334 |  470 | `	return rc;` |
|       2 |  471 |  |
|       - |  472 | `/*` |
|       - |  473 | ` * Nullify a ph7_value.In other words invalidate any prior` |
|       - |  474 | ` * representation.` |
|       - |  475 | ` */` |
|     ! 0 |  476 | `PH7_PRIVATE sxi32 PH7_MemObjToNull(ph7_value *pObj)` |
|     ! 0 |  477 |  |
|     ! 0 |  478 | `	return PH7_MemObjRelease(pObj);` |
|     ! 0 |  479 |  |
|       - |  480 | `/*` |
|       - |  481 | ` * Convert a ph7_value to type array.Invalidate any prior representations.` |
|       - |  482 | `  * According to the PHP language reference manual.` |
|       - |  483 | `  *   For any of the types: integer, float, string, boolean converting a value` |
|       - |  484 | `  *   to an array results in an array with a single element with index zero` |
|       - |  485 | `  *   and the value of the scalar which was converted.` |
|       - |  486 | `  */` |
|      20 |  487 | `PH7_PRIVATE sxi32 PH7_MemObjToHashmap(ph7_value *pObj)` |
|       1 |  488 |  |
|      21 |  489 | `	if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       - |  490 | `		ph7_hashmap *pMap;` |
|       - |  491 | `		/* Allocate a new hashmap instance */` |
|      21 |  492 | `		pMap = PH7_NewHashmap(pObj->pVm,0,0);` |
|      21 |  493 | `		if( pMap == 0 ){` |
|     ! 0 |  494 | `			return SXERR_MEM;` |
|       - |  495 | `		}` |
|      21 |  496 | `		if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|       - |  497 | `			/*` |
|       - |  498 | `			 * According to the PHP language reference manual.` |
|       - |  499 | `			 *   For any of the types: integer, float, string, boolean converting a value` |
|       - |  500 | `			 *   to an array results in an array with a single element with index zero` |
|       - |  501 | `			 *   and the value of the scalar which was converted.` |
|       - |  502 | `			 */` |
|      17 |  503 | `			if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       - |  504 | `				/* Object cast */` |
|       7 |  505 | `				PH7_ClassInstanceToHashmap((ph7_class_instance *)pObj->x.pOther,pMap);` |
|       4 |  506 | `			}else{` |
|       - |  507 | `				/* Insert a single element */` |
|      11 |  508 | `				PH7_HashmapInsert(pMap,0/* Automatic index assign */,&(*pObj));` |
|       - |  509 | `			}` |
|      17 |  510 | `			SyBlobRelease(&pObj->sBlob);` |
|       8 |  511 | `		}` |
|       - |  512 | `		/* Invalidate any prior representation */` |
|      21 |  513 | `		PH7_MemObjRelease(pObj);` |
|      21 |  514 | `		MemObjSetType(pObj,MEMOBJ_HASHMAP);` |
|      21 |  515 | `		pObj->x.pOther = pMap;` |
|      10 |  516 | `	}` |
|      21 |  517 | `	return SXRET_OK;` |
|      11 |  518 |  |
|       - |  519 | `/*` |
|       - |  520 | ` * Convert a ph7_value to type object.Invalidate any prior representations.` |
|       - |  521 | ` * The new object is instantiated from the builtin stdClass().` |
|       - |  522 | ` * The stdClass() class have a single attribute which is '$value'. This attribute` |
|       - |  523 | ` * hold a copy of the converted ph7_value.` |
|       - |  524 | ` * The internal of the stdClass is as follows:` |
|       - |  525 | ` * class stdClass{` |
|       - |  526 | ` *	 public $value;` |
|       - |  527 | ` *	 public function __toInt(){ return (int)$this->value; }` |
|       - |  528 | ` *	 public function __toBool(){ return (bool)$this->value; }` |
|       - |  529 | ` *	 public function __toFloat(){ return (float)$this->value; }` |
|       - |  530 | ` *	 public function __toString(){ return (string)$this->value; }` |
|       - |  531 | ` *	 function __construct($v){ $this->value = $v; }"` |
|       - |  532 | ` *  }` |
|       - |  533 | ` * Refer to the official documentation for more information.` |
|       - |  534 | ` */` |
|      16 |  535 | `PH7_PRIVATE sxi32 PH7_MemObjToObject(ph7_value *pObj)` |
|       1 |  536 |  |
|      17 |  537 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|       - |  538 | `		ph7_class_instance *pStd;` |
|       - |  539 | `		ph7_class_method *pCons;` |
|       - |  540 | `		ph7_class *pClass;` |
|       - |  541 | `		ph7_vm *pVm;` |
|       - |  542 | `		/* Point to the underlying VM */` |
|      17 |  543 | `		pVm = pObj->pVm;` |
|       - |  544 | `		/* Point to the stdClass() */` |
|      17 |  545 | `		pClass = PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,0,0);` |
|      17 |  546 | `		if( pClass == 0 ){` |
|       - |  547 | `			/* Can't happen,load null instead */` |
|     ! 0 |  548 | `			PH7_MemObjRelease(pObj);` |
|     ! 0 |  549 | `			return SXRET_OK;` |
|       - |  550 | `		}` |
|       - |  551 | `		/* Instanciate a new stdClass() object */` |
|      17 |  552 | `		pStd = PH7_NewClassInstance(pVm,pClass);` |
|      17 |  553 | `		if( pStd == 0 ){` |
|       - |  554 | `			/* Out of memory */` |
|     ! 0 |  555 | `			PH7_MemObjRelease(pObj);` |
|     ! 0 |  556 | `			return SXRET_OK;` |
|       - |  557 | `		}` |
|       - |  558 | `		/* Check if a constructor is available */` |
|      17 |  559 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      17 |  560 | `		if( pCons ){` |
|       - |  561 | `			ph7_value *apArg[2];` |
|       - |  562 | `			/* Invoke the constructor with one argument */` |
|      17 |  563 | `			apArg[0] = pObj;` |
|      17 |  564 | `			PH7_VmCallClassMethod(pVm,pStd,pCons,0,1,apArg);` |
|      17 |  565 | `			if( pStd->iRef < 1 ){` |
|     ! 0 |  566 | `				pStd->iRef = 1;` |
|     ! 0 |  567 | `			}` |
|       8 |  568 | `		}` |
|       - |  569 | `		/* Invalidate any prior representation */` |
|      17 |  570 | `		PH7_MemObjRelease(pObj);` |
|       - |  571 | `		/* Save the new instance */` |
|      17 |  572 | `		pObj->x.pOther = pStd;` |
|      17 |  573 | `		MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       8 |  574 | `	}` |
|      17 |  575 | `	return SXRET_OK;` |
|       9 |  576 |  |
|       - |  577 | `/*` |
|       - |  578 | ` * Return a pointer to the appropriate convertion method associated` |
|       - |  579 | ` * with the given type.` |
|       - |  580 | ` * Note on type juggling.` |
|       - |  581 | ` * Accoding to the PHP language reference manual` |
|       - |  582 | ` *  PHP does not require (or support) explicit type definition in variable` |
|       - |  583 | ` *  declaration; a variable's type is determined by the context in which` |
|       - |  584 | ` *  the variable is used. That is to say, if a string value is assigned` |
|       - |  585 | ` *  to variable $var, $var becomes a string. If an integer value is then` |
|       - |  586 | ` *  assigned to $var, it becomes an integer.` |
|       - |  587 | ` */` |
|     ! 0 |  588 | `PH7_PRIVATE ProcMemObjCast PH7_MemObjCastMethod(sxi32 iFlags)` |
|     ! 0 |  589 |  |
|     ! 0 |  590 | `	if( iFlags & MEMOBJ_STRING ){` |
|     ! 0 |  591 | `		return PH7_MemObjToString;` |
|     ! 0 |  592 | `	}else if( iFlags & MEMOBJ_INT ){` |
|     ! 0 |  593 | `		return PH7_MemObjToInteger;` |
|     ! 0 |  594 | `	}else if( iFlags & MEMOBJ_REAL ){` |
|     ! 0 |  595 | `		return PH7_MemObjToReal;` |
|     ! 0 |  596 | `	}else if( iFlags & MEMOBJ_BOOL ){` |
|     ! 0 |  597 | `		return PH7_MemObjToBool;` |
|     ! 0 |  598 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|     ! 0 |  599 | `		return PH7_MemObjToHashmap;` |
|     ! 0 |  600 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|     ! 0 |  601 | `		return PH7_MemObjToObject;` |
|       - |  602 | `	}` |
|       - |  603 | `	/* NULL cast */` |
|     ! 0 |  604 | `	return PH7_MemObjToNull;` |
|     ! 0 |  605 |  |
|       - |  606 | `/*` |
|       - |  607 | ` * Check whether the ph7_value is numeric [i.e: int/float/bool] or looks` |
|       - |  608 | ` * like a numeric number [i.e: if the ph7_value is of type string.].` |
|       - |  609 | ` * Return TRUE if numeric.FALSE otherwise.` |
|       - |  610 | ` */` |
|  269176 |  611 | `PH7_PRIVATE sxi32 PH7_MemObjIsNumeric(ph7_value *pObj)` |
|       2 |  612 |  |
|  269178 |  613 | `	if( pObj->iFlags & ( MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      48 |  614 | `		return TRUE;` |
|  269132 |  615 | `	}else if( pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|       6 |  616 | `		return FALSE;` |
|  269128 |  617 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       - |  618 | `		SyString sStr;` |
|       - |  619 | `		sxi32 rc;` |
|  269128 |  620 | `		SyStringInitFromBuf(&sStr,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|  269128 |  621 | `		if( sStr.nByte <= 0 ){` |
|       - |  622 | `			/* Empty string */` |
|      73 |  623 | `			return FALSE;` |
|       - |  624 | `		}` |
|       - |  625 | `		/* Check if the string representation looks like a numeric number */` |
|  269056 |  626 | `		rc = SyStrIsNumeric(sStr.zString,sStr.nByte,0,0);` |
|  269056 |  627 | `		return rc == SXRET_OK ? TRUE : FALSE;` |
|       - |  628 | `	}` |
|       - |  629 | `	/* NOT REACHED */` |
|     ! 0 |  630 | `	return FALSE;` |
|  134688 |  631 |  |
|       - |  632 | `/*` |
|       - |  633 | ` * Check whether the ph7_value is empty.Return TRUE if empty.` |
|       - |  634 | ` * FALSE otherwise.` |
|       - |  635 | ` * An ph7_value is considered empty if the following are true:` |
|       - |  636 | ` * NULL value.` |
|       - |  637 | ` * Boolean FALSE.` |
|       - |  638 | ` * Integer/Float with a 0 (zero) value.` |
|       - |  639 | ` * An empty string or a stream of 0 (zero) [i.e: "0","00","000",...].` |
|       - |  640 | ` * An empty array.` |
|       - |  641 | ` * NOTE` |
|       - |  642 | ` *  OBJECT VALUE MUST NOT BE MODIFIED.` |
|       - |  643 | ` */` |
|   17672 |  644 | `PH7_PRIVATE sxi32 PH7_MemObjIsEmpty(ph7_value *pObj)` |
|       2 |  645 |  |
|   17674 |  646 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|      10 |  647 | `		return TRUE;` |
|   17666 |  648 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|      13 |  649 | `		return pObj->x.iVal == 0 ? TRUE : FALSE;` |
|   17654 |  650 | `	}else if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 |  651 | `		return pObj->rVal == (ph7_real)0 ? TRUE : FALSE;` |
|   17654 |  652 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|       5 |  653 | `		return !pObj->x.iVal;` |
|   17650 |  654 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|   17618 |  655 | `		if( SyBlobLength(&pObj->sBlob) <= 0 ){` |
|   14968 |  656 | `			return TRUE;` |
|     ! 0 |  657 | `		}else{` |
|       - |  658 | `			const char *zIn,*zEnd;` |
|    2652 |  659 | `			zIn = (const char *)SyBlobData(&pObj->sBlob);` |
|    2652 |  660 | `			zEnd = &zIn[SyBlobLength(&pObj->sBlob)];` |
|    2652 |  661 | `			while( zIn < zEnd ){` |
|    2652 |  662 | `				if( zIn[0] != '0' ){` |
|    2652 |  663 | `					break;` |
|       - |  664 | `				}` |
|     ! 0 |  665 | `				zIn++;` |
|     ! 0 |  666 | `			}` |
|    2652 |  667 | `			return zIn >= zEnd ? TRUE : FALSE;` |
|     ! 0 |  668 | `		}` |
|      34 |  669 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|      34 |  670 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      34 |  671 | `		return pMap->nEntry == 0 ? TRUE : FALSE;` |
|     ! 0 |  672 | `	}else if ( pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|     ! 0 |  673 | `		return FALSE;` |
|       - |  674 | `	}` |
|       - |  675 | `	/* Assume empty by default */` |
|     ! 0 |  676 | `	return TRUE;` |
|    8838 |  677 |  |
|       - |  678 | `/*` |
|       - |  679 | ` * Convert a ph7_value so that it has types MEMOBJ_REAL or MEMOBJ_INT` |
|       - |  680 | ` * or both.` |
|       - |  681 | ` * Invalidate any prior representations. Every effort is made to force` |
|       - |  682 | ` * the conversion, even if the input is a string that does not look` |
|       - |  683 | ` * completely like a number.Convert as much of the string as we can` |
|       - |  684 | ` * and ignore the rest.` |
|       - |  685 | ` */` |
|  317424 |  686 | `PH7_PRIVATE sxi32 PH7_MemObjToNumeric(ph7_value *pObj)` |
|       2 |  687 |  |
|  317426 |  688 | `	if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|  317392 |  689 | `		if( pObj->iFlags & (MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|       3 |  690 | `			if( pObj->iFlags & MEMOBJ_NULL ){` |
|     ! 0 |  691 | `				pObj->x.iVal = 0;` |
|     ! 0 |  692 | `			}` |
|       3 |  693 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|       1 |  694 | `		}` |
|       - |  695 | `		/* Already numeric */` |
|  317392 |  696 | `		return  SXRET_OK;` |
|       - |  697 | `	}` |
|      35 |  698 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|      35 |  699 | `		sxi32 rc = SXERR_INVALID;` |
|      35 |  700 | `		sxu8 bReal = FALSE;` |
|       - |  701 | `		SyString sString;` |
|      35 |  702 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|       - |  703 | `		/* Check if the given string looks like a numeric number */` |
|      35 |  704 | `		if( sString.nByte > 0 ){` |
|      35 |  705 | `			rc = SyStrIsNumeric(sString.zString,sString.nByte,&bReal,0);` |
|      17 |  706 | `		}` |
|      35 |  707 | `		if( bReal ){` |
|       3 |  708 | `			PH7_MemObjToReal(&(*pObj));` |
|       2 |  709 | `		}else{` |
|      33 |  710 | `			if( rc != SXRET_OK ){` |
|       - |  711 | `				/* The input does not look at all like a number,set the value to 0 */` |
|     ! 0 |  712 | `				pObj->x.iVal = 0;` |
|     ! 0 |  713 | `			}else{` |
|       - |  714 | `				/* Convert as much as we can */` |
|      33 |  715 | `				pObj->x.iVal = MemObjStringToInt(&(*pObj));` |
|       - |  716 | `			}` |
|      33 |  717 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|      33 |  718 | `			SyBlobRelease(&pObj->sBlob);` |
|       1 |  719 | `		}` |
|      17 |  720 | `	}else if(pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)){` |
|     ! 0 |  721 | `		PH7_MemObjToInteger(pObj);` |
|     ! 0 |  722 | `	}else{` |
|       - |  723 | `		/* Perform a blind cast */` |
|     ! 0 |  724 | `		PH7_MemObjToReal(&(*pObj));` |
|       - |  725 | `	}` |
|      35 |  726 | `	return SXRET_OK;` |
|  158736 |  727 |  |
|       - |  728 | `/*` |
|       - |  729 | ` * Try a get an integer representation of the given ph7_value.` |
|       - |  730 | ` * If the ph7_value is not of type real,this function is a no-op.` |
|       - |  731 | ` */` |
|     592 |  732 | `PH7_PRIVATE sxi32 PH7_MemObjTryInteger(ph7_value *pObj)` |
|       1 |  733 |  |
|     593 |  734 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|       - |  735 | `		/* Work only with reals */` |
|     593 |  736 | `		MemObjTryIntger(&(*pObj));` |
|     296 |  737 | `	}` |
|     593 |  738 | `	return SXRET_OK;` |
|       1 |  739 |  |
|       - |  740 | `/*` |
|       - |  741 | ` * Initialize a ph7_value to the null type.` |
|       - |  742 | ` */` |
| 5061582 |  743 | `PH7_PRIVATE sxi32 PH7_MemObjInit(ph7_vm *pVm,ph7_value *pObj)` |
|       2 |  744 |  |
|       - |  745 | `	/* Zero the structure */` |
| 5061584 |  746 | `	SyZero(pObj,sizeof(ph7_value));` |
|       - |  747 | `	/* Initialize fields */` |
| 5061584 |  748 | `	pObj->pVm = pVm;` |
| 5061584 |  749 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|       - |  750 | `	/* Set the NULL type */` |
| 5061584 |  751 | `	pObj->iFlags = MEMOBJ_NULL;` |
| 5061584 |  752 | `	return SXRET_OK;` |
|       2 |  753 |  |
|       - |  754 | `/*` |
|       - |  755 | ` * Initialize a ph7_value to the integer type.` |
|       - |  756 | ` */` |
|   79494 |  757 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromInt(ph7_vm *pVm,ph7_value *pObj,sxi64 iVal)` |
|       2 |  758 |  |
|       - |  759 | `	/* Zero the structure */` |
|   79496 |  760 | `	SyZero(pObj,sizeof(ph7_value));` |
|       - |  761 | `	/* Initialize fields */` |
|   79496 |  762 | `	pObj->pVm = pVm;` |
|   79496 |  763 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|       - |  764 | `	/* Set the desired type */` |
|   79496 |  765 | `	pObj->x.iVal = iVal;` |
|   79496 |  766 | `	pObj->iFlags = MEMOBJ_INT;` |
|   79496 |  767 | `	return SXRET_OK;` |
|       2 |  768 |  |
|       - |  769 | `/*` |
|       - |  770 | ` * Initialize a ph7_value to the boolean type.` |
|       - |  771 | ` */` |
|   10498 |  772 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromBool(ph7_vm *pVm,ph7_value *pObj,sxi32 iVal)` |
|       2 |  773 |  |
|       - |  774 | `	/* Zero the structure */` |
|   10500 |  775 | `	SyZero(pObj,sizeof(ph7_value));` |
|       - |  776 | `	/* Initialize fields */` |
|   10500 |  777 | `	pObj->pVm = pVm;` |
|   10500 |  778 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|       - |  779 | `	/* Set the desired type */` |
|   10500 |  780 | `	pObj->x.iVal = iVal ? 1 : 0;` |
|   10500 |  781 | `	pObj->iFlags = MEMOBJ_BOOL;` |
|   10500 |  782 | `	return SXRET_OK;` |
|       2 |  783 |  |
|       - |  784 | `#if 0` |
|       - |  785 | `/*` |
|       - |  786 | ` * Initialize a ph7_value to the real type.` |
|       - |  787 | ` */` |
|       - |  788 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromReal(ph7_vm *pVm,ph7_value *pObj,ph7_real rVal)` |
|       - |  789 |  |
|       - |  790 | `	/* Zero the structure */` |
|       - |  791 | `	SyZero(pObj,sizeof(ph7_value));` |
|       - |  792 | `	/* Initialize fields */` |
|       - |  793 | `	pObj->pVm = pVm;` |
|       - |  794 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|       - |  795 | `	/* Set the desired type */` |
|       - |  796 | `	pObj->rVal = rVal;` |
|       - |  797 | `	pObj->iFlags = MEMOBJ_REAL;` |
|       - |  798 | `	return SXRET_OK;` |
|       - |  799 |  |
|       - |  800 | `#endif` |
|       - |  801 | `/*` |
|       - |  802 | ` * Initialize a ph7_value to the array type.` |
|       - |  803 | ` */` |
|   21678 |  804 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromArray(ph7_vm *pVm,ph7_value *pObj,ph7_hashmap *pArray)` |
|       2 |  805 |  |
|       - |  806 | `	/* Zero the structure */` |
|   21680 |  807 | `	SyZero(pObj,sizeof(ph7_value));` |
|       - |  808 | `	/* Initialize fields */` |
|   21680 |  809 | `	pObj->pVm = pVm;` |
|   21680 |  810 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|       - |  811 | `	/* Set the desired type */` |
|   21680 |  812 | `	pObj->iFlags = MEMOBJ_HASHMAP;` |
|   21680 |  813 | `	pObj->x.pOther = pArray;` |
|   21680 |  814 | `	return SXRET_OK;` |
|       2 |  815 |  |
|       - |  816 | `/*` |
|       - |  817 | ` * Initialize a ph7_value to the string type.` |
|       - |  818 | ` */` |
|  172714 |  819 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromString(ph7_vm *pVm,ph7_value *pObj,const SyString *pVal)` |
|       2 |  820 |  |
|       - |  821 | `	/* Zero the structure */` |
|  172716 |  822 | `	SyZero(pObj,sizeof(ph7_value));` |
|       - |  823 | `	/* Initialize fields */` |
|  172716 |  824 | `	pObj->pVm = pVm;` |
|  172716 |  825 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|  172716 |  826 | `	if( pVal ){` |
|       - |  827 | `		/* Append contents */` |
|  109170 |  828 | `		SyBlobAppend(&pObj->sBlob,(const void *)pVal->zString,pVal->nByte);` |
|   54584 |  829 | `	}` |
|       - |  830 | `	/* Set the desired type */` |
|  172716 |  831 | `	pObj->iFlags = MEMOBJ_STRING;` |
|  172716 |  832 | `	return SXRET_OK;` |
|       2 |  833 |  |
|       - |  834 | `/*` |
|       - |  835 | ` * Append some contents to the internal buffer of a given ph7_value.` |
|       - |  836 | ` * If the given ph7_value is not of type string,this function` |
|       - |  837 | ` * invalidate any prior representation and set the string type.` |
|       - |  838 | ` * Then a simple append operation is performed.` |
|       - |  839 | ` */` |
|  181266 |  840 | `PH7_PRIVATE sxi32 PH7_MemObjStringAppend(ph7_value *pObj,const char *zData,sxu32 nLen)` |
|       2 |  841 |  |
|       - |  842 | `	sxi32 rc;` |
|  181268 |  843 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  844 | `		/* Invalidate any prior representation */` |
|       5 |  845 | `		PH7_MemObjRelease(pObj);` |
|       5 |  846 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|       2 |  847 | `	}` |
|       - |  848 | `	/* Append contents */` |
|  181268 |  849 | `	rc = SyBlobAppend(&pObj->sBlob,zData,nLen);` |
|  181268 |  850 | `	return rc;` |
|       2 |  851 |  |
|       - |  852 | `#if 0` |
|       - |  853 | `/*` |
|       - |  854 | ` * Format and append some contents to the internal buffer of a given ph7_value.` |
|       - |  855 | ` * If the given ph7_value is not of type string,this function invalidate` |
|       - |  856 | ` * any prior representation and set the string type.` |
|       - |  857 | ` * Then a simple format and append operation is performed.` |
|       - |  858 | ` */` |
|       - |  859 | `PH7_PRIVATE sxi32 PH7_MemObjStringFormat(ph7_value *pObj,const char *zFormat,va_list ap)` |
|       - |  860 |  |
|       - |  861 | `	sxi32 rc;` |
|       - |  862 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  863 | `		/* Invalidate any prior representation */` |
|       - |  864 | `		PH7_MemObjRelease(pObj);` |
|       - |  865 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|       - |  866 | `	}` |
|       - |  867 | `	/* Format and append contents */` |
|       - |  868 | `	rc = SyBlobFormatAp(&pObj->sBlob,zFormat,ap);` |
|       - |  869 | `	return rc;` |
|       - |  870 |  |
|       - |  871 | `#endif` |
|       - |  872 | `/*` |
|       - |  873 | ` * Duplicate the contents of a ph7_value.` |
|       - |  874 | ` */` |
| 3666750 |  875 | `PH7_PRIVATE sxi32 PH7_MemObjStore(ph7_value *pSrc,ph7_value *pDest)` |
|       2 |  876 |  |
| 3666752 |  877 | `	ph7_class_instance *pObj = 0;` |
| 3666752 |  878 | `	ph7_hashmap *pMap = 0;` |
|       - |  879 | `	sxi32 rc;` |
| 3666752 |  880 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|       - |  881 | `		/* Increment reference count */` |
|   95142 |  882 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
| 3619182 |  883 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|       - |  884 | `		/* Increment reference count */` |
|    1112 |  885 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|     555 |  886 | `	}` |
| 3666752 |  887 | `	if( pDest->iFlags & MEMOBJ_HASHMAP ){` |
|   28078 |  888 | `		pMap = (ph7_hashmap *)pDest->x.pOther;` |
| 3652714 |  889 | `	}else if( pDest->iFlags & MEMOBJ_OBJ ){` |
|     738 |  890 | `		pObj = (ph7_class_instance *)pDest->x.pOther;` |
|     368 |  891 | `	}` |
| 3666752 |  892 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
| 3666752 |  893 | `	pDest->iFlags &= ~MEMOBJ_AUX;` |
| 3666752 |  894 | `	rc = SXRET_OK;` |
| 3666752 |  895 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
| 3191200 |  896 | `		SyBlobReset(&pDest->sBlob);` |
| 3191200 |  897 | `		rc = SyBlobDup(&pSrc->sBlob,&pDest->sBlob);` |
| 1595601 |  898 | `	}else{` |
|  475554 |  899 | `		if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|  193012 |  900 | `			SyBlobRelease(&pDest->sBlob);` |
|   96527 |  901 | `		}` |
|       - |  902 | `	}` |
| 3666752 |  903 | `	if( pMap ){` |
|   28078 |  904 | `		PH7_HashmapUnref(pMap);` |
| 3652714 |  905 | `	}else if( pObj ){` |
|     738 |  906 | `		PH7_ClassInstanceUnref(pObj);` |
|     368 |  907 | `	}` |
| 3666752 |  908 | `	return rc;` |
|       2 |  909 |  |
|       - |  910 | `/*` |
|       - |  911 | ` * Duplicate the contents of a ph7_value but do not copy internal` |
|       - |  912 | ` * buffer contents,simply point to it.` |
|       - |  913 | ` */` |
| 4086786 |  914 | `PH7_PRIVATE sxi32 PH7_MemObjLoad(ph7_value *pSrc,ph7_value *pDest)` |
|       2 |  915 |  |
| 4086788 |  916 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),` |
|       - |  917 | `		sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
| 4086788 |  918 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|       - |  919 | `		/* Increment reference count */` |
|  285086 |  920 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
| 3944246 |  921 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|       - |  922 | `		/* Increment reference count */` |
|    3550 |  923 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|    1774 |  924 | `	}` |
| 4086788 |  925 | `	if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|      19 |  926 | `		SyBlobRelease(&pDest->sBlob);` |
|       9 |  927 | `	}` |
| 4086788 |  928 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
| 2124466 |  929 | `		SyBlobReadOnly(&pDest->sBlob,SyBlobData(&pSrc->sBlob),SyBlobLength(&pSrc->sBlob));` |
| 1062374 |  930 | `	}` |
| 4086788 |  931 | `	return SXRET_OK;` |
|       2 |  932 |  |
|       - |  933 | `/*` |
|       - |  934 | ` * Invalidate any prior representation of a given ph7_value.` |
|       - |  935 | ` */` |
| 8692164 |  936 | `PH7_PRIVATE sxi32 PH7_MemObjRelease(ph7_value *pObj)` |
|       2 |  937 |  |
| 8692166 |  938 | `	if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
| 8080132 |  939 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|  353278 |  940 | `			PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
| 7903494 |  941 | `		}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|    5880 |  942 | `			PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|    2939 |  943 | `		}` |
|       - |  944 | `		/* Release the internal buffer */` |
| 8080132 |  945 | `		SyBlobRelease(&pObj->sBlob);` |
|       - |  946 | `		/* Invalidate any prior representation */` |
| 8080132 |  947 | `		pObj->iFlags = MEMOBJ_NULL;` |
| 4040342 |  948 | `	}` |
| 8692166 |  949 | `	return SXRET_OK;` |
|       2 |  950 |  |
|       - |  951 | `/*` |
|       - |  952 | ` * Compare two ph7_values.` |
|       - |  953 | ` * Return 0 if the values are equals, > 0 if pObj1 is greater than pObj2` |
|       - |  954 | ` * or < 0 if pObj2 is greater than pObj1.` |
|       - |  955 | ` * Type comparison table taken from the PHP language reference manual.` |
|       - |  956 | ` * Comparisons of $x with PHP functions Expression` |
|       - |  957 | ` *              gettype() 	empty() 	is_null() 	isset() 	boolean : if($x)` |
|       - |  958 | ` * $x = ""; 	string 	    TRUE 	FALSE 	TRUE 	FALSE` |
|       - |  959 | ` * $x = null 	NULL 	    TRUE 	TRUE 	FALSE 	FALSE` |
|       - |  960 | ` * var $x; 	    NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|       - |  961 | ` * $x is undefined 	NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|       - |  962 | ` *  $x = array(); 	array 	TRUE 	FALSE 	TRUE 	FALSE` |
|       - |  963 | ` * $x = false; 	boolean 	TRUE 	FALSE 	TRUE 	FALSE` |
|       - |  964 | ` * $x = true; 	boolean 	FALSE 	FALSE 	TRUE 	TRUE` |
|       - |  965 | ` * $x = 1; 	    integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|       - |  966 | ` * $x = 42; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|       - |  967 | ` * $x = 0; 	    integer 	TRUE 	FALSE 	TRUE 	FALSE` |
|       - |  968 | ` * $x = -1; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|       - |  969 | ` * $x = "1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|       - |  970 | ` * $x = "0"; 	string 	TRUE 	FALSE 	TRUE 	FALSE` |
|       - |  971 | ` * $x = "-1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|       - |  972 | ` * $x = "php"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|       - |  973 | ` * $x = "true"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|       - |  974 | ` * $x = "false"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|       - |  975 | ` *      Loose comparisons with ==` |
|       - |  976 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|       - |  977 | ` * TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE` |
|       - |  978 | ` * FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|       - |  979 | ` * 1 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  980 | ` * 0 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE` |
|       - |  981 | ` * -1 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  982 | ` * "1" 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  983 | ` * "0" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  984 | ` * "-1" 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  985 | ` * NULL 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|       - |  986 | ` * array() 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	FALSE` |
|       - |  987 | ` * "php" 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|       - |  988 | ` * "" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE` |
|       - |  989 | ` *    Strict comparisons with ===` |
|       - |  990 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|       - |  991 | ` * TRUE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  992 | ` * FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  993 | ` * 1 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  994 | ` * 0 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  995 | ` * -1 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  996 | ` * "1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  997 | ` * "0" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  998 | ` * "-1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  999 | ` * NULL 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE` |
|       - | 1000 | ` * array() 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE` |
|       - | 1001 | ` * "php" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|       - | 1002 | ` * "" 	    FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE` |
|       - | 1003 | ` */` |
|  790961 | 1004 | `PH7_PRIVATE sxi32 PH7_MemObjCmp(ph7_value *pObj1,ph7_value *pObj2,int bStrict,int iNest)` |
|       2 | 1005 |  |
|       - | 1006 | `	sxi32 iComb;` |
|       - | 1007 | `	sxi32 rc;` |
|  790963 | 1008 | `	if( bStrict ){` |
|       - | 1009 | `		sxi32 iF1,iF2;` |
|       - | 1010 | `		/* Strict comparisons with === */` |
|  384240 | 1011 | `		iF1 = pObj1->iFlags&~MEMOBJ_AUX;` |
|  384240 | 1012 | `		iF2 = pObj2->iFlags&~MEMOBJ_AUX;` |
|  384240 | 1013 | `		if( iF1 != iF2 ){` |
|       - | 1014 | `			/* Not of the same type */` |
|  103382 | 1015 | `			return 1;` |
|       - | 1016 | `		}` |
|  140429 | 1017 | `	}` |
|       - | 1018 | `	/* Combine flag together */` |
|  687583 | 1019 | `	iComb = pObj1->iFlags\|pObj2->iFlags;` |
|  687583 | 1020 | `	if( iComb & (MEMOBJ_NULL\|MEMOBJ_RES\|MEMOBJ_BOOL) ){` |
|       - | 1021 | `		/* Convert to boolean: Keep in mind FALSE < TRUE */` |
|   12882 | 1022 | `		if( (pObj1->iFlags & MEMOBJ_BOOL) == 0 ){` |
|    4580 | 1023 | `			PH7_MemObjToBool(pObj1);` |
|    2289 | 1024 | `		}` |
|   12882 | 1025 | `		if( (pObj2->iFlags & MEMOBJ_BOOL) == 0 ){` |
|    3768 | 1026 | `			PH7_MemObjToBool(pObj2);` |
|    1883 | 1027 | `		}` |
|   12882 | 1028 | `		return (sxi32)((pObj1->x.iVal != 0) - (pObj2->x.iVal != 0));` |
|  674703 | 1029 | `	}else if ( iComb & MEMOBJ_HASHMAP ){` |
|       - | 1030 | `		/* Hashmap aka 'array' comparison */` |
|       9 | 1031 | `		if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       - | 1032 | `			/* Array is always greater */` |
|     ! 0 | 1033 | `			return -1;` |
|       - | 1034 | `		}` |
|       9 | 1035 | `		if( (pObj2->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       - | 1036 | `			/* Array is always greater */` |
|     ! 0 | 1037 | `			return 1;` |
|       - | 1038 | `		}` |
|       - | 1039 | `		/* Perform the comparison */` |
|       9 | 1040 | `		rc = PH7_HashmapCmp((ph7_hashmap *)pObj1->x.pOther,(ph7_hashmap *)pObj2->x.pOther,bStrict);` |
|       9 | 1041 | `		return rc;` |
|  674695 | 1042 | `	}else if(iComb & MEMOBJ_OBJ ){` |
|       - | 1043 | `		/* Object comparison */` |
|     162 | 1044 | `		if( (pObj1->iFlags & MEMOBJ_OBJ) == 0 ){` |
|       - | 1045 | `			/* Object is always greater */` |
|     ! 0 | 1046 | `			return -1;` |
|       - | 1047 | `		}` |
|     162 | 1048 | `		if( (pObj2->iFlags & MEMOBJ_OBJ) == 0 ){` |
|       - | 1049 | `			/* Object is always greater */` |
|     ! 0 | 1050 | `			return 1;` |
|       - | 1051 | `		}` |
|       - | 1052 | `		/* Perform the comparison */` |
|     162 | 1053 | `		rc = PH7_ClassInstanceCmp((ph7_class_instance *)pObj1->x.pOther,(ph7_class_instance *)pObj2->x.pOther,bStrict,iNest);` |
|     162 | 1054 | `		return rc;` |
|  674535 | 1055 | `	}else if ( iComb & MEMOBJ_STRING ){` |
|       - | 1056 | `		SyString s1,s2;` |
|  402834 | 1057 | `		if( !bStrict ){` |
|       - | 1058 | `			/*` |
|       - | 1059 | `			 * According to the PHP language reference manual:` |
|       - | 1060 | `			 *` |
|       - | 1061 | `			 *  If you compare a number with a string or the comparison involves numerical` |
|       - | 1062 | `			 *  strings, then each string is converted to a number and the comparison` |
|       - | 1063 | `			 *  performed numerically.` |
|       - | 1064 | `			 */` |
|  134566 | 1065 | `			if( PH7_MemObjIsNumeric(pObj1) ){` |
|       - | 1066 | `				/* Perform a numeric comparison */` |
|      11 | 1067 | `				goto Numeric;` |
|       - | 1068 | `			}` |
|  134556 | 1069 | `			if( PH7_MemObjIsNumeric(pObj2) ){` |
|       - | 1070 | `				/* Perform a numeric comparison */` |
|     ! 0 | 1071 | `				goto Numeric;` |
|       - | 1072 | `			}` |
|   67326 | 1073 | `		}` |
|       - | 1074 | `		/* Perform a strict string comparison.*/` |
|  402824 | 1075 | `		if( (pObj1->iFlags&MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1076 | `			PH7_MemObjToString(pObj1);` |
|     ! 0 | 1077 | `		}` |
|  402824 | 1078 | `		if( (pObj2->iFlags&MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1079 | `			PH7_MemObjToString(pObj2);` |
|     ! 0 | 1080 | `		}` |
|  402824 | 1081 | `		SyStringInitFromBuf(&s1,SyBlobData(&pObj1->sBlob),SyBlobLength(&pObj1->sBlob));` |
|  402824 | 1082 | `		SyStringInitFromBuf(&s2,SyBlobData(&pObj2->sBlob),SyBlobLength(&pObj2->sBlob));` |
|       - | 1083 | `		/*` |
|       - | 1084 | `		 * Strings are compared using memcmp(). If one value is an exact prefix of the` |
|       - | 1085 | `		 * other, then the shorter value is less than the longer value.` |
|       - | 1086 | `		 */` |
|  402824 | 1087 | `		rc = SyMemcmp((const void *)s1.zString,(const void *)s2.zString,SXMIN(s1.nByte,s2.nByte));` |
|  402824 | 1088 | `		if( rc == 0 ){` |
|  146256 | 1089 | `			if( s1.nByte != s2.nByte ){` |
|    1046 | 1090 | `				rc = s1.nByte < s2.nByte ? -1 : 1;` |
|     524 | 1091 | `			}` |
|   73129 | 1092 | `		}` |
|  402824 | 1093 | `		return rc;` |
|  271703 | 1094 | `	}else if( iComb & (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|  135827 | 1095 | `Numeric:` |
|       - | 1096 | `		/* Perform a numeric comparison if one of the operand is numeric(integer or real) */` |
|  271713 | 1097 | `		if( (pObj1->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|       7 | 1098 | `			PH7_MemObjToNumeric(pObj1);` |
|       3 | 1099 | `		}` |
|  271713 | 1100 | `		if( (pObj2->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|       9 | 1101 | `			PH7_MemObjToNumeric(pObj2);` |
|       4 | 1102 | `		}` |
|  271713 | 1103 | `		if( (pObj1->iFlags & pObj2->iFlags & MEMOBJ_INT) == 0) {` |
|       - | 1104 | `			/*` |
|       - | 1105 | `			 * Symisc eXtension to the PHP language:` |
|       - | 1106 | `			 *  Floating point comparison is introduced and works as expected.` |
|       - | 1107 | `			 */` |
|       - | 1108 | `			ph7_real r1,r2;` |
|       - | 1109 | `			/* Compare as reals */` |
|     149 | 1110 | `			if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|       9 | 1111 | `				PH7_MemObjToReal(pObj1);` |
|       4 | 1112 | `			}` |
|     149 | 1113 | `			r1 = pObj1->rVal;` |
|     149 | 1114 | `			if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|      23 | 1115 | `				PH7_MemObjToReal(pObj2);` |
|      11 | 1116 | `			}` |
|     149 | 1117 | `			r2 = pObj2->rVal;` |
|     149 | 1118 | `			if( PH7_IS_NAN(r1) \|\| PH7_IS_NAN(r2) ){` |
|       - | 1119 | `				/*` |
|       - | 1120 | `				 * Keep a strict three-way comparator contract even for NaN values.` |
|       - | 1121 | `				 * For ordering purposes, NaN compares equal to NaN and greater than` |
|       - | 1122 | `				 * any non-NaN numeric value.` |
|       - | 1123 | `				 */` |
|      31 | 1124 | `				if( PH7_IS_NAN(r1) ){` |
|      23 | 1125 | `					return PH7_IS_NAN(r2) ? 0 : 1;` |
|       - | 1126 | `				}` |
|       9 | 1127 | `				return -1;` |
|       - | 1128 | `			}` |
|     119 | 1129 | `			if( r1 > r2 ){` |
|      11 | 1130 | `				return 1;` |
|     109 | 1131 | `			}else if( r1 < r2 ){` |
|     101 | 1132 | `				return -1;` |
|       - | 1133 | `			}` |
|       9 | 1134 | `			return 0;` |
|     ! 0 | 1135 | `		}else{` |
|       - | 1136 | `			/* Integer comparison */` |
|  271565 | 1137 | `			if( pObj1->x.iVal > pObj2->x.iVal ){` |
|    1794 | 1138 | `				return 1;` |
|  269773 | 1139 | `			}else if( pObj1->x.iVal < pObj2->x.iVal ){` |
|  266786 | 1140 | `				return -1;` |
|       - | 1141 | `			}` |
|    2989 | 1142 | `			return 0;` |
|       - | 1143 | `		}` |
|       - | 1144 | `	}` |
|       - | 1145 | `	/* NOT REACHED */` |
|     ! 0 | 1146 | `	return 0;` |
|  395555 | 1147 |  |
|       - | 1148 | `/*` |
|       - | 1149 | ` * Perform an addition operation of two ph7_values.` |
|       - | 1150 | ` * The reason this function is implemented here rather than 'vm.c'` |
|       - | 1151 | ` * is that the '+' operator is overloaded.` |
|       - | 1152 | ` * That is,the '+' operator is used for arithmetic operation and also` |
|       - | 1153 | ` * used for operation on arrays [i.e: union]. When used with an array` |
|       - | 1154 | ` * The + operator returns the right-hand array appended to the left-hand array.` |
|       - | 1155 | ` * For keys that exist in both arrays, the elements from the left-hand array` |
|       - | 1156 | ` * will be used, and the matching elements from the right-hand array will` |
|       - | 1157 | ` * be ignored.` |
|       - | 1158 | ` * This function take care of handling all the scenarios.` |
|       - | 1159 | ` */` |
|    1802 | 1160 | `PH7_PRIVATE sxi32 PH7_MemObjAdd(ph7_value *pObj1,ph7_value *pObj2,int bAddStore)` |
|       2 | 1161 |  |
|    1804 | 1162 | `	if( ((pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP) == 0 ){` |
|       - | 1163 | `			/* Arithemtic operation */` |
|    1800 | 1164 | `			PH7_MemObjToNumeric(pObj1);` |
|    1800 | 1165 | `			PH7_MemObjToNumeric(pObj2);` |
|    1800 | 1166 | `			if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_REAL ){` |
|       - | 1167 | `				/* Floating point arithmetic */` |
|       - | 1168 | `				ph7_real a,b;` |
|      25 | 1169 | `				if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|       7 | 1170 | `					PH7_MemObjToReal(pObj1);` |
|       3 | 1171 | `				}` |
|      25 | 1172 | `				if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|       3 | 1173 | `					PH7_MemObjToReal(pObj2);` |
|       1 | 1174 | `				}` |
|      25 | 1175 | `				a = pObj1->rVal;` |
|      25 | 1176 | `				b = pObj2->rVal;` |
|      25 | 1177 | `				pObj1->rVal = a+b;` |
|      25 | 1178 | `				MemObjSetType(pObj1,MEMOBJ_REAL);` |
|       - | 1179 | `				/* Try to get an integer representation also */` |
|      25 | 1180 | `				MemObjTryIntger(&(*pObj1));` |
|      13 | 1181 | `			}else{` |
|       - | 1182 | `				/* Integer arithmetic */` |
|       - | 1183 | `				sxi64 a,b;` |
|    1776 | 1184 | `				a = pObj1->x.iVal;` |
|    1776 | 1185 | `				b = pObj2->x.iVal;` |
|    1776 | 1186 | `				pObj1->x.iVal = a+b;` |
|    1776 | 1187 | `				MemObjSetType(pObj1,MEMOBJ_INT);` |
|       - | 1188 | `			}` |
|     901 | 1189 | `	}else{` |
|       6 | 1190 | `		if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP ){` |
|       - | 1191 | `			ph7_hashmap *pMap;` |
|       - | 1192 | `			sxi32 rc;` |
|       6 | 1193 | `			if( bAddStore ){` |
|       - | 1194 | `				/* Do not duplicate the hashmap,use the left one since its an add&store operation.` |
|       - | 1195 | `				 */` |
|     ! 0 | 1196 | `				if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       - | 1197 | `					/* Force a hashmap cast */` |
|     ! 0 | 1198 | `					rc = PH7_MemObjToHashmap(pObj1);` |
|     ! 0 | 1199 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 1200 | `						PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|     ! 0 | 1201 | `						return rc;` |
|       - | 1202 | `					}` |
|     ! 0 | 1203 | `				}` |
|       - | 1204 | `				/* Point to the structure that describe the hashmap */` |
|     ! 0 | 1205 | `				pMap = (ph7_hashmap *)pObj1->x.pOther;` |
|     ! 0 | 1206 | `			}else{` |
|       - | 1207 | `				/* Create a new hashmap */` |
|       6 | 1208 | `				pMap = PH7_NewHashmap(pObj1->pVm,0,0);` |
|       6 | 1209 | `				if( pMap == 0){` |
|     ! 0 | 1210 | `					PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|     ! 0 | 1211 | `					return SXERR_MEM;` |
|       - | 1212 | `				}` |
|       - | 1213 | `			}` |
|       6 | 1214 | `			if( !bAddStore ){` |
|       6 | 1215 | `				if(pObj1->iFlags & MEMOBJ_HASHMAP ){` |
|       - | 1216 | `					/* Perform a hashmap duplication */` |
|       6 | 1217 | `					PH7_HashmapDup((ph7_hashmap *)pObj1->x.pOther,pMap);` |
|       4 | 1218 | `				}else{` |
|     ! 0 | 1219 | `					if((pObj1->iFlags & MEMOBJ_NULL) == 0 ){` |
|       - | 1220 | `						/* Simple insertion */` |
|     ! 0 | 1221 | `						PH7_HashmapInsert(pMap,0,pObj1);` |
|     ! 0 | 1222 | `					}` |
|       - | 1223 | `				}` |
|       2 | 1224 | `			}` |
|       - | 1225 | `			/* Perform the union */` |
|       6 | 1226 | `			if(pObj2->iFlags & MEMOBJ_HASHMAP ){` |
|       6 | 1227 | `				PH7_HashmapUnion(pMap,(ph7_hashmap *)pObj2->x.pOther);` |
|       4 | 1228 | `			}else{` |
|     ! 0 | 1229 | `				if((pObj2->iFlags & MEMOBJ_NULL) == 0 ){` |
|       - | 1230 | `					/* Simple insertion */` |
|     ! 0 | 1231 | `					PH7_HashmapInsert(pMap,0,pObj2);` |
|     ! 0 | 1232 | `				}` |
|       - | 1233 | `			}` |
|       - | 1234 | `			/* Reflect the change */` |
|       6 | 1235 | `			if( pObj1->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 1236 | `				SyBlobRelease(&pObj1->sBlob);` |
|     ! 0 | 1237 | `			}` |
|       6 | 1238 | `			pObj1->x.pOther = pMap;` |
|       6 | 1239 | `			MemObjSetType(pObj1,MEMOBJ_HASHMAP);` |
|       2 | 1240 | `		}` |
|       - | 1241 | `	}` |
|    1804 | 1242 | `	return SXRET_OK;` |
|     903 | 1243 |  |
|       - | 1244 | `/*` |
|       - | 1245 | ` * Return a printable representation of the type of a given` |
|       - | 1246 | ` * ph7_value.` |
|       - | 1247 | ` */` |
|     414 | 1248 | `PH7_PRIVATE const char * PH7_MemObjTypeDump(ph7_value *pVal)` |
|       2 | 1249 |  |
|     416 | 1250 | `	const char *zType = "";` |
|     416 | 1251 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|       9 | 1252 | `		zType = "null";` |
|     412 | 1253 | `	}else if( pVal->iFlags & MEMOBJ_INT ){` |
|      96 | 1254 | `		zType = "int";` |
|     361 | 1255 | `	}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|       7 | 1256 | `		zType = "double";` |
|     311 | 1257 | `	}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|      56 | 1258 | `		zType = "string";` |
|     281 | 1259 | `	}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|     110 | 1260 | `		zType = "bool";` |
|     200 | 1261 | `	}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|      16 | 1262 | `		zType = "array";` |
|     138 | 1263 | `	}else if( pVal->iFlags & MEMOBJ_OBJ ){` |
|     131 | 1264 | `		zType = "object";` |
|      65 | 1265 | `	}else if( pVal->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 1266 | `		zType = "resource";` |
|     ! 0 | 1267 | `	}` |
|     416 | 1268 | `	return zType;` |
|       2 | 1269 |  |
|       - | 1270 | `/*` |
|       - | 1271 | ` * Dump a ph7_value [i.e: get a printable representation of it's type and contents.].` |
|       - | 1272 | ` * Store the dump in the given blob.` |
|       - | 1273 | ` */` |
|     474 | 1274 | `PH7_PRIVATE sxi32 PH7_MemObjDump(` |
|       - | 1275 | `	SyBlob *pOut,      /* Store the dump here */` |
|       - | 1276 | `	ph7_value *pObj,   /* Dump this */` |
|       - | 1277 | `	int ShowType,      /* TRUE to output value type */` |
|       - | 1278 | `	int nTab,          /* # of Whitespace to insert */` |
|       - | 1279 | `	int nDepth,        /* Nesting level */` |
|       - | 1280 | `	int isRef          /* TRUE if referenced object */` |
|       - | 1281 | `	)` |
|       2 | 1282 |  |
|     476 | 1283 | `	sxi32 rc = SXRET_OK;` |
|       - | 1284 | `	const char *zType;` |
|       - | 1285 | `	int i;` |
|    4656 | 1286 | `	for( i = 0 ; i < nTab ; i++ ){` |
|    4182 | 1287 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|    2092 | 1288 | `	}` |
|     476 | 1289 | `	if( ShowType ){` |
|     386 | 1290 | `		if( isRef ){` |
|     ! 0 | 1291 | `			SyBlobAppend(&(*pOut),"&",sizeof(char));` |
|     ! 0 | 1292 | `		}` |
|       - | 1293 | `		/* Get value type first */` |
|     386 | 1294 | `		zType = PH7_MemObjTypeDump(pObj);` |
|     386 | 1295 | `		SyBlobAppend(&(*pOut),zType,SyStrlen(zType));` |
|     192 | 1296 | `	}` |
|     476 | 1297 | `	if((pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|     470 | 1298 | `		if ( ShowType ){` |
|     380 | 1299 | `			SyBlobAppend(&(*pOut),"(",sizeof(char));` |
|     189 | 1300 | `		}` |
|     470 | 1301 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       - | 1302 | `			/* Dump hashmap entries */` |
|      28 | 1303 | `			rc = PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,ShowType,nTab+1,nDepth+1);` |
|     457 | 1304 | `		}else if(pObj->iFlags & MEMOBJ_OBJ ){` |
|       - | 1305 | `			/* Dump class instance attributes */` |
|     133 | 1306 | `			rc = PH7_ClassInstanceDump(&(*pOut),(ph7_class_instance *)pObj->x.pOther,ShowType,nTab+1,nDepth+1);` |
|      67 | 1307 | `		}else{` |
|     312 | 1308 | `			SyBlob *pContents = &pObj->sBlob;` |
|       - | 1309 | `			/* Get a printable representation of the contents */` |
|     312 | 1310 | `			if((pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|     222 | 1311 | `				MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|     112 | 1312 | `			}else{` |
|       - | 1313 | `				/* Append length first */` |
|      92 | 1314 | `				if( ShowType ){` |
|      36 | 1315 | `					SyBlobFormat(&(*pOut),"%u '",SyBlobLength(&pObj->sBlob));` |
|      17 | 1316 | `				}` |
|      92 | 1317 | `				if( SyBlobLength(pContents) > 0 ){` |
|      82 | 1318 | `					SyBlobAppend(&(*pOut),SyBlobData(pContents),SyBlobLength(pContents));` |
|      40 | 1319 | `				}` |
|      92 | 1320 | `				if( ShowType ){` |
|      36 | 1321 | `					SyBlobAppend(&(*pOut),"'",sizeof(char));` |
|      17 | 1322 | `				}` |
|       - | 1323 | `			}` |
|       - | 1324 | `		}` |
|     470 | 1325 | `		if( ShowType ){` |
|     380 | 1326 | `			if( (pObj->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|     238 | 1327 | `				SyBlobAppend(&(*pOut),")",sizeof(char));` |
|     118 | 1328 | `			}` |
|     189 | 1329 | `		}` |
|     234 | 1330 | `	}` |
|       - | 1331 | `#ifdef __WINNT__` |
|       2 | 1332 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 1333 | `#else` |
|     474 | 1334 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 1335 | `#endif` |
|     476 | 1336 | `	return rc;` |
|       2 | 1337 |  |
|       - | 1338 |  |
