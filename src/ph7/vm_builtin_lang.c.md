# src/ph7/vm_builtin_lang.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 820/1041 lines (78.77%)

[Root index](../../index.md) | [Directory index](index.md)

|     Hits | Line | Source |
| -------: | ---: | :--- |
|        - |    1 | `/**` |
|        - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|        - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|        - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|        - |    5 | ` */` |
|        - |    6 | `#include "ph7int.h"` |
|        - |    7 | `/*` |
|        - |    8 | ` * Section:` |
|        - |    9 | ` *    Language-level builtins: define/defined/constant and the enum` |
|        - |   10 | ` *    helpers, the rand/random_* family, echo/print/exit, version and` |
|        - |   11 | ` *    credits, parse_url, compact/extract and import_request_variables.` |
|        - |   12 | ` *    Registration rows stay in vm.c's aVmFunc[].` |
|        - |   13 | ` * Status:` |
|        - |   14 | ` *    Stable.` |
|        - |   15 | ` */` |
|        - |   16 | `/*` |
|        - |   17 | ` * bool defined(string $name)` |
|        - |   18 | ` *  Checks whether a given named constant exists.` |
|        - |   19 | ` * Parameter:` |
|        - |   20 | ` *  Name of the desired constant.` |
|        - |   21 | ` * Return` |
|        - |   22 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |   23 | ` */` |
|       30 |   24 | `PH7_PRIVATE int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 |   25 | `{` |
|        - |   26 | `	const char *zName;` |
|       34 |   27 | `	int nLen = 0;` |
|       34 |   28 | `	int res = 0;` |
|       34 |   29 | `	if( nArg < 1 ){` |
|        - |   30 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |   31 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |   32 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |   33 | `		return SXRET_OK;` |
|        - |   34 | `	}` |
|        - |   35 | `	/* Extract constant name */` |
|       34 |   36 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |   37 | `	/* Perform the lookup */` |
|       34 |   38 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |   39 | `		/* Already defined */` |
|       32 |   40 | `		res = 1;` |
|       14 |   41 | `	}` |
|       34 |   42 | `	ph7_result_bool(pCtx,res);` |
|       34 |   43 | `	return SXRET_OK;` |
|       19 |   44 | `}` |
|        - |   45 | `/*` |
|        - |   46 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |   47 | ` * below.` |
|        - |   48 | ` */` |
|       28 |   49 | `PH7_PRIVATE void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        3 |   50 | `{` |
|       31 |   51 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |   52 | `	/* Expand constant value */` |
|       31 |   53 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       31 |   54 | `}` |
|        - |   55 | `/*` |
|        - |   56 | ` * bool define(string $constant_name,expression value)` |
|        - |   57 | ` *  Defines a named constant at runtime.` |
|        - |   58 | ` * Parameter:` |
|        - |   59 | ` *  $constant_name` |
|        - |   60 | ` *   The name of the constant` |
|        - |   61 | ` *  $value` |
|        - |   62 | ` *   Constant value` |
|        - |   63 | ` * Return:` |
|        - |   64 | ` *   TRUE on success,FALSE on failure.` |
|        - |   65 | ` */` |
|       22 |   66 | `PH7_PRIVATE int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 |   67 | `{` |
|        - |   68 | `	const char *zName;  /* Constant name */` |
|        - |   69 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       25 |   70 | `	int nLen = 0;       /* Name length */` |
|        - |   71 | `	sxi32 rc;` |
|       25 |   72 | `	if( nArg < 2 ){` |
|        - |   73 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |   74 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |   75 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |   76 | `		return SXRET_OK;` |
|        - |   77 | `	}` |
|       25 |   78 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |   79 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |   80 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |   81 | `		return SXRET_OK;` |
|        - |   82 | `	}` |
|        - |   83 | `	/* Extract constant name */` |
|       25 |   84 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       25 |   85 | `	if( nLen < 1 ){` |
|      ! 0 |   86 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |   87 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |   88 | `		return SXRET_OK;` |
|        - |   89 | `	}` |
|        - |   90 | `	/* Duplicate constant value */` |
|       25 |   91 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       25 |   92 | `	if( pValue == 0 ){` |
|      ! 0 |   93 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |   94 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |   95 | `		return SXRET_OK;` |
|        - |   96 | `	}` |
|        - |   97 | `	/* Initialize the memory object */` |
|       25 |   98 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |   99 | `	/* Register the constant */` |
|        - |  100 | `	{` |
|        - |  101 | `		SyString sConsName;` |
|       25 |  102 | `		SyStringInitFromBuf(&sConsName,zName,(sxu32)nLen);` |
|       36 |  103 | `		rc = PH7_VmRegisterConstantEx(pCtx->pVm,&sConsName,VmExpandUserConstant,pValue,` |
|       22 |  104 | `			(SyString *)SySetPeek(&pCtx->pVm->aFiles),0,1);` |
|        - |  105 | `	}` |
|       25 |  106 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  107 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  108 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  109 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  110 | `		return SXRET_OK;` |
|        - |  111 | `	}` |
|        - |  112 | `	/* Duplicate constant value */` |
|       25 |  113 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       25 |  114 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  115 | `		/* Lower case the constant name */` |
|      ! 0 |  116 | `		char *zCur = (char *)zName;` |
|      ! 0 |  117 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  118 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  119 | `				/* UTF-8 stream */` |
|      ! 0 |  120 | `				zCur++;` |
|      ! 0 |  121 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  122 | `					zCur++;` |
|      ! 0 |  123 | `				}` |
|      ! 0 |  124 | `				continue;` |
|        - |  125 | `			}` |
|      ! 0 |  126 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  127 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  128 | `				zCur[0] = (char)c;` |
|      ! 0 |  129 | `			}` |
|      ! 0 |  130 | `			zCur++;` |
|      ! 0 |  131 | `		}` |
|        - |  132 | `		/* Register the lowercase alias with its OWN value copy (not the same` |
|        - |  133 | `		 * pValue) so the two entries don't share one object — otherwise freeing` |
|        - |  134 | `		 * one on a later overwrite would dangle the other. */` |
|        - |  135 | `		{` |
|      ! 0 |  136 | `			ph7_value *pAlias = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|      ! 0 |  137 | `			if( pAlias ){` |
|      ! 0 |  138 | `				PH7_MemObjInit(pCtx->pVm,pAlias);` |
|      ! 0 |  139 | `				PH7_MemObjStore(apArg[1],pAlias);` |
|      ! 0 |  140 | `				ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pAlias);` |
|      ! 0 |  141 | `			}` |
|        - |  142 | `		}` |
|      ! 0 |  143 | `	}` |
|        - |  144 | `	/* All done,return TRUE */` |
|       25 |  145 | `	ph7_result_bool(pCtx,1);` |
|       25 |  146 | `	return SXRET_OK;` |
|       14 |  147 | `}` |
|        - |  148 | `/*` |
|        - |  149 | ` * value constant(string $name)` |
|        - |  150 | ` *  Returns the value of a constant` |
|        - |  151 | ` * Parameter` |
|        - |  152 | ` *  $name` |
|        - |  153 | ` *    Name of the constant.` |
|        - |  154 | ` * Return` |
|        - |  155 | ` *  Constant value or NULL if not defined.` |
|        - |  156 | ` */` |
|        - |  157 | `/*` |
|        - |  158 | ` * Enum method thunks (PHP 8.1). Every enum's synthesized cases()/from()/` |
|        - |  159 | ` * tryFrom() methods (GenStateCompileEnumMethods, compile.c) forward here with` |
|        - |  160 | ` * the enum's FQN as a literal first argument — the same forwarder pattern the` |
|        - |  161 | ` * Generator/Fiber/Reflection builtins use.` |
|        - |  162 | ` */` |
|        - |  163 | `/* array __phl_enum_cases(string $enumFqn) — declaration-order case list */` |
|        8 |  164 | `PH7_PRIVATE int vm_builtin_enum_cases(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  165 | `{` |
|        9 |  166 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  167 | `	ph7_class_attr **apCase;` |
|        - |  168 | `	ph7_class *pClass;` |
|        - |  169 | `	ph7_value *pArray;` |
|        - |  170 | `	sxu32 n;` |
|        - |  171 | `	sxi32 rc;` |
|        9 |  172 | `	if( nArg < 1 \|\| (pClass = VmExtractEnumClass(pVm,apArg[0])) == 0 ){` |
|      ! 0 |  173 | `		ph7_result_null(pCtx);` |
|      ! 0 |  174 | `		return SXRET_OK;` |
|        - |  175 | `	}` |
|        9 |  176 | `	rc = VmEnumMaterialize(pVm,pClass);` |
|        9 |  177 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  178 | `		return rc;` |
|        - |  179 | `	}` |
|        9 |  180 | `	pArray = ph7_context_new_array(pCtx);` |
|        9 |  181 | `	if( pArray == 0 ){` |
|      ! 0 |  182 | `		ph7_result_null(pCtx);` |
|      ! 0 |  183 | `		return SXRET_OK;` |
|        - |  184 | `	}` |
|        9 |  185 | `	apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|       25 |  186 | `	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|       17 |  187 | `		ph7_value *pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,apCase[n]->nIdx);` |
|       17 |  188 | `		if( pSlot ){` |
|       17 |  189 | `			ph7_array_add_elem(pArray,0,pSlot); /* Copies; the object ref is retained */` |
|        8 |  190 | `		}` |
|        9 |  191 | `	}` |
|        9 |  192 | `	ph7_result_value(pCtx,pArray);` |
|        9 |  193 | `	return SXRET_OK;` |
|        5 |  194 | `}` |
|        - |  195 | `/* Shared scan for from()/tryFrom(): return the slot of the case whose backing` |
|        - |  196 | ` * value equals *pNeedle (already coerced to the backing type by the synthesized` |
|        - |  197 | ` * method's signature), or 0 on miss. */` |
|       20 |  198 | `static ph7_value * VmEnumFindCaseByValue(ph7_vm *pVm,ph7_class *pClass,ph7_value *pNeedle)` |
|        1 |  199 | `{` |
|       21 |  200 | `	ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|        - |  201 | `	sxu32 n;` |
|       47 |  202 | `	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|       39 |  203 | `		ph7_value *pVal = VmEnumCaseBackingValue(pVm,apCase[n]);` |
|       39 |  204 | `		int bMatch = 0;` |
|       39 |  205 | `		if( pVal ){` |
|       39 |  206 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|        3 |  207 | `				bMatch = (pNeedle->iFlags & MEMOBJ_INT) && pVal->x.iVal == pNeedle->x.iVal;` |
|        2 |  208 | `			}else{` |
|       55 |  209 | `				bMatch = (pNeedle->iFlags & MEMOBJ_STRING)` |
|       36 |  210 | `					&& SyBlobLength(&pVal->sBlob) == SyBlobLength(&pNeedle->sBlob)` |
|       54 |  211 | `					&& SyMemcmp(SyBlobData(&pVal->sBlob),SyBlobData(&pNeedle->sBlob),` |
|       24 |  212 | `						SyBlobLength(&pNeedle->sBlob)) == 0;` |
|        - |  213 | `			}` |
|       19 |  214 | `		}` |
|       39 |  215 | `		if( bMatch ){` |
|       13 |  216 | `			return (ph7_value *)SySetAt(&pVm->aMemObj,apCase[n]->nIdx);` |
|        - |  217 | `		}` |
|       14 |  218 | `	}` |
|        9 |  219 | `	return 0;` |
|       11 |  220 | `}` |
|        - |  221 | `/* static from(int\|string $value) / static tryFrom(int\|string $value) */` |
|       20 |  222 | `static int VmEnumFromCommon(ph7_context *pCtx,int nArg,ph7_value **apArg,int bTry)` |
|        1 |  223 | `{` |
|       21 |  224 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  225 | `	ph7_class *pClass;` |
|        - |  226 | `	ph7_value *pFound;` |
|        - |  227 | `	sxi32 rc;` |
|       21 |  228 | `	if( nArg < 2 \|\| (pClass = VmExtractEnumClass(pVm,apArg[0])) == 0 ){` |
|      ! 0 |  229 | `		ph7_result_null(pCtx);` |
|      ! 0 |  230 | `		return SXRET_OK;` |
|        - |  231 | `	}` |
|       21 |  232 | `	rc = VmEnumMaterialize(pVm,pClass);` |
|       21 |  233 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  234 | `		return rc;` |
|        - |  235 | `	}` |
|       21 |  236 | `	pFound = VmEnumFindCaseByValue(pVm,pClass,apArg[1]);` |
|       21 |  237 | `	if( pFound ){` |
|       13 |  238 | `		ph7_result_value(pCtx,pFound);` |
|       13 |  239 | `		return SXRET_OK;` |
|        - |  240 | `	}` |
|        9 |  241 | `	if( bTry ){` |
|        7 |  242 | `		ph7_result_null(pCtx);` |
|        7 |  243 | `		return SXRET_OK;` |
|        - |  244 | `	}` |
|        3 |  245 | `	if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|        - |  246 | `		char zVal[32];` |
|      ! 0 |  247 | `		SyBufferFormat(zVal,sizeof(zVal),"%qd",ph7_value_to_int64(apArg[1]));` |
|      ! 0 |  248 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      ! 0 |  249 | `			"%s is not a valid backing value for enum %z",zVal,&pClass->sName);` |
|        - |  250 | `	}` |
|        4 |  251 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|        - |  252 | `		"\"%.*s\" is not a valid backing value for enum %z",` |
|        2 |  253 | `		(int)SyBlobLength(&apArg[1]->sBlob),(const char *)SyBlobData(&apArg[1]->sBlob),` |
|        1 |  254 | `		&pClass->sName);` |
|       11 |  255 | `}` |
|       14 |  256 | `PH7_PRIVATE int vm_builtin_enum_from(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  257 | `{` |
|       15 |  258 | `	return VmEnumFromCommon(pCtx,nArg,apArg,FALSE);` |
|        1 |  259 | `}` |
|        6 |  260 | `PH7_PRIVATE int vm_builtin_enum_tryfrom(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  261 | `{` |
|        7 |  262 | `	return VmEnumFromCommon(pCtx,nArg,apArg,TRUE);` |
|        1 |  263 | `}` |
|        - |  264 | `/*` |
|        - |  265 | ` * bool enum_exists(string $enum, bool $autoload = true)` |
|        - |  266 | ` *  TRUE only for a declared enum (PHP 8.1); a plain class/interface is FALSE.` |
|        - |  267 | ` */` |
|        6 |  268 | `PH7_PRIVATE int vm_builtin_enum_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  269 | `{` |
|        7 |  270 | `	ph7_class *pClass = 0;` |
|        7 |  271 | `	if( nArg > 0 ){` |
|        7 |  272 | `		pClass = VmExtractEnumClass(pCtx->pVm,apArg[0]);` |
|        3 |  273 | `	}` |
|        7 |  274 | `	ph7_result_bool(pCtx,pClass != 0);` |
|        7 |  275 | `	return SXRET_OK;` |
|        1 |  276 | `}` |
|       24 |  277 | `PH7_PRIVATE int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 |  278 | `{` |
|        - |  279 | `	SyHashEntry *pEntry;` |
|        - |  280 | `	ph7_constant *pCons;` |
|        - |  281 | `	const char *zName; /* Constant name */` |
|        - |  282 | `	ph7_value sVal;    /* Constant value */` |
|        - |  283 | `	int nLen;` |
|       27 |  284 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  285 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  286 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  287 | `		ph7_result_null(pCtx);` |
|      ! 0 |  288 | `		return SXRET_OK;` |
|        - |  289 | `	}` |
|        - |  290 | `	/* Extract the constant name */` |
|       27 |  291 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  292 | `	/* Class-constant form "C::K" (band A #4): resolve the class — interfaces` |
|        - |  293 | `	 * included — and read the mounted constant slot; php throws a catchable` |
|        - |  294 | `	 * Error for an unknown class or constant (pre-fix this path warned` |
|        - |  295 | `	 * "Undefined constant" and returned NULL without ever looking at the` |
|        - |  296 | `	 * class). */` |
|        - |  297 | `	{` |
|        - |  298 | `		int iSep;` |
|      207 |  299 | `		for( iSep = 0; iSep + 1 < nLen; iSep++ ){` |
|      195 |  300 | `			if( zName[iSep] == ':' && zName[iSep+1] == ':' ){` |
|       13 |  301 | `				break;` |
|        - |  302 | `			}` |
|       93 |  303 | `		}` |
|       27 |  304 | `		if( iSep + 1 < nLen ){` |
|       13 |  305 | `			ph7_class *pClass = iSep > 0 ?` |
|       12 |  306 | `				PH7_VmExtractClass(pCtx->pVm,zName,(sxu32)iSep,FALSE,0) : 0;` |
|       13 |  307 | `			if( pClass == 0 ){` |
|        4 |  308 | `				return PH7_VmThrowException(pCtx,"Error",` |
|        1 |  309 | `					"Class \"%.*s\" not found",iSep,zName);` |
|        - |  310 | `			}` |
|       11 |  311 | `			if( iSep + 2 < nLen ){` |
|       16 |  312 | `				ph7_class_attr *pAttr = PH7_ClassExtractAttribute(pClass,` |
|       10 |  313 | `					&zName[iSep+2],(sxu32)(nLen - iSep - 2));` |
|       11 |  314 | `				if( pAttr && pAttr->nIdx == SXU32_HIGH ){` |
|        - |  315 | `					/* Unmaterialized: enum case → materialize the singletons` |
|        - |  316 | `					 * (all of them: constant("S::A") is a direct access, like` |
|        - |  317 | `					 * OP_MEMBER); plain constant → run its initializer. */` |
|        - |  318 | `					sxi32 rcEnum;` |
|        7 |  319 | `					if( pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE ){` |
|      ! 0 |  320 | `						rcEnum = VmEnumMaterialize(pCtx->pVm,pClass);` |
|      ! 0 |  321 | `					}else{` |
|        7 |  322 | `						rcEnum = VmClassConstEvalOnDemand(pCtx->pVm,pClass,pAttr);` |
|        - |  323 | `					}` |
|        7 |  324 | `					if( rcEnum != SXRET_OK ){` |
|      ! 0 |  325 | `						return rcEnum;` |
|        - |  326 | `					}` |
|        3 |  327 | `				}` |
|       11 |  328 | `				if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) ){` |
|        9 |  329 | `					ph7_value *pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pAttr->nIdx);` |
|        9 |  330 | `					if( pValue ){` |
|        9 |  331 | `						if( SySetUsed(&pAttr->aAttrs) > 0 ){` |
|        - |  332 | `							/* #[\Deprecated] warns through constant() too (php) */` |
|        3 |  333 | `							VmDeprecatedConstNotice(pCtx->pVm,pClass,pAttr);` |
|        1 |  334 | `						}` |
|        9 |  335 | `						ph7_result_value(pCtx,pValue);` |
|        9 |  336 | `						return SXRET_OK;` |
|        - |  337 | `					}` |
|      ! 0 |  338 | `				}` |
|        1 |  339 | `			}` |
|        4 |  340 | `			return PH7_VmThrowException(pCtx,"Error",` |
|        1 |  341 | `				"Undefined constant %.*s",nLen,zName);` |
|        - |  342 | `		}` |
|        - |  343 | `	}` |
|        - |  344 | `	/* Perform the query */` |
|       15 |  345 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       15 |  346 | `	if( pEntry == 0 ){` |
|        - |  347 | `		/* php 8: a catchable Error, not a notice + NULL (band A #4) */` |
|        8 |  348 | `		return PH7_VmThrowException(pCtx,"Error",` |
|        2 |  349 | `			"Undefined constant \"%.*s\"",nLen,zName);` |
|        - |  350 | `	}` |
|       11 |  351 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  352 | `	/* Point to the structure that describe the constant */` |
|       11 |  353 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  354 | `	/* Extract constant value by calling it's associated callback` |
|        - |  355 | `	 * (emits the #[\Deprecated] notice first when attributed, php) */` |
|       11 |  356 | `	VmExpandConstantWithNotice(pCtx->pVm,pCons,&sVal);` |
|        - |  357 | `	/* Return that value */` |
|       11 |  358 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  359 | `	/* Cleanup */` |
|       11 |  360 | `	PH7_MemObjRelease(&sVal);` |
|       11 |  361 | `	return SXRET_OK;` |
|       15 |  362 | `}` |
|        - |  363 | `/*` |
|        - |  364 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  365 | ` * defined below.` |
|        - |  366 | ` */` |
|      932 |  367 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  368 | `{` |
|      933 |  369 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  370 | `	ph7_value sName;` |
|        - |  371 | `	sxi32 rc;` |
|        - |  372 | `	/* Prepare the constant name for insertion */` |
|      933 |  373 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      933 |  374 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  375 | `	/* Perform the insertion */` |
|      933 |  376 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      933 |  377 | `	PH7_MemObjRelease(&sName);` |
|      933 |  378 | `	return rc;` |
|        1 |  379 | `}` |
|        - |  380 | `/*` |
|        - |  381 | ` * array get_defined_constants(void)` |
|        - |  382 | ` *  Returns an associative array with the names of all defined` |
|        - |  383 | ` *  constants.` |
|        - |  384 | ` * Parameters` |
|        - |  385 | ` *  NONE.` |
|        - |  386 | ` * Returns` |
|        - |  387 | ` *  Returns the names of all the constants currently defined.` |
|        - |  388 | ` */` |
|        2 |  389 | `PH7_PRIVATE int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  390 | `{` |
|        - |  391 | `	ph7_value *pArray;` |
|        - |  392 | `	/* Create the array first*/` |
|        3 |  393 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  394 | `	if( pArray == 0 ){` |
|      ! 0 |  395 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  396 | `		SXUNUSED(apArg);` |
|        - |  397 | `		/* Return NULL */` |
|      ! 0 |  398 | `		ph7_result_null(pCtx);` |
|      ! 0 |  399 | `		return SXRET_OK;` |
|        - |  400 | `	}` |
|        - |  401 | `	/* Fill the array with the defined constants */` |
|        3 |  402 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  403 | `	/* Return the created array */` |
|        3 |  404 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  405 | `	return SXRET_OK;` |
|        2 |  406 | `}` |
|        - |  407 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - |  408 | `/*` |
|        - |  409 | ` * Section:` |
|        - |  410 | ` *  Random numbers/string generators.` |
|        - |  411 | ` * Status:` |
|        - |  412 | ` *    Stable.` |
|        - |  413 | ` */` |
|        - |  414 | `/*` |
|        - |  415 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  416 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|        - |  417 | ` * implemented in src/sx/sxrand.c).` |
|        - |  418 | ` */` |
|     3388 |  419 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        5 |  420 | `{` |
|        - |  421 | `	sxu32 iNum;` |
|     3393 |  422 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     3393 |  423 | `	return iNum;` |
|        5 |  424 | `}` |
|        - |  425 | `/*` |
|        - |  426 | ` * The MT19937 generator that backs PHP's rand()/mt_rand() family. It is kept` |
|        - |  427 | ` * separate from the RC4 SyRandomness above so that srand()/mt_srand() give` |
|        - |  428 | ` * userland PHP's reproducible sequence without perturbing the engine's internal` |
|        - |  429 | ` * entropy (object ids, uniqid, quicksort pivots stay on the RC4 generator, as` |
|        - |  430 | ` * they are in PHP too — srand does not touch those).` |
|        - |  431 | ` */` |
|        - |  432 | `/*` |
|        - |  433 | ` * Reset the MT19937 state to a 32-bit seed (PHP truncates its int seed likewise).` |
|        - |  434 | ` */` |
|       36 |  435 | `PH7_PRIVATE void PH7_VmMtSrand(ph7_vm *pVm,sxu32 nSeed)` |
|        1 |  436 | `{` |
|       37 |  437 | `	SyMT19937Seed(&pVm->sMt,nSeed);` |
|       37 |  438 | `	pVm->mtSeeded = TRUE;` |
|       37 |  439 | `}` |
|        - |  440 | `/*` |
|        - |  441 | ` * Draw the next full 32-bit MT19937 word, seeding lazily from the OS CSPRNG on` |
|        - |  442 | ` * first use exactly as PHP auto-seeds when rand()/mt_rand() runs before srand().` |
|        - |  443 | ` */` |
|     1781 |  444 | `PH7_PRIVATE sxu32 PH7_VmMtRand(ph7_vm *pVm)` |
|        1 |  445 | `{` |
|     1782 |  446 | `	if( !pVm->mtSeeded ){` |
|        - |  447 | `		sxu32 nSeed;` |
|        3 |  448 | `		if( SyOSCSPRNG((void *)&nSeed,sizeof(nSeed)) != SXRET_OK ){` |
|        - |  449 | `			/* No OS entropy source: fall back to the RC4 generator's output. */` |
|      ! 0 |  450 | `			nSeed = PH7_VmRandomNum(pVm);` |
|      ! 0 |  451 | `		}` |
|        3 |  452 | `		SyMT19937Seed(&pVm->sMt,nSeed);` |
|        3 |  453 | `		pVm->mtSeeded = TRUE;` |
|        1 |  454 | `	}` |
|     1782 |  455 | `	return SyMT19937Next(&pVm->sMt);` |
|        1 |  456 | `}` |
|        - |  457 | `/*` |
|        - |  458 | ` * Map a full 32-bit draw uniformly into [0,uMax] (uMax is the range width, i.e.` |
|        - |  459 | ` * max-min). Rejection sampling against the largest unbiased ceiling, matching` |
|        - |  460 | ` * PHP's php_random_range32().` |
|        - |  461 | ` */` |
|     1658 |  462 | `static sxu32 VmMtRange32(ph7_vm *pVm,sxu32 uMax)` |
|        1 |  463 | `{` |
|        - |  464 | `	sxu32 result,limit;` |
|     1659 |  465 | `	result = PH7_VmMtRand(pVm);` |
|        - |  466 | `	/* Whole 32-bit domain: no scaling needed. */` |
|     1659 |  467 | `	if( uMax == 0xFFFFFFFFU ){` |
|      ! 0 |  468 | `		return result;` |
|        - |  469 | `	}` |
|        - |  470 | `	/* Make the range inclusive of max. */` |
|     1659 |  471 | `	uMax++;` |
|        - |  472 | `	/* Powers of two are unbiased under a plain mask. */` |
|     1659 |  473 | `	if( (uMax & (uMax - 1)) == 0 ){` |
|        7 |  474 | `		return result & (uMax - 1);` |
|        - |  475 | `	}` |
|        - |  476 | `	/* Ceiling under which 0xFFFFFFFF % uMax == 0; discard draws above it. */` |
|     1653 |  477 | `	limit = 0xFFFFFFFFU - (0xFFFFFFFFU % uMax) - 1;` |
|     1653 |  478 | `	while( result > limit ){` |
|      ! 0 |  479 | `		result = PH7_VmMtRand(pVm);` |
|      ! 0 |  480 | `	}` |
|     1653 |  481 | `	return result % uMax;` |
|      830 |  482 | `}` |
|        - |  483 | `/*` |
|        - |  484 | ` * 64-bit-wide range: assemble two draws (high word first, as PHP does) and` |
|        - |  485 | ` * reject-sample. Matches PHP's php_random_range64().` |
|        - |  486 | ` */` |
|        4 |  487 | `static sxu64 VmMtRange64(ph7_vm *pVm,sxu64 uMax)` |
|        1 |  488 | `{` |
|        - |  489 | `	sxu64 result,limit;` |
|        - |  490 | `	/* First draw fills the low word, second draw the high word — order is` |
|        - |  491 | `	 * significant and matches php's php_random_range64() assembly. */` |
|        5 |  492 | `	result = (sxu64)PH7_VmMtRand(pVm);` |
|        5 |  493 | `	result \|= (sxu64)PH7_VmMtRand(pVm) << 32;` |
|        5 |  494 | `	if( uMax == 0xFFFFFFFFFFFFFFFFULL ){` |
|      ! 0 |  495 | `		return result;` |
|        - |  496 | `	}` |
|        5 |  497 | `	uMax++;` |
|        5 |  498 | `	if( (uMax & (uMax - 1)) == 0 ){` |
|        3 |  499 | `		return result & (uMax - 1);` |
|        - |  500 | `	}` |
|        3 |  501 | `	limit = 0xFFFFFFFFFFFFFFFFULL - (0xFFFFFFFFFFFFFFFFULL % uMax) - 1;` |
|        3 |  502 | `	while( result > limit ){` |
|      ! 0 |  503 | `		result = (sxu64)PH7_VmMtRand(pVm);` |
|      ! 0 |  504 | `		result \|= (sxu64)PH7_VmMtRand(pVm) << 32;` |
|      ! 0 |  505 | `	}` |
|        3 |  506 | `	return result % uMax;` |
|        3 |  507 | `}` |
|        - |  508 | `/*` |
|        - |  509 | ` * Return a value uniformly in the inclusive range [iMin,iMax]. The caller` |
|        - |  510 | ` * guarantees iMin <= iMax. Mirrors PHP's php_mt_rand_range(): a range that fits` |
|        - |  511 | ` * in 32 bits takes the 32-bit path, a wider one the 64-bit path.` |
|        - |  512 | ` */` |
|     1662 |  513 | `PH7_PRIVATE sxi64 PH7_VmMtRandRange(ph7_vm *pVm,sxi64 iMin,sxi64 iMax)` |
|        1 |  514 | `{` |
|     1663 |  515 | `	sxu64 uMax = (sxu64)iMax - (sxu64)iMin;` |
|     1663 |  516 | `	if( uMax > 0xFFFFFFFFULL ){` |
|        5 |  517 | `		return (sxi64)(VmMtRange64(pVm,uMax) + (sxu64)iMin);` |
|        - |  518 | `	}` |
|     1659 |  519 | `	return (sxi64)((sxu64)VmMtRange32(pVm,(sxu32)uMax) + (sxu64)iMin);` |
|      832 |  520 | `}` |
|        - |  521 | `/*` |
|        - |  522 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  523 | ` * Note that the generated string is NOT null terminated.` |
|        - |  524 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|        - |  525 | ` * implemented in src/sx/sxrand.c).` |
|        - |  526 | ` */` |
|  2883236 |  527 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        5 |  528 | `{` |
|        - |  529 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  530 | `	int i;` |
|        - |  531 | `	/* Generate a binary string first */` |
|  2883241 |  532 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  533 | `	/* Turn the binary string into english based alphabet */` |
| 31715789 |  534 | `	for( i = 0 ; i < nLen ; ++i ){` |
| 28832553 |  535 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
| 14416279 |  536 | `	 }` |
|  2883241 |  537 | `}` |
|        - |  538 | `/*` |
|        - |  539 | ` * int rand()` |
|        - |  540 | ` * int mt_rand()` |
|        - |  541 | ` * int rand(int $min,int $max)` |
|        - |  542 | ` * int mt_rand(int $min,int $max)` |
|        - |  543 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  544 | ` * Parameter` |
|        - |  545 | ` *  $min` |
|        - |  546 | ` *    The lowest value to return (default: 0)` |
|        - |  547 | ` *  $max` |
|        - |  548 | ` *   The highest value to return (default: getrandmax())` |
|        - |  549 | ` * Return` |
|        - |  550 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  551 | ` * Note:` |
|        - |  552 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  553 | ` *  by te SQLite3 library.` |
|        - |  554 | ` */` |
|     1728 |  555 | `PH7_PRIVATE int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  556 | `{` |
|     1729 |  557 | `	SyString *pName = &pCtx->pFunc->sName;` |
|     3019 |  558 | `	int bMt = (pName->nByte == sizeof("mt_rand")-1` |
|     1728 |  559 | `		&& SyMemcmp(pName->zString,"mt_rand",sizeof("mt_rand")-1) == 0);` |
|        - |  560 | `	/* php accepts exactly 0 or exactly 2 arguments (min,max); 1 or 3+ is an` |
|        - |  561 | `	 * ArgumentCountError. The central arity table can't express "0 or 2", so` |
|        - |  562 | `	 * it is enforced here (was a silent wrong result for the raw draw). */` |
|     1729 |  563 | `	if( nArg == 1 \|\| nArg > 2 ){` |
|       13 |  564 | `		return PH7_VmThrowException(pCtx,` |
|        - |  565 | `			"ArgumentCountError",` |
|        - |  566 | `			"%z() expects exactly 2 arguments, %d given",` |
|        4 |  567 | `			pName, nArg` |
|        - |  568 | `			);` |
|        - |  569 | `	}` |
|     1721 |  570 | `	if( nArg == 2 ){` |
|        - |  571 | `		sxi64 iMin,iMax;` |
|        - |  572 | `		/* Signed 64-bit endpoints: the old unsigned math wrapped negative` |
|        - |  573 | `		 * ranges to huge positives (rand(-10,-1) -> ~4e9) and mis-handled` |
|        - |  574 | `		 * min==max. */` |
|     1659 |  575 | `		iMin = ph7_value_to_int64(apArg[0]);` |
|     1659 |  576 | `		iMax = ph7_value_to_int64(apArg[1]);` |
|     1659 |  577 | `		if( iMin > iMax ){` |
|        9 |  578 | `			if( bMt ){` |
|        - |  579 | `				/* mt_rand() is strict: php throws a catchable ValueError. */` |
|        5 |  580 | `				return PH7_VmThrowException(pCtx,` |
|        - |  581 | `					"ValueError",` |
|        - |  582 | `					"mt_rand(): Argument #2 ($max) must be greater than or equal to argument #1 ($min)"` |
|        - |  583 | `					);` |
|        - |  584 | `			}` |
|        - |  585 | `			/* rand() swaps the bounds for backward compatibility (php keeps` |
|        - |  586 | `			 * this quirk; only mt_rand() rejects a reversed range). */` |
|        5 |  587 | `			{ sxi64 iTmp = iMin; iMin = iMax; iMax = iTmp; }` |
|        2 |  588 | `		}` |
|        - |  589 | `		/* MT19937-backed uniform draw over [iMin,iMax], bit-for-bit as php. */` |
|     1655 |  590 | `		ph7_result_int64(pCtx,PH7_VmMtRandRange(pCtx->pVm,iMin,iMax));` |
|     1655 |  591 | `		return SXRET_OK;` |
|        - |  592 | `	}` |
|        - |  593 | `	/* No-argument form: a 31-bit value in [0, mt_getrandmax()]. php returns` |
|        - |  594 | `	 * php_mt_rand() >> 1 for the bare draw (the full 32-bit word feeds the` |
|        - |  595 | `	 * range form above, but the bare form drops the low bit). */` |
|       63 |  596 | `	ph7_result_int64(pCtx,(sxi64)(PH7_VmMtRand(pCtx->pVm) >> 1));` |
|       63 |  597 | `	return SXRET_OK;` |
|      865 |  598 | `}` |
|        - |  599 | `/*` |
|        - |  600 | ` * int getrandmax(void)` |
|        - |  601 | ` * int mt_getrandmax(void)` |
|        - |  602 | ` * int rc4_getrandmax(void)` |
|        - |  603 | ` *   Show largest possible random value` |
|        - |  604 | ` * Return` |
|        - |  605 | ` *  The largest possible random value returned by rand()/mt_rand(): php's` |
|        - |  606 | ` *  MT19937 backing makes this 2^31-1 (2147483647) for both.` |
|        - |  607 | ` */` |
|        8 |  608 | `PH7_PRIVATE int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  609 | `{` |
|        4 |  610 | `	SXUNUSED(nArg); /* cc warning */` |
|        4 |  611 | `	SXUNUSED(apArg);` |
|        - |  612 | `	/* php: PHP_MT_RAND_MAX == (1<<31)-1; bare rand()/mt_rand() draw >> 1 lands` |
|        - |  613 | `	 * exactly in [0, this]. */` |
|        9 |  614 | `	ph7_result_int64(pCtx,2147483647);` |
|        9 |  615 | `	return SXRET_OK;` |
|        1 |  616 | `}` |
|        - |  617 | `/*` |
|        - |  618 | ` * string rand_str()` |
|        - |  619 | ` * string rand_str(int $len)` |
|        - |  620 | ` *  Generate a random string (English alphabet).` |
|        - |  621 | ` * Parameter` |
|        - |  622 | ` *  $len` |
|        - |  623 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  624 | ` * Return` |
|        - |  625 | ` *   A pseudo random string.` |
|        - |  626 | ` * Note:` |
|        - |  627 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  628 | ` *  by te SQLite3 library.` |
|        - |  629 | ` *  This function is a symisc extension.` |
|        - |  630 | ` */` |
|      130 |  631 | `PH7_PRIVATE int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 |  632 | `{` |
|        - |  633 | `	char zString[1024];` |
|      133 |  634 | `	int iLen = 0x10;` |
|      133 |  635 | `	if( nArg > 0 ){` |
|        - |  636 | `		/* Get the desired length */` |
|      133 |  637 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      133 |  638 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  639 | `			/* Default length */` |
|        3 |  640 | `			iLen = 0x10;` |
|        1 |  641 | `		}` |
|       65 |  642 | `	}` |
|        - |  643 | `	/* Generate the random string */` |
|      133 |  644 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  645 | `	/* Return the generated string */` |
|      133 |  646 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      133 |  647 | `	return SXRET_OK;` |
|        3 |  648 | `}` |
|        - |  649 | `/*` |
|        - |  650 | ` * Reject non-numeric values (array/object/resource and non-numeric strings)` |
|        - |  651 | ` * the same way intdiv() does. Returns SXRET_OK if the value is acceptable as` |
|        - |  652 | ` * an int (PHP coerces float and numeric string silently).` |
|        - |  653 | ` */` |
|      484 |  654 | `static int VmRandomCheckIntArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgPos,const char *zParamName)` |
|        2 |  655 | `{` |
|      484 |  656 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg)` |
|      486 |  657 | `		\|\| ph7_value_is_resource(pArg) ){` |
|      ! 0 |  658 | `		return PH7_VmThrowException(pCtx,` |
|        - |  659 | `			"TypeError",` |
|        - |  660 | `			"%s(): Argument #%d (%s) must be of type int, %s given",` |
|      ! 0 |  661 | `			zFunc,iArgPos,zParamName,` |
|      ! 0 |  662 | `			ph7_type_name(pArg)` |
|        - |  663 | `			);` |
|        - |  664 | `	}` |
|      486 |  665 | `	if( ph7_value_is_string(pArg) ){` |
|        - |  666 | `		int len;` |
|        9 |  667 | `		const char *zStr = ph7_value_to_string(pArg, &len);` |
|        9 |  668 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|        7 |  669 | `			return PH7_VmThrowException(pCtx,` |
|        - |  670 | `				"TypeError",` |
|        - |  671 | `				"%s(): Argument #%d (%s) must be of type int, string given",` |
|        2 |  672 | `				zFunc,iArgPos,zParamName` |
|        - |  673 | `				);` |
|        - |  674 | `		}` |
|        2 |  675 | `	}` |
|      482 |  676 | `	return SXRET_OK;` |
|      244 |  677 | `}` |
|        - |  678 | `/*` |
|        - |  679 | ` * int random_int(int $min, int $max)` |
|        - |  680 | ` *  Generate a cryptographically secure pseudo-random integer in [$min, $max].` |
|        - |  681 | ` *  Mirrors PHP 7.0+ random_int(). Uses the OS CSPRNG via SyOSCSPRNG().` |
|        - |  682 | ` *  Distribution is uniform via rejection sampling against the smallest` |
|        - |  683 | ` *  power-of-two mask covering the range.` |
|        - |  684 | ` */` |
|      232 |  685 | `PH7_PRIVATE int vm_builtin_random_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  686 | `{` |
|        - |  687 | `	sxi64 iMin,iMax;` |
|        - |  688 | `	sxu64 uRange,uMask,uResult;` |
|        - |  689 | `	unsigned int nAttempt;` |
|        - |  690 | `	int rc;` |
|      233 |  691 | `	if( nArg != 2 ){` |
|      ! 0 |  692 | `		return PH7_VmThrowException(pCtx,` |
|        - |  693 | `			"ArgumentCountError",` |
|        - |  694 | `			"random_int() expects exactly 2 arguments, %d given",` |
|      ! 0 |  695 | `			nArg` |
|        - |  696 | `			);` |
|        - |  697 | `	}` |
|      233 |  698 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_int",1,"$min");` |
|      233 |  699 | `	if( rc != SXRET_OK ){ return rc; }` |
|      231 |  700 | `	rc = VmRandomCheckIntArg(pCtx,apArg[1],"random_int",2,"$max");` |
|      231 |  701 | `	if( rc != SXRET_OK ){ return rc; }` |
|      231 |  702 | `	iMin = ph7_value_to_int64(apArg[0]);` |
|      231 |  703 | `	iMax = ph7_value_to_int64(apArg[1]);` |
|      231 |  704 | `	if( iMin > iMax ){` |
|        3 |  705 | `		return PH7_VmThrowException(pCtx,` |
|        - |  706 | `			"ValueError",` |
|        - |  707 | `			"random_int(): Argument #1 ($min) must be less than or equal to argument #2 ($max)"` |
|        - |  708 | `			);` |
|        - |  709 | `	}` |
|      229 |  710 | `	if( iMin == iMax ){` |
|        5 |  711 | `		ph7_result_int64(pCtx,iMin);` |
|        5 |  712 | `		return SXRET_OK;` |
|        - |  713 | `	}` |
|      225 |  714 | `	uRange = (sxu64)iMax - (sxu64)iMin;` |
|      225 |  715 | `	uMask = uRange;` |
|      225 |  716 | `	uMask \|= uMask >> 1;` |
|      225 |  717 | `	uMask \|= uMask >> 2;` |
|      225 |  718 | `	uMask \|= uMask >> 4;` |
|      225 |  719 | `	uMask \|= uMask >> 8;` |
|      225 |  720 | `	uMask \|= uMask >> 16;` |
|      225 |  721 | `	uMask \|= uMask >> 32;` |
|      225 |  722 | `	uResult = 0;` |
|      359 |  723 | `	for( nAttempt = 0 ; nAttempt < 50 ; ++nAttempt ){` |
|        - |  724 | `		/* Always draw a full 8 bytes so endianness of the cast doesn't matter` |
|        - |  725 | `		 * (a 4-byte fill into a sxu64 would land in the high half on big-endian` |
|        - |  726 | `		 * and the low-half mask would always read 0). */` |
|        - |  727 | `		sxu64 uDraw;` |
|      359 |  728 | `		if( SyOSCSPRNG(&uDraw,sizeof(uDraw)) != SXRET_OK ){` |
|      ! 0 |  729 | `			return PH7_VmThrowException(pCtx,` |
|        - |  730 | `				"Random\\RandomException",` |
|        - |  731 | `				"Cannot gather sufficient random data"` |
|        - |  732 | `				);` |
|        - |  733 | `		}` |
|      359 |  734 | `		uDraw &= uMask;` |
|      359 |  735 | `		if( uDraw <= uRange ){` |
|      225 |  736 | `			uResult = uDraw;` |
|      225 |  737 | `			break;` |
|        - |  738 | `		}` |
|       63 |  739 | `	}` |
|      225 |  740 | `	if( nAttempt >= 50 ){` |
|      ! 0 |  741 | `		return PH7_VmThrowException(pCtx,` |
|        - |  742 | `			"Random\\RandomException",` |
|        - |  743 | `			"Cannot gather sufficient random data"` |
|        - |  744 | `			);` |
|        - |  745 | `	}` |
|      225 |  746 | `	ph7_result_int64(pCtx,(sxi64)((sxu64)iMin + uResult));` |
|      225 |  747 | `	return SXRET_OK;` |
|      117 |  748 | `}` |
|        - |  749 | `/*` |
|        - |  750 | ` * string random_bytes(int $length)` |
|        - |  751 | ` *  Generate $length cryptographically secure random bytes via SyOSCSPRNG().` |
|        - |  752 | ` *  Mirrors PHP 7.0+ random_bytes().` |
|        - |  753 | ` */` |
|       22 |  754 | `PH7_PRIVATE int vm_builtin_random_bytes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  755 | `{` |
|        - |  756 | `	sxi64 iLen;` |
|        - |  757 | `	unsigned char zStack[256];` |
|        - |  758 | `	void *pBuf;` |
|        - |  759 | `	int rc;` |
|       24 |  760 | `	int bHeap = 0;` |
|       24 |  761 | `	if( nArg != 1 ){` |
|      ! 0 |  762 | `		return PH7_VmThrowException(pCtx,` |
|        - |  763 | `			"ArgumentCountError",` |
|        - |  764 | `			"random_bytes() expects exactly 1 argument, %d given",` |
|      ! 0 |  765 | `			nArg` |
|        - |  766 | `			);` |
|        - |  767 | `	}` |
|       24 |  768 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_bytes",1,"$length");` |
|       24 |  769 | `	if( rc != SXRET_OK ){ return rc; }` |
|       22 |  770 | `	iLen = ph7_value_to_int64(apArg[0]);` |
|       22 |  771 | `	if( iLen < 1 ){` |
|        5 |  772 | `		return PH7_VmThrowException(pCtx,` |
|        - |  773 | `			"ValueError",` |
|        - |  774 | `			"random_bytes(): Argument #1 ($length) must be greater than 0"` |
|        - |  775 | `			);` |
|        - |  776 | `	}` |
|        - |  777 | `	/* The PH7 allocator and ph7_result_string both take sxu32/int sizes,` |
|        - |  778 | `	 * so we can't honor a length above 2 GiB. Reject early rather than` |
|        - |  779 | `	 * silently truncating via the (sxu32) cast below. */` |
|       18 |  780 | `	if( iLen > 0x7FFFFFFF ){` |
|      ! 0 |  781 | `		return PH7_VmThrowException(pCtx,` |
|        - |  782 | `			"ValueError",` |
|        - |  783 | `			"random_bytes(): Argument #1 ($length) is too large"` |
|        - |  784 | `			);` |
|        - |  785 | `	}` |
|       18 |  786 | `	if( iLen <= (sxi64)sizeof(zStack) ){` |
|       18 |  787 | `		pBuf = zStack;` |
|       10 |  788 | `	}else{` |
|      ! 0 |  789 | `		pBuf = SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)iLen);` |
|      ! 0 |  790 | `		if( pBuf == 0 ){` |
|      ! 0 |  791 | `			return PH7_VmThrowException(pCtx,` |
|        - |  792 | `				"Exception",` |
|        - |  793 | `				"random_bytes(): Failed to allocate %qd bytes",` |
|      ! 0 |  794 | `				iLen` |
|        - |  795 | `				);` |
|        - |  796 | `		}` |
|      ! 0 |  797 | `		bHeap = 1;` |
|        - |  798 | `	}` |
|       18 |  799 | `	if( SyOSCSPRNG(pBuf,(sxu32)iLen) != SXRET_OK ){` |
|      ! 0 |  800 | `		if( bHeap ){` |
|      ! 0 |  801 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 |  802 | `		}` |
|      ! 0 |  803 | `		return PH7_VmThrowException(pCtx,` |
|        - |  804 | `			"Random\\RandomException",` |
|        - |  805 | `			"Cannot gather sufficient random data"` |
|        - |  806 | `			);` |
|        - |  807 | `	}` |
|       18 |  808 | `	ph7_result_string(pCtx,(const char *)pBuf,(int)iLen);` |
|       18 |  809 | `	if( bHeap ){` |
|      ! 0 |  810 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 |  811 | `	}` |
|       18 |  812 | `	return SXRET_OK;` |
|       13 |  813 | `}` |
|        - |  814 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  815 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  816 | `/* Unique ID private data */` |
|        - |  817 | `struct unique_id_data` |
|        - |  818 | `{` |
|        - |  819 | `	ph7_context *pCtx; /* Call context */` |
|        - |  820 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  821 | `};` |
|        - |  822 | `/*` |
|        - |  823 | ` * Binary to hex consumer callback.` |
|        - |  824 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  825 | ` * defined below.` |
|        - |  826 | ` */` |
|      192 |  827 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  828 | `{` |
|      193 |  829 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  830 | `	sxu32 nBuflen;` |
|        - |  831 | `	/* Extract result buffer length */` |
|      193 |  832 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  833 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  834 | `			/*` |
|        - |  835 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  836 | `			 * string will be 13 characters long` |
|        - |  837 | `			 */` |
|       25 |  838 | `		return SXERR_ABORT;` |
|        - |  839 | `	}` |
|      169 |  840 | `	if( nBuflen > 22 ){` |
|      ! 0 |  841 | `		return SXERR_ABORT;` |
|        - |  842 | `	}` |
|        - |  843 | `	/* Safely Consume the hex stream */` |
|      169 |  844 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  845 | `	return SXRET_OK;` |
|       97 |  846 | `}` |
|        - |  847 | `/*` |
|        - |  848 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  849 | ` *  Generate a unique ID` |
|        - |  850 | ` * Parameter` |
|        - |  851 | ` * $prefix` |
|        - |  852 | ` *  Append this prefix to the generated unique ID.` |
|        - |  853 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  854 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  855 | ` * $more_entropy` |
|        - |  856 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  857 | ` *  that the result will be unique.` |
|        - |  858 | ` * Return` |
|        - |  859 | ` *  Returns the unique identifier, as a string.` |
|        - |  860 | ` */` |
|       24 |  861 | `PH7_PRIVATE int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  862 | `{` |
|        - |  863 | `	struct unique_id_data sUniq;` |
|        - |  864 | `	unsigned char zDigest[20];` |
|       25 |  865 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  866 | `	const char *zPrefix;` |
|        - |  867 | `	SHA1Context sCtx;` |
|        - |  868 | `	char zRandom[7];` |
|        - |  869 | `	int nPrefix;` |
|        - |  870 | `	int entropy;` |
|        - |  871 | `	/* Generate a random string first */` |
|       25 |  872 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  873 | `	/* Initialize fields */` |
|       25 |  874 | `	zPrefix = 0;` |
|       25 |  875 | `	nPrefix = 0;` |
|       25 |  876 | `	entropy = 0;` |
|       25 |  877 | `	if( nArg > 0 ){` |
|        - |  878 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  879 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  880 | `		if( nArg > 1 ){` |
|      ! 0 |  881 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  882 | `		}` |
|      ! 0 |  883 | `	}` |
|       25 |  884 | `	SHA1Init(&sCtx);` |
|        - |  885 | `	/* Generate the random ID */` |
|       25 |  886 | `	if( nPrefix > 0 ){` |
|      ! 0 |  887 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  888 | `	}` |
|        - |  889 | `	/* Append the random ID */` |
|       25 |  890 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  891 | `	/* Append the random string */` |
|       25 |  892 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  893 | `	/* Increment the number */` |
|       25 |  894 | `	pVm->unique_id++;` |
|       25 |  895 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  896 | `	/* Hexify the digest */` |
|       25 |  897 | `	sUniq.pCtx = pCtx;` |
|       25 |  898 | `	sUniq.entropy = entropy;` |
|       25 |  899 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  900 | `	/* All done */` |
|       25 |  901 | `	return PH7_OK;` |
|        1 |  902 | `}` |
|        - |  903 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  904 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  905 | `/*` |
|        - |  906 | ` * Section:` |
|        - |  907 | ` *  Language construct implementation as foreign functions.` |
|        - |  908 | ` * Status:` |
|        - |  909 | ` *    Stable.` |
|        - |  910 | ` */` |
|        - |  911 | `/*` |
|        - |  912 | ` * void echo($string...)` |
|        - |  913 | ` *  Output one or more messages.` |
|        - |  914 | ` * Parameters` |
|        - |  915 | ` *  $string` |
|        - |  916 | ` *   Message to output.` |
|        - |  917 | ` * Return` |
|        - |  918 | ` *  NULL.` |
|        - |  919 | ` */` |
|      ! 0 |  920 | `PH7_PRIVATE int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  921 | `{` |
|        - |  922 | `	const char *zData;` |
|      ! 0 |  923 | `	int nDataLen = 0;` |
|        - |  924 | `	ph7_vm *pVm;` |
|        - |  925 | `	int i,rc;` |
|        - |  926 | `	/* Point to the target VM */` |
|      ! 0 |  927 | `	pVm = pCtx->pVm;` |
|        - |  928 | `	/* Output */` |
|      ! 0 |  929 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  930 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  931 | `		if( nDataLen > 0 ){` |
|      ! 0 |  932 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  933 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 |  934 | `			if( rc == SXERR_ABORT ){` |
|        - |  935 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  936 | `				return PH7_ABORT;` |
|        - |  937 | `			}` |
|      ! 0 |  938 | `		}` |
|      ! 0 |  939 | `	}` |
|      ! 0 |  940 | `	return SXRET_OK;` |
|      ! 0 |  941 | `}` |
|        - |  942 | `/*` |
|        - |  943 | ` * int print($string...)` |
|        - |  944 | ` *  Output one or more messages.` |
|        - |  945 | ` * Parameters` |
|        - |  946 | ` *  $string` |
|        - |  947 | ` *   Message to output.` |
|        - |  948 | ` * Return` |
|        - |  949 | ` *  1 always.` |
|        - |  950 | ` */` |
|       34 |  951 | `PH7_PRIVATE int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  952 | `{` |
|        - |  953 | `	const char *zData;` |
|       36 |  954 | `	int nDataLen = 0;` |
|        - |  955 | `	ph7_vm *pVm;` |
|        - |  956 | `	int i,rc;` |
|        - |  957 | `	/* Point to the target VM */` |
|       36 |  958 | `	pVm = pCtx->pVm;` |
|        - |  959 | `	/* Output */` |
|       70 |  960 | `	for( i = 0 ; i < nArg ; ++i ){` |
|       36 |  961 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|       36 |  962 | `		if( nDataLen > 0 ){` |
|       36 |  963 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|       36 |  964 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|       36 |  965 | `			if( rc == SXERR_ABORT ){` |
|        - |  966 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  967 | `				return PH7_ABORT;` |
|        - |  968 | `			}` |
|       17 |  969 | `		}` |
|       19 |  970 | `	}` |
|        - |  971 | `	/* Return 1 */` |
|       36 |  972 | `	ph7_result_int(pCtx,1);` |
|       36 |  973 | `	return SXRET_OK;` |
|       19 |  974 | `}` |
|        - |  975 | `/*` |
|        - |  976 | ` * void exit(string $msg)` |
|        - |  977 | ` * void exit(int $status)` |
|        - |  978 | ` * void die(string $ms)` |
|        - |  979 | ` * void die(int $status)` |
|        - |  980 | ` *   Output a message and terminate program execution.` |
|        - |  981 | ` * Parameter` |
|        - |  982 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  983 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  984 | ` *  and not printed` |
|        - |  985 | ` * Return` |
|        - |  986 | ` *  NULL` |
|        - |  987 | ` */` |
|      ! 0 |  988 | `PH7_PRIVATE int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  989 | `{` |
|      ! 0 |  990 | `	if( nArg > 0 ){` |
|      ! 0 |  991 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  992 | `			const char *zData;` |
|      ! 0 |  993 | `			int iLen = 0;` |
|        - |  994 | `			/* Print exit message */` |
|      ! 0 |  995 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  996 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  997 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  998 | `			sxi32 iExitStatus;` |
|        - |  999 | `			/* Record exit status code */` |
|      ! 0 | 1000 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 | 1001 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 | 1002 | `		}` |
|      ! 0 | 1003 | `	}` |
|        - | 1004 | `	/* Request a VM-wide halt (see PH7_OP_HALT) and abort processing` |
|        - | 1005 | `	 * immediately; the abort unwinds enclosing frames and execution units.` |
|        - | 1006 | `	 */` |
|      ! 0 | 1007 | `	pCtx->pVm->bHaltRequested = 1;` |
|      ! 0 | 1008 | `	return PH7_ABORT;` |
|      ! 0 | 1009 | `}` |
|        - | 1010 | `/*` |
|        - | 1011 | ` * Section:` |
|        - | 1012 | ` *  Version,Credits and Copyright related functions.` |
|        - | 1013 | ` * Status:` |
|        - | 1014 | ` *    Stable.` |
|        - | 1015 | ` */` |
|        - | 1016 | `/*` |
|        - | 1017 | ` * string ph7version(void)` |
|        - | 1018 | ` *  Returns the running version of the PH7 version.` |
|        - | 1019 | ` * Parameters` |
|        - | 1020 | ` *  None` |
|        - | 1021 | ` * Return` |
|        - | 1022 | ` * Current PH7 version.` |
|        - | 1023 | ` */` |
|        2 | 1024 | `PH7_PRIVATE int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1025 | `{` |
|        1 | 1026 | `	SXUNUSED(nArg);` |
|        1 | 1027 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 1028 | `	/* Current engine version */` |
|        3 | 1029 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 1030 | `	return PH7_OK;` |
|        1 | 1031 | `}` |
|        - | 1032 | `/*` |
|        - | 1033 | ` * string phpversion([ string $extension ])` |
|        - | 1034 | ` *  Returns the PHP-compatibility version PHL advertises (see PHP_COMPAT_VERSION).` |
|        - | 1035 | ` * Parameters` |
|        - | 1036 | ` *  $extension (optional): an extension name. PHL has no extension registry, so any` |
|        - | 1037 | ` *  argument yields NULL (PHP returns FALSE for an unknown extension).` |
|        - | 1038 | ` * Return` |
|        - | 1039 | ` *  The PHP-compat version string, or NULL when called with an extension argument.` |
|        - | 1040 | ` */` |
|        4 | 1041 | `PH7_PRIVATE int vm_builtin_phpversion(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1042 | `{` |
|        2 | 1043 | `	SXUNUSED(apArg); /* cc warning */` |
|        5 | 1044 | `	if( nArg > 0 ){` |
|      ! 0 | 1045 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1046 | `		return PH7_OK;` |
|        - | 1047 | `	}` |
|        5 | 1048 | `	ph7_result_string(pCtx,PHP_COMPAT_VERSION,(int)sizeof(PHP_COMPAT_VERSION) - 1);` |
|        5 | 1049 | `	return PH7_OK;` |
|        3 | 1050 | `}` |
|        - | 1051 | `/*` |
|        - | 1052 | ` * string php_sapi_name(void)` |
|        - | 1053 | ` *  Returns the type of interface (SAPI) PHL is running under.` |
|        - | 1054 | ` * Parameters` |
|        - | 1055 | ` *  None` |
|        - | 1056 | ` * Return` |
|        - | 1057 | ` *  "cli-server" while serving an HTTP request via the built-in -S server, "cli" otherwise.` |
|        - | 1058 | ` */` |
|        2 | 1059 | `PH7_PRIVATE int vm_builtin_php_sapi_name(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1060 | `{` |
|        3 | 1061 | `	const char *zSapi = pCtx->pVm->bHttpContext ? "cli-server" : "cli";` |
|        1 | 1062 | `	SXUNUSED(nArg);` |
|        1 | 1063 | `	SXUNUSED(apArg); /* cc warning */` |
|        3 | 1064 | `	ph7_result_string(pCtx,zSapi,-1);` |
|        3 | 1065 | `	return PH7_OK;` |
|        1 | 1066 | `}` |
|        - | 1067 | `/*` |
|        - | 1068 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 1069 | ` */` |
|        - | 1070 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 1071 | ` "<html><head>"\` |
|        - | 1072 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 1073 | ` "<style type=\"text/css\">"\` |
|        - | 1074 | ` "div {"\` |
|        - | 1075 | `     "border: 1px solid #cccccc;"\` |
|        - | 1076 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 1077 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 1078 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 1079 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 1080 | `     "-webkit-border-radius: 10px;"\` |
|        - | 1081 | `     "-o-border-radius: 10px;"\` |
|        - | 1082 | `     "border-radius: 10px;"\` |
|        - | 1083 | `     "padding-left: 2em;"\` |
|        - | 1084 | `     "background-color: white;"\` |
|        - | 1085 | `     "margin-left: auto;"\` |
|        - | 1086 | `     "font-family: verdana;"\` |
|        - | 1087 | `     "padding-right: 2em;"\` |
|        - | 1088 | `     "margin-right: auto;"\` |
|        - | 1089 | `     "}"\` |
|        - | 1090 | `     "body {"\` |
|        - | 1091 | `     "padding: 0.2em;"\` |
|        - | 1092 | `     "font-style: normal;"\` |
|        - | 1093 | `     "font-size: medium;"\` |
|        - | 1094 | `     "background-color: #f2f2f2;"\` |
|        - | 1095 | `     "}"\` |
|        - | 1096 | `     "hr {"\` |
|        - | 1097 | `     "border-style: solid none none;"\` |
|        - | 1098 | `     "border-width: 1px medium medium;"\` |
|        - | 1099 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 1100 | `     "height: 1px;"\` |
|        - | 1101 | `     "}"\` |
|        - | 1102 | `     "a {"\` |
|        - | 1103 | `     "color: #3366cc;"\` |
|        - | 1104 | `     "text-decoration: none;"\` |
|        - | 1105 | `     "}"\` |
|        - | 1106 | `     "a:hover {"\` |
|        - | 1107 | `     "color: #999999;"\` |
|        - | 1108 | `     "}"\` |
|        - | 1109 | `     "a:active {"\` |
|        - | 1110 | `     "color: #663399;"\` |
|        - | 1111 | `     "}"\` |
|        - | 1112 | `     "h1 {"\` |
|        - | 1113 | `     "margin: 0;"\` |
|        - | 1114 | `     "padding: 0;"\` |
|        - | 1115 | `     "font-family: Verdana;"\` |
|        - | 1116 | `     "font-weight: bold;"\` |
|        - | 1117 | `     "font-style: normal;"\` |
|        - | 1118 | `     "font-size: medium;"\` |
|        - | 1119 | `     "text-transform: capitalize;"\` |
|        - | 1120 | `     "color: #0a328c;"\` |
|        - | 1121 | `     "}"\` |
|        - | 1122 | `     "p {"\` |
|        - | 1123 | `     "margin: 0 auto;"\` |
|        - | 1124 | `     "font-size: medium;"\` |
|        - | 1125 | `     "font-style: normal;"\` |
|        - | 1126 | `     "font-family: verdana;"\` |
|        - | 1127 | `     "}"\` |
|        - | 1128 | `"</style></head><body>"\` |
|        - | 1129 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 1130 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 1131 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 1132 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 1133 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 1134 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 1135 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 1136 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 1137 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 1138 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 1139 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 1140 |  |
|        - | 1141 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 1142 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 1143 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 1144 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 1145 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 1146 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 1147 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 1148 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 1149 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 1150 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 1151 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 1152 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 1153 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 1154 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 1155 |  |
|        - | 1156 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 1157 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 1158 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 1159 | `"&nbsp;*<br>"\` |
|        - | 1160 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 1161 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 1162 | `"&nbsp;* are met:<br>"\` |
|        - | 1163 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 1164 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 1165 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 1166 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 1167 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 1168 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 1169 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 1170 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 1171 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 1172 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 1173 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 1174 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 1175 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 1176 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 1177 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 1178 | `"&nbsp;*<br>"\` |
|        - | 1179 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 1180 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 1181 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 1182 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 1183 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 1184 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 1185 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 1186 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 1187 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 1188 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 1189 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 1190 | `"&nbsp;*/<br>"\` |
|        - | 1191 | `"</span></small></small></p>"\` |
|        - | 1192 | `"</div></body></html>"` |
|        - | 1193 | `/*` |
|        - | 1194 | ` * bool ph7credits(void)` |
|        - | 1195 | ` * bool ph7info(void)` |
|        - | 1196 | ` * bool ph7copyright(void)` |
|        - | 1197 | ` *  Prints out the credits for PH7 engine` |
|        - | 1198 | ` * Parameters` |
|        - | 1199 | ` *  None` |
|        - | 1200 | ` * Return` |
|        - | 1201 | ` *  Always TRUE` |
|        - | 1202 | ` */` |
|        2 | 1203 | `PH7_PRIVATE int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1204 | `{` |
|        3 | 1205 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 1206 | `	/* Expand the HTML page above*/` |
|        3 | 1207 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 1208 | `	ph7_context_output_format(` |
|        1 | 1209 | `		pCtx,` |
|        - | 1210 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 1211 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 1212 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 1213 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 1214 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 1215 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 1216 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 1217 | `#ifdef __WINNT__` |
|        - | 1218 | `		"Windows NT"` |
|        - | 1219 | `#elif defined(__UNIXES__)` |
|        - | 1220 | `		"UNIX-Like"` |
|        - | 1221 | `#else` |
|        - | 1222 | `		"Other OS"` |
|        - | 1223 | `#endif` |
|        - | 1224 | `		);` |
|        3 | 1225 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 1226 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 1227 | `	SXUNUSED(apArg);` |
|        - | 1228 | `	/* Return TRUE */` |
|        - | 1229 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 1230 | `	return PH7_OK;` |
|        1 | 1231 | `}` |
|        - | 1232 | `/*` |
|        - | 1233 | ` * Section:` |
|        - | 1234 | ` *    URL related routines.` |
|        - | 1235 | ` * Status:` |
|        - | 1236 | ` *    Stable.` |
|        - | 1237 | ` */` |
|        - | 1238 | `/*` |
|        - | 1239 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 1240 | ` *  Parse a URL and return its fields.` |
|        - | 1241 | ` * Parameters` |
|        - | 1242 | ` *  $url` |
|        - | 1243 | ` *   The URL to parse.` |
|        - | 1244 | ` * $component` |
|        - | 1245 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 1246 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 1247 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 1248 | ` *  in which case the return value will be an integer).` |
|        - | 1249 | ` * Return` |
|        - | 1250 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 1251 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 1252 | ` *  this array are:` |
|        - | 1253 | ` *   scheme - e.g. http` |
|        - | 1254 | ` *   host` |
|        - | 1255 | ` *   port` |
|        - | 1256 | ` *   user` |
|        - | 1257 | ` *   pass` |
|        - | 1258 | ` *   path` |
|        - | 1259 | ` *   query - after the question mark ?` |
|        - | 1260 | ` *   fragment - after the hashmark #` |
|        - | 1261 | ` * Note:` |
|        - | 1262 | ` *  FALSE is returned on failure.` |
|        - | 1263 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 1264 | ` *  with the standard PHP engine.` |
|        - | 1265 | ` */` |
|        - | 1266 | `/*` |
|        - | 1267 | ` * parse_url() component set.` |
|        - | 1268 | ` *` |
|        - | 1269 | ` * A bare SyString cannot express "present but empty", which php needs:` |
|        - | 1270 | ` * parse_url("") is ['path'=>''] and parse_url("?") is ['query'=>''], both` |
|        - | 1271 | ` * distinct from the component being absent. So presence is tracked separately.` |
|        - | 1272 | ` */` |
|        - | 1273 | `typedef struct VmUrlParts VmUrlParts;` |
|        - | 1274 | `struct VmUrlParts` |
|        - | 1275 | `{` |
|        - | 1276 | `	SyString sScheme,sUser,sPass,sHost,sPath,sQuery,sFragment;` |
|        - | 1277 | `	int iPort;     /* Resolved port, meaningful only when bPort is set */` |
|        - | 1278 | `	sxu8 bScheme,bUser,bPass,bHost,bPort,bPath,bQuery,bFragment;` |
|        - | 1279 | `};` |
|      256 | 1280 | `static int VmUrlIsAlnum(int c)` |
|        1 | 1281 | `{` |
|      257 | 1282 | `	return (c >= '0' && c <= '9') \|\| (c >= 'a' && c <= 'z') \|\| (c >= 'A' && c <= 'Z');` |
|        1 | 1283 | `}` |
|        4 | 1284 | `static int VmUrlIsAlpha(int c)` |
|        1 | 1285 | `{` |
|        5 | 1286 | `	return (c >= 'a' && c <= 'z') \|\| (c >= 'A' && c <= 'Z');` |
|        1 | 1287 | `}` |
|        - | 1288 | `/* scheme = ALNUM *( ALNUM / "+" / "-" / "." ) -- php accepts a digit first. */` |
|      256 | 1289 | `static int VmUrlIsSchemeByte(int c)` |
|        1 | 1290 | `{` |
|      257 | 1291 | `	return VmUrlIsAlnum(c) \|\| c == '+' \|\| c == '-' \|\| c == '.';` |
|        1 | 1292 | `}` |
|        - | 1293 | `/*` |
|        - | 1294 | ` * Resolve the port span that followed the ':' in an authority.` |
|        - | 1295 | ` *` |
|        - | 1296 | ` * Returns 1 (usable port in *piPort), 0 (the span is empty, so there is simply` |
|        - | 1297 | ` * no port) or -1 (php rejects the whole URL). php is lenient about what follows` |
|        - | 1298 | ` * the digits -- ":8a" and ":8 0" both yield 8 -- but demands at least one digit` |
|        - | 1299 | ` * and a value that fits a port, so ":abc", ":-80" and ":65536" are all failures.` |
|        - | 1300 | ` */` |
|       42 | 1301 | `static int VmUrlParsePort(const char *z,int n,int *piPort)` |
|        1 | 1302 | `{` |
|       43 | 1303 | `	int i = 0,iVal = 0,nDigit = 0;` |
|       43 | 1304 | `	if( n < 1 ){` |
|      ! 0 | 1305 | `		return 0;` |
|        - | 1306 | `	}` |
|       64 | 1307 | `	while( i < n && (z[i] == ' ' \|\| z[i] == '\t') ){` |
|      ! 0 | 1308 | `		i++;` |
|      ! 0 | 1309 | `	}` |
|       43 | 1310 | `	if( i < n && (z[i] == '+' \|\| z[i] == '-') ){` |
|      ! 0 | 1311 | `		if( z[i] == '-' ){` |
|      ! 0 | 1312 | `			return -1;` |
|        - | 1313 | `		}` |
|      ! 0 | 1314 | `		i++;` |
|      ! 0 | 1315 | `	}` |
|      161 | 1316 | `	while( i < n && z[i] >= '0' && z[i] <= '9' ){` |
|      119 | 1317 | `		iVal = iVal * 10 + (z[i] - '0');` |
|      119 | 1318 | `		if( iVal > 65535 ){` |
|      ! 0 | 1319 | `			return -1; /* Out of range, and this also caps the accumulator */` |
|        - | 1320 | `		}` |
|      119 | 1321 | `		nDigit++;` |
|      119 | 1322 | `		i++;` |
|        1 | 1323 | `	}` |
|       43 | 1324 | `	if( nDigit < 1 ){` |
|        3 | 1325 | `		return -1;` |
|        - | 1326 | `	}` |
|       41 | 1327 | `	*piPort = iVal;` |
|       41 | 1328 | `	return 1;` |
|       22 | 1329 | `}` |
|        - | 1330 | `/*` |
|        - | 1331 | ` * Split an authority -- "[user[:pass]@]host[:port]" -- into pOut.` |
|        - | 1332 | ` * Returns 0 for the malformed input php reports as FALSE.` |
|        - | 1333 | ` */` |
|       66 | 1334 | `static int VmUrlParseAuthority(const char *z,int n,VmUrlParts *pOut,int bPortKnown)` |
|        1 | 1335 | `{` |
|        - | 1336 | `	const char *zHost;` |
|       67 | 1337 | `	int i,iAt = -1,iColon = -1,iSep = -1,nHost,iPort = 0,rc;` |
|        - | 1338 | `	/* php splits the credentials at the LAST '@' ("//a@b@c" is user "a@b") */` |
|      699 | 1339 | `	for( i = 0 ; i < n ; ++i ){` |
|      633 | 1340 | `		if( z[i] == '@' ){` |
|       25 | 1341 | `			iAt = i;` |
|       12 | 1342 | `		}` |
|      317 | 1343 | `	}` |
|       67 | 1344 | `	if( iAt >= 0 ){` |
|        - | 1345 | `		/* and the user from the password at the FIRST ':' before it */` |
|      109 | 1346 | `		for( i = 0 ; i < iAt ; ++i ){` |
|      107 | 1347 | `			if( z[i] == ':' ){` |
|       23 | 1348 | `				iColon = i;` |
|       23 | 1349 | `				break;` |
|        - | 1350 | `			}` |
|       43 | 1351 | `		}` |
|       25 | 1352 | `		if( iColon >= 0 ){` |
|       23 | 1353 | `			SyStringInitFromBuf(&pOut->sUser,z,(sxu32)iColon);` |
|       23 | 1354 | `			SyStringInitFromBuf(&pOut->sPass,&z[iColon+1],(sxu32)(iAt - iColon - 1));` |
|       23 | 1355 | `			pOut->bUser = pOut->bPass = 1;` |
|       12 | 1356 | `		}else{` |
|        3 | 1357 | `			SyStringInitFromBuf(&pOut->sUser,z,(sxu32)iAt);` |
|        3 | 1358 | `			pOut->bUser = 1;` |
|        - | 1359 | `		}` |
|       25 | 1360 | `		z += iAt + 1;` |
|       25 | 1361 | `		n -= iAt + 1;` |
|       12 | 1362 | `	}` |
|       67 | 1363 | `	zHost = z;` |
|       67 | 1364 | `	nHost = n;` |
|       67 | 1365 | `	if( !(n > 1 && z[0] == '[' && z[n-1] == ']') ){` |
|        - | 1366 | `		/* Unless a bracketed IPv6 literal fills the WHOLE authority -- in which` |
|        - | 1367 | `		 * case php keeps the brackets in "host" and scans no port, so every ':'` |
|        - | 1368 | `		 * inside belongs to the address -- the port hangs off the LAST ':'.` |
|        - | 1369 | `		 * php decides that on the first and last byte alone, which is why` |
|        - | 1370 | `		 * "//[:]\|]" is a single host while "//[::1]:8080" splits a port off. */` |
|      507 | 1371 | `		for( i = 0 ; i < n ; ++i ){` |
|      443 | 1372 | `			if( z[i] == ':' ){` |
|       43 | 1373 | `				iSep = i;` |
|       21 | 1374 | `			}` |
|      222 | 1375 | `		}` |
|       65 | 1376 | `		if( iSep >= 0 ){` |
|        - | 1377 | `			/* The host is trimmed at the ':' whether or not the port was already` |
|        - | 1378 | `			 * resolved by the caller. */` |
|       43 | 1379 | `			nHost = iSep;` |
|       43 | 1380 | `			if( !bPortKnown ){` |
|       37 | 1381 | `				rc = VmUrlParsePort(&z[iSep+1],n - iSep - 1,&iPort);` |
|       37 | 1382 | `				if( rc < 0 ){` |
|        3 | 1383 | `					return 0;` |
|        - | 1384 | `				}` |
|       35 | 1385 | `				if( rc > 0 ){` |
|       35 | 1386 | `					pOut->iPort = iPort;` |
|       35 | 1387 | `					pOut->bPort = 1;` |
|       17 | 1388 | `				}` |
|       17 | 1389 | `			}` |
|       20 | 1390 | `		}` |
|       31 | 1391 | `	}` |
|       65 | 1392 | `	if( nHost < 1 ){` |
|        - | 1393 | `		/* php requires a non-empty host once an authority is in play, which is` |
|        - | 1394 | `		 * what makes "//", "http://" and ":80" all FALSE. */` |
|        9 | 1395 | `		return 0;` |
|        - | 1396 | `	}` |
|       57 | 1397 | `	SyStringInitFromBuf(&pOut->sHost,zHost,(sxu32)nHost);` |
|       57 | 1398 | `	pOut->bHost = 1;` |
|       57 | 1399 | `	return 1;` |
|       34 | 1400 | `}` |
|        - | 1401 | `/*` |
|        - | 1402 | ` * Split "path[?query][#fragment]". The fragment is taken FIRST and the query` |
|        - | 1403 | ` * only from what precedes it, so "#a?b" is a fragment of "a?b" with no query.` |
|        - | 1404 | ` */` |
|       80 | 1405 | `static void VmUrlParsePath(const char *z,int n,VmUrlParts *pOut)` |
|        1 | 1406 | `{` |
|       81 | 1407 | `	int i,iEnd = n;` |
|      547 | 1408 | `	for( i = 0 ; i < n ; ++i ){` |
|      501 | 1409 | `		if( z[i] == '#' ){` |
|       35 | 1410 | `			SyStringInitFromBuf(&pOut->sFragment,&z[i+1],(sxu32)(n - i - 1));` |
|       35 | 1411 | `			pOut->bFragment = 1;` |
|       35 | 1412 | `			iEnd = i;` |
|       35 | 1413 | `			break;` |
|        - | 1414 | `		}` |
|      234 | 1415 | `	}` |
|      393 | 1416 | `	for( i = 0 ; i < iEnd ; ++i ){` |
|      347 | 1417 | `		if( z[i] == '?' ){` |
|       35 | 1418 | `			SyStringInitFromBuf(&pOut->sQuery,&z[i+1],(sxu32)(iEnd - i - 1));` |
|       35 | 1419 | `			pOut->bQuery = 1;` |
|       35 | 1420 | `			iEnd = i;` |
|       35 | 1421 | `			break;` |
|        - | 1422 | `		}` |
|      157 | 1423 | `	}` |
|       81 | 1424 | `	if( iEnd > 0 ){` |
|       71 | 1425 | `		SyStringInitFromBuf(&pOut->sPath,z,(sxu32)iEnd);` |
|       71 | 1426 | `		pOut->bPath = 1;` |
|       35 | 1427 | `	}` |
|       81 | 1428 | `}` |
|        - | 1429 | `/* Parse "host[:port]" followed by an optional path/query/fragment. */` |
|       66 | 1430 | `static int VmUrlAuthorityThenPath(const char *z,int n,VmUrlParts *pOut,int bPortKnown)` |
|        1 | 1431 | `{` |
|       67 | 1432 | `	int i,iEnd = n;` |
|      699 | 1433 | `	for( i = 0 ; i < n ; ++i ){` |
|      683 | 1434 | `		if( z[i] == '/' \|\| z[i] == '?' \|\| z[i] == '#' ){` |
|       51 | 1435 | `			iEnd = i;` |
|       51 | 1436 | `			break;` |
|        - | 1437 | `		}` |
|      317 | 1438 | `	}` |
|       67 | 1439 | `	if( !VmUrlParseAuthority(z,iEnd,pOut,bPortKnown) ){` |
|       11 | 1440 | `		return 0;` |
|        - | 1441 | `	}` |
|       57 | 1442 | `	if( iEnd < n ){` |
|       47 | 1443 | `		VmUrlParsePath(&z[iEnd],n - iEnd,pOut);` |
|       23 | 1444 | `	}` |
|       57 | 1445 | `	return 1;` |
|       34 | 1446 | `}` |
|        - | 1447 | `/*` |
|        - | 1448 | ` * Resolve the port php took from the FIRST ':' of a host:port URL.` |
|        - | 1449 | ` *` |
|        - | 1450 | ` * php reads the port straight off that colon before it works out where the host` |
|        - | 1451 | ` * ends, so the digits can even belong to what becomes the path: parse_url()` |
|        - | 1452 | ` * reports port 1 for "a/:1", whose path is "/:1". Mirroring the order keeps` |
|        - | 1453 | ` * that quirk. Returns 0 for a port php rejects.` |
|        - | 1454 | ` */` |
|        6 | 1455 | `static int VmUrlPreparePort(const char *z,int k,int nEnd,VmUrlParts *pOut)` |
|        1 | 1456 | `{` |
|        7 | 1457 | `	int iPort = 0;` |
|        7 | 1458 | `	int rc = VmUrlParsePort(&z[k+1],nEnd - k - 1,&iPort);` |
|        7 | 1459 | `	if( rc < 0 ){` |
|      ! 0 | 1460 | `		return 0;` |
|        - | 1461 | `	}` |
|        7 | 1462 | `	if( rc > 0 ){` |
|        7 | 1463 | `		pOut->iPort = iPort;` |
|        7 | 1464 | `		pOut->bPort = 1;` |
|        3 | 1465 | `	}` |
|        7 | 1466 | `	return 1;` |
|        4 | 1467 | `}` |
|        - | 1468 | `/*` |
|        - | 1469 | ` * php's URL parser, as parse_url() needs it. Returns 0 where php returns FALSE.` |
|        - | 1470 | ` *` |
|        - | 1471 | ` * PH7_VmHttpSplitURI cannot serve here: it is an HTTP REQUEST-target splitter` |
|        - | 1472 | ` * (the HTTP server and filter_var share it) and assumes the input is` |
|        - | 1473 | ` * authority-first, so it read "a" as a host and "mailto:me@x.com" as` |
|        - | 1474 | ` * user:pass@host. php instead decides authority-vs-path up front: only a "//",` |
|        - | 1475 | ` * with or without a scheme before it, introduces an authority.` |
|        - | 1476 | ` */` |
|      104 | 1477 | `static int VmUrlSplit(const char *z,int n,VmUrlParts *pOut)` |
|        1 | 1478 | `{` |
|      105 | 1479 | `	int i,k = -1,bScheme,bPortForm = 0,nPortEnd = 0;` |
|      105 | 1480 | `	SyZero(pOut,sizeof(VmUrlParts));` |
|        - | 1481 | `	/* A leading "//" settles it before any colon is considered: the authority` |
|        - | 1482 | `	 * starts after the slashes. Reading the colon first turned "//h:80" into a` |
|        - | 1483 | `	 * host called "//h" and "//[::1]" into a path. */` |
|      105 | 1484 | `	if( n >= 2 && z[0] == '/' && z[1] == '/' ){` |
|       13 | 1485 | `		return VmUrlAuthorityThenPath(&z[2],n - 2,pOut,0);` |
|        - | 1486 | `	}` |
|      441 | 1487 | `	for( i = 0 ; i < n ; ++i ){` |
|      419 | 1488 | `		if( z[i] == ':' ){` |
|       71 | 1489 | `			k = i;` |
|       71 | 1490 | `			break;` |
|        - | 1491 | `		}` |
|      175 | 1492 | `	}` |
|       93 | 1493 | `	if( k == 0 && n == 1 ){` |
|        - | 1494 | `		/* A lone ":" is an EMPTY scheme, which php rejects outright -- unlike` |
|        - | 1495 | `		 * ":a" or "::", which are simply paths. */` |
|        3 | 1496 | `		return 0;` |
|        - | 1497 | `	}` |
|       91 | 1498 | `	bScheme = k > 0;` |
|      347 | 1499 | `	for( i = 0 ; bScheme && i < k ; ++i ){` |
|      257 | 1500 | `		if( !VmUrlIsSchemeByte((unsigned char)z[i]) ){` |
|      ! 0 | 1501 | `			bScheme = 0;` |
|      ! 0 | 1502 | `		}` |
|      129 | 1503 | `	}` |
|       91 | 1504 | `	if( bScheme && k + 1 == n ){` |
|        - | 1505 | `		/* "x:" -- the scheme is the whole URL */` |
|        3 | 1506 | `		SyStringInitFromBuf(&pOut->sScheme,z,(sxu32)k);` |
|        3 | 1507 | `		pOut->bScheme = 1;` |
|        3 | 1508 | `		return 1;` |
|        - | 1509 | `	}` |
|        - | 1510 | `	/* Decide whether that ':' introduces a PORT rather than a scheme: at least` |
|        - | 1511 | `	 * one digit, running to the end of the string or to a '/'. That is what` |
|        - | 1512 | `	 * makes "a:80" a host:port while "a:80?q" is a scheme with path "80", and` |
|        - | 1513 | `	 * it holds however the ':' is reached -- ":1" and "/:1" are both authorities` |
|        - | 1514 | `	 * with an empty host, which php rejects. php caps the run, so a long number` |
|        - | 1515 | `	 * stays a path: "a:123456" is a path, "a:99999" an out-of-range port. */` |
|       89 | 1516 | `	if( k >= 0 ){` |
|       67 | 1517 | `		int p = k + 1;` |
|       67 | 1518 | `		int bBeforeQuery = 1;` |
|       67 | 1519 | `		nPortEnd = k + 1;` |
|        - | 1520 | `		/* A ':' that sits inside a query or fragment is just data: "?:1" is a` |
|        - | 1521 | `		 * query of ":1", not an authority with an empty host. */` |
|      321 | 1522 | `		for( i = 0 ; i < k ; ++i ){` |
|      255 | 1523 | `			if( z[i] == '?' \|\| z[i] == '#' ){` |
|      ! 0 | 1524 | `				bBeforeQuery = 0;` |
|      ! 0 | 1525 | `				break;` |
|        - | 1526 | `			}` |
|      128 | 1527 | `		}` |
|       77 | 1528 | `		while( p < n && z[p] >= '0' && z[p] <= '9' ){` |
|       11 | 1529 | `			p++;` |
|        1 | 1530 | `		}` |
|       67 | 1531 | `		if( bBeforeQuery && p > k + 1 && (p >= n \|\| z[p] == '/') && (p - k) < 7 ){` |
|        7 | 1532 | `			bPortForm = 1;` |
|        7 | 1533 | `			nPortEnd = p;` |
|        3 | 1534 | `		}` |
|       33 | 1535 | `	}` |
|       89 | 1536 | `	if( !bScheme ){` |
|       25 | 1537 | `		if( bPortForm ){` |
|        3 | 1538 | `			if( !VmUrlPreparePort(z,k,nPortEnd,pOut) ){` |
|      ! 0 | 1539 | `				return 0;` |
|        - | 1540 | `			}` |
|        3 | 1541 | `			return VmUrlAuthorityThenPath(z,n,pOut,1);` |
|        - | 1542 | `		}` |
|       23 | 1543 | `		VmUrlParsePath(z,n,pOut);` |
|       23 | 1544 | `		if( !pOut->bPath && !pOut->bQuery && !pOut->bFragment ){` |
|        - | 1545 | `			/* php reports the empty URL as an empty PATH, not as no components */` |
|        3 | 1546 | `			SyStringInitFromBuf(&pOut->sPath,z,0);` |
|        3 | 1547 | `			pOut->bPath = 1;` |
|        1 | 1548 | `		}` |
|       23 | 1549 | `		return 1;` |
|        - | 1550 | `	}` |
|       65 | 1551 | `	if( bPortForm ){` |
|        5 | 1552 | `		if( !VmUrlPreparePort(z,k,nPortEnd,pOut) ){` |
|      ! 0 | 1553 | `			return 0;` |
|        - | 1554 | `		}` |
|        5 | 1555 | `		return VmUrlAuthorityThenPath(z,n,pOut,1);` |
|        - | 1556 | `	}` |
|       61 | 1557 | `	SyStringInitFromBuf(&pOut->sScheme,z,(sxu32)k);` |
|       61 | 1558 | `	pOut->bScheme = 1;` |
|       61 | 1559 | `	if( z[k+1] == '/' && k + 2 < n && z[k+2] == '/' ){` |
|       52 | 1560 | `		if( k + 3 < n && z[k+3] == '/' && pOut->sScheme.nByte == 4` |
|        4 | 1561 | `		 && (z[0]=='f'\|\|z[0]=='F') && (z[1]=='i'\|\|z[1]=='I')` |
|        5 | 1562 | `		 && (z[2]=='l'\|\|z[2]=='L') && (z[3]=='e'\|\|z[3]=='E') ){` |
|        - | 1563 | `			/* file:/// has no authority: the path starts at the third slash,` |
|        - | 1564 | `			 * except that a "c:" drive letter swallows it (file:///c:/x is the` |
|        - | 1565 | `			 * path "c:/x"). A '\|' in place of the ':' does NOT count. */` |
|        5 | 1566 | `			int iBase = k + 3;` |
|        5 | 1567 | `			if( iBase + 2 < n && VmUrlIsAlpha((unsigned char)z[iBase+1]) && z[iBase+2] == ':' ){` |
|      ! 0 | 1568 | `				iBase++;` |
|      ! 0 | 1569 | `			}` |
|        5 | 1570 | `			VmUrlParsePath(&z[iBase],n - iBase,pOut);` |
|        5 | 1571 | `			return 1;` |
|        - | 1572 | `		}` |
|       49 | 1573 | `		return VmUrlAuthorityThenPath(&z[k+3],n - k - 3,pOut,0);` |
|        - | 1574 | `	}` |
|        - | 1575 | `	/* "mailto:me@x.com", "x:y", "http:/x": everything after the ':' is a path */` |
|        9 | 1576 | `	VmUrlParsePath(&z[k+1],n - k - 1,pOut);` |
|        9 | 1577 | `	return 1;` |
|       53 | 1578 | `}` |
|        - | 1579 | `/*` |
|        - | 1580 | ` * Emit one parsed component into pValue, replacing control bytes with '_'.` |
|        - | 1581 | ` *` |
|        - | 1582 | ` * php runs php_replace_controlchars_ex over every string component it returns,` |
|        - | 1583 | ` * so a URL carrying a raw NUL, newline or DEL cannot smuggle it through into` |
|        - | 1584 | ` * whatever the caller splices the component into (a header, a log line, a` |
|        - | 1585 | ` * redirect). Bytes >= 0x80 are deliberately left alone -- php only folds the` |
|        - | 1586 | ` * ASCII control range.` |
|        - | 1587 | ` */` |
|      164 | 1588 | `static void VmUrlSetComponent(ph7_value *pValue,const SyString *pComp)` |
|        1 | 1589 | `{` |
|      165 | 1590 | `	const char *z = pComp->zString;` |
|      165 | 1591 | `	sxu32 n = pComp->nByte,i,iRun = 0;` |
|      165 | 1592 | `	if( n < 1 \|\| z == 0 ){` |
|        3 | 1593 | `		ph7_value_string(pValue,"",0);` |
|        3 | 1594 | `		return;` |
|        - | 1595 | `	}` |
|      955 | 1596 | `	for( i = 0 ; i < n ; ++i ){` |
|      793 | 1597 | `		unsigned char c = (unsigned char)z[i];` |
|      793 | 1598 | `		if( c < 0x20 \|\| c == 0x7f ){` |
|        3 | 1599 | `			if( i > iRun ){` |
|        3 | 1600 | `				ph7_value_string(pValue,&z[iRun],(int)(i - iRun));` |
|        1 | 1601 | `			}` |
|        3 | 1602 | `			ph7_value_string(pValue,"_",1);` |
|        3 | 1603 | `			iRun = i + 1;` |
|        1 | 1604 | `		}` |
|      397 | 1605 | `	}` |
|      163 | 1606 | `	if( n > iRun ){` |
|      163 | 1607 | `		ph7_value_string(pValue,&z[iRun],(int)(n - iRun));` |
|       81 | 1608 | `	}` |
|       83 | 1609 | `}` |
|      104 | 1610 | `PH7_PRIVATE int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1611 | `{` |
|        - | 1612 | `	const char *zStr; /* Input string */` |
|        - | 1613 | `	VmUrlParts sUrl;  /* Parse of the given URI */` |
|        - | 1614 | `	SyString *pComp;` |
|        - | 1615 | `	int bHave;` |
|        - | 1616 | `	int nLen;` |
|      105 | 1617 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 1618 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 1619 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 1620 | `		return PH7_OK;` |
|        - | 1621 | `	}` |
|        - | 1622 | `	/* Extract the given URI. An empty string is NOT a failure: php parses it as` |
|        - | 1623 | `	 * an empty path. */` |
|      105 | 1624 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|      105 | 1625 | `	if( nLen < 0 ){` |
|      ! 0 | 1626 | `		nLen = 0;` |
|      ! 0 | 1627 | `	}` |
|      105 | 1628 | `	if( !VmUrlSplit(zStr,nLen,&sUrl) ){` |
|        - | 1629 | `		/* Malformed input,return FALSE */` |
|       13 | 1630 | `		ph7_result_bool(pCtx,0);` |
|       13 | 1631 | `		return PH7_OK;` |
|        - | 1632 | `	}` |
|      103 | 1633 | `	if( nArg > 1 && ph7_value_to_int(apArg[1]) >= 0 ){` |
|        - | 1634 | `		/* Refer to constant.c for constants values (php's PHP_URL_* are 0-based;` |
|        - | 1635 | `		 * PHL used to number them from 1, so every literal component id selected` |
|        - | 1636 | `		 * the WRONG field -- and the constants' own tests only asserted "%d",` |
|        - | 1637 | `		 * which any numbering satisfies). A negative id means "the whole array",` |
|        - | 1638 | `		 * which is what the default $component = -1 relies on. */` |
|       27 | 1639 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|       27 | 1640 | `		pComp = 0;` |
|       27 | 1641 | `		bHave = 0;` |
|       27 | 1642 | `		switch(nComponent){` |
|        3 | 1643 | `		case 0: /* PHP_URL_SCHEME */   pComp = &sUrl.sScheme;   bHave = sUrl.bScheme;   break;` |
|        5 | 1644 | `		case 1: /* PHP_URL_HOST */     pComp = &sUrl.sHost;     bHave = sUrl.bHost;     break;` |
|        2 | 1645 | `		case 2: /* PHP_URL_PORT */` |
|        5 | 1646 | `			if( sUrl.bPort ){` |
|        5 | 1647 | `				ph7_result_int(pCtx,sUrl.iPort);` |
|        3 | 1648 | `			}else{` |
|      ! 0 | 1649 | `				ph7_result_null(pCtx);` |
|        - | 1650 | `			}` |
|        5 | 1651 | `			return PH7_OK;` |
|        3 | 1652 | `		case 3: /* PHP_URL_USER */     pComp = &sUrl.sUser;     bHave = sUrl.bUser;     break;` |
|        3 | 1653 | `		case 4: /* PHP_URL_PASS */     pComp = &sUrl.sPass;     bHave = sUrl.bPass;     break;` |
|        3 | 1654 | `		case 5: /* PHP_URL_PATH */     pComp = &sUrl.sPath;     bHave = sUrl.bPath;     break;` |
|        5 | 1655 | `		case 6: /* PHP_URL_QUERY */    pComp = &sUrl.sQuery;    bHave = sUrl.bQuery;    break;` |
|        5 | 1656 | `		case 7: /* PHP_URL_FRAGMENT */ pComp = &sUrl.sFragment; bHave = sUrl.bFragment; break;` |
|        1 | 1657 | `		default:` |
|        4 | 1658 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|        - | 1659 | `				"parse_url(): Argument #2 ($component) must be a valid URL component identifier, %d given",` |
|        1 | 1660 | `				nComponent);` |
|        - | 1661 | `		}` |
|       21 | 1662 | `		if( bHave ){` |
|       19 | 1663 | `			ph7_value *pOut = ph7_context_new_scalar(pCtx);` |
|       19 | 1664 | `			if( pOut == 0 ){` |
|      ! 0 | 1665 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|      ! 0 | 1666 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 1667 | `				return PH7_OK;` |
|        - | 1668 | `			}` |
|       19 | 1669 | `			VmUrlSetComponent(pOut,pComp);` |
|       19 | 1670 | `			ph7_result_value(pCtx,pOut);` |
|       10 | 1671 | `		}else{` |
|        - | 1672 | `			/* No available value,return NULL */` |
|        3 | 1673 | `			ph7_result_null(pCtx);` |
|        - | 1674 | `		}` |
|       11 | 1675 | `	}else{` |
|        - | 1676 | `		ph7_value *pArray,*pValue;` |
|        - | 1677 | `		/* Return an associative array */` |
|       67 | 1678 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       67 | 1679 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       67 | 1680 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 1681 | `			/* Out of memory */` |
|      ! 0 | 1682 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 1683 | `			/* Return false */` |
|      ! 0 | 1684 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 1685 | `			return PH7_OK;` |
|        - | 1686 | `		}` |
|        - | 1687 | `		/* Fill the array, in php's key order. A component is emitted whenever it` |
|        - | 1688 | `		 * is PRESENT, even when empty -- parse_url("?") is ['query'=>'']. */` |
|       67 | 1689 | `		if( sUrl.bScheme ){` |
|       33 | 1690 | `			VmUrlSetComponent(pValue,&sUrl.sScheme);` |
|       33 | 1691 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|       33 | 1692 | `			ph7_value_reset_string_cursor(pValue);` |
|       16 | 1693 | `		}` |
|       67 | 1694 | `		if( sUrl.bHost ){` |
|       31 | 1695 | `			VmUrlSetComponent(pValue,&sUrl.sHost);` |
|       31 | 1696 | `			ph7_array_add_strkey_elem(pArray,"host",pValue);` |
|       31 | 1697 | `			ph7_value_reset_string_cursor(pValue);` |
|       15 | 1698 | `		}` |
|       67 | 1699 | `		if( sUrl.bPort ){` |
|       17 | 1700 | `			ph7_value_int(pValue,sUrl.iPort);` |
|       17 | 1701 | `			ph7_array_add_strkey_elem(pArray,"port",pValue);` |
|       17 | 1702 | `			ph7_value_reset_string_cursor(pValue);` |
|        8 | 1703 | `		}` |
|       67 | 1704 | `		if( sUrl.bUser ){` |
|        9 | 1705 | `			VmUrlSetComponent(pValue,&sUrl.sUser);` |
|        9 | 1706 | `			ph7_array_add_strkey_elem(pArray,"user",pValue);` |
|        9 | 1707 | `			ph7_value_reset_string_cursor(pValue);` |
|        4 | 1708 | `		}` |
|       67 | 1709 | `		if( sUrl.bPass ){` |
|        7 | 1710 | `			VmUrlSetComponent(pValue,&sUrl.sPass);` |
|        7 | 1711 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue);` |
|        7 | 1712 | `			ph7_value_reset_string_cursor(pValue);` |
|        3 | 1713 | `		}` |
|       67 | 1714 | `		if( sUrl.bPath ){` |
|       47 | 1715 | `			VmUrlSetComponent(pValue,&sUrl.sPath);` |
|       47 | 1716 | `			ph7_array_add_strkey_elem(pArray,"path",pValue);` |
|       47 | 1717 | `			ph7_value_reset_string_cursor(pValue);` |
|       23 | 1718 | `		}` |
|       67 | 1719 | `		if( sUrl.bQuery ){` |
|       13 | 1720 | `			VmUrlSetComponent(pValue,&sUrl.sQuery);` |
|       13 | 1721 | `			ph7_array_add_strkey_elem(pArray,"query",pValue);` |
|       13 | 1722 | `			ph7_value_reset_string_cursor(pValue);` |
|        6 | 1723 | `		}` |
|       67 | 1724 | `		if( sUrl.bFragment ){` |
|       13 | 1725 | `			VmUrlSetComponent(pValue,&sUrl.sFragment);` |
|       13 | 1726 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue);` |
|        6 | 1727 | `		}` |
|        - | 1728 | `		/* Return the created array */` |
|       67 | 1729 | `		ph7_result_value(pCtx,pArray);` |
|        - | 1730 | `		/* NOTE:` |
|        - | 1731 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 1732 | `		 * automatically as soon we return from this function.` |
|        - | 1733 | `		 */` |
|        - | 1734 | `	}` |
|        - | 1735 | `	/* All done */` |
|       87 | 1736 | `	return PH7_OK;` |
|       53 | 1737 | `}` |
|        - | 1738 |  |
|        - | 1739 | `/*` |
|        - | 1740 | ` * Section:` |
|        - | 1741 | ` *   Array related routines.` |
|        - | 1742 | ` * Status:` |
|        - | 1743 | ` *    Stable.` |
|        - | 1744 | ` * Note 2012-5-21 01:04:15:` |
|        - | 1745 | ` *  Array related functions that need access to the underlying` |
|        - | 1746 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 1747 | ` */` |
|        - | 1748 | `/*` |
|        - | 1749 | ` * The [compact()] function store it's state information in an instance` |
|        - | 1750 | ` * of the following structure.` |
|        - | 1751 | ` */` |
|        - | 1752 | `struct compact_data` |
|        - | 1753 | `{` |
|        - | 1754 | `	ph7_value *pArray;  /* Target array */` |
|        - | 1755 | `	int nRecCount;      /* Recursion count */` |
|        - | 1756 | `};` |
|        - | 1757 | `/*` |
|        - | 1758 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 1759 | ` */` |
|      ! 0 | 1760 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 1761 | `{` |
|      ! 0 | 1762 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 1763 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 1764 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 1765 | `	/* Act according to the hashmap value */` |
|      ! 0 | 1766 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 1767 | `		SyString sVar;` |
|      ! 0 | 1768 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 1769 | `		if( sVar.nByte > 0 ){` |
|        - | 1770 | `			/* Query the current frame */` |
|      ! 0 | 1771 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 1772 | `			/* ^` |
|        - | 1773 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 1774 | `			 */` |
|      ! 0 | 1775 | `			if( pKey ){` |
|        - | 1776 | `				/* Perform the insertion */` |
|      ! 0 | 1777 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 1778 | `			}` |
|      ! 0 | 1779 | `		}` |
|      ! 0 | 1780 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 1781 | `		int rc;` |
|        - | 1782 | `		/* Recursively traverse this array */` |
|      ! 0 | 1783 | `		pData->nRecCount++;` |
|      ! 0 | 1784 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 1785 | `		pData->nRecCount--;` |
|      ! 0 | 1786 | `		return rc;` |
|        - | 1787 | `	}` |
|      ! 0 | 1788 | `	return SXRET_OK;` |
|      ! 0 | 1789 | `}` |
|        - | 1790 | `/*` |
|        - | 1791 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 1792 | ` *  Create array containing variables and their values.` |
|        - | 1793 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 1794 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 1795 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 1796 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 1797 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 1798 | ` * Parameters` |
|        - | 1799 | ` *  $varname` |
|        - | 1800 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 1801 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 1802 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 1803 | ` *   it recursively.` |
|        - | 1804 | ` * Return` |
|        - | 1805 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 1806 | ` */` |
|        2 | 1807 | `PH7_PRIVATE int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1808 | `{` |
|        - | 1809 | `	ph7_value *pArray,*pObj;` |
|        3 | 1810 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 1811 | `	const char *zName;` |
|        - | 1812 | `	SyString sVar;` |
|        - | 1813 | `	int i,nLen;` |
|        3 | 1814 | `	if( nArg < 1 ){` |
|        - | 1815 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 1816 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1817 | `		return PH7_OK;` |
|        - | 1818 | `	}` |
|        - | 1819 | `	/* Create the array */` |
|        3 | 1820 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 1821 | `	if( pArray == 0 ){` |
|        - | 1822 | `		/* Out of memory */` |
|      ! 0 | 1823 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 1824 | `		/* Return NULL */` |
|      ! 0 | 1825 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1826 | `		return PH7_OK;` |
|        - | 1827 | `	}` |
|        - | 1828 | `	/* Perform the requested operation */` |
|        7 | 1829 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 1830 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 1831 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 1832 | `				struct compact_data sData;` |
|      ! 0 | 1833 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 1834 | `				/* Recursively walk the array */` |
|      ! 0 | 1835 | `				sData.nRecCount = 0;` |
|      ! 0 | 1836 | `				sData.pArray = pArray;` |
|      ! 0 | 1837 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 1838 | `			}` |
|      ! 0 | 1839 | `		}else{` |
|        - | 1840 | `			/* Extract variable name */` |
|        5 | 1841 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 1842 | `			if( nLen > 0 ){` |
|        5 | 1843 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 1844 | `				/* Check if the variable is available in the current frame */` |
|        5 | 1845 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 1846 | `				if( pObj ){` |
|        5 | 1847 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 1848 | `				}` |
|        2 | 1849 | `			}` |
|        - | 1850 | `		}` |
|        3 | 1851 | `	}` |
|        - | 1852 | `	/* Return the array */` |
|        3 | 1853 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 1854 | `	return PH7_OK;` |
|        2 | 1855 | `}` |
|        - | 1856 | `/*` |
|        - | 1857 | ` * The [extract()] function store it's state information in an instance` |
|        - | 1858 | ` * of the following structure.` |
|        - | 1859 | ` */` |
|        - | 1860 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 1861 | `struct extract_aux_data` |
|        - | 1862 | `{` |
|        - | 1863 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 1864 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 1865 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 1866 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 1867 | `	int iFlags;           /* Control flags */` |
|        - | 1868 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 1869 | `};` |
|        - | 1870 | `/* Forward declaration */` |
|        - | 1871 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 1872 | `/*` |
|        - | 1873 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 1874 | ` *   Import variables into the current symbol table from an array.` |
|        - | 1875 | ` * Parameters` |
|        - | 1876 | ` * $var_array` |
|        - | 1877 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 1878 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 1879 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 1880 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 1881 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 1882 | ` * $extract_type` |
|        - | 1883 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 1884 | ` *  It can be one of the following values:` |
|        - | 1885 | ` *   EXTR_OVERWRITE` |
|        - | 1886 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 1887 | ` *   EXTR_SKIP` |
|        - | 1888 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 1889 | ` *   EXTR_PREFIX_SAME` |
|        - | 1890 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 1891 | ` *   EXTR_PREFIX_ALL` |
|        - | 1892 | ` *       Prefix all variable names with prefix.` |
|        - | 1893 | ` *   EXTR_PREFIX_INVALID` |
|        - | 1894 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 1895 | ` *   EXTR_IF_EXISTS` |
|        - | 1896 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 1897 | ` *       otherwise do nothing.` |
|        - | 1898 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 1899 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 1900 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 1901 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 1902 | ` *      the current symbol table.` |
|        - | 1903 | ` * $prefix` |
|        - | 1904 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 1905 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 1906 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 1907 | ` *  underscore character.` |
|        - | 1908 | ` * Return` |
|        - | 1909 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 1910 | ` */` |
|        4 | 1911 | `PH7_PRIVATE int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1912 | `{` |
|        - | 1913 | `	extract_aux_data sAux;` |
|        - | 1914 | `	ph7_hashmap *pMap;` |
|        5 | 1915 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 1916 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 1917 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 1918 | `		return PH7_OK;` |
|        - | 1919 | `	}` |
|        - | 1920 | `	/* Point to the target hashmap */` |
|        5 | 1921 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 1922 | `	if( pMap->nEntry < 1 ){` |
|        - | 1923 | `		/* Empty map,return  0 */` |
|      ! 0 | 1924 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 1925 | `		return PH7_OK;` |
|        - | 1926 | `	}` |
|        - | 1927 | `	/* Prepare the aux data */` |
|        5 | 1928 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 1929 | `	if( nArg > 1 ){` |
|        3 | 1930 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 1931 | `		if( nArg > 2 ){` |
|      ! 0 | 1932 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 1933 | `		}` |
|        1 | 1934 | `	}` |
|        5 | 1935 | `	sAux.pVm = pCtx->pVm;` |
|        - | 1936 | `	/* Invoke the worker callback */` |
|        5 | 1937 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 1938 | `	/* Number of variables successfully imported */` |
|        5 | 1939 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 1940 | `	return PH7_OK;` |
|        3 | 1941 | `}` |
|        - | 1942 | `/*` |
|        - | 1943 | ` * Worker callback for the [extract()] function defined` |
|        - | 1944 | ` * below.` |
|        - | 1945 | ` */` |
|        8 | 1946 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 1947 | `{` |
|        9 | 1948 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 1949 | `	int iFlags = pAux->iFlags;` |
|        9 | 1950 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 1951 | `	ph7_value *pObj;` |
|        - | 1952 | `	SyString sVar;` |
|        9 | 1953 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 1954 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 1955 | `	}` |
|        - | 1956 | `	/* Perform a string cast */` |
|        9 | 1957 | `	PH7_MemObjToString(pKey);` |
|        9 | 1958 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 1959 | `		/* Unavailable variable name */` |
|      ! 0 | 1960 | `		return SXRET_OK;` |
|        - | 1961 | `	}` |
|        9 | 1962 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 1963 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 1964 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 1965 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 1966 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 1967 | `			);` |
|      ! 0 | 1968 | `	}else{` |
|       13 | 1969 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 1970 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 1971 | `	}` |
|        9 | 1972 | `	sVar.zString = pAux->zWorker;` |
|        - | 1973 | `	/* Try to extract the variable */` |
|        9 | 1974 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 1975 | `	if( pObj ){` |
|        - | 1976 | `		/* Collision */` |
|        3 | 1977 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 1978 | `			return SXRET_OK;` |
|        - | 1979 | `		}` |
|        3 | 1980 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 1981 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 1982 | `				/* Already prefixed */` |
|      ! 0 | 1983 | `				return SXRET_OK;` |
|        - | 1984 | `			}` |
|      ! 0 | 1985 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 1986 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 1987 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 1988 | `				);` |
|      ! 0 | 1989 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 1990 | `		}` |
|        2 | 1991 | `	}else{` |
|        - | 1992 | `		/* Create the variable */` |
|        7 | 1993 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 1994 | `	}` |
|        9 | 1995 | `	if( pObj ){` |
|        - | 1996 | `		/* Overwrite the old value */` |
|        9 | 1997 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 1998 | `		/* Increment counter */` |
|        9 | 1999 | `		pAux->iCount++;` |
|        4 | 2000 | `	}` |
|        9 | 2001 | `	return SXRET_OK;` |
|        5 | 2002 | `}` |
|        - | 2003 | `/*` |
|        - | 2004 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 2005 | ` * defined below.` |
|        - | 2006 | ` */` |
|        2 | 2007 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 2008 | `{` |
|        3 | 2009 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 2010 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 2011 | `	ph7_value *pObj;` |
|        - | 2012 | `	SyString sVar;` |
|        - | 2013 | `	/* Perform a string cast */` |
|        3 | 2014 | `	PH7_MemObjToString(pKey);` |
|        3 | 2015 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 2016 | `		/* Unavailable variable name */` |
|      ! 0 | 2017 | `		return SXRET_OK;` |
|        - | 2018 | `	}` |
|        3 | 2019 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 2020 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 2021 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 2022 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 2023 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 2024 | `			);` |
|        2 | 2025 | `	}else{` |
|      ! 0 | 2026 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 2027 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 2028 | `	}` |
|        3 | 2029 | `	sVar.zString = pAux->zWorker;` |
|        - | 2030 | `	/* Extract the variable */` |
|        3 | 2031 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 2032 | `	if( pObj ){` |
|        3 | 2033 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 2034 | `	}` |
|        3 | 2035 | `	return SXRET_OK;` |
|        2 | 2036 | `}` |
|        - | 2037 | `/*` |
|        - | 2038 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 2039 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 2040 | ` * Parameters` |
|        - | 2041 | ` * $types` |
|        - | 2042 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 2043 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 2044 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 2045 | ` *  POST includes the POST uploaded file information.` |
|        - | 2046 | ` *  Note:` |
|        - | 2047 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 2048 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 2049 | ` * $prefix` |
|        - | 2050 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 2051 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 2052 | ` *  variable named $pref_userid.` |
|        - | 2053 | ` * Return` |
|        - | 2054 | ` *  TRUE on success or FALSE on failure.` |
|        - | 2055 | ` */` |
|        2 | 2056 | `PH7_PRIVATE int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 2057 | `{` |
|        - | 2058 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 2059 | `	extract_aux_data sAux;` |
|        - | 2060 | `	int nLen,nPrefixLen;` |
|        - | 2061 | `	ph7_value *pSuper;` |
|        - | 2062 | `	ph7_vm *pVm;` |
|        - | 2063 | `	/* By default import only $_GET variables  */` |
|        3 | 2064 | `	zImport = "G";` |
|        3 | 2065 | `	nLen = (int)sizeof(char);` |
|        3 | 2066 | `	zPrefix = 0;` |
|        3 | 2067 | `	nPrefixLen = 0;` |
|        3 | 2068 | `	if( nArg > 0 ){` |
|        3 | 2069 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 2070 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 2071 | `		}` |
|        3 | 2072 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 2073 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 2074 | `		}` |
|        1 | 2075 | `	}` |
|        - | 2076 | `	/* Point to the underlying VM */` |
|        3 | 2077 | `	pVm = pCtx->pVm;` |
|        - | 2078 | `	/* Initialize the aux data */` |
|        3 | 2079 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 2080 | `	sAux.zPrefix = zPrefix;` |
|        3 | 2081 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 2082 | `	sAux.pVm = pVm;` |
|        - | 2083 | `	/* Extract */` |
|        3 | 2084 | `	zEnd = &zImport[nLen];` |
|        5 | 2085 | `	while( zImport < zEnd ){` |
|        3 | 2086 | `		int c = zImport[0];` |
|        3 | 2087 | `		pSuper = 0;` |
|        3 | 2088 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 2089 | `			/* Import $_GET variables */` |
|        3 | 2090 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 2091 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 2092 | `			/* Import $_POST variables */` |
|      ! 0 | 2093 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 2094 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 2095 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 2096 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 2097 | `		}` |
|        3 | 2098 | `		if( pSuper ){` |
|        - | 2099 | `			/* Iterate throw array entries */` |
|        3 | 2100 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 2101 | `		}` |
|        - | 2102 | `		/* Advance the cursor */` |
|        3 | 2103 | `		zImport++;` |
|        1 | 2104 | `	}` |
|        - | 2105 | `	/* All done,return TRUE*/` |
|        3 | 2106 | `	ph7_result_bool(pCtx,0);` |
|        3 | 2107 | `	return PH7_OK;` |
|        1 | 2108 | `}` |
|        - | 2109 |  |
