# src/ph7/vm_builtin_lang.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1221/1424 lines (85.74%)

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
|        - |   17 | ` * What a "C::K" constant NAME resolved to. php's defined() and constant() ask the same` |
|        - |   18 | ` * question of the same string and only differ in how they REPORT the answer — defined()` |
|        - |   19 | `` * turns every miss into `false`, constant() into a catchable Error — so the resolution`` |
|        - |   20 | ` * itself lives here once.` |
|        - |   21 | ` */` |
|        - |   22 | `#define VM_CCONST_PLAIN     0 /* no "::" in the name: a global constant, not this form */` |
|        - |   23 | `#define VM_CCONST_OK        1 /* class and constant found, and visible from here */` |
|        - |   24 | `#define VM_CCONST_NOCLASS   2 /* the class part names nothing (autoload already tried) */` |
|        - |   25 | `#define VM_CCONST_NOSCOPE   3 /* self/parent/static named with no class scope active */` |
|        - |   26 | `#define VM_CCONST_NOCONST   4 /* the class exists but declares no such constant */` |
|        - |   27 | `#define VM_CCONST_NOACCESS  5 /* it exists but is private/protected out of scope */` |
|        - |   28 | ``#define VM_CCONST_NOPARENT  6 /* `parent` named from a class that has none */`` |
|        - |   29 | `/*` |
|        - |   30 | ` * Split "C::K" and resolve both halves. The class half goes through` |
|        - |   31 | `` * PH7_VmResolveScopeName, so `self`/`parent`/`static` answer against the live class`` |
|        - |   32 | ` * context and a plain name AUTOLOADS on a miss (php does both here). The constant half` |
|        - |   33 | ` * is looked up without evaluating anything: an unmaterialized enum case or an on-demand` |
|        - |   34 | ` * constant initializer must not run just because someone ASKED whether the name exists.` |
|        - |   35 | ` * The class name is case-insensitive and the constant name is not, exactly as php.` |
|        - |   36 | ` */` |
|     3332 |   37 | `static int VmClassConstLookup(` |
|        - |   38 | `	ph7_vm *pVm,             /* Target VM */` |
|        - |   39 | `	const char *zName,       /* Constant name, possibly of the "C::K" form */` |
|        - |   40 | `	int nLen,                /* zName length */` |
|        - |   41 | `	ph7_class **ppClass,     /* OUT: resolved class (may be 0) */` |
|        - |   42 | `	ph7_class_attr **ppAttr, /* OUT: resolved constant (may be 0) */` |
|        - |   43 | `	int *pSep                /* OUT: offset of the "::" separator */` |
|        - |   44 | `	)` |
|        5 |   45 | `{` |
|        - |   46 | `	ph7_class_attr *pAttr;` |
|        - |   47 | `	ph7_class *pClass;` |
|        - |   48 | `	int iSep;` |
|     3337 |   49 | `	*ppClass = 0;` |
|     3337 |   50 | `	*ppAttr = 0;` |
|    64723 |   51 | `	for( iSep = 0; iSep + 1 < nLen; iSep++ ){` |
|    61571 |   52 | `		if( zName[iSep] == ':' && zName[iSep+1] == ':' ){` |
|      184 |   53 | `			break;` |
|        - |   54 | `		}` |
|    30698 |   55 | `	}` |
|     3337 |   56 | `	if( iSep + 1 >= nLen ){` |
|     3157 |   57 | `		return VM_CCONST_PLAIN;` |
|        - |   58 | `	}` |
|      184 |   59 | `	*pSep = iSep;` |
|      184 |   60 | `	pClass = iSep > 0 ? PH7_VmResolveScopeName(&(*pVm),zName,(sxu32)iSep) : 0;` |
|      184 |   61 | `	if( pClass == 0 ){` |
|       31 |   62 | `		if( iSep > 0 && PH7_VmIsScopeKeyword(zName,(sxu32)iSep) ){` |
|        - |   63 | `			/* php separates the two ways a keyword can fail to resolve, so tell them` |
|        - |   64 | ``			 * apart here: `parent` inside a class that simply has no parent is a`` |
|        - |   65 | `			 * different sentence from a keyword named with no class scope at all. */` |
|       16 |   66 | `			if( iSep == 6 && SyMemcmp(zName,"parent",6) == 0` |
|       10 |   67 | `			 && (PH7_VmPeekTopClass(&(*pVm)) \|\| PH7_VmPeekDeclaringClass(&(*pVm))) ){` |
|        3 |   68 | `				return VM_CCONST_NOPARENT;` |
|        - |   69 | `			}` |
|       16 |   70 | `			return VM_CCONST_NOSCOPE;` |
|        - |   71 | `		}` |
|       14 |   72 | `		return VM_CCONST_NOCLASS;` |
|        - |   73 | `	}` |
|      155 |   74 | `	*ppClass = pClass;` |
|      155 |   75 | `	if( iSep + 2 >= nLen ){` |
|        5 |   76 | `		return VM_CCONST_NOCONST; /* "C::" names no constant */` |
|        - |   77 | `	}` |
|        - |   78 | `	/* This form names a class CONSTANT or an enum case (hConst), never a property. */` |
|      151 |   79 | `	pAttr = PH7_ClassExtractConstant(pClass,&zName[iSep+2],(sxu32)(nLen - iSep - 2));` |
|      151 |   80 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       19 |   81 | `		return VM_CCONST_NOCONST;` |
|        - |   82 | `	}` |
|      132 |   83 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|       82 |   84 | `	 && pAttr->pDeclClass && pAttr->pDeclClass != pClass ){` |
|        - |   85 | `		/* php does not put a private constant in a SUBCLASS's table at all, so naming it` |
|        - |   86 | ``		 * through the child is not an access denial but a plain miss — `Sub::P` reports`` |
|        - |   87 | ``		 * `Undefined constant Sub::P` even from inside the declaring class, and`` |
|        - |   88 | `		 * defined("Sub::P") is false there too. Only the declaring class can answer. */` |
|        7 |   89 | `		return VM_CCONST_NOCONST;` |
|        - |   90 | `	}` |
|      129 |   91 | `	*ppAttr = pAttr;` |
|        - |   92 | ``	/* php answers by the CALLING scope, the same rule the direct `C::K` access uses:`` |
|        - |   93 | `	 * a private constant is invisible from outside its declaring class even to a` |
|        - |   94 | `	 * subclass, and a protected one is visible down the hierarchy. */` |
|      129 |   95 | `	if( !PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|       19 |   96 | `		return VM_CCONST_NOACCESS;` |
|        - |   97 | `	}` |
|      111 |   98 | `	return VM_CCONST_OK;` |
|     1671 |   99 | `}` |
|        - |  100 | `/*` |
|        - |  101 | ` * Raise the Error php raises for a "C::K" name that did not resolve. php prints the class` |
|        - |  102 | `` * part exactly as the caller WROTE it — `c::Q`, `self::P`, `parent::P` — rather than the`` |
|        - |  103 | ` * canonical class name, so the message quotes the source span. Never called with` |
|        - |  104 | ` * VM_CCONST_OK or VM_CCONST_PLAIN.` |
|        - |  105 | ` */` |
|       46 |  106 | `static int VmClassConstError(` |
|        - |  107 | `	ph7_context *pCtx,      /* Call context */` |
|        - |  108 | `	int rc,                 /* VmClassConstLookup() verdict */` |
|        - |  109 | `	const char *zName,      /* The whole "C::K" name */` |
|        - |  110 | `	int nLen,               /* zName length */` |
|        - |  111 | `	int iSep,               /* Offset of the "::" */` |
|        - |  112 | `	ph7_class_attr *pAttr   /* The constant, when one was found */` |
|        - |  113 | `	)` |
|        3 |  114 | `{` |
|       49 |  115 | `	if( nLen > 0 && zName[0] == '\\' ){` |
|        - |  116 | `` 		/* The global-namespace anchor is not part of the name php echoes back: `\C::P` `` |
|        - |  117 | ``		 * reports `C::P` (exactly ONE leading backslash goes, the rest stays). */`` |
|        3 |  118 | `		zName++;` |
|        3 |  119 | `		nLen--;` |
|        3 |  120 | `		iSep--;` |
|        1 |  121 | `	}` |
|       49 |  122 | `	switch( rc ){` |
|        3 |  123 | `		case VM_CCONST_NOCLASS:` |
|        8 |  124 | `			return PH7_VmThrowException(pCtx,"Error","Class \"%.*s\" not found",iSep,zName);` |
|        7 |  125 | `		case VM_CCONST_NOSCOPE:` |
|       23 |  126 | `			return PH7_VmThrowException(pCtx,"Error",` |
|        7 |  127 | `				"Cannot access \"%.*s\" when no class scope is active",iSep,zName);` |
|        1 |  128 | `		case VM_CCONST_NOPARENT:` |
|        3 |  129 | `			return PH7_VmThrowException(pCtx,"Error",` |
|        - |  130 | `				"Cannot access \"parent\" when current class scope has no parent");` |
|        6 |  131 | `		case VM_CCONST_NOACCESS:` |
|       25 |  132 | `			return PH7_VmThrowException(pCtx,"Error","Cannot access %s constant %.*s",` |
|       12 |  133 | `				(pAttr && pAttr->iProtection == PH7_CLASS_PROT_PRIVATE) ? "private" : "protected",` |
|        6 |  134 | `				nLen,zName);` |
|        6 |  135 | `		default:` |
|       12 |  136 | `			break;` |
|        - |  137 | `	}` |
|       14 |  138 | `	return PH7_VmThrowException(pCtx,"Error","Undefined constant %.*s",nLen,zName);` |
|       26 |  139 | `}` |
|        - |  140 | `/*` |
|        - |  141 | ` * bool defined(string $name)` |
|        - |  142 | ` *  Checks whether a given named constant exists.` |
|        - |  143 | ` * Parameter:` |
|        - |  144 | ` *  Name of the desired constant.` |
|        - |  145 | ` * Return` |
|        - |  146 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  147 | ` */` |
|     1604 |  148 | `PH7_PRIVATE int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  149 | `{` |
|        - |  150 | `	ph7_class_attr *pAttr;` |
|        - |  151 | `	ph7_class *pClass;` |
|        - |  152 | `	const char *zName;` |
|     1609 |  153 | `	int nLen = 0;` |
|     1609 |  154 | `	int iSep = 0;` |
|     1609 |  155 | `	int res = 0;` |
|     1609 |  156 | `	if( nArg < 1 ){` |
|        - |  157 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  158 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  159 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  160 | `		return SXRET_OK;` |
|        - |  161 | `	}` |
|        - |  162 | `	/* Extract constant name */` |
|     1609 |  163 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  164 | `	/* Class-constant form "C::K": every miss — unknown class, unknown constant, or one` |
|        - |  165 | `	 * that is not visible from here — is a plain FALSE, since asking whether a name is` |
|        - |  166 | `	 * defined is exactly what defined() is for (this used to consult the` |
|        - |  167 | `	 * global constant table only, so EVERY class constant answered false while` |
|        - |  168 | `	 * constant() read the same name correctly). */` |
|     1609 |  169 | `	if( nLen > 0 ){` |
|     1609 |  170 | `		int iRc = VmClassConstLookup(pCtx->pVm,zName,nLen,&pClass,&pAttr,&iSep);` |
|     1609 |  171 | `		switch( iRc ){` |
|      766 |  172 | `			case VM_CCONST_PLAIN:` |
|     1537 |  173 | `				break;` |
|       18 |  174 | `			case VM_CCONST_OK:` |
|       38 |  175 | `				ph7_result_bool(pCtx,1);` |
|       38 |  176 | `				return SXRET_OK;` |
|        5 |  177 | `			case VM_CCONST_NOSCOPE:` |
|        - |  178 | `			case VM_CCONST_NOPARENT:` |
|        - |  179 | ``				/* php refuses the question rather than answering it: naming `self` where`` |
|        - |  180 | ``				 * no class scope is active is an Error, not a `false`. Every OTHER miss`` |
|        - |  181 | `				 * is a false, so only these two reach the shared thrower. */` |
|       11 |  182 | `				return VmClassConstError(pCtx,iRc,zName,nLen,iSep,pAttr);` |
|       13 |  183 | `			default:` |
|       28 |  184 | `				ph7_result_bool(pCtx,0);` |
|       28 |  185 | `				return SXRET_OK;` |
|        - |  186 | `		}` |
|      766 |  187 | `	}` |
|        - |  188 | `	/* Perform the lookup */` |
|     1537 |  189 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  190 | `		/* Already defined */` |
|     1527 |  191 | `		res = 1;` |
|      761 |  192 | `	}` |
|     1537 |  193 | `	ph7_result_bool(pCtx,res);` |
|     1537 |  194 | `	return SXRET_OK;` |
|      807 |  195 | `}` |
|        - |  196 | `/*` |
|        - |  197 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  198 | ` * below.` |
|        - |  199 | ` */` |
|   102044 |  200 | `PH7_PRIVATE void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        5 |  201 | `{` |
|   102049 |  202 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  203 | `	/* Expand constant value */` |
|   102049 |  204 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|   102049 |  205 | `}` |
|        - |  206 | `/*` |
|        - |  207 | ` * bool define(string $constant_name,expression value)` |
|        - |  208 | ` *  Defines a named constant at runtime.` |
|        - |  209 | ` * Parameter:` |
|        - |  210 | ` *  $constant_name` |
|        - |  211 | ` *   The name of the constant` |
|        - |  212 | ` *  $value` |
|        - |  213 | ` *   Constant value` |
|        - |  214 | ` * Return:` |
|        - |  215 | ` *   TRUE on success,FALSE on failure.` |
|        - |  216 | ` */` |
|      140 |  217 | `PH7_PRIVATE int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  218 | `{` |
|        - |  219 | `	const char *zName;  /* Constant name */` |
|        - |  220 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|      145 |  221 | `	int nLen = 0;       /* Name length */` |
|        - |  222 | `	sxi32 rc;` |
|      145 |  223 | `	if( nArg < 2 ){` |
|        - |  224 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  225 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  226 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  227 | `		return SXRET_OK;` |
|        - |  228 | `	}` |
|      145 |  229 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  230 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  231 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  232 | `		return SXRET_OK;` |
|        - |  233 | `	}` |
|        - |  234 | `	/* Extract constant name */` |
|      145 |  235 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|      145 |  236 | `	if( nLen < 1 ){` |
|      ! 0 |  237 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  238 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  239 | `		return SXRET_OK;` |
|        - |  240 | `	}` |
|        - |  241 | `	/* Duplicate constant value */` |
|      145 |  242 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|      145 |  243 | `	if( pValue == 0 ){` |
|      ! 0 |  244 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  245 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  246 | `		return SXRET_OK;` |
|        - |  247 | `	}` |
|        - |  248 | `	/* Initialize the memory object */` |
|      145 |  249 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  250 | `	/* Register the constant */` |
|        - |  251 | `	{` |
|        - |  252 | `		SyString sConsName;` |
|      145 |  253 | `		SyStringInitFromBuf(&sConsName,zName,(sxu32)nLen);` |
|      215 |  254 | `		rc = PH7_VmRegisterConstantEx(pCtx->pVm,&sConsName,VmExpandUserConstant,pValue,` |
|      140 |  255 | `			(SyString *)SySetPeek(&pCtx->pVm->aFiles),0,1);` |
|        - |  256 | `	}` |
|      145 |  257 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  258 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  259 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  260 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  261 | `		return SXRET_OK;` |
|        - |  262 | `	}` |
|        - |  263 | `	/* Duplicate constant value */` |
|      145 |  264 | `	PH7_MemObjStore(apArg[1],pValue);` |
|      145 |  265 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  266 | `		/* Lower case the constant name */` |
|      ! 0 |  267 | `		char *zCur = (char *)zName;` |
|      ! 0 |  268 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  269 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  270 | `				/* UTF-8 stream */` |
|      ! 0 |  271 | `				zCur++;` |
|      ! 0 |  272 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  273 | `					zCur++;` |
|      ! 0 |  274 | `				}` |
|      ! 0 |  275 | `				continue;` |
|        - |  276 | `			}` |
|      ! 0 |  277 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  278 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  279 | `				zCur[0] = (char)c;` |
|      ! 0 |  280 | `			}` |
|      ! 0 |  281 | `			zCur++;` |
|      ! 0 |  282 | `		}` |
|        - |  283 | `		/* Register the lowercase alias with its OWN value copy (not the same` |
|        - |  284 | `		 * pValue) so the two entries don't share one object — otherwise freeing` |
|        - |  285 | `		 * one on a later overwrite would dangle the other. */` |
|        - |  286 | `		{` |
|      ! 0 |  287 | `			ph7_value *pAlias = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|      ! 0 |  288 | `			if( pAlias ){` |
|      ! 0 |  289 | `				PH7_MemObjInit(pCtx->pVm,pAlias);` |
|      ! 0 |  290 | `				PH7_MemObjStore(apArg[1],pAlias);` |
|      ! 0 |  291 | `				ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pAlias);` |
|      ! 0 |  292 | `			}` |
|        - |  293 | `		}` |
|      ! 0 |  294 | `	}` |
|        - |  295 | `	/* All done,return TRUE */` |
|      145 |  296 | `	ph7_result_bool(pCtx,1);` |
|      145 |  297 | `	return SXRET_OK;` |
|       75 |  298 | `}` |
|        - |  299 | `/*` |
|        - |  300 | ` * value constant(string $name)` |
|        - |  301 | ` *  Returns the value of a constant` |
|        - |  302 | ` * Parameter` |
|        - |  303 | ` *  $name` |
|        - |  304 | ` *    Name of the constant.` |
|        - |  305 | ` * Return` |
|        - |  306 | ` *  Constant value or NULL if not defined.` |
|        - |  307 | ` */` |
|        - |  308 | `/*` |
|        - |  309 | ` * Enum method thunks (PHP 8.1). Every enum's synthesized cases()/from()/` |
|        - |  310 | ` * tryFrom() methods (GenStateCompileEnumMethods, compile.c) forward here with` |
|        - |  311 | ` * the enum's FQN as a literal first argument — the same forwarder pattern the` |
|        - |  312 | ` * Generator/Fiber/Reflection builtins use.` |
|        - |  313 | ` */` |
|        - |  314 | `/* array __phl_enum_cases(string $enumFqn) — declaration-order case list */` |
|       20 |  315 | `PH7_PRIVATE int vm_builtin_enum_cases(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  316 | `{` |
|       21 |  317 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  318 | `	ph7_class_attr **apCase;` |
|        - |  319 | `	ph7_class *pClass;` |
|        - |  320 | `	ph7_value *pArray;` |
|        - |  321 | `	sxu32 n;` |
|        - |  322 | `	sxi32 rc;` |
|       21 |  323 | `	if( nArg < 1 \|\| (pClass = VmExtractEnumClass(pVm,apArg[0])) == 0 ){` |
|      ! 0 |  324 | `		ph7_result_null(pCtx);` |
|      ! 0 |  325 | `		return SXRET_OK;` |
|        - |  326 | `	}` |
|       21 |  327 | `	rc = VmEnumMaterialize(pVm,pClass);` |
|       21 |  328 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  329 | `		return rc;` |
|        - |  330 | `	}` |
|       21 |  331 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 |  332 | `	if( pArray == 0 ){` |
|      ! 0 |  333 | `		ph7_result_null(pCtx);` |
|      ! 0 |  334 | `		return SXRET_OK;` |
|        - |  335 | `	}` |
|       21 |  336 | `	apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|       83 |  337 | `	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|       63 |  338 | `		ph7_value *pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,apCase[n]->nIdx);` |
|       63 |  339 | `		if( pSlot ){` |
|       63 |  340 | `			ph7_array_add_elem(pArray,0,pSlot); /* Copies; the object ref is retained */` |
|       31 |  341 | `		}` |
|       32 |  342 | `	}` |
|       21 |  343 | `	ph7_result_value(pCtx,pArray);` |
|       21 |  344 | `	return SXRET_OK;` |
|       11 |  345 | `}` |
|        - |  346 | `/*` |
|        - |  347 | `` * php declares from()/tryFrom() as `string\|int $value` on the BackedEnum`` |
|        - |  348 | ` * prototype, so the argument arrives in either form and the enum's own backing` |
|        - |  349 | ` * type decides what happens next -- which is why the refusal is worded against` |
|        - |  350 | ` * the BACKING type ("must be of type int, string given" for an int-backed enum` |
|        - |  351 | ` * given "2x") and not against the declared union. php words the union only when` |
|        - |  352 | ` * the value is neither a string nor an int and the enum is string-backed; the` |
|        - |  353 | ` * asymmetry is php's own.` |
|        - |  354 | ` *` |
|        - |  355 | ` * Everything else is ordinary weak coercion, so an int-backed enum accepts "02"` |
|        - |  356 | ` * and " 2" as 2, and a string-backed one takes an int (or a bool, or a` |
|        - |  357 | ` * non-lossy float) through the INT arm first: S::from(1.0) looks for "1", not` |
|        - |  358 | ` * "1.0", and S::from(false) for "0". A LOSSY float and a null are php` |
|        - |  359 | ` * DEPRECATIONS, so PH7_IntArgResolve refuses them (§10 scope policy) with the` |
|        - |  360 | ` * TypeError php will eventually raise.` |
|        - |  361 | ` */` |
|      178 |  362 | `static sxi32 VmEnumCoerceNeedle(ph7_context *pCtx,ph7_class *pClass,ph7_value *pArg,` |
|        - |  363 | `	ph7_value *pOut)` |
|        2 |  364 | `{` |
|      180 |  365 | `	int bStrBacked = pClass->nEnumBacking != MEMOBJ_INT;` |
|      180 |  366 | `	sxi64 iVal = 0;` |
|        - |  367 | `	sxi32 rc;` |
|      180 |  368 | `	PH7_MemObjInit(pCtx->pVm,pOut);` |
|      180 |  369 | `	if( ph7_value_is_string(pArg) && bStrBacked ){` |
|       72 |  370 | `		PH7_MemObjLoad(pArg,pOut);` |
|       72 |  371 | `		return SXRET_OK;` |
|        - |  372 | `	}` |
|        - |  373 | `	/* A native method's own name is already qualified ("I2::from"). */` |
|      163 |  374 | `	rc = PH7_IntArgResolve(pCtx,pArg,ph7_function_name(pCtx),1,"$value",` |
|       54 |  375 | `		bStrBacked ? "string\|int" : "int",&iVal);` |
|      109 |  376 | `	if( rc != PH7_OK ){` |
|       13 |  377 | `		return rc;` |
|        - |  378 | `	}` |
|       97 |  379 | `	if( bStrBacked ){` |
|        - |  380 | `		char zNum[32];` |
|       35 |  381 | `		int nNum = SyBufferFormat(zNum,sizeof(zNum),"%qd",iVal);` |
|       35 |  382 | `		PH7_MemObjStringAppend(pOut,zNum,(sxu32)nNum);` |
|       18 |  383 | `	}else{` |
|       63 |  384 | `		pOut->x.iVal = iVal;` |
|       63 |  385 | `		MemObjSetType(pOut,MEMOBJ_INT);` |
|        - |  386 | `	}` |
|       97 |  387 | `	return SXRET_OK;` |
|       91 |  388 | `}` |
|        - |  389 | `/* Shared scan for from()/tryFrom(): return the slot of the case whose backing` |
|        - |  390 | ` * value equals *pNeedle (already coerced to the backing type), or 0 on miss. */` |
|      166 |  391 | `static ph7_value * VmEnumFindCaseByValue(ph7_vm *pVm,ph7_class *pClass,ph7_value *pNeedle)` |
|        2 |  392 | `{` |
|      168 |  393 | `	ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|        - |  394 | `	sxu32 n;` |
|      384 |  395 | `	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|      298 |  396 | `		ph7_value *pVal = VmEnumCaseBackingValue(pVm,apCase[n]);` |
|      298 |  397 | `		int bMatch = 0;` |
|      298 |  398 | `		if( pVal ){` |
|      298 |  399 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|      101 |  400 | `				bMatch = (pNeedle->iFlags & MEMOBJ_INT) && pVal->x.iVal == pNeedle->x.iVal;` |
|       51 |  401 | `			}else{` |
|      296 |  402 | `				bMatch = (pNeedle->iFlags & MEMOBJ_STRING)` |
|      196 |  403 | `					&& SyBlobLength(&pVal->sBlob) == SyBlobLength(&pNeedle->sBlob)` |
|      294 |  404 | `					&& SyMemcmp(SyBlobData(&pVal->sBlob),SyBlobData(&pNeedle->sBlob),` |
|      126 |  405 | `						SyBlobLength(&pNeedle->sBlob)) == 0;` |
|        - |  406 | `			}` |
|      148 |  407 | `		}` |
|      298 |  408 | `		if( bMatch ){` |
|       82 |  409 | `			return (ph7_value *)SySetAt(&pVm->aMemObj,apCase[n]->nIdx);` |
|        - |  410 | `		}` |
|      109 |  411 | `	}` |
|       87 |  412 | `	return 0;` |
|       85 |  413 | `}` |
|        - |  414 | `/* static from(int\|string $value) / static tryFrom(int\|string $value) */` |
|      178 |  415 | `static int VmEnumFromCommon(ph7_context *pCtx,int nArg,ph7_value **apArg,int bTry)` |
|        2 |  416 | `{` |
|      180 |  417 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  418 | `	ph7_class *pClass;` |
|        - |  419 | `	ph7_value *pFound;` |
|        - |  420 | `	ph7_value sNeedle;` |
|        - |  421 | `	sxi32 rc;` |
|      180 |  422 | `	if( nArg < 2 \|\| (pClass = VmExtractEnumClass(pVm,apArg[0])) == 0 ){` |
|      ! 0 |  423 | `		ph7_result_null(pCtx);` |
|      ! 0 |  424 | `		return SXRET_OK;` |
|        - |  425 | `	}` |
|      180 |  426 | `	rc = VmEnumMaterialize(pVm,pClass);` |
|      180 |  427 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  428 | `		return rc;` |
|        - |  429 | `	}` |
|      180 |  430 | `	rc = VmEnumCoerceNeedle(pCtx,pClass,apArg[1],&sNeedle);` |
|      180 |  431 | `	if( rc != SXRET_OK ){` |
|       13 |  432 | `		PH7_MemObjRelease(&sNeedle);` |
|       13 |  433 | `		return rc;` |
|        - |  434 | `	}` |
|      168 |  435 | `	pFound = VmEnumFindCaseByValue(pVm,pClass,&sNeedle);` |
|      168 |  436 | `	if( pFound ){` |
|       82 |  437 | `		ph7_result_value(pCtx,pFound);` |
|       82 |  438 | `		PH7_MemObjRelease(&sNeedle);` |
|       82 |  439 | `		return SXRET_OK;` |
|        - |  440 | `	}` |
|       87 |  441 | `	if( bTry ){` |
|       47 |  442 | `		ph7_result_null(pCtx);` |
|       47 |  443 | `		PH7_MemObjRelease(&sNeedle);` |
|       47 |  444 | `		return SXRET_OK;` |
|        - |  445 | `	}` |
|       41 |  446 | `	if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|        - |  447 | `		char zVal[32];` |
|        9 |  448 | `		SyBufferFormat(zVal,sizeof(zVal),"%qd",sNeedle.x.iVal);` |
|       13 |  449 | `		rc = PH7_VmThrowException(pCtx,"ValueError",` |
|        4 |  450 | `			"%s is not a valid backing value for enum %z",zVal,&pClass->sName);` |
|        5 |  451 | `	}else{` |
|       49 |  452 | `		rc = PH7_VmThrowException(pCtx,"ValueError",` |
|        - |  453 | `			"\"%.*s\" is not a valid backing value for enum %z",` |
|       32 |  454 | `			(int)SyBlobLength(&sNeedle.sBlob),(const char *)SyBlobData(&sNeedle.sBlob),` |
|       16 |  455 | `			&pClass->sName);` |
|        - |  456 | `	}` |
|       41 |  457 | `	PH7_MemObjRelease(&sNeedle);` |
|       41 |  458 | `	return rc;` |
|       91 |  459 | `}` |
|       96 |  460 | `PH7_PRIVATE int vm_builtin_enum_from(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  461 | `{` |
|       98 |  462 | `	return VmEnumFromCommon(pCtx,nArg,apArg,FALSE);` |
|        2 |  463 | `}` |
|       82 |  464 | `PH7_PRIVATE int vm_builtin_enum_tryfrom(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  465 | `{` |
|       83 |  466 | `	return VmEnumFromCommon(pCtx,nArg,apArg,TRUE);` |
|        1 |  467 | `}` |
|        - |  468 | `/*` |
|        - |  469 | ` * bool enum_exists(string $enum, bool $autoload = true)` |
|        - |  470 | ` *  TRUE only for a declared enum (PHP 8.1); a plain class/interface is FALSE.` |
|        - |  471 | ` */` |
|       14 |  472 | `PH7_PRIVATE int vm_builtin_enum_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  473 | `{` |
|       16 |  474 | `	ph7_class *pClass = 0;` |
|       16 |  475 | `	if( nArg > 0 ){` |
|       16 |  476 | `		pClass = VmExtractEnumClass(pCtx->pVm,apArg[0]);` |
|        7 |  477 | `	}` |
|       16 |  478 | `	ph7_result_bool(pCtx,pClass != 0);` |
|       16 |  479 | `	return SXRET_OK;` |
|        2 |  480 | `}` |
|     1728 |  481 | `PH7_PRIVATE int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 |  482 | `{` |
|        - |  483 | `	SyHashEntry *pEntry;` |
|        - |  484 | `	ph7_constant *pCons;` |
|        - |  485 | `	const char *zName; /* Constant name */` |
|        - |  486 | `	ph7_value sVal;    /* Constant value */` |
|        - |  487 | `	int nLen;` |
|     1732 |  488 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  489 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  490 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  491 | `		ph7_result_null(pCtx);` |
|      ! 0 |  492 | `		return SXRET_OK;` |
|        - |  493 | `	}` |
|        - |  494 | `	/* Extract the constant name */` |
|     1732 |  495 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  496 | `	/* Class-constant form "C::K" (band A #4): resolve the class — interfaces` |
|        - |  497 | `	 * included — and read the mounted constant slot; php throws a catchable` |
|        - |  498 | `	 * Error for an unknown class or constant (pre-fix this path warned` |
|        - |  499 | `	 * "Undefined constant" and returned NULL without ever looking at the` |
|        - |  500 | `` 	 * class). The resolution is defined()'s: it also answers `self`/`parent`/`static` `` |
|        - |  501 | `	 * against the live class scope and refuses a constant that is not VISIBLE from` |
|        - |  502 | ``	 * here — both of which this used to walk straight past, so a `private const` was`` |
|        - |  503 | ``	 * readable from anywhere through the string form while the direct `C::K` access`` |
|        - |  504 | `	 * threw. */` |
|        - |  505 | `	{` |
|     1732 |  506 | `		ph7_class_attr *pAttr = 0;` |
|     1732 |  507 | `		ph7_class *pClass = 0;` |
|     1732 |  508 | `		int iSep = 0;` |
|     1732 |  509 | `		int iRc = VmClassConstLookup(pCtx->pVm,zName,nLen,&pClass,&pAttr,&iSep);` |
|     1732 |  510 | `		if( iRc != VM_CCONST_PLAIN ){` |
|      111 |  511 | `			if( iRc != VM_CCONST_OK ){` |
|       74 |  512 | `				return VmClassConstError(pCtx,iRc,zName,nLen,iSep,pAttr);` |
|        - |  513 | `			}` |
|       75 |  514 | `			if( pAttr->nIdx == SXU32_HIGH ){` |
|        - |  515 | `				/* Unmaterialized: enum case → materialize the singletons` |
|        - |  516 | `				 * (all of them: constant("S::A") is a direct access, like` |
|        - |  517 | `				 * OP_MEMBER); plain constant → run its initializer. Unlike` |
|        - |  518 | `				 * defined(), reading the value has to force this. */` |
|        - |  519 | `				sxi32 rcEnum;` |
|       45 |  520 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE ){` |
|        3 |  521 | `					rcEnum = VmEnumMaterialize(pCtx->pVm,pClass);` |
|        2 |  522 | `				}else{` |
|       43 |  523 | `					rcEnum = VmClassConstEvalOnDemand(pCtx->pVm,pClass,pAttr);` |
|        - |  524 | `				}` |
|       45 |  525 | `				if( rcEnum != SXRET_OK ){` |
|        3 |  526 | `					return rcEnum;` |
|        - |  527 | `				}` |
|       20 |  528 | `			}` |
|        - |  529 | `			{` |
|       72 |  530 | `				ph7_value *pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pAttr->nIdx);` |
|       72 |  531 | `				if( pValue ){` |
|       72 |  532 | `					if( SySetUsed(&pAttr->aAttrs) > 0 ){` |
|        - |  533 | `						/* #[\Deprecated] warns through constant() too (php) */` |
|        3 |  534 | `						VmDeprecatedConstNotice(pCtx->pVm,pClass,pAttr);` |
|        1 |  535 | `					}` |
|       72 |  536 | `					ph7_result_value(pCtx,pValue);` |
|       72 |  537 | `					return SXRET_OK;` |
|        - |  538 | `				}` |
|        - |  539 | `			}` |
|        - |  540 | `			/* Declared but with no slot to read: the same dead end the pre-fix code` |
|        - |  541 | `			 * fell through to. */` |
|      ! 0 |  542 | `			return VmClassConstError(pCtx,VM_CCONST_NOCONST,zName,nLen,iSep,pAttr);` |
|        - |  543 | `		}` |
|        - |  544 | `	}` |
|        - |  545 | `	/* Perform the query */` |
|     1623 |  546 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|     1623 |  547 | `	if( pEntry == 0 ){` |
|        - |  548 | `		/* php 8: a catchable Error, not a notice + NULL (band A #4) */` |
|        8 |  549 | `		return PH7_VmThrowException(pCtx,"Error",` |
|        2 |  550 | `			"Undefined constant \"%.*s\"",nLen,zName);` |
|        - |  551 | `	}` |
|     1619 |  552 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  553 | `	/* Point to the structure that describe the constant */` |
|     1619 |  554 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  555 | `	/* Extract constant value by calling it's associated callback` |
|        - |  556 | `	 * (emits the #[\Deprecated] notice first when attributed, php) */` |
|     1619 |  557 | `	VmExpandConstantWithNotice(pCtx->pVm,pCons,&sVal);` |
|        - |  558 | `	/* Return that value */` |
|     1619 |  559 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  560 | `	/* Cleanup */` |
|     1619 |  561 | `	PH7_MemObjRelease(&sVal);` |
|     1619 |  562 | `	return SXRET_OK;` |
|      868 |  563 | `}` |
|        - |  564 | `/*` |
|        - |  565 | ` * Hash walker callback used by the [get_defined_constants()] function defined` |
|        - |  566 | ` * below. php's answer is a MAP -- the constant's name is the KEY and its VALUE` |
|        - |  567 | ``  * is the element -- which is what makes `get_defined_constants()['PHP_EOL']` `` |
|        - |  568 | ` * the documented way to read one. PHL used to answer a LIST of names, so every` |
|        - |  569 | ``  * such lookup was an `Undefined array key` and NULL, and `in_array($n, $c)` `` |
|        - |  570 | `` * answered where php wants `isset($c[$n])`: the array had the right length and`` |
|        - |  571 | ` * the wrong shape.` |
|        - |  572 | ` */` |
|    83212 |  573 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        3 |  574 | `{` |
|        - |  575 | ``	/* SNAPSHOT ONLY -- nothing is expanded during the walk. A `const` whose`` |
|        - |  576 | `	 * initializer is a bytecode program runs USER CODE when it expands, and user` |
|        - |  577 | ``	 * code can `define()`: that grows hConstant while SyHashForEach is holding a`` |
|        - |  578 | `	 * fixed entry count, and the walk then runs off the end of the bucket chain` |
|        - |  579 | `	 * (a segfault, reproducible from a const initializer that constructs an` |
|        - |  580 | `	 * object whose __construct defines a constant). Collect first, expand after. */` |
|    83215 |  581 | `	SySet *pOut = (SySet *)pUserData;` |
|    83215 |  582 | `	if( pEntry == 0 \|\| pEntry->pUserData == 0 ){` |
|      ! 0 |  583 | `		return SXRET_OK;` |
|        - |  584 | `	}` |
|    83215 |  585 | `	SySetPut(pOut,(const void *)&pEntry);` |
|    83215 |  586 | `	return SXRET_OK;` |
|    41609 |  587 | `}` |
|        - |  588 | `/*` |
|        - |  589 | ` * Add one snapshotted constant to the answer, under its name.` |
|        - |  590 | ` */` |
|    83212 |  591 | `static sxi32 VmConstDumpEntry(ph7_value *pTarget,SyHashEntry *pEntry)` |
|        3 |  592 | `{` |
|    83215 |  593 | `	ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  594 | `	ph7_value sName,sVal;` |
|        - |  595 | `	sxi32 rc;` |
|        - |  596 | `	/* Prepare the constant name for insertion */` |
|    83215 |  597 | `	PH7_MemObjInitFromString(pTarget->pVm,&sName,0);` |
|    83215 |  598 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  599 | `	/* ...and its VALUE, through the SAME evaluate-once path an ordinary read` |
|        - |  600 | ``	 * takes -- so a `const C = new Foo();` reported here is the object the`` |
|        - |  601 | ``	 * program itself sees, not a second one. The `#[\Deprecated]` NOTICE is what`` |
|        - |  602 | `	 * is skipped (VmExpandConstantOnce rather than …WithNotice): describing a` |
|        - |  603 | `	 * constant is not reading one, and php raises nothing here either. */` |
|    83215 |  604 | `	PH7_MemObjInit(pTarget->pVm,&sVal);` |
|    83215 |  605 | `	rc = VmExpandConstantOnce(pTarget->pVm,pCons,&sVal);` |
|    83215 |  606 | `	if( rc == SXRET_OK ){` |
|    83215 |  607 | `		ph7_array_add_elem(pTarget,&sName,&sVal); /* Will make its own copy */` |
|    41606 |  608 | `	}` |
|    83215 |  609 | `	PH7_MemObjRelease(&sVal);` |
|    83215 |  610 | `	PH7_MemObjRelease(&sName);` |
|    83215 |  611 | `	return rc;` |
|        3 |  612 | `}` |
|        - |  613 | `/*` |
|        - |  614 | ` * array get_defined_constants(bool $categorize = false)` |
|        - |  615 | ` *  Returns an associative array with the names AND VALUES of all defined` |
|        - |  616 | ` *  constants.` |
|        - |  617 | ` * Parameters` |
|        - |  618 | ` *  $categorize` |
|        - |  619 | ` *   TRUE groups the map one level deeper, by the extension each constant` |
|        - |  620 | ` *   belongs to. This engine has no extension partition (the same limitation` |
|        - |  621 | ` *   ReflectionFunction::getExtensionName() records), so it answers php's two` |
|        - |  622 | `` *   buckets it CAN tell apart: `user` for everything a script defined with`` |
|        - |  623 | `` *   define()/const, and `Core` for the engine's own -- where php would spread`` |
|        - |  624 | ` *   the latter over standard/date/pcre/json/… as well.` |
|        - |  625 | ` * Returns` |
|        - |  626 | ` *  The constants currently defined, name => value.` |
|        - |  627 | ` */` |
|       62 |  628 | `PH7_PRIVATE int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 |  629 | `{` |
|       65 |  630 | `	ph7_value *pArray,*pAll,*pUser = 0;` |
|        - |  631 | `	SySet aSnap;` |
|        - |  632 | `	SyHashEntry **apEntry;` |
|        - |  633 | `	sxu32 n,nSnap;` |
|       65 |  634 | `	int bCategorize = nArg > 0 && ph7_value_to_bool(apArg[0]);` |
|        - |  635 | `	/* Create the array first*/` |
|       65 |  636 | `	pArray = ph7_context_new_array(pCtx);` |
|       65 |  637 | `	if( pArray == 0 ){` |
|        - |  638 | `		/* Return NULL */` |
|      ! 0 |  639 | `		ph7_result_null(pCtx);` |
|      ! 0 |  640 | `		return SXRET_OK;` |
|        - |  641 | `	}` |
|       65 |  642 | `	pAll = pArray;` |
|       65 |  643 | `	if( bCategorize ){` |
|       13 |  644 | `		pAll = ph7_context_new_array(pCtx);` |
|       13 |  645 | `		pUser = ph7_context_new_array(pCtx);` |
|       13 |  646 | `		if( pAll == 0 \|\| pUser == 0 ){` |
|      ! 0 |  647 | `			ph7_result_null(pCtx);` |
|      ! 0 |  648 | `			return SXRET_OK;` |
|        - |  649 | `		}` |
|        5 |  650 | `	}` |
|        - |  651 | `	/* Snapshot the table, then expand: expanding runs user code, which may` |
|        - |  652 | `	 * define() and grow the table under the walk (see VmHashConstStep). */` |
|       65 |  653 | `	SySetInit(&aSnap,&pCtx->pVm->sAllocator,sizeof(SyHashEntry *));` |
|       65 |  654 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,&aSnap);` |
|       65 |  655 | `	apEntry = (SyHashEntry **)SySetBasePtr(&aSnap);` |
|       65 |  656 | `	nSnap = SySetUsed(&aSnap);` |
|        - |  657 | `	/* Describing the table is not READING its entries: php's deprecated constants` |
|        - |  658 | `	 * report when a program names one, and get_defined_constants() lists them in` |
|        - |  659 | `	 * silence. */` |
|       65 |  660 | `	pCtx->pVm->bConstEnum++;` |
|    83277 |  661 | `	for( n = 0 ; n < nSnap ; ++n ){` |
|    83215 |  662 | `		ph7_constant *pCons = (ph7_constant *)apEntry[n]->pUserData;` |
|    83215 |  663 | `		sxi32 rcExp = VmConstDumpEntry(pUser && pCons->bUserDefined ? pUser : pAll,apEntry[n]);` |
|    83215 |  664 | `		if( rcExp != SXRET_OK ){` |
|        - |  665 | `			/* An initializer raised while being described: stop, exactly as any` |
|        - |  666 | `			 * other builtin does when the php it invoked did not return.` |
|        - |  667 | `			 * Carrying on would run every LATER initializer past a throw that has` |
|        - |  668 | `			 * already been landed. */` |
|      ! 0 |  669 | `			pCtx->pVm->bConstEnum--;` |
|      ! 0 |  670 | `			SySetRelease(&aSnap);` |
|      ! 0 |  671 | `			if( bCategorize ){` |
|      ! 0 |  672 | `				ph7_context_release_value(pCtx,pAll);` |
|      ! 0 |  673 | `				ph7_context_release_value(pCtx,pUser);` |
|      ! 0 |  674 | `			}` |
|      ! 0 |  675 | `			return rcExp;` |
|        - |  676 | `		}` |
|    41609 |  677 | `	}` |
|       65 |  678 | `	pCtx->pVm->bConstEnum--;` |
|       65 |  679 | `	SySetRelease(&aSnap);` |
|       65 |  680 | `	if( bCategorize ){` |
|        - |  681 | ``		/* php's own order: the engine's buckets first, `user` last -- and php`` |
|        - |  682 | `		 * omits a category with nothing in it, so a script that defined no` |
|        - |  683 | ``		 * constant of its own has no `user` key at all rather than an empty one. */`` |
|       13 |  684 | `		ph7_array_add_strkey_elem(pArray,"Core",pAll);` |
|       13 |  685 | `		if( ph7_array_count(pUser) > 0 ){` |
|       11 |  686 | `			ph7_array_add_strkey_elem(pArray,"user",pUser);` |
|        4 |  687 | `		}` |
|       13 |  688 | `		ph7_context_release_value(pCtx,pAll);` |
|       13 |  689 | `		ph7_context_release_value(pCtx,pUser);` |
|        5 |  690 | `	}` |
|        - |  691 | `	/* Return the created array */` |
|       65 |  692 | `	ph7_result_value(pCtx,pArray);` |
|       65 |  693 | `	return SXRET_OK;` |
|       34 |  694 | `}` |
|        - |  695 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - |  696 | `/*` |
|        - |  697 | ` * Section:` |
|        - |  698 | ` *  Random numbers/string generators.` |
|        - |  699 | ` * Status:` |
|        - |  700 | ` *    Stable.` |
|        - |  701 | ` */` |
|        - |  702 | `/*` |
|        - |  703 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  704 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|        - |  705 | ` * implemented in src/sx/sxrand.c).` |
|        - |  706 | ` */` |
|     4974 |  707 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        5 |  708 | `{` |
|        - |  709 | `	sxu32 iNum;` |
|     4979 |  710 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     4979 |  711 | `	return iNum;` |
|        5 |  712 | `}` |
|        - |  713 | `/*` |
|        - |  714 | ` * The MT19937 generator that backs PHP's rand()/mt_rand() family. It is kept` |
|        - |  715 | ` * separate from the RC4 SyRandomness above so that srand()/mt_srand() give` |
|        - |  716 | ` * userland PHP's reproducible sequence without perturbing the engine's internal` |
|        - |  717 | ` * entropy (object ids, uniqid, quicksort pivots stay on the RC4 generator, as` |
|        - |  718 | ` * they are in PHP too — srand does not touch those).` |
|        - |  719 | ` */` |
|        - |  720 | `/*` |
|        - |  721 | ` * Reset the MT19937 state to a 32-bit seed (PHP truncates its int seed likewise).` |
|        - |  722 | ` */` |
|      314 |  723 | `PH7_PRIVATE void PH7_VmMtSrand(ph7_vm *pVm,sxu32 nSeed,int bLegacyTwist)` |
|        2 |  724 | `{` |
|      316 |  725 | `	SyMT19937Seed(&pVm->sMt,nSeed,bLegacyTwist);` |
|      316 |  726 | `	pVm->mtSeeded = TRUE;` |
|      316 |  727 | `}` |
|        - |  728 | `/*` |
|        - |  729 | ` * Draw the next full 32-bit MT19937 word, seeding lazily from the OS CSPRNG on` |
|        - |  730 | ` * first use exactly as PHP auto-seeds when rand()/mt_rand() runs before srand().` |
|        - |  731 | ` */` |
|     2582 |  732 | `PH7_PRIVATE sxu32 PH7_VmMtRand(ph7_vm *pVm)` |
|        2 |  733 | `{` |
|     2584 |  734 | `	if( !pVm->mtSeeded ){` |
|        - |  735 | `		sxu32 nSeed;` |
|        3 |  736 | `		if( SyOSCSPRNG((void *)&nSeed,sizeof(nSeed)) != SXRET_OK ){` |
|        - |  737 | `			/* No OS entropy source: fall back to the RC4 generator's output. */` |
|      ! 0 |  738 | `			nSeed = PH7_VmRandomNum(pVm);` |
|      ! 0 |  739 | `		}` |
|        - |  740 | `		/* An un-seeded generator is php's default one, never MT_RAND_PHP. */` |
|        3 |  741 | `		SyMT19937Seed(&pVm->sMt,nSeed,FALSE);` |
|        3 |  742 | `		pVm->mtSeeded = TRUE;` |
|        1 |  743 | `	}` |
|     2584 |  744 | `	return SyMT19937Next(&pVm->sMt);` |
|        2 |  745 | `}` |
|        - |  746 | `/*` |
|        - |  747 | ` * Map a full 32-bit draw uniformly into [0,uMax] (uMax is the range width, i.e.` |
|        - |  748 | ` * max-min). Rejection sampling against the largest unbiased ceiling, matching` |
|        - |  749 | ` * PHP's php_random_range32().` |
|        - |  750 | ` */` |
|     2378 |  751 | `static sxu32 VmMtRange32(ph7_vm *pVm,sxu32 uMax)` |
|        2 |  752 | `{` |
|        - |  753 | `	sxu32 result,limit;` |
|     2380 |  754 | `	result = PH7_VmMtRand(pVm);` |
|        - |  755 | `	/* Whole 32-bit domain: no scaling needed. */` |
|     2380 |  756 | `	if( uMax == 0xFFFFFFFFU ){` |
|      ! 0 |  757 | `		return result;` |
|        - |  758 | `	}` |
|        - |  759 | `	/* Make the range inclusive of max. */` |
|     2380 |  760 | `	uMax++;` |
|        - |  761 | `	/* Powers of two are unbiased under a plain mask. */` |
|     2380 |  762 | `	if( (uMax & (uMax - 1)) == 0 ){` |
|       86 |  763 | `		return result & (uMax - 1);` |
|        - |  764 | `	}` |
|        - |  765 | `	/* Ceiling under which 0xFFFFFFFF % uMax == 0; discard draws above it. */` |
|     2296 |  766 | `	limit = 0xFFFFFFFFU - (0xFFFFFFFFU % uMax) - 1;` |
|     2296 |  767 | `	while( result > limit ){` |
|      ! 0 |  768 | `		result = PH7_VmMtRand(pVm);` |
|      ! 0 |  769 | `	}` |
|     2296 |  770 | `	return result % uMax;` |
|     1191 |  771 | `}` |
|        - |  772 | `/*` |
|        - |  773 | ` * 64-bit-wide range: assemble two draws (high word first, as PHP does) and` |
|        - |  774 | ` * reject-sample. Matches PHP's php_random_range64().` |
|        - |  775 | ` */` |
|       14 |  776 | `static sxu64 VmMtRange64(ph7_vm *pVm,sxu64 uMax)` |
|        2 |  777 | `{` |
|        - |  778 | `	sxu64 result,limit;` |
|        - |  779 | `	/* First draw fills the low word, second draw the high word — order is` |
|        - |  780 | `	 * significant and matches php's php_random_range64() assembly. */` |
|       16 |  781 | `	result = (sxu64)PH7_VmMtRand(pVm);` |
|       16 |  782 | `	result \|= (sxu64)PH7_VmMtRand(pVm) << 32;` |
|       16 |  783 | `	if( uMax == 0xFFFFFFFFFFFFFFFFULL ){` |
|      ! 0 |  784 | `		return result;` |
|        - |  785 | `	}` |
|       16 |  786 | `	uMax++;` |
|       16 |  787 | `	if( (uMax & (uMax - 1)) == 0 ){` |
|       14 |  788 | `		return result & (uMax - 1);` |
|        - |  789 | `	}` |
|        3 |  790 | `	limit = 0xFFFFFFFFFFFFFFFFULL - (0xFFFFFFFFFFFFFFFFULL % uMax) - 1;` |
|        3 |  791 | `	while( result > limit ){` |
|      ! 0 |  792 | `		result = (sxu64)PH7_VmMtRand(pVm);` |
|      ! 0 |  793 | `		result \|= (sxu64)PH7_VmMtRand(pVm) << 32;` |
|      ! 0 |  794 | `	}` |
|        3 |  795 | `	return result % uMax;` |
|        9 |  796 | `}` |
|        - |  797 | `/*` |
|        - |  798 | ` * Return a value uniformly in the inclusive range [iMin,iMax]. The caller` |
|        - |  799 | ` * guarantees iMin <= iMax. Mirrors PHP's php_mt_rand_range(): a range that fits` |
|        - |  800 | ` * in 32 bits takes the 32-bit path, a wider one the 64-bit path.` |
|        - |  801 | ` */` |
|     2392 |  802 | `PH7_PRIVATE sxi64 PH7_VmMtRandRange(ph7_vm *pVm,sxi64 iMin,sxi64 iMax)` |
|        2 |  803 | `{` |
|     2394 |  804 | `	sxu64 uMax = (sxu64)iMax - (sxu64)iMin;` |
|     2394 |  805 | `	if( uMax > 0xFFFFFFFFULL ){` |
|       16 |  806 | `		return (sxi64)(VmMtRange64(pVm,uMax) + (sxu64)iMin);` |
|        - |  807 | `	}` |
|     2380 |  808 | `	return (sxi64)((sxu64)VmMtRange32(pVm,(sxu32)uMax) + (sxu64)iMin);` |
|     1198 |  809 | `}` |
|        - |  810 | `/*` |
|        - |  811 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  812 | ` * Note that the generated string is NOT null terminated.` |
|        - |  813 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|        - |  814 | ` * implemented in src/sx/sxrand.c).` |
|        - |  815 | ` */` |
|  6755168 |  816 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        5 |  817 | `{` |
|        - |  818 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  819 | `	int i;` |
|        - |  820 | `	/* Generate a binary string first */` |
|  6755173 |  821 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  822 | `	/* Turn the binary string into english based alphabet */` |
| 74307885 |  823 | `	for( i = 0 ; i < nLen ; ++i ){` |
| 67552717 |  824 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
| 33776361 |  825 | `	 }` |
|  6755173 |  826 | `}` |
|        - |  827 | `/*` |
|        - |  828 | ` * int rand()` |
|        - |  829 | ` * int mt_rand()` |
|        - |  830 | ` * int rand(int $min,int $max)` |
|        - |  831 | ` * int mt_rand(int $min,int $max)` |
|        - |  832 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  833 | ` * Parameter` |
|        - |  834 | ` *  $min` |
|        - |  835 | ` *    The lowest value to return (default: 0)` |
|        - |  836 | ` *  $max` |
|        - |  837 | ` *   The highest value to return (default: getrandmax())` |
|        - |  838 | ` * Return` |
|        - |  839 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  840 | ` * Note:` |
|        - |  841 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  842 | ` *  by te SQLite3 library.` |
|        - |  843 | ` */` |
|     1894 |  844 | `PH7_PRIVATE int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  845 | `{` |
|     1896 |  846 | `	SyString *pName = &pCtx->pFunc->sName;` |
|     3348 |  847 | `	int bMt = (pName->nByte == sizeof("mt_rand")-1` |
|     1894 |  848 | `		&& SyMemcmp(pName->zString,"mt_rand",sizeof("mt_rand")-1) == 0);` |
|        - |  849 | `	/* php accepts exactly 0 or exactly 2 arguments (min,max); 1 or 3+ is an` |
|        - |  850 | `	 * ArgumentCountError. The central arity table can't express "0 or 2", so` |
|        - |  851 | `	 * it is enforced here (was a silent wrong result for the raw draw). */` |
|     1896 |  852 | `	if( nArg == 1 \|\| nArg > 2 ){` |
|       13 |  853 | `		return PH7_VmThrowException(pCtx,` |
|        - |  854 | `			"ArgumentCountError",` |
|        - |  855 | `			"%z() expects exactly 2 arguments, %d given",` |
|        4 |  856 | `			pName, nArg` |
|        - |  857 | `			);` |
|        - |  858 | `	}` |
|     1888 |  859 | `	if( nArg == 2 ){` |
|        - |  860 | `		sxi64 iMin,iMax;` |
|        - |  861 | `		/* Signed 64-bit endpoints: the old unsigned math wrapped negative` |
|        - |  862 | `		 * ranges to huge positives (rand(-10,-1) -> ~4e9) and mis-handled` |
|        - |  863 | `		 * min==max. */` |
|     1776 |  864 | `		iMin = ph7_value_to_int64(apArg[0]);` |
|     1776 |  865 | `		iMax = ph7_value_to_int64(apArg[1]);` |
|     1776 |  866 | `		if( iMin > iMax ){` |
|        9 |  867 | `			if( bMt ){` |
|        - |  868 | `				/* mt_rand() is strict: php throws a catchable ValueError. */` |
|        5 |  869 | `				return PH7_VmThrowException(pCtx,` |
|        - |  870 | `					"ValueError",` |
|        - |  871 | `					"mt_rand(): Argument #2 ($max) must be greater than or equal to argument #1 ($min)"` |
|        - |  872 | `					);` |
|        - |  873 | `			}` |
|        - |  874 | `			/* rand() swaps the bounds for backward compatibility (php keeps` |
|        - |  875 | `			 * this quirk; only mt_rand() rejects a reversed range). */` |
|        5 |  876 | `			{ sxi64 iTmp = iMin; iMin = iMax; iMax = iTmp; }` |
|        2 |  877 | `		}` |
|     1772 |  878 | `		if( pCtx->pVm->sMt.bLegacyTwist ){` |
|        - |  879 | `			/* MT_RAND_PHP is a whole generator, mapping included: php keeps its old` |
|        - |  880 | `			 * RAND_RANGE_BADSCALING here — a 31-bit draw scaled through a double,` |
|        - |  881 | `			 * which is biased and is exactly what the recorded sequence a caller` |
|        - |  882 | `			 * asked for was produced with. */` |
|       65 |  883 | `			double rNum = (double)(PH7_VmMtRand(pCtx->pVm) >> 1);` |
|       65 |  884 | `			double rSpan = (double)iMax - (double)iMin + 1.0;` |
|        - |  885 | `			/* php's own arithmetic, types included: the product lands in an` |
|        - |  886 | `			 * unsigned 64-bit result, so a span wider than the SIGNED range keeps` |
|        - |  887 | `			 * its value and wraps around the minimum rather than saturating. The` |
|        - |  888 | `			 * product is never negative (the span is at least 1, the fraction at` |
|        - |  889 | `			 * least 0), so the unsigned conversion is total. */` |
|       65 |  890 | `			sxu64 uOut = (sxu64)iMin + (sxu64)(rSpan * (rNum / (2147483647.0 + 1.0)));` |
|       65 |  891 | `			ph7_result_int64(pCtx,(sxi64)uOut);` |
|       65 |  892 | `			return SXRET_OK;` |
|        - |  893 | `		}` |
|        - |  894 | `		/* MT19937-backed uniform draw over [iMin,iMax], bit-for-bit as php. */` |
|     1708 |  895 | `		ph7_result_int64(pCtx,PH7_VmMtRandRange(pCtx->pVm,iMin,iMax));` |
|     1708 |  896 | `		return SXRET_OK;` |
|        - |  897 | `	}` |
|        - |  898 | `	/* No-argument form: a 31-bit value in [0, mt_getrandmax()]. php returns` |
|        - |  899 | `	 * php_mt_rand() >> 1 for the bare draw (the full 32-bit word feeds the` |
|        - |  900 | `	 * range form above, but the bare form drops the low bit). */` |
|      114 |  901 | `	ph7_result_int64(pCtx,(sxi64)(PH7_VmMtRand(pCtx->pVm) >> 1));` |
|      114 |  902 | `	return SXRET_OK;` |
|      949 |  903 | `}` |
|        - |  904 | `/*` |
|        - |  905 | ` * int getrandmax(void)` |
|        - |  906 | ` * int mt_getrandmax(void)` |
|        - |  907 | ` * int rc4_getrandmax(void)` |
|        - |  908 | ` *   Show largest possible random value` |
|        - |  909 | ` * Return` |
|        - |  910 | ` *  The largest possible random value returned by rand()/mt_rand(): php's` |
|        - |  911 | ` *  MT19937 backing makes this 2^31-1 (2147483647) for both.` |
|        - |  912 | ` */` |
|        8 |  913 | `PH7_PRIVATE int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  914 | `{` |
|        4 |  915 | `	SXUNUSED(nArg); /* cc warning */` |
|        4 |  916 | `	SXUNUSED(apArg);` |
|        - |  917 | `	/* php: PHP_MT_RAND_MAX == (1<<31)-1; bare rand()/mt_rand() draw >> 1 lands` |
|        - |  918 | `	 * exactly in [0, this]. */` |
|        9 |  919 | `	ph7_result_int64(pCtx,2147483647);` |
|        9 |  920 | `	return SXRET_OK;` |
|        1 |  921 | `}` |
|        - |  922 | `/*` |
|        - |  923 | ` * string rand_str()` |
|        - |  924 | ` * string rand_str(int $len)` |
|        - |  925 | ` *  Generate a random string (English alphabet).` |
|        - |  926 | ` * Parameter` |
|        - |  927 | ` *  $len` |
|        - |  928 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  929 | ` * Return` |
|        - |  930 | ` *   A pseudo random string.` |
|        - |  931 | ` * Note:` |
|        - |  932 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  933 | ` *  by te SQLite3 library.` |
|        - |  934 | ` *  This function is a symisc extension.` |
|        - |  935 | ` */` |
|      552 |  936 | `PH7_PRIVATE int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  937 | `{` |
|        - |  938 | `	char zString[1024];` |
|      557 |  939 | `	int iLen = 0x10;` |
|      557 |  940 | `	if( nArg > 0 ){` |
|        - |  941 | `		/* Get the desired length */` |
|      557 |  942 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      557 |  943 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  944 | `			/* Default length */` |
|        3 |  945 | `			iLen = 0x10;` |
|        1 |  946 | `		}` |
|      276 |  947 | `	}` |
|        - |  948 | `	/* Generate the random string */` |
|      557 |  949 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  950 | `	/* Return the generated string */` |
|      557 |  951 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      557 |  952 | `	return SXRET_OK;` |
|        5 |  953 | `}` |
|        - |  954 | `/*` |
|        - |  955 | ` * Reject non-numeric values (array/object/resource and non-numeric strings)` |
|        - |  956 | ` * the same way intdiv() does. Returns SXRET_OK if the value is acceptable as` |
|        - |  957 | ` * an int (PHP coerces float and numeric string silently).` |
|        - |  958 | ` */` |
|      476 |  959 | `static int VmRandomCheckIntArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgPos,const char *zParamName)` |
|        1 |  960 | `{` |
|      476 |  961 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg)` |
|      477 |  962 | `		\|\| ph7_value_is_resource(pArg) ){` |
|      ! 0 |  963 | `		return PH7_VmThrowException(pCtx,` |
|        - |  964 | `			"TypeError",` |
|        - |  965 | `			"%s(): Argument #%d (%s) must be of type int, %s given",` |
|      ! 0 |  966 | `			zFunc,iArgPos,zParamName,` |
|      ! 0 |  967 | `			ph7_type_name(pArg)` |
|        - |  968 | `			);` |
|        - |  969 | `	}` |
|      477 |  970 | `	if( ph7_value_is_string(pArg) ){` |
|        - |  971 | `		int len;` |
|        5 |  972 | `		const char *zStr = ph7_value_to_string(pArg, &len);` |
|        5 |  973 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|      ! 0 |  974 | `			return PH7_VmThrowException(pCtx,` |
|        - |  975 | `				"TypeError",` |
|        - |  976 | `				"%s(): Argument #%d (%s) must be of type int, string given",` |
|      ! 0 |  977 | `				zFunc,iArgPos,zParamName` |
|        - |  978 | `				);` |
|        - |  979 | `		}` |
|        2 |  980 | `	}` |
|      477 |  981 | `	return SXRET_OK;` |
|      239 |  982 | `}` |
|        - |  983 | `/*` |
|        - |  984 | ` * int random_int(int $min, int $max)` |
|        - |  985 | ` *  Generate a cryptographically secure pseudo-random integer in [$min, $max].` |
|        - |  986 | ` *  Mirrors PHP 7.0+ random_int(). Uses the OS CSPRNG via SyOSCSPRNG().` |
|        - |  987 | ` *  Distribution is uniform via rejection sampling against the smallest` |
|        - |  988 | ` *  power-of-two mask covering the range.` |
|        - |  989 | ` */` |
|      230 |  990 | `PH7_PRIVATE int vm_builtin_random_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  991 | `{` |
|        - |  992 | `	sxi64 iMin,iMax;` |
|        - |  993 | `	sxu64 uRange,uMask,uResult;` |
|        - |  994 | `	unsigned int nAttempt;` |
|        - |  995 | `	int rc;` |
|      231 |  996 | `	if( nArg != 2 ){` |
|      ! 0 |  997 | `		return PH7_VmThrowException(pCtx,` |
|        - |  998 | `			"ArgumentCountError",` |
|        - |  999 | `			"random_int() expects exactly 2 arguments, %d given",` |
|      ! 0 | 1000 | `			nArg` |
|        - | 1001 | `			);` |
|        - | 1002 | `	}` |
|      231 | 1003 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_int",1,"$min");` |
|      231 | 1004 | `	if( rc != SXRET_OK ){ return rc; }` |
|      231 | 1005 | `	rc = VmRandomCheckIntArg(pCtx,apArg[1],"random_int",2,"$max");` |
|      231 | 1006 | `	if( rc != SXRET_OK ){ return rc; }` |
|      231 | 1007 | `	iMin = ph7_value_to_int64(apArg[0]);` |
|      231 | 1008 | `	iMax = ph7_value_to_int64(apArg[1]);` |
|      231 | 1009 | `	if( iMin > iMax ){` |
|        3 | 1010 | `		return PH7_VmThrowException(pCtx,` |
|        - | 1011 | `			"ValueError",` |
|        - | 1012 | `			"random_int(): Argument #1 ($min) must be less than or equal to argument #2 ($max)"` |
|        - | 1013 | `			);` |
|        - | 1014 | `	}` |
|      229 | 1015 | `	if( iMin == iMax ){` |
|        5 | 1016 | `		ph7_result_int64(pCtx,iMin);` |
|        5 | 1017 | `		return SXRET_OK;` |
|        - | 1018 | `	}` |
|      225 | 1019 | `	uRange = (sxu64)iMax - (sxu64)iMin;` |
|      225 | 1020 | `	uMask = uRange;` |
|      225 | 1021 | `	uMask \|= uMask >> 1;` |
|      225 | 1022 | `	uMask \|= uMask >> 2;` |
|      225 | 1023 | `	uMask \|= uMask >> 4;` |
|      225 | 1024 | `	uMask \|= uMask >> 8;` |
|      225 | 1025 | `	uMask \|= uMask >> 16;` |
|      225 | 1026 | `	uMask \|= uMask >> 32;` |
|      225 | 1027 | `	uResult = 0;` |
|      374 | 1028 | `	for( nAttempt = 0 ; nAttempt < 50 ; ++nAttempt ){` |
|        - | 1029 | `		/* Always draw a full 8 bytes so endianness of the cast doesn't matter` |
|        - | 1030 | `		 * (a 4-byte fill into a sxu64 would land in the high half on big-endian` |
|        - | 1031 | `		 * and the low-half mask would always read 0). */` |
|        - | 1032 | `		sxu64 uDraw;` |
|      374 | 1033 | `		if( SyOSCSPRNG(&uDraw,sizeof(uDraw)) != SXRET_OK ){` |
|      ! 0 | 1034 | `			return PH7_VmThrowException(pCtx,` |
|        - | 1035 | `				"Random\\RandomException",` |
|        - | 1036 | `				"Cannot gather sufficient random data"` |
|        - | 1037 | `				);` |
|        - | 1038 | `		}` |
|      374 | 1039 | `		uDraw &= uMask;` |
|      374 | 1040 | `		if( uDraw <= uRange ){` |
|      225 | 1041 | `			uResult = uDraw;` |
|      225 | 1042 | `			break;` |
|        - | 1043 | `		}` |
|       66 | 1044 | `	}` |
|      225 | 1045 | `	if( nAttempt >= 50 ){` |
|      ! 0 | 1046 | `		return PH7_VmThrowException(pCtx,` |
|        - | 1047 | `			"Random\\RandomException",` |
|        - | 1048 | `			"Cannot gather sufficient random data"` |
|        - | 1049 | `			);` |
|        - | 1050 | `	}` |
|      225 | 1051 | `	ph7_result_int64(pCtx,(sxi64)((sxu64)iMin + uResult));` |
|      225 | 1052 | `	return SXRET_OK;` |
|      116 | 1053 | `}` |
|        - | 1054 | `/*` |
|        - | 1055 | ` * string random_bytes(int $length)` |
|        - | 1056 | ` *  Generate $length cryptographically secure random bytes via SyOSCSPRNG().` |
|        - | 1057 | ` *  Mirrors PHP 7.0+ random_bytes().` |
|        - | 1058 | ` */` |
|       16 | 1059 | `PH7_PRIVATE int vm_builtin_random_bytes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1060 | `{` |
|        - | 1061 | `	sxi64 iLen;` |
|        - | 1062 | `	unsigned char zStack[256];` |
|        - | 1063 | `	void *pBuf;` |
|        - | 1064 | `	int rc;` |
|       17 | 1065 | `	int bHeap = 0;` |
|       17 | 1066 | `	if( nArg != 1 ){` |
|      ! 0 | 1067 | `		return PH7_VmThrowException(pCtx,` |
|        - | 1068 | `			"ArgumentCountError",` |
|        - | 1069 | `			"random_bytes() expects exactly 1 argument, %d given",` |
|      ! 0 | 1070 | `			nArg` |
|        - | 1071 | `			);` |
|        - | 1072 | `	}` |
|       17 | 1073 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_bytes",1,"$length");` |
|       17 | 1074 | `	if( rc != SXRET_OK ){ return rc; }` |
|       17 | 1075 | `	iLen = ph7_value_to_int64(apArg[0]);` |
|       17 | 1076 | `	if( iLen < 1 ){` |
|        5 | 1077 | `		return PH7_VmThrowException(pCtx,` |
|        - | 1078 | `			"ValueError",` |
|        - | 1079 | `			"random_bytes(): Argument #1 ($length) must be greater than 0"` |
|        - | 1080 | `			);` |
|        - | 1081 | `	}` |
|        - | 1082 | `	/* The PH7 allocator and ph7_result_string both take sxu32/int sizes,` |
|        - | 1083 | `	 * so we can't honor a length above 2 GiB. Reject early rather than` |
|        - | 1084 | `	 * silently truncating via the (sxu32) cast below. */` |
|       13 | 1085 | `	if( iLen > 0x7FFFFFFF ){` |
|      ! 0 | 1086 | `		return PH7_VmThrowException(pCtx,` |
|        - | 1087 | `			"ValueError",` |
|        - | 1088 | `			"random_bytes(): Argument #1 ($length) is too large"` |
|        - | 1089 | `			);` |
|        - | 1090 | `	}` |
|       13 | 1091 | `	if( iLen <= (sxi64)sizeof(zStack) ){` |
|       13 | 1092 | `		pBuf = zStack;` |
|        7 | 1093 | `	}else{` |
|      ! 0 | 1094 | `		pBuf = SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)iLen);` |
|      ! 0 | 1095 | `		if( pBuf == 0 ){` |
|      ! 0 | 1096 | `			return PH7_VmThrowException(pCtx,` |
|        - | 1097 | `				"Exception",` |
|        - | 1098 | `				"random_bytes(): Failed to allocate %qd bytes",` |
|      ! 0 | 1099 | `				iLen` |
|        - | 1100 | `				);` |
|        - | 1101 | `		}` |
|      ! 0 | 1102 | `		bHeap = 1;` |
|        - | 1103 | `	}` |
|       13 | 1104 | `	if( SyOSCSPRNG(pBuf,(sxu32)iLen) != SXRET_OK ){` |
|      ! 0 | 1105 | `		if( bHeap ){` |
|      ! 0 | 1106 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 1107 | `		}` |
|      ! 0 | 1108 | `		return PH7_VmThrowException(pCtx,` |
|        - | 1109 | `			"Random\\RandomException",` |
|        - | 1110 | `			"Cannot gather sufficient random data"` |
|        - | 1111 | `			);` |
|        - | 1112 | `	}` |
|       13 | 1113 | `	ph7_result_string(pCtx,(const char *)pBuf,(int)iLen);` |
|       13 | 1114 | `	if( bHeap ){` |
|      ! 0 | 1115 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 1116 | `	}` |
|       13 | 1117 | `	return SXRET_OK;` |
|        9 | 1118 | `}` |
|        - | 1119 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 1120 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 1121 | `/* Unique ID private data */` |
|        - | 1122 | `struct unique_id_data` |
|        - | 1123 | `{` |
|        - | 1124 | `	ph7_context *pCtx; /* Call context */` |
|        - | 1125 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - | 1126 | `};` |
|        - | 1127 | `/*` |
|        - | 1128 | ` * Binary to hex consumer callback.` |
|        - | 1129 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - | 1130 | ` * defined below.` |
|        - | 1131 | ` */` |
|      192 | 1132 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 | 1133 | `{` |
|      193 | 1134 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - | 1135 | `	sxu32 nBuflen;` |
|        - | 1136 | `	/* Extract result buffer length */` |
|      193 | 1137 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 | 1138 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - | 1139 | `			/*` |
|        - | 1140 | `			 * If the more_entropy flag is not set,then the returned` |
|        - | 1141 | `			 * string will be 13 characters long` |
|        - | 1142 | `			 */` |
|       25 | 1143 | `		return SXERR_ABORT;` |
|        - | 1144 | `	}` |
|      169 | 1145 | `	if( nBuflen > 22 ){` |
|      ! 0 | 1146 | `		return SXERR_ABORT;` |
|        - | 1147 | `	}` |
|        - | 1148 | `	/* Safely Consume the hex stream */` |
|      169 | 1149 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 | 1150 | `	return SXRET_OK;` |
|       97 | 1151 | `}` |
|        - | 1152 | `/*` |
|        - | 1153 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - | 1154 | ` *  Generate a unique ID` |
|        - | 1155 | ` * Parameter` |
|        - | 1156 | ` * $prefix` |
|        - | 1157 | ` *  Append this prefix to the generated unique ID.` |
|        - | 1158 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - | 1159 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - | 1160 | ` * $more_entropy` |
|        - | 1161 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - | 1162 | ` *  that the result will be unique.` |
|        - | 1163 | ` * Return` |
|        - | 1164 | ` *  Returns the unique identifier, as a string.` |
|        - | 1165 | ` */` |
|       24 | 1166 | `PH7_PRIVATE int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1167 | `{` |
|        - | 1168 | `	struct unique_id_data sUniq;` |
|        - | 1169 | `	unsigned char zDigest[20];` |
|       25 | 1170 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 1171 | `	const char *zPrefix;` |
|        - | 1172 | `	SHA1Context sCtx;` |
|        - | 1173 | `	char zRandom[7];` |
|        - | 1174 | `	int nPrefix;` |
|        - | 1175 | `	int entropy;` |
|        - | 1176 | `	/* Generate a random string first */` |
|       25 | 1177 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - | 1178 | `	/* Initialize fields */` |
|       25 | 1179 | `	zPrefix = 0;` |
|       25 | 1180 | `	nPrefix = 0;` |
|       25 | 1181 | `	entropy = 0;` |
|       25 | 1182 | `	if( nArg > 0 ){` |
|        - | 1183 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 | 1184 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 | 1185 | `		if( nArg > 1 ){` |
|      ! 0 | 1186 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 1187 | `		}` |
|      ! 0 | 1188 | `	}` |
|       25 | 1189 | `	SHA1Init(&sCtx);` |
|        - | 1190 | `	/* Generate the random ID */` |
|       25 | 1191 | `	if( nPrefix > 0 ){` |
|      ! 0 | 1192 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 | 1193 | `	}` |
|        - | 1194 | `	/* Append the random ID */` |
|       25 | 1195 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - | 1196 | `	/* Append the random string */` |
|       25 | 1197 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - | 1198 | `	/* Increment the number */` |
|       25 | 1199 | `	pVm->unique_id++;` |
|       25 | 1200 | `	SHA1Final(&sCtx,zDigest);` |
|        - | 1201 | `	/* Hexify the digest */` |
|       25 | 1202 | `	sUniq.pCtx = pCtx;` |
|       25 | 1203 | `	sUniq.entropy = entropy;` |
|       25 | 1204 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - | 1205 | `	/* All done */` |
|       25 | 1206 | `	return PH7_OK;` |
|        1 | 1207 | `}` |
|        - | 1208 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 1209 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 1210 | `/*` |
|        - | 1211 | ` * Section:` |
|        - | 1212 | ` *  Language construct implementation as foreign functions.` |
|        - | 1213 | ` * Status:` |
|        - | 1214 | ` *    Stable.` |
|        - | 1215 | ` */` |
|        - | 1216 | `/*` |
|        - | 1217 | ` * The user-visible string coercion an OUTPUT construct performs on one of its` |
|        - | 1218 | ` * arguments (echo/print reached as host functions rather than as OP_CONSUME).` |
|        - | 1219 | ` * An ARRAY warns and still renders as "Array"; an object whose class has no` |
|        - | 1220 | ` * __toString() is php's catchable "could not be converted to string" Error,` |
|        - | 1221 | ` * and the construct outputs nothing for it. The status is recorded on the` |
|        - | 1222 | ` * context too, so OP_CALL cannot treat the throwing call as a normal return.` |
|        - | 1223 | ` */` |
|       46 | 1224 | `static sxi32 VmOutputArgToString(ph7_context *pCtx,ph7_value *pArg,const char **pzData,int *pnLen)` |
|        5 | 1225 | `{` |
|       51 | 1226 | `	sxi32 rc = PH7_MemObjToStringUV(pArg);` |
|       51 | 1227 | `	if( rc != SXRET_OK ){` |
|        3 | 1228 | `		pCtx->nThrowRc = rc;` |
|        3 | 1229 | `		return rc;` |
|        - | 1230 | `	}` |
|       48 | 1231 | `	*pzData = ph7_value_to_string(pArg,pnLen);` |
|       48 | 1232 | `	return SXRET_OK;` |
|       28 | 1233 | `}` |
|        - | 1234 | `/*` |
|        - | 1235 | ` * void echo($string...)` |
|        - | 1236 | ` *  Output one or more messages.` |
|        - | 1237 | ` * Parameters` |
|        - | 1238 | ` *  $string` |
|        - | 1239 | ` *   Message to output.` |
|        - | 1240 | ` * Return` |
|        - | 1241 | ` *  NULL.` |
|        - | 1242 | ` */` |
|      ! 0 | 1243 | `PH7_PRIVATE int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 1244 | `{` |
|        - | 1245 | `	const char *zData;` |
|      ! 0 | 1246 | `	int nDataLen = 0;` |
|        - | 1247 | `	ph7_vm *pVm;` |
|        - | 1248 | `	int i,rc;` |
|        - | 1249 | `	/* Point to the target VM */` |
|      ! 0 | 1250 | `	pVm = pCtx->pVm;` |
|        - | 1251 | `	/* Output */` |
|      ! 0 | 1252 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 | 1253 | `		sxi32 rcSv = VmOutputArgToString(pCtx,apArg[i],&zData,&nDataLen);` |
|      ! 0 | 1254 | `		if( rcSv != SXRET_OK ){` |
|      ! 0 | 1255 | `			return rcSv;` |
|        - | 1256 | `		}` |
|      ! 0 | 1257 | `		if( nDataLen > 0 ){` |
|      ! 0 | 1258 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 | 1259 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 | 1260 | `			if( rc == SXERR_ABORT ){` |
|        - | 1261 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 1262 | `				return PH7_ABORT;` |
|        - | 1263 | `			}` |
|      ! 0 | 1264 | `		}` |
|      ! 0 | 1265 | `	}` |
|      ! 0 | 1266 | `	return SXRET_OK;` |
|      ! 0 | 1267 | `}` |
|        - | 1268 | `/*` |
|        - | 1269 | ` * int print($string...)` |
|        - | 1270 | ` *  Output one or more messages.` |
|        - | 1271 | ` * Parameters` |
|        - | 1272 | ` *  $string` |
|        - | 1273 | ` *   Message to output.` |
|        - | 1274 | ` * Return` |
|        - | 1275 | ` *  1 always.` |
|        - | 1276 | ` */` |
|       46 | 1277 | `PH7_PRIVATE int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 1278 | `{` |
|        - | 1279 | `	const char *zData;` |
|       51 | 1280 | `	int nDataLen = 0;` |
|        - | 1281 | `	ph7_vm *pVm;` |
|        - | 1282 | `	int i,rc;` |
|        - | 1283 | `	/* Point to the target VM */` |
|       51 | 1284 | `	pVm = pCtx->pVm;` |
|        - | 1285 | `	/* Output */` |
|       95 | 1286 | `	for( i = 0 ; i < nArg ; ++i ){` |
|       51 | 1287 | `		sxi32 rcSv = VmOutputArgToString(pCtx,apArg[i],&zData,&nDataLen);` |
|       51 | 1288 | `		if( rcSv != SXRET_OK ){` |
|        3 | 1289 | `			return rcSv;` |
|        - | 1290 | `		}` |
|       48 | 1291 | `		if( nDataLen > 0 ){` |
|       48 | 1292 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|       48 | 1293 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|       48 | 1294 | `			if( rc == SXERR_ABORT ){` |
|        - | 1295 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 1296 | `				return PH7_ABORT;` |
|        - | 1297 | `			}` |
|       22 | 1298 | `		}` |
|       26 | 1299 | `	}` |
|        - | 1300 | `	/* Return 1 */` |
|       48 | 1301 | `	ph7_result_int(pCtx,1);` |
|       48 | 1302 | `	return SXRET_OK;` |
|       28 | 1303 | `}` |
|        - | 1304 | `/*` |
|        - | 1305 | ` * void exit(string $msg)` |
|        - | 1306 | ` * void exit(int $status)` |
|        - | 1307 | ` * void die(string $ms)` |
|        - | 1308 | ` * void die(int $status)` |
|        - | 1309 | ` *   Output a message and terminate program execution.` |
|        - | 1310 | ` * Parameter` |
|        - | 1311 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - | 1312 | ` *  If status is an integer, that value will be used as the exit status` |
|        - | 1313 | ` *  and not printed` |
|        - | 1314 | ` * Return` |
|        - | 1315 | ` *  NULL` |
|        - | 1316 | ` */` |
|      ! 0 | 1317 | `PH7_PRIVATE int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 1318 | `{` |
|      ! 0 | 1319 | `	if( nArg > 0 ){` |
|      ! 0 | 1320 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - | 1321 | `			const char *zData;` |
|      ! 0 | 1322 | `			int iLen = 0;` |
|        - | 1323 | `			/* Print exit message */` |
|      ! 0 | 1324 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 | 1325 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 | 1326 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - | 1327 | `			sxi32 iExitStatus;` |
|        - | 1328 | `			/* Record exit status code */` |
|      ! 0 | 1329 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 | 1330 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 | 1331 | `		}` |
|      ! 0 | 1332 | `	}` |
|        - | 1333 | `	/* Request a VM-wide halt (see PH7_OP_HALT) and abort processing` |
|        - | 1334 | `	 * immediately; the abort unwinds enclosing frames and execution units.` |
|        - | 1335 | `	 */` |
|      ! 0 | 1336 | `	pCtx->pVm->bHaltRequested = 1;` |
|      ! 0 | 1337 | `	return PH7_ABORT;` |
|      ! 0 | 1338 | `}` |
|        - | 1339 | `/*` |
|        - | 1340 | ` * Section:` |
|        - | 1341 | ` *  Version,Credits and Copyright related functions.` |
|        - | 1342 | ` * Status:` |
|        - | 1343 | ` *    Stable.` |
|        - | 1344 | ` */` |
|        - | 1345 | `/*` |
|        - | 1346 | ` * string ph7version(void)` |
|        - | 1347 | ` *  Returns the running version of the PH7 version.` |
|        - | 1348 | ` * Parameters` |
|        - | 1349 | ` *  None` |
|        - | 1350 | ` * Return` |
|        - | 1351 | ` * Current PH7 version.` |
|        - | 1352 | ` */` |
|        2 | 1353 | `PH7_PRIVATE int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1354 | `{` |
|        1 | 1355 | `	SXUNUSED(nArg);` |
|        1 | 1356 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 1357 | `	/* Current engine version */` |
|        3 | 1358 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 1359 | `	return PH7_OK;` |
|        1 | 1360 | `}` |
|        - | 1361 | `/*` |
|        - | 1362 | ` * string\|false phpversion([ ?string $extension = null ])` |
|        - | 1363 | ` *  Returns the PHP-compatibility version PHL advertises (see PHP_COMPAT_VERSION).` |
|        - | 1364 | ` * Parameters` |
|        - | 1365 | ` *  $extension (optional): an extension name, matched case-insensitively against` |
|        - | 1366 | ` *  the ones this engine reports as loaded.` |
|        - | 1367 | ` * Return` |
|        - | 1368 | ` *  The version string — for the engine with no argument (or an explicit NULL),` |
|        - | 1369 | ` *  and for a loaded extension, whose version IS the engine's since every one of` |
|        - | 1370 | ` *  them is part of it — or FALSE for a name it does not report.` |
|        - | 1371 | ` */` |
|        - | 1372 | `static int VmExtensionIsLoaded(ph7_context *pCtx,ph7_value *pName);` |
|      170 | 1373 | `PH7_PRIVATE int vm_builtin_phpversion(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 1374 | `{` |
|        - | 1375 | `	/* $extension was declared in the signature and answered NULL for everything:` |
|        - | 1376 | `	 * an unknown one where php answers FALSE (so the documented` |
|        - | 1377 | ``	 * `if (phpversion($e) === false)` check never fired and a version comparison`` |
|        - | 1378 | `	 * ran against NULL), a KNOWN one where php answers the version string, and` |
|        - | 1379 | `	 * even the explicit NULL that means "no extension" at all.` |
|        - | 1380 | `	 *` |
|        - | 1381 | `	 * Every extension this engine reports as loaded is part of the engine, so its` |
|        - | 1382 | `	 * version IS the engine's — which is also what php answers for its own` |
|        - | 1383 | `	 * bundled ones — and a name it does not report is php's false. */` |
|      172 | 1384 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0 ){` |
|      163 | 1385 | `		if( !VmExtensionIsLoaded(pCtx,apArg[0]) ){` |
|       15 | 1386 | `			ph7_result_bool(pCtx,0);` |
|       15 | 1387 | `			return PH7_OK;` |
|        - | 1388 | `		}` |
|       74 | 1389 | `	}` |
|      158 | 1390 | `	ph7_result_string(pCtx,PHP_COMPAT_VERSION,(int)sizeof(PHP_COMPAT_VERSION) - 1);` |
|      158 | 1391 | `	return PH7_OK;` |
|       87 | 1392 | `}` |
|        - | 1393 | `/*` |
|        - | 1394 | ` * The extensions PHL reports as loaded, in the order get_loaded_extensions()` |
|        - | 1395 | `` * lists them and with the CASE php uses for each. `extension_loaded()` matches`` |
|        - | 1396 | ` * case-INSENSITIVELY, which is why one table serves both.` |
|        - | 1397 | ` */` |
|        - | 1398 | `static const char * const azExtension[] = {` |
|        - | 1399 | `	"Core", "date", "pcre", "SPL", "json", "standard", "bcmath", "calendar",` |
|        - | 1400 | `	"ctype", "filter", "hash", "Reflection", "session", "mbstring", "iconv",` |
|        - | 1401 | `	/* php 8.2 moved rand/mt_rand/random_int/random_bytes into ext/random and` |
|        - | 1402 | `	 * gave them the Randomizer surface; this build has both halves now, and` |
|        - | 1403 | `	 * the name was missing while eight of its functions were already here. */` |
|        - | 1404 | `	"random"` |
|        - | 1405 | `#ifdef PH7_ENABLE_LIBXML` |
|        - | 1406 | `	, "libxml", "xml", "dom", "xmlwriter"` |
|        - | 1407 | `#endif` |
|        - | 1408 | `#ifdef PH7_ENABLE_SQLITE` |
|        - | 1409 | `	/* ext/sqlite3 (the SQLite3 class family) is NOT one of these: §10 scopes` |
|        - | 1410 | `	 * this build to PDO's sqlite DRIVER, so only the two pdo names load. */` |
|        - | 1411 | `	, "PDO", "pdo_sqlite"` |
|        - | 1412 | `#endif` |
|        - | 1413 | `#ifdef PH7_ENABLE_CURL` |
|        - | 1414 | `	, "curl"` |
|        - | 1415 | `#endif` |
|        - | 1416 | `};` |
|        - | 1417 | `/*` |
|        - | 1418 | `` * `phl.stub_extensions` is a PHL-only directive: a comma-separated list of`` |
|        - | 1419 | ` * extensions PHL does NOT implement but reports as LOADED, so software that` |
|        - | 1420 | ` * only GATES on extension_loaded() (PHPUnit's dom/xmlwriter check) runs` |
|        - | 1421 | ` * unmodified. It synthesizes nothing -- no class, no function.` |
|        - | 1422 | ` *` |
|        - | 1423 | ` * Walk it, handing each trimmed name to xVisit until one answers non-zero.` |
|        - | 1424 | ` */` |
|       42 | 1425 | `static int VmStubExtWalk(ph7_vm *pVm,int (*xVisit)(const char *,int,void *),void *pData)` |
|        3 | 1426 | `{` |
|        - | 1427 | `	SyBlob sList;` |
|        - | 1428 | `	const char *z;` |
|       45 | 1429 | `	int nByte,i = 0,rc = 0;` |
|       45 | 1430 | `	SyBlobInit(&sList,&pVm->sAllocator);` |
|       45 | 1431 | `	PH7_VmIniGetStr(pVm,"phl.stub_extensions",&sList);` |
|       45 | 1432 | `	z = (const char *)SyBlobData(&sList);` |
|       45 | 1433 | `	nByte = (int)SyBlobLength(&sList);` |
|       63 | 1434 | `	while( rc == 0 && i < nByte ){` |
|        - | 1435 | `		int iStart,iEnd;` |
|       27 | 1436 | `		while( i < nByte && z[i] == ',' ){ i++; }` |
|       19 | 1437 | `		iStart = i;` |
|      223 | 1438 | `		while( i < nByte && z[i] != ',' ){ i++; }` |
|       19 | 1439 | `		iEnd = i;` |
|       36 | 1440 | `		while( iStart < iEnd && (z[iStart] == ' ' \|\| z[iStart] == '\t') ){ iStart++; }` |
|       28 | 1441 | `		while( iEnd > iStart && (z[iEnd-1] == ' ' \|\| z[iEnd-1] == '\t') ){ iEnd--; }` |
|       19 | 1442 | `		if( iEnd > iStart ){` |
|       19 | 1443 | `			rc = xVisit(&z[iStart],iEnd - iStart,pData);` |
|        9 | 1444 | `		}` |
|        1 | 1445 | `	}` |
|       45 | 1446 | `	SyBlobRelease(&sList);` |
|       45 | 1447 | `	return rc;` |
|        3 | 1448 | `}` |
|        - | 1449 | `typedef struct vm_ext_match vm_ext_match;` |
|        - | 1450 | `struct vm_ext_match {` |
|        - | 1451 | `	const char *zName;` |
|        - | 1452 | `	int nName;` |
|        - | 1453 | `};` |
|       14 | 1454 | `static int VmStubExtMatch(const char *zName,int nName,void *pData)` |
|        1 | 1455 | `{` |
|       15 | 1456 | `	vm_ext_match *p = (vm_ext_match *)pData;` |
|       15 | 1457 | `	return nName == p->nName && SyStrnicmp(zName,p->zName,(sxu32)nName) == 0;` |
|        1 | 1458 | `}` |
|        - | 1459 | `/*` |
|        - | 1460 | ` * Is this name one of the extensions this engine reports as loaded? Shared by` |
|        - | 1461 | ` * extension_loaded() and phpversion(), which php answers from the same list.` |
|        - | 1462 | ` */` |
|      348 | 1463 | `static int VmExtensionIsLoaded(ph7_context *pCtx,ph7_value *pName)` |
|        5 | 1464 | `{` |
|        - | 1465 | `	vm_ext_match sMatch;` |
|        - | 1466 | `	const char *zName;` |
|        - | 1467 | `	int nName;` |
|        - | 1468 | `	sxu32 n;` |
|      353 | 1469 | `	zName = ph7_value_to_string(pName,&nName);` |
|     4701 | 1470 | `	for( n = 0 ; n < SX_ARRAYSIZE(azExtension) ; ++n ){` |
|     4666 | 1471 | `		if( nName == (int)SyStrlen(azExtension[n])` |
|     2831 | 1472 | `		 && SyStrnicmp(zName,azExtension[n],(sxu32)nName) == 0 ){` |
|      323 | 1473 | `			return 1;` |
|        - | 1474 | `		}` |
|     2179 | 1475 | `	}` |
|       33 | 1476 | `	sMatch.zName = zName;` |
|       33 | 1477 | `	sMatch.nName = nName;` |
|       33 | 1478 | `	return VmStubExtWalk(pCtx->pVm,VmStubExtMatch,&sMatch);` |
|      179 | 1479 | `}` |
|        - | 1480 | `/*` |
|        - | 1481 | ` * bool extension_loaded(string $extension)` |
|        - | 1482 | ` *  php matches the name case-insensitively.` |
|        - | 1483 | ` */` |
|      186 | 1484 | `PH7_PRIVATE int vm_builtin_extension_loaded(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 1485 | `{` |
|      191 | 1486 | `	if( nArg < 1 ){` |
|      ! 0 | 1487 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 1488 | `		return PH7_OK;` |
|        - | 1489 | `	}` |
|      191 | 1490 | `	ph7_result_bool(pCtx,VmExtensionIsLoaded(pCtx,apArg[0]));` |
|      191 | 1491 | `	return PH7_OK;` |
|       98 | 1492 | `}` |
|        4 | 1493 | `static int VmStubExtCollect(const char *zName,int nName,void *pData)` |
|        1 | 1494 | `{` |
|        5 | 1495 | `	ph7_context *pCtx = (ph7_context *)((void **)pData)[0];` |
|        5 | 1496 | `	ph7_value *pArray = (ph7_value *)((void **)pData)[1];` |
|        5 | 1497 | `	ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|        5 | 1498 | `	if( pVal ){` |
|        5 | 1499 | `		ph7_value_string(pVal,zName,nName);` |
|        5 | 1500 | `		ph7_array_add_elem(pArray,0,pVal);` |
|        5 | 1501 | `		ph7_context_release_value(pCtx,pVal);` |
|        2 | 1502 | `	}` |
|        5 | 1503 | `	return 0;` |
|        1 | 1504 | `}` |
|        - | 1505 | `/*` |
|        - | 1506 | ` * array get_loaded_extensions(bool $zend_extensions = false)` |
|        - | 1507 | ` *  PHL loads no Zend extension, so the zend list is always empty.` |
|        - | 1508 | ` */` |
|       12 | 1509 | `PH7_PRIVATE int vm_builtin_get_loaded_extensions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 | 1510 | `{` |
|       15 | 1511 | `	ph7_value *pArray = ph7_context_new_array(pCtx);` |
|        - | 1512 | `	void *apData[2];` |
|        - | 1513 | `	sxu32 n;` |
|       15 | 1514 | `	if( pArray == 0 ){` |
|      ! 0 | 1515 | `		return PH7_ContextMemoryError(pCtx);` |
|        - | 1516 | `	}` |
|       15 | 1517 | `	if( nArg > 0 && ph7_value_to_bool(apArg[0]) ){` |
|      ! 0 | 1518 | `		ph7_result_value(pCtx,pArray);` |
|      ! 0 | 1519 | `		return PH7_OK;` |
|        - | 1520 | `	}` |
|      291 | 1521 | `	for( n = 0 ; n < SX_ARRAYSIZE(azExtension) ; ++n ){` |
|      279 | 1522 | `		ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|      279 | 1523 | `		if( pVal ){` |
|      279 | 1524 | `			ph7_value_string(pVal,azExtension[n],-1);` |
|      279 | 1525 | `			ph7_array_add_elem(pArray,0,pVal);` |
|      279 | 1526 | `			ph7_context_release_value(pCtx,pVal);` |
|      138 | 1527 | `		}` |
|      141 | 1528 | `	}` |
|       15 | 1529 | `	apData[0] = pCtx;` |
|       15 | 1530 | `	apData[1] = pArray;` |
|       15 | 1531 | `	VmStubExtWalk(pCtx->pVm,VmStubExtCollect,apData);` |
|       15 | 1532 | `	ph7_result_value(pCtx,pArray);` |
|       15 | 1533 | `	return PH7_OK;` |
|        9 | 1534 | `}` |
|        - | 1535 | `/*` |
|        - | 1536 | ` * string php_sapi_name(void)` |
|        - | 1537 | ` *  Returns the type of interface (SAPI) PHL is running under.` |
|        - | 1538 | ` * Parameters` |
|        - | 1539 | ` *  None` |
|        - | 1540 | ` * Return` |
|        - | 1541 | ` *  "cli-server" while serving an HTTP request via the built-in -S server, "cli" otherwise.` |
|        - | 1542 | ` */` |
|        2 | 1543 | `PH7_PRIVATE int vm_builtin_php_sapi_name(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1544 | `{` |
|        3 | 1545 | `	const char *zSapi = pCtx->pVm->bHttpContext ? "cli-server" : "cli";` |
|        1 | 1546 | `	SXUNUSED(nArg);` |
|        1 | 1547 | `	SXUNUSED(apArg); /* cc warning */` |
|        3 | 1548 | `	ph7_result_string(pCtx,zSapi,-1);` |
|        3 | 1549 | `	return PH7_OK;` |
|        1 | 1550 | `}` |
|        - | 1551 | `/*` |
|        - | 1552 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 1553 | ` */` |
|        - | 1554 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 1555 | ` "<html><head>"\` |
|        - | 1556 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 1557 | ` "<style type=\"text/css\">"\` |
|        - | 1558 | ` "div {"\` |
|        - | 1559 | `     "border: 1px solid #cccccc;"\` |
|        - | 1560 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 1561 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 1562 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 1563 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 1564 | `     "-webkit-border-radius: 10px;"\` |
|        - | 1565 | `     "-o-border-radius: 10px;"\` |
|        - | 1566 | `     "border-radius: 10px;"\` |
|        - | 1567 | `     "padding-left: 2em;"\` |
|        - | 1568 | `     "background-color: white;"\` |
|        - | 1569 | `     "margin-left: auto;"\` |
|        - | 1570 | `     "font-family: verdana;"\` |
|        - | 1571 | `     "padding-right: 2em;"\` |
|        - | 1572 | `     "margin-right: auto;"\` |
|        - | 1573 | `     "}"\` |
|        - | 1574 | `     "body {"\` |
|        - | 1575 | `     "padding: 0.2em;"\` |
|        - | 1576 | `     "font-style: normal;"\` |
|        - | 1577 | `     "font-size: medium;"\` |
|        - | 1578 | `     "background-color: #f2f2f2;"\` |
|        - | 1579 | `     "}"\` |
|        - | 1580 | `     "hr {"\` |
|        - | 1581 | `     "border-style: solid none none;"\` |
|        - | 1582 | `     "border-width: 1px medium medium;"\` |
|        - | 1583 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 1584 | `     "height: 1px;"\` |
|        - | 1585 | `     "}"\` |
|        - | 1586 | `     "a {"\` |
|        - | 1587 | `     "color: #3366cc;"\` |
|        - | 1588 | `     "text-decoration: none;"\` |
|        - | 1589 | `     "}"\` |
|        - | 1590 | `     "a:hover {"\` |
|        - | 1591 | `     "color: #999999;"\` |
|        - | 1592 | `     "}"\` |
|        - | 1593 | `     "a:active {"\` |
|        - | 1594 | `     "color: #663399;"\` |
|        - | 1595 | `     "}"\` |
|        - | 1596 | `     "h1 {"\` |
|        - | 1597 | `     "margin: 0;"\` |
|        - | 1598 | `     "padding: 0;"\` |
|        - | 1599 | `     "font-family: Verdana;"\` |
|        - | 1600 | `     "font-weight: bold;"\` |
|        - | 1601 | `     "font-style: normal;"\` |
|        - | 1602 | `     "font-size: medium;"\` |
|        - | 1603 | `     "text-transform: capitalize;"\` |
|        - | 1604 | `     "color: #0a328c;"\` |
|        - | 1605 | `     "}"\` |
|        - | 1606 | `     "p {"\` |
|        - | 1607 | `     "margin: 0 auto;"\` |
|        - | 1608 | `     "font-size: medium;"\` |
|        - | 1609 | `     "font-style: normal;"\` |
|        - | 1610 | `     "font-family: verdana;"\` |
|        - | 1611 | `     "}"\` |
|        - | 1612 | `"</style></head><body>"\` |
|        - | 1613 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 1614 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 1615 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 1616 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 1617 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 1618 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 1619 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 1620 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 1621 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 1622 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 1623 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 1624 |  |
|        - | 1625 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 1626 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 1627 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 1628 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 1629 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 1630 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 1631 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 1632 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 1633 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 1634 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 1635 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 1636 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 1637 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 1638 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 1639 |  |
|        - | 1640 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 1641 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 1642 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 1643 | `"&nbsp;*<br>"\` |
|        - | 1644 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 1645 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 1646 | `"&nbsp;* are met:<br>"\` |
|        - | 1647 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 1648 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 1649 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 1650 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 1651 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 1652 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 1653 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 1654 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 1655 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 1656 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 1657 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 1658 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 1659 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 1660 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 1661 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 1662 | `"&nbsp;*<br>"\` |
|        - | 1663 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 1664 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 1665 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 1666 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 1667 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 1668 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 1669 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 1670 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 1671 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 1672 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 1673 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 1674 | `"&nbsp;*/<br>"\` |
|        - | 1675 | `"</span></small></small></p>"\` |
|        - | 1676 | `"</div></body></html>"` |
|        - | 1677 | `/*` |
|        - | 1678 | ` * bool ph7credits(void)` |
|        - | 1679 | ` * bool ph7info(void)` |
|        - | 1680 | ` * bool ph7copyright(void)` |
|        - | 1681 | ` *  Prints out the credits for PH7 engine` |
|        - | 1682 | ` * Parameters` |
|        - | 1683 | ` *  None` |
|        - | 1684 | ` * Return` |
|        - | 1685 | ` *  Always TRUE` |
|        - | 1686 | ` */` |
|        2 | 1687 | `PH7_PRIVATE int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1688 | `{` |
|        3 | 1689 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 1690 | `	/* Expand the HTML page above*/` |
|        3 | 1691 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 1692 | `	ph7_context_output_format(` |
|        1 | 1693 | `		pCtx,` |
|        - | 1694 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 1695 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 1696 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 1697 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 1698 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 1699 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 1700 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 1701 | `#ifdef __WINNT__` |
|        - | 1702 | `		"Windows NT"` |
|        - | 1703 | `#elif defined(__UNIXES__)` |
|        - | 1704 | `		"UNIX-Like"` |
|        - | 1705 | `#else` |
|        - | 1706 | `		"Other OS"` |
|        - | 1707 | `#endif` |
|        - | 1708 | `		);` |
|        3 | 1709 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 1710 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 1711 | `	SXUNUSED(apArg);` |
|        - | 1712 | `	/* Return TRUE */` |
|        - | 1713 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 1714 | `	return PH7_OK;` |
|        1 | 1715 | `}` |
|        - | 1716 | `/*` |
|        - | 1717 | ` * Section:` |
|        - | 1718 | ` *    URL related routines.` |
|        - | 1719 | ` * Status:` |
|        - | 1720 | ` *    Stable.` |
|        - | 1721 | ` */` |
|        - | 1722 | `/*` |
|        - | 1723 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 1724 | ` *  Parse a URL and return its fields.` |
|        - | 1725 | ` * Parameters` |
|        - | 1726 | ` *  $url` |
|        - | 1727 | ` *   The URL to parse.` |
|        - | 1728 | ` * $component` |
|        - | 1729 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 1730 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 1731 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 1732 | ` *  in which case the return value will be an integer).` |
|        - | 1733 | ` * Return` |
|        - | 1734 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 1735 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 1736 | ` *  this array are:` |
|        - | 1737 | ` *   scheme - e.g. http` |
|        - | 1738 | ` *   host` |
|        - | 1739 | ` *   port` |
|        - | 1740 | ` *   user` |
|        - | 1741 | ` *   pass` |
|        - | 1742 | ` *   path` |
|        - | 1743 | ` *   query - after the question mark ?` |
|        - | 1744 | ` *   fragment - after the hashmark #` |
|        - | 1745 | ` * Note:` |
|        - | 1746 | ` *  FALSE is returned on failure.` |
|        - | 1747 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 1748 | ` *  with the standard PHP engine.` |
|        - | 1749 | ` */` |
|        - | 1750 | `/*` |
|        - | 1751 | ` * parse_url() component set.` |
|        - | 1752 | ` *` |
|        - | 1753 | ` * A bare SyString cannot express "present but empty", which php needs:` |
|        - | 1754 | ` * parse_url("") is ['path'=>''] and parse_url("?") is ['query'=>''], both` |
|        - | 1755 | ` * distinct from the component being absent. So presence is tracked separately.` |
|        - | 1756 | ` */` |
|     1328 | 1757 | `static int VmUrlIsAlnum(int c)` |
|        2 | 1758 | `{` |
|     1330 | 1759 | `	return (c >= '0' && c <= '9') \|\| (c >= 'a' && c <= 'z') \|\| (c >= 'A' && c <= 'Z');` |
|        2 | 1760 | `}` |
|       12 | 1761 | `static int VmUrlIsAlpha(int c)` |
|        2 | 1762 | `{` |
|       14 | 1763 | `	return (c >= 'a' && c <= 'z') \|\| (c >= 'A' && c <= 'Z');` |
|        2 | 1764 | `}` |
|        - | 1765 | `/* scheme = ALNUM *( ALNUM / "+" / "-" / "." ) -- php accepts a digit first. */` |
|     1328 | 1766 | `static int VmUrlIsSchemeByte(int c)` |
|        2 | 1767 | `{` |
|     1330 | 1768 | `	return VmUrlIsAlnum(c) \|\| c == '+' \|\| c == '-' \|\| c == '.';` |
|        2 | 1769 | `}` |
|        - | 1770 | `/*` |
|        - | 1771 | ` * Resolve the port span that followed the ':' in an authority.` |
|        - | 1772 | ` *` |
|        - | 1773 | ` * Returns 1 (usable port in *piPort), 0 (the span is empty, so there is simply` |
|        - | 1774 | ` * no port) or -1 (php rejects the whole URL). php is lenient about what follows` |
|        - | 1775 | ` * the digits -- ":8a" and ":8 0" both yield 8 -- but demands at least one digit` |
|        - | 1776 | ` * and a value that fits a port, so ":abc", ":-80" and ":65536" are all failures.` |
|        - | 1777 | ` */` |
|       92 | 1778 | `static int VmUrlParsePort(const char *z,int n,int *piPort)` |
|        2 | 1779 | `{` |
|       94 | 1780 | `	int i = 0,iVal = 0,nDigit = 0;` |
|       94 | 1781 | `	if( n < 1 ){` |
|      ! 0 | 1782 | `		return 0;` |
|        - | 1783 | `	}` |
|      140 | 1784 | `	while( i < n && (z[i] == ' ' \|\| z[i] == '\t') ){` |
|      ! 0 | 1785 | `		i++;` |
|      ! 0 | 1786 | `	}` |
|       94 | 1787 | `	if( i < n && (z[i] == '+' \|\| z[i] == '-') ){` |
|      ! 0 | 1788 | `		if( z[i] == '-' ){` |
|      ! 0 | 1789 | `			return -1;` |
|        - | 1790 | `		}` |
|      ! 0 | 1791 | `		i++;` |
|      ! 0 | 1792 | `	}` |
|      318 | 1793 | `	while( i < n && z[i] >= '0' && z[i] <= '9' ){` |
|      226 | 1794 | `		iVal = iVal * 10 + (z[i] - '0');` |
|      226 | 1795 | `		if( iVal > 65535 ){` |
|      ! 0 | 1796 | `			return -1; /* Out of range, and this also caps the accumulator */` |
|        - | 1797 | `		}` |
|      226 | 1798 | `		nDigit++;` |
|      226 | 1799 | `		i++;` |
|        2 | 1800 | `	}` |
|       94 | 1801 | `	if( nDigit < 1 ){` |
|       12 | 1802 | `		return -1;` |
|        - | 1803 | `	}` |
|       84 | 1804 | `	*piPort = iVal;` |
|       84 | 1805 | `	return 1;` |
|       48 | 1806 | `}` |
|        - | 1807 | `/*` |
|        - | 1808 | ` * Split an authority -- "[user[:pass]@]host[:port]" -- into pOut.` |
|        - | 1809 | ` * Returns 0 for the malformed input php reports as FALSE.` |
|        - | 1810 | ` */` |
|      278 | 1811 | `static int VmUrlParseAuthority(const char *z,int n,VmUrlParts *pOut,int bPortKnown)` |
|        2 | 1812 | `{` |
|        - | 1813 | `	const char *zHost;` |
|      280 | 1814 | `	int i,iAt = -1,iColon = -1,iSep = -1,nHost,iPort = 0,rc;` |
|        - | 1815 | `	/* php splits the credentials at the LAST '@' ("//a@b@c" is user "a@b") */` |
|     2110 | 1816 | `	for( i = 0 ; i < n ; ++i ){` |
|     1832 | 1817 | `		if( z[i] == '@' ){` |
|       44 | 1818 | `			iAt = i;` |
|       21 | 1819 | `		}` |
|      917 | 1820 | `	}` |
|      280 | 1821 | `	if( iAt >= 0 ){` |
|        - | 1822 | `		/* and the user from the password at the FIRST ':' before it */` |
|      154 | 1823 | `		for( i = 0 ; i < iAt ; ++i ){` |
|      152 | 1824 | `			if( z[i] == ':' ){` |
|       42 | 1825 | `				iColon = i;` |
|       42 | 1826 | `				break;` |
|        - | 1827 | `			}` |
|       57 | 1828 | `		}` |
|       44 | 1829 | `		if( iColon >= 0 ){` |
|       42 | 1830 | `			SyStringInitFromBuf(&pOut->sUser,z,(sxu32)iColon);` |
|       42 | 1831 | `			SyStringInitFromBuf(&pOut->sPass,&z[iColon+1],(sxu32)(iAt - iColon - 1));` |
|       42 | 1832 | `			pOut->bUser = pOut->bPass = 1;` |
|       22 | 1833 | `		}else{` |
|        3 | 1834 | `			SyStringInitFromBuf(&pOut->sUser,z,(sxu32)iAt);` |
|        3 | 1835 | `			pOut->bUser = 1;` |
|        - | 1836 | `		}` |
|       44 | 1837 | `		z += iAt + 1;` |
|       44 | 1838 | `		n -= iAt + 1;` |
|       21 | 1839 | `	}` |
|      280 | 1840 | `	zHost = z;` |
|      280 | 1841 | `	nHost = n;` |
|      280 | 1842 | `	if( !(n > 1 && z[0] == '[' && z[n-1] == ']') ){` |
|        - | 1843 | `		/* Unless a bracketed IPv6 literal fills the WHOLE authority -- in which` |
|        - | 1844 | `		 * case php keeps the brackets in "host" and scans no port, so every ':'` |
|        - | 1845 | `		 * inside belongs to the address -- the port hangs off the LAST ':'.` |
|        - | 1846 | `		 * php decides that on the first and last byte alone, which is why` |
|        - | 1847 | `		 * "//[:]\|]" is a single host while "//[::1]:8080" splits a port off. */` |
|     1838 | 1848 | `		for( i = 0 ; i < n ; ++i ){` |
|     1562 | 1849 | `			if( z[i] == ':' ){` |
|      118 | 1850 | `				iSep = i;` |
|       58 | 1851 | `			}` |
|      782 | 1852 | `		}` |
|      278 | 1853 | `		if( iSep >= 0 ){` |
|        - | 1854 | `			/* The host is trimmed at the ':' whether or not the port was already` |
|        - | 1855 | `			 * resolved by the caller. */` |
|       94 | 1856 | `			nHost = iSep;` |
|       94 | 1857 | `			if( !bPortKnown ){` |
|       88 | 1858 | `				rc = VmUrlParsePort(&z[iSep+1],n - iSep - 1,&iPort);` |
|       88 | 1859 | `				if( rc < 0 ){` |
|       12 | 1860 | `					return 0;` |
|        - | 1861 | `				}` |
|       78 | 1862 | `				if( rc > 0 ){` |
|       78 | 1863 | `					pOut->iPort = iPort;` |
|       78 | 1864 | `					pOut->bPort = 1;` |
|       38 | 1865 | `				}` |
|       38 | 1866 | `			}` |
|       41 | 1867 | `		}` |
|      133 | 1868 | `	}` |
|      270 | 1869 | `	if( nHost < 1 ){` |
|        - | 1870 | `		/* php requires a non-empty host once an authority is in play, which is` |
|        - | 1871 | `		 * what makes "//", "http://" and ":80" all FALSE. */` |
|       36 | 1872 | `		return 0;` |
|        - | 1873 | `	}` |
|      236 | 1874 | `	SyStringInitFromBuf(&pOut->sHost,zHost,(sxu32)nHost);` |
|      236 | 1875 | `	pOut->bHost = 1;` |
|      236 | 1876 | `	return 1;` |
|      141 | 1877 | `}` |
|        - | 1878 | `/*` |
|        - | 1879 | ` * Split "path[?query][#fragment]". The fragment is taken FIRST and the query` |
|        - | 1880 | ` * only from what precedes it, so "#a?b" is a fragment of "a?b" with no query.` |
|        - | 1881 | ` */` |
|      278 | 1882 | `static void VmUrlParsePath(const char *z,int n,VmUrlParts *pOut)` |
|        3 | 1883 | `{` |
|      281 | 1884 | `	int i,iEnd = n;` |
|     1669 | 1885 | `	for( i = 0 ; i < n ; ++i ){` |
|     1443 | 1886 | `		if( z[i] == '#' ){` |
|       54 | 1887 | `			SyStringInitFromBuf(&pOut->sFragment,&z[i+1],(sxu32)(n - i - 1));` |
|       54 | 1888 | `			pOut->bFragment = 1;` |
|       54 | 1889 | `			iEnd = i;` |
|       54 | 1890 | `			break;` |
|        - | 1891 | `		}` |
|      697 | 1892 | `	}` |
|     1399 | 1893 | `	for( i = 0 ; i < iEnd ; ++i ){` |
|     1189 | 1894 | `		if( z[i] == '?' ){` |
|       70 | 1895 | `			SyStringInitFromBuf(&pOut->sQuery,&z[i+1],(sxu32)(iEnd - i - 1));` |
|       70 | 1896 | `			pOut->bQuery = 1;` |
|       70 | 1897 | `			iEnd = i;` |
|       70 | 1898 | `			break;` |
|        - | 1899 | `		}` |
|      562 | 1900 | `	}` |
|      281 | 1901 | `	if( iEnd > 0 ){` |
|      263 | 1902 | `		SyStringInitFromBuf(&pOut->sPath,z,(sxu32)iEnd);` |
|      263 | 1903 | `		pOut->bPath = 1;` |
|      130 | 1904 | `	}` |
|      281 | 1905 | `}` |
|        - | 1906 | `/* Parse "host[:port]" followed by an optional path/query/fragment. */` |
|      278 | 1907 | `static int VmUrlAuthorityThenPath(const char *z,int n,VmUrlParts *pOut,int bPortKnown)` |
|        2 | 1908 | `{` |
|      280 | 1909 | `	int i,iEnd = n;` |
|     2110 | 1910 | `	for( i = 0 ; i < n ; ++i ){` |
|     2020 | 1911 | `		if( z[i] == '/' \|\| z[i] == '?' \|\| z[i] == '#' ){` |
|      190 | 1912 | `			iEnd = i;` |
|      190 | 1913 | `			break;` |
|        - | 1914 | `		}` |
|      917 | 1915 | `	}` |
|      280 | 1916 | `	if( !VmUrlParseAuthority(z,iEnd,pOut,bPortKnown) ){` |
|       46 | 1917 | `		return 0;` |
|        - | 1918 | `	}` |
|      236 | 1919 | `	if( iEnd < n ){` |
|      170 | 1920 | `		VmUrlParsePath(&z[iEnd],n - iEnd,pOut);` |
|       84 | 1921 | `	}` |
|      236 | 1922 | `	return 1;` |
|      141 | 1923 | `}` |
|        - | 1924 | `/*` |
|        - | 1925 | ` * Resolve the port php took from the FIRST ':' of a host:port URL.` |
|        - | 1926 | ` *` |
|        - | 1927 | ` * php reads the port straight off that colon before it works out where the host` |
|        - | 1928 | ` * ends, so the digits can even belong to what becomes the path: parse_url()` |
|        - | 1929 | ` * reports port 1 for "a/:1", whose path is "/:1". Mirroring the order keeps` |
|        - | 1930 | ` * that quirk. Returns 0 for a port php rejects.` |
|        - | 1931 | ` */` |
|        6 | 1932 | `static int VmUrlPreparePort(const char *z,int k,int nEnd,VmUrlParts *pOut)` |
|        1 | 1933 | `{` |
|        7 | 1934 | `	int iPort = 0;` |
|        7 | 1935 | `	int rc = VmUrlParsePort(&z[k+1],nEnd - k - 1,&iPort);` |
|        7 | 1936 | `	if( rc < 0 ){` |
|      ! 0 | 1937 | `		return 0;` |
|        - | 1938 | `	}` |
|        7 | 1939 | `	if( rc > 0 ){` |
|        7 | 1940 | `		pOut->iPort = iPort;` |
|        7 | 1941 | `		pOut->bPort = 1;` |
|        3 | 1942 | `	}` |
|        7 | 1943 | `	return 1;` |
|        4 | 1944 | `}` |
|        - | 1945 | `/*` |
|        - | 1946 | ` * php's URL parser, as parse_url() needs it. Returns 0 where php returns FALSE.` |
|        - | 1947 | ` *` |
|        - | 1948 | ` * PH7_VmHttpSplitURI cannot serve here: it is an HTTP REQUEST-target splitter` |
|        - | 1949 | ` * (the HTTP server and filter_var share it) and assumes the input is` |
|        - | 1950 | ` * authority-first, so it read "a" as a host and "mailto:me@x.com" as` |
|        - | 1951 | ` * user:pass@host. php instead decides authority-vs-path up front: only a "//",` |
|        - | 1952 | ` * with or without a scheme before it, introduces an authority.` |
|        - | 1953 | ` */` |
|      392 | 1954 | `PH7_PRIVATE int PH7_VmUrlSplit(const char *z,int n,VmUrlParts *pOut)` |
|        3 | 1955 | `{` |
|      395 | 1956 | `	int i,k = -1,bScheme,bPortForm = 0,nPortEnd = 0;` |
|      395 | 1957 | `	SyZero(pOut,sizeof(VmUrlParts));` |
|        - | 1958 | `	/* A leading "//" settles it before any colon is considered: the authority` |
|        - | 1959 | `	 * starts after the slashes. Reading the colon first turned "//h:80" into a` |
|        - | 1960 | `	 * host called "//h" and "//[::1]" into a path. */` |
|      395 | 1961 | `	if( n >= 2 && z[0] == '/' && z[1] == '/' ){` |
|       24 | 1962 | `		return VmUrlAuthorityThenPath(&z[2],n - 2,pOut,0);` |
|        - | 1963 | `	}` |
|     1883 | 1964 | `	for( i = 0 ; i < n ; ++i ){` |
|     1841 | 1965 | `		if( z[i] == ':' ){` |
|      330 | 1966 | `			k = i;` |
|      330 | 1967 | `			break;` |
|        - | 1968 | `		}` |
|      758 | 1969 | `	}` |
|      373 | 1970 | `	if( k == 0 && n == 1 ){` |
|        - | 1971 | `		/* A lone ":" is an EMPTY scheme, which php rejects outright -- unlike` |
|        - | 1972 | `		 * ":a" or "::", which are simply paths. */` |
|        3 | 1973 | `		return 0;` |
|        - | 1974 | `	}` |
|      371 | 1975 | `	bScheme = k > 0;` |
|     1699 | 1976 | `	for( i = 0 ; bScheme && i < k ; ++i ){` |
|     1330 | 1977 | `		if( !VmUrlIsSchemeByte((unsigned char)z[i]) ){` |
|      ! 0 | 1978 | `			bScheme = 0;` |
|      ! 0 | 1979 | `		}` |
|      666 | 1980 | `	}` |
|      371 | 1981 | `	if( bScheme && k + 1 == n ){` |
|        - | 1982 | `		/* "x:" -- the scheme is the whole URL */` |
|        3 | 1983 | `		SyStringInitFromBuf(&pOut->sScheme,z,(sxu32)k);` |
|        3 | 1984 | `		pOut->bScheme = 1;` |
|        3 | 1985 | `		return 1;` |
|        - | 1986 | `	}` |
|        - | 1987 | `	/* Decide whether that ':' introduces a PORT rather than a scheme: at least` |
|        - | 1988 | `	 * one digit, running to the end of the string or to a '/'. That is what` |
|        - | 1989 | `	 * makes "a:80" a host:port while "a:80?q" is a scheme with path "80", and` |
|        - | 1990 | `	 * it holds however the ':' is reached -- ":1" and "/:1" are both authorities` |
|        - | 1991 | `	 * with an empty host, which php rejects. php caps the run, so a long number` |
|        - | 1992 | `	 * stays a path: "a:123456" is a path, "a:99999" an out-of-range port. */` |
|      369 | 1993 | `	if( k >= 0 ){` |
|      326 | 1994 | `		int p = k + 1;` |
|      326 | 1995 | `		int bBeforeQuery = 1;` |
|      326 | 1996 | `		nPortEnd = k + 1;` |
|        - | 1997 | `		/* A ':' that sits inside a query or fragment is just data: "?:1" is a` |
|        - | 1998 | `		 * query of ":1", not an authority with an empty host. */` |
|     1652 | 1999 | `		for( i = 0 ; i < k ; ++i ){` |
|     1328 | 2000 | `			if( z[i] == '?' \|\| z[i] == '#' ){` |
|      ! 0 | 2001 | `				bBeforeQuery = 0;` |
|      ! 0 | 2002 | `				break;` |
|        - | 2003 | `			}` |
|      665 | 2004 | `		}` |
|      336 | 2005 | `		while( p < n && z[p] >= '0' && z[p] <= '9' ){` |
|       11 | 2006 | `			p++;` |
|        1 | 2007 | `		}` |
|      326 | 2008 | `		if( bBeforeQuery && p > k + 1 && (p >= n \|\| z[p] == '/') && (p - k) < 7 ){` |
|        7 | 2009 | `			bPortForm = 1;` |
|        7 | 2010 | `			nPortEnd = p;` |
|        3 | 2011 | `		}` |
|      162 | 2012 | `	}` |
|      369 | 2013 | `	if( !bScheme ){` |
|       47 | 2014 | `		if( bPortForm ){` |
|        3 | 2015 | `			if( !VmUrlPreparePort(z,k,nPortEnd,pOut) ){` |
|      ! 0 | 2016 | `				return 0;` |
|        - | 2017 | `			}` |
|        3 | 2018 | `			return VmUrlAuthorityThenPath(z,n,pOut,1);` |
|        - | 2019 | `		}` |
|       45 | 2020 | `		VmUrlParsePath(z,n,pOut);` |
|       45 | 2021 | `		if( !pOut->bPath && !pOut->bQuery && !pOut->bFragment ){` |
|        - | 2022 | `			/* php reports the empty URL as an empty PATH, not as no components */` |
|        3 | 2023 | `			SyStringInitFromBuf(&pOut->sPath,z,0);` |
|        3 | 2024 | `			pOut->bPath = 1;` |
|        1 | 2025 | `		}` |
|       45 | 2026 | `		return 1;` |
|        - | 2027 | `	}` |
|      324 | 2028 | `	if( bPortForm ){` |
|        5 | 2029 | `		if( !VmUrlPreparePort(z,k,nPortEnd,pOut) ){` |
|      ! 0 | 2030 | `			return 0;` |
|        - | 2031 | `		}` |
|        5 | 2032 | `		return VmUrlAuthorityThenPath(z,n,pOut,1);` |
|        - | 2033 | `	}` |
|      320 | 2034 | `	SyStringInitFromBuf(&pOut->sScheme,z,(sxu32)k);` |
|      320 | 2035 | `	pOut->bScheme = 1;` |
|      320 | 2036 | `	if( z[k+1] == '/' && k + 2 < n && z[k+2] == '/' ){` |
|      262 | 2037 | `		if( k + 3 < n && z[k+3] == '/' && pOut->sScheme.nByte == 4` |
|       20 | 2038 | `		 && (z[0]=='f'\|\|z[0]=='F') && (z[1]=='i'\|\|z[1]=='I')` |
|       14 | 2039 | `		 && (z[2]=='l'\|\|z[2]=='L') && (z[3]=='e'\|\|z[3]=='E') ){` |
|        - | 2040 | `			/* file:/// has no authority: the path starts at the third slash,` |
|        - | 2041 | `			 * except that a "c:" drive letter swallows it (file:///c:/x is the` |
|        - | 2042 | `			 * path "c:/x"). A '\|' in place of the ':' does NOT count. */` |
|       14 | 2043 | `			int iBase = k + 3;` |
|       14 | 2044 | `			if( iBase + 2 < n && VmUrlIsAlpha((unsigned char)z[iBase+1]) && z[iBase+2] == ':' ){` |
|      ! 0 | 2045 | `				iBase++;` |
|      ! 0 | 2046 | `			}` |
|       14 | 2047 | `			VmUrlParsePath(&z[iBase],n - iBase,pOut);` |
|       14 | 2048 | `			return 1;` |
|        - | 2049 | `		}` |
|      252 | 2050 | `		return VmUrlAuthorityThenPath(&z[k+3],n - k - 3,pOut,0);` |
|        - | 2051 | `	}` |
|        - | 2052 | `	/* "mailto:me@x.com", "x:y", "http:/x": everything after the ':' is a path */` |
|       58 | 2053 | `	VmUrlParsePath(&z[k+1],n - k - 1,pOut);` |
|       58 | 2054 | `	return 1;` |
|      199 | 2055 | `}` |
|        - | 2056 | `/*` |
|        - | 2057 | ` * Emit one parsed component into pValue, replacing control bytes with '_'.` |
|        - | 2058 | ` *` |
|        - | 2059 | ` * php runs php_replace_controlchars_ex over every string component it returns,` |
|        - | 2060 | ` * so a URL carrying a raw NUL, newline or DEL cannot smuggle it through into` |
|        - | 2061 | ` * whatever the caller splices the component into (a header, a log line, a` |
|        - | 2062 | ` * redirect). Bytes >= 0x80 are deliberately left alone -- php only folds the` |
|        - | 2063 | ` * ASCII control range.` |
|        - | 2064 | ` */` |
|      164 | 2065 | `static void VmUrlSetComponent(ph7_value *pValue,const SyString *pComp)` |
|        1 | 2066 | `{` |
|      165 | 2067 | `	const char *z = pComp->zString;` |
|      165 | 2068 | `	sxu32 n = pComp->nByte,i,iRun = 0;` |
|      165 | 2069 | `	if( n < 1 \|\| z == 0 ){` |
|        3 | 2070 | `		ph7_value_string(pValue,"",0);` |
|        3 | 2071 | `		return;` |
|        - | 2072 | `	}` |
|      955 | 2073 | `	for( i = 0 ; i < n ; ++i ){` |
|      793 | 2074 | `		unsigned char c = (unsigned char)z[i];` |
|      793 | 2075 | `		if( c < 0x20 \|\| c == 0x7f ){` |
|        3 | 2076 | `			if( i > iRun ){` |
|        3 | 2077 | `				ph7_value_string(pValue,&z[iRun],(int)(i - iRun));` |
|        1 | 2078 | `			}` |
|        3 | 2079 | `			ph7_value_string(pValue,"_",1);` |
|        3 | 2080 | `			iRun = i + 1;` |
|        1 | 2081 | `		}` |
|      397 | 2082 | `	}` |
|      163 | 2083 | `	if( n > iRun ){` |
|      163 | 2084 | `		ph7_value_string(pValue,&z[iRun],(int)(n - iRun));` |
|       81 | 2085 | `	}` |
|       83 | 2086 | `}` |
|      104 | 2087 | `PH7_PRIVATE int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 2088 | `{` |
|        - | 2089 | `	const char *zStr; /* Input string */` |
|        - | 2090 | `	VmUrlParts sUrl;  /* Parse of the given URI */` |
|        - | 2091 | `	SyString *pComp;` |
|        - | 2092 | `	int bHave;` |
|        - | 2093 | `	int nLen;` |
|      105 | 2094 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 2095 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 2096 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 2097 | `		return PH7_OK;` |
|        - | 2098 | `	}` |
|        - | 2099 | `	/* Extract the given URI. An empty string is NOT a failure: php parses it as` |
|        - | 2100 | `	 * an empty path. */` |
|      105 | 2101 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|      105 | 2102 | `	if( nLen < 0 ){` |
|      ! 0 | 2103 | `		nLen = 0;` |
|      ! 0 | 2104 | `	}` |
|      105 | 2105 | `	if( !PH7_VmUrlSplit(zStr,nLen,&sUrl) ){` |
|        - | 2106 | `		/* Malformed input,return FALSE */` |
|       13 | 2107 | `		ph7_result_bool(pCtx,0);` |
|       13 | 2108 | `		return PH7_OK;` |
|        - | 2109 | `	}` |
|      103 | 2110 | `	if( nArg > 1 && ph7_value_to_int(apArg[1]) >= 0 ){` |
|        - | 2111 | `		/* Refer to constant.c for constants values (php's PHP_URL_* are 0-based;` |
|        - | 2112 | `		 * PHL used to number them from 1, so every literal component id selected` |
|        - | 2113 | `		 * the WRONG field -- and the constants' own tests only asserted "%d",` |
|        - | 2114 | `		 * which any numbering satisfies). A negative id means "the whole array",` |
|        - | 2115 | `		 * which is what the default $component = -1 relies on. */` |
|       27 | 2116 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|       27 | 2117 | `		pComp = 0;` |
|       27 | 2118 | `		bHave = 0;` |
|       27 | 2119 | `		switch(nComponent){` |
|        3 | 2120 | `		case 0: /* PHP_URL_SCHEME */   pComp = &sUrl.sScheme;   bHave = sUrl.bScheme;   break;` |
|        5 | 2121 | `		case 1: /* PHP_URL_HOST */     pComp = &sUrl.sHost;     bHave = sUrl.bHost;     break;` |
|        2 | 2122 | `		case 2: /* PHP_URL_PORT */` |
|        5 | 2123 | `			if( sUrl.bPort ){` |
|        5 | 2124 | `				ph7_result_int(pCtx,sUrl.iPort);` |
|        3 | 2125 | `			}else{` |
|      ! 0 | 2126 | `				ph7_result_null(pCtx);` |
|        - | 2127 | `			}` |
|        5 | 2128 | `			return PH7_OK;` |
|        3 | 2129 | `		case 3: /* PHP_URL_USER */     pComp = &sUrl.sUser;     bHave = sUrl.bUser;     break;` |
|        3 | 2130 | `		case 4: /* PHP_URL_PASS */     pComp = &sUrl.sPass;     bHave = sUrl.bPass;     break;` |
|        3 | 2131 | `		case 5: /* PHP_URL_PATH */     pComp = &sUrl.sPath;     bHave = sUrl.bPath;     break;` |
|        5 | 2132 | `		case 6: /* PHP_URL_QUERY */    pComp = &sUrl.sQuery;    bHave = sUrl.bQuery;    break;` |
|        5 | 2133 | `		case 7: /* PHP_URL_FRAGMENT */ pComp = &sUrl.sFragment; bHave = sUrl.bFragment; break;` |
|        1 | 2134 | `		default:` |
|        4 | 2135 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|        - | 2136 | `				"parse_url(): Argument #2 ($component) must be a valid URL component identifier, %d given",` |
|        1 | 2137 | `				nComponent);` |
|        - | 2138 | `		}` |
|       21 | 2139 | `		if( bHave ){` |
|       19 | 2140 | `			ph7_value *pOut = ph7_context_new_scalar(pCtx);` |
|       19 | 2141 | `			if( pOut == 0 ){` |
|      ! 0 | 2142 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|      ! 0 | 2143 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 2144 | `				return PH7_OK;` |
|        - | 2145 | `			}` |
|       19 | 2146 | `			VmUrlSetComponent(pOut,pComp);` |
|       19 | 2147 | `			ph7_result_value(pCtx,pOut);` |
|       10 | 2148 | `		}else{` |
|        - | 2149 | `			/* No available value,return NULL */` |
|        3 | 2150 | `			ph7_result_null(pCtx);` |
|        - | 2151 | `		}` |
|       11 | 2152 | `	}else{` |
|        - | 2153 | `		ph7_value *pArray,*pValue;` |
|        - | 2154 | `		/* Return an associative array */` |
|       67 | 2155 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       67 | 2156 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       67 | 2157 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 2158 | `			/* Out of memory */` |
|      ! 0 | 2159 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 2160 | `			/* Return false */` |
|      ! 0 | 2161 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 2162 | `			return PH7_OK;` |
|        - | 2163 | `		}` |
|        - | 2164 | `		/* Fill the array, in php's key order. A component is emitted whenever it` |
|        - | 2165 | `		 * is PRESENT, even when empty -- parse_url("?") is ['query'=>'']. */` |
|       67 | 2166 | `		if( sUrl.bScheme ){` |
|       33 | 2167 | `			VmUrlSetComponent(pValue,&sUrl.sScheme);` |
|       33 | 2168 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|       33 | 2169 | `			ph7_value_reset_string_cursor(pValue);` |
|       16 | 2170 | `		}` |
|       67 | 2171 | `		if( sUrl.bHost ){` |
|       31 | 2172 | `			VmUrlSetComponent(pValue,&sUrl.sHost);` |
|       31 | 2173 | `			ph7_array_add_strkey_elem(pArray,"host",pValue);` |
|       31 | 2174 | `			ph7_value_reset_string_cursor(pValue);` |
|       15 | 2175 | `		}` |
|       67 | 2176 | `		if( sUrl.bPort ){` |
|       17 | 2177 | `			ph7_value_int(pValue,sUrl.iPort);` |
|       17 | 2178 | `			ph7_array_add_strkey_elem(pArray,"port",pValue);` |
|       17 | 2179 | `			ph7_value_reset_string_cursor(pValue);` |
|        8 | 2180 | `		}` |
|       67 | 2181 | `		if( sUrl.bUser ){` |
|        9 | 2182 | `			VmUrlSetComponent(pValue,&sUrl.sUser);` |
|        9 | 2183 | `			ph7_array_add_strkey_elem(pArray,"user",pValue);` |
|        9 | 2184 | `			ph7_value_reset_string_cursor(pValue);` |
|        4 | 2185 | `		}` |
|       67 | 2186 | `		if( sUrl.bPass ){` |
|        7 | 2187 | `			VmUrlSetComponent(pValue,&sUrl.sPass);` |
|        7 | 2188 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue);` |
|        7 | 2189 | `			ph7_value_reset_string_cursor(pValue);` |
|        3 | 2190 | `		}` |
|       67 | 2191 | `		if( sUrl.bPath ){` |
|       47 | 2192 | `			VmUrlSetComponent(pValue,&sUrl.sPath);` |
|       47 | 2193 | `			ph7_array_add_strkey_elem(pArray,"path",pValue);` |
|       47 | 2194 | `			ph7_value_reset_string_cursor(pValue);` |
|       23 | 2195 | `		}` |
|       67 | 2196 | `		if( sUrl.bQuery ){` |
|       13 | 2197 | `			VmUrlSetComponent(pValue,&sUrl.sQuery);` |
|       13 | 2198 | `			ph7_array_add_strkey_elem(pArray,"query",pValue);` |
|       13 | 2199 | `			ph7_value_reset_string_cursor(pValue);` |
|        6 | 2200 | `		}` |
|       67 | 2201 | `		if( sUrl.bFragment ){` |
|       13 | 2202 | `			VmUrlSetComponent(pValue,&sUrl.sFragment);` |
|       13 | 2203 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue);` |
|        6 | 2204 | `		}` |
|        - | 2205 | `		/* Return the created array */` |
|       67 | 2206 | `		ph7_result_value(pCtx,pArray);` |
|        - | 2207 | `		/* NOTE:` |
|        - | 2208 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 2209 | `		 * automatically as soon we return from this function.` |
|        - | 2210 | `		 */` |
|        - | 2211 | `	}` |
|        - | 2212 | `	/* All done */` |
|       87 | 2213 | `	return PH7_OK;` |
|       53 | 2214 | `}` |
|        - | 2215 |  |
|        - | 2216 | `/*` |
|        - | 2217 | ` * Section:` |
|        - | 2218 | ` *   Array related routines.` |
|        - | 2219 | ` * Status:` |
|        - | 2220 | ` *    Stable.` |
|        - | 2221 | ` * Note 2012-5-21 01:04:15:` |
|        - | 2222 | ` *  Array related functions that need access to the underlying` |
|        - | 2223 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 2224 | ` */` |
|        - | 2225 | `/*` |
|        - | 2226 | ` * The [compact()] function store it's state information in an instance` |
|        - | 2227 | ` * of the following structure.` |
|        - | 2228 | ` */` |
|        - | 2229 | `struct compact_data` |
|        - | 2230 | `{` |
|        - | 2231 | `	ph7_value *pArray;  /* Target array */` |
|        - | 2232 | `	int nRecCount;      /* Recursion count */` |
|        - | 2233 | `	ph7_context *pCtx;  /* Call context, for php's two warnings */` |
|        - | 2234 | `	int iArg;           /* 1-based ARGUMENT this element came from: php names the` |
|        - | 2235 | `	                     * argument even for an element found inside a nested` |
|        - | 2236 | `	                     * array, never the element's own position. */` |
|        - | 2237 | `};` |
|        - | 2238 | `/*` |
|        - | 2239 | ` * php's two compact() diagnostics, both E_WARNING and both non-fatal (the entry` |
|        - | 2240 | ` * is skipped and the rest of the call proceeds): a name that is neither a string` |
|        - | 2241 | ` * nor an array of strings, and a string naming a variable the frame does not` |
|        - | 2242 | ` * have. PHL emitted neither, so compact(1) and compact('typo') answered a` |
|        - | 2243 | ` * shorter array with nothing said -- the caller's only clue that a name was` |
|        - | 2244 | ` * dropped was the array's own size.` |
|        - | 2245 | ` */` |
|       14 | 2246 | `static void VmCompactBadName(ph7_context *pCtx,int iArg,ph7_value *pValue)` |
|        1 | 2247 | `{` |
|        - | 2248 | `	char zGiven[64];` |
|       22 | 2249 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 2250 | `		"Argument #%d must be string or array of strings, %s given",` |
|        7 | 2251 | `		iArg,VmValueGivenName(pValue,zGiven,sizeof(zGiven)));` |
|       15 | 2252 | `}` |
|        6 | 2253 | `static void VmCompactUndefined(ph7_context *pCtx,SyString *pVar)` |
|        1 | 2254 | `{` |
|       10 | 2255 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        6 | 2256 | `		"Undefined variable $%.*s",(int)pVar->nByte,pVar->zString);` |
|        7 | 2257 | `}` |
|        - | 2258 | `/*` |
|        - | 2259 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 2260 | ` */` |
|       16 | 2261 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 2262 | `{` |
|       17 | 2263 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|       17 | 2264 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|       17 | 2265 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 2266 | `	/* Act according to the hashmap value */` |
|       17 | 2267 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 2268 | `		SyString sVar;` |
|        9 | 2269 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|        - | 2270 | `		/* Query the current frame. An EMPTY name is looked up like any other:` |
|        - | 2271 | `		 * php reports it as "Undefined variable $" rather than skipping it. */` |
|        9 | 2272 | `		pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 2273 | `		/* ^` |
|        - | 2274 | `		 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 2275 | `		 */` |
|        9 | 2276 | `		if( pKey ){` |
|        - | 2277 | `			/* Perform the insertion */` |
|        7 | 2278 | `			ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|        4 | 2279 | `		}else{` |
|        3 | 2280 | `			VmCompactUndefined(pData->pCtx,&sVar);` |
|        1 | 2281 | `		}` |
|       13 | 2282 | `	}else if( ph7_value_is_array(pValue) ){` |
|        - | 2283 | `		/* Recursively traverse this array. Past the depth cap the element is` |
|        - | 2284 | `		 * dropped in silence, as it always was: the cap is PHL's own guard and` |
|        - | 2285 | `		 * the "must be string or array of strings" warning would be a lie about` |
|        - | 2286 | `		 * an argument that IS an array of strings. */` |
|        7 | 2287 | `		if( pData->nRecCount < 32 ){` |
|        - | 2288 | `			int rc;` |
|        7 | 2289 | `			pData->nRecCount++;` |
|        7 | 2290 | `			rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|        7 | 2291 | `			pData->nRecCount--;` |
|        7 | 2292 | `			return rc;` |
|        - | 2293 | `		}` |
|      ! 0 | 2294 | `	}else{` |
|        3 | 2295 | `		VmCompactBadName(pData->pCtx,pData->iArg,pValue);` |
|        - | 2296 | `	}` |
|       11 | 2297 | `	return SXRET_OK;` |
|        9 | 2298 | `}` |
|        - | 2299 | `/*` |
|        - | 2300 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 2301 | ` *  Create array containing variables and their values.` |
|        - | 2302 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 2303 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 2304 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 2305 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 2306 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 2307 | ` * Parameters` |
|        - | 2308 | ` *  $varname` |
|        - | 2309 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 2310 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 2311 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 2312 | ` *   it recursively.` |
|        - | 2313 | ` * Return` |
|        - | 2314 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 2315 | ` */` |
|       26 | 2316 | `PH7_PRIVATE int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 2317 | `{` |
|        - | 2318 | `	ph7_value *pArray,*pObj;` |
|       27 | 2319 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 2320 | `	const char *zName;` |
|        - | 2321 | `	SyString sVar;` |
|        - | 2322 | `	int i,nLen;` |
|       27 | 2323 | `	if( nArg < 1 ){` |
|        - | 2324 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 2325 | `		ph7_result_null(pCtx);` |
|      ! 0 | 2326 | `		return PH7_OK;` |
|        - | 2327 | `	}` |
|        - | 2328 | `	/* Create the array */` |
|       27 | 2329 | `	pArray = ph7_context_new_array(pCtx);` |
|       27 | 2330 | `	if( pArray == 0 ){` |
|        - | 2331 | `		/* Out of memory */` |
|      ! 0 | 2332 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 2333 | `		/* Return NULL */` |
|      ! 0 | 2334 | `		ph7_result_null(pCtx);` |
|      ! 0 | 2335 | `		return PH7_OK;` |
|        - | 2336 | `	}` |
|        - | 2337 | `	/* Perform the requested operation */` |
|       65 | 2338 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       39 | 2339 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|       19 | 2340 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 2341 | `				struct compact_data sData;` |
|        7 | 2342 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 2343 | `				/* Recursively walk the array */` |
|        7 | 2344 | `				sData.nRecCount = 0;` |
|        7 | 2345 | `				sData.pArray = pArray;` |
|        7 | 2346 | `				sData.pCtx = pCtx;` |
|        7 | 2347 | `				sData.iArg = i + 1;` |
|        7 | 2348 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|        4 | 2349 | `			}else{` |
|       13 | 2350 | `				VmCompactBadName(pCtx,i + 1,apArg[i]);` |
|        - | 2351 | `			}` |
|       10 | 2352 | `		}else{` |
|        - | 2353 | `			/* Extract variable name. An EMPTY one is looked up like any other:` |
|        - | 2354 | `			 * php reports it as "Undefined variable $" rather than skipping it. */` |
|       21 | 2355 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|       21 | 2356 | `			SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 2357 | `			/* Check if the variable is available in the current frame */` |
|       21 | 2358 | `			pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|       21 | 2359 | `			if( pObj ){` |
|       17 | 2360 | `				ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        9 | 2361 | `			}else{` |
|        5 | 2362 | `				VmCompactUndefined(pCtx,&sVar);` |
|        - | 2363 | `			}` |
|        - | 2364 | `		}` |
|       20 | 2365 | `	}` |
|        - | 2366 | `	/* Return the array */` |
|       27 | 2367 | `	ph7_result_value(pCtx,pArray);` |
|       27 | 2368 | `	return PH7_OK;` |
|       14 | 2369 | `}` |
|        - | 2370 | `/*` |
|        - | 2371 | ` * The [import_request_variables()] function store it's state information` |
|        - | 2372 | ` * in an instance of the following structure.` |
|        - | 2373 | ` */` |
|        - | 2374 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 2375 | `struct extract_aux_data` |
|        - | 2376 | `{` |
|        - | 2377 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 2378 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 2379 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 2380 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 2381 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 2382 | `};` |
|        - | 2383 | `/*` |
|        - | 2384 | ` * php's php_valid_var_name(): a legal PHP variable name matches` |
|        - | 2385 | ` * [A-Za-z_\x80-\xff][A-Za-z0-9_\x80-\xff]* . Byte-wise and locale-free on` |
|        - | 2386 | ` * purpose (php has been locale-independent here since 8.0); the high-byte` |
|        - | 2387 | ` * range is what lets UTF-8 identifiers through. extract() drops every key` |
|        - | 2388 | ` * that does not pass, instead of installing an unreachable variable.` |
|        - | 2389 | ` */` |
|      162 | 2390 | `static int VmIsValidVarName(const char *zName,sxu32 nByte)` |
|        4 | 2391 | `{` |
|        - | 2392 | `	unsigned char c;` |
|        - | 2393 | `	sxu32 i;` |
|      166 | 2394 | `	if( nByte < 1 ){` |
|        7 | 2395 | `		return FALSE;` |
|        - | 2396 | `	}` |
|      160 | 2397 | `	c = (unsigned char)zName[0];` |
|      160 | 2398 | `	if( c != '_' && !(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') && c < 0x80 ){` |
|       11 | 2399 | `		return FALSE;` |
|        - | 2400 | `	}` |
|      392 | 2401 | `	for( i = 1 ; i < nByte ; ++i ){` |
|      263 | 2402 | `		c = (unsigned char)zName[i];` |
|      260 | 2403 | `		if( c != '_' && !(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z')` |
|       80 | 2404 | `		 && !(c >= '0' && c <= '9') && c < 0x80 ){` |
|       20 | 2405 | `			return FALSE;` |
|        - | 2406 | `		}` |
|      124 | 2407 | `	}` |
|      132 | 2408 | `	return TRUE;` |
|       85 | 2409 | `}` |
|        - | 2410 | `/* TRUE when the name is exactly "this": php refuses to re-assign $this. */` |
|      170 | 2411 | `static int VmExtractIsThis(const char *zName,sxu32 nByte)` |
|        4 | 2412 | `{` |
|      174 | 2413 | `	return nByte == sizeof("this")-1 && SyMemcmp(zName,"this",sizeof("this")-1) == 0;` |
|        4 | 2414 | `}` |
|        - | 2415 | `/*` |
|        - | 2416 | ` * TRUE when the name belongs to the superglobal table ($GLOBALS, $_SERVER,` |
|        - | 2417 | ` * $_GET, …). php hands extract() a per-frame symbol table that holds no` |
|        - | 2418 | ` * superglobal, so such a key lands in the LOCAL table and the real superglobal` |
|        - | 2419 | ` * is untouched. In PHL the name resolves to the superglobal SLOT itself` |
|        - | 2420 | ` * (VmExtractMemObj consults hSuper first), so a plain store would replace` |
|        - | 2421 | ` * $GLOBALS/$_SERVER with the imported value and take the whole symbol table /` |
|        - | 2422 | ` * request environment with it. Those keys are dropped instead — a prefixed` |
|        - | 2423 | ` * name ($p__SERVER) is a normal local and stores fine.` |
|        - | 2424 | ` */` |
|      118 | 2425 | `static int VmExtractIsProtected(ph7_vm *pVm,const char *zName,sxu32 nByte)` |
|        4 | 2426 | `{` |
|      122 | 2427 | `	return SyHashGet(&pVm->hSuper,(const void *)zName,nByte) != 0;` |
|        4 | 2428 | `}` |
|        - | 2429 | `/*` |
|        - | 2430 | ` * TRUE when the calling frame already holds this variable name.` |
|        - | 2431 | ` * "this" and the superglobals always answer FALSE, matching the symbol table` |
|        - | 2432 | ` * php hands extract(): $this is bound implicitly and superglobals live outside` |
|        - | 2433 | ` * the frame. So EXTR_IF_EXISTS/EXTR_PREFIX_IF_EXISTS skip those keys (php-exact)` |
|        - | 2434 | ` * and EXTR_PREFIX_SAME takes its not-a-collision branch.` |
|        - | 2435 | ` */` |
|       72 | 2436 | `static int VmExtractVarExists(ph7_vm *pVm,const char *zName,sxu32 nByte)` |
|        4 | 2437 | `{` |
|        - | 2438 | `	SyString sVar;` |
|       76 | 2439 | `	if( VmExtractIsThis(zName,nByte) \|\| VmExtractIsProtected(pVm,zName,nByte) ){` |
|       20 | 2440 | `		return FALSE;` |
|        - | 2441 | `	}` |
|       57 | 2442 | `	SyStringInitFromBuf(&sVar,zName,nByte);` |
|       57 | 2443 | `	return VmExtractMemObj(pVm,&sVar,FALSE,FALSE) != 0;` |
|       40 | 2444 | `}` |
|        - | 2445 | `/*` |
|        - | 2446 | ` * Create-or-overwrite a variable of the calling frame with a copy of pValue.` |
|        - | 2447 | ` * Returns TRUE when the variable was written (php counts exactly those).` |
|        - | 2448 | ` */` |
|        - | 2449 | `/*` |
|        - | 2450 | ` * EXTR_REFS: bind the imported NAME to the array element's own slot instead of copying` |
|        - | 2451 | ` * the value, so a later write through the variable reaches the caller's array — php's` |
|        - | 2452 | ` * by-reference extraction. The binding is registered (PH7_VmBindVarSlot), which is what` |
|        - | 2453 | ` * makes the element count the new name as a holder and read as a reference.` |
|        - | 2454 | ` *` |
|        - | 2455 | ` * The name is DUPLICATED: the symbol table keeps the pointer it is given, and the caller's` |
|        - | 2456 | ` * is a scratch blob the next entry reuses.` |
|        - | 2457 | ` */` |
|        8 | 2458 | `static int VmExtractBindVar(ph7_vm *pVm,const char *zName,sxu32 nByte,sxu32 nIdx)` |
|        1 | 2459 | `{` |
|        9 | 2460 | `	VmFrame *pFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|        - | 2461 | `	char *zDup;` |
|        9 | 2462 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 | 2463 | `		return FALSE;` |
|        - | 2464 | `	}` |
|        9 | 2465 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zName,nByte);` |
|        9 | 2466 | `	if( zDup == 0 ){` |
|      ! 0 | 2467 | `		return FALSE;` |
|        - | 2468 | `	}` |
|        9 | 2469 | `	PH7_VmBindVarSlot(&(*pVm),pFrame,zDup,nByte,nIdx);` |
|        9 | 2470 | `	return TRUE;` |
|        5 | 2471 | `}` |
|       66 | 2472 | `static int VmExtractStoreVar(ph7_vm *pVm,const char *zName,sxu32 nByte,ph7_value *pValue)` |
|        3 | 2473 | `{` |
|        - | 2474 | `	ph7_value *pObj;` |
|        - | 2475 | `	SyString sVar;` |
|       69 | 2476 | `	SyStringInitFromBuf(&sVar,zName,nByte);` |
|        - | 2477 | `	/* bDup: the name lives in a scratch blob that is reused by the next entry */` |
|       69 | 2478 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|       69 | 2479 | `	if( pObj == 0 ){` |
|      ! 0 | 2480 | `		return FALSE;` |
|        - | 2481 | `	}` |
|       69 | 2482 | `	PH7_MemObjStore(pValue,pObj);` |
|       69 | 2483 | `	return TRUE;` |
|       36 | 2484 | `}` |
|        - | 2485 | `/*` |
|        - | 2486 | ` * Build php's prefixed name "<prefix>_<key>" (php_prefix_varname with` |
|        - | 2487 | ` * add_underscore): the separator is unconditional, so an empty prefix still` |
|        - | 2488 | ` * yields "_key" exactly like php.` |
|        - | 2489 | ` */` |
|       40 | 2490 | `static sxi32 VmExtractPrefixName(SyBlob *pOut,const char *zPrefix,int nPrefix,` |
|        - | 2491 | `	const char *zKey,sxu32 nKey)` |
|        3 | 2492 | `{` |
|       43 | 2493 | `	SyBlobReset(pOut);` |
|       43 | 2494 | `	if( nPrefix > 0 && SyBlobAppend(pOut,zPrefix,(sxu32)nPrefix) != SXRET_OK ){` |
|      ! 0 | 2495 | `		return SXERR_MEM;` |
|        - | 2496 | `	}` |
|       43 | 2497 | `	if( SyBlobAppend(pOut,"_",sizeof(char)) != SXRET_OK ){` |
|      ! 0 | 2498 | `		return SXERR_MEM;` |
|        - | 2499 | `	}` |
|       43 | 2500 | `	if( nKey > 0 && SyBlobAppend(pOut,zKey,nKey) != SXRET_OK ){` |
|      ! 0 | 2501 | `		return SXERR_MEM;` |
|        - | 2502 | `	}` |
|       43 | 2503 | `	return SXRET_OK;` |
|       23 | 2504 | `}` |
|        - | 2505 | `/* What to do with one array entry, decided by the extract mode. */` |
|        - | 2506 | `#define VM_EXTRACT_DROP     0 /* php skips this key entirely */` |
|        - | 2507 | `#define VM_EXTRACT_PLAIN    1 /* install under the key itself */` |
|        - | 2508 | `#define VM_EXTRACT_PREFIX   2 /* install under "<prefix>_<key>" */` |
|        - | 2509 | `/*` |
|        - | 2510 | ` * int extract(array &$array[,int $flags = EXTR_OVERWRITE[,string $prefix = "" ]])` |
|        - | 2511 | ` *   Import variables into the current symbol table from an array.` |
|        - | 2512 | ` *` |
|        - | 2513 | ` * $flags is php's ENUM (PH7_EXTR_* in ph7int.h), not a bitmask: the mode is` |
|        - | 2514 | ` * (flags & 0xff) and anything above EXTR_IF_EXISTS(6) is a ValueError. The` |
|        - | 2515 | ` * modes, mirroring php's per-mode helpers in ext/standard/array.c:` |
|        - | 2516 | ` *   EXTR_OVERWRITE(0)        collisions overwrite` |
|        - | 2517 | ` *   EXTR_SKIP(1)             collisions keep the existing variable` |
|        - | 2518 | ` *   EXTR_PREFIX_SAME(2)      collisions install "<prefix>_<key>"` |
|        - | 2519 | ` *   EXTR_PREFIX_ALL(3)       every key installs as "<prefix>_<key>"` |
|        - | 2520 | ` *   EXTR_PREFIX_INVALID(4)   only illegal names (and numeric keys) get prefixed` |
|        - | 2521 | ` *   EXTR_PREFIX_IF_EXISTS(5) install "<prefix>_<key>" only if <key> exists` |
|        - | 2522 | ` *   EXTR_IF_EXISTS(6)        overwrite only variables that already exist` |
|        - | 2523 | `` * Modes 2..5 require $prefix (php: `is required when using this extract type`),`` |
|        - | 2524 | ` * a non-empty $prefix must itself be a legal identifier, and a key whose final` |
|        - | 2525 | ` * name is not a legal variable name is dropped rather than installed. Only` |
|        - | 2526 | ` * EXTR_PREFIX_ALL/EXTR_PREFIX_INVALID look at numeric keys at all.` |
|        - | 2527 | ` *` |
|        - | 2528 | `` * $this is never a target: php throws `Cannot re-assign $this` where a store`` |
|        - | 2529 | ` * would land on it, and skips it where a store would not (EXTR_SKIP), and` |
|        - | 2530 | ` * $GLOBALS is never clobbered.` |
|        - | 2531 | ` * Return` |
|        - | 2532 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 2533 | ` */` |
|      102 | 2534 | `PH7_PRIVATE int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 | 2535 | `{` |
|      106 | 2536 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 2537 | `	ph7_hashmap_node *pEntry;` |
|        - | 2538 | `	ph7_hashmap *pMap;` |
|      106 | 2539 | `	const char *zPrefix = 0;` |
|      106 | 2540 | `	sxi64 iFlags = PH7_EXTR_OVERWRITE;` |
|      106 | 2541 | `	sxi64 iCount = 0;` |
|        - | 2542 | `	ph7_value sValue;` |
|        - | 2543 | `	SyBlob sWorker;` |
|      106 | 2544 | `	int nPrefix = 0;` |
|      106 | 2545 | `	sxi32 rc = PH7_OK;` |
|        - | 2546 | `	int iType;` |
|        - | 2547 | `	sxu32 n;` |
|      106 | 2548 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|        - | 2549 | `		char zBuf[64];` |
|      ! 0 | 2550 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|        - | 2551 | `			"extract(): Argument #1 ($array) must be of type array, %s given",` |
|      ! 0 | 2552 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|        - | 2553 | `	}` |
|      106 | 2554 | `	if( nArg > 1 ){` |
|       98 | 2555 | `		rc = PH7_IntArgResolve(pCtx,apArg[1],"extract",2,"$flags","int",&iFlags);` |
|       98 | 2556 | `		if( rc != PH7_OK ){` |
|      ! 0 | 2557 | `			return rc;` |
|        - | 2558 | `		}` |
|       47 | 2559 | `	}` |
|        - | 2560 | `	/* php: the mode is the low byte; EXTR_REFS(0x100) rides above it */` |
|      106 | 2561 | `	iType = (int)(iFlags & 0xff);` |
|      106 | 2562 | `	if( iType > PH7_EXTR_IF_EXISTS ){` |
|        7 | 2563 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|        - | 2564 | `			"extract(): Argument #2 ($flags) must be a valid extract type");` |
|        - | 2565 | `	}` |
|      100 | 2566 | `	if( iType > PH7_EXTR_SKIP && iType <= PH7_EXTR_PREFIX_IF_EXISTS && nArg < 3 ){` |
|       12 | 2567 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|        - | 2568 | `			"extract(): Argument #3 ($prefix) is required when using this extract type");` |
|        - | 2569 | `	}` |
|       90 | 2570 | `	if( nArg > 2 ){` |
|       41 | 2571 | `		zPrefix = ph7_value_to_string(apArg[2],&nPrefix);` |
|       41 | 2572 | `		if( nPrefix > 0 && !VmIsValidVarName(zPrefix,(sxu32)nPrefix) ){` |
|        5 | 2573 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|        - | 2574 | `				"extract(): Argument #3 ($prefix) must be a valid identifier");` |
|        - | 2575 | `		}` |
|       17 | 2576 | `	}` |
|        - | 2577 | `	/* Point to the target hashmap */` |
|       86 | 2578 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       86 | 2579 | `	if( pMap->nEntry < 1 ){` |
|        - | 2580 | `		/* Empty map,return  0 */` |
|      ! 0 | 2581 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 2582 | `		return PH7_OK;` |
|        - | 2583 | `	}` |
|       86 | 2584 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|       86 | 2585 | `	PH7_MemObjInit(pVm,&sValue);` |
|        - | 2586 | `	/* php walks a COPY of the array, so an entry that overwrites the caller's own` |
|        - | 2587 | `	 * array variable ($arr = ['arr'=>1]; extract($arr)) cannot pull the map from` |
|        - | 2588 | `	 * under the walk. Pinning the map for the walk is the same guarantee. */` |
|       86 | 2589 | `	pMap->iRef++;` |
|       86 | 2590 | `	pEntry = pMap->pFirst;` |
|        - | 2591 | `	/* pFirst walks the insertion order through pPrev — PH7 links the entry list` |
|        - | 2592 | `	 * in reverse (same traversal PH7_HashmapWalk uses). */` |
|      252 | 2593 | `	for( n = pMap->nEntry ; n > 0 && pEntry ; --n, pEntry = pEntry->pPrev ){` |
|        - | 2594 | `		const char *zKey, *zFinal;` |
|        - | 2595 | `		sxu32 nKey, nFinal;` |
|        - | 2596 | `		char zNum[32];` |
|        - | 2597 | `		int bIntKey, iAction;` |
|        - | 2598 | `		/* Work off a COPY of the entry value: installing a variable can grow` |
|        - | 2599 | `		 * pVm->aMemObj, and a pointer into that set would dangle across the` |
|        - | 2600 | `		 * reallocation (this is why the walk API hands out copies too). The` |
|        - | 2601 | `		 * release comes FIRST so it covers every continue/goto below: the load` |
|        - | 2602 | `		 * takes a reference on an array/object value and does not drop the one` |
|        - | 2603 | `		 * the previous entry left behind (PH7_HashmapWalk releases per iteration` |
|        - | 2604 | `		 * for the same reason — without it a whole-array extract() pins every` |
|        - | 2605 | `		 * value it copied, and their destructors never run). */` |
|      172 | 2606 | `		PH7_MemObjRelease(&sValue);` |
|      172 | 2607 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|      172 | 2608 | `		bIntKey = (pEntry->iType == HASHMAP_INT_NODE);` |
|      172 | 2609 | `		if( bIntKey ){` |
|        - | 2610 | `			/* Only the two prefixing modes below ever look at a numeric key */` |
|       22 | 2611 | `			nKey = SyBufferFormat(zNum,sizeof(zNum),"%qd",pEntry->xKey.iKey);` |
|       22 | 2612 | `			zKey = zNum;` |
|       12 | 2613 | `		}else{` |
|      152 | 2614 | `			zKey = (const char *)SyBlobData(&pEntry->xKey.sKey);` |
|      152 | 2615 | `			nKey = SyBlobLength(&pEntry->xKey.sKey);` |
|        - | 2616 | `		}` |
|      172 | 2617 | `		iAction = VM_EXTRACT_DROP;` |
|      172 | 2618 | `		switch( iType ){` |
|       19 | 2619 | `		case PH7_EXTR_OVERWRITE:` |
|       42 | 2620 | `			if( bIntKey \|\| !VmIsValidVarName(zKey,nKey) ){` |
|        8 | 2621 | `				break;` |
|        - | 2622 | `			}` |
|       30 | 2623 | `			if( VmExtractIsThis(zKey,nKey) ){` |
|        3 | 2624 | `				goto this_error;` |
|        - | 2625 | `			}` |
|       28 | 2626 | `			iAction = VM_EXTRACT_PLAIN;` |
|       28 | 2627 | `			break;` |
|       12 | 2628 | `		case PH7_EXTR_SKIP:` |
|       27 | 2629 | `			if( bIntKey \|\| !VmIsValidVarName(zKey,nKey) \|\| VmExtractIsThis(zKey,nKey) ){` |
|        6 | 2630 | `				break;` |
|        - | 2631 | `			}` |
|       17 | 2632 | `			if( VmExtractVarExists(pVm,zKey,nKey) ){` |
|        8 | 2633 | `				break; /* collision: keep the existing variable */` |
|        - | 2634 | `			}` |
|       10 | 2635 | `			iAction = VM_EXTRACT_PLAIN;` |
|       10 | 2636 | `			break;` |
|       16 | 2637 | `		case PH7_EXTR_IF_EXISTS:` |
|       36 | 2638 | `			if( bIntKey \|\| !VmExtractVarExists(pVm,zKey,nKey) ){` |
|       14 | 2639 | `				break;` |
|        - | 2640 | `			}` |
|       12 | 2641 | `			if( !VmIsValidVarName(zKey,nKey) ){` |
|      ! 0 | 2642 | `				break;` |
|        - | 2643 | `			}` |
|       12 | 2644 | `			if( VmExtractIsThis(zKey,nKey) ){` |
|      ! 0 | 2645 | `				goto this_error;` |
|        - | 2646 | `			}` |
|       12 | 2647 | `			iAction = VM_EXTRACT_PLAIN;` |
|       12 | 2648 | `			break;` |
|        9 | 2649 | `		case PH7_EXTR_PREFIX_SAME:` |
|       21 | 2650 | `			if( bIntKey \|\| nKey < 1 ){` |
|        3 | 2651 | `				break;` |
|        - | 2652 | `			}` |
|       17 | 2653 | `			if( VmExtractVarExists(pVm,zKey,nKey) ){` |
|        6 | 2654 | `				iAction = VM_EXTRACT_PREFIX; /* collision */` |
|       14 | 2655 | `			}else if( !VmIsValidVarName(zKey,nKey) ){` |
|        5 | 2656 | `				break;` |
|      ! 0 | 2657 | `			}else{` |
|        - | 2658 | `				/* $this cannot be a target, but its prefixed form can */` |
|        8 | 2659 | `				iAction = VmExtractIsThis(zKey,nKey) ? VM_EXTRACT_PREFIX : VM_EXTRACT_PLAIN;` |
|        - | 2660 | `			}` |
|       13 | 2661 | `			break;` |
|       13 | 2662 | `		case PH7_EXTR_PREFIX_ALL:` |
|       28 | 2663 | `			if( !bIntKey && nKey < 1 ){` |
|        3 | 2664 | `				break;` |
|        - | 2665 | `			}` |
|       26 | 2666 | `			iAction = VM_EXTRACT_PREFIX;` |
|       26 | 2667 | `			break;` |
|        7 | 2668 | `		case PH7_EXTR_PREFIX_INVALID:` |
|       15 | 2669 | `			iAction = (bIntKey \|\| !VmIsValidVarName(zKey,nKey) \|\| VmExtractIsThis(zKey,nKey))` |
|       13 | 2670 | `				? VM_EXTRACT_PREFIX : VM_EXTRACT_PLAIN;` |
|       16 | 2671 | `			break;` |
|        8 | 2672 | `		case PH7_EXTR_PREFIX_IF_EXISTS:` |
|       18 | 2673 | `			if( !bIntKey && VmExtractVarExists(pVm,zKey,nKey) ){` |
|        3 | 2674 | `				iAction = VM_EXTRACT_PREFIX;` |
|        1 | 2675 | `			}` |
|       16 | 2676 | `			break;` |
|      ! 0 | 2677 | `		default:` |
|      ! 0 | 2678 | `			break;` |
|        - | 2679 | `		}` |
|      170 | 2680 | `		if( iAction == VM_EXTRACT_DROP ){` |
|      124 | 2681 | `			continue;` |
|        - | 2682 | `		}` |
|       96 | 2683 | `		if( iAction == VM_EXTRACT_PREFIX ){` |
|       43 | 2684 | `			if( VmExtractPrefixName(&sWorker,zPrefix,nPrefix,zKey,nKey) != SXRET_OK ){` |
|      ! 0 | 2685 | `				rc = PH7_VmThrowException(pCtx,"Error","PH7 engine is running out of memory");` |
|      ! 0 | 2686 | `				goto done;` |
|        - | 2687 | `			}` |
|       43 | 2688 | `			zFinal = (const char *)SyBlobData(&sWorker);` |
|       43 | 2689 | `			nFinal = SyBlobLength(&sWorker);` |
|       43 | 2690 | `			if( !VmIsValidVarName(zFinal,nFinal) ){` |
|        7 | 2691 | `				continue; /* php drops a prefixed name that is not an identifier */` |
|        - | 2692 | `			}` |
|       37 | 2693 | `			if( VmExtractIsThis(zFinal,nFinal) ){` |
|      ! 0 | 2694 | `				goto this_error;` |
|        - | 2695 | `			}` |
|       20 | 2696 | `		}else{` |
|       56 | 2697 | `			if( VmExtractIsProtected(pVm,zKey,nKey) ){` |
|       13 | 2698 | `				continue; /* $GLOBALS/$_SERVER/... are never a plain target */` |
|        - | 2699 | `			}` |
|       44 | 2700 | `			zFinal = zKey;` |
|       44 | 2701 | `			nFinal = nKey;` |
|        - | 2702 | `		}` |
|       78 | 2703 | `		if( iFlags & PH7_EXTR_REFS ){` |
|        - | 2704 | `			/* Bind to the element's own slot (php's EXTR_REFS) rather than copying */` |
|        9 | 2705 | `			if( VmExtractBindVar(pVm,zFinal,nFinal,pEntry->nValIdx) ){` |
|        9 | 2706 | `				iCount++;` |
|        5 | 2707 | `			}` |
|       73 | 2708 | `		}else if( VmExtractStoreVar(pVm,zFinal,nFinal,&sValue) ){` |
|       69 | 2709 | `			iCount++;` |
|       33 | 2710 | `		}` |
|       78 | 2711 | `		continue;` |
|        1 | 2712 | `this_error:` |
|        3 | 2713 | `		rc = PH7_VmThrowException(pCtx,"Error","Cannot re-assign $this");` |
|        3 | 2714 | `		goto done;` |
|      ! 0 | 2715 | `	}` |
|        - | 2716 | `	/* Number of variables successfully imported */` |
|       84 | 2717 | `	ph7_result_int64(pCtx,iCount);` |
|       41 | 2718 | `done:` |
|       86 | 2719 | `	PH7_MemObjRelease(&sValue);` |
|       86 | 2720 | `	SyBlobRelease(&sWorker);` |
|       86 | 2721 | `	PH7_HashmapUnref(pMap);` |
|       86 | 2722 | `	return rc;` |
|       55 | 2723 | `}` |
|        - | 2724 | `/*` |
|        - | 2725 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 2726 | ` * defined below.` |
|        - | 2727 | ` */` |
|        2 | 2728 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 2729 | `{` |
|        3 | 2730 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 2731 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 2732 | `	ph7_value *pObj;` |
|        - | 2733 | `	SyString sVar;` |
|        - | 2734 | `	/* Perform a string cast */` |
|        3 | 2735 | `	PH7_MemObjToString(pKey);` |
|        3 | 2736 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 2737 | `		/* Unavailable variable name */` |
|      ! 0 | 2738 | `		return SXRET_OK;` |
|        - | 2739 | `	}` |
|        3 | 2740 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 2741 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 2742 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 2743 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 2744 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 2745 | `			);` |
|        2 | 2746 | `	}else{` |
|      ! 0 | 2747 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 2748 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 2749 | `	}` |
|        3 | 2750 | `	sVar.zString = pAux->zWorker;` |
|        - | 2751 | `	/* Extract the variable */` |
|        3 | 2752 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 2753 | `	if( pObj ){` |
|        3 | 2754 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 2755 | `	}` |
|        3 | 2756 | `	return SXRET_OK;` |
|        2 | 2757 | `}` |
|        - | 2758 | `/*` |
|        - | 2759 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 2760 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 2761 | ` * Parameters` |
|        - | 2762 | ` * $types` |
|        - | 2763 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 2764 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 2765 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 2766 | ` *  POST includes the POST uploaded file information.` |
|        - | 2767 | ` *  Note:` |
|        - | 2768 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 2769 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 2770 | ` * $prefix` |
|        - | 2771 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 2772 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 2773 | ` *  variable named $pref_userid.` |
|        - | 2774 | ` * Return` |
|        - | 2775 | ` *  TRUE on success or FALSE on failure.` |
|        - | 2776 | ` */` |
|        2 | 2777 | `PH7_PRIVATE int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 2778 | `{` |
|        - | 2779 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 2780 | `	extract_aux_data sAux;` |
|        - | 2781 | `	int nLen,nPrefixLen;` |
|        - | 2782 | `	ph7_value *pSuper;` |
|        - | 2783 | `	ph7_vm *pVm;` |
|        - | 2784 | `	/* By default import only $_GET variables  */` |
|        3 | 2785 | `	zImport = "G";` |
|        3 | 2786 | `	nLen = (int)sizeof(char);` |
|        3 | 2787 | `	zPrefix = 0;` |
|        3 | 2788 | `	nPrefixLen = 0;` |
|        3 | 2789 | `	if( nArg > 0 ){` |
|        3 | 2790 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 2791 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 2792 | `		}` |
|        3 | 2793 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 2794 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 2795 | `		}` |
|        1 | 2796 | `	}` |
|        - | 2797 | `	/* Point to the underlying VM */` |
|        3 | 2798 | `	pVm = pCtx->pVm;` |
|        - | 2799 | `	/* Initialize the aux data */` |
|        3 | 2800 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 2801 | `	sAux.zPrefix = zPrefix;` |
|        3 | 2802 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 2803 | `	sAux.pVm = pVm;` |
|        - | 2804 | `	/* Extract */` |
|        3 | 2805 | `	zEnd = &zImport[nLen];` |
|        5 | 2806 | `	while( zImport < zEnd ){` |
|        3 | 2807 | `		int c = zImport[0];` |
|        3 | 2808 | `		pSuper = 0;` |
|        3 | 2809 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 2810 | `			/* Import $_GET variables */` |
|        3 | 2811 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 2812 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 2813 | `			/* Import $_POST variables */` |
|      ! 0 | 2814 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 2815 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 2816 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 2817 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 2818 | `		}` |
|        3 | 2819 | `		if( pSuper ){` |
|        - | 2820 | `			/* Iterate throw array entries */` |
|        3 | 2821 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 2822 | `		}` |
|        - | 2823 | `		/* Advance the cursor */` |
|        3 | 2824 | `		zImport++;` |
|        1 | 2825 | `	}` |
|        - | 2826 | `	/* All done,return TRUE*/` |
|        3 | 2827 | `	ph7_result_bool(pCtx,0);` |
|        3 | 2828 | `	return PH7_OK;` |
|        1 | 2829 | `}` |
|        - | 2830 |  |
