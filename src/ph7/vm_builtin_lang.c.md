# src/ph7/vm_builtin_lang.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 551/808 lines (68.19%)

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
|       36 |   40 | `		res = 1;` |
|       16 |   41 | `	}` |
|       38 |   42 | `	ph7_result_bool(pCtx,res);` |
|       38 |   43 | `	return SXRET_OK;` |
|       21 |   44 | `}` |
|        - |   45 | `/*` |
|        - |   46 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |   47 | ` * below.` |
|        - |   48 | ` */` |
|       26 |   49 | `PH7_PRIVATE void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        3 |   50 | `{` |
|       29 |   51 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |   52 | `	/* Expand constant value */` |
|       29 |   53 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       29 |   54 | `}` |
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
|       20 |   66 | `PH7_PRIVATE int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 |   67 | `{` |
|        - |   68 | `	const char *zName;  /* Constant name */` |
|        - |   69 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       23 |   70 | `	int nLen = 0;       /* Name length */` |
|        - |   71 | `	sxi32 rc;` |
|       23 |   72 | `	if( nArg < 2 ){` |
|        - |   73 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |   74 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |   75 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |   76 | `		return SXRET_OK;` |
|        - |   77 | `	}` |
|       23 |   78 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |   79 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |   80 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |   81 | `		return SXRET_OK;` |
|        - |   82 | `	}` |
|        - |   83 | `	/* Extract constant name */` |
|       23 |   84 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       23 |   85 | `	if( nLen < 1 ){` |
|      ! 0 |   86 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |   87 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |   88 | `		return SXRET_OK;` |
|        - |   89 | `	}` |
|        - |   90 | `	/* Duplicate constant value */` |
|       23 |   91 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       23 |   92 | `	if( pValue == 0 ){` |
|      ! 0 |   93 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |   94 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |   95 | `		return SXRET_OK;` |
|        - |   96 | `	}` |
|        - |   97 | `	/* Initialize the memory object */` |
|       23 |   98 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |   99 | `	/* Register the constant */` |
|        - |  100 | `	{` |
|        - |  101 | `		SyString sConsName;` |
|       23 |  102 | `		SyStringInitFromBuf(&sConsName,zName,(sxu32)nLen);` |
|       33 |  103 | `		rc = PH7_VmRegisterConstantEx(pCtx->pVm,&sConsName,VmExpandUserConstant,pValue,` |
|       20 |  104 | `			(SyString *)SySetPeek(&pCtx->pVm->aFiles),0,1);` |
|        - |  105 | `	}` |
|       23 |  106 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  107 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  108 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  109 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  110 | `		return SXRET_OK;` |
|        - |  111 | `	}` |
|        - |  112 | `	/* Duplicate constant value */` |
|       23 |  113 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       23 |  114 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
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
|       23 |  145 | `	ph7_result_bool(pCtx,1);` |
|       23 |  146 | `	return SXRET_OK;` |
|       13 |  147 | `}` |
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
|      920 |  367 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  368 | `{` |
|      921 |  369 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  370 | `	ph7_value sName;` |
|        - |  371 | `	sxi32 rc;` |
|        - |  372 | `	/* Prepare the constant name for insertion */` |
|      921 |  373 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      921 |  374 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  375 | `	/* Perform the insertion */` |
|      921 |  376 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      921 |  377 | `	PH7_MemObjRelease(&sName);` |
|      921 |  378 | `	return rc;` |
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
|     5110 |  419 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        5 |  420 | `{` |
|        - |  421 | `	sxu32 iNum;` |
|     5115 |  422 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     5115 |  423 | `	return iNum;` |
|        5 |  424 | `}` |
|        - |  425 | `/*` |
|        - |  426 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  427 | ` * Note that the generated string is NOT null terminated.` |
|        - |  428 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|        - |  429 | ` * implemented in src/sx/sxrand.c).` |
|        - |  430 | ` */` |
|  2893672 |  431 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        5 |  432 | `{` |
|        - |  433 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  434 | `	int i;` |
|        - |  435 | `	/* Generate a binary string first */` |
|  2893677 |  436 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  437 | `	/* Turn the binary string into english based alphabet */` |
| 31830585 |  438 | `	for( i = 0 ; i < nLen ; ++i ){` |
| 28936913 |  439 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
| 14468459 |  440 | `	 }` |
|  2893677 |  441 | `}` |
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
|        2 |  548 | `{` |
|        - |  549 | `	char zString[1024];` |
|      132 |  550 | `	int iLen = 0x10;` |
|      132 |  551 | `	if( nArg > 0 ){` |
|        - |  552 | `		/* Get the desired length */` |
|      132 |  553 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      132 |  554 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  555 | `			/* Default length */` |
|        3 |  556 | `			iLen = 0x10;` |
|        1 |  557 | `		}` |
|       65 |  558 | `	}` |
|        - |  559 | `	/* Generate the random string */` |
|      132 |  560 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  561 | `	/* Return the generated string */` |
|      132 |  562 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      132 |  563 | `	return SXRET_OK;` |
|        2 |  564 | `}` |
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
|      330 |  639 | `	for( nAttempt = 0 ; nAttempt < 50 ; ++nAttempt ){` |
|        - |  640 | `		/* Always draw a full 8 bytes so endianness of the cast doesn't matter` |
|        - |  641 | `		 * (a 4-byte fill into a sxu64 would land in the high half on big-endian` |
|        - |  642 | `		 * and the low-half mask would always read 0). */` |
|        - |  643 | `		sxu64 uDraw;` |
|      330 |  644 | `		if( SyOSCSPRNG(&uDraw,sizeof(uDraw)) != SXRET_OK ){` |
|      ! 0 |  645 | `			return PH7_VmThrowException(pCtx,` |
|        - |  646 | `				"Random\\RandomException",` |
|        - |  647 | `				"Cannot gather sufficient random data"` |
|        - |  648 | `				);` |
|        - |  649 | `		}` |
|      330 |  650 | `		uDraw &= uMask;` |
|      330 |  651 | `		if( uDraw <= uRange ){` |
|      225 |  652 | `			uResult = uDraw;` |
|      225 |  653 | `			break;` |
|        - |  654 | `		}` |
|       52 |  655 | `	}` |
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
|       28 | 1182 | `PH7_PRIVATE int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1183 | `{` |
|        - | 1184 | `	const char *zStr; /* Input string */` |
|        - | 1185 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 1186 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 1187 | `	int nLen;` |
|        - | 1188 | `	sxi32 rc;` |
|       29 | 1189 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 1190 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 1191 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 1192 | `		return PH7_OK;` |
|        - | 1193 | `	}` |
|        - | 1194 | `	/* Extract the given URI */` |
|       29 | 1195 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 1196 | `	if( nLen < 1 ){` |
|        - | 1197 | `		/* Nothing to process,return FALSE */` |
|        3 | 1198 | `		ph7_result_bool(pCtx,0);` |
|        3 | 1199 | `		return PH7_OK;` |
|        - | 1200 | `	}` |
|        - | 1201 | `	/* Get a parse */` |
|       27 | 1202 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 1203 | `	if( rc != SXRET_OK ){` |
|        - | 1204 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 1205 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 1206 | `		return PH7_OK;` |
|        - | 1207 | `	}` |
|       27 | 1208 | `	if( nArg > 1 ){` |
|      ! 0 | 1209 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 1210 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 1211 | `		switch(nComponent){` |
|      ! 0 | 1212 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 1213 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 1214 | `			if( pComp->nByte < 1 ){` |
|        - | 1215 | `				/* No available value,return NULL */` |
|      ! 0 | 1216 | `				ph7_result_null(pCtx);` |
|      ! 0 | 1217 | `			}else{` |
|      ! 0 | 1218 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 1219 | `			}` |
|      ! 0 | 1220 | `			break;` |
|      ! 0 | 1221 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 1222 | `			pComp = &sURI.sHost;` |
|      ! 0 | 1223 | `			if( pComp->nByte < 1 ){` |
|        - | 1224 | `				/* No available value,return NULL */` |
|      ! 0 | 1225 | `				ph7_result_null(pCtx);` |
|      ! 0 | 1226 | `			}else{` |
|      ! 0 | 1227 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 1228 | `			}` |
|      ! 0 | 1229 | `			break;` |
|      ! 0 | 1230 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 1231 | `			pComp = &sURI.sPort;` |
|      ! 0 | 1232 | `			if( pComp->nByte < 1 ){` |
|        - | 1233 | `				/* No available value,return NULL */` |
|      ! 0 | 1234 | `				ph7_result_null(pCtx);` |
|      ! 0 | 1235 | `			}else{` |
|      ! 0 | 1236 | `				int iPort = 0;` |
|        - | 1237 | `				/* Cast the value to integer */` |
|      ! 0 | 1238 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 1239 | `				ph7_result_int(pCtx,iPort);` |
|        - | 1240 | `			}` |
|      ! 0 | 1241 | `			break;` |
|      ! 0 | 1242 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 1243 | `			pComp = &sURI.sUser;` |
|      ! 0 | 1244 | `			if( pComp->nByte < 1 ){` |
|        - | 1245 | `				/* No available value,return NULL */` |
|      ! 0 | 1246 | `				ph7_result_null(pCtx);` |
|      ! 0 | 1247 | `			}else{` |
|      ! 0 | 1248 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 1249 | `			}` |
|      ! 0 | 1250 | `			break;` |
|      ! 0 | 1251 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 1252 | `			pComp = &sURI.sPass;` |
|      ! 0 | 1253 | `			if( pComp->nByte < 1 ){` |
|        - | 1254 | `				/* No available value,return NULL */` |
|      ! 0 | 1255 | `				ph7_result_null(pCtx);` |
|      ! 0 | 1256 | `			}else{` |
|      ! 0 | 1257 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 1258 | `			}` |
|      ! 0 | 1259 | `			break;` |
|      ! 0 | 1260 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 1261 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 1262 | `			if( pComp->nByte < 1 ){` |
|        - | 1263 | `				/* No available value,return NULL */` |
|      ! 0 | 1264 | `				ph7_result_null(pCtx);` |
|      ! 0 | 1265 | `			}else{` |
|      ! 0 | 1266 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 1267 | `			}` |
|      ! 0 | 1268 | `			break;` |
|      ! 0 | 1269 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 1270 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 1271 | `			if( pComp->nByte < 1 ){` |
|        - | 1272 | `				/* No available value,return NULL */` |
|      ! 0 | 1273 | `				ph7_result_null(pCtx);` |
|      ! 0 | 1274 | `			}else{` |
|      ! 0 | 1275 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 1276 | `			}` |
|      ! 0 | 1277 | `			break;` |
|      ! 0 | 1278 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 1279 | `			pComp = &sURI.sPath;` |
|      ! 0 | 1280 | `			if( pComp->nByte < 1 ){` |
|        - | 1281 | `				/* No available value,return NULL */` |
|      ! 0 | 1282 | `				ph7_result_null(pCtx);` |
|      ! 0 | 1283 | `			}else{` |
|      ! 0 | 1284 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 1285 | `			}` |
|      ! 0 | 1286 | `			break;` |
|      ! 0 | 1287 | `		default:` |
|        - | 1288 | `			/* No such entry,return NULL */` |
|      ! 0 | 1289 | `			ph7_result_null(pCtx);` |
|      ! 0 | 1290 | `			break;` |
|        - | 1291 | `		}` |
|      ! 0 | 1292 | `	}else{` |
|        - | 1293 | `		ph7_value *pArray,*pValue;` |
|        - | 1294 | `		/* Return an associative array */` |
|       27 | 1295 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 1296 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 1297 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 1298 | `			/* Out of memory */` |
|      ! 0 | 1299 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 1300 | `			/* Return false */` |
|      ! 0 | 1301 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 1302 | `			return PH7_OK;` |
|        - | 1303 | `		}` |
|        - | 1304 | `		/* Fill the array */` |
|       27 | 1305 | `		pComp = &sURI.sScheme;` |
|       27 | 1306 | `		if( pComp->nByte > 0 ){` |
|       19 | 1307 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 1308 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 1309 | `		}` |
|        - | 1310 | `		/* Reset the string cursor */` |
|       27 | 1311 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 1312 | `		pComp = &sURI.sHost;` |
|       27 | 1313 | `		if( pComp->nByte > 0 ){` |
|       25 | 1314 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 1315 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 1316 | `		}` |
|        - | 1317 | `		/* Reset the string cursor */` |
|       27 | 1318 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 1319 | `		pComp = &sURI.sPort;` |
|       27 | 1320 | `		if( pComp->nByte > 0 ){` |
|       11 | 1321 | `			int iPort = 0;/* cc warning */` |
|        - | 1322 | `			/* Convert to integer */` |
|       11 | 1323 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 1324 | `			ph7_value_int(pValue,iPort);` |
|       11 | 1325 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 1326 | `		}` |
|        - | 1327 | `		/* Reset the string cursor */` |
|       27 | 1328 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 1329 | `		pComp = &sURI.sUser;` |
|       27 | 1330 | `		if( pComp->nByte > 0 ){` |
|        7 | 1331 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 1332 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 1333 | `		}` |
|        - | 1334 | `		/* Reset the string cursor */` |
|       27 | 1335 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 1336 | `		pComp = &sURI.sPass;` |
|       27 | 1337 | `		if( pComp->nByte > 0 ){` |
|        7 | 1338 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 1339 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 1340 | `		}` |
|        - | 1341 | `		/* Reset the string cursor */` |
|       27 | 1342 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 1343 | `		pComp = &sURI.sPath;` |
|       27 | 1344 | `		if( pComp->nByte > 0 ){` |
|       17 | 1345 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 1346 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 1347 | `		}` |
|        - | 1348 | `		/* Reset the string cursor */` |
|       27 | 1349 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 1350 | `		pComp = &sURI.sQuery;` |
|       27 | 1351 | `		if( pComp->nByte > 0 ){` |
|        5 | 1352 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 1353 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 1354 | `		}` |
|        - | 1355 | `		/* Reset the string cursor */` |
|       27 | 1356 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 1357 | `		pComp = &sURI.sFragment;` |
|       27 | 1358 | `		if( pComp->nByte > 0 ){` |
|        5 | 1359 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 1360 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 1361 | `		}` |
|        - | 1362 | `		/* Return the created array */` |
|       27 | 1363 | `		ph7_result_value(pCtx,pArray);` |
|        - | 1364 | `		/* NOTE:` |
|        - | 1365 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 1366 | `		 * automatically as soon we return from this function.` |
|        - | 1367 | `		 */` |
|        - | 1368 | `	}` |
|        - | 1369 | `	/* All done */` |
|       27 | 1370 | `	return PH7_OK;` |
|       15 | 1371 | `}` |
|        - | 1372 | `/*` |
|        - | 1373 | ` * Section:` |
|        - | 1374 | ` *   Array related routines.` |
|        - | 1375 | ` * Status:` |
|        - | 1376 | ` *    Stable.` |
|        - | 1377 | ` * Note 2012-5-21 01:04:15:` |
|        - | 1378 | ` *  Array related functions that need access to the underlying` |
|        - | 1379 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 1380 | ` */` |
|        - | 1381 | `/*` |
|        - | 1382 | ` * The [compact()] function store it's state information in an instance` |
|        - | 1383 | ` * of the following structure.` |
|        - | 1384 | ` */` |
|        - | 1385 | `struct compact_data` |
|        - | 1386 | `{` |
|        - | 1387 | `	ph7_value *pArray;  /* Target array */` |
|        - | 1388 | `	int nRecCount;      /* Recursion count */` |
|        - | 1389 | `};` |
|        - | 1390 | `/*` |
|        - | 1391 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 1392 | ` */` |
|      ! 0 | 1393 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 1394 | `{` |
|      ! 0 | 1395 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 1396 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 1397 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 1398 | `	/* Act according to the hashmap value */` |
|      ! 0 | 1399 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 1400 | `		SyString sVar;` |
|      ! 0 | 1401 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 1402 | `		if( sVar.nByte > 0 ){` |
|        - | 1403 | `			/* Query the current frame */` |
|      ! 0 | 1404 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 1405 | `			/* ^` |
|        - | 1406 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 1407 | `			 */` |
|      ! 0 | 1408 | `			if( pKey ){` |
|        - | 1409 | `				/* Perform the insertion */` |
|      ! 0 | 1410 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 1411 | `			}` |
|      ! 0 | 1412 | `		}` |
|      ! 0 | 1413 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 1414 | `		int rc;` |
|        - | 1415 | `		/* Recursively traverse this array */` |
|      ! 0 | 1416 | `		pData->nRecCount++;` |
|      ! 0 | 1417 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 1418 | `		pData->nRecCount--;` |
|      ! 0 | 1419 | `		return rc;` |
|        - | 1420 | `	}` |
|      ! 0 | 1421 | `	return SXRET_OK;` |
|      ! 0 | 1422 | `}` |
|        - | 1423 | `/*` |
|        - | 1424 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 1425 | ` *  Create array containing variables and their values.` |
|        - | 1426 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 1427 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 1428 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 1429 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 1430 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 1431 | ` * Parameters` |
|        - | 1432 | ` *  $varname` |
|        - | 1433 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 1434 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 1435 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 1436 | ` *   it recursively.` |
|        - | 1437 | ` * Return` |
|        - | 1438 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 1439 | ` */` |
|        2 | 1440 | `PH7_PRIVATE int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1441 | `{` |
|        - | 1442 | `	ph7_value *pArray,*pObj;` |
|        3 | 1443 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 1444 | `	const char *zName;` |
|        - | 1445 | `	SyString sVar;` |
|        - | 1446 | `	int i,nLen;` |
|        3 | 1447 | `	if( nArg < 1 ){` |
|        - | 1448 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 1449 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1450 | `		return PH7_OK;` |
|        - | 1451 | `	}` |
|        - | 1452 | `	/* Create the array */` |
|        3 | 1453 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 1454 | `	if( pArray == 0 ){` |
|        - | 1455 | `		/* Out of memory */` |
|      ! 0 | 1456 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 1457 | `		/* Return NULL */` |
|      ! 0 | 1458 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1459 | `		return PH7_OK;` |
|        - | 1460 | `	}` |
|        - | 1461 | `	/* Perform the requested operation */` |
|        7 | 1462 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 1463 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 1464 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 1465 | `				struct compact_data sData;` |
|      ! 0 | 1466 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 1467 | `				/* Recursively walk the array */` |
|      ! 0 | 1468 | `				sData.nRecCount = 0;` |
|      ! 0 | 1469 | `				sData.pArray = pArray;` |
|      ! 0 | 1470 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 1471 | `			}` |
|      ! 0 | 1472 | `		}else{` |
|        - | 1473 | `			/* Extract variable name */` |
|        5 | 1474 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 1475 | `			if( nLen > 0 ){` |
|        5 | 1476 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 1477 | `				/* Check if the variable is available in the current frame */` |
|        5 | 1478 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 1479 | `				if( pObj ){` |
|        5 | 1480 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 1481 | `				}` |
|        2 | 1482 | `			}` |
|        - | 1483 | `		}` |
|        3 | 1484 | `	}` |
|        - | 1485 | `	/* Return the array */` |
|        3 | 1486 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 1487 | `	return PH7_OK;` |
|        2 | 1488 | `}` |
|        - | 1489 | `/*` |
|        - | 1490 | ` * The [extract()] function store it's state information in an instance` |
|        - | 1491 | ` * of the following structure.` |
|        - | 1492 | ` */` |
|        - | 1493 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 1494 | `struct extract_aux_data` |
|        - | 1495 | `{` |
|        - | 1496 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 1497 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 1498 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 1499 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 1500 | `	int iFlags;           /* Control flags */` |
|        - | 1501 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 1502 | `};` |
|        - | 1503 | `/* Forward declaration */` |
|        - | 1504 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 1505 | `/*` |
|        - | 1506 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 1507 | ` *   Import variables into the current symbol table from an array.` |
|        - | 1508 | ` * Parameters` |
|        - | 1509 | ` * $var_array` |
|        - | 1510 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 1511 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 1512 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 1513 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 1514 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 1515 | ` * $extract_type` |
|        - | 1516 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 1517 | ` *  It can be one of the following values:` |
|        - | 1518 | ` *   EXTR_OVERWRITE` |
|        - | 1519 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 1520 | ` *   EXTR_SKIP` |
|        - | 1521 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 1522 | ` *   EXTR_PREFIX_SAME` |
|        - | 1523 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 1524 | ` *   EXTR_PREFIX_ALL` |
|        - | 1525 | ` *       Prefix all variable names with prefix.` |
|        - | 1526 | ` *   EXTR_PREFIX_INVALID` |
|        - | 1527 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 1528 | ` *   EXTR_IF_EXISTS` |
|        - | 1529 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 1530 | ` *       otherwise do nothing.` |
|        - | 1531 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 1532 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 1533 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 1534 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 1535 | ` *      the current symbol table.` |
|        - | 1536 | ` * $prefix` |
|        - | 1537 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 1538 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 1539 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 1540 | ` *  underscore character.` |
|        - | 1541 | ` * Return` |
|        - | 1542 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 1543 | ` */` |
|        4 | 1544 | `PH7_PRIVATE int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1545 | `{` |
|        - | 1546 | `	extract_aux_data sAux;` |
|        - | 1547 | `	ph7_hashmap *pMap;` |
|        5 | 1548 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 1549 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 1550 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 1551 | `		return PH7_OK;` |
|        - | 1552 | `	}` |
|        - | 1553 | `	/* Point to the target hashmap */` |
|        5 | 1554 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 1555 | `	if( pMap->nEntry < 1 ){` |
|        - | 1556 | `		/* Empty map,return  0 */` |
|      ! 0 | 1557 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 1558 | `		return PH7_OK;` |
|        - | 1559 | `	}` |
|        - | 1560 | `	/* Prepare the aux data */` |
|        5 | 1561 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 1562 | `	if( nArg > 1 ){` |
|        3 | 1563 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 1564 | `		if( nArg > 2 ){` |
|      ! 0 | 1565 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 1566 | `		}` |
|        1 | 1567 | `	}` |
|        5 | 1568 | `	sAux.pVm = pCtx->pVm;` |
|        - | 1569 | `	/* Invoke the worker callback */` |
|        5 | 1570 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 1571 | `	/* Number of variables successfully imported */` |
|        5 | 1572 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 1573 | `	return PH7_OK;` |
|        3 | 1574 | `}` |
|        - | 1575 | `/*` |
|        - | 1576 | ` * Worker callback for the [extract()] function defined` |
|        - | 1577 | ` * below.` |
|        - | 1578 | ` */` |
|        8 | 1579 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 1580 | `{` |
|        9 | 1581 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 1582 | `	int iFlags = pAux->iFlags;` |
|        9 | 1583 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 1584 | `	ph7_value *pObj;` |
|        - | 1585 | `	SyString sVar;` |
|        9 | 1586 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 1587 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 1588 | `	}` |
|        - | 1589 | `	/* Perform a string cast */` |
|        9 | 1590 | `	PH7_MemObjToString(pKey);` |
|        9 | 1591 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 1592 | `		/* Unavailable variable name */` |
|      ! 0 | 1593 | `		return SXRET_OK;` |
|        - | 1594 | `	}` |
|        9 | 1595 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 1596 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 1597 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 1598 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 1599 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 1600 | `			);` |
|      ! 0 | 1601 | `	}else{` |
|       13 | 1602 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 1603 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 1604 | `	}` |
|        9 | 1605 | `	sVar.zString = pAux->zWorker;` |
|        - | 1606 | `	/* Try to extract the variable */` |
|        9 | 1607 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 1608 | `	if( pObj ){` |
|        - | 1609 | `		/* Collision */` |
|        3 | 1610 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 1611 | `			return SXRET_OK;` |
|        - | 1612 | `		}` |
|        3 | 1613 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 1614 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 1615 | `				/* Already prefixed */` |
|      ! 0 | 1616 | `				return SXRET_OK;` |
|        - | 1617 | `			}` |
|      ! 0 | 1618 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 1619 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 1620 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 1621 | `				);` |
|      ! 0 | 1622 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 1623 | `		}` |
|        2 | 1624 | `	}else{` |
|        - | 1625 | `		/* Create the variable */` |
|        7 | 1626 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 1627 | `	}` |
|        9 | 1628 | `	if( pObj ){` |
|        - | 1629 | `		/* Overwrite the old value */` |
|        9 | 1630 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 1631 | `		/* Increment counter */` |
|        9 | 1632 | `		pAux->iCount++;` |
|        4 | 1633 | `	}` |
|        9 | 1634 | `	return SXRET_OK;` |
|        5 | 1635 | `}` |
|        - | 1636 | `/*` |
|        - | 1637 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 1638 | ` * defined below.` |
|        - | 1639 | ` */` |
|        2 | 1640 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 1641 | `{` |
|        3 | 1642 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 1643 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 1644 | `	ph7_value *pObj;` |
|        - | 1645 | `	SyString sVar;` |
|        - | 1646 | `	/* Perform a string cast */` |
|        3 | 1647 | `	PH7_MemObjToString(pKey);` |
|        3 | 1648 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 1649 | `		/* Unavailable variable name */` |
|      ! 0 | 1650 | `		return SXRET_OK;` |
|        - | 1651 | `	}` |
|        3 | 1652 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 1653 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 1654 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 1655 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 1656 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 1657 | `			);` |
|        2 | 1658 | `	}else{` |
|      ! 0 | 1659 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 1660 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 1661 | `	}` |
|        3 | 1662 | `	sVar.zString = pAux->zWorker;` |
|        - | 1663 | `	/* Extract the variable */` |
|        3 | 1664 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 1665 | `	if( pObj ){` |
|        3 | 1666 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 1667 | `	}` |
|        3 | 1668 | `	return SXRET_OK;` |
|        2 | 1669 | `}` |
|        - | 1670 | `/*` |
|        - | 1671 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 1672 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 1673 | ` * Parameters` |
|        - | 1674 | ` * $types` |
|        - | 1675 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 1676 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 1677 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 1678 | ` *  POST includes the POST uploaded file information.` |
|        - | 1679 | ` *  Note:` |
|        - | 1680 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 1681 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 1682 | ` * $prefix` |
|        - | 1683 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 1684 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 1685 | ` *  variable named $pref_userid.` |
|        - | 1686 | ` * Return` |
|        - | 1687 | ` *  TRUE on success or FALSE on failure.` |
|        - | 1688 | ` */` |
|        2 | 1689 | `PH7_PRIVATE int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1690 | `{` |
|        - | 1691 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 1692 | `	extract_aux_data sAux;` |
|        - | 1693 | `	int nLen,nPrefixLen;` |
|        - | 1694 | `	ph7_value *pSuper;` |
|        - | 1695 | `	ph7_vm *pVm;` |
|        - | 1696 | `	/* By default import only $_GET variables  */` |
|        3 | 1697 | `	zImport = "G";` |
|        3 | 1698 | `	nLen = (int)sizeof(char);` |
|        3 | 1699 | `	zPrefix = 0;` |
|        3 | 1700 | `	nPrefixLen = 0;` |
|        3 | 1701 | `	if( nArg > 0 ){` |
|        3 | 1702 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 1703 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 1704 | `		}` |
|        3 | 1705 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 1706 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 1707 | `		}` |
|        1 | 1708 | `	}` |
|        - | 1709 | `	/* Point to the underlying VM */` |
|        3 | 1710 | `	pVm = pCtx->pVm;` |
|        - | 1711 | `	/* Initialize the aux data */` |
|        3 | 1712 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 1713 | `	sAux.zPrefix = zPrefix;` |
|        3 | 1714 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 1715 | `	sAux.pVm = pVm;` |
|        - | 1716 | `	/* Extract */` |
|        3 | 1717 | `	zEnd = &zImport[nLen];` |
|        5 | 1718 | `	while( zImport < zEnd ){` |
|        3 | 1719 | `		int c = zImport[0];` |
|        3 | 1720 | `		pSuper = 0;` |
|        3 | 1721 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 1722 | `			/* Import $_GET variables */` |
|        3 | 1723 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 1724 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 1725 | `			/* Import $_POST variables */` |
|      ! 0 | 1726 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 1727 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 1728 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 1729 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 1730 | `		}` |
|        3 | 1731 | `		if( pSuper ){` |
|        - | 1732 | `			/* Iterate throw array entries */` |
|        3 | 1733 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 1734 | `		}` |
|        - | 1735 | `		/* Advance the cursor */` |
|        3 | 1736 | `		zImport++;` |
|        1 | 1737 | `	}` |
|        - | 1738 | `	/* All done,return TRUE*/` |
|        3 | 1739 | `	ph7_result_bool(pCtx,0);` |
|        3 | 1740 | `	return PH7_OK;` |
|        1 | 1741 | `}` |
|        - | 1742 |  |
