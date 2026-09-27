# src/ph7/vm_ops_load.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1349/1477 lines (91.33%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|       - |    7 | `/*` |
|       - |    8 | ` * Section:` |
|       - |    9 | ` *    Opcode handlers extracted from vm.c's dispatch loop. Each handler runs` |
|       - |   10 | ` *    one opcode arm against the caller's VmExecState: the loop syncs pTos/pc` |
|       - |   11 | ` *    in, calls the handler, reloads them and routes the returned VmOpRc onto` |
|       - |   12 | ` *    its labels (same idiom as VmCallFinish).` |
|       - |   13 | ` * Status:` |
|       - |   14 | ` *    Stable.` |
|       - |   15 | ` */` |
|       - |   16 | `/* Bind the dispatch-routing exits to handler semantics (see vm_dispatch.h),` |
|       - |   17 | ` * and map the macros' sState references onto our state parameter. */` |
|       - |   18 | `#define VM_EXIT_BREAK      { pState->pTos = pTos; pState->pc = pc; return VM_OP_NEXT; }` |
|       - |   19 | `#define VM_EXIT_ABORT      { pState->pTos = pTos; pState->pc = pc; return VM_OP_ABORT; }` |
|       - |   20 | `#define VM_EXIT_EXCEPTION  { pState->pTos = pTos; pState->pc = pc; return VM_OP_EXCEPTION; }` |
|       - |   21 | `#include "vm_dispatch.h"` |
|       - |   22 | `#define sState (*pState)` |
|       - |   23 |  |
|       - |   24 | `/*` |
|       - |   25 | ` * OP_STORE_REF: body moved verbatim from the OP_STORE_REF arm of` |
|       - |   26 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |   27 | ` */` |
|    3214 |   28 | `PH7_PRIVATE VmOpRc VmExecOpStoreRef(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       4 |   29 | `{` |
|    3218 |   30 | `	ph7_value *pTos = pState->pTos;` |
|    3218 |   31 | `	ph7_value *pStack = pState->pStack;` |
|    3218 |   32 | `	VmInstr *aInstr = pState->aInstr;` |
|    3218 |   33 | `	sxi32 pc = pState->pc;` |
|       - |   34 | `	sxi32 rc;` |
|    1607 |   35 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    3218 |   36 | `	 SyString sName = { 0 , 0 };` |
|       - |   37 | `	 VmFrame *pFrameLocal;` |
|       - |   38 | `	SyHashEntry *pEntry;` |
|       - |   39 | `	sxu32 nIdx;` |
|       - |   40 | `#ifdef UNTRUST` |
|       - |   41 | `	if( pTos < pStack ){` |
|       - |   42 | `		VM_EXIT_ABORT;` |
|       - |   43 | `	}` |
|       - |   44 | `#endif` |
|    3218 |   45 | `	if( pInstr->iP2 == 1 ){` |
|       - |   46 | ``		/* Member reference target: `$o->p =& $x` / `self::$s =& $x`. The`` |
|       - |   47 | `		 * preceding OP_MEMBER (PH7_MEMBER_REF_TARGET) resolved the property slot` |
|       - |   48 | `		 * and stashed it. Stack: [ ... , source, member-result(top) ]. Rebind the` |
|       - |   49 | `		 * property's nIdx to alias the source variable's slot and pin that slot` |
|       - |   50 | `		 * past its owning frame (like a use(&$x) capture) so neither frame` |
|       - |   51 | `		 * teardown nor a later unset recycles it while the property aliases it. */` |
|      44 |   52 | `		ph7_value *pSrc = &pTos[-1];` |
|      44 |   53 | `		sxu32 nSrcIdx = pSrc->nIdx;` |
|      44 |   54 | `		VmClassAttr *pVmAttr = pVm->pRefTargetAttr;` |
|      44 |   55 | `		ph7_class_attr *pStAttr = pVm->pRefTargetStaticAttr;` |
|      44 |   56 | `		if( pSrc->iFlags & MEMOBJ_AUX_STROFFSET ){` |
|       - |   57 | ``			/* `$o->p =& $s[1]`: a string offset is not a slot (its index is the`` |
|       - |   58 | `			 * BASE STRING's), and php refuses the reference outright. Settle the` |
|       - |   59 | `			 * stashed target state exactly as the success path does, then throw. */` |
|       5 |   60 | `			if( pVm->pRefTargetThis ){` |
|       3 |   61 | `				PH7_ClassInstanceUnref(pVm->pRefTargetThis);` |
|       1 |   62 | `			}` |
|       5 |   63 | `			pVm->pRefTargetAttr = 0;` |
|       5 |   64 | `			pVm->pRefTargetStaticAttr = 0;` |
|       5 |   65 | `			pVm->pRefTargetThis = 0;` |
|       5 |   66 | `			VmPopOperand(&pTos,1); /* the member result */` |
|       5 |   67 | `			rc = VmThrowFromVm(&(*pVm),"Error","Cannot create references to/from string offsets",` |
|       - |   68 | `				sizeof("Cannot create references to/from string offsets")-1);` |
|       5 |   69 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       5 |   70 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |   71 | `		}` |
|      39 |   72 | `		if( nSrcIdx == SXU32_HIGH ){` |
|       - |   73 | ``			/* php: the RHS of `=&` must be a variable, not a constant expression.`` |
|       - |   74 | `			 * (The compiler already rejects the obvious literal forms.) */` |
|     ! 0 |   75 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - |   76 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      39 |   77 | `		}else if( pVmAttr ){` |
|      29 |   78 | `			sxu32 nOldIdx = pVmAttr->nIdx;` |
|      29 |   79 | `			if( nOldIdx != nSrcIdx ){` |
|      29 |   80 | `				if( (pVmAttr->iState & VM_CLASS_ATTR_REFBOUND) == 0 ){` |
|       - |   81 | `					/* Release this property's own (unshared) slot before repointing.` |
|       - |   82 | `					 * A reference-bound property bypasses typed coercion in php, so` |
|       - |   83 | `					 * drop any typed-slot enforcement entry too. */` |
|      23 |   84 | `					PH7_VmStoreFilterDrop(&(*pVm),pVmAttr->pAttr,nOldIdx);` |
|      23 |   85 | `					PH7_VmUnsetMemObj(&(*pVm),nOldIdx,TRUE);` |
|      12 |   86 | `				}else{` |
|       - |   87 | `					/* Already bound elsewhere: give that slot its pin back, which` |
|       - |   88 | `					 * releases it when this property was its last holder. */` |
|       7 |   89 | `					VmUnpinMemObjSlot(&(*pVm),nOldIdx);` |
|       - |   90 | `				}` |
|      29 |   91 | `				pVmAttr->nIdx = nSrcIdx;` |
|      29 |   92 | `				pVmAttr->iState \|= VM_CLASS_ATTR_REFBOUND;` |
|      29 |   93 | `				pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      29 |   94 | `				VmPinMemObjSlotCounted(&(*pVm),nSrcIdx);` |
|      15 |   95 | `			}` |
|      25 |   96 | `		}else if( pStAttr ){` |
|      11 |   97 | `			sxu32 nOldIdx = pStAttr->nIdx;` |
|      11 |   98 | `			if( nOldIdx != nSrcIdx ){` |
|       - |   99 | `				/* Give the previous target back, exactly as the instance arm above does.` |
|       - |  100 | `				 * A permanent pin was left on every slot the property had ever named, so` |
|       - |  101 | `				 * each of them stayed a REFERENCE for the rest of the script — which an` |
|       - |  102 | ``				 * ordinary array COPY then shared (`$d = $a; $d[0] = 99;` wrote through`` |
|       - |  103 | ``				 * to `$a[0]`, silently), since "is this element a reference" is answered`` |
|       - |  104 | `				 * by who still holds it. */` |
|      11 |  105 | `				if( (pStAttr->iFlags & PH7_CLASS_ATTR_REFBOUND) == 0 ){` |
|       - |  106 | `					/* The static's own (unshared) slot. A reference-bound property bypasses` |
|       - |  107 | `					 * typed coercion in php, so drop any typed-slot enforcement entry too. */` |
|       7 |  108 | `					PH7_VmStoreFilterDrop(&(*pVm),pStAttr,nOldIdx);` |
|       7 |  109 | `					PH7_VmUnsetMemObj(&(*pVm),nOldIdx,TRUE);` |
|       4 |  110 | `				}else{` |
|       5 |  111 | `					VmUnpinMemObjSlot(&(*pVm),nOldIdx);` |
|       - |  112 | `				}` |
|      11 |  113 | `				pStAttr->nIdx = nSrcIdx;` |
|      11 |  114 | `				pStAttr->iFlags \|= PH7_CLASS_ATTR_REFBOUND;` |
|      11 |  115 | `				VmPinMemObjSlotCounted(&(*pVm),nSrcIdx);` |
|       5 |  116 | `			}` |
|       5 |  117 | `		}` |
|      39 |  118 | `		if( pVm->pRefTargetThis ){` |
|      29 |  119 | `			PH7_ClassInstanceUnref(pVm->pRefTargetThis);` |
|      14 |  120 | `		}` |
|      39 |  121 | `		pVm->pRefTargetAttr = 0;` |
|      39 |  122 | `		pVm->pRefTargetStaticAttr = 0;` |
|      39 |  123 | `		pVm->pRefTargetThis = 0;` |
|       - |  124 | `		/* Pop the member-result; leave the source as the expression value. */` |
|      39 |  125 | `		VmPopOperand(&pTos,1);` |
|      39 |  126 | `		VM_EXIT_BREAK;` |
|       - |  127 | `	}` |
|    3176 |  128 | `	if( pInstr->p3 == 0 ){` |
|       - |  129 | `		char *zName;` |
|       - |  130 | `		/* Take the variable name from the Next on the stack */` |
|     ! 0 |  131 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  132 | `			/* Force a string cast */` |
|     ! 0 |  133 | `			PH7_MemObjToString(pTos);` |
|     ! 0 |  134 | `		}` |
|     ! 0 |  135 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|     ! 0 |  136 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|     ! 0 |  137 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     ! 0 |  138 | `			if( zName ){` |
|     ! 0 |  139 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|     ! 0 |  140 | `			}` |
|     ! 0 |  141 | `		}` |
|     ! 0 |  142 | `		PH7_MemObjRelease(pTos);` |
|     ! 0 |  143 | `		pTos--;` |
|     ! 0 |  144 | `	}else{` |
|    3176 |  145 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|       - |  146 | `	}` |
|    3176 |  147 | `	if( pTos->iFlags & MEMOBJ_AUX_STROFFSET ){` |
|       - |  148 | `		/* php: a string OFFSET cannot be either end of a reference. The value read` |
|       - |  149 | `		 * out of a string still carries the BASE VARIABLE's slot, so binding it` |
|       - |  150 | `		 * aliased the whole string and a later write through the reference REPLACED` |
|       - |  151 | `		 * it ($s = "abc"; $r = &$s[1]; $r = "Z"; left $s === "Z"). Same Error the` |
|       - |  152 | `		 * by-ref ARGUMENT path raises for f($s[1]). */` |
|       7 |  153 | `		rc = VmThrowFromVm(&(*pVm),"Error","Cannot create references to/from string offsets",` |
|       - |  154 | `			sizeof("Cannot create references to/from string offsets")-1);` |
|       7 |  155 | `		PH7_MemObjRelease(pTos);` |
|       7 |  156 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|       7 |  157 | `		pTos->nIdx = SXU32_HIGH;` |
|       7 |  158 | `		if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       9 |  159 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  160 | `	}` |
|    3170 |  161 | `	nIdx = pTos->nIdx;` |
|    3170 |  162 | `	if(nIdx == SXU32_HIGH ){` |
|       7 |  163 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES\|MEMOBJ_AUX_NATIVEPROP)) == 0 ){` |
|     ! 0 |  164 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - |  165 | `				"Reference operator require a variable not a constant as it's right operand");` |
|     ! 0 |  166 | `		}else{` |
|       - |  167 | `			/* An object/array/resource value, or a NATIVE class's handler-backed` |
|       - |  168 | ``			 * property (`$r = &$i->f`), which php binds to a fresh variable in`` |
|       - |  169 | `			 * silence because there is no slot behind it to alias. */` |
|       - |  170 | `			ph7_value *pObj;` |
|       - |  171 | `			/* Extract the desired variable and if not available dynamically create it */` |
|       7 |  172 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|       7 |  173 | `			if( pObj == 0 ){` |
|     ! 0 |  174 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - |  175 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|     ! 0 |  176 | `				VM_EXIT_ABORT;` |
|       - |  177 | `			}` |
|       - |  178 | `			/* Perform the store operation */` |
|       7 |  179 | `			PH7_MemObjStore(pTos,pObj);` |
|       7 |  180 | `			pTos->nIdx = pObj->nIdx;` |
|       1 |  181 | `		}` |
|    3167 |  182 | `	}else if( sName.nByte > 0){` |
|    3164 |  183 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|       - |  184 | `			/* php 8.1's non-catchable fatal (compile-time there) */` |
|       3 |  185 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot acquire reference to $GLOBALS");` |
|       3 |  186 | `			pVm->iExitStatus = 255;` |
|       3 |  187 | `			pVm->bHaltRequested = 1;` |
|       3 |  188 | `			VM_EXIT_ABORT;` |
|     ! 0 |  189 | `		}else{` |
|    3162 |  190 | `			pFrameLocal = pVm->pFrame;` |
|    3162 |  191 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       - |  192 | `			/* Query the local frame */` |
|    3162 |  193 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|    3162 |  194 | `			if( pEntry ){` |
|       - |  195 | ``				/* php RE-BINDS a name that already exists (`$y = 2; $y = &$x;`, and the`` |
|       - |  196 | ``				 * `$r = &$a[$k]` idiom from the second loop step on) — the old binding`` |
|       - |  197 | `				 * goes, its value with it if nothing else holds it. */` |
|    3083 |  198 | `				PH7_VmRebindVarSlot(&(*pVm),pFrameLocal,pEntry,sName.zString,sName.nByte,nIdx);` |
|    3083 |  199 | `				if( pInstr->p3 == 0 && sName.zString ){` |
|       - |  200 | `					/* The name was duplicated for a symbol-table key this rebind does` |
|       - |  201 | `					 * not need — the entry keeps the key it was created with. */` |
|     ! 0 |  202 | `					SyMemBackendFree(&pVm->sAllocator,(void *)sName.zString);` |
|     ! 0 |  203 | `				}` |
|    1542 |  204 | `			}else{` |
|      80 |  205 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|      80 |  206 | `				if( pFrameLocal->pParent == 0 ){` |
|       - |  207 | `					/* Insert in the $GLOBALS array */` |
|      64 |  208 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|      30 |  209 | `				}` |
|      80 |  210 | `				if( rc == SXRET_OK ){` |
|      80 |  211 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|      38 |  212 | `				}` |
|       - |  213 | `			}` |
|       - |  214 | `		}` |
|    1579 |  215 | `	}` |
|    3168 |  216 | `	VM_EXIT_BREAK;` |
|     ! 0 |  217 | `	VM_EXIT_BREAK;` |
|    1611 |  218 | `}` |
|       - |  219 |  |
|       - |  220 | `/* LOAD_IDX's own iP2 context codes (they do NOT line up with PH7_MEMBER_*). */` |
|       - |  221 | `#define VM_IDX_CTX_ISSET 4` |
|       - |  222 | `#define VM_IDX_CTX_UNSET 5` |
|       - |  223 | ``/* An INTERMEDIATE subscript of an unset chain (`unset($a['k']['n'])`): every unset`` |
|       - |  224 | ` * rule below applies to it — COW-separate the parent, never vivify a missing key,` |
|       - |  225 | ` * unset's own wording for a bad base — except the removal itself, which belongs to` |
|       - |  226 | ` * the OUTERMOST subscript alone. */` |
|       - |  227 | `#define VM_IDX_CTX_UNSET_BASE 10` |
|       - |  228 | `#define VM_IDX_IS_UNSET(iP2) ((iP2) == VM_IDX_CTX_UNSET \|\| (iP2) == VM_IDX_CTX_UNSET_BASE)` |
|       - |  229 | `#define VM_IDX_CTX_EMPTY 6` |
|       - |  230 | `/*` |
|       - |  231 | ` * php rejects an OBJECT or an ARRAY used as an array offset, naming the offending` |
|       - |  232 | ` * type the way get_debug_type() does — the CLASS name for an object, "array" for` |
|       - |  233 | ` * an array — and wording the failure by context:` |
|       - |  234 | ` *` |
|       - |  235 | ` *   read/write   Cannot access offset of type Foo on array` |
|       - |  236 | ` *   isset/empty  Cannot access offset of type Foo in isset or empty` |
|       - |  237 | ` *   unset        Cannot unset offset of type Foo on array` |
|       - |  238 | ` *` |
|       - |  239 | ` * A RESOURCE is deliberately absent: php does not reject it, it warns` |
|       - |  240 | ` * ("Resource ID#N used as offset, casting to integer (N)") and uses the id as an` |
|       - |  241 | ` * integer key. VmOffsetResourceWarn() below handles that half.` |
|       - |  242 | ` *` |
|       - |  243 | ` * Returns TRUE and fills pMsg when the key must be rejected. iCtx is the` |
|       - |  244 | ` * instruction's iP2 (any value other than the isset/unset/empty codes reads as` |
|       - |  245 | ` * an access).` |
|       - |  246 | ` */` |
|  272631 |  247 | `static int VmOffsetTypeRejected(ph7_vm *pVm,ph7_value *pKey,int iCtx,SyBlob *pMsg)` |
|       5 |  248 | `{` |
|       - |  249 | `	const char *zType;` |
|  272636 |  250 | `	SyString *pClass = 0;` |
|  272636 |  251 | `	if( pKey == 0 ){` |
|     ! 0 |  252 | `		return FALSE;` |
|       - |  253 | `	}` |
|  272636 |  254 | `	if( pKey->iFlags & MEMOBJ_OBJ ){` |
|      26 |  255 | `		ph7_class_instance *pInst = (ph7_class_instance *)pKey->x.pOther;` |
|      26 |  256 | `		if( pInst && pInst->pClass ){` |
|      26 |  257 | `			pClass = &pInst->pClass->sName;` |
|      12 |  258 | `		}` |
|      26 |  259 | `		zType = "object";` |
|  272624 |  260 | `	}else if( pKey->iFlags & MEMOBJ_HASHMAP ){` |
|      15 |  261 | `		zType = "array";` |
|       8 |  262 | `	}else{` |
|  272598 |  263 | `		return FALSE;` |
|       - |  264 | `	}` |
|      40 |  265 | `	SyBlobInit(pMsg,&pVm->sAllocator);` |
|      40 |  266 | `	if( VM_IDX_IS_UNSET(iCtx) ){` |
|       3 |  267 | `		SyBlobAppend(pMsg,"Cannot unset offset of type ",sizeof("Cannot unset offset of type ")-1);` |
|       2 |  268 | `	}else{` |
|      38 |  269 | `		SyBlobAppend(pMsg,"Cannot access offset of type ",sizeof("Cannot access offset of type ")-1);` |
|       - |  270 | `	}` |
|      40 |  271 | `	if( pClass ){` |
|      26 |  272 | `		SyBlobAppend(pMsg,pClass->zString,pClass->nByte);` |
|      14 |  273 | `	}else{` |
|      15 |  274 | `		SyBlobAppend(pMsg,zType,(sxu32)SyStrlen(zType));` |
|       - |  275 | `	}` |
|      40 |  276 | `	if( iCtx == VM_IDX_CTX_ISSET \|\| iCtx == VM_IDX_CTX_EMPTY ){` |
|       7 |  277 | `		SyBlobAppend(pMsg," in isset or empty",sizeof(" in isset or empty")-1);` |
|       4 |  278 | `	}else{` |
|      34 |  279 | `		SyBlobAppend(pMsg," on array",sizeof(" on array")-1);` |
|       - |  280 | `	}` |
|      40 |  281 | `	return TRUE;` |
|  136320 |  282 | `}` |
|       - |  283 | `/*` |
|       - |  284 | ` * php's other half of the offset-type rules: a RESOURCE offset is accepted, with` |
|       - |  285 | `` * `Warning: Resource ID#N used as offset, casting to integer (N)`, and the key`` |
|       - |  286 | ` * becomes that integer. Rewrites pKey in place so the normal integer-key path` |
|       - |  287 | ` * takes over.` |
|       - |  288 | ` */` |
|  272593 |  289 | `static void VmOffsetResourceWarn(ph7_vm *pVm,ph7_value *pKey)` |
|       5 |  290 | `{` |
|       - |  291 | `	sxu32 nId;` |
|  272598 |  292 | `	if( pKey == 0 \|\| (pKey->iFlags & MEMOBJ_RES) == 0 ){` |
|  272588 |  293 | `		return;` |
|       - |  294 | `	}` |
|      11 |  295 | `	nId = PH7_VmResourceId(pVm,pKey->x.pOther);` |
|      16 |  296 | `	VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       5 |  297 | `		"Resource ID#%u used as offset, casting to integer (%u)",nId,nId);` |
|      11 |  298 | `	PH7_MemObjRelease(pKey);` |
|      11 |  299 | `	pKey->x.iVal = (sxi64)nId;` |
|      11 |  300 | `	MemObjSetType(pKey,MEMOBJ_INT);` |
|  136301 |  301 | `}` |
|       - |  302 | `/*` |
|       - |  303 | ` * php DEPRECATES (it does not reject) a NULL array offset, then normalizes it to` |
|       - |  304 | `` * the empty-string key "": `$a[null]`, `$a[$undef]` and `[null => v]` all warn`` |
|       - |  305 | `` * `Using null as an array offset is deprecated, use an empty string instead` and`` |
|       - |  306 | ` * then read/write the "" slot. Emit that E_DEPRECATED notice and return TRUE so` |
|       - |  307 | ` * the caller falls through to the ordinary lookup/insert, which already casts` |
|       - |  308 | ` * NULL->"" (HashmapLookup / HashmapInsert). A LOSSY-FLOAT offset stays a rejected` |
|       - |  309 | ` * TypeError: PHL deliberately targets php's NON-deprecated surface for the` |
|       - |  310 | ` * float->int truncation (VmRejectFloatOperand policy, §2), so only the null case` |
|       - |  311 | ` * is coerced here. Returns FALSE (no notice) for a non-null key.` |
|       - |  312 | ` */` |
|  390281 |  313 | `static int VmNullOffsetDeprecate(ph7_vm *pVm,ph7_value *pKey)` |
|       5 |  314 | `{` |
|  390286 |  315 | `	if( pKey == 0 \|\| (pKey->iFlags & MEMOBJ_NULL) == 0 ){` |
|  390262 |  316 | `		return FALSE;` |
|       - |  317 | `	}` |
|      27 |  318 | `	PH7_VmThrowError(&(*pVm),0,E_DEPRECATED,` |
|       - |  319 | `		"Using null as an array offset is deprecated, use an empty string instead");` |
|      27 |  320 | `	return TRUE;` |
|  195134 |  321 | `}` |
|       - |  322 | `/*` |
|       - |  323 | ` * The three rules above, applied to a BUILTIN's key argument.` |
|       - |  324 | ` *` |
|       - |  325 | `` * php's array_key_exists() does not run a `string\|int` ZPP row on its $key — it`` |
|       - |  326 | `` * hands the value to the same offset machinery `$a[$key]` uses, so the two agree`` |
|       - |  327 | ` * on every type: an object or an array is the catchable` |
|       - |  328 | `` * `Cannot access offset of type X on array`, a RESOURCE warns and becomes its`` |
|       - |  329 | `` * integer id, `null` deprecates and reads the "" key, and a bool/float/numeric`` |
|       - |  330 | ` * string folds the way any subscript folds. PHL's builtin had its own narrower` |
|       - |  331 | `` * check and therefore its own answers — a `string\|int` TypeError for the two`` |
|       - |  332 | `` * cases php ACCEPTS (null, lossy float) and a silent `false` for the three php`` |
|       - |  333 | ` * REJECTS or coerces (object, array, resource). The object case was the worst of` |
|       - |  334 | ` * them: a __toString() object was stringified and could answer TRUE for a key` |
|       - |  335 | ` * php refuses to look up at all.` |
|       - |  336 | ` *` |
|       - |  337 | ` * bZppWording picks which of php's two messages the caller reports. php words the` |
|       - |  338 | ` * illegal-type rejection differently in the alias than in the function itself —` |
|       - |  339 | `` * `key_exists(): Argument #1 ($key) must be a valid array offset type` vs the`` |
|       - |  340 | ` * engine's offset Error — verified against 8.5.8; the null-key DEPRECATION is the` |
|       - |  341 | ` * array_key_exists() wording in both.` |
|       - |  342 | ` *` |
|       - |  343 | ` * pKey is rewritten in place (resource -> int), so callers pass a private copy,` |
|       - |  344 | ` * not the caller's own argument slot. Returns SXRET_OK when the key is usable, or` |
|       - |  345 | ` * the status of the TypeError thrown.` |
|       - |  346 | ` */` |
|     128 |  347 | `PH7_PRIVATE sxi32 PH7_VmArrayKeyArg(ph7_context *pCtx,ph7_value *pKey,int bZppWording)` |
|       5 |  348 | `{` |
|     133 |  349 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - |  350 | `	SyBlob sMsg;` |
|     133 |  351 | `	if( VmOffsetTypeRejected(pVm,pKey,0,&sMsg) ){` |
|       - |  352 | `		sxi32 rc;` |
|      11 |  353 | `		if( bZppWording ){` |
|       5 |  354 | `			SyBlobRelease(&sMsg);` |
|       7 |  355 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  356 | `				"%s(): Argument #1 ($key) must be a valid array offset type",` |
|       2 |  357 | `				ph7_function_name(pCtx));` |
|       - |  358 | `		}` |
|      10 |  359 | `		rc = PH7_VmThrowException(pCtx,"TypeError","%.*s",` |
|       6 |  360 | `			(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|       7 |  361 | `		SyBlobRelease(&sMsg);` |
|       7 |  362 | `		return rc;` |
|       - |  363 | `	}` |
|     123 |  364 | `	VmOffsetResourceWarn(pVm,pKey);` |
|     118 |  365 | `	if( (pKey->iFlags & MEMOBJ_REAL)` |
|      69 |  366 | `	 && pKey->rVal != (ph7_real)(sxi64)pKey->rVal ){` |
|       - |  367 | `		/* php DEPRECATES the lossy float and truncates; PHL rejects it, here as` |
|       - |  368 | ``		 * everywhere else (§10) — and with the SAME message `$a[5.7]` gives, so`` |
|       - |  369 | `		 * the builtin and the subscript stay one rule. */` |
|       7 |  370 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  371 | `			"Cannot access offset of type float on array");` |
|       - |  372 | `	}` |
|     117 |  373 | `	if( pKey->iFlags & MEMOBJ_NULL ){` |
|       - |  374 | `		/* php names the parameter here rather than "an array offset"; the effect` |
|       - |  375 | `		 * is the engine's — the lookup below reads the "" key. */` |
|       3 |  376 | `		PH7_VmThrowError(pVm,0,E_DEPRECATED,` |
|       - |  377 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|       - |  378 | `			"use an empty string instead");` |
|       1 |  379 | `	}` |
|     117 |  380 | `	return SXRET_OK;` |
|      69 |  381 | `}` |
|       - |  382 | `/*` |
|       - |  383 | ` * Does this string START with an integer php's is_numeric_string would answer` |
|       - |  384 | ` * IS_LONG for? Returns 0 when it does not — no digits at all ("", "-", "p"), a` |
|       - |  385 | ` * FLOAT-shaped prefix ("1.5", ".5", "1.", "1e2", "1E2x") or a run that overflows` |
|       - |  386 | ` * a signed 64-bit int ("9223372036854775808") — 1 when it does and only optional` |
|       - |  387 | ` * trailing WHITESPACE follows (" 12 ", "+12", "007"), and 2 when it does but` |
|       - |  388 | ` * trailing DATA follows ("12abc", "0x1", "1e", "1 x"), which is php's` |
|       - |  389 | `` * `Illegal string offset` warning. *piVal receives the integer for 1 and 2.`` |
|       - |  390 | ` *` |
|       - |  391 | ` * php reaches the same three answers through is_numeric_string_ex(allow_errors=1)` |
|       - |  392 | ` * plus its IS_LONG/IS_DOUBLE verdict: a fraction or a well-formed exponent makes` |
|       - |  393 | ` * the verdict IS_DOUBLE, which a string offset refuses, while a bare 'e' is just` |
|       - |  394 | ` * trailing data.` |
|       - |  395 | ` */` |
|     100 |  396 | `static int VmStringOffsetInt(const char *zIn,sxu32 nByte,sxi64 *piVal)` |
|       4 |  397 | `{` |
|     104 |  398 | `	const char *z = zIn, *zEnd = &zIn[nByte], *zDigit;` |
|     104 |  399 | `	sxu64 uVal = 0, uLimit;` |
|     104 |  400 | `	int isNeg = 0, nDigit, i;` |
|     116 |  401 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){` |
|      14 |  402 | `		z++;` |
|       2 |  403 | `	}` |
|     104 |  404 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|       7 |  405 | `		isNeg = z[0] == '-';` |
|       7 |  406 | `		z++;` |
|       3 |  407 | `	}` |
|     104 |  408 | `	zDigit = z;` |
|     242 |  409 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|     140 |  410 | `		z++;` |
|       2 |  411 | `	}` |
|     104 |  412 | `	nDigit = (int)(z - zDigit);` |
|     104 |  413 | `	if( nDigit < 1 ){` |
|      42 |  414 | `		return 0;` |
|       - |  415 | `	}` |
|      64 |  416 | `	if( z < zEnd && z[0] == '.' ){` |
|       - |  417 | `		/* "1." and "1.5" alike: php reads a double from here. */` |
|       5 |  418 | `		return 0;` |
|       - |  419 | `	}` |
|      60 |  420 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|       3 |  421 | `		const char *zExp = &z[1];` |
|       3 |  422 | `		if( zExp < zEnd && (zExp[0] == '+' \|\| zExp[0] == '-') ){` |
|     ! 0 |  423 | `			zExp++;` |
|     ! 0 |  424 | `		}` |
|       3 |  425 | `		if( zExp < zEnd && (unsigned char)zExp[0] < 0xc0 && SyisDigit(zExp[0]) ){` |
|       3 |  426 | `			return 0;` |
|       - |  427 | `		}` |
|     ! 0 |  428 | `	}` |
|       - |  429 | `	/* Accumulate unsigned so PHP_INT_MIN's magnitude (2^63) is representable —` |
|       - |  430 | `	 * "-9223372036854775808" is a legal offset, "9223372036854775808" is not. */` |
|      62 |  431 | `	while( nDigit > 1 && zDigit[0] == '0' ){` |
|       5 |  432 | `		zDigit++; nDigit--;` |
|       1 |  433 | `	}` |
|      58 |  434 | `	uLimit = isNeg ? (sxu64)SXI64_HIGH + 1 : (sxu64)SXI64_HIGH;` |
|      58 |  435 | `	if( nDigit > 19 ){` |
|     ! 0 |  436 | `		return 0;` |
|       - |  437 | `	}` |
|     184 |  438 | `	for( i = 0 ; i < nDigit ; ++i ){` |
|     130 |  439 | `		sxu64 d = (sxu64)(zDigit[i] - '0');` |
|     130 |  440 | `		if( uVal > (uLimit - d)/10 ){` |
|       3 |  441 | `			return 0;` |
|       - |  442 | `		}` |
|     128 |  443 | `		uVal = uVal*10 + d;` |
|      65 |  444 | `	}` |
|      56 |  445 | `	*piVal = isNeg ? (sxi64)(~uVal + 1) : (sxi64)uVal;` |
|      66 |  446 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){` |
|      12 |  447 | `		z++;` |
|       2 |  448 | `	}` |
|      56 |  449 | `	return z == zEnd ? 1 : 2;` |
|      54 |  450 | `}` |
|       - |  451 | `/*` |
|       - |  452 | ` * php's offset rules for a STRING container (zend_check_string_offset, and the` |
|       - |  453 | ` * isset/empty arm of ZEND_ISSET_ISEMPTY_DIM_OBJ). They are NOT the array rules,` |
|       - |  454 | ` * and PHL applied none of them: every offset went through an int cast, so` |
|       - |  455 | `` * `$s[""]`, `$s["-"]` and `$s["p"]` all answered `$s[0]` — a silent wrong answer`` |
|       - |  456 | ` * on code php refuses to run. php's table:` |
|       - |  457 | ` *` |
|       - |  458 | ` *   int                     the offset` |
|       - |  459 | ` *   null / bool / float     Warning: String offset cast occurred, then cast` |
|       - |  460 | ` *   integer-shaped string   the offset (leading/trailing space and '+' allowed)` |
|       - |  461 | ` *   int-then-garbage string Warning: Illegal string offset "12abc", then 12` |
|       - |  462 | ` *   any other string        TypeError: Cannot access offset of type string on string` |
|       - |  463 | ` *   array/object/resource   TypeError, naming the type (an object's CLASS)` |
|       - |  464 | ` *` |
|       - |  465 | ` * isset()/empty() raise NOTHING and answer "not set" for every shape the read` |
|       - |  466 | `` * path would reject OR warn about: `isset($s["0x1"])` is false even though`` |
|       - |  467 | `` * reading it warns and yields `$s[0]`. A `??` fetch sits BETWEEN that and a real`` |
|       - |  468 | ` * read — it suppresses the not-set diagnostics but still warns about the offset` |
|       - |  469 | ` * SHAPE and still reads it. iLevel selects which of the three (VM_STROFF_LOUD /` |
|       - |  470 | ` * _COALESCE / _ISSET).` |
|       - |  471 | ` */` |
|  853321 |  472 | `PH7_PRIVATE int VmStringOffsetResolve(ph7_vm *pVm,ph7_value *pIdx,int iLevel,sxi64 *piOfft,SyBlob *pMsg)` |
|       5 |  473 | `{` |
|  853326 |  474 | `	if( (pIdx->iFlags & MEMOBJ_INT) != 0 && (pIdx->iFlags & MEMOBJ_REAL) == 0 ){` |
|  853160 |  475 | `		*piOfft = pIdx->x.iVal;` |
|  853160 |  476 | `		return VM_STROFF_OK;` |
|       - |  477 | `	}` |
|     170 |  478 | `	if( pIdx->iFlags & MEMOBJ_STRING ){` |
|     154 |  479 | `		int eInt = VmStringOffsetInt((const char *)SyBlobData(&pIdx->sBlob),` |
|      50 |  480 | `			SyBlobLength(&pIdx->sBlob),piOfft);` |
|     104 |  481 | `		if( eInt == 1 ){` |
|      24 |  482 | `			return VM_STROFF_OK;` |
|       - |  483 | `		}` |
|      82 |  484 | `		if( eInt == 2 && iLevel != VM_STROFF_ISSET ){` |
|       - |  485 | ``			/* int-then-garbage. This is the one diagnostic a `??` fetch keeps:`` |
|       - |  486 | ``			 * `$s["1x"] ?? "d"` warns and answers $s[1] (PHL called it "not set"`` |
|       - |  487 | `			 * and answered the default — a wrong VALUE, not just a missing` |
|       - |  488 | `			 * warning). Only isset()/empty() stay silent about it. */` |
|       - |  489 | `			SyString sKey;` |
|      26 |  490 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob));` |
|      26 |  491 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Illegal string offset \"%z\"",&sKey);` |
|      26 |  492 | `			return VM_STROFF_OK;` |
|       - |  493 | `		}` |
|      58 |  494 | `		if( iLevel != VM_STROFF_LOUD ){` |
|      24 |  495 | `			return VM_STROFF_MISS;` |
|       4 |  496 | `		}` |
|      84 |  497 | `	}else if( (pIdx->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|       - |  498 | `		/* null / bool / float: php casts, but says so in a real read or write. */` |
|      46 |  499 | `		if( iLevel == VM_STROFF_LOUD ){` |
|      32 |  500 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"String offset cast occurred");` |
|      15 |  501 | `		}` |
|      46 |  502 | `		PH7_MemObjToInteger(pIdx);` |
|      46 |  503 | `		*piOfft = pIdx->x.iVal;` |
|      46 |  504 | `		return VM_STROFF_OK;` |
|      24 |  505 | `	}else if( iLevel == VM_STROFF_ISSET ){` |
|       - |  506 | `		/* An array/object/resource offset is "not set" for isset()/empty() — but a` |
|       - |  507 | ``		 * `??` fetch RAISES for it (probed: `$s[[]] ?? "d"` is the TypeError while`` |
|       - |  508 | ``		 * `isset($s[[]])` is false), so only the fully-quiet level answers MISS. */`` |
|      10 |  509 | `		return VM_STROFF_MISS;` |
|       - |  510 | `	}` |
|       - |  511 | `	{` |
|       - |  512 | `		char zBuf[128];` |
|      50 |  513 | `		SyBlobInit(pMsg,&pVm->sAllocator);` |
|      73 |  514 | `		SyBlobFormat(pMsg,"Cannot access offset of type %s on string",` |
|      23 |  515 | `			VmValueGivenName(pIdx,zBuf,sizeof(zBuf)));` |
|       - |  516 | `	}` |
|      50 |  517 | `	return VM_STROFF_REJECT;` |
|  427718 |  518 | `}` |
|       - |  519 | `/*` |
|       - |  520 | ` * OP_STORE_IDX_REF: body moved verbatim from the OP_STORE_IDX_REF arm of` |
|       - |  521 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  522 | ` */` |
|  390859 |  523 | `PH7_PRIVATE VmOpRc VmExecOpStoreIdxRef(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  524 | `{` |
|  390864 |  525 | `	ph7_value *pTos = pState->pTos;` |
|  390864 |  526 | `	ph7_value *pStack = pState->pStack;` |
|  390864 |  527 | `	VmInstr *aInstr = pState->aInstr;` |
|  390864 |  528 | `	sxi32 pc = pState->pc;` |
|       - |  529 | `	sxi32 rc;` |
|  195418 |  530 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|  390864 |  531 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|       - |  532 | `	ph7_value *pKey;` |
|       - |  533 | `	sxu32 nIdx;` |
|  390864 |  534 | `	if( pInstr->iP1 ){` |
|       - |  535 | `		/* Key is next on stack */` |
|   80304 |  536 | `		pKey = pTos;` |
|   80304 |  537 | `		pTos--;` |
|   40154 |  538 | `	}else{` |
|  310565 |  539 | `		pKey = 0;` |
|       - |  540 | `	}` |
|       - |  541 | `		/* php DEPRECATES a null / lossy-float write subscript (then normalizes "" /` |
|       - |  542 | ``		 * truncates); it does not reject either. A null key — by-VALUE (`$a[$k]=v`)`` |
|       - |  543 | ``		 * OR by-REF (`$a[$k]=&$x`) — deprecates + coerces to the "" key: the notice is`` |
|       - |  544 | `		 * emitted DOWN AT THE INSERT (below), not here, so a null/false container that` |
|       - |  545 | ``		 * auto-vivifies (`$x=null; $x[null]=v`) gets it too (the base is not yet a`` |
|       - |  546 | `		 * hashmap at this point), and PH7_HashmapInsert / HashmapInsertByRef both cast` |
|       - |  547 | `		 * NULL->"". A lossy-FLOAT subscript stays a loud TypeError in BOTH forms (the` |
|       - |  548 | `		 * recorded non-deprecated-surface policy, §2). */` |
|  390864 |  549 | `		if( pKey && (pTos->iFlags & MEMOBJ_HASHMAP) ){` |
|       - |  550 | `			SyBlob sTypeMsg;` |
|       - |  551 | `			/* An object/array key is php's TypeError; a resource key warns and` |
|       - |  552 | `			 * becomes its integer id. Both used to be stringified silently. */` |
|   79706 |  553 | `			if( VmOffsetTypeRejected(&(*pVm),pKey,0,&sTypeMsg) ){` |
|       - |  554 | `				sxi32 rcSc;` |
|       8 |  555 | `				PH7_MemObjRelease(pKey);` |
|       8 |  556 | `				VmPopOperand(&pTos,1);` |
|       8 |  557 | `				rcSc = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|      12 |  558 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       8 |  559 | `				rc = rcSc;` |
|       8 |  560 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  561 | `			}` |
|   79700 |  562 | `			VmOffsetResourceWarn(&(*pVm),pKey);` |
|   79695 |  563 | `			if( (pKey->iFlags & MEMOBJ_REAL)` |
|   39855 |  564 | `			 && pKey->rVal != (ph7_real)(sxi64)pKey->rVal ){` |
|       - |  565 | `				sxi32 rcSc;` |
|       3 |  566 | `				const char *zErr = "Cannot access offset of type float on array";` |
|       3 |  567 | `				PH7_MemObjRelease(pKey);` |
|       3 |  568 | `				VmPopOperand(&pTos,1);` |
|       3 |  569 | `				rcSc = VmThrowFromVm(&(*pVm),"TypeError",zErr,(sxu32)SyStrlen(zErr));` |
|       3 |  570 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       3 |  571 | `				rc = rcSc;` |
|       3 |  572 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  573 | `			}` |
|   39846 |  574 | `		}` |
|  390856 |  575 | `	nIdx = pTos->nIdx;` |
|       - |  576 | `	{` |
|       - |  577 | `		/* ArrayAccess::offsetSet dispatch.` |
|       - |  578 | `		 * Container may be on the stack as MEMOBJ_OBJ, or referenced via` |
|       - |  579 | `		 * the backing variable slot at nIdx. */` |
|  390856 |  580 | `		ph7_class_instance *pInst = 0;` |
|  390856 |  581 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|     436 |  582 | `			pInst = (ph7_class_instance *)pTos->x.pOther;` |
|  390640 |  583 | `		}else if( nIdx != SXU32_HIGH ){` |
|  390404 |  584 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  390404 |  585 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_OBJ) ){` |
|     ! 0 |  586 | `				pInst = (ph7_class_instance *)pBacking->x.pOther;` |
|     ! 0 |  587 | `			}` |
|  195188 |  588 | `		}` |
|  390856 |  589 | `		if( pInst ){` |
|     436 |  590 | `			ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|     436 |  591 | `			if( pArrayAccess && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|       - |  592 | `				ph7_class_method *pMeth;` |
|       - |  593 | `				ph7_value sNullKey;` |
|       - |  594 | `				ph7_value *apArg[2];` |
|     428 |  595 | `				if( pInstr->iOp == PH7_OP_STORE_IDX_REF ){` |
|     ! 0 |  596 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - |  597 | `						"Cannot assign by reference to overloaded object");` |
|     ! 0 |  598 | `					if( pKey ){ PH7_MemObjRelease(pKey); }` |
|     ! 0 |  599 | `					VmPopOperand(&pTos,2); /* container + value */` |
|     ! 0 |  600 | `					VM_EXIT_BREAK;` |
|       - |  601 | `				}` |
|     428 |  602 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - |  603 | `					"offsetSet",sizeof("offsetSet")-1);` |
|       - |  604 | `				/* Pop container; pTos now points to the value */` |
|     428 |  605 | `				VmPopOperand(&pTos,1);` |
|     428 |  606 | `				if( pKey == 0 ){` |
|      12 |  607 | `					PH7_MemObjInit(&(*pVm),&sNullKey);` |
|      12 |  608 | `					apArg[0] = &sNullKey;` |
|       7 |  609 | `				}else{` |
|     418 |  610 | `					apArg[0] = pKey;` |
|       - |  611 | `				}` |
|     428 |  612 | `				apArg[1] = pTos;` |
|     428 |  613 | `				if( pMeth ){` |
|     428 |  614 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,0,2,apArg);` |
|     212 |  615 | `				}` |
|     428 |  616 | `				if( pKey ){` |
|     418 |  617 | `					PH7_MemObjRelease(pKey);` |
|     211 |  618 | `				}else{` |
|      12 |  619 | `					PH7_MemObjRelease(&sNullKey);` |
|       - |  620 | `				}` |
|       - |  621 | `				/* Pop the value */` |
|     428 |  622 | `				VmPopOperand(&pTos,1);` |
|     428 |  623 | `				VM_EXIT_BREAK;` |
|       - |  624 | `			}` |
|       - |  625 | `			/* Object without ArrayAccess: PHP throws a fatal Error rather` |
|       - |  626 | `			 * than silently coercing the object into a hashmap (which is` |
|       - |  627 | `			 * what the legacy PH7 fall-through would do via MemObjToHashmap` |
|       - |  628 | `			 * a few lines below). Match PHP. */` |
|       - |  629 | `			{` |
|       - |  630 | `				char zMsg[256];` |
|      10 |  631 | `				SyString *pName = &pInst->pClass->sName;` |
|      14 |  632 | `				sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - |  633 | `					"Cannot use object of type %.*s as array",` |
|       8 |  634 | `					(int)pName->nByte,pName->zString);` |
|      10 |  635 | `				rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|      10 |  636 | `				if( pKey ){ PH7_MemObjRelease(pKey); }` |
|      10 |  637 | `				VmPopOperand(&pTos,2); /* container + value */` |
|      10 |  638 | `				if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      10 |  639 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  640 | `			}` |
|       - |  641 | `		}` |
|       - |  642 | `	}` |
|  390424 |  643 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       - |  644 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|       - |  645 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|       - |  646 | `		 * checking true sharing count, then re-add after separation. */` |
|  390220 |  647 | `		if( nIdx != SXU32_HIGH ){` |
|  390200 |  648 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  585309 |  649 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|  390200 |  650 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|       - |  651 | `				/* Only adjust refcount / perform COW if the backing variable` |
|       - |  652 | `				 * is still sharing the same hashmap instance. This mirrors` |
|       - |  653 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|       - |  654 | `				 * refcounts if the backing array was already separated. */` |
|  390200 |  655 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|  390200 |  656 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|  390200 |  657 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|  390200 |  658 | `					pMap->iRef++;  /* Re-add stack ref */` |
|  390200 |  659 | `					pTos->x.pOther = pMap;` |
|  195091 |  660 | `				}else{` |
|       - |  661 | `					/* Backing variable no longer points at pCur: skip COW here` |
|       - |  662 | `					 * and operate on the hashmap currently on the stack. */` |
|     ! 0 |  663 | `					pMap = pCur;` |
|       - |  664 | `				}` |
|  195091 |  665 | `			}else{` |
|     ! 0 |  666 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       - |  667 | `			}` |
|  195091 |  668 | `		}else{` |
|      21 |  669 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       - |  670 | `		}` |
|  390220 |  671 | `		if( pMap->iRef < 2 ){` |
|       - |  672 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|       - |  673 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|       - |  674 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|       - |  675 | `			 * no code checks iRef for COW decisions. */` |
|      19 |  676 | `			pMap->iRef = 2;` |
|       9 |  677 | `		}` |
|  195101 |  678 | `	}else{` |
|       - |  679 | `		ph7_value *pObj;` |
|     209 |  680 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     209 |  681 | `		if( pObj == 0 ){` |
|     ! 0 |  682 | `			if( pKey ){` |
|     ! 0 |  683 | `			  PH7_MemObjRelease(pKey);` |
|     ! 0 |  684 | `			}` |
|     ! 0 |  685 | `			VmPopOperand(&pTos,1);` |
|     ! 0 |  686 | `			VM_EXIT_BREAK;` |
|       - |  687 | `		}` |
|       - |  688 | `		/* Phase#1: Load the array */` |
|     209 |  689 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|     156 |  690 | `			VmPopOperand(&pTos,1);` |
|     156 |  691 | `			if( pKey == 0 ){` |
|       - |  692 | ``				/* `$s[] = 'x'` on a STRING: php raises the catchable Error`` |
|       - |  693 | `				 * "[] operator not supported for strings" and leaves the string` |
|       - |  694 | `				 * untouched. PHL silently APPENDED, so code that meant to build` |
|       - |  695 | `				 * an array from a variable holding a string quietly produced a` |
|       - |  696 | `				 * longer string instead of failing — a wrong answer, not a` |
|       - |  697 | `				 * missing diagnostic. */` |
|       - |  698 | `				SyBlob sErrMsg;` |
|       6 |  699 | `				SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       6 |  700 | `				SyBlobAppend(&sErrMsg,"[] operator not supported for strings",` |
|       - |  701 | `					sizeof("[] operator not supported for strings")-1);` |
|       6 |  702 | `				VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       6 |  703 | `				VM_EXIT_BREAK;` |
|     ! 0 |  704 | `			}else{` |
|     152 |  705 | `				sxi64 iOfft = 0;` |
|       - |  706 | `				SyBlob sTypeMsg;` |
|       - |  707 | `				/* php's offset rules run BEFORE the RHS is looked at: an offset it` |
|       - |  708 | `				 * refuses is the TypeError alone. The RHS cast below used to happen` |
|       - |  709 | ``				 * first, so every rejected shape — and `$s[] = [1,2]` above — came`` |
|       - |  710 | ``				 * with a spurious `Array to string conversion` in front of it. */`` |
|     152 |  711 | `				if( VmStringOffsetResolve(&(*pVm),pKey,VM_STROFF_LOUD,&iOfft,&sTypeMsg) == VM_STROFF_REJECT ){` |
|       - |  712 | `					sxi32 rcSc;` |
|       7 |  713 | `					PH7_MemObjRelease(pKey);` |
|       7 |  714 | `					rcSc = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|      16 |  715 | `					if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       7 |  716 | `					rc = rcSc;` |
|       7 |  717 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  718 | `				}` |
|       - |  719 | `				/* Force a string cast on the RHS (user-visible: an array warns` |
|       - |  720 | `				 * "Array to string conversion" before the offset write, §2, and a` |
|       - |  721 | `				 * not-stringable object throws — the target string is untouched) */` |
|       - |  722 | `				{` |
|     146 |  723 | `					sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|     146 |  724 | `					if( rcSv != SXRET_OK ){` |
|       5 |  725 | `						PH7_MemObjRelease(pKey);` |
|       7 |  726 | `						PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|     ! 0 |  727 | `					}` |
|       - |  728 | `				}` |
|     142 |  729 | `				if( VmStringOffsetWrite(&(*pVm),pObj,iOfft,pTos) != SXRET_OK ){` |
|       - |  730 | `					sxi32 rcEm;` |
|       9 |  731 | `					PH7_MemObjRelease(pKey);` |
|       9 |  732 | `					rcEm = VmThrowFromVm(&(*pVm),"Error",` |
|       - |  733 | `						"Cannot assign an empty string to a string offset",` |
|       - |  734 | `						sizeof("Cannot assign an empty string to a string offset")-1);` |
|       9 |  735 | `					if( rcEm == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       9 |  736 | `					rc = rcEm;` |
|      11 |  737 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  738 | `				}` |
|       - |  739 | `			}` |
|     134 |  740 | `			if( pKey ){` |
|     134 |  741 | `			  PH7_MemObjRelease(pKey);` |
|      65 |  742 | `			}` |
|     134 |  743 | `			VM_EXIT_BREAK;` |
|      56 |  744 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       - |  745 | `			/* php: only NULL and FALSE auto-vivify into an array. Writing an index into an` |
|       - |  746 | `			 * int/float/resource/TRUE is a catchable Error -- PH7 quietly REPLACED the value` |
|       - |  747 | `			 * with an array, destroying it ($x = 5; $x[0] = 1; left $x === [1]). */` |
|       - |  748 | `			/* php auto-vivifies false into an array with an 8.1 DEPRECATION; PHL` |
|       - |  749 | `			 * rejects any scalar base, false included (null still auto-vivifies). */` |
|      56 |  750 | `			int bScalar = (pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_RES\|MEMOBJ_BOOL)) != 0;` |
|      56 |  751 | `			if( bScalar ){` |
|       - |  752 | `				sxi32 rcSc;` |
|       8 |  753 | `				if( pKey ){` |
|       5 |  754 | `					PH7_MemObjRelease(pKey);` |
|       2 |  755 | `				}` |
|       8 |  756 | `				VmPopOperand(&pTos,1);` |
|       8 |  757 | `				rcSc = VmThrowFromVm(&(*pVm),"Error","Cannot use a scalar value as an array",` |
|       - |  758 | `					sizeof("Cannot use a scalar value as an array")-1);` |
|       8 |  759 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       8 |  760 | `				rc = rcSc;` |
|       8 |  761 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  762 | `			}` |
|       - |  763 | `			/* Force a hashmap cast  */` |
|      50 |  764 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      50 |  765 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  766 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|     ! 0 |  767 | `				VM_EXIT_ABORT;` |
|       - |  768 | `			}` |
|      23 |  769 | `		}` |
|       - |  770 | `		/* COW separate the backing variable before mutation */` |
|      50 |  771 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|       - |  772 | `	}` |
|  390266 |  773 | `	VmPopOperand(&pTos,1);` |
|       - |  774 | `	/* Phase#2: Perform the insertion. A null key deprecates + normalizes to "" for` |
|       - |  775 | `	 * BOTH the by-value and the by-ref store — emitted HERE, after a null/false` |
|       - |  776 | ``	 * container has auto-vivified to an array, so `$x[null]=v` and `$x[null]=&$y` on`` |
|       - |  777 | `	 * an undefined $x get the notice too (the top-of-handler check runs before` |
|       - |  778 | `	 * vivification, when the base is not yet a hashmap). HashmapInsert /` |
|       - |  779 | `	 * HashmapInsertByRef both then cast NULL->"". A null pKey==0 (an append, no key)` |
|       - |  780 | `	 * is not a null OFFSET and is left alone. */` |
|  390266 |  781 | `	VmNullOffsetDeprecate(&(*pVm),pKey);` |
|  390266 |  782 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && (pTos->iFlags & MEMOBJ_AUX_STROFFSET) ){` |
|       - |  783 | ``		/* `$a[] = &$s[1]`: the source is a string OFFSET, which php refuses to`` |
|       - |  784 | `		 * reference — and whose slot index is the BASE STRING's, so binding it` |
|       - |  785 | ``		 * aliased the whole string. Same Error the `=&` / by-ref-argument paths`` |
|       - |  786 | `		 * raise. The key is already popped; drop it and abandon the store. */` |
|       - |  787 | `		sxi32 rcSc;` |
|       5 |  788 | `		if( pKey ){` |
|       3 |  789 | `			PH7_MemObjRelease(pKey);` |
|       1 |  790 | `		}` |
|       5 |  791 | `		rcSc = VmThrowFromVm(&(*pVm),"Error","Cannot create references to/from string offsets",` |
|       - |  792 | `			sizeof("Cannot create references to/from string offsets")-1);` |
|       5 |  793 | `		if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       5 |  794 | `		rc = rcSc;` |
|       5 |  795 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  796 | `	}` |
|  390262 |  797 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|      54 |  798 | `		if( pMap == pVm->pGlobal ){` |
|       - |  799 | `			/* php 8.1: $GLOBALS['y'] =& $x binds the global $y to $x's` |
|       - |  800 | `			 * slot; an append has no name to bind (catchable Error). */` |
|       5 |  801 | `			if( pKey == 0 ){` |
|     ! 0 |  802 | `				rc = PH7_VmThrowGlobalsAppendError(&(*pVm));` |
|     ! 0 |  803 | `			}else{` |
|       5 |  804 | `				if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 |  805 | `					PH7_MemObjToString(pKey);` |
|     ! 0 |  806 | `				}` |
|       5 |  807 | `				if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|       - |  808 | `					/* Pathological empty name: keep the legacy diagnostic */` |
|     ! 0 |  809 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|       - |  810 | `						"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 |  811 | `					rc = SXRET_OK;` |
|     ! 0 |  812 | `				}else{` |
|       7 |  813 | `					rc = PH7_VmInstallGlobalVar(&(*pVm),` |
|       4 |  814 | `						(const char *)SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),` |
|       4 |  815 | `						0,pTos->nIdx);` |
|       - |  816 | `				}` |
|       - |  817 | `			}` |
|       3 |  818 | `		}else{` |
|       - |  819 | `			/* Insertion by reference */` |
|      50 |  820 | `			rc = PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|       - |  821 | `		}` |
|      28 |  822 | `	}else{` |
|  390210 |  823 | `		rc = PH7_HashmapInsert(pMap,pKey,pTos);` |
|       - |  824 | `	}` |
|  390262 |  825 | `	if( pKey ){` |
|   79722 |  826 | `		PH7_MemObjRelease(pKey);` |
|   39858 |  827 | `	}` |
|       - |  828 | `	/* An append onto the occupied saturated auto-index threw php's catchable` |
|       - |  829 | `	 * Error (PH7_VmThrowArrayNextIndexError) — dispatch it like any other` |
|       - |  830 | `	 * store-path throw. Plain failures (OOM) keep their existing routes. */` |
|  390264 |  831 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|  390258 |  832 | `	VM_EXIT_BREAK;` |
|     ! 0 |  833 | `	VM_EXIT_BREAK;` |
|  195423 |  834 | `}` |
|       - |  835 |  |
|       - |  836 | `/*` |
|       - |  837 | ` * OP_LOAD_CLOSURE: body moved verbatim from the OP_LOAD_CLOSURE arm of` |
|       - |  838 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  839 | ` */` |
|   10054 |  840 | `PH7_PRIVATE VmOpRc VmExecOpLoadClosure(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  841 | `{` |
|   10059 |  842 | `	ph7_value *pTos = pState->pTos;` |
|   10059 |  843 | `	ph7_value *pStack = pState->pStack;` |
|   10059 |  844 | `	VmInstr *aInstr = pState->aInstr;` |
|   10059 |  845 | `	sxi32 pc = pState->pc;` |
|       - |  846 | `	sxi32 rc;` |
|    5027 |  847 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|   10059 |  848 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|       - |  849 | `	/* The function whose name the Closure object will wrap: a fresh per-instantiation` |
|       - |  850 | `	 * copy for a real closure (built below), or the shared lambda function itself for a` |
|       - |  851 | `	 * plain anonymous function with no captured environment. */` |
|   10059 |  852 | `	ph7_vm_func *pTarget = pFunc;` |
|       - |  853 | `	/* A no-capture lambda declared inside a class method is not VM_FUNC_CLOSURE` |
|       - |  854 | `	 * (its env is empty), yet php still binds the creation-site class as its scope` |
|       - |  855 | ``	 * so `self::`/private access inside the body works. Detect the enclosing class`` |
|       - |  856 | `	 * here and route such a lambda through the per-instance closure path (a global` |
|       - |  857 | `	 * no-capture lambda peeks NULL and stays a shared function). */` |
|   10059 |  858 | `	ph7_class *pLoadScope = (pFunc->iFlags & VM_FUNC_CLOSURE) ? 0 : PH7_VmPeekDeclaringClass(pVm);` |
|   10059 |  859 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) \|\| pLoadScope ){` |
|       - |  860 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|       - |  861 | `		ph7_vm_func *pClosure;` |
|       - |  862 | `		char *zName;` |
|       - |  863 | `		sxu32 mLen;` |
|       - |  864 | `		sxu32 n;` |
|       - |  865 | `		/* Create a new VM function */` |
|    9879 |  866 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|       - |  867 | `		/* Generate an unique closure name */` |
|    9879 |  868 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|    9879 |  869 | `		if( pClosure == 0 \|\| zName == 0){` |
|     ! 0 |  870 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|     ! 0 |  871 | `			VM_EXIT_ABORT;` |
|       - |  872 | `		}` |
|    9879 |  873 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|    9879 |  874 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|     ! 0 |  875 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|     ! 0 |  876 | `		}` |
|       - |  877 | `		/* Zero the stucture */` |
|    9879 |  878 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|       - |  879 | `		/* Perform a structure assignment on read-only items */` |
|    9879 |  880 | `		pClosure->aArgs = pFunc->aArgs;` |
|    9879 |  881 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|    9879 |  882 | `		pClosure->aStatic = pFunc->aStatic;` |
|    9879 |  883 | `		pClosure->iFlags = pFunc->iFlags;` |
|       - |  884 | `		/* An in-class no-capture lambda routed here (pLoadScope set) must be a` |
|       - |  885 | `		 * real closure so PH7_VmClassMemberAccess honors its pUserData scope. */` |
|    9879 |  886 | `		pClosure->iFlags \|= VM_FUNC_CLOSURE;` |
|    9879 |  887 | `		pClosure->pUserData = pFunc->pUserData;` |
|    9879 |  888 | `		pClosure->sSignature = pFunc->sSignature;` |
|    9879 |  889 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|    9879 |  890 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|    9879 |  891 | `		pClosure->aReturnUnion = pFunc->aReturnUnion;` |
|    9879 |  892 | `		pClosure->sReturnTypeName = pFunc->sReturnTypeName;` |
|    9879 |  893 | `		pClosure->bStrictTypes = pFunc->bStrictTypes;` |
|    9879 |  894 | `		pClosure->nMaxStack = pFunc->nMaxStack;` |
|    9879 |  895 | `		if( pClosure->pUserData == 0 ){` |
|       - |  896 | `			/* Stamp the creation-site class scope (see PH7_VmPeekDeclaringClass):` |
|       - |  897 | `			 * a closure made in a method — or in another closure, whose own stamp` |
|       - |  898 | `			 * the peek reads — resolves self::/parent:: against it like php. */` |
|    9879 |  899 | `			pClosure->pUserData = (void *)PH7_VmPeekDeclaringClass(pVm);` |
|    4937 |  900 | `		}` |
|       - |  901 | ``		/* Capture the creation-site late-static-binding class so `static::` inside`` |
|       - |  902 | `		 * the closure body resolves like php (the "called class", which may differ` |
|       - |  903 | `		 * from the declaring scope stamped above — e.g. a closure made in an` |
|       - |  904 | `		 * inherited method). A closure made outside any class captures NULL. */` |
|    9879 |  905 | `		pClosure->pLsbClass = (void *)PH7_VmPeekTopClass(pVm);` |
|       - |  906 | `		/* Reflection descriptor fields (getDocComment/getAttributes/getStartLine/` |
|       - |  907 | `		 * getEndLine/getFileName read the INSTANTIATED copy via $__fn) */` |
|    9879 |  908 | `		pClosure->aAttrs = pFunc->aAttrs;` |
|    9879 |  909 | `		pClosure->sDoc = pFunc->sDoc;` |
|    9879 |  910 | `		pClosure->sFile = pFunc->sFile;` |
|    9879 |  911 | `		pClosure->nLine = pFunc->nLine;` |
|    9879 |  912 | `		pClosure->nEndLine = pFunc->nEndLine;` |
|       - |  913 | ``		/* php's visible `{closure:...}` name is a property of the DECLARATION, so every`` |
|       - |  914 | `		 * per-instantiation copy answers the same one (php has a single op_array here). */` |
|    9879 |  915 | `		pClosure->sClosureName = pFunc->sClosureName;` |
|    9879 |  916 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|       - |  917 | `		/* Register the closure */` |
|    9879 |  918 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|       - |  919 | `		/* Set up closure environment */` |
|    9879 |  920 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|    9879 |  921 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|   23355 |  922 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|       - |  923 | `			ph7_value *pValue;` |
|   13481 |  924 | `			pEnv = &aEnv[n];` |
|   13481 |  925 | `			sEnv.sName  = pEnv->sName;` |
|   13481 |  926 | `			sEnv.iFlags = pEnv->iFlags;` |
|   13481 |  927 | `			sEnv.nLine = pEnv->nLine;` |
|   13481 |  928 | `			sEnv.nIdx = SXU32_HIGH;` |
|   13481 |  929 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|   13476 |  930 | `			if( (sEnv.iFlags & (VM_FUNC_ARG_BY_REF\|VM_FUNC_ARG_IGNORE)) == VM_FUNC_ARG_BY_REF` |
|    7113 |  931 | `			 && !(SyStringLength(&sEnv.sName) == sizeof("this")-1` |
|     370 |  932 | `				&& SyMemcmp(SyStringData(&sEnv.sName),"this",sizeof("this")-1) == 0) ){` |
|       - |  933 | `				/* Capture by reference: bind the env entry to the variable's` |
|       - |  934 | `				 * memory slot — creating a fresh null variable when missing,` |
|       - |  935 | ``				 * as php does (`use (&$f)` before $f is assigned) — and pin`` |
|       - |  936 | `				 * the slot past the creating frame's teardown so the closure` |
|       - |  937 | `				 * can outlive its birth scope. The call-time env install` |
|       - |  938 | `				 * aliases the name to this slot instead of copying a value. */` |
|     505 |  939 | `				pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,TRUE);` |
|     505 |  940 | `				if( pValue ){` |
|     505 |  941 | `					sEnv.nIdx = pValue->nIdx;` |
|     505 |  942 | `					VmPinMemObjSlot(pVm,pValue->nIdx);` |
|     250 |  943 | `				}` |
|     255 |  944 | `			}else{` |
|       - |  945 | `				/* Standard pass by value */` |
|   12981 |  946 | `				pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|   12981 |  947 | `				if( pValue ){` |
|       - |  948 | `					/* Copy imported value */` |
|    3387 |  949 | `					PH7_MemObjStore(pValue,&sEnv.sValue);` |
|   11290 |  950 | `				}else if( (sEnv.iFlags & (VM_FUNC_ARG_BY_REF\|VM_FUNC_ARG_IGNORE)) == 0` |
|    4825 |  951 | `					&& !(SyStringLength(&sEnv.sName) == sizeof("this")-1` |
|      23 |  952 | `						&& SyMemcmp(SyStringData(&sEnv.sName),"this",sizeof("this")-1) == 0) ){` |
|      47 |  953 | `					if( pFunc->iFlags & VM_FUNC_ARROW ){` |
|       - |  954 | `						/* An arrow function auto-captures free variables by value, but` |
|       - |  955 | `						 * php does NOT capture one that is UNDEFINED at creation: the` |
|       - |  956 | `						 * isolated body scope then simply has no such variable, so a` |
|       - |  957 | `						 * read of it there raises the normal "Undefined variable"` |
|       - |  958 | `						 * warning (and reflection's getClosureUsedVariables omits it).` |
|       - |  959 | `						 * Skip installing the capture so the body READ — not the` |
|       - |  960 | `						 * creation — warns, matching php. (A later assignment to the` |
|       - |  961 | `						 * outer variable does not retro-capture: arrow scope is` |
|       - |  962 | `						 * isolated.) An explicit by-value use() instead warns here and` |
|       - |  963 | `						 * binds NULL, handled just below. */` |
|      37 |  964 | `						continue;` |
|       - |  965 | `					}` |
|       - |  966 | ``					/* php reads a by-value `use ($q)` capture AT CLOSURE CREATION and`` |
|       - |  967 | `					 * warns when the variable is undefined there (the by-ref form` |
|       - |  968 | ``					 * `use (&$q)` above stays silent — it creates the binding). The`` |
|       - |  969 | `					 * auto-injected $this (VM_FUNC_ARG_IGNORE) never warns. The` |
|       - |  970 | `					 * capture still proceeds as NULL, as php does. php attributes the` |
|       - |  971 | `					 * warning to the capture's own line (which can differ from the` |
|       - |  972 | `					 * OP_LOAD_CLOSURE instruction line when the use-clause wraps), so` |
|       - |  973 | `					 * borrow the recorded line for the emission and restore it. */` |
|      11 |  974 | `					sxu32 nSavedLine = pVm->nCurLine;` |
|      11 |  975 | `					if( sEnv.nLine ){` |
|      11 |  976 | `						pVm->nCurLine = sEnv.nLine;` |
|       5 |  977 | `					}` |
|      11 |  978 | `					VmErrorFormat(pVm,PH7_CTX_WARNING,"Undefined variable $%z",&sEnv.sName);` |
|      11 |  979 | `					pVm->nCurLine = nSavedLine;` |
|       5 |  980 | `				}` |
|       - |  981 | `			}` |
|       - |  982 | `			/* Insert the imported variable */` |
|   13449 |  983 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|    6727 |  984 | `		}` |
|    9879 |  985 | `		pTarget = pClosure;` |
|    4937 |  986 | `	}` |
|       - |  987 | `	/* Wrap the target function in a Closure object and push it. Its captured environment` |
|       - |  988 | ``	 * (incl. any `$this`) stays in pTarget->aClosureEnv and is delivered by the normal call`` |
|       - |  989 | `	 * path when the closure is dispatched by name. */` |
|   10059 |  990 | `	pTos++;` |
|       - |  991 | `	{` |
|   10059 |  992 | `		ph7_class_instance *pCloObj = VmCreateClosure(pVm, &pTarget->sName, 0, 0);` |
|   10059 |  993 | `		if( pCloObj ){` |
|   10059 |  994 | `			pCloObj->iRef++;` |
|   10059 |  995 | `			pTos->x.pOther = pCloObj;` |
|   10059 |  996 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|    5032 |  997 | `		}else{` |
|       - |  998 | `			/* OOM fallback: the name string is still a usable callable. */` |
|     ! 0 |  999 | `			PH7_MemObjStringAppend(pTos, pTarget->sName.zString, pTarget->sName.nByte);` |
|       - | 1000 | `		}` |
|       - | 1001 | `	}` |
|   10059 | 1002 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1003 | `	VM_EXIT_BREAK;` |
|    5032 | 1004 | `}` |
|       - | 1005 |  |
|       - | 1006 |  |
|       - | 1007 | `/*` |
|       - | 1008 | `` * TRUE when this LOAD_IDX feeds a `??`: the coalesce test is the very next`` |
|       - | 1009 | `` * instruction. php evaluates the whole left operand of `??` in isset-context,`` |
|       - | 1010 | ` * so the access must stay SILENT and, when it misses, must yield NULL — an` |
|       - | 1011 | ``  * out-of-range string offset that yielded "" instead made `$s[99] ?? $d` `` |
|       - | 1012 | ` * evaluate to "" rather than $d, a wrong answer rather than a stray notice.` |
|       - | 1013 | ` */` |
|  853123 | 1014 | `static int VmIdxFeedsCoalesce(const VmInstr *pInstr)` |
|       5 | 1015 | `{` |
|  853128 | 1016 | `	return (pInstr+1)->iOp == PH7_OP_NULLC \|\| (pInstr+1)->iOp == PH7_OP_NULLC_JMP;` |
|       5 | 1017 | `}` |
|       - | 1018 | `/*` |
|       - | 1019 | `` * php's string-offset STORE, shared by `$s[i] = v` (OP_STORE_IDX) and`` |
|       - | 1020 | `` * `$s[i] ??= v` (OP_NULLC_STORE — `??=` is not an assign-op, so php performs a`` |
|       - | 1021 | ` * real offset write there): resolve iRawOfft against the string's CURRENT length` |
|       - | 1022 | ` * (a negative offset counts back from the end and, when it still lands before the` |
|       - | 1023 | `` * start, warns `Illegal string offset` and writes NOTHING), refuse an EMPTY`` |
|       - | 1024 | ` * replacement, warn when more than one byte was handed over, PAD WITH SPACES up` |
|       - | 1025 | ` * to the offset, then write the first byte.` |
|       - | 1026 | ` *` |
|       - | 1027 | ` * pVal must ALREADY be a string: that coercion is user-visible (it warns for an` |
|       - | 1028 | ` * array, throws for a not-stringable object) and stays with the callers, which` |
|       - | 1029 | ` * are the only places that can route a throw. Answers SXRET_OK when the store` |
|       - | 1030 | `` * happened or was skipped, and SXERR_INVALID for php's `Cannot assign an empty`` |
|       - | 1031 | `` * string to a string offset` Error — raised by the caller for the same reason.`` |
|       - | 1032 | ` */` |
|     178 | 1033 | `PH7_PRIVATE sxi32 VmStringOffsetWrite(ph7_vm *pVm,ph7_value *pStr,sxi64 iRawOfft,ph7_value *pVal)` |
|       4 | 1034 | `{` |
|     182 | 1035 | `	sxi64 nLen = (sxi64)SyBlobLength(&pStr->sBlob);` |
|     182 | 1036 | `	sxi64 iOfft = iRawOfft;` |
|       - | 1037 | `	const char *zVal;` |
|     182 | 1038 | `	if( iOfft < 0 ){` |
|       - | 1039 | `		/* php 7.1: a negative offset writes back from the end. */` |
|       9 | 1040 | `		iOfft += nLen;` |
|       9 | 1041 | `		if( iOfft < 0 ){` |
|       7 | 1042 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Illegal string offset %qd",iRawOfft);` |
|       7 | 1043 | `			return SXRET_OK;` |
|       - | 1044 | `		}` |
|       1 | 1045 | `	}` |
|     176 | 1046 | `	if( SyBlobLength(&pVal->sBlob) < 1 ){` |
|       - | 1047 | ``		/* php refuses to write NOTHING into an offset — `$s[2] = ""`, and the`` |
|       - | 1048 | ``		 * `= null` / `= false` that stringify to "" — where PHL silently ignored`` |
|       - | 1049 | `		 * the store. */` |
|      13 | 1050 | `		return SXERR_INVALID;` |
|       - | 1051 | `	}` |
|     164 | 1052 | `	zVal = (const char *)SyBlobData(&pVal->sBlob);` |
|     164 | 1053 | `	if( SyBlobLength(&pVal->sBlob) > 1 ){` |
|      10 | 1054 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 1055 | `			"Only the first byte will be assigned to the string offset");` |
|       4 | 1056 | `	}` |
|     164 | 1057 | `	if( iOfft >= nLen ){` |
|       - | 1058 | `		/* php PADS WITH SPACES up to the offset. PH7 simply appended the byte, so` |
|       - | 1059 | `		 * "abc" with [6]="Z" became "abcZ" rather than "abc   Z" -- a silently` |
|       - | 1060 | `		 * wrong string. */` |
|       - | 1061 | `		sxi64 nPad;` |
|     209 | 1062 | `		for( nPad = nLen ; nPad < iOfft ; ++nPad ){` |
|     169 | 1063 | `			SyBlobAppend(&pStr->sBlob," ",sizeof(char));` |
|      85 | 1064 | `		}` |
|      41 | 1065 | `		SyBlobAppend(&pStr->sBlob,(const void *)zVal,sizeof(char));` |
|      21 | 1066 | `	}else{` |
|     124 | 1067 | `		char *zData = (char *)SyBlobData(&pStr->sBlob);` |
|     124 | 1068 | `		zData[iOfft] = zVal[0];` |
|       - | 1069 | `	}` |
|     164 | 1070 | `	return SXRET_OK;` |
|      93 | 1071 | `}` |
|       - | 1072 | `/*` |
|       - | 1073 | `` * Does this LOAD_IDX feed a `??=` (rather than a plain `??`)? The compiler emits`` |
|       - | 1074 | ` * the LHS peek, then OP_NULLC_JMP, then the RHS, then OP_NULLC_STORE — so the` |
|       - | 1075 | ` * NULLC_JMP right after is what distinguishes the assigning form, whose store` |
|       - | 1076 | ` * still has to happen when the peek answers null.` |
|       - | 1077 | ` */` |
|  853123 | 1078 | `static int VmIdxFeedsCoalesceAssign(const VmInstr *pInstr)` |
|       5 | 1079 | `{` |
|  853128 | 1080 | `	return (pInstr+1)->iOp == PH7_OP_NULLC_JMP;` |
|       5 | 1081 | `}` |
|       - | 1082 | `/*` |
|       - | 1083 | `` * Arm the write-back half of `$o[$k] op= v` on an ArrayAccess container. The`` |
|       - | 1084 | ` * value offsetGet just answered goes into a fresh SCRATCH memobj that pTos` |
|       - | 1085 | ` * points at, so the compound-assign op computes IN that slot the way it would` |
|       - | 1086 | ` * in an ordinary variable; the pending entry then makes the op's tail dispatch` |
|       - | 1087 | `` * `offsetSet($k, computed)`. The KEY gets a reserved slot of its own (pIdx is`` |
|       - | 1088 | ` * released as soon as this opcode returns, and the write happens one opcode` |
|       - | 1089 | `` * later); an ABSENT key — `$o[] op= v` — is carried as NULL, which is the value`` |
|       - | 1090 | ` * php hands both accessors for that shape.` |
|       - | 1091 | ` *` |
|       - | 1092 | ` * A failed reservation simply leaves the value unarmed: pTos keeps its` |
|       - | 1093 | ` * no-slot temp and the op falls back to the pre-existing refusal.` |
|       - | 1094 | ` */` |
|      50 | 1095 | `static void VmDimRmwArm(` |
|       - | 1096 | `	ph7_vm *pVm,` |
|       - | 1097 | `	ph7_class_instance *pInst,` |
|       - | 1098 | `	ph7_value *pIdx,` |
|       - | 1099 | `	ph7_value *pTos,` |
|       - | 1100 | `	void *pOwnerStack,` |
|       - | 1101 | `	void *pInstrs,` |
|       - | 1102 | `	sxu32 nPc` |
|       - | 1103 | `	)` |
|       1 | 1104 | `{` |
|       - | 1105 | `	ph7_value *pSlot;` |
|       - | 1106 | `	sxu32 nScratch;` |
|       - | 1107 | `	sxu32 nKey;` |
|       - | 1108 | `	VmHookRmw sRmw;` |
|      51 | 1109 | `	pSlot = PH7_ReserveMemObj(&(*pVm));` |
|      51 | 1110 | `	if( pSlot == 0 ){` |
|     ! 0 | 1111 | `		return;` |
|       - | 1112 | `	}` |
|      51 | 1113 | `	nScratch = pSlot->nIdx;` |
|      51 | 1114 | `	pSlot = PH7_ReserveMemObj(&(*pVm));` |
|      51 | 1115 | `	if( pSlot == 0 ){` |
|     ! 0 | 1116 | `		VmHookRmwFreeScratch(&(*pVm),nScratch);` |
|     ! 0 | 1117 | `		return;` |
|       - | 1118 | `	}` |
|      51 | 1119 | `	nKey = pSlot->nIdx;` |
|       - | 1120 | `	/* Reserving can GROW the aMemObj set, so address both slots by index from` |
|       - | 1121 | `	 * here on — the pointer the first reservation handed back may be stale. */` |
|      51 | 1122 | `	if( pIdx ){` |
|      51 | 1123 | `		PH7_MemObjStore(pIdx,pSlot);` |
|      25 | 1124 | `	}` |
|      51 | 1125 | `	pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,nScratch);` |
|      51 | 1126 | `	if( pSlot == 0 ){` |
|     ! 0 | 1127 | `		VmHookRmwFreeScratch(&(*pVm),nKey);` |
|     ! 0 | 1128 | `		VmHookRmwFreeScratch(&(*pVm),nScratch);` |
|     ! 0 | 1129 | `		return;` |
|       - | 1130 | `	}` |
|      51 | 1131 | `	PH7_MemObjStore(pTos,pSlot);` |
|      51 | 1132 | `	sRmw.iKind = VM_HOOK_PEND_RMW_DIM;` |
|      51 | 1133 | `	sRmw.pThis = pInst;` |
|      51 | 1134 | `	sRmw.pAttr = 0;` |
|      51 | 1135 | `	sRmw.nBackIdx = nKey;` |
|      51 | 1136 | `	sRmw.nScratchIdx = nScratch;` |
|      51 | 1137 | `	SyBlobInit(&sRmw.sName,&pVm->sAllocator);` |
|      51 | 1138 | `	sRmw.pOwnerStack = pOwnerStack;` |
|      51 | 1139 | `	sRmw.pInstrs = pInstrs;` |
|      51 | 1140 | `	sRmw.nJmpPc = nPc;  /* the modify op ... */` |
|      51 | 1141 | `	sRmw.nPc = nPc;     /* ... is the whole window */` |
|      51 | 1142 | `	pInst->iRef++;` |
|      51 | 1143 | `	pTos->nIdx = nScratch;` |
|      51 | 1144 | `	SySetPut(&pVm->aHookRmw,(const void *)&sRmw);` |
|      26 | 1145 | `}` |
|       - | 1146 | `/*` |
|       - | 1147 | ` * Is this LOAD_IDX fetching the dimension for WRITING — php's BP_VAR_W / BP_VAR_RW` |
|       - | 1148 | ` * / BP_VAR_UNSET fetch, the one that asks the container for a slot to MODIFY` |
|       - | 1149 | ` * rather than a value to read?` |
|       - | 1150 | ` *` |
|       - | 1151 | ` * iP2 answers for most of it. The two shapes it cannot are the ones where the` |
|       - | 1152 | ` * fetch is compiled as a plain read and the NEXT instruction is what makes it a` |
|       - | 1153 | `` * write: binding a reference to the element (`$r = &$o[$k]`) and iterating it by`` |
|       - | 1154 | `` * reference (`foreach ($o[$k] as &$v)`). A compound assign is the reverse case —`` |
|       - | 1155 | ` * iP2 says write, but php compiles it to ASSIGN_DIM_OP, a read plus a write` |
|       - | 1156 | ` * through the container's own handlers (VmDimRmwArm), not a write FETCH.` |
|       - | 1157 | ` */` |
|     788 | 1158 | `static int VmIdxFetchForWrite(const VmInstr *pInstr,sxi32 iP2)` |
|       5 | 1159 | `{` |
|     793 | 1160 | `	const VmInstr *pNext = pInstr + 1;` |
|     793 | 1161 | `	if( iP2 == 1 ){` |
|     165 | 1162 | `		return !VmNextIsCompoundAssign(pNext);` |
|       - | 1163 | `	}` |
|     629 | 1164 | `	if( iP2 == VM_IDX_CTX_UNSET_BASE ){` |
|       - | 1165 | `		/* An INTERMEDIATE subscript of an unset chain: php fetches it for` |
|       - | 1166 | `		 * writing so the removal one level down can land. */` |
|       9 | 1167 | `		return 1;` |
|       - | 1168 | `	}` |
|     621 | 1169 | `	if( iP2 == 0 ){` |
|     451 | 1170 | `		if( pNext->iOp == PH7_OP_STORE_REF ){` |
|       9 | 1171 | `			return 1;` |
|       - | 1172 | `		}` |
|     443 | 1173 | `		if( pNext->iOp == PH7_OP_FOREACH_INIT && pNext->p3 ){` |
|       5 | 1174 | `			return (((ph7_foreach_info *)pNext->p3)->iFlags & PH7_4EACH_STEP_REF) != 0;` |
|       - | 1175 | `		}` |
|     217 | 1176 | `	}` |
|     609 | 1177 | `	return 0;` |
|     399 | 1178 | `}` |
|       - | 1179 | `/*` |
|       - | 1180 | ` * Which fetch contexts may answer out of a WRITABLE container's own storage` |
|       - | 1181 | `` * (PH7_SplDimElemSlot, vm_builtin_spl.c) instead of through `offsetGet`?`` |
|       - | 1182 | ` *` |
|       - | 1183 | ` * The ones that ask for a VALUE and may go on to MODIFY it: a plain read — whose` |
|       - | 1184 | ` * result carries the element's slot exactly as an array element's does, which is` |
|       - | 1185 | ` * what lets a by-reference ARGUMENT bind it — a write-context fetch, and the` |
|       - | 1186 | `` * INTERMEDIATE step of an unset chain. isset()/empty()/`??`/`??=` must reach`` |
|       - | 1187 | ` * offsetExists, and the OUTERMOST unset must reach offsetUnset, so those keep the` |
|       - | 1188 | ` * accessor. (iP2 is the NORMALIZED context here: the deferred-argument record mode` |
|       - | 1189 | ` * has already become a plain read.)` |
|       - | 1190 | ` *` |
|       - | 1191 | ` * A COMPOUND assign is deliberately not one of them, which is why this asks` |
|       - | 1192 | `` * VmIdxFetchForWrite rather than testing iP2 == 1 itself: `$ao[k] op= v` is php's`` |
|       - | 1193 | ` * ASSIGN_DIM_OP on an OBJECT, and that one reads and writes through the accessors` |
|       - | 1194 | ` * whatever the read handler could have offered — a subclass overriding only` |
|       - | 1195 | `` * offsetSet sees its own method called for `+=` and not for `++`.`` |
|       - | 1196 | ` */` |
|     480 | 1197 | `static int VmDimFastFetchCtx(const VmInstr *pInstr,sxi32 iP2)` |
|       5 | 1198 | `{` |
|     485 | 1199 | `	return iP2 == 0 \|\| VmIdxFetchForWrite(pInstr,iP2);` |
|       5 | 1200 | `}` |
|       - | 1201 | `/*` |
|       - | 1202 | `` * php's `Indirect modification of overloaded element of C has no effect`: the`` |
|       - | 1203 | ` * write-context fetch above landed on a container that answers with a COPY, so` |
|       - | 1204 | ` * whatever the rest of the expression writes is thrown away. php says so and` |
|       - | 1205 | ` * carries on.` |
|       - | 1206 | ` *` |
|       - | 1207 | ` * PHL had neither half. The notice was missing, and the copy was not a copy: a` |
|       - | 1208 | ` * userland offsetGet returns the container's own nested hashmap by COW, and` |
|       - | 1209 | ` * OP_STORE_IDX on a base with no slot index writes STRAIGHT INTO the shared map —` |
|       - | 1210 | `` * so `$o['a']['b'] = 9`, `$o['a'][] = 5` and `foreach ($o['a'] as &$v)` all`` |
|       - | 1211 | ` * modified the object php leaves untouched, silently. Separating the value here` |
|       - | 1212 | ` * is what makes the write land nowhere.` |
|       - | 1213 | ` *` |
|       - | 1214 | ` * php stays silent for an OBJECT, and so does this: an object is a handle, the` |
|       - | 1215 | ` * write through it is not lost, and nothing about it is indirect.` |
|       - | 1216 | ` */` |
|      50 | 1217 | `PH7_PRIVATE void PH7_VmOverloadedElemNotice(ph7_vm *pVm,ph7_class *pClass,ph7_value *pVal)` |
|       1 | 1218 | `{` |
|      51 | 1219 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       9 | 1220 | `		return;` |
|       - | 1221 | `	}` |
|      64 | 1222 | `	VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|       - | 1223 | `		"Indirect modification of overloaded element of %z has no effect",` |
|      21 | 1224 | `		&pClass->sName);` |
|      43 | 1225 | `	if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|      35 | 1226 | `		PH7_HashmapCowSeparate(&(*pVm),pVal);` |
|      17 | 1227 | `	}` |
|      26 | 1228 | `}` |
|       - | 1229 | `/*` |
|       - | 1230 | ` * OP_LOAD_IDX: body moved verbatim from the OP_LOAD_IDX arm of` |
|       - | 1231 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 1232 | ` */` |
| 1077175 | 1233 | `PH7_PRIVATE VmOpRc VmExecOpLoadIdx(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 1234 | `{` |
| 1077180 | 1235 | `	ph7_value *pTos = pState->pTos;` |
| 1077180 | 1236 | `	ph7_value *pStack = pState->pStack;` |
| 1077180 | 1237 | `	VmInstr *aInstr = pState->aInstr;` |
| 1077180 | 1238 | `	sxi32 pc = pState->pc;` |
|       - | 1239 | `	sxi32 rc;` |
|  539831 | 1240 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 1077180 | 1241 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
| 1077180 | 1242 | `	ph7_hashmap *pMap = 0;` |
|       - | 1243 | `	ph7_value *pIdx;` |
|       - | 1244 | `	/* D1 commit 2: iP2==9 is the deferred-record mode. It behaves exactly like a plain read` |
|       - | 1245 | `	 * (iP2==0) for base dispatch / the read tail, EXCEPT the dedicated block right after the` |
|       - | 1246 | `	 * index is popped, which — on a lookup MISS with a reachable base — captures the lvalue` |
|       - | 1247 | `	 * path (MEMOBJ_AUX_DEFPATH) instead of warning, and exits. Everything else sees iP2. */` |
| 1077180 | 1248 | `	sxi32 iP2 = (pInstr->iP2 == 9) ? 0 : pInstr->iP2;` |
| 1077180 | 1249 | `	pIdx = 0;` |
| 1077180 | 1250 | `	if( pInstr->iP1 == 0 ){` |
|      40 | 1251 | `		if( !iP2){` |
|       - | 1252 | ``			/* `[]` with nothing to append INTO. Every placement php refuses is a compile`` |
|       - | 1253 | `			 * error now (compile.c), so the only shape that reaches here is the one php` |
|       - | 1254 | `			 * also settles at runtime: a call ARGUMENT, whose parameter may turn out to be` |
|       - | 1255 | `			 * by-reference (php appends and binds) or by-value (php's Error). Record the` |
|       - | 1256 | `			 * append as a step of the deferred lvalue path and let OP_CALL decide. */` |
|      11 | 1257 | `			if( pInstr->iP2 == 9 ){` |
|      11 | 1258 | `				VmDeferredPath *pPath = 0;` |
|      11 | 1259 | `				if( pTos->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|     ! 0 | 1260 | `					pPath = (VmDeferredPath *)pTos->x.pOther;` |
|     ! 0 | 1261 | `					if( VmDeferPathPushAppend(pPath) == SXRET_OK ){` |
|     ! 0 | 1262 | `						VM_EXIT_BREAK; /* carrier already on pTos */` |
|     ! 0 | 1263 | `					}` |
|      11 | 1264 | `				}else if( pTos->iFlags & MEMOBJ_AUX_DEFERRED ){` |
|       - | 1265 | `					SyString sRootName;` |
|       3 | 1266 | `					SyStringInitFromBuf(&sRootName,(const char *)pTos->x.pOther,` |
|       - | 1267 | `						pTos->x.pOther ? SyStrlen((const char *)pTos->x.pOther) : 0);` |
|       3 | 1268 | `					pPath = VmDeferPathNew(&(*pVm),1,SXU32_HIGH,&sRootName);` |
|       3 | 1269 | `					if( pPath && VmDeferPathPushAppend(pPath) == SXRET_OK ){` |
|       3 | 1270 | `						pTos->iFlags &= ~MEMOBJ_AUX_DEFERRED; /* borrowed name; don't free x.pOther */` |
|       3 | 1271 | `						pTos->x.pOther = pPath;` |
|       3 | 1272 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|       3 | 1273 | `						pTos->nIdx = SXU32_HIGH;` |
|       3 | 1274 | `						VM_EXIT_BREAK;` |
|       - | 1275 | `					}` |
|     ! 0 | 1276 | `					VmFreeDeferredPath(pPath);` |
|       9 | 1277 | `				}else if( pTos->nIdx != SXU32_HIGH ){` |
|       9 | 1278 | `					pPath = VmDeferPathNew(&(*pVm),0,pTos->nIdx,0);` |
|       9 | 1279 | `					if( pPath && VmDeferPathPushAppend(pPath) == SXRET_OK ){` |
|       9 | 1280 | `						PH7_MemObjRelease(pTos);` |
|       9 | 1281 | `						pTos->x.pOther = pPath;` |
|       9 | 1282 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|       9 | 1283 | `						pTos->nIdx = SXU32_HIGH;` |
|       9 | 1284 | `						VM_EXIT_BREAK;` |
|       - | 1285 | `					}` |
|     ! 0 | 1286 | `					VmFreeDeferredPath(pPath);` |
|     ! 0 | 1287 | `				}` |
|     ! 0 | 1288 | `			}` |
|       - | 1289 | `			/* Not a deferrable argument (or out of memory recording it): php's own` |
|       - | 1290 | `			 * Error, which replaced PH7's notice-and-NULL. */` |
|     ! 0 | 1291 | `			if( pTos >= pStack ){` |
|     ! 0 | 1292 | `				PH7_MemObjRelease(pTos);` |
|     ! 0 | 1293 | `			}else{` |
|       - | 1294 | `				/* TICKET 1433-020: Empty stack */` |
|     ! 0 | 1295 | `				pTos++;` |
|     ! 0 | 1296 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|     ! 0 | 1297 | `				pTos->nIdx = SXU32_HIGH;` |
|       - | 1298 | `			}` |
|       - | 1299 | `			{` |
|     ! 0 | 1300 | `			sxi32 rcRd = VmThrowFromVm(&(*pVm),"Error","Cannot use [] for reading",` |
|       - | 1301 | `				sizeof("Cannot use [] for reading")-1);` |
|     ! 0 | 1302 | `			if( rcRd == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     ! 0 | 1303 | `			rc = rcRd;` |
|     ! 0 | 1304 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1305 | `			}` |
|       - | 1306 | `		}` |
|      16 | 1307 | `	}else{` |
| 1077142 | 1308 | `		pIdx = pTos;` |
| 1077142 | 1309 | `		pTos--;` |
|       - | 1310 | `	}` |
| 1077170 | 1311 | `	if( pInstr->iP2 == 9 && pIdx ){` |
|       - | 1312 | `		/* D1 commit 2 record mode. Decide whether to CAPTURE this subscript as a deferred` |
|       - | 1313 | `		 * lvalue step (the by-ref/by-value decision is not known until OP_CALL), or fall` |
|       - | 1314 | `		 * through to a plain read (iP2 was normalized to 0 above). We defer only when the` |
|       - | 1315 | `		 * base can be reached again at resolve time: an existing descriptor (nested), the` |
|       - | 1316 | `		 * commit-1 undefined-variable marker, or a real container/string/scalar slot` |
|       - | 1317 | `		 * (nIdx != SXU32_HIGH). On a present array key we DO NOT defer — a hit already` |
|       - | 1318 | `		 * yields the aliasable read slot the by-ref binder needs. */` |
|   70199 | 1319 | `		VmDeferredPath *pPath = 0;` |
|   70199 | 1320 | `		int bDefer = 0, eRoot = 0;` |
|   70199 | 1321 | `		if( pTos->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|       - | 1322 | `			/* Nested: the base already carries a descriptor — extend it in place. */` |
|      22 | 1323 | `			pPath = (VmDeferredPath *)pTos->x.pOther;` |
|      22 | 1324 | `			bDefer = 1;` |
|   70189 | 1325 | `		}else if( pTos->iFlags & MEMOBJ_AUX_DEFERRED ){` |
|       - | 1326 | `			/* Undefined base variable (commit-1 marker): root the descriptor by name. */` |
|       - | 1327 | `			SyString sRootName;` |
|       3 | 1328 | `			SyStringInitFromBuf(&sRootName,(const char *)pTos->x.pOther,` |
|       - | 1329 | `				pTos->x.pOther ? SyStrlen((const char *)pTos->x.pOther) : 0);` |
|       3 | 1330 | `			pPath = VmDeferPathNew(&(*pVm),1,SXU32_HIGH,&sRootName);` |
|       3 | 1331 | `			pTos->iFlags &= ~MEMOBJ_AUX_DEFERRED; /* borrowed name; don't free x.pOther */` |
|       3 | 1332 | `			pTos->x.pOther = 0;` |
|       3 | 1333 | `			bDefer = (pPath != 0);` |
|   70178 | 1334 | `		}else if( pTos->nIdx != SXU32_HIGH ){` |
|       - | 1335 | `			/* A real base slot: array/scalar -> eRoot 0 (by-ref may vivify), string -> 2. */` |
|   70087 | 1336 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       - | 1337 | `				/* Probe hit/miss on a COPY of the key: PH7_HashmapLookup casts a NULL key to` |
|       - | 1338 | `				 * "" in place, which would suppress the fall-through read's null-offset` |
|       - | 1339 | `				 * deprecation (hit) and capture the wrong key in the step (miss). */` |
|       - | 1340 | `				ph7_value idxProbe;` |
|   25083 | 1341 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   25083 | 1342 | `				PH7_MemObjInit(&(*pVm),&idxProbe);` |
|   25083 | 1343 | `				PH7_MemObjStore(pIdx,&idxProbe);` |
|   25083 | 1344 | `				if( PH7_HashmapLookup(pMap,&idxProbe,&pNode) == SXRET_OK ){` |
|   25025 | 1345 | `					bDefer = 0; /* present key: fall through and read it as an aliasable slot */` |
|   12515 | 1346 | `				}else{` |
|      61 | 1347 | `					eRoot = 0; bDefer = 1;` |
|       - | 1348 | `				}` |
|   25083 | 1349 | `				PH7_MemObjRelease(&idxProbe);` |
|   57548 | 1350 | `			}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       - | 1351 | `				/* An ArrayAccess base splits the way php's read_dimension does. A WRITABLE` |
|       - | 1352 | `				 * container answers out of its own storage with no accessor call, so the` |
|       - | 1353 | `				 * fetch really can wait for the callee: deferring it is what lets a` |
|       - | 1354 | `				 * by-reference argument take php's WRITE fetch, which CREATES a missing key` |
|       - | 1355 | ``				 * (`sort($ao['nokey'])`) instead of warning about a read and passing NULL.`` |
|       - | 1356 | `				 * Everything else answers through a METHOD, and php runs that method where` |
|       - | 1357 | `				 * the subscript is WRITTEN — so the accessor runs below and its RESULT rides` |
|       - | 1358 | `				 * a prefetch carrier built at the tail of the ArrayAccess branch. */` |
|     127 | 1359 | `				ph7_class_instance *pRecInst = (ph7_class_instance *)pTos->x.pOther;` |
|     127 | 1360 | `				eRoot = 0;` |
|     253 | 1361 | `				bDefer = (pRecInst && pVm->pArrayAccessClass` |
|     126 | 1362 | `				       && PH7_VmInstanceOf(pRecInst->pClass,pVm->pArrayAccessClass)` |
|     189 | 1363 | `				       && PH7_VmDimFetchWritable(pRecInst->pClass)) ? 1 : 0;` |
|   44946 | 1364 | `			}else if( pTos->iFlags & MEMOBJ_STRING ){` |
|   44883 | 1365 | `				eRoot = 2; bDefer = 1;` |
|   22613 | 1366 | `			}else{` |
|     ! 0 | 1367 | `				eRoot = 0; bDefer = 1; /* NULL/other reachable scalar base */` |
|       - | 1368 | `			}` |
|   70087 | 1369 | `			if( bDefer && pPath == 0 ){` |
|   45013 | 1370 | `				pPath = VmDeferPathNew(&(*pVm),eRoot,pTos->nIdx,0);` |
|   45013 | 1371 | `				if( pPath == 0 ){` |
|     ! 0 | 1372 | `					bDefer = 0;` |
|     ! 0 | 1373 | `				}` |
|   22673 | 1374 | `			}` |
|   35210 | 1375 | `		}` |
|   70199 | 1376 | `		if( bDefer ){` |
|       - | 1377 | `			/* Append this element step (deep-copies pIdx) and leave the carrier on pTos. */` |
|   45035 | 1378 | `			if( VmDeferPathPushElem(pPath,pIdx) == SXRET_OK ){` |
|   45035 | 1379 | `				if( (pTos->iFlags & MEMOBJ_AUX_DEFPATH) == 0 ){` |
|       - | 1380 | `					/* Collapse the base value into the descriptor carrier. */` |
|   45015 | 1381 | `					PH7_MemObjRelease(pTos);` |
|   45015 | 1382 | `					pTos->x.pOther = pPath;` |
|   45015 | 1383 | `					pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|   45015 | 1384 | `					pTos->nIdx = SXU32_HIGH;` |
|   22674 | 1385 | `				}` |
|   45035 | 1386 | `				PH7_MemObjRelease(pIdx);` |
|   45035 | 1387 | `				VM_EXIT_BREAK;` |
|       - | 1388 | `			}` |
|       - | 1389 | `			/* Out of memory appending a step. */` |
|     ! 0 | 1390 | `			if( pTos->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|       - | 1391 | `				/* Nested base: pPath IS pTos's carrier and stays owned by it — keep it (a` |
|       - | 1392 | `				 * step short) and exit, rather than fall through and misread a NULL-typed` |
|       - | 1393 | `				 * carrier as an array base. Resolves the shorter path; OOM-only degradation. */` |
|     ! 0 | 1394 | `				PH7_MemObjRelease(pIdx);` |
|     ! 0 | 1395 | `				VM_EXIT_BREAK;` |
|       - | 1396 | `			}` |
|       - | 1397 | `			/* A freshly-allocated path we still own: drop it and fall back to a plain read. */` |
|     ! 0 | 1398 | `			VmFreeDeferredPath(pPath);` |
|     ! 0 | 1399 | `		}` |
|   12582 | 1400 | `	}` |
| 1032140 | 1401 | `	if( iP2 == 7 && (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|       - | 1402 | ``		/* Keyed list destructuring `["k"=>$v] = $src` from a NON-array source: yield NULL`` |
|       - | 1403 | `		 * (never char-index a string), warning once per key — matching PHP, which warns per` |
|       - | 1404 | `		 * key here. A NULL source is silent; unlike the positional OP_LOAD_LIST path, a bool` |
|       - | 1405 | `		 * source DOES warn (PHP warns for bool in keyed destructuring).` |
|       - | 1406 | `		 * An OBJECT is not one of these: php destructures it through its` |
|       - | 1407 | `		 * read_dimension handler like any other subscript, so it falls through to` |
|       - | 1408 | `		 * the object dispatch below — which answers out of the accessor and` |
|       - | 1409 | ``		 * raises php's `Cannot use object of type C as array` for a class that`` |
|       - | 1410 | ``		 * has none. `["k"=>$v] = $obj` warned and yielded NULL for every source`` |
|       - | 1411 | `		 * but the one shape (a writable container answering out of its own` |
|       - | 1412 | `		 * storage) that reached the fast path underneath. */` |
|       7 | 1413 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       5 | 1414 | `			VmWarnCannotUseAsArray(&(*pVm),pTos->iFlags);` |
|       2 | 1415 | `		}` |
|       7 | 1416 | `		if( pIdx ){` |
|       - | 1417 | `			/* Release the key (a string literal for keyed destructuring), like the` |
|       - | 1418 | `			 * normal hashmap-read exit below — otherwise its blob is orphaned. */` |
|       7 | 1419 | `			PH7_MemObjRelease(pIdx);` |
|       3 | 1420 | `		}` |
|       7 | 1421 | `		PH7_MemObjRelease(pTos);` |
|       7 | 1422 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|       7 | 1423 | `		VM_EXIT_BREAK;` |
|       - | 1424 | `	}` |
| 1032134 | 1425 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|       - | 1426 | `		/* String access */` |
|  853138 | 1427 | `		if( VM_IDX_IS_UNSET(iP2) ){` |
|       - | 1428 | ``			/* php: a string offset cannot be unset AT ALL — `unset($s[0])` is the`` |
|       - | 1429 | ``			 * catchable `Error: Cannot unset string offsets`, whatever the offset is`` |
|       - | 1430 | `			 * and whether or not it is in range. PHL read the character but left the` |
|       - | 1431 | `			 * BASE VARIABLE's slot index on the result, so the trailing unset()` |
|       - | 1432 | ``			 * builtin freed the base itself: `$s = "abc"; unset($s[1]);` left $s`` |
|       - | 1433 | ``			 * UNDEFINED, and `unset($a["k"][0])` deleted the whole element. */`` |
|      11 | 1434 | `			rc = VmThrowFromVm(&(*pVm),"Error","Cannot unset string offsets",` |
|       - | 1435 | `				sizeof("Cannot unset string offsets")-1);` |
|      11 | 1436 | `			if( pIdx ){` |
|      11 | 1437 | `				PH7_MemObjRelease(pIdx);` |
|       5 | 1438 | `			}` |
|      11 | 1439 | `			PH7_MemObjRelease(pTos);` |
|      11 | 1440 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      11 | 1441 | `			pTos->nIdx = SXU32_HIGH;` |
|      11 | 1442 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      11 | 1443 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1444 | `		}` |
|  853128 | 1445 | `		if( pIdx ){` |
|  853128 | 1446 | `			sxi64 iOfft = 0, iRaw;` |
|  853128 | 1447 | `			sxi64 nLen = (sxi64)SyBlobLength(&pTos->sBlob);` |
|       - | 1448 | `			/* LOAD_IDX carries its OWN iP2 codes, which do NOT line up with the` |
|       - | 1449 | ``			 * PH7_MEMBER_* ones: 4 = isset, 5 = unset, 6 = empty, 8 = `??` read,`` |
|       - | 1450 | ``			 * 3 = `??=` peek. All are LOOKUPS, but php splits them into TWO`` |
|       - | 1451 | `			 * levels: isset()/empty()/unset() say nothing at all, while a` |
|       - | 1452 | ``			 * `??`/`??=` fetch still warns about the offset SHAPE and reads it`` |
|       - | 1453 | `			 * (VM_STROFF_COALESCE). The ??= peek is recognised by the NULLC_JMP` |
|       - | 1454 | `			 * that follows it, since its iP2 does not distinguish the base type. */` |
| 1280706 | 1455 | `			int iOfftLevel = (iP2 == 4 \|\| VM_IDX_IS_UNSET(iP2) \|\| iP2 == 6) ? VM_STROFF_ISSET` |
| 1704066 | 1456 | `				: ((iP2 == 8 \|\| VmIdxFeedsCoalesce(pInstr)) ? VM_STROFF_COALESCE` |
|  850959 | 1457 | `				: VM_STROFF_LOUD);` |
|  853128 | 1458 | `			int bQuiet = iOfftLevel != VM_STROFF_LOUD;` |
|       - | 1459 | `			SyBlob sTypeMsg;` |
|       - | 1460 | `			int eOfft;` |
|  853128 | 1461 | `			VmCoalStrOff *pCoalOff = 0;` |
|  853128 | 1462 | `			if( VmIdxFeedsCoalesceAssign(pInstr) ){` |
|       - | 1463 | ``				/* `$s[k] ??= v`: the OP_NULLC_STORE ahead has to write into the`` |
|       - | 1464 | `				 * string OFFSET, and by then the offset is gone — this op consumes` |
|       - | 1465 | `				 * it. Carry the RAW index to the store on the peek's own result` |
|       - | 1466 | `				 * (MEMOBJ_AUX_COALSTROFF), which nests and cannot leak; the copy` |
|       - | 1467 | `				 * must predate the resolution below, which casts a float/null/bool` |
|       - | 1468 | `				 * in place, because php re-resolves the offset LOUDLY at the store:` |
|       - | 1469 | `				 * the peek is the quiet half of its pair. */` |
|      53 | 1470 | `				pCoalOff = VmCoalStrOffNew(&(*pVm),pIdx);` |
|      26 | 1471 | `			}` |
|  853128 | 1472 | `			eOfft = VmStringOffsetResolve(&(*pVm),pIdx,iOfftLevel,&iOfft,&sTypeMsg);` |
|  853128 | 1473 | `			if( eOfft == VM_STROFF_MISS ){` |
|       - | 1474 | `				/* A lookup over an offset php refuses: not set, in silence. */` |
|      32 | 1475 | `				PH7_MemObjRelease(pIdx);` |
|      32 | 1476 | `				PH7_MemObjRelease(pTos);` |
|      32 | 1477 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      32 | 1478 | `				if( pCoalOff ){` |
|       - | 1479 | ``					/* A `??=` whose offset the READ refuses: php raises at the`` |
|       - | 1480 | `					 * STORE instead, so keep the base slot reachable and hand the` |
|       - | 1481 | `					 * offset to OP_NULLC_STORE. */` |
|       3 | 1482 | `					pTos->x.pOther = (void *)pCoalOff;` |
|       3 | 1483 | `					pTos->iFlags \|= MEMOBJ_AUX_STROFFSET\|MEMOBJ_AUX_COALSTROFF;` |
|       2 | 1484 | `				}else{` |
|      30 | 1485 | `					pTos->nIdx = SXU32_HIGH;` |
|       - | 1486 | `				}` |
|      50 | 1487 | `				VM_EXIT_BREAK;` |
|       - | 1488 | `			}` |
|  853098 | 1489 | `			if( eOfft == VM_STROFF_REJECT ){` |
|       - | 1490 | `				/* php's TypeError for an offset type a string refuses. PHL cast` |
|       - | 1491 | ``				 * every offset to int, so `$s[""]`, `$s["-"]` and `$s["p"]` all`` |
|       - | 1492 | ``				 * answered `$s[0]`. Routed as a mid-expression throw (this opcode`` |
|       - | 1493 | `				 * is not a call boundary), so the rest of the expression is` |
|       - | 1494 | `				 * abandoned the way php abandons it. */` |
|      40 | 1495 | `				VmFreeCoalStrOff(pCoalOff);` |
|      40 | 1496 | `				rc = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|      40 | 1497 | `				PH7_MemObjRelease(pIdx);` |
|      40 | 1498 | `				PH7_MemObjRelease(pTos);` |
|      40 | 1499 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      40 | 1500 | `				pTos->nIdx = SXU32_HIGH;` |
|      40 | 1501 | `				if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      40 | 1502 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1503 | `			}` |
|  853062 | 1504 | `			iRaw = iOfft;` |
|       - | 1505 | `			/* php 7.1: a NEGATIVE offset counts back from the end ($s[-1] is the last` |
|       - | 1506 | `			 * character). The offset used to be cast to UNSIGNED, so -1 became a huge` |
|       - | 1507 | `			 * number, ran past the end and quietly produced NULL. */` |
|  853062 | 1508 | `			if( iOfft < 0 ){` |
|      20 | 1509 | `				iOfft += nLen;` |
|       9 | 1510 | `			}` |
|  853062 | 1511 | `			if( iOfft < 0 \|\| iOfft >= nLen ){` |
|       - | 1512 | `				/* Out of range. In an isset()/empty() lookup php answers FALSE, so load` |
|       - | 1513 | `				 * NULL there; everywhere else it WARNS and yields the empty string (PH7` |
|       - | 1514 | `				 * silently produced NULL in both cases). */` |
|      68 | 1515 | `				PH7_MemObjRelease(pTos);` |
|      68 | 1516 | `				if( bQuiet ){` |
|      58 | 1517 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      30 | 1518 | `				}else{` |
|      12 | 1519 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|      17 | 1520 | `					VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Uninitialized string offset %qd",` |
|       5 | 1521 | `						iRaw);` |
|       - | 1522 | `				}` |
|      35 | 1523 | `			}else{` |
|  852996 | 1524 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|  852996 | 1525 | `				int c = zData[iOfft];` |
|  852996 | 1526 | `				PH7_MemObjRelease(pTos);` |
|  852996 | 1527 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|  852996 | 1528 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|       - | 1529 | `			}` |
|  853062 | 1530 | `			if( pCoalOff ){` |
|      49 | 1531 | `				if( pTos->iFlags & MEMOBJ_NULL ){` |
|       - | 1532 | ``					/* Out of range: the `??=` will store, so hand the offset over. */`` |
|      41 | 1533 | `					pTos->x.pOther = (void *)pCoalOff;` |
|      41 | 1534 | `					pTos->iFlags \|= MEMOBJ_AUX_COALSTROFF;` |
|      21 | 1535 | `				}else{` |
|       - | 1536 | ``					/* A real byte: the `??=` short-circuits over the store. */`` |
|       9 | 1537 | `					VmFreeCoalStrOff(pCoalOff);` |
|       - | 1538 | `				}` |
|      24 | 1539 | `			}` |
|       - | 1540 | `			/* The result still carries the BASE VARIABLE's slot index, which is` |
|       - | 1541 | `			 * harmless for a plain read and WRONG for anything that would ALIAS it:` |
|       - | 1542 | `			 * a string offset is not a slot. Mark it so the reference-binding sites` |
|       - | 1543 | `			 * raise php's Error instead of aliasing the whole string --` |
|       - | 1544 | ``			 * `$r = &$s[1]; $r = "Z";` REPLACED $s with "Z". */`` |
|  853062 | 1545 | `			pTos->iFlags \|= MEMOBJ_AUX_STROFFSET;` |
|  427586 | 1546 | `		}else{` |
|       - | 1547 | `			/* No available index,load NULL */` |
|     ! 0 | 1548 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       - | 1549 | `		}` |
|  853062 | 1550 | `		VM_EXIT_BREAK;` |
|       - | 1551 | `	}` |
|  179001 | 1552 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       - | 1553 | `		/* Object subscript: ArrayAccess dispatch.` |
|       - | 1554 | `		 * iP2 codes:` |
|       - | 1555 | `		 *   0 = read       → offsetGet` |
|       - | 1556 | `		 *   3 = ??= peek   → offsetExists; offsetGet on hit; arm coalesce` |
|       - | 1557 | `		 *                    target on miss for the upcoming NULLC_STORE` |
|       - | 1558 | `		 *   4 = isset()    → offsetExists` |
|       - | 1559 | `		 *   5 = unset()    → offsetUnset` |
|       - | 1560 | `		 *   6 = empty()    → offsetExists, then offsetGet on hit` |
|       - | 1561 | ``		 *   8 = `??` read  → same probe as 6 (offsetExists, then offsetGet on a`` |
|       - | 1562 | `		 *                    hit) and no diagnostics anywhere in this op: php` |
|       - | 1563 | ``		 *                    evaluates the whole left operand of `??` in`` |
|       - | 1564 | `		 *                    isset-context, and calling offsetGet blindly also` |
|       - | 1565 | `		 *                    surfaced warnings raised INSIDE a userland` |
|       - | 1566 | `		 *                    offsetGet (e.g. ArrayObject's own array read). */` |
|     801 | 1567 | `		ph7_class_instance *pInst = (ph7_class_instance *)pTos->x.pOther;` |
|     801 | 1568 | `		ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|       - | 1569 | `		/* php's read_dimension / has_dimension HANDLERS, which a native class may` |
|       - | 1570 | ``		 * carry without implementing ArrayAccess -- `$list[0]` reads a DOMNodeList`` |
|       - | 1571 | ``		 * there while `$list instanceof ArrayAccess` is false. They come FIRST`` |
|       - | 1572 | `		 * because php's interface is implemented THROUGH the handler: a user` |
|       - | 1573 | `		 * subclass declaring ArrayAccess inherits the parent's handler, so its own` |
|       - | 1574 | `		 * offsetGet/offsetExists are not consulted for a READ. The WRITE half is` |
|       - | 1575 | `		 * not here at all -- a store, an append and an unset (iP2 5) fall past` |
|       - | 1576 | ``		 * this into php's `Cannot use object of type C as array` unless the class`` |
|       - | 1577 | `		 * really implements the interface, which is php's own split (that same` |
|       - | 1578 | `		 * subclass DOES get its offsetSet called). */` |
|     801 | 1579 | `		if( pInst && iP2 != 5 && PH7_ClassHasNativeDim(pInst->pClass) ){` |
|       - | 1580 | `			PH7_NativeDimCtx sDim;` |
|       - | 1581 | `			ph7_value sResult;` |
|       - | 1582 | ``			/* `$o[$k] op= v` is php's read-then-WRITE pair, and php reports the`` |
|       - | 1583 | `			 * WRITE's refusal when the read answered nothing at all: an offset the` |
|       - | 1584 | ``			 * handler REFUSED comes back to `zend_binary_assign_op_obj_dim` as a`` |
|       - | 1585 | ``			 * miss, which raises `Cannot use object of type C as array` and chains`` |
|       - | 1586 | ``			 * the refusal behind it. `??=` is not that pair -- it reads in`` |
|       - | 1587 | `			 * isset-context, so its refusal is what surfaces -- and neither of them` |
|       - | 1588 | `			 * decides the store itself: that goes through the ordinary write path` |
|       - | 1589 | `			 * below, which is offsetSet for a subclass that has one. */` |
|     304 | 1590 | `			int bRmwCtx = (iP2 == 1) && VmNextIsCompoundAssign(pInstr + 1);` |
|     304 | 1591 | `			int bIsset = (iP2 == 4 \|\| iP2 == 6);` |
|     304 | 1592 | `			PH7_MemObjInit(&(*pVm),&sResult);` |
|     304 | 1593 | `			sDim.iMode = bIsset ? PH7_NATIVE_DIM_ISSET : PH7_NATIVE_DIM_READ;` |
|     304 | 1594 | `			sDim.pOffset = pIdx;` |
|     304 | 1595 | `			sDim.pResult = &sResult;` |
|     304 | 1596 | `			sDim.zThrowClass = 0;` |
|     304 | 1597 | `			sDim.zThrowMsg[0] = 0;` |
|     304 | 1598 | `			PH7_ClassNativeDim(pInst,&sDim);` |
|     304 | 1599 | `			if( iP2 == 6 && sDim.zThrowClass == 0 && ph7_value_to_bool(&sResult) ){` |
|       - | 1600 | `				/* empty(): php asks has_dimension first and reads the VALUE only on` |
|       - | 1601 | ``				 * a hit, which is why an out-of-range `empty($map[-1])` is a plain`` |
|       - | 1602 | `				 * TRUE where the read of the same offset refuses. */` |
|       7 | 1603 | `				PH7_MemObjRelease(&sResult);` |
|       7 | 1604 | `				PH7_MemObjInit(&(*pVm),&sResult);` |
|       7 | 1605 | `				sDim.iMode = PH7_NATIVE_DIM_READ;` |
|       7 | 1606 | `				sDim.pResult = &sResult;` |
|       7 | 1607 | `				PH7_ClassNativeDim(pInst,&sDim);` |
|       3 | 1608 | `			}` |
|     304 | 1609 | `			if( sDim.zThrowClass ){` |
|       - | 1610 | `				char zMsg[256];` |
|      32 | 1611 | `				const char *zClass = sDim.zThrowClass;` |
|      32 | 1612 | `				const char *zText = sDim.zThrowMsg;` |
|       - | 1613 | `				sxu32 nMsg;` |
|      32 | 1614 | `				if( bRmwCtx ){` |
|       5 | 1615 | `					SyString *pName = &pInst->pClass->sName;` |
|       5 | 1616 | `					zClass = "Error";` |
|       5 | 1617 | `					zText = zMsg;` |
|       7 | 1618 | `					nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - | 1619 | `						"Cannot use object of type %.*s as array",` |
|       4 | 1620 | `						(int)pName->nByte,pName->zString);` |
|       3 | 1621 | `				}else{` |
|      28 | 1622 | `					nMsg = (sxu32)SyStrlen(zText);` |
|       - | 1623 | `				}` |
|      32 | 1624 | `				VmCoalesceDisarm(pVm);` |
|      32 | 1625 | `				rc = VmThrowFromVm(pVm,zClass,zText,nMsg);` |
|      32 | 1626 | `				PH7_MemObjRelease(&sResult);` |
|      32 | 1627 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|      32 | 1628 | `				PH7_MemObjRelease(pTos);` |
|      32 | 1629 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      32 | 1630 | `				pTos->nIdx = SXU32_HIGH;` |
|      32 | 1631 | `				if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      54 | 1632 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1633 | `			}` |
|     274 | 1634 | `			if( iP2 == 4 ){` |
|       - | 1635 | `				/* isset(): push a BOOL, which is also what keeps vm_builtin_isset` |
|       - | 1636 | `				 * from warning about a non-variable operand. */` |
|      27 | 1637 | `				int bExists = ph7_value_to_bool(&sResult);` |
|      27 | 1638 | `				PH7_MemObjRelease(&sResult);` |
|      27 | 1639 | `				PH7_MemObjRelease(pTos);` |
|      27 | 1640 | `				pTos->nIdx = SXU32_HIGH;` |
|      27 | 1641 | `				if( bExists ){` |
|       7 | 1642 | `					MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       7 | 1643 | `					pTos->x.iVal = 1;` |
|       4 | 1644 | `				}else{` |
|      21 | 1645 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       1 | 1646 | `				}` |
|     261 | 1647 | `			}else if( iP2 == 3 && (sResult.iFlags & MEMOBJ_NULL) ){` |
|       - | 1648 | ``				/* `$o[$k] ??= v` and the read found nothing: arm (object, key) so`` |
|       - | 1649 | `				 * the NULLC_STORE that follows performs php's store -- offsetSet for` |
|       - | 1650 | ``				 * a subclass that declares one, and `Cannot use object of type C as`` |
|       - | 1651 | ``				 * array` for the collections themselves, which is the same verdict`` |
|       - | 1652 | ``				 * the plain `$o[$k] = v` gets. */`` |
|       5 | 1653 | `				VmCoalesceDisarm(pVm);` |
|       5 | 1654 | `				PH7_MemObjRelease(pTos);` |
|       5 | 1655 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|       5 | 1656 | `				pTos->nIdx = SXU32_HIGH;` |
|       5 | 1657 | `				if( pIdx ){` |
|       5 | 1658 | `					PH7_MemObjStore(pIdx,&pVm->sCoalesceKey);` |
|       2 | 1659 | `				}` |
|       5 | 1660 | `				pVm->pCoalesceObj = pInst;` |
|       5 | 1661 | `				pInst->iRef++;` |
|       5 | 1662 | `				pVm->bCoalesceArmed = 1;` |
|       5 | 1663 | `				PH7_MemObjRelease(&sResult);` |
|       3 | 1664 | `			}else{` |
|       - | 1665 | `				/* The base slot may be the only thing holding this instance, and the` |
|       - | 1666 | `				 * write-context tail below still speaks for its CLASS -- hold a` |
|       - | 1667 | `				 * reference across the release, as the ArrayAccess arm does. */` |
|     244 | 1668 | `				pInst->iRef++;` |
|     244 | 1669 | `				if( iP2 == 3 ){` |
|       3 | 1670 | `					VmCoalesceDisarm(pVm); /* a hit short-circuits over the store */` |
|       1 | 1671 | `				}` |
|     244 | 1672 | `				PH7_MemObjRelease(pTos);` |
|     244 | 1673 | `				PH7_MemObjStore(&sResult,pTos);` |
|     244 | 1674 | `				pTos->nIdx = SXU32_HIGH;` |
|     244 | 1675 | `				if( bRmwCtx ){` |
|       - | 1676 | `					/* php's ASSIGN_DIM_OP: the read gave the current value, the op` |
|       - | 1677 | `					 * computes on it, and the result goes back out through the write` |
|       - | 1678 | `					 * path -- offsetSet where there is one, php's Error where there` |
|       - | 1679 | `					 * is not (VmHookRmwConsume). */` |
|      13 | 1680 | `					VmDimRmwArm(&(*pVm),pInst,pIdx,pTos,` |
|       8 | 1681 | `						(void *)pStack,(void *)aInstr,(sxu32)(pc + 1));` |
|     240 | 1682 | `				}else if( VmIdxFetchForWrite(pInstr,iP2) ){` |
|       - | 1683 | ``					/* php's `Indirect modification of overloaded element` -- silent`` |
|       - | 1684 | `					 * for an OBJECT, which is every value these containers answer,` |
|       - | 1685 | ``					 * and raised for the NULL a miss leaves (`$list[9]++`). */`` |
|       5 | 1686 | `					PH7_VmOverloadedElemNotice(&(*pVm),pInst->pClass,pTos);` |
|     234 | 1687 | `				}else if( pInstr->iP2 == 9 ){` |
|       - | 1688 | `					/* A deferred call ARGUMENT: the read has happened, and whether php` |
|       - | 1689 | `					 * performed a W fetch is the callee's to say. Carry the value plus` |
|       - | 1690 | `					 * the class that answered it so the verdict lands at the call. */` |
|      31 | 1691 | `					VmDeferredPath *pPre = VmDeferPathNewPrefetch(&(*pVm),VM_OVER_ELEM,` |
|      10 | 1692 | `						pInst->pClass,0,pTos);` |
|      21 | 1693 | `					if( pPre ){` |
|      21 | 1694 | `						PH7_MemObjRelease(pTos);` |
|      21 | 1695 | `						pTos->x.pOther = pPre;` |
|      21 | 1696 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|      21 | 1697 | `						pTos->nIdx = SXU32_HIGH;` |
|      10 | 1698 | `					}` |
|      10 | 1699 | `				}` |
|     244 | 1700 | `				PH7_ClassInstanceUnref(pInst);` |
|     244 | 1701 | `				PH7_MemObjRelease(&sResult);` |
|       - | 1702 | `			}` |
|     274 | 1703 | `			if( pIdx ){` |
|     274 | 1704 | `				PH7_MemObjRelease(pIdx);` |
|     136 | 1705 | `			}` |
|     274 | 1706 | `			VM_EXIT_BREAK;` |
|       - | 1707 | `		}` |
|     499 | 1708 | `		if( pArrayAccess && pInst && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|       - | 1709 | `			ph7_class_method *pMeth;` |
|       - | 1710 | `			ph7_value sResult;` |
|       - | 1711 | `			ph7_value sNullIdx;` |
|       - | 1712 | `			ph7_value *apArg[1];` |
|     482 | 1713 | `			if( pIdx && VmDimFastFetchCtx(pInstr,iP2) && PH7_VmDimFetchWritable(pInst->pClass)` |
|     218 | 1714 | `			 && pInst->iRef > 1 ){` |
|       - | 1715 | `				/* php hands a writable container's element back BY SLOT, and that is what` |
|       - | 1716 | ``				 * makes an indirect modification through it land. The `iRef > 1` guard is`` |
|       - | 1717 | ``				 * the object half of the array path's `pMap->iRef < 2` rule: releasing the`` |
|       - | 1718 | `				 * base below drops this stack slot's own reference, and a TEMPORARY` |
|       - | 1719 | ``				 * container (`(new ArrayObject([1]))[0]`) would be destroyed with its`` |
|       - | 1720 | `				 * storage while the result still views it. Such a base has nothing that` |
|       - | 1721 | `				 * could observe the write anyway, so it takes the accessor's copy. */` |
|     174 | 1722 | `				sxu32 nElem = PH7_SplDimElemSlot(&(*pVm),pInst,pIdx,` |
|       - | 1723 | `					/* php's write-context vivification, and only there: a W/RW fetch —` |
|       - | 1724 | ``					 * including the `$r = &$ao['k']` and by-ref-foreach shapes iP2 alone`` |
|       - | 1725 | `					 * cannot name — creates the missing element, while an unset chain's` |
|       - | 1726 | `					 * intermediate step never does. */` |
|     147 | 1727 | `					VmIdxFetchForWrite(pInstr,iP2) && !VM_IDX_IS_UNSET(iP2));` |
|     124 | 1728 | `				ph7_value *pElem = (nElem == SXU32_HIGH) ? 0` |
|     118 | 1729 | `					: (ph7_value *)SySetAt(&pVm->aMemObj,nElem);` |
|     124 | 1730 | `				if( pElem ){` |
|     116 | 1731 | `					PH7_MemObjRelease(pTos);` |
|     116 | 1732 | `					PH7_MemObjLoad(pElem,pTos);` |
|     116 | 1733 | `					pTos->nIdx = nElem;` |
|     116 | 1734 | `					PH7_MemObjRelease(pIdx);` |
|     116 | 1735 | `					VM_EXIT_BREAK;` |
|       - | 1736 | `				}` |
|       4 | 1737 | `			}` |
|     373 | 1738 | `			if( (iP2 == 0 \|\| iP2 == 3 \|\| iP2 == 8) && pIdx == 0 ){` |
|       - | 1739 | ``				/* `$obj[]` read — PHP rejects this. */`` |
|     ! 0 | 1740 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|       - | 1741 | `					"Cannot use [] for reading");` |
|     ! 0 | 1742 | `				PH7_MemObjRelease(pTos);` |
|     ! 0 | 1743 | `				pTos->nIdx = SXU32_HIGH;` |
|     ! 0 | 1744 | `				VM_EXIT_BREAK;` |
|       - | 1745 | `			}` |
|     373 | 1746 | `			PH7_MemObjInit(&(*pVm),&sResult);` |
|     373 | 1747 | `			if( iP2 == 4 \|\| iP2 == 6 \|\| iP2 == 3 \|\| iP2 == 8 ){` |
|       - | 1748 | ``				/* isset, empty, ??= and `??` all start with offsetExists. */`` |
|     105 | 1749 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 1750 | `					"offsetExists",sizeof("offsetExists")-1);` |
|     105 | 1751 | `				apArg[0] = pIdx;` |
|     105 | 1752 | `				if( pMeth ){` |
|     105 | 1753 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|      55 | 1754 | `				}` |
|     323 | 1755 | `			}else if( iP2 == 5 ){` |
|      38 | 1756 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 1757 | `					"offsetUnset",sizeof("offsetUnset")-1);` |
|      38 | 1758 | `				apArg[0] = pIdx;` |
|      38 | 1759 | `				if( pMeth ){` |
|      38 | 1760 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|      17 | 1761 | `				}` |
|      21 | 1762 | `			}else{` |
|     239 | 1763 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 1764 | `					"offsetGet",sizeof("offsetGet")-1);` |
|     239 | 1765 | `				if( pIdx == 0 ){` |
|       - | 1766 | ``					/* `$o[] op= v` — the one read that reaches here without a key.`` |
|       - | 1767 | `					 * php hands the accessors NULL for the absent offset (its` |
|       - | 1768 | `					 * read_dimension substitutes one), so passing NO argument` |
|       - | 1769 | `					 * turned an assignment php performs into an` |
|       - | 1770 | `					 * ArgumentCountError against the class's own offsetGet. */` |
|       3 | 1771 | `					PH7_MemObjInit(&(*pVm),&sNullIdx);` |
|       3 | 1772 | `					pIdx = &sNullIdx;` |
|       1 | 1773 | `				}` |
|     239 | 1774 | `				apArg[0] = pIdx;` |
|     239 | 1775 | `				if( pMeth ){` |
|     239 | 1776 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|     117 | 1777 | `				}` |
|       - | 1778 | `			}` |
|     373 | 1779 | `			if( iP2 == 4 ){` |
|       - | 1780 | `				/* isset: push MEMOBJ_BOOL so vm_builtin_isset reports the` |
|       - | 1781 | `				 * right truth value AND skips its "Expecting a variable not` |
|       - | 1782 | `				 * a constant" warning (keyed on MEMOBJ_BOOL). */` |
|      65 | 1783 | `				int bExists = ph7_value_to_bool(&sResult);` |
|      65 | 1784 | `				PH7_MemObjRelease(pTos);` |
|      65 | 1785 | `				pTos->nIdx = SXU32_HIGH;` |
|      65 | 1786 | `				if( bExists ){` |
|      36 | 1787 | `					MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      36 | 1788 | `					pTos->x.iVal = 1;` |
|      20 | 1789 | `				}else{` |
|      33 | 1790 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       5 | 1791 | `				}` |
|     343 | 1792 | `			}else if( iP2 == 5 ){` |
|       - | 1793 | `				/* offsetUnset return is discarded; push NULL so the trailing` |
|       - | 1794 | `				 * vm_builtin_unset is a harmless no-op. */` |
|      38 | 1795 | `				PH7_MemObjRelease(pTos);` |
|      38 | 1796 | `				pTos->nIdx = SXU32_HIGH;` |
|      38 | 1797 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|     296 | 1798 | `			}else if( iP2 == 6 \|\| iP2 == 8 ){` |
|       - | 1799 | `				/* empty: if offsetExists is false, push NULL so empty=true` |
|       - | 1800 | `				 * without calling offsetGet. If true, call offsetGet and` |
|       - | 1801 | `				 * push the value so PH7_builtin_empty evaluates emptiness.` |
|       - | 1802 | ``				 * `??` (8) needs the identical shape: NULL on a miss so the`` |
|       - | 1803 | `				 * coalesce takes the default, the real value on a hit. */` |
|      36 | 1804 | `				int bExists = ph7_value_to_bool(&sResult);` |
|      36 | 1805 | `				PH7_MemObjRelease(&sResult);` |
|      36 | 1806 | `				PH7_MemObjRelease(pTos);` |
|      36 | 1807 | `				pTos->nIdx = SXU32_HIGH;` |
|      36 | 1808 | `				if( !bExists ){` |
|      12 | 1809 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       8 | 1810 | `				}else{` |
|      28 | 1811 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 1812 | `						"offsetGet",sizeof("offsetGet")-1);` |
|       - | 1813 | `					ph7_value sValue;` |
|      28 | 1814 | `					PH7_MemObjInit(&(*pVm),&sValue);` |
|      28 | 1815 | `					apArg[0] = pIdx;` |
|      28 | 1816 | `					if( pGet ){` |
|      28 | 1817 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|      12 | 1818 | `					}` |
|      28 | 1819 | `					PH7_MemObjStore(&sValue,pTos);` |
|      28 | 1820 | `					PH7_MemObjRelease(&sValue);` |
|       - | 1821 | `				}` |
|      36 | 1822 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|      36 | 1823 | `				VM_EXIT_BREAK; /* skip the duplicate sResult release below */` |
|     247 | 1824 | `			}else if( iP2 == 3 ){` |
|       - | 1825 | `				/* ?? null-coalesce peek: emulate PHP semantics —` |
|       - | 1826 | `				 *   if !offsetExists OR offsetGet() === null → arm` |
|       - | 1827 | `				 *     coalesce slot (NULLC_STORE will call offsetSet)` |
|       - | 1828 | `				 *     and push NULL.` |
|       - | 1829 | `				 *   else → push offsetGet's value (NULLC_JMP skips). */` |
|      10 | 1830 | `				int bExists = ph7_value_to_bool(&sResult);` |
|      10 | 1831 | `				int bShouldArm = !bExists;` |
|       - | 1832 | `				ph7_value sValue;` |
|      10 | 1833 | `				PH7_MemObjRelease(&sResult);` |
|       - | 1834 | `				/* Reset any prior arming defensively */` |
|      10 | 1835 | `				VmCoalesceDisarm(pVm);` |
|      10 | 1836 | `				PH7_MemObjInit(&(*pVm),&sValue);` |
|      10 | 1837 | `				if( bExists ){` |
|       5 | 1838 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 1839 | `						"offsetGet",sizeof("offsetGet")-1);` |
|       5 | 1840 | `					apArg[0] = pIdx;` |
|       5 | 1841 | `					if( pGet ){` |
|       5 | 1842 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|       2 | 1843 | `					}` |
|       5 | 1844 | `					if( sValue.iFlags & MEMOBJ_NULL ){` |
|       3 | 1845 | `						bShouldArm = 1;` |
|       1 | 1846 | `					}` |
|       2 | 1847 | `				}` |
|      10 | 1848 | `				PH7_MemObjRelease(pTos);` |
|      10 | 1849 | `				pTos->nIdx = SXU32_HIGH;` |
|      10 | 1850 | `				if( bShouldArm ){` |
|       - | 1851 | `					/* Arm: remember (object, key) so NULLC_STORE dispatches` |
|       - | 1852 | `					 * to offsetSet. Hold a ref on the instance to survive` |
|       - | 1853 | `					 * intervening expression evaluation. */` |
|       8 | 1854 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       8 | 1855 | `					if( pIdx ){` |
|       8 | 1856 | `						PH7_MemObjStore(pIdx,&pVm->sCoalesceKey);` |
|       3 | 1857 | `					}` |
|       8 | 1858 | `					pVm->pCoalesceObj = pInst;` |
|       8 | 1859 | `					pInst->iRef++;` |
|       8 | 1860 | `					pVm->bCoalesceArmed = 1;` |
|       5 | 1861 | `				}else{` |
|       3 | 1862 | `					PH7_MemObjStore(&sValue,pTos);` |
|       - | 1863 | `				}` |
|      10 | 1864 | `				PH7_MemObjRelease(&sValue);` |
|      10 | 1865 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|      10 | 1866 | `				VM_EXIT_BREAK;` |
|     ! 0 | 1867 | `			}else{` |
|       - | 1868 | `				/* offsetGet: replace pTos with the returned value.` |
|       - | 1869 | `				 *` |
|       - | 1870 | `				 * The base slot may be the only thing holding this instance — a` |
|       - | 1871 | ``				 * TEMPORARY container (`f((new C)['a'])`, a getter's return) dies with`` |
|       - | 1872 | `				 * it — and everything below still speaks for the object: the writable` |
|       - | 1873 | `				 * test, php's notice and the read-modify-write arming all read its` |
|       - | 1874 | `				 * CLASS, and the deferred-argument carrier records it. Hold a reference` |
|       - | 1875 | `				 * of our own across the release so none of them is left reading freed` |
|       - | 1876 | `				 * memory. */` |
|     239 | 1877 | `				pInst->iRef++;` |
|     239 | 1878 | `				PH7_MemObjRelease(pTos);` |
|     239 | 1879 | `				PH7_MemObjStore(&sResult,pTos);` |
|     239 | 1880 | `				pTos->nIdx = SXU32_HIGH;` |
|     239 | 1881 | `				if( iP2 == 1 && VmNextIsCompoundAssign(pInstr + 1) ){` |
|       - | 1882 | ``					/* `$o[$k] op= v` is php's ASSIGN_DIM_OP: offsetGet gave the`` |
|       - | 1883 | `					 * current value, the op computes on it, and the result goes` |
|       - | 1884 | `					 * back through offsetSet($k, …). PHL had no write-back at` |
|       - | 1885 | `					 * all here — the fetched value carried no slot, so every` |
|       - | 1886 | `					 * compound assign on an ArrayAccess element died on` |
|       - | 1887 | `					 * "Cannot perform assignment on a constant class attribute"` |
|       - | 1888 | `					 * and stored nothing. Arm the scratch slot the op mutates;` |
|       - | 1889 | `					 * its tail (PH7_HOOK_RMW_WRITEBACK) dispatches offsetSet. */` |
|      64 | 1890 | `					VmDimRmwArm(&(*pVm),pInst,pIdx,pTos,` |
|      42 | 1891 | `						(void *)pStack,(void *)aInstr,(sxu32)(pc + 1));` |
|     214 | 1892 | `				}else if( VmIdxFetchForWrite(pInstr,iP2)` |
|     115 | 1893 | `				       && !PH7_VmDimFetchWritable(pInst->pClass) ){` |
|      25 | 1894 | `					PH7_VmOverloadedElemNotice(&(*pVm),pInst->pClass,pTos);` |
|     185 | 1895 | `				}else if( pInstr->iP2 == 9 ){` |
|       - | 1896 | `					/* A deferred call ARGUMENT. The accessor has just run — php runs it` |
|       - | 1897 | `					 * where the subscript is written, whatever the parameter turns out to` |
|       - | 1898 | `					 * be — but WHICH fetch php performed is the callee's to say, and only` |
|       - | 1899 | `					 * OP_CALL knows: a by-reference parameter makes it a W fetch, which on` |
|       - | 1900 | `					 * a container that can only answer with a VALUE is php's` |
|       - | 1901 | ``					 * `Indirect modification of overloaded element` notice and a write`` |
|       - | 1902 | `					 * thrown away. Carry the result plus the class that answered it, so the` |
|       - | 1903 | `					 * verdict lands at the call without the accessor running twice or the` |
|       - | 1904 | `					 * argument arriving as NULL. The value would otherwise reach the callee` |
|       - | 1905 | `					 * still SHARING the container's own nested map by COW, and a by-ref` |
|       - | 1906 | ``					 * `f($o['a']['b'])` wrote straight into the object php leaves untouched. */`` |
|      47 | 1907 | `					VmDeferredPath *pPre = VmDeferPathNewPrefetch(&(*pVm),VM_OVER_ELEM,pInst->pClass,0,pTos);` |
|      47 | 1908 | `					if( pPre ){` |
|      47 | 1909 | `						PH7_MemObjRelease(pTos);` |
|      47 | 1910 | `						pTos->x.pOther = pPre;` |
|      47 | 1911 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|      47 | 1912 | `						pTos->nIdx = SXU32_HIGH;` |
|      23 | 1913 | `					}` |
|      23 | 1914 | `				}` |
|     239 | 1915 | `				PH7_ClassInstanceUnref(pInst);` |
|       - | 1916 | `			}` |
|     333 | 1917 | `			PH7_MemObjRelease(&sResult);` |
|     333 | 1918 | `			if( pIdx ){` |
|     333 | 1919 | `				PH7_MemObjRelease(pIdx);` |
|     164 | 1920 | `			}` |
|     333 | 1921 | `			VM_EXIT_BREAK;` |
|       - | 1922 | `		}` |
|       - | 1923 | `		/* Object without ArrayAccess: PHP throws fatal Error in all subscript` |
|       - | 1924 | `		 * contexts (read, isset, unset, empty). Match it. */` |
|      15 | 1925 | `		if( pInst ){` |
|       - | 1926 | `			char zMsg[256];` |
|      15 | 1927 | `			SyString *pName = &pInst->pClass->sName;` |
|      21 | 1928 | `			sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - | 1929 | `				"Cannot use object of type %.*s as array",` |
|      12 | 1930 | `				(int)pName->nByte,pName->zString);` |
|      15 | 1931 | `			rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|      15 | 1932 | `			if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|      15 | 1933 | `			PH7_MemObjRelease(pTos);` |
|      15 | 1934 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      15 | 1935 | `			pTos->nIdx = SXU32_HIGH;` |
|      15 | 1936 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       - | 1937 | ``			/* `break` used to resume at the NEXT instruction: the catch ran and then`` |
|       - | 1938 | `			 * execution carried on inside the try block. */` |
|      21 | 1939 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1940 | `		}` |
|     ! 0 | 1941 | `	}` |
|  178205 | 1942 | `	if( (iP2 == 1 \|\| iP2 == 3 \|\| VM_IDX_IS_UNSET(iP2)) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      47 | 1943 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|       - | 1944 | `			ph7_value *pObj;` |
|      43 | 1945 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       - | 1946 | `				/* php 8 write-context auto-vivify rules: NULL converts to array` |
|       - | 1947 | `				 * silently; FALSE converts with the 8.1 deprecation; any other` |
|       - | 1948 | `				 * scalar base — int/float/true/resource — is php's catchable` |
|       - | 1949 | `				 * "Cannot use a scalar value as an array" Error and the variable` |
|       - | 1950 | `				 * stays untouched (pre-fix the base was silently CONVERTED,` |
|       - | 1951 | ``				 * corrupting e.g. `$i = 5; $i[0]++` into array(0 => 6); string`` |
|       - | 1952 | `				 * bases were intercepted by the string-offset paths above). */` |
|       - | 1953 | `				/* php auto-converts false to an array with an 8.1 DEPRECATION; PHL` |
|       - | 1954 | `				 * rejects it like any other scalar base (null still auto-vivifies —` |
|       - | 1955 | `				 * it is not a bool). */` |
|      43 | 1956 | `				if( (pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_RES\|MEMOBJ_BOOL)) != 0 ){` |
|       - | 1957 | `					/* unset() has its own wording for the same base: php's` |
|       - | 1958 | `					 * "Cannot unset offset in a non-array variable". */` |
|      12 | 1959 | `					const char *zErr = VM_IDX_IS_UNSET(iP2)` |
|       - | 1960 | `						? "Cannot unset offset in a non-array variable"` |
|      10 | 1961 | `						: "Cannot use a scalar value as an array";` |
|       - | 1962 | `					SyBlob sErrMsg;` |
|      16 | 1963 | `					SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      16 | 1964 | `					SyBlobAppend(&sErrMsg,zErr,(sxu32)SyStrlen(zErr));` |
|      16 | 1965 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      16 | 1966 | `					if( pIdx ){` |
|      16 | 1967 | `						PH7_MemObjRelease(pIdx);` |
|       7 | 1968 | `					}` |
|      16 | 1969 | `					PH7_MemObjRelease(pTos);` |
|      16 | 1970 | `					pTos->nIdx = SXU32_HIGH;` |
|      16 | 1971 | `					VM_EXIT_BREAK;` |
|       - | 1972 | `				}` |
|       - | 1973 | `				/* unset() CREATES NOTHING: a null/undefined base stays null where PHL` |
|       - | 1974 | ``				 * converted it to an empty array (`$n = null; unset($n[0]);` left`` |
|       - | 1975 | `				 * $n === []), which is also what materialised the missing intermediate` |
|       - | 1976 | ``				 * in `unset($a["y"]["z"])`. Leaving the base alone, the lookup below`` |
|       - | 1977 | `				 * misses, the tail loads NULL with no slot index, and the trailing` |
|       - | 1978 | `				 * unset() builtin is the no-op php's is. */` |
|      29 | 1979 | `				if( !VM_IDX_IS_UNSET(iP2) ){` |
|      27 | 1980 | `					PH7_MemObjToHashmap(pObj);` |
|      27 | 1981 | `					PH7_MemObjLoad(pObj,pTos);` |
|      12 | 1982 | `				}` |
|      13 | 1983 | `			}` |
|      13 | 1984 | `		}` |
|      15 | 1985 | `	}` |
|  178191 | 1986 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|       - | 1987 | `	/* php DEPRECATES both a null offset and a lossy-float subscript (then normalizes` |
|       - | 1988 | `	 * "" / truncates). PHL now matches php on the NULL offset (deprecate + coerce to` |
|       - | 1989 | `	 * the "" key, §2) but still rejects the lossy-FLOAT subscript with a TypeError on` |
|       - | 1990 | `	 * a READ or WRITE (iP2 0/1) — the recorded non-deprecated-surface policy. Both` |
|       - | 1991 | ``	 * stay lenient in isset()/empty()/`??`/unset(), where a throw would be wrong. */`` |
|       - | 1992 | `	/* An object/array key is rejected in EVERY context, including isset()/empty()/` |
|       - | 1993 | `	 * unset() where php still throws (only the wording changes) — unlike the` |
|       - | 1994 | `	 * null/float deprecations below, which stay lenient there. A resource key is` |
|       - | 1995 | `	 * accepted with a warning and becomes its integer id. */` |
|  178191 | 1996 | `	if( (pTos->iFlags & MEMOBJ_HASHMAP) && pIdx ){` |
|       - | 1997 | `		SyBlob sTypeMsg;` |
|  178099 | 1998 | `		if( VmOffsetTypeRejected(&(*pVm),pIdx,iP2,&sTypeMsg) ){` |
|       - | 1999 | `			/* Routed as a mid-expression throw, like the STRING arm above: this` |
|       - | 2000 | `			 * opcode is not a call boundary, so PARKING it let the rest of the` |
|       - | 2001 | ``			 * expression run first — `str_repeat($a[$obj], 2)` reached the builtin`` |
|       - | 2002 | `			 * with the NULL the abandoned read left and died on its ZPP TypeError` |
|       - | 2003 | `			 * instead, and a CAUGHT one still ran the enclosing call afterwards. */` |
|      21 | 2004 | `			rc = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|      21 | 2005 | `			PH7_MemObjRelease(pIdx);` |
|      21 | 2006 | `			PH7_MemObjRelease(pTos);` |
|      21 | 2007 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      21 | 2008 | `			pTos->nIdx = SXU32_HIGH;` |
|      30 | 2009 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      19 | 2010 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2011 | `		}` |
|  178079 | 2012 | `		VmOffsetResourceWarn(&(*pVm),pIdx);` |
|   89037 | 2013 | `	}` |
|  178171 | 2014 | `	if( (pTos->iFlags & MEMOBJ_HASHMAP) && pIdx ){` |
|       - | 2015 | `		/* php DEPRECATES a null offset in EVERY subscript context except unset()` |
|       - | 2016 | `		 * (iP2==5), then normalizes it to the "" key — read, write, isset (4),` |
|       - | 2017 | ``		 * empty (6), `??`/`??=` (3), the coalesce-chain probe (8), destructure`` |
|       - | 2018 | `		 * (2/7). Emit it here so all of them get it, not just read/write; the` |
|       - | 2019 | `		 * lookup/insert below casts NULL->"", and a plain read miss then warns` |
|       - | 2020 | ``		 * `Undefined array key ""` on the now-string pIdx (php's exact pair). */`` |
|  178079 | 2021 | `		if( (pIdx->iFlags & MEMOBJ_NULL) && !VM_IDX_IS_UNSET(iP2) ){` |
|      21 | 2022 | `			VmNullOffsetDeprecate(&(*pVm),pIdx);` |
|       9 | 2023 | `		}` |
|       - | 2024 | `		/* A lossy-FLOAT subscript stays a rejected TypeError on a READ or WRITE` |
|       - | 2025 | `		 * (iP2 0/1) — the recorded non-deprecated-surface policy (§2); the lenient` |
|       - | 2026 | `		 * contexts (isset/empty/??/unset) truncate quietly as php's value does. */` |
|  178074 | 2027 | `		if( (iP2 == 0 \|\| iP2 == 1)` |
|  105793 | 2028 | `		 && (pIdx->iFlags & MEMOBJ_REAL)` |
|   89047 | 2029 | `		 && pIdx->rVal != (ph7_real)(sxi64)pIdx->rVal ){` |
|       - | 2030 | `			SyBlob sErrMsg;` |
|       6 | 2031 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       6 | 2032 | `			SyBlobAppend(&sErrMsg,"Cannot access offset of type float on array",` |
|       - | 2033 | `				sizeof("Cannot access offset of type float on array")-1);` |
|       6 | 2034 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|       6 | 2035 | `			PH7_MemObjRelease(pIdx);` |
|       6 | 2036 | `			PH7_MemObjRelease(pTos);` |
|       6 | 2037 | `			pTos->nIdx = SXU32_HIGH;` |
|       6 | 2038 | `			VM_EXIT_BREAK;` |
|       - | 2039 | `		}` |
|   89035 | 2040 | `	}` |
|  178167 | 2041 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|  178097 | 2042 | `		if( iP2 == 1 \|\| VM_IDX_IS_UNSET(iP2) ){` |
|       - | 2043 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|       - | 2044 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|       - | 2045 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|       - | 2046 | `			 * NOT separate — that would defeat COW on every element read.` |
|       - | 2047 | `			 * iP2=5 is unset-context, treated like iP2=1 for arrays so the` |
|       - | 2048 | `			 * trailing unset() builtin can drop the slot via pTos->nIdx. */` |
|    1631 | 2049 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|     813 | 2050 | `		}` |
|       - | 2051 | `		/* Point to the hashmap */` |
|  178097 | 2052 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|  178097 | 2053 | `		if( pIdx ){` |
|       - | 2054 | `			/* Load the desired entry */` |
|  178075 | 2055 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|   89035 | 2056 | `		}` |
|  178097 | 2057 | `		if( iP2 == 3 ){` |
|       - | 2058 | `			/* Null coalescing assign peek mode: separate only when we will` |
|       - | 2059 | `			 * actually write back. If the looked-up value is non-null, the` |
|       - | 2060 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|       - | 2061 | `			 * the parent can stay shared. If the value is null or the key is` |
|       - | 2062 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|       - | 2063 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|       - | 2064 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|       - | 2065 | `			 * correct for the outermost write. */` |
|      25 | 2066 | `			int needWrite = (rc != SXRET_OK);` |
|      25 | 2067 | `			if( !needWrite && pNode ){` |
|      13 | 2068 | `				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|      13 | 2069 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|       7 | 2070 | `					needWrite = 1;` |
|       3 | 2071 | `				}` |
|       6 | 2072 | `			}` |
|      25 | 2073 | `			if( needWrite ){` |
|      19 | 2074 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      19 | 2075 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|       - | 2076 | `					/* The map was actually copied — re-lookup so pNode points` |
|       - | 2077 | `					 * into the new map's storage. */` |
|       7 | 2078 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       7 | 2079 | `					if( pIdx ){` |
|       7 | 2080 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|       3 | 2081 | `					}` |
|       3 | 2082 | `				}` |
|       9 | 2083 | `			}` |
|      12 | 2084 | `		}` |
|       - | 2085 | `		/* iP2 == 5 (unset) is deliberately NOT here: php's unset() never creates` |
|       - | 2086 | `		 * the key it is about to remove, so a MISS must stay a miss. The` |
|       - | 2087 | `		 * insert-then-unset round trip was invisible on the LAST step but left` |
|       - | 2088 | ``		 * every INTERMEDIATE behind -- `unset($a["y"]["z"])` grew an empty`` |
|       - | 2089 | `		 * $a["y"]. The COW separation the unset context needs happened above and` |
|       - | 2090 | `		 * does not depend on this insert. */` |
|  178097 | 2091 | `		if( rc != SXRET_OK && (iP2 == 1 \|\| iP2 == 3) ){` |
|       - | 2092 | `			/* Create a new empty entry */` |
|     135 | 2093 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|     135 | 2094 | `			if( rc == SXRET_OK ){` |
|       - | 2095 | `				/* Point to the last inserted entry */` |
|     132 | 2096 | `				pNode = pMap->pLast;` |
|      68 | 2097 | `			}else{` |
|       - | 2098 | ``				/* An append lvalue (`$a[][...] = v`) whose saturated auto-index`` |
|       - | 2099 | `				 * is occupied threw php's catchable Error. Dispatch it here —` |
|       - | 2100 | `				 * falling through with a stale pMap->pLast is what silently` |
|       - | 2101 | `				 * overwrote $a[PHP_INT_MAX]. */` |
|       7 | 2102 | `				PH7_DISPATCH_ENFORCE_RC(rc)` |
|       - | 2103 | `			}` |
|      64 | 2104 | `		}` |
|   89045 | 2105 | `	}` |
|  178160 | 2106 | `	if( rc != SXRET_OK && pIdx && (iP2 == 2 \|\| iP2 == 0)` |
|   54797 | 2107 | `	 && (pTos->iFlags & MEMOBJ_HASHMAP)` |
|      40 | 2108 | `	 && !VmIdxFeedsCoalesce(pInstr) ){` |
|       - | 2109 | ``		/* `$a['k'] ?? $d` compiles its LHS as a plain read (iP2 == 0) followed`` |
|       - | 2110 | `		 * by NULLC/NULLC_JMP — php does NOT warn there, so peek ahead and stay` |
|       - | 2111 | `		 * silent (same guard the magic-accessor read path uses). */` |
|       - | 2112 | `		/* php warns when a missing key is READ (iP2 == 0) or destructured` |
|       - | 2113 | `		 * (iP2 == 2). isset/empty/??/unset (iP2 3-6) and write-context` |
|       - | 2114 | `		 * vivification (iP2 == 1) stay silent, as does a read on a non-array` |
|       - | 2115 | `		 * base (already diagnosed above). php prints an INT key bare and a` |
|       - | 2116 | `		 * STRING key quoted, and it decides which one the key IS by the same fold` |
|       - | 2117 | ``		 * the lookup just used: `$a["10"]` looked for the INTEGER key 10, so php`` |
|       - | 2118 | ``		 * says `Undefined array key 10`. PHL rendered the operand as WRITTEN --`` |
|       - | 2119 | ``		 * quoting every string and, worse, printing `$a[false]` as the "" key it`` |
|       - | 2120 | `		 * never looked in (false is the integer key 0). The canonical-numeric rule` |
|       - | 2121 | `		 * is the hashmap's own, so ask it rather than re-derive it. */` |
|       - | 2122 | `		SyBlob sMsg;` |
|      75 | 2123 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      75 | 2124 | `		if( PH7_HashmapKeyIsInt(pIdx) ){` |
|      24 | 2125 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      15 | 2126 | `				PH7_MemObjToInteger(pIdx);` |
|       7 | 2127 | `			}` |
|      24 | 2128 | `			SyBlobFormat(&sMsg,"Undefined array key %qd",pIdx->x.iVal);` |
|      13 | 2129 | `		}else{` |
|       - | 2130 | `			/* Not an integer key: PH7_HashmapKeyIsInt left it a printable string. */` |
|       - | 2131 | `			SyString sKey;` |
|      53 | 2132 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob));` |
|      53 | 2133 | `			SyBlobFormat(&sMsg,"Undefined array key \"%z\"",&sKey);` |
|       - | 2134 | `		}` |
|      75 | 2135 | `		SyBlobNullAppend(&sMsg);` |
|      75 | 2136 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|      75 | 2137 | `		SyBlobRelease(&sMsg);` |
|      35 | 2138 | `	}` |
|  178116 | 2139 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0` |
|   89071 | 2140 | `	 && (iP2 == 0 \|\| iP2 == 2)` |
|      22 | 2141 | `	 && !VmIdxFeedsCoalesce(pInstr) ){` |
|       - | 2142 | `		/* Subscripting a scalar base is a WARNING in php ("Trying to access array offset` |
|       - | 2143 | `		 * on int") that yields NULL. PH7 yielded NULL in silence. */` |
|      14 | 2144 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Trying to access array offset on %s",` |
|       4 | 2145 | `			VmArithValueName(pTos));` |
|       4 | 2146 | `	}` |
|  178116 | 2147 | `	if( iP2 == VM_IDX_CTX_UNSET && rc == SXRET_OK && pNode != 0` |
|     919 | 2148 | `	 && (pTos->iFlags & MEMOBJ_HASHMAP) ){` |
|       - | 2149 | ``		/* php's `unset($a[k])` removes the ELEMENT. PH7 left the element's value slot`` |
|       - | 2150 | `		 * on the stack and let the trailing unset() builtin drop it — but dropping a` |
|       - | 2151 | `` 		 * SLOT unlinks everything that holds it, so `$r = &$a['k']; unset($a['k']);` `` |
|       - | 2152 | ``		 * destroyed $r too (`Undefined variable $r`) where php leaves it reading the`` |
|       - | 2153 | `		 * value it still refers to. Unlink the node itself, which releases the value` |
|       - | 2154 | `		 * only when this element was its last holder, and leave the builtin nothing. */` |
|     918 | 2155 | `		ph7_hashmap *pTarget = (ph7_hashmap *)pTos->x.pOther;` |
|     918 | 2156 | `		int bDone = 0;` |
|     918 | 2157 | `		if( pTarget == pVm->pGlobal && pIdx ){` |
|       - | 2158 | ``			/* `$GLOBALS['x']` IS the global $x, so this is an unset of the NAME: it has`` |
|       - | 2159 | `			 * to drop the symbol-table entry as well as this node, and it must not` |
|       - | 2160 | `			 * destroy the value another holder still refers to — exactly what` |
|       - | 2161 | ``			 * VmUnsetVarByName does for `unset($x)`. A superglobal has no entry in the`` |
|       - | 2162 | `			 * global frame and falls through to the plain node unlink below. */` |
|     159 | 2163 | `			VmFrame *pGlobalFrame = pVm->pFrame;` |
|       - | 2164 | `			SyHashEntry *pNameEntry;` |
|     159 | 2165 | `			while( pGlobalFrame->pParent ){` |
|     ! 0 | 2166 | `				pGlobalFrame = pGlobalFrame->pParent;` |
|     ! 0 | 2167 | `			}` |
|     159 | 2168 | `			if( (pIdx->iFlags & MEMOBJ_STRING) == 0 ){` |
|       3 | 2169 | `				PH7_MemObjToString(pIdx);` |
|       1 | 2170 | `			}` |
|     238 | 2171 | `			pNameEntry = SyHashGet(&pGlobalFrame->hVar,` |
|     158 | 2172 | `				(const void *)SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob));` |
|     159 | 2173 | `			if( pNameEntry ){` |
|     238 | 2174 | `				sxi32 rcUnset = VmUnsetVarByNameEx(&(*pVm),pGlobalFrame,` |
|     158 | 2175 | `					(const char *)SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob),FALSE);` |
|     159 | 2176 | `				bDone = 1;` |
|     159 | 2177 | `				if( rcUnset == PH7_ABORT ){` |
|     ! 0 | 2178 | `					PH7_MemObjRelease(pIdx);` |
|     ! 0 | 2179 | `					VM_EXIT_ABORT;` |
|       - | 2180 | `				}` |
|      79 | 2181 | `			}` |
|      79 | 2182 | `		}` |
|     918 | 2183 | `		if( !bDone ){` |
|     760 | 2184 | `			PH7_HashmapUnlinkNode(pNode,TRUE);` |
|     378 | 2185 | `		}` |
|     918 | 2186 | `		if( pIdx ){` |
|     918 | 2187 | `			PH7_MemObjRelease(pIdx);` |
|     457 | 2188 | `		}` |
|     918 | 2189 | `		PH7_MemObjRelease(pTos);` |
|     918 | 2190 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|     918 | 2191 | `		pTos->nIdx = SXU32_HIGH;` |
|     918 | 2192 | `		VM_EXIT_BREAK;` |
|       - | 2193 | `	}` |
|  177207 | 2194 | `	if( pIdx ){` |
|  177187 | 2195 | `		PH7_MemObjRelease(pIdx);` |
|   88591 | 2196 | `	}` |
|  177207 | 2197 | `	if( rc == SXRET_OK ){` |
|       - | 2198 | `		/* Load entry contents */` |
|   67691 | 2199 | `		if( pMap->iRef < 2 ){` |
|       - | 2200 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|       - | 2201 | `			 * of the entry value,rather than pointing to it.` |
|       - | 2202 | `			 */` |
|     445 | 2203 | `			pTos->nIdx = SXU32_HIGH;` |
|     445 | 2204 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|     225 | 2205 | `		}else{` |
|   67251 | 2206 | `			pTos->nIdx = pNode->nValIdx;` |
|   67251 | 2207 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|   67251 | 2208 | `			PH7_HashmapUnref(pMap);` |
|       - | 2209 | `		}` |
|   33848 | 2210 | `	}else{` |
|       - | 2211 | `		/* No such entry,load NULL */` |
|  109521 | 2212 | `		PH7_MemObjRelease(pTos);` |
|  109521 | 2213 | `		pTos->nIdx = SXU32_HIGH;` |
|       - | 2214 | `	}` |
|  177207 | 2215 | `	if( iP2 == 4 && (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       - | 2216 | `		/* isset() context: reduce a found element to the same non-null marker the` |
|       - | 2217 | `		 * ArrayAccess arm above pushes. A TEMPORARY array (a call's return value,` |
|       - | 2218 | ``		 * or an accessor's -- `isset(f()['k'])`, `isset($o->magic['k'])`) leaves no`` |
|       - | 2219 | `		 * variable index behind, and the trailing builtin read that as a CONSTANT` |
|       - | 2220 | `		 * and warned about it; php's isset() is a language construct with no such` |
|       - | 2221 | `		 * diagnostic. */` |
|   33663 | 2222 | `		PH7_MemObjRelease(pTos);` |
|   33663 | 2223 | `		pTos->x.iVal = 1;` |
|   33663 | 2224 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   33663 | 2225 | `		pTos->nIdx = SXU32_HIGH;` |
|   16829 | 2226 | `	}` |
|  177207 | 2227 | `	VM_EXIT_BREAK;` |
|     ! 0 | 2228 | `	VM_EXIT_BREAK;` |
|  539792 | 2229 | `}` |
|       - | 2230 |  |
|       - | 2231 | `/*` |
|       - | 2232 | ` * OP_LOAD_MAP: body moved verbatim from the OP_LOAD_MAP arm of` |
|       - | 2233 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 2234 | ` */` |
|  104476 | 2235 | `PH7_PRIVATE VmOpRc VmExecOpLoadMap(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 2236 | `{` |
|  104481 | 2237 | `	ph7_value *pTos = pState->pTos;` |
|  104481 | 2238 | `	ph7_value *pStack = pState->pStack;` |
|  104481 | 2239 | `	VmInstr *aInstr = pState->aInstr;` |
|  104481 | 2240 | `	sxi32 pc = pState->pc;` |
|       - | 2241 | `	sxi32 rc;` |
|   52237 | 2242 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - | 2243 | `	ph7_hashmap *pMap;` |
|       - | 2244 | `	/* Allocate a new hashmap instance */` |
|  104481 | 2245 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|  104481 | 2246 | `	if( pMap == 0 ){` |
|     ! 0 | 2247 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|     ! 0 | 2248 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|     ! 0 | 2249 | `		VM_EXIT_ABORT;` |
|       - | 2250 | `	}` |
|  104481 | 2251 | `	if( pInstr->iP1 > 0 ){` |
|   21579 | 2252 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|   21579 | 2253 | `		sxi32 rcSpread = SXRET_OK;` |
|       - | 2254 | `		/* Perform the insertion */` |
|   74447 | 2255 | `		while( pEntry < pTos ){` |
|   52891 | 2256 | `			if( pEntry[1].iFlags & MEMOBJ_AUX_SPREAD ){` |
|       - | 2257 | `				/* Array unpacking: '...$expr'. Merge entries with PHP 8.1` |
|       - | 2258 | `				 * semantics — string keys preserved (later wins), int keys` |
|       - | 2259 | `				 * renumbered. Same routine that backs array_merge. */` |
|     687 | 2260 | `				if( pEntry[1].iFlags & MEMOBJ_HASHMAP ){` |
|     664 | 2261 | `					sxi32 rcMerge = PH7_HashmapMerge((ph7_hashmap *)pEntry[1].x.pOther,pMap);` |
|     664 | 2262 | `					if( rcMerge != SXRET_OK ){` |
|       - | 2263 | `						/* Merge failure (OOM): match the PH7_NewHashmap OOM` |
|       - | 2264 | `						 * path — emit fatal and abort, leaving no partial` |
|       - | 2265 | `						 * map dangling. */` |
|     ! 0 | 2266 | `						VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|     ! 0 | 2267 | `							"Fatal, PH7 engine is running out of memory while spreading array at instruction #:%d",pc);` |
|     ! 0 | 2268 | `						rcSpread = PH7_ABORT;` |
|     ! 0 | 2269 | `						break;` |
|       2 | 2270 | `					}` |
|     355 | 2271 | `				}else if( VmValueIsTraversable(pVm,&pEntry[1]) ){` |
|       - | 2272 | `					/* Traversable unpacking (PHP 8.1): walk it into the map using the` |
|       - | 2273 | `					 * same key rules as array spread (string keys kept, int renumbered). */` |
|       5 | 2274 | `					sxi32 rcW = PH7_VmIteratorWalk(&(*pVm),&pEntry[1],VmSpreadMergeStep,pMap);` |
|       5 | 2275 | `					if( rcW == PH7_EXCEPTION \|\| rcW == PH7_ABORT ){` |
|     ! 0 | 2276 | `						rcSpread = rcW;` |
|     ! 0 | 2277 | `						break;` |
|       - | 2278 | `					}` |
|       3 | 2279 | `				}else{` |
|       - | 2280 | `					/* Throw a catchable Error matching PHP semantics. */` |
|      20 | 2281 | `					rcSpread = VmThrowSpreadError(&(*pVm),&pEntry[1]);` |
|      20 | 2282 | `					break;` |
|       2 | 2283 | `				}` |
|   52540 | 2284 | `			}else if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|       - | 2285 | `				/* Insertion by reference */` |
|     244 | 2286 | `				PH7_HashmapInsertByRef(pMap,` |
|     162 | 2287 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     162 | 2288 | `					(sxu32)pEntry[1].x.iVal` |
|       - | 2289 | `					);` |
|      82 | 2290 | `			}else{` |
|       - | 2291 | `				/* An explicit key in an array LITERAL gets the same php diagnostics a` |
|       - | 2292 | `				 * subscript does — a float key that truncates deprecates, and so does an` |
|       - | 2293 | `				 * explicit null key. Only the subscript sites (LOAD_IDX/STORE_IDX) used to` |
|       - | 2294 | ``				 * emit these, so `[1.5 => "v"]` and `[null => "v"]` were silent. A key that`` |
|       - | 2295 | `				 * is ABSENT (auto-index) is a NULL slot here, not a null key, so the` |
|       - | 2296 | `				 * MEMOBJ_NULL check below is what tells the two apart. */` |
|   52045 | 2297 | `					if( (pEntry->iFlags & MEMOBJ_AUX_NOKEY) == 0 ){` |
|       - | 2298 | `						/* An object/array literal key is php's TypeError, a resource one` |
|       - | 2299 | `						 * warns and becomes its id — same rules as a subscript. */` |
|       - | 2300 | `						SyBlob sTypeMsg;` |
|   14713 | 2301 | `						if( VmOffsetTypeRejected(&(*pVm),pEntry,0,&sTypeMsg) ){` |
|       3 | 2302 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg));` |
|       2 | 2303 | `						}else{` |
|   14711 | 2304 | `							VmOffsetResourceWarn(&(*pVm),pEntry);` |
|       - | 2305 | `						}` |
|       - | 2306 | `						/* php DEPRECATES a null literal key (then normalizes to "") and` |
|       - | 2307 | `						 * rejects nothing there; PHL matches that (deprecate + fall` |
|       - | 2308 | `						 * through, PH7_HashmapInsert casts NULL->""). A lossy-FLOAT` |
|       - | 2309 | `						 * literal key still rejects with a TypeError — the recorded` |
|       - | 2310 | `						 * non-deprecated-surface policy, same as the subscript site. */` |
|   14713 | 2311 | `						int bNull = (pEntry->iFlags & MEMOBJ_NULL) != 0;` |
|   22068 | 2312 | `						int bLossyFloat = (pEntry->iFlags & MEMOBJ_REAL) != 0` |
|   14708 | 2313 | `							&& pEntry->rVal != (ph7_real)(sxi64)pEntry->rVal;` |
|   14713 | 2314 | `						if( bNull ){` |
|       3 | 2315 | `							VmNullOffsetDeprecate(&(*pVm),pEntry);` |
|   14712 | 2316 | `						}else if( bLossyFloat ){` |
|       3 | 2317 | `							const char *zErr = "Cannot access offset of type float on array";` |
|       - | 2318 | `							SyBlob sErrMsg;` |
|       3 | 2319 | `							SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       3 | 2320 | `							SyBlobAppend(&sErrMsg,zErr,(sxu32)SyStrlen(zErr));` |
|       3 | 2321 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|       1 | 2322 | `						}` |
|    7354 | 2323 | `					}` |
|       - | 2324 | `				/* Standard insertion */` |
|   78065 | 2325 | `				PH7_HashmapInsert(pMap,` |
|   52040 | 2326 | `					(pEntry->iFlags & MEMOBJ_AUX_NOKEY) ? 0 /* Automatic index assign */ : pEntry,` |
|   26020 | 2327 | `					&pEntry[1]` |
|       - | 2328 | `				);` |
|       - | 2329 | `			}` |
|       - | 2330 | `			/* Next pair on the stack */` |
|   52873 | 2331 | `			pEntry += 2;` |
|       5 | 2332 | `		}` |
|       - | 2333 | `		/* Pop P1 elements */` |
|   21579 | 2334 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|   21579 | 2335 | `		if( rcSpread != SXRET_OK ){` |
|       - | 2336 | `			/* Discard the partially-built map and propagate the exception. */` |
|      20 | 2337 | `			PH7_HashmapRelease(pMap,TRUE);` |
|      20 | 2338 | `			if( rcSpread == PH7_ABORT ){` |
|     ! 0 | 2339 | `				VM_EXIT_ABORT;` |
|       - | 2340 | `			}` |
|       - | 2341 | `			{` |
|       - | 2342 | `				sxi32 iRp;` |
|      20 | 2343 | `				if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|       6 | 2344 | `					pc = iRp;` |
|       6 | 2345 | `					VM_EXIT_BREAK;` |
|       - | 2346 | `				}` |
|       - | 2347 | `			}` |
|      15 | 2348 | `			VM_EXIT_EXCEPTION;` |
|       - | 2349 | `		}` |
|   10778 | 2350 | `	}` |
|       - | 2351 | `	/* Push the hashmap */` |
|  104463 | 2352 | `	pTos++;` |
|  104463 | 2353 | `	pTos->nIdx = SXU32_HIGH;` |
|  104463 | 2354 | `	pTos->x.pOther = pMap;` |
|  104463 | 2355 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|  104463 | 2356 | `	VM_EXIT_BREAK;` |
|     ! 0 | 2357 | `	VM_EXIT_BREAK;` |
|   52242 | 2358 | `}` |
|       - | 2359 |  |
|       - | 2360 | `/*` |
|       - | 2361 | `` * Can `$o[$k]` be READ at all? php's read_dimension is either the class's own`` |
|       - | 2362 | ` * native handler -- which a class may carry WITHOUT implementing ArrayAccess,` |
|       - | 2363 | ` * php's DOMNodeList -- or the standard one, which needs the interface. Neither` |
|       - | 2364 | `` * is php's `Cannot use object of type C as array`.`` |
|       - | 2365 | ` */` |
|      28 | 2366 | `static int VmObjectDimReadable(ph7_vm *pVm,ph7_class_instance *pInst)` |
|       1 | 2367 | `{` |
|      29 | 2368 | `	if( pInst == 0 ){` |
|     ! 0 | 2369 | `		return 0;` |
|       - | 2370 | `	}` |
|      42 | 2371 | `	return PH7_ClassHasNativeDim(pInst->pClass)` |
|      28 | 2372 | `	    \|\| (pVm->pArrayAccessClass && PH7_VmInstanceOf(pInst->pClass,pVm->pArrayAccessClass));` |
|      15 | 2373 | `}` |
|       - | 2374 | `/*` |
|       - | 2375 | ` * One such READ, into pOut (which the caller inits and owns). The native` |
|       - | 2376 | ` * handler comes first for the same reason it does at the subscript opcode: php` |
|       - | 2377 | ` * implements the interface THROUGH the handler. A refusal is dropped here --` |
|       - | 2378 | ` * the only caller indexes 0..N-1 of its own target list, which no handler` |
|       - | 2379 | ` * refuses -- and pOut is simply left as it was.` |
|       - | 2380 | ` *` |
|       - | 2381 | ` * Answers the accessor's own status so a caller reading a RUN of positions can` |
|       - | 2382 | ` * stop where php stops: a userland offsetGet that THROWS abandons the rest of` |
|       - | 2383 | ` * the destructure, leaving every later target at its previous value.` |
|       - | 2384 | ` */` |
|      36 | 2385 | `static sxi32 VmObjectDimRead(ph7_vm *pVm,ph7_class_instance *pInst,ph7_value *pKey,ph7_value *pOut)` |
|       1 | 2386 | `{` |
|       - | 2387 | `	ph7_class_method *pGet;` |
|       - | 2388 | `	sxi32 rcCall;` |
|      37 | 2389 | `	if( PH7_ClassHasNativeDim(pInst->pClass) ){` |
|       - | 2390 | `		PH7_NativeDimCtx sDim;` |
|       5 | 2391 | `		sDim.iMode = PH7_NATIVE_DIM_READ;` |
|       5 | 2392 | `		sDim.pOffset = pKey;` |
|       5 | 2393 | `		sDim.pResult = pOut;` |
|       5 | 2394 | `		sDim.zThrowClass = 0;` |
|       5 | 2395 | `		sDim.zThrowMsg[0] = 0;` |
|       5 | 2396 | `		PH7_ClassNativeDim(pInst,&sDim);` |
|       5 | 2397 | `		return SXRET_OK;` |
|       - | 2398 | `	}` |
|      33 | 2399 | `	pGet = PH7_ClassExtractMethod(pInst->pClass,"offsetGet",sizeof("offsetGet")-1);` |
|      33 | 2400 | `	if( pGet == 0 ){` |
|     ! 0 | 2401 | `		return SXRET_OK;` |
|       - | 2402 | `	}` |
|       - | 2403 | `	{` |
|       - | 2404 | `		ph7_value *apArg[1];` |
|      33 | 2405 | `		apArg[0] = pKey;` |
|      33 | 2406 | `		rcCall = PH7_VmCallClassMethod(&(*pVm),pInst,pGet,pOut,1,apArg);` |
|       - | 2407 | `	}` |
|      33 | 2408 | `	return (rcCall == PH7_EXCEPTION \|\| pVm->nBoundaryRc != 0) ? PH7_EXCEPTION : SXRET_OK;` |
|      19 | 2409 | `}` |
|       - | 2410 | `/*` |
|       - | 2411 | ` * OP_LOAD_LIST: body moved verbatim from the OP_LOAD_LIST arm of` |
|       - | 2412 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 2413 | ` */` |
|    1456 | 2414 | `PH7_PRIVATE VmOpRc VmExecOpLoadList(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 2415 | `{` |
|    1461 | 2416 | `	ph7_value *pTos = pState->pTos;` |
|    1461 | 2417 | `	ph7_value *pStack = pState->pStack;` |
|    1461 | 2418 | `	VmInstr *aInstr = pState->aInstr;` |
|    1461 | 2419 | `	sxi32 pc = pState->pc;` |
|       - | 2420 | `	sxi32 rc;` |
|     728 | 2421 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - | 2422 | `	ph7_value *pEntry;` |
|    1461 | 2423 | `	sxi32 rcEnforce = SXRET_OK;` |
|    1461 | 2424 | `	if( pInstr->iP1 <= 0 ){` |
|       - | 2425 | `		/* Empty list,break immediately */` |
|     ! 0 | 2426 | `		VM_EXIT_BREAK;` |
|       - | 2427 | `	}` |
|    1461 | 2428 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|       - | 2429 | `#ifdef UNTRUST` |
|       - | 2430 | `	if( &pEntry[-1] < pStack ){` |
|       - | 2431 | `		VM_EXIT_ABORT;` |
|       - | 2432 | `	}` |
|       - | 2433 | `#endif` |
|    1461 | 2434 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|    1417 | 2435 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|       - | 2436 | `		ph7_hashmap_node *pNode;` |
|       - | 2437 | `		ph7_value sKey,*pObj;` |
|       - | 2438 | `		/* Start Copying */` |
|    1417 | 2439 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|    4307 | 2440 | `		while( pEntry <= pTos ){` |
|    2911 | 2441 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|    2881 | 2442 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|    2881 | 2443 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|    5607 | 2444 | `					int bTyped = SyHashTotalEntry(&pVm->hTypedSlot) > 0` |
|    2876 | 2445 | `						&& SyHashGet(&pVm->hTypedSlot,(const void *)&pEntry->nIdx,sizeof(sxu32)) != 0;` |
|    2881 | 2446 | `					if( rc != SXRET_OK ){` |
|       - | 2447 | `						/* Undefined array key */` |
|       - | 2448 | `						char zMsg[128];` |
|       5 | 2449 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|       5 | 2450 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|       2 | 2451 | `					}` |
|    2881 | 2452 | `					if( !bTyped ){` |
|    2847 | 2453 | `						if( rc == SXRET_OK ){` |
|       - | 2454 | `							/* Store node value */` |
|    2847 | 2455 | `							PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|    1426 | 2456 | `						}else{` |
|     ! 0 | 2457 | `							PH7_MemObjRelease(pObj);` |
|       - | 2458 | `						}` |
|    1426 | 2459 | `					}else{` |
|       - | 2460 | ``						/* Typed/readonly property target (`[$o->p] = [...]`): a`` |
|       - | 2461 | `						 * direct slot write would bypass the typed-slot table, so` |
|       - | 2462 | `						 * enforce on a temp first — a TypeError leaves the property` |
|       - | 2463 | `						 * untouched, and a missing key assigns null, which a` |
|       - | 2464 | `						 * non-nullable type rejects exactly like php (warning, then` |
|       - | 2465 | `						 * "Cannot assign null to property ... of type ..."). */` |
|       - | 2466 | `						ph7_value sVal;` |
|      36 | 2467 | `						PH7_MemObjInit(&(*pVm),&sVal);` |
|      36 | 2468 | `						if( rc == SXRET_OK ){` |
|      32 | 2469 | `							PH7_HashmapExtractNodeValue(pNode,&sVal,TRUE);` |
|      15 | 2470 | `						}` |
|      36 | 2471 | `						rcEnforce = VmEnforcePropertyTypeOnStore(&(*pVm),pEntry->nIdx,&sVal,0);` |
|      36 | 2472 | `						if( rcEnforce != SXRET_OK ){` |
|       - | 2473 | `							/* Thrown: stop assigning (php aborts the list at the` |
|       - | 2474 | `							 * first failing element), settle the stack, route. */` |
|      17 | 2475 | `							PH7_MemObjRelease(&sVal);` |
|      17 | 2476 | `							break;` |
|       - | 2477 | `						}` |
|      20 | 2478 | `						PH7_MemObjStore(&sVal,pObj);` |
|      20 | 2479 | `						PH7_MemObjRelease(&sVal);` |
|       - | 2480 | `					}` |
|    1430 | 2481 | `				}` |
|    1430 | 2482 | `			}` |
|    2895 | 2483 | `			sKey.x.iVal++; /* Next numeric index */` |
|    2895 | 2484 | `			pEntry++;` |
|       5 | 2485 | `		}` |
|     764 | 2486 | `	}else if( (pEntry[-1].iFlags & MEMOBJ_OBJ) && pEntry[-1].x.pOther ){` |
|       - | 2487 | `		/* php destructures an OBJECT through its read_dimension handler, one` |
|       - | 2488 | ``		 * READ per POSITION -- `[$a, , $c] = $o` asks for 0 and 2 and never 1 --`` |
|       - | 2489 | `		 * so an ArrayObject, an SplFixedArray and (since the handler landed) a` |
|       - | 2490 | `		 * DOMNodeList all come apart the way an array does. PHL treated every` |
|       - | 2491 | `` 		 * object as a non-array source: it warned `Cannot use object as array` `` |
|       - | 2492 | ``		 * and assigned NULL to every target, so `[$first, $second] = $list` --`` |
|       - | 2493 | `		 * the shape every modern DOM and SPL example is written in -- silently` |
|       - | 2494 | `		 * produced two nulls. An object with NO dimension reader is php's` |
|       - | 2495 | `		 * catchable Error rather than that warning, and it is raised before any` |
|       - | 2496 | ``		 * target is touched. (The KEYED spelling `['k' => $a] = $o` never came`` |
|       - | 2497 | `		 * here: the compiler routes it through OP_LOAD_IDX, which has had the` |
|       - | 2498 | `		 * accessor dispatch all along.) */` |
|      29 | 2499 | `		ph7_class_instance *pInst = (ph7_class_instance *)pEntry[-1].x.pOther;` |
|       - | 2500 | `		ph7_value sKey;` |
|      29 | 2501 | `		if( !VmObjectDimReadable(&(*pVm),pInst) ){` |
|       - | 2502 | `			/* Routed mid-expression, like every other catchable Error raised from` |
|       - | 2503 | `			 * an opcode that is not a call boundary: the destructure is abandoned` |
|       - | 2504 | `			 * and an enclosing try in THIS frame lands on its own handler. Settle` |
|       - | 2505 | `			 * the targets AND the source first — the statement's OP_POP is skipped` |
|       - | 2506 | `			 * when a catch resumes at the landing pad. */` |
|       - | 2507 | `			char zMsg[256];` |
|      11 | 2508 | `			SyString *pName = &pInst->pClass->sName;` |
|      16 | 2509 | `			sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - | 2510 | `				"Cannot use object of type %.*s as array",` |
|      10 | 2511 | `				(int)pName->nByte,pName->zString);` |
|      11 | 2512 | `			rc = VmThrowFromVm(&(*pVm),"Error",zMsg,nMsg);` |
|      11 | 2513 | `			VmPopOperand(&pTos,pInstr->iP1 + 1);` |
|      11 | 2514 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      11 | 2515 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|     ! 0 | 2516 | `		}else{` |
|      19 | 2517 | `			PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      59 | 2518 | `			while( pEntry <= pTos ){` |
|      43 | 2519 | `				if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */ ){` |
|      37 | 2520 | `					sxu32 nSlot = pEntry->nIdx;` |
|      73 | 2521 | `					int bTyped = SyHashTotalEntry(&pVm->hTypedSlot) > 0` |
|      36 | 2522 | `						&& SyHashGet(&pVm->hTypedSlot,(const void *)&nSlot,sizeof(sxu32)) != 0;` |
|       - | 2523 | `					ph7_value sVal,*pObj;` |
|      37 | 2524 | `					PH7_MemObjInit(&(*pVm),&sVal);` |
|      37 | 2525 | `					if( VmObjectDimRead(&(*pVm),pInst,&sKey,&sVal) != SXRET_OK ){` |
|       - | 2526 | `						/* The accessor threw: php abandons the destructure there,` |
|       - | 2527 | `						 * so every later target keeps the value it had -- and this` |
|       - | 2528 | `						 * target does too, since php assigns nothing for the read` |
|       - | 2529 | `						 * that failed. */` |
|       3 | 2530 | `						PH7_MemObjRelease(&sVal);` |
|       3 | 2531 | `						rcEnforce = PH7_EXCEPTION;` |
|       3 | 2532 | `						break;` |
|       - | 2533 | `					}` |
|      35 | 2534 | `					if( bTyped ){` |
|       - | 2535 | `						/* Same rule as the array source's typed target: enforce on` |
|       - | 2536 | `						 * the temp so a TypeError leaves the property untouched. */` |
|     ! 0 | 2537 | `						rcEnforce = VmEnforcePropertyTypeOnStore(&(*pVm),nSlot,&sVal,0);` |
|     ! 0 | 2538 | `						if( rcEnforce != SXRET_OK ){` |
|     ! 0 | 2539 | `							PH7_MemObjRelease(&sVal);` |
|     ! 0 | 2540 | `							break;` |
|       - | 2541 | `						}` |
|     ! 0 | 2542 | `					}` |
|       - | 2543 | `					/* Re-fetch AFTER the read: a userland offsetGet can reserve` |
|       - | 2544 | `					 * slots, and growing aMemObj relocates every pointer into it. */` |
|      35 | 2545 | `					pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nSlot);` |
|      35 | 2546 | `					if( pObj ){` |
|      35 | 2547 | `						PH7_MemObjStore(&sVal,pObj);` |
|      17 | 2548 | `					}` |
|      35 | 2549 | `					PH7_MemObjRelease(&sVal);` |
|      17 | 2550 | `				}` |
|      41 | 2551 | `				sKey.x.iVal++; /* Next numeric index */` |
|      41 | 2552 | `				pEntry++;` |
|       1 | 2553 | `			}` |
|      19 | 2554 | `			PH7_MemObjRelease(&sKey);` |
|       - | 2555 | `		}` |
|      10 | 2556 | `	}else{` |
|       - | 2557 | `		/* Source is not an array: php warns first (silencing ONLY null — a bool` |
|       - | 2558 | `		 * source warns too, php 8), then assigns null to every target. A typed` |
|       - | 2559 | `		 * property target receives that null THROUGH enforcement, so a` |
|       - | 2560 | `		 * non-nullable type throws "Cannot assign null to property ..." exactly` |
|       - | 2561 | `		 * like php instead of silently nulling the slot. PHL DIVERGENCE: bool` |
|       - | 2562 | `		 * FALSE stays silent (php warns) — it is the end-of-array sentinel of` |
|       - | 2563 | `` 		 * the documented each() extension's `while (list(..) = each($a))` `` |
|       - | 2564 | `		 * idiom, which would otherwise warn on every normal loop exit. */` |
|       - | 2565 | `		ph7_value *pObj;` |
|      30 | 2566 | `		int bFalseSrc = (pTos[-pInstr->iP1].iFlags & MEMOBJ_BOOL) != 0` |
|      16 | 2567 | `			&& pTos[-pInstr->iP1].x.iVal == 0;` |
|      19 | 2568 | `		if( (pTos[-pInstr->iP1].iFlags & MEMOBJ_NULL) == 0 && !bFalseSrc ){` |
|      12 | 2569 | `			VmWarnCannotUseAsArray(&(*pVm),pTos[-pInstr->iP1].iFlags);` |
|       5 | 2570 | `		}` |
|      33 | 2571 | `		while( pEntry <= pTos ){` |
|      23 | 2572 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|      23 | 2573 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|      42 | 2574 | `					int bTyped = SyHashTotalEntry(&pVm->hTypedSlot) > 0` |
|      20 | 2575 | `						&& SyHashGet(&pVm->hTypedSlot,(const void *)&pEntry->nIdx,sizeof(sxu32)) != 0;` |
|      23 | 2576 | `					if( !bTyped ){` |
|      15 | 2577 | `						PH7_MemObjRelease(pObj);` |
|       9 | 2578 | `					}else{` |
|       - | 2579 | `						ph7_value sVal;` |
|       9 | 2580 | `						PH7_MemObjInit(&(*pVm),&sVal);` |
|       9 | 2581 | `						rcEnforce = VmEnforcePropertyTypeOnStore(&(*pVm),pEntry->nIdx,&sVal,0);` |
|       9 | 2582 | `						if( rcEnforce != SXRET_OK ){` |
|       7 | 2583 | `							PH7_MemObjRelease(&sVal);` |
|       7 | 2584 | `							break;` |
|       - | 2585 | `						}` |
|       3 | 2586 | `						PH7_MemObjStore(&sVal,pObj);` |
|       3 | 2587 | `						PH7_MemObjRelease(&sVal);` |
|       - | 2588 | `					}` |
|       7 | 2589 | `				}` |
|       7 | 2590 | `			}` |
|      17 | 2591 | `			pEntry++;` |
|       3 | 2592 | `		}` |
|       - | 2593 | `	}` |
|    1451 | 2594 | `	if( rcEnforce != SXRET_OK ){` |
|       - | 2595 | `		/* Settle this op's own operands: the P1 entries AND the source value —` |
|       - | 2596 | `		 * its statement-level OP_POP is skipped when a catch resumes at the` |
|       - | 2597 | `		 * landing pad, so leaving it would leak one operand slot per caught` |
|       - | 2598 | `		 * throw. A NESTED destructure can still have the outer list's operands` |
|       - | 2599 | `		 * abandoned above the try's base, so on an in-place catch drain to the` |
|       - | 2600 | `		 * catching try's recorded depth (like the fetch-point router and the` |
|       - | 2601 | `		 * generator inject path), not just our own pops. */` |
|      26 | 2602 | `		VmPopOperand(&pTos,pInstr->iP1 + 1);` |
|      26 | 2603 | `		if( rcEnforce == PH7_ABORT ){` |
|     ! 0 | 2604 | `			VM_EXIT_ABORT;` |
|       - | 2605 | `		}` |
|       - | 2606 | `		{` |
|       - | 2607 | `			sxi32 _iRpL;` |
|      38 | 2608 | `			PH7_INLINE_RESUME_BREAK()` |
|      26 | 2609 | `			if( VmRecordedResume(pVm,&_iRpL,pState->pEntryFrame,aInstr) ){` |
|      28 | 2610 | `				PH7_RESUME_DRAIN()` |
|      26 | 2611 | `				pc = _iRpL;` |
|      26 | 2612 | `				VM_EXIT_BREAK;` |
|       - | 2613 | `			}` |
|       - | 2614 | `		}` |
|     ! 0 | 2615 | `		VM_EXIT_EXCEPTION;` |
|       - | 2616 | `	}` |
|    1427 | 2617 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|    1427 | 2618 | `	VM_EXIT_BREAK;` |
|     ! 0 | 2619 | `	VM_EXIT_BREAK;` |
|     733 | 2620 | `}` |
|       - | 2621 |  |
|       - | 2622 | `/*` |
|       - | 2623 | ` * OP_UNSET_VAR: body moved verbatim from the OP_UNSET_VAR arm of` |
|       - | 2624 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 2625 | ` */` |
|    7804 | 2626 | `PH7_PRIVATE VmOpRc VmExecOpUnsetVar(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 2627 | `{` |
|    7809 | 2628 | `	ph7_value *pTos = pState->pTos;` |
|    7809 | 2629 | `	ph7_value *pStack = pState->pStack;` |
|    7809 | 2630 | `	VmInstr *aInstr = pState->aInstr;` |
|    7809 | 2631 | `	sxi32 pc = pState->pc;` |
|       - | 2632 | `	sxi32 rc;` |
|    3902 | 2633 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - | 2634 | `	/* unset($name): p3 is the variable name. Drops the NAME only — see VmUnsetVarByName */` |
|    7809 | 2635 | `	SyString *pName = (SyString *)pInstr->p3;` |
|    7809 | 2636 | `	if( pName && pVm->pFrame ){` |
|       - | 2637 | `		/* Inside a try{} the VM pushes an EXCEPTION frame; variables live in the body` |
|       - | 2638 | `		 * frame below it, so skip past it exactly as every other variable path does.` |
|       - | 2639 | `		 * Without this, unset($x) inside a try silently found nothing and did nothing. */` |
|    7809 | 2640 | `		VmFrame *pVarFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|    7809 | 2641 | `		sxi32 rcU = VmUnsetVarByName(&(*pVm),pVarFrame,pName->zString,pName->nByte);` |
|    7809 | 2642 | `		if( rcU == PH7_ABORT ){` |
|       3 | 2643 | `			VM_EXIT_ABORT;` |
|       - | 2644 | `		}` |
|       - | 2645 | `		/* Releasing the last holder can run a __destruct(), and that destructor may` |
|       - | 2646 | `		 * throw. Such a throw is PARKED in nBoundaryRc by the boundary rail; consume it` |
|       - | 2647 | `		 * here and route it, or the catch runs and execution resumes inside the try` |
|       - | 2648 | `		 * ("resumed-dtor" instead of php's "caught-dtor"). */` |
|    7807 | 2649 | `		if( pVm->nBoundaryRc != 0 ){` |
|       3 | 2650 | `			rc = pVm->nBoundaryRc;` |
|       3 | 2651 | `			pVm->nBoundaryRc = 0;` |
|       3 | 2652 | `			if( rc == PH7_ABORT ){` |
|     ! 0 | 2653 | `				VM_EXIT_ABORT;` |
|       - | 2654 | `			}` |
|       3 | 2655 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2656 | `		}` |
|    3900 | 2657 | `	}` |
|    7805 | 2658 | `	VM_EXIT_BREAK;` |
|     ! 0 | 2659 | `	VM_EXIT_BREAK;` |
|    3907 | 2660 | `}` |
|       - | 2661 |  |
