# src/ph7/oo_native.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 604/693 lines (87.16%)

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
| 9833096 |   36 | `PH7_PRIVATE void PH7_NativeLiteralValue(ph7_vm *pVm,const void *pLiteral,ph7_value *pOut)` |
|       5 |   37 | `{` |
| 9833101 |   38 | `	const PH7_NativeConstDef *pLit = (const PH7_NativeConstDef *)pLiteral;` |
|       - |   39 | `	/* Every PH7_MemObjInitFrom* below SyZeros the whole value, nIdx included — and` |
|       - |   40 | `	 * a CONSTANT's or STATIC's slot is addressed by that index afterwards` |
|       - |   41 | ``	 * (`pAttr->nIdx = pMemObj->nIdx`). Losing it pointed every native class`` |
|       - |   42 | `	 * constant at slot 0, which reads as $GLOBALS. The instance-property path was` |
|       - |   43 | `	 * unaffected (it records the index before calling this), which is why nothing` |
|       - |   44 | `	 * saw it until the date family declared the first native constants. */` |
| 9833101 |   45 | `	sxu32 nSlot = pOut->nIdx;` |
| 9833101 |   46 | `	switch( pLit->iType ){` |
| 1234307 |   47 | `		case PH7_NATIVE_VAL_INT:` |
| 2468618 |   48 | `			PH7_MemObjInitFromInt(&(*pVm),pOut,pLit->iValue);` |
| 2468618 |   49 | `			break;` |
|    6891 |   50 | `		case PH7_NATIVE_VAL_BOOL:` |
|   13787 |   51 | `			PH7_MemObjInitFromBool(&(*pVm),pOut,(sxi32)pLit->iValue);` |
|   13787 |   52 | `			break;` |
| 2188709 |   53 | `		case PH7_NATIVE_VAL_STRING: {` |
|       - |   54 | `			SyString sLit;` |
| 4377420 |   55 | `			SyStringInitFromBuf(&sLit,pLit->zValue,SyStrlen(pLit->zValue));` |
| 4377420 |   56 | `			PH7_MemObjInitFromString(&(*pVm),pOut,&sLit);` |
| 4377420 |   57 | `			break;` |
|       - |   58 | `		}` |
|       - |   59 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      92 |   60 | `		case PH7_NATIVE_VAL_DOUBLE:` |
|     186 |   61 | `			PH7_MemObjInitFromReal(&(*pVm),pOut,pLit->rValue);` |
|     186 |   62 | `			break;` |
|       - |   63 | `#endif` |
|  728647 |   64 | `		case PH7_NATIVE_VAL_ARRAY: {` |
|       - |   65 | ``			/* The empty array — php's `private array $trace = [];`. Each instance`` |
|       - |   66 | `			 * needs its OWN map (the exception's trace is written per throw), so` |
|       - |   67 | `			 * this allocates rather than sharing one. */` |
| 1457298 |   68 | `			ph7_hashmap *pMap = PH7_NewHashmap(&(*pVm),0,0);` |
| 1457298 |   69 | `			if( pMap == 0 ){` |
|     ! 0 |   70 | `				PH7_MemObjInit(&(*pVm),pOut);` |
|     ! 0 |   71 | `			}else{` |
| 1457298 |   72 | `				PH7_MemObjInitFromArray(&(*pVm),pOut,pMap);` |
|       - |   73 | `			}` |
| 1457298 |   74 | `			break;` |
|       - |   75 | `		}` |
|  757905 |   76 | `		default:` |
| 1515814 |   77 | `			PH7_MemObjInit(&(*pVm),pOut);` |
| 1515809 |   78 | `			break;` |
|       - |   79 | `	}` |
| 9833101 |   80 | `	pOut->nIdx = nSlot;` |
| 9833101 |   81 | `}` |
|       - |   82 | `/*` |
|       - |   83 | ` * Write a declared property of an instance from C.` |
|       - |   84 | ` *` |
|       - |   85 | ` * Every native class that hands an OBJECT back to PHP has to fill one in, and the` |
|       - |   86 | ` * write has to go through the instance's own slot table (hAttr -> VmClassAttr ->` |
|       - |   87 | ` * pVm->aMemObj) rather than the class declaration, or the value lands nowhere.` |
|       - |   88 | ` * Silently does nothing for a name the class does not declare -- callers pass` |
|       - |   89 | ` * literals from their own spec table, so a miss is a build error, not input.` |
|       - |   90 | ` */` |
|    7304 |   91 | `PH7_PRIVATE void PH7_NativeSetProp(ph7_vm *pVm,ph7_class_instance *pObj,` |
|       - |   92 | `	const char *zProp,sxu32 nProp,ph7_value *pSrcVal)` |
|       5 |   93 | `{` |
|    7309 |   94 | `	SyHashEntry *pEntry = SyHashGet(&pObj->hAttr,(const void *)zProp,nProp);` |
|       - |   95 | `	VmClassAttr *pVmAttr;` |
|       - |   96 | `	ph7_value *pSlot;` |
|    7309 |   97 | `	if( pEntry == 0 ){` |
|     ! 0 |   98 | `		return;` |
|       - |   99 | `	}` |
|    7309 |  100 | `	pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    7309 |  101 | `	pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,pVmAttr->nIdx);` |
|    7309 |  102 | `	if( pSlot == 0 ){` |
|     ! 0 |  103 | `		return;` |
|       - |  104 | `	}` |
|    7309 |  105 | `	PH7_MemObjStore(pSrcVal,pSlot);` |
|    7309 |  106 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|    3657 |  107 | `}` |
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
| 1610217 |  120 | `PH7_PRIVATE ph7_value * PH7_NativeAttr(ph7_class_instance *pObj,const char *zName)` |
|       5 |  121 | `{` |
|       - |  122 | `	SyString sName;` |
| 1610222 |  123 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
| 1610222 |  124 | `	return PH7_ClassInstanceFetchAttr(pObj,&sName);` |
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
|   19426 |  141 | `PH7_PRIVATE sxi64 PH7_NativeAttrInt(ph7_class_instance *pObj,const char *zName)` |
|       5 |  142 | `{` |
|   19431 |  143 | `	ph7_value *pVal = PH7_NativeAttr(pObj,zName);` |
|   19431 |  144 | `	if( pVal && (pVal->iFlags & MEMOBJ_INT) ){` |
|   19431 |  145 | `		return pVal->x.iVal;` |
|       - |  146 | `	}` |
|     ! 0 |  147 | `	return 0;` |
|    9719 |  148 | `}` |
|       - |  149 | `/* Borrow a string slot's bytes (empty when it holds anything else). */` |
|    9924 |  150 | `PH7_PRIVATE void PH7_NativeAttrStr(ph7_class_instance *pObj,const char *zName,` |
|       - |  151 | `	const char **pzOut,int *pnOut)` |
|       5 |  152 | `{` |
|    9929 |  153 | `	ph7_value *pVal = PH7_NativeAttr(pObj,zName);` |
|    9929 |  154 | `	*pzOut = "";` |
|    9929 |  155 | `	*pnOut = 0;` |
|    9929 |  156 | `	if( pVal && (pVal->iFlags & MEMOBJ_STRING) ){` |
|    9919 |  157 | `		*pzOut = (const char *)SyBlobData(&pVal->sBlob);` |
|    9919 |  158 | `		*pnOut = (int)SyBlobLength(&pVal->sBlob);` |
|    4957 |  159 | `	}` |
|    9929 |  160 | `}` |
|       - |  161 | `/* The object stored in a slot, or NULL when it holds anything else. */` |
|   21242 |  162 | `PH7_PRIVATE ph7_class_instance * PH7_NativeAttrObj(ph7_class_instance *pObj,const char *zName)` |
|       5 |  163 | `{` |
|   21247 |  164 | `	ph7_value *pVal = PH7_NativeAttr(pObj,zName);` |
|   21247 |  165 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    3243 |  166 | `		return 0;` |
|       - |  167 | `	}` |
|   18009 |  168 | `	return (ph7_class_instance *)pVal->x.pOther;` |
|   10626 |  169 | `}` |
|       - |  170 | `/* Truth of a bool/int slot, again without converting it. */` |
|   11654 |  171 | `PH7_PRIVATE int PH7_NativeAttrTruthy(ph7_class_instance *pObj,const char *zName)` |
|       5 |  172 | `{` |
|   11659 |  173 | `	ph7_value *pVal = PH7_NativeAttr(pObj,zName);` |
|   11659 |  174 | `	if( pVal && (pVal->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|   11659 |  175 | `		return pVal->x.iVal != 0;` |
|       - |  176 | `	}` |
|     ! 0 |  177 | `	return 0;` |
|    5832 |  178 | `}` |
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
| 1488522 |  191 | `static void NativeAttrMarkInit(ph7_class_instance *pObj,const char *zName)` |
|       5 |  192 | `{` |
| 1488527 |  193 | `	SyHashEntry *pEntry = SyHashGet(&pObj->hAttr,(const void *)zName,SyStrlen(zName));` |
| 1488527 |  194 | `	if( pEntry ){` |
| 1488527 |  195 | `		((VmClassAttr *)pEntry->pUserData)->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|  744263 |  196 | `	}` |
| 1488527 |  197 | `}` |
|   11341 |  198 | `PH7_PRIVATE void PH7_NativeSetAttrInt(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,sxi64 iVal)` |
|       5 |  199 | `{` |
|   11346 |  200 | `	ph7_value *pSlot = PH7_NativeAttr(pObj,zName);` |
|       - |  201 | `	ph7_value sVal;` |
|   11346 |  202 | `	if( pSlot == 0 ){` |
|     ! 0 |  203 | `		return;` |
|       - |  204 | `	}` |
|   11346 |  205 | `	PH7_MemObjInitFromInt(&(*pVm),&sVal,iVal);` |
|   11346 |  206 | `	PH7_MemObjStore(&sVal,pSlot);` |
|   11346 |  207 | `	PH7_MemObjRelease(&sVal);` |
|   11346 |  208 | `	NativeAttrMarkInit(pObj,zName);` |
|    5676 |  209 | `}` |
|       - |  210 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      56 |  211 | `PH7_PRIVATE void PH7_NativeSetAttrReal(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,ph7_real rVal)` |
|       1 |  212 | `{` |
|      57 |  213 | `	ph7_value *pSlot = PH7_NativeAttr(pObj,zName);` |
|       - |  214 | `	ph7_value sVal;` |
|      57 |  215 | `	if( pSlot == 0 ){` |
|     ! 0 |  216 | `		return;` |
|       - |  217 | `	}` |
|      57 |  218 | `	PH7_MemObjInitFromReal(&(*pVm),&sVal,rVal);` |
|      57 |  219 | `	PH7_MemObjStore(&sVal,pSlot);` |
|      57 |  220 | `	PH7_MemObjRelease(&sVal);` |
|      57 |  221 | `	NativeAttrMarkInit(pObj,zName);` |
|      29 |  222 | `}` |
|       - |  223 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
| 1466863 |  224 | `PH7_PRIVATE void PH7_NativeSetAttrStr(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,` |
|       - |  225 | `	const char *zVal,int nVal)` |
|       5 |  226 | `{` |
| 1466868 |  227 | `	ph7_value *pSlot = PH7_NativeAttr(pObj,zName);` |
|       - |  228 | `	ph7_value sVal;` |
|       - |  229 | `	SyString sStr;` |
| 1466868 |  230 | `	if( pSlot == 0 ){` |
|     ! 0 |  231 | `		return;` |
|       - |  232 | `	}` |
| 1466868 |  233 | `	SyStringInitFromBuf(&sStr,zVal,nVal);` |
| 1466868 |  234 | `	PH7_MemObjInitFromString(&(*pVm),&sVal,&sStr);` |
| 1466868 |  235 | `	PH7_MemObjStore(&sVal,pSlot);` |
| 1466868 |  236 | `	PH7_MemObjRelease(&sVal);	NativeAttrMarkInit(pObj,zName);` |
|  733438 |  237 | `}` |
|    2430 |  238 | `PH7_PRIVATE void PH7_NativeSetAttrBool(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,int bVal)` |
|       4 |  239 | `{` |
|    2434 |  240 | `	ph7_value *pSlot = PH7_NativeAttr(pObj,zName);` |
|       - |  241 | `	ph7_value sVal;` |
|    2434 |  242 | `	if( pSlot == 0 ){` |
|     ! 0 |  243 | `		return;` |
|       - |  244 | `	}` |
|    2434 |  245 | `	PH7_MemObjInitFromBool(&(*pVm),&sVal,bVal);` |
|    2434 |  246 | `	PH7_MemObjStore(&sVal,pSlot);` |
|    2434 |  247 | `	PH7_MemObjRelease(&sVal);	NativeAttrMarkInit(pObj,zName);` |
|    1219 |  248 | `}` |
|       - |  249 | `/* Store an object in a slot, or NULL to clear it. PH7_MemObjStore takes the` |
|       - |  250 | ` * reference the slot needs, so the temp never holds one of its own. */` |
|    7832 |  251 | `PH7_PRIVATE void PH7_NativeSetAttrObj(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,` |
|       - |  252 | `	ph7_class_instance *pVal)` |
|       5 |  253 | `{` |
|    7837 |  254 | `	ph7_value *pSlot = PH7_NativeAttr(pObj,zName);` |
|       - |  255 | `	ph7_value sVal;` |
|    7837 |  256 | `	if( pSlot == 0 ){` |
|     ! 0 |  257 | `		return;` |
|       - |  258 | `	}` |
|    7837 |  259 | `	PH7_MemObjInit(&(*pVm),&sVal);` |
|    7837 |  260 | `	if( pVal ){` |
|    7601 |  261 | `		sVal.x.pOther = pVal;` |
|    7601 |  262 | `		sVal.iFlags = MEMOBJ_OBJ;` |
|    3798 |  263 | `	}` |
|    7837 |  264 | `	PH7_MemObjStore(&sVal,pSlot);` |
|    7837 |  265 | `	NativeAttrMarkInit(pObj,zName);` |
|    3921 |  266 | `}` |
|       - |  267 | `/*` |
|       - |  268 | ` * Hand an instance back as a native call's result, dropping the reference` |
|       - |  269 | ` * PH7_NewClassInstance/PH7_CloneClassInstance handed the caller.` |
|       - |  270 | ` */` |
|    2176 |  271 | `PH7_PRIVATE void PH7_NativeResultObject(ph7_context *pCtx,ph7_class_instance *pObj)` |
|       5 |  272 | `{` |
|       - |  273 | `	ph7_value sRes;` |
|    2181 |  274 | `	PH7_MemObjInit(pCtx->pVm,&sRes);` |
|    2181 |  275 | `	sRes.x.pOther = pObj;` |
|    2181 |  276 | `	sRes.iFlags = MEMOBJ_OBJ;` |
|    2181 |  277 | `	ph7_result_value(pCtx,&sRes);   /* takes its own reference */` |
|    2181 |  278 | `	PH7_ClassInstanceUnref(pObj);` |
|    2181 |  279 | `}` |
|       - |  280 | `/*` |
|       - |  281 | ` * Resolve a class by name for the builder's own use (parents and interfaces).` |
|       - |  282 | ` * Autoload is deliberately NOT triggered: these run at VM init, where the only` |
|       - |  283 | ` * classes that can exist are the ones installed before this call, and a missing` |
|       - |  284 | ` * name is a build-order bug in the engine, not a userland lookup.` |
|       - |  285 | ` */` |
|  777592 |  286 | `static ph7_class * NativeLookupClass(ph7_vm *pVm,const char *zName)` |
|       5 |  287 | `{` |
|  777597 |  288 | `	return PH7_VmExtractClass(&(*pVm),zName,(sxu32)SyStrlen(zName),FALSE,0);` |
|       5 |  289 | `}` |
|       - |  290 | `/*` |
|       - |  291 | ` * Translate the builder's PH7_MOD_* modifier bits into the protection level and` |
|       - |  292 | ` * the attribute/method flag word the class structures actually store.` |
|       - |  293 | ` */` |
| 7382070 |  294 | `static sxi32 NativeProtection(sxi32 iMods)` |
|       5 |  295 | `{` |
| 7382075 |  296 | `	if( iMods & PH7_MOD_PRIVATE ){` |
|  520151 |  297 | `		return PH7_CLASS_PROT_PRIVATE;` |
|       - |  298 | `	}` |
| 6861929 |  299 | `	if( iMods & PH7_MOD_PROTECTED ){` |
|  162879 |  300 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - |  301 | `	}` |
| 6699055 |  302 | `	return PH7_CLASS_PROT_PUBLIC;` |
| 3691040 |  303 | `}` |
|       - |  304 | `/*` |
|       - |  305 | ` * Attach one C-bodied method to an already-created class.` |
|       - |  306 | ` *` |
|       - |  307 | ` * The ph7_user_func built here is NOT registered in pVm->hHostFunction: it hangs` |
|       - |  308 | ` * off the method and is reachable only by dispatching the method, which is exactly` |
|       - |  309 | ` * the property that retires the global thunks. Its sName is the php-facing` |
|       - |  310 | `` * `Class::method`, so an ArgumentCountError raised at the OP_CALL choke point`` |
|       - |  311 | ` * names what a php user would recognise.` |
|       - |  312 | ` */` |
| 5338264 |  313 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallMethod(` |
|       - |  314 | `	ph7_vm *pVm,` |
|       - |  315 | `	ph7_class *pClass,` |
|       - |  316 | `	const PH7_NativeMethodDef *pDef,` |
|       - |  317 | `	void *pUserData` |
|       - |  318 | `	)` |
|       5 |  319 | `{` |
|       - |  320 | `	ph7_class_method *pMeth;` |
|       - |  321 | `	ph7_user_func *pNative;` |
|       - |  322 | `	SyString sName;` |
|       - |  323 | `	SyString sVmName;` |
|       - |  324 | `	char zQual[128];` |
|       - |  325 | `	sxi32 iFuncFlags;` |
|       - |  326 | `	sxi32 rc;` |
| 5338269 |  327 | `	SyStringInitFromBuf(&sName,pDef->zName,SyStrlen(pDef->zName));` |
| 5338269 |  328 | `	iFuncFlags = VM_FUNC_NATIVE;` |
| 5338269 |  329 | `	if( pVm->bCompilingBuiltin ){` |
|       - |  330 | `		/* Same stamp the embedded chunks got, so Reflection keeps reporting these` |
|       - |  331 | `		 * as internal: isInternal() true, getFileName() false. */` |
| 5338069 |  332 | `		iFuncFlags \|= VM_FUNC_INTERNAL;` |
| 2669032 |  333 | `	}` |
| 8007401 |  334 | `	pMeth = PH7_NewClassMethod(&(*pVm),pClass,&sName,0,` |
| 5338264 |  335 | `		NativeProtection(pDef->iMods),` |
| 5338264 |  336 | `		((pDef->iMods & PH7_MOD_FINAL) ? PH7_CLASS_ATTR_FINAL : 0)` |
| 5338264 |  337 | `		\| ((pDef->iMods & PH7_MOD_ABSTRACT) ? PH7_CLASS_ATTR_ABSTRACT : 0),` |
| 2669132 |  338 | `		iFuncFlags);` |
| 5338269 |  339 | `	if( pMeth == 0 ){` |
|     ! 0 |  340 | `		return SXERR_MEM;` |
|       - |  341 | `	}` |
|       - |  342 | `	/* Staticness has to be recorded in BOTH places: on the method (where the class` |
|       - |  343 | `	 * machinery and Reflection read it) and on the function (where the OP_CALL` |
|       - |  344 | `	 * dispatcher reads it, to keep the caller's $this from leaking in as a receiver` |
|       - |  345 | `	 * — see VM_FUNC_NATIVE_STATIC). */` |
| 5338269 |  346 | `	if( pDef->iMods & PH7_MOD_STATIC ){` |
|  178841 |  347 | `		pMeth->iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|  178841 |  348 | `		pMeth->sFunc.iFlags \|= VM_FUNC_NATIVE_STATIC;` |
|   89418 |  349 | `	}` |
|       - |  350 | `	/* An ABSTRACT method has no body to install — an INTERFACE's methods are the` |
|       - |  351 | ``	 * reason the builder needs the case at all (`SeekableIterator::seek()`), and`` |
|       - |  352 | `	 * php reports them with their declared signature like any other. The dispatch` |
|       - |  353 | `	 * never reaches one: OP_CALL and OP_MEMBER both refuse an abstract method` |
|       - |  354 | `	 * ("Cannot call abstract method C::m()") before they look at a body. */` |
| 5338269 |  355 | `	if( pDef->iMods & PH7_MOD_ABSTRACT ){` |
|  267959 |  356 | `		if( pDef->zSig ){` |
|  267959 |  357 | `			rc = PH7_NewForeignFunction(&(*pVm),&sName,pDef->xFunc,pUserData,&pNative);` |
|  267959 |  358 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  359 | `				return rc;` |
|       - |  360 | `			}` |
|  267959 |  361 | `			pNative->zSig = pDef->zSig;` |
|  267959 |  362 | `			if( pDef->zRet && pDef->zRet[0] ){` |
|  183895 |  363 | `				pNative->zRet = pDef->zRet;` |
|   91945 |  364 | `			}` |
|  267959 |  365 | `			pMeth->sFunc.pNative = pNative;` |
|  133977 |  366 | `		}` |
|  267959 |  367 | `		return PH7_ClassInstallMethod(pClass,pMeth);` |
|       - |  368 | `	}` |
|       - |  369 | `	/* The C body. The diagnostic name is the qualified one php would print. */` |
| 5070315 |  370 | `	SyBufferFormat(zQual,sizeof(zQual),"%z::%s",&pClass->sName,pDef->zName);` |
| 5070315 |  371 | `	SyStringInitFromBuf(&sVmName,zQual,SyStrlen(zQual));` |
| 5070315 |  372 | `	rc = PH7_NewForeignFunction(&(*pVm),&sVmName,pDef->xFunc,pUserData,&pNative);` |
| 5070315 |  373 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  374 | `		return rc;` |
|       - |  375 | `	}` |
|       - |  376 | `	/* Arity bounds and by-ref positions come from the declared signature, exactly` |
|       - |  377 | `	 * as they do for a builtin (VmSetBuiltinSignatures) -- one string is the single` |
|       - |  378 | `	 * source of truth, and it doubles as the Reflection parameter list.` |
|       - |  379 | `	 *` |
|       - |  380 | `	 * NULL and "" mean different things here, and the difference is load-bearing.` |
|       - |  381 | `	 * A builtin without a row in aBuiltinSig[] is simply undescribed, so it stays` |
|       - |  382 | `	 * UNENFORCED (the SyZero'd bHasMaxArg == 0 reads as "unchecked", never as` |
|       - |  383 | `	 * "accepts at most zero"). A native method, by contrast, always states its` |
|       - |  384 | `	 * signature deliberately, so "" is a positive declaration of ZERO parameters` |
|       - |  385 | ``	 * and must enforce a maximum of zero -- otherwise `$gen->current(1)` and`` |
|       - |  386 | ``	 * `$fiber->isStarted(1)` are swallowed where php raises "expects exactly 0`` |
|       - |  387 | `	 * arguments, 1 given". VmDeriveArityFromSig("") already answers min 0 / max 0 /` |
|       - |  388 | `	 * enforced; only NULL opts out. */` |
| 5070315 |  389 | `	if( pDef->zSig ){` |
| 5065061 |  390 | `		sxi16 nMin = 0, nMax = 0;` |
| 5065061 |  391 | `		sxu8 bAtLeast = 0, bHasMax = 0;` |
| 5065061 |  392 | `		pNative->zSig = pDef->zSig;` |
| 5065061 |  393 | `		pNative->nByRefMask = VmDeriveByRefMaskFromSig(pDef->zSig);` |
| 5065061 |  394 | `		VmDeriveArityFromSig(pDef->zSig,&nMin,&bAtLeast,&nMax,&bHasMax);` |
| 5065061 |  395 | `		pNative->nMinArg = nMin;` |
| 5065061 |  396 | `		pNative->bAtLeast = bAtLeast;` |
| 5065061 |  397 | `		pNative->nMaxArg = nMax;` |
| 5065061 |  398 | `		pNative->bHasMaxArg = bHasMax;` |
| 2532528 |  399 | `	}` |
| 5070315 |  400 | `	if( pDef->zRet && pDef->zRet[0] ){` |
| 4466105 |  401 | `		pNative->zRet = pDef->zRet;` |
| 2233050 |  402 | `	}` |
| 5070315 |  403 | `	pMeth->sFunc.pNative = pNative;` |
| 5070315 |  404 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
| 5070315 |  405 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  406 | `		return rc;` |
|       - |  407 | `	}` |
| 5070315 |  408 | `	if( pClass->bMounted ){` |
|       - |  409 | `		/* The class is already live (a method attached after installation): mount` |
|       - |  410 | `		 * this one method now, since VmMountUserClass will not run again. */` |
|     ! 0 |  411 | `		return PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|       - |  412 | `	}` |
| 5070315 |  413 | `	return SXRET_OK;` |
| 2669137 |  414 | `}` |
|       - |  415 | `/*` |
|       - |  416 | ` * Install one class constant carrying a scalar value.` |
|       - |  417 | ` *` |
|       - |  418 | ` * A declared default is normally a compiled initializer (ph7_class_attr::aByteCode)` |
|       - |  419 | ` * evaluated at mount. The builder has no compiler to hand, so it evaluates the` |
|       - |  420 | ` * value NOW into the constant's reserved slot and leaves the byte-code empty --` |
|       - |  421 | ` * which is what a literal initializer would have produced anyway.` |
|       - |  422 | ` */` |
|  982498 |  423 | `static sxi32 NativeInstallConstant(ph7_vm *pVm,ph7_class *pClass,const PH7_NativeConstDef *pDef)` |
|       5 |  424 | `{` |
|       - |  425 | `	ph7_class_attr *pAttr;` |
|       - |  426 | `	SyString sName;` |
|  982503 |  427 | `	SyStringInitFromBuf(&sName,pDef->zName,SyStrlen(pDef->zName));` |
|  982503 |  428 | `	pAttr = PH7_NewClassAttr(&(*pVm),&sName,0,NativeProtection(pDef->iMods),` |
|       - |  429 | `		PH7_CLASS_ATTR_CONSTANT);` |
|  982503 |  430 | `	if( pAttr == 0 ){` |
|     ! 0 |  431 | `		return SXERR_MEM;` |
|       - |  432 | `	}` |
|  982503 |  433 | `	pAttr->pDeclClass = pClass;` |
|       - |  434 | `	/* Stash the literal; VmMountUserClassAttrs materializes it into the slot. */` |
|  982503 |  435 | `	pAttr->pNativeValue = pDef;` |
|  982503 |  436 | `	return PH7_ClassInstallAttr(pClass,pAttr);` |
|  491254 |  437 | `}` |
|       - |  438 | `/*` |
|       - |  439 | ` * Fill in a declared property TYPE from the text a spec row states, exactly as` |
|       - |  440 | ` * GenStateCopyTypeToAttr fills it from a parsed declaration: nType is the` |
|       - |  441 | ` * MEMOBJ_* the atom names (SXU32_HIGH plus sClass for a class name, which is` |
|       - |  442 | `` * also how the compiler carries `mixed` and `iterable`), the `?` becomes the`` |
|       - |  443 | ` * NULLABLE flag rather than a type bit, and sTypeName keeps the text VERBATIM --` |
|       - |  444 | ` * both the TypeError and Reflection print what the declaration said.` |
|       - |  445 | ` *` |
|       - |  446 | ` * Single atoms only. A union needs the alternative SET the compiler builds, and` |
|       - |  447 | ` * nothing native declares one; a spec that tries reads as the class name it is` |
|       - |  448 | ` * spelled with, which is why the parse stays this literal.` |
|       - |  449 | ` */` |
|  320494 |  450 | `static void NativeAttrType(ph7_class_attr *pAttr,const char *zType)` |
|       5 |  451 | `{` |
|       - |  452 | `	static const struct { const char *zName; sxu32 nType; } aScalar[] = {` |
|       - |  453 | `		{ "int",    MEMOBJ_INT },` |
|       - |  454 | `		{ "float",  MEMOBJ_REAL },` |
|       - |  455 | `		{ "string", MEMOBJ_STRING },` |
|       - |  456 | `		{ "bool",   MEMOBJ_BOOL },` |
|       - |  457 | `		{ "array",  MEMOBJ_HASHMAP },` |
|       - |  458 | `		{ "object", MEMOBJ_OBJ },` |
|       - |  459 | `	};` |
|  320499 |  460 | `	const char *zAtom = zType;` |
|       - |  461 | `	sxu32 nAtom, n;` |
|  320499 |  462 | `	if( zAtom[0] == '?' ){` |
|   52545 |  463 | `		pAttr->iFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|   52545 |  464 | `		zAtom++;` |
|   26270 |  465 | `	}` |
|  320499 |  466 | `	nAtom = SyStrlen(zAtom);` |
|  320499 |  467 | `	pAttr->iFlags \|= PH7_CLASS_ATTR_TYPED;` |
|  320499 |  468 | `	SyStringInitFromBuf(&pAttr->sTypeName,zType,SyStrlen(zType));` |
| 1066567 |  469 | `	for( n = 0 ; n < SX_ARRAYSIZE(aScalar) ; ++n ){` |
| 1024530 |  470 | `		if( nAtom == SyStrlen(aScalar[n].zName)` |
|  664636 |  471 | `		 && SyMemcmp(zAtom,aScalar[n].zName,(sxu32)nAtom) == 0 ){` |
|  278467 |  472 | `			pAttr->nType = aScalar[n].nType;` |
|  278467 |  473 | `			return;` |
|       - |  474 | `		}` |
|  373039 |  475 | `	}` |
|   42037 |  476 | `	pAttr->nType = SXU32_HIGH;` |
|   42037 |  477 | `	SyStringInitFromBuf(&pAttr->sClass,zAtom,nAtom);` |
|  160252 |  478 | `}` |
|       - |  479 | `/*` |
|       - |  480 | ` * Install one declared property.` |
|       - |  481 | ` *` |
|       - |  482 | ` * The default is carried as a literal rather than compiled byte-code (see` |
|       - |  483 | `` * PH7_NativeLiteralValue); an instance property materializes it at `new`, a static`` |
|       - |  484 | `` * one at mount. A PH7_NATIVE_VAL_NULL default is the plain `public $p;` case.`` |
|       - |  485 | ` */` |
| 1061308 |  486 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallProperty(ph7_vm *pVm,ph7_class *pClass,` |
|       - |  487 | `	const PH7_NativePropDef *pDef)` |
|       5 |  488 | `{` |
|       - |  489 | `	ph7_class_attr *pAttr;` |
|       - |  490 | `	SyString sName;` |
| 1061313 |  491 | `	sxi32 iFlags = 0;` |
| 1061313 |  492 | `	SyStringInitFromBuf(&sName,pDef->zName,SyStrlen(pDef->zName));` |
| 1061313 |  493 | `	if( pDef->iMods & PH7_MOD_STATIC ){` |
|     ! 0 |  494 | `		iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  495 | `	}` |
| 1061313 |  496 | `	if( pDef->iMods & PH7_MOD_HIDDEN ){` |
|  646247 |  497 | `		iFlags \|= PH7_CLASS_ATTR_HIDDEN;` |
|  323121 |  498 | `	}` |
|       - |  499 | `	/* php declares several native slots readonly and asymmetrically visible` |
|       - |  500 | ``	 * (`public protected(set) readonly string $path` on Directory), and both are`` |
|       - |  501 | `	 * php-visible twice over: the write refusal and Reflection's modifier list. */` |
| 1061313 |  502 | `	if( pDef->iMods & PH7_MOD_READONLY ){` |
|   26275 |  503 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|   13135 |  504 | `	}` |
| 1061313 |  505 | `	if( pDef->iMods & PH7_MOD_PROT_SET ){` |
|   21021 |  506 | `		iFlags \|= PH7_CLASS_ATTR_PROTECTED_SET;` |
|   10508 |  507 | `	}` |
| 1061313 |  508 | `	if( pDef->iMods & PH7_MOD_PRIV_SET ){` |
|     ! 0 |  509 | `		iFlags \|= PH7_CLASS_ATTR_PRIVATE_SET;` |
|     ! 0 |  510 | `	}` |
| 1061313 |  511 | `	pAttr = PH7_NewClassAttr(&(*pVm),&sName,0,NativeProtection(pDef->iMods),iFlags);` |
| 1061313 |  512 | `	if( pAttr == 0 ){` |
|     ! 0 |  513 | `		return SXERR_MEM;` |
|       - |  514 | `	}` |
| 1061313 |  515 | `	pAttr->pDeclClass = pClass;` |
|       - |  516 | `	/* NO default is not the same as a NULL one, and the difference is php-visible:` |
|       - |  517 | ``	 * a typed slot without a default is UNINITIALIZED at `new` (reading it before`` |
|       - |  518 | `	 * the class writes it is an Error, hasDefaultValue() is false), which is what` |
|       - |  519 | `	 * php declares for LibXMLError's six fields and every reflector's $name. The` |
|       - |  520 | ``	 * machinery is already there for a compiled `public string $p;` — leaving`` |
|       - |  521 | `	 * pNativeValue at 0 is what selects it. */` |
| 1061313 |  522 | `	if( pDef->sDefault.iType != PH7_NATIVE_VAL_NONE ){` |
|  903693 |  523 | `		pAttr->pNativeValue = &pDef->sDefault;` |
|  451844 |  524 | `	}` |
| 1061313 |  525 | `	if( pDef->zType && pDef->zType[0] ){` |
|  320499 |  526 | `		NativeAttrType(pAttr,pDef->zType);` |
|  160247 |  527 | `	}` |
| 1061313 |  528 | `	return PH7_ClassInstallAttr(pClass,pAttr);` |
|  530659 |  529 | `}` |
|       - |  530 | `/*` |
|       - |  531 | `` * Attach an `#[Attr(...)]` to a class declared from C.`` |
|       - |  532 | ` *` |
|       - |  533 | `` * php declares `#[Attribute(Attribute::TARGET_CLASS)]` on Attribute itself and a`` |
|       - |  534 | ` * target mask on Deprecated, and both records are LOAD-BEARING: the compiler` |
|       - |  535 | `` * reads them to decide whether a user's `#[Deprecated]` may sit where it does,`` |
|       - |  536 | ` * and ReflectionAttribute answers them. A compiled attribute holds its argument` |
|       - |  537 | ` * as byte-code; there is no compiler here, so the argument rides as the same` |
|       - |  538 | ` * literal record a native constant or property default uses and every reader` |
|       - |  539 | ` * takes that branch when the byte-code is empty.` |
|       - |  540 | ` */` |
|   10508 |  541 | `PH7_PRIVATE sxi32 PH7_NativeClassAddAttribute(ph7_vm *pVm,ph7_class *pClass,` |
|       - |  542 | `	const char *zAttr,const PH7_NativeAttrArg *aArg,sxu32 nArg)` |
|       5 |  543 | `{` |
|       - |  544 | `	ph7_attribute sAttr;` |
|       - |  545 | `	char *zDup;` |
|       - |  546 | `	sxu32 n;` |
|   10513 |  547 | `	if( pClass == 0 ){` |
|     ! 0 |  548 | `		return SXERR_NOTFOUND;` |
|       - |  549 | `	}` |
|   10513 |  550 | `	SyZero(&sAttr,sizeof(sAttr));` |
|   10513 |  551 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zAttr,SyStrlen(zAttr));` |
|   10513 |  552 | `	if( zDup == 0 ){` |
|     ! 0 |  553 | `		return SXERR_MEM;` |
|       - |  554 | `	}` |
|   10513 |  555 | `	SyStringInitFromBuf(&sAttr.sName,zDup,SyStrlen(zAttr));` |
|   10513 |  556 | `	SySetInit(&sAttr.aArgs,&pVm->sAllocator,sizeof(ph7_attr_arg));` |
|   21021 |  557 | `	for( n = 0 ; n < nArg ; n++ ){` |
|       - |  558 | `		ph7_attr_arg sArgRec;` |
|   10513 |  559 | `		SyZero(&sArgRec,sizeof(sArgRec));` |
|   10513 |  560 | `		SySetInit(&sArgRec.aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|   10513 |  561 | `		if( aArg[n].zName ){` |
|     ! 0 |  562 | `			char *zN = SyMemBackendStrDup(&pVm->sAllocator,aArg[n].zName,SyStrlen(aArg[n].zName));` |
|     ! 0 |  563 | `			if( zN ){` |
|     ! 0 |  564 | `				SyStringInitFromBuf(&sArgRec.sName,zN,SyStrlen(aArg[n].zName));` |
|     ! 0 |  565 | `			}` |
|     ! 0 |  566 | `		}` |
|   10513 |  567 | `		sArgRec.pNativeValue = (const void *)&aArg[n].sValue;` |
|   10513 |  568 | `		SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|    5259 |  569 | `	}` |
|   10513 |  570 | `	return SySetPut(&pClass->aAttrs,(const void *)&sAttr);` |
|    5259 |  571 | `}` |
|       - |  572 | `/*` |
|       - |  573 | ` * php's get_properties / get_debug_info handlers: the SHAPE a class SHOWS.` |
|       - |  574 | ` *` |
|       - |  575 | ` * Several native classes present something that is not their storage. php shows a` |
|       - |  576 | ` * DateTime as date/timezone_type/timezone (the state is a timestamp, an offset and` |
|       - |  577 | ` * a zone name), a DateTimeZone as timezone_type/timezone, and a WeakReference as` |
|       - |  578 | ` * ["object"]. Those slots carry PH7_MOD_HIDDEN so no presentation surface sees the` |
|       - |  579 | ` * engine state; this fills an array with what php shows instead.` |
|       - |  580 | ` *` |
|       - |  581 | ` * Answers 1 when the class (or an ancestor) declared a hook and pOut was filled.` |
|       - |  582 | ` * bDebug distinguishes php's two handlers: 1 for var_dump/print_r (get_debug_info),` |
|       - |  583 | ` * 0 for var_export and the (array) cast (get_properties). They disagree — a` |
|       - |  584 | ` * WeakReference shows ["object"] to var_dump and nothing to (array) — so the` |
|       - |  585 | ` * callback is told which is asking rather than each caller guessing.` |
|       - |  586 | ` * get_object_vars() and foreach are NOT callers: php answers those from the real` |
|       - |  587 | ` * properties with the caller's scope applied, which for every class here is empty.` |
|       - |  588 | ` */` |
|     640 |  589 | `PH7_PRIVATE int PH7_ClassInstancePresent(ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|       5 |  590 | `{` |
|       - |  591 | `	ph7_class *pClass;` |
|     645 |  592 | `	if( pThis == 0 \|\| pOut == 0 ){` |
|     ! 0 |  593 | `		return 0;` |
|       - |  594 | `	}` |
|    1157 |  595 | `	for( pClass = pThis->pClass ; pClass ; pClass = pClass->pBase ){` |
|     673 |  596 | `		if( pClass->xPresent ){` |
|     159 |  597 | `			return pClass->xPresent(pThis->pVm,pThis,pOut,bDebug) == SXRET_OK;` |
|       - |  598 | `		}` |
|     261 |  599 | `	}` |
|     489 |  600 | `	return 0;` |
|     325 |  601 | `}` |
|       - |  602 | `/*` |
|       - |  603 | ` * The nearest ph7_class::xDim in a class's base chain -- php's handler` |
|       - |  604 | ` * inheritance, so a user subclass of DOMNodeList reads dimensions the way its` |
|       - |  605 | ` * parent does.` |
|       - |  606 | ` */` |
|    1132 |  607 | `static ph7_class * NativeDimClass(ph7_class *pClass)` |
|       5 |  608 | `{` |
|    1723 |  609 | `	while( pClass ){` |
|    1211 |  610 | `		if( pClass->xDim ){` |
|     622 |  611 | `			return pClass;` |
|       - |  612 | `		}` |
|     591 |  613 | `		pClass = pClass->pBase;` |
|       5 |  614 | `	}` |
|     517 |  615 | `	return 0;` |
|     571 |  616 | `}` |
|       - |  617 | `/*` |
|       - |  618 | `` * Does `$o[$k]` mean anything for an instance of this class? The subscript`` |
|       - |  619 | `` * opcode asks BEFORE it commits to php's `Cannot use object of type C as`` |
|       - |  620 | `` * array`, which is still the answer for every class that has no hook.`` |
|       - |  621 | ` */` |
|     820 |  622 | `PH7_PRIVATE int PH7_ClassHasNativeDim(ph7_class *pClass)` |
|       5 |  623 | `{` |
|     825 |  624 | `	return NativeDimClass(pClass) != 0;` |
|       5 |  625 | `}` |
|       - |  626 | `/*` |
|       - |  627 | ` * Run the hook. Answers 0 when the class has none (nothing in pCtx is touched);` |
|       - |  628 | ` * 1 when it answered, which includes a REFUSAL -- the caller reads zThrowClass` |
|       - |  629 | ` * to tell the two apart.` |
|       - |  630 | ` */` |
|     312 |  631 | `PH7_PRIVATE int PH7_ClassNativeDim(ph7_class_instance *pThis,PH7_NativeDimCtx *pCtx)` |
|       2 |  632 | `{` |
|     314 |  633 | `	ph7_class *pClass = pThis ? NativeDimClass(pThis->pClass) : 0;` |
|     314 |  634 | `	if( pClass == 0 ){` |
|     ! 0 |  635 | `		return 0;` |
|       - |  636 | `	}` |
|     314 |  637 | `	pClass->xDim(pThis->pVm,pThis,pCtx);` |
|     314 |  638 | `	return 1;` |
|     158 |  639 | `}` |
|       - |  640 | `/*` |
|       - |  641 | ` * The nearest ph7_class::xSet in a class's base chain -- the same handler` |
|       - |  642 | ` * inheritance the dimension hook gets, so a user subclass of DateInterval` |
|       - |  643 | ` * converts its writes the way its parent does.` |
|       - |  644 | ` */` |
|     612 |  645 | `static ph7_class * NativeSetClass(ph7_class *pClass)` |
|       2 |  646 | `{` |
|     616 |  647 | `	while( pClass ){` |
|     616 |  648 | `		if( pClass->xSet ){` |
|     614 |  649 | `			return pClass;` |
|       - |  650 | `		}` |
|       3 |  651 | `		pClass = pClass->pBase;` |
|       1 |  652 | `	}` |
|     ! 0 |  653 | `	return 0;` |
|     308 |  654 | `}` |
|       - |  655 | `/*` |
|       - |  656 | ` * Run the write handler for a property store. Answers 0 when no class in the` |
|       - |  657 | ` * chain has one (nothing in pCtx is touched); 1 when it ran, which includes a` |
|       - |  658 | ` * REFUSAL -- the caller reads zThrowClass to tell the two apart.` |
|       - |  659 | ` */` |
|     612 |  660 | `PH7_PRIVATE int PH7_ClassNativeSet(ph7_class_instance *pThis,PH7_NativeSetCtx *pCtx)` |
|       2 |  661 | `{` |
|     614 |  662 | `	ph7_class *pClass = pThis ? NativeSetClass(pThis->pClass) : 0;` |
|     614 |  663 | `	if( pClass == 0 ){` |
|     ! 0 |  664 | `		return 0;` |
|       - |  665 | `	}` |
|     614 |  666 | `	pClass->xSet(pThis->pVm,pThis,pCtx);` |
|     614 |  667 | `	return 1;` |
|     308 |  668 | `}` |
|       - |  669 | `/*` |
|       - |  670 | ` * Install a write handler on a mounted native class and mark every INSTANCE` |
|       - |  671 | ` * property it declares as filtered, which is what makes instantiation register` |
|       - |  672 | ` * the slots the filter looks up. Called by the owning installer right after` |
|       - |  673 | ` * PH7_InstallNativeClasses, for the same reason xClone and xDim are: the spec` |
|       - |  674 | ` * table has no field for a hook.` |
|       - |  675 | ` */` |
|    5254 |  676 | `PH7_PRIVATE sxi32 PH7_NativeClassInstallSetHook(ph7_vm *pVm,const char *zClass,` |
|       - |  677 | `	void (*xSet)(ph7_vm *,ph7_class_instance *,PH7_NativeSetCtx *))` |
|       5 |  678 | `{` |
|    5259 |  679 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|       - |  680 | `	SyHashEntry *pEntry;` |
|    5259 |  681 | `	if( pClass == 0 ){` |
|     ! 0 |  682 | `		return SXERR_NOTFOUND;` |
|       - |  683 | `	}` |
|    5259 |  684 | `	pClass->xSet = xSet;` |
|    5259 |  685 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|   65680 |  686 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|   57799 |  687 | `		ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   57799 |  688 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|   57799 |  689 | `			pAttr->iFlags \|= PH7_CLASS_ATTR_NATIVE_SET;` |
|   28897 |  690 | `		}` |
|       5 |  691 | `	}` |
|    5259 |  692 | `	return SXRET_OK;` |
|    2632 |  693 | `}` |
|       - |  694 | `/*` |
|       - |  695 | ` * Create and install ONE class from its spec: constants and properties, but` |
|       - |  696 | ` * neither methods nor its base chain.` |
|       - |  697 | ` *` |
|       - |  698 | ` * Split from the two passes that follow because a spec table may describe` |
|       - |  699 | ` * classes that extend each other, and PH7_ClassInherit needs the parent to` |
|       - |  700 | ` * exist -- and to already CARRY ITS METHODS, since inheriting is what copies` |
|       - |  701 | ` * them down.` |
|       - |  702 | ` */` |
|  851148 |  703 | `static sxi32 NativeDeclareClass(ph7_vm *pVm,const PH7_NativeClassSpec *pSpec,ph7_class **ppOut)` |
|       5 |  704 | `{` |
|       - |  705 | `	ph7_class *pClass;` |
|       - |  706 | `	SyString sName;` |
|       - |  707 | `	sxu32 n;` |
|       - |  708 | `	sxi32 rc;` |
|  851153 |  709 | `	SyStringInitFromBuf(&sName,pSpec->zName,SyStrlen(pSpec->zName));` |
|  851153 |  710 | `	pClass = PH7_NewRawClass(&(*pVm),&sName,0);` |
|  851153 |  711 | `	if( pClass == 0 ){` |
|     ! 0 |  712 | `		return SXERR_MEM;` |
|       - |  713 | `	}` |
|       - |  714 | `	/* PH7_CLASS_BOUND: an engine class is declared unconditionally, exactly once,` |
|       - |  715 | ``	 * so `class DateTime {}` in user code is php's "Cannot redeclare class`` |
|       - |  716 | `	 * DateTime". Only the compiler used to set the flag, so EVERY native class` |
|       - |  717 | `	 * was silently redeclarable — the chunk-declared ones (Exception, stdClass)` |
|       - |  718 | `	 * fataled and their C replacements did not. */` |
|  851153 |  719 | `	pClass->iFlags \|= pSpec->iFlags \| PH7_CLASS_BOUND;` |
|  851153 |  720 | `	pClass->xRelease = pSpec->xRelease;` |
|  851153 |  721 | `	pClass->pIterVtab = pSpec->pIterVtab;` |
|  851153 |  722 | `	pClass->xPresent = pSpec->xPresent;` |
| 1833651 |  723 | `	for( n = 0 ; n < pSpec->nConst ; n++ ){` |
|  982503 |  724 | `		rc = NativeInstallConstant(&(*pVm),pClass,&pSpec->aConst[n]);` |
|  982503 |  725 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  726 | `			return rc;` |
|       - |  727 | `		}` |
|  491254 |  728 | `	}` |
| 1912461 |  729 | `	for( n = 0 ; n < pSpec->nProp ; n++ ){` |
| 1061313 |  730 | `		rc = PH7_NativeClassInstallProperty(&(*pVm),pClass,&pSpec->aProp[n]);` |
| 1061313 |  731 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  732 | `			return rc;` |
|       - |  733 | `		}` |
|  530659 |  734 | `	}` |
|  851153 |  735 | `	*ppOut = pClass;` |
|  851153 |  736 | `	return PH7_VmInstallClass(&(*pVm),pClass);` |
|  425579 |  737 | `}` |
|       - |  738 | `/*` |
|       - |  739 | ` * Wire ONE class's base chain and interfaces.` |
|       - |  740 | ` *` |
|       - |  741 | ` * This runs AFTER every class in the table has its own methods, which is the` |
|       - |  742 | ` * compiler's order too (GenStateCompileClassEx compiles the whole body and only` |
|       - |  743 | ` * then calls PH7_ClassInherit/PH7_ClassImplement). Two things depend on it:` |
|       - |  744 | ` * PH7_ClassInherit COPIES the base's methods into the subclass, so a base whose` |
|       - |  745 | ` * methods were not installed yet hands down an empty table -- which is how` |
|       - |  746 | `` * `DOMDocument::C14N()` came out undefined, the first time a native class`` |
|       - |  747 | ` * extended another native class that had methods; and PH7_ClassImplement stubs` |
|       - |  748 | ` * every interface method the class lacks as ABSTRACT, so a spec may now name an` |
|       - |  749 | ` * interface it implements itself rather than attaching it by hand afterwards.` |
|       - |  750 | ` */` |
|  851148 |  751 | `static sxi32 NativeLinkClass(ph7_vm *pVm,const PH7_NativeClassSpec *pSpec,ph7_class *pClass)` |
|       5 |  752 | `{` |
|       - |  753 | `	sxi32 rc;` |
|  851153 |  754 | `	if( pSpec->zParent ){` |
|  457103 |  755 | `		ph7_class *pBase = NativeLookupClass(&(*pVm),pSpec->zParent);` |
|  457103 |  756 | `		if( pBase == 0 ){` |
|     ! 0 |  757 | `			return SXERR_NOTFOUND;` |
|       - |  758 | `		}` |
|       - |  759 | `		/* The VM's OWN generator state, not a NULL one: PH7_ClassInherit takes its` |
|       - |  760 | `		 * scratch allocator from pGen->pVm and reports every inheritance rule it` |
|       - |  761 | `		 * enforces through PH7_GenCompileError(pGen, ...). Passing 0 crashed the` |
|       - |  762 | `		 * moment a native spec first named a parent (the date exceptions). */` |
|  685652 |  763 | `		rc = (pClass->iFlags & PH7_CLASS_INTERFACE)` |
|   42032 |  764 | `			? PH7_ClassInterfaceInherit(pClass,pBase)` |
|  436082 |  765 | `			: PH7_ClassInherit(&pVm->sCodeGen,pClass,pBase);` |
|  457103 |  766 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  767 | `			return rc;` |
|       - |  768 | `		}` |
|  228549 |  769 | `	}` |
|  851153 |  770 | `	if( pSpec->zImplements ){` |
|       - |  771 | `		/* Comma-separated list, so one spec row can name several interfaces. */` |
|  199657 |  772 | `		const char *zCur = pSpec->zImplements;` |
|  499135 |  773 | `		while( zCur[0] != '\0' ){` |
|       - |  774 | `			const char *zStart;` |
|       - |  775 | `			char zIface[64];` |
|       - |  776 | `			sxu32 nLen;` |
|       - |  777 | `			ph7_class *pIface;` |
|  399309 |  778 | `			while( zCur[0] == ' ' \|\| zCur[0] == ',' ){` |
|   99831 |  779 | `				zCur++;` |
|       5 |  780 | `			}` |
|  299483 |  781 | `			if( zCur[0] == '\0' ){` |
|     ! 0 |  782 | `				break;` |
|       - |  783 | `			}` |
|  299483 |  784 | `			zStart = zCur;` |
| 3898473 |  785 | `			while( zCur[0] != '\0' && zCur[0] != ',' && zCur[0] != ' ' ){` |
| 3598995 |  786 | `				zCur++;` |
|       5 |  787 | `			}` |
|  299483 |  788 | `			nLen = (sxu32)(zCur - zStart);` |
|  299483 |  789 | `			if( nLen >= sizeof(zIface) ){` |
|     ! 0 |  790 | `				return SXERR_SYNTAX;` |
|       - |  791 | `			}` |
|  299483 |  792 | `			SyMemcpy(zStart,zIface,nLen);` |
|  299483 |  793 | `			zIface[nLen] = '\0';` |
|  299483 |  794 | `			pIface = NativeLookupClass(&(*pVm),zIface);` |
|  299483 |  795 | `			if( pIface == 0 ){` |
|     ! 0 |  796 | `				return SXERR_NOTFOUND;` |
|       - |  797 | `			}` |
|  299483 |  798 | `			rc = PH7_ClassImplement(pClass,pIface);` |
|  299483 |  799 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  800 | `				return rc;` |
|       - |  801 | `			}` |
|       5 |  802 | `		}` |
|   99826 |  803 | `	}` |
|  851153 |  804 | `	return SXRET_OK;` |
|  425579 |  805 | `}` |
|       - |  806 | `/*` |
|       - |  807 | ` * Install a whole table of native classes, in the compiler's own order: declare` |
|       - |  808 | ` * them all (so later rows may extend earlier ones), fill in their methods, wire` |
|       - |  809 | ` * the base chains and interfaces, then mount.` |
|       - |  810 | ` *` |
|       - |  811 | ` * Interfaces are declared but never mounted -- VmMountUserClass skips them anyway,` |
|       - |  812 | ` * their methods being non-invocable.` |
|       - |  813 | ` */` |
|  199652 |  814 | `PH7_PRIVATE sxi32 PH7_InstallNativeClasses(ph7_vm *pVm,const PH7_NativeClassSpec *aSpec,sxu32 nSpec)` |
|       5 |  815 | `{` |
|       - |  816 | `	ph7_class **apClass;` |
|       - |  817 | `	sxu32 i,j;` |
|       - |  818 | `	sxi32 rc;` |
|  199657 |  819 | `	apClass = (ph7_class **)SyMemBackendAlloc(&pVm->sAllocator,nSpec * sizeof(ph7_class *));` |
|  199657 |  820 | `	if( apClass == 0 ){` |
|     ! 0 |  821 | `		return SXERR_MEM;` |
|       - |  822 | `	}` |
| 1050805 |  823 | `	for( i = 0 ; i < nSpec ; i++ ){` |
|  851153 |  824 | `		apClass[i] = 0;` |
|  851153 |  825 | `		rc = NativeDeclareClass(&(*pVm),&aSpec[i],&apClass[i]);` |
|  851153 |  826 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  827 | `			goto Done;` |
|       - |  828 | `		}` |
|  425579 |  829 | `	}` |
| 1050805 |  830 | `	for( i = 0 ; i < nSpec ; i++ ){` |
| 6173455 |  831 | `		for( j = 0 ; j < aSpec[i].nMethod ; j++ ){` |
| 5322307 |  832 | `			rc = PH7_NativeClassInstallMethod(&(*pVm),apClass[i],&aSpec[i].aMethod[j],0);` |
| 5322307 |  833 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  834 | `				goto Done;` |
|       - |  835 | `			}` |
| 2661156 |  836 | `		}` |
|  425579 |  837 | `	}` |
| 1050805 |  838 | `	for( i = 0 ; i < nSpec ; i++ ){` |
|  851153 |  839 | `		rc = NativeLinkClass(&(*pVm),&aSpec[i],apClass[i]);` |
|  851153 |  840 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  841 | `			goto Done;` |
|       - |  842 | `		}` |
|  425579 |  843 | `	}` |
| 1050805 |  844 | `	for( i = 0 ; i < nSpec ; i++ ){` |
|  851153 |  845 | `		rc = VmMountUserClass(&(*pVm),apClass[i]);` |
|  851153 |  846 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  847 | `			goto Done;` |
|       - |  848 | `		}` |
|  425579 |  849 | `	}` |
|  199657 |  850 | `	rc = SXRET_OK;` |
|   99826 |  851 | `Done:` |
|  199657 |  852 | `	SyMemBackendFree(&pVm->sAllocator,apClass);` |
|  199657 |  853 | `	return rc;` |
|   99831 |  854 | `}` |
|       - |  855 | `/*` |
|       - |  856 | ` * ---------------------------------------------------------------------------` |
|       - |  857 | ` * Declaring an ENUM from C.` |
|       - |  858 | ` *` |
|       - |  859 | `` * An enum is not a class with constants: each `case` is a class constant whose`` |
|       - |  860 | ` * slot holds THE singleton instance of the enum for that case, materialized` |
|       - |  861 | ` * lazily on first access (VmEnumMaterializeCase). The compiler builds one by` |
|       - |  862 | `` * declaring the readonly `name`/`value` properties, pushing each case onto`` |
|       - |  863 | ` * ph7_class::aEnumCases, and SYNTHESIZING cases()/from()/tryFrom() as PHP` |
|       - |  864 | `` * source that forwards to the `__phl_enum_*` thunks.`` |
|       - |  865 | ` *` |
|       - |  866 | ` * This does the same three things without a compiler: the case's backing value` |
|       - |  867 | ` * rides as a literal (ph7_class_attr::pNativeValue, which the materializer` |
|       - |  868 | ` * reads where a compiled case has byte-code), and the three interface methods` |
|       - |  869 | ` * are C bodies that call the very same engine workers the synthesized PHP` |
|       - |  870 | `` * forwards to — so a native enum is an ordinary one to `instanceof`,`` |
|       - |  871 | `` * `match`, Reflection and `===` case identity.`` |
|       - |  872 | ` * ---------------------------------------------------------------------------` |
|       - |  873 | ` */` |
|       - |  874 | `/* The enum a static native method was called on. */` |
|     194 |  875 | `static ph7_class * NativeEnumSelf(ph7_context *pCtx,ph7_value *pName)` |
|       2 |  876 | `{` |
|     196 |  877 | `	ph7_class *pClass = PH7_ContextCalledClass(pCtx);` |
|     196 |  878 | `	PH7_MemObjInit(pCtx->pVm,pName);` |
|     196 |  879 | `	if( pClass ){` |
|     196 |  880 | `		ph7_value_string(pName,SyStringData(&pClass->sName),(int)SyStringLength(&pClass->sName));` |
|      97 |  881 | `	}` |
|     196 |  882 | `	return pClass;` |
|       2 |  883 | `}` |
|       - |  884 | `/*` |
|       - |  885 | ` * cases() / from() / tryFrom(): the engine thunks take the enum's FQN as their` |
|       - |  886 | ` * first argument, exactly as the compiler's synthesized bodies pass it.` |
|       - |  887 | ` */` |
|      16 |  888 | `static int vm_builtin_NativeEnum_cases(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  889 | `{` |
|       - |  890 | `	ph7_value sName;` |
|       - |  891 | `	ph7_value *ap[1];` |
|       - |  892 | `	int rc;` |
|       8 |  893 | `	SXUNUSED(nArg);` |
|       8 |  894 | `	SXUNUSED(apArg);` |
|      17 |  895 | `	if( NativeEnumSelf(pCtx,&sName) == 0 ){` |
|     ! 0 |  896 | `		ph7_result_null(pCtx);` |
|     ! 0 |  897 | `		return PH7_OK;` |
|       - |  898 | `	}` |
|      17 |  899 | `	ap[0] = &sName;` |
|      17 |  900 | `	rc = vm_builtin_enum_cases(pCtx,1,ap);` |
|      17 |  901 | `	PH7_MemObjRelease(&sName);` |
|      17 |  902 | `	return rc;` |
|       9 |  903 | `}` |
|     178 |  904 | `static int NativeEnumFrom(ph7_context *pCtx,int nArg,ph7_value **apArg,int bTry)` |
|       2 |  905 | `{` |
|       - |  906 | `	ph7_value sName;` |
|       - |  907 | `	ph7_value *ap[2];` |
|       - |  908 | `	int rc;` |
|     180 |  909 | `	if( nArg < 1 \|\| NativeEnumSelf(pCtx,&sName) == 0 ){` |
|     ! 0 |  910 | `		ph7_result_null(pCtx);` |
|     ! 0 |  911 | `		return PH7_OK;` |
|       - |  912 | `	}` |
|     180 |  913 | `	ap[0] = &sName;` |
|     180 |  914 | `	ap[1] = apArg[0];` |
|     180 |  915 | `	rc = bTry ? vm_builtin_enum_tryfrom(pCtx,2,ap) : vm_builtin_enum_from(pCtx,2,ap);` |
|     180 |  916 | `	PH7_MemObjRelease(&sName);` |
|     180 |  917 | `	return rc;` |
|      91 |  918 | `}` |
|      96 |  919 | `static int vm_builtin_NativeEnum_from(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  920 | `{` |
|      98 |  921 | `	return NativeEnumFrom(pCtx,nArg,apArg,0);` |
|       2 |  922 | `}` |
|      82 |  923 | `static int vm_builtin_NativeEnum_tryFrom(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  924 | `{` |
|      83 |  925 | `	return NativeEnumFrom(pCtx,nArg,apArg,1);` |
|       1 |  926 | `}` |
|       - |  927 | ``/* The readonly `name` (every enum) and `value` (backed only) case properties,`` |
|       - |  928 | ` * declared exactly as GenStateCompileEnum does. */` |
|   10508 |  929 | `static sxi32 NativeEnumInstallProp(ph7_vm *pVm,ph7_class *pClass,const char *zName,` |
|       - |  930 | `	sxu32 nType,const char *zTypeName)` |
|       5 |  931 | `{` |
|       - |  932 | `	SyString sName;` |
|       - |  933 | `	ph7_class_attr *pAttr;` |
|   10513 |  934 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|   10513 |  935 | `	pAttr = PH7_NewClassAttr(&(*pVm),&sName,0,PH7_CLASS_PROT_PUBLIC,` |
|       - |  936 | `		PH7_CLASS_ATTR_READONLY\|PH7_CLASS_ATTR_TYPED);` |
|   10513 |  937 | `	if( pAttr == 0 ){` |
|     ! 0 |  938 | `		return SXERR_MEM;` |
|       - |  939 | `	}` |
|   10513 |  940 | `	pAttr->nType = nType;` |
|   10513 |  941 | `	SyStringInitFromBuf(&pAttr->sTypeName,zTypeName,SyStrlen(zTypeName));` |
|   10513 |  942 | `	return PH7_ClassInstallAttr(pClass,pAttr);` |
|    5259 |  943 | `}` |
|       - |  944 | `/*` |
|       - |  945 | ` * cases()/from()/tryFrom(), installed on ANY enum -- one declared from C here` |
|       - |  946 | ` * and one the compiler just finished reading from source. php declares them on` |
|       - |  947 | ``  * the UnitEnum/BackedEnum prototypes, which is why `from` takes `string\|int` `` |
|       - |  948 | ` * rather than the enum's own backing type: the refusal for the wrong one is a` |
|       - |  949 | ` * VALUE check inside the body, not the parameter's.` |
|       - |  950 | ` *` |
|       - |  951 | ` * The compiler used to synthesize PHP source forwarding to three global` |
|       - |  952 | `` * `__phl_enum_*` thunks. Those names were php-visible, and the methods reported`` |
|       - |  953 | `` * as `<user>` with the enum's file and line where php reports`` |
|       - |  954 | `` * `<internal, prototype BackedEnum>`.`` |
|       - |  955 | ` */` |
|       - |  956 | `/*` |
|       - |  957 | ` * Install one of them and stamp it INTERNAL unconditionally. The usual` |
|       - |  958 | `` * `bCompilingBuiltin` stamp only fires while the engine compiles its OWN`` |
|       - |  959 | ` * sources, and these three are attached to a class the compiler is reading out` |
|       - |  960 | ` * of a USER file — but php reports them as internal wherever the enum is` |
|       - |  961 | ` * declared: isInternal() true, getFileName() false, getStartLine() 0.` |
|       - |  962 | ` */` |
|   15962 |  963 | `static sxi32 NativeEnumInstallMethod(ph7_vm *pVm,ph7_class *pClass,` |
|       - |  964 | `	const PH7_NativeMethodDef *pDef)` |
|       5 |  965 | `{` |
|       - |  966 | `	ph7_class_method *pMeth;` |
|   15967 |  967 | `	sxi32 rc = PH7_NativeClassInstallMethod(&(*pVm),pClass,pDef,0);` |
|   15967 |  968 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  969 | `		return rc;` |
|       - |  970 | `	}` |
|   15967 |  971 | `	pMeth = PH7_ClassExtractMethod(pClass,pDef->zName,(sxu32)SyStrlen(pDef->zName));` |
|   15967 |  972 | `	if( pMeth ){` |
|   15967 |  973 | `		pMeth->sFunc.iFlags \|= VM_FUNC_INTERNAL;` |
|       - |  974 | `		/* getFileName() reads the recorded source file rather than the flag, and` |
|       - |  975 | `		 * PH7_NewClassMethod stamped the user file the enum was read from. */` |
|   15967 |  976 | `		SyStringInitFromBuf(&pMeth->sFunc.sFile,"",0);` |
|    7981 |  977 | `	}` |
|   15967 |  978 | `	return SXRET_OK;` |
|    7986 |  979 | `}` |
|    5350 |  980 | `PH7_PRIVATE sxi32 PH7_InstallEnumInterfaceMethods(ph7_vm *pVm,ph7_class *pClass)` |
|       5 |  981 | `{` |
|       - |  982 | `	static const PH7_NativeMethodDef sCases =` |
|       - |  983 | `		{ "cases", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "", "array", vm_builtin_NativeEnum_cases };` |
|       - |  984 | `	static const PH7_NativeMethodDef aFrom[] = {` |
|       - |  985 | `		{ "from",    PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "string\|int $value", "static",` |
|       - |  986 | `		  vm_builtin_NativeEnum_from },` |
|       - |  987 | `		{ "tryFrom", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "string\|int $value", "?static",` |
|       - |  988 | `		  vm_builtin_NativeEnum_tryFrom },` |
|       - |  989 | `	};` |
|       - |  990 | `	sxu32 n;` |
|    5355 |  991 | `	sxi32 rc = NativeEnumInstallMethod(&(*pVm),pClass,&sCases);` |
|    5355 |  992 | `	if( rc != SXRET_OK \|\| pClass->nEnumBacking == 0 ){` |
|      48 |  993 | `		return rc;` |
|       - |  994 | `	}` |
|   15923 |  995 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFrom) ; n++ ){` |
|   10617 |  996 | `		rc = NativeEnumInstallMethod(&(*pVm),pClass,&aFrom[n]);` |
|   10617 |  997 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  998 | `			return rc;` |
|       - |  999 | `		}` |
|    5311 | 1000 | `	}` |
|    5311 | 1001 | `	return SXRET_OK;` |
|    2680 | 1002 | `}` |
|    5254 | 1003 | `PH7_PRIVATE sxi32 PH7_InstallNativeEnum(ph7_vm *pVm,const char *zName,sxu32 nBacking,` |
|       - | 1004 | `	const PH7_NativeEnumCase *aCase,sxu32 nCase,` |
|       - | 1005 | `	const PH7_NativeMethodDef *aMethod,sxu32 nMethod)` |
|       5 | 1006 | `{` |
|       - | 1007 | `	ph7_class *pClass, *pIface;` |
|       - | 1008 | `	SyString sName;` |
|       - | 1009 | `	sxu32 n;` |
|       - | 1010 | `	sxi32 rc;` |
|    5259 | 1011 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|    5259 | 1012 | `	pClass = PH7_NewRawClass(&(*pVm),&sName,0);` |
|    5259 | 1013 | `	if( pClass == 0 ){` |
|     ! 0 | 1014 | `		return SXERR_MEM;` |
|       - | 1015 | `	}` |
|       - | 1016 | `	/* php: an enum is implicitly FINAL and cannot be instantiated. */` |
|    5259 | 1017 | `	pClass->iFlags \|= PH7_CLASS_ENUM\|PH7_CLASS_FINAL;` |
|    5259 | 1018 | `	pClass->nEnumBacking = nBacking;` |
|    5259 | 1019 | `	rc = NativeEnumInstallProp(&(*pVm),pClass,"name",MEMOBJ_STRING,"string");` |
|    5259 | 1020 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1021 | `		return rc;` |
|       - | 1022 | `	}` |
|    5259 | 1023 | `	if( nBacking != 0 ){` |
|    7886 | 1024 | `		rc = NativeEnumInstallProp(&(*pVm),pClass,"value",nBacking,` |
|    2627 | 1025 | `			nBacking == MEMOBJ_INT ? "int" : "string");` |
|    5259 | 1026 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1027 | `			return rc;` |
|       - | 1028 | `		}` |
|    2627 | 1029 | `	}` |
|   15767 | 1030 | `	for( n = 0 ; n < nCase ; n++ ){` |
|       - | 1031 | `		ph7_class_attr *pAttr;` |
|       - | 1032 | `		SyString sCase;` |
|   10513 | 1033 | `		SyStringInitFromBuf(&sCase,aCase[n].zName,SyStrlen(aCase[n].zName));` |
|   10513 | 1034 | `		pAttr = PH7_NewClassAttr(&(*pVm),&sCase,0,PH7_CLASS_PROT_PUBLIC,` |
|       - | 1035 | `			PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_ENUMCASE);` |
|   10513 | 1036 | `		if( pAttr == 0 ){` |
|     ! 0 | 1037 | `			return SXERR_MEM;` |
|       - | 1038 | `		}` |
|   10513 | 1039 | `		pAttr->pDeclClass = pClass;` |
|       - | 1040 | `		/* The backing literal where a compiled case carries byte-code. */` |
|   10513 | 1041 | `		if( nBacking != 0 ){` |
|   10513 | 1042 | `			pAttr->pNativeValue = &aCase[n].sValue;` |
|    5254 | 1043 | `		}` |
|   10513 | 1044 | `		rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   10513 | 1045 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1046 | `			return rc;` |
|       - | 1047 | `		}` |
|       - | 1048 | `		/* Declaration order, which is the order cases() reports. */` |
|   10513 | 1049 | `		SySetPut(&pClass->aEnumCases,(const void *)&pAttr);` |
|    5259 | 1050 | `	}` |
|    5259 | 1051 | `	for( n = 0 ; n < nMethod ; n++ ){` |
|     ! 0 | 1052 | `		rc = PH7_NativeClassInstallMethod(&(*pVm),pClass,&aMethod[n],0);` |
|     ! 0 | 1053 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1054 | `			return rc;` |
|       - | 1055 | `		}` |
|     ! 0 | 1056 | `	}` |
|    5259 | 1057 | `	rc = PH7_InstallEnumInterfaceMethods(&(*pVm),pClass);` |
|    5259 | 1058 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1059 | `		return rc;` |
|       - | 1060 | `	}` |
|    5259 | 1061 | `	rc = PH7_VmInstallClass(&(*pVm),pClass);` |
|    5259 | 1062 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1063 | `		return rc;` |
|       - | 1064 | `	}` |
|       - | 1065 | ``	/* php 8.1: every enum satisfies `instanceof UnitEnum`, a backed one`` |
|       - | 1066 | ``	 * `BackedEnum` too. Attached AFTER the methods, so PH7_ClassImplement's`` |
|       - | 1067 | `	 * abstract stubbing finds cases()/from()/tryFrom() already declared. */` |
|    5259 | 1068 | `	pIface = NativeLookupClass(&(*pVm),"UnitEnum");` |
|    5259 | 1069 | `	if( pIface == 0 ){` |
|     ! 0 | 1070 | `		return SXERR_NOTFOUND;` |
|       - | 1071 | `	}` |
|    5259 | 1072 | `	rc = PH7_ClassImplement(pClass,pIface);` |
|    5259 | 1073 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1074 | `		return rc;` |
|       - | 1075 | `	}` |
|    5259 | 1076 | `	if( nBacking != 0 ){` |
|    5259 | 1077 | `		pIface = NativeLookupClass(&(*pVm),"BackedEnum");` |
|    5259 | 1078 | `		if( pIface == 0 ){` |
|     ! 0 | 1079 | `			return SXERR_NOTFOUND;` |
|       - | 1080 | `		}` |
|    5259 | 1081 | `		rc = PH7_ClassImplement(pClass,pIface);` |
|    5259 | 1082 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1083 | `			return rc;` |
|       - | 1084 | `		}` |
|    2627 | 1085 | `	}` |
|    5259 | 1086 | `	return VmMountUserClass(&(*pVm),pClass);` |
|    2632 | 1087 | `}` |
|       - | 1088 | `/*` |
|       - | 1089 | ` * ---------------------------------------------------------------------------` |
|       - | 1090 | ` * InternalIterator.` |
|       - | 1091 | ` *` |
|       - | 1092 | ` * A native IteratorAggregate cannot answer a Generator: a PHP generator IS its` |
|       - | 1093 | ` * byte-code, and a C body has none. php never answers one either -- its internal` |
|       - | 1094 | ` * aggregates hand back an InternalIterator wrapping the iterator their class` |
|       - | 1095 | ` * declared -- so PHL declares that class once, here, and every native aggregate` |
|       - | 1096 | ` * reaches it through ph7_class::pIterVtab.` |
|       - | 1097 | ` *` |
|       - | 1098 | ` * The cursor lives entirely in the iterator's own private slots. A vtable states` |
|       - | 1099 | ` * only how to REACH a position; reading it back is the same three methods for` |
|       - | 1100 | ` * everyone.` |
|       - | 1101 | ` * ---------------------------------------------------------------------------` |
|       - | 1102 | ` */` |
|       - | 1103 | `/* The walk THIS iterator was made for: the vtable of the aggregate it holds. */` |
|     572 | 1104 | `static const PH7_NativeIterVtab * NativeIterVtab(ph7_class_instance *pIt)` |
|       2 | 1105 | `{` |
|     574 | 1106 | `	ph7_class_instance *pSrc = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);` |
|     574 | 1107 | `	return pSrc ? pSrc->pClass->pIterVtab : 0;` |
|       2 | 1108 | `}` |
|     172 | 1109 | `static int vm_builtin_InternalIterator_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1110 | `{` |
|     174 | 1111 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       - | 1112 | `	const PH7_NativeIterVtab *pVtab;` |
|      86 | 1113 | `	SXUNUSED(nArg);` |
|      86 | 1114 | `	SXUNUSED(apArg);` |
|     174 | 1115 | `	if( pThis == 0 ){` |
|     ! 0 | 1116 | `		return PH7_OK;` |
|       - | 1117 | `	}` |
|     174 | 1118 | `	pVtab = NativeIterVtab(pThis);` |
|     174 | 1119 | `	if( pVtab == 0 \|\| pVtab->xRewind == 0 ){` |
|     ! 0 | 1120 | `		PH7_NativeSetAttrBool(pCtx->pVm,pThis,PH7_NATIVE_IT_DONE,1);` |
|     ! 0 | 1121 | `		return PH7_OK;` |
|       - | 1122 | `	}` |
|     174 | 1123 | `	pVtab->xRewind(pCtx->pVm,pThis);` |
|     174 | 1124 | `	return PH7_OK;` |
|      88 | 1125 | `}` |
|     400 | 1126 | `static int vm_builtin_InternalIterator_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1127 | `{` |
|     402 | 1128 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       - | 1129 | `	const PH7_NativeIterVtab *pVtab;` |
|     200 | 1130 | `	SXUNUSED(nArg);` |
|     200 | 1131 | `	SXUNUSED(apArg);` |
|     402 | 1132 | `	if( pThis == 0 \|\| PH7_NativeAttrTruthy(pThis,PH7_NATIVE_IT_DONE) ){` |
|     ! 0 | 1133 | `		return PH7_OK;` |
|       - | 1134 | `	}` |
|     402 | 1135 | `	pVtab = NativeIterVtab(pThis);` |
|     402 | 1136 | `	if( pVtab == 0 \|\| pVtab->xNext == 0 ){` |
|     ! 0 | 1137 | `		PH7_NativeSetAttrBool(pCtx->pVm,pThis,PH7_NATIVE_IT_DONE,1);` |
|     ! 0 | 1138 | `		return PH7_OK;` |
|       - | 1139 | `	}` |
|     402 | 1140 | `	pVtab->xNext(pCtx->pVm,pThis);` |
|     402 | 1141 | `	return PH7_OK;` |
|     202 | 1142 | `}` |
|     566 | 1143 | `static int vm_builtin_InternalIterator_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1144 | `{` |
|     568 | 1145 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     283 | 1146 | `	SXUNUSED(nArg);` |
|     283 | 1147 | `	SXUNUSED(apArg);` |
|     568 | 1148 | `	ph7_result_bool(pCtx,pThis != 0 && !PH7_NativeAttrTruthy(pThis,PH7_NATIVE_IT_DONE));` |
|     568 | 1149 | `	return PH7_OK;` |
|       2 | 1150 | `}` |
|       - | 1151 | `/* current() and key() answer the slots the walk left behind -- and NULL once it` |
|       - | 1152 | ` * is over, which is what php's exhausted InternalIterator answers too. */` |
|     548 | 1153 | `static int NativeIterReadSlot(ph7_context *pCtx,const char *zSlot)` |
|       2 | 1154 | `{` |
|     550 | 1155 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|       - | 1156 | `	ph7_value *pVal;` |
|     550 | 1157 | `	if( pThis == 0 \|\| PH7_NativeAttrTruthy(pThis,PH7_NATIVE_IT_DONE) ){` |
|     ! 0 | 1158 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1159 | `		return PH7_OK;` |
|       - | 1160 | `	}` |
|     550 | 1161 | `	pVal = PH7_NativeAttr(pThis,zSlot);` |
|     550 | 1162 | `	if( pVal ){` |
|     550 | 1163 | `		ph7_result_value(pCtx,pVal);` |
|     274 | 1164 | `	}` |
|     550 | 1165 | `	return PH7_OK;` |
|     276 | 1166 | `}` |
|     398 | 1167 | `static int vm_builtin_InternalIterator_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1168 | `{` |
|     199 | 1169 | `	SXUNUSED(nArg);` |
|     199 | 1170 | `	SXUNUSED(apArg);` |
|     400 | 1171 | `	return NativeIterReadSlot(pCtx,PH7_NATIVE_IT_CUR);` |
|       2 | 1172 | `}` |
|     150 | 1173 | `static int vm_builtin_InternalIterator_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1174 | `{` |
|      75 | 1175 | `	SXUNUSED(nArg);` |
|      75 | 1176 | `	SXUNUSED(apArg);` |
|     152 | 1177 | `	return NativeIterReadSlot(pCtx,PH7_NATIVE_IT_KEY);` |
|       2 | 1178 | `}` |
|       - | 1179 | `/* InternalIterator::__construct() — private in php, and never reached from PHP:` |
|       - | 1180 | ` * PH7_NativeIteratorNew builds the instance directly. */` |
|     ! 0 | 1181 | `static int vm_builtin_InternalIterator_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 | 1182 | `{` |
|     ! 0 | 1183 | `	SXUNUSED(nArg);` |
|     ! 0 | 1184 | `	SXUNUSED(apArg);` |
|     ! 0 | 1185 | `	SXUNUSED(pCtx);` |
|     ! 0 | 1186 | `	return PH7_OK;` |
|     ! 0 | 1187 | `}` |
|       - | 1188 | `/*` |
|       - | 1189 | ` * The iterator a native getIterator() answers: bound to its aggregate and already` |
|       - | 1190 | ` * positioned, because php's is valid() before the first rewind().` |
|       - | 1191 | ` */` |
|     190 | 1192 | `PH7_PRIVATE ph7_class_instance * PH7_NativeIteratorNew(ph7_vm *pVm,ph7_class_instance *pSrc)` |
|       2 | 1193 | `{` |
|     192 | 1194 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),"InternalIterator",` |
|       - | 1195 | `		sizeof("InternalIterator")-1,FALSE,0);` |
|       - | 1196 | `	ph7_class_instance *pIt;` |
|       - | 1197 | `	const PH7_NativeIterVtab *pVtab;` |
|     192 | 1198 | `	if( pClass == 0 \|\| pSrc == 0 ){` |
|     ! 0 | 1199 | `		return 0;` |
|       - | 1200 | `	}` |
|     192 | 1201 | `	pIt = PH7_NewClassInstance(&(*pVm),pClass);` |
|     192 | 1202 | `	if( pIt == 0 ){` |
|     ! 0 | 1203 | `		return 0;` |
|       - | 1204 | `	}` |
|     192 | 1205 | `	PH7_NativeSetAttrObj(&(*pVm),pIt,PH7_NATIVE_IT_SRC,pSrc);` |
|     192 | 1206 | `	PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|     192 | 1207 | `	pVtab = pSrc->pClass->pIterVtab;` |
|     192 | 1208 | `	if( pVtab && pVtab->xRewind ){` |
|     192 | 1209 | `		pVtab->xRewind(&(*pVm),pIt);` |
|      95 | 1210 | `	}` |
|     192 | 1211 | `	return pIt;` |
|      97 | 1212 | `}` |
|    5254 | 1213 | `PH7_PRIVATE sxi32 PH7_VmInstallNativeIterator(ph7_vm *pVm)` |
|       5 | 1214 | `{` |
|       - | 1215 | `	static const PH7_NativePropDef aProp[] = {` |
|       - | 1216 | `		{ PH7_NATIVE_IT_SRC,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|       - | 1217 | `		{ PH7_NATIVE_IT_CUR,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|       - | 1218 | `		{ PH7_NATIVE_IT_KEY,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|       - | 1219 | `		{ PH7_NATIVE_IT_POS,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,  0, 0, 0.0 }, 0 },` |
|       - | 1220 | `		{ PH7_NATIVE_IT_AUX,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,  0, 0, 0.0 }, 0 },` |
|       - | 1221 | `		{ PH7_NATIVE_IT_DONE, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_BOOL, 1, 0, 0.0 }, 0 },` |
|       - | 1222 | `	};` |
|       - | 1223 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|       - | 1224 | `		{ "__construct", PH7_MOD_PRIVATE, "", "", vm_builtin_InternalIterator_construct },` |
|       - | 1225 | `		{ "current",     PH7_MOD_PUBLIC, "", "mixed", vm_builtin_InternalIterator_current },` |
|       - | 1226 | `		{ "key",         PH7_MOD_PUBLIC, "", "mixed", vm_builtin_InternalIterator_key },` |
|       - | 1227 | `		{ "next",        PH7_MOD_PUBLIC, "", "void",  vm_builtin_InternalIterator_next },` |
|       - | 1228 | `		{ "valid",       PH7_MOD_PUBLIC, "", "bool",  vm_builtin_InternalIterator_valid },` |
|       - | 1229 | `		{ "rewind",      PH7_MOD_PUBLIC, "", "void",  vm_builtin_InternalIterator_rewind },` |
|       - | 1230 | `	};` |
|       - | 1231 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|       - | 1232 | `		{ "InternalIterator", 0, 0, PH7_CLASS_FINAL,` |
|       - | 1233 | `		  aMethod, SX_ARRAYSIZE(aMethod), 0, 0, aProp, SX_ARRAYSIZE(aProp), 0, 0, 0 },` |
|       - | 1234 | `	};` |
|       - | 1235 | `	ph7_class *pIt,*pIterator;` |
|       - | 1236 | `	sxi32 rc;` |
|    5259 | 1237 | `	rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|    5259 | 1238 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1239 | `		return rc;` |
|       - | 1240 | `	}` |
|       - | 1241 | ``	/* `implements Iterator` only NOW: PH7_ClassImplement stubs every interface`` |
|       - | 1242 | `	 * method the class does not already declare as ABSTRACT, so attaching it` |
|       - | 1243 | `	 * through the spec would have made InternalIterator uninstantiable. */` |
|    5259 | 1244 | `	pIt = NativeLookupClass(&(*pVm),"InternalIterator");` |
|    5259 | 1245 | `	pIterator = NativeLookupClass(&(*pVm),"Iterator");` |
|    5259 | 1246 | `	if( pIt == 0 \|\| pIterator == 0 ){` |
|     ! 0 | 1247 | `		return SXERR_NOTFOUND;` |
|       - | 1248 | `	}` |
|    5259 | 1249 | `	return PH7_ClassImplement(pIt,pIterator);` |
|    2632 | 1250 | `}` |
|       - | 1251 |  |
