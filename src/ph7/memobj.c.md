# src/ph7/memobj.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 610/704 lines (86.65%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|       - |    7 | `/* This file handle low-level stuff related to indexed memory objects [i.e: ph7_value] */` |
|       - |    8 | `/*` |
|       - |    9 | ` * Notes on memory objects [i.e: ph7_value].` |
|       - |   10 | ` * Internally, the PH7 virtual machine manipulates nearly all PHP values` |
|       - |   11 | ` * [i.e: string,int,float,resource,object,bool,null..] as ph7_values structures.` |
|       - |   12 | ` * Each ph7_values struct may cache multiple representations (string,` |
|       - |   13 | ` * integer etc.) of the same value.` |
|       - |   14 | ` */` |
|       - |   15 | `/*` |
|       - |   16 | ` * Convert a 64-bit IEEE double into a 64-bit signed integer.` |
|       - |   17 | ` * If the double is too large, return 0x8000000000000000.` |
|       - |   18 | ` *` |
|       - |   19 | ` * Most systems appear to do this simply by assigning ariables and without` |
|       - |   20 | ` * the extra range tests.` |
|       - |   21 | ` * But there are reports that windows throws an expection if the floating` |
|       - |   22 | ` * point value is out of range.` |
|       - |   23 | ` */` |
|    1078 |   24 | `static sxi64 MemObjRealToInt(ph7_value *pObj)` |
|       2 |   25 |  |
|       - |   26 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|       - |   27 | `	/* Real and 64bit integer are the same when floating point arithmetic` |
|       - |   28 | `	 * is omitted from the build.` |
|       - |   29 | `	 */` |
|       - |   30 | `	return pObj->rVal;` |
|       - |   31 | `#else` |
|       - |   32 | ` /*` |
|       - |   33 | `  ** Many compilers we encounter do not define constants for the` |
|       - |   34 | `  ** minimum and maximum 64-bit integers, or they define them` |
|       - |   35 | `  ** inconsistently.  And many do not understand the "LL" notation.` |
|       - |   36 | `  ** So we define our own static constants here using nothing` |
|       - |   37 | `  ** larger than a 32-bit integer constant.` |
|       - |   38 | `  */` |
|       - |   39 | `  static const sxi64 maxInt = LARGEST_INT64;` |
|       - |   40 | `  static const sxi64 minInt = SMALLEST_INT64;` |
|    1080 |   41 | `  ph7_real r = pObj->rVal;` |
|    1080 |   42 | `  if( r<(ph7_real)minInt ){` |
|     ! 0 |   43 | `    return minInt;` |
|    1080 |   44 | `  }else if( r>(ph7_real)maxInt ){` |
|       - |   45 | `    /* minInt is correct here - not maxInt.  It turns out that assigning` |
|       - |   46 | `    ** a very large positive number to an integer results in a very large` |
|       - |   47 | `    ** negative integer.  This makes no sense, but it is what x86 hardware` |
|       - |   48 | `    ** does so for compatibility we will do the same in software. */` |
|       3 |   49 | `    return minInt;` |
|     ! 0 |   50 | `  }else{` |
|    1078 |   51 | `    return (sxi64)r;` |
|       - |   52 | `  }` |
|       - |   53 | `#endif` |
|     541 |   54 |  |
|       - |   55 | `/*` |
|       - |   56 | ` * Convert a raw token value typically a stream of digit [i.e: hex,octal,binary or decimal]` |
|       - |   57 | ` * to a 64-bit integer.` |
|       - |   58 | ` */` |
|   41182 |   59 | `PH7_PRIVATE sxi64 PH7_TokenValueToInt64(SyString *pVal)` |
|       2 |   60 |  |
|   41184 |   61 | `	sxi64 iVal = 0;` |
|   41184 |   62 | `	if( pVal->nByte <= 0 ){` |
|     ! 0 |   63 | `		return 0;` |
|       - |   64 | `	}` |
|   41184 |   65 | `	if( pVal->zString[0] == '0' ){` |
|       - |   66 | `		sxi32 c;` |
|   16182 |   67 | `		if( pVal->nByte == sizeof(char) ){` |
|   16120 |   68 | `			return 0;` |
|       - |   69 | `		}` |
|      63 |   70 | `		c = pVal->zString[1];` |
|      63 |   71 | `		if( c  == 'x' \|\| c == 'X' ){` |
|       - |   72 | `			/* Hex digit stream */` |
|      13 |   73 | `			SyHexStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|      57 |   74 | `		}else if( c == 'b' \|\| c == 'B' ){` |
|       - |   75 | `			/* Binary digit stream */` |
|      31 |   76 | `			SyBinaryStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|      16 |   77 | `		}else{` |
|       - |   78 | `			/* Octal digit stream */` |
|      21 |   79 | `			SyOctalStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|       - |   80 | `		}` |
|      32 |   81 | `	}else{` |
|       - |   82 | `		/* Decimal digit stream */` |
|   25004 |   83 | `		SyStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|       - |   84 | `	}` |
|   25066 |   85 | `	return iVal;` |
|   20593 |   86 |  |
|       - |   87 | `/*` |
|       - |   88 | ` * Return some kind of 64-bit integer value which is the best we can` |
|       - |   89 | ` * do at representing the value that pObj describes as a string` |
|       - |   90 | ` * representation.` |
|       - |   91 | ` */` |
|     280 |   92 | `static sxi64 MemObjStringToInt(ph7_value *pObj)` |
|       1 |   93 |  |
|       - |   94 | `	SyString sVal;` |
|     281 |   95 | `	SyStringInitFromBuf(&sVal,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|     281 |   96 | `	return PH7_TokenValueToInt64(&sVal);` |
|       1 |   97 |  |
|       - |   98 | `/*` |
|       - |   99 | ` * Call a magic class method [i.e: __toString(),__toInt(),...]` |
|       - |  100 | ` * Return SXRET_OK if the magic method is available and have been` |
|       - |  101 | ` * successfully called. Any other return value indicates failure.` |
|       - |  102 | ` */` |
|      86 |  103 | `static sxi32 MemObjCallClassCastMethod(` |
|       - |  104 | `	ph7_vm *pVm,               /* VM that trigger the invocation */` |
|       - |  105 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object] */` |
|       - |  106 | `	const char *zMethod,       /* Magic method name [i.e: __toString] */` |
|       - |  107 | `	sxu32 nLen,                /* Method name length */` |
|       - |  108 | `	ph7_value *pResult         /* OUT: Store the return value of the magic method here */` |
|       - |  109 | `	)` |
|       2 |  110 |  |
|       - |  111 | `	ph7_class_method *pMethod;` |
|       - |  112 | `	/* Check if the method is available */` |
|      88 |  113 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,zMethod,nLen);` |
|      88 |  114 | `	if( pMethod == 0 ){` |
|       - |  115 | `		/* No such method */` |
|       3 |  116 | `		return SXERR_NOTFOUND;` |
|       - |  117 | `	}` |
|       - |  118 | `	/* Invoke the desired method */` |
|      86 |  119 | `	PH7_VmCallClassMethod(&(*pVm),&(*pThis),pMethod,&(*pResult),0,0);` |
|       - |  120 | `	/* Method successfully called,pResult should hold the return value */` |
|      86 |  121 | `	return SXRET_OK;` |
|      45 |  122 |  |
|       - |  123 | `/*` |
|       - |  124 | ` * Return some kind of integer value which is the best we can` |
|       - |  125 | ` * do at representing the value that pObj describes as an integer.` |
|       - |  126 | ` * If pObj is an integer, then the value is exact. If pObj is` |
|       - |  127 | ` * a floating-point then  the value returned is the integer part.` |
|       - |  128 | ` * If pObj is a string, then we make an attempt to convert it into` |
|       - |  129 | ` * a integer and return that.` |
|       - |  130 | ` * If pObj represents a NULL value, return 0.` |
|       - |  131 | ` */` |
|     316 |  132 | `static sxi64 MemObjIntValue(ph7_value *pObj)` |
|       1 |  133 |  |
|       - |  134 | `	sxi32 iFlags;` |
|     317 |  135 | `	iFlags = pObj->iFlags;` |
|     317 |  136 | `	if (iFlags & MEMOBJ_REAL ){` |
|      19 |  137 | `		return MemObjRealToInt(&(*pObj));` |
|     299 |  138 | `	}else if( iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      17 |  139 | `		return pObj->x.iVal;` |
|     283 |  140 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|     251 |  141 | `		return MemObjStringToInt(&(*pObj));` |
|      33 |  142 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|      21 |  143 | `		return 0;` |
|      13 |  144 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|       7 |  145 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|       7 |  146 | `		sxu32 n = pMap->nEntry;` |
|       7 |  147 | `		PH7_HashmapUnref(pMap);` |
|       - |  148 | `		/* Return total number of entries in the hashmap */` |
|       7 |  149 | `		return n;` |
|       7 |  150 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|       - |  151 | `		ph7_value sResult;` |
|       5 |  152 | `		sxi64 iVal = 1;` |
|       - |  153 | `		sxi32 rc;` |
|       - |  154 | `		/* Invoke the [__toInt()] magic method if available [note that this is a symisc extension]  */` |
|       5 |  155 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|       5 |  156 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|       - |  157 | `			"__toInt",sizeof("__toInt")-1,&sResult);` |
|       5 |  158 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_INT) ){` |
|       - |  159 | `			/* Extract method return value */` |
|       5 |  160 | `			iVal = sResult.x.iVal;` |
|       2 |  161 | `		}` |
|       5 |  162 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|       5 |  163 | `		PH7_MemObjRelease(&sResult);` |
|       5 |  164 | `		return iVal;` |
|       3 |  165 | `	}else if(iFlags & MEMOBJ_RES ){` |
|       3 |  166 | `		return pObj->x.pOther != 0;` |
|       - |  167 | `	}` |
|       - |  168 | `	/* CANT HAPPEN */` |
|     ! 0 |  169 | `	return 0;` |
|     159 |  170 |  |
|       - |  171 | `/*` |
|       - |  172 | ` * Return some kind of real value which is the best we can` |
|       - |  173 | ` * do at representing the value that pObj describes as a real.` |
|       - |  174 | ` * If pObj is a real, then the value is exact.If pObj is an` |
|       - |  175 | ` * integer then the integer  is promoted to real and that value` |
|       - |  176 | ` * is returned.` |
|       - |  177 | ` * If pObj is a string, then we make an attempt to convert it` |
|       - |  178 | ` * into a real and return that.` |
|       - |  179 | ` * If pObj represents a NULL value, return 0.0` |
|       - |  180 | ` */` |
|     568 |  181 | `static ph7_real MemObjRealValue(ph7_value *pObj)` |
|       2 |  182 |  |
|       - |  183 | `	sxi32 iFlags;` |
|     570 |  184 | `	iFlags = pObj->iFlags;` |
|     570 |  185 | `	if( iFlags & MEMOBJ_REAL ){` |
|     ! 0 |  186 | `		return pObj->rVal;` |
|     570 |  187 | `	}else if (iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     246 |  188 | `		return (ph7_real)pObj->x.iVal;` |
|     325 |  189 | `	}else if (iFlags & MEMOBJ_STRING){` |
|       - |  190 | `		SyString sString;` |
|       - |  191 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|       - |  192 | `		ph7_real rVal = 0;` |
|       - |  193 | `#else` |
|     317 |  194 | `		ph7_real rVal = 0.0;` |
|       - |  195 | `#endif` |
|     317 |  196 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|     317 |  197 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       - |  198 | `			/* Convert as much as we can */` |
|       - |  199 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|       - |  200 | `			rVal = MemObjStringToInt(&(*pObj));` |
|       - |  201 | `#else` |
|     317 |  202 | `			SyStrToReal(sString.zString,sString.nByte,(void *)&rVal,0);` |
|       - |  203 | `#endif` |
|     158 |  204 | `		}` |
|     317 |  205 | `		return rVal;` |
|       9 |  206 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|       - |  207 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|       - |  208 | `		return 0;` |
|       - |  209 | `#else` |
|     ! 0 |  210 | `		return 0.0;` |
|       - |  211 | `#endif` |
|       9 |  212 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|       - |  213 | `		/* Return the total number of entries in the hashmap */` |
|       3 |  214 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|       3 |  215 | `		ph7_real n = (ph7_real)pMap->nEntry;` |
|       3 |  216 | `		PH7_HashmapUnref(pMap);` |
|       3 |  217 | `		return n;` |
|       7 |  218 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|       - |  219 | `		ph7_value sResult;` |
|       5 |  220 | `		ph7_real rVal = 1;` |
|       - |  221 | `		sxi32 rc;` |
|       - |  222 | `		/* Invoke the [__toFloat()] magic method if available [note that this is a symisc extension]  */` |
|       5 |  223 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|       5 |  224 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|       - |  225 | `			"__toFloat",sizeof("__toFloat")-1,&sResult);` |
|       5 |  226 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_REAL) ){` |
|       - |  227 | `			/* Extract method return value */` |
|       5 |  228 | `			rVal = sResult.rVal;` |
|       2 |  229 | `		}` |
|       5 |  230 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|       5 |  231 | `		PH7_MemObjRelease(&sResult);` |
|       5 |  232 | `		return rVal;` |
|       3 |  233 | `	}else if(iFlags & MEMOBJ_RES ){` |
|       3 |  234 | `		return (ph7_real)(pObj->x.pOther != 0);` |
|       - |  235 | `	}` |
|       - |  236 | `	/* NOT REACHED  */` |
|     ! 0 |  237 | `	return 0;` |
|     286 |  238 |  |
|       - |  239 | `/*` |
|       - |  240 | ` * Return the string representation of a given ph7_value.` |
|       - |  241 | ` * This function never fail and always return SXRET_OK.` |
|       - |  242 | ` */` |
|   58382 |  243 | `static sxi32 MemObjStringValue(SyBlob *pOut,ph7_value *pObj,sxu8 bStrictBool)` |
|       2 |  244 |  |
|   58384 |  245 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|      96 |  246 | `		SyBlobFormat(&(*pOut),"%.15g",pObj->rVal);` |
|   58337 |  247 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|   54110 |  248 | `		SyBlobFormat(&(*pOut),"%qd",pObj->x.iVal);` |
|       - |  249 | `		/* %qd (BSD quad) is equivalent to %lld in the libc printf */` |
|   31236 |  250 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|     134 |  251 | `		if( pObj->x.iVal ){` |
|      70 |  252 | `			SyBlobAppend(&(*pOut),"TRUE",sizeof("TRUE")-1);` |
|      36 |  253 | `		}else{` |
|      66 |  254 | `			if( !bStrictBool ){` |
|      58 |  255 | `				SyBlobAppend(&(*pOut),"FALSE",sizeof("FALSE")-1);` |
|      28 |  256 | `			}` |
|       2 |  257 | `		}` |
|    4116 |  258 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       5 |  259 | `		SyBlobAppend(&(*pOut),"Array",sizeof("Array")-1);` |
|       5 |  260 | `		PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
|    4048 |  261 | `	}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       - |  262 | `		ph7_value sResult;` |
|       - |  263 | `		sxi32 rc;` |
|       - |  264 | `		/* Invoke the __toString() method if available */` |
|      76 |  265 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|      76 |  266 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|       - |  267 | `			"__toString",sizeof("__toString")-1,&sResult);` |
|      76 |  268 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_STRING) && SyBlobLength(&sResult.sBlob) > 0){` |
|       - |  269 | `			/* Expand method return value */` |
|      70 |  270 | `			SyBlobDup(&sResult.sBlob,pOut);` |
|      36 |  271 | `		}else{` |
|       - |  272 | `			/* Expand "Object" as requested by the PHP language reference manual */` |
|       8 |  273 | `			SyBlobAppend(&(*pOut),"Object",sizeof("Object")-1);` |
|       - |  274 | `		}` |
|      76 |  275 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|      76 |  276 | `		PH7_MemObjRelease(&sResult);` |
|    4009 |  277 | `	}else if(pObj->iFlags & MEMOBJ_RES ){` |
|       3 |  278 | `		SyBlobFormat(&(*pOut),"ResourceID_%#x",pObj->x.pOther);` |
|       1 |  279 | `	}` |
|   58384 |  280 | `	return SXRET_OK;` |
|       2 |  281 |  |
|       - |  282 | `/*` |
|       - |  283 | ` * Return some kind of boolean value which is the best we can do` |
|       - |  284 | ` * at representing the value that pObj describes as a boolean.` |
|       - |  285 | ` * When converting to boolean, the following values are considered FALSE:` |
|       - |  286 | ` * NULL` |
|       - |  287 | ` * the boolean FALSE itself.` |
|       - |  288 | ` * the integer 0 (zero).` |
|       - |  289 | ` * the real 0.0 (zero).` |
|       - |  290 | ` * the empty string,a stream of zero [i.e: "0","00","000",...] and the string` |
|       - |  291 | ` * "false".` |
|       - |  292 | ` * an array with zero elements.` |
|       - |  293 | ` */` |
|    7246 |  294 | `static sxi32 MemObjBooleanValue(ph7_value *pObj)` |
|       2 |  295 |  |
|       - |  296 | `	sxi32 iFlags;` |
|    7248 |  297 | `	iFlags = pObj->iFlags;` |
|    7248 |  298 | `	if (iFlags & MEMOBJ_REAL ){` |
|       - |  299 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|       - |  300 | `		return pObj->rVal ? 1 : 0;` |
|       - |  301 | `#else` |
|     ! 0 |  302 | `		return pObj->rVal != 0.0 ? 1 : 0;` |
|       - |  303 | `#endif` |
|    7248 |  304 | `	}else if( iFlags & MEMOBJ_INT ){` |
|      19 |  305 | `		return pObj->x.iVal ? 1 : 0;` |
|    7230 |  306 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|       - |  307 | `		SyString sString;` |
|      63 |  308 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|      63 |  309 | `		if( sString.nByte == 0 ){` |
|       - |  310 | `			/* Empty string */` |
|      33 |  311 | `			return 0;` |
|      30 |  312 | `		}else if( (sString.nByte == sizeof("true") - 1 && SyStrnicmp(sString.zString,"true",sizeof("true")-1) == 0) \|\|` |
|      31 |  313 | `			(sString.nByte == sizeof("on") - 1 && SyStrnicmp(sString.zString,"on",sizeof("on")-1) == 0) \|\|` |
|      30 |  314 | `			(sString.nByte == sizeof("yes") - 1 && SyStrnicmp(sString.zString,"yes",sizeof("yes")-1) == 0) ){` |
|     ! 0 |  315 | `				return 1;` |
|      31 |  316 | `		}else if( sString.nByte == sizeof("false") - 1 && SyStrnicmp(sString.zString,"false",sizeof("false")-1) == 0 ){` |
|     ! 0 |  317 | `			return 0;` |
|     ! 0 |  318 | `		}else{` |
|       - |  319 | `			const char *zIn,*zEnd;` |
|      31 |  320 | `			zIn = sString.zString;` |
|      31 |  321 | `			zEnd = &zIn[sString.nByte];` |
|      33 |  322 | `			while( zIn < zEnd && zIn[0] == '0' ){` |
|       3 |  323 | `				zIn++;` |
|       1 |  324 | `			}` |
|      31 |  325 | `			return zIn >= zEnd ? 0 : 1;` |
|     ! 0 |  326 | `		}` |
|    7168 |  327 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|    6362 |  328 | `		return 0;` |
|     808 |  329 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|      15 |  330 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      15 |  331 | `		sxu32 n = pMap->nEntry;` |
|      15 |  332 | `		PH7_HashmapUnref(pMap);` |
|      15 |  333 | `		return n > 0 ? TRUE : FALSE;` |
|     794 |  334 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|       - |  335 | `		ph7_value sResult;` |
|       5 |  336 | `		sxi32 iVal = 1;` |
|       - |  337 | `		sxi32 rc;` |
|       - |  338 | `		/* Invoke the __toBool() method if available [note that this is a symisc extension]  */` |
|       5 |  339 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|       5 |  340 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|       - |  341 | `			"__toBool",sizeof("__toBool")-1,&sResult);` |
|       5 |  342 | `		if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL)) ){` |
|       - |  343 | `			/* Extract method return value */` |
|       5 |  344 | `			iVal = (sxi32)(sResult.x.iVal != 0); /* Stupid cc warning -W -Wall -O6 */` |
|       2 |  345 | `		}` |
|       5 |  346 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|       5 |  347 | `		PH7_MemObjRelease(&sResult);` |
|       5 |  348 | `		return iVal;` |
|     790 |  349 | `	}else if(iFlags & MEMOBJ_RES ){` |
|     790 |  350 | `		return pObj->x.pOther != 0;` |
|       - |  351 | `	}` |
|       - |  352 | `	/* NOT REACHED */` |
|     ! 0 |  353 | `	return 0;` |
|    3625 |  354 |  |
|       - |  355 | `/*` |
|       - |  356 | ` * If the ph7_value is of type real,try to make it an integer also.` |
|       - |  357 | ` */` |
|    1060 |  358 | `static sxi32 MemObjTryIntger(ph7_value *pObj)` |
|       2 |  359 |  |
|    1062 |  360 | `	pObj->x.iVal = MemObjRealToInt(&(*pObj));` |
|       - |  361 | `  /* Only mark the value as an integer if` |
|       - |  362 | `  **` |
|       - |  363 | `  **    (1) the round-trip conversion real->int->real is a no-op, and` |
|       - |  364 | `  **    (2) The integer is neither the largest nor the smallest` |
|       - |  365 | `  **        possible integer` |
|       - |  366 | `  **` |
|       - |  367 | `  ** The second and third terms in the following conditional enforces` |
|       - |  368 | `  ** the second condition under the assumption that addition overflow causes` |
|       - |  369 | `  ** values to wrap around.  On x86 hardware, the third term is always` |
|       - |  370 | `  ** true and could be omitted.  But we leave it in because other` |
|       - |  371 | `  ** architectures might behave differently.` |
|       - |  372 | `  */` |
|    1060 |  373 | `	if( pObj->rVal ==(ph7_real)pObj->x.iVal && pObj->x.iVal>SMALLEST_INT64` |
|     566 |  374 | `      && pObj->x.iVal<LARGEST_INT64 ){` |
|     566 |  375 | `		  pObj->iFlags \|= MEMOBJ_INT;` |
|     284 |  376 | `	}` |
|    1062 |  377 | `	return SXRET_OK;` |
|       2 |  378 |  |
|       - |  379 | `/*` |
|       - |  380 | ` * Convert a ph7_value to type integer.Invalidate any prior representations.` |
|       - |  381 | ` */` |
|  214234 |  382 | `PH7_PRIVATE sxi32 PH7_MemObjToInteger(ph7_value *pObj)` |
|       2 |  383 |  |
|  214236 |  384 | `	if( (pObj->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  385 | `		/* Preform the conversion */` |
|     317 |  386 | `		pObj->x.iVal = MemObjIntValue(&(*pObj));` |
|       - |  387 | `		/* Invalidate any prior representations */` |
|     317 |  388 | `		SyBlobRelease(&pObj->sBlob);` |
|     317 |  389 | `		MemObjSetType(pObj,MEMOBJ_INT);` |
|     158 |  390 | `	}` |
|  214236 |  391 | `	return SXRET_OK;` |
|       2 |  392 |  |
|       - |  393 | `/*` |
|       - |  394 | ` * Convert a ph7_value to type real (Try to get an integer representation also).` |
|       - |  395 | ` * Invalidate any prior representations` |
|       - |  396 | ` */` |
|     748 |  397 | `PH7_PRIVATE sxi32 PH7_MemObjToReal(ph7_value *pObj)` |
|       2 |  398 |  |
|     750 |  399 | `	if((pObj->iFlags & MEMOBJ_REAL) == 0 ){` |
|       - |  400 | `		/* Preform the conversion */` |
|     570 |  401 | `		pObj->rVal = MemObjRealValue(&(*pObj));` |
|       - |  402 | `		/* Invalidate any prior representations */` |
|     570 |  403 | `		SyBlobRelease(&pObj->sBlob);` |
|     570 |  404 | `		MemObjSetType(pObj,MEMOBJ_REAL);` |
|       - |  405 | `		/* Try to get an integer representation */` |
|     570 |  406 | `		MemObjTryIntger(&(*pObj));` |
|     284 |  407 | `	}` |
|     750 |  408 | `	return SXRET_OK;` |
|       2 |  409 |  |
|       - |  410 | `/*` |
|       - |  411 | ` * Convert a ph7_value to type boolean.Invalidate any prior representations.` |
|       - |  412 | ` */` |
|    7364 |  413 | `PH7_PRIVATE sxi32 PH7_MemObjToBool(ph7_value *pObj)` |
|       2 |  414 |  |
|    7366 |  415 | `	if( (pObj->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       - |  416 | `		/* Preform the conversion */` |
|    7248 |  417 | `		pObj->x.iVal = MemObjBooleanValue(&(*pObj));` |
|       - |  418 | `		/* Invalidate any prior representations */` |
|    7248 |  419 | `		SyBlobRelease(&pObj->sBlob);` |
|    7248 |  420 | `		MemObjSetType(pObj,MEMOBJ_BOOL);` |
|    3623 |  421 | `	}` |
|    7366 |  422 | `	return SXRET_OK;` |
|       2 |  423 |  |
|       - |  424 | `/*` |
|       - |  425 | ` * Convert a ph7_value to type string.Prior representations are NOT invalidated.` |
|       - |  426 | ` */` |
|  454610 |  427 | `PH7_PRIVATE sxi32 PH7_MemObjToString(ph7_value *pObj)` |
|       2 |  428 |  |
|  454612 |  429 | `	sxi32 rc = SXRET_OK;` |
|  454612 |  430 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  431 | `		/* Perform the conversion */` |
|   58140 |  432 | `		SyBlobReset(&pObj->sBlob); /* Reset the internal buffer */` |
|   58140 |  433 | `		rc = MemObjStringValue(&pObj->sBlob,&(*pObj),TRUE);` |
|   58140 |  434 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|   29069 |  435 | `	}` |
|  454612 |  436 | `	return rc;` |
|       2 |  437 |  |
|       - |  438 | `/*` |
|       - |  439 | ` * Nullify a ph7_value.In other words invalidate any prior` |
|       - |  440 | ` * representation.` |
|       - |  441 | ` */` |
|     ! 0 |  442 | `PH7_PRIVATE sxi32 PH7_MemObjToNull(ph7_value *pObj)` |
|     ! 0 |  443 |  |
|     ! 0 |  444 | `	return PH7_MemObjRelease(pObj);` |
|     ! 0 |  445 |  |
|       - |  446 | `/*` |
|       - |  447 | ` * Convert a ph7_value to type array.Invalidate any prior representations.` |
|       - |  448 | `  * According to the PHP language reference manual.` |
|       - |  449 | `  *   For any of the types: integer, float, string, boolean converting a value` |
|       - |  450 | `  *   to an array results in an array with a single element with index zero` |
|       - |  451 | `  *   and the value of the scalar which was converted.` |
|       - |  452 | `  */` |
|      20 |  453 | `PH7_PRIVATE sxi32 PH7_MemObjToHashmap(ph7_value *pObj)` |
|       1 |  454 |  |
|      21 |  455 | `	if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       - |  456 | `		ph7_hashmap *pMap;` |
|       - |  457 | `		/* Allocate a new hashmap instance */` |
|      21 |  458 | `		pMap = PH7_NewHashmap(pObj->pVm,0,0);` |
|      21 |  459 | `		if( pMap == 0 ){` |
|     ! 0 |  460 | `			return SXERR_MEM;` |
|       - |  461 | `		}` |
|      21 |  462 | `		if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|       - |  463 | `			/*` |
|       - |  464 | `			 * According to the PHP language reference manual.` |
|       - |  465 | `			 *   For any of the types: integer, float, string, boolean converting a value` |
|       - |  466 | `			 *   to an array results in an array with a single element with index zero` |
|       - |  467 | `			 *   and the value of the scalar which was converted.` |
|       - |  468 | `			 */` |
|      17 |  469 | `			if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       - |  470 | `				/* Object cast */` |
|       7 |  471 | `				PH7_ClassInstanceToHashmap((ph7_class_instance *)pObj->x.pOther,pMap);` |
|       4 |  472 | `			}else{` |
|       - |  473 | `				/* Insert a single element */` |
|      11 |  474 | `				PH7_HashmapInsert(pMap,0/* Automatic index assign */,&(*pObj));` |
|       - |  475 | `			}` |
|      17 |  476 | `			SyBlobRelease(&pObj->sBlob);` |
|       8 |  477 | `		}` |
|       - |  478 | `		/* Invalidate any prior representation */` |
|      21 |  479 | `		PH7_MemObjRelease(pObj);` |
|      21 |  480 | `		MemObjSetType(pObj,MEMOBJ_HASHMAP);` |
|      21 |  481 | `		pObj->x.pOther = pMap;` |
|      10 |  482 | `	}` |
|      21 |  483 | `	return SXRET_OK;` |
|      11 |  484 |  |
|       - |  485 | `/*` |
|       - |  486 | ` * Convert a ph7_value to type object.Invalidate any prior representations.` |
|       - |  487 | ` * The new object is instantiated from the builtin stdClass().` |
|       - |  488 | ` * The stdClass() class have a single attribute which is '$value'. This attribute` |
|       - |  489 | ` * hold a copy of the converted ph7_value.` |
|       - |  490 | ` * The internal of the stdClass is as follows:` |
|       - |  491 | ` * class stdClass{` |
|       - |  492 | ` *	 public $value;` |
|       - |  493 | ` *	 public function __toInt(){ return (int)$this->value; }` |
|       - |  494 | ` *	 public function __toBool(){ return (bool)$this->value; }` |
|       - |  495 | ` *	 public function __toFloat(){ return (float)$this->value; }` |
|       - |  496 | ` *	 public function __toString(){ return (string)$this->value; }` |
|       - |  497 | ` *	 function __construct($v){ $this->value = $v; }"` |
|       - |  498 | ` *  }` |
|       - |  499 | ` * Refer to the official documentation for more information.` |
|       - |  500 | ` */` |
|      16 |  501 | `PH7_PRIVATE sxi32 PH7_MemObjToObject(ph7_value *pObj)` |
|       1 |  502 |  |
|      17 |  503 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|       - |  504 | `		ph7_class_instance *pStd;` |
|       - |  505 | `		ph7_class_method *pCons;` |
|       - |  506 | `		ph7_class *pClass;` |
|       - |  507 | `		ph7_vm *pVm;` |
|       - |  508 | `		/* Point to the underlying VM */` |
|      17 |  509 | `		pVm = pObj->pVm;` |
|       - |  510 | `		/* Point to the stdClass() */` |
|      17 |  511 | `		pClass = PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,0,0);` |
|      17 |  512 | `		if( pClass == 0 ){` |
|       - |  513 | `			/* Can't happen,load null instead */` |
|     ! 0 |  514 | `			PH7_MemObjRelease(pObj);` |
|     ! 0 |  515 | `			return SXRET_OK;` |
|       - |  516 | `		}` |
|       - |  517 | `		/* Instanciate a new stdClass() object */` |
|      17 |  518 | `		pStd = PH7_NewClassInstance(pVm,pClass);` |
|      17 |  519 | `		if( pStd == 0 ){` |
|       - |  520 | `			/* Out of memory */` |
|     ! 0 |  521 | `			PH7_MemObjRelease(pObj);` |
|     ! 0 |  522 | `			return SXRET_OK;` |
|       - |  523 | `		}` |
|       - |  524 | `		/* Check if a constructor is available */` |
|      17 |  525 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      17 |  526 | `		if( pCons ){` |
|       - |  527 | `			ph7_value *apArg[2];` |
|       - |  528 | `			/* Invoke the constructor with one argument */` |
|      17 |  529 | `			apArg[0] = pObj;` |
|      17 |  530 | `			PH7_VmCallClassMethod(pVm,pStd,pCons,0,1,apArg);` |
|      17 |  531 | `			if( pStd->iRef < 1 ){` |
|     ! 0 |  532 | `				pStd->iRef = 1;` |
|     ! 0 |  533 | `			}` |
|       8 |  534 | `		}` |
|       - |  535 | `		/* Invalidate any prior representation */` |
|      17 |  536 | `		PH7_MemObjRelease(pObj);` |
|       - |  537 | `		/* Save the new instance */` |
|      17 |  538 | `		pObj->x.pOther = pStd;` |
|      17 |  539 | `		MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       8 |  540 | `	}` |
|      17 |  541 | `	return SXRET_OK;` |
|       9 |  542 |  |
|       - |  543 | `/*` |
|       - |  544 | ` * Return a pointer to the appropriate convertion method associated` |
|       - |  545 | ` * with the given type.` |
|       - |  546 | ` * Note on type juggling.` |
|       - |  547 | ` * Accoding to the PHP language reference manual` |
|       - |  548 | ` *  PHP does not require (or support) explicit type definition in variable` |
|       - |  549 | ` *  declaration; a variable's type is determined by the context in which` |
|       - |  550 | ` *  the variable is used. That is to say, if a string value is assigned` |
|       - |  551 | ` *  to variable $var, $var becomes a string. If an integer value is then` |
|       - |  552 | ` *  assigned to $var, it becomes an integer.` |
|       - |  553 | ` */` |
|     ! 0 |  554 | `PH7_PRIVATE ProcMemObjCast PH7_MemObjCastMethod(sxi32 iFlags)` |
|     ! 0 |  555 |  |
|     ! 0 |  556 | `	if( iFlags & MEMOBJ_STRING ){` |
|     ! 0 |  557 | `		return PH7_MemObjToString;` |
|     ! 0 |  558 | `	}else if( iFlags & MEMOBJ_INT ){` |
|     ! 0 |  559 | `		return PH7_MemObjToInteger;` |
|     ! 0 |  560 | `	}else if( iFlags & MEMOBJ_REAL ){` |
|     ! 0 |  561 | `		return PH7_MemObjToReal;` |
|     ! 0 |  562 | `	}else if( iFlags & MEMOBJ_BOOL ){` |
|     ! 0 |  563 | `		return PH7_MemObjToBool;` |
|     ! 0 |  564 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|     ! 0 |  565 | `		return PH7_MemObjToHashmap;` |
|     ! 0 |  566 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|     ! 0 |  567 | `		return PH7_MemObjToObject;` |
|       - |  568 | `	}` |
|       - |  569 | `	/* NULL cast */` |
|     ! 0 |  570 | `	return PH7_MemObjToNull;` |
|     ! 0 |  571 |  |
|       - |  572 | `/*` |
|       - |  573 | ` * Check whether the ph7_value is numeric [i.e: int/float/bool] or looks` |
|       - |  574 | ` * like a numeric number [i.e: if the ph7_value is of type string.].` |
|       - |  575 | ` * Return TRUE if numeric.FALSE otherwise.` |
|       - |  576 | ` */` |
|  228280 |  577 | `PH7_PRIVATE sxi32 PH7_MemObjIsNumeric(ph7_value *pObj)` |
|       2 |  578 |  |
|  228282 |  579 | `	if( pObj->iFlags & ( MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      30 |  580 | `		return TRUE;` |
|  228254 |  581 | `	}else if( pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|       3 |  582 | `		return FALSE;` |
|  228252 |  583 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       - |  584 | `		SyString sStr;` |
|       - |  585 | `		sxi32 rc;` |
|  228252 |  586 | `		SyStringInitFromBuf(&sStr,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|  228252 |  587 | `		if( sStr.nByte <= 0 ){` |
|       - |  588 | `			/* Empty string */` |
|     ! 0 |  589 | `			return FALSE;` |
|       - |  590 | `		}` |
|       - |  591 | `		/* Check if the string representation looks like a numeric number */` |
|  228252 |  592 | `		rc = SyStrIsNumeric(sStr.zString,sStr.nByte,0,0);` |
|  228252 |  593 | `		return rc == SXRET_OK ? TRUE : FALSE;` |
|       - |  594 | `	}` |
|       - |  595 | `	/* NOT REACHED */` |
|     ! 0 |  596 | `	return FALSE;` |
|  114211 |  597 |  |
|       - |  598 | `/*` |
|       - |  599 | ` * Check whether the ph7_value is empty.Return TRUE if empty.` |
|       - |  600 | ` * FALSE otherwise.` |
|       - |  601 | ` * An ph7_value is considered empty if the following are true:` |
|       - |  602 | ` * NULL value.` |
|       - |  603 | ` * Boolean FALSE.` |
|       - |  604 | ` * Integer/Float with a 0 (zero) value.` |
|       - |  605 | ` * An empty string or a stream of 0 (zero) [i.e: "0","00","000",...].` |
|       - |  606 | ` * An empty array.` |
|       - |  607 | ` * NOTE` |
|       - |  608 | ` *  OBJECT VALUE MUST NOT BE MODIFIED.` |
|       - |  609 | ` */` |
|   15300 |  610 | `PH7_PRIVATE sxi32 PH7_MemObjIsEmpty(ph7_value *pObj)` |
|       2 |  611 |  |
|   15302 |  612 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|   11780 |  613 | `		return TRUE;` |
|    3524 |  614 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|     ! 0 |  615 | `		return pObj->x.iVal == 0 ? TRUE : FALSE;` |
|    3524 |  616 | `	}else if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 |  617 | `		return pObj->rVal == (ph7_real)0 ? TRUE : FALSE;` |
|    3524 |  618 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|     ! 0 |  619 | `		return !pObj->x.iVal;` |
|    3524 |  620 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|    3504 |  621 | `		if( SyBlobLength(&pObj->sBlob) <= 0 ){` |
|    1622 |  622 | `			return TRUE;` |
|     ! 0 |  623 | `		}else{` |
|       - |  624 | `			const char *zIn,*zEnd;` |
|    1884 |  625 | `			zIn = (const char *)SyBlobData(&pObj->sBlob);` |
|    1884 |  626 | `			zEnd = &zIn[SyBlobLength(&pObj->sBlob)];` |
|    1884 |  627 | `			while( zIn < zEnd ){` |
|    1884 |  628 | `				if( zIn[0] != '0' ){` |
|    1884 |  629 | `					break;` |
|       - |  630 | `				}` |
|     ! 0 |  631 | `				zIn++;` |
|     ! 0 |  632 | `			}` |
|    1884 |  633 | `			return zIn >= zEnd ? TRUE : FALSE;` |
|     ! 0 |  634 | `		}` |
|      22 |  635 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|      22 |  636 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      22 |  637 | `		return pMap->nEntry == 0 ? TRUE : FALSE;` |
|     ! 0 |  638 | `	}else if ( pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|     ! 0 |  639 | `		return FALSE;` |
|       - |  640 | `	}` |
|       - |  641 | `	/* Assume empty by default */` |
|     ! 0 |  642 | `	return TRUE;` |
|    7652 |  643 |  |
|       - |  644 | `/*` |
|       - |  645 | ` * Convert a ph7_value so that it has types MEMOBJ_REAL or MEMOBJ_INT` |
|       - |  646 | ` * or both.` |
|       - |  647 | ` * Invalidate any prior representations. Every effort is made to force` |
|       - |  648 | ` * the conversion, even if the input is a string that does not look` |
|       - |  649 | ` * completely like a number.Convert as much of the string as we can` |
|       - |  650 | ` * and ignore the rest.` |
|       - |  651 | ` */` |
|  226874 |  652 | `PH7_PRIVATE sxi32 PH7_MemObjToNumeric(ph7_value *pObj)` |
|       2 |  653 |  |
|  226876 |  654 | `	if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|  226844 |  655 | `		if( pObj->iFlags & (MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|       3 |  656 | `			if( pObj->iFlags & MEMOBJ_NULL ){` |
|     ! 0 |  657 | `				pObj->x.iVal = 0;` |
|     ! 0 |  658 | `			}` |
|       3 |  659 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|       1 |  660 | `		}` |
|       - |  661 | `		/* Already numeric */` |
|  226844 |  662 | `		return  SXRET_OK;` |
|       - |  663 | `	}` |
|      33 |  664 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|      33 |  665 | `		sxi32 rc = SXERR_INVALID;` |
|      33 |  666 | `		sxu8 bReal = FALSE;` |
|       - |  667 | `		SyString sString;` |
|      33 |  668 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|       - |  669 | `		/* Check if the given string looks like a numeric number */` |
|      33 |  670 | `		if( sString.nByte > 0 ){` |
|      33 |  671 | `			rc = SyStrIsNumeric(sString.zString,sString.nByte,&bReal,0);` |
|      16 |  672 | `		}` |
|      33 |  673 | `		if( bReal ){` |
|       3 |  674 | `			PH7_MemObjToReal(&(*pObj));` |
|       2 |  675 | `		}else{` |
|      31 |  676 | `			if( rc != SXRET_OK ){` |
|       - |  677 | `				/* The input does not look at all like a number,set the value to 0 */` |
|     ! 0 |  678 | `				pObj->x.iVal = 0;` |
|     ! 0 |  679 | `			}else{` |
|       - |  680 | `				/* Convert as much as we can */` |
|      31 |  681 | `				pObj->x.iVal = MemObjStringToInt(&(*pObj));` |
|       - |  682 | `			}` |
|      31 |  683 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|      31 |  684 | `			SyBlobRelease(&pObj->sBlob);` |
|       1 |  685 | `		}` |
|      16 |  686 | `	}else if(pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)){` |
|     ! 0 |  687 | `		PH7_MemObjToInteger(pObj);` |
|     ! 0 |  688 | `	}else{` |
|       - |  689 | `		/* Perform a blind cast */` |
|     ! 0 |  690 | `		PH7_MemObjToReal(&(*pObj));` |
|       - |  691 | `	}` |
|      33 |  692 | `	return SXRET_OK;` |
|  113461 |  693 |  |
|       - |  694 | `/*` |
|       - |  695 | ` * Try a get an integer representation of the given ph7_value.` |
|       - |  696 | ` * If the ph7_value is not of type real,this function is a no-op.` |
|       - |  697 | ` */` |
|     468 |  698 | `PH7_PRIVATE sxi32 PH7_MemObjTryInteger(ph7_value *pObj)` |
|       1 |  699 |  |
|     469 |  700 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|       - |  701 | `		/* Work only with reals */` |
|     469 |  702 | `		MemObjTryIntger(&(*pObj));` |
|     234 |  703 | `	}` |
|     469 |  704 | `	return SXRET_OK;` |
|       1 |  705 |  |
|       - |  706 | `/*` |
|       - |  707 | ` * Initialize a ph7_value to the null type.` |
|       - |  708 | ` */` |
| 2375830 |  709 | `PH7_PRIVATE sxi32 PH7_MemObjInit(ph7_vm *pVm,ph7_value *pObj)` |
|       2 |  710 |  |
|       - |  711 | `	/* Zero the structure */` |
| 2375832 |  712 | `	SyZero(pObj,sizeof(ph7_value));` |
|       - |  713 | `	/* Initialize fields */` |
| 2375832 |  714 | `	pObj->pVm = pVm;` |
| 2375832 |  715 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|       - |  716 | `	/* Set the NULL type */` |
| 2375832 |  717 | `	pObj->iFlags = MEMOBJ_NULL;` |
| 2375832 |  718 | `	return SXRET_OK;` |
|       2 |  719 |  |
|       - |  720 | `/*` |
|       - |  721 | ` * Initialize a ph7_value to the integer type.` |
|       - |  722 | ` */` |
|   63596 |  723 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromInt(ph7_vm *pVm,ph7_value *pObj,sxi64 iVal)` |
|       2 |  724 |  |
|       - |  725 | `	/* Zero the structure */` |
|   63598 |  726 | `	SyZero(pObj,sizeof(ph7_value));` |
|       - |  727 | `	/* Initialize fields */` |
|   63598 |  728 | `	pObj->pVm = pVm;` |
|   63598 |  729 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|       - |  730 | `	/* Set the desired type */` |
|   63598 |  731 | `	pObj->x.iVal = iVal;` |
|   63598 |  732 | `	pObj->iFlags = MEMOBJ_INT;` |
|   63598 |  733 | `	return SXRET_OK;` |
|       2 |  734 |  |
|       - |  735 | `/*` |
|       - |  736 | ` * Initialize a ph7_value to the boolean type.` |
|       - |  737 | ` */` |
|    8834 |  738 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromBool(ph7_vm *pVm,ph7_value *pObj,sxi32 iVal)` |
|       2 |  739 |  |
|       - |  740 | `	/* Zero the structure */` |
|    8836 |  741 | `	SyZero(pObj,sizeof(ph7_value));` |
|       - |  742 | `	/* Initialize fields */` |
|    8836 |  743 | `	pObj->pVm = pVm;` |
|    8836 |  744 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|       - |  745 | `	/* Set the desired type */` |
|    8836 |  746 | `	pObj->x.iVal = iVal ? 1 : 0;` |
|    8836 |  747 | `	pObj->iFlags = MEMOBJ_BOOL;` |
|    8836 |  748 | `	return SXRET_OK;` |
|       2 |  749 |  |
|       - |  750 | `#if 0` |
|       - |  751 | `/*` |
|       - |  752 | ` * Initialize a ph7_value to the real type.` |
|       - |  753 | ` */` |
|       - |  754 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromReal(ph7_vm *pVm,ph7_value *pObj,ph7_real rVal)` |
|       - |  755 |  |
|       - |  756 | `	/* Zero the structure */` |
|       - |  757 | `	SyZero(pObj,sizeof(ph7_value));` |
|       - |  758 | `	/* Initialize fields */` |
|       - |  759 | `	pObj->pVm = pVm;` |
|       - |  760 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|       - |  761 | `	/* Set the desired type */` |
|       - |  762 | `	pObj->rVal = rVal;` |
|       - |  763 | `	pObj->iFlags = MEMOBJ_REAL;` |
|       - |  764 | `	return SXRET_OK;` |
|       - |  765 |  |
|       - |  766 | `#endif` |
|       - |  767 | `/*` |
|       - |  768 | ` * Initialize a ph7_value to the array type.` |
|       - |  769 | ` */` |
|   14634 |  770 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromArray(ph7_vm *pVm,ph7_value *pObj,ph7_hashmap *pArray)` |
|       2 |  771 |  |
|       - |  772 | `	/* Zero the structure */` |
|   14636 |  773 | `	SyZero(pObj,sizeof(ph7_value));` |
|       - |  774 | `	/* Initialize fields */` |
|   14636 |  775 | `	pObj->pVm = pVm;` |
|   14636 |  776 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|       - |  777 | `	/* Set the desired type */` |
|   14636 |  778 | `	pObj->iFlags = MEMOBJ_HASHMAP;` |
|   14636 |  779 | `	pObj->x.pOther = pArray;` |
|   14636 |  780 | `	return SXRET_OK;` |
|       2 |  781 |  |
|       - |  782 | `/*` |
|       - |  783 | ` * Initialize a ph7_value to the string type.` |
|       - |  784 | ` */` |
|  119860 |  785 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromString(ph7_vm *pVm,ph7_value *pObj,const SyString *pVal)` |
|       2 |  786 |  |
|       - |  787 | `	/* Zero the structure */` |
|  119862 |  788 | `	SyZero(pObj,sizeof(ph7_value));` |
|       - |  789 | `	/* Initialize fields */` |
|  119862 |  790 | `	pObj->pVm = pVm;` |
|  119862 |  791 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|  119862 |  792 | `	if( pVal ){` |
|       - |  793 | `		/* Append contents */` |
|   75852 |  794 | `		SyBlobAppend(&pObj->sBlob,(const void *)pVal->zString,pVal->nByte);` |
|   37925 |  795 | `	}` |
|       - |  796 | `	/* Set the desired type */` |
|  119862 |  797 | `	pObj->iFlags = MEMOBJ_STRING;` |
|  119862 |  798 | `	return SXRET_OK;` |
|       2 |  799 |  |
|       - |  800 | `/*` |
|       - |  801 | ` * Append some contents to the internal buffer of a given ph7_value.` |
|       - |  802 | ` * If the given ph7_value is not of type string,this function` |
|       - |  803 | ` * invalidate any prior representation and set the string type.` |
|       - |  804 | ` * Then a simple append operation is performed.` |
|       - |  805 | ` */` |
|  163068 |  806 | `PH7_PRIVATE sxi32 PH7_MemObjStringAppend(ph7_value *pObj,const char *zData,sxu32 nLen)` |
|       2 |  807 |  |
|       - |  808 | `	sxi32 rc;` |
|  163070 |  809 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  810 | `		/* Invalidate any prior representation */` |
|       5 |  811 | `		PH7_MemObjRelease(pObj);` |
|       5 |  812 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|       2 |  813 | `	}` |
|       - |  814 | `	/* Append contents */` |
|  163070 |  815 | `	rc = SyBlobAppend(&pObj->sBlob,zData,nLen);` |
|  163070 |  816 | `	return rc;` |
|       2 |  817 |  |
|       - |  818 | `#if 0` |
|       - |  819 | `/*` |
|       - |  820 | ` * Format and append some contents to the internal buffer of a given ph7_value.` |
|       - |  821 | ` * If the given ph7_value is not of type string,this function invalidate` |
|       - |  822 | ` * any prior representation and set the string type.` |
|       - |  823 | ` * Then a simple format and append operation is performed.` |
|       - |  824 | ` */` |
|       - |  825 | `PH7_PRIVATE sxi32 PH7_MemObjStringFormat(ph7_value *pObj,const char *zFormat,va_list ap)` |
|       - |  826 |  |
|       - |  827 | `	sxi32 rc;` |
|       - |  828 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  829 | `		/* Invalidate any prior representation */` |
|       - |  830 | `		PH7_MemObjRelease(pObj);` |
|       - |  831 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|       - |  832 | `	}` |
|       - |  833 | `	/* Format and append contents */` |
|       - |  834 | `	rc = SyBlobFormatAp(&pObj->sBlob,zFormat,ap);` |
|       - |  835 | `	return rc;` |
|       - |  836 |  |
|       - |  837 | `#endif` |
|       - |  838 | `/*` |
|       - |  839 | ` * Duplicate the contents of a ph7_value.` |
|       - |  840 | ` */` |
| 1263198 |  841 | `PH7_PRIVATE sxi32 PH7_MemObjStore(ph7_value *pSrc,ph7_value *pDest)` |
|       2 |  842 |  |
| 1263200 |  843 | `	ph7_class_instance *pObj = 0;` |
| 1263200 |  844 | `	ph7_hashmap *pMap = 0;` |
|       - |  845 | `	sxi32 rc;` |
| 1263200 |  846 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|       - |  847 | `		/* Increment reference count */` |
|   79320 |  848 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
| 1223541 |  849 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|       - |  850 | `		/* Increment reference count */` |
|    1116 |  851 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|     557 |  852 | `	}` |
| 1263200 |  853 | `	if( pDest->iFlags & MEMOBJ_HASHMAP ){` |
|   23396 |  854 | `		pMap = (ph7_hashmap *)pDest->x.pOther;` |
| 1251503 |  855 | `	}else if( pDest->iFlags & MEMOBJ_OBJ ){` |
|     732 |  856 | `		pObj = (ph7_class_instance *)pDest->x.pOther;` |
|     365 |  857 | `	}` |
| 1263200 |  858 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
| 1263200 |  859 | `	pDest->iFlags &= ~MEMOBJ_AUX;` |
| 1263200 |  860 | `	rc = SXRET_OK;` |
| 1263200 |  861 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  877948 |  862 | `		SyBlobReset(&pDest->sBlob);` |
|  877948 |  863 | `		rc = SyBlobDup(&pSrc->sBlob,&pDest->sBlob);` |
|  438975 |  864 | `	}else{` |
|  385254 |  865 | `		if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|  148772 |  866 | `			SyBlobRelease(&pDest->sBlob);` |
|   74407 |  867 | `		}` |
|       - |  868 | `	}` |
| 1263200 |  869 | `	if( pMap ){` |
|   23396 |  870 | `		PH7_HashmapUnref(pMap);` |
| 1251503 |  871 | `	}else if( pObj ){` |
|     732 |  872 | `		PH7_ClassInstanceUnref(pObj);` |
|     365 |  873 | `	}` |
| 1263200 |  874 | `	return rc;` |
|       2 |  875 |  |
|       - |  876 | `/*` |
|       - |  877 | ` * Duplicate the contents of a ph7_value but do not copy internal` |
|       - |  878 | ` * buffer contents,simply point to it.` |
|       - |  879 | ` */` |
| 3220762 |  880 | `PH7_PRIVATE sxi32 PH7_MemObjLoad(ph7_value *pSrc,ph7_value *pDest)` |
|       2 |  881 |  |
| 3220764 |  882 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),` |
|       - |  883 | `		sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
| 3220764 |  884 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|       - |  885 | `		/* Increment reference count */` |
|  251994 |  886 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
| 3094768 |  887 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|       - |  888 | `		/* Increment reference count */` |
|    2092 |  889 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|    1045 |  890 | `	}` |
| 3220764 |  891 | `	if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|      25 |  892 | `		SyBlobRelease(&pDest->sBlob);` |
|      12 |  893 | `	}` |
| 3220764 |  894 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
| 1726712 |  895 | `		SyBlobReadOnly(&pDest->sBlob,SyBlobData(&pSrc->sBlob),SyBlobLength(&pSrc->sBlob));` |
|  863468 |  896 | `	}` |
| 3220764 |  897 | `	return SXRET_OK;` |
|       2 |  898 |  |
|       - |  899 | `/*` |
|       - |  900 | ` * Invalidate any prior representation of a given ph7_value.` |
|       - |  901 | ` */` |
| 5175086 |  902 | `PH7_PRIVATE sxi32 PH7_MemObjRelease(ph7_value *pObj)` |
|       2 |  903 |  |
| 5175088 |  904 | `	if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
| 4659780 |  905 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|  306906 |  906 | `			PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
| 4506328 |  907 | `		}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|    3204 |  908 | `			PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|    1601 |  909 | `		}` |
|       - |  910 | `		/* Release the internal buffer */` |
| 4659780 |  911 | `		SyBlobRelease(&pObj->sBlob);` |
|       - |  912 | `		/* Invalidate any prior representation */` |
| 4659780 |  913 | `		pObj->iFlags = MEMOBJ_NULL;` |
| 2330136 |  914 | `	}` |
| 5175088 |  915 | `	return SXRET_OK;` |
|       2 |  916 |  |
|       - |  917 | `/*` |
|       - |  918 | ` * Compare two ph7_values.` |
|       - |  919 | ` * Return 0 if the values are equals, > 0 if pObj1 is greater than pObj2` |
|       - |  920 | ` * or < 0 if pObj2 is greater than pObj1.` |
|       - |  921 | ` * Type comparison table taken from the PHP language reference manual.` |
|       - |  922 | ` * Comparisons of $x with PHP functions Expression` |
|       - |  923 | ` *              gettype() 	empty() 	is_null() 	isset() 	boolean : if($x)` |
|       - |  924 | ` * $x = ""; 	string 	    TRUE 	FALSE 	TRUE 	FALSE` |
|       - |  925 | ` * $x = null 	NULL 	    TRUE 	TRUE 	FALSE 	FALSE` |
|       - |  926 | ` * var $x; 	    NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|       - |  927 | ` * $x is undefined 	NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|       - |  928 | ` *  $x = array(); 	array 	TRUE 	FALSE 	TRUE 	FALSE` |
|       - |  929 | ` * $x = false; 	boolean 	TRUE 	FALSE 	TRUE 	FALSE` |
|       - |  930 | ` * $x = true; 	boolean 	FALSE 	FALSE 	TRUE 	TRUE` |
|       - |  931 | ` * $x = 1; 	    integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|       - |  932 | ` * $x = 42; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|       - |  933 | ` * $x = 0; 	    integer 	TRUE 	FALSE 	TRUE 	FALSE` |
|       - |  934 | ` * $x = -1; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|       - |  935 | ` * $x = "1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|       - |  936 | ` * $x = "0"; 	string 	TRUE 	FALSE 	TRUE 	FALSE` |
|       - |  937 | ` * $x = "-1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|       - |  938 | ` * $x = "php"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|       - |  939 | ` * $x = "true"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|       - |  940 | ` * $x = "false"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|       - |  941 | ` *      Loose comparisons with ==` |
|       - |  942 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|       - |  943 | ` * TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE` |
|       - |  944 | ` * FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|       - |  945 | ` * 1 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  946 | ` * 0 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE` |
|       - |  947 | ` * -1 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  948 | ` * "1" 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  949 | ` * "0" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  950 | ` * "-1" 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  951 | ` * NULL 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|       - |  952 | ` * array() 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	FALSE` |
|       - |  953 | ` * "php" 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|       - |  954 | ` * "" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE` |
|       - |  955 | ` *    Strict comparisons with ===` |
|       - |  956 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|       - |  957 | ` * TRUE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  958 | ` * FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  959 | ` * 1 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  960 | ` * 0 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  961 | ` * -1 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  962 | ` * "1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  963 | ` * "0" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  964 | ` * "-1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|       - |  965 | ` * NULL 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE` |
|       - |  966 | ` * array() 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE` |
|       - |  967 | ` * "php" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|       - |  968 | ` * "" 	    FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE` |
|       - |  969 | ` */` |
|  595097 |  970 | `PH7_PRIVATE sxi32 PH7_MemObjCmp(ph7_value *pObj1,ph7_value *pObj2,int bStrict,int iNest)` |
|       2 |  971 |  |
|       - |  972 | `	sxi32 iComb;` |
|       - |  973 | `	sxi32 rc;` |
|  595099 |  974 | `	if( bStrict ){` |
|       - |  975 | `		sxi32 iF1,iF2;` |
|       - |  976 | `		/* Strict comparisons with === */` |
|  293932 |  977 | `		iF1 = pObj1->iFlags&~MEMOBJ_AUX;` |
|  293932 |  978 | `		iF2 = pObj2->iFlags&~MEMOBJ_AUX;` |
|  293932 |  979 | `		if( iF1 != iF2 ){` |
|       - |  980 | `			/* Not of the same type */` |
|   89846 |  981 | `			return 1;` |
|       - |  982 | `		}` |
|  102043 |  983 | `	}` |
|       - |  984 | `	/* Combine flag together */` |
|  505255 |  985 | `	iComb = pObj1->iFlags\|pObj2->iFlags;` |
|  505255 |  986 | `	if( iComb & (MEMOBJ_NULL\|MEMOBJ_RES\|MEMOBJ_BOOL) ){` |
|       - |  987 | `		/* Convert to boolean: Keep in mind FALSE < TRUE */` |
|   10934 |  988 | `		if( (pObj1->iFlags & MEMOBJ_BOOL) == 0 ){` |
|    3942 |  989 | `			PH7_MemObjToBool(pObj1);` |
|    1970 |  990 | `		}` |
|   10934 |  991 | `		if( (pObj2->iFlags & MEMOBJ_BOOL) == 0 ){` |
|    3200 |  992 | `			PH7_MemObjToBool(pObj2);` |
|    1599 |  993 | `		}` |
|   10934 |  994 | `		return (sxi32)((pObj1->x.iVal != 0) - (pObj2->x.iVal != 0));` |
|  494323 |  995 | `	}else if ( iComb & MEMOBJ_HASHMAP ){` |
|       - |  996 | `		/* Hashmap aka 'array' comparison */` |
|       7 |  997 | `		if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       - |  998 | `			/* Array is always greater */` |
|     ! 0 |  999 | `			return -1;` |
|       - | 1000 | `		}` |
|       7 | 1001 | `		if( (pObj2->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       - | 1002 | `			/* Array is always greater */` |
|     ! 0 | 1003 | `			return 1;` |
|       - | 1004 | `		}` |
|       - | 1005 | `		/* Perform the comparison */` |
|       7 | 1006 | `		rc = PH7_HashmapCmp((ph7_hashmap *)pObj1->x.pOther,(ph7_hashmap *)pObj2->x.pOther,bStrict);` |
|       7 | 1007 | `		return rc;` |
|  494317 | 1008 | `	}else if(iComb & MEMOBJ_OBJ ){` |
|       - | 1009 | `		/* Object comparison */` |
|     162 | 1010 | `		if( (pObj1->iFlags & MEMOBJ_OBJ) == 0 ){` |
|       - | 1011 | `			/* Object is always greater */` |
|     ! 0 | 1012 | `			return -1;` |
|       - | 1013 | `		}` |
|     162 | 1014 | `		if( (pObj2->iFlags & MEMOBJ_OBJ) == 0 ){` |
|       - | 1015 | `			/* Object is always greater */` |
|     ! 0 | 1016 | `			return 1;` |
|       - | 1017 | `		}` |
|       - | 1018 | `		/* Perform the comparison */` |
|     162 | 1019 | `		rc = PH7_ClassInstanceCmp((ph7_class_instance *)pObj1->x.pOther,(ph7_class_instance *)pObj2->x.pOther,bStrict,iNest);` |
|     162 | 1020 | `		return rc;` |
|  494157 | 1021 | `	}else if ( iComb & MEMOBJ_STRING ){` |
|       - | 1022 | `		SyString s1,s2;` |
|  307691 | 1023 | `		if( !bStrict ){` |
|       - | 1024 | `			/*` |
|       - | 1025 | `			 * According to the PHP language reference manual:` |
|       - | 1026 | `			 *` |
|       - | 1027 | `			 *  If you compare a number with a string or the comparison involves numerical` |
|       - | 1028 | `			 *  strings, then each string is converted to a number and the comparison` |
|       - | 1029 | `			 *  performed numerically.` |
|       - | 1030 | `			 */` |
|  114127 | 1031 | `			if( PH7_MemObjIsNumeric(pObj1) ){` |
|       - | 1032 | `				/* Perform a numeric comparison */` |
|       9 | 1033 | `				goto Numeric;` |
|       - | 1034 | `			}` |
|  114119 | 1035 | `			if( PH7_MemObjIsNumeric(pObj2) ){` |
|       - | 1036 | `				/* Perform a numeric comparison */` |
|     ! 0 | 1037 | `				goto Numeric;` |
|       - | 1038 | `			}` |
|   57093 | 1039 | `		}` |
|       - | 1040 | `		/* Perform a strict string comparison.*/` |
|  307683 | 1041 | `		if( (pObj1->iFlags&MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1042 | `			PH7_MemObjToString(pObj1);` |
|     ! 0 | 1043 | `		}` |
|  307683 | 1044 | `		if( (pObj2->iFlags&MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1045 | `			PH7_MemObjToString(pObj2);` |
|     ! 0 | 1046 | `		}` |
|  307683 | 1047 | `		SyStringInitFromBuf(&s1,SyBlobData(&pObj1->sBlob),SyBlobLength(&pObj1->sBlob));` |
|  307683 | 1048 | `		SyStringInitFromBuf(&s2,SyBlobData(&pObj2->sBlob),SyBlobLength(&pObj2->sBlob));` |
|       - | 1049 | `		/*` |
|       - | 1050 | `		 * Strings are compared using memcmp(). If one value is an exact prefix of the` |
|       - | 1051 | `		 * other, then the shorter value is less than the longer value.` |
|       - | 1052 | `		 */` |
|  307683 | 1053 | `		rc = SyMemcmp((const void *)s1.zString,(const void *)s2.zString,SXMIN(s1.nByte,s2.nByte));` |
|  307683 | 1054 | `		if( rc == 0 ){` |
|  108612 | 1055 | `			if( s1.nByte != s2.nByte ){` |
|     754 | 1056 | `				rc = s1.nByte < s2.nByte ? -1 : 1;` |
|     377 | 1057 | `			}` |
|   54306 | 1058 | `		}` |
|  307683 | 1059 | `		return rc;` |
|  186468 | 1060 | `	}else if( iComb & (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|   93210 | 1061 | `Numeric:` |
|       - | 1062 | `		/* Perform a numeric comparison if one of the operand is numeric(integer or real) */` |
|  186476 | 1063 | `		if( (pObj1->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|       7 | 1064 | `			PH7_MemObjToNumeric(pObj1);` |
|       3 | 1065 | `		}` |
|  186476 | 1066 | `		if( (pObj2->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|       7 | 1067 | `			PH7_MemObjToNumeric(pObj2);` |
|       3 | 1068 | `		}` |
|  186476 | 1069 | `		if( (pObj1->iFlags & pObj2->iFlags & MEMOBJ_INT) == 0) {` |
|       - | 1070 | `			/*` |
|       - | 1071 | `			 * Symisc eXtension to the PHP language:` |
|       - | 1072 | `			 *  Floating point comparison is introduced and works as expected.` |
|       - | 1073 | `			 */` |
|       - | 1074 | `			ph7_real r1,r2;` |
|       - | 1075 | `			/* Compare as reals */` |
|      97 | 1076 | `			if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|     ! 0 | 1077 | `				PH7_MemObjToReal(pObj1);` |
|     ! 0 | 1078 | `			}` |
|      97 | 1079 | `			r1 = pObj1->rVal;` |
|      97 | 1080 | `			if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|      13 | 1081 | `				PH7_MemObjToReal(pObj2);` |
|       6 | 1082 | `			}` |
|      97 | 1083 | `			r2 = pObj2->rVal;` |
|      97 | 1084 | `			if( r1 > r2 ){` |
|       9 | 1085 | `				return 1;` |
|      89 | 1086 | `			}else if( r1 < r2 ){` |
|      79 | 1087 | `				return -1;` |
|       - | 1088 | `			}` |
|      11 | 1089 | `			return 0;` |
|     ! 0 | 1090 | `		}else{` |
|       - | 1091 | `			/* Integer comparison */` |
|  186380 | 1092 | `			if( pObj1->x.iVal > pObj2->x.iVal ){` |
|    1636 | 1093 | `				return 1;` |
|  184746 | 1094 | `			}else if( pObj1->x.iVal < pObj2->x.iVal ){` |
|  182202 | 1095 | `				return -1;` |
|       - | 1096 | `			}` |
|    2546 | 1097 | `			return 0;` |
|       - | 1098 | `		}` |
|       - | 1099 | `	}` |
|       - | 1100 | `	/* NOT REACHED */` |
|     ! 0 | 1101 | `	return 0;` |
|  297608 | 1102 |  |
|       - | 1103 | `/*` |
|       - | 1104 | ` * Perform an addition operation of two ph7_values.` |
|       - | 1105 | ` * The reason this function is implemented here rather than 'vm.c'` |
|       - | 1106 | ` * is that the '+' operator is overloaded.` |
|       - | 1107 | ` * That is,the '+' operator is used for arithmetic operation and also` |
|       - | 1108 | ` * used for operation on arrays [i.e: union]. When used with an array` |
|       - | 1109 | ` * The + operator returns the right-hand array appended to the left-hand array.` |
|       - | 1110 | ` * For keys that exist in both arrays, the elements from the left-hand array` |
|       - | 1111 | ` * will be used, and the matching elements from the right-hand array will` |
|       - | 1112 | ` * be ignored.` |
|       - | 1113 | ` * This function take care of handling all the scenarios.` |
|       - | 1114 | ` */` |
|    1810 | 1115 | `PH7_PRIVATE sxi32 PH7_MemObjAdd(ph7_value *pObj1,ph7_value *pObj2,int bAddStore)` |
|       2 | 1116 |  |
|    1812 | 1117 | `	if( ((pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP) == 0 ){` |
|       - | 1118 | `			/* Arithemtic operation */` |
|    1808 | 1119 | `			PH7_MemObjToNumeric(pObj1);` |
|    1808 | 1120 | `			PH7_MemObjToNumeric(pObj2);` |
|    1808 | 1121 | `			if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_REAL ){` |
|       - | 1122 | `				/* Floating point arithmetic */` |
|       - | 1123 | `				ph7_real a,b;` |
|      25 | 1124 | `				if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|       7 | 1125 | `					PH7_MemObjToReal(pObj1);` |
|       3 | 1126 | `				}` |
|      25 | 1127 | `				if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|       3 | 1128 | `					PH7_MemObjToReal(pObj2);` |
|       1 | 1129 | `				}` |
|      25 | 1130 | `				a = pObj1->rVal;` |
|      25 | 1131 | `				b = pObj2->rVal;` |
|      25 | 1132 | `				pObj1->rVal = a+b;` |
|      25 | 1133 | `				MemObjSetType(pObj1,MEMOBJ_REAL);` |
|       - | 1134 | `				/* Try to get an integer representation also */` |
|      25 | 1135 | `				MemObjTryIntger(&(*pObj1));` |
|      13 | 1136 | `			}else{` |
|       - | 1137 | `				/* Integer arithmetic */` |
|       - | 1138 | `				sxi64 a,b;` |
|    1784 | 1139 | `				a = pObj1->x.iVal;` |
|    1784 | 1140 | `				b = pObj2->x.iVal;` |
|    1784 | 1141 | `				pObj1->x.iVal = a+b;` |
|    1784 | 1142 | `				MemObjSetType(pObj1,MEMOBJ_INT);` |
|       - | 1143 | `			}` |
|     905 | 1144 | `	}else{` |
|       6 | 1145 | `		if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP ){` |
|       - | 1146 | `			ph7_hashmap *pMap;` |
|       - | 1147 | `			sxi32 rc;` |
|       6 | 1148 | `			if( bAddStore ){` |
|       - | 1149 | `				/* Do not duplicate the hashmap,use the left one since its an add&store operation.` |
|       - | 1150 | `				 */` |
|     ! 0 | 1151 | `				if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       - | 1152 | `					/* Force a hashmap cast */` |
|     ! 0 | 1153 | `					rc = PH7_MemObjToHashmap(pObj1);` |
|     ! 0 | 1154 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 1155 | `						PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|     ! 0 | 1156 | `						return rc;` |
|       - | 1157 | `					}` |
|     ! 0 | 1158 | `				}` |
|       - | 1159 | `				/* Point to the structure that describe the hashmap */` |
|     ! 0 | 1160 | `				pMap = (ph7_hashmap *)pObj1->x.pOther;` |
|     ! 0 | 1161 | `			}else{` |
|       - | 1162 | `				/* Create a new hashmap */` |
|       6 | 1163 | `				pMap = PH7_NewHashmap(pObj1->pVm,0,0);` |
|       6 | 1164 | `				if( pMap == 0){` |
|     ! 0 | 1165 | `					PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|     ! 0 | 1166 | `					return SXERR_MEM;` |
|       - | 1167 | `				}` |
|       - | 1168 | `			}` |
|       6 | 1169 | `			if( !bAddStore ){` |
|       6 | 1170 | `				if(pObj1->iFlags & MEMOBJ_HASHMAP ){` |
|       - | 1171 | `					/* Perform a hashmap duplication */` |
|       6 | 1172 | `					PH7_HashmapDup((ph7_hashmap *)pObj1->x.pOther,pMap);` |
|       4 | 1173 | `				}else{` |
|     ! 0 | 1174 | `					if((pObj1->iFlags & MEMOBJ_NULL) == 0 ){` |
|       - | 1175 | `						/* Simple insertion */` |
|     ! 0 | 1176 | `						PH7_HashmapInsert(pMap,0,pObj1);` |
|     ! 0 | 1177 | `					}` |
|       - | 1178 | `				}` |
|       2 | 1179 | `			}` |
|       - | 1180 | `			/* Perform the union */` |
|       6 | 1181 | `			if(pObj2->iFlags & MEMOBJ_HASHMAP ){` |
|       6 | 1182 | `				PH7_HashmapUnion(pMap,(ph7_hashmap *)pObj2->x.pOther);` |
|       4 | 1183 | `			}else{` |
|     ! 0 | 1184 | `				if((pObj2->iFlags & MEMOBJ_NULL) == 0 ){` |
|       - | 1185 | `					/* Simple insertion */` |
|     ! 0 | 1186 | `					PH7_HashmapInsert(pMap,0,pObj2);` |
|     ! 0 | 1187 | `				}` |
|       - | 1188 | `			}` |
|       - | 1189 | `			/* Reflect the change */` |
|       6 | 1190 | `			if( pObj1->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 1191 | `				SyBlobRelease(&pObj1->sBlob);` |
|     ! 0 | 1192 | `			}` |
|       6 | 1193 | `			pObj1->x.pOther = pMap;` |
|       6 | 1194 | `			MemObjSetType(pObj1,MEMOBJ_HASHMAP);` |
|       2 | 1195 | `		}` |
|       - | 1196 | `	}` |
|    1812 | 1197 | `	return SXRET_OK;` |
|     907 | 1198 |  |
|       - | 1199 | `/*` |
|       - | 1200 | ` * Return a printable representation of the type of a given` |
|       - | 1201 | ` * ph7_value.` |
|       - | 1202 | ` */` |
|     442 | 1203 | `PH7_PRIVATE const char * PH7_MemObjTypeDump(ph7_value *pVal)` |
|       2 | 1204 |  |
|     444 | 1205 | `	const char *zType = "";` |
|     444 | 1206 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|      15 | 1207 | `		zType = "null";` |
|     437 | 1208 | `	}else if( pVal->iFlags & MEMOBJ_INT ){` |
|     104 | 1209 | `		zType = "int";` |
|     379 | 1210 | `	}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|       3 | 1211 | `		zType = "float";` |
|     327 | 1212 | `	}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|      52 | 1213 | `		zType = "string";` |
|     301 | 1214 | `	}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|     126 | 1215 | `		zType = "bool";` |
|     214 | 1216 | `	}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|      22 | 1217 | `		zType = "array";` |
|     141 | 1218 | `	}else if( pVal->iFlags & MEMOBJ_OBJ ){` |
|     131 | 1219 | `		zType = "object";` |
|      65 | 1220 | `	}else if( pVal->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 1221 | `		zType = "resource";` |
|     ! 0 | 1222 | `	}` |
|     444 | 1223 | `	return zType;` |
|       2 | 1224 |  |
|       - | 1225 | `/*` |
|       - | 1226 | ` * Dump a ph7_value [i.e: get a printable representation of it's type and contents.].` |
|       - | 1227 | ` * Store the dump in the given blob.` |
|       - | 1228 | ` */` |
|     506 | 1229 | `PH7_PRIVATE sxi32 PH7_MemObjDump(` |
|       - | 1230 | `	SyBlob *pOut,      /* Store the dump here */` |
|       - | 1231 | `	ph7_value *pObj,   /* Dump this */` |
|       - | 1232 | `	int ShowType,      /* TRUE to output value type */` |
|       - | 1233 | `	int nTab,          /* # of Whitespace to insert */` |
|       - | 1234 | `	int nDepth,        /* Nesting level */` |
|       - | 1235 | `	int isRef          /* TRUE if referenced object */` |
|       - | 1236 | `	)` |
|       2 | 1237 |  |
|     508 | 1238 | `	sxi32 rc = SXRET_OK;` |
|       - | 1239 | `	const char *zType;` |
|       - | 1240 | `	int i;` |
|    4696 | 1241 | `	for( i = 0 ; i < nTab ; i++ ){` |
|    4190 | 1242 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|    2096 | 1243 | `	}` |
|     508 | 1244 | `	if( ShowType ){` |
|     418 | 1245 | `		if( isRef ){` |
|     ! 0 | 1246 | `			SyBlobAppend(&(*pOut),"&",sizeof(char));` |
|     ! 0 | 1247 | `		}` |
|       - | 1248 | `		/* Get value type first */` |
|     418 | 1249 | `		zType = PH7_MemObjTypeDump(pObj);` |
|     418 | 1250 | `		SyBlobAppend(&(*pOut),zType,SyStrlen(zType));` |
|     208 | 1251 | `	}` |
|     508 | 1252 | `	if((pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|     498 | 1253 | `		if ( ShowType ){` |
|     408 | 1254 | `			SyBlobAppend(&(*pOut),"(",sizeof(char));` |
|     203 | 1255 | `		}` |
|     498 | 1256 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       - | 1257 | `			/* Dump hashmap entries */` |
|      30 | 1258 | `			rc = PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,ShowType,nTab+1,nDepth+1);` |
|     484 | 1259 | `		}else if(pObj->iFlags & MEMOBJ_OBJ ){` |
|       - | 1260 | `			/* Dump class instance attributes */` |
|     133 | 1261 | `			rc = PH7_ClassInstanceDump(&(*pOut),(ph7_class_instance *)pObj->x.pOther,ShowType,nTab+1,nDepth+1);` |
|      67 | 1262 | `		}else{` |
|     338 | 1263 | `			SyBlob *pContents = &pObj->sBlob;` |
|       - | 1264 | `			/* Get a printable representation of the contents */` |
|     338 | 1265 | `			if((pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|     246 | 1266 | `				MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|     124 | 1267 | `			}else{` |
|       - | 1268 | `				/* Append length first */` |
|      94 | 1269 | `				if( ShowType ){` |
|      38 | 1270 | `					SyBlobFormat(&(*pOut),"%u '",SyBlobLength(&pObj->sBlob));` |
|      18 | 1271 | `				}` |
|      94 | 1272 | `				if( SyBlobLength(pContents) > 0 ){` |
|      82 | 1273 | `					SyBlobAppend(&(*pOut),SyBlobData(pContents),SyBlobLength(pContents));` |
|      40 | 1274 | `				}` |
|      94 | 1275 | `				if( ShowType ){` |
|      38 | 1276 | `					SyBlobAppend(&(*pOut),"'",sizeof(char));` |
|      18 | 1277 | `				}` |
|       - | 1278 | `			}` |
|       - | 1279 | `		}` |
|     498 | 1280 | `		if( ShowType ){` |
|     408 | 1281 | `			if( (pObj->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|     264 | 1282 | `				SyBlobAppend(&(*pOut),")",sizeof(char));` |
|     131 | 1283 | `			}` |
|     203 | 1284 | `		}` |
|     248 | 1285 | `	}` |
|       - | 1286 | `#ifdef __WINNT__` |
|       2 | 1287 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 1288 | `#else` |
|     506 | 1289 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 1290 | `#endif` |
|     508 | 1291 | `	return rc;` |
|       2 | 1292 |  |
|       - | 1293 |  |
