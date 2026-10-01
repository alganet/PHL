# src/ph7/vm_builtin_lang.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1229/1432 lines (85.82%)

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
|      3440 |   43 | `static int VmClassConstLookup(` |
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
|      3445 |   55 | `	*ppClass = 0;` |
|      3445 |   56 | `	*ppAttr = 0;` |
|     65925 |   57 | `	for( iSep = 0; iSep + 1 < nLen; iSep++ ){` |
|     62673 |   58 | `		if( zName[iSep] == ':' && zName[iSep+1] == ':' ){` |
|       191 |   59 | `			break;` |
|         - |   60 | `		}` |
|     31245 |   61 | `	}` |
|      3445 |   62 | `	if( iSep + 1 >= nLen ){` |
|      3257 |   63 | `		return VM_CCONST_PLAIN;` |
|         - |   64 | `	}` |
|       191 |   65 | `	*pSep = iSep;` |
|       191 |   66 | `	pClass = iSep > 0 ? PH7_VmResolveScopeName(&(*pVm),zName,(sxu32)iSep) : 0;` |
|       191 |   67 | `	if( pClass == 0 ){` |
|        31 |   68 | `		if( iSep > 0 && PH7_VmIsScopeKeyword(zName,(sxu32)iSep) ){` |
|         - |   69 | `			/* php separates the two ways a keyword can fail to resolve, so tell them` |
|         - |   70 | ``			 * apart here: `parent` inside a class that simply has no parent is a`` |
|         - |   71 | `			 * different sentence from a keyword named with no class scope at all. */` |
|        16 |   72 | `			if( iSep == 6 && SyMemcmp(zName,"parent",6) == 0` |
|        10 |   73 | `			 && (PH7_VmPeekTopClass(&(*pVm)) \|\| PH7_VmPeekDeclaringClass(&(*pVm))) ){` |
|         3 |   74 | `				return VM_CCONST_NOPARENT;` |
|         - |   75 | `			}` |
|        16 |   76 | `			return VM_CCONST_NOSCOPE;` |
|         - |   77 | `		}` |
|        14 |   78 | `		return VM_CCONST_NOCLASS;` |
|         - |   79 | `	}` |
|       162 |   80 | `	*ppClass = pClass;` |
|       162 |   81 | `	if( iSep + 2 >= nLen ){` |
|         5 |   82 | `		return VM_CCONST_NOCONST; /* "C::" names no constant */` |
|         - |   83 | `	}` |
|         - |   84 | `	/* This form names a class CONSTANT or an enum case (hConst), never a property. */` |
|       158 |   85 | `	pAttr = PH7_ClassExtractConstant(pClass,&zName[iSep+2],(sxu32)(nLen - iSep - 2));` |
|       158 |   86 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        28 |   87 | `		return VM_CCONST_NOCONST;` |
|         - |   88 | `	}` |
|       132 |   89 | `	if( (pClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|         - |   90 | `		/* A trait constant belongs to the classes that COMPOSE the trait: php refuses` |
|         - |   91 | ``		 * `constant("T::K")` outright, and answers `defined("T::K")` with false. */`` |
|         3 |   92 | `		*ppAttr = pAttr;` |
|         3 |   93 | `		return VM_CCONST_TRAIT;` |
|         - |   94 | `	}` |
|       128 |   95 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|        77 |   96 | `	 && pAttr->pDeclClass && pAttr->pDeclClass != pClass ){` |
|         - |   97 | `		/* php does not put a private constant in a SUBCLASS's table at all, so naming it` |
|         - |   98 | ``		 * through the child is not an access denial but a plain miss — `Sub::P` reports`` |
|         - |   99 | ``		 * `Undefined constant Sub::P` even from inside the declaring class, and`` |
|         - |  100 | `		 * defined("Sub::P") is false there too. Only the declaring class can answer. */` |
|       ! 0 |  101 | `		return VM_CCONST_NOCONST;` |
|         - |  102 | `	}` |
|       130 |  103 | `	*ppAttr = pAttr;` |
|         - |  104 | ``	/* php answers by the CALLING scope, the same rule the direct `C::K` access uses:`` |
|         - |  105 | `	 * a private constant is invisible from outside its declaring class even to a` |
|         - |  106 | `	 * subclass, and a protected one is visible down the hierarchy. */` |
|       130 |  107 | `	if( !PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        22 |  108 | `		return VM_CCONST_NOACCESS;` |
|         - |  109 | `	}` |
|       110 |  110 | `	return VM_CCONST_OK;` |
|      1725 |  111 | `}` |
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
|      1640 |  163 | `PH7_PRIVATE int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 |  164 | `{` |
|         - |  165 | `	ph7_class_attr *pAttr;` |
|         - |  166 | `	ph7_class *pClass;` |
|         - |  167 | `	const char *zName;` |
|      1645 |  168 | `	int nLen = 0;` |
|      1645 |  169 | `	int iSep = 0;` |
|      1645 |  170 | `	int res = 0;` |
|      1645 |  171 | `	if( nArg < 1 ){` |
|         - |  172 | `		/* Missing constant name,return FALSE */` |
|       ! 0 |  173 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|       ! 0 |  174 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 |  175 | `		return SXRET_OK;` |
|         - |  176 | `	}` |
|         - |  177 | `	/* Extract constant name */` |
|      1645 |  178 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|         - |  179 | `	/* Class-constant form "C::K": every miss — unknown class, unknown constant, or one` |
|         - |  180 | `	 * that is not visible from here — is a plain FALSE, since asking whether a name is` |
|         - |  181 | `	 * defined is exactly what defined() is for (this used to consult the` |
|         - |  182 | `	 * global constant table only, so EVERY class constant answered false while` |
|         - |  183 | `	 * constant() read the same name correctly). */` |
|      1645 |  184 | `	if( nLen > 0 ){` |
|      1645 |  185 | `		int iRc = VmClassConstLookup(pCtx->pVm,zName,nLen,&pClass,&pAttr,&iSep);` |
|      1645 |  186 | `		switch( iRc ){` |
|       781 |  187 | `			case VM_CCONST_PLAIN:` |
|      1567 |  188 | `				break;` |
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
|        34 |  199 | `				ph7_result_bool(pCtx,0);` |
|        34 |  200 | `				return SXRET_OK;` |
|         - |  201 | `		}` |
|       781 |  202 | `	}` |
|         - |  203 | `	/* Perform the lookup */` |
|      1567 |  204 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|         - |  205 | `		/* Already defined */` |
|      1551 |  206 | `		res = 1;` |
|       772 |  207 | `	}` |
|      1567 |  208 | `	ph7_result_bool(pCtx,res);` |
|      1567 |  209 | `	return SXRET_OK;` |
|       825 |  210 | `}` |
|         - |  211 | `/*` |
|         - |  212 | ` * Constant expansion callback used by the [define()] function defined` |
|         - |  213 | ` * below.` |
|         - |  214 | ` */` |
|    102214 |  215 | `PH7_PRIVATE void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|         5 |  216 | `{` |
|    102219 |  217 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|         - |  218 | `	/* Expand constant value */` |
|    102219 |  219 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|    102219 |  220 | `}` |
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
|       156 |  232 | `PH7_PRIVATE int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 |  233 | `{` |
|         - |  234 | `	const char *zName;  /* Constant name */` |
|         - |  235 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       160 |  236 | `	int nLen = 0;       /* Name length */` |
|         - |  237 | `	sxi32 rc;` |
|       160 |  238 | `	if( nArg < 2 ){` |
|         - |  239 | `		/* Missing arguments,throw a ntoice and return false */` |
|       ! 0 |  240 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|       ! 0 |  241 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 |  242 | `		return SXRET_OK;` |
|         - |  243 | `	}` |
|       160 |  244 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|       ! 0 |  245 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|       ! 0 |  246 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 |  247 | `		return SXRET_OK;` |
|         - |  248 | `	}` |
|         - |  249 | `	/* Extract constant name */` |
|       160 |  250 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       160 |  251 | `	if( nLen < 1 ){` |
|       ! 0 |  252 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|       ! 0 |  253 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 |  254 | `		return SXRET_OK;` |
|         - |  255 | `	}` |
|         - |  256 | `	/* Duplicate constant value */` |
|       160 |  257 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       160 |  258 | `	if( pValue == 0 ){` |
|       ! 0 |  259 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|       ! 0 |  260 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 |  261 | `		return SXRET_OK;` |
|         - |  262 | `	}` |
|         - |  263 | `	/* Initialize the memory object */` |
|       160 |  264 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|         - |  265 | `	/* Register the constant */` |
|         - |  266 | `	{` |
|         - |  267 | `		SyString sConsName;` |
|       160 |  268 | `		SyStringInitFromBuf(&sConsName,zName,(sxu32)nLen);` |
|       238 |  269 | `		rc = PH7_VmRegisterConstantEx(pCtx->pVm,&sConsName,VmExpandUserConstant,pValue,` |
|       156 |  270 | `			(SyString *)SySetPeek(&pCtx->pVm->aFiles),0,1);` |
|         - |  271 | `	}` |
|       160 |  272 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  273 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|       ! 0 |  274 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|       ! 0 |  275 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 |  276 | `		return SXRET_OK;` |
|         - |  277 | `	}` |
|         - |  278 | `	/* Duplicate constant value */` |
|       160 |  279 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       160 |  280 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
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
|       160 |  311 | `	ph7_result_bool(pCtx,1);` |
|       160 |  312 | `	return SXRET_OK;` |
|        82 |  313 | `}` |
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
|         - |  374 | ` * DEPRECATIONS, so PH7_IntArgResolve refuses them (§10 scope policy) with the` |
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
|         4 |  465 | `			"%s is not a valid backing value for enum %z",zVal,&pClass->sName);` |
|         5 |  466 | `	}else{` |
|        49 |  467 | `		rc = PH7_VmThrowException(pCtx,"ValueError",` |
|         - |  468 | `			"\"%.*s\" is not a valid backing value for enum %z",` |
|        32 |  469 | `			(int)SyBlobLength(&sNeedle.sBlob),(const char *)SyBlobData(&sNeedle.sBlob),` |
|        16 |  470 | `			&pClass->sName);` |
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
|      1800 |  521 | `PH7_PRIVATE int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 |  522 | `{` |
|         - |  523 | `	SyHashEntry *pEntry;` |
|         - |  524 | `	ph7_constant *pCons;` |
|         - |  525 | `	const char *zName; /* Constant name */` |
|         - |  526 | `	ph7_value sVal;    /* Constant value */` |
|         - |  527 | `	int nLen;` |
|      1804 |  528 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|         - |  529 | `		/* Invallid argument,return NULL */` |
|       ! 0 |  530 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|       ! 0 |  531 | `		ph7_result_null(pCtx);` |
|       ! 0 |  532 | `		return SXRET_OK;` |
|         - |  533 | `	}` |
|         - |  534 | `	/* Extract the constant name */` |
|      1804 |  535 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
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
|      1804 |  546 | `		ph7_class_attr *pAttr = 0;` |
|      1804 |  547 | `		ph7_class *pClass = 0;` |
|      1804 |  548 | `		int iSep = 0;` |
|      1804 |  549 | `		int iRc = VmClassConstLookup(pCtx->pVm,zName,nLen,&pClass,&pAttr,&iSep);` |
|      1804 |  550 | `		if( iRc != VM_CCONST_PLAIN ){` |
|       112 |  551 | `			if( iRc != VM_CCONST_OK ){` |
|        76 |  552 | `				return VmClassConstError(pCtx,iRc,zName,nLen,iSep,pAttr);` |
|         - |  553 | `			}` |
|        74 |  554 | `			if( pAttr->nIdx == SXU32_HIGH ){` |
|         - |  555 | `				/* Unmaterialized: enum case → materialize the singletons` |
|         - |  556 | `				 * (all of them: constant("S::A") is a direct access, like` |
|         - |  557 | `				 * OP_MEMBER); plain constant → run its initializer. Unlike` |
|         - |  558 | `				 * defined(), reading the value has to force this. */` |
|         - |  559 | `				sxi32 rcEnum;` |
|        44 |  560 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE ){` |
|         3 |  561 | `					rcEnum = VmEnumMaterialize(pCtx->pVm,pClass);` |
|         2 |  562 | `				}else{` |
|        42 |  563 | `					rcEnum = VmClassConstEvalOnDemand(pCtx->pVm,pClass,pAttr);` |
|         - |  564 | `				}` |
|        44 |  565 | `				if( rcEnum != SXRET_OK ){` |
|         3 |  566 | `					return rcEnum;` |
|         - |  567 | `				}` |
|        20 |  568 | `			}` |
|         - |  569 | `			{` |
|        72 |  570 | `				ph7_value *pValue = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj,pAttr->nIdx);` |
|        72 |  571 | `				if( pValue ){` |
|        72 |  572 | `					if( SySetUsed(&pAttr->aAttrs) > 0 ){` |
|         - |  573 | `						/* #[\Deprecated] warns through constant() too (php) */` |
|         3 |  574 | `						VmDeprecatedConstNotice(pCtx->pVm,pClass,pAttr);` |
|         1 |  575 | `					}` |
|        72 |  576 | `					ph7_result_value(pCtx,pValue);` |
|        72 |  577 | `					return SXRET_OK;` |
|         - |  578 | `				}` |
|         - |  579 | `			}` |
|         - |  580 | `			/* Declared but with no slot to read: the same dead end the pre-fix code` |
|         - |  581 | `			 * fell through to. */` |
|       ! 0 |  582 | `			return VmClassConstError(pCtx,VM_CCONST_NOCONST,zName,nLen,iSep,pAttr);` |
|         - |  583 | `		}` |
|         - |  584 | `	}` |
|         - |  585 | `	/* Perform the query */` |
|      1693 |  586 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|      1693 |  587 | `	if( pEntry == 0 ){` |
|         - |  588 | `		/* php 8: a catchable Error, not a notice + NULL (band A #4) */` |
|         8 |  589 | `		return PH7_VmThrowException(pCtx,"Error",` |
|         2 |  590 | `			"Undefined constant \"%.*s\"",nLen,zName);` |
|         - |  591 | `	}` |
|      1689 |  592 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|         - |  593 | `	/* Point to the structure that describe the constant */` |
|      1689 |  594 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|         - |  595 | `	/* Extract constant value by calling it's associated callback` |
|         - |  596 | `	 * (emits the #[\Deprecated] notice first when attributed, php) */` |
|      1689 |  597 | `	VmExpandConstantWithNotice(pCtx->pVm,pCons,&sVal);` |
|         - |  598 | `	/* Return that value */` |
|      1689 |  599 | `	ph7_result_value(pCtx,&sVal);` |
|         - |  600 | `	/* Cleanup */` |
|      1689 |  601 | `	PH7_MemObjRelease(&sVal);` |
|      1689 |  602 | `	return SXRET_OK;` |
|       904 |  603 | `}` |
|         - |  604 | `/*` |
|         - |  605 | ` * Hash walker callback used by the [get_defined_constants()] function defined` |
|         - |  606 | ` * below. php's answer is a MAP -- the constant's name is the KEY and its VALUE` |
|         - |  607 | ``  * is the element -- which is what makes `get_defined_constants()['PHP_EOL']` `` |
|         - |  608 | ` * the documented way to read one. PHL used to answer a LIST of names, so every` |
|         - |  609 | ``  * such lookup was an `Undefined array key` and NULL, and `in_array($n, $c)` `` |
|         - |  610 | `` * answered where php wants `isset($c[$n])`: the array had the right length and`` |
|         - |  611 | ` * the wrong shape.` |
|         - |  612 | ` */` |
|    131676 |  613 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|         3 |  614 | `{` |
|         - |  615 | ``	/* SNAPSHOT ONLY -- nothing is expanded during the walk. A `const` whose`` |
|         - |  616 | `	 * initializer is a bytecode program runs USER CODE when it expands, and user` |
|         - |  617 | ``	 * code can `define()`: that grows hConstant while SyHashForEach is holding a`` |
|         - |  618 | `	 * fixed entry count, and the walk then runs off the end of the bucket chain` |
|         - |  619 | `	 * (a segfault, reproducible from a const initializer that constructs an` |
|         - |  620 | `	 * object whose __construct defines a constant). Collect first, expand after. */` |
|    131679 |  621 | `	SySet *pOut = (SySet *)pUserData;` |
|    131679 |  622 | `	if( pEntry == 0 \|\| pEntry->pUserData == 0 ){` |
|       ! 0 |  623 | `		return SXRET_OK;` |
|         - |  624 | `	}` |
|    131679 |  625 | `	SySetPut(pOut,(const void *)&pEntry);` |
|    131679 |  626 | `	return SXRET_OK;` |
|     63920 |  627 | `}` |
|         - |  628 | `/*` |
|         - |  629 | ` * Add one snapshotted constant to the answer, under its name.` |
|         - |  630 | ` */` |
|    131676 |  631 | `static sxi32 VmConstDumpEntry(ph7_value *pTarget,SyHashEntry *pEntry)` |
|         3 |  632 | `{` |
|    131679 |  633 | `	ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|         - |  634 | `	ph7_value sName,sVal;` |
|         - |  635 | `	sxi32 rc;` |
|         - |  636 | `	/* Prepare the constant name for insertion */` |
|    131679 |  637 | `	PH7_MemObjInitFromString(pTarget->pVm,&sName,0);` |
|    131679 |  638 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|         - |  639 | `	/* ...and its VALUE, through the SAME evaluate-once path an ordinary read` |
|         - |  640 | ``	 * takes -- so a `const C = new Foo();` reported here is the object the`` |
|         - |  641 | ``	 * program itself sees, not a second one. The `#[\Deprecated]` NOTICE is what`` |
|         - |  642 | `	 * is skipped (VmExpandConstantOnce rather than …WithNotice): describing a` |
|         - |  643 | `	 * constant is not reading one, and php raises nothing here either. */` |
|    131679 |  644 | `	PH7_MemObjInit(pTarget->pVm,&sVal);` |
|    131679 |  645 | `	rc = VmExpandConstantOnce(pTarget->pVm,pCons,&sVal);` |
|    131679 |  646 | `	if( rc == SXRET_OK ){` |
|    131679 |  647 | `		ph7_array_add_elem(pTarget,&sName,&sVal); /* Will make its own copy */` |
|     63917 |  648 | `	}` |
|    131679 |  649 | `	PH7_MemObjRelease(&sVal);` |
|    131679 |  650 | `	PH7_MemObjRelease(&sName);` |
|    131679 |  651 | `	return rc;` |
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
|     19180 |  680 | `static int VmConstBucketStep(const char *zName,int nName,void *pData)` |
|         3 |  681 | `{` |
|     19183 |  682 | `	VmConstBucket *p = (VmConstBucket *)pData;` |
|         - |  683 | `	SyHashEntry *pEntry;` |
|     19183 |  684 | `	if( !PH7_VmInternalNameExists(p->pVm,PH7_EXT_KIND_CONST,zName,nName) ){` |
|       688 |  685 | `		return 0;` |
|         - |  686 | `	}` |
|     18498 |  687 | `	pEntry = SyHashGet(&p->pVm->hConstant,(const void *)zName,(sxu32)nName);` |
|     18498 |  688 | `	if( pEntry == 0 ){` |
|       ! 0 |  689 | `		return 0;` |
|         - |  690 | `	}` |
|     18498 |  691 | `	p->rc = VmConstDumpEntry(p->pOut,pEntry);` |
|     18498 |  692 | `	return p->rc == SXRET_OK ? 0 : 1;` |
|      9593 |  693 | `}` |
|        68 |  694 | `PH7_PRIVATE int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 |  695 | `{` |
|        71 |  696 | `	ph7_value *pArray,*pAll,*pUser = 0;` |
|         - |  697 | `	ph7_value *apBucket[PH7_EXT_MAX];   /* one per extension; filled below when categorizing */` |
|         - |  698 | `	SySet aSnap;` |
|         - |  699 | `	SyHashEntry **apEntry;` |
|         - |  700 | `	sxu32 n,nSnap;` |
|        71 |  701 | `	int iExt,nExt = 0;` |
|        71 |  702 | `	int bCategorize = nArg > 0 && ph7_value_to_bool(apArg[0]);` |
|      4423 |  703 | `	for( iExt = 0 ; iExt < PH7_EXT_MAX ; ++iExt ){` |
|      4355 |  704 | `		apBucket[iExt] = 0;` |
|      2179 |  705 | `	}` |
|         - |  706 | `	/* Create the array first*/` |
|        71 |  707 | `	pArray = ph7_context_new_array(pCtx);` |
|        71 |  708 | `	if( pArray == 0 ){` |
|         - |  709 | `		/* Return NULL */` |
|       ! 0 |  710 | `		ph7_result_null(pCtx);` |
|       ! 0 |  711 | `		return SXRET_OK;` |
|         - |  712 | `	}` |
|        71 |  713 | `	pAll = pArray;` |
|        71 |  714 | `	if( bCategorize ){` |
|         - |  715 | `		/* php's categories are the extensions, in the order get_loaded_extensions()` |
|         - |  716 | ``		 * lists them and each in its OWN registration order, with `user` last --`` |
|         - |  717 | `		 * and php OMITS a category with nothing in it. Before this engine had an` |
|         - |  718 | ``		 * extension partition all 1314 answers sat under a single `Core`.`` |
|         - |  719 | `		 *` |
|         - |  720 | `		 * The per-extension pass runs first so that each bucket is in php's` |
|         - |  721 | `		 * order; the table walk that follows only has to place what the` |
|         - |  722 | `		 * partition has no row for, which rides with Core. */` |
|        13 |  723 | `		nExt = PH7_VmExtensionCount();` |
|        13 |  724 | `		if( nExt > PH7_EXT_MAX ){` |
|       ! 0 |  725 | `			nExt = PH7_EXT_MAX;` |
|       ! 0 |  726 | `		}` |
|       363 |  727 | `		for( iExt = 0 ; iExt < nExt ; ++iExt ){` |
|       353 |  728 | `			apBucket[iExt] = PH7_VmExtensionAvailable(iExt) ? ph7_context_new_array(pCtx) : 0;` |
|       178 |  729 | `		}` |
|        13 |  730 | `		pUser = ph7_context_new_array(pCtx);` |
|        13 |  731 | `		pAll = apBucket[PH7_EXT_CORE];` |
|        13 |  732 | `		if( pAll == 0 \|\| pUser == 0 ){` |
|       ! 0 |  733 | `			ph7_result_null(pCtx);` |
|       ! 0 |  734 | `			return SXRET_OK;` |
|         - |  735 | `		}` |
|       363 |  736 | `		for( iExt = 0 ; iExt < nExt ; ++iExt ){` |
|         - |  737 | `			VmConstBucket sBucket;` |
|       353 |  738 | `			if( apBucket[iExt] == 0 ){` |
|         3 |  739 | `				continue;` |
|         - |  740 | `			}` |
|       353 |  741 | `			sBucket.pVm = pCtx->pVm;` |
|       353 |  742 | `			sBucket.pOut = apBucket[iExt];` |
|       353 |  743 | `			sBucket.rc = SXRET_OK;` |
|       353 |  744 | `			pCtx->pVm->bConstEnum++;` |
|       353 |  745 | `			PH7_VmExtWalk(iExt,PH7_EXT_KIND_CONST,VmConstBucketStep,&sBucket);` |
|       353 |  746 | `			pCtx->pVm->bConstEnum--;` |
|       353 |  747 | `			if( sBucket.rc != SXRET_OK ){` |
|       ! 0 |  748 | `				return sBucket.rc;` |
|         - |  749 | `			}` |
|       178 |  750 | `		}` |
|         5 |  751 | `	}` |
|         - |  752 | `	/* Snapshot the table, then expand: expanding runs user code, which may` |
|         - |  753 | `	 * define() and grow the table under the walk (see VmHashConstStep). */` |
|        71 |  754 | `	SySetInit(&aSnap,&pCtx->pVm->sAllocator,sizeof(SyHashEntry *));` |
|        71 |  755 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,&aSnap);` |
|        71 |  756 | `	apEntry = (SyHashEntry **)SySetBasePtr(&aSnap);` |
|        71 |  757 | `	nSnap = SySetUsed(&aSnap);` |
|         - |  758 | `	/* Describing the table is not READING its entries: php's deprecated constants` |
|         - |  759 | `	 * report when a program names one, and get_defined_constants() lists them in` |
|         - |  760 | `	 * silence. */` |
|        71 |  761 | `	pCtx->pVm->bConstEnum++;` |
|    131747 |  762 | `	for( n = 0 ; n < nSnap ; ++n ){` |
|    131679 |  763 | `		ph7_constant *pCons = (ph7_constant *)apEntry[n]->pUserData;` |
|         - |  764 | `		sxi32 rcExp;` |
|    131676 |  765 | `		if( bCategorize && !pCons->bUserDefined` |
|     19175 |  766 | `		 && PH7_VmExtHasName(PH7_EXT_KIND_CONST,(const char *)apEntry[n]->pKey,` |
|     19075 |  767 | `				(int)apEntry[n]->nKeyLen) ){` |
|     18498 |  768 | `			continue;   /* the per-extension pass already placed it */` |
|         - |  769 | `		}` |
|    113184 |  770 | `		rcExp = VmConstDumpEntry(pUser && pCons->bUserDefined ? pUser : pAll,apEntry[n]);` |
|    113184 |  771 | `		if( rcExp != SXRET_OK ){` |
|         - |  772 | `			/* An initializer raised while being described: stop, exactly as any` |
|         - |  773 | `			 * other builtin does when the php it invoked did not return.` |
|         - |  774 | `			 * Carrying on would run every LATER initializer past a throw that has` |
|         - |  775 | `			 * already been landed. */` |
|       ! 0 |  776 | `			pCtx->pVm->bConstEnum--;` |
|       ! 0 |  777 | `			SySetRelease(&aSnap);` |
|       ! 0 |  778 | `			return rcExp;` |
|         - |  779 | `		}` |
|     54955 |  780 | `	}` |
|        71 |  781 | `	pCtx->pVm->bConstEnum--;` |
|        71 |  782 | `	SySetRelease(&aSnap);` |
|        71 |  783 | `	if( bCategorize ){` |
|       363 |  784 | `		for( iExt = 0 ; iExt < nExt ; ++iExt ){` |
|       353 |  785 | `			if( apBucket[iExt] == 0 ){` |
|         3 |  786 | `				continue;` |
|         - |  787 | `			}` |
|       353 |  788 | `			if( ph7_array_count(apBucket[iExt]) > 0 ){` |
|       243 |  789 | `				ph7_array_add_strkey_elem(pArray,PH7_VmExtensionName(iExt),apBucket[iExt]);` |
|       120 |  790 | `			}` |
|       353 |  791 | `			ph7_context_release_value(pCtx,apBucket[iExt]);` |
|       178 |  792 | `		}` |
|        13 |  793 | `		if( ph7_array_count(pUser) > 0 ){` |
|        11 |  794 | `			ph7_array_add_strkey_elem(pArray,"user",pUser);` |
|         4 |  795 | `		}` |
|        13 |  796 | `		ph7_context_release_value(pCtx,pUser);` |
|         5 |  797 | `	}` |
|         - |  798 | `	/* Return the created array */` |
|        71 |  799 | `	ph7_result_value(pCtx,pArray);` |
|        71 |  800 | `	return SXRET_OK;` |
|        37 |  801 | `}` |
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
|   9435465 |  923 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|         5 |  924 | `{` |
|         - |  925 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|         - |  926 | `	int i;` |
|         - |  927 | `	/* Generate a binary string first */` |
|   9435470 |  928 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|         - |  929 | `	/* Turn the binary string into english based alphabet */` |
| 103791276 |  930 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  94355811 |  931 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|  47114773 |  932 | `	 }` |
|   9435470 |  933 | `}` |
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
|       578 | 1043 | `PH7_PRIVATE int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 1044 | `{` |
|         - | 1045 | `	char zString[1024];` |
|       583 | 1046 | `	int iLen = 0x10;` |
|       583 | 1047 | `	if( nArg > 0 ){` |
|         - | 1048 | `		/* Get the desired length */` |
|       583 | 1049 | `		iLen = ph7_value_to_int(apArg[0]);` |
|       583 | 1050 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|         - | 1051 | `			/* Default length */` |
|         3 | 1052 | `			iLen = 0x10;` |
|         1 | 1053 | `		}` |
|       289 | 1054 | `	}` |
|         - | 1055 | `	/* Generate the random string */` |
|       583 | 1056 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|         - | 1057 | `	/* Return the generated string */` |
|       583 | 1058 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|       583 | 1059 | `	return SXRET_OK;` |
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
|       337 | 1136 | `	for( nAttempt = 0 ; nAttempt < 50 ; ++nAttempt ){` |
|         - | 1137 | `		/* Always draw a full 8 bytes so endianness of the cast doesn't matter` |
|         - | 1138 | `		 * (a 4-byte fill into a sxu64 would land in the high half on big-endian` |
|         - | 1139 | `		 * and the low-half mask would always read 0). */` |
|         - | 1140 | `		sxu64 uDraw;` |
|       337 | 1141 | `		if( SyOSCSPRNG(&uDraw,sizeof(uDraw)) != SXRET_OK ){` |
|       ! 0 | 1142 | `			return PH7_VmThrowException(pCtx,` |
|         - | 1143 | `				"Random\\RandomException",` |
|         - | 1144 | `				"Cannot gather sufficient random data"` |
|         - | 1145 | `				);` |
|         - | 1146 | `		}` |
|       337 | 1147 | `		uDraw &= uMask;` |
|       337 | 1148 | `		if( uDraw <= uRange ){` |
|       225 | 1149 | `			uResult = uDraw;` |
|       225 | 1150 | `			break;` |
|         - | 1151 | `		}` |
|        52 | 1152 | `	}` |
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
|         1 | 1263 | `			pVm->iLcgS2 = -pVm->iLcgS2;` |
|         1 | 1264 | `		}` |
|         3 | 1265 | `		pVm->iLcgS1 = (pVm->iLcgS1 % 2147483562) + 1;` |
|         3 | 1266 | `		pVm->iLcgS2 = (pVm->iLcgS2 % 2147483398) + 1;` |
|         3 | 1267 | `		pVm->bLcgSeeded = 1;` |
|         1 | 1268 | `	}` |
|       605 | 1269 | `	PH7_LCG_MODMULT(53668,40014,12211,2147483563L,pVm->iLcgS1)` |
|       605 | 1270 | `	PH7_LCG_MODMULT(52774,40692,3791,2147483399L,pVm->iLcgS2)` |
|       605 | 1271 | `	z = pVm->iLcgS1 - pVm->iLcgS2;` |
|       605 | 1272 | `	if( z < 1 ){` |
|       287 | 1273 | `		z += 2147483562;` |
|       139 | 1274 | `	}` |
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
|         - | 1289 | ` * comparison and stays a string on the other (ECOSYSTEM.md F69).` |
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
|        58 | 1344 | `static sxi32 VmOutputArgToString(ph7_context *pCtx,ph7_value *pArg,const char **pzData,int *pnLen)` |
|         4 | 1345 | `{` |
|        62 | 1346 | `	sxi32 rc = PH7_MemObjToStringUV(pArg);` |
|        62 | 1347 | `	if( rc != SXRET_OK ){` |
|         3 | 1348 | `		pCtx->nThrowRc = rc;` |
|         3 | 1349 | `		return rc;` |
|         - | 1350 | `	}` |
|        60 | 1351 | `	*pzData = ph7_value_to_string(pArg,pnLen);` |
|        60 | 1352 | `	return SXRET_OK;` |
|        33 | 1353 | `}` |
|         - | 1354 | `/*` |
|         - | 1355 | ` * void echo($string...)` |
|         - | 1356 | ` *  Output one or more messages.` |
|         - | 1357 | ` * Parameters` |
|         - | 1358 | ` *  $string` |
|         - | 1359 | ` *   Message to output.` |
|         - | 1360 | ` * Return` |
|         - | 1361 | ` *  NULL.` |
|         - | 1362 | ` */` |
|       ! 0 | 1363 | `PH7_PRIVATE int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       ! 0 | 1364 | `{` |
|         - | 1365 | `	const char *zData;` |
|       ! 0 | 1366 | `	int nDataLen = 0;` |
|         - | 1367 | `	ph7_vm *pVm;` |
|         - | 1368 | `	int i,rc;` |
|         - | 1369 | `	/* Point to the target VM */` |
|       ! 0 | 1370 | `	pVm = pCtx->pVm;` |
|         - | 1371 | `	/* Output */` |
|       ! 0 | 1372 | `	for( i = 0 ; i < nArg ; ++i ){` |
|       ! 0 | 1373 | `		sxi32 rcSv = VmOutputArgToString(pCtx,apArg[i],&zData,&nDataLen);` |
|       ! 0 | 1374 | `		if( rcSv != SXRET_OK ){` |
|       ! 0 | 1375 | `			return rcSv;` |
|         - | 1376 | `		}` |
|       ! 0 | 1377 | `		if( nDataLen > 0 ){` |
|       ! 0 | 1378 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|       ! 0 | 1379 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|       ! 0 | 1380 | `			if( rc == SXERR_ABORT ){` |
|         - | 1381 | `				/* Output consumer callback request an operation abort */` |
|       ! 0 | 1382 | `				return PH7_ABORT;` |
|         - | 1383 | `			}` |
|       ! 0 | 1384 | `		}` |
|       ! 0 | 1385 | `	}` |
|       ! 0 | 1386 | `	return SXRET_OK;` |
|       ! 0 | 1387 | `}` |
|         - | 1388 | `/*` |
|         - | 1389 | ` * int print($string...)` |
|         - | 1390 | ` *  Output one or more messages.` |
|         - | 1391 | ` * Parameters` |
|         - | 1392 | ` *  $string` |
|         - | 1393 | ` *   Message to output.` |
|         - | 1394 | ` * Return` |
|         - | 1395 | ` *  1 always.` |
|         - | 1396 | ` */` |
|        58 | 1397 | `PH7_PRIVATE int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 1398 | `{` |
|         - | 1399 | `	const char *zData;` |
|        62 | 1400 | `	int nDataLen = 0;` |
|         - | 1401 | `	ph7_vm *pVm;` |
|         - | 1402 | `	int i,rc;` |
|         - | 1403 | `	/* Point to the target VM */` |
|        62 | 1404 | `	pVm = pCtx->pVm;` |
|         - | 1405 | `	/* Output */` |
|       118 | 1406 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        62 | 1407 | `		sxi32 rcSv = VmOutputArgToString(pCtx,apArg[i],&zData,&nDataLen);` |
|        62 | 1408 | `		if( rcSv != SXRET_OK ){` |
|         3 | 1409 | `			return rcSv;` |
|         - | 1410 | `		}` |
|        60 | 1411 | `		if( nDataLen > 0 ){` |
|        60 | 1412 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        60 | 1413 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        60 | 1414 | `			if( rc == SXERR_ABORT ){` |
|         - | 1415 | `				/* Output consumer callback request an operation abort */` |
|       ! 0 | 1416 | `				return PH7_ABORT;` |
|         - | 1417 | `			}` |
|        28 | 1418 | `		}` |
|        32 | 1419 | `	}` |
|         - | 1420 | `	/* Return 1 */` |
|        60 | 1421 | `	ph7_result_int(pCtx,1);` |
|        60 | 1422 | `	return SXRET_OK;` |
|        33 | 1423 | `}` |
|         - | 1424 | `/*` |
|         - | 1425 | ` * void exit(string $msg)` |
|         - | 1426 | ` * void exit(int $status)` |
|         - | 1427 | ` * void die(string $ms)` |
|         - | 1428 | ` * void die(int $status)` |
|         - | 1429 | ` *   Output a message and terminate program execution.` |
|         - | 1430 | ` * Parameter` |
|         - | 1431 | ` *  If status is a string, this function prints the status just before exiting.` |
|         - | 1432 | ` *  If status is an integer, that value will be used as the exit status` |
|         - | 1433 | ` *  and not printed` |
|         - | 1434 | ` * Return` |
|         - | 1435 | ` *  NULL` |
|         - | 1436 | ` */` |
|       ! 0 | 1437 | `PH7_PRIVATE int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       ! 0 | 1438 | `{` |
|       ! 0 | 1439 | `	if( nArg > 0 ){` |
|       ! 0 | 1440 | `		if( ph7_value_is_string(apArg[0]) ){` |
|         - | 1441 | `			const char *zData;` |
|       ! 0 | 1442 | `			int iLen = 0;` |
|         - | 1443 | `			/* Print exit message */` |
|       ! 0 | 1444 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|       ! 0 | 1445 | `			ph7_context_output(pCtx,zData,iLen);` |
|       ! 0 | 1446 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|         - | 1447 | `			sxi32 iExitStatus;` |
|         - | 1448 | `			/* Record exit status code */` |
|       ! 0 | 1449 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|       ! 0 | 1450 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|       ! 0 | 1451 | `		}` |
|       ! 0 | 1452 | `	}` |
|         - | 1453 | `	/* Request a VM-wide halt (see PH7_OP_HALT) and abort processing` |
|         - | 1454 | `	 * immediately; the abort unwinds enclosing frames and execution units.` |
|         - | 1455 | `	 */` |
|       ! 0 | 1456 | `	pCtx->pVm->bHaltRequested = 1;` |
|       ! 0 | 1457 | `	return PH7_ABORT;` |
|       ! 0 | 1458 | `}` |
|         - | 1459 | `/*` |
|         - | 1460 | ` * Section:` |
|         - | 1461 | ` *  Version,Credits and Copyright related functions.` |
|         - | 1462 | ` * Status:` |
|         - | 1463 | ` *    Stable.` |
|         - | 1464 | ` */` |
|         - | 1465 | `/*` |
|         - | 1466 | ` * string ph7version(void)` |
|         - | 1467 | ` *  Returns the running version of the PH7 version.` |
|         - | 1468 | ` * Parameters` |
|         - | 1469 | ` *  None` |
|         - | 1470 | ` * Return` |
|         - | 1471 | ` * Current PH7 version.` |
|         - | 1472 | ` */` |
|         2 | 1473 | `PH7_PRIVATE int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 1474 | `{` |
|         1 | 1475 | `	SXUNUSED(nArg);` |
|         1 | 1476 | `	SXUNUSED(apArg); /* cc warning */` |
|         - | 1477 | `	/* Current engine version */` |
|         3 | 1478 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|         3 | 1479 | `	return PH7_OK;` |
|         1 | 1480 | `}` |
|         - | 1481 | `/*` |
|         - | 1482 | ` * string\|false phpversion([ ?string $extension = null ])` |
|         - | 1483 | ` *  Returns the PHP-compatibility version PHL advertises (see PHP_COMPAT_VERSION).` |
|         - | 1484 | ` * Parameters` |
|         - | 1485 | ` *  $extension (optional): an extension name, matched case-insensitively against` |
|         - | 1486 | ` *  the ones this engine reports as loaded.` |
|         - | 1487 | ` * Return` |
|         - | 1488 | ` *  The version string — for the engine with no argument (or an explicit NULL),` |
|         - | 1489 | ` *  and for a loaded extension, whose version IS the engine's since every one of` |
|         - | 1490 | ` *  them is part of it — or FALSE for a name it does not report.` |
|         - | 1491 | ` */` |
|       244 | 1492 | `PH7_PRIVATE int vm_builtin_phpversion(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 1493 | `{` |
|         - | 1494 | `	/* $extension was declared in the signature and answered NULL for everything:` |
|         - | 1495 | `	 * an unknown one where php answers FALSE (so the documented` |
|         - | 1496 | ``	 * `if (phpversion($e) === false)` check never fired and a version comparison`` |
|         - | 1497 | `	 * ran against NULL), a KNOWN one where php answers the version string, and` |
|         - | 1498 | `	 * even the explicit NULL that means "no extension" at all.` |
|         - | 1499 | `	 *` |
|         - | 1500 | `	 * Every extension this engine reports as loaded is part of the engine, so its` |
|         - | 1501 | `	 * version IS the engine's — which is also what php answers for its own` |
|         - | 1502 | `	 * bundled ones — and a name it does not report is php's false. */` |
|       246 | 1503 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0 ){` |
|         - | 1504 | `		int nName;` |
|       238 | 1505 | `		const char *zName = ph7_value_to_string(apArg[0],&nName);` |
|       238 | 1506 | `		if( !PH7_VmExtensionIsLoaded(pCtx->pVm,zName,nName) ){` |
|        15 | 1507 | `			ph7_result_bool(pCtx,0);` |
|        15 | 1508 | `			return PH7_OK;` |
|         - | 1509 | `		}` |
|       111 | 1510 | `	}` |
|       232 | 1511 | `	ph7_result_string(pCtx,PHP_COMPAT_VERSION,(int)sizeof(PHP_COMPAT_VERSION) - 1);` |
|       232 | 1512 | `	return PH7_OK;` |
|       124 | 1513 | `}` |
|         - | 1514 | `/*` |
|         - | 1515 | ` * string php_sapi_name(void)` |
|         - | 1516 | ` *  Returns the type of interface (SAPI) PHL is running under.` |
|         - | 1517 | ` * Parameters` |
|         - | 1518 | ` *  None` |
|         - | 1519 | ` * Return` |
|         - | 1520 | ` *  "cli-server" while serving an HTTP request via the built-in -S server, "cli" otherwise.` |
|         - | 1521 | ` */` |
|         2 | 1522 | `PH7_PRIVATE int vm_builtin_php_sapi_name(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 1523 | `{` |
|         3 | 1524 | `	const char *zSapi = pCtx->pVm->bHttpContext ? "cli-server" : "cli";` |
|         1 | 1525 | `	SXUNUSED(nArg);` |
|         1 | 1526 | `	SXUNUSED(apArg); /* cc warning */` |
|         3 | 1527 | `	ph7_result_string(pCtx,zSapi,-1);` |
|         3 | 1528 | `	return PH7_OK;` |
|         1 | 1529 | `}` |
|         - | 1530 | `/*` |
|         - | 1531 | ` * string\|false php_ini_loaded_file(void)` |
|         - | 1532 | ` * string\|false php_ini_scanned_files(void)` |
|         - | 1533 | ` *  Which php.ini this interpreter read, and which files it then scanned.` |
|         - | 1534 | ` * Return` |
|         - | 1535 | ` *  false from both, and that is php's OWN answer rather than a stub: a php` |
|         - | 1536 | `` *  started with `-n`, or built with no `--with-config-file-scan-dir`, answers`` |
|         - | 1537 | ` *  exactly this. PHL reads no configuration FILE at all -- its directives come` |
|         - | 1538 | `` *  from the table in vm_builtin_ini.c and from `-d` -- so there is never a path`` |
|         - | 1539 | ` *  to name. Composer's XdebugHandler asks both on its way to reporting where a` |
|         - | 1540 | ` *  directive came from, and takes "nowhere" for an answer.` |
|         - | 1541 | ` */` |
|       ! 0 | 1542 | `PH7_PRIVATE int vm_builtin_php_ini_loaded_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       ! 0 | 1543 | `{` |
|       ! 0 | 1544 | `	SXUNUSED(nArg);` |
|       ! 0 | 1545 | `	SXUNUSED(apArg);` |
|       ! 0 | 1546 | `	ph7_result_bool(pCtx,0);` |
|       ! 0 | 1547 | `	return PH7_OK;` |
|       ! 0 | 1548 | `}` |
|         - | 1549 | `/*` |
|         - | 1550 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|         - | 1551 | ` */` |
|         - | 1552 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|         - | 1553 | ` "<html><head>"\` |
|         - | 1554 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|         - | 1555 | ` "<style type=\"text/css\">"\` |
|         - | 1556 | ` "div {"\` |
|         - | 1557 | `     "border: 1px solid #cccccc;"\` |
|         - | 1558 | `     "-moz-border-radius-topleft: 10px;"\` |
|         - | 1559 | `     "-moz-border-radius-bottomright: 10px;"\` |
|         - | 1560 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|         - | 1561 | `     "-moz-border-radius-topright: 10px;"\` |
|         - | 1562 | `     "-webkit-border-radius: 10px;"\` |
|         - | 1563 | `     "-o-border-radius: 10px;"\` |
|         - | 1564 | `     "border-radius: 10px;"\` |
|         - | 1565 | `     "padding-left: 2em;"\` |
|         - | 1566 | `     "background-color: white;"\` |
|         - | 1567 | `     "margin-left: auto;"\` |
|         - | 1568 | `     "font-family: verdana;"\` |
|         - | 1569 | `     "padding-right: 2em;"\` |
|         - | 1570 | `     "margin-right: auto;"\` |
|         - | 1571 | `     "}"\` |
|         - | 1572 | `     "body {"\` |
|         - | 1573 | `     "padding: 0.2em;"\` |
|         - | 1574 | `     "font-style: normal;"\` |
|         - | 1575 | `     "font-size: medium;"\` |
|         - | 1576 | `     "background-color: #f2f2f2;"\` |
|         - | 1577 | `     "}"\` |
|         - | 1578 | `     "hr {"\` |
|         - | 1579 | `     "border-style: solid none none;"\` |
|         - | 1580 | `     "border-width: 1px medium medium;"\` |
|         - | 1581 | `     "border-top: 1px solid #cccccc;"\` |
|         - | 1582 | `     "height: 1px;"\` |
|         - | 1583 | `     "}"\` |
|         - | 1584 | `     "a {"\` |
|         - | 1585 | `     "color: #3366cc;"\` |
|         - | 1586 | `     "text-decoration: none;"\` |
|         - | 1587 | `     "}"\` |
|         - | 1588 | `     "a:hover {"\` |
|         - | 1589 | `     "color: #999999;"\` |
|         - | 1590 | `     "}"\` |
|         - | 1591 | `     "a:active {"\` |
|         - | 1592 | `     "color: #663399;"\` |
|         - | 1593 | `     "}"\` |
|         - | 1594 | `     "h1 {"\` |
|         - | 1595 | `     "margin: 0;"\` |
|         - | 1596 | `     "padding: 0;"\` |
|         - | 1597 | `     "font-family: Verdana;"\` |
|         - | 1598 | `     "font-weight: bold;"\` |
|         - | 1599 | `     "font-style: normal;"\` |
|         - | 1600 | `     "font-size: medium;"\` |
|         - | 1601 | `     "text-transform: capitalize;"\` |
|         - | 1602 | `     "color: #0a328c;"\` |
|         - | 1603 | `     "}"\` |
|         - | 1604 | `     "p {"\` |
|         - | 1605 | `     "margin: 0 auto;"\` |
|         - | 1606 | `     "font-size: medium;"\` |
|         - | 1607 | `     "font-style: normal;"\` |
|         - | 1608 | `     "font-family: verdana;"\` |
|         - | 1609 | `     "}"\` |
|         - | 1610 | `"</style></head><body>"\` |
|         - | 1611 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|         - | 1612 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|         - | 1613 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|         - | 1614 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|         - | 1615 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|         - | 1616 | `"<p style=\"text-align: left;\"><small><small>"\` |
|         - | 1617 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|         - | 1618 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|         - | 1619 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|         - | 1620 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|         - | 1621 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|         - | 1622 |  |
|         - | 1623 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|         - | 1624 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|         - | 1625 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|         - | 1626 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|         - | 1627 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|         - | 1628 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|         - | 1629 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|         - | 1630 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|         - | 1631 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|         - | 1632 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|         - | 1633 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|         - | 1634 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|         - | 1635 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|         - | 1636 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|         - | 1637 |  |
|         - | 1638 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|         - | 1639 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|         - | 1640 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|         - | 1641 | `"&nbsp;*<br>"\` |
|         - | 1642 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|         - | 1643 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|         - | 1644 | `"&nbsp;* are met:<br>"\` |
|         - | 1645 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|         - | 1646 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|         - | 1647 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|         - | 1648 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|         - | 1649 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|         - | 1650 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|         - | 1651 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|         - | 1652 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|         - | 1653 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|         - | 1654 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|         - | 1655 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|         - | 1656 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|         - | 1657 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|         - | 1658 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|         - | 1659 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|         - | 1660 | `"&nbsp;*<br>"\` |
|         - | 1661 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|         - | 1662 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|         - | 1663 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|         - | 1664 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|         - | 1665 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|         - | 1666 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|         - | 1667 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|         - | 1668 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|         - | 1669 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|         - | 1670 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|         - | 1671 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|         - | 1672 | `"&nbsp;*/<br>"\` |
|         - | 1673 | `"</span></small></small></p>"\` |
|         - | 1674 | `"</div></body></html>"` |
|         - | 1675 | `/*` |
|         - | 1676 | ` * bool ph7credits(void)` |
|         - | 1677 | ` * bool ph7info(void)` |
|         - | 1678 | ` * bool ph7copyright(void)` |
|         - | 1679 | ` *  Prints out the credits for PH7 engine` |
|         - | 1680 | ` * Parameters` |
|         - | 1681 | ` *  None` |
|         - | 1682 | ` * Return` |
|         - | 1683 | ` *  Always TRUE` |
|         - | 1684 | ` */` |
|         2 | 1685 | `PH7_PRIVATE int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 1686 | `{` |
|         3 | 1687 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|         - | 1688 | `	/* Expand the HTML page above*/` |
|         3 | 1689 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|         2 | 1690 | `	ph7_context_output_format(` |
|         1 | 1691 | `		pCtx,` |
|         - | 1692 | `		PH7_HTML_PAGE_FORMAT,` |
|         1 | 1693 | `		ph7_lib_version(),   /* Engine version */` |
|         1 | 1694 | `		ph7_lib_signature(), /* Engine signature */` |
|         1 | 1695 | `		ph7_lib_ident(),     /* Engine ID */` |
|         2 | 1696 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|         2 | 1697 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|         1 | 1698 | `		SyHashTotalEntry(&pVm->hClass),` |
|         - | 1699 | `#ifdef __WINNT__` |
|         - | 1700 | `		"Windows NT"` |
|         - | 1701 | `#elif defined(__UNIXES__)` |
|         - | 1702 | `		"UNIX-Like"` |
|         - | 1703 | `#else` |
|         - | 1704 | `		"Other OS"` |
|         - | 1705 | `#endif` |
|         - | 1706 | `		);` |
|         3 | 1707 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|         1 | 1708 | `	SXUNUSED(nArg); /* cc warning */` |
|         1 | 1709 | `	SXUNUSED(apArg);` |
|         - | 1710 | `	/* Return TRUE */` |
|         - | 1711 | `	//ph7_result_bool(pCtx,1);` |
|         3 | 1712 | `	return PH7_OK;` |
|         1 | 1713 | `}` |
|         - | 1714 | `/*` |
|         - | 1715 | ` * Section:` |
|         - | 1716 | ` *    URL related routines.` |
|         - | 1717 | ` * Status:` |
|         - | 1718 | ` *    Stable.` |
|         - | 1719 | ` */` |
|         - | 1720 | `/*` |
|         - | 1721 | ` * value parse_url(string $url [, int $component = -1 ])` |
|         - | 1722 | ` *  Parse a URL and return its fields.` |
|         - | 1723 | ` * Parameters` |
|         - | 1724 | ` *  $url` |
|         - | 1725 | ` *   The URL to parse.` |
|         - | 1726 | ` * $component` |
|         - | 1727 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|         - | 1728 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|         - | 1729 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|         - | 1730 | ` *  in which case the return value will be an integer).` |
|         - | 1731 | ` * Return` |
|         - | 1732 | ` *  If the component parameter is omitted, an associative array is returned.` |
|         - | 1733 | ` *  At least one element will be present within the array. Potential keys within` |
|         - | 1734 | ` *  this array are:` |
|         - | 1735 | ` *   scheme - e.g. http` |
|         - | 1736 | ` *   host` |
|         - | 1737 | ` *   port` |
|         - | 1738 | ` *   user` |
|         - | 1739 | ` *   pass` |
|         - | 1740 | ` *   path` |
|         - | 1741 | ` *   query - after the question mark ?` |
|         - | 1742 | ` *   fragment - after the hashmark #` |
|         - | 1743 | ` * Note:` |
|         - | 1744 | ` *  FALSE is returned on failure.` |
|         - | 1745 | ` *  This function work with relative URL unlike the one shipped` |
|         - | 1746 | ` *  with the standard PHP engine.` |
|         - | 1747 | ` */` |
|         - | 1748 | `/*` |
|         - | 1749 | ` * parse_url() component set.` |
|         - | 1750 | ` *` |
|         - | 1751 | ` * A bare SyString cannot express "present but empty", which php needs:` |
|         - | 1752 | ` * parse_url("") is ['path'=>''] and parse_url("?") is ['query'=>''], both` |
|         - | 1753 | ` * distinct from the component being absent. So presence is tracked separately.` |
|         - | 1754 | ` */` |
|      1328 | 1755 | `static int VmUrlIsAlnum(int c)` |
|         2 | 1756 | `{` |
|      1330 | 1757 | `	return (c >= '0' && c <= '9') \|\| (c >= 'a' && c <= 'z') \|\| (c >= 'A' && c <= 'Z');` |
|         2 | 1758 | `}` |
|        12 | 1759 | `static int VmUrlIsAlpha(int c)` |
|         2 | 1760 | `{` |
|        14 | 1761 | `	return (c >= 'a' && c <= 'z') \|\| (c >= 'A' && c <= 'Z');` |
|         2 | 1762 | `}` |
|         - | 1763 | `/* scheme = ALNUM *( ALNUM / "+" / "-" / "." ) -- php accepts a digit first. */` |
|      1328 | 1764 | `static int VmUrlIsSchemeByte(int c)` |
|         2 | 1765 | `{` |
|      1330 | 1766 | `	return VmUrlIsAlnum(c) \|\| c == '+' \|\| c == '-' \|\| c == '.';` |
|         2 | 1767 | `}` |
|         - | 1768 | `/*` |
|         - | 1769 | ` * Resolve the port span that followed the ':' in an authority.` |
|         - | 1770 | ` *` |
|         - | 1771 | ` * Returns 1 (usable port in *piPort), 0 (the span is empty, so there is simply` |
|         - | 1772 | ` * no port) or -1 (php rejects the whole URL). php is lenient about what follows` |
|         - | 1773 | ` * the digits -- ":8a" and ":8 0" both yield 8 -- but demands at least one digit` |
|         - | 1774 | ` * and a value that fits a port, so ":abc", ":-80" and ":65536" are all failures.` |
|         - | 1775 | ` */` |
|        92 | 1776 | `static int VmUrlParsePort(const char *z,int n,int *piPort)` |
|         2 | 1777 | `{` |
|        94 | 1778 | `	int i = 0,iVal = 0,nDigit = 0;` |
|        94 | 1779 | `	if( n < 1 ){` |
|       ! 0 | 1780 | `		return 0;` |
|         - | 1781 | `	}` |
|       140 | 1782 | `	while( i < n && (z[i] == ' ' \|\| z[i] == '\t') ){` |
|       ! 0 | 1783 | `		i++;` |
|       ! 0 | 1784 | `	}` |
|        94 | 1785 | `	if( i < n && (z[i] == '+' \|\| z[i] == '-') ){` |
|       ! 0 | 1786 | `		if( z[i] == '-' ){` |
|       ! 0 | 1787 | `			return -1;` |
|         - | 1788 | `		}` |
|       ! 0 | 1789 | `		i++;` |
|       ! 0 | 1790 | `	}` |
|       318 | 1791 | `	while( i < n && z[i] >= '0' && z[i] <= '9' ){` |
|       226 | 1792 | `		iVal = iVal * 10 + (z[i] - '0');` |
|       226 | 1793 | `		if( iVal > 65535 ){` |
|       ! 0 | 1794 | `			return -1; /* Out of range, and this also caps the accumulator */` |
|         - | 1795 | `		}` |
|       226 | 1796 | `		nDigit++;` |
|       226 | 1797 | `		i++;` |
|         2 | 1798 | `	}` |
|        94 | 1799 | `	if( nDigit < 1 ){` |
|        12 | 1800 | `		return -1;` |
|         - | 1801 | `	}` |
|        84 | 1802 | `	*piPort = iVal;` |
|        84 | 1803 | `	return 1;` |
|        48 | 1804 | `}` |
|         - | 1805 | `/*` |
|         - | 1806 | ` * Split an authority -- "[user[:pass]@]host[:port]" -- into pOut.` |
|         - | 1807 | ` * Returns 0 for the malformed input php reports as FALSE.` |
|         - | 1808 | ` */` |
|       278 | 1809 | `static int VmUrlParseAuthority(const char *z,int n,VmUrlParts *pOut,int bPortKnown)` |
|         2 | 1810 | `{` |
|         - | 1811 | `	const char *zHost;` |
|       280 | 1812 | `	int i,iAt = -1,iColon = -1,iSep = -1,nHost,iPort = 0,rc;` |
|         - | 1813 | `	/* php splits the credentials at the LAST '@' ("//a@b@c" is user "a@b") */` |
|      2110 | 1814 | `	for( i = 0 ; i < n ; ++i ){` |
|      1832 | 1815 | `		if( z[i] == '@' ){` |
|        44 | 1816 | `			iAt = i;` |
|        21 | 1817 | `		}` |
|       917 | 1818 | `	}` |
|       280 | 1819 | `	if( iAt >= 0 ){` |
|         - | 1820 | `		/* and the user from the password at the FIRST ':' before it */` |
|       154 | 1821 | `		for( i = 0 ; i < iAt ; ++i ){` |
|       152 | 1822 | `			if( z[i] == ':' ){` |
|        42 | 1823 | `				iColon = i;` |
|        42 | 1824 | `				break;` |
|         - | 1825 | `			}` |
|        57 | 1826 | `		}` |
|        44 | 1827 | `		if( iColon >= 0 ){` |
|        42 | 1828 | `			SyStringInitFromBuf(&pOut->sUser,z,(sxu32)iColon);` |
|        42 | 1829 | `			SyStringInitFromBuf(&pOut->sPass,&z[iColon+1],(sxu32)(iAt - iColon - 1));` |
|        42 | 1830 | `			pOut->bUser = pOut->bPass = 1;` |
|        22 | 1831 | `		}else{` |
|         3 | 1832 | `			SyStringInitFromBuf(&pOut->sUser,z,(sxu32)iAt);` |
|         3 | 1833 | `			pOut->bUser = 1;` |
|         - | 1834 | `		}` |
|        44 | 1835 | `		z += iAt + 1;` |
|        44 | 1836 | `		n -= iAt + 1;` |
|        21 | 1837 | `	}` |
|       280 | 1838 | `	zHost = z;` |
|       280 | 1839 | `	nHost = n;` |
|       280 | 1840 | `	if( !(n > 1 && z[0] == '[' && z[n-1] == ']') ){` |
|         - | 1841 | `		/* Unless a bracketed IPv6 literal fills the WHOLE authority -- in which` |
|         - | 1842 | `		 * case php keeps the brackets in "host" and scans no port, so every ':'` |
|         - | 1843 | `		 * inside belongs to the address -- the port hangs off the LAST ':'.` |
|         - | 1844 | `		 * php decides that on the first and last byte alone, which is why` |
|         - | 1845 | `		 * "//[:]\|]" is a single host while "//[::1]:8080" splits a port off. */` |
|      1838 | 1846 | `		for( i = 0 ; i < n ; ++i ){` |
|      1562 | 1847 | `			if( z[i] == ':' ){` |
|       118 | 1848 | `				iSep = i;` |
|        58 | 1849 | `			}` |
|       782 | 1850 | `		}` |
|       278 | 1851 | `		if( iSep >= 0 ){` |
|         - | 1852 | `			/* The host is trimmed at the ':' whether or not the port was already` |
|         - | 1853 | `			 * resolved by the caller. */` |
|        94 | 1854 | `			nHost = iSep;` |
|        94 | 1855 | `			if( !bPortKnown ){` |
|        88 | 1856 | `				rc = VmUrlParsePort(&z[iSep+1],n - iSep - 1,&iPort);` |
|        88 | 1857 | `				if( rc < 0 ){` |
|        12 | 1858 | `					return 0;` |
|         - | 1859 | `				}` |
|        78 | 1860 | `				if( rc > 0 ){` |
|        78 | 1861 | `					pOut->iPort = iPort;` |
|        78 | 1862 | `					pOut->bPort = 1;` |
|        38 | 1863 | `				}` |
|        38 | 1864 | `			}` |
|        41 | 1865 | `		}` |
|       133 | 1866 | `	}` |
|       270 | 1867 | `	if( nHost < 1 ){` |
|         - | 1868 | `		/* php requires a non-empty host once an authority is in play, which is` |
|         - | 1869 | `		 * what makes "//", "http://" and ":80" all FALSE. */` |
|        36 | 1870 | `		return 0;` |
|         - | 1871 | `	}` |
|       236 | 1872 | `	SyStringInitFromBuf(&pOut->sHost,zHost,(sxu32)nHost);` |
|       236 | 1873 | `	pOut->bHost = 1;` |
|       236 | 1874 | `	return 1;` |
|       141 | 1875 | `}` |
|         - | 1876 | `/*` |
|         - | 1877 | ` * Split "path[?query][#fragment]". The fragment is taken FIRST and the query` |
|         - | 1878 | ` * only from what precedes it, so "#a?b" is a fragment of "a?b" with no query.` |
|         - | 1879 | ` */` |
|       278 | 1880 | `static void VmUrlParsePath(const char *z,int n,VmUrlParts *pOut)` |
|         3 | 1881 | `{` |
|       281 | 1882 | `	int i,iEnd = n;` |
|      1669 | 1883 | `	for( i = 0 ; i < n ; ++i ){` |
|      1443 | 1884 | `		if( z[i] == '#' ){` |
|        54 | 1885 | `			SyStringInitFromBuf(&pOut->sFragment,&z[i+1],(sxu32)(n - i - 1));` |
|        54 | 1886 | `			pOut->bFragment = 1;` |
|        54 | 1887 | `			iEnd = i;` |
|        54 | 1888 | `			break;` |
|         - | 1889 | `		}` |
|       697 | 1890 | `	}` |
|      1399 | 1891 | `	for( i = 0 ; i < iEnd ; ++i ){` |
|      1189 | 1892 | `		if( z[i] == '?' ){` |
|        70 | 1893 | `			SyStringInitFromBuf(&pOut->sQuery,&z[i+1],(sxu32)(iEnd - i - 1));` |
|        70 | 1894 | `			pOut->bQuery = 1;` |
|        70 | 1895 | `			iEnd = i;` |
|        70 | 1896 | `			break;` |
|         - | 1897 | `		}` |
|       562 | 1898 | `	}` |
|       281 | 1899 | `	if( iEnd > 0 ){` |
|       263 | 1900 | `		SyStringInitFromBuf(&pOut->sPath,z,(sxu32)iEnd);` |
|       263 | 1901 | `		pOut->bPath = 1;` |
|       130 | 1902 | `	}` |
|       281 | 1903 | `}` |
|         - | 1904 | `/* Parse "host[:port]" followed by an optional path/query/fragment. */` |
|       278 | 1905 | `static int VmUrlAuthorityThenPath(const char *z,int n,VmUrlParts *pOut,int bPortKnown)` |
|         2 | 1906 | `{` |
|       280 | 1907 | `	int i,iEnd = n;` |
|      2110 | 1908 | `	for( i = 0 ; i < n ; ++i ){` |
|      2020 | 1909 | `		if( z[i] == '/' \|\| z[i] == '?' \|\| z[i] == '#' ){` |
|       190 | 1910 | `			iEnd = i;` |
|       190 | 1911 | `			break;` |
|         - | 1912 | `		}` |
|       917 | 1913 | `	}` |
|       280 | 1914 | `	if( !VmUrlParseAuthority(z,iEnd,pOut,bPortKnown) ){` |
|        46 | 1915 | `		return 0;` |
|         - | 1916 | `	}` |
|       236 | 1917 | `	if( iEnd < n ){` |
|       170 | 1918 | `		VmUrlParsePath(&z[iEnd],n - iEnd,pOut);` |
|        84 | 1919 | `	}` |
|       236 | 1920 | `	return 1;` |
|       141 | 1921 | `}` |
|         - | 1922 | `/*` |
|         - | 1923 | ` * Resolve the port php took from the FIRST ':' of a host:port URL.` |
|         - | 1924 | ` *` |
|         - | 1925 | ` * php reads the port straight off that colon before it works out where the host` |
|         - | 1926 | ` * ends, so the digits can even belong to what becomes the path: parse_url()` |
|         - | 1927 | ` * reports port 1 for "a/:1", whose path is "/:1". Mirroring the order keeps` |
|         - | 1928 | ` * that quirk. Returns 0 for a port php rejects.` |
|         - | 1929 | ` */` |
|         6 | 1930 | `static int VmUrlPreparePort(const char *z,int k,int nEnd,VmUrlParts *pOut)` |
|         1 | 1931 | `{` |
|         7 | 1932 | `	int iPort = 0;` |
|         7 | 1933 | `	int rc = VmUrlParsePort(&z[k+1],nEnd - k - 1,&iPort);` |
|         7 | 1934 | `	if( rc < 0 ){` |
|       ! 0 | 1935 | `		return 0;` |
|         - | 1936 | `	}` |
|         7 | 1937 | `	if( rc > 0 ){` |
|         7 | 1938 | `		pOut->iPort = iPort;` |
|         7 | 1939 | `		pOut->bPort = 1;` |
|         3 | 1940 | `	}` |
|         7 | 1941 | `	return 1;` |
|         4 | 1942 | `}` |
|         - | 1943 | `/*` |
|         - | 1944 | ` * php's URL parser, as parse_url() needs it. Returns 0 where php returns FALSE.` |
|         - | 1945 | ` *` |
|         - | 1946 | ` * PH7_VmHttpSplitURI cannot serve here: it is an HTTP REQUEST-target splitter` |
|         - | 1947 | ` * (the HTTP server and filter_var share it) and assumes the input is` |
|         - | 1948 | ` * authority-first, so it read "a" as a host and "mailto:me@x.com" as` |
|         - | 1949 | ` * user:pass@host. php instead decides authority-vs-path up front: only a "//",` |
|         - | 1950 | ` * with or without a scheme before it, introduces an authority.` |
|         - | 1951 | ` */` |
|       392 | 1952 | `PH7_PRIVATE int PH7_VmUrlSplit(const char *z,int n,VmUrlParts *pOut)` |
|         3 | 1953 | `{` |
|       395 | 1954 | `	int i,k = -1,bScheme,bPortForm = 0,nPortEnd = 0;` |
|       395 | 1955 | `	SyZero(pOut,sizeof(VmUrlParts));` |
|         - | 1956 | `	/* A leading "//" settles it before any colon is considered: the authority` |
|         - | 1957 | `	 * starts after the slashes. Reading the colon first turned "//h:80" into a` |
|         - | 1958 | `	 * host called "//h" and "//[::1]" into a path. */` |
|       395 | 1959 | `	if( n >= 2 && z[0] == '/' && z[1] == '/' ){` |
|        24 | 1960 | `		return VmUrlAuthorityThenPath(&z[2],n - 2,pOut,0);` |
|         - | 1961 | `	}` |
|      1883 | 1962 | `	for( i = 0 ; i < n ; ++i ){` |
|      1841 | 1963 | `		if( z[i] == ':' ){` |
|       330 | 1964 | `			k = i;` |
|       330 | 1965 | `			break;` |
|         - | 1966 | `		}` |
|       758 | 1967 | `	}` |
|       373 | 1968 | `	if( k == 0 && n == 1 ){` |
|         - | 1969 | `		/* A lone ":" is an EMPTY scheme, which php rejects outright -- unlike` |
|         - | 1970 | `		 * ":a" or "::", which are simply paths. */` |
|         3 | 1971 | `		return 0;` |
|         - | 1972 | `	}` |
|       371 | 1973 | `	bScheme = k > 0;` |
|      1699 | 1974 | `	for( i = 0 ; bScheme && i < k ; ++i ){` |
|      1330 | 1975 | `		if( !VmUrlIsSchemeByte((unsigned char)z[i]) ){` |
|       ! 0 | 1976 | `			bScheme = 0;` |
|       ! 0 | 1977 | `		}` |
|       666 | 1978 | `	}` |
|       371 | 1979 | `	if( bScheme && k + 1 == n ){` |
|         - | 1980 | `		/* "x:" -- the scheme is the whole URL */` |
|         3 | 1981 | `		SyStringInitFromBuf(&pOut->sScheme,z,(sxu32)k);` |
|         3 | 1982 | `		pOut->bScheme = 1;` |
|         3 | 1983 | `		return 1;` |
|         - | 1984 | `	}` |
|         - | 1985 | `	/* Decide whether that ':' introduces a PORT rather than a scheme: at least` |
|         - | 1986 | `	 * one digit, running to the end of the string or to a '/'. That is what` |
|         - | 1987 | `	 * makes "a:80" a host:port while "a:80?q" is a scheme with path "80", and` |
|         - | 1988 | `	 * it holds however the ':' is reached -- ":1" and "/:1" are both authorities` |
|         - | 1989 | `	 * with an empty host, which php rejects. php caps the run, so a long number` |
|         - | 1990 | `	 * stays a path: "a:123456" is a path, "a:99999" an out-of-range port. */` |
|       369 | 1991 | `	if( k >= 0 ){` |
|       326 | 1992 | `		int p = k + 1;` |
|       326 | 1993 | `		int bBeforeQuery = 1;` |
|       326 | 1994 | `		nPortEnd = k + 1;` |
|         - | 1995 | `		/* A ':' that sits inside a query or fragment is just data: "?:1" is a` |
|         - | 1996 | `		 * query of ":1", not an authority with an empty host. */` |
|      1652 | 1997 | `		for( i = 0 ; i < k ; ++i ){` |
|      1328 | 1998 | `			if( z[i] == '?' \|\| z[i] == '#' ){` |
|       ! 0 | 1999 | `				bBeforeQuery = 0;` |
|       ! 0 | 2000 | `				break;` |
|         - | 2001 | `			}` |
|       665 | 2002 | `		}` |
|       336 | 2003 | `		while( p < n && z[p] >= '0' && z[p] <= '9' ){` |
|        11 | 2004 | `			p++;` |
|         1 | 2005 | `		}` |
|       326 | 2006 | `		if( bBeforeQuery && p > k + 1 && (p >= n \|\| z[p] == '/') && (p - k) < 7 ){` |
|         7 | 2007 | `			bPortForm = 1;` |
|         7 | 2008 | `			nPortEnd = p;` |
|         3 | 2009 | `		}` |
|       162 | 2010 | `	}` |
|       369 | 2011 | `	if( !bScheme ){` |
|        47 | 2012 | `		if( bPortForm ){` |
|         3 | 2013 | `			if( !VmUrlPreparePort(z,k,nPortEnd,pOut) ){` |
|       ! 0 | 2014 | `				return 0;` |
|         - | 2015 | `			}` |
|         3 | 2016 | `			return VmUrlAuthorityThenPath(z,n,pOut,1);` |
|         - | 2017 | `		}` |
|        45 | 2018 | `		VmUrlParsePath(z,n,pOut);` |
|        45 | 2019 | `		if( !pOut->bPath && !pOut->bQuery && !pOut->bFragment ){` |
|         - | 2020 | `			/* php reports the empty URL as an empty PATH, not as no components */` |
|         3 | 2021 | `			SyStringInitFromBuf(&pOut->sPath,z,0);` |
|         3 | 2022 | `			pOut->bPath = 1;` |
|         1 | 2023 | `		}` |
|        45 | 2024 | `		return 1;` |
|         - | 2025 | `	}` |
|       324 | 2026 | `	if( bPortForm ){` |
|         5 | 2027 | `		if( !VmUrlPreparePort(z,k,nPortEnd,pOut) ){` |
|       ! 0 | 2028 | `			return 0;` |
|         - | 2029 | `		}` |
|         5 | 2030 | `		return VmUrlAuthorityThenPath(z,n,pOut,1);` |
|         - | 2031 | `	}` |
|       320 | 2032 | `	SyStringInitFromBuf(&pOut->sScheme,z,(sxu32)k);` |
|       320 | 2033 | `	pOut->bScheme = 1;` |
|       320 | 2034 | `	if( z[k+1] == '/' && k + 2 < n && z[k+2] == '/' ){` |
|       262 | 2035 | `		if( k + 3 < n && z[k+3] == '/' && pOut->sScheme.nByte == 4` |
|        20 | 2036 | `		 && (z[0]=='f'\|\|z[0]=='F') && (z[1]=='i'\|\|z[1]=='I')` |
|        14 | 2037 | `		 && (z[2]=='l'\|\|z[2]=='L') && (z[3]=='e'\|\|z[3]=='E') ){` |
|         - | 2038 | `			/* file:/// has no authority: the path starts at the third slash,` |
|         - | 2039 | `			 * except that a "c:" drive letter swallows it (file:///c:/x is the` |
|         - | 2040 | `			 * path "c:/x"). A '\|' in place of the ':' does NOT count. */` |
|        14 | 2041 | `			int iBase = k + 3;` |
|        14 | 2042 | `			if( iBase + 2 < n && VmUrlIsAlpha((unsigned char)z[iBase+1]) && z[iBase+2] == ':' ){` |
|       ! 0 | 2043 | `				iBase++;` |
|       ! 0 | 2044 | `			}` |
|        14 | 2045 | `			VmUrlParsePath(&z[iBase],n - iBase,pOut);` |
|        14 | 2046 | `			return 1;` |
|         - | 2047 | `		}` |
|       252 | 2048 | `		return VmUrlAuthorityThenPath(&z[k+3],n - k - 3,pOut,0);` |
|         - | 2049 | `	}` |
|         - | 2050 | `	/* "mailto:me@x.com", "x:y", "http:/x": everything after the ':' is a path */` |
|        58 | 2051 | `	VmUrlParsePath(&z[k+1],n - k - 1,pOut);` |
|        58 | 2052 | `	return 1;` |
|       199 | 2053 | `}` |
|         - | 2054 | `/*` |
|         - | 2055 | ` * Emit one parsed component into pValue, replacing control bytes with '_'.` |
|         - | 2056 | ` *` |
|         - | 2057 | ` * php runs php_replace_controlchars_ex over every string component it returns,` |
|         - | 2058 | ` * so a URL carrying a raw NUL, newline or DEL cannot smuggle it through into` |
|         - | 2059 | ` * whatever the caller splices the component into (a header, a log line, a` |
|         - | 2060 | ` * redirect). Bytes >= 0x80 are deliberately left alone -- php only folds the` |
|         - | 2061 | ` * ASCII control range.` |
|         - | 2062 | ` */` |
|       164 | 2063 | `static void VmUrlSetComponent(ph7_value *pValue,const SyString *pComp)` |
|         1 | 2064 | `{` |
|       165 | 2065 | `	const char *z = pComp->zString;` |
|       165 | 2066 | `	sxu32 n = pComp->nByte,i,iRun = 0;` |
|       165 | 2067 | `	if( n < 1 \|\| z == 0 ){` |
|         3 | 2068 | `		ph7_value_string(pValue,"",0);` |
|         3 | 2069 | `		return;` |
|         - | 2070 | `	}` |
|       955 | 2071 | `	for( i = 0 ; i < n ; ++i ){` |
|       793 | 2072 | `		unsigned char c = (unsigned char)z[i];` |
|       793 | 2073 | `		if( c < 0x20 \|\| c == 0x7f ){` |
|         3 | 2074 | `			if( i > iRun ){` |
|         3 | 2075 | `				ph7_value_string(pValue,&z[iRun],(int)(i - iRun));` |
|         1 | 2076 | `			}` |
|         3 | 2077 | `			ph7_value_string(pValue,"_",1);` |
|         3 | 2078 | `			iRun = i + 1;` |
|         1 | 2079 | `		}` |
|       397 | 2080 | `	}` |
|       163 | 2081 | `	if( n > iRun ){` |
|       163 | 2082 | `		ph7_value_string(pValue,&z[iRun],(int)(n - iRun));` |
|        81 | 2083 | `	}` |
|        83 | 2084 | `}` |
|       104 | 2085 | `PH7_PRIVATE int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2086 | `{` |
|         - | 2087 | `	const char *zStr; /* Input string */` |
|         - | 2088 | `	VmUrlParts sUrl;  /* Parse of the given URI */` |
|         - | 2089 | `	SyString *pComp;` |
|         - | 2090 | `	int bHave;` |
|         - | 2091 | `	int nLen;` |
|       105 | 2092 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|         - | 2093 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2094 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2095 | `		return PH7_OK;` |
|         - | 2096 | `	}` |
|         - | 2097 | `	/* Extract the given URI. An empty string is NOT a failure: php parses it as` |
|         - | 2098 | `	 * an empty path. */` |
|       105 | 2099 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       105 | 2100 | `	if( nLen < 0 ){` |
|       ! 0 | 2101 | `		nLen = 0;` |
|       ! 0 | 2102 | `	}` |
|       105 | 2103 | `	if( !PH7_VmUrlSplit(zStr,nLen,&sUrl) ){` |
|         - | 2104 | `		/* Malformed input,return FALSE */` |
|        13 | 2105 | `		ph7_result_bool(pCtx,0);` |
|        13 | 2106 | `		return PH7_OK;` |
|         - | 2107 | `	}` |
|       103 | 2108 | `	if( nArg > 1 && ph7_value_to_int(apArg[1]) >= 0 ){` |
|         - | 2109 | `		/* Refer to constant.c for constants values (php's PHP_URL_* are 0-based;` |
|         - | 2110 | `		 * PHL used to number them from 1, so every literal component id selected` |
|         - | 2111 | `		 * the WRONG field -- and the constants' own tests only asserted "%d",` |
|         - | 2112 | `		 * which any numbering satisfies). A negative id means "the whole array",` |
|         - | 2113 | `		 * which is what the default $component = -1 relies on. */` |
|        27 | 2114 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        27 | 2115 | `		pComp = 0;` |
|        27 | 2116 | `		bHave = 0;` |
|        27 | 2117 | `		switch(nComponent){` |
|         3 | 2118 | `		case 0: /* PHP_URL_SCHEME */   pComp = &sUrl.sScheme;   bHave = sUrl.bScheme;   break;` |
|         5 | 2119 | `		case 1: /* PHP_URL_HOST */     pComp = &sUrl.sHost;     bHave = sUrl.bHost;     break;` |
|         2 | 2120 | `		case 2: /* PHP_URL_PORT */` |
|         5 | 2121 | `			if( sUrl.bPort ){` |
|         5 | 2122 | `				ph7_result_int(pCtx,sUrl.iPort);` |
|         3 | 2123 | `			}else{` |
|       ! 0 | 2124 | `				ph7_result_null(pCtx);` |
|         - | 2125 | `			}` |
|         5 | 2126 | `			return PH7_OK;` |
|         3 | 2127 | `		case 3: /* PHP_URL_USER */     pComp = &sUrl.sUser;     bHave = sUrl.bUser;     break;` |
|         3 | 2128 | `		case 4: /* PHP_URL_PASS */     pComp = &sUrl.sPass;     bHave = sUrl.bPass;     break;` |
|         3 | 2129 | `		case 5: /* PHP_URL_PATH */     pComp = &sUrl.sPath;     bHave = sUrl.bPath;     break;` |
|         5 | 2130 | `		case 6: /* PHP_URL_QUERY */    pComp = &sUrl.sQuery;    bHave = sUrl.bQuery;    break;` |
|         5 | 2131 | `		case 7: /* PHP_URL_FRAGMENT */ pComp = &sUrl.sFragment; bHave = sUrl.bFragment; break;` |
|         1 | 2132 | `		default:` |
|         4 | 2133 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 2134 | `				"parse_url(): Argument #2 ($component) must be a valid URL component identifier, %d given",` |
|         1 | 2135 | `				nComponent);` |
|         - | 2136 | `		}` |
|        21 | 2137 | `		if( bHave ){` |
|        19 | 2138 | `			ph7_value *pOut = ph7_context_new_scalar(pCtx);` |
|        19 | 2139 | `			if( pOut == 0 ){` |
|       ! 0 | 2140 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|       ! 0 | 2141 | `				ph7_result_bool(pCtx,0);` |
|       ! 0 | 2142 | `				return PH7_OK;` |
|         - | 2143 | `			}` |
|        19 | 2144 | `			VmUrlSetComponent(pOut,pComp);` |
|        19 | 2145 | `			ph7_result_value(pCtx,pOut);` |
|        10 | 2146 | `		}else{` |
|         - | 2147 | `			/* No available value,return NULL */` |
|         3 | 2148 | `			ph7_result_null(pCtx);` |
|         - | 2149 | `		}` |
|        11 | 2150 | `	}else{` |
|         - | 2151 | `		ph7_value *pArray,*pValue;` |
|         - | 2152 | `		/* Return an associative array */` |
|        67 | 2153 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|        67 | 2154 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|        67 | 2155 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|         - | 2156 | `			/* Out of memory */` |
|       ! 0 | 2157 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|         - | 2158 | `			/* Return false */` |
|       ! 0 | 2159 | `			ph7_result_bool(pCtx,0);` |
|       ! 0 | 2160 | `			return PH7_OK;` |
|         - | 2161 | `		}` |
|         - | 2162 | `		/* Fill the array, in php's key order. A component is emitted whenever it` |
|         - | 2163 | `		 * is PRESENT, even when empty -- parse_url("?") is ['query'=>'']. */` |
|        67 | 2164 | `		if( sUrl.bScheme ){` |
|        33 | 2165 | `			VmUrlSetComponent(pValue,&sUrl.sScheme);` |
|        33 | 2166 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        33 | 2167 | `			ph7_value_reset_string_cursor(pValue);` |
|        16 | 2168 | `		}` |
|        67 | 2169 | `		if( sUrl.bHost ){` |
|        31 | 2170 | `			VmUrlSetComponent(pValue,&sUrl.sHost);` |
|        31 | 2171 | `			ph7_array_add_strkey_elem(pArray,"host",pValue);` |
|        31 | 2172 | `			ph7_value_reset_string_cursor(pValue);` |
|        15 | 2173 | `		}` |
|        67 | 2174 | `		if( sUrl.bPort ){` |
|        17 | 2175 | `			ph7_value_int(pValue,sUrl.iPort);` |
|        17 | 2176 | `			ph7_array_add_strkey_elem(pArray,"port",pValue);` |
|        17 | 2177 | `			ph7_value_reset_string_cursor(pValue);` |
|         8 | 2178 | `		}` |
|        67 | 2179 | `		if( sUrl.bUser ){` |
|         9 | 2180 | `			VmUrlSetComponent(pValue,&sUrl.sUser);` |
|         9 | 2181 | `			ph7_array_add_strkey_elem(pArray,"user",pValue);` |
|         9 | 2182 | `			ph7_value_reset_string_cursor(pValue);` |
|         4 | 2183 | `		}` |
|        67 | 2184 | `		if( sUrl.bPass ){` |
|         7 | 2185 | `			VmUrlSetComponent(pValue,&sUrl.sPass);` |
|         7 | 2186 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue);` |
|         7 | 2187 | `			ph7_value_reset_string_cursor(pValue);` |
|         3 | 2188 | `		}` |
|        67 | 2189 | `		if( sUrl.bPath ){` |
|        47 | 2190 | `			VmUrlSetComponent(pValue,&sUrl.sPath);` |
|        47 | 2191 | `			ph7_array_add_strkey_elem(pArray,"path",pValue);` |
|        47 | 2192 | `			ph7_value_reset_string_cursor(pValue);` |
|        23 | 2193 | `		}` |
|        67 | 2194 | `		if( sUrl.bQuery ){` |
|        13 | 2195 | `			VmUrlSetComponent(pValue,&sUrl.sQuery);` |
|        13 | 2196 | `			ph7_array_add_strkey_elem(pArray,"query",pValue);` |
|        13 | 2197 | `			ph7_value_reset_string_cursor(pValue);` |
|         6 | 2198 | `		}` |
|        67 | 2199 | `		if( sUrl.bFragment ){` |
|        13 | 2200 | `			VmUrlSetComponent(pValue,&sUrl.sFragment);` |
|        13 | 2201 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue);` |
|         6 | 2202 | `		}` |
|         - | 2203 | `		/* Return the created array */` |
|        67 | 2204 | `		ph7_result_value(pCtx,pArray);` |
|         - | 2205 | `		/* NOTE:` |
|         - | 2206 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|         - | 2207 | `		 * automatically as soon we return from this function.` |
|         - | 2208 | `		 */` |
|         - | 2209 | `	}` |
|         - | 2210 | `	/* All done */` |
|        87 | 2211 | `	return PH7_OK;` |
|        53 | 2212 | `}` |
|         - | 2213 |  |
|         - | 2214 | `/*` |
|         - | 2215 | ` * Section:` |
|         - | 2216 | ` *   Array related routines.` |
|         - | 2217 | ` * Status:` |
|         - | 2218 | ` *    Stable.` |
|         - | 2219 | ` * Note 2012-5-21 01:04:15:` |
|         - | 2220 | ` *  Array related functions that need access to the underlying` |
|         - | 2221 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|         - | 2222 | ` */` |
|         - | 2223 | `/*` |
|         - | 2224 | ` * The [compact()] function store it's state information in an instance` |
|         - | 2225 | ` * of the following structure.` |
|         - | 2226 | ` */` |
|         - | 2227 | `struct compact_data` |
|         - | 2228 | `{` |
|         - | 2229 | `	ph7_value *pArray;  /* Target array */` |
|         - | 2230 | `	int nRecCount;      /* Recursion count */` |
|         - | 2231 | `	ph7_context *pCtx;  /* Call context, for php's two warnings */` |
|         - | 2232 | `	int iArg;           /* 1-based ARGUMENT this element came from: php names the` |
|         - | 2233 | `	                     * argument even for an element found inside a nested` |
|         - | 2234 | `	                     * array, never the element's own position. */` |
|         - | 2235 | `};` |
|         - | 2236 | `/*` |
|         - | 2237 | ` * php's two compact() diagnostics, both E_WARNING and both non-fatal (the entry` |
|         - | 2238 | ` * is skipped and the rest of the call proceeds): a name that is neither a string` |
|         - | 2239 | ` * nor an array of strings, and a string naming a variable the frame does not` |
|         - | 2240 | ` * have. PHL emitted neither, so compact(1) and compact('typo') answered a` |
|         - | 2241 | ` * shorter array with nothing said -- the caller's only clue that a name was` |
|         - | 2242 | ` * dropped was the array's own size.` |
|         - | 2243 | ` */` |
|        14 | 2244 | `static void VmCompactBadName(ph7_context *pCtx,int iArg,ph7_value *pValue)` |
|         1 | 2245 | `{` |
|         - | 2246 | `	char zGiven[64];` |
|        22 | 2247 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 2248 | `		"Argument #%d must be string or array of strings, %s given",` |
|         7 | 2249 | `		iArg,VmValueGivenName(pValue,zGiven,sizeof(zGiven)));` |
|        15 | 2250 | `}` |
|         8 | 2251 | `static void VmCompactUndefined(ph7_context *pCtx,SyString *pVar)` |
|         1 | 2252 | `{` |
|         8 | 2253 | `	if( pVar->nByte == sizeof("this")-1` |
|         6 | 2254 | `	 && SyMemcmp(pVar->zString,"this",sizeof("this")-1) == 0 ){` |
|         - | 2255 | ``		/* php's one silent miss here: `compact('this')` outside an object context`` |
|         - | 2256 | ``		 * skips the name and says nothing -- `$this` is not a variable, so it is`` |
|         - | 2257 | `		 * not an undefined one either, and compact() is the one door that neither` |
|         - | 2258 | `		 * warns nor throws for it. */` |
|         3 | 2259 | `		return;` |
|         - | 2260 | `	}` |
|        10 | 2261 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         6 | 2262 | `		"Undefined variable $%.*s",(int)pVar->nByte,pVar->zString);` |
|         5 | 2263 | `}` |
|         - | 2264 | `/*` |
|         - | 2265 | ` * Walker callback for the [compact()] function defined below.` |
|         - | 2266 | ` */` |
|        16 | 2267 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|         1 | 2268 | `{` |
|        17 | 2269 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|        17 | 2270 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|        17 | 2271 | `	ph7_vm *pVm = pArray->pVm;` |
|         - | 2272 | `	/* Act according to the hashmap value */` |
|        17 | 2273 | `	if( ph7_value_is_string(pValue) ){` |
|         - | 2274 | `		SyString sVar;` |
|         9 | 2275 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|         - | 2276 | `		/* Query the current frame. An EMPTY name is looked up like any other:` |
|         - | 2277 | `		 * php reports it as "Undefined variable $" rather than skipping it. */` |
|         9 | 2278 | `		pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|         - | 2279 | `		/* ^` |
|         - | 2280 | `		 * \| Avoid wasting variable and use 'pKey' instead` |
|         - | 2281 | `		 */` |
|         9 | 2282 | `		if( pKey ){` |
|         - | 2283 | `			/* Perform the insertion */` |
|         7 | 2284 | `			ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|         4 | 2285 | `		}else{` |
|         3 | 2286 | `			VmCompactUndefined(pData->pCtx,&sVar);` |
|         1 | 2287 | `		}` |
|        13 | 2288 | `	}else if( ph7_value_is_array(pValue) ){` |
|         - | 2289 | `		/* Recursively traverse this array. Past the depth cap the element is` |
|         - | 2290 | `		 * dropped in silence, as it always was: the cap is PHL's own guard and` |
|         - | 2291 | `		 * the "must be string or array of strings" warning would be a lie about` |
|         - | 2292 | `		 * an argument that IS an array of strings. */` |
|         7 | 2293 | `		if( pData->nRecCount < 32 ){` |
|         - | 2294 | `			int rc;` |
|         7 | 2295 | `			pData->nRecCount++;` |
|         7 | 2296 | `			rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|         7 | 2297 | `			pData->nRecCount--;` |
|         7 | 2298 | `			return rc;` |
|         - | 2299 | `		}` |
|       ! 0 | 2300 | `	}else{` |
|         3 | 2301 | `		VmCompactBadName(pData->pCtx,pData->iArg,pValue);` |
|         - | 2302 | `	}` |
|        11 | 2303 | `	return SXRET_OK;` |
|         9 | 2304 | `}` |
|         - | 2305 | `/*` |
|         - | 2306 | ` * array compact(mixed $varname [, mixed $... ])` |
|         - | 2307 | ` *  Create array containing variables and their values.` |
|         - | 2308 | ` *  For each of these, compact() looks for a variable with that name` |
|         - | 2309 | ` *  in the current symbol table and adds it to the output array such` |
|         - | 2310 | ` *  that the variable name becomes the key and the contents of the variable` |
|         - | 2311 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|         - | 2312 | ` *  Any strings that are not set will simply be skipped.` |
|         - | 2313 | ` * Parameters` |
|         - | 2314 | ` *  $varname` |
|         - | 2315 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|         - | 2316 | ` *   a string containing the name of the variable, or an array of variable names.` |
|         - | 2317 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|         - | 2318 | ` *   it recursively.` |
|         - | 2319 | ` * Return` |
|         - | 2320 | ` *  The output array with all the variables added to it or NULL on failure` |
|         - | 2321 | ` */` |
|        28 | 2322 | `PH7_PRIVATE int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2323 | `{` |
|         - | 2324 | `	ph7_value *pArray,*pObj;` |
|        29 | 2325 | `	ph7_vm *pVm = pCtx->pVm;` |
|         - | 2326 | `	const char *zName;` |
|         - | 2327 | `	SyString sVar;` |
|         - | 2328 | `	int i,nLen;` |
|        29 | 2329 | `	if( nArg < 1 ){` |
|         - | 2330 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 2331 | `		ph7_result_null(pCtx);` |
|       ! 0 | 2332 | `		return PH7_OK;` |
|         - | 2333 | `	}` |
|         - | 2334 | `	/* Create the array */` |
|        29 | 2335 | `	pArray = ph7_context_new_array(pCtx);` |
|        29 | 2336 | `	if( pArray == 0 ){` |
|         - | 2337 | `		/* Out of memory */` |
|       ! 0 | 2338 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|         - | 2339 | `		/* Return NULL */` |
|       ! 0 | 2340 | `		ph7_result_null(pCtx);` |
|       ! 0 | 2341 | `		return PH7_OK;` |
|         - | 2342 | `	}` |
|         - | 2343 | `	/* Perform the requested operation */` |
|        69 | 2344 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        41 | 2345 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|        19 | 2346 | `			if( ph7_value_is_array(apArg[i]) ){` |
|         - | 2347 | `				struct compact_data sData;` |
|         7 | 2348 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 2349 | `				/* Recursively walk the array */` |
|         7 | 2350 | `				sData.nRecCount = 0;` |
|         7 | 2351 | `				sData.pArray = pArray;` |
|         7 | 2352 | `				sData.pCtx = pCtx;` |
|         7 | 2353 | `				sData.iArg = i + 1;` |
|         7 | 2354 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|         4 | 2355 | `			}else{` |
|        13 | 2356 | `				VmCompactBadName(pCtx,i + 1,apArg[i]);` |
|         - | 2357 | `			}` |
|        10 | 2358 | `		}else{` |
|         - | 2359 | `			/* Extract variable name. An EMPTY one is looked up like any other:` |
|         - | 2360 | `			 * php reports it as "Undefined variable $" rather than skipping it. */` |
|        23 | 2361 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        23 | 2362 | `			SyStringInitFromBuf(&sVar,zName,nLen);` |
|         - | 2363 | `			/* Check if the variable is available in the current frame */` |
|        23 | 2364 | `			pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        23 | 2365 | `			if( pObj ){` |
|        17 | 2366 | `				ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|         9 | 2367 | `			}else{` |
|         7 | 2368 | `				VmCompactUndefined(pCtx,&sVar);` |
|         - | 2369 | `			}` |
|         - | 2370 | `		}` |
|        21 | 2371 | `	}` |
|         - | 2372 | `	/* Return the array */` |
|        29 | 2373 | `	ph7_result_value(pCtx,pArray);` |
|        29 | 2374 | `	return PH7_OK;` |
|        15 | 2375 | `}` |
|         - | 2376 | `/*` |
|         - | 2377 | ` * The [import_request_variables()] function store it's state information` |
|         - | 2378 | ` * in an instance of the following structure.` |
|         - | 2379 | ` */` |
|         - | 2380 | `typedef struct extract_aux_data extract_aux_data;` |
|         - | 2381 | `struct extract_aux_data` |
|         - | 2382 | `{` |
|         - | 2383 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|         - | 2384 | `	int iCount;           /* Number of variables successfully imported  */` |
|         - | 2385 | `	const char *zPrefix;  /* Prefix name */` |
|         - | 2386 | `	int Prefixlen;        /* Prefix  length */` |
|         - | 2387 | `	char zWorker[1024];   /* Working buffer */` |
|         - | 2388 | `};` |
|         - | 2389 | `/*` |
|         - | 2390 | ` * php's php_valid_var_name(): a legal PHP variable name matches` |
|         - | 2391 | ` * [A-Za-z_\x80-\xff][A-Za-z0-9_\x80-\xff]* . Byte-wise and locale-free on` |
|         - | 2392 | ` * purpose (php has been locale-independent here since 8.0); the high-byte` |
|         - | 2393 | ` * range is what lets UTF-8 identifiers through. extract() drops every key` |
|         - | 2394 | ` * that does not pass, instead of installing an unreachable variable.` |
|         - | 2395 | ` */` |
|       164 | 2396 | `static int VmIsValidVarName(const char *zName,sxu32 nByte)` |
|         4 | 2397 | `{` |
|         - | 2398 | `	unsigned char c;` |
|         - | 2399 | `	sxu32 i;` |
|       168 | 2400 | `	if( nByte < 1 ){` |
|         7 | 2401 | `		return FALSE;` |
|         - | 2402 | `	}` |
|       162 | 2403 | `	c = (unsigned char)zName[0];` |
|       162 | 2404 | `	if( c != '_' && !(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') && c < 0x80 ){` |
|        11 | 2405 | `		return FALSE;` |
|         - | 2406 | `	}` |
|       394 | 2407 | `	for( i = 1 ; i < nByte ; ++i ){` |
|       263 | 2408 | `		c = (unsigned char)zName[i];` |
|       260 | 2409 | `		if( c != '_' && !(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z')` |
|        80 | 2410 | `		 && !(c >= '0' && c <= '9') && c < 0x80 ){` |
|        20 | 2411 | `			return FALSE;` |
|         - | 2412 | `		}` |
|       124 | 2413 | `	}` |
|       134 | 2414 | `	return TRUE;` |
|        86 | 2415 | `}` |
|         - | 2416 | `/* TRUE when the name is exactly "this": php refuses to re-assign $this. */` |
|       172 | 2417 | `static int VmExtractIsThis(const char *zName,sxu32 nByte)` |
|         4 | 2418 | `{` |
|       176 | 2419 | `	return nByte == sizeof("this")-1 && SyMemcmp(zName,"this",sizeof("this")-1) == 0;` |
|         4 | 2420 | `}` |
|         - | 2421 | `/*` |
|         - | 2422 | ` * TRUE when the name belongs to the superglobal table ($GLOBALS, $_SERVER,` |
|         - | 2423 | ` * $_GET, …). php hands extract() a per-frame symbol table that holds no` |
|         - | 2424 | ` * superglobal, so such a key lands in the LOCAL table and the real superglobal` |
|         - | 2425 | ` * is untouched. In PHL the name resolves to the superglobal SLOT itself` |
|         - | 2426 | ` * (VmExtractMemObj consults hSuper first), so a plain store would replace` |
|         - | 2427 | ` * $GLOBALS/$_SERVER with the imported value and take the whole symbol table /` |
|         - | 2428 | ` * request environment with it. Those keys are dropped instead — a prefixed` |
|         - | 2429 | ` * name ($p__SERVER) is a normal local and stores fine.` |
|         - | 2430 | ` */` |
|       120 | 2431 | `static int VmExtractIsProtected(ph7_vm *pVm,const char *zName,sxu32 nByte)` |
|         4 | 2432 | `{` |
|       124 | 2433 | `	return PH7_VmSuperGet(&(*pVm),zName,nByte) != 0;` |
|         4 | 2434 | `}` |
|         - | 2435 | `/*` |
|         - | 2436 | ` * TRUE when the calling frame already holds this variable name.` |
|         - | 2437 | ` * "this" and the superglobals always answer FALSE, matching the symbol table` |
|         - | 2438 | ` * php hands extract(): $this is bound implicitly and superglobals live outside` |
|         - | 2439 | ` * the frame. So EXTR_IF_EXISTS/EXTR_PREFIX_IF_EXISTS skip those keys (php-exact)` |
|         - | 2440 | ` * and EXTR_PREFIX_SAME takes its not-a-collision branch.` |
|         - | 2441 | ` */` |
|        72 | 2442 | `static int VmExtractVarExists(ph7_vm *pVm,const char *zName,sxu32 nByte)` |
|         4 | 2443 | `{` |
|         - | 2444 | `	SyString sVar;` |
|        76 | 2445 | `	if( VmExtractIsThis(zName,nByte) \|\| VmExtractIsProtected(pVm,zName,nByte) ){` |
|        20 | 2446 | `		return FALSE;` |
|         - | 2447 | `	}` |
|        57 | 2448 | `	SyStringInitFromBuf(&sVar,zName,nByte);` |
|        57 | 2449 | `	return VmExtractMemObj(pVm,&sVar,FALSE,FALSE) != 0;` |
|        40 | 2450 | `}` |
|         - | 2451 | `/*` |
|         - | 2452 | ` * Create-or-overwrite a variable of the calling frame with a copy of pValue.` |
|         - | 2453 | ` * Returns TRUE when the variable was written (php counts exactly those).` |
|         - | 2454 | ` */` |
|         - | 2455 | `/*` |
|         - | 2456 | ` * EXTR_REFS: bind the imported NAME to the array element's own slot instead of copying` |
|         - | 2457 | ` * the value, so a later write through the variable reaches the caller's array — php's` |
|         - | 2458 | ` * by-reference extraction. The binding is registered (PH7_VmBindVarSlot), which is what` |
|         - | 2459 | ` * makes the element count the new name as a holder and read as a reference.` |
|         - | 2460 | ` *` |
|         - | 2461 | ` * The name is DUPLICATED: the symbol table keeps the pointer it is given, and the caller's` |
|         - | 2462 | ` * is a scratch blob the next entry reuses.` |
|         - | 2463 | ` */` |
|         8 | 2464 | `static int VmExtractBindVar(ph7_vm *pVm,const char *zName,sxu32 nByte,sxu32 nIdx)` |
|         1 | 2465 | `{` |
|         9 | 2466 | `	VmFrame *pFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|         - | 2467 | `	char *zDup;` |
|         9 | 2468 | `	if( nIdx == SXU32_HIGH ){` |
|       ! 0 | 2469 | `		return FALSE;` |
|         - | 2470 | `	}` |
|         9 | 2471 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zName,nByte);` |
|         9 | 2472 | `	if( zDup == 0 ){` |
|       ! 0 | 2473 | `		return FALSE;` |
|         - | 2474 | `	}` |
|         9 | 2475 | `	PH7_VmBindVarSlot(&(*pVm),pFrame,zDup,nByte,nIdx);` |
|         9 | 2476 | `	return TRUE;` |
|         5 | 2477 | `}` |
|        68 | 2478 | `static int VmExtractStoreVar(ph7_vm *pVm,const char *zName,sxu32 nByte,ph7_value *pValue)` |
|         4 | 2479 | `{` |
|         - | 2480 | `	ph7_value *pObj;` |
|         - | 2481 | `	SyString sVar;` |
|        72 | 2482 | `	SyStringInitFromBuf(&sVar,zName,nByte);` |
|         - | 2483 | `	/* bDup: the name lives in a scratch blob that is reused by the next entry */` |
|        72 | 2484 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        72 | 2485 | `	if( pObj == 0 ){` |
|       ! 0 | 2486 | `		return FALSE;` |
|         - | 2487 | `	}` |
|        72 | 2488 | `	PH7_MemObjStore(pValue,pObj);` |
|        72 | 2489 | `	return TRUE;` |
|        38 | 2490 | `}` |
|         - | 2491 | `/*` |
|         - | 2492 | ` * Build php's prefixed name "<prefix>_<key>" (php_prefix_varname with` |
|         - | 2493 | ` * add_underscore): the separator is unconditional, so an empty prefix still` |
|         - | 2494 | ` * yields "_key" exactly like php.` |
|         - | 2495 | ` */` |
|        40 | 2496 | `static sxi32 VmExtractPrefixName(SyBlob *pOut,const char *zPrefix,int nPrefix,` |
|         - | 2497 | `	const char *zKey,sxu32 nKey)` |
|         3 | 2498 | `{` |
|        43 | 2499 | `	SyBlobReset(pOut);` |
|        43 | 2500 | `	if( nPrefix > 0 && SyBlobAppend(pOut,zPrefix,(sxu32)nPrefix) != SXRET_OK ){` |
|       ! 0 | 2501 | `		return SXERR_MEM;` |
|         - | 2502 | `	}` |
|        43 | 2503 | `	if( SyBlobAppend(pOut,"_",sizeof(char)) != SXRET_OK ){` |
|       ! 0 | 2504 | `		return SXERR_MEM;` |
|         - | 2505 | `	}` |
|        43 | 2506 | `	if( nKey > 0 && SyBlobAppend(pOut,zKey,nKey) != SXRET_OK ){` |
|       ! 0 | 2507 | `		return SXERR_MEM;` |
|         - | 2508 | `	}` |
|        43 | 2509 | `	return SXRET_OK;` |
|        23 | 2510 | `}` |
|         - | 2511 | `/* What to do with one array entry, decided by the extract mode. */` |
|         - | 2512 | `#define VM_EXTRACT_DROP     0 /* php skips this key entirely */` |
|         - | 2513 | `#define VM_EXTRACT_PLAIN    1 /* install under the key itself */` |
|         - | 2514 | `#define VM_EXTRACT_PREFIX   2 /* install under "<prefix>_<key>" */` |
|         - | 2515 | `/*` |
|         - | 2516 | ` * int extract(array &$array[,int $flags = EXTR_OVERWRITE[,string $prefix = "" ]])` |
|         - | 2517 | ` *   Import variables into the current symbol table from an array.` |
|         - | 2518 | ` *` |
|         - | 2519 | ` * $flags is php's ENUM (PH7_EXTR_* in ph7int.h), not a bitmask: the mode is` |
|         - | 2520 | ` * (flags & 0xff) and anything above EXTR_IF_EXISTS(6) is a ValueError. The` |
|         - | 2521 | ` * modes, mirroring php's per-mode helpers in ext/standard/array.c:` |
|         - | 2522 | ` *   EXTR_OVERWRITE(0)        collisions overwrite` |
|         - | 2523 | ` *   EXTR_SKIP(1)             collisions keep the existing variable` |
|         - | 2524 | ` *   EXTR_PREFIX_SAME(2)      collisions install "<prefix>_<key>"` |
|         - | 2525 | ` *   EXTR_PREFIX_ALL(3)       every key installs as "<prefix>_<key>"` |
|         - | 2526 | ` *   EXTR_PREFIX_INVALID(4)   only illegal names (and numeric keys) get prefixed` |
|         - | 2527 | ` *   EXTR_PREFIX_IF_EXISTS(5) install "<prefix>_<key>" only if <key> exists` |
|         - | 2528 | ` *   EXTR_IF_EXISTS(6)        overwrite only variables that already exist` |
|         - | 2529 | `` * Modes 2..5 require $prefix (php: `is required when using this extract type`),`` |
|         - | 2530 | ` * a non-empty $prefix must itself be a legal identifier, and a key whose final` |
|         - | 2531 | ` * name is not a legal variable name is dropped rather than installed. Only` |
|         - | 2532 | ` * EXTR_PREFIX_ALL/EXTR_PREFIX_INVALID look at numeric keys at all.` |
|         - | 2533 | ` *` |
|         - | 2534 | `` * $this is never a target: php throws `Cannot re-assign $this` where a store`` |
|         - | 2535 | ` * would land on it, and skips it where a store would not (EXTR_SKIP), and` |
|         - | 2536 | ` * $GLOBALS is never clobbered.` |
|         - | 2537 | ` * Return` |
|         - | 2538 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|         - | 2539 | ` */` |
|       104 | 2540 | `PH7_PRIVATE int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 2541 | `{` |
|       108 | 2542 | `	ph7_vm *pVm = pCtx->pVm;` |
|         - | 2543 | `	ph7_hashmap_node *pEntry;` |
|         - | 2544 | `	ph7_hashmap *pMap;` |
|       108 | 2545 | `	const char *zPrefix = 0;` |
|       108 | 2546 | `	sxi64 iFlags = PH7_EXTR_OVERWRITE;` |
|       108 | 2547 | `	sxi64 iCount = 0;` |
|         - | 2548 | `	ph7_value sValue;` |
|         - | 2549 | `	SyBlob sWorker;` |
|       108 | 2550 | `	int nPrefix = 0;` |
|       108 | 2551 | `	sxi32 rc = PH7_OK;` |
|         - | 2552 | `	int iType;` |
|         - | 2553 | `	sxu32 n;` |
|       108 | 2554 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 2555 | `		char zBuf[64];` |
|       ! 0 | 2556 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 2557 | `			"extract(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 2558 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|         - | 2559 | `	}` |
|       108 | 2560 | `	if( nArg > 1 ){` |
|        98 | 2561 | `		rc = PH7_IntArgResolve(pCtx,apArg[1],"extract",2,"$flags","int",&iFlags);` |
|        98 | 2562 | `		if( rc != PH7_OK ){` |
|       ! 0 | 2563 | `			return rc;` |
|         - | 2564 | `		}` |
|        47 | 2565 | `	}` |
|         - | 2566 | `	/* php: the mode is the low byte; EXTR_REFS(0x100) rides above it */` |
|       108 | 2567 | `	iType = (int)(iFlags & 0xff);` |
|       108 | 2568 | `	if( iType > PH7_EXTR_IF_EXISTS ){` |
|         7 | 2569 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 2570 | `			"extract(): Argument #2 ($flags) must be a valid extract type");` |
|         - | 2571 | `	}` |
|       102 | 2572 | `	if( iType > PH7_EXTR_SKIP && iType <= PH7_EXTR_PREFIX_IF_EXISTS && nArg < 3 ){` |
|        12 | 2573 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 2574 | `			"extract(): Argument #3 ($prefix) is required when using this extract type");` |
|         - | 2575 | `	}` |
|        92 | 2576 | `	if( nArg > 2 ){` |
|        41 | 2577 | `		zPrefix = ph7_value_to_string(apArg[2],&nPrefix);` |
|        41 | 2578 | `		if( nPrefix > 0 && !VmIsValidVarName(zPrefix,(sxu32)nPrefix) ){` |
|         5 | 2579 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 2580 | `				"extract(): Argument #3 ($prefix) must be a valid identifier");` |
|         - | 2581 | `		}` |
|        17 | 2582 | `	}` |
|         - | 2583 | `	/* Point to the target hashmap */` |
|        88 | 2584 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        88 | 2585 | `	if( pMap->nEntry < 1 ){` |
|         - | 2586 | `		/* Empty map,return  0 */` |
|       ! 0 | 2587 | `		ph7_result_int(pCtx,0);` |
|       ! 0 | 2588 | `		return PH7_OK;` |
|         - | 2589 | `	}` |
|        88 | 2590 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        88 | 2591 | `	PH7_MemObjInit(pVm,&sValue);` |
|         - | 2592 | `	/* php walks a COPY of the array, so an entry that overwrites the caller's own` |
|         - | 2593 | `	 * array variable ($arr = ['arr'=>1]; extract($arr)) cannot pull the map from` |
|         - | 2594 | `	 * under the walk. Pinning the map for the walk is the same guarantee. */` |
|        88 | 2595 | `	pMap->iRef++;` |
|        88 | 2596 | `	pEntry = pMap->pFirst;` |
|         - | 2597 | `	/* pFirst walks the insertion order through pPrev — PH7 links the entry list` |
|         - | 2598 | `	 * in reverse (same traversal PH7_HashmapWalk uses). */` |
|       256 | 2599 | `	for( n = pMap->nEntry ; n > 0 && pEntry ; --n, pEntry = pEntry->pPrev ){` |
|         - | 2600 | `		const char *zKey, *zFinal;` |
|         - | 2601 | `		sxu32 nKey, nFinal;` |
|         - | 2602 | `		char zNum[32];` |
|         - | 2603 | `		int bIntKey, iAction;` |
|         - | 2604 | `		/* Work off a COPY of the entry value. Installing a variable used to grow` |
|         - | 2605 | `		 * pVm->aMemObj and dangle a pointer into it (this is why the walk API` |
|         - | 2606 | `		 * hands out copies too); P1's fixed segments retired that, but the copy` |
|         - | 2607 | `		 * still earns its place for the reference discipline below. The` |
|         - | 2608 | `		 * release comes FIRST so it covers every continue/goto below: the load` |
|         - | 2609 | `		 * takes a reference on an array/object value and does not drop the one` |
|         - | 2610 | `		 * the previous entry left behind (PH7_HashmapWalk releases per iteration` |
|         - | 2611 | `		 * for the same reason — without it a whole-array extract() pins every` |
|         - | 2612 | `		 * value it copied, and their destructors never run). */` |
|       174 | 2613 | `		PH7_MemObjRelease(&sValue);` |
|       174 | 2614 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|       174 | 2615 | `		bIntKey = (pEntry->iType == HASHMAP_INT_NODE);` |
|       174 | 2616 | `		if( bIntKey ){` |
|         - | 2617 | `			/* Only the two prefixing modes below ever look at a numeric key */` |
|        22 | 2618 | `			nKey = SyBufferFormat(zNum,sizeof(zNum),"%qd",pEntry->xKey.iKey);` |
|        22 | 2619 | `			zKey = zNum;` |
|        12 | 2620 | `		}else{` |
|       154 | 2621 | `			zKey = (const char *)SyBlobData(&pEntry->xKey.sKey);` |
|       154 | 2622 | `			nKey = SyBlobLength(&pEntry->xKey.sKey);` |
|         - | 2623 | `		}` |
|       174 | 2624 | `		iAction = VM_EXTRACT_DROP;` |
|       174 | 2625 | `		switch( iType ){` |
|        20 | 2626 | `		case PH7_EXTR_OVERWRITE:` |
|        44 | 2627 | `			if( bIntKey \|\| !VmIsValidVarName(zKey,nKey) ){` |
|         8 | 2628 | `				break;` |
|         - | 2629 | `			}` |
|        32 | 2630 | `			if( VmExtractIsThis(zKey,nKey) ){` |
|         3 | 2631 | `				goto this_error;` |
|         - | 2632 | `			}` |
|        30 | 2633 | `			iAction = VM_EXTRACT_PLAIN;` |
|        30 | 2634 | `			break;` |
|        12 | 2635 | `		case PH7_EXTR_SKIP:` |
|        27 | 2636 | `			if( bIntKey \|\| !VmIsValidVarName(zKey,nKey) \|\| VmExtractIsThis(zKey,nKey) ){` |
|         6 | 2637 | `				break;` |
|         - | 2638 | `			}` |
|        17 | 2639 | `			if( VmExtractVarExists(pVm,zKey,nKey) ){` |
|         8 | 2640 | `				break; /* collision: keep the existing variable */` |
|         - | 2641 | `			}` |
|        10 | 2642 | `			iAction = VM_EXTRACT_PLAIN;` |
|        10 | 2643 | `			break;` |
|        16 | 2644 | `		case PH7_EXTR_IF_EXISTS:` |
|        36 | 2645 | `			if( bIntKey \|\| !VmExtractVarExists(pVm,zKey,nKey) ){` |
|        14 | 2646 | `				break;` |
|         - | 2647 | `			}` |
|        12 | 2648 | `			if( !VmIsValidVarName(zKey,nKey) ){` |
|       ! 0 | 2649 | `				break;` |
|         - | 2650 | `			}` |
|        12 | 2651 | `			if( VmExtractIsThis(zKey,nKey) ){` |
|       ! 0 | 2652 | `				goto this_error;` |
|         - | 2653 | `			}` |
|        12 | 2654 | `			iAction = VM_EXTRACT_PLAIN;` |
|        12 | 2655 | `			break;` |
|         9 | 2656 | `		case PH7_EXTR_PREFIX_SAME:` |
|        21 | 2657 | `			if( bIntKey \|\| nKey < 1 ){` |
|         3 | 2658 | `				break;` |
|         - | 2659 | `			}` |
|        17 | 2660 | `			if( VmExtractVarExists(pVm,zKey,nKey) ){` |
|         6 | 2661 | `				iAction = VM_EXTRACT_PREFIX; /* collision */` |
|        14 | 2662 | `			}else if( !VmIsValidVarName(zKey,nKey) ){` |
|         5 | 2663 | `				break;` |
|       ! 0 | 2664 | `			}else{` |
|         - | 2665 | `				/* $this cannot be a target, but its prefixed form can */` |
|         8 | 2666 | `				iAction = VmExtractIsThis(zKey,nKey) ? VM_EXTRACT_PREFIX : VM_EXTRACT_PLAIN;` |
|         - | 2667 | `			}` |
|        13 | 2668 | `			break;` |
|        13 | 2669 | `		case PH7_EXTR_PREFIX_ALL:` |
|        28 | 2670 | `			if( !bIntKey && nKey < 1 ){` |
|         3 | 2671 | `				break;` |
|         - | 2672 | `			}` |
|        26 | 2673 | `			iAction = VM_EXTRACT_PREFIX;` |
|        26 | 2674 | `			break;` |
|         7 | 2675 | `		case PH7_EXTR_PREFIX_INVALID:` |
|        15 | 2676 | `			iAction = (bIntKey \|\| !VmIsValidVarName(zKey,nKey) \|\| VmExtractIsThis(zKey,nKey))` |
|        13 | 2677 | `				? VM_EXTRACT_PREFIX : VM_EXTRACT_PLAIN;` |
|        16 | 2678 | `			break;` |
|         8 | 2679 | `		case PH7_EXTR_PREFIX_IF_EXISTS:` |
|        18 | 2680 | `			if( !bIntKey && VmExtractVarExists(pVm,zKey,nKey) ){` |
|         3 | 2681 | `				iAction = VM_EXTRACT_PREFIX;` |
|         1 | 2682 | `			}` |
|        16 | 2683 | `			break;` |
|       ! 0 | 2684 | `		default:` |
|       ! 0 | 2685 | `			break;` |
|         - | 2686 | `		}` |
|       172 | 2687 | `		if( iAction == VM_EXTRACT_DROP ){` |
|       125 | 2688 | `			continue;` |
|         - | 2689 | `		}` |
|        98 | 2690 | `		if( iAction == VM_EXTRACT_PREFIX ){` |
|        43 | 2691 | `			if( VmExtractPrefixName(&sWorker,zPrefix,nPrefix,zKey,nKey) != SXRET_OK ){` |
|       ! 0 | 2692 | `				rc = PH7_VmThrowException(pCtx,"Error","PH7 engine is running out of memory");` |
|       ! 0 | 2693 | `				goto done;` |
|         - | 2694 | `			}` |
|        43 | 2695 | `			zFinal = (const char *)SyBlobData(&sWorker);` |
|        43 | 2696 | `			nFinal = SyBlobLength(&sWorker);` |
|        43 | 2697 | `			if( !VmIsValidVarName(zFinal,nFinal) ){` |
|         7 | 2698 | `				continue; /* php drops a prefixed name that is not an identifier */` |
|         - | 2699 | `			}` |
|        37 | 2700 | `			if( VmExtractIsThis(zFinal,nFinal) ){` |
|       ! 0 | 2701 | `				goto this_error;` |
|         - | 2702 | `			}` |
|        20 | 2703 | `		}else{` |
|        58 | 2704 | `			if( VmExtractIsProtected(pVm,zKey,nKey) ){` |
|        13 | 2705 | `				continue; /* $GLOBALS/$_SERVER/... are never a plain target */` |
|         - | 2706 | `			}` |
|        45 | 2707 | `			zFinal = zKey;` |
|        45 | 2708 | `			nFinal = nKey;` |
|         - | 2709 | `		}` |
|        80 | 2710 | `		if( iFlags & PH7_EXTR_REFS ){` |
|         - | 2711 | `			/* Bind to the element's own slot (php's EXTR_REFS) rather than copying */` |
|         9 | 2712 | `			if( VmExtractBindVar(pVm,zFinal,nFinal,pEntry->nValIdx) ){` |
|         9 | 2713 | `				iCount++;` |
|         5 | 2714 | `			}` |
|        76 | 2715 | `		}else if( VmExtractStoreVar(pVm,zFinal,nFinal,&sValue) ){` |
|        72 | 2716 | `			iCount++;` |
|        34 | 2717 | `		}` |
|        80 | 2718 | `		continue;` |
|         1 | 2719 | `this_error:` |
|         3 | 2720 | `		rc = PH7_VmThrowException(pCtx,"Error","Cannot re-assign $this");` |
|         3 | 2721 | `		goto done;` |
|       ! 0 | 2722 | `	}` |
|         - | 2723 | `	/* Number of variables successfully imported */` |
|        86 | 2724 | `	ph7_result_int64(pCtx,iCount);` |
|        42 | 2725 | `done:` |
|        88 | 2726 | `	PH7_MemObjRelease(&sValue);` |
|        88 | 2727 | `	SyBlobRelease(&sWorker);` |
|        88 | 2728 | `	PH7_HashmapUnref(pMap);` |
|        88 | 2729 | `	return rc;` |
|        56 | 2730 | `}` |
|         - | 2731 | `/*` |
|         - | 2732 | ` * Worker callback for the [import_request_variables()] function` |
|         - | 2733 | ` * defined below.` |
|         - | 2734 | ` */` |
|         2 | 2735 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|         1 | 2736 | `{` |
|         3 | 2737 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|         3 | 2738 | `	ph7_vm *pVm = pAux->pVm;` |
|         - | 2739 | `	ph7_value *pObj;` |
|         - | 2740 | `	SyString sVar;` |
|         - | 2741 | `	/* Perform a string cast */` |
|         3 | 2742 | `	PH7_MemObjToString(pKey);` |
|         3 | 2743 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|         - | 2744 | `		/* Unavailable variable name */` |
|       ! 0 | 2745 | `		return SXRET_OK;` |
|         - | 2746 | `	}` |
|         3 | 2747 | `	sVar.nByte = 0; /* cc warning */` |
|         3 | 2748 | `	if( pAux->Prefixlen > 0 ){` |
|         4 | 2749 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|         1 | 2750 | `			pAux->Prefixlen,pAux->zPrefix,` |
|         1 | 2751 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|         - | 2752 | `			);` |
|         2 | 2753 | `	}else{` |
|       ! 0 | 2754 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|       ! 0 | 2755 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|         - | 2756 | `	}` |
|         3 | 2757 | `	sVar.zString = pAux->zWorker;` |
|         - | 2758 | `	/* Extract the variable */` |
|         3 | 2759 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|         3 | 2760 | `	if( pObj ){` |
|         3 | 2761 | `		PH7_MemObjStore(pValue,pObj);` |
|         1 | 2762 | `	}` |
|         3 | 2763 | `	return SXRET_OK;` |
|         2 | 2764 | `}` |
|         - | 2765 | `/*` |
|         - | 2766 | ` * bool import_request_variables(string $types[,string $prefix])` |
|         - | 2767 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|         - | 2768 | ` * Parameters` |
|         - | 2769 | ` * $types` |
|         - | 2770 | ` *  Using the types parameter, you can specify which request variables to import.` |
|         - | 2771 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|         - | 2772 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|         - | 2773 | ` *  POST includes the POST uploaded file information.` |
|         - | 2774 | ` *  Note:` |
|         - | 2775 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|         - | 2776 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|         - | 2777 | ` * $prefix` |
|         - | 2778 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|         - | 2779 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|         - | 2780 | ` *  variable named $pref_userid.` |
|         - | 2781 | ` * Return` |
|         - | 2782 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2783 | ` */` |
|         2 | 2784 | `PH7_PRIVATE int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2785 | `{` |
|         - | 2786 | `	const char *zPrefix,*zEnd,*zImport;` |
|         - | 2787 | `	extract_aux_data sAux;` |
|         - | 2788 | `	int nLen,nPrefixLen;` |
|         - | 2789 | `	ph7_value *pSuper;` |
|         - | 2790 | `	ph7_vm *pVm;` |
|         - | 2791 | `	/* By default import only $_GET variables  */` |
|         3 | 2792 | `	zImport = "G";` |
|         3 | 2793 | `	nLen = (int)sizeof(char);` |
|         3 | 2794 | `	zPrefix = 0;` |
|         3 | 2795 | `	nPrefixLen = 0;` |
|         3 | 2796 | `	if( nArg > 0 ){` |
|         3 | 2797 | `		if( ph7_value_is_string(apArg[0]) ){` |
|         3 | 2798 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|         1 | 2799 | `		}` |
|         3 | 2800 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|         3 | 2801 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|         1 | 2802 | `		}` |
|         1 | 2803 | `	}` |
|         - | 2804 | `	/* Point to the underlying VM */` |
|         3 | 2805 | `	pVm = pCtx->pVm;` |
|         - | 2806 | `	/* Initialize the aux data */` |
|         3 | 2807 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|         3 | 2808 | `	sAux.zPrefix = zPrefix;` |
|         3 | 2809 | `	sAux.Prefixlen = nPrefixLen;` |
|         3 | 2810 | `	sAux.pVm = pVm;` |
|         - | 2811 | `	/* Extract */` |
|         3 | 2812 | `	zEnd = &zImport[nLen];` |
|         5 | 2813 | `	while( zImport < zEnd ){` |
|         3 | 2814 | `		int c = zImport[0];` |
|         3 | 2815 | `		pSuper = 0;` |
|         3 | 2816 | `		if( c == 'G' \|\| c == 'g' ){` |
|         - | 2817 | `			/* Import $_GET variables */` |
|         3 | 2818 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|         1 | 2819 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|         - | 2820 | `			/* Import $_POST variables */` |
|       ! 0 | 2821 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|       ! 0 | 2822 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|         - | 2823 | `			/* Import $_COOKIE variables */` |
|       ! 0 | 2824 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|       ! 0 | 2825 | `		}` |
|         3 | 2826 | `		if( pSuper ){` |
|         - | 2827 | `			/* Iterate throw array entries */` |
|         3 | 2828 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|         1 | 2829 | `		}` |
|         - | 2830 | `		/* Advance the cursor */` |
|         3 | 2831 | `		zImport++;` |
|         1 | 2832 | `	}` |
|         - | 2833 | `	/* All done,return TRUE*/` |
|         3 | 2834 | `	ph7_result_bool(pCtx,0);` |
|         3 | 2835 | `	return PH7_OK;` |
|         1 | 2836 | `}` |
|         - | 2837 |  |
