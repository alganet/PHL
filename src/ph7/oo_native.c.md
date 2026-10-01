# src/ph7/oo_native.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 956/1061 lines (90.10%)

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
|  9956158 |   36 | `PH7_PRIVATE void PH7_NativeLiteralValue(ph7_vm *pVm,const void *pLiteral,ph7_value *pOut)` |
|        5 |   37 | `{` |
|  9956163 |   38 | `	const PH7_NativeConstDef *pLit = (const PH7_NativeConstDef *)pLiteral;` |
|        - |   39 | `	/* Every PH7_MemObjInitFrom* below SyZeros the whole value, nIdx included — and` |
|        - |   40 | `	 * a CONSTANT's or STATIC's slot is addressed by that index afterwards` |
|        - |   41 | ``	 * (`pAttr->nIdx = pMemObj->nIdx`). Losing it pointed every native class`` |
|        - |   42 | `	 * constant at slot 0, which reads as $GLOBALS. The instance-property path was` |
|        - |   43 | `	 * unaffected (it records the index before calling this), which is why nothing` |
|        - |   44 | `	 * saw it until the date family declared the first native constants. */` |
|  9956163 |   45 | `	sxu32 nSlot = pOut->nIdx;` |
|  9956163 |   46 | `	switch( pLit->iType ){` |
|  1255232 |   47 | `		case PH7_NATIVE_VAL_INT:` |
|  2510369 |   48 | `			PH7_MemObjInitFromInt(&(*pVm),pOut,pLit->iValue);` |
|  2510369 |   49 | `			break;` |
|     1856 |   50 | `		case PH7_NATIVE_VAL_BOOL:` |
|     3714 |   51 | `			PH7_MemObjInitFromBool(&(*pVm),pOut,(sxi32)pLit->iValue);` |
|     3714 |   52 | `			break;` |
|  2205967 |   53 | `		case PH7_NATIVE_VAL_STRING: {` |
|        - |   54 | `			SyString sLit;` |
|  4411740 |   55 | `			SyStringInitFromBuf(&sLit,pLit->zValue,SyStrlen(pLit->zValue));` |
|  4411740 |   56 | `			PH7_MemObjInitFromString(&(*pVm),pOut,&sLit);` |
|  4411740 |   57 | `			break;` |
|        - |   58 | `		}` |
|        - |   59 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      230 |   60 | `		case PH7_NATIVE_VAL_DOUBLE:` |
|      463 |   61 | `			PH7_MemObjInitFromReal(&(*pVm),pOut,pLit->rValue);` |
|      463 |   62 | `			break;` |
|        - |   63 | `#endif` |
|   732840 |   64 | `		case PH7_NATIVE_VAL_ARRAY: {` |
|        - |   65 | ``			/* The empty array — php's `private array $trace = [];`. Each instance`` |
|        - |   66 | `			 * needs its OWN map (the exception's trace is written per throw), so` |
|        - |   67 | `			 * this allocates rather than sharing one. */` |
|  1465630 |   68 | `			ph7_hashmap *pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|  1465630 |   69 | `			if( pMap == 0 ){` |
|      ! 0 |   70 | `				PH7_MemObjInit(&(*pVm),pOut);` |
|      ! 0 |   71 | `			}else{` |
|  1465630 |   72 | `				PH7_MemObjInitFromArray(&(*pVm),pOut,pMap);` |
|        - |   73 | `			}` |
|  1465630 |   74 | `			break;` |
|        - |   75 | `		}` |
|   782535 |   76 | `		default:` |
|  1564270 |   77 | `			PH7_MemObjInit(&(*pVm),pOut);` |
|  1564265 |   78 | `			break;` |
|        - |   79 | `	}` |
|  9956163 |   80 | `	pOut->nIdx = nSlot;` |
|  9956163 |   81 | `}` |
|        - |   82 | `/*` |
|        - |   83 | ` * Write a declared property of an instance from C.` |
|        - |   84 | ` *` |
|        - |   85 | ` * Every native class that hands an OBJECT back to PHP has to fill one in, and the` |
|        - |   86 | ` * write has to go through the instance's own slot table (hAttr -> VmClassAttr ->` |
|        - |   87 | ` * pVm->aMemObj) rather than the class declaration, or the value lands nowhere.` |
|        - |   88 | ` * Silently does nothing for a name the class does not declare -- callers pass` |
|        - |   89 | ` * literals from their own spec table, so a miss is a build error, not input.` |
|        - |   90 | ` */` |
|     8427 |   91 | `PH7_PRIVATE void PH7_NativeSetProp(ph7_vm *pVm,ph7_class_instance *pObj,` |
|        - |   92 | `	const char *zProp,sxu32 nProp,ph7_value *pSrcVal)` |
|        5 |   93 | `{` |
|        - |   94 | `	SyHashEntry *pEntry;` |
|        - |   95 | `	VmClassAttr *pVmAttr;` |
|        - |   96 | `	ph7_value *pSlot;` |
|     8432 |   97 | `	PH7_NativeMaterializeLazy(&(*pVm),pObj);` |
|     8432 |   98 | `	pEntry = SyHashGet(&pObj->hAttr,(const void *)zProp,nProp);` |
|     8432 |   99 | `	if( pEntry == 0 ){` |
|      ! 0 |  100 | `		return;` |
|        - |  101 | `	}` |
|     8432 |  102 | `	pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     8432 |  103 | `	pSlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pVmAttr->nIdx);` |
|     8432 |  104 | `	if( pSlot == 0 ){` |
|      ! 0 |  105 | `		return;` |
|        - |  106 | `	}` |
|     8432 |  107 | `	PH7_MemObjStore(pSrcVal,pSlot);` |
|     8432 |  108 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|     4213 |  109 | `}` |
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
|  1880279 |  122 | `PH7_PRIVATE ph7_value * PH7_NativeAttr(ph7_class_instance *pObj,const char *zName)` |
|        5 |  123 | `{` |
|        - |  124 | `	SyString sName;` |
|  1880284 |  125 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|  1880284 |  126 | `	return PH7_ClassInstanceFetchAttr(pObj,&sName);` |
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
|    74053 |  143 | `PH7_PRIVATE sxi64 PH7_NativeAttrInt(ph7_class_instance *pObj,const char *zName)` |
|        5 |  144 | `{` |
|    74058 |  145 | `	ph7_value *pVal = PH7_NativeAttr(pObj,zName);` |
|    74058 |  146 | `	if( pVal && (pVal->iFlags & MEMOBJ_INT) ){` |
|    74056 |  147 | `		return pVal->x.iVal;` |
|        - |  148 | `	}` |
|        3 |  149 | `	return 0;` |
|    37034 |  150 | `}` |
|        - |  151 | `/* Borrow a string slot's bytes (empty when it holds anything else). */` |
|    56326 |  152 | `PH7_PRIVATE void PH7_NativeAttrStr(ph7_class_instance *pObj,const char *zName,` |
|        - |  153 | `	const char **pzOut,int *pnOut)` |
|        5 |  154 | `{` |
|    56331 |  155 | `	ph7_value *pVal = PH7_NativeAttr(pObj,zName);` |
|    56331 |  156 | `	*pzOut = "";` |
|    56331 |  157 | `	*pnOut = 0;` |
|    56331 |  158 | `	if( pVal && (pVal->iFlags & MEMOBJ_STRING) ){` |
|    56057 |  159 | `		*pzOut = (const char *)SyBlobData(&pVal->sBlob);` |
|    56057 |  160 | `		*pnOut = (int)SyBlobLength(&pVal->sBlob);` |
|    28008 |  161 | `	}` |
|    56331 |  162 | `}` |
|        - |  163 | `/* The object stored in a slot, or NULL when it holds anything else. */` |
|    39460 |  164 | `PH7_PRIVATE ph7_class_instance * PH7_NativeAttrObj(ph7_class_instance *pObj,const char *zName)` |
|        5 |  165 | `{` |
|    39465 |  166 | `	ph7_value *pVal = PH7_NativeAttr(pObj,zName);` |
|    39465 |  167 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     6721 |  168 | `		return 0;` |
|        - |  169 | `	}` |
|    32749 |  170 | `	return (ph7_class_instance *)pVal->x.pOther;` |
|    19733 |  171 | `}` |
|        - |  172 | `/* Truth of a bool/int slot, again without converting it. */` |
|     3332 |  173 | `PH7_PRIVATE int PH7_NativeAttrTruthy(ph7_class_instance *pObj,const char *zName)` |
|        5 |  174 | `{` |
|     3337 |  175 | `	ph7_value *pVal = PH7_NativeAttr(pObj,zName);` |
|     3337 |  176 | `	if( pVal && (pVal->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|     3337 |  177 | `		return pVal->x.iVal != 0;` |
|        - |  178 | `	}` |
|      ! 0 |  179 | `	return 0;` |
|     1671 |  180 | `}` |
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
|  1586259 |  193 | `static void NativeAttrMarkInit(ph7_class_instance *pObj,const char *zName)` |
|        5 |  194 | `{` |
|  1586264 |  195 | `	SyHashEntry *pEntry = SyHashGet(&pObj->hAttr,(const void *)zName,SyStrlen(zName));` |
|  1586264 |  196 | `	if( pEntry ){` |
|  1586264 |  197 | `		((VmClassAttr *)pEntry->pUserData)->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|   793096 |  198 | `	}` |
|  1586264 |  199 | `}` |
|        - |  200 | `/*` |
|        - |  201 | ` * The slot fetch on the WRITE side. A class whose php-visible properties are LAZY` |
|        - |  202 | ` * has none of them on the object until a C body fills one, and that first write is` |
|        - |  203 | ` * what installs the set -- which is php's constructor writing its struct into the` |
|        - |  204 | ` * property table. Every native writer goes through here so the bookkeeping lives` |
|        - |  205 | ` * with the write rather than with one of the ways of writing.` |
|        - |  206 | ` */` |
|  1586259 |  207 | `static ph7_value * NativeAttrForWrite(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName)` |
|        5 |  208 | `{` |
|        - |  209 | `	ph7_value *pSlot;` |
|  1586264 |  210 | `	PH7_NativeMaterializeLazy(&(*pVm),pObj);` |
|  1586264 |  211 | `	pSlot = PH7_NativeAttr(pObj,zName);` |
|  1586264 |  212 | `	if( pSlot == 0 && pObj ){` |
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
|  1586264 |  229 | `	return pSlot;` |
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
|    45834 |  247 | `PH7_PRIVATE void PH7_NativeSetAttrInt(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,sxi64 iVal)` |
|        5 |  248 | `{` |
|    45839 |  249 | `	ph7_value *pSlot = NativeAttrForWrite(&(*pVm),pObj,zName);` |
|        - |  250 | `	ph7_value sVal;` |
|    45839 |  251 | `	if( pSlot == 0 ){` |
|      ! 0 |  252 | `		return;` |
|        - |  253 | `	}` |
|    45839 |  254 | `	PH7_MemObjInitFromInt(&(*pVm),&sVal,iVal);` |
|    45839 |  255 | `	PH7_MemObjStore(&sVal,pSlot);` |
|    45839 |  256 | `	PH7_MemObjRelease(&sVal);` |
|    45839 |  257 | `	NativeAttrMarkInit(pObj,zName);` |
|    22923 |  258 | `}` |
|        - |  259 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      462 |  260 | `PH7_PRIVATE void PH7_NativeSetAttrReal(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,ph7_real rVal)` |
|        3 |  261 | `{` |
|      465 |  262 | `	ph7_value *pSlot = NativeAttrForWrite(&(*pVm),pObj,zName);` |
|        - |  263 | `	ph7_value sVal;` |
|      465 |  264 | `	if( pSlot == 0 ){` |
|      ! 0 |  265 | `		return;` |
|        - |  266 | `	}` |
|      465 |  267 | `	PH7_MemObjInitFromReal(&(*pVm),&sVal,rVal);` |
|      465 |  268 | `	PH7_MemObjStore(&sVal,pSlot);` |
|      465 |  269 | `	PH7_MemObjRelease(&sVal);` |
|      465 |  270 | `	NativeAttrMarkInit(pObj,zName);` |
|      234 |  271 | `}` |
|        - |  272 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|  1525320 |  273 | `PH7_PRIVATE void PH7_NativeSetAttrStr(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,` |
|        - |  274 | `	const char *zVal,int nVal)` |
|        5 |  275 | `{` |
|  1525325 |  276 | `	ph7_value *pSlot = NativeAttrForWrite(&(*pVm),pObj,zName);` |
|        - |  277 | `	ph7_value sVal;` |
|        - |  278 | `	SyString sStr;` |
|  1525325 |  279 | `	if( pSlot == 0 ){` |
|      ! 0 |  280 | `		return;` |
|        - |  281 | `	}` |
|  1525325 |  282 | `	SyStringInitFromBuf(&sStr,zVal,nVal);` |
|  1525325 |  283 | `	PH7_MemObjInitFromString(&(*pVm),&sVal,&sStr);` |
|  1525325 |  284 | `	PH7_MemObjStore(&sVal,pSlot);` |
|  1525325 |  285 | `	PH7_MemObjRelease(&sVal);	NativeAttrMarkInit(pObj,zName);` |
|   762632 |  286 | `}` |
|     3955 |  287 | `PH7_PRIVATE void PH7_NativeSetAttrBool(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,int bVal)` |
|        5 |  288 | `{` |
|     3960 |  289 | `	ph7_value *pSlot = NativeAttrForWrite(&(*pVm),pObj,zName);` |
|        - |  290 | `	ph7_value sVal;` |
|     3960 |  291 | `	if( pSlot == 0 ){` |
|      ! 0 |  292 | `		return;` |
|        - |  293 | `	}` |
|     3960 |  294 | `	PH7_MemObjInitFromBool(&(*pVm),&sVal,bVal);` |
|     3960 |  295 | `	PH7_MemObjStore(&sVal,pSlot);` |
|     3960 |  296 | `	PH7_MemObjRelease(&sVal);	NativeAttrMarkInit(pObj,zName);` |
|     1981 |  297 | `}` |
|        - |  298 | `/* Store an object in a slot, or NULL to clear it. PH7_MemObjStore takes the` |
|        - |  299 | ` * reference the slot needs, so the temp never holds one of its own. */` |
|    10688 |  300 | `PH7_PRIVATE void PH7_NativeSetAttrObj(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,` |
|        - |  301 | `	ph7_class_instance *pVal)` |
|        5 |  302 | `{` |
|    10693 |  303 | `	ph7_value *pSlot = NativeAttrForWrite(&(*pVm),pObj,zName);` |
|        - |  304 | `	ph7_value sVal;` |
|    10693 |  305 | `	if( pSlot == 0 ){` |
|      ! 0 |  306 | `		return;` |
|        - |  307 | `	}` |
|    10693 |  308 | `	PH7_MemObjInit(&(*pVm),&sVal);` |
|    10693 |  309 | `	if( pVal ){` |
|    10039 |  310 | `		sVal.x.pOther = pVal;` |
|    10039 |  311 | `		sVal.iFlags = MEMOBJ_OBJ;` |
|     5017 |  312 | `	}` |
|    10693 |  313 | `	PH7_MemObjStore(&sVal,pSlot);` |
|    10693 |  314 | `	NativeAttrMarkInit(pObj,zName);` |
|     5349 |  315 | `}` |
|        - |  316 | `/*` |
|        - |  317 | ` * Hand an instance back as a native call's result, dropping the reference` |
|        - |  318 | ` * PH7_NewClassInstance/PH7_CloneClassInstance handed the caller.` |
|        - |  319 | ` */` |
|     5660 |  320 | `PH7_PRIVATE void PH7_NativeResultObject(ph7_context *pCtx,ph7_class_instance *pObj)` |
|        5 |  321 | `{` |
|        - |  322 | `	ph7_value sRes;` |
|     5665 |  323 | `	PH7_MemObjInit(pCtx->pVm,&sRes);` |
|     5665 |  324 | `	sRes.x.pOther = pObj;` |
|     5665 |  325 | `	sRes.iFlags = MEMOBJ_OBJ;` |
|     5665 |  326 | `	ph7_result_value(pCtx,&sRes);   /* takes its own reference */` |
|     5665 |  327 | `	PH7_ClassInstanceUnref(pObj);` |
|     5665 |  328 | `}` |
|        - |  329 | `/*` |
|        - |  330 | ` * Resolve a class by name for the builder's own use (parents and interfaces).` |
|        - |  331 | ` * Autoload is deliberately NOT triggered: these run at VM init, where the only` |
|        - |  332 | ` * classes that can exist are the ones installed before this call, and a missing` |
|        - |  333 | ` * name is a build-order bug in the engine, not a userland lookup.` |
|        - |  334 | ` */` |
|  1303874 |  335 | `static ph7_class * NativeLookupClass(ph7_vm *pVm,const char *zName)` |
|        5 |  336 | `{` |
|  1303879 |  337 | `	return PH7_VmExtractClass(&(*pVm),zName,(sxu32)SyStrlen(zName),FALSE,0);` |
|        5 |  338 | `}` |
|        - |  339 | `/*` |
|        - |  340 | ` * Translate the builder's PH7_MOD_* modifier bits into the protection level and` |
|        - |  341 | ` * the attribute/method flag word the class structures actually store.` |
|        - |  342 | ` */` |
| 14524337 |  343 | `static sxi32 NativeProtection(sxi32 iMods)` |
|        5 |  344 | `{` |
| 14524342 |  345 | `	if( iMods & PH7_MOD_PRIVATE ){` |
|  1075365 |  346 | `		return PH7_CLASS_PROT_PRIVATE;` |
|        - |  347 | `	}` |
| 13448982 |  348 | `	if( iMods & PH7_MOD_PROTECTED ){` |
|   215077 |  349 | `		return PH7_CLASS_PROT_PROTECTED;` |
|        - |  350 | `	}` |
| 13233910 |  351 | `	return PH7_CLASS_PROT_PUBLIC;` |
|  7252449 |  352 | `}` |
|        - |  353 | `/*` |
|        - |  354 | ` * Attach one C-bodied method to an already-created class.` |
|        - |  355 | ` *` |
|        - |  356 | ` * The ph7_user_func built here is NOT registered in pVm->hHostFunction: it hangs` |
|        - |  357 | ` * off the method and is reachable only by dispatching the method, which is exactly` |
|        - |  358 | ` * the property that retires the global thunks. Its sName is the php-facing` |
|        - |  359 | `` * `Class::method`, so an ArgumentCountError raised at the OP_CALL choke point`` |
|        - |  360 | ` * names what a php user would recognise.` |
|        - |  361 | ` */` |
|  9429819 |  362 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallMethod(` |
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
|  9429824 |  376 | `	SyStringInitFromBuf(&sName,pDef->zName,SyStrlen(pDef->zName));` |
|  9429824 |  377 | `	iFuncFlags = VM_FUNC_NATIVE;` |
|  9429824 |  378 | `	if( pVm->bCompilingBuiltin ){` |
|        - |  379 | `		/* Same stamp the embedded chunks got, so Reflection keeps reporting these` |
|        - |  380 | `		 * as internal: isInternal() true, getFileName() false. */` |
|  9429568 |  381 | `		iFuncFlags \|= VM_FUNC_INTERNAL;` |
|  4708468 |  382 | `	}` |
| 14138420 |  383 | `	pMeth = PH7_NewClassMethod(&(*pVm),pClass,&sName,0,` |
|  9429819 |  384 | `		NativeProtection(pDef->iMods),` |
|  9429819 |  385 | `		((pDef->iMods & PH7_MOD_FINAL) ? PH7_CLASS_ATTR_FINAL : 0)` |
|  9429819 |  386 | `		\| ((pDef->iMods & PH7_MOD_ABSTRACT) ? PH7_CLASS_ATTR_ABSTRACT : 0),` |
|  4708596 |  387 | `		iFuncFlags);` |
|  9429824 |  388 | `	if( pMeth == 0 ){` |
|      ! 0 |  389 | `		return SXERR_MEM;` |
|        - |  390 | `	}` |
|        - |  391 | `	/* Staticness has to be recorded in BOTH places: on the method (where the class` |
|        - |  392 | `	 * machinery and Reflection read it) and on the function (where the OP_CALL` |
|        - |  393 | `	 * dispatcher reads it, to keep the caller's $this from leaking in as a receiver` |
|        - |  394 | `	 * — see VM_FUNC_NATIVE_STATIC). */` |
|  9429824 |  395 | `	if( pDef->iMods & PH7_MOD_STATIC ){` |
|   484173 |  396 | `		pMeth->iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|   484173 |  397 | `		pMeth->sFunc.iFlags \|= VM_FUNC_NATIVE_STATIC;` |
|   241760 |  398 | `	}` |
|        - |  399 | `	/* An ABSTRACT method has no body to install — an INTERFACE's methods are the` |
|        - |  400 | ``	 * reason the builder needs the case at all (`SeekableIterator::seek()`), and`` |
|        - |  401 | `	 * php reports them with their declared signature like any other. The dispatch` |
|        - |  402 | `	 * never reaches one: OP_CALL and OP_MEMBER both refuse an abstract method` |
|        - |  403 | `	 * ("Cannot call abstract method C::m()") before they look at a body. */` |
|  9429824 |  404 | `	if( pDef->iMods & PH7_MOD_ABSTRACT ){` |
|   409986 |  405 | `		if( pDef->zSig ){` |
|   409986 |  406 | `			rc = PH7_NewForeignFunction(&(*pVm),&sName,pDef->xFunc,pUserData,&pNative);` |
|   409986 |  407 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  408 | `				return rc;` |
|        - |  409 | `			}` |
|   409986 |  410 | `			pNative->zSig = pDef->zSig;` |
|   409986 |  411 | `			if( pDef->zRet && pDef->zRet[0] ){` |
|   389823 |  412 | `				pNative->zRet = pDef->zRet;` |
|   194648 |  413 | `			}` |
|   409986 |  414 | `			pMeth->sFunc.pNative = pNative;` |
|   204716 |  415 | `		}` |
|   409986 |  416 | `		return PH7_ClassInstallMethod(pClass,pMeth);` |
|        - |  417 | `	}` |
|        - |  418 | `	/* The C body. The diagnostic name is the qualified one php would print. */` |
|  9019843 |  419 | `	SyBufferFormat(zQual,sizeof(zQual),"%z::%s",&pClass->sName,pDef->zName);` |
|  9019843 |  420 | `	SyStringInitFromBuf(&sVmName,zQual,SyStrlen(zQual));` |
|  9019843 |  421 | `	rc = PH7_NewForeignFunction(&(*pVm),&sVmName,pDef->xFunc,pUserData,&pNative);` |
|  9019843 |  422 | `	if( rc != SXRET_OK ){` |
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
|  9019843 |  438 | `	if( pDef->zSig ){` |
|  9013122 |  439 | `		sxi16 nMin = 0, nMax = 0;` |
|  9013122 |  440 | `		sxu8 bAtLeast = 0, bHasMax = 0;` |
|  9013122 |  441 | `		pNative->zSig = pDef->zSig;` |
|  9013122 |  442 | `		pNative->nByRefMask = VmDeriveByRefMaskFromSig(pDef->zSig);` |
|  9013122 |  443 | `		VmDeriveArityFromSig(pDef->zSig,&nMin,&bAtLeast,&nMax,&bHasMax);` |
|  9013122 |  444 | `		pNative->nMinArg = nMin;` |
|  9013122 |  445 | `		pNative->bAtLeast = bAtLeast;` |
|  9013122 |  446 | `		pNative->nMaxArg = nMax;` |
|  9013122 |  447 | `		pNative->bHasMaxArg = bHasMax;` |
|  4500524 |  448 | `	}` |
|  9019843 |  449 | `	if( pDef->zRet && pDef->zRet[0] ){` |
|  8152834 |  450 | `		pNative->zRet = pDef->zRet;` |
|  4070956 |  451 | `	}` |
|  9019843 |  452 | `	pMeth->sFunc.pNative = pNative;` |
|  9019843 |  453 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  9019843 |  454 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  455 | `		return rc;` |
|        - |  456 | `	}` |
|  9019843 |  457 | `	if( pClass->bMounted ){` |
|        - |  458 | `		/* The class is already live (a method attached after installation): mount` |
|        - |  459 | `		 * this one method now, since VmMountUserClass will not run again. */` |
|      ! 0 |  460 | `		return PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|        - |  461 | `	}` |
|  9019843 |  462 | `	return SXRET_OK;` |
|  4708601 |  463 | `}` |
|        - |  464 | `/*` |
|        - |  465 | ` * Install one class constant carrying a scalar value.` |
|        - |  466 | ` *` |
|        - |  467 | ` * A declared default is normally a compiled initializer (ph7_class_attr::aByteCode)` |
|        - |  468 | ` * evaluated at mount. The builder has no compiler to hand, so it evaluates the` |
|        - |  469 | ` * value NOW into the constant's reserved slot and leaves the byte-code empty --` |
|        - |  470 | ` * which is what a literal initializer would have produced anyway.` |
|        - |  471 | ` */` |
|  2574143 |  472 | `static sxi32 NativeInstallConstant(ph7_vm *pVm,ph7_class *pClass,const PH7_NativeConstDef *pDef)` |
|        5 |  473 | `{` |
|        - |  474 | `	ph7_class_attr *pAttr;` |
|        - |  475 | `	SyString sName;` |
|  2574148 |  476 | `	SyStringInitFromBuf(&sName,pDef->zName,SyStrlen(pDef->zName));` |
|  2574148 |  477 | `	pAttr = PH7_NewClassAttr(&(*pVm),&sName,0,NativeProtection(pDef->iMods),` |
|        - |  478 | `		PH7_CLASS_ATTR_CONSTANT);` |
|  2574148 |  479 | `	if( pAttr == 0 ){` |
|      ! 0 |  480 | `		return SXERR_MEM;` |
|        - |  481 | `	}` |
|  2574148 |  482 | `	pAttr->pDeclClass = pClass;` |
|        - |  483 | `	/* Stash the literal; VmMountUserClassAttrs materializes it into the slot. */` |
|  2574148 |  484 | `	pAttr->pNativeValue = pDef;` |
|  2574148 |  485 | `	return PH7_ClassInstallAttr(pClass,pAttr);` |
|  1285353 |  486 | `}` |
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
|  1095523 |  499 | `static void NativeAttrType(ph7_class_attr *pAttr,const char *zType)` |
|        5 |  500 | `{` |
|        - |  501 | `	static const struct { const char *zName; sxu32 nType; } aScalar[] = {` |
|        - |  502 | `		{ "int",    MEMOBJ_INT },` |
|        - |  503 | `		{ "float",  MEMOBJ_REAL },` |
|        - |  504 | `		{ "string", MEMOBJ_STRING },` |
|        - |  505 | `		{ "bool",   MEMOBJ_BOOL },` |
|        - |  506 | `		{ "array",  MEMOBJ_HASHMAP },` |
|        - |  507 | `		{ "object", MEMOBJ_OBJ },` |
|        - |  508 | `	};` |
|  1095528 |  509 | `	const char *zAtom = zType;` |
|        - |  510 | `	sxu32 nAtom, n;` |
|  1095528 |  511 | `	if( zAtom[0] == '?' ){` |
|   369660 |  512 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|   369660 |  513 | `		zAtom++;` |
|   184580 |  514 | `	}` |
|  1095528 |  515 | `	nAtom = SyStrlen(zAtom);` |
|  1095528 |  516 | `	pAttr->iFlags \|= PH7_CLASS_ATTR_TYPED;` |
|  1095528 |  517 | `	SyStringInitFromBuf(&pAttr->sTypeName,zType,SyStrlen(zType));` |
|  4180467 |  518 | `	for( n = 0 ; n < SX_ARRAYSIZE(aScalar) ; ++n ){` |
|  3904901 |  519 | `		if( nAtom == SyStrlen(aScalar[n].zName)` |
|  2407481 |  520 | `		 && SyMemcmp(zAtom,aScalar[n].zName,(sxu32)nAtom) == 0 ){` |
|   819967 |  521 | `			pAttr->nType = aScalar[n].nType;` |
|   819967 |  522 | `			return;` |
|        - |  523 | `		}` |
|  1540409 |  524 | `	}` |
|   275566 |  525 | `	pAttr->nType = SXU32_HIGH;` |
|   275566 |  526 | `	SyStringInitFromBuf(&pAttr->sClass,zAtom,nAtom);` |
|   547033 |  527 | `}` |
|        - |  528 | `/*` |
|        - |  529 | ` * Install one declared property.` |
|        - |  530 | ` *` |
|        - |  531 | ` * The default is carried as a literal rather than compiled byte-code (see` |
|        - |  532 | `` * PH7_NativeLiteralValue); an instance property materializes it at `new`, a static`` |
|        - |  533 | `` * one at mount. A PH7_NATIVE_VAL_NULL default is the plain `public $p;` case.`` |
|        - |  534 | ` */` |
|  2520375 |  535 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallProperty(ph7_vm *pVm,ph7_class *pClass,` |
|        - |  536 | `	const PH7_NativePropDef *pDef)` |
|        5 |  537 | `{` |
|        - |  538 | `	ph7_class_attr *pAttr;` |
|        - |  539 | `	SyString sName;` |
|  2520380 |  540 | `	sxi32 iFlags = 0;` |
|  2520380 |  541 | `	SyStringInitFromBuf(&sName,pDef->zName,SyStrlen(pDef->zName));` |
|  2520380 |  542 | `	if( pDef->iMods & PH7_MOD_STATIC ){` |
|      ! 0 |  543 | `		iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|      ! 0 |  544 | `	}` |
|  2520380 |  545 | `	if( pDef->iMods & PH7_MOD_HIDDEN ){` |
|  1290437 |  546 | `		iFlags \|= PH7_CLASS_ATTR_HIDDEN;` |
|   644352 |  547 | `	}` |
|  2520380 |  548 | `	if( pDef->iMods & PH7_MOD_ONDEMAND ){` |
|     6726 |  549 | `		iFlags \|= PH7_CLASS_ATTR_NATIVE_ONDEMAND;` |
|     3356 |  550 | `	}` |
|        - |  551 | `	/* php's VIRTUAL property: declared, and answered by the class's own handlers` |
|        - |  552 | `	 * rather than by a slot. NATIVE_VIRTUAL rides with it because that is exactly` |
|        - |  553 | `	 * what the name means to Reflection (modifiers 512) and to the object` |
|        - |  554 | `	 * comparator -- there is no real property behind it to compare. */` |
|  2520380 |  555 | `	if( pDef->iMods & PH7_MOD_VIRTUAL ){` |
|   618337 |  556 | `		iFlags \|= PH7_CLASS_ATTR_NATIVE_NOSLOT\|PH7_CLASS_ATTR_NATIVE_VIRTUAL;` |
|   308752 |  557 | `	}` |
|        - |  558 | `	/* php declares several native slots readonly and asymmetrically visible` |
|        - |  559 | ``	 * (`public protected(set) readonly string $path` on Directory), and both are`` |
|        - |  560 | `	 * php-visible twice over: the write refusal and Reflection's modifier list. */` |
|  2520380 |  561 | `	if( pDef->iMods & PH7_MOD_READONLY ){` |
|    67215 |  562 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|    33560 |  563 | `	}` |
|  2520380 |  564 | `	if( pDef->iMods & PH7_MOD_PROT_SET ){` |
|    60494 |  565 | `		iFlags \|= PH7_CLASS_ATTR_PROTECTED_SET;` |
|    30204 |  566 | `	}` |
|  2520380 |  567 | `	if( pDef->iMods & PH7_MOD_PRIV_SET ){` |
|      ! 0 |  568 | `		iFlags \|= PH7_CLASS_ATTR_PRIVATE_SET;` |
|      ! 0 |  569 | `	}` |
|  2520380 |  570 | `	pAttr = PH7_NewClassAttr(&(*pVm),&sName,0,NativeProtection(pDef->iMods),iFlags);` |
|  2520380 |  571 | `	if( pAttr == 0 ){` |
|      ! 0 |  572 | `		return SXERR_MEM;` |
|        - |  573 | `	}` |
|  2520380 |  574 | `	pAttr->pDeclClass = pClass;` |
|        - |  575 | `	/* NO default is not the same as a NULL one, and the difference is php-visible:` |
|        - |  576 | ``	 * a typed slot without a default is UNINITIALIZED at `new` (reading it before`` |
|        - |  577 | `	 * the class writes it is an Error, hasDefaultValue() is false), which is what` |
|        - |  578 | `	 * php declares for LibXMLError's six fields and every reflector's $name. The` |
|        - |  579 | ``	 * machinery is already there for a compiled `public string $p;` — leaving`` |
|        - |  580 | `	 * pNativeValue at 0 is what selects it. */` |
|  2520380 |  581 | `	if( pDef->sDefault.iType != PH7_NATIVE_VAL_NONE ){` |
|  1579440 |  582 | `		pAttr->pNativeValue = &pDef->sDefault;` |
|   788660 |  583 | `	}` |
|  2520380 |  584 | `	if( pDef->zType && pDef->zType[0] ){` |
|  1095528 |  585 | `		NativeAttrType(pAttr,pDef->zType);` |
|   547028 |  586 | `	}` |
|  2520380 |  587 | `	return PH7_ClassInstallAttr(pClass,pAttr);` |
|  1258505 |  588 | `}` |
|        - |  589 | `/*` |
|        - |  590 | `` * Attach an `#[Attr(...)]` to something declared from C.`` |
|        - |  591 | ` *` |
|        - |  592 | `` * php declares `#[Attribute(Attribute::TARGET_CLASS)]` on Attribute itself, a`` |
|        - |  593 | `` * target mask on every other attribute class, and `#[NoDiscard(message: …)]` on`` |
|        - |  594 | ` * nine DateTimeImmutable methods — and those records are LOAD-BEARING: the` |
|        - |  595 | `` * compiler reads them to decide whether a user's `#[Deprecated]` may sit where`` |
|        - |  596 | ` * it does, the NoDiscard warning reads its message from them, and` |
|        - |  597 | ` * ReflectionAttribute answers them all. A compiled attribute holds its argument` |
|        - |  598 | ` * as byte-code; there is no compiler here, so the argument rides as the same` |
|        - |  599 | ` * literal record a native constant or property default uses and every reader` |
|        - |  600 | ` * takes that branch when the byte-code is empty.` |
|        - |  601 | ` *` |
|        - |  602 | ` * NativeBuildAttr is the shared half; the two entry points below hang the record` |
|        - |  603 | ` * on a class or on one of its methods. aArg is BORROWED, so callers state their` |
|        - |  604 | `` * rows `static const`.`` |
|        - |  605 | ` */` |
|   114257 |  606 | `static sxi32 NativeBuildAttr(ph7_vm *pVm,ph7_attribute *pAttr,const char *zAttr,` |
|        - |  607 | `	const PH7_NativeAttrArg *aArg,sxu32 nArg)` |
|        5 |  608 | `{` |
|        - |  609 | `	char *zDup;` |
|        - |  610 | `	sxu32 n;` |
|   114262 |  611 | `	SyZero(pAttr,sizeof(*pAttr));` |
|   114262 |  612 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zAttr,SyStrlen(zAttr));` |
|   114262 |  613 | `	if( zDup == 0 ){` |
|      ! 0 |  614 | `		return SXERR_MEM;` |
|        - |  615 | `	}` |
|   114262 |  616 | `	SyStringInitFromBuf(&pAttr->sName,zDup,SyStrlen(zAttr));` |
|   114262 |  617 | `	SySetInit(&pAttr->aArgs,&pVm->sAllocator,sizeof(ph7_attr_arg));` |
|   228519 |  618 | `	for( n = 0 ; n < nArg ; n++ ){` |
|        - |  619 | `		ph7_attr_arg sArgRec;` |
|   114262 |  620 | `		SyZero(&sArgRec,sizeof(sArgRec));` |
|   114262 |  621 | `		SySetInit(&sArgRec.aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|   114262 |  622 | `		if( aArg[n].zName ){` |
|    60494 |  623 | `			char *zN = SyMemBackendStrDup(&pVm->sAllocator,aArg[n].zName,SyStrlen(aArg[n].zName));` |
|    60494 |  624 | `			if( zN ){` |
|    60494 |  625 | `				SyStringInitFromBuf(&sArgRec.sName,zN,SyStrlen(aArg[n].zName));` |
|    30204 |  626 | `			}` |
|    30204 |  627 | `		}` |
|        - |  628 | `		/* The literal is BORROWED, not copied: aArg must have static storage` |
|        - |  629 | ``		 * duration (every caller states its rows as `static const`). */`` |
|   114262 |  630 | `		sArgRec.pNativeValue = (const void *)&aArg[n].sValue;` |
|   114262 |  631 | `		SySetPut(&pAttr->aArgs,(const void *)&sArgRec);` |
|    57057 |  632 | `	}` |
|   114262 |  633 | `	return SXRET_OK;` |
|    57057 |  634 | `}` |
|        - |  635 | `/*` |
|        - |  636 | `` * Declare one of a native class's METHODS php 8.5's `#[\NoDiscard]`, argument`` |
|        - |  637 | ` * and all — the same record a compiled declaration carries, so Reflection` |
|        - |  638 | ` * reports the attribute and the warning reads its message from the one place a` |
|        - |  639 | ` * userland one is read from. Assigned by the owning installer after` |
|        - |  640 | ` * PH7_InstallNativeClasses, like xClone/xDim/xSet: a spec-row field would have` |
|        - |  641 | ` * to be left empty by every other table.` |
|        - |  642 | ` */` |
|    60489 |  643 | `PH7_PRIVATE sxi32 PH7_NativeMethodSetNoDiscard(ph7_vm *pVm,ph7_class *pClass,` |
|        - |  644 | `	const char *zMethod,const PH7_NativeAttrArg *aArg,sxu32 nArg)` |
|        5 |  645 | `{` |
|        - |  646 | `	ph7_class_method *pMeth;` |
|        - |  647 | `	ph7_attribute sAttr;` |
|        - |  648 | `	sxi32 rc;` |
|    60494 |  649 | `	if( pClass == 0 ){` |
|      ! 0 |  650 | `		return SXERR_NOTFOUND;` |
|        - |  651 | `	}` |
|    60494 |  652 | `	pMeth = PH7_ClassExtractMethod(pClass,zMethod,(sxu32)SyStrlen(zMethod));` |
|    60494 |  653 | `	if( pMeth == 0 ){` |
|      ! 0 |  654 | `		return SXERR_NOTFOUND;` |
|        - |  655 | `	}` |
|    60494 |  656 | `	rc = NativeBuildAttr(&(*pVm),&sAttr,"NoDiscard",aArg,nArg);` |
|    60494 |  657 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  658 | `		return rc;` |
|        - |  659 | `	}` |
|    60494 |  660 | `	pMeth->sFunc.iFlags \|= VM_FUNC_NODISCARD;` |
|    60494 |  661 | `	return SySetPut(&pMeth->sFunc.aAttrs,(const void *)&sAttr);` |
|    30209 |  662 | `}` |
|    53768 |  663 | `PH7_PRIVATE sxi32 PH7_NativeClassAddAttribute(ph7_vm *pVm,ph7_class *pClass,` |
|        - |  664 | `	const char *zAttr,const PH7_NativeAttrArg *aArg,sxu32 nArg)` |
|        5 |  665 | `{` |
|        - |  666 | `	ph7_attribute sAttr;` |
|        - |  667 | `	sxi32 rc;` |
|    53773 |  668 | `	if( pClass == 0 ){` |
|      ! 0 |  669 | `		return SXERR_NOTFOUND;` |
|        - |  670 | `	}` |
|    53773 |  671 | `	rc = NativeBuildAttr(&(*pVm),&sAttr,zAttr,aArg,nArg);` |
|    53773 |  672 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  673 | `		return rc;` |
|        - |  674 | `	}` |
|    53773 |  675 | `	return SySetPut(&pClass->aAttrs,(const void *)&sAttr);` |
|    26853 |  676 | `}` |
|        - |  677 | `/*` |
|        - |  678 | ` * php's get_properties / get_debug_info handlers: the SHAPE a class SHOWS.` |
|        - |  679 | ` *` |
|        - |  680 | ` * Several native classes present something that is not their storage. php shows a` |
|        - |  681 | ` * DateTime as date/timezone_type/timezone (the state is a timestamp, an offset and` |
|        - |  682 | ` * a zone name), a DateTimeZone as timezone_type/timezone, and a WeakReference as` |
|        - |  683 | ` * ["object"]. Those slots carry PH7_MOD_HIDDEN so no presentation surface sees the` |
|        - |  684 | ` * engine state; this fills an array with what php shows instead.` |
|        - |  685 | ` *` |
|        - |  686 | ` * Answers 1 when the class (or an ancestor) declared a hook and pOut was filled.` |
|        - |  687 | ` * bDebug distinguishes php's two handlers: 1 for var_dump/print_r (get_debug_info),` |
|        - |  688 | ` * 0 for var_export and the (array) cast (get_properties). They disagree — a` |
|        - |  689 | ` * WeakReference shows ["object"] to var_dump and nothing to (array) — so the` |
|        - |  690 | ` * callback is told which is asking rather than each caller guessing.` |
|        - |  691 | ` * get_object_vars() and foreach are NOT callers: php answers those from the real` |
|        - |  692 | ` * properties with the caller's scope applied, which for every class here is empty.` |
|        - |  693 | ` */` |
|     1588 |  694 | `PH7_PRIVATE int PH7_ClassInstancePresent(ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|        5 |  695 | `{` |
|        - |  696 | `	ph7_class *pClass;` |
|     1593 |  697 | `	if( pThis == 0 \|\| pOut == 0 ){` |
|      ! 0 |  698 | `		return 0;` |
|        - |  699 | `	}` |
|     2391 |  700 | `	for( pClass = pThis->pClass ; pClass ; pClass = pClass->pBase ){` |
|     1721 |  701 | `		if( pClass->xPresent ){` |
|      922 |  702 | `			return pClass->xPresent(pThis->pVm,pThis,pOut,bDebug) == SXRET_OK;` |
|        - |  703 | `		}` |
|      404 |  704 | `	}` |
|      675 |  705 | `	return 0;` |
|      799 |  706 | `}` |
|        - |  707 | `/*` |
|        - |  708 | ` * The nearest ph7_class::xDim in a class's base chain -- php's handler` |
|        - |  709 | ` * inheritance, so a user subclass of DOMNodeList reads dimensions the way its` |
|        - |  710 | ` * parent does.` |
|        - |  711 | ` */` |
|     1488 |  712 | `static ph7_class * NativeDimClass(ph7_class *pClass)` |
|        5 |  713 | `{` |
|     2387 |  714 | `	while( pClass ){` |
|     1707 |  715 | `		if( pClass->xDim ){` |
|      812 |  716 | `			return pClass;` |
|        - |  717 | `		}` |
|      899 |  718 | `		pClass = pClass->pBase;` |
|        5 |  719 | `	}` |
|      685 |  720 | `	return 0;` |
|      749 |  721 | `}` |
|        - |  722 | `/*` |
|        - |  723 | `` * Does `$o[$k]` mean anything for an instance of this class? The subscript`` |
|        - |  724 | `` * opcode asks BEFORE it commits to php's `Cannot use object of type C as`` |
|        - |  725 | `` * array`, which is still the answer for every class that has no hook.`` |
|        - |  726 | ` */` |
|     1026 |  727 | `PH7_PRIVATE int PH7_ClassHasNativeDim(ph7_class *pClass)` |
|        5 |  728 | `{` |
|     1031 |  729 | `	return NativeDimClass(pClass) != 0;` |
|        5 |  730 | `}` |
|        - |  731 | `/*` |
|        - |  732 | ` * Run the hook. Answers 0 when the class has none (nothing in pCtx is touched);` |
|        - |  733 | ` * 1 when it answered, which includes a REFUSAL -- the caller reads zThrowClass` |
|        - |  734 | ` * to tell the two apart.` |
|        - |  735 | ` */` |
|      370 |  736 | `PH7_PRIVATE int PH7_ClassNativeDim(ph7_class_instance *pThis,PH7_NativeDimCtx *pCtx)` |
|        4 |  737 | `{` |
|      374 |  738 | `	ph7_class *pClass = pThis ? NativeDimClass(pThis->pClass) : 0;` |
|      374 |  739 | `	if( pClass == 0 ){` |
|      ! 0 |  740 | `		return 0;` |
|        - |  741 | `	}` |
|      374 |  742 | `	pClass->xDim(pThis->pVm,pThis,pCtx);` |
|      374 |  743 | `	return 1;` |
|      189 |  744 | `}` |
|        - |  745 | `/*` |
|        - |  746 | ` * The refusal a dimension WRITE, APPEND or UNSET takes on an object. php's own` |
|        - |  747 | ` * sentence for a class that is not an ArrayAccess is` |
|        - |  748 | `` * `Cannot use object of type C as array`; a class whose read handler answers`` |
|        - |  749 | ` * something words its own (php's PDORow names the operation and the class),` |
|        - |  750 | ` * which the hook supplies through the same refusal fields a read uses.` |
|        - |  751 | ` */` |
|       28 |  752 | `PH7_PRIVATE sxu32 PH7_ClassNativeDimRefusal(ph7_class_instance *pThis,int iMode,` |
|        - |  753 | `	char *zMsg,sxu32 nMsg)` |
|        4 |  754 | `{` |
|       32 |  755 | `	ph7_class *pClass = pThis ? NativeDimClass(pThis->pClass) : 0;` |
|       32 |  756 | `	if( pClass ){` |
|        - |  757 | `		PH7_NativeDimCtx sDim;` |
|       19 |  758 | `		sDim.iMode = iMode;` |
|       19 |  759 | `		sDim.pOffset = 0;` |
|       19 |  760 | `		sDim.pResult = 0;` |
|       19 |  761 | `		sDim.zThrowClass = 0;` |
|       19 |  762 | `		sDim.zThrowMsg[0] = 0;` |
|       19 |  763 | `		sDim.bStored = 0;` |
|       19 |  764 | `		pClass->xDim(pThis->pVm,pThis,&sDim);` |
|       19 |  765 | `		if( sDim.zThrowClass ){` |
|        3 |  766 | `			return SyBufferFormat(zMsg,nMsg,"%s",sDim.zThrowMsg);` |
|        - |  767 | `		}` |
|        8 |  768 | `	}` |
|       69 |  769 | `	return SyBufferFormat(zMsg,nMsg,"Cannot use object of type %.*s as array",` |
|       26 |  770 | `		pThis ? (int)pThis->pClass->sName.nByte : 0,` |
|       26 |  771 | `		pThis ? pThis->pClass->sName.zString : "");` |
|       18 |  772 | `}` |
|        - |  773 | `/*` |
|        - |  774 | ` * Offer a dimension WRITE, APPEND or UNSET to the class's own handler, with the` |
|        - |  775 | ` * offset and the value the refusal-only form does not carry.` |
|        - |  776 | ` *` |
|        - |  777 | ` * Answers 1 when the handler took the access -- either by STORING (bStored,` |
|        - |  778 | ` * with zThrowClass still 0) or by wording its own refusal in` |
|        - |  779 | ` * zThrowClass/zThrowMsg -- and 0 when the class has no handler or its handler` |
|        - |  780 | `` * declined, which puts the access on the ordinary `Cannot use object of type C`` |
|        - |  781 | `` * as array` path. pCtx is the caller's scratch: it is initialized here and left`` |
|        - |  782 | ` * filled for the caller to read.` |
|        - |  783 | ` */` |
|       64 |  784 | `PH7_PRIVATE int PH7_ClassNativeDimStore(ph7_class_instance *pThis,int iMode,` |
|        - |  785 | `	ph7_value *pOffset,ph7_value *pValue,PH7_NativeDimCtx *pCtx)` |
|        3 |  786 | `{` |
|       67 |  787 | `	ph7_class *pClass = pThis ? NativeDimClass(pThis->pClass) : 0;` |
|       67 |  788 | `	pCtx->iMode = iMode;` |
|       67 |  789 | `	pCtx->pOffset = pOffset;` |
|       67 |  790 | `	pCtx->pResult = pValue;` |
|       67 |  791 | `	pCtx->zThrowClass = 0;` |
|       67 |  792 | `	pCtx->zThrowMsg[0] = 0;` |
|       67 |  793 | `	pCtx->bStored = 0;` |
|       67 |  794 | `	if( pClass == 0 ){` |
|        6 |  795 | `		return 0;` |
|        - |  796 | `	}` |
|       62 |  797 | `	pClass->xDim(pThis->pVm,pThis,pCtx);` |
|       62 |  798 | `	return pCtx->bStored \|\| pCtx->zThrowClass ? 1 : 0;` |
|       35 |  799 | `}` |
|        - |  800 | `/*` |
|        - |  801 | ` * The nearest ph7_class::xProp in a class's base chain -- the same handler` |
|        - |  802 | ` * inheritance xDim and xSet get, and php's own: a subclass of a class whose` |
|        - |  803 | ` * properties are not storage reads them through the parent's handler.` |
|        - |  804 | ` */` |
|    19590 |  805 | `static ph7_class * NativePropClass(ph7_class *pClass)` |
|        5 |  806 | `{` |
|    38183 |  807 | `	while( pClass ){` |
|    36813 |  808 | `		if( pClass->xProp ){` |
|    18225 |  809 | `			return pClass;` |
|        - |  810 | `		}` |
|    18593 |  811 | `		pClass = pClass->pBase;` |
|        5 |  812 | `	}` |
|     1375 |  813 | `	return 0;` |
|     9801 |  814 | `}` |
|        - |  815 | `/*` |
|        - |  816 | `` * Does `$o->p` MEAN something this class answers for itself? Asked before the`` |
|        - |  817 | ` * miss path commits to creating a property, warning about an undefined one or` |
|        - |  818 | ` * dispatching __get.` |
|        - |  819 | ` */` |
|     7889 |  820 | `PH7_PRIVATE int PH7_ClassHasNativeProp(ph7_class *pClass)` |
|        5 |  821 | `{` |
|     7894 |  822 | `	return NativePropClass(pClass) != 0;` |
|        5 |  823 | `}` |
|        - |  824 | `/*` |
|        - |  825 | ` * Run the hook. Answers 0 when the class has none, or when the hook DECLINED` |
|        - |  826 | ` * the name (bAnswered left at 0); 1 when it answered, which includes a` |
|        - |  827 | ` * REFUSAL -- the caller reads zThrowClass to tell the two apart.` |
|        - |  828 | ` */` |
|     9529 |  829 | `PH7_PRIVATE int PH7_ClassNativeProp(ph7_class_instance *pThis,PH7_NativePropCtx *pCtx)` |
|        5 |  830 | `{` |
|     9534 |  831 | `	ph7_class *pClass = pThis ? NativePropClass(pThis->pClass) : 0;` |
|     9534 |  832 | `	if( pClass == 0 ){` |
|       17 |  833 | `		return 0;` |
|        - |  834 | `	}` |
|     9518 |  835 | `	pClass->xProp(pThis->pVm,pThis,pCtx);` |
|     9518 |  836 | `	return pCtx->bAnswered \|\| pCtx->zThrowClass != 0;` |
|     4770 |  837 | `}` |
|        - |  838 | `/*` |
|        - |  839 | ` * Fill a caller-owned context and run the hook, for the callers that ask` |
|        - |  840 | ` * OUTSIDE the member opcode: Reflection's getValue()/setValue() and` |
|        - |  841 | ` * property_exists(), each of which reaches php's handlers by its own door.` |
|        - |  842 | ` */` |
|     2356 |  843 | `PH7_PRIVATE int PH7_ClassNativePropAsk(ph7_class_instance *pThis,PH7_NativePropCtx *pCtx,` |
|        - |  844 | `	int iMode,const SyString *pName,ph7_value *pResult)` |
|        4 |  845 | `{` |
|     2360 |  846 | `	pCtx->iMode = iMode;` |
|     2360 |  847 | `	pCtx->pName = pName;` |
|     2360 |  848 | `	pCtx->pResult = pResult;` |
|     2360 |  849 | `	pCtx->bAnswered = 0;` |
|     2360 |  850 | `	pCtx->zThrowClass = 0;` |
|     2360 |  851 | `	pCtx->zThrowMsg[0] = 0;` |
|     2360 |  852 | `	pCtx->iThrowCode = 0;` |
|     2360 |  853 | `	pCtx->bQuiet = 0;` |
|     2360 |  854 | `	pCtx->bWriteCtx = 0;` |
|     2360 |  855 | `	pCtx->nSlot = SXU32_HIGH;` |
|     2360 |  856 | `	return PH7_ClassNativeProp(pThis,pCtx);` |
|        4 |  857 | `}` |
|        - |  858 | `/*` |
|        - |  859 | `` * Does this class answer `$o->p` through a HANDLER of its own -- php's question`` |
|        - |  860 | ` * "is the name in the class's property-handler table"?` |
|        - |  861 | ` *` |
|        - |  862 | ` * The hook answers for itself, because what the answer depends on differs per` |
|        - |  863 | ` * class: ext/dom reads the class's VIRTUAL declarations (PH7_MOD_VIRTUAL -- a` |
|        - |  864 | ` * name declared with no slot of any kind), ArrayObject reads the object's` |
|        - |  865 | ` * ARRAY_AS_PROPS flag and owns every name once it is set. Whether a REAL property` |
|        - |  866 | ` * of that name is in the way is not the hook's question: every caller asks only` |
|        - |  867 | ` * after the instance's own table missed, which is php's order too.` |
|        - |  868 | ` *` |
|        - |  869 | `` * It is what makes the handler beat a subclass's own `__get`: a name the table`` |
|        - |  870 | ` * carries never reaches the magic layer, and a name it does not carry falls` |
|        - |  871 | ` * through to it, which is php's handler order. Asked at the write shapes, where` |
|        - |  872 | ` * the member opcode has no value yet and the write half cannot be run for an` |
|        - |  873 | ` * answer.` |
|        - |  874 | ` */` |
|     2172 |  875 | `PH7_PRIVATE int PH7_ClassNativePropOwns(ph7_class_instance *pThis,const SyString *pName)` |
|        5 |  876 | `{` |
|        - |  877 | `	PH7_NativePropCtx sCtx;` |
|     2177 |  878 | `	if( pThis == 0 \|\| NativePropClass(pThis->pClass) == 0 ){` |
|      643 |  879 | `		return 0;` |
|        - |  880 | `	}` |
|     2297 |  881 | `	return PH7_ClassNativePropAsk(pThis,&sCtx,PH7_NATIVE_PROP_OWNS,pName,0)` |
|     1534 |  882 | `	    && sCtx.zThrowClass == 0;` |
|     1091 |  883 | `}` |
|        - |  884 | `/*` |
|        - |  885 | ` * Install a property handler on a mounted native class. Called by the owning` |
|        - |  886 | ` * installer right after PH7_InstallNativeClasses, for the same reason xClone,` |
|        - |  887 | ` * xDim and xSet are: the spec table has no field for a hook.` |
|        - |  888 | ` */` |
|    60489 |  889 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallPropHook(ph7_vm *pVm,const char *zClass,` |
|        - |  890 | `	void (*xProp)(ph7_vm *,ph7_class_instance *,PH7_NativePropCtx *))` |
|        5 |  891 | `{` |
|    60494 |  892 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|    60494 |  893 | `	if( pClass == 0 ){` |
|      ! 0 |  894 | `		return SXERR_NOTFOUND;` |
|        - |  895 | `	}` |
|    60494 |  896 | `	pClass->xProp = xProp;` |
|    60494 |  897 | `	return SXRET_OK;` |
|    30209 |  898 | `}` |
|        - |  899 | `/*` |
|        - |  900 | ` * The nearest ph7_class::xSet in a class's base chain -- the same handler` |
|        - |  901 | ` * inheritance the dimension hook gets, so a user subclass of DateInterval` |
|        - |  902 | ` * converts its writes the way its parent does.` |
|        - |  903 | ` */` |
|      618 |  904 | `static ph7_class * NativeSetClass(ph7_class *pClass)` |
|        4 |  905 | `{` |
|      624 |  906 | `	while( pClass ){` |
|      624 |  907 | `		if( pClass->xSet ){` |
|      622 |  908 | `			return pClass;` |
|        - |  909 | `		}` |
|        3 |  910 | `		pClass = pClass->pBase;` |
|        1 |  911 | `	}` |
|      ! 0 |  912 | `	return 0;` |
|      313 |  913 | `}` |
|        - |  914 | `/*` |
|        - |  915 | ` * Run the write handler for a property store. Answers 0 when no class in the` |
|        - |  916 | ` * chain has one (nothing in pCtx is touched); 1 when it ran, which includes a` |
|        - |  917 | ` * REFUSAL -- the caller reads zThrowClass to tell the two apart.` |
|        - |  918 | ` */` |
|      618 |  919 | `PH7_PRIVATE int PH7_ClassNativeSet(ph7_class_instance *pThis,PH7_NativeSetCtx *pCtx)` |
|        4 |  920 | `{` |
|      622 |  921 | `	ph7_class *pClass = pThis ? NativeSetClass(pThis->pClass) : 0;` |
|      622 |  922 | `	if( pClass == 0 ){` |
|      ! 0 |  923 | `		return 0;` |
|        - |  924 | `	}` |
|      622 |  925 | `	pClass->xSet(pThis->pVm,pThis,pCtx);` |
|      622 |  926 | `	return 1;` |
|      313 |  927 | `}` |
|        - |  928 | `/*` |
|        - |  929 | ` * php's create_object handler for a mounted native class: the C routine that` |
|        - |  930 | ` * runs once the instance frame exists and before any constructor.` |
|        - |  931 | ` *` |
|        - |  932 | ` * It exists for one shape -- a class whose properties php DECLARES and then` |
|        - |  933 | ` * answers through a read_property handler rather than out of the slots. Seeding` |
|        - |  934 | `` * the slots reproduces every surface of that at once (the read, `var_dump`, the`` |
|        - |  935 | `` * `(array)` cast, `json_encode`, `foreach`, `serialize`, `get_object_vars`),`` |
|        - |  936 | ` * and leaves the DECLARATION alone: the properties still have no default, so` |
|        - |  937 | ` * Reflection's hasDefaultValue() answers false the way php's does.` |
|        - |  938 | ` */` |
|     6721 |  939 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallNewHook(ph7_vm *pVm,const char *zClass,` |
|        - |  940 | `	void (*xNew)(ph7_vm *,ph7_class_instance *))` |
|        5 |  941 | `{` |
|     6726 |  942 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|     6726 |  943 | `	if( pClass == 0 ){` |
|      ! 0 |  944 | `		return SXERR_NOTFOUND;` |
|        - |  945 | `	}` |
|     6726 |  946 | `	pClass->xNew = xNew;` |
|     6726 |  947 | `	return SXRET_OK;` |
|     3361 |  948 | `}` |
|        - |  949 | `/*` |
|        - |  950 | ` * Install a write handler on a mounted native class and mark every INSTANCE` |
|        - |  951 | ` * property it declares as filtered, which is what makes instantiation register` |
|        - |  952 | ` * the slots the filter looks up. Called by the owning installer right after` |
|        - |  953 | ` * PH7_InstallNativeClasses, for the same reason xClone and xDim are: the spec` |
|        - |  954 | ` * table has no field for a hook.` |
|        - |  955 | ` */` |
|    13442 |  956 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallSetHook(ph7_vm *pVm,const char *zClass,` |
|        - |  957 | `	void (*xSet)(ph7_vm *,ph7_class_instance *,PH7_NativeSetCtx *))` |
|        5 |  958 | `{` |
|    13447 |  959 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|        - |  960 | `	SyHashEntry *pEntry;` |
|    13447 |  961 | `	if( pClass == 0 ){` |
|      ! 0 |  962 | `		return SXERR_NOTFOUND;` |
|        - |  963 | `	}` |
|    13447 |  964 | `	pClass->xSet = xSet;` |
|    13447 |  965 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|   154597 |  966 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|   134425 |  967 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   134425 |  968 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|   134425 |  969 | `			pAttr->iFlags \|= PH7_CLASS_ATTR_NATIVE_SET;` |
|    67120 |  970 | `		}` |
|        5 |  971 | `	}` |
|    13447 |  972 | `	return SXRET_OK;` |
|     6717 |  973 | `}` |
|        - |  974 | `/*` |
|        - |  975 | ` * The nearest ph7_class::xCmp in a class's base chain -- the same handler` |
|        - |  976 | ` * inheritance xDim and xSet get, and php's own: a subclass of DateTime still` |
|        - |  977 | ` * compares as an instant, whatever properties it adds.` |
|        - |  978 | ` */` |
|      706 |  979 | `static ph7_class * NativeCmpClass(ph7_class *pClass)` |
|        5 |  980 | `{` |
|     1131 |  981 | `	while( pClass ){` |
|      721 |  982 | `		if( pClass->xCmp ){` |
|      300 |  983 | `			return pClass;` |
|        - |  984 | `		}` |
|      425 |  985 | `		pClass = pClass->pBase;` |
|        5 |  986 | `	}` |
|      415 |  987 | `	return 0;` |
|      358 |  988 | `}` |
|        - |  989 | `/*` |
|        - |  990 | ` * Ask the LEFT operand's compare handler, php-style. Answers 0 when no class in` |
|        - |  991 | ` * its chain has one (the caller falls back to the property walk); 1 when the` |
|        - |  992 | ` * handler decided, and *pResult is then the ordering -- which includes a` |
|        - |  993 | ` * REFUSAL, recorded on the VM for the nearest throw boundary to raise, with the` |
|        - |  994 | ` * uncomparable 1 standing in as the answer meanwhile.` |
|        - |  995 | ` */` |
|      382 |  996 | `PH7_PRIVATE int PH7_ClassNativeCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,sxi32 *pResult)` |
|        5 |  997 | `{` |
|      387 |  998 | `	ph7_class *pClass = pLeft ? NativeCmpClass(pLeft->pClass) : 0;` |
|        - |  999 | `	PH7_NativeCmpCtx sCtx;` |
|        - | 1000 | `	ph7_vm *pVm;` |
|      387 | 1001 | `	if( pClass == 0 ){` |
|      211 | 1002 | `		return 0;` |
|        - | 1003 | `	}` |
|      180 | 1004 | `	pVm = pLeft->pVm;` |
|      180 | 1005 | `	SyZero(&sCtx,sizeof(sCtx));` |
|      180 | 1006 | `	sCtx.pOther = pRight;` |
|      180 | 1007 | `	sCtx.iResult = 1;   /* php's ZEND_UNCOMPARABLE: what a hook that recognizes nothing answers */` |
|      180 | 1008 | `	pClass->xCmp(pVm,pLeft,&sCtx);` |
|      180 | 1009 | `	if( sCtx.zThrowClass && pVm->zCmpRefusalClass == 0 ){` |
|        - | 1010 | `		/* First refusal wins: a driver that keeps comparing after one (sort() does)` |
|        - | 1011 | `		 * must not overwrite the message the script will actually see. */` |
|       53 | 1012 | `		pVm->zCmpRefusalClass = sCtx.zThrowClass;` |
|       53 | 1013 | `		SyMemcpy(sCtx.zThrowMsg,pVm->zCmpRefusalMsg,sizeof(pVm->zCmpRefusalMsg));` |
|       53 | 1014 | `		pVm->zCmpRefusalMsg[sizeof(pVm->zCmpRefusalMsg)-1] = 0;` |
|       26 | 1015 | `	}` |
|      180 | 1016 | `	*pResult = sCtx.iResult;` |
|      180 | 1017 | `	return 1;` |
|      196 | 1018 | `}` |
|        - | 1019 | `/*` |
|        - | 1020 | ` * The same handler, asked about a SCALAR partner -- php's compare handler is one` |
|        - | 1021 | `` * door and `$n == 2` reaches it exactly as `$n == $m` does. Answers 1 only when`` |
|        - | 1022 | ` * the hook RECOGNIZED the value; otherwise the caller falls back to php's` |
|        - | 1023 | ` * cast-the-object rule, which is what every class without a handler gets.` |
|        - | 1024 | ` */` |
|      324 | 1025 | `PH7_PRIVATE int PH7_ClassNativeCmpValue(ph7_class_instance *pLeft,ph7_value *pOther,` |
|        - | 1026 | `	int bReversed,sxi32 *pResult)` |
|        4 | 1027 | `{` |
|      328 | 1028 | `	ph7_class *pClass = pLeft ? NativeCmpClass(pLeft->pClass) : 0;` |
|        - | 1029 | `	PH7_NativeCmpCtx sCtx;` |
|        - | 1030 | `	ph7_vm *pVm;` |
|      328 | 1031 | `	if( pClass == 0 ){` |
|      208 | 1032 | `		return 0;` |
|        - | 1033 | `	}` |
|      123 | 1034 | `	pVm = pLeft->pVm;` |
|      123 | 1035 | `	SyZero(&sCtx,sizeof(sCtx));` |
|      123 | 1036 | `	sCtx.pOtherValue = pOther;` |
|      123 | 1037 | `	sCtx.bReversed = bReversed;` |
|      123 | 1038 | `	sCtx.iResult = 1;   /* php's ZEND_UNCOMPARABLE */` |
|      123 | 1039 | `	pClass->xCmp(pVm,pLeft,&sCtx);` |
|      123 | 1040 | `	if( sCtx.zThrowClass && pVm->zCmpRefusalClass == 0 ){` |
|      ! 0 | 1041 | `		pVm->zCmpRefusalClass = sCtx.zThrowClass;` |
|      ! 0 | 1042 | `		SyMemcpy(sCtx.zThrowMsg,pVm->zCmpRefusalMsg,sizeof(pVm->zCmpRefusalMsg));` |
|      ! 0 | 1043 | `		pVm->zCmpRefusalMsg[sizeof(pVm->zCmpRefusalMsg)-1] = 0;` |
|      ! 0 | 1044 | `	}` |
|      123 | 1045 | `	if( !sCtx.bAnswered ){` |
|       29 | 1046 | `		return 0;` |
|        - | 1047 | `	}` |
|       96 | 1048 | `	*pResult = sCtx.iResult;` |
|       96 | 1049 | `	return 1;` |
|      166 | 1050 | `}` |
|        - | 1051 | `/*` |
|        - | 1052 | ` * Install a compare handler on a mounted native class. Called by the owning` |
|        - | 1053 | ` * installer right after PH7_InstallNativeClasses, for the reason xClone, xDim` |
|        - | 1054 | ` * and xSet are: the spec table has no field for a hook.` |
|        - | 1055 | ` */` |
|        - | 1056 | `/*` |
|        - | 1057 | ` * php's cast_object handler for _IS_BOOL, which is the one conversion an object` |
|        - | 1058 | ` * may decide for itself. Answers 1 when the class HAS a handler, with the truth` |
|        - | 1059 | ` * value in *pOut; every other class keeps php's rule that an object is truthy.` |
|        - | 1060 | ` */` |
|      314 | 1061 | `PH7_PRIVATE int PH7_ClassNativeBool(ph7_class_instance *pThis,int *pOut)` |
|        4 | 1062 | `{` |
|        - | 1063 | `	ph7_class *pClass;` |
|      786 | 1064 | `	for( pClass = pThis ? pThis->pClass : 0 ; pClass ; pClass = pClass->pBase ){` |
|      492 | 1065 | `		if( pClass->xBool ){` |
|       22 | 1066 | `			*pOut = pClass->xBool(pThis->pVm,pThis) ? 1 : 0;` |
|       22 | 1067 | `			return 1;` |
|        - | 1068 | `		}` |
|      236 | 1069 | `	}` |
|      298 | 1070 | `	return 0;` |
|      159 | 1071 | `}` |
|     6721 | 1072 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallBoolHook(ph7_vm *pVm,const char *zClass,` |
|        - | 1073 | `	int (*xBool)(ph7_vm *,ph7_class_instance *))` |
|        5 | 1074 | `{` |
|     6726 | 1075 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|     6726 | 1076 | `	if( pClass == 0 ){` |
|      ! 0 | 1077 | `		return SXERR_NOTFOUND;` |
|        - | 1078 | `	}` |
|     6726 | 1079 | `	pClass->xBool = xBool;` |
|     6726 | 1080 | `	return SXRET_OK;` |
|     3361 | 1081 | `}` |
|     6721 | 1082 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallArithHook(ph7_vm *pVm,const char *zClass,` |
|        - | 1083 | `	void (*xArith)(ph7_vm *,ph7_class_instance *,PH7_NativeArithCtx *))` |
|        5 | 1084 | `{` |
|     6726 | 1085 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|     6726 | 1086 | `	if( pClass == 0 ){` |
|      ! 0 | 1087 | `		return SXERR_NOTFOUND;` |
|        - | 1088 | `	}` |
|     6726 | 1089 | `	pClass->xArith = xArith;` |
|     6726 | 1090 | `	return SXRET_OK;` |
|     3361 | 1091 | `}` |
|        - | 1092 | `/*` |
|        - | 1093 | ` * php's compare handler for an OPAQUE HANDLE class -- one whose object stands for` |
|        - | 1094 | ` * something outside the engine (a curl easy/multi/share handle, a PDO connection,` |
|        - | 1095 | ` * a statement, a lazy row). php gives each of them a handler that recognizes` |
|        - | 1096 | ` * NOTHING, so every comparison that is not the identity shortcut is` |
|        - | 1097 | `` * ZEND_UNCOMPARABLE: `$h == 1`, `$h < 2`, `$h > 0` and `$h == $other` are all`` |
|        - | 1098 | `` * false, `$h <=> $x` is 1 from either direction, and none of it says a word.`` |
|        - | 1099 | ` * Without it these fell through to php's cast-the-object rule, which warns` |
|        - | 1100 | `` * `could not be converted to int` and then calls the handle equal to 1 -- so`` |
|        - | 1101 | ` * in_array($h, [1,2,3]) was TRUE, and sorting a list that held one was noise.` |
|        - | 1102 | ` *` |
|        - | 1103 | ` * The uncomparable 1 is deliberately not flipped for bReversed: php answers it` |
|        - | 1104 | ` * from both sides alike, which is what leaves every relational spelling false.` |
|        - | 1105 | ` */` |
|      108 | 1106 | `PH7_PRIVATE void PH7_NativeCmpOpaqueHandle(ph7_vm *pVm,ph7_class_instance *pThis,` |
|        - | 1107 | `	PH7_NativeCmpCtx *pCtx)` |
|        3 | 1108 | `{` |
|       54 | 1109 | `	SXUNUSED(pVm);` |
|       54 | 1110 | `	SXUNUSED(pThis);` |
|      111 | 1111 | `	if( pCtx->pOtherValue && (pCtx->pOtherValue->iFlags & MEMOBJ_BOOL) ){` |
|       16 | 1112 | `		return;   /* declined: php's cast rule decides an object against a bool */` |
|        - | 1113 | `	}` |
|       97 | 1114 | `	pCtx->bAnswered = 1;` |
|       97 | 1115 | `	pCtx->iResult = 1;   /* php's ZEND_UNCOMPARABLE */` |
|       57 | 1116 | `}` |
|        - | 1117 | `/*` |
|        - | 1118 | `` * TRUE when `(int)` on an instance of this class answers the object handle`` |
|        - | 1119 | ` * (PH7_CLASS_HANDLE_ID). Resolved through the ANCESTORS, like every other native` |
|        - | 1120 | ` * hook: php installs the cast on the class's object handlers, and a subclass` |
|        - | 1121 | ` * inherits the whole handler table.` |
|        - | 1122 | ` */` |
|       40 | 1123 | `PH7_PRIVATE int PH7_ClassCastsToHandleId(ph7_class *pClass)` |
|        4 | 1124 | `{` |
|       64 | 1125 | `	while( pClass ){` |
|       44 | 1126 | `		if( pClass->iFlags & PH7_CLASS_HANDLE_ID ){` |
|       21 | 1127 | `			return 1;` |
|        - | 1128 | `		}` |
|       24 | 1129 | `		pClass = pClass->pBase;` |
|        4 | 1130 | `	}` |
|       24 | 1131 | `	return 0;` |
|       24 | 1132 | `}` |
|        - | 1133 | `/* The same base-chain question for the two flags beside it: a subclass of` |
|        - | 1134 | ` * SimpleXMLElement casts to a number and answers get_object_vars the way its` |
|        - | 1135 | ` * parent does, which is php's handler inheritance. */` |
|      114 | 1136 | `PH7_PRIVATE int PH7_ClassNumberIsString(ph7_class *pClass)` |
|        5 | 1137 | `{` |
|      213 | 1138 | `	while( pClass ){` |
|      119 | 1139 | `		if( pClass->iFlags & PH7_CLASS_NUM_AS_STRING ){` |
|       21 | 1140 | `			return 1;` |
|        - | 1141 | `		}` |
|       99 | 1142 | `		pClass = pClass->pBase;` |
|        5 | 1143 | `	}` |
|       99 | 1144 | `	return 0;` |
|       62 | 1145 | `}` |
|      354 | 1146 | `PH7_PRIVATE int PH7_ClassVarsFromPresent(ph7_class *pClass)` |
|        4 | 1147 | `{` |
|      774 | 1148 | `	while( pClass ){` |
|      422 | 1149 | `		if( pClass->iFlags & PH7_CLASS_VARS_PRESENT ){` |
|        3 | 1150 | `			return 1;` |
|        - | 1151 | `		}` |
|      420 | 1152 | `		pClass = pClass->pBase;` |
|        4 | 1153 | `	}` |
|      356 | 1154 | `	return 0;` |
|      181 | 1155 | `}` |
|    47047 | 1156 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallCmpHook(ph7_vm *pVm,const char *zClass,` |
|        - | 1157 | `	void (*xCmp)(ph7_vm *,ph7_class_instance *,PH7_NativeCmpCtx *))` |
|        5 | 1158 | `{` |
|    47052 | 1159 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|    47052 | 1160 | `	if( pClass == 0 ){` |
|      ! 0 | 1161 | `		return SXERR_NOTFOUND;` |
|        - | 1162 | `	}` |
|    47052 | 1163 | `	pClass->xCmp = xCmp;` |
|    47052 | 1164 | `	return SXRET_OK;` |
|    23497 | 1165 | `}` |
|        - | 1166 | `/*` |
|        - | 1167 | ` * Mark every INSTANCE property a mounted native class declares as one php` |
|        - | 1168 | ` * FABRICATES rather than stores (PH7_CLASS_ATTR_NATIVE_VIRTUAL), which is what` |
|        - | 1169 | ` * keeps the object comparator from seeing it. DatePeriod is the whole caller` |
|        - | 1170 | ` * list: php's object has an EMPTY real property table, so two of them are equal` |
|        - | 1171 | ` * whatever they contain, while a subclass's own property still decides.` |
|        - | 1172 | ` */` |
|    13442 | 1173 | `PH7_PRIVATE sxi32 PH7_NativeClassMarkVirtualProps(ph7_vm *pVm,const char *zClass)` |
|        5 | 1174 | `{` |
|    13447 | 1175 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|        - | 1176 | `	SyHashEntry *pEntry;` |
|    13447 | 1177 | `	if( pClass == 0 ){` |
|      ! 0 | 1178 | `		return SXERR_NOTFOUND;` |
|        - | 1179 | `	}` |
|    13447 | 1180 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|    87387 | 1181 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|    67215 | 1182 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|    67215 | 1183 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|    67215 | 1184 | `			pAttr->iFlags \|= PH7_CLASS_ATTR_NATIVE_VIRTUAL;` |
|    33560 | 1185 | `		}` |
|        5 | 1186 | `	}` |
|    13447 | 1187 | `	return SXRET_OK;` |
|     6717 | 1188 | `}` |
|        - | 1189 | `/*` |
|        - | 1190 | ` * Mark every php-VISIBLE instance property a mounted native class declares as one` |
|        - | 1191 | ` * the OBJECT does not hold until its constructor fills it` |
|        - | 1192 | ` * (PH7_CLASS_ATTR_NATIVE_LAZY). php's DateInterval and DatePeriod are the caller` |
|        - | 1193 | ` * list: the state is a C struct the constructor allocates and the property table` |
|        - | 1194 | ` * is written FROM it, so an object nobody constructed has no such property at all.` |
|        - | 1195 | ` *` |
|        - | 1196 | ` * The HIDDEN slots are left alone -- they are PHL's own storage, they have to` |
|        - | 1197 | `` * exist from `new` (the initialized FLAG lives in one of them), and php shows`` |
|        - | 1198 | ` * nothing for them either way.` |
|        - | 1199 | ` *` |
|        - | 1200 | ` * bDefaultRead selects which of php's two handlers the class has: with it, a read` |
|        - | 1201 | ` * of a still-absent slot answers the DECLARED literal in silence (DatePeriod's` |
|        - | 1202 | ` * read_property over the zeroed struct); without it, the name really is undefined` |
|        - | 1203 | ` * until the constructor runs (DateInterval).` |
|        - | 1204 | ` */` |
|        - | 1205 | `/*` |
|        - | 1206 | ` * Mark every php-VISIBLE instance property a mounted native class declares as one` |
|        - | 1207 | ` * whose WRITE php's handler refuses (PH7_CLASS_ATTR_NATIVE_NOWRITE). DatePeriod is` |
|        - | 1208 | `` * the caller list: php answers `Cannot modify readonly property DatePeriod::$p` to`` |
|        - | 1209 | `` * every write form and `Cannot unset DatePeriod::$p` to an unset, while Reflection`` |
|        - | 1210 | ` * still reports isReadOnly() false -- the wording is the handler's, not the` |
|        - | 1211 | ` * readonly flag's. The C bodies that fill the seven write their slots directly and` |
|        - | 1212 | ` * never pass the store filter, so the refusal costs the class nothing.` |
|        - | 1213 | ` */` |
|     6721 | 1214 | `PH7_PRIVATE sxi32 PH7_NativeClassMarkNoWriteProps(ph7_vm *pVm,const char *zClass)` |
|        5 | 1215 | `{` |
|     6726 | 1216 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|        - | 1217 | `	SyHashEntry *pEntry;` |
|     6726 | 1218 | `	if( pClass == 0 ){` |
|      ! 0 | 1219 | `		return SXERR_NOTFOUND;` |
|        - | 1220 | `	}` |
|     6726 | 1221 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|    60494 | 1222 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|    53773 | 1223 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|    53773 | 1224 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_HIDDEN) ){` |
|     6726 | 1225 | `			continue;` |
|        - | 1226 | `		}` |
|    47052 | 1227 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_NATIVE_NOWRITE;` |
|        5 | 1228 | `	}` |
|     6726 | 1229 | `	return SXRET_OK;` |
|     3361 | 1230 | `}` |
|        - | 1231 | `/*` |
|        - | 1232 | ` * Mark ONE property of ONE instance as php's read-only kind: a plain store and` |
|        - | 1233 | `` * an unset() refuse with `Property p is read only`, everything that takes a`` |
|        - | 1234 | ` * pointer to it goes through. It is marked per OBJECT because php's handler is` |
|        - | 1235 | ` * -- a PDOStatement with no cursor behind it takes the write, and only the one` |
|        - | 1236 | ` * a driver built refuses.` |
|        - | 1237 | ` */` |
|      920 | 1238 | `PH7_PRIVATE void PH7_NativeMarkAttrReadOnly(ph7_class_instance *pThis,const char *zProp)` |
|        4 | 1239 | `{` |
|      924 | 1240 | `	SyHashEntry *pEntry = pThis` |
|      920 | 1241 | `		? SyHashGet(&pThis->hAttr,(const void *)zProp,(sxu32)SyStrlen(zProp)) : 0;` |
|      924 | 1242 | `	if( pEntry ){` |
|      924 | 1243 | `		((VmClassAttr *)pEntry->pUserData)->iState \|= VM_CLASS_ATTR_RDONLY;` |
|      460 | 1244 | `	}` |
|      924 | 1245 | `}` |
|    20163 | 1246 | `PH7_PRIVATE sxi32 PH7_NativeClassMarkLazyProps(ph7_vm *pVm,const char *zClass,int bDefaultRead)` |
|        5 | 1247 | `{` |
|    20168 | 1248 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|        - | 1249 | `	SyHashEntry *pEntry;` |
|    20168 | 1250 | `	if( pClass == 0 ){` |
|      ! 0 | 1251 | `		return SXERR_NOTFOUND;` |
|        - | 1252 | `	}` |
|    20168 | 1253 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|   181472 | 1254 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|   161309 | 1255 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   161309 | 1256 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_HIDDEN) ){` |
|    33610 | 1257 | `			continue;` |
|        - | 1258 | `		}` |
|   127704 | 1259 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_NATIVE_LAZY;` |
|   127704 | 1260 | `		if( bDefaultRead ){` |
|    47052 | 1261 | `			pAttr->iFlags \|= PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT;` |
|    23492 | 1262 | `		}` |
|   127704 | 1263 | `		pClass->iFlags \|= PH7_CLASS_LAZY_ATTR;` |
|        5 | 1264 | `	}` |
|    20168 | 1265 | `	return SXRET_OK;` |
|    10073 | 1266 | `}` |
|        - | 1267 | `/*` |
|        - | 1268 | ` * Install this object's LAZY properties -- the whole set, in the order the class` |
|        - | 1269 | ` * declares them, skipping any the object already carries.` |
|        - | 1270 | ` *` |
|        - | 1271 | ` * The ORDER is php's: its constructor writes the struct's fields into the property` |
|        - | 1272 | ` * table one after another, so a name the object already has keeps its POSITION and` |
|        - | 1273 | ` * only takes the new value, and the rest are appended in declared order behind it.` |
|        - | 1274 | ` * VmRecreateDeclaredAttr tail-inserts exactly that way.` |
|        - | 1275 | ` *` |
|        - | 1276 | ` * The declared literal goes in as the slot's starting value (php's zeroed struct),` |
|        - | 1277 | ` * and the not-yet-initialized mark a TYPED slot would carry is cleared with it:` |
|        - | 1278 | ` * these are filled by the C body that is about to write them, and a read between` |
|        - | 1279 | ` * the two is php's default, not its Error.` |
|        - | 1280 | ` */` |
|  1594692 | 1281 | `PH7_PRIVATE void PH7_NativeMaterializeLazy(ph7_vm *pVm,ph7_class_instance *pObj)` |
|        5 | 1282 | `{` |
|        - | 1283 | `	SyHashEntry *pEntry;` |
|  1594692 | 1284 | `	if( pObj == 0 \|\| (pObj->pClass->iFlags & PH7_CLASS_LAZY_ATTR) == 0` |
|   800842 | 1285 | `	 \|\| (pObj->iFlags & VM_INSTANCE_LAZY_DONE) ){` |
|  1594079 | 1286 | `		return;` |
|        - | 1287 | `	}` |
|      621 | 1288 | `	pObj->iFlags \|= VM_INSTANCE_LAZY_DONE;` |
|      621 | 1289 | `	SyHashResetLoopCursor(&pObj->pClass->hAttr);` |
|     7887 | 1290 | `	while( (pEntry = SyHashGetNextEntry(&pObj->pClass->hAttr)) != 0 ){` |
|     7269 | 1291 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     7269 | 1292 | `		VmClassAttr *pVmAttr = 0;` |
|        - | 1293 | `		ph7_value *pSlot;` |
|     7266 | 1294 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_LAZY) == 0` |
|     6719 | 1295 | `		 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_ONDEMAND) != 0 ){` |
|     1563 | 1296 | `			continue;   /* ...and an ON-DEMAND one waits for the write that names it */` |
|        - | 1297 | `		}` |
|     5709 | 1298 | `		if( SyHashGet(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName)) != 0 ){` |
|      ! 0 | 1299 | `			continue;` |
|        - | 1300 | `		}` |
|     5709 | 1301 | `		VmRecreateDeclaredAttr(&(*pVm),pObj,pAttr,&pVmAttr);` |
|     5709 | 1302 | `		if( pVmAttr == 0 ){` |
|      ! 0 | 1303 | `			continue;   /* OOM: the caller's write lands nowhere, as it would have anyway */` |
|        - | 1304 | `		}` |
|     5709 | 1305 | `		pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|     5709 | 1306 | `		if( pAttr->pNativeValue == 0 ){` |
|      ! 0 | 1307 | `			continue;` |
|        - | 1308 | `		}` |
|     5709 | 1309 | `		pSlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pVmAttr->nIdx);` |
|     5709 | 1310 | `		if( pSlot ){` |
|     5709 | 1311 | `			PH7_NativeLiteralValue(&(*pVm),pAttr->pNativeValue,pSlot);` |
|     2853 | 1312 | `		}` |
|        3 | 1313 | `	}` |
|   797312 | 1314 | `}` |
|        - | 1315 | `/*` |
|        - | 1316 | ` * Create and install ONE class from its spec: constants and properties, but` |
|        - | 1317 | ` * neither methods nor its base chain.` |
|        - | 1318 | ` *` |
|        - | 1319 | ` * Split from the two passes that follow because a spec table may describe` |
|        - | 1320 | ` * classes that extend each other, and PH7_ClassInherit needs the parent to` |
|        - | 1321 | ` * exist -- and to already CARRY ITS METHODS, since inheriting is what copies` |
|        - | 1322 | ` * them down.` |
|        - | 1323 | ` */` |
|  1445015 | 1324 | `static sxi32 NativeDeclareClass(ph7_vm *pVm,const PH7_NativeClassSpec *pSpec,ph7_class **ppOut)` |
|        5 | 1325 | `{` |
|        - | 1326 | `	ph7_class *pClass;` |
|        - | 1327 | `	SyString sName;` |
|        - | 1328 | `	sxu32 n;` |
|        - | 1329 | `	sxi32 rc;` |
|  1445020 | 1330 | `	SyStringInitFromBuf(&sName,pSpec->zName,SyStrlen(pSpec->zName));` |
|  1445020 | 1331 | `	pClass = PH7_NewRawClass(&(*pVm),&sName,0);` |
|  1445020 | 1332 | `	if( pClass == 0 ){` |
|      ! 0 | 1333 | `		return SXERR_MEM;` |
|        - | 1334 | `	}` |
|        - | 1335 | `	/* PH7_CLASS_BOUND: an engine class is declared unconditionally, exactly once,` |
|        - | 1336 | ``	 * so `class DateTime {}` in user code is php's "Cannot redeclare class`` |
|        - | 1337 | `	 * DateTime". Only the compiler used to set the flag, so EVERY native class` |
|        - | 1338 | `	 * was silently redeclarable — the chunk-declared ones (Exception, stdClass)` |
|        - | 1339 | `	 * fataled and their C replacements did not. */` |
|  1445020 | 1340 | `	pClass->iFlags \|= pSpec->iFlags \| PH7_CLASS_BOUND;` |
|  1445020 | 1341 | `	pClass->xRelease = pSpec->xRelease;` |
|  1445020 | 1342 | `	pClass->pIterVtab = pSpec->pIterVtab;` |
|  1445020 | 1343 | `	pClass->xPresent = pSpec->xPresent;` |
|  4019163 | 1344 | `	for( n = 0 ; n < pSpec->nConst ; n++ ){` |
|  2574148 | 1345 | `		rc = NativeInstallConstant(&(*pVm),pClass,&pSpec->aConst[n]);` |
|  2574148 | 1346 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1347 | `			return rc;` |
|        - | 1348 | `		}` |
|  1285353 | 1349 | `	}` |
|  3965395 | 1350 | `	for( n = 0 ; n < pSpec->nProp ; n++ ){` |
|  2520380 | 1351 | `		rc = PH7_NativeClassInstallProperty(&(*pVm),pClass,&pSpec->aProp[n]);` |
|  2520380 | 1352 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1353 | `			return rc;` |
|        - | 1354 | `		}` |
|  1258505 | 1355 | `	}` |
|  1445020 | 1356 | `	*ppOut = pClass;` |
|  1445020 | 1357 | `	return PH7_VmInstallClass(&(*pVm),pClass);` |
|   721545 | 1358 | `}` |
|        - | 1359 | `/*` |
|        - | 1360 | ` * Wire ONE class's base chain and interfaces.` |
|        - | 1361 | ` *` |
|        - | 1362 | ` * This runs AFTER every class in the table has its own methods, which is the` |
|        - | 1363 | ` * compiler's order too (GenStateCompileClassEx compiles the whole body and only` |
|        - | 1364 | ` * then calls PH7_ClassInherit/PH7_ClassImplement). Two things depend on it:` |
|        - | 1365 | ` * PH7_ClassInherit COPIES the base's methods into the subclass, so a base whose` |
|        - | 1366 | ` * methods were not installed yet hands down an empty table -- which is how` |
|        - | 1367 | `` * `DOMDocument::C14N()` came out undefined, the first time a native class`` |
|        - | 1368 | ` * extended another native class that had methods; and PH7_ClassImplement stubs` |
|        - | 1369 | ` * every interface method the class lacks as ABSTRACT, so a spec may now name an` |
|        - | 1370 | ` * interface it implements itself rather than attaching it by hand afterwards.` |
|        - | 1371 | ` */` |
|  1445015 | 1372 | `static sxi32 NativeLinkClass(ph7_vm *pVm,const PH7_NativeClassSpec *pSpec,ph7_class *pClass)` |
|        5 | 1373 | `{` |
|        - | 1374 | `	sxi32 rc;` |
|  1445020 | 1375 | `	if( pSpec->zParent ){` |
|   712431 | 1376 | `		ph7_class *pBase = NativeLookupClass(&(*pVm),pSpec->zParent);` |
|   712431 | 1377 | `		if( pBase == 0 ){` |
|      ! 0 | 1378 | `			return SXERR_NOTFOUND;` |
|        - | 1379 | `		}` |
|        - | 1380 | `		/* The VM's OWN generator state, not a NULL one: PH7_ClassInherit takes its` |
|        - | 1381 | `		 * scratch allocator from pGen->pVm and reports every inheritance rule it` |
|        - | 1382 | `		 * enforces through PH7_GenCompileError(pGen, ...). Passing 0 crashed the` |
|        - | 1383 | `		 * moment a native spec first named a parent (the date exceptions). */` |
|  1069121 | 1384 | `		rc = (pClass->iFlags & PH7_CLASS_INTERFACE)` |
|    60489 | 1385 | `			? PH7_ClassInterfaceInherit(pClass,pBase)` |
|   682222 | 1386 | `			: PH7_ClassInherit(&pVm->sCodeGen,pClass,pBase);` |
|   712431 | 1387 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1388 | `			return rc;` |
|        - | 1389 | `		}` |
|   355736 | 1390 | `	}` |
|  1445020 | 1391 | `	if( pSpec->zImplements ){` |
|        - | 1392 | `		/* Comma-separated list, so one spec row can name several interfaces. */` |
|   362939 | 1393 | `		const char *zCur = pSpec->zImplements;` |
|   914061 | 1394 | `		while( zCur[0] != '\0' ){` |
|        - | 1395 | `			const char *zStart;` |
|        - | 1396 | `			char zIface[64];` |
|        - | 1397 | `			sxu32 nLen;` |
|        - | 1398 | `			ph7_class *pIface;` |
|   739315 | 1399 | `			while( zCur[0] == ' ' \|\| zCur[0] == ',' ){` |
|   188193 | 1400 | `				zCur++;` |
|        5 | 1401 | `			}` |
|   551127 | 1402 | `			if( zCur[0] == '\0' ){` |
|      ! 0 | 1403 | `				break;` |
|        - | 1404 | `			}` |
|   551127 | 1405 | `			zStart = zCur;` |
|  7251964 | 1406 | `			while( zCur[0] != '\0' && zCur[0] != ',' && zCur[0] != ' ' ){` |
|  6700842 | 1407 | `				zCur++;` |
|        5 | 1408 | `			}` |
|   551127 | 1409 | `			nLen = (sxu32)(zCur - zStart);` |
|   551127 | 1410 | `			if( nLen >= sizeof(zIface) ){` |
|      ! 0 | 1411 | `				return SXERR_SYNTAX;` |
|        - | 1412 | `			}` |
|   551127 | 1413 | `			SyMemcpy(zStart,zIface,nLen);` |
|   551127 | 1414 | `			zIface[nLen] = '\0';` |
|   551127 | 1415 | `			pIface = NativeLookupClass(&(*pVm),zIface);` |
|   551127 | 1416 | `			if( pIface == 0 ){` |
|      ! 0 | 1417 | `				return SXERR_NOTFOUND;` |
|        - | 1418 | `			}` |
|   551127 | 1419 | `			rc = PH7_ClassImplement(pClass,pIface);` |
|   551127 | 1420 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1421 | `				return rc;` |
|        - | 1422 | `			}` |
|        5 | 1423 | `		}` |
|   181224 | 1424 | `	}` |
|  1445020 | 1425 | `	return SXRET_OK;` |
|   721545 | 1426 | `}` |
|        - | 1427 | `/*` |
|        - | 1428 | ` * Install a whole table of native classes, in the compiler's own order: declare` |
|        - | 1429 | ` * them all (so later rows may extend earlier ones), fill in their methods, wire` |
|        - | 1430 | ` * the base chains and interfaces, then mount.` |
|        - | 1431 | ` *` |
|        - | 1432 | ` * Interfaces are declared but never mounted -- VmMountUserClass skips them anyway,` |
|        - | 1433 | ` * their methods being non-invocable.` |
|        - | 1434 | ` */` |
|   342771 | 1435 | `PH7_PRIVATE sxi32 PH7_InstallNativeClasses(ph7_vm *pVm,const PH7_NativeClassSpec *aSpec,sxu32 nSpec)` |
|        5 | 1436 | `{` |
|        - | 1437 | `	ph7_class **apClass;` |
|        - | 1438 | `	sxu32 i,j;` |
|        - | 1439 | `	sxi32 rc;` |
|   342776 | 1440 | `	apClass = (ph7_class **)SyMemBackendAlloc(&pVm->sAllocator,nSpec * sizeof(ph7_class *));` |
|   342776 | 1441 | `	if( apClass == 0 ){` |
|      ! 0 | 1442 | `		return SXERR_MEM;` |
|        - | 1443 | `	}` |
|  1787791 | 1444 | `	for( i = 0 ; i < nSpec ; i++ ){` |
|  1445020 | 1445 | `		apClass[i] = 0;` |
|  1445020 | 1446 | `		rc = NativeDeclareClass(&(*pVm),&aSpec[i],&apClass[i]);` |
|  1445020 | 1447 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1448 | `			goto Done;` |
|        - | 1449 | `		}` |
|   721545 | 1450 | `	}` |
|  1787791 | 1451 | `	for( i = 0 ; i < nSpec ; i++ ){` |
| 10834257 | 1452 | `		for( j = 0 ; j < aSpec[i].nMethod ; j++ ){` |
|  9389242 | 1453 | `			rc = PH7_NativeClassInstallMethod(&(*pVm),apClass[i],&aSpec[i].aMethod[j],0);` |
|  9389242 | 1454 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1455 | `				goto Done;` |
|        - | 1456 | `			}` |
|  4688337 | 1457 | `		}` |
|   721545 | 1458 | `	}` |
|  1787791 | 1459 | `	for( i = 0 ; i < nSpec ; i++ ){` |
|  1445020 | 1460 | `		rc = NativeLinkClass(&(*pVm),&aSpec[i],apClass[i]);` |
|  1445020 | 1461 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1462 | `			goto Done;` |
|        - | 1463 | `		}` |
|   721545 | 1464 | `	}` |
|  1787791 | 1465 | `	for( i = 0 ; i < nSpec ; i++ ){` |
|  1445020 | 1466 | `		rc = VmMountUserClass(&(*pVm),apClass[i]);` |
|  1445020 | 1467 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1468 | `			goto Done;` |
|        - | 1469 | `		}` |
|   721545 | 1470 | `	}` |
|   342776 | 1471 | `	rc = SXRET_OK;` |
|   171615 | 1472 | `Done:` |
|   342776 | 1473 | `	SyMemBackendFree(&pVm->sAllocator,apClass);` |
|   342776 | 1474 | `	return rc;` |
|   171161 | 1475 | `}` |
|        - | 1476 | `/*` |
|        - | 1477 | ` * ---------------------------------------------------------------------------` |
|        - | 1478 | ` * Declaring an ENUM from C.` |
|        - | 1479 | ` *` |
|        - | 1480 | `` * An enum is not a class with constants: each `case` is a class constant whose`` |
|        - | 1481 | ` * slot holds THE singleton instance of the enum for that case, materialized` |
|        - | 1482 | ` * lazily on first access (VmEnumMaterializeCase). The compiler builds one by` |
|        - | 1483 | `` * declaring the readonly `name`/`value` properties, pushing each case onto`` |
|        - | 1484 | ` * ph7_class::aEnumCases, and SYNTHESIZING cases()/from()/tryFrom() as PHP` |
|        - | 1485 | `` * source that forwards to the `__phl_enum_*` thunks.`` |
|        - | 1486 | ` *` |
|        - | 1487 | ` * This does the same three things without a compiler: the case's backing value` |
|        - | 1488 | ` * rides as a literal (ph7_class_attr::pNativeValue, which the materializer` |
|        - | 1489 | ` * reads where a compiled case has byte-code), and the three interface methods` |
|        - | 1490 | ` * are C bodies that call the very same engine workers the synthesized PHP` |
|        - | 1491 | `` * forwards to — so a native enum is an ordinary one to `instanceof`,`` |
|        - | 1492 | `` * `match`, Reflection and `===` case identity.`` |
|        - | 1493 | ` * ---------------------------------------------------------------------------` |
|        - | 1494 | ` */` |
|        - | 1495 | `/* The enum a static native method was called on. */` |
|      199 | 1496 | `static ph7_class * NativeEnumSelf(ph7_context *pCtx,ph7_value *pName)` |
|        2 | 1497 | `{` |
|      201 | 1498 | `	ph7_class *pClass = PH7_ContextCalledClass(pCtx);` |
|      201 | 1499 | `	PH7_MemObjInit(pCtx->pVm,pName);` |
|      201 | 1500 | `	if( pClass ){` |
|      201 | 1501 | `		ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|       99 | 1502 | `	}` |
|      201 | 1503 | `	return pClass;` |
|        2 | 1504 | `}` |
|        - | 1505 | `/*` |
|        - | 1506 | ` * cases() / from() / tryFrom(): the engine thunks take the enum's FQN as their` |
|        - | 1507 | ` * first argument, exactly as the compiler's synthesized bodies pass it.` |
|        - | 1508 | ` */` |
|       21 | 1509 | `static int vm_builtin_NativeEnum_cases(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1510 | `{` |
|        - | 1511 | `	ph7_value sName;` |
|        - | 1512 | `	ph7_value *ap[1];` |
|        - | 1513 | `	int rc;` |
|       10 | 1514 | `	SXUNUSED(nArg);` |
|       10 | 1515 | `	SXUNUSED(apArg);` |
|       22 | 1516 | `	if( NativeEnumSelf(pCtx,&sName) == 0 ){` |
|      ! 0 | 1517 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1518 | `		return PH7_OK;` |
|        - | 1519 | `	}` |
|       22 | 1520 | `	ap[0] = &sName;` |
|       22 | 1521 | `	rc = vm_builtin_enum_cases(pCtx,1,ap);` |
|       22 | 1522 | `	PH7_MemObjRelease(&sName);` |
|       22 | 1523 | `	return rc;` |
|       11 | 1524 | `}` |
|      178 | 1525 | `static int NativeEnumFrom(ph7_context *pCtx,int nArg,ph7_value **apArg,int bTry)` |
|        2 | 1526 | `{` |
|        - | 1527 | `	ph7_value sName;` |
|        - | 1528 | `	ph7_value *ap[2];` |
|        - | 1529 | `	int rc;` |
|      180 | 1530 | `	if( nArg < 1 \|\| NativeEnumSelf(pCtx,&sName) == 0 ){` |
|      ! 0 | 1531 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1532 | `		return PH7_OK;` |
|        - | 1533 | `	}` |
|      180 | 1534 | `	ap[0] = &sName;` |
|      180 | 1535 | `	ap[1] = apArg[0];` |
|      180 | 1536 | `	rc = bTry ? vm_builtin_enum_tryfrom(pCtx,2,ap) : vm_builtin_enum_from(pCtx,2,ap);` |
|      180 | 1537 | `	PH7_MemObjRelease(&sName);` |
|      180 | 1538 | `	return rc;` |
|       91 | 1539 | `}` |
|       96 | 1540 | `static int vm_builtin_NativeEnum_from(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 1541 | `{` |
|       98 | 1542 | `	return NativeEnumFrom(pCtx,nArg,apArg,0);` |
|        2 | 1543 | `}` |
|       82 | 1544 | `static int vm_builtin_NativeEnum_tryFrom(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 1545 | `{` |
|       83 | 1546 | `	return NativeEnumFrom(pCtx,nArg,apArg,1);` |
|        1 | 1547 | `}` |
|        - | 1548 | ``/* The readonly `name` (every enum) and `value` (backed only) case properties,`` |
|        - | 1549 | ` * declared exactly as GenStateCompileEnum does. */` |
|    33605 | 1550 | `static sxi32 NativeEnumInstallProp(ph7_vm *pVm,ph7_class *pClass,const char *zName,` |
|        - | 1551 | `	sxu32 nType,const char *zTypeName)` |
|        5 | 1552 | `{` |
|        - | 1553 | `	SyString sName;` |
|        - | 1554 | `	ph7_class_attr *pAttr;` |
|    33610 | 1555 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|    33610 | 1556 | `	pAttr = PH7_NewClassAttr(&(*pVm),&sName,0,PH7_CLASS_PROT_PUBLIC,` |
|        - | 1557 | `		PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|    33610 | 1558 | `	if( pAttr == 0 ){` |
|      ! 0 | 1559 | `		return SXERR_MEM;` |
|        - | 1560 | `	}` |
|    33610 | 1561 | `	pAttr->nType = nType;` |
|    33610 | 1562 | `	SyStringInitFromBuf(&pAttr->sTypeName,zTypeName,SyStrlen(zTypeName));` |
|    33610 | 1563 | `	return PH7_ClassInstallAttr(pClass,pAttr);` |
|    16785 | 1564 | `}` |
|        - | 1565 | `/*` |
|        - | 1566 | ` * cases()/from()/tryFrom(), installed on ANY enum -- one declared from C here` |
|        - | 1567 | ` * and one the compiler just finished reading from source. php declares them on` |
|        - | 1568 | ``  * the UnitEnum/BackedEnum prototypes, which is why `from` takes `string\|int` `` |
|        - | 1569 | ` * rather than the enum's own backing type: the refusal for the wrong one is a` |
|        - | 1570 | ` * VALUE check inside the body, not the parameter's.` |
|        - | 1571 | ` *` |
|        - | 1572 | ` * The compiler used to synthesize PHP source forwarding to three global` |
|        - | 1573 | `` * `__phl_enum_*` thunks. Those names were php-visible, and the methods reported`` |
|        - | 1574 | `` * as `<user>` with the enum's file and line where php reports`` |
|        - | 1575 | `` * `<internal, prototype BackedEnum>`.`` |
|        - | 1576 | ` */` |
|        - | 1577 | `/*` |
|        - | 1578 | ` * Install one of them and stamp it INTERNAL unconditionally. The usual` |
|        - | 1579 | `` * `bCompilingBuiltin` stamp only fires while the engine compiles its OWN`` |
|        - | 1580 | ` * sources, and these three are attached to a class the compiler is reading out` |
|        - | 1581 | ` * of a USER file — but php reports them as internal wherever the enum is` |
|        - | 1582 | ` * declared: isInternal() true, getFileName() false, getStartLine() 0.` |
|        - | 1583 | ` */` |
|    40582 | 1584 | `static sxi32 NativeEnumInstallMethod(ph7_vm *pVm,ph7_class *pClass,` |
|        - | 1585 | `	const PH7_NativeMethodDef *pDef)` |
|        5 | 1586 | `{` |
|        - | 1587 | `	ph7_class_method *pMeth;` |
|    40587 | 1588 | `	sxi32 rc = PH7_NativeClassInstallMethod(&(*pVm),pClass,pDef,0);` |
|    40587 | 1589 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1590 | `		return rc;` |
|        - | 1591 | `	}` |
|    40587 | 1592 | `	pMeth = PH7_ClassExtractMethod(pClass,pDef->zName,(sxu32)SyStrlen(pDef->zName));` |
|    40587 | 1593 | `	if( pMeth ){` |
|    40587 | 1594 | `		pMeth->sFunc.iFlags \|= VM_FUNC_INTERNAL;` |
|        - | 1595 | `		/* getFileName() reads the recorded source file rather than the flag, and` |
|        - | 1596 | `		 * PH7_NewClassMethod stamped the user file the enum was read from. */` |
|    40587 | 1597 | `		SyStringInitFromBuf(&pMeth->sFunc.sFile,"",0);` |
|    20264 | 1598 | `	}` |
|    40587 | 1599 | `	return SXRET_OK;` |
|    20269 | 1600 | `}` |
|    27012 | 1601 | `PH7_PRIVATE sxi32 PH7_InstallEnumInterfaceMethods(ph7_vm *pVm,ph7_class *pClass)` |
|        5 | 1602 | `{` |
|        - | 1603 | `	static const PH7_NativeMethodDef sCases =` |
|        - | 1604 | `		{ "cases", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "", "array", vm_builtin_NativeEnum_cases };` |
|        - | 1605 | `	static const PH7_NativeMethodDef aFrom[] = {` |
|        - | 1606 | `		{ "from",    PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "string\|int $value", "static",` |
|        - | 1607 | `		  vm_builtin_NativeEnum_from },` |
|        - | 1608 | `		{ "tryFrom", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "string\|int $value", "?static",` |
|        - | 1609 | `		  vm_builtin_NativeEnum_tryFrom },` |
|        - | 1610 | `	};` |
|        - | 1611 | `	sxu32 n;` |
|    27017 | 1612 | `	sxi32 rc = NativeEnumInstallMethod(&(*pVm),pClass,&sCases);` |
|    27017 | 1613 | `	if( rc != SXRET_OK \|\| pClass->nEnumBacking == 0 ){` |
|    20232 | 1614 | `		return rc;` |
|        - | 1615 | `	}` |
|    20360 | 1616 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFrom) ; n++ ){` |
|    13575 | 1617 | `		rc = NativeEnumInstallMethod(&(*pVm),pClass,&aFrom[n]);` |
|    13575 | 1618 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1619 | `			return rc;` |
|        - | 1620 | `		}` |
|     6781 | 1621 | `	}` |
|     6790 | 1622 | `	return SXRET_OK;` |
|    13493 | 1623 | `}` |
|    26884 | 1624 | `PH7_PRIVATE sxi32 PH7_InstallNativeEnum(ph7_vm *pVm,const char *zName,sxu32 nBacking,` |
|        - | 1625 | `	const PH7_NativeEnumCase *aCase,sxu32 nCase,` |
|        - | 1626 | `	const PH7_NativeMethodDef *aMethod,sxu32 nMethod)` |
|        5 | 1627 | `{` |
|        - | 1628 | `	ph7_class *pClass, *pIface;` |
|        - | 1629 | `	SyString sName;` |
|        - | 1630 | `	sxu32 n;` |
|        - | 1631 | `	sxi32 rc;` |
|    26889 | 1632 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|    26889 | 1633 | `	pClass = PH7_NewRawClass(&(*pVm),&sName,0);` |
|    26889 | 1634 | `	if( pClass == 0 ){` |
|      ! 0 | 1635 | `		return SXERR_MEM;` |
|        - | 1636 | `	}` |
|        - | 1637 | `	/* php: no enum can be extended or instantiated, and the ENUM flag alone says` |
|        - | 1638 | ``	 * so -- the `extends` refusal names the enum rather than a final class, and it`` |
|        - | 1639 | `	 * is asked first. The FINAL flag is deliberately NOT set: php stamps` |
|        - | 1640 | `` 	 * ZEND_ACC_FINAL on a COMPILED enum only, so `isFinal()`/`getModifiers()` `` |
|        - | 1641 | ``	 * answer true/32 for `enum U {}` and false/0 for every enum php declares from`` |
|        - | 1642 | `	 * C (RoundingMode, PropertyHookType). Setting it here made an internal enum` |
|        - | 1643 | `	 * report itself as a userland one. */` |
|    26889 | 1644 | `	pClass->iFlags \|= PH7_CLASS_ENUM;` |
|    26889 | 1645 | `	pClass->nEnumBacking = nBacking;` |
|    26889 | 1646 | `	rc = NativeEnumInstallProp(&(*pVm),pClass,"name",MEMOBJ_STRING,"string");` |
|    26889 | 1647 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1648 | `		return rc;` |
|        - | 1649 | `	}` |
|    26889 | 1650 | `	if( nBacking != 0 ){` |
|    10082 | 1651 | `		rc = NativeEnumInstallProp(&(*pVm),pClass,"value",nBacking,` |
|     3356 | 1652 | `			nBacking == MEMOBJ_INT ? "int" : "string");` |
|     6726 | 1653 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1654 | `			return rc;` |
|        - | 1655 | `		}` |
|     3356 | 1656 | `	}` |
|   154588 | 1657 | `	for( n = 0 ; n < nCase ; n++ ){` |
|        - | 1658 | `		ph7_class_attr *pAttr;` |
|        - | 1659 | `		SyString sCase;` |
|   127704 | 1660 | `		SyStringInitFromBuf(&sCase,aCase[n].zName,SyStrlen(aCase[n].zName));` |
|   127704 | 1661 | `		pAttr = PH7_NewClassAttr(&(*pVm),&sCase,0,PH7_CLASS_PROT_PUBLIC,` |
|        - | 1662 | `			PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|   127704 | 1663 | `		if( pAttr == 0 ){` |
|      ! 0 | 1664 | `			return SXERR_MEM;` |
|        - | 1665 | `		}` |
|   127704 | 1666 | `		pAttr->pDeclClass = pClass;` |
|        - | 1667 | `		/* The backing literal where a compiled case carries byte-code. */` |
|   127704 | 1668 | `		if( nBacking != 0 ){` |
|    13447 | 1669 | `			pAttr->pNativeValue = &aCase[n].sValue;` |
|     6712 | 1670 | `		}` |
|   127704 | 1671 | `		rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   127704 | 1672 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1673 | `			return rc;` |
|        - | 1674 | `		}` |
|        - | 1675 | `		/* Declaration order, which is the order cases() reports. */` |
|   127704 | 1676 | `		SySetPut(&pClass->aEnumCases,(const void *)&pAttr);` |
|    63769 | 1677 | `	}` |
|    26889 | 1678 | `	for( n = 0 ; n < nMethod ; n++ ){` |
|      ! 0 | 1679 | `		rc = PH7_NativeClassInstallMethod(&(*pVm),pClass,&aMethod[n],0);` |
|      ! 0 | 1680 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1681 | `			return rc;` |
|        - | 1682 | `		}` |
|      ! 0 | 1683 | `	}` |
|    26889 | 1684 | `	rc = PH7_InstallEnumInterfaceMethods(&(*pVm),pClass);` |
|    26889 | 1685 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1686 | `		return rc;` |
|        - | 1687 | `	}` |
|    26889 | 1688 | `	rc = PH7_VmInstallClass(&(*pVm),pClass);` |
|    26889 | 1689 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1690 | `		return rc;` |
|        - | 1691 | `	}` |
|        - | 1692 | ``	/* php 8.1: every enum satisfies `instanceof UnitEnum`, a backed one`` |
|        - | 1693 | ``	 * `BackedEnum` too. Attached AFTER the methods, so PH7_ClassImplement's`` |
|        - | 1694 | `	 * abstract stubbing finds cases()/from()/tryFrom() already declared.` |
|        - | 1695 | `	 * A backed one names only BackedEnum, which BRINGS UnitEnum: php's own` |
|        - | 1696 | ``	 * internal enums list `BackedEnum, UnitEnum` in that order, and naming`` |
|        - | 1697 | `	 * both here would answer them the other way round. A compiled enum is a` |
|        - | 1698 | ``	 * different registration and really does name both (`Ct, UnitEnum,`` |
|        - | 1699 | ``	 * BackedEnum`), which is what compile_class.c spells. */`` |
|    26889 | 1700 | `	pIface = NativeLookupClass(&(*pVm),nBacking != 0 ? "BackedEnum" : "UnitEnum");` |
|    26889 | 1701 | `	if( pIface == 0 ){` |
|      ! 0 | 1702 | `		return SXERR_NOTFOUND;` |
|        - | 1703 | `	}` |
|    26889 | 1704 | `	rc = PH7_ClassImplement(pClass,pIface);` |
|    26889 | 1705 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1706 | `		return rc;` |
|        - | 1707 | `	}` |
|    26889 | 1708 | `	return VmMountUserClass(&(*pVm),pClass);` |
|    13429 | 1709 | `}` |
|        - | 1710 | `/*` |
|        - | 1711 | ` * ---------------------------------------------------------------------------` |
|        - | 1712 | ` * InternalIterator.` |
|        - | 1713 | ` *` |
|        - | 1714 | ` * A native IteratorAggregate cannot answer a Generator: a PHP generator IS its` |
|        - | 1715 | ` * byte-code, and a C body has none. php never answers one either -- its internal` |
|        - | 1716 | ` * aggregates hand back an InternalIterator wrapping the iterator their class` |
|        - | 1717 | ` * declared -- so PHL declares that class once, here, and every native aggregate` |
|        - | 1718 | ` * reaches it through ph7_class::pIterVtab.` |
|        - | 1719 | ` *` |
|        - | 1720 | ` * The cursor lives entirely in the iterator's own private slots. A vtable states` |
|        - | 1721 | ` * only how to REACH a position; reading it back is the same three methods for` |
|        - | 1722 | ` * everyone.` |
|        - | 1723 | ` * ---------------------------------------------------------------------------` |
|        - | 1724 | ` */` |
|        - | 1725 | `/*` |
|        - | 1726 | ` * The walk THIS iterator was made for: the vtable of the aggregate it holds,` |
|        - | 1727 | ` * looked up ALONG THE BASE CHAIN. A subclass of a native aggregate inherits the` |
|        - | 1728 | ` * walk the way it inherits the getIterator() that reaches it -- without this a` |
|        - | 1729 | `` * `class P extends DatePeriod {}` (or DOMNodeList, WeakMap, PDOStatement,`` |
|        - | 1730 | ` * FilesystemIterator) answered a real InternalIterator that yielded NOTHING, so` |
|        - | 1731 | ` * every foreach over one was silently empty.` |
|        - | 1732 | ` */` |
|     6034 | 1733 | `static const PH7_NativeIterVtab * NativeIterVtab(ph7_class_instance *pIt)` |
|        2 | 1734 | `{` |
|     6036 | 1735 | `	ph7_class_instance *pSrc = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);` |
|        - | 1736 | `	ph7_class *pClass;` |
|     6256 | 1737 | `	for( pClass = pSrc ? pSrc->pClass : 0 ; pClass ; pClass = pClass->pBase ){` |
|     6256 | 1738 | `		if( pClass->pIterVtab ){` |
|     6036 | 1739 | `			return pClass->pIterVtab;` |
|        - | 1740 | `		}` |
|      111 | 1741 | `	}` |
|      ! 0 | 1742 | `	return 0;` |
|     3019 | 1743 | `}` |
|        - | 1744 | `/* Hand the cursor back to the aggregate, for a class that shows its walk as one` |
|        - | 1745 | ` * of its own properties (see PH7_NativeIterVtab::xPublish). Every InternalIterator` |
|        - | 1746 | ` * method calls this -- php's aggregate is written from the iterator's methods, not` |
|        - | 1747 | ` * from the walk, so a getIterator() nobody has touched yet leaves it alone. */` |
|     2448 | 1748 | `static void NativeIterPublish(ph7_vm *pVm,ph7_class_instance *pIt)` |
|        2 | 1749 | `{` |
|     2450 | 1750 | `	const PH7_NativeIterVtab *pVtab = NativeIterVtab(pIt);` |
|     2450 | 1751 | `	if( pVtab && pVtab->xPublish ){` |
|      949 | 1752 | `		pVtab->xPublish(&(*pVm),pIt);` |
|      474 | 1753 | `	}` |
|     2450 | 1754 | `}` |
|        - | 1755 | `/* May this iterator be walked? See PH7_NativeIterVtab::xGuard -- the aggregate` |
|        - | 1756 | ` * gets to refuse at each of the five methods, which is where php refuses. */` |
|     2462 | 1757 | `static int NativeIterRefused(ph7_context *pCtx,ph7_class_instance *pIt)` |
|        2 | 1758 | `{` |
|     2464 | 1759 | `	const PH7_NativeIterVtab *pVtab = NativeIterVtab(pIt);` |
|     2464 | 1760 | `	return (pVtab && pVtab->xGuard) ? pVtab->xGuard(pCtx,pIt) : 0;` |
|        2 | 1761 | `}` |
|      268 | 1762 | `static int vm_builtin_InternalIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 1763 | `{` |
|      270 | 1764 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|        - | 1765 | `	const PH7_NativeIterVtab *pVtab;` |
|      134 | 1766 | `	SXUNUSED(nArg);` |
|      134 | 1767 | `	SXUNUSED(apArg);` |
|      270 | 1768 | `	if( pThis == 0 ){` |
|      ! 0 | 1769 | `		return PH7_OK;` |
|        - | 1770 | `	}` |
|      270 | 1771 | `	if( NativeIterRefused(pCtx,pThis) ){` |
|       15 | 1772 | `		return PH7_OK;` |
|        - | 1773 | `	}` |
|      256 | 1774 | `	pVtab = NativeIterVtab(pThis);` |
|      256 | 1775 | `	if( pVtab == 0 \|\| pVtab->xRewind == 0 ){` |
|      ! 0 | 1776 | `		PH7_NativeSetAttrBool(pCtx->pVm,pThis,PH7_NATIVE_IT_DONE,1);` |
|      ! 0 | 1777 | `		return PH7_OK;` |
|        - | 1778 | `	}` |
|      256 | 1779 | `	pVtab->xRewind(pCtx->pVm,pThis);` |
|      256 | 1780 | `	NativeIterPublish(pCtx->pVm,pThis);` |
|      256 | 1781 | `	return PH7_OK;` |
|      136 | 1782 | `}` |
|      576 | 1783 | `static int vm_builtin_InternalIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 1784 | `{` |
|      578 | 1785 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|        - | 1786 | `	const PH7_NativeIterVtab *pVtab;` |
|      288 | 1787 | `	SXUNUSED(nArg);` |
|      288 | 1788 | `	SXUNUSED(apArg);` |
|      578 | 1789 | `	if( pThis && NativeIterRefused(pCtx,pThis) ){` |
|      ! 0 | 1790 | `		return PH7_OK;` |
|        - | 1791 | `	}` |
|      578 | 1792 | `	if( pThis == 0 \|\| PH7_NativeAttrTruthy(pThis,PH7_NATIVE_IT_DONE) ){` |
|      ! 0 | 1793 | `		return PH7_OK;` |
|        - | 1794 | `	}` |
|      578 | 1795 | `	pVtab = NativeIterVtab(pThis);` |
|      578 | 1796 | `	if( pVtab == 0 \|\| pVtab->xNext == 0 ){` |
|      ! 0 | 1797 | `		PH7_NativeSetAttrBool(pCtx->pVm,pThis,PH7_NATIVE_IT_DONE,1);` |
|      ! 0 | 1798 | `		return PH7_OK;` |
|        - | 1799 | `	}` |
|      578 | 1800 | `	pVtab->xNext(pCtx->pVm,pThis);` |
|      578 | 1801 | `	NativeIterPublish(pCtx->pVm,pThis);` |
|      578 | 1802 | `	return PH7_OK;` |
|      290 | 1803 | `}` |
|      822 | 1804 | `static int vm_builtin_InternalIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 1805 | `{` |
|      824 | 1806 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      411 | 1807 | `	SXUNUSED(nArg);` |
|      411 | 1808 | `	SXUNUSED(apArg);` |
|      824 | 1809 | `	if( pThis && NativeIterRefused(pCtx,pThis) ){` |
|      ! 0 | 1810 | `		return PH7_OK;` |
|        - | 1811 | `	}` |
|      824 | 1812 | `	if( pThis ){` |
|      824 | 1813 | `		NativeIterPublish(pCtx->pVm,pThis);` |
|      411 | 1814 | `	}` |
|      824 | 1815 | `	ph7_result_bool(pCtx,pThis != 0 && !PH7_NativeAttrTruthy(pThis,PH7_NATIVE_IT_DONE));` |
|      824 | 1816 | `	return PH7_OK;` |
|      413 | 1817 | `}` |
|        - | 1818 | `/* current() and key() answer the slots the walk left behind -- and NULL once it` |
|        - | 1819 | ` * is over, which is what php's exhausted InternalIterator answers too. */` |
|      796 | 1820 | `static int NativeIterReadSlot(ph7_context *pCtx,const char *zSlot)` |
|        2 | 1821 | `{` |
|      798 | 1822 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|        - | 1823 | `	ph7_value *pVal;` |
|      798 | 1824 | `	if( pThis && NativeIterRefused(pCtx,pThis) ){` |
|      ! 0 | 1825 | `		return PH7_OK;` |
|        - | 1826 | `	}` |
|      798 | 1827 | `	if( pThis ){` |
|      798 | 1828 | `		NativeIterPublish(pCtx->pVm,pThis);` |
|      398 | 1829 | `	}` |
|      798 | 1830 | `	if( pThis == 0 \|\| PH7_NativeAttrTruthy(pThis,PH7_NATIVE_IT_DONE) ){` |
|      ! 0 | 1831 | `		ph7_result_null(pCtx);` |
|      ! 0 | 1832 | `		return PH7_OK;` |
|        - | 1833 | `	}` |
|      798 | 1834 | `	pVal = PH7_NativeAttr(pThis,zSlot);` |
|      798 | 1835 | `	if( pVal ){` |
|      798 | 1836 | `		ph7_result_value(pCtx,pVal);` |
|      398 | 1837 | `	}` |
|      798 | 1838 | `	return PH7_OK;` |
|      400 | 1839 | `}` |
|      572 | 1840 | `static int vm_builtin_InternalIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 1841 | `{` |
|      286 | 1842 | `	SXUNUSED(nArg);` |
|      286 | 1843 | `	SXUNUSED(apArg);` |
|      574 | 1844 | `	return NativeIterReadSlot(pCtx,PH7_NATIVE_IT_CUR);` |
|        2 | 1845 | `}` |
|      224 | 1846 | `static int vm_builtin_InternalIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 1847 | `{` |
|      112 | 1848 | `	SXUNUSED(nArg);` |
|      112 | 1849 | `	SXUNUSED(apArg);` |
|      226 | 1850 | `	return NativeIterReadSlot(pCtx,PH7_NATIVE_IT_KEY);` |
|        2 | 1851 | `}` |
|        - | 1852 | `/* InternalIterator::__construct() — private in php, and never reached from PHP:` |
|        - | 1853 | ` * PH7_NativeIteratorNew builds the instance directly. */` |
|      ! 0 | 1854 | `static int vm_builtin_InternalIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 1855 | `{` |
|      ! 0 | 1856 | `	SXUNUSED(nArg);` |
|      ! 0 | 1857 | `	SXUNUSED(apArg);` |
|      ! 0 | 1858 | `	SXUNUSED(pCtx);` |
|      ! 0 | 1859 | `	return PH7_OK;` |
|      ! 0 | 1860 | `}` |
|        - | 1861 | `/*` |
|        - | 1862 | ` * The iterator a native getIterator() answers: bound to its aggregate and already` |
|        - | 1863 | ` * positioned, because php's is valid() before the first rewind().` |
|        - | 1864 | ` */` |
|      294 | 1865 | `PH7_PRIVATE ph7_class_instance * PH7_NativeIteratorNew(ph7_vm *pVm,ph7_class_instance *pSrc)` |
|        2 | 1866 | `{` |
|      296 | 1867 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),"InternalIterator",` |
|        - | 1868 | `		sizeof("InternalIterator")-1,FALSE,0);` |
|        - | 1869 | `	ph7_class_instance *pIt;` |
|        - | 1870 | `	const PH7_NativeIterVtab *pVtab;` |
|      296 | 1871 | `	if( pClass == 0 \|\| pSrc == 0 ){` |
|      ! 0 | 1872 | `		return 0;` |
|        - | 1873 | `	}` |
|      296 | 1874 | `	pIt = PH7_NewClassInstance(&(*pVm),pClass);` |
|      296 | 1875 | `	if( pIt == 0 ){` |
|      ! 0 | 1876 | `		return 0;` |
|        - | 1877 | `	}` |
|      296 | 1878 | `	PH7_NativeSetAttrObj(&(*pVm),pIt,PH7_NATIVE_IT_SRC,pSrc);` |
|      296 | 1879 | `	PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|      296 | 1880 | `	pVtab = NativeIterVtab(pIt);` |
|      296 | 1881 | `	if( pVtab && pVtab->xRewind ){` |
|      296 | 1882 | `		pVtab->xRewind(&(*pVm),pIt);` |
|      147 | 1883 | `	}` |
|      296 | 1884 | `	return pIt;` |
|      149 | 1885 | `}` |
|     6721 | 1886 | `PH7_PRIVATE sxi32 PH7_VmInstallNativeIterator(ph7_vm *pVm)` |
|        5 | 1887 | `{` |
|        - | 1888 | `	static const PH7_NativePropDef aProp[] = {` |
|        - | 1889 | `		{ PH7_NATIVE_IT_SRC,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|        - | 1890 | `		{ PH7_NATIVE_IT_CUR,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|        - | 1891 | `		{ PH7_NATIVE_IT_KEY,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|        - | 1892 | `		{ PH7_NATIVE_IT_POS,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,  0, 0, 0.0 }, 0 },` |
|        - | 1893 | `		{ PH7_NATIVE_IT_AUX,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,  0, 0, 0.0 }, 0 },` |
|        - | 1894 | `		{ PH7_NATIVE_IT_DONE, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_BOOL, 1, 0, 0.0 }, 0 },` |
|        - | 1895 | `	};` |
|        - | 1896 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|        - | 1897 | `		{ "__construct", PH7_MOD_PRIVATE, "", "", vm_builtin_InternalIterator_construct },` |
|        - | 1898 | `		{ "current",     PH7_MOD_PUBLIC, "", "mixed", vm_builtin_InternalIterator_current },` |
|        - | 1899 | `		{ "key",         PH7_MOD_PUBLIC, "", "mixed", vm_builtin_InternalIterator_key },` |
|        - | 1900 | `		{ "next",        PH7_MOD_PUBLIC, "", "void",  vm_builtin_InternalIterator_next },` |
|        - | 1901 | `		{ "valid",       PH7_MOD_PUBLIC, "", "bool",  vm_builtin_InternalIterator_valid },` |
|        - | 1902 | `		{ "rewind",      PH7_MOD_PUBLIC, "", "void",  vm_builtin_InternalIterator_rewind },` |
|        - | 1903 | `	};` |
|        - | 1904 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|        - | 1905 | `		{ "InternalIterator", 0, 0, PH7_CLASS_FINAL,` |
|        - | 1906 | `		  aMethod, SX_ARRAYSIZE(aMethod), 0, 0, aProp, SX_ARRAYSIZE(aProp), 0, 0, 0 },` |
|        - | 1907 | `	};` |
|        - | 1908 | `	ph7_class *pIt,*pIterator;` |
|        - | 1909 | `	sxi32 rc;` |
|     6726 | 1910 | `	rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     6726 | 1911 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1912 | `		return rc;` |
|        - | 1913 | `	}` |
|        - | 1914 | ``	/* `implements Iterator` only NOW: PH7_ClassImplement stubs every interface`` |
|        - | 1915 | `	 * method the class does not already declare as ABSTRACT, so attaching it` |
|        - | 1916 | `	 * through the spec would have made InternalIterator uninstantiable. */` |
|     6726 | 1917 | `	pIt = NativeLookupClass(&(*pVm),"InternalIterator");` |
|     6726 | 1918 | `	pIterator = NativeLookupClass(&(*pVm),"Iterator");` |
|     6726 | 1919 | `	if( pIt == 0 \|\| pIterator == 0 ){` |
|      ! 0 | 1920 | `		return SXERR_NOTFOUND;` |
|        - | 1921 | `	}` |
|     6726 | 1922 | `	return PH7_ClassImplement(pIt,pIterator);` |
|     3361 | 1923 | `}` |
|        - | 1924 |  |
