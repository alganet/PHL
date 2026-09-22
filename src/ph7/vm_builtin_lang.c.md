# src/ph7/vm_builtin_lang.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1017/1233 lines (82.48%)

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
|      142 |   53 | `			break;` |
|        - |   54 | `		}` |
|     1023 |   55 | `	}` |
|      263 |   56 | `	if( iSep + 1 >= nLen ){` |
|      125 |   57 | `		return VM_CCONST_PLAIN;` |
|        - |   58 | `	}` |
|      142 |   59 | `	*pSep = iSep;` |
|      142 |   60 | `	pClass = iSep > 0 ? PH7_VmResolveScopeName(&(*pVm),zName,(sxu32)iSep) : 0;` |
|      142 |   61 | `	if( pClass == 0 ){` |
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
|      113 |   74 | `	*ppClass = pClass;` |
|      113 |   75 | `	if( iSep + 2 >= nLen ){` |
|        6 |   76 | `		return VM_CCONST_NOCONST; /* "C::" names no constant */` |
|        - |   77 | `	}` |
|        - |   78 | `	/* This form names a class CONSTANT or an enum case (hConst), never a property. */` |
|      109 |   79 | `	pAttr = PH7_ClassExtractConstant(pClass,&zName[iSep+2],(sxu32)(nLen - iSep - 2));` |
|      109 |   80 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       17 |   81 | `		return VM_CCONST_NOCONST;` |
|        - |   82 | `	}` |
|       92 |   83 | `	if( pAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|       62 |   84 | `	 && pAttr->pDeclClass && pAttr->pDeclClass != pClass ){` |
|        - |   85 | `		/* php does not put a private constant in a SUBCLASS's table at all, so naming it` |
|        - |   86 | ``		 * through the child is not an access denial but a plain miss — `Sub::P` reports`` |
|        - |   87 | ``		 * `Undefined constant Sub::P` even from inside the declaring class, and`` |
|        - |   88 | `		 * defined("Sub::P") is false there too. Only the declaring class can answer. */` |
|        7 |   89 | `		return VM_CCONST_NOCONST;` |
|        - |   90 | `	}` |
|       89 |   91 | `	*ppAttr = pAttr;` |
|        - |   92 | ``	/* php answers by the CALLING scope, the same rule the direct `C::K` access uses:`` |
|        - |   93 | `	 * a private constant is invisible from outside its declaring class even to a` |
|        - |   94 | `	 * subclass, and a protected one is visible down the hierarchy. */` |
|       89 |   95 | `	if( !PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|       20 |   96 | `		return VM_CCONST_NOACCESS;` |
|        - |   97 | `	}` |
|       71 |   98 | `	return VM_CCONST_OK;` |
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
|       35 |  191 | `		res = 1;` |
|       15 |  192 | `	}` |
|       43 |  193 | `	ph7_result_bool(pCtx,res);` |
|       43 |  194 | `	return SXRET_OK;` |
|       59 |  195 | `}` |
|        - |  196 | `/*` |
|        - |  197 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  198 | ` * below.` |
|        - |  199 | ` */` |
|       46 |  200 | `PH7_PRIVATE void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        5 |  201 | `{` |
|       51 |  202 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  203 | `	/* Expand constant value */` |
|       51 |  204 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       51 |  205 | `}` |
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
|       44 |  217 | `PH7_PRIVATE int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  218 | `{` |
|        - |  219 | `	const char *zName;  /* Constant name */` |
|        - |  220 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       49 |  221 | `	int nLen = 0;       /* Name length */` |
|        - |  222 | `	sxi32 rc;` |
|       49 |  223 | `	if( nArg < 2 ){` |
|        - |  224 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  225 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  226 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  227 | `		return SXRET_OK;` |
|        - |  228 | `	}` |
|       49 |  229 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  230 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  231 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  232 | `		return SXRET_OK;` |
|        - |  233 | `	}` |
|        - |  234 | `	/* Extract constant name */` |
|       49 |  235 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       49 |  236 | `	if( nLen < 1 ){` |
|      ! 0 |  237 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  238 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  239 | `		return SXRET_OK;` |
|        - |  240 | `	}` |
|        - |  241 | `	/* Duplicate constant value */` |
|       49 |  242 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       49 |  243 | `	if( pValue == 0 ){` |
|      ! 0 |  244 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  245 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  246 | `		return SXRET_OK;` |
|        - |  247 | `	}` |
|        - |  248 | `	/* Initialize the memory object */` |
|       49 |  249 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  250 | `	/* Register the constant */` |
|        - |  251 | `	{` |
|        - |  252 | `		SyString sConsName;` |
|       49 |  253 | `		SyStringInitFromBuf(&sConsName,zName,(sxu32)nLen);` |
|       71 |  254 | `		rc = PH7_VmRegisterConstantEx(pCtx->pVm,&sConsName,VmExpandUserConstant,pValue,` |
|       44 |  255 | `			(SyString *)SySetPeek(&pCtx->pVm->aFiles),0,1);` |
|        - |  256 | `	}` |
|       49 |  257 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  258 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  259 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  260 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  261 | `		return SXRET_OK;` |
|        - |  262 | `	}` |
|        - |  263 | `	/* Duplicate constant value */` |
|       49 |  264 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       49 |  265 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
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
|       49 |  296 | `	ph7_result_bool(pCtx,1);` |
|       49 |  297 | `	return SXRET_OK;` |
|       27 |  298 | `}` |
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
|        8 |  315 | `PH7_PRIVATE int vm_builtin_enum_cases(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  316 | `{` |
|        9 |  317 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  318 | `	ph7_class_attr **apCase;` |
|        - |  319 | `	ph7_class *pClass;` |
|        - |  320 | `	ph7_value *pArray;` |
|        - |  321 | `	sxu32 n;` |
|        - |  322 | `	sxi32 rc;` |
|        9 |  323 | `	if( nArg < 1 \|\| (pClass = VmExtractEnumClass(pVm,apArg[0])) == 0 ){` |
|      ! 0 |  324 | `		ph7_result_null(pCtx);` |
|      ! 0 |  325 | `		return SXRET_OK;` |
|        - |  326 | `	}` |
|        9 |  327 | `	rc = VmEnumMaterialize(pVm,pClass);` |
|        9 |  328 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  329 | `		return rc;` |
|        - |  330 | `	}` |
|        9 |  331 | `	pArray = ph7_context_new_array(pCtx);` |
|        9 |  332 | `	if( pArray == 0 ){` |
|      ! 0 |  333 | `		ph7_result_null(pCtx);` |
|      ! 0 |  334 | `		return SXRET_OK;` |
|        - |  335 | `	}` |
|        9 |  336 | `	apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|       25 |  337 | `	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|       17 |  338 | `		ph7_value *pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,apCase[n]->nIdx);` |
|       17 |  339 | `		if( pSlot ){` |
|       17 |  340 | `			ph7_array_add_elem(pArray,0,pSlot); /* Copies; the object ref is retained */` |
|        8 |  341 | `		}` |
|        9 |  342 | `	}` |
|        9 |  343 | `	ph7_result_value(pCtx,pArray);` |
|        9 |  344 | `	return SXRET_OK;` |
|        5 |  345 | `}` |
|        - |  346 | `/* Shared scan for from()/tryFrom(): return the slot of the case whose backing` |
|        - |  347 | ` * value equals *pNeedle (already coerced to the backing type by the synthesized` |
|        - |  348 | ` * method's signature), or 0 on miss. */` |
|       24 |  349 | `static ph7_value * VmEnumFindCaseByValue(ph7_vm *pVm,ph7_class *pClass,ph7_value *pNeedle)` |
|        2 |  350 | `{` |
|       26 |  351 | `	ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|        - |  352 | `	sxu32 n;` |
|       54 |  353 | `	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|       46 |  354 | `		ph7_value *pVal = VmEnumCaseBackingValue(pVm,apCase[n]);` |
|       46 |  355 | `		int bMatch = 0;` |
|       46 |  356 | `		if( pVal ){` |
|       46 |  357 | `			if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|        3 |  358 | `				bMatch = (pNeedle->iFlags & MEMOBJ_INT) && pVal->x.iVal == pNeedle->x.iVal;` |
|        2 |  359 | `			}else{` |
|       65 |  360 | `				bMatch = (pNeedle->iFlags & MEMOBJ_STRING)` |
|       42 |  361 | `					&& SyBlobLength(&pVal->sBlob) == SyBlobLength(&pNeedle->sBlob)` |
|       63 |  362 | `					&& SyMemcmp(SyBlobData(&pVal->sBlob),SyBlobData(&pNeedle->sBlob),` |
|       30 |  363 | `						SyBlobLength(&pNeedle->sBlob)) == 0;` |
|        - |  364 | `			}` |
|       22 |  365 | `		}` |
|       46 |  366 | `		if( bMatch ){` |
|       18 |  367 | `			return (ph7_value *)SySetAt(&pVm->aMemObj,apCase[n]->nIdx);` |
|        - |  368 | `		}` |
|       15 |  369 | `	}` |
|        9 |  370 | `	return 0;` |
|       14 |  371 | `}` |
|        - |  372 | `/* static from(int\|string $value) / static tryFrom(int\|string $value) */` |
|       24 |  373 | `static int VmEnumFromCommon(ph7_context *pCtx,int nArg,ph7_value **apArg,int bTry)` |
|        2 |  374 | `{` |
|       26 |  375 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  376 | `	ph7_class *pClass;` |
|        - |  377 | `	ph7_value *pFound;` |
|        - |  378 | `	sxi32 rc;` |
|       26 |  379 | `	if( nArg < 2 \|\| (pClass = VmExtractEnumClass(pVm,apArg[0])) == 0 ){` |
|      ! 0 |  380 | `		ph7_result_null(pCtx);` |
|      ! 0 |  381 | `		return SXRET_OK;` |
|        - |  382 | `	}` |
|       26 |  383 | `	rc = VmEnumMaterialize(pVm,pClass);` |
|       26 |  384 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  385 | `		return rc;` |
|        - |  386 | `	}` |
|       26 |  387 | `	pFound = VmEnumFindCaseByValue(pVm,pClass,apArg[1]);` |
|       26 |  388 | `	if( pFound ){` |
|       18 |  389 | `		ph7_result_value(pCtx,pFound);` |
|       18 |  390 | `		return SXRET_OK;` |
|        - |  391 | `	}` |
|        9 |  392 | `	if( bTry ){` |
|        7 |  393 | `		ph7_result_null(pCtx);` |
|        7 |  394 | `		return SXRET_OK;` |
|        - |  395 | `	}` |
|        3 |  396 | `	if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|        - |  397 | `		char zVal[32];` |
|      ! 0 |  398 | `		SyBufferFormat(zVal,sizeof(zVal),"%qd",ph7_value_to_int64(apArg[1]));` |
|      ! 0 |  399 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      ! 0 |  400 | `			"%s is not a valid backing value for enum %z",zVal,&pClass->sName);` |
|        - |  401 | `	}` |
|        4 |  402 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|        - |  403 | `		"\"%.*s\" is not a valid backing value for enum %z",` |
|        2 |  404 | `		(int)SyBlobLength(&apArg[1]->sBlob),(const char *)SyBlobData(&apArg[1]->sBlob),` |
|        1 |  405 | `		&pClass->sName);` |
|       14 |  406 | `}` |
|       18 |  407 | `PH7_PRIVATE int vm_builtin_enum_from(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  408 | `{` |
|       20 |  409 | `	return VmEnumFromCommon(pCtx,nArg,apArg,FALSE);` |
|        2 |  410 | `}` |
|        6 |  411 | `PH7_PRIVATE int vm_builtin_enum_tryfrom(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  412 | `{` |
|        7 |  413 | `	return VmEnumFromCommon(pCtx,nArg,apArg,TRUE);` |
|        1 |  414 | `}` |
|        - |  415 | `/*` |
|        - |  416 | ` * bool enum_exists(string $enum, bool $autoload = true)` |
|        - |  417 | ` *  TRUE only for a declared enum (PHP 8.1); a plain class/interface is FALSE.` |
|        - |  418 | ` */` |
|        8 |  419 | `PH7_PRIVATE int vm_builtin_enum_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  420 | `{` |
|       10 |  421 | `	ph7_class *pClass = 0;` |
|       10 |  422 | `	if( nArg > 0 ){` |
|       10 |  423 | `		pClass = VmExtractEnumClass(pCtx->pVm,apArg[0]);` |
|        4 |  424 | `	}` |
|       10 |  425 | `	ph7_result_bool(pCtx,pClass != 0);` |
|       10 |  426 | `	return SXRET_OK;` |
|        2 |  427 | `}` |
|      150 |  428 | `PH7_PRIVATE int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 |  429 | `{` |
|        - |  430 | `	SyHashEntry *pEntry;` |
|        - |  431 | `	ph7_constant *pCons;` |
|        - |  432 | `	const char *zName; /* Constant name */` |
|        - |  433 | `	ph7_value sVal;    /* Constant value */` |
|        - |  434 | `	int nLen;` |
|      154 |  435 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  436 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  437 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  438 | `		ph7_result_null(pCtx);` |
|      ! 0 |  439 | `		return SXRET_OK;` |
|        - |  440 | `	}` |
|        - |  441 | `	/* Extract the constant name */` |
|      154 |  442 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  443 | `	/* Class-constant form "C::K" (band A #4): resolve the class — interfaces` |
|        - |  444 | `	 * included — and read the mounted constant slot; php throws a catchable` |
|        - |  445 | `	 * Error for an unknown class or constant (pre-fix this path warned` |
|        - |  446 | `	 * "Undefined constant" and returned NULL without ever looking at the` |
|        - |  447 | `` 	 * class). The resolution is defined()'s: it also answers `self`/`parent`/`static` `` |
|        - |  448 | `	 * against the live class scope and refuses a constant that is not VISIBLE from` |
|        - |  449 | ``	 * here — both of which this used to walk straight past, so a `private const` was`` |
|        - |  450 | ``	 * readable from anywhere through the string form while the direct `C::K` access`` |
|        - |  451 | `	 * threw. */` |
|        - |  452 | `	{` |
|      154 |  453 | `		ph7_class_attr *pAttr = 0;` |
|      154 |  454 | `		ph7_class *pClass = 0;` |
|      154 |  455 | `		int iSep = 0;` |
|      154 |  456 | `		int iRc = VmClassConstLookup(pCtx->pVm,zName,nLen,&pClass,&pAttr,&iSep);` |
|      154 |  457 | `		if( iRc != VM_CCONST_PLAIN ){` |
|       71 |  458 | `			if( iRc != VM_CCONST_OK ){` |
|       54 |  459 | `				return VmClassConstError(pCtx,iRc,zName,nLen,iSep,pAttr);` |
|        - |  460 | `			}` |
|       35 |  461 | `			if( pAttr->nIdx == SXU32_HIGH ){` |
|        - |  462 | `				/* Unmaterialized: enum case → materialize the singletons` |
|        - |  463 | `				 * (all of them: constant("S::A") is a direct access, like` |
|        - |  464 | `				 * OP_MEMBER); plain constant → run its initializer. Unlike` |
|        - |  465 | `				 * defined(), reading the value has to force this. */` |
|        - |  466 | `				sxi32 rcEnum;` |
|       21 |  467 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE ){` |
|        3 |  468 | `					rcEnum = VmEnumMaterialize(pCtx->pVm,pClass);` |
|        2 |  469 | `				}else{` |
|       19 |  470 | `					rcEnum = VmClassConstEvalOnDemand(pCtx->pVm,pClass,pAttr);` |
|        - |  471 | `				}` |
|       21 |  472 | `				if( rcEnum != SXRET_OK ){` |
|        3 |  473 | `					return rcEnum;` |
|        - |  474 | `				}` |
|        8 |  475 | `			}` |
|        - |  476 | `			{` |
|       32 |  477 | `				ph7_value *pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pAttr->nIdx);` |
|       32 |  478 | `				if( pValue ){` |
|       32 |  479 | `					if( SySetUsed(&pAttr->aAttrs) > 0 ){` |
|        - |  480 | `						/* #[\Deprecated] warns through constant() too (php) */` |
|        3 |  481 | `						VmDeprecatedConstNotice(pCtx->pVm,pClass,pAttr);` |
|        1 |  482 | `					}` |
|       32 |  483 | `					ph7_result_value(pCtx,pValue);` |
|       32 |  484 | `					return SXRET_OK;` |
|        - |  485 | `				}` |
|        - |  486 | `			}` |
|        - |  487 | `			/* Declared but with no slot to read: the same dead end the pre-fix code` |
|        - |  488 | `			 * fell through to. */` |
|      ! 0 |  489 | `			return VmClassConstError(pCtx,VM_CCONST_NOCONST,zName,nLen,iSep,pAttr);` |
|        - |  490 | `		}` |
|        - |  491 | `	}` |
|        - |  492 | `	/* Perform the query */` |
|       85 |  493 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       85 |  494 | `	if( pEntry == 0 ){` |
|        - |  495 | `		/* php 8: a catchable Error, not a notice + NULL (band A #4) */` |
|        8 |  496 | `		return PH7_VmThrowException(pCtx,"Error",` |
|        2 |  497 | `			"Undefined constant \"%.*s\"",nLen,zName);` |
|        - |  498 | `	}` |
|       81 |  499 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  500 | `	/* Point to the structure that describe the constant */` |
|       81 |  501 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  502 | `	/* Extract constant value by calling it's associated callback` |
|        - |  503 | `	 * (emits the #[\Deprecated] notice first when attributed, php) */` |
|       81 |  504 | `	VmExpandConstantWithNotice(pCtx->pVm,pCons,&sVal);` |
|        - |  505 | `	/* Return that value */` |
|       81 |  506 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  507 | `	/* Cleanup */` |
|       81 |  508 | `	PH7_MemObjRelease(&sVal);` |
|       81 |  509 | `	return SXRET_OK;` |
|       79 |  510 | `}` |
|        - |  511 | `/*` |
|        - |  512 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  513 | ` * defined below.` |
|        - |  514 | ` */` |
|      948 |  515 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  516 | `{` |
|      949 |  517 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  518 | `	ph7_value sName;` |
|        - |  519 | `	sxi32 rc;` |
|        - |  520 | `	/* Prepare the constant name for insertion */` |
|      949 |  521 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      949 |  522 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  523 | `	/* Perform the insertion */` |
|      949 |  524 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      949 |  525 | `	PH7_MemObjRelease(&sName);` |
|      949 |  526 | `	return rc;` |
|        1 |  527 | `}` |
|        - |  528 | `/*` |
|        - |  529 | ` * array get_defined_constants(void)` |
|        - |  530 | ` *  Returns an associative array with the names of all defined` |
|        - |  531 | ` *  constants.` |
|        - |  532 | ` * Parameters` |
|        - |  533 | ` *  NONE.` |
|        - |  534 | ` * Returns` |
|        - |  535 | ` *  Returns the names of all the constants currently defined.` |
|        - |  536 | ` */` |
|        2 |  537 | `PH7_PRIVATE int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  538 | `{` |
|        - |  539 | `	ph7_value *pArray;` |
|        - |  540 | `	/* Create the array first*/` |
|        3 |  541 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  542 | `	if( pArray == 0 ){` |
|      ! 0 |  543 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  544 | `		SXUNUSED(apArg);` |
|        - |  545 | `		/* Return NULL */` |
|      ! 0 |  546 | `		ph7_result_null(pCtx);` |
|      ! 0 |  547 | `		return SXRET_OK;` |
|        - |  548 | `	}` |
|        - |  549 | `	/* Fill the array with the defined constants */` |
|        3 |  550 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  551 | `	/* Return the created array */` |
|        3 |  552 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  553 | `	return SXRET_OK;` |
|        2 |  554 | `}` |
|        - |  555 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - |  556 | `/*` |
|        - |  557 | ` * Section:` |
|        - |  558 | ` *  Random numbers/string generators.` |
|        - |  559 | ` * Status:` |
|        - |  560 | ` *    Stable.` |
|        - |  561 | ` */` |
|        - |  562 | `/*` |
|        - |  563 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  564 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|        - |  565 | ` * implemented in src/sx/sxrand.c).` |
|        - |  566 | ` */` |
|     3960 |  567 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        5 |  568 | `{` |
|        - |  569 | `	sxu32 iNum;` |
|     3965 |  570 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     3965 |  571 | `	return iNum;` |
|        5 |  572 | `}` |
|        - |  573 | `/*` |
|        - |  574 | ` * The MT19937 generator that backs PHP's rand()/mt_rand() family. It is kept` |
|        - |  575 | ` * separate from the RC4 SyRandomness above so that srand()/mt_srand() give` |
|        - |  576 | ` * userland PHP's reproducible sequence without perturbing the engine's internal` |
|        - |  577 | ` * entropy (object ids, uniqid, quicksort pivots stay on the RC4 generator, as` |
|        - |  578 | ` * they are in PHP too — srand does not touch those).` |
|        - |  579 | ` */` |
|        - |  580 | `/*` |
|        - |  581 | ` * Reset the MT19937 state to a 32-bit seed (PHP truncates its int seed likewise).` |
|        - |  582 | ` */` |
|       36 |  583 | `PH7_PRIVATE void PH7_VmMtSrand(ph7_vm *pVm,sxu32 nSeed)` |
|        1 |  584 | `{` |
|       37 |  585 | `	SyMT19937Seed(&pVm->sMt,nSeed);` |
|       37 |  586 | `	pVm->mtSeeded = TRUE;` |
|       37 |  587 | `}` |
|        - |  588 | `/*` |
|        - |  589 | ` * Draw the next full 32-bit MT19937 word, seeding lazily from the OS CSPRNG on` |
|        - |  590 | ` * first use exactly as PHP auto-seeds when rand()/mt_rand() runs before srand().` |
|        - |  591 | ` */` |
|     1782 |  592 | `PH7_PRIVATE sxu32 PH7_VmMtRand(ph7_vm *pVm)` |
|        1 |  593 | `{` |
|     1783 |  594 | `	if( !pVm->mtSeeded ){` |
|        - |  595 | `		sxu32 nSeed;` |
|        3 |  596 | `		if( SyOSCSPRNG((void *)&nSeed,sizeof(nSeed)) != SXRET_OK ){` |
|        - |  597 | `			/* No OS entropy source: fall back to the RC4 generator's output. */` |
|      ! 0 |  598 | `			nSeed = PH7_VmRandomNum(pVm);` |
|      ! 0 |  599 | `		}` |
|        3 |  600 | `		SyMT19937Seed(&pVm->sMt,nSeed);` |
|        3 |  601 | `		pVm->mtSeeded = TRUE;` |
|        1 |  602 | `	}` |
|     1783 |  603 | `	return SyMT19937Next(&pVm->sMt);` |
|        1 |  604 | `}` |
|        - |  605 | `/*` |
|        - |  606 | ` * Map a full 32-bit draw uniformly into [0,uMax] (uMax is the range width, i.e.` |
|        - |  607 | ` * max-min). Rejection sampling against the largest unbiased ceiling, matching` |
|        - |  608 | ` * PHP's php_random_range32().` |
|        - |  609 | ` */` |
|     1658 |  610 | `static sxu32 VmMtRange32(ph7_vm *pVm,sxu32 uMax)` |
|        1 |  611 | `{` |
|        - |  612 | `	sxu32 result,limit;` |
|     1659 |  613 | `	result = PH7_VmMtRand(pVm);` |
|        - |  614 | `	/* Whole 32-bit domain: no scaling needed. */` |
|     1659 |  615 | `	if( uMax == 0xFFFFFFFFU ){` |
|      ! 0 |  616 | `		return result;` |
|        - |  617 | `	}` |
|        - |  618 | `	/* Make the range inclusive of max. */` |
|     1659 |  619 | `	uMax++;` |
|        - |  620 | `	/* Powers of two are unbiased under a plain mask. */` |
|     1659 |  621 | `	if( (uMax & (uMax - 1)) == 0 ){` |
|        7 |  622 | `		return result & (uMax - 1);` |
|        - |  623 | `	}` |
|        - |  624 | `	/* Ceiling under which 0xFFFFFFFF % uMax == 0; discard draws above it. */` |
|     1653 |  625 | `	limit = 0xFFFFFFFFU - (0xFFFFFFFFU % uMax) - 1;` |
|     1653 |  626 | `	while( result > limit ){` |
|      ! 0 |  627 | `		result = PH7_VmMtRand(pVm);` |
|      ! 0 |  628 | `	}` |
|     1653 |  629 | `	return result % uMax;` |
|      830 |  630 | `}` |
|        - |  631 | `/*` |
|        - |  632 | ` * 64-bit-wide range: assemble two draws (high word first, as PHP does) and` |
|        - |  633 | ` * reject-sample. Matches PHP's php_random_range64().` |
|        - |  634 | ` */` |
|        4 |  635 | `static sxu64 VmMtRange64(ph7_vm *pVm,sxu64 uMax)` |
|        1 |  636 | `{` |
|        - |  637 | `	sxu64 result,limit;` |
|        - |  638 | `	/* First draw fills the low word, second draw the high word — order is` |
|        - |  639 | `	 * significant and matches php's php_random_range64() assembly. */` |
|        5 |  640 | `	result = (sxu64)PH7_VmMtRand(pVm);` |
|        5 |  641 | `	result \|= (sxu64)PH7_VmMtRand(pVm) << 32;` |
|        5 |  642 | `	if( uMax == 0xFFFFFFFFFFFFFFFFULL ){` |
|      ! 0 |  643 | `		return result;` |
|        - |  644 | `	}` |
|        5 |  645 | `	uMax++;` |
|        5 |  646 | `	if( (uMax & (uMax - 1)) == 0 ){` |
|        3 |  647 | `		return result & (uMax - 1);` |
|        - |  648 | `	}` |
|        3 |  649 | `	limit = 0xFFFFFFFFFFFFFFFFULL - (0xFFFFFFFFFFFFFFFFULL % uMax) - 1;` |
|        3 |  650 | `	while( result > limit ){` |
|      ! 0 |  651 | `		result = (sxu64)PH7_VmMtRand(pVm);` |
|      ! 0 |  652 | `		result \|= (sxu64)PH7_VmMtRand(pVm) << 32;` |
|      ! 0 |  653 | `	}` |
|        3 |  654 | `	return result % uMax;` |
|        3 |  655 | `}` |
|        - |  656 | `/*` |
|        - |  657 | ` * Return a value uniformly in the inclusive range [iMin,iMax]. The caller` |
|        - |  658 | ` * guarantees iMin <= iMax. Mirrors PHP's php_mt_rand_range(): a range that fits` |
|        - |  659 | ` * in 32 bits takes the 32-bit path, a wider one the 64-bit path.` |
|        - |  660 | ` */` |
|     1662 |  661 | `PH7_PRIVATE sxi64 PH7_VmMtRandRange(ph7_vm *pVm,sxi64 iMin,sxi64 iMax)` |
|        1 |  662 | `{` |
|     1663 |  663 | `	sxu64 uMax = (sxu64)iMax - (sxu64)iMin;` |
|     1663 |  664 | `	if( uMax > 0xFFFFFFFFULL ){` |
|        5 |  665 | `		return (sxi64)(VmMtRange64(pVm,uMax) + (sxu64)iMin);` |
|        - |  666 | `	}` |
|     1659 |  667 | `	return (sxi64)((sxu64)VmMtRange32(pVm,(sxu32)uMax) + (sxu64)iMin);` |
|      832 |  668 | `}` |
|        - |  669 | `/*` |
|        - |  670 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  671 | ` * Note that the generated string is NOT null terminated.` |
|        - |  672 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|        - |  673 | ` * implemented in src/sx/sxrand.c).` |
|        - |  674 | ` */` |
|  3380782 |  675 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        5 |  676 | `{` |
|        - |  677 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  678 | `	int i;` |
|        - |  679 | `	/* Generate a binary string first */` |
|  3380787 |  680 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  681 | `	/* Turn the binary string into english based alphabet */` |
| 37188823 |  682 | `	for( i = 0 ; i < nLen ; ++i ){` |
| 33808041 |  683 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
| 16904023 |  684 | `	 }` |
|  3380787 |  685 | `}` |
|        - |  686 | `/*` |
|        - |  687 | ` * int rand()` |
|        - |  688 | ` * int mt_rand()` |
|        - |  689 | ` * int rand(int $min,int $max)` |
|        - |  690 | ` * int mt_rand(int $min,int $max)` |
|        - |  691 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  692 | ` * Parameter` |
|        - |  693 | ` *  $min` |
|        - |  694 | ` *    The lowest value to return (default: 0)` |
|        - |  695 | ` *  $max` |
|        - |  696 | ` *   The highest value to return (default: getrandmax())` |
|        - |  697 | ` * Return` |
|        - |  698 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  699 | ` * Note:` |
|        - |  700 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  701 | ` *  by te SQLite3 library.` |
|        - |  702 | ` */` |
|     1728 |  703 | `PH7_PRIVATE int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  704 | `{` |
|     1729 |  705 | `	SyString *pName = &pCtx->pFunc->sName;` |
|     3019 |  706 | `	int bMt = (pName->nByte == sizeof("mt_rand")-1` |
|     1728 |  707 | `		&& SyMemcmp(pName->zString,"mt_rand",sizeof("mt_rand")-1) == 0);` |
|        - |  708 | `	/* php accepts exactly 0 or exactly 2 arguments (min,max); 1 or 3+ is an` |
|        - |  709 | `	 * ArgumentCountError. The central arity table can't express "0 or 2", so` |
|        - |  710 | `	 * it is enforced here (was a silent wrong result for the raw draw). */` |
|     1729 |  711 | `	if( nArg == 1 \|\| nArg > 2 ){` |
|       13 |  712 | `		return PH7_VmThrowException(pCtx,` |
|        - |  713 | `			"ArgumentCountError",` |
|        - |  714 | `			"%z() expects exactly 2 arguments, %d given",` |
|        4 |  715 | `			pName, nArg` |
|        - |  716 | `			);` |
|        - |  717 | `	}` |
|     1721 |  718 | `	if( nArg == 2 ){` |
|        - |  719 | `		sxi64 iMin,iMax;` |
|        - |  720 | `		/* Signed 64-bit endpoints: the old unsigned math wrapped negative` |
|        - |  721 | `		 * ranges to huge positives (rand(-10,-1) -> ~4e9) and mis-handled` |
|        - |  722 | `		 * min==max. */` |
|     1659 |  723 | `		iMin = ph7_value_to_int64(apArg[0]);` |
|     1659 |  724 | `		iMax = ph7_value_to_int64(apArg[1]);` |
|     1659 |  725 | `		if( iMin > iMax ){` |
|        9 |  726 | `			if( bMt ){` |
|        - |  727 | `				/* mt_rand() is strict: php throws a catchable ValueError. */` |
|        5 |  728 | `				return PH7_VmThrowException(pCtx,` |
|        - |  729 | `					"ValueError",` |
|        - |  730 | `					"mt_rand(): Argument #2 ($max) must be greater than or equal to argument #1 ($min)"` |
|        - |  731 | `					);` |
|        - |  732 | `			}` |
|        - |  733 | `			/* rand() swaps the bounds for backward compatibility (php keeps` |
|        - |  734 | `			 * this quirk; only mt_rand() rejects a reversed range). */` |
|        5 |  735 | `			{ sxi64 iTmp = iMin; iMin = iMax; iMax = iTmp; }` |
|        2 |  736 | `		}` |
|        - |  737 | `		/* MT19937-backed uniform draw over [iMin,iMax], bit-for-bit as php. */` |
|     1655 |  738 | `		ph7_result_int64(pCtx,PH7_VmMtRandRange(pCtx->pVm,iMin,iMax));` |
|     1655 |  739 | `		return SXRET_OK;` |
|        - |  740 | `	}` |
|        - |  741 | `	/* No-argument form: a 31-bit value in [0, mt_getrandmax()]. php returns` |
|        - |  742 | `	 * php_mt_rand() >> 1 for the bare draw (the full 32-bit word feeds the` |
|        - |  743 | `	 * range form above, but the bare form drops the low bit). */` |
|       63 |  744 | `	ph7_result_int64(pCtx,(sxi64)(PH7_VmMtRand(pCtx->pVm) >> 1));` |
|       63 |  745 | `	return SXRET_OK;` |
|      865 |  746 | `}` |
|        - |  747 | `/*` |
|        - |  748 | ` * int getrandmax(void)` |
|        - |  749 | ` * int mt_getrandmax(void)` |
|        - |  750 | ` * int rc4_getrandmax(void)` |
|        - |  751 | ` *   Show largest possible random value` |
|        - |  752 | ` * Return` |
|        - |  753 | ` *  The largest possible random value returned by rand()/mt_rand(): php's` |
|        - |  754 | ` *  MT19937 backing makes this 2^31-1 (2147483647) for both.` |
|        - |  755 | ` */` |
|        8 |  756 | `PH7_PRIVATE int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  757 | `{` |
|        4 |  758 | `	SXUNUSED(nArg); /* cc warning */` |
|        4 |  759 | `	SXUNUSED(apArg);` |
|        - |  760 | `	/* php: PHP_MT_RAND_MAX == (1<<31)-1; bare rand()/mt_rand() draw >> 1 lands` |
|        - |  761 | `	 * exactly in [0, this]. */` |
|        9 |  762 | `	ph7_result_int64(pCtx,2147483647);` |
|        9 |  763 | `	return SXRET_OK;` |
|        1 |  764 | `}` |
|        - |  765 | `/*` |
|        - |  766 | ` * string rand_str()` |
|        - |  767 | ` * string rand_str(int $len)` |
|        - |  768 | ` *  Generate a random string (English alphabet).` |
|        - |  769 | ` * Parameter` |
|        - |  770 | ` *  $len` |
|        - |  771 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  772 | ` * Return` |
|        - |  773 | ` *   A pseudo random string.` |
|        - |  774 | ` * Note:` |
|        - |  775 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  776 | ` *  by te SQLite3 library.` |
|        - |  777 | ` *  This function is a symisc extension.` |
|        - |  778 | ` */` |
|      144 |  779 | `PH7_PRIVATE int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 |  780 | `{` |
|        - |  781 | `	char zString[1024];` |
|      147 |  782 | `	int iLen = 0x10;` |
|      147 |  783 | `	if( nArg > 0 ){` |
|        - |  784 | `		/* Get the desired length */` |
|      147 |  785 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      147 |  786 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  787 | `			/* Default length */` |
|        3 |  788 | `			iLen = 0x10;` |
|        1 |  789 | `		}` |
|       72 |  790 | `	}` |
|        - |  791 | `	/* Generate the random string */` |
|      147 |  792 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  793 | `	/* Return the generated string */` |
|      147 |  794 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      147 |  795 | `	return SXRET_OK;` |
|        3 |  796 | `}` |
|        - |  797 | `/*` |
|        - |  798 | ` * Reject non-numeric values (array/object/resource and non-numeric strings)` |
|        - |  799 | ` * the same way intdiv() does. Returns SXRET_OK if the value is acceptable as` |
|        - |  800 | ` * an int (PHP coerces float and numeric string silently).` |
|        - |  801 | ` */` |
|      484 |  802 | `static int VmRandomCheckIntArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgPos,const char *zParamName)` |
|        2 |  803 | `{` |
|      484 |  804 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg)` |
|      486 |  805 | `		\|\| ph7_value_is_resource(pArg) ){` |
|      ! 0 |  806 | `		return PH7_VmThrowException(pCtx,` |
|        - |  807 | `			"TypeError",` |
|        - |  808 | `			"%s(): Argument #%d (%s) must be of type int, %s given",` |
|      ! 0 |  809 | `			zFunc,iArgPos,zParamName,` |
|      ! 0 |  810 | `			ph7_type_name(pArg)` |
|        - |  811 | `			);` |
|        - |  812 | `	}` |
|      486 |  813 | `	if( ph7_value_is_string(pArg) ){` |
|        - |  814 | `		int len;` |
|        9 |  815 | `		const char *zStr = ph7_value_to_string(pArg, &len);` |
|        9 |  816 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|        7 |  817 | `			return PH7_VmThrowException(pCtx,` |
|        - |  818 | `				"TypeError",` |
|        - |  819 | `				"%s(): Argument #%d (%s) must be of type int, string given",` |
|        2 |  820 | `				zFunc,iArgPos,zParamName` |
|        - |  821 | `				);` |
|        - |  822 | `		}` |
|        2 |  823 | `	}` |
|      482 |  824 | `	return SXRET_OK;` |
|      244 |  825 | `}` |
|        - |  826 | `/*` |
|        - |  827 | ` * int random_int(int $min, int $max)` |
|        - |  828 | ` *  Generate a cryptographically secure pseudo-random integer in [$min, $max].` |
|        - |  829 | ` *  Mirrors PHP 7.0+ random_int(). Uses the OS CSPRNG via SyOSCSPRNG().` |
|        - |  830 | ` *  Distribution is uniform via rejection sampling against the smallest` |
|        - |  831 | ` *  power-of-two mask covering the range.` |
|        - |  832 | ` */` |
|      232 |  833 | `PH7_PRIVATE int vm_builtin_random_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  834 | `{` |
|        - |  835 | `	sxi64 iMin,iMax;` |
|        - |  836 | `	sxu64 uRange,uMask,uResult;` |
|        - |  837 | `	unsigned int nAttempt;` |
|        - |  838 | `	int rc;` |
|      233 |  839 | `	if( nArg != 2 ){` |
|      ! 0 |  840 | `		return PH7_VmThrowException(pCtx,` |
|        - |  841 | `			"ArgumentCountError",` |
|        - |  842 | `			"random_int() expects exactly 2 arguments, %d given",` |
|      ! 0 |  843 | `			nArg` |
|        - |  844 | `			);` |
|        - |  845 | `	}` |
|      233 |  846 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_int",1,"$min");` |
|      233 |  847 | `	if( rc != SXRET_OK ){ return rc; }` |
|      231 |  848 | `	rc = VmRandomCheckIntArg(pCtx,apArg[1],"random_int",2,"$max");` |
|      231 |  849 | `	if( rc != SXRET_OK ){ return rc; }` |
|      231 |  850 | `	iMin = ph7_value_to_int64(apArg[0]);` |
|      231 |  851 | `	iMax = ph7_value_to_int64(apArg[1]);` |
|      231 |  852 | `	if( iMin > iMax ){` |
|        3 |  853 | `		return PH7_VmThrowException(pCtx,` |
|        - |  854 | `			"ValueError",` |
|        - |  855 | `			"random_int(): Argument #1 ($min) must be less than or equal to argument #2 ($max)"` |
|        - |  856 | `			);` |
|        - |  857 | `	}` |
|      229 |  858 | `	if( iMin == iMax ){` |
|        5 |  859 | `		ph7_result_int64(pCtx,iMin);` |
|        5 |  860 | `		return SXRET_OK;` |
|        - |  861 | `	}` |
|      225 |  862 | `	uRange = (sxu64)iMax - (sxu64)iMin;` |
|      225 |  863 | `	uMask = uRange;` |
|      225 |  864 | `	uMask \|= uMask >> 1;` |
|      225 |  865 | `	uMask \|= uMask >> 2;` |
|      225 |  866 | `	uMask \|= uMask >> 4;` |
|      225 |  867 | `	uMask \|= uMask >> 8;` |
|      225 |  868 | `	uMask \|= uMask >> 16;` |
|      225 |  869 | `	uMask \|= uMask >> 32;` |
|      225 |  870 | `	uResult = 0;` |
|      333 |  871 | `	for( nAttempt = 0 ; nAttempt < 50 ; ++nAttempt ){` |
|        - |  872 | `		/* Always draw a full 8 bytes so endianness of the cast doesn't matter` |
|        - |  873 | `		 * (a 4-byte fill into a sxu64 would land in the high half on big-endian` |
|        - |  874 | `		 * and the low-half mask would always read 0). */` |
|        - |  875 | `		sxu64 uDraw;` |
|      333 |  876 | `		if( SyOSCSPRNG(&uDraw,sizeof(uDraw)) != SXRET_OK ){` |
|      ! 0 |  877 | `			return PH7_VmThrowException(pCtx,` |
|        - |  878 | `				"Random\\RandomException",` |
|        - |  879 | `				"Cannot gather sufficient random data"` |
|        - |  880 | `				);` |
|        - |  881 | `		}` |
|      333 |  882 | `		uDraw &= uMask;` |
|      333 |  883 | `		if( uDraw <= uRange ){` |
|      225 |  884 | `			uResult = uDraw;` |
|      225 |  885 | `			break;` |
|        - |  886 | `		}` |
|       53 |  887 | `	}` |
|      225 |  888 | `	if( nAttempt >= 50 ){` |
|      ! 0 |  889 | `		return PH7_VmThrowException(pCtx,` |
|        - |  890 | `			"Random\\RandomException",` |
|        - |  891 | `			"Cannot gather sufficient random data"` |
|        - |  892 | `			);` |
|        - |  893 | `	}` |
|      225 |  894 | `	ph7_result_int64(pCtx,(sxi64)((sxu64)iMin + uResult));` |
|      225 |  895 | `	return SXRET_OK;` |
|      117 |  896 | `}` |
|        - |  897 | `/*` |
|        - |  898 | ` * string random_bytes(int $length)` |
|        - |  899 | ` *  Generate $length cryptographically secure random bytes via SyOSCSPRNG().` |
|        - |  900 | ` *  Mirrors PHP 7.0+ random_bytes().` |
|        - |  901 | ` */` |
|       22 |  902 | `PH7_PRIVATE int vm_builtin_random_bytes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  903 | `{` |
|        - |  904 | `	sxi64 iLen;` |
|        - |  905 | `	unsigned char zStack[256];` |
|        - |  906 | `	void *pBuf;` |
|        - |  907 | `	int rc;` |
|       24 |  908 | `	int bHeap = 0;` |
|       24 |  909 | `	if( nArg != 1 ){` |
|      ! 0 |  910 | `		return PH7_VmThrowException(pCtx,` |
|        - |  911 | `			"ArgumentCountError",` |
|        - |  912 | `			"random_bytes() expects exactly 1 argument, %d given",` |
|      ! 0 |  913 | `			nArg` |
|        - |  914 | `			);` |
|        - |  915 | `	}` |
|       24 |  916 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_bytes",1,"$length");` |
|       24 |  917 | `	if( rc != SXRET_OK ){ return rc; }` |
|       22 |  918 | `	iLen = ph7_value_to_int64(apArg[0]);` |
|       22 |  919 | `	if( iLen < 1 ){` |
|        5 |  920 | `		return PH7_VmThrowException(pCtx,` |
|        - |  921 | `			"ValueError",` |
|        - |  922 | `			"random_bytes(): Argument #1 ($length) must be greater than 0"` |
|        - |  923 | `			);` |
|        - |  924 | `	}` |
|        - |  925 | `	/* The PH7 allocator and ph7_result_string both take sxu32/int sizes,` |
|        - |  926 | `	 * so we can't honor a length above 2 GiB. Reject early rather than` |
|        - |  927 | `	 * silently truncating via the (sxu32) cast below. */` |
|       18 |  928 | `	if( iLen > 0x7FFFFFFF ){` |
|      ! 0 |  929 | `		return PH7_VmThrowException(pCtx,` |
|        - |  930 | `			"ValueError",` |
|        - |  931 | `			"random_bytes(): Argument #1 ($length) is too large"` |
|        - |  932 | `			);` |
|        - |  933 | `	}` |
|       18 |  934 | `	if( iLen <= (sxi64)sizeof(zStack) ){` |
|       18 |  935 | `		pBuf = zStack;` |
|       10 |  936 | `	}else{` |
|      ! 0 |  937 | `		pBuf = SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)iLen);` |
|      ! 0 |  938 | `		if( pBuf == 0 ){` |
|      ! 0 |  939 | `			return PH7_VmThrowException(pCtx,` |
|        - |  940 | `				"Exception",` |
|        - |  941 | `				"random_bytes(): Failed to allocate %qd bytes",` |
|      ! 0 |  942 | `				iLen` |
|        - |  943 | `				);` |
|        - |  944 | `		}` |
|      ! 0 |  945 | `		bHeap = 1;` |
|        - |  946 | `	}` |
|       18 |  947 | `	if( SyOSCSPRNG(pBuf,(sxu32)iLen) != SXRET_OK ){` |
|      ! 0 |  948 | `		if( bHeap ){` |
|      ! 0 |  949 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 |  950 | `		}` |
|      ! 0 |  951 | `		return PH7_VmThrowException(pCtx,` |
|        - |  952 | `			"Random\\RandomException",` |
|        - |  953 | `			"Cannot gather sufficient random data"` |
|        - |  954 | `			);` |
|        - |  955 | `	}` |
|       18 |  956 | `	ph7_result_string(pCtx,(const char *)pBuf,(int)iLen);` |
|       18 |  957 | `	if( bHeap ){` |
|      ! 0 |  958 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 |  959 | `	}` |
|       18 |  960 | `	return SXRET_OK;` |
|       13 |  961 | `}` |
|        - |  962 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  963 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  964 | `/* Unique ID private data */` |
|        - |  965 | `struct unique_id_data` |
|        - |  966 | `{` |
|        - |  967 | `	ph7_context *pCtx; /* Call context */` |
|        - |  968 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  969 | `};` |
|        - |  970 | `/*` |
|        - |  971 | ` * Binary to hex consumer callback.` |
|        - |  972 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  973 | ` * defined below.` |
|        - |  974 | ` */` |
|      192 |  975 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  976 | `{` |
|      193 |  977 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  978 | `	sxu32 nBuflen;` |
|        - |  979 | `	/* Extract result buffer length */` |
|      193 |  980 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  981 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  982 | `			/*` |
|        - |  983 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  984 | `			 * string will be 13 characters long` |
|        - |  985 | `			 */` |
|       25 |  986 | `		return SXERR_ABORT;` |
|        - |  987 | `	}` |
|      169 |  988 | `	if( nBuflen > 22 ){` |
|      ! 0 |  989 | `		return SXERR_ABORT;` |
|        - |  990 | `	}` |
|        - |  991 | `	/* Safely Consume the hex stream */` |
|      169 |  992 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  993 | `	return SXRET_OK;` |
|       97 |  994 | `}` |
|        - |  995 | `/*` |
|        - |  996 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  997 | ` *  Generate a unique ID` |
|        - |  998 | ` * Parameter` |
|        - |  999 | ` * $prefix` |
|        - | 1000 | ` *  Append this prefix to the generated unique ID.` |
|        - | 1001 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - | 1002 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - | 1003 | ` * $more_entropy` |
|        - | 1004 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - | 1005 | ` *  that the result will be unique.` |
|        - | 1006 | ` * Return` |
|        - | 1007 | ` *  Returns the unique identifier, as a string.` |
|        - | 1008 | ` */` |
|       24 | 1009 | `PH7_PRIVATE int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1010 | `{` |
|        - | 1011 | `	struct unique_id_data sUniq;` |
|        - | 1012 | `	unsigned char zDigest[20];` |
|       25 | 1013 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 1014 | `	const char *zPrefix;` |
|        - | 1015 | `	SHA1Context sCtx;` |
|        - | 1016 | `	char zRandom[7];` |
|        - | 1017 | `	int nPrefix;` |
|        - | 1018 | `	int entropy;` |
|        - | 1019 | `	/* Generate a random string first */` |
|       25 | 1020 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - | 1021 | `	/* Initialize fields */` |
|       25 | 1022 | `	zPrefix = 0;` |
|       25 | 1023 | `	nPrefix = 0;` |
|       25 | 1024 | `	entropy = 0;` |
|       25 | 1025 | `	if( nArg > 0 ){` |
|        - | 1026 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 | 1027 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 | 1028 | `		if( nArg > 1 ){` |
|      ! 0 | 1029 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 1030 | `		}` |
|      ! 0 | 1031 | `	}` |
|       25 | 1032 | `	SHA1Init(&sCtx);` |
|        - | 1033 | `	/* Generate the random ID */` |
|       25 | 1034 | `	if( nPrefix > 0 ){` |
|      ! 0 | 1035 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 | 1036 | `	}` |
|        - | 1037 | `	/* Append the random ID */` |
|       25 | 1038 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - | 1039 | `	/* Append the random string */` |
|       25 | 1040 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - | 1041 | `	/* Increment the number */` |
|       25 | 1042 | `	pVm->unique_id++;` |
|       25 | 1043 | `	SHA1Final(&sCtx,zDigest);` |
|        - | 1044 | `	/* Hexify the digest */` |
|       25 | 1045 | `	sUniq.pCtx = pCtx;` |
|       25 | 1046 | `	sUniq.entropy = entropy;` |
|       25 | 1047 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - | 1048 | `	/* All done */` |
|       25 | 1049 | `	return PH7_OK;` |
|        1 | 1050 | `}` |
|        - | 1051 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 1052 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 1053 | `/*` |
|        - | 1054 | ` * Section:` |
|        - | 1055 | ` *  Language construct implementation as foreign functions.` |
|        - | 1056 | ` * Status:` |
|        - | 1057 | ` *    Stable.` |
|        - | 1058 | ` */` |
|        - | 1059 | `/*` |
|        - | 1060 | ` * The user-visible string coercion an OUTPUT construct performs on one of its` |
|        - | 1061 | ` * arguments (echo/print reached as host functions rather than as OP_CONSUME).` |
|        - | 1062 | ` * An ARRAY warns and still renders as "Array"; an object whose class has no` |
|        - | 1063 | ` * __toString() is php's catchable "could not be converted to string" Error,` |
|        - | 1064 | ` * and the construct outputs nothing for it. The status is recorded on the` |
|        - | 1065 | ` * context too, so OP_CALL cannot treat the throwing call as a normal return.` |
|        - | 1066 | ` */` |
|       40 | 1067 | `static sxi32 VmOutputArgToString(ph7_context *pCtx,ph7_value *pArg,const char **pzData,int *pnLen)` |
|        4 | 1068 | `{` |
|       44 | 1069 | `	sxi32 rc = PH7_MemObjToStringUV(pArg);` |
|       44 | 1070 | `	if( rc != SXRET_OK ){` |
|        3 | 1071 | `		pCtx->nThrowRc = rc;` |
|        3 | 1072 | `		return rc;` |
|        - | 1073 | `	}` |
|       41 | 1074 | `	*pzData = ph7_value_to_string(pArg,pnLen);` |
|       41 | 1075 | `	return SXRET_OK;` |
|       24 | 1076 | `}` |
|        - | 1077 | `/*` |
|        - | 1078 | ` * void echo($string...)` |
|        - | 1079 | ` *  Output one or more messages.` |
|        - | 1080 | ` * Parameters` |
|        - | 1081 | ` *  $string` |
|        - | 1082 | ` *   Message to output.` |
|        - | 1083 | ` * Return` |
|        - | 1084 | ` *  NULL.` |
|        - | 1085 | ` */` |
|      ! 0 | 1086 | `PH7_PRIVATE int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 1087 | `{` |
|        - | 1088 | `	const char *zData;` |
|      ! 0 | 1089 | `	int nDataLen = 0;` |
|        - | 1090 | `	ph7_vm *pVm;` |
|        - | 1091 | `	int i,rc;` |
|        - | 1092 | `	/* Point to the target VM */` |
|      ! 0 | 1093 | `	pVm = pCtx->pVm;` |
|        - | 1094 | `	/* Output */` |
|      ! 0 | 1095 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 | 1096 | `		sxi32 rcSv = VmOutputArgToString(pCtx,apArg[i],&zData,&nDataLen);` |
|      ! 0 | 1097 | `		if( rcSv != SXRET_OK ){` |
|      ! 0 | 1098 | `			return rcSv;` |
|        - | 1099 | `		}` |
|      ! 0 | 1100 | `		if( nDataLen > 0 ){` |
|      ! 0 | 1101 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 | 1102 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 | 1103 | `			if( rc == SXERR_ABORT ){` |
|        - | 1104 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 1105 | `				return PH7_ABORT;` |
|        - | 1106 | `			}` |
|      ! 0 | 1107 | `		}` |
|      ! 0 | 1108 | `	}` |
|      ! 0 | 1109 | `	return SXRET_OK;` |
|      ! 0 | 1110 | `}` |
|        - | 1111 | `/*` |
|        - | 1112 | ` * int print($string...)` |
|        - | 1113 | ` *  Output one or more messages.` |
|        - | 1114 | ` * Parameters` |
|        - | 1115 | ` *  $string` |
|        - | 1116 | ` *   Message to output.` |
|        - | 1117 | ` * Return` |
|        - | 1118 | ` *  1 always.` |
|        - | 1119 | ` */` |
|       40 | 1120 | `PH7_PRIVATE int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 | 1121 | `{` |
|        - | 1122 | `	const char *zData;` |
|       44 | 1123 | `	int nDataLen = 0;` |
|        - | 1124 | `	ph7_vm *pVm;` |
|        - | 1125 | `	int i,rc;` |
|        - | 1126 | `	/* Point to the target VM */` |
|       44 | 1127 | `	pVm = pCtx->pVm;` |
|        - | 1128 | `	/* Output */` |
|       82 | 1129 | `	for( i = 0 ; i < nArg ; ++i ){` |
|       44 | 1130 | `		sxi32 rcSv = VmOutputArgToString(pCtx,apArg[i],&zData,&nDataLen);` |
|       44 | 1131 | `		if( rcSv != SXRET_OK ){` |
|        3 | 1132 | `			return rcSv;` |
|        - | 1133 | `		}` |
|       41 | 1134 | `		if( nDataLen > 0 ){` |
|       41 | 1135 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|       41 | 1136 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|       41 | 1137 | `			if( rc == SXERR_ABORT ){` |
|        - | 1138 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 1139 | `				return PH7_ABORT;` |
|        - | 1140 | `			}` |
|       19 | 1141 | `		}` |
|       22 | 1142 | `	}` |
|        - | 1143 | `	/* Return 1 */` |
|       41 | 1144 | `	ph7_result_int(pCtx,1);` |
|       41 | 1145 | `	return SXRET_OK;` |
|       24 | 1146 | `}` |
|        - | 1147 | `/*` |
|        - | 1148 | ` * void exit(string $msg)` |
|        - | 1149 | ` * void exit(int $status)` |
|        - | 1150 | ` * void die(string $ms)` |
|        - | 1151 | ` * void die(int $status)` |
|        - | 1152 | ` *   Output a message and terminate program execution.` |
|        - | 1153 | ` * Parameter` |
|        - | 1154 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - | 1155 | ` *  If status is an integer, that value will be used as the exit status` |
|        - | 1156 | ` *  and not printed` |
|        - | 1157 | ` * Return` |
|        - | 1158 | ` *  NULL` |
|        - | 1159 | ` */` |
|      ! 0 | 1160 | `PH7_PRIVATE int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 1161 | `{` |
|      ! 0 | 1162 | `	if( nArg > 0 ){` |
|      ! 0 | 1163 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - | 1164 | `			const char *zData;` |
|      ! 0 | 1165 | `			int iLen = 0;` |
|        - | 1166 | `			/* Print exit message */` |
|      ! 0 | 1167 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 | 1168 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 | 1169 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - | 1170 | `			sxi32 iExitStatus;` |
|        - | 1171 | `			/* Record exit status code */` |
|      ! 0 | 1172 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 | 1173 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 | 1174 | `		}` |
|      ! 0 | 1175 | `	}` |
|        - | 1176 | `	/* Request a VM-wide halt (see PH7_OP_HALT) and abort processing` |
|        - | 1177 | `	 * immediately; the abort unwinds enclosing frames and execution units.` |
|        - | 1178 | `	 */` |
|      ! 0 | 1179 | `	pCtx->pVm->bHaltRequested = 1;` |
|      ! 0 | 1180 | `	return PH7_ABORT;` |
|      ! 0 | 1181 | `}` |
|        - | 1182 | `/*` |
|        - | 1183 | ` * Section:` |
|        - | 1184 | ` *  Version,Credits and Copyright related functions.` |
|        - | 1185 | ` * Status:` |
|        - | 1186 | ` *    Stable.` |
|        - | 1187 | ` */` |
|        - | 1188 | `/*` |
|        - | 1189 | ` * string ph7version(void)` |
|        - | 1190 | ` *  Returns the running version of the PH7 version.` |
|        - | 1191 | ` * Parameters` |
|        - | 1192 | ` *  None` |
|        - | 1193 | ` * Return` |
|        - | 1194 | ` * Current PH7 version.` |
|        - | 1195 | ` */` |
|        2 | 1196 | `PH7_PRIVATE int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1197 | `{` |
|        1 | 1198 | `	SXUNUSED(nArg);` |
|        1 | 1199 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 1200 | `	/* Current engine version */` |
|        3 | 1201 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 1202 | `	return PH7_OK;` |
|        1 | 1203 | `}` |
|        - | 1204 | `/*` |
|        - | 1205 | ` * string phpversion([ string $extension ])` |
|        - | 1206 | ` *  Returns the PHP-compatibility version PHL advertises (see PHP_COMPAT_VERSION).` |
|        - | 1207 | ` * Parameters` |
|        - | 1208 | ` *  $extension (optional): an extension name. PHL has no extension registry, so any` |
|        - | 1209 | ` *  argument yields NULL (PHP returns FALSE for an unknown extension).` |
|        - | 1210 | ` * Return` |
|        - | 1211 | ` *  The PHP-compat version string, or NULL when called with an extension argument.` |
|        - | 1212 | ` */` |
|        4 | 1213 | `PH7_PRIVATE int vm_builtin_phpversion(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1214 | `{` |
|        2 | 1215 | `	SXUNUSED(apArg); /* cc warning */` |
|        5 | 1216 | `	if( nArg > 0 ){` |
|      ! 0 | 1217 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1218 | `		return PH7_OK;` |
|        - | 1219 | `	}` |
|        5 | 1220 | `	ph7_result_string(pCtx,PHP_COMPAT_VERSION,(int)sizeof(PHP_COMPAT_VERSION) - 1);` |
|        5 | 1221 | `	return PH7_OK;` |
|        3 | 1222 | `}` |
|        - | 1223 | `/*` |
|        - | 1224 | ` * string php_sapi_name(void)` |
|        - | 1225 | ` *  Returns the type of interface (SAPI) PHL is running under.` |
|        - | 1226 | ` * Parameters` |
|        - | 1227 | ` *  None` |
|        - | 1228 | ` * Return` |
|        - | 1229 | ` *  "cli-server" while serving an HTTP request via the built-in -S server, "cli" otherwise.` |
|        - | 1230 | ` */` |
|        2 | 1231 | `PH7_PRIVATE int vm_builtin_php_sapi_name(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1232 | `{` |
|        3 | 1233 | `	const char *zSapi = pCtx->pVm->bHttpContext ? "cli-server" : "cli";` |
|        1 | 1234 | `	SXUNUSED(nArg);` |
|        1 | 1235 | `	SXUNUSED(apArg); /* cc warning */` |
|        3 | 1236 | `	ph7_result_string(pCtx,zSapi,-1);` |
|        3 | 1237 | `	return PH7_OK;` |
|        1 | 1238 | `}` |
|        - | 1239 | `/*` |
|        - | 1240 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 1241 | ` */` |
|        - | 1242 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 1243 | ` "<html><head>"\` |
|        - | 1244 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 1245 | ` "<style type=\"text/css\">"\` |
|        - | 1246 | ` "div {"\` |
|        - | 1247 | `     "border: 1px solid #cccccc;"\` |
|        - | 1248 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 1249 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 1250 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 1251 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 1252 | `     "-webkit-border-radius: 10px;"\` |
|        - | 1253 | `     "-o-border-radius: 10px;"\` |
|        - | 1254 | `     "border-radius: 10px;"\` |
|        - | 1255 | `     "padding-left: 2em;"\` |
|        - | 1256 | `     "background-color: white;"\` |
|        - | 1257 | `     "margin-left: auto;"\` |
|        - | 1258 | `     "font-family: verdana;"\` |
|        - | 1259 | `     "padding-right: 2em;"\` |
|        - | 1260 | `     "margin-right: auto;"\` |
|        - | 1261 | `     "}"\` |
|        - | 1262 | `     "body {"\` |
|        - | 1263 | `     "padding: 0.2em;"\` |
|        - | 1264 | `     "font-style: normal;"\` |
|        - | 1265 | `     "font-size: medium;"\` |
|        - | 1266 | `     "background-color: #f2f2f2;"\` |
|        - | 1267 | `     "}"\` |
|        - | 1268 | `     "hr {"\` |
|        - | 1269 | `     "border-style: solid none none;"\` |
|        - | 1270 | `     "border-width: 1px medium medium;"\` |
|        - | 1271 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 1272 | `     "height: 1px;"\` |
|        - | 1273 | `     "}"\` |
|        - | 1274 | `     "a {"\` |
|        - | 1275 | `     "color: #3366cc;"\` |
|        - | 1276 | `     "text-decoration: none;"\` |
|        - | 1277 | `     "}"\` |
|        - | 1278 | `     "a:hover {"\` |
|        - | 1279 | `     "color: #999999;"\` |
|        - | 1280 | `     "}"\` |
|        - | 1281 | `     "a:active {"\` |
|        - | 1282 | `     "color: #663399;"\` |
|        - | 1283 | `     "}"\` |
|        - | 1284 | `     "h1 {"\` |
|        - | 1285 | `     "margin: 0;"\` |
|        - | 1286 | `     "padding: 0;"\` |
|        - | 1287 | `     "font-family: Verdana;"\` |
|        - | 1288 | `     "font-weight: bold;"\` |
|        - | 1289 | `     "font-style: normal;"\` |
|        - | 1290 | `     "font-size: medium;"\` |
|        - | 1291 | `     "text-transform: capitalize;"\` |
|        - | 1292 | `     "color: #0a328c;"\` |
|        - | 1293 | `     "}"\` |
|        - | 1294 | `     "p {"\` |
|        - | 1295 | `     "margin: 0 auto;"\` |
|        - | 1296 | `     "font-size: medium;"\` |
|        - | 1297 | `     "font-style: normal;"\` |
|        - | 1298 | `     "font-family: verdana;"\` |
|        - | 1299 | `     "}"\` |
|        - | 1300 | `"</style></head><body>"\` |
|        - | 1301 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 1302 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 1303 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 1304 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 1305 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 1306 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 1307 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 1308 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 1309 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 1310 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 1311 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 1312 |  |
|        - | 1313 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 1314 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 1315 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 1316 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 1317 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 1318 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 1319 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 1320 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 1321 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 1322 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 1323 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 1324 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 1325 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 1326 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 1327 |  |
|        - | 1328 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 1329 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 1330 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 1331 | `"&nbsp;*<br>"\` |
|        - | 1332 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 1333 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 1334 | `"&nbsp;* are met:<br>"\` |
|        - | 1335 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 1336 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 1337 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 1338 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 1339 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 1340 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 1341 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 1342 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 1343 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 1344 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 1345 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 1346 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 1347 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 1348 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 1349 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 1350 | `"&nbsp;*<br>"\` |
|        - | 1351 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 1352 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 1353 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 1354 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 1355 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 1356 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 1357 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 1358 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 1359 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 1360 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 1361 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 1362 | `"&nbsp;*/<br>"\` |
|        - | 1363 | `"</span></small></small></p>"\` |
|        - | 1364 | `"</div></body></html>"` |
|        - | 1365 | `/*` |
|        - | 1366 | ` * bool ph7credits(void)` |
|        - | 1367 | ` * bool ph7info(void)` |
|        - | 1368 | ` * bool ph7copyright(void)` |
|        - | 1369 | ` *  Prints out the credits for PH7 engine` |
|        - | 1370 | ` * Parameters` |
|        - | 1371 | ` *  None` |
|        - | 1372 | ` * Return` |
|        - | 1373 | ` *  Always TRUE` |
|        - | 1374 | ` */` |
|        2 | 1375 | `PH7_PRIVATE int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1376 | `{` |
|        3 | 1377 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 1378 | `	/* Expand the HTML page above*/` |
|        3 | 1379 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 1380 | `	ph7_context_output_format(` |
|        1 | 1381 | `		pCtx,` |
|        - | 1382 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 1383 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 1384 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 1385 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 1386 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 1387 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 1388 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 1389 | `#ifdef __WINNT__` |
|        - | 1390 | `		"Windows NT"` |
|        - | 1391 | `#elif defined(__UNIXES__)` |
|        - | 1392 | `		"UNIX-Like"` |
|        - | 1393 | `#else` |
|        - | 1394 | `		"Other OS"` |
|        - | 1395 | `#endif` |
|        - | 1396 | `		);` |
|        3 | 1397 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 1398 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 1399 | `	SXUNUSED(apArg);` |
|        - | 1400 | `	/* Return TRUE */` |
|        - | 1401 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 1402 | `	return PH7_OK;` |
|        1 | 1403 | `}` |
|        - | 1404 | `/*` |
|        - | 1405 | ` * Section:` |
|        - | 1406 | ` *    URL related routines.` |
|        - | 1407 | ` * Status:` |
|        - | 1408 | ` *    Stable.` |
|        - | 1409 | ` */` |
|        - | 1410 | `/*` |
|        - | 1411 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 1412 | ` *  Parse a URL and return its fields.` |
|        - | 1413 | ` * Parameters` |
|        - | 1414 | ` *  $url` |
|        - | 1415 | ` *   The URL to parse.` |
|        - | 1416 | ` * $component` |
|        - | 1417 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 1418 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 1419 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 1420 | ` *  in which case the return value will be an integer).` |
|        - | 1421 | ` * Return` |
|        - | 1422 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 1423 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 1424 | ` *  this array are:` |
|        - | 1425 | ` *   scheme - e.g. http` |
|        - | 1426 | ` *   host` |
|        - | 1427 | ` *   port` |
|        - | 1428 | ` *   user` |
|        - | 1429 | ` *   pass` |
|        - | 1430 | ` *   path` |
|        - | 1431 | ` *   query - after the question mark ?` |
|        - | 1432 | ` *   fragment - after the hashmark #` |
|        - | 1433 | ` * Note:` |
|        - | 1434 | ` *  FALSE is returned on failure.` |
|        - | 1435 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 1436 | ` *  with the standard PHP engine.` |
|        - | 1437 | ` */` |
|        - | 1438 | `/*` |
|        - | 1439 | ` * parse_url() component set.` |
|        - | 1440 | ` *` |
|        - | 1441 | ` * A bare SyString cannot express "present but empty", which php needs:` |
|        - | 1442 | ` * parse_url("") is ['path'=>''] and parse_url("?") is ['query'=>''], both` |
|        - | 1443 | ` * distinct from the component being absent. So presence is tracked separately.` |
|        - | 1444 | ` */` |
|        - | 1445 | `typedef struct VmUrlParts VmUrlParts;` |
|        - | 1446 | `struct VmUrlParts` |
|        - | 1447 | `{` |
|        - | 1448 | `	SyString sScheme,sUser,sPass,sHost,sPath,sQuery,sFragment;` |
|        - | 1449 | `	int iPort;     /* Resolved port, meaningful only when bPort is set */` |
|        - | 1450 | `	sxu8 bScheme,bUser,bPass,bHost,bPort,bPath,bQuery,bFragment;` |
|        - | 1451 | `};` |
|      256 | 1452 | `static int VmUrlIsAlnum(int c)` |
|        1 | 1453 | `{` |
|      257 | 1454 | `	return (c >= '0' && c <= '9') \|\| (c >= 'a' && c <= 'z') \|\| (c >= 'A' && c <= 'Z');` |
|        1 | 1455 | `}` |
|        4 | 1456 | `static int VmUrlIsAlpha(int c)` |
|        1 | 1457 | `{` |
|        5 | 1458 | `	return (c >= 'a' && c <= 'z') \|\| (c >= 'A' && c <= 'Z');` |
|        1 | 1459 | `}` |
|        - | 1460 | `/* scheme = ALNUM *( ALNUM / "+" / "-" / "." ) -- php accepts a digit first. */` |
|      256 | 1461 | `static int VmUrlIsSchemeByte(int c)` |
|        1 | 1462 | `{` |
|      257 | 1463 | `	return VmUrlIsAlnum(c) \|\| c == '+' \|\| c == '-' \|\| c == '.';` |
|        1 | 1464 | `}` |
|        - | 1465 | `/*` |
|        - | 1466 | ` * Resolve the port span that followed the ':' in an authority.` |
|        - | 1467 | ` *` |
|        - | 1468 | ` * Returns 1 (usable port in *piPort), 0 (the span is empty, so there is simply` |
|        - | 1469 | ` * no port) or -1 (php rejects the whole URL). php is lenient about what follows` |
|        - | 1470 | ` * the digits -- ":8a" and ":8 0" both yield 8 -- but demands at least one digit` |
|        - | 1471 | ` * and a value that fits a port, so ":abc", ":-80" and ":65536" are all failures.` |
|        - | 1472 | ` */` |
|       42 | 1473 | `static int VmUrlParsePort(const char *z,int n,int *piPort)` |
|        1 | 1474 | `{` |
|       43 | 1475 | `	int i = 0,iVal = 0,nDigit = 0;` |
|       43 | 1476 | `	if( n < 1 ){` |
|      ! 0 | 1477 | `		return 0;` |
|        - | 1478 | `	}` |
|       64 | 1479 | `	while( i < n && (z[i] == ' ' \|\| z[i] == '\t') ){` |
|      ! 0 | 1480 | `		i++;` |
|      ! 0 | 1481 | `	}` |
|       43 | 1482 | `	if( i < n && (z[i] == '+' \|\| z[i] == '-') ){` |
|      ! 0 | 1483 | `		if( z[i] == '-' ){` |
|      ! 0 | 1484 | `			return -1;` |
|        - | 1485 | `		}` |
|      ! 0 | 1486 | `		i++;` |
|      ! 0 | 1487 | `	}` |
|      161 | 1488 | `	while( i < n && z[i] >= '0' && z[i] <= '9' ){` |
|      119 | 1489 | `		iVal = iVal * 10 + (z[i] - '0');` |
|      119 | 1490 | `		if( iVal > 65535 ){` |
|      ! 0 | 1491 | `			return -1; /* Out of range, and this also caps the accumulator */` |
|        - | 1492 | `		}` |
|      119 | 1493 | `		nDigit++;` |
|      119 | 1494 | `		i++;` |
|        1 | 1495 | `	}` |
|       43 | 1496 | `	if( nDigit < 1 ){` |
|        3 | 1497 | `		return -1;` |
|        - | 1498 | `	}` |
|       41 | 1499 | `	*piPort = iVal;` |
|       41 | 1500 | `	return 1;` |
|       22 | 1501 | `}` |
|        - | 1502 | `/*` |
|        - | 1503 | ` * Split an authority -- "[user[:pass]@]host[:port]" -- into pOut.` |
|        - | 1504 | ` * Returns 0 for the malformed input php reports as FALSE.` |
|        - | 1505 | ` */` |
|       66 | 1506 | `static int VmUrlParseAuthority(const char *z,int n,VmUrlParts *pOut,int bPortKnown)` |
|        1 | 1507 | `{` |
|        - | 1508 | `	const char *zHost;` |
|       67 | 1509 | `	int i,iAt = -1,iColon = -1,iSep = -1,nHost,iPort = 0,rc;` |
|        - | 1510 | `	/* php splits the credentials at the LAST '@' ("//a@b@c" is user "a@b") */` |
|      699 | 1511 | `	for( i = 0 ; i < n ; ++i ){` |
|      633 | 1512 | `		if( z[i] == '@' ){` |
|       25 | 1513 | `			iAt = i;` |
|       12 | 1514 | `		}` |
|      317 | 1515 | `	}` |
|       67 | 1516 | `	if( iAt >= 0 ){` |
|        - | 1517 | `		/* and the user from the password at the FIRST ':' before it */` |
|      109 | 1518 | `		for( i = 0 ; i < iAt ; ++i ){` |
|      107 | 1519 | `			if( z[i] == ':' ){` |
|       23 | 1520 | `				iColon = i;` |
|       23 | 1521 | `				break;` |
|        - | 1522 | `			}` |
|       43 | 1523 | `		}` |
|       25 | 1524 | `		if( iColon >= 0 ){` |
|       23 | 1525 | `			SyStringInitFromBuf(&pOut->sUser,z,(sxu32)iColon);` |
|       23 | 1526 | `			SyStringInitFromBuf(&pOut->sPass,&z[iColon+1],(sxu32)(iAt - iColon - 1));` |
|       23 | 1527 | `			pOut->bUser = pOut->bPass = 1;` |
|       12 | 1528 | `		}else{` |
|        3 | 1529 | `			SyStringInitFromBuf(&pOut->sUser,z,(sxu32)iAt);` |
|        3 | 1530 | `			pOut->bUser = 1;` |
|        - | 1531 | `		}` |
|       25 | 1532 | `		z += iAt + 1;` |
|       25 | 1533 | `		n -= iAt + 1;` |
|       12 | 1534 | `	}` |
|       67 | 1535 | `	zHost = z;` |
|       67 | 1536 | `	nHost = n;` |
|       67 | 1537 | `	if( !(n > 1 && z[0] == '[' && z[n-1] == ']') ){` |
|        - | 1538 | `		/* Unless a bracketed IPv6 literal fills the WHOLE authority -- in which` |
|        - | 1539 | `		 * case php keeps the brackets in "host" and scans no port, so every ':'` |
|        - | 1540 | `		 * inside belongs to the address -- the port hangs off the LAST ':'.` |
|        - | 1541 | `		 * php decides that on the first and last byte alone, which is why` |
|        - | 1542 | `		 * "//[:]\|]" is a single host while "//[::1]:8080" splits a port off. */` |
|      507 | 1543 | `		for( i = 0 ; i < n ; ++i ){` |
|      443 | 1544 | `			if( z[i] == ':' ){` |
|       43 | 1545 | `				iSep = i;` |
|       21 | 1546 | `			}` |
|      222 | 1547 | `		}` |
|       65 | 1548 | `		if( iSep >= 0 ){` |
|        - | 1549 | `			/* The host is trimmed at the ':' whether or not the port was already` |
|        - | 1550 | `			 * resolved by the caller. */` |
|       43 | 1551 | `			nHost = iSep;` |
|       43 | 1552 | `			if( !bPortKnown ){` |
|       37 | 1553 | `				rc = VmUrlParsePort(&z[iSep+1],n - iSep - 1,&iPort);` |
|       37 | 1554 | `				if( rc < 0 ){` |
|        3 | 1555 | `					return 0;` |
|        - | 1556 | `				}` |
|       35 | 1557 | `				if( rc > 0 ){` |
|       35 | 1558 | `					pOut->iPort = iPort;` |
|       35 | 1559 | `					pOut->bPort = 1;` |
|       17 | 1560 | `				}` |
|       17 | 1561 | `			}` |
|       20 | 1562 | `		}` |
|       31 | 1563 | `	}` |
|       65 | 1564 | `	if( nHost < 1 ){` |
|        - | 1565 | `		/* php requires a non-empty host once an authority is in play, which is` |
|        - | 1566 | `		 * what makes "//", "http://" and ":80" all FALSE. */` |
|        9 | 1567 | `		return 0;` |
|        - | 1568 | `	}` |
|       57 | 1569 | `	SyStringInitFromBuf(&pOut->sHost,zHost,(sxu32)nHost);` |
|       57 | 1570 | `	pOut->bHost = 1;` |
|       57 | 1571 | `	return 1;` |
|       34 | 1572 | `}` |
|        - | 1573 | `/*` |
|        - | 1574 | ` * Split "path[?query][#fragment]". The fragment is taken FIRST and the query` |
|        - | 1575 | ` * only from what precedes it, so "#a?b" is a fragment of "a?b" with no query.` |
|        - | 1576 | ` */` |
|       80 | 1577 | `static void VmUrlParsePath(const char *z,int n,VmUrlParts *pOut)` |
|        1 | 1578 | `{` |
|       81 | 1579 | `	int i,iEnd = n;` |
|      547 | 1580 | `	for( i = 0 ; i < n ; ++i ){` |
|      501 | 1581 | `		if( z[i] == '#' ){` |
|       35 | 1582 | `			SyStringInitFromBuf(&pOut->sFragment,&z[i+1],(sxu32)(n - i - 1));` |
|       35 | 1583 | `			pOut->bFragment = 1;` |
|       35 | 1584 | `			iEnd = i;` |
|       35 | 1585 | `			break;` |
|        - | 1586 | `		}` |
|      234 | 1587 | `	}` |
|      393 | 1588 | `	for( i = 0 ; i < iEnd ; ++i ){` |
|      347 | 1589 | `		if( z[i] == '?' ){` |
|       35 | 1590 | `			SyStringInitFromBuf(&pOut->sQuery,&z[i+1],(sxu32)(iEnd - i - 1));` |
|       35 | 1591 | `			pOut->bQuery = 1;` |
|       35 | 1592 | `			iEnd = i;` |
|       35 | 1593 | `			break;` |
|        - | 1594 | `		}` |
|      157 | 1595 | `	}` |
|       81 | 1596 | `	if( iEnd > 0 ){` |
|       71 | 1597 | `		SyStringInitFromBuf(&pOut->sPath,z,(sxu32)iEnd);` |
|       71 | 1598 | `		pOut->bPath = 1;` |
|       35 | 1599 | `	}` |
|       81 | 1600 | `}` |
|        - | 1601 | `/* Parse "host[:port]" followed by an optional path/query/fragment. */` |
|       66 | 1602 | `static int VmUrlAuthorityThenPath(const char *z,int n,VmUrlParts *pOut,int bPortKnown)` |
|        1 | 1603 | `{` |
|       67 | 1604 | `	int i,iEnd = n;` |
|      699 | 1605 | `	for( i = 0 ; i < n ; ++i ){` |
|      683 | 1606 | `		if( z[i] == '/' \|\| z[i] == '?' \|\| z[i] == '#' ){` |
|       51 | 1607 | `			iEnd = i;` |
|       51 | 1608 | `			break;` |
|        - | 1609 | `		}` |
|      317 | 1610 | `	}` |
|       67 | 1611 | `	if( !VmUrlParseAuthority(z,iEnd,pOut,bPortKnown) ){` |
|       11 | 1612 | `		return 0;` |
|        - | 1613 | `	}` |
|       57 | 1614 | `	if( iEnd < n ){` |
|       47 | 1615 | `		VmUrlParsePath(&z[iEnd],n - iEnd,pOut);` |
|       23 | 1616 | `	}` |
|       57 | 1617 | `	return 1;` |
|       34 | 1618 | `}` |
|        - | 1619 | `/*` |
|        - | 1620 | ` * Resolve the port php took from the FIRST ':' of a host:port URL.` |
|        - | 1621 | ` *` |
|        - | 1622 | ` * php reads the port straight off that colon before it works out where the host` |
|        - | 1623 | ` * ends, so the digits can even belong to what becomes the path: parse_url()` |
|        - | 1624 | ` * reports port 1 for "a/:1", whose path is "/:1". Mirroring the order keeps` |
|        - | 1625 | ` * that quirk. Returns 0 for a port php rejects.` |
|        - | 1626 | ` */` |
|        6 | 1627 | `static int VmUrlPreparePort(const char *z,int k,int nEnd,VmUrlParts *pOut)` |
|        1 | 1628 | `{` |
|        7 | 1629 | `	int iPort = 0;` |
|        7 | 1630 | `	int rc = VmUrlParsePort(&z[k+1],nEnd - k - 1,&iPort);` |
|        7 | 1631 | `	if( rc < 0 ){` |
|      ! 0 | 1632 | `		return 0;` |
|        - | 1633 | `	}` |
|        7 | 1634 | `	if( rc > 0 ){` |
|        7 | 1635 | `		pOut->iPort = iPort;` |
|        7 | 1636 | `		pOut->bPort = 1;` |
|        3 | 1637 | `	}` |
|        7 | 1638 | `	return 1;` |
|        4 | 1639 | `}` |
|        - | 1640 | `/*` |
|        - | 1641 | ` * php's URL parser, as parse_url() needs it. Returns 0 where php returns FALSE.` |
|        - | 1642 | ` *` |
|        - | 1643 | ` * PH7_VmHttpSplitURI cannot serve here: it is an HTTP REQUEST-target splitter` |
|        - | 1644 | ` * (the HTTP server and filter_var share it) and assumes the input is` |
|        - | 1645 | ` * authority-first, so it read "a" as a host and "mailto:me@x.com" as` |
|        - | 1646 | ` * user:pass@host. php instead decides authority-vs-path up front: only a "//",` |
|        - | 1647 | ` * with or without a scheme before it, introduces an authority.` |
|        - | 1648 | ` */` |
|      104 | 1649 | `static int VmUrlSplit(const char *z,int n,VmUrlParts *pOut)` |
|        1 | 1650 | `{` |
|      105 | 1651 | `	int i,k = -1,bScheme,bPortForm = 0,nPortEnd = 0;` |
|      105 | 1652 | `	SyZero(pOut,sizeof(VmUrlParts));` |
|        - | 1653 | `	/* A leading "//" settles it before any colon is considered: the authority` |
|        - | 1654 | `	 * starts after the slashes. Reading the colon first turned "//h:80" into a` |
|        - | 1655 | `	 * host called "//h" and "//[::1]" into a path. */` |
|      105 | 1656 | `	if( n >= 2 && z[0] == '/' && z[1] == '/' ){` |
|       13 | 1657 | `		return VmUrlAuthorityThenPath(&z[2],n - 2,pOut,0);` |
|        - | 1658 | `	}` |
|      441 | 1659 | `	for( i = 0 ; i < n ; ++i ){` |
|      419 | 1660 | `		if( z[i] == ':' ){` |
|       71 | 1661 | `			k = i;` |
|       71 | 1662 | `			break;` |
|        - | 1663 | `		}` |
|      175 | 1664 | `	}` |
|       93 | 1665 | `	if( k == 0 && n == 1 ){` |
|        - | 1666 | `		/* A lone ":" is an EMPTY scheme, which php rejects outright -- unlike` |
|        - | 1667 | `		 * ":a" or "::", which are simply paths. */` |
|        3 | 1668 | `		return 0;` |
|        - | 1669 | `	}` |
|       91 | 1670 | `	bScheme = k > 0;` |
|      347 | 1671 | `	for( i = 0 ; bScheme && i < k ; ++i ){` |
|      257 | 1672 | `		if( !VmUrlIsSchemeByte((unsigned char)z[i]) ){` |
|      ! 0 | 1673 | `			bScheme = 0;` |
|      ! 0 | 1674 | `		}` |
|      129 | 1675 | `	}` |
|       91 | 1676 | `	if( bScheme && k + 1 == n ){` |
|        - | 1677 | `		/* "x:" -- the scheme is the whole URL */` |
|        3 | 1678 | `		SyStringInitFromBuf(&pOut->sScheme,z,(sxu32)k);` |
|        3 | 1679 | `		pOut->bScheme = 1;` |
|        3 | 1680 | `		return 1;` |
|        - | 1681 | `	}` |
|        - | 1682 | `	/* Decide whether that ':' introduces a PORT rather than a scheme: at least` |
|        - | 1683 | `	 * one digit, running to the end of the string or to a '/'. That is what` |
|        - | 1684 | `	 * makes "a:80" a host:port while "a:80?q" is a scheme with path "80", and` |
|        - | 1685 | `	 * it holds however the ':' is reached -- ":1" and "/:1" are both authorities` |
|        - | 1686 | `	 * with an empty host, which php rejects. php caps the run, so a long number` |
|        - | 1687 | `	 * stays a path: "a:123456" is a path, "a:99999" an out-of-range port. */` |
|       89 | 1688 | `	if( k >= 0 ){` |
|       67 | 1689 | `		int p = k + 1;` |
|       67 | 1690 | `		int bBeforeQuery = 1;` |
|       67 | 1691 | `		nPortEnd = k + 1;` |
|        - | 1692 | `		/* A ':' that sits inside a query or fragment is just data: "?:1" is a` |
|        - | 1693 | `		 * query of ":1", not an authority with an empty host. */` |
|      321 | 1694 | `		for( i = 0 ; i < k ; ++i ){` |
|      255 | 1695 | `			if( z[i] == '?' \|\| z[i] == '#' ){` |
|      ! 0 | 1696 | `				bBeforeQuery = 0;` |
|      ! 0 | 1697 | `				break;` |
|        - | 1698 | `			}` |
|      128 | 1699 | `		}` |
|       77 | 1700 | `		while( p < n && z[p] >= '0' && z[p] <= '9' ){` |
|       11 | 1701 | `			p++;` |
|        1 | 1702 | `		}` |
|       67 | 1703 | `		if( bBeforeQuery && p > k + 1 && (p >= n \|\| z[p] == '/') && (p - k) < 7 ){` |
|        7 | 1704 | `			bPortForm = 1;` |
|        7 | 1705 | `			nPortEnd = p;` |
|        3 | 1706 | `		}` |
|       33 | 1707 | `	}` |
|       89 | 1708 | `	if( !bScheme ){` |
|       25 | 1709 | `		if( bPortForm ){` |
|        3 | 1710 | `			if( !VmUrlPreparePort(z,k,nPortEnd,pOut) ){` |
|      ! 0 | 1711 | `				return 0;` |
|        - | 1712 | `			}` |
|        3 | 1713 | `			return VmUrlAuthorityThenPath(z,n,pOut,1);` |
|        - | 1714 | `		}` |
|       23 | 1715 | `		VmUrlParsePath(z,n,pOut);` |
|       23 | 1716 | `		if( !pOut->bPath && !pOut->bQuery && !pOut->bFragment ){` |
|        - | 1717 | `			/* php reports the empty URL as an empty PATH, not as no components */` |
|        3 | 1718 | `			SyStringInitFromBuf(&pOut->sPath,z,0);` |
|        3 | 1719 | `			pOut->bPath = 1;` |
|        1 | 1720 | `		}` |
|       23 | 1721 | `		return 1;` |
|        - | 1722 | `	}` |
|       65 | 1723 | `	if( bPortForm ){` |
|        5 | 1724 | `		if( !VmUrlPreparePort(z,k,nPortEnd,pOut) ){` |
|      ! 0 | 1725 | `			return 0;` |
|        - | 1726 | `		}` |
|        5 | 1727 | `		return VmUrlAuthorityThenPath(z,n,pOut,1);` |
|        - | 1728 | `	}` |
|       61 | 1729 | `	SyStringInitFromBuf(&pOut->sScheme,z,(sxu32)k);` |
|       61 | 1730 | `	pOut->bScheme = 1;` |
|       61 | 1731 | `	if( z[k+1] == '/' && k + 2 < n && z[k+2] == '/' ){` |
|       52 | 1732 | `		if( k + 3 < n && z[k+3] == '/' && pOut->sScheme.nByte == 4` |
|        4 | 1733 | `		 && (z[0]=='f'\|\|z[0]=='F') && (z[1]=='i'\|\|z[1]=='I')` |
|        5 | 1734 | `		 && (z[2]=='l'\|\|z[2]=='L') && (z[3]=='e'\|\|z[3]=='E') ){` |
|        - | 1735 | `			/* file:/// has no authority: the path starts at the third slash,` |
|        - | 1736 | `			 * except that a "c:" drive letter swallows it (file:///c:/x is the` |
|        - | 1737 | `			 * path "c:/x"). A '\|' in place of the ':' does NOT count. */` |
|        5 | 1738 | `			int iBase = k + 3;` |
|        5 | 1739 | `			if( iBase + 2 < n && VmUrlIsAlpha((unsigned char)z[iBase+1]) && z[iBase+2] == ':' ){` |
|      ! 0 | 1740 | `				iBase++;` |
|      ! 0 | 1741 | `			}` |
|        5 | 1742 | `			VmUrlParsePath(&z[iBase],n - iBase,pOut);` |
|        5 | 1743 | `			return 1;` |
|        - | 1744 | `		}` |
|       49 | 1745 | `		return VmUrlAuthorityThenPath(&z[k+3],n - k - 3,pOut,0);` |
|        - | 1746 | `	}` |
|        - | 1747 | `	/* "mailto:me@x.com", "x:y", "http:/x": everything after the ':' is a path */` |
|        9 | 1748 | `	VmUrlParsePath(&z[k+1],n - k - 1,pOut);` |
|        9 | 1749 | `	return 1;` |
|       53 | 1750 | `}` |
|        - | 1751 | `/*` |
|        - | 1752 | ` * Emit one parsed component into pValue, replacing control bytes with '_'.` |
|        - | 1753 | ` *` |
|        - | 1754 | ` * php runs php_replace_controlchars_ex over every string component it returns,` |
|        - | 1755 | ` * so a URL carrying a raw NUL, newline or DEL cannot smuggle it through into` |
|        - | 1756 | ` * whatever the caller splices the component into (a header, a log line, a` |
|        - | 1757 | ` * redirect). Bytes >= 0x80 are deliberately left alone -- php only folds the` |
|        - | 1758 | ` * ASCII control range.` |
|        - | 1759 | ` */` |
|      164 | 1760 | `static void VmUrlSetComponent(ph7_value *pValue,const SyString *pComp)` |
|        1 | 1761 | `{` |
|      165 | 1762 | `	const char *z = pComp->zString;` |
|      165 | 1763 | `	sxu32 n = pComp->nByte,i,iRun = 0;` |
|      165 | 1764 | `	if( n < 1 \|\| z == 0 ){` |
|        3 | 1765 | `		ph7_value_string(pValue,"",0);` |
|        3 | 1766 | `		return;` |
|        - | 1767 | `	}` |
|      955 | 1768 | `	for( i = 0 ; i < n ; ++i ){` |
|      793 | 1769 | `		unsigned char c = (unsigned char)z[i];` |
|      793 | 1770 | `		if( c < 0x20 \|\| c == 0x7f ){` |
|        3 | 1771 | `			if( i > iRun ){` |
|        3 | 1772 | `				ph7_value_string(pValue,&z[iRun],(int)(i - iRun));` |
|        1 | 1773 | `			}` |
|        3 | 1774 | `			ph7_value_string(pValue,"_",1);` |
|        3 | 1775 | `			iRun = i + 1;` |
|        1 | 1776 | `		}` |
|      397 | 1777 | `	}` |
|      163 | 1778 | `	if( n > iRun ){` |
|      163 | 1779 | `		ph7_value_string(pValue,&z[iRun],(int)(n - iRun));` |
|       81 | 1780 | `	}` |
|       83 | 1781 | `}` |
|      104 | 1782 | `PH7_PRIVATE int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1783 | `{` |
|        - | 1784 | `	const char *zStr; /* Input string */` |
|        - | 1785 | `	VmUrlParts sUrl;  /* Parse of the given URI */` |
|        - | 1786 | `	SyString *pComp;` |
|        - | 1787 | `	int bHave;` |
|        - | 1788 | `	int nLen;` |
|      105 | 1789 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 1790 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 1791 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 1792 | `		return PH7_OK;` |
|        - | 1793 | `	}` |
|        - | 1794 | `	/* Extract the given URI. An empty string is NOT a failure: php parses it as` |
|        - | 1795 | `	 * an empty path. */` |
|      105 | 1796 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|      105 | 1797 | `	if( nLen < 0 ){` |
|      ! 0 | 1798 | `		nLen = 0;` |
|      ! 0 | 1799 | `	}` |
|      105 | 1800 | `	if( !VmUrlSplit(zStr,nLen,&sUrl) ){` |
|        - | 1801 | `		/* Malformed input,return FALSE */` |
|       13 | 1802 | `		ph7_result_bool(pCtx,0);` |
|       13 | 1803 | `		return PH7_OK;` |
|        - | 1804 | `	}` |
|      103 | 1805 | `	if( nArg > 1 && ph7_value_to_int(apArg[1]) >= 0 ){` |
|        - | 1806 | `		/* Refer to constant.c for constants values (php's PHP_URL_* are 0-based;` |
|        - | 1807 | `		 * PHL used to number them from 1, so every literal component id selected` |
|        - | 1808 | `		 * the WRONG field -- and the constants' own tests only asserted "%d",` |
|        - | 1809 | `		 * which any numbering satisfies). A negative id means "the whole array",` |
|        - | 1810 | `		 * which is what the default $component = -1 relies on. */` |
|       27 | 1811 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|       27 | 1812 | `		pComp = 0;` |
|       27 | 1813 | `		bHave = 0;` |
|       27 | 1814 | `		switch(nComponent){` |
|        3 | 1815 | `		case 0: /* PHP_URL_SCHEME */   pComp = &sUrl.sScheme;   bHave = sUrl.bScheme;   break;` |
|        5 | 1816 | `		case 1: /* PHP_URL_HOST */     pComp = &sUrl.sHost;     bHave = sUrl.bHost;     break;` |
|        2 | 1817 | `		case 2: /* PHP_URL_PORT */` |
|        5 | 1818 | `			if( sUrl.bPort ){` |
|        5 | 1819 | `				ph7_result_int(pCtx,sUrl.iPort);` |
|        3 | 1820 | `			}else{` |
|      ! 0 | 1821 | `				ph7_result_null(pCtx);` |
|        - | 1822 | `			}` |
|        5 | 1823 | `			return PH7_OK;` |
|        3 | 1824 | `		case 3: /* PHP_URL_USER */     pComp = &sUrl.sUser;     bHave = sUrl.bUser;     break;` |
|        3 | 1825 | `		case 4: /* PHP_URL_PASS */     pComp = &sUrl.sPass;     bHave = sUrl.bPass;     break;` |
|        3 | 1826 | `		case 5: /* PHP_URL_PATH */     pComp = &sUrl.sPath;     bHave = sUrl.bPath;     break;` |
|        5 | 1827 | `		case 6: /* PHP_URL_QUERY */    pComp = &sUrl.sQuery;    bHave = sUrl.bQuery;    break;` |
|        5 | 1828 | `		case 7: /* PHP_URL_FRAGMENT */ pComp = &sUrl.sFragment; bHave = sUrl.bFragment; break;` |
|        1 | 1829 | `		default:` |
|        4 | 1830 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|        - | 1831 | `				"parse_url(): Argument #2 ($component) must be a valid URL component identifier, %d given",` |
|        1 | 1832 | `				nComponent);` |
|        - | 1833 | `		}` |
|       21 | 1834 | `		if( bHave ){` |
|       19 | 1835 | `			ph7_value *pOut = ph7_context_new_scalar(pCtx);` |
|       19 | 1836 | `			if( pOut == 0 ){` |
|      ! 0 | 1837 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|      ! 0 | 1838 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 1839 | `				return PH7_OK;` |
|        - | 1840 | `			}` |
|       19 | 1841 | `			VmUrlSetComponent(pOut,pComp);` |
|       19 | 1842 | `			ph7_result_value(pCtx,pOut);` |
|       10 | 1843 | `		}else{` |
|        - | 1844 | `			/* No available value,return NULL */` |
|        3 | 1845 | `			ph7_result_null(pCtx);` |
|        - | 1846 | `		}` |
|       11 | 1847 | `	}else{` |
|        - | 1848 | `		ph7_value *pArray,*pValue;` |
|        - | 1849 | `		/* Return an associative array */` |
|       67 | 1850 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       67 | 1851 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       67 | 1852 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 1853 | `			/* Out of memory */` |
|      ! 0 | 1854 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 1855 | `			/* Return false */` |
|      ! 0 | 1856 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 1857 | `			return PH7_OK;` |
|        - | 1858 | `		}` |
|        - | 1859 | `		/* Fill the array, in php's key order. A component is emitted whenever it` |
|        - | 1860 | `		 * is PRESENT, even when empty -- parse_url("?") is ['query'=>'']. */` |
|       67 | 1861 | `		if( sUrl.bScheme ){` |
|       33 | 1862 | `			VmUrlSetComponent(pValue,&sUrl.sScheme);` |
|       33 | 1863 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|       33 | 1864 | `			ph7_value_reset_string_cursor(pValue);` |
|       16 | 1865 | `		}` |
|       67 | 1866 | `		if( sUrl.bHost ){` |
|       31 | 1867 | `			VmUrlSetComponent(pValue,&sUrl.sHost);` |
|       31 | 1868 | `			ph7_array_add_strkey_elem(pArray,"host",pValue);` |
|       31 | 1869 | `			ph7_value_reset_string_cursor(pValue);` |
|       15 | 1870 | `		}` |
|       67 | 1871 | `		if( sUrl.bPort ){` |
|       17 | 1872 | `			ph7_value_int(pValue,sUrl.iPort);` |
|       17 | 1873 | `			ph7_array_add_strkey_elem(pArray,"port",pValue);` |
|       17 | 1874 | `			ph7_value_reset_string_cursor(pValue);` |
|        8 | 1875 | `		}` |
|       67 | 1876 | `		if( sUrl.bUser ){` |
|        9 | 1877 | `			VmUrlSetComponent(pValue,&sUrl.sUser);` |
|        9 | 1878 | `			ph7_array_add_strkey_elem(pArray,"user",pValue);` |
|        9 | 1879 | `			ph7_value_reset_string_cursor(pValue);` |
|        4 | 1880 | `		}` |
|       67 | 1881 | `		if( sUrl.bPass ){` |
|        7 | 1882 | `			VmUrlSetComponent(pValue,&sUrl.sPass);` |
|        7 | 1883 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue);` |
|        7 | 1884 | `			ph7_value_reset_string_cursor(pValue);` |
|        3 | 1885 | `		}` |
|       67 | 1886 | `		if( sUrl.bPath ){` |
|       47 | 1887 | `			VmUrlSetComponent(pValue,&sUrl.sPath);` |
|       47 | 1888 | `			ph7_array_add_strkey_elem(pArray,"path",pValue);` |
|       47 | 1889 | `			ph7_value_reset_string_cursor(pValue);` |
|       23 | 1890 | `		}` |
|       67 | 1891 | `		if( sUrl.bQuery ){` |
|       13 | 1892 | `			VmUrlSetComponent(pValue,&sUrl.sQuery);` |
|       13 | 1893 | `			ph7_array_add_strkey_elem(pArray,"query",pValue);` |
|       13 | 1894 | `			ph7_value_reset_string_cursor(pValue);` |
|        6 | 1895 | `		}` |
|       67 | 1896 | `		if( sUrl.bFragment ){` |
|       13 | 1897 | `			VmUrlSetComponent(pValue,&sUrl.sFragment);` |
|       13 | 1898 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue);` |
|        6 | 1899 | `		}` |
|        - | 1900 | `		/* Return the created array */` |
|       67 | 1901 | `		ph7_result_value(pCtx,pArray);` |
|        - | 1902 | `		/* NOTE:` |
|        - | 1903 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 1904 | `		 * automatically as soon we return from this function.` |
|        - | 1905 | `		 */` |
|        - | 1906 | `	}` |
|        - | 1907 | `	/* All done */` |
|       87 | 1908 | `	return PH7_OK;` |
|       53 | 1909 | `}` |
|        - | 1910 |  |
|        - | 1911 | `/*` |
|        - | 1912 | ` * Section:` |
|        - | 1913 | ` *   Array related routines.` |
|        - | 1914 | ` * Status:` |
|        - | 1915 | ` *    Stable.` |
|        - | 1916 | ` * Note 2012-5-21 01:04:15:` |
|        - | 1917 | ` *  Array related functions that need access to the underlying` |
|        - | 1918 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 1919 | ` */` |
|        - | 1920 | `/*` |
|        - | 1921 | ` * The [compact()] function store it's state information in an instance` |
|        - | 1922 | ` * of the following structure.` |
|        - | 1923 | ` */` |
|        - | 1924 | `struct compact_data` |
|        - | 1925 | `{` |
|        - | 1926 | `	ph7_value *pArray;  /* Target array */` |
|        - | 1927 | `	int nRecCount;      /* Recursion count */` |
|        - | 1928 | `};` |
|        - | 1929 | `/*` |
|        - | 1930 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 1931 | ` */` |
|      ! 0 | 1932 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 1933 | `{` |
|      ! 0 | 1934 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 1935 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 1936 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 1937 | `	/* Act according to the hashmap value */` |
|      ! 0 | 1938 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 1939 | `		SyString sVar;` |
|      ! 0 | 1940 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 1941 | `		if( sVar.nByte > 0 ){` |
|        - | 1942 | `			/* Query the current frame */` |
|      ! 0 | 1943 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 1944 | `			/* ^` |
|        - | 1945 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 1946 | `			 */` |
|      ! 0 | 1947 | `			if( pKey ){` |
|        - | 1948 | `				/* Perform the insertion */` |
|      ! 0 | 1949 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 1950 | `			}` |
|      ! 0 | 1951 | `		}` |
|      ! 0 | 1952 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 1953 | `		int rc;` |
|        - | 1954 | `		/* Recursively traverse this array */` |
|      ! 0 | 1955 | `		pData->nRecCount++;` |
|      ! 0 | 1956 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 1957 | `		pData->nRecCount--;` |
|      ! 0 | 1958 | `		return rc;` |
|        - | 1959 | `	}` |
|      ! 0 | 1960 | `	return SXRET_OK;` |
|      ! 0 | 1961 | `}` |
|        - | 1962 | `/*` |
|        - | 1963 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 1964 | ` *  Create array containing variables and their values.` |
|        - | 1965 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 1966 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 1967 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 1968 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 1969 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 1970 | ` * Parameters` |
|        - | 1971 | ` *  $varname` |
|        - | 1972 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 1973 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 1974 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 1975 | ` *   it recursively.` |
|        - | 1976 | ` * Return` |
|        - | 1977 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 1978 | ` */` |
|        2 | 1979 | `PH7_PRIVATE int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1980 | `{` |
|        - | 1981 | `	ph7_value *pArray,*pObj;` |
|        3 | 1982 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 1983 | `	const char *zName;` |
|        - | 1984 | `	SyString sVar;` |
|        - | 1985 | `	int i,nLen;` |
|        3 | 1986 | `	if( nArg < 1 ){` |
|        - | 1987 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 1988 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1989 | `		return PH7_OK;` |
|        - | 1990 | `	}` |
|        - | 1991 | `	/* Create the array */` |
|        3 | 1992 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 1993 | `	if( pArray == 0 ){` |
|        - | 1994 | `		/* Out of memory */` |
|      ! 0 | 1995 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 1996 | `		/* Return NULL */` |
|      ! 0 | 1997 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1998 | `		return PH7_OK;` |
|        - | 1999 | `	}` |
|        - | 2000 | `	/* Perform the requested operation */` |
|        7 | 2001 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 2002 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 2003 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 2004 | `				struct compact_data sData;` |
|      ! 0 | 2005 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 2006 | `				/* Recursively walk the array */` |
|      ! 0 | 2007 | `				sData.nRecCount = 0;` |
|      ! 0 | 2008 | `				sData.pArray = pArray;` |
|      ! 0 | 2009 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 2010 | `			}` |
|      ! 0 | 2011 | `		}else{` |
|        - | 2012 | `			/* Extract variable name */` |
|        5 | 2013 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 2014 | `			if( nLen > 0 ){` |
|        5 | 2015 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 2016 | `				/* Check if the variable is available in the current frame */` |
|        5 | 2017 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 2018 | `				if( pObj ){` |
|        5 | 2019 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 2020 | `				}` |
|        2 | 2021 | `			}` |
|        - | 2022 | `		}` |
|        3 | 2023 | `	}` |
|        - | 2024 | `	/* Return the array */` |
|        3 | 2025 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 2026 | `	return PH7_OK;` |
|        2 | 2027 | `}` |
|        - | 2028 | `/*` |
|        - | 2029 | ` * The [import_request_variables()] function store it's state information` |
|        - | 2030 | ` * in an instance of the following structure.` |
|        - | 2031 | ` */` |
|        - | 2032 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 2033 | `struct extract_aux_data` |
|        - | 2034 | `{` |
|        - | 2035 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 2036 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 2037 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 2038 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 2039 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 2040 | `};` |
|        - | 2041 | `/*` |
|        - | 2042 | ` * php's php_valid_var_name(): a legal PHP variable name matches` |
|        - | 2043 | ` * [A-Za-z_\x80-\xff][A-Za-z0-9_\x80-\xff]* . Byte-wise and locale-free on` |
|        - | 2044 | ` * purpose (php has been locale-independent here since 8.0); the high-byte` |
|        - | 2045 | ` * range is what lets UTF-8 identifiers through. extract() drops every key` |
|        - | 2046 | ` * that does not pass, instead of installing an unreachable variable.` |
|        - | 2047 | ` */` |
|      148 | 2048 | `static int VmIsValidVarName(const char *zName,sxu32 nByte)` |
|        3 | 2049 | `{` |
|        - | 2050 | `	unsigned char c;` |
|        - | 2051 | `	sxu32 i;` |
|      151 | 2052 | `	if( nByte < 1 ){` |
|        7 | 2053 | `		return FALSE;` |
|        - | 2054 | `	}` |
|      145 | 2055 | `	c = (unsigned char)zName[0];` |
|      145 | 2056 | `	if( c != '_' && !(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') && c < 0x80 ){` |
|       11 | 2057 | `		return FALSE;` |
|        - | 2058 | `	}` |
|      367 | 2059 | `	for( i = 1 ; i < nByte ; ++i ){` |
|      250 | 2060 | `		c = (unsigned char)zName[i];` |
|      248 | 2061 | `		if( c != '_' && !(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z')` |
|       77 | 2062 | `		 && !(c >= '0' && c <= '9') && c < 0x80 ){` |
|       17 | 2063 | `			return FALSE;` |
|        - | 2064 | `		}` |
|      118 | 2065 | `	}` |
|      119 | 2066 | `	return TRUE;` |
|       77 | 2067 | `}` |
|        - | 2068 | `/* TRUE when the name is exactly "this": php refuses to re-assign $this. */` |
|      148 | 2069 | `static int VmExtractIsThis(const char *zName,sxu32 nByte)` |
|        2 | 2070 | `{` |
|      150 | 2071 | `	return nByte == sizeof("this")-1 && SyMemcmp(zName,"this",sizeof("this")-1) == 0;` |
|        2 | 2072 | `}` |
|        - | 2073 | `/*` |
|        - | 2074 | ` * TRUE when the name belongs to the superglobal table ($GLOBALS, $_SERVER,` |
|        - | 2075 | ` * $_GET, …). php hands extract() a per-frame symbol table that holds no` |
|        - | 2076 | ` * superglobal, so such a key lands in the LOCAL table and the real superglobal` |
|        - | 2077 | ` * is untouched. In PHL the name resolves to the superglobal SLOT itself` |
|        - | 2078 | ` * (VmExtractMemObj consults hSuper first), so a plain store would replace` |
|        - | 2079 | ` * $GLOBALS/$_SERVER with the imported value and take the whole symbol table /` |
|        - | 2080 | ` * request environment with it. Those keys are dropped instead — a prefixed` |
|        - | 2081 | ` * name ($p__SERVER) is a normal local and stores fine.` |
|        - | 2082 | ` */` |
|      100 | 2083 | `static int VmExtractIsProtected(ph7_vm *pVm,const char *zName,sxu32 nByte)` |
|        2 | 2084 | `{` |
|      102 | 2085 | `	return SyHashGet(&pVm->hSuper,(const void *)zName,nByte) != 0;` |
|        2 | 2086 | `}` |
|        - | 2087 | `/*` |
|        - | 2088 | ` * TRUE when the calling frame already holds this variable name.` |
|        - | 2089 | ` * "this" and the superglobals always answer FALSE, matching the symbol table` |
|        - | 2090 | ` * php hands extract(): $this is bound implicitly and superglobals live outside` |
|        - | 2091 | ` * the frame. So EXTR_IF_EXISTS/EXTR_PREFIX_IF_EXISTS skip those keys (php-exact)` |
|        - | 2092 | ` * and EXTR_PREFIX_SAME takes its not-a-collision branch.` |
|        - | 2093 | ` */` |
|       66 | 2094 | `static int VmExtractVarExists(ph7_vm *pVm,const char *zName,sxu32 nByte)` |
|        2 | 2095 | `{` |
|        - | 2096 | `	SyString sVar;` |
|       68 | 2097 | `	if( VmExtractIsThis(zName,nByte) \|\| VmExtractIsProtected(pVm,zName,nByte) ){` |
|       20 | 2098 | `		return FALSE;` |
|        - | 2099 | `	}` |
|       50 | 2100 | `	SyStringInitFromBuf(&sVar,zName,nByte);` |
|       50 | 2101 | `	return VmExtractMemObj(pVm,&sVar,FALSE,FALSE) != 0;` |
|       35 | 2102 | `}` |
|        - | 2103 | `/*` |
|        - | 2104 | ` * Create-or-overwrite a variable of the calling frame with a copy of pValue.` |
|        - | 2105 | ` * Returns TRUE when the variable was written (php counts exactly those).` |
|        - | 2106 | ` */` |
|       60 | 2107 | `static int VmExtractStoreVar(ph7_vm *pVm,const char *zName,sxu32 nByte,ph7_value *pValue)` |
|        2 | 2108 | `{` |
|        - | 2109 | `	ph7_value *pObj;` |
|        - | 2110 | `	SyString sVar;` |
|       62 | 2111 | `	SyStringInitFromBuf(&sVar,zName,nByte);` |
|        - | 2112 | `	/* bDup: the name lives in a scratch blob that is reused by the next entry */` |
|       62 | 2113 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|       62 | 2114 | `	if( pObj == 0 ){` |
|      ! 0 | 2115 | `		return FALSE;` |
|        - | 2116 | `	}` |
|       62 | 2117 | `	PH7_MemObjStore(pValue,pObj);` |
|       62 | 2118 | `	return TRUE;` |
|       32 | 2119 | `}` |
|        - | 2120 | `/*` |
|        - | 2121 | ` * Build php's prefixed name "<prefix>_<key>" (php_prefix_varname with` |
|        - | 2122 | ` * add_underscore): the separator is unconditional, so an empty prefix still` |
|        - | 2123 | ` * yields "_key" exactly like php.` |
|        - | 2124 | ` */` |
|       38 | 2125 | `static sxi32 VmExtractPrefixName(SyBlob *pOut,const char *zPrefix,int nPrefix,` |
|        - | 2126 | `	const char *zKey,sxu32 nKey)` |
|        2 | 2127 | `{` |
|       40 | 2128 | `	SyBlobReset(pOut);` |
|       40 | 2129 | `	if( nPrefix > 0 && SyBlobAppend(pOut,zPrefix,(sxu32)nPrefix) != SXRET_OK ){` |
|      ! 0 | 2130 | `		return SXERR_MEM;` |
|        - | 2131 | `	}` |
|       40 | 2132 | `	if( SyBlobAppend(pOut,"_",sizeof(char)) != SXRET_OK ){` |
|      ! 0 | 2133 | `		return SXERR_MEM;` |
|        - | 2134 | `	}` |
|       40 | 2135 | `	if( nKey > 0 && SyBlobAppend(pOut,zKey,nKey) != SXRET_OK ){` |
|      ! 0 | 2136 | `		return SXERR_MEM;` |
|        - | 2137 | `	}` |
|       40 | 2138 | `	return SXRET_OK;` |
|       21 | 2139 | `}` |
|        - | 2140 | `/* What to do with one array entry, decided by the extract mode. */` |
|        - | 2141 | `#define VM_EXTRACT_DROP     0 /* php skips this key entirely */` |
|        - | 2142 | `#define VM_EXTRACT_PLAIN    1 /* install under the key itself */` |
|        - | 2143 | `#define VM_EXTRACT_PREFIX   2 /* install under "<prefix>_<key>" */` |
|        - | 2144 | `/*` |
|        - | 2145 | ` * int extract(array &$array[,int $flags = EXTR_OVERWRITE[,string $prefix = "" ]])` |
|        - | 2146 | ` *   Import variables into the current symbol table from an array.` |
|        - | 2147 | ` *` |
|        - | 2148 | ` * $flags is php's ENUM (PH7_EXTR_* in ph7int.h), not a bitmask: the mode is` |
|        - | 2149 | ` * (flags & 0xff) and anything above EXTR_IF_EXISTS(6) is a ValueError. The` |
|        - | 2150 | ` * modes, mirroring php's per-mode helpers in ext/standard/array.c:` |
|        - | 2151 | ` *   EXTR_OVERWRITE(0)        collisions overwrite` |
|        - | 2152 | ` *   EXTR_SKIP(1)             collisions keep the existing variable` |
|        - | 2153 | ` *   EXTR_PREFIX_SAME(2)      collisions install "<prefix>_<key>"` |
|        - | 2154 | ` *   EXTR_PREFIX_ALL(3)       every key installs as "<prefix>_<key>"` |
|        - | 2155 | ` *   EXTR_PREFIX_INVALID(4)   only illegal names (and numeric keys) get prefixed` |
|        - | 2156 | ` *   EXTR_PREFIX_IF_EXISTS(5) install "<prefix>_<key>" only if <key> exists` |
|        - | 2157 | ` *   EXTR_IF_EXISTS(6)        overwrite only variables that already exist` |
|        - | 2158 | `` * Modes 2..5 require $prefix (php: `is required when using this extract type`),`` |
|        - | 2159 | ` * a non-empty $prefix must itself be a legal identifier, and a key whose final` |
|        - | 2160 | ` * name is not a legal variable name is dropped rather than installed. Only` |
|        - | 2161 | ` * EXTR_PREFIX_ALL/EXTR_PREFIX_INVALID look at numeric keys at all.` |
|        - | 2162 | ` *` |
|        - | 2163 | `` * $this is never a target: php throws `Cannot re-assign $this` where a store`` |
|        - | 2164 | ` * would land on it, and skips it where a store would not (EXTR_SKIP), and` |
|        - | 2165 | ` * $GLOBALS is never clobbered.` |
|        - | 2166 | ` * Return` |
|        - | 2167 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 2168 | ` */` |
|       98 | 2169 | `PH7_PRIVATE int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 | 2170 | `{` |
|      101 | 2171 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 2172 | `	ph7_hashmap_node *pEntry;` |
|        - | 2173 | `	ph7_hashmap *pMap;` |
|      101 | 2174 | `	const char *zPrefix = 0;` |
|      101 | 2175 | `	sxi64 iFlags = PH7_EXTR_OVERWRITE;` |
|      101 | 2176 | `	sxi64 iCount = 0;` |
|        - | 2177 | `	ph7_value sValue;` |
|        - | 2178 | `	SyBlob sWorker;` |
|      101 | 2179 | `	int nPrefix = 0;` |
|      101 | 2180 | `	sxi32 rc = PH7_OK;` |
|        - | 2181 | `	int iType;` |
|        - | 2182 | `	sxu32 n;` |
|      101 | 2183 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|        - | 2184 | `		char zBuf[64];` |
|      ! 0 | 2185 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|        - | 2186 | `			"extract(): Argument #1 ($array) must be of type array, %s given",` |
|      ! 0 | 2187 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|        - | 2188 | `	}` |
|      101 | 2189 | `	if( nArg > 1 ){` |
|       95 | 2190 | `		rc = PH7_IntArgResolve(pCtx,apArg[1],"extract",2,"$flags","int",&iFlags);` |
|       95 | 2191 | `		if( rc != PH7_OK ){` |
|        3 | 2192 | `			return rc;` |
|        - | 2193 | `		}` |
|       45 | 2194 | `	}` |
|        - | 2195 | `	/* php: the mode is the low byte; EXTR_REFS(0x100) rides above it */` |
|       99 | 2196 | `	iType = (int)(iFlags & 0xff);` |
|       99 | 2197 | `	if( iType > PH7_EXTR_IF_EXISTS ){` |
|        7 | 2198 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|        - | 2199 | `			"extract(): Argument #2 ($flags) must be a valid extract type");` |
|        - | 2200 | `	}` |
|       93 | 2201 | `	if( iType > PH7_EXTR_SKIP && iType <= PH7_EXTR_PREFIX_IF_EXISTS && nArg < 3 ){` |
|       12 | 2202 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|        - | 2203 | `			"extract(): Argument #3 ($prefix) is required when using this extract type");` |
|        - | 2204 | `	}` |
|       83 | 2205 | `	if( nArg > 2 ){` |
|       45 | 2206 | `		zPrefix = ph7_value_to_string(apArg[2],&nPrefix);` |
|       45 | 2207 | `		if( nPrefix > 0 && !VmIsValidVarName(zPrefix,(sxu32)nPrefix) ){` |
|        5 | 2208 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|        - | 2209 | `				"extract(): Argument #3 ($prefix) must be a valid identifier");` |
|        - | 2210 | `		}` |
|       19 | 2211 | `	}` |
|       79 | 2212 | `	if( iFlags & PH7_EXTR_REFS ){` |
|        - | 2213 | `		/* php binds each extracted name BY REFERENCE to its array slot. PHL has` |
|        - | 2214 | `		 * no by-ref extraction; importing by VALUE instead would be a divergence` |
|        - | 2215 | `		 * the caller cannot see (writes stop propagating), so it is loud (§10).` |
|        - | 2216 | `		 * The EXTR_REFS constant itself stays undefined. */` |
|        7 | 2217 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|        - | 2218 | `			"extract(): Argument #2 ($flags) EXTR_REFS is not supported");` |
|        - | 2219 | `	}` |
|        - | 2220 | `	/* Point to the target hashmap */` |
|       72 | 2221 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       72 | 2222 | `	if( pMap->nEntry < 1 ){` |
|        - | 2223 | `		/* Empty map,return  0 */` |
|      ! 0 | 2224 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 2225 | `		return PH7_OK;` |
|        - | 2226 | `	}` |
|       72 | 2227 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|       72 | 2228 | `	PH7_MemObjInit(pVm,&sValue);` |
|        - | 2229 | `	/* php walks a COPY of the array, so an entry that overwrites the caller's own` |
|        - | 2230 | `	 * array variable ($arr = ['arr'=>1]; extract($arr)) cannot pull the map from` |
|        - | 2231 | `	 * under the walk. Pinning the map for the walk is the same guarantee. */` |
|       72 | 2232 | `	pMap->iRef++;` |
|       72 | 2233 | `	pEntry = pMap->pFirst;` |
|        - | 2234 | `	/* pFirst walks the insertion order through pPrev — PH7 links the entry list` |
|        - | 2235 | `	 * in reverse (same traversal PH7_HashmapWalk uses). */` |
|      222 | 2236 | `	for( n = pMap->nEntry ; n > 0 && pEntry ; --n, pEntry = pEntry->pPrev ){` |
|        - | 2237 | `		const char *zKey, *zFinal;` |
|        - | 2238 | `		sxu32 nKey, nFinal;` |
|        - | 2239 | `		char zNum[32];` |
|        - | 2240 | `		int bIntKey, iAction;` |
|        - | 2241 | `		/* Work off a COPY of the entry value: installing a variable can grow` |
|        - | 2242 | `		 * pVm->aMemObj, and a pointer into that set would dangle across the` |
|        - | 2243 | `		 * reallocation (this is why the walk API hands out copies too). The` |
|        - | 2244 | `		 * release comes FIRST so it covers every continue/goto below: the load` |
|        - | 2245 | `		 * takes a reference on an array/object value and does not drop the one` |
|        - | 2246 | `		 * the previous entry left behind (PH7_HashmapWalk releases per iteration` |
|        - | 2247 | `		 * for the same reason — without it a whole-array extract() pins every` |
|        - | 2248 | `		 * value it copied, and their destructors never run). */` |
|      154 | 2249 | `		PH7_MemObjRelease(&sValue);` |
|      154 | 2250 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|      154 | 2251 | `		bIntKey = (pEntry->iType == HASHMAP_INT_NODE);` |
|      154 | 2252 | `		if( bIntKey ){` |
|        - | 2253 | `			/* Only the two prefixing modes below ever look at a numeric key */` |
|       19 | 2254 | `			nKey = SyBufferFormat(zNum,sizeof(zNum),"%qd",pEntry->xKey.iKey);` |
|       19 | 2255 | `			zKey = zNum;` |
|       10 | 2256 | `		}else{` |
|      136 | 2257 | `			zKey = (const char *)SyBlobData(&pEntry->xKey.sKey);` |
|      136 | 2258 | `			nKey = SyBlobLength(&pEntry->xKey.sKey);` |
|        - | 2259 | `		}` |
|      154 | 2260 | `		iAction = VM_EXTRACT_DROP;` |
|      154 | 2261 | `		switch( iType ){` |
|       14 | 2262 | `		case PH7_EXTR_OVERWRITE:` |
|       30 | 2263 | `			if( bIntKey \|\| !VmIsValidVarName(zKey,nKey) ){` |
|        5 | 2264 | `				break;` |
|        - | 2265 | `			}` |
|       22 | 2266 | `			if( VmExtractIsThis(zKey,nKey) ){` |
|        3 | 2267 | `				goto this_error;` |
|        - | 2268 | `			}` |
|       20 | 2269 | `			iAction = VM_EXTRACT_PLAIN;` |
|       20 | 2270 | `			break;` |
|       11 | 2271 | `		case PH7_EXTR_SKIP:` |
|       24 | 2272 | `			if( bIntKey \|\| !VmIsValidVarName(zKey,nKey) \|\| VmExtractIsThis(zKey,nKey) ){` |
|        6 | 2273 | `				break;` |
|        - | 2274 | `			}` |
|       14 | 2275 | `			if( VmExtractVarExists(pVm,zKey,nKey) ){` |
|        5 | 2276 | `				break; /* collision: keep the existing variable */` |
|        - | 2277 | `			}` |
|       10 | 2278 | `			iAction = VM_EXTRACT_PLAIN;` |
|       10 | 2279 | `			break;` |
|       15 | 2280 | `		case PH7_EXTR_IF_EXISTS:` |
|       32 | 2281 | `			if( bIntKey \|\| !VmExtractVarExists(pVm,zKey,nKey) ){` |
|       15 | 2282 | `				break;` |
|        - | 2283 | `			}` |
|        5 | 2284 | `			if( !VmIsValidVarName(zKey,nKey) ){` |
|      ! 0 | 2285 | `				break;` |
|        - | 2286 | `			}` |
|        5 | 2287 | `			if( VmExtractIsThis(zKey,nKey) ){` |
|      ! 0 | 2288 | `				goto this_error;` |
|        - | 2289 | `			}` |
|        5 | 2290 | `			iAction = VM_EXTRACT_PLAIN;` |
|        5 | 2291 | `			break;` |
|        8 | 2292 | `		case PH7_EXTR_PREFIX_SAME:` |
|       18 | 2293 | `			if( bIntKey \|\| nKey < 1 ){` |
|        3 | 2294 | `				break;` |
|        - | 2295 | `			}` |
|       14 | 2296 | `			if( VmExtractVarExists(pVm,zKey,nKey) ){` |
|        3 | 2297 | `				iAction = VM_EXTRACT_PREFIX; /* collision */` |
|       13 | 2298 | `			}else if( !VmIsValidVarName(zKey,nKey) ){` |
|        5 | 2299 | `				break;` |
|      ! 0 | 2300 | `			}else{` |
|        - | 2301 | `				/* $this cannot be a target, but its prefixed form can */` |
|        8 | 2302 | `				iAction = VmExtractIsThis(zKey,nKey) ? VM_EXTRACT_PREFIX : VM_EXTRACT_PLAIN;` |
|        - | 2303 | `			}` |
|       10 | 2304 | `			break;` |
|       13 | 2305 | `		case PH7_EXTR_PREFIX_ALL:` |
|       28 | 2306 | `			if( !bIntKey && nKey < 1 ){` |
|        3 | 2307 | `				break;` |
|        - | 2308 | `			}` |
|       26 | 2309 | `			iAction = VM_EXTRACT_PREFIX;` |
|       26 | 2310 | `			break;` |
|        7 | 2311 | `		case PH7_EXTR_PREFIX_INVALID:` |
|       15 | 2312 | `			iAction = (bIntKey \|\| !VmIsValidVarName(zKey,nKey) \|\| VmExtractIsThis(zKey,nKey))` |
|       13 | 2313 | `				? VM_EXTRACT_PREFIX : VM_EXTRACT_PLAIN;` |
|       16 | 2314 | `			break;` |
|        8 | 2315 | `		case PH7_EXTR_PREFIX_IF_EXISTS:` |
|       18 | 2316 | `			if( !bIntKey && VmExtractVarExists(pVm,zKey,nKey) ){` |
|        3 | 2317 | `				iAction = VM_EXTRACT_PREFIX;` |
|        1 | 2318 | `			}` |
|       16 | 2319 | `			break;` |
|      ! 0 | 2320 | `		default:` |
|      ! 0 | 2321 | `			break;` |
|        - | 2322 | `		}` |
|      152 | 2323 | `		if( iAction == VM_EXTRACT_DROP ){` |
|      113 | 2324 | `			continue;` |
|        - | 2325 | `		}` |
|       80 | 2326 | `		if( iAction == VM_EXTRACT_PREFIX ){` |
|       40 | 2327 | `			if( VmExtractPrefixName(&sWorker,zPrefix,nPrefix,zKey,nKey) != SXRET_OK ){` |
|      ! 0 | 2328 | `				rc = PH7_VmThrowException(pCtx,"Error","PH7 engine is running out of memory");` |
|      ! 0 | 2329 | `				goto done;` |
|        - | 2330 | `			}` |
|       40 | 2331 | `			zFinal = (const char *)SyBlobData(&sWorker);` |
|       40 | 2332 | `			nFinal = SyBlobLength(&sWorker);` |
|       40 | 2333 | `			if( !VmIsValidVarName(zFinal,nFinal) ){` |
|        7 | 2334 | `				continue; /* php drops a prefixed name that is not an identifier */` |
|        - | 2335 | `			}` |
|       34 | 2336 | `			if( VmExtractIsThis(zFinal,nFinal) ){` |
|      ! 0 | 2337 | `				goto this_error;` |
|        - | 2338 | `			}` |
|       18 | 2339 | `		}else{` |
|       42 | 2340 | `			if( VmExtractIsProtected(pVm,zKey,nKey) ){` |
|       13 | 2341 | `				continue; /* $GLOBALS/$_SERVER/... are never a plain target */` |
|        - | 2342 | `			}` |
|       30 | 2343 | `			zFinal = zKey;` |
|       30 | 2344 | `			nFinal = nKey;` |
|        - | 2345 | `		}` |
|       62 | 2346 | `		if( VmExtractStoreVar(pVm,zFinal,nFinal,&sValue) ){` |
|       62 | 2347 | `			iCount++;` |
|       30 | 2348 | `		}` |
|       62 | 2349 | `		continue;` |
|        1 | 2350 | `this_error:` |
|        3 | 2351 | `		rc = PH7_VmThrowException(pCtx,"Error","Cannot re-assign $this");` |
|        3 | 2352 | `		goto done;` |
|      ! 0 | 2353 | `	}` |
|        - | 2354 | `	/* Number of variables successfully imported */` |
|       70 | 2355 | `	ph7_result_int64(pCtx,iCount);` |
|       35 | 2356 | `done:` |
|       72 | 2357 | `	PH7_MemObjRelease(&sValue);` |
|       72 | 2358 | `	SyBlobRelease(&sWorker);` |
|       72 | 2359 | `	PH7_HashmapUnref(pMap);` |
|       72 | 2360 | `	return rc;` |
|       52 | 2361 | `}` |
|        - | 2362 | `/*` |
|        - | 2363 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 2364 | ` * defined below.` |
|        - | 2365 | ` */` |
|        2 | 2366 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 2367 | `{` |
|        3 | 2368 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 2369 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 2370 | `	ph7_value *pObj;` |
|        - | 2371 | `	SyString sVar;` |
|        - | 2372 | `	/* Perform a string cast */` |
|        3 | 2373 | `	PH7_MemObjToString(pKey);` |
|        3 | 2374 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 2375 | `		/* Unavailable variable name */` |
|      ! 0 | 2376 | `		return SXRET_OK;` |
|        - | 2377 | `	}` |
|        3 | 2378 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 2379 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 2380 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 2381 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 2382 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 2383 | `			);` |
|        2 | 2384 | `	}else{` |
|      ! 0 | 2385 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 2386 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 2387 | `	}` |
|        3 | 2388 | `	sVar.zString = pAux->zWorker;` |
|        - | 2389 | `	/* Extract the variable */` |
|        3 | 2390 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 2391 | `	if( pObj ){` |
|        3 | 2392 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 2393 | `	}` |
|        3 | 2394 | `	return SXRET_OK;` |
|        2 | 2395 | `}` |
|        - | 2396 | `/*` |
|        - | 2397 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 2398 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 2399 | ` * Parameters` |
|        - | 2400 | ` * $types` |
|        - | 2401 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 2402 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 2403 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 2404 | ` *  POST includes the POST uploaded file information.` |
|        - | 2405 | ` *  Note:` |
|        - | 2406 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 2407 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 2408 | ` * $prefix` |
|        - | 2409 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 2410 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 2411 | ` *  variable named $pref_userid.` |
|        - | 2412 | ` * Return` |
|        - | 2413 | ` *  TRUE on success or FALSE on failure.` |
|        - | 2414 | ` */` |
|        2 | 2415 | `PH7_PRIVATE int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 2416 | `{` |
|        - | 2417 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 2418 | `	extract_aux_data sAux;` |
|        - | 2419 | `	int nLen,nPrefixLen;` |
|        - | 2420 | `	ph7_value *pSuper;` |
|        - | 2421 | `	ph7_vm *pVm;` |
|        - | 2422 | `	/* By default import only $_GET variables  */` |
|        3 | 2423 | `	zImport = "G";` |
|        3 | 2424 | `	nLen = (int)sizeof(char);` |
|        3 | 2425 | `	zPrefix = 0;` |
|        3 | 2426 | `	nPrefixLen = 0;` |
|        3 | 2427 | `	if( nArg > 0 ){` |
|        3 | 2428 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 2429 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 2430 | `		}` |
|        3 | 2431 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 2432 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 2433 | `		}` |
|        1 | 2434 | `	}` |
|        - | 2435 | `	/* Point to the underlying VM */` |
|        3 | 2436 | `	pVm = pCtx->pVm;` |
|        - | 2437 | `	/* Initialize the aux data */` |
|        3 | 2438 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 2439 | `	sAux.zPrefix = zPrefix;` |
|        3 | 2440 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 2441 | `	sAux.pVm = pVm;` |
|        - | 2442 | `	/* Extract */` |
|        3 | 2443 | `	zEnd = &zImport[nLen];` |
|        5 | 2444 | `	while( zImport < zEnd ){` |
|        3 | 2445 | `		int c = zImport[0];` |
|        3 | 2446 | `		pSuper = 0;` |
|        3 | 2447 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 2448 | `			/* Import $_GET variables */` |
|        3 | 2449 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 2450 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 2451 | `			/* Import $_POST variables */` |
|      ! 0 | 2452 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 2453 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 2454 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 2455 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 2456 | `		}` |
|        3 | 2457 | `		if( pSuper ){` |
|        - | 2458 | `			/* Iterate throw array entries */` |
|        3 | 2459 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 2460 | `		}` |
|        - | 2461 | `		/* Advance the cursor */` |
|        3 | 2462 | `		zImport++;` |
|        1 | 2463 | `	}` |
|        - | 2464 | `	/* All done,return TRUE*/` |
|        3 | 2465 | `	ph7_result_bool(pCtx,0);` |
|        3 | 2466 | `	return PH7_OK;` |
|        1 | 2467 | `}` |
|        - | 2468 |  |
