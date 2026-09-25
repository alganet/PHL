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
|      336 |   37 | `static int VmClassConstLookup(` |
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
|      341 |   49 | `	*ppClass = 0;` |
|      341 |   50 | `	*ppAttr = 0;` |
|     4045 |   51 | `	for( iSep = 0; iSep + 1 < nLen; iSep++ ){` |
|     3847 |   52 | `		if( zName[iSep] == ':' && zName[iSep+1] == ':' ){` |
|      142 |   53 | `			break;` |
|        - |   54 | `		}` |
|     1857 |   55 | `	}` |
|      341 |   56 | `	if( iSep + 1 >= nLen ){` |
|      203 |   57 | `		return VM_CCONST_PLAIN;` |
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
|      173 |   99 | `}` |
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
|      140 |  148 | `PH7_PRIVATE int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  149 | `{` |
|        - |  150 | `	ph7_class_attr *pAttr;` |
|        - |  151 | `	ph7_class *pClass;` |
|        - |  152 | `	const char *zName;` |
|      145 |  153 | `	int nLen = 0;` |
|      145 |  154 | `	int iSep = 0;` |
|      145 |  155 | `	int res = 0;` |
|      145 |  156 | `	if( nArg < 1 ){` |
|        - |  157 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  158 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  159 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  160 | `		return SXRET_OK;` |
|        - |  161 | `	}` |
|        - |  162 | `	/* Extract constant name */` |
|      145 |  163 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  164 | `	/* Class-constant form "C::K": every miss — unknown class, unknown constant, or one` |
|        - |  165 | `	 * that is not visible from here — is a plain FALSE, since asking whether a name is` |
|        - |  166 | `	 * defined is exactly what defined() is for (this used to consult the` |
|        - |  167 | `	 * global constant table only, so EVERY class constant answered false while` |
|        - |  168 | `	 * constant() read the same name correctly). */` |
|      145 |  169 | `	if( nLen > 0 ){` |
|      145 |  170 | `		int iRc = VmClassConstLookup(pCtx->pVm,zName,nLen,&pClass,&pAttr,&iSep);` |
|      145 |  171 | `		switch( iRc ){` |
|       35 |  172 | `			case VM_CCONST_PLAIN:` |
|       75 |  173 | `				break;` |
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
|       35 |  187 | `	}` |
|        - |  188 | `	/* Perform the lookup */` |
|       75 |  189 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  190 | `		/* Already defined */` |
|       64 |  191 | `		res = 1;` |
|       30 |  192 | `	}` |
|       75 |  193 | `	ph7_result_bool(pCtx,res);` |
|       75 |  194 | `	return SXRET_OK;` |
|       75 |  195 | `}` |
|        - |  196 | `/*` |
|        - |  197 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  198 | ` * below.` |
|        - |  199 | ` */` |
|   102042 |  200 | `PH7_PRIVATE void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        5 |  201 | `{` |
|   102047 |  202 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  203 | `	/* Expand constant value */` |
|   102047 |  204 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|   102047 |  205 | `}` |
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
|      196 |  481 | `PH7_PRIVATE int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 |  482 | `{` |
|        - |  483 | `	SyHashEntry *pEntry;` |
|        - |  484 | `	ph7_constant *pCons;` |
|        - |  485 | `	const char *zName; /* Constant name */` |
|        - |  486 | `	ph7_value sVal;    /* Constant value */` |
|        - |  487 | `	int nLen;` |
|      200 |  488 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  489 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  490 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  491 | `		ph7_result_null(pCtx);` |
|      ! 0 |  492 | `		return SXRET_OK;` |
|        - |  493 | `	}` |
|        - |  494 | `	/* Extract the constant name */` |
|      200 |  495 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
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
|      200 |  506 | `		ph7_class_attr *pAttr = 0;` |
|      200 |  507 | `		ph7_class *pClass = 0;` |
|      200 |  508 | `		int iSep = 0;` |
|      200 |  509 | `		int iRc = VmClassConstLookup(pCtx->pVm,zName,nLen,&pClass,&pAttr,&iSep);` |
|      200 |  510 | `		if( iRc != VM_CCONST_PLAIN ){` |
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
|      131 |  546 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|      131 |  547 | `	if( pEntry == 0 ){` |
|        - |  548 | `		/* php 8: a catchable Error, not a notice + NULL (band A #4) */` |
|        8 |  549 | `		return PH7_VmThrowException(pCtx,"Error",` |
|        2 |  550 | `			"Undefined constant \"%.*s\"",nLen,zName);` |
|        - |  551 | `	}` |
|      127 |  552 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  553 | `	/* Point to the structure that describe the constant */` |
|      127 |  554 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  555 | `	/* Extract constant value by calling it's associated callback` |
|        - |  556 | `	 * (emits the #[\Deprecated] notice first when attributed, php) */` |
|      127 |  557 | `	VmExpandConstantWithNotice(pCtx->pVm,pCons,&sVal);` |
|        - |  558 | `	/* Return that value */` |
|      127 |  559 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  560 | `	/* Cleanup */` |
|      127 |  561 | `	PH7_MemObjRelease(&sVal);` |
|      127 |  562 | `	return SXRET_OK;` |
|      102 |  563 | `}` |
|        - |  564 | `/*` |
|        - |  565 | ` * Hash walker callback used by the [get_defined_constants()] function defined` |
|        - |  566 | ` * below. php's answer is a MAP -- the constant's name is the KEY and its VALUE` |
|        - |  567 | ``  * is the element -- which is what makes `get_defined_constants()['PHP_EOL']` `` |
|        - |  568 | ` * the documented way to read one. PHL used to answer a LIST of names, so every` |
|        - |  569 | ``  * such lookup was an `Undefined array key` and NULL, and `in_array($n, $c)` `` |
|        - |  570 | `` * answered where php wants `isset($c[$n])`: the array had the right length and`` |
|        - |  571 | ` * the wrong shape.` |
|        - |  572 | ` */` |
|    35780 |  573 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        3 |  574 | `{` |
|        - |  575 | ``	/* SNAPSHOT ONLY -- nothing is expanded during the walk. A `const` whose`` |
|        - |  576 | `	 * initializer is a bytecode program runs USER CODE when it expands, and user` |
|        - |  577 | ``	 * code can `define()`: that grows hConstant while SyHashForEach is holding a`` |
|        - |  578 | `	 * fixed entry count, and the walk then runs off the end of the bucket chain` |
|        - |  579 | `	 * (a segfault, reproducible from a const initializer that constructs an` |
|        - |  580 | `	 * object whose __construct defines a constant). Collect first, expand after. */` |
|    35783 |  581 | `	SySet *pOut = (SySet *)pUserData;` |
|    35783 |  582 | `	if( pEntry == 0 \|\| pEntry->pUserData == 0 ){` |
|      ! 0 |  583 | `		return SXRET_OK;` |
|        - |  584 | `	}` |
|    35783 |  585 | `	SySetPut(pOut,(const void *)&pEntry);` |
|    35783 |  586 | `	return SXRET_OK;` |
|    17893 |  587 | `}` |
|        - |  588 | `/*` |
|        - |  589 | ` * Add one snapshotted constant to the answer, under its name.` |
|        - |  590 | ` */` |
|    35780 |  591 | `static sxi32 VmConstDumpEntry(ph7_value *pTarget,SyHashEntry *pEntry)` |
|        3 |  592 | `{` |
|    35783 |  593 | `	ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  594 | `	ph7_value sName,sVal;` |
|        - |  595 | `	sxi32 rc;` |
|        - |  596 | `	/* Prepare the constant name for insertion */` |
|    35783 |  597 | `	PH7_MemObjInitFromString(pTarget->pVm,&sName,0);` |
|    35783 |  598 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  599 | `	/* ...and its VALUE, through the SAME evaluate-once path an ordinary read` |
|        - |  600 | ``	 * takes -- so a `const C = new Foo();` reported here is the object the`` |
|        - |  601 | ``	 * program itself sees, not a second one. The `#[\Deprecated]` NOTICE is what`` |
|        - |  602 | `	 * is skipped (VmExpandConstantOnce rather than …WithNotice): describing a` |
|        - |  603 | `	 * constant is not reading one, and php raises nothing here either. */` |
|    35783 |  604 | `	PH7_MemObjInit(pTarget->pVm,&sVal);` |
|    35783 |  605 | `	rc = VmExpandConstantOnce(pTarget->pVm,pCons,&sVal);` |
|    35783 |  606 | `	if( rc == SXRET_OK ){` |
|    35783 |  607 | `		ph7_array_add_elem(pTarget,&sName,&sVal); /* Will make its own copy */` |
|    17890 |  608 | `	}` |
|    35783 |  609 | `	PH7_MemObjRelease(&sVal);` |
|    35783 |  610 | `	PH7_MemObjRelease(&sName);` |
|    35783 |  611 | `	return rc;` |
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
|    35845 |  661 | `	for( n = 0 ; n < nSnap ; ++n ){` |
|    35783 |  662 | `		ph7_constant *pCons = (ph7_constant *)apEntry[n]->pUserData;` |
|    35783 |  663 | `		sxi32 rcExp = VmConstDumpEntry(pUser && pCons->bUserDefined ? pUser : pAll,apEntry[n]);` |
|    35783 |  664 | `		if( rcExp != SXRET_OK ){` |
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
|    17893 |  677 | `	}` |
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
|     4564 |  707 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        5 |  708 | `{` |
|        - |  709 | `	sxu32 iNum;` |
|     4569 |  710 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     4569 |  711 | `	return iNum;` |
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
|        3 |  724 | `{` |
|      317 |  725 | `	SyMT19937Seed(&pVm->sMt,nSeed,bLegacyTwist);` |
|      317 |  726 | `	pVm->mtSeeded = TRUE;` |
|      317 |  727 | `}` |
|        - |  728 | `/*` |
|        - |  729 | ` * Draw the next full 32-bit MT19937 word, seeding lazily from the OS CSPRNG on` |
|        - |  730 | ` * first use exactly as PHP auto-seeds when rand()/mt_rand() runs before srand().` |
|        - |  731 | ` */` |
|     2582 |  732 | `PH7_PRIVATE sxu32 PH7_VmMtRand(ph7_vm *pVm)` |
|        3 |  733 | `{` |
|     2585 |  734 | `	if( !pVm->mtSeeded ){` |
|        - |  735 | `		sxu32 nSeed;` |
|        3 |  736 | `		if( SyOSCSPRNG((void *)&nSeed,sizeof(nSeed)) != SXRET_OK ){` |
|        - |  737 | `			/* No OS entropy source: fall back to the RC4 generator's output. */` |
|      ! 0 |  738 | `			nSeed = PH7_VmRandomNum(pVm);` |
|      ! 0 |  739 | `		}` |
|        - |  740 | `		/* An un-seeded generator is php's default one, never MT_RAND_PHP. */` |
|        3 |  741 | `		SyMT19937Seed(&pVm->sMt,nSeed,FALSE);` |
|        3 |  742 | `		pVm->mtSeeded = TRUE;` |
|        1 |  743 | `	}` |
|     2585 |  744 | `	return SyMT19937Next(&pVm->sMt);` |
|        3 |  745 | `}` |
|        - |  746 | `/*` |
|        - |  747 | ` * Map a full 32-bit draw uniformly into [0,uMax] (uMax is the range width, i.e.` |
|        - |  748 | ` * max-min). Rejection sampling against the largest unbiased ceiling, matching` |
|        - |  749 | ` * PHP's php_random_range32().` |
|        - |  750 | ` */` |
|     2378 |  751 | `static sxu32 VmMtRange32(ph7_vm *pVm,sxu32 uMax)` |
|        3 |  752 | `{` |
|        - |  753 | `	sxu32 result,limit;` |
|     2381 |  754 | `	result = PH7_VmMtRand(pVm);` |
|        - |  755 | `	/* Whole 32-bit domain: no scaling needed. */` |
|     2381 |  756 | `	if( uMax == 0xFFFFFFFFU ){` |
|      ! 0 |  757 | `		return result;` |
|        - |  758 | `	}` |
|        - |  759 | `	/* Make the range inclusive of max. */` |
|     2381 |  760 | `	uMax++;` |
|        - |  761 | `	/* Powers of two are unbiased under a plain mask. */` |
|     2381 |  762 | `	if( (uMax & (uMax - 1)) == 0 ){` |
|       87 |  763 | `		return result & (uMax - 1);` |
|        - |  764 | `	}` |
|        - |  765 | `	/* Ceiling under which 0xFFFFFFFF % uMax == 0; discard draws above it. */` |
|     2297 |  766 | `	limit = 0xFFFFFFFFU - (0xFFFFFFFFU % uMax) - 1;` |
|     2297 |  767 | `	while( result > limit ){` |
|      ! 0 |  768 | `		result = PH7_VmMtRand(pVm);` |
|      ! 0 |  769 | `	}` |
|     2297 |  770 | `	return result % uMax;` |
|     1192 |  771 | `}` |
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
|        3 |  803 | `{` |
|     2395 |  804 | `	sxu64 uMax = (sxu64)iMax - (sxu64)iMin;` |
|     2395 |  805 | `	if( uMax > 0xFFFFFFFFULL ){` |
|       16 |  806 | `		return (sxi64)(VmMtRange64(pVm,uMax) + (sxu64)iMin);` |
|        - |  807 | `	}` |
|     2381 |  808 | `	return (sxi64)((sxu64)VmMtRange32(pVm,(sxu32)uMax) + (sxu64)iMin);` |
|     1199 |  809 | `}` |
|        - |  810 | `/*` |
|        - |  811 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  812 | ` * Note that the generated string is NOT null terminated.` |
|        - |  813 | ` * PH7 uses its own private PRNG (the SQLite3-derived RC4 generator` |
|        - |  814 | ` * implemented in src/sx/sxrand.c).` |
|        - |  815 | ` */` |
|  4157026 |  816 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        5 |  817 | `{` |
|        - |  818 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  819 | `	int i;` |
|        - |  820 | `	/* Generate a binary string first */` |
|  4157031 |  821 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  822 | `	/* Turn the binary string into english based alphabet */` |
| 45727663 |  823 | `	for( i = 0 ; i < nLen ; ++i ){` |
| 41570637 |  824 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
| 20785321 |  825 | `	 }` |
|  4157031 |  826 | `}` |
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
|      222 |  936 | `PH7_PRIVATE int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 |  937 | `{` |
|        - |  938 | `	char zString[1024];` |
|      227 |  939 | `	int iLen = 0x10;` |
|      227 |  940 | `	if( nArg > 0 ){` |
|        - |  941 | `		/* Get the desired length */` |
|      227 |  942 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      227 |  943 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  944 | `			/* Default length */` |
|        3 |  945 | `			iLen = 0x10;` |
|        1 |  946 | `		}` |
|      111 |  947 | `	}` |
|        - |  948 | `	/* Generate the random string */` |
|      227 |  949 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  950 | `	/* Return the generated string */` |
|      227 |  951 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      227 |  952 | `	return SXRET_OK;` |
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
|      386 | 1028 | `	for( nAttempt = 0 ; nAttempt < 50 ; ++nAttempt ){` |
|        - | 1029 | `		/* Always draw a full 8 bytes so endianness of the cast doesn't matter` |
|        - | 1030 | `		 * (a 4-byte fill into a sxu64 would land in the high half on big-endian` |
|        - | 1031 | `		 * and the low-half mask would always read 0). */` |
|        - | 1032 | `		sxu64 uDraw;` |
|      386 | 1033 | `		if( SyOSCSPRNG(&uDraw,sizeof(uDraw)) != SXRET_OK ){` |
|      ! 0 | 1034 | `			return PH7_VmThrowException(pCtx,` |
|        - | 1035 | `				"Random\\RandomException",` |
|        - | 1036 | `				"Cannot gather sufficient random data"` |
|        - | 1037 | `				);` |
|        - | 1038 | `		}` |
|      386 | 1039 | `		uDraw &= uMask;` |
|      386 | 1040 | `		if( uDraw <= uRange ){` |
|      225 | 1041 | `			uResult = uDraw;` |
|      225 | 1042 | `			break;` |
|        - | 1043 | `		}` |
|       82 | 1044 | `	}` |
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
|       40 | 1224 | `static sxi32 VmOutputArgToString(ph7_context *pCtx,ph7_value *pArg,const char **pzData,int *pnLen)` |
|        3 | 1225 | `{` |
|       43 | 1226 | `	sxi32 rc = PH7_MemObjToStringUV(pArg);` |
|       43 | 1227 | `	if( rc != SXRET_OK ){` |
|        3 | 1228 | `		pCtx->nThrowRc = rc;` |
|        3 | 1229 | `		return rc;` |
|        - | 1230 | `	}` |
|       41 | 1231 | `	*pzData = ph7_value_to_string(pArg,pnLen);` |
|       41 | 1232 | `	return SXRET_OK;` |
|       23 | 1233 | `}` |
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
|       40 | 1277 | `PH7_PRIVATE int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        3 | 1278 | `{` |
|        - | 1279 | `	const char *zData;` |
|       43 | 1280 | `	int nDataLen = 0;` |
|        - | 1281 | `	ph7_vm *pVm;` |
|        - | 1282 | `	int i,rc;` |
|        - | 1283 | `	/* Point to the target VM */` |
|       43 | 1284 | `	pVm = pCtx->pVm;` |
|        - | 1285 | `	/* Output */` |
|       81 | 1286 | `	for( i = 0 ; i < nArg ; ++i ){` |
|       43 | 1287 | `		sxi32 rcSv = VmOutputArgToString(pCtx,apArg[i],&zData,&nDataLen);` |
|       43 | 1288 | `		if( rcSv != SXRET_OK ){` |
|        3 | 1289 | `			return rcSv;` |
|        - | 1290 | `		}` |
|       41 | 1291 | `		if( nDataLen > 0 ){` |
|       41 | 1292 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|       41 | 1293 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|       41 | 1294 | `			if( rc == SXERR_ABORT ){` |
|        - | 1295 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 1296 | `				return PH7_ABORT;` |
|        - | 1297 | `			}` |
|       19 | 1298 | `		}` |
|       22 | 1299 | `	}` |
|        - | 1300 | `	/* Return 1 */` |
|       41 | 1301 | `	ph7_result_int(pCtx,1);` |
|       41 | 1302 | `	return SXRET_OK;` |
|       23 | 1303 | `}` |
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
|      122 | 1373 | `PH7_PRIVATE int vm_builtin_phpversion(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
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
|      124 | 1384 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0 ){` |
|      115 | 1385 | `		if( !VmExtensionIsLoaded(pCtx,apArg[0]) ){` |
|       15 | 1386 | `			ph7_result_bool(pCtx,0);` |
|       15 | 1387 | `			return PH7_OK;` |
|        - | 1388 | `		}` |
|       50 | 1389 | `	}` |
|      110 | 1390 | `	ph7_result_string(pCtx,PHP_COMPAT_VERSION,(int)sizeof(PHP_COMPAT_VERSION) - 1);` |
|      110 | 1391 | `	return PH7_OK;` |
|       63 | 1392 | `}` |
|        - | 1393 | `/*` |
|        - | 1394 | ` * The extensions PHL reports as loaded, in the order get_loaded_extensions()` |
|        - | 1395 | `` * lists them and with the CASE php uses for each. `extension_loaded()` matches`` |
|        - | 1396 | ` * case-INSENSITIVELY, which is why one table serves both.` |
|        - | 1397 | ` */` |
|        - | 1398 | `static const char * const azExtension[] = {` |
|        - | 1399 | `	"Core", "date", "pcre", "SPL", "json", "standard",` |
|        - | 1400 | `	"ctype", "filter", "hash", "Reflection", "session", "mbstring"` |
|        - | 1401 | `#ifdef PH7_ENABLE_LIBXML` |
|        - | 1402 | `	, "libxml", "dom", "xmlwriter"` |
|        - | 1403 | `#endif` |
|        - | 1404 | `};` |
|        - | 1405 | `/*` |
|        - | 1406 | `` * `phl.stub_extensions` is a PHL-only directive: a comma-separated list of`` |
|        - | 1407 | ` * extensions PHL does NOT implement but reports as LOADED, so software that` |
|        - | 1408 | ` * only GATES on extension_loaded() (PHPUnit's dom/xmlwriter check) runs` |
|        - | 1409 | ` * unmodified. It synthesizes nothing -- no class, no function.` |
|        - | 1410 | ` *` |
|        - | 1411 | ` * Walk it, handing each trimmed name to xVisit until one answers non-zero.` |
|        - | 1412 | ` */` |
|       36 | 1413 | `static int VmStubExtWalk(ph7_vm *pVm,int (*xVisit)(const char *,int,void *),void *pData)` |
|        2 | 1414 | `{` |
|        - | 1415 | `	SyBlob sList;` |
|        - | 1416 | `	const char *z;` |
|       38 | 1417 | `	int nByte,i = 0,rc = 0;` |
|       38 | 1418 | `	SyBlobInit(&sList,&pVm->sAllocator);` |
|       38 | 1419 | `	PH7_VmIniGetStr(pVm,"phl.stub_extensions",&sList);` |
|       38 | 1420 | `	z = (const char *)SyBlobData(&sList);` |
|       38 | 1421 | `	nByte = (int)SyBlobLength(&sList);` |
|       56 | 1422 | `	while( rc == 0 && i < nByte ){` |
|        - | 1423 | `		int iStart,iEnd;` |
|       27 | 1424 | `		while( i < nByte && z[i] == ',' ){ i++; }` |
|       19 | 1425 | `		iStart = i;` |
|      223 | 1426 | `		while( i < nByte && z[i] != ',' ){ i++; }` |
|       19 | 1427 | `		iEnd = i;` |
|       36 | 1428 | `		while( iStart < iEnd && (z[iStart] == ' ' \|\| z[iStart] == '\t') ){ iStart++; }` |
|       28 | 1429 | `		while( iEnd > iStart && (z[iEnd-1] == ' ' \|\| z[iEnd-1] == '\t') ){ iEnd--; }` |
|       19 | 1430 | `		if( iEnd > iStart ){` |
|       19 | 1431 | `			rc = xVisit(&z[iStart],iEnd - iStart,pData);` |
|        9 | 1432 | `		}` |
|        1 | 1433 | `	}` |
|       38 | 1434 | `	SyBlobRelease(&sList);` |
|       38 | 1435 | `	return rc;` |
|        2 | 1436 | `}` |
|        - | 1437 | `typedef struct vm_ext_match vm_ext_match;` |
|        - | 1438 | `struct vm_ext_match {` |
|        - | 1439 | `	const char *zName;` |
|        - | 1440 | `	int nName;` |
|        - | 1441 | `};` |
|       14 | 1442 | `static int VmStubExtMatch(const char *zName,int nName,void *pData)` |
|        1 | 1443 | `{` |
|       15 | 1444 | `	vm_ext_match *p = (vm_ext_match *)pData;` |
|       15 | 1445 | `	return nName == p->nName && SyStrnicmp(zName,p->zName,(sxu32)nName) == 0;` |
|        1 | 1446 | `}` |
|        - | 1447 | `/*` |
|        - | 1448 | ` * Is this name one of the extensions this engine reports as loaded? Shared by` |
|        - | 1449 | ` * extension_loaded() and phpversion(), which php answers from the same list.` |
|        - | 1450 | ` */` |
|      218 | 1451 | `static int VmExtensionIsLoaded(ph7_context *pCtx,ph7_value *pName)` |
|        2 | 1452 | `{` |
|        - | 1453 | `	vm_ext_match sMatch;` |
|        - | 1454 | `	const char *zName;` |
|        - | 1455 | `	int nName;` |
|        - | 1456 | `	sxu32 n;` |
|      220 | 1457 | `	zName = ph7_value_to_string(pName,&nName);` |
|     1758 | 1458 | `	for( n = 0 ; n < SX_ARRAYSIZE(azExtension) ; ++n ){` |
|     1726 | 1459 | `		if( nName == (int)SyStrlen(azExtension[n])` |
|     1056 | 1460 | `		 && SyStrnicmp(zName,azExtension[n],(sxu32)nName) == 0 ){` |
|      190 | 1461 | `			return 1;` |
|        - | 1462 | `		}` |
|      771 | 1463 | `	}` |
|       32 | 1464 | `	sMatch.zName = zName;` |
|       32 | 1465 | `	sMatch.nName = nName;` |
|       32 | 1466 | `	return VmStubExtWalk(pCtx->pVm,VmStubExtMatch,&sMatch);` |
|      111 | 1467 | `}` |
|        - | 1468 | `/*` |
|        - | 1469 | ` * bool extension_loaded(string $extension)` |
|        - | 1470 | ` *  php matches the name case-insensitively.` |
|        - | 1471 | ` */` |
|      104 | 1472 | `PH7_PRIVATE int vm_builtin_extension_loaded(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 1473 | `{` |
|      106 | 1474 | `	if( nArg < 1 ){` |
|      ! 0 | 1475 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 1476 | `		return PH7_OK;` |
|        - | 1477 | `	}` |
|      106 | 1478 | `	ph7_result_bool(pCtx,VmExtensionIsLoaded(pCtx,apArg[0]));` |
|      106 | 1479 | `	return PH7_OK;` |
|       54 | 1480 | `}` |
|        4 | 1481 | `static int VmStubExtCollect(const char *zName,int nName,void *pData)` |
|        1 | 1482 | `{` |
|        5 | 1483 | `	ph7_context *pCtx = (ph7_context *)((void **)pData)[0];` |
|        5 | 1484 | `	ph7_value *pArray = (ph7_value *)((void **)pData)[1];` |
|        5 | 1485 | `	ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|        5 | 1486 | `	if( pVal ){` |
|        5 | 1487 | `		ph7_value_string(pVal,zName,nName);` |
|        5 | 1488 | `		ph7_array_add_elem(pArray,0,pVal);` |
|        5 | 1489 | `		ph7_context_release_value(pCtx,pVal);` |
|        2 | 1490 | `	}` |
|        5 | 1491 | `	return 0;` |
|        1 | 1492 | `}` |
|        - | 1493 | `/*` |
|        - | 1494 | ` * array get_loaded_extensions(bool $zend_extensions = false)` |
|        - | 1495 | ` *  PHL loads no Zend extension, so the zend list is always empty.` |
|        - | 1496 | ` */` |
|        6 | 1497 | `PH7_PRIVATE int vm_builtin_get_loaded_extensions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 1498 | `{` |
|        8 | 1499 | `	ph7_value *pArray = ph7_context_new_array(pCtx);` |
|        - | 1500 | `	void *apData[2];` |
|        - | 1501 | `	sxu32 n;` |
|        8 | 1502 | `	if( pArray == 0 ){` |
|      ! 0 | 1503 | `		return PH7_ContextMemoryError(pCtx);` |
|        - | 1504 | `	}` |
|        8 | 1505 | `	if( nArg > 0 && ph7_value_to_bool(apArg[0]) ){` |
|      ! 0 | 1506 | `		ph7_result_value(pCtx,pArray);` |
|      ! 0 | 1507 | `		return PH7_OK;` |
|        - | 1508 | `	}` |
|       98 | 1509 | `	for( n = 0 ; n < SX_ARRAYSIZE(azExtension) ; ++n ){` |
|       92 | 1510 | `		ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|       92 | 1511 | `		if( pVal ){` |
|       92 | 1512 | `			ph7_value_string(pVal,azExtension[n],-1);` |
|       92 | 1513 | `			ph7_array_add_elem(pArray,0,pVal);` |
|       92 | 1514 | `			ph7_context_release_value(pCtx,pVal);` |
|       45 | 1515 | `		}` |
|       47 | 1516 | `	}` |
|        8 | 1517 | `	apData[0] = pCtx;` |
|        8 | 1518 | `	apData[1] = pArray;` |
|        8 | 1519 | `	VmStubExtWalk(pCtx->pVm,VmStubExtCollect,apData);` |
|        8 | 1520 | `	ph7_result_value(pCtx,pArray);` |
|        8 | 1521 | `	return PH7_OK;` |
|        5 | 1522 | `}` |
|        - | 1523 | `/*` |
|        - | 1524 | ` * string php_sapi_name(void)` |
|        - | 1525 | ` *  Returns the type of interface (SAPI) PHL is running under.` |
|        - | 1526 | ` * Parameters` |
|        - | 1527 | ` *  None` |
|        - | 1528 | ` * Return` |
|        - | 1529 | ` *  "cli-server" while serving an HTTP request via the built-in -S server, "cli" otherwise.` |
|        - | 1530 | ` */` |
|        2 | 1531 | `PH7_PRIVATE int vm_builtin_php_sapi_name(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1532 | `{` |
|        3 | 1533 | `	const char *zSapi = pCtx->pVm->bHttpContext ? "cli-server" : "cli";` |
|        1 | 1534 | `	SXUNUSED(nArg);` |
|        1 | 1535 | `	SXUNUSED(apArg); /* cc warning */` |
|        3 | 1536 | `	ph7_result_string(pCtx,zSapi,-1);` |
|        3 | 1537 | `	return PH7_OK;` |
|        1 | 1538 | `}` |
|        - | 1539 | `/*` |
|        - | 1540 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 1541 | ` */` |
|        - | 1542 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 1543 | ` "<html><head>"\` |
|        - | 1544 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 1545 | ` "<style type=\"text/css\">"\` |
|        - | 1546 | ` "div {"\` |
|        - | 1547 | `     "border: 1px solid #cccccc;"\` |
|        - | 1548 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 1549 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 1550 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 1551 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 1552 | `     "-webkit-border-radius: 10px;"\` |
|        - | 1553 | `     "-o-border-radius: 10px;"\` |
|        - | 1554 | `     "border-radius: 10px;"\` |
|        - | 1555 | `     "padding-left: 2em;"\` |
|        - | 1556 | `     "background-color: white;"\` |
|        - | 1557 | `     "margin-left: auto;"\` |
|        - | 1558 | `     "font-family: verdana;"\` |
|        - | 1559 | `     "padding-right: 2em;"\` |
|        - | 1560 | `     "margin-right: auto;"\` |
|        - | 1561 | `     "}"\` |
|        - | 1562 | `     "body {"\` |
|        - | 1563 | `     "padding: 0.2em;"\` |
|        - | 1564 | `     "font-style: normal;"\` |
|        - | 1565 | `     "font-size: medium;"\` |
|        - | 1566 | `     "background-color: #f2f2f2;"\` |
|        - | 1567 | `     "}"\` |
|        - | 1568 | `     "hr {"\` |
|        - | 1569 | `     "border-style: solid none none;"\` |
|        - | 1570 | `     "border-width: 1px medium medium;"\` |
|        - | 1571 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 1572 | `     "height: 1px;"\` |
|        - | 1573 | `     "}"\` |
|        - | 1574 | `     "a {"\` |
|        - | 1575 | `     "color: #3366cc;"\` |
|        - | 1576 | `     "text-decoration: none;"\` |
|        - | 1577 | `     "}"\` |
|        - | 1578 | `     "a:hover {"\` |
|        - | 1579 | `     "color: #999999;"\` |
|        - | 1580 | `     "}"\` |
|        - | 1581 | `     "a:active {"\` |
|        - | 1582 | `     "color: #663399;"\` |
|        - | 1583 | `     "}"\` |
|        - | 1584 | `     "h1 {"\` |
|        - | 1585 | `     "margin: 0;"\` |
|        - | 1586 | `     "padding: 0;"\` |
|        - | 1587 | `     "font-family: Verdana;"\` |
|        - | 1588 | `     "font-weight: bold;"\` |
|        - | 1589 | `     "font-style: normal;"\` |
|        - | 1590 | `     "font-size: medium;"\` |
|        - | 1591 | `     "text-transform: capitalize;"\` |
|        - | 1592 | `     "color: #0a328c;"\` |
|        - | 1593 | `     "}"\` |
|        - | 1594 | `     "p {"\` |
|        - | 1595 | `     "margin: 0 auto;"\` |
|        - | 1596 | `     "font-size: medium;"\` |
|        - | 1597 | `     "font-style: normal;"\` |
|        - | 1598 | `     "font-family: verdana;"\` |
|        - | 1599 | `     "}"\` |
|        - | 1600 | `"</style></head><body>"\` |
|        - | 1601 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 1602 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 1603 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 1604 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 1605 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 1606 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 1607 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 1608 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 1609 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 1610 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 1611 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 1612 |  |
|        - | 1613 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 1614 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 1615 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 1616 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 1617 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 1618 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 1619 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 1620 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 1621 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 1622 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 1623 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 1624 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 1625 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 1626 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 1627 |  |
|        - | 1628 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 1629 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 1630 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 1631 | `"&nbsp;*<br>"\` |
|        - | 1632 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 1633 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 1634 | `"&nbsp;* are met:<br>"\` |
|        - | 1635 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 1636 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 1637 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 1638 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 1639 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 1640 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 1641 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 1642 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 1643 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 1644 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 1645 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 1646 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 1647 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 1648 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 1649 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 1650 | `"&nbsp;*<br>"\` |
|        - | 1651 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 1652 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 1653 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 1654 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 1655 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 1656 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 1657 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 1658 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 1659 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 1660 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 1661 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 1662 | `"&nbsp;*/<br>"\` |
|        - | 1663 | `"</span></small></small></p>"\` |
|        - | 1664 | `"</div></body></html>"` |
|        - | 1665 | `/*` |
|        - | 1666 | ` * bool ph7credits(void)` |
|        - | 1667 | ` * bool ph7info(void)` |
|        - | 1668 | ` * bool ph7copyright(void)` |
|        - | 1669 | ` *  Prints out the credits for PH7 engine` |
|        - | 1670 | ` * Parameters` |
|        - | 1671 | ` *  None` |
|        - | 1672 | ` * Return` |
|        - | 1673 | ` *  Always TRUE` |
|        - | 1674 | ` */` |
|        2 | 1675 | `PH7_PRIVATE int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1676 | `{` |
|        3 | 1677 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 1678 | `	/* Expand the HTML page above*/` |
|        3 | 1679 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 1680 | `	ph7_context_output_format(` |
|        1 | 1681 | `		pCtx,` |
|        - | 1682 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 1683 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 1684 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 1685 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 1686 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 1687 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 1688 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 1689 | `#ifdef __WINNT__` |
|        - | 1690 | `		"Windows NT"` |
|        - | 1691 | `#elif defined(__UNIXES__)` |
|        - | 1692 | `		"UNIX-Like"` |
|        - | 1693 | `#else` |
|        - | 1694 | `		"Other OS"` |
|        - | 1695 | `#endif` |
|        - | 1696 | `		);` |
|        3 | 1697 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 1698 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 1699 | `	SXUNUSED(apArg);` |
|        - | 1700 | `	/* Return TRUE */` |
|        - | 1701 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 1702 | `	return PH7_OK;` |
|        1 | 1703 | `}` |
|        - | 1704 | `/*` |
|        - | 1705 | ` * Section:` |
|        - | 1706 | ` *    URL related routines.` |
|        - | 1707 | ` * Status:` |
|        - | 1708 | ` *    Stable.` |
|        - | 1709 | ` */` |
|        - | 1710 | `/*` |
|        - | 1711 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 1712 | ` *  Parse a URL and return its fields.` |
|        - | 1713 | ` * Parameters` |
|        - | 1714 | ` *  $url` |
|        - | 1715 | ` *   The URL to parse.` |
|        - | 1716 | ` * $component` |
|        - | 1717 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 1718 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 1719 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 1720 | ` *  in which case the return value will be an integer).` |
|        - | 1721 | ` * Return` |
|        - | 1722 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 1723 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 1724 | ` *  this array are:` |
|        - | 1725 | ` *   scheme - e.g. http` |
|        - | 1726 | ` *   host` |
|        - | 1727 | ` *   port` |
|        - | 1728 | ` *   user` |
|        - | 1729 | ` *   pass` |
|        - | 1730 | ` *   path` |
|        - | 1731 | ` *   query - after the question mark ?` |
|        - | 1732 | ` *   fragment - after the hashmark #` |
|        - | 1733 | ` * Note:` |
|        - | 1734 | ` *  FALSE is returned on failure.` |
|        - | 1735 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 1736 | ` *  with the standard PHP engine.` |
|        - | 1737 | ` */` |
|        - | 1738 | `/*` |
|        - | 1739 | ` * parse_url() component set.` |
|        - | 1740 | ` *` |
|        - | 1741 | ` * A bare SyString cannot express "present but empty", which php needs:` |
|        - | 1742 | ` * parse_url("") is ['path'=>''] and parse_url("?") is ['query'=>''], both` |
|        - | 1743 | ` * distinct from the component being absent. So presence is tracked separately.` |
|        - | 1744 | ` */` |
|     1328 | 1745 | `static int VmUrlIsAlnum(int c)` |
|        2 | 1746 | `{` |
|     1330 | 1747 | `	return (c >= '0' && c <= '9') \|\| (c >= 'a' && c <= 'z') \|\| (c >= 'A' && c <= 'Z');` |
|        2 | 1748 | `}` |
|       12 | 1749 | `static int VmUrlIsAlpha(int c)` |
|        2 | 1750 | `{` |
|       14 | 1751 | `	return (c >= 'a' && c <= 'z') \|\| (c >= 'A' && c <= 'Z');` |
|        2 | 1752 | `}` |
|        - | 1753 | `/* scheme = ALNUM *( ALNUM / "+" / "-" / "." ) -- php accepts a digit first. */` |
|     1328 | 1754 | `static int VmUrlIsSchemeByte(int c)` |
|        2 | 1755 | `{` |
|     1330 | 1756 | `	return VmUrlIsAlnum(c) \|\| c == '+' \|\| c == '-' \|\| c == '.';` |
|        2 | 1757 | `}` |
|        - | 1758 | `/*` |
|        - | 1759 | ` * Resolve the port span that followed the ':' in an authority.` |
|        - | 1760 | ` *` |
|        - | 1761 | ` * Returns 1 (usable port in *piPort), 0 (the span is empty, so there is simply` |
|        - | 1762 | ` * no port) or -1 (php rejects the whole URL). php is lenient about what follows` |
|        - | 1763 | ` * the digits -- ":8a" and ":8 0" both yield 8 -- but demands at least one digit` |
|        - | 1764 | ` * and a value that fits a port, so ":abc", ":-80" and ":65536" are all failures.` |
|        - | 1765 | ` */` |
|       92 | 1766 | `static int VmUrlParsePort(const char *z,int n,int *piPort)` |
|        2 | 1767 | `{` |
|       94 | 1768 | `	int i = 0,iVal = 0,nDigit = 0;` |
|       94 | 1769 | `	if( n < 1 ){` |
|      ! 0 | 1770 | `		return 0;` |
|        - | 1771 | `	}` |
|      140 | 1772 | `	while( i < n && (z[i] == ' ' \|\| z[i] == '\t') ){` |
|      ! 0 | 1773 | `		i++;` |
|      ! 0 | 1774 | `	}` |
|       94 | 1775 | `	if( i < n && (z[i] == '+' \|\| z[i] == '-') ){` |
|      ! 0 | 1776 | `		if( z[i] == '-' ){` |
|      ! 0 | 1777 | `			return -1;` |
|        - | 1778 | `		}` |
|      ! 0 | 1779 | `		i++;` |
|      ! 0 | 1780 | `	}` |
|      318 | 1781 | `	while( i < n && z[i] >= '0' && z[i] <= '9' ){` |
|      226 | 1782 | `		iVal = iVal * 10 + (z[i] - '0');` |
|      226 | 1783 | `		if( iVal > 65535 ){` |
|      ! 0 | 1784 | `			return -1; /* Out of range, and this also caps the accumulator */` |
|        - | 1785 | `		}` |
|      226 | 1786 | `		nDigit++;` |
|      226 | 1787 | `		i++;` |
|        2 | 1788 | `	}` |
|       94 | 1789 | `	if( nDigit < 1 ){` |
|       12 | 1790 | `		return -1;` |
|        - | 1791 | `	}` |
|       84 | 1792 | `	*piPort = iVal;` |
|       84 | 1793 | `	return 1;` |
|       48 | 1794 | `}` |
|        - | 1795 | `/*` |
|        - | 1796 | ` * Split an authority -- "[user[:pass]@]host[:port]" -- into pOut.` |
|        - | 1797 | ` * Returns 0 for the malformed input php reports as FALSE.` |
|        - | 1798 | ` */` |
|      278 | 1799 | `static int VmUrlParseAuthority(const char *z,int n,VmUrlParts *pOut,int bPortKnown)` |
|        2 | 1800 | `{` |
|        - | 1801 | `	const char *zHost;` |
|      280 | 1802 | `	int i,iAt = -1,iColon = -1,iSep = -1,nHost,iPort = 0,rc;` |
|        - | 1803 | `	/* php splits the credentials at the LAST '@' ("//a@b@c" is user "a@b") */` |
|     2110 | 1804 | `	for( i = 0 ; i < n ; ++i ){` |
|     1832 | 1805 | `		if( z[i] == '@' ){` |
|       44 | 1806 | `			iAt = i;` |
|       21 | 1807 | `		}` |
|      917 | 1808 | `	}` |
|      280 | 1809 | `	if( iAt >= 0 ){` |
|        - | 1810 | `		/* and the user from the password at the FIRST ':' before it */` |
|      154 | 1811 | `		for( i = 0 ; i < iAt ; ++i ){` |
|      152 | 1812 | `			if( z[i] == ':' ){` |
|       42 | 1813 | `				iColon = i;` |
|       42 | 1814 | `				break;` |
|        - | 1815 | `			}` |
|       57 | 1816 | `		}` |
|       44 | 1817 | `		if( iColon >= 0 ){` |
|       42 | 1818 | `			SyStringInitFromBuf(&pOut->sUser,z,(sxu32)iColon);` |
|       42 | 1819 | `			SyStringInitFromBuf(&pOut->sPass,&z[iColon+1],(sxu32)(iAt - iColon - 1));` |
|       42 | 1820 | `			pOut->bUser = pOut->bPass = 1;` |
|       22 | 1821 | `		}else{` |
|        3 | 1822 | `			SyStringInitFromBuf(&pOut->sUser,z,(sxu32)iAt);` |
|        3 | 1823 | `			pOut->bUser = 1;` |
|        - | 1824 | `		}` |
|       44 | 1825 | `		z += iAt + 1;` |
|       44 | 1826 | `		n -= iAt + 1;` |
|       21 | 1827 | `	}` |
|      280 | 1828 | `	zHost = z;` |
|      280 | 1829 | `	nHost = n;` |
|      280 | 1830 | `	if( !(n > 1 && z[0] == '[' && z[n-1] == ']') ){` |
|        - | 1831 | `		/* Unless a bracketed IPv6 literal fills the WHOLE authority -- in which` |
|        - | 1832 | `		 * case php keeps the brackets in "host" and scans no port, so every ':'` |
|        - | 1833 | `		 * inside belongs to the address -- the port hangs off the LAST ':'.` |
|        - | 1834 | `		 * php decides that on the first and last byte alone, which is why` |
|        - | 1835 | `		 * "//[:]\|]" is a single host while "//[::1]:8080" splits a port off. */` |
|     1838 | 1836 | `		for( i = 0 ; i < n ; ++i ){` |
|     1562 | 1837 | `			if( z[i] == ':' ){` |
|      118 | 1838 | `				iSep = i;` |
|       58 | 1839 | `			}` |
|      782 | 1840 | `		}` |
|      278 | 1841 | `		if( iSep >= 0 ){` |
|        - | 1842 | `			/* The host is trimmed at the ':' whether or not the port was already` |
|        - | 1843 | `			 * resolved by the caller. */` |
|       94 | 1844 | `			nHost = iSep;` |
|       94 | 1845 | `			if( !bPortKnown ){` |
|       88 | 1846 | `				rc = VmUrlParsePort(&z[iSep+1],n - iSep - 1,&iPort);` |
|       88 | 1847 | `				if( rc < 0 ){` |
|       12 | 1848 | `					return 0;` |
|        - | 1849 | `				}` |
|       78 | 1850 | `				if( rc > 0 ){` |
|       78 | 1851 | `					pOut->iPort = iPort;` |
|       78 | 1852 | `					pOut->bPort = 1;` |
|       38 | 1853 | `				}` |
|       38 | 1854 | `			}` |
|       41 | 1855 | `		}` |
|      133 | 1856 | `	}` |
|      270 | 1857 | `	if( nHost < 1 ){` |
|        - | 1858 | `		/* php requires a non-empty host once an authority is in play, which is` |
|        - | 1859 | `		 * what makes "//", "http://" and ":80" all FALSE. */` |
|       36 | 1860 | `		return 0;` |
|        - | 1861 | `	}` |
|      236 | 1862 | `	SyStringInitFromBuf(&pOut->sHost,zHost,(sxu32)nHost);` |
|      236 | 1863 | `	pOut->bHost = 1;` |
|      236 | 1864 | `	return 1;` |
|      141 | 1865 | `}` |
|        - | 1866 | `/*` |
|        - | 1867 | ` * Split "path[?query][#fragment]". The fragment is taken FIRST and the query` |
|        - | 1868 | ` * only from what precedes it, so "#a?b" is a fragment of "a?b" with no query.` |
|        - | 1869 | ` */` |
|      278 | 1870 | `static void VmUrlParsePath(const char *z,int n,VmUrlParts *pOut)` |
|        3 | 1871 | `{` |
|      281 | 1872 | `	int i,iEnd = n;` |
|     1669 | 1873 | `	for( i = 0 ; i < n ; ++i ){` |
|     1443 | 1874 | `		if( z[i] == '#' ){` |
|       54 | 1875 | `			SyStringInitFromBuf(&pOut->sFragment,&z[i+1],(sxu32)(n - i - 1));` |
|       54 | 1876 | `			pOut->bFragment = 1;` |
|       54 | 1877 | `			iEnd = i;` |
|       54 | 1878 | `			break;` |
|        - | 1879 | `		}` |
|      697 | 1880 | `	}` |
|     1399 | 1881 | `	for( i = 0 ; i < iEnd ; ++i ){` |
|     1189 | 1882 | `		if( z[i] == '?' ){` |
|       70 | 1883 | `			SyStringInitFromBuf(&pOut->sQuery,&z[i+1],(sxu32)(iEnd - i - 1));` |
|       70 | 1884 | `			pOut->bQuery = 1;` |
|       70 | 1885 | `			iEnd = i;` |
|       70 | 1886 | `			break;` |
|        - | 1887 | `		}` |
|      562 | 1888 | `	}` |
|      281 | 1889 | `	if( iEnd > 0 ){` |
|      263 | 1890 | `		SyStringInitFromBuf(&pOut->sPath,z,(sxu32)iEnd);` |
|      263 | 1891 | `		pOut->bPath = 1;` |
|      130 | 1892 | `	}` |
|      281 | 1893 | `}` |
|        - | 1894 | `/* Parse "host[:port]" followed by an optional path/query/fragment. */` |
|      278 | 1895 | `static int VmUrlAuthorityThenPath(const char *z,int n,VmUrlParts *pOut,int bPortKnown)` |
|        2 | 1896 | `{` |
|      280 | 1897 | `	int i,iEnd = n;` |
|     2110 | 1898 | `	for( i = 0 ; i < n ; ++i ){` |
|     2020 | 1899 | `		if( z[i] == '/' \|\| z[i] == '?' \|\| z[i] == '#' ){` |
|      190 | 1900 | `			iEnd = i;` |
|      190 | 1901 | `			break;` |
|        - | 1902 | `		}` |
|      917 | 1903 | `	}` |
|      280 | 1904 | `	if( !VmUrlParseAuthority(z,iEnd,pOut,bPortKnown) ){` |
|       46 | 1905 | `		return 0;` |
|        - | 1906 | `	}` |
|      236 | 1907 | `	if( iEnd < n ){` |
|      170 | 1908 | `		VmUrlParsePath(&z[iEnd],n - iEnd,pOut);` |
|       84 | 1909 | `	}` |
|      236 | 1910 | `	return 1;` |
|      141 | 1911 | `}` |
|        - | 1912 | `/*` |
|        - | 1913 | ` * Resolve the port php took from the FIRST ':' of a host:port URL.` |
|        - | 1914 | ` *` |
|        - | 1915 | ` * php reads the port straight off that colon before it works out where the host` |
|        - | 1916 | ` * ends, so the digits can even belong to what becomes the path: parse_url()` |
|        - | 1917 | ` * reports port 1 for "a/:1", whose path is "/:1". Mirroring the order keeps` |
|        - | 1918 | ` * that quirk. Returns 0 for a port php rejects.` |
|        - | 1919 | ` */` |
|        6 | 1920 | `static int VmUrlPreparePort(const char *z,int k,int nEnd,VmUrlParts *pOut)` |
|        1 | 1921 | `{` |
|        7 | 1922 | `	int iPort = 0;` |
|        7 | 1923 | `	int rc = VmUrlParsePort(&z[k+1],nEnd - k - 1,&iPort);` |
|        7 | 1924 | `	if( rc < 0 ){` |
|      ! 0 | 1925 | `		return 0;` |
|        - | 1926 | `	}` |
|        7 | 1927 | `	if( rc > 0 ){` |
|        7 | 1928 | `		pOut->iPort = iPort;` |
|        7 | 1929 | `		pOut->bPort = 1;` |
|        3 | 1930 | `	}` |
|        7 | 1931 | `	return 1;` |
|        4 | 1932 | `}` |
|        - | 1933 | `/*` |
|        - | 1934 | ` * php's URL parser, as parse_url() needs it. Returns 0 where php returns FALSE.` |
|        - | 1935 | ` *` |
|        - | 1936 | ` * PH7_VmHttpSplitURI cannot serve here: it is an HTTP REQUEST-target splitter` |
|        - | 1937 | ` * (the HTTP server and filter_var share it) and assumes the input is` |
|        - | 1938 | ` * authority-first, so it read "a" as a host and "mailto:me@x.com" as` |
|        - | 1939 | ` * user:pass@host. php instead decides authority-vs-path up front: only a "//",` |
|        - | 1940 | ` * with or without a scheme before it, introduces an authority.` |
|        - | 1941 | ` */` |
|      392 | 1942 | `PH7_PRIVATE int PH7_VmUrlSplit(const char *z,int n,VmUrlParts *pOut)` |
|        3 | 1943 | `{` |
|      395 | 1944 | `	int i,k = -1,bScheme,bPortForm = 0,nPortEnd = 0;` |
|      395 | 1945 | `	SyZero(pOut,sizeof(VmUrlParts));` |
|        - | 1946 | `	/* A leading "//" settles it before any colon is considered: the authority` |
|        - | 1947 | `	 * starts after the slashes. Reading the colon first turned "//h:80" into a` |
|        - | 1948 | `	 * host called "//h" and "//[::1]" into a path. */` |
|      395 | 1949 | `	if( n >= 2 && z[0] == '/' && z[1] == '/' ){` |
|       24 | 1950 | `		return VmUrlAuthorityThenPath(&z[2],n - 2,pOut,0);` |
|        - | 1951 | `	}` |
|     1883 | 1952 | `	for( i = 0 ; i < n ; ++i ){` |
|     1841 | 1953 | `		if( z[i] == ':' ){` |
|      330 | 1954 | `			k = i;` |
|      330 | 1955 | `			break;` |
|        - | 1956 | `		}` |
|      758 | 1957 | `	}` |
|      373 | 1958 | `	if( k == 0 && n == 1 ){` |
|        - | 1959 | `		/* A lone ":" is an EMPTY scheme, which php rejects outright -- unlike` |
|        - | 1960 | `		 * ":a" or "::", which are simply paths. */` |
|        3 | 1961 | `		return 0;` |
|        - | 1962 | `	}` |
|      371 | 1963 | `	bScheme = k > 0;` |
|     1699 | 1964 | `	for( i = 0 ; bScheme && i < k ; ++i ){` |
|     1330 | 1965 | `		if( !VmUrlIsSchemeByte((unsigned char)z[i]) ){` |
|      ! 0 | 1966 | `			bScheme = 0;` |
|      ! 0 | 1967 | `		}` |
|      666 | 1968 | `	}` |
|      371 | 1969 | `	if( bScheme && k + 1 == n ){` |
|        - | 1970 | `		/* "x:" -- the scheme is the whole URL */` |
|        3 | 1971 | `		SyStringInitFromBuf(&pOut->sScheme,z,(sxu32)k);` |
|        3 | 1972 | `		pOut->bScheme = 1;` |
|        3 | 1973 | `		return 1;` |
|        - | 1974 | `	}` |
|        - | 1975 | `	/* Decide whether that ':' introduces a PORT rather than a scheme: at least` |
|        - | 1976 | `	 * one digit, running to the end of the string or to a '/'. That is what` |
|        - | 1977 | `	 * makes "a:80" a host:port while "a:80?q" is a scheme with path "80", and` |
|        - | 1978 | `	 * it holds however the ':' is reached -- ":1" and "/:1" are both authorities` |
|        - | 1979 | `	 * with an empty host, which php rejects. php caps the run, so a long number` |
|        - | 1980 | `	 * stays a path: "a:123456" is a path, "a:99999" an out-of-range port. */` |
|      369 | 1981 | `	if( k >= 0 ){` |
|      326 | 1982 | `		int p = k + 1;` |
|      326 | 1983 | `		int bBeforeQuery = 1;` |
|      326 | 1984 | `		nPortEnd = k + 1;` |
|        - | 1985 | `		/* A ':' that sits inside a query or fragment is just data: "?:1" is a` |
|        - | 1986 | `		 * query of ":1", not an authority with an empty host. */` |
|     1652 | 1987 | `		for( i = 0 ; i < k ; ++i ){` |
|     1328 | 1988 | `			if( z[i] == '?' \|\| z[i] == '#' ){` |
|      ! 0 | 1989 | `				bBeforeQuery = 0;` |
|      ! 0 | 1990 | `				break;` |
|        - | 1991 | `			}` |
|      665 | 1992 | `		}` |
|      336 | 1993 | `		while( p < n && z[p] >= '0' && z[p] <= '9' ){` |
|       11 | 1994 | `			p++;` |
|        1 | 1995 | `		}` |
|      326 | 1996 | `		if( bBeforeQuery && p > k + 1 && (p >= n \|\| z[p] == '/') && (p - k) < 7 ){` |
|        7 | 1997 | `			bPortForm = 1;` |
|        7 | 1998 | `			nPortEnd = p;` |
|        3 | 1999 | `		}` |
|      162 | 2000 | `	}` |
|      369 | 2001 | `	if( !bScheme ){` |
|       47 | 2002 | `		if( bPortForm ){` |
|        3 | 2003 | `			if( !VmUrlPreparePort(z,k,nPortEnd,pOut) ){` |
|      ! 0 | 2004 | `				return 0;` |
|        - | 2005 | `			}` |
|        3 | 2006 | `			return VmUrlAuthorityThenPath(z,n,pOut,1);` |
|        - | 2007 | `		}` |
|       45 | 2008 | `		VmUrlParsePath(z,n,pOut);` |
|       45 | 2009 | `		if( !pOut->bPath && !pOut->bQuery && !pOut->bFragment ){` |
|        - | 2010 | `			/* php reports the empty URL as an empty PATH, not as no components */` |
|        3 | 2011 | `			SyStringInitFromBuf(&pOut->sPath,z,0);` |
|        3 | 2012 | `			pOut->bPath = 1;` |
|        1 | 2013 | `		}` |
|       45 | 2014 | `		return 1;` |
|        - | 2015 | `	}` |
|      324 | 2016 | `	if( bPortForm ){` |
|        5 | 2017 | `		if( !VmUrlPreparePort(z,k,nPortEnd,pOut) ){` |
|      ! 0 | 2018 | `			return 0;` |
|        - | 2019 | `		}` |
|        5 | 2020 | `		return VmUrlAuthorityThenPath(z,n,pOut,1);` |
|        - | 2021 | `	}` |
|      320 | 2022 | `	SyStringInitFromBuf(&pOut->sScheme,z,(sxu32)k);` |
|      320 | 2023 | `	pOut->bScheme = 1;` |
|      320 | 2024 | `	if( z[k+1] == '/' && k + 2 < n && z[k+2] == '/' ){` |
|      262 | 2025 | `		if( k + 3 < n && z[k+3] == '/' && pOut->sScheme.nByte == 4` |
|       20 | 2026 | `		 && (z[0]=='f'\|\|z[0]=='F') && (z[1]=='i'\|\|z[1]=='I')` |
|       14 | 2027 | `		 && (z[2]=='l'\|\|z[2]=='L') && (z[3]=='e'\|\|z[3]=='E') ){` |
|        - | 2028 | `			/* file:/// has no authority: the path starts at the third slash,` |
|        - | 2029 | `			 * except that a "c:" drive letter swallows it (file:///c:/x is the` |
|        - | 2030 | `			 * path "c:/x"). A '\|' in place of the ':' does NOT count. */` |
|       14 | 2031 | `			int iBase = k + 3;` |
|       14 | 2032 | `			if( iBase + 2 < n && VmUrlIsAlpha((unsigned char)z[iBase+1]) && z[iBase+2] == ':' ){` |
|      ! 0 | 2033 | `				iBase++;` |
|      ! 0 | 2034 | `			}` |
|       14 | 2035 | `			VmUrlParsePath(&z[iBase],n - iBase,pOut);` |
|       14 | 2036 | `			return 1;` |
|        - | 2037 | `		}` |
|      252 | 2038 | `		return VmUrlAuthorityThenPath(&z[k+3],n - k - 3,pOut,0);` |
|        - | 2039 | `	}` |
|        - | 2040 | `	/* "mailto:me@x.com", "x:y", "http:/x": everything after the ':' is a path */` |
|       58 | 2041 | `	VmUrlParsePath(&z[k+1],n - k - 1,pOut);` |
|       58 | 2042 | `	return 1;` |
|      199 | 2043 | `}` |
|        - | 2044 | `/*` |
|        - | 2045 | ` * Emit one parsed component into pValue, replacing control bytes with '_'.` |
|        - | 2046 | ` *` |
|        - | 2047 | ` * php runs php_replace_controlchars_ex over every string component it returns,` |
|        - | 2048 | ` * so a URL carrying a raw NUL, newline or DEL cannot smuggle it through into` |
|        - | 2049 | ` * whatever the caller splices the component into (a header, a log line, a` |
|        - | 2050 | ` * redirect). Bytes >= 0x80 are deliberately left alone -- php only folds the` |
|        - | 2051 | ` * ASCII control range.` |
|        - | 2052 | ` */` |
|      164 | 2053 | `static void VmUrlSetComponent(ph7_value *pValue,const SyString *pComp)` |
|        1 | 2054 | `{` |
|      165 | 2055 | `	const char *z = pComp->zString;` |
|      165 | 2056 | `	sxu32 n = pComp->nByte,i,iRun = 0;` |
|      165 | 2057 | `	if( n < 1 \|\| z == 0 ){` |
|        3 | 2058 | `		ph7_value_string(pValue,"",0);` |
|        3 | 2059 | `		return;` |
|        - | 2060 | `	}` |
|      955 | 2061 | `	for( i = 0 ; i < n ; ++i ){` |
|      793 | 2062 | `		unsigned char c = (unsigned char)z[i];` |
|      793 | 2063 | `		if( c < 0x20 \|\| c == 0x7f ){` |
|        3 | 2064 | `			if( i > iRun ){` |
|        3 | 2065 | `				ph7_value_string(pValue,&z[iRun],(int)(i - iRun));` |
|        1 | 2066 | `			}` |
|        3 | 2067 | `			ph7_value_string(pValue,"_",1);` |
|        3 | 2068 | `			iRun = i + 1;` |
|        1 | 2069 | `		}` |
|      397 | 2070 | `	}` |
|      163 | 2071 | `	if( n > iRun ){` |
|      163 | 2072 | `		ph7_value_string(pValue,&z[iRun],(int)(n - iRun));` |
|       81 | 2073 | `	}` |
|       83 | 2074 | `}` |
|      104 | 2075 | `PH7_PRIVATE int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 2076 | `{` |
|        - | 2077 | `	const char *zStr; /* Input string */` |
|        - | 2078 | `	VmUrlParts sUrl;  /* Parse of the given URI */` |
|        - | 2079 | `	SyString *pComp;` |
|        - | 2080 | `	int bHave;` |
|        - | 2081 | `	int nLen;` |
|      105 | 2082 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 2083 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 2084 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 2085 | `		return PH7_OK;` |
|        - | 2086 | `	}` |
|        - | 2087 | `	/* Extract the given URI. An empty string is NOT a failure: php parses it as` |
|        - | 2088 | `	 * an empty path. */` |
|      105 | 2089 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|      105 | 2090 | `	if( nLen < 0 ){` |
|      ! 0 | 2091 | `		nLen = 0;` |
|      ! 0 | 2092 | `	}` |
|      105 | 2093 | `	if( !PH7_VmUrlSplit(zStr,nLen,&sUrl) ){` |
|        - | 2094 | `		/* Malformed input,return FALSE */` |
|       13 | 2095 | `		ph7_result_bool(pCtx,0);` |
|       13 | 2096 | `		return PH7_OK;` |
|        - | 2097 | `	}` |
|      103 | 2098 | `	if( nArg > 1 && ph7_value_to_int(apArg[1]) >= 0 ){` |
|        - | 2099 | `		/* Refer to constant.c for constants values (php's PHP_URL_* are 0-based;` |
|        - | 2100 | `		 * PHL used to number them from 1, so every literal component id selected` |
|        - | 2101 | `		 * the WRONG field -- and the constants' own tests only asserted "%d",` |
|        - | 2102 | `		 * which any numbering satisfies). A negative id means "the whole array",` |
|        - | 2103 | `		 * which is what the default $component = -1 relies on. */` |
|       27 | 2104 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|       27 | 2105 | `		pComp = 0;` |
|       27 | 2106 | `		bHave = 0;` |
|       27 | 2107 | `		switch(nComponent){` |
|        3 | 2108 | `		case 0: /* PHP_URL_SCHEME */   pComp = &sUrl.sScheme;   bHave = sUrl.bScheme;   break;` |
|        5 | 2109 | `		case 1: /* PHP_URL_HOST */     pComp = &sUrl.sHost;     bHave = sUrl.bHost;     break;` |
|        2 | 2110 | `		case 2: /* PHP_URL_PORT */` |
|        5 | 2111 | `			if( sUrl.bPort ){` |
|        5 | 2112 | `				ph7_result_int(pCtx,sUrl.iPort);` |
|        3 | 2113 | `			}else{` |
|      ! 0 | 2114 | `				ph7_result_null(pCtx);` |
|        - | 2115 | `			}` |
|        5 | 2116 | `			return PH7_OK;` |
|        3 | 2117 | `		case 3: /* PHP_URL_USER */     pComp = &sUrl.sUser;     bHave = sUrl.bUser;     break;` |
|        3 | 2118 | `		case 4: /* PHP_URL_PASS */     pComp = &sUrl.sPass;     bHave = sUrl.bPass;     break;` |
|        3 | 2119 | `		case 5: /* PHP_URL_PATH */     pComp = &sUrl.sPath;     bHave = sUrl.bPath;     break;` |
|        5 | 2120 | `		case 6: /* PHP_URL_QUERY */    pComp = &sUrl.sQuery;    bHave = sUrl.bQuery;    break;` |
|        5 | 2121 | `		case 7: /* PHP_URL_FRAGMENT */ pComp = &sUrl.sFragment; bHave = sUrl.bFragment; break;` |
|        1 | 2122 | `		default:` |
|        4 | 2123 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|        - | 2124 | `				"parse_url(): Argument #2 ($component) must be a valid URL component identifier, %d given",` |
|        1 | 2125 | `				nComponent);` |
|        - | 2126 | `		}` |
|       21 | 2127 | `		if( bHave ){` |
|       19 | 2128 | `			ph7_value *pOut = ph7_context_new_scalar(pCtx);` |
|       19 | 2129 | `			if( pOut == 0 ){` |
|      ! 0 | 2130 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|      ! 0 | 2131 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 2132 | `				return PH7_OK;` |
|        - | 2133 | `			}` |
|       19 | 2134 | `			VmUrlSetComponent(pOut,pComp);` |
|       19 | 2135 | `			ph7_result_value(pCtx,pOut);` |
|       10 | 2136 | `		}else{` |
|        - | 2137 | `			/* No available value,return NULL */` |
|        3 | 2138 | `			ph7_result_null(pCtx);` |
|        - | 2139 | `		}` |
|       11 | 2140 | `	}else{` |
|        - | 2141 | `		ph7_value *pArray,*pValue;` |
|        - | 2142 | `		/* Return an associative array */` |
|       67 | 2143 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       67 | 2144 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       67 | 2145 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 2146 | `			/* Out of memory */` |
|      ! 0 | 2147 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 2148 | `			/* Return false */` |
|      ! 0 | 2149 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 2150 | `			return PH7_OK;` |
|        - | 2151 | `		}` |
|        - | 2152 | `		/* Fill the array, in php's key order. A component is emitted whenever it` |
|        - | 2153 | `		 * is PRESENT, even when empty -- parse_url("?") is ['query'=>'']. */` |
|       67 | 2154 | `		if( sUrl.bScheme ){` |
|       33 | 2155 | `			VmUrlSetComponent(pValue,&sUrl.sScheme);` |
|       33 | 2156 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|       33 | 2157 | `			ph7_value_reset_string_cursor(pValue);` |
|       16 | 2158 | `		}` |
|       67 | 2159 | `		if( sUrl.bHost ){` |
|       31 | 2160 | `			VmUrlSetComponent(pValue,&sUrl.sHost);` |
|       31 | 2161 | `			ph7_array_add_strkey_elem(pArray,"host",pValue);` |
|       31 | 2162 | `			ph7_value_reset_string_cursor(pValue);` |
|       15 | 2163 | `		}` |
|       67 | 2164 | `		if( sUrl.bPort ){` |
|       17 | 2165 | `			ph7_value_int(pValue,sUrl.iPort);` |
|       17 | 2166 | `			ph7_array_add_strkey_elem(pArray,"port",pValue);` |
|       17 | 2167 | `			ph7_value_reset_string_cursor(pValue);` |
|        8 | 2168 | `		}` |
|       67 | 2169 | `		if( sUrl.bUser ){` |
|        9 | 2170 | `			VmUrlSetComponent(pValue,&sUrl.sUser);` |
|        9 | 2171 | `			ph7_array_add_strkey_elem(pArray,"user",pValue);` |
|        9 | 2172 | `			ph7_value_reset_string_cursor(pValue);` |
|        4 | 2173 | `		}` |
|       67 | 2174 | `		if( sUrl.bPass ){` |
|        7 | 2175 | `			VmUrlSetComponent(pValue,&sUrl.sPass);` |
|        7 | 2176 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue);` |
|        7 | 2177 | `			ph7_value_reset_string_cursor(pValue);` |
|        3 | 2178 | `		}` |
|       67 | 2179 | `		if( sUrl.bPath ){` |
|       47 | 2180 | `			VmUrlSetComponent(pValue,&sUrl.sPath);` |
|       47 | 2181 | `			ph7_array_add_strkey_elem(pArray,"path",pValue);` |
|       47 | 2182 | `			ph7_value_reset_string_cursor(pValue);` |
|       23 | 2183 | `		}` |
|       67 | 2184 | `		if( sUrl.bQuery ){` |
|       13 | 2185 | `			VmUrlSetComponent(pValue,&sUrl.sQuery);` |
|       13 | 2186 | `			ph7_array_add_strkey_elem(pArray,"query",pValue);` |
|       13 | 2187 | `			ph7_value_reset_string_cursor(pValue);` |
|        6 | 2188 | `		}` |
|       67 | 2189 | `		if( sUrl.bFragment ){` |
|       13 | 2190 | `			VmUrlSetComponent(pValue,&sUrl.sFragment);` |
|       13 | 2191 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue);` |
|        6 | 2192 | `		}` |
|        - | 2193 | `		/* Return the created array */` |
|       67 | 2194 | `		ph7_result_value(pCtx,pArray);` |
|        - | 2195 | `		/* NOTE:` |
|        - | 2196 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 2197 | `		 * automatically as soon we return from this function.` |
|        - | 2198 | `		 */` |
|        - | 2199 | `	}` |
|        - | 2200 | `	/* All done */` |
|       87 | 2201 | `	return PH7_OK;` |
|       53 | 2202 | `}` |
|        - | 2203 |  |
|        - | 2204 | `/*` |
|        - | 2205 | ` * Section:` |
|        - | 2206 | ` *   Array related routines.` |
|        - | 2207 | ` * Status:` |
|        - | 2208 | ` *    Stable.` |
|        - | 2209 | ` * Note 2012-5-21 01:04:15:` |
|        - | 2210 | ` *  Array related functions that need access to the underlying` |
|        - | 2211 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 2212 | ` */` |
|        - | 2213 | `/*` |
|        - | 2214 | ` * The [compact()] function store it's state information in an instance` |
|        - | 2215 | ` * of the following structure.` |
|        - | 2216 | ` */` |
|        - | 2217 | `struct compact_data` |
|        - | 2218 | `{` |
|        - | 2219 | `	ph7_value *pArray;  /* Target array */` |
|        - | 2220 | `	int nRecCount;      /* Recursion count */` |
|        - | 2221 | `	ph7_context *pCtx;  /* Call context, for php's two warnings */` |
|        - | 2222 | `	int iArg;           /* 1-based ARGUMENT this element came from: php names the` |
|        - | 2223 | `	                     * argument even for an element found inside a nested` |
|        - | 2224 | `	                     * array, never the element's own position. */` |
|        - | 2225 | `};` |
|        - | 2226 | `/*` |
|        - | 2227 | ` * php's two compact() diagnostics, both E_WARNING and both non-fatal (the entry` |
|        - | 2228 | ` * is skipped and the rest of the call proceeds): a name that is neither a string` |
|        - | 2229 | ` * nor an array of strings, and a string naming a variable the frame does not` |
|        - | 2230 | ` * have. PHL emitted neither, so compact(1) and compact('typo') answered a` |
|        - | 2231 | ` * shorter array with nothing said -- the caller's only clue that a name was` |
|        - | 2232 | ` * dropped was the array's own size.` |
|        - | 2233 | ` */` |
|       14 | 2234 | `static void VmCompactBadName(ph7_context *pCtx,int iArg,ph7_value *pValue)` |
|        1 | 2235 | `{` |
|        - | 2236 | `	char zGiven[64];` |
|       22 | 2237 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 2238 | `		"Argument #%d must be string or array of strings, %s given",` |
|        7 | 2239 | `		iArg,VmValueGivenName(pValue,zGiven,sizeof(zGiven)));` |
|       15 | 2240 | `}` |
|        6 | 2241 | `static void VmCompactUndefined(ph7_context *pCtx,SyString *pVar)` |
|        1 | 2242 | `{` |
|       10 | 2243 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        6 | 2244 | `		"Undefined variable $%.*s",(int)pVar->nByte,pVar->zString);` |
|        7 | 2245 | `}` |
|        - | 2246 | `/*` |
|        - | 2247 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 2248 | ` */` |
|       16 | 2249 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 2250 | `{` |
|       17 | 2251 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|       17 | 2252 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|       17 | 2253 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 2254 | `	/* Act according to the hashmap value */` |
|       17 | 2255 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 2256 | `		SyString sVar;` |
|        9 | 2257 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|        - | 2258 | `		/* Query the current frame. An EMPTY name is looked up like any other:` |
|        - | 2259 | `		 * php reports it as "Undefined variable $" rather than skipping it. */` |
|        9 | 2260 | `		pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 2261 | `		/* ^` |
|        - | 2262 | `		 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 2263 | `		 */` |
|        9 | 2264 | `		if( pKey ){` |
|        - | 2265 | `			/* Perform the insertion */` |
|        7 | 2266 | `			ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|        4 | 2267 | `		}else{` |
|        3 | 2268 | `			VmCompactUndefined(pData->pCtx,&sVar);` |
|        1 | 2269 | `		}` |
|       13 | 2270 | `	}else if( ph7_value_is_array(pValue) ){` |
|        - | 2271 | `		/* Recursively traverse this array. Past the depth cap the element is` |
|        - | 2272 | `		 * dropped in silence, as it always was: the cap is PHL's own guard and` |
|        - | 2273 | `		 * the "must be string or array of strings" warning would be a lie about` |
|        - | 2274 | `		 * an argument that IS an array of strings. */` |
|        7 | 2275 | `		if( pData->nRecCount < 32 ){` |
|        - | 2276 | `			int rc;` |
|        7 | 2277 | `			pData->nRecCount++;` |
|        7 | 2278 | `			rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|        7 | 2279 | `			pData->nRecCount--;` |
|        7 | 2280 | `			return rc;` |
|        - | 2281 | `		}` |
|      ! 0 | 2282 | `	}else{` |
|        3 | 2283 | `		VmCompactBadName(pData->pCtx,pData->iArg,pValue);` |
|        - | 2284 | `	}` |
|       11 | 2285 | `	return SXRET_OK;` |
|        9 | 2286 | `}` |
|        - | 2287 | `/*` |
|        - | 2288 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 2289 | ` *  Create array containing variables and their values.` |
|        - | 2290 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 2291 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 2292 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 2293 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 2294 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 2295 | ` * Parameters` |
|        - | 2296 | ` *  $varname` |
|        - | 2297 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 2298 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 2299 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 2300 | ` *   it recursively.` |
|        - | 2301 | ` * Return` |
|        - | 2302 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 2303 | ` */` |
|       26 | 2304 | `PH7_PRIVATE int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 2305 | `{` |
|        - | 2306 | `	ph7_value *pArray,*pObj;` |
|       27 | 2307 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 2308 | `	const char *zName;` |
|        - | 2309 | `	SyString sVar;` |
|        - | 2310 | `	int i,nLen;` |
|       27 | 2311 | `	if( nArg < 1 ){` |
|        - | 2312 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 2313 | `		ph7_result_null(pCtx);` |
|      ! 0 | 2314 | `		return PH7_OK;` |
|        - | 2315 | `	}` |
|        - | 2316 | `	/* Create the array */` |
|       27 | 2317 | `	pArray = ph7_context_new_array(pCtx);` |
|       27 | 2318 | `	if( pArray == 0 ){` |
|        - | 2319 | `		/* Out of memory */` |
|      ! 0 | 2320 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 2321 | `		/* Return NULL */` |
|      ! 0 | 2322 | `		ph7_result_null(pCtx);` |
|      ! 0 | 2323 | `		return PH7_OK;` |
|        - | 2324 | `	}` |
|        - | 2325 | `	/* Perform the requested operation */` |
|       65 | 2326 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       39 | 2327 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|       19 | 2328 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 2329 | `				struct compact_data sData;` |
|        7 | 2330 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 2331 | `				/* Recursively walk the array */` |
|        7 | 2332 | `				sData.nRecCount = 0;` |
|        7 | 2333 | `				sData.pArray = pArray;` |
|        7 | 2334 | `				sData.pCtx = pCtx;` |
|        7 | 2335 | `				sData.iArg = i + 1;` |
|        7 | 2336 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|        4 | 2337 | `			}else{` |
|       13 | 2338 | `				VmCompactBadName(pCtx,i + 1,apArg[i]);` |
|        - | 2339 | `			}` |
|       10 | 2340 | `		}else{` |
|        - | 2341 | `			/* Extract variable name. An EMPTY one is looked up like any other:` |
|        - | 2342 | `			 * php reports it as "Undefined variable $" rather than skipping it. */` |
|       21 | 2343 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|       21 | 2344 | `			SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 2345 | `			/* Check if the variable is available in the current frame */` |
|       21 | 2346 | `			pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|       21 | 2347 | `			if( pObj ){` |
|       17 | 2348 | `				ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        9 | 2349 | `			}else{` |
|        5 | 2350 | `				VmCompactUndefined(pCtx,&sVar);` |
|        - | 2351 | `			}` |
|        - | 2352 | `		}` |
|       20 | 2353 | `	}` |
|        - | 2354 | `	/* Return the array */` |
|       27 | 2355 | `	ph7_result_value(pCtx,pArray);` |
|       27 | 2356 | `	return PH7_OK;` |
|       14 | 2357 | `}` |
|        - | 2358 | `/*` |
|        - | 2359 | ` * The [import_request_variables()] function store it's state information` |
|        - | 2360 | ` * in an instance of the following structure.` |
|        - | 2361 | ` */` |
|        - | 2362 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 2363 | `struct extract_aux_data` |
|        - | 2364 | `{` |
|        - | 2365 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 2366 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 2367 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 2368 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 2369 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 2370 | `};` |
|        - | 2371 | `/*` |
|        - | 2372 | ` * php's php_valid_var_name(): a legal PHP variable name matches` |
|        - | 2373 | ` * [A-Za-z_\x80-\xff][A-Za-z0-9_\x80-\xff]* . Byte-wise and locale-free on` |
|        - | 2374 | ` * purpose (php has been locale-independent here since 8.0); the high-byte` |
|        - | 2375 | ` * range is what lets UTF-8 identifiers through. extract() drops every key` |
|        - | 2376 | ` * that does not pass, instead of installing an unreachable variable.` |
|        - | 2377 | ` */` |
|      158 | 2378 | `static int VmIsValidVarName(const char *zName,sxu32 nByte)` |
|        4 | 2379 | `{` |
|        - | 2380 | `	unsigned char c;` |
|        - | 2381 | `	sxu32 i;` |
|      162 | 2382 | `	if( nByte < 1 ){` |
|        7 | 2383 | `		return FALSE;` |
|        - | 2384 | `	}` |
|      156 | 2385 | `	c = (unsigned char)zName[0];` |
|      156 | 2386 | `	if( c != '_' && !(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') && c < 0x80 ){` |
|       11 | 2387 | `		return FALSE;` |
|        - | 2388 | `	}` |
|      388 | 2389 | `	for( i = 1 ; i < nByte ; ++i ){` |
|      263 | 2390 | `		c = (unsigned char)zName[i];` |
|      260 | 2391 | `		if( c != '_' && !(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z')` |
|       80 | 2392 | `		 && !(c >= '0' && c <= '9') && c < 0x80 ){` |
|       20 | 2393 | `			return FALSE;` |
|        - | 2394 | `		}` |
|      124 | 2395 | `	}` |
|      128 | 2396 | `	return TRUE;` |
|       83 | 2397 | `}` |
|        - | 2398 | `/* TRUE when the name is exactly "this": php refuses to re-assign $this. */` |
|      166 | 2399 | `static int VmExtractIsThis(const char *zName,sxu32 nByte)` |
|        4 | 2400 | `{` |
|      170 | 2401 | `	return nByte == sizeof("this")-1 && SyMemcmp(zName,"this",sizeof("this")-1) == 0;` |
|        4 | 2402 | `}` |
|        - | 2403 | `/*` |
|        - | 2404 | ` * TRUE when the name belongs to the superglobal table ($GLOBALS, $_SERVER,` |
|        - | 2405 | ` * $_GET, …). php hands extract() a per-frame symbol table that holds no` |
|        - | 2406 | ` * superglobal, so such a key lands in the LOCAL table and the real superglobal` |
|        - | 2407 | ` * is untouched. In PHL the name resolves to the superglobal SLOT itself` |
|        - | 2408 | ` * (VmExtractMemObj consults hSuper first), so a plain store would replace` |
|        - | 2409 | ` * $GLOBALS/$_SERVER with the imported value and take the whole symbol table /` |
|        - | 2410 | ` * request environment with it. Those keys are dropped instead — a prefixed` |
|        - | 2411 | ` * name ($p__SERVER) is a normal local and stores fine.` |
|        - | 2412 | ` */` |
|      114 | 2413 | `static int VmExtractIsProtected(ph7_vm *pVm,const char *zName,sxu32 nByte)` |
|        4 | 2414 | `{` |
|      118 | 2415 | `	return SyHashGet(&pVm->hSuper,(const void *)zName,nByte) != 0;` |
|        4 | 2416 | `}` |
|        - | 2417 | `/*` |
|        - | 2418 | ` * TRUE when the calling frame already holds this variable name.` |
|        - | 2419 | ` * "this" and the superglobals always answer FALSE, matching the symbol table` |
|        - | 2420 | ` * php hands extract(): $this is bound implicitly and superglobals live outside` |
|        - | 2421 | ` * the frame. So EXTR_IF_EXISTS/EXTR_PREFIX_IF_EXISTS skip those keys (php-exact)` |
|        - | 2422 | ` * and EXTR_PREFIX_SAME takes its not-a-collision branch.` |
|        - | 2423 | ` */` |
|       72 | 2424 | `static int VmExtractVarExists(ph7_vm *pVm,const char *zName,sxu32 nByte)` |
|        4 | 2425 | `{` |
|        - | 2426 | `	SyString sVar;` |
|       76 | 2427 | `	if( VmExtractIsThis(zName,nByte) \|\| VmExtractIsProtected(pVm,zName,nByte) ){` |
|       20 | 2428 | `		return FALSE;` |
|        - | 2429 | `	}` |
|       57 | 2430 | `	SyStringInitFromBuf(&sVar,zName,nByte);` |
|       57 | 2431 | `	return VmExtractMemObj(pVm,&sVar,FALSE,FALSE) != 0;` |
|       40 | 2432 | `}` |
|        - | 2433 | `/*` |
|        - | 2434 | ` * Create-or-overwrite a variable of the calling frame with a copy of pValue.` |
|        - | 2435 | ` * Returns TRUE when the variable was written (php counts exactly those).` |
|        - | 2436 | ` */` |
|        - | 2437 | `/*` |
|        - | 2438 | ` * EXTR_REFS: bind the imported NAME to the array element's own slot instead of copying` |
|        - | 2439 | ` * the value, so a later write through the variable reaches the caller's array — php's` |
|        - | 2440 | ` * by-reference extraction. The binding is registered (PH7_VmBindVarSlot), which is what` |
|        - | 2441 | ` * makes the element count the new name as a holder and read as a reference.` |
|        - | 2442 | ` *` |
|        - | 2443 | ` * The name is DUPLICATED: the symbol table keeps the pointer it is given, and the caller's` |
|        - | 2444 | ` * is a scratch blob the next entry reuses.` |
|        - | 2445 | ` */` |
|        8 | 2446 | `static int VmExtractBindVar(ph7_vm *pVm,const char *zName,sxu32 nByte,sxu32 nIdx)` |
|        1 | 2447 | `{` |
|        9 | 2448 | `	VmFrame *pFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|        - | 2449 | `	char *zDup;` |
|        9 | 2450 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 | 2451 | `		return FALSE;` |
|        - | 2452 | `	}` |
|        9 | 2453 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zName,nByte);` |
|        9 | 2454 | `	if( zDup == 0 ){` |
|      ! 0 | 2455 | `		return FALSE;` |
|        - | 2456 | `	}` |
|        9 | 2457 | `	PH7_VmBindVarSlot(&(*pVm),pFrame,zDup,nByte,nIdx);` |
|        9 | 2458 | `	return TRUE;` |
|        5 | 2459 | `}` |
|       62 | 2460 | `static int VmExtractStoreVar(ph7_vm *pVm,const char *zName,sxu32 nByte,ph7_value *pValue)` |
|        3 | 2461 | `{` |
|        - | 2462 | `	ph7_value *pObj;` |
|        - | 2463 | `	SyString sVar;` |
|       65 | 2464 | `	SyStringInitFromBuf(&sVar,zName,nByte);` |
|        - | 2465 | `	/* bDup: the name lives in a scratch blob that is reused by the next entry */` |
|       65 | 2466 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|       65 | 2467 | `	if( pObj == 0 ){` |
|      ! 0 | 2468 | `		return FALSE;` |
|        - | 2469 | `	}` |
|       65 | 2470 | `	PH7_MemObjStore(pValue,pObj);` |
|       65 | 2471 | `	return TRUE;` |
|       34 | 2472 | `}` |
|        - | 2473 | `/*` |
|        - | 2474 | ` * Build php's prefixed name "<prefix>_<key>" (php_prefix_varname with` |
|        - | 2475 | ` * add_underscore): the separator is unconditional, so an empty prefix still` |
|        - | 2476 | ` * yields "_key" exactly like php.` |
|        - | 2477 | ` */` |
|       40 | 2478 | `static sxi32 VmExtractPrefixName(SyBlob *pOut,const char *zPrefix,int nPrefix,` |
|        - | 2479 | `	const char *zKey,sxu32 nKey)` |
|        3 | 2480 | `{` |
|       43 | 2481 | `	SyBlobReset(pOut);` |
|       43 | 2482 | `	if( nPrefix > 0 && SyBlobAppend(pOut,zPrefix,(sxu32)nPrefix) != SXRET_OK ){` |
|      ! 0 | 2483 | `		return SXERR_MEM;` |
|        - | 2484 | `	}` |
|       43 | 2485 | `	if( SyBlobAppend(pOut,"_",sizeof(char)) != SXRET_OK ){` |
|      ! 0 | 2486 | `		return SXERR_MEM;` |
|        - | 2487 | `	}` |
|       43 | 2488 | `	if( nKey > 0 && SyBlobAppend(pOut,zKey,nKey) != SXRET_OK ){` |
|      ! 0 | 2489 | `		return SXERR_MEM;` |
|        - | 2490 | `	}` |
|       43 | 2491 | `	return SXRET_OK;` |
|       23 | 2492 | `}` |
|        - | 2493 | `/* What to do with one array entry, decided by the extract mode. */` |
|        - | 2494 | `#define VM_EXTRACT_DROP     0 /* php skips this key entirely */` |
|        - | 2495 | `#define VM_EXTRACT_PLAIN    1 /* install under the key itself */` |
|        - | 2496 | `#define VM_EXTRACT_PREFIX   2 /* install under "<prefix>_<key>" */` |
|        - | 2497 | `/*` |
|        - | 2498 | ` * int extract(array &$array[,int $flags = EXTR_OVERWRITE[,string $prefix = "" ]])` |
|        - | 2499 | ` *   Import variables into the current symbol table from an array.` |
|        - | 2500 | ` *` |
|        - | 2501 | ` * $flags is php's ENUM (PH7_EXTR_* in ph7int.h), not a bitmask: the mode is` |
|        - | 2502 | ` * (flags & 0xff) and anything above EXTR_IF_EXISTS(6) is a ValueError. The` |
|        - | 2503 | ` * modes, mirroring php's per-mode helpers in ext/standard/array.c:` |
|        - | 2504 | ` *   EXTR_OVERWRITE(0)        collisions overwrite` |
|        - | 2505 | ` *   EXTR_SKIP(1)             collisions keep the existing variable` |
|        - | 2506 | ` *   EXTR_PREFIX_SAME(2)      collisions install "<prefix>_<key>"` |
|        - | 2507 | ` *   EXTR_PREFIX_ALL(3)       every key installs as "<prefix>_<key>"` |
|        - | 2508 | ` *   EXTR_PREFIX_INVALID(4)   only illegal names (and numeric keys) get prefixed` |
|        - | 2509 | ` *   EXTR_PREFIX_IF_EXISTS(5) install "<prefix>_<key>" only if <key> exists` |
|        - | 2510 | ` *   EXTR_IF_EXISTS(6)        overwrite only variables that already exist` |
|        - | 2511 | `` * Modes 2..5 require $prefix (php: `is required when using this extract type`),`` |
|        - | 2512 | ` * a non-empty $prefix must itself be a legal identifier, and a key whose final` |
|        - | 2513 | ` * name is not a legal variable name is dropped rather than installed. Only` |
|        - | 2514 | ` * EXTR_PREFIX_ALL/EXTR_PREFIX_INVALID look at numeric keys at all.` |
|        - | 2515 | ` *` |
|        - | 2516 | `` * $this is never a target: php throws `Cannot re-assign $this` where a store`` |
|        - | 2517 | ` * would land on it, and skips it where a store would not (EXTR_SKIP), and` |
|        - | 2518 | ` * $GLOBALS is never clobbered.` |
|        - | 2519 | ` * Return` |
|        - | 2520 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 2521 | ` */` |
|      102 | 2522 | `PH7_PRIVATE int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        4 | 2523 | `{` |
|      106 | 2524 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 2525 | `	ph7_hashmap_node *pEntry;` |
|        - | 2526 | `	ph7_hashmap *pMap;` |
|      106 | 2527 | `	const char *zPrefix = 0;` |
|      106 | 2528 | `	sxi64 iFlags = PH7_EXTR_OVERWRITE;` |
|      106 | 2529 | `	sxi64 iCount = 0;` |
|        - | 2530 | `	ph7_value sValue;` |
|        - | 2531 | `	SyBlob sWorker;` |
|      106 | 2532 | `	int nPrefix = 0;` |
|      106 | 2533 | `	sxi32 rc = PH7_OK;` |
|        - | 2534 | `	int iType;` |
|        - | 2535 | `	sxu32 n;` |
|      106 | 2536 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|        - | 2537 | `		char zBuf[64];` |
|      ! 0 | 2538 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|        - | 2539 | `			"extract(): Argument #1 ($array) must be of type array, %s given",` |
|      ! 0 | 2540 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|        - | 2541 | `	}` |
|      106 | 2542 | `	if( nArg > 1 ){` |
|       98 | 2543 | `		rc = PH7_IntArgResolve(pCtx,apArg[1],"extract",2,"$flags","int",&iFlags);` |
|       98 | 2544 | `		if( rc != PH7_OK ){` |
|      ! 0 | 2545 | `			return rc;` |
|        - | 2546 | `		}` |
|       47 | 2547 | `	}` |
|        - | 2548 | `	/* php: the mode is the low byte; EXTR_REFS(0x100) rides above it */` |
|      106 | 2549 | `	iType = (int)(iFlags & 0xff);` |
|      106 | 2550 | `	if( iType > PH7_EXTR_IF_EXISTS ){` |
|        7 | 2551 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|        - | 2552 | `			"extract(): Argument #2 ($flags) must be a valid extract type");` |
|        - | 2553 | `	}` |
|      100 | 2554 | `	if( iType > PH7_EXTR_SKIP && iType <= PH7_EXTR_PREFIX_IF_EXISTS && nArg < 3 ){` |
|       12 | 2555 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|        - | 2556 | `			"extract(): Argument #3 ($prefix) is required when using this extract type");` |
|        - | 2557 | `	}` |
|       90 | 2558 | `	if( nArg > 2 ){` |
|       41 | 2559 | `		zPrefix = ph7_value_to_string(apArg[2],&nPrefix);` |
|       41 | 2560 | `		if( nPrefix > 0 && !VmIsValidVarName(zPrefix,(sxu32)nPrefix) ){` |
|        5 | 2561 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|        - | 2562 | `				"extract(): Argument #3 ($prefix) must be a valid identifier");` |
|        - | 2563 | `		}` |
|       17 | 2564 | `	}` |
|        - | 2565 | `	/* Point to the target hashmap */` |
|       86 | 2566 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       86 | 2567 | `	if( pMap->nEntry < 1 ){` |
|        - | 2568 | `		/* Empty map,return  0 */` |
|      ! 0 | 2569 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 2570 | `		return PH7_OK;` |
|        - | 2571 | `	}` |
|       86 | 2572 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|       86 | 2573 | `	PH7_MemObjInit(pVm,&sValue);` |
|        - | 2574 | `	/* php walks a COPY of the array, so an entry that overwrites the caller's own` |
|        - | 2575 | `	 * array variable ($arr = ['arr'=>1]; extract($arr)) cannot pull the map from` |
|        - | 2576 | `	 * under the walk. Pinning the map for the walk is the same guarantee. */` |
|       86 | 2577 | `	pMap->iRef++;` |
|       86 | 2578 | `	pEntry = pMap->pFirst;` |
|        - | 2579 | `	/* pFirst walks the insertion order through pPrev — PH7 links the entry list` |
|        - | 2580 | `	 * in reverse (same traversal PH7_HashmapWalk uses). */` |
|      252 | 2581 | `	for( n = pMap->nEntry ; n > 0 && pEntry ; --n, pEntry = pEntry->pPrev ){` |
|        - | 2582 | `		const char *zKey, *zFinal;` |
|        - | 2583 | `		sxu32 nKey, nFinal;` |
|        - | 2584 | `		char zNum[32];` |
|        - | 2585 | `		int bIntKey, iAction;` |
|        - | 2586 | `		/* Work off a COPY of the entry value: installing a variable can grow` |
|        - | 2587 | `		 * pVm->aMemObj, and a pointer into that set would dangle across the` |
|        - | 2588 | `		 * reallocation (this is why the walk API hands out copies too). The` |
|        - | 2589 | `		 * release comes FIRST so it covers every continue/goto below: the load` |
|        - | 2590 | `		 * takes a reference on an array/object value and does not drop the one` |
|        - | 2591 | `		 * the previous entry left behind (PH7_HashmapWalk releases per iteration` |
|        - | 2592 | `		 * for the same reason — without it a whole-array extract() pins every` |
|        - | 2593 | `		 * value it copied, and their destructors never run). */` |
|      172 | 2594 | `		PH7_MemObjRelease(&sValue);` |
|      172 | 2595 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|      172 | 2596 | `		bIntKey = (pEntry->iType == HASHMAP_INT_NODE);` |
|      172 | 2597 | `		if( bIntKey ){` |
|        - | 2598 | `			/* Only the two prefixing modes below ever look at a numeric key */` |
|       22 | 2599 | `			nKey = SyBufferFormat(zNum,sizeof(zNum),"%qd",pEntry->xKey.iKey);` |
|       22 | 2600 | `			zKey = zNum;` |
|       12 | 2601 | `		}else{` |
|      152 | 2602 | `			zKey = (const char *)SyBlobData(&pEntry->xKey.sKey);` |
|      152 | 2603 | `			nKey = SyBlobLength(&pEntry->xKey.sKey);` |
|        - | 2604 | `		}` |
|      172 | 2605 | `		iAction = VM_EXTRACT_DROP;` |
|      172 | 2606 | `		switch( iType ){` |
|       19 | 2607 | `		case PH7_EXTR_OVERWRITE:` |
|       42 | 2608 | `			if( bIntKey \|\| !VmIsValidVarName(zKey,nKey) ){` |
|        8 | 2609 | `				break;` |
|        - | 2610 | `			}` |
|       30 | 2611 | `			if( VmExtractIsThis(zKey,nKey) ){` |
|        3 | 2612 | `				goto this_error;` |
|        - | 2613 | `			}` |
|       28 | 2614 | `			iAction = VM_EXTRACT_PLAIN;` |
|       28 | 2615 | `			break;` |
|       12 | 2616 | `		case PH7_EXTR_SKIP:` |
|       27 | 2617 | `			if( bIntKey \|\| !VmIsValidVarName(zKey,nKey) \|\| VmExtractIsThis(zKey,nKey) ){` |
|        6 | 2618 | `				break;` |
|        - | 2619 | `			}` |
|       17 | 2620 | `			if( VmExtractVarExists(pVm,zKey,nKey) ){` |
|        8 | 2621 | `				break; /* collision: keep the existing variable */` |
|        - | 2622 | `			}` |
|       10 | 2623 | `			iAction = VM_EXTRACT_PLAIN;` |
|       10 | 2624 | `			break;` |
|       16 | 2625 | `		case PH7_EXTR_IF_EXISTS:` |
|       36 | 2626 | `			if( bIntKey \|\| !VmExtractVarExists(pVm,zKey,nKey) ){` |
|       16 | 2627 | `				break;` |
|        - | 2628 | `			}` |
|        8 | 2629 | `			if( !VmIsValidVarName(zKey,nKey) ){` |
|      ! 0 | 2630 | `				break;` |
|        - | 2631 | `			}` |
|        8 | 2632 | `			if( VmExtractIsThis(zKey,nKey) ){` |
|      ! 0 | 2633 | `				goto this_error;` |
|        - | 2634 | `			}` |
|        8 | 2635 | `			iAction = VM_EXTRACT_PLAIN;` |
|        8 | 2636 | `			break;` |
|        9 | 2637 | `		case PH7_EXTR_PREFIX_SAME:` |
|       21 | 2638 | `			if( bIntKey \|\| nKey < 1 ){` |
|        3 | 2639 | `				break;` |
|        - | 2640 | `			}` |
|       17 | 2641 | `			if( VmExtractVarExists(pVm,zKey,nKey) ){` |
|        6 | 2642 | `				iAction = VM_EXTRACT_PREFIX; /* collision */` |
|       14 | 2643 | `			}else if( !VmIsValidVarName(zKey,nKey) ){` |
|        5 | 2644 | `				break;` |
|      ! 0 | 2645 | `			}else{` |
|        - | 2646 | `				/* $this cannot be a target, but its prefixed form can */` |
|        8 | 2647 | `				iAction = VmExtractIsThis(zKey,nKey) ? VM_EXTRACT_PREFIX : VM_EXTRACT_PLAIN;` |
|        - | 2648 | `			}` |
|       13 | 2649 | `			break;` |
|       13 | 2650 | `		case PH7_EXTR_PREFIX_ALL:` |
|       28 | 2651 | `			if( !bIntKey && nKey < 1 ){` |
|        3 | 2652 | `				break;` |
|        - | 2653 | `			}` |
|       26 | 2654 | `			iAction = VM_EXTRACT_PREFIX;` |
|       26 | 2655 | `			break;` |
|        7 | 2656 | `		case PH7_EXTR_PREFIX_INVALID:` |
|       15 | 2657 | `			iAction = (bIntKey \|\| !VmIsValidVarName(zKey,nKey) \|\| VmExtractIsThis(zKey,nKey))` |
|       13 | 2658 | `				? VM_EXTRACT_PREFIX : VM_EXTRACT_PLAIN;` |
|       16 | 2659 | `			break;` |
|        8 | 2660 | `		case PH7_EXTR_PREFIX_IF_EXISTS:` |
|       18 | 2661 | `			if( !bIntKey && VmExtractVarExists(pVm,zKey,nKey) ){` |
|        3 | 2662 | `				iAction = VM_EXTRACT_PREFIX;` |
|        1 | 2663 | `			}` |
|       16 | 2664 | `			break;` |
|      ! 0 | 2665 | `		default:` |
|      ! 0 | 2666 | `			break;` |
|        - | 2667 | `		}` |
|      170 | 2668 | `		if( iAction == VM_EXTRACT_DROP ){` |
|      126 | 2669 | `			continue;` |
|        - | 2670 | `		}` |
|       92 | 2671 | `		if( iAction == VM_EXTRACT_PREFIX ){` |
|       43 | 2672 | `			if( VmExtractPrefixName(&sWorker,zPrefix,nPrefix,zKey,nKey) != SXRET_OK ){` |
|      ! 0 | 2673 | `				rc = PH7_VmThrowException(pCtx,"Error","PH7 engine is running out of memory");` |
|      ! 0 | 2674 | `				goto done;` |
|        - | 2675 | `			}` |
|       43 | 2676 | `			zFinal = (const char *)SyBlobData(&sWorker);` |
|       43 | 2677 | `			nFinal = SyBlobLength(&sWorker);` |
|       43 | 2678 | `			if( !VmIsValidVarName(zFinal,nFinal) ){` |
|        7 | 2679 | `				continue; /* php drops a prefixed name that is not an identifier */` |
|        - | 2680 | `			}` |
|       37 | 2681 | `			if( VmExtractIsThis(zFinal,nFinal) ){` |
|      ! 0 | 2682 | `				goto this_error;` |
|        - | 2683 | `			}` |
|       20 | 2684 | `		}else{` |
|       52 | 2685 | `			if( VmExtractIsProtected(pVm,zKey,nKey) ){` |
|       13 | 2686 | `				continue; /* $GLOBALS/$_SERVER/... are never a plain target */` |
|        - | 2687 | `			}` |
|       39 | 2688 | `			zFinal = zKey;` |
|       39 | 2689 | `			nFinal = nKey;` |
|        - | 2690 | `		}` |
|       74 | 2691 | `		if( iFlags & PH7_EXTR_REFS ){` |
|        - | 2692 | `			/* Bind to the element's own slot (php's EXTR_REFS) rather than copying */` |
|        9 | 2693 | `			if( VmExtractBindVar(pVm,zFinal,nFinal,pEntry->nValIdx) ){` |
|        9 | 2694 | `				iCount++;` |
|        5 | 2695 | `			}` |
|       69 | 2696 | `		}else if( VmExtractStoreVar(pVm,zFinal,nFinal,&sValue) ){` |
|       65 | 2697 | `			iCount++;` |
|       31 | 2698 | `		}` |
|       74 | 2699 | `		continue;` |
|        1 | 2700 | `this_error:` |
|        3 | 2701 | `		rc = PH7_VmThrowException(pCtx,"Error","Cannot re-assign $this");` |
|        3 | 2702 | `		goto done;` |
|      ! 0 | 2703 | `	}` |
|        - | 2704 | `	/* Number of variables successfully imported */` |
|       84 | 2705 | `	ph7_result_int64(pCtx,iCount);` |
|       41 | 2706 | `done:` |
|       86 | 2707 | `	PH7_MemObjRelease(&sValue);` |
|       86 | 2708 | `	SyBlobRelease(&sWorker);` |
|       86 | 2709 | `	PH7_HashmapUnref(pMap);` |
|       86 | 2710 | `	return rc;` |
|       55 | 2711 | `}` |
|        - | 2712 | `/*` |
|        - | 2713 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 2714 | ` * defined below.` |
|        - | 2715 | ` */` |
|        2 | 2716 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 2717 | `{` |
|        3 | 2718 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 2719 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 2720 | `	ph7_value *pObj;` |
|        - | 2721 | `	SyString sVar;` |
|        - | 2722 | `	/* Perform a string cast */` |
|        3 | 2723 | `	PH7_MemObjToString(pKey);` |
|        3 | 2724 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 2725 | `		/* Unavailable variable name */` |
|      ! 0 | 2726 | `		return SXRET_OK;` |
|        - | 2727 | `	}` |
|        3 | 2728 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 2729 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 2730 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 2731 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 2732 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 2733 | `			);` |
|        2 | 2734 | `	}else{` |
|      ! 0 | 2735 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 2736 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 2737 | `	}` |
|        3 | 2738 | `	sVar.zString = pAux->zWorker;` |
|        - | 2739 | `	/* Extract the variable */` |
|        3 | 2740 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 2741 | `	if( pObj ){` |
|        3 | 2742 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 2743 | `	}` |
|        3 | 2744 | `	return SXRET_OK;` |
|        2 | 2745 | `}` |
|        - | 2746 | `/*` |
|        - | 2747 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 2748 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 2749 | ` * Parameters` |
|        - | 2750 | ` * $types` |
|        - | 2751 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 2752 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 2753 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 2754 | ` *  POST includes the POST uploaded file information.` |
|        - | 2755 | ` *  Note:` |
|        - | 2756 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 2757 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 2758 | ` * $prefix` |
|        - | 2759 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 2760 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 2761 | ` *  variable named $pref_userid.` |
|        - | 2762 | ` * Return` |
|        - | 2763 | ` *  TRUE on success or FALSE on failure.` |
|        - | 2764 | ` */` |
|        2 | 2765 | `PH7_PRIVATE int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 2766 | `{` |
|        - | 2767 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 2768 | `	extract_aux_data sAux;` |
|        - | 2769 | `	int nLen,nPrefixLen;` |
|        - | 2770 | `	ph7_value *pSuper;` |
|        - | 2771 | `	ph7_vm *pVm;` |
|        - | 2772 | `	/* By default import only $_GET variables  */` |
|        3 | 2773 | `	zImport = "G";` |
|        3 | 2774 | `	nLen = (int)sizeof(char);` |
|        3 | 2775 | `	zPrefix = 0;` |
|        3 | 2776 | `	nPrefixLen = 0;` |
|        3 | 2777 | `	if( nArg > 0 ){` |
|        3 | 2778 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 2779 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 2780 | `		}` |
|        3 | 2781 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 2782 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 2783 | `		}` |
|        1 | 2784 | `	}` |
|        - | 2785 | `	/* Point to the underlying VM */` |
|        3 | 2786 | `	pVm = pCtx->pVm;` |
|        - | 2787 | `	/* Initialize the aux data */` |
|        3 | 2788 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 2789 | `	sAux.zPrefix = zPrefix;` |
|        3 | 2790 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 2791 | `	sAux.pVm = pVm;` |
|        - | 2792 | `	/* Extract */` |
|        3 | 2793 | `	zEnd = &zImport[nLen];` |
|        5 | 2794 | `	while( zImport < zEnd ){` |
|        3 | 2795 | `		int c = zImport[0];` |
|        3 | 2796 | `		pSuper = 0;` |
|        3 | 2797 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 2798 | `			/* Import $_GET variables */` |
|        3 | 2799 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 2800 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 2801 | `			/* Import $_POST variables */` |
|      ! 0 | 2802 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 2803 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 2804 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 2805 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 2806 | `		}` |
|        3 | 2807 | `		if( pSuper ){` |
|        - | 2808 | `			/* Iterate throw array entries */` |
|        3 | 2809 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 2810 | `		}` |
|        - | 2811 | `		/* Advance the cursor */` |
|        3 | 2812 | `		zImport++;` |
|        1 | 2813 | `	}` |
|        - | 2814 | `	/* All done,return TRUE*/` |
|        3 | 2815 | `	ph7_result_bool(pCtx,0);` |
|        3 | 2816 | `	return PH7_OK;` |
|        1 | 2817 | `}` |
|        - | 2818 |  |
