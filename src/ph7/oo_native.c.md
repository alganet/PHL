# src/ph7/oo_native.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 108/187 lines (57.75%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    4 | ` */` |
|      - |    5 | `#include "ph7int.h"` |
|      - |    6 | `/*` |
|      - |    7 | ` * Declaring a class from C.` |
|      - |    8 | ` *` |
|      - |    9 | ` * Every built-in class subsystem used to be an embedded PHP source string compiled` |
|      - |   10 | ` * at VM init, reaching the engine through a GLOBAL C thunk per operation` |
|      - |   11 | `` * (`__reflect_class_info()`, `__gen_next()`, `__dom_*`, ~110 of them). The reason`` |
|      - |   12 | ` * was structural: a ph7_class_method carries a ph7_vm_func, whose only body is` |
|      - |   13 | ` * bytecode, so C code could only ever be a global function.` |
|      - |   14 | ` *` |
|      - |   15 | ` * VM_FUNC_NATIVE removed that restriction (a method body may be a C routine), and` |
|      - |   16 | ` * this file is the front door to it: a declarative table describing a class —` |
|      - |   17 | ` * parent, interfaces, constants, methods — that PH7_InstallNativeClasses() turns` |
|      - |   18 | ` * into a real, mounted ph7_class. The thunks become what they always were,` |
|      - |   19 | ` * methods, and stop being visible in the global namespace.` |
|      - |   20 | ` *` |
|      - |   21 | ` * Nothing here is new machinery. It drives the same builders the COMPILER drives` |
|      - |   22 | `` * for `class Foo {}` — PH7_NewRawClass, PH7_NewClassMethod, PH7_ClassInstallMethod,`` |
|      - |   23 | ` * PH7_ClassInherit, PH7_ClassImplement, PH7_VmInstallClass, VmMountUserClass — so a` |
|      - |   24 | ` * native class is not a second kind of class: Reflection, instanceof, inheritance,` |
|      - |   25 | ` * visibility and autoloading all see an ordinary one.` |
|      - |   26 | ` */` |
|      - |   27 | `/*` |
|      - |   28 | ` * Materialize a native declaration's literal initializer into a value slot.` |
|      - |   29 | ` *` |
|      - |   30 | ` * A compiled declaration expresses its default as byte-code evaluated at mount` |
|      - |   31 | `` * (constants, statics) or at `new` (instance properties). The C builder has no`` |
|      - |   32 | ` * compiler to emit that, so it carries the literal on the attribute` |
|      - |   33 | ` * (ph7_class_attr::pNativeValue) and both of those sites call this instead --` |
|      - |   34 | ` * which is what a literal initializer's byte-code would have produced anyway.` |
|      - |   35 | ` */` |
|    898 |   36 | `PH7_PRIVATE void PH7_NativeLiteralValue(ph7_vm *pVm,const void *pLiteral,ph7_value *pOut)` |
|      5 |   37 | `{` |
|    903 |   38 | `	const PH7_NativeConstDef *pLit = (const PH7_NativeConstDef *)pLiteral;` |
|    903 |   39 | `	switch( pLit->iType ){` |
|    ! 0 |   40 | `		case PH7_NATIVE_VAL_INT:` |
|    ! 0 |   41 | `			PH7_MemObjInitFromInt(&(*pVm),pOut,pLit->iValue);` |
|    ! 0 |   42 | `			break;` |
|    ! 0 |   43 | `		case PH7_NATIVE_VAL_BOOL:` |
|    ! 0 |   44 | `			PH7_MemObjInitFromBool(&(*pVm),pOut,(sxi32)pLit->iValue);` |
|    ! 0 |   45 | `			break;` |
|    ! 0 |   46 | `		case PH7_NATIVE_VAL_STRING: {` |
|      - |   47 | `			SyString sLit;` |
|    ! 0 |   48 | `			SyStringInitFromBuf(&sLit,pLit->zValue,SyStrlen(pLit->zValue));` |
|    ! 0 |   49 | `			PH7_MemObjInitFromString(&(*pVm),pOut,&sLit);` |
|    ! 0 |   50 | `			break;` |
|      - |   51 | `		}` |
|      - |   52 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|    ! 0 |   53 | `		case PH7_NATIVE_VAL_DOUBLE:` |
|    ! 0 |   54 | `			PH7_MemObjInitFromReal(&(*pVm),pOut,pLit->rValue);` |
|    ! 0 |   55 | `			break;` |
|      - |   56 | `#endif` |
|    449 |   57 | `		default:` |
|    903 |   58 | `			PH7_MemObjInit(&(*pVm),pOut);` |
|    898 |   59 | `			break;` |
|      - |   60 | `	}` |
|    903 |   61 | `}` |
|      - |   62 | `/*` |
|      - |   63 | ` * Resolve a class by name for the builder's own use (parents and interfaces).` |
|      - |   64 | ` * Autoload is deliberately NOT triggered: these run at VM init, where the only` |
|      - |   65 | ` * classes that can exist are the ones installed before this call, and a missing` |
|      - |   66 | ` * name is a build-order bug in the engine, not a userland lookup.` |
|      - |   67 | ` */` |
|    ! 0 |   68 | `static ph7_class * NativeLookupClass(ph7_vm *pVm,const char *zName)` |
|    ! 0 |   69 | `{` |
|    ! 0 |   70 | `	return PH7_VmExtractClass(&(*pVm),zName,(sxu32)SyStrlen(zName),FALSE,0);` |
|    ! 0 |   71 | `}` |
|      - |   72 | `/*` |
|      - |   73 | ` * Translate the builder's PH7_MOD_* modifier bits into the protection level and` |
|      - |   74 | ` * the attribute/method flag word the class structures actually store.` |
|      - |   75 | ` */` |
| 113200 |   76 | `static sxi32 NativeProtection(sxi32 iMods)` |
|      5 |   77 | `{` |
| 113205 |   78 | `	if( iMods & PH7_MOD_PRIVATE ){` |
|  13589 |   79 | `		return PH7_CLASS_PROT_PRIVATE;` |
|      - |   80 | `	}` |
|  99621 |   81 | `	if( iMods & PH7_MOD_PROTECTED ){` |
|    ! 0 |   82 | `		return PH7_CLASS_PROT_PROTECTED;` |
|      - |   83 | `	}` |
|  99621 |   84 | `	return PH7_CLASS_PROT_PUBLIC;` |
|  56605 |   85 | `}` |
|      - |   86 | `/*` |
|      - |   87 | ` * Attach one C-bodied method to an already-created class.` |
|      - |   88 | ` *` |
|      - |   89 | ` * The ph7_user_func built here is NOT registered in pVm->hHostFunction: it hangs` |
|      - |   90 | ` * off the method and is reachable only by dispatching the method, which is exactly` |
|      - |   91 | ` * the property that retires the global thunks. Its sName is the php-facing` |
|      - |   92 | `` * `Class::method`, so an ArgumentCountError raised at the OP_CALL choke point`` |
|      - |   93 | ` * names what a php user would recognise.` |
|      - |   94 | ` */` |
|  99616 |   95 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallMethod(` |
|      - |   96 | `	ph7_vm *pVm,` |
|      - |   97 | `	ph7_class *pClass,` |
|      - |   98 | `	const PH7_NativeMethodDef *pDef,` |
|      - |   99 | `	void *pUserData` |
|      - |  100 | `	)` |
|      5 |  101 | `{` |
|      - |  102 | `	ph7_class_method *pMeth;` |
|      - |  103 | `	ph7_user_func *pNative;` |
|      - |  104 | `	SyString sName;` |
|      - |  105 | `	SyString sVmName;` |
|      - |  106 | `	char zQual[128];` |
|      - |  107 | `	sxi32 iFuncFlags;` |
|      - |  108 | `	sxi32 rc;` |
|  99621 |  109 | `	SyStringInitFromBuf(&sName,pDef->zName,SyStrlen(pDef->zName));` |
|  99621 |  110 | `	iFuncFlags = VM_FUNC_NATIVE;` |
|  99621 |  111 | `	if( pVm->bCompilingBuiltin ){` |
|      - |  112 | `		/* Same stamp the embedded chunks got, so Reflection keeps reporting these` |
|      - |  113 | `		 * as internal: isInternal() true, getFileName() false. */` |
|  99621 |  114 | `		iFuncFlags \|= VM_FUNC_INTERNAL;` |
|  49808 |  115 | `	}` |
| 149429 |  116 | `	pMeth = PH7_NewClassMethod(&(*pVm),pClass,&sName,0,` |
|  99616 |  117 | `		NativeProtection(pDef->iMods),` |
|  99616 |  118 | `		(pDef->iMods & PH7_MOD_FINAL) ? PH7_CLASS_ATTR_FINAL : 0,` |
|  49808 |  119 | `		iFuncFlags);` |
|  99621 |  120 | `	if( pMeth == 0 ){` |
|    ! 0 |  121 | `		return SXERR_MEM;` |
|      - |  122 | `	}` |
|      - |  123 | `	/* Staticness has to be recorded in BOTH places: on the method (where the class` |
|      - |  124 | `	 * machinery and Reflection read it) and on the function (where the OP_CALL` |
|      - |  125 | `	 * dispatcher reads it, to keep the caller's $this from leaking in as a receiver` |
|      - |  126 | `	 * — see VM_FUNC_NATIVE_STATIC). */` |
|  99621 |  127 | `	if( pDef->iMods & PH7_MOD_STATIC ){` |
|  13589 |  128 | `		pMeth->iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|  13589 |  129 | `		pMeth->sFunc.iFlags \|= VM_FUNC_NATIVE_STATIC;` |
|   6792 |  130 | `	}` |
|      - |  131 | `	/* The C body. The diagnostic name is the qualified one php would print. */` |
|  99621 |  132 | `	SyBufferFormat(zQual,sizeof(zQual),"%z::%s",&pClass->sName,pDef->zName);` |
|  99621 |  133 | `	SyStringInitFromBuf(&sVmName,zQual,SyStrlen(zQual));` |
|  99621 |  134 | `	rc = PH7_NewForeignFunction(&(*pVm),&sVmName,pDef->xFunc,pUserData,&pNative);` |
|  99621 |  135 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  136 | `		return rc;` |
|      - |  137 | `	}` |
|      - |  138 | `	/* Arity bounds and by-ref positions come from the declared signature, exactly` |
|      - |  139 | `	 * as they do for a builtin (VmSetBuiltinSignatures) -- one string is the single` |
|      - |  140 | `	 * source of truth, and it doubles as the Reflection parameter list.` |
|      - |  141 | `	 *` |
|      - |  142 | `	 * NULL and "" mean different things here, and the difference is load-bearing.` |
|      - |  143 | `	 * A builtin without a row in aBuiltinSig[] is simply undescribed, so it stays` |
|      - |  144 | `	 * UNENFORCED (the SyZero'd bHasMaxArg == 0 reads as "unchecked", never as` |
|      - |  145 | `	 * "accepts at most zero"). A native method, by contrast, always states its` |
|      - |  146 | `	 * signature deliberately, so "" is a positive declaration of ZERO parameters` |
|      - |  147 | ``	 * and must enforce a maximum of zero -- otherwise `$gen->current(1)` and`` |
|      - |  148 | ``	 * `$fiber->isStarted(1)` are swallowed where php raises "expects exactly 0`` |
|      - |  149 | `	 * arguments, 1 given". VmDeriveArityFromSig("") already answers min 0 / max 0 /` |
|      - |  150 | `	 * enforced; only NULL opts out. */` |
|  99621 |  151 | `	if( pDef->zSig ){` |
|  99621 |  152 | `		sxi16 nMin = 0, nMax = 0;` |
|  99621 |  153 | `		sxu8 bAtLeast = 0, bHasMax = 0;` |
|  99621 |  154 | `		pNative->zSig = pDef->zSig;` |
|  99621 |  155 | `		pNative->nByRefMask = VmDeriveByRefMaskFromSig(pDef->zSig);` |
|  99621 |  156 | `		VmDeriveArityFromSig(pDef->zSig,&nMin,&bAtLeast,&nMax,&bHasMax);` |
|  99621 |  157 | `		pNative->nMinArg = nMin;` |
|  99621 |  158 | `		pNative->bAtLeast = bAtLeast;` |
|  99621 |  159 | `		pNative->nMaxArg = nMax;` |
|  99621 |  160 | `		pNative->bHasMaxArg = bHasMax;` |
|  49808 |  161 | `	}` |
|  99621 |  162 | `	if( pDef->zRet && pDef->zRet[0] ){` |
|  86037 |  163 | `		pNative->zRet = pDef->zRet;` |
|  43016 |  164 | `	}` |
|  99621 |  165 | `	pMeth->sFunc.pNative = pNative;` |
|  99621 |  166 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  99621 |  167 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  168 | `		return rc;` |
|      - |  169 | `	}` |
|  99621 |  170 | `	if( pClass->bMounted ){` |
|      - |  171 | `		/* The class is already live (a method attached after installation): mount` |
|      - |  172 | `		 * this one method now, since VmMountUserClass will not run again. */` |
|    ! 0 |  173 | `		return PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|      - |  174 | `	}` |
|  99621 |  175 | `	return SXRET_OK;` |
|  49813 |  176 | `}` |
|      - |  177 | `/*` |
|      - |  178 | ` * Install one class constant carrying a scalar value.` |
|      - |  179 | ` *` |
|      - |  180 | ` * A declared default is normally a compiled initializer (ph7_class_attr::aByteCode)` |
|      - |  181 | ` * evaluated at mount. The builder has no compiler to hand, so it evaluates the` |
|      - |  182 | ` * value NOW into the constant's reserved slot and leaves the byte-code empty --` |
|      - |  183 | ` * which is what a literal initializer would have produced anyway.` |
|      - |  184 | ` */` |
|    ! 0 |  185 | `static sxi32 NativeInstallConstant(ph7_vm *pVm,ph7_class *pClass,const PH7_NativeConstDef *pDef)` |
|    ! 0 |  186 | `{` |
|      - |  187 | `	ph7_class_attr *pAttr;` |
|      - |  188 | `	SyString sName;` |
|    ! 0 |  189 | `	SyStringInitFromBuf(&sName,pDef->zName,SyStrlen(pDef->zName));` |
|    ! 0 |  190 | `	pAttr = PH7_NewClassAttr(&(*pVm),&sName,0,NativeProtection(pDef->iMods),` |
|      - |  191 | `		PH7_CLASS_ATTR_CONSTANT);` |
|    ! 0 |  192 | `	if( pAttr == 0 ){` |
|    ! 0 |  193 | `		return SXERR_MEM;` |
|      - |  194 | `	}` |
|    ! 0 |  195 | `	pAttr->pDeclClass = pClass;` |
|      - |  196 | `	/* Stash the literal; VmMountUserClassAttrs materializes it into the slot. */` |
|    ! 0 |  197 | `	pAttr->pNativeValue = pDef;` |
|    ! 0 |  198 | `	return PH7_ClassInstallAttr(pClass,pAttr);` |
|    ! 0 |  199 | `}` |
|      - |  200 | `/*` |
|      - |  201 | ` * Install one declared property.` |
|      - |  202 | ` *` |
|      - |  203 | ` * The default is carried as a literal rather than compiled byte-code (see` |
|      - |  204 | `` * PH7_NativeLiteralValue); an instance property materializes it at `new`, a static`` |
|      - |  205 | `` * one at mount. A PH7_NATIVE_VAL_NULL default is the plain `public $p;` case.`` |
|      - |  206 | ` */` |
|  13584 |  207 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallProperty(ph7_vm *pVm,ph7_class *pClass,` |
|      - |  208 | `	const PH7_NativePropDef *pDef)` |
|      5 |  209 | `{` |
|      - |  210 | `	ph7_class_attr *pAttr;` |
|      - |  211 | `	SyString sName;` |
|  13589 |  212 | `	sxi32 iFlags = 0;` |
|  13589 |  213 | `	SyStringInitFromBuf(&sName,pDef->zName,SyStrlen(pDef->zName));` |
|  13589 |  214 | `	if( pDef->iMods & PH7_MOD_STATIC ){` |
|    ! 0 |  215 | `		iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|    ! 0 |  216 | `	}` |
|  13589 |  217 | `	pAttr = PH7_NewClassAttr(&(*pVm),&sName,0,NativeProtection(pDef->iMods),iFlags);` |
|  13589 |  218 | `	if( pAttr == 0 ){` |
|    ! 0 |  219 | `		return SXERR_MEM;` |
|      - |  220 | `	}` |
|  13589 |  221 | `	pAttr->pDeclClass = pClass;` |
|  13589 |  222 | `	pAttr->pNativeValue = &pDef->sDefault;` |
|  13589 |  223 | `	return PH7_ClassInstallAttr(pClass,pAttr);` |
|   6797 |  224 | `}` |
|      - |  225 | `/*` |
|      - |  226 | ` * Create and install ONE class from its spec, without its methods.` |
|      - |  227 | ` *` |
|      - |  228 | ` * Split from the method pass because a spec table may describe classes that extend` |
|      - |  229 | ` * each other, and PH7_ClassInherit needs the parent to exist first: callers list` |
|      - |  230 | ` * bases before subclasses, and PH7_InstallNativeClasses runs this pass over the` |
|      - |  231 | ` * whole table before touching any method.` |
|      - |  232 | ` */` |
|   9056 |  233 | `static sxi32 NativeDeclareClass(ph7_vm *pVm,const PH7_NativeClassSpec *pSpec,ph7_class **ppOut)` |
|      5 |  234 | `{` |
|      - |  235 | `	ph7_class *pClass;` |
|      - |  236 | `	SyString sName;` |
|      - |  237 | `	sxu32 n;` |
|      - |  238 | `	sxi32 rc;` |
|   9061 |  239 | `	SyStringInitFromBuf(&sName,pSpec->zName,SyStrlen(pSpec->zName));` |
|   9061 |  240 | `	pClass = PH7_NewRawClass(&(*pVm),&sName,0);` |
|   9061 |  241 | `	if( pClass == 0 ){` |
|    ! 0 |  242 | `		return SXERR_MEM;` |
|      - |  243 | `	}` |
|   9061 |  244 | `	pClass->iFlags \|= pSpec->iFlags;` |
|   9061 |  245 | `	for( n = 0 ; n < pSpec->nConst ; n++ ){` |
|    ! 0 |  246 | `		rc = NativeInstallConstant(&(*pVm),pClass,&pSpec->aConst[n]);` |
|    ! 0 |  247 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  248 | `			return rc;` |
|      - |  249 | `		}` |
|    ! 0 |  250 | `	}` |
|  22645 |  251 | `	for( n = 0 ; n < pSpec->nProp ; n++ ){` |
|  13589 |  252 | `		rc = PH7_NativeClassInstallProperty(&(*pVm),pClass,&pSpec->aProp[n]);` |
|  13589 |  253 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  254 | `			return rc;` |
|      - |  255 | `		}` |
|   6797 |  256 | `	}` |
|   9061 |  257 | `	rc = PH7_VmInstallClass(&(*pVm),pClass);` |
|   9061 |  258 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  259 | `		return rc;` |
|      - |  260 | `	}` |
|      - |  261 | `	/* Inheritance AFTER installation, mirroring the compiler's order in` |
|      - |  262 | `	 * GenStateCompileClassEx: the class must be findable while its own base chain` |
|      - |  263 | `	 * is wired, or a self-referential hierarchy cannot resolve. */` |
|   9061 |  264 | `	if( pSpec->zParent ){` |
|    ! 0 |  265 | `		ph7_class *pBase = NativeLookupClass(&(*pVm),pSpec->zParent);` |
|    ! 0 |  266 | `		if( pBase == 0 ){` |
|    ! 0 |  267 | `			return SXERR_NOTFOUND;` |
|      - |  268 | `		}` |
|    ! 0 |  269 | `		rc = (pClass->iFlags & PH7_CLASS_INTERFACE)` |
|    ! 0 |  270 | `			? PH7_ClassInterfaceInherit(pClass,pBase)` |
|    ! 0 |  271 | `			: PH7_ClassInherit(0,pClass,pBase);` |
|    ! 0 |  272 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  273 | `			return rc;` |
|      - |  274 | `		}` |
|    ! 0 |  275 | `	}` |
|   9061 |  276 | `	if( pSpec->zImplements ){` |
|      - |  277 | `		/* Comma-separated list, so one spec row can name several interfaces. */` |
|    ! 0 |  278 | `		const char *zCur = pSpec->zImplements;` |
|    ! 0 |  279 | `		while( zCur[0] != '\0' ){` |
|      - |  280 | `			const char *zStart;` |
|      - |  281 | `			char zIface[64];` |
|      - |  282 | `			sxu32 nLen;` |
|      - |  283 | `			ph7_class *pIface;` |
|    ! 0 |  284 | `			while( zCur[0] == ' ' \|\| zCur[0] == ',' ){` |
|    ! 0 |  285 | `				zCur++;` |
|    ! 0 |  286 | `			}` |
|    ! 0 |  287 | `			if( zCur[0] == '\0' ){` |
|    ! 0 |  288 | `				break;` |
|      - |  289 | `			}` |
|    ! 0 |  290 | `			zStart = zCur;` |
|    ! 0 |  291 | `			while( zCur[0] != '\0' && zCur[0] != ',' && zCur[0] != ' ' ){` |
|    ! 0 |  292 | `				zCur++;` |
|    ! 0 |  293 | `			}` |
|    ! 0 |  294 | `			nLen = (sxu32)(zCur - zStart);` |
|    ! 0 |  295 | `			if( nLen >= sizeof(zIface) ){` |
|    ! 0 |  296 | `				return SXERR_SYNTAX;` |
|      - |  297 | `			}` |
|    ! 0 |  298 | `			SyMemcpy(zStart,zIface,nLen);` |
|    ! 0 |  299 | `			zIface[nLen] = '\0';` |
|    ! 0 |  300 | `			pIface = NativeLookupClass(&(*pVm),zIface);` |
|    ! 0 |  301 | `			if( pIface == 0 ){` |
|    ! 0 |  302 | `				return SXERR_NOTFOUND;` |
|      - |  303 | `			}` |
|    ! 0 |  304 | `			rc = PH7_ClassImplement(pClass,pIface);` |
|    ! 0 |  305 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  306 | `				return rc;` |
|      - |  307 | `			}` |
|    ! 0 |  308 | `		}` |
|    ! 0 |  309 | `	}` |
|   9061 |  310 | `	*ppOut = pClass;` |
|   9061 |  311 | `	return SXRET_OK;` |
|   4533 |  312 | `}` |
|      - |  313 | `/*` |
|      - |  314 | ` * Install a whole table of native classes: declare them all (so later rows may` |
|      - |  315 | ` * extend earlier ones), then fill in their methods, then mount.` |
|      - |  316 | ` *` |
|      - |  317 | ` * Interfaces are declared but never mounted -- VmMountUserClass skips them anyway,` |
|      - |  318 | ` * their methods being non-invocable.` |
|      - |  319 | ` */` |
|   9056 |  320 | `PH7_PRIVATE sxi32 PH7_InstallNativeClasses(ph7_vm *pVm,const PH7_NativeClassSpec *aSpec,sxu32 nSpec)` |
|      5 |  321 | `{` |
|      - |  322 | `	ph7_class **apClass;` |
|      - |  323 | `	sxu32 i,j;` |
|      - |  324 | `	sxi32 rc;` |
|   9061 |  325 | `	apClass = (ph7_class **)SyMemBackendAlloc(&pVm->sAllocator,nSpec * sizeof(ph7_class *));` |
|   9061 |  326 | `	if( apClass == 0 ){` |
|    ! 0 |  327 | `		return SXERR_MEM;` |
|      - |  328 | `	}` |
|  18117 |  329 | `	for( i = 0 ; i < nSpec ; i++ ){` |
|   9061 |  330 | `		apClass[i] = 0;` |
|   9061 |  331 | `		rc = NativeDeclareClass(&(*pVm),&aSpec[i],&apClass[i]);` |
|   9061 |  332 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  333 | `			goto Done;` |
|      - |  334 | `		}` |
|   4533 |  335 | `	}` |
|  18117 |  336 | `	for( i = 0 ; i < nSpec ; i++ ){` |
|  95093 |  337 | `		for( j = 0 ; j < aSpec[i].nMethod ; j++ ){` |
|  86037 |  338 | `			rc = PH7_NativeClassInstallMethod(&(*pVm),apClass[i],&aSpec[i].aMethod[j],0);` |
|  86037 |  339 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  340 | `				goto Done;` |
|      - |  341 | `			}` |
|  43021 |  342 | `		}` |
|   4533 |  343 | `	}` |
|  18117 |  344 | `	for( i = 0 ; i < nSpec ; i++ ){` |
|   9061 |  345 | `		rc = VmMountUserClass(&(*pVm),apClass[i]);` |
|   9061 |  346 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  347 | `			goto Done;` |
|      - |  348 | `		}` |
|   4533 |  349 | `	}` |
|   9061 |  350 | `	rc = SXRET_OK;` |
|   4528 |  351 | `Done:` |
|   9061 |  352 | `	SyMemBackendFree(&pVm->sAllocator,apClass);` |
|   9061 |  353 | `	return rc;` |
|   4533 |  354 | `}` |
|      - |  355 |  |
