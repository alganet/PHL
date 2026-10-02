# src/ph7/oo_native.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 957/1062 lines (90.11%)

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
|  9980476 |   36 | `PH7_PRIVATE void PH7_NativeLiteralValue(ph7_vm *pVm,const void *pLiteral,ph7_value *pOut)` |
|        5 |   37 | `{` |
|  9980481 |   38 | `	const PH7_NativeConstDef *pLit = (const PH7_NativeConstDef *)pLiteral;` |
|        - |   39 | `	/* Every PH7_MemObjInitFrom* below SyZeros the whole value, nIdx included — and` |
|        - |   40 | `	 * a CONSTANT's or STATIC's slot is addressed by that index afterwards` |
|        - |   41 | ``	 * (`pAttr->nIdx = pMemObj->nIdx`). Losing it pointed every native class`` |
|        - |   42 | `	 * constant at slot 0, which reads as $GLOBALS. The instance-property path was` |
|        - |   43 | `	 * unaffected (it records the index before calling this), which is why nothing` |
|        - |   44 | `	 * saw it until the date family declared the first native constants. */` |
|  9980481 |   45 | `	sxu32 nSlot = pOut->nIdx;` |
|  9980481 |   46 | `	switch( pLit->iType ){` |
|  1261056 |   47 | `		case PH7_NATIVE_VAL_INT:` |
|  2522017 |   48 | `			PH7_MemObjInitFromInt(&(*pVm),pOut,pLit->iValue);` |
|  2522017 |   49 | `			break;` |
|     2050 |   50 | `		case PH7_NATIVE_VAL_BOOL:` |
|     4102 |   51 | `			PH7_MemObjInitFromBool(&(*pVm),pOut,(sxi32)pLit->iValue);` |
|     4102 |   52 | `			break;` |
|  2208049 |   53 | `		case PH7_NATIVE_VAL_STRING: {` |
|        - |   54 | `			SyString sLit;` |
|  4415904 |   55 | `			SyStringInitFromBuf(&sLit,pLit->zValue,SyStrlen(pLit->zValue));` |
|  4415904 |   56 | `			PH7_MemObjInitFromString(&(*pVm),pOut,&sLit);` |
|  4415904 |   57 | `			break;` |
|        - |   58 | `		}` |
|        - |   59 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      306 |   60 | `		case PH7_NATIVE_VAL_DOUBLE:` |
|      615 |   61 | `			PH7_MemObjInitFromReal(&(*pVm),pOut,pLit->rValue);` |
|      615 |   62 | `			break;` |
|        - |   63 | `#endif` |
|   733098 |   64 | `		case PH7_NATIVE_VAL_ARRAY: {` |
|        - |   65 | ``			/* The empty array — php's `private array $trace = [];`. Each instance`` |
|        - |   66 | `			 * needs its OWN map (the exception's trace is written per throw), so` |
|        - |   67 | `			 * this allocates rather than sharing one. */` |
|  1466146 |   68 | `			ph7_hashmap *pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|  1466146 |   69 | `			if( pMap == 0 ){` |
|      ! 0 |   70 | `				PH7_MemObjInit(&(*pVm),pOut);` |
|      ! 0 |   71 | `			}else{` |
|  1466146 |   72 | `				PH7_MemObjInitFromArray(&(*pVm),pOut,pMap);` |
|        - |   73 | `			}` |
|  1466146 |   74 | `			break;` |
|        - |   75 | `		}` |
|   786260 |   76 | `		default:` |
|  1571720 |   77 | `			PH7_MemObjInit(&(*pVm),pOut);` |
|  1571715 |   78 | `			break;` |
|        - |   79 | `	}` |
|  9980481 |   80 | `	pOut->nIdx = nSlot;` |
|  9980481 |   81 | `}` |
|        - |   82 | `/*` |
|        - |   83 | ` * Write a declared property of an instance from C.` |
|        - |   84 | ` *` |
|        - |   85 | ` * Every native class that hands an OBJECT back to PHP has to fill one in, and the` |
|        - |   86 | ` * write has to go through the instance's own slot table (hAttr -> VmClassAttr ->` |
|        - |   87 | ` * pVm->aMemObj) rather than the class declaration, or the value lands nowhere.` |
|        - |   88 | ` * Silently does nothing for a name the class does not declare -- callers pass` |
|        - |   89 | ` * literals from their own spec table, so a miss is a build error, not input.` |
|        - |   90 | ` */` |
|     9669 |   91 | `PH7_PRIVATE void PH7_NativeSetProp(ph7_vm *pVm,ph7_class_instance *pObj,` |
|        - |   92 | `	const char *zProp,sxu32 nProp,ph7_value *pSrcVal)` |
|        5 |   93 | `{` |
|        - |   94 | `	SyHashEntry *pEntry;` |
|        - |   95 | `	VmClassAttr *pVmAttr;` |
|        - |   96 | `	ph7_value *pSlot;` |
|     9674 |   97 | `	PH7_NativeMaterializeLazy(&(*pVm),pObj);` |
|     9674 |   98 | `	pEntry = SyHashGet(&pObj->hAttr,(const void *)zProp,nProp);` |
|     9674 |   99 | `	if( pEntry == 0 ){` |
|      ! 0 |  100 | `		return;` |
|        - |  101 | `	}` |
|     9674 |  102 | `	pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     9674 |  103 | `	pSlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pVmAttr->nIdx);` |
|     9674 |  104 | `	if( pSlot == 0 ){` |
|      ! 0 |  105 | `		return;` |
|        - |  106 | `	}` |
|     9674 |  107 | `	PH7_MemObjStore(pSrcVal,pSlot);` |
|     9674 |  108 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|     4834 |  109 | `}` |
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
|  1934709 |  122 | `PH7_PRIVATE ph7_value * PH7_NativeAttr(ph7_class_instance *pObj,const char *zName)` |
|        5 |  123 | `{` |
|        - |  124 | `	SyString sName;` |
|  1934714 |  125 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|  1934714 |  126 | `	return PH7_ClassInstanceFetchAttr(pObj,&sName);` |
|        5 |  127 | `}` |
|        - |  128 | `/*` |
|        - |  129 | ` * Has this declared slot never been written? A PH7_NATIVE_VAL_NONE property is` |
|        - |  130 | `` * php's `public int $id;` — typed, with no default — and reading one before the`` |
|        - |  131 | ` * class has filled it is php's "must not be accessed before initialization".` |
|        - |  132 | ` * The property-read opcode raises that itself; a C body reading the slot` |
|        - |  133 | ` * directly has to ask.` |
|        - |  134 | ` */` |
|       58 |  135 | `PH7_PRIVATE int PH7_NativeAttrIsUninit(ph7_class_instance *pObj,const char *zName)` |
|        2 |  136 | `{` |
|       60 |  137 | `	SyHashEntry *pEntry = pObj ? SyHashGet(&pObj->hAttr,(const void *)zName,SyStrlen(zName)) : 0;` |
|       60 |  138 | `	return pEntry != 0` |
|       58 |  139 | `		&& (((VmClassAttr *)pEntry->pUserData)->iState & VM_CLASS_ATTR_UNINIT) != 0;` |
|        2 |  140 | `}` |
|        - |  141 | `/* Read an int slot WITHOUT converting it: ph7_value_to_int64() converts the` |
|        - |  142 | ` * attribute in place, which would rewrite the object's own state. */` |
|    90659 |  143 | `PH7_PRIVATE sxi64 PH7_NativeAttrInt(ph7_class_instance *pObj,const char *zName)` |
|        5 |  144 | `{` |
|    90664 |  145 | `	ph7_value *pVal = PH7_NativeAttr(pObj,zName);` |
|    90664 |  146 | `	if( pVal && (pVal->iFlags & MEMOBJ_INT) ){` |
|    90662 |  147 | `		return pVal->x.iVal;` |
|        - |  148 | `	}` |
|        3 |  149 | `	return 0;` |
|    45337 |  150 | `}` |
|        - |  151 | `/* Borrow a string slot's bytes (empty when it holds anything else). */` |
|    61144 |  152 | `PH7_PRIVATE void PH7_NativeAttrStr(ph7_class_instance *pObj,const char *zName,` |
|        - |  153 | `	const char **pzOut,int *pnOut)` |
|        5 |  154 | `{` |
|    61149 |  155 | `	ph7_value *pVal = PH7_NativeAttr(pObj,zName);` |
|    61149 |  156 | `	*pzOut = "";` |
|    61149 |  157 | `	*pnOut = 0;` |
|    61149 |  158 | `	if( pVal && (pVal->iFlags & MEMOBJ_STRING) ){` |
|    60875 |  159 | `		*pzOut = (const char *)SyBlobData(&pVal->sBlob);` |
|    60875 |  160 | `		*pnOut = (int)SyBlobLength(&pVal->sBlob);` |
|    30417 |  161 | `	}` |
|    61149 |  162 | `}` |
|        - |  163 | `/* The object stored in a slot, or NULL when it holds anything else. */` |
|    44638 |  164 | `PH7_PRIVATE ph7_class_instance * PH7_NativeAttrObj(ph7_class_instance *pObj,const char *zName)` |
|        5 |  165 | `{` |
|    44643 |  166 | `	ph7_value *pVal = PH7_NativeAttr(pObj,zName);` |
|    44643 |  167 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     7337 |  168 | `		return 0;` |
|        - |  169 | `	}` |
|    37311 |  170 | `	return (ph7_class_instance *)pVal->x.pOther;` |
|    22322 |  171 | `}` |
|        - |  172 | `/* Truth of a bool/int slot, again without converting it. */` |
|     3432 |  173 | `PH7_PRIVATE int PH7_NativeAttrTruthy(ph7_class_instance *pObj,const char *zName)` |
|        4 |  174 | `{` |
|     3436 |  175 | `	ph7_value *pVal = PH7_NativeAttr(pObj,zName);` |
|     3436 |  176 | `	if( pVal && (pVal->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|     3436 |  177 | `		return pVal->x.iVal != 0;` |
|        - |  178 | `	}` |
|      ! 0 |  179 | `	return 0;` |
|     1720 |  180 | `}` |
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
|  1602549 |  193 | `static void NativeAttrMarkInit(ph7_class_instance *pObj,const char *zName)` |
|        5 |  194 | `{` |
|  1602554 |  195 | `	SyHashEntry *pEntry = SyHashGet(&pObj->hAttr,(const void *)zName,SyStrlen(zName));` |
|  1602554 |  196 | `	if( pEntry ){` |
|  1602554 |  197 | `		((VmClassAttr *)pEntry->pUserData)->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|   801241 |  198 | `	}` |
|  1602554 |  199 | `}` |
|        - |  200 | `/*` |
|        - |  201 | ` * The slot fetch on the WRITE side. A class whose php-visible properties are LAZY` |
|        - |  202 | ` * has none of them on the object until a C body fills one, and that first write is` |
|        - |  203 | ` * what installs the set -- which is php's constructor writing its struct into the` |
|        - |  204 | ` * property table. Every native writer goes through here so the bookkeeping lives` |
|        - |  205 | ` * with the write rather than with one of the ways of writing.` |
|        - |  206 | ` */` |
|  1602549 |  207 | `static ph7_value * NativeAttrForWrite(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName)` |
|        5 |  208 | `{` |
|        - |  209 | `	ph7_value *pSlot;` |
|  1602554 |  210 | `	PH7_NativeMaterializeLazy(&(*pVm),pObj);` |
|  1602554 |  211 | `	pSlot = PH7_NativeAttr(pObj,zName);` |
|  1602554 |  212 | `	if( pSlot == 0 && pObj ){` |
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
|      124 |  224 | `					pSlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pVmAttr->nIdx);` |
|       61 |  225 | `				}` |
|       61 |  226 | `			}` |
|       61 |  227 | `		}` |
|       61 |  228 | `	}` |
|  1602554 |  229 | `	return pSlot;` |
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
|    57068 |  247 | `PH7_PRIVATE void PH7_NativeSetAttrInt(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,sxi64 iVal)` |
|        5 |  248 | `{` |
|    57073 |  249 | `	ph7_value *pSlot = NativeAttrForWrite(&(*pVm),pObj,zName);` |
|        - |  250 | `	ph7_value sVal;` |
|    57073 |  251 | `	if( pSlot == 0 ){` |
|      ! 0 |  252 | `		return;` |
|        - |  253 | `	}` |
|    57073 |  254 | `	PH7_MemObjInitFromInt(&(*pVm),&sVal,iVal);` |
|    57073 |  255 | `	PH7_MemObjStore(&sVal,pSlot);` |
|    57073 |  256 | `	PH7_MemObjRelease(&sVal);` |
|    57073 |  257 | `	NativeAttrMarkInit(pObj,zName);` |
|    28540 |  258 | `}` |
|        - |  259 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      614 |  260 | `PH7_PRIVATE void PH7_NativeSetAttrReal(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,ph7_real rVal)` |
|        3 |  261 | `{` |
|      617 |  262 | `	ph7_value *pSlot = NativeAttrForWrite(&(*pVm),pObj,zName);` |
|        - |  263 | `	ph7_value sVal;` |
|      617 |  264 | `	if( pSlot == 0 ){` |
|      ! 0 |  265 | `		return;` |
|        - |  266 | `	}` |
|      617 |  267 | `	PH7_MemObjInitFromReal(&(*pVm),&sVal,rVal);` |
|      617 |  268 | `	PH7_MemObjStore(&sVal,pSlot);` |
|      617 |  269 | `	PH7_MemObjRelease(&sVal);` |
|      617 |  270 | `	NativeAttrMarkInit(pObj,zName);` |
|      310 |  271 | `}` |
|        - |  272 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|  1528788 |  273 | `PH7_PRIVATE void PH7_NativeSetAttrStr(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,` |
|        - |  274 | `	const char *zVal,int nVal)` |
|        5 |  275 | `{` |
|  1528793 |  276 | `	ph7_value *pSlot = NativeAttrForWrite(&(*pVm),pObj,zName);` |
|        - |  277 | `	ph7_value sVal;` |
|        - |  278 | `	SyString sStr;` |
|  1528793 |  279 | `	if( pSlot == 0 ){` |
|      ! 0 |  280 | `		return;` |
|        - |  281 | `	}` |
|  1528793 |  282 | `	SyStringInitFromBuf(&sStr,zVal,nVal);` |
|  1528793 |  283 | `	PH7_MemObjInitFromString(&(*pVm),&sVal,&sStr);` |
|  1528793 |  284 | `	PH7_MemObjStore(&sVal,pSlot);` |
|  1528793 |  285 | `	PH7_MemObjRelease(&sVal);	NativeAttrMarkInit(pObj,zName);` |
|   764366 |  286 | `}` |
|     4063 |  287 | `PH7_PRIVATE void PH7_NativeSetAttrBool(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,int bVal)` |
|        5 |  288 | `{` |
|     4068 |  289 | `	ph7_value *pSlot = NativeAttrForWrite(&(*pVm),pObj,zName);` |
|        - |  290 | `	ph7_value sVal;` |
|     4068 |  291 | `	if( pSlot == 0 ){` |
|      ! 0 |  292 | `		return;` |
|        - |  293 | `	}` |
|     4068 |  294 | `	PH7_MemObjInitFromBool(&(*pVm),&sVal,bVal);` |
|     4068 |  295 | `	PH7_MemObjStore(&sVal,pSlot);` |
|     4068 |  296 | `	PH7_MemObjRelease(&sVal);	NativeAttrMarkInit(pObj,zName);` |
|     2035 |  297 | `}` |
|        - |  298 | `/* Store an object in a slot, or NULL to clear it. PH7_MemObjStore takes the` |
|        - |  299 | ` * reference the slot needs, so the temp never holds one of its own. */` |
|    12016 |  300 | `PH7_PRIVATE void PH7_NativeSetAttrObj(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,` |
|        - |  301 | `	ph7_class_instance *pVal)` |
|        5 |  302 | `{` |
|    12021 |  303 | `	ph7_value *pSlot = NativeAttrForWrite(&(*pVm),pObj,zName);` |
|        - |  304 | `	ph7_value sVal;` |
|    12021 |  305 | `	if( pSlot == 0 ){` |
|      ! 0 |  306 | `		return;` |
|        - |  307 | `	}` |
|    12021 |  308 | `	PH7_MemObjInit(&(*pVm),&sVal);` |
|    12021 |  309 | `	if( pVal ){` |
|    11367 |  310 | `		sVal.x.pOther = pVal;` |
|    11367 |  311 | `		sVal.iFlags = MEMOBJ_OBJ;` |
|     5681 |  312 | `	}` |
|    12021 |  313 | `	PH7_MemObjStore(&sVal,pSlot);` |
|    12021 |  314 | `	NativeAttrMarkInit(pObj,zName);` |
|     6013 |  315 | `}` |
|        - |  316 | `/*` |
|        - |  317 | ` * Hand an instance back as a native call's result, dropping the reference` |
|        - |  318 | ` * PH7_NewClassInstance/PH7_CloneClassInstance handed the caller.` |
|        - |  319 | ` */` |
|     5918 |  320 | `PH7_PRIVATE void PH7_NativeResultObject(ph7_context *pCtx,ph7_class_instance *pObj)` |
|        5 |  321 | `{` |
|        - |  322 | `	ph7_value sRes;` |
|     5923 |  323 | `	PH7_MemObjInit(pCtx->pVm,&sRes);` |
|     5923 |  324 | `	sRes.x.pOther = pObj;` |
|     5923 |  325 | `	sRes.iFlags = MEMOBJ_OBJ;` |
|     5923 |  326 | `	ph7_result_value(pCtx,&sRes);   /* takes its own reference */` |
|     5923 |  327 | `	PH7_ClassInstanceUnref(pObj);` |
|     5923 |  328 | `}` |
|        - |  329 | `/*` |
|        - |  330 | ` * Resolve a class by name for the builder's own use (parents and interfaces).` |
|        - |  331 | ` * Autoload is deliberately NOT triggered: these run at VM init, where the only` |
|        - |  332 | ` * classes that can exist are the ones installed before this call, and a missing` |
|        - |  333 | ` * name is a build-order bug in the engine, not a userland lookup.` |
|        - |  334 | ` */` |
|  1537450 |  335 | `static ph7_class * NativeLookupClass(ph7_vm *pVm,const char *zName)` |
|        5 |  336 | `{` |
|  1537455 |  337 | `	return PH7_VmExtractClass(&(*pVm),zName,(sxu32)SyStrlen(zName),FALSE,0);` |
|        5 |  338 | `}` |
|        - |  339 | `/*` |
|        - |  340 | ` * Translate the builder's PH7_MOD_* modifier bits into the protection level and` |
|        - |  341 | ` * the attribute/method flag word the class structures actually store.` |
|        - |  342 | ` */` |
| 17276762 |  343 | `static sxi32 NativeProtection(sxi32 iMods)` |
|        5 |  344 | `{` |
| 17276767 |  345 | `	if( iMods & PH7_MOD_PRIVATE ){` |
|  1268005 |  346 | `		return PH7_CLASS_PROT_PRIVATE;` |
|        - |  347 | `	}` |
| 16008767 |  348 | `	if( iMods & PH7_MOD_PROTECTED ){` |
|   253605 |  349 | `		return PH7_CLASS_PROT_PROTECTED;` |
|        - |  350 | `	}` |
| 15755167 |  351 | `	return PH7_CLASS_PROT_PUBLIC;` |
|  8626396 |  352 | `}` |
|        - |  353 | `/*` |
|        - |  354 | ` * Attach one C-bodied method to an already-created class.` |
|        - |  355 | ` *` |
|        - |  356 | ` * The ph7_user_func built here is NOT registered in pVm->hHostFunction: it hangs` |
|        - |  357 | ` * off the method and is reachable only by dispatching the method, which is exactly` |
|        - |  358 | ` * the property that retires the global thunks. Its sName is the php-facing` |
|        - |  359 | `` * `Class::method`, so an ArgumentCountError raised at the OP_CALL choke point`` |
|        - |  360 | ` * names what a php user would recognise.` |
|        - |  361 | ` */` |
| 11158662 |  362 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallMethod(` |
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
| 11158667 |  376 | `	SyStringInitFromBuf(&sName,pDef->zName,SyStrlen(pDef->zName));` |
| 11158667 |  377 | `	iFuncFlags = VM_FUNC_NATIVE;` |
| 11158667 |  378 | `	if( pVm->bCompilingBuiltin ){` |
|        - |  379 | `		/* Same stamp the embedded chunks got, so Reflection keeps reporting these` |
|        - |  380 | `		 * as internal: isInternal() true, getFileName() false. */` |
| 11158405 |  381 | `		iFuncFlags \|= VM_FUNC_INTERNAL;` |
|  5571456 |  382 | `	}` |
| 16730254 |  383 | `	pMeth = PH7_NewClassMethod(&(*pVm),pClass,&sName,0,` |
| 11158662 |  384 | `		NativeProtection(pDef->iMods),` |
| 11158662 |  385 | `		((pDef->iMods & PH7_MOD_FINAL) ? PH7_CLASS_ATTR_FINAL : 0)` |
| 11158662 |  386 | `		\| ((pDef->iMods & PH7_MOD_ABSTRACT) ? PH7_CLASS_ATTR_ABSTRACT : 0)` |
| 11158662 |  387 | `		\| ((pDef->iMods & PH7_MOD_FABRICATED) ? PH7_CLASS_ATTR_FABRICATED : 0),` |
|  5571587 |  388 | `		iFuncFlags);` |
| 11158667 |  389 | `	if( pMeth == 0 ){` |
|      ! 0 |  390 | `		return SXERR_MEM;` |
|        - |  391 | `	}` |
|        - |  392 | `	/* Staticness has to be recorded in BOTH places: on the method (where the class` |
|        - |  393 | `	 * machinery and Reflection read it) and on the function (where the OP_CALL` |
|        - |  394 | `	 * dispatcher reads it, to keep the caller's $this from leaking in as a receiver` |
|        - |  395 | `	 * — see VM_FUNC_NATIVE_STATIC). */` |
| 11158667 |  396 | `	if( pDef->iMods & PH7_MOD_STATIC ){` |
|   586717 |  397 | `		pMeth->iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|   586717 |  398 | `		pMeth->sFunc.iFlags \|= VM_FUNC_NATIVE_STATIC;` |
|   292949 |  399 | `	}` |
|        - |  400 | `	/* An ABSTRACT method has no body to install — an INTERFACE's methods are the` |
|        - |  401 | ``	 * reason the builder needs the case at all (`SeekableIterator::seek()`), and`` |
|        - |  402 | `	 * php reports them with their declared signature like any other. The dispatch` |
|        - |  403 | `	 * never reaches one: OP_CALL and OP_MEMBER both refuse an abstract method` |
|        - |  404 | `	 * ("Cannot call abstract method C::m()") before they look at a body. */` |
| 11158667 |  405 | `	if( pDef->iMods & PH7_MOD_ABSTRACT ){` |
|   483430 |  406 | `		if( pDef->zSig ){` |
|   483430 |  407 | `			rc = PH7_NewForeignFunction(&(*pVm),&sName,pDef->xFunc,pUserData,&pNative);` |
|   483430 |  408 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  409 | `				return rc;` |
|        - |  410 | `			}` |
|   483430 |  411 | `			pNative->zSig = pDef->zSig;` |
|   483430 |  412 | `			if( pDef->zRet && pDef->zRet[0] ){` |
|   459655 |  413 | `				pNative->zRet = pDef->zRet;` |
|   229506 |  414 | `			}` |
|   483430 |  415 | `			pMeth->sFunc.pNative = pNative;` |
|   241377 |  416 | `		}` |
|   483430 |  417 | `		return PH7_ClassInstallMethod(pClass,pMeth);` |
|        - |  418 | `	}` |
|        - |  419 | `	/* The C body. The diagnostic name is the qualified one php would print. */` |
| 10675242 |  420 | `	SyBufferFormat(zQual,sizeof(zQual),"%z::%s",&pClass->sName,pDef->zName);` |
| 10675242 |  421 | `	SyStringInitFromBuf(&sVmName,zQual,SyStrlen(zQual));` |
| 10675242 |  422 | `	rc = PH7_NewForeignFunction(&(*pVm),&sVmName,pDef->xFunc,pUserData,&pNative);` |
| 10675242 |  423 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  424 | `		return rc;` |
|        - |  425 | `	}` |
|        - |  426 | `	/* Arity bounds and by-ref positions come from the declared signature, exactly` |
|        - |  427 | `	 * as they do for a builtin (VmSetBuiltinSignatures) -- one string is the single` |
|        - |  428 | `	 * source of truth, and it doubles as the Reflection parameter list.` |
|        - |  429 | `	 *` |
|        - |  430 | `	 * NULL and "" mean different things here, and the difference is load-bearing.` |
|        - |  431 | `	 * A builtin without a row in aBuiltinSig[] is simply undescribed, so it stays` |
|        - |  432 | `	 * UNENFORCED (the SyZero'd bHasMaxArg == 0 reads as "unchecked", never as` |
|        - |  433 | `	 * "accepts at most zero"). A native method, by contrast, always states its` |
|        - |  434 | `	 * signature deliberately, so "" is a positive declaration of ZERO parameters` |
|        - |  435 | ``	 * and must enforce a maximum of zero -- otherwise `$gen->current(1)` and`` |
|        - |  436 | ``	 * `$fiber->isStarted(1)` are swallowed where php raises "expects exactly 0`` |
|        - |  437 | `	 * arguments, 1 given". VmDeriveArityFromSig("") already answers min 0 / max 0 /` |
|        - |  438 | `	 * enforced; only NULL opts out. */` |
| 10675242 |  439 | `	if( pDef->zSig ){` |
| 10659392 |  440 | `		sxi16 nMin = 0, nMax = 0;` |
| 10659392 |  441 | `		sxu8 bAtLeast = 0, bHasMax = 0;` |
| 10659392 |  442 | `		pNative->zSig = pDef->zSig;` |
| 10659392 |  443 | `		pNative->nByRefMask = VmDeriveByRefMaskFromSig(pDef->zSig);` |
| 10659392 |  444 | `		VmDeriveArityFromSig(pDef->zSig,&nMin,&bAtLeast,&nMax,&bHasMax);` |
| 10659392 |  445 | `		pNative->nMinArg = nMin;` |
| 10659392 |  446 | `		pNative->bAtLeast = bAtLeast;` |
| 10659392 |  447 | `		pNative->nMaxArg = nMax;` |
| 10659392 |  448 | `		pNative->bHasMaxArg = bHasMax;` |
|  5322296 |  449 | `	}` |
| 10675242 |  450 | `	if( pDef->zRet && pDef->zRet[0] ){` |
|  9644992 |  451 | `		pNative->zRet = pDef->zRet;` |
|  4815800 |  452 | `	}` |
| 10675242 |  453 | `	pMeth->sFunc.pNative = pNative;` |
| 10675242 |  454 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
| 10675242 |  455 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  456 | `		return rc;` |
|        - |  457 | `	}` |
| 10675242 |  458 | `	if( pClass->bMounted ){` |
|        - |  459 | `		/* The class is already live (a method attached after installation): mount` |
|        - |  460 | `		 * this one method now, since VmMountUserClass will not run again. */` |
|      ! 0 |  461 | `		return PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|        - |  462 | `	}` |
| 10675242 |  463 | `	return SXRET_OK;` |
|  5571592 |  464 | `}` |
|        - |  465 | `/*` |
|        - |  466 | ` * Install one class constant carrying a scalar value.` |
|        - |  467 | ` *` |
|        - |  468 | ` * A declared default is normally a compiled initializer (ph7_class_attr::aByteCode)` |
|        - |  469 | ` * evaluated at mount. The builder has no compiler to hand, so it evaluates the` |
|        - |  470 | ` * value NOW into the constant's reserved slot and leaves the byte-code empty --` |
|        - |  471 | ` * which is what a literal initializer would have produced anyway.` |
|        - |  472 | ` */` |
|  3146225 |  473 | `static sxi32 NativeInstallConstant(ph7_vm *pVm,ph7_class *pClass,const PH7_NativeConstDef *pDef)` |
|        5 |  474 | `{` |
|        - |  475 | `	ph7_class_attr *pAttr;` |
|        - |  476 | `	SyString sName;` |
|  3146230 |  477 | `	SyStringInitFromBuf(&sName,pDef->zName,SyStrlen(pDef->zName));` |
|  3146230 |  478 | `	pAttr = PH7_NewClassAttr(&(*pVm),&sName,0,NativeProtection(pDef->iMods),` |
|        - |  479 | `		PH7_CLASS_ATTR_CONSTANT);` |
|  3146230 |  480 | `	if( pAttr == 0 ){` |
|      ! 0 |  481 | `		return SXERR_MEM;` |
|        - |  482 | `	}` |
|  3146230 |  483 | `	pAttr->pDeclClass = pClass;` |
|        - |  484 | `	/* Stash the literal; VmMountUserClassAttrs materializes it into the slot. */` |
|  3146230 |  485 | `	pAttr->pNativeValue = pDef;` |
|  3146230 |  486 | `	return PH7_ClassInstallAttr(pClass,pAttr);` |
|  1570934 |  487 | `}` |
|        - |  488 | `/*` |
|        - |  489 | ` * Fill in a declared property TYPE from the text a spec row states, exactly as` |
|        - |  490 | ` * GenStateCopyTypeToAttr fills it from a parsed declaration: nType is the` |
|        - |  491 | ` * MEMOBJ_* the atom names (SXU32_HIGH plus sClass for a class name, which is` |
|        - |  492 | `` * also how the compiler carries `mixed` and `iterable`), the `?` becomes the`` |
|        - |  493 | ` * NULLABLE flag rather than a type bit, and sTypeName keeps the text VERBATIM --` |
|        - |  494 | ` * both the TypeError and Reflection print what the declaration said.` |
|        - |  495 | ` *` |
|        - |  496 | ` * Single atoms only. A union needs the alternative SET the compiler builds, and` |
|        - |  497 | ` * nothing native declares one; a spec that tries reads as the class name it is` |
|        - |  498 | ` * spelled with, which is why the parse stays this literal.` |
|        - |  499 | ` */` |
|  1291775 |  500 | `static void NativeAttrType(ph7_class_attr *pAttr,const char *zType)` |
|        5 |  501 | `{` |
|        - |  502 | `	static const struct { const char *zName; sxu32 nType; } aScalar[] = {` |
|        - |  503 | `		{ "int",    MEMOBJ_INT },` |
|        - |  504 | `		{ "float",  MEMOBJ_REAL },` |
|        - |  505 | `		{ "string", MEMOBJ_STRING },` |
|        - |  506 | `		{ "bool",   MEMOBJ_BOOL },` |
|        - |  507 | `		{ "array",  MEMOBJ_HASHMAP },` |
|        - |  508 | `		{ "object", MEMOBJ_OBJ },` |
|        - |  509 | `	};` |
|  1291780 |  510 | `	const char *zAtom = zType;` |
|        - |  511 | `	sxu32 nAtom, n;` |
|  1291780 |  512 | `	if( zAtom[0] == '?' ){` |
|   435880 |  513 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|   435880 |  514 | `		zAtom++;` |
|   217635 |  515 | `	}` |
|  1291780 |  516 | `	nAtom = SyStrlen(zAtom);` |
|  1291780 |  517 | `	pAttr->iFlags \|= PH7_CLASS_ATTR_TYPED;` |
|  1291780 |  518 | `	SyStringInitFromBuf(&pAttr->sTypeName,zType,SyStrlen(zType));` |
|  4929355 |  519 | `	for( n = 0 ; n < SX_ARRAYSIZE(aScalar) ; ++n ){` |
|  4604425 |  520 | `		if( nAtom == SyStrlen(aScalar[n].zName)` |
|  2838670 |  521 | `		 && SyMemcmp(zAtom,aScalar[n].zName,(sxu32)nAtom) == 0 ){` |
|   966855 |  522 | `			pAttr->nType = aScalar[n].nType;` |
|   966855 |  523 | `			return;` |
|        - |  524 | `		}` |
|  1816268 |  525 | `	}` |
|   324930 |  526 | `	pAttr->nType = SXU32_HIGH;` |
|   324930 |  527 | `	SyStringInitFromBuf(&pAttr->sClass,zAtom,nAtom);` |
|   644996 |  528 | `}` |
|        - |  529 | `/*` |
|        - |  530 | ` * Install one declared property.` |
|        - |  531 | ` *` |
|        - |  532 | ` * The default is carried as a literal rather than compiled byte-code (see` |
|        - |  533 | `` * PH7_NativeLiteralValue); an instance property materializes it at `new`, a static`` |
|        - |  534 | `` * one at mount. A PH7_NATIVE_VAL_NULL default is the plain `public $p;` case.`` |
|        - |  535 | ` */` |
|  2971875 |  536 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallProperty(ph7_vm *pVm,ph7_class *pClass,` |
|        - |  537 | `	const PH7_NativePropDef *pDef)` |
|        5 |  538 | `{` |
|        - |  539 | `	ph7_class_attr *pAttr;` |
|        - |  540 | `	SyString sName;` |
|  2971880 |  541 | `	sxi32 iFlags = 0;` |
|  2971880 |  542 | `	SyStringInitFromBuf(&sName,pDef->zName,SyStrlen(pDef->zName));` |
|  2971880 |  543 | `	if( pDef->iMods & PH7_MOD_STATIC ){` |
|      ! 0 |  544 | `		iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|      ! 0 |  545 | `	}` |
|  2971880 |  546 | `	if( pDef->iMods & PH7_MOD_HIDDEN ){` |
|  1521605 |  547 | `		iFlags \|= PH7_CLASS_ATTR_HIDDEN;` |
|   759744 |  548 | `	}` |
|  2971880 |  549 | `	if( pDef->iMods & PH7_MOD_ONDEMAND ){` |
|     7930 |  550 | `		iFlags \|= PH7_CLASS_ATTR_NATIVE_ONDEMAND;` |
|     3957 |  551 | `	}` |
|        - |  552 | `	/* php's VIRTUAL property: declared, and answered by the class's own handlers` |
|        - |  553 | `	 * rather than by a slot. NATIVE_VIRTUAL rides with it because that is exactly` |
|        - |  554 | `	 * what the name means to Reflection (modifiers 512) and to the object` |
|        - |  555 | `	 * comparator -- there is no real property behind it to compare. */` |
|  2971880 |  556 | `	if( pDef->iMods & PH7_MOD_VIRTUAL ){` |
|   729105 |  557 | `		iFlags \|= PH7_CLASS_ATTR_NATIVE_NOSLOT\|PH7_CLASS_ATTR_NATIVE_VIRTUAL;` |
|   364044 |  558 | `	}` |
|        - |  559 | `	/* php declares several native slots readonly and asymmetrically visible` |
|        - |  560 | ``	 * (`public protected(set) readonly string $path` on Directory), and both are`` |
|        - |  561 | `	 * php-visible twice over: the write refusal and Reflection's modifier list. */` |
|  2971880 |  562 | `	if( pDef->iMods & PH7_MOD_READONLY ){` |
|    79255 |  563 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|    39570 |  564 | `	}` |
|  2971880 |  565 | `	if( pDef->iMods & PH7_MOD_PROT_SET ){` |
|    71330 |  566 | `		iFlags \|= PH7_CLASS_ATTR_PROTECTED_SET;` |
|    35613 |  567 | `	}` |
|  2971880 |  568 | `	if( pDef->iMods & PH7_MOD_PRIV_SET ){` |
|      ! 0 |  569 | `		iFlags \|= PH7_CLASS_ATTR_PRIVATE_SET;` |
|      ! 0 |  570 | `	}` |
|  2971880 |  571 | `	pAttr = PH7_NewClassAttr(&(*pVm),&sName,0,NativeProtection(pDef->iMods),iFlags);` |
|  2971880 |  572 | `	if( pAttr == 0 ){` |
|      ! 0 |  573 | `		return SXERR_MEM;` |
|        - |  574 | `	}` |
|  2971880 |  575 | `	pAttr->pDeclClass = pClass;` |
|        - |  576 | `	/* NO default is not the same as a NULL one, and the difference is php-visible:` |
|        - |  577 | ``	 * a typed slot without a default is UNINITIALIZED at `new` (reading it before`` |
|        - |  578 | `	 * the class writes it is an Error, hasDefaultValue() is false), which is what` |
|        - |  579 | `	 * php declares for LibXMLError's six fields and every reflector's $name. The` |
|        - |  580 | ``	 * machinery is already there for a compiled `public string $p;` — leaving`` |
|        - |  581 | `	 * pNativeValue at 0 is what selects it. */` |
|  2971880 |  582 | `	if( pDef->sDefault.iType != PH7_NATIVE_VAL_NONE ){` |
|  1862380 |  583 | `		pAttr->pNativeValue = &pDef->sDefault;` |
|   929895 |  584 | `	}` |
|  2971880 |  585 | `	if( pDef->zType && pDef->zType[0] ){` |
|  1291780 |  586 | `		NativeAttrType(pAttr,pDef->zType);` |
|   644991 |  587 | `	}` |
|  2971880 |  588 | `	return PH7_ClassInstallAttr(pClass,pAttr);` |
|  1483880 |  589 | `}` |
|        - |  590 | `/*` |
|        - |  591 | `` * Attach an `#[Attr(...)]` to something declared from C.`` |
|        - |  592 | ` *` |
|        - |  593 | `` * php declares `#[Attribute(Attribute::TARGET_CLASS)]` on Attribute itself, a`` |
|        - |  594 | `` * target mask on every other attribute class, and `#[NoDiscard(message: …)]` on`` |
|        - |  595 | ` * nine DateTimeImmutable methods — and those records are LOAD-BEARING: the` |
|        - |  596 | `` * compiler reads them to decide whether a user's `#[Deprecated]` may sit where`` |
|        - |  597 | ` * it does, the NoDiscard warning reads its message from them, and` |
|        - |  598 | ` * ReflectionAttribute answers them all. A compiled attribute holds its argument` |
|        - |  599 | ` * as byte-code; there is no compiler here, so the argument rides as the same` |
|        - |  600 | ` * literal record a native constant or property default uses and every reader` |
|        - |  601 | ` * takes that branch when the byte-code is empty.` |
|        - |  602 | ` *` |
|        - |  603 | ` * NativeBuildAttr is the shared half; the two entry points below hang the record` |
|        - |  604 | ` * on a class or on one of its methods. aArg is BORROWED, so callers state their` |
|        - |  605 | `` * rows `static const`.`` |
|        - |  606 | ` */` |
|   134725 |  607 | `static sxi32 NativeBuildAttr(ph7_vm *pVm,ph7_attribute *pAttr,const char *zAttr,` |
|        - |  608 | `	const PH7_NativeAttrArg *aArg,sxu32 nArg)` |
|        5 |  609 | `{` |
|        - |  610 | `	char *zDup;` |
|        - |  611 | `	sxu32 n;` |
|   134730 |  612 | `	SyZero(pAttr,sizeof(*pAttr));` |
|   134730 |  613 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zAttr,SyStrlen(zAttr));` |
|   134730 |  614 | `	if( zDup == 0 ){` |
|      ! 0 |  615 | `		return SXERR_MEM;` |
|        - |  616 | `	}` |
|   134730 |  617 | `	SyStringInitFromBuf(&pAttr->sName,zDup,SyStrlen(zAttr));` |
|   134730 |  618 | `	SySetInit(&pAttr->aArgs,&pVm->sAllocator,sizeof(ph7_attr_arg));` |
|   269455 |  619 | `	for( n = 0 ; n < nArg ; n++ ){` |
|        - |  620 | `		ph7_attr_arg sArgRec;` |
|   134730 |  621 | `		SyZero(&sArgRec,sizeof(sArgRec));` |
|   134730 |  622 | `		SySetInit(&sArgRec.aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|   134730 |  623 | `		if( aArg[n].zName ){` |
|    71330 |  624 | `			char *zN = SyMemBackendStrDup(&pVm->sAllocator,aArg[n].zName,SyStrlen(aArg[n].zName));` |
|    71330 |  625 | `			if( zN ){` |
|    71330 |  626 | `				SyStringInitFromBuf(&sArgRec.sName,zN,SyStrlen(aArg[n].zName));` |
|    35613 |  627 | `			}` |
|    35613 |  628 | `		}` |
|        - |  629 | `		/* The literal is BORROWED, not copied: aArg must have static storage` |
|        - |  630 | ``		 * duration (every caller states its rows as `static const`). */`` |
|   134730 |  631 | `		sArgRec.pNativeValue = (const void *)&aArg[n].sValue;` |
|   134730 |  632 | `		SySetPut(&pAttr->aArgs,(const void *)&sArgRec);` |
|    67274 |  633 | `	}` |
|   134730 |  634 | `	return SXRET_OK;` |
|    67274 |  635 | `}` |
|        - |  636 | `/*` |
|        - |  637 | `` * Declare one of a native class's METHODS php 8.5's `#[\NoDiscard]`, argument`` |
|        - |  638 | ` * and all — the same record a compiled declaration carries, so Reflection` |
|        - |  639 | ` * reports the attribute and the warning reads its message from the one place a` |
|        - |  640 | ` * userland one is read from. Assigned by the owning installer after` |
|        - |  641 | ` * PH7_InstallNativeClasses, like xClone/xDim/xSet: a spec-row field would have` |
|        - |  642 | ` * to be left empty by every other table.` |
|        - |  643 | ` */` |
|    71325 |  644 | `PH7_PRIVATE sxi32 PH7_NativeMethodSetNoDiscard(ph7_vm *pVm,ph7_class *pClass,` |
|        - |  645 | `	const char *zMethod,const PH7_NativeAttrArg *aArg,sxu32 nArg)` |
|        5 |  646 | `{` |
|        - |  647 | `	ph7_class_method *pMeth;` |
|        - |  648 | `	ph7_attribute sAttr;` |
|        - |  649 | `	sxi32 rc;` |
|    71330 |  650 | `	if( pClass == 0 ){` |
|      ! 0 |  651 | `		return SXERR_NOTFOUND;` |
|        - |  652 | `	}` |
|    71330 |  653 | `	pMeth = PH7_ClassExtractMethod(pClass,zMethod,(sxu32)SyStrlen(zMethod));` |
|    71330 |  654 | `	if( pMeth == 0 ){` |
|      ! 0 |  655 | `		return SXERR_NOTFOUND;` |
|        - |  656 | `	}` |
|    71330 |  657 | `	rc = NativeBuildAttr(&(*pVm),&sAttr,"NoDiscard",aArg,nArg);` |
|    71330 |  658 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  659 | `		return rc;` |
|        - |  660 | `	}` |
|    71330 |  661 | `	pMeth->sFunc.iFlags \|= VM_FUNC_NODISCARD;` |
|    71330 |  662 | `	return SySetPut(&pMeth->sFunc.aAttrs,(const void *)&sAttr);` |
|    35618 |  663 | `}` |
|    63400 |  664 | `PH7_PRIVATE sxi32 PH7_NativeClassAddAttribute(ph7_vm *pVm,ph7_class *pClass,` |
|        - |  665 | `	const char *zAttr,const PH7_NativeAttrArg *aArg,sxu32 nArg)` |
|        5 |  666 | `{` |
|        - |  667 | `	ph7_attribute sAttr;` |
|        - |  668 | `	sxi32 rc;` |
|    63405 |  669 | `	if( pClass == 0 ){` |
|      ! 0 |  670 | `		return SXERR_NOTFOUND;` |
|        - |  671 | `	}` |
|    63405 |  672 | `	rc = NativeBuildAttr(&(*pVm),&sAttr,zAttr,aArg,nArg);` |
|    63405 |  673 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  674 | `		return rc;` |
|        - |  675 | `	}` |
|    63405 |  676 | `	return SySetPut(&pClass->aAttrs,(const void *)&sAttr);` |
|    31661 |  677 | `}` |
|        - |  678 | `/*` |
|        - |  679 | ` * php's get_properties / get_debug_info handlers: the SHAPE a class SHOWS.` |
|        - |  680 | ` *` |
|        - |  681 | ` * Several native classes present something that is not their storage. php shows a` |
|        - |  682 | ` * DateTime as date/timezone_type/timezone (the state is a timestamp, an offset and` |
|        - |  683 | ` * a zone name), a DateTimeZone as timezone_type/timezone, and a WeakReference as` |
|        - |  684 | ` * ["object"]. Those slots carry PH7_MOD_HIDDEN so no presentation surface sees the` |
|        - |  685 | ` * engine state; this fills an array with what php shows instead.` |
|        - |  686 | ` *` |
|        - |  687 | ` * Answers 1 when the class (or an ancestor) declared a hook and pOut was filled.` |
|        - |  688 | ` * bDebug distinguishes php's two handlers: 1 for var_dump/print_r (get_debug_info),` |
|        - |  689 | ` * 0 for var_export and the (array) cast (get_properties). They disagree — a` |
|        - |  690 | ` * WeakReference shows ["object"] to var_dump and nothing to (array) — so the` |
|        - |  691 | ` * callback is told which is asking rather than each caller guessing.` |
|        - |  692 | ` * get_object_vars() and foreach are NOT callers: php answers those from the real` |
|        - |  693 | ` * properties with the caller's scope applied, which for every class here is empty.` |
|        - |  694 | ` */` |
|     1758 |  695 | `PH7_PRIVATE int PH7_ClassInstancePresent(ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|        5 |  696 | `{` |
|        - |  697 | `	ph7_class *pClass;` |
|     1763 |  698 | `	if( pThis == 0 \|\| pOut == 0 ){` |
|      ! 0 |  699 | `		return 0;` |
|        - |  700 | `	}` |
|     2617 |  701 | `	for( pClass = pThis->pClass ; pClass ; pClass = pClass->pBase ){` |
|     1897 |  702 | `		if( pClass->xPresent ){` |
|     1042 |  703 | `			return pClass->xPresent(pThis->pVm,pThis,pOut,bDebug) == SXRET_OK;` |
|        - |  704 | `		}` |
|      432 |  705 | `	}` |
|      725 |  706 | `	return 0;` |
|      884 |  707 | `}` |
|        - |  708 | `/*` |
|        - |  709 | ` * The nearest ph7_class::xDim in a class's base chain -- php's handler` |
|        - |  710 | ` * inheritance, so a user subclass of DOMNodeList reads dimensions the way its` |
|        - |  711 | ` * parent does.` |
|        - |  712 | ` */` |
|     1500 |  713 | `static ph7_class * NativeDimClass(ph7_class *pClass)` |
|        5 |  714 | `{` |
|     2407 |  715 | `	while( pClass ){` |
|     1723 |  716 | `		if( pClass->xDim ){` |
|      821 |  717 | `			return pClass;` |
|        - |  718 | `		}` |
|      907 |  719 | `		pClass = pClass->pBase;` |
|        5 |  720 | `	}` |
|      689 |  721 | `	return 0;` |
|      755 |  722 | `}` |
|        - |  723 | `/*` |
|        - |  724 | `` * Does `$o[$k]` mean anything for an instance of this class? The subscript`` |
|        - |  725 | `` * opcode asks BEFORE it commits to php's `Cannot use object of type C as`` |
|        - |  726 | `` * array`, which is still the answer for every class that has no hook.`` |
|        - |  727 | ` */` |
|     1028 |  728 | `PH7_PRIVATE int PH7_ClassHasNativeDim(ph7_class *pClass)` |
|        5 |  729 | `{` |
|     1033 |  730 | `	return NativeDimClass(pClass) != 0;` |
|        5 |  731 | `}` |
|        - |  732 | `/*` |
|        - |  733 | ` * Run the hook. Answers 0 when the class has none (nothing in pCtx is touched);` |
|        - |  734 | ` * 1 when it answered, which includes a REFUSAL -- the caller reads zThrowClass` |
|        - |  735 | ` * to tell the two apart.` |
|        - |  736 | ` */` |
|      370 |  737 | `PH7_PRIVATE int PH7_ClassNativeDim(ph7_class_instance *pThis,PH7_NativeDimCtx *pCtx)` |
|        4 |  738 | `{` |
|      374 |  739 | `	ph7_class *pClass = pThis ? NativeDimClass(pThis->pClass) : 0;` |
|      374 |  740 | `	if( pClass == 0 ){` |
|      ! 0 |  741 | `		return 0;` |
|        - |  742 | `	}` |
|      374 |  743 | `	pClass->xDim(pThis->pVm,pThis,pCtx);` |
|      374 |  744 | `	return 1;` |
|      189 |  745 | `}` |
|        - |  746 | `/*` |
|        - |  747 | ` * The refusal a dimension WRITE, APPEND or UNSET takes on an object. php's own` |
|        - |  748 | ` * sentence for a class that is not an ArrayAccess is` |
|        - |  749 | `` * `Cannot use object of type C as array`; a class whose read handler answers`` |
|        - |  750 | ` * something words its own (php's PDORow names the operation and the class),` |
|        - |  751 | ` * which the hook supplies through the same refusal fields a read uses.` |
|        - |  752 | ` */` |
|       30 |  753 | `PH7_PRIVATE sxu32 PH7_ClassNativeDimRefusal(ph7_class_instance *pThis,int iMode,` |
|        - |  754 | `	char *zMsg,sxu32 nMsg)` |
|        3 |  755 | `{` |
|       33 |  756 | `	ph7_class *pClass = pThis ? NativeDimClass(pThis->pClass) : 0;` |
|       33 |  757 | `	if( pClass ){` |
|        - |  758 | `		PH7_NativeDimCtx sDim;` |
|       19 |  759 | `		sDim.iMode = iMode;` |
|       19 |  760 | `		sDim.pOffset = 0;` |
|       19 |  761 | `		sDim.pResult = 0;` |
|       19 |  762 | `		sDim.zThrowClass = 0;` |
|       19 |  763 | `		sDim.zThrowMsg[0] = 0;` |
|       19 |  764 | `		sDim.bStored = 0;` |
|       19 |  765 | `		pClass->xDim(pThis->pVm,pThis,&sDim);` |
|       19 |  766 | `		if( sDim.zThrowClass ){` |
|        3 |  767 | `			return SyBufferFormat(zMsg,nMsg,"%s",sDim.zThrowMsg);` |
|        - |  768 | `		}` |
|        8 |  769 | `	}` |
|       73 |  770 | `	return SyBufferFormat(zMsg,nMsg,"Cannot use object of type %.*s as array",` |
|       28 |  771 | `		pThis ? (int)pThis->pClass->sDisp.nByte : 0,` |
|       28 |  772 | `		pThis ? pThis->pClass->sDisp.zString : "");` |
|       18 |  773 | `}` |
|        - |  774 | `/*` |
|        - |  775 | ` * Offer a dimension WRITE, APPEND or UNSET to the class's own handler, with the` |
|        - |  776 | ` * offset and the value the refusal-only form does not carry.` |
|        - |  777 | ` *` |
|        - |  778 | ` * Answers 1 when the handler took the access -- either by STORING (bStored,` |
|        - |  779 | ` * with zThrowClass still 0) or by wording its own refusal in` |
|        - |  780 | ` * zThrowClass/zThrowMsg -- and 0 when the class has no handler or its handler` |
|        - |  781 | `` * declined, which puts the access on the ordinary `Cannot use object of type C`` |
|        - |  782 | `` * as array` path. pCtx is the caller's scratch: it is initialized here and left`` |
|        - |  783 | ` * filled for the caller to read.` |
|        - |  784 | ` */` |
|       72 |  785 | `PH7_PRIVATE int PH7_ClassNativeDimStore(ph7_class_instance *pThis,int iMode,` |
|        - |  786 | `	ph7_value *pOffset,ph7_value *pValue,PH7_NativeDimCtx *pCtx)` |
|        3 |  787 | `{` |
|       75 |  788 | `	ph7_class *pClass = pThis ? NativeDimClass(pThis->pClass) : 0;` |
|       75 |  789 | `	pCtx->iMode = iMode;` |
|       75 |  790 | `	pCtx->pOffset = pOffset;` |
|       75 |  791 | `	pCtx->pResult = pValue;` |
|       75 |  792 | `	pCtx->zThrowClass = 0;` |
|       75 |  793 | `	pCtx->zThrowMsg[0] = 0;` |
|       75 |  794 | `	pCtx->bStored = 0;` |
|       75 |  795 | `	if( pClass == 0 ){` |
|        5 |  796 | `		return 0;` |
|        - |  797 | `	}` |
|       71 |  798 | `	pClass->xDim(pThis->pVm,pThis,pCtx);` |
|       71 |  799 | `	return pCtx->bStored \|\| pCtx->zThrowClass ? 1 : 0;` |
|       39 |  800 | `}` |
|        - |  801 | `/*` |
|        - |  802 | ` * The nearest ph7_class::xProp in a class's base chain -- the same handler` |
|        - |  803 | ` * inheritance xDim and xSet get, and php's own: a subclass of a class whose` |
|        - |  804 | ` * properties are not storage reads them through the parent's handler.` |
|        - |  805 | ` */` |
|    19734 |  806 | `static ph7_class * NativePropClass(ph7_class *pClass)` |
|        5 |  807 | `{` |
|    38401 |  808 | `	while( pClass ){` |
|    37009 |  809 | `		if( pClass->xProp ){` |
|    18347 |  810 | `			return pClass;` |
|        - |  811 | `		}` |
|    18667 |  812 | `		pClass = pClass->pBase;` |
|        5 |  813 | `	}` |
|     1397 |  814 | `	return 0;` |
|     9873 |  815 | `}` |
|        - |  816 | `/*` |
|        - |  817 | `` * Does `$o->p` MEAN something this class answers for itself? Asked before the`` |
|        - |  818 | ` * miss path commits to creating a property, warning about an undefined one or` |
|        - |  819 | ` * dispatching __get.` |
|        - |  820 | ` */` |
|     7937 |  821 | `PH7_PRIVATE int PH7_ClassHasNativeProp(ph7_class *pClass)` |
|        5 |  822 | `{` |
|     7942 |  823 | `	return NativePropClass(pClass) != 0;` |
|        5 |  824 | `}` |
|        - |  825 | `/*` |
|        - |  826 | ` * Run the hook. Answers 0 when the class has none, or when the hook DECLINED` |
|        - |  827 | ` * the name (bAnswered left at 0); 1 when it answered, which includes a` |
|        - |  828 | ` * REFUSAL -- the caller reads zThrowClass to tell the two apart.` |
|        - |  829 | ` */` |
|     9595 |  830 | `PH7_PRIVATE int PH7_ClassNativeProp(ph7_class_instance *pThis,PH7_NativePropCtx *pCtx)` |
|        5 |  831 | `{` |
|     9600 |  832 | `	ph7_class *pClass = pThis ? NativePropClass(pThis->pClass) : 0;` |
|     9600 |  833 | `	if( pClass == 0 ){` |
|       17 |  834 | `		return 0;` |
|        - |  835 | `	}` |
|     9584 |  836 | `	pClass->xProp(pThis->pVm,pThis,pCtx);` |
|     9584 |  837 | `	return pCtx->bAnswered \|\| pCtx->zThrowClass != 0;` |
|     4803 |  838 | `}` |
|        - |  839 | `/*` |
|        - |  840 | ` * Fill a caller-owned context and run the hook, for the callers that ask` |
|        - |  841 | ` * OUTSIDE the member opcode: Reflection's getValue()/setValue() and` |
|        - |  842 | ` * property_exists(), each of which reaches php's handlers by its own door.` |
|        - |  843 | ` */` |
|     2386 |  844 | `PH7_PRIVATE int PH7_ClassNativePropAsk(ph7_class_instance *pThis,PH7_NativePropCtx *pCtx,` |
|        - |  845 | `	int iMode,const SyString *pName,ph7_value *pResult)` |
|        4 |  846 | `{` |
|     2390 |  847 | `	pCtx->iMode = iMode;` |
|     2390 |  848 | `	pCtx->pName = pName;` |
|     2390 |  849 | `	pCtx->pResult = pResult;` |
|     2390 |  850 | `	pCtx->bAnswered = 0;` |
|     2390 |  851 | `	pCtx->zThrowClass = 0;` |
|     2390 |  852 | `	pCtx->zThrowMsg[0] = 0;` |
|     2390 |  853 | `	pCtx->iThrowCode = 0;` |
|     2390 |  854 | `	pCtx->bQuiet = 0;` |
|     2390 |  855 | `	pCtx->bWriteCtx = 0;` |
|     2390 |  856 | `	pCtx->nSlot = SXU32_HIGH;` |
|     2390 |  857 | `	return PH7_ClassNativeProp(pThis,pCtx);` |
|        4 |  858 | `}` |
|        - |  859 | `/*` |
|        - |  860 | `` * Does this class answer `$o->p` through a HANDLER of its own -- php's question`` |
|        - |  861 | ` * "is the name in the class's property-handler table"?` |
|        - |  862 | ` *` |
|        - |  863 | ` * The hook answers for itself, because what the answer depends on differs per` |
|        - |  864 | ` * class: ext/dom reads the class's VIRTUAL declarations (PH7_MOD_VIRTUAL -- a` |
|        - |  865 | ` * name declared with no slot of any kind), ArrayObject reads the object's` |
|        - |  866 | ` * ARRAY_AS_PROPS flag and owns every name once it is set. Whether a REAL property` |
|        - |  867 | ` * of that name is in the way is not the hook's question: every caller asks only` |
|        - |  868 | ` * after the instance's own table missed, which is php's order too.` |
|        - |  869 | ` *` |
|        - |  870 | `` * It is what makes the handler beat a subclass's own `__get`: a name the table`` |
|        - |  871 | ` * carries never reaches the magic layer, and a name it does not carry falls` |
|        - |  872 | ` * through to it, which is php's handler order. Asked at the write shapes, where` |
|        - |  873 | ` * the member opcode has no value yet and the write half cannot be run for an` |
|        - |  874 | ` * answer.` |
|        - |  875 | ` */` |
|     2202 |  876 | `PH7_PRIVATE int PH7_ClassNativePropOwns(ph7_class_instance *pThis,const SyString *pName)` |
|        5 |  877 | `{` |
|        - |  878 | `	PH7_NativePropCtx sCtx;` |
|     2207 |  879 | `	if( pThis == 0 \|\| NativePropClass(pThis->pClass) == 0 ){` |
|      653 |  880 | `		return 0;` |
|        - |  881 | `	}` |
|     2327 |  882 | `	return PH7_ClassNativePropAsk(pThis,&sCtx,PH7_NATIVE_PROP_OWNS,pName,0)` |
|     1554 |  883 | `	    && sCtx.zThrowClass == 0;` |
|     1106 |  884 | `}` |
|        - |  885 | `/*` |
|        - |  886 | ` * Install a property handler on a mounted native class. Called by the owning` |
|        - |  887 | ` * installer right after PH7_InstallNativeClasses, for the same reason xClone,` |
|        - |  888 | ` * xDim and xSet are: the spec table has no field for a hook.` |
|        - |  889 | ` */` |
|    71325 |  890 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallPropHook(ph7_vm *pVm,const char *zClass,` |
|        - |  891 | `	void (*xProp)(ph7_vm *,ph7_class_instance *,PH7_NativePropCtx *))` |
|        5 |  892 | `{` |
|    71330 |  893 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|    71330 |  894 | `	if( pClass == 0 ){` |
|      ! 0 |  895 | `		return SXERR_NOTFOUND;` |
|        - |  896 | `	}` |
|    71330 |  897 | `	pClass->xProp = xProp;` |
|    71330 |  898 | `	return SXRET_OK;` |
|    35618 |  899 | `}` |
|        - |  900 | `/*` |
|        - |  901 | ` * The nearest ph7_class::xSet in a class's base chain -- the same handler` |
|        - |  902 | ` * inheritance the dimension hook gets, so a user subclass of DateInterval` |
|        - |  903 | ` * converts its writes the way its parent does.` |
|        - |  904 | ` */` |
|      618 |  905 | `static ph7_class * NativeSetClass(ph7_class *pClass)` |
|        4 |  906 | `{` |
|      624 |  907 | `	while( pClass ){` |
|      624 |  908 | `		if( pClass->xSet ){` |
|      622 |  909 | `			return pClass;` |
|        - |  910 | `		}` |
|        3 |  911 | `		pClass = pClass->pBase;` |
|        1 |  912 | `	}` |
|      ! 0 |  913 | `	return 0;` |
|      313 |  914 | `}` |
|        - |  915 | `/*` |
|        - |  916 | ` * Run the write handler for a property store. Answers 0 when no class in the` |
|        - |  917 | ` * chain has one (nothing in pCtx is touched); 1 when it ran, which includes a` |
|        - |  918 | ` * REFUSAL -- the caller reads zThrowClass to tell the two apart.` |
|        - |  919 | ` */` |
|      618 |  920 | `PH7_PRIVATE int PH7_ClassNativeSet(ph7_class_instance *pThis,PH7_NativeSetCtx *pCtx)` |
|        4 |  921 | `{` |
|      622 |  922 | `	ph7_class *pClass = pThis ? NativeSetClass(pThis->pClass) : 0;` |
|      622 |  923 | `	if( pClass == 0 ){` |
|      ! 0 |  924 | `		return 0;` |
|        - |  925 | `	}` |
|      622 |  926 | `	pClass->xSet(pThis->pVm,pThis,pCtx);` |
|      622 |  927 | `	return 1;` |
|      313 |  928 | `}` |
|        - |  929 | `/*` |
|        - |  930 | ` * php's create_object handler for a mounted native class: the C routine that` |
|        - |  931 | ` * runs once the instance frame exists and before any constructor.` |
|        - |  932 | ` *` |
|        - |  933 | ` * It exists for one shape -- a class whose properties php DECLARES and then` |
|        - |  934 | ` * answers through a read_property handler rather than out of the slots. Seeding` |
|        - |  935 | `` * the slots reproduces every surface of that at once (the read, `var_dump`, the`` |
|        - |  936 | `` * `(array)` cast, `json_encode`, `foreach`, `serialize`, `get_object_vars`),`` |
|        - |  937 | ` * and leaves the DECLARATION alone: the properties still have no default, so` |
|        - |  938 | ` * Reflection's hasDefaultValue() answers false the way php's does.` |
|        - |  939 | ` */` |
|     7925 |  940 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallNewHook(ph7_vm *pVm,const char *zClass,` |
|        - |  941 | `	void (*xNew)(ph7_vm *,ph7_class_instance *))` |
|        5 |  942 | `{` |
|     7930 |  943 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|     7930 |  944 | `	if( pClass == 0 ){` |
|      ! 0 |  945 | `		return SXERR_NOTFOUND;` |
|        - |  946 | `	}` |
|     7930 |  947 | `	pClass->xNew = xNew;` |
|     7930 |  948 | `	return SXRET_OK;` |
|     3962 |  949 | `}` |
|        - |  950 | `/*` |
|        - |  951 | ` * Install a write handler on a mounted native class and mark every INSTANCE` |
|        - |  952 | ` * property it declares as filtered, which is what makes instantiation register` |
|        - |  953 | ` * the slots the filter looks up. Called by the owning installer right after` |
|        - |  954 | ` * PH7_InstallNativeClasses, for the same reason xClone and xDim are: the spec` |
|        - |  955 | ` * table has no field for a hook.` |
|        - |  956 | ` */` |
|    15850 |  957 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallSetHook(ph7_vm *pVm,const char *zClass,` |
|        - |  958 | `	void (*xSet)(ph7_vm *,ph7_class_instance *,PH7_NativeSetCtx *))` |
|        5 |  959 | `{` |
|    15855 |  960 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|        - |  961 | `	SyHashEntry *pEntry;` |
|    15855 |  962 | `	if( pClass == 0 ){` |
|      ! 0 |  963 | `		return SXERR_NOTFOUND;` |
|        - |  964 | `	}` |
|    15855 |  965 | `	pClass->xSet = xSet;` |
|    15855 |  966 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|   182291 |  967 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|   158505 |  968 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   158505 |  969 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|   158505 |  970 | `			pAttr->iFlags \|= PH7_CLASS_ATTR_NATIVE_SET;` |
|    79140 |  971 | `		}` |
|        5 |  972 | `	}` |
|    15855 |  973 | `	return SXRET_OK;` |
|     7919 |  974 | `}` |
|        - |  975 | `/*` |
|        - |  976 | ` * The nearest ph7_class::xCmp in a class's base chain -- the same handler` |
|        - |  977 | ` * inheritance xDim and xSet get, and php's own: a subclass of DateTime still` |
|        - |  978 | ` * compares as an instant, whatever properties it adds.` |
|        - |  979 | ` */` |
|     1162 |  980 | `static ph7_class * NativeCmpClass(ph7_class *pClass)` |
|        5 |  981 | `{` |
|     2043 |  982 | `	while( pClass ){` |
|     1177 |  983 | `		if( pClass->xCmp ){` |
|      301 |  984 | `			return pClass;` |
|        - |  985 | `		}` |
|      881 |  986 | `		pClass = pClass->pBase;` |
|        5 |  987 | `	}` |
|      871 |  988 | `	return 0;` |
|      586 |  989 | `}` |
|        - |  990 | `/*` |
|        - |  991 | ` * Ask the LEFT operand's compare handler, php-style. Answers 0 when no class in` |
|        - |  992 | ` * its chain has one (the caller falls back to the property walk); 1 when the` |
|        - |  993 | ` * handler decided, and *pResult is then the ordering -- which includes a` |
|        - |  994 | ` * REFUSAL, recorded on the VM for the nearest throw boundary to raise, with the` |
|        - |  995 | ` * uncomparable 1 standing in as the answer meanwhile.` |
|        - |  996 | ` */` |
|      822 |  997 | `PH7_PRIVATE int PH7_ClassNativeCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,sxi32 *pResult)` |
|        5 |  998 | `{` |
|      827 |  999 | `	ph7_class *pClass = pLeft ? NativeCmpClass(pLeft->pClass) : 0;` |
|        - | 1000 | `	PH7_NativeCmpCtx sCtx;` |
|        - | 1001 | `	ph7_vm *pVm;` |
|      827 | 1002 | `	if( pClass == 0 ){` |
|      651 | 1003 | `		return 0;` |
|        - | 1004 | `	}` |
|      180 | 1005 | `	pVm = pLeft->pVm;` |
|      180 | 1006 | `	SyZero(&sCtx,sizeof(sCtx));` |
|      180 | 1007 | `	sCtx.pOther = pRight;` |
|      180 | 1008 | `	sCtx.iResult = 1;   /* php's ZEND_UNCOMPARABLE: what a hook that recognizes nothing answers */` |
|      180 | 1009 | `	pClass->xCmp(pVm,pLeft,&sCtx);` |
|      180 | 1010 | `	if( sCtx.zThrowClass && pVm->zCmpRefusalClass == 0 ){` |
|        - | 1011 | `		/* First refusal wins: a driver that keeps comparing after one (sort() does)` |
|        - | 1012 | `		 * must not overwrite the message the script will actually see. */` |
|       53 | 1013 | `		pVm->zCmpRefusalClass = sCtx.zThrowClass;` |
|       53 | 1014 | `		SyMemcpy(sCtx.zThrowMsg,pVm->zCmpRefusalMsg,sizeof(pVm->zCmpRefusalMsg));` |
|       53 | 1015 | `		pVm->zCmpRefusalMsg[sizeof(pVm->zCmpRefusalMsg)-1] = 0;` |
|       26 | 1016 | `	}` |
|      180 | 1017 | `	*pResult = sCtx.iResult;` |
|      180 | 1018 | `	return 1;` |
|      416 | 1019 | `}` |
|        - | 1020 | `/*` |
|        - | 1021 | ` * The same handler, asked about a SCALAR partner -- php's compare handler is one` |
|        - | 1022 | `` * door and `$n == 2` reaches it exactly as `$n == $m` does. Answers 1 only when`` |
|        - | 1023 | ` * the hook RECOGNIZED the value; otherwise the caller falls back to php's` |
|        - | 1024 | ` * cast-the-object rule, which is what every class without a handler gets.` |
|        - | 1025 | ` */` |
|      340 | 1026 | `PH7_PRIVATE int PH7_ClassNativeCmpValue(ph7_class_instance *pLeft,ph7_value *pOther,` |
|        - | 1027 | `	int bReversed,sxi32 *pResult)` |
|        5 | 1028 | `{` |
|      345 | 1029 | `	ph7_class *pClass = pLeft ? NativeCmpClass(pLeft->pClass) : 0;` |
|        - | 1030 | `	PH7_NativeCmpCtx sCtx;` |
|        - | 1031 | `	ph7_vm *pVm;` |
|      345 | 1032 | `	if( pClass == 0 ){` |
|      225 | 1033 | `		return 0;` |
|        - | 1034 | `	}` |
|      124 | 1035 | `	pVm = pLeft->pVm;` |
|      124 | 1036 | `	SyZero(&sCtx,sizeof(sCtx));` |
|      124 | 1037 | `	sCtx.pOtherValue = pOther;` |
|      124 | 1038 | `	sCtx.bReversed = bReversed;` |
|      124 | 1039 | `	sCtx.iResult = 1;   /* php's ZEND_UNCOMPARABLE */` |
|      124 | 1040 | `	pClass->xCmp(pVm,pLeft,&sCtx);` |
|      124 | 1041 | `	if( sCtx.zThrowClass && pVm->zCmpRefusalClass == 0 ){` |
|      ! 0 | 1042 | `		pVm->zCmpRefusalClass = sCtx.zThrowClass;` |
|      ! 0 | 1043 | `		SyMemcpy(sCtx.zThrowMsg,pVm->zCmpRefusalMsg,sizeof(pVm->zCmpRefusalMsg));` |
|      ! 0 | 1044 | `		pVm->zCmpRefusalMsg[sizeof(pVm->zCmpRefusalMsg)-1] = 0;` |
|      ! 0 | 1045 | `	}` |
|      124 | 1046 | `	if( !sCtx.bAnswered ){` |
|       29 | 1047 | `		return 0;` |
|        - | 1048 | `	}` |
|       97 | 1049 | `	*pResult = sCtx.iResult;` |
|       97 | 1050 | `	return 1;` |
|      175 | 1051 | `}` |
|        - | 1052 | `/*` |
|        - | 1053 | ` * Install a compare handler on a mounted native class. Called by the owning` |
|        - | 1054 | ` * installer right after PH7_InstallNativeClasses, for the reason xClone, xDim` |
|        - | 1055 | ` * and xSet are: the spec table has no field for a hook.` |
|        - | 1056 | ` */` |
|        - | 1057 | `/*` |
|        - | 1058 | ` * php's cast_object handler for _IS_BOOL, which is the one conversion an object` |
|        - | 1059 | ` * may decide for itself. Answers 1 when the class HAS a handler, with the truth` |
|        - | 1060 | ` * value in *pOut; every other class keeps php's rule that an object is truthy.` |
|        - | 1061 | ` */` |
|      336 | 1062 | `PH7_PRIVATE int PH7_ClassNativeBool(ph7_class_instance *pThis,int *pOut)` |
|        5 | 1063 | `{` |
|        - | 1064 | `	ph7_class *pClass;` |
|      843 | 1065 | `	for( pClass = pThis ? pThis->pClass : 0 ; pClass ; pClass = pClass->pBase ){` |
|      527 | 1066 | `		if( pClass->xBool ){` |
|       22 | 1067 | `			*pOut = pClass->xBool(pThis->pVm,pThis) ? 1 : 0;` |
|       22 | 1068 | `			return 1;` |
|        - | 1069 | `		}` |
|      254 | 1070 | `	}` |
|      321 | 1071 | `	return 0;` |
|      171 | 1072 | `}` |
|     7925 | 1073 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallBoolHook(ph7_vm *pVm,const char *zClass,` |
|        - | 1074 | `	int (*xBool)(ph7_vm *,ph7_class_instance *))` |
|        5 | 1075 | `{` |
|     7930 | 1076 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|     7930 | 1077 | `	if( pClass == 0 ){` |
|      ! 0 | 1078 | `		return SXERR_NOTFOUND;` |
|        - | 1079 | `	}` |
|     7930 | 1080 | `	pClass->xBool = xBool;` |
|     7930 | 1081 | `	return SXRET_OK;` |
|     3962 | 1082 | `}` |
|     7925 | 1083 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallArithHook(ph7_vm *pVm,const char *zClass,` |
|        - | 1084 | `	void (*xArith)(ph7_vm *,ph7_class_instance *,PH7_NativeArithCtx *))` |
|        5 | 1085 | `{` |
|     7930 | 1086 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|     7930 | 1087 | `	if( pClass == 0 ){` |
|      ! 0 | 1088 | `		return SXERR_NOTFOUND;` |
|        - | 1089 | `	}` |
|     7930 | 1090 | `	pClass->xArith = xArith;` |
|     7930 | 1091 | `	return SXRET_OK;` |
|     3962 | 1092 | `}` |
|        - | 1093 | `/*` |
|        - | 1094 | ` * php's compare handler for an OPAQUE HANDLE class -- one whose object stands for` |
|        - | 1095 | ` * something outside the engine (a curl easy/multi/share handle, a PDO connection,` |
|        - | 1096 | ` * a statement, a lazy row). php gives each of them a handler that recognizes` |
|        - | 1097 | ` * NOTHING, so every comparison that is not the identity shortcut is` |
|        - | 1098 | `` * ZEND_UNCOMPARABLE: `$h == 1`, `$h < 2`, `$h > 0` and `$h == $other` are all`` |
|        - | 1099 | `` * false, `$h <=> $x` is 1 from either direction, and none of it says a word.`` |
|        - | 1100 | ` * Without it these fell through to php's cast-the-object rule, which warns` |
|        - | 1101 | `` * `could not be converted to int` and then calls the handle equal to 1 -- so`` |
|        - | 1102 | ` * in_array($h, [1,2,3]) was TRUE, and sorting a list that held one was noise.` |
|        - | 1103 | ` *` |
|        - | 1104 | ` * The uncomparable 1 is deliberately not flipped for bReversed: php answers it` |
|        - | 1105 | ` * from both sides alike, which is what leaves every relational spelling false.` |
|        - | 1106 | ` */` |
|      108 | 1107 | `PH7_PRIVATE void PH7_NativeCmpOpaqueHandle(ph7_vm *pVm,ph7_class_instance *pThis,` |
|        - | 1108 | `	PH7_NativeCmpCtx *pCtx)` |
|        3 | 1109 | `{` |
|       54 | 1110 | `	SXUNUSED(pVm);` |
|       54 | 1111 | `	SXUNUSED(pThis);` |
|      111 | 1112 | `	if( pCtx->pOtherValue && (pCtx->pOtherValue->iFlags & MEMOBJ_BOOL) ){` |
|       16 | 1113 | `		return;   /* declined: php's cast rule decides an object against a bool */` |
|        - | 1114 | `	}` |
|       97 | 1115 | `	pCtx->bAnswered = 1;` |
|       97 | 1116 | `	pCtx->iResult = 1;   /* php's ZEND_UNCOMPARABLE */` |
|       57 | 1117 | `}` |
|        - | 1118 | `/*` |
|        - | 1119 | `` * TRUE when `(int)` on an instance of this class answers the object handle`` |
|        - | 1120 | ` * (PH7_CLASS_HANDLE_ID). Resolved through the ANCESTORS, like every other native` |
|        - | 1121 | ` * hook: php installs the cast on the class's object handlers, and a subclass` |
|        - | 1122 | ` * inherits the whole handler table.` |
|        - | 1123 | ` */` |
|       40 | 1124 | `PH7_PRIVATE int PH7_ClassCastsToHandleId(ph7_class *pClass)` |
|        3 | 1125 | `{` |
|       63 | 1126 | `	while( pClass ){` |
|       43 | 1127 | `		if( pClass->iFlags & PH7_CLASS_HANDLE_ID ){` |
|       21 | 1128 | `			return 1;` |
|        - | 1129 | `		}` |
|       23 | 1130 | `		pClass = pClass->pBase;` |
|        3 | 1131 | `	}` |
|       23 | 1132 | `	return 0;` |
|       23 | 1133 | `}` |
|        - | 1134 | `/* The same base-chain question for the two flags beside it: a subclass of` |
|        - | 1135 | ` * SimpleXMLElement casts to a number and answers get_object_vars the way its` |
|        - | 1136 | ` * parent does, which is php's handler inheritance. */` |
|      114 | 1137 | `PH7_PRIVATE int PH7_ClassNumberIsString(ph7_class *pClass)` |
|        4 | 1138 | `{` |
|      212 | 1139 | `	while( pClass ){` |
|      118 | 1140 | `		if( pClass->iFlags & PH7_CLASS_NUM_AS_STRING ){` |
|       21 | 1141 | `			return 1;` |
|        - | 1142 | `		}` |
|       98 | 1143 | `		pClass = pClass->pBase;` |
|        4 | 1144 | `	}` |
|       98 | 1145 | `	return 0;` |
|       61 | 1146 | `}` |
|      354 | 1147 | `PH7_PRIVATE int PH7_ClassVarsFromPresent(ph7_class *pClass)` |
|        5 | 1148 | `{` |
|      775 | 1149 | `	while( pClass ){` |
|      423 | 1150 | `		if( pClass->iFlags & PH7_CLASS_VARS_PRESENT ){` |
|        3 | 1151 | `			return 1;` |
|        - | 1152 | `		}` |
|      421 | 1153 | `		pClass = pClass->pBase;` |
|        5 | 1154 | `	}` |
|      357 | 1155 | `	return 0;` |
|      182 | 1156 | `}` |
|    55475 | 1157 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallCmpHook(ph7_vm *pVm,const char *zClass,` |
|        - | 1158 | `	void (*xCmp)(ph7_vm *,ph7_class_instance *,PH7_NativeCmpCtx *))` |
|        5 | 1159 | `{` |
|    55480 | 1160 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|    55480 | 1161 | `	if( pClass == 0 ){` |
|      ! 0 | 1162 | `		return SXERR_NOTFOUND;` |
|        - | 1163 | `	}` |
|    55480 | 1164 | `	pClass->xCmp = xCmp;` |
|    55480 | 1165 | `	return SXRET_OK;` |
|    27704 | 1166 | `}` |
|        - | 1167 | `/*` |
|        - | 1168 | ` * Mark every INSTANCE property a mounted native class declares as one php` |
|        - | 1169 | ` * FABRICATES rather than stores (PH7_CLASS_ATTR_NATIVE_VIRTUAL), which is what` |
|        - | 1170 | ` * keeps the object comparator from seeing it. DatePeriod is the whole caller` |
|        - | 1171 | ` * list: php's object has an EMPTY real property table, so two of them are equal` |
|        - | 1172 | ` * whatever they contain, while a subclass's own property still decides.` |
|        - | 1173 | ` */` |
|    15850 | 1174 | `PH7_PRIVATE sxi32 PH7_NativeClassMarkVirtualProps(ph7_vm *pVm,const char *zClass)` |
|        5 | 1175 | `{` |
|    15855 | 1176 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|        - | 1177 | `	SyHashEntry *pEntry;` |
|    15855 | 1178 | `	if( pClass == 0 ){` |
|      ! 0 | 1179 | `		return SXERR_NOTFOUND;` |
|        - | 1180 | `	}` |
|    15855 | 1181 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|   103041 | 1182 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|    79255 | 1183 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|    79255 | 1184 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|    79255 | 1185 | `			pAttr->iFlags \|= PH7_CLASS_ATTR_NATIVE_VIRTUAL;` |
|    39570 | 1186 | `		}` |
|        5 | 1187 | `	}` |
|    15855 | 1188 | `	return SXRET_OK;` |
|     7919 | 1189 | `}` |
|        - | 1190 | `/*` |
|        - | 1191 | ` * Mark every php-VISIBLE instance property a mounted native class declares as one` |
|        - | 1192 | ` * the OBJECT does not hold until its constructor fills it` |
|        - | 1193 | ` * (PH7_CLASS_ATTR_NATIVE_LAZY). php's DateInterval and DatePeriod are the caller` |
|        - | 1194 | ` * list: the state is a C struct the constructor allocates and the property table` |
|        - | 1195 | ` * is written FROM it, so an object nobody constructed has no such property at all.` |
|        - | 1196 | ` *` |
|        - | 1197 | ` * The HIDDEN slots are left alone -- they are PHL's own storage, they have to` |
|        - | 1198 | `` * exist from `new` (the initialized FLAG lives in one of them), and php shows`` |
|        - | 1199 | ` * nothing for them either way.` |
|        - | 1200 | ` *` |
|        - | 1201 | ` * bDefaultRead selects which of php's two handlers the class has: with it, a read` |
|        - | 1202 | ` * of a still-absent slot answers the DECLARED literal in silence (DatePeriod's` |
|        - | 1203 | ` * read_property over the zeroed struct); without it, the name really is undefined` |
|        - | 1204 | ` * until the constructor runs (DateInterval).` |
|        - | 1205 | ` */` |
|        - | 1206 | `/*` |
|        - | 1207 | ` * Mark every php-VISIBLE instance property a mounted native class declares as one` |
|        - | 1208 | ` * whose WRITE php's handler refuses (PH7_CLASS_ATTR_NATIVE_NOWRITE). DatePeriod is` |
|        - | 1209 | `` * the caller list: php answers `Cannot modify readonly property DatePeriod::$p` to`` |
|        - | 1210 | `` * every write form and `Cannot unset DatePeriod::$p` to an unset, while Reflection`` |
|        - | 1211 | ` * still reports isReadOnly() false -- the wording is the handler's, not the` |
|        - | 1212 | ` * readonly flag's. The C bodies that fill the seven write their slots directly and` |
|        - | 1213 | ` * never pass the store filter, so the refusal costs the class nothing.` |
|        - | 1214 | ` */` |
|     7925 | 1215 | `PH7_PRIVATE sxi32 PH7_NativeClassMarkNoWriteProps(ph7_vm *pVm,const char *zClass)` |
|        5 | 1216 | `{` |
|     7930 | 1217 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|        - | 1218 | `	SyHashEntry *pEntry;` |
|     7930 | 1219 | `	if( pClass == 0 ){` |
|      ! 0 | 1220 | `		return SXERR_NOTFOUND;` |
|        - | 1221 | `	}` |
|     7930 | 1222 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|    71330 | 1223 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|    63405 | 1224 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|    63405 | 1225 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_HIDDEN) ){` |
|     7930 | 1226 | `			continue;` |
|        - | 1227 | `		}` |
|    55480 | 1228 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_NATIVE_NOWRITE;` |
|        5 | 1229 | `	}` |
|     7930 | 1230 | `	return SXRET_OK;` |
|     3962 | 1231 | `}` |
|        - | 1232 | `/*` |
|        - | 1233 | ` * Mark ONE property of ONE instance as php's read-only kind: a plain store and` |
|        - | 1234 | `` * an unset() refuse with `Property p is read only`, everything that takes a`` |
|        - | 1235 | ` * pointer to it goes through. It is marked per OBJECT because php's handler is` |
|        - | 1236 | ` * -- a PDOStatement with no cursor behind it takes the write, and only the one` |
|        - | 1237 | ` * a driver built refuses.` |
|        - | 1238 | ` */` |
|      920 | 1239 | `PH7_PRIVATE void PH7_NativeMarkAttrReadOnly(ph7_class_instance *pThis,const char *zProp)` |
|        3 | 1240 | `{` |
|      923 | 1241 | `	SyHashEntry *pEntry = pThis` |
|      920 | 1242 | `		? SyHashGet(&pThis->hAttr,(const void *)zProp,(sxu32)SyStrlen(zProp)) : 0;` |
|      923 | 1243 | `	if( pEntry ){` |
|      923 | 1244 | `		((VmClassAttr *)pEntry->pUserData)->iState \|= VM_CLASS_ATTR_RDONLY;` |
|      460 | 1245 | `	}` |
|      923 | 1246 | `}` |
|    23775 | 1247 | `PH7_PRIVATE sxi32 PH7_NativeClassMarkLazyProps(ph7_vm *pVm,const char *zClass,int bDefaultRead)` |
|        5 | 1248 | `{` |
|    23780 | 1249 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|        - | 1250 | `	SyHashEntry *pEntry;` |
|    23780 | 1251 | `	if( pClass == 0 ){` |
|      ! 0 | 1252 | `		return SXERR_NOTFOUND;` |
|        - | 1253 | `	}` |
|    23780 | 1254 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|   213980 | 1255 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|   190205 | 1256 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   190205 | 1257 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_HIDDEN) ){` |
|    39630 | 1258 | `			continue;` |
|        - | 1259 | `		}` |
|   150580 | 1260 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_NATIVE_LAZY;` |
|   150580 | 1261 | `		if( bDefaultRead ){` |
|    55480 | 1262 | `			pAttr->iFlags \|= PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT;` |
|    27699 | 1263 | `		}` |
|   150580 | 1264 | `		pClass->iFlags \|= PH7_CLASS_LAZY_ATTR;` |
|        5 | 1265 | `	}` |
|    23780 | 1266 | `	return SXRET_OK;` |
|    11876 | 1267 | `}` |
|        - | 1268 | `/*` |
|        - | 1269 | ` * Install this object's LAZY properties -- the whole set, in the order the class` |
|        - | 1270 | ` * declares them, skipping any the object already carries.` |
|        - | 1271 | ` *` |
|        - | 1272 | ` * The ORDER is php's: its constructor writes the struct's fields into the property` |
|        - | 1273 | ` * table one after another, so a name the object already has keeps its POSITION and` |
|        - | 1274 | ` * only takes the new value, and the rest are appended in declared order behind it.` |
|        - | 1275 | ` * VmRecreateDeclaredAttr tail-inserts exactly that way.` |
|        - | 1276 | ` *` |
|        - | 1277 | ` * The declared literal goes in as the slot's starting value (php's zeroed struct),` |
|        - | 1278 | ` * and the not-yet-initialized mark a TYPED slot would carry is cleared with it:` |
|        - | 1279 | ` * these are filled by the C body that is about to write them, and a read between` |
|        - | 1280 | ` * the two is php's default, not its Error.` |
|        - | 1281 | ` */` |
|  1612224 | 1282 | `PH7_PRIVATE void PH7_NativeMaterializeLazy(ph7_vm *pVm,ph7_class_instance *pObj)` |
|        5 | 1283 | `{` |
|        - | 1284 | `	SyHashEntry *pEntry;` |
|  1612224 | 1285 | `	if( pObj == 0 \|\| (pObj->pClass->iFlags & PH7_CLASS_LAZY_ATTR) == 0` |
|   810444 | 1286 | `	 \|\| (pObj->iFlags & VM_INSTANCE_LAZY_DONE) ){` |
|  1611455 | 1287 | `		return;` |
|        - | 1288 | `	}` |
|      777 | 1289 | `	pObj->iFlags \|= VM_INSTANCE_LAZY_DONE;` |
|      777 | 1290 | `	SyHashResetLoopCursor(&pObj->pClass->hAttr);` |
|    10051 | 1291 | `	while( (pEntry = SyHashGetNextEntry(&pObj->pClass->hAttr)) != 0 ){` |
|     9277 | 1292 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     9277 | 1293 | `		VmClassAttr *pVmAttr = 0;` |
|        - | 1294 | `		ph7_value *pSlot;` |
|     9274 | 1295 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_LAZY) == 0` |
|     8573 | 1296 | `		 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_ONDEMAND) != 0 ){` |
|     2023 | 1297 | `			continue;   /* ...and an ON-DEMAND one waits for the write that names it */` |
|        - | 1298 | `		}` |
|     7257 | 1299 | `		if( SyHashGet(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName)) != 0 ){` |
|      ! 0 | 1300 | `			continue;` |
|        - | 1301 | `		}` |
|     7257 | 1302 | `		VmRecreateDeclaredAttr(&(*pVm),pObj,pAttr,&pVmAttr);` |
|     7257 | 1303 | `		if( pVmAttr == 0 ){` |
|      ! 0 | 1304 | `			continue;   /* OOM: the caller's write lands nowhere, as it would have anyway */` |
|        - | 1305 | `		}` |
|     7257 | 1306 | `		pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|     7257 | 1307 | `		if( pAttr->pNativeValue == 0 ){` |
|      ! 0 | 1308 | `			continue;` |
|        - | 1309 | `		}` |
|     7257 | 1310 | `		pSlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pVmAttr->nIdx);` |
|     7257 | 1311 | `		if( pSlot ){` |
|     7257 | 1312 | `			PH7_NativeLiteralValue(&(*pVm),pAttr->pNativeValue,pSlot);` |
|     3627 | 1313 | `		}` |
|        3 | 1314 | `	}` |
|   806078 | 1315 | `}` |
|        - | 1316 | `/*` |
|        - | 1317 | ` * Create and install ONE class from its spec: constants and properties, but` |
|        - | 1318 | ` * neither methods nor its base chain.` |
|        - | 1319 | ` *` |
|        - | 1320 | ` * Split from the two passes that follow because a spec table may describe` |
|        - | 1321 | ` * classes that extend each other, and PH7_ClassInherit needs the parent to` |
|        - | 1322 | ` * exist -- and to already CARRY ITS METHODS, since inheriting is what copies` |
|        - | 1323 | ` * them down.` |
|        - | 1324 | ` */` |
|  1703875 | 1325 | `static sxi32 NativeDeclareClass(ph7_vm *pVm,const PH7_NativeClassSpec *pSpec,ph7_class **ppOut)` |
|        5 | 1326 | `{` |
|        - | 1327 | `	ph7_class *pClass;` |
|        - | 1328 | `	SyString sName;` |
|        - | 1329 | `	sxu32 n;` |
|        - | 1330 | `	sxi32 rc;` |
|  1703880 | 1331 | `	SyStringInitFromBuf(&sName,pSpec->zName,SyStrlen(pSpec->zName));` |
|  1703880 | 1332 | `	pClass = PH7_NewRawClass(&(*pVm),&sName,0);` |
|  1703880 | 1333 | `	if( pClass == 0 ){` |
|      ! 0 | 1334 | `		return SXERR_MEM;` |
|        - | 1335 | `	}` |
|        - | 1336 | `	/* PH7_CLASS_BOUND: an engine class is declared unconditionally, exactly once,` |
|        - | 1337 | ``	 * so `class DateTime {}` in user code is php's "Cannot redeclare class`` |
|        - | 1338 | `	 * DateTime". Only the compiler used to set the flag, so EVERY native class` |
|        - | 1339 | `	 * was silently redeclarable — the chunk-declared ones (Exception, stdClass)` |
|        - | 1340 | `	 * fataled and their C replacements did not. */` |
|  1703880 | 1341 | `	pClass->iFlags \|= pSpec->iFlags \| PH7_CLASS_BOUND;` |
|  1703880 | 1342 | `	pClass->xRelease = pSpec->xRelease;` |
|  1703880 | 1343 | `	pClass->pIterVtab = pSpec->pIterVtab;` |
|  1703880 | 1344 | `	pClass->xPresent = pSpec->xPresent;` |
|  4850105 | 1345 | `	for( n = 0 ; n < pSpec->nConst ; n++ ){` |
|  3146230 | 1346 | `		rc = NativeInstallConstant(&(*pVm),pClass,&pSpec->aConst[n]);` |
|  3146230 | 1347 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1348 | `			return rc;` |
|        - | 1349 | `		}` |
|  1570934 | 1350 | `	}` |
|  4675755 | 1351 | `	for( n = 0 ; n < pSpec->nProp ; n++ ){` |
|  2971880 | 1352 | `		rc = PH7_NativeClassInstallProperty(&(*pVm),pClass,&pSpec->aProp[n]);` |
|  2971880 | 1353 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1354 | `			return rc;` |
|        - | 1355 | `		}` |
|  1483880 | 1356 | `	}` |
|  1703880 | 1357 | `	*ppOut = pClass;` |
|  1703880 | 1358 | `	return PH7_VmInstallClass(&(*pVm),pClass);` |
|   850760 | 1359 | `}` |
|        - | 1360 | `/*` |
|        - | 1361 | ` * Wire ONE class's base chain and interfaces.` |
|        - | 1362 | ` *` |
|        - | 1363 | ` * This runs AFTER every class in the table has its own methods, which is the` |
|        - | 1364 | ` * compiler's order too (GenStateCompileClassEx compiles the whole body and only` |
|        - | 1365 | ` * then calls PH7_ClassInherit/PH7_ClassImplement). Two things depend on it:` |
|        - | 1366 | ` * PH7_ClassInherit COPIES the base's methods into the subclass, so a base whose` |
|        - | 1367 | ` * methods were not installed yet hands down an empty table -- which is how` |
|        - | 1368 | `` * `DOMDocument::C14N()` came out undefined, the first time a native class`` |
|        - | 1369 | ` * extended another native class that had methods; and PH7_ClassImplement stubs` |
|        - | 1370 | ` * every interface method the class lacks as ABSTRACT, so a spec may now name an` |
|        - | 1371 | ` * interface it implements itself rather than attaching it by hand afterwards.` |
|        - | 1372 | ` */` |
|  1703875 | 1373 | `static sxi32 NativeLinkClass(ph7_vm *pVm,const PH7_NativeClassSpec *pSpec,ph7_class *pClass)` |
|        5 | 1374 | `{` |
|        - | 1375 | `	sxi32 rc;` |
|  1703880 | 1376 | `	if( pSpec->zParent ){` |
|   840055 | 1377 | `		ph7_class *pBase = NativeLookupClass(&(*pVm),pSpec->zParent);` |
|   840055 | 1378 | `		if( pBase == 0 ){` |
|      ! 0 | 1379 | `			return SXERR_NOTFOUND;` |
|        - | 1380 | `		}` |
|        - | 1381 | `		/* The VM's OWN generator state, not a NULL one: PH7_ClassInherit takes its` |
|        - | 1382 | `		 * scratch allocator from pGen->pVm and reports every inheritance rule it` |
|        - | 1383 | `		 * enforces through PH7_GenCompileError(pGen, ...). Passing 0 crashed the` |
|        - | 1384 | `		 * moment a native spec first named a parent (the date exceptions). */` |
|  1260663 | 1385 | `		rc = (pClass->iFlags & PH7_CLASS_INTERFACE)` |
|    71325 | 1386 | `			? PH7_ClassInterfaceInherit(pClass,pBase)` |
|   804437 | 1387 | `			: PH7_ClassInherit(&pVm->sCodeGen,pClass,pBase);` |
|   840055 | 1388 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1389 | `			return rc;` |
|        - | 1390 | `		}` |
|   419442 | 1391 | `	}` |
|  1703880 | 1392 | `	if( pSpec->zImplements ){` |
|        - | 1393 | `		/* Comma-separated list, so one spec row can name several interfaces. */` |
|   427955 | 1394 | `		const char *zCur = pSpec->zImplements;` |
|  1077805 | 1395 | `		while( zCur[0] != '\0' ){` |
|        - | 1396 | `			const char *zStart;` |
|        - | 1397 | `			char zIface[64];` |
|        - | 1398 | `			sxu32 nLen;` |
|        - | 1399 | `			ph7_class *pIface;` |
|   871755 | 1400 | `			while( zCur[0] == ' ' \|\| zCur[0] == ',' ){` |
|   221905 | 1401 | `				zCur++;` |
|        5 | 1402 | `			}` |
|   649855 | 1403 | `			if( zCur[0] == '\0' ){` |
|      ! 0 | 1404 | `				break;` |
|        - | 1405 | `			}` |
|   649855 | 1406 | `			zStart = zCur;` |
|  8551080 | 1407 | `			while( zCur[0] != '\0' && zCur[0] != ',' && zCur[0] != ' ' ){` |
|  7901230 | 1408 | `				zCur++;` |
|        5 | 1409 | `			}` |
|   649855 | 1410 | `			nLen = (sxu32)(zCur - zStart);` |
|   649855 | 1411 | `			if( nLen >= sizeof(zIface) ){` |
|      ! 0 | 1412 | `				return SXERR_SYNTAX;` |
|        - | 1413 | `			}` |
|   649855 | 1414 | `			SyMemcpy(zStart,zIface,nLen);` |
|   649855 | 1415 | `			zIface[nLen] = '\0';` |
|   649855 | 1416 | `			pIface = NativeLookupClass(&(*pVm),zIface);` |
|   649855 | 1417 | `			if( pIface == 0 ){` |
|      ! 0 | 1418 | `				return SXERR_NOTFOUND;` |
|        - | 1419 | `			}` |
|   649855 | 1420 | `			rc = PH7_ClassImplement(pClass,pIface);` |
|   649855 | 1421 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1422 | `				return rc;` |
|        - | 1423 | `			}` |
|        5 | 1424 | `		}` |
|   213678 | 1425 | `	}` |
|  1703880 | 1426 | `	return SXRET_OK;` |
|   850760 | 1427 | `}` |
|        - | 1428 | `/*` |
|        - | 1429 | ` * Install a whole table of native classes, in the compiler's own order: declare` |
|        - | 1430 | ` * them all (so later rows may extend earlier ones), fill in their methods, wire` |
|        - | 1431 | ` * the base chains and interfaces, then mount.` |
|        - | 1432 | ` *` |
|        - | 1433 | ` * Interfaces are declared but never mounted -- VmMountUserClass skips them anyway,` |
|        - | 1434 | ` * their methods being non-invocable.` |
|        - | 1435 | ` */` |
|   404175 | 1436 | `PH7_PRIVATE sxi32 PH7_InstallNativeClasses(ph7_vm *pVm,const PH7_NativeClassSpec *aSpec,sxu32 nSpec)` |
|        5 | 1437 | `{` |
|        - | 1438 | `	ph7_class **apClass;` |
|        - | 1439 | `	sxu32 i,j;` |
|        - | 1440 | `	sxi32 rc;` |
|   404180 | 1441 | `	apClass = (ph7_class **)SyMemBackendAlloc(&pVm->sAllocator,nSpec * sizeof(ph7_class *));` |
|   404180 | 1442 | `	if( apClass == 0 ){` |
|      ! 0 | 1443 | `		return SXERR_MEM;` |
|        - | 1444 | `	}` |
|  2108055 | 1445 | `	for( i = 0 ; i < nSpec ; i++ ){` |
|  1703880 | 1446 | `		apClass[i] = 0;` |
|  1703880 | 1447 | `		rc = NativeDeclareClass(&(*pVm),&aSpec[i],&apClass[i]);` |
|  1703880 | 1448 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1449 | `			goto Done;` |
|        - | 1450 | `		}` |
|   850760 | 1451 | `	}` |
|  2108055 | 1452 | `	for( i = 0 ; i < nSpec ; i++ ){` |
| 12814730 | 1453 | `		for( j = 0 ; j < aSpec[i].nMethod ; j++ ){` |
| 11110855 | 1454 | `			rc = PH7_NativeClassInstallMethod(&(*pVm),apClass[i],&aSpec[i].aMethod[j],0);` |
| 11110855 | 1455 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1456 | `				goto Done;` |
|        - | 1457 | `			}` |
|  5547719 | 1458 | `		}` |
|   850760 | 1459 | `	}` |
|  2108055 | 1460 | `	for( i = 0 ; i < nSpec ; i++ ){` |
|  1703880 | 1461 | `		rc = NativeLinkClass(&(*pVm),&aSpec[i],apClass[i]);` |
|  1703880 | 1462 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1463 | `			goto Done;` |
|        - | 1464 | `		}` |
|   850760 | 1465 | `	}` |
|  2108055 | 1466 | `	for( i = 0 ; i < nSpec ; i++ ){` |
|  1703880 | 1467 | `		rc = VmMountUserClass(&(*pVm),apClass[i]);` |
|  1703880 | 1468 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1469 | `			goto Done;` |
|        - | 1470 | `		}` |
|   850760 | 1471 | `	}` |
|   404180 | 1472 | `	rc = SXRET_OK;` |
|   202368 | 1473 | `Done:` |
|   404180 | 1474 | `	SyMemBackendFree(&pVm->sAllocator,apClass);` |
|   404180 | 1475 | `	return rc;` |
|   201812 | 1476 | `}` |
|        - | 1477 | `/*` |
|        - | 1478 | ` * ---------------------------------------------------------------------------` |
|        - | 1479 | ` * Declaring an ENUM from C.` |
|        - | 1480 | ` *` |
|        - | 1481 | `` * An enum is not a class with constants: each `case` is a class constant whose`` |
|        - | 1482 | ` * slot holds THE singleton instance of the enum for that case, materialized` |
|        - | 1483 | ` * lazily on first access (VmEnumMaterializeCase). The compiler builds one by` |
|        - | 1484 | `` * declaring the readonly `name`/`value` properties, pushing each case onto`` |
|        - | 1485 | ` * ph7_class::aEnumCases, and SYNTHESIZING cases()/from()/tryFrom() as PHP` |
|        - | 1486 | `` * source that forwards to the `__phl_enum_*` thunks.`` |
|        - | 1487 | ` *` |
|        - | 1488 | ` * This does the same three things without a compiler: the case's backing value` |
|        - | 1489 | ` * rides as a literal (ph7_class_attr::pNativeValue, which the materializer` |
|        - | 1490 | ` * reads where a compiled case has byte-code), and the three interface methods` |
|        - | 1491 | ` * are C bodies that call the very same engine workers the synthesized PHP` |
|        - | 1492 | `` * forwards to — so a native enum is an ordinary one to `instanceof`,`` |
|        - | 1493 | `` * `match`, Reflection and `===` case identity.`` |
|        - | 1494 | ` * ---------------------------------------------------------------------------` |
|        - | 1495 | ` */` |
|        - | 1496 | `/* The enum a static native method was called on. */` |
|      199 | 1497 | `static ph7_class * NativeEnumSelf(ph7_context *pCtx,ph7_value *pName)` |
|        2 | 1498 | `{` |
|      201 | 1499 | `	ph7_class *pClass = PH7_ContextCalledClass(pCtx);` |
|      201 | 1500 | `	PH7_MemObjInit(pCtx->pVm,pName);` |
|      201 | 1501 | `	if( pClass ){` |
|      201 | 1502 | `		ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|       99 | 1503 | `	}` |
|      201 | 1504 | `	return pClass;` |
|        2 | 1505 | `}` |
|        - | 1506 | `/*` |
|        - | 1507 | ` * cases() / from() / tryFrom(): the engine thunks take the enum's FQN as their` |
|        - | 1508 | ` * first argument, exactly as the compiler's synthesized bodies pass it.` |
|        - | 1509 | ` */` |
|       21 | 1510 | `static int vm_builtin_NativeEnum_cases(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1511 | `{` |
|        - | 1512 | `	ph7_value sName;` |
|        - | 1513 | `	ph7_value *ap[1];` |
|        - | 1514 | `	int rc;` |
|       10 | 1515 | `	SXUNUSED(nArg);` |
|       10 | 1516 | `	SXUNUSED(apArg);` |
|       22 | 1517 | `	if( NativeEnumSelf(pCtx,&sName) == 0 ){` |
|      ! 0 | 1518 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1519 | `		return PH7_OK;` |
|        - | 1520 | `	}` |
|       22 | 1521 | `	ap[0] = &sName;` |
|       22 | 1522 | `	rc = vm_builtin_enum_cases(pCtx,1,ap);` |
|       22 | 1523 | `	PH7_MemObjRelease(&sName);` |
|       22 | 1524 | `	return rc;` |
|       11 | 1525 | `}` |
|      178 | 1526 | `static int NativeEnumFrom(ph7_context *pCtx,int nArg,ph7_value **apArg,int bTry)` |
|        2 | 1527 | `{` |
|        - | 1528 | `	ph7_value sName;` |
|        - | 1529 | `	ph7_value *ap[2];` |
|        - | 1530 | `	int rc;` |
|      180 | 1531 | `	if( nArg < 1 \|\| NativeEnumSelf(pCtx,&sName) == 0 ){` |
|      ! 0 | 1532 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1533 | `		return PH7_OK;` |
|        - | 1534 | `	}` |
|      180 | 1535 | `	ap[0] = &sName;` |
|      180 | 1536 | `	ap[1] = apArg[0];` |
|      180 | 1537 | `	rc = bTry ? vm_builtin_enum_tryfrom(pCtx,2,ap) : vm_builtin_enum_from(pCtx,2,ap);` |
|      180 | 1538 | `	PH7_MemObjRelease(&sName);` |
|      180 | 1539 | `	return rc;` |
|       91 | 1540 | `}` |
|       96 | 1541 | `static int vm_builtin_NativeEnum_from(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 1542 | `{` |
|       98 | 1543 | `	return NativeEnumFrom(pCtx,nArg,apArg,0);` |
|        2 | 1544 | `}` |
|       82 | 1545 | `static int vm_builtin_NativeEnum_tryFrom(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1546 | `{` |
|       83 | 1547 | `	return NativeEnumFrom(pCtx,nArg,apArg,1);` |
|        1 | 1548 | `}` |
|        - | 1549 | ``/* The readonly `name` (every enum) and `value` (backed only) case properties,`` |
|        - | 1550 | ` * declared exactly as GenStateCompileEnum does. */` |
|    39625 | 1551 | `static sxi32 NativeEnumInstallProp(ph7_vm *pVm,ph7_class *pClass,const char *zName,` |
|        - | 1552 | `	sxu32 nType,const char *zTypeName)` |
|        5 | 1553 | `{` |
|        - | 1554 | `	SyString sName;` |
|        - | 1555 | `	ph7_class_attr *pAttr;` |
|    39630 | 1556 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|    39630 | 1557 | `	pAttr = PH7_NewClassAttr(&(*pVm),&sName,0,PH7_CLASS_PROT_PUBLIC,` |
|        - | 1558 | `		PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|    39630 | 1559 | `	if( pAttr == 0 ){` |
|      ! 0 | 1560 | `		return SXERR_MEM;` |
|        - | 1561 | `	}` |
|    39630 | 1562 | `	pAttr->nType = nType;` |
|    39630 | 1563 | `	SyStringInitFromBuf(&pAttr->sTypeName,zTypeName,SyStrlen(zTypeName));` |
|    39630 | 1564 | `	return PH7_ClassInstallAttr(pClass,pAttr);` |
|    19790 | 1565 | `}` |
|        - | 1566 | `/*` |
|        - | 1567 | ` * cases()/from()/tryFrom(), installed on ANY enum -- one declared from C here` |
|        - | 1568 | ` * and one the compiler just finished reading from source. php declares them on` |
|        - | 1569 | ``  * the UnitEnum/BackedEnum prototypes, which is why `from` takes `string\|int` `` |
|        - | 1570 | ` * rather than the enum's own backing type: the refusal for the wrong one is a` |
|        - | 1571 | ` * VALUE check inside the body, not the parameter's.` |
|        - | 1572 | ` *` |
|        - | 1573 | ` * The compiler used to synthesize PHP source forwarding to three global` |
|        - | 1574 | `` * `__phl_enum_*` thunks. Those names were php-visible, and the methods reported`` |
|        - | 1575 | `` * as `<user>` with the enum's file and line where php reports`` |
|        - | 1576 | `` * `<internal, prototype BackedEnum>`.`` |
|        - | 1577 | ` */` |
|        - | 1578 | `/*` |
|        - | 1579 | ` * Install one of them and stamp it INTERNAL unconditionally. The usual` |
|        - | 1580 | `` * `bCompilingBuiltin` stamp only fires while the engine compiles its OWN`` |
|        - | 1581 | ` * sources, and these three are attached to a class the compiler is reading out` |
|        - | 1582 | ` * of a USER file — but php reports them as internal wherever the enum is` |
|        - | 1583 | ` * declared: isInternal() true, getFileName() false, getStartLine() 0.` |
|        - | 1584 | ` */` |
|    47812 | 1585 | `static sxi32 NativeEnumInstallMethod(ph7_vm *pVm,ph7_class *pClass,` |
|        - | 1586 | `	const PH7_NativeMethodDef *pDef)` |
|        5 | 1587 | `{` |
|        - | 1588 | `	ph7_class_method *pMeth;` |
|    47817 | 1589 | `	sxi32 rc = PH7_NativeClassInstallMethod(&(*pVm),pClass,pDef,0);` |
|    47817 | 1590 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1591 | `		return rc;` |
|        - | 1592 | `	}` |
|    47817 | 1593 | `	pMeth = PH7_ClassExtractMethod(pClass,pDef->zName,(sxu32)SyStrlen(pDef->zName));` |
|    47817 | 1594 | `	if( pMeth ){` |
|    47817 | 1595 | `		pMeth->sFunc.iFlags \|= VM_FUNC_INTERNAL;` |
|        - | 1596 | `		/* getFileName() reads the recorded source file rather than the flag, and` |
|        - | 1597 | `		 * PH7_NewClassMethod stamped the user file the enum was read from. */` |
|    47817 | 1598 | `		SyStringInitFromBuf(&pMeth->sFunc.sFile,"",0);` |
|    23873 | 1599 | `	}` |
|    47817 | 1600 | `	return SXRET_OK;` |
|    23878 | 1601 | `}` |
|    31830 | 1602 | `PH7_PRIVATE sxi32 PH7_InstallEnumInterfaceMethods(ph7_vm *pVm,ph7_class *pClass)` |
|        5 | 1603 | `{` |
|        - | 1604 | `	static const PH7_NativeMethodDef sCases =` |
|        - | 1605 | `		{ "cases", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "", "array", vm_builtin_NativeEnum_cases };` |
|        - | 1606 | `	static const PH7_NativeMethodDef aFrom[] = {` |
|        - | 1607 | `		{ "from",    PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "string\|int $value", "static",` |
|        - | 1608 | `		  vm_builtin_NativeEnum_from },` |
|        - | 1609 | `		{ "tryFrom", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "string\|int $value", "?static",` |
|        - | 1610 | `		  vm_builtin_NativeEnum_tryFrom },` |
|        - | 1611 | `	};` |
|        - | 1612 | `	sxu32 n;` |
|    31835 | 1613 | `	sxi32 rc = NativeEnumInstallMethod(&(*pVm),pClass,&sCases);` |
|    31835 | 1614 | `	if( rc != SXRET_OK \|\| pClass->nEnumBacking == 0 ){` |
|    23844 | 1615 | `		return rc;` |
|        - | 1616 | `	}` |
|    23978 | 1617 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFrom) ; n++ ){` |
|    15987 | 1618 | `		rc = NativeEnumInstallMethod(&(*pVm),pClass,&aFrom[n]);` |
|    15987 | 1619 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1620 | `			return rc;` |
|        - | 1621 | `		}` |
|     7985 | 1622 | `	}` |
|     7996 | 1623 | `	return SXRET_OK;` |
|    15898 | 1624 | `}` |
|    31700 | 1625 | `PH7_PRIVATE sxi32 PH7_InstallNativeEnum(ph7_vm *pVm,const char *zName,sxu32 nBacking,` |
|        - | 1626 | `	const PH7_NativeEnumCase *aCase,sxu32 nCase,` |
|        - | 1627 | `	const PH7_NativeMethodDef *aMethod,sxu32 nMethod)` |
|        5 | 1628 | `{` |
|        - | 1629 | `	ph7_class *pClass, *pIface;` |
|        - | 1630 | `	SyString sName;` |
|        - | 1631 | `	sxu32 n;` |
|        - | 1632 | `	sxi32 rc;` |
|    31705 | 1633 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|    31705 | 1634 | `	pClass = PH7_NewRawClass(&(*pVm),&sName,0);` |
|    31705 | 1635 | `	if( pClass == 0 ){` |
|      ! 0 | 1636 | `		return SXERR_MEM;` |
|        - | 1637 | `	}` |
|        - | 1638 | `	/* php: no enum can be extended or instantiated, and the ENUM flag alone says` |
|        - | 1639 | ``	 * so -- the `extends` refusal names the enum rather than a final class, and it`` |
|        - | 1640 | `	 * is asked first. The FINAL flag is deliberately NOT set: php stamps` |
|        - | 1641 | `` 	 * ZEND_ACC_FINAL on a COMPILED enum only, so `isFinal()`/`getModifiers()` `` |
|        - | 1642 | ``	 * answer true/32 for `enum U {}` and false/0 for every enum php declares from`` |
|        - | 1643 | `	 * C (RoundingMode, PropertyHookType). Setting it here made an internal enum` |
|        - | 1644 | `	 * report itself as a userland one. */` |
|    31705 | 1645 | `	pClass->iFlags \|= PH7_CLASS_ENUM;` |
|    31705 | 1646 | `	pClass->nEnumBacking = nBacking;` |
|    31705 | 1647 | `	rc = NativeEnumInstallProp(&(*pVm),pClass,"name",MEMOBJ_STRING,"string");` |
|    31705 | 1648 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1649 | `		return rc;` |
|        - | 1650 | `	}` |
|    31705 | 1651 | `	if( nBacking != 0 ){` |
|    11887 | 1652 | `		rc = NativeEnumInstallProp(&(*pVm),pClass,"value",nBacking,` |
|     3957 | 1653 | `			nBacking == MEMOBJ_INT ? "int" : "string");` |
|     7930 | 1654 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1655 | `			return rc;` |
|        - | 1656 | `		}` |
|     3957 | 1657 | `	}` |
|   182280 | 1658 | `	for( n = 0 ; n < nCase ; n++ ){` |
|        - | 1659 | `		ph7_class_attr *pAttr;` |
|        - | 1660 | `		SyString sCase;` |
|   150580 | 1661 | `		SyStringInitFromBuf(&sCase,aCase[n].zName,SyStrlen(aCase[n].zName));` |
|   150580 | 1662 | `		pAttr = PH7_NewClassAttr(&(*pVm),&sCase,0,PH7_CLASS_PROT_PUBLIC,` |
|        - | 1663 | `			PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|   150580 | 1664 | `		if( pAttr == 0 ){` |
|      ! 0 | 1665 | `			return SXERR_MEM;` |
|        - | 1666 | `		}` |
|   150580 | 1667 | `		pAttr->pDeclClass = pClass;` |
|        - | 1668 | `		/* The backing literal where a compiled case carries byte-code. */` |
|   150580 | 1669 | `		if( nBacking != 0 ){` |
|    15855 | 1670 | `			pAttr->pNativeValue = &aCase[n].sValue;` |
|     7914 | 1671 | `		}` |
|   150580 | 1672 | `		rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   150580 | 1673 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1674 | `			return rc;` |
|        - | 1675 | `		}` |
|        - | 1676 | `		/* Declaration order, which is the order cases() reports. */` |
|   150580 | 1677 | `		SySetPut(&pClass->aEnumCases,(const void *)&pAttr);` |
|    75188 | 1678 | `	}` |
|    31705 | 1679 | `	for( n = 0 ; n < nMethod ; n++ ){` |
|      ! 0 | 1680 | `		rc = PH7_NativeClassInstallMethod(&(*pVm),pClass,&aMethod[n],0);` |
|      ! 0 | 1681 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1682 | `			return rc;` |
|        - | 1683 | `		}` |
|      ! 0 | 1684 | `	}` |
|    31705 | 1685 | `	rc = PH7_InstallEnumInterfaceMethods(&(*pVm),pClass);` |
|    31705 | 1686 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1687 | `		return rc;` |
|        - | 1688 | `	}` |
|    31705 | 1689 | `	rc = PH7_VmInstallClass(&(*pVm),pClass);` |
|    31705 | 1690 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1691 | `		return rc;` |
|        - | 1692 | `	}` |
|        - | 1693 | ``	/* php 8.1: every enum satisfies `instanceof UnitEnum`, a backed one`` |
|        - | 1694 | ``	 * `BackedEnum` too. Attached AFTER the methods, so PH7_ClassImplement's`` |
|        - | 1695 | `	 * abstract stubbing finds cases()/from()/tryFrom() already declared.` |
|        - | 1696 | `	 * A backed one names only BackedEnum, which BRINGS UnitEnum: php's own` |
|        - | 1697 | ``	 * internal enums list `BackedEnum, UnitEnum` in that order, and naming`` |
|        - | 1698 | `	 * both here would answer them the other way round. A compiled enum is a` |
|        - | 1699 | ``	 * different registration and really does name both (`Ct, UnitEnum,`` |
|        - | 1700 | ``	 * BackedEnum`), which is what compile_class.c spells. */`` |
|    31705 | 1701 | `	pIface = NativeLookupClass(&(*pVm),nBacking != 0 ? "BackedEnum" : "UnitEnum");` |
|    31705 | 1702 | `	if( pIface == 0 ){` |
|      ! 0 | 1703 | `		return SXERR_NOTFOUND;` |
|        - | 1704 | `	}` |
|    31705 | 1705 | `	rc = PH7_ClassImplement(pClass,pIface);` |
|    31705 | 1706 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1707 | `		return rc;` |
|        - | 1708 | `	}` |
|    31705 | 1709 | `	return VmMountUserClass(&(*pVm),pClass);` |
|    15833 | 1710 | `}` |
|        - | 1711 | `/*` |
|        - | 1712 | ` * ---------------------------------------------------------------------------` |
|        - | 1713 | ` * InternalIterator.` |
|        - | 1714 | ` *` |
|        - | 1715 | ` * A native IteratorAggregate cannot answer a Generator: a PHP generator IS its` |
|        - | 1716 | ` * byte-code, and a C body has none. php never answers one either -- its internal` |
|        - | 1717 | ` * aggregates hand back an InternalIterator wrapping the iterator their class` |
|        - | 1718 | ` * declared -- so PHL declares that class once, here, and every native aggregate` |
|        - | 1719 | ` * reaches it through ph7_class::pIterVtab.` |
|        - | 1720 | ` *` |
|        - | 1721 | ` * The cursor lives entirely in the iterator's own private slots. A vtable states` |
|        - | 1722 | ` * only how to REACH a position; reading it back is the same three methods for` |
|        - | 1723 | ` * everyone.` |
|        - | 1724 | ` * ---------------------------------------------------------------------------` |
|        - | 1725 | ` */` |
|        - | 1726 | `/*` |
|        - | 1727 | ` * The walk THIS iterator was made for: the vtable of the aggregate it holds,` |
|        - | 1728 | ` * looked up ALONG THE BASE CHAIN. A subclass of a native aggregate inherits the` |
|        - | 1729 | ` * walk the way it inherits the getIterator() that reaches it -- without this a` |
|        - | 1730 | `` * `class P extends DatePeriod {}` (or DOMNodeList, WeakMap, PDOStatement,`` |
|        - | 1731 | ` * FilesystemIterator) answered a real InternalIterator that yielded NOTHING, so` |
|        - | 1732 | ` * every foreach over one was silently empty.` |
|        - | 1733 | ` */` |
|     6170 | 1734 | `static const PH7_NativeIterVtab * NativeIterVtab(ph7_class_instance *pIt)` |
|        2 | 1735 | `{` |
|     6172 | 1736 | `	ph7_class_instance *pSrc = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);` |
|        - | 1737 | `	ph7_class *pClass;` |
|     6392 | 1738 | `	for( pClass = pSrc ? pSrc->pClass : 0 ; pClass ; pClass = pClass->pBase ){` |
|     6392 | 1739 | `		if( pClass->pIterVtab ){` |
|     6172 | 1740 | `			return pClass->pIterVtab;` |
|        - | 1741 | `		}` |
|      111 | 1742 | `	}` |
|      ! 0 | 1743 | `	return 0;` |
|     3087 | 1744 | `}` |
|        - | 1745 | `/* Hand the cursor back to the aggregate, for a class that shows its walk as one` |
|        - | 1746 | ` * of its own properties (see PH7_NativeIterVtab::xPublish). Every InternalIterator` |
|        - | 1747 | ` * method calls this -- php's aggregate is written from the iterator's methods, not` |
|        - | 1748 | ` * from the walk, so a getIterator() nobody has touched yet leaves it alone. */` |
|     2504 | 1749 | `static void NativeIterPublish(ph7_vm *pVm,ph7_class_instance *pIt)` |
|        2 | 1750 | `{` |
|     2506 | 1751 | `	const PH7_NativeIterVtab *pVtab = NativeIterVtab(pIt);` |
|     2506 | 1752 | `	if( pVtab && pVtab->xPublish ){` |
|     1005 | 1753 | `		pVtab->xPublish(&(*pVm),pIt);` |
|      502 | 1754 | `	}` |
|     2506 | 1755 | `}` |
|        - | 1756 | `/* May this iterator be walked? See PH7_NativeIterVtab::xGuard -- the aggregate` |
|        - | 1757 | ` * gets to refuse at each of the five methods, which is where php refuses. */` |
|     2518 | 1758 | `static int NativeIterRefused(ph7_context *pCtx,ph7_class_instance *pIt)` |
|        2 | 1759 | `{` |
|     2520 | 1760 | `	const PH7_NativeIterVtab *pVtab = NativeIterVtab(pIt);` |
|     2520 | 1761 | `	return (pVtab && pVtab->xGuard) ? pVtab->xGuard(pCtx,pIt) : 0;` |
|        2 | 1762 | `}` |
|      272 | 1763 | `static int vm_builtin_InternalIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 1764 | `{` |
|      274 | 1765 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|        - | 1766 | `	const PH7_NativeIterVtab *pVtab;` |
|      136 | 1767 | `	SXUNUSED(nArg);` |
|      136 | 1768 | `	SXUNUSED(apArg);` |
|      274 | 1769 | `	if( pThis == 0 ){` |
|      ! 0 | 1770 | `		return PH7_OK;` |
|        - | 1771 | `	}` |
|      274 | 1772 | `	if( NativeIterRefused(pCtx,pThis) ){` |
|       15 | 1773 | `		return PH7_OK;` |
|        - | 1774 | `	}` |
|      260 | 1775 | `	pVtab = NativeIterVtab(pThis);` |
|      260 | 1776 | `	if( pVtab == 0 \|\| pVtab->xRewind == 0 ){` |
|      ! 0 | 1777 | `		PH7_NativeSetAttrBool(pCtx->pVm,pThis,PH7_NATIVE_IT_DONE,1);` |
|      ! 0 | 1778 | `		return PH7_OK;` |
|        - | 1779 | `	}` |
|      260 | 1780 | `	pVtab->xRewind(pCtx->pVm,pThis);` |
|      260 | 1781 | `	NativeIterPublish(pCtx->pVm,pThis);` |
|      260 | 1782 | `	return PH7_OK;` |
|      138 | 1783 | `}` |
|      592 | 1784 | `static int vm_builtin_InternalIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 1785 | `{` |
|      594 | 1786 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|        - | 1787 | `	const PH7_NativeIterVtab *pVtab;` |
|      296 | 1788 | `	SXUNUSED(nArg);` |
|      296 | 1789 | `	SXUNUSED(apArg);` |
|      594 | 1790 | `	if( pThis && NativeIterRefused(pCtx,pThis) ){` |
|      ! 0 | 1791 | `		return PH7_OK;` |
|        - | 1792 | `	}` |
|      594 | 1793 | `	if( pThis == 0 \|\| PH7_NativeAttrTruthy(pThis,PH7_NATIVE_IT_DONE) ){` |
|      ! 0 | 1794 | `		return PH7_OK;` |
|        - | 1795 | `	}` |
|      594 | 1796 | `	pVtab = NativeIterVtab(pThis);` |
|      594 | 1797 | `	if( pVtab == 0 \|\| pVtab->xNext == 0 ){` |
|      ! 0 | 1798 | `		PH7_NativeSetAttrBool(pCtx->pVm,pThis,PH7_NATIVE_IT_DONE,1);` |
|      ! 0 | 1799 | `		return PH7_OK;` |
|        - | 1800 | `	}` |
|      594 | 1801 | `	pVtab->xNext(pCtx->pVm,pThis);` |
|      594 | 1802 | `	NativeIterPublish(pCtx->pVm,pThis);` |
|      594 | 1803 | `	return PH7_OK;` |
|      298 | 1804 | `}` |
|      842 | 1805 | `static int vm_builtin_InternalIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 1806 | `{` |
|      844 | 1807 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      421 | 1808 | `	SXUNUSED(nArg);` |
|      421 | 1809 | `	SXUNUSED(apArg);` |
|      844 | 1810 | `	if( pThis && NativeIterRefused(pCtx,pThis) ){` |
|      ! 0 | 1811 | `		return PH7_OK;` |
|        - | 1812 | `	}` |
|      844 | 1813 | `	if( pThis ){` |
|      844 | 1814 | `		NativeIterPublish(pCtx->pVm,pThis);` |
|      421 | 1815 | `	}` |
|      844 | 1816 | `	ph7_result_bool(pCtx,pThis != 0 && !PH7_NativeAttrTruthy(pThis,PH7_NATIVE_IT_DONE));` |
|      844 | 1817 | `	return PH7_OK;` |
|      423 | 1818 | `}` |
|        - | 1819 | `/* current() and key() answer the slots the walk left behind -- and NULL once it` |
|        - | 1820 | ` * is over, which is what php's exhausted InternalIterator answers too. */` |
|      812 | 1821 | `static int NativeIterReadSlot(ph7_context *pCtx,const char *zSlot)` |
|        2 | 1822 | `{` |
|      814 | 1823 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|        - | 1824 | `	ph7_value *pVal;` |
|      814 | 1825 | `	if( pThis && NativeIterRefused(pCtx,pThis) ){` |
|      ! 0 | 1826 | `		return PH7_OK;` |
|        - | 1827 | `	}` |
|      814 | 1828 | `	if( pThis ){` |
|      814 | 1829 | `		NativeIterPublish(pCtx->pVm,pThis);` |
|      406 | 1830 | `	}` |
|      814 | 1831 | `	if( pThis == 0 \|\| PH7_NativeAttrTruthy(pThis,PH7_NATIVE_IT_DONE) ){` |
|      ! 0 | 1832 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1833 | `		return PH7_OK;` |
|        - | 1834 | `	}` |
|      814 | 1835 | `	pVal = PH7_NativeAttr(pThis,zSlot);` |
|      814 | 1836 | `	if( pVal ){` |
|      814 | 1837 | `		ph7_result_value(pCtx,pVal);` |
|      406 | 1838 | `	}` |
|      814 | 1839 | `	return PH7_OK;` |
|      408 | 1840 | `}` |
|      588 | 1841 | `static int vm_builtin_InternalIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 1842 | `{` |
|      294 | 1843 | `	SXUNUSED(nArg);` |
|      294 | 1844 | `	SXUNUSED(apArg);` |
|      590 | 1845 | `	return NativeIterReadSlot(pCtx,PH7_NATIVE_IT_CUR);` |
|        2 | 1846 | `}` |
|      224 | 1847 | `static int vm_builtin_InternalIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 1848 | `{` |
|      112 | 1849 | `	SXUNUSED(nArg);` |
|      112 | 1850 | `	SXUNUSED(apArg);` |
|      226 | 1851 | `	return NativeIterReadSlot(pCtx,PH7_NATIVE_IT_KEY);` |
|        2 | 1852 | `}` |
|        - | 1853 | `/* InternalIterator::__construct() — private in php, and never reached from PHP:` |
|        - | 1854 | ` * PH7_NativeIteratorNew builds the instance directly. */` |
|      ! 0 | 1855 | `static int vm_builtin_InternalIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 1856 | `{` |
|      ! 0 | 1857 | `	SXUNUSED(nArg);` |
|      ! 0 | 1858 | `	SXUNUSED(apArg);` |
|      ! 0 | 1859 | `	SXUNUSED(pCtx);` |
|      ! 0 | 1860 | `	return PH7_OK;` |
|      ! 0 | 1861 | `}` |
|        - | 1862 | `/*` |
|        - | 1863 | ` * The iterator a native getIterator() answers: bound to its aggregate and already` |
|        - | 1864 | ` * positioned, because php's is valid() before the first rewind().` |
|        - | 1865 | ` */` |
|      298 | 1866 | `PH7_PRIVATE ph7_class_instance * PH7_NativeIteratorNew(ph7_vm *pVm,ph7_class_instance *pSrc)` |
|        2 | 1867 | `{` |
|      300 | 1868 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),"InternalIterator",` |
|        - | 1869 | `		sizeof("InternalIterator")-1,FALSE,0);` |
|        - | 1870 | `	ph7_class_instance *pIt;` |
|        - | 1871 | `	const PH7_NativeIterVtab *pVtab;` |
|      300 | 1872 | `	if( pClass == 0 \|\| pSrc == 0 ){` |
|      ! 0 | 1873 | `		return 0;` |
|        - | 1874 | `	}` |
|      300 | 1875 | `	pIt = PH7_NewClassInstance(&(*pVm),pClass);` |
|      300 | 1876 | `	if( pIt == 0 ){` |
|      ! 0 | 1877 | `		return 0;` |
|        - | 1878 | `	}` |
|      300 | 1879 | `	PH7_NativeSetAttrObj(&(*pVm),pIt,PH7_NATIVE_IT_SRC,pSrc);` |
|      300 | 1880 | `	PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|      300 | 1881 | `	pVtab = NativeIterVtab(pIt);` |
|      300 | 1882 | `	if( pVtab && pVtab->xRewind ){` |
|      300 | 1883 | `		pVtab->xRewind(&(*pVm),pIt);` |
|      149 | 1884 | `	}` |
|      300 | 1885 | `	return pIt;` |
|      151 | 1886 | `}` |
|     7925 | 1887 | `PH7_PRIVATE sxi32 PH7_VmInstallNativeIterator(ph7_vm *pVm)` |
|        5 | 1888 | `{` |
|        - | 1889 | `	static const PH7_NativePropDef aProp[] = {` |
|        - | 1890 | `		{ PH7_NATIVE_IT_SRC,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|        - | 1891 | `		{ PH7_NATIVE_IT_CUR,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|        - | 1892 | `		{ PH7_NATIVE_IT_KEY,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|        - | 1893 | `		{ PH7_NATIVE_IT_POS,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,  0, 0, 0.0 }, 0 },` |
|        - | 1894 | `		{ PH7_NATIVE_IT_AUX,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,  0, 0, 0.0 }, 0 },` |
|        - | 1895 | `		{ PH7_NATIVE_IT_DONE, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_BOOL, 1, 0, 0.0 }, 0 },` |
|        - | 1896 | `	};` |
|        - | 1897 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|        - | 1898 | `		{ "__construct", PH7_MOD_PRIVATE, "", "", vm_builtin_InternalIterator_construct },` |
|        - | 1899 | `		{ "current",     PH7_MOD_PUBLIC, "", "mixed", vm_builtin_InternalIterator_current },` |
|        - | 1900 | `		{ "key",         PH7_MOD_PUBLIC, "", "mixed", vm_builtin_InternalIterator_key },` |
|        - | 1901 | `		{ "next",        PH7_MOD_PUBLIC, "", "void",  vm_builtin_InternalIterator_next },` |
|        - | 1902 | `		{ "valid",       PH7_MOD_PUBLIC, "", "bool",  vm_builtin_InternalIterator_valid },` |
|        - | 1903 | `		{ "rewind",      PH7_MOD_PUBLIC, "", "void",  vm_builtin_InternalIterator_rewind },` |
|        - | 1904 | `	};` |
|        - | 1905 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|        - | 1906 | `		{ "InternalIterator", 0, 0, PH7_CLASS_FINAL,` |
|        - | 1907 | `		  aMethod, SX_ARRAYSIZE(aMethod), 0, 0, aProp, SX_ARRAYSIZE(aProp), 0, 0, 0 },` |
|        - | 1908 | `	};` |
|        - | 1909 | `	ph7_class *pIt,*pIterator;` |
|        - | 1910 | `	sxi32 rc;` |
|     7930 | 1911 | `	rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     7930 | 1912 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1913 | `		return rc;` |
|        - | 1914 | `	}` |
|        - | 1915 | ``	/* `implements Iterator` only NOW: PH7_ClassImplement stubs every interface`` |
|        - | 1916 | `	 * method the class does not already declare as ABSTRACT, so attaching it` |
|        - | 1917 | `	 * through the spec would have made InternalIterator uninstantiable. */` |
|     7930 | 1918 | `	pIt = NativeLookupClass(&(*pVm),"InternalIterator");` |
|     7930 | 1919 | `	pIterator = NativeLookupClass(&(*pVm),"Iterator");` |
|     7930 | 1920 | `	if( pIt == 0 \|\| pIterator == 0 ){` |
|      ! 0 | 1921 | `		return SXERR_NOTFOUND;` |
|        - | 1922 | `	}` |
|     7930 | 1923 | `	return PH7_ClassImplement(pIt,pIterator);` |
|     3962 | 1924 | `}` |
|        - | 1925 |  |
