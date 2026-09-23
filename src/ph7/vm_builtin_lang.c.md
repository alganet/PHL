# src/ph7/vm_builtin_lang.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1170/1367 lines (85.59%)

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
|      258 |   37 | `static int VmClassConstLookup(` |
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
|      263 |   49 | `	*ppClass = 0;` |
|      263 |   50 | `	*ppAttr = 0;` |
|     2299 |   51 | `	for( iSep = 0; iSep + 1 < nLen; iSep++ ){` |
|     2179 |   52 | `		if( zName[iSep] == ':' && zName[iSep+1] == ':' ){` |
|      143 |   53 | `			break;` |
|        - |   54 | `		}` |
|     1023 |   55 | `	}` |
|      263 |   56 | `	if( iSep + 1 >= nLen ){` |
|      125 |   57 | `		return VM_CCONST_PLAIN;` |
|        - |   58 | `	}` |
|      143 |   59 | `	*pSep = iSep;` |
|      143 |   60 | `	pClass = iSep > 0 ? PH7_VmResolveScopeName(&(*pVm),zName,(sxu32)iSep) : 0;` |
|      143 |   61 | `	if( pClass == 0 ){` |
|       32 |   62 | `		if( iSep > 0 && PH7_VmIsScopeKeyword(zName,(sxu32)iSep) ){` |
|        - |   63 | `			/* php separates the two ways a keyword can fail to resolve, so tell them` |
|        - |   64 | ``			 * apart here: `parent` inside a class that simply has no parent is a`` |
|        - |   65 | `			 * different sentence from a keyword named with no class scope at all. */` |
|       16 |   66 | `			if( iSep == 6 && SyMemcmp(zName,"parent",6) == 0` |
|       10 |   67 | `			 && (PH7_VmPeekTopClass(&(*pVm)) \|\| PH7_VmPeekDeclaringClass(&(*pVm))) ){` |
|        3 |   68 | `				return VM_CCONST_NOPARENT;` |
|        - |   69 | `			}` |
|       16 |   70 | `			return VM_CCONST_NOSCOPE;` |
|        - |   71 | `		}` |
|       15 |   72 | `		return VM_CCONST_NOCLASS;` |
|        - |   73 | `	}` |
|      114 |   74 | `	*ppClass = pClass;` |
|      114 |   75 | `	if( iSep + 2 >= nLen ){` |
|        6 |   76 | `		return VM_CCONST_NOCONST; /* "C::" names no constant */` |
|        - |   77 | `	}` |
|        - |   78 | `	/* This form names a class CONSTANT or an enum case (hConst), never a property. */` |
|      110 |   79 | `	pAttr = PH7_ClassExtractConstant(pClass,&zName[iSep+2],(sxu32)(nLen - iSep - 2));` |
|      110 |   80 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       17 |   81 | `		return VM_CCONST_NOCONST;` |
|        - |   82 | `	}` |
|       92 |   83 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|       63 |   84 | `	 && pAttr->pDeclClass && pAttr->pDeclClass != pClass ){` |
|        - |   85 | `		/* php does not put a private constant in a SUBCLASS's table at all, so naming it` |
|        - |   86 | ``		 * through the child is not an access denial but a plain miss — `Sub::P` reports`` |
|        - |   87 | ``		 * `Undefined constant Sub::P` even from inside the declaring class, and`` |
|        - |   88 | `		 * defined("Sub::P") is false there too. Only the declaring class can answer. */` |
|        7 |   89 | `		return VM_CCONST_NOCONST;` |
|        - |   90 | `	}` |
|       90 |   91 | `	*ppAttr = pAttr;` |
|        - |   92 | ``	/* php answers by the CALLING scope, the same rule the direct `C::K` access uses:`` |
|        - |   93 | `	 * a private constant is invisible from outside its declaring class even to a` |
|        - |   94 | `	 * subclass, and a protected one is visible down the hierarchy. */` |
|       90 |   95 | `	if( !PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|       20 |   96 | `		return VM_CCONST_NOACCESS;` |
|        - |   97 | `	}` |
|       72 |   98 | `	return VM_CCONST_OK;` |
|      134 |   99 | `}` |
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
|      108 |  148 | `PH7_PRIVATE int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  149 | `{` |
|        - |  150 | `	ph7_class_attr *pAttr;` |
|        - |  151 | `	ph7_class *pClass;` |
|        - |  152 | `	const char *zName;` |
|      113 |  153 | `	int nLen = 0;` |
|      113 |  154 | `	int iSep = 0;` |
|      113 |  155 | `	int res = 0;` |
|      113 |  156 | `	if( nArg < 1 ){` |
|        - |  157 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  158 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  159 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  160 | `		return SXRET_OK;` |
|        - |  161 | `	}` |
|        - |  162 | `	/* Extract constant name */` |
|      113 |  163 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  164 | `	/* Class-constant form "C::K": every miss — unknown class, unknown constant, or one` |
|        - |  165 | `	 * that is not visible from here — is a plain FALSE, since asking whether a name is` |
|        - |  166 | `	 * defined is exactly what defined() is for (this used to consult the` |
|        - |  167 | `	 * global constant table only, so EVERY class constant answered false while` |
|        - |  168 | `	 * constant() read the same name correctly). */` |
|      113 |  169 | `	if( nLen > 0 ){` |
|      113 |  170 | `		int iRc = VmClassConstLookup(pCtx->pVm,zName,nLen,&pClass,&pAttr,&iSep);` |
|      113 |  171 | `		switch( iRc ){` |
|       19 |  172 | `			case VM_CCONST_PLAIN:` |
|       43 |  173 | `				break;` |
|       18 |  174 | `			case VM_CCONST_OK:` |
|       38 |  175 | `				ph7_result_bool(pCtx,1);` |
|       38 |  176 | `				return SXRET_OK;` |
|        5 |  177 | `			case VM_CCONST_NOSCOPE:` |
|        - |  178 | `			case VM_CCONST_NOPARENT:` |
|        - |  179 | ``				/* php refuses the question rather than answering it: naming `self` where`` |
|        - |  180 | ``				 * no class scope is active is an Error, not a `false`. Every OTHER miss`` |
|        - |  181 | `				 * is a false, so only these two reach the shared thrower. */` |
|       11 |  182 | `				return VmClassConstError(pCtx,iRc,zName,nLen,iSep,pAttr);` |
|       12 |  183 | `			default:` |
|       25 |  184 | `				ph7_result_bool(pCtx,0);` |
|       25 |  185 | `				return SXRET_OK;` |
|        - |  186 | `		}` |
|       19 |  187 | `	}` |
|        - |  188 | `	/* Perform the lookup */` |
|       43 |  189 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  190 | `		/* Already defined */` |
|       37 |  191 | `		res = 1;` |
|       16 |  192 | `	}` |
|       43 |  193 | `	ph7_result_bool(pCtx,res);` |
|       43 |  194 | `	return SXRET_OK;` |
|       59 |  195 | `}` |
|        - |  196 | `/*` |
|        - |  197 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  198 | ` * below.` |
|        - |  199 | ` */` |
|       42 |  200 | `PH7_PRIVATE void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        3 |  201 | `{` |
|       45 |  202 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  203 | `	/* Expand constant value */` |
|       45 |  204 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       45 |  205 | `}` |
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
|       48 |  217 | `PH7_PRIVATE int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 |  218 | `{` |
|        - |  219 | `	const char *zName;  /* Constant name */` |
|        - |  220 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       52 |  221 | `	int nLen = 0;       /* Name length */` |
|        - |  222 | `	sxi32 rc;` |
|       52 |  223 | `	if( nArg < 2 ){` |
|        - |  224 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  225 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  226 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  227 | `		return SXRET_OK;` |
|        - |  228 | `	}` |
|       52 |  229 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  230 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  231 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  232 | `		return SXRET_OK;` |
|        - |  233 | `	}` |
|        - |  234 | `	/* Extract constant name */` |
|       52 |  235 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       52 |  236 | `	if( nLen < 1 ){` |
|      ! 0 |  237 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  238 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  239 | `		return SXRET_OK;` |
|        - |  240 | `	}` |
|        - |  241 | `	/* Duplicate constant value */` |
|       52 |  242 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       52 |  243 | `	if( pValue == 0 ){` |
|      ! 0 |  244 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  245 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  246 | `		return SXRET_OK;` |
|        - |  247 | `	}` |
|        - |  248 | `	/* Initialize the memory object */` |
|       52 |  249 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  250 | `	/* Register the constant */` |
|        - |  251 | `	{` |
|        - |  252 | `		SyString sConsName;` |
|       52 |  253 | `		SyStringInitFromBuf(&sConsName,zName,(sxu32)nLen);` |
|       76 |  254 | `		rc = PH7_VmRegisterConstantEx(pCtx->pVm,&sConsName,VmExpandUserConstant,pValue,` |
|       48 |  255 | `			(SyString *)SySetPeek(&pCtx->pVm->aFiles),0,1);` |
|        - |  256 | `	}` |
|       52 |  257 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  258 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  259 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  260 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  261 | `		return SXRET_OK;` |
|        - |  262 | `	}` |
|        - |  263 | `	/* Duplicate constant value */` |
|       52 |  264 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       52 |  265 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
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
|       52 |  296 | `	ph7_result_bool(pCtx,1);` |
|       52 |  297 | `	return SXRET_OK;` |
|       28 |  298 | `}` |
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
|       16 |  315 | `PH7_PRIVATE int vm_builtin_enum_cases(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  316 | `{` |
|       17 |  317 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  318 | `	ph7_class_attr **apCase;` |
|        - |  319 | `	ph7_class *pClass;` |
|        - |  320 | `	ph7_value *pArray;` |
|        - |  321 | `	sxu32 n;` |
|        - |  322 | `	sxi32 rc;` |
|       17 |  323 | `	if( nArg < 1 \|\| (pClass = VmExtractEnumClass(pVm,apArg[0])) == 0 ){` |
|      ! 0 |  324 | `		ph7_result_null(pCtx);` |
|      ! 0 |  325 | `		return SXRET_OK;` |
|        - |  326 | `	}` |
|       17 |  327 | `	rc = VmEnumMaterialize(pVm,pClass);` |
|       17 |  328 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  329 | `		return rc;` |
|        - |  330 | `	}` |
|       17 |  331 | `	pArray = ph7_context_new_array(pCtx);` |
|       17 |  332 | `	if( pArray == 0 ){` |
|      ! 0 |  333 | `		ph7_result_null(pCtx);` |
|      ! 0 |  334 | `		return SXRET_OK;` |
|        - |  335 | `	}` |
|       17 |  336 | `	apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|       47 |  337 | `	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|       31 |  338 | `		ph7_value *pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,apCase[n]->nIdx);` |
|       31 |  339 | `		if( pSlot ){` |
|       31 |  340 | `			ph7_array_add_elem(pArray,0,pSlot); /* Copies; the object ref is retained */` |
|       15 |  341 | `		}` |
|       16 |  342 | `	}` |
|       17 |  343 | `	ph7_result_value(pCtx,pArray);` |
|       17 |  344 | `	return SXRET_OK;` |
|        9 |  345 | `}` |
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
|       10 |  472 | `PH7_PRIVATE int vm_builtin_enum_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  473 | `{` |
|       12 |  474 | `	ph7_class *pClass = 0;` |
|       12 |  475 | `	if( nArg > 0 ){` |
|       12 |  476 | `		pClass = VmExtractEnumClass(pCtx->pVm,apArg[0]);` |
|        5 |  477 | `	}` |
|       12 |  478 | `	ph7_result_bool(pCtx,pClass != 0);` |
|       12 |  479 | `	return SXRET_OK;` |
|        2 |  480 | `}` |
|      150 |  481 | `PH7_PRIVATE int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 |  482 | `{` |
|        - |  483 | `	SyHashEntry *pEntry;` |
|        - |  484 | `	ph7_constant *pCons;` |
|        - |  485 | `	const char *zName; /* Constant name */` |
|        - |  486 | `	ph7_value sVal;    /* Constant value */` |
|        - |  487 | `	int nLen;` |
|      153 |  488 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  489 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  490 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  491 | `		ph7_result_null(pCtx);` |
|      ! 0 |  492 | `		return SXRET_OK;` |
|        - |  493 | `	}` |
|        - |  494 | `	/* Extract the constant name */` |
|      153 |  495 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
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
|      153 |  506 | `		ph7_class_attr *pAttr = 0;` |
|      153 |  507 | `		ph7_class *pClass = 0;` |
|      153 |  508 | `		int iSep = 0;` |
|      153 |  509 | `		int iRc = VmClassConstLookup(pCtx->pVm,zName,nLen,&pClass,&pAttr,&iSep);` |
|      153 |  510 | `		if( iRc != VM_CCONST_PLAIN ){` |
|       71 |  511 | `			if( iRc != VM_CCONST_OK ){` |
|       54 |  512 | `				return VmClassConstError(pCtx,iRc,zName,nLen,iSep,pAttr);` |
|        - |  513 | `			}` |
|       35 |  514 | `			if( pAttr->nIdx == SXU32_HIGH ){` |
|        - |  515 | `				/* Unmaterialized: enum case → materialize the singletons` |
|        - |  516 | `				 * (all of them: constant("S::A") is a direct access, like` |
|        - |  517 | `				 * OP_MEMBER); plain constant → run its initializer. Unlike` |
|        - |  518 | `				 * defined(), reading the value has to force this. */` |
|        - |  519 | `				sxi32 rcEnum;` |
|       21 |  520 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE ){` |
|        3 |  521 | `					rcEnum = VmEnumMaterialize(pCtx->pVm,pClass);` |
|        2 |  522 | `				}else{` |
|       19 |  523 | `					rcEnum = VmClassConstEvalOnDemand(pCtx->pVm,pClass,pAttr);` |
|        - |  524 | `				}` |
|       21 |  525 | `				if( rcEnum != SXRET_OK ){` |
|        3 |  526 | `					return rcEnum;` |
|        - |  527 | `				}` |
|        8 |  528 | `			}` |
|        - |  529 | `			{` |
|       32 |  530 | `				ph7_value *pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pAttr->nIdx);` |
|       32 |  531 | `				if( pValue ){` |
|       32 |  532 | `					if( SySetUsed(&pAttr->aAttrs) > 0 ){` |
|        - |  533 | `						/* #[\Deprecated] warns through constant() too (php) */` |
|        3 |  534 | `						VmDeprecatedConstNotice(pCtx->pVm,pClass,pAttr);` |
|        1 |  535 | `					}` |
|       32 |  536 | `					ph7_result_value(pCtx,pValue);` |
|       32 |  537 | `					return SXRET_OK;` |
|        - |  538 | `				}` |
|        - |  539 | `			}` |
|        - |  540 | `			/* Declared but with no slot to read: the same dead end the pre-fix code` |
|        - |  541 | `			 * fell through to. */` |
|      ! 0 |  542 | `			return VmClassConstError(pCtx,VM_CCONST_NOCONST,zName,nLen,iSep,pAttr);` |
|        - |  543 | `		}` |
|        - |  544 | `	}` |
|        - |  545 | `	/* Perform the query */` |
|       85 |  546 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       85 |  547 | `	if( pEntry == 0 ){` |
|        - |  548 | `		/* php 8: a catchable Error, not a notice + NULL (band A #4) */` |
|        8 |  549 | `		return PH7_VmThrowException(pCtx,"Error",` |
|        2 |  550 | `			"Undefined constant \"%.*s\"",nLen,zName);` |
|        - |  551 | `	}` |
|       81 |  552 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  553 | `	/* Point to the structure that describe the constant */` |
|       81 |  554 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  555 | `	/* Extract constant value by calling it's associated callback` |
|        - |  556 | `	 * (emits the #[\Deprecated] notice first when attributed, php) */` |
|       81 |  557 | `	VmExpandConstantWithNotice(pCtx->pVm,pCons,&sVal);` |
|        - |  558 | `	/* Return that value */` |
|       81 |  559 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  560 | `	/* Cleanup */` |
|       81 |  561 | `	PH7_MemObjRelease(&sVal);` |
|       81 |  562 | `	return SXRET_OK;` |
|       78 |  563 | `}` |
|        - |  564 | `/*` |
|        - |  565 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  566 | ` * defined below.` |
|        - |  567 | ` */` |
|      972 |  568 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  569 | `{` |
|      973 |  570 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  571 | `	ph7_value sName;` |
|        - |  572 | `	sxi32 rc;` |
|        - |  573 | `	/* Prepare the constant name for insertion */` |
|      973 |  574 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      973 |  575 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  576 | `	/* Perform the insertion */` |
|      973 |  577 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      973 |  578 | `	PH7_MemObjRelease(&sName);` |
|      973 |  579 | `	return rc;` |
|        1 |  580 | `}` |
|        - |  581 | `/*` |
|        - |  582 | ` * array get_defined_constants(void)` |
|        - |  583 | ` *  Returns an associative array with the names of all defined` |
|        - |  584 | ` *  constants.` |
|        - |  585 | ` * Parameters` |
|        - |  586 | ` *  NONE.` |
|        - |  587 | ` * Returns` |
|        - |  588 | ` *  Returns the names of all the constants currently defined.` |
|        - |  589 | ` */` |
|        2 |  590 | `PH7_PRIVATE int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  591 | `{` |
|        - |  592 | `	ph7_value *pArray;` |
|        - |  593 | `	/* Create the array first*/` |
|        3 |  594 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  595 | `	if( pArray == 0 ){` |
|      ! 0 |  596 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  597 | `		SXUNUSED(apArg);` |
|        - |  598 | `		/* Return NULL */` |
|      ! 0 |  599 | `		ph7_result_null(pCtx);` |
|      ! 0 |  600 | `		return SXRET_OK;` |
|        - |  601 | `	}` |
|        - |  602 | `	/* Fill the array with the defined constants */` |
|        3 |  603 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  604 | `	/* Return the created array */` |
|        3 |  605 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  606 | `	return SXRET_OK;` |
|        2 |  607 | `}` |
|        - |  608 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - |  609 | `/*` |
|        - |  610 | ` * Section:` |
|        - |  611 | ` *  Random numbers/string generators.` |
|        - |  612 | ` * Status:` |
|        - |  613 | ` *    Stable.` |
|        - |  614 | ` */` |
|        - |  615 | `/*` |
|        - |  616 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  617 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|        - |  618 | ` * implemented in src/sx/sxrand.c).` |
|        - |  619 | ` */` |
|     4080 |  620 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        5 |  621 | `{` |
|        - |  622 | `	sxu32 iNum;` |
|     4085 |  623 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     4085 |  624 | `	return iNum;` |
|        5 |  625 | `}` |
|        - |  626 | `/*` |
|        - |  627 | ` * The MT19937 generator that backs PHP's rand()/mt_rand() family. It is kept` |
|        - |  628 | ` * separate from the RC4 SyRandomness above so that srand()/mt_srand() give` |
|        - |  629 | ` * userland PHP's reproducible sequence without perturbing the engine's internal` |
|        - |  630 | ` * entropy (object ids, uniqid, quicksort pivots stay on the RC4 generator, as` |
|        - |  631 | ` * they are in PHP too — srand does not touch those).` |
|        - |  632 | ` */` |
|        - |  633 | `/*` |
|        - |  634 | ` * Reset the MT19937 state to a 32-bit seed (PHP truncates its int seed likewise).` |
|        - |  635 | ` */` |
|       36 |  636 | `PH7_PRIVATE void PH7_VmMtSrand(ph7_vm *pVm,sxu32 nSeed)` |
|        1 |  637 | `{` |
|       37 |  638 | `	SyMT19937Seed(&pVm->sMt,nSeed);` |
|       37 |  639 | `	pVm->mtSeeded = TRUE;` |
|       37 |  640 | `}` |
|        - |  641 | `/*` |
|        - |  642 | ` * Draw the next full 32-bit MT19937 word, seeding lazily from the OS CSPRNG on` |
|        - |  643 | ` * first use exactly as PHP auto-seeds when rand()/mt_rand() runs before srand().` |
|        - |  644 | ` */` |
|     1782 |  645 | `PH7_PRIVATE sxu32 PH7_VmMtRand(ph7_vm *pVm)` |
|        1 |  646 | `{` |
|     1783 |  647 | `	if( !pVm->mtSeeded ){` |
|        - |  648 | `		sxu32 nSeed;` |
|        3 |  649 | `		if( SyOSCSPRNG((void *)&nSeed,sizeof(nSeed)) != SXRET_OK ){` |
|        - |  650 | `			/* No OS entropy source: fall back to the RC4 generator's output. */` |
|      ! 0 |  651 | `			nSeed = PH7_VmRandomNum(pVm);` |
|      ! 0 |  652 | `		}` |
|        3 |  653 | `		SyMT19937Seed(&pVm->sMt,nSeed);` |
|        3 |  654 | `		pVm->mtSeeded = TRUE;` |
|        1 |  655 | `	}` |
|     1783 |  656 | `	return SyMT19937Next(&pVm->sMt);` |
|        1 |  657 | `}` |
|        - |  658 | `/*` |
|        - |  659 | ` * Map a full 32-bit draw uniformly into [0,uMax] (uMax is the range width, i.e.` |
|        - |  660 | ` * max-min). Rejection sampling against the largest unbiased ceiling, matching` |
|        - |  661 | ` * PHP's php_random_range32().` |
|        - |  662 | ` */` |
|     1658 |  663 | `static sxu32 VmMtRange32(ph7_vm *pVm,sxu32 uMax)` |
|        1 |  664 | `{` |
|        - |  665 | `	sxu32 result,limit;` |
|     1659 |  666 | `	result = PH7_VmMtRand(pVm);` |
|        - |  667 | `	/* Whole 32-bit domain: no scaling needed. */` |
|     1659 |  668 | `	if( uMax == 0xFFFFFFFFU ){` |
|      ! 0 |  669 | `		return result;` |
|        - |  670 | `	}` |
|        - |  671 | `	/* Make the range inclusive of max. */` |
|     1659 |  672 | `	uMax++;` |
|        - |  673 | `	/* Powers of two are unbiased under a plain mask. */` |
|     1659 |  674 | `	if( (uMax & (uMax - 1)) == 0 ){` |
|        7 |  675 | `		return result & (uMax - 1);` |
|        - |  676 | `	}` |
|        - |  677 | `	/* Ceiling under which 0xFFFFFFFF % uMax == 0; discard draws above it. */` |
|     1653 |  678 | `	limit = 0xFFFFFFFFU - (0xFFFFFFFFU % uMax) - 1;` |
|     1653 |  679 | `	while( result > limit ){` |
|      ! 0 |  680 | `		result = PH7_VmMtRand(pVm);` |
|      ! 0 |  681 | `	}` |
|     1653 |  682 | `	return result % uMax;` |
|      830 |  683 | `}` |
|        - |  684 | `/*` |
|        - |  685 | ` * 64-bit-wide range: assemble two draws (high word first, as PHP does) and` |
|        - |  686 | ` * reject-sample. Matches PHP's php_random_range64().` |
|        - |  687 | ` */` |
|        4 |  688 | `static sxu64 VmMtRange64(ph7_vm *pVm,sxu64 uMax)` |
|        1 |  689 | `{` |
|        - |  690 | `	sxu64 result,limit;` |
|        - |  691 | `	/* First draw fills the low word, second draw the high word — order is` |
|        - |  692 | `	 * significant and matches php's php_random_range64() assembly. */` |
|        5 |  693 | `	result = (sxu64)PH7_VmMtRand(pVm);` |
|        5 |  694 | `	result \|= (sxu64)PH7_VmMtRand(pVm) << 32;` |
|        5 |  695 | `	if( uMax == 0xFFFFFFFFFFFFFFFFULL ){` |
|      ! 0 |  696 | `		return result;` |
|        - |  697 | `	}` |
|        5 |  698 | `	uMax++;` |
|        5 |  699 | `	if( (uMax & (uMax - 1)) == 0 ){` |
|        3 |  700 | `		return result & (uMax - 1);` |
|        - |  701 | `	}` |
|        3 |  702 | `	limit = 0xFFFFFFFFFFFFFFFFULL - (0xFFFFFFFFFFFFFFFFULL % uMax) - 1;` |
|        3 |  703 | `	while( result > limit ){` |
|      ! 0 |  704 | `		result = (sxu64)PH7_VmMtRand(pVm);` |
|      ! 0 |  705 | `		result \|= (sxu64)PH7_VmMtRand(pVm) << 32;` |
|      ! 0 |  706 | `	}` |
|        3 |  707 | `	return result % uMax;` |
|        3 |  708 | `}` |
|        - |  709 | `/*` |
|        - |  710 | ` * Return a value uniformly in the inclusive range [iMin,iMax]. The caller` |
|        - |  711 | ` * guarantees iMin <= iMax. Mirrors PHP's php_mt_rand_range(): a range that fits` |
|        - |  712 | ` * in 32 bits takes the 32-bit path, a wider one the 64-bit path.` |
|        - |  713 | ` */` |
|     1662 |  714 | `PH7_PRIVATE sxi64 PH7_VmMtRandRange(ph7_vm *pVm,sxi64 iMin,sxi64 iMax)` |
|        1 |  715 | `{` |
|     1663 |  716 | `	sxu64 uMax = (sxu64)iMax - (sxu64)iMin;` |
|     1663 |  717 | `	if( uMax > 0xFFFFFFFFULL ){` |
|        5 |  718 | `		return (sxi64)(VmMtRange64(pVm,uMax) + (sxu64)iMin);` |
|        - |  719 | `	}` |
|     1659 |  720 | `	return (sxi64)((sxu64)VmMtRange32(pVm,(sxu32)uMax) + (sxu64)iMin);` |
|      832 |  721 | `}` |
|        - |  722 | `/*` |
|        - |  723 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  724 | ` * Note that the generated string is NOT null terminated.` |
|        - |  725 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|        - |  726 | ` * implemented in src/sx/sxrand.c).` |
|        - |  727 | ` */` |
|  3665094 |  728 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        5 |  729 | `{` |
|        - |  730 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  731 | `	int i;` |
|        - |  732 | `	/* Generate a binary string first */` |
|  3665099 |  733 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  734 | `	/* Turn the binary string into english based alphabet */` |
| 40316283 |  735 | `	for( i = 0 ; i < nLen ; ++i ){` |
| 36651189 |  736 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
| 18325597 |  737 | `	 }` |
|  3665099 |  738 | `}` |
|        - |  739 | `/*` |
|        - |  740 | ` * int rand()` |
|        - |  741 | ` * int mt_rand()` |
|        - |  742 | ` * int rand(int $min,int $max)` |
|        - |  743 | ` * int mt_rand(int $min,int $max)` |
|        - |  744 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  745 | ` * Parameter` |
|        - |  746 | ` *  $min` |
|        - |  747 | ` *    The lowest value to return (default: 0)` |
|        - |  748 | ` *  $max` |
|        - |  749 | ` *   The highest value to return (default: getrandmax())` |
|        - |  750 | ` * Return` |
|        - |  751 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  752 | ` * Note:` |
|        - |  753 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  754 | ` *  by te SQLite3 library.` |
|        - |  755 | ` */` |
|     1728 |  756 | `PH7_PRIVATE int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  757 | `{` |
|     1729 |  758 | `	SyString *pName = &pCtx->pFunc->sName;` |
|     3019 |  759 | `	int bMt = (pName->nByte == sizeof("mt_rand")-1` |
|     1728 |  760 | `		&& SyMemcmp(pName->zString,"mt_rand",sizeof("mt_rand")-1) == 0);` |
|        - |  761 | `	/* php accepts exactly 0 or exactly 2 arguments (min,max); 1 or 3+ is an` |
|        - |  762 | `	 * ArgumentCountError. The central arity table can't express "0 or 2", so` |
|        - |  763 | `	 * it is enforced here (was a silent wrong result for the raw draw). */` |
|     1729 |  764 | `	if( nArg == 1 \|\| nArg > 2 ){` |
|       13 |  765 | `		return PH7_VmThrowException(pCtx,` |
|        - |  766 | `			"ArgumentCountError",` |
|        - |  767 | `			"%z() expects exactly 2 arguments, %d given",` |
|        4 |  768 | `			pName, nArg` |
|        - |  769 | `			);` |
|        - |  770 | `	}` |
|     1721 |  771 | `	if( nArg == 2 ){` |
|        - |  772 | `		sxi64 iMin,iMax;` |
|        - |  773 | `		/* Signed 64-bit endpoints: the old unsigned math wrapped negative` |
|        - |  774 | `		 * ranges to huge positives (rand(-10,-1) -> ~4e9) and mis-handled` |
|        - |  775 | `		 * min==max. */` |
|     1659 |  776 | `		iMin = ph7_value_to_int64(apArg[0]);` |
|     1659 |  777 | `		iMax = ph7_value_to_int64(apArg[1]);` |
|     1659 |  778 | `		if( iMin > iMax ){` |
|        9 |  779 | `			if( bMt ){` |
|        - |  780 | `				/* mt_rand() is strict: php throws a catchable ValueError. */` |
|        5 |  781 | `				return PH7_VmThrowException(pCtx,` |
|        - |  782 | `					"ValueError",` |
|        - |  783 | `					"mt_rand(): Argument #2 ($max) must be greater than or equal to argument #1 ($min)"` |
|        - |  784 | `					);` |
|        - |  785 | `			}` |
|        - |  786 | `			/* rand() swaps the bounds for backward compatibility (php keeps` |
|        - |  787 | `			 * this quirk; only mt_rand() rejects a reversed range). */` |
|        5 |  788 | `			{ sxi64 iTmp = iMin; iMin = iMax; iMax = iTmp; }` |
|        2 |  789 | `		}` |
|        - |  790 | `		/* MT19937-backed uniform draw over [iMin,iMax], bit-for-bit as php. */` |
|     1655 |  791 | `		ph7_result_int64(pCtx,PH7_VmMtRandRange(pCtx->pVm,iMin,iMax));` |
|     1655 |  792 | `		return SXRET_OK;` |
|        - |  793 | `	}` |
|        - |  794 | `	/* No-argument form: a 31-bit value in [0, mt_getrandmax()]. php returns` |
|        - |  795 | `	 * php_mt_rand() >> 1 for the bare draw (the full 32-bit word feeds the` |
|        - |  796 | `	 * range form above, but the bare form drops the low bit). */` |
|       63 |  797 | `	ph7_result_int64(pCtx,(sxi64)(PH7_VmMtRand(pCtx->pVm) >> 1));` |
|       63 |  798 | `	return SXRET_OK;` |
|      865 |  799 | `}` |
|        - |  800 | `/*` |
|        - |  801 | ` * int getrandmax(void)` |
|        - |  802 | ` * int mt_getrandmax(void)` |
|        - |  803 | ` * int rc4_getrandmax(void)` |
|        - |  804 | ` *   Show largest possible random value` |
|        - |  805 | ` * Return` |
|        - |  806 | ` *  The largest possible random value returned by rand()/mt_rand(): php's` |
|        - |  807 | ` *  MT19937 backing makes this 2^31-1 (2147483647) for both.` |
|        - |  808 | ` */` |
|        8 |  809 | `PH7_PRIVATE int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  810 | `{` |
|        4 |  811 | `	SXUNUSED(nArg); /* cc warning */` |
|        4 |  812 | `	SXUNUSED(apArg);` |
|        - |  813 | `	/* php: PHP_MT_RAND_MAX == (1<<31)-1; bare rand()/mt_rand() draw >> 1 lands` |
|        - |  814 | `	 * exactly in [0, this]. */` |
|        9 |  815 | `	ph7_result_int64(pCtx,2147483647);` |
|        9 |  816 | `	return SXRET_OK;` |
|        1 |  817 | `}` |
|        - |  818 | `/*` |
|        - |  819 | ` * string rand_str()` |
|        - |  820 | ` * string rand_str(int $len)` |
|        - |  821 | ` *  Generate a random string (English alphabet).` |
|        - |  822 | ` * Parameter` |
|        - |  823 | ` *  $len` |
|        - |  824 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  825 | ` * Return` |
|        - |  826 | ` *   A pseudo random string.` |
|        - |  827 | ` * Note:` |
|        - |  828 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  829 | ` *  by te SQLite3 library.` |
|        - |  830 | ` *  This function is a symisc extension.` |
|        - |  831 | ` */` |
|      158 |  832 | `PH7_PRIVATE int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  833 | `{` |
|        - |  834 | `	char zString[1024];` |
|      160 |  835 | `	int iLen = 0x10;` |
|      160 |  836 | `	if( nArg > 0 ){` |
|        - |  837 | `		/* Get the desired length */` |
|      160 |  838 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      160 |  839 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  840 | `			/* Default length */` |
|        3 |  841 | `			iLen = 0x10;` |
|        1 |  842 | `		}` |
|       79 |  843 | `	}` |
|        - |  844 | `	/* Generate the random string */` |
|      160 |  845 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  846 | `	/* Return the generated string */` |
|      160 |  847 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      160 |  848 | `	return SXRET_OK;` |
|        2 |  849 | `}` |
|        - |  850 | `/*` |
|        - |  851 | ` * Reject non-numeric values (array/object/resource and non-numeric strings)` |
|        - |  852 | ` * the same way intdiv() does. Returns SXRET_OK if the value is acceptable as` |
|        - |  853 | ` * an int (PHP coerces float and numeric string silently).` |
|        - |  854 | ` */` |
|      476 |  855 | `static int VmRandomCheckIntArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgPos,const char *zParamName)` |
|        1 |  856 | `{` |
|      476 |  857 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg)` |
|      477 |  858 | `		\|\| ph7_value_is_resource(pArg) ){` |
|      ! 0 |  859 | `		return PH7_VmThrowException(pCtx,` |
|        - |  860 | `			"TypeError",` |
|        - |  861 | `			"%s(): Argument #%d (%s) must be of type int, %s given",` |
|      ! 0 |  862 | `			zFunc,iArgPos,zParamName,` |
|      ! 0 |  863 | `			ph7_type_name(pArg)` |
|        - |  864 | `			);` |
|        - |  865 | `	}` |
|      477 |  866 | `	if( ph7_value_is_string(pArg) ){` |
|        - |  867 | `		int len;` |
|        5 |  868 | `		const char *zStr = ph7_value_to_string(pArg, &len);` |
|        5 |  869 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|      ! 0 |  870 | `			return PH7_VmThrowException(pCtx,` |
|        - |  871 | `				"TypeError",` |
|        - |  872 | `				"%s(): Argument #%d (%s) must be of type int, string given",` |
|      ! 0 |  873 | `				zFunc,iArgPos,zParamName` |
|        - |  874 | `				);` |
|        - |  875 | `		}` |
|        2 |  876 | `	}` |
|      477 |  877 | `	return SXRET_OK;` |
|      239 |  878 | `}` |
|        - |  879 | `/*` |
|        - |  880 | ` * int random_int(int $min, int $max)` |
|        - |  881 | ` *  Generate a cryptographically secure pseudo-random integer in [$min, $max].` |
|        - |  882 | ` *  Mirrors PHP 7.0+ random_int(). Uses the OS CSPRNG via SyOSCSPRNG().` |
|        - |  883 | ` *  Distribution is uniform via rejection sampling against the smallest` |
|        - |  884 | ` *  power-of-two mask covering the range.` |
|        - |  885 | ` */` |
|      230 |  886 | `PH7_PRIVATE int vm_builtin_random_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  887 | `{` |
|        - |  888 | `	sxi64 iMin,iMax;` |
|        - |  889 | `	sxu64 uRange,uMask,uResult;` |
|        - |  890 | `	unsigned int nAttempt;` |
|        - |  891 | `	int rc;` |
|      231 |  892 | `	if( nArg != 2 ){` |
|      ! 0 |  893 | `		return PH7_VmThrowException(pCtx,` |
|        - |  894 | `			"ArgumentCountError",` |
|        - |  895 | `			"random_int() expects exactly 2 arguments, %d given",` |
|      ! 0 |  896 | `			nArg` |
|        - |  897 | `			);` |
|        - |  898 | `	}` |
|      231 |  899 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_int",1,"$min");` |
|      231 |  900 | `	if( rc != SXRET_OK ){ return rc; }` |
|      231 |  901 | `	rc = VmRandomCheckIntArg(pCtx,apArg[1],"random_int",2,"$max");` |
|      231 |  902 | `	if( rc != SXRET_OK ){ return rc; }` |
|      231 |  903 | `	iMin = ph7_value_to_int64(apArg[0]);` |
|      231 |  904 | `	iMax = ph7_value_to_int64(apArg[1]);` |
|      231 |  905 | `	if( iMin > iMax ){` |
|        3 |  906 | `		return PH7_VmThrowException(pCtx,` |
|        - |  907 | `			"ValueError",` |
|        - |  908 | `			"random_int(): Argument #1 ($min) must be less than or equal to argument #2 ($max)"` |
|        - |  909 | `			);` |
|        - |  910 | `	}` |
|      229 |  911 | `	if( iMin == iMax ){` |
|        5 |  912 | `		ph7_result_int64(pCtx,iMin);` |
|        5 |  913 | `		return SXRET_OK;` |
|        - |  914 | `	}` |
|      225 |  915 | `	uRange = (sxu64)iMax - (sxu64)iMin;` |
|      225 |  916 | `	uMask = uRange;` |
|      225 |  917 | `	uMask \|= uMask >> 1;` |
|      225 |  918 | `	uMask \|= uMask >> 2;` |
|      225 |  919 | `	uMask \|= uMask >> 4;` |
|      225 |  920 | `	uMask \|= uMask >> 8;` |
|      225 |  921 | `	uMask \|= uMask >> 16;` |
|      225 |  922 | `	uMask \|= uMask >> 32;` |
|      225 |  923 | `	uResult = 0;` |
|      348 |  924 | `	for( nAttempt = 0 ; nAttempt < 50 ; ++nAttempt ){` |
|        - |  925 | `		/* Always draw a full 8 bytes so endianness of the cast doesn't matter` |
|        - |  926 | `		 * (a 4-byte fill into a sxu64 would land in the high half on big-endian` |
|        - |  927 | `		 * and the low-half mask would always read 0). */` |
|        - |  928 | `		sxu64 uDraw;` |
|      348 |  929 | `		if( SyOSCSPRNG(&uDraw,sizeof(uDraw)) != SXRET_OK ){` |
|      ! 0 |  930 | `			return PH7_VmThrowException(pCtx,` |
|        - |  931 | `				"Random\\RandomException",` |
|        - |  932 | `				"Cannot gather sufficient random data"` |
|        - |  933 | `				);` |
|        - |  934 | `		}` |
|      348 |  935 | `		uDraw &= uMask;` |
|      348 |  936 | `		if( uDraw <= uRange ){` |
|      225 |  937 | `			uResult = uDraw;` |
|      225 |  938 | `			break;` |
|        - |  939 | `		}` |
|       61 |  940 | `	}` |
|      225 |  941 | `	if( nAttempt >= 50 ){` |
|      ! 0 |  942 | `		return PH7_VmThrowException(pCtx,` |
|        - |  943 | `			"Random\\RandomException",` |
|        - |  944 | `			"Cannot gather sufficient random data"` |
|        - |  945 | `			);` |
|        - |  946 | `	}` |
|      225 |  947 | `	ph7_result_int64(pCtx,(sxi64)((sxu64)iMin + uResult));` |
|      225 |  948 | `	return SXRET_OK;` |
|      116 |  949 | `}` |
|        - |  950 | `/*` |
|        - |  951 | ` * string random_bytes(int $length)` |
|        - |  952 | ` *  Generate $length cryptographically secure random bytes via SyOSCSPRNG().` |
|        - |  953 | ` *  Mirrors PHP 7.0+ random_bytes().` |
|        - |  954 | ` */` |
|       16 |  955 | `PH7_PRIVATE int vm_builtin_random_bytes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  956 | `{` |
|        - |  957 | `	sxi64 iLen;` |
|        - |  958 | `	unsigned char zStack[256];` |
|        - |  959 | `	void *pBuf;` |
|        - |  960 | `	int rc;` |
|       17 |  961 | `	int bHeap = 0;` |
|       17 |  962 | `	if( nArg != 1 ){` |
|      ! 0 |  963 | `		return PH7_VmThrowException(pCtx,` |
|        - |  964 | `			"ArgumentCountError",` |
|        - |  965 | `			"random_bytes() expects exactly 1 argument, %d given",` |
|      ! 0 |  966 | `			nArg` |
|        - |  967 | `			);` |
|        - |  968 | `	}` |
|       17 |  969 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_bytes",1,"$length");` |
|       17 |  970 | `	if( rc != SXRET_OK ){ return rc; }` |
|       17 |  971 | `	iLen = ph7_value_to_int64(apArg[0]);` |
|       17 |  972 | `	if( iLen < 1 ){` |
|        5 |  973 | `		return PH7_VmThrowException(pCtx,` |
|        - |  974 | `			"ValueError",` |
|        - |  975 | `			"random_bytes(): Argument #1 ($length) must be greater than 0"` |
|        - |  976 | `			);` |
|        - |  977 | `	}` |
|        - |  978 | `	/* The PH7 allocator and ph7_result_string both take sxu32/int sizes,` |
|        - |  979 | `	 * so we can't honor a length above 2 GiB. Reject early rather than` |
|        - |  980 | `	 * silently truncating via the (sxu32) cast below. */` |
|       13 |  981 | `	if( iLen > 0x7FFFFFFF ){` |
|      ! 0 |  982 | `		return PH7_VmThrowException(pCtx,` |
|        - |  983 | `			"ValueError",` |
|        - |  984 | `			"random_bytes(): Argument #1 ($length) is too large"` |
|        - |  985 | `			);` |
|        - |  986 | `	}` |
|       13 |  987 | `	if( iLen <= (sxi64)sizeof(zStack) ){` |
|       13 |  988 | `		pBuf = zStack;` |
|        7 |  989 | `	}else{` |
|      ! 0 |  990 | `		pBuf = SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)iLen);` |
|      ! 0 |  991 | `		if( pBuf == 0 ){` |
|      ! 0 |  992 | `			return PH7_VmThrowException(pCtx,` |
|        - |  993 | `				"Exception",` |
|        - |  994 | `				"random_bytes(): Failed to allocate %qd bytes",` |
|      ! 0 |  995 | `				iLen` |
|        - |  996 | `				);` |
|        - |  997 | `		}` |
|      ! 0 |  998 | `		bHeap = 1;` |
|        - |  999 | `	}` |
|       13 | 1000 | `	if( SyOSCSPRNG(pBuf,(sxu32)iLen) != SXRET_OK ){` |
|      ! 0 | 1001 | `		if( bHeap ){` |
|      ! 0 | 1002 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 1003 | `		}` |
|      ! 0 | 1004 | `		return PH7_VmThrowException(pCtx,` |
|        - | 1005 | `			"Random\\RandomException",` |
|        - | 1006 | `			"Cannot gather sufficient random data"` |
|        - | 1007 | `			);` |
|        - | 1008 | `	}` |
|       13 | 1009 | `	ph7_result_string(pCtx,(const char *)pBuf,(int)iLen);` |
|       13 | 1010 | `	if( bHeap ){` |
|      ! 0 | 1011 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 1012 | `	}` |
|       13 | 1013 | `	return SXRET_OK;` |
|        9 | 1014 | `}` |
|        - | 1015 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 1016 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 1017 | `/* Unique ID private data */` |
|        - | 1018 | `struct unique_id_data` |
|        - | 1019 | `{` |
|        - | 1020 | `	ph7_context *pCtx; /* Call context */` |
|        - | 1021 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - | 1022 | `};` |
|        - | 1023 | `/*` |
|        - | 1024 | ` * Binary to hex consumer callback.` |
|        - | 1025 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - | 1026 | ` * defined below.` |
|        - | 1027 | ` */` |
|      192 | 1028 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 | 1029 | `{` |
|      193 | 1030 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - | 1031 | `	sxu32 nBuflen;` |
|        - | 1032 | `	/* Extract result buffer length */` |
|      193 | 1033 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 | 1034 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - | 1035 | `			/*` |
|        - | 1036 | `			 * If the more_entropy flag is not set,then the returned` |
|        - | 1037 | `			 * string will be 13 characters long` |
|        - | 1038 | `			 */` |
|       25 | 1039 | `		return SXERR_ABORT;` |
|        - | 1040 | `	}` |
|      169 | 1041 | `	if( nBuflen > 22 ){` |
|      ! 0 | 1042 | `		return SXERR_ABORT;` |
|        - | 1043 | `	}` |
|        - | 1044 | `	/* Safely Consume the hex stream */` |
|      169 | 1045 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 | 1046 | `	return SXRET_OK;` |
|       97 | 1047 | `}` |
|        - | 1048 | `/*` |
|        - | 1049 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - | 1050 | ` *  Generate a unique ID` |
|        - | 1051 | ` * Parameter` |
|        - | 1052 | ` * $prefix` |
|        - | 1053 | ` *  Append this prefix to the generated unique ID.` |
|        - | 1054 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - | 1055 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - | 1056 | ` * $more_entropy` |
|        - | 1057 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - | 1058 | ` *  that the result will be unique.` |
|        - | 1059 | ` * Return` |
|        - | 1060 | ` *  Returns the unique identifier, as a string.` |
|        - | 1061 | ` */` |
|       24 | 1062 | `PH7_PRIVATE int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1063 | `{` |
|        - | 1064 | `	struct unique_id_data sUniq;` |
|        - | 1065 | `	unsigned char zDigest[20];` |
|       25 | 1066 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 1067 | `	const char *zPrefix;` |
|        - | 1068 | `	SHA1Context sCtx;` |
|        - | 1069 | `	char zRandom[7];` |
|        - | 1070 | `	int nPrefix;` |
|        - | 1071 | `	int entropy;` |
|        - | 1072 | `	/* Generate a random string first */` |
|       25 | 1073 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - | 1074 | `	/* Initialize fields */` |
|       25 | 1075 | `	zPrefix = 0;` |
|       25 | 1076 | `	nPrefix = 0;` |
|       25 | 1077 | `	entropy = 0;` |
|       25 | 1078 | `	if( nArg > 0 ){` |
|        - | 1079 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 | 1080 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 | 1081 | `		if( nArg > 1 ){` |
|      ! 0 | 1082 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 1083 | `		}` |
|      ! 0 | 1084 | `	}` |
|       25 | 1085 | `	SHA1Init(&sCtx);` |
|        - | 1086 | `	/* Generate the random ID */` |
|       25 | 1087 | `	if( nPrefix > 0 ){` |
|      ! 0 | 1088 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 | 1089 | `	}` |
|        - | 1090 | `	/* Append the random ID */` |
|       25 | 1091 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - | 1092 | `	/* Append the random string */` |
|       25 | 1093 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - | 1094 | `	/* Increment the number */` |
|       25 | 1095 | `	pVm->unique_id++;` |
|       25 | 1096 | `	SHA1Final(&sCtx,zDigest);` |
|        - | 1097 | `	/* Hexify the digest */` |
|       25 | 1098 | `	sUniq.pCtx = pCtx;` |
|       25 | 1099 | `	sUniq.entropy = entropy;` |
|       25 | 1100 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - | 1101 | `	/* All done */` |
|       25 | 1102 | `	return PH7_OK;` |
|        1 | 1103 | `}` |
|        - | 1104 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 1105 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 1106 | `/*` |
|        - | 1107 | ` * Section:` |
|        - | 1108 | ` *  Language construct implementation as foreign functions.` |
|        - | 1109 | ` * Status:` |
|        - | 1110 | ` *    Stable.` |
|        - | 1111 | ` */` |
|        - | 1112 | `/*` |
|        - | 1113 | ` * The user-visible string coercion an OUTPUT construct performs on one of its` |
|        - | 1114 | ` * arguments (echo/print reached as host functions rather than as OP_CONSUME).` |
|        - | 1115 | ` * An ARRAY warns and still renders as "Array"; an object whose class has no` |
|        - | 1116 | ` * __toString() is php's catchable "could not be converted to string" Error,` |
|        - | 1117 | ` * and the construct outputs nothing for it. The status is recorded on the` |
|        - | 1118 | ` * context too, so OP_CALL cannot treat the throwing call as a normal return.` |
|        - | 1119 | ` */` |
|       40 | 1120 | `static sxi32 VmOutputArgToString(ph7_context *pCtx,ph7_value *pArg,const char **pzData,int *pnLen)` |
|        5 | 1121 | `{` |
|       45 | 1122 | `	sxi32 rc = PH7_MemObjToStringUV(pArg);` |
|       45 | 1123 | `	if( rc != SXRET_OK ){` |
|        3 | 1124 | `		pCtx->nThrowRc = rc;` |
|        3 | 1125 | `		return rc;` |
|        - | 1126 | `	}` |
|       42 | 1127 | `	*pzData = ph7_value_to_string(pArg,pnLen);` |
|       42 | 1128 | `	return SXRET_OK;` |
|       25 | 1129 | `}` |
|        - | 1130 | `/*` |
|        - | 1131 | ` * void echo($string...)` |
|        - | 1132 | ` *  Output one or more messages.` |
|        - | 1133 | ` * Parameters` |
|        - | 1134 | ` *  $string` |
|        - | 1135 | ` *   Message to output.` |
|        - | 1136 | ` * Return` |
|        - | 1137 | ` *  NULL.` |
|        - | 1138 | ` */` |
|      ! 0 | 1139 | `PH7_PRIVATE int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 1140 | `{` |
|        - | 1141 | `	const char *zData;` |
|      ! 0 | 1142 | `	int nDataLen = 0;` |
|        - | 1143 | `	ph7_vm *pVm;` |
|        - | 1144 | `	int i,rc;` |
|        - | 1145 | `	/* Point to the target VM */` |
|      ! 0 | 1146 | `	pVm = pCtx->pVm;` |
|        - | 1147 | `	/* Output */` |
|      ! 0 | 1148 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 | 1149 | `		sxi32 rcSv = VmOutputArgToString(pCtx,apArg[i],&zData,&nDataLen);` |
|      ! 0 | 1150 | `		if( rcSv != SXRET_OK ){` |
|      ! 0 | 1151 | `			return rcSv;` |
|        - | 1152 | `		}` |
|      ! 0 | 1153 | `		if( nDataLen > 0 ){` |
|      ! 0 | 1154 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 | 1155 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 | 1156 | `			if( rc == SXERR_ABORT ){` |
|        - | 1157 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 1158 | `				return PH7_ABORT;` |
|        - | 1159 | `			}` |
|      ! 0 | 1160 | `		}` |
|      ! 0 | 1161 | `	}` |
|      ! 0 | 1162 | `	return SXRET_OK;` |
|      ! 0 | 1163 | `}` |
|        - | 1164 | `/*` |
|        - | 1165 | ` * int print($string...)` |
|        - | 1166 | ` *  Output one or more messages.` |
|        - | 1167 | ` * Parameters` |
|        - | 1168 | ` *  $string` |
|        - | 1169 | ` *   Message to output.` |
|        - | 1170 | ` * Return` |
|        - | 1171 | ` *  1 always.` |
|        - | 1172 | ` */` |
|       40 | 1173 | `PH7_PRIVATE int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 1174 | `{` |
|        - | 1175 | `	const char *zData;` |
|       45 | 1176 | `	int nDataLen = 0;` |
|        - | 1177 | `	ph7_vm *pVm;` |
|        - | 1178 | `	int i,rc;` |
|        - | 1179 | `	/* Point to the target VM */` |
|       45 | 1180 | `	pVm = pCtx->pVm;` |
|        - | 1181 | `	/* Output */` |
|       83 | 1182 | `	for( i = 0 ; i < nArg ; ++i ){` |
|       45 | 1183 | `		sxi32 rcSv = VmOutputArgToString(pCtx,apArg[i],&zData,&nDataLen);` |
|       45 | 1184 | `		if( rcSv != SXRET_OK ){` |
|        3 | 1185 | `			return rcSv;` |
|        - | 1186 | `		}` |
|       42 | 1187 | `		if( nDataLen > 0 ){` |
|       42 | 1188 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|       42 | 1189 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|       42 | 1190 | `			if( rc == SXERR_ABORT ){` |
|        - | 1191 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 1192 | `				return PH7_ABORT;` |
|        - | 1193 | `			}` |
|       19 | 1194 | `		}` |
|       23 | 1195 | `	}` |
|        - | 1196 | `	/* Return 1 */` |
|       42 | 1197 | `	ph7_result_int(pCtx,1);` |
|       42 | 1198 | `	return SXRET_OK;` |
|       25 | 1199 | `}` |
|        - | 1200 | `/*` |
|        - | 1201 | ` * void exit(string $msg)` |
|        - | 1202 | ` * void exit(int $status)` |
|        - | 1203 | ` * void die(string $ms)` |
|        - | 1204 | ` * void die(int $status)` |
|        - | 1205 | ` *   Output a message and terminate program execution.` |
|        - | 1206 | ` * Parameter` |
|        - | 1207 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - | 1208 | ` *  If status is an integer, that value will be used as the exit status` |
|        - | 1209 | ` *  and not printed` |
|        - | 1210 | ` * Return` |
|        - | 1211 | ` *  NULL` |
|        - | 1212 | ` */` |
|      ! 0 | 1213 | `PH7_PRIVATE int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 1214 | `{` |
|      ! 0 | 1215 | `	if( nArg > 0 ){` |
|      ! 0 | 1216 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - | 1217 | `			const char *zData;` |
|      ! 0 | 1218 | `			int iLen = 0;` |
|        - | 1219 | `			/* Print exit message */` |
|      ! 0 | 1220 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 | 1221 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 | 1222 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - | 1223 | `			sxi32 iExitStatus;` |
|        - | 1224 | `			/* Record exit status code */` |
|      ! 0 | 1225 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 | 1226 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 | 1227 | `		}` |
|      ! 0 | 1228 | `	}` |
|        - | 1229 | `	/* Request a VM-wide halt (see PH7_OP_HALT) and abort processing` |
|        - | 1230 | `	 * immediately; the abort unwinds enclosing frames and execution units.` |
|        - | 1231 | `	 */` |
|      ! 0 | 1232 | `	pCtx->pVm->bHaltRequested = 1;` |
|      ! 0 | 1233 | `	return PH7_ABORT;` |
|      ! 0 | 1234 | `}` |
|        - | 1235 | `/*` |
|        - | 1236 | ` * Section:` |
|        - | 1237 | ` *  Version,Credits and Copyright related functions.` |
|        - | 1238 | ` * Status:` |
|        - | 1239 | ` *    Stable.` |
|        - | 1240 | ` */` |
|        - | 1241 | `/*` |
|        - | 1242 | ` * string ph7version(void)` |
|        - | 1243 | ` *  Returns the running version of the PH7 version.` |
|        - | 1244 | ` * Parameters` |
|        - | 1245 | ` *  None` |
|        - | 1246 | ` * Return` |
|        - | 1247 | ` * Current PH7 version.` |
|        - | 1248 | ` */` |
|        2 | 1249 | `PH7_PRIVATE int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1250 | `{` |
|        1 | 1251 | `	SXUNUSED(nArg);` |
|        1 | 1252 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 1253 | `	/* Current engine version */` |
|        3 | 1254 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 1255 | `	return PH7_OK;` |
|        1 | 1256 | `}` |
|        - | 1257 | `/*` |
|        - | 1258 | ` * string phpversion([ string $extension ])` |
|        - | 1259 | ` *  Returns the PHP-compatibility version PHL advertises (see PHP_COMPAT_VERSION).` |
|        - | 1260 | ` * Parameters` |
|        - | 1261 | ` *  $extension (optional): an extension name. PHL has no extension registry, so any` |
|        - | 1262 | ` *  argument yields NULL (PHP returns FALSE for an unknown extension).` |
|        - | 1263 | ` * Return` |
|        - | 1264 | ` *  The PHP-compat version string, or NULL when called with an extension argument.` |
|        - | 1265 | ` */` |
|        4 | 1266 | `PH7_PRIVATE int vm_builtin_phpversion(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1267 | `{` |
|        2 | 1268 | `	SXUNUSED(apArg); /* cc warning */` |
|        5 | 1269 | `	if( nArg > 0 ){` |
|      ! 0 | 1270 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1271 | `		return PH7_OK;` |
|        - | 1272 | `	}` |
|        5 | 1273 | `	ph7_result_string(pCtx,PHP_COMPAT_VERSION,(int)sizeof(PHP_COMPAT_VERSION) - 1);` |
|        5 | 1274 | `	return PH7_OK;` |
|        3 | 1275 | `}` |
|        - | 1276 | `/*` |
|        - | 1277 | ` * The extensions PHL reports as loaded, in the order get_loaded_extensions()` |
|        - | 1278 | `` * lists them and with the CASE php uses for each. `extension_loaded()` matches`` |
|        - | 1279 | ` * case-INSENSITIVELY, which is why one table serves both.` |
|        - | 1280 | ` */` |
|        - | 1281 | `static const char * const azExtension[] = {` |
|        - | 1282 | `	"Core", "date", "pcre", "SPL", "json", "standard",` |
|        - | 1283 | `	"ctype", "filter", "hash", "Reflection", "session", "mbstring"` |
|        - | 1284 | `#ifdef PH7_ENABLE_LIBXML` |
|        - | 1285 | `	, "libxml", "dom", "xmlwriter"` |
|        - | 1286 | `#endif` |
|        - | 1287 | `};` |
|        - | 1288 | `/*` |
|        - | 1289 | `` * `phl.stub_extensions` is a PHL-only directive: a comma-separated list of`` |
|        - | 1290 | ` * extensions PHL does NOT implement but reports as LOADED, so software that` |
|        - | 1291 | ` * only GATES on extension_loaded() (PHPUnit's dom/xmlwriter check) runs` |
|        - | 1292 | ` * unmodified. It synthesizes nothing -- no class, no function.` |
|        - | 1293 | ` *` |
|        - | 1294 | ` * Walk it, handing each trimmed name to xVisit until one answers non-zero.` |
|        - | 1295 | ` */` |
|       16 | 1296 | `static int VmStubExtWalk(ph7_vm *pVm,int (*xVisit)(const char *,int,void *),void *pData)` |
|        2 | 1297 | `{` |
|        - | 1298 | `	SyBlob sList;` |
|        - | 1299 | `	const char *z;` |
|       18 | 1300 | `	int nByte,i = 0,rc = 0;` |
|       18 | 1301 | `	SyBlobInit(&sList,&pVm->sAllocator);` |
|       18 | 1302 | `	PH7_VmIniGetStr(pVm,"phl.stub_extensions",&sList);` |
|       18 | 1303 | `	z = (const char *)SyBlobData(&sList);` |
|       18 | 1304 | `	nByte = (int)SyBlobLength(&sList);` |
|       36 | 1305 | `	while( rc == 0 && i < nByte ){` |
|        - | 1306 | `		int iStart,iEnd;` |
|       27 | 1307 | `		while( i < nByte && z[i] == ',' ){ i++; }` |
|       19 | 1308 | `		iStart = i;` |
|      223 | 1309 | `		while( i < nByte && z[i] != ',' ){ i++; }` |
|       19 | 1310 | `		iEnd = i;` |
|       36 | 1311 | `		while( iStart < iEnd && (z[iStart] == ' ' \|\| z[iStart] == '\t') ){ iStart++; }` |
|       28 | 1312 | `		while( iEnd > iStart && (z[iEnd-1] == ' ' \|\| z[iEnd-1] == '\t') ){ iEnd--; }` |
|       19 | 1313 | `		if( iEnd > iStart ){` |
|       19 | 1314 | `			rc = xVisit(&z[iStart],iEnd - iStart,pData);` |
|        9 | 1315 | `		}` |
|        1 | 1316 | `	}` |
|       18 | 1317 | `	SyBlobRelease(&sList);` |
|       18 | 1318 | `	return rc;` |
|        2 | 1319 | `}` |
|        - | 1320 | `typedef struct vm_ext_match vm_ext_match;` |
|        - | 1321 | `struct vm_ext_match {` |
|        - | 1322 | `	const char *zName;` |
|        - | 1323 | `	int nName;` |
|        - | 1324 | `};` |
|       14 | 1325 | `static int VmStubExtMatch(const char *zName,int nName,void *pData)` |
|        1 | 1326 | `{` |
|       15 | 1327 | `	vm_ext_match *p = (vm_ext_match *)pData;` |
|       15 | 1328 | `	return nName == p->nName && SyStrnicmp(zName,p->zName,(sxu32)nName) == 0;` |
|        1 | 1329 | `}` |
|        - | 1330 | `/*` |
|        - | 1331 | ` * bool extension_loaded(string $extension)` |
|        - | 1332 | ` *  php matches the name case-insensitively.` |
|        - | 1333 | ` */` |
|       94 | 1334 | `PH7_PRIVATE int vm_builtin_extension_loaded(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 1335 | `{` |
|        - | 1336 | `	vm_ext_match sMatch;` |
|        - | 1337 | `	const char *zName;` |
|        - | 1338 | `	int nName;` |
|        - | 1339 | `	sxu32 n;` |
|       96 | 1340 | `	if( nArg < 1 ){` |
|      ! 0 | 1341 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 1342 | `		return PH7_OK;` |
|        - | 1343 | `	}` |
|       96 | 1344 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|      706 | 1345 | `	for( n = 0 ; n < SX_ARRAYSIZE(azExtension) ; ++n ){` |
|      692 | 1346 | `		if( nName == (int)SyStrlen(azExtension[n])` |
|      434 | 1347 | `		 && SyStrnicmp(zName,azExtension[n],(sxu32)nName) == 0 ){` |
|       84 | 1348 | `			ph7_result_bool(pCtx,1);` |
|       84 | 1349 | `			return PH7_OK;` |
|        - | 1350 | `		}` |
|      307 | 1351 | `	}` |
|       14 | 1352 | `	sMatch.zName = zName;` |
|       14 | 1353 | `	sMatch.nName = nName;` |
|       14 | 1354 | `	ph7_result_bool(pCtx,VmStubExtWalk(pCtx->pVm,VmStubExtMatch,&sMatch));` |
|       14 | 1355 | `	return PH7_OK;` |
|       49 | 1356 | `}` |
|        4 | 1357 | `static int VmStubExtCollect(const char *zName,int nName,void *pData)` |
|        1 | 1358 | `{` |
|        5 | 1359 | `	ph7_context *pCtx = (ph7_context *)((void **)pData)[0];` |
|        5 | 1360 | `	ph7_value *pArray = (ph7_value *)((void **)pData)[1];` |
|        5 | 1361 | `	ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|        5 | 1362 | `	if( pVal ){` |
|        5 | 1363 | `		ph7_value_string(pVal,zName,nName);` |
|        5 | 1364 | `		ph7_array_add_elem(pArray,0,pVal);` |
|        5 | 1365 | `		ph7_context_release_value(pCtx,pVal);` |
|        2 | 1366 | `	}` |
|        5 | 1367 | `	return 0;` |
|        1 | 1368 | `}` |
|        - | 1369 | `/*` |
|        - | 1370 | ` * array get_loaded_extensions(bool $zend_extensions = false)` |
|        - | 1371 | ` *  PHL loads no Zend extension, so the zend list is always empty.` |
|        - | 1372 | ` */` |
|        4 | 1373 | `PH7_PRIVATE int vm_builtin_get_loaded_extensions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 1374 | `{` |
|        6 | 1375 | `	ph7_value *pArray = ph7_context_new_array(pCtx);` |
|        - | 1376 | `	void *apData[2];` |
|        - | 1377 | `	sxu32 n;` |
|        6 | 1378 | `	if( pArray == 0 ){` |
|      ! 0 | 1379 | `		return PH7_ContextMemoryError(pCtx);` |
|        - | 1380 | `	}` |
|        6 | 1381 | `	if( nArg > 0 && ph7_value_to_bool(apArg[0]) ){` |
|      ! 0 | 1382 | `		ph7_result_value(pCtx,pArray);` |
|      ! 0 | 1383 | `		return PH7_OK;` |
|        - | 1384 | `	}` |
|       66 | 1385 | `	for( n = 0 ; n < SX_ARRAYSIZE(azExtension) ; ++n ){` |
|       62 | 1386 | `		ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|       62 | 1387 | `		if( pVal ){` |
|       62 | 1388 | `			ph7_value_string(pVal,azExtension[n],-1);` |
|       62 | 1389 | `			ph7_array_add_elem(pArray,0,pVal);` |
|       62 | 1390 | `			ph7_context_release_value(pCtx,pVal);` |
|       30 | 1391 | `		}` |
|       32 | 1392 | `	}` |
|        6 | 1393 | `	apData[0] = pCtx;` |
|        6 | 1394 | `	apData[1] = pArray;` |
|        6 | 1395 | `	VmStubExtWalk(pCtx->pVm,VmStubExtCollect,apData);` |
|        6 | 1396 | `	ph7_result_value(pCtx,pArray);` |
|        6 | 1397 | `	return PH7_OK;` |
|        4 | 1398 | `}` |
|        - | 1399 | `/*` |
|        - | 1400 | ` * string php_sapi_name(void)` |
|        - | 1401 | ` *  Returns the type of interface (SAPI) PHL is running under.` |
|        - | 1402 | ` * Parameters` |
|        - | 1403 | ` *  None` |
|        - | 1404 | ` * Return` |
|        - | 1405 | ` *  "cli-server" while serving an HTTP request via the built-in -S server, "cli" otherwise.` |
|        - | 1406 | ` */` |
|        2 | 1407 | `PH7_PRIVATE int vm_builtin_php_sapi_name(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1408 | `{` |
|        3 | 1409 | `	const char *zSapi = pCtx->pVm->bHttpContext ? "cli-server" : "cli";` |
|        1 | 1410 | `	SXUNUSED(nArg);` |
|        1 | 1411 | `	SXUNUSED(apArg); /* cc warning */` |
|        3 | 1412 | `	ph7_result_string(pCtx,zSapi,-1);` |
|        3 | 1413 | `	return PH7_OK;` |
|        1 | 1414 | `}` |
|        - | 1415 | `/*` |
|        - | 1416 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 1417 | ` */` |
|        - | 1418 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 1419 | ` "<html><head>"\` |
|        - | 1420 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 1421 | ` "<style type=\"text/css\">"\` |
|        - | 1422 | ` "div {"\` |
|        - | 1423 | `     "border: 1px solid #cccccc;"\` |
|        - | 1424 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 1425 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 1426 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 1427 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 1428 | `     "-webkit-border-radius: 10px;"\` |
|        - | 1429 | `     "-o-border-radius: 10px;"\` |
|        - | 1430 | `     "border-radius: 10px;"\` |
|        - | 1431 | `     "padding-left: 2em;"\` |
|        - | 1432 | `     "background-color: white;"\` |
|        - | 1433 | `     "margin-left: auto;"\` |
|        - | 1434 | `     "font-family: verdana;"\` |
|        - | 1435 | `     "padding-right: 2em;"\` |
|        - | 1436 | `     "margin-right: auto;"\` |
|        - | 1437 | `     "}"\` |
|        - | 1438 | `     "body {"\` |
|        - | 1439 | `     "padding: 0.2em;"\` |
|        - | 1440 | `     "font-style: normal;"\` |
|        - | 1441 | `     "font-size: medium;"\` |
|        - | 1442 | `     "background-color: #f2f2f2;"\` |
|        - | 1443 | `     "}"\` |
|        - | 1444 | `     "hr {"\` |
|        - | 1445 | `     "border-style: solid none none;"\` |
|        - | 1446 | `     "border-width: 1px medium medium;"\` |
|        - | 1447 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 1448 | `     "height: 1px;"\` |
|        - | 1449 | `     "}"\` |
|        - | 1450 | `     "a {"\` |
|        - | 1451 | `     "color: #3366cc;"\` |
|        - | 1452 | `     "text-decoration: none;"\` |
|        - | 1453 | `     "}"\` |
|        - | 1454 | `     "a:hover {"\` |
|        - | 1455 | `     "color: #999999;"\` |
|        - | 1456 | `     "}"\` |
|        - | 1457 | `     "a:active {"\` |
|        - | 1458 | `     "color: #663399;"\` |
|        - | 1459 | `     "}"\` |
|        - | 1460 | `     "h1 {"\` |
|        - | 1461 | `     "margin: 0;"\` |
|        - | 1462 | `     "padding: 0;"\` |
|        - | 1463 | `     "font-family: Verdana;"\` |
|        - | 1464 | `     "font-weight: bold;"\` |
|        - | 1465 | `     "font-style: normal;"\` |
|        - | 1466 | `     "font-size: medium;"\` |
|        - | 1467 | `     "text-transform: capitalize;"\` |
|        - | 1468 | `     "color: #0a328c;"\` |
|        - | 1469 | `     "}"\` |
|        - | 1470 | `     "p {"\` |
|        - | 1471 | `     "margin: 0 auto;"\` |
|        - | 1472 | `     "font-size: medium;"\` |
|        - | 1473 | `     "font-style: normal;"\` |
|        - | 1474 | `     "font-family: verdana;"\` |
|        - | 1475 | `     "}"\` |
|        - | 1476 | `"</style></head><body>"\` |
|        - | 1477 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 1478 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 1479 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 1480 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 1481 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 1482 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 1483 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 1484 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 1485 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 1486 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 1487 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 1488 |  |
|        - | 1489 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 1490 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 1491 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 1492 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 1493 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 1494 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 1495 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 1496 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 1497 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 1498 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 1499 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 1500 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 1501 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 1502 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 1503 |  |
|        - | 1504 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 1505 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 1506 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 1507 | `"&nbsp;*<br>"\` |
|        - | 1508 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 1509 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 1510 | `"&nbsp;* are met:<br>"\` |
|        - | 1511 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 1512 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 1513 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 1514 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 1515 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 1516 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 1517 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 1518 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 1519 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 1520 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 1521 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 1522 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 1523 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 1524 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 1525 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 1526 | `"&nbsp;*<br>"\` |
|        - | 1527 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 1528 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 1529 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 1530 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 1531 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 1532 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 1533 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 1534 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 1535 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 1536 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 1537 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 1538 | `"&nbsp;*/<br>"\` |
|        - | 1539 | `"</span></small></small></p>"\` |
|        - | 1540 | `"</div></body></html>"` |
|        - | 1541 | `/*` |
|        - | 1542 | ` * bool ph7credits(void)` |
|        - | 1543 | ` * bool ph7info(void)` |
|        - | 1544 | ` * bool ph7copyright(void)` |
|        - | 1545 | ` *  Prints out the credits for PH7 engine` |
|        - | 1546 | ` * Parameters` |
|        - | 1547 | ` *  None` |
|        - | 1548 | ` * Return` |
|        - | 1549 | ` *  Always TRUE` |
|        - | 1550 | ` */` |
|        2 | 1551 | `PH7_PRIVATE int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1552 | `{` |
|        3 | 1553 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 1554 | `	/* Expand the HTML page above*/` |
|        3 | 1555 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 1556 | `	ph7_context_output_format(` |
|        1 | 1557 | `		pCtx,` |
|        - | 1558 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 1559 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 1560 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 1561 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 1562 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 1563 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 1564 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 1565 | `#ifdef __WINNT__` |
|        - | 1566 | `		"Windows NT"` |
|        - | 1567 | `#elif defined(__UNIXES__)` |
|        - | 1568 | `		"UNIX-Like"` |
|        - | 1569 | `#else` |
|        - | 1570 | `		"Other OS"` |
|        - | 1571 | `#endif` |
|        - | 1572 | `		);` |
|        3 | 1573 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 1574 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 1575 | `	SXUNUSED(apArg);` |
|        - | 1576 | `	/* Return TRUE */` |
|        - | 1577 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 1578 | `	return PH7_OK;` |
|        1 | 1579 | `}` |
|        - | 1580 | `/*` |
|        - | 1581 | ` * Section:` |
|        - | 1582 | ` *    URL related routines.` |
|        - | 1583 | ` * Status:` |
|        - | 1584 | ` *    Stable.` |
|        - | 1585 | ` */` |
|        - | 1586 | `/*` |
|        - | 1587 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 1588 | ` *  Parse a URL and return its fields.` |
|        - | 1589 | ` * Parameters` |
|        - | 1590 | ` *  $url` |
|        - | 1591 | ` *   The URL to parse.` |
|        - | 1592 | ` * $component` |
|        - | 1593 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 1594 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 1595 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 1596 | ` *  in which case the return value will be an integer).` |
|        - | 1597 | ` * Return` |
|        - | 1598 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 1599 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 1600 | ` *  this array are:` |
|        - | 1601 | ` *   scheme - e.g. http` |
|        - | 1602 | ` *   host` |
|        - | 1603 | ` *   port` |
|        - | 1604 | ` *   user` |
|        - | 1605 | ` *   pass` |
|        - | 1606 | ` *   path` |
|        - | 1607 | ` *   query - after the question mark ?` |
|        - | 1608 | ` *   fragment - after the hashmark #` |
|        - | 1609 | ` * Note:` |
|        - | 1610 | ` *  FALSE is returned on failure.` |
|        - | 1611 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 1612 | ` *  with the standard PHP engine.` |
|        - | 1613 | ` */` |
|        - | 1614 | `/*` |
|        - | 1615 | ` * parse_url() component set.` |
|        - | 1616 | ` *` |
|        - | 1617 | ` * A bare SyString cannot express "present but empty", which php needs:` |
|        - | 1618 | ` * parse_url("") is ['path'=>''] and parse_url("?") is ['query'=>''], both` |
|        - | 1619 | ` * distinct from the component being absent. So presence is tracked separately.` |
|        - | 1620 | ` */` |
|        - | 1621 | `typedef struct VmUrlParts VmUrlParts;` |
|        - | 1622 | `struct VmUrlParts` |
|        - | 1623 | `{` |
|        - | 1624 | `	SyString sScheme,sUser,sPass,sHost,sPath,sQuery,sFragment;` |
|        - | 1625 | `	int iPort;     /* Resolved port, meaningful only when bPort is set */` |
|        - | 1626 | `	sxu8 bScheme,bUser,bPass,bHost,bPort,bPath,bQuery,bFragment;` |
|        - | 1627 | `};` |
|      256 | 1628 | `static int VmUrlIsAlnum(int c)` |
|        1 | 1629 | `{` |
|      257 | 1630 | `	return (c >= '0' && c <= '9') \|\| (c >= 'a' && c <= 'z') \|\| (c >= 'A' && c <= 'Z');` |
|        1 | 1631 | `}` |
|        4 | 1632 | `static int VmUrlIsAlpha(int c)` |
|        1 | 1633 | `{` |
|        5 | 1634 | `	return (c >= 'a' && c <= 'z') \|\| (c >= 'A' && c <= 'Z');` |
|        1 | 1635 | `}` |
|        - | 1636 | `/* scheme = ALNUM *( ALNUM / "+" / "-" / "." ) -- php accepts a digit first. */` |
|      256 | 1637 | `static int VmUrlIsSchemeByte(int c)` |
|        1 | 1638 | `{` |
|      257 | 1639 | `	return VmUrlIsAlnum(c) \|\| c == '+' \|\| c == '-' \|\| c == '.';` |
|        1 | 1640 | `}` |
|        - | 1641 | `/*` |
|        - | 1642 | ` * Resolve the port span that followed the ':' in an authority.` |
|        - | 1643 | ` *` |
|        - | 1644 | ` * Returns 1 (usable port in *piPort), 0 (the span is empty, so there is simply` |
|        - | 1645 | ` * no port) or -1 (php rejects the whole URL). php is lenient about what follows` |
|        - | 1646 | ` * the digits -- ":8a" and ":8 0" both yield 8 -- but demands at least one digit` |
|        - | 1647 | ` * and a value that fits a port, so ":abc", ":-80" and ":65536" are all failures.` |
|        - | 1648 | ` */` |
|       42 | 1649 | `static int VmUrlParsePort(const char *z,int n,int *piPort)` |
|        1 | 1650 | `{` |
|       43 | 1651 | `	int i = 0,iVal = 0,nDigit = 0;` |
|       43 | 1652 | `	if( n < 1 ){` |
|      ! 0 | 1653 | `		return 0;` |
|        - | 1654 | `	}` |
|       64 | 1655 | `	while( i < n && (z[i] == ' ' \|\| z[i] == '\t') ){` |
|      ! 0 | 1656 | `		i++;` |
|      ! 0 | 1657 | `	}` |
|       43 | 1658 | `	if( i < n && (z[i] == '+' \|\| z[i] == '-') ){` |
|      ! 0 | 1659 | `		if( z[i] == '-' ){` |
|      ! 0 | 1660 | `			return -1;` |
|        - | 1661 | `		}` |
|      ! 0 | 1662 | `		i++;` |
|      ! 0 | 1663 | `	}` |
|      161 | 1664 | `	while( i < n && z[i] >= '0' && z[i] <= '9' ){` |
|      119 | 1665 | `		iVal = iVal * 10 + (z[i] - '0');` |
|      119 | 1666 | `		if( iVal > 65535 ){` |
|      ! 0 | 1667 | `			return -1; /* Out of range, and this also caps the accumulator */` |
|        - | 1668 | `		}` |
|      119 | 1669 | `		nDigit++;` |
|      119 | 1670 | `		i++;` |
|        1 | 1671 | `	}` |
|       43 | 1672 | `	if( nDigit < 1 ){` |
|        3 | 1673 | `		return -1;` |
|        - | 1674 | `	}` |
|       41 | 1675 | `	*piPort = iVal;` |
|       41 | 1676 | `	return 1;` |
|       22 | 1677 | `}` |
|        - | 1678 | `/*` |
|        - | 1679 | ` * Split an authority -- "[user[:pass]@]host[:port]" -- into pOut.` |
|        - | 1680 | ` * Returns 0 for the malformed input php reports as FALSE.` |
|        - | 1681 | ` */` |
|       66 | 1682 | `static int VmUrlParseAuthority(const char *z,int n,VmUrlParts *pOut,int bPortKnown)` |
|        1 | 1683 | `{` |
|        - | 1684 | `	const char *zHost;` |
|       67 | 1685 | `	int i,iAt = -1,iColon = -1,iSep = -1,nHost,iPort = 0,rc;` |
|        - | 1686 | `	/* php splits the credentials at the LAST '@' ("//a@b@c" is user "a@b") */` |
|      699 | 1687 | `	for( i = 0 ; i < n ; ++i ){` |
|      633 | 1688 | `		if( z[i] == '@' ){` |
|       25 | 1689 | `			iAt = i;` |
|       12 | 1690 | `		}` |
|      317 | 1691 | `	}` |
|       67 | 1692 | `	if( iAt >= 0 ){` |
|        - | 1693 | `		/* and the user from the password at the FIRST ':' before it */` |
|      109 | 1694 | `		for( i = 0 ; i < iAt ; ++i ){` |
|      107 | 1695 | `			if( z[i] == ':' ){` |
|       23 | 1696 | `				iColon = i;` |
|       23 | 1697 | `				break;` |
|        - | 1698 | `			}` |
|       43 | 1699 | `		}` |
|       25 | 1700 | `		if( iColon >= 0 ){` |
|       23 | 1701 | `			SyStringInitFromBuf(&pOut->sUser,z,(sxu32)iColon);` |
|       23 | 1702 | `			SyStringInitFromBuf(&pOut->sPass,&z[iColon+1],(sxu32)(iAt - iColon - 1));` |
|       23 | 1703 | `			pOut->bUser = pOut->bPass = 1;` |
|       12 | 1704 | `		}else{` |
|        3 | 1705 | `			SyStringInitFromBuf(&pOut->sUser,z,(sxu32)iAt);` |
|        3 | 1706 | `			pOut->bUser = 1;` |
|        - | 1707 | `		}` |
|       25 | 1708 | `		z += iAt + 1;` |
|       25 | 1709 | `		n -= iAt + 1;` |
|       12 | 1710 | `	}` |
|       67 | 1711 | `	zHost = z;` |
|       67 | 1712 | `	nHost = n;` |
|       67 | 1713 | `	if( !(n > 1 && z[0] == '[' && z[n-1] == ']') ){` |
|        - | 1714 | `		/* Unless a bracketed IPv6 literal fills the WHOLE authority -- in which` |
|        - | 1715 | `		 * case php keeps the brackets in "host" and scans no port, so every ':'` |
|        - | 1716 | `		 * inside belongs to the address -- the port hangs off the LAST ':'.` |
|        - | 1717 | `		 * php decides that on the first and last byte alone, which is why` |
|        - | 1718 | `		 * "//[:]\|]" is a single host while "//[::1]:8080" splits a port off. */` |
|      507 | 1719 | `		for( i = 0 ; i < n ; ++i ){` |
|      443 | 1720 | `			if( z[i] == ':' ){` |
|       43 | 1721 | `				iSep = i;` |
|       21 | 1722 | `			}` |
|      222 | 1723 | `		}` |
|       65 | 1724 | `		if( iSep >= 0 ){` |
|        - | 1725 | `			/* The host is trimmed at the ':' whether or not the port was already` |
|        - | 1726 | `			 * resolved by the caller. */` |
|       43 | 1727 | `			nHost = iSep;` |
|       43 | 1728 | `			if( !bPortKnown ){` |
|       37 | 1729 | `				rc = VmUrlParsePort(&z[iSep+1],n - iSep - 1,&iPort);` |
|       37 | 1730 | `				if( rc < 0 ){` |
|        3 | 1731 | `					return 0;` |
|        - | 1732 | `				}` |
|       35 | 1733 | `				if( rc > 0 ){` |
|       35 | 1734 | `					pOut->iPort = iPort;` |
|       35 | 1735 | `					pOut->bPort = 1;` |
|       17 | 1736 | `				}` |
|       17 | 1737 | `			}` |
|       20 | 1738 | `		}` |
|       31 | 1739 | `	}` |
|       65 | 1740 | `	if( nHost < 1 ){` |
|        - | 1741 | `		/* php requires a non-empty host once an authority is in play, which is` |
|        - | 1742 | `		 * what makes "//", "http://" and ":80" all FALSE. */` |
|        9 | 1743 | `		return 0;` |
|        - | 1744 | `	}` |
|       57 | 1745 | `	SyStringInitFromBuf(&pOut->sHost,zHost,(sxu32)nHost);` |
|       57 | 1746 | `	pOut->bHost = 1;` |
|       57 | 1747 | `	return 1;` |
|       34 | 1748 | `}` |
|        - | 1749 | `/*` |
|        - | 1750 | ` * Split "path[?query][#fragment]". The fragment is taken FIRST and the query` |
|        - | 1751 | ` * only from what precedes it, so "#a?b" is a fragment of "a?b" with no query.` |
|        - | 1752 | ` */` |
|       80 | 1753 | `static void VmUrlParsePath(const char *z,int n,VmUrlParts *pOut)` |
|        1 | 1754 | `{` |
|       81 | 1755 | `	int i,iEnd = n;` |
|      547 | 1756 | `	for( i = 0 ; i < n ; ++i ){` |
|      501 | 1757 | `		if( z[i] == '#' ){` |
|       35 | 1758 | `			SyStringInitFromBuf(&pOut->sFragment,&z[i+1],(sxu32)(n - i - 1));` |
|       35 | 1759 | `			pOut->bFragment = 1;` |
|       35 | 1760 | `			iEnd = i;` |
|       35 | 1761 | `			break;` |
|        - | 1762 | `		}` |
|      234 | 1763 | `	}` |
|      393 | 1764 | `	for( i = 0 ; i < iEnd ; ++i ){` |
|      347 | 1765 | `		if( z[i] == '?' ){` |
|       35 | 1766 | `			SyStringInitFromBuf(&pOut->sQuery,&z[i+1],(sxu32)(iEnd - i - 1));` |
|       35 | 1767 | `			pOut->bQuery = 1;` |
|       35 | 1768 | `			iEnd = i;` |
|       35 | 1769 | `			break;` |
|        - | 1770 | `		}` |
|      157 | 1771 | `	}` |
|       81 | 1772 | `	if( iEnd > 0 ){` |
|       71 | 1773 | `		SyStringInitFromBuf(&pOut->sPath,z,(sxu32)iEnd);` |
|       71 | 1774 | `		pOut->bPath = 1;` |
|       35 | 1775 | `	}` |
|       81 | 1776 | `}` |
|        - | 1777 | `/* Parse "host[:port]" followed by an optional path/query/fragment. */` |
|       66 | 1778 | `static int VmUrlAuthorityThenPath(const char *z,int n,VmUrlParts *pOut,int bPortKnown)` |
|        1 | 1779 | `{` |
|       67 | 1780 | `	int i,iEnd = n;` |
|      699 | 1781 | `	for( i = 0 ; i < n ; ++i ){` |
|      683 | 1782 | `		if( z[i] == '/' \|\| z[i] == '?' \|\| z[i] == '#' ){` |
|       51 | 1783 | `			iEnd = i;` |
|       51 | 1784 | `			break;` |
|        - | 1785 | `		}` |
|      317 | 1786 | `	}` |
|       67 | 1787 | `	if( !VmUrlParseAuthority(z,iEnd,pOut,bPortKnown) ){` |
|       11 | 1788 | `		return 0;` |
|        - | 1789 | `	}` |
|       57 | 1790 | `	if( iEnd < n ){` |
|       47 | 1791 | `		VmUrlParsePath(&z[iEnd],n - iEnd,pOut);` |
|       23 | 1792 | `	}` |
|       57 | 1793 | `	return 1;` |
|       34 | 1794 | `}` |
|        - | 1795 | `/*` |
|        - | 1796 | ` * Resolve the port php took from the FIRST ':' of a host:port URL.` |
|        - | 1797 | ` *` |
|        - | 1798 | ` * php reads the port straight off that colon before it works out where the host` |
|        - | 1799 | ` * ends, so the digits can even belong to what becomes the path: parse_url()` |
|        - | 1800 | ` * reports port 1 for "a/:1", whose path is "/:1". Mirroring the order keeps` |
|        - | 1801 | ` * that quirk. Returns 0 for a port php rejects.` |
|        - | 1802 | ` */` |
|        6 | 1803 | `static int VmUrlPreparePort(const char *z,int k,int nEnd,VmUrlParts *pOut)` |
|        1 | 1804 | `{` |
|        7 | 1805 | `	int iPort = 0;` |
|        7 | 1806 | `	int rc = VmUrlParsePort(&z[k+1],nEnd - k - 1,&iPort);` |
|        7 | 1807 | `	if( rc < 0 ){` |
|      ! 0 | 1808 | `		return 0;` |
|        - | 1809 | `	}` |
|        7 | 1810 | `	if( rc > 0 ){` |
|        7 | 1811 | `		pOut->iPort = iPort;` |
|        7 | 1812 | `		pOut->bPort = 1;` |
|        3 | 1813 | `	}` |
|        7 | 1814 | `	return 1;` |
|        4 | 1815 | `}` |
|        - | 1816 | `/*` |
|        - | 1817 | ` * php's URL parser, as parse_url() needs it. Returns 0 where php returns FALSE.` |
|        - | 1818 | ` *` |
|        - | 1819 | ` * PH7_VmHttpSplitURI cannot serve here: it is an HTTP REQUEST-target splitter` |
|        - | 1820 | ` * (the HTTP server and filter_var share it) and assumes the input is` |
|        - | 1821 | ` * authority-first, so it read "a" as a host and "mailto:me@x.com" as` |
|        - | 1822 | ` * user:pass@host. php instead decides authority-vs-path up front: only a "//",` |
|        - | 1823 | ` * with or without a scheme before it, introduces an authority.` |
|        - | 1824 | ` */` |
|      104 | 1825 | `static int VmUrlSplit(const char *z,int n,VmUrlParts *pOut)` |
|        1 | 1826 | `{` |
|      105 | 1827 | `	int i,k = -1,bScheme,bPortForm = 0,nPortEnd = 0;` |
|      105 | 1828 | `	SyZero(pOut,sizeof(VmUrlParts));` |
|        - | 1829 | `	/* A leading "//" settles it before any colon is considered: the authority` |
|        - | 1830 | `	 * starts after the slashes. Reading the colon first turned "//h:80" into a` |
|        - | 1831 | `	 * host called "//h" and "//[::1]" into a path. */` |
|      105 | 1832 | `	if( n >= 2 && z[0] == '/' && z[1] == '/' ){` |
|       13 | 1833 | `		return VmUrlAuthorityThenPath(&z[2],n - 2,pOut,0);` |
|        - | 1834 | `	}` |
|      441 | 1835 | `	for( i = 0 ; i < n ; ++i ){` |
|      419 | 1836 | `		if( z[i] == ':' ){` |
|       71 | 1837 | `			k = i;` |
|       71 | 1838 | `			break;` |
|        - | 1839 | `		}` |
|      175 | 1840 | `	}` |
|       93 | 1841 | `	if( k == 0 && n == 1 ){` |
|        - | 1842 | `		/* A lone ":" is an EMPTY scheme, which php rejects outright -- unlike` |
|        - | 1843 | `		 * ":a" or "::", which are simply paths. */` |
|        3 | 1844 | `		return 0;` |
|        - | 1845 | `	}` |
|       91 | 1846 | `	bScheme = k > 0;` |
|      347 | 1847 | `	for( i = 0 ; bScheme && i < k ; ++i ){` |
|      257 | 1848 | `		if( !VmUrlIsSchemeByte((unsigned char)z[i]) ){` |
|      ! 0 | 1849 | `			bScheme = 0;` |
|      ! 0 | 1850 | `		}` |
|      129 | 1851 | `	}` |
|       91 | 1852 | `	if( bScheme && k + 1 == n ){` |
|        - | 1853 | `		/* "x:" -- the scheme is the whole URL */` |
|        3 | 1854 | `		SyStringInitFromBuf(&pOut->sScheme,z,(sxu32)k);` |
|        3 | 1855 | `		pOut->bScheme = 1;` |
|        3 | 1856 | `		return 1;` |
|        - | 1857 | `	}` |
|        - | 1858 | `	/* Decide whether that ':' introduces a PORT rather than a scheme: at least` |
|        - | 1859 | `	 * one digit, running to the end of the string or to a '/'. That is what` |
|        - | 1860 | `	 * makes "a:80" a host:port while "a:80?q" is a scheme with path "80", and` |
|        - | 1861 | `	 * it holds however the ':' is reached -- ":1" and "/:1" are both authorities` |
|        - | 1862 | `	 * with an empty host, which php rejects. php caps the run, so a long number` |
|        - | 1863 | `	 * stays a path: "a:123456" is a path, "a:99999" an out-of-range port. */` |
|       89 | 1864 | `	if( k >= 0 ){` |
|       67 | 1865 | `		int p = k + 1;` |
|       67 | 1866 | `		int bBeforeQuery = 1;` |
|       67 | 1867 | `		nPortEnd = k + 1;` |
|        - | 1868 | `		/* A ':' that sits inside a query or fragment is just data: "?:1" is a` |
|        - | 1869 | `		 * query of ":1", not an authority with an empty host. */` |
|      321 | 1870 | `		for( i = 0 ; i < k ; ++i ){` |
|      255 | 1871 | `			if( z[i] == '?' \|\| z[i] == '#' ){` |
|      ! 0 | 1872 | `				bBeforeQuery = 0;` |
|      ! 0 | 1873 | `				break;` |
|        - | 1874 | `			}` |
|      128 | 1875 | `		}` |
|       77 | 1876 | `		while( p < n && z[p] >= '0' && z[p] <= '9' ){` |
|       11 | 1877 | `			p++;` |
|        1 | 1878 | `		}` |
|       67 | 1879 | `		if( bBeforeQuery && p > k + 1 && (p >= n \|\| z[p] == '/') && (p - k) < 7 ){` |
|        7 | 1880 | `			bPortForm = 1;` |
|        7 | 1881 | `			nPortEnd = p;` |
|        3 | 1882 | `		}` |
|       33 | 1883 | `	}` |
|       89 | 1884 | `	if( !bScheme ){` |
|       25 | 1885 | `		if( bPortForm ){` |
|        3 | 1886 | `			if( !VmUrlPreparePort(z,k,nPortEnd,pOut) ){` |
|      ! 0 | 1887 | `				return 0;` |
|        - | 1888 | `			}` |
|        3 | 1889 | `			return VmUrlAuthorityThenPath(z,n,pOut,1);` |
|        - | 1890 | `		}` |
|       23 | 1891 | `		VmUrlParsePath(z,n,pOut);` |
|       23 | 1892 | `		if( !pOut->bPath && !pOut->bQuery && !pOut->bFragment ){` |
|        - | 1893 | `			/* php reports the empty URL as an empty PATH, not as no components */` |
|        3 | 1894 | `			SyStringInitFromBuf(&pOut->sPath,z,0);` |
|        3 | 1895 | `			pOut->bPath = 1;` |
|        1 | 1896 | `		}` |
|       23 | 1897 | `		return 1;` |
|        - | 1898 | `	}` |
|       65 | 1899 | `	if( bPortForm ){` |
|        5 | 1900 | `		if( !VmUrlPreparePort(z,k,nPortEnd,pOut) ){` |
|      ! 0 | 1901 | `			return 0;` |
|        - | 1902 | `		}` |
|        5 | 1903 | `		return VmUrlAuthorityThenPath(z,n,pOut,1);` |
|        - | 1904 | `	}` |
|       61 | 1905 | `	SyStringInitFromBuf(&pOut->sScheme,z,(sxu32)k);` |
|       61 | 1906 | `	pOut->bScheme = 1;` |
|       61 | 1907 | `	if( z[k+1] == '/' && k + 2 < n && z[k+2] == '/' ){` |
|       52 | 1908 | `		if( k + 3 < n && z[k+3] == '/' && pOut->sScheme.nByte == 4` |
|        4 | 1909 | `		 && (z[0]=='f'\|\|z[0]=='F') && (z[1]=='i'\|\|z[1]=='I')` |
|        5 | 1910 | `		 && (z[2]=='l'\|\|z[2]=='L') && (z[3]=='e'\|\|z[3]=='E') ){` |
|        - | 1911 | `			/* file:/// has no authority: the path starts at the third slash,` |
|        - | 1912 | `			 * except that a "c:" drive letter swallows it (file:///c:/x is the` |
|        - | 1913 | `			 * path "c:/x"). A '\|' in place of the ':' does NOT count. */` |
|        5 | 1914 | `			int iBase = k + 3;` |
|        5 | 1915 | `			if( iBase + 2 < n && VmUrlIsAlpha((unsigned char)z[iBase+1]) && z[iBase+2] == ':' ){` |
|      ! 0 | 1916 | `				iBase++;` |
|      ! 0 | 1917 | `			}` |
|        5 | 1918 | `			VmUrlParsePath(&z[iBase],n - iBase,pOut);` |
|        5 | 1919 | `			return 1;` |
|        - | 1920 | `		}` |
|       49 | 1921 | `		return VmUrlAuthorityThenPath(&z[k+3],n - k - 3,pOut,0);` |
|        - | 1922 | `	}` |
|        - | 1923 | `	/* "mailto:me@x.com", "x:y", "http:/x": everything after the ':' is a path */` |
|        9 | 1924 | `	VmUrlParsePath(&z[k+1],n - k - 1,pOut);` |
|        9 | 1925 | `	return 1;` |
|       53 | 1926 | `}` |
|        - | 1927 | `/*` |
|        - | 1928 | ` * Emit one parsed component into pValue, replacing control bytes with '_'.` |
|        - | 1929 | ` *` |
|        - | 1930 | ` * php runs php_replace_controlchars_ex over every string component it returns,` |
|        - | 1931 | ` * so a URL carrying a raw NUL, newline or DEL cannot smuggle it through into` |
|        - | 1932 | ` * whatever the caller splices the component into (a header, a log line, a` |
|        - | 1933 | ` * redirect). Bytes >= 0x80 are deliberately left alone -- php only folds the` |
|        - | 1934 | ` * ASCII control range.` |
|        - | 1935 | ` */` |
|      164 | 1936 | `static void VmUrlSetComponent(ph7_value *pValue,const SyString *pComp)` |
|        1 | 1937 | `{` |
|      165 | 1938 | `	const char *z = pComp->zString;` |
|      165 | 1939 | `	sxu32 n = pComp->nByte,i,iRun = 0;` |
|      165 | 1940 | `	if( n < 1 \|\| z == 0 ){` |
|        3 | 1941 | `		ph7_value_string(pValue,"",0);` |
|        3 | 1942 | `		return;` |
|        - | 1943 | `	}` |
|      955 | 1944 | `	for( i = 0 ; i < n ; ++i ){` |
|      793 | 1945 | `		unsigned char c = (unsigned char)z[i];` |
|      793 | 1946 | `		if( c < 0x20 \|\| c == 0x7f ){` |
|        3 | 1947 | `			if( i > iRun ){` |
|        3 | 1948 | `				ph7_value_string(pValue,&z[iRun],(int)(i - iRun));` |
|        1 | 1949 | `			}` |
|        3 | 1950 | `			ph7_value_string(pValue,"_",1);` |
|        3 | 1951 | `			iRun = i + 1;` |
|        1 | 1952 | `		}` |
|      397 | 1953 | `	}` |
|      163 | 1954 | `	if( n > iRun ){` |
|      163 | 1955 | `		ph7_value_string(pValue,&z[iRun],(int)(n - iRun));` |
|       81 | 1956 | `	}` |
|       83 | 1957 | `}` |
|      104 | 1958 | `PH7_PRIVATE int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1959 | `{` |
|        - | 1960 | `	const char *zStr; /* Input string */` |
|        - | 1961 | `	VmUrlParts sUrl;  /* Parse of the given URI */` |
|        - | 1962 | `	SyString *pComp;` |
|        - | 1963 | `	int bHave;` |
|        - | 1964 | `	int nLen;` |
|      105 | 1965 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 1966 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 1967 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 1968 | `		return PH7_OK;` |
|        - | 1969 | `	}` |
|        - | 1970 | `	/* Extract the given URI. An empty string is NOT a failure: php parses it as` |
|        - | 1971 | `	 * an empty path. */` |
|      105 | 1972 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|      105 | 1973 | `	if( nLen < 0 ){` |
|      ! 0 | 1974 | `		nLen = 0;` |
|      ! 0 | 1975 | `	}` |
|      105 | 1976 | `	if( !VmUrlSplit(zStr,nLen,&sUrl) ){` |
|        - | 1977 | `		/* Malformed input,return FALSE */` |
|       13 | 1978 | `		ph7_result_bool(pCtx,0);` |
|       13 | 1979 | `		return PH7_OK;` |
|        - | 1980 | `	}` |
|      103 | 1981 | `	if( nArg > 1 && ph7_value_to_int(apArg[1]) >= 0 ){` |
|        - | 1982 | `		/* Refer to constant.c for constants values (php's PHP_URL_* are 0-based;` |
|        - | 1983 | `		 * PHL used to number them from 1, so every literal component id selected` |
|        - | 1984 | `		 * the WRONG field -- and the constants' own tests only asserted "%d",` |
|        - | 1985 | `		 * which any numbering satisfies). A negative id means "the whole array",` |
|        - | 1986 | `		 * which is what the default $component = -1 relies on. */` |
|       27 | 1987 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|       27 | 1988 | `		pComp = 0;` |
|       27 | 1989 | `		bHave = 0;` |
|       27 | 1990 | `		switch(nComponent){` |
|        3 | 1991 | `		case 0: /* PHP_URL_SCHEME */   pComp = &sUrl.sScheme;   bHave = sUrl.bScheme;   break;` |
|        5 | 1992 | `		case 1: /* PHP_URL_HOST */     pComp = &sUrl.sHost;     bHave = sUrl.bHost;     break;` |
|        2 | 1993 | `		case 2: /* PHP_URL_PORT */` |
|        5 | 1994 | `			if( sUrl.bPort ){` |
|        5 | 1995 | `				ph7_result_int(pCtx,sUrl.iPort);` |
|        3 | 1996 | `			}else{` |
|      ! 0 | 1997 | `				ph7_result_null(pCtx);` |
|        - | 1998 | `			}` |
|        5 | 1999 | `			return PH7_OK;` |
|        3 | 2000 | `		case 3: /* PHP_URL_USER */     pComp = &sUrl.sUser;     bHave = sUrl.bUser;     break;` |
|        3 | 2001 | `		case 4: /* PHP_URL_PASS */     pComp = &sUrl.sPass;     bHave = sUrl.bPass;     break;` |
|        3 | 2002 | `		case 5: /* PHP_URL_PATH */     pComp = &sUrl.sPath;     bHave = sUrl.bPath;     break;` |
|        5 | 2003 | `		case 6: /* PHP_URL_QUERY */    pComp = &sUrl.sQuery;    bHave = sUrl.bQuery;    break;` |
|        5 | 2004 | `		case 7: /* PHP_URL_FRAGMENT */ pComp = &sUrl.sFragment; bHave = sUrl.bFragment; break;` |
|        1 | 2005 | `		default:` |
|        4 | 2006 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|        - | 2007 | `				"parse_url(): Argument #2 ($component) must be a valid URL component identifier, %d given",` |
|        1 | 2008 | `				nComponent);` |
|        - | 2009 | `		}` |
|       21 | 2010 | `		if( bHave ){` |
|       19 | 2011 | `			ph7_value *pOut = ph7_context_new_scalar(pCtx);` |
|       19 | 2012 | `			if( pOut == 0 ){` |
|      ! 0 | 2013 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|      ! 0 | 2014 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 2015 | `				return PH7_OK;` |
|        - | 2016 | `			}` |
|       19 | 2017 | `			VmUrlSetComponent(pOut,pComp);` |
|       19 | 2018 | `			ph7_result_value(pCtx,pOut);` |
|       10 | 2019 | `		}else{` |
|        - | 2020 | `			/* No available value,return NULL */` |
|        3 | 2021 | `			ph7_result_null(pCtx);` |
|        - | 2022 | `		}` |
|       11 | 2023 | `	}else{` |
|        - | 2024 | `		ph7_value *pArray,*pValue;` |
|        - | 2025 | `		/* Return an associative array */` |
|       67 | 2026 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       67 | 2027 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       67 | 2028 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 2029 | `			/* Out of memory */` |
|      ! 0 | 2030 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 2031 | `			/* Return false */` |
|      ! 0 | 2032 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 2033 | `			return PH7_OK;` |
|        - | 2034 | `		}` |
|        - | 2035 | `		/* Fill the array, in php's key order. A component is emitted whenever it` |
|        - | 2036 | `		 * is PRESENT, even when empty -- parse_url("?") is ['query'=>'']. */` |
|       67 | 2037 | `		if( sUrl.bScheme ){` |
|       33 | 2038 | `			VmUrlSetComponent(pValue,&sUrl.sScheme);` |
|       33 | 2039 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|       33 | 2040 | `			ph7_value_reset_string_cursor(pValue);` |
|       16 | 2041 | `		}` |
|       67 | 2042 | `		if( sUrl.bHost ){` |
|       31 | 2043 | `			VmUrlSetComponent(pValue,&sUrl.sHost);` |
|       31 | 2044 | `			ph7_array_add_strkey_elem(pArray,"host",pValue);` |
|       31 | 2045 | `			ph7_value_reset_string_cursor(pValue);` |
|       15 | 2046 | `		}` |
|       67 | 2047 | `		if( sUrl.bPort ){` |
|       17 | 2048 | `			ph7_value_int(pValue,sUrl.iPort);` |
|       17 | 2049 | `			ph7_array_add_strkey_elem(pArray,"port",pValue);` |
|       17 | 2050 | `			ph7_value_reset_string_cursor(pValue);` |
|        8 | 2051 | `		}` |
|       67 | 2052 | `		if( sUrl.bUser ){` |
|        9 | 2053 | `			VmUrlSetComponent(pValue,&sUrl.sUser);` |
|        9 | 2054 | `			ph7_array_add_strkey_elem(pArray,"user",pValue);` |
|        9 | 2055 | `			ph7_value_reset_string_cursor(pValue);` |
|        4 | 2056 | `		}` |
|       67 | 2057 | `		if( sUrl.bPass ){` |
|        7 | 2058 | `			VmUrlSetComponent(pValue,&sUrl.sPass);` |
|        7 | 2059 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue);` |
|        7 | 2060 | `			ph7_value_reset_string_cursor(pValue);` |
|        3 | 2061 | `		}` |
|       67 | 2062 | `		if( sUrl.bPath ){` |
|       47 | 2063 | `			VmUrlSetComponent(pValue,&sUrl.sPath);` |
|       47 | 2064 | `			ph7_array_add_strkey_elem(pArray,"path",pValue);` |
|       47 | 2065 | `			ph7_value_reset_string_cursor(pValue);` |
|       23 | 2066 | `		}` |
|       67 | 2067 | `		if( sUrl.bQuery ){` |
|       13 | 2068 | `			VmUrlSetComponent(pValue,&sUrl.sQuery);` |
|       13 | 2069 | `			ph7_array_add_strkey_elem(pArray,"query",pValue);` |
|       13 | 2070 | `			ph7_value_reset_string_cursor(pValue);` |
|        6 | 2071 | `		}` |
|       67 | 2072 | `		if( sUrl.bFragment ){` |
|       13 | 2073 | `			VmUrlSetComponent(pValue,&sUrl.sFragment);` |
|       13 | 2074 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue);` |
|        6 | 2075 | `		}` |
|        - | 2076 | `		/* Return the created array */` |
|       67 | 2077 | `		ph7_result_value(pCtx,pArray);` |
|        - | 2078 | `		/* NOTE:` |
|        - | 2079 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 2080 | `		 * automatically as soon we return from this function.` |
|        - | 2081 | `		 */` |
|        - | 2082 | `	}` |
|        - | 2083 | `	/* All done */` |
|       87 | 2084 | `	return PH7_OK;` |
|       53 | 2085 | `}` |
|        - | 2086 |  |
|        - | 2087 | `/*` |
|        - | 2088 | ` * Section:` |
|        - | 2089 | ` *   Array related routines.` |
|        - | 2090 | ` * Status:` |
|        - | 2091 | ` *    Stable.` |
|        - | 2092 | ` * Note 2012-5-21 01:04:15:` |
|        - | 2093 | ` *  Array related functions that need access to the underlying` |
|        - | 2094 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 2095 | ` */` |
|        - | 2096 | `/*` |
|        - | 2097 | ` * The [compact()] function store it's state information in an instance` |
|        - | 2098 | ` * of the following structure.` |
|        - | 2099 | ` */` |
|        - | 2100 | `struct compact_data` |
|        - | 2101 | `{` |
|        - | 2102 | `	ph7_value *pArray;  /* Target array */` |
|        - | 2103 | `	int nRecCount;      /* Recursion count */` |
|        - | 2104 | `	ph7_context *pCtx;  /* Call context, for php's two warnings */` |
|        - | 2105 | `	int iArg;           /* 1-based ARGUMENT this element came from: php names the` |
|        - | 2106 | `	                     * argument even for an element found inside a nested` |
|        - | 2107 | `	                     * array, never the element's own position. */` |
|        - | 2108 | `};` |
|        - | 2109 | `/*` |
|        - | 2110 | ` * php's two compact() diagnostics, both E_WARNING and both non-fatal (the entry` |
|        - | 2111 | ` * is skipped and the rest of the call proceeds): a name that is neither a string` |
|        - | 2112 | ` * nor an array of strings, and a string naming a variable the frame does not` |
|        - | 2113 | ` * have. PHL emitted neither, so compact(1) and compact('typo') answered a` |
|        - | 2114 | ` * shorter array with nothing said -- the caller's only clue that a name was` |
|        - | 2115 | ` * dropped was the array's own size.` |
|        - | 2116 | ` */` |
|       14 | 2117 | `static void VmCompactBadName(ph7_context *pCtx,int iArg,ph7_value *pValue)` |
|        1 | 2118 | `{` |
|        - | 2119 | `	char zGiven[64];` |
|       22 | 2120 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 2121 | `		"Argument #%d must be string or array of strings, %s given",` |
|        7 | 2122 | `		iArg,VmValueGivenName(pValue,zGiven,sizeof(zGiven)));` |
|       15 | 2123 | `}` |
|        6 | 2124 | `static void VmCompactUndefined(ph7_context *pCtx,SyString *pVar)` |
|        1 | 2125 | `{` |
|       10 | 2126 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        6 | 2127 | `		"Undefined variable $%.*s",(int)pVar->nByte,pVar->zString);` |
|        7 | 2128 | `}` |
|        - | 2129 | `/*` |
|        - | 2130 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 2131 | ` */` |
|       16 | 2132 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 2133 | `{` |
|       17 | 2134 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|       17 | 2135 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|       17 | 2136 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 2137 | `	/* Act according to the hashmap value */` |
|       17 | 2138 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 2139 | `		SyString sVar;` |
|        9 | 2140 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|        - | 2141 | `		/* Query the current frame. An EMPTY name is looked up like any other:` |
|        - | 2142 | `		 * php reports it as "Undefined variable $" rather than skipping it. */` |
|        9 | 2143 | `		pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 2144 | `		/* ^` |
|        - | 2145 | `		 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 2146 | `		 */` |
|        9 | 2147 | `		if( pKey ){` |
|        - | 2148 | `			/* Perform the insertion */` |
|        7 | 2149 | `			ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|        4 | 2150 | `		}else{` |
|        3 | 2151 | `			VmCompactUndefined(pData->pCtx,&sVar);` |
|        1 | 2152 | `		}` |
|       13 | 2153 | `	}else if( ph7_value_is_array(pValue) ){` |
|        - | 2154 | `		/* Recursively traverse this array. Past the depth cap the element is` |
|        - | 2155 | `		 * dropped in silence, as it always was: the cap is PHL's own guard and` |
|        - | 2156 | `		 * the "must be string or array of strings" warning would be a lie about` |
|        - | 2157 | `		 * an argument that IS an array of strings. */` |
|        7 | 2158 | `		if( pData->nRecCount < 32 ){` |
|        - | 2159 | `			int rc;` |
|        7 | 2160 | `			pData->nRecCount++;` |
|        7 | 2161 | `			rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|        7 | 2162 | `			pData->nRecCount--;` |
|        7 | 2163 | `			return rc;` |
|        - | 2164 | `		}` |
|      ! 0 | 2165 | `	}else{` |
|        3 | 2166 | `		VmCompactBadName(pData->pCtx,pData->iArg,pValue);` |
|        - | 2167 | `	}` |
|       11 | 2168 | `	return SXRET_OK;` |
|        9 | 2169 | `}` |
|        - | 2170 | `/*` |
|        - | 2171 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 2172 | ` *  Create array containing variables and their values.` |
|        - | 2173 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 2174 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 2175 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 2176 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 2177 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 2178 | ` * Parameters` |
|        - | 2179 | ` *  $varname` |
|        - | 2180 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 2181 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 2182 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 2183 | ` *   it recursively.` |
|        - | 2184 | ` * Return` |
|        - | 2185 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 2186 | ` */` |
|       26 | 2187 | `PH7_PRIVATE int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 2188 | `{` |
|        - | 2189 | `	ph7_value *pArray,*pObj;` |
|       27 | 2190 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 2191 | `	const char *zName;` |
|        - | 2192 | `	SyString sVar;` |
|        - | 2193 | `	int i,nLen;` |
|       27 | 2194 | `	if( nArg < 1 ){` |
|        - | 2195 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 2196 | `		ph7_result_null(pCtx);` |
|      ! 0 | 2197 | `		return PH7_OK;` |
|        - | 2198 | `	}` |
|        - | 2199 | `	/* Create the array */` |
|       27 | 2200 | `	pArray = ph7_context_new_array(pCtx);` |
|       27 | 2201 | `	if( pArray == 0 ){` |
|        - | 2202 | `		/* Out of memory */` |
|      ! 0 | 2203 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 2204 | `		/* Return NULL */` |
|      ! 0 | 2205 | `		ph7_result_null(pCtx);` |
|      ! 0 | 2206 | `		return PH7_OK;` |
|        - | 2207 | `	}` |
|        - | 2208 | `	/* Perform the requested operation */` |
|       65 | 2209 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       39 | 2210 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|       19 | 2211 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 2212 | `				struct compact_data sData;` |
|        7 | 2213 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 2214 | `				/* Recursively walk the array */` |
|        7 | 2215 | `				sData.nRecCount = 0;` |
|        7 | 2216 | `				sData.pArray = pArray;` |
|        7 | 2217 | `				sData.pCtx = pCtx;` |
|        7 | 2218 | `				sData.iArg = i + 1;` |
|        7 | 2219 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|        4 | 2220 | `			}else{` |
|       13 | 2221 | `				VmCompactBadName(pCtx,i + 1,apArg[i]);` |
|        - | 2222 | `			}` |
|       10 | 2223 | `		}else{` |
|        - | 2224 | `			/* Extract variable name. An EMPTY one is looked up like any other:` |
|        - | 2225 | `			 * php reports it as "Undefined variable $" rather than skipping it. */` |
|       21 | 2226 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|       21 | 2227 | `			SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 2228 | `			/* Check if the variable is available in the current frame */` |
|       21 | 2229 | `			pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|       21 | 2230 | `			if( pObj ){` |
|       17 | 2231 | `				ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        9 | 2232 | `			}else{` |
|        5 | 2233 | `				VmCompactUndefined(pCtx,&sVar);` |
|        - | 2234 | `			}` |
|        - | 2235 | `		}` |
|       20 | 2236 | `	}` |
|        - | 2237 | `	/* Return the array */` |
|       27 | 2238 | `	ph7_result_value(pCtx,pArray);` |
|       27 | 2239 | `	return PH7_OK;` |
|       14 | 2240 | `}` |
|        - | 2241 | `/*` |
|        - | 2242 | ` * The [import_request_variables()] function store it's state information` |
|        - | 2243 | ` * in an instance of the following structure.` |
|        - | 2244 | ` */` |
|        - | 2245 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 2246 | `struct extract_aux_data` |
|        - | 2247 | `{` |
|        - | 2248 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 2249 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 2250 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 2251 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 2252 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 2253 | `};` |
|        - | 2254 | `/*` |
|        - | 2255 | ` * php's php_valid_var_name(): a legal PHP variable name matches` |
|        - | 2256 | ` * [A-Za-z_\x80-\xff][A-Za-z0-9_\x80-\xff]* . Byte-wise and locale-free on` |
|        - | 2257 | ` * purpose (php has been locale-independent here since 8.0); the high-byte` |
|        - | 2258 | ` * range is what lets UTF-8 identifiers through. extract() drops every key` |
|        - | 2259 | ` * that does not pass, instead of installing an unreachable variable.` |
|        - | 2260 | ` */` |
|      158 | 2261 | `static int VmIsValidVarName(const char *zName,sxu32 nByte)` |
|        5 | 2262 | `{` |
|        - | 2263 | `	unsigned char c;` |
|        - | 2264 | `	sxu32 i;` |
|      163 | 2265 | `	if( nByte < 1 ){` |
|        7 | 2266 | `		return FALSE;` |
|        - | 2267 | `	}` |
|      157 | 2268 | `	c = (unsigned char)zName[0];` |
|      157 | 2269 | `	if( c != '_' && !(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') && c < 0x80 ){` |
|       11 | 2270 | `		return FALSE;` |
|        - | 2271 | `	}` |
|      389 | 2272 | `	for( i = 1 ; i < nByte ; ++i ){` |
|      263 | 2273 | `		c = (unsigned char)zName[i];` |
|      260 | 2274 | `		if( c != '_' && !(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z')` |
|       80 | 2275 | `		 && !(c >= '0' && c <= '9') && c < 0x80 ){` |
|       20 | 2276 | `			return FALSE;` |
|        - | 2277 | `		}` |
|      124 | 2278 | `	}` |
|      129 | 2279 | `	return TRUE;` |
|       84 | 2280 | `}` |
|        - | 2281 | `/* TRUE when the name is exactly "this": php refuses to re-assign $this. */` |
|      166 | 2282 | `static int VmExtractIsThis(const char *zName,sxu32 nByte)` |
|        5 | 2283 | `{` |
|      171 | 2284 | `	return nByte == sizeof("this")-1 && SyMemcmp(zName,"this",sizeof("this")-1) == 0;` |
|        5 | 2285 | `}` |
|        - | 2286 | `/*` |
|        - | 2287 | ` * TRUE when the name belongs to the superglobal table ($GLOBALS, $_SERVER,` |
|        - | 2288 | ` * $_GET, …). php hands extract() a per-frame symbol table that holds no` |
|        - | 2289 | ` * superglobal, so such a key lands in the LOCAL table and the real superglobal` |
|        - | 2290 | ` * is untouched. In PHL the name resolves to the superglobal SLOT itself` |
|        - | 2291 | ` * (VmExtractMemObj consults hSuper first), so a plain store would replace` |
|        - | 2292 | ` * $GLOBALS/$_SERVER with the imported value and take the whole symbol table /` |
|        - | 2293 | ` * request environment with it. Those keys are dropped instead — a prefixed` |
|        - | 2294 | ` * name ($p__SERVER) is a normal local and stores fine.` |
|        - | 2295 | ` */` |
|      114 | 2296 | `static int VmExtractIsProtected(ph7_vm *pVm,const char *zName,sxu32 nByte)` |
|        5 | 2297 | `{` |
|      119 | 2298 | `	return SyHashGet(&pVm->hSuper,(const void *)zName,nByte) != 0;` |
|        5 | 2299 | `}` |
|        - | 2300 | `/*` |
|        - | 2301 | ` * TRUE when the calling frame already holds this variable name.` |
|        - | 2302 | ` * "this" and the superglobals always answer FALSE, matching the symbol table` |
|        - | 2303 | ` * php hands extract(): $this is bound implicitly and superglobals live outside` |
|        - | 2304 | ` * the frame. So EXTR_IF_EXISTS/EXTR_PREFIX_IF_EXISTS skip those keys (php-exact)` |
|        - | 2305 | ` * and EXTR_PREFIX_SAME takes its not-a-collision branch.` |
|        - | 2306 | ` */` |
|       72 | 2307 | `static int VmExtractVarExists(ph7_vm *pVm,const char *zName,sxu32 nByte)` |
|        4 | 2308 | `{` |
|        - | 2309 | `	SyString sVar;` |
|       76 | 2310 | `	if( VmExtractIsThis(zName,nByte) \|\| VmExtractIsProtected(pVm,zName,nByte) ){` |
|       20 | 2311 | `		return FALSE;` |
|        - | 2312 | `	}` |
|       57 | 2313 | `	SyStringInitFromBuf(&sVar,zName,nByte);` |
|       57 | 2314 | `	return VmExtractMemObj(pVm,&sVar,FALSE,FALSE) != 0;` |
|       40 | 2315 | `}` |
|        - | 2316 | `/*` |
|        - | 2317 | ` * Create-or-overwrite a variable of the calling frame with a copy of pValue.` |
|        - | 2318 | ` * Returns TRUE when the variable was written (php counts exactly those).` |
|        - | 2319 | ` */` |
|        - | 2320 | `/*` |
|        - | 2321 | ` * EXTR_REFS: bind the imported NAME to the array element's own slot instead of copying` |
|        - | 2322 | ` * the value, so a later write through the variable reaches the caller's array — php's` |
|        - | 2323 | ` * by-reference extraction. The binding is registered (PH7_VmBindVarSlot), which is what` |
|        - | 2324 | ` * makes the element count the new name as a holder and read as a reference.` |
|        - | 2325 | ` *` |
|        - | 2326 | ` * The name is DUPLICATED: the symbol table keeps the pointer it is given, and the caller's` |
|        - | 2327 | ` * is a scratch blob the next entry reuses.` |
|        - | 2328 | ` */` |
|        8 | 2329 | `static int VmExtractBindVar(ph7_vm *pVm,const char *zName,sxu32 nByte,sxu32 nIdx)` |
|        1 | 2330 | `{` |
|        9 | 2331 | `	VmFrame *pFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|        - | 2332 | `	char *zDup;` |
|        9 | 2333 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 | 2334 | `		return FALSE;` |
|        - | 2335 | `	}` |
|        9 | 2336 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zName,nByte);` |
|        9 | 2337 | `	if( zDup == 0 ){` |
|      ! 0 | 2338 | `		return FALSE;` |
|        - | 2339 | `	}` |
|        9 | 2340 | `	PH7_VmBindVarSlot(&(*pVm),pFrame,zDup,nByte,nIdx);` |
|        9 | 2341 | `	return TRUE;` |
|        5 | 2342 | `}` |
|       62 | 2343 | `static int VmExtractStoreVar(ph7_vm *pVm,const char *zName,sxu32 nByte,ph7_value *pValue)` |
|        4 | 2344 | `{` |
|        - | 2345 | `	ph7_value *pObj;` |
|        - | 2346 | `	SyString sVar;` |
|       66 | 2347 | `	SyStringInitFromBuf(&sVar,zName,nByte);` |
|        - | 2348 | `	/* bDup: the name lives in a scratch blob that is reused by the next entry */` |
|       66 | 2349 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|       66 | 2350 | `	if( pObj == 0 ){` |
|      ! 0 | 2351 | `		return FALSE;` |
|        - | 2352 | `	}` |
|       66 | 2353 | `	PH7_MemObjStore(pValue,pObj);` |
|       66 | 2354 | `	return TRUE;` |
|       35 | 2355 | `}` |
|        - | 2356 | `/*` |
|        - | 2357 | ` * Build php's prefixed name "<prefix>_<key>" (php_prefix_varname with` |
|        - | 2358 | ` * add_underscore): the separator is unconditional, so an empty prefix still` |
|        - | 2359 | ` * yields "_key" exactly like php.` |
|        - | 2360 | ` */` |
|       40 | 2361 | `static sxi32 VmExtractPrefixName(SyBlob *pOut,const char *zPrefix,int nPrefix,` |
|        - | 2362 | `	const char *zKey,sxu32 nKey)` |
|        3 | 2363 | `{` |
|       43 | 2364 | `	SyBlobReset(pOut);` |
|       43 | 2365 | `	if( nPrefix > 0 && SyBlobAppend(pOut,zPrefix,(sxu32)nPrefix) != SXRET_OK ){` |
|      ! 0 | 2366 | `		return SXERR_MEM;` |
|        - | 2367 | `	}` |
|       43 | 2368 | `	if( SyBlobAppend(pOut,"_",sizeof(char)) != SXRET_OK ){` |
|      ! 0 | 2369 | `		return SXERR_MEM;` |
|        - | 2370 | `	}` |
|       43 | 2371 | `	if( nKey > 0 && SyBlobAppend(pOut,zKey,nKey) != SXRET_OK ){` |
|      ! 0 | 2372 | `		return SXERR_MEM;` |
|        - | 2373 | `	}` |
|       43 | 2374 | `	return SXRET_OK;` |
|       23 | 2375 | `}` |
|        - | 2376 | `/* What to do with one array entry, decided by the extract mode. */` |
|        - | 2377 | `#define VM_EXTRACT_DROP     0 /* php skips this key entirely */` |
|        - | 2378 | `#define VM_EXTRACT_PLAIN    1 /* install under the key itself */` |
|        - | 2379 | `#define VM_EXTRACT_PREFIX   2 /* install under "<prefix>_<key>" */` |
|        - | 2380 | `/*` |
|        - | 2381 | ` * int extract(array &$array[,int $flags = EXTR_OVERWRITE[,string $prefix = "" ]])` |
|        - | 2382 | ` *   Import variables into the current symbol table from an array.` |
|        - | 2383 | ` *` |
|        - | 2384 | ` * $flags is php's ENUM (PH7_EXTR_* in ph7int.h), not a bitmask: the mode is` |
|        - | 2385 | ` * (flags & 0xff) and anything above EXTR_IF_EXISTS(6) is a ValueError. The` |
|        - | 2386 | ` * modes, mirroring php's per-mode helpers in ext/standard/array.c:` |
|        - | 2387 | ` *   EXTR_OVERWRITE(0)        collisions overwrite` |
|        - | 2388 | ` *   EXTR_SKIP(1)             collisions keep the existing variable` |
|        - | 2389 | ` *   EXTR_PREFIX_SAME(2)      collisions install "<prefix>_<key>"` |
|        - | 2390 | ` *   EXTR_PREFIX_ALL(3)       every key installs as "<prefix>_<key>"` |
|        - | 2391 | ` *   EXTR_PREFIX_INVALID(4)   only illegal names (and numeric keys) get prefixed` |
|        - | 2392 | ` *   EXTR_PREFIX_IF_EXISTS(5) install "<prefix>_<key>" only if <key> exists` |
|        - | 2393 | ` *   EXTR_IF_EXISTS(6)        overwrite only variables that already exist` |
|        - | 2394 | `` * Modes 2..5 require $prefix (php: `is required when using this extract type`),`` |
|        - | 2395 | ` * a non-empty $prefix must itself be a legal identifier, and a key whose final` |
|        - | 2396 | ` * name is not a legal variable name is dropped rather than installed. Only` |
|        - | 2397 | ` * EXTR_PREFIX_ALL/EXTR_PREFIX_INVALID look at numeric keys at all.` |
|        - | 2398 | ` *` |
|        - | 2399 | `` * $this is never a target: php throws `Cannot re-assign $this` where a store`` |
|        - | 2400 | ` * would land on it, and skips it where a store would not (EXTR_SKIP), and` |
|        - | 2401 | ` * $GLOBALS is never clobbered.` |
|        - | 2402 | ` * Return` |
|        - | 2403 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 2404 | ` */` |
|      102 | 2405 | `PH7_PRIVATE int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 2406 | `{` |
|      107 | 2407 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 2408 | `	ph7_hashmap_node *pEntry;` |
|        - | 2409 | `	ph7_hashmap *pMap;` |
|      107 | 2410 | `	const char *zPrefix = 0;` |
|      107 | 2411 | `	sxi64 iFlags = PH7_EXTR_OVERWRITE;` |
|      107 | 2412 | `	sxi64 iCount = 0;` |
|        - | 2413 | `	ph7_value sValue;` |
|        - | 2414 | `	SyBlob sWorker;` |
|      107 | 2415 | `	int nPrefix = 0;` |
|      107 | 2416 | `	sxi32 rc = PH7_OK;` |
|        - | 2417 | `	int iType;` |
|        - | 2418 | `	sxu32 n;` |
|      107 | 2419 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|        - | 2420 | `		char zBuf[64];` |
|      ! 0 | 2421 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|        - | 2422 | `			"extract(): Argument #1 ($array) must be of type array, %s given",` |
|      ! 0 | 2423 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|        - | 2424 | `	}` |
|      107 | 2425 | `	if( nArg > 1 ){` |
|       98 | 2426 | `		rc = PH7_IntArgResolve(pCtx,apArg[1],"extract",2,"$flags","int",&iFlags);` |
|       98 | 2427 | `		if( rc != PH7_OK ){` |
|      ! 0 | 2428 | `			return rc;` |
|        - | 2429 | `		}` |
|       47 | 2430 | `	}` |
|        - | 2431 | `	/* php: the mode is the low byte; EXTR_REFS(0x100) rides above it */` |
|      107 | 2432 | `	iType = (int)(iFlags & 0xff);` |
|      107 | 2433 | `	if( iType > PH7_EXTR_IF_EXISTS ){` |
|        7 | 2434 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|        - | 2435 | `			"extract(): Argument #2 ($flags) must be a valid extract type");` |
|        - | 2436 | `	}` |
|      101 | 2437 | `	if( iType > PH7_EXTR_SKIP && iType <= PH7_EXTR_PREFIX_IF_EXISTS && nArg < 3 ){` |
|       12 | 2438 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|        - | 2439 | `			"extract(): Argument #3 ($prefix) is required when using this extract type");` |
|        - | 2440 | `	}` |
|       91 | 2441 | `	if( nArg > 2 ){` |
|       41 | 2442 | `		zPrefix = ph7_value_to_string(apArg[2],&nPrefix);` |
|       41 | 2443 | `		if( nPrefix > 0 && !VmIsValidVarName(zPrefix,(sxu32)nPrefix) ){` |
|        5 | 2444 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|        - | 2445 | `				"extract(): Argument #3 ($prefix) must be a valid identifier");` |
|        - | 2446 | `		}` |
|       17 | 2447 | `	}` |
|        - | 2448 | `	/* Point to the target hashmap */` |
|       87 | 2449 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       87 | 2450 | `	if( pMap->nEntry < 1 ){` |
|        - | 2451 | `		/* Empty map,return  0 */` |
|      ! 0 | 2452 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 2453 | `		return PH7_OK;` |
|        - | 2454 | `	}` |
|       87 | 2455 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|       87 | 2456 | `	PH7_MemObjInit(pVm,&sValue);` |
|        - | 2457 | `	/* php walks a COPY of the array, so an entry that overwrites the caller's own` |
|        - | 2458 | `	 * array variable ($arr = ['arr'=>1]; extract($arr)) cannot pull the map from` |
|        - | 2459 | `	 * under the walk. Pinning the map for the walk is the same guarantee. */` |
|       87 | 2460 | `	pMap->iRef++;` |
|       87 | 2461 | `	pEntry = pMap->pFirst;` |
|        - | 2462 | `	/* pFirst walks the insertion order through pPrev — PH7 links the entry list` |
|        - | 2463 | `	 * in reverse (same traversal PH7_HashmapWalk uses). */` |
|      253 | 2464 | `	for( n = pMap->nEntry ; n > 0 && pEntry ; --n, pEntry = pEntry->pPrev ){` |
|        - | 2465 | `		const char *zKey, *zFinal;` |
|        - | 2466 | `		sxu32 nKey, nFinal;` |
|        - | 2467 | `		char zNum[32];` |
|        - | 2468 | `		int bIntKey, iAction;` |
|        - | 2469 | `		/* Work off a COPY of the entry value: installing a variable can grow` |
|        - | 2470 | `		 * pVm->aMemObj, and a pointer into that set would dangle across the` |
|        - | 2471 | `		 * reallocation (this is why the walk API hands out copies too). The` |
|        - | 2472 | `		 * release comes FIRST so it covers every continue/goto below: the load` |
|        - | 2473 | `		 * takes a reference on an array/object value and does not drop the one` |
|        - | 2474 | `		 * the previous entry left behind (PH7_HashmapWalk releases per iteration` |
|        - | 2475 | `		 * for the same reason — without it a whole-array extract() pins every` |
|        - | 2476 | `		 * value it copied, and their destructors never run). */` |
|      173 | 2477 | `		PH7_MemObjRelease(&sValue);` |
|      173 | 2478 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|      173 | 2479 | `		bIntKey = (pEntry->iType == HASHMAP_INT_NODE);` |
|      173 | 2480 | `		if( bIntKey ){` |
|        - | 2481 | `			/* Only the two prefixing modes below ever look at a numeric key */` |
|       22 | 2482 | `			nKey = SyBufferFormat(zNum,sizeof(zNum),"%qd",pEntry->xKey.iKey);` |
|       22 | 2483 | `			zKey = zNum;` |
|       12 | 2484 | `		}else{` |
|      153 | 2485 | `			zKey = (const char *)SyBlobData(&pEntry->xKey.sKey);` |
|      153 | 2486 | `			nKey = SyBlobLength(&pEntry->xKey.sKey);` |
|        - | 2487 | `		}` |
|      173 | 2488 | `		iAction = VM_EXTRACT_DROP;` |
|      173 | 2489 | `		switch( iType ){` |
|       19 | 2490 | `		case PH7_EXTR_OVERWRITE:` |
|       43 | 2491 | `			if( bIntKey \|\| !VmIsValidVarName(zKey,nKey) ){` |
|        8 | 2492 | `				break;` |
|        - | 2493 | `			}` |
|       31 | 2494 | `			if( VmExtractIsThis(zKey,nKey) ){` |
|        3 | 2495 | `				goto this_error;` |
|        - | 2496 | `			}` |
|       29 | 2497 | `			iAction = VM_EXTRACT_PLAIN;` |
|       29 | 2498 | `			break;` |
|       12 | 2499 | `		case PH7_EXTR_SKIP:` |
|       27 | 2500 | `			if( bIntKey \|\| !VmIsValidVarName(zKey,nKey) \|\| VmExtractIsThis(zKey,nKey) ){` |
|        6 | 2501 | `				break;` |
|        - | 2502 | `			}` |
|       17 | 2503 | `			if( VmExtractVarExists(pVm,zKey,nKey) ){` |
|        8 | 2504 | `				break; /* collision: keep the existing variable */` |
|        - | 2505 | `			}` |
|       10 | 2506 | `			iAction = VM_EXTRACT_PLAIN;` |
|       10 | 2507 | `			break;` |
|       16 | 2508 | `		case PH7_EXTR_IF_EXISTS:` |
|       36 | 2509 | `			if( bIntKey \|\| !VmExtractVarExists(pVm,zKey,nKey) ){` |
|       16 | 2510 | `				break;` |
|        - | 2511 | `			}` |
|        8 | 2512 | `			if( !VmIsValidVarName(zKey,nKey) ){` |
|      ! 0 | 2513 | `				break;` |
|        - | 2514 | `			}` |
|        8 | 2515 | `			if( VmExtractIsThis(zKey,nKey) ){` |
|      ! 0 | 2516 | `				goto this_error;` |
|        - | 2517 | `			}` |
|        8 | 2518 | `			iAction = VM_EXTRACT_PLAIN;` |
|        8 | 2519 | `			break;` |
|        9 | 2520 | `		case PH7_EXTR_PREFIX_SAME:` |
|       21 | 2521 | `			if( bIntKey \|\| nKey < 1 ){` |
|        3 | 2522 | `				break;` |
|        - | 2523 | `			}` |
|       17 | 2524 | `			if( VmExtractVarExists(pVm,zKey,nKey) ){` |
|        6 | 2525 | `				iAction = VM_EXTRACT_PREFIX; /* collision */` |
|       14 | 2526 | `			}else if( !VmIsValidVarName(zKey,nKey) ){` |
|        5 | 2527 | `				break;` |
|      ! 0 | 2528 | `			}else{` |
|        - | 2529 | `				/* $this cannot be a target, but its prefixed form can */` |
|        8 | 2530 | `				iAction = VmExtractIsThis(zKey,nKey) ? VM_EXTRACT_PREFIX : VM_EXTRACT_PLAIN;` |
|        - | 2531 | `			}` |
|       13 | 2532 | `			break;` |
|       13 | 2533 | `		case PH7_EXTR_PREFIX_ALL:` |
|       28 | 2534 | `			if( !bIntKey && nKey < 1 ){` |
|        3 | 2535 | `				break;` |
|        - | 2536 | `			}` |
|       26 | 2537 | `			iAction = VM_EXTRACT_PREFIX;` |
|       26 | 2538 | `			break;` |
|        7 | 2539 | `		case PH7_EXTR_PREFIX_INVALID:` |
|       15 | 2540 | `			iAction = (bIntKey \|\| !VmIsValidVarName(zKey,nKey) \|\| VmExtractIsThis(zKey,nKey))` |
|       13 | 2541 | `				? VM_EXTRACT_PREFIX : VM_EXTRACT_PLAIN;` |
|       16 | 2542 | `			break;` |
|        8 | 2543 | `		case PH7_EXTR_PREFIX_IF_EXISTS:` |
|       18 | 2544 | `			if( !bIntKey && VmExtractVarExists(pVm,zKey,nKey) ){` |
|        3 | 2545 | `				iAction = VM_EXTRACT_PREFIX;` |
|        1 | 2546 | `			}` |
|       16 | 2547 | `			break;` |
|      ! 0 | 2548 | `		default:` |
|      ! 0 | 2549 | `			break;` |
|        - | 2550 | `		}` |
|      171 | 2551 | `		if( iAction == VM_EXTRACT_DROP ){` |
|      126 | 2552 | `			continue;` |
|        - | 2553 | `		}` |
|       93 | 2554 | `		if( iAction == VM_EXTRACT_PREFIX ){` |
|       43 | 2555 | `			if( VmExtractPrefixName(&sWorker,zPrefix,nPrefix,zKey,nKey) != SXRET_OK ){` |
|      ! 0 | 2556 | `				rc = PH7_VmThrowException(pCtx,"Error","PH7 engine is running out of memory");` |
|      ! 0 | 2557 | `				goto done;` |
|        - | 2558 | `			}` |
|       43 | 2559 | `			zFinal = (const char *)SyBlobData(&sWorker);` |
|       43 | 2560 | `			nFinal = SyBlobLength(&sWorker);` |
|       43 | 2561 | `			if( !VmIsValidVarName(zFinal,nFinal) ){` |
|        7 | 2562 | `				continue; /* php drops a prefixed name that is not an identifier */` |
|        - | 2563 | `			}` |
|       37 | 2564 | `			if( VmExtractIsThis(zFinal,nFinal) ){` |
|      ! 0 | 2565 | `				goto this_error;` |
|        - | 2566 | `			}` |
|       20 | 2567 | `		}else{` |
|       53 | 2568 | `			if( VmExtractIsProtected(pVm,zKey,nKey) ){` |
|       13 | 2569 | `				continue; /* $GLOBALS/$_SERVER/... are never a plain target */` |
|        - | 2570 | `			}` |
|       40 | 2571 | `			zFinal = zKey;` |
|       40 | 2572 | `			nFinal = nKey;` |
|        - | 2573 | `		}` |
|       75 | 2574 | `		if( iFlags & PH7_EXTR_REFS ){` |
|        - | 2575 | `			/* Bind to the element's own slot (php's EXTR_REFS) rather than copying */` |
|        9 | 2576 | `			if( VmExtractBindVar(pVm,zFinal,nFinal,pEntry->nValIdx) ){` |
|        9 | 2577 | `				iCount++;` |
|        5 | 2578 | `			}` |
|       70 | 2579 | `		}else if( VmExtractStoreVar(pVm,zFinal,nFinal,&sValue) ){` |
|       66 | 2580 | `			iCount++;` |
|       31 | 2581 | `		}` |
|       75 | 2582 | `		continue;` |
|        1 | 2583 | `this_error:` |
|        3 | 2584 | `		rc = PH7_VmThrowException(pCtx,"Error","Cannot re-assign $this");` |
|        3 | 2585 | `		goto done;` |
|      ! 0 | 2586 | `	}` |
|        - | 2587 | `	/* Number of variables successfully imported */` |
|       85 | 2588 | `	ph7_result_int64(pCtx,iCount);` |
|       41 | 2589 | `done:` |
|       87 | 2590 | `	PH7_MemObjRelease(&sValue);` |
|       87 | 2591 | `	SyBlobRelease(&sWorker);` |
|       87 | 2592 | `	PH7_HashmapUnref(pMap);` |
|       87 | 2593 | `	return rc;` |
|       56 | 2594 | `}` |
|        - | 2595 | `/*` |
|        - | 2596 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 2597 | ` * defined below.` |
|        - | 2598 | ` */` |
|        2 | 2599 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 2600 | `{` |
|        3 | 2601 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 2602 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 2603 | `	ph7_value *pObj;` |
|        - | 2604 | `	SyString sVar;` |
|        - | 2605 | `	/* Perform a string cast */` |
|        3 | 2606 | `	PH7_MemObjToString(pKey);` |
|        3 | 2607 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 2608 | `		/* Unavailable variable name */` |
|      ! 0 | 2609 | `		return SXRET_OK;` |
|        - | 2610 | `	}` |
|        3 | 2611 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 2612 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 2613 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 2614 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 2615 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 2616 | `			);` |
|        2 | 2617 | `	}else{` |
|      ! 0 | 2618 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 2619 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 2620 | `	}` |
|        3 | 2621 | `	sVar.zString = pAux->zWorker;` |
|        - | 2622 | `	/* Extract the variable */` |
|        3 | 2623 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 2624 | `	if( pObj ){` |
|        3 | 2625 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 2626 | `	}` |
|        3 | 2627 | `	return SXRET_OK;` |
|        2 | 2628 | `}` |
|        - | 2629 | `/*` |
|        - | 2630 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 2631 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 2632 | ` * Parameters` |
|        - | 2633 | ` * $types` |
|        - | 2634 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 2635 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 2636 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 2637 | ` *  POST includes the POST uploaded file information.` |
|        - | 2638 | ` *  Note:` |
|        - | 2639 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 2640 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 2641 | ` * $prefix` |
|        - | 2642 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 2643 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 2644 | ` *  variable named $pref_userid.` |
|        - | 2645 | ` * Return` |
|        - | 2646 | ` *  TRUE on success or FALSE on failure.` |
|        - | 2647 | ` */` |
|        2 | 2648 | `PH7_PRIVATE int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 2649 | `{` |
|        - | 2650 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 2651 | `	extract_aux_data sAux;` |
|        - | 2652 | `	int nLen,nPrefixLen;` |
|        - | 2653 | `	ph7_value *pSuper;` |
|        - | 2654 | `	ph7_vm *pVm;` |
|        - | 2655 | `	/* By default import only $_GET variables  */` |
|        3 | 2656 | `	zImport = "G";` |
|        3 | 2657 | `	nLen = (int)sizeof(char);` |
|        3 | 2658 | `	zPrefix = 0;` |
|        3 | 2659 | `	nPrefixLen = 0;` |
|        3 | 2660 | `	if( nArg > 0 ){` |
|        3 | 2661 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 2662 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 2663 | `		}` |
|        3 | 2664 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 2665 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 2666 | `		}` |
|        1 | 2667 | `	}` |
|        - | 2668 | `	/* Point to the underlying VM */` |
|        3 | 2669 | `	pVm = pCtx->pVm;` |
|        - | 2670 | `	/* Initialize the aux data */` |
|        3 | 2671 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 2672 | `	sAux.zPrefix = zPrefix;` |
|        3 | 2673 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 2674 | `	sAux.pVm = pVm;` |
|        - | 2675 | `	/* Extract */` |
|        3 | 2676 | `	zEnd = &zImport[nLen];` |
|        5 | 2677 | `	while( zImport < zEnd ){` |
|        3 | 2678 | `		int c = zImport[0];` |
|        3 | 2679 | `		pSuper = 0;` |
|        3 | 2680 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 2681 | `			/* Import $_GET variables */` |
|        3 | 2682 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 2683 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 2684 | `			/* Import $_POST variables */` |
|      ! 0 | 2685 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 2686 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 2687 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 2688 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 2689 | `		}` |
|        3 | 2690 | `		if( pSuper ){` |
|        - | 2691 | `			/* Iterate throw array entries */` |
|        3 | 2692 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 2693 | `		}` |
|        - | 2694 | `		/* Advance the cursor */` |
|        3 | 2695 | `		zImport++;` |
|        1 | 2696 | `	}` |
|        - | 2697 | `	/* All done,return TRUE*/` |
|        3 | 2698 | `	ph7_result_bool(pCtx,0);` |
|        3 | 2699 | `	return PH7_OK;` |
|        1 | 2700 | `}` |
|        - | 2701 |  |
