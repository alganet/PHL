# src/ph7/vm_builtin_lang.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 945/1165 lines (81.12%)

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
|       34 |   24 | `PH7_PRIVATE int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 |   25 | `{` |
|        - |   26 | `	const char *zName;` |
|       38 |   27 | `	int nLen = 0;` |
|       38 |   28 | `	int res = 0;` |
|       38 |   29 | `	if( nArg < 1 ){` |
|        - |   30 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |   31 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |   32 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |   33 | `		return SXRET_OK;` |
|        - |   34 | `	}` |
|        - |   35 | `	/* Extract constant name */` |
|       38 |   36 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |   37 | `	/* Perform the lookup */` |
|       38 |   38 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |   39 | `		/* Already defined */` |
|       32 |   40 | `		res = 1;` |
|       14 |   41 | `	}` |
|       38 |   42 | `	ph7_result_bool(pCtx,res);` |
|       38 |   43 | `	return SXRET_OK;` |
|       21 |   44 | `}` |
|        - |   45 | `/*` |
|        - |   46 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |   47 | ` * below.` |
|        - |   48 | ` */` |
|       32 |   49 | `PH7_PRIVATE void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        3 |   50 | `{` |
|       35 |   51 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |   52 | `	/* Expand constant value */` |
|       35 |   53 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       35 |   54 | `}` |
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
|       26 |   66 | `PH7_PRIVATE int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 |   67 | `{` |
|        - |   68 | `	const char *zName;  /* Constant name */` |
|        - |   69 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       29 |   70 | `	int nLen = 0;       /* Name length */` |
|        - |   71 | `	sxi32 rc;` |
|       29 |   72 | `	if( nArg < 2 ){` |
|        - |   73 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |   74 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |   75 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |   76 | `		return SXRET_OK;` |
|        - |   77 | `	}` |
|       29 |   78 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |   79 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |   80 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |   81 | `		return SXRET_OK;` |
|        - |   82 | `	}` |
|        - |   83 | `	/* Extract constant name */` |
|       29 |   84 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       29 |   85 | `	if( nLen < 1 ){` |
|      ! 0 |   86 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |   87 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |   88 | `		return SXRET_OK;` |
|        - |   89 | `	}` |
|        - |   90 | `	/* Duplicate constant value */` |
|       29 |   91 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       29 |   92 | `	if( pValue == 0 ){` |
|      ! 0 |   93 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |   94 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |   95 | `		return SXRET_OK;` |
|        - |   96 | `	}` |
|        - |   97 | `	/* Initialize the memory object */` |
|       29 |   98 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |   99 | `	/* Register the constant */` |
|        - |  100 | `	{` |
|        - |  101 | `		SyString sConsName;` |
|       29 |  102 | `		SyStringInitFromBuf(&sConsName,zName,(sxu32)nLen);` |
|       42 |  103 | `		rc = PH7_VmRegisterConstantEx(pCtx->pVm,&sConsName,VmExpandUserConstant,pValue,` |
|       26 |  104 | `			(SyString *)SySetPeek(&pCtx->pVm->aFiles),0,1);` |
|        - |  105 | `	}` |
|       29 |  106 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  107 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  108 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  109 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  110 | `		return SXRET_OK;` |
|        - |  111 | `	}` |
|        - |  112 | `	/* Duplicate constant value */` |
|       29 |  113 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       29 |  114 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
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
|       29 |  145 | `	ph7_result_bool(pCtx,1);` |
|       29 |  146 | `	return SXRET_OK;` |
|       16 |  147 | `}` |
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
|       22 |  198 | `static ph7_value * VmEnumFindCaseByValue(ph7_vm *pVm,ph7_class *pClass,ph7_value *pNeedle)` |
|        1 |  199 | `{` |
|       23 |  200 | `	ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|        - |  201 | `	sxu32 n;` |
|       51 |  202 | `	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|       43 |  203 | `		ph7_value *pVal = VmEnumCaseBackingValue(pVm,apCase[n]);` |
|       43 |  204 | `		int bMatch = 0;` |
|       43 |  205 | `		if( pVal ){` |
|       43 |  206 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|        3 |  207 | `				bMatch = (pNeedle->iFlags & MEMOBJ_INT) && pVal->x.iVal == pNeedle->x.iVal;` |
|        2 |  208 | `			}else{` |
|       61 |  209 | `				bMatch = (pNeedle->iFlags & MEMOBJ_STRING)` |
|       40 |  210 | `					&& SyBlobLength(&pVal->sBlob) == SyBlobLength(&pNeedle->sBlob)` |
|       60 |  211 | `					&& SyMemcmp(SyBlobData(&pVal->sBlob),SyBlobData(&pNeedle->sBlob),` |
|       28 |  212 | `						SyBlobLength(&pNeedle->sBlob)) == 0;` |
|        - |  213 | `			}` |
|       21 |  214 | `		}` |
|       43 |  215 | `		if( bMatch ){` |
|       15 |  216 | `			return (ph7_value *)SySetAt(&pVm->aMemObj,apCase[n]->nIdx);` |
|        - |  217 | `		}` |
|       15 |  218 | `	}` |
|        9 |  219 | `	return 0;` |
|       12 |  220 | `}` |
|        - |  221 | `/* static from(int\|string $value) / static tryFrom(int\|string $value) */` |
|       22 |  222 | `static int VmEnumFromCommon(ph7_context *pCtx,int nArg,ph7_value **apArg,int bTry)` |
|        1 |  223 | `{` |
|       23 |  224 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  225 | `	ph7_class *pClass;` |
|        - |  226 | `	ph7_value *pFound;` |
|        - |  227 | `	sxi32 rc;` |
|       23 |  228 | `	if( nArg < 2 \|\| (pClass = VmExtractEnumClass(pVm,apArg[0])) == 0 ){` |
|      ! 0 |  229 | `		ph7_result_null(pCtx);` |
|      ! 0 |  230 | `		return SXRET_OK;` |
|        - |  231 | `	}` |
|       23 |  232 | `	rc = VmEnumMaterialize(pVm,pClass);` |
|       23 |  233 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  234 | `		return rc;` |
|        - |  235 | `	}` |
|       23 |  236 | `	pFound = VmEnumFindCaseByValue(pVm,pClass,apArg[1]);` |
|       23 |  237 | `	if( pFound ){` |
|       15 |  238 | `		ph7_result_value(pCtx,pFound);` |
|       15 |  239 | `		return SXRET_OK;` |
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
|       12 |  255 | `}` |
|       16 |  256 | `PH7_PRIVATE int vm_builtin_enum_from(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  257 | `{` |
|       17 |  258 | `	return VmEnumFromCommon(pCtx,nArg,apArg,FALSE);` |
|        1 |  259 | `}` |
|        6 |  260 | `PH7_PRIVATE int vm_builtin_enum_tryfrom(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  261 | `{` |
|        7 |  262 | `	return VmEnumFromCommon(pCtx,nArg,apArg,TRUE);` |
|        1 |  263 | `}` |
|        - |  264 | `/*` |
|        - |  265 | ` * bool enum_exists(string $enum, bool $autoload = true)` |
|        - |  266 | ` *  TRUE only for a declared enum (PHP 8.1); a plain class/interface is FALSE.` |
|        - |  267 | ` */` |
|        8 |  268 | `PH7_PRIVATE int vm_builtin_enum_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  269 | `{` |
|       10 |  270 | `	ph7_class *pClass = 0;` |
|       10 |  271 | `	if( nArg > 0 ){` |
|       10 |  272 | `		pClass = VmExtractEnumClass(pCtx->pVm,apArg[0]);` |
|        4 |  273 | `	}` |
|       10 |  274 | `	ph7_result_bool(pCtx,pClass != 0);` |
|       10 |  275 | `	return SXRET_OK;` |
|        2 |  276 | `}` |
|       96 |  277 | `PH7_PRIVATE int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 |  278 | `{` |
|        - |  279 | `	SyHashEntry *pEntry;` |
|        - |  280 | `	ph7_constant *pCons;` |
|        - |  281 | `	const char *zName; /* Constant name */` |
|        - |  282 | `	ph7_value sVal;    /* Constant value */` |
|        - |  283 | `	int nLen;` |
|       99 |  284 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  285 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  286 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  287 | `		ph7_result_null(pCtx);` |
|      ! 0 |  288 | `		return SXRET_OK;` |
|        - |  289 | `	}` |
|        - |  290 | `	/* Extract the constant name */` |
|       99 |  291 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  292 | `	/* Class-constant form "C::K" (band A #4): resolve the class — interfaces` |
|        - |  293 | `	 * included — and read the mounted constant slot; php throws a catchable` |
|        - |  294 | `	 * Error for an unknown class or constant (pre-fix this path warned` |
|        - |  295 | `	 * "Undefined constant" and returned NULL without ever looking at the` |
|        - |  296 | `	 * class). */` |
|        - |  297 | `	{` |
|        - |  298 | `		int iSep;` |
|     1249 |  299 | `		for( iSep = 0; iSep + 1 < nLen; iSep++ ){` |
|     1167 |  300 | `			if( zName[iSep] == ':' && zName[iSep+1] == ':' ){` |
|       15 |  301 | `				break;` |
|        - |  302 | `			}` |
|      578 |  303 | `		}` |
|       99 |  304 | `		if( iSep + 1 < nLen ){` |
|       15 |  305 | `			ph7_class *pClass = iSep > 0 ?` |
|       14 |  306 | `				PH7_VmExtractClass(pCtx->pVm,zName,(sxu32)iSep,FALSE,0) : 0;` |
|       15 |  307 | `			if( pClass == 0 ){` |
|        4 |  308 | `				return PH7_VmThrowException(pCtx,"Error",` |
|        1 |  309 | `					"Class \"%.*s\" not found",iSep,zName);` |
|        - |  310 | `			}` |
|       13 |  311 | `			if( iSep + 2 < nLen ){` |
|        - |  312 | `				/* constant("C::NAME") names a class CONSTANT or enum case (hConst),` |
|        - |  313 | `				 * never a property. */` |
|       19 |  314 | `				ph7_class_attr *pAttr = PH7_ClassExtractConstant(pClass,` |
|       12 |  315 | `					&zName[iSep+2],(sxu32)(nLen - iSep - 2));` |
|       13 |  316 | `				if( pAttr && pAttr->nIdx == SXU32_HIGH ){` |
|        - |  317 | `					/* Unmaterialized: enum case → materialize the singletons` |
|        - |  318 | `					 * (all of them: constant("S::A") is a direct access, like` |
|        - |  319 | `					 * OP_MEMBER); plain constant → run its initializer. */` |
|        - |  320 | `					sxi32 rcEnum;` |
|        7 |  321 | `					if( pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE ){` |
|      ! 0 |  322 | `						rcEnum = VmEnumMaterialize(pCtx->pVm,pClass);` |
|      ! 0 |  323 | `					}else{` |
|        7 |  324 | `						rcEnum = VmClassConstEvalOnDemand(pCtx->pVm,pClass,pAttr);` |
|        - |  325 | `					}` |
|        7 |  326 | `					if( rcEnum != SXRET_OK ){` |
|      ! 0 |  327 | `						return rcEnum;` |
|        - |  328 | `					}` |
|        3 |  329 | `				}` |
|       13 |  330 | `				if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) ){` |
|       11 |  331 | `					ph7_value *pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pAttr->nIdx);` |
|       11 |  332 | `					if( pValue ){` |
|       11 |  333 | `						if( SySetUsed(&pAttr->aAttrs) > 0 ){` |
|        - |  334 | `							/* #[\Deprecated] warns through constant() too (php) */` |
|        3 |  335 | `							VmDeprecatedConstNotice(pCtx->pVm,pClass,pAttr);` |
|        1 |  336 | `						}` |
|       11 |  337 | `						ph7_result_value(pCtx,pValue);` |
|       11 |  338 | `						return SXRET_OK;` |
|        - |  339 | `					}` |
|      ! 0 |  340 | `				}` |
|        1 |  341 | `			}` |
|        4 |  342 | `			return PH7_VmThrowException(pCtx,"Error",` |
|        1 |  343 | `				"Undefined constant %.*s",nLen,zName);` |
|        - |  344 | `		}` |
|        - |  345 | `	}` |
|        - |  346 | `	/* Perform the query */` |
|       85 |  347 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       85 |  348 | `	if( pEntry == 0 ){` |
|        - |  349 | `		/* php 8: a catchable Error, not a notice + NULL (band A #4) */` |
|        8 |  350 | `		return PH7_VmThrowException(pCtx,"Error",` |
|        2 |  351 | `			"Undefined constant \"%.*s\"",nLen,zName);` |
|        - |  352 | `	}` |
|       81 |  353 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  354 | `	/* Point to the structure that describe the constant */` |
|       81 |  355 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  356 | `	/* Extract constant value by calling it's associated callback` |
|        - |  357 | `	 * (emits the #[\Deprecated] notice first when attributed, php) */` |
|       81 |  358 | `	VmExpandConstantWithNotice(pCtx->pVm,pCons,&sVal);` |
|        - |  359 | `	/* Return that value */` |
|       81 |  360 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  361 | `	/* Cleanup */` |
|       81 |  362 | `	PH7_MemObjRelease(&sVal);` |
|       81 |  363 | `	return SXRET_OK;` |
|       51 |  364 | `}` |
|        - |  365 | `/*` |
|        - |  366 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  367 | ` * defined below.` |
|        - |  368 | ` */` |
|      938 |  369 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  370 | `{` |
|      939 |  371 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  372 | `	ph7_value sName;` |
|        - |  373 | `	sxi32 rc;` |
|        - |  374 | `	/* Prepare the constant name for insertion */` |
|      939 |  375 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      939 |  376 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  377 | `	/* Perform the insertion */` |
|      939 |  378 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      939 |  379 | `	PH7_MemObjRelease(&sName);` |
|      939 |  380 | `	return rc;` |
|        1 |  381 | `}` |
|        - |  382 | `/*` |
|        - |  383 | ` * array get_defined_constants(void)` |
|        - |  384 | ` *  Returns an associative array with the names of all defined` |
|        - |  385 | ` *  constants.` |
|        - |  386 | ` * Parameters` |
|        - |  387 | ` *  NONE.` |
|        - |  388 | ` * Returns` |
|        - |  389 | ` *  Returns the names of all the constants currently defined.` |
|        - |  390 | ` */` |
|        2 |  391 | `PH7_PRIVATE int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  392 | `{` |
|        - |  393 | `	ph7_value *pArray;` |
|        - |  394 | `	/* Create the array first*/` |
|        3 |  395 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  396 | `	if( pArray == 0 ){` |
|      ! 0 |  397 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  398 | `		SXUNUSED(apArg);` |
|        - |  399 | `		/* Return NULL */` |
|      ! 0 |  400 | `		ph7_result_null(pCtx);` |
|      ! 0 |  401 | `		return SXRET_OK;` |
|        - |  402 | `	}` |
|        - |  403 | `	/* Fill the array with the defined constants */` |
|        3 |  404 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  405 | `	/* Return the created array */` |
|        3 |  406 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  407 | `	return SXRET_OK;` |
|        2 |  408 | `}` |
|        - |  409 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - |  410 | `/*` |
|        - |  411 | ` * Section:` |
|        - |  412 | ` *  Random numbers/string generators.` |
|        - |  413 | ` * Status:` |
|        - |  414 | ` *    Stable.` |
|        - |  415 | ` */` |
|        - |  416 | `/*` |
|        - |  417 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  418 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|        - |  419 | ` * implemented in src/sx/sxrand.c).` |
|        - |  420 | ` */` |
|     3652 |  421 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        5 |  422 | `{` |
|        - |  423 | `	sxu32 iNum;` |
|     3657 |  424 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     3657 |  425 | `	return iNum;` |
|        5 |  426 | `}` |
|        - |  427 | `/*` |
|        - |  428 | ` * The MT19937 generator that backs PHP's rand()/mt_rand() family. It is kept` |
|        - |  429 | ` * separate from the RC4 SyRandomness above so that srand()/mt_srand() give` |
|        - |  430 | ` * userland PHP's reproducible sequence without perturbing the engine's internal` |
|        - |  431 | ` * entropy (object ids, uniqid, quicksort pivots stay on the RC4 generator, as` |
|        - |  432 | ` * they are in PHP too — srand does not touch those).` |
|        - |  433 | ` */` |
|        - |  434 | `/*` |
|        - |  435 | ` * Reset the MT19937 state to a 32-bit seed (PHP truncates its int seed likewise).` |
|        - |  436 | ` */` |
|       36 |  437 | `PH7_PRIVATE void PH7_VmMtSrand(ph7_vm *pVm,sxu32 nSeed)` |
|        1 |  438 | `{` |
|       37 |  439 | `	SyMT19937Seed(&pVm->sMt,nSeed);` |
|       37 |  440 | `	pVm->mtSeeded = TRUE;` |
|       37 |  441 | `}` |
|        - |  442 | `/*` |
|        - |  443 | ` * Draw the next full 32-bit MT19937 word, seeding lazily from the OS CSPRNG on` |
|        - |  444 | ` * first use exactly as PHP auto-seeds when rand()/mt_rand() runs before srand().` |
|        - |  445 | ` */` |
|     1781 |  446 | `PH7_PRIVATE sxu32 PH7_VmMtRand(ph7_vm *pVm)` |
|        1 |  447 | `{` |
|     1782 |  448 | `	if( !pVm->mtSeeded ){` |
|        - |  449 | `		sxu32 nSeed;` |
|        3 |  450 | `		if( SyOSCSPRNG((void *)&nSeed,sizeof(nSeed)) != SXRET_OK ){` |
|        - |  451 | `			/* No OS entropy source: fall back to the RC4 generator's output. */` |
|      ! 0 |  452 | `			nSeed = PH7_VmRandomNum(pVm);` |
|      ! 0 |  453 | `		}` |
|        3 |  454 | `		SyMT19937Seed(&pVm->sMt,nSeed);` |
|        3 |  455 | `		pVm->mtSeeded = TRUE;` |
|        1 |  456 | `	}` |
|     1782 |  457 | `	return SyMT19937Next(&pVm->sMt);` |
|        1 |  458 | `}` |
|        - |  459 | `/*` |
|        - |  460 | ` * Map a full 32-bit draw uniformly into [0,uMax] (uMax is the range width, i.e.` |
|        - |  461 | ` * max-min). Rejection sampling against the largest unbiased ceiling, matching` |
|        - |  462 | ` * PHP's php_random_range32().` |
|        - |  463 | ` */` |
|     1658 |  464 | `static sxu32 VmMtRange32(ph7_vm *pVm,sxu32 uMax)` |
|        1 |  465 | `{` |
|        - |  466 | `	sxu32 result,limit;` |
|     1659 |  467 | `	result = PH7_VmMtRand(pVm);` |
|        - |  468 | `	/* Whole 32-bit domain: no scaling needed. */` |
|     1659 |  469 | `	if( uMax == 0xFFFFFFFFU ){` |
|      ! 0 |  470 | `		return result;` |
|        - |  471 | `	}` |
|        - |  472 | `	/* Make the range inclusive of max. */` |
|     1659 |  473 | `	uMax++;` |
|        - |  474 | `	/* Powers of two are unbiased under a plain mask. */` |
|     1659 |  475 | `	if( (uMax & (uMax - 1)) == 0 ){` |
|        7 |  476 | `		return result & (uMax - 1);` |
|        - |  477 | `	}` |
|        - |  478 | `	/* Ceiling under which 0xFFFFFFFF % uMax == 0; discard draws above it. */` |
|     1653 |  479 | `	limit = 0xFFFFFFFFU - (0xFFFFFFFFU % uMax) - 1;` |
|     1653 |  480 | `	while( result > limit ){` |
|      ! 0 |  481 | `		result = PH7_VmMtRand(pVm);` |
|      ! 0 |  482 | `	}` |
|     1653 |  483 | `	return result % uMax;` |
|      830 |  484 | `}` |
|        - |  485 | `/*` |
|        - |  486 | ` * 64-bit-wide range: assemble two draws (high word first, as PHP does) and` |
|        - |  487 | ` * reject-sample. Matches PHP's php_random_range64().` |
|        - |  488 | ` */` |
|        4 |  489 | `static sxu64 VmMtRange64(ph7_vm *pVm,sxu64 uMax)` |
|        1 |  490 | `{` |
|        - |  491 | `	sxu64 result,limit;` |
|        - |  492 | `	/* First draw fills the low word, second draw the high word — order is` |
|        - |  493 | `	 * significant and matches php's php_random_range64() assembly. */` |
|        5 |  494 | `	result = (sxu64)PH7_VmMtRand(pVm);` |
|        5 |  495 | `	result \|= (sxu64)PH7_VmMtRand(pVm) << 32;` |
|        5 |  496 | `	if( uMax == 0xFFFFFFFFFFFFFFFFULL ){` |
|      ! 0 |  497 | `		return result;` |
|        - |  498 | `	}` |
|        5 |  499 | `	uMax++;` |
|        5 |  500 | `	if( (uMax & (uMax - 1)) == 0 ){` |
|        3 |  501 | `		return result & (uMax - 1);` |
|        - |  502 | `	}` |
|        3 |  503 | `	limit = 0xFFFFFFFFFFFFFFFFULL - (0xFFFFFFFFFFFFFFFFULL % uMax) - 1;` |
|        3 |  504 | `	while( result > limit ){` |
|      ! 0 |  505 | `		result = (sxu64)PH7_VmMtRand(pVm);` |
|      ! 0 |  506 | `		result \|= (sxu64)PH7_VmMtRand(pVm) << 32;` |
|      ! 0 |  507 | `	}` |
|        3 |  508 | `	return result % uMax;` |
|        3 |  509 | `}` |
|        - |  510 | `/*` |
|        - |  511 | ` * Return a value uniformly in the inclusive range [iMin,iMax]. The caller` |
|        - |  512 | ` * guarantees iMin <= iMax. Mirrors PHP's php_mt_rand_range(): a range that fits` |
|        - |  513 | ` * in 32 bits takes the 32-bit path, a wider one the 64-bit path.` |
|        - |  514 | ` */` |
|     1662 |  515 | `PH7_PRIVATE sxi64 PH7_VmMtRandRange(ph7_vm *pVm,sxi64 iMin,sxi64 iMax)` |
|        1 |  516 | `{` |
|     1663 |  517 | `	sxu64 uMax = (sxu64)iMax - (sxu64)iMin;` |
|     1663 |  518 | `	if( uMax > 0xFFFFFFFFULL ){` |
|        5 |  519 | `		return (sxi64)(VmMtRange64(pVm,uMax) + (sxu64)iMin);` |
|        - |  520 | `	}` |
|     1659 |  521 | `	return (sxi64)((sxu64)VmMtRange32(pVm,(sxu32)uMax) + (sxu64)iMin);` |
|      832 |  522 | `}` |
|        - |  523 | `/*` |
|        - |  524 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  525 | ` * Note that the generated string is NOT null terminated.` |
|        - |  526 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|        - |  527 | ` * implemented in src/sx/sxrand.c).` |
|        - |  528 | ` */` |
|  3090852 |  529 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        5 |  530 | `{` |
|        - |  531 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  532 | `	int i;` |
|        - |  533 | `	/* Generate a binary string first */` |
|  3090857 |  534 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  535 | `	/* Turn the binary string into english based alphabet */` |
| 33999593 |  536 | `	for( i = 0 ; i < nLen ; ++i ){` |
| 30908741 |  537 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
| 15454373 |  538 | `	 }` |
|  3090857 |  539 | `}` |
|        - |  540 | `/*` |
|        - |  541 | ` * int rand()` |
|        - |  542 | ` * int mt_rand()` |
|        - |  543 | ` * int rand(int $min,int $max)` |
|        - |  544 | ` * int mt_rand(int $min,int $max)` |
|        - |  545 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  546 | ` * Parameter` |
|        - |  547 | ` *  $min` |
|        - |  548 | ` *    The lowest value to return (default: 0)` |
|        - |  549 | ` *  $max` |
|        - |  550 | ` *   The highest value to return (default: getrandmax())` |
|        - |  551 | ` * Return` |
|        - |  552 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  553 | ` * Note:` |
|        - |  554 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  555 | ` *  by te SQLite3 library.` |
|        - |  556 | ` */` |
|     1728 |  557 | `PH7_PRIVATE int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  558 | `{` |
|     1729 |  559 | `	SyString *pName = &pCtx->pFunc->sName;` |
|     3019 |  560 | `	int bMt = (pName->nByte == sizeof("mt_rand")-1` |
|     1728 |  561 | `		&& SyMemcmp(pName->zString,"mt_rand",sizeof("mt_rand")-1) == 0);` |
|        - |  562 | `	/* php accepts exactly 0 or exactly 2 arguments (min,max); 1 or 3+ is an` |
|        - |  563 | `	 * ArgumentCountError. The central arity table can't express "0 or 2", so` |
|        - |  564 | `	 * it is enforced here (was a silent wrong result for the raw draw). */` |
|     1729 |  565 | `	if( nArg == 1 \|\| nArg > 2 ){` |
|       13 |  566 | `		return PH7_VmThrowException(pCtx,` |
|        - |  567 | `			"ArgumentCountError",` |
|        - |  568 | `			"%z() expects exactly 2 arguments, %d given",` |
|        4 |  569 | `			pName, nArg` |
|        - |  570 | `			);` |
|        - |  571 | `	}` |
|     1721 |  572 | `	if( nArg == 2 ){` |
|        - |  573 | `		sxi64 iMin,iMax;` |
|        - |  574 | `		/* Signed 64-bit endpoints: the old unsigned math wrapped negative` |
|        - |  575 | `		 * ranges to huge positives (rand(-10,-1) -> ~4e9) and mis-handled` |
|        - |  576 | `		 * min==max. */` |
|     1659 |  577 | `		iMin = ph7_value_to_int64(apArg[0]);` |
|     1659 |  578 | `		iMax = ph7_value_to_int64(apArg[1]);` |
|     1659 |  579 | `		if( iMin > iMax ){` |
|        9 |  580 | `			if( bMt ){` |
|        - |  581 | `				/* mt_rand() is strict: php throws a catchable ValueError. */` |
|        5 |  582 | `				return PH7_VmThrowException(pCtx,` |
|        - |  583 | `					"ValueError",` |
|        - |  584 | `					"mt_rand(): Argument #2 ($max) must be greater than or equal to argument #1 ($min)"` |
|        - |  585 | `					);` |
|        - |  586 | `			}` |
|        - |  587 | `			/* rand() swaps the bounds for backward compatibility (php keeps` |
|        - |  588 | `			 * this quirk; only mt_rand() rejects a reversed range). */` |
|        5 |  589 | `			{ sxi64 iTmp = iMin; iMin = iMax; iMax = iTmp; }` |
|        2 |  590 | `		}` |
|        - |  591 | `		/* MT19937-backed uniform draw over [iMin,iMax], bit-for-bit as php. */` |
|     1655 |  592 | `		ph7_result_int64(pCtx,PH7_VmMtRandRange(pCtx->pVm,iMin,iMax));` |
|     1655 |  593 | `		return SXRET_OK;` |
|        - |  594 | `	}` |
|        - |  595 | `	/* No-argument form: a 31-bit value in [0, mt_getrandmax()]. php returns` |
|        - |  596 | `	 * php_mt_rand() >> 1 for the bare draw (the full 32-bit word feeds the` |
|        - |  597 | `	 * range form above, but the bare form drops the low bit). */` |
|       63 |  598 | `	ph7_result_int64(pCtx,(sxi64)(PH7_VmMtRand(pCtx->pVm) >> 1));` |
|       63 |  599 | `	return SXRET_OK;` |
|      865 |  600 | `}` |
|        - |  601 | `/*` |
|        - |  602 | ` * int getrandmax(void)` |
|        - |  603 | ` * int mt_getrandmax(void)` |
|        - |  604 | ` * int rc4_getrandmax(void)` |
|        - |  605 | ` *   Show largest possible random value` |
|        - |  606 | ` * Return` |
|        - |  607 | ` *  The largest possible random value returned by rand()/mt_rand(): php's` |
|        - |  608 | ` *  MT19937 backing makes this 2^31-1 (2147483647) for both.` |
|        - |  609 | ` */` |
|        8 |  610 | `PH7_PRIVATE int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  611 | `{` |
|        4 |  612 | `	SXUNUSED(nArg); /* cc warning */` |
|        4 |  613 | `	SXUNUSED(apArg);` |
|        - |  614 | `	/* php: PHP_MT_RAND_MAX == (1<<31)-1; bare rand()/mt_rand() draw >> 1 lands` |
|        - |  615 | `	 * exactly in [0, this]. */` |
|        9 |  616 | `	ph7_result_int64(pCtx,2147483647);` |
|        9 |  617 | `	return SXRET_OK;` |
|        1 |  618 | `}` |
|        - |  619 | `/*` |
|        - |  620 | ` * string rand_str()` |
|        - |  621 | ` * string rand_str(int $len)` |
|        - |  622 | ` *  Generate a random string (English alphabet).` |
|        - |  623 | ` * Parameter` |
|        - |  624 | ` *  $len` |
|        - |  625 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  626 | ` * Return` |
|        - |  627 | ` *   A pseudo random string.` |
|        - |  628 | ` * Note:` |
|        - |  629 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  630 | ` *  by te SQLite3 library.` |
|        - |  631 | ` *  This function is a symisc extension.` |
|        - |  632 | ` */` |
|      144 |  633 | `PH7_PRIVATE int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  634 | `{` |
|        - |  635 | `	char zString[1024];` |
|      146 |  636 | `	int iLen = 0x10;` |
|      146 |  637 | `	if( nArg > 0 ){` |
|        - |  638 | `		/* Get the desired length */` |
|      146 |  639 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      146 |  640 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  641 | `			/* Default length */` |
|        3 |  642 | `			iLen = 0x10;` |
|        1 |  643 | `		}` |
|       72 |  644 | `	}` |
|        - |  645 | `	/* Generate the random string */` |
|      146 |  646 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  647 | `	/* Return the generated string */` |
|      146 |  648 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      146 |  649 | `	return SXRET_OK;` |
|        2 |  650 | `}` |
|        - |  651 | `/*` |
|        - |  652 | ` * Reject non-numeric values (array/object/resource and non-numeric strings)` |
|        - |  653 | ` * the same way intdiv() does. Returns SXRET_OK if the value is acceptable as` |
|        - |  654 | ` * an int (PHP coerces float and numeric string silently).` |
|        - |  655 | ` */` |
|      484 |  656 | `static int VmRandomCheckIntArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgPos,const char *zParamName)` |
|        2 |  657 | `{` |
|      484 |  658 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg)` |
|      486 |  659 | `		\|\| ph7_value_is_resource(pArg) ){` |
|      ! 0 |  660 | `		return PH7_VmThrowException(pCtx,` |
|        - |  661 | `			"TypeError",` |
|        - |  662 | `			"%s(): Argument #%d (%s) must be of type int, %s given",` |
|      ! 0 |  663 | `			zFunc,iArgPos,zParamName,` |
|      ! 0 |  664 | `			ph7_type_name(pArg)` |
|        - |  665 | `			);` |
|        - |  666 | `	}` |
|      486 |  667 | `	if( ph7_value_is_string(pArg) ){` |
|        - |  668 | `		int len;` |
|        9 |  669 | `		const char *zStr = ph7_value_to_string(pArg, &len);` |
|        9 |  670 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|        7 |  671 | `			return PH7_VmThrowException(pCtx,` |
|        - |  672 | `				"TypeError",` |
|        - |  673 | `				"%s(): Argument #%d (%s) must be of type int, string given",` |
|        2 |  674 | `				zFunc,iArgPos,zParamName` |
|        - |  675 | `				);` |
|        - |  676 | `		}` |
|        2 |  677 | `	}` |
|      482 |  678 | `	return SXRET_OK;` |
|      244 |  679 | `}` |
|        - |  680 | `/*` |
|        - |  681 | ` * int random_int(int $min, int $max)` |
|        - |  682 | ` *  Generate a cryptographically secure pseudo-random integer in [$min, $max].` |
|        - |  683 | ` *  Mirrors PHP 7.0+ random_int(). Uses the OS CSPRNG via SyOSCSPRNG().` |
|        - |  684 | ` *  Distribution is uniform via rejection sampling against the smallest` |
|        - |  685 | ` *  power-of-two mask covering the range.` |
|        - |  686 | ` */` |
|      232 |  687 | `PH7_PRIVATE int vm_builtin_random_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  688 | `{` |
|        - |  689 | `	sxi64 iMin,iMax;` |
|        - |  690 | `	sxu64 uRange,uMask,uResult;` |
|        - |  691 | `	unsigned int nAttempt;` |
|        - |  692 | `	int rc;` |
|      233 |  693 | `	if( nArg != 2 ){` |
|      ! 0 |  694 | `		return PH7_VmThrowException(pCtx,` |
|        - |  695 | `			"ArgumentCountError",` |
|        - |  696 | `			"random_int() expects exactly 2 arguments, %d given",` |
|      ! 0 |  697 | `			nArg` |
|        - |  698 | `			);` |
|        - |  699 | `	}` |
|      233 |  700 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_int",1,"$min");` |
|      233 |  701 | `	if( rc != SXRET_OK ){ return rc; }` |
|      231 |  702 | `	rc = VmRandomCheckIntArg(pCtx,apArg[1],"random_int",2,"$max");` |
|      231 |  703 | `	if( rc != SXRET_OK ){ return rc; }` |
|      231 |  704 | `	iMin = ph7_value_to_int64(apArg[0]);` |
|      231 |  705 | `	iMax = ph7_value_to_int64(apArg[1]);` |
|      231 |  706 | `	if( iMin > iMax ){` |
|        3 |  707 | `		return PH7_VmThrowException(pCtx,` |
|        - |  708 | `			"ValueError",` |
|        - |  709 | `			"random_int(): Argument #1 ($min) must be less than or equal to argument #2 ($max)"` |
|        - |  710 | `			);` |
|        - |  711 | `	}` |
|      229 |  712 | `	if( iMin == iMax ){` |
|        5 |  713 | `		ph7_result_int64(pCtx,iMin);` |
|        5 |  714 | `		return SXRET_OK;` |
|        - |  715 | `	}` |
|      225 |  716 | `	uRange = (sxu64)iMax - (sxu64)iMin;` |
|      225 |  717 | `	uMask = uRange;` |
|      225 |  718 | `	uMask \|= uMask >> 1;` |
|      225 |  719 | `	uMask \|= uMask >> 2;` |
|      225 |  720 | `	uMask \|= uMask >> 4;` |
|      225 |  721 | `	uMask \|= uMask >> 8;` |
|      225 |  722 | `	uMask \|= uMask >> 16;` |
|      225 |  723 | `	uMask \|= uMask >> 32;` |
|      225 |  724 | `	uResult = 0;` |
|      324 |  725 | `	for( nAttempt = 0 ; nAttempt < 50 ; ++nAttempt ){` |
|        - |  726 | `		/* Always draw a full 8 bytes so endianness of the cast doesn't matter` |
|        - |  727 | `		 * (a 4-byte fill into a sxu64 would land in the high half on big-endian` |
|        - |  728 | `		 * and the low-half mask would always read 0). */` |
|        - |  729 | `		sxu64 uDraw;` |
|      324 |  730 | `		if( SyOSCSPRNG(&uDraw,sizeof(uDraw)) != SXRET_OK ){` |
|      ! 0 |  731 | `			return PH7_VmThrowException(pCtx,` |
|        - |  732 | `				"Random\\RandomException",` |
|        - |  733 | `				"Cannot gather sufficient random data"` |
|        - |  734 | `				);` |
|        - |  735 | `		}` |
|      324 |  736 | `		uDraw &= uMask;` |
|      324 |  737 | `		if( uDraw <= uRange ){` |
|      225 |  738 | `			uResult = uDraw;` |
|      225 |  739 | `			break;` |
|        - |  740 | `		}` |
|       49 |  741 | `	}` |
|      225 |  742 | `	if( nAttempt >= 50 ){` |
|      ! 0 |  743 | `		return PH7_VmThrowException(pCtx,` |
|        - |  744 | `			"Random\\RandomException",` |
|        - |  745 | `			"Cannot gather sufficient random data"` |
|        - |  746 | `			);` |
|        - |  747 | `	}` |
|      225 |  748 | `	ph7_result_int64(pCtx,(sxi64)((sxu64)iMin + uResult));` |
|      225 |  749 | `	return SXRET_OK;` |
|      117 |  750 | `}` |
|        - |  751 | `/*` |
|        - |  752 | ` * string random_bytes(int $length)` |
|        - |  753 | ` *  Generate $length cryptographically secure random bytes via SyOSCSPRNG().` |
|        - |  754 | ` *  Mirrors PHP 7.0+ random_bytes().` |
|        - |  755 | ` */` |
|       22 |  756 | `PH7_PRIVATE int vm_builtin_random_bytes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  757 | `{` |
|        - |  758 | `	sxi64 iLen;` |
|        - |  759 | `	unsigned char zStack[256];` |
|        - |  760 | `	void *pBuf;` |
|        - |  761 | `	int rc;` |
|       24 |  762 | `	int bHeap = 0;` |
|       24 |  763 | `	if( nArg != 1 ){` |
|      ! 0 |  764 | `		return PH7_VmThrowException(pCtx,` |
|        - |  765 | `			"ArgumentCountError",` |
|        - |  766 | `			"random_bytes() expects exactly 1 argument, %d given",` |
|      ! 0 |  767 | `			nArg` |
|        - |  768 | `			);` |
|        - |  769 | `	}` |
|       24 |  770 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_bytes",1,"$length");` |
|       24 |  771 | `	if( rc != SXRET_OK ){ return rc; }` |
|       22 |  772 | `	iLen = ph7_value_to_int64(apArg[0]);` |
|       22 |  773 | `	if( iLen < 1 ){` |
|        5 |  774 | `		return PH7_VmThrowException(pCtx,` |
|        - |  775 | `			"ValueError",` |
|        - |  776 | `			"random_bytes(): Argument #1 ($length) must be greater than 0"` |
|        - |  777 | `			);` |
|        - |  778 | `	}` |
|        - |  779 | `	/* The PH7 allocator and ph7_result_string both take sxu32/int sizes,` |
|        - |  780 | `	 * so we can't honor a length above 2 GiB. Reject early rather than` |
|        - |  781 | `	 * silently truncating via the (sxu32) cast below. */` |
|       18 |  782 | `	if( iLen > 0x7FFFFFFF ){` |
|      ! 0 |  783 | `		return PH7_VmThrowException(pCtx,` |
|        - |  784 | `			"ValueError",` |
|        - |  785 | `			"random_bytes(): Argument #1 ($length) is too large"` |
|        - |  786 | `			);` |
|        - |  787 | `	}` |
|       18 |  788 | `	if( iLen <= (sxi64)sizeof(zStack) ){` |
|       18 |  789 | `		pBuf = zStack;` |
|       10 |  790 | `	}else{` |
|      ! 0 |  791 | `		pBuf = SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)iLen);` |
|      ! 0 |  792 | `		if( pBuf == 0 ){` |
|      ! 0 |  793 | `			return PH7_VmThrowException(pCtx,` |
|        - |  794 | `				"Exception",` |
|        - |  795 | `				"random_bytes(): Failed to allocate %qd bytes",` |
|      ! 0 |  796 | `				iLen` |
|        - |  797 | `				);` |
|        - |  798 | `		}` |
|      ! 0 |  799 | `		bHeap = 1;` |
|        - |  800 | `	}` |
|       18 |  801 | `	if( SyOSCSPRNG(pBuf,(sxu32)iLen) != SXRET_OK ){` |
|      ! 0 |  802 | `		if( bHeap ){` |
|      ! 0 |  803 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 |  804 | `		}` |
|      ! 0 |  805 | `		return PH7_VmThrowException(pCtx,` |
|        - |  806 | `			"Random\\RandomException",` |
|        - |  807 | `			"Cannot gather sufficient random data"` |
|        - |  808 | `			);` |
|        - |  809 | `	}` |
|       18 |  810 | `	ph7_result_string(pCtx,(const char *)pBuf,(int)iLen);` |
|       18 |  811 | `	if( bHeap ){` |
|      ! 0 |  812 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 |  813 | `	}` |
|       18 |  814 | `	return SXRET_OK;` |
|       13 |  815 | `}` |
|        - |  816 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  817 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  818 | `/* Unique ID private data */` |
|        - |  819 | `struct unique_id_data` |
|        - |  820 | `{` |
|        - |  821 | `	ph7_context *pCtx; /* Call context */` |
|        - |  822 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  823 | `};` |
|        - |  824 | `/*` |
|        - |  825 | ` * Binary to hex consumer callback.` |
|        - |  826 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  827 | ` * defined below.` |
|        - |  828 | ` */` |
|      192 |  829 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  830 | `{` |
|      193 |  831 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  832 | `	sxu32 nBuflen;` |
|        - |  833 | `	/* Extract result buffer length */` |
|      193 |  834 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  835 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  836 | `			/*` |
|        - |  837 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  838 | `			 * string will be 13 characters long` |
|        - |  839 | `			 */` |
|       25 |  840 | `		return SXERR_ABORT;` |
|        - |  841 | `	}` |
|      169 |  842 | `	if( nBuflen > 22 ){` |
|      ! 0 |  843 | `		return SXERR_ABORT;` |
|        - |  844 | `	}` |
|        - |  845 | `	/* Safely Consume the hex stream */` |
|      169 |  846 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  847 | `	return SXRET_OK;` |
|       97 |  848 | `}` |
|        - |  849 | `/*` |
|        - |  850 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  851 | ` *  Generate a unique ID` |
|        - |  852 | ` * Parameter` |
|        - |  853 | ` * $prefix` |
|        - |  854 | ` *  Append this prefix to the generated unique ID.` |
|        - |  855 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  856 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  857 | ` * $more_entropy` |
|        - |  858 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  859 | ` *  that the result will be unique.` |
|        - |  860 | ` * Return` |
|        - |  861 | ` *  Returns the unique identifier, as a string.` |
|        - |  862 | ` */` |
|       24 |  863 | `PH7_PRIVATE int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  864 | `{` |
|        - |  865 | `	struct unique_id_data sUniq;` |
|        - |  866 | `	unsigned char zDigest[20];` |
|       25 |  867 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  868 | `	const char *zPrefix;` |
|        - |  869 | `	SHA1Context sCtx;` |
|        - |  870 | `	char zRandom[7];` |
|        - |  871 | `	int nPrefix;` |
|        - |  872 | `	int entropy;` |
|        - |  873 | `	/* Generate a random string first */` |
|       25 |  874 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  875 | `	/* Initialize fields */` |
|       25 |  876 | `	zPrefix = 0;` |
|       25 |  877 | `	nPrefix = 0;` |
|       25 |  878 | `	entropy = 0;` |
|       25 |  879 | `	if( nArg > 0 ){` |
|        - |  880 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  881 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  882 | `		if( nArg > 1 ){` |
|      ! 0 |  883 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  884 | `		}` |
|      ! 0 |  885 | `	}` |
|       25 |  886 | `	SHA1Init(&sCtx);` |
|        - |  887 | `	/* Generate the random ID */` |
|       25 |  888 | `	if( nPrefix > 0 ){` |
|      ! 0 |  889 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  890 | `	}` |
|        - |  891 | `	/* Append the random ID */` |
|       25 |  892 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  893 | `	/* Append the random string */` |
|       25 |  894 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  895 | `	/* Increment the number */` |
|       25 |  896 | `	pVm->unique_id++;` |
|       25 |  897 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  898 | `	/* Hexify the digest */` |
|       25 |  899 | `	sUniq.pCtx = pCtx;` |
|       25 |  900 | `	sUniq.entropy = entropy;` |
|       25 |  901 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  902 | `	/* All done */` |
|       25 |  903 | `	return PH7_OK;` |
|        1 |  904 | `}` |
|        - |  905 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  906 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  907 | `/*` |
|        - |  908 | ` * Section:` |
|        - |  909 | ` *  Language construct implementation as foreign functions.` |
|        - |  910 | ` * Status:` |
|        - |  911 | ` *    Stable.` |
|        - |  912 | ` */` |
|        - |  913 | `/*` |
|        - |  914 | ` * void echo($string...)` |
|        - |  915 | ` *  Output one or more messages.` |
|        - |  916 | ` * Parameters` |
|        - |  917 | ` *  $string` |
|        - |  918 | ` *   Message to output.` |
|        - |  919 | ` * Return` |
|        - |  920 | ` *  NULL.` |
|        - |  921 | ` */` |
|      ! 0 |  922 | `PH7_PRIVATE int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  923 | `{` |
|        - |  924 | `	const char *zData;` |
|      ! 0 |  925 | `	int nDataLen = 0;` |
|        - |  926 | `	ph7_vm *pVm;` |
|        - |  927 | `	int i,rc;` |
|        - |  928 | `	/* Point to the target VM */` |
|      ! 0 |  929 | `	pVm = pCtx->pVm;` |
|        - |  930 | `	/* Output */` |
|      ! 0 |  931 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        - |  932 | `		/* php's user-visible array->string warning (§2); the value still renders` |
|        - |  933 | `		 * as "Array". */` |
|      ! 0 |  934 | `		if( apArg[i]->iFlags & MEMOBJ_HASHMAP ){` |
|      ! 0 |  935 | `			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,"Array to string conversion");` |
|      ! 0 |  936 | `		}` |
|      ! 0 |  937 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  938 | `		if( nDataLen > 0 ){` |
|      ! 0 |  939 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  940 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 |  941 | `			if( rc == SXERR_ABORT ){` |
|        - |  942 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  943 | `				return PH7_ABORT;` |
|        - |  944 | `			}` |
|      ! 0 |  945 | `		}` |
|      ! 0 |  946 | `	}` |
|      ! 0 |  947 | `	return SXRET_OK;` |
|      ! 0 |  948 | `}` |
|        - |  949 | `/*` |
|        - |  950 | ` * int print($string...)` |
|        - |  951 | ` *  Output one or more messages.` |
|        - |  952 | ` * Parameters` |
|        - |  953 | ` *  $string` |
|        - |  954 | ` *   Message to output.` |
|        - |  955 | ` * Return` |
|        - |  956 | ` *  1 always.` |
|        - |  957 | ` */` |
|       38 |  958 | `PH7_PRIVATE int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 |  959 | `{` |
|        - |  960 | `	const char *zData;` |
|       41 |  961 | `	int nDataLen = 0;` |
|        - |  962 | `	ph7_vm *pVm;` |
|        - |  963 | `	int i,rc;` |
|        - |  964 | `	/* Point to the target VM */` |
|       41 |  965 | `	pVm = pCtx->pVm;` |
|        - |  966 | `	/* Output */` |
|       79 |  967 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        - |  968 | `		/* php's user-visible array->string warning (§2); the value still renders` |
|        - |  969 | `		 * as "Array". */` |
|       41 |  970 | `		if( apArg[i]->iFlags & MEMOBJ_HASHMAP ){` |
|        3 |  971 | `			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,"Array to string conversion");` |
|        1 |  972 | `		}` |
|       41 |  973 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|       41 |  974 | `		if( nDataLen > 0 ){` |
|       41 |  975 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|       41 |  976 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|       41 |  977 | `			if( rc == SXERR_ABORT ){` |
|        - |  978 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  979 | `				return PH7_ABORT;` |
|        - |  980 | `			}` |
|       19 |  981 | `		}` |
|       22 |  982 | `	}` |
|        - |  983 | `	/* Return 1 */` |
|       41 |  984 | `	ph7_result_int(pCtx,1);` |
|       41 |  985 | `	return SXRET_OK;` |
|       22 |  986 | `}` |
|        - |  987 | `/*` |
|        - |  988 | ` * void exit(string $msg)` |
|        - |  989 | ` * void exit(int $status)` |
|        - |  990 | ` * void die(string $ms)` |
|        - |  991 | ` * void die(int $status)` |
|        - |  992 | ` *   Output a message and terminate program execution.` |
|        - |  993 | ` * Parameter` |
|        - |  994 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  995 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  996 | ` *  and not printed` |
|        - |  997 | ` * Return` |
|        - |  998 | ` *  NULL` |
|        - |  999 | ` */` |
|      ! 0 | 1000 | `PH7_PRIVATE int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 1001 | `{` |
|      ! 0 | 1002 | `	if( nArg > 0 ){` |
|      ! 0 | 1003 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - | 1004 | `			const char *zData;` |
|      ! 0 | 1005 | `			int iLen = 0;` |
|        - | 1006 | `			/* Print exit message */` |
|      ! 0 | 1007 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 | 1008 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 | 1009 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - | 1010 | `			sxi32 iExitStatus;` |
|        - | 1011 | `			/* Record exit status code */` |
|      ! 0 | 1012 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 | 1013 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 | 1014 | `		}` |
|      ! 0 | 1015 | `	}` |
|        - | 1016 | `	/* Request a VM-wide halt (see PH7_OP_HALT) and abort processing` |
|        - | 1017 | `	 * immediately; the abort unwinds enclosing frames and execution units.` |
|        - | 1018 | `	 */` |
|      ! 0 | 1019 | `	pCtx->pVm->bHaltRequested = 1;` |
|      ! 0 | 1020 | `	return PH7_ABORT;` |
|      ! 0 | 1021 | `}` |
|        - | 1022 | `/*` |
|        - | 1023 | ` * Section:` |
|        - | 1024 | ` *  Version,Credits and Copyright related functions.` |
|        - | 1025 | ` * Status:` |
|        - | 1026 | ` *    Stable.` |
|        - | 1027 | ` */` |
|        - | 1028 | `/*` |
|        - | 1029 | ` * string ph7version(void)` |
|        - | 1030 | ` *  Returns the running version of the PH7 version.` |
|        - | 1031 | ` * Parameters` |
|        - | 1032 | ` *  None` |
|        - | 1033 | ` * Return` |
|        - | 1034 | ` * Current PH7 version.` |
|        - | 1035 | ` */` |
|        2 | 1036 | `PH7_PRIVATE int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1037 | `{` |
|        1 | 1038 | `	SXUNUSED(nArg);` |
|        1 | 1039 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 1040 | `	/* Current engine version */` |
|        3 | 1041 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 1042 | `	return PH7_OK;` |
|        1 | 1043 | `}` |
|        - | 1044 | `/*` |
|        - | 1045 | ` * string phpversion([ string $extension ])` |
|        - | 1046 | ` *  Returns the PHP-compatibility version PHL advertises (see PHP_COMPAT_VERSION).` |
|        - | 1047 | ` * Parameters` |
|        - | 1048 | ` *  $extension (optional): an extension name. PHL has no extension registry, so any` |
|        - | 1049 | ` *  argument yields NULL (PHP returns FALSE for an unknown extension).` |
|        - | 1050 | ` * Return` |
|        - | 1051 | ` *  The PHP-compat version string, or NULL when called with an extension argument.` |
|        - | 1052 | ` */` |
|        4 | 1053 | `PH7_PRIVATE int vm_builtin_phpversion(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1054 | `{` |
|        2 | 1055 | `	SXUNUSED(apArg); /* cc warning */` |
|        5 | 1056 | `	if( nArg > 0 ){` |
|      ! 0 | 1057 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1058 | `		return PH7_OK;` |
|        - | 1059 | `	}` |
|        5 | 1060 | `	ph7_result_string(pCtx,PHP_COMPAT_VERSION,(int)sizeof(PHP_COMPAT_VERSION) - 1);` |
|        5 | 1061 | `	return PH7_OK;` |
|        3 | 1062 | `}` |
|        - | 1063 | `/*` |
|        - | 1064 | ` * string php_sapi_name(void)` |
|        - | 1065 | ` *  Returns the type of interface (SAPI) PHL is running under.` |
|        - | 1066 | ` * Parameters` |
|        - | 1067 | ` *  None` |
|        - | 1068 | ` * Return` |
|        - | 1069 | ` *  "cli-server" while serving an HTTP request via the built-in -S server, "cli" otherwise.` |
|        - | 1070 | ` */` |
|        2 | 1071 | `PH7_PRIVATE int vm_builtin_php_sapi_name(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1072 | `{` |
|        3 | 1073 | `	const char *zSapi = pCtx->pVm->bHttpContext ? "cli-server" : "cli";` |
|        1 | 1074 | `	SXUNUSED(nArg);` |
|        1 | 1075 | `	SXUNUSED(apArg); /* cc warning */` |
|        3 | 1076 | `	ph7_result_string(pCtx,zSapi,-1);` |
|        3 | 1077 | `	return PH7_OK;` |
|        1 | 1078 | `}` |
|        - | 1079 | `/*` |
|        - | 1080 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 1081 | ` */` |
|        - | 1082 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 1083 | ` "<html><head>"\` |
|        - | 1084 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 1085 | ` "<style type=\"text/css\">"\` |
|        - | 1086 | ` "div {"\` |
|        - | 1087 | `     "border: 1px solid #cccccc;"\` |
|        - | 1088 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 1089 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 1090 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 1091 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 1092 | `     "-webkit-border-radius: 10px;"\` |
|        - | 1093 | `     "-o-border-radius: 10px;"\` |
|        - | 1094 | `     "border-radius: 10px;"\` |
|        - | 1095 | `     "padding-left: 2em;"\` |
|        - | 1096 | `     "background-color: white;"\` |
|        - | 1097 | `     "margin-left: auto;"\` |
|        - | 1098 | `     "font-family: verdana;"\` |
|        - | 1099 | `     "padding-right: 2em;"\` |
|        - | 1100 | `     "margin-right: auto;"\` |
|        - | 1101 | `     "}"\` |
|        - | 1102 | `     "body {"\` |
|        - | 1103 | `     "padding: 0.2em;"\` |
|        - | 1104 | `     "font-style: normal;"\` |
|        - | 1105 | `     "font-size: medium;"\` |
|        - | 1106 | `     "background-color: #f2f2f2;"\` |
|        - | 1107 | `     "}"\` |
|        - | 1108 | `     "hr {"\` |
|        - | 1109 | `     "border-style: solid none none;"\` |
|        - | 1110 | `     "border-width: 1px medium medium;"\` |
|        - | 1111 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 1112 | `     "height: 1px;"\` |
|        - | 1113 | `     "}"\` |
|        - | 1114 | `     "a {"\` |
|        - | 1115 | `     "color: #3366cc;"\` |
|        - | 1116 | `     "text-decoration: none;"\` |
|        - | 1117 | `     "}"\` |
|        - | 1118 | `     "a:hover {"\` |
|        - | 1119 | `     "color: #999999;"\` |
|        - | 1120 | `     "}"\` |
|        - | 1121 | `     "a:active {"\` |
|        - | 1122 | `     "color: #663399;"\` |
|        - | 1123 | `     "}"\` |
|        - | 1124 | `     "h1 {"\` |
|        - | 1125 | `     "margin: 0;"\` |
|        - | 1126 | `     "padding: 0;"\` |
|        - | 1127 | `     "font-family: Verdana;"\` |
|        - | 1128 | `     "font-weight: bold;"\` |
|        - | 1129 | `     "font-style: normal;"\` |
|        - | 1130 | `     "font-size: medium;"\` |
|        - | 1131 | `     "text-transform: capitalize;"\` |
|        - | 1132 | `     "color: #0a328c;"\` |
|        - | 1133 | `     "}"\` |
|        - | 1134 | `     "p {"\` |
|        - | 1135 | `     "margin: 0 auto;"\` |
|        - | 1136 | `     "font-size: medium;"\` |
|        - | 1137 | `     "font-style: normal;"\` |
|        - | 1138 | `     "font-family: verdana;"\` |
|        - | 1139 | `     "}"\` |
|        - | 1140 | `"</style></head><body>"\` |
|        - | 1141 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 1142 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 1143 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 1144 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 1145 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 1146 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 1147 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 1148 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 1149 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 1150 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 1151 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 1152 |  |
|        - | 1153 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 1154 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 1155 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 1156 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 1157 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 1158 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 1159 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 1160 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 1161 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 1162 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 1163 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 1164 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 1165 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 1166 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 1167 |  |
|        - | 1168 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 1169 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 1170 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 1171 | `"&nbsp;*<br>"\` |
|        - | 1172 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 1173 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 1174 | `"&nbsp;* are met:<br>"\` |
|        - | 1175 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 1176 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 1177 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 1178 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 1179 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 1180 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 1181 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 1182 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 1183 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 1184 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 1185 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 1186 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 1187 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 1188 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 1189 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 1190 | `"&nbsp;*<br>"\` |
|        - | 1191 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 1192 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 1193 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 1194 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 1195 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 1196 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 1197 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 1198 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 1199 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 1200 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 1201 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 1202 | `"&nbsp;*/<br>"\` |
|        - | 1203 | `"</span></small></small></p>"\` |
|        - | 1204 | `"</div></body></html>"` |
|        - | 1205 | `/*` |
|        - | 1206 | ` * bool ph7credits(void)` |
|        - | 1207 | ` * bool ph7info(void)` |
|        - | 1208 | ` * bool ph7copyright(void)` |
|        - | 1209 | ` *  Prints out the credits for PH7 engine` |
|        - | 1210 | ` * Parameters` |
|        - | 1211 | ` *  None` |
|        - | 1212 | ` * Return` |
|        - | 1213 | ` *  Always TRUE` |
|        - | 1214 | ` */` |
|        2 | 1215 | `PH7_PRIVATE int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1216 | `{` |
|        3 | 1217 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 1218 | `	/* Expand the HTML page above*/` |
|        3 | 1219 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 1220 | `	ph7_context_output_format(` |
|        1 | 1221 | `		pCtx,` |
|        - | 1222 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 1223 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 1224 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 1225 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 1226 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 1227 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 1228 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 1229 | `#ifdef __WINNT__` |
|        - | 1230 | `		"Windows NT"` |
|        - | 1231 | `#elif defined(__UNIXES__)` |
|        - | 1232 | `		"UNIX-Like"` |
|        - | 1233 | `#else` |
|        - | 1234 | `		"Other OS"` |
|        - | 1235 | `#endif` |
|        - | 1236 | `		);` |
|        3 | 1237 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 1238 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 1239 | `	SXUNUSED(apArg);` |
|        - | 1240 | `	/* Return TRUE */` |
|        - | 1241 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 1242 | `	return PH7_OK;` |
|        1 | 1243 | `}` |
|        - | 1244 | `/*` |
|        - | 1245 | ` * Section:` |
|        - | 1246 | ` *    URL related routines.` |
|        - | 1247 | ` * Status:` |
|        - | 1248 | ` *    Stable.` |
|        - | 1249 | ` */` |
|        - | 1250 | `/*` |
|        - | 1251 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 1252 | ` *  Parse a URL and return its fields.` |
|        - | 1253 | ` * Parameters` |
|        - | 1254 | ` *  $url` |
|        - | 1255 | ` *   The URL to parse.` |
|        - | 1256 | ` * $component` |
|        - | 1257 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 1258 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 1259 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 1260 | ` *  in which case the return value will be an integer).` |
|        - | 1261 | ` * Return` |
|        - | 1262 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 1263 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 1264 | ` *  this array are:` |
|        - | 1265 | ` *   scheme - e.g. http` |
|        - | 1266 | ` *   host` |
|        - | 1267 | ` *   port` |
|        - | 1268 | ` *   user` |
|        - | 1269 | ` *   pass` |
|        - | 1270 | ` *   path` |
|        - | 1271 | ` *   query - after the question mark ?` |
|        - | 1272 | ` *   fragment - after the hashmark #` |
|        - | 1273 | ` * Note:` |
|        - | 1274 | ` *  FALSE is returned on failure.` |
|        - | 1275 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 1276 | ` *  with the standard PHP engine.` |
|        - | 1277 | ` */` |
|        - | 1278 | `/*` |
|        - | 1279 | ` * parse_url() component set.` |
|        - | 1280 | ` *` |
|        - | 1281 | ` * A bare SyString cannot express "present but empty", which php needs:` |
|        - | 1282 | ` * parse_url("") is ['path'=>''] and parse_url("?") is ['query'=>''], both` |
|        - | 1283 | ` * distinct from the component being absent. So presence is tracked separately.` |
|        - | 1284 | ` */` |
|        - | 1285 | `typedef struct VmUrlParts VmUrlParts;` |
|        - | 1286 | `struct VmUrlParts` |
|        - | 1287 | `{` |
|        - | 1288 | `	SyString sScheme,sUser,sPass,sHost,sPath,sQuery,sFragment;` |
|        - | 1289 | `	int iPort;     /* Resolved port, meaningful only when bPort is set */` |
|        - | 1290 | `	sxu8 bScheme,bUser,bPass,bHost,bPort,bPath,bQuery,bFragment;` |
|        - | 1291 | `};` |
|      256 | 1292 | `static int VmUrlIsAlnum(int c)` |
|        1 | 1293 | `{` |
|      257 | 1294 | `	return (c >= '0' && c <= '9') \|\| (c >= 'a' && c <= 'z') \|\| (c >= 'A' && c <= 'Z');` |
|        1 | 1295 | `}` |
|        4 | 1296 | `static int VmUrlIsAlpha(int c)` |
|        1 | 1297 | `{` |
|        5 | 1298 | `	return (c >= 'a' && c <= 'z') \|\| (c >= 'A' && c <= 'Z');` |
|        1 | 1299 | `}` |
|        - | 1300 | `/* scheme = ALNUM *( ALNUM / "+" / "-" / "." ) -- php accepts a digit first. */` |
|      256 | 1301 | `static int VmUrlIsSchemeByte(int c)` |
|        1 | 1302 | `{` |
|      257 | 1303 | `	return VmUrlIsAlnum(c) \|\| c == '+' \|\| c == '-' \|\| c == '.';` |
|        1 | 1304 | `}` |
|        - | 1305 | `/*` |
|        - | 1306 | ` * Resolve the port span that followed the ':' in an authority.` |
|        - | 1307 | ` *` |
|        - | 1308 | ` * Returns 1 (usable port in *piPort), 0 (the span is empty, so there is simply` |
|        - | 1309 | ` * no port) or -1 (php rejects the whole URL). php is lenient about what follows` |
|        - | 1310 | ` * the digits -- ":8a" and ":8 0" both yield 8 -- but demands at least one digit` |
|        - | 1311 | ` * and a value that fits a port, so ":abc", ":-80" and ":65536" are all failures.` |
|        - | 1312 | ` */` |
|       42 | 1313 | `static int VmUrlParsePort(const char *z,int n,int *piPort)` |
|        1 | 1314 | `{` |
|       43 | 1315 | `	int i = 0,iVal = 0,nDigit = 0;` |
|       43 | 1316 | `	if( n < 1 ){` |
|      ! 0 | 1317 | `		return 0;` |
|        - | 1318 | `	}` |
|       64 | 1319 | `	while( i < n && (z[i] == ' ' \|\| z[i] == '\t') ){` |
|      ! 0 | 1320 | `		i++;` |
|      ! 0 | 1321 | `	}` |
|       43 | 1322 | `	if( i < n && (z[i] == '+' \|\| z[i] == '-') ){` |
|      ! 0 | 1323 | `		if( z[i] == '-' ){` |
|      ! 0 | 1324 | `			return -1;` |
|        - | 1325 | `		}` |
|      ! 0 | 1326 | `		i++;` |
|      ! 0 | 1327 | `	}` |
|      161 | 1328 | `	while( i < n && z[i] >= '0' && z[i] <= '9' ){` |
|      119 | 1329 | `		iVal = iVal * 10 + (z[i] - '0');` |
|      119 | 1330 | `		if( iVal > 65535 ){` |
|      ! 0 | 1331 | `			return -1; /* Out of range, and this also caps the accumulator */` |
|        - | 1332 | `		}` |
|      119 | 1333 | `		nDigit++;` |
|      119 | 1334 | `		i++;` |
|        1 | 1335 | `	}` |
|       43 | 1336 | `	if( nDigit < 1 ){` |
|        3 | 1337 | `		return -1;` |
|        - | 1338 | `	}` |
|       41 | 1339 | `	*piPort = iVal;` |
|       41 | 1340 | `	return 1;` |
|       22 | 1341 | `}` |
|        - | 1342 | `/*` |
|        - | 1343 | ` * Split an authority -- "[user[:pass]@]host[:port]" -- into pOut.` |
|        - | 1344 | ` * Returns 0 for the malformed input php reports as FALSE.` |
|        - | 1345 | ` */` |
|       66 | 1346 | `static int VmUrlParseAuthority(const char *z,int n,VmUrlParts *pOut,int bPortKnown)` |
|        1 | 1347 | `{` |
|        - | 1348 | `	const char *zHost;` |
|       67 | 1349 | `	int i,iAt = -1,iColon = -1,iSep = -1,nHost,iPort = 0,rc;` |
|        - | 1350 | `	/* php splits the credentials at the LAST '@' ("//a@b@c" is user "a@b") */` |
|      699 | 1351 | `	for( i = 0 ; i < n ; ++i ){` |
|      633 | 1352 | `		if( z[i] == '@' ){` |
|       25 | 1353 | `			iAt = i;` |
|       12 | 1354 | `		}` |
|      317 | 1355 | `	}` |
|       67 | 1356 | `	if( iAt >= 0 ){` |
|        - | 1357 | `		/* and the user from the password at the FIRST ':' before it */` |
|      109 | 1358 | `		for( i = 0 ; i < iAt ; ++i ){` |
|      107 | 1359 | `			if( z[i] == ':' ){` |
|       23 | 1360 | `				iColon = i;` |
|       23 | 1361 | `				break;` |
|        - | 1362 | `			}` |
|       43 | 1363 | `		}` |
|       25 | 1364 | `		if( iColon >= 0 ){` |
|       23 | 1365 | `			SyStringInitFromBuf(&pOut->sUser,z,(sxu32)iColon);` |
|       23 | 1366 | `			SyStringInitFromBuf(&pOut->sPass,&z[iColon+1],(sxu32)(iAt - iColon - 1));` |
|       23 | 1367 | `			pOut->bUser = pOut->bPass = 1;` |
|       12 | 1368 | `		}else{` |
|        3 | 1369 | `			SyStringInitFromBuf(&pOut->sUser,z,(sxu32)iAt);` |
|        3 | 1370 | `			pOut->bUser = 1;` |
|        - | 1371 | `		}` |
|       25 | 1372 | `		z += iAt + 1;` |
|       25 | 1373 | `		n -= iAt + 1;` |
|       12 | 1374 | `	}` |
|       67 | 1375 | `	zHost = z;` |
|       67 | 1376 | `	nHost = n;` |
|       67 | 1377 | `	if( !(n > 1 && z[0] == '[' && z[n-1] == ']') ){` |
|        - | 1378 | `		/* Unless a bracketed IPv6 literal fills the WHOLE authority -- in which` |
|        - | 1379 | `		 * case php keeps the brackets in "host" and scans no port, so every ':'` |
|        - | 1380 | `		 * inside belongs to the address -- the port hangs off the LAST ':'.` |
|        - | 1381 | `		 * php decides that on the first and last byte alone, which is why` |
|        - | 1382 | `		 * "//[:]\|]" is a single host while "//[::1]:8080" splits a port off. */` |
|      507 | 1383 | `		for( i = 0 ; i < n ; ++i ){` |
|      443 | 1384 | `			if( z[i] == ':' ){` |
|       43 | 1385 | `				iSep = i;` |
|       21 | 1386 | `			}` |
|      222 | 1387 | `		}` |
|       65 | 1388 | `		if( iSep >= 0 ){` |
|        - | 1389 | `			/* The host is trimmed at the ':' whether or not the port was already` |
|        - | 1390 | `			 * resolved by the caller. */` |
|       43 | 1391 | `			nHost = iSep;` |
|       43 | 1392 | `			if( !bPortKnown ){` |
|       37 | 1393 | `				rc = VmUrlParsePort(&z[iSep+1],n - iSep - 1,&iPort);` |
|       37 | 1394 | `				if( rc < 0 ){` |
|        3 | 1395 | `					return 0;` |
|        - | 1396 | `				}` |
|       35 | 1397 | `				if( rc > 0 ){` |
|       35 | 1398 | `					pOut->iPort = iPort;` |
|       35 | 1399 | `					pOut->bPort = 1;` |
|       17 | 1400 | `				}` |
|       17 | 1401 | `			}` |
|       20 | 1402 | `		}` |
|       31 | 1403 | `	}` |
|       65 | 1404 | `	if( nHost < 1 ){` |
|        - | 1405 | `		/* php requires a non-empty host once an authority is in play, which is` |
|        - | 1406 | `		 * what makes "//", "http://" and ":80" all FALSE. */` |
|        9 | 1407 | `		return 0;` |
|        - | 1408 | `	}` |
|       57 | 1409 | `	SyStringInitFromBuf(&pOut->sHost,zHost,(sxu32)nHost);` |
|       57 | 1410 | `	pOut->bHost = 1;` |
|       57 | 1411 | `	return 1;` |
|       34 | 1412 | `}` |
|        - | 1413 | `/*` |
|        - | 1414 | ` * Split "path[?query][#fragment]". The fragment is taken FIRST and the query` |
|        - | 1415 | ` * only from what precedes it, so "#a?b" is a fragment of "a?b" with no query.` |
|        - | 1416 | ` */` |
|       80 | 1417 | `static void VmUrlParsePath(const char *z,int n,VmUrlParts *pOut)` |
|        1 | 1418 | `{` |
|       81 | 1419 | `	int i,iEnd = n;` |
|      547 | 1420 | `	for( i = 0 ; i < n ; ++i ){` |
|      501 | 1421 | `		if( z[i] == '#' ){` |
|       35 | 1422 | `			SyStringInitFromBuf(&pOut->sFragment,&z[i+1],(sxu32)(n - i - 1));` |
|       35 | 1423 | `			pOut->bFragment = 1;` |
|       35 | 1424 | `			iEnd = i;` |
|       35 | 1425 | `			break;` |
|        - | 1426 | `		}` |
|      234 | 1427 | `	}` |
|      393 | 1428 | `	for( i = 0 ; i < iEnd ; ++i ){` |
|      347 | 1429 | `		if( z[i] == '?' ){` |
|       35 | 1430 | `			SyStringInitFromBuf(&pOut->sQuery,&z[i+1],(sxu32)(iEnd - i - 1));` |
|       35 | 1431 | `			pOut->bQuery = 1;` |
|       35 | 1432 | `			iEnd = i;` |
|       35 | 1433 | `			break;` |
|        - | 1434 | `		}` |
|      157 | 1435 | `	}` |
|       81 | 1436 | `	if( iEnd > 0 ){` |
|       71 | 1437 | `		SyStringInitFromBuf(&pOut->sPath,z,(sxu32)iEnd);` |
|       71 | 1438 | `		pOut->bPath = 1;` |
|       35 | 1439 | `	}` |
|       81 | 1440 | `}` |
|        - | 1441 | `/* Parse "host[:port]" followed by an optional path/query/fragment. */` |
|       66 | 1442 | `static int VmUrlAuthorityThenPath(const char *z,int n,VmUrlParts *pOut,int bPortKnown)` |
|        1 | 1443 | `{` |
|       67 | 1444 | `	int i,iEnd = n;` |
|      699 | 1445 | `	for( i = 0 ; i < n ; ++i ){` |
|      683 | 1446 | `		if( z[i] == '/' \|\| z[i] == '?' \|\| z[i] == '#' ){` |
|       51 | 1447 | `			iEnd = i;` |
|       51 | 1448 | `			break;` |
|        - | 1449 | `		}` |
|      317 | 1450 | `	}` |
|       67 | 1451 | `	if( !VmUrlParseAuthority(z,iEnd,pOut,bPortKnown) ){` |
|       11 | 1452 | `		return 0;` |
|        - | 1453 | `	}` |
|       57 | 1454 | `	if( iEnd < n ){` |
|       47 | 1455 | `		VmUrlParsePath(&z[iEnd],n - iEnd,pOut);` |
|       23 | 1456 | `	}` |
|       57 | 1457 | `	return 1;` |
|       34 | 1458 | `}` |
|        - | 1459 | `/*` |
|        - | 1460 | ` * Resolve the port php took from the FIRST ':' of a host:port URL.` |
|        - | 1461 | ` *` |
|        - | 1462 | ` * php reads the port straight off that colon before it works out where the host` |
|        - | 1463 | ` * ends, so the digits can even belong to what becomes the path: parse_url()` |
|        - | 1464 | ` * reports port 1 for "a/:1", whose path is "/:1". Mirroring the order keeps` |
|        - | 1465 | ` * that quirk. Returns 0 for a port php rejects.` |
|        - | 1466 | ` */` |
|        6 | 1467 | `static int VmUrlPreparePort(const char *z,int k,int nEnd,VmUrlParts *pOut)` |
|        1 | 1468 | `{` |
|        7 | 1469 | `	int iPort = 0;` |
|        7 | 1470 | `	int rc = VmUrlParsePort(&z[k+1],nEnd - k - 1,&iPort);` |
|        7 | 1471 | `	if( rc < 0 ){` |
|      ! 0 | 1472 | `		return 0;` |
|        - | 1473 | `	}` |
|        7 | 1474 | `	if( rc > 0 ){` |
|        7 | 1475 | `		pOut->iPort = iPort;` |
|        7 | 1476 | `		pOut->bPort = 1;` |
|        3 | 1477 | `	}` |
|        7 | 1478 | `	return 1;` |
|        4 | 1479 | `}` |
|        - | 1480 | `/*` |
|        - | 1481 | ` * php's URL parser, as parse_url() needs it. Returns 0 where php returns FALSE.` |
|        - | 1482 | ` *` |
|        - | 1483 | ` * PH7_VmHttpSplitURI cannot serve here: it is an HTTP REQUEST-target splitter` |
|        - | 1484 | ` * (the HTTP server and filter_var share it) and assumes the input is` |
|        - | 1485 | ` * authority-first, so it read "a" as a host and "mailto:me@x.com" as` |
|        - | 1486 | ` * user:pass@host. php instead decides authority-vs-path up front: only a "//",` |
|        - | 1487 | ` * with or without a scheme before it, introduces an authority.` |
|        - | 1488 | ` */` |
|      104 | 1489 | `static int VmUrlSplit(const char *z,int n,VmUrlParts *pOut)` |
|        1 | 1490 | `{` |
|      105 | 1491 | `	int i,k = -1,bScheme,bPortForm = 0,nPortEnd = 0;` |
|      105 | 1492 | `	SyZero(pOut,sizeof(VmUrlParts));` |
|        - | 1493 | `	/* A leading "//" settles it before any colon is considered: the authority` |
|        - | 1494 | `	 * starts after the slashes. Reading the colon first turned "//h:80" into a` |
|        - | 1495 | `	 * host called "//h" and "//[::1]" into a path. */` |
|      105 | 1496 | `	if( n >= 2 && z[0] == '/' && z[1] == '/' ){` |
|       13 | 1497 | `		return VmUrlAuthorityThenPath(&z[2],n - 2,pOut,0);` |
|        - | 1498 | `	}` |
|      441 | 1499 | `	for( i = 0 ; i < n ; ++i ){` |
|      419 | 1500 | `		if( z[i] == ':' ){` |
|       71 | 1501 | `			k = i;` |
|       71 | 1502 | `			break;` |
|        - | 1503 | `		}` |
|      175 | 1504 | `	}` |
|       93 | 1505 | `	if( k == 0 && n == 1 ){` |
|        - | 1506 | `		/* A lone ":" is an EMPTY scheme, which php rejects outright -- unlike` |
|        - | 1507 | `		 * ":a" or "::", which are simply paths. */` |
|        3 | 1508 | `		return 0;` |
|        - | 1509 | `	}` |
|       91 | 1510 | `	bScheme = k > 0;` |
|      347 | 1511 | `	for( i = 0 ; bScheme && i < k ; ++i ){` |
|      257 | 1512 | `		if( !VmUrlIsSchemeByte((unsigned char)z[i]) ){` |
|      ! 0 | 1513 | `			bScheme = 0;` |
|      ! 0 | 1514 | `		}` |
|      129 | 1515 | `	}` |
|       91 | 1516 | `	if( bScheme && k + 1 == n ){` |
|        - | 1517 | `		/* "x:" -- the scheme is the whole URL */` |
|        3 | 1518 | `		SyStringInitFromBuf(&pOut->sScheme,z,(sxu32)k);` |
|        3 | 1519 | `		pOut->bScheme = 1;` |
|        3 | 1520 | `		return 1;` |
|        - | 1521 | `	}` |
|        - | 1522 | `	/* Decide whether that ':' introduces a PORT rather than a scheme: at least` |
|        - | 1523 | `	 * one digit, running to the end of the string or to a '/'. That is what` |
|        - | 1524 | `	 * makes "a:80" a host:port while "a:80?q" is a scheme with path "80", and` |
|        - | 1525 | `	 * it holds however the ':' is reached -- ":1" and "/:1" are both authorities` |
|        - | 1526 | `	 * with an empty host, which php rejects. php caps the run, so a long number` |
|        - | 1527 | `	 * stays a path: "a:123456" is a path, "a:99999" an out-of-range port. */` |
|       89 | 1528 | `	if( k >= 0 ){` |
|       67 | 1529 | `		int p = k + 1;` |
|       67 | 1530 | `		int bBeforeQuery = 1;` |
|       67 | 1531 | `		nPortEnd = k + 1;` |
|        - | 1532 | `		/* A ':' that sits inside a query or fragment is just data: "?:1" is a` |
|        - | 1533 | `		 * query of ":1", not an authority with an empty host. */` |
|      321 | 1534 | `		for( i = 0 ; i < k ; ++i ){` |
|      255 | 1535 | `			if( z[i] == '?' \|\| z[i] == '#' ){` |
|      ! 0 | 1536 | `				bBeforeQuery = 0;` |
|      ! 0 | 1537 | `				break;` |
|        - | 1538 | `			}` |
|      128 | 1539 | `		}` |
|       77 | 1540 | `		while( p < n && z[p] >= '0' && z[p] <= '9' ){` |
|       11 | 1541 | `			p++;` |
|        1 | 1542 | `		}` |
|       67 | 1543 | `		if( bBeforeQuery && p > k + 1 && (p >= n \|\| z[p] == '/') && (p - k) < 7 ){` |
|        7 | 1544 | `			bPortForm = 1;` |
|        7 | 1545 | `			nPortEnd = p;` |
|        3 | 1546 | `		}` |
|       33 | 1547 | `	}` |
|       89 | 1548 | `	if( !bScheme ){` |
|       25 | 1549 | `		if( bPortForm ){` |
|        3 | 1550 | `			if( !VmUrlPreparePort(z,k,nPortEnd,pOut) ){` |
|      ! 0 | 1551 | `				return 0;` |
|        - | 1552 | `			}` |
|        3 | 1553 | `			return VmUrlAuthorityThenPath(z,n,pOut,1);` |
|        - | 1554 | `		}` |
|       23 | 1555 | `		VmUrlParsePath(z,n,pOut);` |
|       23 | 1556 | `		if( !pOut->bPath && !pOut->bQuery && !pOut->bFragment ){` |
|        - | 1557 | `			/* php reports the empty URL as an empty PATH, not as no components */` |
|        3 | 1558 | `			SyStringInitFromBuf(&pOut->sPath,z,0);` |
|        3 | 1559 | `			pOut->bPath = 1;` |
|        1 | 1560 | `		}` |
|       23 | 1561 | `		return 1;` |
|        - | 1562 | `	}` |
|       65 | 1563 | `	if( bPortForm ){` |
|        5 | 1564 | `		if( !VmUrlPreparePort(z,k,nPortEnd,pOut) ){` |
|      ! 0 | 1565 | `			return 0;` |
|        - | 1566 | `		}` |
|        5 | 1567 | `		return VmUrlAuthorityThenPath(z,n,pOut,1);` |
|        - | 1568 | `	}` |
|       61 | 1569 | `	SyStringInitFromBuf(&pOut->sScheme,z,(sxu32)k);` |
|       61 | 1570 | `	pOut->bScheme = 1;` |
|       61 | 1571 | `	if( z[k+1] == '/' && k + 2 < n && z[k+2] == '/' ){` |
|       52 | 1572 | `		if( k + 3 < n && z[k+3] == '/' && pOut->sScheme.nByte == 4` |
|        4 | 1573 | `		 && (z[0]=='f'\|\|z[0]=='F') && (z[1]=='i'\|\|z[1]=='I')` |
|        5 | 1574 | `		 && (z[2]=='l'\|\|z[2]=='L') && (z[3]=='e'\|\|z[3]=='E') ){` |
|        - | 1575 | `			/* file:/// has no authority: the path starts at the third slash,` |
|        - | 1576 | `			 * except that a "c:" drive letter swallows it (file:///c:/x is the` |
|        - | 1577 | `			 * path "c:/x"). A '\|' in place of the ':' does NOT count. */` |
|        5 | 1578 | `			int iBase = k + 3;` |
|        5 | 1579 | `			if( iBase + 2 < n && VmUrlIsAlpha((unsigned char)z[iBase+1]) && z[iBase+2] == ':' ){` |
|      ! 0 | 1580 | `				iBase++;` |
|      ! 0 | 1581 | `			}` |
|        5 | 1582 | `			VmUrlParsePath(&z[iBase],n - iBase,pOut);` |
|        5 | 1583 | `			return 1;` |
|        - | 1584 | `		}` |
|       49 | 1585 | `		return VmUrlAuthorityThenPath(&z[k+3],n - k - 3,pOut,0);` |
|        - | 1586 | `	}` |
|        - | 1587 | `	/* "mailto:me@x.com", "x:y", "http:/x": everything after the ':' is a path */` |
|        9 | 1588 | `	VmUrlParsePath(&z[k+1],n - k - 1,pOut);` |
|        9 | 1589 | `	return 1;` |
|       53 | 1590 | `}` |
|        - | 1591 | `/*` |
|        - | 1592 | ` * Emit one parsed component into pValue, replacing control bytes with '_'.` |
|        - | 1593 | ` *` |
|        - | 1594 | ` * php runs php_replace_controlchars_ex over every string component it returns,` |
|        - | 1595 | ` * so a URL carrying a raw NUL, newline or DEL cannot smuggle it through into` |
|        - | 1596 | ` * whatever the caller splices the component into (a header, a log line, a` |
|        - | 1597 | ` * redirect). Bytes >= 0x80 are deliberately left alone -- php only folds the` |
|        - | 1598 | ` * ASCII control range.` |
|        - | 1599 | ` */` |
|      164 | 1600 | `static void VmUrlSetComponent(ph7_value *pValue,const SyString *pComp)` |
|        1 | 1601 | `{` |
|      165 | 1602 | `	const char *z = pComp->zString;` |
|      165 | 1603 | `	sxu32 n = pComp->nByte,i,iRun = 0;` |
|      165 | 1604 | `	if( n < 1 \|\| z == 0 ){` |
|        3 | 1605 | `		ph7_value_string(pValue,"",0);` |
|        3 | 1606 | `		return;` |
|        - | 1607 | `	}` |
|      955 | 1608 | `	for( i = 0 ; i < n ; ++i ){` |
|      793 | 1609 | `		unsigned char c = (unsigned char)z[i];` |
|      793 | 1610 | `		if( c < 0x20 \|\| c == 0x7f ){` |
|        3 | 1611 | `			if( i > iRun ){` |
|        3 | 1612 | `				ph7_value_string(pValue,&z[iRun],(int)(i - iRun));` |
|        1 | 1613 | `			}` |
|        3 | 1614 | `			ph7_value_string(pValue,"_",1);` |
|        3 | 1615 | `			iRun = i + 1;` |
|        1 | 1616 | `		}` |
|      397 | 1617 | `	}` |
|      163 | 1618 | `	if( n > iRun ){` |
|      163 | 1619 | `		ph7_value_string(pValue,&z[iRun],(int)(n - iRun));` |
|       81 | 1620 | `	}` |
|       83 | 1621 | `}` |
|      104 | 1622 | `PH7_PRIVATE int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1623 | `{` |
|        - | 1624 | `	const char *zStr; /* Input string */` |
|        - | 1625 | `	VmUrlParts sUrl;  /* Parse of the given URI */` |
|        - | 1626 | `	SyString *pComp;` |
|        - | 1627 | `	int bHave;` |
|        - | 1628 | `	int nLen;` |
|      105 | 1629 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 1630 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 1631 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 1632 | `		return PH7_OK;` |
|        - | 1633 | `	}` |
|        - | 1634 | `	/* Extract the given URI. An empty string is NOT a failure: php parses it as` |
|        - | 1635 | `	 * an empty path. */` |
|      105 | 1636 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|      105 | 1637 | `	if( nLen < 0 ){` |
|      ! 0 | 1638 | `		nLen = 0;` |
|      ! 0 | 1639 | `	}` |
|      105 | 1640 | `	if( !VmUrlSplit(zStr,nLen,&sUrl) ){` |
|        - | 1641 | `		/* Malformed input,return FALSE */` |
|       13 | 1642 | `		ph7_result_bool(pCtx,0);` |
|       13 | 1643 | `		return PH7_OK;` |
|        - | 1644 | `	}` |
|      103 | 1645 | `	if( nArg > 1 && ph7_value_to_int(apArg[1]) >= 0 ){` |
|        - | 1646 | `		/* Refer to constant.c for constants values (php's PHP_URL_* are 0-based;` |
|        - | 1647 | `		 * PHL used to number them from 1, so every literal component id selected` |
|        - | 1648 | `		 * the WRONG field -- and the constants' own tests only asserted "%d",` |
|        - | 1649 | `		 * which any numbering satisfies). A negative id means "the whole array",` |
|        - | 1650 | `		 * which is what the default $component = -1 relies on. */` |
|       27 | 1651 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|       27 | 1652 | `		pComp = 0;` |
|       27 | 1653 | `		bHave = 0;` |
|       27 | 1654 | `		switch(nComponent){` |
|        3 | 1655 | `		case 0: /* PHP_URL_SCHEME */   pComp = &sUrl.sScheme;   bHave = sUrl.bScheme;   break;` |
|        5 | 1656 | `		case 1: /* PHP_URL_HOST */     pComp = &sUrl.sHost;     bHave = sUrl.bHost;     break;` |
|        2 | 1657 | `		case 2: /* PHP_URL_PORT */` |
|        5 | 1658 | `			if( sUrl.bPort ){` |
|        5 | 1659 | `				ph7_result_int(pCtx,sUrl.iPort);` |
|        3 | 1660 | `			}else{` |
|      ! 0 | 1661 | `				ph7_result_null(pCtx);` |
|        - | 1662 | `			}` |
|        5 | 1663 | `			return PH7_OK;` |
|        3 | 1664 | `		case 3: /* PHP_URL_USER */     pComp = &sUrl.sUser;     bHave = sUrl.bUser;     break;` |
|        3 | 1665 | `		case 4: /* PHP_URL_PASS */     pComp = &sUrl.sPass;     bHave = sUrl.bPass;     break;` |
|        3 | 1666 | `		case 5: /* PHP_URL_PATH */     pComp = &sUrl.sPath;     bHave = sUrl.bPath;     break;` |
|        5 | 1667 | `		case 6: /* PHP_URL_QUERY */    pComp = &sUrl.sQuery;    bHave = sUrl.bQuery;    break;` |
|        5 | 1668 | `		case 7: /* PHP_URL_FRAGMENT */ pComp = &sUrl.sFragment; bHave = sUrl.bFragment; break;` |
|        1 | 1669 | `		default:` |
|        4 | 1670 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|        - | 1671 | `				"parse_url(): Argument #2 ($component) must be a valid URL component identifier, %d given",` |
|        1 | 1672 | `				nComponent);` |
|        - | 1673 | `		}` |
|       21 | 1674 | `		if( bHave ){` |
|       19 | 1675 | `			ph7_value *pOut = ph7_context_new_scalar(pCtx);` |
|       19 | 1676 | `			if( pOut == 0 ){` |
|      ! 0 | 1677 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|      ! 0 | 1678 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 1679 | `				return PH7_OK;` |
|        - | 1680 | `			}` |
|       19 | 1681 | `			VmUrlSetComponent(pOut,pComp);` |
|       19 | 1682 | `			ph7_result_value(pCtx,pOut);` |
|       10 | 1683 | `		}else{` |
|        - | 1684 | `			/* No available value,return NULL */` |
|        3 | 1685 | `			ph7_result_null(pCtx);` |
|        - | 1686 | `		}` |
|       11 | 1687 | `	}else{` |
|        - | 1688 | `		ph7_value *pArray,*pValue;` |
|        - | 1689 | `		/* Return an associative array */` |
|       67 | 1690 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       67 | 1691 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       67 | 1692 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 1693 | `			/* Out of memory */` |
|      ! 0 | 1694 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 1695 | `			/* Return false */` |
|      ! 0 | 1696 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 1697 | `			return PH7_OK;` |
|        - | 1698 | `		}` |
|        - | 1699 | `		/* Fill the array, in php's key order. A component is emitted whenever it` |
|        - | 1700 | `		 * is PRESENT, even when empty -- parse_url("?") is ['query'=>'']. */` |
|       67 | 1701 | `		if( sUrl.bScheme ){` |
|       33 | 1702 | `			VmUrlSetComponent(pValue,&sUrl.sScheme);` |
|       33 | 1703 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|       33 | 1704 | `			ph7_value_reset_string_cursor(pValue);` |
|       16 | 1705 | `		}` |
|       67 | 1706 | `		if( sUrl.bHost ){` |
|       31 | 1707 | `			VmUrlSetComponent(pValue,&sUrl.sHost);` |
|       31 | 1708 | `			ph7_array_add_strkey_elem(pArray,"host",pValue);` |
|       31 | 1709 | `			ph7_value_reset_string_cursor(pValue);` |
|       15 | 1710 | `		}` |
|       67 | 1711 | `		if( sUrl.bPort ){` |
|       17 | 1712 | `			ph7_value_int(pValue,sUrl.iPort);` |
|       17 | 1713 | `			ph7_array_add_strkey_elem(pArray,"port",pValue);` |
|       17 | 1714 | `			ph7_value_reset_string_cursor(pValue);` |
|        8 | 1715 | `		}` |
|       67 | 1716 | `		if( sUrl.bUser ){` |
|        9 | 1717 | `			VmUrlSetComponent(pValue,&sUrl.sUser);` |
|        9 | 1718 | `			ph7_array_add_strkey_elem(pArray,"user",pValue);` |
|        9 | 1719 | `			ph7_value_reset_string_cursor(pValue);` |
|        4 | 1720 | `		}` |
|       67 | 1721 | `		if( sUrl.bPass ){` |
|        7 | 1722 | `			VmUrlSetComponent(pValue,&sUrl.sPass);` |
|        7 | 1723 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue);` |
|        7 | 1724 | `			ph7_value_reset_string_cursor(pValue);` |
|        3 | 1725 | `		}` |
|       67 | 1726 | `		if( sUrl.bPath ){` |
|       47 | 1727 | `			VmUrlSetComponent(pValue,&sUrl.sPath);` |
|       47 | 1728 | `			ph7_array_add_strkey_elem(pArray,"path",pValue);` |
|       47 | 1729 | `			ph7_value_reset_string_cursor(pValue);` |
|       23 | 1730 | `		}` |
|       67 | 1731 | `		if( sUrl.bQuery ){` |
|       13 | 1732 | `			VmUrlSetComponent(pValue,&sUrl.sQuery);` |
|       13 | 1733 | `			ph7_array_add_strkey_elem(pArray,"query",pValue);` |
|       13 | 1734 | `			ph7_value_reset_string_cursor(pValue);` |
|        6 | 1735 | `		}` |
|       67 | 1736 | `		if( sUrl.bFragment ){` |
|       13 | 1737 | `			VmUrlSetComponent(pValue,&sUrl.sFragment);` |
|       13 | 1738 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue);` |
|        6 | 1739 | `		}` |
|        - | 1740 | `		/* Return the created array */` |
|       67 | 1741 | `		ph7_result_value(pCtx,pArray);` |
|        - | 1742 | `		/* NOTE:` |
|        - | 1743 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 1744 | `		 * automatically as soon we return from this function.` |
|        - | 1745 | `		 */` |
|        - | 1746 | `	}` |
|        - | 1747 | `	/* All done */` |
|       87 | 1748 | `	return PH7_OK;` |
|       53 | 1749 | `}` |
|        - | 1750 |  |
|        - | 1751 | `/*` |
|        - | 1752 | ` * Section:` |
|        - | 1753 | ` *   Array related routines.` |
|        - | 1754 | ` * Status:` |
|        - | 1755 | ` *    Stable.` |
|        - | 1756 | ` * Note 2012-5-21 01:04:15:` |
|        - | 1757 | ` *  Array related functions that need access to the underlying` |
|        - | 1758 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 1759 | ` */` |
|        - | 1760 | `/*` |
|        - | 1761 | ` * The [compact()] function store it's state information in an instance` |
|        - | 1762 | ` * of the following structure.` |
|        - | 1763 | ` */` |
|        - | 1764 | `struct compact_data` |
|        - | 1765 | `{` |
|        - | 1766 | `	ph7_value *pArray;  /* Target array */` |
|        - | 1767 | `	int nRecCount;      /* Recursion count */` |
|        - | 1768 | `};` |
|        - | 1769 | `/*` |
|        - | 1770 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 1771 | ` */` |
|      ! 0 | 1772 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 1773 | `{` |
|      ! 0 | 1774 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 1775 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 1776 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 1777 | `	/* Act according to the hashmap value */` |
|      ! 0 | 1778 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 1779 | `		SyString sVar;` |
|      ! 0 | 1780 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 1781 | `		if( sVar.nByte > 0 ){` |
|        - | 1782 | `			/* Query the current frame */` |
|      ! 0 | 1783 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 1784 | `			/* ^` |
|        - | 1785 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 1786 | `			 */` |
|      ! 0 | 1787 | `			if( pKey ){` |
|        - | 1788 | `				/* Perform the insertion */` |
|      ! 0 | 1789 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 1790 | `			}` |
|      ! 0 | 1791 | `		}` |
|      ! 0 | 1792 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 1793 | `		int rc;` |
|        - | 1794 | `		/* Recursively traverse this array */` |
|      ! 0 | 1795 | `		pData->nRecCount++;` |
|      ! 0 | 1796 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 1797 | `		pData->nRecCount--;` |
|      ! 0 | 1798 | `		return rc;` |
|        - | 1799 | `	}` |
|      ! 0 | 1800 | `	return SXRET_OK;` |
|      ! 0 | 1801 | `}` |
|        - | 1802 | `/*` |
|        - | 1803 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 1804 | ` *  Create array containing variables and their values.` |
|        - | 1805 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 1806 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 1807 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 1808 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 1809 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 1810 | ` * Parameters` |
|        - | 1811 | ` *  $varname` |
|        - | 1812 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 1813 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 1814 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 1815 | ` *   it recursively.` |
|        - | 1816 | ` * Return` |
|        - | 1817 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 1818 | ` */` |
|        2 | 1819 | `PH7_PRIVATE int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1820 | `{` |
|        - | 1821 | `	ph7_value *pArray,*pObj;` |
|        3 | 1822 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 1823 | `	const char *zName;` |
|        - | 1824 | `	SyString sVar;` |
|        - | 1825 | `	int i,nLen;` |
|        3 | 1826 | `	if( nArg < 1 ){` |
|        - | 1827 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 1828 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1829 | `		return PH7_OK;` |
|        - | 1830 | `	}` |
|        - | 1831 | `	/* Create the array */` |
|        3 | 1832 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 1833 | `	if( pArray == 0 ){` |
|        - | 1834 | `		/* Out of memory */` |
|      ! 0 | 1835 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 1836 | `		/* Return NULL */` |
|      ! 0 | 1837 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1838 | `		return PH7_OK;` |
|        - | 1839 | `	}` |
|        - | 1840 | `	/* Perform the requested operation */` |
|        7 | 1841 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 1842 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 1843 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 1844 | `				struct compact_data sData;` |
|      ! 0 | 1845 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 1846 | `				/* Recursively walk the array */` |
|      ! 0 | 1847 | `				sData.nRecCount = 0;` |
|      ! 0 | 1848 | `				sData.pArray = pArray;` |
|      ! 0 | 1849 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 1850 | `			}` |
|      ! 0 | 1851 | `		}else{` |
|        - | 1852 | `			/* Extract variable name */` |
|        5 | 1853 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 1854 | `			if( nLen > 0 ){` |
|        5 | 1855 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 1856 | `				/* Check if the variable is available in the current frame */` |
|        5 | 1857 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 1858 | `				if( pObj ){` |
|        5 | 1859 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 1860 | `				}` |
|        2 | 1861 | `			}` |
|        - | 1862 | `		}` |
|        3 | 1863 | `	}` |
|        - | 1864 | `	/* Return the array */` |
|        3 | 1865 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 1866 | `	return PH7_OK;` |
|        2 | 1867 | `}` |
|        - | 1868 | `/*` |
|        - | 1869 | ` * The [import_request_variables()] function store it's state information` |
|        - | 1870 | ` * in an instance of the following structure.` |
|        - | 1871 | ` */` |
|        - | 1872 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 1873 | `struct extract_aux_data` |
|        - | 1874 | `{` |
|        - | 1875 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 1876 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 1877 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 1878 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 1879 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 1880 | `};` |
|        - | 1881 | `/*` |
|        - | 1882 | ` * php's php_valid_var_name(): a legal PHP variable name matches` |
|        - | 1883 | ` * [A-Za-z_\x80-\xff][A-Za-z0-9_\x80-\xff]* . Byte-wise and locale-free on` |
|        - | 1884 | ` * purpose (php has been locale-independent here since 8.0); the high-byte` |
|        - | 1885 | ` * range is what lets UTF-8 identifiers through. extract() drops every key` |
|        - | 1886 | ` * that does not pass, instead of installing an unreachable variable.` |
|        - | 1887 | ` */` |
|      148 | 1888 | `static int VmIsValidVarName(const char *zName,sxu32 nByte)` |
|        3 | 1889 | `{` |
|        - | 1890 | `	unsigned char c;` |
|        - | 1891 | `	sxu32 i;` |
|      151 | 1892 | `	if( nByte < 1 ){` |
|        7 | 1893 | `		return FALSE;` |
|        - | 1894 | `	}` |
|      145 | 1895 | `	c = (unsigned char)zName[0];` |
|      145 | 1896 | `	if( c != '_' && !(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') && c < 0x80 ){` |
|       11 | 1897 | `		return FALSE;` |
|        - | 1898 | `	}` |
|      367 | 1899 | `	for( i = 1 ; i < nByte ; ++i ){` |
|      250 | 1900 | `		c = (unsigned char)zName[i];` |
|      248 | 1901 | `		if( c != '_' && !(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z')` |
|       77 | 1902 | `		 && !(c >= '0' && c <= '9') && c < 0x80 ){` |
|       17 | 1903 | `			return FALSE;` |
|        - | 1904 | `		}` |
|      118 | 1905 | `	}` |
|      119 | 1906 | `	return TRUE;` |
|       77 | 1907 | `}` |
|        - | 1908 | `/* TRUE when the name is exactly "this": php refuses to re-assign $this. */` |
|      148 | 1909 | `static int VmExtractIsThis(const char *zName,sxu32 nByte)` |
|        2 | 1910 | `{` |
|      150 | 1911 | `	return nByte == sizeof("this")-1 && SyMemcmp(zName,"this",sizeof("this")-1) == 0;` |
|        2 | 1912 | `}` |
|        - | 1913 | `/*` |
|        - | 1914 | ` * TRUE when the name belongs to the superglobal table ($GLOBALS, $_SERVER,` |
|        - | 1915 | ` * $_GET, …). php hands extract() a per-frame symbol table that holds no` |
|        - | 1916 | ` * superglobal, so such a key lands in the LOCAL table and the real superglobal` |
|        - | 1917 | ` * is untouched. In PHL the name resolves to the superglobal SLOT itself` |
|        - | 1918 | ` * (VmExtractMemObj consults hSuper first), so a plain store would replace` |
|        - | 1919 | ` * $GLOBALS/$_SERVER with the imported value and take the whole symbol table /` |
|        - | 1920 | ` * request environment with it. Those keys are dropped instead — a prefixed` |
|        - | 1921 | ` * name ($p__SERVER) is a normal local and stores fine.` |
|        - | 1922 | ` */` |
|      100 | 1923 | `static int VmExtractIsProtected(ph7_vm *pVm,const char *zName,sxu32 nByte)` |
|        2 | 1924 | `{` |
|      102 | 1925 | `	return SyHashGet(&pVm->hSuper,(const void *)zName,nByte) != 0;` |
|        2 | 1926 | `}` |
|        - | 1927 | `/*` |
|        - | 1928 | ` * TRUE when the calling frame already holds this variable name.` |
|        - | 1929 | ` * "this" and the superglobals always answer FALSE, matching the symbol table` |
|        - | 1930 | ` * php hands extract(): $this is bound implicitly and superglobals live outside` |
|        - | 1931 | ` * the frame. So EXTR_IF_EXISTS/EXTR_PREFIX_IF_EXISTS skip those keys (php-exact)` |
|        - | 1932 | ` * and EXTR_PREFIX_SAME takes its not-a-collision branch.` |
|        - | 1933 | ` */` |
|       66 | 1934 | `static int VmExtractVarExists(ph7_vm *pVm,const char *zName,sxu32 nByte)` |
|        2 | 1935 | `{` |
|        - | 1936 | `	SyString sVar;` |
|       68 | 1937 | `	if( VmExtractIsThis(zName,nByte) \|\| VmExtractIsProtected(pVm,zName,nByte) ){` |
|       20 | 1938 | `		return FALSE;` |
|        - | 1939 | `	}` |
|       50 | 1940 | `	SyStringInitFromBuf(&sVar,zName,nByte);` |
|       50 | 1941 | `	return VmExtractMemObj(pVm,&sVar,FALSE,FALSE) != 0;` |
|       35 | 1942 | `}` |
|        - | 1943 | `/*` |
|        - | 1944 | ` * Create-or-overwrite a variable of the calling frame with a copy of pValue.` |
|        - | 1945 | ` * Returns TRUE when the variable was written (php counts exactly those).` |
|        - | 1946 | ` */` |
|       60 | 1947 | `static int VmExtractStoreVar(ph7_vm *pVm,const char *zName,sxu32 nByte,ph7_value *pValue)` |
|        2 | 1948 | `{` |
|        - | 1949 | `	ph7_value *pObj;` |
|        - | 1950 | `	SyString sVar;` |
|       62 | 1951 | `	SyStringInitFromBuf(&sVar,zName,nByte);` |
|        - | 1952 | `	/* bDup: the name lives in a scratch blob that is reused by the next entry */` |
|       62 | 1953 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|       62 | 1954 | `	if( pObj == 0 ){` |
|      ! 0 | 1955 | `		return FALSE;` |
|        - | 1956 | `	}` |
|       62 | 1957 | `	PH7_MemObjStore(pValue,pObj);` |
|       62 | 1958 | `	return TRUE;` |
|       32 | 1959 | `}` |
|        - | 1960 | `/*` |
|        - | 1961 | ` * Build php's prefixed name "<prefix>_<key>" (php_prefix_varname with` |
|        - | 1962 | ` * add_underscore): the separator is unconditional, so an empty prefix still` |
|        - | 1963 | ` * yields "_key" exactly like php.` |
|        - | 1964 | ` */` |
|       38 | 1965 | `static sxi32 VmExtractPrefixName(SyBlob *pOut,const char *zPrefix,int nPrefix,` |
|        - | 1966 | `	const char *zKey,sxu32 nKey)` |
|        2 | 1967 | `{` |
|       40 | 1968 | `	SyBlobReset(pOut);` |
|       40 | 1969 | `	if( nPrefix > 0 && SyBlobAppend(pOut,zPrefix,(sxu32)nPrefix) != SXRET_OK ){` |
|      ! 0 | 1970 | `		return SXERR_MEM;` |
|        - | 1971 | `	}` |
|       40 | 1972 | `	if( SyBlobAppend(pOut,"_",sizeof(char)) != SXRET_OK ){` |
|      ! 0 | 1973 | `		return SXERR_MEM;` |
|        - | 1974 | `	}` |
|       40 | 1975 | `	if( nKey > 0 && SyBlobAppend(pOut,zKey,nKey) != SXRET_OK ){` |
|      ! 0 | 1976 | `		return SXERR_MEM;` |
|        - | 1977 | `	}` |
|       40 | 1978 | `	return SXRET_OK;` |
|       21 | 1979 | `}` |
|        - | 1980 | `/* What to do with one array entry, decided by the extract mode. */` |
|        - | 1981 | `#define VM_EXTRACT_DROP     0 /* php skips this key entirely */` |
|        - | 1982 | `#define VM_EXTRACT_PLAIN    1 /* install under the key itself */` |
|        - | 1983 | `#define VM_EXTRACT_PREFIX   2 /* install under "<prefix>_<key>" */` |
|        - | 1984 | `/*` |
|        - | 1985 | ` * int extract(array &$array[,int $flags = EXTR_OVERWRITE[,string $prefix = "" ]])` |
|        - | 1986 | ` *   Import variables into the current symbol table from an array.` |
|        - | 1987 | ` *` |
|        - | 1988 | ` * $flags is php's ENUM (PH7_EXTR_* in ph7int.h), not a bitmask: the mode is` |
|        - | 1989 | ` * (flags & 0xff) and anything above EXTR_IF_EXISTS(6) is a ValueError. The` |
|        - | 1990 | ` * modes, mirroring php's per-mode helpers in ext/standard/array.c:` |
|        - | 1991 | ` *   EXTR_OVERWRITE(0)        collisions overwrite` |
|        - | 1992 | ` *   EXTR_SKIP(1)             collisions keep the existing variable` |
|        - | 1993 | ` *   EXTR_PREFIX_SAME(2)      collisions install "<prefix>_<key>"` |
|        - | 1994 | ` *   EXTR_PREFIX_ALL(3)       every key installs as "<prefix>_<key>"` |
|        - | 1995 | ` *   EXTR_PREFIX_INVALID(4)   only illegal names (and numeric keys) get prefixed` |
|        - | 1996 | ` *   EXTR_PREFIX_IF_EXISTS(5) install "<prefix>_<key>" only if <key> exists` |
|        - | 1997 | ` *   EXTR_IF_EXISTS(6)        overwrite only variables that already exist` |
|        - | 1998 | `` * Modes 2..5 require $prefix (php: `is required when using this extract type`),`` |
|        - | 1999 | ` * a non-empty $prefix must itself be a legal identifier, and a key whose final` |
|        - | 2000 | ` * name is not a legal variable name is dropped rather than installed. Only` |
|        - | 2001 | ` * EXTR_PREFIX_ALL/EXTR_PREFIX_INVALID look at numeric keys at all.` |
|        - | 2002 | ` *` |
|        - | 2003 | `` * $this is never a target: php throws `Cannot re-assign $this` where a store`` |
|        - | 2004 | ` * would land on it, and skips it where a store would not (EXTR_SKIP), and` |
|        - | 2005 | ` * $GLOBALS is never clobbered.` |
|        - | 2006 | ` * Return` |
|        - | 2007 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 2008 | ` */` |
|       98 | 2009 | `PH7_PRIVATE int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 | 2010 | `{` |
|      101 | 2011 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 2012 | `	ph7_hashmap_node *pEntry;` |
|        - | 2013 | `	ph7_hashmap *pMap;` |
|      101 | 2014 | `	const char *zPrefix = 0;` |
|      101 | 2015 | `	sxi64 iFlags = PH7_EXTR_OVERWRITE;` |
|      101 | 2016 | `	sxi64 iCount = 0;` |
|        - | 2017 | `	ph7_value sValue;` |
|        - | 2018 | `	SyBlob sWorker;` |
|      101 | 2019 | `	int nPrefix = 0;` |
|      101 | 2020 | `	sxi32 rc = PH7_OK;` |
|        - | 2021 | `	int iType;` |
|        - | 2022 | `	sxu32 n;` |
|      101 | 2023 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|        - | 2024 | `		char zBuf[64];` |
|      ! 0 | 2025 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|        - | 2026 | `			"extract(): Argument #1 ($array) must be of type array, %s given",` |
|      ! 0 | 2027 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|        - | 2028 | `	}` |
|      101 | 2029 | `	if( nArg > 1 ){` |
|       95 | 2030 | `		rc = PH7_IntArgResolve(pCtx,apArg[1],"extract",2,"$flags","int",&iFlags);` |
|       95 | 2031 | `		if( rc != PH7_OK ){` |
|        3 | 2032 | `			return rc;` |
|        - | 2033 | `		}` |
|       45 | 2034 | `	}` |
|        - | 2035 | `	/* php: the mode is the low byte; EXTR_REFS(0x100) rides above it */` |
|       99 | 2036 | `	iType = (int)(iFlags & 0xff);` |
|       99 | 2037 | `	if( iType > PH7_EXTR_IF_EXISTS ){` |
|        7 | 2038 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|        - | 2039 | `			"extract(): Argument #2 ($flags) must be a valid extract type");` |
|        - | 2040 | `	}` |
|       93 | 2041 | `	if( iType > PH7_EXTR_SKIP && iType <= PH7_EXTR_PREFIX_IF_EXISTS && nArg < 3 ){` |
|       12 | 2042 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|        - | 2043 | `			"extract(): Argument #3 ($prefix) is required when using this extract type");` |
|        - | 2044 | `	}` |
|       83 | 2045 | `	if( nArg > 2 ){` |
|       45 | 2046 | `		zPrefix = ph7_value_to_string(apArg[2],&nPrefix);` |
|       45 | 2047 | `		if( nPrefix > 0 && !VmIsValidVarName(zPrefix,(sxu32)nPrefix) ){` |
|        5 | 2048 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|        - | 2049 | `				"extract(): Argument #3 ($prefix) must be a valid identifier");` |
|        - | 2050 | `		}` |
|       19 | 2051 | `	}` |
|       79 | 2052 | `	if( iFlags & PH7_EXTR_REFS ){` |
|        - | 2053 | `		/* php binds each extracted name BY REFERENCE to its array slot. PHL has` |
|        - | 2054 | `		 * no by-ref extraction; importing by VALUE instead would be a divergence` |
|        - | 2055 | `		 * the caller cannot see (writes stop propagating), so it is loud (§10).` |
|        - | 2056 | `		 * The EXTR_REFS constant itself stays undefined. */` |
|        7 | 2057 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|        - | 2058 | `			"extract(): Argument #2 ($flags) EXTR_REFS is not supported");` |
|        - | 2059 | `	}` |
|        - | 2060 | `	/* Point to the target hashmap */` |
|       72 | 2061 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       72 | 2062 | `	if( pMap->nEntry < 1 ){` |
|        - | 2063 | `		/* Empty map,return  0 */` |
|      ! 0 | 2064 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 2065 | `		return PH7_OK;` |
|        - | 2066 | `	}` |
|       72 | 2067 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|       72 | 2068 | `	PH7_MemObjInit(pVm,&sValue);` |
|        - | 2069 | `	/* php walks a COPY of the array, so an entry that overwrites the caller's own` |
|        - | 2070 | `	 * array variable ($arr = ['arr'=>1]; extract($arr)) cannot pull the map from` |
|        - | 2071 | `	 * under the walk. Pinning the map for the walk is the same guarantee. */` |
|       72 | 2072 | `	pMap->iRef++;` |
|       72 | 2073 | `	pEntry = pMap->pFirst;` |
|        - | 2074 | `	/* pFirst walks the insertion order through pPrev — PH7 links the entry list` |
|        - | 2075 | `	 * in reverse (same traversal PH7_HashmapWalk uses). */` |
|      222 | 2076 | `	for( n = pMap->nEntry ; n > 0 && pEntry ; --n, pEntry = pEntry->pPrev ){` |
|        - | 2077 | `		const char *zKey, *zFinal;` |
|        - | 2078 | `		sxu32 nKey, nFinal;` |
|        - | 2079 | `		char zNum[32];` |
|        - | 2080 | `		int bIntKey, iAction;` |
|        - | 2081 | `		/* Work off a COPY of the entry value: installing a variable can grow` |
|        - | 2082 | `		 * pVm->aMemObj, and a pointer into that set would dangle across the` |
|        - | 2083 | `		 * reallocation (this is why the walk API hands out copies too). The` |
|        - | 2084 | `		 * release comes FIRST so it covers every continue/goto below: the load` |
|        - | 2085 | `		 * takes a reference on an array/object value and does not drop the one` |
|        - | 2086 | `		 * the previous entry left behind (PH7_HashmapWalk releases per iteration` |
|        - | 2087 | `		 * for the same reason — without it a whole-array extract() pins every` |
|        - | 2088 | `		 * value it copied, and their destructors never run). */` |
|      154 | 2089 | `		PH7_MemObjRelease(&sValue);` |
|      154 | 2090 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|      154 | 2091 | `		bIntKey = (pEntry->iType == HASHMAP_INT_NODE);` |
|      154 | 2092 | `		if( bIntKey ){` |
|        - | 2093 | `			/* Only the two prefixing modes below ever look at a numeric key */` |
|       19 | 2094 | `			nKey = SyBufferFormat(zNum,sizeof(zNum),"%qd",pEntry->xKey.iKey);` |
|       19 | 2095 | `			zKey = zNum;` |
|       10 | 2096 | `		}else{` |
|      136 | 2097 | `			zKey = (const char *)SyBlobData(&pEntry->xKey.sKey);` |
|      136 | 2098 | `			nKey = SyBlobLength(&pEntry->xKey.sKey);` |
|        - | 2099 | `		}` |
|      154 | 2100 | `		iAction = VM_EXTRACT_DROP;` |
|      154 | 2101 | `		switch( iType ){` |
|       14 | 2102 | `		case PH7_EXTR_OVERWRITE:` |
|       30 | 2103 | `			if( bIntKey \|\| !VmIsValidVarName(zKey,nKey) ){` |
|        5 | 2104 | `				break;` |
|        - | 2105 | `			}` |
|       22 | 2106 | `			if( VmExtractIsThis(zKey,nKey) ){` |
|        3 | 2107 | `				goto this_error;` |
|        - | 2108 | `			}` |
|       20 | 2109 | `			iAction = VM_EXTRACT_PLAIN;` |
|       20 | 2110 | `			break;` |
|       11 | 2111 | `		case PH7_EXTR_SKIP:` |
|       24 | 2112 | `			if( bIntKey \|\| !VmIsValidVarName(zKey,nKey) \|\| VmExtractIsThis(zKey,nKey) ){` |
|        6 | 2113 | `				break;` |
|        - | 2114 | `			}` |
|       14 | 2115 | `			if( VmExtractVarExists(pVm,zKey,nKey) ){` |
|        5 | 2116 | `				break; /* collision: keep the existing variable */` |
|        - | 2117 | `			}` |
|       10 | 2118 | `			iAction = VM_EXTRACT_PLAIN;` |
|       10 | 2119 | `			break;` |
|       15 | 2120 | `		case PH7_EXTR_IF_EXISTS:` |
|       32 | 2121 | `			if( bIntKey \|\| !VmExtractVarExists(pVm,zKey,nKey) ){` |
|       15 | 2122 | `				break;` |
|        - | 2123 | `			}` |
|        5 | 2124 | `			if( !VmIsValidVarName(zKey,nKey) ){` |
|      ! 0 | 2125 | `				break;` |
|        - | 2126 | `			}` |
|        5 | 2127 | `			if( VmExtractIsThis(zKey,nKey) ){` |
|      ! 0 | 2128 | `				goto this_error;` |
|        - | 2129 | `			}` |
|        5 | 2130 | `			iAction = VM_EXTRACT_PLAIN;` |
|        5 | 2131 | `			break;` |
|        8 | 2132 | `		case PH7_EXTR_PREFIX_SAME:` |
|       18 | 2133 | `			if( bIntKey \|\| nKey < 1 ){` |
|        3 | 2134 | `				break;` |
|        - | 2135 | `			}` |
|       14 | 2136 | `			if( VmExtractVarExists(pVm,zKey,nKey) ){` |
|        3 | 2137 | `				iAction = VM_EXTRACT_PREFIX; /* collision */` |
|       13 | 2138 | `			}else if( !VmIsValidVarName(zKey,nKey) ){` |
|        5 | 2139 | `				break;` |
|      ! 0 | 2140 | `			}else{` |
|        - | 2141 | `				/* $this cannot be a target, but its prefixed form can */` |
|        8 | 2142 | `				iAction = VmExtractIsThis(zKey,nKey) ? VM_EXTRACT_PREFIX : VM_EXTRACT_PLAIN;` |
|        - | 2143 | `			}` |
|       10 | 2144 | `			break;` |
|       13 | 2145 | `		case PH7_EXTR_PREFIX_ALL:` |
|       28 | 2146 | `			if( !bIntKey && nKey < 1 ){` |
|        3 | 2147 | `				break;` |
|        - | 2148 | `			}` |
|       26 | 2149 | `			iAction = VM_EXTRACT_PREFIX;` |
|       26 | 2150 | `			break;` |
|        7 | 2151 | `		case PH7_EXTR_PREFIX_INVALID:` |
|       15 | 2152 | `			iAction = (bIntKey \|\| !VmIsValidVarName(zKey,nKey) \|\| VmExtractIsThis(zKey,nKey))` |
|       13 | 2153 | `				? VM_EXTRACT_PREFIX : VM_EXTRACT_PLAIN;` |
|       16 | 2154 | `			break;` |
|        8 | 2155 | `		case PH7_EXTR_PREFIX_IF_EXISTS:` |
|       18 | 2156 | `			if( !bIntKey && VmExtractVarExists(pVm,zKey,nKey) ){` |
|        3 | 2157 | `				iAction = VM_EXTRACT_PREFIX;` |
|        1 | 2158 | `			}` |
|       16 | 2159 | `			break;` |
|      ! 0 | 2160 | `		default:` |
|      ! 0 | 2161 | `			break;` |
|        - | 2162 | `		}` |
|      152 | 2163 | `		if( iAction == VM_EXTRACT_DROP ){` |
|      113 | 2164 | `			continue;` |
|        - | 2165 | `		}` |
|       80 | 2166 | `		if( iAction == VM_EXTRACT_PREFIX ){` |
|       40 | 2167 | `			if( VmExtractPrefixName(&sWorker,zPrefix,nPrefix,zKey,nKey) != SXRET_OK ){` |
|      ! 0 | 2168 | `				rc = PH7_VmThrowException(pCtx,"Error","PH7 engine is running out of memory");` |
|      ! 0 | 2169 | `				goto done;` |
|        - | 2170 | `			}` |
|       40 | 2171 | `			zFinal = (const char *)SyBlobData(&sWorker);` |
|       40 | 2172 | `			nFinal = SyBlobLength(&sWorker);` |
|       40 | 2173 | `			if( !VmIsValidVarName(zFinal,nFinal) ){` |
|        7 | 2174 | `				continue; /* php drops a prefixed name that is not an identifier */` |
|        - | 2175 | `			}` |
|       34 | 2176 | `			if( VmExtractIsThis(zFinal,nFinal) ){` |
|      ! 0 | 2177 | `				goto this_error;` |
|        - | 2178 | `			}` |
|       18 | 2179 | `		}else{` |
|       42 | 2180 | `			if( VmExtractIsProtected(pVm,zKey,nKey) ){` |
|       13 | 2181 | `				continue; /* $GLOBALS/$_SERVER/... are never a plain target */` |
|        - | 2182 | `			}` |
|       30 | 2183 | `			zFinal = zKey;` |
|       30 | 2184 | `			nFinal = nKey;` |
|        - | 2185 | `		}` |
|       62 | 2186 | `		if( VmExtractStoreVar(pVm,zFinal,nFinal,&sValue) ){` |
|       62 | 2187 | `			iCount++;` |
|       30 | 2188 | `		}` |
|       62 | 2189 | `		continue;` |
|        1 | 2190 | `this_error:` |
|        3 | 2191 | `		rc = PH7_VmThrowException(pCtx,"Error","Cannot re-assign $this");` |
|        3 | 2192 | `		goto done;` |
|      ! 0 | 2193 | `	}` |
|        - | 2194 | `	/* Number of variables successfully imported */` |
|       70 | 2195 | `	ph7_result_int64(pCtx,iCount);` |
|       35 | 2196 | `done:` |
|       72 | 2197 | `	PH7_MemObjRelease(&sValue);` |
|       72 | 2198 | `	SyBlobRelease(&sWorker);` |
|       72 | 2199 | `	PH7_HashmapUnref(pMap);` |
|       72 | 2200 | `	return rc;` |
|       52 | 2201 | `}` |
|        - | 2202 | `/*` |
|        - | 2203 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 2204 | ` * defined below.` |
|        - | 2205 | ` */` |
|        2 | 2206 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 2207 | `{` |
|        3 | 2208 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 2209 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 2210 | `	ph7_value *pObj;` |
|        - | 2211 | `	SyString sVar;` |
|        - | 2212 | `	/* Perform a string cast */` |
|        3 | 2213 | `	PH7_MemObjToString(pKey);` |
|        3 | 2214 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 2215 | `		/* Unavailable variable name */` |
|      ! 0 | 2216 | `		return SXRET_OK;` |
|        - | 2217 | `	}` |
|        3 | 2218 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 2219 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 2220 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 2221 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 2222 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 2223 | `			);` |
|        2 | 2224 | `	}else{` |
|      ! 0 | 2225 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 2226 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 2227 | `	}` |
|        3 | 2228 | `	sVar.zString = pAux->zWorker;` |
|        - | 2229 | `	/* Extract the variable */` |
|        3 | 2230 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 2231 | `	if( pObj ){` |
|        3 | 2232 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 2233 | `	}` |
|        3 | 2234 | `	return SXRET_OK;` |
|        2 | 2235 | `}` |
|        - | 2236 | `/*` |
|        - | 2237 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 2238 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 2239 | ` * Parameters` |
|        - | 2240 | ` * $types` |
|        - | 2241 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 2242 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 2243 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 2244 | ` *  POST includes the POST uploaded file information.` |
|        - | 2245 | ` *  Note:` |
|        - | 2246 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 2247 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 2248 | ` * $prefix` |
|        - | 2249 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 2250 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 2251 | ` *  variable named $pref_userid.` |
|        - | 2252 | ` * Return` |
|        - | 2253 | ` *  TRUE on success or FALSE on failure.` |
|        - | 2254 | ` */` |
|        2 | 2255 | `PH7_PRIVATE int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 2256 | `{` |
|        - | 2257 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 2258 | `	extract_aux_data sAux;` |
|        - | 2259 | `	int nLen,nPrefixLen;` |
|        - | 2260 | `	ph7_value *pSuper;` |
|        - | 2261 | `	ph7_vm *pVm;` |
|        - | 2262 | `	/* By default import only $_GET variables  */` |
|        3 | 2263 | `	zImport = "G";` |
|        3 | 2264 | `	nLen = (int)sizeof(char);` |
|        3 | 2265 | `	zPrefix = 0;` |
|        3 | 2266 | `	nPrefixLen = 0;` |
|        3 | 2267 | `	if( nArg > 0 ){` |
|        3 | 2268 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 2269 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 2270 | `		}` |
|        3 | 2271 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 2272 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 2273 | `		}` |
|        1 | 2274 | `	}` |
|        - | 2275 | `	/* Point to the underlying VM */` |
|        3 | 2276 | `	pVm = pCtx->pVm;` |
|        - | 2277 | `	/* Initialize the aux data */` |
|        3 | 2278 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 2279 | `	sAux.zPrefix = zPrefix;` |
|        3 | 2280 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 2281 | `	sAux.pVm = pVm;` |
|        - | 2282 | `	/* Extract */` |
|        3 | 2283 | `	zEnd = &zImport[nLen];` |
|        5 | 2284 | `	while( zImport < zEnd ){` |
|        3 | 2285 | `		int c = zImport[0];` |
|        3 | 2286 | `		pSuper = 0;` |
|        3 | 2287 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 2288 | `			/* Import $_GET variables */` |
|        3 | 2289 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 2290 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 2291 | `			/* Import $_POST variables */` |
|      ! 0 | 2292 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 2293 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 2294 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 2295 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 2296 | `		}` |
|        3 | 2297 | `		if( pSuper ){` |
|        - | 2298 | `			/* Iterate throw array entries */` |
|        3 | 2299 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 2300 | `		}` |
|        - | 2301 | `		/* Advance the cursor */` |
|        3 | 2302 | `		zImport++;` |
|        1 | 2303 | `	}` |
|        - | 2304 | `	/* All done,return TRUE*/` |
|        3 | 2305 | `	ph7_result_bool(pCtx,0);` |
|        3 | 2306 | `	return PH7_OK;` |
|        1 | 2307 | `}` |
|        - | 2308 |  |
