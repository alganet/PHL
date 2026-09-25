# src/ph7/oo_native.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 546/630 lines (86.67%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    4 | ` */` |
|       - |    5 | `#include "ph7int.h"` |
|       - |    6 | `/*` |
|       - |    7 | ` * Declaring a class from C.` |
|       - |    8 | ` *` |
|       - |    9 | ` * Every built-in class subsystem used to be an embedded PHP source string compiled` |
|       - |   10 | ` * at VM init, reaching the engine through a GLOBAL C thunk per operation` |
|       - |   11 | `` * (`__reflect_class_info()`, `__gen_next()`, `__dom_*`, ~110 of them). The reason`` |
|       - |   12 | ` * was structural: a ph7_class_method carries a ph7_vm_func, whose only body is` |
|       - |   13 | ` * bytecode, so C code could only ever be a global function.` |
|       - |   14 | ` *` |
|       - |   15 | ` * VM_FUNC_NATIVE removed that restriction (a method body may be a C routine), and` |
|       - |   16 | ` * this file is the front door to it: a declarative table describing a class —` |
|       - |   17 | ` * parent, interfaces, constants, methods — that PH7_InstallNativeClasses() turns` |
|       - |   18 | ` * into a real, mounted ph7_class. The thunks become what they always were,` |
|       - |   19 | ` * methods, and stop being visible in the global namespace.` |
|       - |   20 | ` *` |
|       - |   21 | ` * Nothing here is new machinery. It drives the same builders the COMPILER drives` |
|       - |   22 | `` * for `class Foo {}` — PH7_NewRawClass, PH7_NewClassMethod, PH7_ClassInstallMethod,`` |
|       - |   23 | ` * PH7_ClassInherit, PH7_ClassImplement, PH7_VmInstallClass, VmMountUserClass — so a` |
|       - |   24 | ` * native class is not a second kind of class: Reflection, instanceof, inheritance,` |
|       - |   25 | ` * visibility and autoloading all see an ordinary one.` |
|       - |   26 | ` */` |
|       - |   27 | `/*` |
|       - |   28 | ` * Materialize a native declaration's literal initializer into a value slot.` |
|       - |   29 | ` *` |
|       - |   30 | ` * A compiled declaration expresses its default as byte-code evaluated at mount` |
|       - |   31 | `` * (constants, statics) or at `new` (instance properties). The C builder has no`` |
|       - |   32 | ` * compiler to emit that, so it carries the literal on the attribute` |
|       - |   33 | ` * (ph7_class_attr::pNativeValue) and both of those sites call this instead --` |
|       - |   34 | ` * which is what a literal initializer's byte-code would have produced anyway.` |
|       - |   35 | ` */` |
| 9782234 |   36 | `PH7_PRIVATE void PH7_NativeLiteralValue(ph7_vm *pVm,const void *pLiteral,ph7_value *pOut)` |
|       5 |   37 | `{` |
| 9782239 |   38 | `	const PH7_NativeConstDef *pLit = (const PH7_NativeConstDef *)pLiteral;` |
|       - |   39 | `	/* Every PH7_MemObjInitFrom* below SyZeros the whole value, nIdx included — and` |
|       - |   40 | `	 * a CONSTANT's or STATIC's slot is addressed by that index afterwards` |
|       - |   41 | ``	 * (`pAttr->nIdx = pMemObj->nIdx`). Losing it pointed every native class`` |
|       - |   42 | `	 * constant at slot 0, which reads as $GLOBALS. The instance-property path was` |
|       - |   43 | `	 * unaffected (it records the index before calling this), which is why nothing` |
|       - |   44 | `	 * saw it until the date family declared the first native constants. */` |
| 9782239 |   45 | `	sxu32 nSlot = pOut->nIdx;` |
| 9782239 |   46 | `	switch( pLit->iType ){` |
| 1232026 |   47 | `		case PH7_NATIVE_VAL_INT:` |
| 2464057 |   48 | `			PH7_MemObjInitFromInt(&(*pVm),pOut,pLit->iValue);` |
| 2464057 |   49 | `			break;` |
|     605 |   50 | `		case PH7_NATIVE_VAL_BOOL:` |
|    1214 |   51 | `			PH7_MemObjInitFromBool(&(*pVm),pOut,(sxi32)pLit->iValue);` |
|    1214 |   52 | `			break;` |
| 2185542 |   53 | `		case PH7_NATIVE_VAL_STRING: {` |
|       - |   54 | `			SyString sLit;` |
| 4371089 |   55 | `			SyStringInitFromBuf(&sLit,pLit->zValue,SyStrlen(pLit->zValue));` |
| 4371089 |   56 | `			PH7_MemObjInitFromString(&(*pVm),pOut,&sLit);` |
| 4371089 |   57 | `			break;` |
|       - |   58 | `		}` |
|       - |   59 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      48 |   60 | `		case PH7_NATIVE_VAL_DOUBLE:` |
|      97 |   61 | `			PH7_MemObjInitFromReal(&(*pVm),pOut,pLit->rValue);` |
|      97 |   62 | `			break;` |
|       - |   63 | `#endif` |
|  728015 |   64 | `		case PH7_NATIVE_VAL_ARRAY: {` |
|       - |   65 | ``			/* The empty array — php's `private array $trace = [];`. Each instance`` |
|       - |   66 | `			 * needs its OWN map (the exception's trace is written per throw), so` |
|       - |   67 | `			 * this allocates rather than sharing one. */` |
| 1456035 |   68 | `			ph7_hashmap *pMap = PH7_NewHashmap(&(*pVm),0,0);` |
| 1456035 |   69 | `			if( pMap == 0 ){` |
|     ! 0 |   70 | `				PH7_MemObjInit(&(*pVm),pOut);` |
|     ! 0 |   71 | `			}else{` |
| 1456035 |   72 | `				PH7_MemObjInitFromArray(&(*pVm),pOut,pMap);` |
|       - |   73 | `			}` |
| 1456035 |   74 | `			break;` |
|       - |   75 | `		}` |
|  744881 |   76 | `		default:` |
| 1489767 |   77 | `			PH7_MemObjInit(&(*pVm),pOut);` |
| 1489762 |   78 | `			break;` |
|       - |   79 | `	}` |
| 9782239 |   80 | `	pOut->nIdx = nSlot;` |
| 9782239 |   81 | `}` |
|       - |   82 | `/*` |
|       - |   83 | ` * Write a declared property of an instance from C.` |
|       - |   84 | ` *` |
|       - |   85 | ` * Every native class that hands an OBJECT back to PHP has to fill one in, and the` |
|       - |   86 | ` * write has to go through the instance's own slot table (hAttr -> VmClassAttr ->` |
|       - |   87 | ` * pVm->aMemObj) rather than the class declaration, or the value lands nowhere.` |
|       - |   88 | ` * Silently does nothing for a name the class does not declare -- callers pass` |
|       - |   89 | ` * literals from their own spec table, so a miss is a build error, not input.` |
|       - |   90 | ` */` |
|     806 |   91 | `PH7_PRIVATE void PH7_NativeSetProp(ph7_vm *pVm,ph7_class_instance *pObj,` |
|       - |   92 | `	const char *zProp,sxu32 nProp,ph7_value *pSrcVal)` |
|       5 |   93 | `{` |
|     811 |   94 | `	SyHashEntry *pEntry = SyHashGet(&pObj->hAttr,(const void *)zProp,nProp);` |
|       - |   95 | `	VmClassAttr *pVmAttr;` |
|       - |   96 | `	ph7_value *pSlot;` |
|     811 |   97 | `	if( pEntry == 0 ){` |
|     ! 0 |   98 | `		return;` |
|       - |   99 | `	}` |
|     811 |  100 | `	pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     811 |  101 | `	pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,pVmAttr->nIdx);` |
|     811 |  102 | `	if( pSlot == 0 ){` |
|     ! 0 |  103 | `		return;` |
|       - |  104 | `	}` |
|     811 |  105 | `	PH7_MemObjStore(pSrcVal,pSlot);` |
|     811 |  106 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|     408 |  107 | `}` |
|       - |  108 | `/*` |
|       - |  109 | ` * ---------------------------------------------------------------------------` |
|       - |  110 | ` * Reading and writing a native instance's own declared slots.` |
|       - |  111 | ` *` |
|       - |  112 | `` * A compiled method reaches `$this->p` through the byte-code that resolves the`` |
|       - |  113 | ` * attribute; a C body has to walk the instance's slot table itself. Every native` |
|       - |  114 | ` * class needs the same six or seven moves, so they live here rather than being` |
|       - |  115 | ` * re-declared per subsystem (the date family carried a private copy of the whole` |
|       - |  116 | ` * set, which is what these replace).` |
|       - |  117 | ` * ---------------------------------------------------------------------------` |
|       - |  118 | ` */` |
|       - |  119 | `/* Fetch a declared INSTANCE slot by name (never a static or a constant). */` |
| 1535278 |  120 | `PH7_PRIVATE ph7_value * PH7_NativeAttr(ph7_class_instance *pObj,const char *zName)` |
|       5 |  121 | `{` |
|       - |  122 | `	SyString sName;` |
| 1535283 |  123 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
| 1535283 |  124 | `	return PH7_ClassInstanceFetchAttr(pObj,&sName);` |
|       5 |  125 | `}` |
|       - |  126 | `/*` |
|       - |  127 | ` * Has this declared slot never been written? A PH7_NATIVE_VAL_NONE property is` |
|       - |  128 | `` * php's `public int $id;` — typed, with no default — and reading one before the`` |
|       - |  129 | ` * class has filled it is php's "must not be accessed before initialization".` |
|       - |  130 | ` * The property-read opcode raises that itself; a C body reading the slot` |
|       - |  131 | ` * directly has to ask.` |
|       - |  132 | ` */` |
|      40 |  133 | `PH7_PRIVATE int PH7_NativeAttrIsUninit(ph7_class_instance *pObj,const char *zName)` |
|       1 |  134 | `{` |
|      41 |  135 | `	SyHashEntry *pEntry = pObj ? SyHashGet(&pObj->hAttr,(const void *)zName,SyStrlen(zName)) : 0;` |
|      41 |  136 | `	return pEntry != 0` |
|      40 |  137 | `		&& (((VmClassAttr *)pEntry->pUserData)->iState & VM_CLASS_ATTR_UNINIT) != 0;` |
|       1 |  138 | `}` |
|       - |  139 | `/* Read an int slot WITHOUT converting it: ph7_value_to_int64() converts the` |
|       - |  140 | ` * attribute in place, which would rewrite the object's own state. */` |
|   12850 |  141 | `PH7_PRIVATE sxi64 PH7_NativeAttrInt(ph7_class_instance *pObj,const char *zName)` |
|       4 |  142 | `{` |
|   12854 |  143 | `	ph7_value *pVal = PH7_NativeAttr(pObj,zName);` |
|   12854 |  144 | `	if( pVal && (pVal->iFlags & MEMOBJ_INT) ){` |
|   12854 |  145 | `		return pVal->x.iVal;` |
|       - |  146 | `	}` |
|     ! 0 |  147 | `	return 0;` |
|    6430 |  148 | `}` |
|       - |  149 | `/* Borrow a string slot's bytes (empty when it holds anything else). */` |
|    7740 |  150 | `PH7_PRIVATE void PH7_NativeAttrStr(ph7_class_instance *pObj,const char *zName,` |
|       - |  151 | `	const char **pzOut,int *pnOut)` |
|       5 |  152 | `{` |
|    7745 |  153 | `	ph7_value *pVal = PH7_NativeAttr(pObj,zName);` |
|    7745 |  154 | `	*pzOut = "";` |
|    7745 |  155 | `	*pnOut = 0;` |
|    7745 |  156 | `	if( pVal && (pVal->iFlags & MEMOBJ_STRING) ){` |
|    7735 |  157 | `		*pzOut = (const char *)SyBlobData(&pVal->sBlob);` |
|    7735 |  158 | `		*pnOut = (int)SyBlobLength(&pVal->sBlob);` |
|    3865 |  159 | `	}` |
|    7745 |  160 | `}` |
|       - |  161 | `/* The object stored in a slot, or NULL when it holds anything else. */` |
|    9308 |  162 | `PH7_PRIVATE ph7_class_instance * PH7_NativeAttrObj(ph7_class_instance *pObj,const char *zName)` |
|       5 |  163 | `{` |
|    9313 |  164 | `	ph7_value *pVal = PH7_NativeAttr(pObj,zName);` |
|    9313 |  165 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    2583 |  166 | `		return 0;` |
|       - |  167 | `	}` |
|    6734 |  168 | `	return (ph7_class_instance *)pVal->x.pOther;` |
|    4659 |  169 | `}` |
|       - |  170 | `/* Truth of a bool/int slot, again without converting it. */` |
|    1252 |  171 | `PH7_PRIVATE int PH7_NativeAttrTruthy(ph7_class_instance *pObj,const char *zName)` |
|       3 |  172 | `{` |
|    1255 |  173 | `	ph7_value *pVal = PH7_NativeAttr(pObj,zName);` |
|    1255 |  174 | `	if( pVal && (pVal->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|    1255 |  175 | `		return pVal->x.iVal != 0;` |
|       - |  176 | `	}` |
|     ! 0 |  177 | `	return 0;` |
|     629 |  178 | `}` |
|       - |  179 | `/*` |
|       - |  180 | ` * Clear the not-yet-initialized mark a TYPED slot without a default carries.` |
|       - |  181 | ` *` |
|       - |  182 | ` * There are two families of writer here: PH7_NativeSetProp, which looks the` |
|       - |  183 | ` * VmClassAttr up and clears the bit, and the four typed shortcuts below, which` |
|       - |  184 | ` * write the ph7_value through PH7_NativeAttr and used to leave it set. That was` |
|       - |  185 | ` * invisible while nothing native declared a default-less typed property; the` |
|       - |  186 | `` * moment `public string $name` arrived on the reflectors, every one of them`` |
|       - |  187 | ` * threw "must not be accessed before initialization" from a constructor that HAD` |
|       - |  188 | ` * written the slot. Rule 44's family: the bookkeeping has to live with the write,` |
|       - |  189 | ` * not with one of the two ways of writing.` |
|       - |  190 | ` */` |
| 1473525 |  191 | `static void NativeAttrMarkInit(ph7_class_instance *pObj,const char *zName)` |
|       5 |  192 | `{` |
| 1473530 |  193 | `	SyHashEntry *pEntry = SyHashGet(&pObj->hAttr,(const void *)zName,SyStrlen(zName));` |
| 1473530 |  194 | `	if( pEntry ){` |
| 1473530 |  195 | `		((VmClassAttr *)pEntry->pUserData)->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|  736765 |  196 | `	}` |
| 1473530 |  197 | `}` |
|    7591 |  198 | `PH7_PRIVATE void PH7_NativeSetAttrInt(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,sxi64 iVal)` |
|       4 |  199 | `{` |
|    7595 |  200 | `	ph7_value *pSlot = PH7_NativeAttr(pObj,zName);` |
|       - |  201 | `	ph7_value sVal;` |
|    7595 |  202 | `	if( pSlot == 0 ){` |
|     ! 0 |  203 | `		return;` |
|       - |  204 | `	}` |
|    7595 |  205 | `	PH7_MemObjInitFromInt(&(*pVm),&sVal,iVal);` |
|    7595 |  206 | `	PH7_MemObjStore(&sVal,pSlot);` |
|    7595 |  207 | `	PH7_MemObjRelease(&sVal);` |
|    7595 |  208 | `	NativeAttrMarkInit(pObj,zName);` |
|    3800 |  209 | `}` |
| 1463022 |  210 | `PH7_PRIVATE void PH7_NativeSetAttrStr(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,` |
|       - |  211 | `	const char *zVal,int nVal)` |
|       5 |  212 | `{` |
| 1463027 |  213 | `	ph7_value *pSlot = PH7_NativeAttr(pObj,zName);` |
|       - |  214 | `	ph7_value sVal;` |
|       - |  215 | `	SyString sStr;` |
| 1463027 |  216 | `	if( pSlot == 0 ){` |
|     ! 0 |  217 | `		return;` |
|       - |  218 | `	}` |
| 1463027 |  219 | `	SyStringInitFromBuf(&sStr,zVal,nVal);` |
| 1463027 |  220 | `	PH7_MemObjInitFromString(&(*pVm),&sVal,&sStr);` |
| 1463027 |  221 | `	PH7_MemObjStore(&sVal,pSlot);` |
| 1463027 |  222 | `	PH7_MemObjRelease(&sVal);	NativeAttrMarkInit(pObj,zName);` |
|  731518 |  223 | `}` |
|    1246 |  224 | `PH7_PRIVATE void PH7_NativeSetAttrBool(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,int bVal)` |
|       4 |  225 | `{` |
|    1250 |  226 | `	ph7_value *pSlot = PH7_NativeAttr(pObj,zName);` |
|       - |  227 | `	ph7_value sVal;` |
|    1250 |  228 | `	if( pSlot == 0 ){` |
|     ! 0 |  229 | `		return;` |
|       - |  230 | `	}` |
|    1250 |  231 | `	PH7_MemObjInitFromBool(&(*pVm),&sVal,bVal);` |
|    1250 |  232 | `	PH7_MemObjStore(&sVal,pSlot);` |
|    1250 |  233 | `	PH7_MemObjRelease(&sVal);	NativeAttrMarkInit(pObj,zName);` |
|     627 |  234 | `}` |
|       - |  235 | `/* Store an object in a slot, or NULL to clear it. PH7_MemObjStore takes the` |
|       - |  236 | ` * reference the slot needs, so the temp never holds one of its own. */` |
|    1666 |  237 | `PH7_PRIVATE void PH7_NativeSetAttrObj(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,` |
|       - |  238 | `	ph7_class_instance *pVal)` |
|       4 |  239 | `{` |
|    1670 |  240 | `	ph7_value *pSlot = PH7_NativeAttr(pObj,zName);` |
|       - |  241 | `	ph7_value sVal;` |
|    1670 |  242 | `	if( pSlot == 0 ){` |
|     ! 0 |  243 | `		return;` |
|       - |  244 | `	}` |
|    1670 |  245 | `	PH7_MemObjInit(&(*pVm),&sVal);` |
|    1670 |  246 | `	if( pVal ){` |
|    1434 |  247 | `		sVal.x.pOther = pVal;` |
|    1434 |  248 | `		sVal.iFlags = MEMOBJ_OBJ;` |
|     715 |  249 | `	}` |
|    1670 |  250 | `	PH7_MemObjStore(&sVal,pSlot);` |
|    1670 |  251 | `	NativeAttrMarkInit(pObj,zName);` |
|     837 |  252 | `}` |
|       - |  253 | `/*` |
|       - |  254 | ` * Hand an instance back as a native call's result, dropping the reference` |
|       - |  255 | ` * PH7_NewClassInstance/PH7_CloneClassInstance handed the caller.` |
|       - |  256 | ` */` |
|     902 |  257 | `PH7_PRIVATE void PH7_NativeResultObject(ph7_context *pCtx,ph7_class_instance *pObj)` |
|       5 |  258 | `{` |
|       - |  259 | `	ph7_value sRes;` |
|     907 |  260 | `	PH7_MemObjInit(pCtx->pVm,&sRes);` |
|     907 |  261 | `	sRes.x.pOther = pObj;` |
|     907 |  262 | `	sRes.iFlags = MEMOBJ_OBJ;` |
|     907 |  263 | `	ph7_result_value(pCtx,&sRes);   /* takes its own reference */` |
|     907 |  264 | `	PH7_ClassInstanceUnref(pObj);` |
|     907 |  265 | `}` |
|       - |  266 | `/*` |
|       - |  267 | ` * Resolve a class by name for the builder's own use (parents and interfaces).` |
|       - |  268 | ` * Autoload is deliberately NOT triggered: these run at VM init, where the only` |
|       - |  269 | ` * classes that can exist are the ones installed before this call, and a missing` |
|       - |  270 | ` * name is a build-order bug in the engine, not a userland lookup.` |
|       - |  271 | ` */` |
|  668980 |  272 | `static ph7_class * NativeLookupClass(ph7_vm *pVm,const char *zName)` |
|       5 |  273 | `{` |
|  668985 |  274 | `	return PH7_VmExtractClass(&(*pVm),zName,(sxu32)SyStrlen(zName),FALSE,0);` |
|       5 |  275 | `}` |
|       - |  276 | `/*` |
|       - |  277 | ` * Translate the builder's PH7_MOD_* modifier bits into the protection level and` |
|       - |  278 | ` * the attribute/method flag word the class structures actually store.` |
|       - |  279 | ` */` |
| 5516712 |  280 | `static sxi32 NativeProtection(sxi32 iMods)` |
|       5 |  281 | `{` |
| 5516717 |  282 | `	if( iMods & PH7_MOD_PRIVATE ){` |
|  483729 |  283 | `		return PH7_CLASS_PROT_PRIVATE;` |
|       - |  284 | `	}` |
| 5032993 |  285 | `	if( iMods & PH7_MOD_PROTECTED ){` |
|  154385 |  286 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - |  287 | `	}` |
| 4878613 |  288 | `	return PH7_CLASS_PROT_PUBLIC;` |
| 2758361 |  289 | `}` |
|       - |  290 | `/*` |
|       - |  291 | ` * Attach one C-bodied method to an already-created class.` |
|       - |  292 | ` *` |
|       - |  293 | ` * The ph7_user_func built here is NOT registered in pVm->hHostFunction: it hangs` |
|       - |  294 | ` * off the method and is reachable only by dispatching the method, which is exactly` |
|       - |  295 | ` * the property that retires the global thunks. Its sName is the php-facing` |
|       - |  296 | `` * `Class::method`, so an ArgumentCountError raised at the OP_CALL choke point`` |
|       - |  297 | ` * names what a php user would recognise.` |
|       - |  298 | ` */` |
| 4153022 |  299 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallMethod(` |
|       - |  300 | `	ph7_vm *pVm,` |
|       - |  301 | `	ph7_class *pClass,` |
|       - |  302 | `	const PH7_NativeMethodDef *pDef,` |
|       - |  303 | `	void *pUserData` |
|       - |  304 | `	)` |
|       5 |  305 | `{` |
|       - |  306 | `	ph7_class_method *pMeth;` |
|       - |  307 | `	ph7_user_func *pNative;` |
|       - |  308 | `	SyString sName;` |
|       - |  309 | `	SyString sVmName;` |
|       - |  310 | `	char zQual[128];` |
|       - |  311 | `	sxi32 iFuncFlags;` |
|       - |  312 | `	sxi32 rc;` |
| 4153027 |  313 | `	SyStringInitFromBuf(&sName,pDef->zName,SyStrlen(pDef->zName));` |
| 4153027 |  314 | `	iFuncFlags = VM_FUNC_NATIVE;` |
| 4153027 |  315 | `	if( pVm->bCompilingBuiltin ){` |
|       - |  316 | `		/* Same stamp the embedded chunks got, so Reflection keeps reporting these` |
|       - |  317 | `		 * as internal: isInternal() true, getFileName() false. */` |
| 4152827 |  318 | `		iFuncFlags \|= VM_FUNC_INTERNAL;` |
| 2076411 |  319 | `	}` |
| 6229538 |  320 | `	pMeth = PH7_NewClassMethod(&(*pVm),pClass,&sName,0,` |
| 4153022 |  321 | `		NativeProtection(pDef->iMods),` |
| 4153022 |  322 | `		((pDef->iMods & PH7_MOD_FINAL) ? PH7_CLASS_ATTR_FINAL : 0)` |
| 4153022 |  323 | `		\| ((pDef->iMods & PH7_MOD_ABSTRACT) ? PH7_CLASS_ATTR_ABSTRACT : 0),` |
| 2076511 |  324 | `		iFuncFlags);` |
| 4153027 |  325 | `	if( pMeth == 0 ){` |
|     ! 0 |  326 | `		return SXERR_MEM;` |
|       - |  327 | `	}` |
|       - |  328 | `	/* Staticness has to be recorded in BOTH places: on the method (where the class` |
|       - |  329 | `	 * machinery and Reflection read it) and on the function (where the OP_CALL` |
|       - |  330 | `	 * dispatcher reads it, to keep the caller's $this from leaking in as a receiver` |
|       - |  331 | `	 * — see VM_FUNC_NATIVE_STATIC). */` |
| 4153027 |  332 | `	if( pDef->iMods & PH7_MOD_STATIC ){` |
|  144293 |  333 | `		pMeth->iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|  144293 |  334 | `		pMeth->sFunc.iFlags \|= VM_FUNC_NATIVE_STATIC;` |
|   72144 |  335 | `	}` |
|       - |  336 | `	/* An ABSTRACT method has no body to install — an INTERFACE's methods are the` |
|       - |  337 | ``	 * reason the builder needs the case at all (`SeekableIterator::seek()`), and`` |
|       - |  338 | `	 * php reports them with their declared signature like any other. The dispatch` |
|       - |  339 | `	 * never reaches one: OP_CALL and OP_MEMBER both refuse an abstract method` |
|       - |  340 | `	 * ("Cannot call abstract method C::m()") before they look at a body. */` |
| 4153027 |  341 | `	if( pDef->iMods & PH7_MOD_ABSTRACT ){` |
|  226429 |  342 | `		if( pDef->zSig ){` |
|  226429 |  343 | `			rc = PH7_NewForeignFunction(&(*pVm),&sName,pDef->xFunc,pUserData,&pNative);` |
|  226429 |  344 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  345 | `				return rc;` |
|       - |  346 | `			}` |
|  226429 |  347 | `			pNative->zSig = pDef->zSig;` |
|  226429 |  348 | `			if( pDef->zRet && pDef->zRet[0] ){` |
|  144093 |  349 | `				pNative->zRet = pDef->zRet;` |
|   72044 |  350 | `			}` |
|  226429 |  351 | `			pMeth->sFunc.pNative = pNative;` |
|  113212 |  352 | `		}` |
|  226429 |  353 | `		return PH7_ClassInstallMethod(pClass,pMeth);` |
|       - |  354 | `	}` |
|       - |  355 | `	/* The C body. The diagnostic name is the qualified one php would print. */` |
| 3926603 |  356 | `	SyBufferFormat(zQual,sizeof(zQual),"%z::%s",&pClass->sName,pDef->zName);` |
| 3926603 |  357 | `	SyStringInitFromBuf(&sVmName,zQual,SyStrlen(zQual));` |
| 3926603 |  358 | `	rc = PH7_NewForeignFunction(&(*pVm),&sVmName,pDef->xFunc,pUserData,&pNative);` |
| 3926603 |  359 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  360 | `		return rc;` |
|       - |  361 | `	}` |
|       - |  362 | `	/* Arity bounds and by-ref positions come from the declared signature, exactly` |
|       - |  363 | `	 * as they do for a builtin (VmSetBuiltinSignatures) -- one string is the single` |
|       - |  364 | `	 * source of truth, and it doubles as the Reflection parameter list.` |
|       - |  365 | `	 *` |
|       - |  366 | `	 * NULL and "" mean different things here, and the difference is load-bearing.` |
|       - |  367 | `	 * A builtin without a row in aBuiltinSig[] is simply undescribed, so it stays` |
|       - |  368 | `	 * UNENFORCED (the SyZero'd bHasMaxArg == 0 reads as "unchecked", never as` |
|       - |  369 | `	 * "accepts at most zero"). A native method, by contrast, always states its` |
|       - |  370 | `	 * signature deliberately, so "" is a positive declaration of ZERO parameters` |
|       - |  371 | ``	 * and must enforce a maximum of zero -- otherwise `$gen->current(1)` and`` |
|       - |  372 | ``	 * `$fiber->isStarted(1)` are swallowed where php raises "expects exactly 0`` |
|       - |  373 | `	 * arguments, 1 given". VmDeriveArityFromSig("") already answers min 0 / max 0 /` |
|       - |  374 | `	 * enforced; only NULL opts out. */` |
| 3926603 |  375 | `	if( pDef->zSig ){` |
| 3921457 |  376 | `		sxi16 nMin = 0, nMax = 0;` |
| 3921457 |  377 | `		sxu8 bAtLeast = 0, bHasMax = 0;` |
| 3921457 |  378 | `		pNative->zSig = pDef->zSig;` |
| 3921457 |  379 | `		pNative->nByRefMask = VmDeriveByRefMaskFromSig(pDef->zSig);` |
| 3921457 |  380 | `		VmDeriveArityFromSig(pDef->zSig,&nMin,&bAtLeast,&nMax,&bHasMax);` |
| 3921457 |  381 | `		pNative->nMinArg = nMin;` |
| 3921457 |  382 | `		pNative->bAtLeast = bAtLeast;` |
| 3921457 |  383 | `		pNative->nMaxArg = nMax;` |
| 3921457 |  384 | `		pNative->bHasMaxArg = bHasMax;` |
| 1960726 |  385 | `	}` |
| 3926603 |  386 | `	if( pDef->zRet && pDef->zRet[0] ){` |
| 3499485 |  387 | `		pNative->zRet = pDef->zRet;` |
| 1749740 |  388 | `	}` |
| 3926603 |  389 | `	pMeth->sFunc.pNative = pNative;` |
| 3926603 |  390 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
| 3926603 |  391 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  392 | `		return rc;` |
|       - |  393 | `	}` |
| 3926603 |  394 | `	if( pClass->bMounted ){` |
|       - |  395 | `		/* The class is already live (a method attached after installation): mount` |
|       - |  396 | `		 * this one method now, since VmMountUserClass will not run again. */` |
|     ! 0 |  397 | `		return PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|       - |  398 | `	}` |
| 3926603 |  399 | `	return SXRET_OK;` |
| 2076516 |  400 | `}` |
|       - |  401 | `/*` |
|       - |  402 | ` * Install one class constant carrying a scalar value.` |
|       - |  403 | ` *` |
|       - |  404 | ` * A declared default is normally a compiled initializer (ph7_class_attr::aByteCode)` |
|       - |  405 | ` * evaluated at mount. The builder has no compiler to hand, so it evaluates the` |
|       - |  406 | ` * value NOW into the constant's reserved slot and leaves the byte-code empty --` |
|       - |  407 | ` * which is what a literal initializer would have produced anyway.` |
|       - |  408 | ` */` |
|  452848 |  409 | `static sxi32 NativeInstallConstant(ph7_vm *pVm,ph7_class *pClass,const PH7_NativeConstDef *pDef)` |
|       5 |  410 | `{` |
|       - |  411 | `	ph7_class_attr *pAttr;` |
|       - |  412 | `	SyString sName;` |
|  452853 |  413 | `	SyStringInitFromBuf(&sName,pDef->zName,SyStrlen(pDef->zName));` |
|  452853 |  414 | `	pAttr = PH7_NewClassAttr(&(*pVm),&sName,0,NativeProtection(pDef->iMods),` |
|       - |  415 | `		PH7_CLASS_ATTR_CONSTANT);` |
|  452853 |  416 | `	if( pAttr == 0 ){` |
|     ! 0 |  417 | `		return SXERR_MEM;` |
|       - |  418 | `	}` |
|  452853 |  419 | `	pAttr->pDeclClass = pClass;` |
|       - |  420 | `	/* Stash the literal; VmMountUserClassAttrs materializes it into the slot. */` |
|  452853 |  421 | `	pAttr->pNativeValue = pDef;` |
|  452853 |  422 | `	return PH7_ClassInstallAttr(pClass,pAttr);` |
|  226429 |  423 | `}` |
|       - |  424 | `/*` |
|       - |  425 | ` * Fill in a declared property TYPE from the text a spec row states, exactly as` |
|       - |  426 | ` * GenStateCopyTypeToAttr fills it from a parsed declaration: nType is the` |
|       - |  427 | ` * MEMOBJ_* the atom names (SXU32_HIGH plus sClass for a class name, which is` |
|       - |  428 | `` * also how the compiler carries `mixed` and `iterable`), the `?` becomes the`` |
|       - |  429 | ` * NULLABLE flag rather than a type bit, and sTypeName keeps the text VERBATIM --` |
|       - |  430 | ` * both the TypeError and Reflection print what the declaration said.` |
|       - |  431 | ` *` |
|       - |  432 | ` * Single atoms only. A union needs the alternative SET the compiler builds, and` |
|       - |  433 | ` * nothing native declares one; a spec that tries reads as the class name it is` |
|       - |  434 | ` * spelled with, which is why the parse stays this literal.` |
|       - |  435 | ` */` |
|  272738 |  436 | `static void NativeAttrType(ph7_class_attr *pAttr,const char *zType)` |
|       5 |  437 | `{` |
|       - |  438 | `	static const struct { const char *zName; sxu32 nType; } aScalar[] = {` |
|       - |  439 | `		{ "int",    MEMOBJ_INT },` |
|       - |  440 | `		{ "float",  MEMOBJ_REAL },` |
|       - |  441 | `		{ "string", MEMOBJ_STRING },` |
|       - |  442 | `		{ "bool",   MEMOBJ_BOOL },` |
|       - |  443 | `		{ "array",  MEMOBJ_HASHMAP },` |
|       - |  444 | `		{ "object", MEMOBJ_OBJ },` |
|       - |  445 | `	};` |
|  272743 |  446 | `	const char *zAtom = zType;` |
|       - |  447 | `	sxu32 nAtom, n;` |
|  272743 |  448 | `	if( zAtom[0] == '?' ){` |
|   46319 |  449 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|   46319 |  450 | `		zAtom++;` |
|   23157 |  451 | `	}` |
|  272743 |  452 | `	nAtom = SyStrlen(zAtom);` |
|  272743 |  453 | `	pAttr->iFlags \|= PH7_CLASS_ATTR_TYPED;` |
|  272743 |  454 | `	SyStringInitFromBuf(&pAttr->sTypeName,zType,SyStrlen(zType));` |
|  879971 |  455 | `	for( n = 0 ; n < SX_ARRAYSIZE(aScalar) ; ++n ){` |
|  838798 |  456 | `		if( nAtom == SyStrlen(aScalar[n].zName)` |
|  545481 |  457 | `		 && SyMemcmp(zAtom,aScalar[n].zName,(sxu32)nAtom) == 0 ){` |
|  231575 |  458 | `			pAttr->nType = aScalar[n].nType;` |
|  231575 |  459 | `			return;` |
|       - |  460 | `		}` |
|  303619 |  461 | `	}` |
|   41173 |  462 | `	pAttr->nType = SXU32_HIGH;` |
|   41173 |  463 | `	SyStringInitFromBuf(&pAttr->sClass,zAtom,nAtom);` |
|  136374 |  464 | `}` |
|       - |  465 | `/*` |
|       - |  466 | ` * Install one declared property.` |
|       - |  467 | ` *` |
|       - |  468 | ` * The default is carried as a literal rather than compiled byte-code (see` |
|       - |  469 | `` * PH7_NativeLiteralValue); an instance property materializes it at `new`, a static`` |
|       - |  470 | `` * one at mount. A PH7_NATIVE_VAL_NULL default is the plain `public $p;` case.`` |
|       - |  471 | ` */` |
|  910842 |  472 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallProperty(ph7_vm *pVm,ph7_class *pClass,` |
|       - |  473 | `	const PH7_NativePropDef *pDef)` |
|       5 |  474 | `{` |
|       - |  475 | `	ph7_class_attr *pAttr;` |
|       - |  476 | `	SyString sName;` |
|  910847 |  477 | `	sxi32 iFlags = 0;` |
|  910847 |  478 | `	SyStringInitFromBuf(&sName,pDef->zName,SyStrlen(pDef->zName));` |
|  910847 |  479 | `	if( pDef->iMods & PH7_MOD_STATIC ){` |
|     ! 0 |  480 | `		iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  481 | `	}` |
|  910847 |  482 | `	if( pDef->iMods & PH7_MOD_HIDDEN ){` |
|  550627 |  483 | `		iFlags \|= PH7_CLASS_ATTR_HIDDEN;` |
|  275311 |  484 | `	}` |
|       - |  485 | `	/* php declares several native slots readonly and asymmetrically visible` |
|       - |  486 | ``	 * (`public protected(set) readonly string $path` on Directory), and both are`` |
|       - |  487 | `	 * php-visible twice over: the write refusal and Reflection's modifier list. */` |
|  910847 |  488 | `	if( pDef->iMods & PH7_MOD_READONLY ){` |
|   20589 |  489 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|   10292 |  490 | `	}` |
|  910847 |  491 | `	if( pDef->iMods & PH7_MOD_PROT_SET ){` |
|   20589 |  492 | `		iFlags \|= PH7_CLASS_ATTR_PROTECTED_SET;` |
|   10292 |  493 | `	}` |
|  910847 |  494 | `	if( pDef->iMods & PH7_MOD_PRIV_SET ){` |
|     ! 0 |  495 | `		iFlags \|= PH7_CLASS_ATTR_PRIVATE_SET;` |
|     ! 0 |  496 | `	}` |
|  910847 |  497 | `	pAttr = PH7_NewClassAttr(&(*pVm),&sName,0,NativeProtection(pDef->iMods),iFlags);` |
|  910847 |  498 | `	if( pAttr == 0 ){` |
|     ! 0 |  499 | `		return SXERR_MEM;` |
|       - |  500 | `	}` |
|  910847 |  501 | `	pAttr->pDeclClass = pClass;` |
|       - |  502 | `	/* NO default is not the same as a NULL one, and the difference is php-visible:` |
|       - |  503 | ``	 * a typed slot without a default is UNINITIALIZED at `new` (reading it before`` |
|       - |  504 | `	 * the class writes it is an Error, hasDefaultValue() is false), which is what` |
|       - |  505 | `	 * php declares for LibXMLError's six fields and every reflector's $name. The` |
|       - |  506 | ``	 * machinery is already there for a compiled `public string $p;` — leaving`` |
|       - |  507 | `	 * pNativeValue at 0 is what selects it. */` |
|  910847 |  508 | `	if( pDef->sDefault.iType != PH7_NATIVE_VAL_NONE ){` |
|  761613 |  509 | `		pAttr->pNativeValue = &pDef->sDefault;` |
|  380804 |  510 | `	}` |
|  910847 |  511 | `	if( pDef->zType && pDef->zType[0] ){` |
|  272743 |  512 | `		NativeAttrType(pAttr,pDef->zType);` |
|  136369 |  513 | `	}` |
|  910847 |  514 | `	return PH7_ClassInstallAttr(pClass,pAttr);` |
|  455426 |  515 | `}` |
|       - |  516 | `/*` |
|       - |  517 | `` * Attach an `#[Attr(...)]` to a class declared from C.`` |
|       - |  518 | ` *` |
|       - |  519 | `` * php declares `#[Attribute(Attribute::TARGET_CLASS)]` on Attribute itself and a`` |
|       - |  520 | ` * target mask on Deprecated, and both records are LOAD-BEARING: the compiler` |
|       - |  521 | `` * reads them to decide whether a user's `#[Deprecated]` may sit where it does,`` |
|       - |  522 | ` * and ReflectionAttribute answers them. A compiled attribute holds its argument` |
|       - |  523 | ` * as byte-code; there is no compiler here, so the argument rides as the same` |
|       - |  524 | ` * literal record a native constant or property default uses and every reader` |
|       - |  525 | ` * takes that branch when the byte-code is empty.` |
|       - |  526 | ` */` |
|   10292 |  527 | `PH7_PRIVATE sxi32 PH7_NativeClassAddAttribute(ph7_vm *pVm,ph7_class *pClass,` |
|       - |  528 | `	const char *zAttr,const PH7_NativeAttrArg *aArg,sxu32 nArg)` |
|       5 |  529 | `{` |
|       - |  530 | `	ph7_attribute sAttr;` |
|       - |  531 | `	char *zDup;` |
|       - |  532 | `	sxu32 n;` |
|   10297 |  533 | `	if( pClass == 0 ){` |
|     ! 0 |  534 | `		return SXERR_NOTFOUND;` |
|       - |  535 | `	}` |
|   10297 |  536 | `	SyZero(&sAttr,sizeof(sAttr));` |
|   10297 |  537 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zAttr,SyStrlen(zAttr));` |
|   10297 |  538 | `	if( zDup == 0 ){` |
|     ! 0 |  539 | `		return SXERR_MEM;` |
|       - |  540 | `	}` |
|   10297 |  541 | `	SyStringInitFromBuf(&sAttr.sName,zDup,SyStrlen(zAttr));` |
|   10297 |  542 | `	SySetInit(&sAttr.aArgs,&pVm->sAllocator,sizeof(ph7_attr_arg));` |
|   20589 |  543 | `	for( n = 0 ; n < nArg ; n++ ){` |
|       - |  544 | `		ph7_attr_arg sArgRec;` |
|   10297 |  545 | `		SyZero(&sArgRec,sizeof(sArgRec));` |
|   10297 |  546 | `		SySetInit(&sArgRec.aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|   10297 |  547 | `		if( aArg[n].zName ){` |
|     ! 0 |  548 | `			char *zN = SyMemBackendStrDup(&pVm->sAllocator,aArg[n].zName,SyStrlen(aArg[n].zName));` |
|     ! 0 |  549 | `			if( zN ){` |
|     ! 0 |  550 | `				SyStringInitFromBuf(&sArgRec.sName,zN,SyStrlen(aArg[n].zName));` |
|     ! 0 |  551 | `			}` |
|     ! 0 |  552 | `		}` |
|   10297 |  553 | `		sArgRec.pNativeValue = (const void *)&aArg[n].sValue;` |
|   10297 |  554 | `		SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|    5151 |  555 | `	}` |
|   10297 |  556 | `	return SySetPut(&pClass->aAttrs,(const void *)&sAttr);` |
|    5151 |  557 | `}` |
|       - |  558 | `/*` |
|       - |  559 | ` * php's get_properties / get_debug_info handlers: the SHAPE a class SHOWS.` |
|       - |  560 | ` *` |
|       - |  561 | ` * Several native classes present something that is not their storage. php shows a` |
|       - |  562 | ` * DateTime as date/timezone_type/timezone (the state is a timestamp, an offset and` |
|       - |  563 | ` * a zone name), a DateTimeZone as timezone_type/timezone, and a WeakReference as` |
|       - |  564 | ` * ["object"]. Those slots carry PH7_MOD_HIDDEN so no presentation surface sees the` |
|       - |  565 | ` * engine state; this fills an array with what php shows instead.` |
|       - |  566 | ` *` |
|       - |  567 | ` * Answers 1 when the class (or an ancestor) declared a hook and pOut was filled.` |
|       - |  568 | ` * bDebug distinguishes php's two handlers: 1 for var_dump/print_r (get_debug_info),` |
|       - |  569 | ` * 0 for var_export and the (array) cast (get_properties). They disagree — a` |
|       - |  570 | ` * WeakReference shows ["object"] to var_dump and nothing to (array) — so the` |
|       - |  571 | ` * callback is told which is asking rather than each caller guessing.` |
|       - |  572 | ` * get_object_vars() and foreach are NOT callers: php answers those from the real` |
|       - |  573 | ` * properties with the caller's scope applied, which for every class here is empty.` |
|       - |  574 | ` */` |
|     556 |  575 | `PH7_PRIVATE int PH7_ClassInstancePresent(ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|       5 |  576 | `{` |
|       - |  577 | `	ph7_class *pClass;` |
|     561 |  578 | `	if( pThis == 0 \|\| pOut == 0 ){` |
|     ! 0 |  579 | `		return 0;` |
|       - |  580 | `	}` |
|     989 |  581 | `	for( pClass = pThis->pClass ; pClass ; pClass = pClass->pBase ){` |
|     589 |  582 | `		if( pClass->xPresent ){` |
|     159 |  583 | `			return pClass->xPresent(pThis->pVm,pThis,pOut,bDebug) == SXRET_OK;` |
|       - |  584 | `		}` |
|     219 |  585 | `	}` |
|     405 |  586 | `	return 0;` |
|     283 |  587 | `}` |
|       - |  588 | `/*` |
|       - |  589 | ` * Create and install ONE class from its spec: constants and properties, but` |
|       - |  590 | ` * neither methods nor its base chain.` |
|       - |  591 | ` *` |
|       - |  592 | ` * Split from the two passes that follow because a spec table may describe` |
|       - |  593 | ` * classes that extend each other, and PH7_ClassInherit needs the parent to` |
|       - |  594 | ` * exist -- and to already CARRY ITS METHODS, since inheriting is what copies` |
|       - |  595 | ` * them down.` |
|       - |  596 | ` */` |
|  730732 |  597 | `static sxi32 NativeDeclareClass(ph7_vm *pVm,const PH7_NativeClassSpec *pSpec,ph7_class **ppOut)` |
|       5 |  598 | `{` |
|       - |  599 | `	ph7_class *pClass;` |
|       - |  600 | `	SyString sName;` |
|       - |  601 | `	sxu32 n;` |
|       - |  602 | `	sxi32 rc;` |
|  730737 |  603 | `	SyStringInitFromBuf(&sName,pSpec->zName,SyStrlen(pSpec->zName));` |
|  730737 |  604 | `	pClass = PH7_NewRawClass(&(*pVm),&sName,0);` |
|  730737 |  605 | `	if( pClass == 0 ){` |
|     ! 0 |  606 | `		return SXERR_MEM;` |
|       - |  607 | `	}` |
|       - |  608 | `	/* PH7_CLASS_BOUND: an engine class is declared unconditionally, exactly once,` |
|       - |  609 | ``	 * so `class DateTime {}` in user code is php's "Cannot redeclare class`` |
|       - |  610 | `	 * DateTime". Only the compiler used to set the flag, so EVERY native class` |
|       - |  611 | `	 * was silently redeclarable — the chunk-declared ones (Exception, stdClass)` |
|       - |  612 | `	 * fataled and their C replacements did not. */` |
|  730737 |  613 | `	pClass->iFlags \|= pSpec->iFlags \| PH7_CLASS_BOUND;` |
|  730737 |  614 | `	pClass->xRelease = pSpec->xRelease;` |
|  730737 |  615 | `	pClass->pIterVtab = pSpec->pIterVtab;` |
|  730737 |  616 | `	pClass->xPresent = pSpec->xPresent;` |
| 1183585 |  617 | `	for( n = 0 ; n < pSpec->nConst ; n++ ){` |
|  452853 |  618 | `		rc = NativeInstallConstant(&(*pVm),pClass,&pSpec->aConst[n]);` |
|  452853 |  619 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  620 | `			return rc;` |
|       - |  621 | `		}` |
|  226429 |  622 | `	}` |
| 1641579 |  623 | `	for( n = 0 ; n < pSpec->nProp ; n++ ){` |
|  910847 |  624 | `		rc = PH7_NativeClassInstallProperty(&(*pVm),pClass,&pSpec->aProp[n]);` |
|  910847 |  625 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  626 | `			return rc;` |
|       - |  627 | `		}` |
|  455426 |  628 | `	}` |
|  730737 |  629 | `	*ppOut = pClass;` |
|  730737 |  630 | `	return PH7_VmInstallClass(&(*pVm),pClass);` |
|  365371 |  631 | `}` |
|       - |  632 | `/*` |
|       - |  633 | ` * Wire ONE class's base chain and interfaces.` |
|       - |  634 | ` *` |
|       - |  635 | ` * This runs AFTER every class in the table has its own methods, which is the` |
|       - |  636 | ` * compiler's order too (GenStateCompileClassEx compiles the whole body and only` |
|       - |  637 | ` * then calls PH7_ClassInherit/PH7_ClassImplement). Two things depend on it:` |
|       - |  638 | ` * PH7_ClassInherit COPIES the base's methods into the subclass, so a base whose` |
|       - |  639 | ` * methods were not installed yet hands down an empty table -- which is how` |
|       - |  640 | `` * `DOMDocument::C14N()` came out undefined, the first time a native class`` |
|       - |  641 | ` * extended another native class that had methods; and PH7_ClassImplement stubs` |
|       - |  642 | ` * every interface method the class lacks as ABSTRACT, so a spec may now name an` |
|       - |  643 | ` * interface it implements itself rather than attaching it by hand afterwards.` |
|       - |  644 | ` */` |
|  730732 |  645 | `static sxi32 NativeLinkClass(ph7_vm *pVm,const PH7_NativeClassSpec *pSpec,ph7_class *pClass)` |
|       5 |  646 | `{` |
|       - |  647 | `	sxi32 rc;` |
|  730737 |  648 | `	if( pSpec->zParent ){` |
|  385955 |  649 | `		ph7_class *pBase = NativeLookupClass(&(*pVm),pSpec->zParent);` |
|  385955 |  650 | `		if( pBase == 0 ){` |
|     ! 0 |  651 | `			return SXERR_NOTFOUND;` |
|       - |  652 | `		}` |
|       - |  653 | `		/* The VM's OWN generator state, not a NULL one: PH7_ClassInherit takes its` |
|       - |  654 | `		 * scratch allocator from pGen->pVm and reports every inheritance rule it` |
|       - |  655 | `		 * enforces through PH7_GenCompileError(pGen, ...). Passing 0 crashed the` |
|       - |  656 | `		 * moment a native spec first named a parent (the date exceptions). */` |
|  578930 |  657 | `		rc = (pClass->iFlags & PH7_CLASS_INTERFACE)` |
|   41168 |  658 | `			? PH7_ClassInterfaceInherit(pClass,pBase)` |
|  365366 |  659 | `			: PH7_ClassInherit(&pVm->sCodeGen,pClass,pBase);` |
|  385955 |  660 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  661 | `			return rc;` |
|       - |  662 | `		}` |
|  192975 |  663 | `	}` |
|  730737 |  664 | `	if( pSpec->zImplements ){` |
|       - |  665 | `		/* Comma-separated list, so one spec row can name several interfaces. */` |
|  169823 |  666 | `		const char *zCur = pSpec->zImplements;` |
|  432269 |  667 | `		while( zCur[0] != '\0' ){` |
|       - |  668 | `			const char *zStart;` |
|       - |  669 | `			char zIface[64];` |
|       - |  670 | `			sxu32 nLen;` |
|       - |  671 | `			ph7_class *pIface;` |
|  355079 |  672 | `			while( zCur[0] == ' ' \|\| zCur[0] == ',' ){` |
|   92633 |  673 | `				zCur++;` |
|       5 |  674 | `			}` |
|  262451 |  675 | `			if( zCur[0] == '\0' ){` |
|     ! 0 |  676 | `				break;` |
|       - |  677 | `			}` |
|  262451 |  678 | `			zStart = zCur;` |
| 3375781 |  679 | `			while( zCur[0] != '\0' && zCur[0] != ',' && zCur[0] != ' ' ){` |
| 3113335 |  680 | `				zCur++;` |
|       5 |  681 | `			}` |
|  262451 |  682 | `			nLen = (sxu32)(zCur - zStart);` |
|  262451 |  683 | `			if( nLen >= sizeof(zIface) ){` |
|     ! 0 |  684 | `				return SXERR_SYNTAX;` |
|       - |  685 | `			}` |
|  262451 |  686 | `			SyMemcpy(zStart,zIface,nLen);` |
|  262451 |  687 | `			zIface[nLen] = '\0';` |
|  262451 |  688 | `			pIface = NativeLookupClass(&(*pVm),zIface);` |
|  262451 |  689 | `			if( pIface == 0 ){` |
|     ! 0 |  690 | `				return SXERR_NOTFOUND;` |
|       - |  691 | `			}` |
|  262451 |  692 | `			rc = PH7_ClassImplement(pClass,pIface);` |
|  262451 |  693 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  694 | `				return rc;` |
|       - |  695 | `			}` |
|       5 |  696 | `		}` |
|   84909 |  697 | `	}` |
|  730737 |  698 | `	return SXRET_OK;` |
|  365371 |  699 | `}` |
|       - |  700 | `/*` |
|       - |  701 | ` * Install a whole table of native classes, in the compiler's own order: declare` |
|       - |  702 | ` * them all (so later rows may extend earlier ones), fill in their methods, wire` |
|       - |  703 | ` * the base chains and interfaces, then mount.` |
|       - |  704 | ` *` |
|       - |  705 | ` * Interfaces are declared but never mounted -- VmMountUserClass skips them anyway,` |
|       - |  706 | ` * their methods being non-invocable.` |
|       - |  707 | ` */` |
|  174964 |  708 | `PH7_PRIVATE sxi32 PH7_InstallNativeClasses(ph7_vm *pVm,const PH7_NativeClassSpec *aSpec,sxu32 nSpec)` |
|       5 |  709 | `{` |
|       - |  710 | `	ph7_class **apClass;` |
|       - |  711 | `	sxu32 i,j;` |
|       - |  712 | `	sxi32 rc;` |
|  174969 |  713 | `	apClass = (ph7_class **)SyMemBackendAlloc(&pVm->sAllocator,nSpec * sizeof(ph7_class *));` |
|  174969 |  714 | `	if( apClass == 0 ){` |
|     ! 0 |  715 | `		return SXERR_MEM;` |
|       - |  716 | `	}` |
|  905701 |  717 | `	for( i = 0 ; i < nSpec ; i++ ){` |
|  730737 |  718 | `		apClass[i] = 0;` |
|  730737 |  719 | `		rc = NativeDeclareClass(&(*pVm),&aSpec[i],&apClass[i]);` |
|  730737 |  720 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  721 | `			goto Done;` |
|       - |  722 | `		}` |
|  365371 |  723 | `	}` |
|  905701 |  724 | `	for( i = 0 ; i < nSpec ; i++ ){` |
| 4868121 |  725 | `		for( j = 0 ; j < aSpec[i].nMethod ; j++ ){` |
| 4137389 |  726 | `			rc = PH7_NativeClassInstallMethod(&(*pVm),apClass[i],&aSpec[i].aMethod[j],0);` |
| 4137389 |  727 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  728 | `				goto Done;` |
|       - |  729 | `			}` |
| 2068697 |  730 | `		}` |
|  365371 |  731 | `	}` |
|  905701 |  732 | `	for( i = 0 ; i < nSpec ; i++ ){` |
|  730737 |  733 | `		rc = NativeLinkClass(&(*pVm),&aSpec[i],apClass[i]);` |
|  730737 |  734 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  735 | `			goto Done;` |
|       - |  736 | `		}` |
|  365371 |  737 | `	}` |
|  905701 |  738 | `	for( i = 0 ; i < nSpec ; i++ ){` |
|  730737 |  739 | `		rc = VmMountUserClass(&(*pVm),apClass[i]);` |
|  730737 |  740 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  741 | `			goto Done;` |
|       - |  742 | `		}` |
|  365371 |  743 | `	}` |
|  174969 |  744 | `	rc = SXRET_OK;` |
|   87482 |  745 | `Done:` |
|  174969 |  746 | `	SyMemBackendFree(&pVm->sAllocator,apClass);` |
|  174969 |  747 | `	return rc;` |
|   87487 |  748 | `}` |
|       - |  749 | `/*` |
|       - |  750 | ` * ---------------------------------------------------------------------------` |
|       - |  751 | ` * Declaring an ENUM from C.` |
|       - |  752 | ` *` |
|       - |  753 | `` * An enum is not a class with constants: each `case` is a class constant whose`` |
|       - |  754 | ` * slot holds THE singleton instance of the enum for that case, materialized` |
|       - |  755 | ` * lazily on first access (VmEnumMaterializeCase). The compiler builds one by` |
|       - |  756 | `` * declaring the readonly `name`/`value` properties, pushing each case onto`` |
|       - |  757 | ` * ph7_class::aEnumCases, and SYNTHESIZING cases()/from()/tryFrom() as PHP` |
|       - |  758 | `` * source that forwards to the `__phl_enum_*` thunks.`` |
|       - |  759 | ` *` |
|       - |  760 | ` * This does the same three things without a compiler: the case's backing value` |
|       - |  761 | ` * rides as a literal (ph7_class_attr::pNativeValue, which the materializer` |
|       - |  762 | ` * reads where a compiled case has byte-code), and the three interface methods` |
|       - |  763 | ` * are C bodies that call the very same engine workers the synthesized PHP` |
|       - |  764 | `` * forwards to — so a native enum is an ordinary one to `instanceof`,`` |
|       - |  765 | `` * `match`, Reflection and `===` case identity.`` |
|       - |  766 | ` * ---------------------------------------------------------------------------` |
|       - |  767 | ` */` |
|       - |  768 | `/* The enum a static native method was called on. */` |
|     194 |  769 | `static ph7_class * NativeEnumSelf(ph7_context *pCtx,ph7_value *pName)` |
|       2 |  770 | `{` |
|     196 |  771 | `	ph7_class *pClass = PH7_ContextCalledClass(pCtx);` |
|     196 |  772 | `	PH7_MemObjInit(pCtx->pVm,pName);` |
|     196 |  773 | `	if( pClass ){` |
|     196 |  774 | `		ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|      97 |  775 | `	}` |
|     196 |  776 | `	return pClass;` |
|       2 |  777 | `}` |
|       - |  778 | `/*` |
|       - |  779 | ` * cases() / from() / tryFrom(): the engine thunks take the enum's FQN as their` |
|       - |  780 | ` * first argument, exactly as the compiler's synthesized bodies pass it.` |
|       - |  781 | ` */` |
|      16 |  782 | `static int vm_builtin_NativeEnum_cases(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  783 | `{` |
|       - |  784 | `	ph7_value sName;` |
|       - |  785 | `	ph7_value *ap[1];` |
|       - |  786 | `	int rc;` |
|       8 |  787 | `	SXUNUSED(nArg);` |
|       8 |  788 | `	SXUNUSED(apArg);` |
|      17 |  789 | `	if( NativeEnumSelf(pCtx,&sName) == 0 ){` |
|     ! 0 |  790 | `		ph7_result_null(pCtx);` |
|     ! 0 |  791 | `		return PH7_OK;` |
|       - |  792 | `	}` |
|      17 |  793 | `	ap[0] = &sName;` |
|      17 |  794 | `	rc = vm_builtin_enum_cases(pCtx,1,ap);` |
|      17 |  795 | `	PH7_MemObjRelease(&sName);` |
|      17 |  796 | `	return rc;` |
|       9 |  797 | `}` |
|     178 |  798 | `static int NativeEnumFrom(ph7_context *pCtx,int nArg,ph7_value **apArg,int bTry)` |
|       2 |  799 | `{` |
|       - |  800 | `	ph7_value sName;` |
|       - |  801 | `	ph7_value *ap[2];` |
|       - |  802 | `	int rc;` |
|     180 |  803 | `	if( nArg < 1 \|\| NativeEnumSelf(pCtx,&sName) == 0 ){` |
|     ! 0 |  804 | `		ph7_result_null(pCtx);` |
|     ! 0 |  805 | `		return PH7_OK;` |
|       - |  806 | `	}` |
|     180 |  807 | `	ap[0] = &sName;` |
|     180 |  808 | `	ap[1] = apArg[0];` |
|     180 |  809 | `	rc = bTry ? vm_builtin_enum_tryfrom(pCtx,2,ap) : vm_builtin_enum_from(pCtx,2,ap);` |
|     180 |  810 | `	PH7_MemObjRelease(&sName);` |
|     180 |  811 | `	return rc;` |
|      91 |  812 | `}` |
|      96 |  813 | `static int vm_builtin_NativeEnum_from(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  814 | `{` |
|      98 |  815 | `	return NativeEnumFrom(pCtx,nArg,apArg,0);` |
|       2 |  816 | `}` |
|      82 |  817 | `static int vm_builtin_NativeEnum_tryFrom(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  818 | `{` |
|      83 |  819 | `	return NativeEnumFrom(pCtx,nArg,apArg,1);` |
|       1 |  820 | `}` |
|       - |  821 | ``/* The readonly `name` (every enum) and `value` (backed only) case properties,`` |
|       - |  822 | ` * declared exactly as GenStateCompileEnum does. */` |
|   10292 |  823 | `static sxi32 NativeEnumInstallProp(ph7_vm *pVm,ph7_class *pClass,const char *zName,` |
|       - |  824 | `	sxu32 nType,const char *zTypeName)` |
|       5 |  825 | `{` |
|       - |  826 | `	SyString sName;` |
|       - |  827 | `	ph7_class_attr *pAttr;` |
|   10297 |  828 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|   10297 |  829 | `	pAttr = PH7_NewClassAttr(&(*pVm),&sName,0,PH7_CLASS_PROT_PUBLIC,` |
|       - |  830 | `		PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|   10297 |  831 | `	if( pAttr == 0 ){` |
|     ! 0 |  832 | `		return SXERR_MEM;` |
|       - |  833 | `	}` |
|   10297 |  834 | `	pAttr->nType = nType;` |
|   10297 |  835 | `	SyStringInitFromBuf(&pAttr->sTypeName,zTypeName,SyStrlen(zTypeName));` |
|   10297 |  836 | `	return PH7_ClassInstallAttr(pClass,pAttr);` |
|    5151 |  837 | `}` |
|       - |  838 | `/*` |
|       - |  839 | ` * cases()/from()/tryFrom(), installed on ANY enum -- one declared from C here` |
|       - |  840 | ` * and one the compiler just finished reading from source. php declares them on` |
|       - |  841 | ``  * the UnitEnum/BackedEnum prototypes, which is why `from` takes `string\|int` `` |
|       - |  842 | ` * rather than the enum's own backing type: the refusal for the wrong one is a` |
|       - |  843 | ` * VALUE check inside the body, not the parameter's.` |
|       - |  844 | ` *` |
|       - |  845 | ` * The compiler used to synthesize PHP source forwarding to three global` |
|       - |  846 | `` * `__phl_enum_*` thunks. Those names were php-visible, and the methods reported`` |
|       - |  847 | `` * as `<user>` with the enum's file and line where php reports`` |
|       - |  848 | `` * `<internal, prototype BackedEnum>`.`` |
|       - |  849 | ` */` |
|       - |  850 | `/*` |
|       - |  851 | ` * Install one of them and stamp it INTERNAL unconditionally. The usual` |
|       - |  852 | `` * `bCompilingBuiltin` stamp only fires while the engine compiles its OWN`` |
|       - |  853 | ` * sources, and these three are attached to a class the compiler is reading out` |
|       - |  854 | ` * of a USER file — but php reports them as internal wherever the enum is` |
|       - |  855 | ` * declared: isInternal() true, getFileName() false, getStartLine() 0.` |
|       - |  856 | ` */` |
|   15638 |  857 | `static sxi32 NativeEnumInstallMethod(ph7_vm *pVm,ph7_class *pClass,` |
|       - |  858 | `	const PH7_NativeMethodDef *pDef)` |
|       5 |  859 | `{` |
|       - |  860 | `	ph7_class_method *pMeth;` |
|   15643 |  861 | `	sxi32 rc = PH7_NativeClassInstallMethod(&(*pVm),pClass,pDef,0);` |
|   15643 |  862 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  863 | `		return rc;` |
|       - |  864 | `	}` |
|   15643 |  865 | `	pMeth = PH7_ClassExtractMethod(pClass,pDef->zName,(sxu32)SyStrlen(pDef->zName));` |
|   15643 |  866 | `	if( pMeth ){` |
|   15643 |  867 | `		pMeth->sFunc.iFlags \|= VM_FUNC_INTERNAL;` |
|       - |  868 | `		/* getFileName() reads the recorded source file rather than the flag, and` |
|       - |  869 | `		 * PH7_NewClassMethod stamped the user file the enum was read from. */` |
|   15643 |  870 | `		SyStringInitFromBuf(&pMeth->sFunc.sFile,"",0);` |
|    7819 |  871 | `	}` |
|   15643 |  872 | `	return SXRET_OK;` |
|    7824 |  873 | `}` |
|    5242 |  874 | `PH7_PRIVATE sxi32 PH7_InstallEnumInterfaceMethods(ph7_vm *pVm,ph7_class *pClass)` |
|       5 |  875 | `{` |
|       - |  876 | `	static const PH7_NativeMethodDef sCases =` |
|       - |  877 | `		{ "cases", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "", "array", vm_builtin_NativeEnum_cases };` |
|       - |  878 | `	static const PH7_NativeMethodDef aFrom[] = {` |
|       - |  879 | `		{ "from",    PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "string\|int $value", "static",` |
|       - |  880 | `		  vm_builtin_NativeEnum_from },` |
|       - |  881 | `		{ "tryFrom", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "string\|int $value", "?static",` |
|       - |  882 | `		  vm_builtin_NativeEnum_tryFrom },` |
|       - |  883 | `	};` |
|       - |  884 | `	sxu32 n;` |
|    5247 |  885 | `	sxi32 rc = NativeEnumInstallMethod(&(*pVm),pClass,&sCases);` |
|    5247 |  886 | `	if( rc != SXRET_OK \|\| pClass->nEnumBacking == 0 ){` |
|      48 |  887 | `		return rc;` |
|       - |  888 | `	}` |
|   15599 |  889 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFrom) ; n++ ){` |
|   10401 |  890 | `		rc = NativeEnumInstallMethod(&(*pVm),pClass,&aFrom[n]);` |
|   10401 |  891 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  892 | `			return rc;` |
|       - |  893 | `		}` |
|    5203 |  894 | `	}` |
|    5203 |  895 | `	return SXRET_OK;` |
|    2626 |  896 | `}` |
|    5146 |  897 | `PH7_PRIVATE sxi32 PH7_InstallNativeEnum(ph7_vm *pVm,const char *zName,sxu32 nBacking,` |
|       - |  898 | `	const PH7_NativeEnumCase *aCase,sxu32 nCase,` |
|       - |  899 | `	const PH7_NativeMethodDef *aMethod,sxu32 nMethod)` |
|       5 |  900 | `{` |
|       - |  901 | `	ph7_class *pClass, *pIface;` |
|       - |  902 | `	SyString sName;` |
|       - |  903 | `	sxu32 n;` |
|       - |  904 | `	sxi32 rc;` |
|    5151 |  905 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|    5151 |  906 | `	pClass = PH7_NewRawClass(&(*pVm),&sName,0);` |
|    5151 |  907 | `	if( pClass == 0 ){` |
|     ! 0 |  908 | `		return SXERR_MEM;` |
|       - |  909 | `	}` |
|       - |  910 | `	/* php: an enum is implicitly FINAL and cannot be instantiated. */` |
|    5151 |  911 | `	pClass->iFlags \|= PH7_CLASS_ENUM\|PH7_CLASS_FINAL;` |
|    5151 |  912 | `	pClass->nEnumBacking = nBacking;` |
|    5151 |  913 | `	rc = NativeEnumInstallProp(&(*pVm),pClass,"name",MEMOBJ_STRING,"string");` |
|    5151 |  914 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  915 | `		return rc;` |
|       - |  916 | `	}` |
|    5151 |  917 | `	if( nBacking != 0 ){` |
|    7724 |  918 | `		rc = NativeEnumInstallProp(&(*pVm),pClass,"value",nBacking,` |
|    2573 |  919 | `			nBacking == MEMOBJ_INT ? "int" : "string");` |
|    5151 |  920 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  921 | `			return rc;` |
|       - |  922 | `		}` |
|    2573 |  923 | `	}` |
|   15443 |  924 | `	for( n = 0 ; n < nCase ; n++ ){` |
|       - |  925 | `		ph7_class_attr *pAttr;` |
|       - |  926 | `		SyString sCase;` |
|   10297 |  927 | `		SyStringInitFromBuf(&sCase,aCase[n].zName,SyStrlen(aCase[n].zName));` |
|   10297 |  928 | `		pAttr = PH7_NewClassAttr(&(*pVm),&sCase,0,PH7_CLASS_PROT_PUBLIC,` |
|       - |  929 | `			PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|   10297 |  930 | `		if( pAttr == 0 ){` |
|     ! 0 |  931 | `			return SXERR_MEM;` |
|       - |  932 | `		}` |
|   10297 |  933 | `		pAttr->pDeclClass = pClass;` |
|       - |  934 | `		/* The backing literal where a compiled case carries byte-code. */` |
|   10297 |  935 | `		if( nBacking != 0 ){` |
|   10297 |  936 | `			pAttr->pNativeValue = &aCase[n].sValue;` |
|    5146 |  937 | `		}` |
|   10297 |  938 | `		rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   10297 |  939 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  940 | `			return rc;` |
|       - |  941 | `		}` |
|       - |  942 | `		/* Declaration order, which is the order cases() reports. */` |
|   10297 |  943 | `		SySetPut(&pClass->aEnumCases,(const void *)&pAttr);` |
|    5151 |  944 | `	}` |
|    5151 |  945 | `	for( n = 0 ; n < nMethod ; n++ ){` |
|     ! 0 |  946 | `		rc = PH7_NativeClassInstallMethod(&(*pVm),pClass,&aMethod[n],0);` |
|     ! 0 |  947 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  948 | `			return rc;` |
|       - |  949 | `		}` |
|     ! 0 |  950 | `	}` |
|    5151 |  951 | `	rc = PH7_InstallEnumInterfaceMethods(&(*pVm),pClass);` |
|    5151 |  952 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  953 | `		return rc;` |
|       - |  954 | `	}` |
|    5151 |  955 | `	rc = PH7_VmInstallClass(&(*pVm),pClass);` |
|    5151 |  956 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  957 | `		return rc;` |
|       - |  958 | `	}` |
|       - |  959 | ``	/* php 8.1: every enum satisfies `instanceof UnitEnum`, a backed one`` |
|       - |  960 | ``	 * `BackedEnum` too. Attached AFTER the methods, so PH7_ClassImplement's`` |
|       - |  961 | `	 * abstract stubbing finds cases()/from()/tryFrom() already declared. */` |
|    5151 |  962 | `	pIface = NativeLookupClass(&(*pVm),"UnitEnum");` |
|    5151 |  963 | `	if( pIface == 0 ){` |
|     ! 0 |  964 | `		return SXERR_NOTFOUND;` |
|       - |  965 | `	}` |
|    5151 |  966 | `	rc = PH7_ClassImplement(pClass,pIface);` |
|    5151 |  967 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  968 | `		return rc;` |
|       - |  969 | `	}` |
|    5151 |  970 | `	if( nBacking != 0 ){` |
|    5151 |  971 | `		pIface = NativeLookupClass(&(*pVm),"BackedEnum");` |
|    5151 |  972 | `		if( pIface == 0 ){` |
|     ! 0 |  973 | `			return SXERR_NOTFOUND;` |
|       - |  974 | `		}` |
|    5151 |  975 | `		rc = PH7_ClassImplement(pClass,pIface);` |
|    5151 |  976 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  977 | `			return rc;` |
|       - |  978 | `		}` |
|    2573 |  979 | `	}` |
|    5151 |  980 | `	return VmMountUserClass(&(*pVm),pClass);` |
|    2578 |  981 | `}` |
|       - |  982 | `/*` |
|       - |  983 | ` * ---------------------------------------------------------------------------` |
|       - |  984 | ` * InternalIterator.` |
|       - |  985 | ` *` |
|       - |  986 | ` * A native IteratorAggregate cannot answer a Generator: a PHP generator IS its` |
|       - |  987 | ` * byte-code, and a C body has none. php never answers one either -- its internal` |
|       - |  988 | ` * aggregates hand back an InternalIterator wrapping the iterator their class` |
|       - |  989 | ` * declared -- so PHL declares that class once, here, and every native aggregate` |
|       - |  990 | ` * reaches it through ph7_class::pIterVtab.` |
|       - |  991 | ` *` |
|       - |  992 | ` * The cursor lives entirely in the iterator's own private slots. A vtable states` |
|       - |  993 | ` * only how to REACH a position; reading it back is the same three methods for` |
|       - |  994 | ` * everyone.` |
|       - |  995 | ` * ---------------------------------------------------------------------------` |
|       - |  996 | ` */` |
|       - |  997 | `/* The walk THIS iterator was made for: the vtable of the aggregate it holds. */` |
|     272 |  998 | `static const PH7_NativeIterVtab * NativeIterVtab(ph7_class_instance *pIt)` |
|       2 |  999 | `{` |
|     274 | 1000 | `	ph7_class_instance *pSrc = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);` |
|     274 | 1001 | `	return pSrc ? pSrc->pClass->pIterVtab : 0;` |
|       2 | 1002 | `}` |
|      76 | 1003 | `static int vm_builtin_InternalIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1004 | `{` |
|      78 | 1005 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       - | 1006 | `	const PH7_NativeIterVtab *pVtab;` |
|      38 | 1007 | `	SXUNUSED(nArg);` |
|      38 | 1008 | `	SXUNUSED(apArg);` |
|      78 | 1009 | `	if( pThis == 0 ){` |
|     ! 0 | 1010 | `		return PH7_OK;` |
|       - | 1011 | `	}` |
|      78 | 1012 | `	pVtab = NativeIterVtab(pThis);` |
|      78 | 1013 | `	if( pVtab == 0 \|\| pVtab->xRewind == 0 ){` |
|     ! 0 | 1014 | `		PH7_NativeSetAttrBool(pCtx->pVm,pThis,PH7_NATIVE_IT_DONE,1);` |
|     ! 0 | 1015 | `		return PH7_OK;` |
|       - | 1016 | `	}` |
|      78 | 1017 | `	pVtab->xRewind(pCtx->pVm,pThis);` |
|      78 | 1018 | `	return PH7_OK;` |
|      40 | 1019 | `}` |
|     196 | 1020 | `static int vm_builtin_InternalIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1021 | `{` |
|     198 | 1022 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       - | 1023 | `	const PH7_NativeIterVtab *pVtab;` |
|      98 | 1024 | `	SXUNUSED(nArg);` |
|      98 | 1025 | `	SXUNUSED(apArg);` |
|     198 | 1026 | `	if( pThis == 0 \|\| PH7_NativeAttrTruthy(pThis,PH7_NATIVE_IT_DONE) ){` |
|     ! 0 | 1027 | `		return PH7_OK;` |
|       - | 1028 | `	}` |
|     198 | 1029 | `	pVtab = NativeIterVtab(pThis);` |
|     198 | 1030 | `	if( pVtab == 0 \|\| pVtab->xNext == 0 ){` |
|     ! 0 | 1031 | `		PH7_NativeSetAttrBool(pCtx->pVm,pThis,PH7_NATIVE_IT_DONE,1);` |
|     ! 0 | 1032 | `		return PH7_OK;` |
|       - | 1033 | `	}` |
|     198 | 1034 | `	pVtab->xNext(pCtx->pVm,pThis);` |
|     198 | 1035 | `	return PH7_OK;` |
|     100 | 1036 | `}` |
|     266 | 1037 | `static int vm_builtin_InternalIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1038 | `{` |
|     268 | 1039 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     133 | 1040 | `	SXUNUSED(nArg);` |
|     133 | 1041 | `	SXUNUSED(apArg);` |
|     268 | 1042 | `	ph7_result_bool(pCtx,pThis != 0 && !PH7_NativeAttrTruthy(pThis,PH7_NATIVE_IT_DONE));` |
|     268 | 1043 | `	return PH7_OK;` |
|       2 | 1044 | `}` |
|       - | 1045 | `/* current() and key() answer the slots the walk left behind -- and NULL once it` |
|       - | 1046 | ` * is over, which is what php's exhausted InternalIterator answers too. */` |
|     322 | 1047 | `static int NativeIterReadSlot(ph7_context *pCtx,const char *zSlot)` |
|       2 | 1048 | `{` |
|     324 | 1049 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       - | 1050 | `	ph7_value *pVal;` |
|     324 | 1051 | `	if( pThis == 0 \|\| PH7_NativeAttrTruthy(pThis,PH7_NATIVE_IT_DONE) ){` |
|     ! 0 | 1052 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1053 | `		return PH7_OK;` |
|       - | 1054 | `	}` |
|     324 | 1055 | `	pVal = PH7_NativeAttr(pThis,zSlot);` |
|     324 | 1056 | `	if( pVal ){` |
|     324 | 1057 | `		ph7_result_value(pCtx,pVal);` |
|     161 | 1058 | `	}` |
|     324 | 1059 | `	return PH7_OK;` |
|     163 | 1060 | `}` |
|     194 | 1061 | `static int vm_builtin_InternalIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1062 | `{` |
|      97 | 1063 | `	SXUNUSED(nArg);` |
|      97 | 1064 | `	SXUNUSED(apArg);` |
|     196 | 1065 | `	return NativeIterReadSlot(pCtx,PH7_NATIVE_IT_CUR);` |
|       2 | 1066 | `}` |
|     128 | 1067 | `static int vm_builtin_InternalIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1068 | `{` |
|      64 | 1069 | `	SXUNUSED(nArg);` |
|      64 | 1070 | `	SXUNUSED(apArg);` |
|     130 | 1071 | `	return NativeIterReadSlot(pCtx,PH7_NATIVE_IT_KEY);` |
|       2 | 1072 | `}` |
|       - | 1073 | `/* InternalIterator::__construct() — private in php, and never reached from PHP:` |
|       - | 1074 | ` * PH7_NativeIteratorNew builds the instance directly. */` |
|     ! 0 | 1075 | `static int vm_builtin_InternalIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 | 1076 | `{` |
|     ! 0 | 1077 | `	SXUNUSED(nArg);` |
|     ! 0 | 1078 | `	SXUNUSED(apArg);` |
|     ! 0 | 1079 | `	SXUNUSED(pCtx);` |
|     ! 0 | 1080 | `	return PH7_OK;` |
|     ! 0 | 1081 | `}` |
|       - | 1082 | `/*` |
|       - | 1083 | ` * The iterator a native getIterator() answers: bound to its aggregate and already` |
|       - | 1084 | ` * positioned, because php's is valid() before the first rewind().` |
|       - | 1085 | ` */` |
|      92 | 1086 | `PH7_PRIVATE ph7_class_instance * PH7_NativeIteratorNew(ph7_vm *pVm,ph7_class_instance *pSrc)` |
|       2 | 1087 | `{` |
|      94 | 1088 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),"InternalIterator",` |
|       - | 1089 | `		sizeof("InternalIterator")-1,FALSE,0);` |
|       - | 1090 | `	ph7_class_instance *pIt;` |
|       - | 1091 | `	const PH7_NativeIterVtab *pVtab;` |
|      94 | 1092 | `	if( pClass == 0 \|\| pSrc == 0 ){` |
|     ! 0 | 1093 | `		return 0;` |
|       - | 1094 | `	}` |
|      94 | 1095 | `	pIt = PH7_NewClassInstance(&(*pVm),pClass);` |
|      94 | 1096 | `	if( pIt == 0 ){` |
|     ! 0 | 1097 | `		return 0;` |
|       - | 1098 | `	}` |
|      94 | 1099 | `	PH7_NativeSetAttrObj(&(*pVm),pIt,PH7_NATIVE_IT_SRC,pSrc);` |
|      94 | 1100 | `	PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|      94 | 1101 | `	pVtab = pSrc->pClass->pIterVtab;` |
|      94 | 1102 | `	if( pVtab && pVtab->xRewind ){` |
|      94 | 1103 | `		pVtab->xRewind(&(*pVm),pIt);` |
|      46 | 1104 | `	}` |
|      94 | 1105 | `	return pIt;` |
|      48 | 1106 | `}` |
|    5146 | 1107 | `PH7_PRIVATE sxi32 PH7_VmInstallNativeIterator(ph7_vm *pVm)` |
|       5 | 1108 | `{` |
|       - | 1109 | `	static const PH7_NativePropDef aProp[] = {` |
|       - | 1110 | `		{ PH7_NATIVE_IT_SRC,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|       - | 1111 | `		{ PH7_NATIVE_IT_CUR,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|       - | 1112 | `		{ PH7_NATIVE_IT_KEY,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|       - | 1113 | `		{ PH7_NATIVE_IT_POS,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,  0, 0, 0.0 }, 0 },` |
|       - | 1114 | `		{ PH7_NATIVE_IT_AUX,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,  0, 0, 0.0 }, 0 },` |
|       - | 1115 | `		{ PH7_NATIVE_IT_DONE, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_BOOL, 1, 0, 0.0 }, 0 },` |
|       - | 1116 | `	};` |
|       - | 1117 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|       - | 1118 | `		{ "__construct", PH7_MOD_PRIVATE, "", "", vm_builtin_InternalIterator_construct },` |
|       - | 1119 | `		{ "current",     PH7_MOD_PUBLIC, "", "mixed", vm_builtin_InternalIterator_current },` |
|       - | 1120 | `		{ "key",         PH7_MOD_PUBLIC, "", "mixed", vm_builtin_InternalIterator_key },` |
|       - | 1121 | `		{ "next",        PH7_MOD_PUBLIC, "", "void",  vm_builtin_InternalIterator_next },` |
|       - | 1122 | `		{ "valid",       PH7_MOD_PUBLIC, "", "bool",  vm_builtin_InternalIterator_valid },` |
|       - | 1123 | `		{ "rewind",      PH7_MOD_PUBLIC, "", "void",  vm_builtin_InternalIterator_rewind },` |
|       - | 1124 | `	};` |
|       - | 1125 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|       - | 1126 | `		{ "InternalIterator", 0, 0, PH7_CLASS_FINAL,` |
|       - | 1127 | `		  aMethod, SX_ARRAYSIZE(aMethod), 0, 0, aProp, SX_ARRAYSIZE(aProp), 0, 0, 0 },` |
|       - | 1128 | `	};` |
|       - | 1129 | `	ph7_class *pIt,*pIterator;` |
|       - | 1130 | `	sxi32 rc;` |
|    5151 | 1131 | `	rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|    5151 | 1132 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1133 | `		return rc;` |
|       - | 1134 | `	}` |
|       - | 1135 | ``	/* `implements Iterator` only NOW: PH7_ClassImplement stubs every interface`` |
|       - | 1136 | `	 * method the class does not already declare as ABSTRACT, so attaching it` |
|       - | 1137 | `	 * through the spec would have made InternalIterator uninstantiable. */` |
|    5151 | 1138 | `	pIt = NativeLookupClass(&(*pVm),"InternalIterator");` |
|    5151 | 1139 | `	pIterator = NativeLookupClass(&(*pVm),"Iterator");` |
|    5151 | 1140 | `	if( pIt == 0 \|\| pIterator == 0 ){` |
|     ! 0 | 1141 | `		return SXERR_NOTFOUND;` |
|       - | 1142 | `	}` |
|    5151 | 1143 | `	return PH7_ClassImplement(pIt,pIterator);` |
|    2578 | 1144 | `}` |
|       - | 1145 |  |
