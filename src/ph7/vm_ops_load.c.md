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
|    5070 |   28 | `PH7_PRIVATE VmOpRc VmExecOpStoreRef(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |   29 | `{` |
|    5075 |   30 | `	ph7_value *pTos = pState->pTos;` |
|    5075 |   31 | `	ph7_value *pStack = pState->pStack;` |
|    5075 |   32 | `	VmInstr *aInstr = pState->aInstr;` |
|    5075 |   33 | `	sxi32 pc = pState->pc;` |
|       - |   34 | `	sxi32 rc;` |
|    2535 |   35 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    5075 |   36 | `	 SyString sName = { 0 , 0 };` |
|       - |   37 | `	 VmFrame *pFrameLocal;` |
|       - |   38 | `	SyHashEntry *pEntry;` |
|       - |   39 | `	sxu32 nIdx;` |
|       - |   40 | `#ifdef UNTRUST` |
|       - |   41 | `	if( pTos < pStack ){` |
|       - |   42 | `		VM_EXIT_ABORT;` |
|       - |   43 | `	}` |
|       - |   44 | `#endif` |
|    5075 |   45 | `	if( pInstr->iP2 == 1 ){` |
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
|    5003 |  134 | `	if( pInstr->p3 == 0 ){` |
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
|    5003 |  151 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|       - |  152 | `	}` |
|    5003 |  153 | `	if( pTos->iFlags & MEMOBJ_AUX_STROFFSET ){` |
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
|    4995 |  167 | `	nIdx = pTos->nIdx;` |
|       - |  168 | `	{` |
|       - |  169 | ``		/* `$r = &$o->p` on a property no write may reach: php refuses the BIND,`` |
|       - |  170 | `		 * because the alias would let a later write through $r reach the property` |
|       - |  171 | ``		 * with nothing in the way. PHL bound it, so `$r = 99` afterwards rewrote a`` |
|       - |  172 | `		 * readonly property and a DatePeriod's recurrence count alike. */` |
|    4995 |  173 | `		sxi32 rcNw = PH7_VmCheckIndirectModify(&(*pVm),nIdx);` |
|    4995 |  174 | `		if( rcNw != SXRET_OK ){` |
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
|    4985 |  185 | `	if(nIdx == SXU32_HIGH ){` |
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
|    4974 |  214 | `	}else if( sName.nByte > 0){` |
|    4963 |  215 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|       - |  216 | `			/* php 8.1's non-catchable fatal (compile-time there) */` |
|       3 |  217 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot acquire reference to $GLOBALS");` |
|       3 |  218 | `			pVm->iExitStatus = 255;` |
|       3 |  219 | `			pVm->bHaltRequested = 1;` |
|       3 |  220 | `			VM_EXIT_ABORT;` |
|     ! 0 |  221 | `		}else{` |
|    4961 |  222 | `			pFrameLocal = pVm->pFrame;` |
|    4961 |  223 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       - |  224 | `			/* Query the local frame */` |
|    4961 |  225 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|    4961 |  226 | `			if( pEntry ){` |
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
|     271 |  237 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       - |  238 | `				/* Installing a name is a rebind too, for a frame that may still` |
|       - |  239 | `				 * remember where the name USED to live -- the same reason` |
|       - |  240 | `				 * PH7_VmBindVarSlot flushes on its own insert branch. */` |
|     271 |  241 | `				VmVarMemoFlush(pFrameLocal);` |
|     271 |  242 | `				if( pFrameLocal->pParent == 0 ){` |
|       - |  243 | `					/* Insert in the $GLOBALS array */` |
|     177 |  244 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|      86 |  245 | `				}` |
|     271 |  246 | `				if( rc == SXRET_OK ){` |
|     271 |  247 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|     133 |  248 | `				}` |
|       - |  249 | `			}` |
|       - |  250 | `		}` |
|    2478 |  251 | `	}` |
|    4983 |  252 | `	VM_EXIT_BREAK;` |
|     ! 0 |  253 | `	VM_EXIT_BREAK;` |
|    2540 |  254 | `}` |
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
|  576004 |  278 | `static int VmOffsetTypeRejected(ph7_vm *pVm,ph7_value *pKey,int iCtx,SyBlob *pMsg)` |
|       5 |  279 | `{` |
|       - |  280 | `	const char *zType;` |
|  576009 |  281 | `	SyString *pClass = 0;` |
|  576009 |  282 | `	if( pKey == 0 ){` |
|     ! 0 |  283 | `		return FALSE;` |
|       - |  284 | `	}` |
|  576009 |  285 | `	if( pKey->iFlags & MEMOBJ_OBJ ){` |
|      38 |  286 | `		ph7_class_instance *pInst = (ph7_class_instance *)pKey->x.pOther;` |
|      38 |  287 | `		if( pInst && pInst->pClass ){` |
|      38 |  288 | `			pClass = &pInst->pClass->sName;` |
|      18 |  289 | `		}` |
|      38 |  290 | `		zType = "object";` |
|  575991 |  291 | `	}else if( pKey->iFlags & MEMOBJ_HASHMAP ){` |
|      32 |  292 | `		zType = "array";` |
|      17 |  293 | `	}else{` |
|  575943 |  294 | `		return FALSE;` |
|       - |  295 | `	}` |
|      68 |  296 | `	SyBlobInit(pMsg,&pVm->sAllocator);` |
|      68 |  297 | `	if( VM_IDX_IS_UNSET(iCtx) ){` |
|       3 |  298 | `		SyBlobAppend(pMsg,"Cannot unset offset of type ",sizeof("Cannot unset offset of type ")-1);` |
|       2 |  299 | `	}else{` |
|      66 |  300 | `		SyBlobAppend(pMsg,"Cannot access offset of type ",sizeof("Cannot access offset of type ")-1);` |
|       - |  301 | `	}` |
|      68 |  302 | `	if( pClass ){` |
|      38 |  303 | `		SyBlobAppend(pMsg,pClass->zString,pClass->nByte);` |
|      20 |  304 | `	}else{` |
|      32 |  305 | `		SyBlobAppend(pMsg,zType,(sxu32)SyStrlen(zType));` |
|       - |  306 | `	}` |
|      68 |  307 | `	if( iCtx == VM_IDX_CTX_ISSET \|\| iCtx == VM_IDX_CTX_EMPTY ){` |
|       7 |  308 | `		SyBlobAppend(pMsg," in isset or empty",sizeof(" in isset or empty")-1);` |
|       4 |  309 | `	}else{` |
|      62 |  310 | `		SyBlobAppend(pMsg," on array",sizeof(" on array")-1);` |
|       - |  311 | `	}` |
|      68 |  312 | `	return TRUE;` |
|  287833 |  313 | `}` |
|       - |  314 | `/*` |
|       - |  315 | ` * php's other half of the offset-type rules: a RESOURCE offset is accepted, with` |
|       - |  316 | `` * `Warning: Resource ID#N used as offset, casting to integer (N)`, and the key`` |
|       - |  317 | ` * becomes that integer. Rewrites pKey in place so the normal integer-key path` |
|       - |  318 | ` * takes over.` |
|       - |  319 | ` */` |
|  576138 |  320 | `PH7_PRIVATE void PH7_VmOffsetResourceWarn(ph7_vm *pVm,ph7_value *pKey)` |
|       5 |  321 | `{` |
|       - |  322 | `	sxu32 nId;` |
|  576143 |  323 | `	if( pKey == 0 \|\| (pKey->iFlags & MEMOBJ_RES) == 0 ){` |
|  576071 |  324 | `		return;` |
|       - |  325 | `	}` |
|      74 |  326 | `	nId = PH7_VmResourceId(pVm,pKey->x.pOther);` |
|     110 |  327 | `	VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|      36 |  328 | `		"Resource ID#%u used as offset, casting to integer (%u)",nId,nId);` |
|      74 |  329 | `	PH7_MemObjRelease(pKey);` |
|      74 |  330 | `	pKey->x.iVal = (sxi64)nId;` |
|      74 |  331 | `	MemObjSetType(pKey,MEMOBJ_INT);` |
|  287900 |  332 | `}` |
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
|  614629 |  344 | `PH7_PRIVATE int PH7_VmNullOffsetDeprecate(ph7_vm *pVm,ph7_value *pKey)` |
|       5 |  345 | `{` |
|  614634 |  346 | `	if( pKey == 0 \|\| (pKey->iFlags & MEMOBJ_NULL) == 0 ){` |
|  614580 |  347 | `		return FALSE;` |
|       - |  348 | `	}` |
|      58 |  349 | `	PH7_VmThrowError(&(*pVm),0,E_DEPRECATED,` |
|       - |  350 | `		"Using null as an array offset is deprecated, use an empty string instead");` |
|      58 |  351 | `	return TRUE;` |
|  307207 |  352 | `}` |
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
|       - |  376 | ` * php words the illegal-type rejection by DOOR: the compiled array_key_exists() call` |
|       - |  377 | ` * its compiler folds into ZEND_ARRAY_KEY_EXISTS answers the engine's offset Error,` |
|       - |  378 | ` * and every real call -- the key_exists alias, a name in a variable, array_map, a` |
|       - |  379 | `` * first-class callable -- the ZPP `…(): Argument #1 ($key) must be a valid array`` |
|       - |  380 | `` * offset type` (see PH7_CTX_CALL_FOLDED); the`` |
|       - |  381 | ` * null-key DEPRECATION names array_key_exists() for BOTH of those spellings and is` |
|       - |  382 | `` * the engine's own `Using null as an array offset is deprecated` everywhere else.`` |
|       - |  383 | ` *` |
|       - |  384 | ` * pKey is rewritten in place (resource -> int), so callers pass a private copy,` |
|       - |  385 | ` * not the caller's own argument slot. Returns SXRET_OK when the key is usable, or` |
|       - |  386 | ` * the status of the TypeError thrown.` |
|       - |  387 | ` */` |
|     550 |  388 | `PH7_PRIVATE sxi32 PH7_VmArrayKeyArg(ph7_context *pCtx,ph7_value *pKey,int iWording)` |
|       5 |  389 | `{` |
|     555 |  390 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - |  391 | `	SyBlob sMsg;` |
|     555 |  392 | `	if( VmOffsetTypeRejected(pVm,pKey,0,&sMsg) ){` |
|       - |  393 | `		sxi32 rc;` |
|      40 |  394 | `		if( iWording == PH7_ARRAYKEY_ZPP ){` |
|      13 |  395 | `			SyBlobRelease(&sMsg);` |
|      19 |  396 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  397 | `				"%s(): Argument #1 ($key) must be a valid array offset type",` |
|       6 |  398 | `				ph7_function_name(pCtx));` |
|       - |  399 | `		}` |
|      41 |  400 | `		rc = PH7_VmThrowException(pCtx,"TypeError","%.*s",` |
|      26 |  401 | `			(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      28 |  402 | `		SyBlobRelease(&sMsg);` |
|      28 |  403 | `		return rc;` |
|       - |  404 | `	}` |
|     517 |  405 | `	PH7_VmOffsetResourceWarn(pVm,pKey);` |
|     512 |  406 | `	if( (pKey->iFlags & MEMOBJ_REAL)` |
|     271 |  407 | `	 && pKey->rVal != (ph7_real)(sxi64)pKey->rVal ){` |
|       - |  408 | `		/* php DEPRECATES the lossy float and truncates; PHL rejects it, here as` |
|       - |  409 | ``		 * everywhere else (the scope policy) — and with the SAME message `$a[5.7]` gives, so`` |
|       - |  410 | `		 * the builtin and the subscript stay one rule. */` |
|      14 |  411 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  412 | `			"Cannot access offset of type float on array");` |
|       - |  413 | `	}` |
|     505 |  414 | `	if( pKey->iFlags & MEMOBJ_NULL ){` |
|      10 |  415 | `		if( iWording == PH7_ARRAYKEY_OFFSET ){` |
|       7 |  416 | `			PH7_VmNullOffsetDeprecate(pVm,pKey);` |
|       4 |  417 | `		}else{` |
|       - |  418 | `			/* php names the parameter here rather than "an array offset"; the effect` |
|       - |  419 | `			 * is the engine's — the lookup below reads the "" key. */` |
|       3 |  420 | `			PH7_VmThrowError(pVm,0,E_DEPRECATED,` |
|       - |  421 | `				"Using null as the key parameter for array_key_exists() is deprecated, "` |
|       - |  422 | `				"use an empty string instead");` |
|       - |  423 | `		}` |
|       4 |  424 | `	}` |
|     505 |  425 | `	return SXRET_OK;` |
|     280 |  426 | `}` |
|       - |  427 | `/*` |
|       - |  428 | ` * Does this string START with an integer php's is_numeric_string would answer` |
|       - |  429 | ` * IS_LONG for? Returns 0 when it does not — no digits at all ("", "-", "p"), a` |
|       - |  430 | ` * FLOAT-shaped prefix ("1.5", ".5", "1.", "1e2", "1E2x") or a run that overflows` |
|       - |  431 | ` * a signed 64-bit int ("9223372036854775808") — 1 when it does and only optional` |
|       - |  432 | ` * trailing WHITESPACE follows (" 12 ", "+12", "007"), and 2 when it does but` |
|       - |  433 | ` * trailing DATA follows ("12abc", "0x1", "1e", "1 x"), which is php's` |
|       - |  434 | `` * `Illegal string offset` warning. *piVal receives the integer for 1 and 2.`` |
|       - |  435 | ` *` |
|       - |  436 | ` * php reaches the same three answers through is_numeric_string_ex(allow_errors=1)` |
|       - |  437 | ` * plus its IS_LONG/IS_DOUBLE verdict: a fraction or a well-formed exponent makes` |
|       - |  438 | ` * the verdict IS_DOUBLE, which a string offset refuses, while a bare 'e' is just` |
|       - |  439 | ` * trailing data.` |
|       - |  440 | ` */` |
|     104 |  441 | `static int VmStringOffsetInt(const char *zIn,sxu32 nByte,sxi64 *piVal)` |
|       3 |  442 | `{` |
|     107 |  443 | `	const char *z = zIn, *zEnd = &zIn[nByte], *zDigit;` |
|     107 |  444 | `	sxu64 uVal = 0, uLimit;` |
|     107 |  445 | `	int isNeg = 0, nDigit, i;` |
|     119 |  446 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){` |
|      14 |  447 | `		z++;` |
|       2 |  448 | `	}` |
|     107 |  449 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|       7 |  450 | `		isNeg = z[0] == '-';` |
|       7 |  451 | `		z++;` |
|       3 |  452 | `	}` |
|     107 |  453 | `	zDigit = z;` |
|     247 |  454 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|     142 |  455 | `		z++;` |
|       2 |  456 | `	}` |
|     107 |  457 | `	nDigit = (int)(z - zDigit);` |
|     107 |  458 | `	if( nDigit < 1 ){` |
|      43 |  459 | `		return 0;` |
|       - |  460 | `	}` |
|      66 |  461 | `	if( z < zEnd && z[0] == '.' ){` |
|       - |  462 | `		/* "1." and "1.5" alike: php reads a double from here. */` |
|       5 |  463 | `		return 0;` |
|       - |  464 | `	}` |
|      62 |  465 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|       3 |  466 | `		const char *zExp = &z[1];` |
|       3 |  467 | `		if( zExp < zEnd && (zExp[0] == '+' \|\| zExp[0] == '-') ){` |
|     ! 0 |  468 | `			zExp++;` |
|     ! 0 |  469 | `		}` |
|       3 |  470 | `		if( zExp < zEnd && (unsigned char)zExp[0] < 0xc0 && SyisDigit(zExp[0]) ){` |
|       3 |  471 | `			return 0;` |
|       - |  472 | `		}` |
|     ! 0 |  473 | `	}` |
|       - |  474 | `	/* Accumulate unsigned so PHP_INT_MIN's magnitude (2^63) is representable —` |
|       - |  475 | `	 * "-9223372036854775808" is a legal offset, "9223372036854775808" is not. */` |
|      64 |  476 | `	while( nDigit > 1 && zDigit[0] == '0' ){` |
|       5 |  477 | `		zDigit++; nDigit--;` |
|       1 |  478 | `	}` |
|      60 |  479 | `	uLimit = isNeg ? (sxu64)SXI64_HIGH + 1 : (sxu64)SXI64_HIGH;` |
|      60 |  480 | `	if( nDigit > 19 ){` |
|     ! 0 |  481 | `		return 0;` |
|       - |  482 | `	}` |
|     188 |  483 | `	for( i = 0 ; i < nDigit ; ++i ){` |
|     132 |  484 | `		sxu64 d = (sxu64)(zDigit[i] - '0');` |
|     132 |  485 | `		if( uVal > (uLimit - d)/10 ){` |
|       3 |  486 | `			return 0;` |
|       - |  487 | `		}` |
|     130 |  488 | `		uVal = uVal*10 + d;` |
|      66 |  489 | `	}` |
|      58 |  490 | `	*piVal = isNeg ? (sxi64)(~uVal + 1) : (sxi64)uVal;` |
|      68 |  491 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){` |
|      12 |  492 | `		z++;` |
|       2 |  493 | `	}` |
|      58 |  494 | `	return z == zEnd ? 1 : 2;` |
|      55 |  495 | `}` |
|       - |  496 | `/*` |
|       - |  497 | ` * php's offset rules for a STRING container (zend_check_string_offset, and the` |
|       - |  498 | ` * isset/empty arm of ZEND_ISSET_ISEMPTY_DIM_OBJ). They are NOT the array rules,` |
|       - |  499 | ` * and PHL applied none of them: every offset went through an int cast, so` |
|       - |  500 | `` * `$s[""]`, `$s["-"]` and `$s["p"]` all answered `$s[0]` — a silent wrong answer`` |
|       - |  501 | ` * on code php refuses to run. php's table:` |
|       - |  502 | ` *` |
|       - |  503 | ` *   int                     the offset` |
|       - |  504 | ` *   null / bool / float     Warning: String offset cast occurred, then cast` |
|       - |  505 | ` *   integer-shaped string   the offset (leading/trailing space and '+' allowed)` |
|       - |  506 | ` *   int-then-garbage string Warning: Illegal string offset "12abc", then 12` |
|       - |  507 | ` *   any other string        TypeError: Cannot access offset of type string on string` |
|       - |  508 | ` *   array/object/resource   TypeError, naming the type (an object's CLASS)` |
|       - |  509 | ` *` |
|       - |  510 | ` * isset()/empty() raise NOTHING and answer "not set" for every shape the read` |
|       - |  511 | `` * path would reject OR warn about: `isset($s["0x1"])` is false even though`` |
|       - |  512 | `` * reading it warns and yields `$s[0]`. A `??` fetch sits BETWEEN that and a real`` |
|       - |  513 | ` * read — it suppresses the not-set diagnostics but still warns about the offset` |
|       - |  514 | ` * SHAPE and still reads it. iLevel selects which of the three (VM_STROFF_LOUD /` |
|       - |  515 | ` * _COALESCE / _ISSET).` |
|       - |  516 | ` */` |
| 1320338 |  517 | `PH7_PRIVATE int VmStringOffsetResolve(ph7_vm *pVm,ph7_value *pIdx,int iLevel,sxi64 *piOfft,SyBlob *pMsg)` |
|       5 |  518 | `{` |
| 1320343 |  519 | `	if( (pIdx->iFlags & MEMOBJ_INT) != 0 && (pIdx->iFlags & MEMOBJ_REAL) == 0 ){` |
| 1320171 |  520 | `		*piOfft = pIdx->x.iVal;` |
| 1320171 |  521 | `		return VM_STROFF_OK;` |
|       - |  522 | `	}` |
|     175 |  523 | `	if( pIdx->iFlags & MEMOBJ_STRING ){` |
|     159 |  524 | `		int eInt = VmStringOffsetInt((const char *)SyBlobData(&pIdx->sBlob),` |
|      52 |  525 | `			SyBlobLength(&pIdx->sBlob),piOfft);` |
|     107 |  526 | `		if( eInt == 1 ){` |
|      24 |  527 | `			return VM_STROFF_OK;` |
|       - |  528 | `		}` |
|      85 |  529 | `		if( eInt == 2 && iLevel != VM_STROFF_ISSET ){` |
|       - |  530 | ``			/* int-then-garbage. This is the one diagnostic a `??` fetch keeps:`` |
|       - |  531 | ``			 * `$s["1x"] ?? "d"` warns and answers $s[1] (PHL called it "not set"`` |
|       - |  532 | `			 * and answered the default — a wrong VALUE, not just a missing` |
|       - |  533 | `			 * warning). Only isset()/empty() — and the intermediate step of an` |
|       - |  534 | `			 * unset chain, which php keeps quiet about the shape — stay silent. */` |
|      28 |  535 | `			if( iLevel != VM_STROFF_UNSETBASE ){` |
|       - |  536 | `				SyString sKey;` |
|      28 |  537 | `				SyStringInitFromBuf(&sKey,SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob));` |
|      28 |  538 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Illegal string offset \"%z\"",&sKey);` |
|      13 |  539 | `			}` |
|      28 |  540 | `			return VM_STROFF_OK;` |
|       - |  541 | `		}` |
|      59 |  542 | `		if( iLevel != VM_STROFF_LOUD && iLevel != VM_STROFF_UNSETBASE ){` |
|      24 |  543 | `			return VM_STROFF_MISS;` |
|       3 |  544 | `		}` |
|      87 |  545 | `	}else if( (pIdx->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|       - |  546 | `		/* null / bool / float: php casts, but says so in a real read or write —` |
|       - |  547 | `		 * and in the intermediate step of an unset chain, which reads the offset` |
|       - |  548 | ``		 * to hand it on (`unset($s[1.5][0])` warns about the cast). */`` |
|      48 |  549 | `		if( iLevel == VM_STROFF_LOUD \|\| iLevel == VM_STROFF_UNSETBASE ){` |
|      34 |  550 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"String offset cast occurred");` |
|      16 |  551 | `		}` |
|      48 |  552 | `		PH7_MemObjToInteger(pIdx);` |
|      48 |  553 | `		*piOfft = pIdx->x.iVal;` |
|      48 |  554 | `		return VM_STROFF_OK;` |
|      24 |  555 | `	}else if( iLevel == VM_STROFF_ISSET ){` |
|       - |  556 | `		/* An array/object/resource offset is "not set" for isset()/empty() — but a` |
|       - |  557 | ``		 * `??` fetch RAISES for it (probed: `$s[[]] ?? "d"` is the TypeError while`` |
|       - |  558 | ``		 * `isset($s[[]])` is false), so only the fully-quiet level answers MISS. */`` |
|      10 |  559 | `		return VM_STROFF_MISS;` |
|       - |  560 | `	}` |
|       - |  561 | `	{` |
|       - |  562 | `		char zBuf[128];` |
|      51 |  563 | `		SyBlobInit(pMsg,&pVm->sAllocator);` |
|      75 |  564 | `		SyBlobFormat(pMsg,"Cannot access offset of type %s on string",` |
|      24 |  565 | `			VmValueGivenName(pIdx,zBuf,sizeof(zBuf)));` |
|       - |  566 | `	}` |
|      51 |  567 | `	return VM_STROFF_REJECT;` |
|  663321 |  568 | `}` |
|       - |  569 | `/*` |
|       - |  570 | ` * OP_STORE_IDX_REF: body moved verbatim from the OP_STORE_IDX_REF arm of` |
|       - |  571 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  572 | ` */` |
|  615565 |  573 | `PH7_PRIVATE VmOpRc VmExecOpStoreIdxRef(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  574 | `{` |
|  615570 |  575 | `	ph7_value *pTos = pState->pTos;` |
|  615570 |  576 | `	ph7_value *pStack = pState->pStack;` |
|  615570 |  577 | `	VmInstr *aInstr = pState->aInstr;` |
|  615570 |  578 | `	sxi32 pc = pState->pc;` |
|       - |  579 | `	sxi32 rc;` |
|  307670 |  580 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|  615570 |  581 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|       - |  582 | `	ph7_value *pKey;` |
|       - |  583 | `	sxu32 nIdx;` |
|  615570 |  584 | `	if( pInstr->iP1 & 1 ){` |
|       - |  585 | `		/* Key is next on stack (bit 1 is PH7_STOREREF_CALLSRC, not a key) */` |
|  104618 |  586 | `		pKey = pTos;` |
|  104618 |  587 | `		pTos--;` |
|   52229 |  588 | `	}else{` |
|  510957 |  589 | `		pKey = 0;` |
|       - |  590 | `	}` |
|       - |  591 | `		/* php DEPRECATES a null / lossy-float write subscript (then normalizes "" /` |
|       - |  592 | ``		 * truncates); it does not reject either. A null key — by-VALUE (`$a[$k]=v`)`` |
|       - |  593 | ``		 * OR by-REF (`$a[$k]=&$x`) — deprecates + coerces to the "" key: the notice is`` |
|       - |  594 | `		 * emitted DOWN AT THE INSERT (below), not here, so a null/false container that` |
|       - |  595 | ``		 * auto-vivifies (`$x=null; $x[null]=v`) gets it too (the base is not yet a`` |
|       - |  596 | `		 * hashmap at this point), and PH7_HashmapInsert / HashmapInsertByRef both cast` |
|       - |  597 | `		 * NULL->"". A lossy-FLOAT subscript stays a loud TypeError in BOTH forms (the` |
|       - |  598 | `		 * recorded non-deprecated-surface policy). */` |
|  615570 |  599 | `		if( pKey && (pTos->iFlags & MEMOBJ_HASHMAP) ){` |
|       - |  600 | `			SyBlob sTypeMsg;` |
|       - |  601 | `			/* An object/array key is php's TypeError; a resource key warns and` |
|       - |  602 | `			 * becomes its integer id. Both used to be stringified silently. */` |
|  103544 |  603 | `			if( VmOffsetTypeRejected(&(*pVm),pKey,0,&sTypeMsg) ){` |
|       - |  604 | `				sxi32 rcSc;` |
|       8 |  605 | `				PH7_MemObjRelease(pKey);` |
|       8 |  606 | `				VmPopOperand(&pTos,1);` |
|       8 |  607 | `				rcSc = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|      12 |  608 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       8 |  609 | `				rc = rcSc;` |
|       8 |  610 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  611 | `			}` |
|  103538 |  612 | `			PH7_VmOffsetResourceWarn(&(*pVm),pKey);` |
|  103533 |  613 | `			if( (pKey->iFlags & MEMOBJ_REAL)` |
|   51692 |  614 | `			 && pKey->rVal != (ph7_real)(sxi64)pKey->rVal ){` |
|       - |  615 | `				sxi32 rcSc;` |
|       3 |  616 | `				const char *zErr = "Cannot access offset of type float on array";` |
|       3 |  617 | `				PH7_MemObjRelease(pKey);` |
|       3 |  618 | `				VmPopOperand(&pTos,1);` |
|       3 |  619 | `				rcSc = VmThrowFromVm(&(*pVm),"TypeError",zErr,(sxu32)SyStrlen(zErr));` |
|       3 |  620 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       3 |  621 | `				rc = rcSc;` |
|       3 |  622 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  623 | `			}` |
|   51683 |  624 | `		}` |
|  615562 |  625 | `	if( pTos->iFlags & MEMOBJ_AUX_STROFFSET ){` |
|       - |  626 | ``		/* The CONTAINER is a string offset: `$s[0][1] = 'x'`, `$s[0][] = 'x'`,`` |
|       - |  627 | ``		 * `$a['k'][0][1] = 'x'`. php refuses to reach inside one — `Cannot use string`` |
|       - |  628 | ``		 * offset as an array`, the same refusal the fetch path raises — and this is`` |
|       - |  629 | `		 * where the outermost level of an ordinary assignment arrives, its LOAD_IDX` |
|       - |  630 | `		 * folded into the store. The character read out of the string still carries` |
|       - |  631 | ``		 * the BASE STRING's slot, so the write landed ON the base: `$s = 'ab';`` |
|       - |  632 | ``		 * $s[0][1] = 'x';` left `$s === 'ax'`, in silence. */`` |
|       - |  633 | `		sxi32 rcSo;` |
|       9 |  634 | `		if( pKey ){` |
|       7 |  635 | `			PH7_MemObjRelease(pKey);` |
|       3 |  636 | `		}` |
|       9 |  637 | `		VmPopOperand(&pTos,1);` |
|       9 |  638 | `		rcSo = VmThrowFromVm(&(*pVm),"Error","Cannot use string offset as an array",` |
|       - |  639 | `			sizeof("Cannot use string offset as an array")-1);` |
|       9 |  640 | `		if( rcSo == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       9 |  641 | `		rc = rcSo;` |
|       9 |  642 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  643 | `	}` |
|  615554 |  644 | `	nIdx = pTos->nIdx;` |
|       - |  645 | `	{` |
|       - |  646 | `		/* ArrayAccess::offsetSet dispatch.` |
|       - |  647 | `		 * Container may be on the stack as MEMOBJ_OBJ, or referenced via` |
|       - |  648 | `		 * the backing variable slot at nIdx. */` |
|  615554 |  649 | `		ph7_class_instance *pInst = 0;` |
|  615554 |  650 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|     606 |  651 | `			pInst = (ph7_class_instance *)pTos->x.pOther;` |
|  615253 |  652 | `		}else if( nIdx != SXU32_HIGH ){` |
|  614876 |  653 | `			ph7_value *pBacking = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
|  614876 |  654 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_OBJ) ){` |
|     ! 0 |  655 | `				pInst = (ph7_class_instance *)pBacking->x.pOther;` |
|     ! 0 |  656 | `			}` |
|  307323 |  657 | `		}` |
|  615554 |  658 | `		if( pInst ){` |
|     606 |  659 | `			ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|     606 |  660 | `			if( pArrayAccess && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|       - |  661 | `				ph7_class_method *pMeth;` |
|       - |  662 | `				ph7_value sNullKey;` |
|       - |  663 | `				ph7_value *apArg[2];` |
|     550 |  664 | `				if( pInstr->iOp == PH7_OP_STORE_IDX_REF ){` |
|     ! 0 |  665 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - |  666 | `						"Cannot assign by reference to overloaded object");` |
|     ! 0 |  667 | `					if( pKey ){ PH7_MemObjRelease(pKey); }` |
|     ! 0 |  668 | `					VmPopOperand(&pTos,2); /* container + value */` |
|     ! 0 |  669 | `					VM_EXIT_BREAK;` |
|       - |  670 | `				}` |
|     550 |  671 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - |  672 | `					"offsetSet",sizeof("offsetSet")-1);` |
|       - |  673 | `				/* Pop container; pTos now points to the value */` |
|     550 |  674 | `				VmPopOperand(&pTos,1);` |
|     550 |  675 | `				if( pKey == 0 ){` |
|      18 |  676 | `					PH7_MemObjInit(&(*pVm),&sNullKey);` |
|      18 |  677 | `					apArg[0] = &sNullKey;` |
|      10 |  678 | `				}else{` |
|     534 |  679 | `					apArg[0] = pKey;` |
|       - |  680 | `				}` |
|     550 |  681 | `				apArg[1] = pTos;` |
|     550 |  682 | `				if( pMeth ){` |
|     550 |  683 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,0,2,apArg);` |
|     273 |  684 | `				}` |
|     550 |  685 | `				if( pKey ){` |
|     534 |  686 | `					PH7_MemObjRelease(pKey);` |
|     269 |  687 | `				}else{` |
|      18 |  688 | `					PH7_MemObjRelease(&sNullKey);` |
|       - |  689 | `				}` |
|       - |  690 | `				/* The VALUE stays on the stack: a store IS an expression and its` |
|       - |  691 | `` 				 * result is what was assigned, which is why `$x = ($c[$k] = 42)` `` |
|       - |  692 | ``				 * and `return $this[$k] = 42;` are 42 in php even when offsetSet`` |
|       - |  693 | `				 * stored something else. This arm popped it, so the expression it` |
|       - |  694 | ``				 * belongs to read a slot the stack no longer owned -- `var_dump($c[$k]`` |
|       - |  695 | ``				 * = 5)` printed garbage for a native ArrayAccess and SEGFAULTED for a`` |
|       - |  696 | `				 * user one. The ordinary hashmap store below pops only the container,` |
|       - |  697 | `				 * for exactly this reason. */` |
|     550 |  698 | `				VM_EXIT_BREAK;` |
|       - |  699 | `			}` |
|       - |  700 | `			/* Object without ArrayAccess, but with a dimension handler that` |
|       - |  701 | `			 * really STORES: php's SimpleXMLElement writes an attribute for` |
|       - |  702 | ``			 * `$x['a'] = '1'` and appends an element for `$x->kid[] = 'v'`,`` |
|       - |  703 | `			 * through a write_dimension it has instead of the interface. The` |
|       - |  704 | `			 * handler is offered the write before the refusal below, and takes` |
|       - |  705 | `			 * it or leaves it -- DOMNodeList and PDORow leave it. */` |
|      56 |  706 | `			if( pInstr->iOp == PH7_OP_STORE_IDX_REF` |
|      31 |  707 | `			 && PH7_ClassHasNativeDim(pInst->pClass) ){` |
|       - |  708 | `				/* php words a by-reference store into a class whose OWN handler` |
|       - |  709 | `				 * answers dimensions differently from one that answers none:` |
|       - |  710 | `` 				 * `Cannot assign by reference to an array dimension of an object` `` |
|       - |  711 | ``				 * rather than `Cannot use object of type C as array`, which stays`` |
|       - |  712 | `				 * the answer for a plain object. (An ArrayAccess class has its own` |
|       - |  713 | `				 * third sentence, raised above.) */` |
|     ! 0 |  714 | `				const char *zRef = "Cannot assign by reference to an array dimension of an object";` |
|     ! 0 |  715 | `				rc = VmThrowFromVm(pVm,"Error",zRef,(sxu32)SyStrlen(zRef));` |
|     ! 0 |  716 | `				if( pKey ){ PH7_MemObjRelease(pKey); }` |
|     ! 0 |  717 | `				VmPopOperand(&pTos,2);` |
|     ! 0 |  718 | `				if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     ! 0 |  719 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  720 | `			}` |
|      59 |  721 | `			if( pInstr->iOp != PH7_OP_STORE_IDX_REF ){` |
|       - |  722 | `				PH7_NativeDimCtx sDim;` |
|       - |  723 | `				/* The stack is [value, container]; the container is popped only` |
|       - |  724 | `				 * once the handler has taken the write, so the refusal below` |
|       - |  725 | `				 * still sees both operands. */` |
|      87 |  726 | `				if( PH7_ClassNativeDimStore(pInst,` |
|      28 |  727 | `					pKey == 0 ? PH7_NATIVE_DIM_APPEND : PH7_NATIVE_DIM_WRITE,` |
|      28 |  728 | `					pKey,pTos - 1,&sDim) ){` |
|      42 |  729 | `					if( pKey ){ PH7_MemObjRelease(pKey); }` |
|      42 |  730 | `					if( sDim.zThrowClass ){` |
|       - |  731 | `						/* The refusal takes BOTH operands, like the ordinary one` |
|       - |  732 | `						 * below: the store never happened, so its value is not the` |
|       - |  733 | `						 * expression's result. */` |
|       8 |  734 | `						VmPopOperand(&pTos,2);` |
|      11 |  735 | `						rc = VmThrowFromVm(pVm,sDim.zThrowClass,sDim.zThrowMsg,` |
|       6 |  736 | `							(sxu32)SyStrlen(sDim.zThrowMsg));` |
|      28 |  737 | `						if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       8 |  738 | `						PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  739 | `					}` |
|      35 |  740 | `					VmPopOperand(&pTos,1);` |
|       - |  741 | `					/* The VALUE stays on the stack: a store IS an expression. */` |
|      35 |  742 | `					VM_EXIT_BREAK;` |
|       - |  743 | `				}` |
|       8 |  744 | `			}` |
|       - |  745 | `			/* Otherwise: PHP throws a fatal Error rather` |
|       - |  746 | `			 * than silently coercing the object into a hashmap (which is` |
|       - |  747 | `			 * what the legacy PH7 fall-through would do via MemObjToHashmap` |
|       - |  748 | `			 * a few lines below). Match PHP -- and let a class whose READ` |
|       - |  749 | `			 * handler answers word its own refusal, which is how php's` |
|       - |  750 | ``			 * PDORow says `Cannot write to PDORow offset` and, for the`` |
|       - |  751 | ``			 * keyless spelling, `Cannot append to PDORow offset`. */`` |
|       - |  752 | `			{` |
|       - |  753 | `				char zMsg[256];` |
|      26 |  754 | `				sxu32 nMsg = PH7_ClassNativeDimRefusal(pInst,` |
|       8 |  755 | `					pKey == 0 ? PH7_NATIVE_DIM_APPEND : PH7_NATIVE_DIM_WRITE,` |
|       8 |  756 | `					zMsg,sizeof(zMsg));` |
|      18 |  757 | `				rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|      18 |  758 | `				if( pKey ){ PH7_MemObjRelease(pKey); }` |
|      18 |  759 | `				VmPopOperand(&pTos,2); /* container + value */` |
|      18 |  760 | `				if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      18 |  761 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  762 | `			}` |
|       - |  763 | `		}` |
|       - |  764 | `	}` |
|  614952 |  765 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       - |  766 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|       - |  767 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|       - |  768 | `		 * checking true sharing count, then re-add after separation. */` |
|  614426 |  769 | `		if( nIdx != SXU32_HIGH ){` |
|  614376 |  770 | `			ph7_value *pBacking = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
|  921674 |  771 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|  614376 |  772 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|       - |  773 | `				/* Only adjust refcount / perform COW if the backing variable` |
|       - |  774 | `				 * is still sharing the same hashmap instance. This mirrors` |
|       - |  775 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|       - |  776 | `				 * refcounts if the backing array was already separated. */` |
|  614376 |  777 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|  614376 |  778 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|  614376 |  779 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|  614376 |  780 | `					pMap->iRef++;  /* Re-add stack ref */` |
|  614376 |  781 | `					pTos->x.pOther = pMap;` |
|  307078 |  782 | `				}else{` |
|       - |  783 | `					/* Backing variable no longer points at pCur: skip COW here` |
|       - |  784 | `					 * and operate on the hashmap currently on the stack. */` |
|     ! 0 |  785 | `					pMap = pCur;` |
|       - |  786 | `				}` |
|  307078 |  787 | `			}else{` |
|     ! 0 |  788 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       - |  789 | `			}` |
|  307078 |  790 | `		}else{` |
|      51 |  791 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       - |  792 | `		}` |
|  614426 |  793 | `		if( pMap->iRef < 2 ){` |
|       - |  794 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|       - |  795 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|       - |  796 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|       - |  797 | `			 * no code checks iRef for COW decisions. */` |
|      49 |  798 | `			pMap->iRef = 2;` |
|      24 |  799 | `		}` |
|  307103 |  800 | `	}else{` |
|       - |  801 | `		ph7_value *pObj;` |
|     531 |  802 | `		pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
|     531 |  803 | `		if( pObj == 0 ){` |
|       - |  804 | `			/* No slot behind the container: this is a write THROUGH a TEMPORARY` |
|       - |  805 | ``			 * (`f()[0] = 5`, `f()[0][1] = 5`). php still screens the base's TYPE the`` |
|       - |  806 | `			 * same way it does for a variable — writing an index into an int, a` |
|       - |  807 | `			 * float, a resource or a bool is its catchable "Cannot use a scalar value` |
|       - |  808 | `			 * as an array" — and only the value it writes is discarded with the` |
|       - |  809 | `			 * temporary. PHL skipped the screen along with the write, so the whole` |
|       - |  810 | `			 * statement ran in silence. A NULL base keeps php's silence: the array it` |
|       - |  811 | `			 * vivifies into dies with the temporary, and so does an offset written` |
|       - |  812 | `			 * into a temporary STRING. */` |
|      27 |  813 | `			if( (pTos->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_RES\|MEMOBJ_BOOL)) != 0 ){` |
|       - |  814 | `				sxi32 rcSc;` |
|      13 |  815 | `				if( pKey ){` |
|      13 |  816 | `					PH7_MemObjRelease(pKey);` |
|       6 |  817 | `				}` |
|      13 |  818 | `				VmPopOperand(&pTos,1);` |
|      13 |  819 | `				rcSc = VmThrowFromVm(&(*pVm),"Error","Cannot use a scalar value as an array",` |
|       - |  820 | `					sizeof("Cannot use a scalar value as an array")-1);` |
|      13 |  821 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      13 |  822 | `				rc = rcSc;` |
|      21 |  823 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  824 | `			}` |
|      15 |  825 | `			if( pKey ){` |
|      15 |  826 | `			  PH7_MemObjRelease(pKey);` |
|       7 |  827 | `			}` |
|      15 |  828 | `			VmPopOperand(&pTos,1);` |
|      15 |  829 | `			VM_EXIT_BREAK;` |
|       - |  830 | `		}` |
|       - |  831 | `		/* Phase#1: Load the array */` |
|     505 |  832 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|     430 |  833 | `			VmPopOperand(&pTos,1);` |
|     430 |  834 | `			if( pKey == 0 ){` |
|       - |  835 | ``				/* `$s[] = 'x'` on a STRING: php raises the catchable Error`` |
|       - |  836 | `				 * "[] operator not supported for strings" and leaves the string` |
|       - |  837 | `				 * untouched. PHL silently APPENDED, so code that meant to build` |
|       - |  838 | `				 * an array from a variable holding a string quietly produced a` |
|       - |  839 | `				 * longer string instead of failing — a wrong answer, not a` |
|       - |  840 | `				 * missing diagnostic. */` |
|       - |  841 | `				SyBlob sErrMsg;` |
|       8 |  842 | `				SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       8 |  843 | `				SyBlobAppend(&sErrMsg,"[] operator not supported for strings",` |
|       - |  844 | `					sizeof("[] operator not supported for strings")-1);` |
|       8 |  845 | `				VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       8 |  846 | `				VM_EXIT_BREAK;` |
|     ! 0 |  847 | `			}else{` |
|     424 |  848 | `				sxi64 iOfft = 0;` |
|       - |  849 | `				SyBlob sTypeMsg;` |
|       - |  850 | `				/* php's offset rules run BEFORE the RHS is looked at: an offset it` |
|       - |  851 | `				 * refuses is the TypeError alone. The RHS cast below used to happen` |
|       - |  852 | ``				 * first, so every rejected shape — and `$s[] = [1,2]` above — came`` |
|       - |  853 | ``				 * with a spurious `Array to string conversion` in front of it. */`` |
|     424 |  854 | `				if( VmStringOffsetResolve(&(*pVm),pKey,VM_STROFF_LOUD,&iOfft,&sTypeMsg) == VM_STROFF_REJECT ){` |
|       - |  855 | `					sxi32 rcSc;` |
|       7 |  856 | `					PH7_MemObjRelease(pKey);` |
|       7 |  857 | `					rcSc = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|      16 |  858 | `					if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       7 |  859 | `					rc = rcSc;` |
|       7 |  860 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  861 | `				}` |
|       - |  862 | `				/* Force a string cast on the RHS (user-visible: an array warns` |
|       - |  863 | `				 * "Array to string conversion" before the offset write, and a` |
|       - |  864 | `				 * not-stringable object throws — the target string is untouched) */` |
|       - |  865 | `				{` |
|     418 |  866 | `					sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|     418 |  867 | `					if( rcSv != SXRET_OK ){` |
|       5 |  868 | `						PH7_MemObjRelease(pKey);` |
|       7 |  869 | `						PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|     ! 0 |  870 | `					}` |
|       - |  871 | `				}` |
|     414 |  872 | `				if( VmStringOffsetWrite(&(*pVm),pObj,iOfft,pTos) != SXRET_OK ){` |
|       - |  873 | `					sxi32 rcEm;` |
|       9 |  874 | `					PH7_MemObjRelease(pKey);` |
|       9 |  875 | `					rcEm = VmThrowFromVm(&(*pVm),"Error",` |
|       - |  876 | `						"Cannot assign an empty string to a string offset",` |
|       - |  877 | `						sizeof("Cannot assign an empty string to a string offset")-1);` |
|       9 |  878 | `					if( rcEm == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       9 |  879 | `					rc = rcEm;` |
|       9 |  880 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  881 | `				}` |
|       - |  882 | `			}` |
|     406 |  883 | `			if( pKey ){` |
|     406 |  884 | `			  PH7_MemObjRelease(pKey);` |
|     201 |  885 | `			}` |
|     406 |  886 | `			VM_EXIT_BREAK;` |
|      79 |  887 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       - |  888 | `			/* php: only NULL and FALSE auto-vivify into an array. Writing an index into an` |
|       - |  889 | `			 * int/float/resource/TRUE is a catchable Error -- PH7 quietly REPLACED the value` |
|       - |  890 | `			 * with an array, destroying it ($x = 5; $x[0] = 1; left $x === [1]). */` |
|       - |  891 | `			/* php auto-vivifies false into an array with an 8.1 DEPRECATION; PHL` |
|       - |  892 | `			 * rejects any scalar base, false included (null still auto-vivifies). */` |
|      79 |  893 | `			int bScalar = (pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_RES\|MEMOBJ_BOOL)) != 0;` |
|      79 |  894 | `			if( bScalar ){` |
|       - |  895 | `				sxi32 rcSc;` |
|      10 |  896 | `				if( pKey ){` |
|       7 |  897 | `					PH7_MemObjRelease(pKey);` |
|       3 |  898 | `				}` |
|      10 |  899 | `				VmPopOperand(&pTos,1);` |
|      10 |  900 | `				rcSc = VmThrowFromVm(&(*pVm),"Error","Cannot use a scalar value as an array",` |
|       - |  901 | `					sizeof("Cannot use a scalar value as an array")-1);` |
|      10 |  902 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      10 |  903 | `				rc = rcSc;` |
|      12 |  904 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  905 | `			}` |
|       - |  906 | `			/* Force a hashmap cast  */` |
|      71 |  907 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      71 |  908 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  909 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|     ! 0 |  910 | `				VM_EXIT_ABORT;` |
|       - |  911 | `			}` |
|      33 |  912 | `		}` |
|       - |  913 | `		/* COW separate the backing variable before mutation */` |
|      71 |  914 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|       - |  915 | `	}` |
|  614492 |  916 | `	VmPopOperand(&pTos,1);` |
|       - |  917 | `	/* Phase#2: Perform the insertion. A null key deprecates + normalizes to "" for` |
|       - |  918 | `	 * BOTH the by-value and the by-ref store — emitted HERE, after a null/false` |
|       - |  919 | ``	 * container has auto-vivified to an array, so `$x[null]=v` and `$x[null]=&$y` on`` |
|       - |  920 | `	 * an undefined $x get the notice too (the top-of-handler check runs before` |
|       - |  921 | `	 * vivification, when the base is not yet a hashmap). HashmapInsert /` |
|       - |  922 | `	 * HashmapInsertByRef both then cast NULL->"". A null pKey==0 (an append, no key)` |
|       - |  923 | `	 * is not a null OFFSET and is left alone. */` |
|  614492 |  924 | `	PH7_VmNullOffsetDeprecate(&(*pVm),pKey);` |
|  614487 |  925 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF` |
|  307197 |  926 | `	 && (pInstr->iP1 & PH7_STOREREF_CALLSRC)` |
|      70 |  927 | `	 && (pTos->iFlags & MEMOBJ_AUX_STROFFSET) == 0` |
|      13 |  928 | `	 && pTos->nIdx == SXU32_HIGH ){` |
|       - |  929 | ``		/* `$a[] =& f()` / `$a[$k] =& f()`: the source was written as a CALL and the`` |
|       - |  930 | `		 * callee did not return by reference, so php binds the value it answered and` |
|       - |  931 | ``		 * says so. Same notice the plain `$r =& f()` bind raises. */`` |
|       5 |  932 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|       - |  933 | `			"Only variables should be assigned by reference");` |
|       2 |  934 | `	}` |
|  614492 |  935 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && (pTos->iFlags & MEMOBJ_AUX_STROFFSET) ){` |
|       - |  936 | ``		/* `$a[] = &$s[1]`: the source is a string OFFSET, which php refuses to`` |
|       - |  937 | `		 * reference — and whose slot index is the BASE STRING's, so binding it` |
|       - |  938 | ``		 * aliased the whole string. Same Error the `=&` / by-ref-argument paths`` |
|       - |  939 | `		 * raise. The key is already popped; drop it and abandon the store. */` |
|       - |  940 | `		sxi32 rcSc;` |
|       5 |  941 | `		if( pKey ){` |
|       3 |  942 | `			PH7_MemObjRelease(pKey);` |
|       1 |  943 | `		}` |
|       5 |  944 | `		rcSc = VmThrowFromVm(&(*pVm),"Error","Cannot create references to/from string offsets",` |
|       - |  945 | `			sizeof("Cannot create references to/from string offsets")-1);` |
|       5 |  946 | `		if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       5 |  947 | `		rc = rcSc;` |
|       5 |  948 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  949 | `	}` |
|  614488 |  950 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|     128 |  951 | `		if( pMap == pVm->pGlobal ){` |
|       - |  952 | `			/* php 8.1: $GLOBALS['y'] =& $x binds the global $y to $x's` |
|       - |  953 | `			 * slot; an append has no name to bind (catchable Error). */` |
|      46 |  954 | `			if( pKey == 0 ){` |
|     ! 0 |  955 | `				rc = PH7_VmThrowGlobalsAppendError(&(*pVm));` |
|     ! 0 |  956 | `			}else{` |
|      46 |  957 | `				if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 |  958 | `					PH7_MemObjToString(pKey);` |
|     ! 0 |  959 | `				}` |
|      46 |  960 | `				if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|       - |  961 | `					/* Pathological empty name: keep the legacy diagnostic */` |
|     ! 0 |  962 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|       - |  963 | `						"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 |  964 | `					rc = SXRET_OK;` |
|     ! 0 |  965 | `				}else{` |
|      68 |  966 | `					rc = PH7_VmInstallGlobalVar(&(*pVm),` |
|      44 |  967 | `						(const char *)SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),` |
|      44 |  968 | `						0,pTos->nIdx);` |
|       - |  969 | `				}` |
|       - |  970 | `			}` |
|      24 |  971 | `		}else{` |
|       - |  972 | `			/* Insertion by reference */` |
|      83 |  973 | `			rc = PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|       - |  974 | `		}` |
|      66 |  975 | `	}else{` |
|  614364 |  976 | `		rc = PH7_HashmapInsert(pMap,pKey,pTos);` |
|       - |  977 | `	}` |
|  614488 |  978 | `	if( pKey ){` |
|  103572 |  979 | `		PH7_MemObjRelease(pKey);` |
|   51701 |  980 | `	}` |
|       - |  981 | `	/* An append onto the occupied saturated auto-index threw php's catchable` |
|       - |  982 | `	 * Error (PH7_VmThrowArrayNextIndexError) — dispatch it like any other` |
|       - |  983 | `	 * store-path throw. Plain failures (OOM) keep their existing routes. */` |
|  614488 |  984 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|  614482 |  985 | `	VM_EXIT_BREAK;` |
|     ! 0 |  986 | `	VM_EXIT_BREAK;` |
|  307675 |  987 | `}` |
|       - |  988 |  |
|       - |  989 | `/*` |
|       - |  990 | ` * OP_LOAD_CLOSURE: body moved verbatim from the OP_LOAD_CLOSURE arm of` |
|       - |  991 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  992 | ` */` |
|   23521 |  993 | `PH7_PRIVATE VmOpRc VmExecOpLoadClosure(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  994 | `{` |
|   23526 |  995 | `	ph7_value *pTos = pState->pTos;` |
|   23526 |  996 | `	ph7_value *pStack = pState->pStack;` |
|   23526 |  997 | `	VmInstr *aInstr = pState->aInstr;` |
|   23526 |  998 | `	sxi32 pc = pState->pc;` |
|       - |  999 | `	sxi32 rc;` |
|   11639 | 1000 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|   23526 | 1001 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|       - | 1002 | `	/* The function whose name the Closure object will wrap: a fresh per-instantiation` |
|       - | 1003 | `	 * copy for a real closure (built below), or the shared lambda function itself for a` |
|       - | 1004 | `	 * plain anonymous function with no captured environment. */` |
|   23526 | 1005 | `	ph7_vm_func *pTarget = pFunc;` |
|       - | 1006 | `	/* A no-capture lambda declared inside a class method is not VM_FUNC_CLOSURE` |
|       - | 1007 | `	 * (its env is empty), yet php still binds the creation-site class as its scope` |
|       - | 1008 | ``	 * so `self::`/private access inside the body works. Detect the enclosing class`` |
|       - | 1009 | `	 * here and route such a lambda through the per-instance closure path (a global` |
|       - | 1010 | `	 * no-capture lambda peeks NULL and stays a shared function). */` |
|   23526 | 1011 | `	ph7_class *pLoadScope = (pFunc->iFlags & VM_FUNC_CLOSURE) ? 0 : PH7_VmPeekDeclaringClass(pVm);` |
|   23526 | 1012 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) \|\| pLoadScope ){` |
|       - | 1013 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|       - | 1014 | `		ph7_vm_func *pClosure;` |
|       - | 1015 | `		char *zName;` |
|       - | 1016 | `		sxu32 mLen;` |
|       - | 1017 | `		sxu32 n;` |
|       - | 1018 | `		/* Create a new VM function */` |
|   23186 | 1019 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|       - | 1020 | `		/* Generate an unique closure name */` |
|   23186 | 1021 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|   23186 | 1022 | `		if( pClosure == 0 \|\| zName == 0){` |
|     ! 0 | 1023 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|     ! 0 | 1024 | `			VM_EXIT_ABORT;` |
|       - | 1025 | `		}` |
|   23186 | 1026 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|   23186 | 1027 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|     ! 0 | 1028 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|     ! 0 | 1029 | `		}` |
|       - | 1030 | `		/* Zero the stucture */` |
|   23186 | 1031 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|       - | 1032 | `		/* Perform a structure assignment on read-only items */` |
|   23186 | 1033 | `		pClosure->aArgs = pFunc->aArgs;` |
|   23186 | 1034 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|   23186 | 1035 | `		pClosure->aStatic = pFunc->aStatic;` |
|   23186 | 1036 | `		pClosure->iFlags = pFunc->iFlags;` |
|       - | 1037 | `		/* An in-class no-capture lambda routed here (pLoadScope set) must be a` |
|       - | 1038 | `		 * real closure so PH7_VmClassMemberAccess honors its pUserData scope. */` |
|   23186 | 1039 | `		pClosure->iFlags \|= VM_FUNC_CLOSURE;` |
|   23186 | 1040 | `		pClosure->pUserData = pFunc->pUserData;` |
|   23186 | 1041 | `		pClosure->sSignature = pFunc->sSignature;` |
|   23186 | 1042 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|   23186 | 1043 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|   23186 | 1044 | `		pClosure->aReturnUnion = pFunc->aReturnUnion;` |
|   23186 | 1045 | `		pClosure->sReturnTypeName = pFunc->sReturnTypeName;` |
|   23186 | 1046 | `		pClosure->bStrictTypes = pFunc->bStrictTypes;` |
|   23186 | 1047 | `		pClosure->nMaxStack = pFunc->nMaxStack;` |
|   23186 | 1048 | `		if( pClosure->pUserData == 0 ){` |
|       - | 1049 | `			/* Stamp the creation-site class scope (see PH7_VmPeekDeclaringClass):` |
|       - | 1050 | `			 * a closure made in a method — or in another closure, whose own stamp` |
|       - | 1051 | `			 * the peek reads — resolves self::/parent:: against it like php. */` |
|   23186 | 1052 | `			pClosure->pUserData = (void *)PH7_VmPeekDeclaringClass(pVm);` |
|   11469 | 1053 | `		}` |
|       - | 1054 | ``		/* Capture the creation-site late-static-binding class so `static::` inside`` |
|       - | 1055 | `		 * the closure body resolves like php (the "called class", which may differ` |
|       - | 1056 | `		 * from the declaring scope stamped above — e.g. a closure made in an` |
|       - | 1057 | `		 * inherited method). A closure made outside any class captures NULL. */` |
|   23186 | 1058 | `		pClosure->pLsbClass = (void *)PH7_VmPeekTopClass(pVm);` |
|       - | 1059 | `		/* Reflection descriptor fields (getDocComment/getAttributes/getStartLine/` |
|       - | 1060 | `		 * getEndLine/getFileName read the INSTANTIATED copy via $__fn) */` |
|   23186 | 1061 | `		pClosure->aAttrs = pFunc->aAttrs;` |
|   23186 | 1062 | `		pClosure->sDoc = pFunc->sDoc;` |
|   23186 | 1063 | `		pClosure->sFile = pFunc->sFile;` |
|   23186 | 1064 | `		pClosure->nLine = pFunc->nLine;` |
|   23186 | 1065 | `		pClosure->nEndLine = pFunc->nEndLine;` |
|       - | 1066 | ``		/* php's visible `{closure:...}` name is a property of the DECLARATION, so every`` |
|       - | 1067 | `		 * per-instantiation copy answers the same one (php has a single op_array here). */` |
|   23186 | 1068 | `		pClosure->sClosureName = pFunc->sClosureName;` |
|   23186 | 1069 | `		pClosure->sClosureScope = pFunc->sClosureScope;` |
|   23186 | 1070 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|       - | 1071 | `		/* Register the closure */` |
|   23186 | 1072 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|       - | 1073 | `		/* Set up closure environment */` |
|   23186 | 1074 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|   23186 | 1075 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|   58418 | 1076 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|       - | 1077 | `			ph7_value *pValue;` |
|   35237 | 1078 | `			pEnv = &aEnv[n];` |
|   35237 | 1079 | `			sEnv.sName  = pEnv->sName;` |
|   35237 | 1080 | `			sEnv.iFlags = pEnv->iFlags;` |
|   35237 | 1081 | `			sEnv.nLine = pEnv->nLine;` |
|   35237 | 1082 | `			sEnv.nIdx = SXU32_HIGH;` |
|   35237 | 1083 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|   35232 | 1084 | `			if( (sEnv.iFlags & (VM_FUNC_ARG_BY_REF\|VM_FUNC_ARG_IGNORE)) == VM_FUNC_ARG_BY_REF` |
|   18923 | 1085 | `			 && !(SyStringLength(&sEnv.sName) == sizeof("this")-1` |
|    1441 | 1086 | `				&& SyMemcmp(SyStringData(&sEnv.sName),"this",sizeof("this")-1) == 0) ){` |
|       - | 1087 | `				/* Capture by reference: bind the env entry to the variable's` |
|       - | 1088 | `				 * memory slot — creating a fresh null variable when missing,` |
|       - | 1089 | ``				 * as php does (`use (&$f)` before $f is assigned) — and pin`` |
|       - | 1090 | `				 * the slot past the creating frame's teardown so the closure` |
|       - | 1091 | `				 * can outlive its birth scope. The call-time env install` |
|       - | 1092 | `				 * aliases the name to this slot instead of copying a value. */` |
|    1917 | 1093 | `				pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,TRUE);` |
|    1917 | 1094 | `				if( pValue ){` |
|    1917 | 1095 | `					sEnv.nIdx = pValue->nIdx;` |
|    1917 | 1096 | `					VmPinMemObjSlot(pVm,pValue->nIdx);` |
|     952 | 1097 | `				}` |
|     957 | 1098 | `			}else{` |
|       - | 1099 | `				/* Standard pass by value */` |
|   33325 | 1100 | `				pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|   33325 | 1101 | `				if( pValue ){` |
|       - | 1102 | `					/* Copy imported value */` |
|   11096 | 1103 | `					PH7_MemObjStore(pValue,&sEnv.sValue);` |
|   27761 | 1104 | `				}else if( (sEnv.iFlags & (VM_FUNC_ARG_BY_REF\|VM_FUNC_ARG_IGNORE)) == 0` |
|   11124 | 1105 | `					&& !(SyStringLength(&sEnv.sName) == sizeof("this")-1` |
|     123 | 1106 | `						&& SyMemcmp(SyStringData(&sEnv.sName),"this",sizeof("this")-1) == 0) ){` |
|     251 | 1107 | `					if( pFunc->iFlags & VM_FUNC_ARROW ){` |
|       - | 1108 | `						/* An arrow function auto-captures free variables by value, but` |
|       - | 1109 | `						 * php does NOT capture one that is UNDEFINED at creation: the` |
|       - | 1110 | `						 * isolated body scope then simply has no such variable, so a` |
|       - | 1111 | `						 * read of it there raises the normal "Undefined variable"` |
|       - | 1112 | `						 * warning (and reflection's getClosureUsedVariables omits it).` |
|       - | 1113 | `						 * Skip installing the capture so the body READ — not the` |
|       - | 1114 | `						 * creation — warns, matching php. (A later assignment to the` |
|       - | 1115 | `						 * outer variable does not retro-capture: arrow scope is` |
|       - | 1116 | `						 * isolated.) An explicit by-value use() instead warns here and` |
|       - | 1117 | `						 * binds NULL, handled just below. */` |
|     241 | 1118 | `						continue;` |
|       - | 1119 | `					}` |
|       - | 1120 | ``					/* php reads a by-value `use ($q)` capture AT CLOSURE CREATION and`` |
|       - | 1121 | `					 * warns when the variable is undefined there (the by-ref form` |
|       - | 1122 | ``					 * `use (&$q)` above stays silent — it creates the binding). The`` |
|       - | 1123 | `					 * auto-injected $this (VM_FUNC_ARG_IGNORE) never warns. The` |
|       - | 1124 | `					 * capture still proceeds as NULL, as php does. php attributes the` |
|       - | 1125 | `					 * warning to the capture's own line (which can differ from the` |
|       - | 1126 | `					 * OP_LOAD_CLOSURE instruction line when the use-clause wraps), so` |
|       - | 1127 | `					 * borrow the recorded line for the emission and restore it. */` |
|      11 | 1128 | `					sxu32 nSavedLine = pVm->nCurLine;` |
|      11 | 1129 | `					if( sEnv.nLine ){` |
|      11 | 1130 | `						pVm->nCurLine = sEnv.nLine;` |
|       5 | 1131 | `					}` |
|      11 | 1132 | `					VmErrorFormat(pVm,PH7_CTX_WARNING,"Undefined variable $%z",&sEnv.sName);` |
|      11 | 1133 | `					pVm->nCurLine = nSavedLine;` |
|       5 | 1134 | `				}` |
|       - | 1135 | `			}` |
|       - | 1136 | `			/* Insert the imported variable */` |
|   35001 | 1137 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|   17359 | 1138 | `		}` |
|   23186 | 1139 | `		pTarget = pClosure;` |
|   11469 | 1140 | `	}` |
|       - | 1141 | `	/* Wrap the target function in a Closure object and push it. Its captured environment` |
|       - | 1142 | ``	 * (incl. any `$this`) stays in pTarget->aClosureEnv and is delivered by the normal call`` |
|       - | 1143 | `	 * path when the closure is dispatched by name. */` |
|   23526 | 1144 | `	pTos++;` |
|       - | 1145 | `	{` |
|   23526 | 1146 | `		ph7_class_instance *pCloObj = VmCreateClosure(pVm, &pTarget->sName, 0, 0);` |
|   23526 | 1147 | `		if( pCloObj ){` |
|       - | 1148 | `			/* The instance is born holding ONE reference and this stack slot is what` |
|       - | 1149 | `			 * holds it -- the same handover OP_NEW makes. Taking a second one here` |
|       - | 1150 | `			 * meant no Closure object ever reached zero: every closure expression a` |
|       - | 1151 | `			 * program evaluated leaked its object, its three slots and (through the` |
|       - | 1152 | `			 * object) the per-instantiation function body behind it. */` |
|   23526 | 1153 | `			pTos->x.pOther = pCloObj;` |
|   23526 | 1154 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|   11644 | 1155 | `		}else{` |
|       - | 1156 | `			/* OOM fallback: the name string is still a usable callable. */` |
|     ! 0 | 1157 | `			PH7_MemObjStringAppend(pTos, pTarget->sName.zString, pTarget->sName.nByte);` |
|       - | 1158 | `		}` |
|       - | 1159 | `	}` |
|   23526 | 1160 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1161 | `	VM_EXIT_BREAK;` |
|   11644 | 1162 | `}` |
|       - | 1163 |  |
|       - | 1164 |  |
|       - | 1165 | `/*` |
|       - | 1166 | `` * TRUE when this LOAD_IDX feeds a `??`: the coalesce test is the very next`` |
|       - | 1167 | `` * instruction. php evaluates the whole left operand of `??` in isset-context,`` |
|       - | 1168 | ` * so the access must stay SILENT and, when it misses, must yield NULL — an` |
|       - | 1169 | ``  * out-of-range string offset that yielded "" instead made `$s[99] ?? $d` `` |
|       - | 1170 | ` * evaluate to "" rather than $d, a wrong answer rather than a stray notice.` |
|       - | 1171 | ` */` |
| 1319934 | 1172 | `static int VmIdxFeedsCoalesce(const VmInstr *pInstr)` |
|       5 | 1173 | `{` |
| 1319939 | 1174 | `	return (pInstr+1)->iOp == PH7_OP_NULLC \|\| (pInstr+1)->iOp == PH7_OP_NULLC_JMP;` |
|       5 | 1175 | `}` |
|       - | 1176 | `/*` |
|       - | 1177 | `` * php's string-offset STORE, shared by `$s[i] = v` (OP_STORE_IDX) and`` |
|       - | 1178 | `` * `$s[i] ??= v` (OP_NULLC_STORE — `??=` is not an assign-op, so php performs a`` |
|       - | 1179 | ` * real offset write there): resolve iRawOfft against the string's CURRENT length` |
|       - | 1180 | ` * (a negative offset counts back from the end and, when it still lands before the` |
|       - | 1181 | `` * start, warns `Illegal string offset` and writes NOTHING), refuse an EMPTY`` |
|       - | 1182 | ` * replacement, warn when more than one byte was handed over, PAD WITH SPACES up` |
|       - | 1183 | ` * to the offset, then write the first byte.` |
|       - | 1184 | ` *` |
|       - | 1185 | ` * pVal must ALREADY be a string: that coercion is user-visible (it warns for an` |
|       - | 1186 | ` * array, throws for a not-stringable object) and stays with the callers, which` |
|       - | 1187 | ` * are the only places that can route a throw. Answers SXRET_OK when the store` |
|       - | 1188 | `` * happened or was skipped, and SXERR_INVALID for php's `Cannot assign an empty`` |
|       - | 1189 | `` * string to a string offset` Error — raised by the caller for the same reason.`` |
|       - | 1190 | ` */` |
|     450 | 1191 | `PH7_PRIVATE sxi32 VmStringOffsetWrite(ph7_vm *pVm,ph7_value *pStr,sxi64 iRawOfft,ph7_value *pVal)` |
|       4 | 1192 | `{` |
|     454 | 1193 | `	sxi64 nLen = (sxi64)SyBlobLength(&pStr->sBlob);` |
|     454 | 1194 | `	sxi64 iOfft = iRawOfft;` |
|       - | 1195 | `	const char *zVal;` |
|     454 | 1196 | `	if( iOfft < 0 ){` |
|       - | 1197 | `		/* php 7.1: a negative offset writes back from the end. */` |
|       9 | 1198 | `		iOfft += nLen;` |
|       9 | 1199 | `		if( iOfft < 0 ){` |
|       7 | 1200 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Illegal string offset %qd",iRawOfft);` |
|       7 | 1201 | `			return SXRET_OK;` |
|       - | 1202 | `		}` |
|       1 | 1203 | `	}` |
|     448 | 1204 | `	if( SyBlobLength(&pVal->sBlob) < 1 ){` |
|       - | 1205 | ``		/* php refuses to write NOTHING into an offset — `$s[2] = ""`, and the`` |
|       - | 1206 | ``		 * `= null` / `= false` that stringify to "" — where PHL silently ignored`` |
|       - | 1207 | `		 * the store. */` |
|      13 | 1208 | `		return SXERR_INVALID;` |
|       - | 1209 | `	}` |
|     436 | 1210 | `	zVal = (const char *)SyBlobData(&pVal->sBlob);` |
|     436 | 1211 | `	if( SyBlobLength(&pVal->sBlob) > 1 ){` |
|      10 | 1212 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|       - | 1213 | `			"Only the first byte will be assigned to the string offset");` |
|       4 | 1214 | `	}` |
|     436 | 1215 | `	if( iOfft >= nLen ){` |
|       - | 1216 | `		/* php PADS WITH SPACES up to the offset. PH7 simply appended the byte, so` |
|       - | 1217 | `		 * "abc" with [6]="Z" became "abcZ" rather than "abc   Z" -- a silently` |
|       - | 1218 | `		 * wrong string. */` |
|       - | 1219 | `		sxi64 nPad;` |
|     217 | 1220 | `		for( nPad = nLen ; nPad < iOfft ; ++nPad ){` |
|     175 | 1221 | `			SyBlobAppend(&pStr->sBlob," ",sizeof(char));` |
|      88 | 1222 | `		}` |
|      43 | 1223 | `		SyBlobAppend(&pStr->sBlob,(const void *)zVal,sizeof(char));` |
|      22 | 1224 | `	}else{` |
|     394 | 1225 | `		char *zData = (char *)SyBlobData(&pStr->sBlob);` |
|     394 | 1226 | `		zData[iOfft] = zVal[0];` |
|       - | 1227 | `	}` |
|     436 | 1228 | `	return SXRET_OK;` |
|     229 | 1229 | `}` |
|       - | 1230 | `/*` |
|       - | 1231 | `` * Does this LOAD_IDX feed a `??=` (rather than a plain `??`)? The compiler emits`` |
|       - | 1232 | ` * the LHS peek, then OP_NULLC_JMP, then the RHS, then OP_NULLC_STORE — so the` |
|       - | 1233 | ` * NULLC_JMP right after is what distinguishes the assigning form, whose store` |
|       - | 1234 | ` * still has to happen when the peek answers null.` |
|       - | 1235 | ` */` |
| 1319862 | 1236 | `static int VmIdxFeedsCoalesceAssign(const VmInstr *pInstr)` |
|       5 | 1237 | `{` |
| 1319867 | 1238 | `	return (pInstr+1)->iOp == PH7_OP_NULLC_JMP;` |
|       5 | 1239 | `}` |
|       - | 1240 | `/*` |
|       - | 1241 | `` * Arm the write-back half of `$o[$k] op= v` on an ArrayAccess container. The`` |
|       - | 1242 | ` * value offsetGet just answered goes into a fresh SCRATCH memobj that pTos` |
|       - | 1243 | ` * points at, so the compound-assign op computes IN that slot the way it would` |
|       - | 1244 | ` * in an ordinary variable; the pending entry then makes the op's tail dispatch` |
|       - | 1245 | `` * `offsetSet($k, computed)`. The KEY gets a reserved slot of its own (pIdx is`` |
|       - | 1246 | ` * released as soon as this opcode returns, and the write happens one opcode` |
|       - | 1247 | `` * later); an ABSENT key — `$o[] op= v` — is carried as NULL, which is the value`` |
|       - | 1248 | ` * php hands both accessors for that shape.` |
|       - | 1249 | ` *` |
|       - | 1250 | ` * A failed reservation simply leaves the value unarmed: pTos keeps its` |
|       - | 1251 | ` * no-slot temp and the op falls back to the pre-existing refusal.` |
|       - | 1252 | ` */` |
|      54 | 1253 | `static void VmDimRmwArm(` |
|       - | 1254 | `	ph7_vm *pVm,` |
|       - | 1255 | `	ph7_class_instance *pInst,` |
|       - | 1256 | `	ph7_value *pIdx,` |
|       - | 1257 | `	ph7_value *pTos,` |
|       - | 1258 | `	void *pOwnerStack,` |
|       - | 1259 | `	void *pInstrs,` |
|       - | 1260 | `	sxu32 nPc` |
|       - | 1261 | `	)` |
|       2 | 1262 | `{` |
|       - | 1263 | `	ph7_value *pSlot;` |
|       - | 1264 | `	sxu32 nScratch;` |
|       - | 1265 | `	sxu32 nKey;` |
|       - | 1266 | `	VmHookRmw sRmw;` |
|      56 | 1267 | `	pSlot = PH7_ReserveMemObj(&(*pVm));` |
|      56 | 1268 | `	if( pSlot == 0 ){` |
|     ! 0 | 1269 | `		return;` |
|       - | 1270 | `	}` |
|      56 | 1271 | `	nScratch = pSlot->nIdx;` |
|      56 | 1272 | `	pSlot = PH7_ReserveMemObj(&(*pVm));` |
|      56 | 1273 | `	if( pSlot == 0 ){` |
|     ! 0 | 1274 | `		VmHookRmwFreeScratch(&(*pVm),nScratch);` |
|     ! 0 | 1275 | `		return;` |
|       - | 1276 | `	}` |
|      56 | 1277 | `	nKey = pSlot->nIdx;` |
|       - | 1278 | `	/* Address both slots by index from here on. This guarded a reservation` |
|       - | 1279 | `	 * GROWING the aMemObj set under the pointer the first one handed back;` |
|       - | 1280 | `	 * redundant since P1 -- the pool's segments are fixed, so a slot's address` |
|       - | 1281 | `	 * never moves. Left for the harvest sweep. */` |
|      56 | 1282 | `	if( pIdx ){` |
|      56 | 1283 | `		PH7_MemObjStore(pIdx,pSlot);` |
|      27 | 1284 | `	}` |
|      56 | 1285 | `	pSlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nScratch);` |
|      56 | 1286 | `	if( pSlot == 0 ){` |
|     ! 0 | 1287 | `		VmHookRmwFreeScratch(&(*pVm),nKey);` |
|     ! 0 | 1288 | `		VmHookRmwFreeScratch(&(*pVm),nScratch);` |
|     ! 0 | 1289 | `		return;` |
|       - | 1290 | `	}` |
|      56 | 1291 | `	PH7_MemObjStore(pTos,pSlot);` |
|      56 | 1292 | `	sRmw.iKind = VM_HOOK_PEND_RMW_DIM;` |
|      56 | 1293 | `	sRmw.pThis = pInst;` |
|      56 | 1294 | `	sRmw.pAttr = 0;` |
|      56 | 1295 | `	sRmw.nBackIdx = nKey;` |
|      56 | 1296 | `	sRmw.nScratchIdx = nScratch;` |
|      56 | 1297 | `	SyBlobInit(&sRmw.sName,&pVm->sAllocator);` |
|      56 | 1298 | `	sRmw.pOwnerStack = pOwnerStack;` |
|      56 | 1299 | `	sRmw.pInstrs = pInstrs;` |
|      56 | 1300 | `	sRmw.nJmpPc = nPc;  /* the modify op ... */` |
|      56 | 1301 | `	sRmw.nPc = nPc;     /* ... is the whole window */` |
|      56 | 1302 | `	pInst->iRef++;` |
|      56 | 1303 | `	pTos->nIdx = nScratch;` |
|      56 | 1304 | `	SySetPut(&pVm->aHookRmw,(const void *)&sRmw);` |
|      29 | 1305 | `}` |
|       - | 1306 | `/*` |
|       - | 1307 | ` * Is this LOAD_IDX fetching the dimension for WRITING — php's BP_VAR_W / BP_VAR_RW` |
|       - | 1308 | ` * / BP_VAR_UNSET fetch, the one that asks the container for a slot to MODIFY` |
|       - | 1309 | ` * rather than a value to read?` |
|       - | 1310 | ` *` |
|       - | 1311 | ` * iP2 answers for most of it. The two shapes it cannot are the ones where the` |
|       - | 1312 | ` * fetch is compiled as a plain read and the NEXT instruction is what makes it a` |
|       - | 1313 | `` * write: binding a reference to the element (`$r = &$o[$k]`) and iterating it by`` |
|       - | 1314 | `` * reference (`foreach ($o[$k] as &$v)`). A compound assign is the reverse case —`` |
|       - | 1315 | ` * iP2 says write, but php compiles it to ASSIGN_DIM_OP, a read plus a write` |
|       - | 1316 | ` * through the container's own handlers (VmDimRmwArm), not a write FETCH.` |
|       - | 1317 | ` */` |
|    5324 | 1318 | `static int VmIdxFetchForWrite(const VmInstr *pInstr,sxi32 iP2)` |
|       5 | 1319 | `{` |
|    5329 | 1320 | `	const VmInstr *pNext = pInstr + 1;` |
|    5329 | 1321 | `	if( iP2 == 1 ){` |
|     191 | 1322 | `		return !VmNextIsCompoundAssign(pNext);` |
|       - | 1323 | `	}` |
|    5139 | 1324 | `	if( iP2 == VM_IDX_CTX_UNSET_BASE ){` |
|       - | 1325 | `		/* An INTERMEDIATE subscript of an unset chain: php fetches it for` |
|       - | 1326 | `		 * writing so the removal one level down can land. */` |
|       9 | 1327 | `		return 1;` |
|       - | 1328 | `	}` |
|    5131 | 1329 | `	if( iP2 == 0 ){` |
|    4847 | 1330 | `		if( pNext->iOp == PH7_OP_STORE_REF ){` |
|     ! 0 | 1331 | `			return 1;` |
|       - | 1332 | `		}` |
|    4847 | 1333 | `		if( pNext->iOp == PH7_OP_FOREACH_INIT && pNext->p3 ){` |
|       5 | 1334 | `			return (((ph7_foreach_info *)pNext->p3)->iFlags & PH7_4EACH_STEP_REF) != 0;` |
|       - | 1335 | `		}` |
|    2419 | 1336 | `	}` |
|    5127 | 1337 | `	return 0;` |
|    2667 | 1338 | `}` |
|       - | 1339 | `/*` |
|       - | 1340 | ` * Which fetch contexts may answer out of a WRITABLE container's own storage` |
|       - | 1341 | `` * (PH7_SplDimElemSlot, vm_builtin_spl.c) instead of through `offsetGet`?`` |
|       - | 1342 | ` *` |
|       - | 1343 | ` * The ones that ask for a VALUE and may go on to MODIFY it: a plain read — whose` |
|       - | 1344 | ` * result carries the element's slot exactly as an array element's does, which is` |
|       - | 1345 | ` * what lets a by-reference ARGUMENT bind it — a write-context fetch, and the` |
|       - | 1346 | `` * INTERMEDIATE step of an unset chain. isset()/empty()/`??`/`??=` must reach`` |
|       - | 1347 | ` * offsetExists, and the OUTERMOST unset must reach offsetUnset, so those keep the` |
|       - | 1348 | ` * accessor. (iP2 is the NORMALIZED context here: the deferred-argument record mode` |
|       - | 1349 | ` * has already become a plain read.)` |
|       - | 1350 | ` *` |
|       - | 1351 | ` * A COMPOUND assign is deliberately not one of them, which is why this asks` |
|       - | 1352 | `` * VmIdxFetchForWrite rather than testing iP2 == 1 itself: `$ao[k] op= v` is php's`` |
|       - | 1353 | ` * ASSIGN_DIM_OP on an OBJECT, and that one reads and writes through the accessors` |
|       - | 1354 | ` * whatever the read handler could have offered — a subclass overriding only` |
|       - | 1355 | `` * offsetSet sees its own method called for `+=` and not for `++`.`` |
|       - | 1356 | ` */` |
|     674 | 1357 | `static int VmDimFastFetchCtx(const VmInstr *pInstr,sxi32 iP2)` |
|       5 | 1358 | `{` |
|     679 | 1359 | `	return iP2 == 0 \|\| VmIdxFetchForWrite(pInstr,iP2);` |
|       5 | 1360 | `}` |
|       - | 1361 | `/*` |
|       - | 1362 | ` * Is this fetch asking the base to BE a container — the question php answers with` |
|       - | 1363 | `` * `Cannot use string offset as an array` when the base is a string offset?`` |
|       - | 1364 | ` *` |
|       - | 1365 | ` * Every context that reaches INTO the base to write, vivify or remove: the write` |
|       - | 1366 | ` * contexts (1, the read-modify-write among them) and both halves of an unset chain.` |
|       - | 1367 | `` * The LOOKUPS — a plain read, isset()/empty()/`??`, a destructure, a deferred`` |
|       - | 1368 | `` * argument — are php's own silence (`$x = $s[0][1]` reads a character out of a`` |
|       - | 1369 | `` * character) and are not this. Neither is the `??=` PEEK (3): it reads, and`` |
|       - | 1370 | `` * `$s[0][0] ??= 7` finds a character and never stores at all, so only the peek that`` |
|       - | 1371 | ` * comes back EMPTY is a write — that one is refused where the read lands.` |
|       - | 1372 | ` *` |
|       - | 1373 | ` * iP2 is the NORMALIZED context, so VM_IDX_CTX_RMW has already become 1.` |
|       - | 1374 | ` */` |
|      24 | 1375 | `static int VmIdxCtxIsContainerWrite(sxi32 iP2)` |
|       1 | 1376 | `{` |
|      25 | 1377 | `	return iP2 == 1 \|\| VM_IDX_IS_UNSET(iP2);` |
|       1 | 1378 | `}` |
|       - | 1379 | `/*` |
|       - | 1380 | `` * php's `Indirect modification of overloaded element of C has no effect`: the`` |
|       - | 1381 | ` * write-context fetch above landed on a container that answers with a COPY, so` |
|       - | 1382 | ` * whatever the rest of the expression writes is thrown away. php says so and` |
|       - | 1383 | ` * carries on.` |
|       - | 1384 | ` *` |
|       - | 1385 | ` * PHL had neither half. The notice was missing, and the copy was not a copy: a` |
|       - | 1386 | ` * userland offsetGet returns the container's own nested hashmap by COW, and` |
|       - | 1387 | ` * OP_STORE_IDX on a base with no slot index writes STRAIGHT INTO the shared map —` |
|       - | 1388 | `` * so `$o['a']['b'] = 9`, `$o['a'][] = 5` and `foreach ($o['a'] as &$v)` all`` |
|       - | 1389 | ` * modified the object php leaves untouched, silently. Separating the value here` |
|       - | 1390 | ` * is what makes the write land nowhere.` |
|       - | 1391 | ` *` |
|       - | 1392 | ` * php stays silent for an OBJECT, and so does this: an object is a handle, the` |
|       - | 1393 | ` * write through it is not lost, and nothing about it is indirect.` |
|       - | 1394 | ` */` |
|      52 | 1395 | `PH7_PRIVATE void PH7_VmOverloadedElemNotice(ph7_vm *pVm,ph7_class *pClass,ph7_value *pVal)` |
|       1 | 1396 | `{` |
|      53 | 1397 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      11 | 1398 | `		return;` |
|       - | 1399 | `	}` |
|      64 | 1400 | `	VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|       - | 1401 | `		"Indirect modification of overloaded element of %z has no effect",` |
|      21 | 1402 | `		&pClass->sDisp);` |
|      43 | 1403 | `	if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|      35 | 1404 | `		PH7_HashmapCowSeparate(&(*pVm),pVal);` |
|      17 | 1405 | `	}` |
|      27 | 1406 | `}` |
|       - | 1407 | `/*` |
|       - | 1408 | ` * OP_LOAD_IDX: body moved verbatim from the OP_LOAD_IDX arm of` |
|       - | 1409 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 1410 | ` */` |
| 1694711 | 1411 | `PH7_PRIVATE VmOpRc VmExecOpLoadIdx(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 1412 | `{` |
| 1694716 | 1413 | `	ph7_value *pTos = pState->pTos;` |
| 1694716 | 1414 | `	ph7_value *pStack = pState->pStack;` |
| 1694716 | 1415 | `	VmInstr *aInstr = pState->aInstr;` |
| 1694716 | 1416 | `	sxi32 pc = pState->pc;` |
|       - | 1417 | `	sxi32 rc;` |
|  851149 | 1418 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 1694716 | 1419 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
| 1694716 | 1420 | `	ph7_hashmap *pMap = 0;` |
|       - | 1421 | `	ph7_value *pIdx;` |
|       - | 1422 | `	/* D1 commit 2: iP2==9 is the deferred-record mode. It behaves exactly like a plain read` |
|       - | 1423 | `	 * (iP2==0) for base dispatch / the read tail, EXCEPT the dedicated block right after the` |
|       - | 1424 | `	 * index is popped, which — on a lookup MISS with a reachable base — captures the lvalue` |
|       - | 1425 | `	 * path (MEMOBJ_AUX_DEFPATH) instead of warning, and exits. Everything else sees iP2. */` |
|       - | 1426 | ``	/* A read-modify-write fetch (VM_IDX_CTX_RMW: `$a[k] += v`, `$a[k]++`) IS the`` |
|       - | 1427 | `	 * write context for everything below — it COW-separates, it vivifies a missing` |
|       - | 1428 | `	 * key, it refuses the same offset TYPES — so normalize it to 1 here and keep` |
|       - | 1429 | `	 * the single thing that separates the two: php READ the element first, so a key` |
|       - | 1430 | `	 * that was not there to read WARNS before it is created. */` |
| 1694716 | 1431 | `	int bRmwFetch = (pInstr->iP2 == VM_IDX_CTX_RMW);` |
| 1694716 | 1432 | `	int bRmwMiss = 0;` |
| 1694716 | 1433 | `	int bBaseStrOff = 0;` |
| 1694716 | 1434 | `	sxi32 iP2 = (pInstr->iP2 == 9) ? 0 : (bRmwFetch ? 1 : pInstr->iP2);` |
| 1694716 | 1435 | `	pIdx = 0;` |
| 1694716 | 1436 | `	if( pInstr->iP1 == 0 ){` |
|      52 | 1437 | `		if( !iP2){` |
|       - | 1438 | ``			/* `[]` with nothing to append INTO. Every placement php refuses is a compile`` |
|       - | 1439 | `			 * error now (compile.c), so the only shape that reaches here is the one php` |
|       - | 1440 | `			 * also settles at runtime: a call ARGUMENT, whose parameter may turn out to be` |
|       - | 1441 | `			 * by-reference (php appends and binds) or by-value (php's Error). Record the` |
|       - | 1442 | `			 * append as a step of the deferred lvalue path and let OP_CALL decide. */` |
|      13 | 1443 | `			if( pInstr->iP2 == 9 ){` |
|      13 | 1444 | `				VmDeferredPath *pPath = 0;` |
|      13 | 1445 | `				if( pTos->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|     ! 0 | 1446 | `					pPath = (VmDeferredPath *)pTos->x.pOther;` |
|     ! 0 | 1447 | `					if( VmDeferPathPushAppend(pPath) == SXRET_OK ){` |
|     ! 0 | 1448 | `						VM_EXIT_BREAK; /* carrier already on pTos */` |
|     ! 0 | 1449 | `					}` |
|      13 | 1450 | `				}else if( pTos->iFlags & MEMOBJ_AUX_DEFERRED ){` |
|       - | 1451 | `					SyString sRootName;` |
|       3 | 1452 | `					SyStringInitFromBuf(&sRootName,(const char *)pTos->x.pOther,` |
|       - | 1453 | `						pTos->x.pOther ? SyStrlen((const char *)pTos->x.pOther) : 0);` |
|       3 | 1454 | `					pPath = VmDeferPathNew(&(*pVm),1,SXU32_HIGH,&sRootName);` |
|       3 | 1455 | `					if( pPath && VmDeferPathPushAppend(pPath) == SXRET_OK ){` |
|       3 | 1456 | `						pTos->iFlags &= ~MEMOBJ_AUX_DEFERRED; /* borrowed name; don't free x.pOther */` |
|       3 | 1457 | `						pTos->x.pOther = pPath;` |
|       3 | 1458 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|       3 | 1459 | `						pTos->nIdx = SXU32_HIGH;` |
|       3 | 1460 | `						VM_EXIT_BREAK;` |
|       - | 1461 | `					}` |
|     ! 0 | 1462 | `					VmFreeDeferredPath(pPath);` |
|      11 | 1463 | `				}else if( pTos->nIdx != SXU32_HIGH ){` |
|      11 | 1464 | `					pPath = VmDeferPathNew(&(*pVm),0,pTos->nIdx,0);` |
|      11 | 1465 | `					if( pPath && VmDeferPathPushAppend(pPath) == SXRET_OK ){` |
|      11 | 1466 | `						PH7_MemObjRelease(pTos);` |
|      11 | 1467 | `						pTos->x.pOther = pPath;` |
|      11 | 1468 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|      11 | 1469 | `						pTos->nIdx = SXU32_HIGH;` |
|      11 | 1470 | `						VM_EXIT_BREAK;` |
|       - | 1471 | `					}` |
|     ! 0 | 1472 | `					VmFreeDeferredPath(pPath);` |
|     ! 0 | 1473 | `				}` |
|     ! 0 | 1474 | `			}` |
|       - | 1475 | `			/* Not a deferrable argument (or out of memory recording it): php's own` |
|       - | 1476 | `			 * Error, which replaced PH7's notice-and-NULL. */` |
|     ! 0 | 1477 | `			if( pTos >= pStack ){` |
|     ! 0 | 1478 | `				PH7_MemObjRelease(pTos);` |
|     ! 0 | 1479 | `			}else{` |
|       - | 1480 | `				/* TICKET 1433-020: Empty stack */` |
|     ! 0 | 1481 | `				pTos++;` |
|     ! 0 | 1482 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|     ! 0 | 1483 | `				pTos->nIdx = SXU32_HIGH;` |
|       - | 1484 | `			}` |
|       - | 1485 | `			{` |
|     ! 0 | 1486 | `			sxi32 rcRd = VmThrowFromVm(&(*pVm),"Error","Cannot use [] for reading",` |
|       - | 1487 | `				sizeof("Cannot use [] for reading")-1);` |
|     ! 0 | 1488 | `			if( rcRd == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     ! 0 | 1489 | `			rc = rcRd;` |
|     ! 0 | 1490 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1491 | `			}` |
|       - | 1492 | `		}` |
|      21 | 1493 | `	}else{` |
| 1694666 | 1494 | `		pIdx = pTos;` |
| 1694666 | 1495 | `		pTos--;` |
|       - | 1496 | `	}` |
| 1694704 | 1497 | `	if( VM_IDX_IS_UNSET(iP2) ){` |
|       - | 1498 | ``		/* `unset($o->p[$k])` reaches INTO what the property holds, which is php's`` |
|       - | 1499 | `		 * indirect modification of the property itself -- refused for a readonly` |
|       - | 1500 | `		 * one, and for a native property whose handler takes no write. The write` |
|       - | 1501 | ``		 * shapes (`$o->p[$k] = v`, `$o->p[] = v`) are screened at OP_MEMBER, which`` |
|       - | 1502 | `		 * knows them from its own context tag; an unset BASE is tagged as an` |
|       - | 1503 | `		 * ordinary read there and is only recognizable here. */` |
|    1743 | 1504 | `		sxi32 rcInd = PH7_VmCheckIndirectModify(&(*pVm),pTos->nIdx);` |
|    1743 | 1505 | `		if( rcInd != SXRET_OK ){` |
|       5 | 1506 | `			if( pIdx ){` |
|       5 | 1507 | `				PH7_MemObjRelease(pIdx);` |
|       2 | 1508 | `			}` |
|       5 | 1509 | `			PH7_MemObjRelease(pTos);` |
|       5 | 1510 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       5 | 1511 | `			pTos->nIdx = SXU32_HIGH;` |
|       5 | 1512 | `			if( rcInd == PH7_ABORT ){ VM_EXIT_ABORT; }` |
|       5 | 1513 | `			PH7_THROW_ROUTE_MIDEXPR(rcInd)` |
|       - | 1514 | `		}` |
|     867 | 1515 | `	}` |
| 1694700 | 1516 | `	if( pInstr->iP2 == 9 && pIdx ){` |
|       - | 1517 | `		/* D1 commit 2 record mode. Decide whether to CAPTURE this subscript as a deferred` |
|       - | 1518 | `		 * lvalue step (the by-ref/by-value decision is not known until OP_CALL), or fall` |
|       - | 1519 | `		 * through to a plain read (iP2 was normalized to 0 above). We defer only when the` |
|       - | 1520 | `		 * base can be reached again at resolve time: an existing descriptor (nested), the` |
|       - | 1521 | `		 * commit-1 undefined-variable marker, or a real container/string/scalar slot` |
|       - | 1522 | `		 * (nIdx != SXU32_HIGH). On a present array key we DO NOT defer — a hit already` |
|       - | 1523 | `		 * yields the aliasable read slot the by-ref binder needs. */` |
|  158393 | 1524 | `		VmDeferredPath *pPath = 0;` |
|  158393 | 1525 | `		int bDefer = 0, eRoot = 0;` |
|  158393 | 1526 | `		if( pTos->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|       - | 1527 | `			/* Nested: the base already carries a descriptor — extend it in place. */` |
|      25 | 1528 | `			pPath = (VmDeferredPath *)pTos->x.pOther;` |
|      25 | 1529 | `			bDefer = 1;` |
|  158382 | 1530 | `		}else if( pTos->iFlags & MEMOBJ_AUX_DEFERRED ){` |
|       - | 1531 | `			/* Undefined base variable (commit-1 marker): root the descriptor by name. */` |
|       - | 1532 | `			SyString sRootName;` |
|       9 | 1533 | `			SyStringInitFromBuf(&sRootName,(const char *)pTos->x.pOther,` |
|       - | 1534 | `				pTos->x.pOther ? SyStrlen((const char *)pTos->x.pOther) : 0);` |
|       9 | 1535 | `			pPath = VmDeferPathNew(&(*pVm),1,SXU32_HIGH,&sRootName);` |
|       9 | 1536 | `			pTos->iFlags &= ~MEMOBJ_AUX_DEFERRED; /* borrowed name; don't free x.pOther */` |
|       9 | 1537 | `			pTos->x.pOther = 0;` |
|       9 | 1538 | `			bDefer = (pPath != 0);` |
|  158368 | 1539 | `		}else if( pTos->nIdx != SXU32_HIGH ){` |
|       - | 1540 | `			/* A real base slot: array/scalar -> eRoot 0 (by-ref may vivify), string -> 2. */` |
|  154639 | 1541 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       - | 1542 | `				/* Probe hit/miss on a COPY of the key: PH7_HashmapLookup casts a NULL key to` |
|       - | 1543 | `				 * "" in place, which would suppress the fall-through read's null-offset` |
|       - | 1544 | `				 * deprecation (hit) and capture the wrong key in the step (miss). */` |
|       - | 1545 | `				ph7_value idxProbe;` |
|   71735 | 1546 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   71735 | 1547 | `				PH7_MemObjInit(&(*pVm),&idxProbe);` |
|   71735 | 1548 | `				PH7_MemObjStore(pIdx,&idxProbe);` |
|   71735 | 1549 | `				if( PH7_HashmapLookup(pMap,&idxProbe,&pNode) == SXRET_OK ){` |
|   71659 | 1550 | `					bDefer = 0; /* present key: fall through and read it as an aliasable slot */` |
|   35826 | 1551 | `				}else{` |
|      81 | 1552 | `					eRoot = 0; bDefer = 1;` |
|       - | 1553 | `				}` |
|   71735 | 1554 | `				PH7_MemObjRelease(&idxProbe);` |
|  118768 | 1555 | `			}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       - | 1556 | `				/* An ArrayAccess base splits the way php's read_dimension does. A WRITABLE` |
|       - | 1557 | `				 * container answers out of its own storage with no accessor call, so the` |
|       - | 1558 | `				 * fetch really can wait for the callee: deferring it is what lets a` |
|       - | 1559 | `				 * by-reference argument take php's WRITE fetch, which CREATES a missing key` |
|       - | 1560 | ``				 * (`sort($ao['nokey'])`) instead of warning about a read and passing NULL.`` |
|       - | 1561 | `				 * Everything else answers through a METHOD, and php runs that method where` |
|       - | 1562 | `				 * the subscript is WRITTEN — so the accessor runs below and its RESULT rides` |
|       - | 1563 | `				 * a prefetch carrier built at the tail of the ArrayAccess branch. */` |
|     174 | 1564 | `				ph7_class_instance *pRecInst = (ph7_class_instance *)pTos->x.pOther;` |
|     174 | 1565 | `				eRoot = 0;` |
|     344 | 1566 | `				bDefer = (pRecInst && pVm->pArrayAccessClass` |
|     170 | 1567 | `				       && PH7_VmInstanceOf(pRecInst->pClass,pVm->pArrayAccessClass)` |
|     255 | 1568 | `				       && PH7_VmDimFetchWritable(pRecInst->pClass)) ? 1 : 0;` |
|   82824 | 1569 | `			}else if( pTos->iFlags & MEMOBJ_STRING ){` |
|   82739 | 1570 | `				eRoot = 2; bDefer = 1;` |
|   42036 | 1571 | `			}else{` |
|     ! 0 | 1572 | `				eRoot = 0; bDefer = 1; /* NULL/other reachable scalar base */` |
|       - | 1573 | `			}` |
|  154639 | 1574 | `			if( bDefer && pPath == 0 ){` |
|   82891 | 1575 | `				pPath = VmDeferPathNew(&(*pVm),eRoot,pTos->nIdx,0);` |
|   82891 | 1576 | `				if( pPath == 0 ){` |
|     ! 0 | 1577 | `					bDefer = 0;` |
|     ! 0 | 1578 | `				}` |
|   42107 | 1579 | `			}` |
|   77975 | 1580 | `		}` |
|  158393 | 1581 | `		if( bDefer ){` |
|       - | 1582 | `			/* Append this element step (deep-copies pIdx) and leave the carrier on pTos. */` |
|   82919 | 1583 | `			if( VmDeferPathPushElem(pPath,pIdx) == SXRET_OK ){` |
|   82919 | 1584 | `				if( (pTos->iFlags & MEMOBJ_AUX_DEFPATH) == 0 ){` |
|       - | 1585 | `					/* Collapse the base value into the descriptor carrier. */` |
|   82897 | 1586 | `					PH7_MemObjRelease(pTos);` |
|   82897 | 1587 | `					pTos->x.pOther = pPath;` |
|   82897 | 1588 | `					pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|   82897 | 1589 | `					pTos->nIdx = SXU32_HIGH;` |
|   42110 | 1590 | `				}` |
|   82919 | 1591 | `				PH7_MemObjRelease(pIdx);` |
|   82919 | 1592 | `				VM_EXIT_BREAK;` |
|       - | 1593 | `			}` |
|       - | 1594 | `			/* Out of memory appending a step. */` |
|     ! 0 | 1595 | `			if( pTos->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|       - | 1596 | `				/* Nested base: pPath IS pTos's carrier and stays owned by it — keep it (a` |
|       - | 1597 | `				 * step short) and exit, rather than fall through and misread a NULL-typed` |
|       - | 1598 | `				 * carrier as an array base. Resolves the shorter path; OOM-only degradation. */` |
|     ! 0 | 1599 | `				PH7_MemObjRelease(pIdx);` |
|     ! 0 | 1600 | `				VM_EXIT_BREAK;` |
|       - | 1601 | `			}` |
|       - | 1602 | `			/* A freshly-allocated path we still own: drop it and fall back to a plain read. */` |
|     ! 0 | 1603 | `			VmFreeDeferredPath(pPath);` |
|     ! 0 | 1604 | `		}` |
|   37730 | 1605 | `	}` |
| 1611786 | 1606 | `	if( iP2 == 7 && (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|       - | 1607 | ``		/* Keyed list destructuring `["k"=>$v] = $src` from a NON-array source: yield NULL`` |
|       - | 1608 | `		 * (never char-index a string), warning once per key — matching PHP, which warns per` |
|       - | 1609 | `		 * key here. A NULL source is silent; unlike the positional OP_LOAD_LIST path, a bool` |
|       - | 1610 | `		 * source DOES warn (PHP warns for bool in keyed destructuring).` |
|       - | 1611 | `		 * An OBJECT is not one of these: php destructures it through its` |
|       - | 1612 | `		 * read_dimension handler like any other subscript, so it falls through to` |
|       - | 1613 | `		 * the object dispatch below — which answers out of the accessor and` |
|       - | 1614 | ``		 * raises php's `Cannot use object of type C as array` for a class that`` |
|       - | 1615 | ``		 * has none. `["k"=>$v] = $obj` warned and yielded NULL for every source`` |
|       - | 1616 | `		 * but the one shape (a writable container answering out of its own` |
|       - | 1617 | `		 * storage) that reached the fast path underneath. */` |
|       7 | 1618 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       5 | 1619 | `			VmWarnCannotUseAsArray(&(*pVm),pTos->iFlags);` |
|       2 | 1620 | `		}` |
|       7 | 1621 | `		if( pIdx ){` |
|       - | 1622 | `			/* Release the key (a string literal for keyed destructuring), like the` |
|       - | 1623 | `			 * normal hashmap-read exit below — otherwise its blob is orphaned. */` |
|       7 | 1624 | `			PH7_MemObjRelease(pIdx);` |
|       3 | 1625 | `		}` |
|       7 | 1626 | `		PH7_MemObjRelease(pTos);` |
|       7 | 1627 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|       7 | 1628 | `		VM_EXIT_BREAK;` |
|       - | 1629 | `	}` |
| 1611780 | 1630 | `	bBaseStrOff = (pTos->iFlags & MEMOBJ_AUX_STROFFSET) != 0;` |
| 1611780 | 1631 | `	if( bBaseStrOff && VmIdxCtxIsContainerWrite(iP2) ){` |
|       - | 1632 | `		/* A string OFFSET used as a CONTAINER. php reaches a string offset through a` |
|       - | 1633 | `		 * marker zval and lets whichever opcode CONSUMES it name the refusal` |
|       - | 1634 | ``		 * (`zend_wrong_string_offset_error`); a fetch that wants to reach INSIDE it —`` |
|       - | 1635 | ``		 * `$s[0][1] = 'x'`, `$s[0][1] += 1`, `unset($s[0][1])`,`` |
|       - | 1636 | ``		 * `$s[0][] = 'x'`, a by-reference argument — is `Cannot use string offset as`` |
|       - | 1637 | ``		 * an array`. PHL read the character and handed back a value still carrying the`` |
|       - | 1638 | ``		 * BASE STRING's slot, so the write landed on the base: `$s = 'ab';`` |
|       - | 1639 | ``		 * $s[0][1] = 'x';` left `$s === 'ax'` and `$a['k'][0][1] = 'x'` rewrote the`` |
|       - | 1640 | ``		 * ELEMENT. A READ (`$x = $s[0][1]`) and the lookup contexts are php's own`` |
|       - | 1641 | `		 * silence and stay out of this. */` |
|       9 | 1642 | `		if( pIdx ){` |
|       9 | 1643 | `			PH7_MemObjRelease(pIdx);` |
|       4 | 1644 | `		}` |
|       9 | 1645 | `		rc = VmThrowFromVm(&(*pVm),"Error","Cannot use string offset as an array",` |
|       - | 1646 | `			sizeof("Cannot use string offset as an array")-1);` |
|       9 | 1647 | `		PH7_MemObjRelease(pTos);` |
|       9 | 1648 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|       9 | 1649 | `		pTos->nIdx = SXU32_HIGH;` |
|       9 | 1650 | `		if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       9 | 1651 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1652 | `	}` |
| 1611772 | 1653 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|       - | 1654 | `		/* String access */` |
| 1319883 | 1655 | `		if( pIdx == 0 && VmIdxCtxIsContainerWrite(iP2) ){` |
|       - | 1656 | ``			/* `$s[] op= v` / `$s[]++`: php refuses an APPEND to a string wherever it`` |
|       - | 1657 | ``			 * lands — `[] operator not supported for strings` — and the plain`` |
|       - | 1658 | ``			 * `$s[] = 'x'` store already raises it (OP_STORE_IDX). The`` |
|       - | 1659 | `			 * read-modify-write spellings fell through to "load NULL" here and then` |
|       - | 1660 | `` 			 * wrote the computed value back through the BASE's slot, so `$s[] .= 'x'` `` |
|       - | 1661 | ``			 * APPENDED to the string and `$s[]++` incremented the whole of it. */`` |
|       5 | 1662 | `			rc = VmThrowFromVm(&(*pVm),"Error","[] operator not supported for strings",` |
|       - | 1663 | `				sizeof("[] operator not supported for strings")-1);` |
|       5 | 1664 | `			PH7_MemObjRelease(pTos);` |
|       5 | 1665 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       5 | 1666 | `			pTos->nIdx = SXU32_HIGH;` |
|       5 | 1667 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       5 | 1668 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1669 | `		}` |
| 1319879 | 1670 | `		if( iP2 == VM_IDX_CTX_UNSET ){` |
|       - | 1671 | ``			/* php: a string offset cannot be unset AT ALL — `unset($s[0])` is the`` |
|       - | 1672 | ``			 * catchable `Error: Cannot unset string offsets`, whatever the offset is`` |
|       - | 1673 | `			 * and whether or not it is in range. PHL read the character but left the` |
|       - | 1674 | `			 * BASE VARIABLE's slot index on the result, so the trailing unset()` |
|       - | 1675 | ``			 * builtin freed the base itself: `$s = "abc"; unset($s[1]);` left $s`` |
|       - | 1676 | ``			 * UNDEFINED, and `unset($a["k"][0])` deleted the whole element.`` |
|       - | 1677 | ``			 * An INTERMEDIATE level of the chain (`unset($s[0][1])`,`` |
|       - | 1678 | ``			 * `unset($s[0]->p)`) is NOT this: that fetch hands the offset on to`` |
|       - | 1679 | `			 * something that reaches INSIDE it, and php words the refusal from` |
|       - | 1680 | `			 * whatever that is — so it falls through to the offset resolution below` |
|       - | 1681 | `			 * (a write-shaped fetch) and the consumer raises. */` |
|      14 | 1682 | `			rc = VmThrowFromVm(&(*pVm),"Error","Cannot unset string offsets",` |
|       - | 1683 | `				sizeof("Cannot unset string offsets")-1);` |
|      14 | 1684 | `			if( pIdx ){` |
|      14 | 1685 | `				PH7_MemObjRelease(pIdx);` |
|       6 | 1686 | `			}` |
|      14 | 1687 | `			PH7_MemObjRelease(pTos);` |
|      14 | 1688 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      14 | 1689 | `			pTos->nIdx = SXU32_HIGH;` |
|      14 | 1690 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      14 | 1691 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1692 | `		}` |
| 1319867 | 1693 | `		if( pIdx ){` |
| 1319867 | 1694 | `			sxi64 iOfft = 0, iRaw;` |
| 1319867 | 1695 | `			sxi64 nLen = (sxi64)SyBlobLength(&pTos->sBlob);` |
|       - | 1696 | `			/* LOAD_IDX carries its OWN iP2 codes, which do NOT line up with the` |
|       - | 1697 | ``			 * PH7_MEMBER_* ones: 4 = isset, 5 = unset, 6 = empty, 8 = `??` read,`` |
|       - | 1698 | ``			 * 3 = `??=` peek. All are LOOKUPS, but php splits them into TWO`` |
|       - | 1699 | `			 * levels: isset()/empty()/unset() say nothing at all, while a` |
|       - | 1700 | ``			 * `??`/`??=` fetch still warns about the offset SHAPE and reads it`` |
|       - | 1701 | `			 * (VM_STROFF_COALESCE). The ??= peek is recognised by the NULLC_JMP` |
|       - | 1702 | `			 * that follows it, since its iP2 does not distinguish the base type. */` |
| 1982900 | 1703 | `			int iOfftLevel = (iP2 == 4 \|\| iP2 == VM_IDX_CTX_UNSET \|\| iP2 == 6) ? VM_STROFF_ISSET` |
| 2639652 | 1704 | `				: (iP2 == VM_IDX_CTX_UNSET_BASE ? VM_STROFF_UNSETBASE` |
| 2633301 | 1705 | `				: ((iP2 == 8 \|\| VmIdxFeedsCoalesce(pInstr)) ? VM_STROFF_COALESCE` |
| 1313491 | 1706 | `				: VM_STROFF_LOUD));` |
| 1319867 | 1707 | `			int bQuiet = iOfftLevel != VM_STROFF_LOUD;` |
|       - | 1708 | `			SyBlob sTypeMsg;` |
|       - | 1709 | `			int eOfft;` |
| 1319867 | 1710 | `			VmCoalStrOff *pCoalOff = 0;` |
| 1319867 | 1711 | `			if( VmIdxFeedsCoalesceAssign(pInstr) ){` |
|       - | 1712 | ``				/* `$s[k] ??= v`: the OP_NULLC_STORE ahead has to write into the`` |
|       - | 1713 | `				 * string OFFSET, and by then the offset is gone — this op consumes` |
|       - | 1714 | `				 * it. Carry the RAW index to the store on the peek's own result` |
|       - | 1715 | `				 * (MEMOBJ_AUX_COALSTROFF), which nests and cannot leak; the copy` |
|       - | 1716 | `				 * must predate the resolution below, which casts a float/null/bool` |
|       - | 1717 | `				 * in place, because php re-resolves the offset LOUDLY at the store:` |
|       - | 1718 | `				 * the peek is the quiet half of its pair. */` |
|      55 | 1719 | `				pCoalOff = VmCoalStrOffNew(&(*pVm),pIdx);` |
|      27 | 1720 | `			}` |
| 1319867 | 1721 | `			eOfft = VmStringOffsetResolve(&(*pVm),pIdx,iOfftLevel,&iOfft,&sTypeMsg);` |
| 1319867 | 1722 | `			if( eOfft == VM_STROFF_MISS ){` |
|       - | 1723 | `				/* A lookup over an offset php refuses: not set, in silence. */` |
|      32 | 1724 | `				PH7_MemObjRelease(pIdx);` |
|      32 | 1725 | `				PH7_MemObjRelease(pTos);` |
|      32 | 1726 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      32 | 1727 | `				if( pCoalOff && bBaseStrOff ){` |
|       - | 1728 | `					/* The store this peek is arming would land INSIDE a string offset:` |
|       - | 1729 | `					 * refused, like every other reach-inside (the same answer the` |
|       - | 1730 | `					 * out-of-range peek below gets). */` |
|     ! 0 | 1731 | `					VmFreeCoalStrOff(pCoalOff);` |
|     ! 0 | 1732 | `					rc = VmThrowFromVm(&(*pVm),"Error","Cannot use string offset as an array",` |
|       - | 1733 | `						sizeof("Cannot use string offset as an array")-1);` |
|     ! 0 | 1734 | `					pTos->nIdx = SXU32_HIGH;` |
|      35 | 1735 | `					if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     ! 0 | 1736 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1737 | `				}` |
|      32 | 1738 | `				if( pCoalOff ){` |
|       - | 1739 | ``					/* A `??=` whose offset the READ refuses: php raises at the`` |
|       - | 1740 | `					 * STORE instead, so keep the base slot reachable and hand the` |
|       - | 1741 | `					 * offset to OP_NULLC_STORE. */` |
|       3 | 1742 | `					pTos->x.pOther = (void *)pCoalOff;` |
|       3 | 1743 | `					pTos->iFlags \|= MEMOBJ_AUX_STROFFSET\|MEMOBJ_AUX_COALSTROFF;` |
|       2 | 1744 | `				}else{` |
|      30 | 1745 | `					pTos->nIdx = SXU32_HIGH;` |
|       - | 1746 | `				}` |
|      32 | 1747 | `				VM_EXIT_BREAK;` |
|       - | 1748 | `			}` |
| 1319837 | 1749 | `			if( eOfft == VM_STROFF_REJECT && iOfftLevel == VM_STROFF_UNSETBASE ){` |
|       - | 1750 | `				/* The intermediate step of an unset chain over an offset php cannot` |
|       - | 1751 | ``				 * use at all (`unset($s["k"][0])`, `unset($s[""]->p)`): the refusal`` |
|       - | 1752 | `				 * is the UNSET's, not the read's TypeError. */` |
|     ! 0 | 1753 | `				VmFreeCoalStrOff(pCoalOff);` |
|     ! 0 | 1754 | `				SyBlobRelease(&sTypeMsg);` |
|     ! 0 | 1755 | `				rc = VmThrowFromVm(&(*pVm),"Error","Cannot unset string offsets",` |
|       - | 1756 | `					sizeof("Cannot unset string offsets")-1);` |
|     ! 0 | 1757 | `				PH7_MemObjRelease(pIdx);` |
|     ! 0 | 1758 | `				PH7_MemObjRelease(pTos);` |
|     ! 0 | 1759 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|     ! 0 | 1760 | `				pTos->nIdx = SXU32_HIGH;` |
|     ! 0 | 1761 | `				if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     ! 0 | 1762 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1763 | `			}` |
| 1319837 | 1764 | `			if( eOfft == VM_STROFF_REJECT ){` |
|       - | 1765 | `				/* php's TypeError for an offset type a string refuses. PHL cast` |
|       - | 1766 | ``				 * every offset to int, so `$s[""]`, `$s["-"]` and `$s["p"]` all`` |
|       - | 1767 | ``				 * answered `$s[0]`. Routed as a mid-expression throw (this opcode`` |
|       - | 1768 | `				 * is not a call boundary), so the rest of the expression is` |
|       - | 1769 | `				 * abandoned the way php abandons it. */` |
|      41 | 1770 | `				VmFreeCoalStrOff(pCoalOff);` |
|      41 | 1771 | `				rc = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|      41 | 1772 | `				PH7_MemObjRelease(pIdx);` |
|      41 | 1773 | `				PH7_MemObjRelease(pTos);` |
|      41 | 1774 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      41 | 1775 | `				pTos->nIdx = SXU32_HIGH;` |
|      41 | 1776 | `				if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      41 | 1777 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1778 | `			}` |
| 1319799 | 1779 | `			iRaw = iOfft;` |
|       - | 1780 | `			/* php 7.1: a NEGATIVE offset counts back from the end ($s[-1] is the last` |
|       - | 1781 | `			 * character). The offset used to be cast to UNSIGNED, so -1 became a huge` |
|       - | 1782 | `			 * number, ran past the end and quietly produced NULL. */` |
| 1319799 | 1783 | `			if( iOfft < 0 ){` |
|      20 | 1784 | `				iOfft += nLen;` |
|       9 | 1785 | `			}` |
| 1319799 | 1786 | `			if( iOfft < 0 \|\| iOfft >= nLen ){` |
|       - | 1787 | `				/* Out of range. In an isset()/empty() lookup php answers FALSE, so load` |
|       - | 1788 | `				 * NULL there; everywhere else it WARNS and yields the empty string (PH7` |
|       - | 1789 | `				 * silently produced NULL in both cases). */` |
|      89 | 1790 | `				PH7_MemObjRelease(pTos);` |
|      89 | 1791 | `				if( bQuiet ){` |
|      70 | 1792 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      36 | 1793 | `				}else{` |
|      20 | 1794 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|      20 | 1795 | `					if( iP2 != 1 && iP2 != VM_IDX_CTX_UNSET_BASE ){` |
|       - | 1796 | `						/* A WRITE-context fetch never READS the character in php: it` |
|       - | 1797 | `						 * resolves the offset (loudly — the SHAPE diagnostics above are` |
|       - | 1798 | `						 * php's there too) and hands back its offset marker, and the` |
|       - | 1799 | ``						 * opcode that consumes it refuses. So `$s[5] .= 'x'` and`` |
|       - | 1800 | ``						 * `$s[5]++` are the assign-op / incr-decr Error with nothing`` |
|       - | 1801 | ``						 * said about offset 5, where PHL announced an `Uninitialized`` |
|       - | 1802 | ``						 * string offset 5` it never had to look at. */`` |
|      23 | 1803 | `						VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Uninitialized string offset %qd",` |
|       7 | 1804 | `							iRaw);` |
|       7 | 1805 | `					}` |
|       - | 1806 | `				}` |
|      46 | 1807 | `			}else{` |
| 1319713 | 1808 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
| 1319713 | 1809 | `				int c = zData[iOfft];` |
| 1319713 | 1810 | `				PH7_MemObjRelease(pTos);` |
| 1319713 | 1811 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
| 1319713 | 1812 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|       - | 1813 | `			}` |
| 1319799 | 1814 | `			if( pCoalOff ){` |
|      51 | 1815 | `				if( (pTos->iFlags & MEMOBJ_NULL) && bBaseStrOff ){` |
|       - | 1816 | ``					/* `$s[0][9] ??= v`: the peek came back empty, so the `??=` WILL`` |
|       - | 1817 | `					 * store — and the thing it would store into is a character inside a` |
|       - | 1818 | `					 * string offset, which php refuses like every other reach-inside.` |
|       - | 1819 | `					 * The refusal belongs to the store, which is why the peek that finds` |
|       - | 1820 | ``					 * a character (`$s[0][0] ??= 7`) short-circuits and says nothing at`` |
|       - | 1821 | `					 * all in php. */` |
|       3 | 1822 | `					VmFreeCoalStrOff(pCoalOff);` |
|       3 | 1823 | `					rc = VmThrowFromVm(&(*pVm),"Error","Cannot use string offset as an array",` |
|       - | 1824 | `						sizeof("Cannot use string offset as an array")-1);` |
|       3 | 1825 | `					PH7_MemObjRelease(pTos);` |
|       3 | 1826 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       3 | 1827 | `					pTos->nIdx = SXU32_HIGH;` |
|       3 | 1828 | `					if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       3 | 1829 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1830 | `				}` |
|      49 | 1831 | `				if( pTos->iFlags & MEMOBJ_NULL ){` |
|       - | 1832 | ``					/* Out of range: the `??=` will store, so hand the offset over. */`` |
|      41 | 1833 | `					pTos->x.pOther = (void *)pCoalOff;` |
|      41 | 1834 | `					pTos->iFlags \|= MEMOBJ_AUX_COALSTROFF;` |
|      21 | 1835 | `				}else{` |
|       - | 1836 | ``					/* A real byte: the `??=` short-circuits over the store. */`` |
|       9 | 1837 | `					VmFreeCoalStrOff(pCoalOff);` |
|       - | 1838 | `				}` |
|      24 | 1839 | `			}` |
|       - | 1840 | `			/* The result still carries the BASE VARIABLE's slot index, which is` |
|       - | 1841 | `			 * harmless for a plain read and WRONG for anything that would ALIAS it:` |
|       - | 1842 | `			 * a string offset is not a slot. Mark it so the reference-binding sites` |
|       - | 1843 | `			 * raise php's Error instead of aliasing the whole string --` |
|       - | 1844 | ``			 * `$r = &$s[1]; $r = "Z";` REPLACED $s with "Z". */`` |
| 1319797 | 1845 | `			pTos->iFlags \|= MEMOBJ_AUX_STROFFSET;` |
|  663048 | 1846 | `		}else{` |
|       - | 1847 | `			/* No available index,load NULL */` |
|     ! 0 | 1848 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       - | 1849 | `		}` |
| 1319797 | 1850 | `		VM_EXIT_BREAK;` |
|       - | 1851 | `	}` |
|  291894 | 1852 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       - | 1853 | `		/* Object subscript: ArrayAccess dispatch.` |
|       - | 1854 | `		 * iP2 codes:` |
|       - | 1855 | `		 *   0 = read       → offsetGet` |
|       - | 1856 | `		 *   3 = ??= peek   → offsetExists; offsetGet on hit; arm coalesce` |
|       - | 1857 | `		 *                    target on miss for the upcoming NULLC_STORE` |
|       - | 1858 | `		 *   4 = isset()    → offsetExists` |
|       - | 1859 | `		 *   5 = unset()    → offsetUnset` |
|       - | 1860 | `		 *   6 = empty()    → offsetExists, then offsetGet on hit` |
|       - | 1861 | ``		 *   8 = `??` read  → same probe as 6 (offsetExists, then offsetGet on a`` |
|       - | 1862 | `		 *                    hit) and no diagnostics anywhere in this op: php` |
|       - | 1863 | ``		 *                    evaluates the whole left operand of `??` in`` |
|       - | 1864 | `		 *                    isset-context, and calling offsetGet blindly also` |
|       - | 1865 | `		 *                    surfaced warnings raised INSIDE a userland` |
|       - | 1866 | `		 *                    offsetGet (e.g. ArrayObject's own array read). */` |
|    5479 | 1867 | `		ph7_class_instance *pInst = (ph7_class_instance *)pTos->x.pOther;` |
|    5479 | 1868 | `		ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|       - | 1869 | `		/* php's read_dimension / has_dimension HANDLERS, which a native class may` |
|       - | 1870 | ``		 * carry without implementing ArrayAccess -- `$list[0]` reads a DOMNodeList`` |
|       - | 1871 | ``		 * there while `$list instanceof ArrayAccess` is false. They come FIRST`` |
|       - | 1872 | `		 * because php's interface is implemented THROUGH the handler: a user` |
|       - | 1873 | `		 * subclass declaring ArrayAccess inherits the parent's handler, so its own` |
|       - | 1874 | `		 * offsetGet/offsetExists are not consulted for a READ. The WRITE half is` |
|       - | 1875 | `		 * not here at all -- a store, an append and an unset (iP2 5) fall past` |
|       - | 1876 | ``		 * this into php's `Cannot use object of type C as array` unless the class`` |
|       - | 1877 | `		 * really implements the interface, which is php's own split (that same` |
|       - | 1878 | `		 * subclass DOES get its offsetSet called). */` |
|    5479 | 1879 | `		if( pInst && iP2 != 5 && PH7_ClassHasNativeDim(pInst->pClass) ){` |
|       - | 1880 | `			PH7_NativeDimCtx sDim;` |
|       - | 1881 | `			ph7_value sResult;` |
|       - | 1882 | ``			/* `$o[$k] op= v` is php's read-then-WRITE pair, and php reports the`` |
|       - | 1883 | `			 * WRITE's refusal when the read answered nothing at all: an offset the` |
|       - | 1884 | ``			 * handler REFUSED comes back to `zend_binary_assign_op_obj_dim` as a`` |
|       - | 1885 | ``			 * miss, which raises `Cannot use object of type C as array` and chains`` |
|       - | 1886 | ``			 * the refusal behind it. `??=` is not that pair -- it reads in`` |
|       - | 1887 | `			 * isset-context, so its refusal is what surfaces -- and neither of them` |
|       - | 1888 | `			 * decides the store itself: that goes through the ordinary write path` |
|       - | 1889 | `			 * below, which is offsetSet for a subclass that has one. */` |
|    4773 | 1890 | `			int bRmwCtx = (iP2 == 1) && VmNextIsCompoundAssign(pInstr + 1);` |
|    4773 | 1891 | `			int bIsset = (iP2 == 4 \|\| iP2 == 6);` |
|    4773 | 1892 | `			PH7_MemObjInit(&(*pVm),&sResult);` |
|    4773 | 1893 | `			sDim.iMode = bIsset ? PH7_NATIVE_DIM_ISSET : PH7_NATIVE_DIM_READ;` |
|    4773 | 1894 | `			sDim.pOffset = pIdx;` |
|    4773 | 1895 | `			sDim.pResult = &sResult;` |
|    4773 | 1896 | `			sDim.zThrowClass = 0;` |
|    4773 | 1897 | `			sDim.zThrowMsg[0] = 0;` |
|    4773 | 1898 | `			sDim.bStored = 0;` |
|    4773 | 1899 | `			PH7_ClassNativeDim(pInst,&sDim);` |
|    4773 | 1900 | `			if( iP2 == 6 && sDim.zThrowClass == 0 && ph7_value_to_bool(&sResult) ){` |
|       - | 1901 | `				/* empty(): php asks has_dimension first and reads the VALUE only on` |
|       - | 1902 | ``				 * a hit, which is why an out-of-range `empty($map[-1])` is a plain`` |
|       - | 1903 | `				 * TRUE where the read of the same offset refuses. The emptiness` |
|       - | 1904 | `				 * question goes to the handler first -- a class that judges it on` |
|       - | 1905 | `				 * something other than the value it would HAND BACK answers here --` |
|       - | 1906 | `				 * and a handler that has no answer leaves the value read below. */` |
|       7 | 1907 | `				PH7_MemObjRelease(&sResult);` |
|       7 | 1908 | `				PH7_MemObjInit(&(*pVm),&sResult);` |
|       7 | 1909 | `				sDim.iMode = PH7_NATIVE_DIM_NOTEMPTY;` |
|       7 | 1910 | `				sDim.pResult = &sResult;` |
|       7 | 1911 | `				PH7_ClassNativeDim(pInst,&sDim);` |
|       7 | 1912 | `				if( (sResult.iFlags & MEMOBJ_NULL) && sDim.zThrowClass == 0 ){` |
|       7 | 1913 | `					sDim.iMode = PH7_NATIVE_DIM_READ;` |
|       7 | 1914 | `					sDim.pResult = &sResult;` |
|       7 | 1915 | `					PH7_ClassNativeDim(pInst,&sDim);` |
|       3 | 1916 | `				}` |
|       3 | 1917 | `			}` |
|    4773 | 1918 | `			if( sDim.zThrowClass ){` |
|       - | 1919 | `				char zMsg[256];` |
|     108 | 1920 | `				const char *zClass = sDim.zThrowClass;` |
|     108 | 1921 | `				const char *zText = sDim.zThrowMsg;` |
|       - | 1922 | `				sxu32 nMsg;` |
|     108 | 1923 | `				if( bRmwCtx ){` |
|       5 | 1924 | `					SyString *pName = &pInst->pClass->sName;` |
|       5 | 1925 | `					zClass = "Error";` |
|       5 | 1926 | `					zText = zMsg;` |
|       7 | 1927 | `					nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - | 1928 | `						"Cannot use object of type %.*s as array",` |
|       4 | 1929 | `						(int)pName->nByte,pName->zString);` |
|       3 | 1930 | `				}else{` |
|     104 | 1931 | `					nMsg = (sxu32)SyStrlen(zText);` |
|       - | 1932 | `				}` |
|     108 | 1933 | `				VmCoalesceDisarm(pVm);` |
|     108 | 1934 | `				rc = VmThrowFromVm(pVm,zClass,zText,nMsg);` |
|     108 | 1935 | `				PH7_MemObjRelease(&sResult);` |
|     108 | 1936 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|     108 | 1937 | `				PH7_MemObjRelease(pTos);` |
|     108 | 1938 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|     108 | 1939 | `				pTos->nIdx = SXU32_HIGH;` |
|     108 | 1940 | `				if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     228 | 1941 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1942 | `			}` |
|    4667 | 1943 | `			if( iP2 == 4 ){` |
|       - | 1944 | `				/* isset(): push a BOOL, which is also what keeps vm_builtin_isset` |
|       - | 1945 | `				 * from warning about a non-variable operand. */` |
|     104 | 1946 | `				int bExists = ph7_value_to_bool(&sResult);` |
|     104 | 1947 | `				PH7_MemObjRelease(&sResult);` |
|     104 | 1948 | `				PH7_MemObjRelease(pTos);` |
|     104 | 1949 | `				pTos->nIdx = SXU32_HIGH;` |
|     104 | 1950 | `				if( bExists ){` |
|      16 | 1951 | `					MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      16 | 1952 | `					pTos->x.iVal = 1;` |
|       9 | 1953 | `				}else{` |
|      90 | 1954 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       2 | 1955 | `				}` |
|    4616 | 1956 | `			}else if( iP2 == 3 && (sResult.iFlags & MEMOBJ_NULL) ){` |
|       - | 1957 | ``				/* `$o[$k] ??= v` and the read found nothing: arm (object, key) so`` |
|       - | 1958 | `				 * the NULLC_STORE that follows performs php's store -- offsetSet for` |
|       - | 1959 | ``				 * a subclass that declares one, and `Cannot use object of type C as`` |
|       - | 1960 | ``				 * array` for the collections themselves, which is the same verdict`` |
|       - | 1961 | ``				 * the plain `$o[$k] = v` gets. */`` |
|       5 | 1962 | `				VmCoalesceDisarm(pVm);` |
|       5 | 1963 | `				PH7_MemObjRelease(pTos);` |
|       5 | 1964 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|       5 | 1965 | `				pTos->nIdx = SXU32_HIGH;` |
|       5 | 1966 | `				if( pIdx ){` |
|       5 | 1967 | `					PH7_MemObjStore(pIdx,&pVm->sCoalesceKey);` |
|       2 | 1968 | `				}` |
|       5 | 1969 | `				pVm->pCoalesceObj = pInst;` |
|       5 | 1970 | `				pInst->iRef++;` |
|       5 | 1971 | `				pVm->bCoalesceArmed = 1;` |
|       5 | 1972 | `				PH7_MemObjRelease(&sResult);` |
|       3 | 1973 | `			}else{` |
|       - | 1974 | `				/* The base slot may be the only thing holding this instance, and the` |
|       - | 1975 | `				 * write-context tail below still speaks for its CLASS -- hold a` |
|       - | 1976 | `				 * reference across the release, as the ArrayAccess arm does. */` |
|    4561 | 1977 | `				pInst->iRef++;` |
|    4561 | 1978 | `				if( iP2 == 3 ){` |
|       3 | 1979 | `					VmCoalesceDisarm(pVm); /* a hit short-circuits over the store */` |
|       1 | 1980 | `				}` |
|    4561 | 1981 | `				PH7_MemObjRelease(pTos);` |
|    4561 | 1982 | `				PH7_MemObjStore(&sResult,pTos);` |
|    4561 | 1983 | `				pTos->nIdx = SXU32_HIGH;` |
|    4561 | 1984 | `				if( bRmwCtx ){` |
|       - | 1985 | `					/* php's ASSIGN_DIM_OP: the read gave the current value, the op` |
|       - | 1986 | `					 * computes on it, and the result goes back out through the write` |
|       - | 1987 | `					 * path -- offsetSet where there is one, php's Error where there` |
|       - | 1988 | `					 * is not (VmHookRmwConsume). */` |
|      20 | 1989 | `					VmDimRmwArm(&(*pVm),pInst,pIdx,pTos,` |
|      12 | 1990 | `						(void *)pStack,(void *)aInstr,(sxu32)(pc + 1));` |
|    4555 | 1991 | `				}else if( VmIdxFetchForWrite(pInstr,iP2) ){` |
|       - | 1992 | ``					/* php's `Indirect modification of overloaded element` -- silent`` |
|       - | 1993 | `					 * for an OBJECT, which is every value these containers answer,` |
|       - | 1994 | ``					 * and raised for the NULL a miss leaves (`$list[9]++`). */`` |
|       7 | 1995 | `					PH7_VmOverloadedElemNotice(&(*pVm),pInst->pClass,pTos);` |
|    4546 | 1996 | `				}else if( pInstr->iP2 == 9 ){` |
|       - | 1997 | `					/* A deferred call ARGUMENT: the read has happened, and whether php` |
|       - | 1998 | `					 * performed a W fetch is the callee's to say. Carry the value plus` |
|       - | 1999 | `					 * the class that answered it so the verdict lands at the call. */` |
|    5216 | 2000 | `					VmDeferredPath *pPre = VmDeferPathNewPrefetch(&(*pVm),VM_OVER_ELEM,` |
|    1737 | 2001 | `						pInst->pClass,0,pTos);` |
|    3479 | 2002 | `					if( pPre ){` |
|    3479 | 2003 | `						PH7_MemObjRelease(pTos);` |
|    3479 | 2004 | `						pTos->x.pOther = pPre;` |
|    3479 | 2005 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|    3479 | 2006 | `						pTos->nIdx = SXU32_HIGH;` |
|    1737 | 2007 | `					}` |
|    1737 | 2008 | `				}` |
|    4561 | 2009 | `				PH7_ClassInstanceUnref(pInst);` |
|    4561 | 2010 | `				PH7_MemObjRelease(&sResult);` |
|       - | 2011 | `			}` |
|    4667 | 2012 | `			if( pIdx ){` |
|    4667 | 2013 | `				PH7_MemObjRelease(pIdx);` |
|    2331 | 2014 | `			}` |
|    4667 | 2015 | `			VM_EXIT_BREAK;` |
|       - | 2016 | `		}` |
|     711 | 2017 | `		if( pArrayAccess && pInst && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|       - | 2018 | `			ph7_class_method *pMeth;` |
|       - | 2019 | `			ph7_value sResult;` |
|       - | 2020 | `			ph7_value sNullIdx;` |
|       - | 2021 | `			ph7_value *apArg[1];` |
|     676 | 2022 | `			if( pIdx && VmDimFastFetchCtx(pInstr,iP2) && PH7_VmDimFetchWritable(pInst->pClass)` |
|     283 | 2023 | `			 && pInst->iRef > 1 ){` |
|       - | 2024 | `				/* php hands a writable container's element back BY SLOT, and that is what` |
|       - | 2025 | ``				 * makes an indirect modification through it land. The `iRef > 1` guard is`` |
|       - | 2026 | ``				 * the object half of the array path's `pMap->iRef < 2` rule: releasing the`` |
|       - | 2027 | `				 * base below drops this stack slot's own reference, and a TEMPORARY` |
|       - | 2028 | ``				 * container (`(new ArrayObject([1]))[0]`) would be destroyed with its`` |
|       - | 2029 | `				 * storage while the result still views it. Such a base has nothing that` |
|       - | 2030 | `				 * could observe the write anyway, so it takes the accessor's copy. */` |
|     211 | 2031 | `				sxu32 nElem = PH7_SplDimElemSlot(&(*pVm),pInst,pIdx,` |
|       - | 2032 | `					/* php's write-context vivification, and only there: a W/RW fetch —` |
|       - | 2033 | ``					 * including the `$r = &$ao['k']` and by-ref-foreach shapes iP2 alone`` |
|       - | 2034 | `					 * cannot name — creates the missing element, while an unset chain's` |
|       - | 2035 | `					 * intermediate step never does. */` |
|     181 | 2036 | `					VmIdxFetchForWrite(pInstr,iP2) && !VM_IDX_IS_UNSET(iP2));` |
|     157 | 2037 | `				ph7_value *pElem = (nElem == SXU32_HIGH) ? 0` |
|     140 | 2038 | `					: (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nElem);` |
|     157 | 2039 | `				if( pElem ){` |
|     128 | 2040 | `					PH7_MemObjRelease(pTos);` |
|     128 | 2041 | `					PH7_MemObjLoad(pElem,pTos);` |
|     128 | 2042 | `					pTos->nIdx = nElem;` |
|     128 | 2043 | `					PH7_MemObjRelease(pIdx);` |
|     128 | 2044 | `					VM_EXIT_BREAK;` |
|       - | 2045 | `				}` |
|      14 | 2046 | `			}` |
|     555 | 2047 | `			if( (iP2 == 0 \|\| iP2 == 3 \|\| iP2 == 8) && pIdx == 0 ){` |
|       - | 2048 | ``				/* `$obj[]` read — PHP rejects this. */`` |
|     ! 0 | 2049 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|       - | 2050 | `					"Cannot use [] for reading");` |
|     ! 0 | 2051 | `				PH7_MemObjRelease(pTos);` |
|     ! 0 | 2052 | `				pTos->nIdx = SXU32_HIGH;` |
|     ! 0 | 2053 | `				VM_EXIT_BREAK;` |
|       - | 2054 | `			}` |
|     555 | 2055 | `			PH7_MemObjInit(&(*pVm),&sResult);` |
|     555 | 2056 | `			if( iP2 == 4 \|\| iP2 == 6 \|\| iP2 == 3 \|\| iP2 == 8 ){` |
|       - | 2057 | ``				/* isset, empty, ??= and `??` all start with offsetExists. */`` |
|     173 | 2058 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 2059 | `					"offsetExists",sizeof("offsetExists")-1);` |
|     173 | 2060 | `				apArg[0] = pIdx;` |
|     173 | 2061 | `				if( pMeth ){` |
|     173 | 2062 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|      89 | 2063 | `				}` |
|     471 | 2064 | `			}else if( iP2 == 5 ){` |
|      81 | 2065 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 2066 | `					"offsetUnset",sizeof("offsetUnset")-1);` |
|      81 | 2067 | `				apArg[0] = pIdx;` |
|      81 | 2068 | `				if( pMeth ){` |
|      81 | 2069 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|      38 | 2070 | `				}` |
|      43 | 2071 | `			}else{` |
|     311 | 2072 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 2073 | `					"offsetGet",sizeof("offsetGet")-1);` |
|     311 | 2074 | `				if( pIdx == 0 ){` |
|       - | 2075 | ``					/* `$o[] op= v` — the one read that reaches here without a key.`` |
|       - | 2076 | `					 * php hands the accessors NULL for the absent offset (its` |
|       - | 2077 | `					 * read_dimension substitutes one), so passing NO argument` |
|       - | 2078 | `					 * turned an assignment php performs into an` |
|       - | 2079 | `					 * ArgumentCountError against the class's own offsetGet. */` |
|       3 | 2080 | `					PH7_MemObjInit(&(*pVm),&sNullIdx);` |
|       3 | 2081 | `					pIdx = &sNullIdx;` |
|       1 | 2082 | `				}` |
|     311 | 2083 | `				apArg[0] = pIdx;` |
|     311 | 2084 | `				if( pMeth ){` |
|     311 | 2085 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|     153 | 2086 | `				}` |
|       - | 2087 | `			}` |
|     555 | 2088 | `			if( iP2 == 4 ){` |
|       - | 2089 | `				/* isset: push MEMOBJ_BOOL so vm_builtin_isset reports the` |
|       - | 2090 | `				 * right truth value AND skips its "Expecting a variable not` |
|       - | 2091 | `				 * a constant" warning (keyed on MEMOBJ_BOOL). */` |
|     119 | 2092 | `				int bExists = ph7_value_to_bool(&sResult);` |
|     119 | 2093 | `				PH7_MemObjRelease(pTos);` |
|     119 | 2094 | `				pTos->nIdx = SXU32_HIGH;` |
|     119 | 2095 | `				if( bExists ){` |
|      59 | 2096 | `					MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      59 | 2097 | `					pTos->x.iVal = 1;` |
|      32 | 2098 | `				}else{` |
|      65 | 2099 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       5 | 2100 | `				}` |
|     498 | 2101 | `			}else if( iP2 == 5 ){` |
|       - | 2102 | `				/* offsetUnset return is discarded; push NULL so the trailing` |
|       - | 2103 | `				 * vm_builtin_unset is a harmless no-op. */` |
|      81 | 2104 | `				PH7_MemObjRelease(pTos);` |
|      81 | 2105 | `				pTos->nIdx = SXU32_HIGH;` |
|      81 | 2106 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|     403 | 2107 | `			}else if( iP2 == 6 \|\| iP2 == 8 ){` |
|       - | 2108 | `				/* empty: if offsetExists is false, push NULL so empty=true` |
|       - | 2109 | `				 * without calling offsetGet. If true, call offsetGet and` |
|       - | 2110 | `				 * push the value so PH7_builtin_empty evaluates emptiness.` |
|       - | 2111 | ``				 * `??` (8) needs the identical shape: NULL on a miss so the`` |
|       - | 2112 | `				 * coalesce takes the default, the real value on a hit. */` |
|      50 | 2113 | `				int bExists = ph7_value_to_bool(&sResult);` |
|      50 | 2114 | `				PH7_MemObjRelease(&sResult);` |
|      50 | 2115 | `				PH7_MemObjRelease(pTos);` |
|      50 | 2116 | `				pTos->nIdx = SXU32_HIGH;` |
|      50 | 2117 | `				if( !bExists ){` |
|      24 | 2118 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      14 | 2119 | `				}else{` |
|      30 | 2120 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 2121 | `						"offsetGet",sizeof("offsetGet")-1);` |
|       - | 2122 | `					ph7_value sValue;` |
|      30 | 2123 | `					PH7_MemObjInit(&(*pVm),&sValue);` |
|      30 | 2124 | `					apArg[0] = pIdx;` |
|      30 | 2125 | `					if( pGet ){` |
|      30 | 2126 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|      13 | 2127 | `					}` |
|      30 | 2128 | `					PH7_MemObjStore(&sValue,pTos);` |
|      30 | 2129 | `					PH7_MemObjRelease(&sValue);` |
|       - | 2130 | `				}` |
|      50 | 2131 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|      50 | 2132 | `				VM_EXIT_BREAK; /* skip the duplicate sResult release below */` |
|     319 | 2133 | `			}else if( iP2 == 3 ){` |
|       - | 2134 | `				/* ?? null-coalesce peek: emulate PHP semantics —` |
|       - | 2135 | `				 *   if !offsetExists OR offsetGet() === null → arm` |
|       - | 2136 | `				 *     coalesce slot (NULLC_STORE will call offsetSet)` |
|       - | 2137 | `				 *     and push NULL.` |
|       - | 2138 | `				 *   else → push offsetGet's value (NULLC_JMP skips). */` |
|      10 | 2139 | `				int bExists = ph7_value_to_bool(&sResult);` |
|      10 | 2140 | `				int bShouldArm = !bExists;` |
|       - | 2141 | `				ph7_value sValue;` |
|      10 | 2142 | `				PH7_MemObjRelease(&sResult);` |
|       - | 2143 | `				/* Reset any prior arming defensively */` |
|      10 | 2144 | `				VmCoalesceDisarm(pVm);` |
|      10 | 2145 | `				PH7_MemObjInit(&(*pVm),&sValue);` |
|      10 | 2146 | `				if( bExists ){` |
|       5 | 2147 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 2148 | `						"offsetGet",sizeof("offsetGet")-1);` |
|       5 | 2149 | `					apArg[0] = pIdx;` |
|       5 | 2150 | `					if( pGet ){` |
|       5 | 2151 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|       2 | 2152 | `					}` |
|       5 | 2153 | `					if( sValue.iFlags & MEMOBJ_NULL ){` |
|       3 | 2154 | `						bShouldArm = 1;` |
|       1 | 2155 | `					}` |
|       2 | 2156 | `				}` |
|      10 | 2157 | `				PH7_MemObjRelease(pTos);` |
|      10 | 2158 | `				pTos->nIdx = SXU32_HIGH;` |
|      10 | 2159 | `				if( bShouldArm ){` |
|       - | 2160 | `					/* Arm: remember (object, key) so NULLC_STORE dispatches` |
|       - | 2161 | `					 * to offsetSet. Hold a ref on the instance to survive` |
|       - | 2162 | `					 * intervening expression evaluation. */` |
|       8 | 2163 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       8 | 2164 | `					if( pIdx ){` |
|       8 | 2165 | `						PH7_MemObjStore(pIdx,&pVm->sCoalesceKey);` |
|       3 | 2166 | `					}` |
|       8 | 2167 | `					pVm->pCoalesceObj = pInst;` |
|       8 | 2168 | `					pInst->iRef++;` |
|       8 | 2169 | `					pVm->bCoalesceArmed = 1;` |
|       5 | 2170 | `				}else{` |
|       3 | 2171 | `					PH7_MemObjStore(&sValue,pTos);` |
|       - | 2172 | `				}` |
|      10 | 2173 | `				PH7_MemObjRelease(&sValue);` |
|      10 | 2174 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|      10 | 2175 | `				VM_EXIT_BREAK;` |
|     ! 0 | 2176 | `			}else{` |
|       - | 2177 | `				/* offsetGet: replace pTos with the returned value.` |
|       - | 2178 | `				 *` |
|       - | 2179 | `				 * The base slot may be the only thing holding this instance — a` |
|       - | 2180 | ``				 * TEMPORARY container (`f((new C)['a'])`, a getter's return) dies with`` |
|       - | 2181 | `				 * it — and everything below still speaks for the object: the writable` |
|       - | 2182 | `				 * test, php's notice and the read-modify-write arming all read its` |
|       - | 2183 | `				 * CLASS, and the deferred-argument carrier records it. Hold a reference` |
|       - | 2184 | `				 * of our own across the release so none of them is left reading freed` |
|       - | 2185 | `				 * memory. */` |
|     311 | 2186 | `				pInst->iRef++;` |
|     311 | 2187 | `				PH7_MemObjRelease(pTos);` |
|     311 | 2188 | `				PH7_MemObjStore(&sResult,pTos);` |
|     311 | 2189 | `				pTos->nIdx = SXU32_HIGH;` |
|     311 | 2190 | `				if( iP2 == 1 && VmNextIsCompoundAssign(pInstr + 1) ){` |
|       - | 2191 | ``					/* `$o[$k] op= v` is php's ASSIGN_DIM_OP: offsetGet gave the`` |
|       - | 2192 | `					 * current value, the op computes on it, and the result goes` |
|       - | 2193 | `					 * back through offsetSet($k, …). PHL had no write-back at` |
|       - | 2194 | `					 * all here — the fetched value carried no slot, so every` |
|       - | 2195 | `					 * compound assign on an ArrayAccess element died on` |
|       - | 2196 | `					 * "Cannot perform assignment on a constant class attribute"` |
|       - | 2197 | `					 * and stored nothing. Arm the scratch slot the op mutates;` |
|       - | 2198 | `					 * its tail (PH7_HOOK_RMW_WRITEBACK) dispatches offsetSet. */` |
|      64 | 2199 | `					VmDimRmwArm(&(*pVm),pInst,pIdx,pTos,` |
|      42 | 2200 | `						(void *)pStack,(void *)aInstr,(sxu32)(pc + 1));` |
|     286 | 2201 | `				}else if( VmIdxFetchForWrite(pInstr,iP2)` |
|     151 | 2202 | `				       && !PH7_VmDimFetchWritable(pInst->pClass) ){` |
|      25 | 2203 | `					PH7_VmOverloadedElemNotice(&(*pVm),pInst->pClass,pTos);` |
|     257 | 2204 | `				}else if( pInstr->iP2 == 9 ){` |
|       - | 2205 | `					/* A deferred call ARGUMENT. The accessor has just run — php runs it` |
|       - | 2206 | `					 * where the subscript is written, whatever the parameter turns out to` |
|       - | 2207 | `					 * be — but WHICH fetch php performed is the callee's to say, and only` |
|       - | 2208 | `					 * OP_CALL knows: a by-reference parameter makes it a W fetch, which on` |
|       - | 2209 | `					 * a container that can only answer with a VALUE is php's` |
|       - | 2210 | ``					 * `Indirect modification of overloaded element` notice and a write`` |
|       - | 2211 | `					 * thrown away. Carry the result plus the class that answered it, so the` |
|       - | 2212 | `					 * verdict lands at the call without the accessor running twice or the` |
|       - | 2213 | `					 * argument arriving as NULL. The value would otherwise reach the callee` |
|       - | 2214 | `					 * still SHARING the container's own nested map by COW, and a by-ref` |
|       - | 2215 | ``					 * `f($o['a']['b'])` wrote straight into the object php leaves untouched. */`` |
|      49 | 2216 | `					VmDeferredPath *pPre = VmDeferPathNewPrefetch(&(*pVm),VM_OVER_ELEM,pInst->pClass,0,pTos);` |
|      49 | 2217 | `					if( pPre ){` |
|      49 | 2218 | `						PH7_MemObjRelease(pTos);` |
|      49 | 2219 | `						pTos->x.pOther = pPre;` |
|      49 | 2220 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|      49 | 2221 | `						pTos->nIdx = SXU32_HIGH;` |
|      24 | 2222 | `					}` |
|      24 | 2223 | `				}` |
|     311 | 2224 | `				PH7_ClassInstanceUnref(pInst);` |
|       - | 2225 | `			}` |
|     501 | 2226 | `			PH7_MemObjRelease(&sResult);` |
|     501 | 2227 | `			if( pIdx ){` |
|     501 | 2228 | `				PH7_MemObjRelease(pIdx);` |
|     248 | 2229 | `			}` |
|     501 | 2230 | `			VM_EXIT_BREAK;` |
|       - | 2231 | `		}` |
|       - | 2232 | `		/* Object without ArrayAccess: PHP throws fatal Error in all subscript` |
|       - | 2233 | `		 * contexts (read, isset, unset, empty). Match it. A class carrying a` |
|       - | 2234 | `		 * READ handler reaches here only for the unset (iP2 5), which the hook` |
|       - | 2235 | `		 * branch above skips, and words that refusal itself. */` |
|      32 | 2236 | `		if( pInst && iP2 == 5 ){` |
|       - | 2237 | `			/* unset($o[$k]) on a handler that really removes something: php's` |
|       - | 2238 | `			 * SimpleXMLElement drops the attribute or the element. Offered the` |
|       - | 2239 | `			 * access before the refusal below. */` |
|       - | 2240 | `			PH7_NativeDimCtx sDim;` |
|      22 | 2241 | `			if( PH7_ClassNativeDimStore(pInst,PH7_NATIVE_DIM_UNSET,pIdx,0,&sDim)` |
|      17 | 2242 | `			 && sDim.zThrowClass == 0 ){` |
|       7 | 2243 | `				if( pIdx ){` |
|       7 | 2244 | `					PH7_MemObjRelease(pIdx);` |
|       3 | 2245 | `				}` |
|       7 | 2246 | `				PH7_MemObjRelease(pTos);` |
|       7 | 2247 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|       7 | 2248 | `				pTos->nIdx = SXU32_HIGH;` |
|       7 | 2249 | `				VM_EXIT_BREAK;` |
|       - | 2250 | `			}` |
|       8 | 2251 | `		}` |
|      26 | 2252 | `		if( pInst ){` |
|       - | 2253 | `			char zMsg[256];` |
|      38 | 2254 | `			sxu32 nMsg = PH7_ClassNativeDimRefusal(pInst,` |
|      12 | 2255 | `				iP2 == 5 ? PH7_NATIVE_DIM_UNSET : PH7_NATIVE_DIM_WRITE,` |
|      12 | 2256 | `				zMsg,sizeof(zMsg));` |
|      26 | 2257 | `			rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|      26 | 2258 | `			if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|      26 | 2259 | `			PH7_MemObjRelease(pTos);` |
|      26 | 2260 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      26 | 2261 | `			pTos->nIdx = SXU32_HIGH;` |
|      26 | 2262 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       - | 2263 | ``			/* `break` used to resume at the NEXT instruction: the catch ran and then`` |
|       - | 2264 | `			 * execution carried on inside the try block. */` |
|      36 | 2265 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2266 | `		}` |
|     ! 0 | 2267 | `	}` |
|  286420 | 2268 | `	if( (iP2 == 1 \|\| iP2 == 3 \|\| VM_IDX_IS_UNSET(iP2)) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       - | 2269 | `		{` |
|       - | 2270 | `			/* The base's TYPE decides here, whether or not there is a SLOT behind it.` |
|       - | 2271 | ``			 * A write THROUGH a temporary — `f()[0] = 5`, `f()[0][1] = 5` — gets the`` |
|       - | 2272 | `			 * same verdict from php; the only difference is that what it writes is` |
|       - | 2273 | `			 * discarded afterwards. PHL skipped the whole screen when the base had no` |
|       - | 2274 | ``			 * slot, so `ui()[0] += 5` over an int RESULT ran in silence where php`` |
|       - | 2275 | `			 * throws. The temporary is screened and vivified in place, on the stack —` |
|       - | 2276 | `			 * there is nowhere to write it back to. */` |
|     105 | 2277 | `			ph7_value *pObj = (pTos->nIdx != SXU32_HIGH)` |
|      62 | 2278 | `				? (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pTos->nIdx)` |
|      37 | 2279 | `				: pTos;` |
|      71 | 2280 | `			if( pObj != 0 ){` |
|       - | 2281 | `				/* php 8 write-context auto-vivify rules: NULL converts to array` |
|       - | 2282 | `				 * silently; FALSE converts with the 8.1 deprecation; any other` |
|       - | 2283 | `				 * scalar base — int/float/true/resource — is php's catchable` |
|       - | 2284 | `				 * "Cannot use a scalar value as an array" Error and the variable` |
|       - | 2285 | `				 * stays untouched (pre-fix the base was silently CONVERTED,` |
|       - | 2286 | ``				 * corrupting e.g. `$i = 5; $i[0]++` into array(0 => 6); string`` |
|       - | 2287 | `				 * bases were intercepted by the string-offset paths above). */` |
|       - | 2288 | `				/* php auto-converts false to an array with an 8.1 DEPRECATION; PHL` |
|       - | 2289 | `				 * rejects it like any other scalar base (null still auto-vivifies —` |
|       - | 2290 | `				 * it is not a bool). */` |
|      71 | 2291 | `				if( (pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_RES\|MEMOBJ_BOOL)) != 0 ){` |
|       - | 2292 | `					/* unset() has its own wording for the same base: php's` |
|       - | 2293 | `					 * "Cannot unset offset in a non-array variable". */` |
|      14 | 2294 | `					const char *zErr = VM_IDX_IS_UNSET(iP2)` |
|       - | 2295 | `						? "Cannot unset offset in a non-array variable"` |
|      12 | 2296 | `						: "Cannot use a scalar value as an array";` |
|       - | 2297 | `					SyBlob sErrMsg;` |
|      18 | 2298 | `					SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      18 | 2299 | `					SyBlobAppend(&sErrMsg,zErr,(sxu32)SyStrlen(zErr));` |
|      18 | 2300 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      18 | 2301 | `					if( pIdx ){` |
|      18 | 2302 | `						PH7_MemObjRelease(pIdx);` |
|       8 | 2303 | `					}` |
|      18 | 2304 | `					PH7_MemObjRelease(pTos);` |
|      18 | 2305 | `					pTos->nIdx = SXU32_HIGH;` |
|      18 | 2306 | `					VM_EXIT_BREAK;` |
|       - | 2307 | `				}` |
|       - | 2308 | `				/* unset() CREATES NOTHING: a null/undefined base stays null where PHL` |
|       - | 2309 | ``				 * converted it to an empty array (`$n = null; unset($n[0]);` left`` |
|       - | 2310 | `				 * $n === []), which is also what materialised the missing intermediate` |
|       - | 2311 | ``				 * in `unset($a["y"]["z"])`. Leaving the base alone, the lookup below`` |
|       - | 2312 | `				 * misses, the tail loads NULL with no slot index, and the trailing` |
|       - | 2313 | `				 * unset() builtin is the no-op php's is. */` |
|      55 | 2314 | `				if( !VM_IDX_IS_UNSET(iP2) ){` |
|      49 | 2315 | `					PH7_MemObjToHashmap(pObj);` |
|      49 | 2316 | `					if( pObj != pTos ){` |
|      49 | 2317 | `						PH7_MemObjLoad(pObj,pTos);` |
|      23 | 2318 | `					}` |
|      23 | 2319 | `				}` |
|      26 | 2320 | `			}` |
|       - | 2321 | `		}` |
|      26 | 2322 | `	}` |
|  286404 | 2323 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|       - | 2324 | `	/* php DEPRECATES both a null offset and a lossy-float subscript (then normalizes` |
|       - | 2325 | `	 * "" / truncates). PHL now matches php on the NULL offset (deprecate + coerce to` |
|       - | 2326 | `	 * the "" key) but still rejects the lossy-FLOAT subscript with a TypeError on` |
|       - | 2327 | `	 * a READ or WRITE (iP2 0/1) — the recorded non-deprecated-surface policy. Both` |
|       - | 2328 | ``	 * stay lenient in isset()/empty()/`??`/unset(), where a throw would be wrong. */`` |
|       - | 2329 | `	/* An object/array key is rejected in EVERY context, including isset()/empty()/` |
|       - | 2330 | `	 * unset() where php still throws (only the wording changes) — unlike the` |
|       - | 2331 | `	 * null/float deprecations below, which stay lenient there. A resource key is` |
|       - | 2332 | `	 * accepted with a warning and becomes its integer id. */` |
|  286404 | 2333 | `	if( (pTos->iFlags & MEMOBJ_HASHMAP) && pIdx ){` |
|       - | 2334 | `		SyBlob sTypeMsg;` |
|  286252 | 2335 | `		if( VmOffsetTypeRejected(&(*pVm),pIdx,iP2,&sTypeMsg) ){` |
|       - | 2336 | `			/* Routed as a mid-expression throw, like the STRING arm above: this` |
|       - | 2337 | `			 * opcode is not a call boundary, so PARKING it let the rest of the` |
|       - | 2338 | ``			 * expression run first — `str_repeat($a[$obj], 2)` reached the builtin`` |
|       - | 2339 | `			 * with the NULL the abandoned read left and died on its ZPP TypeError` |
|       - | 2340 | `			 * instead, and a CAUGHT one still ran the enclosing call afterwards. */` |
|      21 | 2341 | `			rc = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|      21 | 2342 | `			PH7_MemObjRelease(pIdx);` |
|      21 | 2343 | `			PH7_MemObjRelease(pTos);` |
|      21 | 2344 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      21 | 2345 | `			pTos->nIdx = SXU32_HIGH;` |
|      30 | 2346 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      19 | 2347 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2348 | `		}` |
|  286232 | 2349 | `		PH7_VmOffsetResourceWarn(&(*pVm),pIdx);` |
|  143052 | 2350 | `	}` |
|  286384 | 2351 | `	if( (pTos->iFlags & MEMOBJ_HASHMAP) && pIdx ){` |
|       - | 2352 | `		/* php DEPRECATES a null offset in EVERY subscript context except unset()` |
|       - | 2353 | `		 * (iP2==5), then normalizes it to the "" key — read, write, isset (4),` |
|       - | 2354 | ``		 * empty (6), `??`/`??=` (3), the coalesce-chain probe (8), destructure`` |
|       - | 2355 | `		 * (2/7). Emit it here so all of them get it, not just read/write; the` |
|       - | 2356 | `		 * lookup/insert below casts NULL->"", and a plain read miss then warns` |
|       - | 2357 | ``		 * `Undefined array key ""` on the now-string pIdx (php's exact pair). */`` |
|  286232 | 2358 | `		if( (pIdx->iFlags & MEMOBJ_NULL) && !VM_IDX_IS_UNSET(iP2) ){` |
|      21 | 2359 | `			PH7_VmNullOffsetDeprecate(&(*pVm),pIdx);` |
|       9 | 2360 | `		}` |
|       - | 2361 | `		/* A lossy-FLOAT subscript stays a rejected TypeError on a READ or WRITE` |
|       - | 2362 | `		 * (iP2 0/1) — the recorded non-deprecated-surface policy; the lenient` |
|       - | 2363 | `		 * contexts (isset/empty/??/unset) truncate quietly as php's value does. */` |
|  286227 | 2364 | `		if( (iP2 == 0 \|\| iP2 == 1)` |
|  193562 | 2365 | `		 && (pIdx->iFlags & MEMOBJ_REAL)` |
|  143062 | 2366 | `		 && pIdx->rVal != (ph7_real)(sxi64)pIdx->rVal ){` |
|       - | 2367 | `			SyBlob sErrMsg;` |
|       6 | 2368 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       6 | 2369 | `			SyBlobAppend(&sErrMsg,"Cannot access offset of type float on array",` |
|       - | 2370 | `				sizeof("Cannot access offset of type float on array")-1);` |
|       6 | 2371 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|       6 | 2372 | `			PH7_MemObjRelease(pIdx);` |
|       6 | 2373 | `			PH7_MemObjRelease(pTos);` |
|       6 | 2374 | `			pTos->nIdx = SXU32_HIGH;` |
|       6 | 2375 | `			VM_EXIT_BREAK;` |
|       - | 2376 | `		}` |
|  143050 | 2377 | `	}` |
|  286380 | 2378 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|  286256 | 2379 | `		if( iP2 == 1 \|\| VM_IDX_IS_UNSET(iP2) ){` |
|       - | 2380 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|       - | 2381 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|       - | 2382 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|       - | 2383 | `			 * NOT separate — that would defeat COW on every element read.` |
|       - | 2384 | `			 * iP2=5 is unset-context, treated like iP2=1 for arrays so the` |
|       - | 2385 | `			 * trailing unset() builtin can drop the slot via pTos->nIdx. */` |
|    7469 | 2386 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|    3736 | 2387 | `		}` |
|       - | 2388 | `		/* Point to the hashmap */` |
|  286256 | 2389 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|  286256 | 2390 | `		if( pIdx ){` |
|       - | 2391 | `			/* Load the desired entry */` |
|  286228 | 2392 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|  143050 | 2393 | `		}` |
|  286256 | 2394 | `		if( iP2 == 3 ){` |
|       - | 2395 | `			/* Null coalescing assign peek mode: separate only when we will` |
|       - | 2396 | `			 * actually write back. If the looked-up value is non-null, the` |
|       - | 2397 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|       - | 2398 | `			 * the parent can stay shared. If the value is null or the key is` |
|       - | 2399 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|       - | 2400 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|       - | 2401 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|       - | 2402 | `			 * correct for the outermost write. */` |
|      27 | 2403 | `			int needWrite = (rc != SXRET_OK);` |
|      27 | 2404 | `			if( !needWrite && pNode ){` |
|      13 | 2405 | `				ph7_value *pVal = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pNode->nValIdx);` |
|      13 | 2406 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|       7 | 2407 | `					needWrite = 1;` |
|       3 | 2408 | `				}` |
|       6 | 2409 | `			}` |
|      27 | 2410 | `			if( needWrite ){` |
|      21 | 2411 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      21 | 2412 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|       - | 2413 | `					/* The map was actually copied — re-lookup so pNode points` |
|       - | 2414 | `					 * into the new map's storage. */` |
|       7 | 2415 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       7 | 2416 | `					if( pIdx ){` |
|       7 | 2417 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|       3 | 2418 | `					}` |
|       3 | 2419 | `				}` |
|      10 | 2420 | `			}` |
|      13 | 2421 | `		}` |
|       - | 2422 | `		/* iP2 == 5 (unset) is deliberately NOT here: php's unset() never creates` |
|       - | 2423 | `		 * the key it is about to remove, so a MISS must stay a miss. The` |
|       - | 2424 | `		 * insert-then-unset round trip was invisible on the LAST step but left` |
|       - | 2425 | ``		 * every INTERMEDIATE behind -- `unset($a["y"]["z"])` grew an empty`` |
|       - | 2426 | `		 * $a["y"]. The COW separation the unset context needs happened above and` |
|       - | 2427 | `		 * does not depend on this insert. */` |
|  286256 | 2428 | `		if( bRmwFetch && rc != SXRET_OK ){` |
|       - | 2429 | `			/* php's read half missed. Record it BEFORE the vivification below` |
|       - | 2430 | `			 * overwrites rc — the warning is about what was not there to read, and` |
|       - | 2431 | `			 * it is emitted after the slot exists, exactly as php does it. */` |
|      45 | 2432 | `			bRmwMiss = 1;` |
|      22 | 2433 | `		}` |
|  286256 | 2434 | `		if( rc != SXRET_OK && (iP2 == 1 \|\| iP2 == 3) ){` |
|       - | 2435 | `			/* Create a new empty entry */` |
|     222 | 2436 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|     222 | 2437 | `			if( rc == SXRET_OK ){` |
|       - | 2438 | `				/* Point to the last inserted entry */` |
|     220 | 2439 | `				pNode = pMap->pLast;` |
|     112 | 2440 | `			}else{` |
|       - | 2441 | ``				/* An append lvalue (`$a[][...] = v`) whose saturated auto-index`` |
|       - | 2442 | `				 * is occupied threw php's catchable Error. Dispatch it here —` |
|       - | 2443 | `				 * falling through with a stale pMap->pLast is what silently` |
|       - | 2444 | `				 * overwrote $a[PHP_INT_MAX]. */` |
|       7 | 2445 | `				PH7_DISPATCH_ENFORCE_RC(rc)` |
|       - | 2446 | `			}` |
|     108 | 2447 | `		}` |
|  143063 | 2448 | `	}` |
|  286373 | 2449 | `	if( pIdx && (bRmwMiss \|\| (rc != SXRET_OK && (iP2 == 2 \|\| iP2 == 7 \|\| iP2 == 0)))` |
|   70802 | 2450 | `	 && (pTos->iFlags & MEMOBJ_HASHMAP)` |
|     106 | 2451 | `	 && !VmIdxFeedsCoalesce(pInstr) ){` |
|       - | 2452 | ``		/* `$a['k'] ?? $d` compiles its LHS as a plain read (iP2 == 0) followed`` |
|       - | 2453 | `		 * by NULLC/NULLC_JMP — php does NOT warn there, so peek ahead and stay` |
|       - | 2454 | `		 * silent (same guard the magic-accessor read path uses). */` |
|       - | 2455 | `		/* php warns when a missing key is READ (iP2 == 0), destructured -- both` |
|       - | 2456 | `		 * positionally (iP2 == 2) and by KEY (iP2 == 7, which used to stay silent` |
|       - | 2457 | ``		 * where php says `Undefined array key "k"` for `['k' => $v] = []`) -- or`` |
|       - | 2458 | `		 * read by the READ half of a read-modify-write (bRmwMiss).` |
|       - | 2459 | `		 * isset/empty/??/unset (iP2 3-6) and plain write-context` |
|       - | 2460 | `		 * vivification (iP2 == 1) stay silent, as does a read on a non-array` |
|       - | 2461 | `		 * base (already diagnosed above). php prints an INT key bare and a` |
|       - | 2462 | `		 * STRING key quoted, and it decides which one the key IS by the same fold` |
|       - | 2463 | ``		 * the lookup just used: `$a["10"]` looked for the INTEGER key 10, so php`` |
|       - | 2464 | ``		 * says `Undefined array key 10`. PHL rendered the operand as WRITTEN --`` |
|       - | 2465 | ``		 * quoting every string and, worse, printing `$a[false]` as the "" key it`` |
|       - | 2466 | `		 * never looked in (false is the integer key 0). The canonical-numeric rule` |
|       - | 2467 | `		 * is the hashmap's own, so ask it rather than re-derive it. */` |
|       - | 2468 | `		SyBlob sMsg;` |
|     163 | 2469 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     163 | 2470 | `		if( (ph7_hashmap *)pTos->x.pOther == pVm->pGlobal ){` |
|       - | 2471 | `			/* $GLOBALS is the symbol table, so a key that is not there is a VARIABLE` |
|       - | 2472 | ``			 * that is not there, and php says so: `Undefined global variable $x`,`` |
|       - | 2473 | ``			 * with the subscript spelled RAW after the `$` ($GLOBALS[5] reads`` |
|       - | 2474 | ``			 * `$5`) rather than folded and quoted the way an array key is. Only the`` |
|       - | 2475 | `			 * LIVE map takes this wording — a copy of $GLOBALS is a by-value` |
|       - | 2476 | `			 * snapshot (memobj.c) and warns as the ordinary array it is. */` |
|       - | 2477 | `			SyString sName;` |
|       5 | 2478 | `			if( (pIdx->iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 2479 | `				PH7_MemObjToString(pIdx);` |
|     ! 0 | 2480 | `			}` |
|       5 | 2481 | `			SyStringInitFromBuf(&sName,SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob));` |
|       5 | 2482 | `			SyBlobFormat(&sMsg,"Undefined global variable $%z",&sName);` |
|     161 | 2483 | `		}else if( PH7_HashmapKeyIsInt(pIdx) ){` |
|      48 | 2484 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      15 | 2485 | `				PH7_MemObjToInteger(pIdx);` |
|       7 | 2486 | `			}` |
|      48 | 2487 | `			SyBlobFormat(&sMsg,"Undefined array key %qd",pIdx->x.iVal);` |
|      25 | 2488 | `		}else{` |
|       - | 2489 | `			/* Not an integer key: PH7_HashmapKeyIsInt left it a printable string. */` |
|       - | 2490 | `			SyString sKey;` |
|     113 | 2491 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob));` |
|     113 | 2492 | `			SyBlobFormat(&sMsg,"Undefined array key \"%z\"",&sKey);` |
|       - | 2493 | `		}` |
|     163 | 2494 | `		SyBlobNullAppend(&sMsg);` |
|     163 | 2495 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|     163 | 2496 | `		SyBlobRelease(&sMsg);` |
|      79 | 2497 | `	}` |
|  286487 | 2498 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0` |
|  143301 | 2499 | `	 && iP2 == 0` |
|      30 | 2500 | `	 && !VmIdxFeedsCoalesce(pInstr) ){` |
|       - | 2501 | `		/* Subscripting a scalar base is a WARNING in php ("Trying to access array offset` |
|       - | 2502 | `		 * on int") that yields NULL. PH7 yielded NULL in silence. (NOT for iP2 == 2, the` |
|       - | 2503 | `		 * fetch a NESTED destructuring level makes: php never subscripts there -- its` |
|       - | 2504 | `		 * outer list already answered that position with NULL and warned once for it --` |
|       - | 2505 | `		 * so a second sentence about the same byte is one php does not print.) */` |
|      23 | 2506 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Trying to access array offset on %s",` |
|       7 | 2507 | `			VmArithValueName(pTos));` |
|       7 | 2508 | `	}` |
|  286285 | 2509 | `	if( iP2 == VM_IDX_CTX_UNSET && rc == SXRET_OK && pNode != 0` |
|    1137 | 2510 | `	 && (pTos->iFlags & MEMOBJ_HASHMAP) ){` |
|       - | 2511 | ``		/* php's `unset($a[k])` removes the ELEMENT. PH7 left the element's value slot`` |
|       - | 2512 | `		 * on the stack and let the trailing unset() builtin drop it — but dropping a` |
|       - | 2513 | `` 		 * SLOT unlinks everything that holds it, so `$r = &$a['k']; unset($a['k']);` `` |
|       - | 2514 | ``		 * destroyed $r too (`Undefined variable $r`) where php leaves it reading the`` |
|       - | 2515 | `		 * value it still refers to. Unlink the node itself, which releases the value` |
|       - | 2516 | `		 * only when this element was its last holder, and leave the builtin nothing. */` |
|    1137 | 2517 | `		ph7_hashmap *pTarget = (ph7_hashmap *)pTos->x.pOther;` |
|    1137 | 2518 | `		int bDone = 0;` |
|    1137 | 2519 | `		if( pTarget == pVm->pGlobal && pIdx ){` |
|       - | 2520 | ``			/* `$GLOBALS['x']` IS the global $x, so this is an unset of the NAME: it has`` |
|       - | 2521 | `			 * to drop the symbol-table entry as well as this node, and it must not` |
|       - | 2522 | `			 * destroy the value another holder still refers to — exactly what` |
|       - | 2523 | ``			 * VmUnsetVarByName does for `unset($x)`. A superglobal has no entry in the`` |
|       - | 2524 | `			 * global frame and falls through to the plain node unlink below. */` |
|     161 | 2525 | `			VmFrame *pGlobalFrame = pVm->pFrame;` |
|       - | 2526 | `			SyHashEntry *pNameEntry;` |
|     161 | 2527 | `			while( pGlobalFrame->pParent ){` |
|     ! 0 | 2528 | `				pGlobalFrame = pGlobalFrame->pParent;` |
|     ! 0 | 2529 | `			}` |
|     161 | 2530 | `			if( (pIdx->iFlags & MEMOBJ_STRING) == 0 ){` |
|       3 | 2531 | `				PH7_MemObjToString(pIdx);` |
|       1 | 2532 | `			}` |
|     241 | 2533 | `			pNameEntry = SyHashGet(&pGlobalFrame->hVar,` |
|     160 | 2534 | `				(const void *)SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob));` |
|     161 | 2535 | `			if( pNameEntry ){` |
|     241 | 2536 | `				sxi32 rcUnset = VmUnsetVarByNameEx(&(*pVm),pGlobalFrame,` |
|     160 | 2537 | `					(const char *)SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob),FALSE);` |
|     161 | 2538 | `				bDone = 1;` |
|     161 | 2539 | `				if( rcUnset == PH7_ABORT ){` |
|     ! 0 | 2540 | `					PH7_MemObjRelease(pIdx);` |
|     ! 0 | 2541 | `					VM_EXIT_ABORT;` |
|       - | 2542 | `				}` |
|      80 | 2543 | `			}` |
|      80 | 2544 | `		}` |
|    1137 | 2545 | `		if( !bDone ){` |
|     977 | 2546 | `			PH7_HashmapUnlinkNode(pNode,TRUE);` |
|     486 | 2547 | `		}` |
|    1137 | 2548 | `		if( pIdx ){` |
|    1137 | 2549 | `			PH7_MemObjRelease(pIdx);` |
|     566 | 2550 | `		}` |
|    1137 | 2551 | `		PH7_MemObjRelease(pTos);` |
|    1137 | 2552 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|    1137 | 2553 | `		pTos->nIdx = SXU32_HIGH;` |
|    1137 | 2554 | `		VM_EXIT_BREAK;` |
|       - | 2555 | `	}` |
|  285158 | 2556 | `	if( pIdx ){` |
|  285132 | 2557 | `		PH7_MemObjRelease(pIdx);` |
|  142502 | 2558 | `	}` |
|  285158 | 2559 | `	if( rc == SXRET_OK ){` |
|       - | 2560 | `		/* Load entry contents */` |
|  143718 | 2561 | `		if( pMap->iRef < 2 ){` |
|       - | 2562 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|       - | 2563 | `			 * of the entry value,rather than pointing to it.` |
|       - | 2564 | `			 */` |
|    1236 | 2565 | `			pTos->nIdx = SXU32_HIGH;` |
|    1236 | 2566 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|     618 | 2567 | `		}else{` |
|  142487 | 2568 | `			pTos->nIdx = pNode->nValIdx;` |
|  142487 | 2569 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|  142487 | 2570 | `			PH7_HashmapUnref(pMap);` |
|       - | 2571 | `		}` |
|   71824 | 2572 | `	}else{` |
|       - | 2573 | `		/* No such entry,load NULL */` |
|  141445 | 2574 | `		PH7_MemObjRelease(pTos);` |
|  141445 | 2575 | `		pTos->nIdx = SXU32_HIGH;` |
|       - | 2576 | `	}` |
|  285158 | 2577 | `	if( iP2 == 4 && (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       - | 2578 | `		/* isset() context: reduce a found element to the same non-null marker the` |
|       - | 2579 | `		 * ArrayAccess arm above pushes. A TEMPORARY array (a call's return value,` |
|       - | 2580 | ``		 * or an accessor's -- `isset(f()['k'])`, `isset($o->magic['k'])`) leaves no`` |
|       - | 2581 | `		 * variable index behind, and the trailing builtin read that as a CONSTANT` |
|       - | 2582 | `		 * and warned about it; php's isset() is a language construct with no such` |
|       - | 2583 | `		 * diagnostic. */` |
|   40189 | 2584 | `		PH7_MemObjRelease(pTos);` |
|   40189 | 2585 | `		pTos->x.iVal = 1;` |
|   40189 | 2586 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   40189 | 2587 | `		pTos->nIdx = SXU32_HIGH;` |
|   20088 | 2588 | `	}` |
|  285158 | 2589 | `	VM_EXIT_BREAK;` |
|     ! 0 | 2590 | `	VM_EXIT_BREAK;` |
|  851066 | 2591 | `}` |
|       - | 2592 |  |
|       - | 2593 | `/*` |
|       - | 2594 | ` * OP_LOAD_MAP: body moved verbatim from the OP_LOAD_MAP arm of` |
|       - | 2595 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 2596 | ` */` |
|  312102 | 2597 | `PH7_PRIVATE VmOpRc VmExecOpLoadMap(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 2598 | `{` |
|  312107 | 2599 | `	ph7_value *pTos = pState->pTos;` |
|  312107 | 2600 | `	ph7_value *pStack = pState->pStack;` |
|  312107 | 2601 | `	VmInstr *aInstr = pState->aInstr;` |
|  312107 | 2602 | `	sxi32 pc = pState->pc;` |
|       - | 2603 | `	sxi32 rc;` |
|  155979 | 2604 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - | 2605 | `	ph7_hashmap *pMap;` |
|       - | 2606 | `	/* Allocate a new hashmap instance */` |
|  312107 | 2607 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|  312107 | 2608 | `	if( pMap == 0 ){` |
|     ! 0 | 2609 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|     ! 0 | 2610 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|     ! 0 | 2611 | `		VM_EXIT_ABORT;` |
|       - | 2612 | `	}` |
|  312107 | 2613 | `	if( pInstr->iP1 > 0 ){` |
|  203609 | 2614 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|  203609 | 2615 | `		sxi32 rcSpread = SXRET_OK;` |
|       - | 2616 | `		/* Perform the insertion */` |
|  477571 | 2617 | `		while( pEntry < pTos ){` |
|  274013 | 2618 | `			if( pEntry[1].iFlags & MEMOBJ_AUX_SPREAD ){` |
|       - | 2619 | `				/* Array unpacking: '...$expr'. Merge entries with PHP 8.1` |
|       - | 2620 | `				 * semantics — string keys preserved (later wins), int keys` |
|       - | 2621 | `				 * renumbered. Same routine that backs array_merge. */` |
|     754 | 2622 | `				if( pEntry[1].iFlags & MEMOBJ_HASHMAP ){` |
|     692 | 2623 | `					sxi32 rcMerge = PH7_HashmapMerge((ph7_hashmap *)pEntry[1].x.pOther,pMap);` |
|     692 | 2624 | `					if( rcMerge != SXRET_OK ){` |
|       - | 2625 | `						/* Merge failure (OOM): match the PH7_NewHashmap OOM` |
|       - | 2626 | `						 * path — emit fatal and abort, leaving no partial` |
|       - | 2627 | `						 * map dangling. */` |
|     ! 0 | 2628 | `						VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|     ! 0 | 2629 | `							"Fatal, PH7 engine is running out of memory while spreading array at instruction #:%d",pc);` |
|     ! 0 | 2630 | `						rcSpread = PH7_ABORT;` |
|     ! 0 | 2631 | `						break;` |
|       4 | 2632 | `					}` |
|     409 | 2633 | `				}else if( VmValueIsTraversable(pVm,&pEntry[1]) ){` |
|       - | 2634 | `					/* Traversable unpacking (PHP 8.1): walk it into the map using the` |
|       - | 2635 | `					 * same key rules as array spread (string keys kept, int renumbered). */` |
|      35 | 2636 | `					sxi32 rcW = PH7_VmIteratorWalk(&(*pVm),&pEntry[1],VmSpreadMergeStep,pMap);` |
|      35 | 2637 | `					if( rcW == PH7_EXCEPTION \|\| rcW == PH7_ABORT ){` |
|      18 | 2638 | `						rcSpread = rcW;` |
|      18 | 2639 | `						break;` |
|       - | 2640 | `					}` |
|      11 | 2641 | `				}else{` |
|       - | 2642 | `					/* Throw a catchable Error matching PHP semantics. */` |
|      33 | 2643 | `					rcSpread = VmThrowSpreadError(&(*pVm),&pEntry[1],0);` |
|      33 | 2644 | `					break;` |
|       4 | 2645 | `				}` |
|  273615 | 2646 | `			}else if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|       - | 2647 | `				/* Insertion by reference */` |
|     305 | 2648 | `				PH7_HashmapInsertByRef(pMap,` |
|     202 | 2649 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     202 | 2650 | `					(sxu32)pEntry[1].x.iVal` |
|       - | 2651 | `					);` |
|     103 | 2652 | `			}else{` |
|       - | 2653 | `				/* An explicit key in an array LITERAL gets the same php diagnostics a` |
|       - | 2654 | `				 * subscript does — a float key that truncates deprecates, and so does an` |
|       - | 2655 | `				 * explicit null key. Only the subscript sites (LOAD_IDX/STORE_IDX) used to` |
|       - | 2656 | ``				 * emit these, so `[1.5 => "v"]` and `[null => "v"]` were silent. A key that`` |
|       - | 2657 | `				 * is ABSENT (auto-index) is a NULL slot here, not a null key, so the` |
|       - | 2658 | `				 * MEMOBJ_NULL check below is what tells the two apart. */` |
|  273061 | 2659 | `					if( (pEntry->iFlags & MEMOBJ_AUX_NOKEY) == 0 ){` |
|       - | 2660 | `						/* An object/array literal key is php's TypeError, a resource one` |
|       - | 2661 | `						 * warns and becomes its id — same rules as a subscript. */` |
|       - | 2662 | `						SyBlob sTypeMsg;` |
|  185673 | 2663 | `						if( VmOffsetTypeRejected(&(*pVm),pEntry,0,&sTypeMsg) ){` |
|       3 | 2664 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg));` |
|       2 | 2665 | `						}else{` |
|  185671 | 2666 | `							PH7_VmOffsetResourceWarn(&(*pVm),pEntry);` |
|       - | 2667 | `						}` |
|       - | 2668 | `						/* php DEPRECATES a null literal key (then normalizes to "") and` |
|       - | 2669 | `						 * rejects nothing there; PHL matches that (deprecate + fall` |
|       - | 2670 | `						 * through, PH7_HashmapInsert casts NULL->""). A lossy-FLOAT` |
|       - | 2671 | `						 * literal key still rejects with a TypeError — the recorded` |
|       - | 2672 | `						 * non-deprecated-surface policy, same as the subscript site. */` |
|  185673 | 2673 | `						int bNull = (pEntry->iFlags & MEMOBJ_NULL) != 0;` |
|  278538 | 2674 | `						int bLossyFloat = (pEntry->iFlags & MEMOBJ_REAL) != 0` |
|  185668 | 2675 | `							&& pEntry->rVal != (ph7_real)(sxi64)pEntry->rVal;` |
|  185673 | 2676 | `						if( bNull ){` |
|       3 | 2677 | `							PH7_VmNullOffsetDeprecate(&(*pVm),pEntry);` |
|  185672 | 2678 | `						}else if( bLossyFloat ){` |
|       3 | 2679 | `							const char *zErr = "Cannot access offset of type float on array";` |
|       - | 2680 | `							SyBlob sErrMsg;` |
|       3 | 2681 | `							SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       3 | 2682 | `							SyBlobAppend(&sErrMsg,zErr,(sxu32)SyStrlen(zErr));` |
|       3 | 2683 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|       1 | 2684 | `						}` |
|   92804 | 2685 | `					}` |
|       - | 2686 | `				/* Standard insertion */` |
|  409258 | 2687 | `				PH7_HashmapInsert(pMap,` |
|  273056 | 2688 | `					(pEntry->iFlags & MEMOBJ_AUX_NOKEY) ? 0 /* Automatic index assign */ : pEntry,` |
|  136197 | 2689 | `					&pEntry[1]` |
|       - | 2690 | `				);` |
|       - | 2691 | `			}` |
|       - | 2692 | `			/* Next pair on the stack */` |
|  273967 | 2693 | `			pEntry += 2;` |
|       5 | 2694 | `		}` |
|       - | 2695 | `		/* Pop P1 elements */` |
|  203609 | 2696 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|  203609 | 2697 | `		if( rcSpread != SXRET_OK ){` |
|       - | 2698 | `			/* Discard the partially-built map and propagate the exception. */` |
|      49 | 2699 | `			PH7_HashmapRelease(pMap,TRUE);` |
|      49 | 2700 | `			if( rcSpread == PH7_ABORT ){` |
|     ! 0 | 2701 | `				VM_EXIT_ABORT;` |
|       - | 2702 | `			}` |
|       - | 2703 | `			{` |
|       - | 2704 | `				sxi32 iRp;` |
|      49 | 2705 | `				if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|       6 | 2706 | `					pc = iRp;` |
|       6 | 2707 | `					VM_EXIT_BREAK;` |
|       - | 2708 | `				}` |
|       - | 2709 | `			}` |
|      45 | 2710 | `			VM_EXIT_EXCEPTION;` |
|       - | 2711 | `		}` |
|  101719 | 2712 | `	}` |
|       - | 2713 | `	/* Push the hashmap */` |
|  312061 | 2714 | `	pTos++;` |
|  312061 | 2715 | `	pTos->nIdx = SXU32_HIGH;` |
|  312061 | 2716 | `	pTos->x.pOther = pMap;` |
|  312061 | 2717 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|  312061 | 2718 | `	VM_EXIT_BREAK;` |
|     ! 0 | 2719 | `	VM_EXIT_BREAK;` |
|  155984 | 2720 | `}` |
|       - | 2721 |  |
|       - | 2722 | `/*` |
|       - | 2723 | `` * Can `$o[$k]` be READ at all? php's read_dimension is either the class's own`` |
|       - | 2724 | ` * native handler -- which a class may carry WITHOUT implementing ArrayAccess,` |
|       - | 2725 | ` * php's DOMNodeList -- or the standard one, which needs the interface. Neither` |
|       - | 2726 | `` * is php's `Cannot use object of type C as array`.`` |
|       - | 2727 | ` */` |
|      30 | 2728 | `static int VmObjectDimReadable(ph7_vm *pVm,ph7_class_instance *pInst)` |
|       1 | 2729 | `{` |
|      31 | 2730 | `	if( pInst == 0 ){` |
|     ! 0 | 2731 | `		return 0;` |
|       - | 2732 | `	}` |
|      45 | 2733 | `	return PH7_ClassHasNativeDim(pInst->pClass)` |
|      30 | 2734 | `	    \|\| (pVm->pArrayAccessClass && PH7_VmInstanceOf(pInst->pClass,pVm->pArrayAccessClass));` |
|      16 | 2735 | `}` |
|       - | 2736 | `/*` |
|       - | 2737 | ` * One such READ, into pOut (which the caller inits and owns). The native` |
|       - | 2738 | ` * handler comes first for the same reason it does at the subscript opcode: php` |
|       - | 2739 | ` * implements the interface THROUGH the handler. A refusal is dropped here --` |
|       - | 2740 | ` * the only caller indexes 0..N-1 of its own target list, which no handler` |
|       - | 2741 | ` * refuses -- and pOut is simply left as it was.` |
|       - | 2742 | ` *` |
|       - | 2743 | ` * Answers the accessor's own status so a caller reading a RUN of positions can` |
|       - | 2744 | ` * stop where php stops: a userland offsetGet that THROWS abandons the rest of` |
|       - | 2745 | ` * the destructure, leaving every later target at its previous value.` |
|       - | 2746 | ` */` |
|      36 | 2747 | `static sxi32 VmObjectDimRead(ph7_vm *pVm,ph7_class_instance *pInst,ph7_value *pKey,ph7_value *pOut)` |
|       1 | 2748 | `{` |
|       - | 2749 | `	ph7_class_method *pGet;` |
|       - | 2750 | `	sxi32 rcCall;` |
|      37 | 2751 | `	if( PH7_ClassHasNativeDim(pInst->pClass) ){` |
|       - | 2752 | `		PH7_NativeDimCtx sDim;` |
|       5 | 2753 | `		sDim.iMode = PH7_NATIVE_DIM_READ;` |
|       5 | 2754 | `		sDim.pOffset = pKey;` |
|       5 | 2755 | `		sDim.pResult = pOut;` |
|       5 | 2756 | `		sDim.zThrowClass = 0;` |
|       5 | 2757 | `		sDim.zThrowMsg[0] = 0;` |
|       5 | 2758 | `		sDim.bStored = 0;` |
|       5 | 2759 | `		PH7_ClassNativeDim(pInst,&sDim);` |
|       5 | 2760 | `		return SXRET_OK;` |
|       - | 2761 | `	}` |
|      33 | 2762 | `	pGet = PH7_ClassExtractMethod(pInst->pClass,"offsetGet",sizeof("offsetGet")-1);` |
|      33 | 2763 | `	if( pGet == 0 ){` |
|     ! 0 | 2764 | `		return SXRET_OK;` |
|       - | 2765 | `	}` |
|       - | 2766 | `	{` |
|       - | 2767 | `		ph7_value *apArg[1];` |
|      33 | 2768 | `		apArg[0] = pKey;` |
|      33 | 2769 | `		rcCall = PH7_VmCallClassMethod(&(*pVm),pInst,pGet,pOut,1,apArg);` |
|       - | 2770 | `	}` |
|      33 | 2771 | `	return (rcCall == PH7_EXCEPTION \|\| pVm->nBoundaryRc != 0) ? PH7_EXCEPTION : SXRET_OK;` |
|      19 | 2772 | `}` |
|       - | 2773 | `/*` |
|       - | 2774 | ` * OP_LOAD_LIST: body moved verbatim from the OP_LOAD_LIST arm of` |
|       - | 2775 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 2776 | ` */` |
|  393529 | 2777 | `PH7_PRIVATE VmOpRc VmExecOpLoadList(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 2778 | `{` |
|  393534 | 2779 | `	ph7_value *pTos = pState->pTos;` |
|  393534 | 2780 | `	ph7_value *pStack = pState->pStack;` |
|  393534 | 2781 | `	VmInstr *aInstr = pState->aInstr;` |
|  393534 | 2782 | `	sxi32 pc = pState->pc;` |
|       - | 2783 | `	sxi32 rc;` |
|  196761 | 2784 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - | 2785 | `	ph7_value *pEntry;` |
|  393534 | 2786 | `	sxi32 rcEnforce = SXRET_OK;` |
|  393534 | 2787 | `	if( pInstr->iP1 <= 0 ){` |
|       - | 2788 | `		/* Empty list,break immediately */` |
|     ! 0 | 2789 | `		VM_EXIT_BREAK;` |
|       - | 2790 | `	}` |
|  393534 | 2791 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|       - | 2792 | `#ifdef UNTRUST` |
|       - | 2793 | `	if( &pEntry[-1] < pStack ){` |
|       - | 2794 | `		VM_EXIT_ABORT;` |
|       - | 2795 | `	}` |
|       - | 2796 | `#endif` |
|  393534 | 2797 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|  393470 | 2798 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|       - | 2799 | `		ph7_hashmap_node *pNode;` |
|       - | 2800 | `		ph7_value sKey,*pObj;` |
|       - | 2801 | `		/* Start Copying */` |
|  393470 | 2802 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
| 1570958 | 2803 | `		while( pEntry <= pTos ){` |
| 1177511 | 2804 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
| 1177481 | 2805 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
| 1177481 | 2806 | `				if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
| 1177481 | 2807 | `					int bTyped = PH7_VM_STORE_FILTERED(pVm,pEntry->nIdx);` |
| 1177481 | 2808 | `					if( rc != SXRET_OK ){` |
|       - | 2809 | `						/* Undefined array key */` |
|       - | 2810 | `						char zMsg[128];` |
|       8 | 2811 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|       8 | 2812 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|       3 | 2813 | `					}` |
| 1177481 | 2814 | `					if( !bTyped ){` |
| 1177445 | 2815 | `						if( rc == SXRET_OK ){` |
|       - | 2816 | `							/* Store node value */` |
| 1177443 | 2817 | `							PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|  588717 | 2818 | `						}else{` |
|       3 | 2819 | `							PH7_MemObjRelease(pObj);` |
|       - | 2820 | `						}` |
|  588718 | 2821 | `					}else{` |
|       - | 2822 | ``						/* Typed/readonly property target (`[$o->p] = [...]`): a`` |
|       - | 2823 | `						 * direct slot write would bypass the typed-slot table, so` |
|       - | 2824 | `						 * enforce on a temp first — a TypeError leaves the property` |
|       - | 2825 | `						 * untouched, and a missing key assigns null, which a` |
|       - | 2826 | `						 * non-nullable type rejects exactly like php (warning, then` |
|       - | 2827 | `						 * "Cannot assign null to property ... of type ..."). */` |
|       - | 2828 | `						ph7_value sVal;` |
|      38 | 2829 | `						PH7_MemObjInit(&(*pVm),&sVal);` |
|      38 | 2830 | `						if( rc == SXRET_OK ){` |
|      34 | 2831 | `							PH7_HashmapExtractNodeValue(pNode,&sVal,TRUE);` |
|      16 | 2832 | `						}` |
|      38 | 2833 | `						rcEnforce = VmEnforcePropertyTypeOnStore(&(*pVm),pEntry->nIdx,&sVal,0);` |
|      38 | 2834 | `						if( rcEnforce != SXRET_OK ){` |
|       - | 2835 | `							/* Thrown: stop assigning (php aborts the list at the` |
|       - | 2836 | `							 * first failing element), settle the stack, route. */` |
|      20 | 2837 | `							PH7_MemObjRelease(&sVal);` |
|      20 | 2838 | `							break;` |
|       - | 2839 | `						}` |
|      20 | 2840 | `						PH7_MemObjStore(&sVal,pObj);` |
|      20 | 2841 | `						PH7_MemObjRelease(&sVal);` |
|       - | 2842 | `					}` |
|  588722 | 2843 | `				}` |
|  588722 | 2844 | `			}` |
| 1177493 | 2845 | `			sKey.x.iVal++; /* Next numeric index */` |
| 1177493 | 2846 | `			pEntry++;` |
|       5 | 2847 | `		}` |
|  196807 | 2848 | `	}else if( (pEntry[-1].iFlags & MEMOBJ_OBJ) && pEntry[-1].x.pOther ){` |
|       - | 2849 | `		/* php destructures an OBJECT through its read_dimension handler, one` |
|       - | 2850 | ``		 * READ per POSITION -- `[$a, , $c] = $o` asks for 0 and 2 and never 1 --`` |
|       - | 2851 | `		 * so an ArrayObject, an SplFixedArray and (since the handler landed) a` |
|       - | 2852 | `		 * DOMNodeList all come apart the way an array does. PHL treated every` |
|       - | 2853 | `` 		 * object as a non-array source: it warned `Cannot use object as array` `` |
|       - | 2854 | ``		 * and assigned NULL to every target, so `[$first, $second] = $list` --`` |
|       - | 2855 | `		 * the shape every modern DOM and SPL example is written in -- silently` |
|       - | 2856 | `		 * produced two nulls. An object with NO dimension reader is php's` |
|       - | 2857 | `		 * catchable Error rather than that warning, and it is raised before any` |
|       - | 2858 | ``		 * target is touched. (The KEYED spelling `['k' => $a] = $o` never came`` |
|       - | 2859 | `		 * here: the compiler routes it through OP_LOAD_IDX, which has had the` |
|       - | 2860 | `		 * accessor dispatch all along.) */` |
|      31 | 2861 | `		ph7_class_instance *pInst = (ph7_class_instance *)pEntry[-1].x.pOther;` |
|       - | 2862 | `		ph7_value sKey;` |
|      31 | 2863 | `		if( !VmObjectDimReadable(&(*pVm),pInst) ){` |
|       - | 2864 | `			/* Routed mid-expression, like every other catchable Error raised from` |
|       - | 2865 | `			 * an opcode that is not a call boundary: the destructure is abandoned` |
|       - | 2866 | `			 * and an enclosing try in THIS frame lands on its own handler. Settle` |
|       - | 2867 | `			 * the targets AND the source first — the statement's OP_POP is skipped` |
|       - | 2868 | `			 * when a catch resumes at the landing pad. */` |
|       - | 2869 | `			char zMsg[256];` |
|      13 | 2870 | `			SyString *pName = &pInst->pClass->sName;` |
|      19 | 2871 | `			sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - | 2872 | `				"Cannot use object of type %.*s as array",` |
|      12 | 2873 | `				(int)pName->nByte,pName->zString);` |
|      13 | 2874 | `			rc = VmThrowFromVm(&(*pVm),"Error",zMsg,nMsg);` |
|      13 | 2875 | `			VmPopOperand(&pTos,pInstr->iP1 + 1);` |
|      13 | 2876 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      13 | 2877 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|     ! 0 | 2878 | `		}else{` |
|      19 | 2879 | `			PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      59 | 2880 | `			while( pEntry <= pTos ){` |
|      43 | 2881 | `				if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */ ){` |
|      37 | 2882 | `					sxu32 nSlot = pEntry->nIdx;` |
|      37 | 2883 | `					int bTyped = PH7_VM_STORE_FILTERED(pVm,nSlot);` |
|       - | 2884 | `					ph7_value sVal,*pObj;` |
|      37 | 2885 | `					PH7_MemObjInit(&(*pVm),&sVal);` |
|      37 | 2886 | `					if( VmObjectDimRead(&(*pVm),pInst,&sKey,&sVal) != SXRET_OK ){` |
|       - | 2887 | `						/* The accessor threw: php abandons the destructure there,` |
|       - | 2888 | `						 * so every later target keeps the value it had -- and this` |
|       - | 2889 | `						 * target does too, since php assigns nothing for the read` |
|       - | 2890 | `						 * that failed. */` |
|       3 | 2891 | `						PH7_MemObjRelease(&sVal);` |
|       3 | 2892 | `						rcEnforce = PH7_EXCEPTION;` |
|       3 | 2893 | `						break;` |
|       - | 2894 | `					}` |
|      35 | 2895 | `					if( bTyped ){` |
|       - | 2896 | `						/* Same rule as the array source's typed target: enforce on` |
|       - | 2897 | `						 * the temp so a TypeError leaves the property untouched. */` |
|     ! 0 | 2898 | `						rcEnforce = VmEnforcePropertyTypeOnStore(&(*pVm),nSlot,&sVal,0);` |
|     ! 0 | 2899 | `						if( rcEnforce != SXRET_OK ){` |
|     ! 0 | 2900 | `							PH7_MemObjRelease(&sVal);` |
|     ! 0 | 2901 | `							break;` |
|       - | 2902 | `						}` |
|     ! 0 | 2903 | `					}` |
|       - | 2904 | `					/* Re-fetch AFTER the read: a userland offsetGet can reserve` |
|       - | 2905 | `					 * slots, which used to relocate every pointer into the pool.` |
|       - | 2906 | `					 * Redundant now the table is segmented; left for the harvest` |
|       - | 2907 | `					 * sweep. */` |
|      35 | 2908 | `					pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nSlot);` |
|      35 | 2909 | `					if( pObj ){` |
|      35 | 2910 | `						PH7_MemObjStore(&sVal,pObj);` |
|      17 | 2911 | `					}` |
|      35 | 2912 | `					PH7_MemObjRelease(&sVal);` |
|      17 | 2913 | `				}` |
|      41 | 2914 | `				sKey.x.iVal++; /* Next numeric index */` |
|      41 | 2915 | `				pEntry++;` |
|       1 | 2916 | `			}` |
|      19 | 2917 | `			PH7_MemObjRelease(&sKey);` |
|       - | 2918 | `		}` |
|      10 | 2919 | `	}else{` |
|       - | 2920 | `		/* Source is not an array: php warns first (silencing ONLY null — a bool` |
|       - | 2921 | `		 * source warns too, php 8), then assigns null to every target. A typed` |
|       - | 2922 | `		 * property target receives that null THROUGH enforcement, so a` |
|       - | 2923 | `		 * non-nullable type throws "Cannot assign null to property ..." exactly` |
|       - | 2924 | `		 * like php instead of silently nulling the slot. PHL DIVERGENCE: bool` |
|       - | 2925 | `		 * FALSE stays silent (php warns) — it is the end-of-array sentinel of` |
|       - | 2926 | `` 		 * the documented each() extension's `while (list(..) = each($a))` `` |
|       - | 2927 | `		 * idiom, which would otherwise warn on every normal loop exit. */` |
|       - | 2928 | `		ph7_value *pObj;` |
|      57 | 2929 | `		int bFalseSrc = (pTos[-pInstr->iP1].iFlags & MEMOBJ_BOOL) != 0` |
|      34 | 2930 | `			&& pTos[-pInstr->iP1].x.iVal == 0;` |
|      33 | 2931 | `		sxi32 nWarn = (pTos[-pInstr->iP1].iFlags & MEMOBJ_NULL) == 0 && !bFalseSrc` |
|      41 | 2932 | `			? pInstr->iP2 : 0;` |
|       - | 2933 | `		/* php asks the non-array source for each POSITION it means to fill, so the` |
|       - | 2934 | `		 * warning is per ENTRY and not one for the whole list, which is what this` |
|       - | 2935 | `		 * used to raise. An EMPTY slot fills nothing and is not counted -- P2 carries` |
|       - | 2936 | `		 * the count the compiler made. */` |
|      71 | 2937 | `		while( nWarn > 0 ){` |
|      37 | 2938 | `			VmWarnCannotUseAsArray(&(*pVm),pTos[-pInstr->iP1].iFlags);` |
|      37 | 2939 | `			nWarn--;` |
|       3 | 2940 | `		}` |
|      85 | 2941 | `		while( pEntry <= pTos ){` |
|      57 | 2942 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|      53 | 2943 | `				if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|      53 | 2944 | `					int bTyped = PH7_VM_STORE_FILTERED(pVm,pEntry->nIdx);` |
|      53 | 2945 | `					if( !bTyped ){` |
|      45 | 2946 | `						PH7_MemObjRelease(pObj);` |
|      24 | 2947 | `					}else{` |
|       - | 2948 | `						ph7_value sVal;` |
|       9 | 2949 | `						PH7_MemObjInit(&(*pVm),&sVal);` |
|       9 | 2950 | `						rcEnforce = VmEnforcePropertyTypeOnStore(&(*pVm),pEntry->nIdx,&sVal,0);` |
|       9 | 2951 | `						if( rcEnforce != SXRET_OK ){` |
|       7 | 2952 | `							PH7_MemObjRelease(&sVal);` |
|       7 | 2953 | `							break;` |
|       - | 2954 | `						}` |
|       3 | 2955 | `						PH7_MemObjStore(&sVal,pObj);` |
|       3 | 2956 | `						PH7_MemObjRelease(&sVal);` |
|       - | 2957 | `					}` |
|      22 | 2958 | `				}` |
|      22 | 2959 | `			}` |
|      51 | 2960 | `			pEntry++;` |
|       3 | 2961 | `		}` |
|       - | 2962 | `	}` |
|  393522 | 2963 | `	if( rcEnforce != SXRET_OK ){` |
|       - | 2964 | `		/* Settle this op's own operands: the P1 entries AND the source value —` |
|       - | 2965 | `		 * its statement-level OP_POP is skipped when a catch resumes at the` |
|       - | 2966 | `		 * landing pad, so leaving it would leak one operand slot per caught` |
|       - | 2967 | `		 * throw. A NESTED destructure can still have the outer list's operands` |
|       - | 2968 | `		 * abandoned above the try's base, so on an in-place catch drain to the` |
|       - | 2969 | `		 * catching try's recorded depth (like the fetch-point router and the` |
|       - | 2970 | `		 * generator inject path), not just our own pops. */` |
|      28 | 2971 | `		VmPopOperand(&pTos,pInstr->iP1 + 1);` |
|      28 | 2972 | `		if( rcEnforce == PH7_ABORT ){` |
|     ! 0 | 2973 | `			VM_EXIT_ABORT;` |
|       - | 2974 | `		}` |
|       - | 2975 | `		{` |
|       - | 2976 | `			sxi32 _iRpL;` |
|      40 | 2977 | `			PH7_INLINE_RESUME_BREAK()` |
|      28 | 2978 | `			if( VmRecordedResume(pVm,&_iRpL,pState->pEntryFrame,aInstr) ){` |
|      26 | 2979 | `				PH7_RESUME_DRAIN()` |
|      26 | 2980 | `				pc = _iRpL;` |
|      26 | 2981 | `				VM_EXIT_BREAK;` |
|       - | 2982 | `			}` |
|       - | 2983 | `		}` |
|       3 | 2984 | `		VM_EXIT_EXCEPTION;` |
|       - | 2985 | `	}` |
|  393496 | 2986 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|  393496 | 2987 | `	VM_EXIT_BREAK;` |
|     ! 0 | 2988 | `	VM_EXIT_BREAK;` |
|  196766 | 2989 | `}` |
|       - | 2990 |  |
|       - | 2991 | `/*` |
|       - | 2992 | ` * OP_UNSET_VAR: body moved verbatim from the OP_UNSET_VAR arm of` |
|       - | 2993 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 2994 | ` */` |
|    9007 | 2995 | `PH7_PRIVATE VmOpRc VmExecOpUnsetVar(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 2996 | `{` |
|    9012 | 2997 | `	ph7_value *pTos = pState->pTos;` |
|    9012 | 2998 | `	ph7_value *pStack = pState->pStack;` |
|    9012 | 2999 | `	VmInstr *aInstr = pState->aInstr;` |
|    9012 | 3000 | `	sxi32 pc = pState->pc;` |
|       - | 3001 | `	sxi32 rc;` |
|    4497 | 3002 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - | 3003 | `	/* unset($name): p3 is the variable name. Drops the NAME only — see VmUnsetVarByName */` |
|    9012 | 3004 | `	SyString *pName = (SyString *)pInstr->p3;` |
|    9012 | 3005 | `	if( pName && pVm->pFrame ){` |
|       - | 3006 | `		/* Inside a try{} the VM pushes an EXCEPTION frame; variables live in the body` |
|       - | 3007 | `		 * frame below it, so skip past it exactly as every other variable path does.` |
|       - | 3008 | `		 * Without this, unset($x) inside a try silently found nothing and did nothing. */` |
|    9012 | 3009 | `		VmFrame *pVarFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|    9012 | 3010 | `		sxi32 rcU = VmUnsetVarByName(&(*pVm),pVarFrame,pName->zString,pName->nByte);` |
|    9012 | 3011 | `		if( rcU == PH7_ABORT ){` |
|       3 | 3012 | `			VM_EXIT_ABORT;` |
|       - | 3013 | `		}` |
|       - | 3014 | `		/* Releasing the last holder can run a __destruct(), and that destructor may` |
|       - | 3015 | `		 * throw. Such a throw is PARKED in nBoundaryRc by the boundary rail; consume it` |
|       - | 3016 | `		 * here and route it, or the catch runs and execution resumes inside the try` |
|       - | 3017 | `		 * ("resumed-dtor" instead of php's "caught-dtor"). */` |
|    9010 | 3018 | `		if( pVm->nBoundaryRc != 0 ){` |
|       6 | 3019 | `			rc = pVm->nBoundaryRc;` |
|       6 | 3020 | `			pVm->nBoundaryRc = 0;` |
|       6 | 3021 | `			if( rc == PH7_ABORT ){` |
|     ! 0 | 3022 | `				VM_EXIT_ABORT;` |
|       - | 3023 | `			}` |
|       6 | 3024 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 3025 | `		}` |
|    4494 | 3026 | `	}` |
|    9006 | 3027 | `	VM_EXIT_BREAK;` |
|     ! 0 | 3028 | `	VM_EXIT_BREAK;` |
|    4502 | 3029 | `}` |
|       - | 3030 |  |
