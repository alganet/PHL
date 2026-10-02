# src/ph7/vm_ops_load.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1495/1637 lines (91.33%)

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
|    5066 |   28 | `PH7_PRIVATE VmOpRc VmExecOpStoreRef(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |   29 | `{` |
|    5071 |   30 | `	ph7_value *pTos = pState->pTos;` |
|    5071 |   31 | `	ph7_value *pStack = pState->pStack;` |
|    5071 |   32 | `	VmInstr *aInstr = pState->aInstr;` |
|    5071 |   33 | `	sxi32 pc = pState->pc;` |
|       - |   34 | `	sxi32 rc;` |
|    2533 |   35 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    5071 |   36 | `	 SyString sName = { 0 , 0 };` |
|       - |   37 | `	 VmFrame *pFrameLocal;` |
|       - |   38 | `	SyHashEntry *pEntry;` |
|       - |   39 | `	sxu32 nIdx;` |
|       - |   40 | `#ifdef UNTRUST` |
|       - |   41 | `	if( pTos < pStack ){` |
|       - |   42 | `		VM_EXIT_ABORT;` |
|       - |   43 | `	}` |
|       - |   44 | `#endif` |
|    5071 |   45 | `	if( pInstr->iP2 == 1 ){` |
|       - |   46 | ``		/* Member reference target: `$o->p =& $x` / `self::$s =& $x`. The`` |
|       - |   47 | `		 * preceding OP_MEMBER (PH7_MEMBER_REF_TARGET) resolved the property slot` |
|       - |   48 | `		 * and stashed it. Stack: [ ... , source, member-result(top) ]. Rebind the` |
|       - |   49 | `		 * property's nIdx to alias the source variable's slot and pin that slot` |
|       - |   50 | `		 * past its owning frame (like a use(&$x) capture) so neither frame` |
|       - |   51 | `		 * teardown nor a later unset recycles it while the property aliases it. */` |
|      74 |   52 | `		ph7_value *pSrc = &pTos[-1];` |
|      74 |   53 | `		sxu32 nSrcIdx = pSrc->nIdx;` |
|      74 |   54 | `		VmClassAttr *pVmAttr = pVm->pRefTargetAttr;` |
|      74 |   55 | `		ph7_class_attr *pStAttr = pVm->pRefTargetStaticAttr;` |
|      74 |   56 | `		if( pSrc->iFlags & MEMOBJ_AUX_STROFFSET ){` |
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
|      69 |   72 | `		if( nSrcIdx == SXU32_HIGH ){` |
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
|      69 |   94 | `		if( nSrcIdx == SXU32_HIGH ){` |
|       - |   95 | `			/* Reservation failed: nothing to bind to. */` |
|      69 |   96 | `		}else if( pVmAttr ){` |
|      57 |   97 | `			PH7_VmBindAttrRef(&(*pVm),pVmAttr,nSrcIdx);` |
|      41 |   98 | `		}else if( pStAttr ){` |
|      13 |   99 | `			sxu32 nOldIdx = pStAttr->nIdx;` |
|      13 |  100 | `			if( nOldIdx != nSrcIdx ){` |
|       - |  101 | `				/* Give the previous target back, exactly as the instance arm above does.` |
|       - |  102 | `				 * A permanent pin was left on every slot the property had ever named, so` |
|       - |  103 | `				 * each of them stayed a REFERENCE for the rest of the script — which an` |
|       - |  104 | ``				 * ordinary array COPY then shared (`$d = $a; $d[0] = 99;` wrote through`` |
|       - |  105 | ``				 * to `$a[0]`, silently), since "is this element a reference" is answered`` |
|       - |  106 | `				 * by who still holds it. */` |
|      13 |  107 | `				if( pStAttr->iFlags & (PH7_CLASS_ATTR_REFBOUND\|PH7_CLASS_ATTR_REFSRCPIN) ){` |
|       5 |  108 | `					if( (pStAttr->iFlags & PH7_CLASS_ATTR_REFBOUND) == 0 ){` |
|     ! 0 |  109 | `						PH7_VmStoreFilterDrop(&(*pVm),pStAttr,nOldIdx);` |
|     ! 0 |  110 | `					}` |
|       5 |  111 | `					VmUnpinMemObjSlot(&(*pVm),nOldIdx);` |
|       3 |  112 | `				}else{` |
|       - |  113 | `					/* The static's own (unshared) slot. A reference-bound property bypasses` |
|       - |  114 | `					 * typed coercion in php, so drop any typed-slot enforcement entry too. */` |
|       9 |  115 | `					PH7_VmStoreFilterDrop(&(*pVm),pStAttr,nOldIdx);` |
|       9 |  116 | `					PH7_VmUnsetMemObj(&(*pVm),nOldIdx,TRUE);` |
|       - |  117 | `				}` |
|      13 |  118 | `				pStAttr->nIdx = nSrcIdx;` |
|      13 |  119 | `				pStAttr->iFlags \|= PH7_CLASS_ATTR_REFBOUND;` |
|      13 |  120 | `				pStAttr->iFlags &= ~PH7_CLASS_ATTR_REFSRCPIN;` |
|      13 |  121 | `				VmPinMemObjSlotCounted(&(*pVm),nSrcIdx);` |
|       6 |  122 | `			}` |
|       6 |  123 | `		}` |
|      69 |  124 | `		if( pVm->pRefTargetThis ){` |
|      57 |  125 | `			PH7_ClassInstanceUnref(pVm->pRefTargetThis);` |
|      28 |  126 | `		}` |
|      69 |  127 | `		pVm->pRefTargetAttr = 0;` |
|      69 |  128 | `		pVm->pRefTargetStaticAttr = 0;` |
|      69 |  129 | `		pVm->pRefTargetThis = 0;` |
|       - |  130 | `		/* Pop the member-result; leave the source as the expression value. */` |
|      69 |  131 | `		VmPopOperand(&pTos,1);` |
|      69 |  132 | `		VM_EXIT_BREAK;` |
|       - |  133 | `	}` |
|    4999 |  134 | `	if( pInstr->p3 == 0 ){` |
|       - |  135 | `		char *zName;` |
|       - |  136 | `		/* Take the variable name from the Next on the stack */` |
|     ! 0 |  137 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  138 | `			/* Force a string cast */` |
|     ! 0 |  139 | `			PH7_MemObjToString(pTos);` |
|     ! 0 |  140 | `		}` |
|     ! 0 |  141 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|     ! 0 |  142 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|     ! 0 |  143 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     ! 0 |  144 | `			if( zName ){` |
|     ! 0 |  145 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|     ! 0 |  146 | `			}` |
|     ! 0 |  147 | `		}` |
|     ! 0 |  148 | `		PH7_MemObjRelease(pTos);` |
|     ! 0 |  149 | `		pTos--;` |
|     ! 0 |  150 | `	}else{` |
|    4999 |  151 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|       - |  152 | `	}` |
|    4999 |  153 | `	if( pTos->iFlags & MEMOBJ_AUX_STROFFSET ){` |
|       - |  154 | `		/* php: a string OFFSET cannot be either end of a reference. The value read` |
|       - |  155 | `		 * out of a string still carries the BASE VARIABLE's slot, so binding it` |
|       - |  156 | `		 * aliased the whole string and a later write through the reference REPLACED` |
|       - |  157 | `		 * it ($s = "abc"; $r = &$s[1]; $r = "Z"; left $s === "Z"). Same Error the` |
|       - |  158 | `		 * by-ref ARGUMENT path raises for f($s[1]). */` |
|      10 |  159 | `		rc = VmThrowFromVm(&(*pVm),"Error","Cannot create references to/from string offsets",` |
|       - |  160 | `			sizeof("Cannot create references to/from string offsets")-1);` |
|      10 |  161 | `		PH7_MemObjRelease(pTos);` |
|      10 |  162 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|      10 |  163 | `		pTos->nIdx = SXU32_HIGH;` |
|      10 |  164 | `		if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      10 |  165 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  166 | `	}` |
|    4991 |  167 | `	nIdx = pTos->nIdx;` |
|       - |  168 | `	{` |
|       - |  169 | ``		/* `$r = &$o->p` on a property no write may reach: php refuses the BIND,`` |
|       - |  170 | `		 * because the alias would let a later write through $r reach the property` |
|       - |  171 | ``		 * with nothing in the way. PHL bound it, so `$r = 99` afterwards rewrote a`` |
|       - |  172 | `		 * readonly property and a DatePeriod's recurrence count alike. */` |
|    4991 |  173 | `		sxi32 rcNw = PH7_VmCheckIndirectModify(&(*pVm),nIdx);` |
|    4991 |  174 | `		if( rcNw != SXRET_OK ){` |
|      11 |  175 | `			PH7_MemObjRelease(pTos);` |
|      11 |  176 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      11 |  177 | `			pTos->nIdx = SXU32_HIGH;` |
|      11 |  178 | `			if( pInstr->p3 == 0 && sName.zString ){` |
|     ! 0 |  179 | `				SyMemBackendFree(&pVm->sAllocator,(void *)sName.zString);` |
|     ! 0 |  180 | `			}` |
|      11 |  181 | `			if( rcNw == PH7_ABORT ){ VM_EXIT_ABORT; }` |
|      11 |  182 | `			PH7_THROW_ROUTE_MIDEXPR(rcNw)` |
|       - |  183 | `		}` |
|       - |  184 | `	}` |
|    4981 |  185 | `	if(nIdx == SXU32_HIGH ){` |
|       - |  186 | `		{` |
|       - |  187 | `			/* No slot behind the source. php binds a FRESH variable holding the value` |
|       - |  188 | `			 * — whatever its type — and the only thing it ever says about it is the` |
|       - |  189 | ``			 * notice below, so PHL's `Reference operator require a variable not a`` |
|       - |  190 | ``			 * constant as it's right operand` fired on every scalar: `$r =& f()`,`` |
|       - |  191 | ``			 * `$r =& f()[0]` and `$r =& mk()->p` all refused the bind and left $r`` |
|       - |  192 | `			 * undefined too. (Object/array/resource sources already took this path.)` |
|       - |  193 | ``			 * A source WRITTEN as a call is php's `Only variables should be assigned`` |
|       - |  194 | ``			 * by reference`, raised when the callee did not return by reference —`` |
|       - |  195 | `			 * the compiler marked it, since a temporary the call was only the BASE of` |
|       - |  196 | ``			 * (`f()[0]`) is silent. */`` |
|       - |  197 | `			ph7_value *pObj;` |
|      22 |  198 | `			if( (pInstr->iP1 & PH7_STOREREF_CALLSRC)` |
|      17 |  199 | `			 && (pTos->iFlags & (MEMOBJ_AUX_NATIVEPROP\|MEMOBJ_AUX_REFRET)) == 0 ){` |
|       8 |  200 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|       - |  201 | `					"Only variables should be assigned by reference");` |
|       3 |  202 | `			}` |
|       - |  203 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      24 |  204 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      24 |  205 | `			if( pObj == 0 ){` |
|     ! 0 |  206 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - |  207 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|     ! 0 |  208 | `				VM_EXIT_ABORT;` |
|       - |  209 | `			}` |
|       - |  210 | `			/* Perform the store operation */` |
|      24 |  211 | `			PH7_MemObjStore(pTos,pObj);` |
|      24 |  212 | `			pTos->nIdx = pObj->nIdx;` |
|       - |  213 | `		}` |
|    4970 |  214 | `	}else if( sName.nByte > 0){` |
|    4959 |  215 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|       - |  216 | `			/* php 8.1's non-catchable fatal (compile-time there) */` |
|       3 |  217 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot acquire reference to $GLOBALS");` |
|       3 |  218 | `			pVm->iExitStatus = 255;` |
|       3 |  219 | `			pVm->bHaltRequested = 1;` |
|       3 |  220 | `			VM_EXIT_ABORT;` |
|     ! 0 |  221 | `		}else{` |
|    4957 |  222 | `			pFrameLocal = pVm->pFrame;` |
|    4957 |  223 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       - |  224 | `			/* Query the local frame */` |
|    4957 |  225 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|    4957 |  226 | `			if( pEntry ){` |
|       - |  227 | ``				/* php RE-BINDS a name that already exists (`$y = 2; $y = &$x;`, and the`` |
|       - |  228 | ``				 * `$r = &$a[$k]` idiom from the second loop step on) — the old binding`` |
|       - |  229 | `				 * goes, its value with it if nothing else holds it. */` |
|    4692 |  230 | `				PH7_VmRebindVarSlot(&(*pVm),pFrameLocal,pEntry,sName.zString,sName.nByte,nIdx);` |
|    4692 |  231 | `				if( pInstr->p3 == 0 && sName.zString ){` |
|       - |  232 | `					/* The name was duplicated for a symbol-table key this rebind does` |
|       - |  233 | `					 * not need — the entry keeps the key it was created with. */` |
|     ! 0 |  234 | `					SyMemBackendFree(&pVm->sAllocator,(void *)sName.zString);` |
|     ! 0 |  235 | `				}` |
|    2347 |  236 | `			}else{` |
|     267 |  237 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       - |  238 | `				/* Installing a name is a rebind too, for a frame that may still` |
|       - |  239 | `				 * remember where the name USED to live -- the same reason` |
|       - |  240 | `				 * PH7_VmBindVarSlot flushes on its own insert branch. */` |
|     267 |  241 | `				VmVarMemoFlush(pFrameLocal);` |
|     267 |  242 | `				if( pFrameLocal->pParent == 0 ){` |
|       - |  243 | `					/* Insert in the $GLOBALS array */` |
|     173 |  244 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|      84 |  245 | `				}` |
|     267 |  246 | `				if( rc == SXRET_OK ){` |
|     267 |  247 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|     131 |  248 | `				}` |
|       - |  249 | `			}` |
|       - |  250 | `		}` |
|    2476 |  251 | `	}` |
|    4979 |  252 | `	VM_EXIT_BREAK;` |
|     ! 0 |  253 | `	VM_EXIT_BREAK;` |
|    2538 |  254 | `}` |
|       - |  255 |  |
|       - |  256 | `/* LOAD_IDX's own iP2 context codes (they do NOT line up with PH7_MEMBER_*).` |
|       - |  257 | ` * The two UNSET codes live in ph7int.h: the property opcode has to recognize an` |
|       - |  258 | ` * unset-subscript BASE, which is an indirect modification of what the base holds. */` |
|       - |  259 | `#define VM_IDX_CTX_ISSET 4` |
|       - |  260 | `#define VM_IDX_CTX_EMPTY 6` |
|       - |  261 | `/*` |
|       - |  262 | ` * php rejects an OBJECT or an ARRAY used as an array offset, naming the offending` |
|       - |  263 | ` * type the way get_debug_type() does — the CLASS name for an object, "array" for` |
|       - |  264 | ` * an array — and wording the failure by context:` |
|       - |  265 | ` *` |
|       - |  266 | ` *   read/write   Cannot access offset of type Foo on array` |
|       - |  267 | ` *   isset/empty  Cannot access offset of type Foo in isset or empty` |
|       - |  268 | ` *   unset        Cannot unset offset of type Foo on array` |
|       - |  269 | ` *` |
|       - |  270 | ` * A RESOURCE is deliberately absent: php does not reject it, it warns` |
|       - |  271 | ` * ("Resource ID#N used as offset, casting to integer (N)") and uses the id as an` |
|       - |  272 | ` * integer key. PH7_VmOffsetResourceWarn() below handles that half.` |
|       - |  273 | ` *` |
|       - |  274 | ` * Returns TRUE and fills pMsg when the key must be rejected. iCtx is the` |
|       - |  275 | ` * instruction's iP2 (any value other than the isset/unset/empty codes reads as` |
|       - |  276 | ` * an access).` |
|       - |  277 | ` */` |
|  545866 |  278 | `static int VmOffsetTypeRejected(ph7_vm *pVm,ph7_value *pKey,int iCtx,SyBlob *pMsg)` |
|       5 |  279 | `{` |
|       - |  280 | `	const char *zType;` |
|  545871 |  281 | `	SyString *pClass = 0;` |
|  545871 |  282 | `	if( pKey == 0 ){` |
|     ! 0 |  283 | `		return FALSE;` |
|       - |  284 | `	}` |
|  545871 |  285 | `	if( pKey->iFlags & MEMOBJ_OBJ ){` |
|      38 |  286 | `		ph7_class_instance *pInst = (ph7_class_instance *)pKey->x.pOther;` |
|      38 |  287 | `		if( pInst && pInst->pClass ){` |
|      38 |  288 | `			pClass = &pInst->pClass->sName;` |
|      17 |  289 | `		}` |
|      38 |  290 | `		zType = "object";` |
|  545854 |  291 | `	}else if( pKey->iFlags & MEMOBJ_HASHMAP ){` |
|      25 |  292 | `		zType = "array";` |
|      14 |  293 | `	}else{` |
|  545815 |  294 | `		return FALSE;` |
|       - |  295 | `	}` |
|      60 |  296 | `	SyBlobInit(pMsg,&pVm->sAllocator);` |
|      60 |  297 | `	if( VM_IDX_IS_UNSET(iCtx) ){` |
|       3 |  298 | `		SyBlobAppend(pMsg,"Cannot unset offset of type ",sizeof("Cannot unset offset of type ")-1);` |
|       2 |  299 | `	}else{` |
|      58 |  300 | `		SyBlobAppend(pMsg,"Cannot access offset of type ",sizeof("Cannot access offset of type ")-1);` |
|       - |  301 | `	}` |
|      60 |  302 | `	if( pClass ){` |
|      38 |  303 | `		SyBlobAppend(pMsg,pClass->zString,pClass->nByte);` |
|      21 |  304 | `	}else{` |
|      25 |  305 | `		SyBlobAppend(pMsg,zType,(sxu32)SyStrlen(zType));` |
|       - |  306 | `	}` |
|      60 |  307 | `	if( iCtx == VM_IDX_CTX_ISSET \|\| iCtx == VM_IDX_CTX_EMPTY ){` |
|       7 |  308 | `		SyBlobAppend(pMsg," in isset or empty",sizeof(" in isset or empty")-1);` |
|       4 |  309 | `	}else{` |
|      54 |  310 | `		SyBlobAppend(pMsg," on array",sizeof(" on array")-1);` |
|       - |  311 | `	}` |
|      60 |  312 | `	return TRUE;` |
|  272764 |  313 | `}` |
|       - |  314 | `/*` |
|       - |  315 | ` * php's other half of the offset-type rules: a RESOURCE offset is accepted, with` |
|       - |  316 | `` * `Warning: Resource ID#N used as offset, casting to integer (N)`, and the key`` |
|       - |  317 | ` * becomes that integer. Rewrites pKey in place so the normal integer-key path` |
|       - |  318 | ` * takes over.` |
|       - |  319 | ` */` |
|  546008 |  320 | `PH7_PRIVATE void PH7_VmOffsetResourceWarn(ph7_vm *pVm,ph7_value *pKey)` |
|       5 |  321 | `{` |
|       - |  322 | `	sxu32 nId;` |
|  546013 |  323 | `	if( pKey == 0 \|\| (pKey->iFlags & MEMOBJ_RES) == 0 ){` |
|  545941 |  324 | `		return;` |
|       - |  325 | `	}` |
|      74 |  326 | `	nId = PH7_VmResourceId(pVm,pKey->x.pOther);` |
|     110 |  327 | `	VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|      36 |  328 | `		"Resource ID#%u used as offset, casting to integer (%u)",nId,nId);` |
|      74 |  329 | `	PH7_MemObjRelease(pKey);` |
|      74 |  330 | `	pKey->x.iVal = (sxi64)nId;` |
|      74 |  331 | `	MemObjSetType(pKey,MEMOBJ_INT);` |
|  272835 |  332 | `}` |
|       - |  333 | `/*` |
|       - |  334 | ` * php DEPRECATES (it does not reject) a NULL array offset, then normalizes it to` |
|       - |  335 | `` * the empty-string key "": `$a[null]`, `$a[$undef]` and `[null => v]` all warn`` |
|       - |  336 | `` * `Using null as an array offset is deprecated, use an empty string instead` and`` |
|       - |  337 | ` * then read/write the "" slot. Emit that E_DEPRECATED notice and return TRUE so` |
|       - |  338 | ` * the caller falls through to the ordinary lookup/insert, which already casts` |
|       - |  339 | ` * NULL->"" (HashmapLookup / HashmapInsert). A LOSSY-FLOAT offset stays a rejected` |
|       - |  340 | ` * TypeError: PHL deliberately targets php's NON-deprecated surface for the` |
|       - |  341 | ` * float->int truncation (VmRejectFloatOperand policy), so only the null case` |
|       - |  342 | ` * is coerced here. Returns FALSE (no notice) for a non-null key.` |
|       - |  343 | ` */` |
|  546345 |  344 | `PH7_PRIVATE int PH7_VmNullOffsetDeprecate(ph7_vm *pVm,ph7_value *pKey)` |
|       5 |  345 | `{` |
|  546350 |  346 | `	if( pKey == 0 \|\| (pKey->iFlags & MEMOBJ_NULL) == 0 ){` |
|  546296 |  347 | `		return FALSE;` |
|       - |  348 | `	}` |
|      58 |  349 | `	PH7_VmThrowError(&(*pVm),0,E_DEPRECATED,` |
|       - |  350 | `		"Using null as an array offset is deprecated, use an empty string instead");` |
|      58 |  351 | `	return TRUE;` |
|  273065 |  352 | `}` |
|       - |  353 | `/*` |
|       - |  354 | ` * The three rules above, applied to a BUILTIN's key argument.` |
|       - |  355 | ` *` |
|       - |  356 | `` * php's array_key_exists() does not run a `string\|int` ZPP row on its $key — it`` |
|       - |  357 | `` * hands the value to the same offset machinery `$a[$key]` uses, so the two agree`` |
|       - |  358 | ` * on every type: an object or an array is the catchable` |
|       - |  359 | `` * `Cannot access offset of type X on array`, a RESOURCE warns and becomes its`` |
|       - |  360 | `` * integer id, `null` deprecates and reads the "" key, and a bool/float/numeric`` |
|       - |  361 | ` * string folds the way any subscript folds. PHL's builtin had its own narrower` |
|       - |  362 | `` * check and therefore its own answers — a `string\|int` TypeError for the two`` |
|       - |  363 | `` * cases php ACCEPTS (null, lossy float) and a silent `false` for the three php`` |
|       - |  364 | ` * REJECTS or coerces (object, array, resource). The object case was the worst of` |
|       - |  365 | ` * them: a __toString() object was stringified and could answer TRUE for a key` |
|       - |  366 | ` * php refuses to look up at all.` |
|       - |  367 | ` *` |
|       - |  368 | ` * The same rules are what php applies wherever a key arrives as a VALUE rather` |
|       - |  369 | `` * than as a subscript — a Traversable's `key()` collected by iterator_to_array()`` |
|       - |  370 | `` * or cached by CachingIterator, and array_column()'s `$index_key` COLUMN — so the`` |
|       - |  371 | ` * rail is shared by all of them. A door that skips it keys the array by the` |
|       - |  372 | `` * string CAST instead, which is a key php never writes (`"Array"`, `"Object"`,`` |
|       - |  373 | `` * `"Resource id #N"`) and, for an object, a key php refuses outright.`` |
|       - |  374 | ` *` |
|       - |  375 | ` * iWording picks which of php's sentences the caller reports (PH7_ARRAYKEY_*).` |
|       - |  376 | ` * php words the illegal-type rejection differently in the key_exists alias than in` |
|       - |  377 | `` * array_key_exists() itself — `key_exists(): Argument #1 ($key) must be a valid`` |
|       - |  378 | `` * array offset type` vs the engine's offset Error — verified against 8.5.8; the`` |
|       - |  379 | ` * null-key DEPRECATION names array_key_exists() for BOTH of those spellings and is` |
|       - |  380 | `` * the engine's own `Using null as an array offset is deprecated` everywhere else.`` |
|       - |  381 | ` *` |
|       - |  382 | ` * pKey is rewritten in place (resource -> int), so callers pass a private copy,` |
|       - |  383 | ` * not the caller's own argument slot. Returns SXRET_OK when the key is usable, or` |
|       - |  384 | ` * the status of the TypeError thrown.` |
|       - |  385 | ` */` |
|     448 |  386 | `PH7_PRIVATE sxi32 PH7_VmArrayKeyArg(ph7_context *pCtx,ph7_value *pKey,int iWording)` |
|       5 |  387 | `{` |
|     453 |  388 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - |  389 | `	SyBlob sMsg;` |
|     453 |  390 | `	if( VmOffsetTypeRejected(pVm,pKey,0,&sMsg) ){` |
|       - |  391 | `		sxi32 rc;` |
|      30 |  392 | `		if( iWording == PH7_ARRAYKEY_ZPP ){` |
|       5 |  393 | `			SyBlobRelease(&sMsg);` |
|       7 |  394 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  395 | `				"%s(): Argument #1 ($key) must be a valid array offset type",` |
|       2 |  396 | `				ph7_function_name(pCtx));` |
|       - |  397 | `		}` |
|      38 |  398 | `		rc = PH7_VmThrowException(pCtx,"TypeError","%.*s",` |
|      24 |  399 | `			(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      26 |  400 | `		SyBlobRelease(&sMsg);` |
|      26 |  401 | `		return rc;` |
|       - |  402 | `	}` |
|     425 |  403 | `	PH7_VmOffsetResourceWarn(pVm,pKey);` |
|     420 |  404 | `	if( (pKey->iFlags & MEMOBJ_REAL)` |
|     225 |  405 | `	 && pKey->rVal != (ph7_real)(sxi64)pKey->rVal ){` |
|       - |  406 | `		/* php DEPRECATES the lossy float and truncates; PHL rejects it, here as` |
|       - |  407 | ``		 * everywhere else (the scope policy) — and with the SAME message `$a[5.7]` gives, so`` |
|       - |  408 | `		 * the builtin and the subscript stay one rule. */` |
|      15 |  409 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  410 | `			"Cannot access offset of type float on array");` |
|       - |  411 | `	}` |
|     413 |  412 | `	if( pKey->iFlags & MEMOBJ_NULL ){` |
|      10 |  413 | `		if( iWording == PH7_ARRAYKEY_OFFSET ){` |
|       7 |  414 | `			PH7_VmNullOffsetDeprecate(pVm,pKey);` |
|       4 |  415 | `		}else{` |
|       - |  416 | `			/* php names the parameter here rather than "an array offset"; the effect` |
|       - |  417 | `			 * is the engine's — the lookup below reads the "" key. */` |
|       3 |  418 | `			PH7_VmThrowError(pVm,0,E_DEPRECATED,` |
|       - |  419 | `				"Using null as the key parameter for array_key_exists() is deprecated, "` |
|       - |  420 | `				"use an empty string instead");` |
|       - |  421 | `		}` |
|       4 |  422 | `	}` |
|     413 |  423 | `	return SXRET_OK;` |
|     229 |  424 | `}` |
|       - |  425 | `/*` |
|       - |  426 | ` * Does this string START with an integer php's is_numeric_string would answer` |
|       - |  427 | ` * IS_LONG for? Returns 0 when it does not — no digits at all ("", "-", "p"), a` |
|       - |  428 | ` * FLOAT-shaped prefix ("1.5", ".5", "1.", "1e2", "1E2x") or a run that overflows` |
|       - |  429 | ` * a signed 64-bit int ("9223372036854775808") — 1 when it does and only optional` |
|       - |  430 | ` * trailing WHITESPACE follows (" 12 ", "+12", "007"), and 2 when it does but` |
|       - |  431 | ` * trailing DATA follows ("12abc", "0x1", "1e", "1 x"), which is php's` |
|       - |  432 | `` * `Illegal string offset` warning. *piVal receives the integer for 1 and 2.`` |
|       - |  433 | ` *` |
|       - |  434 | ` * php reaches the same three answers through is_numeric_string_ex(allow_errors=1)` |
|       - |  435 | ` * plus its IS_LONG/IS_DOUBLE verdict: a fraction or a well-formed exponent makes` |
|       - |  436 | ` * the verdict IS_DOUBLE, which a string offset refuses, while a bare 'e' is just` |
|       - |  437 | ` * trailing data.` |
|       - |  438 | ` */` |
|     104 |  439 | `static int VmStringOffsetInt(const char *zIn,sxu32 nByte,sxi64 *piVal)` |
|       4 |  440 | `{` |
|     108 |  441 | `	const char *z = zIn, *zEnd = &zIn[nByte], *zDigit;` |
|     108 |  442 | `	sxu64 uVal = 0, uLimit;` |
|     108 |  443 | `	int isNeg = 0, nDigit, i;` |
|     120 |  444 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){` |
|      14 |  445 | `		z++;` |
|       2 |  446 | `	}` |
|     108 |  447 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|       7 |  448 | `		isNeg = z[0] == '-';` |
|       7 |  449 | `		z++;` |
|       3 |  450 | `	}` |
|     108 |  451 | `	zDigit = z;` |
|     248 |  452 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|     142 |  453 | `		z++;` |
|       2 |  454 | `	}` |
|     108 |  455 | `	nDigit = (int)(z - zDigit);` |
|     108 |  456 | `	if( nDigit < 1 ){` |
|      44 |  457 | `		return 0;` |
|       - |  458 | `	}` |
|      66 |  459 | `	if( z < zEnd && z[0] == '.' ){` |
|       - |  460 | `		/* "1." and "1.5" alike: php reads a double from here. */` |
|       5 |  461 | `		return 0;` |
|       - |  462 | `	}` |
|      62 |  463 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|       3 |  464 | `		const char *zExp = &z[1];` |
|       3 |  465 | `		if( zExp < zEnd && (zExp[0] == '+' \|\| zExp[0] == '-') ){` |
|     ! 0 |  466 | `			zExp++;` |
|     ! 0 |  467 | `		}` |
|       3 |  468 | `		if( zExp < zEnd && (unsigned char)zExp[0] < 0xc0 && SyisDigit(zExp[0]) ){` |
|       3 |  469 | `			return 0;` |
|       - |  470 | `		}` |
|     ! 0 |  471 | `	}` |
|       - |  472 | `	/* Accumulate unsigned so PHP_INT_MIN's magnitude (2^63) is representable —` |
|       - |  473 | `	 * "-9223372036854775808" is a legal offset, "9223372036854775808" is not. */` |
|      64 |  474 | `	while( nDigit > 1 && zDigit[0] == '0' ){` |
|       5 |  475 | `		zDigit++; nDigit--;` |
|       1 |  476 | `	}` |
|      60 |  477 | `	uLimit = isNeg ? (sxu64)SXI64_HIGH + 1 : (sxu64)SXI64_HIGH;` |
|      60 |  478 | `	if( nDigit > 19 ){` |
|     ! 0 |  479 | `		return 0;` |
|       - |  480 | `	}` |
|     188 |  481 | `	for( i = 0 ; i < nDigit ; ++i ){` |
|     132 |  482 | `		sxu64 d = (sxu64)(zDigit[i] - '0');` |
|     132 |  483 | `		if( uVal > (uLimit - d)/10 ){` |
|       3 |  484 | `			return 0;` |
|       - |  485 | `		}` |
|     130 |  486 | `		uVal = uVal*10 + d;` |
|      66 |  487 | `	}` |
|      58 |  488 | `	*piVal = isNeg ? (sxi64)(~uVal + 1) : (sxi64)uVal;` |
|      68 |  489 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){` |
|      12 |  490 | `		z++;` |
|       2 |  491 | `	}` |
|      58 |  492 | `	return z == zEnd ? 1 : 2;` |
|      56 |  493 | `}` |
|       - |  494 | `/*` |
|       - |  495 | ` * php's offset rules for a STRING container (zend_check_string_offset, and the` |
|       - |  496 | ` * isset/empty arm of ZEND_ISSET_ISEMPTY_DIM_OBJ). They are NOT the array rules,` |
|       - |  497 | ` * and PHL applied none of them: every offset went through an int cast, so` |
|       - |  498 | `` * `$s[""]`, `$s["-"]` and `$s["p"]` all answered `$s[0]` — a silent wrong answer`` |
|       - |  499 | ` * on code php refuses to run. php's table:` |
|       - |  500 | ` *` |
|       - |  501 | ` *   int                     the offset` |
|       - |  502 | ` *   null / bool / float     Warning: String offset cast occurred, then cast` |
|       - |  503 | ` *   integer-shaped string   the offset (leading/trailing space and '+' allowed)` |
|       - |  504 | ` *   int-then-garbage string Warning: Illegal string offset "12abc", then 12` |
|       - |  505 | ` *   any other string        TypeError: Cannot access offset of type string on string` |
|       - |  506 | ` *   array/object/resource   TypeError, naming the type (an object's CLASS)` |
|       - |  507 | ` *` |
|       - |  508 | ` * isset()/empty() raise NOTHING and answer "not set" for every shape the read` |
|       - |  509 | `` * path would reject OR warn about: `isset($s["0x1"])` is false even though`` |
|       - |  510 | `` * reading it warns and yields `$s[0]`. A `??` fetch sits BETWEEN that and a real`` |
|       - |  511 | ` * read — it suppresses the not-set diagnostics but still warns about the offset` |
|       - |  512 | ` * SHAPE and still reads it. iLevel selects which of the three (VM_STROFF_LOUD /` |
|       - |  513 | ` * _COALESCE / _ISSET).` |
|       - |  514 | ` */` |
| 1071851 |  515 | `PH7_PRIVATE int VmStringOffsetResolve(ph7_vm *pVm,ph7_value *pIdx,int iLevel,sxi64 *piOfft,SyBlob *pMsg)` |
|       5 |  516 | `{` |
| 1071856 |  517 | `	if( (pIdx->iFlags & MEMOBJ_INT) != 0 && (pIdx->iFlags & MEMOBJ_REAL) == 0 ){` |
| 1071684 |  518 | `		*piOfft = pIdx->x.iVal;` |
| 1071684 |  519 | `		return VM_STROFF_OK;` |
|       - |  520 | `	}` |
|     176 |  521 | `	if( pIdx->iFlags & MEMOBJ_STRING ){` |
|     160 |  522 | `		int eInt = VmStringOffsetInt((const char *)SyBlobData(&pIdx->sBlob),` |
|      52 |  523 | `			SyBlobLength(&pIdx->sBlob),piOfft);` |
|     108 |  524 | `		if( eInt == 1 ){` |
|      24 |  525 | `			return VM_STROFF_OK;` |
|       - |  526 | `		}` |
|      86 |  527 | `		if( eInt == 2 && iLevel != VM_STROFF_ISSET ){` |
|       - |  528 | ``			/* int-then-garbage. This is the one diagnostic a `??` fetch keeps:`` |
|       - |  529 | ``			 * `$s["1x"] ?? "d"` warns and answers $s[1] (PHL called it "not set"`` |
|       - |  530 | `			 * and answered the default — a wrong VALUE, not just a missing` |
|       - |  531 | `			 * warning). Only isset()/empty() — and the intermediate step of an` |
|       - |  532 | `			 * unset chain, which php keeps quiet about the shape — stay silent. */` |
|      28 |  533 | `			if( iLevel != VM_STROFF_UNSETBASE ){` |
|       - |  534 | `				SyString sKey;` |
|      28 |  535 | `				SyStringInitFromBuf(&sKey,SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob));` |
|      28 |  536 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Illegal string offset \"%z\"",&sKey);` |
|      13 |  537 | `			}` |
|      28 |  538 | `			return VM_STROFF_OK;` |
|       - |  539 | `		}` |
|      60 |  540 | `		if( iLevel != VM_STROFF_LOUD && iLevel != VM_STROFF_UNSETBASE ){` |
|      24 |  541 | `			return VM_STROFF_MISS;` |
|       4 |  542 | `		}` |
|      87 |  543 | `	}else if( (pIdx->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|       - |  544 | `		/* null / bool / float: php casts, but says so in a real read or write —` |
|       - |  545 | `		 * and in the intermediate step of an unset chain, which reads the offset` |
|       - |  546 | ``		 * to hand it on (`unset($s[1.5][0])` warns about the cast). */`` |
|      48 |  547 | `		if( iLevel == VM_STROFF_LOUD \|\| iLevel == VM_STROFF_UNSETBASE ){` |
|      34 |  548 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"String offset cast occurred");` |
|      16 |  549 | `		}` |
|      48 |  550 | `		PH7_MemObjToInteger(pIdx);` |
|      48 |  551 | `		*piOfft = pIdx->x.iVal;` |
|      48 |  552 | `		return VM_STROFF_OK;` |
|      24 |  553 | `	}else if( iLevel == VM_STROFF_ISSET ){` |
|       - |  554 | `		/* An array/object/resource offset is "not set" for isset()/empty() — but a` |
|       - |  555 | ``		 * `??` fetch RAISES for it (probed: `$s[[]] ?? "d"` is the TypeError while`` |
|       - |  556 | ``		 * `isset($s[[]])` is false), so only the fully-quiet level answers MISS. */`` |
|      10 |  557 | `		return VM_STROFF_MISS;` |
|       - |  558 | `	}` |
|       - |  559 | `	{` |
|       - |  560 | `		char zBuf[128];` |
|      52 |  561 | `		SyBlobInit(pMsg,&pVm->sAllocator);` |
|      76 |  562 | `		SyBlobFormat(pMsg,"Cannot access offset of type %s on string",` |
|      24 |  563 | `			VmValueGivenName(pIdx,zBuf,sizeof(zBuf)));` |
|       - |  564 | `	}` |
|      52 |  565 | `	return VM_STROFF_REJECT;` |
|  538898 |  566 | `}` |
|       - |  567 | `/*` |
|       - |  568 | ` * OP_STORE_IDX_REF: body moved verbatim from the OP_STORE_IDX_REF arm of` |
|       - |  569 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  570 | ` */` |
|  547275 |  571 | `PH7_PRIVATE VmOpRc VmExecOpStoreIdxRef(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  572 | `{` |
|  547280 |  573 | `	ph7_value *pTos = pState->pTos;` |
|  547280 |  574 | `	ph7_value *pStack = pState->pStack;` |
|  547280 |  575 | `	VmInstr *aInstr = pState->aInstr;` |
|  547280 |  576 | `	sxi32 pc = pState->pc;` |
|       - |  577 | `	sxi32 rc;` |
|  273525 |  578 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|  547280 |  579 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|       - |  580 | `	ph7_value *pKey;` |
|       - |  581 | `	sxu32 nIdx;` |
|  547280 |  582 | `	if( pInstr->iP1 & 1 ){` |
|       - |  583 | `		/* Key is next on stack (bit 1 is PH7_STOREREF_CALLSRC, not a key) */` |
|   95140 |  584 | `		pKey = pTos;` |
|   95140 |  585 | `		pTos--;` |
|   47490 |  586 | `	}else{` |
|  452145 |  587 | `		pKey = 0;` |
|       - |  588 | `	}` |
|       - |  589 | `		/* php DEPRECATES a null / lossy-float write subscript (then normalizes "" /` |
|       - |  590 | ``		 * truncates); it does not reject either. A null key — by-VALUE (`$a[$k]=v`)`` |
|       - |  591 | ``		 * OR by-REF (`$a[$k]=&$x`) — deprecates + coerces to the "" key: the notice is`` |
|       - |  592 | `		 * emitted DOWN AT THE INSERT (below), not here, so a null/false container that` |
|       - |  593 | ``		 * auto-vivifies (`$x=null; $x[null]=v`) gets it too (the base is not yet a`` |
|       - |  594 | `		 * hashmap at this point), and PH7_HashmapInsert / HashmapInsertByRef both cast` |
|       - |  595 | `		 * NULL->"". A lossy-FLOAT subscript stays a loud TypeError in BOTH forms (the` |
|       - |  596 | `		 * recorded non-deprecated-surface policy). */` |
|  547280 |  597 | `		if( pKey && (pTos->iFlags & MEMOBJ_HASHMAP) ){` |
|       - |  598 | `			SyBlob sTypeMsg;` |
|       - |  599 | `			/* An object/array key is php's TypeError; a resource key warns and` |
|       - |  600 | `			 * becomes its integer id. Both used to be stringified silently. */` |
|   94074 |  601 | `			if( VmOffsetTypeRejected(&(*pVm),pKey,0,&sTypeMsg) ){` |
|       - |  602 | `				sxi32 rcSc;` |
|       8 |  603 | `				PH7_MemObjRelease(pKey);` |
|       8 |  604 | `				VmPopOperand(&pTos,1);` |
|       8 |  605 | `				rcSc = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|      12 |  606 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       8 |  607 | `				rc = rcSc;` |
|       8 |  608 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  609 | `			}` |
|   94068 |  610 | `			PH7_VmOffsetResourceWarn(&(*pVm),pKey);` |
|   94063 |  611 | `			if( (pKey->iFlags & MEMOBJ_REAL)` |
|   46957 |  612 | `			 && pKey->rVal != (ph7_real)(sxi64)pKey->rVal ){` |
|       - |  613 | `				sxi32 rcSc;` |
|       3 |  614 | `				const char *zErr = "Cannot access offset of type float on array";` |
|       3 |  615 | `				PH7_MemObjRelease(pKey);` |
|       3 |  616 | `				VmPopOperand(&pTos,1);` |
|       3 |  617 | `				rcSc = VmThrowFromVm(&(*pVm),"TypeError",zErr,(sxu32)SyStrlen(zErr));` |
|       3 |  618 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       3 |  619 | `				rc = rcSc;` |
|       3 |  620 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  621 | `			}` |
|   46948 |  622 | `		}` |
|  547272 |  623 | `	if( pTos->iFlags & MEMOBJ_AUX_STROFFSET ){` |
|       - |  624 | ``		/* The CONTAINER is a string offset: `$s[0][1] = 'x'`, `$s[0][] = 'x'`,`` |
|       - |  625 | ``		 * `$a['k'][0][1] = 'x'`. php refuses to reach inside one — `Cannot use string`` |
|       - |  626 | ``		 * offset as an array`, the same refusal the fetch path raises — and this is`` |
|       - |  627 | `		 * where the outermost level of an ordinary assignment arrives, its LOAD_IDX` |
|       - |  628 | `		 * folded into the store. The character read out of the string still carries` |
|       - |  629 | ``		 * the BASE STRING's slot, so the write landed ON the base: `$s = 'ab';`` |
|       - |  630 | ``		 * $s[0][1] = 'x';` left `$s === 'ax'`, in silence. */`` |
|       - |  631 | `		sxi32 rcSo;` |
|       9 |  632 | `		if( pKey ){` |
|       7 |  633 | `			PH7_MemObjRelease(pKey);` |
|       3 |  634 | `		}` |
|       9 |  635 | `		VmPopOperand(&pTos,1);` |
|       9 |  636 | `		rcSo = VmThrowFromVm(&(*pVm),"Error","Cannot use string offset as an array",` |
|       - |  637 | `			sizeof("Cannot use string offset as an array")-1);` |
|       9 |  638 | `		if( rcSo == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       9 |  639 | `		rc = rcSo;` |
|       9 |  640 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  641 | `	}` |
|  547264 |  642 | `	nIdx = pTos->nIdx;` |
|       - |  643 | `	{` |
|       - |  644 | `		/* ArrayAccess::offsetSet dispatch.` |
|       - |  645 | `		 * Container may be on the stack as MEMOBJ_OBJ, or referenced via` |
|       - |  646 | `		 * the backing variable slot at nIdx. */` |
|  547264 |  647 | `		ph7_class_instance *pInst = 0;` |
|  547264 |  648 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|     599 |  649 | `			pInst = (ph7_class_instance *)pTos->x.pOther;` |
|  546967 |  650 | `		}else if( nIdx != SXU32_HIGH ){` |
|  546594 |  651 | `			ph7_value *pBacking = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
|  546594 |  652 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_OBJ) ){` |
|     ! 0 |  653 | `				pInst = (ph7_class_instance *)pBacking->x.pOther;` |
|     ! 0 |  654 | `			}` |
|  273182 |  655 | `		}` |
|  547264 |  656 | `		if( pInst ){` |
|     599 |  657 | `			ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|     599 |  658 | `			if( pArrayAccess && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|       - |  659 | `				ph7_class_method *pMeth;` |
|       - |  660 | `				ph7_value sNullKey;` |
|       - |  661 | `				ph7_value *apArg[2];` |
|     551 |  662 | `				if( pInstr->iOp == PH7_OP_STORE_IDX_REF ){` |
|     ! 0 |  663 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - |  664 | `						"Cannot assign by reference to overloaded object");` |
|     ! 0 |  665 | `					if( pKey ){ PH7_MemObjRelease(pKey); }` |
|     ! 0 |  666 | `					VmPopOperand(&pTos,2); /* container + value */` |
|     ! 0 |  667 | `					VM_EXIT_BREAK;` |
|       - |  668 | `				}` |
|     551 |  669 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - |  670 | `					"offsetSet",sizeof("offsetSet")-1);` |
|       - |  671 | `				/* Pop container; pTos now points to the value */` |
|     551 |  672 | `				VmPopOperand(&pTos,1);` |
|     551 |  673 | `				if( pKey == 0 ){` |
|      19 |  674 | `					PH7_MemObjInit(&(*pVm),&sNullKey);` |
|      19 |  675 | `					apArg[0] = &sNullKey;` |
|      11 |  676 | `				}else{` |
|     535 |  677 | `					apArg[0] = pKey;` |
|       - |  678 | `				}` |
|     551 |  679 | `				apArg[1] = pTos;` |
|     551 |  680 | `				if( pMeth ){` |
|     551 |  681 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,0,2,apArg);` |
|     273 |  682 | `				}` |
|     551 |  683 | `				if( pKey ){` |
|     535 |  684 | `					PH7_MemObjRelease(pKey);` |
|     270 |  685 | `				}else{` |
|      19 |  686 | `					PH7_MemObjRelease(&sNullKey);` |
|       - |  687 | `				}` |
|       - |  688 | `				/* The VALUE stays on the stack: a store IS an expression and its` |
|       - |  689 | `` 				 * result is what was assigned, which is why `$x = ($c[$k] = 42)` `` |
|       - |  690 | ``				 * and `return $this[$k] = 42;` are 42 in php even when offsetSet`` |
|       - |  691 | `				 * stored something else. This arm popped it, so the expression it` |
|       - |  692 | ``				 * belongs to read a slot the stack no longer owned -- `var_dump($c[$k]`` |
|       - |  693 | ``				 * = 5)` printed garbage for a native ArrayAccess and SEGFAULTED for a`` |
|       - |  694 | `				 * user one. The ordinary hashmap store below pops only the container,` |
|       - |  695 | `				 * for exactly this reason. */` |
|     551 |  696 | `				VM_EXIT_BREAK;` |
|       - |  697 | `			}` |
|       - |  698 | `			/* Object without ArrayAccess, but with a dimension handler that` |
|       - |  699 | `			 * really STORES: php's SimpleXMLElement writes an attribute for` |
|       - |  700 | ``			 * `$x['a'] = '1'` and appends an element for `$x->kid[] = 'v'`,`` |
|       - |  701 | `			 * through a write_dimension it has instead of the interface. The` |
|       - |  702 | `			 * handler is offered the write before the refusal below, and takes` |
|       - |  703 | `			 * it or leaves it -- DOMNodeList and PDORow leave it. */` |
|      48 |  704 | `			if( pInstr->iOp == PH7_OP_STORE_IDX_REF` |
|      27 |  705 | `			 && PH7_ClassHasNativeDim(pInst->pClass) ){` |
|       - |  706 | `				/* php words a by-reference store into a class whose OWN handler` |
|       - |  707 | `				 * answers dimensions differently from one that answers none:` |
|       - |  708 | `` 				 * `Cannot assign by reference to an array dimension of an object` `` |
|       - |  709 | ``				 * rather than `Cannot use object of type C as array`, which stays`` |
|       - |  710 | `				 * the answer for a plain object. (An ArrayAccess class has its own` |
|       - |  711 | `				 * third sentence, raised above.) */` |
|     ! 0 |  712 | `				const char *zRef = "Cannot assign by reference to an array dimension of an object";` |
|     ! 0 |  713 | `				rc = VmThrowFromVm(pVm,"Error",zRef,(sxu32)SyStrlen(zRef));` |
|     ! 0 |  714 | `				if( pKey ){ PH7_MemObjRelease(pKey); }` |
|     ! 0 |  715 | `				VmPopOperand(&pTos,2);` |
|     ! 0 |  716 | `				if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     ! 0 |  717 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  718 | `			}` |
|      51 |  719 | `			if( pInstr->iOp != PH7_OP_STORE_IDX_REF ){` |
|       - |  720 | `				PH7_NativeDimCtx sDim;` |
|       - |  721 | `				/* The stack is [value, container]; the container is popped only` |
|       - |  722 | `				 * once the handler has taken the write, so the refusal below` |
|       - |  723 | `				 * still sees both operands. */` |
|      75 |  724 | `				if( PH7_ClassNativeDimStore(pInst,` |
|      24 |  725 | `					pKey == 0 ? PH7_NATIVE_DIM_APPEND : PH7_NATIVE_DIM_WRITE,` |
|      24 |  726 | `					pKey,pTos - 1,&sDim) ){` |
|      43 |  727 | `					if( pKey ){ PH7_MemObjRelease(pKey); }` |
|      43 |  728 | `					if( sDim.zThrowClass ){` |
|       - |  729 | `						/* The refusal takes BOTH operands, like the ordinary one` |
|       - |  730 | `						 * below: the store never happened, so its value is not the` |
|       - |  731 | `						 * expression's result. */` |
|       8 |  732 | `						VmPopOperand(&pTos,2);` |
|      11 |  733 | `						rc = VmThrowFromVm(pVm,sDim.zThrowClass,sDim.zThrowMsg,` |
|       6 |  734 | `							(sxu32)SyStrlen(sDim.zThrowMsg));` |
|      28 |  735 | `						if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       8 |  736 | `						PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  737 | `					}` |
|      36 |  738 | `					VmPopOperand(&pTos,1);` |
|       - |  739 | `					/* The VALUE stays on the stack: a store IS an expression. */` |
|      36 |  740 | `					VM_EXIT_BREAK;` |
|       - |  741 | `				}` |
|       4 |  742 | `			}` |
|       - |  743 | `			/* Otherwise: PHP throws a fatal Error rather` |
|       - |  744 | `			 * than silently coercing the object into a hashmap (which is` |
|       - |  745 | `			 * what the legacy PH7 fall-through would do via MemObjToHashmap` |
|       - |  746 | `			 * a few lines below). Match PHP -- and let a class whose READ` |
|       - |  747 | `			 * handler answers word its own refusal, which is how php's` |
|       - |  748 | ``			 * PDORow says `Cannot write to PDORow offset` and, for the`` |
|       - |  749 | ``			 * keyless spelling, `Cannot append to PDORow offset`. */`` |
|       - |  750 | `			{` |
|       - |  751 | `				char zMsg[256];` |
|      14 |  752 | `				sxu32 nMsg = PH7_ClassNativeDimRefusal(pInst,` |
|       4 |  753 | `					pKey == 0 ? PH7_NATIVE_DIM_APPEND : PH7_NATIVE_DIM_WRITE,` |
|       4 |  754 | `					zMsg,sizeof(zMsg));` |
|      10 |  755 | `				rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|      10 |  756 | `				if( pKey ){ PH7_MemObjRelease(pKey); }` |
|      10 |  757 | `				VmPopOperand(&pTos,2); /* container + value */` |
|      10 |  758 | `				if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      10 |  759 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  760 | `			}` |
|       - |  761 | `		}` |
|       - |  762 | `	}` |
|  546670 |  763 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       - |  764 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|       - |  765 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|       - |  766 | `		 * checking true sharing count, then re-add after separation. */` |
|  546150 |  767 | `		if( nIdx != SXU32_HIGH ){` |
|  546100 |  768 | `			ph7_value *pBacking = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
|  819260 |  769 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|  546100 |  770 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|       - |  771 | `				/* Only adjust refcount / perform COW if the backing variable` |
|       - |  772 | `				 * is still sharing the same hashmap instance. This mirrors` |
|       - |  773 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|       - |  774 | `				 * refcounts if the backing array was already separated. */` |
|  546100 |  775 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|  546100 |  776 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|  546100 |  777 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|  546100 |  778 | `					pMap->iRef++;  /* Re-add stack ref */` |
|  546100 |  779 | `					pTos->x.pOther = pMap;` |
|  272940 |  780 | `				}else{` |
|       - |  781 | `					/* Backing variable no longer points at pCur: skip COW here` |
|       - |  782 | `					 * and operate on the hashmap currently on the stack. */` |
|     ! 0 |  783 | `					pMap = pCur;` |
|       - |  784 | `				}` |
|  272940 |  785 | `			}else{` |
|     ! 0 |  786 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       - |  787 | `			}` |
|  272940 |  788 | `		}else{` |
|      51 |  789 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       - |  790 | `		}` |
|  546150 |  791 | `		if( pMap->iRef < 2 ){` |
|       - |  792 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|       - |  793 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|       - |  794 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|       - |  795 | `			 * no code checks iRef for COW decisions. */` |
|      49 |  796 | `			pMap->iRef = 2;` |
|      24 |  797 | `		}` |
|  272965 |  798 | `	}else{` |
|       - |  799 | `		ph7_value *pObj;` |
|     525 |  800 | `		pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
|     525 |  801 | `		if( pObj == 0 ){` |
|       - |  802 | `			/* No slot behind the container: this is a write THROUGH a TEMPORARY` |
|       - |  803 | ``			 * (`f()[0] = 5`, `f()[0][1] = 5`). php still screens the base's TYPE the`` |
|       - |  804 | `			 * same way it does for a variable — writing an index into an int, a` |
|       - |  805 | `			 * float, a resource or a bool is its catchable "Cannot use a scalar value` |
|       - |  806 | `			 * as an array" — and only the value it writes is discarded with the` |
|       - |  807 | `			 * temporary. PHL skipped the screen along with the write, so the whole` |
|       - |  808 | `			 * statement ran in silence. A NULL base keeps php's silence: the array it` |
|       - |  809 | `			 * vivifies into dies with the temporary, and so does an offset written` |
|       - |  810 | `			 * into a temporary STRING. */` |
|      27 |  811 | `			if( (pTos->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_RES\|MEMOBJ_BOOL)) != 0 ){` |
|       - |  812 | `				sxi32 rcSc;` |
|      13 |  813 | `				if( pKey ){` |
|      13 |  814 | `					PH7_MemObjRelease(pKey);` |
|       6 |  815 | `				}` |
|      13 |  816 | `				VmPopOperand(&pTos,1);` |
|      13 |  817 | `				rcSc = VmThrowFromVm(&(*pVm),"Error","Cannot use a scalar value as an array",` |
|       - |  818 | `					sizeof("Cannot use a scalar value as an array")-1);` |
|      13 |  819 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      13 |  820 | `				rc = rcSc;` |
|      21 |  821 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  822 | `			}` |
|      15 |  823 | `			if( pKey ){` |
|      15 |  824 | `			  PH7_MemObjRelease(pKey);` |
|       7 |  825 | `			}` |
|      15 |  826 | `			VmPopOperand(&pTos,1);` |
|      15 |  827 | `			VM_EXIT_BREAK;` |
|       - |  828 | `		}` |
|       - |  829 | `		/* Phase#1: Load the array */` |
|     499 |  830 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|     431 |  831 | `			VmPopOperand(&pTos,1);` |
|     431 |  832 | `			if( pKey == 0 ){` |
|       - |  833 | ``				/* `$s[] = 'x'` on a STRING: php raises the catchable Error`` |
|       - |  834 | `				 * "[] operator not supported for strings" and leaves the string` |
|       - |  835 | `				 * untouched. PHL silently APPENDED, so code that meant to build` |
|       - |  836 | `				 * an array from a variable holding a string quietly produced a` |
|       - |  837 | `				 * longer string instead of failing — a wrong answer, not a` |
|       - |  838 | `				 * missing diagnostic. */` |
|       - |  839 | `				SyBlob sErrMsg;` |
|       8 |  840 | `				SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       8 |  841 | `				SyBlobAppend(&sErrMsg,"[] operator not supported for strings",` |
|       - |  842 | `					sizeof("[] operator not supported for strings")-1);` |
|       8 |  843 | `				VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       8 |  844 | `				VM_EXIT_BREAK;` |
|     ! 0 |  845 | `			}else{` |
|     425 |  846 | `				sxi64 iOfft = 0;` |
|       - |  847 | `				SyBlob sTypeMsg;` |
|       - |  848 | `				/* php's offset rules run BEFORE the RHS is looked at: an offset it` |
|       - |  849 | `				 * refuses is the TypeError alone. The RHS cast below used to happen` |
|       - |  850 | ``				 * first, so every rejected shape — and `$s[] = [1,2]` above — came`` |
|       - |  851 | ``				 * with a spurious `Array to string conversion` in front of it. */`` |
|     425 |  852 | `				if( VmStringOffsetResolve(&(*pVm),pKey,VM_STROFF_LOUD,&iOfft,&sTypeMsg) == VM_STROFF_REJECT ){` |
|       - |  853 | `					sxi32 rcSc;` |
|       7 |  854 | `					PH7_MemObjRelease(pKey);` |
|       7 |  855 | `					rcSc = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|      16 |  856 | `					if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       7 |  857 | `					rc = rcSc;` |
|       7 |  858 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  859 | `				}` |
|       - |  860 | `				/* Force a string cast on the RHS (user-visible: an array warns` |
|       - |  861 | `				 * "Array to string conversion" before the offset write, and a` |
|       - |  862 | `				 * not-stringable object throws — the target string is untouched) */` |
|       - |  863 | `				{` |
|     419 |  864 | `					sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|     419 |  865 | `					if( rcSv != SXRET_OK ){` |
|       5 |  866 | `						PH7_MemObjRelease(pKey);` |
|       7 |  867 | `						PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|     ! 0 |  868 | `					}` |
|       - |  869 | `				}` |
|     415 |  870 | `				if( VmStringOffsetWrite(&(*pVm),pObj,iOfft,pTos) != SXRET_OK ){` |
|       - |  871 | `					sxi32 rcEm;` |
|       9 |  872 | `					PH7_MemObjRelease(pKey);` |
|       9 |  873 | `					rcEm = VmThrowFromVm(&(*pVm),"Error",` |
|       - |  874 | `						"Cannot assign an empty string to a string offset",` |
|       - |  875 | `						sizeof("Cannot assign an empty string to a string offset")-1);` |
|       9 |  876 | `					if( rcEm == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       9 |  877 | `					rc = rcEm;` |
|       9 |  878 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  879 | `				}` |
|       - |  880 | `			}` |
|     407 |  881 | `			if( pKey ){` |
|     407 |  882 | `			  PH7_MemObjRelease(pKey);` |
|     201 |  883 | `			}` |
|     407 |  884 | `			VM_EXIT_BREAK;` |
|      72 |  885 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       - |  886 | `			/* php: only NULL and FALSE auto-vivify into an array. Writing an index into an` |
|       - |  887 | `			 * int/float/resource/TRUE is a catchable Error -- PH7 quietly REPLACED the value` |
|       - |  888 | `			 * with an array, destroying it ($x = 5; $x[0] = 1; left $x === [1]). */` |
|       - |  889 | `			/* php auto-vivifies false into an array with an 8.1 DEPRECATION; PHL` |
|       - |  890 | `			 * rejects any scalar base, false included (null still auto-vivifies). */` |
|      72 |  891 | `			int bScalar = (pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_RES\|MEMOBJ_BOOL)) != 0;` |
|      72 |  892 | `			if( bScalar ){` |
|       - |  893 | `				sxi32 rcSc;` |
|      10 |  894 | `				if( pKey ){` |
|       7 |  895 | `					PH7_MemObjRelease(pKey);` |
|       3 |  896 | `				}` |
|      10 |  897 | `				VmPopOperand(&pTos,1);` |
|      10 |  898 | `				rcSc = VmThrowFromVm(&(*pVm),"Error","Cannot use a scalar value as an array",` |
|       - |  899 | `					sizeof("Cannot use a scalar value as an array")-1);` |
|      10 |  900 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      10 |  901 | `				rc = rcSc;` |
|      12 |  902 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  903 | `			}` |
|       - |  904 | `			/* Force a hashmap cast  */` |
|      64 |  905 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      64 |  906 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  907 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|     ! 0 |  908 | `				VM_EXIT_ABORT;` |
|       - |  909 | `			}` |
|      30 |  910 | `		}` |
|       - |  911 | `		/* COW separate the backing variable before mutation */` |
|      64 |  912 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|       - |  913 | `	}` |
|  546210 |  914 | `	VmPopOperand(&pTos,1);` |
|       - |  915 | `	/* Phase#2: Perform the insertion. A null key deprecates + normalizes to "" for` |
|       - |  916 | `	 * BOTH the by-value and the by-ref store — emitted HERE, after a null/false` |
|       - |  917 | ``	 * container has auto-vivified to an array, so `$x[null]=v` and `$x[null]=&$y` on`` |
|       - |  918 | `	 * an undefined $x get the notice too (the top-of-handler check runs before` |
|       - |  919 | `	 * vivification, when the base is not yet a hashmap). HashmapInsert /` |
|       - |  920 | `	 * HashmapInsertByRef both then cast NULL->"". A null pKey==0 (an append, no key)` |
|       - |  921 | `	 * is not a null OFFSET and is left alone. */` |
|  546210 |  922 | `	PH7_VmNullOffsetDeprecate(&(*pVm),pKey);` |
|  546205 |  923 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF` |
|  273055 |  924 | `	 && (pInstr->iP1 & PH7_STOREREF_CALLSRC)` |
|      69 |  925 | `	 && (pTos->iFlags & MEMOBJ_AUX_STROFFSET) == 0` |
|      13 |  926 | `	 && pTos->nIdx == SXU32_HIGH ){` |
|       - |  927 | ``		/* `$a[] =& f()` / `$a[$k] =& f()`: the source was written as a CALL and the`` |
|       - |  928 | `		 * callee did not return by reference, so php binds the value it answered and` |
|       - |  929 | ``		 * says so. Same notice the plain `$r =& f()` bind raises. */`` |
|       5 |  930 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|       - |  931 | `			"Only variables should be assigned by reference");` |
|       2 |  932 | `	}` |
|  546210 |  933 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && (pTos->iFlags & MEMOBJ_AUX_STROFFSET) ){` |
|       - |  934 | ``		/* `$a[] = &$s[1]`: the source is a string OFFSET, which php refuses to`` |
|       - |  935 | `		 * reference — and whose slot index is the BASE STRING's, so binding it` |
|       - |  936 | ``		 * aliased the whole string. Same Error the `=&` / by-ref-argument paths`` |
|       - |  937 | `		 * raise. The key is already popped; drop it and abandon the store. */` |
|       - |  938 | `		sxi32 rcSc;` |
|       5 |  939 | `		if( pKey ){` |
|       3 |  940 | `			PH7_MemObjRelease(pKey);` |
|       1 |  941 | `		}` |
|       5 |  942 | `		rcSc = VmThrowFromVm(&(*pVm),"Error","Cannot create references to/from string offsets",` |
|       - |  943 | `			sizeof("Cannot create references to/from string offsets")-1);` |
|       5 |  944 | `		if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       5 |  945 | `		rc = rcSc;` |
|       5 |  946 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  947 | `	}` |
|  546206 |  948 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|     127 |  949 | `		if( pMap == pVm->pGlobal ){` |
|       - |  950 | `			/* php 8.1: $GLOBALS['y'] =& $x binds the global $y to $x's` |
|       - |  951 | `			 * slot; an append has no name to bind (catchable Error). */` |
|      46 |  952 | `			if( pKey == 0 ){` |
|     ! 0 |  953 | `				rc = PH7_VmThrowGlobalsAppendError(&(*pVm));` |
|     ! 0 |  954 | `			}else{` |
|      46 |  955 | `				if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 |  956 | `					PH7_MemObjToString(pKey);` |
|     ! 0 |  957 | `				}` |
|      46 |  958 | `				if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|       - |  959 | `					/* Pathological empty name: keep the legacy diagnostic */` |
|     ! 0 |  960 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|       - |  961 | `						"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 |  962 | `					rc = SXRET_OK;` |
|     ! 0 |  963 | `				}else{` |
|      68 |  964 | `					rc = PH7_VmInstallGlobalVar(&(*pVm),` |
|      44 |  965 | `						(const char *)SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),` |
|      44 |  966 | `						0,pTos->nIdx);` |
|       - |  967 | `				}` |
|       - |  968 | `			}` |
|      24 |  969 | `		}else{` |
|       - |  970 | `			/* Insertion by reference */` |
|      82 |  971 | `			rc = PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|       - |  972 | `		}` |
|      66 |  973 | `	}else{` |
|  546084 |  974 | `		rc = PH7_HashmapInsert(pMap,pKey,pTos);` |
|       - |  975 | `	}` |
|  546206 |  976 | `	if( pKey ){` |
|   94102 |  977 | `		PH7_MemObjRelease(pKey);` |
|   46966 |  978 | `	}` |
|       - |  979 | `	/* An append onto the occupied saturated auto-index threw php's catchable` |
|       - |  980 | `	 * Error (PH7_VmThrowArrayNextIndexError) — dispatch it like any other` |
|       - |  981 | `	 * store-path throw. Plain failures (OOM) keep their existing routes. */` |
|  546206 |  982 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|  546200 |  983 | `	VM_EXIT_BREAK;` |
|     ! 0 |  984 | `	VM_EXIT_BREAK;` |
|  273530 |  985 | `}` |
|       - |  986 |  |
|       - |  987 | `/*` |
|       - |  988 | ` * OP_LOAD_CLOSURE: body moved verbatim from the OP_LOAD_CLOSURE arm of` |
|       - |  989 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  990 | ` */` |
|   19433 |  991 | `PH7_PRIVATE VmOpRc VmExecOpLoadClosure(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  992 | `{` |
|   19438 |  993 | `	ph7_value *pTos = pState->pTos;` |
|   19438 |  994 | `	ph7_value *pStack = pState->pStack;` |
|   19438 |  995 | `	VmInstr *aInstr = pState->aInstr;` |
|   19438 |  996 | `	sxi32 pc = pState->pc;` |
|       - |  997 | `	sxi32 rc;` |
|    9595 |  998 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|   19438 |  999 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|       - | 1000 | `	/* The function whose name the Closure object will wrap: a fresh per-instantiation` |
|       - | 1001 | `	 * copy for a real closure (built below), or the shared lambda function itself for a` |
|       - | 1002 | `	 * plain anonymous function with no captured environment. */` |
|   19438 | 1003 | `	ph7_vm_func *pTarget = pFunc;` |
|       - | 1004 | `	/* A no-capture lambda declared inside a class method is not VM_FUNC_CLOSURE` |
|       - | 1005 | `	 * (its env is empty), yet php still binds the creation-site class as its scope` |
|       - | 1006 | ``	 * so `self::`/private access inside the body works. Detect the enclosing class`` |
|       - | 1007 | `	 * here and route such a lambda through the per-instance closure path (a global` |
|       - | 1008 | `	 * no-capture lambda peeks NULL and stays a shared function). */` |
|   19438 | 1009 | `	ph7_class *pLoadScope = (pFunc->iFlags & VM_FUNC_CLOSURE) ? 0 : PH7_VmPeekDeclaringClass(pVm);` |
|   19438 | 1010 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) \|\| pLoadScope ){` |
|       - | 1011 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|       - | 1012 | `		ph7_vm_func *pClosure;` |
|       - | 1013 | `		char *zName;` |
|       - | 1014 | `		sxu32 mLen;` |
|       - | 1015 | `		sxu32 n;` |
|       - | 1016 | `		/* Create a new VM function */` |
|   19166 | 1017 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|       - | 1018 | `		/* Generate an unique closure name */` |
|   19166 | 1019 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|   19166 | 1020 | `		if( pClosure == 0 \|\| zName == 0){` |
|     ! 0 | 1021 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|     ! 0 | 1022 | `			VM_EXIT_ABORT;` |
|       - | 1023 | `		}` |
|   19166 | 1024 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|   19166 | 1025 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|     ! 0 | 1026 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|     ! 0 | 1027 | `		}` |
|       - | 1028 | `		/* Zero the stucture */` |
|   19166 | 1029 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|       - | 1030 | `		/* Perform a structure assignment on read-only items */` |
|   19166 | 1031 | `		pClosure->aArgs = pFunc->aArgs;` |
|   19166 | 1032 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|   19166 | 1033 | `		pClosure->aStatic = pFunc->aStatic;` |
|   19166 | 1034 | `		pClosure->iFlags = pFunc->iFlags;` |
|       - | 1035 | `		/* An in-class no-capture lambda routed here (pLoadScope set) must be a` |
|       - | 1036 | `		 * real closure so PH7_VmClassMemberAccess honors its pUserData scope. */` |
|   19166 | 1037 | `		pClosure->iFlags \|= VM_FUNC_CLOSURE;` |
|   19166 | 1038 | `		pClosure->pUserData = pFunc->pUserData;` |
|   19166 | 1039 | `		pClosure->sSignature = pFunc->sSignature;` |
|   19166 | 1040 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|   19166 | 1041 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|   19166 | 1042 | `		pClosure->aReturnUnion = pFunc->aReturnUnion;` |
|   19166 | 1043 | `		pClosure->sReturnTypeName = pFunc->sReturnTypeName;` |
|   19166 | 1044 | `		pClosure->bStrictTypes = pFunc->bStrictTypes;` |
|   19166 | 1045 | `		pClosure->nMaxStack = pFunc->nMaxStack;` |
|   19166 | 1046 | `		if( pClosure->pUserData == 0 ){` |
|       - | 1047 | `			/* Stamp the creation-site class scope (see PH7_VmPeekDeclaringClass):` |
|       - | 1048 | `			 * a closure made in a method — or in another closure, whose own stamp` |
|       - | 1049 | `			 * the peek reads — resolves self::/parent:: against it like php. */` |
|   19166 | 1050 | `			pClosure->pUserData = (void *)PH7_VmPeekDeclaringClass(pVm);` |
|    9459 | 1051 | `		}` |
|       - | 1052 | ``		/* Capture the creation-site late-static-binding class so `static::` inside`` |
|       - | 1053 | `		 * the closure body resolves like php (the "called class", which may differ` |
|       - | 1054 | `		 * from the declaring scope stamped above — e.g. a closure made in an` |
|       - | 1055 | `		 * inherited method). A closure made outside any class captures NULL. */` |
|   19166 | 1056 | `		pClosure->pLsbClass = (void *)PH7_VmPeekTopClass(pVm);` |
|       - | 1057 | `		/* Reflection descriptor fields (getDocComment/getAttributes/getStartLine/` |
|       - | 1058 | `		 * getEndLine/getFileName read the INSTANTIATED copy via $__fn) */` |
|   19166 | 1059 | `		pClosure->aAttrs = pFunc->aAttrs;` |
|   19166 | 1060 | `		pClosure->sDoc = pFunc->sDoc;` |
|   19166 | 1061 | `		pClosure->sFile = pFunc->sFile;` |
|   19166 | 1062 | `		pClosure->nLine = pFunc->nLine;` |
|   19166 | 1063 | `		pClosure->nEndLine = pFunc->nEndLine;` |
|       - | 1064 | ``		/* php's visible `{closure:...}` name is a property of the DECLARATION, so every`` |
|       - | 1065 | `		 * per-instantiation copy answers the same one (php has a single op_array here). */` |
|   19166 | 1066 | `		pClosure->sClosureName = pFunc->sClosureName;` |
|   19166 | 1067 | `		pClosure->sClosureScope = pFunc->sClosureScope;` |
|   19166 | 1068 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|       - | 1069 | `		/* Register the closure */` |
|   19166 | 1070 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|       - | 1071 | `		/* Set up closure environment */` |
|   19166 | 1072 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|   19166 | 1073 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|   47938 | 1074 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|       - | 1075 | `			ph7_value *pValue;` |
|   28777 | 1076 | `			pEnv = &aEnv[n];` |
|   28777 | 1077 | `			sEnv.sName  = pEnv->sName;` |
|   28777 | 1078 | `			sEnv.iFlags = pEnv->iFlags;` |
|   28777 | 1079 | `			sEnv.nLine = pEnv->nLine;` |
|   28777 | 1080 | `			sEnv.nIdx = SXU32_HIGH;` |
|   28777 | 1081 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|   28772 | 1082 | `			if( (sEnv.iFlags & (VM_FUNC_ARG_BY_REF\|VM_FUNC_ARG_IGNORE)) == VM_FUNC_ARG_BY_REF` |
|   15641 | 1083 | `			 && !(SyStringLength(&sEnv.sName) == sizeof("this")-1` |
|    1389 | 1084 | `				&& SyMemcmp(SyStringData(&sEnv.sName),"this",sizeof("this")-1) == 0) ){` |
|       - | 1085 | `				/* Capture by reference: bind the env entry to the variable's` |
|       - | 1086 | `				 * memory slot — creating a fresh null variable when missing,` |
|       - | 1087 | ``				 * as php does (`use (&$f)` before $f is assigned) — and pin`` |
|       - | 1088 | `				 * the slot past the creating frame's teardown so the closure` |
|       - | 1089 | `				 * can outlive its birth scope. The call-time env install` |
|       - | 1090 | `				 * aliases the name to this slot instead of copying a value. */` |
|    1851 | 1091 | `				pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,TRUE);` |
|    1851 | 1092 | `				if( pValue ){` |
|    1851 | 1093 | `					sEnv.nIdx = pValue->nIdx;` |
|    1851 | 1094 | `					VmPinMemObjSlot(pVm,pValue->nIdx);` |
|     919 | 1095 | `				}` |
|     924 | 1096 | `			}else{` |
|       - | 1097 | `				/* Standard pass by value */` |
|   26931 | 1098 | `				pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|   26931 | 1099 | `				if( pValue ){` |
|       - | 1100 | `					/* Copy imported value */` |
|    8256 | 1101 | `					PH7_MemObjStore(pValue,&sEnv.sValue);` |
|   22787 | 1102 | `				}else if( (sEnv.iFlags & (VM_FUNC_ARG_BY_REF\|VM_FUNC_ARG_IGNORE)) == 0` |
|    9276 | 1103 | `					&& !(SyStringLength(&sEnv.sName) == sizeof("this")-1` |
|      52 | 1104 | `						&& SyMemcmp(SyStringData(&sEnv.sName),"this",sizeof("this")-1) == 0) ){` |
|     111 | 1105 | `					if( pFunc->iFlags & VM_FUNC_ARROW ){` |
|       - | 1106 | `						/* An arrow function auto-captures free variables by value, but` |
|       - | 1107 | `						 * php does NOT capture one that is UNDEFINED at creation: the` |
|       - | 1108 | `						 * isolated body scope then simply has no such variable, so a` |
|       - | 1109 | `						 * read of it there raises the normal "Undefined variable"` |
|       - | 1110 | `						 * warning (and reflection's getClosureUsedVariables omits it).` |
|       - | 1111 | `						 * Skip installing the capture so the body READ — not the` |
|       - | 1112 | `						 * creation — warns, matching php. (A later assignment to the` |
|       - | 1113 | `						 * outer variable does not retro-capture: arrow scope is` |
|       - | 1114 | `						 * isolated.) An explicit by-value use() instead warns here and` |
|       - | 1115 | `						 * binds NULL, handled just below. */` |
|     101 | 1116 | `						continue;` |
|       - | 1117 | `					}` |
|       - | 1118 | ``					/* php reads a by-value `use ($q)` capture AT CLOSURE CREATION and`` |
|       - | 1119 | `					 * warns when the variable is undefined there (the by-ref form` |
|       - | 1120 | ``					 * `use (&$q)` above stays silent — it creates the binding). The`` |
|       - | 1121 | `					 * auto-injected $this (VM_FUNC_ARG_IGNORE) never warns. The` |
|       - | 1122 | `					 * capture still proceeds as NULL, as php does. php attributes the` |
|       - | 1123 | `					 * warning to the capture's own line (which can differ from the` |
|       - | 1124 | `					 * OP_LOAD_CLOSURE instruction line when the use-clause wraps), so` |
|       - | 1125 | `					 * borrow the recorded line for the emission and restore it. */` |
|      11 | 1126 | `					sxu32 nSavedLine = pVm->nCurLine;` |
|      11 | 1127 | `					if( sEnv.nLine ){` |
|      11 | 1128 | `						pVm->nCurLine = sEnv.nLine;` |
|       5 | 1129 | `					}` |
|      11 | 1130 | `					VmErrorFormat(pVm,PH7_CTX_WARNING,"Undefined variable $%z",&sEnv.sName);` |
|      11 | 1131 | `					pVm->nCurLine = nSavedLine;` |
|       5 | 1132 | `				}` |
|       - | 1133 | `			}` |
|       - | 1134 | `			/* Insert the imported variable */` |
|   28681 | 1135 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|   14199 | 1136 | `		}` |
|   19166 | 1137 | `		pTarget = pClosure;` |
|    9459 | 1138 | `	}` |
|       - | 1139 | `	/* Wrap the target function in a Closure object and push it. Its captured environment` |
|       - | 1140 | ``	 * (incl. any `$this`) stays in pTarget->aClosureEnv and is delivered by the normal call`` |
|       - | 1141 | `	 * path when the closure is dispatched by name. */` |
|   19438 | 1142 | `	pTos++;` |
|       - | 1143 | `	{` |
|   19438 | 1144 | `		ph7_class_instance *pCloObj = VmCreateClosure(pVm, &pTarget->sName, 0, 0);` |
|   19438 | 1145 | `		if( pCloObj ){` |
|       - | 1146 | `			/* The instance is born holding ONE reference and this stack slot is what` |
|       - | 1147 | `			 * holds it -- the same handover OP_NEW makes. Taking a second one here` |
|       - | 1148 | `			 * meant no Closure object ever reached zero: every closure expression a` |
|       - | 1149 | `			 * program evaluated leaked its object, its three slots and (through the` |
|       - | 1150 | `			 * object) the per-instantiation function body behind it. */` |
|   19438 | 1151 | `			pTos->x.pOther = pCloObj;` |
|   19438 | 1152 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|    9600 | 1153 | `		}else{` |
|       - | 1154 | `			/* OOM fallback: the name string is still a usable callable. */` |
|     ! 0 | 1155 | `			PH7_MemObjStringAppend(pTos, pTarget->sName.zString, pTarget->sName.nByte);` |
|       - | 1156 | `		}` |
|       - | 1157 | `	}` |
|   19438 | 1158 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1159 | `	VM_EXIT_BREAK;` |
|    9600 | 1160 | `}` |
|       - | 1161 |  |
|       - | 1162 |  |
|       - | 1163 | `/*` |
|       - | 1164 | `` * TRUE when this LOAD_IDX feeds a `??`: the coalesce test is the very next`` |
|       - | 1165 | `` * instruction. php evaluates the whole left operand of `??` in isset-context,`` |
|       - | 1166 | ` * so the access must stay SILENT and, when it misses, must yield NULL — an` |
|       - | 1167 | ``  * out-of-range string offset that yielded "" instead made `$s[99] ?? $d` `` |
|       - | 1168 | ` * evaluate to "" rather than $d, a wrong answer rather than a stray notice.` |
|       - | 1169 | ` */` |
| 1071427 | 1170 | `static int VmIdxFeedsCoalesce(const VmInstr *pInstr)` |
|       5 | 1171 | `{` |
| 1071432 | 1172 | `	return (pInstr+1)->iOp == PH7_OP_NULLC \|\| (pInstr+1)->iOp == PH7_OP_NULLC_JMP;` |
|       5 | 1173 | `}` |
|       - | 1174 | `/*` |
|       - | 1175 | `` * php's string-offset STORE, shared by `$s[i] = v` (OP_STORE_IDX) and`` |
|       - | 1176 | `` * `$s[i] ??= v` (OP_NULLC_STORE — `??=` is not an assign-op, so php performs a`` |
|       - | 1177 | ` * real offset write there): resolve iRawOfft against the string's CURRENT length` |
|       - | 1178 | ` * (a negative offset counts back from the end and, when it still lands before the` |
|       - | 1179 | `` * start, warns `Illegal string offset` and writes NOTHING), refuse an EMPTY`` |
|       - | 1180 | ` * replacement, warn when more than one byte was handed over, PAD WITH SPACES up` |
|       - | 1181 | ` * to the offset, then write the first byte.` |
|       - | 1182 | ` *` |
|       - | 1183 | ` * pVal must ALREADY be a string: that coercion is user-visible (it warns for an` |
|       - | 1184 | ` * array, throws for a not-stringable object) and stays with the callers, which` |
|       - | 1185 | ` * are the only places that can route a throw. Answers SXRET_OK when the store` |
|       - | 1186 | `` * happened or was skipped, and SXERR_INVALID for php's `Cannot assign an empty`` |
|       - | 1187 | `` * string to a string offset` Error — raised by the caller for the same reason.`` |
|       - | 1188 | ` */` |
|     450 | 1189 | `PH7_PRIVATE sxi32 VmStringOffsetWrite(ph7_vm *pVm,ph7_value *pStr,sxi64 iRawOfft,ph7_value *pVal)` |
|       5 | 1190 | `{` |
|     455 | 1191 | `	sxi64 nLen = (sxi64)SyBlobLength(&pStr->sBlob);` |
|     455 | 1192 | `	sxi64 iOfft = iRawOfft;` |
|       - | 1193 | `	const char *zVal;` |
|     455 | 1194 | `	if( iOfft < 0 ){` |
|       - | 1195 | `		/* php 7.1: a negative offset writes back from the end. */` |
|       9 | 1196 | `		iOfft += nLen;` |
|       9 | 1197 | `		if( iOfft < 0 ){` |
|       7 | 1198 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Illegal string offset %qd",iRawOfft);` |
|       7 | 1199 | `			return SXRET_OK;` |
|       - | 1200 | `		}` |
|       1 | 1201 | `	}` |
|     449 | 1202 | `	if( SyBlobLength(&pVal->sBlob) < 1 ){` |
|       - | 1203 | ``		/* php refuses to write NOTHING into an offset — `$s[2] = ""`, and the`` |
|       - | 1204 | ``		 * `= null` / `= false` that stringify to "" — where PHL silently ignored`` |
|       - | 1205 | `		 * the store. */` |
|      13 | 1206 | `		return SXERR_INVALID;` |
|       - | 1207 | `	}` |
|     437 | 1208 | `	zVal = (const char *)SyBlobData(&pVal->sBlob);` |
|     437 | 1209 | `	if( SyBlobLength(&pVal->sBlob) > 1 ){` |
|      10 | 1210 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 1211 | `			"Only the first byte will be assigned to the string offset");` |
|       4 | 1212 | `	}` |
|     437 | 1213 | `	if( iOfft >= nLen ){` |
|       - | 1214 | `		/* php PADS WITH SPACES up to the offset. PH7 simply appended the byte, so` |
|       - | 1215 | `		 * "abc" with [6]="Z" became "abcZ" rather than "abc   Z" -- a silently` |
|       - | 1216 | `		 * wrong string. */` |
|       - | 1217 | `		sxi64 nPad;` |
|     217 | 1218 | `		for( nPad = nLen ; nPad < iOfft ; ++nPad ){` |
|     175 | 1219 | `			SyBlobAppend(&pStr->sBlob," ",sizeof(char));` |
|      88 | 1220 | `		}` |
|      43 | 1221 | `		SyBlobAppend(&pStr->sBlob,(const void *)zVal,sizeof(char));` |
|      22 | 1222 | `	}else{` |
|     395 | 1223 | `		char *zData = (char *)SyBlobData(&pStr->sBlob);` |
|     395 | 1224 | `		zData[iOfft] = zVal[0];` |
|       - | 1225 | `	}` |
|     437 | 1226 | `	return SXRET_OK;` |
|     230 | 1227 | `}` |
|       - | 1228 | `/*` |
|       - | 1229 | `` * Does this LOAD_IDX feed a `??=` (rather than a plain `??`)? The compiler emits`` |
|       - | 1230 | ` * the LHS peek, then OP_NULLC_JMP, then the RHS, then OP_NULLC_STORE — so the` |
|       - | 1231 | ` * NULLC_JMP right after is what distinguishes the assigning form, whose store` |
|       - | 1232 | ` * still has to happen when the peek answers null.` |
|       - | 1233 | ` */` |
| 1071375 | 1234 | `static int VmIdxFeedsCoalesceAssign(const VmInstr *pInstr)` |
|       5 | 1235 | `{` |
| 1071380 | 1236 | `	return (pInstr+1)->iOp == PH7_OP_NULLC_JMP;` |
|       5 | 1237 | `}` |
|       - | 1238 | `/*` |
|       - | 1239 | `` * Arm the write-back half of `$o[$k] op= v` on an ArrayAccess container. The`` |
|       - | 1240 | ` * value offsetGet just answered goes into a fresh SCRATCH memobj that pTos` |
|       - | 1241 | ` * points at, so the compound-assign op computes IN that slot the way it would` |
|       - | 1242 | ` * in an ordinary variable; the pending entry then makes the op's tail dispatch` |
|       - | 1243 | `` * `offsetSet($k, computed)`. The KEY gets a reserved slot of its own (pIdx is`` |
|       - | 1244 | ` * released as soon as this opcode returns, and the write happens one opcode` |
|       - | 1245 | `` * later); an ABSENT key — `$o[] op= v` — is carried as NULL, which is the value`` |
|       - | 1246 | ` * php hands both accessors for that shape.` |
|       - | 1247 | ` *` |
|       - | 1248 | ` * A failed reservation simply leaves the value unarmed: pTos keeps its` |
|       - | 1249 | ` * no-slot temp and the op falls back to the pre-existing refusal.` |
|       - | 1250 | ` */` |
|      54 | 1251 | `static void VmDimRmwArm(` |
|       - | 1252 | `	ph7_vm *pVm,` |
|       - | 1253 | `	ph7_class_instance *pInst,` |
|       - | 1254 | `	ph7_value *pIdx,` |
|       - | 1255 | `	ph7_value *pTos,` |
|       - | 1256 | `	void *pOwnerStack,` |
|       - | 1257 | `	void *pInstrs,` |
|       - | 1258 | `	sxu32 nPc` |
|       - | 1259 | `	)` |
|       2 | 1260 | `{` |
|       - | 1261 | `	ph7_value *pSlot;` |
|       - | 1262 | `	sxu32 nScratch;` |
|       - | 1263 | `	sxu32 nKey;` |
|       - | 1264 | `	VmHookRmw sRmw;` |
|      56 | 1265 | `	pSlot = PH7_ReserveMemObj(&(*pVm));` |
|      56 | 1266 | `	if( pSlot == 0 ){` |
|     ! 0 | 1267 | `		return;` |
|       - | 1268 | `	}` |
|      56 | 1269 | `	nScratch = pSlot->nIdx;` |
|      56 | 1270 | `	pSlot = PH7_ReserveMemObj(&(*pVm));` |
|      56 | 1271 | `	if( pSlot == 0 ){` |
|     ! 0 | 1272 | `		VmHookRmwFreeScratch(&(*pVm),nScratch);` |
|     ! 0 | 1273 | `		return;` |
|       - | 1274 | `	}` |
|      56 | 1275 | `	nKey = pSlot->nIdx;` |
|       - | 1276 | `	/* Address both slots by index from here on. This guarded a reservation` |
|       - | 1277 | `	 * GROWING the aMemObj set under the pointer the first one handed back;` |
|       - | 1278 | `	 * redundant since P1 -- the pool's segments are fixed, so a slot's address` |
|       - | 1279 | `	 * never moves. Left for the harvest sweep. */` |
|      56 | 1280 | `	if( pIdx ){` |
|      56 | 1281 | `		PH7_MemObjStore(pIdx,pSlot);` |
|      27 | 1282 | `	}` |
|      56 | 1283 | `	pSlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nScratch);` |
|      56 | 1284 | `	if( pSlot == 0 ){` |
|     ! 0 | 1285 | `		VmHookRmwFreeScratch(&(*pVm),nKey);` |
|     ! 0 | 1286 | `		VmHookRmwFreeScratch(&(*pVm),nScratch);` |
|     ! 0 | 1287 | `		return;` |
|       - | 1288 | `	}` |
|      56 | 1289 | `	PH7_MemObjStore(pTos,pSlot);` |
|      56 | 1290 | `	sRmw.iKind = VM_HOOK_PEND_RMW_DIM;` |
|      56 | 1291 | `	sRmw.pThis = pInst;` |
|      56 | 1292 | `	sRmw.pAttr = 0;` |
|      56 | 1293 | `	sRmw.nBackIdx = nKey;` |
|      56 | 1294 | `	sRmw.nScratchIdx = nScratch;` |
|      56 | 1295 | `	SyBlobInit(&sRmw.sName,&pVm->sAllocator);` |
|      56 | 1296 | `	sRmw.pOwnerStack = pOwnerStack;` |
|      56 | 1297 | `	sRmw.pInstrs = pInstrs;` |
|      56 | 1298 | `	sRmw.nJmpPc = nPc;  /* the modify op ... */` |
|      56 | 1299 | `	sRmw.nPc = nPc;     /* ... is the whole window */` |
|      56 | 1300 | `	pInst->iRef++;` |
|      56 | 1301 | `	pTos->nIdx = nScratch;` |
|      56 | 1302 | `	SySetPut(&pVm->aHookRmw,(const void *)&sRmw);` |
|      29 | 1303 | `}` |
|       - | 1304 | `/*` |
|       - | 1305 | ` * Is this LOAD_IDX fetching the dimension for WRITING — php's BP_VAR_W / BP_VAR_RW` |
|       - | 1306 | ` * / BP_VAR_UNSET fetch, the one that asks the container for a slot to MODIFY` |
|       - | 1307 | ` * rather than a value to read?` |
|       - | 1308 | ` *` |
|       - | 1309 | ` * iP2 answers for most of it. The two shapes it cannot are the ones where the` |
|       - | 1310 | ` * fetch is compiled as a plain read and the NEXT instruction is what makes it a` |
|       - | 1311 | `` * write: binding a reference to the element (`$r = &$o[$k]`) and iterating it by`` |
|       - | 1312 | `` * reference (`foreach ($o[$k] as &$v)`). A compound assign is the reverse case —`` |
|       - | 1313 | ` * iP2 says write, but php compiles it to ASSIGN_DIM_OP, a read plus a write` |
|       - | 1314 | ` * through the container's own handlers (VmDimRmwArm), not a write FETCH.` |
|       - | 1315 | ` */` |
|    1056 | 1316 | `static int VmIdxFetchForWrite(const VmInstr *pInstr,sxi32 iP2)` |
|       5 | 1317 | `{` |
|    1061 | 1318 | `	const VmInstr *pNext = pInstr + 1;` |
|    1061 | 1319 | `	if( iP2 == 1 ){` |
|     189 | 1320 | `		return !VmNextIsCompoundAssign(pNext);` |
|       - | 1321 | `	}` |
|     873 | 1322 | `	if( iP2 == VM_IDX_CTX_UNSET_BASE ){` |
|       - | 1323 | `		/* An INTERMEDIATE subscript of an unset chain: php fetches it for` |
|       - | 1324 | `		 * writing so the removal one level down can land. */` |
|       9 | 1325 | `		return 1;` |
|       - | 1326 | `	}` |
|     865 | 1327 | `	if( iP2 == 0 ){` |
|     585 | 1328 | `		if( pNext->iOp == PH7_OP_STORE_REF ){` |
|     ! 0 | 1329 | `			return 1;` |
|       - | 1330 | `		}` |
|     585 | 1331 | `		if( pNext->iOp == PH7_OP_FOREACH_INIT && pNext->p3 ){` |
|       5 | 1332 | `			return (((ph7_foreach_info *)pNext->p3)->iFlags & PH7_4EACH_STEP_REF) != 0;` |
|       - | 1333 | `		}` |
|     288 | 1334 | `	}` |
|     861 | 1335 | `	return 0;` |
|     533 | 1336 | `}` |
|       - | 1337 | `/*` |
|       - | 1338 | ` * Which fetch contexts may answer out of a WRITABLE container's own storage` |
|       - | 1339 | `` * (PH7_SplDimElemSlot, vm_builtin_spl.c) instead of through `offsetGet`?`` |
|       - | 1340 | ` *` |
|       - | 1341 | ` * The ones that ask for a VALUE and may go on to MODIFY it: a plain read — whose` |
|       - | 1342 | ` * result carries the element's slot exactly as an array element's does, which is` |
|       - | 1343 | ` * what lets a by-reference ARGUMENT bind it — a write-context fetch, and the` |
|       - | 1344 | `` * INTERMEDIATE step of an unset chain. isset()/empty()/`??`/`??=` must reach`` |
|       - | 1345 | ` * offsetExists, and the OUTERMOST unset must reach offsetUnset, so those keep the` |
|       - | 1346 | ` * accessor. (iP2 is the NORMALIZED context here: the deferred-argument record mode` |
|       - | 1347 | ` * has already become a plain read.)` |
|       - | 1348 | ` *` |
|       - | 1349 | ` * A COMPOUND assign is deliberately not one of them, which is why this asks` |
|       - | 1350 | `` * VmIdxFetchForWrite rather than testing iP2 == 1 itself: `$ao[k] op= v` is php's`` |
|       - | 1351 | ` * ASSIGN_DIM_OP on an OBJECT, and that one reads and writes through the accessors` |
|       - | 1352 | ` * whatever the read handler could have offered — a subclass overriding only` |
|       - | 1353 | `` * offsetSet sees its own method called for `+=` and not for `++`.`` |
|       - | 1354 | ` */` |
|     674 | 1355 | `static int VmDimFastFetchCtx(const VmInstr *pInstr,sxi32 iP2)` |
|       5 | 1356 | `{` |
|     679 | 1357 | `	return iP2 == 0 \|\| VmIdxFetchForWrite(pInstr,iP2);` |
|       5 | 1358 | `}` |
|       - | 1359 | `/*` |
|       - | 1360 | ` * Is this fetch asking the base to BE a container — the question php answers with` |
|       - | 1361 | `` * `Cannot use string offset as an array` when the base is a string offset?`` |
|       - | 1362 | ` *` |
|       - | 1363 | ` * Every context that reaches INTO the base to write, vivify or remove: the write` |
|       - | 1364 | ` * contexts (1, the read-modify-write among them) and both halves of an unset chain.` |
|       - | 1365 | `` * The LOOKUPS — a plain read, isset()/empty()/`??`, a destructure, a deferred`` |
|       - | 1366 | `` * argument — are php's own silence (`$x = $s[0][1]` reads a character out of a`` |
|       - | 1367 | `` * character) and are not this. Neither is the `??=` PEEK (3): it reads, and`` |
|       - | 1368 | `` * `$s[0][0] ??= 7` finds a character and never stores at all, so only the peek that`` |
|       - | 1369 | ` * comes back EMPTY is a write — that one is refused where the read lands.` |
|       - | 1370 | ` *` |
|       - | 1371 | ` * iP2 is the NORMALIZED context, so VM_IDX_CTX_RMW has already become 1.` |
|       - | 1372 | ` */` |
|      24 | 1373 | `static int VmIdxCtxIsContainerWrite(sxi32 iP2)` |
|       1 | 1374 | `{` |
|      25 | 1375 | `	return iP2 == 1 \|\| VM_IDX_IS_UNSET(iP2);` |
|       1 | 1376 | `}` |
|       - | 1377 | `/*` |
|       - | 1378 | `` * php's `Indirect modification of overloaded element of C has no effect`: the`` |
|       - | 1379 | ` * write-context fetch above landed on a container that answers with a COPY, so` |
|       - | 1380 | ` * whatever the rest of the expression writes is thrown away. php says so and` |
|       - | 1381 | ` * carries on.` |
|       - | 1382 | ` *` |
|       - | 1383 | ` * PHL had neither half. The notice was missing, and the copy was not a copy: a` |
|       - | 1384 | ` * userland offsetGet returns the container's own nested hashmap by COW, and` |
|       - | 1385 | ` * OP_STORE_IDX on a base with no slot index writes STRAIGHT INTO the shared map —` |
|       - | 1386 | `` * so `$o['a']['b'] = 9`, `$o['a'][] = 5` and `foreach ($o['a'] as &$v)` all`` |
|       - | 1387 | ` * modified the object php leaves untouched, silently. Separating the value here` |
|       - | 1388 | ` * is what makes the write land nowhere.` |
|       - | 1389 | ` *` |
|       - | 1390 | ` * php stays silent for an OBJECT, and so does this: an object is a handle, the` |
|       - | 1391 | ` * write through it is not lost, and nothing about it is indirect.` |
|       - | 1392 | ` */` |
|      50 | 1393 | `PH7_PRIVATE void PH7_VmOverloadedElemNotice(ph7_vm *pVm,ph7_class *pClass,ph7_value *pVal)` |
|       1 | 1394 | `{` |
|      51 | 1395 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       9 | 1396 | `		return;` |
|       - | 1397 | `	}` |
|      64 | 1398 | `	VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|       - | 1399 | `		"Indirect modification of overloaded element of %z has no effect",` |
|      21 | 1400 | `		&pClass->sDisp);` |
|      43 | 1401 | `	if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|      35 | 1402 | `		PH7_HashmapCowSeparate(&(*pVm),pVal);` |
|      17 | 1403 | `	}` |
|      26 | 1404 | `}` |
|       - | 1405 | `/*` |
|       - | 1406 | ` * OP_LOAD_IDX: body moved verbatim from the OP_LOAD_IDX arm of` |
|       - | 1407 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 1408 | ` */` |
| 1404068 | 1409 | `PH7_PRIVATE VmOpRc VmExecOpLoadIdx(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 1410 | `{` |
| 1404073 | 1411 | `	ph7_value *pTos = pState->pTos;` |
| 1404073 | 1412 | `	ph7_value *pStack = pState->pStack;` |
| 1404073 | 1413 | `	VmInstr *aInstr = pState->aInstr;` |
| 1404073 | 1414 | `	sxi32 pc = pState->pc;` |
|       - | 1415 | `	sxi32 rc;` |
|  705595 | 1416 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 1404073 | 1417 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
| 1404073 | 1418 | `	ph7_hashmap *pMap = 0;` |
|       - | 1419 | `	ph7_value *pIdx;` |
|       - | 1420 | `	/* D1 commit 2: iP2==9 is the deferred-record mode. It behaves exactly like a plain read` |
|       - | 1421 | `	 * (iP2==0) for base dispatch / the read tail, EXCEPT the dedicated block right after the` |
|       - | 1422 | `	 * index is popped, which — on a lookup MISS with a reachable base — captures the lvalue` |
|       - | 1423 | `	 * path (MEMOBJ_AUX_DEFPATH) instead of warning, and exits. Everything else sees iP2. */` |
|       - | 1424 | ``	/* A read-modify-write fetch (VM_IDX_CTX_RMW: `$a[k] += v`, `$a[k]++`) IS the`` |
|       - | 1425 | `	 * write context for everything below — it COW-separates, it vivifies a missing` |
|       - | 1426 | `	 * key, it refuses the same offset TYPES — so normalize it to 1 here and keep` |
|       - | 1427 | `	 * the single thing that separates the two: php READ the element first, so a key` |
|       - | 1428 | `	 * that was not there to read WARNS before it is created. */` |
| 1404073 | 1429 | `	int bRmwFetch = (pInstr->iP2 == VM_IDX_CTX_RMW);` |
| 1404073 | 1430 | `	int bRmwMiss = 0;` |
| 1404073 | 1431 | `	int bBaseStrOff = 0;` |
| 1404073 | 1432 | `	sxi32 iP2 = (pInstr->iP2 == 9) ? 0 : (bRmwFetch ? 1 : pInstr->iP2);` |
| 1404073 | 1433 | `	pIdx = 0;` |
| 1404073 | 1434 | `	if( pInstr->iP1 == 0 ){` |
|      52 | 1435 | `		if( !iP2){` |
|       - | 1436 | ``			/* `[]` with nothing to append INTO. Every placement php refuses is a compile`` |
|       - | 1437 | `			 * error now (compile.c), so the only shape that reaches here is the one php` |
|       - | 1438 | `			 * also settles at runtime: a call ARGUMENT, whose parameter may turn out to be` |
|       - | 1439 | `			 * by-reference (php appends and binds) or by-value (php's Error). Record the` |
|       - | 1440 | `			 * append as a step of the deferred lvalue path and let OP_CALL decide. */` |
|      13 | 1441 | `			if( pInstr->iP2 == 9 ){` |
|      13 | 1442 | `				VmDeferredPath *pPath = 0;` |
|      13 | 1443 | `				if( pTos->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|     ! 0 | 1444 | `					pPath = (VmDeferredPath *)pTos->x.pOther;` |
|     ! 0 | 1445 | `					if( VmDeferPathPushAppend(pPath) == SXRET_OK ){` |
|     ! 0 | 1446 | `						VM_EXIT_BREAK; /* carrier already on pTos */` |
|     ! 0 | 1447 | `					}` |
|      13 | 1448 | `				}else if( pTos->iFlags & MEMOBJ_AUX_DEFERRED ){` |
|       - | 1449 | `					SyString sRootName;` |
|       3 | 1450 | `					SyStringInitFromBuf(&sRootName,(const char *)pTos->x.pOther,` |
|       - | 1451 | `						pTos->x.pOther ? SyStrlen((const char *)pTos->x.pOther) : 0);` |
|       3 | 1452 | `					pPath = VmDeferPathNew(&(*pVm),1,SXU32_HIGH,&sRootName);` |
|       3 | 1453 | `					if( pPath && VmDeferPathPushAppend(pPath) == SXRET_OK ){` |
|       3 | 1454 | `						pTos->iFlags &= ~MEMOBJ_AUX_DEFERRED; /* borrowed name; don't free x.pOther */` |
|       3 | 1455 | `						pTos->x.pOther = pPath;` |
|       3 | 1456 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|       3 | 1457 | `						pTos->nIdx = SXU32_HIGH;` |
|       3 | 1458 | `						VM_EXIT_BREAK;` |
|       - | 1459 | `					}` |
|     ! 0 | 1460 | `					VmFreeDeferredPath(pPath);` |
|      11 | 1461 | `				}else if( pTos->nIdx != SXU32_HIGH ){` |
|      11 | 1462 | `					pPath = VmDeferPathNew(&(*pVm),0,pTos->nIdx,0);` |
|      11 | 1463 | `					if( pPath && VmDeferPathPushAppend(pPath) == SXRET_OK ){` |
|      11 | 1464 | `						PH7_MemObjRelease(pTos);` |
|      11 | 1465 | `						pTos->x.pOther = pPath;` |
|      11 | 1466 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|      11 | 1467 | `						pTos->nIdx = SXU32_HIGH;` |
|      11 | 1468 | `						VM_EXIT_BREAK;` |
|       - | 1469 | `					}` |
|     ! 0 | 1470 | `					VmFreeDeferredPath(pPath);` |
|     ! 0 | 1471 | `				}` |
|     ! 0 | 1472 | `			}` |
|       - | 1473 | `			/* Not a deferrable argument (or out of memory recording it): php's own` |
|       - | 1474 | `			 * Error, which replaced PH7's notice-and-NULL. */` |
|     ! 0 | 1475 | `			if( pTos >= pStack ){` |
|     ! 0 | 1476 | `				PH7_MemObjRelease(pTos);` |
|     ! 0 | 1477 | `			}else{` |
|       - | 1478 | `				/* TICKET 1433-020: Empty stack */` |
|     ! 0 | 1479 | `				pTos++;` |
|     ! 0 | 1480 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|     ! 0 | 1481 | `				pTos->nIdx = SXU32_HIGH;` |
|       - | 1482 | `			}` |
|       - | 1483 | `			{` |
|     ! 0 | 1484 | `			sxi32 rcRd = VmThrowFromVm(&(*pVm),"Error","Cannot use [] for reading",` |
|       - | 1485 | `				sizeof("Cannot use [] for reading")-1);` |
|     ! 0 | 1486 | `			if( rcRd == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     ! 0 | 1487 | `			rc = rcRd;` |
|     ! 0 | 1488 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1489 | `			}` |
|       - | 1490 | `		}` |
|      21 | 1491 | `	}else{` |
| 1404023 | 1492 | `		pIdx = pTos;` |
| 1404023 | 1493 | `		pTos--;` |
|       - | 1494 | `	}` |
| 1404061 | 1495 | `	if( VM_IDX_IS_UNSET(iP2) ){` |
|       - | 1496 | ``		/* `unset($o->p[$k])` reaches INTO what the property holds, which is php's`` |
|       - | 1497 | `		 * indirect modification of the property itself -- refused for a readonly` |
|       - | 1498 | `		 * one, and for a native property whose handler takes no write. The write` |
|       - | 1499 | ``		 * shapes (`$o->p[$k] = v`, `$o->p[] = v`) are screened at OP_MEMBER, which`` |
|       - | 1500 | `		 * knows them from its own context tag; an unset BASE is tagged as an` |
|       - | 1501 | `		 * ordinary read there and is only recognizable here. */` |
|    1735 | 1502 | `		sxi32 rcInd = PH7_VmCheckIndirectModify(&(*pVm),pTos->nIdx);` |
|    1735 | 1503 | `		if( rcInd != SXRET_OK ){` |
|       5 | 1504 | `			if( pIdx ){` |
|       5 | 1505 | `				PH7_MemObjRelease(pIdx);` |
|       2 | 1506 | `			}` |
|       5 | 1507 | `			PH7_MemObjRelease(pTos);` |
|       5 | 1508 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       5 | 1509 | `			pTos->nIdx = SXU32_HIGH;` |
|       5 | 1510 | `			if( rcInd == PH7_ABORT ){ VM_EXIT_ABORT; }` |
|       5 | 1511 | `			PH7_THROW_ROUTE_MIDEXPR(rcInd)` |
|       - | 1512 | `		}` |
|     863 | 1513 | `	}` |
| 1404057 | 1514 | `	if( pInstr->iP2 == 9 && pIdx ){` |
|       - | 1515 | `		/* D1 commit 2 record mode. Decide whether to CAPTURE this subscript as a deferred` |
|       - | 1516 | `		 * lvalue step (the by-ref/by-value decision is not known until OP_CALL), or fall` |
|       - | 1517 | `		 * through to a plain read (iP2 was normalized to 0 above). We defer only when the` |
|       - | 1518 | `		 * base can be reached again at resolve time: an existing descriptor (nested), the` |
|       - | 1519 | `		 * commit-1 undefined-variable marker, or a real container/string/scalar slot` |
|       - | 1520 | `		 * (nIdx != SXU32_HIGH). On a present array key we DO NOT defer — a hit already` |
|       - | 1521 | `		 * yields the aliasable read slot the by-ref binder needs. */` |
|  132399 | 1522 | `		VmDeferredPath *pPath = 0;` |
|  132399 | 1523 | `		int bDefer = 0, eRoot = 0;` |
|  132399 | 1524 | `		if( pTos->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|       - | 1525 | `			/* Nested: the base already carries a descriptor — extend it in place. */` |
|      20 | 1526 | `			pPath = (VmDeferredPath *)pTos->x.pOther;` |
|      20 | 1527 | `			bDefer = 1;` |
|  132390 | 1528 | `		}else if( pTos->iFlags & MEMOBJ_AUX_DEFERRED ){` |
|       - | 1529 | `			/* Undefined base variable (commit-1 marker): root the descriptor by name. */` |
|       - | 1530 | `			SyString sRootName;` |
|       3 | 1531 | `			SyStringInitFromBuf(&sRootName,(const char *)pTos->x.pOther,` |
|       - | 1532 | `				pTos->x.pOther ? SyStrlen((const char *)pTos->x.pOther) : 0);` |
|       3 | 1533 | `			pPath = VmDeferPathNew(&(*pVm),1,SXU32_HIGH,&sRootName);` |
|       3 | 1534 | `			pTos->iFlags &= ~MEMOBJ_AUX_DEFERRED; /* borrowed name; don't free x.pOther */` |
|       3 | 1535 | `			pTos->x.pOther = 0;` |
|       3 | 1536 | `			bDefer = (pPath != 0);` |
|  132380 | 1537 | `		}else if( pTos->nIdx != SXU32_HIGH ){` |
|       - | 1538 | `			/* A real base slot: array/scalar -> eRoot 0 (by-ref may vivify), string -> 2. */` |
|  132133 | 1539 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       - | 1540 | `				/* Probe hit/miss on a COPY of the key: PH7_HashmapLookup casts a NULL key to` |
|       - | 1541 | `				 * "" in place, which would suppress the fall-through read's null-offset` |
|       - | 1542 | `				 * deprecation (hit) and capture the wrong key in the step (miss). */` |
|       - | 1543 | `				ph7_value idxProbe;` |
|   69193 | 1544 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   69193 | 1545 | `				PH7_MemObjInit(&(*pVm),&idxProbe);` |
|   69193 | 1546 | `				PH7_MemObjStore(pIdx,&idxProbe);` |
|   69193 | 1547 | `				if( PH7_HashmapLookup(pMap,&idxProbe,&pNode) == SXRET_OK ){` |
|   69133 | 1548 | `					bDefer = 0; /* present key: fall through and read it as an aliasable slot */` |
|   34563 | 1549 | `				}else{` |
|      64 | 1550 | `					eRoot = 0; bDefer = 1;` |
|       - | 1551 | `				}` |
|   69193 | 1552 | `				PH7_MemObjRelease(&idxProbe);` |
|   97533 | 1553 | `			}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       - | 1554 | `				/* An ArrayAccess base splits the way php's read_dimension does. A WRITABLE` |
|       - | 1555 | `				 * container answers out of its own storage with no accessor call, so the` |
|       - | 1556 | `				 * fetch really can wait for the callee: deferring it is what lets a` |
|       - | 1557 | `				 * by-reference argument take php's WRITE fetch, which CREATES a missing key` |
|       - | 1558 | ``				 * (`sort($ao['nokey'])`) instead of warning about a read and passing NULL.`` |
|       - | 1559 | `				 * Everything else answers through a METHOD, and php runs that method where` |
|       - | 1560 | `				 * the subscript is WRITTEN — so the accessor runs below and its RESULT rides` |
|       - | 1561 | `				 * a prefetch carrier built at the tail of the ArrayAccess branch. */` |
|     169 | 1562 | `				ph7_class_instance *pRecInst = (ph7_class_instance *)pTos->x.pOther;` |
|     169 | 1563 | `				eRoot = 0;` |
|     335 | 1564 | `				bDefer = (pRecInst && pVm->pArrayAccessClass` |
|     166 | 1565 | `				       && PH7_VmInstanceOf(pRecInst->pClass,pVm->pArrayAccessClass)` |
|     249 | 1566 | `				       && PH7_VmDimFetchWritable(pRecInst->pClass)) ? 1 : 0;` |
|   62862 | 1567 | `			}else if( pTos->iFlags & MEMOBJ_STRING ){` |
|   62779 | 1568 | `				eRoot = 2; bDefer = 1;` |
|   32013 | 1569 | `			}else{` |
|     ! 0 | 1570 | `				eRoot = 0; bDefer = 1; /* NULL/other reachable scalar base */` |
|       - | 1571 | `			}` |
|  132133 | 1572 | `			if( bDefer && pPath == 0 ){` |
|   62915 | 1573 | `				pPath = VmDeferPathNew(&(*pVm),eRoot,pTos->nIdx,0);` |
|   62915 | 1574 | `				if( pPath == 0 ){` |
|     ! 0 | 1575 | `					bDefer = 0;` |
|     ! 0 | 1576 | `				}` |
|   32076 | 1577 | `			}` |
|   66679 | 1578 | `		}` |
|  132399 | 1579 | `		if( bDefer ){` |
|       - | 1580 | `			/* Append this element step (deep-copies pIdx) and leave the carrier on pTos. */` |
|   62935 | 1581 | `			if( VmDeferPathPushElem(pPath,pIdx) == SXRET_OK ){` |
|   62935 | 1582 | `				if( (pTos->iFlags & MEMOBJ_AUX_DEFPATH) == 0 ){` |
|       - | 1583 | `					/* Collapse the base value into the descriptor carrier. */` |
|   62917 | 1584 | `					PH7_MemObjRelease(pTos);` |
|   62917 | 1585 | `					pTos->x.pOther = pPath;` |
|   62917 | 1586 | `					pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|   62917 | 1587 | `					pTos->nIdx = SXU32_HIGH;` |
|   32077 | 1588 | `				}` |
|   62935 | 1589 | `				PH7_MemObjRelease(pIdx);` |
|   62935 | 1590 | `				VM_EXIT_BREAK;` |
|       - | 1591 | `			}` |
|       - | 1592 | `			/* Out of memory appending a step. */` |
|     ! 0 | 1593 | `			if( pTos->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|       - | 1594 | `				/* Nested base: pPath IS pTos's carrier and stays owned by it — keep it (a` |
|       - | 1595 | `				 * step short) and exit, rather than fall through and misread a NULL-typed` |
|       - | 1596 | `				 * carrier as an array base. Resolves the shorter path; OOM-only degradation. */` |
|     ! 0 | 1597 | `				PH7_MemObjRelease(pIdx);` |
|     ! 0 | 1598 | `				VM_EXIT_BREAK;` |
|       - | 1599 | `			}` |
|       - | 1600 | `			/* A freshly-allocated path we still own: drop it and fall back to a plain read. */` |
|     ! 0 | 1601 | `			VmFreeDeferredPath(pPath);` |
|     ! 0 | 1602 | `		}` |
|   34725 | 1603 | `	}` |
| 1341127 | 1604 | `	if( iP2 == 7 && (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|       - | 1605 | ``		/* Keyed list destructuring `["k"=>$v] = $src` from a NON-array source: yield NULL`` |
|       - | 1606 | `		 * (never char-index a string), warning once per key — matching PHP, which warns per` |
|       - | 1607 | `		 * key here. A NULL source is silent; unlike the positional OP_LOAD_LIST path, a bool` |
|       - | 1608 | `		 * source DOES warn (PHP warns for bool in keyed destructuring).` |
|       - | 1609 | `		 * An OBJECT is not one of these: php destructures it through its` |
|       - | 1610 | `		 * read_dimension handler like any other subscript, so it falls through to` |
|       - | 1611 | `		 * the object dispatch below — which answers out of the accessor and` |
|       - | 1612 | ``		 * raises php's `Cannot use object of type C as array` for a class that`` |
|       - | 1613 | ``		 * has none. `["k"=>$v] = $obj` warned and yielded NULL for every source`` |
|       - | 1614 | `		 * but the one shape (a writable container answering out of its own` |
|       - | 1615 | `		 * storage) that reached the fast path underneath. */` |
|       7 | 1616 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       5 | 1617 | `			VmWarnCannotUseAsArray(&(*pVm),pTos->iFlags);` |
|       2 | 1618 | `		}` |
|       7 | 1619 | `		if( pIdx ){` |
|       - | 1620 | `			/* Release the key (a string literal for keyed destructuring), like the` |
|       - | 1621 | `			 * normal hashmap-read exit below — otherwise its blob is orphaned. */` |
|       7 | 1622 | `			PH7_MemObjRelease(pIdx);` |
|       3 | 1623 | `		}` |
|       7 | 1624 | `		PH7_MemObjRelease(pTos);` |
|       7 | 1625 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|       7 | 1626 | `		VM_EXIT_BREAK;` |
|       - | 1627 | `	}` |
| 1341121 | 1628 | `	bBaseStrOff = (pTos->iFlags & MEMOBJ_AUX_STROFFSET) != 0;` |
| 1341121 | 1629 | `	if( bBaseStrOff && VmIdxCtxIsContainerWrite(iP2) ){` |
|       - | 1630 | `		/* A string OFFSET used as a CONTAINER. php reaches a string offset through a` |
|       - | 1631 | `		 * marker zval and lets whichever opcode CONSUMES it name the refusal` |
|       - | 1632 | ``		 * (`zend_wrong_string_offset_error`); a fetch that wants to reach INSIDE it —`` |
|       - | 1633 | ``		 * `$s[0][1] = 'x'`, `$s[0][1] += 1`, `unset($s[0][1])`,`` |
|       - | 1634 | ``		 * `$s[0][] = 'x'`, a by-reference argument — is `Cannot use string offset as`` |
|       - | 1635 | ``		 * an array`. PHL read the character and handed back a value still carrying the`` |
|       - | 1636 | ``		 * BASE STRING's slot, so the write landed on the base: `$s = 'ab';`` |
|       - | 1637 | ``		 * $s[0][1] = 'x';` left `$s === 'ax'` and `$a['k'][0][1] = 'x'` rewrote the`` |
|       - | 1638 | ``		 * ELEMENT. A READ (`$x = $s[0][1]`) and the lookup contexts are php's own`` |
|       - | 1639 | `		 * silence and stay out of this. */` |
|       9 | 1640 | `		if( pIdx ){` |
|       9 | 1641 | `			PH7_MemObjRelease(pIdx);` |
|       4 | 1642 | `		}` |
|       9 | 1643 | `		rc = VmThrowFromVm(&(*pVm),"Error","Cannot use string offset as an array",` |
|       - | 1644 | `			sizeof("Cannot use string offset as an array")-1);` |
|       9 | 1645 | `		PH7_MemObjRelease(pTos);` |
|       9 | 1646 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|       9 | 1647 | `		pTos->nIdx = SXU32_HIGH;` |
|       9 | 1648 | `		if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       9 | 1649 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1650 | `	}` |
| 1341113 | 1651 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|       - | 1652 | `		/* String access */` |
| 1071396 | 1653 | `		if( pIdx == 0 && VmIdxCtxIsContainerWrite(iP2) ){` |
|       - | 1654 | ``			/* `$s[] op= v` / `$s[]++`: php refuses an APPEND to a string wherever it`` |
|       - | 1655 | ``			 * lands — `[] operator not supported for strings` — and the plain`` |
|       - | 1656 | ``			 * `$s[] = 'x'` store already raises it (OP_STORE_IDX). The`` |
|       - | 1657 | `			 * read-modify-write spellings fell through to "load NULL" here and then` |
|       - | 1658 | `` 			 * wrote the computed value back through the BASE's slot, so `$s[] .= 'x'` `` |
|       - | 1659 | ``			 * APPENDED to the string and `$s[]++` incremented the whole of it. */`` |
|       5 | 1660 | `			rc = VmThrowFromVm(&(*pVm),"Error","[] operator not supported for strings",` |
|       - | 1661 | `				sizeof("[] operator not supported for strings")-1);` |
|       5 | 1662 | `			PH7_MemObjRelease(pTos);` |
|       5 | 1663 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       5 | 1664 | `			pTos->nIdx = SXU32_HIGH;` |
|       5 | 1665 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       5 | 1666 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1667 | `		}` |
| 1071392 | 1668 | `		if( iP2 == VM_IDX_CTX_UNSET ){` |
|       - | 1669 | ``			/* php: a string offset cannot be unset AT ALL — `unset($s[0])` is the`` |
|       - | 1670 | ``			 * catchable `Error: Cannot unset string offsets`, whatever the offset is`` |
|       - | 1671 | `			 * and whether or not it is in range. PHL read the character but left the` |
|       - | 1672 | `			 * BASE VARIABLE's slot index on the result, so the trailing unset()` |
|       - | 1673 | ``			 * builtin freed the base itself: `$s = "abc"; unset($s[1]);` left $s`` |
|       - | 1674 | ``			 * UNDEFINED, and `unset($a["k"][0])` deleted the whole element.`` |
|       - | 1675 | ``			 * An INTERMEDIATE level of the chain (`unset($s[0][1])`,`` |
|       - | 1676 | ``			 * `unset($s[0]->p)`) is NOT this: that fetch hands the offset on to`` |
|       - | 1677 | `			 * something that reaches INSIDE it, and php words the refusal from` |
|       - | 1678 | `			 * whatever that is — so it falls through to the offset resolution below` |
|       - | 1679 | `			 * (a write-shaped fetch) and the consumer raises. */` |
|      14 | 1680 | `			rc = VmThrowFromVm(&(*pVm),"Error","Cannot unset string offsets",` |
|       - | 1681 | `				sizeof("Cannot unset string offsets")-1);` |
|      14 | 1682 | `			if( pIdx ){` |
|      14 | 1683 | `				PH7_MemObjRelease(pIdx);` |
|       6 | 1684 | `			}` |
|      14 | 1685 | `			PH7_MemObjRelease(pTos);` |
|      14 | 1686 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      14 | 1687 | `			pTos->nIdx = SXU32_HIGH;` |
|      14 | 1688 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      14 | 1689 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1690 | `		}` |
| 1071380 | 1691 | `		if( pIdx ){` |
| 1071380 | 1692 | `			sxi64 iOfft = 0, iRaw;` |
| 1071380 | 1693 | `			sxi64 nLen = (sxi64)SyBlobLength(&pTos->sBlob);` |
|       - | 1694 | `			/* LOAD_IDX carries its OWN iP2 codes, which do NOT line up with the` |
|       - | 1695 | ``			 * PH7_MEMBER_* ones: 4 = isset, 5 = unset, 6 = empty, 8 = `??` read,`` |
|       - | 1696 | ``			 * 3 = `??=` peek. All are LOOKUPS, but php splits them into TWO`` |
|       - | 1697 | `			 * levels: isset()/empty()/unset() say nothing at all, while a` |
|       - | 1698 | ``			 * `??`/`??=` fetch still warns about the offset SHAPE and reads it`` |
|       - | 1699 | `			 * (VM_STROFF_COALESCE). The ??= peek is recognised by the NULLC_JMP` |
|       - | 1700 | `			 * that follows it, since its iP2 does not distinguish the base type. */` |
| 1609990 | 1701 | `			int iOfftLevel = (iP2 == 4 \|\| iP2 == VM_IDX_CTX_UNSET \|\| iP2 == 6) ? VM_STROFF_ISSET` |
| 2142678 | 1702 | `				: (iP2 == VM_IDX_CTX_UNSET_BASE ? VM_STROFF_UNSETBASE` |
| 2136686 | 1703 | `				: ((iP2 == 8 \|\| VmIdxFeedsCoalesce(pInstr)) ? VM_STROFF_COALESCE` |
| 1065363 | 1704 | `				: VM_STROFF_LOUD));` |
| 1071380 | 1705 | `			int bQuiet = iOfftLevel != VM_STROFF_LOUD;` |
|       - | 1706 | `			SyBlob sTypeMsg;` |
|       - | 1707 | `			int eOfft;` |
| 1071380 | 1708 | `			VmCoalStrOff *pCoalOff = 0;` |
| 1071380 | 1709 | `			if( VmIdxFeedsCoalesceAssign(pInstr) ){` |
|       - | 1710 | ``				/* `$s[k] ??= v`: the OP_NULLC_STORE ahead has to write into the`` |
|       - | 1711 | `				 * string OFFSET, and by then the offset is gone — this op consumes` |
|       - | 1712 | `				 * it. Carry the RAW index to the store on the peek's own result` |
|       - | 1713 | `				 * (MEMOBJ_AUX_COALSTROFF), which nests and cannot leak; the copy` |
|       - | 1714 | `				 * must predate the resolution below, which casts a float/null/bool` |
|       - | 1715 | `				 * in place, because php re-resolves the offset LOUDLY at the store:` |
|       - | 1716 | `				 * the peek is the quiet half of its pair. */` |
|      55 | 1717 | `				pCoalOff = VmCoalStrOffNew(&(*pVm),pIdx);` |
|      27 | 1718 | `			}` |
| 1071380 | 1719 | `			eOfft = VmStringOffsetResolve(&(*pVm),pIdx,iOfftLevel,&iOfft,&sTypeMsg);` |
| 1071380 | 1720 | `			if( eOfft == VM_STROFF_MISS ){` |
|       - | 1721 | `				/* A lookup over an offset php refuses: not set, in silence. */` |
|      32 | 1722 | `				PH7_MemObjRelease(pIdx);` |
|      32 | 1723 | `				PH7_MemObjRelease(pTos);` |
|      32 | 1724 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      32 | 1725 | `				if( pCoalOff && bBaseStrOff ){` |
|       - | 1726 | `					/* The store this peek is arming would land INSIDE a string offset:` |
|       - | 1727 | `					 * refused, like every other reach-inside (the same answer the` |
|       - | 1728 | `					 * out-of-range peek below gets). */` |
|     ! 0 | 1729 | `					VmFreeCoalStrOff(pCoalOff);` |
|     ! 0 | 1730 | `					rc = VmThrowFromVm(&(*pVm),"Error","Cannot use string offset as an array",` |
|       - | 1731 | `						sizeof("Cannot use string offset as an array")-1);` |
|     ! 0 | 1732 | `					pTos->nIdx = SXU32_HIGH;` |
|      35 | 1733 | `					if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     ! 0 | 1734 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1735 | `				}` |
|      32 | 1736 | `				if( pCoalOff ){` |
|       - | 1737 | ``					/* A `??=` whose offset the READ refuses: php raises at the`` |
|       - | 1738 | `					 * STORE instead, so keep the base slot reachable and hand the` |
|       - | 1739 | `					 * offset to OP_NULLC_STORE. */` |
|       3 | 1740 | `					pTos->x.pOther = (void *)pCoalOff;` |
|       3 | 1741 | `					pTos->iFlags \|= MEMOBJ_AUX_STROFFSET\|MEMOBJ_AUX_COALSTROFF;` |
|       2 | 1742 | `				}else{` |
|      30 | 1743 | `					pTos->nIdx = SXU32_HIGH;` |
|       - | 1744 | `				}` |
|      32 | 1745 | `				VM_EXIT_BREAK;` |
|       - | 1746 | `			}` |
| 1071350 | 1747 | `			if( eOfft == VM_STROFF_REJECT && iOfftLevel == VM_STROFF_UNSETBASE ){` |
|       - | 1748 | `				/* The intermediate step of an unset chain over an offset php cannot` |
|       - | 1749 | ``				 * use at all (`unset($s["k"][0])`, `unset($s[""]->p)`): the refusal`` |
|       - | 1750 | `				 * is the UNSET's, not the read's TypeError. */` |
|     ! 0 | 1751 | `				VmFreeCoalStrOff(pCoalOff);` |
|     ! 0 | 1752 | `				SyBlobRelease(&sTypeMsg);` |
|     ! 0 | 1753 | `				rc = VmThrowFromVm(&(*pVm),"Error","Cannot unset string offsets",` |
|       - | 1754 | `					sizeof("Cannot unset string offsets")-1);` |
|     ! 0 | 1755 | `				PH7_MemObjRelease(pIdx);` |
|     ! 0 | 1756 | `				PH7_MemObjRelease(pTos);` |
|     ! 0 | 1757 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|     ! 0 | 1758 | `				pTos->nIdx = SXU32_HIGH;` |
|     ! 0 | 1759 | `				if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     ! 0 | 1760 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1761 | `			}` |
| 1071350 | 1762 | `			if( eOfft == VM_STROFF_REJECT ){` |
|       - | 1763 | `				/* php's TypeError for an offset type a string refuses. PHL cast` |
|       - | 1764 | ``				 * every offset to int, so `$s[""]`, `$s["-"]` and `$s["p"]` all`` |
|       - | 1765 | ``				 * answered `$s[0]`. Routed as a mid-expression throw (this opcode`` |
|       - | 1766 | `				 * is not a call boundary), so the rest of the expression is` |
|       - | 1767 | `				 * abandoned the way php abandons it. */` |
|      42 | 1768 | `				VmFreeCoalStrOff(pCoalOff);` |
|      42 | 1769 | `				rc = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|      42 | 1770 | `				PH7_MemObjRelease(pIdx);` |
|      42 | 1771 | `				PH7_MemObjRelease(pTos);` |
|      42 | 1772 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      42 | 1773 | `				pTos->nIdx = SXU32_HIGH;` |
|      42 | 1774 | `				if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      42 | 1775 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1776 | `			}` |
| 1071312 | 1777 | `			iRaw = iOfft;` |
|       - | 1778 | `			/* php 7.1: a NEGATIVE offset counts back from the end ($s[-1] is the last` |
|       - | 1779 | `			 * character). The offset used to be cast to UNSIGNED, so -1 became a huge` |
|       - | 1780 | `			 * number, ran past the end and quietly produced NULL. */` |
| 1071312 | 1781 | `			if( iOfft < 0 ){` |
|      20 | 1782 | `				iOfft += nLen;` |
|       9 | 1783 | `			}` |
| 1071312 | 1784 | `			if( iOfft < 0 \|\| iOfft >= nLen ){` |
|       - | 1785 | `				/* Out of range. In an isset()/empty() lookup php answers FALSE, so load` |
|       - | 1786 | `				 * NULL there; everywhere else it WARNS and yields the empty string (PH7` |
|       - | 1787 | `				 * silently produced NULL in both cases). */` |
|      88 | 1788 | `				PH7_MemObjRelease(pTos);` |
|      88 | 1789 | `				if( bQuiet ){` |
|      70 | 1790 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      36 | 1791 | `				}else{` |
|      20 | 1792 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|      20 | 1793 | `					if( iP2 != 1 && iP2 != VM_IDX_CTX_UNSET_BASE ){` |
|       - | 1794 | `						/* A WRITE-context fetch never READS the character in php: it` |
|       - | 1795 | `						 * resolves the offset (loudly — the SHAPE diagnostics above are` |
|       - | 1796 | `						 * php's there too) and hands back its offset marker, and the` |
|       - | 1797 | ``						 * opcode that consumes it refuses. So `$s[5] .= 'x'` and`` |
|       - | 1798 | ``						 * `$s[5]++` are the assign-op / incr-decr Error with nothing`` |
|       - | 1799 | ``						 * said about offset 5, where PHL announced an `Uninitialized`` |
|       - | 1800 | ``						 * string offset 5` it never had to look at. */`` |
|      23 | 1801 | `						VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Uninitialized string offset %qd",` |
|       7 | 1802 | `							iRaw);` |
|       7 | 1803 | `					}` |
|       - | 1804 | `				}` |
|      45 | 1805 | `			}else{` |
| 1071226 | 1806 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
| 1071226 | 1807 | `				int c = zData[iOfft];` |
| 1071226 | 1808 | `				PH7_MemObjRelease(pTos);` |
| 1071226 | 1809 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
| 1071226 | 1810 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|       - | 1811 | `			}` |
| 1071312 | 1812 | `			if( pCoalOff ){` |
|      51 | 1813 | `				if( (pTos->iFlags & MEMOBJ_NULL) && bBaseStrOff ){` |
|       - | 1814 | ``					/* `$s[0][9] ??= v`: the peek came back empty, so the `??=` WILL`` |
|       - | 1815 | `					 * store — and the thing it would store into is a character inside a` |
|       - | 1816 | `					 * string offset, which php refuses like every other reach-inside.` |
|       - | 1817 | `					 * The refusal belongs to the store, which is why the peek that finds` |
|       - | 1818 | ``					 * a character (`$s[0][0] ??= 7`) short-circuits and says nothing at`` |
|       - | 1819 | `					 * all in php. */` |
|       3 | 1820 | `					VmFreeCoalStrOff(pCoalOff);` |
|       3 | 1821 | `					rc = VmThrowFromVm(&(*pVm),"Error","Cannot use string offset as an array",` |
|       - | 1822 | `						sizeof("Cannot use string offset as an array")-1);` |
|       3 | 1823 | `					PH7_MemObjRelease(pTos);` |
|       3 | 1824 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       3 | 1825 | `					pTos->nIdx = SXU32_HIGH;` |
|       3 | 1826 | `					if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       3 | 1827 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1828 | `				}` |
|      49 | 1829 | `				if( pTos->iFlags & MEMOBJ_NULL ){` |
|       - | 1830 | ``					/* Out of range: the `??=` will store, so hand the offset over. */`` |
|      41 | 1831 | `					pTos->x.pOther = (void *)pCoalOff;` |
|      41 | 1832 | `					pTos->iFlags \|= MEMOBJ_AUX_COALSTROFF;` |
|      21 | 1833 | `				}else{` |
|       - | 1834 | ``					/* A real byte: the `??=` short-circuits over the store. */`` |
|       9 | 1835 | `					VmFreeCoalStrOff(pCoalOff);` |
|       - | 1836 | `				}` |
|      24 | 1837 | `			}` |
|       - | 1838 | `			/* The result still carries the BASE VARIABLE's slot index, which is` |
|       - | 1839 | `			 * harmless for a plain read and WRONG for anything that would ALIAS it:` |
|       - | 1840 | `			 * a string offset is not a slot. Mark it so the reference-binding sites` |
|       - | 1841 | `			 * raise php's Error instead of aliasing the whole string --` |
|       - | 1842 | ``			 * `$r = &$s[1]; $r = "Z";` REPLACED $s with "Z". */`` |
| 1071310 | 1843 | `			pTos->iFlags \|= MEMOBJ_AUX_STROFFSET;` |
|  538625 | 1844 | `		}else{` |
|       - | 1845 | `			/* No available index,load NULL */` |
|     ! 0 | 1846 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       - | 1847 | `		}` |
| 1071310 | 1848 | `		VM_EXIT_BREAK;` |
|       - | 1849 | `	}` |
|  269722 | 1850 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       - | 1851 | `		/* Object subscript: ArrayAccess dispatch.` |
|       - | 1852 | `		 * iP2 codes:` |
|       - | 1853 | `		 *   0 = read       → offsetGet` |
|       - | 1854 | `		 *   3 = ??= peek   → offsetExists; offsetGet on hit; arm coalesce` |
|       - | 1855 | `		 *                    target on miss for the upcoming NULLC_STORE` |
|       - | 1856 | `		 *   4 = isset()    → offsetExists` |
|       - | 1857 | `		 *   5 = unset()    → offsetUnset` |
|       - | 1858 | `		 *   6 = empty()    → offsetExists, then offsetGet on hit` |
|       - | 1859 | ``		 *   8 = `??` read  → same probe as 6 (offsetExists, then offsetGet on a`` |
|       - | 1860 | `		 *                    hit) and no diagnostics anywhere in this op: php` |
|       - | 1861 | ``		 *                    evaluates the whole left operand of `??` in`` |
|       - | 1862 | `		 *                    isset-context, and calling offsetGet blindly also` |
|       - | 1863 | `		 *                    surfaced warnings raised INSIDE a userland` |
|       - | 1864 | `		 *                    offsetGet (e.g. ArrayObject's own array read). */` |
|    1057 | 1865 | `		ph7_class_instance *pInst = (ph7_class_instance *)pTos->x.pOther;` |
|    1057 | 1866 | `		ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|       - | 1867 | `		/* php's read_dimension / has_dimension HANDLERS, which a native class may` |
|       - | 1868 | ``		 * carry without implementing ArrayAccess -- `$list[0]` reads a DOMNodeList`` |
|       - | 1869 | ``		 * there while `$list instanceof ArrayAccess` is false. They come FIRST`` |
|       - | 1870 | `		 * because php's interface is implemented THROUGH the handler: a user` |
|       - | 1871 | `		 * subclass declaring ArrayAccess inherits the parent's handler, so its own` |
|       - | 1872 | `		 * offsetGet/offsetExists are not consulted for a READ. The WRITE half is` |
|       - | 1873 | `		 * not here at all -- a store, an append and an unset (iP2 5) fall past` |
|       - | 1874 | ``		 * this into php's `Cannot use object of type C as array` unless the class`` |
|       - | 1875 | `		 * really implements the interface, which is php's own split (that same` |
|       - | 1876 | `		 * subclass DOES get its offsetSet called). */` |
|    1057 | 1877 | `		if( pInst && iP2 != 5 && PH7_ClassHasNativeDim(pInst->pClass) ){` |
|       - | 1878 | `			PH7_NativeDimCtx sDim;` |
|       - | 1879 | `			ph7_value sResult;` |
|       - | 1880 | ``			/* `$o[$k] op= v` is php's read-then-WRITE pair, and php reports the`` |
|       - | 1881 | `			 * WRITE's refusal when the read answered nothing at all: an offset the` |
|       - | 1882 | ``			 * handler REFUSED comes back to `zend_binary_assign_op_obj_dim` as a`` |
|       - | 1883 | ``			 * miss, which raises `Cannot use object of type C as array` and chains`` |
|       - | 1884 | ``			 * the refusal behind it. `??=` is not that pair -- it reads in`` |
|       - | 1885 | `			 * isset-context, so its refusal is what surfaces -- and neither of them` |
|       - | 1886 | `			 * decides the store itself: that goes through the ordinary write path` |
|       - | 1887 | `			 * below, which is offsetSet for a subclass that has one. */` |
|     358 | 1888 | `			int bRmwCtx = (iP2 == 1) && VmNextIsCompoundAssign(pInstr + 1);` |
|     358 | 1889 | `			int bIsset = (iP2 == 4 \|\| iP2 == 6);` |
|     358 | 1890 | `			PH7_MemObjInit(&(*pVm),&sResult);` |
|     358 | 1891 | `			sDim.iMode = bIsset ? PH7_NATIVE_DIM_ISSET : PH7_NATIVE_DIM_READ;` |
|     358 | 1892 | `			sDim.pOffset = pIdx;` |
|     358 | 1893 | `			sDim.pResult = &sResult;` |
|     358 | 1894 | `			sDim.zThrowClass = 0;` |
|     358 | 1895 | `			sDim.zThrowMsg[0] = 0;` |
|     358 | 1896 | `			sDim.bStored = 0;` |
|     358 | 1897 | `			PH7_ClassNativeDim(pInst,&sDim);` |
|     358 | 1898 | `			if( iP2 == 6 && sDim.zThrowClass == 0 && ph7_value_to_bool(&sResult) ){` |
|       - | 1899 | `				/* empty(): php asks has_dimension first and reads the VALUE only on` |
|       - | 1900 | ``				 * a hit, which is why an out-of-range `empty($map[-1])` is a plain`` |
|       - | 1901 | `				 * TRUE where the read of the same offset refuses. The emptiness` |
|       - | 1902 | `				 * question goes to the handler first -- a class that judges it on` |
|       - | 1903 | `				 * something other than the value it would HAND BACK answers here --` |
|       - | 1904 | `				 * and a handler that has no answer leaves the value read below. */` |
|       7 | 1905 | `				PH7_MemObjRelease(&sResult);` |
|       7 | 1906 | `				PH7_MemObjInit(&(*pVm),&sResult);` |
|       7 | 1907 | `				sDim.iMode = PH7_NATIVE_DIM_NOTEMPTY;` |
|       7 | 1908 | `				sDim.pResult = &sResult;` |
|       7 | 1909 | `				PH7_ClassNativeDim(pInst,&sDim);` |
|       7 | 1910 | `				if( (sResult.iFlags & MEMOBJ_NULL) && sDim.zThrowClass == 0 ){` |
|       7 | 1911 | `					sDim.iMode = PH7_NATIVE_DIM_READ;` |
|       7 | 1912 | `					sDim.pResult = &sResult;` |
|       7 | 1913 | `					PH7_ClassNativeDim(pInst,&sDim);` |
|       3 | 1914 | `				}` |
|       3 | 1915 | `			}` |
|     358 | 1916 | `			if( sDim.zThrowClass ){` |
|       - | 1917 | `				char zMsg[256];` |
|      32 | 1918 | `				const char *zClass = sDim.zThrowClass;` |
|      32 | 1919 | `				const char *zText = sDim.zThrowMsg;` |
|       - | 1920 | `				sxu32 nMsg;` |
|      32 | 1921 | `				if( bRmwCtx ){` |
|       5 | 1922 | `					SyString *pName = &pInst->pClass->sName;` |
|       5 | 1923 | `					zClass = "Error";` |
|       5 | 1924 | `					zText = zMsg;` |
|       7 | 1925 | `					nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - | 1926 | `						"Cannot use object of type %.*s as array",` |
|       4 | 1927 | `						(int)pName->nByte,pName->zString);` |
|       3 | 1928 | `				}else{` |
|      28 | 1929 | `					nMsg = (sxu32)SyStrlen(zText);` |
|       - | 1930 | `				}` |
|      32 | 1931 | `				VmCoalesceDisarm(pVm);` |
|      32 | 1932 | `				rc = VmThrowFromVm(pVm,zClass,zText,nMsg);` |
|      32 | 1933 | `				PH7_MemObjRelease(&sResult);` |
|      32 | 1934 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|      32 | 1935 | `				PH7_MemObjRelease(pTos);` |
|      32 | 1936 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      32 | 1937 | `				pTos->nIdx = SXU32_HIGH;` |
|      32 | 1938 | `				if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      38 | 1939 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1940 | `			}` |
|     328 | 1941 | `			if( iP2 == 4 ){` |
|       - | 1942 | `				/* isset(): push a BOOL, which is also what keeps vm_builtin_isset` |
|       - | 1943 | `				 * from warning about a non-variable operand. */` |
|      33 | 1944 | `				int bExists = ph7_value_to_bool(&sResult);` |
|      33 | 1945 | `				PH7_MemObjRelease(&sResult);` |
|      33 | 1946 | `				PH7_MemObjRelease(pTos);` |
|      33 | 1947 | `				pTos->nIdx = SXU32_HIGH;` |
|      33 | 1948 | `				if( bExists ){` |
|       9 | 1949 | `					MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       9 | 1950 | `					pTos->x.iVal = 1;` |
|       5 | 1951 | `				}else{` |
|      25 | 1952 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       1 | 1953 | `				}` |
|     312 | 1954 | `			}else if( iP2 == 3 && (sResult.iFlags & MEMOBJ_NULL) ){` |
|       - | 1955 | ``				/* `$o[$k] ??= v` and the read found nothing: arm (object, key) so`` |
|       - | 1956 | `				 * the NULLC_STORE that follows performs php's store -- offsetSet for` |
|       - | 1957 | ``				 * a subclass that declares one, and `Cannot use object of type C as`` |
|       - | 1958 | ``				 * array` for the collections themselves, which is the same verdict`` |
|       - | 1959 | ``				 * the plain `$o[$k] = v` gets. */`` |
|       5 | 1960 | `				VmCoalesceDisarm(pVm);` |
|       5 | 1961 | `				PH7_MemObjRelease(pTos);` |
|       5 | 1962 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|       5 | 1963 | `				pTos->nIdx = SXU32_HIGH;` |
|       5 | 1964 | `				if( pIdx ){` |
|       5 | 1965 | `					PH7_MemObjStore(pIdx,&pVm->sCoalesceKey);` |
|       2 | 1966 | `				}` |
|       5 | 1967 | `				pVm->pCoalesceObj = pInst;` |
|       5 | 1968 | `				pInst->iRef++;` |
|       5 | 1969 | `				pVm->bCoalesceArmed = 1;` |
|       5 | 1970 | `				PH7_MemObjRelease(&sResult);` |
|       3 | 1971 | `			}else{` |
|       - | 1972 | `				/* The base slot may be the only thing holding this instance, and the` |
|       - | 1973 | `				 * write-context tail below still speaks for its CLASS -- hold a` |
|       - | 1974 | `				 * reference across the release, as the ArrayAccess arm does. */` |
|     292 | 1975 | `				pInst->iRef++;` |
|     292 | 1976 | `				if( iP2 == 3 ){` |
|       3 | 1977 | `					VmCoalesceDisarm(pVm); /* a hit short-circuits over the store */` |
|       1 | 1978 | `				}` |
|     292 | 1979 | `				PH7_MemObjRelease(pTos);` |
|     292 | 1980 | `				PH7_MemObjStore(&sResult,pTos);` |
|     292 | 1981 | `				pTos->nIdx = SXU32_HIGH;` |
|     292 | 1982 | `				if( bRmwCtx ){` |
|       - | 1983 | `					/* php's ASSIGN_DIM_OP: the read gave the current value, the op` |
|       - | 1984 | `					 * computes on it, and the result goes back out through the write` |
|       - | 1985 | `					 * path -- offsetSet where there is one, php's Error where there` |
|       - | 1986 | `					 * is not (VmHookRmwConsume). */` |
|      20 | 1987 | `					VmDimRmwArm(&(*pVm),pInst,pIdx,pTos,` |
|      12 | 1988 | `						(void *)pStack,(void *)aInstr,(sxu32)(pc + 1));` |
|     286 | 1989 | `				}else if( VmIdxFetchForWrite(pInstr,iP2) ){` |
|       - | 1990 | ``					/* php's `Indirect modification of overloaded element` -- silent`` |
|       - | 1991 | `					 * for an OBJECT, which is every value these containers answer,` |
|       - | 1992 | ``					 * and raised for the NULL a miss leaves (`$list[9]++`). */`` |
|       5 | 1993 | `					PH7_VmOverloadedElemNotice(&(*pVm),pInst->pClass,pTos);` |
|     278 | 1994 | `				}else if( pInstr->iP2 == 9 ){` |
|       - | 1995 | `					/* A deferred call ARGUMENT: the read has happened, and whether php` |
|       - | 1996 | `					 * performed a W fetch is the callee's to say. Carry the value plus` |
|       - | 1997 | `					 * the class that answered it so the verdict lands at the call. */` |
|     106 | 1998 | `					VmDeferredPath *pPre = VmDeferPathNewPrefetch(&(*pVm),VM_OVER_ELEM,` |
|      34 | 1999 | `						pInst->pClass,0,pTos);` |
|      72 | 2000 | `					if( pPre ){` |
|      72 | 2001 | `						PH7_MemObjRelease(pTos);` |
|      72 | 2002 | `						pTos->x.pOther = pPre;` |
|      72 | 2003 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|      72 | 2004 | `						pTos->nIdx = SXU32_HIGH;` |
|      34 | 2005 | `					}` |
|      34 | 2006 | `				}` |
|     292 | 2007 | `				PH7_ClassInstanceUnref(pInst);` |
|     292 | 2008 | `				PH7_MemObjRelease(&sResult);` |
|       - | 2009 | `			}` |
|     328 | 2010 | `			if( pIdx ){` |
|     328 | 2011 | `				PH7_MemObjRelease(pIdx);` |
|     162 | 2012 | `			}` |
|     328 | 2013 | `			VM_EXIT_BREAK;` |
|       - | 2014 | `		}` |
|     703 | 2015 | `		if( pArrayAccess && pInst && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|       - | 2016 | `			ph7_class_method *pMeth;` |
|       - | 2017 | `			ph7_value sResult;` |
|       - | 2018 | `			ph7_value sNullIdx;` |
|       - | 2019 | `			ph7_value *apArg[1];` |
|     676 | 2020 | `			if( pIdx && VmDimFastFetchCtx(pInstr,iP2) && PH7_VmDimFetchWritable(pInst->pClass)` |
|     283 | 2021 | `			 && pInst->iRef > 1 ){` |
|       - | 2022 | `				/* php hands a writable container's element back BY SLOT, and that is what` |
|       - | 2023 | ``				 * makes an indirect modification through it land. The `iRef > 1` guard is`` |
|       - | 2024 | ``				 * the object half of the array path's `pMap->iRef < 2` rule: releasing the`` |
|       - | 2025 | `				 * base below drops this stack slot's own reference, and a TEMPORARY` |
|       - | 2026 | ``				 * container (`(new ArrayObject([1]))[0]`) would be destroyed with its`` |
|       - | 2027 | `				 * storage while the result still views it. Such a base has nothing that` |
|       - | 2028 | `				 * could observe the write anyway, so it takes the accessor's copy. */` |
|     211 | 2029 | `				sxu32 nElem = PH7_SplDimElemSlot(&(*pVm),pInst,pIdx,` |
|       - | 2030 | `					/* php's write-context vivification, and only there: a W/RW fetch —` |
|       - | 2031 | ``					 * including the `$r = &$ao['k']` and by-ref-foreach shapes iP2 alone`` |
|       - | 2032 | `					 * cannot name — creates the missing element, while an unset chain's` |
|       - | 2033 | `					 * intermediate step never does. */` |
|     181 | 2034 | `					VmIdxFetchForWrite(pInstr,iP2) && !VM_IDX_IS_UNSET(iP2));` |
|     157 | 2035 | `				ph7_value *pElem = (nElem == SXU32_HIGH) ? 0` |
|     140 | 2036 | `					: (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nElem);` |
|     157 | 2037 | `				if( pElem ){` |
|     128 | 2038 | `					PH7_MemObjRelease(pTos);` |
|     128 | 2039 | `					PH7_MemObjLoad(pElem,pTos);` |
|     128 | 2040 | `					pTos->nIdx = nElem;` |
|     128 | 2041 | `					PH7_MemObjRelease(pIdx);` |
|     128 | 2042 | `					VM_EXIT_BREAK;` |
|       - | 2043 | `				}` |
|      14 | 2044 | `			}` |
|     555 | 2045 | `			if( (iP2 == 0 \|\| iP2 == 3 \|\| iP2 == 8) && pIdx == 0 ){` |
|       - | 2046 | ``				/* `$obj[]` read — PHP rejects this. */`` |
|     ! 0 | 2047 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|       - | 2048 | `					"Cannot use [] for reading");` |
|     ! 0 | 2049 | `				PH7_MemObjRelease(pTos);` |
|     ! 0 | 2050 | `				pTos->nIdx = SXU32_HIGH;` |
|     ! 0 | 2051 | `				VM_EXIT_BREAK;` |
|       - | 2052 | `			}` |
|     555 | 2053 | `			PH7_MemObjInit(&(*pVm),&sResult);` |
|     555 | 2054 | `			if( iP2 == 4 \|\| iP2 == 6 \|\| iP2 == 3 \|\| iP2 == 8 ){` |
|       - | 2055 | ``				/* isset, empty, ??= and `??` all start with offsetExists. */`` |
|     173 | 2056 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 2057 | `					"offsetExists",sizeof("offsetExists")-1);` |
|     173 | 2058 | `				apArg[0] = pIdx;` |
|     173 | 2059 | `				if( pMeth ){` |
|     173 | 2060 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|      89 | 2061 | `				}` |
|     471 | 2062 | `			}else if( iP2 == 5 ){` |
|      81 | 2063 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 2064 | `					"offsetUnset",sizeof("offsetUnset")-1);` |
|      81 | 2065 | `				apArg[0] = pIdx;` |
|      81 | 2066 | `				if( pMeth ){` |
|      81 | 2067 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|      38 | 2068 | `				}` |
|      43 | 2069 | `			}else{` |
|     311 | 2070 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 2071 | `					"offsetGet",sizeof("offsetGet")-1);` |
|     311 | 2072 | `				if( pIdx == 0 ){` |
|       - | 2073 | ``					/* `$o[] op= v` — the one read that reaches here without a key.`` |
|       - | 2074 | `					 * php hands the accessors NULL for the absent offset (its` |
|       - | 2075 | `					 * read_dimension substitutes one), so passing NO argument` |
|       - | 2076 | `					 * turned an assignment php performs into an` |
|       - | 2077 | `					 * ArgumentCountError against the class's own offsetGet. */` |
|       3 | 2078 | `					PH7_MemObjInit(&(*pVm),&sNullIdx);` |
|       3 | 2079 | `					pIdx = &sNullIdx;` |
|       1 | 2080 | `				}` |
|     311 | 2081 | `				apArg[0] = pIdx;` |
|     311 | 2082 | `				if( pMeth ){` |
|     311 | 2083 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|     153 | 2084 | `				}` |
|       - | 2085 | `			}` |
|     555 | 2086 | `			if( iP2 == 4 ){` |
|       - | 2087 | `				/* isset: push MEMOBJ_BOOL so vm_builtin_isset reports the` |
|       - | 2088 | `				 * right truth value AND skips its "Expecting a variable not` |
|       - | 2089 | `				 * a constant" warning (keyed on MEMOBJ_BOOL). */` |
|     119 | 2090 | `				int bExists = ph7_value_to_bool(&sResult);` |
|     119 | 2091 | `				PH7_MemObjRelease(pTos);` |
|     119 | 2092 | `				pTos->nIdx = SXU32_HIGH;` |
|     119 | 2093 | `				if( bExists ){` |
|      58 | 2094 | `					MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      58 | 2095 | `					pTos->x.iVal = 1;` |
|      31 | 2096 | `				}else{` |
|      65 | 2097 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       5 | 2098 | `				}` |
|     498 | 2099 | `			}else if( iP2 == 5 ){` |
|       - | 2100 | `				/* offsetUnset return is discarded; push NULL so the trailing` |
|       - | 2101 | `				 * vm_builtin_unset is a harmless no-op. */` |
|      81 | 2102 | `				PH7_MemObjRelease(pTos);` |
|      81 | 2103 | `				pTos->nIdx = SXU32_HIGH;` |
|      81 | 2104 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|     403 | 2105 | `			}else if( iP2 == 6 \|\| iP2 == 8 ){` |
|       - | 2106 | `				/* empty: if offsetExists is false, push NULL so empty=true` |
|       - | 2107 | `				 * without calling offsetGet. If true, call offsetGet and` |
|       - | 2108 | `				 * push the value so PH7_builtin_empty evaluates emptiness.` |
|       - | 2109 | ``				 * `??` (8) needs the identical shape: NULL on a miss so the`` |
|       - | 2110 | `				 * coalesce takes the default, the real value on a hit. */` |
|      50 | 2111 | `				int bExists = ph7_value_to_bool(&sResult);` |
|      50 | 2112 | `				PH7_MemObjRelease(&sResult);` |
|      50 | 2113 | `				PH7_MemObjRelease(pTos);` |
|      50 | 2114 | `				pTos->nIdx = SXU32_HIGH;` |
|      50 | 2115 | `				if( !bExists ){` |
|      23 | 2116 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      13 | 2117 | `				}else{` |
|      30 | 2118 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 2119 | `						"offsetGet",sizeof("offsetGet")-1);` |
|       - | 2120 | `					ph7_value sValue;` |
|      30 | 2121 | `					PH7_MemObjInit(&(*pVm),&sValue);` |
|      30 | 2122 | `					apArg[0] = pIdx;` |
|      30 | 2123 | `					if( pGet ){` |
|      30 | 2124 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|      13 | 2125 | `					}` |
|      30 | 2126 | `					PH7_MemObjStore(&sValue,pTos);` |
|      30 | 2127 | `					PH7_MemObjRelease(&sValue);` |
|       - | 2128 | `				}` |
|      50 | 2129 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|      50 | 2130 | `				VM_EXIT_BREAK; /* skip the duplicate sResult release below */` |
|     319 | 2131 | `			}else if( iP2 == 3 ){` |
|       - | 2132 | `				/* ?? null-coalesce peek: emulate PHP semantics —` |
|       - | 2133 | `				 *   if !offsetExists OR offsetGet() === null → arm` |
|       - | 2134 | `				 *     coalesce slot (NULLC_STORE will call offsetSet)` |
|       - | 2135 | `				 *     and push NULL.` |
|       - | 2136 | `				 *   else → push offsetGet's value (NULLC_JMP skips). */` |
|      10 | 2137 | `				int bExists = ph7_value_to_bool(&sResult);` |
|      10 | 2138 | `				int bShouldArm = !bExists;` |
|       - | 2139 | `				ph7_value sValue;` |
|      10 | 2140 | `				PH7_MemObjRelease(&sResult);` |
|       - | 2141 | `				/* Reset any prior arming defensively */` |
|      10 | 2142 | `				VmCoalesceDisarm(pVm);` |
|      10 | 2143 | `				PH7_MemObjInit(&(*pVm),&sValue);` |
|      10 | 2144 | `				if( bExists ){` |
|       5 | 2145 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 2146 | `						"offsetGet",sizeof("offsetGet")-1);` |
|       5 | 2147 | `					apArg[0] = pIdx;` |
|       5 | 2148 | `					if( pGet ){` |
|       5 | 2149 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|       2 | 2150 | `					}` |
|       5 | 2151 | `					if( sValue.iFlags & MEMOBJ_NULL ){` |
|       3 | 2152 | `						bShouldArm = 1;` |
|       1 | 2153 | `					}` |
|       2 | 2154 | `				}` |
|      10 | 2155 | `				PH7_MemObjRelease(pTos);` |
|      10 | 2156 | `				pTos->nIdx = SXU32_HIGH;` |
|      10 | 2157 | `				if( bShouldArm ){` |
|       - | 2158 | `					/* Arm: remember (object, key) so NULLC_STORE dispatches` |
|       - | 2159 | `					 * to offsetSet. Hold a ref on the instance to survive` |
|       - | 2160 | `					 * intervening expression evaluation. */` |
|       8 | 2161 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       8 | 2162 | `					if( pIdx ){` |
|       8 | 2163 | `						PH7_MemObjStore(pIdx,&pVm->sCoalesceKey);` |
|       3 | 2164 | `					}` |
|       8 | 2165 | `					pVm->pCoalesceObj = pInst;` |
|       8 | 2166 | `					pInst->iRef++;` |
|       8 | 2167 | `					pVm->bCoalesceArmed = 1;` |
|       5 | 2168 | `				}else{` |
|       3 | 2169 | `					PH7_MemObjStore(&sValue,pTos);` |
|       - | 2170 | `				}` |
|      10 | 2171 | `				PH7_MemObjRelease(&sValue);` |
|      10 | 2172 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|      10 | 2173 | `				VM_EXIT_BREAK;` |
|     ! 0 | 2174 | `			}else{` |
|       - | 2175 | `				/* offsetGet: replace pTos with the returned value.` |
|       - | 2176 | `				 *` |
|       - | 2177 | `				 * The base slot may be the only thing holding this instance — a` |
|       - | 2178 | ``				 * TEMPORARY container (`f((new C)['a'])`, a getter's return) dies with`` |
|       - | 2179 | `				 * it — and everything below still speaks for the object: the writable` |
|       - | 2180 | `				 * test, php's notice and the read-modify-write arming all read its` |
|       - | 2181 | `				 * CLASS, and the deferred-argument carrier records it. Hold a reference` |
|       - | 2182 | `				 * of our own across the release so none of them is left reading freed` |
|       - | 2183 | `				 * memory. */` |
|     311 | 2184 | `				pInst->iRef++;` |
|     311 | 2185 | `				PH7_MemObjRelease(pTos);` |
|     311 | 2186 | `				PH7_MemObjStore(&sResult,pTos);` |
|     311 | 2187 | `				pTos->nIdx = SXU32_HIGH;` |
|     311 | 2188 | `				if( iP2 == 1 && VmNextIsCompoundAssign(pInstr + 1) ){` |
|       - | 2189 | ``					/* `$o[$k] op= v` is php's ASSIGN_DIM_OP: offsetGet gave the`` |
|       - | 2190 | `					 * current value, the op computes on it, and the result goes` |
|       - | 2191 | `					 * back through offsetSet($k, …). PHL had no write-back at` |
|       - | 2192 | `					 * all here — the fetched value carried no slot, so every` |
|       - | 2193 | `					 * compound assign on an ArrayAccess element died on` |
|       - | 2194 | `					 * "Cannot perform assignment on a constant class attribute"` |
|       - | 2195 | `					 * and stored nothing. Arm the scratch slot the op mutates;` |
|       - | 2196 | `					 * its tail (PH7_HOOK_RMW_WRITEBACK) dispatches offsetSet. */` |
|      64 | 2197 | `					VmDimRmwArm(&(*pVm),pInst,pIdx,pTos,` |
|      42 | 2198 | `						(void *)pStack,(void *)aInstr,(sxu32)(pc + 1));` |
|     286 | 2199 | `				}else if( VmIdxFetchForWrite(pInstr,iP2)` |
|     151 | 2200 | `				       && !PH7_VmDimFetchWritable(pInst->pClass) ){` |
|      25 | 2201 | `					PH7_VmOverloadedElemNotice(&(*pVm),pInst->pClass,pTos);` |
|     257 | 2202 | `				}else if( pInstr->iP2 == 9 ){` |
|       - | 2203 | `					/* A deferred call ARGUMENT. The accessor has just run — php runs it` |
|       - | 2204 | `					 * where the subscript is written, whatever the parameter turns out to` |
|       - | 2205 | `					 * be — but WHICH fetch php performed is the callee's to say, and only` |
|       - | 2206 | `					 * OP_CALL knows: a by-reference parameter makes it a W fetch, which on` |
|       - | 2207 | `					 * a container that can only answer with a VALUE is php's` |
|       - | 2208 | ``					 * `Indirect modification of overloaded element` notice and a write`` |
|       - | 2209 | `					 * thrown away. Carry the result plus the class that answered it, so the` |
|       - | 2210 | `					 * verdict lands at the call without the accessor running twice or the` |
|       - | 2211 | `					 * argument arriving as NULL. The value would otherwise reach the callee` |
|       - | 2212 | `					 * still SHARING the container's own nested map by COW, and a by-ref` |
|       - | 2213 | ``					 * `f($o['a']['b'])` wrote straight into the object php leaves untouched. */`` |
|      49 | 2214 | `					VmDeferredPath *pPre = VmDeferPathNewPrefetch(&(*pVm),VM_OVER_ELEM,pInst->pClass,0,pTos);` |
|      49 | 2215 | `					if( pPre ){` |
|      49 | 2216 | `						PH7_MemObjRelease(pTos);` |
|      49 | 2217 | `						pTos->x.pOther = pPre;` |
|      49 | 2218 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|      49 | 2219 | `						pTos->nIdx = SXU32_HIGH;` |
|      24 | 2220 | `					}` |
|      24 | 2221 | `				}` |
|     311 | 2222 | `				PH7_ClassInstanceUnref(pInst);` |
|       - | 2223 | `			}` |
|     501 | 2224 | `			PH7_MemObjRelease(&sResult);` |
|     501 | 2225 | `			if( pIdx ){` |
|     501 | 2226 | `				PH7_MemObjRelease(pIdx);` |
|     248 | 2227 | `			}` |
|     501 | 2228 | `			VM_EXIT_BREAK;` |
|       - | 2229 | `		}` |
|       - | 2230 | `		/* Object without ArrayAccess: PHP throws fatal Error in all subscript` |
|       - | 2231 | `		 * contexts (read, isset, unset, empty). Match it. A class carrying a` |
|       - | 2232 | `		 * READ handler reaches here only for the unset (iP2 5), which the hook` |
|       - | 2233 | `		 * branch above skips, and words that refusal itself. */` |
|      25 | 2234 | `		if( pInst && iP2 == 5 ){` |
|       - | 2235 | `			/* unset($o[$k]) on a handler that really removes something: php's` |
|       - | 2236 | `			 * SimpleXMLElement drops the attribute or the element. Offered the` |
|       - | 2237 | `			 * access before the refusal below. */` |
|       - | 2238 | `			PH7_NativeDimCtx sDim;` |
|      14 | 2239 | `			if( PH7_ClassNativeDimStore(pInst,PH7_NATIVE_DIM_UNSET,pIdx,0,&sDim)` |
|      13 | 2240 | `			 && sDim.zThrowClass == 0 ){` |
|       7 | 2241 | `				if( pIdx ){` |
|       7 | 2242 | `					PH7_MemObjRelease(pIdx);` |
|       3 | 2243 | `				}` |
|       7 | 2244 | `				PH7_MemObjRelease(pTos);` |
|       7 | 2245 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|       7 | 2246 | `				pTos->nIdx = SXU32_HIGH;` |
|       7 | 2247 | `				VM_EXIT_BREAK;` |
|       - | 2248 | `			}` |
|       4 | 2249 | `		}` |
|      19 | 2250 | `		if( pInst ){` |
|       - | 2251 | `			char zMsg[256];` |
|      27 | 2252 | `			sxu32 nMsg = PH7_ClassNativeDimRefusal(pInst,` |
|       8 | 2253 | `				iP2 == 5 ? PH7_NATIVE_DIM_UNSET : PH7_NATIVE_DIM_WRITE,` |
|       8 | 2254 | `				zMsg,sizeof(zMsg));` |
|      19 | 2255 | `			rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|      19 | 2256 | `			if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|      19 | 2257 | `			PH7_MemObjRelease(pTos);` |
|      19 | 2258 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      19 | 2259 | `			pTos->nIdx = SXU32_HIGH;` |
|      19 | 2260 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       - | 2261 | ``			/* `break` used to resume at the NEXT instruction: the catch ran and then`` |
|       - | 2262 | `			 * execution carried on inside the try block. */` |
|      21 | 2263 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2264 | `		}` |
|     ! 0 | 2265 | `	}` |
|  268670 | 2266 | `	if( (iP2 == 1 \|\| iP2 == 3 \|\| VM_IDX_IS_UNSET(iP2)) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       - | 2267 | `		{` |
|       - | 2268 | `			/* The base's TYPE decides here, whether or not there is a SLOT behind it.` |
|       - | 2269 | ``			 * A write THROUGH a temporary — `f()[0] = 5`, `f()[0][1] = 5` — gets the`` |
|       - | 2270 | `			 * same verdict from php; the only difference is that what it writes is` |
|       - | 2271 | `			 * discarded afterwards. PHL skipped the whole screen when the base had no` |
|       - | 2272 | ``			 * slot, so `ui()[0] += 5` over an int RESULT ran in silence where php`` |
|       - | 2273 | `			 * throws. The temporary is screened and vivified in place, on the stack —` |
|       - | 2274 | `			 * there is nowhere to write it back to. */` |
|     102 | 2275 | `			ph7_value *pObj = (pTos->nIdx != SXU32_HIGH)` |
|      60 | 2276 | `				? (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pTos->nIdx)` |
|      36 | 2277 | `				: pTos;` |
|      69 | 2278 | `			if( pObj != 0 ){` |
|       - | 2279 | `				/* php 8 write-context auto-vivify rules: NULL converts to array` |
|       - | 2280 | `				 * silently; FALSE converts with the 8.1 deprecation; any other` |
|       - | 2281 | `				 * scalar base — int/float/true/resource — is php's catchable` |
|       - | 2282 | `				 * "Cannot use a scalar value as an array" Error and the variable` |
|       - | 2283 | `				 * stays untouched (pre-fix the base was silently CONVERTED,` |
|       - | 2284 | ``				 * corrupting e.g. `$i = 5; $i[0]++` into array(0 => 6); string`` |
|       - | 2285 | `				 * bases were intercepted by the string-offset paths above). */` |
|       - | 2286 | `				/* php auto-converts false to an array with an 8.1 DEPRECATION; PHL` |
|       - | 2287 | `				 * rejects it like any other scalar base (null still auto-vivifies —` |
|       - | 2288 | `				 * it is not a bool). */` |
|      69 | 2289 | `				if( (pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_RES\|MEMOBJ_BOOL)) != 0 ){` |
|       - | 2290 | `					/* unset() has its own wording for the same base: php's` |
|       - | 2291 | `					 * "Cannot unset offset in a non-array variable". */` |
|      14 | 2292 | `					const char *zErr = VM_IDX_IS_UNSET(iP2)` |
|       - | 2293 | `						? "Cannot unset offset in a non-array variable"` |
|      12 | 2294 | `						: "Cannot use a scalar value as an array";` |
|       - | 2295 | `					SyBlob sErrMsg;` |
|      18 | 2296 | `					SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      18 | 2297 | `					SyBlobAppend(&sErrMsg,zErr,(sxu32)SyStrlen(zErr));` |
|      18 | 2298 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      18 | 2299 | `					if( pIdx ){` |
|      18 | 2300 | `						PH7_MemObjRelease(pIdx);` |
|       8 | 2301 | `					}` |
|      18 | 2302 | `					PH7_MemObjRelease(pTos);` |
|      18 | 2303 | `					pTos->nIdx = SXU32_HIGH;` |
|      18 | 2304 | `					VM_EXIT_BREAK;` |
|       - | 2305 | `				}` |
|       - | 2306 | `				/* unset() CREATES NOTHING: a null/undefined base stays null where PHL` |
|       - | 2307 | ``				 * converted it to an empty array (`$n = null; unset($n[0]);` left`` |
|       - | 2308 | `				 * $n === []), which is also what materialised the missing intermediate` |
|       - | 2309 | ``				 * in `unset($a["y"]["z"])`. Leaving the base alone, the lookup below`` |
|       - | 2310 | `				 * misses, the tail loads NULL with no slot index, and the trailing` |
|       - | 2311 | `				 * unset() builtin is the no-op php's is. */` |
|      53 | 2312 | `				if( !VM_IDX_IS_UNSET(iP2) ){` |
|      47 | 2313 | `					PH7_MemObjToHashmap(pObj);` |
|      47 | 2314 | `					if( pObj != pTos ){` |
|      47 | 2315 | `						PH7_MemObjLoad(pObj,pTos);` |
|      22 | 2316 | `					}` |
|      22 | 2317 | `				}` |
|      25 | 2318 | `			}` |
|       - | 2319 | `		}` |
|      25 | 2320 | `	}` |
|  268654 | 2321 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|       - | 2322 | `	/* php DEPRECATES both a null offset and a lossy-float subscript (then normalizes` |
|       - | 2323 | `	 * "" / truncates). PHL now matches php on the NULL offset (deprecate + coerce to` |
|       - | 2324 | `	 * the "" key) but still rejects the lossy-FLOAT subscript with a TypeError on` |
|       - | 2325 | `	 * a READ or WRITE (iP2 0/1) — the recorded non-deprecated-surface policy. Both` |
|       - | 2326 | ``	 * stay lenient in isset()/empty()/`??`/unset(), where a throw would be wrong. */`` |
|       - | 2327 | `	/* An object/array key is rejected in EVERY context, including isset()/empty()/` |
|       - | 2328 | `	 * unset() where php still throws (only the wording changes) — unlike the` |
|       - | 2329 | `	 * null/float deprecations below, which stay lenient there. A resource key is` |
|       - | 2330 | `	 * accepted with a warning and becomes its integer id. */` |
|  268654 | 2331 | `	if( (pTos->iFlags & MEMOBJ_HASHMAP) && pIdx ){` |
|       - | 2332 | `		SyBlob sTypeMsg;` |
|  268522 | 2333 | `		if( VmOffsetTypeRejected(&(*pVm),pIdx,iP2,&sTypeMsg) ){` |
|       - | 2334 | `			/* Routed as a mid-expression throw, like the STRING arm above: this` |
|       - | 2335 | `			 * opcode is not a call boundary, so PARKING it let the rest of the` |
|       - | 2336 | ``			 * expression run first — `str_repeat($a[$obj], 2)` reached the builtin`` |
|       - | 2337 | `			 * with the NULL the abandoned read left and died on its ZPP TypeError` |
|       - | 2338 | `			 * instead, and a CAUGHT one still ran the enclosing call afterwards. */` |
|      22 | 2339 | `			rc = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|      22 | 2340 | `			PH7_MemObjRelease(pIdx);` |
|      22 | 2341 | `			PH7_MemObjRelease(pTos);` |
|      22 | 2342 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      22 | 2343 | `			pTos->nIdx = SXU32_HIGH;` |
|      31 | 2344 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      20 | 2345 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2346 | `		}` |
|  268502 | 2347 | `		PH7_VmOffsetResourceWarn(&(*pVm),pIdx);` |
|  134187 | 2348 | `	}` |
|  268634 | 2349 | `	if( (pTos->iFlags & MEMOBJ_HASHMAP) && pIdx ){` |
|       - | 2350 | `		/* php DEPRECATES a null offset in EVERY subscript context except unset()` |
|       - | 2351 | `		 * (iP2==5), then normalizes it to the "" key — read, write, isset (4),` |
|       - | 2352 | ``		 * empty (6), `??`/`??=` (3), the coalesce-chain probe (8), destructure`` |
|       - | 2353 | `		 * (2/7). Emit it here so all of them get it, not just read/write; the` |
|       - | 2354 | `		 * lookup/insert below casts NULL->"", and a plain read miss then warns` |
|       - | 2355 | ``		 * `Undefined array key ""` on the now-string pIdx (php's exact pair). */`` |
|  268502 | 2356 | `		if( (pIdx->iFlags & MEMOBJ_NULL) && !VM_IDX_IS_UNSET(iP2) ){` |
|      21 | 2357 | `			PH7_VmNullOffsetDeprecate(&(*pVm),pIdx);` |
|       9 | 2358 | `		}` |
|       - | 2359 | `		/* A lossy-FLOAT subscript stays a rejected TypeError on a READ or WRITE` |
|       - | 2360 | `		 * (iP2 0/1) — the recorded non-deprecated-surface policy; the lenient` |
|       - | 2361 | `		 * contexts (isset/empty/??/unset) truncate quietly as php's value does. */` |
|  268497 | 2362 | `		if( (iP2 == 0 \|\| iP2 == 1)` |
|  182246 | 2363 | `		 && (pIdx->iFlags & MEMOBJ_REAL)` |
|  134197 | 2364 | `		 && pIdx->rVal != (ph7_real)(sxi64)pIdx->rVal ){` |
|       - | 2365 | `			SyBlob sErrMsg;` |
|       6 | 2366 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       6 | 2367 | `			SyBlobAppend(&sErrMsg,"Cannot access offset of type float on array",` |
|       - | 2368 | `				sizeof("Cannot access offset of type float on array")-1);` |
|       6 | 2369 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|       6 | 2370 | `			PH7_MemObjRelease(pIdx);` |
|       6 | 2371 | `			PH7_MemObjRelease(pTos);` |
|       6 | 2372 | `			pTos->nIdx = SXU32_HIGH;` |
|       6 | 2373 | `			VM_EXIT_BREAK;` |
|       - | 2374 | `		}` |
|  134185 | 2375 | `	}` |
|  268630 | 2376 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|  268526 | 2377 | `		if( iP2 == 1 \|\| VM_IDX_IS_UNSET(iP2) ){` |
|       - | 2378 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|       - | 2379 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|       - | 2380 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|       - | 2381 | `			 * NOT separate — that would defeat COW on every element read.` |
|       - | 2382 | `			 * iP2=5 is unset-context, treated like iP2=1 for arrays so the` |
|       - | 2383 | `			 * trailing unset() builtin can drop the slot via pTos->nIdx. */` |
|    7211 | 2384 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|    3607 | 2385 | `		}` |
|       - | 2386 | `		/* Point to the hashmap */` |
|  268526 | 2387 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|  268526 | 2388 | `		if( pIdx ){` |
|       - | 2389 | `			/* Load the desired entry */` |
|  268498 | 2390 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|  134185 | 2391 | `		}` |
|  268526 | 2392 | `		if( iP2 == 3 ){` |
|       - | 2393 | `			/* Null coalescing assign peek mode: separate only when we will` |
|       - | 2394 | `			 * actually write back. If the looked-up value is non-null, the` |
|       - | 2395 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|       - | 2396 | `			 * the parent can stay shared. If the value is null or the key is` |
|       - | 2397 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|       - | 2398 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|       - | 2399 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|       - | 2400 | `			 * correct for the outermost write. */` |
|      27 | 2401 | `			int needWrite = (rc != SXRET_OK);` |
|      27 | 2402 | `			if( !needWrite && pNode ){` |
|      13 | 2403 | `				ph7_value *pVal = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pNode->nValIdx);` |
|      13 | 2404 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|       7 | 2405 | `					needWrite = 1;` |
|       3 | 2406 | `				}` |
|       6 | 2407 | `			}` |
|      27 | 2408 | `			if( needWrite ){` |
|      21 | 2409 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      21 | 2410 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|       - | 2411 | `					/* The map was actually copied — re-lookup so pNode points` |
|       - | 2412 | `					 * into the new map's storage. */` |
|       7 | 2413 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       7 | 2414 | `					if( pIdx ){` |
|       7 | 2415 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|       3 | 2416 | `					}` |
|       3 | 2417 | `				}` |
|      10 | 2418 | `			}` |
|      13 | 2419 | `		}` |
|       - | 2420 | `		/* iP2 == 5 (unset) is deliberately NOT here: php's unset() never creates` |
|       - | 2421 | `		 * the key it is about to remove, so a MISS must stay a miss. The` |
|       - | 2422 | `		 * insert-then-unset round trip was invisible on the LAST step but left` |
|       - | 2423 | ``		 * every INTERMEDIATE behind -- `unset($a["y"]["z"])` grew an empty`` |
|       - | 2424 | `		 * $a["y"]. The COW separation the unset context needs happened above and` |
|       - | 2425 | `		 * does not depend on this insert. */` |
|  268526 | 2426 | `		if( bRmwFetch && rc != SXRET_OK ){` |
|       - | 2427 | `			/* php's read half missed. Record it BEFORE the vivification below` |
|       - | 2428 | `			 * overwrites rc — the warning is about what was not there to read, and` |
|       - | 2429 | `			 * it is emitted after the slot exists, exactly as php does it. */` |
|      45 | 2430 | `			bRmwMiss = 1;` |
|      22 | 2431 | `		}` |
|  268526 | 2432 | `		if( rc != SXRET_OK && (iP2 == 1 \|\| iP2 == 3) ){` |
|       - | 2433 | `			/* Create a new empty entry */` |
|     215 | 2434 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|     215 | 2435 | `			if( rc == SXRET_OK ){` |
|       - | 2436 | `				/* Point to the last inserted entry */` |
|     212 | 2437 | `				pNode = pMap->pLast;` |
|     108 | 2438 | `			}else{` |
|       - | 2439 | ``				/* An append lvalue (`$a[][...] = v`) whose saturated auto-index`` |
|       - | 2440 | `				 * is occupied threw php's catchable Error. Dispatch it here —` |
|       - | 2441 | `				 * falling through with a stale pMap->pLast is what silently` |
|       - | 2442 | `				 * overwrote $a[PHP_INT_MAX]. */` |
|       7 | 2443 | `				PH7_DISPATCH_ENFORCE_RC(rc)` |
|       - | 2444 | `			}` |
|     104 | 2445 | `		}` |
|  134198 | 2446 | `	}` |
|  268623 | 2447 | `	if( pIdx && (bRmwMiss \|\| (rc != SXRET_OK && (iP2 == 2 \|\| iP2 == 7 \|\| iP2 == 0)))` |
|   65760 | 2448 | `	 && (pTos->iFlags & MEMOBJ_HASHMAP)` |
|      96 | 2449 | `	 && !VmIdxFeedsCoalesce(pInstr) ){` |
|       - | 2450 | ``		/* `$a['k'] ?? $d` compiles its LHS as a plain read (iP2 == 0) followed`` |
|       - | 2451 | `		 * by NULLC/NULLC_JMP — php does NOT warn there, so peek ahead and stay` |
|       - | 2452 | `		 * silent (same guard the magic-accessor read path uses). */` |
|       - | 2453 | `		/* php warns when a missing key is READ (iP2 == 0), destructured -- both` |
|       - | 2454 | `		 * positionally (iP2 == 2) and by KEY (iP2 == 7, which used to stay silent` |
|       - | 2455 | ``		 * where php says `Undefined array key "k"` for `['k' => $v] = []`) -- or`` |
|       - | 2456 | `		 * read by the READ half of a read-modify-write (bRmwMiss).` |
|       - | 2457 | `		 * isset/empty/??/unset (iP2 3-6) and plain write-context` |
|       - | 2458 | `		 * vivification (iP2 == 1) stay silent, as does a read on a non-array` |
|       - | 2459 | `		 * base (already diagnosed above). php prints an INT key bare and a` |
|       - | 2460 | `		 * STRING key quoted, and it decides which one the key IS by the same fold` |
|       - | 2461 | ``		 * the lookup just used: `$a["10"]` looked for the INTEGER key 10, so php`` |
|       - | 2462 | ``		 * says `Undefined array key 10`. PHL rendered the operand as WRITTEN --`` |
|       - | 2463 | ``		 * quoting every string and, worse, printing `$a[false]` as the "" key it`` |
|       - | 2464 | `		 * never looked in (false is the integer key 0). The canonical-numeric rule` |
|       - | 2465 | `		 * is the hashmap's own, so ask it rather than re-derive it. */` |
|       - | 2466 | `		SyBlob sMsg;` |
|     143 | 2467 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     143 | 2468 | `		if( (ph7_hashmap *)pTos->x.pOther == pVm->pGlobal ){` |
|       - | 2469 | `			/* $GLOBALS is the symbol table, so a key that is not there is a VARIABLE` |
|       - | 2470 | ``			 * that is not there, and php says so: `Undefined global variable $x`,`` |
|       - | 2471 | ``			 * with the subscript spelled RAW after the `$` ($GLOBALS[5] reads`` |
|       - | 2472 | ``			 * `$5`) rather than folded and quoted the way an array key is. Only the`` |
|       - | 2473 | `			 * LIVE map takes this wording — a copy of $GLOBALS is a by-value` |
|       - | 2474 | `			 * snapshot (memobj.c) and warns as the ordinary array it is. */` |
|       - | 2475 | `			SyString sName;` |
|       5 | 2476 | `			if( (pIdx->iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 2477 | `				PH7_MemObjToString(pIdx);` |
|     ! 0 | 2478 | `			}` |
|       5 | 2479 | `			SyStringInitFromBuf(&sName,SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob));` |
|       5 | 2480 | `			SyBlobFormat(&sMsg,"Undefined global variable $%z",&sName);` |
|     141 | 2481 | `		}else if( PH7_HashmapKeyIsInt(pIdx) ){` |
|      48 | 2482 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      15 | 2483 | `				PH7_MemObjToInteger(pIdx);` |
|       7 | 2484 | `			}` |
|      48 | 2485 | `			SyBlobFormat(&sMsg,"Undefined array key %qd",pIdx->x.iVal);` |
|      25 | 2486 | `		}else{` |
|       - | 2487 | `			/* Not an integer key: PH7_HashmapKeyIsInt left it a printable string. */` |
|       - | 2488 | `			SyString sKey;` |
|      93 | 2489 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob));` |
|      93 | 2490 | `			SyBlobFormat(&sMsg,"Undefined array key \"%z\"",&sKey);` |
|       - | 2491 | `		}` |
|     143 | 2492 | `		SyBlobNullAppend(&sMsg);` |
|     143 | 2493 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|     143 | 2494 | `		SyBlobRelease(&sMsg);` |
|      69 | 2495 | `	}` |
|  268717 | 2496 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0` |
|  134396 | 2497 | `	 && iP2 == 0` |
|      30 | 2498 | `	 && !VmIdxFeedsCoalesce(pInstr) ){` |
|       - | 2499 | `		/* Subscripting a scalar base is a WARNING in php ("Trying to access array offset` |
|       - | 2500 | `		 * on int") that yields NULL. PH7 yielded NULL in silence. (NOT for iP2 == 2, the` |
|       - | 2501 | `		 * fetch a NESTED destructuring level makes: php never subscripts there -- its` |
|       - | 2502 | `		 * outer list already answered that position with NULL and warned once for it --` |
|       - | 2503 | `		 * so a second sentence about the same byte is one php does not print.) */` |
|      23 | 2504 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Trying to access array offset on %s",` |
|       7 | 2505 | `			VmArithValueName(pTos));` |
|       7 | 2506 | `	}` |
|  268555 | 2507 | `	if( iP2 == VM_IDX_CTX_UNSET && rc == SXRET_OK && pNode != 0` |
|    1137 | 2508 | `	 && (pTos->iFlags & MEMOBJ_HASHMAP) ){` |
|       - | 2509 | ``		/* php's `unset($a[k])` removes the ELEMENT. PH7 left the element's value slot`` |
|       - | 2510 | `		 * on the stack and let the trailing unset() builtin drop it — but dropping a` |
|       - | 2511 | `` 		 * SLOT unlinks everything that holds it, so `$r = &$a['k']; unset($a['k']);` `` |
|       - | 2512 | ``		 * destroyed $r too (`Undefined variable $r`) where php leaves it reading the`` |
|       - | 2513 | `		 * value it still refers to. Unlink the node itself, which releases the value` |
|       - | 2514 | `		 * only when this element was its last holder, and leave the builtin nothing. */` |
|    1137 | 2515 | `		ph7_hashmap *pTarget = (ph7_hashmap *)pTos->x.pOther;` |
|    1137 | 2516 | `		int bDone = 0;` |
|    1137 | 2517 | `		if( pTarget == pVm->pGlobal && pIdx ){` |
|       - | 2518 | ``			/* `$GLOBALS['x']` IS the global $x, so this is an unset of the NAME: it has`` |
|       - | 2519 | `			 * to drop the symbol-table entry as well as this node, and it must not` |
|       - | 2520 | `			 * destroy the value another holder still refers to — exactly what` |
|       - | 2521 | ``			 * VmUnsetVarByName does for `unset($x)`. A superglobal has no entry in the`` |
|       - | 2522 | `			 * global frame and falls through to the plain node unlink below. */` |
|     161 | 2523 | `			VmFrame *pGlobalFrame = pVm->pFrame;` |
|       - | 2524 | `			SyHashEntry *pNameEntry;` |
|     161 | 2525 | `			while( pGlobalFrame->pParent ){` |
|     ! 0 | 2526 | `				pGlobalFrame = pGlobalFrame->pParent;` |
|     ! 0 | 2527 | `			}` |
|     161 | 2528 | `			if( (pIdx->iFlags & MEMOBJ_STRING) == 0 ){` |
|       3 | 2529 | `				PH7_MemObjToString(pIdx);` |
|       1 | 2530 | `			}` |
|     241 | 2531 | `			pNameEntry = SyHashGet(&pGlobalFrame->hVar,` |
|     160 | 2532 | `				(const void *)SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob));` |
|     161 | 2533 | `			if( pNameEntry ){` |
|     241 | 2534 | `				sxi32 rcUnset = VmUnsetVarByNameEx(&(*pVm),pGlobalFrame,` |
|     160 | 2535 | `					(const char *)SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob),FALSE);` |
|     161 | 2536 | `				bDone = 1;` |
|     161 | 2537 | `				if( rcUnset == PH7_ABORT ){` |
|     ! 0 | 2538 | `					PH7_MemObjRelease(pIdx);` |
|     ! 0 | 2539 | `					VM_EXIT_ABORT;` |
|       - | 2540 | `				}` |
|      80 | 2541 | `			}` |
|      80 | 2542 | `		}` |
|    1137 | 2543 | `		if( !bDone ){` |
|     977 | 2544 | `			PH7_HashmapUnlinkNode(pNode,TRUE);` |
|     486 | 2545 | `		}` |
|    1137 | 2546 | `		if( pIdx ){` |
|    1137 | 2547 | `			PH7_MemObjRelease(pIdx);` |
|     566 | 2548 | `		}` |
|    1137 | 2549 | `		PH7_MemObjRelease(pTos);` |
|    1137 | 2550 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|    1137 | 2551 | `		pTos->nIdx = SXU32_HIGH;` |
|    1137 | 2552 | `		VM_EXIT_BREAK;` |
|       - | 2553 | `	}` |
|  267428 | 2554 | `	if( pIdx ){` |
|  267402 | 2555 | `		PH7_MemObjRelease(pIdx);` |
|  133637 | 2556 | `	}` |
|  267428 | 2557 | `	if( rc == SXRET_OK ){` |
|       - | 2558 | `		/* Load entry contents */` |
|  136052 | 2559 | `		if( pMap->iRef < 2 ){` |
|       - | 2560 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|       - | 2561 | `			 * of the entry value,rather than pointing to it.` |
|       - | 2562 | `			 */` |
|    1004 | 2563 | `			pTos->nIdx = SXU32_HIGH;` |
|    1004 | 2564 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|     502 | 2565 | `		}else{` |
|  135053 | 2566 | `			pTos->nIdx = pNode->nValIdx;` |
|  135053 | 2567 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|  135053 | 2568 | `			PH7_HashmapUnref(pMap);` |
|       - | 2569 | `		}` |
|   67991 | 2570 | `	}else{` |
|       - | 2571 | `		/* No such entry,load NULL */` |
|  131381 | 2572 | `		PH7_MemObjRelease(pTos);` |
|  131381 | 2573 | `		pTos->nIdx = SXU32_HIGH;` |
|       - | 2574 | `	}` |
|  267428 | 2575 | `	if( iP2 == 4 && (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       - | 2576 | `		/* isset() context: reduce a found element to the same non-null marker the` |
|       - | 2577 | `		 * ArrayAccess arm above pushes. A TEMPORARY array (a call's return value,` |
|       - | 2578 | ``		 * or an accessor's -- `isset(f()['k'])`, `isset($o->magic['k'])`) leaves no`` |
|       - | 2579 | `		 * variable index behind, and the trailing builtin read that as a CONSTANT` |
|       - | 2580 | `		 * and warned about it; php's isset() is a language construct with no such` |
|       - | 2581 | `		 * diagnostic. */` |
|   38281 | 2582 | `		PH7_MemObjRelease(pTos);` |
|   38281 | 2583 | `		pTos->x.iVal = 1;` |
|   38281 | 2584 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   38281 | 2585 | `		pTos->nIdx = SXU32_HIGH;` |
|   19134 | 2586 | `	}` |
|  267428 | 2587 | `	VM_EXIT_BREAK;` |
|     ! 0 | 2588 | `	VM_EXIT_BREAK;` |
|  705532 | 2589 | `}` |
|       - | 2590 |  |
|       - | 2591 | `/*` |
|       - | 2592 | ` * OP_LOAD_MAP: body moved verbatim from the OP_LOAD_MAP arm of` |
|       - | 2593 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 2594 | ` */` |
|  301770 | 2595 | `PH7_PRIVATE VmOpRc VmExecOpLoadMap(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 2596 | `{` |
|  301775 | 2597 | `	ph7_value *pTos = pState->pTos;` |
|  301775 | 2598 | `	ph7_value *pStack = pState->pStack;` |
|  301775 | 2599 | `	VmInstr *aInstr = pState->aInstr;` |
|  301775 | 2600 | `	sxi32 pc = pState->pc;` |
|       - | 2601 | `	sxi32 rc;` |
|  150813 | 2602 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - | 2603 | `	ph7_hashmap *pMap;` |
|       - | 2604 | `	/* Allocate a new hashmap instance */` |
|  301775 | 2605 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|  301775 | 2606 | `	if( pMap == 0 ){` |
|     ! 0 | 2607 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|     ! 0 | 2608 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|     ! 0 | 2609 | `		VM_EXIT_ABORT;` |
|       - | 2610 | `	}` |
|  301775 | 2611 | `	if( pInstr->iP1 > 0 ){` |
|  199603 | 2612 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|  199603 | 2613 | `		sxi32 rcSpread = SXRET_OK;` |
|       - | 2614 | `		/* Perform the insertion */` |
|  460749 | 2615 | `		while( pEntry < pTos ){` |
|  261197 | 2616 | `			if( pEntry[1].iFlags & MEMOBJ_AUX_SPREAD ){` |
|       - | 2617 | `				/* Array unpacking: '...$expr'. Merge entries with PHP 8.1` |
|       - | 2618 | `				 * semantics — string keys preserved (later wins), int keys` |
|       - | 2619 | `				 * renumbered. Same routine that backs array_merge. */` |
|     733 | 2620 | `				if( pEntry[1].iFlags & MEMOBJ_HASHMAP ){` |
|     671 | 2621 | `					sxi32 rcMerge = PH7_HashmapMerge((ph7_hashmap *)pEntry[1].x.pOther,pMap);` |
|     671 | 2622 | `					if( rcMerge != SXRET_OK ){` |
|       - | 2623 | `						/* Merge failure (OOM): match the PH7_NewHashmap OOM` |
|       - | 2624 | `						 * path — emit fatal and abort, leaving no partial` |
|       - | 2625 | `						 * map dangling. */` |
|     ! 0 | 2626 | `						VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|     ! 0 | 2627 | `							"Fatal, PH7 engine is running out of memory while spreading array at instruction #:%d",pc);` |
|     ! 0 | 2628 | `						rcSpread = PH7_ABORT;` |
|     ! 0 | 2629 | `						break;` |
|       3 | 2630 | `					}` |
|     399 | 2631 | `				}else if( VmValueIsTraversable(pVm,&pEntry[1]) ){` |
|       - | 2632 | `					/* Traversable unpacking (PHP 8.1): walk it into the map using the` |
|       - | 2633 | `					 * same key rules as array spread (string keys kept, int renumbered). */` |
|      32 | 2634 | `					sxi32 rcW = PH7_VmIteratorWalk(&(*pVm),&pEntry[1],VmSpreadMergeStep,pMap);` |
|      32 | 2635 | `					if( rcW == PH7_EXCEPTION \|\| rcW == PH7_ABORT ){` |
|      18 | 2636 | `						rcSpread = rcW;` |
|      18 | 2637 | `						break;` |
|       - | 2638 | `					}` |
|       9 | 2639 | `				}else{` |
|       - | 2640 | `					/* Throw a catchable Error matching PHP semantics. */` |
|      34 | 2641 | `					rcSpread = VmThrowSpreadError(&(*pVm),&pEntry[1],0);` |
|      34 | 2642 | `					break;` |
|       3 | 2643 | `				}` |
|  260810 | 2644 | `			}else if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|       - | 2645 | `				/* Insertion by reference */` |
|     296 | 2646 | `				PH7_HashmapInsertByRef(pMap,` |
|     196 | 2647 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     196 | 2648 | `					(sxu32)pEntry[1].x.iVal` |
|       - | 2649 | `					);` |
|     100 | 2650 | `			}else{` |
|       - | 2651 | `				/* An explicit key in an array LITERAL gets the same php diagnostics a` |
|       - | 2652 | `				 * subscript does — a float key that truncates deprecates, and so does an` |
|       - | 2653 | `				 * explicit null key. Only the subscript sites (LOAD_IDX/STORE_IDX) used to` |
|       - | 2654 | ``				 * emit these, so `[1.5 => "v"]` and `[null => "v"]` were silent. A key that`` |
|       - | 2655 | `				 * is ABSENT (auto-index) is a NULL slot here, not a null key, so the` |
|       - | 2656 | `				 * MEMOBJ_NULL check below is what tells the two apart. */` |
|  260273 | 2657 | `					if( (pEntry->iFlags & MEMOBJ_AUX_NOKEY) == 0 ){` |
|       - | 2658 | `						/* An object/array literal key is php's TypeError, a resource one` |
|       - | 2659 | `						 * warns and becomes its id — same rules as a subscript. */` |
|       - | 2660 | `						SyBlob sTypeMsg;` |
|  182837 | 2661 | `						if( VmOffsetTypeRejected(&(*pVm),pEntry,0,&sTypeMsg) ){` |
|       3 | 2662 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg));` |
|       2 | 2663 | `						}else{` |
|  182835 | 2664 | `							PH7_VmOffsetResourceWarn(&(*pVm),pEntry);` |
|       - | 2665 | `						}` |
|       - | 2666 | `						/* php DEPRECATES a null literal key (then normalizes to "") and` |
|       - | 2667 | `						 * rejects nothing there; PHL matches that (deprecate + fall` |
|       - | 2668 | `						 * through, PH7_HashmapInsert casts NULL->""). A lossy-FLOAT` |
|       - | 2669 | `						 * literal key still rejects with a TypeError — the recorded` |
|       - | 2670 | `						 * non-deprecated-surface policy, same as the subscript site. */` |
|  182837 | 2671 | `						int bNull = (pEntry->iFlags & MEMOBJ_NULL) != 0;` |
|  274284 | 2672 | `						int bLossyFloat = (pEntry->iFlags & MEMOBJ_REAL) != 0` |
|  182832 | 2673 | `							&& pEntry->rVal != (ph7_real)(sxi64)pEntry->rVal;` |
|  182837 | 2674 | `						if( bNull ){` |
|       3 | 2675 | `							PH7_VmNullOffsetDeprecate(&(*pVm),pEntry);` |
|  182836 | 2676 | `						}else if( bLossyFloat ){` |
|       3 | 2677 | `							const char *zErr = "Cannot access offset of type float on array";` |
|       - | 2678 | `							SyBlob sErrMsg;` |
|       3 | 2679 | `							SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       3 | 2680 | `							SyBlobAppend(&sErrMsg,zErr,(sxu32)SyStrlen(zErr));` |
|       3 | 2681 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|       1 | 2682 | `						}` |
|   91386 | 2683 | `					}` |
|       - | 2684 | `				/* Standard insertion */` |
|  390076 | 2685 | `				PH7_HashmapInsert(pMap,` |
|  260268 | 2686 | `					(pEntry->iFlags & MEMOBJ_AUX_NOKEY) ? 0 /* Automatic index assign */ : pEntry,` |
|  129803 | 2687 | `					&pEntry[1]` |
|       - | 2688 | `				);` |
|       - | 2689 | `			}` |
|       - | 2690 | `			/* Next pair on the stack */` |
|  261151 | 2691 | `			pEntry += 2;` |
|       5 | 2692 | `		}` |
|       - | 2693 | `		/* Pop P1 elements */` |
|  199603 | 2694 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|  199603 | 2695 | `		if( rcSpread != SXRET_OK ){` |
|       - | 2696 | `			/* Discard the partially-built map and propagate the exception. */` |
|      51 | 2697 | `			PH7_HashmapRelease(pMap,TRUE);` |
|      51 | 2698 | `			if( rcSpread == PH7_ABORT ){` |
|     ! 0 | 2699 | `				VM_EXIT_ABORT;` |
|       - | 2700 | `			}` |
|       - | 2701 | `			{` |
|       - | 2702 | `				sxi32 iRp;` |
|      51 | 2703 | `				if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|       6 | 2704 | `					pc = iRp;` |
|       6 | 2705 | `					VM_EXIT_BREAK;` |
|       - | 2706 | `				}` |
|       - | 2707 | `			}` |
|      46 | 2708 | `			VM_EXIT_EXCEPTION;` |
|       - | 2709 | `		}` |
|   99716 | 2710 | `	}` |
|       - | 2711 | `	/* Push the hashmap */` |
|  301729 | 2712 | `	pTos++;` |
|  301729 | 2713 | `	pTos->nIdx = SXU32_HIGH;` |
|  301729 | 2714 | `	pTos->x.pOther = pMap;` |
|  301729 | 2715 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|  301729 | 2716 | `	VM_EXIT_BREAK;` |
|     ! 0 | 2717 | `	VM_EXIT_BREAK;` |
|  150818 | 2718 | `}` |
|       - | 2719 |  |
|       - | 2720 | `/*` |
|       - | 2721 | `` * Can `$o[$k]` be READ at all? php's read_dimension is either the class's own`` |
|       - | 2722 | ` * native handler -- which a class may carry WITHOUT implementing ArrayAccess,` |
|       - | 2723 | ` * php's DOMNodeList -- or the standard one, which needs the interface. Neither` |
|       - | 2724 | `` * is php's `Cannot use object of type C as array`.`` |
|       - | 2725 | ` */` |
|      30 | 2726 | `static int VmObjectDimReadable(ph7_vm *pVm,ph7_class_instance *pInst)` |
|       1 | 2727 | `{` |
|      31 | 2728 | `	if( pInst == 0 ){` |
|     ! 0 | 2729 | `		return 0;` |
|       - | 2730 | `	}` |
|      45 | 2731 | `	return PH7_ClassHasNativeDim(pInst->pClass)` |
|      30 | 2732 | `	    \|\| (pVm->pArrayAccessClass && PH7_VmInstanceOf(pInst->pClass,pVm->pArrayAccessClass));` |
|      16 | 2733 | `}` |
|       - | 2734 | `/*` |
|       - | 2735 | ` * One such READ, into pOut (which the caller inits and owns). The native` |
|       - | 2736 | ` * handler comes first for the same reason it does at the subscript opcode: php` |
|       - | 2737 | ` * implements the interface THROUGH the handler. A refusal is dropped here --` |
|       - | 2738 | ` * the only caller indexes 0..N-1 of its own target list, which no handler` |
|       - | 2739 | ` * refuses -- and pOut is simply left as it was.` |
|       - | 2740 | ` *` |
|       - | 2741 | ` * Answers the accessor's own status so a caller reading a RUN of positions can` |
|       - | 2742 | ` * stop where php stops: a userland offsetGet that THROWS abandons the rest of` |
|       - | 2743 | ` * the destructure, leaving every later target at its previous value.` |
|       - | 2744 | ` */` |
|      36 | 2745 | `static sxi32 VmObjectDimRead(ph7_vm *pVm,ph7_class_instance *pInst,ph7_value *pKey,ph7_value *pOut)` |
|       1 | 2746 | `{` |
|       - | 2747 | `	ph7_class_method *pGet;` |
|       - | 2748 | `	sxi32 rcCall;` |
|      37 | 2749 | `	if( PH7_ClassHasNativeDim(pInst->pClass) ){` |
|       - | 2750 | `		PH7_NativeDimCtx sDim;` |
|       5 | 2751 | `		sDim.iMode = PH7_NATIVE_DIM_READ;` |
|       5 | 2752 | `		sDim.pOffset = pKey;` |
|       5 | 2753 | `		sDim.pResult = pOut;` |
|       5 | 2754 | `		sDim.zThrowClass = 0;` |
|       5 | 2755 | `		sDim.zThrowMsg[0] = 0;` |
|       5 | 2756 | `		sDim.bStored = 0;` |
|       5 | 2757 | `		PH7_ClassNativeDim(pInst,&sDim);` |
|       5 | 2758 | `		return SXRET_OK;` |
|       - | 2759 | `	}` |
|      33 | 2760 | `	pGet = PH7_ClassExtractMethod(pInst->pClass,"offsetGet",sizeof("offsetGet")-1);` |
|      33 | 2761 | `	if( pGet == 0 ){` |
|     ! 0 | 2762 | `		return SXRET_OK;` |
|       - | 2763 | `	}` |
|       - | 2764 | `	{` |
|       - | 2765 | `		ph7_value *apArg[1];` |
|      33 | 2766 | `		apArg[0] = pKey;` |
|      33 | 2767 | `		rcCall = PH7_VmCallClassMethod(&(*pVm),pInst,pGet,pOut,1,apArg);` |
|       - | 2768 | `	}` |
|      33 | 2769 | `	return (rcCall == PH7_EXCEPTION \|\| pVm->nBoundaryRc != 0) ? PH7_EXCEPTION : SXRET_OK;` |
|      19 | 2770 | `}` |
|       - | 2771 | `/*` |
|       - | 2772 | ` * OP_LOAD_LIST: body moved verbatim from the OP_LOAD_LIST arm of` |
|       - | 2773 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 2774 | ` */` |
|  393163 | 2775 | `PH7_PRIVATE VmOpRc VmExecOpLoadList(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 2776 | `{` |
|  393168 | 2777 | `	ph7_value *pTos = pState->pTos;` |
|  393168 | 2778 | `	ph7_value *pStack = pState->pStack;` |
|  393168 | 2779 | `	VmInstr *aInstr = pState->aInstr;` |
|  393168 | 2780 | `	sxi32 pc = pState->pc;` |
|       - | 2781 | `	sxi32 rc;` |
|  196578 | 2782 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - | 2783 | `	ph7_value *pEntry;` |
|  393168 | 2784 | `	sxi32 rcEnforce = SXRET_OK;` |
|  393168 | 2785 | `	if( pInstr->iP1 <= 0 ){` |
|       - | 2786 | `		/* Empty list,break immediately */` |
|     ! 0 | 2787 | `		VM_EXIT_BREAK;` |
|       - | 2788 | `	}` |
|  393168 | 2789 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|       - | 2790 | `#ifdef UNTRUST` |
|       - | 2791 | `	if( &pEntry[-1] < pStack ){` |
|       - | 2792 | `		VM_EXIT_ABORT;` |
|       - | 2793 | `	}` |
|       - | 2794 | `#endif` |
|  393168 | 2795 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|  393104 | 2796 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|       - | 2797 | `		ph7_hashmap_node *pNode;` |
|       - | 2798 | `		ph7_value sKey,*pObj;` |
|       - | 2799 | `		/* Start Copying */` |
|  393104 | 2800 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
| 1569754 | 2801 | `		while( pEntry <= pTos ){` |
| 1176673 | 2802 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
| 1176643 | 2803 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
| 1176643 | 2804 | `				if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
| 1176643 | 2805 | `					int bTyped = PH7_VM_STORE_FILTERED(pVm,pEntry->nIdx);` |
| 1176643 | 2806 | `					if( rc != SXRET_OK ){` |
|       - | 2807 | `						/* Undefined array key */` |
|       - | 2808 | `						char zMsg[128];` |
|       8 | 2809 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|       8 | 2810 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|       3 | 2811 | `					}` |
| 1176643 | 2812 | `					if( !bTyped ){` |
| 1176607 | 2813 | `						if( rc == SXRET_OK ){` |
|       - | 2814 | `							/* Store node value */` |
| 1176605 | 2815 | `							PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|  588298 | 2816 | `						}else{` |
|       3 | 2817 | `							PH7_MemObjRelease(pObj);` |
|       - | 2818 | `						}` |
|  588299 | 2819 | `					}else{` |
|       - | 2820 | ``						/* Typed/readonly property target (`[$o->p] = [...]`): a`` |
|       - | 2821 | `						 * direct slot write would bypass the typed-slot table, so` |
|       - | 2822 | `						 * enforce on a temp first — a TypeError leaves the property` |
|       - | 2823 | `						 * untouched, and a missing key assigns null, which a` |
|       - | 2824 | `						 * non-nullable type rejects exactly like php (warning, then` |
|       - | 2825 | `						 * "Cannot assign null to property ... of type ..."). */` |
|       - | 2826 | `						ph7_value sVal;` |
|      38 | 2827 | `						PH7_MemObjInit(&(*pVm),&sVal);` |
|      38 | 2828 | `						if( rc == SXRET_OK ){` |
|      34 | 2829 | `							PH7_HashmapExtractNodeValue(pNode,&sVal,TRUE);` |
|      16 | 2830 | `						}` |
|      38 | 2831 | `						rcEnforce = VmEnforcePropertyTypeOnStore(&(*pVm),pEntry->nIdx,&sVal,0);` |
|      38 | 2832 | `						if( rcEnforce != SXRET_OK ){` |
|       - | 2833 | `							/* Thrown: stop assigning (php aborts the list at the` |
|       - | 2834 | `							 * first failing element), settle the stack, route. */` |
|      20 | 2835 | `							PH7_MemObjRelease(&sVal);` |
|      20 | 2836 | `							break;` |
|       - | 2837 | `						}` |
|      20 | 2838 | `						PH7_MemObjStore(&sVal,pObj);` |
|      20 | 2839 | `						PH7_MemObjRelease(&sVal);` |
|       - | 2840 | `					}` |
|  588303 | 2841 | `				}` |
|  588303 | 2842 | `			}` |
| 1176655 | 2843 | `			sKey.x.iVal++; /* Next numeric index */` |
| 1176655 | 2844 | `			pEntry++;` |
|       5 | 2845 | `		}` |
|  196624 | 2846 | `	}else if( (pEntry[-1].iFlags & MEMOBJ_OBJ) && pEntry[-1].x.pOther ){` |
|       - | 2847 | `		/* php destructures an OBJECT through its read_dimension handler, one` |
|       - | 2848 | ``		 * READ per POSITION -- `[$a, , $c] = $o` asks for 0 and 2 and never 1 --`` |
|       - | 2849 | `		 * so an ArrayObject, an SplFixedArray and (since the handler landed) a` |
|       - | 2850 | `		 * DOMNodeList all come apart the way an array does. PHL treated every` |
|       - | 2851 | `` 		 * object as a non-array source: it warned `Cannot use object as array` `` |
|       - | 2852 | ``		 * and assigned NULL to every target, so `[$first, $second] = $list` --`` |
|       - | 2853 | `		 * the shape every modern DOM and SPL example is written in -- silently` |
|       - | 2854 | `		 * produced two nulls. An object with NO dimension reader is php's` |
|       - | 2855 | `		 * catchable Error rather than that warning, and it is raised before any` |
|       - | 2856 | ``		 * target is touched. (The KEYED spelling `['k' => $a] = $o` never came`` |
|       - | 2857 | `		 * here: the compiler routes it through OP_LOAD_IDX, which has had the` |
|       - | 2858 | `		 * accessor dispatch all along.) */` |
|      31 | 2859 | `		ph7_class_instance *pInst = (ph7_class_instance *)pEntry[-1].x.pOther;` |
|       - | 2860 | `		ph7_value sKey;` |
|      31 | 2861 | `		if( !VmObjectDimReadable(&(*pVm),pInst) ){` |
|       - | 2862 | `			/* Routed mid-expression, like every other catchable Error raised from` |
|       - | 2863 | `			 * an opcode that is not a call boundary: the destructure is abandoned` |
|       - | 2864 | `			 * and an enclosing try in THIS frame lands on its own handler. Settle` |
|       - | 2865 | `			 * the targets AND the source first — the statement's OP_POP is skipped` |
|       - | 2866 | `			 * when a catch resumes at the landing pad. */` |
|       - | 2867 | `			char zMsg[256];` |
|      13 | 2868 | `			SyString *pName = &pInst->pClass->sName;` |
|      19 | 2869 | `			sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - | 2870 | `				"Cannot use object of type %.*s as array",` |
|      12 | 2871 | `				(int)pName->nByte,pName->zString);` |
|      13 | 2872 | `			rc = VmThrowFromVm(&(*pVm),"Error",zMsg,nMsg);` |
|      13 | 2873 | `			VmPopOperand(&pTos,pInstr->iP1 + 1);` |
|      13 | 2874 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      13 | 2875 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|     ! 0 | 2876 | `		}else{` |
|      19 | 2877 | `			PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      59 | 2878 | `			while( pEntry <= pTos ){` |
|      43 | 2879 | `				if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */ ){` |
|      37 | 2880 | `					sxu32 nSlot = pEntry->nIdx;` |
|      37 | 2881 | `					int bTyped = PH7_VM_STORE_FILTERED(pVm,nSlot);` |
|       - | 2882 | `					ph7_value sVal,*pObj;` |
|      37 | 2883 | `					PH7_MemObjInit(&(*pVm),&sVal);` |
|      37 | 2884 | `					if( VmObjectDimRead(&(*pVm),pInst,&sKey,&sVal) != SXRET_OK ){` |
|       - | 2885 | `						/* The accessor threw: php abandons the destructure there,` |
|       - | 2886 | `						 * so every later target keeps the value it had -- and this` |
|       - | 2887 | `						 * target does too, since php assigns nothing for the read` |
|       - | 2888 | `						 * that failed. */` |
|       3 | 2889 | `						PH7_MemObjRelease(&sVal);` |
|       3 | 2890 | `						rcEnforce = PH7_EXCEPTION;` |
|       3 | 2891 | `						break;` |
|       - | 2892 | `					}` |
|      35 | 2893 | `					if( bTyped ){` |
|       - | 2894 | `						/* Same rule as the array source's typed target: enforce on` |
|       - | 2895 | `						 * the temp so a TypeError leaves the property untouched. */` |
|     ! 0 | 2896 | `						rcEnforce = VmEnforcePropertyTypeOnStore(&(*pVm),nSlot,&sVal,0);` |
|     ! 0 | 2897 | `						if( rcEnforce != SXRET_OK ){` |
|     ! 0 | 2898 | `							PH7_MemObjRelease(&sVal);` |
|     ! 0 | 2899 | `							break;` |
|       - | 2900 | `						}` |
|     ! 0 | 2901 | `					}` |
|       - | 2902 | `					/* Re-fetch AFTER the read: a userland offsetGet can reserve` |
|       - | 2903 | `					 * slots, which used to relocate every pointer into the pool.` |
|       - | 2904 | `					 * Redundant now the table is segmented; left for the harvest` |
|       - | 2905 | `					 * sweep. */` |
|      35 | 2906 | `					pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nSlot);` |
|      35 | 2907 | `					if( pObj ){` |
|      35 | 2908 | `						PH7_MemObjStore(&sVal,pObj);` |
|      17 | 2909 | `					}` |
|      35 | 2910 | `					PH7_MemObjRelease(&sVal);` |
|      17 | 2911 | `				}` |
|      41 | 2912 | `				sKey.x.iVal++; /* Next numeric index */` |
|      41 | 2913 | `				pEntry++;` |
|       1 | 2914 | `			}` |
|      19 | 2915 | `			PH7_MemObjRelease(&sKey);` |
|       - | 2916 | `		}` |
|      10 | 2917 | `	}else{` |
|       - | 2918 | `		/* Source is not an array: php warns first (silencing ONLY null — a bool` |
|       - | 2919 | `		 * source warns too, php 8), then assigns null to every target. A typed` |
|       - | 2920 | `		 * property target receives that null THROUGH enforcement, so a` |
|       - | 2921 | `		 * non-nullable type throws "Cannot assign null to property ..." exactly` |
|       - | 2922 | `		 * like php instead of silently nulling the slot. PHL DIVERGENCE: bool` |
|       - | 2923 | `		 * FALSE stays silent (php warns) — it is the end-of-array sentinel of` |
|       - | 2924 | `` 		 * the documented each() extension's `while (list(..) = each($a))` `` |
|       - | 2925 | `		 * idiom, which would otherwise warn on every normal loop exit. */` |
|       - | 2926 | `		ph7_value *pObj;` |
|      57 | 2927 | `		int bFalseSrc = (pTos[-pInstr->iP1].iFlags & MEMOBJ_BOOL) != 0` |
|      34 | 2928 | `			&& pTos[-pInstr->iP1].x.iVal == 0;` |
|      33 | 2929 | `		sxi32 nWarn = (pTos[-pInstr->iP1].iFlags & MEMOBJ_NULL) == 0 && !bFalseSrc` |
|      41 | 2930 | `			? pInstr->iP2 : 0;` |
|       - | 2931 | `		/* php asks the non-array source for each POSITION it means to fill, so the` |
|       - | 2932 | `		 * warning is per ENTRY and not one for the whole list, which is what this` |
|       - | 2933 | `		 * used to raise. An EMPTY slot fills nothing and is not counted -- P2 carries` |
|       - | 2934 | `		 * the count the compiler made. */` |
|      71 | 2935 | `		while( nWarn > 0 ){` |
|      37 | 2936 | `			VmWarnCannotUseAsArray(&(*pVm),pTos[-pInstr->iP1].iFlags);` |
|      37 | 2937 | `			nWarn--;` |
|       3 | 2938 | `		}` |
|      85 | 2939 | `		while( pEntry <= pTos ){` |
|      57 | 2940 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|      53 | 2941 | `				if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|      53 | 2942 | `					int bTyped = PH7_VM_STORE_FILTERED(pVm,pEntry->nIdx);` |
|      53 | 2943 | `					if( !bTyped ){` |
|      45 | 2944 | `						PH7_MemObjRelease(pObj);` |
|      24 | 2945 | `					}else{` |
|       - | 2946 | `						ph7_value sVal;` |
|       9 | 2947 | `						PH7_MemObjInit(&(*pVm),&sVal);` |
|       9 | 2948 | `						rcEnforce = VmEnforcePropertyTypeOnStore(&(*pVm),pEntry->nIdx,&sVal,0);` |
|       9 | 2949 | `						if( rcEnforce != SXRET_OK ){` |
|       7 | 2950 | `							PH7_MemObjRelease(&sVal);` |
|       7 | 2951 | `							break;` |
|       - | 2952 | `						}` |
|       3 | 2953 | `						PH7_MemObjStore(&sVal,pObj);` |
|       3 | 2954 | `						PH7_MemObjRelease(&sVal);` |
|       - | 2955 | `					}` |
|      22 | 2956 | `				}` |
|      22 | 2957 | `			}` |
|      51 | 2958 | `			pEntry++;` |
|       3 | 2959 | `		}` |
|       - | 2960 | `	}` |
|  393156 | 2961 | `	if( rcEnforce != SXRET_OK ){` |
|       - | 2962 | `		/* Settle this op's own operands: the P1 entries AND the source value —` |
|       - | 2963 | `		 * its statement-level OP_POP is skipped when a catch resumes at the` |
|       - | 2964 | `		 * landing pad, so leaving it would leak one operand slot per caught` |
|       - | 2965 | `		 * throw. A NESTED destructure can still have the outer list's operands` |
|       - | 2966 | `		 * abandoned above the try's base, so on an in-place catch drain to the` |
|       - | 2967 | `		 * catching try's recorded depth (like the fetch-point router and the` |
|       - | 2968 | `		 * generator inject path), not just our own pops. */` |
|      28 | 2969 | `		VmPopOperand(&pTos,pInstr->iP1 + 1);` |
|      28 | 2970 | `		if( rcEnforce == PH7_ABORT ){` |
|     ! 0 | 2971 | `			VM_EXIT_ABORT;` |
|       - | 2972 | `		}` |
|       - | 2973 | `		{` |
|       - | 2974 | `			sxi32 _iRpL;` |
|      40 | 2975 | `			PH7_INLINE_RESUME_BREAK()` |
|      28 | 2976 | `			if( VmRecordedResume(pVm,&_iRpL,pState->pEntryFrame,aInstr) ){` |
|      26 | 2977 | `				PH7_RESUME_DRAIN()` |
|      26 | 2978 | `				pc = _iRpL;` |
|      26 | 2979 | `				VM_EXIT_BREAK;` |
|       - | 2980 | `			}` |
|       - | 2981 | `		}` |
|       3 | 2982 | `		VM_EXIT_EXCEPTION;` |
|       - | 2983 | `	}` |
|  393130 | 2984 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|  393130 | 2985 | `	VM_EXIT_BREAK;` |
|     ! 0 | 2986 | `	VM_EXIT_BREAK;` |
|  196583 | 2987 | `}` |
|       - | 2988 |  |
|       - | 2989 | `/*` |
|       - | 2990 | ` * OP_UNSET_VAR: body moved verbatim from the OP_UNSET_VAR arm of` |
|       - | 2991 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 2992 | ` */` |
|    8995 | 2993 | `PH7_PRIVATE VmOpRc VmExecOpUnsetVar(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 2994 | `{` |
|    9000 | 2995 | `	ph7_value *pTos = pState->pTos;` |
|    9000 | 2996 | `	ph7_value *pStack = pState->pStack;` |
|    9000 | 2997 | `	VmInstr *aInstr = pState->aInstr;` |
|    9000 | 2998 | `	sxi32 pc = pState->pc;` |
|       - | 2999 | `	sxi32 rc;` |
|    4491 | 3000 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - | 3001 | `	/* unset($name): p3 is the variable name. Drops the NAME only — see VmUnsetVarByName */` |
|    9000 | 3002 | `	SyString *pName = (SyString *)pInstr->p3;` |
|    9000 | 3003 | `	if( pName && pVm->pFrame ){` |
|       - | 3004 | `		/* Inside a try{} the VM pushes an EXCEPTION frame; variables live in the body` |
|       - | 3005 | `		 * frame below it, so skip past it exactly as every other variable path does.` |
|       - | 3006 | `		 * Without this, unset($x) inside a try silently found nothing and did nothing. */` |
|    9000 | 3007 | `		VmFrame *pVarFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|    9000 | 3008 | `		sxi32 rcU = VmUnsetVarByName(&(*pVm),pVarFrame,pName->zString,pName->nByte);` |
|    9000 | 3009 | `		if( rcU == PH7_ABORT ){` |
|       3 | 3010 | `			VM_EXIT_ABORT;` |
|       - | 3011 | `		}` |
|       - | 3012 | `		/* Releasing the last holder can run a __destruct(), and that destructor may` |
|       - | 3013 | `		 * throw. Such a throw is PARKED in nBoundaryRc by the boundary rail; consume it` |
|       - | 3014 | `		 * here and route it, or the catch runs and execution resumes inside the try` |
|       - | 3015 | `		 * ("resumed-dtor" instead of php's "caught-dtor"). */` |
|    8998 | 3016 | `		if( pVm->nBoundaryRc != 0 ){` |
|       6 | 3017 | `			rc = pVm->nBoundaryRc;` |
|       6 | 3018 | `			pVm->nBoundaryRc = 0;` |
|       6 | 3019 | `			if( rc == PH7_ABORT ){` |
|     ! 0 | 3020 | `				VM_EXIT_ABORT;` |
|       - | 3021 | `			}` |
|       6 | 3022 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 3023 | `		}` |
|    4488 | 3024 | `	}` |
|    8994 | 3025 | `	VM_EXIT_BREAK;` |
|     ! 0 | 3026 | `	VM_EXIT_BREAK;` |
|    4496 | 3027 | `}` |
|       - | 3028 |  |
