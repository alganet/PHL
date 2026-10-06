# src/ph7/oo_native.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 960/1065 lines (90.14%)

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
| 10275440 |   36 | `PH7_PRIVATE void PH7_NativeLiteralValue(ph7_vm *pVm,const void *pLiteral,ph7_value *pOut)` |
|        5 |   37 | `{` |
| 10275445 |   38 | `	const PH7_NativeConstDef *pLit = (const PH7_NativeConstDef *)pLiteral;` |
|        - |   39 | `	/* Every PH7_MemObjInitFrom* below SyZeros the whole value, nIdx included — and` |
|        - |   40 | `	 * a CONSTANT's or STATIC's slot is addressed by that index afterwards` |
|        - |   41 | ``	 * (`pAttr->nIdx = pMemObj->nIdx`). Losing it pointed every native class`` |
|        - |   42 | `	 * constant at slot 0, which reads as $GLOBALS. The instance-property path was` |
|        - |   43 | `	 * unaffected (it records the index before calling this), which is why nothing` |
|        - |   44 | `	 * saw it until the date family declared the first native constants. */` |
| 10275445 |   45 | `	sxu32 nSlot = pOut->nIdx;` |
| 10275445 |   46 | `	switch( pLit->iType ){` |
|  1297401 |   47 | `		case PH7_NATIVE_VAL_INT:` |
|  2594707 |   48 | `			PH7_MemObjInitFromInt(&(*pVm),pOut,pLit->iValue);` |
|  2594707 |   49 | `			break;` |
|     2647 |   50 | `		case PH7_NATIVE_VAL_BOOL:` |
|     5296 |   51 | `			PH7_MemObjInitFromBool(&(*pVm),pOut,(sxi32)pLit->iValue);` |
|     5296 |   52 | `			break;` |
|  2219794 |   53 | `		case PH7_NATIVE_VAL_STRING: {` |
|        - |   54 | `			SyString sLit;` |
|  4439394 |   55 | `			SyStringInitFromBuf(&sLit,pLit->zValue,SyStrlen(pLit->zValue));` |
|  4439394 |   56 | `			PH7_MemObjInitFromString(&(*pVm),pOut,&sLit);` |
|  4439394 |   57 | `			break;` |
|        - |   58 | `		}` |
|        - |   59 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      306 |   60 | `		case PH7_NATIVE_VAL_DOUBLE:` |
|      616 |   61 | `			PH7_MemObjInitFromReal(&(*pVm),pOut,pLit->rValue);` |
|      616 |   62 | `			break;` |
|        - |   63 | `#endif` |
|   734976 |   64 | `		case PH7_NATIVE_VAL_ARRAY: {` |
|        - |   65 | ``			/* The empty array — php's `private array $trace = [];`. Each instance`` |
|        - |   66 | `			 * needs its OWN map (the exception's trace is written per throw), so` |
|        - |   67 | `			 * this allocates rather than sharing one. */` |
|  1469902 |   68 | `			ph7_hashmap *pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|  1469902 |   69 | `			if( pMap == 0 ){` |
|      ! 0 |   70 | `				PH7_MemObjInit(&(*pVm),pOut);` |
|      ! 0 |   71 | `			}else{` |
|  1469902 |   72 | `				PH7_MemObjInitFromArray(&(*pVm),pOut,pMap);` |
|        - |   73 | `			}` |
|  1469902 |   74 | `			break;` |
|        - |   75 | `		}` |
|   883300 |   76 | `		default:` |
|  1765554 |   77 | `			PH7_MemObjInit(&(*pVm),pOut);` |
|  1765549 |   78 | `			break;` |
|        - |   79 | `	}` |
| 10275445 |   80 | `	pOut->nIdx = nSlot;` |
| 10275445 |   81 | `}` |
|        - |   82 | `/*` |
|        - |   83 | ` * Write a declared property of an instance from C.` |
|        - |   84 | ` *` |
|        - |   85 | ` * Every native class that hands an OBJECT back to PHP has to fill one in, and the` |
|        - |   86 | ` * write has to go through the instance's own slot table (hAttr -> VmClassAttr ->` |
|        - |   87 | ` * pVm->aMemObj) rather than the class declaration, or the value lands nowhere.` |
|        - |   88 | ` * Silently does nothing for a name the class does not declare -- callers pass` |
|        - |   89 | ` * literals from their own spec table, so a miss is a build error, not input.` |
|        - |   90 | ` */` |
|    43150 |   91 | `PH7_PRIVATE void PH7_NativeSetProp(ph7_vm *pVm,ph7_class_instance *pObj,` |
|        - |   92 | `	const char *zProp,sxu32 nProp,ph7_value *pSrcVal)` |
|        5 |   93 | `{` |
|        - |   94 | `	SyHashEntry *pEntry;` |
|        - |   95 | `	VmClassAttr *pVmAttr;` |
|        - |   96 | `	ph7_value *pSlot;` |
|    43155 |   97 | `	PH7_NativeMaterializeLazy(&(*pVm),pObj);` |
|    43155 |   98 | `	pEntry = SyHashGet(&pObj->hAttr,(const void *)zProp,nProp);` |
|    43155 |   99 | `	if( pEntry == 0 ){` |
|      ! 0 |  100 | `		return;` |
|        - |  101 | `	}` |
|    43155 |  102 | `	pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    43155 |  103 | `	pSlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pVmAttr->nIdx);` |
|    43155 |  104 | `	if( pSlot == 0 ){` |
|      ! 0 |  105 | `		return;` |
|        - |  106 | `	}` |
|    43155 |  107 | `	PH7_MemObjStore(pSrcVal,pSlot);` |
|    43155 |  108 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|    21574 |  109 | `}` |
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
|  2520779 |  122 | `PH7_PRIVATE ph7_value * PH7_NativeAttr(ph7_class_instance *pObj,const char *zName)` |
|        5 |  123 | `{` |
|        - |  124 | `	SyString sName;` |
|  2520784 |  125 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|  2520784 |  126 | `	return PH7_ClassInstanceFetchAttr(pObj,&sName);` |
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
|   227224 |  143 | `PH7_PRIVATE sxi64 PH7_NativeAttrInt(ph7_class_instance *pObj,const char *zName)` |
|        5 |  144 | `{` |
|   227229 |  145 | `	ph7_value *pVal = PH7_NativeAttr(pObj,zName);` |
|   227229 |  146 | `	if( pVal && (pVal->iFlags & MEMOBJ_INT) ){` |
|   227103 |  147 | `		return pVal->x.iVal;` |
|        - |  148 | `	}` |
|      129 |  149 | `	return 0;` |
|   113620 |  150 | `}` |
|        - |  151 | `/* Borrow a string slot's bytes (empty when it holds anything else). */` |
|    73308 |  152 | `PH7_PRIVATE void PH7_NativeAttrStr(ph7_class_instance *pObj,const char *zName,` |
|        - |  153 | `	const char **pzOut,int *pnOut)` |
|        5 |  154 | `{` |
|    73313 |  155 | `	ph7_value *pVal = PH7_NativeAttr(pObj,zName);` |
|    73313 |  156 | `	*pzOut = "";` |
|    73313 |  157 | `	*pnOut = 0;` |
|    73313 |  158 | `	if( pVal && (pVal->iFlags & MEMOBJ_STRING) ){` |
|    73025 |  159 | `		*pzOut = (const char *)SyBlobData(&pVal->sBlob);` |
|    73025 |  160 | `		*pnOut = (int)SyBlobLength(&pVal->sBlob);` |
|    36492 |  161 | `	}` |
|    73313 |  162 | `}` |
|        - |  163 | `/* The object stored in a slot, or NULL when it holds anything else. */` |
|   134620 |  164 | `PH7_PRIVATE ph7_class_instance * PH7_NativeAttrObj(ph7_class_instance *pObj,const char *zName)` |
|        5 |  165 | `{` |
|   134625 |  166 | `	ph7_value *pVal = PH7_NativeAttr(pObj,zName);` |
|   134625 |  167 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     9725 |  168 | `		return 0;` |
|        - |  169 | `	}` |
|   124905 |  170 | `	return (ph7_class_instance *)pVal->x.pOther;` |
|    67312 |  171 | `}` |
|        - |  172 | `/* Truth of a bool/int slot, again without converting it. */` |
|     6676 |  173 | `PH7_PRIVATE int PH7_NativeAttrTruthy(ph7_class_instance *pObj,const char *zName)` |
|        5 |  174 | `{` |
|     6681 |  175 | `	ph7_value *pVal = PH7_NativeAttr(pObj,zName);` |
|     6681 |  176 | `	if( pVal && (pVal->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|     6681 |  177 | `		return pVal->x.iVal != 0;` |
|        - |  178 | `	}` |
|      ! 0 |  179 | `	return 0;` |
|     3343 |  180 | `}` |
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
|  1690910 |  193 | `static void NativeAttrMarkInit(ph7_class_instance *pObj,const char *zName)` |
|        5 |  194 | `{` |
|  1690915 |  195 | `	SyHashEntry *pEntry = SyHashGet(&pObj->hAttr,(const void *)zName,SyStrlen(zName));` |
|  1690915 |  196 | `	if( pEntry ){` |
|  1690915 |  197 | `		((VmClassAttr *)pEntry->pUserData)->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|   845421 |  198 | `	}` |
|  1690915 |  199 | `}` |
|        - |  200 | `/*` |
|        - |  201 | ` * The slot fetch on the WRITE side. A class whose php-visible properties are LAZY` |
|        - |  202 | ` * has none of them on the object until a C body fills one, and that first write is` |
|        - |  203 | ` * what installs the set -- which is php's constructor writing its struct into the` |
|        - |  204 | ` * property table. Every native writer goes through here so the bookkeeping lives` |
|        - |  205 | ` * with the write rather than with one of the ways of writing.` |
|        - |  206 | ` */` |
|  1690910 |  207 | `static ph7_value * NativeAttrForWrite(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName)` |
|        5 |  208 | `{` |
|        - |  209 | `	ph7_value *pSlot;` |
|  1690915 |  210 | `	PH7_NativeMaterializeLazy(&(*pVm),pObj);` |
|  1690915 |  211 | `	pSlot = PH7_NativeAttr(pObj,zName);` |
|  1690915 |  212 | `	if( pSlot == 0 && pObj ){` |
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
|  1690915 |  229 | `	return pSlot;` |
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
|    81446 |  247 | `PH7_PRIVATE void PH7_NativeSetAttrInt(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,sxi64 iVal)` |
|        5 |  248 | `{` |
|    81451 |  249 | `	ph7_value *pSlot = NativeAttrForWrite(&(*pVm),pObj,zName);` |
|        - |  250 | `	ph7_value sVal;` |
|    81451 |  251 | `	if( pSlot == 0 ){` |
|      ! 0 |  252 | `		return;` |
|        - |  253 | `	}` |
|    81451 |  254 | `	PH7_MemObjInitFromInt(&(*pVm),&sVal,iVal);` |
|    81451 |  255 | `	PH7_MemObjStore(&sVal,pSlot);` |
|    81451 |  256 | `	PH7_MemObjRelease(&sVal);` |
|    81451 |  257 | `	NativeAttrMarkInit(pObj,zName);` |
|    40729 |  258 | `}` |
|        - |  259 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      614 |  260 | `PH7_PRIVATE void PH7_NativeSetAttrReal(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,ph7_real rVal)` |
|        4 |  261 | `{` |
|      618 |  262 | `	ph7_value *pSlot = NativeAttrForWrite(&(*pVm),pObj,zName);` |
|        - |  263 | `	ph7_value sVal;` |
|      618 |  264 | `	if( pSlot == 0 ){` |
|      ! 0 |  265 | `		return;` |
|        - |  266 | `	}` |
|      618 |  267 | `	PH7_MemObjInitFromReal(&(*pVm),&sVal,rVal);` |
|      618 |  268 | `	PH7_MemObjStore(&sVal,pSlot);` |
|      618 |  269 | `	PH7_MemObjRelease(&sVal);` |
|      618 |  270 | `	NativeAttrMarkInit(pObj,zName);` |
|      311 |  271 | `}` |
|        - |  272 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|  1543642 |  273 | `PH7_PRIVATE void PH7_NativeSetAttrStr(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,` |
|        - |  274 | `	const char *zVal,int nVal)` |
|        5 |  275 | `{` |
|  1543647 |  276 | `	ph7_value *pSlot = NativeAttrForWrite(&(*pVm),pObj,zName);` |
|        - |  277 | `	ph7_value sVal;` |
|        - |  278 | `	SyString sStr;` |
|  1543647 |  279 | `	if( pSlot == 0 ){` |
|      ! 0 |  280 | `		return;` |
|        - |  281 | `	}` |
|  1543647 |  282 | `	SyStringInitFromBuf(&sStr,zVal,nVal);` |
|  1543647 |  283 | `	PH7_MemObjInitFromString(&(*pVm),&sVal,&sStr);` |
|  1543647 |  284 | `	PH7_MemObjStore(&sVal,pSlot);` |
|  1543647 |  285 | `	PH7_MemObjRelease(&sVal);	NativeAttrMarkInit(pObj,zName);` |
|   771793 |  286 | `}` |
|     7161 |  287 | `PH7_PRIVATE void PH7_NativeSetAttrBool(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,int bVal)` |
|        5 |  288 | `{` |
|     7166 |  289 | `	ph7_value *pSlot = NativeAttrForWrite(&(*pVm),pObj,zName);` |
|        - |  290 | `	ph7_value sVal;` |
|     7166 |  291 | `	if( pSlot == 0 ){` |
|      ! 0 |  292 | `		return;` |
|        - |  293 | `	}` |
|     7166 |  294 | `	PH7_MemObjInitFromBool(&(*pVm),&sVal,bVal);` |
|     7166 |  295 | `	PH7_MemObjStore(&sVal,pSlot);` |
|     7166 |  296 | `	PH7_MemObjRelease(&sVal);	NativeAttrMarkInit(pObj,zName);` |
|     3584 |  297 | `}` |
|        - |  298 | `/* Store an object in a slot, or NULL to clear it. PH7_MemObjStore takes the` |
|        - |  299 | ` * reference the slot needs, so the temp never holds one of its own. */` |
|    58047 |  300 | `PH7_PRIVATE void PH7_NativeSetAttrObj(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,` |
|        - |  301 | `	ph7_class_instance *pVal)` |
|        5 |  302 | `{` |
|    58052 |  303 | `	ph7_value *pSlot = NativeAttrForWrite(&(*pVm),pObj,zName);` |
|        - |  304 | `	ph7_value sVal;` |
|    58052 |  305 | `	if( pSlot == 0 ){` |
|      ! 0 |  306 | `		return;` |
|        - |  307 | `	}` |
|    58052 |  308 | `	PH7_MemObjInit(&(*pVm),&sVal);` |
|    58052 |  309 | `	if( pVal ){` |
|    57380 |  310 | `		sVal.x.pOther = pVal;` |
|    57380 |  311 | `		sVal.iFlags = MEMOBJ_OBJ;` |
|    28687 |  312 | `	}` |
|    58052 |  313 | `	PH7_MemObjStore(&sVal,pSlot);` |
|    58052 |  314 | `	NativeAttrMarkInit(pObj,zName);` |
|    29028 |  315 | `}` |
|        - |  316 | `/*` |
|        - |  317 | ` * Hand an instance back as a native call's result, dropping the reference` |
|        - |  318 | ` * PH7_NewClassInstance/PH7_CloneClassInstance handed the caller.` |
|        - |  319 | ` */` |
|    13182 |  320 | `PH7_PRIVATE void PH7_NativeResultObject(ph7_context *pCtx,ph7_class_instance *pObj)` |
|        5 |  321 | `{` |
|        - |  322 | `	ph7_value sRes;` |
|    13187 |  323 | `	PH7_MemObjInit(pCtx->pVm,&sRes);` |
|    13187 |  324 | `	sRes.x.pOther = pObj;` |
|    13187 |  325 | `	sRes.iFlags = MEMOBJ_OBJ;` |
|    13187 |  326 | `	ph7_result_value(pCtx,&sRes);   /* takes its own reference */` |
|    13187 |  327 | `	PH7_ClassInstanceUnref(pObj);` |
|    13187 |  328 | `}` |
|        - |  329 | `/*` |
|        - |  330 | ` * Resolve a class by name for the builder's own use (parents and interfaces).` |
|        - |  331 | ` * Autoload is deliberately NOT triggered: these run at VM init, where the only` |
|        - |  332 | ` * classes that can exist are the ones installed before this call, and a missing` |
|        - |  333 | ` * name is a build-order bug in the engine, not a userland lookup.` |
|        - |  334 | ` */` |
|  1950795 |  335 | `static ph7_class * NativeLookupClass(ph7_vm *pVm,const char *zName)` |
|        5 |  336 | `{` |
|  1950800 |  337 | `	return PH7_VmExtractClass(&(*pVm),zName,(sxu32)SyStrlen(zName),FALSE,0);` |
|        5 |  338 | `}` |
|        - |  339 | `/*` |
|        - |  340 | ` * Translate the builder's PH7_MOD_* modifier bits into the protection level and` |
|        - |  341 | ` * the attribute/method flag word the class structures actually store.` |
|        - |  342 | ` */` |
| 21045226 |  343 | `static sxi32 NativeProtection(sxi32 iMods)` |
|        5 |  344 | `{` |
| 21045231 |  345 | `	if( iMods & PH7_MOD_PRIVATE ){` |
|  1384985 |  346 | `		return PH7_CLASS_PROT_PRIVATE;` |
|        - |  347 | `	}` |
| 19660251 |  348 | `	if( iMods & PH7_MOD_PROTECTED ){` |
|   270245 |  349 | `		return PH7_CLASS_PROT_PROTECTED;` |
|        - |  350 | `	}` |
| 19390011 |  351 | `	return PH7_CLASS_PROT_PUBLIC;` |
| 10508912 |  352 | `}` |
|        - |  353 | `/*` |
|        - |  354 | ` * Attach one C-bodied method to an already-created class.` |
|        - |  355 | ` *` |
|        - |  356 | ` * The ph7_user_func built here is NOT registered in pVm->hHostFunction: it hangs` |
|        - |  357 | ` * off the method and is reachable only by dispatching the method, which is exactly` |
|        - |  358 | ` * the property that retires the global thunks. Its sName is the php-facing` |
|        - |  359 | `` * `Class::method`, so an ArgumentCountError raised at the OP_CALL choke point`` |
|        - |  360 | ` * names what a php user would recognise.` |
|        - |  361 | ` */` |
| 13360276 |  362 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallMethod(` |
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
| 13360281 |  376 | `	SyStringInitFromBuf(&sName,pDef->zName,SyStrlen(pDef->zName));` |
| 13360281 |  377 | `	iFuncFlags = VM_FUNC_NATIVE;` |
| 13360281 |  378 | `	if( pVm->bCompilingBuiltin ){` |
|        - |  379 | `		/* Same stamp the embedded chunks got, so Reflection keeps reporting these` |
|        - |  380 | `		 * as internal: isInternal() true, getFileName() false. */` |
| 13359995 |  381 | `		iFuncFlags \|= VM_FUNC_INTERNAL;` |
|  6671294 |  382 | `	}` |
| 20031718 |  383 | `	pMeth = PH7_NewClassMethod(&(*pVm),pClass,&sName,0,` |
| 13360276 |  384 | `		NativeProtection(pDef->iMods),` |
| 13360276 |  385 | `		((pDef->iMods & PH7_MOD_FINAL) ? PH7_CLASS_ATTR_FINAL : 0)` |
| 13360276 |  386 | `		\| ((pDef->iMods & PH7_MOD_ABSTRACT) ? PH7_CLASS_ATTR_ABSTRACT : 0)` |
| 13360276 |  387 | `		\| ((pDef->iMods & PH7_MOD_FABRICATED) ? PH7_CLASS_ATTR_FABRICATED : 0),` |
|  6671437 |  388 | `		iFuncFlags);` |
| 13360281 |  389 | `	if( pMeth == 0 ){` |
|      ! 0 |  390 | `		return SXERR_MEM;` |
|        - |  391 | `	}` |
|        - |  392 | `	/* Staticness has to be recorded in BOTH places: on the method (where the class` |
|        - |  393 | `	 * machinery and Reflection read it) and on the function (where the OP_CALL` |
|        - |  394 | `	 * dispatcher reads it, to keep the caller's $this from leaking in as a receiver` |
|        - |  395 | `	 * — see VM_FUNC_NATIVE_STATIC). */` |
| 13360281 |  396 | `	if( pDef->iMods & PH7_MOD_STATIC ){` |
|   709671 |  397 | `		pMeth->iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|   709671 |  398 | `		pMeth->sFunc.iFlags \|= VM_FUNC_NATIVE_STATIC;` |
|   354371 |  399 | `	}` |
|        - |  400 | `	/* An ABSTRACT method has no body to install — an INTERFACE's methods are the` |
|        - |  401 | ``	 * reason the builder needs the case at all (`SeekableIterator::seek()`), and`` |
|        - |  402 | `	 * php reports them with their declared signature like any other. The dispatch` |
|        - |  403 | `	 * never reaches one: OP_CALL and OP_MEMBER both refuse an abstract method` |
|        - |  404 | `	 * ("Cannot call abstract method C::m()") before they look at a body. */` |
| 13360281 |  405 | `	if( pDef->iMods & PH7_MOD_ABSTRACT ){` |
|   591155 |  406 | `		if( pDef->zSig ){` |
|   591155 |  407 | `			rc = PH7_NewForeignFunction(&(*pVm),&sName,pDef->xFunc,pUserData,&pNative);` |
|   591155 |  408 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  409 | `				return rc;` |
|        - |  410 | `			}` |
|   591155 |  411 | `			pNative->zSig = pDef->zSig;` |
|   591155 |  412 | `			if( pDef->zRet && pDef->zRet[0] ){` |
|   565820 |  413 | `				pNative->zRet = pDef->zRet;` |
|   282539 |  414 | `			}` |
|   591155 |  415 | `			pMeth->sFunc.pNative = pNative;` |
|   295190 |  416 | `		}` |
|   591155 |  417 | `		return PH7_ClassInstallMethod(pClass,pMeth);` |
|        - |  418 | `	}` |
|        - |  419 | `	/* The C body. The diagnostic name is the qualified one php would print. */` |
| 12769131 |  420 | `	SyBufferFormat(zQual,sizeof(zQual),"%z::%s",&pClass->sName,pDef->zName);` |
| 12769131 |  421 | `	SyStringInitFromBuf(&sVmName,zQual,SyStrlen(zQual));` |
| 12769131 |  422 | `	rc = PH7_NewForeignFunction(&(*pVm),&sVmName,pDef->xFunc,pUserData,&pNative);` |
| 12769131 |  423 | `	if( rc != SXRET_OK ){` |
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
| 12769131 |  439 | `	if( pDef->zSig ){` |
| 12752241 |  440 | `		sxi16 nMin = 0, nMax = 0;` |
| 12752241 |  441 | `		sxu8 bAtLeast = 0, bHasMax = 0;` |
| 12752241 |  442 | `		pNative->zSig = pDef->zSig;` |
| 12752241 |  443 | `		pNative->nByRefMask = VmDeriveByRefMaskFromSig(pDef->zSig);` |
| 12752241 |  444 | `		VmDeriveArityFromSig(pDef->zSig,&nMin,&bAtLeast,&nMax,&bHasMax);` |
| 12752241 |  445 | `		pNative->nMinArg = nMin;` |
| 12752241 |  446 | `		pNative->bAtLeast = bAtLeast;` |
| 12752241 |  447 | `		pNative->nMaxArg = nMax;` |
| 12752241 |  448 | `		pNative->bHasMaxArg = bHasMax;` |
|  6367813 |  449 | `	}` |
| 12769131 |  450 | `	if( pDef->zRet && pDef->zRet[0] ){` |
| 11637501 |  451 | `		pNative->zRet = pDef->zRet;` |
|  5811169 |  452 | `	}` |
| 12769131 |  453 | `	pMeth->sFunc.pNative = pNative;` |
| 12769131 |  454 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
| 12769131 |  455 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  456 | `		return rc;` |
|        - |  457 | `	}` |
| 12769131 |  458 | `	if( pClass->bMounted ){` |
|        - |  459 | `		/* The class is already live (a method attached after installation): mount` |
|        - |  460 | `		 * this one method now, since VmMountUserClass will not run again. */` |
|      ! 0 |  461 | `		return PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|        - |  462 | `	}` |
| 12769131 |  463 | `	return SXRET_OK;` |
|  6671442 |  464 | `}` |
|        - |  465 | `/*` |
|        - |  466 | ` * Install one class constant carrying a scalar value.` |
|        - |  467 | ` *` |
|        - |  468 | ` * A declared default is normally a compiled initializer (ph7_class_attr::aByteCode)` |
|        - |  469 | ` * evaluated at mount. The builder has no compiler to hand, so it evaluates the` |
|        - |  470 | ` * value NOW into the constant's reserved slot and leaves the byte-code empty --` |
|        - |  471 | ` * which is what a literal initializer would have produced anyway.` |
|        - |  472 | ` */` |
|  3403335 |  473 | `static sxi32 NativeInstallConstant(ph7_vm *pVm,ph7_class *pClass,const PH7_NativeConstDef *pDef)` |
|        5 |  474 | `{` |
|        - |  475 | `	ph7_class_attr *pAttr;` |
|        - |  476 | `	SyString sName;` |
|  3403340 |  477 | `	SyStringInitFromBuf(&sName,pDef->zName,SyStrlen(pDef->zName));` |
|  3403340 |  478 | `	pAttr = PH7_NewClassAttr(&(*pVm),&sName,0,NativeProtection(pDef->iMods),` |
|        - |  479 | `		PH7_CLASS_ATTR_CONSTANT);` |
|  3403340 |  480 | `	if( pAttr == 0 ){` |
|      ! 0 |  481 | `		return SXERR_MEM;` |
|        - |  482 | `	}` |
|  3403340 |  483 | `	pAttr->pDeclClass = pClass;` |
|        - |  484 | `	/* Stash the literal; VmMountUserClassAttrs materializes it into the slot. */` |
|  3403340 |  485 | `	pAttr->pNativeValue = pDef;` |
|  3403340 |  486 | `	return PH7_ClassInstallAttr(pClass,pAttr);` |
|  1699456 |  487 | `}` |
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
|  2136585 |  500 | `static void NativeAttrType(ph7_class_attr *pAttr,const char *zType)` |
|        5 |  501 | `{` |
|        - |  502 | `	static const struct { const char *zName; sxu32 nType; } aScalar[] = {` |
|        - |  503 | `		{ "int",    MEMOBJ_INT },` |
|        - |  504 | `		{ "float",  MEMOBJ_REAL },` |
|        - |  505 | `		{ "string", MEMOBJ_STRING },` |
|        - |  506 | `		{ "bool",   MEMOBJ_BOOL },` |
|        - |  507 | `		{ "array",  MEMOBJ_HASHMAP },` |
|        - |  508 | `		{ "object", MEMOBJ_OBJ },` |
|        - |  509 | `	};` |
|  2136590 |  510 | `	const char *zAtom = zType;` |
|        - |  511 | `	sxu32 nAtom, n;` |
|  2136590 |  512 | `	if( zAtom[0] == '?' ){` |
|   751610 |  513 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|   751610 |  514 | `		zAtom++;` |
|   375313 |  515 | `	}` |
|  2136590 |  516 | `	nAtom = SyStrlen(zAtom);` |
|  2136590 |  517 | `	pAttr->iFlags \|= PH7_CLASS_ATTR_TYPED;` |
|  2136590 |  518 | `	SyStringInitFromBuf(&pAttr->sTypeName,zType,SyStrlen(zType));` |
|  8554790 |  519 | `	for( n = 0 ; n < SX_ARRAYSIZE(aScalar) ; ++n ){` |
|  7921410 |  520 | `		if( nAtom == SyStrlen(aScalar[n].zName)` |
|  4775783 |  521 | `		 && SyMemcmp(zAtom,aScalar[n].zName,(sxu32)nAtom) == 0 ){` |
|  1503215 |  522 | `			pAttr->nType = aScalar[n].nType;` |
|  1503215 |  523 | `			return;` |
|        - |  524 | `		}` |
|  3204925 |  525 | `	}` |
|   633380 |  526 | `	pAttr->nType = SXU32_HIGH;` |
|   633380 |  527 | `	SyStringInitFromBuf(&pAttr->sClass,zAtom,nAtom);` |
|  1066906 |  528 | `}` |
|        - |  529 | `/*` |
|        - |  530 | ` * Install one declared property.` |
|        - |  531 | ` *` |
|        - |  532 | ` * The default is carried as a literal rather than compiled byte-code (see` |
|        - |  533 | `` * PH7_NativeLiteralValue); an instance property materializes it at `new`, a static`` |
|        - |  534 | `` * one at mount. A PH7_NATIVE_VAL_NULL default is the plain `public $p;` case.`` |
|        - |  535 | ` */` |
|  4281615 |  536 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallProperty(ph7_vm *pVm,ph7_class *pClass,` |
|        - |  537 | `	const PH7_NativePropDef *pDef)` |
|        5 |  538 | `{` |
|        - |  539 | `	ph7_class_attr *pAttr;` |
|        - |  540 | `	SyString sName;` |
|  4281620 |  541 | `	sxi32 iFlags = 0;` |
|  4281620 |  542 | `	SyStringInitFromBuf(&sName,pDef->zName,SyStrlen(pDef->zName));` |
|  4281620 |  543 | `	if( pDef->iMods & PH7_MOD_STATIC ){` |
|      ! 0 |  544 | `		iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|      ! 0 |  545 | `	}` |
|  4281620 |  546 | `	if( pDef->iMods & PH7_MOD_HIDDEN ){` |
|  1984580 |  547 | `		iFlags \|= PH7_CLASS_ATTR_HIDDEN;` |
|   990995 |  548 | `	}` |
|  4281620 |  549 | `	if( pDef->iMods & PH7_MOD_ONDEMAND ){` |
|     8450 |  550 | `		iFlags \|= PH7_CLASS_ATTR_NATIVE_ONDEMAND;` |
|     4217 |  551 | `	}` |
|        - |  552 | ``	/* php's lazily-filled REAL slot: the object carries the name from `new`, in its`` |
|        - |  553 | `	 * declared position and uninitialized, and the class's handler fills it on the` |
|        - |  554 | `	 * first read. NOWRITE rides with it because the handler owns the write too --` |
|        - |  555 | `	 * these are the names php refuses with the readonly WORDING while Reflection` |
|        - |  556 | `	 * still reports them writable. */` |
|  4281620 |  557 | `	if( pDef->iMods & PH7_MOD_LAZYSLOT ){` |
|    42230 |  558 | `		iFlags \|= PH7_CLASS_ATTR_NATIVE_LAZYSLOT\|PH7_CLASS_ATTR_NATIVE_NOWRITE;` |
|    21085 |  559 | `	}` |
|        - |  560 | `	/* php's VIRTUAL property: declared, and answered by the class's own handlers` |
|        - |  561 | `	 * rather than by a slot. NATIVE_VIRTUAL rides with it because that is exactly` |
|        - |  562 | `	 * what the name means to Reflection (modifiers 512) and to the object` |
|        - |  563 | `	 * comparator -- there is no real property behind it to compare. */` |
|  4281620 |  564 | `	if( pDef->iMods & PH7_MOD_VIRTUAL ){` |
|  1460990 |  565 | `		iFlags \|= PH7_CLASS_ATTR_NATIVE_NOSLOT\|PH7_CLASS_ATTR_NATIVE_VIRTUAL;` |
|   729541 |  566 | `	}` |
|        - |  567 | `	/* php declares several native slots readonly and asymmetrically visible` |
|        - |  568 | ``	 * (`public protected(set) readonly string $path` on Directory), and both are`` |
|        - |  569 | `	 * php-visible twice over: the write refusal and Reflection's modifier list. */` |
|  4281620 |  570 | `	if( pDef->iMods & PH7_MOD_READONLY ){` |
|   109790 |  571 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|    54821 |  572 | `	}` |
|  4281620 |  573 | `	if( pDef->iMods & PH7_MOD_PROT_SET ){` |
|   101345 |  574 | `		iFlags \|= PH7_CLASS_ATTR_PROTECTED_SET;` |
|    50604 |  575 | `	}` |
|  4281620 |  576 | `	if( pDef->iMods & PH7_MOD_PRIV_SET ){` |
|      ! 0 |  577 | `		iFlags \|= PH7_CLASS_ATTR_PRIVATE_SET;` |
|      ! 0 |  578 | `	}` |
|  4281620 |  579 | `	pAttr = PH7_NewClassAttr(&(*pVm),&sName,0,NativeProtection(pDef->iMods),iFlags);` |
|  4281620 |  580 | `	if( pAttr == 0 ){` |
|      ! 0 |  581 | `		return SXERR_MEM;` |
|        - |  582 | `	}` |
|  4281620 |  583 | `	pAttr->pDeclClass = pClass;` |
|        - |  584 | `	/* NO default is not the same as a NULL one, and the difference is php-visible:` |
|        - |  585 | ``	 * a typed slot without a default is UNINITIALIZED at `new` (reading it before`` |
|        - |  586 | `	 * the class writes it is an Error, hasDefaultValue() is false), which is what` |
|        - |  587 | `	 * php declares for LibXMLError's six fields and every reflector's $name. The` |
|        - |  588 | ``	 * machinery is already there for a compiled `public string $p;` — leaving`` |
|        - |  589 | `	 * pNativeValue at 0 is what selects it. */` |
|  4281620 |  590 | `	if( pDef->sDefault.iType != PH7_NATIVE_VAL_NONE ){` |
|  2347715 |  591 | `		pAttr->pNativeValue = &pDef->sDefault;` |
|  1172326 |  592 | `	}` |
|  4281620 |  593 | `	if( pDef->zType && pDef->zType[0] ){` |
|  2136590 |  594 | `		NativeAttrType(pAttr,pDef->zType);` |
|  1066901 |  595 | `	}` |
|  4281620 |  596 | `	return PH7_ClassInstallAttr(pClass,pAttr);` |
|  2138024 |  597 | `}` |
|        - |  598 | `/*` |
|        - |  599 | `` * Attach an `#[Attr(...)]` to something declared from C.`` |
|        - |  600 | ` *` |
|        - |  601 | `` * php declares `#[Attribute(Attribute::TARGET_CLASS)]` on Attribute itself, a`` |
|        - |  602 | `` * target mask on every other attribute class, and `#[NoDiscard(message: …)]` on`` |
|        - |  603 | ` * nine DateTimeImmutable methods — and those records are LOAD-BEARING: the` |
|        - |  604 | `` * compiler reads them to decide whether a user's `#[Deprecated]` may sit where`` |
|        - |  605 | ` * it does, the NoDiscard warning reads its message from them, and` |
|        - |  606 | ` * ReflectionAttribute answers them all. A compiled attribute holds its argument` |
|        - |  607 | ` * as byte-code; there is no compiler here, so the argument rides as the same` |
|        - |  608 | ` * literal record a native constant or property default uses and every reader` |
|        - |  609 | ` * takes that branch when the byte-code is empty.` |
|        - |  610 | ` *` |
|        - |  611 | ` * NativeBuildAttr is the shared half; the two entry points below hang the record` |
|        - |  612 | ` * on a class or on one of its methods. aArg is BORROWED, so callers state their` |
|        - |  613 | `` * rows `static const`.`` |
|        - |  614 | ` */` |
|   143565 |  615 | `static sxi32 NativeBuildAttr(ph7_vm *pVm,ph7_attribute *pAttr,const char *zAttr,` |
|        - |  616 | `	const PH7_NativeAttrArg *aArg,sxu32 nArg)` |
|        5 |  617 | `{` |
|        - |  618 | `	char *zDup;` |
|        - |  619 | `	sxu32 n;` |
|   143570 |  620 | `	SyZero(pAttr,sizeof(*pAttr));` |
|   143570 |  621 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zAttr,SyStrlen(zAttr));` |
|   143570 |  622 | `	if( zDup == 0 ){` |
|      ! 0 |  623 | `		return SXERR_MEM;` |
|        - |  624 | `	}` |
|   143570 |  625 | `	SyStringInitFromBuf(&pAttr->sName,zDup,SyStrlen(zAttr));` |
|   143570 |  626 | `	SySetInit(&pAttr->aArgs,&pVm->sAllocator,sizeof(ph7_attr_arg));` |
|   287135 |  627 | `	for( n = 0 ; n < nArg ; n++ ){` |
|        - |  628 | `		ph7_attr_arg sArgRec;` |
|   143570 |  629 | `		SyZero(&sArgRec,sizeof(sArgRec));` |
|   143570 |  630 | `		SySetInit(&sArgRec.aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|   143570 |  631 | `		if( aArg[n].zName ){` |
|    76010 |  632 | `			char *zN = SyMemBackendStrDup(&pVm->sAllocator,aArg[n].zName,SyStrlen(aArg[n].zName));` |
|    76010 |  633 | `			if( zN ){` |
|    76010 |  634 | `				SyStringInitFromBuf(&sArgRec.sName,zN,SyStrlen(aArg[n].zName));` |
|    37953 |  635 | `			}` |
|    37953 |  636 | `		}` |
|        - |  637 | `		/* The literal is BORROWED, not copied: aArg must have static storage` |
|        - |  638 | ``		 * duration (every caller states its rows as `static const`). */`` |
|   143570 |  639 | `		sArgRec.pNativeValue = (const void *)&aArg[n].sValue;` |
|   143570 |  640 | `		SySetPut(&pAttr->aArgs,(const void *)&sArgRec);` |
|    71694 |  641 | `	}` |
|   143570 |  642 | `	return SXRET_OK;` |
|    71694 |  643 | `}` |
|        - |  644 | `/*` |
|        - |  645 | `` * Declare one of a native class's METHODS php 8.5's `#[\NoDiscard]`, argument`` |
|        - |  646 | ` * and all — the same record a compiled declaration carries, so Reflection` |
|        - |  647 | ` * reports the attribute and the warning reads its message from the one place a` |
|        - |  648 | ` * userland one is read from. Assigned by the owning installer after` |
|        - |  649 | ` * PH7_InstallNativeClasses, like xClone/xDim/xSet: a spec-row field would have` |
|        - |  650 | ` * to be left empty by every other table.` |
|        - |  651 | ` */` |
|    76005 |  652 | `PH7_PRIVATE sxi32 PH7_NativeMethodSetNoDiscard(ph7_vm *pVm,ph7_class *pClass,` |
|        - |  653 | `	const char *zMethod,const PH7_NativeAttrArg *aArg,sxu32 nArg)` |
|        5 |  654 | `{` |
|        - |  655 | `	ph7_class_method *pMeth;` |
|        - |  656 | `	ph7_attribute sAttr;` |
|        - |  657 | `	sxi32 rc;` |
|    76010 |  658 | `	if( pClass == 0 ){` |
|      ! 0 |  659 | `		return SXERR_NOTFOUND;` |
|        - |  660 | `	}` |
|    76010 |  661 | `	pMeth = PH7_ClassExtractMethod(pClass,zMethod,(sxu32)SyStrlen(zMethod));` |
|    76010 |  662 | `	if( pMeth == 0 ){` |
|      ! 0 |  663 | `		return SXERR_NOTFOUND;` |
|        - |  664 | `	}` |
|    76010 |  665 | `	rc = NativeBuildAttr(&(*pVm),&sAttr,"NoDiscard",aArg,nArg);` |
|    76010 |  666 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  667 | `		return rc;` |
|        - |  668 | `	}` |
|    76010 |  669 | `	pMeth->sFunc.iFlags \|= VM_FUNC_NODISCARD;` |
|    76010 |  670 | `	return SySetPut(&pMeth->sFunc.aAttrs,(const void *)&sAttr);` |
|    37958 |  671 | `}` |
|    67560 |  672 | `PH7_PRIVATE sxi32 PH7_NativeClassAddAttribute(ph7_vm *pVm,ph7_class *pClass,` |
|        - |  673 | `	const char *zAttr,const PH7_NativeAttrArg *aArg,sxu32 nArg)` |
|        5 |  674 | `{` |
|        - |  675 | `	ph7_attribute sAttr;` |
|        - |  676 | `	sxi32 rc;` |
|    67565 |  677 | `	if( pClass == 0 ){` |
|      ! 0 |  678 | `		return SXERR_NOTFOUND;` |
|        - |  679 | `	}` |
|    67565 |  680 | `	rc = NativeBuildAttr(&(*pVm),&sAttr,zAttr,aArg,nArg);` |
|    67565 |  681 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  682 | `		return rc;` |
|        - |  683 | `	}` |
|    67565 |  684 | `	return SySetPut(&pClass->aAttrs,(const void *)&sAttr);` |
|    33741 |  685 | `}` |
|        - |  686 | `/*` |
|        - |  687 | ` * php's get_properties / get_debug_info handlers: the SHAPE a class SHOWS.` |
|        - |  688 | ` *` |
|        - |  689 | ` * Several native classes present something that is not their storage. php shows a` |
|        - |  690 | ` * DateTime as date/timezone_type/timezone (the state is a timestamp, an offset and` |
|        - |  691 | ` * a zone name), a DateTimeZone as timezone_type/timezone, and a WeakReference as` |
|        - |  692 | ` * ["object"]. Those slots carry PH7_MOD_HIDDEN so no presentation surface sees the` |
|        - |  693 | ` * engine state; this fills an array with what php shows instead.` |
|        - |  694 | ` *` |
|        - |  695 | ` * Answers 1 when the class (or an ancestor) declared a hook and pOut was filled.` |
|        - |  696 | ` * bDebug distinguishes php's two handlers: 1 for var_dump/print_r (get_debug_info),` |
|        - |  697 | ` * 0 for var_export and the (array) cast (get_properties). They disagree — a` |
|        - |  698 | ` * WeakReference shows ["object"] to var_dump and nothing to (array) — so the` |
|        - |  699 | ` * callback is told which is asking rather than each caller guessing.` |
|        - |  700 | ` * get_object_vars() and foreach are NOT callers: php answers those from the real` |
|        - |  701 | ` * properties with the caller's scope applied, which for every class here is empty.` |
|        - |  702 | ` */` |
|     1828 |  703 | `PH7_PRIVATE int PH7_ClassInstancePresent(ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|        5 |  704 | `{` |
|        - |  705 | `	ph7_class *pClass;` |
|     1833 |  706 | `	if( pThis == 0 \|\| pOut == 0 ){` |
|      ! 0 |  707 | `		return 0;` |
|        - |  708 | `	}` |
|     2717 |  709 | `	for( pClass = pThis->pClass ; pClass ; pClass = pClass->pBase ){` |
|     1979 |  710 | `		if( pClass->xPresent ){` |
|     1095 |  711 | `			return pClass->xPresent(pThis->pVm,pThis,pOut,bDebug) == SXRET_OK;` |
|        - |  712 | `		}` |
|      447 |  713 | `	}` |
|      743 |  714 | `	return 0;` |
|      919 |  715 | `}` |
|        - |  716 | `/*` |
|        - |  717 | ` * The nearest ph7_class::xDim in a class's base chain -- php's handler` |
|        - |  718 | ` * inheritance, so a user subclass of DOMNodeList reads dimensions the way its` |
|        - |  719 | ` * parent does.` |
|        - |  720 | ` */` |
|    10360 |  721 | `static ph7_class * NativeDimClass(ph7_class *pClass)` |
|        5 |  722 | `{` |
|    11267 |  723 | `	while( pClass ){` |
|    10583 |  724 | `		if( pClass->xDim ){` |
|     9681 |  725 | `			return pClass;` |
|        - |  726 | `		}` |
|      907 |  727 | `		pClass = pClass->pBase;` |
|        5 |  728 | `	}` |
|      689 |  729 | `	return 0;` |
|     5185 |  730 | `}` |
|        - |  731 | `/*` |
|        - |  732 | `` * Does `$o[$k]` mean anything for an instance of this class? The subscript`` |
|        - |  733 | `` * opcode asks BEFORE it commits to php's `Cannot use object of type C as`` |
|        - |  734 | `` * array`, which is still the answer for every class that has no hook.`` |
|        - |  735 | ` */` |
|     5442 |  736 | `PH7_PRIVATE int PH7_ClassHasNativeDim(ph7_class *pClass)` |
|        5 |  737 | `{` |
|     5447 |  738 | `	return NativeDimClass(pClass) != 0;` |
|        5 |  739 | `}` |
|        - |  740 | `/*` |
|        - |  741 | ` * Run the hook. Answers 0 when the class has none (nothing in pCtx is touched);` |
|        - |  742 | ` * 1 when it answered, which includes a REFUSAL -- the caller reads zThrowClass` |
|        - |  743 | ` * to tell the two apart.` |
|        - |  744 | ` */` |
|     4784 |  745 | `PH7_PRIVATE int PH7_ClassNativeDim(ph7_class_instance *pThis,PH7_NativeDimCtx *pCtx)` |
|        5 |  746 | `{` |
|     4789 |  747 | `	ph7_class *pClass = pThis ? NativeDimClass(pThis->pClass) : 0;` |
|     4789 |  748 | `	if( pClass == 0 ){` |
|      ! 0 |  749 | `		return 0;` |
|        - |  750 | `	}` |
|     4789 |  751 | `	pClass->xDim(pThis->pVm,pThis,pCtx);` |
|     4789 |  752 | `	return 1;` |
|     2397 |  753 | `}` |
|        - |  754 | `/*` |
|        - |  755 | ` * The refusal a dimension WRITE, APPEND or UNSET takes on an object. php's own` |
|        - |  756 | ` * sentence for a class that is not an ArrayAccess is` |
|        - |  757 | `` * `Cannot use object of type C as array`; a class whose read handler answers`` |
|        - |  758 | ` * something words its own (php's PDORow names the operation and the class),` |
|        - |  759 | ` * which the hook supplies through the same refusal fields a read uses.` |
|        - |  760 | ` */` |
|       46 |  761 | `PH7_PRIVATE sxu32 PH7_ClassNativeDimRefusal(ph7_class_instance *pThis,int iMode,` |
|        - |  762 | `	char *zMsg,sxu32 nMsg)` |
|        3 |  763 | `{` |
|       49 |  764 | `	ph7_class *pClass = pThis ? NativeDimClass(pThis->pClass) : 0;` |
|       49 |  765 | `	if( pClass ){` |
|        - |  766 | `		PH7_NativeDimCtx sDim;` |
|       35 |  767 | `		sDim.iMode = iMode;` |
|       35 |  768 | `		sDim.pOffset = 0;` |
|       35 |  769 | `		sDim.pResult = 0;` |
|       35 |  770 | `		sDim.zThrowClass = 0;` |
|       35 |  771 | `		sDim.zThrowMsg[0] = 0;` |
|       35 |  772 | `		sDim.bStored = 0;` |
|       35 |  773 | `		pClass->xDim(pThis->pVm,pThis,&sDim);` |
|       35 |  774 | `		if( sDim.zThrowClass ){` |
|        3 |  775 | `			return SyBufferFormat(zMsg,nMsg,"%s",sDim.zThrowMsg);` |
|        - |  776 | `		}` |
|       16 |  777 | `	}` |
|      113 |  778 | `	return SyBufferFormat(zMsg,nMsg,"Cannot use object of type %.*s as array",` |
|       44 |  779 | `		pThis ? (int)pThis->pClass->sDisp.nByte : 0,` |
|       44 |  780 | `		pThis ? pThis->pClass->sDisp.zString : "");` |
|       26 |  781 | `}` |
|        - |  782 | `/*` |
|        - |  783 | ` * Offer a dimension WRITE, APPEND or UNSET to the class's own handler, with the` |
|        - |  784 | ` * offset and the value the refusal-only form does not carry.` |
|        - |  785 | ` *` |
|        - |  786 | ` * Answers 1 when the handler took the access -- either by STORING (bStored,` |
|        - |  787 | ` * with zThrowClass still 0) or by wording its own refusal in` |
|        - |  788 | ` * zThrowClass/zThrowMsg -- and 0 when the class has no handler or its handler` |
|        - |  789 | `` * declined, which puts the access on the ordinary `Cannot use object of type C`` |
|        - |  790 | `` * as array` path. pCtx is the caller's scratch: it is initialized here and left`` |
|        - |  791 | ` * filled for the caller to read.` |
|        - |  792 | ` */` |
|       88 |  793 | `PH7_PRIVATE int PH7_ClassNativeDimStore(ph7_class_instance *pThis,int iMode,` |
|        - |  794 | `	ph7_value *pOffset,ph7_value *pValue,PH7_NativeDimCtx *pCtx)` |
|        3 |  795 | `{` |
|       91 |  796 | `	ph7_class *pClass = pThis ? NativeDimClass(pThis->pClass) : 0;` |
|       91 |  797 | `	pCtx->iMode = iMode;` |
|       91 |  798 | `	pCtx->pOffset = pOffset;` |
|       91 |  799 | `	pCtx->pResult = pValue;` |
|       91 |  800 | `	pCtx->zThrowClass = 0;` |
|       91 |  801 | `	pCtx->zThrowMsg[0] = 0;` |
|       91 |  802 | `	pCtx->bStored = 0;` |
|       91 |  803 | `	if( pClass == 0 ){` |
|        6 |  804 | `		return 0;` |
|        - |  805 | `	}` |
|       86 |  806 | `	pClass->xDim(pThis->pVm,pThis,pCtx);` |
|       86 |  807 | `	return pCtx->bStored \|\| pCtx->zThrowClass ? 1 : 0;` |
|       47 |  808 | `}` |
|        - |  809 | `/*` |
|        - |  810 | ` * The nearest ph7_class::xProp in a class's base chain -- the same handler` |
|        - |  811 | ` * inheritance xDim and xSet get, and php's own: a subclass of a class whose` |
|        - |  812 | ` * properties are not storage reads them through the parent's handler.` |
|        - |  813 | ` */` |
|    82445 |  814 | `static ph7_class * NativePropClass(ph7_class *pClass)` |
|        5 |  815 | `{` |
|   210837 |  816 | `	while( pClass ){` |
|   208626 |  817 | `		if( pClass->xProp ){` |
|    80239 |  818 | `			return pClass;` |
|        - |  819 | `		}` |
|   128392 |  820 | `		pClass = pClass->pBase;` |
|        5 |  821 | `	}` |
|     2216 |  822 | `	return 0;` |
|    41226 |  823 | `}` |
|        - |  824 | `/*` |
|        - |  825 | `` * Does `$o->p` MEAN something this class answers for itself? Asked before the`` |
|        - |  826 | ` * miss path commits to creating a property, warning about an undefined one or` |
|        - |  827 | ` * dispatching __get.` |
|        - |  828 | ` */` |
|    38039 |  829 | `PH7_PRIVATE int PH7_ClassHasNativeProp(ph7_class *pClass)` |
|        5 |  830 | `{` |
|    38044 |  831 | `	return NativePropClass(pClass) != 0;` |
|        5 |  832 | `}` |
|        - |  833 | `/*` |
|        - |  834 | ` * Run the hook. Answers 0 when the class has none, or when the hook DECLINED` |
|        - |  835 | ` * the name (bAnswered left at 0); 1 when it answered, which includes a` |
|        - |  836 | ` * REFUSAL -- the caller reads zThrowClass to tell the two apart.` |
|        - |  837 | ` */` |
|    40713 |  838 | `PH7_PRIVATE int PH7_ClassNativeProp(ph7_class_instance *pThis,PH7_NativePropCtx *pCtx)` |
|        5 |  839 | `{` |
|    40718 |  840 | `	ph7_class *pClass = pThis ? NativePropClass(pThis->pClass) : 0;` |
|    40718 |  841 | `	if( pClass == 0 ){` |
|       17 |  842 | `		return 0;` |
|        - |  843 | `	}` |
|    40702 |  844 | `	pClass->xProp(pThis->pVm,pThis,pCtx);` |
|    40702 |  845 | `	return pCtx->bAnswered \|\| pCtx->zThrowClass != 0;` |
|    20362 |  846 | `}` |
|        - |  847 | `/*` |
|        - |  848 | ` * Fill a caller-owned context and run the hook, for the callers that ask` |
|        - |  849 | ` * OUTSIDE the member opcode: Reflection's getValue()/setValue() and` |
|        - |  850 | ` * property_exists(), each of which reaches php's handlers by its own door.` |
|        - |  851 | ` */` |
|     3410 |  852 | `PH7_PRIVATE int PH7_ClassNativePropAsk(ph7_class_instance *pThis,PH7_NativePropCtx *pCtx,` |
|        - |  853 | `	int iMode,const SyString *pName,ph7_value *pResult)` |
|        5 |  854 | `{` |
|     3415 |  855 | `	pCtx->iMode = iMode;` |
|     3415 |  856 | `	pCtx->pName = pName;` |
|     3415 |  857 | `	pCtx->pResult = pResult;` |
|     3415 |  858 | `	pCtx->bAnswered = 0;` |
|     3415 |  859 | `	pCtx->zThrowClass = 0;` |
|     3415 |  860 | `	pCtx->zThrowMsg[0] = 0;` |
|     3415 |  861 | `	pCtx->iThrowCode = 0;` |
|     3415 |  862 | `	pCtx->bQuiet = 0;` |
|     3415 |  863 | `	pCtx->bWriteCtx = 0;` |
|     3415 |  864 | `	pCtx->nSlot = SXU32_HIGH;` |
|     3415 |  865 | `	return PH7_ClassNativeProp(pThis,pCtx);` |
|        5 |  866 | `}` |
|        - |  867 | `/*` |
|        - |  868 | `` * Does this class answer `$o->p` through a HANDLER of its own -- php's question`` |
|        - |  869 | ` * "is the name in the class's property-handler table"?` |
|        - |  870 | ` *` |
|        - |  871 | ` * The hook answers for itself, because what the answer depends on differs per` |
|        - |  872 | ` * class: ext/dom reads the class's VIRTUAL declarations (PH7_MOD_VIRTUAL -- a` |
|        - |  873 | ` * name declared with no slot of any kind), ArrayObject reads the object's` |
|        - |  874 | ` * ARRAY_AS_PROPS flag and owns every name once it is set. Whether a REAL property` |
|        - |  875 | ` * of that name is in the way is not the hook's question: every caller asks only` |
|        - |  876 | ` * after the instance's own table missed, which is php's order too.` |
|        - |  877 | ` *` |
|        - |  878 | `` * It is what makes the handler beat a subclass's own `__get`: a name the table`` |
|        - |  879 | ` * carries never reaches the magic layer, and a name it does not carry falls` |
|        - |  880 | ` * through to it, which is php's handler order. Asked at the write shapes, where` |
|        - |  881 | ` * the member opcode has no value yet and the write half cannot be run for an` |
|        - |  882 | ` * answer.` |
|        - |  883 | ` */` |
|     3693 |  884 | `PH7_PRIVATE int PH7_ClassNativePropOwns(ph7_class_instance *pThis,const SyString *pName)` |
|        5 |  885 | `{` |
|        - |  886 | `	PH7_NativePropCtx sCtx;` |
|     3698 |  887 | `	if( pThis == 0 \|\| NativePropClass(pThis->pClass) == 0 ){` |
|     1464 |  888 | `		return 0;` |
|        - |  889 | `	}` |
|     3349 |  890 | `	return PH7_ClassNativePropAsk(pThis,&sCtx,PH7_NATIVE_PROP_OWNS,pName,0)` |
|     2234 |  891 | `	    && sCtx.zThrowClass == 0;` |
|     1849 |  892 | `}` |
|        - |  893 | `/*` |
|        - |  894 | ` * Install a property handler on a mounted native class. Called by the owning` |
|        - |  895 | ` * installer right after PH7_InstallNativeClasses, for the same reason xClone,` |
|        - |  896 | ` * xDim and xSet are: the spec table has no field for a hook.` |
|        - |  897 | ` */` |
|   135120 |  898 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallPropHook(ph7_vm *pVm,const char *zClass,` |
|        - |  899 | `	void (*xProp)(ph7_vm *,ph7_class_instance *,PH7_NativePropCtx *))` |
|        5 |  900 | `{` |
|   135125 |  901 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|   135125 |  902 | `	if( pClass == 0 ){` |
|      ! 0 |  903 | `		return SXERR_NOTFOUND;` |
|        - |  904 | `	}` |
|   135125 |  905 | `	pClass->xProp = xProp;` |
|   135125 |  906 | `	return SXRET_OK;` |
|    67477 |  907 | `}` |
|        - |  908 | `/*` |
|        - |  909 | ` * The nearest ph7_class::xSet in a class's base chain -- the same handler` |
|        - |  910 | ` * inheritance the dimension hook gets, so a user subclass of DateInterval` |
|        - |  911 | ` * converts its writes the way its parent does.` |
|        - |  912 | ` */` |
|      618 |  913 | `static ph7_class * NativeSetClass(ph7_class *pClass)` |
|        3 |  914 | `{` |
|      623 |  915 | `	while( pClass ){` |
|      623 |  916 | `		if( pClass->xSet ){` |
|      621 |  917 | `			return pClass;` |
|        - |  918 | `		}` |
|        3 |  919 | `		pClass = pClass->pBase;` |
|        1 |  920 | `	}` |
|      ! 0 |  921 | `	return 0;` |
|      312 |  922 | `}` |
|        - |  923 | `/*` |
|        - |  924 | ` * Run the write handler for a property store. Answers 0 when no class in the` |
|        - |  925 | ` * chain has one (nothing in pCtx is touched); 1 when it ran, which includes a` |
|        - |  926 | ` * REFUSAL -- the caller reads zThrowClass to tell the two apart.` |
|        - |  927 | ` */` |
|      618 |  928 | `PH7_PRIVATE int PH7_ClassNativeSet(ph7_class_instance *pThis,PH7_NativeSetCtx *pCtx)` |
|        3 |  929 | `{` |
|      621 |  930 | `	ph7_class *pClass = pThis ? NativeSetClass(pThis->pClass) : 0;` |
|      621 |  931 | `	if( pClass == 0 ){` |
|      ! 0 |  932 | `		return 0;` |
|        - |  933 | `	}` |
|      621 |  934 | `	pClass->xSet(pThis->pVm,pThis,pCtx);` |
|      621 |  935 | `	return 1;` |
|      312 |  936 | `}` |
|        - |  937 | `/*` |
|        - |  938 | ` * php's create_object handler for a mounted native class: the C routine that` |
|        - |  939 | ` * runs once the instance frame exists and before any constructor.` |
|        - |  940 | ` *` |
|        - |  941 | ` * It exists for one shape -- a class whose properties php DECLARES and then` |
|        - |  942 | ` * answers through a read_property handler rather than out of the slots. Seeding` |
|        - |  943 | `` * the slots reproduces every surface of that at once (the read, `var_dump`, the`` |
|        - |  944 | `` * `(array)` cast, `json_encode`, `foreach`, `serialize`, `get_object_vars`),`` |
|        - |  945 | ` * and leaves the DECLARATION alone: the properties still have no default, so` |
|        - |  946 | ` * Reflection's hasDefaultValue() answers false the way php's does.` |
|        - |  947 | ` */` |
|     8445 |  948 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallNewHook(ph7_vm *pVm,const char *zClass,` |
|        - |  949 | `	void (*xNew)(ph7_vm *,ph7_class_instance *))` |
|        5 |  950 | `{` |
|     8450 |  951 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|     8450 |  952 | `	if( pClass == 0 ){` |
|      ! 0 |  953 | `		return SXERR_NOTFOUND;` |
|        - |  954 | `	}` |
|     8450 |  955 | `	pClass->xNew = xNew;` |
|     8450 |  956 | `	return SXRET_OK;` |
|     4222 |  957 | `}` |
|        - |  958 | `/*` |
|        - |  959 | ` * Install a write handler on a mounted native class and mark every INSTANCE` |
|        - |  960 | ` * property it declares as filtered, which is what makes instantiation register` |
|        - |  961 | ` * the slots the filter looks up. Called by the owning installer right after` |
|        - |  962 | ` * PH7_InstallNativeClasses, for the same reason xClone and xDim are: the spec` |
|        - |  963 | ` * table has no field for a hook.` |
|        - |  964 | ` */` |
|    16890 |  965 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallSetHook(ph7_vm *pVm,const char *zClass,` |
|        - |  966 | `	void (*xSet)(ph7_vm *,ph7_class_instance *,PH7_NativeSetCtx *))` |
|        5 |  967 | `{` |
|    16895 |  968 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|        - |  969 | `	SyHashEntry *pEntry;` |
|    16895 |  970 | `	if( pClass == 0 ){` |
|      ! 0 |  971 | `		return SXERR_NOTFOUND;` |
|        - |  972 | `	}` |
|    16895 |  973 | `	pClass->xSet = xSet;` |
|    16895 |  974 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|   194251 |  975 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|   168905 |  976 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   168905 |  977 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|   168905 |  978 | `			pAttr->iFlags \|= PH7_CLASS_ATTR_NATIVE_SET;` |
|    84340 |  979 | `		}` |
|        5 |  980 | `	}` |
|    16895 |  981 | `	return SXRET_OK;` |
|     8439 |  982 | `}` |
|        - |  983 | `/*` |
|        - |  984 | ` * The nearest ph7_class::xCmp in a class's base chain -- the same handler` |
|        - |  985 | ` * inheritance xDim and xSet get, and php's own: a subclass of DateTime still` |
|        - |  986 | ` * compares as an instant, whatever properties it adds.` |
|        - |  987 | ` */` |
|     1164 |  988 | `static ph7_class * NativeCmpClass(ph7_class *pClass)` |
|        5 |  989 | `{` |
|     2047 |  990 | `	while( pClass ){` |
|     1179 |  991 | `		if( pClass->xCmp ){` |
|      301 |  992 | `			return pClass;` |
|        - |  993 | `		}` |
|      883 |  994 | `		pClass = pClass->pBase;` |
|        5 |  995 | `	}` |
|      873 |  996 | `	return 0;` |
|      587 |  997 | `}` |
|        - |  998 | `/*` |
|        - |  999 | ` * Ask the LEFT operand's compare handler, php-style. Answers 0 when no class in` |
|        - | 1000 | ` * its chain has one (the caller falls back to the property walk); 1 when the` |
|        - | 1001 | ` * handler decided, and *pResult is then the ordering -- which includes a` |
|        - | 1002 | ` * REFUSAL, recorded on the VM for the nearest throw boundary to raise, with the` |
|        - | 1003 | ` * uncomparable 1 standing in as the answer meanwhile.` |
|        - | 1004 | ` */` |
|      822 | 1005 | `PH7_PRIVATE int PH7_ClassNativeCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,sxi32 *pResult)` |
|        5 | 1006 | `{` |
|      827 | 1007 | `	ph7_class *pClass = pLeft ? NativeCmpClass(pLeft->pClass) : 0;` |
|        - | 1008 | `	PH7_NativeCmpCtx sCtx;` |
|        - | 1009 | `	ph7_vm *pVm;` |
|      827 | 1010 | `	if( pClass == 0 ){` |
|      651 | 1011 | `		return 0;` |
|        - | 1012 | `	}` |
|      181 | 1013 | `	pVm = pLeft->pVm;` |
|      181 | 1014 | `	SyZero(&sCtx,sizeof(sCtx));` |
|      181 | 1015 | `	sCtx.pOther = pRight;` |
|      181 | 1016 | `	sCtx.iResult = 1;   /* php's ZEND_UNCOMPARABLE: what a hook that recognizes nothing answers */` |
|      181 | 1017 | `	pClass->xCmp(pVm,pLeft,&sCtx);` |
|      181 | 1018 | `	if( sCtx.zThrowClass && pVm->zCmpRefusalClass == 0 ){` |
|        - | 1019 | `		/* First refusal wins: a driver that keeps comparing after one (sort() does)` |
|        - | 1020 | `		 * must not overwrite the message the script will actually see. */` |
|       53 | 1021 | `		pVm->zCmpRefusalClass = sCtx.zThrowClass;` |
|       53 | 1022 | `		SyMemcpy(sCtx.zThrowMsg,pVm->zCmpRefusalMsg,sizeof(pVm->zCmpRefusalMsg));` |
|       53 | 1023 | `		pVm->zCmpRefusalMsg[sizeof(pVm->zCmpRefusalMsg)-1] = 0;` |
|       26 | 1024 | `	}` |
|      181 | 1025 | `	*pResult = sCtx.iResult;` |
|      181 | 1026 | `	return 1;` |
|      416 | 1027 | `}` |
|        - | 1028 | `/*` |
|        - | 1029 | ` * The same handler, asked about a SCALAR partner -- php's compare handler is one` |
|        - | 1030 | `` * door and `$n == 2` reaches it exactly as `$n == $m` does. Answers 1 only when`` |
|        - | 1031 | ` * the hook RECOGNIZED the value; otherwise the caller falls back to php's` |
|        - | 1032 | ` * cast-the-object rule, which is what every class without a handler gets.` |
|        - | 1033 | ` */` |
|      342 | 1034 | `PH7_PRIVATE int PH7_ClassNativeCmpValue(ph7_class_instance *pLeft,ph7_value *pOther,` |
|        - | 1035 | `	int bReversed,sxi32 *pResult)` |
|        5 | 1036 | `{` |
|      347 | 1037 | `	ph7_class *pClass = pLeft ? NativeCmpClass(pLeft->pClass) : 0;` |
|        - | 1038 | `	PH7_NativeCmpCtx sCtx;` |
|        - | 1039 | `	ph7_vm *pVm;` |
|      347 | 1040 | `	if( pClass == 0 ){` |
|      227 | 1041 | `		return 0;` |
|        - | 1042 | `	}` |
|      124 | 1043 | `	pVm = pLeft->pVm;` |
|      124 | 1044 | `	SyZero(&sCtx,sizeof(sCtx));` |
|      124 | 1045 | `	sCtx.pOtherValue = pOther;` |
|      124 | 1046 | `	sCtx.bReversed = bReversed;` |
|      124 | 1047 | `	sCtx.iResult = 1;   /* php's ZEND_UNCOMPARABLE */` |
|      124 | 1048 | `	pClass->xCmp(pVm,pLeft,&sCtx);` |
|      124 | 1049 | `	if( sCtx.zThrowClass && pVm->zCmpRefusalClass == 0 ){` |
|      ! 0 | 1050 | `		pVm->zCmpRefusalClass = sCtx.zThrowClass;` |
|      ! 0 | 1051 | `		SyMemcpy(sCtx.zThrowMsg,pVm->zCmpRefusalMsg,sizeof(pVm->zCmpRefusalMsg));` |
|      ! 0 | 1052 | `		pVm->zCmpRefusalMsg[sizeof(pVm->zCmpRefusalMsg)-1] = 0;` |
|      ! 0 | 1053 | `	}` |
|      124 | 1054 | `	if( !sCtx.bAnswered ){` |
|       29 | 1055 | `		return 0;` |
|        - | 1056 | `	}` |
|       97 | 1057 | `	*pResult = sCtx.iResult;` |
|       97 | 1058 | `	return 1;` |
|      176 | 1059 | `}` |
|        - | 1060 | `/*` |
|        - | 1061 | ` * Install a compare handler on a mounted native class. Called by the owning` |
|        - | 1062 | ` * installer right after PH7_InstallNativeClasses, for the reason xClone, xDim` |
|        - | 1063 | ` * and xSet are: the spec table has no field for a hook.` |
|        - | 1064 | ` */` |
|        - | 1065 | `/*` |
|        - | 1066 | ` * php's cast_object handler for _IS_BOOL, which is the one conversion an object` |
|        - | 1067 | ` * may decide for itself. Answers 1 when the class HAS a handler, with the truth` |
|        - | 1068 | ` * value in *pOut; every other class keeps php's rule that an object is truthy.` |
|        - | 1069 | ` */` |
|      972 | 1070 | `PH7_PRIVATE int PH7_ClassNativeBool(ph7_class_instance *pThis,int *pOut)` |
|        5 | 1071 | `{` |
|        - | 1072 | `	ph7_class *pClass;` |
|     2905 | 1073 | `	for( pClass = pThis ? pThis->pClass : 0 ; pClass ; pClass = pClass->pBase ){` |
|     1953 | 1074 | `		if( pClass->xBool ){` |
|       22 | 1075 | `			*pOut = pClass->xBool(pThis->pVm,pThis) ? 1 : 0;` |
|       22 | 1076 | `			return 1;` |
|        - | 1077 | `		}` |
|      967 | 1078 | `	}` |
|      957 | 1079 | `	return 0;` |
|      489 | 1080 | `}` |
|     8445 | 1081 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallBoolHook(ph7_vm *pVm,const char *zClass,` |
|        - | 1082 | `	int (*xBool)(ph7_vm *,ph7_class_instance *))` |
|        5 | 1083 | `{` |
|     8450 | 1084 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|     8450 | 1085 | `	if( pClass == 0 ){` |
|      ! 0 | 1086 | `		return SXERR_NOTFOUND;` |
|        - | 1087 | `	}` |
|     8450 | 1088 | `	pClass->xBool = xBool;` |
|     8450 | 1089 | `	return SXRET_OK;` |
|     4222 | 1090 | `}` |
|     8445 | 1091 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallArithHook(ph7_vm *pVm,const char *zClass,` |
|        - | 1092 | `	void (*xArith)(ph7_vm *,ph7_class_instance *,PH7_NativeArithCtx *))` |
|        5 | 1093 | `{` |
|     8450 | 1094 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|     8450 | 1095 | `	if( pClass == 0 ){` |
|      ! 0 | 1096 | `		return SXERR_NOTFOUND;` |
|        - | 1097 | `	}` |
|     8450 | 1098 | `	pClass->xArith = xArith;` |
|     8450 | 1099 | `	return SXRET_OK;` |
|     4222 | 1100 | `}` |
|        - | 1101 | `/*` |
|        - | 1102 | ` * php's compare handler for an OPAQUE HANDLE class -- one whose object stands for` |
|        - | 1103 | ` * something outside the engine (a curl easy/multi/share handle, a PDO connection,` |
|        - | 1104 | ` * a statement, a lazy row). php gives each of them a handler that recognizes` |
|        - | 1105 | ` * NOTHING, so every comparison that is not the identity shortcut is` |
|        - | 1106 | `` * ZEND_UNCOMPARABLE: `$h == 1`, `$h < 2`, `$h > 0` and `$h == $other` are all`` |
|        - | 1107 | `` * false, `$h <=> $x` is 1 from either direction, and none of it says a word.`` |
|        - | 1108 | ` * Without it these fell through to php's cast-the-object rule, which warns` |
|        - | 1109 | `` * `could not be converted to int` and then calls the handle equal to 1 -- so`` |
|        - | 1110 | ` * in_array($h, [1,2,3]) was TRUE, and sorting a list that held one was noise.` |
|        - | 1111 | ` *` |
|        - | 1112 | ` * The uncomparable 1 is deliberately not flipped for bReversed: php answers it` |
|        - | 1113 | ` * from both sides alike, which is what leaves every relational spelling false.` |
|        - | 1114 | ` */` |
|      108 | 1115 | `PH7_PRIVATE void PH7_NativeCmpOpaqueHandle(ph7_vm *pVm,ph7_class_instance *pThis,` |
|        - | 1116 | `	PH7_NativeCmpCtx *pCtx)` |
|        5 | 1117 | `{` |
|       54 | 1118 | `	SXUNUSED(pVm);` |
|       54 | 1119 | `	SXUNUSED(pThis);` |
|      113 | 1120 | `	if( pCtx->pOtherValue && (pCtx->pOtherValue->iFlags & MEMOBJ_BOOL) ){` |
|       16 | 1121 | `		return;   /* declined: php's cast rule decides an object against a bool */` |
|        - | 1122 | `	}` |
|       99 | 1123 | `	pCtx->bAnswered = 1;` |
|       99 | 1124 | `	pCtx->iResult = 1;   /* php's ZEND_UNCOMPARABLE */` |
|       59 | 1125 | `}` |
|        - | 1126 | `/*` |
|        - | 1127 | `` * TRUE when `(int)` on an instance of this class answers the object handle`` |
|        - | 1128 | ` * (PH7_CLASS_HANDLE_ID). Resolved through the ANCESTORS, like every other native` |
|        - | 1129 | ` * hook: php installs the cast on the class's object handlers, and a subclass` |
|        - | 1130 | ` * inherits the whole handler table.` |
|        - | 1131 | ` */` |
|       42 | 1132 | `PH7_PRIVATE int PH7_ClassCastsToHandleId(ph7_class *pClass)` |
|        4 | 1133 | `{` |
|       68 | 1134 | `	while( pClass ){` |
|       46 | 1135 | `		if( pClass->iFlags & PH7_CLASS_HANDLE_ID ){` |
|       21 | 1136 | `			return 1;` |
|        - | 1137 | `		}` |
|       26 | 1138 | `		pClass = pClass->pBase;` |
|        4 | 1139 | `	}` |
|       26 | 1140 | `	return 0;` |
|       25 | 1141 | `}` |
|        - | 1142 | `/* The same base-chain question for the two flags beside it: a subclass of` |
|        - | 1143 | ` * SimpleXMLElement casts to a number and answers get_object_vars the way its` |
|        - | 1144 | ` * parent does, which is php's handler inheritance. */` |
|      126 | 1145 | `PH7_PRIVATE int PH7_ClassNumberIsString(ph7_class *pClass)` |
|        5 | 1146 | `{` |
|      237 | 1147 | `	while( pClass ){` |
|      131 | 1148 | `		if( pClass->iFlags & PH7_CLASS_NUM_AS_STRING ){` |
|       21 | 1149 | `			return 1;` |
|        - | 1150 | `		}` |
|      110 | 1151 | `		pClass = pClass->pBase;` |
|        4 | 1152 | `	}` |
|      110 | 1153 | `	return 0;` |
|       68 | 1154 | `}` |
|      390 | 1155 | `PH7_PRIVATE int PH7_ClassVarsFromPresent(ph7_class *pClass)` |
|        5 | 1156 | `{` |
|      867 | 1157 | `	while( pClass ){` |
|      479 | 1158 | `		if( pClass->iFlags & PH7_CLASS_VARS_PRESENT ){` |
|        3 | 1159 | `			return 1;` |
|        - | 1160 | `		}` |
|      477 | 1161 | `		pClass = pClass->pBase;` |
|        5 | 1162 | `	}` |
|      393 | 1163 | `	return 0;` |
|      200 | 1164 | `}` |
|    59115 | 1165 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallCmpHook(ph7_vm *pVm,const char *zClass,` |
|        - | 1166 | `	void (*xCmp)(ph7_vm *,ph7_class_instance *,PH7_NativeCmpCtx *))` |
|        5 | 1167 | `{` |
|    59120 | 1168 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|    59120 | 1169 | `	if( pClass == 0 ){` |
|      ! 0 | 1170 | `		return SXERR_NOTFOUND;` |
|        - | 1171 | `	}` |
|    59120 | 1172 | `	pClass->xCmp = xCmp;` |
|    59120 | 1173 | `	return SXRET_OK;` |
|    29524 | 1174 | `}` |
|        - | 1175 | `/*` |
|        - | 1176 | ` * Mark every INSTANCE property a mounted native class declares as one php` |
|        - | 1177 | ` * FABRICATES rather than stores (PH7_CLASS_ATTR_NATIVE_VIRTUAL), which is what` |
|        - | 1178 | ` * keeps the object comparator from seeing it. DatePeriod is the whole caller` |
|        - | 1179 | ` * list: php's object has an EMPTY real property table, so two of them are equal` |
|        - | 1180 | ` * whatever they contain, while a subclass's own property still decides.` |
|        - | 1181 | ` */` |
|    16890 | 1182 | `PH7_PRIVATE sxi32 PH7_NativeClassMarkVirtualProps(ph7_vm *pVm,const char *zClass)` |
|        5 | 1183 | `{` |
|    16895 | 1184 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|        - | 1185 | `	SyHashEntry *pEntry;` |
|    16895 | 1186 | `	if( pClass == 0 ){` |
|      ! 0 | 1187 | `		return SXERR_NOTFOUND;` |
|        - | 1188 | `	}` |
|    16895 | 1189 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|   109801 | 1190 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|    84455 | 1191 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|    84455 | 1192 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|    84455 | 1193 | `			pAttr->iFlags \|= PH7_CLASS_ATTR_NATIVE_VIRTUAL;` |
|    42170 | 1194 | `		}` |
|        5 | 1195 | `	}` |
|    16895 | 1196 | `	return SXRET_OK;` |
|     8439 | 1197 | `}` |
|        - | 1198 | `/*` |
|        - | 1199 | ` * Mark every php-VISIBLE instance property a mounted native class declares as one` |
|        - | 1200 | ` * the OBJECT does not hold until its constructor fills it` |
|        - | 1201 | ` * (PH7_CLASS_ATTR_NATIVE_LAZY). php's DateInterval and DatePeriod are the caller` |
|        - | 1202 | ` * list: the state is a C struct the constructor allocates and the property table` |
|        - | 1203 | ` * is written FROM it, so an object nobody constructed has no such property at all.` |
|        - | 1204 | ` *` |
|        - | 1205 | ` * The HIDDEN slots are left alone -- they are PHL's own storage, they have to` |
|        - | 1206 | `` * exist from `new` (the initialized FLAG lives in one of them), and php shows`` |
|        - | 1207 | ` * nothing for them either way.` |
|        - | 1208 | ` *` |
|        - | 1209 | ` * bDefaultRead selects which of php's two handlers the class has: with it, a read` |
|        - | 1210 | ` * of a still-absent slot answers the DECLARED literal in silence (DatePeriod's` |
|        - | 1211 | ` * read_property over the zeroed struct); without it, the name really is undefined` |
|        - | 1212 | ` * until the constructor runs (DateInterval).` |
|        - | 1213 | ` */` |
|        - | 1214 | `/*` |
|        - | 1215 | ` * Mark every php-VISIBLE instance property a mounted native class declares as one` |
|        - | 1216 | ` * whose WRITE php's handler refuses (PH7_CLASS_ATTR_NATIVE_NOWRITE). DatePeriod is` |
|        - | 1217 | `` * the caller list: php answers `Cannot modify readonly property DatePeriod::$p` to`` |
|        - | 1218 | `` * every write form and `Cannot unset DatePeriod::$p` to an unset, while Reflection`` |
|        - | 1219 | ` * still reports isReadOnly() false -- the wording is the handler's, not the` |
|        - | 1220 | ` * readonly flag's. The C bodies that fill the seven write their slots directly and` |
|        - | 1221 | ` * never pass the store filter, so the refusal costs the class nothing.` |
|        - | 1222 | ` */` |
|     8445 | 1223 | `PH7_PRIVATE sxi32 PH7_NativeClassMarkNoWriteProps(ph7_vm *pVm,const char *zClass)` |
|        5 | 1224 | `{` |
|     8450 | 1225 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|        - | 1226 | `	SyHashEntry *pEntry;` |
|     8450 | 1227 | `	if( pClass == 0 ){` |
|      ! 0 | 1228 | `		return SXERR_NOTFOUND;` |
|        - | 1229 | `	}` |
|     8450 | 1230 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|    76010 | 1231 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|    67565 | 1232 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|    67565 | 1233 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_HIDDEN) ){` |
|     8450 | 1234 | `			continue;` |
|        - | 1235 | `		}` |
|    59120 | 1236 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_NATIVE_NOWRITE;` |
|        5 | 1237 | `	}` |
|     8450 | 1238 | `	return SXRET_OK;` |
|     4222 | 1239 | `}` |
|        - | 1240 | `/*` |
|        - | 1241 | ` * Mark ONE property of ONE instance as php's read-only kind: a plain store and` |
|        - | 1242 | `` * an unset() refuse with `Property p is read only`, everything that takes a`` |
|        - | 1243 | ` * pointer to it goes through. It is marked per OBJECT because php's handler is` |
|        - | 1244 | ` * -- a PDOStatement with no cursor behind it takes the write, and only the one` |
|        - | 1245 | ` * a driver built refuses.` |
|        - | 1246 | ` */` |
|      920 | 1247 | `PH7_PRIVATE void PH7_NativeMarkAttrReadOnly(ph7_class_instance *pThis,const char *zProp)` |
|        4 | 1248 | `{` |
|      924 | 1249 | `	SyHashEntry *pEntry = pThis` |
|      920 | 1250 | `		? SyHashGet(&pThis->hAttr,(const void *)zProp,(sxu32)SyStrlen(zProp)) : 0;` |
|      924 | 1251 | `	if( pEntry ){` |
|      924 | 1252 | `		((VmClassAttr *)pEntry->pUserData)->iState \|= VM_CLASS_ATTR_RDONLY;` |
|      460 | 1253 | `	}` |
|      924 | 1254 | `}` |
|    25335 | 1255 | `PH7_PRIVATE sxi32 PH7_NativeClassMarkLazyProps(ph7_vm *pVm,const char *zClass,int bDefaultRead)` |
|        5 | 1256 | `{` |
|    25340 | 1257 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|        - | 1258 | `	SyHashEntry *pEntry;` |
|    25340 | 1259 | `	if( pClass == 0 ){` |
|      ! 0 | 1260 | `		return SXERR_NOTFOUND;` |
|        - | 1261 | `	}` |
|    25340 | 1262 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|   228020 | 1263 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|   202685 | 1264 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   202685 | 1265 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_HIDDEN) ){` |
|    42230 | 1266 | `			continue;` |
|        - | 1267 | `		}` |
|   160460 | 1268 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_NATIVE_LAZY;` |
|   160460 | 1269 | `		if( bDefaultRead ){` |
|    59120 | 1270 | `			pAttr->iFlags \|= PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT;` |
|    29519 | 1271 | `		}` |
|   160460 | 1272 | `		pClass->iFlags \|= PH7_CLASS_LAZY_ATTR;` |
|        5 | 1273 | `	}` |
|    25340 | 1274 | `	return SXRET_OK;` |
|    12656 | 1275 | `}` |
|        - | 1276 | `/*` |
|        - | 1277 | ` * Install this object's LAZY properties -- the whole set, in the order the class` |
|        - | 1278 | ` * declares them, skipping any the object already carries.` |
|        - | 1279 | ` *` |
|        - | 1280 | ` * The ORDER is php's: its constructor writes the struct's fields into the property` |
|        - | 1281 | ` * table one after another, so a name the object already has keeps its POSITION and` |
|        - | 1282 | ` * only takes the new value, and the rest are appended in declared order behind it.` |
|        - | 1283 | ` * VmRecreateDeclaredAttr tail-inserts exactly that way.` |
|        - | 1284 | ` *` |
|        - | 1285 | ` * The declared literal goes in as the slot's starting value (php's zeroed struct),` |
|        - | 1286 | ` * and the not-yet-initialized mark a TYPED slot would carry is cleared with it:` |
|        - | 1287 | ` * these are filled by the C body that is about to write them, and a read between` |
|        - | 1288 | ` * the two is php's default, not its Error.` |
|        - | 1289 | ` */` |
|  1734066 | 1290 | `PH7_PRIVATE void PH7_NativeMaterializeLazy(ph7_vm *pVm,ph7_class_instance *pObj)` |
|        5 | 1291 | `{` |
|        - | 1292 | `	SyHashEntry *pEntry;` |
|  1734066 | 1293 | `	if( pObj == 0 \|\| (pObj->pClass->iFlags & PH7_CLASS_LAZY_ATTR) == 0` |
|   871364 | 1294 | `	 \|\| (pObj->iFlags & VM_INSTANCE_LAZY_DONE) ){` |
|  1733297 | 1295 | `		return;` |
|        - | 1296 | `	}` |
|      778 | 1297 | `	pObj->iFlags \|= VM_INSTANCE_LAZY_DONE;` |
|      778 | 1298 | `	SyHashResetLoopCursor(&pObj->pClass->hAttr);` |
|    10052 | 1299 | `	while( (pEntry = SyHashGetNextEntry(&pObj->pClass->hAttr)) != 0 ){` |
|     9278 | 1300 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     9278 | 1301 | `		VmClassAttr *pVmAttr = 0;` |
|        - | 1302 | `		ph7_value *pSlot;` |
|     9274 | 1303 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_LAZY) == 0` |
|     8574 | 1304 | `		 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_ONDEMAND) != 0 ){` |
|     2024 | 1305 | `			continue;   /* ...and an ON-DEMAND one waits for the write that names it */` |
|        - | 1306 | `		}` |
|     7258 | 1307 | `		if( SyHashGet(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName)) != 0 ){` |
|      ! 0 | 1308 | `			continue;` |
|        - | 1309 | `		}` |
|     7258 | 1310 | `		VmRecreateDeclaredAttr(&(*pVm),pObj,pAttr,&pVmAttr);` |
|     7258 | 1311 | `		if( pVmAttr == 0 ){` |
|      ! 0 | 1312 | `			continue;   /* OOM: the caller's write lands nowhere, as it would have anyway */` |
|        - | 1313 | `		}` |
|     7258 | 1314 | `		pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|     7258 | 1315 | `		if( pAttr->pNativeValue == 0 ){` |
|      ! 0 | 1316 | `			continue;` |
|        - | 1317 | `		}` |
|     7258 | 1318 | `		pSlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pVmAttr->nIdx);` |
|     7258 | 1319 | `		if( pSlot ){` |
|     7258 | 1320 | `			PH7_NativeLiteralValue(&(*pVm),pAttr->pNativeValue,pSlot);` |
|     3627 | 1321 | `		}` |
|        4 | 1322 | `	}` |
|   866998 | 1323 | `}` |
|        - | 1324 | `/*` |
|        - | 1325 | ` * Create and install ONE class from its spec: constants and properties, but` |
|        - | 1326 | ` * neither methods nor its base chain.` |
|        - | 1327 | ` *` |
|        - | 1328 | ` * Split from the two passes that follow because a spec table may describe` |
|        - | 1329 | ` * classes that extend each other, and PH7_ClassInherit needs the parent to` |
|        - | 1330 | ` * exist -- and to already CARRY ITS METHODS, since inheriting is what copies` |
|        - | 1331 | ` * them down.` |
|        - | 1332 | ` */` |
|  2043690 | 1333 | `static sxi32 NativeDeclareClass(ph7_vm *pVm,const PH7_NativeClassSpec *pSpec,ph7_class **ppOut)` |
|        5 | 1334 | `{` |
|        - | 1335 | `	ph7_class *pClass;` |
|        - | 1336 | `	SyString sName;` |
|        - | 1337 | `	sxu32 n;` |
|        - | 1338 | `	sxi32 rc;` |
|  2043695 | 1339 | `	SyStringInitFromBuf(&sName,pSpec->zName,SyStrlen(pSpec->zName));` |
|  2043695 | 1340 | `	pClass = PH7_NewRawClass(&(*pVm),&sName,0);` |
|  2043695 | 1341 | `	if( pClass == 0 ){` |
|      ! 0 | 1342 | `		return SXERR_MEM;` |
|        - | 1343 | `	}` |
|        - | 1344 | `	/* PH7_CLASS_BOUND: an engine class is declared unconditionally, exactly once,` |
|        - | 1345 | ``	 * so `class DateTime {}` in user code is php's "Cannot redeclare class`` |
|        - | 1346 | `	 * DateTime". Only the compiler used to set the flag, so EVERY native class` |
|        - | 1347 | `	 * was silently redeclarable — the chunk-declared ones (Exception, stdClass)` |
|        - | 1348 | `	 * fataled and their C replacements did not. */` |
|  2043695 | 1349 | `	pClass->iFlags \|= pSpec->iFlags \| PH7_CLASS_BOUND;` |
|  2043695 | 1350 | `	pClass->xRelease = pSpec->xRelease;` |
|  2043695 | 1351 | `	pClass->pIterVtab = pSpec->pIterVtab;` |
|  2043695 | 1352 | `	pClass->xPresent = pSpec->xPresent;` |
|  5447030 | 1353 | `	for( n = 0 ; n < pSpec->nConst ; n++ ){` |
|  3403340 | 1354 | `		rc = NativeInstallConstant(&(*pVm),pClass,&pSpec->aConst[n]);` |
|  3403340 | 1355 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1356 | `			return rc;` |
|        - | 1357 | `		}` |
|  1699456 | 1358 | `	}` |
|  6325310 | 1359 | `	for( n = 0 ; n < pSpec->nProp ; n++ ){` |
|  4281620 | 1360 | `		rc = PH7_NativeClassInstallProperty(&(*pVm),pClass,&pSpec->aProp[n]);` |
|  4281620 | 1361 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1362 | `			return rc;` |
|        - | 1363 | `		}` |
|  2138024 | 1364 | `	}` |
|  2043695 | 1365 | `	*ppOut = pClass;` |
|  2043695 | 1366 | `	return PH7_VmInstallClass(&(*pVm),pClass);` |
|  1020519 | 1367 | `}` |
|        - | 1368 | `/*` |
|        - | 1369 | ` * Wire ONE class's base chain and interfaces.` |
|        - | 1370 | ` *` |
|        - | 1371 | ` * This runs AFTER every class in the table has its own methods, which is the` |
|        - | 1372 | ` * compiler's order too (GenStateCompileClassEx compiles the whole body and only` |
|        - | 1373 | ` * then calls PH7_ClassInherit/PH7_ClassImplement). Two things depend on it:` |
|        - | 1374 | ` * PH7_ClassInherit COPIES the base's methods into the subclass, so a base whose` |
|        - | 1375 | ` * methods were not installed yet hands down an empty table -- which is how` |
|        - | 1376 | `` * `DOMDocument::C14N()` came out undefined, the first time a native class`` |
|        - | 1377 | ` * extended another native class that had methods; and PH7_ClassImplement stubs` |
|        - | 1378 | ` * every interface method the class lacks as ABSTRACT, so a spec may now name an` |
|        - | 1379 | ` * interface it implements itself rather than attaching it by hand afterwards.` |
|        - | 1380 | ` */` |
|  2043690 | 1381 | `static sxi32 NativeLinkClass(ph7_vm *pVm,const PH7_NativeClassSpec *pSpec,ph7_class *pClass)` |
|        5 | 1382 | `{` |
|        - | 1383 | `	sxi32 rc;` |
|  2043695 | 1384 | `	if( pSpec->zParent ){` |
|  1030295 | 1385 | `		ph7_class *pBase = NativeLookupClass(&(*pVm),pSpec->zParent);` |
|  1030295 | 1386 | `		if( pBase == 0 ){` |
|      ! 0 | 1387 | `			return SXERR_NOTFOUND;` |
|        - | 1388 | `		}` |
|        - | 1389 | `		/* The VM's OWN generator state, not a NULL one: PH7_ClassInherit takes its` |
|        - | 1390 | `		 * scratch allocator from pGen->pVm and reports every inheritance rule it` |
|        - | 1391 | `		 * enforces through PH7_GenCompileError(pGen, ...). Passing 0 crashed the` |
|        - | 1392 | `		 * moment a native spec first named a parent (the date exceptions). */` |
|  1546111 | 1393 | `		rc = (pClass->iFlags & PH7_CLASS_INTERFACE)` |
|    76005 | 1394 | `			? PH7_ClassInterfaceInherit(pClass,pBase)` |
|   992337 | 1395 | `			: PH7_ClassInherit(&pVm->sCodeGen,pClass,pBase);` |
|  1030295 | 1396 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1397 | `			return rc;` |
|        - | 1398 | `		}` |
|   514474 | 1399 | `	}` |
|  2043695 | 1400 | `	if( pSpec->zImplements ){` |
|        - | 1401 | `		/* Comma-separated list, so one spec row can name several interfaces. */` |
|   565820 | 1402 | `		const char *zCur = pSpec->zImplements;` |
|  1427210 | 1403 | `		while( zCur[0] != '\0' ){` |
|        - | 1404 | `			const char *zStart;` |
|        - | 1405 | `			char zIface[64];` |
|        - | 1406 | `			sxu32 nLen;` |
|        - | 1407 | `			ph7_class *pIface;` |
|  1156970 | 1408 | `			while( zCur[0] == ' ' \|\| zCur[0] == ',' ){` |
|   295580 | 1409 | `				zCur++;` |
|        5 | 1410 | `			}` |
|   861395 | 1411 | `			if( zCur[0] == '\0' ){` |
|      ! 0 | 1412 | `				break;` |
|        - | 1413 | `			}` |
|   861395 | 1414 | `			zStart = zCur;` |
| 11527430 | 1415 | `			while( zCur[0] != '\0' && zCur[0] != ',' && zCur[0] != ' ' ){` |
| 10666040 | 1416 | `				zCur++;` |
|        5 | 1417 | `			}` |
|   861395 | 1418 | `			nLen = (sxu32)(zCur - zStart);` |
|   861395 | 1419 | `			if( nLen >= sizeof(zIface) ){` |
|      ! 0 | 1420 | `				return SXERR_SYNTAX;` |
|        - | 1421 | `			}` |
|   861395 | 1422 | `			SyMemcpy(zStart,zIface,nLen);` |
|   861395 | 1423 | `			zIface[nLen] = '\0';` |
|   861395 | 1424 | `			pIface = NativeLookupClass(&(*pVm),zIface);` |
|   861395 | 1425 | `			if( pIface == 0 ){` |
|      ! 0 | 1426 | `				return SXERR_NOTFOUND;` |
|        - | 1427 | `			}` |
|   861395 | 1428 | `			rc = PH7_ClassImplement(pClass,pIface);` |
|   861395 | 1429 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1430 | `				return rc;` |
|        - | 1431 | `			}` |
|        5 | 1432 | `		}` |
|   282539 | 1433 | `	}` |
|  2043695 | 1434 | `	return SXRET_OK;` |
|  1020519 | 1435 | `}` |
|        - | 1436 | `/*` |
|        - | 1437 | ` * Install a whole table of native classes, in the compiler's own order: declare` |
|        - | 1438 | ` * them all (so later rows may extend earlier ones), fill in their methods, wire` |
|        - | 1439 | ` * the base chains and interfaces, then mount.` |
|        - | 1440 | ` *` |
|        - | 1441 | ` * Interfaces are declared but never mounted -- VmMountUserClass skips them anyway,` |
|        - | 1442 | ` * their methods being non-invocable.` |
|        - | 1443 | ` */` |
|   439140 | 1444 | `PH7_PRIVATE sxi32 PH7_InstallNativeClasses(ph7_vm *pVm,const PH7_NativeClassSpec *aSpec,sxu32 nSpec)` |
|        5 | 1445 | `{` |
|        - | 1446 | `	ph7_class **apClass;` |
|        - | 1447 | `	sxu32 i,j;` |
|        - | 1448 | `	sxi32 rc;` |
|   439145 | 1449 | `	apClass = (ph7_class **)SyMemBackendAlloc(&pVm->sAllocator,nSpec * sizeof(ph7_class *));` |
|   439145 | 1450 | `	if( apClass == 0 ){` |
|      ! 0 | 1451 | `		return SXERR_MEM;` |
|        - | 1452 | `	}` |
|  2482835 | 1453 | `	for( i = 0 ; i < nSpec ; i++ ){` |
|  2043695 | 1454 | `		apClass[i] = 0;` |
|  2043695 | 1455 | `		rc = NativeDeclareClass(&(*pVm),&aSpec[i],&apClass[i]);` |
|  2043695 | 1456 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1457 | `			goto Done;` |
|        - | 1458 | `		}` |
|  1020519 | 1459 | `	}` |
|  2482835 | 1460 | `	for( i = 0 ; i < nSpec ; i++ ){` |
| 15327680 | 1461 | `		for( j = 0 ; j < aSpec[i].nMethod ; j++ ){` |
| 13283990 | 1462 | `			rc = PH7_NativeClassInstallMethod(&(*pVm),apClass[i],&aSpec[i].aMethod[j],0);` |
| 13283990 | 1463 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1464 | `				goto Done;` |
|        - | 1465 | `			}` |
|  6633346 | 1466 | `		}` |
|  1020519 | 1467 | `	}` |
|  2482835 | 1468 | `	for( i = 0 ; i < nSpec ; i++ ){` |
|  2043695 | 1469 | `		rc = NativeLinkClass(&(*pVm),&aSpec[i],apClass[i]);` |
|  2043695 | 1470 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1471 | `			goto Done;` |
|        - | 1472 | `		}` |
|  1020519 | 1473 | `	}` |
|  2482835 | 1474 | `	for( i = 0 ; i < nSpec ; i++ ){` |
|  2043695 | 1475 | `		rc = VmMountUserClass(&(*pVm),apClass[i]);` |
|  2043695 | 1476 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1477 | `			goto Done;` |
|        - | 1478 | `		}` |
|  1020519 | 1479 | `	}` |
|   439145 | 1480 | `	rc = SXRET_OK;` |
|   219856 | 1481 | `Done:` |
|   439145 | 1482 | `	SyMemBackendFree(&pVm->sAllocator,apClass);` |
|   439145 | 1483 | `	return rc;` |
|   219289 | 1484 | `}` |
|        - | 1485 | `/*` |
|        - | 1486 | ` * ---------------------------------------------------------------------------` |
|        - | 1487 | ` * Declaring an ENUM from C.` |
|        - | 1488 | ` *` |
|        - | 1489 | `` * An enum is not a class with constants: each `case` is a class constant whose`` |
|        - | 1490 | ` * slot holds THE singleton instance of the enum for that case, materialized` |
|        - | 1491 | ` * lazily on first access (VmEnumMaterializeCase). The compiler builds one by` |
|        - | 1492 | `` * declaring the readonly `name`/`value` properties, pushing each case onto`` |
|        - | 1493 | ` * ph7_class::aEnumCases, and SYNTHESIZING cases()/from()/tryFrom() as PHP` |
|        - | 1494 | `` * source that forwards to the `__phl_enum_*` thunks.`` |
|        - | 1495 | ` *` |
|        - | 1496 | ` * This does the same three things without a compiler: the case's backing value` |
|        - | 1497 | ` * rides as a literal (ph7_class_attr::pNativeValue, which the materializer` |
|        - | 1498 | ` * reads where a compiled case has byte-code), and the three interface methods` |
|        - | 1499 | ` * are C bodies that call the very same engine workers the synthesized PHP` |
|        - | 1500 | `` * forwards to — so a native enum is an ordinary one to `instanceof`,`` |
|        - | 1501 | `` * `match`, Reflection and `===` case identity.`` |
|        - | 1502 | ` * ---------------------------------------------------------------------------` |
|        - | 1503 | ` */` |
|        - | 1504 | `/* The enum a static native method was called on. */` |
|      209 | 1505 | `static ph7_class * NativeEnumSelf(ph7_context *pCtx,ph7_value *pName)` |
|        2 | 1506 | `{` |
|      211 | 1507 | `	ph7_class *pClass = PH7_ContextCalledClass(pCtx);` |
|      211 | 1508 | `	PH7_MemObjInit(pCtx->pVm,pName);` |
|      211 | 1509 | `	if( pClass ){` |
|      211 | 1510 | `		ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|      104 | 1511 | `	}` |
|      211 | 1512 | `	return pClass;` |
|        2 | 1513 | `}` |
|        - | 1514 | `/*` |
|        - | 1515 | ` * cases() / from() / tryFrom(): the engine thunks take the enum's FQN as their` |
|        - | 1516 | ` * first argument, exactly as the compiler's synthesized bodies pass it.` |
|        - | 1517 | ` */` |
|       23 | 1518 | `static int vm_builtin_NativeEnum_cases(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 1519 | `{` |
|        - | 1520 | `	ph7_value sName;` |
|        - | 1521 | `	ph7_value *ap[1];` |
|        - | 1522 | `	int rc;` |
|       11 | 1523 | `	SXUNUSED(nArg);` |
|       11 | 1524 | `	SXUNUSED(apArg);` |
|       25 | 1525 | `	if( NativeEnumSelf(pCtx,&sName) == 0 ){` |
|      ! 0 | 1526 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1527 | `		return PH7_OK;` |
|        - | 1528 | `	}` |
|       25 | 1529 | `	ap[0] = &sName;` |
|       25 | 1530 | `	rc = vm_builtin_enum_cases(pCtx,1,ap);` |
|       25 | 1531 | `	PH7_MemObjRelease(&sName);` |
|       25 | 1532 | `	return rc;` |
|       13 | 1533 | `}` |
|      186 | 1534 | `static int NativeEnumFrom(ph7_context *pCtx,int nArg,ph7_value **apArg,int bTry)` |
|        2 | 1535 | `{` |
|        - | 1536 | `	ph7_value sName;` |
|        - | 1537 | `	ph7_value *ap[2];` |
|        - | 1538 | `	int rc;` |
|      188 | 1539 | `	if( nArg < 1 \|\| NativeEnumSelf(pCtx,&sName) == 0 ){` |
|      ! 0 | 1540 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1541 | `		return PH7_OK;` |
|        - | 1542 | `	}` |
|      188 | 1543 | `	ap[0] = &sName;` |
|      188 | 1544 | `	ap[1] = apArg[0];` |
|      188 | 1545 | `	rc = bTry ? vm_builtin_enum_tryfrom(pCtx,2,ap) : vm_builtin_enum_from(pCtx,2,ap);` |
|      188 | 1546 | `	PH7_MemObjRelease(&sName);` |
|      188 | 1547 | `	return rc;` |
|       95 | 1548 | `}` |
|      102 | 1549 | `static int vm_builtin_NativeEnum_from(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 1550 | `{` |
|      104 | 1551 | `	return NativeEnumFrom(pCtx,nArg,apArg,0);` |
|        2 | 1552 | `}` |
|       84 | 1553 | `static int vm_builtin_NativeEnum_tryFrom(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 1554 | `{` |
|       86 | 1555 | `	return NativeEnumFrom(pCtx,nArg,apArg,1);` |
|        2 | 1556 | `}` |
|        - | 1557 | ``/* The readonly `name` (every enum) and `value` (backed only) case properties,`` |
|        - | 1558 | ` * declared exactly as GenStateCompileEnum does. */` |
|    59115 | 1559 | `static sxi32 NativeEnumInstallProp(ph7_vm *pVm,ph7_class *pClass,const char *zName,` |
|        - | 1560 | `	sxu32 nType,const char *zTypeName)` |
|        5 | 1561 | `{` |
|        - | 1562 | `	SyString sName;` |
|        - | 1563 | `	ph7_class_attr *pAttr;` |
|    59120 | 1564 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|    59120 | 1565 | `	pAttr = PH7_NewClassAttr(&(*pVm),&sName,0,PH7_CLASS_PROT_PUBLIC,` |
|        - | 1566 | `		PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|    59120 | 1567 | `	if( pAttr == 0 ){` |
|      ! 0 | 1568 | `		return SXERR_MEM;` |
|        - | 1569 | `	}` |
|    59120 | 1570 | `	pAttr->nType = nType;` |
|    59120 | 1571 | `	SyStringInitFromBuf(&pAttr->sTypeName,zTypeName,SyStrlen(zTypeName));` |
|    59120 | 1572 | `	return PH7_ClassInstallAttr(pClass,pAttr);` |
|    29524 | 1573 | `}` |
|        - | 1574 | `/*` |
|        - | 1575 | ` * cases()/from()/tryFrom(), installed on ANY enum -- one declared from C here` |
|        - | 1576 | ` * and one the compiler just finished reading from source. php declares them on` |
|        - | 1577 | ``  * the UnitEnum/BackedEnum prototypes, which is why `from` takes `string\|int` `` |
|        - | 1578 | ` * rather than the enum's own backing type: the refusal for the wrong one is a` |
|        - | 1579 | ` * VALUE check inside the body, not the parameter's.` |
|        - | 1580 | ` *` |
|        - | 1581 | ` * The compiler used to synthesize PHP source forwarding to three global` |
|        - | 1582 | `` * `__phl_enum_*` thunks. Those names were php-visible, and the methods reported`` |
|        - | 1583 | `` * as `<user>` with the enum's file and line where php reports`` |
|        - | 1584 | `` * `<internal, prototype BackedEnum>`.`` |
|        - | 1585 | ` */` |
|        - | 1586 | `/*` |
|        - | 1587 | ` * Install one of them and stamp it INTERNAL unconditionally. The usual` |
|        - | 1588 | `` * `bCompilingBuiltin` stamp only fires while the engine compiles its OWN`` |
|        - | 1589 | ` * sources, and these three are attached to a class the compiler is reading out` |
|        - | 1590 | ` * of a USER file — but php reports them as internal wherever the enum is` |
|        - | 1591 | ` * declared: isInternal() true, getFileName() false, getStartLine() 0.` |
|        - | 1592 | ` */` |
|    76291 | 1593 | `static sxi32 NativeEnumInstallMethod(ph7_vm *pVm,ph7_class *pClass,` |
|        - | 1594 | `	const PH7_NativeMethodDef *pDef)` |
|        5 | 1595 | `{` |
|        - | 1596 | `	ph7_class_method *pMeth;` |
|    76296 | 1597 | `	sxi32 rc = PH7_NativeClassInstallMethod(&(*pVm),pClass,pDef,0);` |
|    76296 | 1598 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1599 | `		return rc;` |
|        - | 1600 | `	}` |
|    76296 | 1601 | `	pMeth = PH7_ClassExtractMethod(pClass,pDef->zName,(sxu32)SyStrlen(pDef->zName));` |
|    76296 | 1602 | `	if( pMeth ){` |
|    76296 | 1603 | `		pMeth->sFunc.iFlags \|= VM_FUNC_INTERNAL;` |
|        - | 1604 | `		/* getFileName() reads the recorded source file rather than the flag, and` |
|        - | 1605 | `		 * PH7_NewClassMethod stamped the user file the enum was read from. */` |
|    76296 | 1606 | `		SyStringInitFromBuf(&pMeth->sFunc.sFile,"",0);` |
|    38096 | 1607 | `	}` |
|    76296 | 1608 | `	return SXRET_OK;` |
|    38101 | 1609 | `}` |
|    42371 | 1610 | `PH7_PRIVATE sxi32 PH7_InstallEnumInterfaceMethods(ph7_vm *pVm,ph7_class *pClass)` |
|        5 | 1611 | `{` |
|        - | 1612 | `	static const PH7_NativeMethodDef sCases =` |
|        - | 1613 | `		{ "cases", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "", "array", vm_builtin_NativeEnum_cases };` |
|        - | 1614 | `	static const PH7_NativeMethodDef aFrom[] = {` |
|        - | 1615 | `		{ "from",    PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "string\|int $value", "static",` |
|        - | 1616 | `		  vm_builtin_NativeEnum_from },` |
|        - | 1617 | `		{ "tryFrom", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "string\|int $value", "?static",` |
|        - | 1618 | `		  vm_builtin_NativeEnum_tryFrom },` |
|        - | 1619 | `	};` |
|        - | 1620 | `	sxu32 n;` |
|    42376 | 1621 | `	sxi32 rc = NativeEnumInstallMethod(&(*pVm),pClass,&sCases);` |
|    42376 | 1622 | `	if( rc != SXRET_OK \|\| pClass->nEnumBacking == 0 ){` |
|    25416 | 1623 | `		return rc;` |
|        - | 1624 | `	}` |
|    50885 | 1625 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFrom) ; n++ ){` |
|    33925 | 1626 | `		rc = NativeEnumInstallMethod(&(*pVm),pClass,&aFrom[n]);` |
|    33925 | 1627 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1628 | `			return rc;` |
|        - | 1629 | `		}` |
|    16943 | 1630 | `	}` |
|    16965 | 1631 | `	return SXRET_OK;` |
|    21163 | 1632 | `}` |
|    42225 | 1633 | `PH7_PRIVATE sxi32 PH7_InstallNativeEnum(ph7_vm *pVm,const char *zName,sxu32 nBacking,` |
|        - | 1634 | `	const PH7_NativeEnumCase *aCase,sxu32 nCase,` |
|        - | 1635 | `	const PH7_NativeMethodDef *aMethod,sxu32 nMethod)` |
|        5 | 1636 | `{` |
|        - | 1637 | `	ph7_class *pClass, *pIface;` |
|        - | 1638 | `	SyString sName;` |
|        - | 1639 | `	sxu32 n;` |
|        - | 1640 | `	sxi32 rc;` |
|    42230 | 1641 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|    42230 | 1642 | `	pClass = PH7_NewRawClass(&(*pVm),&sName,0);` |
|    42230 | 1643 | `	if( pClass == 0 ){` |
|      ! 0 | 1644 | `		return SXERR_MEM;` |
|        - | 1645 | `	}` |
|        - | 1646 | `	/* php: no enum can be extended or instantiated, and the ENUM flag alone says` |
|        - | 1647 | ``	 * so -- the `extends` refusal names the enum rather than a final class, and it`` |
|        - | 1648 | `	 * is asked first. The FINAL flag is deliberately NOT set: php stamps` |
|        - | 1649 | `` 	 * ZEND_ACC_FINAL on a COMPILED enum only, so `isFinal()`/`getModifiers()` `` |
|        - | 1650 | ``	 * answer true/32 for `enum U {}` and false/0 for every enum php declares from`` |
|        - | 1651 | `	 * C (RoundingMode, PropertyHookType). Setting it here made an internal enum` |
|        - | 1652 | `	 * report itself as a userland one. */` |
|    42230 | 1653 | `	pClass->iFlags \|= PH7_CLASS_ENUM;` |
|    42230 | 1654 | `	pClass->nEnumBacking = nBacking;` |
|    42230 | 1655 | `	rc = NativeEnumInstallProp(&(*pVm),pClass,"name",MEMOBJ_STRING,"string");` |
|    42230 | 1656 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1657 | `		return rc;` |
|        - | 1658 | `	}` |
|    42230 | 1659 | `	if( nBacking != 0 ){` |
|    25329 | 1660 | `		rc = NativeEnumInstallProp(&(*pVm),pClass,"value",nBacking,` |
|     8434 | 1661 | `			nBacking == MEMOBJ_INT ? "int" : "string");` |
|    16895 | 1662 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1663 | `			return rc;` |
|        - | 1664 | `		}` |
|     8434 | 1665 | `	}` |
|   236465 | 1666 | `	for( n = 0 ; n < nCase ; n++ ){` |
|        - | 1667 | `		ph7_class_attr *pAttr;` |
|        - | 1668 | `		SyString sCase;` |
|   194240 | 1669 | `		SyStringInitFromBuf(&sCase,aCase[n].zName,SyStrlen(aCase[n].zName));` |
|   194240 | 1670 | `		pAttr = PH7_NewClassAttr(&(*pVm),&sCase,0,PH7_CLASS_PROT_PUBLIC,` |
|        - | 1671 | `			PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|   194240 | 1672 | `		if( pAttr == 0 ){` |
|      ! 0 | 1673 | `			return SXERR_MEM;` |
|        - | 1674 | `		}` |
|   194240 | 1675 | `		pAttr->pDeclClass = pClass;` |
|        - | 1676 | `		/* The backing literal where a compiled case carries byte-code. */` |
|   194240 | 1677 | `		if( nBacking != 0 ){` |
|    50675 | 1678 | `			pAttr->pNativeValue = &aCase[n].sValue;` |
|    25302 | 1679 | `		}` |
|   194240 | 1680 | `		rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   194240 | 1681 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1682 | `			return rc;` |
|        - | 1683 | `		}` |
|        - | 1684 | `		/* Declaration order, which is the order cases() reports. */` |
|   194240 | 1685 | `		SySetPut(&pClass->aEnumCases,(const void *)&pAttr);` |
|    96996 | 1686 | `	}` |
|    42230 | 1687 | `	for( n = 0 ; n < nMethod ; n++ ){` |
|      ! 0 | 1688 | `		rc = PH7_NativeClassInstallMethod(&(*pVm),pClass,&aMethod[n],0);` |
|      ! 0 | 1689 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1690 | `			return rc;` |
|        - | 1691 | `		}` |
|      ! 0 | 1692 | `	}` |
|    42230 | 1693 | `	rc = PH7_InstallEnumInterfaceMethods(&(*pVm),pClass);` |
|    42230 | 1694 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1695 | `		return rc;` |
|        - | 1696 | `	}` |
|    42230 | 1697 | `	rc = PH7_VmInstallClass(&(*pVm),pClass);` |
|    42230 | 1698 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1699 | `		return rc;` |
|        - | 1700 | `	}` |
|        - | 1701 | ``	/* php 8.1: every enum satisfies `instanceof UnitEnum`, a backed one`` |
|        - | 1702 | ``	 * `BackedEnum` too. Attached AFTER the methods, so PH7_ClassImplement's`` |
|        - | 1703 | `	 * abstract stubbing finds cases()/from()/tryFrom() already declared.` |
|        - | 1704 | `	 * A backed one names only BackedEnum, which BRINGS UnitEnum: php's own` |
|        - | 1705 | ``	 * internal enums list `BackedEnum, UnitEnum` in that order, and naming`` |
|        - | 1706 | `	 * both here would answer them the other way round. A compiled enum is a` |
|        - | 1707 | ``	 * different registration and really does name both (`Ct, UnitEnum,`` |
|        - | 1708 | ``	 * BackedEnum`), which is what compile_class.c spells. */`` |
|    42230 | 1709 | `	pIface = NativeLookupClass(&(*pVm),nBacking != 0 ? "BackedEnum" : "UnitEnum");` |
|    42230 | 1710 | `	if( pIface == 0 ){` |
|      ! 0 | 1711 | `		return SXERR_NOTFOUND;` |
|        - | 1712 | `	}` |
|    42230 | 1713 | `	rc = PH7_ClassImplement(pClass,pIface);` |
|    42230 | 1714 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1715 | `		return rc;` |
|        - | 1716 | `	}` |
|    42230 | 1717 | `	return VmMountUserClass(&(*pVm),pClass);` |
|    21090 | 1718 | `}` |
|        - | 1719 | `/*` |
|        - | 1720 | ` * ---------------------------------------------------------------------------` |
|        - | 1721 | ` * InternalIterator.` |
|        - | 1722 | ` *` |
|        - | 1723 | ` * A native IteratorAggregate cannot answer a Generator: a PHP generator IS its` |
|        - | 1724 | ` * byte-code, and a C body has none. php never answers one either -- its internal` |
|        - | 1725 | ` * aggregates hand back an InternalIterator wrapping the iterator their class` |
|        - | 1726 | ` * declared -- so PHL declares that class once, here, and every native aggregate` |
|        - | 1727 | ` * reaches it through ph7_class::pIterVtab.` |
|        - | 1728 | ` *` |
|        - | 1729 | ` * The cursor lives entirely in the iterator's own private slots. A vtable states` |
|        - | 1730 | ` * only how to REACH a position; reading it back is the same three methods for` |
|        - | 1731 | ` * everyone.` |
|        - | 1732 | ` * ---------------------------------------------------------------------------` |
|        - | 1733 | ` */` |
|        - | 1734 | `/*` |
|        - | 1735 | ` * The walk THIS iterator was made for: the vtable of the aggregate it holds,` |
|        - | 1736 | ` * looked up ALONG THE BASE CHAIN. A subclass of a native aggregate inherits the` |
|        - | 1737 | ` * walk the way it inherits the getIterator() that reaches it -- without this a` |
|        - | 1738 | `` * `class P extends DatePeriod {}` (or DOMNodeList, WeakMap, PDOStatement,`` |
|        - | 1739 | ` * FilesystemIterator) answered a real InternalIterator that yielded NOTHING, so` |
|        - | 1740 | ` * every foreach over one was silently empty.` |
|        - | 1741 | ` */` |
|    15556 | 1742 | `static const PH7_NativeIterVtab * NativeIterVtab(ph7_class_instance *pIt)` |
|        5 | 1743 | `{` |
|    15561 | 1744 | `	ph7_class_instance *pSrc = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);` |
|        - | 1745 | `	ph7_class *pClass;` |
|    15781 | 1746 | `	for( pClass = pSrc ? pSrc->pClass : 0 ; pClass ; pClass = pClass->pBase ){` |
|    15781 | 1747 | `		if( pClass->pIterVtab ){` |
|    15561 | 1748 | `			return pClass->pIterVtab;` |
|        - | 1749 | `		}` |
|      111 | 1750 | `	}` |
|      ! 0 | 1751 | `	return 0;` |
|     7783 | 1752 | `}` |
|        - | 1753 | `/* Hand the cursor back to the aggregate, for a class that shows its walk as one` |
|        - | 1754 | ` * of its own properties (see PH7_NativeIterVtab::xPublish). Every InternalIterator` |
|        - | 1755 | ` * method calls this -- php's aggregate is written from the iterator's methods, not` |
|        - | 1756 | ` * from the walk, so a getIterator() nobody has touched yet leaves it alone. */` |
|     6224 | 1757 | `static void NativeIterPublish(ph7_vm *pVm,ph7_class_instance *pIt)` |
|        5 | 1758 | `{` |
|     6229 | 1759 | `	const PH7_NativeIterVtab *pVtab = NativeIterVtab(pIt);` |
|     6229 | 1760 | `	if( pVtab && pVtab->xPublish ){` |
|     1005 | 1761 | `		pVtab->xPublish(&(*pVm),pIt);` |
|      502 | 1762 | `	}` |
|     6229 | 1763 | `}` |
|        - | 1764 | `/* May this iterator be walked? See PH7_NativeIterVtab::xGuard -- the aggregate` |
|        - | 1765 | ` * gets to refuse at each of the five methods, which is where php refuses. */` |
|     6238 | 1766 | `static int NativeIterRefused(ph7_context *pCtx,ph7_class_instance *pIt)` |
|        5 | 1767 | `{` |
|     6243 | 1768 | `	const PH7_NativeIterVtab *pVtab = NativeIterVtab(pIt);` |
|     6243 | 1769 | `	return (pVtab && pVtab->xGuard) ? pVtab->xGuard(pCtx,pIt) : 0;` |
|        5 | 1770 | `}` |
|      814 | 1771 | `static int vm_builtin_InternalIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 1772 | `{` |
|      819 | 1773 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|        - | 1774 | `	const PH7_NativeIterVtab *pVtab;` |
|      407 | 1775 | `	SXUNUSED(nArg);` |
|      407 | 1776 | `	SXUNUSED(apArg);` |
|      819 | 1777 | `	if( pThis == 0 ){` |
|      ! 0 | 1778 | `		return PH7_OK;` |
|        - | 1779 | `	}` |
|      819 | 1780 | `	if( NativeIterRefused(pCtx,pThis) ){` |
|       15 | 1781 | `		return PH7_OK;` |
|        - | 1782 | `	}` |
|      805 | 1783 | `	pVtab = NativeIterVtab(pThis);` |
|      805 | 1784 | `	if( pVtab == 0 \|\| pVtab->xRewind == 0 ){` |
|      ! 0 | 1785 | `		PH7_NativeSetAttrBool(pCtx->pVm,pThis,PH7_NATIVE_IT_DONE,1);` |
|      ! 0 | 1786 | `		return PH7_OK;` |
|        - | 1787 | `	}` |
|      805 | 1788 | `	pVtab->xRewind(pCtx->pVm,pThis);` |
|      805 | 1789 | `	NativeIterPublish(pCtx->pVm,pThis);` |
|      805 | 1790 | `	return PH7_OK;` |
|      412 | 1791 | `}` |
|     1448 | 1792 | `static int vm_builtin_InternalIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 1793 | `{` |
|     1453 | 1794 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|        - | 1795 | `	const PH7_NativeIterVtab *pVtab;` |
|      724 | 1796 | `	SXUNUSED(nArg);` |
|      724 | 1797 | `	SXUNUSED(apArg);` |
|     1453 | 1798 | `	if( pThis && NativeIterRefused(pCtx,pThis) ){` |
|      ! 0 | 1799 | `		return PH7_OK;` |
|        - | 1800 | `	}` |
|     1453 | 1801 | `	if( pThis == 0 \|\| PH7_NativeAttrTruthy(pThis,PH7_NATIVE_IT_DONE) ){` |
|      ! 0 | 1802 | `		return PH7_OK;` |
|        - | 1803 | `	}` |
|     1453 | 1804 | `	pVtab = NativeIterVtab(pThis);` |
|     1453 | 1805 | `	if( pVtab == 0 \|\| pVtab->xNext == 0 ){` |
|      ! 0 | 1806 | `		PH7_NativeSetAttrBool(pCtx->pVm,pThis,PH7_NATIVE_IT_DONE,1);` |
|      ! 0 | 1807 | `		return PH7_OK;` |
|        - | 1808 | `	}` |
|     1453 | 1809 | `	pVtab->xNext(pCtx->pVm,pThis);` |
|     1453 | 1810 | `	NativeIterPublish(pCtx->pVm,pThis);` |
|     1453 | 1811 | `	return PH7_OK;` |
|      729 | 1812 | `}` |
|     2240 | 1813 | `static int vm_builtin_InternalIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 1814 | `{` |
|     2245 | 1815 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     1120 | 1816 | `	SXUNUSED(nArg);` |
|     1120 | 1817 | `	SXUNUSED(apArg);` |
|     2245 | 1818 | `	if( pThis && NativeIterRefused(pCtx,pThis) ){` |
|      ! 0 | 1819 | `		return PH7_OK;` |
|        - | 1820 | `	}` |
|     2245 | 1821 | `	if( pThis ){` |
|     2245 | 1822 | `		NativeIterPublish(pCtx->pVm,pThis);` |
|     1120 | 1823 | `	}` |
|     2245 | 1824 | `	ph7_result_bool(pCtx,pThis != 0 && !PH7_NativeAttrTruthy(pThis,PH7_NATIVE_IT_DONE));` |
|     2245 | 1825 | `	return PH7_OK;` |
|     1125 | 1826 | `}` |
|        - | 1827 | `/* current() and key() answer the slots the walk left behind -- and NULL once it` |
|        - | 1828 | ` * is over, which is what php's exhausted InternalIterator answers too. */` |
|     1736 | 1829 | `static int NativeIterReadSlot(ph7_context *pCtx,const char *zSlot)` |
|        5 | 1830 | `{` |
|     1741 | 1831 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|        - | 1832 | `	ph7_value *pVal;` |
|     1741 | 1833 | `	if( pThis && NativeIterRefused(pCtx,pThis) ){` |
|      ! 0 | 1834 | `		return PH7_OK;` |
|        - | 1835 | `	}` |
|     1741 | 1836 | `	if( pThis ){` |
|     1741 | 1837 | `		NativeIterPublish(pCtx->pVm,pThis);` |
|      868 | 1838 | `	}` |
|     1741 | 1839 | `	if( pThis == 0 \|\| PH7_NativeAttrTruthy(pThis,PH7_NATIVE_IT_DONE) ){` |
|      ! 0 | 1840 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1841 | `		return PH7_OK;` |
|        - | 1842 | `	}` |
|     1741 | 1843 | `	pVal = PH7_NativeAttr(pThis,zSlot);` |
|     1741 | 1844 | `	if( pVal ){` |
|     1741 | 1845 | `		ph7_result_value(pCtx,pVal);` |
|      868 | 1846 | `	}` |
|     1741 | 1847 | `	return PH7_OK;` |
|      873 | 1848 | `}` |
|     1444 | 1849 | `static int vm_builtin_InternalIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 1850 | `{` |
|      722 | 1851 | `	SXUNUSED(nArg);` |
|      722 | 1852 | `	SXUNUSED(apArg);` |
|     1449 | 1853 | `	return NativeIterReadSlot(pCtx,PH7_NATIVE_IT_CUR);` |
|        5 | 1854 | `}` |
|      292 | 1855 | `static int vm_builtin_InternalIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        5 | 1856 | `{` |
|      146 | 1857 | `	SXUNUSED(nArg);` |
|      146 | 1858 | `	SXUNUSED(apArg);` |
|      297 | 1859 | `	return NativeIterReadSlot(pCtx,PH7_NATIVE_IT_KEY);` |
|        5 | 1860 | `}` |
|        - | 1861 | `/* InternalIterator::__construct() — private in php, and never reached from PHP:` |
|        - | 1862 | ` * PH7_NativeIteratorNew builds the instance directly. */` |
|      ! 0 | 1863 | `static int vm_builtin_InternalIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 1864 | `{` |
|      ! 0 | 1865 | `	SXUNUSED(nArg);` |
|      ! 0 | 1866 | `	SXUNUSED(apArg);` |
|      ! 0 | 1867 | `	SXUNUSED(pCtx);` |
|      ! 0 | 1868 | `	return PH7_OK;` |
|      ! 0 | 1869 | `}` |
|        - | 1870 | `/*` |
|        - | 1871 | ` * The iterator a native getIterator() answers: bound to its aggregate and already` |
|        - | 1872 | ` * positioned, because php's is valid() before the first rewind().` |
|        - | 1873 | ` */` |
|      846 | 1874 | `PH7_PRIVATE ph7_class_instance * PH7_NativeIteratorNew(ph7_vm *pVm,ph7_class_instance *pSrc)` |
|        5 | 1875 | `{` |
|      851 | 1876 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),"InternalIterator",` |
|        - | 1877 | `		sizeof("InternalIterator")-1,FALSE,0);` |
|        - | 1878 | `	ph7_class_instance *pIt;` |
|        - | 1879 | `	const PH7_NativeIterVtab *pVtab;` |
|      851 | 1880 | `	if( pClass == 0 \|\| pSrc == 0 ){` |
|      ! 0 | 1881 | `		return 0;` |
|        - | 1882 | `	}` |
|      851 | 1883 | `	pIt = PH7_NewClassInstance(&(*pVm),pClass);` |
|      851 | 1884 | `	if( pIt == 0 ){` |
|      ! 0 | 1885 | `		return 0;` |
|        - | 1886 | `	}` |
|      851 | 1887 | `	PH7_NativeSetAttrObj(&(*pVm),pIt,PH7_NATIVE_IT_SRC,pSrc);` |
|      851 | 1888 | `	PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|      851 | 1889 | `	pVtab = NativeIterVtab(pIt);` |
|      851 | 1890 | `	if( pVtab && pVtab->xRewind ){` |
|      851 | 1891 | `		pVtab->xRewind(&(*pVm),pIt);` |
|      423 | 1892 | `	}` |
|      851 | 1893 | `	return pIt;` |
|      428 | 1894 | `}` |
|     8445 | 1895 | `PH7_PRIVATE sxi32 PH7_VmInstallNativeIterator(ph7_vm *pVm)` |
|        5 | 1896 | `{` |
|        - | 1897 | `	static const PH7_NativePropDef aProp[] = {` |
|        - | 1898 | `		{ PH7_NATIVE_IT_SRC,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|        - | 1899 | `		{ PH7_NATIVE_IT_CUR,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|        - | 1900 | `		{ PH7_NATIVE_IT_KEY,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|        - | 1901 | `		{ PH7_NATIVE_IT_POS,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,  0, 0, 0.0 }, 0 },` |
|        - | 1902 | `		{ PH7_NATIVE_IT_AUX,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,  0, 0, 0.0 }, 0 },` |
|        - | 1903 | `		{ PH7_NATIVE_IT_DONE, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_BOOL, 1, 0, 0.0 }, 0 },` |
|        - | 1904 | `	};` |
|        - | 1905 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|        - | 1906 | `		{ "__construct", PH7_MOD_PRIVATE, "", "", vm_builtin_InternalIterator_construct },` |
|        - | 1907 | `		{ "current",     PH7_MOD_PUBLIC, "", "mixed", vm_builtin_InternalIterator_current },` |
|        - | 1908 | `		{ "key",         PH7_MOD_PUBLIC, "", "mixed", vm_builtin_InternalIterator_key },` |
|        - | 1909 | `		{ "next",        PH7_MOD_PUBLIC, "", "void",  vm_builtin_InternalIterator_next },` |
|        - | 1910 | `		{ "valid",       PH7_MOD_PUBLIC, "", "bool",  vm_builtin_InternalIterator_valid },` |
|        - | 1911 | `		{ "rewind",      PH7_MOD_PUBLIC, "", "void",  vm_builtin_InternalIterator_rewind },` |
|        - | 1912 | `	};` |
|        - | 1913 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|        - | 1914 | `		{ "InternalIterator", 0, 0, PH7_CLASS_FINAL,` |
|        - | 1915 | `		  aMethod, SX_ARRAYSIZE(aMethod), 0, 0, aProp, SX_ARRAYSIZE(aProp), 0, 0, 0 },` |
|        - | 1916 | `	};` |
|        - | 1917 | `	ph7_class *pIt,*pIterator;` |
|        - | 1918 | `	sxi32 rc;` |
|     8450 | 1919 | `	rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     8450 | 1920 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1921 | `		return rc;` |
|        - | 1922 | `	}` |
|        - | 1923 | ``	/* `implements Iterator` only NOW: PH7_ClassImplement stubs every interface`` |
|        - | 1924 | `	 * method the class does not already declare as ABSTRACT, so attaching it` |
|        - | 1925 | `	 * through the spec would have made InternalIterator uninstantiable. */` |
|     8450 | 1926 | `	pIt = NativeLookupClass(&(*pVm),"InternalIterator");` |
|     8450 | 1927 | `	pIterator = NativeLookupClass(&(*pVm),"Iterator");` |
|     8450 | 1928 | `	if( pIt == 0 \|\| pIterator == 0 ){` |
|      ! 0 | 1929 | `		return SXERR_NOTFOUND;` |
|        - | 1930 | `	}` |
|     8450 | 1931 | `	return PH7_ClassImplement(pIt,pIterator);` |
|     4222 | 1932 | `}` |
|        - | 1933 |  |
