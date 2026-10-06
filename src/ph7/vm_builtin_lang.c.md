# src/ph7/vm_builtin_lang.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1276/1444 lines (88.37%)

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
|         - |   37 | `` * PH7_VmResolveCallableScope, so `self`/`parent`/`static` (in any case) answer against the live class`` |
|         - |   38 | ` * context and a plain name AUTOLOADS on a miss (php does both here). The constant half` |
|         - |   39 | ` * is looked up without evaluating anything: an unmaterialized enum case or an on-demand` |
|         - |   40 | ` * constant initializer must not run just because someone ASKED whether the name exists.` |
|         - |   41 | ` * The class name is case-insensitive and the constant name is not, exactly as php.` |
|         - |   42 | ` */` |
|      3738 |   43 | `static int VmClassConstLookup(` |
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
|      3743 |   55 | `	*ppClass = 0;` |
|      3743 |   56 | `	*ppAttr = 0;` |
|     71947 |   57 | `	for( iSep = 0; iSep + 1 < nLen; iSep++ ){` |
|     68447 |   58 | `		if( zName[iSep] == ':' && zName[iSep+1] == ':' ){` |
|       243 |   59 | `			break;` |
|         - |   60 | `		}` |
|     34107 |   61 | `	}` |
|      3743 |   62 | `	if( iSep + 1 >= nLen ){` |
|      3505 |   63 | `		return VM_CCONST_PLAIN;` |
|         - |   64 | `	}` |
|       243 |   65 | `	*pSep = iSep;` |
|       243 |   66 | `	pClass = iSep > 0 ? PH7_VmResolveCallableScope(&(*pVm),zName,(sxu32)iSep) : 0;` |
|       243 |   67 | `	if( pClass == 0 ){` |
|        49 |   68 | `		if( iSep > 0 && PH7_VmIsScopeKeyword(zName,(sxu32)iSep) ){` |
|         - |   69 | `			/* php separates the two ways a keyword can fail to resolve, so tell them` |
|         - |   70 | ``			 * apart here: `parent` inside a class that simply has no parent is a`` |
|         - |   71 | `			 * different sentence from a keyword named with no class scope at all. */` |
|        30 |   72 | `			if( iSep == 6 && SyStrnicmp(zName,"parent",6) == 0` |
|        19 |   73 | `			 && (PH7_VmPeekTopClass(&(*pVm)) \|\| PH7_VmPeekDeclaringClass(&(*pVm))) ){` |
|         6 |   74 | `				return VM_CCONST_NOPARENT;` |
|         - |   75 | `			}` |
|        29 |   76 | `			return VM_CCONST_NOSCOPE;` |
|         - |   77 | `		}` |
|        18 |   78 | `		return VM_CCONST_NOCLASS;` |
|         - |   79 | `	}` |
|       198 |   80 | `	*ppClass = pClass;` |
|       198 |   81 | `	if( iSep + 2 >= nLen ){` |
|         6 |   82 | `		return VM_CCONST_NOCONST; /* "C::" names no constant */` |
|         - |   83 | `	}` |
|         - |   84 | `	/* This form names a class CONSTANT or an enum case (hConst), never a property. */` |
|       194 |   85 | `	pAttr = PH7_ClassExtractConstant(pClass,&zName[iSep+2],(sxu32)(nLen - iSep - 2));` |
|       194 |   86 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        30 |   87 | `		return VM_CCONST_NOCONST;` |
|         - |   88 | `	}` |
|       167 |   89 | `	if( (pClass->iFlags & PH7_CLASS_TRAIT) != 0 ){` |
|         - |   90 | `		/* A trait constant belongs to the classes that COMPOSE the trait: php refuses` |
|         - |   91 | ``		 * `constant("T::K")` outright, and answers `defined("T::K")` with false. */`` |
|         3 |   92 | `		*ppAttr = pAttr;` |
|         3 |   93 | `		return VM_CCONST_TRAIT;` |
|         - |   94 | `	}` |
|       162 |   95 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|        95 |   96 | `	 && pAttr->pDeclClass && pAttr->pDeclClass != pClass ){` |
|         - |   97 | `		/* php does not put a private constant in a SUBCLASS's table at all, so naming it` |
|         - |   98 | ``		 * through the child is not an access denial but a plain miss — `Sub::P` reports`` |
|         - |   99 | ``		 * `Undefined constant Sub::P` even from inside the declaring class, and`` |
|         - |  100 | `		 * defined("Sub::P") is false there too. Only the declaring class can answer. */` |
|       ! 0 |  101 | `		return VM_CCONST_NOCONST;` |
|         - |  102 | `	}` |
|       165 |  103 | `	*ppAttr = pAttr;` |
|         - |  104 | ``	/* php answers by the CALLING scope, the same rule the direct `C::K` access uses:`` |
|         - |  105 | `	 * a private constant is invisible from outside its declaring class even to a` |
|         - |  106 | `	 * subclass, and a protected one is visible down the hierarchy. */` |
|       165 |  107 | `	if( !PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        23 |  108 | `		return VM_CCONST_NOACCESS;` |
|         - |  109 | `	}` |
|       145 |  110 | `	return VM_CCONST_OK;` |
|      1874 |  111 | `}` |
|         - |  112 | `/*` |
|         - |  113 | ` * Raise the Error php raises for a "C::K" name that did not resolve. php prints the class` |
|         - |  114 | `` * part exactly as the caller WROTE it — `c::Q`, `self::P`, `parent::P` — rather than the`` |
|         - |  115 | ` * canonical class name, so the message quotes the source span. Never called with` |
|         - |  116 | ` * VM_CCONST_OK or VM_CCONST_PLAIN.` |
|         - |  117 | ` */` |
|        62 |  118 | `static int VmClassConstError(` |
|         - |  119 | `	ph7_context *pCtx,      /* Call context */` |
|         - |  120 | `	int rc,                 /* VmClassConstLookup() verdict */` |
|         - |  121 | `	const char *zName,      /* The whole "C::K" name */` |
|         - |  122 | `	int nLen,               /* zName length */` |
|         - |  123 | `	int iSep,               /* Offset of the "::" */` |
|         - |  124 | `	ph7_class_attr *pAttr   /* The constant, when one was found */` |
|         - |  125 | `	)` |
|         3 |  126 | `{` |
|        65 |  127 | `	if( nLen > 0 && zName[0] == '\\' ){` |
|         - |  128 | `` 		/* The global-namespace anchor is not part of the name php echoes back: `\C::P` `` |
|         - |  129 | ``		 * reports `C::P` (exactly ONE leading backslash goes, the rest stays). */`` |
|         3 |  130 | `		zName++;` |
|         3 |  131 | `		nLen--;` |
|         3 |  132 | `		iSep--;` |
|         1 |  133 | `	}` |
|        65 |  134 | `	switch( rc ){` |
|         3 |  135 | `		case VM_CCONST_NOCLASS:` |
|         8 |  136 | `			return PH7_VmThrowException(pCtx,"Error","Class \"%.*s\" not found",iSep,zName);` |
|        13 |  137 | `		case VM_CCONST_NOSCOPE: {` |
|         - |  138 | `			/* php names the keyword as it folded it, whatever case it was written in. */` |
|         - |  139 | `			char zKw[8];` |
|         - |  140 | `			int i;` |
|       165 |  141 | `			for( i = 0 ; i < iSep && i < (int)sizeof(zKw) ; ++i ){` |
|       139 |  142 | `				zKw[i] = (zName[i] >= 'A' && zName[i] <= 'Z') ? (char)(zName[i] + 32) : zName[i];` |
|        71 |  143 | `			}` |
|        42 |  144 | `			return PH7_VmThrowException(pCtx,"Error",` |
|        13 |  145 | `				"Cannot access \"%.*s\" when no class scope is active",i,zKw);` |
|         - |  146 | `		}` |
|         2 |  147 | `		case VM_CCONST_NOPARENT:` |
|         6 |  148 | `			return PH7_VmThrowException(pCtx,"Error",` |
|         - |  149 | `				"Cannot access \"parent\" when current class scope has no parent");` |
|         6 |  150 | `		case VM_CCONST_NOACCESS:` |
|        25 |  151 | `			return PH7_VmThrowException(pCtx,"Error","Cannot access %s constant %.*s",` |
|        12 |  152 | `				(pAttr && pAttr->iProtection == PH7_CLASS_PROT_PRIVATE) ? "private" : "protected",` |
|         6 |  153 | `				nLen,zName);` |
|       ! 0 |  154 | `		case VM_CCONST_TRAIT:` |
|       ! 0 |  155 | `			return PH7_VmThrowException(pCtx,"Error",` |
|       ! 0 |  156 | `				"Cannot access trait constant %.*s directly",nLen,zName);` |
|         7 |  157 | `		default:` |
|        14 |  158 | `			break;` |
|         - |  159 | `	}` |
|        16 |  160 | `	return PH7_VmThrowException(pCtx,"Error","Undefined constant %.*s",nLen,zName);` |
|        34 |  161 | `}` |
|         - |  162 | `/*` |
|         - |  163 | ` * bool defined(string $name)` |
|         - |  164 | ` *  Checks whether a given named constant exists.` |
|         - |  165 | ` * Parameter:` |
|         - |  166 | ` *  Name of the desired constant.` |
|         - |  167 | ` * Return` |
|         - |  168 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|         - |  169 | ` */` |
|      1814 |  170 | `PH7_PRIVATE int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 |  171 | `{` |
|         - |  172 | `	ph7_class_attr *pAttr;` |
|         - |  173 | `	ph7_class *pClass;` |
|         - |  174 | `	const char *zName;` |
|      1819 |  175 | `	int nLen = 0;` |
|      1819 |  176 | `	int iSep = 0;` |
|      1819 |  177 | `	int res = 0;` |
|      1819 |  178 | `	if( nArg < 1 ){` |
|         - |  179 | `		/* Missing constant name,return FALSE */` |
|       ! 0 |  180 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|       ! 0 |  181 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 |  182 | `		return SXRET_OK;` |
|         - |  183 | `	}` |
|         - |  184 | `	/* Extract constant name */` |
|      1819 |  185 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|         - |  186 | `	/* Class-constant form "C::K": every miss — unknown class, unknown constant, or one` |
|         - |  187 | `	 * that is not visible from here — is a plain FALSE, since asking whether a name is` |
|         - |  188 | `	 * defined is exactly what defined() is for (this used to consult the` |
|         - |  189 | `	 * global constant table only, so EVERY class constant answered false while` |
|         - |  190 | `	 * constant() read the same name correctly). */` |
|      1819 |  191 | `	if( nLen > 0 ){` |
|      1819 |  192 | `		int iRc = VmClassConstLookup(pCtx->pVm,zName,nLen,&pClass,&pAttr,&iSep);` |
|      1819 |  193 | `		switch( iRc ){` |
|       861 |  194 | `			case VM_CCONST_PLAIN:` |
|      1727 |  195 | `				break;` |
|        21 |  196 | `			case VM_CCONST_OK:` |
|        44 |  197 | `				ph7_result_bool(pCtx,1);` |
|        44 |  198 | `				return SXRET_OK;` |
|         8 |  199 | `			case VM_CCONST_NOSCOPE:` |
|         - |  200 | `			case VM_CCONST_NOPARENT:` |
|         - |  201 | ``				/* php refuses the question rather than answering it: naming `self` where`` |
|         - |  202 | ``				 * no class scope is active is an Error, not a `false`. Every OTHER miss`` |
|         - |  203 | `				 * is a false, so only these two reach the shared thrower. */` |
|        18 |  204 | `				return VmClassConstError(pCtx,iRc,zName,nLen,iSep,pAttr);` |
|        17 |  205 | `			default:` |
|        38 |  206 | `				ph7_result_bool(pCtx,0);` |
|        38 |  207 | `				return SXRET_OK;` |
|         - |  208 | `		}` |
|       861 |  209 | `	}` |
|         - |  210 | `	/* Perform the lookup */` |
|      1727 |  211 | `	if( nLen > 0 && PH7_VmConstantFetch(pCtx->pVm,zName,(sxu32)nLen,1) != 0 ){` |
|         - |  212 | `		/* Already defined */` |
|      1681 |  213 | `		res = 1;` |
|       837 |  214 | `	}` |
|      1727 |  215 | `	ph7_result_bool(pCtx,res);` |
|      1727 |  216 | `	return SXRET_OK;` |
|       912 |  217 | `}` |
|         - |  218 | `/*` |
|         - |  219 | ` * Constant expansion callback used by the [define()] function defined` |
|         - |  220 | ` * below.` |
|         - |  221 | ` */` |
|    102762 |  222 | `PH7_PRIVATE void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|         5 |  223 | `{` |
|    102767 |  224 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|         - |  225 | `	/* Expand constant value */` |
|    102767 |  226 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|    102767 |  227 | `}` |
|         - |  228 | `/*` |
|         - |  229 | `` * Whether define() or a `const` statement must refuse zName, raising php's warning`` |
|         - |  230 | ` * when it must. php refuses the three keyword constants and __COMPILER_HALT_OFFSET__` |
|         - |  231 | ` * by spelling, then any name the table already holds; the message names the folded` |
|         - |  232 | ` * key, namespace part lowercased. A user constant a reused VM's EARLIER run made is` |
|         - |  233 | ` * not a redefinition: each run is a fresh php request.` |
|         - |  234 | ` */` |
|       466 |  235 | `PH7_PRIVATE int PH7_VmConstantNameTaken(ph7_vm *pVm,const char *zName,sxu32 nLen)` |
|         5 |  236 | `{` |
|         - |  237 | `	SyHashEntry *pEntry;` |
|       471 |  238 | `	ph7_constant *pCons = 0;` |
|       471 |  239 | `	const char *zShow = zName;` |
|       466 |  240 | `	if( (nLen == 4 && (SyStrnicmp(zName,"null",4) == 0 \|\| SyStrnicmp(zName,"true",4) == 0))` |
|       463 |  241 | `	 \|\| (nLen == 5 && SyStrnicmp(zName,"false",5) == 0)` |
|       467 |  242 | `	 \|\| (nLen == 24 && SyMemcmp(zName,"__COMPILER_HALT_OFFSET__",24) == 0) ){` |
|         9 |  243 | `		goto Taken;` |
|         - |  244 | `	}` |
|       463 |  245 | `	pEntry = PH7_VmConstantFetch(pVm,zName,nLen,0);` |
|       463 |  246 | `	if( pEntry == 0 ){` |
|       431 |  247 | `		return 0;` |
|         - |  248 | `	}` |
|        35 |  249 | `	pCons = (ph7_constant *)pEntry->pUserData;` |
|        35 |  250 | `	if( pCons->bUserDefined && pCons->nRunGen != pVm->nRunGen ){` |
|       ! 0 |  251 | `		return 0;` |
|         - |  252 | `	}` |
|        41 |  253 | `	if( pCons->zKey ){` |
|        14 |  254 | `		zShow = pCons->zKey;` |
|         6 |  255 | `	}` |
|        10 |  256 | `Taken:` |
|        63 |  257 | `	PH7_VmThrowWarningFmt(pVm,"Constant %.*s already defined, this will be an error in PHP 9",` |
|        20 |  258 | `		(int)nLen,zShow);` |
|        43 |  259 | `	return 1;` |
|       238 |  260 | `}` |
|         - |  261 | `/*` |
|         - |  262 | ` * bool define(string $constant_name,expression value)` |
|         - |  263 | ` *  Defines a named constant at runtime.` |
|         - |  264 | ` * Parameter:` |
|         - |  265 | ` *  $constant_name` |
|         - |  266 | ` *   The name of the constant` |
|         - |  267 | ` *  $value` |
|         - |  268 | ` *   Constant value` |
|         - |  269 | ` * Return:` |
|         - |  270 | ` *   TRUE on success,FALSE on failure.` |
|         - |  271 | ` */` |
|       224 |  272 | `PH7_PRIVATE int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 |  273 | `{` |
|         - |  274 | `	const char *zName;  /* Constant name */` |
|         - |  275 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       229 |  276 | `	int nLen = 0;       /* Name length */` |
|         - |  277 | `	sxi32 rc;` |
|       229 |  278 | `	if( nArg < 2 ){` |
|         - |  279 | `		/* Missing arguments,throw a ntoice and return false */` |
|       ! 0 |  280 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|       ! 0 |  281 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 |  282 | `		return SXRET_OK;` |
|         - |  283 | `	}` |
|       229 |  284 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|       ! 0 |  285 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|       ! 0 |  286 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 |  287 | `		return SXRET_OK;` |
|         - |  288 | `	}` |
|         - |  289 | `	/* Extract constant name */` |
|       229 |  290 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       229 |  291 | `	if( nLen < 1 ){` |
|       ! 0 |  292 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|       ! 0 |  293 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 |  294 | `		return SXRET_OK;` |
|         - |  295 | `	}` |
|         - |  296 | `	/* php's refusals, in its order: a class-constant name throws, the retired third` |
|         - |  297 | `	 * argument only warns, and a name already taken answers FALSE under a warning` |
|         - |  298 | `	 * and KEEPS the first value -- engine constants (M_PI, E_ALL) included. */` |
|         - |  299 | `	{` |
|         - |  300 | `		int i;` |
|      2681 |  301 | `		for( i = 0 ; i + 1 < nLen ; ++i ){` |
|      2459 |  302 | `			if( zName[i] == ':' && zName[i+1] == ':' ){` |
|         3 |  303 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - |  304 | `					"define(): Argument #1 ($constant_name) cannot be a class constant");` |
|         - |  305 | `			}` |
|      1231 |  306 | `		}` |
|         - |  307 | `	}` |
|       227 |  308 | `	if( nArg > 2 && ph7_value_to_bool(apArg[2]) ){` |
|         3 |  309 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Argument #3 ($case_insensitive) is ignored "` |
|         - |  310 | `			"since declaration of case-insensitive constants is no longer supported");` |
|         1 |  311 | `	}` |
|       227 |  312 | `	if( PH7_VmConstantNameTaken(pCtx->pVm,zName,(sxu32)nLen) ){` |
|        30 |  313 | `		ph7_result_bool(pCtx,0);` |
|        30 |  314 | `		return SXRET_OK;` |
|         - |  315 | `	}` |
|         - |  316 | `	/* Duplicate constant value */` |
|       199 |  317 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       199 |  318 | `	if( pValue == 0 ){` |
|       ! 0 |  319 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|       ! 0 |  320 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 |  321 | `		return SXRET_OK;` |
|         - |  322 | `	}` |
|         - |  323 | `	/* Initialize the memory object */` |
|       199 |  324 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|         - |  325 | `	/* Register the constant */` |
|         - |  326 | `	{` |
|         - |  327 | `		SyString sConsName;` |
|       199 |  328 | `		SyStringInitFromBuf(&sConsName,zName,(sxu32)nLen);` |
|       296 |  329 | `		rc = PH7_VmRegisterConstantEx(pCtx->pVm,&sConsName,VmExpandUserConstant,pValue,` |
|       194 |  330 | `			(SyString *)SySetPeek(&pCtx->pVm->aFiles),0,1);` |
|         - |  331 | `	}` |
|       199 |  332 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  333 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|       ! 0 |  334 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|       ! 0 |  335 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 |  336 | `		return SXRET_OK;` |
|         - |  337 | `	}` |
|         - |  338 | `	/* Duplicate constant value */` |
|       199 |  339 | `	PH7_MemObjStore(apArg[1],pValue);` |
|         - |  340 | `	/* All done,return TRUE */` |
|       199 |  341 | `	ph7_result_bool(pCtx,1);` |
|       199 |  342 | `	return SXRET_OK;` |
|       117 |  343 | `}` |
|         - |  344 | `/*` |
|         - |  345 | ` * value constant(string $name)` |
|         - |  346 | ` *  Returns the value of a constant` |
|         - |  347 | ` * Parameter` |
|         - |  348 | ` *  $name` |
|         - |  349 | ` *    Name of the constant.` |
|         - |  350 | ` * Return` |
|         - |  351 | ` *  Constant value or NULL if not defined.` |
|         - |  352 | ` */` |
|         - |  353 | `/*` |
|         - |  354 | ` * Enum method thunks (PHP 8.1). Every enum's synthesized cases()/from()/` |
|         - |  355 | ` * tryFrom() methods (GenStateCompileEnumMethods, compile.c) forward here with` |
|         - |  356 | ` * the enum's FQN as a literal first argument — the same forwarder pattern the` |
|         - |  357 | ` * Generator/Fiber/Reflection builtins use.` |
|         - |  358 | ` */` |
|         - |  359 | `/* array __phl_enum_cases(string $enumFqn) — declaration-order case list */` |
|        23 |  360 | `PH7_PRIVATE int vm_builtin_enum_cases(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 |  361 | `{` |
|        25 |  362 | `	ph7_vm *pVm = pCtx->pVm;` |
|         - |  363 | `	ph7_class_attr **apCase;` |
|         - |  364 | `	ph7_class *pClass;` |
|         - |  365 | `	ph7_value *pArray;` |
|         - |  366 | `	sxu32 n;` |
|         - |  367 | `	sxi32 rc;` |
|        25 |  368 | `	if( nArg < 1 \|\| (pClass = VmExtractEnumClass(pVm,apArg[0])) == 0 ){` |
|       ! 0 |  369 | `		ph7_result_null(pCtx);` |
|       ! 0 |  370 | `		return SXRET_OK;` |
|         - |  371 | `	}` |
|        25 |  372 | `	rc = VmEnumMaterialize(pVm,pClass);` |
|        25 |  373 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  374 | `		return rc;` |
|         - |  375 | `	}` |
|        25 |  376 | `	pArray = ph7_context_new_array(pCtx);` |
|        25 |  377 | `	if( pArray == 0 ){` |
|       ! 0 |  378 | `		ph7_result_null(pCtx);` |
|       ! 0 |  379 | `		return SXRET_OK;` |
|         - |  380 | `	}` |
|        25 |  381 | `	apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|       100 |  382 | `	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|        77 |  383 | `		ph7_value *pSlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,apCase[n]->nIdx);` |
|        77 |  384 | `		if( pSlot ){` |
|        77 |  385 | `			ph7_array_add_elem(pArray,0,pSlot); /* Copies; the object ref is retained */` |
|        35 |  386 | `		}` |
|        37 |  387 | `	}` |
|        25 |  388 | `	ph7_result_value(pCtx,pArray);` |
|        25 |  389 | `	return SXRET_OK;` |
|        13 |  390 | `}` |
|         - |  391 | `/*` |
|         - |  392 | `` * php declares from()/tryFrom() as `string\|int $value` on the BackedEnum`` |
|         - |  393 | ` * prototype, so the argument arrives in either form and the enum's own backing` |
|         - |  394 | ` * type decides what happens next -- which is why the refusal is worded against` |
|         - |  395 | ` * the BACKING type ("must be of type int, string given" for an int-backed enum` |
|         - |  396 | ` * given "2x") and not against the declared union. php words the union only when` |
|         - |  397 | ` * the value is neither a string nor an int and the enum is string-backed; the` |
|         - |  398 | ` * asymmetry is php's own.` |
|         - |  399 | ` *` |
|         - |  400 | ` * Everything else is ordinary weak coercion, so an int-backed enum accepts "02"` |
|         - |  401 | ` * and " 2" as 2, and a string-backed one takes an int (or a bool, or a` |
|         - |  402 | ` * non-lossy float) through the INT arm first: S::from(1.0) looks for "1", not` |
|         - |  403 | ` * "1.0", and S::from(false) for "0". A LOSSY float and a null are php` |
|         - |  404 | ` * DEPRECATIONS, so PH7_IntArgResolve refuses them (the scope policy scope policy) with the` |
|         - |  405 | ` * TypeError php will eventually raise.` |
|         - |  406 | ` */` |
|       186 |  407 | `static sxi32 VmEnumCoerceNeedle(ph7_context *pCtx,ph7_class *pClass,ph7_value *pArg,` |
|         - |  408 | `	ph7_value *pOut)` |
|         2 |  409 | `{` |
|       188 |  410 | `	int bStrBacked = pClass->nEnumBacking != MEMOBJ_INT;` |
|       188 |  411 | `	sxi64 iVal = 0;` |
|         - |  412 | `	sxi32 rc;` |
|       188 |  413 | `	PH7_MemObjInit(pCtx->pVm,pOut);` |
|       188 |  414 | `	if( ph7_value_is_string(pArg) && bStrBacked ){` |
|        80 |  415 | `		PH7_MemObjLoad(pArg,pOut);` |
|        80 |  416 | `		return SXRET_OK;` |
|         - |  417 | `	}` |
|         - |  418 | `	/* A native method's own name is already qualified ("I2::from"). */` |
|       163 |  419 | `	rc = PH7_IntArgResolve(pCtx,pArg,ph7_function_name(pCtx),1,"$value",` |
|        54 |  420 | `		bStrBacked ? "string\|int" : "int",&iVal);` |
|       109 |  421 | `	if( rc != PH7_OK ){` |
|        13 |  422 | `		return rc;` |
|         - |  423 | `	}` |
|        97 |  424 | `	if( bStrBacked ){` |
|         - |  425 | `		char zNum[32];` |
|        35 |  426 | `		int nNum = SyBufferFormat(zNum,sizeof(zNum),"%qd",iVal);` |
|        35 |  427 | `		PH7_MemObjStringAppend(pOut,zNum,(sxu32)nNum);` |
|        18 |  428 | `	}else{` |
|        63 |  429 | `		pOut->x.iVal = iVal;` |
|        63 |  430 | `		MemObjSetType(pOut,MEMOBJ_INT);` |
|         - |  431 | `	}` |
|        97 |  432 | `	return SXRET_OK;` |
|        95 |  433 | `}` |
|         - |  434 | `/* Shared scan for from()/tryFrom(): return the slot of the case whose backing` |
|         - |  435 | ` * value equals *pNeedle (already coerced to the backing type), or 0 on miss. */` |
|       174 |  436 | `static ph7_value * VmEnumFindCaseByValue(ph7_vm *pVm,ph7_class *pClass,ph7_value *pNeedle)` |
|         2 |  437 | `{` |
|       176 |  438 | `	ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|         - |  439 | `	sxu32 n;` |
|       412 |  440 | `	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|       322 |  441 | `		ph7_value *pVal = VmEnumCaseBackingValue(pVm,apCase[n]);` |
|       322 |  442 | `		int bMatch = 0;` |
|       322 |  443 | `		if( pVal ){` |
|       322 |  444 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|       101 |  445 | `				bMatch = (pNeedle->iFlags & MEMOBJ_INT) && pVal->x.iVal == pNeedle->x.iVal;` |
|        51 |  446 | `			}else{` |
|       332 |  447 | `				bMatch = (pNeedle->iFlags & MEMOBJ_STRING)` |
|       220 |  448 | `					&& SyBlobLength(&pVal->sBlob) == SyBlobLength(&pNeedle->sBlob)` |
|       330 |  449 | `					&& SyMemcmp(SyBlobData(&pVal->sBlob),SyBlobData(&pNeedle->sBlob),` |
|       132 |  450 | `						SyBlobLength(&pNeedle->sBlob)) == 0;` |
|         - |  451 | `			}` |
|       160 |  452 | `		}` |
|       322 |  453 | `		if( bMatch ){` |
|        86 |  454 | `			return (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,apCase[n]->nIdx);` |
|         - |  455 | `		}` |
|       120 |  456 | `	}` |
|        92 |  457 | `	return 0;` |
|        89 |  458 | `}` |
|         - |  459 | `/* static from(int\|string $value) / static tryFrom(int\|string $value) */` |
|       186 |  460 | `static int VmEnumFromCommon(ph7_context *pCtx,int nArg,ph7_value **apArg,int bTry)` |
|         2 |  461 | `{` |
|       188 |  462 | `	ph7_vm *pVm = pCtx->pVm;` |
|         - |  463 | `	ph7_class *pClass;` |
|         - |  464 | `	ph7_value *pFound;` |
|         - |  465 | `	ph7_value sNeedle;` |
|         - |  466 | `	sxi32 rc;` |
|       188 |  467 | `	if( nArg < 2 \|\| (pClass = VmExtractEnumClass(pVm,apArg[0])) == 0 ){` |
|       ! 0 |  468 | `		ph7_result_null(pCtx);` |
|       ! 0 |  469 | `		return SXRET_OK;` |
|         - |  470 | `	}` |
|       188 |  471 | `	rc = VmEnumMaterialize(pVm,pClass);` |
|       188 |  472 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  473 | `		return rc;` |
|         - |  474 | `	}` |
|       188 |  475 | `	rc = VmEnumCoerceNeedle(pCtx,pClass,apArg[1],&sNeedle);` |
|       188 |  476 | `	if( rc != SXRET_OK ){` |
|        13 |  477 | `		PH7_MemObjRelease(&sNeedle);` |
|        13 |  478 | `		return rc;` |
|         - |  479 | `	}` |
|       176 |  480 | `	pFound = VmEnumFindCaseByValue(pVm,pClass,&sNeedle);` |
|       176 |  481 | `	if( pFound ){` |
|        86 |  482 | `		ph7_result_value(pCtx,pFound);` |
|        86 |  483 | `		PH7_MemObjRelease(&sNeedle);` |
|        86 |  484 | `		return SXRET_OK;` |
|         - |  485 | `	}` |
|        92 |  486 | `	if( bTry ){` |
|        50 |  487 | `		ph7_result_null(pCtx);` |
|        50 |  488 | `		PH7_MemObjRelease(&sNeedle);` |
|        50 |  489 | `		return SXRET_OK;` |
|         - |  490 | `	}` |
|        44 |  491 | `	if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|         - |  492 | `		char zVal[32];` |
|         9 |  493 | `		SyBufferFormat(zVal,sizeof(zVal),"%qd",sNeedle.x.iVal);` |
|        13 |  494 | `		rc = PH7_VmThrowException(pCtx,"ValueError",` |
|         4 |  495 | `			"%s is not a valid backing value for enum %z",zVal,&pClass->sDisp);` |
|         5 |  496 | `	}else{` |
|        53 |  497 | `		rc = PH7_VmThrowException(pCtx,"ValueError",` |
|         - |  498 | `			"\"%.*s\" is not a valid backing value for enum %z",` |
|        34 |  499 | `			(int)SyBlobLength(&sNeedle.sBlob),(const char *)SyBlobData(&sNeedle.sBlob),` |
|        17 |  500 | `			&pClass->sDisp);` |
|         - |  501 | `	}` |
|        44 |  502 | `	PH7_MemObjRelease(&sNeedle);` |
|        44 |  503 | `	return rc;` |
|        95 |  504 | `}` |
|       102 |  505 | `PH7_PRIVATE int vm_builtin_enum_from(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 |  506 | `{` |
|       104 |  507 | `	return VmEnumFromCommon(pCtx,nArg,apArg,FALSE);` |
|         2 |  508 | `}` |
|        84 |  509 | `PH7_PRIVATE int vm_builtin_enum_tryfrom(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 |  510 | `{` |
|        86 |  511 | `	return VmEnumFromCommon(pCtx,nArg,apArg,TRUE);` |
|         2 |  512 | `}` |
|         - |  513 | `/*` |
|         - |  514 | ` * bool enum_exists(string $enum, bool $autoload = true)` |
|         - |  515 | ` *  TRUE only for a declared enum (PHP 8.1); a plain class/interface is FALSE.` |
|         - |  516 | ` *` |
|         - |  517 | ` * $autoload was declared and never read: VmExtractEnumClass resolves through` |
|         - |  518 | ``  * PH7_VmExtractClass, which always asks the autoloader, so `enum_exists($n,false)` `` |
|         - |  519 | ` * ran the loader anyway. That is the one spelling a program uses to ask "is this` |
|         - |  520 | ` * already loaded" WITHOUT paying for a load, and its whole point is the refusal.` |
|         - |  521 | ` */` |
|        39 |  522 | `PH7_PRIVATE int vm_builtin_enum_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 |  523 | `{` |
|        42 |  524 | `	ph7_class *pClass = 0;` |
|        42 |  525 | `	if( nArg > 0 ){` |
|        49 |  526 | `		if( nArg >= 2 && !ph7_value_to_bool(apArg[1]) ){` |
|         - |  527 | `			/* Declared-only lookup: probe the class table directly, then filter` |
|         - |  528 | `			 * the same-name chain down to an enum exactly as VmExtractEnumClass does. */` |
|         - |  529 | `			const char *zName;` |
|         - |  530 | `			int nLen;` |
|         - |  531 | `			sxu32 nName;` |
|        16 |  532 | `			zName = ph7_value_to_string(apArg[0],&nLen);` |
|        16 |  533 | `			nName = (sxu32)nLen;` |
|        16 |  534 | `			PH7_VmClassNameAnchor(&zName,&nName);` |
|        16 |  535 | `			if( nName > 0 ){` |
|        16 |  536 | `				SyHashEntry *pEntry = PH7_VmClassEntry(pCtx->pVm,zName,nName);` |
|        16 |  537 | `				if( pEntry ){` |
|        10 |  538 | `					pClass = (ph7_class *)pEntry->pUserData;` |
|        12 |  539 | `					while( pClass && (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|         3 |  540 | `						pClass = pClass->pNextName;` |
|         1 |  541 | `					}` |
|         4 |  542 | `				}` |
|         7 |  543 | `			}` |
|         9 |  544 | `		}else{` |
|        28 |  545 | `			pClass = VmExtractEnumClass(pCtx->pVm,apArg[0]);` |
|         - |  546 | `		}` |
|        19 |  547 | `	}` |
|        42 |  548 | `	ph7_result_bool(pCtx,pClass != 0);` |
|        42 |  549 | `	return SXRET_OK;` |
|         3 |  550 | `}` |
|      1924 |  551 | `PH7_PRIVATE int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 |  552 | `{` |
|         - |  553 | `	SyHashEntry *pEntry;` |
|         - |  554 | `	ph7_constant *pCons;` |
|         - |  555 | `	const char *zName; /* Constant name */` |
|         - |  556 | `	ph7_value sVal;    /* Constant value */` |
|         - |  557 | `	int nLen;` |
|      1929 |  558 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|         - |  559 | `		/* Invallid argument,return NULL */` |
|       ! 0 |  560 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|       ! 0 |  561 | `		ph7_result_null(pCtx);` |
|       ! 0 |  562 | `		return SXRET_OK;` |
|         - |  563 | `	}` |
|         - |  564 | `	/* Extract the constant name */` |
|      1929 |  565 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|         - |  566 | `	/* Class-constant form "C::K" (band A #4): resolve the class — interfaces` |
|         - |  567 | `	 * included — and read the mounted constant slot; php throws a catchable` |
|         - |  568 | `	 * Error for an unknown class or constant (pre-fix this path warned` |
|         - |  569 | `	 * "Undefined constant" and returned NULL without ever looking at the` |
|         - |  570 | `` 	 * class). The resolution is defined()'s: it also answers `self`/`parent`/`static` `` |
|         - |  571 | `	 * against the live class scope and refuses a constant that is not VISIBLE from` |
|         - |  572 | ``	 * here — both of which this used to walk straight past, so a `private const` was`` |
|         - |  573 | ``	 * readable from anywhere through the string form while the direct `C::K` access`` |
|         - |  574 | `	 * threw. */` |
|         - |  575 | `	{` |
|      1929 |  576 | `		ph7_class_attr *pAttr = 0;` |
|      1929 |  577 | `		ph7_class *pClass = 0;` |
|      1929 |  578 | `		int iSep = 0;` |
|      1929 |  579 | `		int iRc = VmClassConstLookup(pCtx->pVm,zName,nLen,&pClass,&pAttr,&iSep);` |
|      1929 |  580 | `		if( iRc != VM_CCONST_PLAIN ){` |
|       148 |  581 | `			if( iRc != VM_CCONST_OK ){` |
|        98 |  582 | `				return VmClassConstError(pCtx,iRc,zName,nLen,iSep,pAttr);` |
|         - |  583 | `			}` |
|       102 |  584 | `			if( pAttr->nIdx == SXU32_HIGH ){` |
|         - |  585 | `				/* Unmaterialized: enum case → materialize the singletons` |
|         - |  586 | `				 * (all of them: constant("S::A") is a direct access, like` |
|         - |  587 | `				 * OP_MEMBER); plain constant → run its initializer. Unlike` |
|         - |  588 | `				 * defined(), reading the value has to force this. */` |
|         - |  589 | `				sxi32 rcEnum;` |
|        48 |  590 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE ){` |
|         3 |  591 | `					rcEnum = VmEnumMaterialize(pCtx->pVm,pClass);` |
|         2 |  592 | `				}else{` |
|        46 |  593 | `					rcEnum = VmClassConstEvalOnDemand(pCtx->pVm,pClass,pAttr);` |
|         - |  594 | `				}` |
|        48 |  595 | `				if( rcEnum != SXRET_OK ){` |
|         3 |  596 | `					return rcEnum;` |
|         - |  597 | `				}` |
|        22 |  598 | `			}` |
|         - |  599 | `			{` |
|       100 |  600 | `				ph7_value *pValue = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj,pAttr->nIdx);` |
|       100 |  601 | `				if( pValue ){` |
|       100 |  602 | `					if( SySetUsed(&pAttr->aAttrs) > 0 ){` |
|         - |  603 | `						/* #[\Deprecated] warns through constant() too (php) */` |
|         3 |  604 | `						VmDeprecatedConstNotice(pCtx->pVm,pClass,pAttr);` |
|         1 |  605 | `					}` |
|       100 |  606 | `					ph7_result_value(pCtx,pValue);` |
|       100 |  607 | `					return SXRET_OK;` |
|         - |  608 | `				}` |
|         - |  609 | `			}` |
|         - |  610 | `			/* Declared but with no slot to read: the same dead end the pre-fix code` |
|         - |  611 | `			 * fell through to. */` |
|       ! 0 |  612 | `			return VmClassConstError(pCtx,VM_CCONST_NOCONST,zName,nLen,iSep,pAttr);` |
|         - |  613 | `		}` |
|         - |  614 | `	}` |
|         - |  615 | `	/* Perform the query */` |
|      1782 |  616 | `	pEntry = PH7_VmConstantFetch(pCtx->pVm,zName,(sxu32)nLen,1);` |
|      1782 |  617 | `	if( pEntry == 0 ){` |
|         - |  618 | `		/* php 8: a catchable Error, not a notice + NULL (band A #4) */` |
|         8 |  619 | `		return PH7_VmThrowException(pCtx,"Error",` |
|         2 |  620 | `			"Undefined constant \"%.*s\"",nLen,zName);` |
|         - |  621 | `	}` |
|      1778 |  622 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|         - |  623 | `	/* Point to the structure that describe the constant */` |
|      1778 |  624 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|         - |  625 | `	/* Extract constant value by calling it's associated callback` |
|         - |  626 | `	 * (emits the #[\Deprecated] notice first when attributed, php) */` |
|      1778 |  627 | `	VmExpandConstantWithNotice(pCtx->pVm,pCons,&sVal);` |
|         - |  628 | `	/* Return that value */` |
|      1778 |  629 | `	ph7_result_value(pCtx,&sVal);` |
|         - |  630 | `	/* Cleanup */` |
|      1778 |  631 | `	PH7_MemObjRelease(&sVal);` |
|      1778 |  632 | `	return SXRET_OK;` |
|       967 |  633 | `}` |
|         - |  634 | `/*` |
|         - |  635 | ` * Hash walker callback used by the [get_defined_constants()] function defined` |
|         - |  636 | ` * below. php's answer is a MAP -- the constant's name is the KEY and its VALUE` |
|         - |  637 | ``  * is the element -- which is what makes `get_defined_constants()['PHP_EOL']` `` |
|         - |  638 | ` * the documented way to read one. PHL used to answer a LIST of names, so every` |
|         - |  639 | ``  * such lookup was an `Undefined array key` and NULL, and `in_array($n, $c)` `` |
|         - |  640 | `` * answered where php wants `isset($c[$n])`: the array had the right length and`` |
|         - |  641 | ` * the wrong shape.` |
|         - |  642 | ` */` |
|    158184 |  643 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|         5 |  644 | `{` |
|         - |  645 | ``	/* SNAPSHOT ONLY -- nothing is expanded during the walk. A `const` whose`` |
|         - |  646 | `	 * initializer is a bytecode program runs USER CODE when it expands, and user` |
|         - |  647 | ``	 * code can `define()`: that grows hConstant while SyHashForEach is holding a`` |
|         - |  648 | `	 * fixed entry count, and the walk then runs off the end of the bucket chain` |
|         - |  649 | `	 * (a segfault, reproducible from a const initializer that constructs an` |
|         - |  650 | `	 * object whose __construct defines a constant). Collect first, expand after. */` |
|    158189 |  651 | `	SySet *pOut = (SySet *)pUserData;` |
|    158189 |  652 | `	if( pEntry == 0 \|\| pEntry->pUserData == 0 ){` |
|       ! 0 |  653 | `		return SXRET_OK;` |
|         - |  654 | `	}` |
|    158189 |  655 | `	SySetPut(pOut,(const void *)&pEntry);` |
|    158189 |  656 | `	return SXRET_OK;` |
|     76837 |  657 | `}` |
|         - |  658 | `/*` |
|         - |  659 | ` * Add one snapshotted constant to the answer, under its name.` |
|         - |  660 | ` */` |
|    158472 |  661 | `static sxi32 VmConstDumpEntry(ph7_value *pTarget,SyHashEntry *pEntry)` |
|         5 |  662 | `{` |
|    158477 |  663 | `	ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|         - |  664 | `	ph7_value sName,sVal;` |
|         - |  665 | `	sxi32 rc;` |
|         - |  666 | `	/* Prepare the constant name for insertion */` |
|    158477 |  667 | `	PH7_MemObjInitFromString(pTarget->pVm,&sName,0);` |
|         - |  668 | `	/* ...under the spelling it was DECLARED with. The entry KEY is the folded` |
|         - |  669 | ``	 * one hConstant matches on (PH7_VmConstantFetch); php lists `Aa\Bb\DEE`. */`` |
|    158477 |  670 | `	PH7_MemObjStringAppend(&sName,SyStringData(&pCons->sName),SyStringLength(&pCons->sName));` |
|         - |  671 | `	/* ...and its VALUE, through the SAME evaluate-once path an ordinary read` |
|         - |  672 | ``	 * takes -- so a `const C = new Foo();` reported here is the object the`` |
|         - |  673 | ``	 * program itself sees, not a second one. The `#[\Deprecated]` NOTICE is what`` |
|         - |  674 | `	 * is skipped (VmExpandConstantOnce rather than …WithNotice): describing a` |
|         - |  675 | `	 * constant is not reading one, and php raises nothing here either. */` |
|    158477 |  676 | `	PH7_MemObjInit(pTarget->pVm,&sVal);` |
|    158477 |  677 | `	rc = VmExpandConstantOnce(pTarget->pVm,pCons,&sVal);` |
|    158477 |  678 | `	if( rc == SXRET_OK ){` |
|    158477 |  679 | `		ph7_array_add_elem(pTarget,&sName,&sVal); /* Will make its own copy */` |
|     76976 |  680 | `	}` |
|    158477 |  681 | `	PH7_MemObjRelease(&sVal);` |
|    158477 |  682 | `	PH7_MemObjRelease(&sName);` |
|    158477 |  683 | `	return rc;` |
|         5 |  684 | `}` |
|         - |  685 | `/*` |
|         - |  686 | ` * array get_defined_constants(bool $categorize = false)` |
|         - |  687 | ` *  Returns an associative array with the names AND VALUES of all defined` |
|         - |  688 | ` *  constants.` |
|         - |  689 | ` * Parameters` |
|         - |  690 | ` *  $categorize` |
|         - |  691 | ` *   TRUE groups the map one level deeper, by the extension each constant` |
|         - |  692 | ` *   belongs to. This engine has no extension partition (the same limitation` |
|         - |  693 | ` *   ReflectionFunction::getExtensionName() records), so it answers php's two` |
|         - |  694 | `` *   buckets it CAN tell apart: `user` for everything a script defined with`` |
|         - |  695 | `` *   define()/const, and `Core` for the engine's own -- where php would spread`` |
|         - |  696 | ` *   the latter over standard/date/pcre/json/… as well.` |
|         - |  697 | ` * Returns` |
|         - |  698 | ` *  The constants currently defined, name => value.` |
|         - |  699 | ` */` |
|         - |  700 | `/*` |
|         - |  701 | ` * One category of get_defined_constants(true): the constants ONE extension` |
|         - |  702 | ` * registers that this build actually has, expanded in php's own registration` |
|         - |  703 | ` * order. Answers 0 while it is filling; a raising initializer stops the walk` |
|         - |  704 | ` * the way the flat pass does.` |
|         - |  705 | ` */` |
|         - |  706 | `typedef struct VmConstBucket VmConstBucket;` |
|         - |  707 | `struct VmConstBucket {` |
|         - |  708 | `	ph7_vm *pVm;` |
|         - |  709 | `	ph7_value *pOut;` |
|         - |  710 | `	sxi32 rc;` |
|         - |  711 | `};` |
|     34830 |  712 | `static int VmConstBucketStep(const char *zName,int nName,void *pData)` |
|         5 |  713 | `{` |
|     34835 |  714 | `	VmConstBucket *p = (VmConstBucket *)pData;` |
|         - |  715 | `	SyHashEntry *pEntry;` |
|     34835 |  716 | `	if( !PH7_VmInternalNameExists(p->pVm,PH7_EXT_KIND_CONST,zName,nName) ){` |
|      1238 |  717 | `		return 0;` |
|         - |  718 | `	}` |
|     33602 |  719 | `	pEntry = PH7_VmConstantFetch(p->pVm,zName,(sxu32)nName,1);` |
|     33602 |  720 | `	if( pEntry == 0 ){` |
|       ! 0 |  721 | `		return 0;` |
|         - |  722 | `	}` |
|     33602 |  723 | `	p->rc = VmConstDumpEntry(p->pOut,pEntry);` |
|     33602 |  724 | `	return p->rc == SXRET_OK ? 0 : 1;` |
|     17420 |  725 | `}` |
|        80 |  726 | `PH7_PRIVATE int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 |  727 | `{` |
|        85 |  728 | `	ph7_value *pArray,*pAll,*pUser = 0;` |
|         - |  729 | `	ph7_value *apBucket[PH7_EXT_MAX];   /* one per extension; filled below when categorizing */` |
|         - |  730 | `	SySet aSnap;` |
|         - |  731 | `	SyHashEntry **apEntry;` |
|         - |  732 | `	sxu32 n,nSnap;` |
|        85 |  733 | `	int iExt,nExt = 0;` |
|        85 |  734 | `	int bCategorize = nArg > 0 && ph7_value_to_bool(apArg[0]);` |
|      5205 |  735 | `	for( iExt = 0 ; iExt < PH7_EXT_MAX ; ++iExt ){` |
|      5125 |  736 | `		apBucket[iExt] = 0;` |
|      2565 |  737 | `	}` |
|         - |  738 | `	/* Create the array first*/` |
|        85 |  739 | `	pArray = ph7_context_new_array(pCtx);` |
|        85 |  740 | `	if( pArray == 0 ){` |
|         - |  741 | `		/* Return NULL */` |
|       ! 0 |  742 | `		ph7_result_null(pCtx);` |
|       ! 0 |  743 | `		return SXRET_OK;` |
|         - |  744 | `	}` |
|        85 |  745 | `	pAll = pArray;` |
|        85 |  746 | `	if( bCategorize ){` |
|         - |  747 | `		/* php's categories are the extensions, in the order get_loaded_extensions()` |
|         - |  748 | ``		 * lists them and each in its OWN registration order, with `user` last --`` |
|         - |  749 | `		 * and php OMITS a category with nothing in it. Before this engine had an` |
|         - |  750 | ``		 * extension partition all 1314 answers sat under a single `Core`.`` |
|         - |  751 | `		 *` |
|         - |  752 | `		 * The per-extension pass runs first so that each bucket is in php's` |
|         - |  753 | `		 * order; the table walk that follows only has to place what the` |
|         - |  754 | `		 * partition has no row for, which rides with Core. */` |
|        23 |  755 | `		nExt = PH7_VmExtensionCount();` |
|        23 |  756 | `		if( nExt > PH7_EXT_MAX ){` |
|       ! 0 |  757 | `			nExt = PH7_EXT_MAX;` |
|       ! 0 |  758 | `		}` |
|       653 |  759 | `		for( iExt = 0 ; iExt < nExt ; ++iExt ){` |
|       635 |  760 | `			apBucket[iExt] = PH7_VmExtensionAvailable(iExt) ? ph7_context_new_array(pCtx) : 0;` |
|       320 |  761 | `		}` |
|        23 |  762 | `		pUser = ph7_context_new_array(pCtx);` |
|        23 |  763 | `		pAll = apBucket[PH7_EXT_CORE];` |
|        23 |  764 | `		if( pAll == 0 \|\| pUser == 0 ){` |
|       ! 0 |  765 | `			ph7_result_null(pCtx);` |
|       ! 0 |  766 | `			return SXRET_OK;` |
|         - |  767 | `		}` |
|       653 |  768 | `		for( iExt = 0 ; iExt < nExt ; ++iExt ){` |
|         - |  769 | `			VmConstBucket sBucket;` |
|       635 |  770 | `			if( apBucket[iExt] == 0 ){` |
|         5 |  771 | `				continue;` |
|         - |  772 | `			}` |
|       635 |  773 | `			sBucket.pVm = pCtx->pVm;` |
|       635 |  774 | `			sBucket.pOut = apBucket[iExt];` |
|       635 |  775 | `			sBucket.rc = SXRET_OK;` |
|       635 |  776 | `			pCtx->pVm->bConstEnum++;` |
|       635 |  777 | `			PH7_VmExtWalk(iExt,PH7_EXT_KIND_CONST,VmConstBucketStep,&sBucket);` |
|       635 |  778 | `			pCtx->pVm->bConstEnum--;` |
|       635 |  779 | `			if( sBucket.rc != SXRET_OK ){` |
|       ! 0 |  780 | `				return sBucket.rc;` |
|         - |  781 | `			}` |
|       320 |  782 | `		}` |
|         9 |  783 | `	}` |
|         - |  784 | `	/* Snapshot the table, then expand: expanding runs user code, which may` |
|         - |  785 | `	 * define() and grow the table under the walk (see VmHashConstStep). The` |
|         - |  786 | `	 * walk runs tail to head: hConstant head-pushes, so a forward walk listed` |
|         - |  787 | `	 * the newest constant first where php lists them in definition order. */` |
|        85 |  788 | `	SySetInit(&aSnap,&pCtx->pVm->sAllocator,sizeof(SyHashEntry *));` |
|        85 |  789 | `	SyHashForEachReverse(&pCtx->pVm->hConstant,VmHashConstStep,&aSnap);` |
|        85 |  790 | `	apEntry = (SyHashEntry **)SySetBasePtr(&aSnap);` |
|        85 |  791 | `	nSnap = SySetUsed(&aSnap);` |
|         - |  792 | `	/* Describing the table is not READING its entries: php's deprecated constants` |
|         - |  793 | `	 * report when a program names one, and get_defined_constants() lists them in` |
|         - |  794 | `	 * silence. */` |
|        85 |  795 | `	pCtx->pVm->bConstEnum++;` |
|    158269 |  796 | `	for( n = 0 ; n < nSnap ; ++n ){` |
|    158189 |  797 | `		ph7_constant *pCons = (ph7_constant *)apEntry[n]->pUserData;` |
|         - |  798 | `		sxi32 rcExp;` |
|    158184 |  799 | `		if( bCategorize && !pCons->bUserDefined` |
|     35254 |  800 | `		 && PH7_VmExtHasName(PH7_EXT_KIND_CONST,(const char *)apEntry[n]->pKey,` |
|     35109 |  801 | `				(int)apEntry[n]->nKeyLen) ){` |
|     33314 |  802 | `			continue;   /* the per-extension pass already placed it */` |
|         - |  803 | `		}` |
|    124880 |  804 | `		rcExp = VmConstDumpEntry(pUser && pCons->bUserDefined ? pUser : pAll,apEntry[n]);` |
|    124880 |  805 | `		if( rcExp != SXRET_OK ){` |
|         - |  806 | `			/* An initializer raised while being described: stop, exactly as any` |
|         - |  807 | `			 * other builtin does when the php it invoked did not return.` |
|         - |  808 | `			 * Carrying on would run every LATER initializer past a throw that has` |
|         - |  809 | `			 * already been landed. */` |
|       ! 0 |  810 | `			pCtx->pVm->bConstEnum--;` |
|       ! 0 |  811 | `			SySetRelease(&aSnap);` |
|       ! 0 |  812 | `			return rcExp;` |
|         - |  813 | `		}` |
|     60691 |  814 | `	}` |
|        85 |  815 | `	pCtx->pVm->bConstEnum--;` |
|        85 |  816 | `	SySetRelease(&aSnap);` |
|        85 |  817 | `	if( bCategorize ){` |
|       653 |  818 | `		for( iExt = 0 ; iExt < nExt ; ++iExt ){` |
|       635 |  819 | `			if( apBucket[iExt] == 0 ){` |
|         5 |  820 | `				continue;` |
|         - |  821 | `			}` |
|       635 |  822 | `			if( ph7_array_count(apBucket[iExt]) > 0 ){` |
|       437 |  823 | `				ph7_array_add_strkey_elem(pArray,PH7_VmExtensionName(iExt),apBucket[iExt]);` |
|       216 |  824 | `			}` |
|       635 |  825 | `			ph7_context_release_value(pCtx,apBucket[iExt]);` |
|       320 |  826 | `		}` |
|        23 |  827 | `		if( ph7_array_count(pUser) > 0 ){` |
|        21 |  828 | `			ph7_array_add_strkey_elem(pArray,"user",pUser);` |
|         8 |  829 | `		}` |
|        23 |  830 | `		ph7_context_release_value(pCtx,pUser);` |
|         9 |  831 | `	}` |
|         - |  832 | `	/* Return the created array */` |
|        85 |  833 | `	ph7_result_value(pCtx,pArray);` |
|        85 |  834 | `	return SXRET_OK;` |
|        45 |  835 | `}` |
|         - |  836 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|         - |  837 | `/*` |
|         - |  838 | ` * Section:` |
|         - |  839 | ` *  Random numbers/string generators.` |
|         - |  840 | ` * Status:` |
|         - |  841 | ` *    Stable.` |
|         - |  842 | ` */` |
|         - |  843 | `/*` |
|         - |  844 | ` * Generate a random 32-bit unsigned integer.` |
|         - |  845 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|         - |  846 | ` * implemented in src/sx/sxrand.c).` |
|         - |  847 | ` */` |
|        68 |  848 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|         2 |  849 | `{` |
|         - |  850 | `	sxu32 iNum;` |
|        70 |  851 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|        70 |  852 | `	return iNum;` |
|         2 |  853 | `}` |
|         - |  854 | `/*` |
|         - |  855 | ` * The MT19937 generator that backs PHP's rand()/mt_rand() family. It is kept` |
|         - |  856 | ` * separate from the RC4 SyRandomness above so that srand()/mt_srand() give` |
|         - |  857 | ` * userland PHP's reproducible sequence without perturbing the engine's internal` |
|         - |  858 | ` * entropy (object ids, uniqid, quicksort pivots stay on the RC4 generator, as` |
|         - |  859 | ` * they are in PHP too — srand does not touch those).` |
|         - |  860 | ` */` |
|         - |  861 | `/*` |
|         - |  862 | ` * Reset the MT19937 state to a 32-bit seed (PHP truncates its int seed likewise).` |
|         - |  863 | ` */` |
|       314 |  864 | `PH7_PRIVATE void PH7_VmMtSrand(ph7_vm *pVm,sxu32 nSeed,int bLegacyTwist)` |
|         3 |  865 | `{` |
|       317 |  866 | `	SyMT19937Seed(&pVm->sMt,nSeed,bLegacyTwist);` |
|       317 |  867 | `	pVm->mtSeeded = TRUE;` |
|       317 |  868 | `}` |
|         - |  869 | `/*` |
|         - |  870 | ` * Draw the next full 32-bit MT19937 word, seeding lazily from the OS CSPRNG on` |
|         - |  871 | ` * first use exactly as PHP auto-seeds when rand()/mt_rand() runs before srand().` |
|         - |  872 | ` */` |
|      2582 |  873 | `PH7_PRIVATE sxu32 PH7_VmMtRand(ph7_vm *pVm)` |
|         3 |  874 | `{` |
|      2585 |  875 | `	if( !pVm->mtSeeded ){` |
|         - |  876 | `		sxu32 nSeed;` |
|         3 |  877 | `		if( SyOSCSPRNG((void *)&nSeed,sizeof(nSeed)) != SXRET_OK ){` |
|         - |  878 | `			/* No OS entropy source: fall back to the RC4 generator's output. */` |
|       ! 0 |  879 | `			nSeed = PH7_VmRandomNum(pVm);` |
|       ! 0 |  880 | `		}` |
|         - |  881 | `		/* An un-seeded generator is php's default one, never MT_RAND_PHP. */` |
|         3 |  882 | `		SyMT19937Seed(&pVm->sMt,nSeed,FALSE);` |
|         3 |  883 | `		pVm->mtSeeded = TRUE;` |
|         1 |  884 | `	}` |
|      2585 |  885 | `	return SyMT19937Next(&pVm->sMt);` |
|         3 |  886 | `}` |
|         - |  887 | `/*` |
|         - |  888 | ` * Map a full 32-bit draw uniformly into [0,uMax] (uMax is the range width, i.e.` |
|         - |  889 | ` * max-min). Rejection sampling against the largest unbiased ceiling, matching` |
|         - |  890 | ` * PHP's php_random_range32().` |
|         - |  891 | ` */` |
|      2378 |  892 | `static sxu32 VmMtRange32(ph7_vm *pVm,sxu32 uMax)` |
|         3 |  893 | `{` |
|         - |  894 | `	sxu32 result,limit;` |
|      2381 |  895 | `	result = PH7_VmMtRand(pVm);` |
|         - |  896 | `	/* Whole 32-bit domain: no scaling needed. */` |
|      2381 |  897 | `	if( uMax == 0xFFFFFFFFU ){` |
|       ! 0 |  898 | `		return result;` |
|         - |  899 | `	}` |
|         - |  900 | `	/* Make the range inclusive of max. */` |
|      2381 |  901 | `	uMax++;` |
|         - |  902 | `	/* Powers of two are unbiased under a plain mask. */` |
|      2381 |  903 | `	if( (uMax & (uMax - 1)) == 0 ){` |
|        87 |  904 | `		return result & (uMax - 1);` |
|         - |  905 | `	}` |
|         - |  906 | `	/* Ceiling under which 0xFFFFFFFF % uMax == 0; discard draws above it. */` |
|      2297 |  907 | `	limit = 0xFFFFFFFFU - (0xFFFFFFFFU % uMax) - 1;` |
|      2297 |  908 | `	while( result > limit ){` |
|       ! 0 |  909 | `		result = PH7_VmMtRand(pVm);` |
|       ! 0 |  910 | `	}` |
|      2297 |  911 | `	return result % uMax;` |
|      1192 |  912 | `}` |
|         - |  913 | `/*` |
|         - |  914 | ` * 64-bit-wide range: assemble two draws (high word first, as PHP does) and` |
|         - |  915 | ` * reject-sample. Matches PHP's php_random_range64().` |
|         - |  916 | ` */` |
|        14 |  917 | `static sxu64 VmMtRange64(ph7_vm *pVm,sxu64 uMax)` |
|         2 |  918 | `{` |
|         - |  919 | `	sxu64 result,limit;` |
|         - |  920 | `	/* First draw fills the low word, second draw the high word — order is` |
|         - |  921 | `	 * significant and matches php's php_random_range64() assembly. */` |
|        16 |  922 | `	result = (sxu64)PH7_VmMtRand(pVm);` |
|        16 |  923 | `	result \|= (sxu64)PH7_VmMtRand(pVm) << 32;` |
|        16 |  924 | `	if( uMax == 0xFFFFFFFFFFFFFFFFULL ){` |
|       ! 0 |  925 | `		return result;` |
|         - |  926 | `	}` |
|        16 |  927 | `	uMax++;` |
|        16 |  928 | `	if( (uMax & (uMax - 1)) == 0 ){` |
|        14 |  929 | `		return result & (uMax - 1);` |
|         - |  930 | `	}` |
|         3 |  931 | `	limit = 0xFFFFFFFFFFFFFFFFULL - (0xFFFFFFFFFFFFFFFFULL % uMax) - 1;` |
|         3 |  932 | `	while( result > limit ){` |
|       ! 0 |  933 | `		result = (sxu64)PH7_VmMtRand(pVm);` |
|       ! 0 |  934 | `		result \|= (sxu64)PH7_VmMtRand(pVm) << 32;` |
|       ! 0 |  935 | `	}` |
|         3 |  936 | `	return result % uMax;` |
|         9 |  937 | `}` |
|         - |  938 | `/*` |
|         - |  939 | ` * Return a value uniformly in the inclusive range [iMin,iMax]. The caller` |
|         - |  940 | ` * guarantees iMin <= iMax. Mirrors PHP's php_mt_rand_range(): a range that fits` |
|         - |  941 | ` * in 32 bits takes the 32-bit path, a wider one the 64-bit path.` |
|         - |  942 | ` */` |
|      2392 |  943 | `PH7_PRIVATE sxi64 PH7_VmMtRandRange(ph7_vm *pVm,sxi64 iMin,sxi64 iMax)` |
|         3 |  944 | `{` |
|      2395 |  945 | `	sxu64 uMax = (sxu64)iMax - (sxu64)iMin;` |
|      2395 |  946 | `	if( uMax > 0xFFFFFFFFULL ){` |
|        16 |  947 | `		return (sxi64)(VmMtRange64(pVm,uMax) + (sxu64)iMin);` |
|         - |  948 | `	}` |
|      2381 |  949 | `	return (sxi64)((sxu64)VmMtRange32(pVm,(sxu32)uMax) + (sxu64)iMin);` |
|      1199 |  950 | `}` |
|         - |  951 | `/*` |
|         - |  952 | ` * Generate a random string (English Alphabet) of length nLen.` |
|         - |  953 | ` * Note that the generated string is NOT null terminated.` |
|         - |  954 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|         - |  955 | ` * implemented in src/sx/sxrand.c).` |
|         - |  956 | ` */` |
|  13367391 |  957 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|         5 |  958 | `{` |
|         - |  959 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|         - |  960 | `	int i;` |
|         - |  961 | `	/* Generate a binary string first */` |
|  13367396 |  962 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|         - |  963 | `	/* Turn the binary string into english based alphabet */` |
| 147042736 |  964 | `	for( i = 0 ; i < nLen ; ++i ){` |
| 133675345 |  965 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|  66750659 |  966 | `	 }` |
|  13367396 |  967 | `}` |
|         - |  968 | `/*` |
|         - |  969 | ` * int rand()` |
|         - |  970 | ` * int mt_rand()` |
|         - |  971 | ` * int rand(int $min,int $max)` |
|         - |  972 | ` * int mt_rand(int $min,int $max)` |
|         - |  973 | ` *  Generate a random (unsigned 32-bit) integer.` |
|         - |  974 | ` * Parameter` |
|         - |  975 | ` *  $min` |
|         - |  976 | ` *    The lowest value to return (default: 0)` |
|         - |  977 | ` *  $max` |
|         - |  978 | ` *   The highest value to return (default: getrandmax())` |
|         - |  979 | ` * Return` |
|         - |  980 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|         - |  981 | ` * Note:` |
|         - |  982 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|         - |  983 | ` *  by te SQLite3 library.` |
|         - |  984 | ` */` |
|      1894 |  985 | `PH7_PRIVATE int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 |  986 | `{` |
|      1896 |  987 | `	SyString *pName = &pCtx->pFunc->sName;` |
|      3348 |  988 | `	int bMt = (pName->nByte == sizeof("mt_rand")-1` |
|      1894 |  989 | `		&& SyMemcmp(pName->zString,"mt_rand",sizeof("mt_rand")-1) == 0);` |
|         - |  990 | `	/* php accepts exactly 0 or exactly 2 arguments (min,max); 1 or 3+ is an` |
|         - |  991 | `	 * ArgumentCountError. The central arity table can't express "0 or 2", so` |
|         - |  992 | `	 * it is enforced here (was a silent wrong result for the raw draw). */` |
|      1896 |  993 | `	if( nArg == 1 \|\| nArg > 2 ){` |
|        13 |  994 | `		return PH7_VmThrowException(pCtx,` |
|         - |  995 | `			"ArgumentCountError",` |
|         - |  996 | `			"%z() expects exactly 2 arguments, %d given",` |
|         4 |  997 | `			pName, nArg` |
|         - |  998 | `			);` |
|         - |  999 | `	}` |
|      1888 | 1000 | `	if( nArg == 2 ){` |
|         - | 1001 | `		sxi64 iMin,iMax;` |
|         - | 1002 | `		/* Signed 64-bit endpoints: the old unsigned math wrapped negative` |
|         - | 1003 | `		 * ranges to huge positives (rand(-10,-1) -> ~4e9) and mis-handled` |
|         - | 1004 | `		 * min==max. */` |
|      1776 | 1005 | `		iMin = ph7_value_to_int64(apArg[0]);` |
|      1776 | 1006 | `		iMax = ph7_value_to_int64(apArg[1]);` |
|      1776 | 1007 | `		if( iMin > iMax ){` |
|         9 | 1008 | `			if( bMt ){` |
|         - | 1009 | `				/* mt_rand() is strict: php throws a catchable ValueError. */` |
|         5 | 1010 | `				return PH7_VmThrowException(pCtx,` |
|         - | 1011 | `					"ValueError",` |
|         - | 1012 | `					"mt_rand(): Argument #2 ($max) must be greater than or equal to argument #1 ($min)"` |
|         - | 1013 | `					);` |
|         - | 1014 | `			}` |
|         - | 1015 | `			/* rand() swaps the bounds for backward compatibility (php keeps` |
|         - | 1016 | `			 * this quirk; only mt_rand() rejects a reversed range). */` |
|         5 | 1017 | `			{ sxi64 iTmp = iMin; iMin = iMax; iMax = iTmp; }` |
|         2 | 1018 | `		}` |
|      1772 | 1019 | `		if( pCtx->pVm->sMt.bLegacyTwist ){` |
|         - | 1020 | `			/* MT_RAND_PHP is a whole generator, mapping included: php keeps its old` |
|         - | 1021 | `			 * RAND_RANGE_BADSCALING here — a 31-bit draw scaled through a double,` |
|         - | 1022 | `			 * which is biased and is exactly what the recorded sequence a caller` |
|         - | 1023 | `			 * asked for was produced with. */` |
|        65 | 1024 | `			double rNum = (double)(PH7_VmMtRand(pCtx->pVm) >> 1);` |
|        65 | 1025 | `			double rSpan = (double)iMax - (double)iMin + 1.0;` |
|         - | 1026 | `			/* php's own arithmetic, types included: the product lands in an` |
|         - | 1027 | `			 * unsigned 64-bit result, so a span wider than the SIGNED range keeps` |
|         - | 1028 | `			 * its value and wraps around the minimum rather than saturating. The` |
|         - | 1029 | `			 * product is never negative (the span is at least 1, the fraction at` |
|         - | 1030 | `			 * least 0), so the unsigned conversion is total. */` |
|        65 | 1031 | `			sxu64 uOut = (sxu64)iMin + (sxu64)(rSpan * (rNum / (2147483647.0 + 1.0)));` |
|        65 | 1032 | `			ph7_result_int64(pCtx,(sxi64)uOut);` |
|        65 | 1033 | `			return SXRET_OK;` |
|         - | 1034 | `		}` |
|         - | 1035 | `		/* MT19937-backed uniform draw over [iMin,iMax], bit-for-bit as php. */` |
|      1708 | 1036 | `		ph7_result_int64(pCtx,PH7_VmMtRandRange(pCtx->pVm,iMin,iMax));` |
|      1708 | 1037 | `		return SXRET_OK;` |
|         - | 1038 | `	}` |
|         - | 1039 | `	/* No-argument form: a 31-bit value in [0, mt_getrandmax()]. php returns` |
|         - | 1040 | `	 * php_mt_rand() >> 1 for the bare draw (the full 32-bit word feeds the` |
|         - | 1041 | `	 * range form above, but the bare form drops the low bit). */` |
|       114 | 1042 | `	ph7_result_int64(pCtx,(sxi64)(PH7_VmMtRand(pCtx->pVm) >> 1));` |
|       114 | 1043 | `	return SXRET_OK;` |
|       949 | 1044 | `}` |
|         - | 1045 | `/*` |
|         - | 1046 | ` * int getrandmax(void)` |
|         - | 1047 | ` * int mt_getrandmax(void)` |
|         - | 1048 | ` * int rc4_getrandmax(void)` |
|         - | 1049 | ` *   Show largest possible random value` |
|         - | 1050 | ` * Return` |
|         - | 1051 | ` *  The largest possible random value returned by rand()/mt_rand(): php's` |
|         - | 1052 | ` *  MT19937 backing makes this 2^31-1 (2147483647) for both.` |
|         - | 1053 | ` */` |
|         8 | 1054 | `PH7_PRIVATE int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 1055 | `{` |
|         4 | 1056 | `	SXUNUSED(nArg); /* cc warning */` |
|         4 | 1057 | `	SXUNUSED(apArg);` |
|         - | 1058 | `	/* php: PHP_MT_RAND_MAX == (1<<31)-1; bare rand()/mt_rand() draw >> 1 lands` |
|         - | 1059 | `	 * exactly in [0, this]. */` |
|         9 | 1060 | `	ph7_result_int64(pCtx,2147483647);` |
|         9 | 1061 | `	return SXRET_OK;` |
|         1 | 1062 | `}` |
|         - | 1063 | `/*` |
|         - | 1064 | ` * string rand_str()` |
|         - | 1065 | ` * string rand_str(int $len)` |
|         - | 1066 | ` *  Generate a random string (English alphabet).` |
|         - | 1067 | ` * Parameter` |
|         - | 1068 | ` *  $len` |
|         - | 1069 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|         - | 1070 | ` * Return` |
|         - | 1071 | ` *   A pseudo random string.` |
|         - | 1072 | ` * Note:` |
|         - | 1073 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|         - | 1074 | ` *  by te SQLite3 library.` |
|         - | 1075 | ` *  This function is a symisc extension.` |
|         - | 1076 | ` */` |
|       715 | 1077 | `PH7_PRIVATE int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 1078 | `{` |
|         - | 1079 | `	char zString[1024];` |
|       720 | 1080 | `	int iLen = 0x10;` |
|       720 | 1081 | `	if( nArg > 0 ){` |
|         - | 1082 | `		/* Get the desired length */` |
|       720 | 1083 | `		iLen = ph7_value_to_int(apArg[0]);` |
|       720 | 1084 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|         - | 1085 | `			/* Default length */` |
|         3 | 1086 | `			iLen = 0x10;` |
|         1 | 1087 | `		}` |
|       357 | 1088 | `	}` |
|         - | 1089 | `	/* Generate the random string */` |
|       720 | 1090 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|         - | 1091 | `	/* Return the generated string */` |
|       720 | 1092 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|       720 | 1093 | `	return SXRET_OK;` |
|         5 | 1094 | `}` |
|         - | 1095 | `/*` |
|         - | 1096 | ` * Reject non-numeric values (array/object/resource and non-numeric strings)` |
|         - | 1097 | ` * the same way intdiv() does. Returns SXRET_OK if the value is acceptable as` |
|         - | 1098 | ` * an int (PHP coerces float and numeric string silently).` |
|         - | 1099 | ` */` |
|       476 | 1100 | `static int VmRandomCheckIntArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgPos,const char *zParamName)` |
|         1 | 1101 | `{` |
|         - | 1102 | `	char zGiven[64];` |
|       476 | 1103 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg)` |
|       477 | 1104 | `		\|\| ph7_value_is_resource(pArg) ){` |
|       ! 0 | 1105 | `		return PH7_VmThrowException(pCtx,` |
|         - | 1106 | `			"TypeError",` |
|         - | 1107 | `			"%s(): Argument #%d (%s) must be of type int, %s given",` |
|       ! 0 | 1108 | `			zFunc,iArgPos,zParamName,` |
|       ! 0 | 1109 | `			VmValueGivenName(pArg,zGiven,sizeof(zGiven))` |
|         - | 1110 | `			);` |
|         - | 1111 | `	}` |
|       477 | 1112 | `	if( ph7_value_is_string(pArg) ){` |
|         - | 1113 | `		int len;` |
|         5 | 1114 | `		const char *zStr = ph7_value_to_string(pArg, &len);` |
|         5 | 1115 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|       ! 0 | 1116 | `			return PH7_VmThrowException(pCtx,` |
|         - | 1117 | `				"TypeError",` |
|         - | 1118 | `				"%s(): Argument #%d (%s) must be of type int, string given",` |
|       ! 0 | 1119 | `				zFunc,iArgPos,zParamName` |
|         - | 1120 | `				);` |
|         - | 1121 | `		}` |
|         2 | 1122 | `	}` |
|       477 | 1123 | `	return SXRET_OK;` |
|       239 | 1124 | `}` |
|         - | 1125 | `/*` |
|         - | 1126 | ` * int random_int(int $min, int $max)` |
|         - | 1127 | ` *  Generate a cryptographically secure pseudo-random integer in [$min, $max].` |
|         - | 1128 | ` *  Mirrors PHP 7.0+ random_int(). Uses the OS CSPRNG via SyOSCSPRNG().` |
|         - | 1129 | ` *  Distribution is uniform via rejection sampling against the smallest` |
|         - | 1130 | ` *  power-of-two mask covering the range.` |
|         - | 1131 | ` */` |
|       230 | 1132 | `PH7_PRIVATE int vm_builtin_random_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 1133 | `{` |
|         - | 1134 | `	sxi64 iMin,iMax;` |
|         - | 1135 | `	sxu64 uRange,uMask,uResult;` |
|         - | 1136 | `	unsigned int nAttempt;` |
|         - | 1137 | `	int rc;` |
|       231 | 1138 | `	if( nArg != 2 ){` |
|       ! 0 | 1139 | `		return PH7_VmThrowException(pCtx,` |
|         - | 1140 | `			"ArgumentCountError",` |
|         - | 1141 | `			"random_int() expects exactly 2 arguments, %d given",` |
|       ! 0 | 1142 | `			nArg` |
|         - | 1143 | `			);` |
|         - | 1144 | `	}` |
|       231 | 1145 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_int",1,"$min");` |
|       231 | 1146 | `	if( rc != SXRET_OK ){ return rc; }` |
|       231 | 1147 | `	rc = VmRandomCheckIntArg(pCtx,apArg[1],"random_int",2,"$max");` |
|       231 | 1148 | `	if( rc != SXRET_OK ){ return rc; }` |
|       231 | 1149 | `	iMin = ph7_value_to_int64(apArg[0]);` |
|       231 | 1150 | `	iMax = ph7_value_to_int64(apArg[1]);` |
|       231 | 1151 | `	if( iMin > iMax ){` |
|         3 | 1152 | `		return PH7_VmThrowException(pCtx,` |
|         - | 1153 | `			"ValueError",` |
|         - | 1154 | `			"random_int(): Argument #1 ($min) must be less than or equal to argument #2 ($max)"` |
|         - | 1155 | `			);` |
|         - | 1156 | `	}` |
|       229 | 1157 | `	if( iMin == iMax ){` |
|         5 | 1158 | `		ph7_result_int64(pCtx,iMin);` |
|         5 | 1159 | `		return SXRET_OK;` |
|         - | 1160 | `	}` |
|       225 | 1161 | `	uRange = (sxu64)iMax - (sxu64)iMin;` |
|       225 | 1162 | `	uMask = uRange;` |
|       225 | 1163 | `	uMask \|= uMask >> 1;` |
|       225 | 1164 | `	uMask \|= uMask >> 2;` |
|       225 | 1165 | `	uMask \|= uMask >> 4;` |
|       225 | 1166 | `	uMask \|= uMask >> 8;` |
|       225 | 1167 | `	uMask \|= uMask >> 16;` |
|       225 | 1168 | `	uMask \|= uMask >> 32;` |
|       225 | 1169 | `	uResult = 0;` |
|       360 | 1170 | `	for( nAttempt = 0 ; nAttempt < 50 ; ++nAttempt ){` |
|         - | 1171 | `		/* Always draw a full 8 bytes so endianness of the cast doesn't matter` |
|         - | 1172 | `		 * (a 4-byte fill into a sxu64 would land in the high half on big-endian` |
|         - | 1173 | `		 * and the low-half mask would always read 0). */` |
|         - | 1174 | `		sxu64 uDraw;` |
|       360 | 1175 | `		if( SyOSCSPRNG(&uDraw,sizeof(uDraw)) != SXRET_OK ){` |
|       ! 0 | 1176 | `			return PH7_VmThrowException(pCtx,` |
|         - | 1177 | `				"Random\\RandomException",` |
|         - | 1178 | `				"Cannot gather sufficient random data"` |
|         - | 1179 | `				);` |
|         - | 1180 | `		}` |
|       360 | 1181 | `		uDraw &= uMask;` |
|       360 | 1182 | `		if( uDraw <= uRange ){` |
|       225 | 1183 | `			uResult = uDraw;` |
|       225 | 1184 | `			break;` |
|         - | 1185 | `		}` |
|        76 | 1186 | `	}` |
|       225 | 1187 | `	if( nAttempt >= 50 ){` |
|       ! 0 | 1188 | `		return PH7_VmThrowException(pCtx,` |
|         - | 1189 | `			"Random\\RandomException",` |
|         - | 1190 | `			"Cannot gather sufficient random data"` |
|         - | 1191 | `			);` |
|         - | 1192 | `	}` |
|       225 | 1193 | `	ph7_result_int64(pCtx,(sxi64)((sxu64)iMin + uResult));` |
|       225 | 1194 | `	return SXRET_OK;` |
|       116 | 1195 | `}` |
|         - | 1196 | `/*` |
|         - | 1197 | ` * string random_bytes(int $length)` |
|         - | 1198 | ` *  Generate $length cryptographically secure random bytes via SyOSCSPRNG().` |
|         - | 1199 | ` *  Mirrors PHP 7.0+ random_bytes().` |
|         - | 1200 | ` */` |
|        16 | 1201 | `PH7_PRIVATE int vm_builtin_random_bytes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 1202 | `{` |
|         - | 1203 | `	sxi64 iLen;` |
|         - | 1204 | `	unsigned char zStack[256];` |
|         - | 1205 | `	void *pBuf;` |
|         - | 1206 | `	int rc;` |
|        17 | 1207 | `	int bHeap = 0;` |
|        17 | 1208 | `	if( nArg != 1 ){` |
|       ! 0 | 1209 | `		return PH7_VmThrowException(pCtx,` |
|         - | 1210 | `			"ArgumentCountError",` |
|         - | 1211 | `			"random_bytes() expects exactly 1 argument, %d given",` |
|       ! 0 | 1212 | `			nArg` |
|         - | 1213 | `			);` |
|         - | 1214 | `	}` |
|        17 | 1215 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_bytes",1,"$length");` |
|        17 | 1216 | `	if( rc != SXRET_OK ){ return rc; }` |
|        17 | 1217 | `	iLen = ph7_value_to_int64(apArg[0]);` |
|        17 | 1218 | `	if( iLen < 1 ){` |
|         5 | 1219 | `		return PH7_VmThrowException(pCtx,` |
|         - | 1220 | `			"ValueError",` |
|         - | 1221 | `			"random_bytes(): Argument #1 ($length) must be greater than 0"` |
|         - | 1222 | `			);` |
|         - | 1223 | `	}` |
|         - | 1224 | `	/* The PH7 allocator and ph7_result_string both take sxu32/int sizes,` |
|         - | 1225 | `	 * so we can't honor a length above 2 GiB. Reject early rather than` |
|         - | 1226 | `	 * silently truncating via the (sxu32) cast below. */` |
|        13 | 1227 | `	if( iLen > 0x7FFFFFFF ){` |
|       ! 0 | 1228 | `		return PH7_VmThrowException(pCtx,` |
|         - | 1229 | `			"ValueError",` |
|         - | 1230 | `			"random_bytes(): Argument #1 ($length) is too large"` |
|         - | 1231 | `			);` |
|         - | 1232 | `	}` |
|        13 | 1233 | `	if( iLen <= (sxi64)sizeof(zStack) ){` |
|        13 | 1234 | `		pBuf = zStack;` |
|         7 | 1235 | `	}else{` |
|       ! 0 | 1236 | `		pBuf = SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)iLen);` |
|       ! 0 | 1237 | `		if( pBuf == 0 ){` |
|       ! 0 | 1238 | `			return PH7_VmThrowException(pCtx,` |
|         - | 1239 | `				"Exception",` |
|         - | 1240 | `				"random_bytes(): Failed to allocate %qd bytes",` |
|       ! 0 | 1241 | `				iLen` |
|         - | 1242 | `				);` |
|         - | 1243 | `		}` |
|       ! 0 | 1244 | `		bHeap = 1;` |
|         - | 1245 | `	}` |
|        13 | 1246 | `	if( SyOSCSPRNG(pBuf,(sxu32)iLen) != SXRET_OK ){` |
|       ! 0 | 1247 | `		if( bHeap ){` |
|       ! 0 | 1248 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|       ! 0 | 1249 | `		}` |
|       ! 0 | 1250 | `		return PH7_VmThrowException(pCtx,` |
|         - | 1251 | `			"Random\\RandomException",` |
|         - | 1252 | `			"Cannot gather sufficient random data"` |
|         - | 1253 | `			);` |
|         - | 1254 | `	}` |
|        13 | 1255 | `	ph7_result_string(pCtx,(const char *)pBuf,(int)iLen);` |
|        13 | 1256 | `	if( bHeap ){` |
|       ! 0 | 1257 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|       ! 0 | 1258 | `	}` |
|        13 | 1259 | `	return SXRET_OK;` |
|         9 | 1260 | `}` |
|         - | 1261 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 1262 | `/* uniqid() used to be gated on PH7_DISABLE_HASH_FUNC as well, because PH7 built` |
|         - | 1263 | ` * its id out of a SHA1. php's is the clock, so the hash guard has nothing to say` |
|         - | 1264 | ` * about it any more. */` |
|         - | 1265 | `/*` |
|         - | 1266 | `` * php's `php_combined_lcg()`: L'Ecuyer's combined linear congruential generator,`` |
|         - | 1267 | ` * two streams whose difference is the answer.` |
|         - | 1268 | ` *` |
|         - | 1269 | ` * It is not the engine's general randomness -- PH7_VmRandomNum is -- and it is` |
|         - | 1270 | ``  * here for one reason: it is what php's uniqid() puts in the `$more_entropy` `` |
|         - | 1271 | ` * tail, and what its lcg_value() answers. The seed is the clock and the engine's` |
|         - | 1272 | ` * own entropy, taken once, the way php seeds its pair once per process (from the` |
|         - | 1273 | ` * clock and the pid); the STREAM is therefore not reproducible between two runs` |
|         - | 1274 | ` * of either engine, and no test pins its value -- only the SHAPE it lands in.` |
|         - | 1275 | ` */` |
|         - | 1276 | `#define PH7_LCG_MODMULT(a,b,c,m,s) { \` |
|         - | 1277 | `	sxi32 q = (s) / (a); \` |
|         - | 1278 | `	(s) = (b) * ((s) % (a)) - (c) * q; \` |
|         - | 1279 | `	if( (s) < 0 ){ (s) += (m); } \` |
|         - | 1280 | `}` |
|       604 | 1281 | `PH7_PRIVATE double PH7_VmCombinedLcg(ph7_vm *pVm)` |
|         1 | 1282 | `{` |
|         - | 1283 | `	sxi32 z;` |
|       605 | 1284 | `	if( !pVm->bLcgSeeded ){` |
|         3 | 1285 | `		ph7_int64 iSec = 0,iUsec = 0;` |
|         3 | 1286 | `		PH7_VmClockNow(pVm,&iSec,&iUsec);` |
|         3 | 1287 | `		pVm->iLcgS1 = (sxi32)(iSec ^ (iUsec << 11));` |
|         3 | 1288 | `		pVm->iLcgS2 = (sxi32)PH7_VmRandomNum(&(*pVm));` |
|         3 | 1289 | `		PH7_VmClockNow(pVm,&iSec,&iUsec);` |
|         3 | 1290 | `		pVm->iLcgS2 ^= (sxi32)(iUsec << 11);` |
|         - | 1291 | `		/* Both streams must start inside their own modulus and away from zero,` |
|         - | 1292 | `		 * which a raw clock word is not. */` |
|         3 | 1293 | `		if( pVm->iLcgS1 < 1 ){` |
|       ! 0 | 1294 | `			pVm->iLcgS1 = -pVm->iLcgS1;` |
|       ! 0 | 1295 | `		}` |
|         3 | 1296 | `		if( pVm->iLcgS2 < 1 ){` |
|         2 | 1297 | `			pVm->iLcgS2 = -pVm->iLcgS2;` |
|         1 | 1298 | `		}` |
|         3 | 1299 | `		pVm->iLcgS1 = (pVm->iLcgS1 % 2147483562) + 1;` |
|         3 | 1300 | `		pVm->iLcgS2 = (pVm->iLcgS2 % 2147483398) + 1;` |
|         3 | 1301 | `		pVm->bLcgSeeded = 1;` |
|         1 | 1302 | `	}` |
|       605 | 1303 | `	PH7_LCG_MODMULT(53668,40014,12211,2147483563L,pVm->iLcgS1)` |
|       605 | 1304 | `	PH7_LCG_MODMULT(52774,40692,3791,2147483399L,pVm->iLcgS2)` |
|       605 | 1305 | `	z = pVm->iLcgS1 - pVm->iLcgS2;` |
|       605 | 1306 | `	if( z < 1 ){` |
|       294 | 1307 | `		z += 2147483562;` |
|       154 | 1308 | `	}` |
|       605 | 1309 | `	return z * 4.656613e-10;` |
|         1 | 1310 | `}` |
|         - | 1311 | `/*` |
|         - | 1312 | ` * string uniqid(string $prefix = "", bool $more_entropy = false)` |
|         - | 1313 | ` *  Generate a unique ID` |
|         - | 1314 | ` *` |
|         - | 1315 | ` * php's id is the CLOCK, not a random number: eight hex digits of epoch seconds` |
|         - | 1316 | `` * and five of the microseconds within them (`%08x%05x`, the microseconds masked`` |
|         - | 1317 | ` * to 0x100000 because five hex digits is all they need). Three things follow,` |
|         - | 1318 | ` * and PH7's SHA1-of-a-random-string answered none of them -- it was 14 hex` |
|         - | 1319 | ` * characters where php's is 13, it did not increase, and it was all DIGITS about` |
|         - | 1320 | ` * once in 1200 calls where php's, carrying the current epoch, effectively never` |
|         - | 1321 | ` * is. That last one is not cosmetic: Respect\Validation feeds a uniqid() through` |
|         - | 1322 | `` * `ctype_digit()`, and an all-digit id becomes an int on one side of a`` |
|         - | 1323 | ` * comparison and stays a string on the other.` |
|         - | 1324 | ` *` |
|         - | 1325 | `` * php also SLEEPS a microsecond first when `$more_entropy` is false, so two`` |
|         - | 1326 | ` * calls in a row cannot land in the same microsecond and the ids are strictly` |
|         - | 1327 | ` * increasing. It does that on POSIX only -- its own Windows build has no` |
|         - | 1328 | ` * usleep() there and makes no such promise -- and so does this.` |
|         - | 1329 | ` */` |
|      1246 | 1330 | `PH7_PRIVATE int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 1331 | `{` |
|      1248 | 1332 | `	ph7_vm *pVm = pCtx->pVm;` |
|      1248 | 1333 | `	ph7_int64 iSec = 0,iUsec = 0;` |
|      1248 | 1334 | `	const char *zPrefix = 0;` |
|      1248 | 1335 | `	int nPrefix = 0;` |
|      1248 | 1336 | `	int bEntropy = 0;` |
|      1248 | 1337 | `	if( nArg > 0 ){` |
|       619 | 1338 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|       619 | 1339 | `		if( nArg > 1 ){` |
|       607 | 1340 | `			bEntropy = ph7_value_to_bool(apArg[1]);` |
|       303 | 1341 | `		}` |
|       309 | 1342 | `	}` |
|         - | 1343 | `#if defined(__UNIXES__)` |
|      1246 | 1344 | `	if( !bEntropy ){` |
|       642 | 1345 | `		usleep(1);` |
|       321 | 1346 | `	}` |
|         - | 1347 | `#endif` |
|      1248 | 1348 | `	PH7_VmClockNow(pVm,&iSec,&iUsec);` |
|      1248 | 1349 | `	if( nPrefix > 0 ){` |
|        15 | 1350 | `		ph7_result_string(pCtx,zPrefix,nPrefix);` |
|         7 | 1351 | `	}` |
|         - | 1352 | ``	/* The seconds are php's `(int) tv.tv_sec` -- a 32-bit field, so the eight`` |
|         - | 1353 | `	 * hex digits are the low word and stay eight after 2038 rather than growing` |
|         - | 1354 | `	 * a ninth. */` |
|      1871 | 1355 | `	ph7_result_string_format(pCtx,"%08x%05x",(unsigned int)(sxu32)iSec,` |
|      1246 | 1356 | `		(unsigned int)(iUsec % 0x100000));` |
|      1248 | 1357 | `	if( bEntropy ){` |
|         - | 1358 | ``		/* php's `%.8F` of the LCG times ten: one digit, a point and eight more. */`` |
|       605 | 1359 | `		ph7_result_string_format(pCtx,"%.8f",PH7_VmCombinedLcg(pVm) * 10);` |
|       302 | 1360 | `	}` |
|      1248 | 1361 | `	return PH7_OK;` |
|         2 | 1362 | `}` |
|         - | 1363 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 1364 | `/*` |
|         - | 1365 | ` * Section:` |
|         - | 1366 | ` *  Language construct implementation as foreign functions.` |
|         - | 1367 | ` * Status:` |
|         - | 1368 | ` *    Stable.` |
|         - | 1369 | ` */` |
|         - | 1370 | `/*` |
|         - | 1371 | ` * The user-visible string coercion an OUTPUT construct performs on one of its` |
|         - | 1372 | ` * arguments (echo/print reached as host functions rather than as OP_CONSUME).` |
|         - | 1373 | ` * An ARRAY warns and still renders as "Array"; an object whose class has no` |
|         - | 1374 | ` * __toString() is php's catchable "could not be converted to string" Error,` |
|         - | 1375 | ` * and the construct outputs nothing for it. The status is recorded on the` |
|         - | 1376 | ` * context too, so OP_CALL cannot treat the throwing call as a normal return.` |
|         - | 1377 | ` */` |
|        64 | 1378 | `static sxi32 VmOutputArgToString(ph7_context *pCtx,ph7_value *pArg,const char **pzData,int *pnLen)` |
|         5 | 1379 | `{` |
|        69 | 1380 | `	sxi32 rc = PH7_MemObjToStringUV(pArg);` |
|        69 | 1381 | `	if( rc != SXRET_OK ){` |
|         3 | 1382 | `		pCtx->nThrowRc = rc;` |
|         3 | 1383 | `		return rc;` |
|         - | 1384 | `	}` |
|        66 | 1385 | `	*pzData = ph7_value_to_string(pArg,pnLen);` |
|        66 | 1386 | `	return SXRET_OK;` |
|        37 | 1387 | `}` |
|         - | 1388 | `/*` |
|         - | 1389 | ` * int print($string...)` |
|         - | 1390 | ` *  Output one or more messages.` |
|         - | 1391 | ` * Parameters` |
|         - | 1392 | ` *  $string` |
|         - | 1393 | ` *   Message to output.` |
|         - | 1394 | ` * Return` |
|         - | 1395 | ` *  1 always.` |
|         - | 1396 | ` */` |
|        64 | 1397 | `PH7_PRIVATE int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 1398 | `{` |
|         - | 1399 | `	const char *zData;` |
|        69 | 1400 | `	int nDataLen = 0;` |
|         - | 1401 | `	ph7_vm *pVm;` |
|         - | 1402 | `	int i,rc;` |
|         - | 1403 | `	/* Point to the target VM */` |
|        69 | 1404 | `	pVm = pCtx->pVm;` |
|         - | 1405 | `	/* Output */` |
|       131 | 1406 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        69 | 1407 | `		sxi32 rcSv = VmOutputArgToString(pCtx,apArg[i],&zData,&nDataLen);` |
|        69 | 1408 | `		if( rcSv != SXRET_OK ){` |
|         3 | 1409 | `			return rcSv;` |
|         - | 1410 | `		}` |
|        66 | 1411 | `		if( nDataLen > 0 ){` |
|        66 | 1412 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        66 | 1413 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        66 | 1414 | `			if( rc == SXERR_ABORT ){` |
|         - | 1415 | `				/* Output consumer callback request an operation abort */` |
|       ! 0 | 1416 | `				return PH7_ABORT;` |
|         - | 1417 | `			}` |
|        31 | 1418 | `		}` |
|        35 | 1419 | `	}` |
|         - | 1420 | `	/* Return 1 */` |
|        66 | 1421 | `	ph7_result_int(pCtx,1);` |
|        66 | 1422 | `	return SXRET_OK;` |
|        37 | 1423 | `}` |
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
|         - | 1532 | ` *  Which php.ini this interpreter read.` |
|         - | 1533 | ` * Return` |
|         - | 1534 | ` *  The path of the file, under the name php quotes for it -- absolute and` |
|         - | 1535 | `` *  canonical, which is what the CLI's `-c` door resolved its argument to and`` |
|         - | 1536 | ` *  the same string a refusal inside that file is dated by. FALSE when no file` |
|         - | 1537 | ` *  was read at all: PHL has no configuration file of its own, so that is the` |
|         - | 1538 | `` *  answer whenever `-c` was not given, and it is php's OWN answer rather than a`` |
|         - | 1539 | `` *  stub -- a php started with `-n`, or built with no php.ini in its search path,`` |
|         - | 1540 | ` *  answers exactly this. Composer's XdebugHandler asks on its way to reporting` |
|         - | 1541 | ` *  where a directive came from, and takes "nowhere" for an answer.` |
|         - | 1542 | ` */` |
|        20 | 1543 | `PH7_PRIVATE int vm_builtin_php_ini_loaded_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       ! 0 | 1544 | `{` |
|        20 | 1545 | `	SyString *pFile = &pCtx->pVm->pEngine->xConf.sIniFile;` |
|        10 | 1546 | `	SXUNUSED(nArg);` |
|        10 | 1547 | `	SXUNUSED(apArg);` |
|        20 | 1548 | `	if( pFile->nByte < 1 ){` |
|         6 | 1549 | `		ph7_result_bool(pCtx,0);` |
|         3 | 1550 | `	}else{` |
|        14 | 1551 | `		ph7_result_string(pCtx,pFile->zString,(int)pFile->nByte);` |
|         - | 1552 | `	}` |
|        20 | 1553 | `	return PH7_OK;` |
|       ! 0 | 1554 | `}` |
|         - | 1555 | `/*` |
|         - | 1556 | ` * string\|false php_ini_scanned_files(void)` |
|         - | 1557 | ` *  Which files the scan directory contributed after php.ini itself.` |
|         - | 1558 | ` * Return` |
|         - | 1559 | ` *  FALSE, always, and this one is not paired with php_ini_loaded_file(): php` |
|         - | 1560 | ` *  answers it from --with-config-file-scan-dir, a build-time directory PHL has` |
|         - | 1561 | ` *  none of. A php built without one answers FALSE here while still naming a` |
|         - | 1562 | ` *  loaded php.ini above.` |
|         - | 1563 | ` */` |
|       ! 0 | 1564 | `PH7_PRIVATE int vm_builtin_php_ini_scanned_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       ! 0 | 1565 | `{` |
|       ! 0 | 1566 | `	SXUNUSED(nArg);` |
|       ! 0 | 1567 | `	SXUNUSED(apArg);` |
|       ! 0 | 1568 | `	ph7_result_bool(pCtx,0);` |
|       ! 0 | 1569 | `	return PH7_OK;` |
|       ! 0 | 1570 | `}` |
|         - | 1571 | `/*` |
|         - | 1572 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|         - | 1573 | ` */` |
|         - | 1574 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|         - | 1575 | ` "<html><head>"\` |
|         - | 1576 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|         - | 1577 | ` "<style type=\"text/css\">"\` |
|         - | 1578 | ` "div {"\` |
|         - | 1579 | `     "border: 1px solid #cccccc;"\` |
|         - | 1580 | `     "-moz-border-radius-topleft: 10px;"\` |
|         - | 1581 | `     "-moz-border-radius-bottomright: 10px;"\` |
|         - | 1582 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|         - | 1583 | `     "-moz-border-radius-topright: 10px;"\` |
|         - | 1584 | `     "-webkit-border-radius: 10px;"\` |
|         - | 1585 | `     "-o-border-radius: 10px;"\` |
|         - | 1586 | `     "border-radius: 10px;"\` |
|         - | 1587 | `     "padding-left: 2em;"\` |
|         - | 1588 | `     "background-color: white;"\` |
|         - | 1589 | `     "margin-left: auto;"\` |
|         - | 1590 | `     "font-family: verdana;"\` |
|         - | 1591 | `     "padding-right: 2em;"\` |
|         - | 1592 | `     "margin-right: auto;"\` |
|         - | 1593 | `     "}"\` |
|         - | 1594 | `     "body {"\` |
|         - | 1595 | `     "padding: 0.2em;"\` |
|         - | 1596 | `     "font-style: normal;"\` |
|         - | 1597 | `     "font-size: medium;"\` |
|         - | 1598 | `     "background-color: #f2f2f2;"\` |
|         - | 1599 | `     "}"\` |
|         - | 1600 | `     "hr {"\` |
|         - | 1601 | `     "border-style: solid none none;"\` |
|         - | 1602 | `     "border-width: 1px medium medium;"\` |
|         - | 1603 | `     "border-top: 1px solid #cccccc;"\` |
|         - | 1604 | `     "height: 1px;"\` |
|         - | 1605 | `     "}"\` |
|         - | 1606 | `     "a {"\` |
|         - | 1607 | `     "color: #3366cc;"\` |
|         - | 1608 | `     "text-decoration: none;"\` |
|         - | 1609 | `     "}"\` |
|         - | 1610 | `     "a:hover {"\` |
|         - | 1611 | `     "color: #999999;"\` |
|         - | 1612 | `     "}"\` |
|         - | 1613 | `     "a:active {"\` |
|         - | 1614 | `     "color: #663399;"\` |
|         - | 1615 | `     "}"\` |
|         - | 1616 | `     "h1 {"\` |
|         - | 1617 | `     "margin: 0;"\` |
|         - | 1618 | `     "padding: 0;"\` |
|         - | 1619 | `     "font-family: Verdana;"\` |
|         - | 1620 | `     "font-weight: bold;"\` |
|         - | 1621 | `     "font-style: normal;"\` |
|         - | 1622 | `     "font-size: medium;"\` |
|         - | 1623 | `     "text-transform: capitalize;"\` |
|         - | 1624 | `     "color: #0a328c;"\` |
|         - | 1625 | `     "}"\` |
|         - | 1626 | `     "p {"\` |
|         - | 1627 | `     "margin: 0 auto;"\` |
|         - | 1628 | `     "font-size: medium;"\` |
|         - | 1629 | `     "font-style: normal;"\` |
|         - | 1630 | `     "font-family: verdana;"\` |
|         - | 1631 | `     "}"\` |
|         - | 1632 | `"</style></head><body>"\` |
|         - | 1633 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|         - | 1634 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|         - | 1635 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|         - | 1636 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|         - | 1637 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|         - | 1638 | `"<p style=\"text-align: left;\"><small><small>"\` |
|         - | 1639 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|         - | 1640 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|         - | 1641 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|         - | 1642 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|         - | 1643 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|         - | 1644 |  |
|         - | 1645 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|         - | 1646 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|         - | 1647 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|         - | 1648 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|         - | 1649 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|         - | 1650 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|         - | 1651 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|         - | 1652 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|         - | 1653 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|         - | 1654 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|         - | 1655 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|         - | 1656 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|         - | 1657 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|         - | 1658 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|         - | 1659 |  |
|         - | 1660 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|         - | 1661 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|         - | 1662 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|         - | 1663 | `"&nbsp;*<br>"\` |
|         - | 1664 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|         - | 1665 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|         - | 1666 | `"&nbsp;* are met:<br>"\` |
|         - | 1667 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|         - | 1668 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|         - | 1669 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|         - | 1670 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|         - | 1671 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|         - | 1672 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|         - | 1673 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|         - | 1674 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|         - | 1675 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|         - | 1676 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|         - | 1677 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|         - | 1678 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|         - | 1679 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|         - | 1680 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|         - | 1681 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|         - | 1682 | `"&nbsp;*<br>"\` |
|         - | 1683 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|         - | 1684 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|         - | 1685 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|         - | 1686 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|         - | 1687 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|         - | 1688 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|         - | 1689 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|         - | 1690 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|         - | 1691 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|         - | 1692 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|         - | 1693 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|         - | 1694 | `"&nbsp;*/<br>"\` |
|         - | 1695 | `"</span></small></small></p>"\` |
|         - | 1696 | `"</div></body></html>"` |
|         - | 1697 | `/*` |
|         - | 1698 | ` * bool ph7credits(void)` |
|         - | 1699 | ` * bool ph7info(void)` |
|         - | 1700 | ` * bool ph7copyright(void)` |
|         - | 1701 | ` *  Prints out the credits for PH7 engine` |
|         - | 1702 | ` * Parameters` |
|         - | 1703 | ` *  None` |
|         - | 1704 | ` * Return` |
|         - | 1705 | ` *  Always TRUE` |
|         - | 1706 | ` */` |
|         2 | 1707 | `PH7_PRIVATE int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 1708 | `{` |
|         3 | 1709 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|         - | 1710 | `	/* Expand the HTML page above*/` |
|         3 | 1711 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|         2 | 1712 | `	ph7_context_output_format(` |
|         1 | 1713 | `		pCtx,` |
|         - | 1714 | `		PH7_HTML_PAGE_FORMAT,` |
|         1 | 1715 | `		ph7_lib_version(),   /* Engine version */` |
|         1 | 1716 | `		ph7_lib_signature(), /* Engine signature */` |
|         1 | 1717 | `		ph7_lib_ident(),     /* Engine ID */` |
|         2 | 1718 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|         2 | 1719 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|         1 | 1720 | `		SyHashTotalEntry(&pVm->hClass),` |
|         - | 1721 | `#ifdef __WINNT__` |
|         - | 1722 | `		"Windows NT"` |
|         - | 1723 | `#elif defined(__UNIXES__)` |
|         - | 1724 | `		"UNIX-Like"` |
|         - | 1725 | `#else` |
|         - | 1726 | `		"Other OS"` |
|         - | 1727 | `#endif` |
|         - | 1728 | `		);` |
|         3 | 1729 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|         1 | 1730 | `	SXUNUSED(nArg); /* cc warning */` |
|         1 | 1731 | `	SXUNUSED(apArg);` |
|         - | 1732 | `	/* Return TRUE */` |
|         - | 1733 | `	//ph7_result_bool(pCtx,1);` |
|         3 | 1734 | `	return PH7_OK;` |
|         1 | 1735 | `}` |
|         - | 1736 | `/*` |
|         - | 1737 | ` * Section:` |
|         - | 1738 | ` *    URL related routines.` |
|         - | 1739 | ` * Status:` |
|         - | 1740 | ` *    Stable.` |
|         - | 1741 | ` */` |
|         - | 1742 | `/*` |
|         - | 1743 | ` * value parse_url(string $url [, int $component = -1 ])` |
|         - | 1744 | ` *  Parse a URL and return its fields.` |
|         - | 1745 | ` * Parameters` |
|         - | 1746 | ` *  $url` |
|         - | 1747 | ` *   The URL to parse.` |
|         - | 1748 | ` * $component` |
|         - | 1749 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|         - | 1750 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|         - | 1751 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|         - | 1752 | ` *  in which case the return value will be an integer).` |
|         - | 1753 | ` * Return` |
|         - | 1754 | ` *  If the component parameter is omitted, an associative array is returned.` |
|         - | 1755 | ` *  At least one element will be present within the array. Potential keys within` |
|         - | 1756 | ` *  this array are:` |
|         - | 1757 | ` *   scheme - e.g. http` |
|         - | 1758 | ` *   host` |
|         - | 1759 | ` *   port` |
|         - | 1760 | ` *   user` |
|         - | 1761 | ` *   pass` |
|         - | 1762 | ` *   path` |
|         - | 1763 | ` *   query - after the question mark ?` |
|         - | 1764 | ` *   fragment - after the hashmark #` |
|         - | 1765 | ` * Note:` |
|         - | 1766 | ` *  FALSE is returned on failure.` |
|         - | 1767 | ` *  This function work with relative URL unlike the one shipped` |
|         - | 1768 | ` *  with the standard PHP engine.` |
|         - | 1769 | ` */` |
|         - | 1770 | `/*` |
|         - | 1771 | ` * parse_url() component set.` |
|         - | 1772 | ` *` |
|         - | 1773 | ` * A bare SyString cannot express "present but empty", which php needs:` |
|         - | 1774 | ` * parse_url("") is ['path'=>''] and parse_url("?") is ['query'=>''], both` |
|         - | 1775 | ` * distinct from the component being absent. So presence is tracked separately.` |
|         - | 1776 | ` */` |
|      1328 | 1777 | `static int VmUrlIsAlnum(int c)` |
|         2 | 1778 | `{` |
|      1330 | 1779 | `	return (c >= '0' && c <= '9') \|\| (c >= 'a' && c <= 'z') \|\| (c >= 'A' && c <= 'Z');` |
|         2 | 1780 | `}` |
|        12 | 1781 | `static int VmUrlIsAlpha(int c)` |
|         2 | 1782 | `{` |
|        14 | 1783 | `	return (c >= 'a' && c <= 'z') \|\| (c >= 'A' && c <= 'Z');` |
|         2 | 1784 | `}` |
|         - | 1785 | `/* scheme = ALNUM *( ALNUM / "+" / "-" / "." ) -- php accepts a digit first. */` |
|      1328 | 1786 | `static int VmUrlIsSchemeByte(int c)` |
|         2 | 1787 | `{` |
|      1330 | 1788 | `	return VmUrlIsAlnum(c) \|\| c == '+' \|\| c == '-' \|\| c == '.';` |
|         2 | 1789 | `}` |
|         - | 1790 | `/*` |
|         - | 1791 | ` * Resolve the port span that followed the ':' in an authority.` |
|         - | 1792 | ` *` |
|         - | 1793 | ` * Returns 1 (usable port in *piPort), 0 (the span is empty, so there is simply` |
|         - | 1794 | ` * no port) or -1 (php rejects the whole URL). php is lenient about what follows` |
|         - | 1795 | ` * the digits -- ":8a" and ":8 0" both yield 8 -- but demands at least one digit` |
|         - | 1796 | ` * and a value that fits a port, so ":abc", ":-80" and ":65536" are all failures.` |
|         - | 1797 | ` */` |
|        92 | 1798 | `static int VmUrlParsePort(const char *z,int n,int *piPort)` |
|         2 | 1799 | `{` |
|        94 | 1800 | `	int i = 0,iVal = 0,nDigit = 0;` |
|        94 | 1801 | `	if( n < 1 ){` |
|       ! 0 | 1802 | `		return 0;` |
|         - | 1803 | `	}` |
|       140 | 1804 | `	while( i < n && (z[i] == ' ' \|\| z[i] == '\t') ){` |
|       ! 0 | 1805 | `		i++;` |
|       ! 0 | 1806 | `	}` |
|        94 | 1807 | `	if( i < n && (z[i] == '+' \|\| z[i] == '-') ){` |
|       ! 0 | 1808 | `		if( z[i] == '-' ){` |
|       ! 0 | 1809 | `			return -1;` |
|         - | 1810 | `		}` |
|       ! 0 | 1811 | `		i++;` |
|       ! 0 | 1812 | `	}` |
|       318 | 1813 | `	while( i < n && z[i] >= '0' && z[i] <= '9' ){` |
|       226 | 1814 | `		iVal = iVal * 10 + (z[i] - '0');` |
|       226 | 1815 | `		if( iVal > 65535 ){` |
|       ! 0 | 1816 | `			return -1; /* Out of range, and this also caps the accumulator */` |
|         - | 1817 | `		}` |
|       226 | 1818 | `		nDigit++;` |
|       226 | 1819 | `		i++;` |
|         2 | 1820 | `	}` |
|        94 | 1821 | `	if( nDigit < 1 ){` |
|        12 | 1822 | `		return -1;` |
|         - | 1823 | `	}` |
|        84 | 1824 | `	*piPort = iVal;` |
|        84 | 1825 | `	return 1;` |
|        48 | 1826 | `}` |
|         - | 1827 | `/*` |
|         - | 1828 | ` * Split an authority -- "[user[:pass]@]host[:port]" -- into pOut.` |
|         - | 1829 | ` * Returns 0 for the malformed input php reports as FALSE.` |
|         - | 1830 | ` */` |
|       278 | 1831 | `static int VmUrlParseAuthority(const char *z,int n,VmUrlParts *pOut,int bPortKnown)` |
|         2 | 1832 | `{` |
|         - | 1833 | `	const char *zHost;` |
|       280 | 1834 | `	int i,iAt = -1,iColon = -1,iSep = -1,nHost,iPort = 0,rc;` |
|         - | 1835 | `	/* php splits the credentials at the LAST '@' ("//a@b@c" is user "a@b") */` |
|      2110 | 1836 | `	for( i = 0 ; i < n ; ++i ){` |
|      1832 | 1837 | `		if( z[i] == '@' ){` |
|        44 | 1838 | `			iAt = i;` |
|        21 | 1839 | `		}` |
|       917 | 1840 | `	}` |
|       280 | 1841 | `	if( iAt >= 0 ){` |
|         - | 1842 | `		/* and the user from the password at the FIRST ':' before it */` |
|       154 | 1843 | `		for( i = 0 ; i < iAt ; ++i ){` |
|       152 | 1844 | `			if( z[i] == ':' ){` |
|        42 | 1845 | `				iColon = i;` |
|        42 | 1846 | `				break;` |
|         - | 1847 | `			}` |
|        57 | 1848 | `		}` |
|        44 | 1849 | `		if( iColon >= 0 ){` |
|        42 | 1850 | `			SyStringInitFromBuf(&pOut->sUser,z,(sxu32)iColon);` |
|        42 | 1851 | `			SyStringInitFromBuf(&pOut->sPass,&z[iColon+1],(sxu32)(iAt - iColon - 1));` |
|        42 | 1852 | `			pOut->bUser = pOut->bPass = 1;` |
|        22 | 1853 | `		}else{` |
|         3 | 1854 | `			SyStringInitFromBuf(&pOut->sUser,z,(sxu32)iAt);` |
|         3 | 1855 | `			pOut->bUser = 1;` |
|         - | 1856 | `		}` |
|        44 | 1857 | `		z += iAt + 1;` |
|        44 | 1858 | `		n -= iAt + 1;` |
|        21 | 1859 | `	}` |
|       280 | 1860 | `	zHost = z;` |
|       280 | 1861 | `	nHost = n;` |
|       280 | 1862 | `	if( !(n > 1 && z[0] == '[' && z[n-1] == ']') ){` |
|         - | 1863 | `		/* Unless a bracketed IPv6 literal fills the WHOLE authority -- in which` |
|         - | 1864 | `		 * case php keeps the brackets in "host" and scans no port, so every ':'` |
|         - | 1865 | `		 * inside belongs to the address -- the port hangs off the LAST ':'.` |
|         - | 1866 | `		 * php decides that on the first and last byte alone, which is why` |
|         - | 1867 | `		 * "//[:]\|]" is a single host while "//[::1]:8080" splits a port off. */` |
|      1838 | 1868 | `		for( i = 0 ; i < n ; ++i ){` |
|      1562 | 1869 | `			if( z[i] == ':' ){` |
|       118 | 1870 | `				iSep = i;` |
|        58 | 1871 | `			}` |
|       782 | 1872 | `		}` |
|       278 | 1873 | `		if( iSep >= 0 ){` |
|         - | 1874 | `			/* The host is trimmed at the ':' whether or not the port was already` |
|         - | 1875 | `			 * resolved by the caller. */` |
|        94 | 1876 | `			nHost = iSep;` |
|        94 | 1877 | `			if( !bPortKnown ){` |
|        88 | 1878 | `				rc = VmUrlParsePort(&z[iSep+1],n - iSep - 1,&iPort);` |
|        88 | 1879 | `				if( rc < 0 ){` |
|        12 | 1880 | `					return 0;` |
|         - | 1881 | `				}` |
|        78 | 1882 | `				if( rc > 0 ){` |
|        78 | 1883 | `					pOut->iPort = iPort;` |
|        78 | 1884 | `					pOut->bPort = 1;` |
|        38 | 1885 | `				}` |
|        38 | 1886 | `			}` |
|        41 | 1887 | `		}` |
|       133 | 1888 | `	}` |
|       270 | 1889 | `	if( nHost < 1 ){` |
|         - | 1890 | `		/* php requires a non-empty host once an authority is in play, which is` |
|         - | 1891 | `		 * what makes "//", "http://" and ":80" all FALSE. */` |
|        36 | 1892 | `		return 0;` |
|         - | 1893 | `	}` |
|       236 | 1894 | `	SyStringInitFromBuf(&pOut->sHost,zHost,(sxu32)nHost);` |
|       236 | 1895 | `	pOut->bHost = 1;` |
|       236 | 1896 | `	return 1;` |
|       141 | 1897 | `}` |
|         - | 1898 | `/*` |
|         - | 1899 | ` * Split "path[?query][#fragment]". The fragment is taken FIRST and the query` |
|         - | 1900 | ` * only from what precedes it, so "#a?b" is a fragment of "a?b" with no query.` |
|         - | 1901 | ` */` |
|       278 | 1902 | `static void VmUrlParsePath(const char *z,int n,VmUrlParts *pOut)` |
|         3 | 1903 | `{` |
|       281 | 1904 | `	int i,iEnd = n;` |
|      1669 | 1905 | `	for( i = 0 ; i < n ; ++i ){` |
|      1443 | 1906 | `		if( z[i] == '#' ){` |
|        54 | 1907 | `			SyStringInitFromBuf(&pOut->sFragment,&z[i+1],(sxu32)(n - i - 1));` |
|        54 | 1908 | `			pOut->bFragment = 1;` |
|        54 | 1909 | `			iEnd = i;` |
|        54 | 1910 | `			break;` |
|         - | 1911 | `		}` |
|       697 | 1912 | `	}` |
|      1399 | 1913 | `	for( i = 0 ; i < iEnd ; ++i ){` |
|      1189 | 1914 | `		if( z[i] == '?' ){` |
|        70 | 1915 | `			SyStringInitFromBuf(&pOut->sQuery,&z[i+1],(sxu32)(iEnd - i - 1));` |
|        70 | 1916 | `			pOut->bQuery = 1;` |
|        70 | 1917 | `			iEnd = i;` |
|        70 | 1918 | `			break;` |
|         - | 1919 | `		}` |
|       562 | 1920 | `	}` |
|       281 | 1921 | `	if( iEnd > 0 ){` |
|       263 | 1922 | `		SyStringInitFromBuf(&pOut->sPath,z,(sxu32)iEnd);` |
|       263 | 1923 | `		pOut->bPath = 1;` |
|       130 | 1924 | `	}` |
|       281 | 1925 | `}` |
|         - | 1926 | `/* Parse "host[:port]" followed by an optional path/query/fragment. */` |
|       278 | 1927 | `static int VmUrlAuthorityThenPath(const char *z,int n,VmUrlParts *pOut,int bPortKnown)` |
|         2 | 1928 | `{` |
|       280 | 1929 | `	int i,iEnd = n;` |
|      2110 | 1930 | `	for( i = 0 ; i < n ; ++i ){` |
|      2020 | 1931 | `		if( z[i] == '/' \|\| z[i] == '?' \|\| z[i] == '#' ){` |
|       190 | 1932 | `			iEnd = i;` |
|       190 | 1933 | `			break;` |
|         - | 1934 | `		}` |
|       917 | 1935 | `	}` |
|       280 | 1936 | `	if( !VmUrlParseAuthority(z,iEnd,pOut,bPortKnown) ){` |
|        46 | 1937 | `		return 0;` |
|         - | 1938 | `	}` |
|       236 | 1939 | `	if( iEnd < n ){` |
|       170 | 1940 | `		VmUrlParsePath(&z[iEnd],n - iEnd,pOut);` |
|        84 | 1941 | `	}` |
|       236 | 1942 | `	return 1;` |
|       141 | 1943 | `}` |
|         - | 1944 | `/*` |
|         - | 1945 | ` * Resolve the port php took from the FIRST ':' of a host:port URL.` |
|         - | 1946 | ` *` |
|         - | 1947 | ` * php reads the port straight off that colon before it works out where the host` |
|         - | 1948 | ` * ends, so the digits can even belong to what becomes the path: parse_url()` |
|         - | 1949 | ` * reports port 1 for "a/:1", whose path is "/:1". Mirroring the order keeps` |
|         - | 1950 | ` * that quirk. Returns 0 for a port php rejects.` |
|         - | 1951 | ` */` |
|         6 | 1952 | `static int VmUrlPreparePort(const char *z,int k,int nEnd,VmUrlParts *pOut)` |
|         1 | 1953 | `{` |
|         7 | 1954 | `	int iPort = 0;` |
|         7 | 1955 | `	int rc = VmUrlParsePort(&z[k+1],nEnd - k - 1,&iPort);` |
|         7 | 1956 | `	if( rc < 0 ){` |
|       ! 0 | 1957 | `		return 0;` |
|         - | 1958 | `	}` |
|         7 | 1959 | `	if( rc > 0 ){` |
|         7 | 1960 | `		pOut->iPort = iPort;` |
|         7 | 1961 | `		pOut->bPort = 1;` |
|         3 | 1962 | `	}` |
|         7 | 1963 | `	return 1;` |
|         4 | 1964 | `}` |
|         - | 1965 | `/*` |
|         - | 1966 | ` * php's URL parser, as parse_url() needs it. Returns 0 where php returns FALSE.` |
|         - | 1967 | ` *` |
|         - | 1968 | ` * PH7_VmHttpSplitURI cannot serve here: it is an HTTP REQUEST-target splitter` |
|         - | 1969 | ` * (the HTTP server and filter_var share it) and assumes the input is` |
|         - | 1970 | ` * authority-first, so it read "a" as a host and "mailto:me@x.com" as` |
|         - | 1971 | ` * user:pass@host. php instead decides authority-vs-path up front: only a "//",` |
|         - | 1972 | ` * with or without a scheme before it, introduces an authority.` |
|         - | 1973 | ` */` |
|       392 | 1974 | `PH7_PRIVATE int PH7_VmUrlSplit(const char *z,int n,VmUrlParts *pOut)` |
|         3 | 1975 | `{` |
|       395 | 1976 | `	int i,k = -1,bScheme,bPortForm = 0,nPortEnd = 0;` |
|       395 | 1977 | `	SyZero(pOut,sizeof(VmUrlParts));` |
|         - | 1978 | `	/* A leading "//" settles it before any colon is considered: the authority` |
|         - | 1979 | `	 * starts after the slashes. Reading the colon first turned "//h:80" into a` |
|         - | 1980 | `	 * host called "//h" and "//[::1]" into a path. */` |
|       395 | 1981 | `	if( n >= 2 && z[0] == '/' && z[1] == '/' ){` |
|        24 | 1982 | `		return VmUrlAuthorityThenPath(&z[2],n - 2,pOut,0);` |
|         - | 1983 | `	}` |
|      1883 | 1984 | `	for( i = 0 ; i < n ; ++i ){` |
|      1841 | 1985 | `		if( z[i] == ':' ){` |
|       330 | 1986 | `			k = i;` |
|       330 | 1987 | `			break;` |
|         - | 1988 | `		}` |
|       758 | 1989 | `	}` |
|       373 | 1990 | `	if( k == 0 && n == 1 ){` |
|         - | 1991 | `		/* A lone ":" is an EMPTY scheme, which php rejects outright -- unlike` |
|         - | 1992 | `		 * ":a" or "::", which are simply paths. */` |
|         3 | 1993 | `		return 0;` |
|         - | 1994 | `	}` |
|       371 | 1995 | `	bScheme = k > 0;` |
|      1699 | 1996 | `	for( i = 0 ; bScheme && i < k ; ++i ){` |
|      1330 | 1997 | `		if( !VmUrlIsSchemeByte((unsigned char)z[i]) ){` |
|       ! 0 | 1998 | `			bScheme = 0;` |
|       ! 0 | 1999 | `		}` |
|       666 | 2000 | `	}` |
|       371 | 2001 | `	if( bScheme && k + 1 == n ){` |
|         - | 2002 | `		/* "x:" -- the scheme is the whole URL */` |
|         3 | 2003 | `		SyStringInitFromBuf(&pOut->sScheme,z,(sxu32)k);` |
|         3 | 2004 | `		pOut->bScheme = 1;` |
|         3 | 2005 | `		return 1;` |
|         - | 2006 | `	}` |
|         - | 2007 | `	/* Decide whether that ':' introduces a PORT rather than a scheme: at least` |
|         - | 2008 | `	 * one digit, running to the end of the string or to a '/'. That is what` |
|         - | 2009 | `	 * makes "a:80" a host:port while "a:80?q" is a scheme with path "80", and` |
|         - | 2010 | `	 * it holds however the ':' is reached -- ":1" and "/:1" are both authorities` |
|         - | 2011 | `	 * with an empty host, which php rejects. php caps the run, so a long number` |
|         - | 2012 | `	 * stays a path: "a:123456" is a path, "a:99999" an out-of-range port. */` |
|       369 | 2013 | `	if( k >= 0 ){` |
|       326 | 2014 | `		int p = k + 1;` |
|       326 | 2015 | `		int bBeforeQuery = 1;` |
|       326 | 2016 | `		nPortEnd = k + 1;` |
|         - | 2017 | `		/* A ':' that sits inside a query or fragment is just data: "?:1" is a` |
|         - | 2018 | `		 * query of ":1", not an authority with an empty host. */` |
|      1652 | 2019 | `		for( i = 0 ; i < k ; ++i ){` |
|      1328 | 2020 | `			if( z[i] == '?' \|\| z[i] == '#' ){` |
|       ! 0 | 2021 | `				bBeforeQuery = 0;` |
|       ! 0 | 2022 | `				break;` |
|         - | 2023 | `			}` |
|       665 | 2024 | `		}` |
|       336 | 2025 | `		while( p < n && z[p] >= '0' && z[p] <= '9' ){` |
|        11 | 2026 | `			p++;` |
|         1 | 2027 | `		}` |
|       326 | 2028 | `		if( bBeforeQuery && p > k + 1 && (p >= n \|\| z[p] == '/') && (p - k) < 7 ){` |
|         7 | 2029 | `			bPortForm = 1;` |
|         7 | 2030 | `			nPortEnd = p;` |
|         3 | 2031 | `		}` |
|       162 | 2032 | `	}` |
|       369 | 2033 | `	if( !bScheme ){` |
|        47 | 2034 | `		if( bPortForm ){` |
|         3 | 2035 | `			if( !VmUrlPreparePort(z,k,nPortEnd,pOut) ){` |
|       ! 0 | 2036 | `				return 0;` |
|         - | 2037 | `			}` |
|         3 | 2038 | `			return VmUrlAuthorityThenPath(z,n,pOut,1);` |
|         - | 2039 | `		}` |
|        45 | 2040 | `		VmUrlParsePath(z,n,pOut);` |
|        45 | 2041 | `		if( !pOut->bPath && !pOut->bQuery && !pOut->bFragment ){` |
|         - | 2042 | `			/* php reports the empty URL as an empty PATH, not as no components */` |
|         3 | 2043 | `			SyStringInitFromBuf(&pOut->sPath,z,0);` |
|         3 | 2044 | `			pOut->bPath = 1;` |
|         1 | 2045 | `		}` |
|        45 | 2046 | `		return 1;` |
|         - | 2047 | `	}` |
|       324 | 2048 | `	if( bPortForm ){` |
|         5 | 2049 | `		if( !VmUrlPreparePort(z,k,nPortEnd,pOut) ){` |
|       ! 0 | 2050 | `			return 0;` |
|         - | 2051 | `		}` |
|         5 | 2052 | `		return VmUrlAuthorityThenPath(z,n,pOut,1);` |
|         - | 2053 | `	}` |
|       320 | 2054 | `	SyStringInitFromBuf(&pOut->sScheme,z,(sxu32)k);` |
|       320 | 2055 | `	pOut->bScheme = 1;` |
|       320 | 2056 | `	if( z[k+1] == '/' && k + 2 < n && z[k+2] == '/' ){` |
|       262 | 2057 | `		if( k + 3 < n && z[k+3] == '/' && pOut->sScheme.nByte == 4` |
|        20 | 2058 | `		 && (z[0]=='f'\|\|z[0]=='F') && (z[1]=='i'\|\|z[1]=='I')` |
|        14 | 2059 | `		 && (z[2]=='l'\|\|z[2]=='L') && (z[3]=='e'\|\|z[3]=='E') ){` |
|         - | 2060 | `			/* file:/// has no authority: the path starts at the third slash,` |
|         - | 2061 | `			 * except that a "c:" drive letter swallows it (file:///c:/x is the` |
|         - | 2062 | `			 * path "c:/x"). A '\|' in place of the ':' does NOT count. */` |
|        14 | 2063 | `			int iBase = k + 3;` |
|        14 | 2064 | `			if( iBase + 2 < n && VmUrlIsAlpha((unsigned char)z[iBase+1]) && z[iBase+2] == ':' ){` |
|       ! 0 | 2065 | `				iBase++;` |
|       ! 0 | 2066 | `			}` |
|        14 | 2067 | `			VmUrlParsePath(&z[iBase],n - iBase,pOut);` |
|        14 | 2068 | `			return 1;` |
|         - | 2069 | `		}` |
|       252 | 2070 | `		return VmUrlAuthorityThenPath(&z[k+3],n - k - 3,pOut,0);` |
|         - | 2071 | `	}` |
|         - | 2072 | `	/* "mailto:me@x.com", "x:y", "http:/x": everything after the ':' is a path */` |
|        58 | 2073 | `	VmUrlParsePath(&z[k+1],n - k - 1,pOut);` |
|        58 | 2074 | `	return 1;` |
|       199 | 2075 | `}` |
|         - | 2076 | `/*` |
|         - | 2077 | ` * Emit one parsed component into pValue, replacing control bytes with '_'.` |
|         - | 2078 | ` *` |
|         - | 2079 | ` * php runs php_replace_controlchars_ex over every string component it returns,` |
|         - | 2080 | ` * so a URL carrying a raw NUL, newline or DEL cannot smuggle it through into` |
|         - | 2081 | ` * whatever the caller splices the component into (a header, a log line, a` |
|         - | 2082 | ` * redirect). Bytes >= 0x80 are deliberately left alone -- php only folds the` |
|         - | 2083 | ` * ASCII control range.` |
|         - | 2084 | ` */` |
|       164 | 2085 | `static void VmUrlSetComponent(ph7_value *pValue,const SyString *pComp)` |
|         1 | 2086 | `{` |
|       165 | 2087 | `	const char *z = pComp->zString;` |
|       165 | 2088 | `	sxu32 n = pComp->nByte,i,iRun = 0;` |
|       165 | 2089 | `	if( n < 1 \|\| z == 0 ){` |
|         3 | 2090 | `		ph7_value_string(pValue,"",0);` |
|         3 | 2091 | `		return;` |
|         - | 2092 | `	}` |
|       955 | 2093 | `	for( i = 0 ; i < n ; ++i ){` |
|       793 | 2094 | `		unsigned char c = (unsigned char)z[i];` |
|       793 | 2095 | `		if( c < 0x20 \|\| c == 0x7f ){` |
|         3 | 2096 | `			if( i > iRun ){` |
|         3 | 2097 | `				ph7_value_string(pValue,&z[iRun],(int)(i - iRun));` |
|         1 | 2098 | `			}` |
|         3 | 2099 | `			ph7_value_string(pValue,"_",1);` |
|         3 | 2100 | `			iRun = i + 1;` |
|         1 | 2101 | `		}` |
|       397 | 2102 | `	}` |
|       163 | 2103 | `	if( n > iRun ){` |
|       163 | 2104 | `		ph7_value_string(pValue,&z[iRun],(int)(n - iRun));` |
|        81 | 2105 | `	}` |
|        83 | 2106 | `}` |
|       104 | 2107 | `PH7_PRIVATE int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2108 | `{` |
|         - | 2109 | `	const char *zStr; /* Input string */` |
|         - | 2110 | `	VmUrlParts sUrl;  /* Parse of the given URI */` |
|         - | 2111 | `	SyString *pComp;` |
|         - | 2112 | `	int bHave;` |
|         - | 2113 | `	int nLen;` |
|       105 | 2114 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|         - | 2115 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2116 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2117 | `		return PH7_OK;` |
|         - | 2118 | `	}` |
|         - | 2119 | `	/* Extract the given URI. An empty string is NOT a failure: php parses it as` |
|         - | 2120 | `	 * an empty path. */` |
|       105 | 2121 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       105 | 2122 | `	if( nLen < 0 ){` |
|       ! 0 | 2123 | `		nLen = 0;` |
|       ! 0 | 2124 | `	}` |
|       105 | 2125 | `	if( !PH7_VmUrlSplit(zStr,nLen,&sUrl) ){` |
|         - | 2126 | `		/* Malformed input,return FALSE */` |
|        13 | 2127 | `		ph7_result_bool(pCtx,0);` |
|        13 | 2128 | `		return PH7_OK;` |
|         - | 2129 | `	}` |
|       103 | 2130 | `	if( nArg > 1 && ph7_value_to_int(apArg[1]) >= 0 ){` |
|         - | 2131 | `		/* Refer to constant.c for constants values (php's PHP_URL_* are 0-based;` |
|         - | 2132 | `		 * PHL used to number them from 1, so every literal component id selected` |
|         - | 2133 | `		 * the WRONG field -- and the constants' own tests only asserted "%d",` |
|         - | 2134 | `		 * which any numbering satisfies). A negative id means "the whole array",` |
|         - | 2135 | `		 * which is what the default $component = -1 relies on. */` |
|        27 | 2136 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        27 | 2137 | `		pComp = 0;` |
|        27 | 2138 | `		bHave = 0;` |
|        27 | 2139 | `		switch(nComponent){` |
|         3 | 2140 | `		case 0: /* PHP_URL_SCHEME */   pComp = &sUrl.sScheme;   bHave = sUrl.bScheme;   break;` |
|         5 | 2141 | `		case 1: /* PHP_URL_HOST */     pComp = &sUrl.sHost;     bHave = sUrl.bHost;     break;` |
|         2 | 2142 | `		case 2: /* PHP_URL_PORT */` |
|         5 | 2143 | `			if( sUrl.bPort ){` |
|         5 | 2144 | `				ph7_result_int(pCtx,sUrl.iPort);` |
|         3 | 2145 | `			}else{` |
|       ! 0 | 2146 | `				ph7_result_null(pCtx);` |
|         - | 2147 | `			}` |
|         5 | 2148 | `			return PH7_OK;` |
|         3 | 2149 | `		case 3: /* PHP_URL_USER */     pComp = &sUrl.sUser;     bHave = sUrl.bUser;     break;` |
|         3 | 2150 | `		case 4: /* PHP_URL_PASS */     pComp = &sUrl.sPass;     bHave = sUrl.bPass;     break;` |
|         3 | 2151 | `		case 5: /* PHP_URL_PATH */     pComp = &sUrl.sPath;     bHave = sUrl.bPath;     break;` |
|         5 | 2152 | `		case 6: /* PHP_URL_QUERY */    pComp = &sUrl.sQuery;    bHave = sUrl.bQuery;    break;` |
|         5 | 2153 | `		case 7: /* PHP_URL_FRAGMENT */ pComp = &sUrl.sFragment; bHave = sUrl.bFragment; break;` |
|         1 | 2154 | `		default:` |
|         4 | 2155 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 2156 | `				"parse_url(): Argument #2 ($component) must be a valid URL component identifier, %d given",` |
|         1 | 2157 | `				nComponent);` |
|         - | 2158 | `		}` |
|        21 | 2159 | `		if( bHave ){` |
|        19 | 2160 | `			ph7_value *pOut = ph7_context_new_scalar(pCtx);` |
|        19 | 2161 | `			if( pOut == 0 ){` |
|       ! 0 | 2162 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|       ! 0 | 2163 | `				ph7_result_bool(pCtx,0);` |
|       ! 0 | 2164 | `				return PH7_OK;` |
|         - | 2165 | `			}` |
|        19 | 2166 | `			VmUrlSetComponent(pOut,pComp);` |
|        19 | 2167 | `			ph7_result_value(pCtx,pOut);` |
|        10 | 2168 | `		}else{` |
|         - | 2169 | `			/* No available value,return NULL */` |
|         3 | 2170 | `			ph7_result_null(pCtx);` |
|         - | 2171 | `		}` |
|        11 | 2172 | `	}else{` |
|         - | 2173 | `		ph7_value *pArray,*pValue;` |
|         - | 2174 | `		/* Return an associative array */` |
|        67 | 2175 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|        67 | 2176 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|        67 | 2177 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|         - | 2178 | `			/* Out of memory */` |
|       ! 0 | 2179 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|         - | 2180 | `			/* Return false */` |
|       ! 0 | 2181 | `			ph7_result_bool(pCtx,0);` |
|       ! 0 | 2182 | `			return PH7_OK;` |
|         - | 2183 | `		}` |
|         - | 2184 | `		/* Fill the array, in php's key order. A component is emitted whenever it` |
|         - | 2185 | `		 * is PRESENT, even when empty -- parse_url("?") is ['query'=>'']. */` |
|        67 | 2186 | `		if( sUrl.bScheme ){` |
|        33 | 2187 | `			VmUrlSetComponent(pValue,&sUrl.sScheme);` |
|        33 | 2188 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        33 | 2189 | `			ph7_value_reset_string_cursor(pValue);` |
|        16 | 2190 | `		}` |
|        67 | 2191 | `		if( sUrl.bHost ){` |
|        31 | 2192 | `			VmUrlSetComponent(pValue,&sUrl.sHost);` |
|        31 | 2193 | `			ph7_array_add_strkey_elem(pArray,"host",pValue);` |
|        31 | 2194 | `			ph7_value_reset_string_cursor(pValue);` |
|        15 | 2195 | `		}` |
|        67 | 2196 | `		if( sUrl.bPort ){` |
|        17 | 2197 | `			ph7_value_int(pValue,sUrl.iPort);` |
|        17 | 2198 | `			ph7_array_add_strkey_elem(pArray,"port",pValue);` |
|        17 | 2199 | `			ph7_value_reset_string_cursor(pValue);` |
|         8 | 2200 | `		}` |
|        67 | 2201 | `		if( sUrl.bUser ){` |
|         9 | 2202 | `			VmUrlSetComponent(pValue,&sUrl.sUser);` |
|         9 | 2203 | `			ph7_array_add_strkey_elem(pArray,"user",pValue);` |
|         9 | 2204 | `			ph7_value_reset_string_cursor(pValue);` |
|         4 | 2205 | `		}` |
|        67 | 2206 | `		if( sUrl.bPass ){` |
|         7 | 2207 | `			VmUrlSetComponent(pValue,&sUrl.sPass);` |
|         7 | 2208 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue);` |
|         7 | 2209 | `			ph7_value_reset_string_cursor(pValue);` |
|         3 | 2210 | `		}` |
|        67 | 2211 | `		if( sUrl.bPath ){` |
|        47 | 2212 | `			VmUrlSetComponent(pValue,&sUrl.sPath);` |
|        47 | 2213 | `			ph7_array_add_strkey_elem(pArray,"path",pValue);` |
|        47 | 2214 | `			ph7_value_reset_string_cursor(pValue);` |
|        23 | 2215 | `		}` |
|        67 | 2216 | `		if( sUrl.bQuery ){` |
|        13 | 2217 | `			VmUrlSetComponent(pValue,&sUrl.sQuery);` |
|        13 | 2218 | `			ph7_array_add_strkey_elem(pArray,"query",pValue);` |
|        13 | 2219 | `			ph7_value_reset_string_cursor(pValue);` |
|         6 | 2220 | `		}` |
|        67 | 2221 | `		if( sUrl.bFragment ){` |
|        13 | 2222 | `			VmUrlSetComponent(pValue,&sUrl.sFragment);` |
|        13 | 2223 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue);` |
|         6 | 2224 | `		}` |
|         - | 2225 | `		/* Return the created array */` |
|        67 | 2226 | `		ph7_result_value(pCtx,pArray);` |
|         - | 2227 | `		/* NOTE:` |
|         - | 2228 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|         - | 2229 | `		 * automatically as soon we return from this function.` |
|         - | 2230 | `		 */` |
|         - | 2231 | `	}` |
|         - | 2232 | `	/* All done */` |
|        87 | 2233 | `	return PH7_OK;` |
|        53 | 2234 | `}` |
|         - | 2235 |  |
|         - | 2236 | `/*` |
|         - | 2237 | ` * Section:` |
|         - | 2238 | ` *   Array related routines.` |
|         - | 2239 | ` * Status:` |
|         - | 2240 | ` *    Stable.` |
|         - | 2241 | ` * Note 2012-5-21 01:04:15:` |
|         - | 2242 | ` *  Array related functions that need access to the underlying` |
|         - | 2243 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|         - | 2244 | ` */` |
|         - | 2245 | `/*` |
|         - | 2246 | ` * The [compact()] function store it's state information in an instance` |
|         - | 2247 | ` * of the following structure.` |
|         - | 2248 | ` */` |
|         - | 2249 | `struct compact_data` |
|         - | 2250 | `{` |
|         - | 2251 | `	ph7_value *pArray;  /* Target array */` |
|         - | 2252 | `	int nRecCount;      /* Recursion count */` |
|         - | 2253 | `	ph7_context *pCtx;  /* Call context, for php's two warnings */` |
|         - | 2254 | `	int iArg;           /* 1-based ARGUMENT this element came from: php names the` |
|         - | 2255 | `	                     * argument even for an element found inside a nested` |
|         - | 2256 | `	                     * array, never the element's own position. */` |
|         - | 2257 | `};` |
|         - | 2258 | `/*` |
|         - | 2259 | ` * php's two compact() diagnostics, both E_WARNING and both non-fatal (the entry` |
|         - | 2260 | ` * is skipped and the rest of the call proceeds): a name that is neither a string` |
|         - | 2261 | ` * nor an array of strings, and a string naming a variable the frame does not` |
|         - | 2262 | ` * have. PHL emitted neither, so compact(1) and compact('typo') answered a` |
|         - | 2263 | ` * shorter array with nothing said -- the caller's only clue that a name was` |
|         - | 2264 | ` * dropped was the array's own size.` |
|         - | 2265 | ` */` |
|        14 | 2266 | `static void VmCompactBadName(ph7_context *pCtx,int iArg,ph7_value *pValue)` |
|         1 | 2267 | `{` |
|         - | 2268 | `	char zGiven[64];` |
|        22 | 2269 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 2270 | `		"Argument #%d must be string or array of strings, %s given",` |
|         7 | 2271 | `		iArg,VmValueGivenName(pValue,zGiven,sizeof(zGiven)));` |
|        15 | 2272 | `}` |
|         8 | 2273 | `static void VmCompactUndefined(ph7_context *pCtx,SyString *pVar)` |
|         1 | 2274 | `{` |
|         8 | 2275 | `	if( pVar->nByte == sizeof("this")-1` |
|         6 | 2276 | `	 && SyMemcmp(pVar->zString,"this",sizeof("this")-1) == 0 ){` |
|         - | 2277 | ``		/* php's one silent miss here: `compact('this')` outside an object context`` |
|         - | 2278 | ``		 * skips the name and says nothing -- `$this` is not a variable, so it is`` |
|         - | 2279 | `		 * not an undefined one either, and compact() is the one door that neither` |
|         - | 2280 | `		 * warns nor throws for it. */` |
|         3 | 2281 | `		return;` |
|         - | 2282 | `	}` |
|        10 | 2283 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         6 | 2284 | `		"Undefined variable $%.*s",(int)pVar->nByte,pVar->zString);` |
|         5 | 2285 | `}` |
|         - | 2286 | `/*` |
|         - | 2287 | ` * Walker callback for the [compact()] function defined below.` |
|         - | 2288 | ` */` |
|        16 | 2289 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|         1 | 2290 | `{` |
|        17 | 2291 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|        17 | 2292 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|        17 | 2293 | `	ph7_vm *pVm = pArray->pVm;` |
|         - | 2294 | `	/* Act according to the hashmap value */` |
|        17 | 2295 | `	if( ph7_value_is_string(pValue) ){` |
|         - | 2296 | `		SyString sVar;` |
|         9 | 2297 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|         - | 2298 | `		/* Query the current frame. An EMPTY name is looked up like any other:` |
|         - | 2299 | `		 * php reports it as "Undefined variable $" rather than skipping it. */` |
|         9 | 2300 | `		pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|         - | 2301 | `		/* ^` |
|         - | 2302 | `		 * \| Avoid wasting variable and use 'pKey' instead` |
|         - | 2303 | `		 */` |
|         9 | 2304 | `		if( pKey ){` |
|         - | 2305 | `			/* Perform the insertion */` |
|         7 | 2306 | `			ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|         4 | 2307 | `		}else{` |
|         3 | 2308 | `			VmCompactUndefined(pData->pCtx,&sVar);` |
|         1 | 2309 | `		}` |
|        13 | 2310 | `	}else if( ph7_value_is_array(pValue) ){` |
|         - | 2311 | `		/* Recursively traverse this array. Past the depth cap the element is` |
|         - | 2312 | `		 * dropped in silence, as it always was: the cap is PHL's own guard and` |
|         - | 2313 | `		 * the "must be string or array of strings" warning would be a lie about` |
|         - | 2314 | `		 * an argument that IS an array of strings. */` |
|         7 | 2315 | `		if( pData->nRecCount < 32 ){` |
|         - | 2316 | `			int rc;` |
|         7 | 2317 | `			pData->nRecCount++;` |
|         7 | 2318 | `			rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|         7 | 2319 | `			pData->nRecCount--;` |
|         7 | 2320 | `			return rc;` |
|         - | 2321 | `		}` |
|       ! 0 | 2322 | `	}else{` |
|         3 | 2323 | `		VmCompactBadName(pData->pCtx,pData->iArg,pValue);` |
|         - | 2324 | `	}` |
|        11 | 2325 | `	return SXRET_OK;` |
|         9 | 2326 | `}` |
|         - | 2327 | `/*` |
|         - | 2328 | ` * array compact(mixed $varname [, mixed $... ])` |
|         - | 2329 | ` *  Create array containing variables and their values.` |
|         - | 2330 | ` *  For each of these, compact() looks for a variable with that name` |
|         - | 2331 | ` *  in the current symbol table and adds it to the output array such` |
|         - | 2332 | ` *  that the variable name becomes the key and the contents of the variable` |
|         - | 2333 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|         - | 2334 | ` *  Any strings that are not set will simply be skipped.` |
|         - | 2335 | ` * Parameters` |
|         - | 2336 | ` *  $varname` |
|         - | 2337 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|         - | 2338 | ` *   a string containing the name of the variable, or an array of variable names.` |
|         - | 2339 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|         - | 2340 | ` *   it recursively.` |
|         - | 2341 | ` * Return` |
|         - | 2342 | ` *  The output array with all the variables added to it or NULL on failure` |
|         - | 2343 | ` */` |
|        76 | 2344 | `PH7_PRIVATE int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 2345 | `{` |
|         - | 2346 | `	ph7_value *pArray,*pObj;` |
|        79 | 2347 | `	ph7_vm *pVm = pCtx->pVm;` |
|         - | 2348 | `	const char *zName;` |
|         - | 2349 | `	SyString sVar;` |
|         - | 2350 | `	int i,nLen;` |
|        79 | 2351 | `	if( nArg < 1 ){` |
|         - | 2352 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 2353 | `		ph7_result_null(pCtx);` |
|       ! 0 | 2354 | `		return PH7_OK;` |
|         - | 2355 | `	}` |
|         - | 2356 | `	/* php screens the arity first (above, and the arity table before it), then the` |
|         - | 2357 | `	 * call shape: the names are looked up in the CALLER's scope, which a dynamic` |
|         - | 2358 | `	 * call does not have. */` |
|         - | 2359 | `	{` |
|        79 | 2360 | `		sxi32 rc = PH7_VmForbidDynamicCall(pCtx);` |
|        79 | 2361 | `		if( rc != PH7_OK ){` |
|        36 | 2362 | `			return rc;` |
|         - | 2363 | `		}` |
|         - | 2364 | `	}` |
|         - | 2365 | `	/* Create the array */` |
|        45 | 2366 | `	pArray = ph7_context_new_array(pCtx);` |
|        45 | 2367 | `	if( pArray == 0 ){` |
|         - | 2368 | `		/* Out of memory */` |
|       ! 0 | 2369 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|         - | 2370 | `		/* Return NULL */` |
|       ! 0 | 2371 | `		ph7_result_null(pCtx);` |
|       ! 0 | 2372 | `		return PH7_OK;` |
|         - | 2373 | `	}` |
|         - | 2374 | `	/* Perform the requested operation */` |
|        99 | 2375 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        57 | 2376 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|        19 | 2377 | `			if( ph7_value_is_array(apArg[i]) ){` |
|         - | 2378 | `				struct compact_data sData;` |
|         7 | 2379 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 2380 | `				/* Recursively walk the array */` |
|         7 | 2381 | `				sData.nRecCount = 0;` |
|         7 | 2382 | `				sData.pArray = pArray;` |
|         7 | 2383 | `				sData.pCtx = pCtx;` |
|         7 | 2384 | `				sData.iArg = i + 1;` |
|         7 | 2385 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|         4 | 2386 | `			}else{` |
|        13 | 2387 | `				VmCompactBadName(pCtx,i + 1,apArg[i]);` |
|         - | 2388 | `			}` |
|        10 | 2389 | `		}else{` |
|         - | 2390 | `			/* Extract variable name. An EMPTY one is looked up like any other:` |
|         - | 2391 | `			 * php reports it as "Undefined variable $" rather than skipping it. */` |
|        39 | 2392 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        39 | 2393 | `			SyStringInitFromBuf(&sVar,zName,nLen);` |
|         - | 2394 | `			/* Check if the variable is available in the current frame */` |
|        39 | 2395 | `			pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        39 | 2396 | `			if( pObj ){` |
|        33 | 2397 | `				ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        18 | 2398 | `			}else{` |
|         7 | 2399 | `				VmCompactUndefined(pCtx,&sVar);` |
|         - | 2400 | `			}` |
|         - | 2401 | `		}` |
|        30 | 2402 | `	}` |
|         - | 2403 | `	/* Return the array */` |
|        45 | 2404 | `	ph7_result_value(pCtx,pArray);` |
|        45 | 2405 | `	return PH7_OK;` |
|        41 | 2406 | `}` |
|         - | 2407 | `/*` |
|         - | 2408 | ` * The [import_request_variables()] function store it's state information` |
|         - | 2409 | ` * in an instance of the following structure.` |
|         - | 2410 | ` */` |
|         - | 2411 | `typedef struct extract_aux_data extract_aux_data;` |
|         - | 2412 | `struct extract_aux_data` |
|         - | 2413 | `{` |
|         - | 2414 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|         - | 2415 | `	int iCount;           /* Number of variables successfully imported  */` |
|         - | 2416 | `	const char *zPrefix;  /* Prefix name */` |
|         - | 2417 | `	int Prefixlen;        /* Prefix  length */` |
|         - | 2418 | `	char zWorker[1024];   /* Working buffer */` |
|         - | 2419 | `};` |
|         - | 2420 | `/*` |
|         - | 2421 | ` * php's php_valid_var_name(): a legal PHP variable name matches` |
|         - | 2422 | ` * [A-Za-z_\x80-\xff][A-Za-z0-9_\x80-\xff]* . Byte-wise and locale-free on` |
|         - | 2423 | ` * purpose (php has been locale-independent here since 8.0); the high-byte` |
|         - | 2424 | ` * range is what lets UTF-8 identifiers through. extract() drops every key` |
|         - | 2425 | ` * that does not pass, instead of installing an unreachable variable.` |
|         - | 2426 | ` */` |
|       170 | 2427 | `static int VmIsValidVarName(const char *zName,sxu32 nByte)` |
|         4 | 2428 | `{` |
|         - | 2429 | `	unsigned char c;` |
|         - | 2430 | `	sxu32 i;` |
|       174 | 2431 | `	if( nByte < 1 ){` |
|         7 | 2432 | `		return FALSE;` |
|         - | 2433 | `	}` |
|       168 | 2434 | `	c = (unsigned char)zName[0];` |
|       168 | 2435 | `	if( c != '_' && !(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') && c < 0x80 ){` |
|        14 | 2436 | `		return FALSE;` |
|         - | 2437 | `	}` |
|       400 | 2438 | `	for( i = 1 ; i < nByte ; ++i ){` |
|       265 | 2439 | `		c = (unsigned char)zName[i];` |
|       262 | 2440 | `		if( c != '_' && !(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z')` |
|        80 | 2441 | `		 && !(c >= '0' && c <= '9') && c < 0x80 ){` |
|        20 | 2442 | `			return FALSE;` |
|         - | 2443 | `		}` |
|       125 | 2444 | `	}` |
|       138 | 2445 | `	return TRUE;` |
|        89 | 2446 | `}` |
|         - | 2447 | `/* TRUE when the name is exactly "this": php refuses to re-assign $this. */` |
|       176 | 2448 | `static int VmExtractIsThis(const char *zName,sxu32 nByte)` |
|         4 | 2449 | `{` |
|       180 | 2450 | `	return nByte == sizeof("this")-1 && SyMemcmp(zName,"this",sizeof("this")-1) == 0;` |
|         4 | 2451 | `}` |
|         - | 2452 | `/*` |
|         - | 2453 | ` * TRUE when the name belongs to the superglobal table ($GLOBALS, $_SERVER,` |
|         - | 2454 | ` * $_GET, …). php hands extract() a per-frame symbol table that holds no` |
|         - | 2455 | ` * superglobal, so such a key lands in the LOCAL table and the real superglobal` |
|         - | 2456 | ` * is untouched. In PHL the name resolves to the superglobal SLOT itself` |
|         - | 2457 | ` * (VmExtractMemObj consults hSuper first), so a plain store would replace` |
|         - | 2458 | ` * $GLOBALS/$_SERVER with the imported value and take the whole symbol table /` |
|         - | 2459 | ` * request environment with it. Those keys are dropped instead — a prefixed` |
|         - | 2460 | ` * name ($p__SERVER) is a normal local and stores fine.` |
|         - | 2461 | ` */` |
|       124 | 2462 | `static int VmExtractIsProtected(ph7_vm *pVm,const char *zName,sxu32 nByte)` |
|         4 | 2463 | `{` |
|       128 | 2464 | `	return PH7_VmSuperGet(&(*pVm),zName,nByte) != 0;` |
|         4 | 2465 | `}` |
|         - | 2466 | `/*` |
|         - | 2467 | ` * TRUE when the calling frame already holds this variable name.` |
|         - | 2468 | ` * "this" and the superglobals always answer FALSE, matching the symbol table` |
|         - | 2469 | ` * php hands extract(): $this is bound implicitly and superglobals live outside` |
|         - | 2470 | ` * the frame. So EXTR_IF_EXISTS/EXTR_PREFIX_IF_EXISTS skip those keys (php-exact)` |
|         - | 2471 | ` * and EXTR_PREFIX_SAME takes its not-a-collision branch.` |
|         - | 2472 | ` */` |
|        72 | 2473 | `static int VmExtractVarExists(ph7_vm *pVm,const char *zName,sxu32 nByte)` |
|         4 | 2474 | `{` |
|         - | 2475 | `	SyString sVar;` |
|        76 | 2476 | `	if( VmExtractIsThis(zName,nByte) \|\| VmExtractIsProtected(pVm,zName,nByte) ){` |
|        20 | 2477 | `		return FALSE;` |
|         - | 2478 | `	}` |
|        57 | 2479 | `	SyStringInitFromBuf(&sVar,zName,nByte);` |
|        57 | 2480 | `	return VmExtractMemObj(pVm,&sVar,FALSE,FALSE) != 0;` |
|        40 | 2481 | `}` |
|         - | 2482 | `/*` |
|         - | 2483 | ` * Create-or-overwrite a variable of the calling frame with a copy of pValue.` |
|         - | 2484 | ` * Returns TRUE when the variable was written (php counts exactly those).` |
|         - | 2485 | ` */` |
|         - | 2486 | `/*` |
|         - | 2487 | ` * EXTR_REFS: bind the imported NAME to the array element's own slot instead of copying` |
|         - | 2488 | ` * the value, so a later write through the variable reaches the caller's array — php's` |
|         - | 2489 | ` * by-reference extraction. The binding is registered (PH7_VmBindVarSlot), which is what` |
|         - | 2490 | ` * makes the element count the new name as a holder and read as a reference.` |
|         - | 2491 | ` *` |
|         - | 2492 | ` * The name is DUPLICATED: the symbol table keeps the pointer it is given, and the caller's` |
|         - | 2493 | ` * is a scratch blob the next entry reuses.` |
|         - | 2494 | ` */` |
|         8 | 2495 | `static int VmExtractBindVar(ph7_vm *pVm,const char *zName,sxu32 nByte,sxu32 nIdx)` |
|         1 | 2496 | `{` |
|         9 | 2497 | `	VmFrame *pFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|         - | 2498 | `	char *zDup;` |
|         9 | 2499 | `	if( nIdx == SXU32_HIGH ){` |
|       ! 0 | 2500 | `		return FALSE;` |
|         - | 2501 | `	}` |
|         9 | 2502 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zName,nByte);` |
|         9 | 2503 | `	if( zDup == 0 ){` |
|       ! 0 | 2504 | `		return FALSE;` |
|         - | 2505 | `	}` |
|         9 | 2506 | `	PH7_VmBindVarSlot(&(*pVm),pFrame,zDup,nByte,nIdx);` |
|         9 | 2507 | `	return TRUE;` |
|         5 | 2508 | `}` |
|        72 | 2509 | `static int VmExtractStoreVar(ph7_vm *pVm,const char *zName,sxu32 nByte,ph7_value *pValue)` |
|         4 | 2510 | `{` |
|         - | 2511 | `	ph7_value *pObj;` |
|         - | 2512 | `	SyString sVar;` |
|        76 | 2513 | `	SyStringInitFromBuf(&sVar,zName,nByte);` |
|         - | 2514 | `	/* bDup: the name lives in a scratch blob that is reused by the next entry */` |
|        76 | 2515 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        76 | 2516 | `	if( pObj == 0 ){` |
|       ! 0 | 2517 | `		return FALSE;` |
|         - | 2518 | `	}` |
|        76 | 2519 | `	PH7_MemObjStore(pValue,pObj);` |
|        76 | 2520 | `	return TRUE;` |
|        40 | 2521 | `}` |
|         - | 2522 | `/*` |
|         - | 2523 | ` * Build php's prefixed name "<prefix>_<key>" (php_prefix_varname with` |
|         - | 2524 | ` * add_underscore): the separator is unconditional, so an empty prefix still` |
|         - | 2525 | ` * yields "_key" exactly like php.` |
|         - | 2526 | ` */` |
|        40 | 2527 | `static sxi32 VmExtractPrefixName(SyBlob *pOut,const char *zPrefix,int nPrefix,` |
|         - | 2528 | `	const char *zKey,sxu32 nKey)` |
|         3 | 2529 | `{` |
|        43 | 2530 | `	SyBlobReset(pOut);` |
|        43 | 2531 | `	if( nPrefix > 0 && SyBlobAppend(pOut,zPrefix,(sxu32)nPrefix) != SXRET_OK ){` |
|       ! 0 | 2532 | `		return SXERR_MEM;` |
|         - | 2533 | `	}` |
|        43 | 2534 | `	if( SyBlobAppend(pOut,"_",sizeof(char)) != SXRET_OK ){` |
|       ! 0 | 2535 | `		return SXERR_MEM;` |
|         - | 2536 | `	}` |
|        43 | 2537 | `	if( nKey > 0 && SyBlobAppend(pOut,zKey,nKey) != SXRET_OK ){` |
|       ! 0 | 2538 | `		return SXERR_MEM;` |
|         - | 2539 | `	}` |
|        43 | 2540 | `	return SXRET_OK;` |
|        23 | 2541 | `}` |
|         - | 2542 | `/* What to do with one array entry, decided by the extract mode. */` |
|         - | 2543 | `#define VM_EXTRACT_DROP     0 /* php skips this key entirely */` |
|         - | 2544 | `#define VM_EXTRACT_PLAIN    1 /* install under the key itself */` |
|         - | 2545 | `#define VM_EXTRACT_PREFIX   2 /* install under "<prefix>_<key>" */` |
|         - | 2546 | `/*` |
|         - | 2547 | ` * int extract(array &$array[,int $flags = EXTR_OVERWRITE[,string $prefix = "" ]])` |
|         - | 2548 | ` *   Import variables into the current symbol table from an array.` |
|         - | 2549 | ` *` |
|         - | 2550 | ` * $flags is php's ENUM (PH7_EXTR_* in ph7int.h), not a bitmask: the mode is` |
|         - | 2551 | ` * (flags & 0xff) and anything above EXTR_IF_EXISTS(6) is a ValueError. The` |
|         - | 2552 | ` * modes, mirroring php's per-mode helpers in ext/standard/array.c:` |
|         - | 2553 | ` *   EXTR_OVERWRITE(0)        collisions overwrite` |
|         - | 2554 | ` *   EXTR_SKIP(1)             collisions keep the existing variable` |
|         - | 2555 | ` *   EXTR_PREFIX_SAME(2)      collisions install "<prefix>_<key>"` |
|         - | 2556 | ` *   EXTR_PREFIX_ALL(3)       every key installs as "<prefix>_<key>"` |
|         - | 2557 | ` *   EXTR_PREFIX_INVALID(4)   only illegal names (and numeric keys) get prefixed` |
|         - | 2558 | ` *   EXTR_PREFIX_IF_EXISTS(5) install "<prefix>_<key>" only if <key> exists` |
|         - | 2559 | ` *   EXTR_IF_EXISTS(6)        overwrite only variables that already exist` |
|         - | 2560 | `` * Modes 2..5 require $prefix (php: `is required when using this extract type`),`` |
|         - | 2561 | ` * a non-empty $prefix must itself be a legal identifier, and a key whose final` |
|         - | 2562 | ` * name is not a legal variable name is dropped rather than installed. Only` |
|         - | 2563 | ` * EXTR_PREFIX_ALL/EXTR_PREFIX_INVALID look at numeric keys at all.` |
|         - | 2564 | ` *` |
|         - | 2565 | `` * $this is never a target: php throws `Cannot re-assign $this` where a store`` |
|         - | 2566 | ` * would land on it, and skips it where a store would not (EXTR_SKIP), and` |
|         - | 2567 | ` * $GLOBALS is never clobbered.` |
|         - | 2568 | ` * Return` |
|         - | 2569 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|         - | 2570 | ` */` |
|       120 | 2571 | `PH7_PRIVATE int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 2572 | `{` |
|       124 | 2573 | `	ph7_vm *pVm = pCtx->pVm;` |
|         - | 2574 | `	ph7_hashmap_node *pEntry;` |
|         - | 2575 | `	ph7_hashmap *pMap;` |
|       124 | 2576 | `	const char *zPrefix = 0;` |
|       124 | 2577 | `	sxi64 iFlags = PH7_EXTR_OVERWRITE;` |
|       124 | 2578 | `	sxi64 iCount = 0;` |
|         - | 2579 | `	ph7_value sValue;` |
|         - | 2580 | `	SyBlob sWorker;` |
|       124 | 2581 | `	int nPrefix = 0;` |
|       124 | 2582 | `	sxi32 rc = PH7_OK;` |
|         - | 2583 | `	int iType;` |
|         - | 2584 | `	sxu32 n;` |
|       124 | 2585 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 2586 | `		char zBuf[64];` |
|       ! 0 | 2587 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 2588 | `			"extract(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 2589 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|         - | 2590 | `	}` |
|       124 | 2591 | `	if( nArg > 1 ){` |
|       104 | 2592 | `		rc = PH7_IntArgResolve(pCtx,apArg[1],"extract",2,"$flags","int",&iFlags);` |
|       104 | 2593 | `		if( rc != PH7_OK ){` |
|       ! 0 | 2594 | `			return rc;` |
|         - | 2595 | `		}` |
|        50 | 2596 | `	}` |
|         - | 2597 | `	/* php: the mode is the low byte; EXTR_REFS(0x100) rides above it */` |
|       124 | 2598 | `	iType = (int)(iFlags & 0xff);` |
|       124 | 2599 | `	if( iType > PH7_EXTR_IF_EXISTS ){` |
|        10 | 2600 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 2601 | `			"extract(): Argument #2 ($flags) must be a valid extract type");` |
|         - | 2602 | `	}` |
|       116 | 2603 | `	if( iType > PH7_EXTR_SKIP && iType <= PH7_EXTR_PREFIX_IF_EXISTS && nArg < 3 ){` |
|        14 | 2604 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 2605 | `			"extract(): Argument #3 ($prefix) is required when using this extract type");` |
|         - | 2606 | `	}` |
|       104 | 2607 | `	if( nArg > 2 ){` |
|        43 | 2608 | `		zPrefix = ph7_value_to_string(apArg[2],&nPrefix);` |
|        43 | 2609 | `		if( nPrefix > 0 && !VmIsValidVarName(zPrefix,(sxu32)nPrefix) ){` |
|         8 | 2610 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 2611 | `				"extract(): Argument #3 ($prefix) must be a valid identifier");` |
|         - | 2612 | `		}` |
|        17 | 2613 | `	}` |
|         - | 2614 | `	/* Every argument screen above runs first in php too; the call shape is the last` |
|         - | 2615 | `	 * refusal before the walk, because the walk writes into the CALLER's scope. */` |
|        98 | 2616 | `	if( (rc = PH7_VmForbidDynamicCall(pCtx)) != PH7_OK ){` |
|         7 | 2617 | `		return rc;` |
|         - | 2618 | `	}` |
|         - | 2619 | `	/* Point to the target hashmap */` |
|        92 | 2620 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        92 | 2621 | `	if( pMap->nEntry < 1 ){` |
|         - | 2622 | `		/* Empty map,return  0 */` |
|       ! 0 | 2623 | `		ph7_result_int(pCtx,0);` |
|       ! 0 | 2624 | `		return PH7_OK;` |
|         - | 2625 | `	}` |
|        92 | 2626 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        92 | 2627 | `	PH7_MemObjInit(pVm,&sValue);` |
|         - | 2628 | `	/* php walks a COPY of the array, so an entry that overwrites the caller's own` |
|         - | 2629 | `	 * array variable ($arr = ['arr'=>1]; extract($arr)) cannot pull the map from` |
|         - | 2630 | `	 * under the walk. Pinning the map for the walk is the same guarantee. */` |
|        92 | 2631 | `	pMap->iRef++;` |
|        92 | 2632 | `	pEntry = pMap->pFirst;` |
|         - | 2633 | `	/* pFirst walks the insertion order through pPrev — PH7 links the entry list` |
|         - | 2634 | `	 * in reverse (same traversal PH7_HashmapWalk uses). */` |
|       264 | 2635 | `	for( n = pMap->nEntry ; n > 0 && pEntry ; --n, pEntry = pEntry->pPrev ){` |
|         - | 2636 | `		const char *zKey, *zFinal;` |
|         - | 2637 | `		sxu32 nKey, nFinal;` |
|         - | 2638 | `		char zNum[32];` |
|         - | 2639 | `		int bIntKey, iAction;` |
|         - | 2640 | `		/* Work off a COPY of the entry value. Installing a variable used to grow` |
|         - | 2641 | `		 * pVm->aMemObj and dangle a pointer into it (this is why the walk API` |
|         - | 2642 | `		 * hands out copies too); P1's fixed segments retired that, but the copy` |
|         - | 2643 | `		 * still earns its place for the reference discipline below. The` |
|         - | 2644 | `		 * release comes FIRST so it covers every continue/goto below: the load` |
|         - | 2645 | `		 * takes a reference on an array/object value and does not drop the one` |
|         - | 2646 | `		 * the previous entry left behind (PH7_HashmapWalk releases per iteration` |
|         - | 2647 | `		 * for the same reason — without it a whole-array extract() pins every` |
|         - | 2648 | `		 * value it copied, and their destructors never run). */` |
|       178 | 2649 | `		PH7_MemObjRelease(&sValue);` |
|       178 | 2650 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|       178 | 2651 | `		bIntKey = (pEntry->iType == HASHMAP_INT_NODE);` |
|       178 | 2652 | `		if( bIntKey ){` |
|         - | 2653 | `			/* Only the two prefixing modes below ever look at a numeric key */` |
|        22 | 2654 | `			nKey = SyBufferFormat(zNum,sizeof(zNum),"%qd",pEntry->xKey.iKey);` |
|        22 | 2655 | `			zKey = zNum;` |
|        12 | 2656 | `		}else{` |
|       158 | 2657 | `			zKey = (const char *)SyBlobData(&pEntry->xKey.sKey);` |
|       158 | 2658 | `			nKey = SyBlobLength(&pEntry->xKey.sKey);` |
|         - | 2659 | `		}` |
|       178 | 2660 | `		iAction = VM_EXTRACT_DROP;` |
|       178 | 2661 | `		switch( iType ){` |
|        22 | 2662 | `		case PH7_EXTR_OVERWRITE:` |
|        48 | 2663 | `			if( bIntKey \|\| !VmIsValidVarName(zKey,nKey) ){` |
|         8 | 2664 | `				break;` |
|         - | 2665 | `			}` |
|        36 | 2666 | `			if( VmExtractIsThis(zKey,nKey) ){` |
|         3 | 2667 | `				goto this_error;` |
|         - | 2668 | `			}` |
|        34 | 2669 | `			iAction = VM_EXTRACT_PLAIN;` |
|        34 | 2670 | `			break;` |
|        12 | 2671 | `		case PH7_EXTR_SKIP:` |
|        27 | 2672 | `			if( bIntKey \|\| !VmIsValidVarName(zKey,nKey) \|\| VmExtractIsThis(zKey,nKey) ){` |
|         6 | 2673 | `				break;` |
|         - | 2674 | `			}` |
|        17 | 2675 | `			if( VmExtractVarExists(pVm,zKey,nKey) ){` |
|         8 | 2676 | `				break; /* collision: keep the existing variable */` |
|         - | 2677 | `			}` |
|        10 | 2678 | `			iAction = VM_EXTRACT_PLAIN;` |
|        10 | 2679 | `			break;` |
|        16 | 2680 | `		case PH7_EXTR_IF_EXISTS:` |
|        36 | 2681 | `			if( bIntKey \|\| !VmExtractVarExists(pVm,zKey,nKey) ){` |
|        14 | 2682 | `				break;` |
|         - | 2683 | `			}` |
|        12 | 2684 | `			if( !VmIsValidVarName(zKey,nKey) ){` |
|       ! 0 | 2685 | `				break;` |
|         - | 2686 | `			}` |
|        12 | 2687 | `			if( VmExtractIsThis(zKey,nKey) ){` |
|       ! 0 | 2688 | `				goto this_error;` |
|         - | 2689 | `			}` |
|        12 | 2690 | `			iAction = VM_EXTRACT_PLAIN;` |
|        12 | 2691 | `			break;` |
|         9 | 2692 | `		case PH7_EXTR_PREFIX_SAME:` |
|        21 | 2693 | `			if( bIntKey \|\| nKey < 1 ){` |
|         3 | 2694 | `				break;` |
|         - | 2695 | `			}` |
|        17 | 2696 | `			if( VmExtractVarExists(pVm,zKey,nKey) ){` |
|         6 | 2697 | `				iAction = VM_EXTRACT_PREFIX; /* collision */` |
|        14 | 2698 | `			}else if( !VmIsValidVarName(zKey,nKey) ){` |
|         5 | 2699 | `				break;` |
|       ! 0 | 2700 | `			}else{` |
|         - | 2701 | `				/* $this cannot be a target, but its prefixed form can */` |
|         8 | 2702 | `				iAction = VmExtractIsThis(zKey,nKey) ? VM_EXTRACT_PREFIX : VM_EXTRACT_PLAIN;` |
|         - | 2703 | `			}` |
|        13 | 2704 | `			break;` |
|        13 | 2705 | `		case PH7_EXTR_PREFIX_ALL:` |
|        28 | 2706 | `			if( !bIntKey && nKey < 1 ){` |
|         3 | 2707 | `				break;` |
|         - | 2708 | `			}` |
|        26 | 2709 | `			iAction = VM_EXTRACT_PREFIX;` |
|        26 | 2710 | `			break;` |
|         7 | 2711 | `		case PH7_EXTR_PREFIX_INVALID:` |
|        15 | 2712 | `			iAction = (bIntKey \|\| !VmIsValidVarName(zKey,nKey) \|\| VmExtractIsThis(zKey,nKey))` |
|        13 | 2713 | `				? VM_EXTRACT_PREFIX : VM_EXTRACT_PLAIN;` |
|        16 | 2714 | `			break;` |
|         8 | 2715 | `		case PH7_EXTR_PREFIX_IF_EXISTS:` |
|        18 | 2716 | `			if( !bIntKey && VmExtractVarExists(pVm,zKey,nKey) ){` |
|         3 | 2717 | `				iAction = VM_EXTRACT_PREFIX;` |
|         1 | 2718 | `			}` |
|        16 | 2719 | `			break;` |
|       ! 0 | 2720 | `		default:` |
|       ! 0 | 2721 | `			break;` |
|         - | 2722 | `		}` |
|       176 | 2723 | `		if( iAction == VM_EXTRACT_DROP ){` |
|       127 | 2724 | `			continue;` |
|         - | 2725 | `		}` |
|       102 | 2726 | `		if( iAction == VM_EXTRACT_PREFIX ){` |
|        43 | 2727 | `			if( VmExtractPrefixName(&sWorker,zPrefix,nPrefix,zKey,nKey) != SXRET_OK ){` |
|       ! 0 | 2728 | `				rc = PH7_VmThrowException(pCtx,"Error","PH7 engine is running out of memory");` |
|       ! 0 | 2729 | `				goto done;` |
|         - | 2730 | `			}` |
|        43 | 2731 | `			zFinal = (const char *)SyBlobData(&sWorker);` |
|        43 | 2732 | `			nFinal = SyBlobLength(&sWorker);` |
|        43 | 2733 | `			if( !VmIsValidVarName(zFinal,nFinal) ){` |
|         7 | 2734 | `				continue; /* php drops a prefixed name that is not an identifier */` |
|         - | 2735 | `			}` |
|        37 | 2736 | `			if( VmExtractIsThis(zFinal,nFinal) ){` |
|       ! 0 | 2737 | `				goto this_error;` |
|         - | 2738 | `			}` |
|        20 | 2739 | `		}else{` |
|        62 | 2740 | `			if( VmExtractIsProtected(pVm,zKey,nKey) ){` |
|        13 | 2741 | `				continue; /* $GLOBALS/$_SERVER/... are never a plain target */` |
|         - | 2742 | `			}` |
|        49 | 2743 | `			zFinal = zKey;` |
|        49 | 2744 | `			nFinal = nKey;` |
|         - | 2745 | `		}` |
|        84 | 2746 | `		if( iFlags & PH7_EXTR_REFS ){` |
|         - | 2747 | `			/* Bind to the element's own slot (php's EXTR_REFS) rather than copying */` |
|         9 | 2748 | `			if( VmExtractBindVar(pVm,zFinal,nFinal,pEntry->nValIdx) ){` |
|         9 | 2749 | `				iCount++;` |
|         5 | 2750 | `			}` |
|        80 | 2751 | `		}else if( VmExtractStoreVar(pVm,zFinal,nFinal,&sValue) ){` |
|        76 | 2752 | `			iCount++;` |
|        36 | 2753 | `		}` |
|        84 | 2754 | `		continue;` |
|         1 | 2755 | `this_error:` |
|         3 | 2756 | `		rc = PH7_VmThrowException(pCtx,"Error","Cannot re-assign $this");` |
|         3 | 2757 | `		goto done;` |
|       ! 0 | 2758 | `	}` |
|         - | 2759 | `	/* Number of variables successfully imported */` |
|        90 | 2760 | `	ph7_result_int64(pCtx,iCount);` |
|        44 | 2761 | `done:` |
|        92 | 2762 | `	PH7_MemObjRelease(&sValue);` |
|        92 | 2763 | `	SyBlobRelease(&sWorker);` |
|        92 | 2764 | `	PH7_HashmapUnref(pMap);` |
|        92 | 2765 | `	return rc;` |
|        64 | 2766 | `}` |
|         - | 2767 | `/*` |
|         - | 2768 | ` * Worker callback for the [import_request_variables()] function` |
|         - | 2769 | ` * defined below.` |
|         - | 2770 | ` */` |
|         2 | 2771 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|         1 | 2772 | `{` |
|         3 | 2773 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|         3 | 2774 | `	ph7_vm *pVm = pAux->pVm;` |
|         - | 2775 | `	ph7_value *pObj;` |
|         - | 2776 | `	SyString sVar;` |
|         - | 2777 | `	/* Perform a string cast */` |
|         3 | 2778 | `	PH7_MemObjToString(pKey);` |
|         3 | 2779 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|         - | 2780 | `		/* Unavailable variable name */` |
|       ! 0 | 2781 | `		return SXRET_OK;` |
|         - | 2782 | `	}` |
|         3 | 2783 | `	sVar.nByte = 0; /* cc warning */` |
|         3 | 2784 | `	if( pAux->Prefixlen > 0 ){` |
|         4 | 2785 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|         1 | 2786 | `			pAux->Prefixlen,pAux->zPrefix,` |
|         1 | 2787 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|         - | 2788 | `			);` |
|         2 | 2789 | `	}else{` |
|       ! 0 | 2790 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|       ! 0 | 2791 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|         - | 2792 | `	}` |
|         3 | 2793 | `	sVar.zString = pAux->zWorker;` |
|         - | 2794 | `	/* Extract the variable */` |
|         3 | 2795 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|         3 | 2796 | `	if( pObj ){` |
|         3 | 2797 | `		PH7_MemObjStore(pValue,pObj);` |
|         1 | 2798 | `	}` |
|         3 | 2799 | `	return SXRET_OK;` |
|         2 | 2800 | `}` |
|         - | 2801 | `/*` |
|         - | 2802 | ` * bool import_request_variables(string $types[,string $prefix])` |
|         - | 2803 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|         - | 2804 | ` * Parameters` |
|         - | 2805 | ` * $types` |
|         - | 2806 | ` *  Using the types parameter, you can specify which request variables to import.` |
|         - | 2807 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|         - | 2808 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|         - | 2809 | ` *  POST includes the POST uploaded file information.` |
|         - | 2810 | ` *  Note:` |
|         - | 2811 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|         - | 2812 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|         - | 2813 | ` * $prefix` |
|         - | 2814 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|         - | 2815 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|         - | 2816 | ` *  variable named $pref_userid.` |
|         - | 2817 | ` * Return` |
|         - | 2818 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2819 | ` */` |
|         2 | 2820 | `PH7_PRIVATE int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2821 | `{` |
|         - | 2822 | `	const char *zPrefix,*zEnd,*zImport;` |
|         - | 2823 | `	extract_aux_data sAux;` |
|         - | 2824 | `	int nLen,nPrefixLen;` |
|         - | 2825 | `	ph7_value *pSuper;` |
|         - | 2826 | `	ph7_vm *pVm;` |
|         - | 2827 | `	/* By default import only $_GET variables  */` |
|         3 | 2828 | `	zImport = "G";` |
|         3 | 2829 | `	nLen = (int)sizeof(char);` |
|         3 | 2830 | `	zPrefix = 0;` |
|         3 | 2831 | `	nPrefixLen = 0;` |
|         3 | 2832 | `	if( nArg > 0 ){` |
|         3 | 2833 | `		if( ph7_value_is_string(apArg[0]) ){` |
|         3 | 2834 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|         1 | 2835 | `		}` |
|         3 | 2836 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|         3 | 2837 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|         1 | 2838 | `		}` |
|         1 | 2839 | `	}` |
|         - | 2840 | `	/* Point to the underlying VM */` |
|         3 | 2841 | `	pVm = pCtx->pVm;` |
|         - | 2842 | `	/* Initialize the aux data */` |
|         3 | 2843 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|         3 | 2844 | `	sAux.zPrefix = zPrefix;` |
|         3 | 2845 | `	sAux.Prefixlen = nPrefixLen;` |
|         3 | 2846 | `	sAux.pVm = pVm;` |
|         - | 2847 | `	/* Extract */` |
|         3 | 2848 | `	zEnd = &zImport[nLen];` |
|         5 | 2849 | `	while( zImport < zEnd ){` |
|         3 | 2850 | `		int c = zImport[0];` |
|         3 | 2851 | `		pSuper = 0;` |
|         3 | 2852 | `		if( c == 'G' \|\| c == 'g' ){` |
|         - | 2853 | `			/* Import $_GET variables */` |
|         3 | 2854 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|         1 | 2855 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|         - | 2856 | `			/* Import $_POST variables */` |
|       ! 0 | 2857 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|       ! 0 | 2858 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|         - | 2859 | `			/* Import $_COOKIE variables */` |
|       ! 0 | 2860 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|       ! 0 | 2861 | `		}` |
|         3 | 2862 | `		if( pSuper ){` |
|         - | 2863 | `			/* Iterate throw array entries */` |
|         3 | 2864 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|         1 | 2865 | `		}` |
|         - | 2866 | `		/* Advance the cursor */` |
|         3 | 2867 | `		zImport++;` |
|         1 | 2868 | `	}` |
|         - | 2869 | `	/* All done,return TRUE*/` |
|         3 | 2870 | `	ph7_result_bool(pCtx,0);` |
|         3 | 2871 | `	return PH7_OK;` |
|         1 | 2872 | `}` |
|         - | 2873 |  |
