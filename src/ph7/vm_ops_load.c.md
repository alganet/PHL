# src/ph7/vm_ops_load.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1466/1601 lines (91.57%)

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
|    3254 |   28 | `PH7_PRIVATE VmOpRc VmExecOpStoreRef(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |   29 | `{` |
|    3259 |   30 | `	ph7_value *pTos = pState->pTos;` |
|    3259 |   31 | `	ph7_value *pStack = pState->pStack;` |
|    3259 |   32 | `	VmInstr *aInstr = pState->aInstr;` |
|    3259 |   33 | `	sxi32 pc = pState->pc;` |
|       - |   34 | `	sxi32 rc;` |
|    1627 |   35 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    3259 |   36 | `	 SyString sName = { 0 , 0 };` |
|       - |   37 | `	 VmFrame *pFrameLocal;` |
|       - |   38 | `	SyHashEntry *pEntry;` |
|       - |   39 | `	sxu32 nIdx;` |
|       - |   40 | `#ifdef UNTRUST` |
|       - |   41 | `	if( pTos < pStack ){` |
|       - |   42 | `		VM_EXIT_ABORT;` |
|       - |   43 | `	}` |
|       - |   44 | `#endif` |
|    3259 |   45 | `	if( pInstr->iP2 == 1 ){` |
|       - |   46 | ``		/* Member reference target: `$o->p =& $x` / `self::$s =& $x`. The`` |
|       - |   47 | `		 * preceding OP_MEMBER (PH7_MEMBER_REF_TARGET) resolved the property slot` |
|       - |   48 | `		 * and stashed it. Stack: [ ... , source, member-result(top) ]. Rebind the` |
|       - |   49 | `		 * property's nIdx to alias the source variable's slot and pin that slot` |
|       - |   50 | `		 * past its owning frame (like a use(&$x) capture) so neither frame` |
|       - |   51 | `		 * teardown nor a later unset recycles it while the property aliases it. */` |
|      48 |   52 | `		ph7_value *pSrc = &pTos[-1];` |
|      48 |   53 | `		sxu32 nSrcIdx = pSrc->nIdx;` |
|      48 |   54 | `		VmClassAttr *pVmAttr = pVm->pRefTargetAttr;` |
|      48 |   55 | `		ph7_class_attr *pStAttr = pVm->pRefTargetStaticAttr;` |
|      48 |   56 | `		if( pSrc->iFlags & MEMOBJ_AUX_STROFFSET ){` |
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
|      43 |   72 | `		if( nSrcIdx == SXU32_HIGH ){` |
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
|      43 |   94 | `		if( nSrcIdx == SXU32_HIGH ){` |
|       - |   95 | `			/* Reservation failed: nothing to bind to. */` |
|      43 |   96 | `		}else if( pVmAttr ){` |
|      31 |   97 | `			sxu32 nOldIdx = pVmAttr->nIdx;` |
|      31 |   98 | `			if( nOldIdx != nSrcIdx ){` |
|      31 |   99 | `				if( (pVmAttr->iState & VM_CLASS_ATTR_REFBOUND) == 0 ){` |
|       - |  100 | `					/* Release this property's own (unshared) slot before repointing.` |
|       - |  101 | `					 * A reference-bound property bypasses typed coercion in php, so` |
|       - |  102 | `					 * drop any typed-slot enforcement entry too. */` |
|      25 |  103 | `					PH7_VmStoreFilterDrop(&(*pVm),pVmAttr->pAttr,nOldIdx);` |
|      25 |  104 | `					PH7_VmUnsetMemObj(&(*pVm),nOldIdx,TRUE);` |
|      13 |  105 | `				}else{` |
|       - |  106 | `					/* Already bound elsewhere: give that slot its pin back, which` |
|       - |  107 | `					 * releases it when this property was its last holder. */` |
|       7 |  108 | `					VmUnpinMemObjSlot(&(*pVm),nOldIdx);` |
|       - |  109 | `				}` |
|      31 |  110 | `				pVmAttr->nIdx = nSrcIdx;` |
|      31 |  111 | `				pVmAttr->iState \|= VM_CLASS_ATTR_REFBOUND;` |
|      31 |  112 | `				pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      31 |  113 | `				VmPinMemObjSlotCounted(&(*pVm),nSrcIdx);` |
|      16 |  114 | `			}` |
|      28 |  115 | `		}else if( pStAttr ){` |
|      13 |  116 | `			sxu32 nOldIdx = pStAttr->nIdx;` |
|      13 |  117 | `			if( nOldIdx != nSrcIdx ){` |
|       - |  118 | `				/* Give the previous target back, exactly as the instance arm above does.` |
|       - |  119 | `				 * A permanent pin was left on every slot the property had ever named, so` |
|       - |  120 | `				 * each of them stayed a REFERENCE for the rest of the script — which an` |
|       - |  121 | ``				 * ordinary array COPY then shared (`$d = $a; $d[0] = 99;` wrote through`` |
|       - |  122 | ``				 * to `$a[0]`, silently), since "is this element a reference" is answered`` |
|       - |  123 | `				 * by who still holds it. */` |
|      13 |  124 | `				if( (pStAttr->iFlags & PH7_CLASS_ATTR_REFBOUND) == 0 ){` |
|       - |  125 | `					/* The static's own (unshared) slot. A reference-bound property bypasses` |
|       - |  126 | `					 * typed coercion in php, so drop any typed-slot enforcement entry too. */` |
|       9 |  127 | `					PH7_VmStoreFilterDrop(&(*pVm),pStAttr,nOldIdx);` |
|       9 |  128 | `					PH7_VmUnsetMemObj(&(*pVm),nOldIdx,TRUE);` |
|       5 |  129 | `				}else{` |
|       5 |  130 | `					VmUnpinMemObjSlot(&(*pVm),nOldIdx);` |
|       - |  131 | `				}` |
|      13 |  132 | `				pStAttr->nIdx = nSrcIdx;` |
|      13 |  133 | `				pStAttr->iFlags \|= PH7_CLASS_ATTR_REFBOUND;` |
|      13 |  134 | `				VmPinMemObjSlotCounted(&(*pVm),nSrcIdx);` |
|       6 |  135 | `			}` |
|       6 |  136 | `		}` |
|      43 |  137 | `		if( pVm->pRefTargetThis ){` |
|      31 |  138 | `			PH7_ClassInstanceUnref(pVm->pRefTargetThis);` |
|      15 |  139 | `		}` |
|      43 |  140 | `		pVm->pRefTargetAttr = 0;` |
|      43 |  141 | `		pVm->pRefTargetStaticAttr = 0;` |
|      43 |  142 | `		pVm->pRefTargetThis = 0;` |
|       - |  143 | `		/* Pop the member-result; leave the source as the expression value. */` |
|      43 |  144 | `		VmPopOperand(&pTos,1);` |
|      43 |  145 | `		VM_EXIT_BREAK;` |
|       - |  146 | `	}` |
|    3213 |  147 | `	if( pInstr->p3 == 0 ){` |
|       - |  148 | `		char *zName;` |
|       - |  149 | `		/* Take the variable name from the Next on the stack */` |
|     ! 0 |  150 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  151 | `			/* Force a string cast */` |
|     ! 0 |  152 | `			PH7_MemObjToString(pTos);` |
|     ! 0 |  153 | `		}` |
|     ! 0 |  154 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|     ! 0 |  155 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|     ! 0 |  156 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     ! 0 |  157 | `			if( zName ){` |
|     ! 0 |  158 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|     ! 0 |  159 | `			}` |
|     ! 0 |  160 | `		}` |
|     ! 0 |  161 | `		PH7_MemObjRelease(pTos);` |
|     ! 0 |  162 | `		pTos--;` |
|     ! 0 |  163 | `	}else{` |
|    3213 |  164 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|       - |  165 | `	}` |
|    3213 |  166 | `	if( pTos->iFlags & MEMOBJ_AUX_STROFFSET ){` |
|       - |  167 | `		/* php: a string OFFSET cannot be either end of a reference. The value read` |
|       - |  168 | `		 * out of a string still carries the BASE VARIABLE's slot, so binding it` |
|       - |  169 | `		 * aliased the whole string and a later write through the reference REPLACED` |
|       - |  170 | `		 * it ($s = "abc"; $r = &$s[1]; $r = "Z"; left $s === "Z"). Same Error the` |
|       - |  171 | `		 * by-ref ARGUMENT path raises for f($s[1]). */` |
|      10 |  172 | `		rc = VmThrowFromVm(&(*pVm),"Error","Cannot create references to/from string offsets",` |
|       - |  173 | `			sizeof("Cannot create references to/from string offsets")-1);` |
|      10 |  174 | `		PH7_MemObjRelease(pTos);` |
|      10 |  175 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|      10 |  176 | `		pTos->nIdx = SXU32_HIGH;` |
|      10 |  177 | `		if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      12 |  178 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  179 | `	}` |
|    3205 |  180 | `	nIdx = pTos->nIdx;` |
|       - |  181 | `	{` |
|       - |  182 | ``		/* `$r = &$o->p` on a property no write may reach: php refuses the BIND,`` |
|       - |  183 | `		 * because the alias would let a later write through $r reach the property` |
|       - |  184 | ``		 * with nothing in the way. PHL bound it, so `$r = 99` afterwards rewrote a`` |
|       - |  185 | `		 * readonly property and a DatePeriod's recurrence count alike. */` |
|    3205 |  186 | `		sxi32 rcNw = PH7_VmCheckIndirectModify(&(*pVm),nIdx);` |
|    3205 |  187 | `		if( rcNw != SXRET_OK ){` |
|       9 |  188 | `			PH7_MemObjRelease(pTos);` |
|       9 |  189 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       9 |  190 | `			pTos->nIdx = SXU32_HIGH;` |
|       9 |  191 | `			if( pInstr->p3 == 0 && sName.zString ){` |
|     ! 0 |  192 | `				SyMemBackendFree(&pVm->sAllocator,(void *)sName.zString);` |
|     ! 0 |  193 | `			}` |
|       9 |  194 | `			if( rcNw == PH7_ABORT ){ VM_EXIT_ABORT; }` |
|       9 |  195 | `			PH7_THROW_ROUTE_MIDEXPR(rcNw)` |
|       - |  196 | `		}` |
|       - |  197 | `	}` |
|    3197 |  198 | `	if(nIdx == SXU32_HIGH ){` |
|       - |  199 | `		{` |
|       - |  200 | `			/* No slot behind the source. php binds a FRESH variable holding the value` |
|       - |  201 | `			 * — whatever its type — and the only thing it ever says about it is the` |
|       - |  202 | ``			 * notice below, so PHL's `Reference operator require a variable not a`` |
|       - |  203 | ``			 * constant as it's right operand` fired on every scalar: `$r =& f()`,`` |
|       - |  204 | ``			 * `$r =& f()[0]` and `$r =& mk()->p` all refused the bind and left $r`` |
|       - |  205 | `			 * undefined too. (Object/array/resource sources already took this path.)` |
|       - |  206 | ``			 * A source WRITTEN as a call is php's `Only variables should be assigned`` |
|       - |  207 | ``			 * by reference`, raised when the callee did not return by reference —`` |
|       - |  208 | `			 * the compiler marked it, since a temporary the call was only the BASE of` |
|       - |  209 | ``			 * (`f()[0]`) is silent. */`` |
|       - |  210 | `			ph7_value *pObj;` |
|      14 |  211 | `			if( (pInstr->iP1 & PH7_STOREREF_CALLSRC)` |
|      10 |  212 | `			 && (pTos->iFlags & MEMOBJ_AUX_NATIVEPROP) == 0 ){` |
|       5 |  213 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|       - |  214 | `					"Only variables should be assigned by reference");` |
|       2 |  215 | `			}` |
|       - |  216 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      15 |  217 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      15 |  218 | `			if( pObj == 0 ){` |
|     ! 0 |  219 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - |  220 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|     ! 0 |  221 | `				VM_EXIT_ABORT;` |
|       - |  222 | `			}` |
|       - |  223 | `			/* Perform the store operation */` |
|      15 |  224 | `			PH7_MemObjStore(pTos,pObj);` |
|      15 |  225 | `			pTos->nIdx = pObj->nIdx;` |
|       - |  226 | `		}` |
|    3190 |  227 | `	}else if( sName.nByte > 0){` |
|    3183 |  228 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|       - |  229 | `			/* php 8.1's non-catchable fatal (compile-time there) */` |
|       3 |  230 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot acquire reference to $GLOBALS");` |
|       3 |  231 | `			pVm->iExitStatus = 255;` |
|       3 |  232 | `			pVm->bHaltRequested = 1;` |
|       3 |  233 | `			VM_EXIT_ABORT;` |
|     ! 0 |  234 | `		}else{` |
|    3181 |  235 | `			pFrameLocal = pVm->pFrame;` |
|    3181 |  236 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       - |  237 | `			/* Query the local frame */` |
|    3181 |  238 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|    3181 |  239 | `			if( pEntry ){` |
|       - |  240 | ``				/* php RE-BINDS a name that already exists (`$y = 2; $y = &$x;`, and the`` |
|       - |  241 | ``				 * `$r = &$a[$k]` idiom from the second loop step on) — the old binding`` |
|       - |  242 | `				 * goes, its value with it if nothing else holds it. */` |
|    3085 |  243 | `				PH7_VmRebindVarSlot(&(*pVm),pFrameLocal,pEntry,sName.zString,sName.nByte,nIdx);` |
|    3085 |  244 | `				if( pInstr->p3 == 0 && sName.zString ){` |
|       - |  245 | `					/* The name was duplicated for a symbol-table key this rebind does` |
|       - |  246 | `					 * not need — the entry keeps the key it was created with. */` |
|     ! 0 |  247 | `					SyMemBackendFree(&pVm->sAllocator,(void *)sName.zString);` |
|     ! 0 |  248 | `				}` |
|    1543 |  249 | `			}else{` |
|      97 |  250 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|      97 |  251 | `				if( pFrameLocal->pParent == 0 ){` |
|       - |  252 | `					/* Insert in the $GLOBALS array */` |
|      80 |  253 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|      38 |  254 | `				}` |
|      97 |  255 | `				if( rc == SXRET_OK ){` |
|      97 |  256 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|      46 |  257 | `				}` |
|       - |  258 | `			}` |
|       - |  259 | `		}` |
|    1588 |  260 | `	}` |
|    3195 |  261 | `	VM_EXIT_BREAK;` |
|     ! 0 |  262 | `	VM_EXIT_BREAK;` |
|    1632 |  263 | `}` |
|       - |  264 |  |
|       - |  265 | `/* LOAD_IDX's own iP2 context codes (they do NOT line up with PH7_MEMBER_*).` |
|       - |  266 | ` * The two UNSET codes live in ph7int.h: the property opcode has to recognize an` |
|       - |  267 | ` * unset-subscript BASE, which is an indirect modification of what the base holds. */` |
|       - |  268 | `#define VM_IDX_CTX_ISSET 4` |
|       - |  269 | `#define VM_IDX_CTX_EMPTY 6` |
|       - |  270 | `/*` |
|       - |  271 | ` * php rejects an OBJECT or an ARRAY used as an array offset, naming the offending` |
|       - |  272 | ` * type the way get_debug_type() does — the CLASS name for an object, "array" for` |
|       - |  273 | ` * an array — and wording the failure by context:` |
|       - |  274 | ` *` |
|       - |  275 | ` *   read/write   Cannot access offset of type Foo on array` |
|       - |  276 | ` *   isset/empty  Cannot access offset of type Foo in isset or empty` |
|       - |  277 | ` *   unset        Cannot unset offset of type Foo on array` |
|       - |  278 | ` *` |
|       - |  279 | ` * A RESOURCE is deliberately absent: php does not reject it, it warns` |
|       - |  280 | ` * ("Resource ID#N used as offset, casting to integer (N)") and uses the id as an` |
|       - |  281 | ` * integer key. VmOffsetResourceWarn() below handles that half.` |
|       - |  282 | ` *` |
|       - |  283 | ` * Returns TRUE and fills pMsg when the key must be rejected. iCtx is the` |
|       - |  284 | ` * instruction's iP2 (any value other than the isset/unset/empty codes reads as` |
|       - |  285 | ` * an access).` |
|       - |  286 | ` */` |
|  332353 |  287 | `static int VmOffsetTypeRejected(ph7_vm *pVm,ph7_value *pKey,int iCtx,SyBlob *pMsg)` |
|       5 |  288 | `{` |
|       - |  289 | `	const char *zType;` |
|  332358 |  290 | `	SyString *pClass = 0;` |
|  332358 |  291 | `	if( pKey == 0 ){` |
|     ! 0 |  292 | `		return FALSE;` |
|       - |  293 | `	}` |
|  332358 |  294 | `	if( pKey->iFlags & MEMOBJ_OBJ ){` |
|      25 |  295 | `		ph7_class_instance *pInst = (ph7_class_instance *)pKey->x.pOther;` |
|      25 |  296 | `		if( pInst && pInst->pClass ){` |
|      25 |  297 | `			pClass = &pInst->pClass->sName;` |
|      12 |  298 | `		}` |
|      25 |  299 | `		zType = "object";` |
|  332346 |  300 | `	}else if( pKey->iFlags & MEMOBJ_HASHMAP ){` |
|      15 |  301 | `		zType = "array";` |
|       8 |  302 | `	}else{` |
|  332320 |  303 | `		return FALSE;` |
|       - |  304 | `	}` |
|      39 |  305 | `	SyBlobInit(pMsg,&pVm->sAllocator);` |
|      39 |  306 | `	if( VM_IDX_IS_UNSET(iCtx) ){` |
|       3 |  307 | `		SyBlobAppend(pMsg,"Cannot unset offset of type ",sizeof("Cannot unset offset of type ")-1);` |
|       2 |  308 | `	}else{` |
|      37 |  309 | `		SyBlobAppend(pMsg,"Cannot access offset of type ",sizeof("Cannot access offset of type ")-1);` |
|       - |  310 | `	}` |
|      39 |  311 | `	if( pClass ){` |
|      25 |  312 | `		SyBlobAppend(pMsg,pClass->zString,pClass->nByte);` |
|      13 |  313 | `	}else{` |
|      15 |  314 | `		SyBlobAppend(pMsg,zType,(sxu32)SyStrlen(zType));` |
|       - |  315 | `	}` |
|      39 |  316 | `	if( iCtx == VM_IDX_CTX_ISSET \|\| iCtx == VM_IDX_CTX_EMPTY ){` |
|       7 |  317 | `		SyBlobAppend(pMsg," in isset or empty",sizeof(" in isset or empty")-1);` |
|       4 |  318 | `	}else{` |
|      33 |  319 | `		SyBlobAppend(pMsg," on array",sizeof(" on array")-1);` |
|       - |  320 | `	}` |
|      39 |  321 | `	return TRUE;` |
|  166182 |  322 | `}` |
|       - |  323 | `/*` |
|       - |  324 | ` * php's other half of the offset-type rules: a RESOURCE offset is accepted, with` |
|       - |  325 | `` * `Warning: Resource ID#N used as offset, casting to integer (N)`, and the key`` |
|       - |  326 | ` * becomes that integer. Rewrites pKey in place so the normal integer-key path` |
|       - |  327 | ` * takes over.` |
|       - |  328 | ` */` |
|  332315 |  329 | `static void VmOffsetResourceWarn(ph7_vm *pVm,ph7_value *pKey)` |
|       5 |  330 | `{` |
|       - |  331 | `	sxu32 nId;` |
|  332320 |  332 | `	if( pKey == 0 \|\| (pKey->iFlags & MEMOBJ_RES) == 0 ){` |
|  332310 |  333 | `		return;` |
|       - |  334 | `	}` |
|      11 |  335 | `	nId = PH7_VmResourceId(pVm,pKey->x.pOther);` |
|      16 |  336 | `	VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       5 |  337 | `		"Resource ID#%u used as offset, casting to integer (%u)",nId,nId);` |
|      11 |  338 | `	PH7_MemObjRelease(pKey);` |
|      11 |  339 | `	pKey->x.iVal = (sxi64)nId;` |
|      11 |  340 | `	MemObjSetType(pKey,MEMOBJ_INT);` |
|  166163 |  341 | `}` |
|       - |  342 | `/*` |
|       - |  343 | ` * php DEPRECATES (it does not reject) a NULL array offset, then normalizes it to` |
|       - |  344 | `` * the empty-string key "": `$a[null]`, `$a[$undef]` and `[null => v]` all warn`` |
|       - |  345 | `` * `Using null as an array offset is deprecated, use an empty string instead` and`` |
|       - |  346 | ` * then read/write the "" slot. Emit that E_DEPRECATED notice and return TRUE so` |
|       - |  347 | ` * the caller falls through to the ordinary lookup/insert, which already casts` |
|       - |  348 | ` * NULL->"" (HashmapLookup / HashmapInsert). A LOSSY-FLOAT offset stays a rejected` |
|       - |  349 | ` * TypeError: PHL deliberately targets php's NON-deprecated surface for the` |
|       - |  350 | ` * float->int truncation (VmRejectFloatOperand policy, §2), so only the null case` |
|       - |  351 | ` * is coerced here. Returns FALSE (no notice) for a non-null key.` |
|       - |  352 | ` */` |
|  436101 |  353 | `static int VmNullOffsetDeprecate(ph7_vm *pVm,ph7_value *pKey)` |
|       5 |  354 | `{` |
|  436106 |  355 | `	if( pKey == 0 \|\| (pKey->iFlags & MEMOBJ_NULL) == 0 ){` |
|  436082 |  356 | `		return FALSE;` |
|       - |  357 | `	}` |
|      27 |  358 | `	PH7_VmThrowError(&(*pVm),0,E_DEPRECATED,` |
|       - |  359 | `		"Using null as an array offset is deprecated, use an empty string instead");` |
|      27 |  360 | `	return TRUE;` |
|  218047 |  361 | `}` |
|       - |  362 | `/*` |
|       - |  363 | ` * The three rules above, applied to a BUILTIN's key argument.` |
|       - |  364 | ` *` |
|       - |  365 | `` * php's array_key_exists() does not run a `string\|int` ZPP row on its $key — it`` |
|       - |  366 | `` * hands the value to the same offset machinery `$a[$key]` uses, so the two agree`` |
|       - |  367 | ` * on every type: an object or an array is the catchable` |
|       - |  368 | `` * `Cannot access offset of type X on array`, a RESOURCE warns and becomes its`` |
|       - |  369 | `` * integer id, `null` deprecates and reads the "" key, and a bool/float/numeric`` |
|       - |  370 | ` * string folds the way any subscript folds. PHL's builtin had its own narrower` |
|       - |  371 | `` * check and therefore its own answers — a `string\|int` TypeError for the two`` |
|       - |  372 | `` * cases php ACCEPTS (null, lossy float) and a silent `false` for the three php`` |
|       - |  373 | ` * REJECTS or coerces (object, array, resource). The object case was the worst of` |
|       - |  374 | ` * them: a __toString() object was stringified and could answer TRUE for a key` |
|       - |  375 | ` * php refuses to look up at all.` |
|       - |  376 | ` *` |
|       - |  377 | ` * bZppWording picks which of php's two messages the caller reports. php words the` |
|       - |  378 | ` * illegal-type rejection differently in the alias than in the function itself —` |
|       - |  379 | `` * `key_exists(): Argument #1 ($key) must be a valid array offset type` vs the`` |
|       - |  380 | ` * engine's offset Error — verified against 8.5.8; the null-key DEPRECATION is the` |
|       - |  381 | ` * array_key_exists() wording in both.` |
|       - |  382 | ` *` |
|       - |  383 | ` * pKey is rewritten in place (resource -> int), so callers pass a private copy,` |
|       - |  384 | ` * not the caller's own argument slot. Returns SXRET_OK when the key is usable, or` |
|       - |  385 | ` * the status of the TypeError thrown.` |
|       - |  386 | ` */` |
|     130 |  387 | `PH7_PRIVATE sxi32 PH7_VmArrayKeyArg(ph7_context *pCtx,ph7_value *pKey,int bZppWording)` |
|       5 |  388 | `{` |
|     135 |  389 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - |  390 | `	SyBlob sMsg;` |
|     135 |  391 | `	if( VmOffsetTypeRejected(pVm,pKey,0,&sMsg) ){` |
|       - |  392 | `		sxi32 rc;` |
|      11 |  393 | `		if( bZppWording ){` |
|       5 |  394 | `			SyBlobRelease(&sMsg);` |
|       7 |  395 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  396 | `				"%s(): Argument #1 ($key) must be a valid array offset type",` |
|       2 |  397 | `				ph7_function_name(pCtx));` |
|       - |  398 | `		}` |
|      10 |  399 | `		rc = PH7_VmThrowException(pCtx,"TypeError","%.*s",` |
|       6 |  400 | `			(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|       7 |  401 | `		SyBlobRelease(&sMsg);` |
|       7 |  402 | `		return rc;` |
|       - |  403 | `	}` |
|     125 |  404 | `	VmOffsetResourceWarn(pVm,pKey);` |
|     120 |  405 | `	if( (pKey->iFlags & MEMOBJ_REAL)` |
|      70 |  406 | `	 && pKey->rVal != (ph7_real)(sxi64)pKey->rVal ){` |
|       - |  407 | `		/* php DEPRECATES the lossy float and truncates; PHL rejects it, here as` |
|       - |  408 | ``		 * everywhere else (§10) — and with the SAME message `$a[5.7]` gives, so`` |
|       - |  409 | `		 * the builtin and the subscript stay one rule. */` |
|       8 |  410 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  411 | `			"Cannot access offset of type float on array");` |
|       - |  412 | `	}` |
|     118 |  413 | `	if( pKey->iFlags & MEMOBJ_NULL ){` |
|       - |  414 | `		/* php names the parameter here rather than "an array offset"; the effect` |
|       - |  415 | `		 * is the engine's — the lookup below reads the "" key. */` |
|       3 |  416 | `		PH7_VmThrowError(pVm,0,E_DEPRECATED,` |
|       - |  417 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|       - |  418 | `			"use an empty string instead");` |
|       1 |  419 | `	}` |
|     118 |  420 | `	return SXRET_OK;` |
|      70 |  421 | `}` |
|       - |  422 | `/*` |
|       - |  423 | ` * Does this string START with an integer php's is_numeric_string would answer` |
|       - |  424 | ` * IS_LONG for? Returns 0 when it does not — no digits at all ("", "-", "p"), a` |
|       - |  425 | ` * FLOAT-shaped prefix ("1.5", ".5", "1.", "1e2", "1E2x") or a run that overflows` |
|       - |  426 | ` * a signed 64-bit int ("9223372036854775808") — 1 when it does and only optional` |
|       - |  427 | ` * trailing WHITESPACE follows (" 12 ", "+12", "007"), and 2 when it does but` |
|       - |  428 | ` * trailing DATA follows ("12abc", "0x1", "1e", "1 x"), which is php's` |
|       - |  429 | `` * `Illegal string offset` warning. *piVal receives the integer for 1 and 2.`` |
|       - |  430 | ` *` |
|       - |  431 | ` * php reaches the same three answers through is_numeric_string_ex(allow_errors=1)` |
|       - |  432 | ` * plus its IS_LONG/IS_DOUBLE verdict: a fraction or a well-formed exponent makes` |
|       - |  433 | ` * the verdict IS_DOUBLE, which a string offset refuses, while a bare 'e' is just` |
|       - |  434 | ` * trailing data.` |
|       - |  435 | ` */` |
|     104 |  436 | `static int VmStringOffsetInt(const char *zIn,sxu32 nByte,sxi64 *piVal)` |
|       4 |  437 | `{` |
|     108 |  438 | `	const char *z = zIn, *zEnd = &zIn[nByte], *zDigit;` |
|     108 |  439 | `	sxu64 uVal = 0, uLimit;` |
|     108 |  440 | `	int isNeg = 0, nDigit, i;` |
|     120 |  441 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){` |
|      14 |  442 | `		z++;` |
|       2 |  443 | `	}` |
|     108 |  444 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|       7 |  445 | `		isNeg = z[0] == '-';` |
|       7 |  446 | `		z++;` |
|       3 |  447 | `	}` |
|     108 |  448 | `	zDigit = z;` |
|     248 |  449 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|     142 |  450 | `		z++;` |
|       2 |  451 | `	}` |
|     108 |  452 | `	nDigit = (int)(z - zDigit);` |
|     108 |  453 | `	if( nDigit < 1 ){` |
|      44 |  454 | `		return 0;` |
|       - |  455 | `	}` |
|      66 |  456 | `	if( z < zEnd && z[0] == '.' ){` |
|       - |  457 | `		/* "1." and "1.5" alike: php reads a double from here. */` |
|       5 |  458 | `		return 0;` |
|       - |  459 | `	}` |
|      62 |  460 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|       3 |  461 | `		const char *zExp = &z[1];` |
|       3 |  462 | `		if( zExp < zEnd && (zExp[0] == '+' \|\| zExp[0] == '-') ){` |
|     ! 0 |  463 | `			zExp++;` |
|     ! 0 |  464 | `		}` |
|       3 |  465 | `		if( zExp < zEnd && (unsigned char)zExp[0] < 0xc0 && SyisDigit(zExp[0]) ){` |
|       3 |  466 | `			return 0;` |
|       - |  467 | `		}` |
|     ! 0 |  468 | `	}` |
|       - |  469 | `	/* Accumulate unsigned so PHP_INT_MIN's magnitude (2^63) is representable —` |
|       - |  470 | `	 * "-9223372036854775808" is a legal offset, "9223372036854775808" is not. */` |
|      64 |  471 | `	while( nDigit > 1 && zDigit[0] == '0' ){` |
|       5 |  472 | `		zDigit++; nDigit--;` |
|       1 |  473 | `	}` |
|      60 |  474 | `	uLimit = isNeg ? (sxu64)SXI64_HIGH + 1 : (sxu64)SXI64_HIGH;` |
|      60 |  475 | `	if( nDigit > 19 ){` |
|     ! 0 |  476 | `		return 0;` |
|       - |  477 | `	}` |
|     188 |  478 | `	for( i = 0 ; i < nDigit ; ++i ){` |
|     132 |  479 | `		sxu64 d = (sxu64)(zDigit[i] - '0');` |
|     132 |  480 | `		if( uVal > (uLimit - d)/10 ){` |
|       3 |  481 | `			return 0;` |
|       - |  482 | `		}` |
|     130 |  483 | `		uVal = uVal*10 + d;` |
|      66 |  484 | `	}` |
|      58 |  485 | `	*piVal = isNeg ? (sxi64)(~uVal + 1) : (sxi64)uVal;` |
|      68 |  486 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){` |
|      12 |  487 | `		z++;` |
|       2 |  488 | `	}` |
|      58 |  489 | `	return z == zEnd ? 1 : 2;` |
|      56 |  490 | `}` |
|       - |  491 | `/*` |
|       - |  492 | ` * php's offset rules for a STRING container (zend_check_string_offset, and the` |
|       - |  493 | ` * isset/empty arm of ZEND_ISSET_ISEMPTY_DIM_OBJ). They are NOT the array rules,` |
|       - |  494 | ` * and PHL applied none of them: every offset went through an int cast, so` |
|       - |  495 | `` * `$s[""]`, `$s["-"]` and `$s["p"]` all answered `$s[0]` — a silent wrong answer`` |
|       - |  496 | ` * on code php refuses to run. php's table:` |
|       - |  497 | ` *` |
|       - |  498 | ` *   int                     the offset` |
|       - |  499 | ` *   null / bool / float     Warning: String offset cast occurred, then cast` |
|       - |  500 | ` *   integer-shaped string   the offset (leading/trailing space and '+' allowed)` |
|       - |  501 | ` *   int-then-garbage string Warning: Illegal string offset "12abc", then 12` |
|       - |  502 | ` *   any other string        TypeError: Cannot access offset of type string on string` |
|       - |  503 | ` *   array/object/resource   TypeError, naming the type (an object's CLASS)` |
|       - |  504 | ` *` |
|       - |  505 | ` * isset()/empty() raise NOTHING and answer "not set" for every shape the read` |
|       - |  506 | `` * path would reject OR warn about: `isset($s["0x1"])` is false even though`` |
|       - |  507 | `` * reading it warns and yields `$s[0]`. A `??` fetch sits BETWEEN that and a real`` |
|       - |  508 | ` * read — it suppresses the not-set diagnostics but still warns about the offset` |
|       - |  509 | ` * SHAPE and still reads it. iLevel selects which of the three (VM_STROFF_LOUD /` |
|       - |  510 | ` * _COALESCE / _ISSET).` |
|       - |  511 | ` */` |
|  866510 |  512 | `PH7_PRIVATE int VmStringOffsetResolve(ph7_vm *pVm,ph7_value *pIdx,int iLevel,sxi64 *piOfft,SyBlob *pMsg)` |
|       5 |  513 | `{` |
|  866515 |  514 | `	if( (pIdx->iFlags & MEMOBJ_INT) != 0 && (pIdx->iFlags & MEMOBJ_REAL) == 0 ){` |
|  866343 |  515 | `		*piOfft = pIdx->x.iVal;` |
|  866343 |  516 | `		return VM_STROFF_OK;` |
|       - |  517 | `	}` |
|     176 |  518 | `	if( pIdx->iFlags & MEMOBJ_STRING ){` |
|     160 |  519 | `		int eInt = VmStringOffsetInt((const char *)SyBlobData(&pIdx->sBlob),` |
|      52 |  520 | `			SyBlobLength(&pIdx->sBlob),piOfft);` |
|     108 |  521 | `		if( eInt == 1 ){` |
|      24 |  522 | `			return VM_STROFF_OK;` |
|       - |  523 | `		}` |
|      86 |  524 | `		if( eInt == 2 && iLevel != VM_STROFF_ISSET ){` |
|       - |  525 | ``			/* int-then-garbage. This is the one diagnostic a `??` fetch keeps:`` |
|       - |  526 | ``			 * `$s["1x"] ?? "d"` warns and answers $s[1] (PHL called it "not set"`` |
|       - |  527 | `			 * and answered the default — a wrong VALUE, not just a missing` |
|       - |  528 | `			 * warning). Only isset()/empty() — and the intermediate step of an` |
|       - |  529 | `			 * unset chain, which php keeps quiet about the shape — stay silent. */` |
|      28 |  530 | `			if( iLevel != VM_STROFF_UNSETBASE ){` |
|       - |  531 | `				SyString sKey;` |
|      28 |  532 | `				SyStringInitFromBuf(&sKey,SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob));` |
|      28 |  533 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Illegal string offset \"%z\"",&sKey);` |
|      13 |  534 | `			}` |
|      28 |  535 | `			return VM_STROFF_OK;` |
|       - |  536 | `		}` |
|      60 |  537 | `		if( iLevel != VM_STROFF_LOUD && iLevel != VM_STROFF_UNSETBASE ){` |
|      24 |  538 | `			return VM_STROFF_MISS;` |
|       4 |  539 | `		}` |
|      87 |  540 | `	}else if( (pIdx->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|       - |  541 | `		/* null / bool / float: php casts, but says so in a real read or write —` |
|       - |  542 | `		 * and in the intermediate step of an unset chain, which reads the offset` |
|       - |  543 | ``		 * to hand it on (`unset($s[1.5][0])` warns about the cast). */`` |
|      48 |  544 | `		if( iLevel == VM_STROFF_LOUD \|\| iLevel == VM_STROFF_UNSETBASE ){` |
|      34 |  545 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"String offset cast occurred");` |
|      16 |  546 | `		}` |
|      48 |  547 | `		PH7_MemObjToInteger(pIdx);` |
|      48 |  548 | `		*piOfft = pIdx->x.iVal;` |
|      48 |  549 | `		return VM_STROFF_OK;` |
|      24 |  550 | `	}else if( iLevel == VM_STROFF_ISSET ){` |
|       - |  551 | `		/* An array/object/resource offset is "not set" for isset()/empty() — but a` |
|       - |  552 | ``		 * `??` fetch RAISES for it (probed: `$s[[]] ?? "d"` is the TypeError while`` |
|       - |  553 | ``		 * `isset($s[[]])` is false), so only the fully-quiet level answers MISS. */`` |
|      10 |  554 | `		return VM_STROFF_MISS;` |
|       - |  555 | `	}` |
|       - |  556 | `	{` |
|       - |  557 | `		char zBuf[128];` |
|      52 |  558 | `		SyBlobInit(pMsg,&pVm->sAllocator);` |
|      76 |  559 | `		SyBlobFormat(pMsg,"Cannot access offset of type %s on string",` |
|      24 |  560 | `			VmValueGivenName(pIdx,zBuf,sizeof(zBuf)));` |
|       - |  561 | `	}` |
|      52 |  562 | `	return VM_STROFF_REJECT;` |
|  434434 |  563 | `}` |
|       - |  564 | `/*` |
|       - |  565 | ` * OP_STORE_IDX_REF: body moved verbatim from the OP_STORE_IDX_REF arm of` |
|       - |  566 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  567 | ` */` |
|  436765 |  568 | `PH7_PRIVATE VmOpRc VmExecOpStoreIdxRef(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  569 | `{` |
|  436770 |  570 | `	ph7_value *pTos = pState->pTos;` |
|  436770 |  571 | `	ph7_value *pStack = pState->pStack;` |
|  436770 |  572 | `	VmInstr *aInstr = pState->aInstr;` |
|  436770 |  573 | `	sxi32 pc = pState->pc;` |
|       - |  574 | `	sxi32 rc;` |
|  218374 |  575 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|  436770 |  576 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|       - |  577 | `	ph7_value *pKey;` |
|       - |  578 | `	sxu32 nIdx;` |
|  436770 |  579 | `	if( pInstr->iP1 & 1 ){` |
|       - |  580 | `		/* Key is next on stack (bit 1 is PH7_STOREREF_CALLSRC, not a key) */` |
|   83158 |  581 | `		pKey = pTos;` |
|   83158 |  582 | `		pTos--;` |
|   41582 |  583 | `	}else{` |
|  353617 |  584 | `		pKey = 0;` |
|       - |  585 | `	}` |
|       - |  586 | `		/* php DEPRECATES a null / lossy-float write subscript (then normalizes "" /` |
|       - |  587 | ``		 * truncates); it does not reject either. A null key — by-VALUE (`$a[$k]=v`)`` |
|       - |  588 | ``		 * OR by-REF (`$a[$k]=&$x`) — deprecates + coerces to the "" key: the notice is`` |
|       - |  589 | `		 * emitted DOWN AT THE INSERT (below), not here, so a null/false container that` |
|       - |  590 | ``		 * auto-vivifies (`$x=null; $x[null]=v`) gets it too (the base is not yet a`` |
|       - |  591 | `		 * hashmap at this point), and PH7_HashmapInsert / HashmapInsertByRef both cast` |
|       - |  592 | `		 * NULL->"". A lossy-FLOAT subscript stays a loud TypeError in BOTH forms (the` |
|       - |  593 | `		 * recorded non-deprecated-surface policy, §2). */` |
|  436770 |  594 | `		if( pKey && (pTos->iFlags & MEMOBJ_HASHMAP) ){` |
|       - |  595 | `			SyBlob sTypeMsg;` |
|       - |  596 | `			/* An object/array key is php's TypeError; a resource key warns and` |
|       - |  597 | `			 * becomes its integer id. Both used to be stringified silently. */` |
|   82480 |  598 | `			if( VmOffsetTypeRejected(&(*pVm),pKey,0,&sTypeMsg) ){` |
|       - |  599 | `				sxi32 rcSc;` |
|       7 |  600 | `				PH7_MemObjRelease(pKey);` |
|       7 |  601 | `				VmPopOperand(&pTos,1);` |
|       7 |  602 | `				rcSc = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|      11 |  603 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       7 |  604 | `				rc = rcSc;` |
|       7 |  605 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  606 | `			}` |
|   82474 |  607 | `			VmOffsetResourceWarn(&(*pVm),pKey);` |
|   82469 |  608 | `			if( (pKey->iFlags & MEMOBJ_REAL)` |
|   41243 |  609 | `			 && pKey->rVal != (ph7_real)(sxi64)pKey->rVal ){` |
|       - |  610 | `				sxi32 rcSc;` |
|       3 |  611 | `				const char *zErr = "Cannot access offset of type float on array";` |
|       3 |  612 | `				PH7_MemObjRelease(pKey);` |
|       3 |  613 | `				VmPopOperand(&pTos,1);` |
|       3 |  614 | `				rcSc = VmThrowFromVm(&(*pVm),"TypeError",zErr,(sxu32)SyStrlen(zErr));` |
|       3 |  615 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       3 |  616 | `				rc = rcSc;` |
|       3 |  617 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  618 | `			}` |
|   41234 |  619 | `		}` |
|  436762 |  620 | `	if( pTos->iFlags & MEMOBJ_AUX_STROFFSET ){` |
|       - |  621 | ``		/* The CONTAINER is a string offset: `$s[0][1] = 'x'`, `$s[0][] = 'x'`,`` |
|       - |  622 | ``		 * `$a['k'][0][1] = 'x'`. php refuses to reach inside one — `Cannot use string`` |
|       - |  623 | ``		 * offset as an array`, the same refusal the fetch path raises — and this is`` |
|       - |  624 | `		 * where the outermost level of an ordinary assignment arrives, its LOAD_IDX` |
|       - |  625 | `		 * folded into the store. The character read out of the string still carries` |
|       - |  626 | ``		 * the BASE STRING's slot, so the write landed ON the base: `$s = 'ab';`` |
|       - |  627 | ``		 * $s[0][1] = 'x';` left `$s === 'ax'`, in silence. */`` |
|       - |  628 | `		sxi32 rcSo;` |
|       9 |  629 | `		if( pKey ){` |
|       7 |  630 | `			PH7_MemObjRelease(pKey);` |
|       3 |  631 | `		}` |
|       9 |  632 | `		VmPopOperand(&pTos,1);` |
|       9 |  633 | `		rcSo = VmThrowFromVm(&(*pVm),"Error","Cannot use string offset as an array",` |
|       - |  634 | `			sizeof("Cannot use string offset as an array")-1);` |
|       9 |  635 | `		if( rcSo == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       9 |  636 | `		rc = rcSo;` |
|       9 |  637 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  638 | `	}` |
|  436754 |  639 | `	nIdx = pTos->nIdx;` |
|       - |  640 | `	{` |
|       - |  641 | `		/* ArrayAccess::offsetSet dispatch.` |
|       - |  642 | `		 * Container may be on the stack as MEMOBJ_OBJ, or referenced via` |
|       - |  643 | `		 * the backing variable slot at nIdx. */` |
|  436754 |  644 | `		ph7_class_instance *pInst = 0;` |
|  436754 |  645 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|     480 |  646 | `			pInst = (ph7_class_instance *)pTos->x.pOther;` |
|  436516 |  647 | `		}else if( nIdx != SXU32_HIGH ){` |
|  436202 |  648 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  436202 |  649 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_OBJ) ){` |
|     ! 0 |  650 | `				pInst = (ph7_class_instance *)pBacking->x.pOther;` |
|     ! 0 |  651 | `			}` |
|  218090 |  652 | `		}` |
|  436754 |  653 | `		if( pInst ){` |
|     480 |  654 | `			ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|     480 |  655 | `			if( pArrayAccess && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|       - |  656 | `				ph7_class_method *pMeth;` |
|       - |  657 | `				ph7_value sNullKey;` |
|       - |  658 | `				ph7_value *apArg[2];` |
|     468 |  659 | `				if( pInstr->iOp == PH7_OP_STORE_IDX_REF ){` |
|     ! 0 |  660 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - |  661 | `						"Cannot assign by reference to overloaded object");` |
|     ! 0 |  662 | `					if( pKey ){ PH7_MemObjRelease(pKey); }` |
|     ! 0 |  663 | `					VmPopOperand(&pTos,2); /* container + value */` |
|     ! 0 |  664 | `					VM_EXIT_BREAK;` |
|       - |  665 | `				}` |
|     468 |  666 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - |  667 | `					"offsetSet",sizeof("offsetSet")-1);` |
|       - |  668 | `				/* Pop container; pTos now points to the value */` |
|     468 |  669 | `				VmPopOperand(&pTos,1);` |
|     468 |  670 | `				if( pKey == 0 ){` |
|      17 |  671 | `					PH7_MemObjInit(&(*pVm),&sNullKey);` |
|      17 |  672 | `					apArg[0] = &sNullKey;` |
|      10 |  673 | `				}else{` |
|     454 |  674 | `					apArg[0] = pKey;` |
|       - |  675 | `				}` |
|     468 |  676 | `				apArg[1] = pTos;` |
|     468 |  677 | `				if( pMeth ){` |
|     468 |  678 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,0,2,apArg);` |
|     232 |  679 | `				}` |
|     468 |  680 | `				if( pKey ){` |
|     454 |  681 | `					PH7_MemObjRelease(pKey);` |
|     229 |  682 | `				}else{` |
|      17 |  683 | `					PH7_MemObjRelease(&sNullKey);` |
|       - |  684 | `				}` |
|       - |  685 | `				/* Pop the value */` |
|     468 |  686 | `				VmPopOperand(&pTos,1);` |
|     468 |  687 | `				VM_EXIT_BREAK;` |
|       - |  688 | `			}` |
|       - |  689 | `			/* Object without ArrayAccess: PHP throws a fatal Error rather` |
|       - |  690 | `			 * than silently coercing the object into a hashmap (which is` |
|       - |  691 | `			 * what the legacy PH7 fall-through would do via MemObjToHashmap` |
|       - |  692 | `			 * a few lines below). Match PHP -- and let a class whose READ` |
|       - |  693 | `			 * handler answers word its own refusal, which is how php's` |
|       - |  694 | ``			 * PDORow says `Cannot write to PDORow offset` and, for the`` |
|       - |  695 | ``			 * keyless spelling, `Cannot append to PDORow offset`. */`` |
|       - |  696 | `			{` |
|       - |  697 | `				char zMsg[256];` |
|      20 |  698 | `				sxu32 nMsg = PH7_ClassNativeDimRefusal(pInst,` |
|       6 |  699 | `					pKey == 0 ? PH7_NATIVE_DIM_APPEND : PH7_NATIVE_DIM_WRITE,` |
|       6 |  700 | `					zMsg,sizeof(zMsg));` |
|      14 |  701 | `				rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|      14 |  702 | `				if( pKey ){ PH7_MemObjRelease(pKey); }` |
|      14 |  703 | `				VmPopOperand(&pTos,2); /* container + value */` |
|      14 |  704 | `				if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      14 |  705 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  706 | `			}` |
|       - |  707 | `		}` |
|       - |  708 | `	}` |
|  436278 |  709 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       - |  710 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|       - |  711 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|       - |  712 | `		 * checking true sharing count, then re-add after separation. */` |
|  436036 |  713 | `		if( nIdx != SXU32_HIGH ){` |
|  435986 |  714 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  653985 |  715 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|  435986 |  716 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|       - |  717 | `				/* Only adjust refcount / perform COW if the backing variable` |
|       - |  718 | `				 * is still sharing the same hashmap instance. This mirrors` |
|       - |  719 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|       - |  720 | `				 * refcounts if the backing array was already separated. */` |
|  435986 |  721 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|  435986 |  722 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|  435986 |  723 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|  435986 |  724 | `					pMap->iRef++;  /* Re-add stack ref */` |
|  435986 |  725 | `					pTos->x.pOther = pMap;` |
|  217987 |  726 | `				}else{` |
|       - |  727 | `					/* Backing variable no longer points at pCur: skip COW here` |
|       - |  728 | `					 * and operate on the hashmap currently on the stack. */` |
|     ! 0 |  729 | `					pMap = pCur;` |
|       - |  730 | `				}` |
|  217987 |  731 | `			}else{` |
|     ! 0 |  732 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       - |  733 | `			}` |
|  217987 |  734 | `		}else{` |
|      51 |  735 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       - |  736 | `		}` |
|  436036 |  737 | `		if( pMap->iRef < 2 ){` |
|       - |  738 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|       - |  739 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|       - |  740 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|       - |  741 | `			 * no code checks iRef for COW decisions. */` |
|      49 |  742 | `			pMap->iRef = 2;` |
|      24 |  743 | `		}` |
|  218012 |  744 | `	}else{` |
|       - |  745 | `		ph7_value *pObj;` |
|     246 |  746 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     246 |  747 | `		if( pObj == 0 ){` |
|       - |  748 | `			/* No slot behind the container: this is a write THROUGH a TEMPORARY` |
|       - |  749 | ``			 * (`f()[0] = 5`, `f()[0][1] = 5`). php still screens the base's TYPE the`` |
|       - |  750 | `			 * same way it does for a variable — writing an index into an int, a` |
|       - |  751 | `			 * float, a resource or a bool is its catchable "Cannot use a scalar value` |
|       - |  752 | `			 * as an array" — and only the value it writes is discarded with the` |
|       - |  753 | `			 * temporary. PHL skipped the screen along with the write, so the whole` |
|       - |  754 | `			 * statement ran in silence. A NULL base keeps php's silence: the array it` |
|       - |  755 | `			 * vivifies into dies with the temporary, and so does an offset written` |
|       - |  756 | `			 * into a temporary STRING. */` |
|      27 |  757 | `			if( (pTos->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_RES\|MEMOBJ_BOOL)) != 0 ){` |
|       - |  758 | `				sxi32 rcSc;` |
|      13 |  759 | `				if( pKey ){` |
|      13 |  760 | `					PH7_MemObjRelease(pKey);` |
|       6 |  761 | `				}` |
|      13 |  762 | `				VmPopOperand(&pTos,1);` |
|      13 |  763 | `				rcSc = VmThrowFromVm(&(*pVm),"Error","Cannot use a scalar value as an array",` |
|       - |  764 | `					sizeof("Cannot use a scalar value as an array")-1);` |
|      13 |  765 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      13 |  766 | `				rc = rcSc;` |
|      25 |  767 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  768 | `			}` |
|      15 |  769 | `			if( pKey ){` |
|      15 |  770 | `			  PH7_MemObjRelease(pKey);` |
|       7 |  771 | `			}` |
|      15 |  772 | `			VmPopOperand(&pTos,1);` |
|      15 |  773 | `			VM_EXIT_BREAK;` |
|       - |  774 | `		}` |
|       - |  775 | `		/* Phase#1: Load the array */` |
|     220 |  776 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|     162 |  777 | `			VmPopOperand(&pTos,1);` |
|     162 |  778 | `			if( pKey == 0 ){` |
|       - |  779 | ``				/* `$s[] = 'x'` on a STRING: php raises the catchable Error`` |
|       - |  780 | `				 * "[] operator not supported for strings" and leaves the string` |
|       - |  781 | `				 * untouched. PHL silently APPENDED, so code that meant to build` |
|       - |  782 | `				 * an array from a variable holding a string quietly produced a` |
|       - |  783 | `				 * longer string instead of failing — a wrong answer, not a` |
|       - |  784 | `				 * missing diagnostic. */` |
|       - |  785 | `				SyBlob sErrMsg;` |
|       8 |  786 | `				SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       8 |  787 | `				SyBlobAppend(&sErrMsg,"[] operator not supported for strings",` |
|       - |  788 | `					sizeof("[] operator not supported for strings")-1);` |
|       8 |  789 | `				VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       8 |  790 | `				VM_EXIT_BREAK;` |
|     ! 0 |  791 | `			}else{` |
|     156 |  792 | `				sxi64 iOfft = 0;` |
|       - |  793 | `				SyBlob sTypeMsg;` |
|       - |  794 | `				/* php's offset rules run BEFORE the RHS is looked at: an offset it` |
|       - |  795 | `				 * refuses is the TypeError alone. The RHS cast below used to happen` |
|       - |  796 | ``				 * first, so every rejected shape — and `$s[] = [1,2]` above — came`` |
|       - |  797 | ``				 * with a spurious `Array to string conversion` in front of it. */`` |
|     156 |  798 | `				if( VmStringOffsetResolve(&(*pVm),pKey,VM_STROFF_LOUD,&iOfft,&sTypeMsg) == VM_STROFF_REJECT ){` |
|       - |  799 | `					sxi32 rcSc;` |
|       7 |  800 | `					PH7_MemObjRelease(pKey);` |
|       7 |  801 | `					rcSc = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|      16 |  802 | `					if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       7 |  803 | `					rc = rcSc;` |
|       7 |  804 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  805 | `				}` |
|       - |  806 | `				/* Force a string cast on the RHS (user-visible: an array warns` |
|       - |  807 | `				 * "Array to string conversion" before the offset write, §2, and a` |
|       - |  808 | `				 * not-stringable object throws — the target string is untouched) */` |
|       - |  809 | `				{` |
|     150 |  810 | `					sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|     150 |  811 | `					if( rcSv != SXRET_OK ){` |
|       5 |  812 | `						PH7_MemObjRelease(pKey);` |
|       7 |  813 | `						PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|     ! 0 |  814 | `					}` |
|       - |  815 | `				}` |
|     145 |  816 | `				if( VmStringOffsetWrite(&(*pVm),pObj,iOfft,pTos) != SXRET_OK ){` |
|       - |  817 | `					sxi32 rcEm;` |
|       9 |  818 | `					PH7_MemObjRelease(pKey);` |
|       9 |  819 | `					rcEm = VmThrowFromVm(&(*pVm),"Error",` |
|       - |  820 | `						"Cannot assign an empty string to a string offset",` |
|       - |  821 | `						sizeof("Cannot assign an empty string to a string offset")-1);` |
|       9 |  822 | `					if( rcEm == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       9 |  823 | `					rc = rcEm;` |
|      11 |  824 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  825 | `				}` |
|       - |  826 | `			}` |
|     137 |  827 | `			if( pKey ){` |
|     137 |  828 | `			  PH7_MemObjRelease(pKey);` |
|      67 |  829 | `			}` |
|     137 |  830 | `			VM_EXIT_BREAK;` |
|      62 |  831 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       - |  832 | `			/* php: only NULL and FALSE auto-vivify into an array. Writing an index into an` |
|       - |  833 | `			 * int/float/resource/TRUE is a catchable Error -- PH7 quietly REPLACED the value` |
|       - |  834 | `			 * with an array, destroying it ($x = 5; $x[0] = 1; left $x === [1]). */` |
|       - |  835 | `			/* php auto-vivifies false into an array with an 8.1 DEPRECATION; PHL` |
|       - |  836 | `			 * rejects any scalar base, false included (null still auto-vivifies). */` |
|      62 |  837 | `			int bScalar = (pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_RES\|MEMOBJ_BOOL)) != 0;` |
|      62 |  838 | `			if( bScalar ){` |
|       - |  839 | `				sxi32 rcSc;` |
|      10 |  840 | `				if( pKey ){` |
|       7 |  841 | `					PH7_MemObjRelease(pKey);` |
|       3 |  842 | `				}` |
|      10 |  843 | `				VmPopOperand(&pTos,1);` |
|      10 |  844 | `				rcSc = VmThrowFromVm(&(*pVm),"Error","Cannot use a scalar value as an array",` |
|       - |  845 | `					sizeof("Cannot use a scalar value as an array")-1);` |
|      10 |  846 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      10 |  847 | `				rc = rcSc;` |
|      12 |  848 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  849 | `			}` |
|       - |  850 | `			/* Force a hashmap cast  */` |
|      54 |  851 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      54 |  852 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  853 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|     ! 0 |  854 | `				VM_EXIT_ABORT;` |
|       - |  855 | `			}` |
|      25 |  856 | `		}` |
|       - |  857 | `		/* COW separate the backing variable before mutation */` |
|      54 |  858 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|       - |  859 | `	}` |
|  436086 |  860 | `	VmPopOperand(&pTos,1);` |
|       - |  861 | `	/* Phase#2: Perform the insertion. A null key deprecates + normalizes to "" for` |
|       - |  862 | `	 * BOTH the by-value and the by-ref store — emitted HERE, after a null/false` |
|       - |  863 | ``	 * container has auto-vivified to an array, so `$x[null]=v` and `$x[null]=&$y` on`` |
|       - |  864 | `	 * an undefined $x get the notice too (the top-of-handler check runs before` |
|       - |  865 | `	 * vivification, when the base is not yet a hashmap). HashmapInsert /` |
|       - |  866 | `	 * HashmapInsertByRef both then cast NULL->"". A null pKey==0 (an append, no key)` |
|       - |  867 | `	 * is not a null OFFSET and is left alone. */` |
|  436086 |  868 | `	VmNullOffsetDeprecate(&(*pVm),pKey);` |
|  436081 |  869 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF` |
|  218063 |  870 | `	 && (pInstr->iP1 & PH7_STOREREF_CALLSRC)` |
|      35 |  871 | `	 && (pTos->iFlags & MEMOBJ_AUX_STROFFSET) == 0` |
|      13 |  872 | `	 && pTos->nIdx == SXU32_HIGH ){` |
|       - |  873 | ``		/* `$a[] =& f()` / `$a[$k] =& f()`: the source was written as a CALL and the`` |
|       - |  874 | `		 * callee did not return by reference, so php binds the value it answered and` |
|       - |  875 | ``		 * says so. Same notice the plain `$r =& f()` bind raises. */`` |
|       5 |  876 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|       - |  877 | `			"Only variables should be assigned by reference");` |
|       2 |  878 | `	}` |
|  436086 |  879 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && (pTos->iFlags & MEMOBJ_AUX_STROFFSET) ){` |
|       - |  880 | ``		/* `$a[] = &$s[1]`: the source is a string OFFSET, which php refuses to`` |
|       - |  881 | `		 * reference — and whose slot index is the BASE STRING's, so binding it` |
|       - |  882 | ``		 * aliased the whole string. Same Error the `=&` / by-ref-argument paths`` |
|       - |  883 | `		 * raise. The key is already popped; drop it and abandon the store. */` |
|       - |  884 | `		sxi32 rcSc;` |
|       5 |  885 | `		if( pKey ){` |
|       3 |  886 | `			PH7_MemObjRelease(pKey);` |
|       1 |  887 | `		}` |
|       5 |  888 | `		rcSc = VmThrowFromVm(&(*pVm),"Error","Cannot create references to/from string offsets",` |
|       - |  889 | `			sizeof("Cannot create references to/from string offsets")-1);` |
|       5 |  890 | `		if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       5 |  891 | `		rc = rcSc;` |
|       5 |  892 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  893 | `	}` |
|  436082 |  894 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|      56 |  895 | `		if( pMap == pVm->pGlobal ){` |
|       - |  896 | `			/* php 8.1: $GLOBALS['y'] =& $x binds the global $y to $x's` |
|       - |  897 | `			 * slot; an append has no name to bind (catchable Error). */` |
|       5 |  898 | `			if( pKey == 0 ){` |
|     ! 0 |  899 | `				rc = PH7_VmThrowGlobalsAppendError(&(*pVm));` |
|     ! 0 |  900 | `			}else{` |
|       5 |  901 | `				if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 |  902 | `					PH7_MemObjToString(pKey);` |
|     ! 0 |  903 | `				}` |
|       5 |  904 | `				if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|       - |  905 | `					/* Pathological empty name: keep the legacy diagnostic */` |
|     ! 0 |  906 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|       - |  907 | `						"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 |  908 | `					rc = SXRET_OK;` |
|     ! 0 |  909 | `				}else{` |
|       7 |  910 | `					rc = PH7_VmInstallGlobalVar(&(*pVm),` |
|       4 |  911 | `						(const char *)SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),` |
|       4 |  912 | `						0,pTos->nIdx);` |
|       - |  913 | `				}` |
|       - |  914 | `			}` |
|       3 |  915 | `		}else{` |
|       - |  916 | `			/* Insertion by reference */` |
|      52 |  917 | `			rc = PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|       - |  918 | `		}` |
|      29 |  919 | `	}else{` |
|  436028 |  920 | `		rc = PH7_HashmapInsert(pMap,pKey,pTos);` |
|       - |  921 | `	}` |
|  436082 |  922 | `	if( pKey ){` |
|   82500 |  923 | `		PH7_MemObjRelease(pKey);` |
|   41248 |  924 | `	}` |
|       - |  925 | `	/* An append onto the occupied saturated auto-index threw php's catchable` |
|       - |  926 | `	 * Error (PH7_VmThrowArrayNextIndexError) — dispatch it like any other` |
|       - |  927 | `	 * store-path throw. Plain failures (OOM) keep their existing routes. */` |
|  436084 |  928 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|  436078 |  929 | `	VM_EXIT_BREAK;` |
|     ! 0 |  930 | `	VM_EXIT_BREAK;` |
|  218379 |  931 | `}` |
|       - |  932 |  |
|       - |  933 | `/*` |
|       - |  934 | ` * OP_LOAD_CLOSURE: body moved verbatim from the OP_LOAD_CLOSURE arm of` |
|       - |  935 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  936 | ` */` |
|   12472 |  937 | `PH7_PRIVATE VmOpRc VmExecOpLoadClosure(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  938 | `{` |
|   12477 |  939 | `	ph7_value *pTos = pState->pTos;` |
|   12477 |  940 | `	ph7_value *pStack = pState->pStack;` |
|   12477 |  941 | `	VmInstr *aInstr = pState->aInstr;` |
|   12477 |  942 | `	sxi32 pc = pState->pc;` |
|       - |  943 | `	sxi32 rc;` |
|    6236 |  944 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|   12477 |  945 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|       - |  946 | `	/* The function whose name the Closure object will wrap: a fresh per-instantiation` |
|       - |  947 | `	 * copy for a real closure (built below), or the shared lambda function itself for a` |
|       - |  948 | `	 * plain anonymous function with no captured environment. */` |
|   12477 |  949 | `	ph7_vm_func *pTarget = pFunc;` |
|       - |  950 | `	/* A no-capture lambda declared inside a class method is not VM_FUNC_CLOSURE` |
|       - |  951 | `	 * (its env is empty), yet php still binds the creation-site class as its scope` |
|       - |  952 | ``	 * so `self::`/private access inside the body works. Detect the enclosing class`` |
|       - |  953 | `	 * here and route such a lambda through the per-instance closure path (a global` |
|       - |  954 | `	 * no-capture lambda peeks NULL and stays a shared function). */` |
|   12477 |  955 | `	ph7_class *pLoadScope = (pFunc->iFlags & VM_FUNC_CLOSURE) ? 0 : PH7_VmPeekDeclaringClass(pVm);` |
|   12477 |  956 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) \|\| pLoadScope ){` |
|       - |  957 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|       - |  958 | `		ph7_vm_func *pClosure;` |
|       - |  959 | `		char *zName;` |
|       - |  960 | `		sxu32 mLen;` |
|       - |  961 | `		sxu32 n;` |
|       - |  962 | `		/* Create a new VM function */` |
|   12275 |  963 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|       - |  964 | `		/* Generate an unique closure name */` |
|   12275 |  965 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|   12275 |  966 | `		if( pClosure == 0 \|\| zName == 0){` |
|     ! 0 |  967 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|     ! 0 |  968 | `			VM_EXIT_ABORT;` |
|       - |  969 | `		}` |
|   12275 |  970 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|   12275 |  971 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|     ! 0 |  972 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|     ! 0 |  973 | `		}` |
|       - |  974 | `		/* Zero the stucture */` |
|   12275 |  975 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|       - |  976 | `		/* Perform a structure assignment on read-only items */` |
|   12275 |  977 | `		pClosure->aArgs = pFunc->aArgs;` |
|   12275 |  978 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|   12275 |  979 | `		pClosure->aStatic = pFunc->aStatic;` |
|   12275 |  980 | `		pClosure->iFlags = pFunc->iFlags;` |
|       - |  981 | `		/* An in-class no-capture lambda routed here (pLoadScope set) must be a` |
|       - |  982 | `		 * real closure so PH7_VmClassMemberAccess honors its pUserData scope. */` |
|   12275 |  983 | `		pClosure->iFlags \|= VM_FUNC_CLOSURE;` |
|   12275 |  984 | `		pClosure->pUserData = pFunc->pUserData;` |
|   12275 |  985 | `		pClosure->sSignature = pFunc->sSignature;` |
|   12275 |  986 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|   12275 |  987 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|   12275 |  988 | `		pClosure->aReturnUnion = pFunc->aReturnUnion;` |
|   12275 |  989 | `		pClosure->sReturnTypeName = pFunc->sReturnTypeName;` |
|   12275 |  990 | `		pClosure->bStrictTypes = pFunc->bStrictTypes;` |
|   12275 |  991 | `		pClosure->nMaxStack = pFunc->nMaxStack;` |
|   12275 |  992 | `		if( pClosure->pUserData == 0 ){` |
|       - |  993 | `			/* Stamp the creation-site class scope (see PH7_VmPeekDeclaringClass):` |
|       - |  994 | `			 * a closure made in a method — or in another closure, whose own stamp` |
|       - |  995 | `			 * the peek reads — resolves self::/parent:: against it like php. */` |
|   12275 |  996 | `			pClosure->pUserData = (void *)PH7_VmPeekDeclaringClass(pVm);` |
|    6135 |  997 | `		}` |
|       - |  998 | ``		/* Capture the creation-site late-static-binding class so `static::` inside`` |
|       - |  999 | `		 * the closure body resolves like php (the "called class", which may differ` |
|       - | 1000 | `		 * from the declaring scope stamped above — e.g. a closure made in an` |
|       - | 1001 | `		 * inherited method). A closure made outside any class captures NULL. */` |
|   12275 | 1002 | `		pClosure->pLsbClass = (void *)PH7_VmPeekTopClass(pVm);` |
|       - | 1003 | `		/* Reflection descriptor fields (getDocComment/getAttributes/getStartLine/` |
|       - | 1004 | `		 * getEndLine/getFileName read the INSTANTIATED copy via $__fn) */` |
|   12275 | 1005 | `		pClosure->aAttrs = pFunc->aAttrs;` |
|   12275 | 1006 | `		pClosure->sDoc = pFunc->sDoc;` |
|   12275 | 1007 | `		pClosure->sFile = pFunc->sFile;` |
|   12275 | 1008 | `		pClosure->nLine = pFunc->nLine;` |
|   12275 | 1009 | `		pClosure->nEndLine = pFunc->nEndLine;` |
|       - | 1010 | ``		/* php's visible `{closure:...}` name is a property of the DECLARATION, so every`` |
|       - | 1011 | `		 * per-instantiation copy answers the same one (php has a single op_array here). */` |
|   12275 | 1012 | `		pClosure->sClosureName = pFunc->sClosureName;` |
|   12275 | 1013 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|       - | 1014 | `		/* Register the closure */` |
|   12275 | 1015 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|       - | 1016 | `		/* Set up closure environment */` |
|   12275 | 1017 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|   12275 | 1018 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|   29801 | 1019 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|       - | 1020 | `			ph7_value *pValue;` |
|   17531 | 1021 | `			pEnv = &aEnv[n];` |
|   17531 | 1022 | `			sEnv.sName  = pEnv->sName;` |
|   17531 | 1023 | `			sEnv.iFlags = pEnv->iFlags;` |
|   17531 | 1024 | `			sEnv.nLine = pEnv->nLine;` |
|   17531 | 1025 | `			sEnv.nIdx = SXU32_HIGH;` |
|   17531 | 1026 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|   17526 | 1027 | `			if( (sEnv.iFlags & (VM_FUNC_ARG_BY_REF\|VM_FUNC_ARG_IGNORE)) == VM_FUNC_ARG_BY_REF` |
|    9226 | 1028 | `			 && !(SyStringLength(&sEnv.sName) == sizeof("this")-1` |
|     458 | 1029 | `				&& SyMemcmp(SyStringData(&sEnv.sName),"this",sizeof("this")-1) == 0) ){` |
|       - | 1030 | `				/* Capture by reference: bind the env entry to the variable's` |
|       - | 1031 | `				 * memory slot — creating a fresh null variable when missing,` |
|       - | 1032 | ``				 * as php does (`use (&$f)` before $f is assigned) — and pin`` |
|       - | 1033 | `				 * the slot past the creating frame's teardown so the closure` |
|       - | 1034 | `				 * can outlive its birth scope. The call-time env install` |
|       - | 1035 | `				 * aliases the name to this slot instead of copying a value. */` |
|     601 | 1036 | `				pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,TRUE);` |
|     601 | 1037 | `				if( pValue ){` |
|     601 | 1038 | `					sEnv.nIdx = pValue->nIdx;` |
|     601 | 1039 | `					VmPinMemObjSlot(pVm,pValue->nIdx);` |
|     298 | 1040 | `				}` |
|     303 | 1041 | `			}else{` |
|       - | 1042 | `				/* Standard pass by value */` |
|   16935 | 1043 | `				pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|   16935 | 1044 | `				if( pValue ){` |
|       - | 1045 | `					/* Copy imported value */` |
|    5055 | 1046 | `					PH7_MemObjStore(pValue,&sEnv.sValue);` |
|   14410 | 1047 | `				}else if( (sEnv.iFlags & (VM_FUNC_ARG_BY_REF\|VM_FUNC_ARG_IGNORE)) == 0` |
|    5980 | 1048 | `					&& !(SyStringLength(&sEnv.sName) == sizeof("this")-1` |
|      35 | 1049 | `						&& SyMemcmp(SyStringData(&sEnv.sName),"this",sizeof("this")-1) == 0) ){` |
|      71 | 1050 | `					if( pFunc->iFlags & VM_FUNC_ARROW ){` |
|       - | 1051 | `						/* An arrow function auto-captures free variables by value, but` |
|       - | 1052 | `						 * php does NOT capture one that is UNDEFINED at creation: the` |
|       - | 1053 | `						 * isolated body scope then simply has no such variable, so a` |
|       - | 1054 | `						 * read of it there raises the normal "Undefined variable"` |
|       - | 1055 | `						 * warning (and reflection's getClosureUsedVariables omits it).` |
|       - | 1056 | `						 * Skip installing the capture so the body READ — not the` |
|       - | 1057 | `						 * creation — warns, matching php. (A later assignment to the` |
|       - | 1058 | `						 * outer variable does not retro-capture: arrow scope is` |
|       - | 1059 | `						 * isolated.) An explicit by-value use() instead warns here and` |
|       - | 1060 | `						 * binds NULL, handled just below. */` |
|      61 | 1061 | `						continue;` |
|       - | 1062 | `					}` |
|       - | 1063 | ``					/* php reads a by-value `use ($q)` capture AT CLOSURE CREATION and`` |
|       - | 1064 | `					 * warns when the variable is undefined there (the by-ref form` |
|       - | 1065 | ``					 * `use (&$q)` above stays silent — it creates the binding). The`` |
|       - | 1066 | `					 * auto-injected $this (VM_FUNC_ARG_IGNORE) never warns. The` |
|       - | 1067 | `					 * capture still proceeds as NULL, as php does. php attributes the` |
|       - | 1068 | `					 * warning to the capture's own line (which can differ from the` |
|       - | 1069 | `					 * OP_LOAD_CLOSURE instruction line when the use-clause wraps), so` |
|       - | 1070 | `					 * borrow the recorded line for the emission and restore it. */` |
|      11 | 1071 | `					sxu32 nSavedLine = pVm->nCurLine;` |
|      11 | 1072 | `					if( sEnv.nLine ){` |
|      11 | 1073 | `						pVm->nCurLine = sEnv.nLine;` |
|       5 | 1074 | `					}` |
|      11 | 1075 | `					VmErrorFormat(pVm,PH7_CTX_WARNING,"Undefined variable $%z",&sEnv.sName);` |
|      11 | 1076 | `					pVm->nCurLine = nSavedLine;` |
|       5 | 1077 | `				}` |
|       - | 1078 | `			}` |
|       - | 1079 | `			/* Insert the imported variable */` |
|   17475 | 1080 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|    8740 | 1081 | `		}` |
|   12275 | 1082 | `		pTarget = pClosure;` |
|    6135 | 1083 | `	}` |
|       - | 1084 | `	/* Wrap the target function in a Closure object and push it. Its captured environment` |
|       - | 1085 | ``	 * (incl. any `$this`) stays in pTarget->aClosureEnv and is delivered by the normal call`` |
|       - | 1086 | `	 * path when the closure is dispatched by name. */` |
|   12477 | 1087 | `	pTos++;` |
|       - | 1088 | `	{` |
|   12477 | 1089 | `		ph7_class_instance *pCloObj = VmCreateClosure(pVm, &pTarget->sName, 0, 0);` |
|   12477 | 1090 | `		if( pCloObj ){` |
|   12477 | 1091 | `			pCloObj->iRef++;` |
|   12477 | 1092 | `			pTos->x.pOther = pCloObj;` |
|   12477 | 1093 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|    6241 | 1094 | `		}else{` |
|       - | 1095 | `			/* OOM fallback: the name string is still a usable callable. */` |
|     ! 0 | 1096 | `			PH7_MemObjStringAppend(pTos, pTarget->sName.zString, pTarget->sName.nByte);` |
|       - | 1097 | `		}` |
|       - | 1098 | `	}` |
|   12477 | 1099 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1100 | `	VM_EXIT_BREAK;` |
|    6241 | 1101 | `}` |
|       - | 1102 |  |
|       - | 1103 |  |
|       - | 1104 | `/*` |
|       - | 1105 | `` * TRUE when this LOAD_IDX feeds a `??`: the coalesce test is the very next`` |
|       - | 1106 | `` * instruction. php evaluates the whole left operand of `??` in isset-context,`` |
|       - | 1107 | ` * so the access must stay SILENT and, when it misses, must yield NULL — an` |
|       - | 1108 | ``  * out-of-range string offset that yielded "" instead made `$s[99] ?? $d` `` |
|       - | 1109 | ` * evaluate to "" rather than $d, a wrong answer rather than a stray notice.` |
|       - | 1110 | ` */` |
|  866332 | 1111 | `static int VmIdxFeedsCoalesce(const VmInstr *pInstr)` |
|       5 | 1112 | `{` |
|  866337 | 1113 | `	return (pInstr+1)->iOp == PH7_OP_NULLC \|\| (pInstr+1)->iOp == PH7_OP_NULLC_JMP;` |
|       5 | 1114 | `}` |
|       - | 1115 | `/*` |
|       - | 1116 | `` * php's string-offset STORE, shared by `$s[i] = v` (OP_STORE_IDX) and`` |
|       - | 1117 | `` * `$s[i] ??= v` (OP_NULLC_STORE — `??=` is not an assign-op, so php performs a`` |
|       - | 1118 | ` * real offset write there): resolve iRawOfft against the string's CURRENT length` |
|       - | 1119 | ` * (a negative offset counts back from the end and, when it still lands before the` |
|       - | 1120 | `` * start, warns `Illegal string offset` and writes NOTHING), refuse an EMPTY`` |
|       - | 1121 | ` * replacement, warn when more than one byte was handed over, PAD WITH SPACES up` |
|       - | 1122 | ` * to the offset, then write the first byte.` |
|       - | 1123 | ` *` |
|       - | 1124 | ` * pVal must ALREADY be a string: that coercion is user-visible (it warns for an` |
|       - | 1125 | ` * array, throws for a not-stringable object) and stays with the callers, which` |
|       - | 1126 | ` * are the only places that can route a throw. Answers SXRET_OK when the store` |
|       - | 1127 | `` * happened or was skipped, and SXERR_INVALID for php's `Cannot assign an empty`` |
|       - | 1128 | `` * string to a string offset` Error — raised by the caller for the same reason.`` |
|       - | 1129 | ` */` |
|     182 | 1130 | `PH7_PRIVATE sxi32 VmStringOffsetWrite(ph7_vm *pVm,ph7_value *pStr,sxi64 iRawOfft,ph7_value *pVal)` |
|       3 | 1131 | `{` |
|     185 | 1132 | `	sxi64 nLen = (sxi64)SyBlobLength(&pStr->sBlob);` |
|     185 | 1133 | `	sxi64 iOfft = iRawOfft;` |
|       - | 1134 | `	const char *zVal;` |
|     185 | 1135 | `	if( iOfft < 0 ){` |
|       - | 1136 | `		/* php 7.1: a negative offset writes back from the end. */` |
|       9 | 1137 | `		iOfft += nLen;` |
|       9 | 1138 | `		if( iOfft < 0 ){` |
|       7 | 1139 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Illegal string offset %qd",iRawOfft);` |
|       7 | 1140 | `			return SXRET_OK;` |
|       - | 1141 | `		}` |
|       1 | 1142 | `	}` |
|     179 | 1143 | `	if( SyBlobLength(&pVal->sBlob) < 1 ){` |
|       - | 1144 | ``		/* php refuses to write NOTHING into an offset — `$s[2] = ""`, and the`` |
|       - | 1145 | ``		 * `= null` / `= false` that stringify to "" — where PHL silently ignored`` |
|       - | 1146 | `		 * the store. */` |
|      13 | 1147 | `		return SXERR_INVALID;` |
|       - | 1148 | `	}` |
|     167 | 1149 | `	zVal = (const char *)SyBlobData(&pVal->sBlob);` |
|     167 | 1150 | `	if( SyBlobLength(&pVal->sBlob) > 1 ){` |
|      10 | 1151 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 1152 | `			"Only the first byte will be assigned to the string offset");` |
|       4 | 1153 | `	}` |
|     167 | 1154 | `	if( iOfft >= nLen ){` |
|       - | 1155 | `		/* php PADS WITH SPACES up to the offset. PH7 simply appended the byte, so` |
|       - | 1156 | `		 * "abc" with [6]="Z" became "abcZ" rather than "abc   Z" -- a silently` |
|       - | 1157 | `		 * wrong string. */` |
|       - | 1158 | `		sxi64 nPad;` |
|     217 | 1159 | `		for( nPad = nLen ; nPad < iOfft ; ++nPad ){` |
|     175 | 1160 | `			SyBlobAppend(&pStr->sBlob," ",sizeof(char));` |
|      88 | 1161 | `		}` |
|      43 | 1162 | `		SyBlobAppend(&pStr->sBlob,(const void *)zVal,sizeof(char));` |
|      22 | 1163 | `	}else{` |
|     125 | 1164 | `		char *zData = (char *)SyBlobData(&pStr->sBlob);` |
|     125 | 1165 | `		zData[iOfft] = zVal[0];` |
|       - | 1166 | `	}` |
|     167 | 1167 | `	return SXRET_OK;` |
|      94 | 1168 | `}` |
|       - | 1169 | `/*` |
|       - | 1170 | `` * Does this LOAD_IDX feed a `??=` (rather than a plain `??`)? The compiler emits`` |
|       - | 1171 | ` * the LHS peek, then OP_NULLC_JMP, then the RHS, then OP_NULLC_STORE — so the` |
|       - | 1172 | ` * NULLC_JMP right after is what distinguishes the assigning form, whose store` |
|       - | 1173 | ` * still has to happen when the peek answers null.` |
|       - | 1174 | ` */` |
|  866302 | 1175 | `static int VmIdxFeedsCoalesceAssign(const VmInstr *pInstr)` |
|       5 | 1176 | `{` |
|  866307 | 1177 | `	return (pInstr+1)->iOp == PH7_OP_NULLC_JMP;` |
|       5 | 1178 | `}` |
|       - | 1179 | `/*` |
|       - | 1180 | `` * Arm the write-back half of `$o[$k] op= v` on an ArrayAccess container. The`` |
|       - | 1181 | ` * value offsetGet just answered goes into a fresh SCRATCH memobj that pTos` |
|       - | 1182 | ` * points at, so the compound-assign op computes IN that slot the way it would` |
|       - | 1183 | ` * in an ordinary variable; the pending entry then makes the op's tail dispatch` |
|       - | 1184 | `` * `offsetSet($k, computed)`. The KEY gets a reserved slot of its own (pIdx is`` |
|       - | 1185 | ` * released as soon as this opcode returns, and the write happens one opcode` |
|       - | 1186 | `` * later); an ABSENT key — `$o[] op= v` — is carried as NULL, which is the value`` |
|       - | 1187 | ` * php hands both accessors for that shape.` |
|       - | 1188 | ` *` |
|       - | 1189 | ` * A failed reservation simply leaves the value unarmed: pTos keeps its` |
|       - | 1190 | ` * no-slot temp and the op falls back to the pre-existing refusal.` |
|       - | 1191 | ` */` |
|      52 | 1192 | `static void VmDimRmwArm(` |
|       - | 1193 | `	ph7_vm *pVm,` |
|       - | 1194 | `	ph7_class_instance *pInst,` |
|       - | 1195 | `	ph7_value *pIdx,` |
|       - | 1196 | `	ph7_value *pTos,` |
|       - | 1197 | `	void *pOwnerStack,` |
|       - | 1198 | `	void *pInstrs,` |
|       - | 1199 | `	sxu32 nPc` |
|       - | 1200 | `	)` |
|       1 | 1201 | `{` |
|       - | 1202 | `	ph7_value *pSlot;` |
|       - | 1203 | `	sxu32 nScratch;` |
|       - | 1204 | `	sxu32 nKey;` |
|       - | 1205 | `	VmHookRmw sRmw;` |
|      53 | 1206 | `	pSlot = PH7_ReserveMemObj(&(*pVm));` |
|      53 | 1207 | `	if( pSlot == 0 ){` |
|     ! 0 | 1208 | `		return;` |
|       - | 1209 | `	}` |
|      53 | 1210 | `	nScratch = pSlot->nIdx;` |
|      53 | 1211 | `	pSlot = PH7_ReserveMemObj(&(*pVm));` |
|      53 | 1212 | `	if( pSlot == 0 ){` |
|     ! 0 | 1213 | `		VmHookRmwFreeScratch(&(*pVm),nScratch);` |
|     ! 0 | 1214 | `		return;` |
|       - | 1215 | `	}` |
|      53 | 1216 | `	nKey = pSlot->nIdx;` |
|       - | 1217 | `	/* Reserving can GROW the aMemObj set, so address both slots by index from` |
|       - | 1218 | `	 * here on — the pointer the first reservation handed back may be stale. */` |
|      53 | 1219 | `	if( pIdx ){` |
|      53 | 1220 | `		PH7_MemObjStore(pIdx,pSlot);` |
|      26 | 1221 | `	}` |
|      53 | 1222 | `	pSlot = (ph7_value *)SySetAt(&pVm->aMemObj,nScratch);` |
|      53 | 1223 | `	if( pSlot == 0 ){` |
|     ! 0 | 1224 | `		VmHookRmwFreeScratch(&(*pVm),nKey);` |
|     ! 0 | 1225 | `		VmHookRmwFreeScratch(&(*pVm),nScratch);` |
|     ! 0 | 1226 | `		return;` |
|       - | 1227 | `	}` |
|      53 | 1228 | `	PH7_MemObjStore(pTos,pSlot);` |
|      53 | 1229 | `	sRmw.iKind = VM_HOOK_PEND_RMW_DIM;` |
|      53 | 1230 | `	sRmw.pThis = pInst;` |
|      53 | 1231 | `	sRmw.pAttr = 0;` |
|      53 | 1232 | `	sRmw.nBackIdx = nKey;` |
|      53 | 1233 | `	sRmw.nScratchIdx = nScratch;` |
|      53 | 1234 | `	SyBlobInit(&sRmw.sName,&pVm->sAllocator);` |
|      53 | 1235 | `	sRmw.pOwnerStack = pOwnerStack;` |
|      53 | 1236 | `	sRmw.pInstrs = pInstrs;` |
|      53 | 1237 | `	sRmw.nJmpPc = nPc;  /* the modify op ... */` |
|      53 | 1238 | `	sRmw.nPc = nPc;     /* ... is the whole window */` |
|      53 | 1239 | `	pInst->iRef++;` |
|      53 | 1240 | `	pTos->nIdx = nScratch;` |
|      53 | 1241 | `	SySetPut(&pVm->aHookRmw,(const void *)&sRmw);` |
|      27 | 1242 | `}` |
|       - | 1243 | `/*` |
|       - | 1244 | ` * Is this LOAD_IDX fetching the dimension for WRITING — php's BP_VAR_W / BP_VAR_RW` |
|       - | 1245 | ` * / BP_VAR_UNSET fetch, the one that asks the container for a slot to MODIFY` |
|       - | 1246 | ` * rather than a value to read?` |
|       - | 1247 | ` *` |
|       - | 1248 | ` * iP2 answers for most of it. The two shapes it cannot are the ones where the` |
|       - | 1249 | ` * fetch is compiled as a plain read and the NEXT instruction is what makes it a` |
|       - | 1250 | `` * write: binding a reference to the element (`$r = &$o[$k]`) and iterating it by`` |
|       - | 1251 | `` * reference (`foreach ($o[$k] as &$v)`). A compound assign is the reverse case —`` |
|       - | 1252 | ` * iP2 says write, but php compiles it to ASSIGN_DIM_OP, a read plus a write` |
|       - | 1253 | ` * through the container's own handlers (VmDimRmwArm), not a write FETCH.` |
|       - | 1254 | ` */` |
|     924 | 1255 | `static int VmIdxFetchForWrite(const VmInstr *pInstr,sxi32 iP2)` |
|       5 | 1256 | `{` |
|     929 | 1257 | `	const VmInstr *pNext = pInstr + 1;` |
|     929 | 1258 | `	if( iP2 == 1 ){` |
|     189 | 1259 | `		return !VmNextIsCompoundAssign(pNext);` |
|       - | 1260 | `	}` |
|     741 | 1261 | `	if( iP2 == VM_IDX_CTX_UNSET_BASE ){` |
|       - | 1262 | `		/* An INTERMEDIATE subscript of an unset chain: php fetches it for` |
|       - | 1263 | `		 * writing so the removal one level down can land. */` |
|       9 | 1264 | `		return 1;` |
|       - | 1265 | `	}` |
|     733 | 1266 | `	if( iP2 == 0 ){` |
|     513 | 1267 | `		if( pNext->iOp == PH7_OP_STORE_REF ){` |
|     ! 0 | 1268 | `			return 1;` |
|       - | 1269 | `		}` |
|     513 | 1270 | `		if( pNext->iOp == PH7_OP_FOREACH_INIT && pNext->p3 ){` |
|       5 | 1271 | `			return (((ph7_foreach_info *)pNext->p3)->iFlags & PH7_4EACH_STEP_REF) != 0;` |
|       - | 1272 | `		}` |
|     252 | 1273 | `	}` |
|     729 | 1274 | `	return 0;` |
|     467 | 1275 | `}` |
|       - | 1276 | `/*` |
|       - | 1277 | ` * Which fetch contexts may answer out of a WRITABLE container's own storage` |
|       - | 1278 | `` * (PH7_SplDimElemSlot, vm_builtin_spl.c) instead of through `offsetGet`?`` |
|       - | 1279 | ` *` |
|       - | 1280 | ` * The ones that ask for a VALUE and may go on to MODIFY it: a plain read — whose` |
|       - | 1281 | ` * result carries the element's slot exactly as an array element's does, which is` |
|       - | 1282 | ` * what lets a by-reference ARGUMENT bind it — a write-context fetch, and the` |
|       - | 1283 | `` * INTERMEDIATE step of an unset chain. isset()/empty()/`??`/`??=` must reach`` |
|       - | 1284 | ` * offsetExists, and the OUTERMOST unset must reach offsetUnset, so those keep the` |
|       - | 1285 | ` * accessor. (iP2 is the NORMALIZED context here: the deferred-argument record mode` |
|       - | 1286 | ` * has already become a plain read.)` |
|       - | 1287 | ` *` |
|       - | 1288 | ` * A COMPOUND assign is deliberately not one of them, which is why this asks` |
|       - | 1289 | `` * VmIdxFetchForWrite rather than testing iP2 == 1 itself: `$ao[k] op= v` is php's`` |
|       - | 1290 | ` * ASSIGN_DIM_OP on an OBJECT, and that one reads and writes through the accessors` |
|       - | 1291 | ` * whatever the read handler could have offered — a subclass overriding only` |
|       - | 1292 | `` * offsetSet sees its own method called for `+=` and not for `++`.`` |
|       - | 1293 | ` */` |
|     572 | 1294 | `static int VmDimFastFetchCtx(const VmInstr *pInstr,sxi32 iP2)` |
|       5 | 1295 | `{` |
|     577 | 1296 | `	return iP2 == 0 \|\| VmIdxFetchForWrite(pInstr,iP2);` |
|       5 | 1297 | `}` |
|       - | 1298 | `/*` |
|       - | 1299 | ` * Is this fetch asking the base to BE a container — the question php answers with` |
|       - | 1300 | `` * `Cannot use string offset as an array` when the base is a string offset?`` |
|       - | 1301 | ` *` |
|       - | 1302 | ` * Every context that reaches INTO the base to write, vivify or remove: the write` |
|       - | 1303 | ` * contexts (1, the read-modify-write among them) and both halves of an unset chain.` |
|       - | 1304 | `` * The LOOKUPS — a plain read, isset()/empty()/`??`, a destructure, a deferred`` |
|       - | 1305 | `` * argument — are php's own silence (`$x = $s[0][1]` reads a character out of a`` |
|       - | 1306 | `` * character) and are not this. Neither is the `??=` PEEK (3): it reads, and`` |
|       - | 1307 | `` * `$s[0][0] ??= 7` finds a character and never stores at all, so only the peek that`` |
|       - | 1308 | ` * comes back EMPTY is a write — that one is refused where the read lands.` |
|       - | 1309 | ` *` |
|       - | 1310 | ` * iP2 is the NORMALIZED context, so VM_IDX_CTX_RMW has already become 1.` |
|       - | 1311 | ` */` |
|      24 | 1312 | `static int VmIdxCtxIsContainerWrite(sxi32 iP2)` |
|       1 | 1313 | `{` |
|      25 | 1314 | `	return iP2 == 1 \|\| VM_IDX_IS_UNSET(iP2);` |
|       1 | 1315 | `}` |
|       - | 1316 | `/*` |
|       - | 1317 | `` * php's `Indirect modification of overloaded element of C has no effect`: the`` |
|       - | 1318 | ` * write-context fetch above landed on a container that answers with a COPY, so` |
|       - | 1319 | ` * whatever the rest of the expression writes is thrown away. php says so and` |
|       - | 1320 | ` * carries on.` |
|       - | 1321 | ` *` |
|       - | 1322 | ` * PHL had neither half. The notice was missing, and the copy was not a copy: a` |
|       - | 1323 | ` * userland offsetGet returns the container's own nested hashmap by COW, and` |
|       - | 1324 | ` * OP_STORE_IDX on a base with no slot index writes STRAIGHT INTO the shared map —` |
|       - | 1325 | `` * so `$o['a']['b'] = 9`, `$o['a'][] = 5` and `foreach ($o['a'] as &$v)` all`` |
|       - | 1326 | ` * modified the object php leaves untouched, silently. Separating the value here` |
|       - | 1327 | ` * is what makes the write land nowhere.` |
|       - | 1328 | ` *` |
|       - | 1329 | ` * php stays silent for an OBJECT, and so does this: an object is a handle, the` |
|       - | 1330 | ` * write through it is not lost, and nothing about it is indirect.` |
|       - | 1331 | ` */` |
|      50 | 1332 | `PH7_PRIVATE void PH7_VmOverloadedElemNotice(ph7_vm *pVm,ph7_class *pClass,ph7_value *pVal)` |
|       1 | 1333 | `{` |
|      51 | 1334 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       9 | 1335 | `		return;` |
|       - | 1336 | `	}` |
|      64 | 1337 | `	VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|       - | 1338 | `		"Indirect modification of overloaded element of %z has no effect",` |
|      21 | 1339 | `		&pClass->sName);` |
|      43 | 1340 | `	if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|      35 | 1341 | `		PH7_HashmapCowSeparate(&(*pVm),pVal);` |
|      17 | 1342 | `	}` |
|      26 | 1343 | `}` |
|       - | 1344 | `/*` |
|       - | 1345 | ` * OP_LOAD_IDX: body moved verbatim from the OP_LOAD_IDX arm of` |
|       - | 1346 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 1347 | ` */` |
| 1146214 | 1348 | `PH7_PRIVATE VmOpRc VmExecOpLoadIdx(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 1349 | `{` |
| 1146219 | 1350 | `	ph7_value *pTos = pState->pTos;` |
| 1146219 | 1351 | `	ph7_value *pStack = pState->pStack;` |
| 1146219 | 1352 | `	VmInstr *aInstr = pState->aInstr;` |
| 1146219 | 1353 | `	sxi32 pc = pState->pc;` |
|       - | 1354 | `	sxi32 rc;` |
|  574471 | 1355 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 1146219 | 1356 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
| 1146219 | 1357 | `	ph7_hashmap *pMap = 0;` |
|       - | 1358 | `	ph7_value *pIdx;` |
|       - | 1359 | `	/* D1 commit 2: iP2==9 is the deferred-record mode. It behaves exactly like a plain read` |
|       - | 1360 | `	 * (iP2==0) for base dispatch / the read tail, EXCEPT the dedicated block right after the` |
|       - | 1361 | `	 * index is popped, which — on a lookup MISS with a reachable base — captures the lvalue` |
|       - | 1362 | `	 * path (MEMOBJ_AUX_DEFPATH) instead of warning, and exits. Everything else sees iP2. */` |
|       - | 1363 | ``	/* A read-modify-write fetch (VM_IDX_CTX_RMW: `$a[k] += v`, `$a[k]++`) IS the`` |
|       - | 1364 | `	 * write context for everything below — it COW-separates, it vivifies a missing` |
|       - | 1365 | `	 * key, it refuses the same offset TYPES — so normalize it to 1 here and keep` |
|       - | 1366 | `	 * the single thing that separates the two: php READ the element first, so a key` |
|       - | 1367 | `	 * that was not there to read WARNS before it is created. */` |
| 1146219 | 1368 | `	int bRmwFetch = (pInstr->iP2 == VM_IDX_CTX_RMW);` |
| 1146219 | 1369 | `	int bRmwMiss = 0;` |
| 1146219 | 1370 | `	int bBaseStrOff = 0;` |
| 1146219 | 1371 | `	sxi32 iP2 = (pInstr->iP2 == 9) ? 0 : (bRmwFetch ? 1 : pInstr->iP2);` |
| 1146219 | 1372 | `	pIdx = 0;` |
| 1146219 | 1373 | `	if( pInstr->iP1 == 0 ){` |
|      50 | 1374 | `		if( !iP2){` |
|       - | 1375 | ``			/* `[]` with nothing to append INTO. Every placement php refuses is a compile`` |
|       - | 1376 | `			 * error now (compile.c), so the only shape that reaches here is the one php` |
|       - | 1377 | `			 * also settles at runtime: a call ARGUMENT, whose parameter may turn out to be` |
|       - | 1378 | `			 * by-reference (php appends and binds) or by-value (php's Error). Record the` |
|       - | 1379 | `			 * append as a step of the deferred lvalue path and let OP_CALL decide. */` |
|      13 | 1380 | `			if( pInstr->iP2 == 9 ){` |
|      13 | 1381 | `				VmDeferredPath *pPath = 0;` |
|      13 | 1382 | `				if( pTos->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|     ! 0 | 1383 | `					pPath = (VmDeferredPath *)pTos->x.pOther;` |
|     ! 0 | 1384 | `					if( VmDeferPathPushAppend(pPath) == SXRET_OK ){` |
|     ! 0 | 1385 | `						VM_EXIT_BREAK; /* carrier already on pTos */` |
|     ! 0 | 1386 | `					}` |
|      13 | 1387 | `				}else if( pTos->iFlags & MEMOBJ_AUX_DEFERRED ){` |
|       - | 1388 | `					SyString sRootName;` |
|       3 | 1389 | `					SyStringInitFromBuf(&sRootName,(const char *)pTos->x.pOther,` |
|       - | 1390 | `						pTos->x.pOther ? SyStrlen((const char *)pTos->x.pOther) : 0);` |
|       3 | 1391 | `					pPath = VmDeferPathNew(&(*pVm),1,SXU32_HIGH,&sRootName);` |
|       3 | 1392 | `					if( pPath && VmDeferPathPushAppend(pPath) == SXRET_OK ){` |
|       3 | 1393 | `						pTos->iFlags &= ~MEMOBJ_AUX_DEFERRED; /* borrowed name; don't free x.pOther */` |
|       3 | 1394 | `						pTos->x.pOther = pPath;` |
|       3 | 1395 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|       3 | 1396 | `						pTos->nIdx = SXU32_HIGH;` |
|       3 | 1397 | `						VM_EXIT_BREAK;` |
|       - | 1398 | `					}` |
|     ! 0 | 1399 | `					VmFreeDeferredPath(pPath);` |
|      11 | 1400 | `				}else if( pTos->nIdx != SXU32_HIGH ){` |
|      11 | 1401 | `					pPath = VmDeferPathNew(&(*pVm),0,pTos->nIdx,0);` |
|      11 | 1402 | `					if( pPath && VmDeferPathPushAppend(pPath) == SXRET_OK ){` |
|      11 | 1403 | `						PH7_MemObjRelease(pTos);` |
|      11 | 1404 | `						pTos->x.pOther = pPath;` |
|      11 | 1405 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|      11 | 1406 | `						pTos->nIdx = SXU32_HIGH;` |
|      11 | 1407 | `						VM_EXIT_BREAK;` |
|       - | 1408 | `					}` |
|     ! 0 | 1409 | `					VmFreeDeferredPath(pPath);` |
|     ! 0 | 1410 | `				}` |
|     ! 0 | 1411 | `			}` |
|       - | 1412 | `			/* Not a deferrable argument (or out of memory recording it): php's own` |
|       - | 1413 | `			 * Error, which replaced PH7's notice-and-NULL. */` |
|     ! 0 | 1414 | `			if( pTos >= pStack ){` |
|     ! 0 | 1415 | `				PH7_MemObjRelease(pTos);` |
|     ! 0 | 1416 | `			}else{` |
|       - | 1417 | `				/* TICKET 1433-020: Empty stack */` |
|     ! 0 | 1418 | `				pTos++;` |
|     ! 0 | 1419 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|     ! 0 | 1420 | `				pTos->nIdx = SXU32_HIGH;` |
|       - | 1421 | `			}` |
|       - | 1422 | `			{` |
|     ! 0 | 1423 | `			sxi32 rcRd = VmThrowFromVm(&(*pVm),"Error","Cannot use [] for reading",` |
|       - | 1424 | `				sizeof("Cannot use [] for reading")-1);` |
|     ! 0 | 1425 | `			if( rcRd == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     ! 0 | 1426 | `			rc = rcRd;` |
|     ! 0 | 1427 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1428 | `			}` |
|       - | 1429 | `		}` |
|      20 | 1430 | `	}else{` |
| 1146171 | 1431 | `		pIdx = pTos;` |
| 1146171 | 1432 | `		pTos--;` |
|       - | 1433 | `	}` |
| 1146207 | 1434 | `	if( VM_IDX_IS_UNSET(iP2) ){` |
|       - | 1435 | ``		/* `unset($o->p[$k])` reaches INTO what the property holds, which is php's`` |
|       - | 1436 | `		 * indirect modification of the property itself -- refused for a readonly` |
|       - | 1437 | `		 * one, and for a native property whose handler takes no write. The write` |
|       - | 1438 | ``		 * shapes (`$o->p[$k] = v`, `$o->p[] = v`) are screened at OP_MEMBER, which`` |
|       - | 1439 | `		 * knows them from its own context tag; an unset BASE is tagged as an` |
|       - | 1440 | `		 * ordinary read there and is only recognizable here. */` |
|    1669 | 1441 | `		sxi32 rcInd = PH7_VmCheckIndirectModify(&(*pVm),pTos->nIdx);` |
|    1669 | 1442 | `		if( rcInd != SXRET_OK ){` |
|       5 | 1443 | `			if( pIdx ){` |
|       5 | 1444 | `				PH7_MemObjRelease(pIdx);` |
|       2 | 1445 | `			}` |
|       5 | 1446 | `			PH7_MemObjRelease(pTos);` |
|       5 | 1447 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       5 | 1448 | `			pTos->nIdx = SXU32_HIGH;` |
|       5 | 1449 | `			if( rcInd == PH7_ABORT ){ VM_EXIT_ABORT; }` |
|       5 | 1450 | `			PH7_THROW_ROUTE_MIDEXPR(rcInd)` |
|       - | 1451 | `		}` |
|     830 | 1452 | `	}` |
| 1146203 | 1453 | `	if( pInstr->iP2 == 9 && pIdx ){` |
|       - | 1454 | `		/* D1 commit 2 record mode. Decide whether to CAPTURE this subscript as a deferred` |
|       - | 1455 | `		 * lvalue step (the by-ref/by-value decision is not known until OP_CALL), or fall` |
|       - | 1456 | `		 * through to a plain read (iP2 was normalized to 0 above). We defer only when the` |
|       - | 1457 | `		 * base can be reached again at resolve time: an existing descriptor (nested), the` |
|       - | 1458 | `		 * commit-1 undefined-variable marker, or a real container/string/scalar slot` |
|       - | 1459 | `		 * (nIdx != SXU32_HIGH). On a present array key we DO NOT defer — a hit already` |
|       - | 1460 | `		 * yields the aliasable read slot the by-ref binder needs. */` |
|  106041 | 1461 | `		VmDeferredPath *pPath = 0;` |
|  106041 | 1462 | `		int bDefer = 0, eRoot = 0;` |
|  106041 | 1463 | `		if( pTos->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|       - | 1464 | `			/* Nested: the base already carries a descriptor — extend it in place. */` |
|      28 | 1465 | `			pPath = (VmDeferredPath *)pTos->x.pOther;` |
|      28 | 1466 | `			bDefer = 1;` |
|  106028 | 1467 | `		}else if( pTos->iFlags & MEMOBJ_AUX_DEFERRED ){` |
|       - | 1468 | `			/* Undefined base variable (commit-1 marker): root the descriptor by name. */` |
|       - | 1469 | `			SyString sRootName;` |
|       3 | 1470 | `			SyStringInitFromBuf(&sRootName,(const char *)pTos->x.pOther,` |
|       - | 1471 | `				pTos->x.pOther ? SyStrlen((const char *)pTos->x.pOther) : 0);` |
|       3 | 1472 | `			pPath = VmDeferPathNew(&(*pVm),1,SXU32_HIGH,&sRootName);` |
|       3 | 1473 | `			pTos->iFlags &= ~MEMOBJ_AUX_DEFERRED; /* borrowed name; don't free x.pOther */` |
|       3 | 1474 | `			pTos->x.pOther = 0;` |
|       3 | 1475 | `			bDefer = (pPath != 0);` |
|  106014 | 1476 | `		}else if( pTos->nIdx != SXU32_HIGH ){` |
|       - | 1477 | `			/* A real base slot: array/scalar -> eRoot 0 (by-ref may vivify), string -> 2. */` |
|  105879 | 1478 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       - | 1479 | `				/* Probe hit/miss on a COPY of the key: PH7_HashmapLookup casts a NULL key to` |
|       - | 1480 | `				 * "" in place, which would suppress the fall-through read's null-offset` |
|       - | 1481 | `				 * deprecation (hit) and capture the wrong key in the step (miss). */` |
|       - | 1482 | `				ph7_value idxProbe;` |
|   60839 | 1483 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   60839 | 1484 | `				PH7_MemObjInit(&(*pVm),&idxProbe);` |
|   60839 | 1485 | `				PH7_MemObjStore(pIdx,&idxProbe);` |
|   60839 | 1486 | `				if( PH7_HashmapLookup(pMap,&idxProbe,&pNode) == SXRET_OK ){` |
|   60779 | 1487 | `					bDefer = 0; /* present key: fall through and read it as an aliasable slot */` |
|   30392 | 1488 | `				}else{` |
|      63 | 1489 | `					eRoot = 0; bDefer = 1;` |
|       - | 1490 | `				}` |
|   60839 | 1491 | `				PH7_MemObjRelease(&idxProbe);` |
|   75462 | 1492 | `			}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       - | 1493 | `				/* An ArrayAccess base splits the way php's read_dimension does. A WRITABLE` |
|       - | 1494 | `				 * container answers out of its own storage with no accessor call, so the` |
|       - | 1495 | `				 * fetch really can wait for the callee: deferring it is what lets a` |
|       - | 1496 | `				 * by-reference argument take php's WRITE fetch, which CREATES a missing key` |
|       - | 1497 | ``				 * (`sort($ao['nokey'])`) instead of warning about a read and passing NULL.`` |
|       - | 1498 | `				 * Everything else answers through a METHOD, and php runs that method where` |
|       - | 1499 | `				 * the subscript is WRITTEN — so the accessor runs below and its RESULT rides` |
|       - | 1500 | `				 * a prefetch carrier built at the tail of the ArrayAccess branch. */` |
|     159 | 1501 | `				ph7_class_instance *pRecInst = (ph7_class_instance *)pTos->x.pOther;` |
|     159 | 1502 | `				eRoot = 0;` |
|     317 | 1503 | `				bDefer = (pRecInst && pVm->pArrayAccessClass` |
|     158 | 1504 | `				       && PH7_VmInstanceOf(pRecInst->pClass,pVm->pArrayAccessClass)` |
|     237 | 1505 | `				       && PH7_VmDimFetchWritable(pRecInst->pClass)) ? 1 : 0;` |
|   44966 | 1506 | `			}else if( pTos->iFlags & MEMOBJ_STRING ){` |
|   44887 | 1507 | `				eRoot = 2; bDefer = 1;` |
|   22612 | 1508 | `			}else{` |
|     ! 0 | 1509 | `				eRoot = 0; bDefer = 1; /* NULL/other reachable scalar base */` |
|       - | 1510 | `			}` |
|  105879 | 1511 | `			if( bDefer && pPath == 0 ){` |
|   45021 | 1512 | `				pPath = VmDeferPathNew(&(*pVm),eRoot,pTos->nIdx,0);` |
|   45021 | 1513 | `				if( pPath == 0 ){` |
|     ! 0 | 1514 | `					bDefer = 0;` |
|     ! 0 | 1515 | `				}` |
|   22674 | 1516 | `			}` |
|   53103 | 1517 | `		}` |
|  106041 | 1518 | `		if( bDefer ){` |
|       - | 1519 | `			/* Append this element step (deep-copies pIdx) and leave the carrier on pTos. */` |
|   45049 | 1520 | `			if( VmDeferPathPushElem(pPath,pIdx) == SXRET_OK ){` |
|   45049 | 1521 | `				if( (pTos->iFlags & MEMOBJ_AUX_DEFPATH) == 0 ){` |
|       - | 1522 | `					/* Collapse the base value into the descriptor carrier. */` |
|   45023 | 1523 | `					PH7_MemObjRelease(pTos);` |
|   45023 | 1524 | `					pTos->x.pOther = pPath;` |
|   45023 | 1525 | `					pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|   45023 | 1526 | `					pTos->nIdx = SXU32_HIGH;` |
|   22675 | 1527 | `				}` |
|   45049 | 1528 | `				PH7_MemObjRelease(pIdx);` |
|   45049 | 1529 | `				VM_EXIT_BREAK;` |
|       - | 1530 | `			}` |
|       - | 1531 | `			/* Out of memory appending a step. */` |
|     ! 0 | 1532 | `			if( pTos->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|       - | 1533 | `				/* Nested base: pPath IS pTos's carrier and stays owned by it — keep it (a` |
|       - | 1534 | `				 * step short) and exit, rather than fall through and misread a NULL-typed` |
|       - | 1535 | `				 * carrier as an array base. Resolves the shorter path; OOM-only degradation. */` |
|     ! 0 | 1536 | `				PH7_MemObjRelease(pIdx);` |
|     ! 0 | 1537 | `				VM_EXIT_BREAK;` |
|       - | 1538 | `			}` |
|       - | 1539 | `			/* A freshly-allocated path we still own: drop it and fall back to a plain read. */` |
|     ! 0 | 1540 | `			VmFreeDeferredPath(pPath);` |
|     ! 0 | 1541 | `		}` |
|   30496 | 1542 | `	}` |
| 1101159 | 1543 | `	if( iP2 == 7 && (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|       - | 1544 | ``		/* Keyed list destructuring `["k"=>$v] = $src` from a NON-array source: yield NULL`` |
|       - | 1545 | `		 * (never char-index a string), warning once per key — matching PHP, which warns per` |
|       - | 1546 | `		 * key here. A NULL source is silent; unlike the positional OP_LOAD_LIST path, a bool` |
|       - | 1547 | `		 * source DOES warn (PHP warns for bool in keyed destructuring).` |
|       - | 1548 | `		 * An OBJECT is not one of these: php destructures it through its` |
|       - | 1549 | `		 * read_dimension handler like any other subscript, so it falls through to` |
|       - | 1550 | `		 * the object dispatch below — which answers out of the accessor and` |
|       - | 1551 | ``		 * raises php's `Cannot use object of type C as array` for a class that`` |
|       - | 1552 | ``		 * has none. `["k"=>$v] = $obj` warned and yielded NULL for every source`` |
|       - | 1553 | `		 * but the one shape (a writable container answering out of its own` |
|       - | 1554 | `		 * storage) that reached the fast path underneath. */` |
|       7 | 1555 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       5 | 1556 | `			VmWarnCannotUseAsArray(&(*pVm),pTos->iFlags);` |
|       2 | 1557 | `		}` |
|       7 | 1558 | `		if( pIdx ){` |
|       - | 1559 | `			/* Release the key (a string literal for keyed destructuring), like the` |
|       - | 1560 | `			 * normal hashmap-read exit below — otherwise its blob is orphaned. */` |
|       7 | 1561 | `			PH7_MemObjRelease(pIdx);` |
|       3 | 1562 | `		}` |
|       7 | 1563 | `		PH7_MemObjRelease(pTos);` |
|       7 | 1564 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|       7 | 1565 | `		VM_EXIT_BREAK;` |
|       - | 1566 | `	}` |
| 1101153 | 1567 | `	bBaseStrOff = (pTos->iFlags & MEMOBJ_AUX_STROFFSET) != 0;` |
| 1101153 | 1568 | `	if( bBaseStrOff && VmIdxCtxIsContainerWrite(iP2) ){` |
|       - | 1569 | `		/* A string OFFSET used as a CONTAINER. php reaches a string offset through a` |
|       - | 1570 | `		 * marker zval and lets whichever opcode CONSUMES it name the refusal` |
|       - | 1571 | ``		 * (`zend_wrong_string_offset_error`); a fetch that wants to reach INSIDE it —`` |
|       - | 1572 | ``		 * `$s[0][1] = 'x'`, `$s[0][1] += 1`, `unset($s[0][1])`,`` |
|       - | 1573 | ``		 * `$s[0][] = 'x'`, a by-reference argument — is `Cannot use string offset as`` |
|       - | 1574 | ``		 * an array`. PHL read the character and handed back a value still carrying the`` |
|       - | 1575 | ``		 * BASE STRING's slot, so the write landed on the base: `$s = 'ab';`` |
|       - | 1576 | ``		 * $s[0][1] = 'x';` left `$s === 'ax'` and `$a['k'][0][1] = 'x'` rewrote the`` |
|       - | 1577 | ``		 * ELEMENT. A READ (`$x = $s[0][1]`) and the lookup contexts are php's own`` |
|       - | 1578 | `		 * silence and stay out of this. */` |
|       9 | 1579 | `		if( pIdx ){` |
|       9 | 1580 | `			PH7_MemObjRelease(pIdx);` |
|       4 | 1581 | `		}` |
|       9 | 1582 | `		rc = VmThrowFromVm(&(*pVm),"Error","Cannot use string offset as an array",` |
|       - | 1583 | `			sizeof("Cannot use string offset as an array")-1);` |
|       9 | 1584 | `		PH7_MemObjRelease(pTos);` |
|       9 | 1585 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|       9 | 1586 | `		pTos->nIdx = SXU32_HIGH;` |
|       9 | 1587 | `		if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       9 | 1588 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1589 | `	}` |
| 1101145 | 1590 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|       - | 1591 | `		/* String access */` |
|  866323 | 1592 | `		if( pIdx == 0 && VmIdxCtxIsContainerWrite(iP2) ){` |
|       - | 1593 | ``			/* `$s[] op= v` / `$s[]++`: php refuses an APPEND to a string wherever it`` |
|       - | 1594 | ``			 * lands — `[] operator not supported for strings` — and the plain`` |
|       - | 1595 | ``			 * `$s[] = 'x'` store already raises it (OP_STORE_IDX). The`` |
|       - | 1596 | `			 * read-modify-write spellings fell through to "load NULL" here and then` |
|       - | 1597 | `` 			 * wrote the computed value back through the BASE's slot, so `$s[] .= 'x'` `` |
|       - | 1598 | ``			 * APPENDED to the string and `$s[]++` incremented the whole of it. */`` |
|       5 | 1599 | `			rc = VmThrowFromVm(&(*pVm),"Error","[] operator not supported for strings",` |
|       - | 1600 | `				sizeof("[] operator not supported for strings")-1);` |
|       5 | 1601 | `			PH7_MemObjRelease(pTos);` |
|       5 | 1602 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       5 | 1603 | `			pTos->nIdx = SXU32_HIGH;` |
|       5 | 1604 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       5 | 1605 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1606 | `		}` |
|  866319 | 1607 | `		if( iP2 == VM_IDX_CTX_UNSET ){` |
|       - | 1608 | ``			/* php: a string offset cannot be unset AT ALL — `unset($s[0])` is the`` |
|       - | 1609 | ``			 * catchable `Error: Cannot unset string offsets`, whatever the offset is`` |
|       - | 1610 | `			 * and whether or not it is in range. PHL read the character but left the` |
|       - | 1611 | `			 * BASE VARIABLE's slot index on the result, so the trailing unset()` |
|       - | 1612 | ``			 * builtin freed the base itself: `$s = "abc"; unset($s[1]);` left $s`` |
|       - | 1613 | ``			 * UNDEFINED, and `unset($a["k"][0])` deleted the whole element.`` |
|       - | 1614 | ``			 * An INTERMEDIATE level of the chain (`unset($s[0][1])`,`` |
|       - | 1615 | ``			 * `unset($s[0]->p)`) is NOT this: that fetch hands the offset on to`` |
|       - | 1616 | `			 * something that reaches INSIDE it, and php words the refusal from` |
|       - | 1617 | `			 * whatever that is — so it falls through to the offset resolution below` |
|       - | 1618 | `			 * (a write-shaped fetch) and the consumer raises. */` |
|      14 | 1619 | `			rc = VmThrowFromVm(&(*pVm),"Error","Cannot unset string offsets",` |
|       - | 1620 | `				sizeof("Cannot unset string offsets")-1);` |
|      14 | 1621 | `			if( pIdx ){` |
|      14 | 1622 | `				PH7_MemObjRelease(pIdx);` |
|       6 | 1623 | `			}` |
|      14 | 1624 | `			PH7_MemObjRelease(pTos);` |
|      14 | 1625 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      14 | 1626 | `			pTos->nIdx = SXU32_HIGH;` |
|      14 | 1627 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      14 | 1628 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1629 | `		}` |
|  866307 | 1630 | `		if( pIdx ){` |
|  866307 | 1631 | `			sxi64 iOfft = 0, iRaw;` |
|  866307 | 1632 | `			sxi64 nLen = (sxi64)SyBlobLength(&pTos->sBlob);` |
|       - | 1633 | `			/* LOAD_IDX carries its OWN iP2 codes, which do NOT line up with the` |
|       - | 1634 | ``			 * PH7_MEMBER_* ones: 4 = isset, 5 = unset, 6 = empty, 8 = `??` read,`` |
|       - | 1635 | ``			 * 3 = `??=` peek. All are LOOKUPS, but php splits them into TWO`` |
|       - | 1636 | `			 * levels: isset()/empty()/unset() say nothing at all, while a` |
|       - | 1637 | ``			 * `??`/`??=` fetch still warns about the offset SHAPE and reads it`` |
|       - | 1638 | `			 * (VM_STROFF_COALESCE). The ??= peek is recognised by the NULLC_JMP` |
|       - | 1639 | `			 * that follows it, since its iP2 does not distinguish the base type. */` |
| 1300593 | 1640 | `			int iOfftLevel = (iP2 == 4 \|\| iP2 == VM_IDX_CTX_UNSET \|\| iP2 == 6) ? VM_STROFF_ISSET` |
| 1732541 | 1641 | `				: (iP2 == VM_IDX_CTX_UNSET_BASE ? VM_STROFF_UNSETBASE` |
| 1730139 | 1642 | `				: ((iP2 == 8 \|\| VmIdxFeedsCoalesce(pInstr)) ? VM_STROFF_COALESCE` |
|  863883 | 1643 | `				: VM_STROFF_LOUD));` |
|  866307 | 1644 | `			int bQuiet = iOfftLevel != VM_STROFF_LOUD;` |
|       - | 1645 | `			SyBlob sTypeMsg;` |
|       - | 1646 | `			int eOfft;` |
|  866307 | 1647 | `			VmCoalStrOff *pCoalOff = 0;` |
|  866307 | 1648 | `			if( VmIdxFeedsCoalesceAssign(pInstr) ){` |
|       - | 1649 | ``				/* `$s[k] ??= v`: the OP_NULLC_STORE ahead has to write into the`` |
|       - | 1650 | `				 * string OFFSET, and by then the offset is gone — this op consumes` |
|       - | 1651 | `				 * it. Carry the RAW index to the store on the peek's own result` |
|       - | 1652 | `				 * (MEMOBJ_AUX_COALSTROFF), which nests and cannot leak; the copy` |
|       - | 1653 | `				 * must predate the resolution below, which casts a float/null/bool` |
|       - | 1654 | `				 * in place, because php re-resolves the offset LOUDLY at the store:` |
|       - | 1655 | `				 * the peek is the quiet half of its pair. */` |
|      55 | 1656 | `				pCoalOff = VmCoalStrOffNew(&(*pVm),pIdx);` |
|      27 | 1657 | `			}` |
|  866307 | 1658 | `			eOfft = VmStringOffsetResolve(&(*pVm),pIdx,iOfftLevel,&iOfft,&sTypeMsg);` |
|  866307 | 1659 | `			if( eOfft == VM_STROFF_MISS ){` |
|       - | 1660 | `				/* A lookup over an offset php refuses: not set, in silence. */` |
|      32 | 1661 | `				PH7_MemObjRelease(pIdx);` |
|      32 | 1662 | `				PH7_MemObjRelease(pTos);` |
|      32 | 1663 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      32 | 1664 | `				if( pCoalOff && bBaseStrOff ){` |
|       - | 1665 | `					/* The store this peek is arming would land INSIDE a string offset:` |
|       - | 1666 | `					 * refused, like every other reach-inside (the same answer the` |
|       - | 1667 | `					 * out-of-range peek below gets). */` |
|     ! 0 | 1668 | `					VmFreeCoalStrOff(pCoalOff);` |
|     ! 0 | 1669 | `					rc = VmThrowFromVm(&(*pVm),"Error","Cannot use string offset as an array",` |
|       - | 1670 | `						sizeof("Cannot use string offset as an array")-1);` |
|     ! 0 | 1671 | `					pTos->nIdx = SXU32_HIGH;` |
|      35 | 1672 | `					if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     ! 0 | 1673 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1674 | `				}` |
|      32 | 1675 | `				if( pCoalOff ){` |
|       - | 1676 | ``					/* A `??=` whose offset the READ refuses: php raises at the`` |
|       - | 1677 | `					 * STORE instead, so keep the base slot reachable and hand the` |
|       - | 1678 | `					 * offset to OP_NULLC_STORE. */` |
|       3 | 1679 | `					pTos->x.pOther = (void *)pCoalOff;` |
|       3 | 1680 | `					pTos->iFlags \|= MEMOBJ_AUX_STROFFSET\|MEMOBJ_AUX_COALSTROFF;` |
|       2 | 1681 | `				}else{` |
|      30 | 1682 | `					pTos->nIdx = SXU32_HIGH;` |
|       - | 1683 | `				}` |
|      32 | 1684 | `				VM_EXIT_BREAK;` |
|       - | 1685 | `			}` |
|  866277 | 1686 | `			if( eOfft == VM_STROFF_REJECT && iOfftLevel == VM_STROFF_UNSETBASE ){` |
|       - | 1687 | `				/* The intermediate step of an unset chain over an offset php cannot` |
|       - | 1688 | ``				 * use at all (`unset($s["k"][0])`, `unset($s[""]->p)`): the refusal`` |
|       - | 1689 | `				 * is the UNSET's, not the read's TypeError. */` |
|     ! 0 | 1690 | `				VmFreeCoalStrOff(pCoalOff);` |
|     ! 0 | 1691 | `				SyBlobRelease(&sTypeMsg);` |
|     ! 0 | 1692 | `				rc = VmThrowFromVm(&(*pVm),"Error","Cannot unset string offsets",` |
|       - | 1693 | `					sizeof("Cannot unset string offsets")-1);` |
|     ! 0 | 1694 | `				PH7_MemObjRelease(pIdx);` |
|     ! 0 | 1695 | `				PH7_MemObjRelease(pTos);` |
|     ! 0 | 1696 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|     ! 0 | 1697 | `				pTos->nIdx = SXU32_HIGH;` |
|     ! 0 | 1698 | `				if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     ! 0 | 1699 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1700 | `			}` |
|  866277 | 1701 | `			if( eOfft == VM_STROFF_REJECT ){` |
|       - | 1702 | `				/* php's TypeError for an offset type a string refuses. PHL cast` |
|       - | 1703 | ``				 * every offset to int, so `$s[""]`, `$s["-"]` and `$s["p"]` all`` |
|       - | 1704 | ``				 * answered `$s[0]`. Routed as a mid-expression throw (this opcode`` |
|       - | 1705 | `				 * is not a call boundary), so the rest of the expression is` |
|       - | 1706 | `				 * abandoned the way php abandons it. */` |
|      42 | 1707 | `				VmFreeCoalStrOff(pCoalOff);` |
|      42 | 1708 | `				rc = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|      42 | 1709 | `				PH7_MemObjRelease(pIdx);` |
|      42 | 1710 | `				PH7_MemObjRelease(pTos);` |
|      42 | 1711 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      42 | 1712 | `				pTos->nIdx = SXU32_HIGH;` |
|      42 | 1713 | `				if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      42 | 1714 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1715 | `			}` |
|  866239 | 1716 | `			iRaw = iOfft;` |
|       - | 1717 | `			/* php 7.1: a NEGATIVE offset counts back from the end ($s[-1] is the last` |
|       - | 1718 | `			 * character). The offset used to be cast to UNSIGNED, so -1 became a huge` |
|       - | 1719 | `			 * number, ran past the end and quietly produced NULL. */` |
|  866239 | 1720 | `			if( iOfft < 0 ){` |
|      20 | 1721 | `				iOfft += nLen;` |
|       9 | 1722 | `			}` |
|  866239 | 1723 | `			if( iOfft < 0 \|\| iOfft >= nLen ){` |
|       - | 1724 | `				/* Out of range. In an isset()/empty() lookup php answers FALSE, so load` |
|       - | 1725 | `				 * NULL there; everywhere else it WARNS and yields the empty string (PH7` |
|       - | 1726 | `				 * silently produced NULL in both cases). */` |
|      85 | 1727 | `				PH7_MemObjRelease(pTos);` |
|      85 | 1728 | `				if( bQuiet ){` |
|      66 | 1729 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      34 | 1730 | `				}else{` |
|      20 | 1731 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|      20 | 1732 | `					if( iP2 != 1 && iP2 != VM_IDX_CTX_UNSET_BASE ){` |
|       - | 1733 | `						/* A WRITE-context fetch never READS the character in php: it` |
|       - | 1734 | `						 * resolves the offset (loudly — the SHAPE diagnostics above are` |
|       - | 1735 | `						 * php's there too) and hands back its offset marker, and the` |
|       - | 1736 | ``						 * opcode that consumes it refuses. So `$s[5] .= 'x'` and`` |
|       - | 1737 | ``						 * `$s[5]++` are the assign-op / incr-decr Error with nothing`` |
|       - | 1738 | ``						 * said about offset 5, where PHL announced an `Uninitialized`` |
|       - | 1739 | ``						 * string offset 5` it never had to look at. */`` |
|      23 | 1740 | `						VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Uninitialized string offset %qd",` |
|       7 | 1741 | `							iRaw);` |
|       7 | 1742 | `					}` |
|       - | 1743 | `				}` |
|      44 | 1744 | `			}else{` |
|  866157 | 1745 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|  866157 | 1746 | `				int c = zData[iOfft];` |
|  866157 | 1747 | `				PH7_MemObjRelease(pTos);` |
|  866157 | 1748 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|  866157 | 1749 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|       - | 1750 | `			}` |
|  866239 | 1751 | `			if( pCoalOff ){` |
|      51 | 1752 | `				if( (pTos->iFlags & MEMOBJ_NULL) && bBaseStrOff ){` |
|       - | 1753 | ``					/* `$s[0][9] ??= v`: the peek came back empty, so the `??=` WILL`` |
|       - | 1754 | `					 * store — and the thing it would store into is a character inside a` |
|       - | 1755 | `					 * string offset, which php refuses like every other reach-inside.` |
|       - | 1756 | `					 * The refusal belongs to the store, which is why the peek that finds` |
|       - | 1757 | ``					 * a character (`$s[0][0] ??= 7`) short-circuits and says nothing at`` |
|       - | 1758 | `					 * all in php. */` |
|       3 | 1759 | `					VmFreeCoalStrOff(pCoalOff);` |
|       3 | 1760 | `					rc = VmThrowFromVm(&(*pVm),"Error","Cannot use string offset as an array",` |
|       - | 1761 | `						sizeof("Cannot use string offset as an array")-1);` |
|       3 | 1762 | `					PH7_MemObjRelease(pTos);` |
|       3 | 1763 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       3 | 1764 | `					pTos->nIdx = SXU32_HIGH;` |
|       3 | 1765 | `					if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       3 | 1766 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1767 | `				}` |
|      49 | 1768 | `				if( pTos->iFlags & MEMOBJ_NULL ){` |
|       - | 1769 | ``					/* Out of range: the `??=` will store, so hand the offset over. */`` |
|      41 | 1770 | `					pTos->x.pOther = (void *)pCoalOff;` |
|      41 | 1771 | `					pTos->iFlags \|= MEMOBJ_AUX_COALSTROFF;` |
|      21 | 1772 | `				}else{` |
|       - | 1773 | ``					/* A real byte: the `??=` short-circuits over the store. */`` |
|       9 | 1774 | `					VmFreeCoalStrOff(pCoalOff);` |
|       - | 1775 | `				}` |
|      24 | 1776 | `			}` |
|       - | 1777 | `			/* The result still carries the BASE VARIABLE's slot index, which is` |
|       - | 1778 | `			 * harmless for a plain read and WRONG for anything that would ALIAS it:` |
|       - | 1779 | `			 * a string offset is not a slot. Mark it so the reference-binding sites` |
|       - | 1780 | `			 * raise php's Error instead of aliasing the whole string --` |
|       - | 1781 | ``			 * `$r = &$s[1]; $r = "Z";` REPLACED $s with "Z". */`` |
|  866237 | 1782 | `			pTos->iFlags \|= MEMOBJ_AUX_STROFFSET;` |
|  434295 | 1783 | `		}else{` |
|       - | 1784 | `			/* No available index,load NULL */` |
|     ! 0 | 1785 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       - | 1786 | `		}` |
|  866237 | 1787 | `		VM_EXIT_BREAK;` |
|       - | 1788 | `	}` |
|  234827 | 1789 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       - | 1790 | `		/* Object subscript: ArrayAccess dispatch.` |
|       - | 1791 | `		 * iP2 codes:` |
|       - | 1792 | `		 *   0 = read       → offsetGet` |
|       - | 1793 | `		 *   3 = ??= peek   → offsetExists; offsetGet on hit; arm coalesce` |
|       - | 1794 | `		 *                    target on miss for the upcoming NULLC_STORE` |
|       - | 1795 | `		 *   4 = isset()    → offsetExists` |
|       - | 1796 | `		 *   5 = unset()    → offsetUnset` |
|       - | 1797 | `		 *   6 = empty()    → offsetExists, then offsetGet on hit` |
|       - | 1798 | ``		 *   8 = `??` read  → same probe as 6 (offsetExists, then offsetGet on a`` |
|       - | 1799 | `		 *                    hit) and no diagnostics anywhere in this op: php` |
|       - | 1800 | ``		 *                    evaluates the whole left operand of `??` in`` |
|       - | 1801 | `		 *                    isset-context, and calling offsetGet blindly also` |
|       - | 1802 | `		 *                    surfaced warnings raised INSIDE a userland` |
|       - | 1803 | `		 *                    offsetGet (e.g. ArrayObject's own array read). */` |
|     933 | 1804 | `		ph7_class_instance *pInst = (ph7_class_instance *)pTos->x.pOther;` |
|     933 | 1805 | `		ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|       - | 1806 | `		/* php's read_dimension / has_dimension HANDLERS, which a native class may` |
|       - | 1807 | ``		 * carry without implementing ArrayAccess -- `$list[0]` reads a DOMNodeList`` |
|       - | 1808 | ``		 * there while `$list instanceof ArrayAccess` is false. They come FIRST`` |
|       - | 1809 | `		 * because php's interface is implemented THROUGH the handler: a user` |
|       - | 1810 | `		 * subclass declaring ArrayAccess inherits the parent's handler, so its own` |
|       - | 1811 | `		 * offsetGet/offsetExists are not consulted for a READ. The WRITE half is` |
|       - | 1812 | `		 * not here at all -- a store, an append and an unset (iP2 5) fall past` |
|       - | 1813 | ``		 * this into php's `Cannot use object of type C as array` unless the class`` |
|       - | 1814 | `		 * really implements the interface, which is php's own split (that same` |
|       - | 1815 | `		 * subclass DOES get its offsetSet called). */` |
|     933 | 1816 | `		if( pInst && iP2 != 5 && PH7_ClassHasNativeDim(pInst->pClass) ){` |
|       - | 1817 | `			PH7_NativeDimCtx sDim;` |
|       - | 1818 | `			ph7_value sResult;` |
|       - | 1819 | ``			/* `$o[$k] op= v` is php's read-then-WRITE pair, and php reports the`` |
|       - | 1820 | `			 * WRITE's refusal when the read answered nothing at all: an offset the` |
|       - | 1821 | ``			 * handler REFUSED comes back to `zend_binary_assign_op_obj_dim` as a`` |
|       - | 1822 | ``			 * miss, which raises `Cannot use object of type C as array` and chains`` |
|       - | 1823 | ``			 * the refusal behind it. `??=` is not that pair -- it reads in`` |
|       - | 1824 | `			 * isset-context, so its refusal is what surfaces -- and neither of them` |
|       - | 1825 | `			 * decides the store itself: that goes through the ordinary write path` |
|       - | 1826 | `			 * below, which is offsetSet for a subclass that has one. */` |
|     342 | 1827 | `			int bRmwCtx = (iP2 == 1) && VmNextIsCompoundAssign(pInstr + 1);` |
|     342 | 1828 | `			int bIsset = (iP2 == 4 \|\| iP2 == 6);` |
|     342 | 1829 | `			PH7_MemObjInit(&(*pVm),&sResult);` |
|     342 | 1830 | `			sDim.iMode = bIsset ? PH7_NATIVE_DIM_ISSET : PH7_NATIVE_DIM_READ;` |
|     342 | 1831 | `			sDim.pOffset = pIdx;` |
|     342 | 1832 | `			sDim.pResult = &sResult;` |
|     342 | 1833 | `			sDim.zThrowClass = 0;` |
|     342 | 1834 | `			sDim.zThrowMsg[0] = 0;` |
|     342 | 1835 | `			PH7_ClassNativeDim(pInst,&sDim);` |
|     342 | 1836 | `			if( iP2 == 6 && sDim.zThrowClass == 0 && ph7_value_to_bool(&sResult) ){` |
|       - | 1837 | `				/* empty(): php asks has_dimension first and reads the VALUE only on` |
|       - | 1838 | ``				 * a hit, which is why an out-of-range `empty($map[-1])` is a plain`` |
|       - | 1839 | `				 * TRUE where the read of the same offset refuses. */` |
|       7 | 1840 | `				PH7_MemObjRelease(&sResult);` |
|       7 | 1841 | `				PH7_MemObjInit(&(*pVm),&sResult);` |
|       7 | 1842 | `				sDim.iMode = PH7_NATIVE_DIM_READ;` |
|       7 | 1843 | `				sDim.pResult = &sResult;` |
|       7 | 1844 | `				PH7_ClassNativeDim(pInst,&sDim);` |
|       3 | 1845 | `			}` |
|     342 | 1846 | `			if( sDim.zThrowClass ){` |
|       - | 1847 | `				char zMsg[256];` |
|      32 | 1848 | `				const char *zClass = sDim.zThrowClass;` |
|      32 | 1849 | `				const char *zText = sDim.zThrowMsg;` |
|       - | 1850 | `				sxu32 nMsg;` |
|      32 | 1851 | `				if( bRmwCtx ){` |
|       5 | 1852 | `					SyString *pName = &pInst->pClass->sName;` |
|       5 | 1853 | `					zClass = "Error";` |
|       5 | 1854 | `					zText = zMsg;` |
|       7 | 1855 | `					nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - | 1856 | `						"Cannot use object of type %.*s as array",` |
|       4 | 1857 | `						(int)pName->nByte,pName->zString);` |
|       3 | 1858 | `				}else{` |
|      28 | 1859 | `					nMsg = (sxu32)SyStrlen(zText);` |
|       - | 1860 | `				}` |
|      32 | 1861 | `				VmCoalesceDisarm(pVm);` |
|      32 | 1862 | `				rc = VmThrowFromVm(pVm,zClass,zText,nMsg);` |
|      32 | 1863 | `				PH7_MemObjRelease(&sResult);` |
|      32 | 1864 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|      32 | 1865 | `				PH7_MemObjRelease(pTos);` |
|      32 | 1866 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      32 | 1867 | `				pTos->nIdx = SXU32_HIGH;` |
|      32 | 1868 | `				if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      54 | 1869 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1870 | `			}` |
|     312 | 1871 | `			if( iP2 == 4 ){` |
|       - | 1872 | `				/* isset(): push a BOOL, which is also what keeps vm_builtin_isset` |
|       - | 1873 | `				 * from warning about a non-variable operand. */` |
|      33 | 1874 | `				int bExists = ph7_value_to_bool(&sResult);` |
|      33 | 1875 | `				PH7_MemObjRelease(&sResult);` |
|      33 | 1876 | `				PH7_MemObjRelease(pTos);` |
|      33 | 1877 | `				pTos->nIdx = SXU32_HIGH;` |
|      33 | 1878 | `				if( bExists ){` |
|       9 | 1879 | `					MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       9 | 1880 | `					pTos->x.iVal = 1;` |
|       5 | 1881 | `				}else{` |
|      25 | 1882 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       1 | 1883 | `				}` |
|     296 | 1884 | `			}else if( iP2 == 3 && (sResult.iFlags & MEMOBJ_NULL) ){` |
|       - | 1885 | ``				/* `$o[$k] ??= v` and the read found nothing: arm (object, key) so`` |
|       - | 1886 | `				 * the NULLC_STORE that follows performs php's store -- offsetSet for` |
|       - | 1887 | ``				 * a subclass that declares one, and `Cannot use object of type C as`` |
|       - | 1888 | ``				 * array` for the collections themselves, which is the same verdict`` |
|       - | 1889 | ``				 * the plain `$o[$k] = v` gets. */`` |
|       5 | 1890 | `				VmCoalesceDisarm(pVm);` |
|       5 | 1891 | `				PH7_MemObjRelease(pTos);` |
|       5 | 1892 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|       5 | 1893 | `				pTos->nIdx = SXU32_HIGH;` |
|       5 | 1894 | `				if( pIdx ){` |
|       5 | 1895 | `					PH7_MemObjStore(pIdx,&pVm->sCoalesceKey);` |
|       2 | 1896 | `				}` |
|       5 | 1897 | `				pVm->pCoalesceObj = pInst;` |
|       5 | 1898 | `				pInst->iRef++;` |
|       5 | 1899 | `				pVm->bCoalesceArmed = 1;` |
|       5 | 1900 | `				PH7_MemObjRelease(&sResult);` |
|       3 | 1901 | `			}else{` |
|       - | 1902 | `				/* The base slot may be the only thing holding this instance, and the` |
|       - | 1903 | `				 * write-context tail below still speaks for its CLASS -- hold a` |
|       - | 1904 | `				 * reference across the release, as the ArrayAccess arm does. */` |
|     276 | 1905 | `				pInst->iRef++;` |
|     276 | 1906 | `				if( iP2 == 3 ){` |
|       3 | 1907 | `					VmCoalesceDisarm(pVm); /* a hit short-circuits over the store */` |
|       1 | 1908 | `				}` |
|     276 | 1909 | `				PH7_MemObjRelease(pTos);` |
|     276 | 1910 | `				PH7_MemObjStore(&sResult,pTos);` |
|     276 | 1911 | `				pTos->nIdx = SXU32_HIGH;` |
|     276 | 1912 | `				if( bRmwCtx ){` |
|       - | 1913 | `					/* php's ASSIGN_DIM_OP: the read gave the current value, the op` |
|       - | 1914 | `					 * computes on it, and the result goes back out through the write` |
|       - | 1915 | `					 * path -- offsetSet where there is one, php's Error where there` |
|       - | 1916 | `					 * is not (VmHookRmwConsume). */` |
|      16 | 1917 | `					VmDimRmwArm(&(*pVm),pInst,pIdx,pTos,` |
|      10 | 1918 | `						(void *)pStack,(void *)aInstr,(sxu32)(pc + 1));` |
|     271 | 1919 | `				}else if( VmIdxFetchForWrite(pInstr,iP2) ){` |
|       - | 1920 | ``					/* php's `Indirect modification of overloaded element` -- silent`` |
|       - | 1921 | `					 * for an OBJECT, which is every value these containers answer,` |
|       - | 1922 | ``					 * and raised for the NULL a miss leaves (`$list[9]++`). */`` |
|       5 | 1923 | `					PH7_VmOverloadedElemNotice(&(*pVm),pInst->pClass,pTos);` |
|     264 | 1924 | `				}else if( pInstr->iP2 == 9 ){` |
|       - | 1925 | `					/* A deferred call ARGUMENT: the read has happened, and whether php` |
|       - | 1926 | `					 * performed a W fetch is the callee's to say. Carry the value plus` |
|       - | 1927 | `					 * the class that answered it so the verdict lands at the call. */` |
|      76 | 1928 | `					VmDeferredPath *pPre = VmDeferPathNewPrefetch(&(*pVm),VM_OVER_ELEM,` |
|      25 | 1929 | `						pInst->pClass,0,pTos);` |
|      51 | 1930 | `					if( pPre ){` |
|      51 | 1931 | `						PH7_MemObjRelease(pTos);` |
|      51 | 1932 | `						pTos->x.pOther = pPre;` |
|      51 | 1933 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|      51 | 1934 | `						pTos->nIdx = SXU32_HIGH;` |
|      25 | 1935 | `					}` |
|      25 | 1936 | `				}` |
|     276 | 1937 | `				PH7_ClassInstanceUnref(pInst);` |
|     276 | 1938 | `				PH7_MemObjRelease(&sResult);` |
|       - | 1939 | `			}` |
|     312 | 1940 | `			if( pIdx ){` |
|     312 | 1941 | `				PH7_MemObjRelease(pIdx);` |
|     155 | 1942 | `			}` |
|     312 | 1943 | `			VM_EXIT_BREAK;` |
|       - | 1944 | `		}` |
|     593 | 1945 | `		if( pArrayAccess && pInst && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|       - | 1946 | `			ph7_class_method *pMeth;` |
|       - | 1947 | `			ph7_value sResult;` |
|       - | 1948 | `			ph7_value sNullIdx;` |
|       - | 1949 | `			ph7_value *apArg[1];` |
|     574 | 1950 | `			if( pIdx && VmDimFastFetchCtx(pInstr,iP2) && PH7_VmDimFetchWritable(pInst->pClass)` |
|     252 | 1951 | `			 && pInst->iRef > 1 ){` |
|       - | 1952 | `				/* php hands a writable container's element back BY SLOT, and that is what` |
|       - | 1953 | ``				 * makes an indirect modification through it land. The `iRef > 1` guard is`` |
|       - | 1954 | ``				 * the object half of the array path's `pMap->iRef < 2` rule: releasing the`` |
|       - | 1955 | `				 * base below drops this stack slot's own reference, and a TEMPORARY` |
|       - | 1956 | ``				 * container (`(new ArrayObject([1]))[0]`) would be destroyed with its`` |
|       - | 1957 | `				 * storage while the result still views it. Such a base has nothing that` |
|       - | 1958 | `				 * could observe the write anyway, so it takes the accessor's copy. */` |
|     190 | 1959 | `				sxu32 nElem = PH7_SplDimElemSlot(&(*pVm),pInst,pIdx,` |
|       - | 1960 | `					/* php's write-context vivification, and only there: a W/RW fetch —` |
|       - | 1961 | ``					 * including the `$r = &$ao['k']` and by-ref-foreach shapes iP2 alone`` |
|       - | 1962 | `					 * cannot name — creates the missing element, while an unset chain's` |
|       - | 1963 | `					 * intermediate step never does. */` |
|     161 | 1964 | `					VmIdxFetchForWrite(pInstr,iP2) && !VM_IDX_IS_UNSET(iP2));` |
|     136 | 1965 | `				ph7_value *pElem = (nElem == SXU32_HIGH) ? 0` |
|     129 | 1966 | `					: (ph7_value *)SySetAt(&pVm->aMemObj,nElem);` |
|     136 | 1967 | `				if( pElem ){` |
|     126 | 1968 | `					PH7_MemObjRelease(pTos);` |
|     126 | 1969 | `					PH7_MemObjLoad(pElem,pTos);` |
|     126 | 1970 | `					pTos->nIdx = nElem;` |
|     126 | 1971 | `					PH7_MemObjRelease(pIdx);` |
|     126 | 1972 | `					VM_EXIT_BREAK;` |
|       - | 1973 | `				}` |
|       5 | 1974 | `			}` |
|     455 | 1975 | `			if( (iP2 == 0 \|\| iP2 == 3 \|\| iP2 == 8) && pIdx == 0 ){` |
|       - | 1976 | ``				/* `$obj[]` read — PHP rejects this. */`` |
|     ! 0 | 1977 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|       - | 1978 | `					"Cannot use [] for reading");` |
|     ! 0 | 1979 | `				PH7_MemObjRelease(pTos);` |
|     ! 0 | 1980 | `				pTos->nIdx = SXU32_HIGH;` |
|     ! 0 | 1981 | `				VM_EXIT_BREAK;` |
|       - | 1982 | `			}` |
|     455 | 1983 | `			PH7_MemObjInit(&(*pVm),&sResult);` |
|     455 | 1984 | `			if( iP2 == 4 \|\| iP2 == 6 \|\| iP2 == 3 \|\| iP2 == 8 ){` |
|       - | 1985 | ``				/* isset, empty, ??= and `??` all start with offsetExists. */`` |
|     139 | 1986 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 1987 | `					"offsetExists",sizeof("offsetExists")-1);` |
|     139 | 1988 | `				apArg[0] = pIdx;` |
|     139 | 1989 | `				if( pMeth ){` |
|     139 | 1990 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|      72 | 1991 | `				}` |
|     388 | 1992 | `			}else if( iP2 == 5 ){` |
|      54 | 1993 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 1994 | `					"offsetUnset",sizeof("offsetUnset")-1);` |
|      54 | 1995 | `				apArg[0] = pIdx;` |
|      54 | 1996 | `				if( pMeth ){` |
|      54 | 1997 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|      25 | 1998 | `				}` |
|      29 | 1999 | `			}else{` |
|     271 | 2000 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 2001 | `					"offsetGet",sizeof("offsetGet")-1);` |
|     271 | 2002 | `				if( pIdx == 0 ){` |
|       - | 2003 | ``					/* `$o[] op= v` — the one read that reaches here without a key.`` |
|       - | 2004 | `					 * php hands the accessors NULL for the absent offset (its` |
|       - | 2005 | `					 * read_dimension substitutes one), so passing NO argument` |
|       - | 2006 | `					 * turned an assignment php performs into an` |
|       - | 2007 | `					 * ArgumentCountError against the class's own offsetGet. */` |
|       3 | 2008 | `					PH7_MemObjInit(&(*pVm),&sNullIdx);` |
|       3 | 2009 | `					pIdx = &sNullIdx;` |
|       1 | 2010 | `				}` |
|     271 | 2011 | `				apArg[0] = pIdx;` |
|     271 | 2012 | `				if( pMeth ){` |
|     271 | 2013 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|     133 | 2014 | `				}` |
|       - | 2015 | `			}` |
|     455 | 2016 | `			if( iP2 == 4 ){` |
|       - | 2017 | `				/* isset: push MEMOBJ_BOOL so vm_builtin_isset reports the` |
|       - | 2018 | `				 * right truth value AND skips its "Expecting a variable not` |
|       - | 2019 | `				 * a constant" warning (keyed on MEMOBJ_BOOL). */` |
|      87 | 2020 | `				int bExists = ph7_value_to_bool(&sResult);` |
|      87 | 2021 | `				PH7_MemObjRelease(pTos);` |
|      87 | 2022 | `				pTos->nIdx = SXU32_HIGH;` |
|      87 | 2023 | `				if( bExists ){` |
|      38 | 2024 | `					MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      38 | 2025 | `					pTos->x.iVal = 1;` |
|      21 | 2026 | `				}else{` |
|      53 | 2027 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       5 | 2028 | `				}` |
|     414 | 2029 | `			}else if( iP2 == 5 ){` |
|       - | 2030 | `				/* offsetUnset return is discarded; push NULL so the trailing` |
|       - | 2031 | `				 * vm_builtin_unset is a harmless no-op. */` |
|      54 | 2032 | `				PH7_MemObjRelease(pTos);` |
|      54 | 2033 | `				pTos->nIdx = SXU32_HIGH;` |
|      54 | 2034 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|     348 | 2035 | `			}else if( iP2 == 6 \|\| iP2 == 8 ){` |
|       - | 2036 | `				/* empty: if offsetExists is false, push NULL so empty=true` |
|       - | 2037 | `				 * without calling offsetGet. If true, call offsetGet and` |
|       - | 2038 | `				 * push the value so PH7_builtin_empty evaluates emptiness.` |
|       - | 2039 | ``				 * `??` (8) needs the identical shape: NULL on a miss so the`` |
|       - | 2040 | `				 * coalesce takes the default, the real value on a hit. */` |
|      48 | 2041 | `				int bExists = ph7_value_to_bool(&sResult);` |
|      48 | 2042 | `				PH7_MemObjRelease(&sResult);` |
|      48 | 2043 | `				PH7_MemObjRelease(pTos);` |
|      48 | 2044 | `				pTos->nIdx = SXU32_HIGH;` |
|      48 | 2045 | `				if( !bExists ){` |
|      24 | 2046 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      14 | 2047 | `				}else{` |
|      28 | 2048 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 2049 | `						"offsetGet",sizeof("offsetGet")-1);` |
|       - | 2050 | `					ph7_value sValue;` |
|      28 | 2051 | `					PH7_MemObjInit(&(*pVm),&sValue);` |
|      28 | 2052 | `					apArg[0] = pIdx;` |
|      28 | 2053 | `					if( pGet ){` |
|      28 | 2054 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|      12 | 2055 | `					}` |
|      28 | 2056 | `					PH7_MemObjStore(&sValue,pTos);` |
|      28 | 2057 | `					PH7_MemObjRelease(&sValue);` |
|       - | 2058 | `				}` |
|      48 | 2059 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|      48 | 2060 | `				VM_EXIT_BREAK; /* skip the duplicate sResult release below */` |
|     279 | 2061 | `			}else if( iP2 == 3 ){` |
|       - | 2062 | `				/* ?? null-coalesce peek: emulate PHP semantics —` |
|       - | 2063 | `				 *   if !offsetExists OR offsetGet() === null → arm` |
|       - | 2064 | `				 *     coalesce slot (NULLC_STORE will call offsetSet)` |
|       - | 2065 | `				 *     and push NULL.` |
|       - | 2066 | `				 *   else → push offsetGet's value (NULLC_JMP skips). */` |
|      10 | 2067 | `				int bExists = ph7_value_to_bool(&sResult);` |
|      10 | 2068 | `				int bShouldArm = !bExists;` |
|       - | 2069 | `				ph7_value sValue;` |
|      10 | 2070 | `				PH7_MemObjRelease(&sResult);` |
|       - | 2071 | `				/* Reset any prior arming defensively */` |
|      10 | 2072 | `				VmCoalesceDisarm(pVm);` |
|      10 | 2073 | `				PH7_MemObjInit(&(*pVm),&sValue);` |
|      10 | 2074 | `				if( bExists ){` |
|       5 | 2075 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 2076 | `						"offsetGet",sizeof("offsetGet")-1);` |
|       5 | 2077 | `					apArg[0] = pIdx;` |
|       5 | 2078 | `					if( pGet ){` |
|       5 | 2079 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|       2 | 2080 | `					}` |
|       5 | 2081 | `					if( sValue.iFlags & MEMOBJ_NULL ){` |
|       3 | 2082 | `						bShouldArm = 1;` |
|       1 | 2083 | `					}` |
|       2 | 2084 | `				}` |
|      10 | 2085 | `				PH7_MemObjRelease(pTos);` |
|      10 | 2086 | `				pTos->nIdx = SXU32_HIGH;` |
|      10 | 2087 | `				if( bShouldArm ){` |
|       - | 2088 | `					/* Arm: remember (object, key) so NULLC_STORE dispatches` |
|       - | 2089 | `					 * to offsetSet. Hold a ref on the instance to survive` |
|       - | 2090 | `					 * intervening expression evaluation. */` |
|       8 | 2091 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       8 | 2092 | `					if( pIdx ){` |
|       8 | 2093 | `						PH7_MemObjStore(pIdx,&pVm->sCoalesceKey);` |
|       3 | 2094 | `					}` |
|       8 | 2095 | `					pVm->pCoalesceObj = pInst;` |
|       8 | 2096 | `					pInst->iRef++;` |
|       8 | 2097 | `					pVm->bCoalesceArmed = 1;` |
|       5 | 2098 | `				}else{` |
|       3 | 2099 | `					PH7_MemObjStore(&sValue,pTos);` |
|       - | 2100 | `				}` |
|      10 | 2101 | `				PH7_MemObjRelease(&sValue);` |
|      10 | 2102 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|      10 | 2103 | `				VM_EXIT_BREAK;` |
|     ! 0 | 2104 | `			}else{` |
|       - | 2105 | `				/* offsetGet: replace pTos with the returned value.` |
|       - | 2106 | `				 *` |
|       - | 2107 | `				 * The base slot may be the only thing holding this instance — a` |
|       - | 2108 | ``				 * TEMPORARY container (`f((new C)['a'])`, a getter's return) dies with`` |
|       - | 2109 | `				 * it — and everything below still speaks for the object: the writable` |
|       - | 2110 | `				 * test, php's notice and the read-modify-write arming all read its` |
|       - | 2111 | `				 * CLASS, and the deferred-argument carrier records it. Hold a reference` |
|       - | 2112 | `				 * of our own across the release so none of them is left reading freed` |
|       - | 2113 | `				 * memory. */` |
|     271 | 2114 | `				pInst->iRef++;` |
|     271 | 2115 | `				PH7_MemObjRelease(pTos);` |
|     271 | 2116 | `				PH7_MemObjStore(&sResult,pTos);` |
|     271 | 2117 | `				pTos->nIdx = SXU32_HIGH;` |
|     271 | 2118 | `				if( iP2 == 1 && VmNextIsCompoundAssign(pInstr + 1) ){` |
|       - | 2119 | ``					/* `$o[$k] op= v` is php's ASSIGN_DIM_OP: offsetGet gave the`` |
|       - | 2120 | `					 * current value, the op computes on it, and the result goes` |
|       - | 2121 | `					 * back through offsetSet($k, …). PHL had no write-back at` |
|       - | 2122 | `					 * all here — the fetched value carried no slot, so every` |
|       - | 2123 | `					 * compound assign on an ArrayAccess element died on` |
|       - | 2124 | `					 * "Cannot perform assignment on a constant class attribute"` |
|       - | 2125 | `					 * and stored nothing. Arm the scratch slot the op mutates;` |
|       - | 2126 | `					 * its tail (PH7_HOOK_RMW_WRITEBACK) dispatches offsetSet. */` |
|      64 | 2127 | `					VmDimRmwArm(&(*pVm),pInst,pIdx,pTos,` |
|      42 | 2128 | `						(void *)pStack,(void *)aInstr,(sxu32)(pc + 1));` |
|     246 | 2129 | `				}else if( VmIdxFetchForWrite(pInstr,iP2)` |
|     131 | 2130 | `				       && !PH7_VmDimFetchWritable(pInst->pClass) ){` |
|      25 | 2131 | `					PH7_VmOverloadedElemNotice(&(*pVm),pInst->pClass,pTos);` |
|     217 | 2132 | `				}else if( pInstr->iP2 == 9 ){` |
|       - | 2133 | `					/* A deferred call ARGUMENT. The accessor has just run — php runs it` |
|       - | 2134 | `					 * where the subscript is written, whatever the parameter turns out to` |
|       - | 2135 | `					 * be — but WHICH fetch php performed is the callee's to say, and only` |
|       - | 2136 | `					 * OP_CALL knows: a by-reference parameter makes it a W fetch, which on` |
|       - | 2137 | `					 * a container that can only answer with a VALUE is php's` |
|       - | 2138 | ``					 * `Indirect modification of overloaded element` notice and a write`` |
|       - | 2139 | `					 * thrown away. Carry the result plus the class that answered it, so the` |
|       - | 2140 | `					 * verdict lands at the call without the accessor running twice or the` |
|       - | 2141 | `					 * argument arriving as NULL. The value would otherwise reach the callee` |
|       - | 2142 | `					 * still SHARING the container's own nested map by COW, and a by-ref` |
|       - | 2143 | ``					 * `f($o['a']['b'])` wrote straight into the object php leaves untouched. */`` |
|      49 | 2144 | `					VmDeferredPath *pPre = VmDeferPathNewPrefetch(&(*pVm),VM_OVER_ELEM,pInst->pClass,0,pTos);` |
|      49 | 2145 | `					if( pPre ){` |
|      49 | 2146 | `						PH7_MemObjRelease(pTos);` |
|      49 | 2147 | `						pTos->x.pOther = pPre;` |
|      49 | 2148 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|      49 | 2149 | `						pTos->nIdx = SXU32_HIGH;` |
|      24 | 2150 | `					}` |
|      24 | 2151 | `				}` |
|     271 | 2152 | `				PH7_ClassInstanceUnref(pInst);` |
|       - | 2153 | `			}` |
|     403 | 2154 | `			PH7_MemObjRelease(&sResult);` |
|     403 | 2155 | `			if( pIdx ){` |
|     403 | 2156 | `				PH7_MemObjRelease(pIdx);` |
|     199 | 2157 | `			}` |
|     403 | 2158 | `			VM_EXIT_BREAK;` |
|       - | 2159 | `		}` |
|       - | 2160 | `		/* Object without ArrayAccess: PHP throws fatal Error in all subscript` |
|       - | 2161 | `		 * contexts (read, isset, unset, empty). Match it. A class carrying a` |
|       - | 2162 | `		 * READ handler reaches here only for the unset (iP2 5), which the hook` |
|       - | 2163 | `		 * branch above skips, and words that refusal itself. */` |
|      17 | 2164 | `		if( pInst ){` |
|       - | 2165 | `			char zMsg[256];` |
|      24 | 2166 | `			sxu32 nMsg = PH7_ClassNativeDimRefusal(pInst,` |
|       7 | 2167 | `				iP2 == 5 ? PH7_NATIVE_DIM_UNSET : PH7_NATIVE_DIM_WRITE,` |
|       7 | 2168 | `				zMsg,sizeof(zMsg));` |
|      17 | 2169 | `			rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|      17 | 2170 | `			if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|      17 | 2171 | `			PH7_MemObjRelease(pTos);` |
|      17 | 2172 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      17 | 2173 | `			pTos->nIdx = SXU32_HIGH;` |
|      17 | 2174 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       - | 2175 | ``			/* `break` used to resume at the NEXT instruction: the catch ran and then`` |
|       - | 2176 | `			 * execution carried on inside the try block. */` |
|      23 | 2177 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2178 | `		}` |
|     ! 0 | 2179 | `	}` |
|  233899 | 2180 | `	if( (iP2 == 1 \|\| iP2 == 3 \|\| VM_IDX_IS_UNSET(iP2)) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       - | 2181 | `		{` |
|       - | 2182 | `			/* The base's TYPE decides here, whether or not there is a SLOT behind it.` |
|       - | 2183 | ``			 * A write THROUGH a temporary — `f()[0] = 5`, `f()[0][1] = 5` — gets the`` |
|       - | 2184 | `			 * same verdict from php; the only difference is that what it writes is` |
|       - | 2185 | `			 * discarded afterwards. PHL skipped the whole screen when the base had no` |
|       - | 2186 | ``			 * slot, so `ui()[0] += 5` over an int RESULT ran in silence where php`` |
|       - | 2187 | `			 * throws. The temporary is screened and vivified in place, on the stack —` |
|       - | 2188 | `			 * there is nowhere to write it back to. */` |
|      84 | 2189 | `			ph7_value *pObj = (pTos->nIdx != SXU32_HIGH)` |
|      48 | 2190 | `				? (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)` |
|      30 | 2191 | `				: pTos;` |
|      57 | 2192 | `			if( pObj != 0 ){` |
|       - | 2193 | `				/* php 8 write-context auto-vivify rules: NULL converts to array` |
|       - | 2194 | `				 * silently; FALSE converts with the 8.1 deprecation; any other` |
|       - | 2195 | `				 * scalar base — int/float/true/resource — is php's catchable` |
|       - | 2196 | `				 * "Cannot use a scalar value as an array" Error and the variable` |
|       - | 2197 | `				 * stays untouched (pre-fix the base was silently CONVERTED,` |
|       - | 2198 | ``				 * corrupting e.g. `$i = 5; $i[0]++` into array(0 => 6); string`` |
|       - | 2199 | `				 * bases were intercepted by the string-offset paths above). */` |
|       - | 2200 | `				/* php auto-converts false to an array with an 8.1 DEPRECATION; PHL` |
|       - | 2201 | `				 * rejects it like any other scalar base (null still auto-vivifies —` |
|       - | 2202 | `				 * it is not a bool). */` |
|      57 | 2203 | `				if( (pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_RES\|MEMOBJ_BOOL)) != 0 ){` |
|       - | 2204 | `					/* unset() has its own wording for the same base: php's` |
|       - | 2205 | `					 * "Cannot unset offset in a non-array variable". */` |
|      14 | 2206 | `					const char *zErr = VM_IDX_IS_UNSET(iP2)` |
|       - | 2207 | `						? "Cannot unset offset in a non-array variable"` |
|      12 | 2208 | `						: "Cannot use a scalar value as an array";` |
|       - | 2209 | `					SyBlob sErrMsg;` |
|      18 | 2210 | `					SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      18 | 2211 | `					SyBlobAppend(&sErrMsg,zErr,(sxu32)SyStrlen(zErr));` |
|      18 | 2212 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      18 | 2213 | `					if( pIdx ){` |
|      18 | 2214 | `						PH7_MemObjRelease(pIdx);` |
|       8 | 2215 | `					}` |
|      18 | 2216 | `					PH7_MemObjRelease(pTos);` |
|      18 | 2217 | `					pTos->nIdx = SXU32_HIGH;` |
|      18 | 2218 | `					VM_EXIT_BREAK;` |
|       - | 2219 | `				}` |
|       - | 2220 | `				/* unset() CREATES NOTHING: a null/undefined base stays null where PHL` |
|       - | 2221 | ``				 * converted it to an empty array (`$n = null; unset($n[0]);` left`` |
|       - | 2222 | `				 * $n === []), which is also what materialised the missing intermediate` |
|       - | 2223 | ``				 * in `unset($a["y"]["z"])`. Leaving the base alone, the lookup below`` |
|       - | 2224 | `				 * misses, the tail loads NULL with no slot index, and the trailing` |
|       - | 2225 | `				 * unset() builtin is the no-op php's is. */` |
|      41 | 2226 | `				if( !VM_IDX_IS_UNSET(iP2) ){` |
|      35 | 2227 | `					PH7_MemObjToHashmap(pObj);` |
|      35 | 2228 | `					if( pObj != pTos ){` |
|      35 | 2229 | `						PH7_MemObjLoad(pObj,pTos);` |
|      16 | 2230 | `					}` |
|      16 | 2231 | `				}` |
|      19 | 2232 | `			}` |
|       - | 2233 | `		}` |
|      19 | 2234 | `	}` |
|  233883 | 2235 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|       - | 2236 | `	/* php DEPRECATES both a null offset and a lossy-float subscript (then normalizes` |
|       - | 2237 | `	 * "" / truncates). PHL now matches php on the NULL offset (deprecate + coerce to` |
|       - | 2238 | `	 * the "" key, §2) but still rejects the lossy-FLOAT subscript with a TypeError on` |
|       - | 2239 | `	 * a READ or WRITE (iP2 0/1) — the recorded non-deprecated-surface policy. Both` |
|       - | 2240 | ``	 * stay lenient in isset()/empty()/`??`/unset(), where a throw would be wrong. */`` |
|       - | 2241 | `	/* An object/array key is rejected in EVERY context, including isset()/empty()/` |
|       - | 2242 | `	 * unset() where php still throws (only the wording changes) — unlike the` |
|       - | 2243 | `	 * null/float deprecations below, which stay lenient there. A resource key is` |
|       - | 2244 | `	 * accepted with a warning and becomes its integer id. */` |
|  233883 | 2245 | `	if( (pTos->iFlags & MEMOBJ_HASHMAP) && pIdx ){` |
|       - | 2246 | `		SyBlob sTypeMsg;` |
|  233777 | 2247 | `		if( VmOffsetTypeRejected(&(*pVm),pIdx,iP2,&sTypeMsg) ){` |
|       - | 2248 | `			/* Routed as a mid-expression throw, like the STRING arm above: this` |
|       - | 2249 | `			 * opcode is not a call boundary, so PARKING it let the rest of the` |
|       - | 2250 | ``			 * expression run first — `str_repeat($a[$obj], 2)` reached the builtin`` |
|       - | 2251 | `			 * with the NULL the abandoned read left and died on its ZPP TypeError` |
|       - | 2252 | `			 * instead, and a CAUGHT one still ran the enclosing call afterwards. */` |
|      21 | 2253 | `			rc = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|      21 | 2254 | `			PH7_MemObjRelease(pIdx);` |
|      21 | 2255 | `			PH7_MemObjRelease(pTos);` |
|      21 | 2256 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      21 | 2257 | `			pTos->nIdx = SXU32_HIGH;` |
|      30 | 2258 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      19 | 2259 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2260 | `		}` |
|  233757 | 2261 | `		VmOffsetResourceWarn(&(*pVm),pIdx);` |
|  116876 | 2262 | `	}` |
|  233863 | 2263 | `	if( (pTos->iFlags & MEMOBJ_HASHMAP) && pIdx ){` |
|       - | 2264 | `		/* php DEPRECATES a null offset in EVERY subscript context except unset()` |
|       - | 2265 | `		 * (iP2==5), then normalizes it to the "" key — read, write, isset (4),` |
|       - | 2266 | ``		 * empty (6), `??`/`??=` (3), the coalesce-chain probe (8), destructure`` |
|       - | 2267 | `		 * (2/7). Emit it here so all of them get it, not just read/write; the` |
|       - | 2268 | `		 * lookup/insert below casts NULL->"", and a plain read miss then warns` |
|       - | 2269 | ``		 * `Undefined array key ""` on the now-string pIdx (php's exact pair). */`` |
|  233757 | 2270 | `		if( (pIdx->iFlags & MEMOBJ_NULL) && !VM_IDX_IS_UNSET(iP2) ){` |
|      21 | 2271 | `			VmNullOffsetDeprecate(&(*pVm),pIdx);` |
|       9 | 2272 | `		}` |
|       - | 2273 | `		/* A lossy-FLOAT subscript stays a rejected TypeError on a READ or WRITE` |
|       - | 2274 | `		 * (iP2 0/1) — the recorded non-deprecated-surface policy (§2); the lenient` |
|       - | 2275 | `		 * contexts (isset/empty/??/unset) truncate quietly as php's value does. */` |
|  233752 | 2276 | `		if( (iP2 == 0 \|\| iP2 == 1)` |
|  157885 | 2277 | `		 && (pIdx->iFlags & MEMOBJ_REAL)` |
|  116886 | 2278 | `		 && pIdx->rVal != (ph7_real)(sxi64)pIdx->rVal ){` |
|       - | 2279 | `			SyBlob sErrMsg;` |
|       6 | 2280 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       6 | 2281 | `			SyBlobAppend(&sErrMsg,"Cannot access offset of type float on array",` |
|       - | 2282 | `				sizeof("Cannot access offset of type float on array")-1);` |
|       6 | 2283 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|       6 | 2284 | `			PH7_MemObjRelease(pIdx);` |
|       6 | 2285 | `			PH7_MemObjRelease(pTos);` |
|       6 | 2286 | `			pTos->nIdx = SXU32_HIGH;` |
|       6 | 2287 | `			VM_EXIT_BREAK;` |
|       - | 2288 | `		}` |
|  116874 | 2289 | `	}` |
|  233859 | 2290 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|  233779 | 2291 | `		if( iP2 == 1 \|\| VM_IDX_IS_UNSET(iP2) ){` |
|       - | 2292 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|       - | 2293 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|       - | 2294 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|       - | 2295 | `			 * NOT separate — that would defeat COW on every element read.` |
|       - | 2296 | `			 * iP2=5 is unset-context, treated like iP2=1 for arrays so the` |
|       - | 2297 | `			 * trailing unset() builtin can drop the slot via pTos->nIdx. */` |
|    5391 | 2298 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|    2693 | 2299 | `		}` |
|       - | 2300 | `		/* Point to the hashmap */` |
|  233779 | 2301 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|  233779 | 2302 | `		if( pIdx ){` |
|       - | 2303 | `			/* Load the desired entry */` |
|  233753 | 2304 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|  116874 | 2305 | `		}` |
|  233779 | 2306 | `		if( iP2 == 3 ){` |
|       - | 2307 | `			/* Null coalescing assign peek mode: separate only when we will` |
|       - | 2308 | `			 * actually write back. If the looked-up value is non-null, the` |
|       - | 2309 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|       - | 2310 | `			 * the parent can stay shared. If the value is null or the key is` |
|       - | 2311 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|       - | 2312 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|       - | 2313 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|       - | 2314 | `			 * correct for the outermost write. */` |
|      27 | 2315 | `			int needWrite = (rc != SXRET_OK);` |
|      27 | 2316 | `			if( !needWrite && pNode ){` |
|      13 | 2317 | `				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|      13 | 2318 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|       7 | 2319 | `					needWrite = 1;` |
|       3 | 2320 | `				}` |
|       6 | 2321 | `			}` |
|      27 | 2322 | `			if( needWrite ){` |
|      21 | 2323 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      21 | 2324 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|       - | 2325 | `					/* The map was actually copied — re-lookup so pNode points` |
|       - | 2326 | `					 * into the new map's storage. */` |
|       7 | 2327 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       7 | 2328 | `					if( pIdx ){` |
|       7 | 2329 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|       3 | 2330 | `					}` |
|       3 | 2331 | `				}` |
|      10 | 2332 | `			}` |
|      13 | 2333 | `		}` |
|       - | 2334 | `		/* iP2 == 5 (unset) is deliberately NOT here: php's unset() never creates` |
|       - | 2335 | `		 * the key it is about to remove, so a MISS must stay a miss. The` |
|       - | 2336 | `		 * insert-then-unset round trip was invisible on the LAST step but left` |
|       - | 2337 | ``		 * every INTERMEDIATE behind -- `unset($a["y"]["z"])` grew an empty`` |
|       - | 2338 | `		 * $a["y"]. The COW separation the unset context needs happened above and` |
|       - | 2339 | `		 * does not depend on this insert. */` |
|  233779 | 2340 | `		if( bRmwFetch && rc != SXRET_OK ){` |
|       - | 2341 | `			/* php's read half missed. Record it BEFORE the vivification below` |
|       - | 2342 | `			 * overwrites rc — the warning is about what was not there to read, and` |
|       - | 2343 | `			 * it is emitted after the slot exists, exactly as php does it. */` |
|      45 | 2344 | `			bRmwMiss = 1;` |
|      22 | 2345 | `		}` |
|  233779 | 2346 | `		if( rc != SXRET_OK && (iP2 == 1 \|\| iP2 == 3) ){` |
|       - | 2347 | `			/* Create a new empty entry */` |
|     195 | 2348 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|     195 | 2349 | `			if( rc == SXRET_OK ){` |
|       - | 2350 | `				/* Point to the last inserted entry */` |
|     192 | 2351 | `				pNode = pMap->pLast;` |
|      98 | 2352 | `			}else{` |
|       - | 2353 | ``				/* An append lvalue (`$a[][...] = v`) whose saturated auto-index`` |
|       - | 2354 | `				 * is occupied threw php's catchable Error. Dispatch it here —` |
|       - | 2355 | `				 * falling through with a stale pMap->pLast is what silently` |
|       - | 2356 | `				 * overwrote $a[PHP_INT_MAX]. */` |
|       7 | 2357 | `				PH7_DISPATCH_ENFORCE_RC(rc)` |
|       - | 2358 | `			}` |
|      94 | 2359 | `		}` |
|  116886 | 2360 | `	}` |
|  233852 | 2361 | `	if( pIdx && (bRmwMiss \|\| (rc != SXRET_OK && (iP2 == 2 \|\| iP2 == 0)))` |
|   57705 | 2362 | `	 && (pTos->iFlags & MEMOBJ_HASHMAP)` |
|      79 | 2363 | `	 && !VmIdxFeedsCoalesce(pInstr) ){` |
|       - | 2364 | ``		/* `$a['k'] ?? $d` compiles its LHS as a plain read (iP2 == 0) followed`` |
|       - | 2365 | `		 * by NULLC/NULLC_JMP — php does NOT warn there, so peek ahead and stay` |
|       - | 2366 | `		 * silent (same guard the magic-accessor read path uses). */` |
|       - | 2367 | `		/* php warns when a missing key is READ (iP2 == 0), destructured` |
|       - | 2368 | `		 * (iP2 == 2), or read by the READ half of a read-modify-write (bRmwMiss).` |
|       - | 2369 | `		 * isset/empty/??/unset (iP2 3-6) and plain write-context` |
|       - | 2370 | `		 * vivification (iP2 == 1) stay silent, as does a read on a non-array` |
|       - | 2371 | `		 * base (already diagnosed above). php prints an INT key bare and a` |
|       - | 2372 | `		 * STRING key quoted, and it decides which one the key IS by the same fold` |
|       - | 2373 | ``		 * the lookup just used: `$a["10"]` looked for the INTEGER key 10, so php`` |
|       - | 2374 | ``		 * says `Undefined array key 10`. PHL rendered the operand as WRITTEN --`` |
|       - | 2375 | ``		 * quoting every string and, worse, printing `$a[false]` as the "" key it`` |
|       - | 2376 | `		 * never looked in (false is the integer key 0). The canonical-numeric rule` |
|       - | 2377 | `		 * is the hashmap's own, so ask it rather than re-derive it. */` |
|       - | 2378 | `		SyBlob sMsg;` |
|     115 | 2379 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     115 | 2380 | `		if( (ph7_hashmap *)pTos->x.pOther == pVm->pGlobal ){` |
|       - | 2381 | `			/* $GLOBALS is the symbol table, so a key that is not there is a VARIABLE` |
|       - | 2382 | ``			 * that is not there, and php says so: `Undefined global variable $x`,`` |
|       - | 2383 | ``			 * with the subscript spelled RAW after the `$` ($GLOBALS[5] reads`` |
|       - | 2384 | ``			 * `$5`) rather than folded and quoted the way an array key is. Only the`` |
|       - | 2385 | `			 * LIVE map takes this wording — a copy of $GLOBALS is a by-value` |
|       - | 2386 | `			 * snapshot (memobj.c) and warns as the ordinary array it is. */` |
|       - | 2387 | `			SyString sName;` |
|       5 | 2388 | `			if( (pIdx->iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 2389 | `				PH7_MemObjToString(pIdx);` |
|     ! 0 | 2390 | `			}` |
|       5 | 2391 | `			SyStringInitFromBuf(&sName,SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob));` |
|       5 | 2392 | `			SyBlobFormat(&sMsg,"Undefined global variable $%z",&sName);` |
|     113 | 2393 | `		}else if( PH7_HashmapKeyIsInt(pIdx) ){` |
|      48 | 2394 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      15 | 2395 | `				PH7_MemObjToInteger(pIdx);` |
|       7 | 2396 | `			}` |
|      48 | 2397 | `			SyBlobFormat(&sMsg,"Undefined array key %qd",pIdx->x.iVal);` |
|      25 | 2398 | `		}else{` |
|       - | 2399 | `			/* Not an integer key: PH7_HashmapKeyIsInt left it a printable string. */` |
|       - | 2400 | `			SyString sKey;` |
|      65 | 2401 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob));` |
|      65 | 2402 | `			SyBlobFormat(&sMsg,"Undefined array key \"%z\"",&sKey);` |
|       - | 2403 | `		}` |
|     115 | 2404 | `		SyBlobNullAppend(&sMsg);` |
|     115 | 2405 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|     115 | 2406 | `		SyBlobRelease(&sMsg);` |
|      55 | 2407 | `	}` |
|  233924 | 2408 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0` |
|  117038 | 2409 | `	 && (iP2 == 0 \|\| iP2 == 2)` |
|      28 | 2410 | `	 && !VmIdxFeedsCoalesce(pInstr) ){` |
|       - | 2411 | `		/* Subscripting a scalar base is a WARNING in php ("Trying to access array offset` |
|       - | 2412 | `		 * on int") that yields NULL. PH7 yielded NULL in silence. */` |
|      23 | 2413 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Trying to access array offset on %s",` |
|       7 | 2414 | `			VmArithValueName(pTos));` |
|       7 | 2415 | `	}` |
|  233804 | 2416 | `	if( iP2 == VM_IDX_CTX_UNSET && rc == SXRET_OK && pNode != 0` |
|    1111 | 2417 | `	 && (pTos->iFlags & MEMOBJ_HASHMAP) ){` |
|       - | 2418 | ``		/* php's `unset($a[k])` removes the ELEMENT. PH7 left the element's value slot`` |
|       - | 2419 | `		 * on the stack and let the trailing unset() builtin drop it — but dropping a` |
|       - | 2420 | `` 		 * SLOT unlinks everything that holds it, so `$r = &$a['k']; unset($a['k']);` `` |
|       - | 2421 | ``		 * destroyed $r too (`Undefined variable $r`) where php leaves it reading the`` |
|       - | 2422 | `		 * value it still refers to. Unlink the node itself, which releases the value` |
|       - | 2423 | `		 * only when this element was its last holder, and leave the builtin nothing. */` |
|    1111 | 2424 | `		ph7_hashmap *pTarget = (ph7_hashmap *)pTos->x.pOther;` |
|    1111 | 2425 | `		int bDone = 0;` |
|    1111 | 2426 | `		if( pTarget == pVm->pGlobal && pIdx ){` |
|       - | 2427 | ``			/* `$GLOBALS['x']` IS the global $x, so this is an unset of the NAME: it has`` |
|       - | 2428 | `			 * to drop the symbol-table entry as well as this node, and it must not` |
|       - | 2429 | `			 * destroy the value another holder still refers to — exactly what` |
|       - | 2430 | ``			 * VmUnsetVarByName does for `unset($x)`. A superglobal has no entry in the`` |
|       - | 2431 | `			 * global frame and falls through to the plain node unlink below. */` |
|     159 | 2432 | `			VmFrame *pGlobalFrame = pVm->pFrame;` |
|       - | 2433 | `			SyHashEntry *pNameEntry;` |
|     159 | 2434 | `			while( pGlobalFrame->pParent ){` |
|     ! 0 | 2435 | `				pGlobalFrame = pGlobalFrame->pParent;` |
|     ! 0 | 2436 | `			}` |
|     159 | 2437 | `			if( (pIdx->iFlags & MEMOBJ_STRING) == 0 ){` |
|       3 | 2438 | `				PH7_MemObjToString(pIdx);` |
|       1 | 2439 | `			}` |
|     238 | 2440 | `			pNameEntry = SyHashGet(&pGlobalFrame->hVar,` |
|     158 | 2441 | `				(const void *)SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob));` |
|     159 | 2442 | `			if( pNameEntry ){` |
|     238 | 2443 | `				sxi32 rcUnset = VmUnsetVarByNameEx(&(*pVm),pGlobalFrame,` |
|     158 | 2444 | `					(const char *)SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob),FALSE);` |
|     159 | 2445 | `				bDone = 1;` |
|     159 | 2446 | `				if( rcUnset == PH7_ABORT ){` |
|     ! 0 | 2447 | `					PH7_MemObjRelease(pIdx);` |
|     ! 0 | 2448 | `					VM_EXIT_ABORT;` |
|       - | 2449 | `				}` |
|      79 | 2450 | `			}` |
|      79 | 2451 | `		}` |
|    1111 | 2452 | `		if( !bDone ){` |
|     953 | 2453 | `			PH7_HashmapUnlinkNode(pNode,TRUE);` |
|     474 | 2454 | `		}` |
|    1111 | 2455 | `		if( pIdx ){` |
|    1111 | 2456 | `			PH7_MemObjRelease(pIdx);` |
|     553 | 2457 | `		}` |
|    1111 | 2458 | `		PH7_MemObjRelease(pTos);` |
|    1111 | 2459 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|    1111 | 2460 | `		pTos->nIdx = SXU32_HIGH;` |
|    1111 | 2461 | `		VM_EXIT_BREAK;` |
|       - | 2462 | `	}` |
|  232703 | 2463 | `	if( pIdx ){` |
|  232679 | 2464 | `		PH7_MemObjRelease(pIdx);` |
|  116337 | 2465 | `	}` |
|  232703 | 2466 | `	if( rc == SXRET_OK ){` |
|       - | 2467 | `		/* Load entry contents */` |
|  117455 | 2468 | `		if( pMap->iRef < 2 ){` |
|       - | 2469 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|       - | 2470 | `			 * of the entry value,rather than pointing to it.` |
|       - | 2471 | `			 */` |
|     541 | 2472 | `			pTos->nIdx = SXU32_HIGH;` |
|     541 | 2473 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|     273 | 2474 | `		}else{` |
|  116919 | 2475 | `			pTos->nIdx = pNode->nValIdx;` |
|  116919 | 2476 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|  116919 | 2477 | `			PH7_HashmapUnref(pMap);` |
|       - | 2478 | `		}` |
|   58730 | 2479 | `	}else{` |
|       - | 2480 | `		/* No such entry,load NULL */` |
|  115253 | 2481 | `		PH7_MemObjRelease(pTos);` |
|  115253 | 2482 | `		pTos->nIdx = SXU32_HIGH;` |
|       - | 2483 | `	}` |
|  232703 | 2484 | `	if( iP2 == 4 && (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       - | 2485 | `		/* isset() context: reduce a found element to the same non-null marker the` |
|       - | 2486 | `		 * ArrayAccess arm above pushes. A TEMPORARY array (a call's return value,` |
|       - | 2487 | ``		 * or an accessor's -- `isset(f()['k'])`, `isset($o->magic['k'])`) leaves no`` |
|       - | 2488 | `		 * variable index behind, and the trailing builtin read that as a CONSTANT` |
|       - | 2489 | `		 * and warned about it; php's isset() is a language construct with no such` |
|       - | 2490 | `		 * diagnostic. */` |
|   34743 | 2491 | `		PH7_MemObjRelease(pTos);` |
|   34743 | 2492 | `		pTos->x.iVal = 1;` |
|   34743 | 2493 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   34743 | 2494 | `		pTos->nIdx = SXU32_HIGH;` |
|   17369 | 2495 | `	}` |
|  232703 | 2496 | `	VM_EXIT_BREAK;` |
|     ! 0 | 2497 | `	VM_EXIT_BREAK;` |
|  574428 | 2498 | `}` |
|       - | 2499 |  |
|       - | 2500 | `/*` |
|       - | 2501 | ` * OP_LOAD_MAP: body moved verbatim from the OP_LOAD_MAP arm of` |
|       - | 2502 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 2503 | ` */` |
|  115576 | 2504 | `PH7_PRIVATE VmOpRc VmExecOpLoadMap(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 2505 | `{` |
|  115581 | 2506 | `	ph7_value *pTos = pState->pTos;` |
|  115581 | 2507 | `	ph7_value *pStack = pState->pStack;` |
|  115581 | 2508 | `	VmInstr *aInstr = pState->aInstr;` |
|  115581 | 2509 | `	sxi32 pc = pState->pc;` |
|       - | 2510 | `	sxi32 rc;` |
|   57788 | 2511 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - | 2512 | `	ph7_hashmap *pMap;` |
|       - | 2513 | `	/* Allocate a new hashmap instance */` |
|  115581 | 2514 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|  115581 | 2515 | `	if( pMap == 0 ){` |
|     ! 0 | 2516 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|     ! 0 | 2517 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|     ! 0 | 2518 | `		VM_EXIT_ABORT;` |
|       - | 2519 | `	}` |
|  115581 | 2520 | `	if( pInstr->iP1 > 0 ){` |
|   28245 | 2521 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|   28245 | 2522 | `		sxi32 rcSpread = SXRET_OK;` |
|       - | 2523 | `		/* Perform the insertion */` |
|  100511 | 2524 | `		while( pEntry < pTos ){` |
|   72291 | 2525 | `			if( pEntry[1].iFlags & MEMOBJ_AUX_SPREAD ){` |
|       - | 2526 | `				/* Array unpacking: '...$expr'. Merge entries with PHP 8.1` |
|       - | 2527 | `				 * semantics — string keys preserved (later wins), int keys` |
|       - | 2528 | `				 * renumbered. Same routine that backs array_merge. */` |
|     690 | 2529 | `				if( pEntry[1].iFlags & MEMOBJ_HASHMAP ){` |
|     664 | 2530 | `					sxi32 rcMerge = PH7_HashmapMerge((ph7_hashmap *)pEntry[1].x.pOther,pMap);` |
|     664 | 2531 | `					if( rcMerge != SXRET_OK ){` |
|       - | 2532 | `						/* Merge failure (OOM): match the PH7_NewHashmap OOM` |
|       - | 2533 | `						 * path — emit fatal and abort, leaving no partial` |
|       - | 2534 | `						 * map dangling. */` |
|     ! 0 | 2535 | `						VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|     ! 0 | 2536 | `							"Fatal, PH7 engine is running out of memory while spreading array at instruction #:%d",pc);` |
|     ! 0 | 2537 | `						rcSpread = PH7_ABORT;` |
|     ! 0 | 2538 | `						break;` |
|       2 | 2539 | `					}` |
|     358 | 2540 | `				}else if( VmValueIsTraversable(pVm,&pEntry[1]) ){` |
|       - | 2541 | `					/* Traversable unpacking (PHP 8.1): walk it into the map using the` |
|       - | 2542 | `					 * same key rules as array spread (string keys kept, int renumbered). */` |
|       7 | 2543 | `					sxi32 rcW = PH7_VmIteratorWalk(&(*pVm),&pEntry[1],VmSpreadMergeStep,pMap);` |
|       7 | 2544 | `					if( rcW == PH7_EXCEPTION \|\| rcW == PH7_ABORT ){` |
|       3 | 2545 | `						rcSpread = rcW;` |
|       3 | 2546 | `						break;` |
|       - | 2547 | `					}` |
|       3 | 2548 | `				}else{` |
|       - | 2549 | `					/* Throw a catchable Error matching PHP semantics. */` |
|      21 | 2550 | `					rcSpread = VmThrowSpreadError(&(*pVm),&pEntry[1]);` |
|      21 | 2551 | `					break;` |
|       2 | 2552 | `				}` |
|   71938 | 2553 | `			}else if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|       - | 2554 | `				/* Insertion by reference */` |
|     244 | 2555 | `				PH7_HashmapInsertByRef(pMap,` |
|     162 | 2556 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     162 | 2557 | `					(sxu32)pEntry[1].x.iVal` |
|       - | 2558 | `					);` |
|      82 | 2559 | `			}else{` |
|       - | 2560 | `				/* An explicit key in an array LITERAL gets the same php diagnostics a` |
|       - | 2561 | `				 * subscript does — a float key that truncates deprecates, and so does an` |
|       - | 2562 | `				 * explicit null key. Only the subscript sites (LOAD_IDX/STORE_IDX) used to` |
|       - | 2563 | ``				 * emit these, so `[1.5 => "v"]` and `[null => "v"]` were silent. A key that`` |
|       - | 2564 | `				 * is ABSENT (auto-index) is a NULL slot here, not a null key, so the` |
|       - | 2565 | `				 * MEMOBJ_NULL check below is what tells the two apart. */` |
|   71443 | 2566 | `					if( (pEntry->iFlags & MEMOBJ_AUX_NOKEY) == 0 ){` |
|       - | 2567 | `						/* An object/array literal key is php's TypeError, a resource one` |
|       - | 2568 | `						 * warns and becomes its id — same rules as a subscript. */` |
|       - | 2569 | `						SyBlob sTypeMsg;` |
|   15981 | 2570 | `						if( VmOffsetTypeRejected(&(*pVm),pEntry,0,&sTypeMsg) ){` |
|       3 | 2571 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg));` |
|       2 | 2572 | `						}else{` |
|   15979 | 2573 | `							VmOffsetResourceWarn(&(*pVm),pEntry);` |
|       - | 2574 | `						}` |
|       - | 2575 | `						/* php DEPRECATES a null literal key (then normalizes to "") and` |
|       - | 2576 | `						 * rejects nothing there; PHL matches that (deprecate + fall` |
|       - | 2577 | `						 * through, PH7_HashmapInsert casts NULL->""). A lossy-FLOAT` |
|       - | 2578 | `						 * literal key still rejects with a TypeError — the recorded` |
|       - | 2579 | `						 * non-deprecated-surface policy, same as the subscript site. */` |
|   15981 | 2580 | `						int bNull = (pEntry->iFlags & MEMOBJ_NULL) != 0;` |
|   23970 | 2581 | `						int bLossyFloat = (pEntry->iFlags & MEMOBJ_REAL) != 0` |
|   15976 | 2582 | `							&& pEntry->rVal != (ph7_real)(sxi64)pEntry->rVal;` |
|   15981 | 2583 | `						if( bNull ){` |
|       3 | 2584 | `							VmNullOffsetDeprecate(&(*pVm),pEntry);` |
|   15980 | 2585 | `						}else if( bLossyFloat ){` |
|       3 | 2586 | `							const char *zErr = "Cannot access offset of type float on array";` |
|       - | 2587 | `							SyBlob sErrMsg;` |
|       3 | 2588 | `							SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       3 | 2589 | `							SyBlobAppend(&sErrMsg,zErr,(sxu32)SyStrlen(zErr));` |
|       3 | 2590 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|       1 | 2591 | `						}` |
|    7988 | 2592 | `					}` |
|       - | 2593 | `				/* Standard insertion */` |
|  107162 | 2594 | `				PH7_HashmapInsert(pMap,` |
|   71438 | 2595 | `					(pEntry->iFlags & MEMOBJ_AUX_NOKEY) ? 0 /* Automatic index assign */ : pEntry,` |
|   35719 | 2596 | `					&pEntry[1]` |
|       - | 2597 | `				);` |
|       - | 2598 | `			}` |
|       - | 2599 | `			/* Next pair on the stack */` |
|   72271 | 2600 | `			pEntry += 2;` |
|       5 | 2601 | `		}` |
|       - | 2602 | `		/* Pop P1 elements */` |
|   28245 | 2603 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|   28245 | 2604 | `		if( rcSpread != SXRET_OK ){` |
|       - | 2605 | `			/* Discard the partially-built map and propagate the exception. */` |
|      23 | 2606 | `			PH7_HashmapRelease(pMap,TRUE);` |
|      23 | 2607 | `			if( rcSpread == PH7_ABORT ){` |
|     ! 0 | 2608 | `				VM_EXIT_ABORT;` |
|       - | 2609 | `			}` |
|       - | 2610 | `			{` |
|       - | 2611 | `				sxi32 iRp;` |
|      23 | 2612 | `				if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|       6 | 2613 | `					pc = iRp;` |
|       6 | 2614 | `					VM_EXIT_BREAK;` |
|       - | 2615 | `				}` |
|       - | 2616 | `			}` |
|      18 | 2617 | `			VM_EXIT_EXCEPTION;` |
|       - | 2618 | `		}` |
|   14110 | 2619 | `	}` |
|       - | 2620 | `	/* Push the hashmap */` |
|  115561 | 2621 | `	pTos++;` |
|  115561 | 2622 | `	pTos->nIdx = SXU32_HIGH;` |
|  115561 | 2623 | `	pTos->x.pOther = pMap;` |
|  115561 | 2624 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|  115561 | 2625 | `	VM_EXIT_BREAK;` |
|     ! 0 | 2626 | `	VM_EXIT_BREAK;` |
|   57793 | 2627 | `}` |
|       - | 2628 |  |
|       - | 2629 | `/*` |
|       - | 2630 | `` * Can `$o[$k]` be READ at all? php's read_dimension is either the class's own`` |
|       - | 2631 | ` * native handler -- which a class may carry WITHOUT implementing ArrayAccess,` |
|       - | 2632 | ` * php's DOMNodeList -- or the standard one, which needs the interface. Neither` |
|       - | 2633 | `` * is php's `Cannot use object of type C as array`.`` |
|       - | 2634 | ` */` |
|      28 | 2635 | `static int VmObjectDimReadable(ph7_vm *pVm,ph7_class_instance *pInst)` |
|       1 | 2636 | `{` |
|      29 | 2637 | `	if( pInst == 0 ){` |
|     ! 0 | 2638 | `		return 0;` |
|       - | 2639 | `	}` |
|      42 | 2640 | `	return PH7_ClassHasNativeDim(pInst->pClass)` |
|      28 | 2641 | `	    \|\| (pVm->pArrayAccessClass && PH7_VmInstanceOf(pInst->pClass,pVm->pArrayAccessClass));` |
|      15 | 2642 | `}` |
|       - | 2643 | `/*` |
|       - | 2644 | ` * One such READ, into pOut (which the caller inits and owns). The native` |
|       - | 2645 | ` * handler comes first for the same reason it does at the subscript opcode: php` |
|       - | 2646 | ` * implements the interface THROUGH the handler. A refusal is dropped here --` |
|       - | 2647 | ` * the only caller indexes 0..N-1 of its own target list, which no handler` |
|       - | 2648 | ` * refuses -- and pOut is simply left as it was.` |
|       - | 2649 | ` *` |
|       - | 2650 | ` * Answers the accessor's own status so a caller reading a RUN of positions can` |
|       - | 2651 | ` * stop where php stops: a userland offsetGet that THROWS abandons the rest of` |
|       - | 2652 | ` * the destructure, leaving every later target at its previous value.` |
|       - | 2653 | ` */` |
|      36 | 2654 | `static sxi32 VmObjectDimRead(ph7_vm *pVm,ph7_class_instance *pInst,ph7_value *pKey,ph7_value *pOut)` |
|       1 | 2655 | `{` |
|       - | 2656 | `	ph7_class_method *pGet;` |
|       - | 2657 | `	sxi32 rcCall;` |
|      37 | 2658 | `	if( PH7_ClassHasNativeDim(pInst->pClass) ){` |
|       - | 2659 | `		PH7_NativeDimCtx sDim;` |
|       5 | 2660 | `		sDim.iMode = PH7_NATIVE_DIM_READ;` |
|       5 | 2661 | `		sDim.pOffset = pKey;` |
|       5 | 2662 | `		sDim.pResult = pOut;` |
|       5 | 2663 | `		sDim.zThrowClass = 0;` |
|       5 | 2664 | `		sDim.zThrowMsg[0] = 0;` |
|       5 | 2665 | `		PH7_ClassNativeDim(pInst,&sDim);` |
|       5 | 2666 | `		return SXRET_OK;` |
|       - | 2667 | `	}` |
|      33 | 2668 | `	pGet = PH7_ClassExtractMethod(pInst->pClass,"offsetGet",sizeof("offsetGet")-1);` |
|      33 | 2669 | `	if( pGet == 0 ){` |
|     ! 0 | 2670 | `		return SXRET_OK;` |
|       - | 2671 | `	}` |
|       - | 2672 | `	{` |
|       - | 2673 | `		ph7_value *apArg[1];` |
|      33 | 2674 | `		apArg[0] = pKey;` |
|      33 | 2675 | `		rcCall = PH7_VmCallClassMethod(&(*pVm),pInst,pGet,pOut,1,apArg);` |
|       - | 2676 | `	}` |
|      33 | 2677 | `	return (rcCall == PH7_EXCEPTION \|\| pVm->nBoundaryRc != 0) ? PH7_EXCEPTION : SXRET_OK;` |
|      19 | 2678 | `}` |
|       - | 2679 | `/*` |
|       - | 2680 | ` * OP_LOAD_LIST: body moved verbatim from the OP_LOAD_LIST arm of` |
|       - | 2681 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 2682 | ` */` |
|  392524 | 2683 | `PH7_PRIVATE VmOpRc VmExecOpLoadList(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 2684 | `{` |
|  392529 | 2685 | `	ph7_value *pTos = pState->pTos;` |
|  392529 | 2686 | `	ph7_value *pStack = pState->pStack;` |
|  392529 | 2687 | `	VmInstr *aInstr = pState->aInstr;` |
|  392529 | 2688 | `	sxi32 pc = pState->pc;` |
|       - | 2689 | `	sxi32 rc;` |
|  196262 | 2690 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - | 2691 | `	ph7_value *pEntry;` |
|  392529 | 2692 | `	sxi32 rcEnforce = SXRET_OK;` |
|  392529 | 2693 | `	if( pInstr->iP1 <= 0 ){` |
|       - | 2694 | `		/* Empty list,break immediately */` |
|     ! 0 | 2695 | `		VM_EXIT_BREAK;` |
|       - | 2696 | `	}` |
|  392529 | 2697 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|       - | 2698 | `#ifdef UNTRUST` |
|       - | 2699 | `	if( &pEntry[-1] < pStack ){` |
|       - | 2700 | `		VM_EXIT_ABORT;` |
|       - | 2701 | `	}` |
|       - | 2702 | `#endif` |
|  392529 | 2703 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|  392485 | 2704 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|       - | 2705 | `		ph7_hashmap_node *pNode;` |
|       - | 2706 | `		ph7_value sKey,*pObj;` |
|       - | 2707 | `		/* Start Copying */` |
|  392485 | 2708 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
| 1567873 | 2709 | `		while( pEntry <= pTos ){` |
| 1175411 | 2710 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
| 1175381 | 2711 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
| 1175381 | 2712 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
| 2350605 | 2713 | `					int bTyped = SyHashTotalEntry(&pVm->hTypedSlot) > 0` |
| 1175376 | 2714 | `						&& SyHashGet(&pVm->hTypedSlot,(const void *)&pEntry->nIdx,sizeof(sxu32)) != 0;` |
| 1175381 | 2715 | `					if( rc != SXRET_OK ){` |
|       - | 2716 | `						/* Undefined array key */` |
|       - | 2717 | `						char zMsg[128];` |
|       5 | 2718 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|       5 | 2719 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|       2 | 2720 | `					}` |
| 1175381 | 2721 | `					if( !bTyped ){` |
| 1175345 | 2722 | `						if( rc == SXRET_OK ){` |
|       - | 2723 | `							/* Store node value */` |
| 1175345 | 2724 | `							PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|  587675 | 2725 | `						}else{` |
|     ! 0 | 2726 | `							PH7_MemObjRelease(pObj);` |
|       - | 2727 | `						}` |
|  587675 | 2728 | `					}else{` |
|       - | 2729 | ``						/* Typed/readonly property target (`[$o->p] = [...]`): a`` |
|       - | 2730 | `						 * direct slot write would bypass the typed-slot table, so` |
|       - | 2731 | `						 * enforce on a temp first — a TypeError leaves the property` |
|       - | 2732 | `						 * untouched, and a missing key assigns null, which a` |
|       - | 2733 | `						 * non-nullable type rejects exactly like php (warning, then` |
|       - | 2734 | `						 * "Cannot assign null to property ... of type ..."). */` |
|       - | 2735 | `						ph7_value sVal;` |
|      38 | 2736 | `						PH7_MemObjInit(&(*pVm),&sVal);` |
|      38 | 2737 | `						if( rc == SXRET_OK ){` |
|      34 | 2738 | `							PH7_HashmapExtractNodeValue(pNode,&sVal,TRUE);` |
|      16 | 2739 | `						}` |
|      38 | 2740 | `						rcEnforce = VmEnforcePropertyTypeOnStore(&(*pVm),pEntry->nIdx,&sVal,0);` |
|      38 | 2741 | `						if( rcEnforce != SXRET_OK ){` |
|       - | 2742 | `							/* Thrown: stop assigning (php aborts the list at the` |
|       - | 2743 | `							 * first failing element), settle the stack, route. */` |
|      20 | 2744 | `							PH7_MemObjRelease(&sVal);` |
|      20 | 2745 | `							break;` |
|       - | 2746 | `						}` |
|      20 | 2747 | `						PH7_MemObjStore(&sVal,pObj);` |
|      20 | 2748 | `						PH7_MemObjRelease(&sVal);` |
|       - | 2749 | `					}` |
|  587679 | 2750 | `				}` |
|  587679 | 2751 | `			}` |
| 1175393 | 2752 | `			sKey.x.iVal++; /* Next numeric index */` |
| 1175393 | 2753 | `			pEntry++;` |
|       5 | 2754 | `		}` |
|  196298 | 2755 | `	}else if( (pEntry[-1].iFlags & MEMOBJ_OBJ) && pEntry[-1].x.pOther ){` |
|       - | 2756 | `		/* php destructures an OBJECT through its read_dimension handler, one` |
|       - | 2757 | ``		 * READ per POSITION -- `[$a, , $c] = $o` asks for 0 and 2 and never 1 --`` |
|       - | 2758 | `		 * so an ArrayObject, an SplFixedArray and (since the handler landed) a` |
|       - | 2759 | `		 * DOMNodeList all come apart the way an array does. PHL treated every` |
|       - | 2760 | `` 		 * object as a non-array source: it warned `Cannot use object as array` `` |
|       - | 2761 | ``		 * and assigned NULL to every target, so `[$first, $second] = $list` --`` |
|       - | 2762 | `		 * the shape every modern DOM and SPL example is written in -- silently` |
|       - | 2763 | `		 * produced two nulls. An object with NO dimension reader is php's` |
|       - | 2764 | `		 * catchable Error rather than that warning, and it is raised before any` |
|       - | 2765 | ``		 * target is touched. (The KEYED spelling `['k' => $a] = $o` never came`` |
|       - | 2766 | `		 * here: the compiler routes it through OP_LOAD_IDX, which has had the` |
|       - | 2767 | `		 * accessor dispatch all along.) */` |
|      29 | 2768 | `		ph7_class_instance *pInst = (ph7_class_instance *)pEntry[-1].x.pOther;` |
|       - | 2769 | `		ph7_value sKey;` |
|      29 | 2770 | `		if( !VmObjectDimReadable(&(*pVm),pInst) ){` |
|       - | 2771 | `			/* Routed mid-expression, like every other catchable Error raised from` |
|       - | 2772 | `			 * an opcode that is not a call boundary: the destructure is abandoned` |
|       - | 2773 | `			 * and an enclosing try in THIS frame lands on its own handler. Settle` |
|       - | 2774 | `			 * the targets AND the source first — the statement's OP_POP is skipped` |
|       - | 2775 | `			 * when a catch resumes at the landing pad. */` |
|       - | 2776 | `			char zMsg[256];` |
|      11 | 2777 | `			SyString *pName = &pInst->pClass->sName;` |
|      16 | 2778 | `			sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - | 2779 | `				"Cannot use object of type %.*s as array",` |
|      10 | 2780 | `				(int)pName->nByte,pName->zString);` |
|      11 | 2781 | `			rc = VmThrowFromVm(&(*pVm),"Error",zMsg,nMsg);` |
|      11 | 2782 | `			VmPopOperand(&pTos,pInstr->iP1 + 1);` |
|      11 | 2783 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      11 | 2784 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|     ! 0 | 2785 | `		}else{` |
|      19 | 2786 | `			PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      59 | 2787 | `			while( pEntry <= pTos ){` |
|      43 | 2788 | `				if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */ ){` |
|      37 | 2789 | `					sxu32 nSlot = pEntry->nIdx;` |
|      73 | 2790 | `					int bTyped = SyHashTotalEntry(&pVm->hTypedSlot) > 0` |
|      36 | 2791 | `						&& SyHashGet(&pVm->hTypedSlot,(const void *)&nSlot,sizeof(sxu32)) != 0;` |
|       - | 2792 | `					ph7_value sVal,*pObj;` |
|      37 | 2793 | `					PH7_MemObjInit(&(*pVm),&sVal);` |
|      37 | 2794 | `					if( VmObjectDimRead(&(*pVm),pInst,&sKey,&sVal) != SXRET_OK ){` |
|       - | 2795 | `						/* The accessor threw: php abandons the destructure there,` |
|       - | 2796 | `						 * so every later target keeps the value it had -- and this` |
|       - | 2797 | `						 * target does too, since php assigns nothing for the read` |
|       - | 2798 | `						 * that failed. */` |
|       3 | 2799 | `						PH7_MemObjRelease(&sVal);` |
|       3 | 2800 | `						rcEnforce = PH7_EXCEPTION;` |
|       3 | 2801 | `						break;` |
|       - | 2802 | `					}` |
|      35 | 2803 | `					if( bTyped ){` |
|       - | 2804 | `						/* Same rule as the array source's typed target: enforce on` |
|       - | 2805 | `						 * the temp so a TypeError leaves the property untouched. */` |
|     ! 0 | 2806 | `						rcEnforce = VmEnforcePropertyTypeOnStore(&(*pVm),nSlot,&sVal,0);` |
|     ! 0 | 2807 | `						if( rcEnforce != SXRET_OK ){` |
|     ! 0 | 2808 | `							PH7_MemObjRelease(&sVal);` |
|     ! 0 | 2809 | `							break;` |
|       - | 2810 | `						}` |
|     ! 0 | 2811 | `					}` |
|       - | 2812 | `					/* Re-fetch AFTER the read: a userland offsetGet can reserve` |
|       - | 2813 | `					 * slots, and growing aMemObj relocates every pointer into it. */` |
|      35 | 2814 | `					pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nSlot);` |
|      35 | 2815 | `					if( pObj ){` |
|      35 | 2816 | `						PH7_MemObjStore(&sVal,pObj);` |
|      17 | 2817 | `					}` |
|      35 | 2818 | `					PH7_MemObjRelease(&sVal);` |
|      17 | 2819 | `				}` |
|      41 | 2820 | `				sKey.x.iVal++; /* Next numeric index */` |
|      41 | 2821 | `				pEntry++;` |
|       1 | 2822 | `			}` |
|      19 | 2823 | `			PH7_MemObjRelease(&sKey);` |
|       - | 2824 | `		}` |
|      10 | 2825 | `	}else{` |
|       - | 2826 | `		/* Source is not an array: php warns first (silencing ONLY null — a bool` |
|       - | 2827 | `		 * source warns too, php 8), then assigns null to every target. A typed` |
|       - | 2828 | `		 * property target receives that null THROUGH enforcement, so a` |
|       - | 2829 | `		 * non-nullable type throws "Cannot assign null to property ..." exactly` |
|       - | 2830 | `		 * like php instead of silently nulling the slot. PHL DIVERGENCE: bool` |
|       - | 2831 | `		 * FALSE stays silent (php warns) — it is the end-of-array sentinel of` |
|       - | 2832 | `` 		 * the documented each() extension's `while (list(..) = each($a))` `` |
|       - | 2833 | `		 * idiom, which would otherwise warn on every normal loop exit. */` |
|       - | 2834 | `		ph7_value *pObj;` |
|      29 | 2835 | `		int bFalseSrc = (pTos[-pInstr->iP1].iFlags & MEMOBJ_BOOL) != 0` |
|      16 | 2836 | `			&& pTos[-pInstr->iP1].x.iVal == 0;` |
|      18 | 2837 | `		if( (pTos[-pInstr->iP1].iFlags & MEMOBJ_NULL) == 0 && !bFalseSrc ){` |
|      11 | 2838 | `			VmWarnCannotUseAsArray(&(*pVm),pTos[-pInstr->iP1].iFlags);` |
|       5 | 2839 | `		}` |
|      32 | 2840 | `		while( pEntry <= pTos ){` |
|      22 | 2841 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|      22 | 2842 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|      41 | 2843 | `					int bTyped = SyHashTotalEntry(&pVm->hTypedSlot) > 0` |
|      20 | 2844 | `						&& SyHashGet(&pVm->hTypedSlot,(const void *)&pEntry->nIdx,sizeof(sxu32)) != 0;` |
|      22 | 2845 | `					if( !bTyped ){` |
|      14 | 2846 | `						PH7_MemObjRelease(pObj);` |
|       8 | 2847 | `					}else{` |
|       - | 2848 | `						ph7_value sVal;` |
|       9 | 2849 | `						PH7_MemObjInit(&(*pVm),&sVal);` |
|       9 | 2850 | `						rcEnforce = VmEnforcePropertyTypeOnStore(&(*pVm),pEntry->nIdx,&sVal,0);` |
|       9 | 2851 | `						if( rcEnforce != SXRET_OK ){` |
|       7 | 2852 | `							PH7_MemObjRelease(&sVal);` |
|       7 | 2853 | `							break;` |
|       - | 2854 | `						}` |
|       3 | 2855 | `						PH7_MemObjStore(&sVal,pObj);` |
|       3 | 2856 | `						PH7_MemObjRelease(&sVal);` |
|       - | 2857 | `					}` |
|       7 | 2858 | `				}` |
|       7 | 2859 | `			}` |
|      16 | 2860 | `			pEntry++;` |
|       2 | 2861 | `		}` |
|       - | 2862 | `	}` |
|  392519 | 2863 | `	if( rcEnforce != SXRET_OK ){` |
|       - | 2864 | `		/* Settle this op's own operands: the P1 entries AND the source value —` |
|       - | 2865 | `		 * its statement-level OP_POP is skipped when a catch resumes at the` |
|       - | 2866 | `		 * landing pad, so leaving it would leak one operand slot per caught` |
|       - | 2867 | `		 * throw. A NESTED destructure can still have the outer list's operands` |
|       - | 2868 | `		 * abandoned above the try's base, so on an in-place catch drain to the` |
|       - | 2869 | `		 * catching try's recorded depth (like the fetch-point router and the` |
|       - | 2870 | `		 * generator inject path), not just our own pops. */` |
|      28 | 2871 | `		VmPopOperand(&pTos,pInstr->iP1 + 1);` |
|      28 | 2872 | `		if( rcEnforce == PH7_ABORT ){` |
|     ! 0 | 2873 | `			VM_EXIT_ABORT;` |
|       - | 2874 | `		}` |
|       - | 2875 | `		{` |
|       - | 2876 | `			sxi32 _iRpL;` |
|      40 | 2877 | `			PH7_INLINE_RESUME_BREAK()` |
|      28 | 2878 | `			if( VmRecordedResume(pVm,&_iRpL,pState->pEntryFrame,aInstr) ){` |
|      28 | 2879 | `				PH7_RESUME_DRAIN()` |
|      26 | 2880 | `				pc = _iRpL;` |
|      26 | 2881 | `				VM_EXIT_BREAK;` |
|       - | 2882 | `			}` |
|       - | 2883 | `		}` |
|       3 | 2884 | `		VM_EXIT_EXCEPTION;` |
|       - | 2885 | `	}` |
|  392493 | 2886 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|  392493 | 2887 | `	VM_EXIT_BREAK;` |
|     ! 0 | 2888 | `	VM_EXIT_BREAK;` |
|  196267 | 2889 | `}` |
|       - | 2890 |  |
|       - | 2891 | `/*` |
|       - | 2892 | ` * OP_UNSET_VAR: body moved verbatim from the OP_UNSET_VAR arm of` |
|       - | 2893 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 2894 | ` */` |
|    8162 | 2895 | `PH7_PRIVATE VmOpRc VmExecOpUnsetVar(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 2896 | `{` |
|    8167 | 2897 | `	ph7_value *pTos = pState->pTos;` |
|    8167 | 2898 | `	ph7_value *pStack = pState->pStack;` |
|    8167 | 2899 | `	VmInstr *aInstr = pState->aInstr;` |
|    8167 | 2900 | `	sxi32 pc = pState->pc;` |
|       - | 2901 | `	sxi32 rc;` |
|    4081 | 2902 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - | 2903 | `	/* unset($name): p3 is the variable name. Drops the NAME only — see VmUnsetVarByName */` |
|    8167 | 2904 | `	SyString *pName = (SyString *)pInstr->p3;` |
|    8167 | 2905 | `	if( pName && pVm->pFrame ){` |
|       - | 2906 | `		/* Inside a try{} the VM pushes an EXCEPTION frame; variables live in the body` |
|       - | 2907 | `		 * frame below it, so skip past it exactly as every other variable path does.` |
|       - | 2908 | `		 * Without this, unset($x) inside a try silently found nothing and did nothing. */` |
|    8167 | 2909 | `		VmFrame *pVarFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|    8167 | 2910 | `		sxi32 rcU = VmUnsetVarByName(&(*pVm),pVarFrame,pName->zString,pName->nByte);` |
|    8167 | 2911 | `		if( rcU == PH7_ABORT ){` |
|       3 | 2912 | `			VM_EXIT_ABORT;` |
|       - | 2913 | `		}` |
|       - | 2914 | `		/* Releasing the last holder can run a __destruct(), and that destructor may` |
|       - | 2915 | `		 * throw. Such a throw is PARKED in nBoundaryRc by the boundary rail; consume it` |
|       - | 2916 | `		 * here and route it, or the catch runs and execution resumes inside the try` |
|       - | 2917 | `		 * ("resumed-dtor" instead of php's "caught-dtor"). */` |
|    8165 | 2918 | `		if( pVm->nBoundaryRc != 0 ){` |
|       3 | 2919 | `			rc = pVm->nBoundaryRc;` |
|       3 | 2920 | `			pVm->nBoundaryRc = 0;` |
|       3 | 2921 | `			if( rc == PH7_ABORT ){` |
|     ! 0 | 2922 | `				VM_EXIT_ABORT;` |
|       - | 2923 | `			}` |
|       3 | 2924 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2925 | `		}` |
|    4079 | 2926 | `	}` |
|    8163 | 2927 | `	VM_EXIT_BREAK;` |
|     ! 0 | 2928 | `	VM_EXIT_BREAK;` |
|    4086 | 2929 | `}` |
|       - | 2930 |  |
