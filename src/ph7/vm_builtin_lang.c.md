# src/ph7/vm_builtin_lang.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1243/1431 lines (86.86%)

[Root index](../../index.md) | [Directory index](index.md)

|      Hits | Line | Source |
| --------: | ---: | :--- |
|         - |    1 | `/**` |
|         - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|         - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|         - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|         - |    5 | ` */` |
|         - |    6 | `#include "ph7int.h"` |
|         - |    7 | `#if defined(__UNIXES__)` |
|         - |    8 | `/* usleep(), which uniqid() spends a microsecond in so two ids cannot collide --` |
|         - |    9 | ` * php's own POSIX-only guarantee. */` |
|         - |   10 | `#include <unistd.h>` |
|         - |   11 | `#endif` |
|         - |   12 | `/*` |
|         - |   13 | ` * Section:` |
|         - |   14 | ` *    Language-level builtins: define/defined/constant and the enum` |
|         - |   15 | ` *    helpers, the rand/random_* family, echo/print/exit, version and` |
|         - |   16 | ` *    credits, parse_url, compact/extract and import_request_variables.` |
|         - |   17 | ` *    Registration rows stay in vm.c's aVmFunc[].` |
|         - |   18 | ` * Status:` |
|         - |   19 | ` *    Stable.` |
|         - |   20 | ` */` |
|         - |   21 | `/*` |
|         - |   22 | ` * What a "C::K" constant NAME resolved to. php's defined() and constant() ask the same` |
|         - |   23 | ` * question of the same string and only differ in how they REPORT the answer — defined()` |
|         - |   24 | `` * turns every miss into `false`, constant() into a catchable Error — so the resolution`` |
|         - |   25 | ` * itself lives here once.` |
|         - |   26 | ` */` |
|         - |   27 | `#define VM_CCONST_PLAIN     0 /* no "::" in the name: a global constant, not this form */` |
|         - |   28 | `#define VM_CCONST_OK        1 /* class and constant found, and visible from here */` |
|         - |   29 | `#define VM_CCONST_NOCLASS   2 /* the class part names nothing (autoload already tried) */` |
|         - |   30 | `#define VM_CCONST_NOSCOPE   3 /* self/parent/static named with no class scope active */` |
|         - |   31 | `#define VM_CCONST_NOCONST   4 /* the class exists but declares no such constant */` |
|         - |   32 | `#define VM_CCONST_NOACCESS  5 /* it exists but is private/protected out of scope */` |
|         - |   33 | ``#define VM_CCONST_NOPARENT  6 /* `parent` named from a class that has none */`` |
|         - |   34 | `#define VM_CCONST_TRAIT     7 /* a TRAIT constant, which php refuses to hand out through the trait */` |
|         - |   35 | `/*` |
|         - |   36 | ` * Split "C::K" and resolve both halves. The class half goes through` |
|         - |   37 | `` * PH7_VmResolveScopeName, so `self`/`parent`/`static` answer against the live class`` |
|         - |   38 | ` * context and a plain name AUTOLOADS on a miss (php does both here). The constant half` |
|         - |   39 | ` * is looked up without evaluating anything: an unmaterialized enum case or an on-demand` |
|         - |   40 | ` * constant initializer must not run just because someone ASKED whether the name exists.` |
|         - |   41 | ` * The class name is case-insensitive and the constant name is not, exactly as php.` |
|         - |   42 | ` */` |
|      3564 |   43 | `static int VmClassConstLookup(` |
|         - |   44 | `	ph7_vm *pVm,             /* Target VM */` |
|         - |   45 | `	const char *zName,       /* Constant name, possibly of the "C::K" form */` |
|         - |   46 | `	int nLen,                /* zName length */` |
|         - |   47 | `	ph7_class **ppClass,     /* OUT: resolved class (may be 0) */` |
|         - |   48 | `	ph7_class_attr **ppAttr, /* OUT: resolved constant (may be 0) */` |
|         - |   49 | `	int *pSep                /* OUT: offset of the "::" separator */` |
|         - |   50 | `	)` |
|         5 |   51 | `{` |
|         - |   52 | `	ph7_class_attr *pAttr;` |
|         - |   53 | `	ph7_class *pClass;` |
|         - |   54 | `	int iSep;` |
|      3569 |   55 | `	*ppClass = 0;` |
|      3569 |   56 | `	*ppAttr = 0;` |
|     69239 |   57 | `	for( iSep = 0; iSep + 1 < nLen; iSep++ ){` |
|     65885 |   58 | `		if( zName[iSep] == ':' && zName[iSep+1] == ':' ){` |
|       214 |   59 | `			break;` |
|         - |   60 | `		}` |
|     32840 |   61 | `	}` |
|      3569 |   62 | `	if( iSep + 1 >= nLen ){` |
|      3359 |   63 | `		return VM_CCONST_PLAIN;` |
|         - |   64 | `	}` |
|       214 |   65 | `	*pSep = iSep;` |
|       214 |   66 | `	pClass = iSep > 0 ? PH7_VmResolveScopeName(&(*pVm),zName,(sxu32)iSep) : 0;` |
|       214 |   67 | `	if( pClass == 0 ){` |
|        32 |   68 | `		if( iSep > 0 && PH7_VmIsScopeKeyword(zName,(sxu32)iSep) ){` |
|         - |   69 | `			/* php separates the two ways a keyword can fail to resolve, so tell them` |
|         - |   70 | ``			 * apart here: `parent` inside a class that simply has no parent is a`` |
|         - |   71 | `			 * different sentence from a keyword named with no class scope at all. */` |
|        16 |   72 | `			if( iSep == 6 && SyMemcmp(zName,"parent",6) == 0` |
|        10 |   73 | `			 && (PH7_VmPeekTopClass(&(*pVm)) \|\| PH7_VmPeekDeclaringClass(&(*pVm))) ){` |
|         3 |   74 | `				return VM_CCONST_NOPARENT;` |
|         - |   75 | `			}` |
|        16 |   76 | `			return VM_CCONST_NOSCOPE;` |
|         - |   77 | `		}` |
|        15 |   78 | `		return VM_CCONST_NOCLASS;` |
|         - |   79 | `	}` |
|       186 |   80 | `	*ppClass = pClass;` |
|       186 |   81 | `	if( iSep + 2 >= nLen ){` |
|         6 |   82 | `		return VM_CCONST_NOCONST; /* "C::" names no constant */` |
|         - |   83 | `	}` |
|         - |   84 | `	/* This form names a class CONSTANT or an enum case (hConst), never a property. */` |
|       182 |   85 | `	pAttr = PH7_ClassExtractConstant(pClass,&zName[iSep+2],(sxu32)(nLen - iSep - 2));` |
|       182 |   86 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        30 |   87 | `		return VM_CCONST_NOCONST;` |
|         - |   88 | `	}` |
|       156 |   89 | `	if( (pClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|         - |   90 | `		/* A trait constant belongs to the classes that COMPOSE the trait: php refuses` |
|         - |   91 | ``		 * `constant("T::K")` outright, and answers `defined("T::K")` with false. */`` |
|         3 |   92 | `		*ppAttr = pAttr;` |
|         3 |   93 | `		return VM_CCONST_TRAIT;` |
|         - |   94 | `	}` |
|       150 |   95 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|        90 |   96 | `	 && pAttr->pDeclClass && pAttr->pDeclClass != pClass ){` |
|         - |   97 | `		/* php does not put a private constant in a SUBCLASS's table at all, so naming it` |
|         - |   98 | ``		 * through the child is not an access denial but a plain miss — `Sub::P` reports`` |
|         - |   99 | ``		 * `Undefined constant Sub::P` even from inside the declaring class, and`` |
|         - |  100 | `		 * defined("Sub::P") is false there too. Only the declaring class can answer. */` |
|       ! 0 |  101 | `		return VM_CCONST_NOCONST;` |
|         - |  102 | `	}` |
|       154 |  103 | `	*ppAttr = pAttr;` |
|         - |  104 | ``	/* php answers by the CALLING scope, the same rule the direct `C::K` access uses:`` |
|         - |  105 | `	 * a private constant is invisible from outside its declaring class even to a` |
|         - |  106 | `	 * subclass, and a protected one is visible down the hierarchy. */` |
|       154 |  107 | `	if( !PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        23 |  108 | `		return VM_CCONST_NOACCESS;` |
|         - |  109 | `	}` |
|       134 |  110 | `	return VM_CCONST_OK;` |
|      1787 |  111 | `}` |
|         - |  112 | `/*` |
|         - |  113 | ` * Raise the Error php raises for a "C::K" name that did not resolve. php prints the class` |
|         - |  114 | `` * part exactly as the caller WROTE it — `c::Q`, `self::P`, `parent::P` — rather than the`` |
|         - |  115 | ` * canonical class name, so the message quotes the source span. Never called with` |
|         - |  116 | ` * VM_CCONST_OK or VM_CCONST_PLAIN.` |
|         - |  117 | ` */` |
|        48 |  118 | `static int VmClassConstError(` |
|         - |  119 | `	ph7_context *pCtx,      /* Call context */` |
|         - |  120 | `	int rc,                 /* VmClassConstLookup() verdict */` |
|         - |  121 | `	const char *zName,      /* The whole "C::K" name */` |
|         - |  122 | `	int nLen,               /* zName length */` |
|         - |  123 | `	int iSep,               /* Offset of the "::" */` |
|         - |  124 | `	ph7_class_attr *pAttr   /* The constant, when one was found */` |
|         - |  125 | `	)` |
|         3 |  126 | `{` |
|        51 |  127 | `	if( nLen > 0 && zName[0] == '\\' ){` |
|         - |  128 | `` 		/* The global-namespace anchor is not part of the name php echoes back: `\C::P` `` |
|         - |  129 | ``		 * reports `C::P` (exactly ONE leading backslash goes, the rest stays). */`` |
|         3 |  130 | `		zName++;` |
|         3 |  131 | `		nLen--;` |
|         3 |  132 | `		iSep--;` |
|         1 |  133 | `	}` |
|        51 |  134 | `	switch( rc ){` |
|         3 |  135 | `		case VM_CCONST_NOCLASS:` |
|         8 |  136 | `			return PH7_VmThrowException(pCtx,"Error","Class \"%.*s\" not found",iSep,zName);` |
|         7 |  137 | `		case VM_CCONST_NOSCOPE:` |
|        23 |  138 | `			return PH7_VmThrowException(pCtx,"Error",` |
|         7 |  139 | `				"Cannot access \"%.*s\" when no class scope is active",iSep,zName);` |
|         1 |  140 | `		case VM_CCONST_NOPARENT:` |
|         3 |  141 | `			return PH7_VmThrowException(pCtx,"Error",` |
|         - |  142 | `				"Cannot access \"parent\" when current class scope has no parent");` |
|         6 |  143 | `		case VM_CCONST_NOACCESS:` |
|        25 |  144 | `			return PH7_VmThrowException(pCtx,"Error","Cannot access %s constant %.*s",` |
|        12 |  145 | `				(pAttr && pAttr->iProtection == PH7_CLASS_PROT_PRIVATE) ? "private" : "protected",` |
|         6 |  146 | `				nLen,zName);` |
|       ! 0 |  147 | `		case VM_CCONST_TRAIT:` |
|       ! 0 |  148 | `			return PH7_VmThrowException(pCtx,"Error",` |
|       ! 0 |  149 | `				"Cannot access trait constant %.*s directly",nLen,zName);` |
|         7 |  150 | `		default:` |
|        14 |  151 | `			break;` |
|         - |  152 | `	}` |
|        16 |  153 | `	return PH7_VmThrowException(pCtx,"Error","Undefined constant %.*s",nLen,zName);` |
|        27 |  154 | `}` |
|         - |  155 | `/*` |
|         - |  156 | ` * bool defined(string $name)` |
|         - |  157 | ` *  Checks whether a given named constant exists.` |
|         - |  158 | ` * Parameter:` |
|         - |  159 | ` *  Name of the desired constant.` |
|         - |  160 | ` * Return` |
|         - |  161 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|         - |  162 | ` */` |
|      1690 |  163 | `PH7_PRIVATE int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 |  164 | `{` |
|         - |  165 | `	ph7_class_attr *pAttr;` |
|         - |  166 | `	ph7_class *pClass;` |
|         - |  167 | `	const char *zName;` |
|      1695 |  168 | `	int nLen = 0;` |
|      1695 |  169 | `	int iSep = 0;` |
|      1695 |  170 | `	int res = 0;` |
|      1695 |  171 | `	if( nArg < 1 ){` |
|         - |  172 | `		/* Missing constant name,return FALSE */` |
|       ! 0 |  173 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|       ! 0 |  174 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 |  175 | `		return SXRET_OK;` |
|         - |  176 | `	}` |
|         - |  177 | `	/* Extract constant name */` |
|      1695 |  178 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|         - |  179 | `	/* Class-constant form "C::K": every miss — unknown class, unknown constant, or one` |
|         - |  180 | `	 * that is not visible from here — is a plain FALSE, since asking whether a name is` |
|         - |  181 | `	 * defined is exactly what defined() is for (this used to consult the` |
|         - |  182 | `	 * global constant table only, so EVERY class constant answered false while` |
|         - |  183 | `	 * constant() read the same name correctly). */` |
|      1695 |  184 | `	if( nLen > 0 ){` |
|      1695 |  185 | `		int iRc = VmClassConstLookup(pCtx->pVm,zName,nLen,&pClass,&pAttr,&iSep);` |
|      1695 |  186 | `		switch( iRc ){` |
|       806 |  187 | `			case VM_CCONST_PLAIN:` |
|      1617 |  188 | `				break;` |
|        18 |  189 | `			case VM_CCONST_OK:` |
|        38 |  190 | `				ph7_result_bool(pCtx,1);` |
|        38 |  191 | `				return SXRET_OK;` |
|         5 |  192 | `			case VM_CCONST_NOSCOPE:` |
|         - |  193 | `			case VM_CCONST_NOPARENT:` |
|         - |  194 | ``				/* php refuses the question rather than answering it: naming `self` where`` |
|         - |  195 | ``				 * no class scope is active is an Error, not a `false`. Every OTHER miss`` |
|         - |  196 | `				 * is a false, so only these two reach the shared thrower. */` |
|        11 |  197 | `				return VmClassConstError(pCtx,iRc,zName,nLen,iSep,pAttr);` |
|        16 |  198 | `			default:` |
|        35 |  199 | `				ph7_result_bool(pCtx,0);` |
|        35 |  200 | `				return SXRET_OK;` |
|         - |  201 | `		}` |
|       806 |  202 | `	}` |
|         - |  203 | `	/* Perform the lookup */` |
|      1617 |  204 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|         - |  205 | `		/* Already defined */` |
|      1601 |  206 | `		res = 1;` |
|       797 |  207 | `	}` |
|      1617 |  208 | `	ph7_result_bool(pCtx,res);` |
|      1617 |  209 | `	return SXRET_OK;` |
|       850 |  210 | `}` |
|         - |  211 | `/*` |
|         - |  212 | ` * Constant expansion callback used by the [define()] function defined` |
|         - |  213 | ` * below.` |
|         - |  214 | ` */` |
|    102246 |  215 | `PH7_PRIVATE void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|         5 |  216 | `{` |
|    102251 |  217 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|         - |  218 | `	/* Expand constant value */` |
|    102251 |  219 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|    102251 |  220 | `}` |
|         - |  221 | `/*` |
|         - |  222 | ` * bool define(string $constant_name,expression value)` |
|         - |  223 | ` *  Defines a named constant at runtime.` |
|         - |  224 | ` * Parameter:` |
|         - |  225 | ` *  $constant_name` |
|         - |  226 | ` *   The name of the constant` |
|         - |  227 | ` *  $value` |
|         - |  228 | ` *   Constant value` |
|         - |  229 | ` * Return:` |
|         - |  230 | ` *   TRUE on success,FALSE on failure.` |
|         - |  231 | ` */` |
|       160 |  232 | `PH7_PRIVATE int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 |  233 | `{` |
|         - |  234 | `	const char *zName;  /* Constant name */` |
|         - |  235 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       165 |  236 | `	int nLen = 0;       /* Name length */` |
|         - |  237 | `	sxi32 rc;` |
|       165 |  238 | `	if( nArg < 2 ){` |
|         - |  239 | `		/* Missing arguments,throw a ntoice and return false */` |
|       ! 0 |  240 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|       ! 0 |  241 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 |  242 | `		return SXRET_OK;` |
|         - |  243 | `	}` |
|       165 |  244 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|       ! 0 |  245 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|       ! 0 |  246 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 |  247 | `		return SXRET_OK;` |
|         - |  248 | `	}` |
|         - |  249 | `	/* Extract constant name */` |
|       165 |  250 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       165 |  251 | `	if( nLen < 1 ){` |
|       ! 0 |  252 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|       ! 0 |  253 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 |  254 | `		return SXRET_OK;` |
|         - |  255 | `	}` |
|         - |  256 | `	/* Duplicate constant value */` |
|       165 |  257 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       165 |  258 | `	if( pValue == 0 ){` |
|       ! 0 |  259 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|       ! 0 |  260 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 |  261 | `		return SXRET_OK;` |
|         - |  262 | `	}` |
|         - |  263 | `	/* Initialize the memory object */` |
|       165 |  264 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|         - |  265 | `	/* Register the constant */` |
|         - |  266 | `	{` |
|         - |  267 | `		SyString sConsName;` |
|       165 |  268 | `		SyStringInitFromBuf(&sConsName,zName,(sxu32)nLen);` |
|       245 |  269 | `		rc = PH7_VmRegisterConstantEx(pCtx->pVm,&sConsName,VmExpandUserConstant,pValue,` |
|       160 |  270 | `			(SyString *)SySetPeek(&pCtx->pVm->aFiles),0,1);` |
|         - |  271 | `	}` |
|       165 |  272 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  273 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|       ! 0 |  274 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|       ! 0 |  275 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 |  276 | `		return SXRET_OK;` |
|         - |  277 | `	}` |
|         - |  278 | `	/* Duplicate constant value */` |
|       165 |  279 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       165 |  280 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|         - |  281 | `		/* Lower case the constant name */` |
|       ! 0 |  282 | `		char *zCur = (char *)zName;` |
|       ! 0 |  283 | `		while( zCur < &zName[nLen] ){` |
|       ! 0 |  284 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|         - |  285 | `				/* UTF-8 stream */` |
|       ! 0 |  286 | `				zCur++;` |
|       ! 0 |  287 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|       ! 0 |  288 | `					zCur++;` |
|       ! 0 |  289 | `				}` |
|       ! 0 |  290 | `				continue;` |
|         - |  291 | `			}` |
|       ! 0 |  292 | `			if( SyisUpper(zCur[0]) ){` |
|       ! 0 |  293 | `				int c = SyToLower(zCur[0]);` |
|       ! 0 |  294 | `				zCur[0] = (char)c;` |
|       ! 0 |  295 | `			}` |
|       ! 0 |  296 | `			zCur++;` |
|       ! 0 |  297 | `		}` |
|         - |  298 | `		/* Register the lowercase alias with its OWN value copy (not the same` |
|         - |  299 | `		 * pValue) so the two entries don't share one object — otherwise freeing` |
|         - |  300 | `		 * one on a later overwrite would dangle the other. */` |
|         - |  301 | `		{` |
|       ! 0 |  302 | `			ph7_value *pAlias = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       ! 0 |  303 | `			if( pAlias ){` |
|       ! 0 |  304 | `				PH7_MemObjInit(pCtx->pVm,pAlias);` |
|       ! 0 |  305 | `				PH7_MemObjStore(apArg[1],pAlias);` |
|       ! 0 |  306 | `				ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pAlias);` |
|       ! 0 |  307 | `			}` |
|         - |  308 | `		}` |
|       ! 0 |  309 | `	}` |
|         - |  310 | `	/* All done,return TRUE */` |
|       165 |  311 | `	ph7_result_bool(pCtx,1);` |
|       165 |  312 | `	return SXRET_OK;` |
|        85 |  313 | `}` |
|         - |  314 | `/*` |
|         - |  315 | ` * value constant(string $name)` |
|         - |  316 | ` *  Returns the value of a constant` |
|         - |  317 | ` * Parameter` |
|         - |  318 | ` *  $name` |
|         - |  319 | ` *    Name of the constant.` |
|         - |  320 | ` * Return` |
|         - |  321 | ` *  Constant value or NULL if not defined.` |
|         - |  322 | ` */` |
|         - |  323 | `/*` |
|         - |  324 | ` * Enum method thunks (PHP 8.1). Every enum's synthesized cases()/from()/` |
|         - |  325 | ` * tryFrom() methods (GenStateCompileEnumMethods, compile.c) forward here with` |
|         - |  326 | ` * the enum's FQN as a literal first argument — the same forwarder pattern the` |
|         - |  327 | ` * Generator/Fiber/Reflection builtins use.` |
|         - |  328 | ` */` |
|         - |  329 | `/* array __phl_enum_cases(string $enumFqn) — declaration-order case list */` |
|        21 |  330 | `PH7_PRIVATE int vm_builtin_enum_cases(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 |  331 | `{` |
|        22 |  332 | `	ph7_vm *pVm = pCtx->pVm;` |
|         - |  333 | `	ph7_class_attr **apCase;` |
|         - |  334 | `	ph7_class *pClass;` |
|         - |  335 | `	ph7_value *pArray;` |
|         - |  336 | `	sxu32 n;` |
|         - |  337 | `	sxi32 rc;` |
|        22 |  338 | `	if( nArg < 1 \|\| (pClass = VmExtractEnumClass(pVm,apArg[0])) == 0 ){` |
|       ! 0 |  339 | `		ph7_result_null(pCtx);` |
|       ! 0 |  340 | `		return SXRET_OK;` |
|         - |  341 | `	}` |
|        22 |  342 | `	rc = VmEnumMaterialize(pVm,pClass);` |
|        22 |  343 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  344 | `		return rc;` |
|         - |  345 | `	}` |
|        22 |  346 | `	pArray = ph7_context_new_array(pCtx);` |
|        22 |  347 | `	if( pArray == 0 ){` |
|       ! 0 |  348 | `		ph7_result_null(pCtx);` |
|       ! 0 |  349 | `		return SXRET_OK;` |
|         - |  350 | `	}` |
|        22 |  351 | `	apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|        89 |  352 | `	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|        68 |  353 | `		ph7_value *pSlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,apCase[n]->nIdx);` |
|        68 |  354 | `		if( pSlot ){` |
|        68 |  355 | `			ph7_array_add_elem(pArray,0,pSlot); /* Copies; the object ref is retained */` |
|        31 |  356 | `		}` |
|        32 |  357 | `	}` |
|        22 |  358 | `	ph7_result_value(pCtx,pArray);` |
|        22 |  359 | `	return SXRET_OK;` |
|        11 |  360 | `}` |
|         - |  361 | `/*` |
|         - |  362 | `` * php declares from()/tryFrom() as `string\|int $value` on the BackedEnum`` |
|         - |  363 | ` * prototype, so the argument arrives in either form and the enum's own backing` |
|         - |  364 | ` * type decides what happens next -- which is why the refusal is worded against` |
|         - |  365 | ` * the BACKING type ("must be of type int, string given" for an int-backed enum` |
|         - |  366 | ` * given "2x") and not against the declared union. php words the union only when` |
|         - |  367 | ` * the value is neither a string nor an int and the enum is string-backed; the` |
|         - |  368 | ` * asymmetry is php's own.` |
|         - |  369 | ` *` |
|         - |  370 | ` * Everything else is ordinary weak coercion, so an int-backed enum accepts "02"` |
|         - |  371 | ` * and " 2" as 2, and a string-backed one takes an int (or a bool, or a` |
|         - |  372 | ` * non-lossy float) through the INT arm first: S::from(1.0) looks for "1", not` |
|         - |  373 | ` * "1.0", and S::from(false) for "0". A LOSSY float and a null are php` |
|         - |  374 | ` * DEPRECATIONS, so PH7_IntArgResolve refuses them (the scope policy scope policy) with the` |
|         - |  375 | ` * TypeError php will eventually raise.` |
|         - |  376 | ` */` |
|       178 |  377 | `static sxi32 VmEnumCoerceNeedle(ph7_context *pCtx,ph7_class *pClass,ph7_value *pArg,` |
|         - |  378 | `	ph7_value *pOut)` |
|         2 |  379 | `{` |
|       180 |  380 | `	int bStrBacked = pClass->nEnumBacking != MEMOBJ_INT;` |
|       180 |  381 | `	sxi64 iVal = 0;` |
|         - |  382 | `	sxi32 rc;` |
|       180 |  383 | `	PH7_MemObjInit(pCtx->pVm,pOut);` |
|       180 |  384 | `	if( ph7_value_is_string(pArg) && bStrBacked ){` |
|        72 |  385 | `		PH7_MemObjLoad(pArg,pOut);` |
|        72 |  386 | `		return SXRET_OK;` |
|         - |  387 | `	}` |
|         - |  388 | `	/* A native method's own name is already qualified ("I2::from"). */` |
|       163 |  389 | `	rc = PH7_IntArgResolve(pCtx,pArg,ph7_function_name(pCtx),1,"$value",` |
|        54 |  390 | `		bStrBacked ? "string\|int" : "int",&iVal);` |
|       109 |  391 | `	if( rc != PH7_OK ){` |
|        13 |  392 | `		return rc;` |
|         - |  393 | `	}` |
|        97 |  394 | `	if( bStrBacked ){` |
|         - |  395 | `		char zNum[32];` |
|        35 |  396 | `		int nNum = SyBufferFormat(zNum,sizeof(zNum),"%qd",iVal);` |
|        35 |  397 | `		PH7_MemObjStringAppend(pOut,zNum,(sxu32)nNum);` |
|        18 |  398 | `	}else{` |
|        63 |  399 | `		pOut->x.iVal = iVal;` |
|        63 |  400 | `		MemObjSetType(pOut,MEMOBJ_INT);` |
|         - |  401 | `	}` |
|        97 |  402 | `	return SXRET_OK;` |
|        91 |  403 | `}` |
|         - |  404 | `/* Shared scan for from()/tryFrom(): return the slot of the case whose backing` |
|         - |  405 | ` * value equals *pNeedle (already coerced to the backing type), or 0 on miss. */` |
|       166 |  406 | `static ph7_value * VmEnumFindCaseByValue(ph7_vm *pVm,ph7_class *pClass,ph7_value *pNeedle)` |
|         2 |  407 | `{` |
|       168 |  408 | `	ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|         - |  409 | `	sxu32 n;` |
|       384 |  410 | `	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|       298 |  411 | `		ph7_value *pVal = VmEnumCaseBackingValue(pVm,apCase[n]);` |
|       298 |  412 | `		int bMatch = 0;` |
|       298 |  413 | `		if( pVal ){` |
|       298 |  414 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|       101 |  415 | `				bMatch = (pNeedle->iFlags & MEMOBJ_INT) && pVal->x.iVal == pNeedle->x.iVal;` |
|        51 |  416 | `			}else{` |
|       296 |  417 | `				bMatch = (pNeedle->iFlags & MEMOBJ_STRING)` |
|       196 |  418 | `					&& SyBlobLength(&pVal->sBlob) == SyBlobLength(&pNeedle->sBlob)` |
|       294 |  419 | `					&& SyMemcmp(SyBlobData(&pVal->sBlob),SyBlobData(&pNeedle->sBlob),` |
|       126 |  420 | `						SyBlobLength(&pNeedle->sBlob)) == 0;` |
|         - |  421 | `			}` |
|       148 |  422 | `		}` |
|       298 |  423 | `		if( bMatch ){` |
|        82 |  424 | `			return (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,apCase[n]->nIdx);` |
|         - |  425 | `		}` |
|       109 |  426 | `	}` |
|        87 |  427 | `	return 0;` |
|        85 |  428 | `}` |
|         - |  429 | `/* static from(int\|string $value) / static tryFrom(int\|string $value) */` |
|       178 |  430 | `static int VmEnumFromCommon(ph7_context *pCtx,int nArg,ph7_value **apArg,int bTry)` |
|         2 |  431 | `{` |
|       180 |  432 | `	ph7_vm *pVm = pCtx->pVm;` |
|         - |  433 | `	ph7_class *pClass;` |
|         - |  434 | `	ph7_value *pFound;` |
|         - |  435 | `	ph7_value sNeedle;` |
|         - |  436 | `	sxi32 rc;` |
|       180 |  437 | `	if( nArg < 2 \|\| (pClass = VmExtractEnumClass(pVm,apArg[0])) == 0 ){` |
|       ! 0 |  438 | `		ph7_result_null(pCtx);` |
|       ! 0 |  439 | `		return SXRET_OK;` |
|         - |  440 | `	}` |
|       180 |  441 | `	rc = VmEnumMaterialize(pVm,pClass);` |
|       180 |  442 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  443 | `		return rc;` |
|         - |  444 | `	}` |
|       180 |  445 | `	rc = VmEnumCoerceNeedle(pCtx,pClass,apArg[1],&sNeedle);` |
|       180 |  446 | `	if( rc != SXRET_OK ){` |
|        13 |  447 | `		PH7_MemObjRelease(&sNeedle);` |
|        13 |  448 | `		return rc;` |
|         - |  449 | `	}` |
|       168 |  450 | `	pFound = VmEnumFindCaseByValue(pVm,pClass,&sNeedle);` |
|       168 |  451 | `	if( pFound ){` |
|        82 |  452 | `		ph7_result_value(pCtx,pFound);` |
|        82 |  453 | `		PH7_MemObjRelease(&sNeedle);` |
|        82 |  454 | `		return SXRET_OK;` |
|         - |  455 | `	}` |
|        87 |  456 | `	if( bTry ){` |
|        47 |  457 | `		ph7_result_null(pCtx);` |
|        47 |  458 | `		PH7_MemObjRelease(&sNeedle);` |
|        47 |  459 | `		return SXRET_OK;` |
|         - |  460 | `	}` |
|        41 |  461 | `	if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|         - |  462 | `		char zVal[32];` |
|         9 |  463 | `		SyBufferFormat(zVal,sizeof(zVal),"%qd",sNeedle.x.iVal);` |
|        13 |  464 | `		rc = PH7_VmThrowException(pCtx,"ValueError",` |
|         4 |  465 | `			"%s is not a valid backing value for enum %z",zVal,&pClass->sDisp);` |
|         5 |  466 | `	}else{` |
|        49 |  467 | `		rc = PH7_VmThrowException(pCtx,"ValueError",` |
|         - |  468 | `			"\"%.*s\" is not a valid backing value for enum %z",` |
|        32 |  469 | `			(int)SyBlobLength(&sNeedle.sBlob),(const char *)SyBlobData(&sNeedle.sBlob),` |
|        16 |  470 | `			&pClass->sDisp);` |
|         - |  471 | `	}` |
|        41 |  472 | `	PH7_MemObjRelease(&sNeedle);` |
|        41 |  473 | `	return rc;` |
|        91 |  474 | `}` |
|        96 |  475 | `PH7_PRIVATE int vm_builtin_enum_from(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 |  476 | `{` |
|        98 |  477 | `	return VmEnumFromCommon(pCtx,nArg,apArg,FALSE);` |
|         2 |  478 | `}` |
|        82 |  479 | `PH7_PRIVATE int vm_builtin_enum_tryfrom(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 |  480 | `{` |
|        83 |  481 | `	return VmEnumFromCommon(pCtx,nArg,apArg,TRUE);` |
|         1 |  482 | `}` |
|         - |  483 | `/*` |
|         - |  484 | ` * bool enum_exists(string $enum, bool $autoload = true)` |
|         - |  485 | ` *  TRUE only for a declared enum (PHP 8.1); a plain class/interface is FALSE.` |
|         - |  486 | ` *` |
|         - |  487 | ` * $autoload was declared and never read: VmExtractEnumClass resolves through` |
|         - |  488 | ``  * PH7_VmExtractClass, which always asks the autoloader, so `enum_exists($n,false)` `` |
|         - |  489 | ` * ran the loader anyway. That is the one spelling a program uses to ask "is this` |
|         - |  490 | ` * already loaded" WITHOUT paying for a load, and its whole point is the refusal.` |
|         - |  491 | ` */` |
|        31 |  492 | `PH7_PRIVATE int vm_builtin_enum_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 |  493 | `{` |
|        34 |  494 | `	ph7_class *pClass = 0;` |
|        34 |  495 | `	if( nArg > 0 ){` |
|        39 |  496 | `		if( nArg >= 2 && !ph7_value_to_bool(apArg[1]) ){` |
|         - |  497 | `			/* Declared-only lookup: probe the class table directly, then filter` |
|         - |  498 | `			 * the same-name chain down to an enum exactly as VmExtractEnumClass does. */` |
|         - |  499 | `			const char *zName;` |
|         - |  500 | `			int nLen;` |
|         - |  501 | `			sxu32 nName;` |
|        11 |  502 | `			zName = ph7_value_to_string(apArg[0],&nLen);` |
|        11 |  503 | `			nName = (sxu32)nLen;` |
|        11 |  504 | `			PH7_VmClassNameAnchor(&zName,&nName);` |
|        11 |  505 | `			if( nName > 0 ){` |
|        11 |  506 | `				SyHashEntry *pEntry = SyHashGet(&pCtx->pVm->hClass,(const void *)zName,nName);` |
|        11 |  507 | `				if( pEntry ){` |
|         7 |  508 | `					pClass = (ph7_class *)pEntry->pUserData;` |
|         9 |  509 | `					while( pClass && (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|         3 |  510 | `						pClass = pClass->pNextName;` |
|         1 |  511 | `					}` |
|         3 |  512 | `				}` |
|         5 |  513 | `			}` |
|         6 |  514 | `		}else{` |
|        24 |  515 | `			pClass = VmExtractEnumClass(pCtx->pVm,apArg[0]);` |
|         - |  516 | `		}` |
|        15 |  517 | `	}` |
|        34 |  518 | `	ph7_result_bool(pCtx,pClass != 0);` |
|        34 |  519 | `	return SXRET_OK;` |
|         3 |  520 | `}` |
|      1874 |  521 | `PH7_PRIVATE int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 |  522 | `{` |
|         - |  523 | `	SyHashEntry *pEntry;` |
|         - |  524 | `	ph7_constant *pCons;` |
|         - |  525 | `	const char *zName; /* Constant name */` |
|         - |  526 | `	ph7_value sVal;    /* Constant value */` |
|         - |  527 | `	int nLen;` |
|      1878 |  528 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|         - |  529 | `		/* Invallid argument,return NULL */` |
|       ! 0 |  530 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|       ! 0 |  531 | `		ph7_result_null(pCtx);` |
|       ! 0 |  532 | `		return SXRET_OK;` |
|         - |  533 | `	}` |
|         - |  534 | `	/* Extract the constant name */` |
|      1878 |  535 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|         - |  536 | `	/* Class-constant form "C::K" (band A #4): resolve the class — interfaces` |
|         - |  537 | `	 * included — and read the mounted constant slot; php throws a catchable` |
|         - |  538 | `	 * Error for an unknown class or constant (pre-fix this path warned` |
|         - |  539 | `	 * "Undefined constant" and returned NULL without ever looking at the` |
|         - |  540 | `` 	 * class). The resolution is defined()'s: it also answers `self`/`parent`/`static` `` |
|         - |  541 | `	 * against the live class scope and refuses a constant that is not VISIBLE from` |
|         - |  542 | ``	 * here — both of which this used to walk straight past, so a `private const` was`` |
|         - |  543 | ``	 * readable from anywhere through the string form while the direct `C::K` access`` |
|         - |  544 | `	 * threw. */` |
|         - |  545 | `	{` |
|      1878 |  546 | `		ph7_class_attr *pAttr = 0;` |
|      1878 |  547 | `		ph7_class *pClass = 0;` |
|      1878 |  548 | `		int iSep = 0;` |
|      1878 |  549 | `		int iRc = VmClassConstLookup(pCtx->pVm,zName,nLen,&pClass,&pAttr,&iSep);` |
|      1878 |  550 | `		if( iRc != VM_CCONST_PLAIN ){` |
|       135 |  551 | `			if( iRc != VM_CCONST_OK ){` |
|        87 |  552 | `				return VmClassConstError(pCtx,iRc,zName,nLen,iSep,pAttr);` |
|         - |  553 | `			}` |
|        97 |  554 | `			if( pAttr->nIdx == SXU32_HIGH ){` |
|         - |  555 | `				/* Unmaterialized: enum case → materialize the singletons` |
|         - |  556 | `				 * (all of them: constant("S::A") is a direct access, like` |
|         - |  557 | `				 * OP_MEMBER); plain constant → run its initializer. Unlike` |
|         - |  558 | `				 * defined(), reading the value has to force this. */` |
|         - |  559 | `				sxi32 rcEnum;` |
|        45 |  560 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE ){` |
|         3 |  561 | `					rcEnum = VmEnumMaterialize(pCtx->pVm,pClass);` |
|         2 |  562 | `				}else{` |
|        43 |  563 | `					rcEnum = VmClassConstEvalOnDemand(pCtx->pVm,pClass,pAttr);` |
|         - |  564 | `				}` |
|        45 |  565 | `				if( rcEnum != SXRET_OK ){` |
|         3 |  566 | `					return rcEnum;` |
|         - |  567 | `				}` |
|        20 |  568 | `			}` |
|         - |  569 | `			{` |
|        94 |  570 | `				ph7_value *pValue = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj,pAttr->nIdx);` |
|        94 |  571 | `				if( pValue ){` |
|        94 |  572 | `					if( SySetUsed(&pAttr->aAttrs) > 0 ){` |
|         - |  573 | `						/* #[\Deprecated] warns through constant() too (php) */` |
|         3 |  574 | `						VmDeprecatedConstNotice(pCtx->pVm,pClass,pAttr);` |
|         1 |  575 | `					}` |
|        94 |  576 | `					ph7_result_value(pCtx,pValue);` |
|        94 |  577 | `					return SXRET_OK;` |
|         - |  578 | `				}` |
|         - |  579 | `			}` |
|         - |  580 | `			/* Declared but with no slot to read: the same dead end the pre-fix code` |
|         - |  581 | `			 * fell through to. */` |
|       ! 0 |  582 | `			return VmClassConstError(pCtx,VM_CCONST_NOCONST,zName,nLen,iSep,pAttr);` |
|         - |  583 | `		}` |
|         - |  584 | `	}` |
|         - |  585 | `	/* Perform the query */` |
|      1745 |  586 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|      1745 |  587 | `	if( pEntry == 0 ){` |
|         - |  588 | `		/* php 8: a catchable Error, not a notice + NULL (band A #4) */` |
|         8 |  589 | `		return PH7_VmThrowException(pCtx,"Error",` |
|         2 |  590 | `			"Undefined constant \"%.*s\"",nLen,zName);` |
|         - |  591 | `	}` |
|      1741 |  592 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|         - |  593 | `	/* Point to the structure that describe the constant */` |
|      1741 |  594 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|         - |  595 | `	/* Extract constant value by calling it's associated callback` |
|         - |  596 | `	 * (emits the #[\Deprecated] notice first when attributed, php) */` |
|      1741 |  597 | `	VmExpandConstantWithNotice(pCtx->pVm,pCons,&sVal);` |
|         - |  598 | `	/* Return that value */` |
|      1741 |  599 | `	ph7_result_value(pCtx,&sVal);` |
|         - |  600 | `	/* Cleanup */` |
|      1741 |  601 | `	PH7_MemObjRelease(&sVal);` |
|      1741 |  602 | `	return SXRET_OK;` |
|       941 |  603 | `}` |
|         - |  604 | `/*` |
|         - |  605 | ` * Hash walker callback used by the [get_defined_constants()] function defined` |
|         - |  606 | ` * below. php's answer is a MAP -- the constant's name is the KEY and its VALUE` |
|         - |  607 | ``  * is the element -- which is what makes `get_defined_constants()['PHP_EOL']` `` |
|         - |  608 | ` * the documented way to read one. PHL used to answer a LIST of names, so every` |
|         - |  609 | ``  * such lookup was an `Undefined array key` and NULL, and `in_array($n, $c)` `` |
|         - |  610 | `` * answered where php wants `isset($c[$n])`: the array had the right length and`` |
|         - |  611 | ` * the wrong shape.` |
|         - |  612 | ` */` |
|    137407 |  613 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|         3 |  614 | `{` |
|         - |  615 | ``	/* SNAPSHOT ONLY -- nothing is expanded during the walk. A `const` whose`` |
|         - |  616 | `	 * initializer is a bytecode program runs USER CODE when it expands, and user` |
|         - |  617 | ``	 * code can `define()`: that grows hConstant while SyHashForEach is holding a`` |
|         - |  618 | `	 * fixed entry count, and the walk then runs off the end of the bucket chain` |
|         - |  619 | `	 * (a segfault, reproducible from a const initializer that constructs an` |
|         - |  620 | `	 * object whose __construct defines a constant). Collect first, expand after. */` |
|    137410 |  621 | `	SySet *pOut = (SySet *)pUserData;` |
|    137410 |  622 | `	if( pEntry == 0 \|\| pEntry->pUserData == 0 ){` |
|       ! 0 |  623 | `		return SXRET_OK;` |
|         - |  624 | `	}` |
|    137410 |  625 | `	SySetPut(pOut,(const void *)&pEntry);` |
|    137410 |  626 | `	return SXRET_OK;` |
|     66729 |  627 | `}` |
|         - |  628 | `/*` |
|         - |  629 | ` * Add one snapshotted constant to the answer, under its name.` |
|         - |  630 | ` */` |
|    137407 |  631 | `static sxi32 VmConstDumpEntry(ph7_value *pTarget,SyHashEntry *pEntry)` |
|         3 |  632 | `{` |
|    137410 |  633 | `	ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|         - |  634 | `	ph7_value sName,sVal;` |
|         - |  635 | `	sxi32 rc;` |
|         - |  636 | `	/* Prepare the constant name for insertion */` |
|    137410 |  637 | `	PH7_MemObjInitFromString(pTarget->pVm,&sName,0);` |
|    137410 |  638 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|         - |  639 | `	/* ...and its VALUE, through the SAME evaluate-once path an ordinary read` |
|         - |  640 | ``	 * takes -- so a `const C = new Foo();` reported here is the object the`` |
|         - |  641 | ``	 * program itself sees, not a second one. The `#[\Deprecated]` NOTICE is what`` |
|         - |  642 | `	 * is skipped (VmExpandConstantOnce rather than …WithNotice): describing a` |
|         - |  643 | `	 * constant is not reading one, and php raises nothing here either. */` |
|    137410 |  644 | `	PH7_MemObjInit(pTarget->pVm,&sVal);` |
|    137410 |  645 | `	rc = VmExpandConstantOnce(pTarget->pVm,pCons,&sVal);` |
|    137410 |  646 | `	if( rc == SXRET_OK ){` |
|    137410 |  647 | `		ph7_array_add_elem(pTarget,&sName,&sVal); /* Will make its own copy */` |
|     66726 |  648 | `	}` |
|    137410 |  649 | `	PH7_MemObjRelease(&sVal);` |
|    137410 |  650 | `	PH7_MemObjRelease(&sName);` |
|    137410 |  651 | `	return rc;` |
|         3 |  652 | `}` |
|         - |  653 | `/*` |
|         - |  654 | ` * array get_defined_constants(bool $categorize = false)` |
|         - |  655 | ` *  Returns an associative array with the names AND VALUES of all defined` |
|         - |  656 | ` *  constants.` |
|         - |  657 | ` * Parameters` |
|         - |  658 | ` *  $categorize` |
|         - |  659 | ` *   TRUE groups the map one level deeper, by the extension each constant` |
|         - |  660 | ` *   belongs to. This engine has no extension partition (the same limitation` |
|         - |  661 | ` *   ReflectionFunction::getExtensionName() records), so it answers php's two` |
|         - |  662 | `` *   buckets it CAN tell apart: `user` for everything a script defined with`` |
|         - |  663 | `` *   define()/const, and `Core` for the engine's own -- where php would spread`` |
|         - |  664 | ` *   the latter over standard/date/pcre/json/… as well.` |
|         - |  665 | ` * Returns` |
|         - |  666 | ` *  The constants currently defined, name => value.` |
|         - |  667 | ` */` |
|         - |  668 | `/*` |
|         - |  669 | ` * One category of get_defined_constants(true): the constants ONE extension` |
|         - |  670 | ` * registers that this build actually has, expanded in php's own registration` |
|         - |  671 | ` * order. Answers 0 while it is filling; a raising initializer stops the walk` |
|         - |  672 | ` * the way the flat pass does.` |
|         - |  673 | ` */` |
|         - |  674 | `typedef struct VmConstBucket VmConstBucket;` |
|         - |  675 | `struct VmConstBucket {` |
|         - |  676 | `	ph7_vm *pVm;` |
|         - |  677 | `	ph7_value *pOut;` |
|         - |  678 | `	sxi32 rc;` |
|         - |  679 | `};` |
|     23028 |  680 | `static int VmConstBucketStep(const char *zName,int nName,void *pData)` |
|         3 |  681 | `{` |
|     23031 |  682 | `	VmConstBucket *p = (VmConstBucket *)pData;` |
|         - |  683 | `	SyHashEntry *pEntry;` |
|     23031 |  684 | `	if( !PH7_VmInternalNameExists(p->pVm,PH7_EXT_KIND_CONST,zName,nName) ){` |
|       825 |  685 | `		return 0;` |
|         - |  686 | `	}` |
|     22209 |  687 | `	pEntry = SyHashGet(&p->pVm->hConstant,(const void *)zName,(sxu32)nName);` |
|     22209 |  688 | `	if( pEntry == 0 ){` |
|       ! 0 |  689 | `		return 0;` |
|         - |  690 | `	}` |
|     22209 |  691 | `	p->rc = VmConstDumpEntry(p->pOut,pEntry);` |
|     22209 |  692 | `	return p->rc == SXRET_OK ? 0 : 1;` |
|     11517 |  693 | `}` |
|        70 |  694 | `PH7_PRIVATE int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 |  695 | `{` |
|        73 |  696 | `	ph7_value *pArray,*pAll,*pUser = 0;` |
|         - |  697 | `	ph7_value *apBucket[PH7_EXT_MAX];   /* one per extension; filled below when categorizing */` |
|         - |  698 | `	SySet aSnap;` |
|         - |  699 | `	SyHashEntry **apEntry;` |
|         - |  700 | `	sxu32 n,nSnap;` |
|        73 |  701 | `	int iExt,nExt = 0;` |
|        73 |  702 | `	int bCategorize = nArg > 0 && ph7_value_to_bool(apArg[0]);` |
|      4553 |  703 | `	for( iExt = 0 ; iExt < PH7_EXT_MAX ; ++iExt ){` |
|      4483 |  704 | `		apBucket[iExt] = 0;` |
|      2243 |  705 | `	}` |
|         - |  706 | `	/* Create the array first*/` |
|        73 |  707 | `	pArray = ph7_context_new_array(pCtx);` |
|        73 |  708 | `	if( pArray == 0 ){` |
|         - |  709 | `		/* Return NULL */` |
|       ! 0 |  710 | `		ph7_result_null(pCtx);` |
|       ! 0 |  711 | `		return SXRET_OK;` |
|         - |  712 | `	}` |
|        73 |  713 | `	pAll = pArray;` |
|        73 |  714 | `	if( bCategorize ){` |
|         - |  715 | `		/* php's categories are the extensions, in the order get_loaded_extensions()` |
|         - |  716 | ``		 * lists them and each in its OWN registration order, with `user` last --`` |
|         - |  717 | `		 * and php OMITS a category with nothing in it. Before this engine had an` |
|         - |  718 | ``		 * extension partition all 1314 answers sat under a single `Core`.`` |
|         - |  719 | `		 *` |
|         - |  720 | `		 * The per-extension pass runs first so that each bucket is in php's` |
|         - |  721 | `		 * order; the table walk that follows only has to place what the` |
|         - |  722 | `		 * partition has no row for, which rides with Core. */` |
|        15 |  723 | `		nExt = PH7_VmExtensionCount();` |
|        15 |  724 | `		if( nExt > PH7_EXT_MAX ){` |
|       ! 0 |  725 | `			nExt = PH7_EXT_MAX;` |
|       ! 0 |  726 | `		}` |
|       435 |  727 | `		for( iExt = 0 ; iExt < nExt ; ++iExt ){` |
|       423 |  728 | `			apBucket[iExt] = PH7_VmExtensionAvailable(iExt) ? ph7_context_new_array(pCtx) : 0;` |
|       213 |  729 | `		}` |
|        15 |  730 | `		pUser = ph7_context_new_array(pCtx);` |
|        15 |  731 | `		pAll = apBucket[PH7_EXT_CORE];` |
|        15 |  732 | `		if( pAll == 0 \|\| pUser == 0 ){` |
|       ! 0 |  733 | `			ph7_result_null(pCtx);` |
|       ! 0 |  734 | `			return SXRET_OK;` |
|         - |  735 | `		}` |
|       435 |  736 | `		for( iExt = 0 ; iExt < nExt ; ++iExt ){` |
|         - |  737 | `			VmConstBucket sBucket;` |
|       423 |  738 | `			if( apBucket[iExt] == 0 ){` |
|         3 |  739 | `				continue;` |
|         - |  740 | `			}` |
|       423 |  741 | `			sBucket.pVm = pCtx->pVm;` |
|       423 |  742 | `			sBucket.pOut = apBucket[iExt];` |
|       423 |  743 | `			sBucket.rc = SXRET_OK;` |
|       423 |  744 | `			pCtx->pVm->bConstEnum++;` |
|       423 |  745 | `			PH7_VmExtWalk(iExt,PH7_EXT_KIND_CONST,VmConstBucketStep,&sBucket);` |
|       423 |  746 | `			pCtx->pVm->bConstEnum--;` |
|       423 |  747 | `			if( sBucket.rc != SXRET_OK ){` |
|       ! 0 |  748 | `				return sBucket.rc;` |
|         - |  749 | `			}` |
|       213 |  750 | `		}` |
|         6 |  751 | `	}` |
|         - |  752 | `	/* Snapshot the table, then expand: expanding runs user code, which may` |
|         - |  753 | `	 * define() and grow the table under the walk (see VmHashConstStep). */` |
|        73 |  754 | `	SySetInit(&aSnap,&pCtx->pVm->sAllocator,sizeof(SyHashEntry *));` |
|        73 |  755 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,&aSnap);` |
|        73 |  756 | `	apEntry = (SyHashEntry **)SySetBasePtr(&aSnap);` |
|        73 |  757 | `	nSnap = SySetUsed(&aSnap);` |
|         - |  758 | `	/* Describing the table is not READING its entries: php's deprecated constants` |
|         - |  759 | `	 * report when a program names one, and get_defined_constants() lists them in` |
|         - |  760 | `	 * silence. */` |
|        73 |  761 | `	pCtx->pVm->bConstEnum++;` |
|    137480 |  762 | `	for( n = 0 ; n < nSnap ; ++n ){` |
|    137410 |  763 | `		ph7_constant *pCons = (ph7_constant *)apEntry[n]->pUserData;` |
|         - |  764 | `		sxi32 rcExp;` |
|    137407 |  765 | `		if( bCategorize && !pCons->bUserDefined` |
|     23327 |  766 | `		 && PH7_VmExtHasName(PH7_EXT_KIND_CONST,(const char *)apEntry[n]->pKey,` |
|     23214 |  767 | `				(int)apEntry[n]->nKeyLen) ){` |
|     22209 |  768 | `			continue;   /* the per-extension pass already placed it */` |
|         - |  769 | `		}` |
|    115204 |  770 | `		rcExp = VmConstDumpEntry(pUser && pCons->bUserDefined ? pUser : pAll,apEntry[n]);` |
|    115204 |  771 | `		if( rcExp != SXRET_OK ){` |
|         - |  772 | `			/* An initializer raised while being described: stop, exactly as any` |
|         - |  773 | `			 * other builtin does when the php it invoked did not return.` |
|         - |  774 | `			 * Carrying on would run every LATER initializer past a throw that has` |
|         - |  775 | `			 * already been landed. */` |
|       ! 0 |  776 | `			pCtx->pVm->bConstEnum--;` |
|       ! 0 |  777 | `			SySetRelease(&aSnap);` |
|       ! 0 |  778 | `			return rcExp;` |
|         - |  779 | `		}` |
|     55965 |  780 | `	}` |
|        73 |  781 | `	pCtx->pVm->bConstEnum--;` |
|        73 |  782 | `	SySetRelease(&aSnap);` |
|        73 |  783 | `	if( bCategorize ){` |
|       435 |  784 | `		for( iExt = 0 ; iExt < nExt ; ++iExt ){` |
|       423 |  785 | `			if( apBucket[iExt] == 0 ){` |
|         3 |  786 | `				continue;` |
|         - |  787 | `			}` |
|       423 |  788 | `			if( ph7_array_count(apBucket[iExt]) > 0 ){` |
|       291 |  789 | `				ph7_array_add_strkey_elem(pArray,PH7_VmExtensionName(iExt),apBucket[iExt]);` |
|       144 |  790 | `			}` |
|       423 |  791 | `			ph7_context_release_value(pCtx,apBucket[iExt]);` |
|       213 |  792 | `		}` |
|        15 |  793 | `		if( ph7_array_count(pUser) > 0 ){` |
|        13 |  794 | `			ph7_array_add_strkey_elem(pArray,"user",pUser);` |
|         5 |  795 | `		}` |
|        15 |  796 | `		ph7_context_release_value(pCtx,pUser);` |
|         6 |  797 | `	}` |
|         - |  798 | `	/* Return the created array */` |
|        73 |  799 | `	ph7_result_value(pCtx,pArray);` |
|        73 |  800 | `	return SXRET_OK;` |
|        38 |  801 | `}` |
|         - |  802 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|         - |  803 | `/*` |
|         - |  804 | ` * Section:` |
|         - |  805 | ` *  Random numbers/string generators.` |
|         - |  806 | ` * Status:` |
|         - |  807 | ` *    Stable.` |
|         - |  808 | ` */` |
|         - |  809 | `/*` |
|         - |  810 | ` * Generate a random 32-bit unsigned integer.` |
|         - |  811 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|         - |  812 | ` * implemented in src/sx/sxrand.c).` |
|         - |  813 | ` */` |
|        68 |  814 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|         2 |  815 | `{` |
|         - |  816 | `	sxu32 iNum;` |
|        70 |  817 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|        70 |  818 | `	return iNum;` |
|         2 |  819 | `}` |
|         - |  820 | `/*` |
|         - |  821 | ` * The MT19937 generator that backs PHP's rand()/mt_rand() family. It is kept` |
|         - |  822 | ` * separate from the RC4 SyRandomness above so that srand()/mt_srand() give` |
|         - |  823 | ` * userland PHP's reproducible sequence without perturbing the engine's internal` |
|         - |  824 | ` * entropy (object ids, uniqid, quicksort pivots stay on the RC4 generator, as` |
|         - |  825 | ` * they are in PHP too — srand does not touch those).` |
|         - |  826 | ` */` |
|         - |  827 | `/*` |
|         - |  828 | ` * Reset the MT19937 state to a 32-bit seed (PHP truncates its int seed likewise).` |
|         - |  829 | ` */` |
|       314 |  830 | `PH7_PRIVATE void PH7_VmMtSrand(ph7_vm *pVm,sxu32 nSeed,int bLegacyTwist)` |
|         3 |  831 | `{` |
|       317 |  832 | `	SyMT19937Seed(&pVm->sMt,nSeed,bLegacyTwist);` |
|       317 |  833 | `	pVm->mtSeeded = TRUE;` |
|       317 |  834 | `}` |
|         - |  835 | `/*` |
|         - |  836 | ` * Draw the next full 32-bit MT19937 word, seeding lazily from the OS CSPRNG on` |
|         - |  837 | ` * first use exactly as PHP auto-seeds when rand()/mt_rand() runs before srand().` |
|         - |  838 | ` */` |
|      2582 |  839 | `PH7_PRIVATE sxu32 PH7_VmMtRand(ph7_vm *pVm)` |
|         3 |  840 | `{` |
|      2585 |  841 | `	if( !pVm->mtSeeded ){` |
|         - |  842 | `		sxu32 nSeed;` |
|         3 |  843 | `		if( SyOSCSPRNG((void *)&nSeed,sizeof(nSeed)) != SXRET_OK ){` |
|         - |  844 | `			/* No OS entropy source: fall back to the RC4 generator's output. */` |
|       ! 0 |  845 | `			nSeed = PH7_VmRandomNum(pVm);` |
|       ! 0 |  846 | `		}` |
|         - |  847 | `		/* An un-seeded generator is php's default one, never MT_RAND_PHP. */` |
|         3 |  848 | `		SyMT19937Seed(&pVm->sMt,nSeed,FALSE);` |
|         3 |  849 | `		pVm->mtSeeded = TRUE;` |
|         1 |  850 | `	}` |
|      2585 |  851 | `	return SyMT19937Next(&pVm->sMt);` |
|         3 |  852 | `}` |
|         - |  853 | `/*` |
|         - |  854 | ` * Map a full 32-bit draw uniformly into [0,uMax] (uMax is the range width, i.e.` |
|         - |  855 | ` * max-min). Rejection sampling against the largest unbiased ceiling, matching` |
|         - |  856 | ` * PHP's php_random_range32().` |
|         - |  857 | ` */` |
|      2378 |  858 | `static sxu32 VmMtRange32(ph7_vm *pVm,sxu32 uMax)` |
|         3 |  859 | `{` |
|         - |  860 | `	sxu32 result,limit;` |
|      2381 |  861 | `	result = PH7_VmMtRand(pVm);` |
|         - |  862 | `	/* Whole 32-bit domain: no scaling needed. */` |
|      2381 |  863 | `	if( uMax == 0xFFFFFFFFU ){` |
|       ! 0 |  864 | `		return result;` |
|         - |  865 | `	}` |
|         - |  866 | `	/* Make the range inclusive of max. */` |
|      2381 |  867 | `	uMax++;` |
|         - |  868 | `	/* Powers of two are unbiased under a plain mask. */` |
|      2381 |  869 | `	if( (uMax & (uMax - 1)) == 0 ){` |
|        87 |  870 | `		return result & (uMax - 1);` |
|         - |  871 | `	}` |
|         - |  872 | `	/* Ceiling under which 0xFFFFFFFF % uMax == 0; discard draws above it. */` |
|      2297 |  873 | `	limit = 0xFFFFFFFFU - (0xFFFFFFFFU % uMax) - 1;` |
|      2297 |  874 | `	while( result > limit ){` |
|       ! 0 |  875 | `		result = PH7_VmMtRand(pVm);` |
|       ! 0 |  876 | `	}` |
|      2297 |  877 | `	return result % uMax;` |
|      1192 |  878 | `}` |
|         - |  879 | `/*` |
|         - |  880 | ` * 64-bit-wide range: assemble two draws (high word first, as PHP does) and` |
|         - |  881 | ` * reject-sample. Matches PHP's php_random_range64().` |
|         - |  882 | ` */` |
|        14 |  883 | `static sxu64 VmMtRange64(ph7_vm *pVm,sxu64 uMax)` |
|         2 |  884 | `{` |
|         - |  885 | `	sxu64 result,limit;` |
|         - |  886 | `	/* First draw fills the low word, second draw the high word — order is` |
|         - |  887 | `	 * significant and matches php's php_random_range64() assembly. */` |
|        16 |  888 | `	result = (sxu64)PH7_VmMtRand(pVm);` |
|        16 |  889 | `	result \|= (sxu64)PH7_VmMtRand(pVm) << 32;` |
|        16 |  890 | `	if( uMax == 0xFFFFFFFFFFFFFFFFULL ){` |
|       ! 0 |  891 | `		return result;` |
|         - |  892 | `	}` |
|        16 |  893 | `	uMax++;` |
|        16 |  894 | `	if( (uMax & (uMax - 1)) == 0 ){` |
|        14 |  895 | `		return result & (uMax - 1);` |
|         - |  896 | `	}` |
|         3 |  897 | `	limit = 0xFFFFFFFFFFFFFFFFULL - (0xFFFFFFFFFFFFFFFFULL % uMax) - 1;` |
|         3 |  898 | `	while( result > limit ){` |
|       ! 0 |  899 | `		result = (sxu64)PH7_VmMtRand(pVm);` |
|       ! 0 |  900 | `		result \|= (sxu64)PH7_VmMtRand(pVm) << 32;` |
|       ! 0 |  901 | `	}` |
|         3 |  902 | `	return result % uMax;` |
|         9 |  903 | `}` |
|         - |  904 | `/*` |
|         - |  905 | ` * Return a value uniformly in the inclusive range [iMin,iMax]. The caller` |
|         - |  906 | ` * guarantees iMin <= iMax. Mirrors PHP's php_mt_rand_range(): a range that fits` |
|         - |  907 | ` * in 32 bits takes the 32-bit path, a wider one the 64-bit path.` |
|         - |  908 | ` */` |
|      2392 |  909 | `PH7_PRIVATE sxi64 PH7_VmMtRandRange(ph7_vm *pVm,sxi64 iMin,sxi64 iMax)` |
|         3 |  910 | `{` |
|      2395 |  911 | `	sxu64 uMax = (sxu64)iMax - (sxu64)iMin;` |
|      2395 |  912 | `	if( uMax > 0xFFFFFFFFULL ){` |
|        16 |  913 | `		return (sxi64)(VmMtRange64(pVm,uMax) + (sxu64)iMin);` |
|         - |  914 | `	}` |
|      2381 |  915 | `	return (sxi64)((sxu64)VmMtRange32(pVm,(sxu32)uMax) + (sxu64)iMin);` |
|      1199 |  916 | `}` |
|         - |  917 | `/*` |
|         - |  918 | ` * Generate a random string (English Alphabet) of length nLen.` |
|         - |  919 | ` * Note that the generated string is NOT null terminated.` |
|         - |  920 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|         - |  921 | ` * implemented in src/sx/sxrand.c).` |
|         - |  922 | ` */` |
|  11164519 |  923 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|         5 |  924 | `{` |
|         - |  925 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|         - |  926 | `	int i;` |
|         - |  927 | `	/* Generate a binary string first */` |
|  11164524 |  928 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|         - |  929 | `	/* Turn the binary string into english based alphabet */` |
| 122810924 |  930 | `	for( i = 0 ; i < nLen ; ++i ){` |
| 111646405 |  931 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|  55745759 |  932 | `	 }` |
|  11164524 |  933 | `}` |
|         - |  934 | `/*` |
|         - |  935 | ` * int rand()` |
|         - |  936 | ` * int mt_rand()` |
|         - |  937 | ` * int rand(int $min,int $max)` |
|         - |  938 | ` * int mt_rand(int $min,int $max)` |
|         - |  939 | ` *  Generate a random (unsigned 32-bit) integer.` |
|         - |  940 | ` * Parameter` |
|         - |  941 | ` *  $min` |
|         - |  942 | ` *    The lowest value to return (default: 0)` |
|         - |  943 | ` *  $max` |
|         - |  944 | ` *   The highest value to return (default: getrandmax())` |
|         - |  945 | ` * Return` |
|         - |  946 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|         - |  947 | ` * Note:` |
|         - |  948 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|         - |  949 | ` *  by te SQLite3 library.` |
|         - |  950 | ` */` |
|      1894 |  951 | `PH7_PRIVATE int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 |  952 | `{` |
|      1896 |  953 | `	SyString *pName = &pCtx->pFunc->sName;` |
|      3348 |  954 | `	int bMt = (pName->nByte == sizeof("mt_rand")-1` |
|      1894 |  955 | `		&& SyMemcmp(pName->zString,"mt_rand",sizeof("mt_rand")-1) == 0);` |
|         - |  956 | `	/* php accepts exactly 0 or exactly 2 arguments (min,max); 1 or 3+ is an` |
|         - |  957 | `	 * ArgumentCountError. The central arity table can't express "0 or 2", so` |
|         - |  958 | `	 * it is enforced here (was a silent wrong result for the raw draw). */` |
|      1896 |  959 | `	if( nArg == 1 \|\| nArg > 2 ){` |
|        13 |  960 | `		return PH7_VmThrowException(pCtx,` |
|         - |  961 | `			"ArgumentCountError",` |
|         - |  962 | `			"%z() expects exactly 2 arguments, %d given",` |
|         4 |  963 | `			pName, nArg` |
|         - |  964 | `			);` |
|         - |  965 | `	}` |
|      1888 |  966 | `	if( nArg == 2 ){` |
|         - |  967 | `		sxi64 iMin,iMax;` |
|         - |  968 | `		/* Signed 64-bit endpoints: the old unsigned math wrapped negative` |
|         - |  969 | `		 * ranges to huge positives (rand(-10,-1) -> ~4e9) and mis-handled` |
|         - |  970 | `		 * min==max. */` |
|      1776 |  971 | `		iMin = ph7_value_to_int64(apArg[0]);` |
|      1776 |  972 | `		iMax = ph7_value_to_int64(apArg[1]);` |
|      1776 |  973 | `		if( iMin > iMax ){` |
|         9 |  974 | `			if( bMt ){` |
|         - |  975 | `				/* mt_rand() is strict: php throws a catchable ValueError. */` |
|         5 |  976 | `				return PH7_VmThrowException(pCtx,` |
|         - |  977 | `					"ValueError",` |
|         - |  978 | `					"mt_rand(): Argument #2 ($max) must be greater than or equal to argument #1 ($min)"` |
|         - |  979 | `					);` |
|         - |  980 | `			}` |
|         - |  981 | `			/* rand() swaps the bounds for backward compatibility (php keeps` |
|         - |  982 | `			 * this quirk; only mt_rand() rejects a reversed range). */` |
|         5 |  983 | `			{ sxi64 iTmp = iMin; iMin = iMax; iMax = iTmp; }` |
|         2 |  984 | `		}` |
|      1772 |  985 | `		if( pCtx->pVm->sMt.bLegacyTwist ){` |
|         - |  986 | `			/* MT_RAND_PHP is a whole generator, mapping included: php keeps its old` |
|         - |  987 | `			 * RAND_RANGE_BADSCALING here — a 31-bit draw scaled through a double,` |
|         - |  988 | `			 * which is biased and is exactly what the recorded sequence a caller` |
|         - |  989 | `			 * asked for was produced with. */` |
|        65 |  990 | `			double rNum = (double)(PH7_VmMtRand(pCtx->pVm) >> 1);` |
|        65 |  991 | `			double rSpan = (double)iMax - (double)iMin + 1.0;` |
|         - |  992 | `			/* php's own arithmetic, types included: the product lands in an` |
|         - |  993 | `			 * unsigned 64-bit result, so a span wider than the SIGNED range keeps` |
|         - |  994 | `			 * its value and wraps around the minimum rather than saturating. The` |
|         - |  995 | `			 * product is never negative (the span is at least 1, the fraction at` |
|         - |  996 | `			 * least 0), so the unsigned conversion is total. */` |
|        65 |  997 | `			sxu64 uOut = (sxu64)iMin + (sxu64)(rSpan * (rNum / (2147483647.0 + 1.0)));` |
|        65 |  998 | `			ph7_result_int64(pCtx,(sxi64)uOut);` |
|        65 |  999 | `			return SXRET_OK;` |
|         - | 1000 | `		}` |
|         - | 1001 | `		/* MT19937-backed uniform draw over [iMin,iMax], bit-for-bit as php. */` |
|      1708 | 1002 | `		ph7_result_int64(pCtx,PH7_VmMtRandRange(pCtx->pVm,iMin,iMax));` |
|      1708 | 1003 | `		return SXRET_OK;` |
|         - | 1004 | `	}` |
|         - | 1005 | `	/* No-argument form: a 31-bit value in [0, mt_getrandmax()]. php returns` |
|         - | 1006 | `	 * php_mt_rand() >> 1 for the bare draw (the full 32-bit word feeds the` |
|         - | 1007 | `	 * range form above, but the bare form drops the low bit). */` |
|       114 | 1008 | `	ph7_result_int64(pCtx,(sxi64)(PH7_VmMtRand(pCtx->pVm) >> 1));` |
|       114 | 1009 | `	return SXRET_OK;` |
|       949 | 1010 | `}` |
|         - | 1011 | `/*` |
|         - | 1012 | ` * int getrandmax(void)` |
|         - | 1013 | ` * int mt_getrandmax(void)` |
|         - | 1014 | ` * int rc4_getrandmax(void)` |
|         - | 1015 | ` *   Show largest possible random value` |
|         - | 1016 | ` * Return` |
|         - | 1017 | ` *  The largest possible random value returned by rand()/mt_rand(): php's` |
|         - | 1018 | ` *  MT19937 backing makes this 2^31-1 (2147483647) for both.` |
|         - | 1019 | ` */` |
|         8 | 1020 | `PH7_PRIVATE int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 1021 | `{` |
|         4 | 1022 | `	SXUNUSED(nArg); /* cc warning */` |
|         4 | 1023 | `	SXUNUSED(apArg);` |
|         - | 1024 | `	/* php: PHP_MT_RAND_MAX == (1<<31)-1; bare rand()/mt_rand() draw >> 1 lands` |
|         - | 1025 | `	 * exactly in [0, this]. */` |
|         9 | 1026 | `	ph7_result_int64(pCtx,2147483647);` |
|         9 | 1027 | `	return SXRET_OK;` |
|         1 | 1028 | `}` |
|         - | 1029 | `/*` |
|         - | 1030 | ` * string rand_str()` |
|         - | 1031 | ` * string rand_str(int $len)` |
|         - | 1032 | ` *  Generate a random string (English alphabet).` |
|         - | 1033 | ` * Parameter` |
|         - | 1034 | ` *  $len` |
|         - | 1035 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|         - | 1036 | ` * Return` |
|         - | 1037 | ` *   A pseudo random string.` |
|         - | 1038 | ` * Note:` |
|         - | 1039 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|         - | 1040 | ` *  by te SQLite3 library.` |
|         - | 1041 | ` *  This function is a symisc extension.` |
|         - | 1042 | ` */` |
|       605 | 1043 | `PH7_PRIVATE int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 1044 | `{` |
|         - | 1045 | `	char zString[1024];` |
|       610 | 1046 | `	int iLen = 0x10;` |
|       610 | 1047 | `	if( nArg > 0 ){` |
|         - | 1048 | `		/* Get the desired length */` |
|       610 | 1049 | `		iLen = ph7_value_to_int(apArg[0]);` |
|       610 | 1050 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|         - | 1051 | `			/* Default length */` |
|         3 | 1052 | `			iLen = 0x10;` |
|         1 | 1053 | `		}` |
|       302 | 1054 | `	}` |
|         - | 1055 | `	/* Generate the random string */` |
|       610 | 1056 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|         - | 1057 | `	/* Return the generated string */` |
|       610 | 1058 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|       610 | 1059 | `	return SXRET_OK;` |
|         5 | 1060 | `}` |
|         - | 1061 | `/*` |
|         - | 1062 | ` * Reject non-numeric values (array/object/resource and non-numeric strings)` |
|         - | 1063 | ` * the same way intdiv() does. Returns SXRET_OK if the value is acceptable as` |
|         - | 1064 | ` * an int (PHP coerces float and numeric string silently).` |
|         - | 1065 | ` */` |
|       476 | 1066 | `static int VmRandomCheckIntArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgPos,const char *zParamName)` |
|         1 | 1067 | `{` |
|         - | 1068 | `	char zGiven[64];` |
|       476 | 1069 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg)` |
|       477 | 1070 | `		\|\| ph7_value_is_resource(pArg) ){` |
|       ! 0 | 1071 | `		return PH7_VmThrowException(pCtx,` |
|         - | 1072 | `			"TypeError",` |
|         - | 1073 | `			"%s(): Argument #%d (%s) must be of type int, %s given",` |
|       ! 0 | 1074 | `			zFunc,iArgPos,zParamName,` |
|       ! 0 | 1075 | `			VmValueGivenName(pArg,zGiven,sizeof(zGiven))` |
|         - | 1076 | `			);` |
|         - | 1077 | `	}` |
|       477 | 1078 | `	if( ph7_value_is_string(pArg) ){` |
|         - | 1079 | `		int len;` |
|         5 | 1080 | `		const char *zStr = ph7_value_to_string(pArg, &len);` |
|         5 | 1081 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|       ! 0 | 1082 | `			return PH7_VmThrowException(pCtx,` |
|         - | 1083 | `				"TypeError",` |
|         - | 1084 | `				"%s(): Argument #%d (%s) must be of type int, string given",` |
|       ! 0 | 1085 | `				zFunc,iArgPos,zParamName` |
|         - | 1086 | `				);` |
|         - | 1087 | `		}` |
|         2 | 1088 | `	}` |
|       477 | 1089 | `	return SXRET_OK;` |
|       239 | 1090 | `}` |
|         - | 1091 | `/*` |
|         - | 1092 | ` * int random_int(int $min, int $max)` |
|         - | 1093 | ` *  Generate a cryptographically secure pseudo-random integer in [$min, $max].` |
|         - | 1094 | ` *  Mirrors PHP 7.0+ random_int(). Uses the OS CSPRNG via SyOSCSPRNG().` |
|         - | 1095 | ` *  Distribution is uniform via rejection sampling against the smallest` |
|         - | 1096 | ` *  power-of-two mask covering the range.` |
|         - | 1097 | ` */` |
|       230 | 1098 | `PH7_PRIVATE int vm_builtin_random_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 1099 | `{` |
|         - | 1100 | `	sxi64 iMin,iMax;` |
|         - | 1101 | `	sxu64 uRange,uMask,uResult;` |
|         - | 1102 | `	unsigned int nAttempt;` |
|         - | 1103 | `	int rc;` |
|       231 | 1104 | `	if( nArg != 2 ){` |
|       ! 0 | 1105 | `		return PH7_VmThrowException(pCtx,` |
|         - | 1106 | `			"ArgumentCountError",` |
|         - | 1107 | `			"random_int() expects exactly 2 arguments, %d given",` |
|       ! 0 | 1108 | `			nArg` |
|         - | 1109 | `			);` |
|         - | 1110 | `	}` |
|       231 | 1111 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_int",1,"$min");` |
|       231 | 1112 | `	if( rc != SXRET_OK ){ return rc; }` |
|       231 | 1113 | `	rc = VmRandomCheckIntArg(pCtx,apArg[1],"random_int",2,"$max");` |
|       231 | 1114 | `	if( rc != SXRET_OK ){ return rc; }` |
|       231 | 1115 | `	iMin = ph7_value_to_int64(apArg[0]);` |
|       231 | 1116 | `	iMax = ph7_value_to_int64(apArg[1]);` |
|       231 | 1117 | `	if( iMin > iMax ){` |
|         3 | 1118 | `		return PH7_VmThrowException(pCtx,` |
|         - | 1119 | `			"ValueError",` |
|         - | 1120 | `			"random_int(): Argument #1 ($min) must be less than or equal to argument #2 ($max)"` |
|         - | 1121 | `			);` |
|         - | 1122 | `	}` |
|       229 | 1123 | `	if( iMin == iMax ){` |
|         5 | 1124 | `		ph7_result_int64(pCtx,iMin);` |
|         5 | 1125 | `		return SXRET_OK;` |
|         - | 1126 | `	}` |
|       225 | 1127 | `	uRange = (sxu64)iMax - (sxu64)iMin;` |
|       225 | 1128 | `	uMask = uRange;` |
|       225 | 1129 | `	uMask \|= uMask >> 1;` |
|       225 | 1130 | `	uMask \|= uMask >> 2;` |
|       225 | 1131 | `	uMask \|= uMask >> 4;` |
|       225 | 1132 | `	uMask \|= uMask >> 8;` |
|       225 | 1133 | `	uMask \|= uMask >> 16;` |
|       225 | 1134 | `	uMask \|= uMask >> 32;` |
|       225 | 1135 | `	uResult = 0;` |
|       358 | 1136 | `	for( nAttempt = 0 ; nAttempt < 50 ; ++nAttempt ){` |
|         - | 1137 | `		/* Always draw a full 8 bytes so endianness of the cast doesn't matter` |
|         - | 1138 | `		 * (a 4-byte fill into a sxu64 would land in the high half on big-endian` |
|         - | 1139 | `		 * and the low-half mask would always read 0). */` |
|         - | 1140 | `		sxu64 uDraw;` |
|       358 | 1141 | `		if( SyOSCSPRNG(&uDraw,sizeof(uDraw)) != SXRET_OK ){` |
|       ! 0 | 1142 | `			return PH7_VmThrowException(pCtx,` |
|         - | 1143 | `				"Random\\RandomException",` |
|         - | 1144 | `				"Cannot gather sufficient random data"` |
|         - | 1145 | `				);` |
|         - | 1146 | `		}` |
|       358 | 1147 | `		uDraw &= uMask;` |
|       358 | 1148 | `		if( uDraw <= uRange ){` |
|       225 | 1149 | `			uResult = uDraw;` |
|       225 | 1150 | `			break;` |
|         - | 1151 | `		}` |
|        64 | 1152 | `	}` |
|       225 | 1153 | `	if( nAttempt >= 50 ){` |
|       ! 0 | 1154 | `		return PH7_VmThrowException(pCtx,` |
|         - | 1155 | `			"Random\\RandomException",` |
|         - | 1156 | `			"Cannot gather sufficient random data"` |
|         - | 1157 | `			);` |
|         - | 1158 | `	}` |
|       225 | 1159 | `	ph7_result_int64(pCtx,(sxi64)((sxu64)iMin + uResult));` |
|       225 | 1160 | `	return SXRET_OK;` |
|       116 | 1161 | `}` |
|         - | 1162 | `/*` |
|         - | 1163 | ` * string random_bytes(int $length)` |
|         - | 1164 | ` *  Generate $length cryptographically secure random bytes via SyOSCSPRNG().` |
|         - | 1165 | ` *  Mirrors PHP 7.0+ random_bytes().` |
|         - | 1166 | ` */` |
|        16 | 1167 | `PH7_PRIVATE int vm_builtin_random_bytes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 1168 | `{` |
|         - | 1169 | `	sxi64 iLen;` |
|         - | 1170 | `	unsigned char zStack[256];` |
|         - | 1171 | `	void *pBuf;` |
|         - | 1172 | `	int rc;` |
|        17 | 1173 | `	int bHeap = 0;` |
|        17 | 1174 | `	if( nArg != 1 ){` |
|       ! 0 | 1175 | `		return PH7_VmThrowException(pCtx,` |
|         - | 1176 | `			"ArgumentCountError",` |
|         - | 1177 | `			"random_bytes() expects exactly 1 argument, %d given",` |
|       ! 0 | 1178 | `			nArg` |
|         - | 1179 | `			);` |
|         - | 1180 | `	}` |
|        17 | 1181 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_bytes",1,"$length");` |
|        17 | 1182 | `	if( rc != SXRET_OK ){ return rc; }` |
|        17 | 1183 | `	iLen = ph7_value_to_int64(apArg[0]);` |
|        17 | 1184 | `	if( iLen < 1 ){` |
|         5 | 1185 | `		return PH7_VmThrowException(pCtx,` |
|         - | 1186 | `			"ValueError",` |
|         - | 1187 | `			"random_bytes(): Argument #1 ($length) must be greater than 0"` |
|         - | 1188 | `			);` |
|         - | 1189 | `	}` |
|         - | 1190 | `	/* The PH7 allocator and ph7_result_string both take sxu32/int sizes,` |
|         - | 1191 | `	 * so we can't honor a length above 2 GiB. Reject early rather than` |
|         - | 1192 | `	 * silently truncating via the (sxu32) cast below. */` |
|        13 | 1193 | `	if( iLen > 0x7FFFFFFF ){` |
|       ! 0 | 1194 | `		return PH7_VmThrowException(pCtx,` |
|         - | 1195 | `			"ValueError",` |
|         - | 1196 | `			"random_bytes(): Argument #1 ($length) is too large"` |
|         - | 1197 | `			);` |
|         - | 1198 | `	}` |
|        13 | 1199 | `	if( iLen <= (sxi64)sizeof(zStack) ){` |
|        13 | 1200 | `		pBuf = zStack;` |
|         7 | 1201 | `	}else{` |
|       ! 0 | 1202 | `		pBuf = SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)iLen);` |
|       ! 0 | 1203 | `		if( pBuf == 0 ){` |
|       ! 0 | 1204 | `			return PH7_VmThrowException(pCtx,` |
|         - | 1205 | `				"Exception",` |
|         - | 1206 | `				"random_bytes(): Failed to allocate %qd bytes",` |
|       ! 0 | 1207 | `				iLen` |
|         - | 1208 | `				);` |
|         - | 1209 | `		}` |
|       ! 0 | 1210 | `		bHeap = 1;` |
|         - | 1211 | `	}` |
|        13 | 1212 | `	if( SyOSCSPRNG(pBuf,(sxu32)iLen) != SXRET_OK ){` |
|       ! 0 | 1213 | `		if( bHeap ){` |
|       ! 0 | 1214 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|       ! 0 | 1215 | `		}` |
|       ! 0 | 1216 | `		return PH7_VmThrowException(pCtx,` |
|         - | 1217 | `			"Random\\RandomException",` |
|         - | 1218 | `			"Cannot gather sufficient random data"` |
|         - | 1219 | `			);` |
|         - | 1220 | `	}` |
|        13 | 1221 | `	ph7_result_string(pCtx,(const char *)pBuf,(int)iLen);` |
|        13 | 1222 | `	if( bHeap ){` |
|       ! 0 | 1223 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|       ! 0 | 1224 | `	}` |
|        13 | 1225 | `	return SXRET_OK;` |
|         9 | 1226 | `}` |
|         - | 1227 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 1228 | `/* uniqid() used to be gated on PH7_DISABLE_HASH_FUNC as well, because PH7 built` |
|         - | 1229 | ` * its id out of a SHA1. php's is the clock, so the hash guard has nothing to say` |
|         - | 1230 | ` * about it any more. */` |
|         - | 1231 | `/*` |
|         - | 1232 | `` * php's `php_combined_lcg()`: L'Ecuyer's combined linear congruential generator,`` |
|         - | 1233 | ` * two streams whose difference is the answer.` |
|         - | 1234 | ` *` |
|         - | 1235 | ` * It is not the engine's general randomness -- PH7_VmRandomNum is -- and it is` |
|         - | 1236 | ``  * here for one reason: it is what php's uniqid() puts in the `$more_entropy` `` |
|         - | 1237 | ` * tail, and what its lcg_value() answers. The seed is the clock and the engine's` |
|         - | 1238 | ` * own entropy, taken once, the way php seeds its pair once per process (from the` |
|         - | 1239 | ` * clock and the pid); the STREAM is therefore not reproducible between two runs` |
|         - | 1240 | ` * of either engine, and no test pins its value -- only the SHAPE it lands in.` |
|         - | 1241 | ` */` |
|         - | 1242 | `#define PH7_LCG_MODMULT(a,b,c,m,s) { \` |
|         - | 1243 | `	sxi32 q = (s) / (a); \` |
|         - | 1244 | `	(s) = (b) * ((s) % (a)) - (c) * q; \` |
|         - | 1245 | `	if( (s) < 0 ){ (s) += (m); } \` |
|         - | 1246 | `}` |
|       604 | 1247 | `PH7_PRIVATE double PH7_VmCombinedLcg(ph7_vm *pVm)` |
|         1 | 1248 | `{` |
|         - | 1249 | `	sxi32 z;` |
|       605 | 1250 | `	if( !pVm->bLcgSeeded ){` |
|         3 | 1251 | `		ph7_int64 iSec = 0,iUsec = 0;` |
|         3 | 1252 | `		PH7_VmClockNow(pVm,&iSec,&iUsec);` |
|         3 | 1253 | `		pVm->iLcgS1 = (sxi32)(iSec ^ (iUsec << 11));` |
|         3 | 1254 | `		pVm->iLcgS2 = (sxi32)PH7_VmRandomNum(&(*pVm));` |
|         3 | 1255 | `		PH7_VmClockNow(pVm,&iSec,&iUsec);` |
|         3 | 1256 | `		pVm->iLcgS2 ^= (sxi32)(iUsec << 11);` |
|         - | 1257 | `		/* Both streams must start inside their own modulus and away from zero,` |
|         - | 1258 | `		 * which a raw clock word is not. */` |
|         3 | 1259 | `		if( pVm->iLcgS1 < 1 ){` |
|       ! 0 | 1260 | `			pVm->iLcgS1 = -pVm->iLcgS1;` |
|       ! 0 | 1261 | `		}` |
|         3 | 1262 | `		if( pVm->iLcgS2 < 1 ){` |
|         2 | 1263 | `			pVm->iLcgS2 = -pVm->iLcgS2;` |
|         1 | 1264 | `		}` |
|         3 | 1265 | `		pVm->iLcgS1 = (pVm->iLcgS1 % 2147483562) + 1;` |
|         3 | 1266 | `		pVm->iLcgS2 = (pVm->iLcgS2 % 2147483398) + 1;` |
|         3 | 1267 | `		pVm->bLcgSeeded = 1;` |
|         1 | 1268 | `	}` |
|       605 | 1269 | `	PH7_LCG_MODMULT(53668,40014,12211,2147483563L,pVm->iLcgS1)` |
|       605 | 1270 | `	PH7_LCG_MODMULT(52774,40692,3791,2147483399L,pVm->iLcgS2)` |
|       605 | 1271 | `	z = pVm->iLcgS1 - pVm->iLcgS2;` |
|       605 | 1272 | `	if( z < 1 ){` |
|       296 | 1273 | `		z += 2147483562;` |
|       143 | 1274 | `	}` |
|       605 | 1275 | `	return z * 4.656613e-10;` |
|         1 | 1276 | `}` |
|         - | 1277 | `/*` |
|         - | 1278 | ` * string uniqid(string $prefix = "", bool $more_entropy = false)` |
|         - | 1279 | ` *  Generate a unique ID` |
|         - | 1280 | ` *` |
|         - | 1281 | ` * php's id is the CLOCK, not a random number: eight hex digits of epoch seconds` |
|         - | 1282 | `` * and five of the microseconds within them (`%08x%05x`, the microseconds masked`` |
|         - | 1283 | ` * to 0x100000 because five hex digits is all they need). Three things follow,` |
|         - | 1284 | ` * and PH7's SHA1-of-a-random-string answered none of them -- it was 14 hex` |
|         - | 1285 | ` * characters where php's is 13, it did not increase, and it was all DIGITS about` |
|         - | 1286 | ` * once in 1200 calls where php's, carrying the current epoch, effectively never` |
|         - | 1287 | ` * is. That last one is not cosmetic: Respect\Validation feeds a uniqid() through` |
|         - | 1288 | `` * `ctype_digit()`, and an all-digit id becomes an int on one side of a`` |
|         - | 1289 | ` * comparison and stays a string on the other.` |
|         - | 1290 | ` *` |
|         - | 1291 | `` * php also SLEEPS a microsecond first when `$more_entropy` is false, so two`` |
|         - | 1292 | ` * calls in a row cannot land in the same microsecond and the ids are strictly` |
|         - | 1293 | ` * increasing. It does that on POSIX only -- its own Windows build has no` |
|         - | 1294 | ` * usleep() there and makes no such promise -- and so does this.` |
|         - | 1295 | ` */` |
|      1246 | 1296 | `PH7_PRIVATE int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 1297 | `{` |
|      1248 | 1298 | `	ph7_vm *pVm = pCtx->pVm;` |
|      1248 | 1299 | `	ph7_int64 iSec = 0,iUsec = 0;` |
|      1248 | 1300 | `	const char *zPrefix = 0;` |
|      1248 | 1301 | `	int nPrefix = 0;` |
|      1248 | 1302 | `	int bEntropy = 0;` |
|      1248 | 1303 | `	if( nArg > 0 ){` |
|       619 | 1304 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|       619 | 1305 | `		if( nArg > 1 ){` |
|       607 | 1306 | `			bEntropy = ph7_value_to_bool(apArg[1]);` |
|       303 | 1307 | `		}` |
|       309 | 1308 | `	}` |
|         - | 1309 | `#if defined(__UNIXES__)` |
|      1246 | 1310 | `	if( !bEntropy ){` |
|       642 | 1311 | `		usleep(1);` |
|       321 | 1312 | `	}` |
|         - | 1313 | `#endif` |
|      1248 | 1314 | `	PH7_VmClockNow(pVm,&iSec,&iUsec);` |
|      1248 | 1315 | `	if( nPrefix > 0 ){` |
|        15 | 1316 | `		ph7_result_string(pCtx,zPrefix,nPrefix);` |
|         7 | 1317 | `	}` |
|         - | 1318 | ``	/* The seconds are php's `(int) tv.tv_sec` -- a 32-bit field, so the eight`` |
|         - | 1319 | `	 * hex digits are the low word and stay eight after 2038 rather than growing` |
|         - | 1320 | `	 * a ninth. */` |
|      1871 | 1321 | `	ph7_result_string_format(pCtx,"%08x%05x",(unsigned int)(sxu32)iSec,` |
|      1246 | 1322 | `		(unsigned int)(iUsec % 0x100000));` |
|      1248 | 1323 | `	if( bEntropy ){` |
|         - | 1324 | ``		/* php's `%.8F` of the LCG times ten: one digit, a point and eight more. */`` |
|       605 | 1325 | `		ph7_result_string_format(pCtx,"%.8f",PH7_VmCombinedLcg(pVm) * 10);` |
|       302 | 1326 | `	}` |
|      1248 | 1327 | `	return PH7_OK;` |
|         2 | 1328 | `}` |
|         - | 1329 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 1330 | `/*` |
|         - | 1331 | ` * Section:` |
|         - | 1332 | ` *  Language construct implementation as foreign functions.` |
|         - | 1333 | ` * Status:` |
|         - | 1334 | ` *    Stable.` |
|         - | 1335 | ` */` |
|         - | 1336 | `/*` |
|         - | 1337 | ` * The user-visible string coercion an OUTPUT construct performs on one of its` |
|         - | 1338 | ` * arguments (echo/print reached as host functions rather than as OP_CONSUME).` |
|         - | 1339 | ` * An ARRAY warns and still renders as "Array"; an object whose class has no` |
|         - | 1340 | ` * __toString() is php's catchable "could not be converted to string" Error,` |
|         - | 1341 | ` * and the construct outputs nothing for it. The status is recorded on the` |
|         - | 1342 | ` * context too, so OP_CALL cannot treat the throwing call as a normal return.` |
|         - | 1343 | ` */` |
|        60 | 1344 | `static sxi32 VmOutputArgToString(ph7_context *pCtx,ph7_value *pArg,const char **pzData,int *pnLen)` |
|         4 | 1345 | `{` |
|        64 | 1346 | `	sxi32 rc = PH7_MemObjToStringUV(pArg);` |
|        64 | 1347 | `	if( rc != SXRET_OK ){` |
|         3 | 1348 | `		pCtx->nThrowRc = rc;` |
|         3 | 1349 | `		return rc;` |
|         - | 1350 | `	}` |
|        62 | 1351 | `	*pzData = ph7_value_to_string(pArg,pnLen);` |
|        62 | 1352 | `	return SXRET_OK;` |
|        34 | 1353 | `}` |
|         - | 1354 | `/*` |
|         - | 1355 | ` * int print($string...)` |
|         - | 1356 | ` *  Output one or more messages.` |
|         - | 1357 | ` * Parameters` |
|         - | 1358 | ` *  $string` |
|         - | 1359 | ` *   Message to output.` |
|         - | 1360 | ` * Return` |
|         - | 1361 | ` *  1 always.` |
|         - | 1362 | ` */` |
|        60 | 1363 | `PH7_PRIVATE int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 1364 | `{` |
|         - | 1365 | `	const char *zData;` |
|        64 | 1366 | `	int nDataLen = 0;` |
|         - | 1367 | `	ph7_vm *pVm;` |
|         - | 1368 | `	int i,rc;` |
|         - | 1369 | `	/* Point to the target VM */` |
|        64 | 1370 | `	pVm = pCtx->pVm;` |
|         - | 1371 | `	/* Output */` |
|       122 | 1372 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        64 | 1373 | `		sxi32 rcSv = VmOutputArgToString(pCtx,apArg[i],&zData,&nDataLen);` |
|        64 | 1374 | `		if( rcSv != SXRET_OK ){` |
|         3 | 1375 | `			return rcSv;` |
|         - | 1376 | `		}` |
|        62 | 1377 | `		if( nDataLen > 0 ){` |
|        62 | 1378 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        62 | 1379 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        62 | 1380 | `			if( rc == SXERR_ABORT ){` |
|         - | 1381 | `				/* Output consumer callback request an operation abort */` |
|       ! 0 | 1382 | `				return PH7_ABORT;` |
|         - | 1383 | `			}` |
|        29 | 1384 | `		}` |
|        33 | 1385 | `	}` |
|         - | 1386 | `	/* Return 1 */` |
|        62 | 1387 | `	ph7_result_int(pCtx,1);` |
|        62 | 1388 | `	return SXRET_OK;` |
|        34 | 1389 | `}` |
|         - | 1390 | `/*` |
|         - | 1391 | ` * void exit(string $msg)` |
|         - | 1392 | ` * void exit(int $status)` |
|         - | 1393 | ` * void die(string $ms)` |
|         - | 1394 | ` * void die(int $status)` |
|         - | 1395 | ` *   Output a message and terminate program execution.` |
|         - | 1396 | ` * Parameter` |
|         - | 1397 | ` *  If status is a string, this function prints the status just before exiting.` |
|         - | 1398 | ` *  If status is an integer, that value will be used as the exit status` |
|         - | 1399 | ` *  and not printed` |
|         - | 1400 | ` * Return` |
|         - | 1401 | ` *  NULL` |
|         - | 1402 | ` */` |
|       ! 0 | 1403 | `PH7_PRIVATE int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       ! 0 | 1404 | `{` |
|       ! 0 | 1405 | `	if( nArg > 0 ){` |
|       ! 0 | 1406 | `		if( ph7_value_is_string(apArg[0]) ){` |
|         - | 1407 | `			const char *zData;` |
|       ! 0 | 1408 | `			int iLen = 0;` |
|         - | 1409 | `			/* Print exit message */` |
|       ! 0 | 1410 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|       ! 0 | 1411 | `			ph7_context_output(pCtx,zData,iLen);` |
|       ! 0 | 1412 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|         - | 1413 | `			sxi32 iExitStatus;` |
|         - | 1414 | `			/* Record exit status code */` |
|       ! 0 | 1415 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|       ! 0 | 1416 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|       ! 0 | 1417 | `		}` |
|       ! 0 | 1418 | `	}` |
|         - | 1419 | `	/* Request a VM-wide halt (see PH7_OP_HALT) and abort processing` |
|         - | 1420 | `	 * immediately; the abort unwinds enclosing frames and execution units.` |
|         - | 1421 | `	 */` |
|       ! 0 | 1422 | `	pCtx->pVm->bHaltRequested = 1;` |
|       ! 0 | 1423 | `	return PH7_ABORT;` |
|       ! 0 | 1424 | `}` |
|         - | 1425 | `/*` |
|         - | 1426 | ` * Section:` |
|         - | 1427 | ` *  Version,Credits and Copyright related functions.` |
|         - | 1428 | ` * Status:` |
|         - | 1429 | ` *    Stable.` |
|         - | 1430 | ` */` |
|         - | 1431 | `/*` |
|         - | 1432 | ` * string ph7version(void)` |
|         - | 1433 | ` *  Returns the running version of the PH7 version.` |
|         - | 1434 | ` * Parameters` |
|         - | 1435 | ` *  None` |
|         - | 1436 | ` * Return` |
|         - | 1437 | ` * Current PH7 version.` |
|         - | 1438 | ` */` |
|         2 | 1439 | `PH7_PRIVATE int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 1440 | `{` |
|         1 | 1441 | `	SXUNUSED(nArg);` |
|         1 | 1442 | `	SXUNUSED(apArg); /* cc warning */` |
|         - | 1443 | `	/* Current engine version */` |
|         3 | 1444 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|         3 | 1445 | `	return PH7_OK;` |
|         1 | 1446 | `}` |
|         - | 1447 | `/*` |
|         - | 1448 | ` * string\|false phpversion([ ?string $extension = null ])` |
|         - | 1449 | ` *  Returns the PHP-compatibility version PHL advertises (see PHP_COMPAT_VERSION).` |
|         - | 1450 | ` * Parameters` |
|         - | 1451 | ` *  $extension (optional): an extension name, matched case-insensitively against` |
|         - | 1452 | ` *  the ones this engine reports as loaded.` |
|         - | 1453 | ` * Return` |
|         - | 1454 | ` *  The version string — for the engine with no argument (or an explicit NULL),` |
|         - | 1455 | ` *  and for a loaded extension, whose version IS the engine's since every one of` |
|         - | 1456 | ` *  them is part of it — or FALSE for a name it does not report.` |
|         - | 1457 | ` */` |
|       244 | 1458 | `PH7_PRIVATE int vm_builtin_phpversion(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 1459 | `{` |
|         - | 1460 | `	/* $extension was declared in the signature and answered NULL for everything:` |
|         - | 1461 | `	 * an unknown one where php answers FALSE (so the documented` |
|         - | 1462 | ``	 * `if (phpversion($e) === false)` check never fired and a version comparison`` |
|         - | 1463 | `	 * ran against NULL), a KNOWN one where php answers the version string, and` |
|         - | 1464 | `	 * even the explicit NULL that means "no extension" at all.` |
|         - | 1465 | `	 *` |
|         - | 1466 | `	 * Every extension this engine reports as loaded is part of the engine, so its` |
|         - | 1467 | `	 * version IS the engine's — which is also what php answers for its own` |
|         - | 1468 | `	 * bundled ones — and a name it does not report is php's false. */` |
|       246 | 1469 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0 ){` |
|         - | 1470 | `		int nName;` |
|       238 | 1471 | `		const char *zName = ph7_value_to_string(apArg[0],&nName);` |
|       238 | 1472 | `		if( !PH7_VmExtensionIsLoaded(pCtx->pVm,zName,nName) ){` |
|        15 | 1473 | `			ph7_result_bool(pCtx,0);` |
|        15 | 1474 | `			return PH7_OK;` |
|         - | 1475 | `		}` |
|       111 | 1476 | `	}` |
|       232 | 1477 | `	ph7_result_string(pCtx,PHP_COMPAT_VERSION,(int)sizeof(PHP_COMPAT_VERSION) - 1);` |
|       232 | 1478 | `	return PH7_OK;` |
|       124 | 1479 | `}` |
|         - | 1480 | `/*` |
|         - | 1481 | ` * string php_sapi_name(void)` |
|         - | 1482 | ` *  Returns the type of interface (SAPI) PHL is running under.` |
|         - | 1483 | ` * Parameters` |
|         - | 1484 | ` *  None` |
|         - | 1485 | ` * Return` |
|         - | 1486 | ` *  "cli-server" while serving an HTTP request via the built-in -S server, "cli" otherwise.` |
|         - | 1487 | ` */` |
|         2 | 1488 | `PH7_PRIVATE int vm_builtin_php_sapi_name(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 1489 | `{` |
|         3 | 1490 | `	const char *zSapi = pCtx->pVm->bHttpContext ? "cli-server" : "cli";` |
|         1 | 1491 | `	SXUNUSED(nArg);` |
|         1 | 1492 | `	SXUNUSED(apArg); /* cc warning */` |
|         3 | 1493 | `	ph7_result_string(pCtx,zSapi,-1);` |
|         3 | 1494 | `	return PH7_OK;` |
|         1 | 1495 | `}` |
|         - | 1496 | `/*` |
|         - | 1497 | ` * string\|false php_ini_loaded_file(void)` |
|         - | 1498 | ` *  Which php.ini this interpreter read.` |
|         - | 1499 | ` * Return` |
|         - | 1500 | ` *  The path of the file, under the name php quotes for it -- absolute and` |
|         - | 1501 | `` *  canonical, which is what the CLI's `-c` door resolved its argument to and`` |
|         - | 1502 | ` *  the same string a refusal inside that file is dated by. FALSE when no file` |
|         - | 1503 | ` *  was read at all: PHL has no configuration file of its own, so that is the` |
|         - | 1504 | `` *  answer whenever `-c` was not given, and it is php's OWN answer rather than a`` |
|         - | 1505 | `` *  stub -- a php started with `-n`, or built with no php.ini in its search path,`` |
|         - | 1506 | ` *  answers exactly this. Composer's XdebugHandler asks on its way to reporting` |
|         - | 1507 | ` *  where a directive came from, and takes "nowhere" for an answer.` |
|         - | 1508 | ` */` |
|        20 | 1509 | `PH7_PRIVATE int vm_builtin_php_ini_loaded_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       ! 0 | 1510 | `{` |
|        20 | 1511 | `	SyString *pFile = &pCtx->pVm->pEngine->xConf.sIniFile;` |
|        10 | 1512 | `	SXUNUSED(nArg);` |
|        10 | 1513 | `	SXUNUSED(apArg);` |
|        20 | 1514 | `	if( pFile->nByte < 1 ){` |
|         6 | 1515 | `		ph7_result_bool(pCtx,0);` |
|         3 | 1516 | `	}else{` |
|        14 | 1517 | `		ph7_result_string(pCtx,pFile->zString,(int)pFile->nByte);` |
|         - | 1518 | `	}` |
|        20 | 1519 | `	return PH7_OK;` |
|       ! 0 | 1520 | `}` |
|         - | 1521 | `/*` |
|         - | 1522 | ` * string\|false php_ini_scanned_files(void)` |
|         - | 1523 | ` *  Which files the scan directory contributed after php.ini itself.` |
|         - | 1524 | ` * Return` |
|         - | 1525 | ` *  FALSE, always, and this one is not paired with php_ini_loaded_file(): php` |
|         - | 1526 | ` *  answers it from --with-config-file-scan-dir, a build-time directory PHL has` |
|         - | 1527 | ` *  none of. A php built without one answers FALSE here while still naming a` |
|         - | 1528 | ` *  loaded php.ini above.` |
|         - | 1529 | ` */` |
|       ! 0 | 1530 | `PH7_PRIVATE int vm_builtin_php_ini_scanned_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       ! 0 | 1531 | `{` |
|       ! 0 | 1532 | `	SXUNUSED(nArg);` |
|       ! 0 | 1533 | `	SXUNUSED(apArg);` |
|       ! 0 | 1534 | `	ph7_result_bool(pCtx,0);` |
|       ! 0 | 1535 | `	return PH7_OK;` |
|       ! 0 | 1536 | `}` |
|         - | 1537 | `/*` |
|         - | 1538 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|         - | 1539 | ` */` |
|         - | 1540 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|         - | 1541 | ` "<html><head>"\` |
|         - | 1542 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|         - | 1543 | ` "<style type=\"text/css\">"\` |
|         - | 1544 | ` "div {"\` |
|         - | 1545 | `     "border: 1px solid #cccccc;"\` |
|         - | 1546 | `     "-moz-border-radius-topleft: 10px;"\` |
|         - | 1547 | `     "-moz-border-radius-bottomright: 10px;"\` |
|         - | 1548 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|         - | 1549 | `     "-moz-border-radius-topright: 10px;"\` |
|         - | 1550 | `     "-webkit-border-radius: 10px;"\` |
|         - | 1551 | `     "-o-border-radius: 10px;"\` |
|         - | 1552 | `     "border-radius: 10px;"\` |
|         - | 1553 | `     "padding-left: 2em;"\` |
|         - | 1554 | `     "background-color: white;"\` |
|         - | 1555 | `     "margin-left: auto;"\` |
|         - | 1556 | `     "font-family: verdana;"\` |
|         - | 1557 | `     "padding-right: 2em;"\` |
|         - | 1558 | `     "margin-right: auto;"\` |
|         - | 1559 | `     "}"\` |
|         - | 1560 | `     "body {"\` |
|         - | 1561 | `     "padding: 0.2em;"\` |
|         - | 1562 | `     "font-style: normal;"\` |
|         - | 1563 | `     "font-size: medium;"\` |
|         - | 1564 | `     "background-color: #f2f2f2;"\` |
|         - | 1565 | `     "}"\` |
|         - | 1566 | `     "hr {"\` |
|         - | 1567 | `     "border-style: solid none none;"\` |
|         - | 1568 | `     "border-width: 1px medium medium;"\` |
|         - | 1569 | `     "border-top: 1px solid #cccccc;"\` |
|         - | 1570 | `     "height: 1px;"\` |
|         - | 1571 | `     "}"\` |
|         - | 1572 | `     "a {"\` |
|         - | 1573 | `     "color: #3366cc;"\` |
|         - | 1574 | `     "text-decoration: none;"\` |
|         - | 1575 | `     "}"\` |
|         - | 1576 | `     "a:hover {"\` |
|         - | 1577 | `     "color: #999999;"\` |
|         - | 1578 | `     "}"\` |
|         - | 1579 | `     "a:active {"\` |
|         - | 1580 | `     "color: #663399;"\` |
|         - | 1581 | `     "}"\` |
|         - | 1582 | `     "h1 {"\` |
|         - | 1583 | `     "margin: 0;"\` |
|         - | 1584 | `     "padding: 0;"\` |
|         - | 1585 | `     "font-family: Verdana;"\` |
|         - | 1586 | `     "font-weight: bold;"\` |
|         - | 1587 | `     "font-style: normal;"\` |
|         - | 1588 | `     "font-size: medium;"\` |
|         - | 1589 | `     "text-transform: capitalize;"\` |
|         - | 1590 | `     "color: #0a328c;"\` |
|         - | 1591 | `     "}"\` |
|         - | 1592 | `     "p {"\` |
|         - | 1593 | `     "margin: 0 auto;"\` |
|         - | 1594 | `     "font-size: medium;"\` |
|         - | 1595 | `     "font-style: normal;"\` |
|         - | 1596 | `     "font-family: verdana;"\` |
|         - | 1597 | `     "}"\` |
|         - | 1598 | `"</style></head><body>"\` |
|         - | 1599 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|         - | 1600 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|         - | 1601 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|         - | 1602 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|         - | 1603 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|         - | 1604 | `"<p style=\"text-align: left;\"><small><small>"\` |
|         - | 1605 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|         - | 1606 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|         - | 1607 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|         - | 1608 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|         - | 1609 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|         - | 1610 |  |
|         - | 1611 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|         - | 1612 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|         - | 1613 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|         - | 1614 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|         - | 1615 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|         - | 1616 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|         - | 1617 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|         - | 1618 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|         - | 1619 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|         - | 1620 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|         - | 1621 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|         - | 1622 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|         - | 1623 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|         - | 1624 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|         - | 1625 |  |
|         - | 1626 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|         - | 1627 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|         - | 1628 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|         - | 1629 | `"&nbsp;*<br>"\` |
|         - | 1630 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|         - | 1631 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|         - | 1632 | `"&nbsp;* are met:<br>"\` |
|         - | 1633 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|         - | 1634 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|         - | 1635 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|         - | 1636 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|         - | 1637 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|         - | 1638 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|         - | 1639 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|         - | 1640 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|         - | 1641 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|         - | 1642 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|         - | 1643 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|         - | 1644 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|         - | 1645 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|         - | 1646 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|         - | 1647 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|         - | 1648 | `"&nbsp;*<br>"\` |
|         - | 1649 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|         - | 1650 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|         - | 1651 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|         - | 1652 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|         - | 1653 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|         - | 1654 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|         - | 1655 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|         - | 1656 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|         - | 1657 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|         - | 1658 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|         - | 1659 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|         - | 1660 | `"&nbsp;*/<br>"\` |
|         - | 1661 | `"</span></small></small></p>"\` |
|         - | 1662 | `"</div></body></html>"` |
|         - | 1663 | `/*` |
|         - | 1664 | ` * bool ph7credits(void)` |
|         - | 1665 | ` * bool ph7info(void)` |
|         - | 1666 | ` * bool ph7copyright(void)` |
|         - | 1667 | ` *  Prints out the credits for PH7 engine` |
|         - | 1668 | ` * Parameters` |
|         - | 1669 | ` *  None` |
|         - | 1670 | ` * Return` |
|         - | 1671 | ` *  Always TRUE` |
|         - | 1672 | ` */` |
|         2 | 1673 | `PH7_PRIVATE int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 1674 | `{` |
|         3 | 1675 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|         - | 1676 | `	/* Expand the HTML page above*/` |
|         3 | 1677 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|         2 | 1678 | `	ph7_context_output_format(` |
|         1 | 1679 | `		pCtx,` |
|         - | 1680 | `		PH7_HTML_PAGE_FORMAT,` |
|         1 | 1681 | `		ph7_lib_version(),   /* Engine version */` |
|         1 | 1682 | `		ph7_lib_signature(), /* Engine signature */` |
|         1 | 1683 | `		ph7_lib_ident(),     /* Engine ID */` |
|         2 | 1684 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|         2 | 1685 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|         1 | 1686 | `		SyHashTotalEntry(&pVm->hClass),` |
|         - | 1687 | `#ifdef __WINNT__` |
|         - | 1688 | `		"Windows NT"` |
|         - | 1689 | `#elif defined(__UNIXES__)` |
|         - | 1690 | `		"UNIX-Like"` |
|         - | 1691 | `#else` |
|         - | 1692 | `		"Other OS"` |
|         - | 1693 | `#endif` |
|         - | 1694 | `		);` |
|         3 | 1695 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|         1 | 1696 | `	SXUNUSED(nArg); /* cc warning */` |
|         1 | 1697 | `	SXUNUSED(apArg);` |
|         - | 1698 | `	/* Return TRUE */` |
|         - | 1699 | `	//ph7_result_bool(pCtx,1);` |
|         3 | 1700 | `	return PH7_OK;` |
|         1 | 1701 | `}` |
|         - | 1702 | `/*` |
|         - | 1703 | ` * Section:` |
|         - | 1704 | ` *    URL related routines.` |
|         - | 1705 | ` * Status:` |
|         - | 1706 | ` *    Stable.` |
|         - | 1707 | ` */` |
|         - | 1708 | `/*` |
|         - | 1709 | ` * value parse_url(string $url [, int $component = -1 ])` |
|         - | 1710 | ` *  Parse a URL and return its fields.` |
|         - | 1711 | ` * Parameters` |
|         - | 1712 | ` *  $url` |
|         - | 1713 | ` *   The URL to parse.` |
|         - | 1714 | ` * $component` |
|         - | 1715 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|         - | 1716 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|         - | 1717 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|         - | 1718 | ` *  in which case the return value will be an integer).` |
|         - | 1719 | ` * Return` |
|         - | 1720 | ` *  If the component parameter is omitted, an associative array is returned.` |
|         - | 1721 | ` *  At least one element will be present within the array. Potential keys within` |
|         - | 1722 | ` *  this array are:` |
|         - | 1723 | ` *   scheme - e.g. http` |
|         - | 1724 | ` *   host` |
|         - | 1725 | ` *   port` |
|         - | 1726 | ` *   user` |
|         - | 1727 | ` *   pass` |
|         - | 1728 | ` *   path` |
|         - | 1729 | ` *   query - after the question mark ?` |
|         - | 1730 | ` *   fragment - after the hashmark #` |
|         - | 1731 | ` * Note:` |
|         - | 1732 | ` *  FALSE is returned on failure.` |
|         - | 1733 | ` *  This function work with relative URL unlike the one shipped` |
|         - | 1734 | ` *  with the standard PHP engine.` |
|         - | 1735 | ` */` |
|         - | 1736 | `/*` |
|         - | 1737 | ` * parse_url() component set.` |
|         - | 1738 | ` *` |
|         - | 1739 | ` * A bare SyString cannot express "present but empty", which php needs:` |
|         - | 1740 | ` * parse_url("") is ['path'=>''] and parse_url("?") is ['query'=>''], both` |
|         - | 1741 | ` * distinct from the component being absent. So presence is tracked separately.` |
|         - | 1742 | ` */` |
|      1328 | 1743 | `static int VmUrlIsAlnum(int c)` |
|         2 | 1744 | `{` |
|      1330 | 1745 | `	return (c >= '0' && c <= '9') \|\| (c >= 'a' && c <= 'z') \|\| (c >= 'A' && c <= 'Z');` |
|         2 | 1746 | `}` |
|        12 | 1747 | `static int VmUrlIsAlpha(int c)` |
|         2 | 1748 | `{` |
|        14 | 1749 | `	return (c >= 'a' && c <= 'z') \|\| (c >= 'A' && c <= 'Z');` |
|         2 | 1750 | `}` |
|         - | 1751 | `/* scheme = ALNUM *( ALNUM / "+" / "-" / "." ) -- php accepts a digit first. */` |
|      1328 | 1752 | `static int VmUrlIsSchemeByte(int c)` |
|         2 | 1753 | `{` |
|      1330 | 1754 | `	return VmUrlIsAlnum(c) \|\| c == '+' \|\| c == '-' \|\| c == '.';` |
|         2 | 1755 | `}` |
|         - | 1756 | `/*` |
|         - | 1757 | ` * Resolve the port span that followed the ':' in an authority.` |
|         - | 1758 | ` *` |
|         - | 1759 | ` * Returns 1 (usable port in *piPort), 0 (the span is empty, so there is simply` |
|         - | 1760 | ` * no port) or -1 (php rejects the whole URL). php is lenient about what follows` |
|         - | 1761 | ` * the digits -- ":8a" and ":8 0" both yield 8 -- but demands at least one digit` |
|         - | 1762 | ` * and a value that fits a port, so ":abc", ":-80" and ":65536" are all failures.` |
|         - | 1763 | ` */` |
|        92 | 1764 | `static int VmUrlParsePort(const char *z,int n,int *piPort)` |
|         2 | 1765 | `{` |
|        94 | 1766 | `	int i = 0,iVal = 0,nDigit = 0;` |
|        94 | 1767 | `	if( n < 1 ){` |
|       ! 0 | 1768 | `		return 0;` |
|         - | 1769 | `	}` |
|       140 | 1770 | `	while( i < n && (z[i] == ' ' \|\| z[i] == '\t') ){` |
|       ! 0 | 1771 | `		i++;` |
|       ! 0 | 1772 | `	}` |
|        94 | 1773 | `	if( i < n && (z[i] == '+' \|\| z[i] == '-') ){` |
|       ! 0 | 1774 | `		if( z[i] == '-' ){` |
|       ! 0 | 1775 | `			return -1;` |
|         - | 1776 | `		}` |
|       ! 0 | 1777 | `		i++;` |
|       ! 0 | 1778 | `	}` |
|       318 | 1779 | `	while( i < n && z[i] >= '0' && z[i] <= '9' ){` |
|       226 | 1780 | `		iVal = iVal * 10 + (z[i] - '0');` |
|       226 | 1781 | `		if( iVal > 65535 ){` |
|       ! 0 | 1782 | `			return -1; /* Out of range, and this also caps the accumulator */` |
|         - | 1783 | `		}` |
|       226 | 1784 | `		nDigit++;` |
|       226 | 1785 | `		i++;` |
|         2 | 1786 | `	}` |
|        94 | 1787 | `	if( nDigit < 1 ){` |
|        12 | 1788 | `		return -1;` |
|         - | 1789 | `	}` |
|        84 | 1790 | `	*piPort = iVal;` |
|        84 | 1791 | `	return 1;` |
|        48 | 1792 | `}` |
|         - | 1793 | `/*` |
|         - | 1794 | ` * Split an authority -- "[user[:pass]@]host[:port]" -- into pOut.` |
|         - | 1795 | ` * Returns 0 for the malformed input php reports as FALSE.` |
|         - | 1796 | ` */` |
|       278 | 1797 | `static int VmUrlParseAuthority(const char *z,int n,VmUrlParts *pOut,int bPortKnown)` |
|         2 | 1798 | `{` |
|         - | 1799 | `	const char *zHost;` |
|       280 | 1800 | `	int i,iAt = -1,iColon = -1,iSep = -1,nHost,iPort = 0,rc;` |
|         - | 1801 | `	/* php splits the credentials at the LAST '@' ("//a@b@c" is user "a@b") */` |
|      2110 | 1802 | `	for( i = 0 ; i < n ; ++i ){` |
|      1832 | 1803 | `		if( z[i] == '@' ){` |
|        44 | 1804 | `			iAt = i;` |
|        21 | 1805 | `		}` |
|       917 | 1806 | `	}` |
|       280 | 1807 | `	if( iAt >= 0 ){` |
|         - | 1808 | `		/* and the user from the password at the FIRST ':' before it */` |
|       154 | 1809 | `		for( i = 0 ; i < iAt ; ++i ){` |
|       152 | 1810 | `			if( z[i] == ':' ){` |
|        42 | 1811 | `				iColon = i;` |
|        42 | 1812 | `				break;` |
|         - | 1813 | `			}` |
|        57 | 1814 | `		}` |
|        44 | 1815 | `		if( iColon >= 0 ){` |
|        42 | 1816 | `			SyStringInitFromBuf(&pOut->sUser,z,(sxu32)iColon);` |
|        42 | 1817 | `			SyStringInitFromBuf(&pOut->sPass,&z[iColon+1],(sxu32)(iAt - iColon - 1));` |
|        42 | 1818 | `			pOut->bUser = pOut->bPass = 1;` |
|        22 | 1819 | `		}else{` |
|         3 | 1820 | `			SyStringInitFromBuf(&pOut->sUser,z,(sxu32)iAt);` |
|         3 | 1821 | `			pOut->bUser = 1;` |
|         - | 1822 | `		}` |
|        44 | 1823 | `		z += iAt + 1;` |
|        44 | 1824 | `		n -= iAt + 1;` |
|        21 | 1825 | `	}` |
|       280 | 1826 | `	zHost = z;` |
|       280 | 1827 | `	nHost = n;` |
|       280 | 1828 | `	if( !(n > 1 && z[0] == '[' && z[n-1] == ']') ){` |
|         - | 1829 | `		/* Unless a bracketed IPv6 literal fills the WHOLE authority -- in which` |
|         - | 1830 | `		 * case php keeps the brackets in "host" and scans no port, so every ':'` |
|         - | 1831 | `		 * inside belongs to the address -- the port hangs off the LAST ':'.` |
|         - | 1832 | `		 * php decides that on the first and last byte alone, which is why` |
|         - | 1833 | `		 * "//[:]\|]" is a single host while "//[::1]:8080" splits a port off. */` |
|      1838 | 1834 | `		for( i = 0 ; i < n ; ++i ){` |
|      1562 | 1835 | `			if( z[i] == ':' ){` |
|       118 | 1836 | `				iSep = i;` |
|        58 | 1837 | `			}` |
|       782 | 1838 | `		}` |
|       278 | 1839 | `		if( iSep >= 0 ){` |
|         - | 1840 | `			/* The host is trimmed at the ':' whether or not the port was already` |
|         - | 1841 | `			 * resolved by the caller. */` |
|        94 | 1842 | `			nHost = iSep;` |
|        94 | 1843 | `			if( !bPortKnown ){` |
|        88 | 1844 | `				rc = VmUrlParsePort(&z[iSep+1],n - iSep - 1,&iPort);` |
|        88 | 1845 | `				if( rc < 0 ){` |
|        12 | 1846 | `					return 0;` |
|         - | 1847 | `				}` |
|        78 | 1848 | `				if( rc > 0 ){` |
|        78 | 1849 | `					pOut->iPort = iPort;` |
|        78 | 1850 | `					pOut->bPort = 1;` |
|        38 | 1851 | `				}` |
|        38 | 1852 | `			}` |
|        41 | 1853 | `		}` |
|       133 | 1854 | `	}` |
|       270 | 1855 | `	if( nHost < 1 ){` |
|         - | 1856 | `		/* php requires a non-empty host once an authority is in play, which is` |
|         - | 1857 | `		 * what makes "//", "http://" and ":80" all FALSE. */` |
|        36 | 1858 | `		return 0;` |
|         - | 1859 | `	}` |
|       236 | 1860 | `	SyStringInitFromBuf(&pOut->sHost,zHost,(sxu32)nHost);` |
|       236 | 1861 | `	pOut->bHost = 1;` |
|       236 | 1862 | `	return 1;` |
|       141 | 1863 | `}` |
|         - | 1864 | `/*` |
|         - | 1865 | ` * Split "path[?query][#fragment]". The fragment is taken FIRST and the query` |
|         - | 1866 | ` * only from what precedes it, so "#a?b" is a fragment of "a?b" with no query.` |
|         - | 1867 | ` */` |
|       278 | 1868 | `static void VmUrlParsePath(const char *z,int n,VmUrlParts *pOut)` |
|         3 | 1869 | `{` |
|       281 | 1870 | `	int i,iEnd = n;` |
|      1669 | 1871 | `	for( i = 0 ; i < n ; ++i ){` |
|      1443 | 1872 | `		if( z[i] == '#' ){` |
|        54 | 1873 | `			SyStringInitFromBuf(&pOut->sFragment,&z[i+1],(sxu32)(n - i - 1));` |
|        54 | 1874 | `			pOut->bFragment = 1;` |
|        54 | 1875 | `			iEnd = i;` |
|        54 | 1876 | `			break;` |
|         - | 1877 | `		}` |
|       697 | 1878 | `	}` |
|      1399 | 1879 | `	for( i = 0 ; i < iEnd ; ++i ){` |
|      1189 | 1880 | `		if( z[i] == '?' ){` |
|        70 | 1881 | `			SyStringInitFromBuf(&pOut->sQuery,&z[i+1],(sxu32)(iEnd - i - 1));` |
|        70 | 1882 | `			pOut->bQuery = 1;` |
|        70 | 1883 | `			iEnd = i;` |
|        70 | 1884 | `			break;` |
|         - | 1885 | `		}` |
|       562 | 1886 | `	}` |
|       281 | 1887 | `	if( iEnd > 0 ){` |
|       263 | 1888 | `		SyStringInitFromBuf(&pOut->sPath,z,(sxu32)iEnd);` |
|       263 | 1889 | `		pOut->bPath = 1;` |
|       130 | 1890 | `	}` |
|       281 | 1891 | `}` |
|         - | 1892 | `/* Parse "host[:port]" followed by an optional path/query/fragment. */` |
|       278 | 1893 | `static int VmUrlAuthorityThenPath(const char *z,int n,VmUrlParts *pOut,int bPortKnown)` |
|         2 | 1894 | `{` |
|       280 | 1895 | `	int i,iEnd = n;` |
|      2110 | 1896 | `	for( i = 0 ; i < n ; ++i ){` |
|      2020 | 1897 | `		if( z[i] == '/' \|\| z[i] == '?' \|\| z[i] == '#' ){` |
|       190 | 1898 | `			iEnd = i;` |
|       190 | 1899 | `			break;` |
|         - | 1900 | `		}` |
|       917 | 1901 | `	}` |
|       280 | 1902 | `	if( !VmUrlParseAuthority(z,iEnd,pOut,bPortKnown) ){` |
|        46 | 1903 | `		return 0;` |
|         - | 1904 | `	}` |
|       236 | 1905 | `	if( iEnd < n ){` |
|       170 | 1906 | `		VmUrlParsePath(&z[iEnd],n - iEnd,pOut);` |
|        84 | 1907 | `	}` |
|       236 | 1908 | `	return 1;` |
|       141 | 1909 | `}` |
|         - | 1910 | `/*` |
|         - | 1911 | ` * Resolve the port php took from the FIRST ':' of a host:port URL.` |
|         - | 1912 | ` *` |
|         - | 1913 | ` * php reads the port straight off that colon before it works out where the host` |
|         - | 1914 | ` * ends, so the digits can even belong to what becomes the path: parse_url()` |
|         - | 1915 | ` * reports port 1 for "a/:1", whose path is "/:1". Mirroring the order keeps` |
|         - | 1916 | ` * that quirk. Returns 0 for a port php rejects.` |
|         - | 1917 | ` */` |
|         6 | 1918 | `static int VmUrlPreparePort(const char *z,int k,int nEnd,VmUrlParts *pOut)` |
|         1 | 1919 | `{` |
|         7 | 1920 | `	int iPort = 0;` |
|         7 | 1921 | `	int rc = VmUrlParsePort(&z[k+1],nEnd - k - 1,&iPort);` |
|         7 | 1922 | `	if( rc < 0 ){` |
|       ! 0 | 1923 | `		return 0;` |
|         - | 1924 | `	}` |
|         7 | 1925 | `	if( rc > 0 ){` |
|         7 | 1926 | `		pOut->iPort = iPort;` |
|         7 | 1927 | `		pOut->bPort = 1;` |
|         3 | 1928 | `	}` |
|         7 | 1929 | `	return 1;` |
|         4 | 1930 | `}` |
|         - | 1931 | `/*` |
|         - | 1932 | ` * php's URL parser, as parse_url() needs it. Returns 0 where php returns FALSE.` |
|         - | 1933 | ` *` |
|         - | 1934 | ` * PH7_VmHttpSplitURI cannot serve here: it is an HTTP REQUEST-target splitter` |
|         - | 1935 | ` * (the HTTP server and filter_var share it) and assumes the input is` |
|         - | 1936 | ` * authority-first, so it read "a" as a host and "mailto:me@x.com" as` |
|         - | 1937 | ` * user:pass@host. php instead decides authority-vs-path up front: only a "//",` |
|         - | 1938 | ` * with or without a scheme before it, introduces an authority.` |
|         - | 1939 | ` */` |
|       392 | 1940 | `PH7_PRIVATE int PH7_VmUrlSplit(const char *z,int n,VmUrlParts *pOut)` |
|         3 | 1941 | `{` |
|       395 | 1942 | `	int i,k = -1,bScheme,bPortForm = 0,nPortEnd = 0;` |
|       395 | 1943 | `	SyZero(pOut,sizeof(VmUrlParts));` |
|         - | 1944 | `	/* A leading "//" settles it before any colon is considered: the authority` |
|         - | 1945 | `	 * starts after the slashes. Reading the colon first turned "//h:80" into a` |
|         - | 1946 | `	 * host called "//h" and "//[::1]" into a path. */` |
|       395 | 1947 | `	if( n >= 2 && z[0] == '/' && z[1] == '/' ){` |
|        24 | 1948 | `		return VmUrlAuthorityThenPath(&z[2],n - 2,pOut,0);` |
|         - | 1949 | `	}` |
|      1883 | 1950 | `	for( i = 0 ; i < n ; ++i ){` |
|      1841 | 1951 | `		if( z[i] == ':' ){` |
|       330 | 1952 | `			k = i;` |
|       330 | 1953 | `			break;` |
|         - | 1954 | `		}` |
|       758 | 1955 | `	}` |
|       373 | 1956 | `	if( k == 0 && n == 1 ){` |
|         - | 1957 | `		/* A lone ":" is an EMPTY scheme, which php rejects outright -- unlike` |
|         - | 1958 | `		 * ":a" or "::", which are simply paths. */` |
|         3 | 1959 | `		return 0;` |
|         - | 1960 | `	}` |
|       371 | 1961 | `	bScheme = k > 0;` |
|      1699 | 1962 | `	for( i = 0 ; bScheme && i < k ; ++i ){` |
|      1330 | 1963 | `		if( !VmUrlIsSchemeByte((unsigned char)z[i]) ){` |
|       ! 0 | 1964 | `			bScheme = 0;` |
|       ! 0 | 1965 | `		}` |
|       666 | 1966 | `	}` |
|       371 | 1967 | `	if( bScheme && k + 1 == n ){` |
|         - | 1968 | `		/* "x:" -- the scheme is the whole URL */` |
|         3 | 1969 | `		SyStringInitFromBuf(&pOut->sScheme,z,(sxu32)k);` |
|         3 | 1970 | `		pOut->bScheme = 1;` |
|         3 | 1971 | `		return 1;` |
|         - | 1972 | `	}` |
|         - | 1973 | `	/* Decide whether that ':' introduces a PORT rather than a scheme: at least` |
|         - | 1974 | `	 * one digit, running to the end of the string or to a '/'. That is what` |
|         - | 1975 | `	 * makes "a:80" a host:port while "a:80?q" is a scheme with path "80", and` |
|         - | 1976 | `	 * it holds however the ':' is reached -- ":1" and "/:1" are both authorities` |
|         - | 1977 | `	 * with an empty host, which php rejects. php caps the run, so a long number` |
|         - | 1978 | `	 * stays a path: "a:123456" is a path, "a:99999" an out-of-range port. */` |
|       369 | 1979 | `	if( k >= 0 ){` |
|       326 | 1980 | `		int p = k + 1;` |
|       326 | 1981 | `		int bBeforeQuery = 1;` |
|       326 | 1982 | `		nPortEnd = k + 1;` |
|         - | 1983 | `		/* A ':' that sits inside a query or fragment is just data: "?:1" is a` |
|         - | 1984 | `		 * query of ":1", not an authority with an empty host. */` |
|      1652 | 1985 | `		for( i = 0 ; i < k ; ++i ){` |
|      1328 | 1986 | `			if( z[i] == '?' \|\| z[i] == '#' ){` |
|       ! 0 | 1987 | `				bBeforeQuery = 0;` |
|       ! 0 | 1988 | `				break;` |
|         - | 1989 | `			}` |
|       665 | 1990 | `		}` |
|       336 | 1991 | `		while( p < n && z[p] >= '0' && z[p] <= '9' ){` |
|        11 | 1992 | `			p++;` |
|         1 | 1993 | `		}` |
|       326 | 1994 | `		if( bBeforeQuery && p > k + 1 && (p >= n \|\| z[p] == '/') && (p - k) < 7 ){` |
|         7 | 1995 | `			bPortForm = 1;` |
|         7 | 1996 | `			nPortEnd = p;` |
|         3 | 1997 | `		}` |
|       162 | 1998 | `	}` |
|       369 | 1999 | `	if( !bScheme ){` |
|        47 | 2000 | `		if( bPortForm ){` |
|         3 | 2001 | `			if( !VmUrlPreparePort(z,k,nPortEnd,pOut) ){` |
|       ! 0 | 2002 | `				return 0;` |
|         - | 2003 | `			}` |
|         3 | 2004 | `			return VmUrlAuthorityThenPath(z,n,pOut,1);` |
|         - | 2005 | `		}` |
|        45 | 2006 | `		VmUrlParsePath(z,n,pOut);` |
|        45 | 2007 | `		if( !pOut->bPath && !pOut->bQuery && !pOut->bFragment ){` |
|         - | 2008 | `			/* php reports the empty URL as an empty PATH, not as no components */` |
|         3 | 2009 | `			SyStringInitFromBuf(&pOut->sPath,z,0);` |
|         3 | 2010 | `			pOut->bPath = 1;` |
|         1 | 2011 | `		}` |
|        45 | 2012 | `		return 1;` |
|         - | 2013 | `	}` |
|       324 | 2014 | `	if( bPortForm ){` |
|         5 | 2015 | `		if( !VmUrlPreparePort(z,k,nPortEnd,pOut) ){` |
|       ! 0 | 2016 | `			return 0;` |
|         - | 2017 | `		}` |
|         5 | 2018 | `		return VmUrlAuthorityThenPath(z,n,pOut,1);` |
|         - | 2019 | `	}` |
|       320 | 2020 | `	SyStringInitFromBuf(&pOut->sScheme,z,(sxu32)k);` |
|       320 | 2021 | `	pOut->bScheme = 1;` |
|       320 | 2022 | `	if( z[k+1] == '/' && k + 2 < n && z[k+2] == '/' ){` |
|       262 | 2023 | `		if( k + 3 < n && z[k+3] == '/' && pOut->sScheme.nByte == 4` |
|        20 | 2024 | `		 && (z[0]=='f'\|\|z[0]=='F') && (z[1]=='i'\|\|z[1]=='I')` |
|        14 | 2025 | `		 && (z[2]=='l'\|\|z[2]=='L') && (z[3]=='e'\|\|z[3]=='E') ){` |
|         - | 2026 | `			/* file:/// has no authority: the path starts at the third slash,` |
|         - | 2027 | `			 * except that a "c:" drive letter swallows it (file:///c:/x is the` |
|         - | 2028 | `			 * path "c:/x"). A '\|' in place of the ':' does NOT count. */` |
|        14 | 2029 | `			int iBase = k + 3;` |
|        14 | 2030 | `			if( iBase + 2 < n && VmUrlIsAlpha((unsigned char)z[iBase+1]) && z[iBase+2] == ':' ){` |
|       ! 0 | 2031 | `				iBase++;` |
|       ! 0 | 2032 | `			}` |
|        14 | 2033 | `			VmUrlParsePath(&z[iBase],n - iBase,pOut);` |
|        14 | 2034 | `			return 1;` |
|         - | 2035 | `		}` |
|       252 | 2036 | `		return VmUrlAuthorityThenPath(&z[k+3],n - k - 3,pOut,0);` |
|         - | 2037 | `	}` |
|         - | 2038 | `	/* "mailto:me@x.com", "x:y", "http:/x": everything after the ':' is a path */` |
|        58 | 2039 | `	VmUrlParsePath(&z[k+1],n - k - 1,pOut);` |
|        58 | 2040 | `	return 1;` |
|       199 | 2041 | `}` |
|         - | 2042 | `/*` |
|         - | 2043 | ` * Emit one parsed component into pValue, replacing control bytes with '_'.` |
|         - | 2044 | ` *` |
|         - | 2045 | ` * php runs php_replace_controlchars_ex over every string component it returns,` |
|         - | 2046 | ` * so a URL carrying a raw NUL, newline or DEL cannot smuggle it through into` |
|         - | 2047 | ` * whatever the caller splices the component into (a header, a log line, a` |
|         - | 2048 | ` * redirect). Bytes >= 0x80 are deliberately left alone -- php only folds the` |
|         - | 2049 | ` * ASCII control range.` |
|         - | 2050 | ` */` |
|       164 | 2051 | `static void VmUrlSetComponent(ph7_value *pValue,const SyString *pComp)` |
|         1 | 2052 | `{` |
|       165 | 2053 | `	const char *z = pComp->zString;` |
|       165 | 2054 | `	sxu32 n = pComp->nByte,i,iRun = 0;` |
|       165 | 2055 | `	if( n < 1 \|\| z == 0 ){` |
|         3 | 2056 | `		ph7_value_string(pValue,"",0);` |
|         3 | 2057 | `		return;` |
|         - | 2058 | `	}` |
|       955 | 2059 | `	for( i = 0 ; i < n ; ++i ){` |
|       793 | 2060 | `		unsigned char c = (unsigned char)z[i];` |
|       793 | 2061 | `		if( c < 0x20 \|\| c == 0x7f ){` |
|         3 | 2062 | `			if( i > iRun ){` |
|         3 | 2063 | `				ph7_value_string(pValue,&z[iRun],(int)(i - iRun));` |
|         1 | 2064 | `			}` |
|         3 | 2065 | `			ph7_value_string(pValue,"_",1);` |
|         3 | 2066 | `			iRun = i + 1;` |
|         1 | 2067 | `		}` |
|       397 | 2068 | `	}` |
|       163 | 2069 | `	if( n > iRun ){` |
|       163 | 2070 | `		ph7_value_string(pValue,&z[iRun],(int)(n - iRun));` |
|        81 | 2071 | `	}` |
|        83 | 2072 | `}` |
|       104 | 2073 | `PH7_PRIVATE int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2074 | `{` |
|         - | 2075 | `	const char *zStr; /* Input string */` |
|         - | 2076 | `	VmUrlParts sUrl;  /* Parse of the given URI */` |
|         - | 2077 | `	SyString *pComp;` |
|         - | 2078 | `	int bHave;` |
|         - | 2079 | `	int nLen;` |
|       105 | 2080 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|         - | 2081 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2082 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2083 | `		return PH7_OK;` |
|         - | 2084 | `	}` |
|         - | 2085 | `	/* Extract the given URI. An empty string is NOT a failure: php parses it as` |
|         - | 2086 | `	 * an empty path. */` |
|       105 | 2087 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       105 | 2088 | `	if( nLen < 0 ){` |
|       ! 0 | 2089 | `		nLen = 0;` |
|       ! 0 | 2090 | `	}` |
|       105 | 2091 | `	if( !PH7_VmUrlSplit(zStr,nLen,&sUrl) ){` |
|         - | 2092 | `		/* Malformed input,return FALSE */` |
|        13 | 2093 | `		ph7_result_bool(pCtx,0);` |
|        13 | 2094 | `		return PH7_OK;` |
|         - | 2095 | `	}` |
|       103 | 2096 | `	if( nArg > 1 && ph7_value_to_int(apArg[1]) >= 0 ){` |
|         - | 2097 | `		/* Refer to constant.c for constants values (php's PHP_URL_* are 0-based;` |
|         - | 2098 | `		 * PHL used to number them from 1, so every literal component id selected` |
|         - | 2099 | `		 * the WRONG field -- and the constants' own tests only asserted "%d",` |
|         - | 2100 | `		 * which any numbering satisfies). A negative id means "the whole array",` |
|         - | 2101 | `		 * which is what the default $component = -1 relies on. */` |
|        27 | 2102 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        27 | 2103 | `		pComp = 0;` |
|        27 | 2104 | `		bHave = 0;` |
|        27 | 2105 | `		switch(nComponent){` |
|         3 | 2106 | `		case 0: /* PHP_URL_SCHEME */   pComp = &sUrl.sScheme;   bHave = sUrl.bScheme;   break;` |
|         5 | 2107 | `		case 1: /* PHP_URL_HOST */     pComp = &sUrl.sHost;     bHave = sUrl.bHost;     break;` |
|         2 | 2108 | `		case 2: /* PHP_URL_PORT */` |
|         5 | 2109 | `			if( sUrl.bPort ){` |
|         5 | 2110 | `				ph7_result_int(pCtx,sUrl.iPort);` |
|         3 | 2111 | `			}else{` |
|       ! 0 | 2112 | `				ph7_result_null(pCtx);` |
|         - | 2113 | `			}` |
|         5 | 2114 | `			return PH7_OK;` |
|         3 | 2115 | `		case 3: /* PHP_URL_USER */     pComp = &sUrl.sUser;     bHave = sUrl.bUser;     break;` |
|         3 | 2116 | `		case 4: /* PHP_URL_PASS */     pComp = &sUrl.sPass;     bHave = sUrl.bPass;     break;` |
|         3 | 2117 | `		case 5: /* PHP_URL_PATH */     pComp = &sUrl.sPath;     bHave = sUrl.bPath;     break;` |
|         5 | 2118 | `		case 6: /* PHP_URL_QUERY */    pComp = &sUrl.sQuery;    bHave = sUrl.bQuery;    break;` |
|         5 | 2119 | `		case 7: /* PHP_URL_FRAGMENT */ pComp = &sUrl.sFragment; bHave = sUrl.bFragment; break;` |
|         1 | 2120 | `		default:` |
|         4 | 2121 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 2122 | `				"parse_url(): Argument #2 ($component) must be a valid URL component identifier, %d given",` |
|         1 | 2123 | `				nComponent);` |
|         - | 2124 | `		}` |
|        21 | 2125 | `		if( bHave ){` |
|        19 | 2126 | `			ph7_value *pOut = ph7_context_new_scalar(pCtx);` |
|        19 | 2127 | `			if( pOut == 0 ){` |
|       ! 0 | 2128 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|       ! 0 | 2129 | `				ph7_result_bool(pCtx,0);` |
|       ! 0 | 2130 | `				return PH7_OK;` |
|         - | 2131 | `			}` |
|        19 | 2132 | `			VmUrlSetComponent(pOut,pComp);` |
|        19 | 2133 | `			ph7_result_value(pCtx,pOut);` |
|        10 | 2134 | `		}else{` |
|         - | 2135 | `			/* No available value,return NULL */` |
|         3 | 2136 | `			ph7_result_null(pCtx);` |
|         - | 2137 | `		}` |
|        11 | 2138 | `	}else{` |
|         - | 2139 | `		ph7_value *pArray,*pValue;` |
|         - | 2140 | `		/* Return an associative array */` |
|        67 | 2141 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|        67 | 2142 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|        67 | 2143 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|         - | 2144 | `			/* Out of memory */` |
|       ! 0 | 2145 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|         - | 2146 | `			/* Return false */` |
|       ! 0 | 2147 | `			ph7_result_bool(pCtx,0);` |
|       ! 0 | 2148 | `			return PH7_OK;` |
|         - | 2149 | `		}` |
|         - | 2150 | `		/* Fill the array, in php's key order. A component is emitted whenever it` |
|         - | 2151 | `		 * is PRESENT, even when empty -- parse_url("?") is ['query'=>'']. */` |
|        67 | 2152 | `		if( sUrl.bScheme ){` |
|        33 | 2153 | `			VmUrlSetComponent(pValue,&sUrl.sScheme);` |
|        33 | 2154 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        33 | 2155 | `			ph7_value_reset_string_cursor(pValue);` |
|        16 | 2156 | `		}` |
|        67 | 2157 | `		if( sUrl.bHost ){` |
|        31 | 2158 | `			VmUrlSetComponent(pValue,&sUrl.sHost);` |
|        31 | 2159 | `			ph7_array_add_strkey_elem(pArray,"host",pValue);` |
|        31 | 2160 | `			ph7_value_reset_string_cursor(pValue);` |
|        15 | 2161 | `		}` |
|        67 | 2162 | `		if( sUrl.bPort ){` |
|        17 | 2163 | `			ph7_value_int(pValue,sUrl.iPort);` |
|        17 | 2164 | `			ph7_array_add_strkey_elem(pArray,"port",pValue);` |
|        17 | 2165 | `			ph7_value_reset_string_cursor(pValue);` |
|         8 | 2166 | `		}` |
|        67 | 2167 | `		if( sUrl.bUser ){` |
|         9 | 2168 | `			VmUrlSetComponent(pValue,&sUrl.sUser);` |
|         9 | 2169 | `			ph7_array_add_strkey_elem(pArray,"user",pValue);` |
|         9 | 2170 | `			ph7_value_reset_string_cursor(pValue);` |
|         4 | 2171 | `		}` |
|        67 | 2172 | `		if( sUrl.bPass ){` |
|         7 | 2173 | `			VmUrlSetComponent(pValue,&sUrl.sPass);` |
|         7 | 2174 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue);` |
|         7 | 2175 | `			ph7_value_reset_string_cursor(pValue);` |
|         3 | 2176 | `		}` |
|        67 | 2177 | `		if( sUrl.bPath ){` |
|        47 | 2178 | `			VmUrlSetComponent(pValue,&sUrl.sPath);` |
|        47 | 2179 | `			ph7_array_add_strkey_elem(pArray,"path",pValue);` |
|        47 | 2180 | `			ph7_value_reset_string_cursor(pValue);` |
|        23 | 2181 | `		}` |
|        67 | 2182 | `		if( sUrl.bQuery ){` |
|        13 | 2183 | `			VmUrlSetComponent(pValue,&sUrl.sQuery);` |
|        13 | 2184 | `			ph7_array_add_strkey_elem(pArray,"query",pValue);` |
|        13 | 2185 | `			ph7_value_reset_string_cursor(pValue);` |
|         6 | 2186 | `		}` |
|        67 | 2187 | `		if( sUrl.bFragment ){` |
|        13 | 2188 | `			VmUrlSetComponent(pValue,&sUrl.sFragment);` |
|        13 | 2189 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue);` |
|         6 | 2190 | `		}` |
|         - | 2191 | `		/* Return the created array */` |
|        67 | 2192 | `		ph7_result_value(pCtx,pArray);` |
|         - | 2193 | `		/* NOTE:` |
|         - | 2194 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|         - | 2195 | `		 * automatically as soon we return from this function.` |
|         - | 2196 | `		 */` |
|         - | 2197 | `	}` |
|         - | 2198 | `	/* All done */` |
|        87 | 2199 | `	return PH7_OK;` |
|        53 | 2200 | `}` |
|         - | 2201 |  |
|         - | 2202 | `/*` |
|         - | 2203 | ` * Section:` |
|         - | 2204 | ` *   Array related routines.` |
|         - | 2205 | ` * Status:` |
|         - | 2206 | ` *    Stable.` |
|         - | 2207 | ` * Note 2012-5-21 01:04:15:` |
|         - | 2208 | ` *  Array related functions that need access to the underlying` |
|         - | 2209 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|         - | 2210 | ` */` |
|         - | 2211 | `/*` |
|         - | 2212 | ` * The [compact()] function store it's state information in an instance` |
|         - | 2213 | ` * of the following structure.` |
|         - | 2214 | ` */` |
|         - | 2215 | `struct compact_data` |
|         - | 2216 | `{` |
|         - | 2217 | `	ph7_value *pArray;  /* Target array */` |
|         - | 2218 | `	int nRecCount;      /* Recursion count */` |
|         - | 2219 | `	ph7_context *pCtx;  /* Call context, for php's two warnings */` |
|         - | 2220 | `	int iArg;           /* 1-based ARGUMENT this element came from: php names the` |
|         - | 2221 | `	                     * argument even for an element found inside a nested` |
|         - | 2222 | `	                     * array, never the element's own position. */` |
|         - | 2223 | `};` |
|         - | 2224 | `/*` |
|         - | 2225 | ` * php's two compact() diagnostics, both E_WARNING and both non-fatal (the entry` |
|         - | 2226 | ` * is skipped and the rest of the call proceeds): a name that is neither a string` |
|         - | 2227 | ` * nor an array of strings, and a string naming a variable the frame does not` |
|         - | 2228 | ` * have. PHL emitted neither, so compact(1) and compact('typo') answered a` |
|         - | 2229 | ` * shorter array with nothing said -- the caller's only clue that a name was` |
|         - | 2230 | ` * dropped was the array's own size.` |
|         - | 2231 | ` */` |
|        14 | 2232 | `static void VmCompactBadName(ph7_context *pCtx,int iArg,ph7_value *pValue)` |
|         1 | 2233 | `{` |
|         - | 2234 | `	char zGiven[64];` |
|        22 | 2235 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 2236 | `		"Argument #%d must be string or array of strings, %s given",` |
|         7 | 2237 | `		iArg,VmValueGivenName(pValue,zGiven,sizeof(zGiven)));` |
|        15 | 2238 | `}` |
|         8 | 2239 | `static void VmCompactUndefined(ph7_context *pCtx,SyString *pVar)` |
|         1 | 2240 | `{` |
|         8 | 2241 | `	if( pVar->nByte == sizeof("this")-1` |
|         6 | 2242 | `	 && SyMemcmp(pVar->zString,"this",sizeof("this")-1) == 0 ){` |
|         - | 2243 | ``		/* php's one silent miss here: `compact('this')` outside an object context`` |
|         - | 2244 | ``		 * skips the name and says nothing -- `$this` is not a variable, so it is`` |
|         - | 2245 | `		 * not an undefined one either, and compact() is the one door that neither` |
|         - | 2246 | `		 * warns nor throws for it. */` |
|         3 | 2247 | `		return;` |
|         - | 2248 | `	}` |
|        10 | 2249 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         6 | 2250 | `		"Undefined variable $%.*s",(int)pVar->nByte,pVar->zString);` |
|         5 | 2251 | `}` |
|         - | 2252 | `/*` |
|         - | 2253 | ` * Walker callback for the [compact()] function defined below.` |
|         - | 2254 | ` */` |
|        16 | 2255 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|         1 | 2256 | `{` |
|        17 | 2257 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|        17 | 2258 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|        17 | 2259 | `	ph7_vm *pVm = pArray->pVm;` |
|         - | 2260 | `	/* Act according to the hashmap value */` |
|        17 | 2261 | `	if( ph7_value_is_string(pValue) ){` |
|         - | 2262 | `		SyString sVar;` |
|         9 | 2263 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|         - | 2264 | `		/* Query the current frame. An EMPTY name is looked up like any other:` |
|         - | 2265 | `		 * php reports it as "Undefined variable $" rather than skipping it. */` |
|         9 | 2266 | `		pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|         - | 2267 | `		/* ^` |
|         - | 2268 | `		 * \| Avoid wasting variable and use 'pKey' instead` |
|         - | 2269 | `		 */` |
|         9 | 2270 | `		if( pKey ){` |
|         - | 2271 | `			/* Perform the insertion */` |
|         7 | 2272 | `			ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|         4 | 2273 | `		}else{` |
|         3 | 2274 | `			VmCompactUndefined(pData->pCtx,&sVar);` |
|         1 | 2275 | `		}` |
|        13 | 2276 | `	}else if( ph7_value_is_array(pValue) ){` |
|         - | 2277 | `		/* Recursively traverse this array. Past the depth cap the element is` |
|         - | 2278 | `		 * dropped in silence, as it always was: the cap is PHL's own guard and` |
|         - | 2279 | `		 * the "must be string or array of strings" warning would be a lie about` |
|         - | 2280 | `		 * an argument that IS an array of strings. */` |
|         7 | 2281 | `		if( pData->nRecCount < 32 ){` |
|         - | 2282 | `			int rc;` |
|         7 | 2283 | `			pData->nRecCount++;` |
|         7 | 2284 | `			rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|         7 | 2285 | `			pData->nRecCount--;` |
|         7 | 2286 | `			return rc;` |
|         - | 2287 | `		}` |
|       ! 0 | 2288 | `	}else{` |
|         3 | 2289 | `		VmCompactBadName(pData->pCtx,pData->iArg,pValue);` |
|         - | 2290 | `	}` |
|        11 | 2291 | `	return SXRET_OK;` |
|         9 | 2292 | `}` |
|         - | 2293 | `/*` |
|         - | 2294 | ` * array compact(mixed $varname [, mixed $... ])` |
|         - | 2295 | ` *  Create array containing variables and their values.` |
|         - | 2296 | ` *  For each of these, compact() looks for a variable with that name` |
|         - | 2297 | ` *  in the current symbol table and adds it to the output array such` |
|         - | 2298 | ` *  that the variable name becomes the key and the contents of the variable` |
|         - | 2299 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|         - | 2300 | ` *  Any strings that are not set will simply be skipped.` |
|         - | 2301 | ` * Parameters` |
|         - | 2302 | ` *  $varname` |
|         - | 2303 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|         - | 2304 | ` *   a string containing the name of the variable, or an array of variable names.` |
|         - | 2305 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|         - | 2306 | ` *   it recursively.` |
|         - | 2307 | ` * Return` |
|         - | 2308 | ` *  The output array with all the variables added to it or NULL on failure` |
|         - | 2309 | ` */` |
|        70 | 2310 | `PH7_PRIVATE int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 2311 | `{` |
|         - | 2312 | `	ph7_value *pArray,*pObj;` |
|        72 | 2313 | `	ph7_vm *pVm = pCtx->pVm;` |
|         - | 2314 | `	const char *zName;` |
|         - | 2315 | `	SyString sVar;` |
|         - | 2316 | `	int i,nLen;` |
|        72 | 2317 | `	if( nArg < 1 ){` |
|         - | 2318 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 2319 | `		ph7_result_null(pCtx);` |
|       ! 0 | 2320 | `		return PH7_OK;` |
|         - | 2321 | `	}` |
|         - | 2322 | `	/* php screens the arity first (above, and the arity table before it), then the` |
|         - | 2323 | `	 * call shape: the names are looked up in the CALLER's scope, which a dynamic` |
|         - | 2324 | `	 * call does not have. */` |
|         - | 2325 | `	{` |
|        72 | 2326 | `		sxi32 rc = PH7_VmForbidDynamicCall(pCtx);` |
|        72 | 2327 | `		if( rc != PH7_OK ){` |
|        31 | 2328 | `			return rc;` |
|         - | 2329 | `		}` |
|         - | 2330 | `	}` |
|         - | 2331 | `	/* Create the array */` |
|        42 | 2332 | `	pArray = ph7_context_new_array(pCtx);` |
|        42 | 2333 | `	if( pArray == 0 ){` |
|         - | 2334 | `		/* Out of memory */` |
|       ! 0 | 2335 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|         - | 2336 | `		/* Return NULL */` |
|       ! 0 | 2337 | `		ph7_result_null(pCtx);` |
|       ! 0 | 2338 | `		return PH7_OK;` |
|         - | 2339 | `	}` |
|         - | 2340 | `	/* Perform the requested operation */` |
|        94 | 2341 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        54 | 2342 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|        19 | 2343 | `			if( ph7_value_is_array(apArg[i]) ){` |
|         - | 2344 | `				struct compact_data sData;` |
|         7 | 2345 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 2346 | `				/* Recursively walk the array */` |
|         7 | 2347 | `				sData.nRecCount = 0;` |
|         7 | 2348 | `				sData.pArray = pArray;` |
|         7 | 2349 | `				sData.pCtx = pCtx;` |
|         7 | 2350 | `				sData.iArg = i + 1;` |
|         7 | 2351 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|         4 | 2352 | `			}else{` |
|        13 | 2353 | `				VmCompactBadName(pCtx,i + 1,apArg[i]);` |
|         - | 2354 | `			}` |
|        10 | 2355 | `		}else{` |
|         - | 2356 | `			/* Extract variable name. An EMPTY one is looked up like any other:` |
|         - | 2357 | `			 * php reports it as "Undefined variable $" rather than skipping it. */` |
|        36 | 2358 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        36 | 2359 | `			SyStringInitFromBuf(&sVar,zName,nLen);` |
|         - | 2360 | `			/* Check if the variable is available in the current frame */` |
|        36 | 2361 | `			pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        36 | 2362 | `			if( pObj ){` |
|        30 | 2363 | `				ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        16 | 2364 | `			}else{` |
|         7 | 2365 | `				VmCompactUndefined(pCtx,&sVar);` |
|         - | 2366 | `			}` |
|         - | 2367 | `		}` |
|        28 | 2368 | `	}` |
|         - | 2369 | `	/* Return the array */` |
|        42 | 2370 | `	ph7_result_value(pCtx,pArray);` |
|        42 | 2371 | `	return PH7_OK;` |
|        37 | 2372 | `}` |
|         - | 2373 | `/*` |
|         - | 2374 | ` * The [import_request_variables()] function store it's state information` |
|         - | 2375 | ` * in an instance of the following structure.` |
|         - | 2376 | ` */` |
|         - | 2377 | `typedef struct extract_aux_data extract_aux_data;` |
|         - | 2378 | `struct extract_aux_data` |
|         - | 2379 | `{` |
|         - | 2380 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|         - | 2381 | `	int iCount;           /* Number of variables successfully imported  */` |
|         - | 2382 | `	const char *zPrefix;  /* Prefix name */` |
|         - | 2383 | `	int Prefixlen;        /* Prefix  length */` |
|         - | 2384 | `	char zWorker[1024];   /* Working buffer */` |
|         - | 2385 | `};` |
|         - | 2386 | `/*` |
|         - | 2387 | ` * php's php_valid_var_name(): a legal PHP variable name matches` |
|         - | 2388 | ` * [A-Za-z_\x80-\xff][A-Za-z0-9_\x80-\xff]* . Byte-wise and locale-free on` |
|         - | 2389 | ` * purpose (php has been locale-independent here since 8.0); the high-byte` |
|         - | 2390 | ` * range is what lets UTF-8 identifiers through. extract() drops every key` |
|         - | 2391 | ` * that does not pass, instead of installing an unreachable variable.` |
|         - | 2392 | ` */` |
|       170 | 2393 | `static int VmIsValidVarName(const char *zName,sxu32 nByte)` |
|         4 | 2394 | `{` |
|         - | 2395 | `	unsigned char c;` |
|         - | 2396 | `	sxu32 i;` |
|       174 | 2397 | `	if( nByte < 1 ){` |
|         7 | 2398 | `		return FALSE;` |
|         - | 2399 | `	}` |
|       168 | 2400 | `	c = (unsigned char)zName[0];` |
|       168 | 2401 | `	if( c != '_' && !(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') && c < 0x80 ){` |
|        14 | 2402 | `		return FALSE;` |
|         - | 2403 | `	}` |
|       400 | 2404 | `	for( i = 1 ; i < nByte ; ++i ){` |
|       265 | 2405 | `		c = (unsigned char)zName[i];` |
|       262 | 2406 | `		if( c != '_' && !(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z')` |
|        80 | 2407 | `		 && !(c >= '0' && c <= '9') && c < 0x80 ){` |
|        20 | 2408 | `			return FALSE;` |
|         - | 2409 | `		}` |
|       125 | 2410 | `	}` |
|       138 | 2411 | `	return TRUE;` |
|        89 | 2412 | `}` |
|         - | 2413 | `/* TRUE when the name is exactly "this": php refuses to re-assign $this. */` |
|       176 | 2414 | `static int VmExtractIsThis(const char *zName,sxu32 nByte)` |
|         4 | 2415 | `{` |
|       180 | 2416 | `	return nByte == sizeof("this")-1 && SyMemcmp(zName,"this",sizeof("this")-1) == 0;` |
|         4 | 2417 | `}` |
|         - | 2418 | `/*` |
|         - | 2419 | ` * TRUE when the name belongs to the superglobal table ($GLOBALS, $_SERVER,` |
|         - | 2420 | ` * $_GET, …). php hands extract() a per-frame symbol table that holds no` |
|         - | 2421 | ` * superglobal, so such a key lands in the LOCAL table and the real superglobal` |
|         - | 2422 | ` * is untouched. In PHL the name resolves to the superglobal SLOT itself` |
|         - | 2423 | ` * (VmExtractMemObj consults hSuper first), so a plain store would replace` |
|         - | 2424 | ` * $GLOBALS/$_SERVER with the imported value and take the whole symbol table /` |
|         - | 2425 | ` * request environment with it. Those keys are dropped instead — a prefixed` |
|         - | 2426 | ` * name ($p__SERVER) is a normal local and stores fine.` |
|         - | 2427 | ` */` |
|       124 | 2428 | `static int VmExtractIsProtected(ph7_vm *pVm,const char *zName,sxu32 nByte)` |
|         4 | 2429 | `{` |
|       128 | 2430 | `	return PH7_VmSuperGet(&(*pVm),zName,nByte) != 0;` |
|         4 | 2431 | `}` |
|         - | 2432 | `/*` |
|         - | 2433 | ` * TRUE when the calling frame already holds this variable name.` |
|         - | 2434 | ` * "this" and the superglobals always answer FALSE, matching the symbol table` |
|         - | 2435 | ` * php hands extract(): $this is bound implicitly and superglobals live outside` |
|         - | 2436 | ` * the frame. So EXTR_IF_EXISTS/EXTR_PREFIX_IF_EXISTS skip those keys (php-exact)` |
|         - | 2437 | ` * and EXTR_PREFIX_SAME takes its not-a-collision branch.` |
|         - | 2438 | ` */` |
|        72 | 2439 | `static int VmExtractVarExists(ph7_vm *pVm,const char *zName,sxu32 nByte)` |
|         4 | 2440 | `{` |
|         - | 2441 | `	SyString sVar;` |
|        76 | 2442 | `	if( VmExtractIsThis(zName,nByte) \|\| VmExtractIsProtected(pVm,zName,nByte) ){` |
|        20 | 2443 | `		return FALSE;` |
|         - | 2444 | `	}` |
|        57 | 2445 | `	SyStringInitFromBuf(&sVar,zName,nByte);` |
|        57 | 2446 | `	return VmExtractMemObj(pVm,&sVar,FALSE,FALSE) != 0;` |
|        40 | 2447 | `}` |
|         - | 2448 | `/*` |
|         - | 2449 | ` * Create-or-overwrite a variable of the calling frame with a copy of pValue.` |
|         - | 2450 | ` * Returns TRUE when the variable was written (php counts exactly those).` |
|         - | 2451 | ` */` |
|         - | 2452 | `/*` |
|         - | 2453 | ` * EXTR_REFS: bind the imported NAME to the array element's own slot instead of copying` |
|         - | 2454 | ` * the value, so a later write through the variable reaches the caller's array — php's` |
|         - | 2455 | ` * by-reference extraction. The binding is registered (PH7_VmBindVarSlot), which is what` |
|         - | 2456 | ` * makes the element count the new name as a holder and read as a reference.` |
|         - | 2457 | ` *` |
|         - | 2458 | ` * The name is DUPLICATED: the symbol table keeps the pointer it is given, and the caller's` |
|         - | 2459 | ` * is a scratch blob the next entry reuses.` |
|         - | 2460 | ` */` |
|         8 | 2461 | `static int VmExtractBindVar(ph7_vm *pVm,const char *zName,sxu32 nByte,sxu32 nIdx)` |
|         1 | 2462 | `{` |
|         9 | 2463 | `	VmFrame *pFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|         - | 2464 | `	char *zDup;` |
|         9 | 2465 | `	if( nIdx == SXU32_HIGH ){` |
|       ! 0 | 2466 | `		return FALSE;` |
|         - | 2467 | `	}` |
|         9 | 2468 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zName,nByte);` |
|         9 | 2469 | `	if( zDup == 0 ){` |
|       ! 0 | 2470 | `		return FALSE;` |
|         - | 2471 | `	}` |
|         9 | 2472 | `	PH7_VmBindVarSlot(&(*pVm),pFrame,zDup,nByte,nIdx);` |
|         9 | 2473 | `	return TRUE;` |
|         5 | 2474 | `}` |
|        72 | 2475 | `static int VmExtractStoreVar(ph7_vm *pVm,const char *zName,sxu32 nByte,ph7_value *pValue)` |
|         3 | 2476 | `{` |
|         - | 2477 | `	ph7_value *pObj;` |
|         - | 2478 | `	SyString sVar;` |
|        75 | 2479 | `	SyStringInitFromBuf(&sVar,zName,nByte);` |
|         - | 2480 | `	/* bDup: the name lives in a scratch blob that is reused by the next entry */` |
|        75 | 2481 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        75 | 2482 | `	if( pObj == 0 ){` |
|       ! 0 | 2483 | `		return FALSE;` |
|         - | 2484 | `	}` |
|        75 | 2485 | `	PH7_MemObjStore(pValue,pObj);` |
|        75 | 2486 | `	return TRUE;` |
|        39 | 2487 | `}` |
|         - | 2488 | `/*` |
|         - | 2489 | ` * Build php's prefixed name "<prefix>_<key>" (php_prefix_varname with` |
|         - | 2490 | ` * add_underscore): the separator is unconditional, so an empty prefix still` |
|         - | 2491 | ` * yields "_key" exactly like php.` |
|         - | 2492 | ` */` |
|        40 | 2493 | `static sxi32 VmExtractPrefixName(SyBlob *pOut,const char *zPrefix,int nPrefix,` |
|         - | 2494 | `	const char *zKey,sxu32 nKey)` |
|         3 | 2495 | `{` |
|        43 | 2496 | `	SyBlobReset(pOut);` |
|        43 | 2497 | `	if( nPrefix > 0 && SyBlobAppend(pOut,zPrefix,(sxu32)nPrefix) != SXRET_OK ){` |
|       ! 0 | 2498 | `		return SXERR_MEM;` |
|         - | 2499 | `	}` |
|        43 | 2500 | `	if( SyBlobAppend(pOut,"_",sizeof(char)) != SXRET_OK ){` |
|       ! 0 | 2501 | `		return SXERR_MEM;` |
|         - | 2502 | `	}` |
|        43 | 2503 | `	if( nKey > 0 && SyBlobAppend(pOut,zKey,nKey) != SXRET_OK ){` |
|       ! 0 | 2504 | `		return SXERR_MEM;` |
|         - | 2505 | `	}` |
|        43 | 2506 | `	return SXRET_OK;` |
|        23 | 2507 | `}` |
|         - | 2508 | `/* What to do with one array entry, decided by the extract mode. */` |
|         - | 2509 | `#define VM_EXTRACT_DROP     0 /* php skips this key entirely */` |
|         - | 2510 | `#define VM_EXTRACT_PLAIN    1 /* install under the key itself */` |
|         - | 2511 | `#define VM_EXTRACT_PREFIX   2 /* install under "<prefix>_<key>" */` |
|         - | 2512 | `/*` |
|         - | 2513 | ` * int extract(array &$array[,int $flags = EXTR_OVERWRITE[,string $prefix = "" ]])` |
|         - | 2514 | ` *   Import variables into the current symbol table from an array.` |
|         - | 2515 | ` *` |
|         - | 2516 | ` * $flags is php's ENUM (PH7_EXTR_* in ph7int.h), not a bitmask: the mode is` |
|         - | 2517 | ` * (flags & 0xff) and anything above EXTR_IF_EXISTS(6) is a ValueError. The` |
|         - | 2518 | ` * modes, mirroring php's per-mode helpers in ext/standard/array.c:` |
|         - | 2519 | ` *   EXTR_OVERWRITE(0)        collisions overwrite` |
|         - | 2520 | ` *   EXTR_SKIP(1)             collisions keep the existing variable` |
|         - | 2521 | ` *   EXTR_PREFIX_SAME(2)      collisions install "<prefix>_<key>"` |
|         - | 2522 | ` *   EXTR_PREFIX_ALL(3)       every key installs as "<prefix>_<key>"` |
|         - | 2523 | ` *   EXTR_PREFIX_INVALID(4)   only illegal names (and numeric keys) get prefixed` |
|         - | 2524 | ` *   EXTR_PREFIX_IF_EXISTS(5) install "<prefix>_<key>" only if <key> exists` |
|         - | 2525 | ` *   EXTR_IF_EXISTS(6)        overwrite only variables that already exist` |
|         - | 2526 | `` * Modes 2..5 require $prefix (php: `is required when using this extract type`),`` |
|         - | 2527 | ` * a non-empty $prefix must itself be a legal identifier, and a key whose final` |
|         - | 2528 | ` * name is not a legal variable name is dropped rather than installed. Only` |
|         - | 2529 | ` * EXTR_PREFIX_ALL/EXTR_PREFIX_INVALID look at numeric keys at all.` |
|         - | 2530 | ` *` |
|         - | 2531 | `` * $this is never a target: php throws `Cannot re-assign $this` where a store`` |
|         - | 2532 | ` * would land on it, and skips it where a store would not (EXTR_SKIP), and` |
|         - | 2533 | ` * $GLOBALS is never clobbered.` |
|         - | 2534 | ` * Return` |
|         - | 2535 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|         - | 2536 | ` */` |
|       120 | 2537 | `PH7_PRIVATE int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 2538 | `{` |
|       124 | 2539 | `	ph7_vm *pVm = pCtx->pVm;` |
|         - | 2540 | `	ph7_hashmap_node *pEntry;` |
|         - | 2541 | `	ph7_hashmap *pMap;` |
|       124 | 2542 | `	const char *zPrefix = 0;` |
|       124 | 2543 | `	sxi64 iFlags = PH7_EXTR_OVERWRITE;` |
|       124 | 2544 | `	sxi64 iCount = 0;` |
|         - | 2545 | `	ph7_value sValue;` |
|         - | 2546 | `	SyBlob sWorker;` |
|       124 | 2547 | `	int nPrefix = 0;` |
|       124 | 2548 | `	sxi32 rc = PH7_OK;` |
|         - | 2549 | `	int iType;` |
|         - | 2550 | `	sxu32 n;` |
|       124 | 2551 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 2552 | `		char zBuf[64];` |
|       ! 0 | 2553 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 2554 | `			"extract(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 2555 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|         - | 2556 | `	}` |
|       124 | 2557 | `	if( nArg > 1 ){` |
|       104 | 2558 | `		rc = PH7_IntArgResolve(pCtx,apArg[1],"extract",2,"$flags","int",&iFlags);` |
|       104 | 2559 | `		if( rc != PH7_OK ){` |
|       ! 0 | 2560 | `			return rc;` |
|         - | 2561 | `		}` |
|        50 | 2562 | `	}` |
|         - | 2563 | `	/* php: the mode is the low byte; EXTR_REFS(0x100) rides above it */` |
|       124 | 2564 | `	iType = (int)(iFlags & 0xff);` |
|       124 | 2565 | `	if( iType > PH7_EXTR_IF_EXISTS ){` |
|        10 | 2566 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 2567 | `			"extract(): Argument #2 ($flags) must be a valid extract type");` |
|         - | 2568 | `	}` |
|       116 | 2569 | `	if( iType > PH7_EXTR_SKIP && iType <= PH7_EXTR_PREFIX_IF_EXISTS && nArg < 3 ){` |
|        15 | 2570 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 2571 | `			"extract(): Argument #3 ($prefix) is required when using this extract type");` |
|         - | 2572 | `	}` |
|       104 | 2573 | `	if( nArg > 2 ){` |
|        43 | 2574 | `		zPrefix = ph7_value_to_string(apArg[2],&nPrefix);` |
|        43 | 2575 | `		if( nPrefix > 0 && !VmIsValidVarName(zPrefix,(sxu32)nPrefix) ){` |
|         8 | 2576 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 2577 | `				"extract(): Argument #3 ($prefix) must be a valid identifier");` |
|         - | 2578 | `		}` |
|        17 | 2579 | `	}` |
|         - | 2580 | `	/* Every argument screen above runs first in php too; the call shape is the last` |
|         - | 2581 | `	 * refusal before the walk, because the walk writes into the CALLER's scope. */` |
|        98 | 2582 | `	if( (rc = PH7_VmForbidDynamicCall(pCtx)) != PH7_OK ){` |
|         7 | 2583 | `		return rc;` |
|         - | 2584 | `	}` |
|         - | 2585 | `	/* Point to the target hashmap */` |
|        92 | 2586 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        92 | 2587 | `	if( pMap->nEntry < 1 ){` |
|         - | 2588 | `		/* Empty map,return  0 */` |
|       ! 0 | 2589 | `		ph7_result_int(pCtx,0);` |
|       ! 0 | 2590 | `		return PH7_OK;` |
|         - | 2591 | `	}` |
|        92 | 2592 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        92 | 2593 | `	PH7_MemObjInit(pVm,&sValue);` |
|         - | 2594 | `	/* php walks a COPY of the array, so an entry that overwrites the caller's own` |
|         - | 2595 | `	 * array variable ($arr = ['arr'=>1]; extract($arr)) cannot pull the map from` |
|         - | 2596 | `	 * under the walk. Pinning the map for the walk is the same guarantee. */` |
|        92 | 2597 | `	pMap->iRef++;` |
|        92 | 2598 | `	pEntry = pMap->pFirst;` |
|         - | 2599 | `	/* pFirst walks the insertion order through pPrev — PH7 links the entry list` |
|         - | 2600 | `	 * in reverse (same traversal PH7_HashmapWalk uses). */` |
|       264 | 2601 | `	for( n = pMap->nEntry ; n > 0 && pEntry ; --n, pEntry = pEntry->pPrev ){` |
|         - | 2602 | `		const char *zKey, *zFinal;` |
|         - | 2603 | `		sxu32 nKey, nFinal;` |
|         - | 2604 | `		char zNum[32];` |
|         - | 2605 | `		int bIntKey, iAction;` |
|         - | 2606 | `		/* Work off a COPY of the entry value. Installing a variable used to grow` |
|         - | 2607 | `		 * pVm->aMemObj and dangle a pointer into it (this is why the walk API` |
|         - | 2608 | `		 * hands out copies too); P1's fixed segments retired that, but the copy` |
|         - | 2609 | `		 * still earns its place for the reference discipline below. The` |
|         - | 2610 | `		 * release comes FIRST so it covers every continue/goto below: the load` |
|         - | 2611 | `		 * takes a reference on an array/object value and does not drop the one` |
|         - | 2612 | `		 * the previous entry left behind (PH7_HashmapWalk releases per iteration` |
|         - | 2613 | `		 * for the same reason — without it a whole-array extract() pins every` |
|         - | 2614 | `		 * value it copied, and their destructors never run). */` |
|       178 | 2615 | `		PH7_MemObjRelease(&sValue);` |
|       178 | 2616 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|       178 | 2617 | `		bIntKey = (pEntry->iType == HASHMAP_INT_NODE);` |
|       178 | 2618 | `		if( bIntKey ){` |
|         - | 2619 | `			/* Only the two prefixing modes below ever look at a numeric key */` |
|        22 | 2620 | `			nKey = SyBufferFormat(zNum,sizeof(zNum),"%qd",pEntry->xKey.iKey);` |
|        22 | 2621 | `			zKey = zNum;` |
|        12 | 2622 | `		}else{` |
|       158 | 2623 | `			zKey = (const char *)SyBlobData(&pEntry->xKey.sKey);` |
|       158 | 2624 | `			nKey = SyBlobLength(&pEntry->xKey.sKey);` |
|         - | 2625 | `		}` |
|       178 | 2626 | `		iAction = VM_EXTRACT_DROP;` |
|       178 | 2627 | `		switch( iType ){` |
|        22 | 2628 | `		case PH7_EXTR_OVERWRITE:` |
|        48 | 2629 | `			if( bIntKey \|\| !VmIsValidVarName(zKey,nKey) ){` |
|         8 | 2630 | `				break;` |
|         - | 2631 | `			}` |
|        36 | 2632 | `			if( VmExtractIsThis(zKey,nKey) ){` |
|         3 | 2633 | `				goto this_error;` |
|         - | 2634 | `			}` |
|        34 | 2635 | `			iAction = VM_EXTRACT_PLAIN;` |
|        34 | 2636 | `			break;` |
|        12 | 2637 | `		case PH7_EXTR_SKIP:` |
|        27 | 2638 | `			if( bIntKey \|\| !VmIsValidVarName(zKey,nKey) \|\| VmExtractIsThis(zKey,nKey) ){` |
|         6 | 2639 | `				break;` |
|         - | 2640 | `			}` |
|        17 | 2641 | `			if( VmExtractVarExists(pVm,zKey,nKey) ){` |
|         8 | 2642 | `				break; /* collision: keep the existing variable */` |
|         - | 2643 | `			}` |
|        10 | 2644 | `			iAction = VM_EXTRACT_PLAIN;` |
|        10 | 2645 | `			break;` |
|        16 | 2646 | `		case PH7_EXTR_IF_EXISTS:` |
|        36 | 2647 | `			if( bIntKey \|\| !VmExtractVarExists(pVm,zKey,nKey) ){` |
|        14 | 2648 | `				break;` |
|         - | 2649 | `			}` |
|        12 | 2650 | `			if( !VmIsValidVarName(zKey,nKey) ){` |
|       ! 0 | 2651 | `				break;` |
|         - | 2652 | `			}` |
|        12 | 2653 | `			if( VmExtractIsThis(zKey,nKey) ){` |
|       ! 0 | 2654 | `				goto this_error;` |
|         - | 2655 | `			}` |
|        12 | 2656 | `			iAction = VM_EXTRACT_PLAIN;` |
|        12 | 2657 | `			break;` |
|         9 | 2658 | `		case PH7_EXTR_PREFIX_SAME:` |
|        21 | 2659 | `			if( bIntKey \|\| nKey < 1 ){` |
|         3 | 2660 | `				break;` |
|         - | 2661 | `			}` |
|        17 | 2662 | `			if( VmExtractVarExists(pVm,zKey,nKey) ){` |
|         6 | 2663 | `				iAction = VM_EXTRACT_PREFIX; /* collision */` |
|        14 | 2664 | `			}else if( !VmIsValidVarName(zKey,nKey) ){` |
|         5 | 2665 | `				break;` |
|       ! 0 | 2666 | `			}else{` |
|         - | 2667 | `				/* $this cannot be a target, but its prefixed form can */` |
|         8 | 2668 | `				iAction = VmExtractIsThis(zKey,nKey) ? VM_EXTRACT_PREFIX : VM_EXTRACT_PLAIN;` |
|         - | 2669 | `			}` |
|        13 | 2670 | `			break;` |
|        13 | 2671 | `		case PH7_EXTR_PREFIX_ALL:` |
|        28 | 2672 | `			if( !bIntKey && nKey < 1 ){` |
|         3 | 2673 | `				break;` |
|         - | 2674 | `			}` |
|        26 | 2675 | `			iAction = VM_EXTRACT_PREFIX;` |
|        26 | 2676 | `			break;` |
|         7 | 2677 | `		case PH7_EXTR_PREFIX_INVALID:` |
|        15 | 2678 | `			iAction = (bIntKey \|\| !VmIsValidVarName(zKey,nKey) \|\| VmExtractIsThis(zKey,nKey))` |
|        13 | 2679 | `				? VM_EXTRACT_PREFIX : VM_EXTRACT_PLAIN;` |
|        16 | 2680 | `			break;` |
|         8 | 2681 | `		case PH7_EXTR_PREFIX_IF_EXISTS:` |
|        18 | 2682 | `			if( !bIntKey && VmExtractVarExists(pVm,zKey,nKey) ){` |
|         3 | 2683 | `				iAction = VM_EXTRACT_PREFIX;` |
|         1 | 2684 | `			}` |
|        16 | 2685 | `			break;` |
|       ! 0 | 2686 | `		default:` |
|       ! 0 | 2687 | `			break;` |
|         - | 2688 | `		}` |
|       176 | 2689 | `		if( iAction == VM_EXTRACT_DROP ){` |
|       127 | 2690 | `			continue;` |
|         - | 2691 | `		}` |
|       102 | 2692 | `		if( iAction == VM_EXTRACT_PREFIX ){` |
|        43 | 2693 | `			if( VmExtractPrefixName(&sWorker,zPrefix,nPrefix,zKey,nKey) != SXRET_OK ){` |
|       ! 0 | 2694 | `				rc = PH7_VmThrowException(pCtx,"Error","PH7 engine is running out of memory");` |
|       ! 0 | 2695 | `				goto done;` |
|         - | 2696 | `			}` |
|        43 | 2697 | `			zFinal = (const char *)SyBlobData(&sWorker);` |
|        43 | 2698 | `			nFinal = SyBlobLength(&sWorker);` |
|        43 | 2699 | `			if( !VmIsValidVarName(zFinal,nFinal) ){` |
|         7 | 2700 | `				continue; /* php drops a prefixed name that is not an identifier */` |
|         - | 2701 | `			}` |
|        37 | 2702 | `			if( VmExtractIsThis(zFinal,nFinal) ){` |
|       ! 0 | 2703 | `				goto this_error;` |
|         - | 2704 | `			}` |
|        20 | 2705 | `		}else{` |
|        62 | 2706 | `			if( VmExtractIsProtected(pVm,zKey,nKey) ){` |
|        13 | 2707 | `				continue; /* $GLOBALS/$_SERVER/... are never a plain target */` |
|         - | 2708 | `			}` |
|        50 | 2709 | `			zFinal = zKey;` |
|        50 | 2710 | `			nFinal = nKey;` |
|         - | 2711 | `		}` |
|        84 | 2712 | `		if( iFlags & PH7_EXTR_REFS ){` |
|         - | 2713 | `			/* Bind to the element's own slot (php's EXTR_REFS) rather than copying */` |
|         9 | 2714 | `			if( VmExtractBindVar(pVm,zFinal,nFinal,pEntry->nValIdx) ){` |
|         9 | 2715 | `				iCount++;` |
|         5 | 2716 | `			}` |
|        79 | 2717 | `		}else if( VmExtractStoreVar(pVm,zFinal,nFinal,&sValue) ){` |
|        75 | 2718 | `			iCount++;` |
|        36 | 2719 | `		}` |
|        84 | 2720 | `		continue;` |
|         1 | 2721 | `this_error:` |
|         3 | 2722 | `		rc = PH7_VmThrowException(pCtx,"Error","Cannot re-assign $this");` |
|         3 | 2723 | `		goto done;` |
|       ! 0 | 2724 | `	}` |
|         - | 2725 | `	/* Number of variables successfully imported */` |
|        90 | 2726 | `	ph7_result_int64(pCtx,iCount);` |
|        44 | 2727 | `done:` |
|        92 | 2728 | `	PH7_MemObjRelease(&sValue);` |
|        92 | 2729 | `	SyBlobRelease(&sWorker);` |
|        92 | 2730 | `	PH7_HashmapUnref(pMap);` |
|        92 | 2731 | `	return rc;` |
|        64 | 2732 | `}` |
|         - | 2733 | `/*` |
|         - | 2734 | ` * Worker callback for the [import_request_variables()] function` |
|         - | 2735 | ` * defined below.` |
|         - | 2736 | ` */` |
|         2 | 2737 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|         1 | 2738 | `{` |
|         3 | 2739 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|         3 | 2740 | `	ph7_vm *pVm = pAux->pVm;` |
|         - | 2741 | `	ph7_value *pObj;` |
|         - | 2742 | `	SyString sVar;` |
|         - | 2743 | `	/* Perform a string cast */` |
|         3 | 2744 | `	PH7_MemObjToString(pKey);` |
|         3 | 2745 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|         - | 2746 | `		/* Unavailable variable name */` |
|       ! 0 | 2747 | `		return SXRET_OK;` |
|         - | 2748 | `	}` |
|         3 | 2749 | `	sVar.nByte = 0; /* cc warning */` |
|         3 | 2750 | `	if( pAux->Prefixlen > 0 ){` |
|         4 | 2751 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|         1 | 2752 | `			pAux->Prefixlen,pAux->zPrefix,` |
|         1 | 2753 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|         - | 2754 | `			);` |
|         2 | 2755 | `	}else{` |
|       ! 0 | 2756 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|       ! 0 | 2757 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|         - | 2758 | `	}` |
|         3 | 2759 | `	sVar.zString = pAux->zWorker;` |
|         - | 2760 | `	/* Extract the variable */` |
|         3 | 2761 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|         3 | 2762 | `	if( pObj ){` |
|         3 | 2763 | `		PH7_MemObjStore(pValue,pObj);` |
|         1 | 2764 | `	}` |
|         3 | 2765 | `	return SXRET_OK;` |
|         2 | 2766 | `}` |
|         - | 2767 | `/*` |
|         - | 2768 | ` * bool import_request_variables(string $types[,string $prefix])` |
|         - | 2769 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|         - | 2770 | ` * Parameters` |
|         - | 2771 | ` * $types` |
|         - | 2772 | ` *  Using the types parameter, you can specify which request variables to import.` |
|         - | 2773 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|         - | 2774 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|         - | 2775 | ` *  POST includes the POST uploaded file information.` |
|         - | 2776 | ` *  Note:` |
|         - | 2777 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|         - | 2778 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|         - | 2779 | ` * $prefix` |
|         - | 2780 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|         - | 2781 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|         - | 2782 | ` *  variable named $pref_userid.` |
|         - | 2783 | ` * Return` |
|         - | 2784 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2785 | ` */` |
|         2 | 2786 | `PH7_PRIVATE int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2787 | `{` |
|         - | 2788 | `	const char *zPrefix,*zEnd,*zImport;` |
|         - | 2789 | `	extract_aux_data sAux;` |
|         - | 2790 | `	int nLen,nPrefixLen;` |
|         - | 2791 | `	ph7_value *pSuper;` |
|         - | 2792 | `	ph7_vm *pVm;` |
|         - | 2793 | `	/* By default import only $_GET variables  */` |
|         3 | 2794 | `	zImport = "G";` |
|         3 | 2795 | `	nLen = (int)sizeof(char);` |
|         3 | 2796 | `	zPrefix = 0;` |
|         3 | 2797 | `	nPrefixLen = 0;` |
|         3 | 2798 | `	if( nArg > 0 ){` |
|         3 | 2799 | `		if( ph7_value_is_string(apArg[0]) ){` |
|         3 | 2800 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|         1 | 2801 | `		}` |
|         3 | 2802 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|         3 | 2803 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|         1 | 2804 | `		}` |
|         1 | 2805 | `	}` |
|         - | 2806 | `	/* Point to the underlying VM */` |
|         3 | 2807 | `	pVm = pCtx->pVm;` |
|         - | 2808 | `	/* Initialize the aux data */` |
|         3 | 2809 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|         3 | 2810 | `	sAux.zPrefix = zPrefix;` |
|         3 | 2811 | `	sAux.Prefixlen = nPrefixLen;` |
|         3 | 2812 | `	sAux.pVm = pVm;` |
|         - | 2813 | `	/* Extract */` |
|         3 | 2814 | `	zEnd = &zImport[nLen];` |
|         5 | 2815 | `	while( zImport < zEnd ){` |
|         3 | 2816 | `		int c = zImport[0];` |
|         3 | 2817 | `		pSuper = 0;` |
|         3 | 2818 | `		if( c == 'G' \|\| c == 'g' ){` |
|         - | 2819 | `			/* Import $_GET variables */` |
|         3 | 2820 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|         1 | 2821 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|         - | 2822 | `			/* Import $_POST variables */` |
|       ! 0 | 2823 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|       ! 0 | 2824 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|         - | 2825 | `			/* Import $_COOKIE variables */` |
|       ! 0 | 2826 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|       ! 0 | 2827 | `		}` |
|         3 | 2828 | `		if( pSuper ){` |
|         - | 2829 | `			/* Iterate throw array entries */` |
|         3 | 2830 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|         1 | 2831 | `		}` |
|         - | 2832 | `		/* Advance the cursor */` |
|         3 | 2833 | `		zImport++;` |
|         1 | 2834 | `	}` |
|         - | 2835 | `	/* All done,return TRUE*/` |
|         3 | 2836 | `	ph7_result_bool(pCtx,0);` |
|         3 | 2837 | `	return PH7_OK;` |
|         1 | 2838 | `}` |
|         - | 2839 |  |
