# src/ph7/oo_native.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 889/996 lines (89.26%)

[Root index](../../index.md) | [Directory index](index.md)

|     Hits | Line | Source |
| -------: | ---: | :--- |
|        - |    1 | `/**` |
|        - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|        - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|        - |    4 | ` */` |
|        - |    5 | `#include "ph7int.h"` |
|        - |    6 | `/*` |
|        - |    7 | ` * Declaring a class from C.` |
|        - |    8 | ` *` |
|        - |    9 | ` * Every built-in class subsystem used to be an embedded PHP source string compiled` |
|        - |   10 | ` * at VM init, reaching the engine through a GLOBAL C thunk per operation` |
|        - |   11 | `` * (`__reflect_class_info()`, `__gen_next()`, `__dom_*`, ~110 of them). The reason`` |
|        - |   12 | ` * was structural: a ph7_class_method carries a ph7_vm_func, whose only body is` |
|        - |   13 | ` * bytecode, so C code could only ever be a global function.` |
|        - |   14 | ` *` |
|        - |   15 | ` * VM_FUNC_NATIVE removed that restriction (a method body may be a C routine), and` |
|        - |   16 | ` * this file is the front door to it: a declarative table describing a class —` |
|        - |   17 | ` * parent, interfaces, constants, methods — that PH7_InstallNativeClasses() turns` |
|        - |   18 | ` * into a real, mounted ph7_class. The thunks become what they always were,` |
|        - |   19 | ` * methods, and stop being visible in the global namespace.` |
|        - |   20 | ` *` |
|        - |   21 | ` * Nothing here is new machinery. It drives the same builders the COMPILER drives` |
|        - |   22 | `` * for `class Foo {}` — PH7_NewRawClass, PH7_NewClassMethod, PH7_ClassInstallMethod,`` |
|        - |   23 | ` * PH7_ClassInherit, PH7_ClassImplement, PH7_VmInstallClass, VmMountUserClass — so a` |
|        - |   24 | ` * native class is not a second kind of class: Reflection, instanceof, inheritance,` |
|        - |   25 | ` * visibility and autoloading all see an ordinary one.` |
|        - |   26 | ` */` |
|        - |   27 | `/*` |
|        - |   28 | ` * Materialize a native declaration's literal initializer into a value slot.` |
|        - |   29 | ` *` |
|        - |   30 | ` * A compiled declaration expresses its default as byte-code evaluated at mount` |
|        - |   31 | `` * (constants, statics) or at `new` (instance properties). The C builder has no`` |
|        - |   32 | ` * compiler to emit that, so it carries the literal on the attribute` |
|        - |   33 | ` * (ph7_class_attr::pNativeValue) and both of those sites call this instead --` |
|        - |   34 | ` * which is what a literal initializer's byte-code would have produced anyway.` |
|        - |   35 | ` */` |
|  9921336 |   36 | `PH7_PRIVATE void PH7_NativeLiteralValue(ph7_vm *pVm,const void *pLiteral,ph7_value *pOut)` |
|        5 |   37 | `{` |
|  9921341 |   38 | `	const PH7_NativeConstDef *pLit = (const PH7_NativeConstDef *)pLiteral;` |
|        - |   39 | `	/* Every PH7_MemObjInitFrom* below SyZeros the whole value, nIdx included — and` |
|        - |   40 | `	 * a CONSTANT's or STATIC's slot is addressed by that index afterwards` |
|        - |   41 | ``	 * (`pAttr->nIdx = pMemObj->nIdx`). Losing it pointed every native class`` |
|        - |   42 | `	 * constant at slot 0, which reads as $GLOBALS. The instance-property path was` |
|        - |   43 | `	 * unaffected (it records the index before calling this), which is why nothing` |
|        - |   44 | `	 * saw it until the date family declared the first native constants. */` |
|  9921341 |   45 | `	sxu32 nSlot = pOut->nIdx;` |
|  9921341 |   46 | `	switch( pLit->iType ){` |
|  1251801 |   47 | `		case PH7_NATIVE_VAL_INT:` |
|  2503606 |   48 | `			PH7_MemObjInitFromInt(&(*pVm),pOut,pLit->iValue);` |
|  2503606 |   49 | `			break;` |
|     7512 |   50 | `		case PH7_NATIVE_VAL_BOOL:` |
|    15029 |   51 | `			PH7_MemObjInitFromBool(&(*pVm),pOut,(sxi32)pLit->iValue);` |
|    15029 |   52 | `			break;` |
|  2201633 |   53 | `		case PH7_NATIVE_VAL_STRING: {` |
|        - |   54 | `			SyString sLit;` |
|  4403268 |   55 | `			SyStringInitFromBuf(&sLit,pLit->zValue,SyStrlen(pLit->zValue));` |
|  4403268 |   56 | `			PH7_MemObjInitFromString(&(*pVm),pOut,&sLit);` |
|  4403268 |   57 | `			break;` |
|        - |   58 | `		}` |
|        - |   59 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      227 |   60 | `		case PH7_NATIVE_VAL_DOUBLE:` |
|      457 |   61 | `			PH7_MemObjInitFromReal(&(*pVm),pOut,pLit->rValue);` |
|      457 |   62 | `			break;` |
|        - |   63 | `#endif` |
|   731843 |   64 | `		case PH7_NATIVE_VAL_ARRAY: {` |
|        - |   65 | ``			/* The empty array — php's `private array $trace = [];`. Each instance`` |
|        - |   66 | `			 * needs its OWN map (the exception's trace is written per throw), so` |
|        - |   67 | `			 * this allocates rather than sharing one. */` |
|  1463690 |   68 | `			ph7_hashmap *pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|  1463690 |   69 | `			if( pMap == 0 ){` |
|      ! 0 |   70 | `				PH7_MemObjInit(&(*pVm),pOut);` |
|      ! 0 |   71 | `			}else{` |
|  1463690 |   72 | `				PH7_MemObjInitFromArray(&(*pVm),pOut,pMap);` |
|        - |   73 | `			}` |
|  1463690 |   74 | `			break;` |
|        - |   75 | `		}` |
|   767655 |   76 | `		default:` |
|  1535314 |   77 | `			PH7_MemObjInit(&(*pVm),pOut);` |
|  1535309 |   78 | `			break;` |
|        - |   79 | `	}` |
|  9921341 |   80 | `	pOut->nIdx = nSlot;` |
|  9921341 |   81 | `}` |
|        - |   82 | `/*` |
|        - |   83 | ` * Write a declared property of an instance from C.` |
|        - |   84 | ` *` |
|        - |   85 | ` * Every native class that hands an OBJECT back to PHP has to fill one in, and the` |
|        - |   86 | ` * write has to go through the instance's own slot table (hAttr -> VmClassAttr ->` |
|        - |   87 | ` * pVm->aMemObj) rather than the class declaration, or the value lands nowhere.` |
|        - |   88 | ` * Silently does nothing for a name the class does not declare -- callers pass` |
|        - |   89 | ` * literals from their own spec table, so a miss is a build error, not input.` |
|        - |   90 | ` */` |
|     7630 |   91 | `PH7_PRIVATE void PH7_NativeSetProp(ph7_vm *pVm,ph7_class_instance *pObj,` |
|        - |   92 | `	const char *zProp,sxu32 nProp,ph7_value *pSrcVal)` |
|        5 |   93 | `{` |
|        - |   94 | `	SyHashEntry *pEntry;` |
|        - |   95 | `	VmClassAttr *pVmAttr;` |
|        - |   96 | `	ph7_value *pSlot;` |
|     7635 |   97 | `	PH7_NativeMaterializeLazy(&(*pVm),pObj);` |
|     7635 |   98 | `	pEntry = SyHashGet(&pObj->hAttr,(const void *)zProp,nProp);` |
|     7635 |   99 | `	if( pEntry == 0 ){` |
|      ! 0 |  100 | `		return;` |
|        - |  101 | `	}` |
|     7635 |  102 | `	pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     7635 |  103 | `	pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,pVmAttr->nIdx);` |
|     7635 |  104 | `	if( pSlot == 0 ){` |
|      ! 0 |  105 | `		return;` |
|        - |  106 | `	}` |
|     7635 |  107 | `	PH7_MemObjStore(pSrcVal,pSlot);` |
|     7635 |  108 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|     3820 |  109 | `}` |
|        - |  110 | `/*` |
|        - |  111 | ` * ---------------------------------------------------------------------------` |
|        - |  112 | ` * Reading and writing a native instance's own declared slots.` |
|        - |  113 | ` *` |
|        - |  114 | `` * A compiled method reaches `$this->p` through the byte-code that resolves the`` |
|        - |  115 | ` * attribute; a C body has to walk the instance's slot table itself. Every native` |
|        - |  116 | ` * class needs the same six or seven moves, so they live here rather than being` |
|        - |  117 | ` * re-declared per subsystem (the date family carried a private copy of the whole` |
|        - |  118 | ` * set, which is what these replace).` |
|        - |  119 | ` * ---------------------------------------------------------------------------` |
|        - |  120 | ` */` |
|        - |  121 | `/* Fetch a declared INSTANCE slot by name (never a static or a constant). */` |
|  1742158 |  122 | `PH7_PRIVATE ph7_value * PH7_NativeAttr(ph7_class_instance *pObj,const char *zName)` |
|        5 |  123 | `{` |
|        - |  124 | `	SyString sName;` |
|  1742163 |  125 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|  1742163 |  126 | `	return PH7_ClassInstanceFetchAttr(pObj,&sName);` |
|        5 |  127 | `}` |
|        - |  128 | `/*` |
|        - |  129 | ` * Has this declared slot never been written? A PH7_NATIVE_VAL_NONE property is` |
|        - |  130 | `` * php's `public int $id;` — typed, with no default — and reading one before the`` |
|        - |  131 | ` * class has filled it is php's "must not be accessed before initialization".` |
|        - |  132 | ` * The property-read opcode raises that itself; a C body reading the slot` |
|        - |  133 | ` * directly has to ask.` |
|        - |  134 | ` */` |
|       40 |  135 | `PH7_PRIVATE int PH7_NativeAttrIsUninit(ph7_class_instance *pObj,const char *zName)` |
|        1 |  136 | `{` |
|       41 |  137 | `	SyHashEntry *pEntry = pObj ? SyHashGet(&pObj->hAttr,(const void *)zName,SyStrlen(zName)) : 0;` |
|       41 |  138 | `	return pEntry != 0` |
|       40 |  139 | `		&& (((VmClassAttr *)pEntry->pUserData)->iState & VM_CLASS_ATTR_UNINIT) != 0;` |
|        1 |  140 | `}` |
|        - |  141 | `/* Read an int slot WITHOUT converting it: ph7_value_to_int64() converts the` |
|        - |  142 | ` * attribute in place, which would rewrite the object's own state. */` |
|    59649 |  143 | `PH7_PRIVATE sxi64 PH7_NativeAttrInt(ph7_class_instance *pObj,const char *zName)` |
|        5 |  144 | `{` |
|    59654 |  145 | `	ph7_value *pVal = PH7_NativeAttr(pObj,zName);` |
|    59654 |  146 | `	if( pVal && (pVal->iFlags & MEMOBJ_INT) ){` |
|    59654 |  147 | `		return pVal->x.iVal;` |
|        - |  148 | `	}` |
|      ! 0 |  149 | `	return 0;` |
|    29832 |  150 | `}` |
|        - |  151 | `/* Borrow a string slot's bytes (empty when it holds anything else). */` |
|    18442 |  152 | `PH7_PRIVATE void PH7_NativeAttrStr(ph7_class_instance *pObj,const char *zName,` |
|        - |  153 | `	const char **pzOut,int *pnOut)` |
|        5 |  154 | `{` |
|    18447 |  155 | `	ph7_value *pVal = PH7_NativeAttr(pObj,zName);` |
|    18447 |  156 | `	*pzOut = "";` |
|    18447 |  157 | `	*pnOut = 0;` |
|    18447 |  158 | `	if( pVal && (pVal->iFlags & MEMOBJ_STRING) ){` |
|    18437 |  159 | `		*pzOut = (const char *)SyBlobData(&pVal->sBlob);` |
|    18437 |  160 | `		*pnOut = (int)SyBlobLength(&pVal->sBlob);` |
|     9216 |  161 | `	}` |
|    18447 |  162 | `}` |
|        - |  163 | `/* The object stored in a slot, or NULL when it holds anything else. */` |
|    36492 |  164 | `PH7_PRIVATE ph7_class_instance * PH7_NativeAttrObj(ph7_class_instance *pObj,const char *zName)` |
|        5 |  165 | `{` |
|    36497 |  166 | `	ph7_value *pVal = PH7_NativeAttr(pObj,zName);` |
|    36497 |  167 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     4447 |  168 | `		return 0;` |
|        - |  169 | `	}` |
|    32055 |  170 | `	return (ph7_class_instance *)pVal->x.pOther;` |
|    18251 |  171 | `}` |
|        - |  172 | `/* Truth of a bool/int slot, again without converting it. */` |
|    12730 |  173 | `PH7_PRIVATE int PH7_NativeAttrTruthy(ph7_class_instance *pObj,const char *zName)` |
|        5 |  174 | `{` |
|    12735 |  175 | `	ph7_value *pVal = PH7_NativeAttr(pObj,zName);` |
|    12735 |  176 | `	if( pVal && (pVal->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|    12735 |  177 | `		return pVal->x.iVal != 0;` |
|        - |  178 | `	}` |
|      ! 0 |  179 | `	return 0;` |
|     6370 |  180 | `}` |
|        - |  181 | `/*` |
|        - |  182 | ` * Clear the not-yet-initialized mark a TYPED slot without a default carries.` |
|        - |  183 | ` *` |
|        - |  184 | ` * There are two families of writer here: PH7_NativeSetProp, which looks the` |
|        - |  185 | ` * VmClassAttr up and clears the bit, and the four typed shortcuts below, which` |
|        - |  186 | ` * write the ph7_value through PH7_NativeAttr and used to leave it set. That was` |
|        - |  187 | ` * invisible while nothing native declared a default-less typed property; the` |
|        - |  188 | `` * moment `public string $name` arrived on the reflectors, every one of them`` |
|        - |  189 | ` * threw "must not be accessed before initialization" from a constructor that HAD` |
|        - |  190 | ` * written the slot. Rule 44's family: the bookkeeping has to live with the write,` |
|        - |  191 | ` * not with one of the two ways of writing.` |
|        - |  192 | ` */` |
|  1535012 |  193 | `static void NativeAttrMarkInit(ph7_class_instance *pObj,const char *zName)` |
|        5 |  194 | `{` |
|  1535017 |  195 | `	SyHashEntry *pEntry = SyHashGet(&pObj->hAttr,(const void *)zName,SyStrlen(zName));` |
|  1535017 |  196 | `	if( pEntry ){` |
|  1535017 |  197 | `		((VmClassAttr *)pEntry->pUserData)->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|   767515 |  198 | `	}` |
|  1535017 |  199 | `}` |
|        - |  200 | `/*` |
|        - |  201 | ` * The slot fetch on the WRITE side. A class whose php-visible properties are LAZY` |
|        - |  202 | ` * has none of them on the object until a C body fills one, and that first write is` |
|        - |  203 | ` * what installs the set -- which is php's constructor writing its struct into the` |
|        - |  204 | ` * property table. Every native writer goes through here so the bookkeeping lives` |
|        - |  205 | ` * with the write rather than with one of the ways of writing.` |
|        - |  206 | ` */` |
|  1535012 |  207 | `static ph7_value * NativeAttrForWrite(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName)` |
|        5 |  208 | `{` |
|        - |  209 | `	ph7_value *pSlot;` |
|  1535017 |  210 | `	PH7_NativeMaterializeLazy(&(*pVm),pObj);` |
|  1535017 |  211 | `	pSlot = PH7_NativeAttr(pObj,zName);` |
|  1535017 |  212 | `	if( pSlot == 0 && pObj ){` |
|        - |  213 | `		/* An ON-DEMAND property is installed by the write that names it and by` |
|        - |  214 | `		 * nothing else, so an object nobody wrote one on does not carry the name` |
|        - |  215 | ``		 * (php's `date_string`, which exists on a from-string DateInterval alone). */`` |
|      124 |  216 | `		SyHashEntry *pEntry = SyHashGet(&pObj->pClass->hAttr,zName,(sxu32)SyStrlen(zName));` |
|      124 |  217 | `		if( pEntry ){` |
|      124 |  218 | `			ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      124 |  219 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_ONDEMAND ){` |
|      124 |  220 | `				VmClassAttr *pVmAttr = 0;` |
|      124 |  221 | `				VmRecreateDeclaredAttr(&(*pVm),pObj,pAttr,&pVmAttr);` |
|      124 |  222 | `				if( pVmAttr ){` |
|      124 |  223 | `					pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      124 |  224 | `					pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,pVmAttr->nIdx);` |
|       61 |  225 | `				}` |
|       61 |  226 | `			}` |
|       61 |  227 | `		}` |
|       61 |  228 | `	}` |
|  1535017 |  229 | `	return pSlot;` |
|        5 |  230 | `}` |
|        - |  231 | `/*` |
|        - |  232 | ` * Keep one of this OBJECT's slots out of every surface that shows it, while it` |
|        - |  233 | ` * goes on reading, writing and answering isset() as it did` |
|        - |  234 | ` * (VM_CLASS_ATTR_UNSEEN).` |
|        - |  235 | ` */` |
|     1098 |  236 | `PH7_PRIVATE void PH7_NativeHideAttr(ph7_class_instance *pObj,const char *zName)` |
|        2 |  237 | `{` |
|        - |  238 | `	SyHashEntry *pEntry;` |
|     1100 |  239 | `	if( pObj == 0 ){` |
|      ! 0 |  240 | `		return;` |
|        - |  241 | `	}` |
|     1100 |  242 | `	pEntry = SyHashGet(&pObj->hAttr,zName,(sxu32)SyStrlen(zName));` |
|     1100 |  243 | `	if( pEntry ){` |
|     1100 |  244 | `		((VmClassAttr *)pEntry->pUserData)->iState \|= VM_CLASS_ATTR_UNSEEN;` |
|      549 |  245 | `	}` |
|      551 |  246 | `}` |
|    38166 |  247 | `PH7_PRIVATE void PH7_NativeSetAttrInt(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,sxi64 iVal)` |
|        5 |  248 | `{` |
|    38171 |  249 | `	ph7_value *pSlot = NativeAttrForWrite(&(*pVm),pObj,zName);` |
|        - |  250 | `	ph7_value sVal;` |
|    38171 |  251 | `	if( pSlot == 0 ){` |
|      ! 0 |  252 | `		return;` |
|        - |  253 | `	}` |
|    38171 |  254 | `	PH7_MemObjInitFromInt(&(*pVm),&sVal,iVal);` |
|    38171 |  255 | `	PH7_MemObjStore(&sVal,pSlot);` |
|    38171 |  256 | `	PH7_MemObjRelease(&sVal);` |
|    38171 |  257 | `	NativeAttrMarkInit(pObj,zName);` |
|    19089 |  258 | `}` |
|        - |  259 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      454 |  260 | `PH7_PRIVATE void PH7_NativeSetAttrReal(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,ph7_real rVal)` |
|        3 |  261 | `{` |
|      457 |  262 | `	ph7_value *pSlot = NativeAttrForWrite(&(*pVm),pObj,zName);` |
|        - |  263 | `	ph7_value sVal;` |
|      457 |  264 | `	if( pSlot == 0 ){` |
|      ! 0 |  265 | `		return;` |
|        - |  266 | `	}` |
|      457 |  267 | `	PH7_MemObjInitFromReal(&(*pVm),&sVal,rVal);` |
|      457 |  268 | `	PH7_MemObjStore(&sVal,pSlot);` |
|      457 |  269 | `	PH7_MemObjRelease(&sVal);` |
|      457 |  270 | `	NativeAttrMarkInit(pObj,zName);` |
|      230 |  271 | `}` |
|        - |  272 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|  1482328 |  273 | `PH7_PRIVATE void PH7_NativeSetAttrStr(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,` |
|        - |  274 | `	const char *zVal,int nVal)` |
|        5 |  275 | `{` |
|  1482333 |  276 | `	ph7_value *pSlot = NativeAttrForWrite(&(*pVm),pObj,zName);` |
|        - |  277 | `	ph7_value sVal;` |
|        - |  278 | `	SyString sStr;` |
|  1482333 |  279 | `	if( pSlot == 0 ){` |
|      ! 0 |  280 | `		return;` |
|        - |  281 | `	}` |
|  1482333 |  282 | `	SyStringInitFromBuf(&sStr,zVal,nVal);` |
|  1482333 |  283 | `	PH7_MemObjInitFromString(&(*pVm),&sVal,&sStr);` |
|  1482333 |  284 | `	PH7_MemObjStore(&sVal,pSlot);` |
|  1482333 |  285 | `	PH7_MemObjRelease(&sVal);	NativeAttrMarkInit(pObj,zName);` |
|   741177 |  286 | `}` |
|     3612 |  287 | `PH7_PRIVATE void PH7_NativeSetAttrBool(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,int bVal)` |
|        5 |  288 | `{` |
|     3617 |  289 | `	ph7_value *pSlot = NativeAttrForWrite(&(*pVm),pObj,zName);` |
|        - |  290 | `	ph7_value sVal;` |
|     3617 |  291 | `	if( pSlot == 0 ){` |
|      ! 0 |  292 | `		return;` |
|        - |  293 | `	}` |
|     3617 |  294 | `	PH7_MemObjInitFromBool(&(*pVm),&sVal,bVal);` |
|     3617 |  295 | `	PH7_MemObjStore(&sVal,pSlot);` |
|     3617 |  296 | `	PH7_MemObjRelease(&sVal);	NativeAttrMarkInit(pObj,zName);` |
|     1811 |  297 | `}` |
|        - |  298 | `/* Store an object in a slot, or NULL to clear it. PH7_MemObjStore takes the` |
|        - |  299 | ` * reference the slot needs, so the temp never holds one of its own. */` |
|    10452 |  300 | `PH7_PRIVATE void PH7_NativeSetAttrObj(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,` |
|        - |  301 | `	ph7_class_instance *pVal)` |
|        5 |  302 | `{` |
|    10457 |  303 | `	ph7_value *pSlot = NativeAttrForWrite(&(*pVm),pObj,zName);` |
|        - |  304 | `	ph7_value sVal;` |
|    10457 |  305 | `	if( pSlot == 0 ){` |
|      ! 0 |  306 | `		return;` |
|        - |  307 | `	}` |
|    10457 |  308 | `	PH7_MemObjInit(&(*pVm),&sVal);` |
|    10457 |  309 | `	if( pVal ){` |
|     9821 |  310 | `		sVal.x.pOther = pVal;` |
|     9821 |  311 | `		sVal.iFlags = MEMOBJ_OBJ;` |
|     4908 |  312 | `	}` |
|    10457 |  313 | `	PH7_MemObjStore(&sVal,pSlot);` |
|    10457 |  314 | `	NativeAttrMarkInit(pObj,zName);` |
|     5231 |  315 | `}` |
|        - |  316 | `/*` |
|        - |  317 | ` * Hand an instance back as a native call's result, dropping the reference` |
|        - |  318 | ` * PH7_NewClassInstance/PH7_CloneClassInstance handed the caller.` |
|        - |  319 | ` */` |
|     4198 |  320 | `PH7_PRIVATE void PH7_NativeResultObject(ph7_context *pCtx,ph7_class_instance *pObj)` |
|        5 |  321 | `{` |
|        - |  322 | `	ph7_value sRes;` |
|     4203 |  323 | `	PH7_MemObjInit(pCtx->pVm,&sRes);` |
|     4203 |  324 | `	sRes.x.pOther = pObj;` |
|     4203 |  325 | `	sRes.iFlags = MEMOBJ_OBJ;` |
|     4203 |  326 | `	ph7_result_value(pCtx,&sRes);   /* takes its own reference */` |
|     4203 |  327 | `	PH7_ClassInstanceUnref(pObj);` |
|     4203 |  328 | `}` |
|        - |  329 | `/*` |
|        - |  330 | ` * Resolve a class by name for the builder's own use (parents and interfaces).` |
|        - |  331 | ` * Autoload is deliberately NOT triggered: these run at VM init, where the only` |
|        - |  332 | ` * classes that can exist are the ones installed before this call, and a missing` |
|        - |  333 | ` * name is a build-order bug in the engine, not a userland lookup.` |
|        - |  334 | ` */` |
|  1015980 |  335 | `static ph7_class * NativeLookupClass(ph7_vm *pVm,const char *zName)` |
|        5 |  336 | `{` |
|  1015985 |  337 | `	return PH7_VmExtractClass(&(*pVm),zName,(sxu32)SyStrlen(zName),FALSE,0);` |
|        5 |  338 | `}` |
|        - |  339 | `/*` |
|        - |  340 | ` * Translate the builder's PH7_MOD_* modifier bits into the protection level and` |
|        - |  341 | ` * the attribute/method flag word the class structures actually store.` |
|        - |  342 | ` */` |
|  9402338 |  343 | `static sxi32 NativeProtection(sxi32 iMods)` |
|        5 |  344 | `{` |
|  9402343 |  345 | `	if( iMods & PH7_MOD_PRIVATE ){` |
|   786385 |  346 | `		return PH7_CLASS_PROT_PRIVATE;` |
|        - |  347 | `	}` |
|  8615963 |  348 | `	if( iMods & PH7_MOD_PROTECTED ){` |
|   177945 |  349 | `		return PH7_CLASS_PROT_PROTECTED;` |
|        - |  350 | `	}` |
|  8438023 |  351 | `	return PH7_CLASS_PROT_PUBLIC;` |
|  4701174 |  352 | `}` |
|        - |  353 | `/*` |
|        - |  354 | ` * Attach one C-bodied method to an already-created class.` |
|        - |  355 | ` *` |
|        - |  356 | ` * The ph7_user_func built here is NOT registered in pVm->hHostFunction: it hangs` |
|        - |  357 | ` * off the method and is reachable only by dispatching the method, which is exactly` |
|        - |  358 | ` * the property that retires the global thunks. Its sName is the php-facing` |
|        - |  359 | `` * `Class::method`, so an ArgumentCountError raised at the OP_CALL choke point`` |
|        - |  360 | ` * names what a php user would recognise.` |
|        - |  361 | ` */` |
|  6750458 |  362 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallMethod(` |
|        - |  363 | `	ph7_vm *pVm,` |
|        - |  364 | `	ph7_class *pClass,` |
|        - |  365 | `	const PH7_NativeMethodDef *pDef,` |
|        - |  366 | `	void *pUserData` |
|        - |  367 | `	)` |
|        5 |  368 | `{` |
|        - |  369 | `	ph7_class_method *pMeth;` |
|        - |  370 | `	ph7_user_func *pNative;` |
|        - |  371 | `	SyString sName;` |
|        - |  372 | `	SyString sVmName;` |
|        - |  373 | `	char zQual[128];` |
|        - |  374 | `	sxi32 iFuncFlags;` |
|        - |  375 | `	sxi32 rc;` |
|  6750463 |  376 | `	SyStringInitFromBuf(&sName,pDef->zName,SyStrlen(pDef->zName));` |
|  6750463 |  377 | `	iFuncFlags = VM_FUNC_NATIVE;` |
|  6750463 |  378 | `	if( pVm->bCompilingBuiltin ){` |
|        - |  379 | `		/* Same stamp the embedded chunks got, so Reflection keeps reporting these` |
|        - |  380 | `		 * as internal: isInternal() true, getFileName() false. */` |
|  6750245 |  381 | `		iFuncFlags \|= VM_FUNC_INTERNAL;` |
|  3375120 |  382 | `	}` |
| 10125692 |  383 | `	pMeth = PH7_NewClassMethod(&(*pVm),pClass,&sName,0,` |
|  6750458 |  384 | `		NativeProtection(pDef->iMods),` |
|  6750458 |  385 | `		((pDef->iMods & PH7_MOD_FINAL) ? PH7_CLASS_ATTR_FINAL : 0)` |
|  6750458 |  386 | `		\| ((pDef->iMods & PH7_MOD_ABSTRACT) ? PH7_CLASS_ATTR_ABSTRACT : 0),` |
|  3375229 |  387 | `		iFuncFlags);` |
|  6750463 |  388 | `	if( pMeth == 0 ){` |
|      ! 0 |  389 | `		return SXERR_MEM;` |
|        - |  390 | `	}` |
|        - |  391 | `	/* Staticness has to be recorded in BOTH places: on the method (where the class` |
|        - |  392 | `	 * machinery and Reflection read it) and on the function (where the OP_CALL` |
|        - |  393 | `	 * dispatcher reads it, to keep the caller's $this from leaking in as a receiver` |
|        - |  394 | `	 * — see VM_FUNC_NATIVE_STATIC). */` |
|  6750463 |  395 | `	if( pDef->iMods & PH7_MOD_STATIC ){` |
|   229823 |  396 | `		pMeth->iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|   229823 |  397 | `		pMeth->sFunc.iFlags \|= VM_FUNC_NATIVE_STATIC;` |
|   114909 |  398 | `	}` |
|        - |  399 | `	/* An ABSTRACT method has no body to install — an INTERFACE's methods are the` |
|        - |  400 | ``	 * reason the builder needs the case at all (`SeekableIterator::seek()`), and`` |
|        - |  401 | `	 * php reports them with their declared signature like any other. The dispatch` |
|        - |  402 | `	 * never reaches one: OP_CALL and OP_MEMBER both refuse an abstract method` |
|        - |  403 | `	 * ("Cannot call abstract method C::m()") before they look at a body. */` |
|  6750463 |  404 | `	if( pDef->iMods & PH7_MOD_ABSTRACT ){` |
|   298485 |  405 | `		if( pDef->zSig ){` |
|   298485 |  406 | `			rc = PH7_NewForeignFunction(&(*pVm),&sName,pDef->xFunc,pUserData,&pNative);` |
|   298485 |  407 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  408 | `				return rc;` |
|        - |  409 | `			}` |
|   298485 |  410 | `			pNative->zSig = pDef->zSig;` |
|   298485 |  411 | `			if( pDef->zRet && pDef->zRet[0] ){` |
|   206645 |  412 | `				pNative->zRet = pDef->zRet;` |
|   103320 |  413 | `			}` |
|   298485 |  414 | `			pMeth->sFunc.pNative = pNative;` |
|   149240 |  415 | `		}` |
|   298485 |  416 | `		return PH7_ClassInstallMethod(pClass,pMeth);` |
|        - |  417 | `	}` |
|        - |  418 | `	/* The C body. The diagnostic name is the qualified one php would print. */` |
|  6451983 |  419 | `	SyBufferFormat(zQual,sizeof(zQual),"%z::%s",&pClass->sName,pDef->zName);` |
|  6451983 |  420 | `	SyStringInitFromBuf(&sVmName,zQual,SyStrlen(zQual));` |
|  6451983 |  421 | `	rc = PH7_NewForeignFunction(&(*pVm),&sVmName,pDef->xFunc,pUserData,&pNative);` |
|  6451983 |  422 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  423 | `		return rc;` |
|        - |  424 | `	}` |
|        - |  425 | `	/* Arity bounds and by-ref positions come from the declared signature, exactly` |
|        - |  426 | `	 * as they do for a builtin (VmSetBuiltinSignatures) -- one string is the single` |
|        - |  427 | `	 * source of truth, and it doubles as the Reflection parameter list.` |
|        - |  428 | `	 *` |
|        - |  429 | `	 * NULL and "" mean different things here, and the difference is load-bearing.` |
|        - |  430 | `	 * A builtin without a row in aBuiltinSig[] is simply undescribed, so it stays` |
|        - |  431 | `	 * UNENFORCED (the SyZero'd bHasMaxArg == 0 reads as "unchecked", never as` |
|        - |  432 | `	 * "accepts at most zero"). A native method, by contrast, always states its` |
|        - |  433 | `	 * signature deliberately, so "" is a positive declaration of ZERO parameters` |
|        - |  434 | ``	 * and must enforce a maximum of zero -- otherwise `$gen->current(1)` and`` |
|        - |  435 | ``	 * `$fiber->isStarted(1)` are swallowed where php raises "expects exactly 0`` |
|        - |  436 | `	 * arguments, 1 given". VmDeriveArityFromSig("") already answers min 0 / max 0 /` |
|        - |  437 | `	 * enforced; only NULL opts out. */` |
|  6451983 |  438 | `	if( pDef->zSig ){` |
|  6446243 |  439 | `		sxi16 nMin = 0, nMax = 0;` |
|  6446243 |  440 | `		sxu8 bAtLeast = 0, bHasMax = 0;` |
|  6446243 |  441 | `		pNative->zSig = pDef->zSig;` |
|  6446243 |  442 | `		pNative->nByRefMask = VmDeriveByRefMaskFromSig(pDef->zSig);` |
|  6446243 |  443 | `		VmDeriveArityFromSig(pDef->zSig,&nMin,&bAtLeast,&nMax,&bHasMax);` |
|  6446243 |  444 | `		pNative->nMinArg = nMin;` |
|  6446243 |  445 | `		pNative->bAtLeast = bAtLeast;` |
|  6446243 |  446 | `		pNative->nMaxArg = nMax;` |
|  6446243 |  447 | `		pNative->bHasMaxArg = bHasMax;` |
|  3223119 |  448 | `	}` |
|  6451983 |  449 | `	if( pDef->zRet && pDef->zRet[0] ){` |
|  5659863 |  450 | `		pNative->zRet = pDef->zRet;` |
|  2829929 |  451 | `	}` |
|  6451983 |  452 | `	pMeth->sFunc.pNative = pNative;` |
|  6451983 |  453 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  6451983 |  454 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  455 | `		return rc;` |
|        - |  456 | `	}` |
|  6451983 |  457 | `	if( pClass->bMounted ){` |
|        - |  458 | `		/* The class is already live (a method attached after installation): mount` |
|        - |  459 | `		 * this one method now, since VmMountUserClass will not run again. */` |
|      ! 0 |  460 | `		return PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|        - |  461 | `	}` |
|  6451983 |  462 | `	return SXRET_OK;` |
|  3375234 |  463 | `}` |
|        - |  464 | `/*` |
|        - |  465 | ` * Install one class constant carrying a scalar value.` |
|        - |  466 | ` *` |
|        - |  467 | ` * A declared default is normally a compiled initializer (ph7_class_attr::aByteCode)` |
|        - |  468 | ` * evaluated at mount. The builder has no compiler to hand, so it evaluates the` |
|        - |  469 | ` * value NOW into the constant's reserved slot and leaves the byte-code empty --` |
|        - |  470 | ` * which is what a literal initializer would have produced anyway.` |
|        - |  471 | ` */` |
|  1199660 |  472 | `static sxi32 NativeInstallConstant(ph7_vm *pVm,ph7_class *pClass,const PH7_NativeConstDef *pDef)` |
|        5 |  473 | `{` |
|        - |  474 | `	ph7_class_attr *pAttr;` |
|        - |  475 | `	SyString sName;` |
|  1199665 |  476 | `	SyStringInitFromBuf(&sName,pDef->zName,SyStrlen(pDef->zName));` |
|  1199665 |  477 | `	pAttr = PH7_NewClassAttr(&(*pVm),&sName,0,NativeProtection(pDef->iMods),` |
|        - |  478 | `		PH7_CLASS_ATTR_CONSTANT);` |
|  1199665 |  479 | `	if( pAttr == 0 ){` |
|      ! 0 |  480 | `		return SXERR_MEM;` |
|        - |  481 | `	}` |
|  1199665 |  482 | `	pAttr->pDeclClass = pClass;` |
|        - |  483 | `	/* Stash the literal; VmMountUserClassAttrs materializes it into the slot. */` |
|  1199665 |  484 | `	pAttr->pNativeValue = pDef;` |
|  1199665 |  485 | `	return PH7_ClassInstallAttr(pClass,pAttr);` |
|   599835 |  486 | `}` |
|        - |  487 | `/*` |
|        - |  488 | ` * Fill in a declared property TYPE from the text a spec row states, exactly as` |
|        - |  489 | ` * GenStateCopyTypeToAttr fills it from a parsed declaration: nType is the` |
|        - |  490 | ` * MEMOBJ_* the atom names (SXU32_HIGH plus sClass for a class name, which is` |
|        - |  491 | `` * also how the compiler carries `mixed` and `iterable`), the `?` becomes the`` |
|        - |  492 | ` * NULLABLE flag rather than a type bit, and sTypeName keeps the text VERBATIM --` |
|        - |  493 | ` * both the TypeError and Reflection print what the declaration said.` |
|        - |  494 | ` *` |
|        - |  495 | ` * Single atoms only. A union needs the alternative SET the compiler builds, and` |
|        - |  496 | ` * nothing native declares one; a spec that tries reads as the class name it is` |
|        - |  497 | ` * spelled with, which is why the parse stays this literal.` |
|        - |  498 | ` */` |
|   424760 |  499 | `static void NativeAttrType(ph7_class_attr *pAttr,const char *zType)` |
|        5 |  500 | `{` |
|        - |  501 | `	static const struct { const char *zName; sxu32 nType; } aScalar[] = {` |
|        - |  502 | `		{ "int",    MEMOBJ_INT },` |
|        - |  503 | `		{ "float",  MEMOBJ_REAL },` |
|        - |  504 | `		{ "string", MEMOBJ_STRING },` |
|        - |  505 | `		{ "bool",   MEMOBJ_BOOL },` |
|        - |  506 | `		{ "array",  MEMOBJ_HASHMAP },` |
|        - |  507 | `		{ "object", MEMOBJ_OBJ },` |
|        - |  508 | `	};` |
|   424765 |  509 | `	const char *zAtom = zType;` |
|        - |  510 | `	sxu32 nAtom, n;` |
|   424765 |  511 | `	if( zAtom[0] == '?' ){` |
|    63145 |  512 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|    63145 |  513 | `		zAtom++;` |
|    31570 |  514 | `	}` |
|   424765 |  515 | `	nAtom = SyStrlen(zAtom);` |
|   424765 |  516 | `	pAttr->iFlags \|= PH7_CLASS_ATTR_TYPED;` |
|   424765 |  517 | `	SyStringInitFromBuf(&pAttr->sTypeName,zType,SyStrlen(zType));` |
|  1435005 |  518 | `	for( n = 0 ; n < SX_ARRAYSIZE(aScalar) ; ++n ){` |
|  1377600 |  519 | `		if( nAtom == SyStrlen(aScalar[n].zName)` |
|   895445 |  520 | `		 && SyMemcmp(zAtom,aScalar[n].zName,(sxu32)nAtom) == 0 ){` |
|   367365 |  521 | `			pAttr->nType = aScalar[n].nType;` |
|   367365 |  522 | `			return;` |
|        - |  523 | `		}` |
|   505125 |  524 | `	}` |
|    57405 |  525 | `	pAttr->nType = SXU32_HIGH;` |
|    57405 |  526 | `	SyStringInitFromBuf(&pAttr->sClass,zAtom,nAtom);` |
|   212385 |  527 | `}` |
|        - |  528 | `/*` |
|        - |  529 | ` * Install one declared property.` |
|        - |  530 | ` *` |
|        - |  531 | ` * The default is carried as a literal rather than compiled byte-code (see` |
|        - |  532 | `` * PH7_NativeLiteralValue); an instance property materializes it at `new`, a static`` |
|        - |  533 | `` * one at mount. A PH7_NATIVE_VAL_NULL default is the plain `public $p;` case.`` |
|        - |  534 | ` */` |
|  1452220 |  535 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallProperty(ph7_vm *pVm,ph7_class *pClass,` |
|        - |  536 | `	const PH7_NativePropDef *pDef)` |
|        5 |  537 | `{` |
|        - |  538 | `	ph7_class_attr *pAttr;` |
|        - |  539 | `	SyString sName;` |
|  1452225 |  540 | `	sxi32 iFlags = 0;` |
|  1452225 |  541 | `	SyStringInitFromBuf(&sName,pDef->zName,SyStrlen(pDef->zName));` |
|  1452225 |  542 | `	if( pDef->iMods & PH7_MOD_STATIC ){` |
|      ! 0 |  543 | `		iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|      ! 0 |  544 | `	}` |
|  1452225 |  545 | `	if( pDef->iMods & PH7_MOD_HIDDEN ){` |
|   918405 |  546 | `		iFlags \|= PH7_CLASS_ATTR_HIDDEN;` |
|   459200 |  547 | `	}` |
|  1452225 |  548 | `	if( pDef->iMods & PH7_MOD_ONDEMAND ){` |
|     5745 |  549 | `		iFlags \|= PH7_CLASS_ATTR_NATIVE_ONDEMAND;` |
|     2870 |  550 | `	}` |
|        - |  551 | `	/* php declares several native slots readonly and asymmetrically visible` |
|        - |  552 | ``	 * (`public protected(set) readonly string $path` on Directory), and both are`` |
|        - |  553 | `	 * php-visible twice over: the write refusal and Reflection's modifier list. */` |
|  1452225 |  554 | `	if( pDef->iMods & PH7_MOD_READONLY ){` |
|    63145 |  555 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|    31570 |  556 | `	}` |
|  1452225 |  557 | `	if( pDef->iMods & PH7_MOD_PROT_SET ){` |
|    51665 |  558 | `		iFlags \|= PH7_CLASS_ATTR_PROTECTED_SET;` |
|    25830 |  559 | `	}` |
|  1452225 |  560 | `	if( pDef->iMods & PH7_MOD_PRIV_SET ){` |
|      ! 0 |  561 | `		iFlags \|= PH7_CLASS_ATTR_PRIVATE_SET;` |
|      ! 0 |  562 | `	}` |
|  1452225 |  563 | `	pAttr = PH7_NewClassAttr(&(*pVm),&sName,0,NativeProtection(pDef->iMods),iFlags);` |
|  1452225 |  564 | `	if( pAttr == 0 ){` |
|      ! 0 |  565 | `		return SXERR_MEM;` |
|        - |  566 | `	}` |
|  1452225 |  567 | `	pAttr->pDeclClass = pClass;` |
|        - |  568 | `	/* NO default is not the same as a NULL one, and the difference is php-visible:` |
|        - |  569 | ``	 * a typed slot without a default is UNINITIALIZED at `new` (reading it before`` |
|        - |  570 | `	 * the class writes it is an Error, hasDefaultValue() is false), which is what` |
|        - |  571 | `	 * php declares for LibXMLError's six fields and every reflector's $name. The` |
|        - |  572 | ``	 * machinery is already there for a compiled `public string $p;` — leaving`` |
|        - |  573 | `	 * pNativeValue at 0 is what selects it. */` |
|  1452225 |  574 | `	if( pDef->sDefault.iType != PH7_NATIVE_VAL_NONE ){` |
|  1222625 |  575 | `		pAttr->pNativeValue = &pDef->sDefault;` |
|   611310 |  576 | `	}` |
|  1452225 |  577 | `	if( pDef->zType && pDef->zType[0] ){` |
|   424765 |  578 | `		NativeAttrType(pAttr,pDef->zType);` |
|   212380 |  579 | `	}` |
|  1452225 |  580 | `	return PH7_ClassInstallAttr(pClass,pAttr);` |
|   726115 |  581 | `}` |
|        - |  582 | `/*` |
|        - |  583 | `` * Attach an `#[Attr(...)]` to something declared from C.`` |
|        - |  584 | ` *` |
|        - |  585 | `` * php declares `#[Attribute(Attribute::TARGET_CLASS)]` on Attribute itself, a`` |
|        - |  586 | `` * target mask on every other attribute class, and `#[NoDiscard(message: …)]` on`` |
|        - |  587 | ` * nine DateTimeImmutable methods — and those records are LOAD-BEARING: the` |
|        - |  588 | `` * compiler reads them to decide whether a user's `#[Deprecated]` may sit where`` |
|        - |  589 | ` * it does, the NoDiscard warning reads its message from them, and` |
|        - |  590 | ` * ReflectionAttribute answers them all. A compiled attribute holds its argument` |
|        - |  591 | ` * as byte-code; there is no compiler here, so the argument rides as the same` |
|        - |  592 | ` * literal record a native constant or property default uses and every reader` |
|        - |  593 | ` * takes that branch when the byte-code is empty.` |
|        - |  594 | ` *` |
|        - |  595 | ` * NativeBuildAttr is the shared half; the two entry points below hang the record` |
|        - |  596 | ` * on a class or on one of its methods. aArg is BORROWED, so callers state their` |
|        - |  597 | `` * rows `static const`.`` |
|        - |  598 | ` */` |
|    91840 |  599 | `static sxi32 NativeBuildAttr(ph7_vm *pVm,ph7_attribute *pAttr,const char *zAttr,` |
|        - |  600 | `	const PH7_NativeAttrArg *aArg,sxu32 nArg)` |
|        5 |  601 | `{` |
|        - |  602 | `	char *zDup;` |
|        - |  603 | `	sxu32 n;` |
|    91845 |  604 | `	SyZero(pAttr,sizeof(*pAttr));` |
|    91845 |  605 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zAttr,SyStrlen(zAttr));` |
|    91845 |  606 | `	if( zDup == 0 ){` |
|      ! 0 |  607 | `		return SXERR_MEM;` |
|        - |  608 | `	}` |
|    91845 |  609 | `	SyStringInitFromBuf(&pAttr->sName,zDup,SyStrlen(zAttr));` |
|    91845 |  610 | `	SySetInit(&pAttr->aArgs,&pVm->sAllocator,sizeof(ph7_attr_arg));` |
|   183685 |  611 | `	for( n = 0 ; n < nArg ; n++ ){` |
|        - |  612 | `		ph7_attr_arg sArgRec;` |
|    91845 |  613 | `		SyZero(&sArgRec,sizeof(sArgRec));` |
|    91845 |  614 | `		SySetInit(&sArgRec.aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    91845 |  615 | `		if( aArg[n].zName ){` |
|    51665 |  616 | `			char *zN = SyMemBackendStrDup(&pVm->sAllocator,aArg[n].zName,SyStrlen(aArg[n].zName));` |
|    51665 |  617 | `			if( zN ){` |
|    51665 |  618 | `				SyStringInitFromBuf(&sArgRec.sName,zN,SyStrlen(aArg[n].zName));` |
|    25830 |  619 | `			}` |
|    25830 |  620 | `		}` |
|        - |  621 | `		/* The literal is BORROWED, not copied: aArg must have static storage` |
|        - |  622 | ``		 * duration (every caller states its rows as `static const`). */`` |
|    91845 |  623 | `		sArgRec.pNativeValue = (const void *)&aArg[n].sValue;` |
|    91845 |  624 | `		SySetPut(&pAttr->aArgs,(const void *)&sArgRec);` |
|    45925 |  625 | `	}` |
|    91845 |  626 | `	return SXRET_OK;` |
|    45925 |  627 | `}` |
|        - |  628 | `/*` |
|        - |  629 | `` * Declare one of a native class's METHODS php 8.5's `#[\NoDiscard]`, argument`` |
|        - |  630 | ` * and all — the same record a compiled declaration carries, so Reflection` |
|        - |  631 | ` * reports the attribute and the warning reads its message from the one place a` |
|        - |  632 | ` * userland one is read from. Assigned by the owning installer after` |
|        - |  633 | ` * PH7_InstallNativeClasses, like xClone/xDim/xSet: a spec-row field would have` |
|        - |  634 | ` * to be left empty by every other table.` |
|        - |  635 | ` */` |
|    51660 |  636 | `PH7_PRIVATE sxi32 PH7_NativeMethodSetNoDiscard(ph7_vm *pVm,ph7_class *pClass,` |
|        - |  637 | `	const char *zMethod,const PH7_NativeAttrArg *aArg,sxu32 nArg)` |
|        5 |  638 | `{` |
|        - |  639 | `	ph7_class_method *pMeth;` |
|        - |  640 | `	ph7_attribute sAttr;` |
|        - |  641 | `	sxi32 rc;` |
|    51665 |  642 | `	if( pClass == 0 ){` |
|      ! 0 |  643 | `		return SXERR_NOTFOUND;` |
|        - |  644 | `	}` |
|    51665 |  645 | `	pMeth = PH7_ClassExtractMethod(pClass,zMethod,(sxu32)SyStrlen(zMethod));` |
|    51665 |  646 | `	if( pMeth == 0 ){` |
|      ! 0 |  647 | `		return SXERR_NOTFOUND;` |
|        - |  648 | `	}` |
|    51665 |  649 | `	rc = NativeBuildAttr(&(*pVm),&sAttr,"NoDiscard",aArg,nArg);` |
|    51665 |  650 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  651 | `		return rc;` |
|        - |  652 | `	}` |
|    51665 |  653 | `	pMeth->sFunc.iFlags \|= VM_FUNC_NODISCARD;` |
|    51665 |  654 | `	return SySetPut(&pMeth->sFunc.aAttrs,(const void *)&sAttr);` |
|    25835 |  655 | `}` |
|    40180 |  656 | `PH7_PRIVATE sxi32 PH7_NativeClassAddAttribute(ph7_vm *pVm,ph7_class *pClass,` |
|        - |  657 | `	const char *zAttr,const PH7_NativeAttrArg *aArg,sxu32 nArg)` |
|        5 |  658 | `{` |
|        - |  659 | `	ph7_attribute sAttr;` |
|        - |  660 | `	sxi32 rc;` |
|    40185 |  661 | `	if( pClass == 0 ){` |
|      ! 0 |  662 | `		return SXERR_NOTFOUND;` |
|        - |  663 | `	}` |
|    40185 |  664 | `	rc = NativeBuildAttr(&(*pVm),&sAttr,zAttr,aArg,nArg);` |
|    40185 |  665 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  666 | `		return rc;` |
|        - |  667 | `	}` |
|    40185 |  668 | `	return SySetPut(&pClass->aAttrs,(const void *)&sAttr);` |
|    20095 |  669 | `}` |
|        - |  670 | `/*` |
|        - |  671 | ` * php's get_properties / get_debug_info handlers: the SHAPE a class SHOWS.` |
|        - |  672 | ` *` |
|        - |  673 | ` * Several native classes present something that is not their storage. php shows a` |
|        - |  674 | ` * DateTime as date/timezone_type/timezone (the state is a timestamp, an offset and` |
|        - |  675 | ` * a zone name), a DateTimeZone as timezone_type/timezone, and a WeakReference as` |
|        - |  676 | ` * ["object"]. Those slots carry PH7_MOD_HIDDEN so no presentation surface sees the` |
|        - |  677 | ` * engine state; this fills an array with what php shows instead.` |
|        - |  678 | ` *` |
|        - |  679 | ` * Answers 1 when the class (or an ancestor) declared a hook and pOut was filled.` |
|        - |  680 | ` * bDebug distinguishes php's two handlers: 1 for var_dump/print_r (get_debug_info),` |
|        - |  681 | ` * 0 for var_export and the (array) cast (get_properties). They disagree — a` |
|        - |  682 | ` * WeakReference shows ["object"] to var_dump and nothing to (array) — so the` |
|        - |  683 | ` * callback is told which is asking rather than each caller guessing.` |
|        - |  684 | ` * get_object_vars() and foreach are NOT callers: php answers those from the real` |
|        - |  685 | ` * properties with the caller's scope applied, which for every class here is empty.` |
|        - |  686 | ` */` |
|     1240 |  687 | `PH7_PRIVATE int PH7_ClassInstancePresent(ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|        5 |  688 | `{` |
|        - |  689 | `	ph7_class *pClass;` |
|     1245 |  690 | `	if( pThis == 0 \|\| pOut == 0 ){` |
|      ! 0 |  691 | `		return 0;` |
|        - |  692 | `	}` |
|     1873 |  693 | `	for( pClass = pThis->pClass ; pClass ; pClass = pClass->pBase ){` |
|     1329 |  694 | `		if( pClass->xPresent ){` |
|      700 |  695 | `			return pClass->xPresent(pThis->pVm,pThis,pOut,bDebug) == SXRET_OK;` |
|        - |  696 | `		}` |
|      319 |  697 | `	}` |
|      549 |  698 | `	return 0;` |
|      625 |  699 | `}` |
|        - |  700 | `/*` |
|        - |  701 | ` * The nearest ph7_class::xDim in a class's base chain -- php's handler` |
|        - |  702 | ` * inheritance, so a user subclass of DOMNodeList reads dimensions the way its` |
|        - |  703 | ` * parent does.` |
|        - |  704 | ` */` |
|     1318 |  705 | `static ph7_class * NativeDimClass(ph7_class *pClass)` |
|        5 |  706 | `{` |
|     2029 |  707 | `	while( pClass ){` |
|     1431 |  708 | `		if( pClass->xDim ){` |
|      722 |  709 | `			return pClass;` |
|        - |  710 | `		}` |
|      711 |  711 | `		pClass = pClass->pBase;` |
|        5 |  712 | `	}` |
|      603 |  713 | `	return 0;` |
|      664 |  714 | `}` |
|        - |  715 | `/*` |
|        - |  716 | `` * Does `$o[$k]` mean anything for an instance of this class? The subscript`` |
|        - |  717 | `` * opcode asks BEFORE it commits to php's `Cannot use object of type C as`` |
|        - |  718 | `` * array`, which is still the answer for every class that has no hook.`` |
|        - |  719 | ` */` |
|      934 |  720 | `PH7_PRIVATE int PH7_ClassHasNativeDim(ph7_class *pClass)` |
|        5 |  721 | `{` |
|      939 |  722 | `	return NativeDimClass(pClass) != 0;` |
|        5 |  723 | `}` |
|        - |  724 | `/*` |
|        - |  725 | ` * Run the hook. Answers 0 when the class has none (nothing in pCtx is touched);` |
|        - |  726 | ` * 1 when it answered, which includes a REFUSAL -- the caller reads zThrowClass` |
|        - |  727 | ` * to tell the two apart.` |
|        - |  728 | ` */` |
|      350 |  729 | `PH7_PRIVATE int PH7_ClassNativeDim(ph7_class_instance *pThis,PH7_NativeDimCtx *pCtx)` |
|        2 |  730 | `{` |
|      352 |  731 | `	ph7_class *pClass = pThis ? NativeDimClass(pThis->pClass) : 0;` |
|      352 |  732 | `	if( pClass == 0 ){` |
|      ! 0 |  733 | `		return 0;` |
|        - |  734 | `	}` |
|      352 |  735 | `	pClass->xDim(pThis->pVm,pThis,pCtx);` |
|      352 |  736 | `	return 1;` |
|      177 |  737 | `}` |
|        - |  738 | `/*` |
|        - |  739 | ` * The refusal a dimension WRITE, APPEND or UNSET takes on an object. php's own` |
|        - |  740 | ` * sentence for a class that is not an ArrayAccess is` |
|        - |  741 | `` * `Cannot use object of type C as array`; a class whose read handler answers`` |
|        - |  742 | ` * something words its own (php's PDORow names the operation and the class),` |
|        - |  743 | ` * which the hook supplies through the same refusal fields a read uses.` |
|        - |  744 | ` */` |
|       34 |  745 | `PH7_PRIVATE sxu32 PH7_ClassNativeDimRefusal(ph7_class_instance *pThis,int iMode,` |
|        - |  746 | `	char *zMsg,sxu32 nMsg)` |
|        4 |  747 | `{` |
|       38 |  748 | `	ph7_class *pClass = pThis ? NativeDimClass(pThis->pClass) : 0;` |
|       38 |  749 | `	if( pClass ){` |
|        - |  750 | `		PH7_NativeDimCtx sDim;` |
|       25 |  751 | `		sDim.iMode = iMode;` |
|       25 |  752 | `		sDim.pOffset = 0;` |
|       25 |  753 | `		sDim.pResult = 0;` |
|       25 |  754 | `		sDim.zThrowClass = 0;` |
|       25 |  755 | `		sDim.zThrowMsg[0] = 0;` |
|       25 |  756 | `		pClass->xDim(pThis->pVm,pThis,&sDim);` |
|       25 |  757 | `		if( sDim.zThrowClass ){` |
|        9 |  758 | `			return SyBufferFormat(zMsg,nMsg,"%s",sDim.zThrowMsg);` |
|        - |  759 | `		}` |
|        8 |  760 | `	}` |
|       69 |  761 | `	return SyBufferFormat(zMsg,nMsg,"Cannot use object of type %.*s as array",` |
|       26 |  762 | `		pThis ? (int)pThis->pClass->sName.nByte : 0,` |
|       26 |  763 | `		pThis ? pThis->pClass->sName.zString : "");` |
|       21 |  764 | `}` |
|        - |  765 | `/*` |
|        - |  766 | ` * The nearest ph7_class::xProp in a class's base chain -- the same handler` |
|        - |  767 | ` * inheritance xDim and xSet get, and php's own: a subclass of a class whose` |
|        - |  768 | ` * properties are not storage reads them through the parent's handler.` |
|        - |  769 | ` */` |
|     7067 |  770 | `static ph7_class * NativePropClass(ph7_class *pClass)` |
|        5 |  771 | `{` |
|    20012 |  772 | `	while( pClass ){` |
|    13259 |  773 | `		if( pClass->xProp ){` |
|      315 |  774 | `			return pClass;` |
|        - |  775 | `		}` |
|    12945 |  776 | `		pClass = pClass->pBase;` |
|        5 |  777 | `	}` |
|     6758 |  778 | `	return 0;` |
|     3539 |  779 | `}` |
|        - |  780 | `/*` |
|        - |  781 | `` * Does `$o->p` MEAN something this class answers for itself? Asked before the`` |
|        - |  782 | ` * miss path commits to creating a property, warning about an undefined one or` |
|        - |  783 | ` * dispatching __get.` |
|        - |  784 | ` */` |
|     6891 |  785 | `PH7_PRIVATE int PH7_ClassHasNativeProp(ph7_class *pClass)` |
|        5 |  786 | `{` |
|     6896 |  787 | `	return NativePropClass(pClass) != 0;` |
|        5 |  788 | `}` |
|        - |  789 | `/*` |
|        - |  790 | ` * Run the hook. Answers 0 when the class has none, or when the hook DECLINED` |
|        - |  791 | ` * the name (bAnswered left at 0); 1 when it answered, which includes a` |
|        - |  792 | ` * REFUSAL -- the caller reads zThrowClass to tell the two apart.` |
|        - |  793 | ` */` |
|      176 |  794 | `PH7_PRIVATE int PH7_ClassNativeProp(ph7_class_instance *pThis,PH7_NativePropCtx *pCtx)` |
|        1 |  795 | `{` |
|      177 |  796 | `	ph7_class *pClass = pThis ? NativePropClass(pThis->pClass) : 0;` |
|      177 |  797 | `	if( pClass == 0 ){` |
|        9 |  798 | `		return 0;` |
|        - |  799 | `	}` |
|      169 |  800 | `	pClass->xProp(pThis->pVm,pThis,pCtx);` |
|      169 |  801 | `	return pCtx->bAnswered \|\| pCtx->zThrowClass != 0;` |
|       89 |  802 | `}` |
|        - |  803 | `/*` |
|        - |  804 | ` * Fill a caller-owned context and run the hook, for the callers that ask` |
|        - |  805 | ` * OUTSIDE the member opcode: Reflection's getValue()/setValue() and` |
|        - |  806 | ` * property_exists(), each of which reaches php's handlers by its own door.` |
|        - |  807 | ` */` |
|       28 |  808 | `PH7_PRIVATE int PH7_ClassNativePropAsk(ph7_class_instance *pThis,PH7_NativePropCtx *pCtx,` |
|        - |  809 | `	int iMode,const SyString *pName,ph7_value *pResult)` |
|        1 |  810 | `{` |
|       29 |  811 | `	pCtx->iMode = iMode;` |
|       29 |  812 | `	pCtx->pName = pName;` |
|       29 |  813 | `	pCtx->pResult = pResult;` |
|       29 |  814 | `	pCtx->bAnswered = 0;` |
|       29 |  815 | `	pCtx->zThrowClass = 0;` |
|       29 |  816 | `	pCtx->zThrowMsg[0] = 0;` |
|       29 |  817 | `	return PH7_ClassNativeProp(pThis,pCtx);` |
|        1 |  818 | `}` |
|        - |  819 | `/*` |
|        - |  820 | ` * Install a property handler on a mounted native class. Called by the owning` |
|        - |  821 | ` * installer right after PH7_InstallNativeClasses, for the same reason xClone,` |
|        - |  822 | ` * xDim and xSet are: the spec table has no field for a hook.` |
|        - |  823 | ` */` |
|     5740 |  824 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallPropHook(ph7_vm *pVm,const char *zClass,` |
|        - |  825 | `	void (*xProp)(ph7_vm *,ph7_class_instance *,PH7_NativePropCtx *))` |
|        5 |  826 | `{` |
|     5745 |  827 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|     5745 |  828 | `	if( pClass == 0 ){` |
|      ! 0 |  829 | `		return SXERR_NOTFOUND;` |
|        - |  830 | `	}` |
|     5745 |  831 | `	pClass->xProp = xProp;` |
|     5745 |  832 | `	return SXRET_OK;` |
|     2875 |  833 | `}` |
|        - |  834 | `/*` |
|        - |  835 | ` * The nearest ph7_class::xSet in a class's base chain -- the same handler` |
|        - |  836 | ` * inheritance the dimension hook gets, so a user subclass of DateInterval` |
|        - |  837 | ` * converts its writes the way its parent does.` |
|        - |  838 | ` */` |
|      616 |  839 | `static ph7_class * NativeSetClass(ph7_class *pClass)` |
|        3 |  840 | `{` |
|      621 |  841 | `	while( pClass ){` |
|      621 |  842 | `		if( pClass->xSet ){` |
|      619 |  843 | `			return pClass;` |
|        - |  844 | `		}` |
|        3 |  845 | `		pClass = pClass->pBase;` |
|        1 |  846 | `	}` |
|      ! 0 |  847 | `	return 0;` |
|      311 |  848 | `}` |
|        - |  849 | `/*` |
|        - |  850 | ` * Run the write handler for a property store. Answers 0 when no class in the` |
|        - |  851 | ` * chain has one (nothing in pCtx is touched); 1 when it ran, which includes a` |
|        - |  852 | ` * REFUSAL -- the caller reads zThrowClass to tell the two apart.` |
|        - |  853 | ` */` |
|      616 |  854 | `PH7_PRIVATE int PH7_ClassNativeSet(ph7_class_instance *pThis,PH7_NativeSetCtx *pCtx)` |
|        3 |  855 | `{` |
|      619 |  856 | `	ph7_class *pClass = pThis ? NativeSetClass(pThis->pClass) : 0;` |
|      619 |  857 | `	if( pClass == 0 ){` |
|      ! 0 |  858 | `		return 0;` |
|        - |  859 | `	}` |
|      619 |  860 | `	pClass->xSet(pThis->pVm,pThis,pCtx);` |
|      619 |  861 | `	return 1;` |
|      311 |  862 | `}` |
|        - |  863 | `/*` |
|        - |  864 | ` * Install a write handler on a mounted native class and mark every INSTANCE` |
|        - |  865 | ` * property it declares as filtered, which is what makes instantiation register` |
|        - |  866 | ` * the slots the filter looks up. Called by the owning installer right after` |
|        - |  867 | ` * PH7_InstallNativeClasses, for the same reason xClone and xDim are: the spec` |
|        - |  868 | ` * table has no field for a hook.` |
|        - |  869 | ` */` |
|     5740 |  870 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallSetHook(ph7_vm *pVm,const char *zClass,` |
|        - |  871 | `	void (*xSet)(ph7_vm *,ph7_class_instance *,PH7_NativeSetCtx *))` |
|        5 |  872 | `{` |
|     5745 |  873 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|        - |  874 | `	SyHashEntry *pEntry;` |
|     5745 |  875 | `	if( pClass == 0 ){` |
|      ! 0 |  876 | `		return SXERR_NOTFOUND;` |
|        - |  877 | `	}` |
|     5745 |  878 | `	pClass->xSet = xSet;` |
|     5745 |  879 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|    83235 |  880 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|    74625 |  881 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|    74625 |  882 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|    74625 |  883 | `			pAttr->iFlags \|= PH7_CLASS_ATTR_NATIVE_SET;` |
|    37310 |  884 | `		}` |
|        5 |  885 | `	}` |
|     5745 |  886 | `	return SXRET_OK;` |
|     2875 |  887 | `}` |
|        - |  888 | `/*` |
|        - |  889 | ` * The nearest ph7_class::xCmp in a class's base chain -- the same handler` |
|        - |  890 | ` * inheritance xDim and xSet get, and php's own: a subclass of DateTime still` |
|        - |  891 | ` * compares as an instant, whatever properties it adds.` |
|        - |  892 | ` */` |
|      602 |  893 | `static ph7_class * NativeCmpClass(ph7_class *pClass)` |
|        5 |  894 | `{` |
|     1033 |  895 | `	while( pClass ){` |
|      617 |  896 | `		if( pClass->xCmp ){` |
|      188 |  897 | `			return pClass;` |
|        - |  898 | `		}` |
|      431 |  899 | `		pClass = pClass->pBase;` |
|        5 |  900 | `	}` |
|      421 |  901 | `	return 0;` |
|      306 |  902 | `}` |
|        - |  903 | `/*` |
|        - |  904 | ` * Ask the LEFT operand's compare handler, php-style. Answers 0 when no class in` |
|        - |  905 | ` * its chain has one (the caller falls back to the property walk); 1 when the` |
|        - |  906 | ` * handler decided, and *pResult is then the ordering -- which includes a` |
|        - |  907 | ` * REFUSAL, recorded on the VM for the nearest throw boundary to raise, with the` |
|        - |  908 | ` * uncomparable 1 standing in as the answer meanwhile.` |
|        - |  909 | ` */` |
|      366 |  910 | `PH7_PRIVATE int PH7_ClassNativeCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,sxi32 *pResult)` |
|        5 |  911 | `{` |
|      371 |  912 | `	ph7_class *pClass = pLeft ? NativeCmpClass(pLeft->pClass) : 0;` |
|        - |  913 | `	PH7_NativeCmpCtx sCtx;` |
|        - |  914 | `	ph7_vm *pVm;` |
|      371 |  915 | `	if( pClass == 0 ){` |
|      217 |  916 | `		return 0;` |
|        - |  917 | `	}` |
|      156 |  918 | `	pVm = pLeft->pVm;` |
|      156 |  919 | `	SyZero(&sCtx,sizeof(sCtx));` |
|      156 |  920 | `	sCtx.pOther = pRight;` |
|      156 |  921 | `	sCtx.iResult = 1;   /* php's ZEND_UNCOMPARABLE: what a hook that recognizes nothing answers */` |
|      156 |  922 | `	pClass->xCmp(pVm,pLeft,&sCtx);` |
|      156 |  923 | `	if( sCtx.zThrowClass && pVm->zCmpRefusalClass == 0 ){` |
|        - |  924 | `		/* First refusal wins: a driver that keeps comparing after one (sort() does)` |
|        - |  925 | `		 * must not overwrite the message the script will actually see. */` |
|       53 |  926 | `		pVm->zCmpRefusalClass = sCtx.zThrowClass;` |
|       53 |  927 | `		SyMemcpy(sCtx.zThrowMsg,pVm->zCmpRefusalMsg,sizeof(pVm->zCmpRefusalMsg));` |
|       53 |  928 | `		pVm->zCmpRefusalMsg[sizeof(pVm->zCmpRefusalMsg)-1] = 0;` |
|       26 |  929 | `	}` |
|      156 |  930 | `	*pResult = sCtx.iResult;` |
|      156 |  931 | `	return 1;` |
|      188 |  932 | `}` |
|        - |  933 | `/*` |
|        - |  934 | ` * The same handler, asked about a SCALAR partner -- php's compare handler is one` |
|        - |  935 | `` * door and `$n == 2` reaches it exactly as `$n == $m` does. Answers 1 only when`` |
|        - |  936 | ` * the hook RECOGNIZED the value; otherwise the caller falls back to php's` |
|        - |  937 | ` * cast-the-object rule, which is what every class without a handler gets.` |
|        - |  938 | ` */` |
|      236 |  939 | `PH7_PRIVATE int PH7_ClassNativeCmpValue(ph7_class_instance *pLeft,ph7_value *pOther,` |
|        - |  940 | `	int bReversed,sxi32 *pResult)` |
|        4 |  941 | `{` |
|      240 |  942 | `	ph7_class *pClass = pLeft ? NativeCmpClass(pLeft->pClass) : 0;` |
|        - |  943 | `	PH7_NativeCmpCtx sCtx;` |
|        - |  944 | `	ph7_vm *pVm;` |
|      240 |  945 | `	if( pClass == 0 ){` |
|      208 |  946 | `		return 0;` |
|        - |  947 | `	}` |
|       34 |  948 | `	pVm = pLeft->pVm;` |
|       34 |  949 | `	SyZero(&sCtx,sizeof(sCtx));` |
|       34 |  950 | `	sCtx.pOtherValue = pOther;` |
|       34 |  951 | `	sCtx.bReversed = bReversed;` |
|       34 |  952 | `	sCtx.iResult = 1;   /* php's ZEND_UNCOMPARABLE */` |
|       34 |  953 | `	pClass->xCmp(pVm,pLeft,&sCtx);` |
|       34 |  954 | `	if( sCtx.zThrowClass && pVm->zCmpRefusalClass == 0 ){` |
|      ! 0 |  955 | `		pVm->zCmpRefusalClass = sCtx.zThrowClass;` |
|      ! 0 |  956 | `		SyMemcpy(sCtx.zThrowMsg,pVm->zCmpRefusalMsg,sizeof(pVm->zCmpRefusalMsg));` |
|      ! 0 |  957 | `		pVm->zCmpRefusalMsg[sizeof(pVm->zCmpRefusalMsg)-1] = 0;` |
|      ! 0 |  958 | `	}` |
|       34 |  959 | `	if( !sCtx.bAnswered ){` |
|       10 |  960 | `		return 0;` |
|        - |  961 | `	}` |
|       26 |  962 | `	*pResult = sCtx.iResult;` |
|       26 |  963 | `	return 1;` |
|      122 |  964 | `}` |
|        - |  965 | `/*` |
|        - |  966 | ` * Install a compare handler on a mounted native class. Called by the owning` |
|        - |  967 | ` * installer right after PH7_InstallNativeClasses, for the reason xClone, xDim` |
|        - |  968 | ` * and xSet are: the spec table has no field for a hook.` |
|        - |  969 | ` */` |
|        - |  970 | `/*` |
|        - |  971 | ` * php's cast_object handler for _IS_BOOL, which is the one conversion an object` |
|        - |  972 | ` * may decide for itself. Answers 1 when the class HAS a handler, with the truth` |
|        - |  973 | ` * value in *pOut; every other class keeps php's rule that an object is truthy.` |
|        - |  974 | ` */` |
|      286 |  975 | `PH7_PRIVATE int PH7_ClassNativeBool(ph7_class_instance *pThis,int *pOut)` |
|        3 |  976 | `{` |
|        - |  977 | `	ph7_class *pClass;` |
|      745 |  978 | `	for( pClass = pThis ? pThis->pClass : 0 ; pClass ; pClass = pClass->pBase ){` |
|      463 |  979 | `		if( pClass->xBool ){` |
|        5 |  980 | `			*pOut = pClass->xBool(pThis->pVm,pThis) ? 1 : 0;` |
|        5 |  981 | `			return 1;` |
|        - |  982 | `		}` |
|      231 |  983 | `	}` |
|      285 |  984 | `	return 0;` |
|      146 |  985 | `}` |
|     5740 |  986 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallBoolHook(ph7_vm *pVm,const char *zClass,` |
|        - |  987 | `	int (*xBool)(ph7_vm *,ph7_class_instance *))` |
|        5 |  988 | `{` |
|     5745 |  989 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|     5745 |  990 | `	if( pClass == 0 ){` |
|      ! 0 |  991 | `		return SXERR_NOTFOUND;` |
|        - |  992 | `	}` |
|     5745 |  993 | `	pClass->xBool = xBool;` |
|     5745 |  994 | `	return SXRET_OK;` |
|     2875 |  995 | `}` |
|     5740 |  996 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallArithHook(ph7_vm *pVm,const char *zClass,` |
|        - |  997 | `	void (*xArith)(ph7_vm *,ph7_class_instance *,PH7_NativeArithCtx *))` |
|        5 |  998 | `{` |
|     5745 |  999 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|     5745 | 1000 | `	if( pClass == 0 ){` |
|      ! 0 | 1001 | `		return SXERR_NOTFOUND;` |
|        - | 1002 | `	}` |
|     5745 | 1003 | `	pClass->xArith = xArith;` |
|     5745 | 1004 | `	return SXRET_OK;` |
|     2875 | 1005 | `}` |
|    28700 | 1006 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallCmpHook(ph7_vm *pVm,const char *zClass,` |
|        - | 1007 | `	void (*xCmp)(ph7_vm *,ph7_class_instance *,PH7_NativeCmpCtx *))` |
|        5 | 1008 | `{` |
|    28705 | 1009 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|    28705 | 1010 | `	if( pClass == 0 ){` |
|      ! 0 | 1011 | `		return SXERR_NOTFOUND;` |
|        - | 1012 | `	}` |
|    28705 | 1013 | `	pClass->xCmp = xCmp;` |
|    28705 | 1014 | `	return SXRET_OK;` |
|    14355 | 1015 | `}` |
|        - | 1016 | `/*` |
|        - | 1017 | ` * Mark every INSTANCE property a mounted native class declares as one php` |
|        - | 1018 | ` * FABRICATES rather than stores (PH7_CLASS_ATTR_NATIVE_VIRTUAL), which is what` |
|        - | 1019 | ` * keeps the object comparator from seeing it. DatePeriod is the whole caller` |
|        - | 1020 | ` * list: php's object has an EMPTY real property table, so two of them are equal` |
|        - | 1021 | ` * whatever they contain, while a subclass's own property still decides.` |
|        - | 1022 | ` */` |
|    11480 | 1023 | `PH7_PRIVATE sxi32 PH7_NativeClassMarkVirtualProps(ph7_vm *pVm,const char *zClass)` |
|        5 | 1024 | `{` |
|    11485 | 1025 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|        - | 1026 | `	SyHashEntry *pEntry;` |
|    11485 | 1027 | `	if( pClass == 0 ){` |
|      ! 0 | 1028 | `		return SXERR_NOTFOUND;` |
|        - | 1029 | `	}` |
|    11485 | 1030 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|    74625 | 1031 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|    57405 | 1032 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|    57405 | 1033 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|    57405 | 1034 | `			pAttr->iFlags \|= PH7_CLASS_ATTR_NATIVE_VIRTUAL;` |
|    28700 | 1035 | `		}` |
|        5 | 1036 | `	}` |
|    11485 | 1037 | `	return SXRET_OK;` |
|     5745 | 1038 | `}` |
|        - | 1039 | `/*` |
|        - | 1040 | ` * Mark every php-VISIBLE instance property a mounted native class declares as one` |
|        - | 1041 | ` * the OBJECT does not hold until its constructor fills it` |
|        - | 1042 | ` * (PH7_CLASS_ATTR_NATIVE_LAZY). php's DateInterval and DatePeriod are the caller` |
|        - | 1043 | ` * list: the state is a C struct the constructor allocates and the property table` |
|        - | 1044 | ` * is written FROM it, so an object nobody constructed has no such property at all.` |
|        - | 1045 | ` *` |
|        - | 1046 | ` * The HIDDEN slots are left alone -- they are PHL's own storage, they have to` |
|        - | 1047 | `` * exist from `new` (the initialized FLAG lives in one of them), and php shows`` |
|        - | 1048 | ` * nothing for them either way.` |
|        - | 1049 | ` *` |
|        - | 1050 | ` * bDefaultRead selects which of php's two handlers the class has: with it, a read` |
|        - | 1051 | ` * of a still-absent slot answers the DECLARED literal in silence (DatePeriod's` |
|        - | 1052 | ` * read_property over the zeroed struct); without it, the name really is undefined` |
|        - | 1053 | ` * until the constructor runs (DateInterval).` |
|        - | 1054 | ` */` |
|        - | 1055 | `/*` |
|        - | 1056 | ` * Mark every php-VISIBLE instance property a mounted native class declares as one` |
|        - | 1057 | ` * whose WRITE php's handler refuses (PH7_CLASS_ATTR_NATIVE_NOWRITE). DatePeriod is` |
|        - | 1058 | `` * the caller list: php answers `Cannot modify readonly property DatePeriod::$p` to`` |
|        - | 1059 | `` * every write form and `Cannot unset DatePeriod::$p` to an unset, while Reflection`` |
|        - | 1060 | ` * still reports isReadOnly() false -- the wording is the handler's, not the` |
|        - | 1061 | ` * readonly flag's. The C bodies that fill the seven write their slots directly and` |
|        - | 1062 | ` * never pass the store filter, so the refusal costs the class nothing.` |
|        - | 1063 | ` */` |
|     5740 | 1064 | `PH7_PRIVATE sxi32 PH7_NativeClassMarkNoWriteProps(ph7_vm *pVm,const char *zClass)` |
|        5 | 1065 | `{` |
|     5745 | 1066 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|        - | 1067 | `	SyHashEntry *pEntry;` |
|     5745 | 1068 | `	if( pClass == 0 ){` |
|      ! 0 | 1069 | `		return SXERR_NOTFOUND;` |
|        - | 1070 | `	}` |
|     5745 | 1071 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|    51665 | 1072 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|    45925 | 1073 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|    45925 | 1074 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_HIDDEN) ){` |
|     5745 | 1075 | `			continue;` |
|        - | 1076 | `		}` |
|    40185 | 1077 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_NATIVE_NOWRITE;` |
|        5 | 1078 | `	}` |
|     5745 | 1079 | `	return SXRET_OK;` |
|     2875 | 1080 | `}` |
|        - | 1081 | `/*` |
|        - | 1082 | ` * Mark ONE property of ONE instance as php's read-only kind: a plain store and` |
|        - | 1083 | `` * an unset() refuse with `Property p is read only`, everything that takes a`` |
|        - | 1084 | ` * pointer to it goes through. It is marked per OBJECT because php's handler is` |
|        - | 1085 | ` * -- a PDOStatement with no cursor behind it takes the write, and only the one` |
|        - | 1086 | ` * a driver built refuses.` |
|        - | 1087 | ` */` |
|      918 | 1088 | `PH7_PRIVATE void PH7_NativeMarkAttrReadOnly(ph7_class_instance *pThis,const char *zProp)` |
|        3 | 1089 | `{` |
|      921 | 1090 | `	SyHashEntry *pEntry = pThis` |
|      918 | 1091 | `		? SyHashGet(&pThis->hAttr,(const void *)zProp,(sxu32)SyStrlen(zProp)) : 0;` |
|      921 | 1092 | `	if( pEntry ){` |
|      921 | 1093 | `		((VmClassAttr *)pEntry->pUserData)->iState \|= VM_CLASS_ATTR_RDONLY;` |
|      459 | 1094 | `	}` |
|      921 | 1095 | `}` |
|    17220 | 1096 | `PH7_PRIVATE sxi32 PH7_NativeClassMarkLazyProps(ph7_vm *pVm,const char *zClass,int bDefaultRead)` |
|        5 | 1097 | `{` |
|    17225 | 1098 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|        - | 1099 | `	SyHashEntry *pEntry;` |
|    17225 | 1100 | `	if( pClass == 0 ){` |
|      ! 0 | 1101 | `		return SXERR_NOTFOUND;` |
|        - | 1102 | `	}` |
|    17225 | 1103 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|   154985 | 1104 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|   137765 | 1105 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   137765 | 1106 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_HIDDEN) ){` |
|    28705 | 1107 | `			continue;` |
|        - | 1108 | `		}` |
|   109065 | 1109 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_NATIVE_LAZY;` |
|   109065 | 1110 | `		if( bDefaultRead ){` |
|    40185 | 1111 | `			pAttr->iFlags \|= PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT;` |
|    20090 | 1112 | `		}` |
|   109065 | 1113 | `		pClass->iFlags \|= PH7_CLASS_LAZY_ATTR;` |
|        5 | 1114 | `	}` |
|    17225 | 1115 | `	return SXRET_OK;` |
|     8615 | 1116 | `}` |
|        - | 1117 | `/*` |
|        - | 1118 | ` * Install this object's LAZY properties -- the whole set, in the order the class` |
|        - | 1119 | ` * declares them, skipping any the object already carries.` |
|        - | 1120 | ` *` |
|        - | 1121 | ` * The ORDER is php's: its constructor writes the struct's fields into the property` |
|        - | 1122 | ` * table one after another, so a name the object already has keeps its POSITION and` |
|        - | 1123 | ` * only takes the new value, and the rest are appended in declared order behind it.` |
|        - | 1124 | ` * VmRecreateDeclaredAttr tail-inserts exactly that way.` |
|        - | 1125 | ` *` |
|        - | 1126 | ` * The declared literal goes in as the slot's starting value (php's zeroed struct),` |
|        - | 1127 | ` * and the not-yet-initialized mark a TYPED slot would carry is cleared with it:` |
|        - | 1128 | ` * these are filled by the C body that is about to write them, and a read between` |
|        - | 1129 | ` * the two is php's default, not its Error.` |
|        - | 1130 | ` */` |
|  1542648 | 1131 | `PH7_PRIVATE void PH7_NativeMaterializeLazy(ph7_vm *pVm,ph7_class_instance *pObj)` |
|        5 | 1132 | `{` |
|        - | 1133 | `	SyHashEntry *pEntry;` |
|  1542648 | 1134 | `	if( pObj == 0 \|\| (pObj->pClass->iFlags & PH7_CLASS_LAZY_ATTR) == 0` |
|   774829 | 1135 | `	 \|\| (pObj->iFlags & VM_INSTANCE_LAZY_DONE) ){` |
|  1542041 | 1136 | `		return;` |
|        - | 1137 | `	}` |
|      615 | 1138 | `	pObj->iFlags \|= VM_INSTANCE_LAZY_DONE;` |
|      615 | 1139 | `	SyHashResetLoopCursor(&pObj->pClass->hAttr);` |
|     7803 | 1140 | `	while( (pEntry = SyHashGetNextEntry(&pObj->pClass->hAttr)) != 0 ){` |
|     7191 | 1141 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     7191 | 1142 | `		VmClassAttr *pVmAttr = 0;` |
|        - | 1143 | `		ph7_value *pSlot;` |
|     7188 | 1144 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_LAZY) == 0` |
|     6647 | 1145 | `		 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_ONDEMAND) != 0 ){` |
|     1545 | 1146 | `			continue;   /* ...and an ON-DEMAND one waits for the write that names it */` |
|        - | 1147 | `		}` |
|     5649 | 1148 | `		if( SyHashGet(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName)) != 0 ){` |
|      ! 0 | 1149 | `			continue;` |
|        - | 1150 | `		}` |
|     5649 | 1151 | `		VmRecreateDeclaredAttr(&(*pVm),pObj,pAttr,&pVmAttr);` |
|     5649 | 1152 | `		if( pVmAttr == 0 ){` |
|      ! 0 | 1153 | `			continue;   /* OOM: the caller's write lands nowhere, as it would have anyway */` |
|        - | 1154 | `		}` |
|     5649 | 1155 | `		pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|     5649 | 1156 | `		if( pAttr->pNativeValue == 0 ){` |
|      ! 0 | 1157 | `			continue;` |
|        - | 1158 | `		}` |
|     5649 | 1159 | `		pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,pVmAttr->nIdx);` |
|     5649 | 1160 | `		if( pSlot ){` |
|     5649 | 1161 | `			PH7_NativeLiteralValue(&(*pVm),pAttr->pNativeValue,pSlot);` |
|     2823 | 1162 | `		}` |
|        3 | 1163 | `	}` |
|   771338 | 1164 | `}` |
|        - | 1165 | `/*` |
|        - | 1166 | ` * Create and install ONE class from its spec: constants and properties, but` |
|        - | 1167 | ` * neither methods nor its base chain.` |
|        - | 1168 | ` *` |
|        - | 1169 | ` * Split from the two passes that follow because a spec table may describe` |
|        - | 1170 | ` * classes that extend each other, and PH7_ClassInherit needs the parent to` |
|        - | 1171 | ` * exist -- and to already CARRY ITS METHODS, since inheriting is what copies` |
|        - | 1172 | ` * them down.` |
|        - | 1173 | ` */` |
|  1113560 | 1174 | `static sxi32 NativeDeclareClass(ph7_vm *pVm,const PH7_NativeClassSpec *pSpec,ph7_class **ppOut)` |
|        5 | 1175 | `{` |
|        - | 1176 | `	ph7_class *pClass;` |
|        - | 1177 | `	SyString sName;` |
|        - | 1178 | `	sxu32 n;` |
|        - | 1179 | `	sxi32 rc;` |
|  1113565 | 1180 | `	SyStringInitFromBuf(&sName,pSpec->zName,SyStrlen(pSpec->zName));` |
|  1113565 | 1181 | `	pClass = PH7_NewRawClass(&(*pVm),&sName,0);` |
|  1113565 | 1182 | `	if( pClass == 0 ){` |
|      ! 0 | 1183 | `		return SXERR_MEM;` |
|        - | 1184 | `	}` |
|        - | 1185 | `	/* PH7_CLASS_BOUND: an engine class is declared unconditionally, exactly once,` |
|        - | 1186 | ``	 * so `class DateTime {}` in user code is php's "Cannot redeclare class`` |
|        - | 1187 | `	 * DateTime". Only the compiler used to set the flag, so EVERY native class` |
|        - | 1188 | `	 * was silently redeclarable — the chunk-declared ones (Exception, stdClass)` |
|        - | 1189 | `	 * fataled and their C replacements did not. */` |
|  1113565 | 1190 | `	pClass->iFlags \|= pSpec->iFlags \| PH7_CLASS_BOUND;` |
|  1113565 | 1191 | `	pClass->xRelease = pSpec->xRelease;` |
|  1113565 | 1192 | `	pClass->pIterVtab = pSpec->pIterVtab;` |
|  1113565 | 1193 | `	pClass->xPresent = pSpec->xPresent;` |
|  2313225 | 1194 | `	for( n = 0 ; n < pSpec->nConst ; n++ ){` |
|  1199665 | 1195 | `		rc = NativeInstallConstant(&(*pVm),pClass,&pSpec->aConst[n]);` |
|  1199665 | 1196 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1197 | `			return rc;` |
|        - | 1198 | `		}` |
|   599835 | 1199 | `	}` |
|  2565785 | 1200 | `	for( n = 0 ; n < pSpec->nProp ; n++ ){` |
|  1452225 | 1201 | `		rc = PH7_NativeClassInstallProperty(&(*pVm),pClass,&pSpec->aProp[n]);` |
|  1452225 | 1202 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1203 | `			return rc;` |
|        - | 1204 | `		}` |
|   726115 | 1205 | `	}` |
|  1113565 | 1206 | `	*ppOut = pClass;` |
|  1113565 | 1207 | `	return PH7_VmInstallClass(&(*pVm),pClass);` |
|   556785 | 1208 | `}` |
|        - | 1209 | `/*` |
|        - | 1210 | ` * Wire ONE class's base chain and interfaces.` |
|        - | 1211 | ` *` |
|        - | 1212 | ` * This runs AFTER every class in the table has its own methods, which is the` |
|        - | 1213 | ` * compiler's order too (GenStateCompileClassEx compiles the whole body and only` |
|        - | 1214 | ` * then calls PH7_ClassInherit/PH7_ClassImplement). Two things depend on it:` |
|        - | 1215 | ` * PH7_ClassInherit COPIES the base's methods into the subclass, so a base whose` |
|        - | 1216 | ` * methods were not installed yet hands down an empty table -- which is how` |
|        - | 1217 | `` * `DOMDocument::C14N()` came out undefined, the first time a native class`` |
|        - | 1218 | ` * extended another native class that had methods; and PH7_ClassImplement stubs` |
|        - | 1219 | ` * every interface method the class lacks as ABSTRACT, so a spec may now name an` |
|        - | 1220 | ` * interface it implements itself rather than attaching it by hand afterwards.` |
|        - | 1221 | ` */` |
|  1113560 | 1222 | `static sxi32 NativeLinkClass(ph7_vm *pVm,const PH7_NativeClassSpec *pSpec,ph7_class *pClass)` |
|        5 | 1223 | `{` |
|        - | 1224 | `	sxi32 rc;` |
|  1113565 | 1225 | `	if( pSpec->zParent ){` |
|   568265 | 1226 | `		ph7_class *pBase = NativeLookupClass(&(*pVm),pSpec->zParent);` |
|   568265 | 1227 | `		if( pBase == 0 ){` |
|      ! 0 | 1228 | `			return SXERR_NOTFOUND;` |
|        - | 1229 | `		}` |
|        - | 1230 | `		/* The VM's OWN generator state, not a NULL one: PH7_ClassInherit takes its` |
|        - | 1231 | `		 * scratch allocator from pGen->pVm and reports every inheritance rule it` |
|        - | 1232 | `		 * enforces through PH7_GenCompileError(pGen, ...). Passing 0 crashed the` |
|        - | 1233 | `		 * moment a native spec first named a parent (the date exceptions). */` |
|   852395 | 1234 | `		rc = (pClass->iFlags & PH7_CLASS_INTERFACE)` |
|    51660 | 1235 | `			? PH7_ClassInterfaceInherit(pClass,pBase)` |
|   542430 | 1236 | `			: PH7_ClassInherit(&pVm->sCodeGen,pClass,pBase);` |
|   568265 | 1237 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1238 | `			return rc;` |
|        - | 1239 | `		}` |
|   284130 | 1240 | `	}` |
|  1113565 | 1241 | `	if( pSpec->zImplements ){` |
|        - | 1242 | `		/* Comma-separated list, so one spec row can name several interfaces. */` |
|   287005 | 1243 | `		const char *zCur = pSpec->zImplements;` |
|   700285 | 1244 | `		while( zCur[0] != '\0' ){` |
|        - | 1245 | `			const char *zStart;` |
|        - | 1246 | `			char zIface[64];` |
|        - | 1247 | `			sxu32 nLen;` |
|        - | 1248 | `			ph7_class *pIface;` |
|   539565 | 1249 | `			while( zCur[0] == ' ' \|\| zCur[0] == ',' ){` |
|   126285 | 1250 | `				zCur++;` |
|        5 | 1251 | `			}` |
|   413285 | 1252 | `			if( zCur[0] == '\0' ){` |
|      ! 0 | 1253 | `				break;` |
|        - | 1254 | `			}` |
|   413285 | 1255 | `			zStart = zCur;` |
|  5510405 | 1256 | `			while( zCur[0] != '\0' && zCur[0] != ',' && zCur[0] != ' ' ){` |
|  5097125 | 1257 | `				zCur++;` |
|        5 | 1258 | `			}` |
|   413285 | 1259 | `			nLen = (sxu32)(zCur - zStart);` |
|   413285 | 1260 | `			if( nLen >= sizeof(zIface) ){` |
|      ! 0 | 1261 | `				return SXERR_SYNTAX;` |
|        - | 1262 | `			}` |
|   413285 | 1263 | `			SyMemcpy(zStart,zIface,nLen);` |
|   413285 | 1264 | `			zIface[nLen] = '\0';` |
|   413285 | 1265 | `			pIface = NativeLookupClass(&(*pVm),zIface);` |
|   413285 | 1266 | `			if( pIface == 0 ){` |
|      ! 0 | 1267 | `				return SXERR_NOTFOUND;` |
|        - | 1268 | `			}` |
|   413285 | 1269 | `			rc = PH7_ClassImplement(pClass,pIface);` |
|   413285 | 1270 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1271 | `				return rc;` |
|        - | 1272 | `			}` |
|        5 | 1273 | `		}` |
|   143500 | 1274 | `	}` |
|  1113565 | 1275 | `	return SXRET_OK;` |
|   556785 | 1276 | `}` |
|        - | 1277 | `/*` |
|        - | 1278 | ` * Install a whole table of native classes, in the compiler's own order: declare` |
|        - | 1279 | ` * them all (so later rows may extend earlier ones), fill in their methods, wire` |
|        - | 1280 | ` * the base chains and interfaces, then mount.` |
|        - | 1281 | ` *` |
|        - | 1282 | ` * Interfaces are declared but never mounted -- VmMountUserClass skips them anyway,` |
|        - | 1283 | ` * their methods being non-invocable.` |
|        - | 1284 | ` */` |
|   246820 | 1285 | `PH7_PRIVATE sxi32 PH7_InstallNativeClasses(ph7_vm *pVm,const PH7_NativeClassSpec *aSpec,sxu32 nSpec)` |
|        5 | 1286 | `{` |
|        - | 1287 | `	ph7_class **apClass;` |
|        - | 1288 | `	sxu32 i,j;` |
|        - | 1289 | `	sxi32 rc;` |
|   246825 | 1290 | `	apClass = (ph7_class **)SyMemBackendAlloc(&pVm->sAllocator,nSpec * sizeof(ph7_class *));` |
|   246825 | 1291 | `	if( apClass == 0 ){` |
|      ! 0 | 1292 | `		return SXERR_MEM;` |
|        - | 1293 | `	}` |
|  1360385 | 1294 | `	for( i = 0 ; i < nSpec ; i++ ){` |
|  1113565 | 1295 | `		apClass[i] = 0;` |
|  1113565 | 1296 | `		rc = NativeDeclareClass(&(*pVm),&aSpec[i],&apClass[i]);` |
|  1113565 | 1297 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1298 | `			goto Done;` |
|        - | 1299 | `		}` |
|   556785 | 1300 | `	}` |
|  1360385 | 1301 | `	for( i = 0 ; i < nSpec ; i++ ){` |
|  7835105 | 1302 | `		for( j = 0 ; j < aSpec[i].nMethod ; j++ ){` |
|  6721545 | 1303 | `			rc = PH7_NativeClassInstallMethod(&(*pVm),apClass[i],&aSpec[i].aMethod[j],0);` |
|  6721545 | 1304 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1305 | `				goto Done;` |
|        - | 1306 | `			}` |
|  3360775 | 1307 | `		}` |
|   556785 | 1308 | `	}` |
|  1360385 | 1309 | `	for( i = 0 ; i < nSpec ; i++ ){` |
|  1113565 | 1310 | `		rc = NativeLinkClass(&(*pVm),&aSpec[i],apClass[i]);` |
|  1113565 | 1311 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1312 | `			goto Done;` |
|        - | 1313 | `		}` |
|   556785 | 1314 | `	}` |
|  1360385 | 1315 | `	for( i = 0 ; i < nSpec ; i++ ){` |
|  1113565 | 1316 | `		rc = VmMountUserClass(&(*pVm),apClass[i]);` |
|  1113565 | 1317 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1318 | `			goto Done;` |
|        - | 1319 | `		}` |
|   556785 | 1320 | `	}` |
|   246825 | 1321 | `	rc = SXRET_OK;` |
|   123410 | 1322 | `Done:` |
|   246825 | 1323 | `	SyMemBackendFree(&pVm->sAllocator,apClass);` |
|   246825 | 1324 | `	return rc;` |
|   123415 | 1325 | `}` |
|        - | 1326 | `/*` |
|        - | 1327 | ` * ---------------------------------------------------------------------------` |
|        - | 1328 | ` * Declaring an ENUM from C.` |
|        - | 1329 | ` *` |
|        - | 1330 | `` * An enum is not a class with constants: each `case` is a class constant whose`` |
|        - | 1331 | ` * slot holds THE singleton instance of the enum for that case, materialized` |
|        - | 1332 | ` * lazily on first access (VmEnumMaterializeCase). The compiler builds one by` |
|        - | 1333 | `` * declaring the readonly `name`/`value` properties, pushing each case onto`` |
|        - | 1334 | ` * ph7_class::aEnumCases, and SYNTHESIZING cases()/from()/tryFrom() as PHP` |
|        - | 1335 | `` * source that forwards to the `__phl_enum_*` thunks.`` |
|        - | 1336 | ` *` |
|        - | 1337 | ` * This does the same three things without a compiler: the case's backing value` |
|        - | 1338 | ` * rides as a literal (ph7_class_attr::pNativeValue, which the materializer` |
|        - | 1339 | ` * reads where a compiled case has byte-code), and the three interface methods` |
|        - | 1340 | ` * are C bodies that call the very same engine workers the synthesized PHP` |
|        - | 1341 | `` * forwards to — so a native enum is an ordinary one to `instanceof`,`` |
|        - | 1342 | `` * `match`, Reflection and `===` case identity.`` |
|        - | 1343 | ` * ---------------------------------------------------------------------------` |
|        - | 1344 | ` */` |
|        - | 1345 | `/* The enum a static native method was called on. */` |
|      198 | 1346 | `static ph7_class * NativeEnumSelf(ph7_context *pCtx,ph7_value *pName)` |
|        2 | 1347 | `{` |
|      200 | 1348 | `	ph7_class *pClass = PH7_ContextCalledClass(pCtx);` |
|      200 | 1349 | `	PH7_MemObjInit(pCtx->pVm,pName);` |
|      200 | 1350 | `	if( pClass ){` |
|      200 | 1351 | `		ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|       99 | 1352 | `	}` |
|      200 | 1353 | `	return pClass;` |
|        2 | 1354 | `}` |
|        - | 1355 | `/*` |
|        - | 1356 | ` * cases() / from() / tryFrom(): the engine thunks take the enum's FQN as their` |
|        - | 1357 | ` * first argument, exactly as the compiler's synthesized bodies pass it.` |
|        - | 1358 | ` */` |
|       20 | 1359 | `static int vm_builtin_NativeEnum_cases(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1360 | `{` |
|        - | 1361 | `	ph7_value sName;` |
|        - | 1362 | `	ph7_value *ap[1];` |
|        - | 1363 | `	int rc;` |
|       10 | 1364 | `	SXUNUSED(nArg);` |
|       10 | 1365 | `	SXUNUSED(apArg);` |
|       21 | 1366 | `	if( NativeEnumSelf(pCtx,&sName) == 0 ){` |
|      ! 0 | 1367 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1368 | `		return PH7_OK;` |
|        - | 1369 | `	}` |
|       21 | 1370 | `	ap[0] = &sName;` |
|       21 | 1371 | `	rc = vm_builtin_enum_cases(pCtx,1,ap);` |
|       21 | 1372 | `	PH7_MemObjRelease(&sName);` |
|       21 | 1373 | `	return rc;` |
|       11 | 1374 | `}` |
|      178 | 1375 | `static int NativeEnumFrom(ph7_context *pCtx,int nArg,ph7_value **apArg,int bTry)` |
|        2 | 1376 | `{` |
|        - | 1377 | `	ph7_value sName;` |
|        - | 1378 | `	ph7_value *ap[2];` |
|        - | 1379 | `	int rc;` |
|      180 | 1380 | `	if( nArg < 1 \|\| NativeEnumSelf(pCtx,&sName) == 0 ){` |
|      ! 0 | 1381 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1382 | `		return PH7_OK;` |
|        - | 1383 | `	}` |
|      180 | 1384 | `	ap[0] = &sName;` |
|      180 | 1385 | `	ap[1] = apArg[0];` |
|      180 | 1386 | `	rc = bTry ? vm_builtin_enum_tryfrom(pCtx,2,ap) : vm_builtin_enum_from(pCtx,2,ap);` |
|      180 | 1387 | `	PH7_MemObjRelease(&sName);` |
|      180 | 1388 | `	return rc;` |
|       91 | 1389 | `}` |
|       96 | 1390 | `static int vm_builtin_NativeEnum_from(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 1391 | `{` |
|       98 | 1392 | `	return NativeEnumFrom(pCtx,nArg,apArg,0);` |
|        2 | 1393 | `}` |
|       82 | 1394 | `static int vm_builtin_NativeEnum_tryFrom(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1395 | `{` |
|       83 | 1396 | `	return NativeEnumFrom(pCtx,nArg,apArg,1);` |
|        1 | 1397 | `}` |
|        - | 1398 | ``/* The readonly `name` (every enum) and `value` (backed only) case properties,`` |
|        - | 1399 | ` * declared exactly as GenStateCompileEnum does. */` |
|    22960 | 1400 | `static sxi32 NativeEnumInstallProp(ph7_vm *pVm,ph7_class *pClass,const char *zName,` |
|        - | 1401 | `	sxu32 nType,const char *zTypeName)` |
|        5 | 1402 | `{` |
|        - | 1403 | `	SyString sName;` |
|        - | 1404 | `	ph7_class_attr *pAttr;` |
|    22965 | 1405 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|    22965 | 1406 | `	pAttr = PH7_NewClassAttr(&(*pVm),&sName,0,PH7_CLASS_PROT_PUBLIC,` |
|        - | 1407 | `		PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|    22965 | 1408 | `	if( pAttr == 0 ){` |
|      ! 0 | 1409 | `		return SXERR_MEM;` |
|        - | 1410 | `	}` |
|    22965 | 1411 | `	pAttr->nType = nType;` |
|    22965 | 1412 | `	SyStringInitFromBuf(&pAttr->sTypeName,zTypeName,SyStrlen(zTypeName));` |
|    22965 | 1413 | `	return PH7_ClassInstallAttr(pClass,pAttr);` |
|    11485 | 1414 | `}` |
|        - | 1415 | `/*` |
|        - | 1416 | ` * cases()/from()/tryFrom(), installed on ANY enum -- one declared from C here` |
|        - | 1417 | ` * and one the compiler just finished reading from source. php declares them on` |
|        - | 1418 | ``  * the UnitEnum/BackedEnum prototypes, which is why `from` takes `string\|int` `` |
|        - | 1419 | ` * rather than the enum's own backing type: the refusal for the wrong one is a` |
|        - | 1420 | ` * VALUE check inside the body, not the parameter's.` |
|        - | 1421 | ` *` |
|        - | 1422 | ` * The compiler used to synthesize PHP source forwarding to three global` |
|        - | 1423 | `` * `__phl_enum_*` thunks. Those names were php-visible, and the methods reported`` |
|        - | 1424 | `` * as `<user>` with the enum's file and line where php reports`` |
|        - | 1425 | `` * `<internal, prototype BackedEnum>`.`` |
|        - | 1426 | ` */` |
|        - | 1427 | `/*` |
|        - | 1428 | ` * Install one of them and stamp it INTERNAL unconditionally. The usual` |
|        - | 1429 | `` * `bCompilingBuiltin` stamp only fires while the engine compiles its OWN`` |
|        - | 1430 | ` * sources, and these three are attached to a class the compiler is reading out` |
|        - | 1431 | ` * of a USER file — but php reports them as internal wherever the enum is` |
|        - | 1432 | ` * declared: isInternal() true, getFileName() false, getStartLine() 0.` |
|        - | 1433 | ` */` |
|    28918 | 1434 | `static sxi32 NativeEnumInstallMethod(ph7_vm *pVm,ph7_class *pClass,` |
|        - | 1435 | `	const PH7_NativeMethodDef *pDef)` |
|        5 | 1436 | `{` |
|        - | 1437 | `	ph7_class_method *pMeth;` |
|    28923 | 1438 | `	sxi32 rc = PH7_NativeClassInstallMethod(&(*pVm),pClass,pDef,0);` |
|    28923 | 1439 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1440 | `		return rc;` |
|        - | 1441 | `	}` |
|    28923 | 1442 | `	pMeth = PH7_ClassExtractMethod(pClass,pDef->zName,(sxu32)SyStrlen(pDef->zName));` |
|    28923 | 1443 | `	if( pMeth ){` |
|    28923 | 1444 | `		pMeth->sFunc.iFlags \|= VM_FUNC_INTERNAL;` |
|        - | 1445 | `		/* getFileName() reads the recorded source file rather than the flag, and` |
|        - | 1446 | `		 * PH7_NewClassMethod stamped the user file the enum was read from. */` |
|    28923 | 1447 | `		SyStringInitFromBuf(&pMeth->sFunc.sFile,"",0);` |
|    14459 | 1448 | `	}` |
|    28923 | 1449 | `	return SXRET_OK;` |
|    14464 | 1450 | `}` |
|    17334 | 1451 | `PH7_PRIVATE sxi32 PH7_InstallEnumInterfaceMethods(ph7_vm *pVm,ph7_class *pClass)` |
|        5 | 1452 | `{` |
|        - | 1453 | `	static const PH7_NativeMethodDef sCases =` |
|        - | 1454 | `		{ "cases", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "", "array", vm_builtin_NativeEnum_cases };` |
|        - | 1455 | `	static const PH7_NativeMethodDef aFrom[] = {` |
|        - | 1456 | `		{ "from",    PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "string\|int $value", "static",` |
|        - | 1457 | `		  vm_builtin_NativeEnum_from },` |
|        - | 1458 | `		{ "tryFrom", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "string\|int $value", "?static",` |
|        - | 1459 | `		  vm_builtin_NativeEnum_tryFrom },` |
|        - | 1460 | `	};` |
|        - | 1461 | `	sxu32 n;` |
|    17339 | 1462 | `	sxi32 rc = NativeEnumInstallMethod(&(*pVm),pClass,&sCases);` |
|    17339 | 1463 | `	if( rc != SXRET_OK \|\| pClass->nEnumBacking == 0 ){` |
|    11547 | 1464 | `		return rc;` |
|        - | 1465 | `	}` |
|    17381 | 1466 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFrom) ; n++ ){` |
|    11589 | 1467 | `		rc = NativeEnumInstallMethod(&(*pVm),pClass,&aFrom[n]);` |
|    11589 | 1468 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1469 | `			return rc;` |
|        - | 1470 | `		}` |
|     5797 | 1471 | `	}` |
|     5797 | 1472 | `	return SXRET_OK;` |
|     8672 | 1473 | `}` |
|    17220 | 1474 | `PH7_PRIVATE sxi32 PH7_InstallNativeEnum(ph7_vm *pVm,const char *zName,sxu32 nBacking,` |
|        - | 1475 | `	const PH7_NativeEnumCase *aCase,sxu32 nCase,` |
|        - | 1476 | `	const PH7_NativeMethodDef *aMethod,sxu32 nMethod)` |
|        5 | 1477 | `{` |
|        - | 1478 | `	ph7_class *pClass, *pIface;` |
|        - | 1479 | `	SyString sName;` |
|        - | 1480 | `	sxu32 n;` |
|        - | 1481 | `	sxi32 rc;` |
|    17225 | 1482 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|    17225 | 1483 | `	pClass = PH7_NewRawClass(&(*pVm),&sName,0);` |
|    17225 | 1484 | `	if( pClass == 0 ){` |
|      ! 0 | 1485 | `		return SXERR_MEM;` |
|        - | 1486 | `	}` |
|        - | 1487 | `	/* php: no enum can be extended or instantiated, and the ENUM flag alone says` |
|        - | 1488 | ``	 * so -- the `extends` refusal names the enum rather than a final class, and it`` |
|        - | 1489 | `	 * is asked first. The FINAL flag is deliberately NOT set: php stamps` |
|        - | 1490 | `` 	 * ZEND_ACC_FINAL on a COMPILED enum only, so `isFinal()`/`getModifiers()` `` |
|        - | 1491 | ``	 * answer true/32 for `enum U {}` and false/0 for every enum php declares from`` |
|        - | 1492 | `	 * C (RoundingMode, PropertyHookType). Setting it here made an internal enum` |
|        - | 1493 | `	 * report itself as a userland one. */` |
|    17225 | 1494 | `	pClass->iFlags \|= PH7_CLASS_ENUM;` |
|    17225 | 1495 | `	pClass->nEnumBacking = nBacking;` |
|    17225 | 1496 | `	rc = NativeEnumInstallProp(&(*pVm),pClass,"name",MEMOBJ_STRING,"string");` |
|    17225 | 1497 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1498 | `		return rc;` |
|        - | 1499 | `	}` |
|    17225 | 1500 | `	if( nBacking != 0 ){` |
|     8615 | 1501 | `		rc = NativeEnumInstallProp(&(*pVm),pClass,"value",nBacking,` |
|     2870 | 1502 | `			nBacking == MEMOBJ_INT ? "int" : "string");` |
|     5745 | 1503 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1504 | `			return rc;` |
|        - | 1505 | `		}` |
|     2870 | 1506 | `	}` |
|    97585 | 1507 | `	for( n = 0 ; n < nCase ; n++ ){` |
|        - | 1508 | `		ph7_class_attr *pAttr;` |
|        - | 1509 | `		SyString sCase;` |
|    80365 | 1510 | `		SyStringInitFromBuf(&sCase,aCase[n].zName,SyStrlen(aCase[n].zName));` |
|    80365 | 1511 | `		pAttr = PH7_NewClassAttr(&(*pVm),&sCase,0,PH7_CLASS_PROT_PUBLIC,` |
|        - | 1512 | `			PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|    80365 | 1513 | `		if( pAttr == 0 ){` |
|      ! 0 | 1514 | `			return SXERR_MEM;` |
|        - | 1515 | `		}` |
|    80365 | 1516 | `		pAttr->pDeclClass = pClass;` |
|        - | 1517 | `		/* The backing literal where a compiled case carries byte-code. */` |
|    80365 | 1518 | `		if( nBacking != 0 ){` |
|    11485 | 1519 | `			pAttr->pNativeValue = &aCase[n].sValue;` |
|     5740 | 1520 | `		}` |
|    80365 | 1521 | `		rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|    80365 | 1522 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1523 | `			return rc;` |
|        - | 1524 | `		}` |
|        - | 1525 | `		/* Declaration order, which is the order cases() reports. */` |
|    80365 | 1526 | `		SySetPut(&pClass->aEnumCases,(const void *)&pAttr);` |
|    40185 | 1527 | `	}` |
|    17225 | 1528 | `	for( n = 0 ; n < nMethod ; n++ ){` |
|      ! 0 | 1529 | `		rc = PH7_NativeClassInstallMethod(&(*pVm),pClass,&aMethod[n],0);` |
|      ! 0 | 1530 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1531 | `			return rc;` |
|        - | 1532 | `		}` |
|      ! 0 | 1533 | `	}` |
|    17225 | 1534 | `	rc = PH7_InstallEnumInterfaceMethods(&(*pVm),pClass);` |
|    17225 | 1535 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1536 | `		return rc;` |
|        - | 1537 | `	}` |
|    17225 | 1538 | `	rc = PH7_VmInstallClass(&(*pVm),pClass);` |
|    17225 | 1539 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1540 | `		return rc;` |
|        - | 1541 | `	}` |
|        - | 1542 | ``	/* php 8.1: every enum satisfies `instanceof UnitEnum`, a backed one`` |
|        - | 1543 | ``	 * `BackedEnum` too. Attached AFTER the methods, so PH7_ClassImplement's`` |
|        - | 1544 | `	 * abstract stubbing finds cases()/from()/tryFrom() already declared. */` |
|    17225 | 1545 | `	pIface = NativeLookupClass(&(*pVm),"UnitEnum");` |
|    17225 | 1546 | `	if( pIface == 0 ){` |
|      ! 0 | 1547 | `		return SXERR_NOTFOUND;` |
|        - | 1548 | `	}` |
|    17225 | 1549 | `	rc = PH7_ClassImplement(pClass,pIface);` |
|    17225 | 1550 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1551 | `		return rc;` |
|        - | 1552 | `	}` |
|    17225 | 1553 | `	if( nBacking != 0 ){` |
|     5745 | 1554 | `		pIface = NativeLookupClass(&(*pVm),"BackedEnum");` |
|     5745 | 1555 | `		if( pIface == 0 ){` |
|      ! 0 | 1556 | `			return SXERR_NOTFOUND;` |
|        - | 1557 | `		}` |
|     5745 | 1558 | `		rc = PH7_ClassImplement(pClass,pIface);` |
|     5745 | 1559 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1560 | `			return rc;` |
|        - | 1561 | `		}` |
|     2870 | 1562 | `	}` |
|    17225 | 1563 | `	return VmMountUserClass(&(*pVm),pClass);` |
|     8615 | 1564 | `}` |
|        - | 1565 | `/*` |
|        - | 1566 | ` * ---------------------------------------------------------------------------` |
|        - | 1567 | ` * InternalIterator.` |
|        - | 1568 | ` *` |
|        - | 1569 | ` * A native IteratorAggregate cannot answer a Generator: a PHP generator IS its` |
|        - | 1570 | ` * byte-code, and a C body has none. php never answers one either -- its internal` |
|        - | 1571 | ` * aggregates hand back an InternalIterator wrapping the iterator their class` |
|        - | 1572 | ` * declared -- so PHL declares that class once, here, and every native aggregate` |
|        - | 1573 | ` * reaches it through ph7_class::pIterVtab.` |
|        - | 1574 | ` *` |
|        - | 1575 | ` * The cursor lives entirely in the iterator's own private slots. A vtable states` |
|        - | 1576 | ` * only how to REACH a position; reading it back is the same three methods for` |
|        - | 1577 | ` * everyone.` |
|        - | 1578 | ` * ---------------------------------------------------------------------------` |
|        - | 1579 | ` */` |
|        - | 1580 | `/*` |
|        - | 1581 | ` * The walk THIS iterator was made for: the vtable of the aggregate it holds,` |
|        - | 1582 | ` * looked up ALONG THE BASE CHAIN. A subclass of a native aggregate inherits the` |
|        - | 1583 | ` * walk the way it inherits the getIterator() that reaches it -- without this a` |
|        - | 1584 | `` * `class P extends DatePeriod {}` (or DOMNodeList, WeakMap, PDOStatement,`` |
|        - | 1585 | ` * FilesystemIterator) answered a real InternalIterator that yielded NOTHING, so` |
|        - | 1586 | ` * every foreach over one was silently empty.` |
|        - | 1587 | ` */` |
|     6034 | 1588 | `static const PH7_NativeIterVtab * NativeIterVtab(ph7_class_instance *pIt)` |
|        2 | 1589 | `{` |
|     6036 | 1590 | `	ph7_class_instance *pSrc = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);` |
|        - | 1591 | `	ph7_class *pClass;` |
|     6256 | 1592 | `	for( pClass = pSrc ? pSrc->pClass : 0 ; pClass ; pClass = pClass->pBase ){` |
|     6256 | 1593 | `		if( pClass->pIterVtab ){` |
|     6036 | 1594 | `			return pClass->pIterVtab;` |
|        - | 1595 | `		}` |
|      111 | 1596 | `	}` |
|      ! 0 | 1597 | `	return 0;` |
|     3019 | 1598 | `}` |
|        - | 1599 | `/* Hand the cursor back to the aggregate, for a class that shows its walk as one` |
|        - | 1600 | ` * of its own properties (see PH7_NativeIterVtab::xPublish). Every InternalIterator` |
|        - | 1601 | ` * method calls this -- php's aggregate is written from the iterator's methods, not` |
|        - | 1602 | ` * from the walk, so a getIterator() nobody has touched yet leaves it alone. */` |
|     2448 | 1603 | `static void NativeIterPublish(ph7_vm *pVm,ph7_class_instance *pIt)` |
|        2 | 1604 | `{` |
|     2450 | 1605 | `	const PH7_NativeIterVtab *pVtab = NativeIterVtab(pIt);` |
|     2450 | 1606 | `	if( pVtab && pVtab->xPublish ){` |
|      949 | 1607 | `		pVtab->xPublish(&(*pVm),pIt);` |
|      474 | 1608 | `	}` |
|     2450 | 1609 | `}` |
|        - | 1610 | `/* May this iterator be walked? See PH7_NativeIterVtab::xGuard -- the aggregate` |
|        - | 1611 | ` * gets to refuse at each of the five methods, which is where php refuses. */` |
|     2462 | 1612 | `static int NativeIterRefused(ph7_context *pCtx,ph7_class_instance *pIt)` |
|        2 | 1613 | `{` |
|     2464 | 1614 | `	const PH7_NativeIterVtab *pVtab = NativeIterVtab(pIt);` |
|     2464 | 1615 | `	return (pVtab && pVtab->xGuard) ? pVtab->xGuard(pCtx,pIt) : 0;` |
|        2 | 1616 | `}` |
|      268 | 1617 | `static int vm_builtin_InternalIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 1618 | `{` |
|      270 | 1619 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|        - | 1620 | `	const PH7_NativeIterVtab *pVtab;` |
|      134 | 1621 | `	SXUNUSED(nArg);` |
|      134 | 1622 | `	SXUNUSED(apArg);` |
|      270 | 1623 | `	if( pThis == 0 ){` |
|      ! 0 | 1624 | `		return PH7_OK;` |
|        - | 1625 | `	}` |
|      270 | 1626 | `	if( NativeIterRefused(pCtx,pThis) ){` |
|       15 | 1627 | `		return PH7_OK;` |
|        - | 1628 | `	}` |
|      256 | 1629 | `	pVtab = NativeIterVtab(pThis);` |
|      256 | 1630 | `	if( pVtab == 0 \|\| pVtab->xRewind == 0 ){` |
|      ! 0 | 1631 | `		PH7_NativeSetAttrBool(pCtx->pVm,pThis,PH7_NATIVE_IT_DONE,1);` |
|      ! 0 | 1632 | `		return PH7_OK;` |
|        - | 1633 | `	}` |
|      256 | 1634 | `	pVtab->xRewind(pCtx->pVm,pThis);` |
|      256 | 1635 | `	NativeIterPublish(pCtx->pVm,pThis);` |
|      256 | 1636 | `	return PH7_OK;` |
|      136 | 1637 | `}` |
|      576 | 1638 | `static int vm_builtin_InternalIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 1639 | `{` |
|      578 | 1640 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|        - | 1641 | `	const PH7_NativeIterVtab *pVtab;` |
|      288 | 1642 | `	SXUNUSED(nArg);` |
|      288 | 1643 | `	SXUNUSED(apArg);` |
|      578 | 1644 | `	if( pThis && NativeIterRefused(pCtx,pThis) ){` |
|      ! 0 | 1645 | `		return PH7_OK;` |
|        - | 1646 | `	}` |
|      578 | 1647 | `	if( pThis == 0 \|\| PH7_NativeAttrTruthy(pThis,PH7_NATIVE_IT_DONE) ){` |
|      ! 0 | 1648 | `		return PH7_OK;` |
|        - | 1649 | `	}` |
|      578 | 1650 | `	pVtab = NativeIterVtab(pThis);` |
|      578 | 1651 | `	if( pVtab == 0 \|\| pVtab->xNext == 0 ){` |
|      ! 0 | 1652 | `		PH7_NativeSetAttrBool(pCtx->pVm,pThis,PH7_NATIVE_IT_DONE,1);` |
|      ! 0 | 1653 | `		return PH7_OK;` |
|        - | 1654 | `	}` |
|      578 | 1655 | `	pVtab->xNext(pCtx->pVm,pThis);` |
|      578 | 1656 | `	NativeIterPublish(pCtx->pVm,pThis);` |
|      578 | 1657 | `	return PH7_OK;` |
|      290 | 1658 | `}` |
|      822 | 1659 | `static int vm_builtin_InternalIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 1660 | `{` |
|      824 | 1661 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      411 | 1662 | `	SXUNUSED(nArg);` |
|      411 | 1663 | `	SXUNUSED(apArg);` |
|      824 | 1664 | `	if( pThis && NativeIterRefused(pCtx,pThis) ){` |
|      ! 0 | 1665 | `		return PH7_OK;` |
|        - | 1666 | `	}` |
|      824 | 1667 | `	if( pThis ){` |
|      824 | 1668 | `		NativeIterPublish(pCtx->pVm,pThis);` |
|      411 | 1669 | `	}` |
|      824 | 1670 | `	ph7_result_bool(pCtx,pThis != 0 && !PH7_NativeAttrTruthy(pThis,PH7_NATIVE_IT_DONE));` |
|      824 | 1671 | `	return PH7_OK;` |
|      413 | 1672 | `}` |
|        - | 1673 | `/* current() and key() answer the slots the walk left behind -- and NULL once it` |
|        - | 1674 | ` * is over, which is what php's exhausted InternalIterator answers too. */` |
|      796 | 1675 | `static int NativeIterReadSlot(ph7_context *pCtx,const char *zSlot)` |
|        2 | 1676 | `{` |
|      798 | 1677 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|        - | 1678 | `	ph7_value *pVal;` |
|      798 | 1679 | `	if( pThis && NativeIterRefused(pCtx,pThis) ){` |
|      ! 0 | 1680 | `		return PH7_OK;` |
|        - | 1681 | `	}` |
|      798 | 1682 | `	if( pThis ){` |
|      798 | 1683 | `		NativeIterPublish(pCtx->pVm,pThis);` |
|      398 | 1684 | `	}` |
|      798 | 1685 | `	if( pThis == 0 \|\| PH7_NativeAttrTruthy(pThis,PH7_NATIVE_IT_DONE) ){` |
|      ! 0 | 1686 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1687 | `		return PH7_OK;` |
|        - | 1688 | `	}` |
|      798 | 1689 | `	pVal = PH7_NativeAttr(pThis,zSlot);` |
|      798 | 1690 | `	if( pVal ){` |
|      798 | 1691 | `		ph7_result_value(pCtx,pVal);` |
|      398 | 1692 | `	}` |
|      798 | 1693 | `	return PH7_OK;` |
|      400 | 1694 | `}` |
|      572 | 1695 | `static int vm_builtin_InternalIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 1696 | `{` |
|      286 | 1697 | `	SXUNUSED(nArg);` |
|      286 | 1698 | `	SXUNUSED(apArg);` |
|      574 | 1699 | `	return NativeIterReadSlot(pCtx,PH7_NATIVE_IT_CUR);` |
|        2 | 1700 | `}` |
|      224 | 1701 | `static int vm_builtin_InternalIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 1702 | `{` |
|      112 | 1703 | `	SXUNUSED(nArg);` |
|      112 | 1704 | `	SXUNUSED(apArg);` |
|      226 | 1705 | `	return NativeIterReadSlot(pCtx,PH7_NATIVE_IT_KEY);` |
|        2 | 1706 | `}` |
|        - | 1707 | `/* InternalIterator::__construct() — private in php, and never reached from PHP:` |
|        - | 1708 | ` * PH7_NativeIteratorNew builds the instance directly. */` |
|      ! 0 | 1709 | `static int vm_builtin_InternalIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 1710 | `{` |
|      ! 0 | 1711 | `	SXUNUSED(nArg);` |
|      ! 0 | 1712 | `	SXUNUSED(apArg);` |
|      ! 0 | 1713 | `	SXUNUSED(pCtx);` |
|      ! 0 | 1714 | `	return PH7_OK;` |
|      ! 0 | 1715 | `}` |
|        - | 1716 | `/*` |
|        - | 1717 | ` * The iterator a native getIterator() answers: bound to its aggregate and already` |
|        - | 1718 | ` * positioned, because php's is valid() before the first rewind().` |
|        - | 1719 | ` */` |
|      294 | 1720 | `PH7_PRIVATE ph7_class_instance * PH7_NativeIteratorNew(ph7_vm *pVm,ph7_class_instance *pSrc)` |
|        2 | 1721 | `{` |
|      296 | 1722 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),"InternalIterator",` |
|        - | 1723 | `		sizeof("InternalIterator")-1,FALSE,0);` |
|        - | 1724 | `	ph7_class_instance *pIt;` |
|        - | 1725 | `	const PH7_NativeIterVtab *pVtab;` |
|      296 | 1726 | `	if( pClass == 0 \|\| pSrc == 0 ){` |
|      ! 0 | 1727 | `		return 0;` |
|        - | 1728 | `	}` |
|      296 | 1729 | `	pIt = PH7_NewClassInstance(&(*pVm),pClass);` |
|      296 | 1730 | `	if( pIt == 0 ){` |
|      ! 0 | 1731 | `		return 0;` |
|        - | 1732 | `	}` |
|      296 | 1733 | `	PH7_NativeSetAttrObj(&(*pVm),pIt,PH7_NATIVE_IT_SRC,pSrc);` |
|      296 | 1734 | `	PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|      296 | 1735 | `	pVtab = NativeIterVtab(pIt);` |
|      296 | 1736 | `	if( pVtab && pVtab->xRewind ){` |
|      296 | 1737 | `		pVtab->xRewind(&(*pVm),pIt);` |
|      147 | 1738 | `	}` |
|      296 | 1739 | `	return pIt;` |
|      149 | 1740 | `}` |
|     5740 | 1741 | `PH7_PRIVATE sxi32 PH7_VmInstallNativeIterator(ph7_vm *pVm)` |
|        5 | 1742 | `{` |
|        - | 1743 | `	static const PH7_NativePropDef aProp[] = {` |
|        - | 1744 | `		{ PH7_NATIVE_IT_SRC,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|        - | 1745 | `		{ PH7_NATIVE_IT_CUR,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|        - | 1746 | `		{ PH7_NATIVE_IT_KEY,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|        - | 1747 | `		{ PH7_NATIVE_IT_POS,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,  0, 0, 0.0 }, 0 },` |
|        - | 1748 | `		{ PH7_NATIVE_IT_AUX,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,  0, 0, 0.0 }, 0 },` |
|        - | 1749 | `		{ PH7_NATIVE_IT_DONE, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_BOOL, 1, 0, 0.0 }, 0 },` |
|        - | 1750 | `	};` |
|        - | 1751 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|        - | 1752 | `		{ "__construct", PH7_MOD_PRIVATE, "", "", vm_builtin_InternalIterator_construct },` |
|        - | 1753 | `		{ "current",     PH7_MOD_PUBLIC, "", "mixed", vm_builtin_InternalIterator_current },` |
|        - | 1754 | `		{ "key",         PH7_MOD_PUBLIC, "", "mixed", vm_builtin_InternalIterator_key },` |
|        - | 1755 | `		{ "next",        PH7_MOD_PUBLIC, "", "void",  vm_builtin_InternalIterator_next },` |
|        - | 1756 | `		{ "valid",       PH7_MOD_PUBLIC, "", "bool",  vm_builtin_InternalIterator_valid },` |
|        - | 1757 | `		{ "rewind",      PH7_MOD_PUBLIC, "", "void",  vm_builtin_InternalIterator_rewind },` |
|        - | 1758 | `	};` |
|        - | 1759 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|        - | 1760 | `		{ "InternalIterator", 0, 0, PH7_CLASS_FINAL,` |
|        - | 1761 | `		  aMethod, SX_ARRAYSIZE(aMethod), 0, 0, aProp, SX_ARRAYSIZE(aProp), 0, 0, 0 },` |
|        - | 1762 | `	};` |
|        - | 1763 | `	ph7_class *pIt,*pIterator;` |
|        - | 1764 | `	sxi32 rc;` |
|     5745 | 1765 | `	rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5745 | 1766 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1767 | `		return rc;` |
|        - | 1768 | `	}` |
|        - | 1769 | ``	/* `implements Iterator` only NOW: PH7_ClassImplement stubs every interface`` |
|        - | 1770 | `	 * method the class does not already declare as ABSTRACT, so attaching it` |
|        - | 1771 | `	 * through the spec would have made InternalIterator uninstantiable. */` |
|     5745 | 1772 | `	pIt = NativeLookupClass(&(*pVm),"InternalIterator");` |
|     5745 | 1773 | `	pIterator = NativeLookupClass(&(*pVm),"Iterator");` |
|     5745 | 1774 | `	if( pIt == 0 \|\| pIterator == 0 ){` |
|      ! 0 | 1775 | `		return SXERR_NOTFOUND;` |
|        - | 1776 | `	}` |
|     5745 | 1777 | `	return PH7_ClassImplement(pIt,pIterator);` |
|     2875 | 1778 | `}` |
|        - | 1779 |  |
