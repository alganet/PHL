# src/ph7/memobj.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 626/729 lines (85.87%)

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
|      54 |   11 | `PH7_PRIVATE const char *ph7_type_name(ph7_value *pVal)` |
|       1 |   12 |  |
|      55 |   13 | `	if( ph7_value_is_null(pVal) ) return "null";` |
|      49 |   14 | `	if( ph7_value_is_bool(pVal) ) return "bool";` |
|      47 |   15 | `	if( ph7_value_is_int(pVal) ) return "int";` |
|      41 |   16 | `	if( ph7_value_is_float(pVal) ) return "float";` |
|      41 |   17 | `	if( ph7_value_is_string(pVal) ) return "string";` |
|      15 |   18 | `	if( ph7_value_is_array(pVal) ) return "array";` |
|       3 |   19 | `	if( ph7_value_is_object(pVal) ) return "object";` |
|     ! 0 |   20 | `	if( ph7_value_is_resource(pVal) ) return "resource";` |
|     ! 0 |   21 | `	return "unknown";` |
|      28 |   22 |  |
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
|    1190 |   40 | `static sxi64 MemObjRealToInt(ph7_value *pObj)` |
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
|    1192 |   57 | `  ph7_real r = pObj->rVal;` |
|    1192 |   58 | `  if( r<(ph7_real)minInt ){` |
|     ! 0 |   59 | `    return minInt;` |
|    1192 |   60 | `  }else if( r>(ph7_real)maxInt ){` |
|       - |   61 | `    /* minInt is correct here - not maxInt.  It turns out that assigning` |
|       - |   62 | `    ** a very large positive number to an integer results in a very large` |
|       - |   63 | `    ** negative integer.  This makes no sense, but it is what x86 hardware` |
|       - |   64 | `    ** does so for compatibility we will do the same in software. */` |
|       7 |   65 | `    return minInt;` |
|     ! 0 |   66 | `  }else{` |
|    1186 |   67 | `    return (sxi64)r;` |
|       - |   68 | `  }` |
|       - |   69 | `#endif` |
|     597 |   70 |  |
|       - |   71 | `/*` |
|       - |   72 | ` * Convert a raw token value typically a stream of digit [i.e: hex,octal,binary or decimal]` |
|       - |   73 | ` * to a 64-bit integer.` |
|       - |   74 | ` */` |
|   50508 |   75 | `PH7_PRIVATE sxi64 PH7_TokenValueToInt64(SyString *pVal)` |
|       2 |   76 |  |
|   50510 |   77 | `	sxi64 iVal = 0;` |
|   50510 |   78 | `	if( pVal->nByte <= 0 ){` |
|       7 |   79 | `		return 0;` |
|       - |   80 | `	}` |
|   50504 |   81 | `	if( pVal->zString[0] == '0' ){` |
|       - |   82 | `		sxi32 c;` |
|   19988 |   83 | `		if( pVal->nByte == sizeof(char) ){` |
|   19926 |   84 | `			return 0;` |
|       - |   85 | `		}` |
|      63 |   86 | `		c = pVal->zString[1];` |
|      63 |   87 | `		if( c  == 'x' \|\| c == 'X' ){` |
|       - |   88 | `			/* Hex digit stream */` |
|      13 |   89 | `			SyHexStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|      57 |   90 | `		}else if( c == 'b' \|\| c == 'B' ){` |
|       - |   91 | `			/* Binary digit stream */` |
|      31 |   92 | `			SyBinaryStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|      16 |   93 | `		}else{` |
|       - |   94 | `			/* Octal digit stream */` |
|      21 |   95 | `			SyOctalStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|       - |   96 | `		}` |
|      32 |   97 | `	}else{` |
|       - |   98 | `		/* Decimal digit stream */` |
|   30518 |   99 | `		SyStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|       - |  100 | `	}` |
|   30580 |  101 | `	return iVal;` |
|   25256 |  102 |  |
|       - |  103 | `/*` |
|       - |  104 | ` * Return some kind of 64-bit integer value which is the best we can` |
|       - |  105 | ` * do at representing the value that pObj describes as a string` |
|       - |  106 | ` * representation.` |
|       - |  107 | ` */` |
|     290 |  108 | `static sxi64 MemObjStringToInt(ph7_value *pObj)` |
|       1 |  109 |  |
|       - |  110 | `	SyString sVal;` |
|     291 |  111 | `	SyStringInitFromBuf(&sVal,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|     291 |  112 | `	return PH7_TokenValueToInt64(&sVal);` |
|       1 |  113 |  |
|       - |  114 | `/*` |
|       - |  115 | ` * Call a magic class method [i.e: __toString(),__toInt(),...]` |
|       - |  116 | ` * Return SXRET_OK if the magic method is available and have been` |
|       - |  117 | ` * successfully called. Any other return value indicates failure.` |
|       - |  118 | ` */` |
|      86 |  119 | `static sxi32 MemObjCallClassCastMethod(` |
|       - |  120 | `	ph7_vm *pVm,               /* VM that trigger the invocation */` |
|       - |  121 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object] */` |
|       - |  122 | `	const char *zMethod,       /* Magic method name [i.e: __toString] */` |
|       - |  123 | `	sxu32 nLen,                /* Method name length */` |
|       - |  124 | `	ph7_value *pResult         /* OUT: Store the return value of the magic method here */` |
|       - |  125 | `	)` |
|       2 |  126 |  |
|       - |  127 | `	ph7_class_method *pMethod;` |
|       - |  128 | `	/* Check if the method is available */` |
|      88 |  129 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,zMethod,nLen);` |
|      88 |  130 | `	if( pMethod == 0 ){` |
|       - |  131 | `		/* No such method */` |
|       3 |  132 | `		return SXERR_NOTFOUND;` |
|       - |  133 | `	}` |
|       - |  134 | `	/* Invoke the desired method */` |
|      86 |  135 | `	PH7_VmCallClassMethod(&(*pVm),&(*pThis),pMethod,&(*pResult),0,0);` |
|       - |  136 | `	/* Method successfully called,pResult should hold the return value */` |
|      86 |  137 | `	return SXRET_OK;` |
|      45 |  138 |  |
|       - |  139 | `/*` |
|       - |  140 | ` * Return some kind of integer value which is the best we can` |
|       - |  141 | ` * do at representing the value that pObj describes as an integer.` |
|       - |  142 | ` * If pObj is an integer, then the value is exact. If pObj is` |
|       - |  143 | ` * a floating-point then  the value returned is the integer part.` |
|       - |  144 | ` * If pObj is a string, then we make an attempt to convert it into` |
|       - |  145 | ` * a integer and return that.` |
|       - |  146 | ` * If pObj represents a NULL value, return 0.` |
|       - |  147 | ` */` |
|     316 |  148 | `static sxi64 MemObjIntValue(ph7_value *pObj)` |
|       2 |  149 |  |
|       - |  150 | `	sxi32 iFlags;` |
|     318 |  151 | `	iFlags = pObj->iFlags;` |
|     318 |  152 | `	if (iFlags & MEMOBJ_REAL ){` |
|      19 |  153 | `		return MemObjRealToInt(&(*pObj));` |
|     300 |  154 | `	}else if( iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      19 |  155 | `		return pObj->x.iVal;` |
|     282 |  156 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|     261 |  157 | `		return MemObjStringToInt(&(*pObj));` |
|      22 |  158 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|      10 |  159 | `		return 0;` |
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
|     160 |  186 |  |
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
|     620 |  197 | `static ph7_real MemObjRealValue(ph7_value *pObj)` |
|       2 |  198 |  |
|       - |  199 | `	sxi32 iFlags;` |
|     622 |  200 | `	iFlags = pObj->iFlags;` |
|     622 |  201 | `	if( iFlags & MEMOBJ_REAL ){` |
|     ! 0 |  202 | `		return pObj->rVal;` |
|     622 |  203 | `	}else if (iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     272 |  204 | `		return (ph7_real)pObj->x.iVal;` |
|     352 |  205 | `	}else if (iFlags & MEMOBJ_STRING){` |
|       - |  206 | `		SyString sString;` |
|       - |  207 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|       - |  208 | `		ph7_real rVal = 0;` |
|       - |  209 | `#else` |
|     346 |  210 | `		ph7_real rVal = 0.0;` |
|       - |  211 | `#endif` |
|     346 |  212 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|     346 |  213 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       - |  214 | `			/* Convert as much as we can */` |
|       - |  215 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|       - |  216 | `			rVal = MemObjStringToInt(&(*pObj));` |
|       - |  217 | `#else` |
|     346 |  218 | `			SyStrToReal(sString.zString,sString.nByte,(void *)&rVal,0);` |
|       - |  219 | `#endif` |
|     172 |  220 | `		}` |
|     346 |  221 | `		return rVal;` |
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
|     312 |  254 |  |
|       - |  255 | `/*` |
|       - |  256 | ` * Return the string representation of a given ph7_value.` |
|       - |  257 | ` * This function never fail and always return SXRET_OK.` |
|       - |  258 | ` */` |
|   51456 |  259 | `static sxi32 MemObjStringValue(SyBlob *pOut,ph7_value *pObj,sxu8 bStrictBool)` |
|       2 |  260 |  |
|   51458 |  261 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|       - |  262 | `		/* Handle special floating-point values first */` |
|     106 |  263 | `		if( PH7_IS_NAN(pObj->rVal) ){` |
|     ! 0 |  264 | `			SyBlobAppend(&(*pOut),"NAN",3);` |
|     106 |  265 | `		}else if( PH7_IS_INF(pObj->rVal) ){` |
|     ! 0 |  266 | `			if( pObj->rVal < 0.0 ){` |
|     ! 0 |  267 | `				SyBlobAppend(&(*pOut),"-INF",4);` |
|     ! 0 |  268 | `			}else{` |
|     ! 0 |  269 | `				SyBlobAppend(&(*pOut),"INF",3);` |
|       - |  270 | `			}` |
|     ! 0 |  271 | `		}else{` |
|     106 |  272 | `			SyBlobFormat(&(*pOut),"%.15g",pObj->rVal);` |
|       2 |  273 | `		}` |
|   51406 |  274 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|   51126 |  275 | `		SyBlobFormat(&(*pOut),"%qd",pObj->x.iVal);` |
|       - |  276 | `		/* %qd (BSD quad) is equivalent to %lld in the libc printf */` |
|   25792 |  277 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|     128 |  278 | `		if( pObj->x.iVal ){` |
|      66 |  279 | `			SyBlobAppend(&(*pOut),"TRUE",sizeof("TRUE")-1);` |
|      34 |  280 | `		}else{` |
|      64 |  281 | `			if( !bStrictBool ){` |
|      56 |  282 | `				SyBlobAppend(&(*pOut),"FALSE",sizeof("FALSE")-1);` |
|      27 |  283 | `			}` |
|       2 |  284 | `		}` |
|     167 |  285 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       5 |  286 | `		SyBlobAppend(&(*pOut),"Array",sizeof("Array")-1);` |
|       5 |  287 | `		PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
|     102 |  288 | `	}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       - |  289 | `		ph7_value sResult;` |
|       - |  290 | `		sxi32 rc;` |
|       - |  291 | `		/* Invoke the __toString() method if available */` |
|      76 |  292 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|      76 |  293 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|       - |  294 | `			"__toString",sizeof("__toString")-1,&sResult);` |
|      76 |  295 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_STRING) && SyBlobLength(&sResult.sBlob) > 0){` |
|       - |  296 | `			/* Expand method return value */` |
|      70 |  297 | `			SyBlobDup(&sResult.sBlob,pOut);` |
|      36 |  298 | `		}else{` |
|       - |  299 | `			/* Expand "Object" as requested by the PHP language reference manual */` |
|       8 |  300 | `			SyBlobAppend(&(*pOut),"Object",sizeof("Object")-1);` |
|       - |  301 | `		}` |
|      76 |  302 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|      76 |  303 | `		PH7_MemObjRelease(&sResult);` |
|      63 |  304 | `	}else if(pObj->iFlags & MEMOBJ_RES ){` |
|       3 |  305 | `		SyBlobFormat(&(*pOut),"ResourceID_%#x",pObj->x.pOther);` |
|       1 |  306 | `	}` |
|   51458 |  307 | `	return SXRET_OK;` |
|       2 |  308 |  |
|       - |  309 | `/*` |
|       - |  310 | ` * Return some kind of boolean value which is the best we can do` |
|       - |  311 | ` * at representing the value that pObj describes as a boolean.` |
|       - |  312 | ` * When converting to boolean, the following values are considered FALSE:` |
|       - |  313 | ` * NULL` |
|       - |  314 | ` * the boolean FALSE itself.` |
|       - |  315 | ` * the integer 0 (zero).` |
|       - |  316 | ` * the real 0.0 (zero).` |
|       - |  317 | ` * the empty string,a stream of zero [i.e: "0","00","000",...] and the string` |
|       - |  318 | ` * "false".` |
|       - |  319 | ` * an array with zero elements.` |
|       - |  320 | ` */` |
|    7676 |  321 | `static sxi32 MemObjBooleanValue(ph7_value *pObj)` |
|       2 |  322 |  |
|       - |  323 | `	sxi32 iFlags;` |
|    7678 |  324 | `	iFlags = pObj->iFlags;` |
|    7678 |  325 | `	if (iFlags & MEMOBJ_REAL ){` |
|       - |  326 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|       - |  327 | `		return pObj->rVal ? 1 : 0;` |
|       - |  328 | `#else` |
|       5 |  329 | `		return pObj->rVal != 0.0 ? 1 : 0;` |
|       - |  330 | `#endif` |
|    7674 |  331 | `	}else if( iFlags & MEMOBJ_INT ){` |
|      23 |  332 | `		return pObj->x.iVal ? 1 : 0;` |
|    7652 |  333 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|       - |  334 | `		SyString sString;` |
|      31 |  335 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|      31 |  336 | `		if( sString.nByte == 0 ){` |
|       - |  337 | `			/* Empty string */` |
|       5 |  338 | `			return 0;` |
|      27 |  339 | `		}else if( (sString.nByte == sizeof("true") - 1 && SyStrnicmp(sString.zString,"true",sizeof("true")-1) == 0) \|\|` |
|      27 |  340 | `			(sString.nByte == sizeof("on") - 1 && SyStrnicmp(sString.zString,"on",sizeof("on")-1) == 0) \|\|` |
|      26 |  341 | `			(sString.nByte == sizeof("yes") - 1 && SyStrnicmp(sString.zString,"yes",sizeof("yes")-1) == 0) ){` |
|     ! 0 |  342 | `				return 1;` |
|      27 |  343 | `		}else if( sString.nByte == sizeof("false") - 1 && SyStrnicmp(sString.zString,"false",sizeof("false")-1) == 0 ){` |
|     ! 0 |  344 | `			return 0;` |
|     ! 0 |  345 | `		}else{` |
|       - |  346 | `			const char *zIn,*zEnd;` |
|      27 |  347 | `			zIn = sString.zString;` |
|      27 |  348 | `			zEnd = &zIn[sString.nByte];` |
|      27 |  349 | `			while( zIn < zEnd && zIn[0] == '0' ){` |
|     ! 0 |  350 | `				zIn++;` |
|     ! 0 |  351 | `			}` |
|      27 |  352 | `			return zIn >= zEnd ? 0 : 1;` |
|     ! 0 |  353 | `		}` |
|    7622 |  354 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|    6784 |  355 | `		return 0;` |
|     840 |  356 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|      15 |  357 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      15 |  358 | `		sxu32 n = pMap->nEntry;` |
|      15 |  359 | `		PH7_HashmapUnref(pMap);` |
|      15 |  360 | `		return n > 0 ? TRUE : FALSE;` |
|     826 |  361 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|       - |  362 | `		ph7_value sResult;` |
|       5 |  363 | `		sxi32 iVal = 1;` |
|       - |  364 | `		sxi32 rc;` |
|       - |  365 | `		/* Invoke the __toBool() method if available [note that this is a symisc extension]  */` |
|       5 |  366 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|       5 |  367 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|       - |  368 | `			"__toBool",sizeof("__toBool")-1,&sResult);` |
|       5 |  369 | `		if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL)) ){` |
|       - |  370 | `			/* Extract method return value */` |
|       5 |  371 | `			iVal = (sxi32)(sResult.x.iVal != 0); /* Stupid cc warning -W -Wall -O6 */` |
|       2 |  372 | `		}` |
|       5 |  373 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|       5 |  374 | `		PH7_MemObjRelease(&sResult);` |
|       5 |  375 | `		return iVal;` |
|     822 |  376 | `	}else if(iFlags & MEMOBJ_RES ){` |
|     822 |  377 | `		return pObj->x.pOther != 0;` |
|       - |  378 | `	}` |
|       - |  379 | `	/* NOT REACHED */` |
|     ! 0 |  380 | `	return 0;` |
|    3840 |  381 |  |
|       - |  382 | `/*` |
|       - |  383 | ` * If the ph7_value is of type real,try to make it an integer also.` |
|       - |  384 | ` */` |
|    1172 |  385 | `static sxi32 MemObjTryIntger(ph7_value *pObj)` |
|       2 |  386 |  |
|    1174 |  387 | `	pObj->x.iVal = MemObjRealToInt(&(*pObj));` |
|       - |  388 | `  /* Only mark the value as an integer if` |
|       - |  389 | `  **` |
|       - |  390 | `  **    (1) the round-trip conversion real->int->real is a no-op, and` |
|       - |  391 | `  **    (2) The integer is neither the largest nor the smallest` |
|       - |  392 | `  **        possible integer` |
|       - |  393 | `  **` |
|       - |  394 | `  ** The second and third terms in the following conditional enforces` |
|       - |  395 | `  ** the second condition under the assumption that addition overflow causes` |
|       - |  396 | `  ** values to wrap around.  On x86 hardware, the third term is always` |
|       - |  397 | `  ** true and could be omitted.  But we leave it in because other` |
|       - |  398 | `  ** architectures might behave differently.` |
|       - |  399 | `  */` |
|    1172 |  400 | `	if( pObj->rVal ==(ph7_real)pObj->x.iVal && pObj->x.iVal>SMALLEST_INT64` |
|     604 |  401 | `      && pObj->x.iVal<LARGEST_INT64 ){` |
|     604 |  402 | `		  pObj->iFlags \|= MEMOBJ_INT;` |
|     303 |  403 | `	}` |
|    1174 |  404 | `	return SXRET_OK;` |
|       2 |  405 |  |
|       - |  406 | `/*` |
|       - |  407 | ` * Convert a ph7_value to type integer.Invalidate any prior representations.` |
|       - |  408 | ` */` |
|  225434 |  409 | `PH7_PRIVATE sxi32 PH7_MemObjToInteger(ph7_value *pObj)` |
|       2 |  410 |  |
|  225436 |  411 | `	if( (pObj->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  412 | `		/* Preform the conversion */` |
|     318 |  413 | `		pObj->x.iVal = MemObjIntValue(&(*pObj));` |
|       - |  414 | `		/* Invalidate any prior representations */` |
|     318 |  415 | `		SyBlobRelease(&pObj->sBlob);` |
|     318 |  416 | `		MemObjSetType(pObj,MEMOBJ_INT);` |
|     158 |  417 | `	}` |
|  225436 |  418 | `	return SXRET_OK;` |
|       2 |  419 |  |
|       - |  420 | `/*` |
|       - |  421 | ` * Convert a ph7_value to type real (Try to get an integer representation also).` |
|       - |  422 | ` * Invalidate any prior representations` |
|       - |  423 | ` */` |
|     810 |  424 | `PH7_PRIVATE sxi32 PH7_MemObjToReal(ph7_value *pObj)` |
|       2 |  425 |  |
|     812 |  426 | `	if((pObj->iFlags & MEMOBJ_REAL) == 0 ){` |
|       - |  427 | `		/* Preform the conversion */` |
|     622 |  428 | `		pObj->rVal = MemObjRealValue(&(*pObj));` |
|       - |  429 | `		/* Invalidate any prior representations */` |
|     622 |  430 | `		SyBlobRelease(&pObj->sBlob);` |
|     622 |  431 | `		MemObjSetType(pObj,MEMOBJ_REAL);` |
|       - |  432 | `		/* Try to get an integer representation */` |
|     622 |  433 | `		MemObjTryIntger(&(*pObj));` |
|     310 |  434 | `	}` |
|     812 |  435 | `	return SXRET_OK;` |
|       2 |  436 |  |
|       - |  437 | `/*` |
|       - |  438 | ` * Convert a ph7_value to type boolean.Invalidate any prior representations.` |
|       - |  439 | ` */` |
|    7800 |  440 | `PH7_PRIVATE sxi32 PH7_MemObjToBool(ph7_value *pObj)` |
|       2 |  441 |  |
|    7802 |  442 | `	if( (pObj->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       - |  443 | `		/* Preform the conversion */` |
|    7678 |  444 | `		pObj->x.iVal = MemObjBooleanValue(&(*pObj));` |
|       - |  445 | `		/* Invalidate any prior representations */` |
|    7678 |  446 | `		SyBlobRelease(&pObj->sBlob);` |
|    7678 |  447 | `		MemObjSetType(pObj,MEMOBJ_BOOL);` |
|    3838 |  448 | `	}` |
|    7802 |  449 | `	return SXRET_OK;` |
|       2 |  450 |  |
|       - |  451 | `/*` |
|       - |  452 | ` * Convert a ph7_value to type string.Prior representations are NOT invalidated.` |
|       - |  453 | ` */` |
|  485910 |  454 | `PH7_PRIVATE sxi32 PH7_MemObjToString(ph7_value *pObj)` |
|       2 |  455 |  |
|  485912 |  456 | `	sxi32 rc = SXRET_OK;` |
|  485912 |  457 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  458 | `		/* Perform the conversion */` |
|   51220 |  459 | `		SyBlobReset(&pObj->sBlob); /* Reset the internal buffer */` |
|   51220 |  460 | `		rc = MemObjStringValue(&pObj->sBlob,&(*pObj),TRUE);` |
|   51220 |  461 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|   25609 |  462 | `	}` |
|  485912 |  463 | `	return rc;` |
|       2 |  464 |  |
|       - |  465 | `/*` |
|       - |  466 | ` * Nullify a ph7_value.In other words invalidate any prior` |
|       - |  467 | ` * representation.` |
|       - |  468 | ` */` |
|     ! 0 |  469 | `PH7_PRIVATE sxi32 PH7_MemObjToNull(ph7_value *pObj)` |
|     ! 0 |  470 |  |
|     ! 0 |  471 | `	return PH7_MemObjRelease(pObj);` |
|     ! 0 |  472 |  |
|       - |  473 | `/*` |
|       - |  474 | ` * Convert a ph7_value to type array.Invalidate any prior representations.` |
|       - |  475 | `  * According to the PHP language reference manual.` |
|       - |  476 | `  *   For any of the types: integer, float, string, boolean converting a value` |
|       - |  477 | `  *   to an array results in an array with a single element with index zero` |
|       - |  478 | `  *   and the value of the scalar which was converted.` |
|       - |  479 | `  */` |
|      20 |  480 | `PH7_PRIVATE sxi32 PH7_MemObjToHashmap(ph7_value *pObj)` |
|       1 |  481 |  |
|      21 |  482 | `	if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       - |  483 | `		ph7_hashmap *pMap;` |
|       - |  484 | `		/* Allocate a new hashmap instance */` |
|      21 |  485 | `		pMap = PH7_NewHashmap(pObj->pVm,0,0);` |
|      21 |  486 | `		if( pMap == 0 ){` |
|     ! 0 |  487 | `			return SXERR_MEM;` |
|       - |  488 | `		}` |
|      21 |  489 | `		if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|       - |  490 | `			/*` |
|       - |  491 | `			 * According to the PHP language reference manual.` |
|       - |  492 | `			 *   For any of the types: integer, float, string, boolean converting a value` |
|       - |  493 | `			 *   to an array results in an array with a single element with index zero` |
|       - |  494 | `			 *   and the value of the scalar which was converted.` |
|       - |  495 | `			 */` |
|      17 |  496 | `			if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       - |  497 | `				/* Object cast */` |
|       7 |  498 | `				PH7_ClassInstanceToHashmap((ph7_class_instance *)pObj->x.pOther,pMap);` |
|       4 |  499 | `			}else{` |
|       - |  500 | `				/* Insert a single element */` |
|      11 |  501 | `				PH7_HashmapInsert(pMap,0/* Automatic index assign */,&(*pObj));` |
|       - |  502 | `			}` |
|      17 |  503 | `			SyBlobRelease(&pObj->sBlob);` |
|       8 |  504 | `		}` |
|       - |  505 | `		/* Invalidate any prior representation */` |
|      21 |  506 | `		PH7_MemObjRelease(pObj);` |
|      21 |  507 | `		MemObjSetType(pObj,MEMOBJ_HASHMAP);` |
|      21 |  508 | `		pObj->x.pOther = pMap;` |
|      10 |  509 | `	}` |
|      21 |  510 | `	return SXRET_OK;` |
|      11 |  511 |  |
|       - |  512 | `/*` |
|       - |  513 | ` * Convert a ph7_value to type object.Invalidate any prior representations.` |
|       - |  514 | ` * The new object is instantiated from the builtin stdClass().` |
|       - |  515 | ` * The stdClass() class have a single attribute which is '$value'. This attribute` |
|       - |  516 | ` * hold a copy of the converted ph7_value.` |
|       - |  517 | ` * The internal of the stdClass is as follows:` |
|       - |  518 | ` * class stdClass{` |
|       - |  519 | ` *	 public $value;` |
|       - |  520 | ` *	 public function __toInt(){ return (int)$this->value; }` |
|       - |  521 | ` *	 public function __toBool(){ return (bool)$this->value; }` |
|       - |  522 | ` *	 public function __toFloat(){ return (float)$this->value; }` |
|       - |  523 | ` *	 public function __toString(){ return (string)$this->value; }` |
|       - |  524 | ` *	 function __construct($v){ $this->value = $v; }"` |
|       - |  525 | ` *  }` |
|       - |  526 | ` * Refer to the official documentation for more information.` |
|       - |  527 | ` */` |
|      16 |  528 | `PH7_PRIVATE sxi32 PH7_MemObjToObject(ph7_value *pObj)` |
|       1 |  529 |  |
|      17 |  530 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|       - |  531 | `		ph7_class_instance *pStd;` |
|       - |  532 | `		ph7_class_method *pCons;` |
|       - |  533 | `		ph7_class *pClass;` |
|       - |  534 | `		ph7_vm *pVm;` |
|       - |  535 | `		/* Point to the underlying VM */` |
|      17 |  536 | `		pVm = pObj->pVm;` |
|       - |  537 | `		/* Point to the stdClass() */` |
|      17 |  538 | `		pClass = PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,0,0);` |
|      17 |  539 | `		if( pClass == 0 ){` |
|       - |  540 | `			/* Can't happen,load null instead */` |
|     ! 0 |  541 | `			PH7_MemObjRelease(pObj);` |
|     ! 0 |  542 | `			return SXRET_OK;` |
|       - |  543 | `		}` |
|       - |  544 | `		/* Instanciate a new stdClass() object */` |
|      17 |  545 | `		pStd = PH7_NewClassInstance(pVm,pClass);` |
|      17 |  546 | `		if( pStd == 0 ){` |
|       - |  547 | `			/* Out of memory */` |
|     ! 0 |  548 | `			PH7_MemObjRelease(pObj);` |
|     ! 0 |  549 | `			return SXRET_OK;` |
|       - |  550 | `		}` |
|       - |  551 | `		/* Check if a constructor is available */` |
|      17 |  552 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      17 |  553 | `		if( pCons ){` |
|       - |  554 | `			ph7_value *apArg[2];` |
|       - |  555 | `			/* Invoke the constructor with one argument */` |
|      17 |  556 | `			apArg[0] = pObj;` |
|      17 |  557 | `			PH7_VmCallClassMethod(pVm,pStd,pCons,0,1,apArg);` |
|      17 |  558 | `			if( pStd->iRef < 1 ){` |
|     ! 0 |  559 | `				pStd->iRef = 1;` |
|     ! 0 |  560 | `			}` |
|       8 |  561 | `		}` |
|       - |  562 | `		/* Invalidate any prior representation */` |
|      17 |  563 | `		PH7_MemObjRelease(pObj);` |
|       - |  564 | `		/* Save the new instance */` |
|      17 |  565 | `		pObj->x.pOther = pStd;` |
|      17 |  566 | `		MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       8 |  567 | `	}` |
|      17 |  568 | `	return SXRET_OK;` |
|       9 |  569 |  |
|       - |  570 | `/*` |
|       - |  571 | ` * Return a pointer to the appropriate convertion method associated` |
|       - |  572 | ` * with the given type.` |
|       - |  573 | ` * Note on type juggling.` |
|       - |  574 | ` * Accoding to the PHP language reference manual` |
|       - |  575 | ` *  PHP does not require (or support) explicit type definition in variable` |
|       - |  576 | ` *  declaration; a variable's type is determined by the context in which` |
|       - |  577 | ` *  the variable is used. That is to say, if a string value is assigned` |
|       - |  578 | ` *  to variable $var, $var becomes a string. If an integer value is then` |
|       - |  579 | ` *  assigned to $var, it becomes an integer.` |
|       - |  580 | ` */` |
|     ! 0 |  581 | `PH7_PRIVATE ProcMemObjCast PH7_MemObjCastMethod(sxi32 iFlags)` |
|     ! 0 |  582 |  |
|     ! 0 |  583 | `	if( iFlags & MEMOBJ_STRING ){` |
|     ! 0 |  584 | `		return PH7_MemObjToString;` |
|     ! 0 |  585 | `	}else if( iFlags & MEMOBJ_INT ){` |
|     ! 0 |  586 | `		return PH7_MemObjToInteger;` |
|     ! 0 |  587 | `	}else if( iFlags & MEMOBJ_REAL ){` |
|     ! 0 |  588 | `		return PH7_MemObjToReal;` |
|     ! 0 |  589 | `	}else if( iFlags & MEMOBJ_BOOL ){` |
|     ! 0 |  590 | `		return PH7_MemObjToBool;` |
|     ! 0 |  591 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|     ! 0 |  592 | `		return PH7_MemObjToHashmap;` |
|     ! 0 |  593 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|     ! 0 |  594 | `		return PH7_MemObjToObject;` |
|       - |  595 | `	}` |
|       - |  596 | `	/* NULL cast */` |
|     ! 0 |  597 | `	return PH7_MemObjToNull;` |
|     ! 0 |  598 |  |
|       - |  599 | `/*` |
|       - |  600 | ` * Check whether the ph7_value is numeric [i.e: int/float/bool] or looks` |
|       - |  601 | ` * like a numeric number [i.e: if the ph7_value is of type string.].` |
|       - |  602 | ` * Return TRUE if numeric.FALSE otherwise.` |
|       - |  603 | ` */` |
|  245282 |  604 | `PH7_PRIVATE sxi32 PH7_MemObjIsNumeric(ph7_value *pObj)` |
|       2 |  605 |  |
|  245284 |  606 | `	if( pObj->iFlags & ( MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      46 |  607 | `		return TRUE;` |
|  245240 |  608 | `	}else if( pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|       6 |  609 | `		return FALSE;` |
|  245236 |  610 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       - |  611 | `		SyString sStr;` |
|       - |  612 | `		sxi32 rc;` |
|  245236 |  613 | `		SyStringInitFromBuf(&sStr,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|  245236 |  614 | `		if( sStr.nByte <= 0 ){` |
|       - |  615 | `			/* Empty string */` |
|      73 |  616 | `			return FALSE;` |
|       - |  617 | `		}` |
|       - |  618 | `		/* Check if the string representation looks like a numeric number */` |
|  245164 |  619 | `		rc = SyStrIsNumeric(sStr.zString,sStr.nByte,0,0);` |
|  245164 |  620 | `		return rc == SXRET_OK ? TRUE : FALSE;` |
|       - |  621 | `	}` |
|       - |  622 | `	/* NOT REACHED */` |
|     ! 0 |  623 | `	return FALSE;` |
|  122687 |  624 |  |
|       - |  625 | `/*` |
|       - |  626 | ` * Check whether the ph7_value is empty.Return TRUE if empty.` |
|       - |  627 | ` * FALSE otherwise.` |
|       - |  628 | ` * An ph7_value is considered empty if the following are true:` |
|       - |  629 | ` * NULL value.` |
|       - |  630 | ` * Boolean FALSE.` |
|       - |  631 | ` * Integer/Float with a 0 (zero) value.` |
|       - |  632 | ` * An empty string or a stream of 0 (zero) [i.e: "0","00","000",...].` |
|       - |  633 | ` * An empty array.` |
|       - |  634 | ` * NOTE` |
|       - |  635 | ` *  OBJECT VALUE MUST NOT BE MODIFIED.` |
|       - |  636 | ` */` |
|   16248 |  637 | `PH7_PRIVATE sxi32 PH7_MemObjIsEmpty(ph7_value *pObj)` |
|       2 |  638 |  |
|   16250 |  639 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|       6 |  640 | `		return TRUE;` |
|   16246 |  641 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|     ! 0 |  642 | `		return pObj->x.iVal == 0 ? TRUE : FALSE;` |
|   16246 |  643 | `	}else if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 |  644 | `		return pObj->rVal == (ph7_real)0 ? TRUE : FALSE;` |
|   16246 |  645 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|     ! 0 |  646 | `		return !pObj->x.iVal;` |
|   16246 |  647 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|   16218 |  648 | `		if( SyBlobLength(&pObj->sBlob) <= 0 ){` |
|   13910 |  649 | `			return TRUE;` |
|     ! 0 |  650 | `		}else{` |
|       - |  651 | `			const char *zIn,*zEnd;` |
|    2310 |  652 | `			zIn = (const char *)SyBlobData(&pObj->sBlob);` |
|    2310 |  653 | `			zEnd = &zIn[SyBlobLength(&pObj->sBlob)];` |
|    2310 |  654 | `			while( zIn < zEnd ){` |
|    2310 |  655 | `				if( zIn[0] != '0' ){` |
|    2310 |  656 | `					break;` |
|       - |  657 | `				}` |
|     ! 0 |  658 | `				zIn++;` |
|     ! 0 |  659 | `			}` |
|    2310 |  660 | `			return zIn >= zEnd ? TRUE : FALSE;` |
|     ! 0 |  661 | `		}` |
|      30 |  662 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|      30 |  663 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      30 |  664 | `		return pMap->nEntry == 0 ? TRUE : FALSE;` |
|     ! 0 |  665 | `	}else if ( pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|     ! 0 |  666 | `		return FALSE;` |
|       - |  667 | `	}` |
|       - |  668 | `	/* Assume empty by default */` |
|     ! 0 |  669 | `	return TRUE;` |
|    8126 |  670 |  |
|       - |  671 | `/*` |
|       - |  672 | ` * Convert a ph7_value so that it has types MEMOBJ_REAL or MEMOBJ_INT` |
|       - |  673 | ` * or both.` |
|       - |  674 | ` * Invalidate any prior representations. Every effort is made to force` |
|       - |  675 | ` * the conversion, even if the input is a string that does not look` |
|       - |  676 | ` * completely like a number.Convert as much of the string as we can` |
|       - |  677 | ` * and ignore the rest.` |
|       - |  678 | ` */` |
|  271008 |  679 | `PH7_PRIVATE sxi32 PH7_MemObjToNumeric(ph7_value *pObj)` |
|       2 |  680 |  |
|  271010 |  681 | `	if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|  270978 |  682 | `		if( pObj->iFlags & (MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|       3 |  683 | `			if( pObj->iFlags & MEMOBJ_NULL ){` |
|     ! 0 |  684 | `				pObj->x.iVal = 0;` |
|     ! 0 |  685 | `			}` |
|       3 |  686 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|       1 |  687 | `		}` |
|       - |  688 | `		/* Already numeric */` |
|  270978 |  689 | `		return  SXRET_OK;` |
|       - |  690 | `	}` |
|      33 |  691 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|      33 |  692 | `		sxi32 rc = SXERR_INVALID;` |
|      33 |  693 | `		sxu8 bReal = FALSE;` |
|       - |  694 | `		SyString sString;` |
|      33 |  695 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|       - |  696 | `		/* Check if the given string looks like a numeric number */` |
|      33 |  697 | `		if( sString.nByte > 0 ){` |
|      33 |  698 | `			rc = SyStrIsNumeric(sString.zString,sString.nByte,&bReal,0);` |
|      16 |  699 | `		}` |
|      33 |  700 | `		if( bReal ){` |
|       3 |  701 | `			PH7_MemObjToReal(&(*pObj));` |
|       2 |  702 | `		}else{` |
|      31 |  703 | `			if( rc != SXRET_OK ){` |
|       - |  704 | `				/* The input does not look at all like a number,set the value to 0 */` |
|     ! 0 |  705 | `				pObj->x.iVal = 0;` |
|     ! 0 |  706 | `			}else{` |
|       - |  707 | `				/* Convert as much as we can */` |
|      31 |  708 | `				pObj->x.iVal = MemObjStringToInt(&(*pObj));` |
|       - |  709 | `			}` |
|      31 |  710 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|      31 |  711 | `			SyBlobRelease(&pObj->sBlob);` |
|       1 |  712 | `		}` |
|      16 |  713 | `	}else if(pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)){` |
|     ! 0 |  714 | `		PH7_MemObjToInteger(pObj);` |
|     ! 0 |  715 | `	}else{` |
|       - |  716 | `		/* Perform a blind cast */` |
|     ! 0 |  717 | `		PH7_MemObjToReal(&(*pObj));` |
|       - |  718 | `	}` |
|      33 |  719 | `	return SXRET_OK;` |
|  135528 |  720 |  |
|       - |  721 | `/*` |
|       - |  722 | ` * Try a get an integer representation of the given ph7_value.` |
|       - |  723 | ` * If the ph7_value is not of type real,this function is a no-op.` |
|       - |  724 | ` */` |
|     528 |  725 | `PH7_PRIVATE sxi32 PH7_MemObjTryInteger(ph7_value *pObj)` |
|       1 |  726 |  |
|     529 |  727 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|       - |  728 | `		/* Work only with reals */` |
|     529 |  729 | `		MemObjTryIntger(&(*pObj));` |
|     264 |  730 | `	}` |
|     529 |  731 | `	return SXRET_OK;` |
|       1 |  732 |  |
|       - |  733 | `/*` |
|       - |  734 | ` * Initialize a ph7_value to the null type.` |
|       - |  735 | ` */` |
| 4723156 |  736 | `PH7_PRIVATE sxi32 PH7_MemObjInit(ph7_vm *pVm,ph7_value *pObj)` |
|       2 |  737 |  |
|       - |  738 | `	/* Zero the structure */` |
| 4723158 |  739 | `	SyZero(pObj,sizeof(ph7_value));` |
|       - |  740 | `	/* Initialize fields */` |
| 4723158 |  741 | `	pObj->pVm = pVm;` |
| 4723158 |  742 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|       - |  743 | `	/* Set the NULL type */` |
| 4723158 |  744 | `	pObj->iFlags = MEMOBJ_NULL;` |
| 4723158 |  745 | `	return SXRET_OK;` |
|       2 |  746 |  |
|       - |  747 | `/*` |
|       - |  748 | ` * Initialize a ph7_value to the integer type.` |
|       - |  749 | ` */` |
|   73524 |  750 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromInt(ph7_vm *pVm,ph7_value *pObj,sxi64 iVal)` |
|       2 |  751 |  |
|       - |  752 | `	/* Zero the structure */` |
|   73526 |  753 | `	SyZero(pObj,sizeof(ph7_value));` |
|       - |  754 | `	/* Initialize fields */` |
|   73526 |  755 | `	pObj->pVm = pVm;` |
|   73526 |  756 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|       - |  757 | `	/* Set the desired type */` |
|   73526 |  758 | `	pObj->x.iVal = iVal;` |
|   73526 |  759 | `	pObj->iFlags = MEMOBJ_INT;` |
|   73526 |  760 | `	return SXRET_OK;` |
|       2 |  761 |  |
|       - |  762 | `/*` |
|       - |  763 | ` * Initialize a ph7_value to the boolean type.` |
|       - |  764 | ` */` |
|    9594 |  765 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromBool(ph7_vm *pVm,ph7_value *pObj,sxi32 iVal)` |
|       2 |  766 |  |
|       - |  767 | `	/* Zero the structure */` |
|    9596 |  768 | `	SyZero(pObj,sizeof(ph7_value));` |
|       - |  769 | `	/* Initialize fields */` |
|    9596 |  770 | `	pObj->pVm = pVm;` |
|    9596 |  771 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|       - |  772 | `	/* Set the desired type */` |
|    9596 |  773 | `	pObj->x.iVal = iVal ? 1 : 0;` |
|    9596 |  774 | `	pObj->iFlags = MEMOBJ_BOOL;` |
|    9596 |  775 | `	return SXRET_OK;` |
|       2 |  776 |  |
|       - |  777 | `#if 0` |
|       - |  778 | `/*` |
|       - |  779 | ` * Initialize a ph7_value to the real type.` |
|       - |  780 | ` */` |
|       - |  781 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromReal(ph7_vm *pVm,ph7_value *pObj,ph7_real rVal)` |
|       - |  782 |  |
|       - |  783 | `	/* Zero the structure */` |
|       - |  784 | `	SyZero(pObj,sizeof(ph7_value));` |
|       - |  785 | `	/* Initialize fields */` |
|       - |  786 | `	pObj->pVm = pVm;` |
|       - |  787 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|       - |  788 | `	/* Set the desired type */` |
|       - |  789 | `	pObj->rVal = rVal;` |
|       - |  790 | `	pObj->iFlags = MEMOBJ_REAL;` |
|       - |  791 | `	return SXRET_OK;` |
|       - |  792 |  |
|       - |  793 | `#endif` |
|       - |  794 | `/*` |
|       - |  795 | ` * Initialize a ph7_value to the array type.` |
|       - |  796 | ` */` |
|   18574 |  797 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromArray(ph7_vm *pVm,ph7_value *pObj,ph7_hashmap *pArray)` |
|       2 |  798 |  |
|       - |  799 | `	/* Zero the structure */` |
|   18576 |  800 | `	SyZero(pObj,sizeof(ph7_value));` |
|       - |  801 | `	/* Initialize fields */` |
|   18576 |  802 | `	pObj->pVm = pVm;` |
|   18576 |  803 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|       - |  804 | `	/* Set the desired type */` |
|   18576 |  805 | `	pObj->iFlags = MEMOBJ_HASHMAP;` |
|   18576 |  806 | `	pObj->x.pOther = pArray;` |
|   18576 |  807 | `	return SXRET_OK;` |
|       2 |  808 |  |
|       - |  809 | `/*` |
|       - |  810 | ` * Initialize a ph7_value to the string type.` |
|       - |  811 | ` */` |
|  144950 |  812 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromString(ph7_vm *pVm,ph7_value *pObj,const SyString *pVal)` |
|       2 |  813 |  |
|       - |  814 | `	/* Zero the structure */` |
|  144952 |  815 | `	SyZero(pObj,sizeof(ph7_value));` |
|       - |  816 | `	/* Initialize fields */` |
|  144952 |  817 | `	pObj->pVm = pVm;` |
|  144952 |  818 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|  144952 |  819 | `	if( pVal ){` |
|       - |  820 | `		/* Append contents */` |
|   92166 |  821 | `		SyBlobAppend(&pObj->sBlob,(const void *)pVal->zString,pVal->nByte);` |
|   46082 |  822 | `	}` |
|       - |  823 | `	/* Set the desired type */` |
|  144952 |  824 | `	pObj->iFlags = MEMOBJ_STRING;` |
|  144952 |  825 | `	return SXRET_OK;` |
|       2 |  826 |  |
|       - |  827 | `/*` |
|       - |  828 | ` * Append some contents to the internal buffer of a given ph7_value.` |
|       - |  829 | ` * If the given ph7_value is not of type string,this function` |
|       - |  830 | ` * invalidate any prior representation and set the string type.` |
|       - |  831 | ` * Then a simple append operation is performed.` |
|       - |  832 | ` */` |
|  163924 |  833 | `PH7_PRIVATE sxi32 PH7_MemObjStringAppend(ph7_value *pObj,const char *zData,sxu32 nLen)` |
|       2 |  834 |  |
|       - |  835 | `	sxi32 rc;` |
|  163926 |  836 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  837 | `		/* Invalidate any prior representation */` |
|       5 |  838 | `		PH7_MemObjRelease(pObj);` |
|       5 |  839 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|       2 |  840 | `	}` |
|       - |  841 | `	/* Append contents */` |
|  163926 |  842 | `	rc = SyBlobAppend(&pObj->sBlob,zData,nLen);` |
|  163926 |  843 | `	return rc;` |
|       2 |  844 |  |
|       - |  845 | `#if 0` |
|       - |  846 | `/*` |
|       - |  847 | ` * Format and append some contents to the internal buffer of a given ph7_value.` |
|       - |  848 | ` * If the given ph7_value is not of type string,this function invalidate` |
|       - |  849 | ` * any prior representation and set the string type.` |
|       - |  850 | ` * Then a simple format and append operation is performed.` |
|       - |  851 | ` */` |
|       - |  852 | `PH7_PRIVATE sxi32 PH7_MemObjStringFormat(ph7_value *pObj,const char *zFormat,va_list ap)` |
|       - |  853 |  |
|       - |  854 | `	sxi32 rc;` |
|       - |  855 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  856 | `		/* Invalidate any prior representation */` |
|       - |  857 | `		PH7_MemObjRelease(pObj);` |
|       - |  858 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|       - |  859 | `	}` |
|       - |  860 | `	/* Format and append contents */` |
|       - |  861 | `	rc = SyBlobFormatAp(&pObj->sBlob,zFormat,ap);` |
|       - |  862 | `	return rc;` |
|       - |  863 |  |
|       - |  864 | `#endif` |
|       - |  865 | `/*` |
|       - |  866 | ` * Duplicate the contents of a ph7_value.` |
|       - |  867 | ` */` |
| 3482190 |  868 | `PH7_PRIVATE sxi32 PH7_MemObjStore(ph7_value *pSrc,ph7_value *pDest)` |
|       2 |  869 |  |
| 3482192 |  870 | `	ph7_class_instance *pObj = 0;` |
| 3482192 |  871 | `	ph7_hashmap *pMap = 0;` |
|       - |  872 | `	sxi32 rc;` |
| 3482192 |  873 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|       - |  874 | `		/* Increment reference count */` |
|   86704 |  875 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
| 3438841 |  876 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|       - |  877 | `		/* Increment reference count */` |
|    1116 |  878 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|     557 |  879 | `	}` |
| 3482192 |  880 | `	if( pDest->iFlags & MEMOBJ_HASHMAP ){` |
|   25178 |  881 | `		pMap = (ph7_hashmap *)pDest->x.pOther;` |
| 3469604 |  882 | `	}else if( pDest->iFlags & MEMOBJ_OBJ ){` |
|     742 |  883 | `		pObj = (ph7_class_instance *)pDest->x.pOther;` |
|     370 |  884 | `	}` |
| 3482192 |  885 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
| 3482192 |  886 | `	pDest->iFlags &= ~MEMOBJ_AUX;` |
| 3482192 |  887 | `	rc = SXRET_OK;` |
| 3482192 |  888 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
| 3052958 |  889 | `		SyBlobReset(&pDest->sBlob);` |
| 3052958 |  890 | `		rc = SyBlobDup(&pSrc->sBlob,&pDest->sBlob);` |
| 1526480 |  891 | `	}else{` |
|  429236 |  892 | `		if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|  169890 |  893 | `			SyBlobRelease(&pDest->sBlob);` |
|   84966 |  894 | `		}` |
|       - |  895 | `	}` |
| 3482192 |  896 | `	if( pMap ){` |
|   25178 |  897 | `		PH7_HashmapUnref(pMap);` |
| 3469604 |  898 | `	}else if( pObj ){` |
|     742 |  899 | `		PH7_ClassInstanceUnref(pObj);` |
|     370 |  900 | `	}` |
| 3482192 |  901 | `	return rc;` |
|       2 |  902 |  |
|       - |  903 | `/*` |
|       - |  904 | ` * Duplicate the contents of a ph7_value but do not copy internal` |
|       - |  905 | ` * buffer contents,simply point to it.` |
|       - |  906 | ` */` |
| 3618548 |  907 | `PH7_PRIVATE sxi32 PH7_MemObjLoad(ph7_value *pSrc,ph7_value *pDest)` |
|       2 |  908 |  |
| 3618550 |  909 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),` |
|       - |  910 | `		sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
| 3618550 |  911 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|       - |  912 | `		/* Increment reference count */` |
|  265128 |  913 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
| 3485987 |  914 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|       - |  915 | `		/* Increment reference count */` |
|    2922 |  916 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|    1460 |  917 | `	}` |
| 3618550 |  918 | `	if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|      19 |  919 | `		SyBlobRelease(&pDest->sBlob);` |
|       9 |  920 | `	}` |
| 3618550 |  921 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
| 1902382 |  922 | `		SyBlobReadOnly(&pDest->sBlob,SyBlobData(&pSrc->sBlob),SyBlobLength(&pSrc->sBlob));` |
|  951278 |  923 | `	}` |
| 3618550 |  924 | `	return SXRET_OK;` |
|       2 |  925 |  |
|       - |  926 | `/*` |
|       - |  927 | ` * Invalidate any prior representation of a given ph7_value.` |
|       - |  928 | ` */` |
| 7915578 |  929 | `PH7_PRIVATE sxi32 PH7_MemObjRelease(ph7_value *pObj)` |
|       2 |  930 |  |
| 7915580 |  931 | `	if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
| 7361646 |  932 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|  326608 |  933 | `			PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
| 7198343 |  934 | `		}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|    4716 |  935 | `			PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|    2357 |  936 | `		}` |
|       - |  937 | `		/* Release the internal buffer */` |
| 7361646 |  938 | `		SyBlobRelease(&pObj->sBlob);` |
|       - |  939 | `		/* Invalidate any prior representation */` |
| 7361646 |  940 | `		pObj->iFlags = MEMOBJ_NULL;` |
| 3681042 |  941 | `	}` |
| 7915580 |  942 | `	return SXRET_OK;` |
|       2 |  943 |  |
|       - |  944 | `/*` |
|       - |  945 | ` * Compare two ph7_values.` |
|       - |  946 | ` * Return 0 if the values are equals, > 0 if pObj1 is greater than pObj2` |
|       - |  947 | ` * or < 0 if pObj2 is greater than pObj1.` |
|       - |  948 | ` * Type comparison table taken from the PHP language reference manual.` |
|       - |  949 | ` * Comparisons of $x with PHP functions Expression` |
|       - |  950 | ` *              gettype() 	empty() 	is_null() 	isset() 	boolean : if($x)` |
|       - |  951 | ` * $x = ""; 	string 	    TRUE 	FALSE 	TRUE 	FALSE` |
|       - |  952 | ` * $x = null 	NULL 	    TRUE 	TRUE 	FALSE 	FALSE` |
|       - |  953 | ` * var $x; 	    NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|       - |  954 | ` * $x is undefined 	NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|       - |  955 | ` *  $x = array(); 	array 	TRUE 	FALSE 	TRUE 	FALSE` |
|       - |  956 | ` * $x = false; 	boolean 	TRUE 	FALSE 	TRUE 	FALSE` |
|       - |  957 | ` * $x = true; 	boolean 	FALSE 	FALSE 	TRUE 	TRUE` |
|       - |  958 | ` * $x = 1; 	    integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|       - |  959 | ` * $x = 42; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|       - |  960 | ` * $x = 0; 	    integer 	TRUE 	FALSE 	TRUE 	FALSE` |
|       - |  961 | ` * $x = -1; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|       - |  962 | ` * $x = "1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|       - |  963 | ` * $x = "0"; 	string 	TRUE 	FALSE 	TRUE 	FALSE` |
|       - |  964 | ` * $x = "-1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|       - |  965 | ` * $x = "php"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|       - |  966 | ` * $x = "true"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|       - |  967 | ` * $x = "false"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|       - |  968 | ` *      Loose comparisons with ==` |
|       - |  969 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|       - |  970 | ` * TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE` |
|       - |  971 | ` * FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|       - |  972 | ` * 1 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  973 | ` * 0 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE` |
|       - |  974 | ` * -1 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  975 | ` * "1" 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  976 | ` * "0" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  977 | ` * "-1" 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  978 | ` * NULL 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|       - |  979 | ` * array() 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	FALSE` |
|       - |  980 | ` * "php" 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|       - |  981 | ` * "" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE` |
|       - |  982 | ` *    Strict comparisons with ===` |
|       - |  983 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|       - |  984 | ` * TRUE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  985 | ` * FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  986 | ` * 1 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  987 | ` * 0 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  988 | ` * -1 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  989 | ` * "1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  990 | ` * "0" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  991 | ` * "-1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  992 | ` * NULL 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE` |
|       - |  993 | ` * array() 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE` |
|       - |  994 | ` * "php" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|       - |  995 | ` * "" 	    FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE` |
|       - |  996 | ` */` |
|  687788 |  997 | `PH7_PRIVATE sxi32 PH7_MemObjCmp(ph7_value *pObj1,ph7_value *pObj2,int bStrict,int iNest)` |
|       2 |  998 |  |
|       - |  999 | `	sxi32 iComb;` |
|       - | 1000 | `	sxi32 rc;` |
|  687790 | 1001 | `	if( bStrict ){` |
|       - | 1002 | `		sxi32 iF1,iF2;` |
|       - | 1003 | `		/* Strict comparisons with === */` |
|  336044 | 1004 | `		iF1 = pObj1->iFlags&~MEMOBJ_AUX;` |
|  336044 | 1005 | `		iF2 = pObj2->iFlags&~MEMOBJ_AUX;` |
|  336044 | 1006 | `		if( iF1 != iF2 ){` |
|       - | 1007 | `			/* Not of the same type */` |
|   95708 | 1008 | `			return 1;` |
|       - | 1009 | `		}` |
|  120168 | 1010 | `	}` |
|       - | 1011 | `	/* Combine flag together */` |
|  592084 | 1012 | `	iComb = pObj1->iFlags\|pObj2->iFlags;` |
|  592084 | 1013 | `	if( iComb & (MEMOBJ_NULL\|MEMOBJ_RES\|MEMOBJ_BOOL) ){` |
|       - | 1014 | `		/* Convert to boolean: Keep in mind FALSE < TRUE */` |
|   11690 | 1015 | `		if( (pObj1->iFlags & MEMOBJ_BOOL) == 0 ){` |
|    4168 | 1016 | `			PH7_MemObjToBool(pObj1);` |
|    2083 | 1017 | `		}` |
|   11690 | 1018 | `		if( (pObj2->iFlags & MEMOBJ_BOOL) == 0 ){` |
|    3394 | 1019 | `			PH7_MemObjToBool(pObj2);` |
|    1696 | 1020 | `		}` |
|   11690 | 1021 | `		return (sxi32)((pObj1->x.iVal != 0) - (pObj2->x.iVal != 0));` |
|  580396 | 1022 | `	}else if ( iComb & MEMOBJ_HASHMAP ){` |
|       - | 1023 | `		/* Hashmap aka 'array' comparison */` |
|       9 | 1024 | `		if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       - | 1025 | `			/* Array is always greater */` |
|     ! 0 | 1026 | `			return -1;` |
|       - | 1027 | `		}` |
|       9 | 1028 | `		if( (pObj2->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       - | 1029 | `			/* Array is always greater */` |
|     ! 0 | 1030 | `			return 1;` |
|       - | 1031 | `		}` |
|       - | 1032 | `		/* Perform the comparison */` |
|       9 | 1033 | `		rc = PH7_HashmapCmp((ph7_hashmap *)pObj1->x.pOther,(ph7_hashmap *)pObj2->x.pOther,bStrict);` |
|       9 | 1034 | `		return rc;` |
|  580388 | 1035 | `	}else if(iComb & MEMOBJ_OBJ ){` |
|       - | 1036 | `		/* Object comparison */` |
|     162 | 1037 | `		if( (pObj1->iFlags & MEMOBJ_OBJ) == 0 ){` |
|       - | 1038 | `			/* Object is always greater */` |
|     ! 0 | 1039 | `			return -1;` |
|       - | 1040 | `		}` |
|     162 | 1041 | `		if( (pObj2->iFlags & MEMOBJ_OBJ) == 0 ){` |
|       - | 1042 | `			/* Object is always greater */` |
|     ! 0 | 1043 | `			return 1;` |
|       - | 1044 | `		}` |
|       - | 1045 | `		/* Perform the comparison */` |
|     162 | 1046 | `		rc = PH7_ClassInstanceCmp((ph7_class_instance *)pObj1->x.pOther,(ph7_class_instance *)pObj2->x.pOther,bStrict,iNest);` |
|     162 | 1047 | `		return rc;` |
|  580228 | 1048 | `	}else if ( iComb & MEMOBJ_STRING ){` |
|       - | 1049 | `		SyString s1,s2;` |
|  351634 | 1050 | `		if( !bStrict ){` |
|       - | 1051 | `			/*` |
|       - | 1052 | `			 * According to the PHP language reference manual:` |
|       - | 1053 | `			 *` |
|       - | 1054 | `			 *  If you compare a number with a string or the comparison involves numerical` |
|       - | 1055 | `			 *  strings, then each string is converted to a number and the comparison` |
|       - | 1056 | `			 *  performed numerically.` |
|       - | 1057 | `			 */` |
|  122618 | 1058 | `			if( PH7_MemObjIsNumeric(pObj1) ){` |
|       - | 1059 | `				/* Perform a numeric comparison */` |
|       9 | 1060 | `				goto Numeric;` |
|       - | 1061 | `			}` |
|  122610 | 1062 | `			if( PH7_MemObjIsNumeric(pObj2) ){` |
|       - | 1063 | `				/* Perform a numeric comparison */` |
|     ! 0 | 1064 | `				goto Numeric;` |
|       - | 1065 | `			}` |
|   61326 | 1066 | `		}` |
|       - | 1067 | `		/* Perform a strict string comparison.*/` |
|  351626 | 1068 | `		if( (pObj1->iFlags&MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1069 | `			PH7_MemObjToString(pObj1);` |
|     ! 0 | 1070 | `		}` |
|  351626 | 1071 | `		if( (pObj2->iFlags&MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1072 | `			PH7_MemObjToString(pObj2);` |
|     ! 0 | 1073 | `		}` |
|  351626 | 1074 | `		SyStringInitFromBuf(&s1,SyBlobData(&pObj1->sBlob),SyBlobLength(&pObj1->sBlob));` |
|  351626 | 1075 | `		SyStringInitFromBuf(&s2,SyBlobData(&pObj2->sBlob),SyBlobLength(&pObj2->sBlob));` |
|       - | 1076 | `		/*` |
|       - | 1077 | `		 * Strings are compared using memcmp(). If one value is an exact prefix of the` |
|       - | 1078 | `		 * other, then the shorter value is less than the longer value.` |
|       - | 1079 | `		 */` |
|  351626 | 1080 | `		rc = SyMemcmp((const void *)s1.zString,(const void *)s2.zString,SXMIN(s1.nByte,s2.nByte));` |
|  351626 | 1081 | `		if( rc == 0 ){` |
|  124737 | 1082 | `			if( s1.nByte != s2.nByte ){` |
|     907 | 1083 | `				rc = s1.nByte < s2.nByte ? -1 : 1;` |
|     454 | 1084 | `			}` |
|   62369 | 1085 | `		}` |
|  351626 | 1086 | `		return rc;` |
|  228596 | 1087 | `	}else if( iComb & (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|  114275 | 1088 | `Numeric:` |
|       - | 1089 | `		/* Perform a numeric comparison if one of the operand is numeric(integer or real) */` |
|  228604 | 1090 | `		if( (pObj1->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|       7 | 1091 | `			PH7_MemObjToNumeric(pObj1);` |
|       3 | 1092 | `		}` |
|  228604 | 1093 | `		if( (pObj2->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|       7 | 1094 | `			PH7_MemObjToNumeric(pObj2);` |
|       3 | 1095 | `		}` |
|  228604 | 1096 | `		if( (pObj1->iFlags & pObj2->iFlags & MEMOBJ_INT) == 0) {` |
|       - | 1097 | `			/*` |
|       - | 1098 | `			 * Symisc eXtension to the PHP language:` |
|       - | 1099 | `			 *  Floating point comparison is introduced and works as expected.` |
|       - | 1100 | `			 */` |
|       - | 1101 | `			ph7_real r1,r2;` |
|       - | 1102 | `			/* Compare as reals */` |
|     129 | 1103 | `			if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|       9 | 1104 | `				PH7_MemObjToReal(pObj1);` |
|       4 | 1105 | `			}` |
|     129 | 1106 | `			r1 = pObj1->rVal;` |
|     129 | 1107 | `			if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|      23 | 1108 | `				PH7_MemObjToReal(pObj2);` |
|      11 | 1109 | `			}` |
|     129 | 1110 | `			r2 = pObj2->rVal;` |
|     129 | 1111 | `			if( PH7_IS_NAN(r1) \|\| PH7_IS_NAN(r2) ){` |
|       - | 1112 | `				/*` |
|       - | 1113 | `				 * Keep a strict three-way comparator contract even for NaN values.` |
|       - | 1114 | `				 * For ordering purposes, NaN compares equal to NaN and greater than` |
|       - | 1115 | `				 * any non-NaN numeric value.` |
|       - | 1116 | `				 */` |
|      31 | 1117 | `				if( PH7_IS_NAN(r1) ){` |
|      23 | 1118 | `					return PH7_IS_NAN(r2) ? 0 : 1;` |
|       - | 1119 | `				}` |
|       9 | 1120 | `				return -1;` |
|       - | 1121 | `			}` |
|      99 | 1122 | `			if( r1 > r2 ){` |
|      11 | 1123 | `				return 1;` |
|      89 | 1124 | `			}else if( r1 < r2 ){` |
|      79 | 1125 | `				return -1;` |
|       - | 1126 | `			}` |
|      11 | 1127 | `			return 0;` |
|     ! 0 | 1128 | `		}else{` |
|       - | 1129 | `			/* Integer comparison */` |
|  228476 | 1130 | `			if( pObj1->x.iVal > pObj2->x.iVal ){` |
|    1673 | 1131 | `				return 1;` |
|  226805 | 1132 | `			}else if( pObj1->x.iVal < pObj2->x.iVal ){` |
|  224075 | 1133 | `				return -1;` |
|       - | 1134 | `			}` |
|    2732 | 1135 | `			return 0;` |
|       - | 1136 | `		}` |
|       - | 1137 | `	}` |
|       - | 1138 | `	/* NOT REACHED */` |
|     ! 0 | 1139 | `	return 0;` |
|  343940 | 1140 |  |
|       - | 1141 | `/*` |
|       - | 1142 | ` * Perform an addition operation of two ph7_values.` |
|       - | 1143 | ` * The reason this function is implemented here rather than 'vm.c'` |
|       - | 1144 | ` * is that the '+' operator is overloaded.` |
|       - | 1145 | ` * That is,the '+' operator is used for arithmetic operation and also` |
|       - | 1146 | ` * used for operation on arrays [i.e: union]. When used with an array` |
|       - | 1147 | ` * The + operator returns the right-hand array appended to the left-hand array.` |
|       - | 1148 | ` * For keys that exist in both arrays, the elements from the left-hand array` |
|       - | 1149 | ` * will be used, and the matching elements from the right-hand array will` |
|       - | 1150 | ` * be ignored.` |
|       - | 1151 | ` * This function take care of handling all the scenarios.` |
|       - | 1152 | ` */` |
|    1802 | 1153 | `PH7_PRIVATE sxi32 PH7_MemObjAdd(ph7_value *pObj1,ph7_value *pObj2,int bAddStore)` |
|       2 | 1154 |  |
|    1804 | 1155 | `	if( ((pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP) == 0 ){` |
|       - | 1156 | `			/* Arithemtic operation */` |
|    1800 | 1157 | `			PH7_MemObjToNumeric(pObj1);` |
|    1800 | 1158 | `			PH7_MemObjToNumeric(pObj2);` |
|    1800 | 1159 | `			if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_REAL ){` |
|       - | 1160 | `				/* Floating point arithmetic */` |
|       - | 1161 | `				ph7_real a,b;` |
|      25 | 1162 | `				if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|       7 | 1163 | `					PH7_MemObjToReal(pObj1);` |
|       3 | 1164 | `				}` |
|      25 | 1165 | `				if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|       3 | 1166 | `					PH7_MemObjToReal(pObj2);` |
|       1 | 1167 | `				}` |
|      25 | 1168 | `				a = pObj1->rVal;` |
|      25 | 1169 | `				b = pObj2->rVal;` |
|      25 | 1170 | `				pObj1->rVal = a+b;` |
|      25 | 1171 | `				MemObjSetType(pObj1,MEMOBJ_REAL);` |
|       - | 1172 | `				/* Try to get an integer representation also */` |
|      25 | 1173 | `				MemObjTryIntger(&(*pObj1));` |
|      13 | 1174 | `			}else{` |
|       - | 1175 | `				/* Integer arithmetic */` |
|       - | 1176 | `				sxi64 a,b;` |
|    1776 | 1177 | `				a = pObj1->x.iVal;` |
|    1776 | 1178 | `				b = pObj2->x.iVal;` |
|    1776 | 1179 | `				pObj1->x.iVal = a+b;` |
|    1776 | 1180 | `				MemObjSetType(pObj1,MEMOBJ_INT);` |
|       - | 1181 | `			}` |
|     901 | 1182 | `	}else{` |
|       6 | 1183 | `		if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP ){` |
|       - | 1184 | `			ph7_hashmap *pMap;` |
|       - | 1185 | `			sxi32 rc;` |
|       6 | 1186 | `			if( bAddStore ){` |
|       - | 1187 | `				/* Do not duplicate the hashmap,use the left one since its an add&store operation.` |
|       - | 1188 | `				 */` |
|     ! 0 | 1189 | `				if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       - | 1190 | `					/* Force a hashmap cast */` |
|     ! 0 | 1191 | `					rc = PH7_MemObjToHashmap(pObj1);` |
|     ! 0 | 1192 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 1193 | `						PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|     ! 0 | 1194 | `						return rc;` |
|       - | 1195 | `					}` |
|     ! 0 | 1196 | `				}` |
|       - | 1197 | `				/* Point to the structure that describe the hashmap */` |
|     ! 0 | 1198 | `				pMap = (ph7_hashmap *)pObj1->x.pOther;` |
|     ! 0 | 1199 | `			}else{` |
|       - | 1200 | `				/* Create a new hashmap */` |
|       6 | 1201 | `				pMap = PH7_NewHashmap(pObj1->pVm,0,0);` |
|       6 | 1202 | `				if( pMap == 0){` |
|     ! 0 | 1203 | `					PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|     ! 0 | 1204 | `					return SXERR_MEM;` |
|       - | 1205 | `				}` |
|       - | 1206 | `			}` |
|       6 | 1207 | `			if( !bAddStore ){` |
|       6 | 1208 | `				if(pObj1->iFlags & MEMOBJ_HASHMAP ){` |
|       - | 1209 | `					/* Perform a hashmap duplication */` |
|       6 | 1210 | `					PH7_HashmapDup((ph7_hashmap *)pObj1->x.pOther,pMap);` |
|       4 | 1211 | `				}else{` |
|     ! 0 | 1212 | `					if((pObj1->iFlags & MEMOBJ_NULL) == 0 ){` |
|       - | 1213 | `						/* Simple insertion */` |
|     ! 0 | 1214 | `						PH7_HashmapInsert(pMap,0,pObj1);` |
|     ! 0 | 1215 | `					}` |
|       - | 1216 | `				}` |
|       2 | 1217 | `			}` |
|       - | 1218 | `			/* Perform the union */` |
|       6 | 1219 | `			if(pObj2->iFlags & MEMOBJ_HASHMAP ){` |
|       6 | 1220 | `				PH7_HashmapUnion(pMap,(ph7_hashmap *)pObj2->x.pOther);` |
|       4 | 1221 | `			}else{` |
|     ! 0 | 1222 | `				if((pObj2->iFlags & MEMOBJ_NULL) == 0 ){` |
|       - | 1223 | `					/* Simple insertion */` |
|     ! 0 | 1224 | `					PH7_HashmapInsert(pMap,0,pObj2);` |
|     ! 0 | 1225 | `				}` |
|       - | 1226 | `			}` |
|       - | 1227 | `			/* Reflect the change */` |
|       6 | 1228 | `			if( pObj1->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 1229 | `				SyBlobRelease(&pObj1->sBlob);` |
|     ! 0 | 1230 | `			}` |
|       6 | 1231 | `			pObj1->x.pOther = pMap;` |
|       6 | 1232 | `			MemObjSetType(pObj1,MEMOBJ_HASHMAP);` |
|       2 | 1233 | `		}` |
|       - | 1234 | `	}` |
|    1804 | 1235 | `	return SXRET_OK;` |
|     903 | 1236 |  |
|       - | 1237 | `/*` |
|       - | 1238 | ` * Return a printable representation of the type of a given` |
|       - | 1239 | ` * ph7_value.` |
|       - | 1240 | ` */` |
|     436 | 1241 | `PH7_PRIVATE const char * PH7_MemObjTypeDump(ph7_value *pVal)` |
|       2 | 1242 |  |
|     438 | 1243 | `	const char *zType = "";` |
|     438 | 1244 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|      13 | 1245 | `		zType = "null";` |
|     432 | 1246 | `	}else if( pVal->iFlags & MEMOBJ_INT ){` |
|     104 | 1247 | `		zType = "int";` |
|     375 | 1248 | `	}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|       7 | 1249 | `		zType = "double";` |
|     321 | 1250 | `	}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|      52 | 1251 | `		zType = "string";` |
|     293 | 1252 | `	}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|     120 | 1253 | `		zType = "bool";` |
|     209 | 1254 | `	}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|      20 | 1255 | `		zType = "array";` |
|     140 | 1256 | `	}else if( pVal->iFlags & MEMOBJ_OBJ ){` |
|     131 | 1257 | `		zType = "object";` |
|      65 | 1258 | `	}else if( pVal->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 1259 | `		zType = "resource";` |
|     ! 0 | 1260 | `	}` |
|     438 | 1261 | `	return zType;` |
|       2 | 1262 |  |
|       - | 1263 | `/*` |
|       - | 1264 | ` * Dump a ph7_value [i.e: get a printable representation of it's type and contents.].` |
|       - | 1265 | ` * Store the dump in the given blob.` |
|       - | 1266 | ` */` |
|     498 | 1267 | `PH7_PRIVATE sxi32 PH7_MemObjDump(` |
|       - | 1268 | `	SyBlob *pOut,      /* Store the dump here */` |
|       - | 1269 | `	ph7_value *pObj,   /* Dump this */` |
|       - | 1270 | `	int ShowType,      /* TRUE to output value type */` |
|       - | 1271 | `	int nTab,          /* # of Whitespace to insert */` |
|       - | 1272 | `	int nDepth,        /* Nesting level */` |
|       - | 1273 | `	int isRef          /* TRUE if referenced object */` |
|       - | 1274 | `	)` |
|       2 | 1275 |  |
|     500 | 1276 | `	sxi32 rc = SXRET_OK;` |
|       - | 1277 | `	const char *zType;` |
|       - | 1278 | `	int i;` |
|    4688 | 1279 | `	for( i = 0 ; i < nTab ; i++ ){` |
|    4190 | 1280 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|    2096 | 1281 | `	}` |
|     500 | 1282 | `	if( ShowType ){` |
|     410 | 1283 | `		if( isRef ){` |
|     ! 0 | 1284 | `			SyBlobAppend(&(*pOut),"&",sizeof(char));` |
|     ! 0 | 1285 | `		}` |
|       - | 1286 | `		/* Get value type first */` |
|     410 | 1287 | `		zType = PH7_MemObjTypeDump(pObj);` |
|     410 | 1288 | `		SyBlobAppend(&(*pOut),zType,SyStrlen(zType));` |
|     204 | 1289 | `	}` |
|     500 | 1290 | `	if((pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|     490 | 1291 | `		if ( ShowType ){` |
|     400 | 1292 | `			SyBlobAppend(&(*pOut),"(",sizeof(char));` |
|     199 | 1293 | `		}` |
|     490 | 1294 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       - | 1295 | `			/* Dump hashmap entries */` |
|      30 | 1296 | `			rc = PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,ShowType,nTab+1,nDepth+1);` |
|     476 | 1297 | `		}else if(pObj->iFlags & MEMOBJ_OBJ ){` |
|       - | 1298 | `			/* Dump class instance attributes */` |
|     133 | 1299 | `			rc = PH7_ClassInstanceDump(&(*pOut),(ph7_class_instance *)pObj->x.pOther,ShowType,nTab+1,nDepth+1);` |
|      67 | 1300 | `		}else{` |
|     330 | 1301 | `			SyBlob *pContents = &pObj->sBlob;` |
|       - | 1302 | `			/* Get a printable representation of the contents */` |
|     330 | 1303 | `			if((pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|     240 | 1304 | `				MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|     121 | 1305 | `			}else{` |
|       - | 1306 | `				/* Append length first */` |
|      92 | 1307 | `				if( ShowType ){` |
|      36 | 1308 | `					SyBlobFormat(&(*pOut),"%u '",SyBlobLength(&pObj->sBlob));` |
|      17 | 1309 | `				}` |
|      92 | 1310 | `				if( SyBlobLength(pContents) > 0 ){` |
|      82 | 1311 | `					SyBlobAppend(&(*pOut),SyBlobData(pContents),SyBlobLength(pContents));` |
|      40 | 1312 | `				}` |
|      92 | 1313 | `				if( ShowType ){` |
|      36 | 1314 | `					SyBlobAppend(&(*pOut),"'",sizeof(char));` |
|      17 | 1315 | `				}` |
|       - | 1316 | `			}` |
|       - | 1317 | `		}` |
|     490 | 1318 | `		if( ShowType ){` |
|     400 | 1319 | `			if( (pObj->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|     256 | 1320 | `				SyBlobAppend(&(*pOut),")",sizeof(char));` |
|     127 | 1321 | `			}` |
|     199 | 1322 | `		}` |
|     244 | 1323 | `	}` |
|       - | 1324 | `#ifdef __WINNT__` |
|       2 | 1325 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 1326 | `#else` |
|     498 | 1327 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 1328 | `#endif` |
|     500 | 1329 | `	return rc;` |
|       2 | 1330 |  |
|       - | 1331 |  |
