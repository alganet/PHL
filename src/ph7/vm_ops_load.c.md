# src/ph7/vm_ops_load.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1202/1324 lines (90.79%)

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
|    3212 |   28 | `PH7_PRIVATE VmOpRc VmExecOpStoreRef(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |   29 | `{` |
|    3217 |   30 | `	ph7_value *pTos = pState->pTos;` |
|    3217 |   31 | `	ph7_value *pStack = pState->pStack;` |
|    3217 |   32 | `	VmInstr *aInstr = pState->aInstr;` |
|    3217 |   33 | `	sxi32 pc = pState->pc;` |
|       - |   34 | `	sxi32 rc;` |
|    1606 |   35 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    3217 |   36 | `	 SyString sName = { 0 , 0 };` |
|       - |   37 | `	 VmFrame *pFrameLocal;` |
|       - |   38 | `	SyHashEntry *pEntry;` |
|       - |   39 | `	sxu32 nIdx;` |
|       - |   40 | `#ifdef UNTRUST` |
|       - |   41 | `	if( pTos < pStack ){` |
|       - |   42 | `		VM_EXIT_ABORT;` |
|       - |   43 | `	}` |
|       - |   44 | `#endif` |
|    3217 |   45 | `	if( pInstr->iP2 == 1 ){` |
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
|      23 |   84 | `					if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|       5 |   85 | `						SyHashDeleteEntry(&pVm->hTypedSlot,(const void *)&nOldIdx,sizeof(sxu32),0);` |
|       2 |   86 | `					}` |
|      23 |   87 | `					PH7_VmUnsetMemObj(&(*pVm),nOldIdx,TRUE);` |
|      12 |   88 | `				}else{` |
|       - |   89 | `					/* Already bound elsewhere: give that slot its pin back, which` |
|       - |   90 | `					 * releases it when this property was its last holder. */` |
|       7 |   91 | `					VmUnpinMemObjSlot(&(*pVm),nOldIdx);` |
|       - |   92 | `				}` |
|      29 |   93 | `				pVmAttr->nIdx = nSrcIdx;` |
|      29 |   94 | `				pVmAttr->iState \|= VM_CLASS_ATTR_REFBOUND;` |
|      29 |   95 | `				pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      29 |   96 | `				VmPinMemObjSlotCounted(&(*pVm),nSrcIdx);` |
|      15 |   97 | `			}` |
|      25 |   98 | `		}else if( pStAttr ){` |
|      11 |   99 | `			sxu32 nOldIdx = pStAttr->nIdx;` |
|      11 |  100 | `			if( nOldIdx != nSrcIdx ){` |
|       - |  101 | `				/* Give the previous target back, exactly as the instance arm above does.` |
|       - |  102 | `				 * A permanent pin was left on every slot the property had ever named, so` |
|       - |  103 | `				 * each of them stayed a REFERENCE for the rest of the script — which an` |
|       - |  104 | ``				 * ordinary array COPY then shared (`$d = $a; $d[0] = 99;` wrote through`` |
|       - |  105 | ``				 * to `$a[0]`, silently), since "is this element a reference" is answered`` |
|       - |  106 | `				 * by who still holds it. */` |
|      11 |  107 | `				if( (pStAttr->iFlags & PH7_CLASS_ATTR_REFBOUND) == 0 ){` |
|       - |  108 | `					/* The static's own (unshared) slot. A reference-bound property bypasses` |
|       - |  109 | `					 * typed coercion in php, so drop any typed-slot enforcement entry too. */` |
|       7 |  110 | `					if( pStAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|     ! 0 |  111 | `						SyHashDeleteEntry(&pVm->hTypedSlot,(const void *)&nOldIdx,sizeof(sxu32),0);` |
|     ! 0 |  112 | `					}` |
|       7 |  113 | `					PH7_VmUnsetMemObj(&(*pVm),nOldIdx,TRUE);` |
|       4 |  114 | `				}else{` |
|       5 |  115 | `					VmUnpinMemObjSlot(&(*pVm),nOldIdx);` |
|       - |  116 | `				}` |
|      11 |  117 | `				pStAttr->nIdx = nSrcIdx;` |
|      11 |  118 | `				pStAttr->iFlags \|= PH7_CLASS_ATTR_REFBOUND;` |
|      11 |  119 | `				VmPinMemObjSlotCounted(&(*pVm),nSrcIdx);` |
|       5 |  120 | `			}` |
|       5 |  121 | `		}` |
|      39 |  122 | `		if( pVm->pRefTargetThis ){` |
|      29 |  123 | `			PH7_ClassInstanceUnref(pVm->pRefTargetThis);` |
|      14 |  124 | `		}` |
|      39 |  125 | `		pVm->pRefTargetAttr = 0;` |
|      39 |  126 | `		pVm->pRefTargetStaticAttr = 0;` |
|      39 |  127 | `		pVm->pRefTargetThis = 0;` |
|       - |  128 | `		/* Pop the member-result; leave the source as the expression value. */` |
|      39 |  129 | `		VmPopOperand(&pTos,1);` |
|      39 |  130 | `		VM_EXIT_BREAK;` |
|       - |  131 | `	}` |
|    3175 |  132 | `	if( pInstr->p3 == 0 ){` |
|       - |  133 | `		char *zName;` |
|       - |  134 | `		/* Take the variable name from the Next on the stack */` |
|     ! 0 |  135 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  136 | `			/* Force a string cast */` |
|     ! 0 |  137 | `			PH7_MemObjToString(pTos);` |
|     ! 0 |  138 | `		}` |
|     ! 0 |  139 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|     ! 0 |  140 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|     ! 0 |  141 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     ! 0 |  142 | `			if( zName ){` |
|     ! 0 |  143 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|     ! 0 |  144 | `			}` |
|     ! 0 |  145 | `		}` |
|     ! 0 |  146 | `		PH7_MemObjRelease(pTos);` |
|     ! 0 |  147 | `		pTos--;` |
|     ! 0 |  148 | `	}else{` |
|    3175 |  149 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|       - |  150 | `	}` |
|    3175 |  151 | `	if( pTos->iFlags & MEMOBJ_AUX_STROFFSET ){` |
|       - |  152 | `		/* php: a string OFFSET cannot be either end of a reference. The value read` |
|       - |  153 | `		 * out of a string still carries the BASE VARIABLE's slot, so binding it` |
|       - |  154 | `		 * aliased the whole string and a later write through the reference REPLACED` |
|       - |  155 | `		 * it ($s = "abc"; $r = &$s[1]; $r = "Z"; left $s === "Z"). Same Error the` |
|       - |  156 | `		 * by-ref ARGUMENT path raises for f($s[1]). */` |
|       7 |  157 | `		rc = VmThrowFromVm(&(*pVm),"Error","Cannot create references to/from string offsets",` |
|       - |  158 | `			sizeof("Cannot create references to/from string offsets")-1);` |
|       7 |  159 | `		PH7_MemObjRelease(pTos);` |
|       7 |  160 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|       7 |  161 | `		pTos->nIdx = SXU32_HIGH;` |
|       7 |  162 | `		if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       9 |  163 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  164 | `	}` |
|    3169 |  165 | `	nIdx = pTos->nIdx;` |
|    3169 |  166 | `	if(nIdx == SXU32_HIGH ){` |
|       5 |  167 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|     ! 0 |  168 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - |  169 | `				"Reference operator require a variable not a constant as it's right operand");` |
|     ! 0 |  170 | `		}else{` |
|       - |  171 | `			ph7_value *pObj;` |
|       - |  172 | `			/* Extract the desired variable and if not available dynamically create it */` |
|       5 |  173 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|       5 |  174 | `			if( pObj == 0 ){` |
|     ! 0 |  175 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - |  176 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|     ! 0 |  177 | `				VM_EXIT_ABORT;` |
|       - |  178 | `			}` |
|       - |  179 | `			/* Perform the store operation */` |
|       5 |  180 | `			PH7_MemObjStore(pTos,pObj);` |
|       5 |  181 | `			pTos->nIdx = pObj->nIdx;` |
|       1 |  182 | `		}` |
|    3167 |  183 | `	}else if( sName.nByte > 0){` |
|    3165 |  184 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|       - |  185 | `			/* php 8.1's non-catchable fatal (compile-time there) */` |
|       3 |  186 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot acquire reference to $GLOBALS");` |
|       3 |  187 | `			pVm->iExitStatus = 255;` |
|       3 |  188 | `			pVm->bHaltRequested = 1;` |
|       3 |  189 | `			VM_EXIT_ABORT;` |
|     ! 0 |  190 | `		}else{` |
|    3162 |  191 | `			pFrameLocal = pVm->pFrame;` |
|    3162 |  192 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       - |  193 | `			/* Query the local frame */` |
|    3162 |  194 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|    3162 |  195 | `			if( pEntry ){` |
|       - |  196 | ``				/* php RE-BINDS a name that already exists (`$y = 2; $y = &$x;`, and the`` |
|       - |  197 | ``				 * `$r = &$a[$k]` idiom from the second loop step on) — the old binding`` |
|       - |  198 | `				 * goes, its value with it if nothing else holds it. */` |
|    3083 |  199 | `				PH7_VmRebindVarSlot(&(*pVm),pFrameLocal,pEntry,sName.zString,sName.nByte,nIdx);` |
|    3083 |  200 | `				if( pInstr->p3 == 0 && sName.zString ){` |
|       - |  201 | `					/* The name was duplicated for a symbol-table key this rebind does` |
|       - |  202 | `					 * not need — the entry keeps the key it was created with. */` |
|     ! 0 |  203 | `					SyMemBackendFree(&pVm->sAllocator,(void *)sName.zString);` |
|     ! 0 |  204 | `				}` |
|    1542 |  205 | `			}else{` |
|      80 |  206 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|      80 |  207 | `				if( pFrameLocal->pParent == 0 ){` |
|       - |  208 | `					/* Insert in the $GLOBALS array */` |
|      64 |  209 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|      30 |  210 | `				}` |
|      80 |  211 | `				if( rc == SXRET_OK ){` |
|      80 |  212 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|      38 |  213 | `				}` |
|       - |  214 | `			}` |
|       - |  215 | `		}` |
|    1579 |  216 | `	}` |
|    3166 |  217 | `	VM_EXIT_BREAK;` |
|     ! 0 |  218 | `	VM_EXIT_BREAK;` |
|    1611 |  219 | `}` |
|       - |  220 |  |
|       - |  221 | `/* LOAD_IDX's own iP2 context codes (they do NOT line up with PH7_MEMBER_*). */` |
|       - |  222 | `#define VM_IDX_CTX_ISSET 4` |
|       - |  223 | `#define VM_IDX_CTX_UNSET 5` |
|       - |  224 | ``/* An INTERMEDIATE subscript of an unset chain (`unset($a['k']['n'])`): every unset`` |
|       - |  225 | ` * rule below applies to it — COW-separate the parent, never vivify a missing key,` |
|       - |  226 | ` * unset's own wording for a bad base — except the removal itself, which belongs to` |
|       - |  227 | ` * the OUTERMOST subscript alone. */` |
|       - |  228 | `#define VM_IDX_CTX_UNSET_BASE 10` |
|       - |  229 | `#define VM_IDX_IS_UNSET(iP2) ((iP2) == VM_IDX_CTX_UNSET \|\| (iP2) == VM_IDX_CTX_UNSET_BASE)` |
|       - |  230 | `#define VM_IDX_CTX_EMPTY 6` |
|       - |  231 | `/*` |
|       - |  232 | ` * php rejects an OBJECT or an ARRAY used as an array offset, naming the offending` |
|       - |  233 | ` * type the way get_debug_type() does — the CLASS name for an object, "array" for` |
|       - |  234 | ` * an array — and wording the failure by context:` |
|       - |  235 | ` *` |
|       - |  236 | ` *   read/write   Cannot access offset of type Foo on array` |
|       - |  237 | ` *   isset/empty  Cannot access offset of type Foo in isset or empty` |
|       - |  238 | ` *   unset        Cannot unset offset of type Foo on array` |
|       - |  239 | ` *` |
|       - |  240 | ` * A RESOURCE is deliberately absent: php does not reject it, it warns` |
|       - |  241 | ` * ("Resource ID#N used as offset, casting to integer (N)") and uses the id as an` |
|       - |  242 | ` * integer key. VmOffsetResourceWarn() below handles that half.` |
|       - |  243 | ` *` |
|       - |  244 | ` * Returns TRUE and fills pMsg when the key must be rejected. iCtx is the` |
|       - |  245 | ` * instruction's iP2 (any value other than the isset/unset/empty codes reads as` |
|       - |  246 | ` * an access).` |
|       - |  247 | ` */` |
|  262562 |  248 | `static int VmOffsetTypeRejected(ph7_vm *pVm,ph7_value *pKey,int iCtx,SyBlob *pMsg)` |
|       5 |  249 | `{` |
|       - |  250 | `	const char *zType;` |
|  262567 |  251 | `	SyString *pClass = 0;` |
|  262567 |  252 | `	if( pKey == 0 ){` |
|     ! 0 |  253 | `		return FALSE;` |
|       - |  254 | `	}` |
|  262567 |  255 | `	if( pKey->iFlags & MEMOBJ_OBJ ){` |
|      27 |  256 | `		ph7_class_instance *pInst = (ph7_class_instance *)pKey->x.pOther;` |
|      27 |  257 | `		if( pInst && pInst->pClass ){` |
|      27 |  258 | `			pClass = &pInst->pClass->sName;` |
|      12 |  259 | `		}` |
|      27 |  260 | `		zType = "object";` |
|  262555 |  261 | `	}else if( pKey->iFlags & MEMOBJ_HASHMAP ){` |
|      16 |  262 | `		zType = "array";` |
|       9 |  263 | `	}else{` |
|  262529 |  264 | `		return FALSE;` |
|       - |  265 | `	}` |
|      41 |  266 | `	SyBlobInit(pMsg,&pVm->sAllocator);` |
|      41 |  267 | `	if( VM_IDX_IS_UNSET(iCtx) ){` |
|       3 |  268 | `		SyBlobAppend(pMsg,"Cannot unset offset of type ",sizeof("Cannot unset offset of type ")-1);` |
|       2 |  269 | `	}else{` |
|      39 |  270 | `		SyBlobAppend(pMsg,"Cannot access offset of type ",sizeof("Cannot access offset of type ")-1);` |
|       - |  271 | `	}` |
|      41 |  272 | `	if( pClass ){` |
|      27 |  273 | `		SyBlobAppend(pMsg,pClass->zString,pClass->nByte);` |
|      15 |  274 | `	}else{` |
|      16 |  275 | `		SyBlobAppend(pMsg,zType,(sxu32)SyStrlen(zType));` |
|       - |  276 | `	}` |
|      41 |  277 | `	if( iCtx == VM_IDX_CTX_ISSET \|\| iCtx == VM_IDX_CTX_EMPTY ){` |
|       7 |  278 | `		SyBlobAppend(pMsg," in isset or empty",sizeof(" in isset or empty")-1);` |
|       4 |  279 | `	}else{` |
|      35 |  280 | `		SyBlobAppend(pMsg," on array",sizeof(" on array")-1);` |
|       - |  281 | `	}` |
|      41 |  282 | `	return TRUE;` |
|  131286 |  283 | `}` |
|       - |  284 | `/*` |
|       - |  285 | ` * php's other half of the offset-type rules: a RESOURCE offset is accepted, with` |
|       - |  286 | `` * `Warning: Resource ID#N used as offset, casting to integer (N)`, and the key`` |
|       - |  287 | ` * becomes that integer. Rewrites pKey in place so the normal integer-key path` |
|       - |  288 | ` * takes over.` |
|       - |  289 | ` */` |
|  262524 |  290 | `static void VmOffsetResourceWarn(ph7_vm *pVm,ph7_value *pKey)` |
|       5 |  291 | `{` |
|       - |  292 | `	sxu32 nId;` |
|  262529 |  293 | `	if( pKey == 0 \|\| (pKey->iFlags & MEMOBJ_RES) == 0 ){` |
|  262519 |  294 | `		return;` |
|       - |  295 | `	}` |
|      11 |  296 | `	nId = PH7_VmResourceId(pVm,pKey->x.pOther);` |
|      16 |  297 | `	VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       5 |  298 | `		"Resource ID#%u used as offset, casting to integer (%u)",nId,nId);` |
|      11 |  299 | `	PH7_MemObjRelease(pKey);` |
|      11 |  300 | `	pKey->x.iVal = (sxi64)nId;` |
|      11 |  301 | `	MemObjSetType(pKey,MEMOBJ_INT);` |
|  131267 |  302 | `}` |
|       - |  303 | `/*` |
|       - |  304 | ` * php DEPRECATES (it does not reject) a NULL array offset, then normalizes it to` |
|       - |  305 | `` * the empty-string key "": `$a[null]`, `$a[$undef]` and `[null => v]` all warn`` |
|       - |  306 | `` * `Using null as an array offset is deprecated, use an empty string instead` and`` |
|       - |  307 | ` * then read/write the "" slot. Emit that E_DEPRECATED notice and return TRUE so` |
|       - |  308 | ` * the caller falls through to the ordinary lookup/insert, which already casts` |
|       - |  309 | ` * NULL->"" (HashmapLookup / HashmapInsert). A LOSSY-FLOAT offset stays a rejected` |
|       - |  310 | ` * TypeError: PHL deliberately targets php's NON-deprecated surface for the` |
|       - |  311 | ` * float->int truncation (VmRejectFloatOperand policy, §2), so only the null case` |
|       - |  312 | ` * is coerced here. Returns FALSE (no notice) for a non-null key.` |
|       - |  313 | ` */` |
|  356700 |  314 | `static int VmNullOffsetDeprecate(ph7_vm *pVm,ph7_value *pKey)` |
|       5 |  315 | `{` |
|  356705 |  316 | `	if( pKey == 0 \|\| (pKey->iFlags & MEMOBJ_NULL) == 0 ){` |
|  356681 |  317 | `		return FALSE;` |
|       - |  318 | `	}` |
|      27 |  319 | `	PH7_VmThrowError(&(*pVm),0,E_DEPRECATED,` |
|       - |  320 | `		"Using null as an array offset is deprecated, use an empty string instead");` |
|      27 |  321 | `	return TRUE;` |
|  178351 |  322 | `}` |
|       - |  323 | `/*` |
|       - |  324 | ` * The three rules above, applied to a BUILTIN's key argument.` |
|       - |  325 | ` *` |
|       - |  326 | `` * php's array_key_exists() does not run a `string\|int` ZPP row on its $key — it`` |
|       - |  327 | `` * hands the value to the same offset machinery `$a[$key]` uses, so the two agree`` |
|       - |  328 | ` * on every type: an object or an array is the catchable` |
|       - |  329 | `` * `Cannot access offset of type X on array`, a RESOURCE warns and becomes its`` |
|       - |  330 | `` * integer id, `null` deprecates and reads the "" key, and a bool/float/numeric`` |
|       - |  331 | ` * string folds the way any subscript folds. PHL's builtin had its own narrower` |
|       - |  332 | `` * check and therefore its own answers — a `string\|int` TypeError for the two`` |
|       - |  333 | `` * cases php ACCEPTS (null, lossy float) and a silent `false` for the three php`` |
|       - |  334 | ` * REJECTS or coerces (object, array, resource). The object case was the worst of` |
|       - |  335 | ` * them: a __toString() object was stringified and could answer TRUE for a key` |
|       - |  336 | ` * php refuses to look up at all.` |
|       - |  337 | ` *` |
|       - |  338 | ` * bZppWording picks which of php's two messages the caller reports. php words the` |
|       - |  339 | ` * illegal-type rejection differently in the alias than in the function itself —` |
|       - |  340 | `` * `key_exists(): Argument #1 ($key) must be a valid array offset type` vs the`` |
|       - |  341 | ` * engine's offset Error — verified against 8.5.8; the null-key DEPRECATION is the` |
|       - |  342 | ` * array_key_exists() wording in both.` |
|       - |  343 | ` *` |
|       - |  344 | ` * pKey is rewritten in place (resource -> int), so callers pass a private copy,` |
|       - |  345 | ` * not the caller's own argument slot. Returns SXRET_OK when the key is usable, or` |
|       - |  346 | ` * the status of the TypeError thrown.` |
|       - |  347 | ` */` |
|     128 |  348 | `PH7_PRIVATE sxi32 PH7_VmArrayKeyArg(ph7_context *pCtx,ph7_value *pKey,int bZppWording)` |
|       5 |  349 | `{` |
|     133 |  350 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - |  351 | `	SyBlob sMsg;` |
|     133 |  352 | `	if( VmOffsetTypeRejected(pVm,pKey,0,&sMsg) ){` |
|       - |  353 | `		sxi32 rc;` |
|      11 |  354 | `		if( bZppWording ){` |
|       5 |  355 | `			SyBlobRelease(&sMsg);` |
|       7 |  356 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  357 | `				"%s(): Argument #1 ($key) must be a valid array offset type",` |
|       2 |  358 | `				ph7_function_name(pCtx));` |
|       - |  359 | `		}` |
|      10 |  360 | `		rc = PH7_VmThrowException(pCtx,"TypeError","%.*s",` |
|       6 |  361 | `			(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|       7 |  362 | `		SyBlobRelease(&sMsg);` |
|       7 |  363 | `		return rc;` |
|       - |  364 | `	}` |
|     123 |  365 | `	VmOffsetResourceWarn(pVm,pKey);` |
|     118 |  366 | `	if( (pKey->iFlags & MEMOBJ_REAL)` |
|      69 |  367 | `	 && pKey->rVal != (ph7_real)(sxi64)pKey->rVal ){` |
|       - |  368 | `		/* php DEPRECATES the lossy float and truncates; PHL rejects it, here as` |
|       - |  369 | ``		 * everywhere else (§10) — and with the SAME message `$a[5.7]` gives, so`` |
|       - |  370 | `		 * the builtin and the subscript stay one rule. */` |
|       8 |  371 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  372 | `			"Cannot access offset of type float on array");` |
|       - |  373 | `	}` |
|     116 |  374 | `	if( pKey->iFlags & MEMOBJ_NULL ){` |
|       - |  375 | `		/* php names the parameter here rather than "an array offset"; the effect` |
|       - |  376 | `		 * is the engine's — the lookup below reads the "" key. */` |
|       3 |  377 | `		PH7_VmThrowError(pVm,0,E_DEPRECATED,` |
|       - |  378 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|       - |  379 | `			"use an empty string instead");` |
|       1 |  380 | `	}` |
|     116 |  381 | `	return SXRET_OK;` |
|      69 |  382 | `}` |
|       - |  383 | `/*` |
|       - |  384 | ` * Does this string START with an integer php's is_numeric_string would answer` |
|       - |  385 | ` * IS_LONG for? Returns 0 when it does not — no digits at all ("", "-", "p"), a` |
|       - |  386 | ` * FLOAT-shaped prefix ("1.5", ".5", "1.", "1e2", "1E2x") or a run that overflows` |
|       - |  387 | ` * a signed 64-bit int ("9223372036854775808") — 1 when it does and only optional` |
|       - |  388 | ` * trailing WHITESPACE follows (" 12 ", "+12", "007"), and 2 when it does but` |
|       - |  389 | ` * trailing DATA follows ("12abc", "0x1", "1e", "1 x"), which is php's` |
|       - |  390 | `` * `Illegal string offset` warning. *piVal receives the integer for 1 and 2.`` |
|       - |  391 | ` *` |
|       - |  392 | ` * php reaches the same three answers through is_numeric_string_ex(allow_errors=1)` |
|       - |  393 | ` * plus its IS_LONG/IS_DOUBLE verdict: a fraction or a well-formed exponent makes` |
|       - |  394 | ` * the verdict IS_DOUBLE, which a string offset refuses, while a bare 'e' is just` |
|       - |  395 | ` * trailing data.` |
|       - |  396 | ` */` |
|     100 |  397 | `static int VmStringOffsetInt(const char *zIn,sxu32 nByte,sxi64 *piVal)` |
|       4 |  398 | `{` |
|     104 |  399 | `	const char *z = zIn, *zEnd = &zIn[nByte], *zDigit;` |
|     104 |  400 | `	sxu64 uVal = 0, uLimit;` |
|     104 |  401 | `	int isNeg = 0, nDigit, i;` |
|     116 |  402 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){` |
|      14 |  403 | `		z++;` |
|       2 |  404 | `	}` |
|     104 |  405 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|       7 |  406 | `		isNeg = z[0] == '-';` |
|       7 |  407 | `		z++;` |
|       3 |  408 | `	}` |
|     104 |  409 | `	zDigit = z;` |
|     242 |  410 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|     140 |  411 | `		z++;` |
|       2 |  412 | `	}` |
|     104 |  413 | `	nDigit = (int)(z - zDigit);` |
|     104 |  414 | `	if( nDigit < 1 ){` |
|      42 |  415 | `		return 0;` |
|       - |  416 | `	}` |
|      64 |  417 | `	if( z < zEnd && z[0] == '.' ){` |
|       - |  418 | `		/* "1." and "1.5" alike: php reads a double from here. */` |
|       5 |  419 | `		return 0;` |
|       - |  420 | `	}` |
|      60 |  421 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|       3 |  422 | `		const char *zExp = &z[1];` |
|       3 |  423 | `		if( zExp < zEnd && (zExp[0] == '+' \|\| zExp[0] == '-') ){` |
|     ! 0 |  424 | `			zExp++;` |
|     ! 0 |  425 | `		}` |
|       3 |  426 | `		if( zExp < zEnd && (unsigned char)zExp[0] < 0xc0 && SyisDigit(zExp[0]) ){` |
|       3 |  427 | `			return 0;` |
|       - |  428 | `		}` |
|     ! 0 |  429 | `	}` |
|       - |  430 | `	/* Accumulate unsigned so PHP_INT_MIN's magnitude (2^63) is representable —` |
|       - |  431 | `	 * "-9223372036854775808" is a legal offset, "9223372036854775808" is not. */` |
|      62 |  432 | `	while( nDigit > 1 && zDigit[0] == '0' ){` |
|       5 |  433 | `		zDigit++; nDigit--;` |
|       1 |  434 | `	}` |
|      58 |  435 | `	uLimit = isNeg ? (sxu64)SXI64_HIGH + 1 : (sxu64)SXI64_HIGH;` |
|      58 |  436 | `	if( nDigit > 19 ){` |
|     ! 0 |  437 | `		return 0;` |
|       - |  438 | `	}` |
|     184 |  439 | `	for( i = 0 ; i < nDigit ; ++i ){` |
|     130 |  440 | `		sxu64 d = (sxu64)(zDigit[i] - '0');` |
|     130 |  441 | `		if( uVal > (uLimit - d)/10 ){` |
|       3 |  442 | `			return 0;` |
|       - |  443 | `		}` |
|     128 |  444 | `		uVal = uVal*10 + d;` |
|      65 |  445 | `	}` |
|      56 |  446 | `	*piVal = isNeg ? (sxi64)(~uVal + 1) : (sxi64)uVal;` |
|      66 |  447 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){` |
|      12 |  448 | `		z++;` |
|       2 |  449 | `	}` |
|      56 |  450 | `	return z == zEnd ? 1 : 2;` |
|      54 |  451 | `}` |
|       - |  452 | `/*` |
|       - |  453 | ` * php's offset rules for a STRING container (zend_check_string_offset, and the` |
|       - |  454 | ` * isset/empty arm of ZEND_ISSET_ISEMPTY_DIM_OBJ). They are NOT the array rules,` |
|       - |  455 | ` * and PHL applied none of them: every offset went through an int cast, so` |
|       - |  456 | `` * `$s[""]`, `$s["-"]` and `$s["p"]` all answered `$s[0]` — a silent wrong answer`` |
|       - |  457 | ` * on code php refuses to run. php's table:` |
|       - |  458 | ` *` |
|       - |  459 | ` *   int                     the offset` |
|       - |  460 | ` *   null / bool / float     Warning: String offset cast occurred, then cast` |
|       - |  461 | ` *   integer-shaped string   the offset (leading/trailing space and '+' allowed)` |
|       - |  462 | ` *   int-then-garbage string Warning: Illegal string offset "12abc", then 12` |
|       - |  463 | ` *   any other string        TypeError: Cannot access offset of type string on string` |
|       - |  464 | ` *   array/object/resource   TypeError, naming the type (an object's CLASS)` |
|       - |  465 | ` *` |
|       - |  466 | ` * isset()/empty() raise NOTHING and answer "not set" for every shape the read` |
|       - |  467 | `` * path would reject OR warn about: `isset($s["0x1"])` is false even though`` |
|       - |  468 | `` * reading it warns and yields `$s[0]`. A `??` fetch sits BETWEEN that and a real`` |
|       - |  469 | ` * read — it suppresses the not-set diagnostics but still warns about the offset` |
|       - |  470 | ` * SHAPE and still reads it. iLevel selects which of the three (VM_STROFF_LOUD /` |
|       - |  471 | ` * _COALESCE / _ISSET).` |
|       - |  472 | ` */` |
|  853245 |  473 | `PH7_PRIVATE int VmStringOffsetResolve(ph7_vm *pVm,ph7_value *pIdx,int iLevel,sxi64 *piOfft,SyBlob *pMsg)` |
|       5 |  474 | `{` |
|  853250 |  475 | `	if( (pIdx->iFlags & MEMOBJ_INT) != 0 && (pIdx->iFlags & MEMOBJ_REAL) == 0 ){` |
|  853090 |  476 | `		*piOfft = pIdx->x.iVal;` |
|  853090 |  477 | `		return VM_STROFF_OK;` |
|       - |  478 | `	}` |
|     164 |  479 | `	if( pIdx->iFlags & MEMOBJ_STRING ){` |
|     154 |  480 | `		int eInt = VmStringOffsetInt((const char *)SyBlobData(&pIdx->sBlob),` |
|      50 |  481 | `			SyBlobLength(&pIdx->sBlob),piOfft);` |
|     104 |  482 | `		if( eInt == 1 ){` |
|      24 |  483 | `			return VM_STROFF_OK;` |
|       - |  484 | `		}` |
|      82 |  485 | `		if( eInt == 2 && iLevel != VM_STROFF_ISSET ){` |
|       - |  486 | ``			/* int-then-garbage. This is the one diagnostic a `??` fetch keeps:`` |
|       - |  487 | ``			 * `$s["1x"] ?? "d"` warns and answers $s[1] (PHL called it "not set"`` |
|       - |  488 | `			 * and answered the default — a wrong VALUE, not just a missing` |
|       - |  489 | `			 * warning). Only isset()/empty() stay silent about it. */` |
|       - |  490 | `			SyString sKey;` |
|      26 |  491 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob));` |
|      26 |  492 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Illegal string offset \"%z\"",&sKey);` |
|      26 |  493 | `			return VM_STROFF_OK;` |
|       - |  494 | `		}` |
|      58 |  495 | `		if( iLevel != VM_STROFF_LOUD ){` |
|      24 |  496 | `			return VM_STROFF_MISS;` |
|       4 |  497 | `		}` |
|      78 |  498 | `	}else if( (pIdx->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|       - |  499 | `		/* null / bool / float: php casts, but says so in a real read or write. */` |
|      40 |  500 | `		if( iLevel == VM_STROFF_LOUD ){` |
|      26 |  501 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"String offset cast occurred");` |
|      12 |  502 | `		}` |
|      40 |  503 | `		PH7_MemObjToInteger(pIdx);` |
|      40 |  504 | `		*piOfft = pIdx->x.iVal;` |
|      40 |  505 | `		return VM_STROFF_OK;` |
|      24 |  506 | `	}else if( iLevel == VM_STROFF_ISSET ){` |
|       - |  507 | `		/* An array/object/resource offset is "not set" for isset()/empty() — but a` |
|       - |  508 | ``		 * `??` fetch RAISES for it (probed: `$s[[]] ?? "d"` is the TypeError while`` |
|       - |  509 | ``		 * `isset($s[[]])` is false), so only the fully-quiet level answers MISS. */`` |
|      10 |  510 | `		return VM_STROFF_MISS;` |
|       - |  511 | `	}` |
|       - |  512 | `	{` |
|       - |  513 | `		char zBuf[128];` |
|      50 |  514 | `		SyBlobInit(pMsg,&pVm->sAllocator);` |
|      73 |  515 | `		SyBlobFormat(pMsg,"Cannot access offset of type %s on string",` |
|      23 |  516 | `			VmValueGivenName(pIdx,zBuf,sizeof(zBuf)));` |
|       - |  517 | `	}` |
|      50 |  518 | `	return VM_STROFF_REJECT;` |
|  427680 |  519 | `}` |
|       - |  520 | `/*` |
|       - |  521 | ` * OP_STORE_IDX_REF: body moved verbatim from the OP_STORE_IDX_REF arm of` |
|       - |  522 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  523 | ` */` |
|  357268 |  524 | `PH7_PRIVATE VmOpRc VmExecOpStoreIdxRef(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  525 | `{` |
|  357273 |  526 | `	ph7_value *pTos = pState->pTos;` |
|  357273 |  527 | `	ph7_value *pStack = pState->pStack;` |
|  357273 |  528 | `	VmInstr *aInstr = pState->aInstr;` |
|  357273 |  529 | `	sxi32 pc = pState->pc;` |
|       - |  530 | `	sxi32 rc;` |
|  178630 |  531 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|  357273 |  532 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|       - |  533 | `	ph7_value *pKey;` |
|       - |  534 | `	sxu32 nIdx;` |
|  357273 |  535 | `	if( pInstr->iP1 ){` |
|       - |  536 | `		/* Key is next on stack */` |
|   78857 |  537 | `		pKey = pTos;` |
|   78857 |  538 | `		pTos--;` |
|   39431 |  539 | `	}else{` |
|  278421 |  540 | `		pKey = 0;` |
|       - |  541 | `	}` |
|       - |  542 | `		/* php DEPRECATES a null / lossy-float write subscript (then normalizes "" /` |
|       - |  543 | ``		 * truncates); it does not reject either. A null key — by-VALUE (`$a[$k]=v`)`` |
|       - |  544 | ``		 * OR by-REF (`$a[$k]=&$x`) — deprecates + coerces to the "" key: the notice is`` |
|       - |  545 | `		 * emitted DOWN AT THE INSERT (below), not here, so a null/false container that` |
|       - |  546 | ``		 * auto-vivifies (`$x=null; $x[null]=v`) gets it too (the base is not yet a`` |
|       - |  547 | `		 * hashmap at this point), and PH7_HashmapInsert / HashmapInsertByRef both cast` |
|       - |  548 | `		 * NULL->"". A lossy-FLOAT subscript stays a loud TypeError in BOTH forms (the` |
|       - |  549 | `		 * recorded non-deprecated-surface policy, §2). */` |
|  357273 |  550 | `		if( pKey && (pTos->iFlags & MEMOBJ_HASHMAP) ){` |
|       - |  551 | `			SyBlob sTypeMsg;` |
|       - |  552 | `			/* An object/array key is php's TypeError; a resource key warns and` |
|       - |  553 | `			 * becomes its integer id. Both used to be stringified silently. */` |
|   78267 |  554 | `			if( VmOffsetTypeRejected(&(*pVm),pKey,0,&sTypeMsg) ){` |
|       - |  555 | `				sxi32 rcSc;` |
|       8 |  556 | `				PH7_MemObjRelease(pKey);` |
|       8 |  557 | `				VmPopOperand(&pTos,1);` |
|       8 |  558 | `				rcSc = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|      12 |  559 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       8 |  560 | `				rc = rcSc;` |
|       8 |  561 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  562 | `			}` |
|   78261 |  563 | `			VmOffsetResourceWarn(&(*pVm),pKey);` |
|   78256 |  564 | `			if( (pKey->iFlags & MEMOBJ_REAL)` |
|   39136 |  565 | `			 && pKey->rVal != (ph7_real)(sxi64)pKey->rVal ){` |
|       - |  566 | `				sxi32 rcSc;` |
|       3 |  567 | `				const char *zErr = "Cannot access offset of type float on array";` |
|       3 |  568 | `				PH7_MemObjRelease(pKey);` |
|       3 |  569 | `				VmPopOperand(&pTos,1);` |
|       3 |  570 | `				rcSc = VmThrowFromVm(&(*pVm),"TypeError",zErr,(sxu32)SyStrlen(zErr));` |
|       3 |  571 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       3 |  572 | `				rc = rcSc;` |
|       3 |  573 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  574 | `			}` |
|   39127 |  575 | `		}` |
|  357265 |  576 | `	nIdx = pTos->nIdx;` |
|       - |  577 | `	{` |
|       - |  578 | `		/* ArrayAccess::offsetSet dispatch.` |
|       - |  579 | `		 * Container may be on the stack as MEMOBJ_OBJ, or referenced via` |
|       - |  580 | `		 * the backing variable slot at nIdx. */` |
|  357265 |  581 | `		ph7_class_instance *pInst = 0;` |
|  357265 |  582 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|     426 |  583 | `			pInst = (ph7_class_instance *)pTos->x.pOther;` |
|  357054 |  584 | `		}else if( nIdx != SXU32_HIGH ){` |
|  356823 |  585 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  356823 |  586 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_OBJ) ){` |
|     ! 0 |  587 | `				pInst = (ph7_class_instance *)pBacking->x.pOther;` |
|     ! 0 |  588 | `			}` |
|  178405 |  589 | `		}` |
|  357265 |  590 | `		if( pInst ){` |
|     426 |  591 | `			ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|     426 |  592 | `			if( pArrayAccess && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|       - |  593 | `				ph7_class_method *pMeth;` |
|       - |  594 | `				ph7_value sNullKey;` |
|       - |  595 | `				ph7_value *apArg[2];` |
|     424 |  596 | `				if( pInstr->iOp == PH7_OP_STORE_IDX_REF ){` |
|     ! 0 |  597 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - |  598 | `						"Cannot assign by reference to overloaded object");` |
|     ! 0 |  599 | `					if( pKey ){ PH7_MemObjRelease(pKey); }` |
|     ! 0 |  600 | `					VmPopOperand(&pTos,2); /* container + value */` |
|     ! 0 |  601 | `					VM_EXIT_BREAK;` |
|       - |  602 | `				}` |
|     424 |  603 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - |  604 | `					"offsetSet",sizeof("offsetSet")-1);` |
|       - |  605 | `				/* Pop container; pTos now points to the value */` |
|     424 |  606 | `				VmPopOperand(&pTos,1);` |
|     424 |  607 | `				if( pKey == 0 ){` |
|      12 |  608 | `					PH7_MemObjInit(&(*pVm),&sNullKey);` |
|      12 |  609 | `					apArg[0] = &sNullKey;` |
|       7 |  610 | `				}else{` |
|     414 |  611 | `					apArg[0] = pKey;` |
|       - |  612 | `				}` |
|     424 |  613 | `				apArg[1] = pTos;` |
|     424 |  614 | `				if( pMeth ){` |
|     424 |  615 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,0,2,apArg);` |
|     210 |  616 | `				}` |
|     424 |  617 | `				if( pKey ){` |
|     414 |  618 | `					PH7_MemObjRelease(pKey);` |
|     209 |  619 | `				}else{` |
|      12 |  620 | `					PH7_MemObjRelease(&sNullKey);` |
|       - |  621 | `				}` |
|       - |  622 | `				/* Pop the value */` |
|     424 |  623 | `				VmPopOperand(&pTos,1);` |
|     424 |  624 | `				VM_EXIT_BREAK;` |
|       - |  625 | `			}` |
|       - |  626 | `			/* Object without ArrayAccess: PHP throws a fatal Error rather` |
|       - |  627 | `			 * than silently coercing the object into a hashmap (which is` |
|       - |  628 | `			 * what the legacy PH7 fall-through would do via MemObjToHashmap` |
|       - |  629 | `			 * a few lines below). Match PHP. */` |
|       - |  630 | `			{` |
|       - |  631 | `				char zMsg[256];` |
|       3 |  632 | `				SyString *pName = &pInst->pClass->sName;` |
|       4 |  633 | `				sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - |  634 | `					"Cannot use object of type %.*s as array",` |
|       2 |  635 | `					(int)pName->nByte,pName->zString);` |
|       3 |  636 | `				rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|       3 |  637 | `				if( pKey ){ PH7_MemObjRelease(pKey); }` |
|       3 |  638 | `				VmPopOperand(&pTos,2); /* container + value */` |
|       3 |  639 | `				if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       3 |  640 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  641 | `			}` |
|       - |  642 | `		}` |
|       - |  643 | `	}` |
|  356843 |  644 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       - |  645 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|       - |  646 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|       - |  647 | `		 * checking true sharing count, then re-add after separation. */` |
|  356639 |  648 | `		if( nIdx != SXU32_HIGH ){` |
|  356619 |  649 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  534930 |  650 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|  356619 |  651 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|       - |  652 | `				/* Only adjust refcount / perform COW if the backing variable` |
|       - |  653 | `				 * is still sharing the same hashmap instance. This mirrors` |
|       - |  654 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|       - |  655 | `				 * refcounts if the backing array was already separated. */` |
|  356619 |  656 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|  356619 |  657 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|  356619 |  658 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|  356619 |  659 | `					pMap->iRef++;  /* Re-add stack ref */` |
|  356619 |  660 | `					pTos->x.pOther = pMap;` |
|  178308 |  661 | `				}else{` |
|       - |  662 | `					/* Backing variable no longer points at pCur: skip COW here` |
|       - |  663 | `					 * and operate on the hashmap currently on the stack. */` |
|     ! 0 |  664 | `					pMap = pCur;` |
|       - |  665 | `				}` |
|  178308 |  666 | `			}else{` |
|     ! 0 |  667 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       - |  668 | `			}` |
|  178308 |  669 | `		}else{` |
|      21 |  670 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       - |  671 | `		}` |
|  356639 |  672 | `		if( pMap->iRef < 2 ){` |
|       - |  673 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|       - |  674 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|       - |  675 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|       - |  676 | `			 * no code checks iRef for COW decisions. */` |
|      19 |  677 | `			pMap->iRef = 2;` |
|       9 |  678 | `		}` |
|  178318 |  679 | `	}else{` |
|       - |  680 | `		ph7_value *pObj;` |
|     209 |  681 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     209 |  682 | `		if( pObj == 0 ){` |
|     ! 0 |  683 | `			if( pKey ){` |
|     ! 0 |  684 | `			  PH7_MemObjRelease(pKey);` |
|     ! 0 |  685 | `			}` |
|     ! 0 |  686 | `			VmPopOperand(&pTos,1);` |
|     ! 0 |  687 | `			VM_EXIT_BREAK;` |
|       - |  688 | `		}` |
|       - |  689 | `		/* Phase#1: Load the array */` |
|     209 |  690 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|     156 |  691 | `			VmPopOperand(&pTos,1);` |
|     156 |  692 | `			if( pKey == 0 ){` |
|       - |  693 | ``				/* `$s[] = 'x'` on a STRING: php raises the catchable Error`` |
|       - |  694 | `				 * "[] operator not supported for strings" and leaves the string` |
|       - |  695 | `				 * untouched. PHL silently APPENDED, so code that meant to build` |
|       - |  696 | `				 * an array from a variable holding a string quietly produced a` |
|       - |  697 | `				 * longer string instead of failing — a wrong answer, not a` |
|       - |  698 | `				 * missing diagnostic. */` |
|       - |  699 | `				SyBlob sErrMsg;` |
|       6 |  700 | `				SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       6 |  701 | `				SyBlobAppend(&sErrMsg,"[] operator not supported for strings",` |
|       - |  702 | `					sizeof("[] operator not supported for strings")-1);` |
|       6 |  703 | `				VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       6 |  704 | `				VM_EXIT_BREAK;` |
|     ! 0 |  705 | `			}else{` |
|     152 |  706 | `				sxi64 iOfft = 0;` |
|       - |  707 | `				SyBlob sTypeMsg;` |
|       - |  708 | `				/* php's offset rules run BEFORE the RHS is looked at: an offset it` |
|       - |  709 | `				 * refuses is the TypeError alone. The RHS cast below used to happen` |
|       - |  710 | ``				 * first, so every rejected shape — and `$s[] = [1,2]` above — came`` |
|       - |  711 | ``				 * with a spurious `Array to string conversion` in front of it. */`` |
|     152 |  712 | `				if( VmStringOffsetResolve(&(*pVm),pKey,VM_STROFF_LOUD,&iOfft,&sTypeMsg) == VM_STROFF_REJECT ){` |
|       - |  713 | `					sxi32 rcSc;` |
|       7 |  714 | `					PH7_MemObjRelease(pKey);` |
|       7 |  715 | `					rcSc = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|      16 |  716 | `					if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       7 |  717 | `					rc = rcSc;` |
|       7 |  718 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  719 | `				}` |
|       - |  720 | `				/* Force a string cast on the RHS (user-visible: an array warns` |
|       - |  721 | `				 * "Array to string conversion" before the offset write, §2, and a` |
|       - |  722 | `				 * not-stringable object throws — the target string is untouched) */` |
|       - |  723 | `				{` |
|     146 |  724 | `					sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|     146 |  725 | `					if( rcSv != SXRET_OK ){` |
|       5 |  726 | `						PH7_MemObjRelease(pKey);` |
|       7 |  727 | `						PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|     ! 0 |  728 | `					}` |
|       - |  729 | `				}` |
|     142 |  730 | `				if( VmStringOffsetWrite(&(*pVm),pObj,iOfft,pTos) != SXRET_OK ){` |
|       - |  731 | `					sxi32 rcEm;` |
|       9 |  732 | `					PH7_MemObjRelease(pKey);` |
|       9 |  733 | `					rcEm = VmThrowFromVm(&(*pVm),"Error",` |
|       - |  734 | `						"Cannot assign an empty string to a string offset",` |
|       - |  735 | `						sizeof("Cannot assign an empty string to a string offset")-1);` |
|       9 |  736 | `					if( rcEm == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       9 |  737 | `					rc = rcEm;` |
|      11 |  738 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  739 | `				}` |
|       - |  740 | `			}` |
|     134 |  741 | `			if( pKey ){` |
|     134 |  742 | `			  PH7_MemObjRelease(pKey);` |
|      65 |  743 | `			}` |
|     134 |  744 | `			VM_EXIT_BREAK;` |
|      57 |  745 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       - |  746 | `			/* php: only NULL and FALSE auto-vivify into an array. Writing an index into an` |
|       - |  747 | `			 * int/float/resource/TRUE is a catchable Error -- PH7 quietly REPLACED the value` |
|       - |  748 | `			 * with an array, destroying it ($x = 5; $x[0] = 1; left $x === [1]). */` |
|       - |  749 | `			/* php auto-vivifies false into an array with an 8.1 DEPRECATION; PHL` |
|       - |  750 | `			 * rejects any scalar base, false included (null still auto-vivifies). */` |
|      57 |  751 | `			int bScalar = (pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_RES\|MEMOBJ_BOOL)) != 0;` |
|      57 |  752 | `			if( bScalar ){` |
|       - |  753 | `				sxi32 rcSc;` |
|       8 |  754 | `				if( pKey ){` |
|       5 |  755 | `					PH7_MemObjRelease(pKey);` |
|       2 |  756 | `				}` |
|       8 |  757 | `				VmPopOperand(&pTos,1);` |
|       8 |  758 | `				rcSc = VmThrowFromVm(&(*pVm),"Error","Cannot use a scalar value as an array",` |
|       - |  759 | `					sizeof("Cannot use a scalar value as an array")-1);` |
|       8 |  760 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       8 |  761 | `				rc = rcSc;` |
|       8 |  762 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  763 | `			}` |
|       - |  764 | `			/* Force a hashmap cast  */` |
|      51 |  765 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      51 |  766 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  767 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|     ! 0 |  768 | `				VM_EXIT_ABORT;` |
|       - |  769 | `			}` |
|      23 |  770 | `		}` |
|       - |  771 | `		/* COW separate the backing variable before mutation */` |
|      51 |  772 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|       - |  773 | `	}` |
|  356685 |  774 | `	VmPopOperand(&pTos,1);` |
|       - |  775 | `	/* Phase#2: Perform the insertion. A null key deprecates + normalizes to "" for` |
|       - |  776 | `	 * BOTH the by-value and the by-ref store — emitted HERE, after a null/false` |
|       - |  777 | ``	 * container has auto-vivified to an array, so `$x[null]=v` and `$x[null]=&$y` on`` |
|       - |  778 | `	 * an undefined $x get the notice too (the top-of-handler check runs before` |
|       - |  779 | `	 * vivification, when the base is not yet a hashmap). HashmapInsert /` |
|       - |  780 | `	 * HashmapInsertByRef both then cast NULL->"". A null pKey==0 (an append, no key)` |
|       - |  781 | `	 * is not a null OFFSET and is left alone. */` |
|  356685 |  782 | `	VmNullOffsetDeprecate(&(*pVm),pKey);` |
|  356685 |  783 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && (pTos->iFlags & MEMOBJ_AUX_STROFFSET) ){` |
|       - |  784 | ``		/* `$a[] = &$s[1]`: the source is a string OFFSET, which php refuses to`` |
|       - |  785 | `		 * reference — and whose slot index is the BASE STRING's, so binding it` |
|       - |  786 | ``		 * aliased the whole string. Same Error the `=&` / by-ref-argument paths`` |
|       - |  787 | `		 * raise. The key is already popped; drop it and abandon the store. */` |
|       - |  788 | `		sxi32 rcSc;` |
|       5 |  789 | `		if( pKey ){` |
|       3 |  790 | `			PH7_MemObjRelease(pKey);` |
|       1 |  791 | `		}` |
|       5 |  792 | `		rcSc = VmThrowFromVm(&(*pVm),"Error","Cannot create references to/from string offsets",` |
|       - |  793 | `			sizeof("Cannot create references to/from string offsets")-1);` |
|       5 |  794 | `		if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       5 |  795 | `		rc = rcSc;` |
|       5 |  796 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  797 | `	}` |
|  356681 |  798 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|      54 |  799 | `		if( pMap == pVm->pGlobal ){` |
|       - |  800 | `			/* php 8.1: $GLOBALS['y'] =& $x binds the global $y to $x's` |
|       - |  801 | `			 * slot; an append has no name to bind (catchable Error). */` |
|       5 |  802 | `			if( pKey == 0 ){` |
|     ! 0 |  803 | `				rc = PH7_VmThrowGlobalsAppendError(&(*pVm));` |
|     ! 0 |  804 | `			}else{` |
|       5 |  805 | `				if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 |  806 | `					PH7_MemObjToString(pKey);` |
|     ! 0 |  807 | `				}` |
|       5 |  808 | `				if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|       - |  809 | `					/* Pathological empty name: keep the legacy diagnostic */` |
|     ! 0 |  810 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|       - |  811 | `						"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 |  812 | `					rc = SXRET_OK;` |
|     ! 0 |  813 | `				}else{` |
|       7 |  814 | `					rc = PH7_VmInstallGlobalVar(&(*pVm),` |
|       4 |  815 | `						(const char *)SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),` |
|       4 |  816 | `						0,pTos->nIdx);` |
|       - |  817 | `				}` |
|       - |  818 | `			}` |
|       3 |  819 | `		}else{` |
|       - |  820 | `			/* Insertion by reference */` |
|      50 |  821 | `			rc = PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|       - |  822 | `		}` |
|      28 |  823 | `	}else{` |
|  356629 |  824 | `		rc = PH7_HashmapInsert(pMap,pKey,pTos);` |
|       - |  825 | `	}` |
|  356681 |  826 | `	if( pKey ){` |
|   78283 |  827 | `		PH7_MemObjRelease(pKey);` |
|   39139 |  828 | `	}` |
|       - |  829 | `	/* An append onto the occupied saturated auto-index threw php's catchable` |
|       - |  830 | `	 * Error (PH7_VmThrowArrayNextIndexError) — dispatch it like any other` |
|       - |  831 | `	 * store-path throw. Plain failures (OOM) keep their existing routes. */` |
|  356683 |  832 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|  356677 |  833 | `	VM_EXIT_BREAK;` |
|     ! 0 |  834 | `	VM_EXIT_BREAK;` |
|  178635 |  835 | `}` |
|       - |  836 |  |
|       - |  837 | `/*` |
|       - |  838 | ` * OP_LOAD_CLOSURE: body moved verbatim from the OP_LOAD_CLOSURE arm of` |
|       - |  839 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  840 | ` */` |
|    8310 |  841 | `PH7_PRIVATE VmOpRc VmExecOpLoadClosure(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  842 | `{` |
|    8315 |  843 | `	ph7_value *pTos = pState->pTos;` |
|    8315 |  844 | `	ph7_value *pStack = pState->pStack;` |
|    8315 |  845 | `	VmInstr *aInstr = pState->aInstr;` |
|    8315 |  846 | `	sxi32 pc = pState->pc;` |
|       - |  847 | `	sxi32 rc;` |
|    4155 |  848 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    8315 |  849 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|       - |  850 | `	/* The function whose name the Closure object will wrap: a fresh per-instantiation` |
|       - |  851 | `	 * copy for a real closure (built below), or the shared lambda function itself for a` |
|       - |  852 | `	 * plain anonymous function with no captured environment. */` |
|    8315 |  853 | `	ph7_vm_func *pTarget = pFunc;` |
|       - |  854 | `	/* A no-capture lambda declared inside a class method is not VM_FUNC_CLOSURE` |
|       - |  855 | `	 * (its env is empty), yet php still binds the creation-site class as its scope` |
|       - |  856 | ``	 * so `self::`/private access inside the body works. Detect the enclosing class`` |
|       - |  857 | `	 * here and route such a lambda through the per-instance closure path (a global` |
|       - |  858 | `	 * no-capture lambda peeks NULL and stays a shared function). */` |
|    8315 |  859 | `	ph7_class *pLoadScope = (pFunc->iFlags & VM_FUNC_CLOSURE) ? 0 : PH7_VmPeekDeclaringClass(pVm);` |
|    8315 |  860 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) \|\| pLoadScope ){` |
|       - |  861 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|       - |  862 | `		ph7_vm_func *pClosure;` |
|       - |  863 | `		char *zName;` |
|       - |  864 | `		sxu32 mLen;` |
|       - |  865 | `		sxu32 n;` |
|       - |  866 | `		/* Create a new VM function */` |
|    8203 |  867 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|       - |  868 | `		/* Generate an unique closure name */` |
|    8203 |  869 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|    8203 |  870 | `		if( pClosure == 0 \|\| zName == 0){` |
|     ! 0 |  871 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|     ! 0 |  872 | `			VM_EXIT_ABORT;` |
|       - |  873 | `		}` |
|    8203 |  874 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|    8203 |  875 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|     ! 0 |  876 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|     ! 0 |  877 | `		}` |
|       - |  878 | `		/* Zero the stucture */` |
|    8203 |  879 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|       - |  880 | `		/* Perform a structure assignment on read-only items */` |
|    8203 |  881 | `		pClosure->aArgs = pFunc->aArgs;` |
|    8203 |  882 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|    8203 |  883 | `		pClosure->aStatic = pFunc->aStatic;` |
|    8203 |  884 | `		pClosure->iFlags = pFunc->iFlags;` |
|       - |  885 | `		/* An in-class no-capture lambda routed here (pLoadScope set) must be a` |
|       - |  886 | `		 * real closure so PH7_VmClassMemberAccess honors its pUserData scope. */` |
|    8203 |  887 | `		pClosure->iFlags \|= VM_FUNC_CLOSURE;` |
|    8203 |  888 | `		pClosure->pUserData = pFunc->pUserData;` |
|    8203 |  889 | `		pClosure->sSignature = pFunc->sSignature;` |
|    8203 |  890 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|    8203 |  891 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|    8203 |  892 | `		pClosure->aReturnUnion = pFunc->aReturnUnion;` |
|    8203 |  893 | `		pClosure->sReturnTypeName = pFunc->sReturnTypeName;` |
|    8203 |  894 | `		pClosure->bStrictTypes = pFunc->bStrictTypes;` |
|    8203 |  895 | `		pClosure->nMaxStack = pFunc->nMaxStack;` |
|    8203 |  896 | `		if( pClosure->pUserData == 0 ){` |
|       - |  897 | `			/* Stamp the creation-site class scope (see PH7_VmPeekDeclaringClass):` |
|       - |  898 | `			 * a closure made in a method — or in another closure, whose own stamp` |
|       - |  899 | `			 * the peek reads — resolves self::/parent:: against it like php. */` |
|    8203 |  900 | `			pClosure->pUserData = (void *)PH7_VmPeekDeclaringClass(pVm);` |
|    4099 |  901 | `		}` |
|       - |  902 | ``		/* Capture the creation-site late-static-binding class so `static::` inside`` |
|       - |  903 | `		 * the closure body resolves like php (the "called class", which may differ` |
|       - |  904 | `		 * from the declaring scope stamped above — e.g. a closure made in an` |
|       - |  905 | `		 * inherited method). A closure made outside any class captures NULL. */` |
|    8203 |  906 | `		pClosure->pLsbClass = (void *)PH7_VmPeekTopClass(pVm);` |
|       - |  907 | `		/* Reflection descriptor fields (getDocComment/getAttributes/getStartLine/` |
|       - |  908 | `		 * getEndLine/getFileName read the INSTANTIATED copy via $__fn) */` |
|    8203 |  909 | `		pClosure->aAttrs = pFunc->aAttrs;` |
|    8203 |  910 | `		pClosure->sDoc = pFunc->sDoc;` |
|    8203 |  911 | `		pClosure->sFile = pFunc->sFile;` |
|    8203 |  912 | `		pClosure->nLine = pFunc->nLine;` |
|    8203 |  913 | `		pClosure->nEndLine = pFunc->nEndLine;` |
|       - |  914 | ``		/* php's visible `{closure:...}` name is a property of the DECLARATION, so every`` |
|       - |  915 | `		 * per-instantiation copy answers the same one (php has a single op_array here). */` |
|    8203 |  916 | `		pClosure->sClosureName = pFunc->sClosureName;` |
|    8203 |  917 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|       - |  918 | `		/* Register the closure */` |
|    8203 |  919 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|       - |  920 | `		/* Set up closure environment */` |
|    8203 |  921 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|    8203 |  922 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|   18819 |  923 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|       - |  924 | `			ph7_value *pValue;` |
|   10621 |  925 | `			pEnv = &aEnv[n];` |
|   10621 |  926 | `			sEnv.sName  = pEnv->sName;` |
|   10621 |  927 | `			sEnv.iFlags = pEnv->iFlags;` |
|   10621 |  928 | `			sEnv.nLine = pEnv->nLine;` |
|   10621 |  929 | `			sEnv.nIdx = SXU32_HIGH;` |
|   10621 |  930 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|   10616 |  931 | `			if( (sEnv.iFlags & (VM_FUNC_ARG_BY_REF\|VM_FUNC_ARG_IGNORE)) == VM_FUNC_ARG_BY_REF` |
|    5666 |  932 | `			 && !(SyStringLength(&sEnv.sName) == sizeof("this")-1` |
|     353 |  933 | `				&& SyMemcmp(SyStringData(&sEnv.sName),"this",sizeof("this")-1) == 0) ){` |
|       - |  934 | `				/* Capture by reference: bind the env entry to the variable's` |
|       - |  935 | `				 * memory slot — creating a fresh null variable when missing,` |
|       - |  936 | ``				 * as php does (`use (&$f)` before $f is assigned) — and pin`` |
|       - |  937 | `				 * the slot past the creating frame's teardown so the closure` |
|       - |  938 | `				 * can outlive its birth scope. The call-time env install` |
|       - |  939 | `				 * aliases the name to this slot instead of copying a value. */` |
|     479 |  940 | `				pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,TRUE);` |
|     479 |  941 | `				if( pValue ){` |
|     479 |  942 | `					sEnv.nIdx = pValue->nIdx;` |
|     479 |  943 | `					VmPinMemObjSlot(pVm,pValue->nIdx);` |
|     237 |  944 | `				}` |
|     242 |  945 | `			}else{` |
|       - |  946 | `				/* Standard pass by value */` |
|   10147 |  947 | `				pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|   10147 |  948 | `				if( pValue ){` |
|       - |  949 | `					/* Copy imported value */` |
|    2205 |  950 | `					PH7_MemObjStore(pValue,&sEnv.sValue);` |
|    9047 |  951 | `				}else if( (sEnv.iFlags & (VM_FUNC_ARG_BY_REF\|VM_FUNC_ARG_IGNORE)) == 0` |
|    3997 |  952 | `					&& !(SyStringLength(&sEnv.sName) == sizeof("this")-1` |
|      21 |  953 | `						&& SyMemcmp(SyStringData(&sEnv.sName),"this",sizeof("this")-1) == 0) ){` |
|      42 |  954 | `					if( pFunc->iFlags & VM_FUNC_ARROW ){` |
|       - |  955 | `						/* An arrow function auto-captures free variables by value, but` |
|       - |  956 | `						 * php does NOT capture one that is UNDEFINED at creation: the` |
|       - |  957 | `						 * isolated body scope then simply has no such variable, so a` |
|       - |  958 | `						 * read of it there raises the normal "Undefined variable"` |
|       - |  959 | `						 * warning (and reflection's getClosureUsedVariables omits it).` |
|       - |  960 | `						 * Skip installing the capture so the body READ — not the` |
|       - |  961 | `						 * creation — warns, matching php. (A later assignment to the` |
|       - |  962 | `						 * outer variable does not retro-capture: arrow scope is` |
|       - |  963 | `						 * isolated.) An explicit by-value use() instead warns here and` |
|       - |  964 | `						 * binds NULL, handled just below. */` |
|      32 |  965 | `						continue;` |
|       - |  966 | `					}` |
|       - |  967 | ``					/* php reads a by-value `use ($q)` capture AT CLOSURE CREATION and`` |
|       - |  968 | `					 * warns when the variable is undefined there (the by-ref form` |
|       - |  969 | ``					 * `use (&$q)` above stays silent — it creates the binding). The`` |
|       - |  970 | `					 * auto-injected $this (VM_FUNC_ARG_IGNORE) never warns. The` |
|       - |  971 | `					 * capture still proceeds as NULL, as php does. php attributes the` |
|       - |  972 | `					 * warning to the capture's own line (which can differ from the` |
|       - |  973 | `					 * OP_LOAD_CLOSURE instruction line when the use-clause wraps), so` |
|       - |  974 | `					 * borrow the recorded line for the emission and restore it. */` |
|      11 |  975 | `					sxu32 nSavedLine = pVm->nCurLine;` |
|      11 |  976 | `					if( sEnv.nLine ){` |
|      11 |  977 | `						pVm->nCurLine = sEnv.nLine;` |
|       5 |  978 | `					}` |
|      11 |  979 | `					VmErrorFormat(pVm,PH7_CTX_WARNING,"Undefined variable $%z",&sEnv.sName);` |
|      11 |  980 | `					pVm->nCurLine = nSavedLine;` |
|       5 |  981 | `				}` |
|       - |  982 | `			}` |
|       - |  983 | `			/* Insert the imported variable */` |
|   10591 |  984 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|    5298 |  985 | `		}` |
|    8203 |  986 | `		pTarget = pClosure;` |
|    4099 |  987 | `	}` |
|       - |  988 | `	/* Wrap the target function in a Closure object and push it. Its captured environment` |
|       - |  989 | ``	 * (incl. any `$this`) stays in pTarget->aClosureEnv and is delivered by the normal call`` |
|       - |  990 | `	 * path when the closure is dispatched by name. */` |
|    8315 |  991 | `	pTos++;` |
|       - |  992 | `	{` |
|    8315 |  993 | `		ph7_class_instance *pCloObj = VmCreateClosure(pVm, &pTarget->sName, 0, 0);` |
|    8315 |  994 | `		if( pCloObj ){` |
|    8315 |  995 | `			pCloObj->iRef++;` |
|    8315 |  996 | `			pTos->x.pOther = pCloObj;` |
|    8315 |  997 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|    4160 |  998 | `		}else{` |
|       - |  999 | `			/* OOM fallback: the name string is still a usable callable. */` |
|     ! 0 | 1000 | `			PH7_MemObjStringAppend(pTos, pTarget->sName.zString, pTarget->sName.nByte);` |
|       - | 1001 | `		}` |
|       - | 1002 | `	}` |
|    8315 | 1003 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1004 | `	VM_EXIT_BREAK;` |
|    4160 | 1005 | `}` |
|       - | 1006 |  |
|       - | 1007 |  |
|       - | 1008 | `/*` |
|       - | 1009 | `` * TRUE when this LOAD_IDX feeds a `??`: the coalesce test is the very next`` |
|       - | 1010 | `` * instruction. php evaluates the whole left operand of `??` in isset-context,`` |
|       - | 1011 | ` * so the access must stay SILENT and, when it misses, must yield NULL — an` |
|       - | 1012 | ``  * out-of-range string offset that yielded "" instead made `$s[99] ?? $d` `` |
|       - | 1013 | ` * evaluate to "" rather than $d, a wrong answer rather than a stray notice.` |
|       - | 1014 | ` */` |
|  853051 | 1015 | `static int VmIdxFeedsCoalesce(const VmInstr *pInstr)` |
|       5 | 1016 | `{` |
|  853056 | 1017 | `	return (pInstr+1)->iOp == PH7_OP_NULLC \|\| (pInstr+1)->iOp == PH7_OP_NULLC_JMP;` |
|       5 | 1018 | `}` |
|       - | 1019 | `/*` |
|       - | 1020 | `` * php's string-offset STORE, shared by `$s[i] = v` (OP_STORE_IDX) and`` |
|       - | 1021 | `` * `$s[i] ??= v` (OP_NULLC_STORE — `??=` is not an assign-op, so php performs a`` |
|       - | 1022 | ` * real offset write there): resolve iRawOfft against the string's CURRENT length` |
|       - | 1023 | ` * (a negative offset counts back from the end and, when it still lands before the` |
|       - | 1024 | `` * start, warns `Illegal string offset` and writes NOTHING), refuse an EMPTY`` |
|       - | 1025 | ` * replacement, warn when more than one byte was handed over, PAD WITH SPACES up` |
|       - | 1026 | ` * to the offset, then write the first byte.` |
|       - | 1027 | ` *` |
|       - | 1028 | ` * pVal must ALREADY be a string: that coercion is user-visible (it warns for an` |
|       - | 1029 | ` * array, throws for a not-stringable object) and stays with the callers, which` |
|       - | 1030 | ` * are the only places that can route a throw. Answers SXRET_OK when the store` |
|       - | 1031 | `` * happened or was skipped, and SXERR_INVALID for php's `Cannot assign an empty`` |
|       - | 1032 | `` * string to a string offset` Error — raised by the caller for the same reason.`` |
|       - | 1033 | ` */` |
|     178 | 1034 | `PH7_PRIVATE sxi32 VmStringOffsetWrite(ph7_vm *pVm,ph7_value *pStr,sxi64 iRawOfft,ph7_value *pVal)` |
|       4 | 1035 | `{` |
|     182 | 1036 | `	sxi64 nLen = (sxi64)SyBlobLength(&pStr->sBlob);` |
|     182 | 1037 | `	sxi64 iOfft = iRawOfft;` |
|       - | 1038 | `	const char *zVal;` |
|     182 | 1039 | `	if( iOfft < 0 ){` |
|       - | 1040 | `		/* php 7.1: a negative offset writes back from the end. */` |
|       9 | 1041 | `		iOfft += nLen;` |
|       9 | 1042 | `		if( iOfft < 0 ){` |
|       7 | 1043 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Illegal string offset %qd",iRawOfft);` |
|       7 | 1044 | `			return SXRET_OK;` |
|       - | 1045 | `		}` |
|       1 | 1046 | `	}` |
|     176 | 1047 | `	if( SyBlobLength(&pVal->sBlob) < 1 ){` |
|       - | 1048 | ``		/* php refuses to write NOTHING into an offset — `$s[2] = ""`, and the`` |
|       - | 1049 | ``		 * `= null` / `= false` that stringify to "" — where PHL silently ignored`` |
|       - | 1050 | `		 * the store. */` |
|      13 | 1051 | `		return SXERR_INVALID;` |
|       - | 1052 | `	}` |
|     164 | 1053 | `	zVal = (const char *)SyBlobData(&pVal->sBlob);` |
|     164 | 1054 | `	if( SyBlobLength(&pVal->sBlob) > 1 ){` |
|      10 | 1055 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 1056 | `			"Only the first byte will be assigned to the string offset");` |
|       4 | 1057 | `	}` |
|     164 | 1058 | `	if( iOfft >= nLen ){` |
|       - | 1059 | `		/* php PADS WITH SPACES up to the offset. PH7 simply appended the byte, so` |
|       - | 1060 | `		 * "abc" with [6]="Z" became "abcZ" rather than "abc   Z" -- a silently` |
|       - | 1061 | `		 * wrong string. */` |
|       - | 1062 | `		sxi64 nPad;` |
|     209 | 1063 | `		for( nPad = nLen ; nPad < iOfft ; ++nPad ){` |
|     169 | 1064 | `			SyBlobAppend(&pStr->sBlob," ",sizeof(char));` |
|      85 | 1065 | `		}` |
|      41 | 1066 | `		SyBlobAppend(&pStr->sBlob,(const void *)zVal,sizeof(char));` |
|      21 | 1067 | `	}else{` |
|     124 | 1068 | `		char *zData = (char *)SyBlobData(&pStr->sBlob);` |
|     124 | 1069 | `		zData[iOfft] = zVal[0];` |
|       - | 1070 | `	}` |
|     164 | 1071 | `	return SXRET_OK;` |
|      93 | 1072 | `}` |
|       - | 1073 | `/*` |
|       - | 1074 | `` * Does this LOAD_IDX feed a `??=` (rather than a plain `??`)? The compiler emits`` |
|       - | 1075 | ` * the LHS peek, then OP_NULLC_JMP, then the RHS, then OP_NULLC_STORE — so the` |
|       - | 1076 | ` * NULLC_JMP right after is what distinguishes the assigning form, whose store` |
|       - | 1077 | ` * still has to happen when the peek answers null.` |
|       - | 1078 | ` */` |
|  853047 | 1079 | `static int VmIdxFeedsCoalesceAssign(const VmInstr *pInstr)` |
|       5 | 1080 | `{` |
|  853052 | 1081 | `	return (pInstr+1)->iOp == PH7_OP_NULLC_JMP;` |
|       5 | 1082 | `}` |
|       - | 1083 | `/*` |
|       - | 1084 | `` * Arm the write-back half of `$o[$k] op= v` on an ArrayAccess container. The`` |
|       - | 1085 | ` * value offsetGet just answered goes into a fresh SCRATCH memobj that pTos` |
|       - | 1086 | ` * points at, so the compound-assign op computes IN that slot the way it would` |
|       - | 1087 | ` * in an ordinary variable; the pending entry then makes the op's tail dispatch` |
|       - | 1088 | `` * `offsetSet($k, computed)`. The KEY gets a reserved slot of its own (pIdx is`` |
|       - | 1089 | ` * released as soon as this opcode returns, and the write happens one opcode` |
|       - | 1090 | `` * later); an ABSENT key — `$o[] op= v` — is carried as NULL, which is the value`` |
|       - | 1091 | ` * php hands both accessors for that shape.` |
|       - | 1092 | ` *` |
|       - | 1093 | ` * A failed reservation simply leaves the value unarmed: pTos keeps its` |
|       - | 1094 | ` * no-slot temp and the op falls back to the pre-existing refusal.` |
|       - | 1095 | ` */` |
|      42 | 1096 | `static void VmDimRmwArm(` |
|       - | 1097 | `	ph7_vm *pVm,` |
|       - | 1098 | `	ph7_class_instance *pInst,` |
|       - | 1099 | `	ph7_value *pIdx,` |
|       - | 1100 | `	ph7_value *pTos,` |
|       - | 1101 | `	void *pOwnerStack,` |
|       - | 1102 | `	void *pInstrs,` |
|       - | 1103 | `	sxu32 nPc` |
|       - | 1104 | `	)` |
|       1 | 1105 | `{` |
|       - | 1106 | `	ph7_value *pSlot;` |
|       - | 1107 | `	sxu32 nScratch;` |
|       - | 1108 | `	sxu32 nKey;` |
|       - | 1109 | `	VmHookRmw sRmw;` |
|      43 | 1110 | `	pSlot = PH7_ReserveMemObj(&(*pVm));` |
|      43 | 1111 | `	if( pSlot == 0 ){` |
|     ! 0 | 1112 | `		return;` |
|       - | 1113 | `	}` |
|      43 | 1114 | `	nScratch = pSlot->nIdx;` |
|      43 | 1115 | `	pSlot = PH7_ReserveMemObj(&(*pVm));` |
|      43 | 1116 | `	if( pSlot == 0 ){` |
|     ! 0 | 1117 | `		VmHookRmwFreeScratch(&(*pVm),nScratch);` |
|     ! 0 | 1118 | `		return;` |
|       - | 1119 | `	}` |
|      43 | 1120 | `	nKey = pSlot->nIdx;` |
|       - | 1121 | `	/* Reserving can GROW the aMemObj set, so address both slots by index from` |
|       - | 1122 | `	 * here on — the pointer the first reservation handed back may be stale. */` |
|      43 | 1123 | `	if( pIdx ){` |
|      43 | 1124 | `		PH7_MemObjStore(pIdx,pSlot);` |
|      21 | 1125 | `	}` |
|      43 | 1126 | `	pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,nScratch);` |
|      43 | 1127 | `	if( pSlot == 0 ){` |
|     ! 0 | 1128 | `		VmHookRmwFreeScratch(&(*pVm),nKey);` |
|     ! 0 | 1129 | `		VmHookRmwFreeScratch(&(*pVm),nScratch);` |
|     ! 0 | 1130 | `		return;` |
|       - | 1131 | `	}` |
|      43 | 1132 | `	PH7_MemObjStore(pTos,pSlot);` |
|      43 | 1133 | `	sRmw.iKind = VM_HOOK_PEND_RMW_DIM;` |
|      43 | 1134 | `	sRmw.pThis = pInst;` |
|      43 | 1135 | `	sRmw.pAttr = 0;` |
|      43 | 1136 | `	sRmw.nBackIdx = nKey;` |
|      43 | 1137 | `	sRmw.nScratchIdx = nScratch;` |
|      43 | 1138 | `	SyBlobInit(&sRmw.sName,&pVm->sAllocator);` |
|      43 | 1139 | `	sRmw.pOwnerStack = pOwnerStack;` |
|      43 | 1140 | `	sRmw.pInstrs = pInstrs;` |
|      43 | 1141 | `	sRmw.nJmpPc = nPc;  /* the modify op ... */` |
|      43 | 1142 | `	sRmw.nPc = nPc;     /* ... is the whole window */` |
|      43 | 1143 | `	pInst->iRef++;` |
|      43 | 1144 | `	pTos->nIdx = nScratch;` |
|      43 | 1145 | `	SySetPut(&pVm->aHookRmw,(const void *)&sRmw);` |
|      22 | 1146 | `}` |
|       - | 1147 | `/*` |
|       - | 1148 | ` * Is this LOAD_IDX fetching the dimension for WRITING — php's BP_VAR_W / BP_VAR_RW` |
|       - | 1149 | ` * / BP_VAR_UNSET fetch, the one that asks the container for a slot to MODIFY` |
|       - | 1150 | ` * rather than a value to read?` |
|       - | 1151 | ` *` |
|       - | 1152 | ` * iP2 answers for most of it. The two shapes it cannot are the ones where the` |
|       - | 1153 | ` * fetch is compiled as a plain read and the NEXT instruction is what makes it a` |
|       - | 1154 | `` * write: binding a reference to the element (`$r = &$o[$k]`) and iterating it by`` |
|       - | 1155 | `` * reference (`foreach ($o[$k] as &$v)`). A compound assign is the reverse case —`` |
|       - | 1156 | ` * iP2 says write, but php compiles it to ASSIGN_DIM_OP, a read plus a write` |
|       - | 1157 | ` * through the container's own handlers (VmDimRmwArm), not a write FETCH.` |
|       - | 1158 | ` */` |
|     532 | 1159 | `static int VmIdxFetchForWrite(const VmInstr *pInstr,sxi32 iP2)` |
|       5 | 1160 | `{` |
|     537 | 1161 | `	const VmInstr *pNext = pInstr + 1;` |
|     537 | 1162 | `	if( iP2 == 1 ){` |
|     161 | 1163 | `		return !VmNextIsCompoundAssign(pNext);` |
|       - | 1164 | `	}` |
|     377 | 1165 | `	if( iP2 == VM_IDX_CTX_UNSET_BASE ){` |
|       - | 1166 | `		/* An INTERMEDIATE subscript of an unset chain: php fetches it for` |
|       - | 1167 | `		 * writing so the removal one level down can land. */` |
|       9 | 1168 | `		return 1;` |
|       - | 1169 | `	}` |
|     369 | 1170 | `	if( iP2 == 0 ){` |
|     249 | 1171 | `		if( pNext->iOp == PH7_OP_STORE_REF ){` |
|       9 | 1172 | `			return 1;` |
|       - | 1173 | `		}` |
|     241 | 1174 | `		if( pNext->iOp == PH7_OP_FOREACH_INIT && pNext->p3 ){` |
|       5 | 1175 | `			return (((ph7_foreach_info *)pNext->p3)->iFlags & PH7_4EACH_STEP_REF) != 0;` |
|       - | 1176 | `		}` |
|     116 | 1177 | `	}` |
|     357 | 1178 | `	return 0;` |
|     271 | 1179 | `}` |
|       - | 1180 | `/*` |
|       - | 1181 | ` * Which fetch contexts may answer out of a WRITABLE container's own storage` |
|       - | 1182 | `` * (PH7_SplDimElemSlot, vm_builtin_spl.c) instead of through `offsetGet`?`` |
|       - | 1183 | ` *` |
|       - | 1184 | ` * The ones that ask for a VALUE and may go on to MODIFY it: a plain read — whose` |
|       - | 1185 | ` * result carries the element's slot exactly as an array element's does, which is` |
|       - | 1186 | ` * what lets a by-reference ARGUMENT bind it — a write-context fetch, and the` |
|       - | 1187 | `` * INTERMEDIATE step of an unset chain. isset()/empty()/`??`/`??=` must reach`` |
|       - | 1188 | ` * offsetExists, and the OUTERMOST unset must reach offsetUnset, so those keep the` |
|       - | 1189 | ` * accessor. (iP2 is the NORMALIZED context here: the deferred-argument record mode` |
|       - | 1190 | ` * has already become a plain read.)` |
|       - | 1191 | ` *` |
|       - | 1192 | ` * A COMPOUND assign is deliberately not one of them, which is why this asks` |
|       - | 1193 | `` * VmIdxFetchForWrite rather than testing iP2 == 1 itself: `$ao[k] op= v` is php's`` |
|       - | 1194 | ` * ASSIGN_DIM_OP on an OBJECT, and that one reads and writes through the accessors` |
|       - | 1195 | ` * whatever the read handler could have offered — a subclass overriding only` |
|       - | 1196 | `` * offsetSet sees its own method called for `+=` and not for `++`.`` |
|       - | 1197 | ` */` |
|     462 | 1198 | `static int VmDimFastFetchCtx(const VmInstr *pInstr,sxi32 iP2)` |
|       5 | 1199 | `{` |
|     467 | 1200 | `	return iP2 == 0 \|\| VmIdxFetchForWrite(pInstr,iP2);` |
|       5 | 1201 | `}` |
|       - | 1202 | `/*` |
|       - | 1203 | `` * php's `Indirect modification of overloaded element of C has no effect`: the`` |
|       - | 1204 | ` * write-context fetch above landed on a container that answers with a COPY, so` |
|       - | 1205 | ` * whatever the rest of the expression writes is thrown away. php says so and` |
|       - | 1206 | ` * carries on.` |
|       - | 1207 | ` *` |
|       - | 1208 | ` * PHL had neither half. The notice was missing, and the copy was not a copy: a` |
|       - | 1209 | ` * userland offsetGet returns the container's own nested hashmap by COW, and` |
|       - | 1210 | ` * OP_STORE_IDX on a base with no slot index writes STRAIGHT INTO the shared map —` |
|       - | 1211 | `` * so `$o['a']['b'] = 9`, `$o['a'][] = 5` and `foreach ($o['a'] as &$v)` all`` |
|       - | 1212 | ` * modified the object php leaves untouched, silently. Separating the value here` |
|       - | 1213 | ` * is what makes the write land nowhere.` |
|       - | 1214 | ` *` |
|       - | 1215 | ` * php stays silent for an OBJECT, and so does this: an object is a handle, the` |
|       - | 1216 | ` * write through it is not lost, and nothing about it is indirect.` |
|       - | 1217 | ` */` |
|      40 | 1218 | `PH7_PRIVATE void PH7_VmOverloadedElemNotice(ph7_vm *pVm,ph7_class *pClass,ph7_value *pVal)` |
|       1 | 1219 | `{` |
|      41 | 1220 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       3 | 1221 | `		return;` |
|       - | 1222 | `	}` |
|      58 | 1223 | `	VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|       - | 1224 | `		"Indirect modification of overloaded element of %z has no effect",` |
|      19 | 1225 | `		&pClass->sName);` |
|      39 | 1226 | `	if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|      35 | 1227 | `		PH7_HashmapCowSeparate(&(*pVm),pVal);` |
|      17 | 1228 | `	}` |
|      21 | 1229 | `}` |
|       - | 1230 | `/*` |
|       - | 1231 | ` * OP_LOAD_IDX: body moved verbatim from the OP_LOAD_IDX arm of` |
|       - | 1232 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 1233 | ` */` |
| 1070253 | 1234 | `PH7_PRIVATE VmOpRc VmExecOpLoadIdx(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 1235 | `{` |
| 1070258 | 1236 | `	ph7_value *pTos = pState->pTos;` |
| 1070258 | 1237 | `	ph7_value *pStack = pState->pStack;` |
| 1070258 | 1238 | `	VmInstr *aInstr = pState->aInstr;` |
| 1070258 | 1239 | `	sxi32 pc = pState->pc;` |
|       - | 1240 | `	sxi32 rc;` |
|  536371 | 1241 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 1070258 | 1242 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
| 1070258 | 1243 | `	ph7_hashmap *pMap = 0;` |
|       - | 1244 | `	ph7_value *pIdx;` |
|       - | 1245 | `	/* D1 commit 2: iP2==9 is the deferred-record mode. It behaves exactly like a plain read` |
|       - | 1246 | `	 * (iP2==0) for base dispatch / the read tail, EXCEPT the dedicated block right after the` |
|       - | 1247 | `	 * index is popped, which — on a lookup MISS with a reachable base — captures the lvalue` |
|       - | 1248 | `	 * path (MEMOBJ_AUX_DEFPATH) instead of warning, and exits. Everything else sees iP2. */` |
| 1070258 | 1249 | `	sxi32 iP2 = (pInstr->iP2 == 9) ? 0 : pInstr->iP2;` |
| 1070258 | 1250 | `	pIdx = 0;` |
| 1070258 | 1251 | `	if( pInstr->iP1 == 0 ){` |
|      34 | 1252 | `		if( !iP2){` |
|       - | 1253 | ``			/* `[]` with nothing to append INTO. Every placement php refuses is a compile`` |
|       - | 1254 | `			 * error now (compile.c), so the only shape that reaches here is the one php` |
|       - | 1255 | `			 * also settles at runtime: a call ARGUMENT, whose parameter may turn out to be` |
|       - | 1256 | `			 * by-reference (php appends and binds) or by-value (php's Error). Record the` |
|       - | 1257 | `			 * append as a step of the deferred lvalue path and let OP_CALL decide. */` |
|       9 | 1258 | `			if( pInstr->iP2 == 9 ){` |
|       9 | 1259 | `				VmDeferredPath *pPath = 0;` |
|       9 | 1260 | `				if( pTos->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|     ! 0 | 1261 | `					pPath = (VmDeferredPath *)pTos->x.pOther;` |
|     ! 0 | 1262 | `					if( VmDeferPathPushAppend(pPath) == SXRET_OK ){` |
|     ! 0 | 1263 | `						VM_EXIT_BREAK; /* carrier already on pTos */` |
|     ! 0 | 1264 | `					}` |
|       9 | 1265 | `				}else if( pTos->iFlags & MEMOBJ_AUX_DEFERRED ){` |
|       - | 1266 | `					SyString sRootName;` |
|       3 | 1267 | `					SyStringInitFromBuf(&sRootName,(const char *)pTos->x.pOther,` |
|       - | 1268 | `						pTos->x.pOther ? SyStrlen((const char *)pTos->x.pOther) : 0);` |
|       3 | 1269 | `					pPath = VmDeferPathNew(&(*pVm),1,SXU32_HIGH,&sRootName);` |
|       3 | 1270 | `					if( pPath && VmDeferPathPushAppend(pPath) == SXRET_OK ){` |
|       3 | 1271 | `						pTos->iFlags &= ~MEMOBJ_AUX_DEFERRED; /* borrowed name; don't free x.pOther */` |
|       3 | 1272 | `						pTos->x.pOther = pPath;` |
|       3 | 1273 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|       3 | 1274 | `						pTos->nIdx = SXU32_HIGH;` |
|       3 | 1275 | `						VM_EXIT_BREAK;` |
|       - | 1276 | `					}` |
|     ! 0 | 1277 | `					VmFreeDeferredPath(pPath);` |
|       7 | 1278 | `				}else if( pTos->nIdx != SXU32_HIGH ){` |
|       7 | 1279 | `					pPath = VmDeferPathNew(&(*pVm),0,pTos->nIdx,0);` |
|       7 | 1280 | `					if( pPath && VmDeferPathPushAppend(pPath) == SXRET_OK ){` |
|       7 | 1281 | `						PH7_MemObjRelease(pTos);` |
|       7 | 1282 | `						pTos->x.pOther = pPath;` |
|       7 | 1283 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|       7 | 1284 | `						pTos->nIdx = SXU32_HIGH;` |
|       7 | 1285 | `						VM_EXIT_BREAK;` |
|       - | 1286 | `					}` |
|     ! 0 | 1287 | `					VmFreeDeferredPath(pPath);` |
|     ! 0 | 1288 | `				}` |
|     ! 0 | 1289 | `			}` |
|       - | 1290 | `			/* Not a deferrable argument (or out of memory recording it): php's own` |
|       - | 1291 | `			 * Error, which replaced PH7's notice-and-NULL. */` |
|     ! 0 | 1292 | `			if( pTos >= pStack ){` |
|     ! 0 | 1293 | `				PH7_MemObjRelease(pTos);` |
|     ! 0 | 1294 | `			}else{` |
|       - | 1295 | `				/* TICKET 1433-020: Empty stack */` |
|     ! 0 | 1296 | `				pTos++;` |
|     ! 0 | 1297 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|     ! 0 | 1298 | `				pTos->nIdx = SXU32_HIGH;` |
|       - | 1299 | `			}` |
|       - | 1300 | `			{` |
|     ! 0 | 1301 | `			sxi32 rcRd = VmThrowFromVm(&(*pVm),"Error","Cannot use [] for reading",` |
|       - | 1302 | `				sizeof("Cannot use [] for reading")-1);` |
|     ! 0 | 1303 | `			if( rcRd == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     ! 0 | 1304 | `			rc = rcRd;` |
|     ! 0 | 1305 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1306 | `			}` |
|       - | 1307 | `		}` |
|      14 | 1308 | `	}else{` |
| 1070226 | 1309 | `		pIdx = pTos;` |
| 1070226 | 1310 | `		pTos--;` |
|       - | 1311 | `	}` |
| 1070250 | 1312 | `	if( pInstr->iP2 == 9 && pIdx ){` |
|       - | 1313 | `		/* D1 commit 2 record mode. Decide whether to CAPTURE this subscript as a deferred` |
|       - | 1314 | `		 * lvalue step (the by-ref/by-value decision is not known until OP_CALL), or fall` |
|       - | 1315 | `		 * through to a plain read (iP2 was normalized to 0 above). We defer only when the` |
|       - | 1316 | `		 * base can be reached again at resolve time: an existing descriptor (nested), the` |
|       - | 1317 | `		 * commit-1 undefined-variable marker, or a real container/string/scalar slot` |
|       - | 1318 | `		 * (nIdx != SXU32_HIGH). On a present array key we DO NOT defer — a hit already` |
|       - | 1319 | `		 * yields the aliasable read slot the by-ref binder needs. */` |
|   69021 | 1320 | `		VmDeferredPath *pPath = 0;` |
|   69021 | 1321 | `		int bDefer = 0, eRoot = 0;` |
|   69021 | 1322 | `		if( pTos->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|       - | 1323 | `			/* Nested: the base already carries a descriptor — extend it in place. */` |
|      14 | 1324 | `			pPath = (VmDeferredPath *)pTos->x.pOther;` |
|      14 | 1325 | `			bDefer = 1;` |
|   69015 | 1326 | `		}else if( pTos->iFlags & MEMOBJ_AUX_DEFERRED ){` |
|       - | 1327 | `			/* Undefined base variable (commit-1 marker): root the descriptor by name. */` |
|       - | 1328 | `			SyString sRootName;` |
|       3 | 1329 | `			SyStringInitFromBuf(&sRootName,(const char *)pTos->x.pOther,` |
|       - | 1330 | `				pTos->x.pOther ? SyStrlen((const char *)pTos->x.pOther) : 0);` |
|       3 | 1331 | `			pPath = VmDeferPathNew(&(*pVm),1,SXU32_HIGH,&sRootName);` |
|       3 | 1332 | `			pTos->iFlags &= ~MEMOBJ_AUX_DEFERRED; /* borrowed name; don't free x.pOther */` |
|       3 | 1333 | `			pTos->x.pOther = 0;` |
|       3 | 1334 | `			bDefer = (pPath != 0);` |
|   69008 | 1335 | `		}else if( pTos->nIdx != SXU32_HIGH ){` |
|       - | 1336 | `			/* A real base slot: array/scalar -> eRoot 0 (by-ref may vivify), string -> 2. */` |
|   68933 | 1337 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       - | 1338 | `				/* Probe hit/miss on a COPY of the key: PH7_HashmapLookup casts a NULL key to` |
|       - | 1339 | `				 * "" in place, which would suppress the fall-through read's null-offset` |
|       - | 1340 | `				 * deprecation (hit) and capture the wrong key in the step (miss). */` |
|       - | 1341 | `				ph7_value idxProbe;` |
|   23943 | 1342 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   23943 | 1343 | `				PH7_MemObjInit(&(*pVm),&idxProbe);` |
|   23943 | 1344 | `				PH7_MemObjStore(pIdx,&idxProbe);` |
|   23943 | 1345 | `				if( PH7_HashmapLookup(pMap,&idxProbe,&pNode) == SXRET_OK ){` |
|   23885 | 1346 | `					bDefer = 0; /* present key: fall through and read it as an aliasable slot */` |
|   11945 | 1347 | `				}else{` |
|      61 | 1348 | `					eRoot = 0; bDefer = 1;` |
|       - | 1349 | `				}` |
|   23943 | 1350 | `				PH7_MemObjRelease(&idxProbe);` |
|   56964 | 1351 | `			}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       - | 1352 | `				/* An ArrayAccess base splits the way php's read_dimension does. A WRITABLE` |
|       - | 1353 | `				 * container answers out of its own storage with no accessor call, so the` |
|       - | 1354 | `				 * fetch really can wait for the callee: deferring it is what lets a` |
|       - | 1355 | `				 * by-reference argument take php's WRITE fetch, which CREATES a missing key` |
|       - | 1356 | ``				 * (`sort($ao['nokey'])`) instead of warning about a read and passing NULL.`` |
|       - | 1357 | `				 * Everything else answers through a METHOD, and php runs that method where` |
|       - | 1358 | `				 * the subscript is WRITTEN — so the accessor runs below and its RESULT rides` |
|       - | 1359 | `				 * a prefetch carrier built at the tail of the ArrayAccess branch. */` |
|     117 | 1360 | `				ph7_class_instance *pRecInst = (ph7_class_instance *)pTos->x.pOther;` |
|     117 | 1361 | `				eRoot = 0;` |
|     233 | 1362 | `				bDefer = (pRecInst && pVm->pArrayAccessClass` |
|     116 | 1363 | `				       && PH7_VmInstanceOf(pRecInst->pClass,pVm->pArrayAccessClass)` |
|     174 | 1364 | `				       && PH7_VmDimFetchWritable(pRecInst->pClass)) ? 1 : 0;` |
|   44937 | 1365 | `			}else if( pTos->iFlags & MEMOBJ_STRING ){` |
|   44879 | 1366 | `				eRoot = 2; bDefer = 1;` |
|   22611 | 1367 | `			}else{` |
|     ! 0 | 1368 | `				eRoot = 0; bDefer = 1; /* NULL/other reachable scalar base */` |
|       - | 1369 | `			}` |
|   68933 | 1370 | `			if( bDefer && pPath == 0 ){` |
|   45009 | 1371 | `				pPath = VmDeferPathNew(&(*pVm),eRoot,pTos->nIdx,0);` |
|   45009 | 1372 | `				if( pPath == 0 ){` |
|     ! 0 | 1373 | `					bDefer = 0;` |
|     ! 0 | 1374 | `				}` |
|   22671 | 1375 | `			}` |
|   34633 | 1376 | `		}` |
|   69021 | 1377 | `		if( bDefer ){` |
|       - | 1378 | `			/* Append this element step (deep-copies pIdx) and leave the carrier on pTos. */` |
|   45023 | 1379 | `			if( VmDeferPathPushElem(pPath,pIdx) == SXRET_OK ){` |
|   45023 | 1380 | `				if( (pTos->iFlags & MEMOBJ_AUX_DEFPATH) == 0 ){` |
|       - | 1381 | `					/* Collapse the base value into the descriptor carrier. */` |
|   45011 | 1382 | `					PH7_MemObjRelease(pTos);` |
|   45011 | 1383 | `					pTos->x.pOther = pPath;` |
|   45011 | 1384 | `					pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|   45011 | 1385 | `					pTos->nIdx = SXU32_HIGH;` |
|   22672 | 1386 | `				}` |
|   45023 | 1387 | `				PH7_MemObjRelease(pIdx);` |
|   45023 | 1388 | `				VM_EXIT_BREAK;` |
|       - | 1389 | `			}` |
|       - | 1390 | `			/* Out of memory appending a step. */` |
|     ! 0 | 1391 | `			if( pTos->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|       - | 1392 | `				/* Nested base: pPath IS pTos's carrier and stays owned by it — keep it (a` |
|       - | 1393 | `				 * step short) and exit, rather than fall through and misread a NULL-typed` |
|       - | 1394 | `				 * carrier as an array base. Resolves the shorter path; OOM-only degradation. */` |
|     ! 0 | 1395 | `				PH7_MemObjRelease(pIdx);` |
|     ! 0 | 1396 | `				VM_EXIT_BREAK;` |
|       - | 1397 | `			}` |
|       - | 1398 | `			/* A freshly-allocated path we still own: drop it and fall back to a plain read. */` |
|     ! 0 | 1399 | `			VmFreeDeferredPath(pPath);` |
|     ! 0 | 1400 | `		}` |
|   11999 | 1401 | `	}` |
| 1025232 | 1402 | `	if( iP2 == 7 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       - | 1403 | ``		/* Keyed list destructuring `["k"=>$v] = $src` from a NON-array source: yield NULL`` |
|       - | 1404 | `		 * (never char-index a string), warning once per key — matching PHP, which warns per` |
|       - | 1405 | `		 * key here. A NULL source is silent; unlike the positional OP_LOAD_LIST path, a bool` |
|       - | 1406 | `		 * source DOES warn (PHP warns for bool in keyed destructuring). */` |
|       7 | 1407 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       5 | 1408 | `			VmWarnCannotUseAsArray(&(*pVm),pTos->iFlags);` |
|       2 | 1409 | `		}` |
|       7 | 1410 | `		if( pIdx ){` |
|       - | 1411 | `			/* Release the key (a string literal for keyed destructuring), like the` |
|       - | 1412 | `			 * normal hashmap-read exit below — otherwise its blob is orphaned. */` |
|       7 | 1413 | `			PH7_MemObjRelease(pIdx);` |
|       3 | 1414 | `		}` |
|       7 | 1415 | `		PH7_MemObjRelease(pTos);` |
|       7 | 1416 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|       7 | 1417 | `		VM_EXIT_BREAK;` |
|       - | 1418 | `	}` |
| 1025226 | 1419 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|       - | 1420 | `		/* String access */` |
|  853062 | 1421 | `		if( VM_IDX_IS_UNSET(iP2) ){` |
|       - | 1422 | ``			/* php: a string offset cannot be unset AT ALL — `unset($s[0])` is the`` |
|       - | 1423 | ``			 * catchable `Error: Cannot unset string offsets`, whatever the offset is`` |
|       - | 1424 | `			 * and whether or not it is in range. PHL read the character but left the` |
|       - | 1425 | `			 * BASE VARIABLE's slot index on the result, so the trailing unset()` |
|       - | 1426 | ``			 * builtin freed the base itself: `$s = "abc"; unset($s[1]);` left $s`` |
|       - | 1427 | ``			 * UNDEFINED, and `unset($a["k"][0])` deleted the whole element. */`` |
|      11 | 1428 | `			rc = VmThrowFromVm(&(*pVm),"Error","Cannot unset string offsets",` |
|       - | 1429 | `				sizeof("Cannot unset string offsets")-1);` |
|      11 | 1430 | `			if( pIdx ){` |
|      11 | 1431 | `				PH7_MemObjRelease(pIdx);` |
|       5 | 1432 | `			}` |
|      11 | 1433 | `			PH7_MemObjRelease(pTos);` |
|      11 | 1434 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      11 | 1435 | `			pTos->nIdx = SXU32_HIGH;` |
|      11 | 1436 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      11 | 1437 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1438 | `		}` |
|  853052 | 1439 | `		if( pIdx ){` |
|  853052 | 1440 | `			sxi64 iOfft = 0, iRaw;` |
|  853052 | 1441 | `			sxi64 nLen = (sxi64)SyBlobLength(&pTos->sBlob);` |
|       - | 1442 | `			/* LOAD_IDX carries its OWN iP2 codes, which do NOT line up with the` |
|       - | 1443 | ``			 * PH7_MEMBER_* ones: 4 = isset, 5 = unset, 6 = empty, 8 = `??` read,`` |
|       - | 1444 | ``			 * 3 = `??=` peek. All are LOOKUPS, but php splits them into TWO`` |
|       - | 1445 | `			 * levels: isset()/empty()/unset() say nothing at all, while a` |
|       - | 1446 | ``			 * `??`/`??=` fetch still warns about the offset SHAPE and reads it`` |
|       - | 1447 | `			 * (VM_STROFF_COALESCE). The ??= peek is recognised by the NULLC_JMP` |
|       - | 1448 | `			 * that follows it, since its iP2 does not distinguish the base type. */` |
| 1280596 | 1449 | `			int iOfftLevel = (iP2 == 4 \|\| VM_IDX_IS_UNSET(iP2) \|\| iP2 == 6) ? VM_STROFF_ISSET` |
| 1703920 | 1450 | `				: ((iP2 == 8 \|\| VmIdxFeedsCoalesce(pInstr)) ? VM_STROFF_COALESCE` |
|  850887 | 1451 | `				: VM_STROFF_LOUD);` |
|  853052 | 1452 | `			int bQuiet = iOfftLevel != VM_STROFF_LOUD;` |
|       - | 1453 | `			SyBlob sTypeMsg;` |
|       - | 1454 | `			int eOfft;` |
|  853052 | 1455 | `			VmCoalStrOff *pCoalOff = 0;` |
|  853052 | 1456 | `			if( VmIdxFeedsCoalesceAssign(pInstr) ){` |
|       - | 1457 | ``				/* `$s[k] ??= v`: the OP_NULLC_STORE ahead has to write into the`` |
|       - | 1458 | `				 * string OFFSET, and by then the offset is gone — this op consumes` |
|       - | 1459 | `				 * it. Carry the RAW index to the store on the peek's own result` |
|       - | 1460 | `				 * (MEMOBJ_AUX_COALSTROFF), which nests and cannot leak; the copy` |
|       - | 1461 | `				 * must predate the resolution below, which casts a float/null/bool` |
|       - | 1462 | `				 * in place, because php re-resolves the offset LOUDLY at the store:` |
|       - | 1463 | `				 * the peek is the quiet half of its pair. */` |
|      53 | 1464 | `				pCoalOff = VmCoalStrOffNew(&(*pVm),pIdx);` |
|      26 | 1465 | `			}` |
|  853052 | 1466 | `			eOfft = VmStringOffsetResolve(&(*pVm),pIdx,iOfftLevel,&iOfft,&sTypeMsg);` |
|  853052 | 1467 | `			if( eOfft == VM_STROFF_MISS ){` |
|       - | 1468 | `				/* A lookup over an offset php refuses: not set, in silence. */` |
|      32 | 1469 | `				PH7_MemObjRelease(pIdx);` |
|      32 | 1470 | `				PH7_MemObjRelease(pTos);` |
|      32 | 1471 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      32 | 1472 | `				if( pCoalOff ){` |
|       - | 1473 | ``					/* A `??=` whose offset the READ refuses: php raises at the`` |
|       - | 1474 | `					 * STORE instead, so keep the base slot reachable and hand the` |
|       - | 1475 | `					 * offset to OP_NULLC_STORE. */` |
|       3 | 1476 | `					pTos->x.pOther = (void *)pCoalOff;` |
|       3 | 1477 | `					pTos->iFlags \|= MEMOBJ_AUX_STROFFSET\|MEMOBJ_AUX_COALSTROFF;` |
|       2 | 1478 | `				}else{` |
|      30 | 1479 | `					pTos->nIdx = SXU32_HIGH;` |
|       - | 1480 | `				}` |
|      50 | 1481 | `				VM_EXIT_BREAK;` |
|       - | 1482 | `			}` |
|  853022 | 1483 | `			if( eOfft == VM_STROFF_REJECT ){` |
|       - | 1484 | `				/* php's TypeError for an offset type a string refuses. PHL cast` |
|       - | 1485 | ``				 * every offset to int, so `$s[""]`, `$s["-"]` and `$s["p"]` all`` |
|       - | 1486 | ``				 * answered `$s[0]`. Routed as a mid-expression throw (this opcode`` |
|       - | 1487 | `				 * is not a call boundary), so the rest of the expression is` |
|       - | 1488 | `				 * abandoned the way php abandons it. */` |
|      40 | 1489 | `				VmFreeCoalStrOff(pCoalOff);` |
|      40 | 1490 | `				rc = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|      40 | 1491 | `				PH7_MemObjRelease(pIdx);` |
|      40 | 1492 | `				PH7_MemObjRelease(pTos);` |
|      40 | 1493 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      40 | 1494 | `				pTos->nIdx = SXU32_HIGH;` |
|      40 | 1495 | `				if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      40 | 1496 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1497 | `			}` |
|  852986 | 1498 | `			iRaw = iOfft;` |
|       - | 1499 | `			/* php 7.1: a NEGATIVE offset counts back from the end ($s[-1] is the last` |
|       - | 1500 | `			 * character). The offset used to be cast to UNSIGNED, so -1 became a huge` |
|       - | 1501 | `			 * number, ran past the end and quietly produced NULL. */` |
|  852986 | 1502 | `			if( iOfft < 0 ){` |
|      18 | 1503 | `				iOfft += nLen;` |
|       8 | 1504 | `			}` |
|  852986 | 1505 | `			if( iOfft < 0 \|\| iOfft >= nLen ){` |
|       - | 1506 | `				/* Out of range. In an isset()/empty() lookup php answers FALSE, so load` |
|       - | 1507 | `				 * NULL there; everywhere else it WARNS and yields the empty string (PH7` |
|       - | 1508 | `				 * silently produced NULL in both cases). */` |
|      65 | 1509 | `				PH7_MemObjRelease(pTos);` |
|      65 | 1510 | `				if( bQuiet ){` |
|      56 | 1511 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      29 | 1512 | `				}else{` |
|      10 | 1513 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|      14 | 1514 | `					VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Uninitialized string offset %qd",` |
|       4 | 1515 | `						iRaw);` |
|       - | 1516 | `				}` |
|      34 | 1517 | `			}else{` |
|  852924 | 1518 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|  852924 | 1519 | `				int c = zData[iOfft];` |
|  852924 | 1520 | `				PH7_MemObjRelease(pTos);` |
|  852924 | 1521 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|  852924 | 1522 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|       - | 1523 | `			}` |
|  852986 | 1524 | `			if( pCoalOff ){` |
|      49 | 1525 | `				if( pTos->iFlags & MEMOBJ_NULL ){` |
|       - | 1526 | ``					/* Out of range: the `??=` will store, so hand the offset over. */`` |
|      41 | 1527 | `					pTos->x.pOther = (void *)pCoalOff;` |
|      41 | 1528 | `					pTos->iFlags \|= MEMOBJ_AUX_COALSTROFF;` |
|      21 | 1529 | `				}else{` |
|       - | 1530 | ``					/* A real byte: the `??=` short-circuits over the store. */`` |
|       9 | 1531 | `					VmFreeCoalStrOff(pCoalOff);` |
|       - | 1532 | `				}` |
|      24 | 1533 | `			}` |
|       - | 1534 | `			/* The result still carries the BASE VARIABLE's slot index, which is` |
|       - | 1535 | `			 * harmless for a plain read and WRONG for anything that would ALIAS it:` |
|       - | 1536 | `			 * a string offset is not a slot. Mark it so the reference-binding sites` |
|       - | 1537 | `			 * raise php's Error instead of aliasing the whole string --` |
|       - | 1538 | ``			 * `$r = &$s[1]; $r = "Z";` REPLACED $s with "Z". */`` |
|  852986 | 1539 | `			pTos->iFlags \|= MEMOBJ_AUX_STROFFSET;` |
|  427548 | 1540 | `		}else{` |
|       - | 1541 | `			/* No available index,load NULL */` |
|     ! 0 | 1542 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       - | 1543 | `		}` |
|  852986 | 1544 | `		VM_EXIT_BREAK;` |
|       - | 1545 | `	}` |
|  172169 | 1546 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       - | 1547 | `		/* Object subscript: ArrayAccess dispatch.` |
|       - | 1548 | `		 * iP2 codes:` |
|       - | 1549 | `		 *   0 = read       → offsetGet` |
|       - | 1550 | `		 *   3 = ??= peek   → offsetExists; offsetGet on hit; arm coalesce` |
|       - | 1551 | `		 *                    target on miss for the upcoming NULLC_STORE` |
|       - | 1552 | `		 *   4 = isset()    → offsetExists` |
|       - | 1553 | `		 *   5 = unset()    → offsetUnset` |
|       - | 1554 | `		 *   6 = empty()    → offsetExists, then offsetGet on hit` |
|       - | 1555 | ``		 *   8 = `??` read  → same probe as 6 (offsetExists, then offsetGet on a`` |
|       - | 1556 | `		 *                    hit) and no diagnostics anywhere in this op: php` |
|       - | 1557 | ``		 *                    evaluates the whole left operand of `??` in`` |
|       - | 1558 | `		 *                    isset-context, and calling offsetGet blindly also` |
|       - | 1559 | `		 *                    surfaced warnings raised INSIDE a userland` |
|       - | 1560 | `		 *                    offsetGet (e.g. ArrayObject's own array read). */` |
|     473 | 1561 | `		ph7_class_instance *pInst = (ph7_class_instance *)pTos->x.pOther;` |
|     473 | 1562 | `		ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|     473 | 1563 | `		if( pArrayAccess && pInst && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|       - | 1564 | `			ph7_class_method *pMeth;` |
|       - | 1565 | `			ph7_value sResult;` |
|       - | 1566 | `			ph7_value sNullIdx;` |
|       - | 1567 | `			ph7_value *apArg[1];` |
|     464 | 1568 | `			if( pIdx && VmDimFastFetchCtx(pInstr,iP2) && PH7_VmDimFetchWritable(pInst->pClass)` |
|     218 | 1569 | `			 && pInst->iRef > 1 ){` |
|       - | 1570 | `				/* php hands a writable container's element back BY SLOT, and that is what` |
|       - | 1571 | ``				 * makes an indirect modification through it land. The `iRef > 1` guard is`` |
|       - | 1572 | ``				 * the object half of the array path's `pMap->iRef < 2` rule: releasing the`` |
|       - | 1573 | `				 * base below drops this stack slot's own reference, and a TEMPORARY` |
|       - | 1574 | ``				 * container (`(new ArrayObject([1]))[0]`) would be destroyed with its`` |
|       - | 1575 | `				 * storage while the result still views it. Such a base has nothing that` |
|       - | 1576 | `				 * could observe the write anyway, so it takes the accessor's copy. */` |
|     174 | 1577 | `				sxu32 nElem = PH7_SplDimElemSlot(&(*pVm),pInst,pIdx,` |
|       - | 1578 | `					/* php's write-context vivification, and only there: a W/RW fetch —` |
|       - | 1579 | ``					 * including the `$r = &$ao['k']` and by-ref-foreach shapes iP2 alone`` |
|       - | 1580 | `					 * cannot name — creates the missing element, while an unset chain's` |
|       - | 1581 | `					 * intermediate step never does. */` |
|     147 | 1582 | `					VmIdxFetchForWrite(pInstr,iP2) && !VM_IDX_IS_UNSET(iP2));` |
|     124 | 1583 | `				ph7_value *pElem = (nElem == SXU32_HIGH) ? 0` |
|     118 | 1584 | `					: (ph7_value *)SySetAt(&pVm->aMemObj,nElem);` |
|     124 | 1585 | `				if( pElem ){` |
|     116 | 1586 | `					PH7_MemObjRelease(pTos);` |
|     116 | 1587 | `					PH7_MemObjLoad(pElem,pTos);` |
|     116 | 1588 | `					pTos->nIdx = nElem;` |
|     116 | 1589 | `					PH7_MemObjRelease(pIdx);` |
|     116 | 1590 | `					VM_EXIT_BREAK;` |
|       - | 1591 | `				}` |
|       4 | 1592 | `			}` |
|     355 | 1593 | `			if( (iP2 == 0 \|\| iP2 == 3 \|\| iP2 == 8) && pIdx == 0 ){` |
|       - | 1594 | ``				/* `$obj[]` read — PHP rejects this. */`` |
|     ! 0 | 1595 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|       - | 1596 | `					"Cannot use [] for reading");` |
|     ! 0 | 1597 | `				PH7_MemObjRelease(pTos);` |
|     ! 0 | 1598 | `				pTos->nIdx = SXU32_HIGH;` |
|     ! 0 | 1599 | `				VM_EXIT_BREAK;` |
|       - | 1600 | `			}` |
|     355 | 1601 | `			PH7_MemObjInit(&(*pVm),&sResult);` |
|     355 | 1602 | `			if( iP2 == 4 \|\| iP2 == 6 \|\| iP2 == 3 \|\| iP2 == 8 ){` |
|       - | 1603 | ``				/* isset, empty, ??= and `??` all start with offsetExists. */`` |
|      93 | 1604 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 1605 | `					"offsetExists",sizeof("offsetExists")-1);` |
|      93 | 1606 | `				apArg[0] = pIdx;` |
|      93 | 1607 | `				if( pMeth ){` |
|      93 | 1608 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|      49 | 1609 | `				}` |
|     311 | 1610 | `			}else if( iP2 == 5 ){` |
|      36 | 1611 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 1612 | `					"offsetUnset",sizeof("offsetUnset")-1);` |
|      36 | 1613 | `				apArg[0] = pIdx;` |
|      36 | 1614 | `				if( pMeth ){` |
|      36 | 1615 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|      16 | 1616 | `				}` |
|      20 | 1617 | `			}else{` |
|     235 | 1618 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 1619 | `					"offsetGet",sizeof("offsetGet")-1);` |
|     235 | 1620 | `				if( pIdx == 0 ){` |
|       - | 1621 | ``					/* `$o[] op= v` — the one read that reaches here without a key.`` |
|       - | 1622 | `					 * php hands the accessors NULL for the absent offset (its` |
|       - | 1623 | `					 * read_dimension substitutes one), so passing NO argument` |
|       - | 1624 | `					 * turned an assignment php performs into an` |
|       - | 1625 | `					 * ArgumentCountError against the class's own offsetGet. */` |
|       3 | 1626 | `					PH7_MemObjInit(&(*pVm),&sNullIdx);` |
|       3 | 1627 | `					pIdx = &sNullIdx;` |
|       1 | 1628 | `				}` |
|     235 | 1629 | `				apArg[0] = pIdx;` |
|     235 | 1630 | `				if( pMeth ){` |
|     235 | 1631 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|     115 | 1632 | `				}` |
|       - | 1633 | `			}` |
|     355 | 1634 | `			if( iP2 == 4 ){` |
|       - | 1635 | `				/* isset: push MEMOBJ_BOOL so vm_builtin_isset reports the` |
|       - | 1636 | `				 * right truth value AND skips its "Expecting a variable not` |
|       - | 1637 | `				 * a constant" warning (keyed on MEMOBJ_BOOL). */` |
|      63 | 1638 | `				int bExists = ph7_value_to_bool(&sResult);` |
|      63 | 1639 | `				PH7_MemObjRelease(pTos);` |
|      63 | 1640 | `				pTos->nIdx = SXU32_HIGH;` |
|      63 | 1641 | `				if( bExists ){` |
|      34 | 1642 | `					MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      34 | 1643 | `					pTos->x.iVal = 1;` |
|      19 | 1644 | `				}else{` |
|      33 | 1645 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       5 | 1646 | `				}` |
|     326 | 1647 | `			}else if( iP2 == 5 ){` |
|       - | 1648 | `				/* offsetUnset return is discarded; push NULL so the trailing` |
|       - | 1649 | `				 * vm_builtin_unset is a harmless no-op. */` |
|      36 | 1650 | `				PH7_MemObjRelease(pTos);` |
|      36 | 1651 | `				pTos->nIdx = SXU32_HIGH;` |
|      36 | 1652 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|     281 | 1653 | `			}else if( iP2 == 6 \|\| iP2 == 8 ){` |
|       - | 1654 | `				/* empty: if offsetExists is false, push NULL so empty=true` |
|       - | 1655 | `				 * without calling offsetGet. If true, call offsetGet and` |
|       - | 1656 | `				 * push the value so PH7_builtin_empty evaluates emptiness.` |
|       - | 1657 | ``				 * `??` (8) needs the identical shape: NULL on a miss so the`` |
|       - | 1658 | `				 * coalesce takes the default, the real value on a hit. */` |
|      25 | 1659 | `				int bExists = ph7_value_to_bool(&sResult);` |
|      25 | 1660 | `				PH7_MemObjRelease(&sResult);` |
|      25 | 1661 | `				PH7_MemObjRelease(pTos);` |
|      25 | 1662 | `				pTos->nIdx = SXU32_HIGH;` |
|      25 | 1663 | `				if( !bExists ){` |
|       8 | 1664 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       5 | 1665 | `				}else{` |
|      19 | 1666 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 1667 | `						"offsetGet",sizeof("offsetGet")-1);` |
|       - | 1668 | `					ph7_value sValue;` |
|      19 | 1669 | `					PH7_MemObjInit(&(*pVm),&sValue);` |
|      19 | 1670 | `					apArg[0] = pIdx;` |
|      19 | 1671 | `					if( pGet ){` |
|      19 | 1672 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|       8 | 1673 | `					}` |
|      19 | 1674 | `					PH7_MemObjStore(&sValue,pTos);` |
|      19 | 1675 | `					PH7_MemObjRelease(&sValue);` |
|       - | 1676 | `				}` |
|      25 | 1677 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|      25 | 1678 | `				VM_EXIT_BREAK; /* skip the duplicate sResult release below */` |
|     243 | 1679 | `			}else if( iP2 == 3 ){` |
|       - | 1680 | `				/* ?? null-coalesce peek: emulate PHP semantics —` |
|       - | 1681 | `				 *   if !offsetExists OR offsetGet() === null → arm` |
|       - | 1682 | `				 *     coalesce slot (NULLC_STORE will call offsetSet)` |
|       - | 1683 | `				 *     and push NULL.` |
|       - | 1684 | `				 *   else → push offsetGet's value (NULLC_JMP skips). */` |
|      10 | 1685 | `				int bExists = ph7_value_to_bool(&sResult);` |
|      10 | 1686 | `				int bShouldArm = !bExists;` |
|       - | 1687 | `				ph7_value sValue;` |
|      10 | 1688 | `				PH7_MemObjRelease(&sResult);` |
|       - | 1689 | `				/* Reset any prior arming defensively */` |
|      10 | 1690 | `				VmCoalesceDisarm(pVm);` |
|      10 | 1691 | `				PH7_MemObjInit(&(*pVm),&sValue);` |
|      10 | 1692 | `				if( bExists ){` |
|       5 | 1693 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 1694 | `						"offsetGet",sizeof("offsetGet")-1);` |
|       5 | 1695 | `					apArg[0] = pIdx;` |
|       5 | 1696 | `					if( pGet ){` |
|       5 | 1697 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|       2 | 1698 | `					}` |
|       5 | 1699 | `					if( sValue.iFlags & MEMOBJ_NULL ){` |
|       3 | 1700 | `						bShouldArm = 1;` |
|       1 | 1701 | `					}` |
|       2 | 1702 | `				}` |
|      10 | 1703 | `				PH7_MemObjRelease(pTos);` |
|      10 | 1704 | `				pTos->nIdx = SXU32_HIGH;` |
|      10 | 1705 | `				if( bShouldArm ){` |
|       - | 1706 | `					/* Arm: remember (object, key) so NULLC_STORE dispatches` |
|       - | 1707 | `					 * to offsetSet. Hold a ref on the instance to survive` |
|       - | 1708 | `					 * intervening expression evaluation. */` |
|       8 | 1709 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       8 | 1710 | `					if( pIdx ){` |
|       8 | 1711 | `						PH7_MemObjStore(pIdx,&pVm->sCoalesceKey);` |
|       3 | 1712 | `					}` |
|       8 | 1713 | `					pVm->pCoalesceObj = pInst;` |
|       8 | 1714 | `					pInst->iRef++;` |
|       8 | 1715 | `					pVm->bCoalesceArmed = 1;` |
|       5 | 1716 | `				}else{` |
|       3 | 1717 | `					PH7_MemObjStore(&sValue,pTos);` |
|       - | 1718 | `				}` |
|      10 | 1719 | `				PH7_MemObjRelease(&sValue);` |
|      10 | 1720 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|      10 | 1721 | `				VM_EXIT_BREAK;` |
|     ! 0 | 1722 | `			}else{` |
|       - | 1723 | `				/* offsetGet: replace pTos with the returned value.` |
|       - | 1724 | `				 *` |
|       - | 1725 | `				 * The base slot may be the only thing holding this instance — a` |
|       - | 1726 | ``				 * TEMPORARY container (`f((new C)['a'])`, a getter's return) dies with`` |
|       - | 1727 | `				 * it — and everything below still speaks for the object: the writable` |
|       - | 1728 | `				 * test, php's notice and the read-modify-write arming all read its` |
|       - | 1729 | `				 * CLASS, and the deferred-argument carrier records it. Hold a reference` |
|       - | 1730 | `				 * of our own across the release so none of them is left reading freed` |
|       - | 1731 | `				 * memory. */` |
|     235 | 1732 | `				pInst->iRef++;` |
|     235 | 1733 | `				PH7_MemObjRelease(pTos);` |
|     235 | 1734 | `				PH7_MemObjStore(&sResult,pTos);` |
|     235 | 1735 | `				pTos->nIdx = SXU32_HIGH;` |
|     235 | 1736 | `				if( iP2 == 1 && VmNextIsCompoundAssign(pInstr + 1) ){` |
|       - | 1737 | ``					/* `$o[$k] op= v` is php's ASSIGN_DIM_OP: offsetGet gave the`` |
|       - | 1738 | `					 * current value, the op computes on it, and the result goes` |
|       - | 1739 | `					 * back through offsetSet($k, …). PHL had no write-back at` |
|       - | 1740 | `					 * all here — the fetched value carried no slot, so every` |
|       - | 1741 | `					 * compound assign on an ArrayAccess element died on` |
|       - | 1742 | `					 * "Cannot perform assignment on a constant class attribute"` |
|       - | 1743 | `					 * and stored nothing. Arm the scratch slot the op mutates;` |
|       - | 1744 | `					 * its tail (PH7_HOOK_RMW_WRITEBACK) dispatches offsetSet. */` |
|      64 | 1745 | `					VmDimRmwArm(&(*pVm),pInst,pIdx,pTos,` |
|      42 | 1746 | `						(void *)pStack,(void *)aInstr,(sxu32)(pc + 1));` |
|     210 | 1747 | `				}else if( VmIdxFetchForWrite(pInstr,iP2)` |
|     113 | 1748 | `				       && !PH7_VmDimFetchWritable(pInst->pClass) ){` |
|      25 | 1749 | `					PH7_VmOverloadedElemNotice(&(*pVm),pInst->pClass,pTos);` |
|     181 | 1750 | `				}else if( pInstr->iP2 == 9 ){` |
|       - | 1751 | `					/* A deferred call ARGUMENT. The accessor has just run — php runs it` |
|       - | 1752 | `					 * where the subscript is written, whatever the parameter turns out to` |
|       - | 1753 | `					 * be — but WHICH fetch php performed is the callee's to say, and only` |
|       - | 1754 | `					 * OP_CALL knows: a by-reference parameter makes it a W fetch, which on` |
|       - | 1755 | `					 * a container that can only answer with a VALUE is php's` |
|       - | 1756 | ``					 * `Indirect modification of overloaded element` notice and a write`` |
|       - | 1757 | `					 * thrown away. Carry the result plus the class that answered it, so the` |
|       - | 1758 | `					 * verdict lands at the call without the accessor running twice or the` |
|       - | 1759 | `					 * argument arriving as NULL. The value would otherwise reach the callee` |
|       - | 1760 | `					 * still SHARING the container's own nested map by COW, and a by-ref` |
|       - | 1761 | ``					 * `f($o['a']['b'])` wrote straight into the object php leaves untouched. */`` |
|      47 | 1762 | `					VmDeferredPath *pPre = VmDeferPathNewPrefetch(&(*pVm),VM_OVER_ELEM,pInst->pClass,0,pTos);` |
|      47 | 1763 | `					if( pPre ){` |
|      47 | 1764 | `						PH7_MemObjRelease(pTos);` |
|      47 | 1765 | `						pTos->x.pOther = pPre;` |
|      47 | 1766 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|      47 | 1767 | `						pTos->nIdx = SXU32_HIGH;` |
|      23 | 1768 | `					}` |
|      23 | 1769 | `				}` |
|     235 | 1770 | `				PH7_ClassInstanceUnref(pInst);` |
|       - | 1771 | `			}` |
|     325 | 1772 | `			PH7_MemObjRelease(&sResult);` |
|     325 | 1773 | `			if( pIdx ){` |
|     325 | 1774 | `				PH7_MemObjRelease(pIdx);` |
|     160 | 1775 | `			}` |
|     325 | 1776 | `			VM_EXIT_BREAK;` |
|       - | 1777 | `		}` |
|       - | 1778 | `		/* Object without ArrayAccess: PHP throws fatal Error in all subscript` |
|       - | 1779 | `		 * contexts (read, isset, unset, empty). Match it. */` |
|       6 | 1780 | `		if( pInst ){` |
|       - | 1781 | `			char zMsg[256];` |
|       6 | 1782 | `			SyString *pName = &pInst->pClass->sName;` |
|       8 | 1783 | `			sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - | 1784 | `				"Cannot use object of type %.*s as array",` |
|       4 | 1785 | `				(int)pName->nByte,pName->zString);` |
|       6 | 1786 | `			rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|       6 | 1787 | `			if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|       6 | 1788 | `			PH7_MemObjRelease(pTos);` |
|       6 | 1789 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       6 | 1790 | `			pTos->nIdx = SXU32_HIGH;` |
|       6 | 1791 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       - | 1792 | ``			/* `break` used to resume at the NEXT instruction: the catch ran and then`` |
|       - | 1793 | `			 * execution carried on inside the try block. */` |
|       8 | 1794 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1795 | `		}` |
|     ! 0 | 1796 | `	}` |
|  171701 | 1797 | `	if( (iP2 == 1 \|\| iP2 == 3 \|\| VM_IDX_IS_UNSET(iP2)) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      47 | 1798 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|       - | 1799 | `			ph7_value *pObj;` |
|      43 | 1800 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       - | 1801 | `				/* php 8 write-context auto-vivify rules: NULL converts to array` |
|       - | 1802 | `				 * silently; FALSE converts with the 8.1 deprecation; any other` |
|       - | 1803 | `				 * scalar base — int/float/true/resource — is php's catchable` |
|       - | 1804 | `				 * "Cannot use a scalar value as an array" Error and the variable` |
|       - | 1805 | `				 * stays untouched (pre-fix the base was silently CONVERTED,` |
|       - | 1806 | ``				 * corrupting e.g. `$i = 5; $i[0]++` into array(0 => 6); string`` |
|       - | 1807 | `				 * bases were intercepted by the string-offset paths above). */` |
|       - | 1808 | `				/* php auto-converts false to an array with an 8.1 DEPRECATION; PHL` |
|       - | 1809 | `				 * rejects it like any other scalar base (null still auto-vivifies —` |
|       - | 1810 | `				 * it is not a bool). */` |
|      43 | 1811 | `				if( (pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_RES\|MEMOBJ_BOOL)) != 0 ){` |
|       - | 1812 | `					/* unset() has its own wording for the same base: php's` |
|       - | 1813 | `					 * "Cannot unset offset in a non-array variable". */` |
|      12 | 1814 | `					const char *zErr = VM_IDX_IS_UNSET(iP2)` |
|       - | 1815 | `						? "Cannot unset offset in a non-array variable"` |
|      10 | 1816 | `						: "Cannot use a scalar value as an array";` |
|       - | 1817 | `					SyBlob sErrMsg;` |
|      16 | 1818 | `					SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      16 | 1819 | `					SyBlobAppend(&sErrMsg,zErr,(sxu32)SyStrlen(zErr));` |
|      16 | 1820 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      16 | 1821 | `					if( pIdx ){` |
|      16 | 1822 | `						PH7_MemObjRelease(pIdx);` |
|       7 | 1823 | `					}` |
|      16 | 1824 | `					PH7_MemObjRelease(pTos);` |
|      16 | 1825 | `					pTos->nIdx = SXU32_HIGH;` |
|      16 | 1826 | `					VM_EXIT_BREAK;` |
|       - | 1827 | `				}` |
|       - | 1828 | `				/* unset() CREATES NOTHING: a null/undefined base stays null where PHL` |
|       - | 1829 | ``				 * converted it to an empty array (`$n = null; unset($n[0]);` left`` |
|       - | 1830 | `				 * $n === []), which is also what materialised the missing intermediate` |
|       - | 1831 | ``				 * in `unset($a["y"]["z"])`. Leaving the base alone, the lookup below`` |
|       - | 1832 | `				 * misses, the tail loads NULL with no slot index, and the trailing` |
|       - | 1833 | `				 * unset() builtin is the no-op php's is. */` |
|      29 | 1834 | `				if( !VM_IDX_IS_UNSET(iP2) ){` |
|      26 | 1835 | `					PH7_MemObjToHashmap(pObj);` |
|      26 | 1836 | `					PH7_MemObjLoad(pObj,pTos);` |
|      12 | 1837 | `				}` |
|      13 | 1838 | `			}` |
|      13 | 1839 | `		}` |
|      15 | 1840 | `	}` |
|  171687 | 1841 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|       - | 1842 | `	/* php DEPRECATES both a null offset and a lossy-float subscript (then normalizes` |
|       - | 1843 | `	 * "" / truncates). PHL now matches php on the NULL offset (deprecate + coerce to` |
|       - | 1844 | `	 * the "" key, §2) but still rejects the lossy-FLOAT subscript with a TypeError on` |
|       - | 1845 | `	 * a READ or WRITE (iP2 0/1) — the recorded non-deprecated-surface policy. Both` |
|       - | 1846 | ``	 * stay lenient in isset()/empty()/`??`/unset(), where a throw would be wrong. */`` |
|       - | 1847 | `	/* An object/array key is rejected in EVERY context, including isset()/empty()/` |
|       - | 1848 | `	 * unset() where php still throws (only the wording changes) — unlike the` |
|       - | 1849 | `	 * null/float deprecations below, which stay lenient there. A resource key is` |
|       - | 1850 | `	 * accepted with a warning and becomes its integer id. */` |
|  171687 | 1851 | `	if( (pTos->iFlags & MEMOBJ_HASHMAP) && pIdx ){` |
|       - | 1852 | `		SyBlob sTypeMsg;` |
|  171595 | 1853 | `		if( VmOffsetTypeRejected(&(*pVm),pIdx,iP2,&sTypeMsg) ){` |
|       - | 1854 | `			/* Routed as a mid-expression throw, like the STRING arm above: this` |
|       - | 1855 | `			 * opcode is not a call boundary, so PARKING it let the rest of the` |
|       - | 1856 | ``			 * expression run first — `str_repeat($a[$obj], 2)` reached the builtin`` |
|       - | 1857 | `			 * with the NULL the abandoned read left and died on its ZPP TypeError` |
|       - | 1858 | `			 * instead, and a CAUGHT one still ran the enclosing call afterwards. */` |
|      22 | 1859 | `			rc = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|      22 | 1860 | `			PH7_MemObjRelease(pIdx);` |
|      22 | 1861 | `			PH7_MemObjRelease(pTos);` |
|      22 | 1862 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      22 | 1863 | `			pTos->nIdx = SXU32_HIGH;` |
|      31 | 1864 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      20 | 1865 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1866 | `		}` |
|  171575 | 1867 | `		VmOffsetResourceWarn(&(*pVm),pIdx);` |
|   85785 | 1868 | `	}` |
|  171667 | 1869 | `	if( (pTos->iFlags & MEMOBJ_HASHMAP) && pIdx ){` |
|       - | 1870 | `		/* php DEPRECATES a null offset in EVERY subscript context except unset()` |
|       - | 1871 | `		 * (iP2==5), then normalizes it to the "" key — read, write, isset (4),` |
|       - | 1872 | ``		 * empty (6), `??`/`??=` (3), the coalesce-chain probe (8), destructure`` |
|       - | 1873 | `		 * (2/7). Emit it here so all of them get it, not just read/write; the` |
|       - | 1874 | `		 * lookup/insert below casts NULL->"", and a plain read miss then warns` |
|       - | 1875 | ``		 * `Undefined array key ""` on the now-string pIdx (php's exact pair). */`` |
|  171575 | 1876 | `		if( (pIdx->iFlags & MEMOBJ_NULL) && !VM_IDX_IS_UNSET(iP2) ){` |
|      21 | 1877 | `			VmNullOffsetDeprecate(&(*pVm),pIdx);` |
|       9 | 1878 | `		}` |
|       - | 1879 | `		/* A lossy-FLOAT subscript stays a rejected TypeError on a READ or WRITE` |
|       - | 1880 | `		 * (iP2 0/1) — the recorded non-deprecated-surface policy (§2); the lenient` |
|       - | 1881 | `		 * contexts (isset/empty/??/unset) truncate quietly as php's value does. */` |
|  171570 | 1882 | `		if( (iP2 == 0 \|\| iP2 == 1)` |
|  101827 | 1883 | `		 && (pIdx->iFlags & MEMOBJ_REAL)` |
|   85795 | 1884 | `		 && pIdx->rVal != (ph7_real)(sxi64)pIdx->rVal ){` |
|       - | 1885 | `			SyBlob sErrMsg;` |
|       5 | 1886 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       5 | 1887 | `			SyBlobAppend(&sErrMsg,"Cannot access offset of type float on array",` |
|       - | 1888 | `				sizeof("Cannot access offset of type float on array")-1);` |
|       5 | 1889 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|       5 | 1890 | `			PH7_MemObjRelease(pIdx);` |
|       5 | 1891 | `			PH7_MemObjRelease(pTos);` |
|       5 | 1892 | `			pTos->nIdx = SXU32_HIGH;` |
|       5 | 1893 | `			VM_EXIT_BREAK;` |
|       - | 1894 | `		}` |
|   85783 | 1895 | `	}` |
|  171663 | 1896 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|  171593 | 1897 | `		if( iP2 == 1 \|\| VM_IDX_IS_UNSET(iP2) ){` |
|       - | 1898 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|       - | 1899 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|       - | 1900 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|       - | 1901 | `			 * NOT separate — that would defeat COW on every element read.` |
|       - | 1902 | `			 * iP2=5 is unset-context, treated like iP2=1 for arrays so the` |
|       - | 1903 | `			 * trailing unset() builtin can drop the slot via pTos->nIdx. */` |
|    1609 | 1904 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|     802 | 1905 | `		}` |
|       - | 1906 | `		/* Point to the hashmap */` |
|  171593 | 1907 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|  171593 | 1908 | `		if( pIdx ){` |
|       - | 1909 | `			/* Load the desired entry */` |
|  171571 | 1910 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|   85783 | 1911 | `		}` |
|  171593 | 1912 | `		if( iP2 == 3 ){` |
|       - | 1913 | `			/* Null coalescing assign peek mode: separate only when we will` |
|       - | 1914 | `			 * actually write back. If the looked-up value is non-null, the` |
|       - | 1915 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|       - | 1916 | `			 * the parent can stay shared. If the value is null or the key is` |
|       - | 1917 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|       - | 1918 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|       - | 1919 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|       - | 1920 | `			 * correct for the outermost write. */` |
|      25 | 1921 | `			int needWrite = (rc != SXRET_OK);` |
|      25 | 1922 | `			if( !needWrite && pNode ){` |
|      13 | 1923 | `				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|      13 | 1924 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|       7 | 1925 | `					needWrite = 1;` |
|       3 | 1926 | `				}` |
|       6 | 1927 | `			}` |
|      25 | 1928 | `			if( needWrite ){` |
|      19 | 1929 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      19 | 1930 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|       - | 1931 | `					/* The map was actually copied — re-lookup so pNode points` |
|       - | 1932 | `					 * into the new map's storage. */` |
|       7 | 1933 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       7 | 1934 | `					if( pIdx ){` |
|       7 | 1935 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|       3 | 1936 | `					}` |
|       3 | 1937 | `				}` |
|       9 | 1938 | `			}` |
|      12 | 1939 | `		}` |
|       - | 1940 | `		/* iP2 == 5 (unset) is deliberately NOT here: php's unset() never creates` |
|       - | 1941 | `		 * the key it is about to remove, so a MISS must stay a miss. The` |
|       - | 1942 | `		 * insert-then-unset round trip was invisible on the LAST step but left` |
|       - | 1943 | ``		 * every INTERMEDIATE behind -- `unset($a["y"]["z"])` grew an empty`` |
|       - | 1944 | `		 * $a["y"]. The COW separation the unset context needs happened above and` |
|       - | 1945 | `		 * does not depend on this insert. */` |
|  171593 | 1946 | `		if( rc != SXRET_OK && (iP2 == 1 \|\| iP2 == 3) ){` |
|       - | 1947 | `			/* Create a new empty entry */` |
|     134 | 1948 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|     134 | 1949 | `			if( rc == SXRET_OK ){` |
|       - | 1950 | `				/* Point to the last inserted entry */` |
|     131 | 1951 | `				pNode = pMap->pLast;` |
|      67 | 1952 | `			}else{` |
|       - | 1953 | ``				/* An append lvalue (`$a[][...] = v`) whose saturated auto-index`` |
|       - | 1954 | `				 * is occupied threw php's catchable Error. Dispatch it here —` |
|       - | 1955 | `				 * falling through with a stale pMap->pLast is what silently` |
|       - | 1956 | `				 * overwrote $a[PHP_INT_MAX]. */` |
|       7 | 1957 | `				PH7_DISPATCH_ENFORCE_RC(rc)` |
|       - | 1958 | `			}` |
|      64 | 1959 | `		}` |
|   85793 | 1960 | `	}` |
|  171656 | 1961 | `	if( rc != SXRET_OK && pIdx && (iP2 == 2 \|\| iP2 == 0)` |
|   52741 | 1962 | `	 && (pTos->iFlags & MEMOBJ_HASHMAP)` |
|      40 | 1963 | `	 && !VmIdxFeedsCoalesce(pInstr) ){` |
|       - | 1964 | ``		/* `$a['k'] ?? $d` compiles its LHS as a plain read (iP2 == 0) followed`` |
|       - | 1965 | `		 * by NULLC/NULLC_JMP — php does NOT warn there, so peek ahead and stay` |
|       - | 1966 | `		 * silent (same guard the magic-accessor read path uses). */` |
|       - | 1967 | `		/* php warns when a missing key is READ (iP2 == 0) or destructured` |
|       - | 1968 | `		 * (iP2 == 2). isset/empty/??/unset (iP2 3-6) and write-context` |
|       - | 1969 | `		 * vivification (iP2 == 1) stay silent, as does a read on a non-array` |
|       - | 1970 | `		 * base (already diagnosed above). php prints an INT key bare and a` |
|       - | 1971 | `		 * STRING key quoted, and it decides which one the key IS by the same fold` |
|       - | 1972 | ``		 * the lookup just used: `$a["10"]` looked for the INTEGER key 10, so php`` |
|       - | 1973 | ``		 * says `Undefined array key 10`. PHL rendered the operand as WRITTEN --`` |
|       - | 1974 | ``		 * quoting every string and, worse, printing `$a[false]` as the "" key it`` |
|       - | 1975 | `		 * never looked in (false is the integer key 0). The canonical-numeric rule` |
|       - | 1976 | `		 * is the hashmap's own, so ask it rather than re-derive it. */` |
|       - | 1977 | `		SyBlob sMsg;` |
|      75 | 1978 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      75 | 1979 | `		if( PH7_HashmapKeyIsInt(pIdx) ){` |
|      24 | 1980 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      15 | 1981 | `				PH7_MemObjToInteger(pIdx);` |
|       7 | 1982 | `			}` |
|      24 | 1983 | `			SyBlobFormat(&sMsg,"Undefined array key %qd",pIdx->x.iVal);` |
|      13 | 1984 | `		}else{` |
|       - | 1985 | `			/* Not an integer key: PH7_HashmapKeyIsInt left it a printable string. */` |
|       - | 1986 | `			SyString sKey;` |
|      53 | 1987 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob));` |
|      53 | 1988 | `			SyBlobFormat(&sMsg,"Undefined array key \"%z\"",&sKey);` |
|       - | 1989 | `		}` |
|      75 | 1990 | `		SyBlobNullAppend(&sMsg);` |
|      75 | 1991 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|      75 | 1992 | `		SyBlobRelease(&sMsg);` |
|      35 | 1993 | `	}` |
|  171610 | 1994 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0` |
|   85817 | 1995 | `	 && (iP2 == 0 \|\| iP2 == 2)` |
|      21 | 1996 | `	 && !VmIdxFeedsCoalesce(pInstr) ){` |
|       - | 1997 | `		/* Subscripting a scalar base is a WARNING in php ("Trying to access array offset` |
|       - | 1998 | `		 * on int") that yields NULL. PH7 yielded NULL in silence. */` |
|      14 | 1999 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Trying to access array offset on %s",` |
|       4 | 2000 | `			VmArithValueName(pTos));` |
|       4 | 2001 | `	}` |
|  171610 | 2002 | `	if( iP2 == VM_IDX_CTX_UNSET && rc == SXRET_OK && pNode != 0` |
|     919 | 2003 | `	 && (pTos->iFlags & MEMOBJ_HASHMAP) ){` |
|       - | 2004 | ``		/* php's `unset($a[k])` removes the ELEMENT. PH7 left the element's value slot`` |
|       - | 2005 | `		 * on the stack and let the trailing unset() builtin drop it — but dropping a` |
|       - | 2006 | `` 		 * SLOT unlinks everything that holds it, so `$r = &$a['k']; unset($a['k']);` `` |
|       - | 2007 | ``		 * destroyed $r too (`Undefined variable $r`) where php leaves it reading the`` |
|       - | 2008 | `		 * value it still refers to. Unlink the node itself, which releases the value` |
|       - | 2009 | `		 * only when this element was its last holder, and leave the builtin nothing. */` |
|     919 | 2010 | `		ph7_hashmap *pTarget = (ph7_hashmap *)pTos->x.pOther;` |
|     919 | 2011 | `		int bDone = 0;` |
|     919 | 2012 | `		if( pTarget == pVm->pGlobal && pIdx ){` |
|       - | 2013 | ``			/* `$GLOBALS['x']` IS the global $x, so this is an unset of the NAME: it has`` |
|       - | 2014 | `			 * to drop the symbol-table entry as well as this node, and it must not` |
|       - | 2015 | `			 * destroy the value another holder still refers to — exactly what` |
|       - | 2016 | ``			 * VmUnsetVarByName does for `unset($x)`. A superglobal has no entry in the`` |
|       - | 2017 | `			 * global frame and falls through to the plain node unlink below. */` |
|     159 | 2018 | `			VmFrame *pGlobalFrame = pVm->pFrame;` |
|       - | 2019 | `			SyHashEntry *pNameEntry;` |
|     159 | 2020 | `			while( pGlobalFrame->pParent ){` |
|     ! 0 | 2021 | `				pGlobalFrame = pGlobalFrame->pParent;` |
|     ! 0 | 2022 | `			}` |
|     159 | 2023 | `			if( (pIdx->iFlags & MEMOBJ_STRING) == 0 ){` |
|       3 | 2024 | `				PH7_MemObjToString(pIdx);` |
|       1 | 2025 | `			}` |
|     238 | 2026 | `			pNameEntry = SyHashGet(&pGlobalFrame->hVar,` |
|     158 | 2027 | `				(const void *)SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob));` |
|     159 | 2028 | `			if( pNameEntry ){` |
|     238 | 2029 | `				sxi32 rcUnset = VmUnsetVarByNameEx(&(*pVm),pGlobalFrame,` |
|     158 | 2030 | `					(const char *)SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob),FALSE);` |
|     159 | 2031 | `				bDone = 1;` |
|     159 | 2032 | `				if( rcUnset == PH7_ABORT ){` |
|     ! 0 | 2033 | `					PH7_MemObjRelease(pIdx);` |
|     ! 0 | 2034 | `					VM_EXIT_ABORT;` |
|       - | 2035 | `				}` |
|      79 | 2036 | `			}` |
|      79 | 2037 | `		}` |
|     919 | 2038 | `		if( !bDone ){` |
|     761 | 2039 | `			PH7_HashmapUnlinkNode(pNode,TRUE);` |
|     378 | 2040 | `		}` |
|     919 | 2041 | `		if( pIdx ){` |
|     919 | 2042 | `			PH7_MemObjRelease(pIdx);` |
|     457 | 2043 | `		}` |
|     919 | 2044 | `		PH7_MemObjRelease(pTos);` |
|     919 | 2045 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|     919 | 2046 | `		pTos->nIdx = SXU32_HIGH;` |
|     919 | 2047 | `		VM_EXIT_BREAK;` |
|       - | 2048 | `	}` |
|  170701 | 2049 | `	if( pIdx ){` |
|  170681 | 2050 | `		PH7_MemObjRelease(pIdx);` |
|   85338 | 2051 | `	}` |
|  170701 | 2052 | `	if( rc == SXRET_OK ){` |
|       - | 2053 | `		/* Load entry contents */` |
|   65297 | 2054 | `		if( pMap->iRef < 2 ){` |
|       - | 2055 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|       - | 2056 | `			 * of the entry value,rather than pointing to it.` |
|       - | 2057 | `			 */` |
|     387 | 2058 | `			pTos->nIdx = SXU32_HIGH;` |
|     387 | 2059 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|     196 | 2060 | `		}else{` |
|   64915 | 2061 | `			pTos->nIdx = pNode->nValIdx;` |
|   64915 | 2062 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|   64915 | 2063 | `			PH7_HashmapUnref(pMap);` |
|       - | 2064 | `		}` |
|   32651 | 2065 | `	}else{` |
|       - | 2066 | `		/* No such entry,load NULL */` |
|  105409 | 2067 | `		PH7_MemObjRelease(pTos);` |
|  105409 | 2068 | `		pTos->nIdx = SXU32_HIGH;` |
|       - | 2069 | `	}` |
|  170701 | 2070 | `	VM_EXIT_BREAK;` |
|     ! 0 | 2071 | `	VM_EXIT_BREAK;` |
|  536330 | 2072 | `}` |
|       - | 2073 |  |
|       - | 2074 | `/*` |
|       - | 2075 | ` * OP_LOAD_MAP: body moved verbatim from the OP_LOAD_MAP arm of` |
|       - | 2076 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 2077 | ` */` |
|   99289 | 2078 | `PH7_PRIVATE VmOpRc VmExecOpLoadMap(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 2079 | `{` |
|   99294 | 2080 | `	ph7_value *pTos = pState->pTos;` |
|   99294 | 2081 | `	ph7_value *pStack = pState->pStack;` |
|   99294 | 2082 | `	VmInstr *aInstr = pState->aInstr;` |
|   99294 | 2083 | `	sxi32 pc = pState->pc;` |
|       - | 2084 | `	sxi32 rc;` |
|   49644 | 2085 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - | 2086 | `	ph7_hashmap *pMap;` |
|       - | 2087 | `	/* Allocate a new hashmap instance */` |
|   99294 | 2088 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|   99294 | 2089 | `	if( pMap == 0 ){` |
|     ! 0 | 2090 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|     ! 0 | 2091 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|     ! 0 | 2092 | `		VM_EXIT_ABORT;` |
|       - | 2093 | `	}` |
|   99294 | 2094 | `	if( pInstr->iP1 > 0 ){` |
|   19341 | 2095 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|   19341 | 2096 | `		sxi32 rcSpread = SXRET_OK;` |
|       - | 2097 | `		/* Perform the insertion */` |
|   64695 | 2098 | `		while( pEntry < pTos ){` |
|   45377 | 2099 | `			if( pEntry[1].iFlags & MEMOBJ_AUX_SPREAD ){` |
|       - | 2100 | `				/* Array unpacking: '...$expr'. Merge entries with PHP 8.1` |
|       - | 2101 | `				 * semantics — string keys preserved (later wins), int keys` |
|       - | 2102 | `				 * renumbered. Same routine that backs array_merge. */` |
|     688 | 2103 | `				if( pEntry[1].iFlags & MEMOBJ_HASHMAP ){` |
|     664 | 2104 | `					sxi32 rcMerge = PH7_HashmapMerge((ph7_hashmap *)pEntry[1].x.pOther,pMap);` |
|     664 | 2105 | `					if( rcMerge != SXRET_OK ){` |
|       - | 2106 | `						/* Merge failure (OOM): match the PH7_NewHashmap OOM` |
|       - | 2107 | `						 * path — emit fatal and abort, leaving no partial` |
|       - | 2108 | `						 * map dangling. */` |
|     ! 0 | 2109 | `						VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|     ! 0 | 2110 | `							"Fatal, PH7 engine is running out of memory while spreading array at instruction #:%d",pc);` |
|     ! 0 | 2111 | `						rcSpread = PH7_ABORT;` |
|     ! 0 | 2112 | `						break;` |
|       2 | 2113 | `					}` |
|     356 | 2114 | `				}else if( VmValueIsTraversable(pVm,&pEntry[1]) ){` |
|       - | 2115 | `					/* Traversable unpacking (PHP 8.1): walk it into the map using the` |
|       - | 2116 | `					 * same key rules as array spread (string keys kept, int renumbered). */` |
|       5 | 2117 | `					sxi32 rcW = PH7_VmIteratorWalk(&(*pVm),&pEntry[1],VmSpreadMergeStep,pMap);` |
|       5 | 2118 | `					if( rcW == PH7_EXCEPTION \|\| rcW == PH7_ABORT ){` |
|     ! 0 | 2119 | `						rcSpread = rcW;` |
|     ! 0 | 2120 | `						break;` |
|       - | 2121 | `					}` |
|       3 | 2122 | `				}else{` |
|       - | 2123 | `					/* Throw a catchable Error matching PHP semantics. */` |
|      21 | 2124 | `					rcSpread = VmThrowSpreadError(&(*pVm),&pEntry[1]);` |
|      21 | 2125 | `					break;` |
|       2 | 2126 | `				}` |
|   45026 | 2127 | `			}else if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|       - | 2128 | `				/* Insertion by reference */` |
|     244 | 2129 | `				PH7_HashmapInsertByRef(pMap,` |
|     162 | 2130 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     162 | 2131 | `					(sxu32)pEntry[1].x.iVal` |
|       - | 2132 | `					);` |
|      82 | 2133 | `			}else{` |
|       - | 2134 | `				/* An explicit key in an array LITERAL gets the same php diagnostics a` |
|       - | 2135 | `				 * subscript does — a float key that truncates deprecates, and so does an` |
|       - | 2136 | `				 * explicit null key. Only the subscript sites (LOAD_IDX/STORE_IDX) used to` |
|       - | 2137 | ``				 * emit these, so `[1.5 => "v"]` and `[null => "v"]` were silent. A key that`` |
|       - | 2138 | `				 * is ABSENT (auto-index) is a NULL slot here, not a null key, so the` |
|       - | 2139 | `				 * MEMOBJ_NULL check below is what tells the two apart. */` |
|   44531 | 2140 | `					if( (pEntry->iFlags & MEMOBJ_AUX_NOKEY) == 0 ){` |
|       - | 2141 | `						/* An object/array literal key is php's TypeError, a resource one` |
|       - | 2142 | `						 * warns and becomes its id — same rules as a subscript. */` |
|       - | 2143 | `						SyBlob sTypeMsg;` |
|   12587 | 2144 | `						if( VmOffsetTypeRejected(&(*pVm),pEntry,0,&sTypeMsg) ){` |
|       3 | 2145 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg));` |
|       2 | 2146 | `						}else{` |
|   12585 | 2147 | `							VmOffsetResourceWarn(&(*pVm),pEntry);` |
|       - | 2148 | `						}` |
|       - | 2149 | `						/* php DEPRECATES a null literal key (then normalizes to "") and` |
|       - | 2150 | `						 * rejects nothing there; PHL matches that (deprecate + fall` |
|       - | 2151 | `						 * through, PH7_HashmapInsert casts NULL->""). A lossy-FLOAT` |
|       - | 2152 | `						 * literal key still rejects with a TypeError — the recorded` |
|       - | 2153 | `						 * non-deprecated-surface policy, same as the subscript site. */` |
|   12587 | 2154 | `						int bNull = (pEntry->iFlags & MEMOBJ_NULL) != 0;` |
|   18879 | 2155 | `						int bLossyFloat = (pEntry->iFlags & MEMOBJ_REAL) != 0` |
|   12582 | 2156 | `							&& pEntry->rVal != (ph7_real)(sxi64)pEntry->rVal;` |
|   12587 | 2157 | `						if( bNull ){` |
|       3 | 2158 | `							VmNullOffsetDeprecate(&(*pVm),pEntry);` |
|   12586 | 2159 | `						}else if( bLossyFloat ){` |
|       3 | 2160 | `							const char *zErr = "Cannot access offset of type float on array";` |
|       - | 2161 | `							SyBlob sErrMsg;` |
|       3 | 2162 | `							SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       3 | 2163 | `							SyBlobAppend(&sErrMsg,zErr,(sxu32)SyStrlen(zErr));` |
|       3 | 2164 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|       1 | 2165 | `						}` |
|    6291 | 2166 | `					}` |
|       - | 2167 | `				/* Standard insertion */` |
|   66794 | 2168 | `				PH7_HashmapInsert(pMap,` |
|   44526 | 2169 | `					(pEntry->iFlags & MEMOBJ_AUX_NOKEY) ? 0 /* Automatic index assign */ : pEntry,` |
|   22263 | 2170 | `					&pEntry[1]` |
|       - | 2171 | `				);` |
|       - | 2172 | `			}` |
|       - | 2173 | `			/* Next pair on the stack */` |
|   45359 | 2174 | `			pEntry += 2;` |
|       5 | 2175 | `		}` |
|       - | 2176 | `		/* Pop P1 elements */` |
|   19341 | 2177 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|   19341 | 2178 | `		if( rcSpread != SXRET_OK ){` |
|       - | 2179 | `			/* Discard the partially-built map and propagate the exception. */` |
|      21 | 2180 | `			PH7_HashmapRelease(pMap,TRUE);` |
|      21 | 2181 | `			if( rcSpread == PH7_ABORT ){` |
|     ! 0 | 2182 | `				VM_EXIT_ABORT;` |
|       - | 2183 | `			}` |
|       - | 2184 | `			{` |
|       - | 2185 | `				sxi32 iRp;` |
|      21 | 2186 | `				if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|       6 | 2187 | `					pc = iRp;` |
|       6 | 2188 | `					VM_EXIT_BREAK;` |
|       - | 2189 | `				}` |
|       - | 2190 | `			}` |
|      15 | 2191 | `			VM_EXIT_EXCEPTION;` |
|       - | 2192 | `		}` |
|    9659 | 2193 | `	}` |
|       - | 2194 | `	/* Push the hashmap */` |
|   99276 | 2195 | `	pTos++;` |
|   99276 | 2196 | `	pTos->nIdx = SXU32_HIGH;` |
|   99276 | 2197 | `	pTos->x.pOther = pMap;` |
|   99276 | 2198 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|   99276 | 2199 | `	VM_EXIT_BREAK;` |
|     ! 0 | 2200 | `	VM_EXIT_BREAK;` |
|   49649 | 2201 | `}` |
|       - | 2202 |  |
|       - | 2203 | `/*` |
|       - | 2204 | ` * OP_LOAD_LIST: body moved verbatim from the OP_LOAD_LIST arm of` |
|       - | 2205 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 2206 | ` */` |
|    1052 | 2207 | `PH7_PRIVATE VmOpRc VmExecOpLoadList(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 2208 | `{` |
|    1057 | 2209 | `	ph7_value *pTos = pState->pTos;` |
|    1057 | 2210 | `	ph7_value *pStack = pState->pStack;` |
|    1057 | 2211 | `	VmInstr *aInstr = pState->aInstr;` |
|    1057 | 2212 | `	sxi32 pc = pState->pc;` |
|       - | 2213 | `	sxi32 rc;` |
|     526 | 2214 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - | 2215 | `	ph7_value *pEntry;` |
|    1057 | 2216 | `	sxi32 rcEnforce = SXRET_OK;` |
|    1057 | 2217 | `	if( pInstr->iP1 <= 0 ){` |
|       - | 2218 | `		/* Empty list,break immediately */` |
|     ! 0 | 2219 | `		VM_EXIT_BREAK;` |
|       - | 2220 | `	}` |
|    1057 | 2221 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|       - | 2222 | `#ifdef UNTRUST` |
|       - | 2223 | `	if( &pEntry[-1] < pStack ){` |
|       - | 2224 | `		VM_EXIT_ABORT;` |
|       - | 2225 | `	}` |
|       - | 2226 | `#endif` |
|    1057 | 2227 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|    1041 | 2228 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|       - | 2229 | `		ph7_hashmap_node *pNode;` |
|       - | 2230 | `		ph7_value sKey,*pObj;` |
|       - | 2231 | `		/* Start Copying */` |
|    1041 | 2232 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|    3105 | 2233 | `		while( pEntry <= pTos ){` |
|    2085 | 2234 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|    2055 | 2235 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|    2055 | 2236 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|    3955 | 2237 | `					int bTyped = SyHashTotalEntry(&pVm->hTypedSlot) > 0` |
|    2050 | 2238 | `						&& SyHashGet(&pVm->hTypedSlot,(const void *)&pEntry->nIdx,sizeof(sxu32)) != 0;` |
|    2055 | 2239 | `					if( rc != SXRET_OK ){` |
|       - | 2240 | `						/* Undefined array key */` |
|       - | 2241 | `						char zMsg[128];` |
|       5 | 2242 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|       5 | 2243 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|       2 | 2244 | `					}` |
|    2055 | 2245 | `					if( !bTyped ){` |
|    2025 | 2246 | `						if( rc == SXRET_OK ){` |
|       - | 2247 | `							/* Store node value */` |
|    2025 | 2248 | `							PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|    1015 | 2249 | `						}else{` |
|     ! 0 | 2250 | `							PH7_MemObjRelease(pObj);` |
|       - | 2251 | `						}` |
|    1015 | 2252 | `					}else{` |
|       - | 2253 | ``						/* Typed/readonly property target (`[$o->p] = [...]`): a`` |
|       - | 2254 | `						 * direct slot write would bypass the typed-slot table, so` |
|       - | 2255 | `						 * enforce on a temp first — a TypeError leaves the property` |
|       - | 2256 | `						 * untouched, and a missing key assigns null, which a` |
|       - | 2257 | `						 * non-nullable type rejects exactly like php (warning, then` |
|       - | 2258 | `						 * "Cannot assign null to property ... of type ..."). */` |
|       - | 2259 | `						ph7_value sVal;` |
|      31 | 2260 | `						PH7_MemObjInit(&(*pVm),&sVal);` |
|      31 | 2261 | `						if( rc == SXRET_OK ){` |
|      27 | 2262 | `							PH7_HashmapExtractNodeValue(pNode,&sVal,TRUE);` |
|      13 | 2263 | `						}` |
|      31 | 2264 | `						rcEnforce = VmEnforcePropertyTypeOnStore(&(*pVm),pEntry->nIdx,&sVal,0);` |
|      31 | 2265 | `						if( rcEnforce != SXRET_OK ){` |
|       - | 2266 | `							/* Thrown: stop assigning (php aborts the list at the` |
|       - | 2267 | `							 * first failing element), settle the stack, route. */` |
|      17 | 2268 | `							PH7_MemObjRelease(&sVal);` |
|      17 | 2269 | `							break;` |
|       - | 2270 | `						}` |
|      15 | 2271 | `						PH7_MemObjStore(&sVal,pObj);` |
|      15 | 2272 | `						PH7_MemObjRelease(&sVal);` |
|       - | 2273 | `					}` |
|    1017 | 2274 | `				}` |
|    1017 | 2275 | `			}` |
|    2069 | 2276 | `			sKey.x.iVal++; /* Next numeric index */` |
|    2069 | 2277 | `			pEntry++;` |
|       5 | 2278 | `		}` |
|     523 | 2279 | `	}else{` |
|       - | 2280 | `		/* Source is not an array: php warns first (silencing ONLY null — a bool` |
|       - | 2281 | `		 * source warns too, php 8), then assigns null to every target. A typed` |
|       - | 2282 | `		 * property target receives that null THROUGH enforcement, so a` |
|       - | 2283 | `		 * non-nullable type throws "Cannot assign null to property ..." exactly` |
|       - | 2284 | `		 * like php instead of silently nulling the slot. PHL DIVERGENCE: bool` |
|       - | 2285 | `		 * FALSE stays silent (php warns) — it is the end-of-array sentinel of` |
|       - | 2286 | `` 		 * the documented each() extension's `while (list(..) = each($a))` `` |
|       - | 2287 | `		 * idiom, which would otherwise warn on every normal loop exit. */` |
|       - | 2288 | `		ph7_value *pObj;` |
|      30 | 2289 | `		int bFalseSrc = (pTos[-pInstr->iP1].iFlags & MEMOBJ_BOOL) != 0` |
|      16 | 2290 | `			&& pTos[-pInstr->iP1].x.iVal == 0;` |
|      19 | 2291 | `		if( (pTos[-pInstr->iP1].iFlags & MEMOBJ_NULL) == 0 && !bFalseSrc ){` |
|      12 | 2292 | `			VmWarnCannotUseAsArray(&(*pVm),pTos[-pInstr->iP1].iFlags);` |
|       5 | 2293 | `		}` |
|      33 | 2294 | `		while( pEntry <= pTos ){` |
|      23 | 2295 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|      23 | 2296 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|      42 | 2297 | `					int bTyped = SyHashTotalEntry(&pVm->hTypedSlot) > 0` |
|      20 | 2298 | `						&& SyHashGet(&pVm->hTypedSlot,(const void *)&pEntry->nIdx,sizeof(sxu32)) != 0;` |
|      23 | 2299 | `					if( !bTyped ){` |
|      15 | 2300 | `						PH7_MemObjRelease(pObj);` |
|       9 | 2301 | `					}else{` |
|       - | 2302 | `						ph7_value sVal;` |
|       9 | 2303 | `						PH7_MemObjInit(&(*pVm),&sVal);` |
|       9 | 2304 | `						rcEnforce = VmEnforcePropertyTypeOnStore(&(*pVm),pEntry->nIdx,&sVal,0);` |
|       9 | 2305 | `						if( rcEnforce != SXRET_OK ){` |
|       7 | 2306 | `							PH7_MemObjRelease(&sVal);` |
|       7 | 2307 | `							break;` |
|       - | 2308 | `						}` |
|       3 | 2309 | `						PH7_MemObjStore(&sVal,pObj);` |
|       3 | 2310 | `						PH7_MemObjRelease(&sVal);` |
|       - | 2311 | `					}` |
|       7 | 2312 | `				}` |
|       7 | 2313 | `			}` |
|      17 | 2314 | `			pEntry++;` |
|       3 | 2315 | `		}` |
|       - | 2316 | `	}` |
|    1057 | 2317 | `	if( rcEnforce != SXRET_OK ){` |
|       - | 2318 | `		/* Settle this op's own operands: the P1 entries AND the source value —` |
|       - | 2319 | `		 * its statement-level OP_POP is skipped when a catch resumes at the` |
|       - | 2320 | `		 * landing pad, so leaving it would leak one operand slot per caught` |
|       - | 2321 | `		 * throw. A NESTED destructure can still have the outer list's operands` |
|       - | 2322 | `		 * abandoned above the try's base, so on an in-place catch drain to the` |
|       - | 2323 | `		 * catching try's recorded depth (like the fetch-point router and the` |
|       - | 2324 | `		 * generator inject path), not just our own pops. */` |
|      23 | 2325 | `		VmPopOperand(&pTos,pInstr->iP1 + 1);` |
|      23 | 2326 | `		if( rcEnforce == PH7_ABORT ){` |
|     ! 0 | 2327 | `			VM_EXIT_ABORT;` |
|       - | 2328 | `		}` |
|       - | 2329 | `		{` |
|       - | 2330 | `			sxi32 _iRpL;` |
|      34 | 2331 | `			PH7_INLINE_RESUME_BREAK()` |
|      23 | 2332 | `			if( VmRecordedResume(pVm,&_iRpL,pState->pEntryFrame,aInstr) ){` |
|      25 | 2333 | `				PH7_RESUME_DRAIN()` |
|      23 | 2334 | `				pc = _iRpL;` |
|      23 | 2335 | `				VM_EXIT_BREAK;` |
|       - | 2336 | `			}` |
|       - | 2337 | `		}` |
|     ! 0 | 2338 | `		VM_EXIT_EXCEPTION;` |
|       - | 2339 | `	}` |
|    1035 | 2340 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|    1035 | 2341 | `	VM_EXIT_BREAK;` |
|     ! 0 | 2342 | `	VM_EXIT_BREAK;` |
|     531 | 2343 | `}` |
|       - | 2344 |  |
|       - | 2345 | `/*` |
|       - | 2346 | ` * OP_UNSET_VAR: body moved verbatim from the OP_UNSET_VAR arm of` |
|       - | 2347 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 2348 | ` */` |
|    7716 | 2349 | `PH7_PRIVATE VmOpRc VmExecOpUnsetVar(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 2350 | `{` |
|    7721 | 2351 | `	ph7_value *pTos = pState->pTos;` |
|    7721 | 2352 | `	ph7_value *pStack = pState->pStack;` |
|    7721 | 2353 | `	VmInstr *aInstr = pState->aInstr;` |
|    7721 | 2354 | `	sxi32 pc = pState->pc;` |
|       - | 2355 | `	sxi32 rc;` |
|    3858 | 2356 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - | 2357 | `	/* unset($name): p3 is the variable name. Drops the NAME only — see VmUnsetVarByName */` |
|    7721 | 2358 | `	SyString *pName = (SyString *)pInstr->p3;` |
|    7721 | 2359 | `	if( pName && pVm->pFrame ){` |
|       - | 2360 | `		/* Inside a try{} the VM pushes an EXCEPTION frame; variables live in the body` |
|       - | 2361 | `		 * frame below it, so skip past it exactly as every other variable path does.` |
|       - | 2362 | `		 * Without this, unset($x) inside a try silently found nothing and did nothing. */` |
|    7721 | 2363 | `		VmFrame *pVarFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|    7721 | 2364 | `		sxi32 rcU = VmUnsetVarByName(&(*pVm),pVarFrame,pName->zString,pName->nByte);` |
|    7721 | 2365 | `		if( rcU == PH7_ABORT ){` |
|       3 | 2366 | `			VM_EXIT_ABORT;` |
|       - | 2367 | `		}` |
|       - | 2368 | `		/* Releasing the last holder can run a __destruct(), and that destructor may` |
|       - | 2369 | `		 * throw. Such a throw is PARKED in nBoundaryRc by the boundary rail; consume it` |
|       - | 2370 | `		 * here and route it, or the catch runs and execution resumes inside the try` |
|       - | 2371 | `		 * ("resumed-dtor" instead of php's "caught-dtor"). */` |
|    7719 | 2372 | `		if( pVm->nBoundaryRc != 0 ){` |
|       3 | 2373 | `			rc = pVm->nBoundaryRc;` |
|       3 | 2374 | `			pVm->nBoundaryRc = 0;` |
|       3 | 2375 | `			if( rc == PH7_ABORT ){` |
|     ! 0 | 2376 | `				VM_EXIT_ABORT;` |
|       - | 2377 | `			}` |
|       3 | 2378 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2379 | `		}` |
|    3856 | 2380 | `	}` |
|    7717 | 2381 | `	VM_EXIT_BREAK;` |
|     ! 0 | 2382 | `	VM_EXIT_BREAK;` |
|    3863 | 2383 | `}` |
|       - | 2384 |  |
