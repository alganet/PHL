# src/ph7/vm_builtin_lang.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 779/993 lines (78.45%)

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
|     5104 |  419 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        5 |  420 | `{` |
|        - |  421 | `	sxu32 iNum;` |
|     5109 |  422 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     5109 |  423 | `	return iNum;` |
|        5 |  424 | `}` |
|        - |  425 | `/*` |
|        - |  426 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  427 | ` * Note that the generated string is NOT null terminated.` |
|        - |  428 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|        - |  429 | ` * implemented in src/sx/sxrand.c).` |
|        - |  430 | ` */` |
|  2899648 |  431 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        5 |  432 | `{` |
|        - |  433 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  434 | `	int i;` |
|        - |  435 | `	/* Generate a binary string first */` |
|  2899653 |  436 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  437 | `	/* Turn the binary string into english based alphabet */` |
| 31896321 |  438 | `	for( i = 0 ; i < nLen ; ++i ){` |
| 28996673 |  439 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
| 14498339 |  440 | `	 }` |
|  2899653 |  441 | `}` |
|        - |  442 | `/*` |
|        - |  443 | ` * int rand()` |
|        - |  444 | ` * int mt_rand()` |
|        - |  445 | ` * int rand(int $min,int $max)` |
|        - |  446 | ` * int mt_rand(int $min,int $max)` |
|        - |  447 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  448 | ` * Parameter` |
|        - |  449 | ` *  $min` |
|        - |  450 | ` *    The lowest value to return (default: 0)` |
|        - |  451 | ` *  $max` |
|        - |  452 | ` *   The highest value to return (default: getrandmax())` |
|        - |  453 | ` * Return` |
|        - |  454 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  455 | ` * Note:` |
|        - |  456 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  457 | ` *  by te SQLite3 library.` |
|        - |  458 | ` */` |
|     1642 |  459 | `PH7_PRIVATE int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  460 | `{` |
|     1643 |  461 | `	SyString *pName = &pCtx->pFunc->sName;` |
|     2871 |  462 | `	int bMt = (pName->nByte == sizeof("mt_rand")-1` |
|     1642 |  463 | `		&& SyMemcmp(pName->zString,"mt_rand",sizeof("mt_rand")-1) == 0);` |
|        - |  464 | `	sxu32 iNum;` |
|        - |  465 | `	/* php accepts exactly 0 or exactly 2 arguments (min,max); 1 or 3+ is an` |
|        - |  466 | `	 * ArgumentCountError. The central arity table can't express "0 or 2", so` |
|        - |  467 | `	 * it is enforced here (was a silent wrong result for the raw draw). */` |
|     1643 |  468 | `	if( nArg == 1 \|\| nArg > 2 ){` |
|       13 |  469 | `		return PH7_VmThrowException(pCtx,` |
|        - |  470 | `			"ArgumentCountError",` |
|        - |  471 | `			"%z() expects exactly 2 arguments, %d given",` |
|        4 |  472 | `			pName, nArg` |
|        - |  473 | `			);` |
|        - |  474 | `	}` |
|        - |  475 | `	/* Generate the random number */` |
|     1635 |  476 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|     1635 |  477 | `	if( nArg == 2 ){` |
|        - |  478 | `		sxi64 iMin,iMax;` |
|        - |  479 | `		sxu64 iSpan;` |
|        - |  480 | `		/* Signed 64-bit endpoints: the old unsigned math wrapped negative` |
|        - |  481 | `		 * ranges to huge positives (rand(-10,-1) -> ~4e9) and mis-handled` |
|        - |  482 | `		 * min==max. */` |
|     1611 |  483 | `		iMin = ph7_value_to_int64(apArg[0]);` |
|     1611 |  484 | `		iMax = ph7_value_to_int64(apArg[1]);` |
|     1611 |  485 | `		if( iMin > iMax ){` |
|        5 |  486 | `			if( bMt ){` |
|        - |  487 | `				/* mt_rand() is strict: php throws a catchable ValueError. */` |
|        3 |  488 | `				return PH7_VmThrowException(pCtx,` |
|        - |  489 | `					"ValueError",` |
|        - |  490 | `					"mt_rand(): Argument #2 ($max) must be greater than or equal to argument #1 ($min)"` |
|        - |  491 | `					);` |
|        - |  492 | `			}` |
|        - |  493 | `			/* rand() swaps the bounds for backward compatibility (php keeps` |
|        - |  494 | `			 * this quirk; only mt_rand() rejects a reversed range). */` |
|        3 |  495 | `			{ sxi64 iTmp = iMin; iMin = iMax; iMax = iTmp; }` |
|        1 |  496 | `		}` |
|        - |  497 | `		/* Map the draw into [iMin,iMax] inclusive using a 64-bit span so a` |
|        - |  498 | `		 * full-width range never overflows. Subtract in unsigned space: the` |
|        - |  499 | `		 * signed (iMax-iMin) would overflow for a range wider than 2^63` |
|        - |  500 | `		 * (up to the full PHP_INT domain), which is C undefined behavior. */` |
|     1609 |  501 | `		iSpan = ((sxu64)iMax - (sxu64)iMin) + 1;` |
|     1609 |  502 | `		if( iSpan == 0 ){` |
|        - |  503 | `			/* Range spans the entire 64-bit domain (PHP_INT_MIN..PHP_INT_MAX). */` |
|      ! 0 |  504 | `			ph7_result_int64(pCtx,(sxi64)((sxu64)iMin + (sxu64)iNum));` |
|      ! 0 |  505 | `			return SXRET_OK;` |
|        - |  506 | `		}` |
|     1609 |  507 | `		ph7_result_int64(pCtx,(sxi64)((sxu64)iMin + (iNum % iSpan)));` |
|     1609 |  508 | `		return SXRET_OK;` |
|        - |  509 | `	}` |
|        - |  510 | `	/* No-argument form: return the raw draw */` |
|       25 |  511 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       25 |  512 | `	return SXRET_OK;` |
|      822 |  513 | `}` |
|        - |  514 | `/*` |
|        - |  515 | ` * int getrandmax(void)` |
|        - |  516 | ` * int mt_getrandmax(void)` |
|        - |  517 | ` * int rc4_getrandmax(void)` |
|        - |  518 | ` *   Show largest possible random value` |
|        - |  519 | ` * Return` |
|        - |  520 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  521 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  522 | ` * Note:` |
|        - |  523 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  524 | ` *  by te SQLite3 library.` |
|        - |  525 | ` */` |
|        4 |  526 | `PH7_PRIVATE int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  527 | `{` |
|        2 |  528 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  529 | `	SXUNUSED(apArg);` |
|        5 |  530 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  531 | `	return SXRET_OK;` |
|        1 |  532 | `}` |
|        - |  533 | `/*` |
|        - |  534 | ` * string rand_str()` |
|        - |  535 | ` * string rand_str(int $len)` |
|        - |  536 | ` *  Generate a random string (English alphabet).` |
|        - |  537 | ` * Parameter` |
|        - |  538 | ` *  $len` |
|        - |  539 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  540 | ` * Return` |
|        - |  541 | ` *   A pseudo random string.` |
|        - |  542 | ` * Note:` |
|        - |  543 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  544 | ` *  by te SQLite3 library.` |
|        - |  545 | ` *  This function is a symisc extension.` |
|        - |  546 | ` */` |
|      130 |  547 | `PH7_PRIVATE int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 |  548 | `{` |
|        - |  549 | `	char zString[1024];` |
|      133 |  550 | `	int iLen = 0x10;` |
|      133 |  551 | `	if( nArg > 0 ){` |
|        - |  552 | `		/* Get the desired length */` |
|      133 |  553 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      133 |  554 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  555 | `			/* Default length */` |
|        3 |  556 | `			iLen = 0x10;` |
|        1 |  557 | `		}` |
|       65 |  558 | `	}` |
|        - |  559 | `	/* Generate the random string */` |
|      133 |  560 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  561 | `	/* Return the generated string */` |
|      133 |  562 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      133 |  563 | `	return SXRET_OK;` |
|        3 |  564 | `}` |
|        - |  565 | `/*` |
|        - |  566 | ` * Reject non-numeric values (array/object/resource and non-numeric strings)` |
|        - |  567 | ` * the same way intdiv() does. Returns SXRET_OK if the value is acceptable as` |
|        - |  568 | ` * an int (PHP coerces float and numeric string silently).` |
|        - |  569 | ` */` |
|      484 |  570 | `static int VmRandomCheckIntArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgPos,const char *zParamName)` |
|        2 |  571 | `{` |
|      484 |  572 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg)` |
|      486 |  573 | `		\|\| ph7_value_is_resource(pArg) ){` |
|      ! 0 |  574 | `		return PH7_VmThrowException(pCtx,` |
|        - |  575 | `			"TypeError",` |
|        - |  576 | `			"%s(): Argument #%d (%s) must be of type int, %s given",` |
|      ! 0 |  577 | `			zFunc,iArgPos,zParamName,` |
|      ! 0 |  578 | `			ph7_type_name(pArg)` |
|        - |  579 | `			);` |
|        - |  580 | `	}` |
|      486 |  581 | `	if( ph7_value_is_string(pArg) ){` |
|        - |  582 | `		int len;` |
|        9 |  583 | `		const char *zStr = ph7_value_to_string(pArg, &len);` |
|        9 |  584 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|        7 |  585 | `			return PH7_VmThrowException(pCtx,` |
|        - |  586 | `				"TypeError",` |
|        - |  587 | `				"%s(): Argument #%d (%s) must be of type int, string given",` |
|        2 |  588 | `				zFunc,iArgPos,zParamName` |
|        - |  589 | `				);` |
|        - |  590 | `		}` |
|        2 |  591 | `	}` |
|      482 |  592 | `	return SXRET_OK;` |
|      244 |  593 | `}` |
|        - |  594 | `/*` |
|        - |  595 | ` * int random_int(int $min, int $max)` |
|        - |  596 | ` *  Generate a cryptographically secure pseudo-random integer in [$min, $max].` |
|        - |  597 | ` *  Mirrors PHP 7.0+ random_int(). Uses the OS CSPRNG via SyOSCSPRNG().` |
|        - |  598 | ` *  Distribution is uniform via rejection sampling against the smallest` |
|        - |  599 | ` *  power-of-two mask covering the range.` |
|        - |  600 | ` */` |
|      232 |  601 | `PH7_PRIVATE int vm_builtin_random_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  602 | `{` |
|        - |  603 | `	sxi64 iMin,iMax;` |
|        - |  604 | `	sxu64 uRange,uMask,uResult;` |
|        - |  605 | `	unsigned int nAttempt;` |
|        - |  606 | `	int rc;` |
|      233 |  607 | `	if( nArg != 2 ){` |
|      ! 0 |  608 | `		return PH7_VmThrowException(pCtx,` |
|        - |  609 | `			"ArgumentCountError",` |
|        - |  610 | `			"random_int() expects exactly 2 arguments, %d given",` |
|      ! 0 |  611 | `			nArg` |
|        - |  612 | `			);` |
|        - |  613 | `	}` |
|      233 |  614 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_int",1,"$min");` |
|      233 |  615 | `	if( rc != SXRET_OK ){ return rc; }` |
|      231 |  616 | `	rc = VmRandomCheckIntArg(pCtx,apArg[1],"random_int",2,"$max");` |
|      231 |  617 | `	if( rc != SXRET_OK ){ return rc; }` |
|      231 |  618 | `	iMin = ph7_value_to_int64(apArg[0]);` |
|      231 |  619 | `	iMax = ph7_value_to_int64(apArg[1]);` |
|      231 |  620 | `	if( iMin > iMax ){` |
|        3 |  621 | `		return PH7_VmThrowException(pCtx,` |
|        - |  622 | `			"ValueError",` |
|        - |  623 | `			"random_int(): Argument #1 ($min) must be less than or equal to argument #2 ($max)"` |
|        - |  624 | `			);` |
|        - |  625 | `	}` |
|      229 |  626 | `	if( iMin == iMax ){` |
|        5 |  627 | `		ph7_result_int64(pCtx,iMin);` |
|        5 |  628 | `		return SXRET_OK;` |
|        - |  629 | `	}` |
|      225 |  630 | `	uRange = (sxu64)iMax - (sxu64)iMin;` |
|      225 |  631 | `	uMask = uRange;` |
|      225 |  632 | `	uMask \|= uMask >> 1;` |
|      225 |  633 | `	uMask \|= uMask >> 2;` |
|      225 |  634 | `	uMask \|= uMask >> 4;` |
|      225 |  635 | `	uMask \|= uMask >> 8;` |
|      225 |  636 | `	uMask \|= uMask >> 16;` |
|      225 |  637 | `	uMask \|= uMask >> 32;` |
|      225 |  638 | `	uResult = 0;` |
|      335 |  639 | `	for( nAttempt = 0 ; nAttempt < 50 ; ++nAttempt ){` |
|        - |  640 | `		/* Always draw a full 8 bytes so endianness of the cast doesn't matter` |
|        - |  641 | `		 * (a 4-byte fill into a sxu64 would land in the high half on big-endian` |
|        - |  642 | `		 * and the low-half mask would always read 0). */` |
|        - |  643 | `		sxu64 uDraw;` |
|      335 |  644 | `		if( SyOSCSPRNG(&uDraw,sizeof(uDraw)) != SXRET_OK ){` |
|      ! 0 |  645 | `			return PH7_VmThrowException(pCtx,` |
|        - |  646 | `				"Random\\RandomException",` |
|        - |  647 | `				"Cannot gather sufficient random data"` |
|        - |  648 | `				);` |
|        - |  649 | `		}` |
|      335 |  650 | `		uDraw &= uMask;` |
|      335 |  651 | `		if( uDraw <= uRange ){` |
|      225 |  652 | `			uResult = uDraw;` |
|      225 |  653 | `			break;` |
|        - |  654 | `		}` |
|       47 |  655 | `	}` |
|      225 |  656 | `	if( nAttempt >= 50 ){` |
|      ! 0 |  657 | `		return PH7_VmThrowException(pCtx,` |
|        - |  658 | `			"Random\\RandomException",` |
|        - |  659 | `			"Cannot gather sufficient random data"` |
|        - |  660 | `			);` |
|        - |  661 | `	}` |
|      225 |  662 | `	ph7_result_int64(pCtx,(sxi64)((sxu64)iMin + uResult));` |
|      225 |  663 | `	return SXRET_OK;` |
|      117 |  664 | `}` |
|        - |  665 | `/*` |
|        - |  666 | ` * string random_bytes(int $length)` |
|        - |  667 | ` *  Generate $length cryptographically secure random bytes via SyOSCSPRNG().` |
|        - |  668 | ` *  Mirrors PHP 7.0+ random_bytes().` |
|        - |  669 | ` */` |
|       22 |  670 | `PH7_PRIVATE int vm_builtin_random_bytes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  671 | `{` |
|        - |  672 | `	sxi64 iLen;` |
|        - |  673 | `	unsigned char zStack[256];` |
|        - |  674 | `	void *pBuf;` |
|        - |  675 | `	int rc;` |
|       24 |  676 | `	int bHeap = 0;` |
|       24 |  677 | `	if( nArg != 1 ){` |
|      ! 0 |  678 | `		return PH7_VmThrowException(pCtx,` |
|        - |  679 | `			"ArgumentCountError",` |
|        - |  680 | `			"random_bytes() expects exactly 1 argument, %d given",` |
|      ! 0 |  681 | `			nArg` |
|        - |  682 | `			);` |
|        - |  683 | `	}` |
|       24 |  684 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_bytes",1,"$length");` |
|       24 |  685 | `	if( rc != SXRET_OK ){ return rc; }` |
|       22 |  686 | `	iLen = ph7_value_to_int64(apArg[0]);` |
|       22 |  687 | `	if( iLen < 1 ){` |
|        5 |  688 | `		return PH7_VmThrowException(pCtx,` |
|        - |  689 | `			"ValueError",` |
|        - |  690 | `			"random_bytes(): Argument #1 ($length) must be greater than 0"` |
|        - |  691 | `			);` |
|        - |  692 | `	}` |
|        - |  693 | `	/* The PH7 allocator and ph7_result_string both take sxu32/int sizes,` |
|        - |  694 | `	 * so we can't honor a length above 2 GiB. Reject early rather than` |
|        - |  695 | `	 * silently truncating via the (sxu32) cast below. */` |
|       18 |  696 | `	if( iLen > 0x7FFFFFFF ){` |
|      ! 0 |  697 | `		return PH7_VmThrowException(pCtx,` |
|        - |  698 | `			"ValueError",` |
|        - |  699 | `			"random_bytes(): Argument #1 ($length) is too large"` |
|        - |  700 | `			);` |
|        - |  701 | `	}` |
|       18 |  702 | `	if( iLen <= (sxi64)sizeof(zStack) ){` |
|       18 |  703 | `		pBuf = zStack;` |
|       10 |  704 | `	}else{` |
|      ! 0 |  705 | `		pBuf = SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)iLen);` |
|      ! 0 |  706 | `		if( pBuf == 0 ){` |
|      ! 0 |  707 | `			return PH7_VmThrowException(pCtx,` |
|        - |  708 | `				"Exception",` |
|        - |  709 | `				"random_bytes(): Failed to allocate %qd bytes",` |
|      ! 0 |  710 | `				iLen` |
|        - |  711 | `				);` |
|        - |  712 | `		}` |
|      ! 0 |  713 | `		bHeap = 1;` |
|        - |  714 | `	}` |
|       18 |  715 | `	if( SyOSCSPRNG(pBuf,(sxu32)iLen) != SXRET_OK ){` |
|      ! 0 |  716 | `		if( bHeap ){` |
|      ! 0 |  717 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 |  718 | `		}` |
|      ! 0 |  719 | `		return PH7_VmThrowException(pCtx,` |
|        - |  720 | `			"Random\\RandomException",` |
|        - |  721 | `			"Cannot gather sufficient random data"` |
|        - |  722 | `			);` |
|        - |  723 | `	}` |
|       18 |  724 | `	ph7_result_string(pCtx,(const char *)pBuf,(int)iLen);` |
|       18 |  725 | `	if( bHeap ){` |
|      ! 0 |  726 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 |  727 | `	}` |
|       18 |  728 | `	return SXRET_OK;` |
|       13 |  729 | `}` |
|        - |  730 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  731 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  732 | `/* Unique ID private data */` |
|        - |  733 | `struct unique_id_data` |
|        - |  734 | `{` |
|        - |  735 | `	ph7_context *pCtx; /* Call context */` |
|        - |  736 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  737 | `};` |
|        - |  738 | `/*` |
|        - |  739 | ` * Binary to hex consumer callback.` |
|        - |  740 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  741 | ` * defined below.` |
|        - |  742 | ` */` |
|      192 |  743 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  744 | `{` |
|      193 |  745 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  746 | `	sxu32 nBuflen;` |
|        - |  747 | `	/* Extract result buffer length */` |
|      193 |  748 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  749 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  750 | `			/*` |
|        - |  751 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  752 | `			 * string will be 13 characters long` |
|        - |  753 | `			 */` |
|       25 |  754 | `		return SXERR_ABORT;` |
|        - |  755 | `	}` |
|      169 |  756 | `	if( nBuflen > 22 ){` |
|      ! 0 |  757 | `		return SXERR_ABORT;` |
|        - |  758 | `	}` |
|        - |  759 | `	/* Safely Consume the hex stream */` |
|      169 |  760 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  761 | `	return SXRET_OK;` |
|       97 |  762 | `}` |
|        - |  763 | `/*` |
|        - |  764 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  765 | ` *  Generate a unique ID` |
|        - |  766 | ` * Parameter` |
|        - |  767 | ` * $prefix` |
|        - |  768 | ` *  Append this prefix to the generated unique ID.` |
|        - |  769 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  770 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  771 | ` * $more_entropy` |
|        - |  772 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  773 | ` *  that the result will be unique.` |
|        - |  774 | ` * Return` |
|        - |  775 | ` *  Returns the unique identifier, as a string.` |
|        - |  776 | ` */` |
|       24 |  777 | `PH7_PRIVATE int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  778 | `{` |
|        - |  779 | `	struct unique_id_data sUniq;` |
|        - |  780 | `	unsigned char zDigest[20];` |
|       25 |  781 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  782 | `	const char *zPrefix;` |
|        - |  783 | `	SHA1Context sCtx;` |
|        - |  784 | `	char zRandom[7];` |
|        - |  785 | `	int nPrefix;` |
|        - |  786 | `	int entropy;` |
|        - |  787 | `	/* Generate a random string first */` |
|       25 |  788 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  789 | `	/* Initialize fields */` |
|       25 |  790 | `	zPrefix = 0;` |
|       25 |  791 | `	nPrefix = 0;` |
|       25 |  792 | `	entropy = 0;` |
|       25 |  793 | `	if( nArg > 0 ){` |
|        - |  794 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  795 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  796 | `		if( nArg > 1 ){` |
|      ! 0 |  797 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  798 | `		}` |
|      ! 0 |  799 | `	}` |
|       25 |  800 | `	SHA1Init(&sCtx);` |
|        - |  801 | `	/* Generate the random ID */` |
|       25 |  802 | `	if( nPrefix > 0 ){` |
|      ! 0 |  803 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  804 | `	}` |
|        - |  805 | `	/* Append the random ID */` |
|       25 |  806 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  807 | `	/* Append the random string */` |
|       25 |  808 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  809 | `	/* Increment the number */` |
|       25 |  810 | `	pVm->unique_id++;` |
|       25 |  811 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  812 | `	/* Hexify the digest */` |
|       25 |  813 | `	sUniq.pCtx = pCtx;` |
|       25 |  814 | `	sUniq.entropy = entropy;` |
|       25 |  815 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  816 | `	/* All done */` |
|       25 |  817 | `	return PH7_OK;` |
|        1 |  818 | `}` |
|        - |  819 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  820 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  821 | `/*` |
|        - |  822 | ` * Section:` |
|        - |  823 | ` *  Language construct implementation as foreign functions.` |
|        - |  824 | ` * Status:` |
|        - |  825 | ` *    Stable.` |
|        - |  826 | ` */` |
|        - |  827 | `/*` |
|        - |  828 | ` * void echo($string...)` |
|        - |  829 | ` *  Output one or more messages.` |
|        - |  830 | ` * Parameters` |
|        - |  831 | ` *  $string` |
|        - |  832 | ` *   Message to output.` |
|        - |  833 | ` * Return` |
|        - |  834 | ` *  NULL.` |
|        - |  835 | ` */` |
|      ! 0 |  836 | `PH7_PRIVATE int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  837 | `{` |
|        - |  838 | `	const char *zData;` |
|      ! 0 |  839 | `	int nDataLen = 0;` |
|        - |  840 | `	ph7_vm *pVm;` |
|        - |  841 | `	int i,rc;` |
|        - |  842 | `	/* Point to the target VM */` |
|      ! 0 |  843 | `	pVm = pCtx->pVm;` |
|        - |  844 | `	/* Output */` |
|      ! 0 |  845 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  846 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  847 | `		if( nDataLen > 0 ){` |
|      ! 0 |  848 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  849 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 |  850 | `			if( rc == SXERR_ABORT ){` |
|        - |  851 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  852 | `				return PH7_ABORT;` |
|        - |  853 | `			}` |
|      ! 0 |  854 | `		}` |
|      ! 0 |  855 | `	}` |
|      ! 0 |  856 | `	return SXRET_OK;` |
|      ! 0 |  857 | `}` |
|        - |  858 | `/*` |
|        - |  859 | ` * int print($string...)` |
|        - |  860 | ` *  Output one or more messages.` |
|        - |  861 | ` * Parameters` |
|        - |  862 | ` *  $string` |
|        - |  863 | ` *   Message to output.` |
|        - |  864 | ` * Return` |
|        - |  865 | ` *  1 always.` |
|        - |  866 | ` */` |
|       34 |  867 | `PH7_PRIVATE int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  868 | `{` |
|        - |  869 | `	const char *zData;` |
|       36 |  870 | `	int nDataLen = 0;` |
|        - |  871 | `	ph7_vm *pVm;` |
|        - |  872 | `	int i,rc;` |
|        - |  873 | `	/* Point to the target VM */` |
|       36 |  874 | `	pVm = pCtx->pVm;` |
|        - |  875 | `	/* Output */` |
|       70 |  876 | `	for( i = 0 ; i < nArg ; ++i ){` |
|       36 |  877 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|       36 |  878 | `		if( nDataLen > 0 ){` |
|       36 |  879 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|       36 |  880 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|       36 |  881 | `			if( rc == SXERR_ABORT ){` |
|        - |  882 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  883 | `				return PH7_ABORT;` |
|        - |  884 | `			}` |
|       17 |  885 | `		}` |
|       19 |  886 | `	}` |
|        - |  887 | `	/* Return 1 */` |
|       36 |  888 | `	ph7_result_int(pCtx,1);` |
|       36 |  889 | `	return SXRET_OK;` |
|       19 |  890 | `}` |
|        - |  891 | `/*` |
|        - |  892 | ` * void exit(string $msg)` |
|        - |  893 | ` * void exit(int $status)` |
|        - |  894 | ` * void die(string $ms)` |
|        - |  895 | ` * void die(int $status)` |
|        - |  896 | ` *   Output a message and terminate program execution.` |
|        - |  897 | ` * Parameter` |
|        - |  898 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  899 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  900 | ` *  and not printed` |
|        - |  901 | ` * Return` |
|        - |  902 | ` *  NULL` |
|        - |  903 | ` */` |
|      ! 0 |  904 | `PH7_PRIVATE int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  905 | `{` |
|      ! 0 |  906 | `	if( nArg > 0 ){` |
|      ! 0 |  907 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  908 | `			const char *zData;` |
|      ! 0 |  909 | `			int iLen = 0;` |
|        - |  910 | `			/* Print exit message */` |
|      ! 0 |  911 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  912 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  913 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  914 | `			sxi32 iExitStatus;` |
|        - |  915 | `			/* Record exit status code */` |
|      ! 0 |  916 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 |  917 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 |  918 | `		}` |
|      ! 0 |  919 | `	}` |
|        - |  920 | `	/* Request a VM-wide halt (see PH7_OP_HALT) and abort processing` |
|        - |  921 | `	 * immediately; the abort unwinds enclosing frames and execution units.` |
|        - |  922 | `	 */` |
|      ! 0 |  923 | `	pCtx->pVm->bHaltRequested = 1;` |
|      ! 0 |  924 | `	return PH7_ABORT;` |
|      ! 0 |  925 | `}` |
|        - |  926 | `/*` |
|        - |  927 | ` * Section:` |
|        - |  928 | ` *  Version,Credits and Copyright related functions.` |
|        - |  929 | ` * Status:` |
|        - |  930 | ` *    Stable.` |
|        - |  931 | ` */` |
|        - |  932 | `/*` |
|        - |  933 | ` * string ph7version(void)` |
|        - |  934 | ` *  Returns the running version of the PH7 version.` |
|        - |  935 | ` * Parameters` |
|        - |  936 | ` *  None` |
|        - |  937 | ` * Return` |
|        - |  938 | ` * Current PH7 version.` |
|        - |  939 | ` */` |
|        2 |  940 | `PH7_PRIVATE int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  941 | `{` |
|        1 |  942 | `	SXUNUSED(nArg);` |
|        1 |  943 | `	SXUNUSED(apArg); /* cc warning */` |
|        - |  944 | `	/* Current engine version */` |
|        3 |  945 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 |  946 | `	return PH7_OK;` |
|        1 |  947 | `}` |
|        - |  948 | `/*` |
|        - |  949 | ` * string phpversion([ string $extension ])` |
|        - |  950 | ` *  Returns the PHP-compatibility version PHL advertises (see PHP_COMPAT_VERSION).` |
|        - |  951 | ` * Parameters` |
|        - |  952 | ` *  $extension (optional): an extension name. PHL has no extension registry, so any` |
|        - |  953 | ` *  argument yields NULL (PHP returns FALSE for an unknown extension).` |
|        - |  954 | ` * Return` |
|        - |  955 | ` *  The PHP-compat version string, or NULL when called with an extension argument.` |
|        - |  956 | ` */` |
|        4 |  957 | `PH7_PRIVATE int vm_builtin_phpversion(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  958 | `{` |
|        2 |  959 | `	SXUNUSED(apArg); /* cc warning */` |
|        5 |  960 | `	if( nArg > 0 ){` |
|      ! 0 |  961 | `		ph7_result_null(pCtx);` |
|      ! 0 |  962 | `		return PH7_OK;` |
|        - |  963 | `	}` |
|        5 |  964 | `	ph7_result_string(pCtx,PHP_COMPAT_VERSION,(int)sizeof(PHP_COMPAT_VERSION) - 1);` |
|        5 |  965 | `	return PH7_OK;` |
|        3 |  966 | `}` |
|        - |  967 | `/*` |
|        - |  968 | ` * string php_sapi_name(void)` |
|        - |  969 | ` *  Returns the type of interface (SAPI) PHL is running under.` |
|        - |  970 | ` * Parameters` |
|        - |  971 | ` *  None` |
|        - |  972 | ` * Return` |
|        - |  973 | ` *  "cli-server" while serving an HTTP request via the built-in -S server, "cli" otherwise.` |
|        - |  974 | ` */` |
|        2 |  975 | `PH7_PRIVATE int vm_builtin_php_sapi_name(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  976 | `{` |
|        3 |  977 | `	const char *zSapi = pCtx->pVm->bHttpContext ? "cli-server" : "cli";` |
|        1 |  978 | `	SXUNUSED(nArg);` |
|        1 |  979 | `	SXUNUSED(apArg); /* cc warning */` |
|        3 |  980 | `	ph7_result_string(pCtx,zSapi,-1);` |
|        3 |  981 | `	return PH7_OK;` |
|        1 |  982 | `}` |
|        - |  983 | `/*` |
|        - |  984 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - |  985 | ` */` |
|        - |  986 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - |  987 | ` "<html><head>"\` |
|        - |  988 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - |  989 | ` "<style type=\"text/css\">"\` |
|        - |  990 | ` "div {"\` |
|        - |  991 | `     "border: 1px solid #cccccc;"\` |
|        - |  992 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - |  993 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - |  994 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - |  995 | `     "-moz-border-radius-topright: 10px;"\` |
|        - |  996 | `     "-webkit-border-radius: 10px;"\` |
|        - |  997 | `     "-o-border-radius: 10px;"\` |
|        - |  998 | `     "border-radius: 10px;"\` |
|        - |  999 | `     "padding-left: 2em;"\` |
|        - | 1000 | `     "background-color: white;"\` |
|        - | 1001 | `     "margin-left: auto;"\` |
|        - | 1002 | `     "font-family: verdana;"\` |
|        - | 1003 | `     "padding-right: 2em;"\` |
|        - | 1004 | `     "margin-right: auto;"\` |
|        - | 1005 | `     "}"\` |
|        - | 1006 | `     "body {"\` |
|        - | 1007 | `     "padding: 0.2em;"\` |
|        - | 1008 | `     "font-style: normal;"\` |
|        - | 1009 | `     "font-size: medium;"\` |
|        - | 1010 | `     "background-color: #f2f2f2;"\` |
|        - | 1011 | `     "}"\` |
|        - | 1012 | `     "hr {"\` |
|        - | 1013 | `     "border-style: solid none none;"\` |
|        - | 1014 | `     "border-width: 1px medium medium;"\` |
|        - | 1015 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 1016 | `     "height: 1px;"\` |
|        - | 1017 | `     "}"\` |
|        - | 1018 | `     "a {"\` |
|        - | 1019 | `     "color: #3366cc;"\` |
|        - | 1020 | `     "text-decoration: none;"\` |
|        - | 1021 | `     "}"\` |
|        - | 1022 | `     "a:hover {"\` |
|        - | 1023 | `     "color: #999999;"\` |
|        - | 1024 | `     "}"\` |
|        - | 1025 | `     "a:active {"\` |
|        - | 1026 | `     "color: #663399;"\` |
|        - | 1027 | `     "}"\` |
|        - | 1028 | `     "h1 {"\` |
|        - | 1029 | `     "margin: 0;"\` |
|        - | 1030 | `     "padding: 0;"\` |
|        - | 1031 | `     "font-family: Verdana;"\` |
|        - | 1032 | `     "font-weight: bold;"\` |
|        - | 1033 | `     "font-style: normal;"\` |
|        - | 1034 | `     "font-size: medium;"\` |
|        - | 1035 | `     "text-transform: capitalize;"\` |
|        - | 1036 | `     "color: #0a328c;"\` |
|        - | 1037 | `     "}"\` |
|        - | 1038 | `     "p {"\` |
|        - | 1039 | `     "margin: 0 auto;"\` |
|        - | 1040 | `     "font-size: medium;"\` |
|        - | 1041 | `     "font-style: normal;"\` |
|        - | 1042 | `     "font-family: verdana;"\` |
|        - | 1043 | `     "}"\` |
|        - | 1044 | `"</style></head><body>"\` |
|        - | 1045 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 1046 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 1047 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 1048 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 1049 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 1050 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 1051 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 1052 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 1053 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 1054 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 1055 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 1056 |  |
|        - | 1057 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 1058 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 1059 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 1060 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 1061 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 1062 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 1063 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 1064 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 1065 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 1066 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 1067 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 1068 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 1069 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 1070 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 1071 |  |
|        - | 1072 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 1073 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 1074 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 1075 | `"&nbsp;*<br>"\` |
|        - | 1076 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 1077 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 1078 | `"&nbsp;* are met:<br>"\` |
|        - | 1079 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 1080 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 1081 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 1082 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 1083 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 1084 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 1085 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 1086 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 1087 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 1088 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 1089 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 1090 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 1091 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 1092 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 1093 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 1094 | `"&nbsp;*<br>"\` |
|        - | 1095 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 1096 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 1097 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 1098 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 1099 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 1100 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 1101 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 1102 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 1103 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 1104 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 1105 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 1106 | `"&nbsp;*/<br>"\` |
|        - | 1107 | `"</span></small></small></p>"\` |
|        - | 1108 | `"</div></body></html>"` |
|        - | 1109 | `/*` |
|        - | 1110 | ` * bool ph7credits(void)` |
|        - | 1111 | ` * bool ph7info(void)` |
|        - | 1112 | ` * bool ph7copyright(void)` |
|        - | 1113 | ` *  Prints out the credits for PH7 engine` |
|        - | 1114 | ` * Parameters` |
|        - | 1115 | ` *  None` |
|        - | 1116 | ` * Return` |
|        - | 1117 | ` *  Always TRUE` |
|        - | 1118 | ` */` |
|        2 | 1119 | `PH7_PRIVATE int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1120 | `{` |
|        3 | 1121 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 1122 | `	/* Expand the HTML page above*/` |
|        3 | 1123 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 1124 | `	ph7_context_output_format(` |
|        1 | 1125 | `		pCtx,` |
|        - | 1126 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 1127 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 1128 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 1129 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 1130 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 1131 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 1132 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 1133 | `#ifdef __WINNT__` |
|        - | 1134 | `		"Windows NT"` |
|        - | 1135 | `#elif defined(__UNIXES__)` |
|        - | 1136 | `		"UNIX-Like"` |
|        - | 1137 | `#else` |
|        - | 1138 | `		"Other OS"` |
|        - | 1139 | `#endif` |
|        - | 1140 | `		);` |
|        3 | 1141 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 1142 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 1143 | `	SXUNUSED(apArg);` |
|        - | 1144 | `	/* Return TRUE */` |
|        - | 1145 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 1146 | `	return PH7_OK;` |
|        1 | 1147 | `}` |
|        - | 1148 | `/*` |
|        - | 1149 | ` * Section:` |
|        - | 1150 | ` *    URL related routines.` |
|        - | 1151 | ` * Status:` |
|        - | 1152 | ` *    Stable.` |
|        - | 1153 | ` */` |
|        - | 1154 | `/*` |
|        - | 1155 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 1156 | ` *  Parse a URL and return its fields.` |
|        - | 1157 | ` * Parameters` |
|        - | 1158 | ` *  $url` |
|        - | 1159 | ` *   The URL to parse.` |
|        - | 1160 | ` * $component` |
|        - | 1161 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 1162 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 1163 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 1164 | ` *  in which case the return value will be an integer).` |
|        - | 1165 | ` * Return` |
|        - | 1166 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 1167 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 1168 | ` *  this array are:` |
|        - | 1169 | ` *   scheme - e.g. http` |
|        - | 1170 | ` *   host` |
|        - | 1171 | ` *   port` |
|        - | 1172 | ` *   user` |
|        - | 1173 | ` *   pass` |
|        - | 1174 | ` *   path` |
|        - | 1175 | ` *   query - after the question mark ?` |
|        - | 1176 | ` *   fragment - after the hashmark #` |
|        - | 1177 | ` * Note:` |
|        - | 1178 | ` *  FALSE is returned on failure.` |
|        - | 1179 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 1180 | ` *  with the standard PHP engine.` |
|        - | 1181 | ` */` |
|        - | 1182 | `/*` |
|        - | 1183 | ` * parse_url() component set.` |
|        - | 1184 | ` *` |
|        - | 1185 | ` * A bare SyString cannot express "present but empty", which php needs:` |
|        - | 1186 | ` * parse_url("") is ['path'=>''] and parse_url("?") is ['query'=>''], both` |
|        - | 1187 | ` * distinct from the component being absent. So presence is tracked separately.` |
|        - | 1188 | ` */` |
|        - | 1189 | `typedef struct VmUrlParts VmUrlParts;` |
|        - | 1190 | `struct VmUrlParts` |
|        - | 1191 | `{` |
|        - | 1192 | `	SyString sScheme,sUser,sPass,sHost,sPath,sQuery,sFragment;` |
|        - | 1193 | `	int iPort;     /* Resolved port, meaningful only when bPort is set */` |
|        - | 1194 | `	sxu8 bScheme,bUser,bPass,bHost,bPort,bPath,bQuery,bFragment;` |
|        - | 1195 | `};` |
|      256 | 1196 | `static int VmUrlIsAlnum(int c)` |
|        1 | 1197 | `{` |
|      257 | 1198 | `	return (c >= '0' && c <= '9') \|\| (c >= 'a' && c <= 'z') \|\| (c >= 'A' && c <= 'Z');` |
|        1 | 1199 | `}` |
|        4 | 1200 | `static int VmUrlIsAlpha(int c)` |
|        1 | 1201 | `{` |
|        5 | 1202 | `	return (c >= 'a' && c <= 'z') \|\| (c >= 'A' && c <= 'Z');` |
|        1 | 1203 | `}` |
|        - | 1204 | `/* scheme = ALNUM *( ALNUM / "+" / "-" / "." ) -- php accepts a digit first. */` |
|      256 | 1205 | `static int VmUrlIsSchemeByte(int c)` |
|        1 | 1206 | `{` |
|      257 | 1207 | `	return VmUrlIsAlnum(c) \|\| c == '+' \|\| c == '-' \|\| c == '.';` |
|        1 | 1208 | `}` |
|        - | 1209 | `/*` |
|        - | 1210 | ` * Resolve the port span that followed the ':' in an authority.` |
|        - | 1211 | ` *` |
|        - | 1212 | ` * Returns 1 (usable port in *piPort), 0 (the span is empty, so there is simply` |
|        - | 1213 | ` * no port) or -1 (php rejects the whole URL). php is lenient about what follows` |
|        - | 1214 | ` * the digits -- ":8a" and ":8 0" both yield 8 -- but demands at least one digit` |
|        - | 1215 | ` * and a value that fits a port, so ":abc", ":-80" and ":65536" are all failures.` |
|        - | 1216 | ` */` |
|       42 | 1217 | `static int VmUrlParsePort(const char *z,int n,int *piPort)` |
|        1 | 1218 | `{` |
|       43 | 1219 | `	int i = 0,iVal = 0,nDigit = 0;` |
|       43 | 1220 | `	if( n < 1 ){` |
|      ! 0 | 1221 | `		return 0;` |
|        - | 1222 | `	}` |
|       64 | 1223 | `	while( i < n && (z[i] == ' ' \|\| z[i] == '\t') ){` |
|      ! 0 | 1224 | `		i++;` |
|      ! 0 | 1225 | `	}` |
|       43 | 1226 | `	if( i < n && (z[i] == '+' \|\| z[i] == '-') ){` |
|      ! 0 | 1227 | `		if( z[i] == '-' ){` |
|      ! 0 | 1228 | `			return -1;` |
|        - | 1229 | `		}` |
|      ! 0 | 1230 | `		i++;` |
|      ! 0 | 1231 | `	}` |
|      161 | 1232 | `	while( i < n && z[i] >= '0' && z[i] <= '9' ){` |
|      119 | 1233 | `		iVal = iVal * 10 + (z[i] - '0');` |
|      119 | 1234 | `		if( iVal > 65535 ){` |
|      ! 0 | 1235 | `			return -1; /* Out of range, and this also caps the accumulator */` |
|        - | 1236 | `		}` |
|      119 | 1237 | `		nDigit++;` |
|      119 | 1238 | `		i++;` |
|        1 | 1239 | `	}` |
|       43 | 1240 | `	if( nDigit < 1 ){` |
|        3 | 1241 | `		return -1;` |
|        - | 1242 | `	}` |
|       41 | 1243 | `	*piPort = iVal;` |
|       41 | 1244 | `	return 1;` |
|       22 | 1245 | `}` |
|        - | 1246 | `/*` |
|        - | 1247 | ` * Split an authority -- "[user[:pass]@]host[:port]" -- into pOut.` |
|        - | 1248 | ` * Returns 0 for the malformed input php reports as FALSE.` |
|        - | 1249 | ` */` |
|       66 | 1250 | `static int VmUrlParseAuthority(const char *z,int n,VmUrlParts *pOut,int bPortKnown)` |
|        1 | 1251 | `{` |
|        - | 1252 | `	const char *zHost;` |
|       67 | 1253 | `	int i,iAt = -1,iColon = -1,iSep = -1,nHost,iPort = 0,rc;` |
|        - | 1254 | `	/* php splits the credentials at the LAST '@' ("//a@b@c" is user "a@b") */` |
|      699 | 1255 | `	for( i = 0 ; i < n ; ++i ){` |
|      633 | 1256 | `		if( z[i] == '@' ){` |
|       25 | 1257 | `			iAt = i;` |
|       12 | 1258 | `		}` |
|      317 | 1259 | `	}` |
|       67 | 1260 | `	if( iAt >= 0 ){` |
|        - | 1261 | `		/* and the user from the password at the FIRST ':' before it */` |
|      109 | 1262 | `		for( i = 0 ; i < iAt ; ++i ){` |
|      107 | 1263 | `			if( z[i] == ':' ){` |
|       23 | 1264 | `				iColon = i;` |
|       23 | 1265 | `				break;` |
|        - | 1266 | `			}` |
|       43 | 1267 | `		}` |
|       25 | 1268 | `		if( iColon >= 0 ){` |
|       23 | 1269 | `			SyStringInitFromBuf(&pOut->sUser,z,(sxu32)iColon);` |
|       23 | 1270 | `			SyStringInitFromBuf(&pOut->sPass,&z[iColon+1],(sxu32)(iAt - iColon - 1));` |
|       23 | 1271 | `			pOut->bUser = pOut->bPass = 1;` |
|       12 | 1272 | `		}else{` |
|        3 | 1273 | `			SyStringInitFromBuf(&pOut->sUser,z,(sxu32)iAt);` |
|        3 | 1274 | `			pOut->bUser = 1;` |
|        - | 1275 | `		}` |
|       25 | 1276 | `		z += iAt + 1;` |
|       25 | 1277 | `		n -= iAt + 1;` |
|       12 | 1278 | `	}` |
|       67 | 1279 | `	zHost = z;` |
|       67 | 1280 | `	nHost = n;` |
|       67 | 1281 | `	if( !(n > 1 && z[0] == '[' && z[n-1] == ']') ){` |
|        - | 1282 | `		/* Unless a bracketed IPv6 literal fills the WHOLE authority -- in which` |
|        - | 1283 | `		 * case php keeps the brackets in "host" and scans no port, so every ':'` |
|        - | 1284 | `		 * inside belongs to the address -- the port hangs off the LAST ':'.` |
|        - | 1285 | `		 * php decides that on the first and last byte alone, which is why` |
|        - | 1286 | `		 * "//[:]\|]" is a single host while "//[::1]:8080" splits a port off. */` |
|      507 | 1287 | `		for( i = 0 ; i < n ; ++i ){` |
|      443 | 1288 | `			if( z[i] == ':' ){` |
|       43 | 1289 | `				iSep = i;` |
|       21 | 1290 | `			}` |
|      222 | 1291 | `		}` |
|       65 | 1292 | `		if( iSep >= 0 ){` |
|        - | 1293 | `			/* The host is trimmed at the ':' whether or not the port was already` |
|        - | 1294 | `			 * resolved by the caller. */` |
|       43 | 1295 | `			nHost = iSep;` |
|       43 | 1296 | `			if( !bPortKnown ){` |
|       37 | 1297 | `				rc = VmUrlParsePort(&z[iSep+1],n - iSep - 1,&iPort);` |
|       37 | 1298 | `				if( rc < 0 ){` |
|        3 | 1299 | `					return 0;` |
|        - | 1300 | `				}` |
|       35 | 1301 | `				if( rc > 0 ){` |
|       35 | 1302 | `					pOut->iPort = iPort;` |
|       35 | 1303 | `					pOut->bPort = 1;` |
|       17 | 1304 | `				}` |
|       17 | 1305 | `			}` |
|       20 | 1306 | `		}` |
|       31 | 1307 | `	}` |
|       65 | 1308 | `	if( nHost < 1 ){` |
|        - | 1309 | `		/* php requires a non-empty host once an authority is in play, which is` |
|        - | 1310 | `		 * what makes "//", "http://" and ":80" all FALSE. */` |
|        9 | 1311 | `		return 0;` |
|        - | 1312 | `	}` |
|       57 | 1313 | `	SyStringInitFromBuf(&pOut->sHost,zHost,(sxu32)nHost);` |
|       57 | 1314 | `	pOut->bHost = 1;` |
|       57 | 1315 | `	return 1;` |
|       34 | 1316 | `}` |
|        - | 1317 | `/*` |
|        - | 1318 | ` * Split "path[?query][#fragment]". The fragment is taken FIRST and the query` |
|        - | 1319 | ` * only from what precedes it, so "#a?b" is a fragment of "a?b" with no query.` |
|        - | 1320 | ` */` |
|       80 | 1321 | `static void VmUrlParsePath(const char *z,int n,VmUrlParts *pOut)` |
|        1 | 1322 | `{` |
|       81 | 1323 | `	int i,iEnd = n;` |
|      547 | 1324 | `	for( i = 0 ; i < n ; ++i ){` |
|      501 | 1325 | `		if( z[i] == '#' ){` |
|       35 | 1326 | `			SyStringInitFromBuf(&pOut->sFragment,&z[i+1],(sxu32)(n - i - 1));` |
|       35 | 1327 | `			pOut->bFragment = 1;` |
|       35 | 1328 | `			iEnd = i;` |
|       35 | 1329 | `			break;` |
|        - | 1330 | `		}` |
|      234 | 1331 | `	}` |
|      393 | 1332 | `	for( i = 0 ; i < iEnd ; ++i ){` |
|      347 | 1333 | `		if( z[i] == '?' ){` |
|       35 | 1334 | `			SyStringInitFromBuf(&pOut->sQuery,&z[i+1],(sxu32)(iEnd - i - 1));` |
|       35 | 1335 | `			pOut->bQuery = 1;` |
|       35 | 1336 | `			iEnd = i;` |
|       35 | 1337 | `			break;` |
|        - | 1338 | `		}` |
|      157 | 1339 | `	}` |
|       81 | 1340 | `	if( iEnd > 0 ){` |
|       71 | 1341 | `		SyStringInitFromBuf(&pOut->sPath,z,(sxu32)iEnd);` |
|       71 | 1342 | `		pOut->bPath = 1;` |
|       35 | 1343 | `	}` |
|       81 | 1344 | `}` |
|        - | 1345 | `/* Parse "host[:port]" followed by an optional path/query/fragment. */` |
|       66 | 1346 | `static int VmUrlAuthorityThenPath(const char *z,int n,VmUrlParts *pOut,int bPortKnown)` |
|        1 | 1347 | `{` |
|       67 | 1348 | `	int i,iEnd = n;` |
|      699 | 1349 | `	for( i = 0 ; i < n ; ++i ){` |
|      683 | 1350 | `		if( z[i] == '/' \|\| z[i] == '?' \|\| z[i] == '#' ){` |
|       51 | 1351 | `			iEnd = i;` |
|       51 | 1352 | `			break;` |
|        - | 1353 | `		}` |
|      317 | 1354 | `	}` |
|       67 | 1355 | `	if( !VmUrlParseAuthority(z,iEnd,pOut,bPortKnown) ){` |
|       11 | 1356 | `		return 0;` |
|        - | 1357 | `	}` |
|       57 | 1358 | `	if( iEnd < n ){` |
|       47 | 1359 | `		VmUrlParsePath(&z[iEnd],n - iEnd,pOut);` |
|       23 | 1360 | `	}` |
|       57 | 1361 | `	return 1;` |
|       34 | 1362 | `}` |
|        - | 1363 | `/*` |
|        - | 1364 | ` * Resolve the port php took from the FIRST ':' of a host:port URL.` |
|        - | 1365 | ` *` |
|        - | 1366 | ` * php reads the port straight off that colon before it works out where the host` |
|        - | 1367 | ` * ends, so the digits can even belong to what becomes the path: parse_url()` |
|        - | 1368 | ` * reports port 1 for "a/:1", whose path is "/:1". Mirroring the order keeps` |
|        - | 1369 | ` * that quirk. Returns 0 for a port php rejects.` |
|        - | 1370 | ` */` |
|        6 | 1371 | `static int VmUrlPreparePort(const char *z,int k,int nEnd,VmUrlParts *pOut)` |
|        1 | 1372 | `{` |
|        7 | 1373 | `	int iPort = 0;` |
|        7 | 1374 | `	int rc = VmUrlParsePort(&z[k+1],nEnd - k - 1,&iPort);` |
|        7 | 1375 | `	if( rc < 0 ){` |
|      ! 0 | 1376 | `		return 0;` |
|        - | 1377 | `	}` |
|        7 | 1378 | `	if( rc > 0 ){` |
|        7 | 1379 | `		pOut->iPort = iPort;` |
|        7 | 1380 | `		pOut->bPort = 1;` |
|        3 | 1381 | `	}` |
|        7 | 1382 | `	return 1;` |
|        4 | 1383 | `}` |
|        - | 1384 | `/*` |
|        - | 1385 | ` * php's URL parser, as parse_url() needs it. Returns 0 where php returns FALSE.` |
|        - | 1386 | ` *` |
|        - | 1387 | ` * PH7_VmHttpSplitURI cannot serve here: it is an HTTP REQUEST-target splitter` |
|        - | 1388 | ` * (the HTTP server and filter_var share it) and assumes the input is` |
|        - | 1389 | ` * authority-first, so it read "a" as a host and "mailto:me@x.com" as` |
|        - | 1390 | ` * user:pass@host. php instead decides authority-vs-path up front: only a "//",` |
|        - | 1391 | ` * with or without a scheme before it, introduces an authority.` |
|        - | 1392 | ` */` |
|      104 | 1393 | `static int VmUrlSplit(const char *z,int n,VmUrlParts *pOut)` |
|        1 | 1394 | `{` |
|      105 | 1395 | `	int i,k = -1,bScheme,bPortForm = 0,nPortEnd = 0;` |
|      105 | 1396 | `	SyZero(pOut,sizeof(VmUrlParts));` |
|        - | 1397 | `	/* A leading "//" settles it before any colon is considered: the authority` |
|        - | 1398 | `	 * starts after the slashes. Reading the colon first turned "//h:80" into a` |
|        - | 1399 | `	 * host called "//h" and "//[::1]" into a path. */` |
|      105 | 1400 | `	if( n >= 2 && z[0] == '/' && z[1] == '/' ){` |
|       13 | 1401 | `		return VmUrlAuthorityThenPath(&z[2],n - 2,pOut,0);` |
|        - | 1402 | `	}` |
|      441 | 1403 | `	for( i = 0 ; i < n ; ++i ){` |
|      419 | 1404 | `		if( z[i] == ':' ){` |
|       71 | 1405 | `			k = i;` |
|       71 | 1406 | `			break;` |
|        - | 1407 | `		}` |
|      175 | 1408 | `	}` |
|       93 | 1409 | `	if( k == 0 && n == 1 ){` |
|        - | 1410 | `		/* A lone ":" is an EMPTY scheme, which php rejects outright -- unlike` |
|        - | 1411 | `		 * ":a" or "::", which are simply paths. */` |
|        3 | 1412 | `		return 0;` |
|        - | 1413 | `	}` |
|       91 | 1414 | `	bScheme = k > 0;` |
|      347 | 1415 | `	for( i = 0 ; bScheme && i < k ; ++i ){` |
|      257 | 1416 | `		if( !VmUrlIsSchemeByte((unsigned char)z[i]) ){` |
|      ! 0 | 1417 | `			bScheme = 0;` |
|      ! 0 | 1418 | `		}` |
|      129 | 1419 | `	}` |
|       91 | 1420 | `	if( bScheme && k + 1 == n ){` |
|        - | 1421 | `		/* "x:" -- the scheme is the whole URL */` |
|        3 | 1422 | `		SyStringInitFromBuf(&pOut->sScheme,z,(sxu32)k);` |
|        3 | 1423 | `		pOut->bScheme = 1;` |
|        3 | 1424 | `		return 1;` |
|        - | 1425 | `	}` |
|        - | 1426 | `	/* Decide whether that ':' introduces a PORT rather than a scheme: at least` |
|        - | 1427 | `	 * one digit, running to the end of the string or to a '/'. That is what` |
|        - | 1428 | `	 * makes "a:80" a host:port while "a:80?q" is a scheme with path "80", and` |
|        - | 1429 | `	 * it holds however the ':' is reached -- ":1" and "/:1" are both authorities` |
|        - | 1430 | `	 * with an empty host, which php rejects. php caps the run, so a long number` |
|        - | 1431 | `	 * stays a path: "a:123456" is a path, "a:99999" an out-of-range port. */` |
|       89 | 1432 | `	if( k >= 0 ){` |
|       67 | 1433 | `		int p = k + 1;` |
|       67 | 1434 | `		int bBeforeQuery = 1;` |
|       67 | 1435 | `		nPortEnd = k + 1;` |
|        - | 1436 | `		/* A ':' that sits inside a query or fragment is just data: "?:1" is a` |
|        - | 1437 | `		 * query of ":1", not an authority with an empty host. */` |
|      321 | 1438 | `		for( i = 0 ; i < k ; ++i ){` |
|      255 | 1439 | `			if( z[i] == '?' \|\| z[i] == '#' ){` |
|      ! 0 | 1440 | `				bBeforeQuery = 0;` |
|      ! 0 | 1441 | `				break;` |
|        - | 1442 | `			}` |
|      128 | 1443 | `		}` |
|       77 | 1444 | `		while( p < n && z[p] >= '0' && z[p] <= '9' ){` |
|       11 | 1445 | `			p++;` |
|        1 | 1446 | `		}` |
|       67 | 1447 | `		if( bBeforeQuery && p > k + 1 && (p >= n \|\| z[p] == '/') && (p - k) < 7 ){` |
|        7 | 1448 | `			bPortForm = 1;` |
|        7 | 1449 | `			nPortEnd = p;` |
|        3 | 1450 | `		}` |
|       33 | 1451 | `	}` |
|       89 | 1452 | `	if( !bScheme ){` |
|       25 | 1453 | `		if( bPortForm ){` |
|        3 | 1454 | `			if( !VmUrlPreparePort(z,k,nPortEnd,pOut) ){` |
|      ! 0 | 1455 | `				return 0;` |
|        - | 1456 | `			}` |
|        3 | 1457 | `			return VmUrlAuthorityThenPath(z,n,pOut,1);` |
|        - | 1458 | `		}` |
|       23 | 1459 | `		VmUrlParsePath(z,n,pOut);` |
|       23 | 1460 | `		if( !pOut->bPath && !pOut->bQuery && !pOut->bFragment ){` |
|        - | 1461 | `			/* php reports the empty URL as an empty PATH, not as no components */` |
|        3 | 1462 | `			SyStringInitFromBuf(&pOut->sPath,z,0);` |
|        3 | 1463 | `			pOut->bPath = 1;` |
|        1 | 1464 | `		}` |
|       23 | 1465 | `		return 1;` |
|        - | 1466 | `	}` |
|       65 | 1467 | `	if( bPortForm ){` |
|        5 | 1468 | `		if( !VmUrlPreparePort(z,k,nPortEnd,pOut) ){` |
|      ! 0 | 1469 | `			return 0;` |
|        - | 1470 | `		}` |
|        5 | 1471 | `		return VmUrlAuthorityThenPath(z,n,pOut,1);` |
|        - | 1472 | `	}` |
|       61 | 1473 | `	SyStringInitFromBuf(&pOut->sScheme,z,(sxu32)k);` |
|       61 | 1474 | `	pOut->bScheme = 1;` |
|       61 | 1475 | `	if( z[k+1] == '/' && k + 2 < n && z[k+2] == '/' ){` |
|       52 | 1476 | `		if( k + 3 < n && z[k+3] == '/' && pOut->sScheme.nByte == 4` |
|        4 | 1477 | `		 && (z[0]=='f'\|\|z[0]=='F') && (z[1]=='i'\|\|z[1]=='I')` |
|        5 | 1478 | `		 && (z[2]=='l'\|\|z[2]=='L') && (z[3]=='e'\|\|z[3]=='E') ){` |
|        - | 1479 | `			/* file:/// has no authority: the path starts at the third slash,` |
|        - | 1480 | `			 * except that a "c:" drive letter swallows it (file:///c:/x is the` |
|        - | 1481 | `			 * path "c:/x"). A '\|' in place of the ':' does NOT count. */` |
|        5 | 1482 | `			int iBase = k + 3;` |
|        5 | 1483 | `			if( iBase + 2 < n && VmUrlIsAlpha((unsigned char)z[iBase+1]) && z[iBase+2] == ':' ){` |
|      ! 0 | 1484 | `				iBase++;` |
|      ! 0 | 1485 | `			}` |
|        5 | 1486 | `			VmUrlParsePath(&z[iBase],n - iBase,pOut);` |
|        5 | 1487 | `			return 1;` |
|        - | 1488 | `		}` |
|       49 | 1489 | `		return VmUrlAuthorityThenPath(&z[k+3],n - k - 3,pOut,0);` |
|        - | 1490 | `	}` |
|        - | 1491 | `	/* "mailto:me@x.com", "x:y", "http:/x": everything after the ':' is a path */` |
|        9 | 1492 | `	VmUrlParsePath(&z[k+1],n - k - 1,pOut);` |
|        9 | 1493 | `	return 1;` |
|       53 | 1494 | `}` |
|        - | 1495 | `/*` |
|        - | 1496 | ` * Emit one parsed component into pValue, replacing control bytes with '_'.` |
|        - | 1497 | ` *` |
|        - | 1498 | ` * php runs php_replace_controlchars_ex over every string component it returns,` |
|        - | 1499 | ` * so a URL carrying a raw NUL, newline or DEL cannot smuggle it through into` |
|        - | 1500 | ` * whatever the caller splices the component into (a header, a log line, a` |
|        - | 1501 | ` * redirect). Bytes >= 0x80 are deliberately left alone -- php only folds the` |
|        - | 1502 | ` * ASCII control range.` |
|        - | 1503 | ` */` |
|      164 | 1504 | `static void VmUrlSetComponent(ph7_value *pValue,const SyString *pComp)` |
|        1 | 1505 | `{` |
|      165 | 1506 | `	const char *z = pComp->zString;` |
|      165 | 1507 | `	sxu32 n = pComp->nByte,i,iRun = 0;` |
|      165 | 1508 | `	if( n < 1 \|\| z == 0 ){` |
|        3 | 1509 | `		ph7_value_string(pValue,"",0);` |
|        3 | 1510 | `		return;` |
|        - | 1511 | `	}` |
|      955 | 1512 | `	for( i = 0 ; i < n ; ++i ){` |
|      793 | 1513 | `		unsigned char c = (unsigned char)z[i];` |
|      793 | 1514 | `		if( c < 0x20 \|\| c == 0x7f ){` |
|        3 | 1515 | `			if( i > iRun ){` |
|        3 | 1516 | `				ph7_value_string(pValue,&z[iRun],(int)(i - iRun));` |
|        1 | 1517 | `			}` |
|        3 | 1518 | `			ph7_value_string(pValue,"_",1);` |
|        3 | 1519 | `			iRun = i + 1;` |
|        1 | 1520 | `		}` |
|      397 | 1521 | `	}` |
|      163 | 1522 | `	if( n > iRun ){` |
|      163 | 1523 | `		ph7_value_string(pValue,&z[iRun],(int)(n - iRun));` |
|       81 | 1524 | `	}` |
|       83 | 1525 | `}` |
|      104 | 1526 | `PH7_PRIVATE int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1527 | `{` |
|        - | 1528 | `	const char *zStr; /* Input string */` |
|        - | 1529 | `	VmUrlParts sUrl;  /* Parse of the given URI */` |
|        - | 1530 | `	SyString *pComp;` |
|        - | 1531 | `	int bHave;` |
|        - | 1532 | `	int nLen;` |
|      105 | 1533 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 1534 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 1535 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 1536 | `		return PH7_OK;` |
|        - | 1537 | `	}` |
|        - | 1538 | `	/* Extract the given URI. An empty string is NOT a failure: php parses it as` |
|        - | 1539 | `	 * an empty path. */` |
|      105 | 1540 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|      105 | 1541 | `	if( nLen < 0 ){` |
|      ! 0 | 1542 | `		nLen = 0;` |
|      ! 0 | 1543 | `	}` |
|      105 | 1544 | `	if( !VmUrlSplit(zStr,nLen,&sUrl) ){` |
|        - | 1545 | `		/* Malformed input,return FALSE */` |
|       13 | 1546 | `		ph7_result_bool(pCtx,0);` |
|       13 | 1547 | `		return PH7_OK;` |
|        - | 1548 | `	}` |
|      103 | 1549 | `	if( nArg > 1 && ph7_value_to_int(apArg[1]) >= 0 ){` |
|        - | 1550 | `		/* Refer to constant.c for constants values (php's PHP_URL_* are 0-based;` |
|        - | 1551 | `		 * PHL used to number them from 1, so every literal component id selected` |
|        - | 1552 | `		 * the WRONG field -- and the constants' own tests only asserted "%d",` |
|        - | 1553 | `		 * which any numbering satisfies). A negative id means "the whole array",` |
|        - | 1554 | `		 * which is what the default $component = -1 relies on. */` |
|       27 | 1555 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|       27 | 1556 | `		pComp = 0;` |
|       27 | 1557 | `		bHave = 0;` |
|       27 | 1558 | `		switch(nComponent){` |
|        3 | 1559 | `		case 0: /* PHP_URL_SCHEME */   pComp = &sUrl.sScheme;   bHave = sUrl.bScheme;   break;` |
|        5 | 1560 | `		case 1: /* PHP_URL_HOST */     pComp = &sUrl.sHost;     bHave = sUrl.bHost;     break;` |
|        2 | 1561 | `		case 2: /* PHP_URL_PORT */` |
|        5 | 1562 | `			if( sUrl.bPort ){` |
|        5 | 1563 | `				ph7_result_int(pCtx,sUrl.iPort);` |
|        3 | 1564 | `			}else{` |
|      ! 0 | 1565 | `				ph7_result_null(pCtx);` |
|        - | 1566 | `			}` |
|        5 | 1567 | `			return PH7_OK;` |
|        3 | 1568 | `		case 3: /* PHP_URL_USER */     pComp = &sUrl.sUser;     bHave = sUrl.bUser;     break;` |
|        3 | 1569 | `		case 4: /* PHP_URL_PASS */     pComp = &sUrl.sPass;     bHave = sUrl.bPass;     break;` |
|        3 | 1570 | `		case 5: /* PHP_URL_PATH */     pComp = &sUrl.sPath;     bHave = sUrl.bPath;     break;` |
|        5 | 1571 | `		case 6: /* PHP_URL_QUERY */    pComp = &sUrl.sQuery;    bHave = sUrl.bQuery;    break;` |
|        5 | 1572 | `		case 7: /* PHP_URL_FRAGMENT */ pComp = &sUrl.sFragment; bHave = sUrl.bFragment; break;` |
|        1 | 1573 | `		default:` |
|        4 | 1574 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|        - | 1575 | `				"parse_url(): Argument #2 ($component) must be a valid URL component identifier, %d given",` |
|        1 | 1576 | `				nComponent);` |
|        - | 1577 | `		}` |
|       21 | 1578 | `		if( bHave ){` |
|       19 | 1579 | `			ph7_value *pOut = ph7_context_new_scalar(pCtx);` |
|       19 | 1580 | `			if( pOut == 0 ){` |
|      ! 0 | 1581 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|      ! 0 | 1582 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 1583 | `				return PH7_OK;` |
|        - | 1584 | `			}` |
|       19 | 1585 | `			VmUrlSetComponent(pOut,pComp);` |
|       19 | 1586 | `			ph7_result_value(pCtx,pOut);` |
|       10 | 1587 | `		}else{` |
|        - | 1588 | `			/* No available value,return NULL */` |
|        3 | 1589 | `			ph7_result_null(pCtx);` |
|        - | 1590 | `		}` |
|       11 | 1591 | `	}else{` |
|        - | 1592 | `		ph7_value *pArray,*pValue;` |
|        - | 1593 | `		/* Return an associative array */` |
|       67 | 1594 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       67 | 1595 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       67 | 1596 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 1597 | `			/* Out of memory */` |
|      ! 0 | 1598 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 1599 | `			/* Return false */` |
|      ! 0 | 1600 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 1601 | `			return PH7_OK;` |
|        - | 1602 | `		}` |
|        - | 1603 | `		/* Fill the array, in php's key order. A component is emitted whenever it` |
|        - | 1604 | `		 * is PRESENT, even when empty -- parse_url("?") is ['query'=>'']. */` |
|       67 | 1605 | `		if( sUrl.bScheme ){` |
|       33 | 1606 | `			VmUrlSetComponent(pValue,&sUrl.sScheme);` |
|       33 | 1607 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|       33 | 1608 | `			ph7_value_reset_string_cursor(pValue);` |
|       16 | 1609 | `		}` |
|       67 | 1610 | `		if( sUrl.bHost ){` |
|       31 | 1611 | `			VmUrlSetComponent(pValue,&sUrl.sHost);` |
|       31 | 1612 | `			ph7_array_add_strkey_elem(pArray,"host",pValue);` |
|       31 | 1613 | `			ph7_value_reset_string_cursor(pValue);` |
|       15 | 1614 | `		}` |
|       67 | 1615 | `		if( sUrl.bPort ){` |
|       17 | 1616 | `			ph7_value_int(pValue,sUrl.iPort);` |
|       17 | 1617 | `			ph7_array_add_strkey_elem(pArray,"port",pValue);` |
|       17 | 1618 | `			ph7_value_reset_string_cursor(pValue);` |
|        8 | 1619 | `		}` |
|       67 | 1620 | `		if( sUrl.bUser ){` |
|        9 | 1621 | `			VmUrlSetComponent(pValue,&sUrl.sUser);` |
|        9 | 1622 | `			ph7_array_add_strkey_elem(pArray,"user",pValue);` |
|        9 | 1623 | `			ph7_value_reset_string_cursor(pValue);` |
|        4 | 1624 | `		}` |
|       67 | 1625 | `		if( sUrl.bPass ){` |
|        7 | 1626 | `			VmUrlSetComponent(pValue,&sUrl.sPass);` |
|        7 | 1627 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue);` |
|        7 | 1628 | `			ph7_value_reset_string_cursor(pValue);` |
|        3 | 1629 | `		}` |
|       67 | 1630 | `		if( sUrl.bPath ){` |
|       47 | 1631 | `			VmUrlSetComponent(pValue,&sUrl.sPath);` |
|       47 | 1632 | `			ph7_array_add_strkey_elem(pArray,"path",pValue);` |
|       47 | 1633 | `			ph7_value_reset_string_cursor(pValue);` |
|       23 | 1634 | `		}` |
|       67 | 1635 | `		if( sUrl.bQuery ){` |
|       13 | 1636 | `			VmUrlSetComponent(pValue,&sUrl.sQuery);` |
|       13 | 1637 | `			ph7_array_add_strkey_elem(pArray,"query",pValue);` |
|       13 | 1638 | `			ph7_value_reset_string_cursor(pValue);` |
|        6 | 1639 | `		}` |
|       67 | 1640 | `		if( sUrl.bFragment ){` |
|       13 | 1641 | `			VmUrlSetComponent(pValue,&sUrl.sFragment);` |
|       13 | 1642 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue);` |
|        6 | 1643 | `		}` |
|        - | 1644 | `		/* Return the created array */` |
|       67 | 1645 | `		ph7_result_value(pCtx,pArray);` |
|        - | 1646 | `		/* NOTE:` |
|        - | 1647 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 1648 | `		 * automatically as soon we return from this function.` |
|        - | 1649 | `		 */` |
|        - | 1650 | `	}` |
|        - | 1651 | `	/* All done */` |
|       87 | 1652 | `	return PH7_OK;` |
|       53 | 1653 | `}` |
|        - | 1654 |  |
|        - | 1655 | `/*` |
|        - | 1656 | ` * Section:` |
|        - | 1657 | ` *   Array related routines.` |
|        - | 1658 | ` * Status:` |
|        - | 1659 | ` *    Stable.` |
|        - | 1660 | ` * Note 2012-5-21 01:04:15:` |
|        - | 1661 | ` *  Array related functions that need access to the underlying` |
|        - | 1662 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 1663 | ` */` |
|        - | 1664 | `/*` |
|        - | 1665 | ` * The [compact()] function store it's state information in an instance` |
|        - | 1666 | ` * of the following structure.` |
|        - | 1667 | ` */` |
|        - | 1668 | `struct compact_data` |
|        - | 1669 | `{` |
|        - | 1670 | `	ph7_value *pArray;  /* Target array */` |
|        - | 1671 | `	int nRecCount;      /* Recursion count */` |
|        - | 1672 | `};` |
|        - | 1673 | `/*` |
|        - | 1674 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 1675 | ` */` |
|      ! 0 | 1676 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 1677 | `{` |
|      ! 0 | 1678 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 1679 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 1680 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 1681 | `	/* Act according to the hashmap value */` |
|      ! 0 | 1682 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 1683 | `		SyString sVar;` |
|      ! 0 | 1684 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 1685 | `		if( sVar.nByte > 0 ){` |
|        - | 1686 | `			/* Query the current frame */` |
|      ! 0 | 1687 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 1688 | `			/* ^` |
|        - | 1689 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 1690 | `			 */` |
|      ! 0 | 1691 | `			if( pKey ){` |
|        - | 1692 | `				/* Perform the insertion */` |
|      ! 0 | 1693 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 1694 | `			}` |
|      ! 0 | 1695 | `		}` |
|      ! 0 | 1696 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 1697 | `		int rc;` |
|        - | 1698 | `		/* Recursively traverse this array */` |
|      ! 0 | 1699 | `		pData->nRecCount++;` |
|      ! 0 | 1700 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 1701 | `		pData->nRecCount--;` |
|      ! 0 | 1702 | `		return rc;` |
|        - | 1703 | `	}` |
|      ! 0 | 1704 | `	return SXRET_OK;` |
|      ! 0 | 1705 | `}` |
|        - | 1706 | `/*` |
|        - | 1707 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 1708 | ` *  Create array containing variables and their values.` |
|        - | 1709 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 1710 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 1711 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 1712 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 1713 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 1714 | ` * Parameters` |
|        - | 1715 | ` *  $varname` |
|        - | 1716 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 1717 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 1718 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 1719 | ` *   it recursively.` |
|        - | 1720 | ` * Return` |
|        - | 1721 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 1722 | ` */` |
|        2 | 1723 | `PH7_PRIVATE int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1724 | `{` |
|        - | 1725 | `	ph7_value *pArray,*pObj;` |
|        3 | 1726 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 1727 | `	const char *zName;` |
|        - | 1728 | `	SyString sVar;` |
|        - | 1729 | `	int i,nLen;` |
|        3 | 1730 | `	if( nArg < 1 ){` |
|        - | 1731 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 1732 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1733 | `		return PH7_OK;` |
|        - | 1734 | `	}` |
|        - | 1735 | `	/* Create the array */` |
|        3 | 1736 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 1737 | `	if( pArray == 0 ){` |
|        - | 1738 | `		/* Out of memory */` |
|      ! 0 | 1739 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 1740 | `		/* Return NULL */` |
|      ! 0 | 1741 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1742 | `		return PH7_OK;` |
|        - | 1743 | `	}` |
|        - | 1744 | `	/* Perform the requested operation */` |
|        7 | 1745 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 1746 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 1747 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 1748 | `				struct compact_data sData;` |
|      ! 0 | 1749 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 1750 | `				/* Recursively walk the array */` |
|      ! 0 | 1751 | `				sData.nRecCount = 0;` |
|      ! 0 | 1752 | `				sData.pArray = pArray;` |
|      ! 0 | 1753 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 1754 | `			}` |
|      ! 0 | 1755 | `		}else{` |
|        - | 1756 | `			/* Extract variable name */` |
|        5 | 1757 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 1758 | `			if( nLen > 0 ){` |
|        5 | 1759 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 1760 | `				/* Check if the variable is available in the current frame */` |
|        5 | 1761 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 1762 | `				if( pObj ){` |
|        5 | 1763 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 1764 | `				}` |
|        2 | 1765 | `			}` |
|        - | 1766 | `		}` |
|        3 | 1767 | `	}` |
|        - | 1768 | `	/* Return the array */` |
|        3 | 1769 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 1770 | `	return PH7_OK;` |
|        2 | 1771 | `}` |
|        - | 1772 | `/*` |
|        - | 1773 | ` * The [extract()] function store it's state information in an instance` |
|        - | 1774 | ` * of the following structure.` |
|        - | 1775 | ` */` |
|        - | 1776 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 1777 | `struct extract_aux_data` |
|        - | 1778 | `{` |
|        - | 1779 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 1780 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 1781 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 1782 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 1783 | `	int iFlags;           /* Control flags */` |
|        - | 1784 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 1785 | `};` |
|        - | 1786 | `/* Forward declaration */` |
|        - | 1787 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 1788 | `/*` |
|        - | 1789 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 1790 | ` *   Import variables into the current symbol table from an array.` |
|        - | 1791 | ` * Parameters` |
|        - | 1792 | ` * $var_array` |
|        - | 1793 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 1794 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 1795 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 1796 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 1797 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 1798 | ` * $extract_type` |
|        - | 1799 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 1800 | ` *  It can be one of the following values:` |
|        - | 1801 | ` *   EXTR_OVERWRITE` |
|        - | 1802 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 1803 | ` *   EXTR_SKIP` |
|        - | 1804 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 1805 | ` *   EXTR_PREFIX_SAME` |
|        - | 1806 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 1807 | ` *   EXTR_PREFIX_ALL` |
|        - | 1808 | ` *       Prefix all variable names with prefix.` |
|        - | 1809 | ` *   EXTR_PREFIX_INVALID` |
|        - | 1810 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 1811 | ` *   EXTR_IF_EXISTS` |
|        - | 1812 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 1813 | ` *       otherwise do nothing.` |
|        - | 1814 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 1815 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 1816 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 1817 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 1818 | ` *      the current symbol table.` |
|        - | 1819 | ` * $prefix` |
|        - | 1820 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 1821 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 1822 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 1823 | ` *  underscore character.` |
|        - | 1824 | ` * Return` |
|        - | 1825 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 1826 | ` */` |
|        4 | 1827 | `PH7_PRIVATE int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1828 | `{` |
|        - | 1829 | `	extract_aux_data sAux;` |
|        - | 1830 | `	ph7_hashmap *pMap;` |
|        5 | 1831 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 1832 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 1833 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 1834 | `		return PH7_OK;` |
|        - | 1835 | `	}` |
|        - | 1836 | `	/* Point to the target hashmap */` |
|        5 | 1837 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 1838 | `	if( pMap->nEntry < 1 ){` |
|        - | 1839 | `		/* Empty map,return  0 */` |
|      ! 0 | 1840 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 1841 | `		return PH7_OK;` |
|        - | 1842 | `	}` |
|        - | 1843 | `	/* Prepare the aux data */` |
|        5 | 1844 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 1845 | `	if( nArg > 1 ){` |
|        3 | 1846 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 1847 | `		if( nArg > 2 ){` |
|      ! 0 | 1848 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 1849 | `		}` |
|        1 | 1850 | `	}` |
|        5 | 1851 | `	sAux.pVm = pCtx->pVm;` |
|        - | 1852 | `	/* Invoke the worker callback */` |
|        5 | 1853 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 1854 | `	/* Number of variables successfully imported */` |
|        5 | 1855 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 1856 | `	return PH7_OK;` |
|        3 | 1857 | `}` |
|        - | 1858 | `/*` |
|        - | 1859 | ` * Worker callback for the [extract()] function defined` |
|        - | 1860 | ` * below.` |
|        - | 1861 | ` */` |
|        8 | 1862 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 1863 | `{` |
|        9 | 1864 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 1865 | `	int iFlags = pAux->iFlags;` |
|        9 | 1866 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 1867 | `	ph7_value *pObj;` |
|        - | 1868 | `	SyString sVar;` |
|        9 | 1869 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 1870 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 1871 | `	}` |
|        - | 1872 | `	/* Perform a string cast */` |
|        9 | 1873 | `	PH7_MemObjToString(pKey);` |
|        9 | 1874 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 1875 | `		/* Unavailable variable name */` |
|      ! 0 | 1876 | `		return SXRET_OK;` |
|        - | 1877 | `	}` |
|        9 | 1878 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 1879 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 1880 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 1881 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 1882 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 1883 | `			);` |
|      ! 0 | 1884 | `	}else{` |
|       13 | 1885 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 1886 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 1887 | `	}` |
|        9 | 1888 | `	sVar.zString = pAux->zWorker;` |
|        - | 1889 | `	/* Try to extract the variable */` |
|        9 | 1890 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 1891 | `	if( pObj ){` |
|        - | 1892 | `		/* Collision */` |
|        3 | 1893 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 1894 | `			return SXRET_OK;` |
|        - | 1895 | `		}` |
|        3 | 1896 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 1897 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 1898 | `				/* Already prefixed */` |
|      ! 0 | 1899 | `				return SXRET_OK;` |
|        - | 1900 | `			}` |
|      ! 0 | 1901 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 1902 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 1903 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 1904 | `				);` |
|      ! 0 | 1905 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 1906 | `		}` |
|        2 | 1907 | `	}else{` |
|        - | 1908 | `		/* Create the variable */` |
|        7 | 1909 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 1910 | `	}` |
|        9 | 1911 | `	if( pObj ){` |
|        - | 1912 | `		/* Overwrite the old value */` |
|        9 | 1913 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 1914 | `		/* Increment counter */` |
|        9 | 1915 | `		pAux->iCount++;` |
|        4 | 1916 | `	}` |
|        9 | 1917 | `	return SXRET_OK;` |
|        5 | 1918 | `}` |
|        - | 1919 | `/*` |
|        - | 1920 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 1921 | ` * defined below.` |
|        - | 1922 | ` */` |
|        2 | 1923 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 1924 | `{` |
|        3 | 1925 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 1926 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 1927 | `	ph7_value *pObj;` |
|        - | 1928 | `	SyString sVar;` |
|        - | 1929 | `	/* Perform a string cast */` |
|        3 | 1930 | `	PH7_MemObjToString(pKey);` |
|        3 | 1931 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 1932 | `		/* Unavailable variable name */` |
|      ! 0 | 1933 | `		return SXRET_OK;` |
|        - | 1934 | `	}` |
|        3 | 1935 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 1936 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 1937 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 1938 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 1939 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 1940 | `			);` |
|        2 | 1941 | `	}else{` |
|      ! 0 | 1942 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 1943 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 1944 | `	}` |
|        3 | 1945 | `	sVar.zString = pAux->zWorker;` |
|        - | 1946 | `	/* Extract the variable */` |
|        3 | 1947 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 1948 | `	if( pObj ){` |
|        3 | 1949 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 1950 | `	}` |
|        3 | 1951 | `	return SXRET_OK;` |
|        2 | 1952 | `}` |
|        - | 1953 | `/*` |
|        - | 1954 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 1955 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 1956 | ` * Parameters` |
|        - | 1957 | ` * $types` |
|        - | 1958 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 1959 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 1960 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 1961 | ` *  POST includes the POST uploaded file information.` |
|        - | 1962 | ` *  Note:` |
|        - | 1963 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 1964 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 1965 | ` * $prefix` |
|        - | 1966 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 1967 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 1968 | ` *  variable named $pref_userid.` |
|        - | 1969 | ` * Return` |
|        - | 1970 | ` *  TRUE on success or FALSE on failure.` |
|        - | 1971 | ` */` |
|        2 | 1972 | `PH7_PRIVATE int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1973 | `{` |
|        - | 1974 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 1975 | `	extract_aux_data sAux;` |
|        - | 1976 | `	int nLen,nPrefixLen;` |
|        - | 1977 | `	ph7_value *pSuper;` |
|        - | 1978 | `	ph7_vm *pVm;` |
|        - | 1979 | `	/* By default import only $_GET variables  */` |
|        3 | 1980 | `	zImport = "G";` |
|        3 | 1981 | `	nLen = (int)sizeof(char);` |
|        3 | 1982 | `	zPrefix = 0;` |
|        3 | 1983 | `	nPrefixLen = 0;` |
|        3 | 1984 | `	if( nArg > 0 ){` |
|        3 | 1985 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 1986 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 1987 | `		}` |
|        3 | 1988 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 1989 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 1990 | `		}` |
|        1 | 1991 | `	}` |
|        - | 1992 | `	/* Point to the underlying VM */` |
|        3 | 1993 | `	pVm = pCtx->pVm;` |
|        - | 1994 | `	/* Initialize the aux data */` |
|        3 | 1995 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 1996 | `	sAux.zPrefix = zPrefix;` |
|        3 | 1997 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 1998 | `	sAux.pVm = pVm;` |
|        - | 1999 | `	/* Extract */` |
|        3 | 2000 | `	zEnd = &zImport[nLen];` |
|        5 | 2001 | `	while( zImport < zEnd ){` |
|        3 | 2002 | `		int c = zImport[0];` |
|        3 | 2003 | `		pSuper = 0;` |
|        3 | 2004 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 2005 | `			/* Import $_GET variables */` |
|        3 | 2006 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 2007 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 2008 | `			/* Import $_POST variables */` |
|      ! 0 | 2009 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 2010 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 2011 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 2012 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 2013 | `		}` |
|        3 | 2014 | `		if( pSuper ){` |
|        - | 2015 | `			/* Iterate throw array entries */` |
|        3 | 2016 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 2017 | `		}` |
|        - | 2018 | `		/* Advance the cursor */` |
|        3 | 2019 | `		zImport++;` |
|        1 | 2020 | `	}` |
|        - | 2021 | `	/* All done,return TRUE*/` |
|        3 | 2022 | `	ph7_result_bool(pCtx,0);` |
|        3 | 2023 | `	return PH7_OK;` |
|        1 | 2024 | `}` |
|        - | 2025 |  |
