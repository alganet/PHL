# src/ph7/vm_ops_load.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1509/1651 lines (91.40%)

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
|    3426 |   28 | `PH7_PRIVATE VmOpRc VmExecOpStoreRef(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |   29 | `{` |
|    3431 |   30 | `	ph7_value *pTos = pState->pTos;` |
|    3431 |   31 | `	ph7_value *pStack = pState->pStack;` |
|    3431 |   32 | `	VmInstr *aInstr = pState->aInstr;` |
|    3431 |   33 | `	sxi32 pc = pState->pc;` |
|       - |   34 | `	sxi32 rc;` |
|    1713 |   35 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    3431 |   36 | `	 SyString sName = { 0 , 0 };` |
|       - |   37 | `	 VmFrame *pFrameLocal;` |
|       - |   38 | `	SyHashEntry *pEntry;` |
|       - |   39 | `	sxu32 nIdx;` |
|       - |   40 | `#ifdef UNTRUST` |
|       - |   41 | `	if( pTos < pStack ){` |
|       - |   42 | `		VM_EXIT_ABORT;` |
|       - |   43 | `	}` |
|       - |   44 | `#endif` |
|    3431 |   45 | `	if( pInstr->iP2 == 1 ){` |
|       - |   46 | ``		/* Member reference target: `$o->p =& $x` / `self::$s =& $x`. The`` |
|       - |   47 | `		 * preceding OP_MEMBER (PH7_MEMBER_REF_TARGET) resolved the property slot` |
|       - |   48 | `		 * and stashed it. Stack: [ ... , source, member-result(top) ]. Rebind the` |
|       - |   49 | `		 * property's nIdx to alias the source variable's slot and pin that slot` |
|       - |   50 | `		 * past its owning frame (like a use(&$x) capture) so neither frame` |
|       - |   51 | `		 * teardown nor a later unset recycles it while the property aliases it. */` |
|      70 |   52 | `		ph7_value *pSrc = &pTos[-1];` |
|      70 |   53 | `		sxu32 nSrcIdx = pSrc->nIdx;` |
|      70 |   54 | `		VmClassAttr *pVmAttr = pVm->pRefTargetAttr;` |
|      70 |   55 | `		ph7_class_attr *pStAttr = pVm->pRefTargetStaticAttr;` |
|      70 |   56 | `		if( pSrc->iFlags & MEMOBJ_AUX_STROFFSET ){` |
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
|      65 |   72 | `		if( nSrcIdx == SXU32_HIGH ){` |
|       - |   73 | ``			/* No slot behind the SOURCE (`$o->p =& f()`, `C::$s =& f()`). php makes a`` |
|       - |   74 | `			 * fresh reference holding the value and binds the property to THAT; PHL` |
|       - |   75 | ``			 * refused the bind with `Reference operator require a variable not a`` |
|       - |   76 | ``			 * constant as it's right operand` and left the property untouched. A`` |
|       - |   77 | `			 * source WRITTEN as a call takes php's notice with it. */` |
|       - |   78 | `			ph7_value *pFresh;` |
|       5 |   79 | `			if( pVmAttr \|\| pStAttr ){` |
|       - |   80 | `				/* …only when there IS a property to bind it to: with no resolved` |
|       - |   81 | `				 * target the bind is a no-op and the slot would never be pinned. */` |
|       5 |   82 | `				if( pInstr->iP1 & PH7_STOREREF_CALLSRC ){` |
|       5 |   83 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|       - |   84 | `						"Only variables should be assigned by reference");` |
|       2 |   85 | `				}` |
|       5 |   86 | `				pFresh = PH7_ReserveMemObj(&(*pVm));` |
|       5 |   87 | `				if( pFresh ){` |
|       5 |   88 | `					nSrcIdx = pFresh->nIdx;` |
|       5 |   89 | `					PH7_MemObjStore(pSrc,pFresh);` |
|       5 |   90 | `					pSrc->nIdx = nSrcIdx;` |
|       2 |   91 | `				}` |
|       2 |   92 | `			}` |
|       2 |   93 | `		}` |
|      65 |   94 | `		if( nSrcIdx == SXU32_HIGH ){` |
|       - |   95 | `			/* Reservation failed: nothing to bind to. */` |
|      65 |   96 | `		}else if( pVmAttr ){` |
|      53 |   97 | `			sxu32 nOldIdx = pVmAttr->nIdx;` |
|      53 |   98 | `			if( nOldIdx != nSrcIdx ){` |
|      53 |   99 | `				if( pVmAttr->iState & (VM_CLASS_ATTR_REFBOUND\|VM_CLASS_ATTR_REFSRCPIN) ){` |
|       - |  100 | `					/* Already at one end of a reference: give that slot its pin back,` |
|       - |  101 | `					 * which releases it when this property was its last holder. A` |
|       - |  102 | ``					 * SOURCE (`$r =& $o->p; $o->p =& $y;`) still owns its declaration,`` |
|       - |  103 | `					 * so its typed-slot enforcement entry goes with the repoint. */` |
|       9 |  104 | `					if( (pVmAttr->iState & VM_CLASS_ATTR_REFBOUND) == 0 ){` |
|       3 |  105 | `						PH7_VmStoreFilterDrop(&(*pVm),pVmAttr->pAttr,nOldIdx);` |
|       1 |  106 | `					}` |
|       9 |  107 | `					VmUnpinMemObjSlot(&(*pVm),nOldIdx);` |
|       5 |  108 | `				}else{` |
|       - |  109 | `					/* Release this property's own (unshared) slot before repointing.` |
|       - |  110 | `					 * A reference-bound property bypasses typed coercion in php, so` |
|       - |  111 | `					 * drop any typed-slot enforcement entry too. */` |
|      45 |  112 | `					PH7_VmStoreFilterDrop(&(*pVm),pVmAttr->pAttr,nOldIdx);` |
|      45 |  113 | `					PH7_VmUnsetMemObj(&(*pVm),nOldIdx,TRUE);` |
|       - |  114 | `				}` |
|      53 |  115 | `				pVmAttr->nIdx = nSrcIdx;` |
|      53 |  116 | `				pVmAttr->iState \|= VM_CLASS_ATTR_REFBOUND;` |
|      53 |  117 | `				pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_REFSRCPIN);` |
|      53 |  118 | `				VmPinMemObjSlotCounted(&(*pVm),nSrcIdx);` |
|      27 |  119 | `			}` |
|      39 |  120 | `		}else if( pStAttr ){` |
|      13 |  121 | `			sxu32 nOldIdx = pStAttr->nIdx;` |
|      13 |  122 | `			if( nOldIdx != nSrcIdx ){` |
|       - |  123 | `				/* Give the previous target back, exactly as the instance arm above does.` |
|       - |  124 | `				 * A permanent pin was left on every slot the property had ever named, so` |
|       - |  125 | `				 * each of them stayed a REFERENCE for the rest of the script — which an` |
|       - |  126 | ``				 * ordinary array COPY then shared (`$d = $a; $d[0] = 99;` wrote through`` |
|       - |  127 | ``				 * to `$a[0]`, silently), since "is this element a reference" is answered`` |
|       - |  128 | `				 * by who still holds it. */` |
|      13 |  129 | `				if( pStAttr->iFlags & (PH7_CLASS_ATTR_REFBOUND\|PH7_CLASS_ATTR_REFSRCPIN) ){` |
|       5 |  130 | `					if( (pStAttr->iFlags & PH7_CLASS_ATTR_REFBOUND) == 0 ){` |
|     ! 0 |  131 | `						PH7_VmStoreFilterDrop(&(*pVm),pStAttr,nOldIdx);` |
|     ! 0 |  132 | `					}` |
|       5 |  133 | `					VmUnpinMemObjSlot(&(*pVm),nOldIdx);` |
|       3 |  134 | `				}else{` |
|       - |  135 | `					/* The static's own (unshared) slot. A reference-bound property bypasses` |
|       - |  136 | `					 * typed coercion in php, so drop any typed-slot enforcement entry too. */` |
|       9 |  137 | `					PH7_VmStoreFilterDrop(&(*pVm),pStAttr,nOldIdx);` |
|       9 |  138 | `					PH7_VmUnsetMemObj(&(*pVm),nOldIdx,TRUE);` |
|       - |  139 | `				}` |
|      13 |  140 | `				pStAttr->nIdx = nSrcIdx;` |
|      13 |  141 | `				pStAttr->iFlags \|= PH7_CLASS_ATTR_REFBOUND;` |
|      13 |  142 | `				pStAttr->iFlags &= ~PH7_CLASS_ATTR_REFSRCPIN;` |
|      13 |  143 | `				VmPinMemObjSlotCounted(&(*pVm),nSrcIdx);` |
|       6 |  144 | `			}` |
|       6 |  145 | `		}` |
|      65 |  146 | `		if( pVm->pRefTargetThis ){` |
|      53 |  147 | `			PH7_ClassInstanceUnref(pVm->pRefTargetThis);` |
|      26 |  148 | `		}` |
|      65 |  149 | `		pVm->pRefTargetAttr = 0;` |
|      65 |  150 | `		pVm->pRefTargetStaticAttr = 0;` |
|      65 |  151 | `		pVm->pRefTargetThis = 0;` |
|       - |  152 | `		/* Pop the member-result; leave the source as the expression value. */` |
|      65 |  153 | `		VmPopOperand(&pTos,1);` |
|      65 |  154 | `		VM_EXIT_BREAK;` |
|       - |  155 | `	}` |
|    3363 |  156 | `	if( pInstr->p3 == 0 ){` |
|       - |  157 | `		char *zName;` |
|       - |  158 | `		/* Take the variable name from the Next on the stack */` |
|     ! 0 |  159 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  160 | `			/* Force a string cast */` |
|     ! 0 |  161 | `			PH7_MemObjToString(pTos);` |
|     ! 0 |  162 | `		}` |
|     ! 0 |  163 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|     ! 0 |  164 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|     ! 0 |  165 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     ! 0 |  166 | `			if( zName ){` |
|     ! 0 |  167 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|     ! 0 |  168 | `			}` |
|     ! 0 |  169 | `		}` |
|     ! 0 |  170 | `		PH7_MemObjRelease(pTos);` |
|     ! 0 |  171 | `		pTos--;` |
|     ! 0 |  172 | `	}else{` |
|    3363 |  173 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|       - |  174 | `	}` |
|    3363 |  175 | `	if( pTos->iFlags & MEMOBJ_AUX_STROFFSET ){` |
|       - |  176 | `		/* php: a string OFFSET cannot be either end of a reference. The value read` |
|       - |  177 | `		 * out of a string still carries the BASE VARIABLE's slot, so binding it` |
|       - |  178 | `		 * aliased the whole string and a later write through the reference REPLACED` |
|       - |  179 | `		 * it ($s = "abc"; $r = &$s[1]; $r = "Z"; left $s === "Z"). Same Error the` |
|       - |  180 | `		 * by-ref ARGUMENT path raises for f($s[1]). */` |
|      10 |  181 | `		rc = VmThrowFromVm(&(*pVm),"Error","Cannot create references to/from string offsets",` |
|       - |  182 | `			sizeof("Cannot create references to/from string offsets")-1);` |
|      10 |  183 | `		PH7_MemObjRelease(pTos);` |
|      10 |  184 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|      10 |  185 | `		pTos->nIdx = SXU32_HIGH;` |
|      10 |  186 | `		if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      10 |  187 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  188 | `	}` |
|    3355 |  189 | `	nIdx = pTos->nIdx;` |
|       - |  190 | `	{` |
|       - |  191 | ``		/* `$r = &$o->p` on a property no write may reach: php refuses the BIND,`` |
|       - |  192 | `		 * because the alias would let a later write through $r reach the property` |
|       - |  193 | ``		 * with nothing in the way. PHL bound it, so `$r = 99` afterwards rewrote a`` |
|       - |  194 | `		 * readonly property and a DatePeriod's recurrence count alike. */` |
|    3355 |  195 | `		sxi32 rcNw = PH7_VmCheckIndirectModify(&(*pVm),nIdx);` |
|    3355 |  196 | `		if( rcNw != SXRET_OK ){` |
|      11 |  197 | `			PH7_MemObjRelease(pTos);` |
|      11 |  198 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      11 |  199 | `			pTos->nIdx = SXU32_HIGH;` |
|      11 |  200 | `			if( pInstr->p3 == 0 && sName.zString ){` |
|     ! 0 |  201 | `				SyMemBackendFree(&pVm->sAllocator,(void *)sName.zString);` |
|     ! 0 |  202 | `			}` |
|      11 |  203 | `			if( rcNw == PH7_ABORT ){ VM_EXIT_ABORT; }` |
|      11 |  204 | `			PH7_THROW_ROUTE_MIDEXPR(rcNw)` |
|       - |  205 | `		}` |
|       - |  206 | `	}` |
|    3345 |  207 | `	if(nIdx == SXU32_HIGH ){` |
|       - |  208 | `		{` |
|       - |  209 | `			/* No slot behind the source. php binds a FRESH variable holding the value` |
|       - |  210 | `			 * — whatever its type — and the only thing it ever says about it is the` |
|       - |  211 | ``			 * notice below, so PHL's `Reference operator require a variable not a`` |
|       - |  212 | ``			 * constant as it's right operand` fired on every scalar: `$r =& f()`,`` |
|       - |  213 | ``			 * `$r =& f()[0]` and `$r =& mk()->p` all refused the bind and left $r`` |
|       - |  214 | `			 * undefined too. (Object/array/resource sources already took this path.)` |
|       - |  215 | ``			 * A source WRITTEN as a call is php's `Only variables should be assigned`` |
|       - |  216 | ``			 * by reference`, raised when the callee did not return by reference —`` |
|       - |  217 | `			 * the compiler marked it, since a temporary the call was only the BASE of` |
|       - |  218 | ``			 * (`f()[0]`) is silent. */`` |
|       - |  219 | `			ph7_value *pObj;` |
|      22 |  220 | `			if( (pInstr->iP1 & PH7_STOREREF_CALLSRC)` |
|      17 |  221 | `			 && (pTos->iFlags & (MEMOBJ_AUX_NATIVEPROP\|MEMOBJ_AUX_REFRET)) == 0 ){` |
|       8 |  222 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|       - |  223 | `					"Only variables should be assigned by reference");` |
|       3 |  224 | `			}` |
|       - |  225 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      24 |  226 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      24 |  227 | `			if( pObj == 0 ){` |
|     ! 0 |  228 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - |  229 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|     ! 0 |  230 | `				VM_EXIT_ABORT;` |
|       - |  231 | `			}` |
|       - |  232 | `			/* Perform the store operation */` |
|      24 |  233 | `			PH7_MemObjStore(pTos,pObj);` |
|      24 |  234 | `			pTos->nIdx = pObj->nIdx;` |
|       - |  235 | `		}` |
|    3334 |  236 | `	}else if( sName.nByte > 0){` |
|    3323 |  237 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|       - |  238 | `			/* php 8.1's non-catchable fatal (compile-time there) */` |
|       3 |  239 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot acquire reference to $GLOBALS");` |
|       3 |  240 | `			pVm->iExitStatus = 255;` |
|       3 |  241 | `			pVm->bHaltRequested = 1;` |
|       3 |  242 | `			VM_EXIT_ABORT;` |
|     ! 0 |  243 | `		}else{` |
|    3320 |  244 | `			pFrameLocal = pVm->pFrame;` |
|    3320 |  245 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       - |  246 | `			/* Query the local frame */` |
|    3320 |  247 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|    3320 |  248 | `			if( pEntry ){` |
|       - |  249 | ``				/* php RE-BINDS a name that already exists (`$y = 2; $y = &$x;`, and the`` |
|       - |  250 | ``				 * `$r = &$a[$k]` idiom from the second loop step on) — the old binding`` |
|       - |  251 | `				 * goes, its value with it if nothing else holds it. */` |
|    3091 |  252 | `				PH7_VmRebindVarSlot(&(*pVm),pFrameLocal,pEntry,sName.zString,sName.nByte,nIdx);` |
|    3091 |  253 | `				if( pInstr->p3 == 0 && sName.zString ){` |
|       - |  254 | `					/* The name was duplicated for a symbol-table key this rebind does` |
|       - |  255 | `					 * not need — the entry keeps the key it was created with. */` |
|     ! 0 |  256 | `					SyMemBackendFree(&pVm->sAllocator,(void *)sName.zString);` |
|     ! 0 |  257 | `				}` |
|    1546 |  258 | `			}else{` |
|     230 |  259 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       - |  260 | `				/* Installing a name is a rebind too, for a frame that may still` |
|       - |  261 | `				 * remember where the name USED to live -- the same reason` |
|       - |  262 | `				 * PH7_VmBindVarSlot flushes on its own insert branch. */` |
|     230 |  263 | `				VmVarMemoFlush(pFrameLocal);` |
|     230 |  264 | `				if( pFrameLocal->pParent == 0 ){` |
|       - |  265 | `					/* Insert in the $GLOBALS array */` |
|     168 |  266 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|      82 |  267 | `				}` |
|     230 |  268 | `				if( rc == SXRET_OK ){` |
|     230 |  269 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|     113 |  270 | `				}` |
|       - |  271 | `			}` |
|       - |  272 | `		}` |
|    1658 |  273 | `	}` |
|    3342 |  274 | `	VM_EXIT_BREAK;` |
|     ! 0 |  275 | `	VM_EXIT_BREAK;` |
|    1718 |  276 | `}` |
|       - |  277 |  |
|       - |  278 | `/* LOAD_IDX's own iP2 context codes (they do NOT line up with PH7_MEMBER_*).` |
|       - |  279 | ` * The two UNSET codes live in ph7int.h: the property opcode has to recognize an` |
|       - |  280 | ` * unset-subscript BASE, which is an indirect modification of what the base holds. */` |
|       - |  281 | `#define VM_IDX_CTX_ISSET 4` |
|       - |  282 | `#define VM_IDX_CTX_EMPTY 6` |
|       - |  283 | `/*` |
|       - |  284 | ` * php rejects an OBJECT or an ARRAY used as an array offset, naming the offending` |
|       - |  285 | ` * type the way get_debug_type() does — the CLASS name for an object, "array" for` |
|       - |  286 | ` * an array — and wording the failure by context:` |
|       - |  287 | ` *` |
|       - |  288 | ` *   read/write   Cannot access offset of type Foo on array` |
|       - |  289 | ` *   isset/empty  Cannot access offset of type Foo in isset or empty` |
|       - |  290 | ` *   unset        Cannot unset offset of type Foo on array` |
|       - |  291 | ` *` |
|       - |  292 | ` * A RESOURCE is deliberately absent: php does not reject it, it warns` |
|       - |  293 | ` * ("Resource ID#N used as offset, casting to integer (N)") and uses the id as an` |
|       - |  294 | ` * integer key. PH7_VmOffsetResourceWarn() below handles that half.` |
|       - |  295 | ` *` |
|       - |  296 | ` * Returns TRUE and fills pMsg when the key must be rejected. iCtx is the` |
|       - |  297 | ` * instruction's iP2 (any value other than the isset/unset/empty codes reads as` |
|       - |  298 | ` * an access).` |
|       - |  299 | ` */` |
|  367176 |  300 | `static int VmOffsetTypeRejected(ph7_vm *pVm,ph7_value *pKey,int iCtx,SyBlob *pMsg)` |
|       5 |  301 | `{` |
|       - |  302 | `	const char *zType;` |
|  367181 |  303 | `	SyString *pClass = 0;` |
|  367181 |  304 | `	if( pKey == 0 ){` |
|     ! 0 |  305 | `		return FALSE;` |
|       - |  306 | `	}` |
|  367181 |  307 | `	if( pKey->iFlags & MEMOBJ_OBJ ){` |
|      37 |  308 | `		ph7_class_instance *pInst = (ph7_class_instance *)pKey->x.pOther;` |
|      37 |  309 | `		if( pInst && pInst->pClass ){` |
|      37 |  310 | `			pClass = &pInst->pClass->sName;` |
|      17 |  311 | `		}` |
|      37 |  312 | `		zType = "object";` |
|  367164 |  313 | `	}else if( pKey->iFlags & MEMOBJ_HASHMAP ){` |
|      24 |  314 | `		zType = "array";` |
|      13 |  315 | `	}else{` |
|  367125 |  316 | `		return FALSE;` |
|       - |  317 | `	}` |
|      59 |  318 | `	SyBlobInit(pMsg,&pVm->sAllocator);` |
|      59 |  319 | `	if( VM_IDX_IS_UNSET(iCtx) ){` |
|       3 |  320 | `		SyBlobAppend(pMsg,"Cannot unset offset of type ",sizeof("Cannot unset offset of type ")-1);` |
|       2 |  321 | `	}else{` |
|      57 |  322 | `		SyBlobAppend(pMsg,"Cannot access offset of type ",sizeof("Cannot access offset of type ")-1);` |
|       - |  323 | `	}` |
|      59 |  324 | `	if( pClass ){` |
|      37 |  325 | `		SyBlobAppend(pMsg,pClass->zString,pClass->nByte);` |
|      20 |  326 | `	}else{` |
|      24 |  327 | `		SyBlobAppend(pMsg,zType,(sxu32)SyStrlen(zType));` |
|       - |  328 | `	}` |
|      59 |  329 | `	if( iCtx == VM_IDX_CTX_ISSET \|\| iCtx == VM_IDX_CTX_EMPTY ){` |
|       7 |  330 | `		SyBlobAppend(pMsg," in isset or empty",sizeof(" in isset or empty")-1);` |
|       4 |  331 | `	}else{` |
|      53 |  332 | `		SyBlobAppend(pMsg," on array",sizeof(" on array")-1);` |
|       - |  333 | `	}` |
|      59 |  334 | `	return TRUE;` |
|  183424 |  335 | `}` |
|       - |  336 | `/*` |
|       - |  337 | ` * php's other half of the offset-type rules: a RESOURCE offset is accepted, with` |
|       - |  338 | `` * `Warning: Resource ID#N used as offset, casting to integer (N)`, and the key`` |
|       - |  339 | ` * becomes that integer. Rewrites pKey in place so the normal integer-key path` |
|       - |  340 | ` * takes over.` |
|       - |  341 | ` */` |
|  367318 |  342 | `PH7_PRIVATE void PH7_VmOffsetResourceWarn(ph7_vm *pVm,ph7_value *pKey)` |
|       5 |  343 | `{` |
|       - |  344 | `	sxu32 nId;` |
|  367323 |  345 | `	if( pKey == 0 \|\| (pKey->iFlags & MEMOBJ_RES) == 0 ){` |
|  367251 |  346 | `		return;` |
|       - |  347 | `	}` |
|      75 |  348 | `	nId = PH7_VmResourceId(pVm,pKey->x.pOther);` |
|     111 |  349 | `	VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|      36 |  350 | `		"Resource ID#%u used as offset, casting to integer (%u)",nId,nId);` |
|      75 |  351 | `	PH7_MemObjRelease(pKey);` |
|      75 |  352 | `	pKey->x.iVal = (sxi64)nId;` |
|      75 |  353 | `	MemObjSetType(pKey,MEMOBJ_INT);` |
|  183495 |  354 | `}` |
|       - |  355 | `/*` |
|       - |  356 | ` * php DEPRECATES (it does not reject) a NULL array offset, then normalizes it to` |
|       - |  357 | `` * the empty-string key "": `$a[null]`, `$a[$undef]` and `[null => v]` all warn`` |
|       - |  358 | `` * `Using null as an array offset is deprecated, use an empty string instead` and`` |
|       - |  359 | ` * then read/write the "" slot. Emit that E_DEPRECATED notice and return TRUE so` |
|       - |  360 | ` * the caller falls through to the ordinary lookup/insert, which already casts` |
|       - |  361 | ` * NULL->"" (HashmapLookup / HashmapInsert). A LOSSY-FLOAT offset stays a rejected` |
|       - |  362 | ` * TypeError: PHL deliberately targets php's NON-deprecated surface for the` |
|       - |  363 | ` * float->int truncation (VmRejectFloatOperand policy, §2), so only the null case` |
|       - |  364 | ` * is coerced here. Returns FALSE (no notice) for a non-null key.` |
|       - |  365 | ` */` |
|  515914 |  366 | `PH7_PRIVATE int PH7_VmNullOffsetDeprecate(ph7_vm *pVm,ph7_value *pKey)` |
|       5 |  367 | `{` |
|  515919 |  368 | `	if( pKey == 0 \|\| (pKey->iFlags & MEMOBJ_NULL) == 0 ){` |
|  515865 |  369 | `		return FALSE;` |
|       - |  370 | `	}` |
|      58 |  371 | `	PH7_VmThrowError(&(*pVm),0,E_DEPRECATED,` |
|       - |  372 | `		"Using null as an array offset is deprecated, use an empty string instead");` |
|      58 |  373 | `	return TRUE;` |
|  257865 |  374 | `}` |
|       - |  375 | `/*` |
|       - |  376 | ` * The three rules above, applied to a BUILTIN's key argument.` |
|       - |  377 | ` *` |
|       - |  378 | `` * php's array_key_exists() does not run a `string\|int` ZPP row on its $key — it`` |
|       - |  379 | `` * hands the value to the same offset machinery `$a[$key]` uses, so the two agree`` |
|       - |  380 | ` * on every type: an object or an array is the catchable` |
|       - |  381 | `` * `Cannot access offset of type X on array`, a RESOURCE warns and becomes its`` |
|       - |  382 | `` * integer id, `null` deprecates and reads the "" key, and a bool/float/numeric`` |
|       - |  383 | ` * string folds the way any subscript folds. PHL's builtin had its own narrower` |
|       - |  384 | `` * check and therefore its own answers — a `string\|int` TypeError for the two`` |
|       - |  385 | `` * cases php ACCEPTS (null, lossy float) and a silent `false` for the three php`` |
|       - |  386 | ` * REJECTS or coerces (object, array, resource). The object case was the worst of` |
|       - |  387 | ` * them: a __toString() object was stringified and could answer TRUE for a key` |
|       - |  388 | ` * php refuses to look up at all.` |
|       - |  389 | ` *` |
|       - |  390 | ` * The same rules are what php applies wherever a key arrives as a VALUE rather` |
|       - |  391 | `` * than as a subscript — a Traversable's `key()` collected by iterator_to_array()`` |
|       - |  392 | `` * or cached by CachingIterator, and array_column()'s `$index_key` COLUMN — so the`` |
|       - |  393 | ` * rail is shared by all of them. A door that skips it keys the array by the` |
|       - |  394 | `` * string CAST instead, which is a key php never writes (`"Array"`, `"Object"`,`` |
|       - |  395 | `` * `"Resource id #N"`) and, for an object, a key php refuses outright.`` |
|       - |  396 | ` *` |
|       - |  397 | ` * iWording picks which of php's sentences the caller reports (PH7_ARRAYKEY_*).` |
|       - |  398 | ` * php words the illegal-type rejection differently in the key_exists alias than in` |
|       - |  399 | `` * array_key_exists() itself — `key_exists(): Argument #1 ($key) must be a valid`` |
|       - |  400 | `` * array offset type` vs the engine's offset Error — verified against 8.5.8; the`` |
|       - |  401 | ` * null-key DEPRECATION names array_key_exists() for BOTH of those spellings and is` |
|       - |  402 | `` * the engine's own `Using null as an array offset is deprecated` everywhere else.`` |
|       - |  403 | ` *` |
|       - |  404 | ` * pKey is rewritten in place (resource -> int), so callers pass a private copy,` |
|       - |  405 | ` * not the caller's own argument slot. Returns SXRET_OK when the key is usable, or` |
|       - |  406 | ` * the status of the TypeError thrown.` |
|       - |  407 | ` */` |
|     444 |  408 | `PH7_PRIVATE sxi32 PH7_VmArrayKeyArg(ph7_context *pCtx,ph7_value *pKey,int iWording)` |
|       5 |  409 | `{` |
|     449 |  410 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - |  411 | `	SyBlob sMsg;` |
|     449 |  412 | `	if( VmOffsetTypeRejected(pVm,pKey,0,&sMsg) ){` |
|       - |  413 | `		sxi32 rc;` |
|      30 |  414 | `		if( iWording == PH7_ARRAYKEY_ZPP ){` |
|       5 |  415 | `			SyBlobRelease(&sMsg);` |
|       7 |  416 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  417 | `				"%s(): Argument #1 ($key) must be a valid array offset type",` |
|       2 |  418 | `				ph7_function_name(pCtx));` |
|       - |  419 | `		}` |
|      38 |  420 | `		rc = PH7_VmThrowException(pCtx,"TypeError","%.*s",` |
|      24 |  421 | `			(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      26 |  422 | `		SyBlobRelease(&sMsg);` |
|      26 |  423 | `		return rc;` |
|       - |  424 | `	}` |
|     421 |  425 | `	PH7_VmOffsetResourceWarn(pVm,pKey);` |
|     416 |  426 | `	if( (pKey->iFlags & MEMOBJ_REAL)` |
|     223 |  427 | `	 && pKey->rVal != (ph7_real)(sxi64)pKey->rVal ){` |
|       - |  428 | `		/* php DEPRECATES the lossy float and truncates; PHL rejects it, here as` |
|       - |  429 | ``		 * everywhere else (§10) — and with the SAME message `$a[5.7]` gives, so`` |
|       - |  430 | `		 * the builtin and the subscript stay one rule. */` |
|      15 |  431 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  432 | `			"Cannot access offset of type float on array");` |
|       - |  433 | `	}` |
|     409 |  434 | `	if( pKey->iFlags & MEMOBJ_NULL ){` |
|      10 |  435 | `		if( iWording == PH7_ARRAYKEY_OFFSET ){` |
|       7 |  436 | `			PH7_VmNullOffsetDeprecate(pVm,pKey);` |
|       4 |  437 | `		}else{` |
|       - |  438 | `			/* php names the parameter here rather than "an array offset"; the effect` |
|       - |  439 | `			 * is the engine's — the lookup below reads the "" key. */` |
|       3 |  440 | `			PH7_VmThrowError(pVm,0,E_DEPRECATED,` |
|       - |  441 | `				"Using null as the key parameter for array_key_exists() is deprecated, "` |
|       - |  442 | `				"use an empty string instead");` |
|       - |  443 | `		}` |
|       4 |  444 | `	}` |
|     409 |  445 | `	return SXRET_OK;` |
|     227 |  446 | `}` |
|       - |  447 | `/*` |
|       - |  448 | ` * Does this string START with an integer php's is_numeric_string would answer` |
|       - |  449 | ` * IS_LONG for? Returns 0 when it does not — no digits at all ("", "-", "p"), a` |
|       - |  450 | ` * FLOAT-shaped prefix ("1.5", ".5", "1.", "1e2", "1E2x") or a run that overflows` |
|       - |  451 | ` * a signed 64-bit int ("9223372036854775808") — 1 when it does and only optional` |
|       - |  452 | ` * trailing WHITESPACE follows (" 12 ", "+12", "007"), and 2 when it does but` |
|       - |  453 | ` * trailing DATA follows ("12abc", "0x1", "1e", "1 x"), which is php's` |
|       - |  454 | `` * `Illegal string offset` warning. *piVal receives the integer for 1 and 2.`` |
|       - |  455 | ` *` |
|       - |  456 | ` * php reaches the same three answers through is_numeric_string_ex(allow_errors=1)` |
|       - |  457 | ` * plus its IS_LONG/IS_DOUBLE verdict: a fraction or a well-formed exponent makes` |
|       - |  458 | ` * the verdict IS_DOUBLE, which a string offset refuses, while a bare 'e' is just` |
|       - |  459 | ` * trailing data.` |
|       - |  460 | ` */` |
|     104 |  461 | `static int VmStringOffsetInt(const char *zIn,sxu32 nByte,sxi64 *piVal)` |
|       3 |  462 | `{` |
|     107 |  463 | `	const char *z = zIn, *zEnd = &zIn[nByte], *zDigit;` |
|     107 |  464 | `	sxu64 uVal = 0, uLimit;` |
|     107 |  465 | `	int isNeg = 0, nDigit, i;` |
|     119 |  466 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){` |
|      14 |  467 | `		z++;` |
|       2 |  468 | `	}` |
|     107 |  469 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|       7 |  470 | `		isNeg = z[0] == '-';` |
|       7 |  471 | `		z++;` |
|       3 |  472 | `	}` |
|     107 |  473 | `	zDigit = z;` |
|     247 |  474 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|     142 |  475 | `		z++;` |
|       2 |  476 | `	}` |
|     107 |  477 | `	nDigit = (int)(z - zDigit);` |
|     107 |  478 | `	if( nDigit < 1 ){` |
|      43 |  479 | `		return 0;` |
|       - |  480 | `	}` |
|      66 |  481 | `	if( z < zEnd && z[0] == '.' ){` |
|       - |  482 | `		/* "1." and "1.5" alike: php reads a double from here. */` |
|       5 |  483 | `		return 0;` |
|       - |  484 | `	}` |
|      62 |  485 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|       3 |  486 | `		const char *zExp = &z[1];` |
|       3 |  487 | `		if( zExp < zEnd && (zExp[0] == '+' \|\| zExp[0] == '-') ){` |
|     ! 0 |  488 | `			zExp++;` |
|     ! 0 |  489 | `		}` |
|       3 |  490 | `		if( zExp < zEnd && (unsigned char)zExp[0] < 0xc0 && SyisDigit(zExp[0]) ){` |
|       3 |  491 | `			return 0;` |
|       - |  492 | `		}` |
|     ! 0 |  493 | `	}` |
|       - |  494 | `	/* Accumulate unsigned so PHP_INT_MIN's magnitude (2^63) is representable —` |
|       - |  495 | `	 * "-9223372036854775808" is a legal offset, "9223372036854775808" is not. */` |
|      64 |  496 | `	while( nDigit > 1 && zDigit[0] == '0' ){` |
|       5 |  497 | `		zDigit++; nDigit--;` |
|       1 |  498 | `	}` |
|      60 |  499 | `	uLimit = isNeg ? (sxu64)SXI64_HIGH + 1 : (sxu64)SXI64_HIGH;` |
|      60 |  500 | `	if( nDigit > 19 ){` |
|     ! 0 |  501 | `		return 0;` |
|       - |  502 | `	}` |
|     188 |  503 | `	for( i = 0 ; i < nDigit ; ++i ){` |
|     132 |  504 | `		sxu64 d = (sxu64)(zDigit[i] - '0');` |
|     132 |  505 | `		if( uVal > (uLimit - d)/10 ){` |
|       3 |  506 | `			return 0;` |
|       - |  507 | `		}` |
|     130 |  508 | `		uVal = uVal*10 + d;` |
|      66 |  509 | `	}` |
|      58 |  510 | `	*piVal = isNeg ? (sxi64)(~uVal + 1) : (sxi64)uVal;` |
|      68 |  511 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){` |
|      12 |  512 | `		z++;` |
|       2 |  513 | `	}` |
|      58 |  514 | `	return z == zEnd ? 1 : 2;` |
|      55 |  515 | `}` |
|       - |  516 | `/*` |
|       - |  517 | ` * php's offset rules for a STRING container (zend_check_string_offset, and the` |
|       - |  518 | ` * isset/empty arm of ZEND_ISSET_ISEMPTY_DIM_OBJ). They are NOT the array rules,` |
|       - |  519 | ` * and PHL applied none of them: every offset went through an int cast, so` |
|       - |  520 | `` * `$s[""]`, `$s["-"]` and `$s["p"]` all answered `$s[0]` — a silent wrong answer`` |
|       - |  521 | ` * on code php refuses to run. php's table:` |
|       - |  522 | ` *` |
|       - |  523 | ` *   int                     the offset` |
|       - |  524 | ` *   null / bool / float     Warning: String offset cast occurred, then cast` |
|       - |  525 | ` *   integer-shaped string   the offset (leading/trailing space and '+' allowed)` |
|       - |  526 | ` *   int-then-garbage string Warning: Illegal string offset "12abc", then 12` |
|       - |  527 | ` *   any other string        TypeError: Cannot access offset of type string on string` |
|       - |  528 | ` *   array/object/resource   TypeError, naming the type (an object's CLASS)` |
|       - |  529 | ` *` |
|       - |  530 | ` * isset()/empty() raise NOTHING and answer "not set" for every shape the read` |
|       - |  531 | `` * path would reject OR warn about: `isset($s["0x1"])` is false even though`` |
|       - |  532 | `` * reading it warns and yields `$s[0]`. A `??` fetch sits BETWEEN that and a real`` |
|       - |  533 | ` * read — it suppresses the not-set diagnostics but still warns about the offset` |
|       - |  534 | ` * SHAPE and still reads it. iLevel selects which of the three (VM_STROFF_LOUD /` |
|       - |  535 | ` * _COALESCE / _ISSET).` |
|       - |  536 | ` */` |
|  951715 |  537 | `PH7_PRIVATE int VmStringOffsetResolve(ph7_vm *pVm,ph7_value *pIdx,int iLevel,sxi64 *piOfft,SyBlob *pMsg)` |
|       5 |  538 | `{` |
|  951720 |  539 | `	if( (pIdx->iFlags & MEMOBJ_INT) != 0 && (pIdx->iFlags & MEMOBJ_REAL) == 0 ){` |
|  951548 |  540 | `		*piOfft = pIdx->x.iVal;` |
|  951548 |  541 | `		return VM_STROFF_OK;` |
|       - |  542 | `	}` |
|     175 |  543 | `	if( pIdx->iFlags & MEMOBJ_STRING ){` |
|     159 |  544 | `		int eInt = VmStringOffsetInt((const char *)SyBlobData(&pIdx->sBlob),` |
|      52 |  545 | `			SyBlobLength(&pIdx->sBlob),piOfft);` |
|     107 |  546 | `		if( eInt == 1 ){` |
|      24 |  547 | `			return VM_STROFF_OK;` |
|       - |  548 | `		}` |
|      85 |  549 | `		if( eInt == 2 && iLevel != VM_STROFF_ISSET ){` |
|       - |  550 | ``			/* int-then-garbage. This is the one diagnostic a `??` fetch keeps:`` |
|       - |  551 | ``			 * `$s["1x"] ?? "d"` warns and answers $s[1] (PHL called it "not set"`` |
|       - |  552 | `			 * and answered the default — a wrong VALUE, not just a missing` |
|       - |  553 | `			 * warning). Only isset()/empty() — and the intermediate step of an` |
|       - |  554 | `			 * unset chain, which php keeps quiet about the shape — stay silent. */` |
|      28 |  555 | `			if( iLevel != VM_STROFF_UNSETBASE ){` |
|       - |  556 | `				SyString sKey;` |
|      28 |  557 | `				SyStringInitFromBuf(&sKey,SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob));` |
|      28 |  558 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Illegal string offset \"%z\"",&sKey);` |
|      13 |  559 | `			}` |
|      28 |  560 | `			return VM_STROFF_OK;` |
|       - |  561 | `		}` |
|      59 |  562 | `		if( iLevel != VM_STROFF_LOUD && iLevel != VM_STROFF_UNSETBASE ){` |
|      24 |  563 | `			return VM_STROFF_MISS;` |
|       3 |  564 | `		}` |
|      87 |  565 | `	}else if( (pIdx->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|       - |  566 | `		/* null / bool / float: php casts, but says so in a real read or write —` |
|       - |  567 | `		 * and in the intermediate step of an unset chain, which reads the offset` |
|       - |  568 | ``		 * to hand it on (`unset($s[1.5][0])` warns about the cast). */`` |
|      48 |  569 | `		if( iLevel == VM_STROFF_LOUD \|\| iLevel == VM_STROFF_UNSETBASE ){` |
|      34 |  570 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"String offset cast occurred");` |
|      16 |  571 | `		}` |
|      48 |  572 | `		PH7_MemObjToInteger(pIdx);` |
|      48 |  573 | `		*piOfft = pIdx->x.iVal;` |
|      48 |  574 | `		return VM_STROFF_OK;` |
|      24 |  575 | `	}else if( iLevel == VM_STROFF_ISSET ){` |
|       - |  576 | `		/* An array/object/resource offset is "not set" for isset()/empty() — but a` |
|       - |  577 | ``		 * `??` fetch RAISES for it (probed: `$s[[]] ?? "d"` is the TypeError while`` |
|       - |  578 | ``		 * `isset($s[[]])` is false), so only the fully-quiet level answers MISS. */`` |
|      10 |  579 | `		return VM_STROFF_MISS;` |
|       - |  580 | `	}` |
|       - |  581 | `	{` |
|       - |  582 | `		char zBuf[128];` |
|      51 |  583 | `		SyBlobInit(pMsg,&pVm->sAllocator);` |
|      75 |  584 | `		SyBlobFormat(pMsg,"Cannot access offset of type %s on string",` |
|      24 |  585 | `			VmValueGivenName(pIdx,zBuf,sizeof(zBuf)));` |
|       - |  586 | `	}` |
|      51 |  587 | `	return VM_STROFF_REJECT;` |
|  477558 |  588 | `}` |
|       - |  589 | `/*` |
|       - |  590 | ` * OP_STORE_IDX_REF: body moved verbatim from the OP_STORE_IDX_REF arm of` |
|       - |  591 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  592 | ` */` |
|  516836 |  593 | `PH7_PRIVATE VmOpRc VmExecOpStoreIdxRef(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  594 | `{` |
|  516841 |  595 | `	ph7_value *pTos = pState->pTos;` |
|  516841 |  596 | `	ph7_value *pStack = pState->pStack;` |
|  516841 |  597 | `	VmInstr *aInstr = pState->aInstr;` |
|  516841 |  598 | `	sxi32 pc = pState->pc;` |
|       - |  599 | `	sxi32 rc;` |
|  258321 |  600 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|  516841 |  601 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|       - |  602 | `	ph7_value *pKey;` |
|       - |  603 | `	sxu32 nIdx;` |
|  516841 |  604 | `	if( pInstr->iP1 & 1 ){` |
|       - |  605 | `		/* Key is next on stack (bit 1 is PH7_STOREREF_CALLSRC, not a key) */` |
|   91467 |  606 | `		pKey = pTos;` |
|   91467 |  607 | `		pTos--;` |
|   45653 |  608 | `	}else{` |
|  425379 |  609 | `		pKey = 0;` |
|       - |  610 | `	}` |
|       - |  611 | `		/* php DEPRECATES a null / lossy-float write subscript (then normalizes "" /` |
|       - |  612 | ``		 * truncates); it does not reject either. A null key — by-VALUE (`$a[$k]=v`)`` |
|       - |  613 | ``		 * OR by-REF (`$a[$k]=&$x`) — deprecates + coerces to the "" key: the notice is`` |
|       - |  614 | `		 * emitted DOWN AT THE INSERT (below), not here, so a null/false container that` |
|       - |  615 | ``		 * auto-vivifies (`$x=null; $x[null]=v`) gets it too (the base is not yet a`` |
|       - |  616 | `		 * hashmap at this point), and PH7_HashmapInsert / HashmapInsertByRef both cast` |
|       - |  617 | `		 * NULL->"". A lossy-FLOAT subscript stays a loud TypeError in BOTH forms (the` |
|       - |  618 | `		 * recorded non-deprecated-surface policy, §2). */` |
|  516841 |  619 | `		if( pKey && (pTos->iFlags & MEMOBJ_HASHMAP) ){` |
|       - |  620 | `			SyBlob sTypeMsg;` |
|       - |  621 | `			/* An object/array key is php's TypeError; a resource key warns and` |
|       - |  622 | `			 * becomes its integer id. Both used to be stringified silently. */` |
|   90415 |  623 | `			if( VmOffsetTypeRejected(&(*pVm),pKey,0,&sTypeMsg) ){` |
|       - |  624 | `				sxi32 rcSc;` |
|       8 |  625 | `				PH7_MemObjRelease(pKey);` |
|       8 |  626 | `				VmPopOperand(&pTos,1);` |
|       8 |  627 | `				rcSc = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|      12 |  628 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       8 |  629 | `				rc = rcSc;` |
|       8 |  630 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  631 | `			}` |
|   90409 |  632 | `			PH7_VmOffsetResourceWarn(&(*pVm),pKey);` |
|   90404 |  633 | `			if( (pKey->iFlags & MEMOBJ_REAL)` |
|   45127 |  634 | `			 && pKey->rVal != (ph7_real)(sxi64)pKey->rVal ){` |
|       - |  635 | `				sxi32 rcSc;` |
|       3 |  636 | `				const char *zErr = "Cannot access offset of type float on array";` |
|       3 |  637 | `				PH7_MemObjRelease(pKey);` |
|       3 |  638 | `				VmPopOperand(&pTos,1);` |
|       3 |  639 | `				rcSc = VmThrowFromVm(&(*pVm),"TypeError",zErr,(sxu32)SyStrlen(zErr));` |
|       3 |  640 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       3 |  641 | `				rc = rcSc;` |
|       3 |  642 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  643 | `			}` |
|   45118 |  644 | `		}` |
|  516833 |  645 | `	if( pTos->iFlags & MEMOBJ_AUX_STROFFSET ){` |
|       - |  646 | ``		/* The CONTAINER is a string offset: `$s[0][1] = 'x'`, `$s[0][] = 'x'`,`` |
|       - |  647 | ``		 * `$a['k'][0][1] = 'x'`. php refuses to reach inside one — `Cannot use string`` |
|       - |  648 | ``		 * offset as an array`, the same refusal the fetch path raises — and this is`` |
|       - |  649 | `		 * where the outermost level of an ordinary assignment arrives, its LOAD_IDX` |
|       - |  650 | `		 * folded into the store. The character read out of the string still carries` |
|       - |  651 | ``		 * the BASE STRING's slot, so the write landed ON the base: `$s = 'ab';`` |
|       - |  652 | ``		 * $s[0][1] = 'x';` left `$s === 'ax'`, in silence. */`` |
|       - |  653 | `		sxi32 rcSo;` |
|       9 |  654 | `		if( pKey ){` |
|       7 |  655 | `			PH7_MemObjRelease(pKey);` |
|       3 |  656 | `		}` |
|       9 |  657 | `		VmPopOperand(&pTos,1);` |
|       9 |  658 | `		rcSo = VmThrowFromVm(&(*pVm),"Error","Cannot use string offset as an array",` |
|       - |  659 | `			sizeof("Cannot use string offset as an array")-1);` |
|       9 |  660 | `		if( rcSo == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       9 |  661 | `		rc = rcSo;` |
|       9 |  662 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  663 | `	}` |
|  516825 |  664 | `	nIdx = pTos->nIdx;` |
|       - |  665 | `	{` |
|       - |  666 | `		/* ArrayAccess::offsetSet dispatch.` |
|       - |  667 | `		 * Container may be on the stack as MEMOBJ_OBJ, or referenced via` |
|       - |  668 | `		 * the backing variable slot at nIdx. */` |
|  516825 |  669 | `		ph7_class_instance *pInst = 0;` |
|  516825 |  670 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|     591 |  671 | `			pInst = (ph7_class_instance *)pTos->x.pOther;` |
|  516532 |  672 | `		}else if( nIdx != SXU32_HIGH ){` |
|  516163 |  673 | `			ph7_value *pBacking = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
|  516163 |  674 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_OBJ) ){` |
|     ! 0 |  675 | `				pInst = (ph7_class_instance *)pBacking->x.pOther;` |
|     ! 0 |  676 | `			}` |
|  257982 |  677 | `		}` |
|  516825 |  678 | `		if( pInst ){` |
|     591 |  679 | `			ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|     591 |  680 | `			if( pArrayAccess && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|       - |  681 | `				ph7_class_method *pMeth;` |
|       - |  682 | `				ph7_value sNullKey;` |
|       - |  683 | `				ph7_value *apArg[2];` |
|     551 |  684 | `				if( pInstr->iOp == PH7_OP_STORE_IDX_REF ){` |
|     ! 0 |  685 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - |  686 | `						"Cannot assign by reference to overloaded object");` |
|     ! 0 |  687 | `					if( pKey ){ PH7_MemObjRelease(pKey); }` |
|     ! 0 |  688 | `					VmPopOperand(&pTos,2); /* container + value */` |
|     ! 0 |  689 | `					VM_EXIT_BREAK;` |
|       - |  690 | `				}` |
|     551 |  691 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - |  692 | `					"offsetSet",sizeof("offsetSet")-1);` |
|       - |  693 | `				/* Pop container; pTos now points to the value */` |
|     551 |  694 | `				VmPopOperand(&pTos,1);` |
|     551 |  695 | `				if( pKey == 0 ){` |
|      18 |  696 | `					PH7_MemObjInit(&(*pVm),&sNullKey);` |
|      18 |  697 | `					apArg[0] = &sNullKey;` |
|      10 |  698 | `				}else{` |
|     535 |  699 | `					apArg[0] = pKey;` |
|       - |  700 | `				}` |
|     551 |  701 | `				apArg[1] = pTos;` |
|     551 |  702 | `				if( pMeth ){` |
|     551 |  703 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,0,2,apArg);` |
|     273 |  704 | `				}` |
|     551 |  705 | `				if( pKey ){` |
|     535 |  706 | `					PH7_MemObjRelease(pKey);` |
|     270 |  707 | `				}else{` |
|      18 |  708 | `					PH7_MemObjRelease(&sNullKey);` |
|       - |  709 | `				}` |
|       - |  710 | `				/* The VALUE stays on the stack: a store IS an expression and its` |
|       - |  711 | `` 				 * result is what was assigned, which is why `$x = ($c[$k] = 42)` `` |
|       - |  712 | ``				 * and `return $this[$k] = 42;` are 42 in php even when offsetSet`` |
|       - |  713 | `				 * stored something else. This arm popped it, so the expression it` |
|       - |  714 | ``				 * belongs to read a slot the stack no longer owned -- `var_dump($c[$k]`` |
|       - |  715 | ``				 * = 5)` printed garbage for a native ArrayAccess and SEGFAULTED for a`` |
|       - |  716 | `				 * user one. The ordinary hashmap store below pops only the container,` |
|       - |  717 | `				 * for exactly this reason. */` |
|     551 |  718 | `				VM_EXIT_BREAK;` |
|       - |  719 | `			}` |
|       - |  720 | `			/* Object without ArrayAccess, but with a dimension handler that` |
|       - |  721 | `			 * really STORES: php's SimpleXMLElement writes an attribute for` |
|       - |  722 | ``			 * `$x['a'] = '1'` and appends an element for `$x->kid[] = 'v'`,`` |
|       - |  723 | `			 * through a write_dimension it has instead of the interface. The` |
|       - |  724 | `			 * handler is offered the write before the refusal below, and takes` |
|       - |  725 | `			 * it or leaves it -- DOMNodeList and PDORow leave it. */` |
|      40 |  726 | `			if( pInstr->iOp == PH7_OP_STORE_IDX_REF` |
|      22 |  727 | `			 && PH7_ClassHasNativeDim(pInst->pClass) ){` |
|       - |  728 | `				/* php words a by-reference store into a class whose OWN handler` |
|       - |  729 | `				 * answers dimensions differently from one that answers none:` |
|       - |  730 | `` 				 * `Cannot assign by reference to an array dimension of an object` `` |
|       - |  731 | ``				 * rather than `Cannot use object of type C as array`, which stays`` |
|       - |  732 | `				 * the answer for a plain object. (An ArrayAccess class has its own` |
|       - |  733 | `				 * third sentence, raised above.) */` |
|     ! 0 |  734 | `				const char *zRef = "Cannot assign by reference to an array dimension of an object";` |
|     ! 0 |  735 | `				rc = VmThrowFromVm(pVm,"Error",zRef,(sxu32)SyStrlen(zRef));` |
|     ! 0 |  736 | `				if( pKey ){ PH7_MemObjRelease(pKey); }` |
|     ! 0 |  737 | `				VmPopOperand(&pTos,2);` |
|     ! 0 |  738 | `				if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     ! 0 |  739 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  740 | `			}` |
|      42 |  741 | `			if( pInstr->iOp != PH7_OP_STORE_IDX_REF ){` |
|       - |  742 | `				PH7_NativeDimCtx sDim;` |
|       - |  743 | `				/* The stack is [value, container]; the container is popped only` |
|       - |  744 | `				 * once the handler has taken the write, so the refusal below` |
|       - |  745 | `				 * still sees both operands. */` |
|      62 |  746 | `				if( PH7_ClassNativeDimStore(pInst,` |
|      20 |  747 | `					pKey == 0 ? PH7_NATIVE_DIM_APPEND : PH7_NATIVE_DIM_WRITE,` |
|      20 |  748 | `					pKey,pTos - 1,&sDim) ){` |
|      34 |  749 | `					if( pKey ){ PH7_MemObjRelease(pKey); }` |
|      34 |  750 | `					if( sDim.zThrowClass ){` |
|       - |  751 | `						/* The refusal takes BOTH operands, like the ordinary one` |
|       - |  752 | `						 * below: the store never happened, so its value is not the` |
|       - |  753 | `						 * expression's result. */` |
|       8 |  754 | `						VmPopOperand(&pTos,2);` |
|      11 |  755 | `						rc = VmThrowFromVm(pVm,sDim.zThrowClass,sDim.zThrowMsg,` |
|       6 |  756 | `							(sxu32)SyStrlen(sDim.zThrowMsg));` |
|      24 |  757 | `						if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       8 |  758 | `						PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  759 | `					}` |
|      27 |  760 | `					VmPopOperand(&pTos,1);` |
|       - |  761 | `					/* The VALUE stays on the stack: a store IS an expression. */` |
|      27 |  762 | `					VM_EXIT_BREAK;` |
|       - |  763 | `				}` |
|       4 |  764 | `			}` |
|       - |  765 | `			/* Otherwise: PHP throws a fatal Error rather` |
|       - |  766 | `			 * than silently coercing the object into a hashmap (which is` |
|       - |  767 | `			 * what the legacy PH7 fall-through would do via MemObjToHashmap` |
|       - |  768 | `			 * a few lines below). Match PHP -- and let a class whose READ` |
|       - |  769 | `			 * handler answers word its own refusal, which is how php's` |
|       - |  770 | ``			 * PDORow says `Cannot write to PDORow offset` and, for the`` |
|       - |  771 | ``			 * keyless spelling, `Cannot append to PDORow offset`. */`` |
|       - |  772 | `			{` |
|       - |  773 | `				char zMsg[256];` |
|      14 |  774 | `				sxu32 nMsg = PH7_ClassNativeDimRefusal(pInst,` |
|       4 |  775 | `					pKey == 0 ? PH7_NATIVE_DIM_APPEND : PH7_NATIVE_DIM_WRITE,` |
|       4 |  776 | `					zMsg,sizeof(zMsg));` |
|      10 |  777 | `				rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|      10 |  778 | `				if( pKey ){ PH7_MemObjRelease(pKey); }` |
|      10 |  779 | `				VmPopOperand(&pTos,2); /* container + value */` |
|      10 |  780 | `				if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      10 |  781 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  782 | `			}` |
|       - |  783 | `		}` |
|       - |  784 | `	}` |
|  516239 |  785 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       - |  786 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|       - |  787 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|       - |  788 | `		 * checking true sharing count, then re-add after separation. */` |
|  515725 |  789 | `		if( nIdx != SXU32_HIGH ){` |
|  515675 |  790 | `			ph7_value *pBacking = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
|  773607 |  791 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|  515675 |  792 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|       - |  793 | `				/* Only adjust refcount / perform COW if the backing variable` |
|       - |  794 | `				 * is still sharing the same hashmap instance. This mirrors` |
|       - |  795 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|       - |  796 | `				 * refcounts if the backing array was already separated. */` |
|  515675 |  797 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|  515675 |  798 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|  515675 |  799 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|  515675 |  800 | `					pMap->iRef++;  /* Re-add stack ref */` |
|  515675 |  801 | `					pTos->x.pOther = pMap;` |
|  257743 |  802 | `				}else{` |
|       - |  803 | `					/* Backing variable no longer points at pCur: skip COW here` |
|       - |  804 | `					 * and operate on the hashmap currently on the stack. */` |
|     ! 0 |  805 | `					pMap = pCur;` |
|       - |  806 | `				}` |
|  257743 |  807 | `			}else{` |
|     ! 0 |  808 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       - |  809 | `			}` |
|  257743 |  810 | `		}else{` |
|      51 |  811 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       - |  812 | `		}` |
|  515725 |  813 | `		if( pMap->iRef < 2 ){` |
|       - |  814 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|       - |  815 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|       - |  816 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|       - |  817 | `			 * no code checks iRef for COW decisions. */` |
|      49 |  818 | `			pMap->iRef = 2;` |
|      24 |  819 | `		}` |
|  257768 |  820 | `	}else{` |
|       - |  821 | `		ph7_value *pObj;` |
|     519 |  822 | `		pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
|     519 |  823 | `		if( pObj == 0 ){` |
|       - |  824 | `			/* No slot behind the container: this is a write THROUGH a TEMPORARY` |
|       - |  825 | ``			 * (`f()[0] = 5`, `f()[0][1] = 5`). php still screens the base's TYPE the`` |
|       - |  826 | `			 * same way it does for a variable — writing an index into an int, a` |
|       - |  827 | `			 * float, a resource or a bool is its catchable "Cannot use a scalar value` |
|       - |  828 | `			 * as an array" — and only the value it writes is discarded with the` |
|       - |  829 | `			 * temporary. PHL skipped the screen along with the write, so the whole` |
|       - |  830 | `			 * statement ran in silence. A NULL base keeps php's silence: the array it` |
|       - |  831 | `			 * vivifies into dies with the temporary, and so does an offset written` |
|       - |  832 | `			 * into a temporary STRING. */` |
|      27 |  833 | `			if( (pTos->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_RES\|MEMOBJ_BOOL)) != 0 ){` |
|       - |  834 | `				sxi32 rcSc;` |
|      13 |  835 | `				if( pKey ){` |
|      13 |  836 | `					PH7_MemObjRelease(pKey);` |
|       6 |  837 | `				}` |
|      13 |  838 | `				VmPopOperand(&pTos,1);` |
|      13 |  839 | `				rcSc = VmThrowFromVm(&(*pVm),"Error","Cannot use a scalar value as an array",` |
|       - |  840 | `					sizeof("Cannot use a scalar value as an array")-1);` |
|      13 |  841 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      13 |  842 | `				rc = rcSc;` |
|      21 |  843 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  844 | `			}` |
|      15 |  845 | `			if( pKey ){` |
|      15 |  846 | `			  PH7_MemObjRelease(pKey);` |
|       7 |  847 | `			}` |
|      15 |  848 | `			VmPopOperand(&pTos,1);` |
|      15 |  849 | `			VM_EXIT_BREAK;` |
|       - |  850 | `		}` |
|       - |  851 | `		/* Phase#1: Load the array */` |
|     493 |  852 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|     431 |  853 | `			VmPopOperand(&pTos,1);` |
|     431 |  854 | `			if( pKey == 0 ){` |
|       - |  855 | ``				/* `$s[] = 'x'` on a STRING: php raises the catchable Error`` |
|       - |  856 | `				 * "[] operator not supported for strings" and leaves the string` |
|       - |  857 | `				 * untouched. PHL silently APPENDED, so code that meant to build` |
|       - |  858 | `				 * an array from a variable holding a string quietly produced a` |
|       - |  859 | `				 * longer string instead of failing — a wrong answer, not a` |
|       - |  860 | `				 * missing diagnostic. */` |
|       - |  861 | `				SyBlob sErrMsg;` |
|       8 |  862 | `				SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       8 |  863 | `				SyBlobAppend(&sErrMsg,"[] operator not supported for strings",` |
|       - |  864 | `					sizeof("[] operator not supported for strings")-1);` |
|       8 |  865 | `				VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       8 |  866 | `				VM_EXIT_BREAK;` |
|     ! 0 |  867 | `			}else{` |
|     425 |  868 | `				sxi64 iOfft = 0;` |
|       - |  869 | `				SyBlob sTypeMsg;` |
|       - |  870 | `				/* php's offset rules run BEFORE the RHS is looked at: an offset it` |
|       - |  871 | `				 * refuses is the TypeError alone. The RHS cast below used to happen` |
|       - |  872 | ``				 * first, so every rejected shape — and `$s[] = [1,2]` above — came`` |
|       - |  873 | ``				 * with a spurious `Array to string conversion` in front of it. */`` |
|     425 |  874 | `				if( VmStringOffsetResolve(&(*pVm),pKey,VM_STROFF_LOUD,&iOfft,&sTypeMsg) == VM_STROFF_REJECT ){` |
|       - |  875 | `					sxi32 rcSc;` |
|       7 |  876 | `					PH7_MemObjRelease(pKey);` |
|       7 |  877 | `					rcSc = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|      16 |  878 | `					if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       7 |  879 | `					rc = rcSc;` |
|       7 |  880 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  881 | `				}` |
|       - |  882 | `				/* Force a string cast on the RHS (user-visible: an array warns` |
|       - |  883 | `				 * "Array to string conversion" before the offset write, §2, and a` |
|       - |  884 | `				 * not-stringable object throws — the target string is untouched) */` |
|       - |  885 | `				{` |
|     419 |  886 | `					sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|     419 |  887 | `					if( rcSv != SXRET_OK ){` |
|       5 |  888 | `						PH7_MemObjRelease(pKey);` |
|       7 |  889 | `						PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|     ! 0 |  890 | `					}` |
|       - |  891 | `				}` |
|     415 |  892 | `				if( VmStringOffsetWrite(&(*pVm),pObj,iOfft,pTos) != SXRET_OK ){` |
|       - |  893 | `					sxi32 rcEm;` |
|       9 |  894 | `					PH7_MemObjRelease(pKey);` |
|       9 |  895 | `					rcEm = VmThrowFromVm(&(*pVm),"Error",` |
|       - |  896 | `						"Cannot assign an empty string to a string offset",` |
|       - |  897 | `						sizeof("Cannot assign an empty string to a string offset")-1);` |
|       9 |  898 | `					if( rcEm == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       9 |  899 | `					rc = rcEm;` |
|       9 |  900 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  901 | `				}` |
|       - |  902 | `			}` |
|     407 |  903 | `			if( pKey ){` |
|     407 |  904 | `			  PH7_MemObjRelease(pKey);` |
|     201 |  905 | `			}` |
|     407 |  906 | `			VM_EXIT_BREAK;` |
|      66 |  907 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       - |  908 | `			/* php: only NULL and FALSE auto-vivify into an array. Writing an index into an` |
|       - |  909 | `			 * int/float/resource/TRUE is a catchable Error -- PH7 quietly REPLACED the value` |
|       - |  910 | `			 * with an array, destroying it ($x = 5; $x[0] = 1; left $x === [1]). */` |
|       - |  911 | `			/* php auto-vivifies false into an array with an 8.1 DEPRECATION; PHL` |
|       - |  912 | `			 * rejects any scalar base, false included (null still auto-vivifies). */` |
|      66 |  913 | `			int bScalar = (pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_RES\|MEMOBJ_BOOL)) != 0;` |
|      66 |  914 | `			if( bScalar ){` |
|       - |  915 | `				sxi32 rcSc;` |
|      10 |  916 | `				if( pKey ){` |
|       7 |  917 | `					PH7_MemObjRelease(pKey);` |
|       3 |  918 | `				}` |
|      10 |  919 | `				VmPopOperand(&pTos,1);` |
|      10 |  920 | `				rcSc = VmThrowFromVm(&(*pVm),"Error","Cannot use a scalar value as an array",` |
|       - |  921 | `					sizeof("Cannot use a scalar value as an array")-1);` |
|      10 |  922 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      10 |  923 | `				rc = rcSc;` |
|      12 |  924 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  925 | `			}` |
|       - |  926 | `			/* Force a hashmap cast  */` |
|      58 |  927 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      58 |  928 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  929 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|     ! 0 |  930 | `				VM_EXIT_ABORT;` |
|       - |  931 | `			}` |
|      27 |  932 | `		}` |
|       - |  933 | `		/* COW separate the backing variable before mutation */` |
|      58 |  934 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|       - |  935 | `	}` |
|  515779 |  936 | `	VmPopOperand(&pTos,1);` |
|       - |  937 | `	/* Phase#2: Perform the insertion. A null key deprecates + normalizes to "" for` |
|       - |  938 | `	 * BOTH the by-value and the by-ref store — emitted HERE, after a null/false` |
|       - |  939 | ``	 * container has auto-vivified to an array, so `$x[null]=v` and `$x[null]=&$y` on`` |
|       - |  940 | `	 * an undefined $x get the notice too (the top-of-handler check runs before` |
|       - |  941 | `	 * vivification, when the base is not yet a hashmap). HashmapInsert /` |
|       - |  942 | `	 * HashmapInsertByRef both then cast NULL->"". A null pKey==0 (an append, no key)` |
|       - |  943 | `	 * is not a null OFFSET and is left alone. */` |
|  515779 |  944 | `	PH7_VmNullOffsetDeprecate(&(*pVm),pKey);` |
|  515774 |  945 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF` |
|  257844 |  946 | `	 && (pInstr->iP1 & PH7_STOREREF_CALLSRC)` |
|      58 |  947 | `	 && (pTos->iFlags & MEMOBJ_AUX_STROFFSET) == 0` |
|      13 |  948 | `	 && pTos->nIdx == SXU32_HIGH ){` |
|       - |  949 | ``		/* `$a[] =& f()` / `$a[$k] =& f()`: the source was written as a CALL and the`` |
|       - |  950 | `		 * callee did not return by reference, so php binds the value it answered and` |
|       - |  951 | ``		 * says so. Same notice the plain `$r =& f()` bind raises. */`` |
|       5 |  952 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|       - |  953 | `			"Only variables should be assigned by reference");` |
|       2 |  954 | `	}` |
|  515779 |  955 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && (pTos->iFlags & MEMOBJ_AUX_STROFFSET) ){` |
|       - |  956 | ``		/* `$a[] = &$s[1]`: the source is a string OFFSET, which php refuses to`` |
|       - |  957 | `		 * reference — and whose slot index is the BASE STRING's, so binding it` |
|       - |  958 | ``		 * aliased the whole string. Same Error the `=&` / by-ref-argument paths`` |
|       - |  959 | `		 * raise. The key is already popped; drop it and abandon the store. */` |
|       - |  960 | `		sxi32 rcSc;` |
|       5 |  961 | `		if( pKey ){` |
|       3 |  962 | `			PH7_MemObjRelease(pKey);` |
|       1 |  963 | `		}` |
|       5 |  964 | `		rcSc = VmThrowFromVm(&(*pVm),"Error","Cannot create references to/from string offsets",` |
|       - |  965 | `			sizeof("Cannot create references to/from string offsets")-1);` |
|       5 |  966 | `		if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       5 |  967 | `		rc = rcSc;` |
|       5 |  968 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  969 | `	}` |
|  515775 |  970 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|     102 |  971 | `		if( pMap == pVm->pGlobal ){` |
|       - |  972 | `			/* php 8.1: $GLOBALS['y'] =& $x binds the global $y to $x's` |
|       - |  973 | `			 * slot; an append has no name to bind (catchable Error). */` |
|      41 |  974 | `			if( pKey == 0 ){` |
|     ! 0 |  975 | `				rc = PH7_VmThrowGlobalsAppendError(&(*pVm));` |
|     ! 0 |  976 | `			}else{` |
|      41 |  977 | `				if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 |  978 | `					PH7_MemObjToString(pKey);` |
|     ! 0 |  979 | `				}` |
|      41 |  980 | `				if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|       - |  981 | `					/* Pathological empty name: keep the legacy diagnostic */` |
|     ! 0 |  982 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|       - |  983 | `						"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 |  984 | `					rc = SXRET_OK;` |
|     ! 0 |  985 | `				}else{` |
|      61 |  986 | `					rc = PH7_VmInstallGlobalVar(&(*pVm),` |
|      40 |  987 | `						(const char *)SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),` |
|      40 |  988 | `						0,pTos->nIdx);` |
|       - |  989 | `				}` |
|       - |  990 | `			}` |
|      21 |  991 | `		}else{` |
|       - |  992 | `			/* Insertion by reference */` |
|      62 |  993 | `			rc = PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|       - |  994 | `		}` |
|      52 |  995 | `	}else{` |
|  515675 |  996 | `		rc = PH7_HashmapInsert(pMap,pKey,pTos);` |
|       - |  997 | `	}` |
|  515775 |  998 | `	if( pKey ){` |
|   90437 |  999 | `		PH7_MemObjRelease(pKey);` |
|   45133 | 1000 | `	}` |
|       - | 1001 | `	/* An append onto the occupied saturated auto-index threw php's catchable` |
|       - | 1002 | `	 * Error (PH7_VmThrowArrayNextIndexError) — dispatch it like any other` |
|       - | 1003 | `	 * store-path throw. Plain failures (OOM) keep their existing routes. */` |
|  515775 | 1004 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|  515769 | 1005 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1006 | `	VM_EXIT_BREAK;` |
|  258326 | 1007 | `}` |
|       - | 1008 |  |
|       - | 1009 | `/*` |
|       - | 1010 | ` * OP_LOAD_CLOSURE: body moved verbatim from the OP_LOAD_CLOSURE arm of` |
|       - | 1011 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 1012 | ` */` |
|   18555 | 1013 | `PH7_PRIVATE VmOpRc VmExecOpLoadClosure(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 1014 | `{` |
|   18560 | 1015 | `	ph7_value *pTos = pState->pTos;` |
|   18560 | 1016 | `	ph7_value *pStack = pState->pStack;` |
|   18560 | 1017 | `	VmInstr *aInstr = pState->aInstr;` |
|   18560 | 1018 | `	sxi32 pc = pState->pc;` |
|       - | 1019 | `	sxi32 rc;` |
|    9156 | 1020 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|   18560 | 1021 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|       - | 1022 | `	/* The function whose name the Closure object will wrap: a fresh per-instantiation` |
|       - | 1023 | `	 * copy for a real closure (built below), or the shared lambda function itself for a` |
|       - | 1024 | `	 * plain anonymous function with no captured environment. */` |
|   18560 | 1025 | `	ph7_vm_func *pTarget = pFunc;` |
|       - | 1026 | `	/* A no-capture lambda declared inside a class method is not VM_FUNC_CLOSURE` |
|       - | 1027 | `	 * (its env is empty), yet php still binds the creation-site class as its scope` |
|       - | 1028 | ``	 * so `self::`/private access inside the body works. Detect the enclosing class`` |
|       - | 1029 | `	 * here and route such a lambda through the per-instance closure path (a global` |
|       - | 1030 | `	 * no-capture lambda peeks NULL and stays a shared function). */` |
|   18560 | 1031 | `	ph7_class *pLoadScope = (pFunc->iFlags & VM_FUNC_CLOSURE) ? 0 : PH7_VmPeekDeclaringClass(pVm);` |
|   18560 | 1032 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) \|\| pLoadScope ){` |
|       - | 1033 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|       - | 1034 | `		ph7_vm_func *pClosure;` |
|       - | 1035 | `		char *zName;` |
|       - | 1036 | `		sxu32 mLen;` |
|       - | 1037 | `		sxu32 n;` |
|       - | 1038 | `		/* Create a new VM function */` |
|   18340 | 1039 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|       - | 1040 | `		/* Generate an unique closure name */` |
|   18340 | 1041 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|   18340 | 1042 | `		if( pClosure == 0 \|\| zName == 0){` |
|     ! 0 | 1043 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|     ! 0 | 1044 | `			VM_EXIT_ABORT;` |
|       - | 1045 | `		}` |
|   18340 | 1046 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|   18340 | 1047 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|     ! 0 | 1048 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|     ! 0 | 1049 | `		}` |
|       - | 1050 | `		/* Zero the stucture */` |
|   18340 | 1051 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|       - | 1052 | `		/* Perform a structure assignment on read-only items */` |
|   18340 | 1053 | `		pClosure->aArgs = pFunc->aArgs;` |
|   18340 | 1054 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|   18340 | 1055 | `		pClosure->aStatic = pFunc->aStatic;` |
|   18340 | 1056 | `		pClosure->iFlags = pFunc->iFlags;` |
|       - | 1057 | `		/* An in-class no-capture lambda routed here (pLoadScope set) must be a` |
|       - | 1058 | `		 * real closure so PH7_VmClassMemberAccess honors its pUserData scope. */` |
|   18340 | 1059 | `		pClosure->iFlags \|= VM_FUNC_CLOSURE;` |
|   18340 | 1060 | `		pClosure->pUserData = pFunc->pUserData;` |
|   18340 | 1061 | `		pClosure->sSignature = pFunc->sSignature;` |
|   18340 | 1062 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|   18340 | 1063 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|   18340 | 1064 | `		pClosure->aReturnUnion = pFunc->aReturnUnion;` |
|   18340 | 1065 | `		pClosure->sReturnTypeName = pFunc->sReturnTypeName;` |
|   18340 | 1066 | `		pClosure->bStrictTypes = pFunc->bStrictTypes;` |
|   18340 | 1067 | `		pClosure->nMaxStack = pFunc->nMaxStack;` |
|   18340 | 1068 | `		if( pClosure->pUserData == 0 ){` |
|       - | 1069 | `			/* Stamp the creation-site class scope (see PH7_VmPeekDeclaringClass):` |
|       - | 1070 | `			 * a closure made in a method — or in another closure, whose own stamp` |
|       - | 1071 | `			 * the peek reads — resolves self::/parent:: against it like php. */` |
|   18340 | 1072 | `			pClosure->pUserData = (void *)PH7_VmPeekDeclaringClass(pVm);` |
|    9046 | 1073 | `		}` |
|       - | 1074 | ``		/* Capture the creation-site late-static-binding class so `static::` inside`` |
|       - | 1075 | `		 * the closure body resolves like php (the "called class", which may differ` |
|       - | 1076 | `		 * from the declaring scope stamped above — e.g. a closure made in an` |
|       - | 1077 | `		 * inherited method). A closure made outside any class captures NULL. */` |
|   18340 | 1078 | `		pClosure->pLsbClass = (void *)PH7_VmPeekTopClass(pVm);` |
|       - | 1079 | `		/* Reflection descriptor fields (getDocComment/getAttributes/getStartLine/` |
|       - | 1080 | `		 * getEndLine/getFileName read the INSTANTIATED copy via $__fn) */` |
|   18340 | 1081 | `		pClosure->aAttrs = pFunc->aAttrs;` |
|   18340 | 1082 | `		pClosure->sDoc = pFunc->sDoc;` |
|   18340 | 1083 | `		pClosure->sFile = pFunc->sFile;` |
|   18340 | 1084 | `		pClosure->nLine = pFunc->nLine;` |
|   18340 | 1085 | `		pClosure->nEndLine = pFunc->nEndLine;` |
|       - | 1086 | ``		/* php's visible `{closure:...}` name is a property of the DECLARATION, so every`` |
|       - | 1087 | `		 * per-instantiation copy answers the same one (php has a single op_array here). */` |
|   18340 | 1088 | `		pClosure->sClosureName = pFunc->sClosureName;` |
|   18340 | 1089 | `		pClosure->sClosureScope = pFunc->sClosureScope;` |
|   18340 | 1090 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|       - | 1091 | `		/* Register the closure */` |
|   18340 | 1092 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|       - | 1093 | `		/* Set up closure environment */` |
|   18340 | 1094 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|   18340 | 1095 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|   45932 | 1096 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|       - | 1097 | `			ph7_value *pValue;` |
|   27597 | 1098 | `			pEnv = &aEnv[n];` |
|   27597 | 1099 | `			sEnv.sName  = pEnv->sName;` |
|   27597 | 1100 | `			sEnv.iFlags = pEnv->iFlags;` |
|   27597 | 1101 | `			sEnv.nLine = pEnv->nLine;` |
|   27597 | 1102 | `			sEnv.nIdx = SXU32_HIGH;` |
|   27597 | 1103 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|   27592 | 1104 | `			if( (sEnv.iFlags & (VM_FUNC_ARG_BY_REF\|VM_FUNC_ARG_IGNORE)) == VM_FUNC_ARG_BY_REF` |
|   14953 | 1105 | `			 && !(SyStringLength(&sEnv.sName) == sizeof("this")-1` |
|    1291 | 1106 | `				&& SyMemcmp(SyStringData(&sEnv.sName),"this",sizeof("this")-1) == 0) ){` |
|       - | 1107 | `				/* Capture by reference: bind the env entry to the variable's` |
|       - | 1108 | `				 * memory slot — creating a fresh null variable when missing,` |
|       - | 1109 | ``				 * as php does (`use (&$f)` before $f is assigned) — and pin`` |
|       - | 1110 | `				 * the slot past the creating frame's teardown so the closure` |
|       - | 1111 | `				 * can outlive its birth scope. The call-time env install` |
|       - | 1112 | `				 * aliases the name to this slot instead of copying a value. */` |
|    1691 | 1113 | `				pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,TRUE);` |
|    1691 | 1114 | `				if( pValue ){` |
|    1691 | 1115 | `					sEnv.nIdx = pValue->nIdx;` |
|    1691 | 1116 | `					VmPinMemObjSlot(pVm,pValue->nIdx);` |
|     839 | 1117 | `				}` |
|     844 | 1118 | `			}else{` |
|       - | 1119 | `				/* Standard pass by value */` |
|   25911 | 1120 | `				pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|   25911 | 1121 | `				if( pValue ){` |
|       - | 1122 | `					/* Copy imported value */` |
|    7982 | 1123 | `					PH7_MemObjStore(pValue,&sEnv.sValue);` |
|   21904 | 1124 | `				}else if( (sEnv.iFlags & (VM_FUNC_ARG_BY_REF\|VM_FUNC_ARG_IGNORE)) == 0` |
|    8896 | 1125 | `					&& !(SyStringLength(&sEnv.sName) == sizeof("this")-1` |
|      45 | 1126 | `						&& SyMemcmp(SyStringData(&sEnv.sName),"this",sizeof("this")-1) == 0) ){` |
|      97 | 1127 | `					if( pFunc->iFlags & VM_FUNC_ARROW ){` |
|       - | 1128 | `						/* An arrow function auto-captures free variables by value, but` |
|       - | 1129 | `						 * php does NOT capture one that is UNDEFINED at creation: the` |
|       - | 1130 | `						 * isolated body scope then simply has no such variable, so a` |
|       - | 1131 | `						 * read of it there raises the normal "Undefined variable"` |
|       - | 1132 | `						 * warning (and reflection's getClosureUsedVariables omits it).` |
|       - | 1133 | `						 * Skip installing the capture so the body READ — not the` |
|       - | 1134 | `						 * creation — warns, matching php. (A later assignment to the` |
|       - | 1135 | `						 * outer variable does not retro-capture: arrow scope is` |
|       - | 1136 | `						 * isolated.) An explicit by-value use() instead warns here and` |
|       - | 1137 | `						 * binds NULL, handled just below. */` |
|      87 | 1138 | `						continue;` |
|       - | 1139 | `					}` |
|       - | 1140 | ``					/* php reads a by-value `use ($q)` capture AT CLOSURE CREATION and`` |
|       - | 1141 | `					 * warns when the variable is undefined there (the by-ref form` |
|       - | 1142 | ``					 * `use (&$q)` above stays silent — it creates the binding). The`` |
|       - | 1143 | `					 * auto-injected $this (VM_FUNC_ARG_IGNORE) never warns. The` |
|       - | 1144 | `					 * capture still proceeds as NULL, as php does. php attributes the` |
|       - | 1145 | `					 * warning to the capture's own line (which can differ from the` |
|       - | 1146 | `					 * OP_LOAD_CLOSURE instruction line when the use-clause wraps), so` |
|       - | 1147 | `					 * borrow the recorded line for the emission and restore it. */` |
|      11 | 1148 | `					sxu32 nSavedLine = pVm->nCurLine;` |
|      11 | 1149 | `					if( sEnv.nLine ){` |
|      11 | 1150 | `						pVm->nCurLine = sEnv.nLine;` |
|       5 | 1151 | `					}` |
|      11 | 1152 | `					VmErrorFormat(pVm,PH7_CTX_WARNING,"Undefined variable $%z",&sEnv.sName);` |
|      11 | 1153 | `					pVm->nCurLine = nSavedLine;` |
|       5 | 1154 | `				}` |
|       - | 1155 | `			}` |
|       - | 1156 | `			/* Insert the imported variable */` |
|   27515 | 1157 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|   13616 | 1158 | `		}` |
|   18340 | 1159 | `		pTarget = pClosure;` |
|    9046 | 1160 | `	}` |
|       - | 1161 | `	/* Wrap the target function in a Closure object and push it. Its captured environment` |
|       - | 1162 | ``	 * (incl. any `$this`) stays in pTarget->aClosureEnv and is delivered by the normal call`` |
|       - | 1163 | `	 * path when the closure is dispatched by name. */` |
|   18560 | 1164 | `	pTos++;` |
|       - | 1165 | `	{` |
|   18560 | 1166 | `		ph7_class_instance *pCloObj = VmCreateClosure(pVm, &pTarget->sName, 0, 0);` |
|   18560 | 1167 | `		if( pCloObj ){` |
|       - | 1168 | `			/* The instance is born holding ONE reference and this stack slot is what` |
|       - | 1169 | `			 * holds it -- the same handover OP_NEW makes. Taking a second one here` |
|       - | 1170 | `			 * meant no Closure object ever reached zero: every closure expression a` |
|       - | 1171 | `			 * program evaluated leaked its object, its three slots and (through the` |
|       - | 1172 | `			 * object) the per-instantiation function body behind it. */` |
|   18560 | 1173 | `			pTos->x.pOther = pCloObj;` |
|   18560 | 1174 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|    9161 | 1175 | `		}else{` |
|       - | 1176 | `			/* OOM fallback: the name string is still a usable callable. */` |
|     ! 0 | 1177 | `			PH7_MemObjStringAppend(pTos, pTarget->sName.zString, pTarget->sName.nByte);` |
|       - | 1178 | `		}` |
|       - | 1179 | `	}` |
|   18560 | 1180 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1181 | `	VM_EXIT_BREAK;` |
|    9161 | 1182 | `}` |
|       - | 1183 |  |
|       - | 1184 |  |
|       - | 1185 | `/*` |
|       - | 1186 | `` * TRUE when this LOAD_IDX feeds a `??`: the coalesce test is the very next`` |
|       - | 1187 | `` * instruction. php evaluates the whole left operand of `??` in isset-context,`` |
|       - | 1188 | ` * so the access must stay SILENT and, when it misses, must yield NULL — an` |
|       - | 1189 | ``  * out-of-range string offset that yielded "" instead made `$s[99] ?? $d` `` |
|       - | 1190 | ` * evaluate to "" rather than $d, a wrong answer rather than a stray notice.` |
|       - | 1191 | ` */` |
|  951275 | 1192 | `static int VmIdxFeedsCoalesce(const VmInstr *pInstr)` |
|       5 | 1193 | `{` |
|  951280 | 1194 | `	return (pInstr+1)->iOp == PH7_OP_NULLC \|\| (pInstr+1)->iOp == PH7_OP_NULLC_JMP;` |
|       5 | 1195 | `}` |
|       - | 1196 | `/*` |
|       - | 1197 | `` * php's string-offset STORE, shared by `$s[i] = v` (OP_STORE_IDX) and`` |
|       - | 1198 | `` * `$s[i] ??= v` (OP_NULLC_STORE — `??=` is not an assign-op, so php performs a`` |
|       - | 1199 | ` * real offset write there): resolve iRawOfft against the string's CURRENT length` |
|       - | 1200 | ` * (a negative offset counts back from the end and, when it still lands before the` |
|       - | 1201 | `` * start, warns `Illegal string offset` and writes NOTHING), refuse an EMPTY`` |
|       - | 1202 | ` * replacement, warn when more than one byte was handed over, PAD WITH SPACES up` |
|       - | 1203 | ` * to the offset, then write the first byte.` |
|       - | 1204 | ` *` |
|       - | 1205 | ` * pVal must ALREADY be a string: that coercion is user-visible (it warns for an` |
|       - | 1206 | ` * array, throws for a not-stringable object) and stays with the callers, which` |
|       - | 1207 | ` * are the only places that can route a throw. Answers SXRET_OK when the store` |
|       - | 1208 | `` * happened or was skipped, and SXERR_INVALID for php's `Cannot assign an empty`` |
|       - | 1209 | `` * string to a string offset` Error — raised by the caller for the same reason.`` |
|       - | 1210 | ` */` |
|     450 | 1211 | `PH7_PRIVATE sxi32 VmStringOffsetWrite(ph7_vm *pVm,ph7_value *pStr,sxi64 iRawOfft,ph7_value *pVal)` |
|       5 | 1212 | `{` |
|     455 | 1213 | `	sxi64 nLen = (sxi64)SyBlobLength(&pStr->sBlob);` |
|     455 | 1214 | `	sxi64 iOfft = iRawOfft;` |
|       - | 1215 | `	const char *zVal;` |
|     455 | 1216 | `	if( iOfft < 0 ){` |
|       - | 1217 | `		/* php 7.1: a negative offset writes back from the end. */` |
|       9 | 1218 | `		iOfft += nLen;` |
|       9 | 1219 | `		if( iOfft < 0 ){` |
|       7 | 1220 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Illegal string offset %qd",iRawOfft);` |
|       7 | 1221 | `			return SXRET_OK;` |
|       - | 1222 | `		}` |
|       1 | 1223 | `	}` |
|     449 | 1224 | `	if( SyBlobLength(&pVal->sBlob) < 1 ){` |
|       - | 1225 | ``		/* php refuses to write NOTHING into an offset — `$s[2] = ""`, and the`` |
|       - | 1226 | ``		 * `= null` / `= false` that stringify to "" — where PHL silently ignored`` |
|       - | 1227 | `		 * the store. */` |
|      13 | 1228 | `		return SXERR_INVALID;` |
|       - | 1229 | `	}` |
|     437 | 1230 | `	zVal = (const char *)SyBlobData(&pVal->sBlob);` |
|     437 | 1231 | `	if( SyBlobLength(&pVal->sBlob) > 1 ){` |
|      10 | 1232 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 1233 | `			"Only the first byte will be assigned to the string offset");` |
|       4 | 1234 | `	}` |
|     437 | 1235 | `	if( iOfft >= nLen ){` |
|       - | 1236 | `		/* php PADS WITH SPACES up to the offset. PH7 simply appended the byte, so` |
|       - | 1237 | `		 * "abc" with [6]="Z" became "abcZ" rather than "abc   Z" -- a silently` |
|       - | 1238 | `		 * wrong string. */` |
|       - | 1239 | `		sxi64 nPad;` |
|     217 | 1240 | `		for( nPad = nLen ; nPad < iOfft ; ++nPad ){` |
|     175 | 1241 | `			SyBlobAppend(&pStr->sBlob," ",sizeof(char));` |
|      88 | 1242 | `		}` |
|      43 | 1243 | `		SyBlobAppend(&pStr->sBlob,(const void *)zVal,sizeof(char));` |
|      22 | 1244 | `	}else{` |
|     395 | 1245 | `		char *zData = (char *)SyBlobData(&pStr->sBlob);` |
|     395 | 1246 | `		zData[iOfft] = zVal[0];` |
|       - | 1247 | `	}` |
|     437 | 1248 | `	return SXRET_OK;` |
|     230 | 1249 | `}` |
|       - | 1250 | `/*` |
|       - | 1251 | `` * Does this LOAD_IDX feed a `??=` (rather than a plain `??`)? The compiler emits`` |
|       - | 1252 | ` * the LHS peek, then OP_NULLC_JMP, then the RHS, then OP_NULLC_STORE — so the` |
|       - | 1253 | ` * NULLC_JMP right after is what distinguishes the assigning form, whose store` |
|       - | 1254 | ` * still has to happen when the peek answers null.` |
|       - | 1255 | ` */` |
|  951239 | 1256 | `static int VmIdxFeedsCoalesceAssign(const VmInstr *pInstr)` |
|       5 | 1257 | `{` |
|  951244 | 1258 | `	return (pInstr+1)->iOp == PH7_OP_NULLC_JMP;` |
|       5 | 1259 | `}` |
|       - | 1260 | `/*` |
|       - | 1261 | `` * Arm the write-back half of `$o[$k] op= v` on an ArrayAccess container. The`` |
|       - | 1262 | ` * value offsetGet just answered goes into a fresh SCRATCH memobj that pTos` |
|       - | 1263 | ` * points at, so the compound-assign op computes IN that slot the way it would` |
|       - | 1264 | ` * in an ordinary variable; the pending entry then makes the op's tail dispatch` |
|       - | 1265 | `` * `offsetSet($k, computed)`. The KEY gets a reserved slot of its own (pIdx is`` |
|       - | 1266 | ` * released as soon as this opcode returns, and the write happens one opcode` |
|       - | 1267 | `` * later); an ABSENT key — `$o[] op= v` — is carried as NULL, which is the value`` |
|       - | 1268 | ` * php hands both accessors for that shape.` |
|       - | 1269 | ` *` |
|       - | 1270 | ` * A failed reservation simply leaves the value unarmed: pTos keeps its` |
|       - | 1271 | ` * no-slot temp and the op falls back to the pre-existing refusal.` |
|       - | 1272 | ` */` |
|      54 | 1273 | `static void VmDimRmwArm(` |
|       - | 1274 | `	ph7_vm *pVm,` |
|       - | 1275 | `	ph7_class_instance *pInst,` |
|       - | 1276 | `	ph7_value *pIdx,` |
|       - | 1277 | `	ph7_value *pTos,` |
|       - | 1278 | `	void *pOwnerStack,` |
|       - | 1279 | `	void *pInstrs,` |
|       - | 1280 | `	sxu32 nPc` |
|       - | 1281 | `	)` |
|       2 | 1282 | `{` |
|       - | 1283 | `	ph7_value *pSlot;` |
|       - | 1284 | `	sxu32 nScratch;` |
|       - | 1285 | `	sxu32 nKey;` |
|       - | 1286 | `	VmHookRmw sRmw;` |
|      56 | 1287 | `	pSlot = PH7_ReserveMemObj(&(*pVm));` |
|      56 | 1288 | `	if( pSlot == 0 ){` |
|     ! 0 | 1289 | `		return;` |
|       - | 1290 | `	}` |
|      56 | 1291 | `	nScratch = pSlot->nIdx;` |
|      56 | 1292 | `	pSlot = PH7_ReserveMemObj(&(*pVm));` |
|      56 | 1293 | `	if( pSlot == 0 ){` |
|     ! 0 | 1294 | `		VmHookRmwFreeScratch(&(*pVm),nScratch);` |
|     ! 0 | 1295 | `		return;` |
|       - | 1296 | `	}` |
|      56 | 1297 | `	nKey = pSlot->nIdx;` |
|       - | 1298 | `	/* Address both slots by index from here on. This guarded a reservation` |
|       - | 1299 | `	 * GROWING the aMemObj set under the pointer the first one handed back;` |
|       - | 1300 | `	 * redundant since P1 -- the pool's segments are fixed, so a slot's address` |
|       - | 1301 | `	 * never moves. Left for the harvest sweep (PERF.md P1). */` |
|      56 | 1302 | `	if( pIdx ){` |
|      56 | 1303 | `		PH7_MemObjStore(pIdx,pSlot);` |
|      27 | 1304 | `	}` |
|      56 | 1305 | `	pSlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nScratch);` |
|      56 | 1306 | `	if( pSlot == 0 ){` |
|     ! 0 | 1307 | `		VmHookRmwFreeScratch(&(*pVm),nKey);` |
|     ! 0 | 1308 | `		VmHookRmwFreeScratch(&(*pVm),nScratch);` |
|     ! 0 | 1309 | `		return;` |
|       - | 1310 | `	}` |
|      56 | 1311 | `	PH7_MemObjStore(pTos,pSlot);` |
|      56 | 1312 | `	sRmw.iKind = VM_HOOK_PEND_RMW_DIM;` |
|      56 | 1313 | `	sRmw.pThis = pInst;` |
|      56 | 1314 | `	sRmw.pAttr = 0;` |
|      56 | 1315 | `	sRmw.nBackIdx = nKey;` |
|      56 | 1316 | `	sRmw.nScratchIdx = nScratch;` |
|      56 | 1317 | `	SyBlobInit(&sRmw.sName,&pVm->sAllocator);` |
|      56 | 1318 | `	sRmw.pOwnerStack = pOwnerStack;` |
|      56 | 1319 | `	sRmw.pInstrs = pInstrs;` |
|      56 | 1320 | `	sRmw.nJmpPc = nPc;  /* the modify op ... */` |
|      56 | 1321 | `	sRmw.nPc = nPc;     /* ... is the whole window */` |
|      56 | 1322 | `	pInst->iRef++;` |
|      56 | 1323 | `	pTos->nIdx = nScratch;` |
|      56 | 1324 | `	SySetPut(&pVm->aHookRmw,(const void *)&sRmw);` |
|      29 | 1325 | `}` |
|       - | 1326 | `/*` |
|       - | 1327 | ` * Is this LOAD_IDX fetching the dimension for WRITING — php's BP_VAR_W / BP_VAR_RW` |
|       - | 1328 | ` * / BP_VAR_UNSET fetch, the one that asks the container for a slot to MODIFY` |
|       - | 1329 | ` * rather than a value to read?` |
|       - | 1330 | ` *` |
|       - | 1331 | ` * iP2 answers for most of it. The two shapes it cannot are the ones where the` |
|       - | 1332 | ` * fetch is compiled as a plain read and the NEXT instruction is what makes it a` |
|       - | 1333 | `` * write: binding a reference to the element (`$r = &$o[$k]`) and iterating it by`` |
|       - | 1334 | `` * reference (`foreach ($o[$k] as &$v)`). A compound assign is the reverse case —`` |
|       - | 1335 | ` * iP2 says write, but php compiles it to ASSIGN_DIM_OP, a read plus a write` |
|       - | 1336 | ` * through the container's own handlers (VmDimRmwArm), not a write FETCH.` |
|       - | 1337 | ` */` |
|    1056 | 1338 | `static int VmIdxFetchForWrite(const VmInstr *pInstr,sxi32 iP2)` |
|       5 | 1339 | `{` |
|    1061 | 1340 | `	const VmInstr *pNext = pInstr + 1;` |
|    1061 | 1341 | `	if( iP2 == 1 ){` |
|     189 | 1342 | `		return !VmNextIsCompoundAssign(pNext);` |
|       - | 1343 | `	}` |
|     873 | 1344 | `	if( iP2 == VM_IDX_CTX_UNSET_BASE ){` |
|       - | 1345 | `		/* An INTERMEDIATE subscript of an unset chain: php fetches it for` |
|       - | 1346 | `		 * writing so the removal one level down can land. */` |
|       9 | 1347 | `		return 1;` |
|       - | 1348 | `	}` |
|     865 | 1349 | `	if( iP2 == 0 ){` |
|     585 | 1350 | `		if( pNext->iOp == PH7_OP_STORE_REF ){` |
|     ! 0 | 1351 | `			return 1;` |
|       - | 1352 | `		}` |
|     585 | 1353 | `		if( pNext->iOp == PH7_OP_FOREACH_INIT && pNext->p3 ){` |
|       5 | 1354 | `			return (((ph7_foreach_info *)pNext->p3)->iFlags & PH7_4EACH_STEP_REF) != 0;` |
|       - | 1355 | `		}` |
|     288 | 1356 | `	}` |
|     861 | 1357 | `	return 0;` |
|     533 | 1358 | `}` |
|       - | 1359 | `/*` |
|       - | 1360 | ` * Which fetch contexts may answer out of a WRITABLE container's own storage` |
|       - | 1361 | `` * (PH7_SplDimElemSlot, vm_builtin_spl.c) instead of through `offsetGet`?`` |
|       - | 1362 | ` *` |
|       - | 1363 | ` * The ones that ask for a VALUE and may go on to MODIFY it: a plain read — whose` |
|       - | 1364 | ` * result carries the element's slot exactly as an array element's does, which is` |
|       - | 1365 | ` * what lets a by-reference ARGUMENT bind it — a write-context fetch, and the` |
|       - | 1366 | `` * INTERMEDIATE step of an unset chain. isset()/empty()/`??`/`??=` must reach`` |
|       - | 1367 | ` * offsetExists, and the OUTERMOST unset must reach offsetUnset, so those keep the` |
|       - | 1368 | ` * accessor. (iP2 is the NORMALIZED context here: the deferred-argument record mode` |
|       - | 1369 | ` * has already become a plain read.)` |
|       - | 1370 | ` *` |
|       - | 1371 | ` * A COMPOUND assign is deliberately not one of them, which is why this asks` |
|       - | 1372 | `` * VmIdxFetchForWrite rather than testing iP2 == 1 itself: `$ao[k] op= v` is php's`` |
|       - | 1373 | ` * ASSIGN_DIM_OP on an OBJECT, and that one reads and writes through the accessors` |
|       - | 1374 | ` * whatever the read handler could have offered — a subclass overriding only` |
|       - | 1375 | `` * offsetSet sees its own method called for `+=` and not for `++`.`` |
|       - | 1376 | ` */` |
|     674 | 1377 | `static int VmDimFastFetchCtx(const VmInstr *pInstr,sxi32 iP2)` |
|       5 | 1378 | `{` |
|     679 | 1379 | `	return iP2 == 0 \|\| VmIdxFetchForWrite(pInstr,iP2);` |
|       5 | 1380 | `}` |
|       - | 1381 | `/*` |
|       - | 1382 | ` * Is this fetch asking the base to BE a container — the question php answers with` |
|       - | 1383 | `` * `Cannot use string offset as an array` when the base is a string offset?`` |
|       - | 1384 | ` *` |
|       - | 1385 | ` * Every context that reaches INTO the base to write, vivify or remove: the write` |
|       - | 1386 | ` * contexts (1, the read-modify-write among them) and both halves of an unset chain.` |
|       - | 1387 | `` * The LOOKUPS — a plain read, isset()/empty()/`??`, a destructure, a deferred`` |
|       - | 1388 | `` * argument — are php's own silence (`$x = $s[0][1]` reads a character out of a`` |
|       - | 1389 | `` * character) and are not this. Neither is the `??=` PEEK (3): it reads, and`` |
|       - | 1390 | `` * `$s[0][0] ??= 7` finds a character and never stores at all, so only the peek that`` |
|       - | 1391 | ` * comes back EMPTY is a write — that one is refused where the read lands.` |
|       - | 1392 | ` *` |
|       - | 1393 | ` * iP2 is the NORMALIZED context, so VM_IDX_CTX_RMW has already become 1.` |
|       - | 1394 | ` */` |
|      24 | 1395 | `static int VmIdxCtxIsContainerWrite(sxi32 iP2)` |
|       1 | 1396 | `{` |
|      25 | 1397 | `	return iP2 == 1 \|\| VM_IDX_IS_UNSET(iP2);` |
|       1 | 1398 | `}` |
|       - | 1399 | `/*` |
|       - | 1400 | `` * php's `Indirect modification of overloaded element of C has no effect`: the`` |
|       - | 1401 | ` * write-context fetch above landed on a container that answers with a COPY, so` |
|       - | 1402 | ` * whatever the rest of the expression writes is thrown away. php says so and` |
|       - | 1403 | ` * carries on.` |
|       - | 1404 | ` *` |
|       - | 1405 | ` * PHL had neither half. The notice was missing, and the copy was not a copy: a` |
|       - | 1406 | ` * userland offsetGet returns the container's own nested hashmap by COW, and` |
|       - | 1407 | ` * OP_STORE_IDX on a base with no slot index writes STRAIGHT INTO the shared map —` |
|       - | 1408 | `` * so `$o['a']['b'] = 9`, `$o['a'][] = 5` and `foreach ($o['a'] as &$v)` all`` |
|       - | 1409 | ` * modified the object php leaves untouched, silently. Separating the value here` |
|       - | 1410 | ` * is what makes the write land nowhere.` |
|       - | 1411 | ` *` |
|       - | 1412 | ` * php stays silent for an OBJECT, and so does this: an object is a handle, the` |
|       - | 1413 | ` * write through it is not lost, and nothing about it is indirect.` |
|       - | 1414 | ` */` |
|      50 | 1415 | `PH7_PRIVATE void PH7_VmOverloadedElemNotice(ph7_vm *pVm,ph7_class *pClass,ph7_value *pVal)` |
|       1 | 1416 | `{` |
|      51 | 1417 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       9 | 1418 | `		return;` |
|       - | 1419 | `	}` |
|      64 | 1420 | `	VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|       - | 1421 | `		"Indirect modification of overloaded element of %z has no effect",` |
|      21 | 1422 | `		&pClass->sName);` |
|      43 | 1423 | `	if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|      35 | 1424 | `		PH7_HashmapCowSeparate(&(*pVm),pVal);` |
|      17 | 1425 | `	}` |
|      26 | 1426 | `}` |
|       - | 1427 | `/*` |
|       - | 1428 | ` * OP_LOAD_IDX: body moved verbatim from the OP_LOAD_IDX arm of` |
|       - | 1429 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 1430 | ` */` |
| 1259538 | 1431 | `PH7_PRIVATE VmOpRc VmExecOpLoadIdx(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 1432 | `{` |
| 1259543 | 1433 | `	ph7_value *pTos = pState->pTos;` |
| 1259543 | 1434 | `	ph7_value *pStack = pState->pStack;` |
| 1259543 | 1435 | `	VmInstr *aInstr = pState->aInstr;` |
| 1259543 | 1436 | `	sxi32 pc = pState->pc;` |
|       - | 1437 | `	sxi32 rc;` |
|  631736 | 1438 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 1259543 | 1439 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
| 1259543 | 1440 | `	ph7_hashmap *pMap = 0;` |
|       - | 1441 | `	ph7_value *pIdx;` |
|       - | 1442 | `	/* D1 commit 2: iP2==9 is the deferred-record mode. It behaves exactly like a plain read` |
|       - | 1443 | `	 * (iP2==0) for base dispatch / the read tail, EXCEPT the dedicated block right after the` |
|       - | 1444 | `	 * index is popped, which — on a lookup MISS with a reachable base — captures the lvalue` |
|       - | 1445 | `	 * path (MEMOBJ_AUX_DEFPATH) instead of warning, and exits. Everything else sees iP2. */` |
|       - | 1446 | ``	/* A read-modify-write fetch (VM_IDX_CTX_RMW: `$a[k] += v`, `$a[k]++`) IS the`` |
|       - | 1447 | `	 * write context for everything below — it COW-separates, it vivifies a missing` |
|       - | 1448 | `	 * key, it refuses the same offset TYPES — so normalize it to 1 here and keep` |
|       - | 1449 | `	 * the single thing that separates the two: php READ the element first, so a key` |
|       - | 1450 | `	 * that was not there to read WARNS before it is created. */` |
| 1259543 | 1451 | `	int bRmwFetch = (pInstr->iP2 == VM_IDX_CTX_RMW);` |
| 1259543 | 1452 | `	int bRmwMiss = 0;` |
| 1259543 | 1453 | `	int bBaseStrOff = 0;` |
| 1259543 | 1454 | `	sxi32 iP2 = (pInstr->iP2 == 9) ? 0 : (bRmwFetch ? 1 : pInstr->iP2);` |
| 1259543 | 1455 | `	pIdx = 0;` |
| 1259543 | 1456 | `	if( pInstr->iP1 == 0 ){` |
|      52 | 1457 | `		if( !iP2){` |
|       - | 1458 | ``			/* `[]` with nothing to append INTO. Every placement php refuses is a compile`` |
|       - | 1459 | `			 * error now (compile.c), so the only shape that reaches here is the one php` |
|       - | 1460 | `			 * also settles at runtime: a call ARGUMENT, whose parameter may turn out to be` |
|       - | 1461 | `			 * by-reference (php appends and binds) or by-value (php's Error). Record the` |
|       - | 1462 | `			 * append as a step of the deferred lvalue path and let OP_CALL decide. */` |
|      13 | 1463 | `			if( pInstr->iP2 == 9 ){` |
|      13 | 1464 | `				VmDeferredPath *pPath = 0;` |
|      13 | 1465 | `				if( pTos->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|     ! 0 | 1466 | `					pPath = (VmDeferredPath *)pTos->x.pOther;` |
|     ! 0 | 1467 | `					if( VmDeferPathPushAppend(pPath) == SXRET_OK ){` |
|     ! 0 | 1468 | `						VM_EXIT_BREAK; /* carrier already on pTos */` |
|     ! 0 | 1469 | `					}` |
|      13 | 1470 | `				}else if( pTos->iFlags & MEMOBJ_AUX_DEFERRED ){` |
|       - | 1471 | `					SyString sRootName;` |
|       3 | 1472 | `					SyStringInitFromBuf(&sRootName,(const char *)pTos->x.pOther,` |
|       - | 1473 | `						pTos->x.pOther ? SyStrlen((const char *)pTos->x.pOther) : 0);` |
|       3 | 1474 | `					pPath = VmDeferPathNew(&(*pVm),1,SXU32_HIGH,&sRootName);` |
|       3 | 1475 | `					if( pPath && VmDeferPathPushAppend(pPath) == SXRET_OK ){` |
|       3 | 1476 | `						pTos->iFlags &= ~MEMOBJ_AUX_DEFERRED; /* borrowed name; don't free x.pOther */` |
|       3 | 1477 | `						pTos->x.pOther = pPath;` |
|       3 | 1478 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|       3 | 1479 | `						pTos->nIdx = SXU32_HIGH;` |
|       3 | 1480 | `						VM_EXIT_BREAK;` |
|       - | 1481 | `					}` |
|     ! 0 | 1482 | `					VmFreeDeferredPath(pPath);` |
|      11 | 1483 | `				}else if( pTos->nIdx != SXU32_HIGH ){` |
|      11 | 1484 | `					pPath = VmDeferPathNew(&(*pVm),0,pTos->nIdx,0);` |
|      11 | 1485 | `					if( pPath && VmDeferPathPushAppend(pPath) == SXRET_OK ){` |
|      11 | 1486 | `						PH7_MemObjRelease(pTos);` |
|      11 | 1487 | `						pTos->x.pOther = pPath;` |
|      11 | 1488 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|      11 | 1489 | `						pTos->nIdx = SXU32_HIGH;` |
|      11 | 1490 | `						VM_EXIT_BREAK;` |
|       - | 1491 | `					}` |
|     ! 0 | 1492 | `					VmFreeDeferredPath(pPath);` |
|     ! 0 | 1493 | `				}` |
|     ! 0 | 1494 | `			}` |
|       - | 1495 | `			/* Not a deferrable argument (or out of memory recording it): php's own` |
|       - | 1496 | `			 * Error, which replaced PH7's notice-and-NULL. */` |
|     ! 0 | 1497 | `			if( pTos >= pStack ){` |
|     ! 0 | 1498 | `				PH7_MemObjRelease(pTos);` |
|     ! 0 | 1499 | `			}else{` |
|       - | 1500 | `				/* TICKET 1433-020: Empty stack */` |
|     ! 0 | 1501 | `				pTos++;` |
|     ! 0 | 1502 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|     ! 0 | 1503 | `				pTos->nIdx = SXU32_HIGH;` |
|       - | 1504 | `			}` |
|       - | 1505 | `			{` |
|     ! 0 | 1506 | `			sxi32 rcRd = VmThrowFromVm(&(*pVm),"Error","Cannot use [] for reading",` |
|       - | 1507 | `				sizeof("Cannot use [] for reading")-1);` |
|     ! 0 | 1508 | `			if( rcRd == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     ! 0 | 1509 | `			rc = rcRd;` |
|     ! 0 | 1510 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1511 | `			}` |
|       - | 1512 | `		}` |
|      21 | 1513 | `	}else{` |
| 1259493 | 1514 | `		pIdx = pTos;` |
| 1259493 | 1515 | `		pTos--;` |
|       - | 1516 | `	}` |
| 1259531 | 1517 | `	if( VM_IDX_IS_UNSET(iP2) ){` |
|       - | 1518 | ``		/* `unset($o->p[$k])` reaches INTO what the property holds, which is php's`` |
|       - | 1519 | `		 * indirect modification of the property itself -- refused for a readonly` |
|       - | 1520 | `		 * one, and for a native property whose handler takes no write. The write` |
|       - | 1521 | ``		 * shapes (`$o->p[$k] = v`, `$o->p[] = v`) are screened at OP_MEMBER, which`` |
|       - | 1522 | `		 * knows them from its own context tag; an unset BASE is tagged as an` |
|       - | 1523 | `		 * ordinary read there and is only recognizable here. */` |
|    1719 | 1524 | `		sxi32 rcInd = PH7_VmCheckIndirectModify(&(*pVm),pTos->nIdx);` |
|    1719 | 1525 | `		if( rcInd != SXRET_OK ){` |
|       5 | 1526 | `			if( pIdx ){` |
|       5 | 1527 | `				PH7_MemObjRelease(pIdx);` |
|       2 | 1528 | `			}` |
|       5 | 1529 | `			PH7_MemObjRelease(pTos);` |
|       5 | 1530 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       5 | 1531 | `			pTos->nIdx = SXU32_HIGH;` |
|       5 | 1532 | `			if( rcInd == PH7_ABORT ){ VM_EXIT_ABORT; }` |
|       5 | 1533 | `			PH7_THROW_ROUTE_MIDEXPR(rcInd)` |
|       - | 1534 | `		}` |
|     855 | 1535 | `	}` |
| 1259527 | 1536 | `	if( pInstr->iP2 == 9 && pIdx ){` |
|       - | 1537 | `		/* D1 commit 2 record mode. Decide whether to CAPTURE this subscript as a deferred` |
|       - | 1538 | `		 * lvalue step (the by-ref/by-value decision is not known until OP_CALL), or fall` |
|       - | 1539 | `		 * through to a plain read (iP2 was normalized to 0 above). We defer only when the` |
|       - | 1540 | `		 * base can be reached again at resolve time: an existing descriptor (nested), the` |
|       - | 1541 | `		 * commit-1 undefined-variable marker, or a real container/string/scalar slot` |
|       - | 1542 | `		 * (nIdx != SXU32_HIGH). On a present array key we DO NOT defer — a hit already` |
|       - | 1543 | `		 * yields the aliasable read slot the by-ref binder needs. */` |
|  117441 | 1544 | `		VmDeferredPath *pPath = 0;` |
|  117441 | 1545 | `		int bDefer = 0, eRoot = 0;` |
|  117441 | 1546 | `		if( pTos->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|       - | 1547 | `			/* Nested: the base already carries a descriptor — extend it in place. */` |
|      20 | 1548 | `			pPath = (VmDeferredPath *)pTos->x.pOther;` |
|      20 | 1549 | `			bDefer = 1;` |
|  117432 | 1550 | `		}else if( pTos->iFlags & MEMOBJ_AUX_DEFERRED ){` |
|       - | 1551 | `			/* Undefined base variable (commit-1 marker): root the descriptor by name. */` |
|       - | 1552 | `			SyString sRootName;` |
|       3 | 1553 | `			SyStringInitFromBuf(&sRootName,(const char *)pTos->x.pOther,` |
|       - | 1554 | `				pTos->x.pOther ? SyStrlen((const char *)pTos->x.pOther) : 0);` |
|       3 | 1555 | `			pPath = VmDeferPathNew(&(*pVm),1,SXU32_HIGH,&sRootName);` |
|       3 | 1556 | `			pTos->iFlags &= ~MEMOBJ_AUX_DEFERRED; /* borrowed name; don't free x.pOther */` |
|       3 | 1557 | `			pTos->x.pOther = 0;` |
|       3 | 1558 | `			bDefer = (pPath != 0);` |
|  117422 | 1559 | `		}else if( pTos->nIdx != SXU32_HIGH ){` |
|       - | 1560 | `			/* A real base slot: array/scalar -> eRoot 0 (by-ref may vivify), string -> 2. */` |
|  117197 | 1561 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       - | 1562 | `				/* Probe hit/miss on a COPY of the key: PH7_HashmapLookup casts a NULL key to` |
|       - | 1563 | `				 * "" in place, which would suppress the fall-through read's null-offset` |
|       - | 1564 | `				 * deprecation (hit) and capture the wrong key in the step (miss). */` |
|       - | 1565 | `				ph7_value idxProbe;` |
|   66407 | 1566 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   66407 | 1567 | `				PH7_MemObjInit(&(*pVm),&idxProbe);` |
|   66407 | 1568 | `				PH7_MemObjStore(pIdx,&idxProbe);` |
|   66407 | 1569 | `				if( PH7_HashmapLookup(pMap,&idxProbe,&pNode) == SXRET_OK ){` |
|   66347 | 1570 | `					bDefer = 0; /* present key: fall through and read it as an aliasable slot */` |
|   33171 | 1571 | `				}else{` |
|      63 | 1572 | `					eRoot = 0; bDefer = 1;` |
|       - | 1573 | `				}` |
|   66407 | 1574 | `				PH7_MemObjRelease(&idxProbe);` |
|   83991 | 1575 | `			}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       - | 1576 | `				/* An ArrayAccess base splits the way php's read_dimension does. A WRITABLE` |
|       - | 1577 | `				 * container answers out of its own storage with no accessor call, so the` |
|       - | 1578 | `				 * fetch really can wait for the callee: deferring it is what lets a` |
|       - | 1579 | `				 * by-reference argument take php's WRITE fetch, which CREATES a missing key` |
|       - | 1580 | ``				 * (`sort($ao['nokey'])`) instead of warning about a read and passing NULL.`` |
|       - | 1581 | `				 * Everything else answers through a METHOD, and php runs that method where` |
|       - | 1582 | `				 * the subscript is WRITTEN — so the accessor runs below and its RESULT rides` |
|       - | 1583 | `				 * a prefetch carrier built at the tail of the ArrayAccess branch. */` |
|     169 | 1584 | `				ph7_class_instance *pRecInst = (ph7_class_instance *)pTos->x.pOther;` |
|     169 | 1585 | `				eRoot = 0;` |
|     335 | 1586 | `				bDefer = (pRecInst && pVm->pArrayAccessClass` |
|     166 | 1587 | `				       && PH7_VmInstanceOf(pRecInst->pClass,pVm->pArrayAccessClass)` |
|     249 | 1588 | `				       && PH7_VmDimFetchWritable(pRecInst->pClass)) ? 1 : 0;` |
|   50712 | 1589 | `			}else if( pTos->iFlags & MEMOBJ_STRING ){` |
|   50629 | 1590 | `				eRoot = 2; bDefer = 1;` |
|   25620 | 1591 | `			}else{` |
|     ! 0 | 1592 | `				eRoot = 0; bDefer = 1; /* NULL/other reachable scalar base */` |
|       - | 1593 | `			}` |
|  117197 | 1594 | `			if( bDefer && pPath == 0 ){` |
|   50765 | 1595 | `				pPath = VmDeferPathNew(&(*pVm),eRoot,pTos->nIdx,0);` |
|   50765 | 1596 | `				if( pPath == 0 ){` |
|     ! 0 | 1597 | `					bDefer = 0;` |
|     ! 0 | 1598 | `				}` |
|   25683 | 1599 | `			}` |
|   58894 | 1600 | `		}` |
|  117441 | 1601 | `		if( bDefer ){` |
|       - | 1602 | `			/* Append this element step (deep-copies pIdx) and leave the carrier on pTos. */` |
|   50785 | 1603 | `			if( VmDeferPathPushElem(pPath,pIdx) == SXRET_OK ){` |
|   50785 | 1604 | `				if( (pTos->iFlags & MEMOBJ_AUX_DEFPATH) == 0 ){` |
|       - | 1605 | `					/* Collapse the base value into the descriptor carrier. */` |
|   50767 | 1606 | `					PH7_MemObjRelease(pTos);` |
|   50767 | 1607 | `					pTos->x.pOther = pPath;` |
|   50767 | 1608 | `					pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|   50767 | 1609 | `					pTos->nIdx = SXU32_HIGH;` |
|   25684 | 1610 | `				}` |
|   50785 | 1611 | `				PH7_MemObjRelease(pIdx);` |
|   50785 | 1612 | `				VM_EXIT_BREAK;` |
|       - | 1613 | `			}` |
|       - | 1614 | `			/* Out of memory appending a step. */` |
|     ! 0 | 1615 | `			if( pTos->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|       - | 1616 | `				/* Nested base: pPath IS pTos's carrier and stays owned by it — keep it (a` |
|       - | 1617 | `				 * step short) and exit, rather than fall through and misread a NULL-typed` |
|       - | 1618 | `				 * carrier as an array base. Resolves the shorter path; OOM-only degradation. */` |
|     ! 0 | 1619 | `				PH7_MemObjRelease(pIdx);` |
|     ! 0 | 1620 | `				VM_EXIT_BREAK;` |
|       - | 1621 | `			}` |
|       - | 1622 | `			/* A freshly-allocated path we still own: drop it and fall back to a plain read. */` |
|     ! 0 | 1623 | `			VmFreeDeferredPath(pPath);` |
|     ! 0 | 1624 | `		}` |
|   33322 | 1625 | `	}` |
| 1208747 | 1626 | `	if( iP2 == 7 && (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|       - | 1627 | ``		/* Keyed list destructuring `["k"=>$v] = $src` from a NON-array source: yield NULL`` |
|       - | 1628 | `		 * (never char-index a string), warning once per key — matching PHP, which warns per` |
|       - | 1629 | `		 * key here. A NULL source is silent; unlike the positional OP_LOAD_LIST path, a bool` |
|       - | 1630 | `		 * source DOES warn (PHP warns for bool in keyed destructuring).` |
|       - | 1631 | `		 * An OBJECT is not one of these: php destructures it through its` |
|       - | 1632 | `		 * read_dimension handler like any other subscript, so it falls through to` |
|       - | 1633 | `		 * the object dispatch below — which answers out of the accessor and` |
|       - | 1634 | ``		 * raises php's `Cannot use object of type C as array` for a class that`` |
|       - | 1635 | ``		 * has none. `["k"=>$v] = $obj` warned and yielded NULL for every source`` |
|       - | 1636 | `		 * but the one shape (a writable container answering out of its own` |
|       - | 1637 | `		 * storage) that reached the fast path underneath. */` |
|       7 | 1638 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       5 | 1639 | `			VmWarnCannotUseAsArray(&(*pVm),pTos->iFlags);` |
|       2 | 1640 | `		}` |
|       7 | 1641 | `		if( pIdx ){` |
|       - | 1642 | `			/* Release the key (a string literal for keyed destructuring), like the` |
|       - | 1643 | `			 * normal hashmap-read exit below — otherwise its blob is orphaned. */` |
|       7 | 1644 | `			PH7_MemObjRelease(pIdx);` |
|       3 | 1645 | `		}` |
|       7 | 1646 | `		PH7_MemObjRelease(pTos);` |
|       7 | 1647 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|       7 | 1648 | `		VM_EXIT_BREAK;` |
|       - | 1649 | `	}` |
| 1208741 | 1650 | `	bBaseStrOff = (pTos->iFlags & MEMOBJ_AUX_STROFFSET) != 0;` |
| 1208741 | 1651 | `	if( bBaseStrOff && VmIdxCtxIsContainerWrite(iP2) ){` |
|       - | 1652 | `		/* A string OFFSET used as a CONTAINER. php reaches a string offset through a` |
|       - | 1653 | `		 * marker zval and lets whichever opcode CONSUMES it name the refusal` |
|       - | 1654 | ``		 * (`zend_wrong_string_offset_error`); a fetch that wants to reach INSIDE it —`` |
|       - | 1655 | ``		 * `$s[0][1] = 'x'`, `$s[0][1] += 1`, `unset($s[0][1])`,`` |
|       - | 1656 | ``		 * `$s[0][] = 'x'`, a by-reference argument — is `Cannot use string offset as`` |
|       - | 1657 | ``		 * an array`. PHL read the character and handed back a value still carrying the`` |
|       - | 1658 | ``		 * BASE STRING's slot, so the write landed on the base: `$s = 'ab';`` |
|       - | 1659 | ``		 * $s[0][1] = 'x';` left `$s === 'ax'` and `$a['k'][0][1] = 'x'` rewrote the`` |
|       - | 1660 | ``		 * ELEMENT. A READ (`$x = $s[0][1]`) and the lookup contexts are php's own`` |
|       - | 1661 | `		 * silence and stay out of this. */` |
|       9 | 1662 | `		if( pIdx ){` |
|       9 | 1663 | `			PH7_MemObjRelease(pIdx);` |
|       4 | 1664 | `		}` |
|       9 | 1665 | `		rc = VmThrowFromVm(&(*pVm),"Error","Cannot use string offset as an array",` |
|       - | 1666 | `			sizeof("Cannot use string offset as an array")-1);` |
|       9 | 1667 | `		PH7_MemObjRelease(pTos);` |
|       9 | 1668 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|       9 | 1669 | `		pTos->nIdx = SXU32_HIGH;` |
|       9 | 1670 | `		if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       9 | 1671 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1672 | `	}` |
| 1208733 | 1673 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|       - | 1674 | `		/* String access */` |
|  951260 | 1675 | `		if( pIdx == 0 && VmIdxCtxIsContainerWrite(iP2) ){` |
|       - | 1676 | ``			/* `$s[] op= v` / `$s[]++`: php refuses an APPEND to a string wherever it`` |
|       - | 1677 | ``			 * lands — `[] operator not supported for strings` — and the plain`` |
|       - | 1678 | ``			 * `$s[] = 'x'` store already raises it (OP_STORE_IDX). The`` |
|       - | 1679 | `			 * read-modify-write spellings fell through to "load NULL" here and then` |
|       - | 1680 | `` 			 * wrote the computed value back through the BASE's slot, so `$s[] .= 'x'` `` |
|       - | 1681 | ``			 * APPENDED to the string and `$s[]++` incremented the whole of it. */`` |
|       5 | 1682 | `			rc = VmThrowFromVm(&(*pVm),"Error","[] operator not supported for strings",` |
|       - | 1683 | `				sizeof("[] operator not supported for strings")-1);` |
|       5 | 1684 | `			PH7_MemObjRelease(pTos);` |
|       5 | 1685 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       5 | 1686 | `			pTos->nIdx = SXU32_HIGH;` |
|       5 | 1687 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       5 | 1688 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1689 | `		}` |
|  951256 | 1690 | `		if( iP2 == VM_IDX_CTX_UNSET ){` |
|       - | 1691 | ``			/* php: a string offset cannot be unset AT ALL — `unset($s[0])` is the`` |
|       - | 1692 | ``			 * catchable `Error: Cannot unset string offsets`, whatever the offset is`` |
|       - | 1693 | `			 * and whether or not it is in range. PHL read the character but left the` |
|       - | 1694 | `			 * BASE VARIABLE's slot index on the result, so the trailing unset()` |
|       - | 1695 | ``			 * builtin freed the base itself: `$s = "abc"; unset($s[1]);` left $s`` |
|       - | 1696 | ``			 * UNDEFINED, and `unset($a["k"][0])` deleted the whole element.`` |
|       - | 1697 | ``			 * An INTERMEDIATE level of the chain (`unset($s[0][1])`,`` |
|       - | 1698 | ``			 * `unset($s[0]->p)`) is NOT this: that fetch hands the offset on to`` |
|       - | 1699 | `			 * something that reaches INSIDE it, and php words the refusal from` |
|       - | 1700 | `			 * whatever that is — so it falls through to the offset resolution below` |
|       - | 1701 | `			 * (a write-shaped fetch) and the consumer raises. */` |
|      14 | 1702 | `			rc = VmThrowFromVm(&(*pVm),"Error","Cannot unset string offsets",` |
|       - | 1703 | `				sizeof("Cannot unset string offsets")-1);` |
|      14 | 1704 | `			if( pIdx ){` |
|      14 | 1705 | `				PH7_MemObjRelease(pIdx);` |
|       6 | 1706 | `			}` |
|      14 | 1707 | `			PH7_MemObjRelease(pTos);` |
|      14 | 1708 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      14 | 1709 | `			pTos->nIdx = SXU32_HIGH;` |
|      14 | 1710 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      14 | 1711 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1712 | `		}` |
|  951244 | 1713 | `		if( pIdx ){` |
|  951244 | 1714 | `			sxi64 iOfft = 0, iRaw;` |
|  951244 | 1715 | `			sxi64 nLen = (sxi64)SyBlobLength(&pTos->sBlob);` |
|       - | 1716 | `			/* LOAD_IDX carries its OWN iP2 codes, which do NOT line up with the` |
|       - | 1717 | ``			 * PH7_MEMBER_* ones: 4 = isset, 5 = unset, 6 = empty, 8 = `??` read,`` |
|       - | 1718 | ``			 * 3 = `??=` peek. All are LOOKUPS, but php splits them into TWO`` |
|       - | 1719 | `			 * levels: isset()/empty()/unset() say nothing at all, while a` |
|       - | 1720 | ``			 * `??`/`??=` fetch still warns about the offset SHAPE and reads it`` |
|       - | 1721 | `			 * (VM_STROFF_COALESCE). The ??= peek is recognised by the NULLC_JMP` |
|       - | 1722 | `			 * that follows it, since its iP2 does not distinguish the base type. */` |
| 1428514 | 1723 | `			int iOfftLevel = (iP2 == 4 \|\| iP2 == VM_IDX_CTX_UNSET \|\| iP2 == 6) ? VM_STROFF_ISSET` |
| 1902406 | 1724 | `				: (iP2 == VM_IDX_CTX_UNSET_BASE ? VM_STROFF_UNSETBASE` |
| 1898958 | 1725 | `				: ((iP2 == 8 \|\| VmIdxFeedsCoalesce(pInstr)) ? VM_STROFF_COALESCE` |
|  947771 | 1726 | `				: VM_STROFF_LOUD));` |
|  951244 | 1727 | `			int bQuiet = iOfftLevel != VM_STROFF_LOUD;` |
|       - | 1728 | `			SyBlob sTypeMsg;` |
|       - | 1729 | `			int eOfft;` |
|  951244 | 1730 | `			VmCoalStrOff *pCoalOff = 0;` |
|  951244 | 1731 | `			if( VmIdxFeedsCoalesceAssign(pInstr) ){` |
|       - | 1732 | ``				/* `$s[k] ??= v`: the OP_NULLC_STORE ahead has to write into the`` |
|       - | 1733 | `				 * string OFFSET, and by then the offset is gone — this op consumes` |
|       - | 1734 | `				 * it. Carry the RAW index to the store on the peek's own result` |
|       - | 1735 | `				 * (MEMOBJ_AUX_COALSTROFF), which nests and cannot leak; the copy` |
|       - | 1736 | `				 * must predate the resolution below, which casts a float/null/bool` |
|       - | 1737 | `				 * in place, because php re-resolves the offset LOUDLY at the store:` |
|       - | 1738 | `				 * the peek is the quiet half of its pair. */` |
|      55 | 1739 | `				pCoalOff = VmCoalStrOffNew(&(*pVm),pIdx);` |
|      27 | 1740 | `			}` |
|  951244 | 1741 | `			eOfft = VmStringOffsetResolve(&(*pVm),pIdx,iOfftLevel,&iOfft,&sTypeMsg);` |
|  951244 | 1742 | `			if( eOfft == VM_STROFF_MISS ){` |
|       - | 1743 | `				/* A lookup over an offset php refuses: not set, in silence. */` |
|      32 | 1744 | `				PH7_MemObjRelease(pIdx);` |
|      32 | 1745 | `				PH7_MemObjRelease(pTos);` |
|      32 | 1746 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      32 | 1747 | `				if( pCoalOff && bBaseStrOff ){` |
|       - | 1748 | `					/* The store this peek is arming would land INSIDE a string offset:` |
|       - | 1749 | `					 * refused, like every other reach-inside (the same answer the` |
|       - | 1750 | `					 * out-of-range peek below gets). */` |
|     ! 0 | 1751 | `					VmFreeCoalStrOff(pCoalOff);` |
|     ! 0 | 1752 | `					rc = VmThrowFromVm(&(*pVm),"Error","Cannot use string offset as an array",` |
|       - | 1753 | `						sizeof("Cannot use string offset as an array")-1);` |
|     ! 0 | 1754 | `					pTos->nIdx = SXU32_HIGH;` |
|      35 | 1755 | `					if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     ! 0 | 1756 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1757 | `				}` |
|      32 | 1758 | `				if( pCoalOff ){` |
|       - | 1759 | ``					/* A `??=` whose offset the READ refuses: php raises at the`` |
|       - | 1760 | `					 * STORE instead, so keep the base slot reachable and hand the` |
|       - | 1761 | `					 * offset to OP_NULLC_STORE. */` |
|       3 | 1762 | `					pTos->x.pOther = (void *)pCoalOff;` |
|       3 | 1763 | `					pTos->iFlags \|= MEMOBJ_AUX_STROFFSET\|MEMOBJ_AUX_COALSTROFF;` |
|       2 | 1764 | `				}else{` |
|      30 | 1765 | `					pTos->nIdx = SXU32_HIGH;` |
|       - | 1766 | `				}` |
|      32 | 1767 | `				VM_EXIT_BREAK;` |
|       - | 1768 | `			}` |
|  951214 | 1769 | `			if( eOfft == VM_STROFF_REJECT && iOfftLevel == VM_STROFF_UNSETBASE ){` |
|       - | 1770 | `				/* The intermediate step of an unset chain over an offset php cannot` |
|       - | 1771 | ``				 * use at all (`unset($s["k"][0])`, `unset($s[""]->p)`): the refusal`` |
|       - | 1772 | `				 * is the UNSET's, not the read's TypeError. */` |
|     ! 0 | 1773 | `				VmFreeCoalStrOff(pCoalOff);` |
|     ! 0 | 1774 | `				SyBlobRelease(&sTypeMsg);` |
|     ! 0 | 1775 | `				rc = VmThrowFromVm(&(*pVm),"Error","Cannot unset string offsets",` |
|       - | 1776 | `					sizeof("Cannot unset string offsets")-1);` |
|     ! 0 | 1777 | `				PH7_MemObjRelease(pIdx);` |
|     ! 0 | 1778 | `				PH7_MemObjRelease(pTos);` |
|     ! 0 | 1779 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|     ! 0 | 1780 | `				pTos->nIdx = SXU32_HIGH;` |
|     ! 0 | 1781 | `				if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     ! 0 | 1782 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1783 | `			}` |
|  951214 | 1784 | `			if( eOfft == VM_STROFF_REJECT ){` |
|       - | 1785 | `				/* php's TypeError for an offset type a string refuses. PHL cast` |
|       - | 1786 | ``				 * every offset to int, so `$s[""]`, `$s["-"]` and `$s["p"]` all`` |
|       - | 1787 | ``				 * answered `$s[0]`. Routed as a mid-expression throw (this opcode`` |
|       - | 1788 | `				 * is not a call boundary), so the rest of the expression is` |
|       - | 1789 | `				 * abandoned the way php abandons it. */` |
|      41 | 1790 | `				VmFreeCoalStrOff(pCoalOff);` |
|      41 | 1791 | `				rc = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|      41 | 1792 | `				PH7_MemObjRelease(pIdx);` |
|      41 | 1793 | `				PH7_MemObjRelease(pTos);` |
|      41 | 1794 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      41 | 1795 | `				pTos->nIdx = SXU32_HIGH;` |
|      41 | 1796 | `				if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      41 | 1797 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1798 | `			}` |
|  951176 | 1799 | `			iRaw = iOfft;` |
|       - | 1800 | `			/* php 7.1: a NEGATIVE offset counts back from the end ($s[-1] is the last` |
|       - | 1801 | `			 * character). The offset used to be cast to UNSIGNED, so -1 became a huge` |
|       - | 1802 | `			 * number, ran past the end and quietly produced NULL. */` |
|  951176 | 1803 | `			if( iOfft < 0 ){` |
|      20 | 1804 | `				iOfft += nLen;` |
|       9 | 1805 | `			}` |
|  951176 | 1806 | `			if( iOfft < 0 \|\| iOfft >= nLen ){` |
|       - | 1807 | `				/* Out of range. In an isset()/empty() lookup php answers FALSE, so load` |
|       - | 1808 | `				 * NULL there; everywhere else it WARNS and yields the empty string (PH7` |
|       - | 1809 | `				 * silently produced NULL in both cases). */` |
|      89 | 1810 | `				PH7_MemObjRelease(pTos);` |
|      89 | 1811 | `				if( bQuiet ){` |
|      70 | 1812 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      36 | 1813 | `				}else{` |
|      20 | 1814 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|      20 | 1815 | `					if( iP2 != 1 && iP2 != VM_IDX_CTX_UNSET_BASE ){` |
|       - | 1816 | `						/* A WRITE-context fetch never READS the character in php: it` |
|       - | 1817 | `						 * resolves the offset (loudly — the SHAPE diagnostics above are` |
|       - | 1818 | `						 * php's there too) and hands back its offset marker, and the` |
|       - | 1819 | ``						 * opcode that consumes it refuses. So `$s[5] .= 'x'` and`` |
|       - | 1820 | ``						 * `$s[5]++` are the assign-op / incr-decr Error with nothing`` |
|       - | 1821 | ``						 * said about offset 5, where PHL announced an `Uninitialized`` |
|       - | 1822 | ``						 * string offset 5` it never had to look at. */`` |
|      23 | 1823 | `						VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Uninitialized string offset %qd",` |
|       7 | 1824 | `							iRaw);` |
|       7 | 1825 | `					}` |
|       - | 1826 | `				}` |
|      46 | 1827 | `			}else{` |
|  951090 | 1828 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|  951090 | 1829 | `				int c = zData[iOfft];` |
|  951090 | 1830 | `				PH7_MemObjRelease(pTos);` |
|  951090 | 1831 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|  951090 | 1832 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|       - | 1833 | `			}` |
|  951176 | 1834 | `			if( pCoalOff ){` |
|      51 | 1835 | `				if( (pTos->iFlags & MEMOBJ_NULL) && bBaseStrOff ){` |
|       - | 1836 | ``					/* `$s[0][9] ??= v`: the peek came back empty, so the `??=` WILL`` |
|       - | 1837 | `					 * store — and the thing it would store into is a character inside a` |
|       - | 1838 | `					 * string offset, which php refuses like every other reach-inside.` |
|       - | 1839 | `					 * The refusal belongs to the store, which is why the peek that finds` |
|       - | 1840 | ``					 * a character (`$s[0][0] ??= 7`) short-circuits and says nothing at`` |
|       - | 1841 | `					 * all in php. */` |
|       3 | 1842 | `					VmFreeCoalStrOff(pCoalOff);` |
|       3 | 1843 | `					rc = VmThrowFromVm(&(*pVm),"Error","Cannot use string offset as an array",` |
|       - | 1844 | `						sizeof("Cannot use string offset as an array")-1);` |
|       3 | 1845 | `					PH7_MemObjRelease(pTos);` |
|       3 | 1846 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       3 | 1847 | `					pTos->nIdx = SXU32_HIGH;` |
|       3 | 1848 | `					if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       3 | 1849 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1850 | `				}` |
|      49 | 1851 | `				if( pTos->iFlags & MEMOBJ_NULL ){` |
|       - | 1852 | ``					/* Out of range: the `??=` will store, so hand the offset over. */`` |
|      41 | 1853 | `					pTos->x.pOther = (void *)pCoalOff;` |
|      41 | 1854 | `					pTos->iFlags \|= MEMOBJ_AUX_COALSTROFF;` |
|      21 | 1855 | `				}else{` |
|       - | 1856 | ``					/* A real byte: the `??=` short-circuits over the store. */`` |
|       9 | 1857 | `					VmFreeCoalStrOff(pCoalOff);` |
|       - | 1858 | `				}` |
|      24 | 1859 | `			}` |
|       - | 1860 | `			/* The result still carries the BASE VARIABLE's slot index, which is` |
|       - | 1861 | `			 * harmless for a plain read and WRONG for anything that would ALIAS it:` |
|       - | 1862 | `			 * a string offset is not a slot. Mark it so the reference-binding sites` |
|       - | 1863 | `			 * raise php's Error instead of aliasing the whole string --` |
|       - | 1864 | ``			 * `$r = &$s[1]; $r = "Z";` REPLACED $s with "Z". */`` |
|  951174 | 1865 | `			pTos->iFlags \|= MEMOBJ_AUX_STROFFSET;` |
|  477285 | 1866 | `		}else{` |
|       - | 1867 | `			/* No available index,load NULL */` |
|     ! 0 | 1868 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       - | 1869 | `		}` |
|  951174 | 1870 | `		VM_EXIT_BREAK;` |
|       - | 1871 | `	}` |
|  257478 | 1872 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       - | 1873 | `		/* Object subscript: ArrayAccess dispatch.` |
|       - | 1874 | `		 * iP2 codes:` |
|       - | 1875 | `		 *   0 = read       → offsetGet` |
|       - | 1876 | `		 *   3 = ??= peek   → offsetExists; offsetGet on hit; arm coalesce` |
|       - | 1877 | `		 *                    target on miss for the upcoming NULLC_STORE` |
|       - | 1878 | `		 *   4 = isset()    → offsetExists` |
|       - | 1879 | `		 *   5 = unset()    → offsetUnset` |
|       - | 1880 | `		 *   6 = empty()    → offsetExists, then offsetGet on hit` |
|       - | 1881 | ``		 *   8 = `??` read  → same probe as 6 (offsetExists, then offsetGet on a`` |
|       - | 1882 | `		 *                    hit) and no diagnostics anywhere in this op: php` |
|       - | 1883 | ``		 *                    evaluates the whole left operand of `??` in`` |
|       - | 1884 | `		 *                    isset-context, and calling offsetGet blindly also` |
|       - | 1885 | `		 *                    surfaced warnings raised INSIDE a userland` |
|       - | 1886 | `		 *                    offsetGet (e.g. ArrayObject's own array read). */` |
|    1055 | 1887 | `		ph7_class_instance *pInst = (ph7_class_instance *)pTos->x.pOther;` |
|    1055 | 1888 | `		ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|       - | 1889 | `		/* php's read_dimension / has_dimension HANDLERS, which a native class may` |
|       - | 1890 | ``		 * carry without implementing ArrayAccess -- `$list[0]` reads a DOMNodeList`` |
|       - | 1891 | ``		 * there while `$list instanceof ArrayAccess` is false. They come FIRST`` |
|       - | 1892 | `		 * because php's interface is implemented THROUGH the handler: a user` |
|       - | 1893 | `		 * subclass declaring ArrayAccess inherits the parent's handler, so its own` |
|       - | 1894 | `		 * offsetGet/offsetExists are not consulted for a READ. The WRITE half is` |
|       - | 1895 | `		 * not here at all -- a store, an append and an unset (iP2 5) fall past` |
|       - | 1896 | ``		 * this into php's `Cannot use object of type C as array` unless the class`` |
|       - | 1897 | `		 * really implements the interface, which is php's own split (that same` |
|       - | 1898 | `		 * subclass DOES get its offsetSet called). */` |
|    1055 | 1899 | `		if( pInst && iP2 != 5 && PH7_ClassHasNativeDim(pInst->pClass) ){` |
|       - | 1900 | `			PH7_NativeDimCtx sDim;` |
|       - | 1901 | `			ph7_value sResult;` |
|       - | 1902 | ``			/* `$o[$k] op= v` is php's read-then-WRITE pair, and php reports the`` |
|       - | 1903 | `			 * WRITE's refusal when the read answered nothing at all: an offset the` |
|       - | 1904 | ``			 * handler REFUSED comes back to `zend_binary_assign_op_obj_dim` as a`` |
|       - | 1905 | ``			 * miss, which raises `Cannot use object of type C as array` and chains`` |
|       - | 1906 | ``			 * the refusal behind it. `??=` is not that pair -- it reads in`` |
|       - | 1907 | `			 * isset-context, so its refusal is what surfaces -- and neither of them` |
|       - | 1908 | `			 * decides the store itself: that goes through the ordinary write path` |
|       - | 1909 | `			 * below, which is offsetSet for a subclass that has one. */` |
|     358 | 1910 | `			int bRmwCtx = (iP2 == 1) && VmNextIsCompoundAssign(pInstr + 1);` |
|     358 | 1911 | `			int bIsset = (iP2 == 4 \|\| iP2 == 6);` |
|     358 | 1912 | `			PH7_MemObjInit(&(*pVm),&sResult);` |
|     358 | 1913 | `			sDim.iMode = bIsset ? PH7_NATIVE_DIM_ISSET : PH7_NATIVE_DIM_READ;` |
|     358 | 1914 | `			sDim.pOffset = pIdx;` |
|     358 | 1915 | `			sDim.pResult = &sResult;` |
|     358 | 1916 | `			sDim.zThrowClass = 0;` |
|     358 | 1917 | `			sDim.zThrowMsg[0] = 0;` |
|     358 | 1918 | `			sDim.bStored = 0;` |
|     358 | 1919 | `			PH7_ClassNativeDim(pInst,&sDim);` |
|     358 | 1920 | `			if( iP2 == 6 && sDim.zThrowClass == 0 && ph7_value_to_bool(&sResult) ){` |
|       - | 1921 | `				/* empty(): php asks has_dimension first and reads the VALUE only on` |
|       - | 1922 | ``				 * a hit, which is why an out-of-range `empty($map[-1])` is a plain`` |
|       - | 1923 | `				 * TRUE where the read of the same offset refuses. The emptiness` |
|       - | 1924 | `				 * question goes to the handler first -- a class that judges it on` |
|       - | 1925 | `				 * something other than the value it would HAND BACK answers here --` |
|       - | 1926 | `				 * and a handler that has no answer leaves the value read below. */` |
|       7 | 1927 | `				PH7_MemObjRelease(&sResult);` |
|       7 | 1928 | `				PH7_MemObjInit(&(*pVm),&sResult);` |
|       7 | 1929 | `				sDim.iMode = PH7_NATIVE_DIM_NOTEMPTY;` |
|       7 | 1930 | `				sDim.pResult = &sResult;` |
|       7 | 1931 | `				PH7_ClassNativeDim(pInst,&sDim);` |
|       7 | 1932 | `				if( (sResult.iFlags & MEMOBJ_NULL) && sDim.zThrowClass == 0 ){` |
|       7 | 1933 | `					sDim.iMode = PH7_NATIVE_DIM_READ;` |
|       7 | 1934 | `					sDim.pResult = &sResult;` |
|       7 | 1935 | `					PH7_ClassNativeDim(pInst,&sDim);` |
|       3 | 1936 | `				}` |
|       3 | 1937 | `			}` |
|     358 | 1938 | `			if( sDim.zThrowClass ){` |
|       - | 1939 | `				char zMsg[256];` |
|      32 | 1940 | `				const char *zClass = sDim.zThrowClass;` |
|      32 | 1941 | `				const char *zText = sDim.zThrowMsg;` |
|       - | 1942 | `				sxu32 nMsg;` |
|      32 | 1943 | `				if( bRmwCtx ){` |
|       5 | 1944 | `					SyString *pName = &pInst->pClass->sName;` |
|       5 | 1945 | `					zClass = "Error";` |
|       5 | 1946 | `					zText = zMsg;` |
|       7 | 1947 | `					nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - | 1948 | `						"Cannot use object of type %.*s as array",` |
|       4 | 1949 | `						(int)pName->nByte,pName->zString);` |
|       3 | 1950 | `				}else{` |
|      28 | 1951 | `					nMsg = (sxu32)SyStrlen(zText);` |
|       - | 1952 | `				}` |
|      32 | 1953 | `				VmCoalesceDisarm(pVm);` |
|      32 | 1954 | `				rc = VmThrowFromVm(pVm,zClass,zText,nMsg);` |
|      32 | 1955 | `				PH7_MemObjRelease(&sResult);` |
|      32 | 1956 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|      32 | 1957 | `				PH7_MemObjRelease(pTos);` |
|      32 | 1958 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      32 | 1959 | `				pTos->nIdx = SXU32_HIGH;` |
|      32 | 1960 | `				if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      38 | 1961 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1962 | `			}` |
|     328 | 1963 | `			if( iP2 == 4 ){` |
|       - | 1964 | `				/* isset(): push a BOOL, which is also what keeps vm_builtin_isset` |
|       - | 1965 | `				 * from warning about a non-variable operand. */` |
|      33 | 1966 | `				int bExists = ph7_value_to_bool(&sResult);` |
|      33 | 1967 | `				PH7_MemObjRelease(&sResult);` |
|      33 | 1968 | `				PH7_MemObjRelease(pTos);` |
|      33 | 1969 | `				pTos->nIdx = SXU32_HIGH;` |
|      33 | 1970 | `				if( bExists ){` |
|       9 | 1971 | `					MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       9 | 1972 | `					pTos->x.iVal = 1;` |
|       5 | 1973 | `				}else{` |
|      25 | 1974 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       1 | 1975 | `				}` |
|     312 | 1976 | `			}else if( iP2 == 3 && (sResult.iFlags & MEMOBJ_NULL) ){` |
|       - | 1977 | ``				/* `$o[$k] ??= v` and the read found nothing: arm (object, key) so`` |
|       - | 1978 | `				 * the NULLC_STORE that follows performs php's store -- offsetSet for` |
|       - | 1979 | ``				 * a subclass that declares one, and `Cannot use object of type C as`` |
|       - | 1980 | ``				 * array` for the collections themselves, which is the same verdict`` |
|       - | 1981 | ``				 * the plain `$o[$k] = v` gets. */`` |
|       5 | 1982 | `				VmCoalesceDisarm(pVm);` |
|       5 | 1983 | `				PH7_MemObjRelease(pTos);` |
|       5 | 1984 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|       5 | 1985 | `				pTos->nIdx = SXU32_HIGH;` |
|       5 | 1986 | `				if( pIdx ){` |
|       5 | 1987 | `					PH7_MemObjStore(pIdx,&pVm->sCoalesceKey);` |
|       2 | 1988 | `				}` |
|       5 | 1989 | `				pVm->pCoalesceObj = pInst;` |
|       5 | 1990 | `				pInst->iRef++;` |
|       5 | 1991 | `				pVm->bCoalesceArmed = 1;` |
|       5 | 1992 | `				PH7_MemObjRelease(&sResult);` |
|       3 | 1993 | `			}else{` |
|       - | 1994 | `				/* The base slot may be the only thing holding this instance, and the` |
|       - | 1995 | `				 * write-context tail below still speaks for its CLASS -- hold a` |
|       - | 1996 | `				 * reference across the release, as the ArrayAccess arm does. */` |
|     292 | 1997 | `				pInst->iRef++;` |
|     292 | 1998 | `				if( iP2 == 3 ){` |
|       3 | 1999 | `					VmCoalesceDisarm(pVm); /* a hit short-circuits over the store */` |
|       1 | 2000 | `				}` |
|     292 | 2001 | `				PH7_MemObjRelease(pTos);` |
|     292 | 2002 | `				PH7_MemObjStore(&sResult,pTos);` |
|     292 | 2003 | `				pTos->nIdx = SXU32_HIGH;` |
|     292 | 2004 | `				if( bRmwCtx ){` |
|       - | 2005 | `					/* php's ASSIGN_DIM_OP: the read gave the current value, the op` |
|       - | 2006 | `					 * computes on it, and the result goes back out through the write` |
|       - | 2007 | `					 * path -- offsetSet where there is one, php's Error where there` |
|       - | 2008 | `					 * is not (VmHookRmwConsume). */` |
|      20 | 2009 | `					VmDimRmwArm(&(*pVm),pInst,pIdx,pTos,` |
|      12 | 2010 | `						(void *)pStack,(void *)aInstr,(sxu32)(pc + 1));` |
|     286 | 2011 | `				}else if( VmIdxFetchForWrite(pInstr,iP2) ){` |
|       - | 2012 | ``					/* php's `Indirect modification of overloaded element` -- silent`` |
|       - | 2013 | `					 * for an OBJECT, which is every value these containers answer,` |
|       - | 2014 | ``					 * and raised for the NULL a miss leaves (`$list[9]++`). */`` |
|       5 | 2015 | `					PH7_VmOverloadedElemNotice(&(*pVm),pInst->pClass,pTos);` |
|     278 | 2016 | `				}else if( pInstr->iP2 == 9 ){` |
|       - | 2017 | `					/* A deferred call ARGUMENT: the read has happened, and whether php` |
|       - | 2018 | `					 * performed a W fetch is the callee's to say. Carry the value plus` |
|       - | 2019 | `					 * the class that answered it so the verdict lands at the call. */` |
|     106 | 2020 | `					VmDeferredPath *pPre = VmDeferPathNewPrefetch(&(*pVm),VM_OVER_ELEM,` |
|      34 | 2021 | `						pInst->pClass,0,pTos);` |
|      72 | 2022 | `					if( pPre ){` |
|      72 | 2023 | `						PH7_MemObjRelease(pTos);` |
|      72 | 2024 | `						pTos->x.pOther = pPre;` |
|      72 | 2025 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|      72 | 2026 | `						pTos->nIdx = SXU32_HIGH;` |
|      34 | 2027 | `					}` |
|      34 | 2028 | `				}` |
|     292 | 2029 | `				PH7_ClassInstanceUnref(pInst);` |
|     292 | 2030 | `				PH7_MemObjRelease(&sResult);` |
|       - | 2031 | `			}` |
|     328 | 2032 | `			if( pIdx ){` |
|     328 | 2033 | `				PH7_MemObjRelease(pIdx);` |
|     162 | 2034 | `			}` |
|     328 | 2035 | `			VM_EXIT_BREAK;` |
|       - | 2036 | `		}` |
|     701 | 2037 | `		if( pArrayAccess && pInst && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|       - | 2038 | `			ph7_class_method *pMeth;` |
|       - | 2039 | `			ph7_value sResult;` |
|       - | 2040 | `			ph7_value sNullIdx;` |
|       - | 2041 | `			ph7_value *apArg[1];` |
|     676 | 2042 | `			if( pIdx && VmDimFastFetchCtx(pInstr,iP2) && PH7_VmDimFetchWritable(pInst->pClass)` |
|     283 | 2043 | `			 && pInst->iRef > 1 ){` |
|       - | 2044 | `				/* php hands a writable container's element back BY SLOT, and that is what` |
|       - | 2045 | ``				 * makes an indirect modification through it land. The `iRef > 1` guard is`` |
|       - | 2046 | ``				 * the object half of the array path's `pMap->iRef < 2` rule: releasing the`` |
|       - | 2047 | `				 * base below drops this stack slot's own reference, and a TEMPORARY` |
|       - | 2048 | ``				 * container (`(new ArrayObject([1]))[0]`) would be destroyed with its`` |
|       - | 2049 | `				 * storage while the result still views it. Such a base has nothing that` |
|       - | 2050 | `				 * could observe the write anyway, so it takes the accessor's copy. */` |
|     212 | 2051 | `				sxu32 nElem = PH7_SplDimElemSlot(&(*pVm),pInst,pIdx,` |
|       - | 2052 | `					/* php's write-context vivification, and only there: a W/RW fetch —` |
|       - | 2053 | ``					 * including the `$r = &$ao['k']` and by-ref-foreach shapes iP2 alone`` |
|       - | 2054 | `					 * cannot name — creates the missing element, while an unset chain's` |
|       - | 2055 | `					 * intermediate step never does. */` |
|     181 | 2056 | `					VmIdxFetchForWrite(pInstr,iP2) && !VM_IDX_IS_UNSET(iP2));` |
|     158 | 2057 | `				ph7_value *pElem = (nElem == SXU32_HIGH) ? 0` |
|     140 | 2058 | `					: (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nElem);` |
|     158 | 2059 | `				if( pElem ){` |
|     128 | 2060 | `					PH7_MemObjRelease(pTos);` |
|     128 | 2061 | `					PH7_MemObjLoad(pElem,pTos);` |
|     128 | 2062 | `					pTos->nIdx = nElem;` |
|     128 | 2063 | `					PH7_MemObjRelease(pIdx);` |
|     128 | 2064 | `					VM_EXIT_BREAK;` |
|       - | 2065 | `				}` |
|      14 | 2066 | `			}` |
|     555 | 2067 | `			if( (iP2 == 0 \|\| iP2 == 3 \|\| iP2 == 8) && pIdx == 0 ){` |
|       - | 2068 | ``				/* `$obj[]` read — PHP rejects this. */`` |
|     ! 0 | 2069 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|       - | 2070 | `					"Cannot use [] for reading");` |
|     ! 0 | 2071 | `				PH7_MemObjRelease(pTos);` |
|     ! 0 | 2072 | `				pTos->nIdx = SXU32_HIGH;` |
|     ! 0 | 2073 | `				VM_EXIT_BREAK;` |
|       - | 2074 | `			}` |
|     555 | 2075 | `			PH7_MemObjInit(&(*pVm),&sResult);` |
|     555 | 2076 | `			if( iP2 == 4 \|\| iP2 == 6 \|\| iP2 == 3 \|\| iP2 == 8 ){` |
|       - | 2077 | ``				/* isset, empty, ??= and `??` all start with offsetExists. */`` |
|     173 | 2078 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 2079 | `					"offsetExists",sizeof("offsetExists")-1);` |
|     173 | 2080 | `				apArg[0] = pIdx;` |
|     173 | 2081 | `				if( pMeth ){` |
|     173 | 2082 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|      89 | 2083 | `				}` |
|     471 | 2084 | `			}else if( iP2 == 5 ){` |
|      81 | 2085 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 2086 | `					"offsetUnset",sizeof("offsetUnset")-1);` |
|      81 | 2087 | `				apArg[0] = pIdx;` |
|      81 | 2088 | `				if( pMeth ){` |
|      81 | 2089 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|      38 | 2090 | `				}` |
|      43 | 2091 | `			}else{` |
|     311 | 2092 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 2093 | `					"offsetGet",sizeof("offsetGet")-1);` |
|     311 | 2094 | `				if( pIdx == 0 ){` |
|       - | 2095 | ``					/* `$o[] op= v` — the one read that reaches here without a key.`` |
|       - | 2096 | `					 * php hands the accessors NULL for the absent offset (its` |
|       - | 2097 | `					 * read_dimension substitutes one), so passing NO argument` |
|       - | 2098 | `					 * turned an assignment php performs into an` |
|       - | 2099 | `					 * ArgumentCountError against the class's own offsetGet. */` |
|       3 | 2100 | `					PH7_MemObjInit(&(*pVm),&sNullIdx);` |
|       3 | 2101 | `					pIdx = &sNullIdx;` |
|       1 | 2102 | `				}` |
|     311 | 2103 | `				apArg[0] = pIdx;` |
|     311 | 2104 | `				if( pMeth ){` |
|     311 | 2105 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|     153 | 2106 | `				}` |
|       - | 2107 | `			}` |
|     555 | 2108 | `			if( iP2 == 4 ){` |
|       - | 2109 | `				/* isset: push MEMOBJ_BOOL so vm_builtin_isset reports the` |
|       - | 2110 | `				 * right truth value AND skips its "Expecting a variable not` |
|       - | 2111 | `				 * a constant" warning (keyed on MEMOBJ_BOOL). */` |
|     119 | 2112 | `				int bExists = ph7_value_to_bool(&sResult);` |
|     119 | 2113 | `				PH7_MemObjRelease(pTos);` |
|     119 | 2114 | `				pTos->nIdx = SXU32_HIGH;` |
|     119 | 2115 | `				if( bExists ){` |
|      58 | 2116 | `					MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      58 | 2117 | `					pTos->x.iVal = 1;` |
|      31 | 2118 | `				}else{` |
|      65 | 2119 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       5 | 2120 | `				}` |
|     498 | 2121 | `			}else if( iP2 == 5 ){` |
|       - | 2122 | `				/* offsetUnset return is discarded; push NULL so the trailing` |
|       - | 2123 | `				 * vm_builtin_unset is a harmless no-op. */` |
|      81 | 2124 | `				PH7_MemObjRelease(pTos);` |
|      81 | 2125 | `				pTos->nIdx = SXU32_HIGH;` |
|      81 | 2126 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|     403 | 2127 | `			}else if( iP2 == 6 \|\| iP2 == 8 ){` |
|       - | 2128 | `				/* empty: if offsetExists is false, push NULL so empty=true` |
|       - | 2129 | `				 * without calling offsetGet. If true, call offsetGet and` |
|       - | 2130 | `				 * push the value so PH7_builtin_empty evaluates emptiness.` |
|       - | 2131 | ``				 * `??` (8) needs the identical shape: NULL on a miss so the`` |
|       - | 2132 | `				 * coalesce takes the default, the real value on a hit. */` |
|      51 | 2133 | `				int bExists = ph7_value_to_bool(&sResult);` |
|      51 | 2134 | `				PH7_MemObjRelease(&sResult);` |
|      51 | 2135 | `				PH7_MemObjRelease(pTos);` |
|      51 | 2136 | `				pTos->nIdx = SXU32_HIGH;` |
|      51 | 2137 | `				if( !bExists ){` |
|      24 | 2138 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      14 | 2139 | `				}else{` |
|      31 | 2140 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 2141 | `						"offsetGet",sizeof("offsetGet")-1);` |
|       - | 2142 | `					ph7_value sValue;` |
|      31 | 2143 | `					PH7_MemObjInit(&(*pVm),&sValue);` |
|      31 | 2144 | `					apArg[0] = pIdx;` |
|      31 | 2145 | `					if( pGet ){` |
|      31 | 2146 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|      13 | 2147 | `					}` |
|      31 | 2148 | `					PH7_MemObjStore(&sValue,pTos);` |
|      31 | 2149 | `					PH7_MemObjRelease(&sValue);` |
|       - | 2150 | `				}` |
|      51 | 2151 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|      51 | 2152 | `				VM_EXIT_BREAK; /* skip the duplicate sResult release below */` |
|     319 | 2153 | `			}else if( iP2 == 3 ){` |
|       - | 2154 | `				/* ?? null-coalesce peek: emulate PHP semantics —` |
|       - | 2155 | `				 *   if !offsetExists OR offsetGet() === null → arm` |
|       - | 2156 | `				 *     coalesce slot (NULLC_STORE will call offsetSet)` |
|       - | 2157 | `				 *     and push NULL.` |
|       - | 2158 | `				 *   else → push offsetGet's value (NULLC_JMP skips). */` |
|      10 | 2159 | `				int bExists = ph7_value_to_bool(&sResult);` |
|      10 | 2160 | `				int bShouldArm = !bExists;` |
|       - | 2161 | `				ph7_value sValue;` |
|      10 | 2162 | `				PH7_MemObjRelease(&sResult);` |
|       - | 2163 | `				/* Reset any prior arming defensively */` |
|      10 | 2164 | `				VmCoalesceDisarm(pVm);` |
|      10 | 2165 | `				PH7_MemObjInit(&(*pVm),&sValue);` |
|      10 | 2166 | `				if( bExists ){` |
|       5 | 2167 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 2168 | `						"offsetGet",sizeof("offsetGet")-1);` |
|       5 | 2169 | `					apArg[0] = pIdx;` |
|       5 | 2170 | `					if( pGet ){` |
|       5 | 2171 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|       2 | 2172 | `					}` |
|       5 | 2173 | `					if( sValue.iFlags & MEMOBJ_NULL ){` |
|       3 | 2174 | `						bShouldArm = 1;` |
|       1 | 2175 | `					}` |
|       2 | 2176 | `				}` |
|      10 | 2177 | `				PH7_MemObjRelease(pTos);` |
|      10 | 2178 | `				pTos->nIdx = SXU32_HIGH;` |
|      10 | 2179 | `				if( bShouldArm ){` |
|       - | 2180 | `					/* Arm: remember (object, key) so NULLC_STORE dispatches` |
|       - | 2181 | `					 * to offsetSet. Hold a ref on the instance to survive` |
|       - | 2182 | `					 * intervening expression evaluation. */` |
|       8 | 2183 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       8 | 2184 | `					if( pIdx ){` |
|       8 | 2185 | `						PH7_MemObjStore(pIdx,&pVm->sCoalesceKey);` |
|       3 | 2186 | `					}` |
|       8 | 2187 | `					pVm->pCoalesceObj = pInst;` |
|       8 | 2188 | `					pInst->iRef++;` |
|       8 | 2189 | `					pVm->bCoalesceArmed = 1;` |
|       5 | 2190 | `				}else{` |
|       3 | 2191 | `					PH7_MemObjStore(&sValue,pTos);` |
|       - | 2192 | `				}` |
|      10 | 2193 | `				PH7_MemObjRelease(&sValue);` |
|      10 | 2194 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|      10 | 2195 | `				VM_EXIT_BREAK;` |
|     ! 0 | 2196 | `			}else{` |
|       - | 2197 | `				/* offsetGet: replace pTos with the returned value.` |
|       - | 2198 | `				 *` |
|       - | 2199 | `				 * The base slot may be the only thing holding this instance — a` |
|       - | 2200 | ``				 * TEMPORARY container (`f((new C)['a'])`, a getter's return) dies with`` |
|       - | 2201 | `				 * it — and everything below still speaks for the object: the writable` |
|       - | 2202 | `				 * test, php's notice and the read-modify-write arming all read its` |
|       - | 2203 | `				 * CLASS, and the deferred-argument carrier records it. Hold a reference` |
|       - | 2204 | `				 * of our own across the release so none of them is left reading freed` |
|       - | 2205 | `				 * memory. */` |
|     311 | 2206 | `				pInst->iRef++;` |
|     311 | 2207 | `				PH7_MemObjRelease(pTos);` |
|     311 | 2208 | `				PH7_MemObjStore(&sResult,pTos);` |
|     311 | 2209 | `				pTos->nIdx = SXU32_HIGH;` |
|     311 | 2210 | `				if( iP2 == 1 && VmNextIsCompoundAssign(pInstr + 1) ){` |
|       - | 2211 | ``					/* `$o[$k] op= v` is php's ASSIGN_DIM_OP: offsetGet gave the`` |
|       - | 2212 | `					 * current value, the op computes on it, and the result goes` |
|       - | 2213 | `					 * back through offsetSet($k, …). PHL had no write-back at` |
|       - | 2214 | `					 * all here — the fetched value carried no slot, so every` |
|       - | 2215 | `					 * compound assign on an ArrayAccess element died on` |
|       - | 2216 | `					 * "Cannot perform assignment on a constant class attribute"` |
|       - | 2217 | `					 * and stored nothing. Arm the scratch slot the op mutates;` |
|       - | 2218 | `					 * its tail (PH7_HOOK_RMW_WRITEBACK) dispatches offsetSet. */` |
|      64 | 2219 | `					VmDimRmwArm(&(*pVm),pInst,pIdx,pTos,` |
|      42 | 2220 | `						(void *)pStack,(void *)aInstr,(sxu32)(pc + 1));` |
|     286 | 2221 | `				}else if( VmIdxFetchForWrite(pInstr,iP2)` |
|     151 | 2222 | `				       && !PH7_VmDimFetchWritable(pInst->pClass) ){` |
|      25 | 2223 | `					PH7_VmOverloadedElemNotice(&(*pVm),pInst->pClass,pTos);` |
|     257 | 2224 | `				}else if( pInstr->iP2 == 9 ){` |
|       - | 2225 | `					/* A deferred call ARGUMENT. The accessor has just run — php runs it` |
|       - | 2226 | `					 * where the subscript is written, whatever the parameter turns out to` |
|       - | 2227 | `					 * be — but WHICH fetch php performed is the callee's to say, and only` |
|       - | 2228 | `					 * OP_CALL knows: a by-reference parameter makes it a W fetch, which on` |
|       - | 2229 | `					 * a container that can only answer with a VALUE is php's` |
|       - | 2230 | ``					 * `Indirect modification of overloaded element` notice and a write`` |
|       - | 2231 | `					 * thrown away. Carry the result plus the class that answered it, so the` |
|       - | 2232 | `					 * verdict lands at the call without the accessor running twice or the` |
|       - | 2233 | `					 * argument arriving as NULL. The value would otherwise reach the callee` |
|       - | 2234 | `					 * still SHARING the container's own nested map by COW, and a by-ref` |
|       - | 2235 | ``					 * `f($o['a']['b'])` wrote straight into the object php leaves untouched. */`` |
|      49 | 2236 | `					VmDeferredPath *pPre = VmDeferPathNewPrefetch(&(*pVm),VM_OVER_ELEM,pInst->pClass,0,pTos);` |
|      49 | 2237 | `					if( pPre ){` |
|      49 | 2238 | `						PH7_MemObjRelease(pTos);` |
|      49 | 2239 | `						pTos->x.pOther = pPre;` |
|      49 | 2240 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|      49 | 2241 | `						pTos->nIdx = SXU32_HIGH;` |
|      24 | 2242 | `					}` |
|      24 | 2243 | `				}` |
|     311 | 2244 | `				PH7_ClassInstanceUnref(pInst);` |
|       - | 2245 | `			}` |
|     501 | 2246 | `			PH7_MemObjRelease(&sResult);` |
|     501 | 2247 | `			if( pIdx ){` |
|     501 | 2248 | `				PH7_MemObjRelease(pIdx);` |
|     248 | 2249 | `			}` |
|     501 | 2250 | `			VM_EXIT_BREAK;` |
|       - | 2251 | `		}` |
|       - | 2252 | `		/* Object without ArrayAccess: PHP throws fatal Error in all subscript` |
|       - | 2253 | `		 * contexts (read, isset, unset, empty). Match it. A class carrying a` |
|       - | 2254 | `		 * READ handler reaches here only for the unset (iP2 5), which the hook` |
|       - | 2255 | `		 * branch above skips, and words that refusal itself. */` |
|      24 | 2256 | `		if( pInst && iP2 == 5 ){` |
|       - | 2257 | `			/* unset($o[$k]) on a handler that really removes something: php's` |
|       - | 2258 | `			 * SimpleXMLElement drops the attribute or the element. Offered the` |
|       - | 2259 | `			 * access before the refusal below. */` |
|       - | 2260 | `			PH7_NativeDimCtx sDim;` |
|      14 | 2261 | `			if( PH7_ClassNativeDimStore(pInst,PH7_NATIVE_DIM_UNSET,pIdx,0,&sDim)` |
|      14 | 2262 | `			 && sDim.zThrowClass == 0 ){` |
|       7 | 2263 | `				if( pIdx ){` |
|       7 | 2264 | `					PH7_MemObjRelease(pIdx);` |
|       3 | 2265 | `				}` |
|       7 | 2266 | `				PH7_MemObjRelease(pTos);` |
|       7 | 2267 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|       7 | 2268 | `				pTos->nIdx = SXU32_HIGH;` |
|       7 | 2269 | `				VM_EXIT_BREAK;` |
|       - | 2270 | `			}` |
|       4 | 2271 | `		}` |
|      17 | 2272 | `		if( pInst ){` |
|       - | 2273 | `			char zMsg[256];` |
|      24 | 2274 | `			sxu32 nMsg = PH7_ClassNativeDimRefusal(pInst,` |
|       7 | 2275 | `				iP2 == 5 ? PH7_NATIVE_DIM_UNSET : PH7_NATIVE_DIM_WRITE,` |
|       7 | 2276 | `				zMsg,sizeof(zMsg));` |
|      17 | 2277 | `			rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|      17 | 2278 | `			if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|      17 | 2279 | `			PH7_MemObjRelease(pTos);` |
|      17 | 2280 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      17 | 2281 | `			pTos->nIdx = SXU32_HIGH;` |
|      17 | 2282 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       - | 2283 | ``			/* `break` used to resume at the NEXT instruction: the catch ran and then`` |
|       - | 2284 | `			 * execution carried on inside the try block. */` |
|      19 | 2285 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2286 | `		}` |
|     ! 0 | 2287 | `	}` |
|  256428 | 2288 | `	if( (iP2 == 1 \|\| iP2 == 3 \|\| VM_IDX_IS_UNSET(iP2)) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       - | 2289 | `		{` |
|       - | 2290 | `			/* The base's TYPE decides here, whether or not there is a SLOT behind it.` |
|       - | 2291 | ``			 * A write THROUGH a temporary — `f()[0] = 5`, `f()[0][1] = 5` — gets the`` |
|       - | 2292 | `			 * same verdict from php; the only difference is that what it writes is` |
|       - | 2293 | `			 * discarded afterwards. PHL skipped the whole screen when the base had no` |
|       - | 2294 | ``			 * slot, so `ui()[0] += 5` over an int RESULT ran in silence where php`` |
|       - | 2295 | `			 * throws. The temporary is screened and vivified in place, on the stack —` |
|       - | 2296 | `			 * there is nowhere to write it back to. */` |
|     100 | 2297 | `			ph7_value *pObj = (pTos->nIdx != SXU32_HIGH)` |
|      58 | 2298 | `				? (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pTos->nIdx)` |
|      35 | 2299 | `				: pTos;` |
|      68 | 2300 | `			if( pObj != 0 ){` |
|       - | 2301 | `				/* php 8 write-context auto-vivify rules: NULL converts to array` |
|       - | 2302 | `				 * silently; FALSE converts with the 8.1 deprecation; any other` |
|       - | 2303 | `				 * scalar base — int/float/true/resource — is php's catchable` |
|       - | 2304 | `				 * "Cannot use a scalar value as an array" Error and the variable` |
|       - | 2305 | `				 * stays untouched (pre-fix the base was silently CONVERTED,` |
|       - | 2306 | ``				 * corrupting e.g. `$i = 5; $i[0]++` into array(0 => 6); string`` |
|       - | 2307 | `				 * bases were intercepted by the string-offset paths above). */` |
|       - | 2308 | `				/* php auto-converts false to an array with an 8.1 DEPRECATION; PHL` |
|       - | 2309 | `				 * rejects it like any other scalar base (null still auto-vivifies —` |
|       - | 2310 | `				 * it is not a bool). */` |
|      68 | 2311 | `				if( (pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_RES\|MEMOBJ_BOOL)) != 0 ){` |
|       - | 2312 | `					/* unset() has its own wording for the same base: php's` |
|       - | 2313 | `					 * "Cannot unset offset in a non-array variable". */` |
|      14 | 2314 | `					const char *zErr = VM_IDX_IS_UNSET(iP2)` |
|       - | 2315 | `						? "Cannot unset offset in a non-array variable"` |
|      12 | 2316 | `						: "Cannot use a scalar value as an array";` |
|       - | 2317 | `					SyBlob sErrMsg;` |
|      18 | 2318 | `					SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      18 | 2319 | `					SyBlobAppend(&sErrMsg,zErr,(sxu32)SyStrlen(zErr));` |
|      18 | 2320 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      18 | 2321 | `					if( pIdx ){` |
|      18 | 2322 | `						PH7_MemObjRelease(pIdx);` |
|       8 | 2323 | `					}` |
|      18 | 2324 | `					PH7_MemObjRelease(pTos);` |
|      18 | 2325 | `					pTos->nIdx = SXU32_HIGH;` |
|      18 | 2326 | `					VM_EXIT_BREAK;` |
|       - | 2327 | `				}` |
|       - | 2328 | `				/* unset() CREATES NOTHING: a null/undefined base stays null where PHL` |
|       - | 2329 | ``				 * converted it to an empty array (`$n = null; unset($n[0]);` left`` |
|       - | 2330 | `				 * $n === []), which is also what materialised the missing intermediate` |
|       - | 2331 | ``				 * in `unset($a["y"]["z"])`. Leaving the base alone, the lookup below`` |
|       - | 2332 | `				 * misses, the tail loads NULL with no slot index, and the trailing` |
|       - | 2333 | `				 * unset() builtin is the no-op php's is. */` |
|      52 | 2334 | `				if( !VM_IDX_IS_UNSET(iP2) ){` |
|      45 | 2335 | `					PH7_MemObjToHashmap(pObj);` |
|      45 | 2336 | `					if( pObj != pTos ){` |
|      45 | 2337 | `						PH7_MemObjLoad(pObj,pTos);` |
|      21 | 2338 | `					}` |
|      21 | 2339 | `				}` |
|      24 | 2340 | `			}` |
|       - | 2341 | `		}` |
|      24 | 2342 | `	}` |
|  256412 | 2343 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|       - | 2344 | `	/* php DEPRECATES both a null offset and a lossy-float subscript (then normalizes` |
|       - | 2345 | `	 * "" / truncates). PHL now matches php on the NULL offset (deprecate + coerce to` |
|       - | 2346 | `	 * the "" key, §2) but still rejects the lossy-FLOAT subscript with a TypeError on` |
|       - | 2347 | `	 * a READ or WRITE (iP2 0/1) — the recorded non-deprecated-surface policy. Both` |
|       - | 2348 | ``	 * stay lenient in isset()/empty()/`??`/unset(), where a throw would be wrong. */`` |
|       - | 2349 | `	/* An object/array key is rejected in EVERY context, including isset()/empty()/` |
|       - | 2350 | `	 * unset() where php still throws (only the wording changes) — unlike the` |
|       - | 2351 | `	 * null/float deprecations below, which stay lenient there. A resource key is` |
|       - | 2352 | `	 * accepted with a warning and becomes its integer id. */` |
|  256412 | 2353 | `	if( (pTos->iFlags & MEMOBJ_HASHMAP) && pIdx ){` |
|       - | 2354 | `		SyBlob sTypeMsg;` |
|  256296 | 2355 | `		if( VmOffsetTypeRejected(&(*pVm),pIdx,iP2,&sTypeMsg) ){` |
|       - | 2356 | `			/* Routed as a mid-expression throw, like the STRING arm above: this` |
|       - | 2357 | `			 * opcode is not a call boundary, so PARKING it let the rest of the` |
|       - | 2358 | ``			 * expression run first — `str_repeat($a[$obj], 2)` reached the builtin`` |
|       - | 2359 | `			 * with the NULL the abandoned read left and died on its ZPP TypeError` |
|       - | 2360 | `			 * instead, and a CAUGHT one still ran the enclosing call afterwards. */` |
|      22 | 2361 | `			rc = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|      22 | 2362 | `			PH7_MemObjRelease(pIdx);` |
|      22 | 2363 | `			PH7_MemObjRelease(pTos);` |
|      22 | 2364 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      22 | 2365 | `			pTos->nIdx = SXU32_HIGH;` |
|      31 | 2366 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      20 | 2367 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2368 | `		}` |
|  256276 | 2369 | `		PH7_VmOffsetResourceWarn(&(*pVm),pIdx);` |
|  128078 | 2370 | `	}` |
|  256392 | 2371 | `	if( (pTos->iFlags & MEMOBJ_HASHMAP) && pIdx ){` |
|       - | 2372 | `		/* php DEPRECATES a null offset in EVERY subscript context except unset()` |
|       - | 2373 | `		 * (iP2==5), then normalizes it to the "" key — read, write, isset (4),` |
|       - | 2374 | ``		 * empty (6), `??`/`??=` (3), the coalesce-chain probe (8), destructure`` |
|       - | 2375 | `		 * (2/7). Emit it here so all of them get it, not just read/write; the` |
|       - | 2376 | `		 * lookup/insert below casts NULL->"", and a plain read miss then warns` |
|       - | 2377 | ``		 * `Undefined array key ""` on the now-string pIdx (php's exact pair). */`` |
|  256276 | 2378 | `		if( (pIdx->iFlags & MEMOBJ_NULL) && !VM_IDX_IS_UNSET(iP2) ){` |
|      20 | 2379 | `			PH7_VmNullOffsetDeprecate(&(*pVm),pIdx);` |
|       9 | 2380 | `		}` |
|       - | 2381 | `		/* A lossy-FLOAT subscript stays a rejected TypeError on a READ or WRITE` |
|       - | 2382 | `		 * (iP2 0/1) — the recorded non-deprecated-surface policy (§2); the lenient` |
|       - | 2383 | `		 * contexts (isset/empty/??/unset) truncate quietly as php's value does. */` |
|  256271 | 2384 | `		if( (iP2 == 0 \|\| iP2 == 1)` |
|  172880 | 2385 | `		 && (pIdx->iFlags & MEMOBJ_REAL)` |
|  128088 | 2386 | `		 && pIdx->rVal != (ph7_real)(sxi64)pIdx->rVal ){` |
|       - | 2387 | `			SyBlob sErrMsg;` |
|       6 | 2388 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       6 | 2389 | `			SyBlobAppend(&sErrMsg,"Cannot access offset of type float on array",` |
|       - | 2390 | `				sizeof("Cannot access offset of type float on array")-1);` |
|       6 | 2391 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|       6 | 2392 | `			PH7_MemObjRelease(pIdx);` |
|       6 | 2393 | `			PH7_MemObjRelease(pTos);` |
|       6 | 2394 | `			pTos->nIdx = SXU32_HIGH;` |
|       6 | 2395 | `			VM_EXIT_BREAK;` |
|       - | 2396 | `		}` |
|  128076 | 2397 | `	}` |
|  256388 | 2398 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|  256300 | 2399 | `		if( iP2 == 1 \|\| VM_IDX_IS_UNSET(iP2) ){` |
|       - | 2400 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|       - | 2401 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|       - | 2402 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|       - | 2403 | `			 * NOT separate — that would defeat COW on every element read.` |
|       - | 2404 | `			 * iP2=5 is unset-context, treated like iP2=1 for arrays so the` |
|       - | 2405 | `			 * trailing unset() builtin can drop the slot via pTos->nIdx. */` |
|    5482 | 2406 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|    2742 | 2407 | `		}` |
|       - | 2408 | `		/* Point to the hashmap */` |
|  256300 | 2409 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|  256300 | 2410 | `		if( pIdx ){` |
|       - | 2411 | `			/* Load the desired entry */` |
|  256272 | 2412 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|  128076 | 2413 | `		}` |
|  256300 | 2414 | `		if( iP2 == 3 ){` |
|       - | 2415 | `			/* Null coalescing assign peek mode: separate only when we will` |
|       - | 2416 | `			 * actually write back. If the looked-up value is non-null, the` |
|       - | 2417 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|       - | 2418 | `			 * the parent can stay shared. If the value is null or the key is` |
|       - | 2419 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|       - | 2420 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|       - | 2421 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|       - | 2422 | `			 * correct for the outermost write. */` |
|      27 | 2423 | `			int needWrite = (rc != SXRET_OK);` |
|      27 | 2424 | `			if( !needWrite && pNode ){` |
|      13 | 2425 | `				ph7_value *pVal = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pNode->nValIdx);` |
|      13 | 2426 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|       7 | 2427 | `					needWrite = 1;` |
|       3 | 2428 | `				}` |
|       6 | 2429 | `			}` |
|      27 | 2430 | `			if( needWrite ){` |
|      21 | 2431 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      21 | 2432 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|       - | 2433 | `					/* The map was actually copied — re-lookup so pNode points` |
|       - | 2434 | `					 * into the new map's storage. */` |
|       7 | 2435 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       7 | 2436 | `					if( pIdx ){` |
|       7 | 2437 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|       3 | 2438 | `					}` |
|       3 | 2439 | `				}` |
|      10 | 2440 | `			}` |
|      13 | 2441 | `		}` |
|       - | 2442 | `		/* iP2 == 5 (unset) is deliberately NOT here: php's unset() never creates` |
|       - | 2443 | `		 * the key it is about to remove, so a MISS must stay a miss. The` |
|       - | 2444 | `		 * insert-then-unset round trip was invisible on the LAST step but left` |
|       - | 2445 | ``		 * every INTERMEDIATE behind -- `unset($a["y"]["z"])` grew an empty`` |
|       - | 2446 | `		 * $a["y"]. The COW separation the unset context needs happened above and` |
|       - | 2447 | `		 * does not depend on this insert. */` |
|  256300 | 2448 | `		if( bRmwFetch && rc != SXRET_OK ){` |
|       - | 2449 | `			/* php's read half missed. Record it BEFORE the vivification below` |
|       - | 2450 | `			 * overwrites rc — the warning is about what was not there to read, and` |
|       - | 2451 | `			 * it is emitted after the slot exists, exactly as php does it. */` |
|      45 | 2452 | `			bRmwMiss = 1;` |
|      22 | 2453 | `		}` |
|  256300 | 2454 | `		if( rc != SXRET_OK && (iP2 == 1 \|\| iP2 == 3) ){` |
|       - | 2455 | `			/* Create a new empty entry */` |
|     208 | 2456 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|     208 | 2457 | `			if( rc == SXRET_OK ){` |
|       - | 2458 | `				/* Point to the last inserted entry */` |
|     206 | 2459 | `				pNode = pMap->pLast;` |
|     105 | 2460 | `			}else{` |
|       - | 2461 | ``				/* An append lvalue (`$a[][...] = v`) whose saturated auto-index`` |
|       - | 2462 | `				 * is occupied threw php's catchable Error. Dispatch it here —` |
|       - | 2463 | `				 * falling through with a stale pMap->pLast is what silently` |
|       - | 2464 | `				 * overwrote $a[PHP_INT_MAX]. */` |
|       7 | 2465 | `				PH7_DISPATCH_ENFORCE_RC(rc)` |
|       - | 2466 | `			}` |
|     101 | 2467 | `		}` |
|  128089 | 2468 | `	}` |
|  256381 | 2469 | `	if( pIdx && (bRmwMiss \|\| (rc != SXRET_OK && (iP2 == 2 \|\| iP2 == 7 \|\| iP2 == 0)))` |
|   63510 | 2470 | `	 && (pTos->iFlags & MEMOBJ_HASHMAP)` |
|      88 | 2471 | `	 && !VmIdxFeedsCoalesce(pInstr) ){` |
|       - | 2472 | ``		/* `$a['k'] ?? $d` compiles its LHS as a plain read (iP2 == 0) followed`` |
|       - | 2473 | `		 * by NULLC/NULLC_JMP — php does NOT warn there, so peek ahead and stay` |
|       - | 2474 | `		 * silent (same guard the magic-accessor read path uses). */` |
|       - | 2475 | `		/* php warns when a missing key is READ (iP2 == 0), destructured -- both` |
|       - | 2476 | `		 * positionally (iP2 == 2) and by KEY (iP2 == 7, which used to stay silent` |
|       - | 2477 | ``		 * where php says `Undefined array key "k"` for `['k' => $v] = []`) -- or`` |
|       - | 2478 | `		 * read by the READ half of a read-modify-write (bRmwMiss).` |
|       - | 2479 | `		 * isset/empty/??/unset (iP2 3-6) and plain write-context` |
|       - | 2480 | `		 * vivification (iP2 == 1) stay silent, as does a read on a non-array` |
|       - | 2481 | `		 * base (already diagnosed above). php prints an INT key bare and a` |
|       - | 2482 | `		 * STRING key quoted, and it decides which one the key IS by the same fold` |
|       - | 2483 | ``		 * the lookup just used: `$a["10"]` looked for the INTEGER key 10, so php`` |
|       - | 2484 | ``		 * says `Undefined array key 10`. PHL rendered the operand as WRITTEN --`` |
|       - | 2485 | ``		 * quoting every string and, worse, printing `$a[false]` as the "" key it`` |
|       - | 2486 | `		 * never looked in (false is the integer key 0). The canonical-numeric rule` |
|       - | 2487 | `		 * is the hashmap's own, so ask it rather than re-derive it. */` |
|       - | 2488 | `		SyBlob sMsg;` |
|     127 | 2489 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     127 | 2490 | `		if( (ph7_hashmap *)pTos->x.pOther == pVm->pGlobal ){` |
|       - | 2491 | `			/* $GLOBALS is the symbol table, so a key that is not there is a VARIABLE` |
|       - | 2492 | ``			 * that is not there, and php says so: `Undefined global variable $x`,`` |
|       - | 2493 | ``			 * with the subscript spelled RAW after the `$` ($GLOBALS[5] reads`` |
|       - | 2494 | ``			 * `$5`) rather than folded and quoted the way an array key is. Only the`` |
|       - | 2495 | `			 * LIVE map takes this wording — a copy of $GLOBALS is a by-value` |
|       - | 2496 | `			 * snapshot (memobj.c) and warns as the ordinary array it is. */` |
|       - | 2497 | `			SyString sName;` |
|       5 | 2498 | `			if( (pIdx->iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 2499 | `				PH7_MemObjToString(pIdx);` |
|     ! 0 | 2500 | `			}` |
|       5 | 2501 | `			SyStringInitFromBuf(&sName,SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob));` |
|       5 | 2502 | `			SyBlobFormat(&sMsg,"Undefined global variable $%z",&sName);` |
|     125 | 2503 | `		}else if( PH7_HashmapKeyIsInt(pIdx) ){` |
|      48 | 2504 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      15 | 2505 | `				PH7_MemObjToInteger(pIdx);` |
|       7 | 2506 | `			}` |
|      48 | 2507 | `			SyBlobFormat(&sMsg,"Undefined array key %qd",pIdx->x.iVal);` |
|      25 | 2508 | `		}else{` |
|       - | 2509 | `			/* Not an integer key: PH7_HashmapKeyIsInt left it a printable string. */` |
|       - | 2510 | `			SyString sKey;` |
|      77 | 2511 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob));` |
|      77 | 2512 | `			SyBlobFormat(&sMsg,"Undefined array key \"%z\"",&sKey);` |
|       - | 2513 | `		}` |
|     127 | 2514 | `		SyBlobNullAppend(&sMsg);` |
|     127 | 2515 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|     127 | 2516 | `		SyBlobRelease(&sMsg);` |
|      61 | 2517 | `	}` |
|  256459 | 2518 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0` |
|  128255 | 2519 | `	 && iP2 == 0` |
|      30 | 2520 | `	 && !VmIdxFeedsCoalesce(pInstr) ){` |
|       - | 2521 | `		/* Subscripting a scalar base is a WARNING in php ("Trying to access array offset` |
|       - | 2522 | `		 * on int") that yields NULL. PH7 yielded NULL in silence. (NOT for iP2 == 2, the` |
|       - | 2523 | `		 * fetch a NESTED destructuring level makes: php never subscripts there -- its` |
|       - | 2524 | `		 * outer list already answered that position with NULL and warned once for it --` |
|       - | 2525 | `		 * so a second sentence about the same byte is one php does not print.) */` |
|      23 | 2526 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Trying to access array offset on %s",` |
|       7 | 2527 | `			VmArithValueName(pTos));` |
|       7 | 2528 | `	}` |
|  256329 | 2529 | `	if( iP2 == VM_IDX_CTX_UNSET && rc == SXRET_OK && pNode != 0` |
|    1129 | 2530 | `	 && (pTos->iFlags & MEMOBJ_HASHMAP) ){` |
|       - | 2531 | ``		/* php's `unset($a[k])` removes the ELEMENT. PH7 left the element's value slot`` |
|       - | 2532 | `		 * on the stack and let the trailing unset() builtin drop it — but dropping a` |
|       - | 2533 | `` 		 * SLOT unlinks everything that holds it, so `$r = &$a['k']; unset($a['k']);` `` |
|       - | 2534 | ``		 * destroyed $r too (`Undefined variable $r`) where php leaves it reading the`` |
|       - | 2535 | `		 * value it still refers to. Unlink the node itself, which releases the value` |
|       - | 2536 | `		 * only when this element was its last holder, and leave the builtin nothing. */` |
|    1129 | 2537 | `		ph7_hashmap *pTarget = (ph7_hashmap *)pTos->x.pOther;` |
|    1129 | 2538 | `		int bDone = 0;` |
|    1129 | 2539 | `		if( pTarget == pVm->pGlobal && pIdx ){` |
|       - | 2540 | ``			/* `$GLOBALS['x']` IS the global $x, so this is an unset of the NAME: it has`` |
|       - | 2541 | `			 * to drop the symbol-table entry as well as this node, and it must not` |
|       - | 2542 | `			 * destroy the value another holder still refers to — exactly what` |
|       - | 2543 | ``			 * VmUnsetVarByName does for `unset($x)`. A superglobal has no entry in the`` |
|       - | 2544 | `			 * global frame and falls through to the plain node unlink below. */` |
|     159 | 2545 | `			VmFrame *pGlobalFrame = pVm->pFrame;` |
|       - | 2546 | `			SyHashEntry *pNameEntry;` |
|     159 | 2547 | `			while( pGlobalFrame->pParent ){` |
|     ! 0 | 2548 | `				pGlobalFrame = pGlobalFrame->pParent;` |
|     ! 0 | 2549 | `			}` |
|     159 | 2550 | `			if( (pIdx->iFlags & MEMOBJ_STRING) == 0 ){` |
|       3 | 2551 | `				PH7_MemObjToString(pIdx);` |
|       1 | 2552 | `			}` |
|     238 | 2553 | `			pNameEntry = SyHashGet(&pGlobalFrame->hVar,` |
|     158 | 2554 | `				(const void *)SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob));` |
|     159 | 2555 | `			if( pNameEntry ){` |
|     238 | 2556 | `				sxi32 rcUnset = VmUnsetVarByNameEx(&(*pVm),pGlobalFrame,` |
|     158 | 2557 | `					(const char *)SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob),FALSE);` |
|     159 | 2558 | `				bDone = 1;` |
|     159 | 2559 | `				if( rcUnset == PH7_ABORT ){` |
|     ! 0 | 2560 | `					PH7_MemObjRelease(pIdx);` |
|     ! 0 | 2561 | `					VM_EXIT_ABORT;` |
|       - | 2562 | `				}` |
|      79 | 2563 | `			}` |
|      79 | 2564 | `		}` |
|    1129 | 2565 | `		if( !bDone ){` |
|     971 | 2566 | `			PH7_HashmapUnlinkNode(pNode,TRUE);` |
|     483 | 2567 | `		}` |
|    1129 | 2568 | `		if( pIdx ){` |
|    1129 | 2569 | `			PH7_MemObjRelease(pIdx);` |
|     562 | 2570 | `		}` |
|    1129 | 2571 | `		PH7_MemObjRelease(pTos);` |
|    1129 | 2572 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|    1129 | 2573 | `		pTos->nIdx = SXU32_HIGH;` |
|    1129 | 2574 | `		VM_EXIT_BREAK;` |
|       - | 2575 | `	}` |
|  255210 | 2576 | `	if( pIdx ){` |
|  255184 | 2577 | `		PH7_MemObjRelease(pIdx);` |
|  127532 | 2578 | `	}` |
|  255210 | 2579 | `	if( rc == SXRET_OK ){` |
|       - | 2580 | `		/* Load entry contents */` |
|  128324 | 2581 | `		if( pMap->iRef < 2 ){` |
|       - | 2582 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|       - | 2583 | `			 * of the entry value,rather than pointing to it.` |
|       - | 2584 | `			 */` |
|     852 | 2585 | `			pTos->nIdx = SXU32_HIGH;` |
|     852 | 2586 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|     426 | 2587 | `		}else{` |
|  127477 | 2588 | `			pTos->nIdx = pNode->nValIdx;` |
|  127477 | 2589 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|  127477 | 2590 | `			PH7_HashmapUnref(pMap);` |
|       - | 2591 | `		}` |
|   64128 | 2592 | `	}else{` |
|       - | 2593 | `		/* No such entry,load NULL */` |
|  126891 | 2594 | `		PH7_MemObjRelease(pTos);` |
|  126891 | 2595 | `		pTos->nIdx = SXU32_HIGH;` |
|       - | 2596 | `	}` |
|  255210 | 2597 | `	if( iP2 == 4 && (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       - | 2598 | `		/* isset() context: reduce a found element to the same non-null marker the` |
|       - | 2599 | `		 * ArrayAccess arm above pushes. A TEMPORARY array (a call's return value,` |
|       - | 2600 | ``		 * or an accessor's -- `isset(f()['k'])`, `isset($o->magic['k'])`) leaves no`` |
|       - | 2601 | `		 * variable index behind, and the trailing builtin read that as a CONSTANT` |
|       - | 2602 | `		 * and warned about it; php's isset() is a language construct with no such` |
|       - | 2603 | `		 * diagnostic. */` |
|   37156 | 2604 | `		PH7_MemObjRelease(pTos);` |
|   37156 | 2605 | `		pTos->x.iVal = 1;` |
|   37156 | 2606 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   37156 | 2607 | `		pTos->nIdx = SXU32_HIGH;` |
|   18572 | 2608 | `	}` |
|  255210 | 2609 | `	VM_EXIT_BREAK;` |
|     ! 0 | 2610 | `	VM_EXIT_BREAK;` |
|  631689 | 2611 | `}` |
|       - | 2612 |  |
|       - | 2613 | `/*` |
|       - | 2614 | ` * OP_LOAD_MAP: body moved verbatim from the OP_LOAD_MAP arm of` |
|       - | 2615 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 2616 | ` */` |
|  133090 | 2617 | `PH7_PRIVATE VmOpRc VmExecOpLoadMap(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 2618 | `{` |
|  133095 | 2619 | `	ph7_value *pTos = pState->pTos;` |
|  133095 | 2620 | `	ph7_value *pStack = pState->pStack;` |
|  133095 | 2621 | `	VmInstr *aInstr = pState->aInstr;` |
|  133095 | 2622 | `	sxi32 pc = pState->pc;` |
|       - | 2623 | `	sxi32 rc;` |
|   66476 | 2624 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - | 2625 | `	ph7_hashmap *pMap;` |
|       - | 2626 | `	/* Allocate a new hashmap instance */` |
|  133095 | 2627 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|  133095 | 2628 | `	if( pMap == 0 ){` |
|     ! 0 | 2629 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|     ! 0 | 2630 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|     ! 0 | 2631 | `		VM_EXIT_ABORT;` |
|       - | 2632 | `	}` |
|  133095 | 2633 | `	if( pInstr->iP1 > 0 ){` |
|   35993 | 2634 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|   35993 | 2635 | `		sxi32 rcSpread = SXRET_OK;` |
|       - | 2636 | `		/* Perform the insertion */` |
|  127632 | 2637 | `		while( pEntry < pTos ){` |
|   91690 | 2638 | `			if( pEntry[1].iFlags & MEMOBJ_AUX_SPREAD ){` |
|       - | 2639 | `				/* Array unpacking: '...$expr'. Merge entries with PHP 8.1` |
|       - | 2640 | `				 * semantics — string keys preserved (later wins), int keys` |
|       - | 2641 | `				 * renumbered. Same routine that backs array_merge. */` |
|     728 | 2642 | `				if( pEntry[1].iFlags & MEMOBJ_HASHMAP ){` |
|     667 | 2643 | `					sxi32 rcMerge = PH7_HashmapMerge((ph7_hashmap *)pEntry[1].x.pOther,pMap);` |
|     667 | 2644 | `					if( rcMerge != SXRET_OK ){` |
|       - | 2645 | `						/* Merge failure (OOM): match the PH7_NewHashmap OOM` |
|       - | 2646 | `						 * path — emit fatal and abort, leaving no partial` |
|       - | 2647 | `						 * map dangling. */` |
|     ! 0 | 2648 | `						VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|     ! 0 | 2649 | `							"Fatal, PH7 engine is running out of memory while spreading array at instruction #:%d",pc);` |
|     ! 0 | 2650 | `						rcSpread = PH7_ABORT;` |
|     ! 0 | 2651 | `						break;` |
|       3 | 2652 | `					}` |
|     395 | 2653 | `				}else if( VmValueIsTraversable(pVm,&pEntry[1]) ){` |
|       - | 2654 | `					/* Traversable unpacking (PHP 8.1): walk it into the map using the` |
|       - | 2655 | `					 * same key rules as array spread (string keys kept, int renumbered). */` |
|      32 | 2656 | `					sxi32 rcW = PH7_VmIteratorWalk(&(*pVm),&pEntry[1],VmSpreadMergeStep,pMap);` |
|      32 | 2657 | `					if( rcW == PH7_EXCEPTION \|\| rcW == PH7_ABORT ){` |
|      18 | 2658 | `						rcSpread = rcW;` |
|      18 | 2659 | `						break;` |
|       - | 2660 | `					}` |
|       9 | 2661 | `				}else{` |
|       - | 2662 | `					/* Throw a catchable Error matching PHP semantics. */` |
|      33 | 2663 | `					rcSpread = VmThrowSpreadError(&(*pVm),&pEntry[1],0);` |
|      33 | 2664 | `					break;` |
|       3 | 2665 | `				}` |
|   91305 | 2666 | `			}else if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|       - | 2667 | `				/* Insertion by reference */` |
|     247 | 2668 | `				PH7_HashmapInsertByRef(pMap,` |
|     164 | 2669 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     164 | 2670 | `					(sxu32)pEntry[1].x.iVal` |
|       - | 2671 | `					);` |
|      83 | 2672 | `			}else{` |
|       - | 2673 | `				/* An explicit key in an array LITERAL gets the same php diagnostics a` |
|       - | 2674 | `				 * subscript does — a float key that truncates deprecates, and so does an` |
|       - | 2675 | `				 * explicit null key. Only the subscript sites (LOAD_IDX/STORE_IDX) used to` |
|       - | 2676 | ``				 * emit these, so `[1.5 => "v"]` and `[null => "v"]` were silent. A key that`` |
|       - | 2677 | `				 * is ABSENT (auto-index) is a NULL slot here, not a null key, so the` |
|       - | 2678 | `				 * MEMOBJ_NULL check below is what tells the two apart. */` |
|   90802 | 2679 | `					if( (pEntry->iFlags & MEMOBJ_AUX_NOKEY) == 0 ){` |
|       - | 2680 | `						/* An object/array literal key is php's TypeError, a resource one` |
|       - | 2681 | `						 * warns and becomes its id — same rules as a subscript. */` |
|       - | 2682 | `						SyBlob sTypeMsg;` |
|   20036 | 2683 | `						if( VmOffsetTypeRejected(&(*pVm),pEntry,0,&sTypeMsg) ){` |
|       3 | 2684 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg));` |
|       2 | 2685 | `						}else{` |
|   20034 | 2686 | `							PH7_VmOffsetResourceWarn(&(*pVm),pEntry);` |
|       - | 2687 | `						}` |
|       - | 2688 | `						/* php DEPRECATES a null literal key (then normalizes to "") and` |
|       - | 2689 | `						 * rejects nothing there; PHL matches that (deprecate + fall` |
|       - | 2690 | `						 * through, PH7_HashmapInsert casts NULL->""). A lossy-FLOAT` |
|       - | 2691 | `						 * literal key still rejects with a TypeError — the recorded` |
|       - | 2692 | `						 * non-deprecated-surface policy, same as the subscript site. */` |
|   20036 | 2693 | `						int bNull = (pEntry->iFlags & MEMOBJ_NULL) != 0;` |
|   30081 | 2694 | `						int bLossyFloat = (pEntry->iFlags & MEMOBJ_REAL) != 0` |
|   20031 | 2695 | `							&& pEntry->rVal != (ph7_real)(sxi64)pEntry->rVal;` |
|   20036 | 2696 | `						if( bNull ){` |
|       3 | 2697 | `							PH7_VmNullOffsetDeprecate(&(*pVm),pEntry);` |
|   20035 | 2698 | `						}else if( bLossyFloat ){` |
|       3 | 2699 | `							const char *zErr = "Cannot access offset of type float on array";` |
|       - | 2700 | `							SyBlob sErrMsg;` |
|       3 | 2701 | `							SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       3 | 2702 | `							SyBlobAppend(&sErrMsg,zErr,(sxu32)SyStrlen(zErr));` |
|       3 | 2703 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|       1 | 2704 | `						}` |
|    9987 | 2705 | `					}` |
|       - | 2706 | `				/* Standard insertion */` |
|  135870 | 2707 | `				PH7_HashmapInsert(pMap,` |
|   90797 | 2708 | `					(pEntry->iFlags & MEMOBJ_AUX_NOKEY) ? 0 /* Automatic index assign */ : pEntry,` |
|   45068 | 2709 | `					&pEntry[1]` |
|       - | 2710 | `				);` |
|       - | 2711 | `			}` |
|       - | 2712 | `			/* Next pair on the stack */` |
|   91644 | 2713 | `			pEntry += 2;` |
|       5 | 2714 | `		}` |
|       - | 2715 | `		/* Pop P1 elements */` |
|   35993 | 2716 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|   35993 | 2717 | `		if( rcSpread != SXRET_OK ){` |
|       - | 2718 | `			/* Discard the partially-built map and propagate the exception. */` |
|      49 | 2719 | `			PH7_HashmapRelease(pMap,TRUE);` |
|      49 | 2720 | `			if( rcSpread == PH7_ABORT ){` |
|     ! 0 | 2721 | `				VM_EXIT_ABORT;` |
|       - | 2722 | `			}` |
|       - | 2723 | `			{` |
|       - | 2724 | `				sxi32 iRp;` |
|      49 | 2725 | `				if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|       6 | 2726 | `					pc = iRp;` |
|       6 | 2727 | `					VM_EXIT_BREAK;` |
|       - | 2728 | `				}` |
|       - | 2729 | `			}` |
|      45 | 2730 | `			VM_EXIT_EXCEPTION;` |
|       - | 2731 | `		}` |
|   17912 | 2732 | `	}` |
|       - | 2733 | `	/* Push the hashmap */` |
|  133049 | 2734 | `	pTos++;` |
|  133049 | 2735 | `	pTos->nIdx = SXU32_HIGH;` |
|  133049 | 2736 | `	pTos->x.pOther = pMap;` |
|  133049 | 2737 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|  133049 | 2738 | `	VM_EXIT_BREAK;` |
|     ! 0 | 2739 | `	VM_EXIT_BREAK;` |
|   66481 | 2740 | `}` |
|       - | 2741 |  |
|       - | 2742 | `/*` |
|       - | 2743 | `` * Can `$o[$k]` be READ at all? php's read_dimension is either the class's own`` |
|       - | 2744 | ` * native handler -- which a class may carry WITHOUT implementing ArrayAccess,` |
|       - | 2745 | ` * php's DOMNodeList -- or the standard one, which needs the interface. Neither` |
|       - | 2746 | `` * is php's `Cannot use object of type C as array`.`` |
|       - | 2747 | ` */` |
|      30 | 2748 | `static int VmObjectDimReadable(ph7_vm *pVm,ph7_class_instance *pInst)` |
|       1 | 2749 | `{` |
|      31 | 2750 | `	if( pInst == 0 ){` |
|     ! 0 | 2751 | `		return 0;` |
|       - | 2752 | `	}` |
|      45 | 2753 | `	return PH7_ClassHasNativeDim(pInst->pClass)` |
|      30 | 2754 | `	    \|\| (pVm->pArrayAccessClass && PH7_VmInstanceOf(pInst->pClass,pVm->pArrayAccessClass));` |
|      16 | 2755 | `}` |
|       - | 2756 | `/*` |
|       - | 2757 | ` * One such READ, into pOut (which the caller inits and owns). The native` |
|       - | 2758 | ` * handler comes first for the same reason it does at the subscript opcode: php` |
|       - | 2759 | ` * implements the interface THROUGH the handler. A refusal is dropped here --` |
|       - | 2760 | ` * the only caller indexes 0..N-1 of its own target list, which no handler` |
|       - | 2761 | ` * refuses -- and pOut is simply left as it was.` |
|       - | 2762 | ` *` |
|       - | 2763 | ` * Answers the accessor's own status so a caller reading a RUN of positions can` |
|       - | 2764 | ` * stop where php stops: a userland offsetGet that THROWS abandons the rest of` |
|       - | 2765 | ` * the destructure, leaving every later target at its previous value.` |
|       - | 2766 | ` */` |
|      36 | 2767 | `static sxi32 VmObjectDimRead(ph7_vm *pVm,ph7_class_instance *pInst,ph7_value *pKey,ph7_value *pOut)` |
|       1 | 2768 | `{` |
|       - | 2769 | `	ph7_class_method *pGet;` |
|       - | 2770 | `	sxi32 rcCall;` |
|      37 | 2771 | `	if( PH7_ClassHasNativeDim(pInst->pClass) ){` |
|       - | 2772 | `		PH7_NativeDimCtx sDim;` |
|       5 | 2773 | `		sDim.iMode = PH7_NATIVE_DIM_READ;` |
|       5 | 2774 | `		sDim.pOffset = pKey;` |
|       5 | 2775 | `		sDim.pResult = pOut;` |
|       5 | 2776 | `		sDim.zThrowClass = 0;` |
|       5 | 2777 | `		sDim.zThrowMsg[0] = 0;` |
|       5 | 2778 | `		sDim.bStored = 0;` |
|       5 | 2779 | `		PH7_ClassNativeDim(pInst,&sDim);` |
|       5 | 2780 | `		return SXRET_OK;` |
|       - | 2781 | `	}` |
|      33 | 2782 | `	pGet = PH7_ClassExtractMethod(pInst->pClass,"offsetGet",sizeof("offsetGet")-1);` |
|      33 | 2783 | `	if( pGet == 0 ){` |
|     ! 0 | 2784 | `		return SXRET_OK;` |
|       - | 2785 | `	}` |
|       - | 2786 | `	{` |
|       - | 2787 | `		ph7_value *apArg[1];` |
|      33 | 2788 | `		apArg[0] = pKey;` |
|      33 | 2789 | `		rcCall = PH7_VmCallClassMethod(&(*pVm),pInst,pGet,pOut,1,apArg);` |
|       - | 2790 | `	}` |
|      33 | 2791 | `	return (rcCall == PH7_EXCEPTION \|\| pVm->nBoundaryRc != 0) ? PH7_EXCEPTION : SXRET_OK;` |
|      19 | 2792 | `}` |
|       - | 2793 | `/*` |
|       - | 2794 | ` * OP_LOAD_LIST: body moved verbatim from the OP_LOAD_LIST arm of` |
|       - | 2795 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 2796 | ` */` |
|  392899 | 2797 | `PH7_PRIVATE VmOpRc VmExecOpLoadList(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 2798 | `{` |
|  392904 | 2799 | `	ph7_value *pTos = pState->pTos;` |
|  392904 | 2800 | `	ph7_value *pStack = pState->pStack;` |
|  392904 | 2801 | `	VmInstr *aInstr = pState->aInstr;` |
|  392904 | 2802 | `	sxi32 pc = pState->pc;` |
|       - | 2803 | `	sxi32 rc;` |
|  196446 | 2804 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - | 2805 | `	ph7_value *pEntry;` |
|  392904 | 2806 | `	sxi32 rcEnforce = SXRET_OK;` |
|  392904 | 2807 | `	if( pInstr->iP1 <= 0 ){` |
|       - | 2808 | `		/* Empty list,break immediately */` |
|     ! 0 | 2809 | `		VM_EXIT_BREAK;` |
|       - | 2810 | `	}` |
|  392904 | 2811 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|       - | 2812 | `#ifdef UNTRUST` |
|       - | 2813 | `	if( &pEntry[-1] < pStack ){` |
|       - | 2814 | `		VM_EXIT_ABORT;` |
|       - | 2815 | `	}` |
|       - | 2816 | `#endif` |
|  392904 | 2817 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|  392840 | 2818 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|       - | 2819 | `		ph7_hashmap_node *pNode;` |
|       - | 2820 | `		ph7_value sKey,*pObj;` |
|       - | 2821 | `		/* Start Copying */` |
|  392840 | 2822 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
| 1568962 | 2823 | `		while( pEntry <= pTos ){` |
| 1176145 | 2824 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
| 1176115 | 2825 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
| 1176115 | 2826 | `				if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
| 1176115 | 2827 | `					int bTyped = PH7_VM_STORE_FILTERED(pVm,pEntry->nIdx);` |
| 1176115 | 2828 | `					if( rc != SXRET_OK ){` |
|       - | 2829 | `						/* Undefined array key */` |
|       - | 2830 | `						char zMsg[128];` |
|       8 | 2831 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|       8 | 2832 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|       3 | 2833 | `					}` |
| 1176115 | 2834 | `					if( !bTyped ){` |
| 1176079 | 2835 | `						if( rc == SXRET_OK ){` |
|       - | 2836 | `							/* Store node value */` |
| 1176077 | 2837 | `							PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|  588034 | 2838 | `						}else{` |
|       3 | 2839 | `							PH7_MemObjRelease(pObj);` |
|       - | 2840 | `						}` |
|  588035 | 2841 | `					}else{` |
|       - | 2842 | ``						/* Typed/readonly property target (`[$o->p] = [...]`): a`` |
|       - | 2843 | `						 * direct slot write would bypass the typed-slot table, so` |
|       - | 2844 | `						 * enforce on a temp first — a TypeError leaves the property` |
|       - | 2845 | `						 * untouched, and a missing key assigns null, which a` |
|       - | 2846 | `						 * non-nullable type rejects exactly like php (warning, then` |
|       - | 2847 | `						 * "Cannot assign null to property ... of type ..."). */` |
|       - | 2848 | `						ph7_value sVal;` |
|      38 | 2849 | `						PH7_MemObjInit(&(*pVm),&sVal);` |
|      38 | 2850 | `						if( rc == SXRET_OK ){` |
|      34 | 2851 | `							PH7_HashmapExtractNodeValue(pNode,&sVal,TRUE);` |
|      16 | 2852 | `						}` |
|      38 | 2853 | `						rcEnforce = VmEnforcePropertyTypeOnStore(&(*pVm),pEntry->nIdx,&sVal,0);` |
|      38 | 2854 | `						if( rcEnforce != SXRET_OK ){` |
|       - | 2855 | `							/* Thrown: stop assigning (php aborts the list at the` |
|       - | 2856 | `							 * first failing element), settle the stack, route. */` |
|      20 | 2857 | `							PH7_MemObjRelease(&sVal);` |
|      20 | 2858 | `							break;` |
|       - | 2859 | `						}` |
|      20 | 2860 | `						PH7_MemObjStore(&sVal,pObj);` |
|      20 | 2861 | `						PH7_MemObjRelease(&sVal);` |
|       - | 2862 | `					}` |
|  588039 | 2863 | `				}` |
|  588039 | 2864 | `			}` |
| 1176127 | 2865 | `			sKey.x.iVal++; /* Next numeric index */` |
| 1176127 | 2866 | `			pEntry++;` |
|       5 | 2867 | `		}` |
|  196492 | 2868 | `	}else if( (pEntry[-1].iFlags & MEMOBJ_OBJ) && pEntry[-1].x.pOther ){` |
|       - | 2869 | `		/* php destructures an OBJECT through its read_dimension handler, one` |
|       - | 2870 | ``		 * READ per POSITION -- `[$a, , $c] = $o` asks for 0 and 2 and never 1 --`` |
|       - | 2871 | `		 * so an ArrayObject, an SplFixedArray and (since the handler landed) a` |
|       - | 2872 | `		 * DOMNodeList all come apart the way an array does. PHL treated every` |
|       - | 2873 | `` 		 * object as a non-array source: it warned `Cannot use object as array` `` |
|       - | 2874 | ``		 * and assigned NULL to every target, so `[$first, $second] = $list` --`` |
|       - | 2875 | `		 * the shape every modern DOM and SPL example is written in -- silently` |
|       - | 2876 | `		 * produced two nulls. An object with NO dimension reader is php's` |
|       - | 2877 | `		 * catchable Error rather than that warning, and it is raised before any` |
|       - | 2878 | ``		 * target is touched. (The KEYED spelling `['k' => $a] = $o` never came`` |
|       - | 2879 | `		 * here: the compiler routes it through OP_LOAD_IDX, which has had the` |
|       - | 2880 | `		 * accessor dispatch all along.) */` |
|      31 | 2881 | `		ph7_class_instance *pInst = (ph7_class_instance *)pEntry[-1].x.pOther;` |
|       - | 2882 | `		ph7_value sKey;` |
|      31 | 2883 | `		if( !VmObjectDimReadable(&(*pVm),pInst) ){` |
|       - | 2884 | `			/* Routed mid-expression, like every other catchable Error raised from` |
|       - | 2885 | `			 * an opcode that is not a call boundary: the destructure is abandoned` |
|       - | 2886 | `			 * and an enclosing try in THIS frame lands on its own handler. Settle` |
|       - | 2887 | `			 * the targets AND the source first — the statement's OP_POP is skipped` |
|       - | 2888 | `			 * when a catch resumes at the landing pad. */` |
|       - | 2889 | `			char zMsg[256];` |
|      13 | 2890 | `			SyString *pName = &pInst->pClass->sName;` |
|      19 | 2891 | `			sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - | 2892 | `				"Cannot use object of type %.*s as array",` |
|      12 | 2893 | `				(int)pName->nByte,pName->zString);` |
|      13 | 2894 | `			rc = VmThrowFromVm(&(*pVm),"Error",zMsg,nMsg);` |
|      13 | 2895 | `			VmPopOperand(&pTos,pInstr->iP1 + 1);` |
|      13 | 2896 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      13 | 2897 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|     ! 0 | 2898 | `		}else{` |
|      19 | 2899 | `			PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      59 | 2900 | `			while( pEntry <= pTos ){` |
|      43 | 2901 | `				if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */ ){` |
|      37 | 2902 | `					sxu32 nSlot = pEntry->nIdx;` |
|      37 | 2903 | `					int bTyped = PH7_VM_STORE_FILTERED(pVm,nSlot);` |
|       - | 2904 | `					ph7_value sVal,*pObj;` |
|      37 | 2905 | `					PH7_MemObjInit(&(*pVm),&sVal);` |
|      37 | 2906 | `					if( VmObjectDimRead(&(*pVm),pInst,&sKey,&sVal) != SXRET_OK ){` |
|       - | 2907 | `						/* The accessor threw: php abandons the destructure there,` |
|       - | 2908 | `						 * so every later target keeps the value it had -- and this` |
|       - | 2909 | `						 * target does too, since php assigns nothing for the read` |
|       - | 2910 | `						 * that failed. */` |
|       3 | 2911 | `						PH7_MemObjRelease(&sVal);` |
|       3 | 2912 | `						rcEnforce = PH7_EXCEPTION;` |
|       3 | 2913 | `						break;` |
|       - | 2914 | `					}` |
|      35 | 2915 | `					if( bTyped ){` |
|       - | 2916 | `						/* Same rule as the array source's typed target: enforce on` |
|       - | 2917 | `						 * the temp so a TypeError leaves the property untouched. */` |
|     ! 0 | 2918 | `						rcEnforce = VmEnforcePropertyTypeOnStore(&(*pVm),nSlot,&sVal,0);` |
|     ! 0 | 2919 | `						if( rcEnforce != SXRET_OK ){` |
|     ! 0 | 2920 | `							PH7_MemObjRelease(&sVal);` |
|     ! 0 | 2921 | `							break;` |
|       - | 2922 | `						}` |
|     ! 0 | 2923 | `					}` |
|       - | 2924 | `					/* Re-fetch AFTER the read: a userland offsetGet can reserve` |
|       - | 2925 | `					 * slots, which used to relocate every pointer into the pool.` |
|       - | 2926 | `					 * Redundant since P1 (fixed segments); left for the harvest` |
|       - | 2927 | `					 * sweep (PERF.md P1). */` |
|      35 | 2928 | `					pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nSlot);` |
|      35 | 2929 | `					if( pObj ){` |
|      35 | 2930 | `						PH7_MemObjStore(&sVal,pObj);` |
|      17 | 2931 | `					}` |
|      35 | 2932 | `					PH7_MemObjRelease(&sVal);` |
|      17 | 2933 | `				}` |
|      41 | 2934 | `				sKey.x.iVal++; /* Next numeric index */` |
|      41 | 2935 | `				pEntry++;` |
|       1 | 2936 | `			}` |
|      19 | 2937 | `			PH7_MemObjRelease(&sKey);` |
|       - | 2938 | `		}` |
|      10 | 2939 | `	}else{` |
|       - | 2940 | `		/* Source is not an array: php warns first (silencing ONLY null — a bool` |
|       - | 2941 | `		 * source warns too, php 8), then assigns null to every target. A typed` |
|       - | 2942 | `		 * property target receives that null THROUGH enforcement, so a` |
|       - | 2943 | `		 * non-nullable type throws "Cannot assign null to property ..." exactly` |
|       - | 2944 | `		 * like php instead of silently nulling the slot. PHL DIVERGENCE: bool` |
|       - | 2945 | `		 * FALSE stays silent (php warns) — it is the end-of-array sentinel of` |
|       - | 2946 | `` 		 * the documented each() extension's `while (list(..) = each($a))` `` |
|       - | 2947 | `		 * idiom, which would otherwise warn on every normal loop exit. */` |
|       - | 2948 | `		ph7_value *pObj;` |
|      56 | 2949 | `		int bFalseSrc = (pTos[-pInstr->iP1].iFlags & MEMOBJ_BOOL) != 0` |
|      34 | 2950 | `			&& pTos[-pInstr->iP1].x.iVal == 0;` |
|      32 | 2951 | `		sxi32 nWarn = (pTos[-pInstr->iP1].iFlags & MEMOBJ_NULL) == 0 && !bFalseSrc` |
|      41 | 2952 | `			? pInstr->iP2 : 0;` |
|       - | 2953 | `		/* php asks the non-array source for each POSITION it means to fill, so the` |
|       - | 2954 | `		 * warning is per ENTRY and not one for the whole list, which is what this` |
|       - | 2955 | `		 * used to raise. An EMPTY slot fills nothing and is not counted -- P2 carries` |
|       - | 2956 | `		 * the count the compiler made. */` |
|      70 | 2957 | `		while( nWarn > 0 ){` |
|      36 | 2958 | `			VmWarnCannotUseAsArray(&(*pVm),pTos[-pInstr->iP1].iFlags);` |
|      36 | 2959 | `			nWarn--;` |
|       2 | 2960 | `		}` |
|      84 | 2961 | `		while( pEntry <= pTos ){` |
|      56 | 2962 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|      52 | 2963 | `				if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|      52 | 2964 | `					int bTyped = PH7_VM_STORE_FILTERED(pVm,pEntry->nIdx);` |
|      52 | 2965 | `					if( !bTyped ){` |
|      44 | 2966 | `						PH7_MemObjRelease(pObj);` |
|      23 | 2967 | `					}else{` |
|       - | 2968 | `						ph7_value sVal;` |
|       9 | 2969 | `						PH7_MemObjInit(&(*pVm),&sVal);` |
|       9 | 2970 | `						rcEnforce = VmEnforcePropertyTypeOnStore(&(*pVm),pEntry->nIdx,&sVal,0);` |
|       9 | 2971 | `						if( rcEnforce != SXRET_OK ){` |
|       7 | 2972 | `							PH7_MemObjRelease(&sVal);` |
|       7 | 2973 | `							break;` |
|       - | 2974 | `						}` |
|       3 | 2975 | `						PH7_MemObjStore(&sVal,pObj);` |
|       3 | 2976 | `						PH7_MemObjRelease(&sVal);` |
|       - | 2977 | `					}` |
|      22 | 2978 | `				}` |
|      22 | 2979 | `			}` |
|      50 | 2980 | `			pEntry++;` |
|       2 | 2981 | `		}` |
|       - | 2982 | `	}` |
|  392892 | 2983 | `	if( rcEnforce != SXRET_OK ){` |
|       - | 2984 | `		/* Settle this op's own operands: the P1 entries AND the source value —` |
|       - | 2985 | `		 * its statement-level OP_POP is skipped when a catch resumes at the` |
|       - | 2986 | `		 * landing pad, so leaving it would leak one operand slot per caught` |
|       - | 2987 | `		 * throw. A NESTED destructure can still have the outer list's operands` |
|       - | 2988 | `		 * abandoned above the try's base, so on an in-place catch drain to the` |
|       - | 2989 | `		 * catching try's recorded depth (like the fetch-point router and the` |
|       - | 2990 | `		 * generator inject path), not just our own pops. */` |
|      28 | 2991 | `		VmPopOperand(&pTos,pInstr->iP1 + 1);` |
|      28 | 2992 | `		if( rcEnforce == PH7_ABORT ){` |
|     ! 0 | 2993 | `			VM_EXIT_ABORT;` |
|       - | 2994 | `		}` |
|       - | 2995 | `		{` |
|       - | 2996 | `			sxi32 _iRpL;` |
|      40 | 2997 | `			PH7_INLINE_RESUME_BREAK()` |
|      28 | 2998 | `			if( VmRecordedResume(pVm,&_iRpL,pState->pEntryFrame,aInstr) ){` |
|      26 | 2999 | `				PH7_RESUME_DRAIN()` |
|      26 | 3000 | `				pc = _iRpL;` |
|      26 | 3001 | `				VM_EXIT_BREAK;` |
|       - | 3002 | `			}` |
|       - | 3003 | `		}` |
|       3 | 3004 | `		VM_EXIT_EXCEPTION;` |
|       - | 3005 | `	}` |
|  392866 | 3006 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|  392866 | 3007 | `	VM_EXIT_BREAK;` |
|     ! 0 | 3008 | `	VM_EXIT_BREAK;` |
|  196451 | 3009 | `}` |
|       - | 3010 |  |
|       - | 3011 | `/*` |
|       - | 3012 | ` * OP_UNSET_VAR: body moved verbatim from the OP_UNSET_VAR arm of` |
|       - | 3013 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 3014 | ` */` |
|    8772 | 3015 | `PH7_PRIVATE VmOpRc VmExecOpUnsetVar(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 3016 | `{` |
|    8777 | 3017 | `	ph7_value *pTos = pState->pTos;` |
|    8777 | 3018 | `	ph7_value *pStack = pState->pStack;` |
|    8777 | 3019 | `	VmInstr *aInstr = pState->aInstr;` |
|    8777 | 3020 | `	sxi32 pc = pState->pc;` |
|       - | 3021 | `	sxi32 rc;` |
|    4385 | 3022 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - | 3023 | `	/* unset($name): p3 is the variable name. Drops the NAME only — see VmUnsetVarByName */` |
|    8777 | 3024 | `	SyString *pName = (SyString *)pInstr->p3;` |
|    8777 | 3025 | `	if( pName && pVm->pFrame ){` |
|       - | 3026 | `		/* Inside a try{} the VM pushes an EXCEPTION frame; variables live in the body` |
|       - | 3027 | `		 * frame below it, so skip past it exactly as every other variable path does.` |
|       - | 3028 | `		 * Without this, unset($x) inside a try silently found nothing and did nothing. */` |
|    8777 | 3029 | `		VmFrame *pVarFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|    8777 | 3030 | `		sxi32 rcU = VmUnsetVarByName(&(*pVm),pVarFrame,pName->zString,pName->nByte);` |
|    8777 | 3031 | `		if( rcU == PH7_ABORT ){` |
|       3 | 3032 | `			VM_EXIT_ABORT;` |
|       - | 3033 | `		}` |
|       - | 3034 | `		/* Releasing the last holder can run a __destruct(), and that destructor may` |
|       - | 3035 | `		 * throw. Such a throw is PARKED in nBoundaryRc by the boundary rail; consume it` |
|       - | 3036 | `		 * here and route it, or the catch runs and execution resumes inside the try` |
|       - | 3037 | `		 * ("resumed-dtor" instead of php's "caught-dtor"). */` |
|    8775 | 3038 | `		if( pVm->nBoundaryRc != 0 ){` |
|       6 | 3039 | `			rc = pVm->nBoundaryRc;` |
|       6 | 3040 | `			pVm->nBoundaryRc = 0;` |
|       6 | 3041 | `			if( rc == PH7_ABORT ){` |
|     ! 0 | 3042 | `				VM_EXIT_ABORT;` |
|       - | 3043 | `			}` |
|       6 | 3044 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 3045 | `		}` |
|    4382 | 3046 | `	}` |
|    8771 | 3047 | `	VM_EXIT_BREAK;` |
|     ! 0 | 3048 | `	VM_EXIT_BREAK;` |
|    4390 | 3049 | `}` |
|       - | 3050 |  |
